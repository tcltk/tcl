/*
 * tclWinConsole.c --
 *
 *	This file implements the Windows-specific console functions, and the
 *	"console" channel driver. Windows 7 or later required.
 *
 * Copyright © 2022 Ashok P. Nadkarni
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef TCL_CONSOLE_DEBUG
#undef NDEBUG /* Enable asserts */
#endif

#include "tclWinInt.h"
#include <assert.h>
#include <ctype.h>

/*
 * A general note on the design: The console channel driver differs from
 * most other drivers in the following respects:
 *
 * - There can be at most 3 console handles at any time since Windows does
 *   support allocation of more than one console (with three handles
 *   corresponding to stdin, stdout, stderr)
 *
 * - Consoles are created / inherited at process startup. There is currently
 *   no way in Tcl to programmatically create a console. Even if these were
 *   added the above Windows limitation would still apply.
 *
 * - Unlike files, sockets etc. where there is a one-to-one
 *   correspondence between Tcl channels and operating system handles,
 *   std* channels are shared amongst threads which means there can be
 *   multiple Tcl channels corresponding to a single console handle.
 *
 * - Even with multiple threads, more than one file event handler is
 * unlikely. It does not make sense for multiple threads to register
 * handlers for stdin because the input would be randomly fragmented amongst
 * the threads.
 *
 * Various design factors are driven by the above, e.g. use of lists instead
 * of hash tables (at most 3 console handles) and use of global instead of
 * per thread queues which simplifies lock management particularly because
 * thread-console relation is not one-one and is likely more performant as
 * well with fewer locks needing to be obtained.
 *
 * Some additional design notes/reminders for the future:
 *
 * Aligned, synchronous reads are done directly by interpreter thread.
 * Unaligned or asynchronous reads are done through the reader thread.
 *
 * The reader thread does not read ahead. That is, it will not post a read
 * until some interpreter thread is actually requesting a read. This is
 * because an interpreter may (for example) turn off echo for passwords and
 * the read ahead would come in the way of that.
 *
 * If multiple threads are reading from stdin, the input is sprayed in
 * random fashion. This is not good application design and hence no plan to
 * address this (not clear what should be done even in theory)
 *
 * For output, we do not restrict all output to the console writer threads.
 * See ConsoleOutputProc for the conditions.
 *
 * Locks are never held when calling the ReadConsole/WriteConsole API's
 * since they may block.
 */

static int gInitialized = 0;

/*
 * Permit CONSOLE_BUFFER_SIZE to be defined on build command for stress test.
 *
 * In theory, at least sizeof(WCHAR) but note the Tcl channel bug
 * https://core.tcl-lang.org/tcl/tktview/b3977d199b08e3979a8da970553d5209b3042e9c
 * will cause failures in test suite if close to max input line in the suite.
 */
#ifndef CONSOLE_BUFFER_SIZE
#define CONSOLE_BUFFER_SIZE 8000 /* In bytes */
#endif

/*
 * Ring buffer for storing data. Actual data is from bufPtr[start]:bufPtr[size-1]
 * and bufPtr[0]:bufPtr[length - (size-start)].
 */
#if TCL_MAJOR_VERSION > 8
typedef ptrdiff_t RingSizeT; /* Tcl9 TODO */
#define RingSizeT_MAX PTRDIFF_MAX
#else
typedef int RingSizeT;
#define RingSizeT_MAX INT_MAX
#endif
typedef struct RingBuffer {
    char *bufPtr;	/* Pointer to buffer storage */
    RingSizeT capacity;	/* Size of the buffer in RingBufferChar */
    RingSizeT start;	/* Start of the data within the buffer. */
    RingSizeT length;	/* Number of RingBufferChar*/
} RingBuffer;
#define RingBufferLength(ringPtr_) ((ringPtr_)->length)
#define RingBufferHasFreeSpace(ringPtr_) ((ringPtr_)->length < (ringPtr_)->capacity)
#define RINGBUFFER_ASSERT(ringPtr_) assert(RingBufferCheck(ringPtr_))

/*
 * The Win32 console API does not support non-blocking I/O in any form. Thus
 * the actual calls are made on a separate thread. Moreover, separate
 * threads are needed for each handle because (for example) blocking on user
 * input on stdin should not prevent output to stdout when non-blocking i/o
 * is configured at the script level.
 *
 * In the input (e.g. stdin) case, the console stdin thread is the producer
 * writing to the buffer ring buffer. The Tcl interpreter threads are the
 * consumer. For the output (e.g. stdout/stderr) case, the Tcl interpreter
 * are the producers while the console stdout/stderr threads are the
 * consumers.
 *
 * Consoles are identified purely by handles and multiple threads may open
 * them (as stdin/stdout/stderr are shared).
 *
 * Note on reference counting - a ConsoleHandleInfo instance has multiple
 * references to it - one each from every channel that is attached to it
 * plus one from the console thread itself which also serves as the reference
 * from gConsoleHandleInfoList.
 */
typedef struct ConsoleHandleInfo {
    struct ConsoleHandleInfo *nextPtr; /* Process-global list of consoles */
    HANDLE console;       /* Console handle */
    HANDLE consoleThread; /* Handle to thread doing actual i/o on the console */
    SRWLOCK lock;	  /* Controls access to this structure.
			   * Cheaper than CRITICAL_SECTION but note does not
			   * support recursive locks or Try* style attempts.*/
    CONDITION_VARIABLE consoleThreadCV;/* For awakening console thread */
    CONDITION_VARIABLE interpThreadCV; /* For awakening interpthread(s) */
    RingBuffer buffer;	  /* Buffer for data transferred between console
			   * threads and Tcl threads. For input consoles,
			   * written by the console thread and read by Tcl
			   * threads. The converse for output threads */
    DWORD initMode;	  /* Initial console mode. */
    DWORD lastError;	  /* An error caused by the last background
			   * operation. Set to 0 if no error has been
			   * detected. */
    int numRefs;	  /* See comments above */
    int permissions;	  /* TCL_READABLE for input consoles, TCL_WRITABLE
			   * for output. Only one or the other can be set. */
    int flags;
#define CONSOLE_DATA_AWAITED 0x0001 /* An interpreter is awaiting data */
} ConsoleHandleInfo;

/*
 * This structure describes per-instance data for a console based channel.
 *
 * Note on locking - this structure has no locks because it is accessed
 * only from the thread owning channel EXCEPT when a console traverses it
 * looking for a channel that is watching for events on the console. Even
 * in that case, no locking is required because that access is only under
 * the gConsoleLock lock which prevents the channel from being removed from
 * the gWatchingChannelList which in turn means it will not be deallocated
 * from under the console thread. Access to individual fields does not need
 * to be controlled because
 *   - the console thread does not write to any fields
 *   - changes to the nextWatchingChannelPtr field
 *   - changes to other fields do not matter because after being read for
 *     queueing events, they are verified again when the event is received
 *     in the interpreter thread (since they could have changed anyways while
 *     the event was in-flight on the event queue)
 *
 * Note on reference counting - a structure instance may be referenced from
 * three places:
 *   - the Tcl channel subsystem. This reference is created when on channel
 *     opening and dropped on channel close. This also covers the reference
 *     from gWatchingChannelList since queueing / dequeuing from that list
 *     happens in conjunction with channel operations.
 *   - the Tcl event queue entries. This reference is added when the event
 *     is queued and dropped on receipt.
 */
typedef struct ConsoleChannelInfo {
    HANDLE handle; 		/* Console handle */
    Tcl_ThreadId threadId;	/* Id of owning thread */
    struct ConsoleChannelInfo
	*nextWatchingChannelPtr; /* Pointer to next channel watching events. */
    Tcl_Channel channel;	/* Pointer to channel structure. */
    DWORD initMode;		/* Initial console mode. */
    int numRefs;		/* See comments above */
    int permissions;            /* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, or TCL_EXCEPTION: indicates
				 * which operations are valid on the file. */
    int watchMask;		/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, or TCL_EXCEPTION: indicates
				 * which events should be reported. */
    int flags;			/* State flags */
#define CONSOLE_EVENT_QUEUED 0x0001 /* Notification event already queued */
#define CONSOLE_ASYNC        0x0002 /* Channel is non-blocking. */
#define CONSOLE_READ_OPS     0x0004 /* Channel supports read-related ops. */
} ConsoleChannelInfo;

/*
 * The following structure is what is added to the Tcl event queue when
 * console events are generated.
 */

typedef struct {
    Tcl_Event header;	/* Information that is standard for all events. */
    ConsoleChannelInfo *chanInfoPtr; /* Pointer to console info structure. Note
				      * that we still have to verify that the
				      * console exists before dereferencing this
				      * pointer. */
} ConsoleEvent;

/*
 * Declarations for functions used only in this file.
 */

static int	ConsoleBlockModeProc(void *instanceData, int mode);
static void	ConsoleCheckProc(void *clientData, int flags);
static int	ConsoleCloseProc(void *instanceData,
		    Tcl_Interp *interp, int flags);
static int	ConsoleEventProc(Tcl_Event *evPtr, int flags);
static void	ConsoleExitHandler(void *clientData);
static int	ConsoleGetHandleProc(void *instanceData,
		    int direction, void **handlePtr);
static int	ConsoleGetOptionProc(void *instanceData,
		    Tcl_Interp *interp, const char *optionName,
		    Tcl_DString *dsPtr);
static void	ConsoleInit(void);
static int	ConsoleInputProc(void *instanceData, char *buf,
		    int toRead, int *errorCode);
static int	ConsoleOutputProc(void *instanceData,
		    const char *buf, int toWrite, int *errorCode);
static int	ConsoleSetOptionProc(void *instanceData,
		    Tcl_Interp *interp, const char *optionName,
		    const char *value);
static void	ConsoleSetupProc(void *clientData, int flags);
static void	ConsoleWatchProc(void *instanceData, int mask);
static void	ProcExitHandler(void *clientData);
static void	ConsoleThreadActionProc(void *instanceData, int action);
static DWORD	ReadConsoleChars(HANDLE hConsole, WCHAR *lpBuffer,
		    RingSizeT nChars, RingSizeT *nCharsReadPtr);
