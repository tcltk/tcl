/* 
 * Tclwinserial.c --
 *
 *	This file implements the Windows-specific serial port functions,
 *	and the "serial" channel driver.
 *
 * Copyright (c) 1999 by Scriptics Corp.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclWinSerial.c,v 1.1.2.4 1999/03/25 00:34:17 redman Exp $
 */

#include "tclWinInt.h"

#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>

/*
 * The following variable is used to tell whether this module has been
 * initialized.
 */

static int initialized = 0;
TCL_DECLARE_MUTEX(procMutex)

/*
 * Bit masks used in the flags field of the SerialInfo structure below.
 */

#define SERIAL_PENDING	(1<<0)	/* Message is pending in the queue. */
#define SERIAL_ASYNC	(1<<1)	/* Channel is non-blocking. */

/*
 * Bit masks used in the sharedFlags field of the SerialInfo structure below.
 */

#define SERIAL_EOF	 (1<<2)  /* Serial has reached EOF. */
#define SERIAL_EXTRABYTE (1<<3)  /* Extra byte consumed while waiting for data */
/*
 * This structure describes per-instance data for a serial based channel.
 */

typedef struct SerialInfo {
    HANDLE handle;
    struct SerialInfo *nextPtr;	/* Pointer to next registered serial. */
    Tcl_Channel channel;	/* Pointer to channel structure. */
    int validMask;		/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, or TCL_EXCEPTION: indicates
				 * which operations are valid on the file. */
    int watchMask;		/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, or TCL_EXCEPTION: indicates
				 * which events should be reported. */
    int flags;			/* State flags, see above for a list. */
    Tcl_ThreadId threadId;	/* Thread to which events should be reported.
				 * This value is used by the reader/writer
				 * threads. */
    HANDLE writeThread;		/* Handle to writer thread. */
    HANDLE readThread;		/* Handle to reader thread. */
    HANDLE writable;		/* Manual-reset event to signal when the
				 * writer thread has finished waiting for
				 * the current buffer to be written. */
    HANDLE readable;		/* Manual-reset event to signal when the
				 * reader thread has finished waiting for
				 * input. */
    HANDLE startWriter;		/* Auto-reset event used by the main thread to
				 * signal when the writer thread should attempt
				 * to write to the serial. */
    HANDLE startReader;		/* Auto-reset event used by the main thread to
				 * signal when the reader thread should attempt
				 * to read from the serial. */
    DWORD writeError;		/* An error caused by the last background
				 * write.  Set to 0 if no error has been
				 * detected.  This word is shared with the
				 * writer thread so access must be
				 * synchronized with the writable object.
				 */
    char *writeBuf;		/* Current background output buffer.
				 * Access is synchronized with the writable
				 * object. */
    int writeBufLen;		/* Size of write buffer.  Access is
				 * synchronized with the writable
				 * object. */
    int toWrite;		/* Current amount to be written.  Access is
				 * synchronized with the writable object. */
    int readFlags;		/* Flags that are shared with the reader
				 * thread.  Access is synchronized with the
				 * readable object.  */
    int writeFlags;		/* Flags that are shared with the writer
				 * thread.  Access is synchronized with the
				 * readable object.  */
    int readyMask;              /* Events that need to be checked on. */
    char extraByte;
    
} SerialInfo;