static DWORD	WriteConsoleChars(HANDLE hConsole,
		    const WCHAR *lpBuffer, RingSizeT nChars,
		    RingSizeT *nCharsWritten);
static void	RingBufferInit(RingBuffer *ringPtr, RingSizeT capacity);
static void	RingBufferClear(RingBuffer *ringPtr);
static RingSizeT	RingBufferIn(RingBuffer *ringPtr, const char *srcPtr,
			    RingSizeT srcLen, int partialCopyOk);
static RingSizeT	RingBufferOut(RingBuffer *ringPtr, char *dstPtr,
			    RingSizeT dstCapacity, int partialCopyOk);
static ConsoleHandleInfo *AllocateConsoleHandleInfo(HANDLE consoleHandle,
			    int permissions);
static ConsoleHandleInfo *FindConsoleInfo(const ConsoleChannelInfo *);
static DWORD WINAPI	ConsoleReaderThread(LPVOID arg);
static DWORD WINAPI	ConsoleWriterThread(LPVOID arg);
static void		NudgeWatchers(HANDLE consoleHandle);
#ifndef NDEBUG
static int	RingBufferCheck(const RingBuffer *ringPtr);
#endif

/*
 * Static data.
 */

typedef struct {
    /* Currently this struct is only used to detect thread initialization */
    int notUsed; /* Dummy field */
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;

/*
 * All access to static data is controlled through a single process-wide
 * lock. A process can have only a single console at a time, with three
 * handles for stdin, stdout and stderr. Creation/destruction of consoles is
 * a relatively rare event (currently only possible during process start),
 * the number of consoles (as opposed to channels) is small (only stdin,
 * stdout and stderr), and contention low. More finer-grained locking would
 * likely not only complicate implementation but be slower due to multiple
 * locks being held. Note console channels also differ from other Tcl
 * channel types in that the channel<->OS descriptor mapping is not one-to-one.
 */
SRWLOCK gConsoleLock;


/* Process-wide list of console handles. Access control through gConsoleLock */
static ConsoleHandleInfo *gConsoleHandleInfoList;

/*
 * Process-wide list of channels that are listening for events. Again access
 * control through gConsoleLock. Common list for all threads is simplifies
 * locking and bookkeeping and is workable because in practice multiple
 * threads are very unlikely to be all waiting on stdin (not workable
 * because input would be randomly distributed to threads)
 */
static ConsoleChannelInfo *gWatchingChannelList;

/*
 * This structure describes the channel type structure for command console
 * based IO.
 */

static const Tcl_ChannelType consoleChannelType = {
    "console",               /* Type name. */
    TCL_CHANNEL_VERSION_5,   /* v5 channel */
    NULL,                    /* Close proc. */
    ConsoleInputProc,        /* Input proc. */
    ConsoleOutputProc,       /* Output proc. */
    NULL,                    /* Seek proc. */
    ConsoleSetOptionProc,    /* Set option proc. */
    ConsoleGetOptionProc,    /* Get option proc. */
    ConsoleWatchProc,        /* Set up notifier to watch the channel. */
    ConsoleGetHandleProc,    /* Get an OS handle from channel. */
    ConsoleCloseProc,        /* close2proc. */
    ConsoleBlockModeProc,    /* Set blocking or non-blocking mode. */
    NULL,                    /* Flush proc. */
    NULL,                    /* Handler proc. */
    NULL,                    /* Wide seek proc. */
    ConsoleThreadActionProc, /* Thread action proc. */
    NULL                     /* Truncation proc. */
};

/*
 *------------------------------------------------------------------------
 *
 * RingBufferInit --
 *
 *    Initializes the ring buffer to a given size.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Panics on allocation failure.
 *
 *------------------------------------------------------------------------
 */
static void
RingBufferInit(RingBuffer *ringPtr, RingSizeT capacity)
{
    if (capacity <= 0 || capacity > RingSizeT_MAX) {
	Tcl_Panic("Internal error: invalid ring buffer capacity requested.");
    }
    ringPtr->bufPtr = (char *)Tcl_Alloc(capacity);
    ringPtr->capacity = capacity;
    ringPtr->start    = 0;
    ringPtr->length   = 0;
}

/*
 *------------------------------------------------------------------------
 *
 * RingBufferClear
 *
 *    Clears the contents of a ring buffer.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The allocated internal buffer is freed.
 *
 *------------------------------------------------------------------------
 */
static void
RingBufferClear(RingBuffer *ringPtr)
{
    if (ringPtr->bufPtr) {
	Tcl_Free(ringPtr->bufPtr);
	ringPtr->bufPtr = NULL;
    }
    ringPtr->capacity = 0;
    ringPtr->start    = 0;
    ringPtr->length   = 0;
}

/*
 *------------------------------------------------------------------------
 *
 * RingBufferIn --
 *
 *    Appends data to the ring buffer.
 *
 * Results:
 *    Returns number of bytes copied.
 *
 * Side effects:
 *    Internal buffer is updated.
 *
 *------------------------------------------------------------------------
 */
static RingSizeT
RingBufferIn(
    RingBuffer *ringPtr,
    const char *srcPtr, /* Source to be copied */
    RingSizeT srcLen,	  /* Length of source */
    int partialCopyOk 		  /* If true, partial copy is permitted */
    )
{
    RingSizeT freeSpace;

    RINGBUFFER_ASSERT(ringPtr);

    freeSpace = ringPtr->capacity - ringPtr->length;
    if (freeSpace < srcLen) {
	if (!partialCopyOk) {
	    return 0;
	}
	/* Copy only as much as free space allows */
	srcLen = freeSpace;
    }

    if (ringPtr->capacity - ringPtr->start > ringPtr->length) {
	/* There is room at the back */
	RingSizeT endSpaceStart = ringPtr->start + ringPtr->length;
	RingSizeT endSpace      = ringPtr->capacity - endSpaceStart;
	if (endSpace >= srcLen) {
	    /* Everything fits at the back */
	    memmove(endSpaceStart + ringPtr->bufPtr, srcPtr, srcLen);
	} else {
	    /* srcLen > endSpace */
	    memmove(endSpaceStart + ringPtr->bufPtr, srcPtr, endSpace);
	    memmove(ringPtr->bufPtr, endSpace + srcPtr, srcLen - endSpace);
	}
    } else {
	/* No room at the back. Existing data wrap to front. */
	RingSizeT wrapLen =
	    ringPtr->start + ringPtr->length - ringPtr->capacity;
	memmove(wrapLen + ringPtr->bufPtr, srcPtr, srcLen);
    }

    ringPtr->length += srcLen;

    RINGBUFFER_ASSERT(ringPtr);

    return srcLen;
}

/*
 *------------------------------------------------------------------------
 *
 * RingBufferOut --
 *
 *    Moves data out of the ring buffer.  If dstPtr is NULL, the data
 *    is simply removed.
 *
 * Results:
 *    Returns number of bytes copied or removed.
 *
 * Side effects:
 *    Internal buffer is updated.
 *
 *------------------------------------------------------------------------
 */
static RingSizeT
RingBufferOut(RingBuffer *ringPtr,
	      char *dstPtr,	      /* Buffer for output data. May be NULL */
	      RingSizeT dstCapacity,  /* Size of buffer */
	      int partialCopyOk)      /* If true, return what's available */
{
    RingSizeT leadLen;

    RINGBUFFER_ASSERT(ringPtr);

    if (dstCapacity > ringPtr->length) {
	if (dstPtr && !partialCopyOk) {
	    return 0;
	}
	dstCapacity = ringPtr->length;
    }

    if (ringPtr->start <= (ringPtr->capacity - ringPtr->length)) {
	/* No content wrap around. So leadLen is entire content */
	leadLen = ringPtr->length;
    } else {
	/* Content wraps around so lead segment stretches to end of buffer */
	leadLen = ringPtr->capacity - ringPtr->start;
    }
    if (leadLen >= dstCapacity) {
	if (dstPtr) {
	    memmove(dstPtr, ringPtr->start + ringPtr->bufPtr, dstCapacity);
	}
	ringPtr->start += dstCapacity;
    } else {
	RingSizeT wrapLen = dstCapacity - leadLen;
	if (dstPtr) {
	    memmove(dstPtr,
		    ringPtr->start + ringPtr->bufPtr,
		    leadLen);
	    memmove(
		leadLen + dstPtr, ringPtr->bufPtr, wrapLen);
	}
	ringPtr->start = wrapLen;
    }

    ringPtr->length -= dstCapacity;
    if (ringPtr->start == ringPtr->capacity || ringPtr->length == 0) {
	ringPtr->start = 0;
    }

    RINGBUFFER_ASSERT(ringPtr);

    return dstCapacity;
}

#ifndef NDEBUG
static int
RingBufferCheck(const RingBuffer *ringPtr)
{
    return (ringPtr->bufPtr != NULL && ringPtr->capacity == CONSOLE_BUFFER_SIZE
	    && ringPtr->start < ringPtr->capacity
	    && ringPtr->length <= ringPtr->capacity);
}
#endif

/*
 *------------------------------------------------------------------------
 *
 * ReadConsoleChars --
 *
 *    Wrapper for ReadConsoleW.
 *
 * Results:
 *    Returns 0 on success, else Windows error code.
 *
 * Side effects:
 *    On success the number of characters (not bytes) read is stored in
 *    *nCharsReadPtr. This will be 0 if the operation was interrupted by
 *    a Ctrl-C or a CancelIo call.
 *
 *------------------------------------------------------------------------
 */
static DWORD
ReadConsoleChars(
    HANDLE hConsole,
    WCHAR *lpBuffer,
    RingSizeT nChars,
    RingSizeT *nCharsReadPtr)
{
    DWORD nRead;
    BOOL result;

    /*
     * If user types a Ctrl-Break or Ctrl-C, ReadConsole will return success
     * with ntchars == 0 and GetLastError() will be ERROR_OPERATION_ABORTED.
     * If no Ctrl signal handlers have been established, the default signal
     * OS handler in a separate thread will terminate the program. If a Ctrl
     * signal handler has been established (through an extension for
     * example), it will run and take whatever action it deems appropriate.
     *
     * If one thread closes its channel, it calls CancelSynchronousIo on the
     * console handle which results again in success being returned and
     * GetLastError() being ERROR_OPERATION_ABORTED but ntchars in
     * unmodified.
     *
     * In both cases above we will return success but with nbytesread as 0.
     * This allows caller to check for thread termination etc.
     *
     * See https://bugs.python.org/issue30237
     * or https://github.com/microsoft/terminal/issues/12143
     */
    nRead = (DWORD)-1;
    result = ReadConsoleW(hConsole, lpBuffer, nChars, &nRead, NULL);
    if (result) {
	if ((nRead == 0 || nRead == (DWORD)-1)
	    && GetLastError() == ERROR_OPERATION_ABORTED) {
	    nRead = 0;
	}
	*nCharsReadPtr = nRead;
	return 0;
    } else
	return GetLastError();
}

/*
 *------------------------------------------------------------------------
 *
 * WriteConsoleChars --
 *
 *    Wrapper for WriteConsoleW.
 *
 * Results:
 *    Returns 0 on success, Windows error code on failure.
 *
 * Side effects:
 *    On success the number of characters (not bytes) written is stored in
 *    *nCharsWrittenPtr. This will be 0 if the operation was interrupted by
 *    a Ctrl-C or a CancelIo call.
 *
 *------------------------------------------------------------------------
 */

static DWORD
WriteConsoleChars(
    HANDLE hConsole,
    const WCHAR *lpBuffer,
    RingSizeT nChars,
    RingSizeT *nCharsWrittenPtr)
{
    DWORD nCharsWritten;
    BOOL result;

    /* See comments in ReadConsoleChars, not sure that applies here */
    nCharsWritten = (DWORD)-1;
    result = WriteConsoleW(hConsole, lpBuffer, nChars, &nCharsWritten, NULL);
    if (result) {
	if (nCharsWritten == (DWORD) -1) {
	    nCharsWritten = 0;
	}
	*nCharsWrittenPtr = nCharsWritten;
	return 0;
    } else {
	return GetLastError();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleInit --
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

static void
ConsoleInit(void)
{
    /*
     * Check the initialized flag first, then check again in the mutex. This
     * is a speed enhancement.
     */

    if (!gInitialized) {
	AcquireSRWLockExclusive(&gConsoleLock);
	if (!gInitialized) {
	    gInitialized = 1;
	    Tcl_CreateExitHandler(ProcExitHandler, NULL);
	}
	ReleaseSRWLockExclusive(&gConsoleLock);
    }

    if (TclThreadDataKeyGet(&dataKey) == NULL) {
	ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

	tsdPtr->notUsed = 0;
	Tcl_CreateEventSource(ConsoleSetupProc, ConsoleCheckProc, NULL);
	Tcl_CreateThreadExitHandler(ConsoleExitHandler, NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleExitHandler --
 *
 *	This function is called to cleanup the console module before Tcl is
 *	unloaded.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes the console event source.
 *
 *----------------------------------------------------------------------
 */

static void
ConsoleExitHandler(
    TCL_UNUSED(void *))
{
    Tcl_DeleteEventSource(ConsoleSetupProc, ConsoleCheckProc, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * ProcExitHandler --
 *
 *	This function is called to cleanup the process list before Tcl is
 *	unloaded.
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
    TCL_UNUSED(void *))
{
    AcquireSRWLockExclusive(&gConsoleLock);
    gInitialized = 0;
    ReleaseSRWLockExclusive(&gConsoleLock);
}

/*
 *------------------------------------------------------------------------
 *
 * NudgeWatchers --
 *
 *    Wakes up all threads which have file event watchers on the passed
 *    console handle.
 *
 *    The function locks and releases gConsoleLock.
 *    Caller must not be holding locks that will violate lock hierarchy.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    As above.
 *------------------------------------------------------------------------
 */
void NudgeWatchers (HANDLE consoleHandle)
{
    ConsoleChannelInfo *chanInfoPtr;
    AcquireSRWLockShared(&gConsoleLock); /* Shared-read lock */
    for (chanInfoPtr = gWatchingChannelList; chanInfoPtr;
	 chanInfoPtr = chanInfoPtr->nextWatchingChannelPtr) {
	/*
	 * Notify channels interested in our handle AND that have
	 * a thread attached.
	 * No lock needed for chanInfoPtr. See ConsoleChannelInfo.
	 */
	if (chanInfoPtr->handle == consoleHandle
	    && chanInfoPtr->threadId != NULL) {
	    Tcl_ThreadAlert(chanInfoPtr->threadId);
	}
    }
    ReleaseSRWLockShared(&gConsoleLock);
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleSetupProc --
 *
 *	This procedure is invoked before Tcl_DoOneEvent blocks waiting for an
 *	event. It walks the channel list and if any input channel has data
 *      available or output channel has space for data, sets the event loop
 *      blocking time to 0 so that it will poll immediately.
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
ConsoleSetupProc(
    TCL_UNUSED(void *),
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    ConsoleChannelInfo *chanInfoPtr;
    Tcl_Time blockTime = { 0, 0 };
    int block = 1;

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }

    /*
     * Walk the list of channels. See general comments for struct
     * ConsoleChannelInfo with regard to locking and field access.
     */
    AcquireSRWLockShared(&gConsoleLock); /* READ lock - no data modification */

    for (chanInfoPtr = gWatchingChannelList; block && chanInfoPtr != NULL;
	 chanInfoPtr = chanInfoPtr->nextWatchingChannelPtr) {
	ConsoleHandleInfo *handleInfoPtr;
	handleInfoPtr = FindConsoleInfo(chanInfoPtr);
	if (handleInfoPtr != NULL) {
	    AcquireSRWLockShared(&handleInfoPtr->lock);
	    /* Remember at most one of READABLE, WRITABLE set */
	    if (chanInfoPtr->watchMask & TCL_READABLE) {
		if (RingBufferLength(&handleInfoPtr->buffer) > 0
		    || handleInfoPtr->lastError != ERROR_SUCCESS) {
		    block = 0; /* Input data available */
		}
	    } else if (chanInfoPtr->watchMask & TCL_WRITABLE) {
		if (RingBufferHasFreeSpace(&handleInfoPtr->buffer)) {
		    /* TCL_WRITABLE */
		    block = 0; /* Output space available */
		}
	    }
	    ReleaseSRWLockShared(&handleInfoPtr->lock);
	}
    }
    ReleaseSRWLockShared(&gConsoleLock);

    if (!block) {
	/* At least one channel is readable/writable. Set block time to 0 */
	Tcl_SetMaxBlockTime(&blockTime);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleCheckProc --
 *
 *	This procedure is called by Tcl_DoOneEvent to check the console event
 *	source for events.
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
ConsoleCheckProc(
    TCL_UNUSED(void *),
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    ConsoleChannelInfo *chanInfoPtr;
    Tcl_ThreadId me;
    int needEvent;

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }

    me = Tcl_GetCurrentThread();

    /*
     * Acquire a shared lock. Note this is ok even though we potentially
     * modify the chanInfoPtr->flags because chanInfoPtr is only modified
     * when it belongs to this thread and no other thread will write to it.
     * THe shared lock is intended to protect the global gWatchingChannelList
     * as we traverse it.
     */
    AcquireSRWLockShared(&gConsoleLock);

    for (chanInfoPtr = gWatchingChannelList; chanInfoPtr != NULL;
	    chanInfoPtr = chanInfoPtr->nextWatchingChannelPtr) {
	ConsoleHandleInfo *handleInfoPtr;

	if (chanInfoPtr->threadId != me) {
	    /* Some other thread owns the channel */
	    continue;
	}
	if (chanInfoPtr->flags & CONSOLE_EVENT_QUEUED) {
	    /* A notification event already queued. No point in another. */
	    continue;
	}

	handleInfoPtr = FindConsoleInfo(chanInfoPtr);
	/* Pointer is safe to access as we are holding gConsoleLock */

	if (handleInfoPtr == NULL) {
	    /* Stale event */
	    continue;
	}

	needEvent = 0;
	AcquireSRWLockShared(&handleInfoPtr->lock);
	/* Rememeber channel is read or write, never both */
	if (chanInfoPtr->watchMask & TCL_READABLE) {
	    if (RingBufferLength(&handleInfoPtr->buffer) > 0
		|| handleInfoPtr->lastError != ERROR_SUCCESS) {
		needEvent = 1; /* Input data available or error/EOF */
	    }
	    /*
	     * TCL_READABLE watch means someone is looking out for data being
	     * available, let reader thread know. Note channel need not be
	     * ASYNC! (Bug [baa51423c2])
	     */
	    handleInfoPtr->flags |= CONSOLE_DATA_AWAITED;
	    WakeConditionVariable(&handleInfoPtr->consoleThreadCV);
	} else if (chanInfoPtr->watchMask & TCL_WRITABLE) {
	    if (RingBufferHasFreeSpace(&handleInfoPtr->buffer)) {
		needEvent = 1; /* Output space available */
	    }
	}
	ReleaseSRWLockShared(&handleInfoPtr->lock);

	if (needEvent) {
	    ConsoleEvent *evPtr = (ConsoleEvent *)Tcl_Alloc(sizeof(ConsoleEvent));

	    /* See note above loop why this can be accessed without locks */
	    chanInfoPtr->flags |= CONSOLE_EVENT_QUEUED;
	    chanInfoPtr->numRefs += 1; /* So it does not go away while event
					  is in queue */
	    evPtr->header.proc = ConsoleEventProc;
	    evPtr->chanInfoPtr = chanInfoPtr;
	    Tcl_QueueEvent((Tcl_Event *) evPtr, TCL_QUEUE_TAIL);
	}
    }

    ReleaseSRWLockShared(&gConsoleLock);
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleBlockModeProc --
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
ConsoleBlockModeProc(
    void *instanceData,	/* Instance data for channel. */
    int mode)			/* TCL_MODE_BLOCKING or
				 * TCL_MODE_NONBLOCKING. */
{
    ConsoleChannelInfo *chanInfoPtr = (ConsoleChannelInfo *)instanceData;

    /*
     * Consoles on Windows can not be switched between blocking and
     * nonblocking, hence we have to emulate the behavior. This is done in the
     * input function by checking against a bit in the state. We set or unset
     * the bit here to cause the input function to emulate the correct
     * behavior.
     */

    if (mode == TCL_MODE_NONBLOCKING) {
	chanInfoPtr->flags |= CONSOLE_ASYNC;
    } else {
	chanInfoPtr->flags &= ~CONSOLE_ASYNC;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleCloseProc --
 *
 *	Closes a console based IO channel.
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
ConsoleCloseProc(
    void *instanceData,	/* Pointer to ConsoleChannelInfo structure. */
    TCL_UNUSED(Tcl_Interp *),
    int flags)
{
    ConsoleChannelInfo *chanInfoPtr = (ConsoleChannelInfo *)instanceData;
    ConsoleHandleInfo *handleInfoPtr;
    int errorCode = 0;
    ConsoleChannelInfo **nextPtrPtr;
    int closeHandle;

    if ((flags & (TCL_CLOSE_READ | TCL_CLOSE_WRITE)) != 0) {
	return EINVAL;
    }
    /*
     * Don't close the Win32 handle if the handle is a standard channel
     * during the thread exit process. Otherwise, one thread may kill the
     * stdio of another while exiting. Note an explicit close in script will
     * still close the handle. That's historical behavior on all platforms.
     */
    if (!TclInThreadExit()
	|| ((GetStdHandle(STD_INPUT_HANDLE) != chanInfoPtr->handle)
	    && (GetStdHandle(STD_OUTPUT_HANDLE) != chanInfoPtr->handle)
	    && (GetStdHandle(STD_ERROR_HANDLE) != chanInfoPtr->handle))) {
	closeHandle = 1;
    } else {
	closeHandle = 0;
    }

    AcquireSRWLockExclusive(&gConsoleLock);

    /* Remove channel from watchers' list */
    for (nextPtrPtr = &gWatchingChannelList; *nextPtrPtr != NULL;
	 nextPtrPtr = &(*nextPtrPtr)->nextWatchingChannelPtr) {
	if (*nextPtrPtr == (ConsoleChannelInfo *) chanInfoPtr) {
	    *nextPtrPtr = (*nextPtrPtr)->nextWatchingChannelPtr;
	    break;
	}
    }

    handleInfoPtr = FindConsoleInfo(chanInfoPtr);
    if (handleInfoPtr) {
	/*
	 * Console thread may be blocked either waiting for console i/o
	 * or waiting on the condition variable for buffer empty/full
	 */
	AcquireSRWLockShared(&handleInfoPtr->lock);

	if (closeHandle) {
	    handleInfoPtr->console = INVALID_HANDLE_VALUE;
	}

	/* Break the thread out of blocking console i/o */
	handleInfoPtr->numRefs -= 1; /* Remove reference from this channel */
	if (handleInfoPtr->numRefs == 1) {
	    /*
	     * Abort the i/o if no other threads are listening on it.
	     * Note without this check, an input line will be skipped on
	     * the cancel.
	     */
	    CancelSynchronousIo(handleInfoPtr->consoleThread);
	}

	/*
	 * Wake up the console handling thread. Note we do not explicitly
	 * tell it handle is closed (below). It will find out on next access
	 */
	WakeConditionVariable(&handleInfoPtr->consoleThreadCV);

	ReleaseSRWLockShared(&handleInfoPtr->lock);
    }

    ReleaseSRWLockExclusive(&gConsoleLock);

    chanInfoPtr->channel     = NULL;
    chanInfoPtr->watchMask   = 0;
    chanInfoPtr->permissions = 0;

    if (closeHandle && chanInfoPtr->handle != INVALID_HANDLE_VALUE) {
	if (CloseHandle(chanInfoPtr->handle) == FALSE) {
	    Tcl_WinConvertError(GetLastError());
	    errorCode = errno;
	}
	chanInfoPtr->handle = INVALID_HANDLE_VALUE;
    }

    /*
     * Note, we can check and manipulate numRefs without a lock because
     * we have removed it from the watch queue so the console thread cannot
     * get at it.
     */
    if (chanInfoPtr->numRefs > 1) {
	/* There may be references already on the event queue */
	chanInfoPtr->numRefs -= 1;
    } else {
	Tcl_Free(chanInfoPtr);
    }

    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleInputProc --
 *
 *	Reads input from the IO channel into the buffer given. Returns count
 *	of how many bytes were actually read, and an error indication.
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
ConsoleInputProc(
    void *instanceData,	/* Console state. */
    char *bufPtr,		/* Where to store data read. */
    int bufSize,		/* How much space is available in the
				 * buffer? */
    int *errorCode)		/* Where to store error code. */
{
    ConsoleChannelInfo *chanInfoPtr = (ConsoleChannelInfo *)instanceData;
    ConsoleHandleInfo *handleInfoPtr;
    RingSizeT numRead;

    if (chanInfoPtr->handle == INVALID_HANDLE_VALUE) {
	return 0; /* EOF */
    }

    *errorCode = 0;

    AcquireSRWLockShared(&gConsoleLock);
    handleInfoPtr = FindConsoleInfo(chanInfoPtr);
    if (handleInfoPtr == NULL) {
	/* Really shouldn't happen since channel is holding a reference */
	ReleaseSRWLockShared(&gConsoleLock);
	return 0; /* EOF */
    }
    AcquireSRWLockExclusive(&handleInfoPtr->lock);
    ReleaseSRWLockShared(&gConsoleLock); /* AFTER acquiring handleInfoPtr->lock */

    while (1) {
	numRead = RingBufferOut(&handleInfoPtr->buffer, bufPtr, bufSize, 1);
	/*
	 * Note: even if channel is closed or has an error, as long there is
	 * buffered data, we will pass it up.
	 */
	if (numRead != 0) {
	    break;
	}
	/*
	 * No data available.
	 *  - If an error was recorded, generate that and reset it.
	 *  - If EOF, indicate as much. It is up to the application to close
	 *    the channel.
	 *  - Otherwise, if non-blocking return EAGAIN or wait for more data.
	 */
	if (handleInfoPtr->lastError != 0) {
	    if (handleInfoPtr->lastError == ERROR_INVALID_HANDLE) {
		numRead = 0; /* Treat as EOF */
	    } else {
		Tcl_WinConvertError(handleInfoPtr->lastError);
		handleInfoPtr->lastError = 0;
		*errorCode = Tcl_GetErrno();
		numRead = -1;
	    }
	    break;
	}
	if (handleInfoPtr->console == INVALID_HANDLE_VALUE) {
	    /* EOF - break with numRead == 0 */
	    chanInfoPtr->handle = INVALID_HANDLE_VALUE;
	    break;
	}

	/* For async, tell caller we are blocked */
	if (chanInfoPtr->flags & CONSOLE_ASYNC) {
	    *errorCode = EWOULDBLOCK;
	    numRead = -1;
	    break;
	}

	/*
	 * Blocking read. Just get data from directly from console. There
	 * is a small complication in that we can only read even number
	 * of bytes (wide-character API) and the destination buffer should be
	 * WCHAR aligned. If either condition is not met, we defer to the
	 * reader thread which handles these case rather than dealing with
	 * them here (which is a little trickier than it might sound.)
	 */
	if ((1 & (ptrdiff_t)bufPtr) == 0 /* aligned buffer */
	    && bufSize > 1         /* Not single byte read */
	) {
	    DWORD lastError;
	    RingSizeT numChars;
	    ReleaseSRWLockExclusive(&handleInfoPtr->lock);
	    lastError = ReadConsoleChars(chanInfoPtr->handle,
					 (WCHAR *)bufPtr,
					 bufSize / sizeof(WCHAR),
					 &numChars);
	    /* NOTE lock released so DON'T break. Return instead */
	    if (lastError != ERROR_SUCCESS) {
		Tcl_WinConvertError(lastError);
		*errorCode = Tcl_GetErrno();
		return -1;
	    } else if (numChars > 0) {
		/* Successfully read something. */
		return numChars * sizeof(WCHAR);
	    } else {
		/*
		 * Ctrl-C/Ctrl-Brk interrupt. Loop around to retry.
		 * We have to reacquire the lock. No worried about handleInfoPtr
		 * having gone away since the channel holds a reference.
		 */
		AcquireSRWLockExclusive(&handleInfoPtr->lock);
		continue;
	    }
	}
	/*
	 * Deferring blocking read to reader thread.
	 * Release the lock and sleep. Note that because the channel
	 * holds a reference count on handleInfoPtr, it will not
	 * be deallocated while the lock is released.
	 */
	handleInfoPtr->flags |= CONSOLE_DATA_AWAITED;
	WakeConditionVariable(&handleInfoPtr->consoleThreadCV);
	if (!SleepConditionVariableSRW(&handleInfoPtr->interpThreadCV,
				       &handleInfoPtr->lock,
				       INFINITE,
				       0)) {
	    Tcl_WinConvertError(GetLastError());
	    *errorCode = Tcl_GetErrno();
	    numRead = -1;
	    break;
	}

	/* Lock is reacquired, loop back to try again */
    }

    /* We read data. Ask for more if either async or watching for reads */
    if ((chanInfoPtr->flags & CONSOLE_ASYNC)
	|| (chanInfoPtr->watchMask & TCL_READABLE)) {
	handleInfoPtr->flags |= CONSOLE_DATA_AWAITED;
	WakeConditionVariable(&handleInfoPtr->consoleThreadCV);
    }

    ReleaseSRWLockExclusive(&handleInfoPtr->lock);
    return numRead;
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleOutputProc --
 *
 *	Writes the given output on the IO channel. Returns count of how many
 *	characters were actually written, and an error indication.
 *
 * Results:
 *	A count of how many characters were written is returned and an error
 *	indication is returned in an output argument.
 *
 * Side effects:
 *	Writes output on the actual channel.
 *
 *----------------------------------------------------------------------
 */
static int
ConsoleOutputProc(
    void *instanceData,	/* Console state. */
    const char *buf,		/* The data buffer. */
    int toWrite,		/* How many bytes to write? */
    int *errorCode)		/* Where to store error code. */
{
    ConsoleChannelInfo *chanInfoPtr = (ConsoleChannelInfo *)instanceData;
    ConsoleHandleInfo *handleInfoPtr;
    RingSizeT numWritten;

    *errorCode = 0;

    if (chanInfoPtr->handle == INVALID_HANDLE_VALUE) {
	/* Some other thread would have *previously* closed the stdio handle */
	*errorCode = EPIPE;
	return -1;
    }

    AcquireSRWLockShared(&gConsoleLock);
    handleInfoPtr = FindConsoleInfo(chanInfoPtr);
    if (handleInfoPtr == NULL) {
	/* Really shouldn't happen since channel is holding a reference */
	*errorCode = EPIPE;
	ReleaseSRWLockShared(&gConsoleLock);
	return -1;
    }
    AcquireSRWLockExclusive(&handleInfoPtr->lock);
    ReleaseSRWLockShared(&gConsoleLock); /* AFTER acquiring handleInfoPtr->lock */

    /* Keep looping until all written. Break out for async and errors */
    numWritten = 0;
    while (1) {
	/* Check for error and closing on every loop. */
	if (handleInfoPtr->lastError != 0) {
	    Tcl_WinConvertError(handleInfoPtr->lastError);
	    *errorCode = Tcl_GetErrno();
	    numWritten = -1;
	    break;
	}
	if (handleInfoPtr->console == INVALID_HANDLE_VALUE) {
	    *errorCode = EPIPE;
	    chanInfoPtr->handle = INVALID_HANDLE_VALUE;
	    numWritten = -1;
	    break;
	}

	/*
	 * We can either write directly or through the console thread's
	 * ring buffer. We have to do the latter when
	 * (1) the operation is async since WriteConsoleChars is always blocking
	 * (2) when there is already data in the ring buffer because we don't
	 *     want to reorder output from within a thread
	 * (3) when there are an odd number of bytes since WriteConsole
	 *     takes whole WCHARs
	 * (4) when the pointer is not aligned on WCHAR
	 * The ring buffer deals with cases (3) and (4). It would be harder
	 * to duplicate that here.
	 */
	if ((chanInfoPtr->flags & CONSOLE_ASYNC)              /* Case (1) */
	    || RingBufferLength(&handleInfoPtr->buffer) != 0  /* Case (2) */
	    || (toWrite & 1) != 0                             /* Case (3) */
	    || (PTR2INT(buf) & 1) != 0                        /* Case (4) */
	    ) {
	    numWritten += RingBufferIn(&handleInfoPtr->buffer,
				       numWritten + buf,
				       toWrite - numWritten,
				       1);
	    if (numWritten == toWrite || chanInfoPtr->flags & CONSOLE_ASYNC) {
		/* All done or async, just accept whatever was written */
		break;
	    }
	    /*
	     * Release the lock and sleep. Note that because the channel
	     * holds a reference count on handleInfoPtr, it will not
	     * be deallocated while the lock is released.
	     */
	    WakeConditionVariable(&handleInfoPtr->consoleThreadCV);
	    if (!SleepConditionVariableSRW(&handleInfoPtr->interpThreadCV,
					   &handleInfoPtr->lock,
					   INFINITE,
					   0)) {
		/* Report the error */
		Tcl_WinConvertError(GetLastError());
		*errorCode = Tcl_GetErrno();
		numWritten = -1;
		break;
	    }
	} else {
	    /* Direct output */
	    DWORD winStatus;
	    HANDLE consoleHandle = handleInfoPtr->console;
	    /* Unlock before blocking in WriteConsole */
	    ReleaseSRWLockExclusive(&handleInfoPtr->lock);
	    /* UNLOCKED so return, DON'T break out of loop as it will unlock again! */
	    winStatus = WriteConsoleChars(consoleHandle,
					  (WCHAR *)buf,
					  toWrite / sizeof(WCHAR),
					  &numWritten);
	    if (winStatus == ERROR_SUCCESS) {
		return numWritten * sizeof(WCHAR);
	    } else {
		Tcl_WinConvertError(winStatus);
		*errorCode = Tcl_GetErrno();
		return -1;
	    }
	}

	/* Lock must have been reacquired before continuing loop */
    }
    WakeConditionVariable(&handleInfoPtr->consoleThreadCV);
    ReleaseSRWLockExclusive(&handleInfoPtr->lock);
    return numWritten;
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleEventProc --
 *
 *	This function is invoked by Tcl_ServiceEvent when a file event reaches
 *	the front of the event queue. This procedure invokes Tcl_NotifyChannel
 *	on the console.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed from
 *	the queue. Returns 0 if the event was not handled, meaning it should
 *	stay on the queue. The only time the event isn't handled is if the
 *	TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the notifier callback does.
 *
 *----------------------------------------------------------------------
 */

static int
ConsoleEventProc(
    Tcl_Event *evPtr,		/* Event to service. */
    int flags)			/* Flags that indicate what events to handle,
				 * such as TCL_FILE_EVENTS. */
{
    ConsoleEvent *consoleEvPtr = (ConsoleEvent *) evPtr;
    ConsoleChannelInfo *chanInfoPtr;
    int freeChannel;
    int mask = 0;

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }

    chanInfoPtr = consoleEvPtr->chanInfoPtr;
    /*
     * We know chanInfoPtr is valid because its reference count would have
     * been incremented when the event was queued. The corresponding release
     * happens in this function.
     */

    /*
     * Global lock used for chanInfoPtr. A read (shared) lock suffices
     * because all access is within the channel owning thread with the
     * exception of watchers which is a read-only access. See comments
     * to ConsoleChannelInfo.
     */
    AcquireSRWLockShared(&gConsoleLock);
    chanInfoPtr->flags &= ~CONSOLE_EVENT_QUEUED;

    /*
     * Only handle the event if the Tcl channel has not gone away AND is
     * still owned by this thread AND is still watching events.
     */
    if (chanInfoPtr->channel && chanInfoPtr->threadId == Tcl_GetCurrentThread()
	&& (chanInfoPtr->watchMask & (TCL_READABLE|TCL_WRITABLE))) {
	ConsoleHandleInfo *handleInfoPtr;
	handleInfoPtr = FindConsoleInfo(chanInfoPtr);
	if (handleInfoPtr == NULL) {
	    /* Console was closed. EOF->read event only (not write) */
	    if (chanInfoPtr->watchMask & TCL_READABLE) {
		mask = TCL_READABLE;
	    }
	} else {
	    AcquireSRWLockShared(&handleInfoPtr->lock);
	    /* Remember at most one of READABLE, WRITABLE set */
	    if ((chanInfoPtr->watchMask & TCL_READABLE)
		&& RingBufferLength(&handleInfoPtr->buffer)) {
		mask = TCL_READABLE;
	    } else if ((chanInfoPtr->watchMask & TCL_WRITABLE)
		     && RingBufferHasFreeSpace(&handleInfoPtr->buffer)) {
		/* Generate write event space available */
		mask = TCL_WRITABLE;
	    }
	    ReleaseSRWLockShared(&handleInfoPtr->lock);
	}
    }

    /*
     * Tcl_NotifyChannel can recurse through the file event callback so need
     * to release locks first. Our reference still holds so no danger of
     * chanInfoPtr being deallocated if the callback closes the channel.
     */
    ReleaseSRWLockShared(&gConsoleLock);
    if (mask) {
	Tcl_NotifyChannel(chanInfoPtr->channel, mask);
	/* Note: chanInfoPtr ref count may have changed */
    }

    /* No need to lock - see comments earlier */

    /* Remove the reference to the channel from event record */
    if (chanInfoPtr->numRefs > 1) {
	chanInfoPtr->numRefs -= 1;
	freeChannel = 0;
    } else {
	assert(chanInfoPtr->channel == NULL);
	freeChannel = 1;
    }

    if (freeChannel) {
	Tcl_Free(chanInfoPtr);
    }

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleWatchProc --
 *
 *	Called by the notifier to set up to watch for events on this channel.
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
ConsoleWatchProc(
    void *instanceData,	/* Console state. */
    int newMask)		/* What events to watch for, one of
				 * of TCL_READABLE, TCL_WRITABLE
				 */
{
    ConsoleChannelInfo **nextPtrPtr, *ptr;
    ConsoleChannelInfo *chanInfoPtr = (ConsoleChannelInfo *)instanceData;
    int oldMask = chanInfoPtr->watchMask;

    /*
     * Since most of the work is handled by the background threads, we just
     * need to update the watchMask and then force the notifier to poll once.
     */

    chanInfoPtr->watchMask = newMask & chanInfoPtr->permissions;
    if (chanInfoPtr->watchMask) {
	Tcl_Time blockTime = { 0, 0 };

	if (!oldMask) {
	    AcquireSRWLockExclusive(&gConsoleLock);
	    /* Add to list of watched channels */
	    chanInfoPtr->nextWatchingChannelPtr = gWatchingChannelList;
	    gWatchingChannelList = chanInfoPtr;

	    /*
	     * For read channels, need to tell the console reader thread
	     * that we are looking for data since it will not do reads until
	     * it knows someone is awaiting.
	     */
	    ConsoleHandleInfo *handleInfoPtr;
	    handleInfoPtr = FindConsoleInfo(chanInfoPtr);
	    if (handleInfoPtr) {
		AcquireSRWLockExclusive(&handleInfoPtr->lock);
		handleInfoPtr->flags |= CONSOLE_DATA_AWAITED;
		WakeConditionVariable(&handleInfoPtr->consoleThreadCV);
		ReleaseSRWLockExclusive(&handleInfoPtr->lock);
	    }
	    ReleaseSRWLockExclusive(&gConsoleLock);
	}
	Tcl_SetMaxBlockTime(&blockTime);
    } else if (oldMask) {
	/* Remove from list of watched channels */

	AcquireSRWLockExclusive(&gConsoleLock);
	for (nextPtrPtr = &gWatchingChannelList, ptr = *nextPtrPtr;
		ptr != NULL;
		nextPtrPtr = &ptr->nextWatchingChannelPtr, ptr = *nextPtrPtr) {
	    if (chanInfoPtr == ptr) {
		*nextPtrPtr = ptr->nextWatchingChannelPtr;
		break;
	    }
	}
	ReleaseSRWLockExclusive(&gConsoleLock);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleGetHandleProc --
 *
 *	Called from Tcl_GetChannelHandle to retrieve OS handles from inside a
 *	command consoleline based channel.
 *
 * Results:
 *	Returns TCL_OK with the fd in handlePtr, or TCL_ERROR if there is no
 *	handle for the specified direction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ConsoleGetHandleProc(
    void *instanceData,	/* The console state. */
    TCL_UNUSED(int) /*direction*/,
    void **handlePtr)	/* Where to store the handle. */
{
    ConsoleChannelInfo *chanInfoPtr = (ConsoleChannelInfo *)instanceData;

    if (chanInfoPtr->handle == INVALID_HANDLE_VALUE) {
	return TCL_ERROR;
    } else {
	*handlePtr = chanInfoPtr->handle;
	return TCL_OK;
    }
}

/*
 *------------------------------------------------------------------------
 *
 * ConsoleDataAvailable --
 *
 *    Checks if there is data in the console input queue.
 *
 * Results:
 *    Returns 1 if the input queue has data, -1 on error else 0 if empty.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------
 */
 static int
 ConsoleDataAvailable (HANDLE consoleHandle)
{
    INPUT_RECORD input[10];
    DWORD count;
    DWORD i;

    /*
     * Need at least one keyboard event.
     */
    if (PeekConsoleInputW(
	    consoleHandle, input, sizeof(input) / sizeof(input[0]), &count)
	== FALSE) {
	return -1;
    }
    /*
     * Even if windows size and mouse events are disabled, can still have
     * events other than keyboard, like focus events. Look for at least one
     * keydown event because a trailing LF keyup is always present from the
     * last input. However, if our buffer is full, assume there is a key
     * down somewhere in the unread buffer. I suppose we could expand the
     * buffer but not worth...
     */
    if (count == (sizeof(input)/sizeof(input[0])))
	return 1;
    for (i = 0; i < count; ++i) {
	if (input[i].EventType == KEY_EVENT
	    && input[i].Event.KeyEvent.bKeyDown) {
	    return 1;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleReaderThread --
 *
 *	This function runs in a separate thread and waits for input to become
 *	available on a console.
 *
 * Results:
 *	Always 0.
 *
 * Side effects:
 *	Signals the main thread when input become available.
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
ConsoleReaderThread(
    LPVOID arg)
{
    ConsoleHandleInfo *handleInfoPtr = (ConsoleHandleInfo *) arg;
    ConsoleHandleInfo **iterator;
    char inputChars[200]; /* Temporary buffer */
    RingSizeT inputLen = 0;
    RingSizeT inputOffset = 0;

    /*
     * Keep looping until one of the following happens.
     * - there are no more channels listening on the console
     * - the console handle has been closed
     */

    /* This thread is holding a reference so pointer is safe */
    AcquireSRWLockExclusive(&handleInfoPtr->lock);

    while (1) {

	if (handleInfoPtr->numRefs == 1) {
	    /*
	     * Sole reference. That's this thread. Exit since no clients
	     * and no way for a thread to attach to a console after process
	     * start.
	     */
	    break;
	}

	/*
	 * Shared buffer has no data. If we have some in our private buffer
	 * copy that. Else check if there has been an error. In both cases
	 * notify the interp threads.
	 */
	if (inputLen > 0 || handleInfoPtr->lastError != 0) {
	    HANDLE consoleHandle;
	    if (inputLen > 0) {
		/* Private buffer has data. Copy it over. */
		RingSizeT nStored;

		assert((inputLen - inputOffset) > 0);

		nStored = RingBufferIn(&handleInfoPtr->buffer,
				       inputOffset + inputChars,
				       inputLen - inputOffset,
				       1);
		inputOffset += nStored;
		if (inputOffset == inputLen) {
		    /* Temp buffer now empty */
		    inputOffset = 0;
		    inputLen = 0;
		}
	    } else {
		/*
		 * On error, nothing but inform caller and wait
		 * We do not want to exit until there are no client interps.
		 */
	    }

	    /*
	     * Wake up any threads waiting either synchronously or
	     * asynchronously. Since we are providing data, turn off the
	     * AWAITED flag. If the data provided is not sufficient the
	     * clients will request again. Note we have to wake up ALL
	     * awaiting threads, not just one, so they can all reissue
	     * requests if needed. (In a properly designed app, at most one
	     * thread should be reading standard input but...)
	     */
	    handleInfoPtr->flags &= ~CONSOLE_DATA_AWAITED;
	    /* Wake synchronous channels */
	    WakeAllConditionVariable(&handleInfoPtr->interpThreadCV);
	    /*
	     * Wake up async channels registered for file events. Note in
	     * order to follow the locking hierarchy, we need to release
	     * handleInfoPtr->lock before calling NudgeWatchers.
	     */
	    consoleHandle = handleInfoPtr->console;
	    ReleaseSRWLockExclusive(&handleInfoPtr->lock);
	    NudgeWatchers(consoleHandle);
	    AcquireSRWLockExclusive(&handleInfoPtr->lock);

	    /*
	     * Loop back to recheck for exit conditions changes while the
	     * the lock was not held.
	     */
	    continue;
	}

	/*
	 * Both shared buffer and private buffer are empty. Need to go get
	 * data from console but do not want to read ahead because the
	 * interp thread might change the read mode, e.g. turning off echo
	 * for password input. So only do so if at least one interpreter has
	 * requested data.
	 */
	if ((handleInfoPtr->flags & CONSOLE_DATA_AWAITED)
	    && ConsoleDataAvailable(handleInfoPtr->console)) {
	    DWORD error;
	    /* Do not hold the lock while blocked in console */
	    ReleaseSRWLockExclusive(&handleInfoPtr->lock);
	    /*
	     * Note - the temporary buffer serves two purposes. It
	     */
	    error = ReadConsoleChars(handleInfoPtr->console,
				     (WCHAR *)inputChars,
				     sizeof(inputChars) / sizeof(WCHAR),
				     &inputLen);
	    AcquireSRWLockExclusive(&handleInfoPtr->lock);
	    if (error == 0) {
		inputLen *= sizeof(WCHAR);
	    } else {
		/*
		 * We only store the last error. It is up to channel
		 * handlers whether to close or not in case of errors.
		 */
		handleInfoPtr->lastError = error;
		if (handleInfoPtr->lastError == ERROR_INVALID_HANDLE) {
		    handleInfoPtr->console = INVALID_HANDLE_VALUE;
		}
	    }
	} else {
	    /*
	     * Either no one was asking for data, or no data was available.
	     * In the former case, wait until someone wakes us asking for
	     * data. In the latter case, there is no alternative but to
	     * poll since ReadConsole does not support async operation.
	     * So sleep for a short while and loop back to retry.
	     */
	    DWORD sleepTime;
	    sleepTime =
		handleInfoPtr->flags & CONSOLE_DATA_AWAITED ? 50 : INFINITE;
	    SleepConditionVariableSRW(&handleInfoPtr->consoleThreadCV,
				      &handleInfoPtr->lock,
				      sleepTime,
				      0);
	}

	/* Loop again to check for exit or wait for readers to wake us */
    }

    /*
     * Exiting:
     * - remove the console from global list
     * - close the handle if still valid
     * - release the structure
     * Note there is not need to check for any watchers because we only
     * exit when there are no channels open to this console.
     */
    ReleaseSRWLockExclusive(&handleInfoPtr->lock);
    AcquireSRWLockExclusive(&gConsoleLock); /* Modifying - exclusive lock */
    for (iterator = &gConsoleHandleInfoList; *iterator;
	 iterator = &(*iterator)->nextPtr) {
	if (*iterator == handleInfoPtr) {
	    *iterator = handleInfoPtr->nextPtr;
	    break;
	}
    }
    ReleaseSRWLockExclusive(&gConsoleLock);

    /* No need for relocking - no other thread should have access to it now */
    RingBufferClear(&handleInfoPtr->buffer);

    if (handleInfoPtr->console != INVALID_HANDLE_VALUE
	&& handleInfoPtr->lastError != ERROR_INVALID_HANDLE) {
	SetConsoleMode(handleInfoPtr->console, handleInfoPtr->initMode);
	/*
	 * NOTE: we do not call CloseHandle(handleInfoPtr->console) here.
	 * As per the GetStdHandle documentation, it need not be closed.
	 * Other components may be directly using it. Note however that
	 * an explicit chan close script command does close the handle
	 * for all threads.
	 */
    }

    Tcl_Free(handleInfoPtr);

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleWriterThread --
 *
 *	This function runs in a separate thread and writes data onto a
 *	console.
 *
 * Results:
 *	Always returns 0.
 *
 * Side effects:
 *	Signals the main thread when an output operation is completed.
 *
 *----------------------------------------------------------------------
 */
static DWORD WINAPI
ConsoleWriterThread(LPVOID arg)
{
    ConsoleHandleInfo *handleInfoPtr = (ConsoleHandleInfo *) arg;
    ConsoleHandleInfo **iterator;
    BOOL success;
    RingSizeT numBytes;
    /*
     * This buffer size has no relation really with the size of the shared
     * buffer. Could be bigger or smaller. Make larger as multiple threads
     * could potentially be writing to it.
     */
    char buffer[2*CONSOLE_BUFFER_SIZE];

    /*
     * Keep looping until one of the following happens.
     *
     * - there are not more channels listening on the console
     * - the console handle has been closed
     *
     * On each iteration,
     * - if the channel buffer is empty, wait for some channel writer to write
     * - if there is data in our buffer, write it to the console
     */

    /* This thread is holding a reference so pointer is safe */
    AcquireSRWLockExclusive(&handleInfoPtr->lock);
    while (1) {
	/* handleInfoPtr->lock must be held on entry to loop */

	int offset;
	HANDLE consoleHandle;

	/*
	 * Sadly, we need to do another copy because do not want to hold
	 * a lock on handleInfoPtr->buffer while calling WriteConsole as that
	 * might block. Also, we only want to copy an integral number of
	 * WCHAR's, i.e. even number of chars so do some length checks up
	 * front.
	 */
	numBytes = RingBufferLength(&handleInfoPtr->buffer);
	numBytes &= ~1; /* Copy integral number of WCHARs -> even number of bytes */
	if (numBytes == 0) {
	    /* No data to write */
	    if (handleInfoPtr->numRefs == 1) {
		/*
		 * Sole reference. That's this thread. Exit since no clients
		 * and no buffered output.
		 */
		break;
	    }
	    /* Wake up any threads waiting synchronously. */
	    WakeConditionVariable(&handleInfoPtr->interpThreadCV);
	    success = SleepConditionVariableSRW(&handleInfoPtr->consoleThreadCV,
						&handleInfoPtr->lock,
						INFINITE,
						0);
	    /* Note: lock has been acquired again! */
	    if (!success && GetLastError() != ERROR_TIMEOUT) {
		/* TODO - what can be done? Should not happen */
		/* For now keep going */
	    }
	    continue;
	}

	/* We have data to write */
	if ((size_t)numBytes > (sizeof(buffer) / sizeof(buffer[0]))) {
	    numBytes = sizeof(buffer);
	}
	/* No need to check result, we already checked length bytes available */
	RingBufferOut(&handleInfoPtr->buffer, buffer, numBytes, 0);

	consoleHandle = handleInfoPtr->console;
	WakeConditionVariable(&handleInfoPtr->interpThreadCV);
	ReleaseSRWLockExclusive(&handleInfoPtr->lock);
	offset = 0;
	while (numBytes > 0) {
	    RingSizeT numWChars = numBytes / sizeof(WCHAR);
	    DWORD status;
	    status = WriteConsoleChars(handleInfoPtr->console,
				       (WCHAR *)(offset + buffer),
				       numWChars,
				       &numWChars);
	    if (status != 0) {
		/* Only overwrite if no previous error */
		if (handleInfoPtr->lastError == 0) {
		    handleInfoPtr->lastError = status;
		}
		if (status == ERROR_INVALID_HANDLE) {
		    handleInfoPtr->console = INVALID_HANDLE_VALUE;
		}
		/* Assume this write is done but keep looping in case
		 * it is a transient error. Not sure just closing handle
		 * and exiting thread is a good idea until all references
		 * from interp threads are gone.
		 */
		break;
	    }
	    numBytes -= numWChars * sizeof(WCHAR);
	    offset += numWChars * sizeof(WCHAR);
	}

	/* Wake up any threads waiting synchronously. */
	WakeConditionVariable(&handleInfoPtr->interpThreadCV);
	/*
	 * Wake up all channels registered for file events. Note in
	 * order to follow the locking hierarchy, we cannot hold any locks
	 * when calling NudgeWatchers.
	 */
	NudgeWatchers(consoleHandle);

	AcquireSRWLockExclusive(&handleInfoPtr->lock);
    }

    /*
     * Exiting:
     * - remove the console from global list
     * - release the structure
     * NOTE: we do not call CloseHandle(handleInfoPtr->console) here.
     * As per the GetStdHandle documentation, it need not be closed.
     * Other components may be directly using it. Note however that
     * an explicit chan close script command does close the handle
     * for all threads.
     */
    ReleaseSRWLockExclusive(&handleInfoPtr->lock);
    AcquireSRWLockExclusive(&gConsoleLock); /* Modifying - exclusive lock */
    for (iterator = &gConsoleHandleInfoList; *iterator;
	 iterator = &(*iterator)->nextPtr) {
	if (*iterator == handleInfoPtr) {
	    *iterator = handleInfoPtr->nextPtr;
	    break;
	}
    }
    ReleaseSRWLockExclusive(&gConsoleLock);

    RingBufferClear(&handleInfoPtr->buffer);

    Tcl_Free(handleInfoPtr);

    return 0;
}

/*
 *------------------------------------------------------------------------
 *
 * AllocateConsoleHandleInfo --
 *
 *    Allocates a ConsoleHandleInfo for the passed console handle. As
 *    a side effect starts a console thread to handle i/o on the handle.
 *
 *    Important: Caller must be holding an EXCLUSIVE lock on gConsoleLock
 *    when calling this function. The lock continues to be held on return.
 *
 * Results:
 *    Pointer to an unlocked ConsoleHandleInfo structure. The reference
 *    count on the structure is 1. This corresponds to the common reference
 *    from the console thread and the gConsoleHandleInfoList. Returns NULL
 *    on error.
 *
 * Side effects:
 *    A console reader or writer thread is started. The returned structure
 *    is placed on the active console handler list gConsoleHandleInfoList.
 *
 *------------------------------------------------------------------------
 */
static ConsoleHandleInfo *
AllocateConsoleHandleInfo(
    HANDLE consoleHandle,
    int permissions)   /* TCL_READABLE or TCL_WRITABLE */
{
    ConsoleHandleInfo *handleInfoPtr;
    DWORD consoleMode;


    handleInfoPtr = (ConsoleHandleInfo *)Tcl_Alloc(sizeof(*handleInfoPtr));
    memset(handleInfoPtr, 0, sizeof(*handleInfoPtr));
    memset(handleInfoPtr, 0, sizeof(*handleInfoPtr));
    handleInfoPtr->console = consoleHandle;
    InitializeSRWLock(&handleInfoPtr->lock);
    InitializeConditionVariable(&handleInfoPtr->consoleThreadCV);
    InitializeConditionVariable(&handleInfoPtr->interpThreadCV);
    RingBufferInit(&handleInfoPtr->buffer, CONSOLE_BUFFER_SIZE);
    handleInfoPtr->lastError = 0;
    handleInfoPtr->permissions = permissions;
    handleInfoPtr->numRefs = 1; /* See function header */
    if (permissions == TCL_READABLE) {
	GetConsoleMode(consoleHandle, &handleInfoPtr->initMode);
	consoleMode = handleInfoPtr->initMode;
	consoleMode &= ~(ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);
	consoleMode |= ENABLE_LINE_INPUT;
	SetConsoleMode(consoleHandle, consoleMode);
    }
    handleInfoPtr->consoleThread = CreateThread(
	NULL, /* default security descriptor */
	2*CONSOLE_BUFFER_SIZE, /* Stack size - gets rounded up to granularity */
	permissions == TCL_READABLE ? ConsoleReaderThread : ConsoleWriterThread,
	handleInfoPtr, /* Pass to thread */
	0,             /* Flags - no special cases */
	NULL);         /* Don't care about thread id */
    if (handleInfoPtr->consoleThread == NULL) {
	/* Note - SRWLock and condition variables do not need finalization */
	RingBufferClear(&handleInfoPtr->buffer);
	Tcl_Free(handleInfoPtr);
	return NULL;
    }

    /* Chain onto global list */
    handleInfoPtr->nextPtr = gConsoleHandleInfoList;
    gConsoleHandleInfoList = handleInfoPtr;

    return handleInfoPtr;
}

/*
 *------------------------------------------------------------------------
 *
 * FindConsoleInfo --
 *
 *    Finds the ConsoleHandleInfo record for a given ConsoleChannelInfo.
 *    The found record must match the console handle. It is the caller's
 *    responsibility to check the permissions (read/write) in the returned
 *    ConsoleHandleInfo match permissions in chanInfoPtr. This function does
 *    not check that.
 *
 *    Important: Caller must be holding an shared or exclusive lock on
 *    gConsoleMutex. That ensures the returned pointer stays valid on
 *    return without risk of deallocation by other threads.
 *
 * Results:
 *    Pointer to the found ConsoleHandleInfo or NULL if not found
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------
 */
static ConsoleHandleInfo *
FindConsoleInfo(const ConsoleChannelInfo *chanInfoPtr)
{
    ConsoleHandleInfo *handleInfoPtr;
    for (handleInfoPtr = gConsoleHandleInfoList; handleInfoPtr; handleInfoPtr = handleInfoPtr->nextPtr) {
	if (handleInfoPtr->console == chanInfoPtr->handle) {
	    return handleInfoPtr;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinOpenConsoleChannel --
 *
 *	Constructs a Console channel for the specified standard OS handle.
 *	This is a helper function to break up the construction of channels
 *	into File, Console, or Serial.
 *
 * Results:
 *	Returns the new channel, or NULL.
 *
 * Side effects:
 *	May open the channel.
 *
 *----------------------------------------------------------------------
 */
Tcl_Channel
TclWinOpenConsoleChannel(
    HANDLE handle,
    char *channelName,
    int permissions)
{
    ConsoleChannelInfo *chanInfoPtr;
    ConsoleHandleInfo *handleInfoPtr;

    /* A console handle can either be input or output, not both */
    if (permissions != TCL_READABLE && permissions != TCL_WRITABLE) {
	return NULL;
    }

    ConsoleInit();

    chanInfoPtr = (ConsoleChannelInfo *)Tcl_Alloc(sizeof(*chanInfoPtr));
    memset(chanInfoPtr, 0, sizeof(*chanInfoPtr));

    chanInfoPtr->permissions = permissions;
    chanInfoPtr->handle = handle;
    chanInfoPtr->channel = (Tcl_Channel) NULL;

    chanInfoPtr->threadId = Tcl_GetCurrentThread();

    /*
     * Use the pointer for the name of the result channel. This keeps the
     * channel names unique, since some may share handles (stdin/stdout/stderr
     * for instance).
     */

    sprintf(channelName, "file%" TCL_Z_MODIFIER "x", (size_t) chanInfoPtr);

    if (permissions & TCL_READABLE) {
	/*
	 * Make sure the console input buffer is ready for only character
	 * input notifications and the buffer is set for line buffering. IOW,
	 * we only want to catch when complete lines are ready for reading.
	 */

	chanInfoPtr->flags |= CONSOLE_READ_OPS;
	GetConsoleMode(handle, &chanInfoPtr->initMode);

#ifdef OBSOLETE
	/* Why was priority being set on console input? Code smell */
	SetThreadPriority(infoPtr->reader.thread, THREAD_PRIORITY_HIGHEST);
#endif
    } else {
	/* Already checked permissions is WRITABLE if not READABLE */
	/* TODO - enable ansi escape processing? */
    }

    /*
     * Global lock but that's ok. See comments top of file. Allocations
     * will happen only a few times in the life of a process and that too
     * generally at start up where only one thread is active.
     */
    AcquireSRWLockExclusive(&gConsoleLock); /*Allocate needs exclusive lock */

    handleInfoPtr = FindConsoleInfo(chanInfoPtr);
    if (handleInfoPtr == NULL) {
	/* Not found. Allocate one */
	handleInfoPtr = AllocateConsoleHandleInfo(handle, permissions);
    } else {
	/* Found. Its direction (read/write) better be the same */
	if (handleInfoPtr->permissions != permissions) {
	    handleInfoPtr = NULL;
	}
    }

    if (handleInfoPtr == NULL) {
	ReleaseSRWLockExclusive(&gConsoleLock);
	if (permissions == TCL_READABLE) {
	    SetConsoleMode(handle, chanInfoPtr->initMode);
	}
	Tcl_Free(chanInfoPtr);
	return NULL;
    }

    /*
     * There is effectively a reference to this structure from the Tcl
     * channel subsystem. So record that. This reference will be dropped
     * when the Tcl channel is closed.
     */
    chanInfoPtr->numRefs = 1;

    /*
     * Need to keep track of number of referencing channels for closing.
     * The pointer is safe since there is a reference held to it from
     * gConsoleHandleInfoList but still need to lock the structure itself
     */
    AcquireSRWLockExclusive(&handleInfoPtr->lock);
    handleInfoPtr->numRefs += 1;
    ReleaseSRWLockExclusive(&handleInfoPtr->lock);

    ReleaseSRWLockExclusive(&gConsoleLock);

    /* Note Tcl_CreateChannel never fails other than panic on error */
    chanInfoPtr->channel = Tcl_CreateChannel(&consoleChannelType, channelName,
	    chanInfoPtr, permissions);

    /*
     * Consoles have default translation of auto and ^Z eof char, which means
     * that a ^Z will be accepted as EOF when reading.
     */

    Tcl_SetChannelOption(NULL, chanInfoPtr->channel, "-translation", "auto");
    Tcl_SetChannelOption(NULL, chanInfoPtr->channel, "-encoding", "utf-16");
    return chanInfoPtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleThreadActionProc --
 *
 *	Insert or remove any thread local refs to this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes thread local list of valid channels.
 *
 *----------------------------------------------------------------------
 */

static void
ConsoleThreadActionProc(
    void *instanceData,
    int action)
{
    ConsoleChannelInfo *chanInfoPtr = (ConsoleChannelInfo *)instanceData;

    /* No need for any locks as no other thread will be writing to it */
    if (action == TCL_CHANNEL_THREAD_INSERT) {
	ConsoleInit(); /* Needed to set up event source handlers for this thread */
	chanInfoPtr->threadId = Tcl_GetCurrentThread();
    } else {
	chanInfoPtr->threadId = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleSetOptionProc --
 *
 *	Sets an option on a channel.
 *
 * Results:
 *	A standard Tcl result. Also sets the interp's result on error if
 *	interp is not NULL.
 *
 * Side effects:
 *	May modify an option on a console. Sets Error message if needed (by
 *	calling Tcl_BadChannelOption).
 *
 *----------------------------------------------------------------------
 */
static int
ConsoleSetOptionProc(
    void *instanceData,	/* File state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL. */
    const char *optionName,	/* Which option to set? */
    const char *value)		/* New value for option. */
{
    ConsoleChannelInfo *chanInfoPtr = (ConsoleChannelInfo *)instanceData;
    int len = strlen(optionName);
    int vlen = strlen(value);

    /*
     * Option -inputmode normal|password|raw
     */

    if ((chanInfoPtr->flags & CONSOLE_READ_OPS) && (len > 1) &&
	    (strncmp(optionName, "-inputmode", len) == 0)) {
	DWORD mode;

	if (GetConsoleMode(chanInfoPtr->handle, &mode) == 0) {
	    Tcl_WinConvertError(GetLastError());
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"couldn't read console mode: %s",
			Tcl_PosixError(interp)));
	    }
	    return TCL_ERROR;
	}
	if (Tcl_UtfNcasecmp(value, "NORMAL", vlen) == 0) {
	    mode |=
		ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
	} else if (Tcl_UtfNcasecmp(value, "PASSWORD", vlen) == 0) {
	    mode |= ENABLE_LINE_INPUT|ENABLE_PROCESSED_INPUT;
	    mode &= ~ENABLE_ECHO_INPUT;
	} else if (Tcl_UtfNcasecmp(value, "RAW", vlen) == 0) {
	    mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
	} else if (Tcl_UtfNcasecmp(value, "RESET", vlen) == 0) {
	    /*
	     * Reset to the initial mode, whatever that is.
	     */
	    mode = chanInfoPtr->initMode;
	} else {
	    if (interp) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"bad mode \"%s\" for -inputmode: must be"
			" normal, password, raw, or reset", value));
		Tcl_SetErrorCode(interp, "TCL", "OPERATION", "FCONFIGURE",
			"VALUE", NULL);
	    }
	    return TCL_ERROR;
	}
	if (SetConsoleMode(chanInfoPtr->handle, mode) == 0) {
	    Tcl_WinConvertError(GetLastError());
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"couldn't set console mode: %s",
			Tcl_PosixError(interp)));
	    }
	    return TCL_ERROR;
	}

	return TCL_OK;
    }

    if (chanInfoPtr->flags & CONSOLE_READ_OPS) {
	return Tcl_BadChannelOption(interp, optionName, "inputmode");
    } else {
	return Tcl_BadChannelOption(interp, optionName, "");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConsoleGetOptionProc --
 *
 *	Gets a mode associated with an IO channel. If the optionName arg is
 *	non-NULL, retrieves the value of that option. If the optionName arg is
 *	NULL, retrieves a list of alternating option names and values for the
 *	given channel.
 *
 * Results:
 *	A standard Tcl result. Also sets the supplied DString to the string
 *	value of the option(s) returned.  Sets error message if needed
 *	(by calling Tcl_BadChannelOption).
 *
 *----------------------------------------------------------------------
 */

static int
ConsoleGetOptionProc(
    void *instanceData,	/* File state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL. */
    const char *optionName,	/* Option to get. */
    Tcl_DString *dsPtr)		/* Where to store value(s). */
{
    ConsoleChannelInfo *chanInfoPtr = (ConsoleChannelInfo *)instanceData;
    int valid = 0;		/* Flag if valid option parsed. */
    unsigned int len;
    char buf[TCL_INTEGER_SPACE];

    if (optionName == NULL) {
	len = 0;
    } else {
	len = strlen(optionName);
    }

    /*
     * Get option -inputmode
     *
     * This is a great simplification of the underlying reality, but actually
     * represents what almost all scripts really want to know.
     */

    if (chanInfoPtr->flags & CONSOLE_READ_OPS) {
	if (len == 0) {
	    Tcl_DStringAppendElement(dsPtr, "-inputmode");
	}
	if (len==0 || (len>1 && strncmp(optionName, "-inputmode", len)==0)) {
	    DWORD mode;

	    valid = 1;
	    if (GetConsoleMode(chanInfoPtr->handle, &mode) == 0) {
		Tcl_WinConvertError(GetLastError());
		if (interp != NULL) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			    "couldn't read console mode: %s",
			    Tcl_PosixError(interp)));
		}
		return TCL_ERROR;
	    }
	    if (mode & ENABLE_LINE_INPUT) {
		if (mode & ENABLE_ECHO_INPUT) {
		    Tcl_DStringAppendElement(dsPtr, "normal");
		} else {
		    Tcl_DStringAppendElement(dsPtr, "password");
		}
	    } else {
		Tcl_DStringAppendElement(dsPtr, "raw");
	    }
	}
    } else {
	/*
	 * Output channel. Get option -winsize
	 * Option is readonly and returned by [fconfigure chan -winsize] but not
	 * returned by [fconfigure chan] without explicit option name.
	 */
	if (len == 0) {
	    Tcl_DStringAppendElement(dsPtr, "-winsize");
	}

	if (len == 0 || (len > 1 && strncmp(optionName, "-winsize", len) == 0)) {
	    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;

	    valid = 1;
	    if (!GetConsoleScreenBufferInfo(chanInfoPtr->handle,
					    &consoleInfo)) {
		Tcl_WinConvertError(GetLastError());
		if (interp != NULL) {
		    Tcl_SetObjResult(
			interp,
			Tcl_ObjPrintf("couldn't read console size: %s",
				      Tcl_PosixError(interp)));
		}
		return TCL_ERROR;
	    }
	    Tcl_DStringStartSublist(dsPtr);
	    sprintf(buf,
		    "%d",
		    consoleInfo.srWindow.Right - consoleInfo.srWindow.Left + 1);
	    Tcl_DStringAppendElement(dsPtr, buf);
	    sprintf(buf,
		    "%d",
		    consoleInfo.srWindow.Bottom - consoleInfo.srWindow.Top + 1);
	    Tcl_DStringAppendElement(dsPtr, buf);
	    Tcl_DStringEndSublist(dsPtr);
	}
    }


    if (valid) {
	return TCL_OK;
    }
    if (chanInfoPtr->flags & CONSOLE_READ_OPS) {
	return Tcl_BadChannelOption(interp, optionName, "inputmode");
    } else {
	return Tcl_BadChannelOption(interp, optionName, "winsize");
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