typedef struct ThreadSpecificData {
    /*
     * The following pointer refers to the head of the list of serials
     * that are being watched for file events.
     */
    
    SerialInfo *firstSerialPtr;
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

/*
 * The following structure is what is added to the Tcl event queue when
 * serial events are generated.
 */

typedef struct SerialEvent {
    Tcl_Event header;		/* Information that is standard for
				 * all events. */
    SerialInfo *infoPtr;	/* Pointer to serial info structure.  Note
				 * that we still have to verify that the
				 * serial exists before dereferencing this
				 * pointer. */
} SerialEvent;

/*
 * Declarations for functions used only in this file.
 */

static int		SerialBlockProc(ClientData instanceData, int mode);
static void		SerialCheckProc(ClientData clientData, int flags);
static int		SerialCloseProc(ClientData instanceData,
			    Tcl_Interp *interp);
static int		SerialEventProc(Tcl_Event *evPtr, int flags);
static void		SerialExitHandler(ClientData clientData);
static int		SerialGetHandleProc(ClientData instanceData,
			    int direction, ClientData *handlePtr);
static ThreadSpecificData *SerialInit(void);
static int		SerialInputProc(ClientData instanceData, char *buf,
			    int toRead, int *errorCode);
static int		SerialOutputProc(ClientData instanceData, char *buf,
			    int toWrite, int *errorCode);
static DWORD WINAPI	SerialReaderThread(LPVOID arg);
static void		SerialSetupProc(ClientData clientData, int flags);
static void		SerialWatchProc(ClientData instanceData, int mask);
static DWORD WINAPI	SerialWriterThread(LPVOID arg);
static void		ProcExitHandler(ClientData clientData);
static int		WaitForRead(SerialInfo *infoPtr, int blocking);
static int	     SerialGetOptionProc _ANSI_ARGS_((ClientData instanceData, 
			    Tcl_Interp *interp, char *optionName,
			    Tcl_DString *dsPtr));
static int	     SerialSetOptionProc _ANSI_ARGS_((ClientData instanceData,
			    Tcl_Interp *interp, char *optionName, 
			    char *value));

/*
 * This structure describes the channel type structure for command serial
 * based IO.
 */

static Tcl_ChannelType serialChannelType = {
    "serial",			/* Type name. */
    SerialBlockProc,	        /* Set blocking or non-blocking mode.*/
    SerialCloseProc,		/* Close proc. */
    SerialInputProc,		/* Input proc. */
    SerialOutputProc,		/* Output proc. */
    NULL,			/* Seek proc. */
    SerialSetOptionProc,	/* Set option proc. */
    SerialGetOptionProc,	/* Get option proc. */
    SerialWatchProc,		/* Set up notifier to watch the channel. */
    SerialGetHandleProc,	/* Get an OS handle from channel. */
};

/*
 *----------------------------------------------------------------------
 *
 * SerialInit --
 *
 *	This function initializes the static variables for this file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new event source.
 *
 *----------------------------------------------------------------------
 */

static ThreadSpecificData *
SerialInit()
{
    ThreadSpecificData *tsdPtr;

    /*
     * Check the initialized flag first, then check it again in the mutex.
     * This is a speed enhancement.
     */
    
    if (!initialized) {
	Tcl_MutexLock(&procMutex);
	if (!initialized) {
	    initialized = 1;
	    Tcl_CreateExitHandler(ProcExitHandler, NULL);
	}
	Tcl_MutexUnlock(&procMutex);
    }

    tsdPtr = (ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);
    if (tsdPtr == NULL) {
	tsdPtr = TCL_TSD_INIT(&dataKey);
	tsdPtr->firstSerialPtr = NULL;
	Tcl_CreateEventSource(SerialSetupProc, SerialCheckProc, NULL);
	Tcl_CreateThreadExitHandler(SerialExitHandler, NULL);
    }
    return tsdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * SerialExitHandler --
 *
 *	This function is called to cleanup the serial module before
 *	Tcl is unloaded.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes the serial event source.
 *
 *----------------------------------------------------------------------
 */

static void
SerialExitHandler(
    ClientData clientData)	/* Old window proc */
{
    Tcl_DeleteEventSource(SerialSetupProc, SerialCheckProc, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * ProcExitHandler --
 *
 *	This function is called to cleanup the process list before
 *	Tcl is unloaded.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resets the process list.
 *
 *----------------------------------------------------------------------
 */

static void
ProcExitHandler(
    ClientData clientData)	/* Old window proc */
{
    Tcl_MutexLock(&procMutex);
    initialized = 0;
    Tcl_MutexUnlock(&procMutex);
}

/*
 *----------------------------------------------------------------------
 *
 * SerialSetupProc --
 *
 *	This procedure is invoked before Tcl_DoOneEvent blocks waiting
 *	for an event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adjusts the block time if needed.
 *
 *----------------------------------------------------------------------
 */

void
SerialSetupProc(
    ClientData data,		/* Not used. */
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    SerialInfo *infoPtr;
    Tcl_Time blockTime = { 0, 0 };
    int block = 1;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }
    
    /*
     * Look to see if any events are already pending.  If they are, poll.
     */

    for (infoPtr = tsdPtr->firstSerialPtr; infoPtr != NULL; 
	    infoPtr = infoPtr->nextPtr) {
	if (infoPtr->watchMask & TCL_WRITABLE) {
	    if (WaitForSingleObject(infoPtr->writable, 0) != WAIT_TIMEOUT) {
		block = 0;
	    }
	}
	if (infoPtr->watchMask & TCL_READABLE) {
	    if (WaitForRead(infoPtr, 0) >= 0) {
		block = 0;
	    }
	}
    }
    if (!block) {
	Tcl_SetMaxBlockTime(&blockTime);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SerialCheckProc --
 *
 *	This procedure is called by Tcl_DoOneEvent to check the serial
 *	event source for events. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May queue an event.
 *
 *----------------------------------------------------------------------
 */

static void
SerialCheckProc(
    ClientData data,		/* Not used. */
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    SerialInfo *infoPtr;
    SerialEvent *evPtr;
    int needEvent;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }
    
    /*
     * Queue events for any ready serials that don't already have events
     * queued.
     */

    for (infoPtr = tsdPtr->firstSerialPtr; infoPtr != NULL; 
	    infoPtr = infoPtr->nextPtr) {
	if (infoPtr->flags & SERIAL_PENDING) {
	    continue;
	}
	
	/*
	 * Queue an event if the serial is signaled for reading or writing.
	 */

	needEvent = 0;
	if (infoPtr->watchMask & TCL_WRITABLE) {
	    if (WaitForSingleObject(infoPtr->writable, 0) != WAIT_TIMEOUT) {
		needEvent = 1;
	    }
	}
	
	if (infoPtr->watchMask & TCL_READABLE) {
	    if (WaitForRead(infoPtr, 0) >= 0) {
		needEvent = 1;
	    }
	}

	if (needEvent) {
	    infoPtr->flags |= SERIAL_PENDING;
	    evPtr = (SerialEvent *) ckalloc(sizeof(SerialEvent));
	    evPtr->header.proc = SerialEventProc;
	    evPtr->infoPtr = infoPtr;
	    Tcl_QueueEvent((Tcl_Event *) evPtr, TCL_QUEUE_TAIL);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SerialBlockProc --
 *
 *	Set blocking or non-blocking mode on channel.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or non-blocking mode.
 *
 *----------------------------------------------------------------------
 */

static int
SerialBlockProc(
    ClientData instanceData,	/* Instance data for channel. */
    int mode)			/* TCL_MODE_BLOCKING or
                                 * TCL_MODE_NONBLOCKING. */
{
    SerialInfo *infoPtr = (SerialInfo *) instanceData;
    
    /*
     * Serial IO on Windows can not be switched between blocking & nonblocking,
     * hence we have to emulate the behavior. This is done in the input
     * function by checking against a bit in the state. We set or unset the
     * bit here to cause the input function to emulate the correct behavior.
     */

    if (mode == TCL_MODE_NONBLOCKING) {
	infoPtr->flags |= SERIAL_ASYNC;
    } else {
	infoPtr->flags &= ~(SERIAL_ASYNC);
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * SerialCloseProc --
 *
 *	Closes a serial based IO channel.
 *
 * Results:
 *	0 on success, errno otherwise.
 *
 * Side effects:
 *	Closes the physical channel.
 *
 *----------------------------------------------------------------------
 */

static int
SerialCloseProc(
    ClientData instanceData,	/* Pointer to SerialInfo structure. */
    Tcl_Interp *interp)		/* For error reporting. */
{
    SerialInfo *serialPtr = (SerialInfo *) instanceData;
    int errorCode, result = 0;
    SerialInfo *infoPtr, **nextPtrPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    errorCode = 0;
    if (serialPtr->readThread) {
	TerminateThread(serialPtr->readThread, 0);
	/*
	 * Wait for the thread to terminate.  This ensures that we are
	 * completely cleaned up before we leave this function. 
	 */

	WaitForSingleObject(serialPtr->readThread, INFINITE);
	CloseHandle(serialPtr->readThread);
	CloseHandle(serialPtr->readable);
	CloseHandle(serialPtr->startReader);
	serialPtr->readThread = NULL;
    }
    serialPtr->validMask &= ~TCL_READABLE;

    if (serialPtr->writeThread) {
	WaitForSingleObject(serialPtr->writable, INFINITE);
	TerminateThread(serialPtr->writeThread, 0);

	/*
	 * Wait for the thread to terminate.  This ensures that we are
	 * completely cleaned up before we leave this function. 
	 */

	WaitForSingleObject(serialPtr->writeThread, INFINITE);
	CloseHandle(serialPtr->writeThread);
	CloseHandle(serialPtr->writable);
	CloseHandle(serialPtr->startWriter);
	serialPtr->writeThread = NULL;
    }
    serialPtr->validMask &= ~TCL_WRITABLE;

    if (CloseHandle(serialPtr->handle) == FALSE) {
	TclWinConvertError(GetLastError());
	errorCode = errno;
    }

    serialPtr->watchMask &= serialPtr->validMask;

    /*
     * Remove the file from the list of watched files.
     */

    for (nextPtrPtr = &(tsdPtr->firstSerialPtr), infoPtr = *nextPtrPtr;
	    infoPtr != NULL;
	    nextPtrPtr = &infoPtr->nextPtr, infoPtr = *nextPtrPtr) {
	if (infoPtr == (SerialInfo *)serialPtr) {
	    *nextPtrPtr = infoPtr->nextPtr;
	    break;
	}
    }

    /*
     * Wrap the error file into a channel and give it to the cleanup
     * routine. 
     */

    if (serialPtr->writeBuf != NULL) {
	ckfree(serialPtr->writeBuf);
	serialPtr->writeBuf = NULL;
    }

    ckfree((char*) serialPtr);

    if (errorCode == 0) {
        return result;
    }
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * SerialInputProc --
 *
 *	Reads input from the IO channel into the buffer given. Returns
 *	count of how many bytes were actually read, and an error indication.
 *
 * Results:
 *	A count of how many bytes were read is returned and an error
 *	indication is returned in an output argument.
 *
 * Side effects:
 *	Reads input from the actual channel.
 *
 *----------------------------------------------------------------------
 */

static int
SerialInputProc(
    ClientData instanceData,		/* Serial state. */
    char *buf,				/* Where to store data read. */
    int bufSize,			/* How much space is available
                                         * in the buffer? */
    int *errorCode)			/* Where to store error code. */
{
    SerialInfo *infoPtr = (SerialInfo *) instanceData;
    DWORD bytesRead = 0;
    int result;
    DWORD err;

    *errorCode = 0;

    /*
     * Synchronize with the reader thread.
     */
    
    result = WaitForRead(infoPtr, (infoPtr->flags & SERIAL_ASYNC) ? 0 : 1);
    
    /*
     * If an error occurred, return immediately.
     */
    
    if (result == -1) {
	*errorCode = errno;
	return -1;
    }

    if (infoPtr->readFlags & SERIAL_EXTRABYTE) {

	/*
	 * If a byte was consumed waiting, then put it in the buffer.
	 */

	*buf = infoPtr->extraByte;
	infoPtr->readFlags &= ~SERIAL_EXTRABYTE;
	buf++;
	bufSize--;
	bytesRead = 1;

	if (result == 0) {
	    return bytesRead;
	}
    }

    if (ReadFile(infoPtr->handle, (LPVOID) buf, (DWORD) bufSize, &bytesRead,
		 NULL) == FALSE) {
	err = GetLastError();
	if (err != ERROR_IO_PENDING) {
	    goto error;
	}
    }
	
    return bytesRead;
	
    error:
    TclWinConvertError(GetLastError());
    *errorCode = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * SerialOutputProc --
 *
 *	Writes the given output on the IO channel. Returns count of how
 *	many characters were actually written, and an error indication.
 *
 * Results:
 *	A count of how many characters were written is returned and an
 *	error indication is returned in an output argument.
 *
 * Side effects:
 *	Writes output on the actual channel.
 *
 *----------------------------------------------------------------------
 */

static int
SerialOutputProc(
    ClientData instanceData,		/* Serial state. */
    char *buf,				/* The data buffer. */
    int toWrite,			/* How many bytes to write? */
    int *errorCode)			/* Where to store error code. */
{
    SerialInfo *infoPtr = (SerialInfo *) instanceData;
    DWORD bytesWritten, timeout, err;

    *errorCode = 0;
    timeout = (infoPtr->flags & SERIAL_ASYNC) ? 0 : INFINITE;
    if (WaitForSingleObject(infoPtr->writable, timeout) == WAIT_TIMEOUT) {
	/*
	 * The writer thread is blocked waiting for a write to complete
	 * and the channel is in non-blocking mode.
	 */

	errno = EAGAIN;
	goto error;
    }
    
    /*
     * Check for a background error on the last write.
     */

    if (infoPtr->writeError) {
	TclWinConvertError(infoPtr->writeError);
	infoPtr->writeError = 0;
	goto error;
    }

    if (infoPtr->flags & SERIAL_ASYNC) {
	/*
	 * The serial is non-blocking, so copy the data into the output
	 * buffer and restart the writer thread.
	 */

	if (toWrite > infoPtr->writeBufLen) {
	    /*
	     * Reallocate the buffer to be large enough to hold the data.
	     */

	    if (infoPtr->writeBuf) {
		ckfree(infoPtr->writeBuf);
	    }
	    infoPtr->writeBufLen = toWrite;
	    infoPtr->writeBuf = ckalloc(toWrite);
	}
	memcpy(infoPtr->writeBuf, buf, toWrite);
	infoPtr->toWrite = toWrite;
	ResetEvent(infoPtr->writable);
	SetEvent(infoPtr->startWriter);
	bytesWritten = toWrite;
    } else {
	/*
	 * In the blocking case, just try to write the buffer directly.
	 * This avoids an unnecessary copy.
	 */
	if (WriteFile(infoPtr->handle, (LPVOID) buf, (DWORD) toWrite,
		&bytesWritten, NULL) == FALSE) {
	    err = GetLastError();
	    if (err != ERROR_IO_PENDING) {
		TclWinConvertError(GetLastError());
		goto error;
	    }
	}
    }
    return bytesWritten;

    error:
    *errorCode = errno;
    return -1;

}

/*
 *----------------------------------------------------------------------
 *
 * SerialEventProc --
 *
 *	This function is invoked by Tcl_ServiceEvent when a file event
 *	reaches the front of the event queue.  This procedure invokes
 *	Tcl_NotifyChannel on the serial.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed
 *	from the queue.  Returns 0 if the event was not handled, meaning
 *	it should stay on the queue.  The only time the event isn't
 *	handled is if the TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the notifier callback does.
 *
 *----------------------------------------------------------------------
 */

static int
SerialEventProc(
    Tcl_Event *evPtr,		/* Event to service. */
    int flags)			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    SerialEvent *serialEvPtr = (SerialEvent *)evPtr;
    SerialInfo *infoPtr;
    int mask;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }

    /*
     * Search through the list of watched serials for the one whose handle
     * matches the event.  We do this rather than simply dereferencing
     * the handle in the event so that serials can be deleted while the
     * event is in the queue.
     */

    for (infoPtr = tsdPtr->firstSerialPtr; infoPtr != NULL;
	    infoPtr = infoPtr->nextPtr) {
	if (serialEvPtr->infoPtr == infoPtr) {
	    infoPtr->flags &= ~(SERIAL_PENDING);
	    break;
	}
    }
    /*
     * Remove stale events.
     */

    if (!infoPtr) {
	return 1;
    }

    /*
     * Check to see if the serial is readable.  Note
     * that we can't tell if a serial is writable, so we always report it
     * as being writable unless we have detected EOF.
     */

    mask = 0;
    if (infoPtr->watchMask & TCL_WRITABLE) {
	if (WaitForSingleObject(infoPtr->writable, 0) != WAIT_TIMEOUT) {
	  mask = TCL_WRITABLE;
	}
    }

    if (infoPtr->watchMask & TCL_READABLE) {
	if (WaitForRead(infoPtr, 0) >= 0) {
	    if (infoPtr->readFlags & SERIAL_EOF) {
		mask = TCL_READABLE;
	    } else {
		mask |= TCL_READABLE;
	    }
	} 
    }

    /*
     * Inform the channel of the events.
     */

    Tcl_NotifyChannel(infoPtr->channel, infoPtr->watchMask & mask);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * SerialWatchProc --
 *
 *	Called by the notifier to set up to watch for events on this
 *	channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
SerialWatchProc(
    ClientData instanceData,		/* Serial state. */
    int mask)				/* What events to watch for, OR-ed
                                         * combination of TCL_READABLE,
                                         * TCL_WRITABLE and TCL_EXCEPTION. */
{
    SerialInfo **nextPtrPtr, *ptr;
    SerialInfo *infoPtr = (SerialInfo *) instanceData;
    int oldMask = infoPtr->watchMask;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    /*
     * Since the file is always ready for events, we set the block time
     * to zero so we will poll.
     */

    infoPtr->watchMask = mask & infoPtr->validMask;
    if (infoPtr->watchMask) {
	Tcl_Time blockTime = { 0, 0 };
	if (!oldMask) {
	    infoPtr->nextPtr = tsdPtr->firstSerialPtr;
	    tsdPtr->firstSerialPtr = infoPtr;
	}
	Tcl_SetMaxBlockTime(&blockTime);
    } else {
	if (oldMask) {
	    /*
	     * Remove the serial port from the list of watched serial ports.
	     */

	    for (nextPtrPtr = &(tsdPtr->firstSerialPtr), ptr = *nextPtrPtr;
		 ptr != NULL;
		 nextPtrPtr = &ptr->nextPtr, ptr = *nextPtrPtr) {
		if (infoPtr == ptr) {
		    *nextPtrPtr = ptr->nextPtr;
		    break;
		}
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SerialGetHandleProc --
 *
 *	Called from Tcl_GetChannelHandle to retrieve OS handles from
 *	inside a command serial port based channel.
 *
 * Results:
 *	Returns TCL_OK with the fd in handlePtr, or TCL_ERROR if
 *	there is no handle for the specified direction. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
SerialGetHandleProc(
    ClientData instanceData,	/* The serial state. */
    int direction,		/* TCL_READABLE or TCL_WRITABLE */
    ClientData *handlePtr)	/* Where to store the handle.  */
{
    SerialInfo *infoPtr = (SerialInfo *) instanceData;

    *handlePtr = (ClientData) infoPtr->handle;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WaitForRead --
 *
 *	Wait until some data is available, the serial is at
 *	EOF or the reader thread is blocked waiting for data (if the
 *	channel is in non-blocking mode).
 *
 * Results:
 *	Returns 1 if serial is readable.  Returns 0 if there is no data
 *	on the serial, but there is buffered data.  Returns -1 if an
 *	error occurred.  If an error occurred, the threads may not
 *	be synchronized.
 *
 * Side effects:
 *	Updates the shared state flags and may consume 1 byte of data
 *	from the serial.  If no error occurred, the reader thread is
 *	blocked waiting for a signal from the main thread.
 *
 *----------------------------------------------------------------------
 */

static int
WaitForRead(
    SerialInfo *infoPtr,	/* Serial state. */
    int blocking)		/* Indicates whether call should be
				 * blocking or not. */
{
    DWORD timeout, errors;
    HANDLE *handle = infoPtr->handle;
    COMSTAT stat;
    
    while (1) {
	/*
	 * Synchronize with the reader thread.
	 */
	
	timeout = blocking ? INFINITE : 0;
	if (WaitForSingleObject(infoPtr->readable, timeout) == WAIT_TIMEOUT) {
	    /*
	     * The reader thread is blocked waiting for data and the channel
	     * is in non-blocking mode.
	     */
	    
	    errno = EAGAIN;
	    return -1;
	}
	
	/*
	 * At this point, the two threads are synchronized, so it is safe
	 * to access shared state.  This code is not called in the ReaderThread
	 * in blocking mode, so it needs to be checked here.
	 */

	/*
	 * If the serial has hit EOF, it is always readable.
	 */
    
	if (infoPtr->readFlags & SERIAL_EOF) {
	    return 1;
	}
	
	if (ClearCommError(infoPtr->handle, &errors, &stat)) {
	    /*
	     * If there are errors, then signal an I/O error.
	     */

	    if (errors != 0) {
		errno = EIO;
		return -1;
	    }
	}
	
	/*
	 * If data is in the queue return 1
	 */
	
	if (stat.cbInQue != 0) {
	    return 1;
	}

	/*
	 * if there is an extra byte that was consumed while
	 * waiting, but no data in the queue, return 0
	 */
	
	if (infoPtr->readFlags & SERIAL_EXTRABYTE) {
	    return 0;
	}
	    
	ResetEvent(infoPtr->readable);
	SetEvent(infoPtr->startReader);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SerialReaderThread --
 *
 *	This function runs in a separate thread and waits for input
 *	to become available on a serial.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Signals the main thread when input become available.  May
 *	cause the main thread to wake up by posting a message.  May
 *	consume one byte from the serial for each wait operation.
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
SerialReaderThread(LPVOID arg)
{
    SerialInfo *infoPtr = (SerialInfo *)arg;
    HANDLE *handle = infoPtr->handle;
    DWORD mask = EV_RXCHAR;
    DWORD count;
    
    for (;;) {
	/*
	 * Wait for the main thread to signal before attempting to wait.
	 */

	WaitForSingleObject(infoPtr->startReader, INFINITE);

	/*
	 * Try waiting for a Comm event.
	 */
	
	WaitCommEvent(handle, NULL, NULL);
	

	/*
	 * try to read one byte
	 */
	
	if (ReadFile(handle, &(infoPtr->extraByte), 1, &count, NULL)
		!= FALSE) {

	    /*
	     * one byte was consumed while waiting to read, keep it
	     */

	    if (count != 0) {
		infoPtr->readFlags |= SERIAL_EXTRABYTE;
	    }

	} else {
            /*
	     * There is an error, signal an EOF.
	     */
	    
	    infoPtr->readFlags |= SERIAL_EOF;
	}

	/*
	 * Signal the main thread by signalling the readable event and
	 * then waking up the notifier thread.
	 */

	SetEvent(infoPtr->readable);
	Tcl_ThreadAlert(infoPtr->threadId);
    }
    return 0;			/* NOT REACHED */
}

/*
 *----------------------------------------------------------------------
 *
 * SerialWriterThread --
 *
 *	This function runs in a separate thread and writes data
 *	onto a serial.
 *
 * Results:
 *	Always returns 0.
 *
 * Side effects:
 *	Signals the main thread when an output operation is completed.
 *	May cause the main thread to wake up by posting a message.  
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
SerialWriterThread(LPVOID arg)
{

    SerialInfo *infoPtr = (SerialInfo *)arg;
    HANDLE *handle = infoPtr->handle;
    DWORD count, toWrite, err;
    char *buf;

    for (;;) {
	/*
	 * Wait for the main thread to signal before attempting to write.
	 */

	WaitForSingleObject(infoPtr->startWriter, INFINITE);

	buf = infoPtr->writeBuf;
	toWrite = infoPtr->toWrite;

	/*
	 * Loop until all of the bytes are written or an error occurs.
	 */

	while (toWrite > 0) {
	    if (WriteFile(handle, (LPVOID) buf, (DWORD) toWrite,
			  &count, NULL) == FALSE) {
		err = GetLastError();
		if (err != ERROR_IO_PENDING) {
		    TclWinConvertError(GetLastError());
		    infoPtr->writeError = err;
		    break;
		}
	    } else {
		toWrite -= count;
		buf += count;
	    }
	}
	
	/*
	 * Signal the main thread by signalling the writable event and
	 * then waking up the notifier thread.
	 */

	SetEvent(infoPtr->writable);
	Tcl_ThreadAlert(infoPtr->threadId);
    }
    return 0;			/* NOT REACHED */
}



/*
 *----------------------------------------------------------------------
 *
 * TclWinOpenConsoleChannel --
 *
 *	Constructs a Serial port channel for the specified standard OS handle.
 *      This is a helper function to break up the construction of 
 *      channels into File, Console, or Serial.
 *
 * Results:
 *	Returns the new channel, or NULL.
 *
 * Side effects:
 *	May open the channel
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclWinOpenSerialChannel(handle, channelName, permissions)
    HANDLE handle;
    char *channelName;
    int permissions;
{
    SerialInfo *infoPtr;
    COMMTIMEOUTS cto;
    ThreadSpecificData *tsdPtr;
    DWORD id;

    tsdPtr = SerialInit();

    SetCommMask(handle, EV_RXCHAR);
    SetupComm(handle, 4096, 4096);
    PurgeComm(handle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR
	      | PURGE_RXCLEAR);
    cto.ReadIntervalTimeout = MAXDWORD;
    cto.ReadTotalTimeoutMultiplier = MAXDWORD;
    cto.ReadTotalTimeoutConstant = 1;
    cto.WriteTotalTimeoutMultiplier = 0;
    cto.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(handle, &cto);
    
    infoPtr = (SerialInfo *) ckalloc((unsigned) sizeof(SerialInfo));
    memset(infoPtr, 0, sizeof(SerialInfo));
    
    infoPtr->validMask = permissions;
    infoPtr->handle = handle;
	
    /*
     * Use the pointer to keep the channel names unique, in case
     * the handles are shared between multiple channels (stdin/stdout).
     */

    wsprintfA(channelName, "file%lx", (int) infoPtr);
	
    infoPtr->channel = Tcl_CreateChannel(&serialChannelType, channelName,
            (ClientData) infoPtr, permissions);
	

    infoPtr->threadId = Tcl_GetCurrentThread();
    
    if (permissions & TCL_READABLE) {
	infoPtr->readable = CreateEvent(NULL, TRUE, TRUE, NULL);
	infoPtr->startReader = CreateEvent(NULL, FALSE, FALSE, NULL);
	infoPtr->readThread = CreateThread(NULL, 8000, SerialReaderThread,
	        infoPtr, 0, &id);
    }
    if (permissions & TCL_WRITABLE) {
	infoPtr->writable = CreateEvent(NULL, TRUE, TRUE, NULL);
	infoPtr->startWriter = CreateEvent(NULL, FALSE, FALSE, NULL);
	infoPtr->writeThread = CreateThread(NULL, 8000, SerialWriterThread,
	        infoPtr, 0, &id);
    }

    /*
     * Files have default translation of AUTO and ^Z eof char, which
     * means that a ^Z will be accepted as EOF when reading.
     */
    
    Tcl_SetChannelOption(NULL, infoPtr->channel, "-translation", "auto");
    Tcl_SetChannelOption(NULL, infoPtr->channel, "-eofchar", "\032 {}");

    return infoPtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * SerialSetOptionProc --
 *
 *	Sets an option on a channel.
 *
 * Results:
 *	A standard Tcl result. Also sets the interp's result on error if
 *	interp is not NULL.
 *
 * Side effects:
 *	May modify an option on a device.
 *
 *----------------------------------------------------------------------
 */

static int		
SerialSetOptionProc(instanceData, interp, optionName, value)
    ClientData instanceData;	/* File state. */
    Tcl_Interp *interp;		/* For error reporting - can be NULL. */
    char *optionName;		/* Which option to set? */
    char *value;		/* New value for option. */
{
    SerialInfo *infoPtr;
    DCB dcb;
    int len;
    BOOL result;
    Tcl_DString ds;
    TCHAR *native;

    infoPtr = (SerialInfo *) instanceData;

    len = strlen(optionName);
    if ((len > 1) && (strncmp(optionName, "-mode", len) == 0)) {
	if (GetCommState(infoPtr->handle, &dcb)) {
	    native = Tcl_WinUtfToTChar(value, -1, &ds);
	    result = (*tclWinProcs->buildCommDCBProc)(native, &dcb);
	    Tcl_DStringFree(&ds);

	    if ((result == FALSE) || 
		    (SetCommState(infoPtr->handle, &dcb) == FALSE)) {
		/*
		 * one should separate the 2 errors... 
		 */

                if (interp) {
                    Tcl_AppendResult(interp, "bad value for -mode: should be ",
			    "baud,parity,data,stop", NULL);
		}
		return TCL_ERROR;
	    } else {
		return TCL_OK;
	    }
	} else {
	    if (interp) {
		Tcl_AppendResult(interp, "can't get comm state", NULL);
	    }
	    return TCL_ERROR;
	}
    } else {
	return Tcl_BadChannelOption(interp, optionName, "mode");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SerialGetOptionProc --
 *
 *	Gets a mode associated with an IO channel. If the optionName arg
 *	is non NULL, retrieves the value of that option. If the optionName
 *	arg is NULL, retrieves a list of alternating option names and
 *	values for the given channel.
 *
 * Results:
 *	A standard Tcl result. Also sets the supplied DString to the
 *	string value of the option(s) returned.
 *
 * Side effects:
 *	The string returned by this function is in static storage and
 *	may be reused at any time subsequent to the call.
 *
 *----------------------------------------------------------------------
 */
static int		
SerialGetOptionProc(instanceData, interp, optionName, dsPtr)
    ClientData instanceData;	/* File state. */
    Tcl_Interp *interp;          /* For error reporting - can be NULL. */
    char *optionName;		/* Option to get. */
    Tcl_DString *dsPtr;		/* Where to store value(s). */
{
    SerialInfo *infoPtr;
    DCB dcb;
    int len;

    infoPtr = (SerialInfo *) instanceData;

    if (optionName == NULL) {
	Tcl_DStringAppendElement(dsPtr, "-mode");
	len = 0;
    } else {
	len = strlen(optionName);
    }
    if ((len == 0) || 
	    ((len > 1) && (strncmp(optionName, "-mode", len) == 0))) {
	if (GetCommState(infoPtr->handle, &dcb) == 0) {
	    /*
	     * shouldn't we flag an error instead ? 
	     */

	    Tcl_DStringAppendElement(dsPtr, "");

	} else {
	    char parity;
	    char *stop;
	    char buf[2 * TCL_INTEGER_SPACE + 16];

	    parity = 'n';
	    if (dcb.Parity < 4) {
		parity = "noems"[dcb.Parity];
	    }

	    stop = (dcb.StopBits == ONESTOPBIT) ? "1" : 
		    (dcb.StopBits == ONE5STOPBITS) ? "1.5" : "2";

	    wsprintfA(buf, "%d,%c,%d,%s", dcb.BaudRate, parity, dcb.ByteSize,
		    stop);
	    Tcl_DStringAppendElement(dsPtr, buf);
	}
	return TCL_OK;
    } else {
	return Tcl_BadChannelOption(interp, optionName, "mode");
    }
}
