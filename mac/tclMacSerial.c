/*
 * tclMacSerial.c
 *
 *  This file adds support for the Serial port to the Mac file channel
 *  driver.  It was originally contributed by Sean Woods, yoda@drexel.edu
 *      It should be considered experimental at this stage.
 *
 *  The serial port is accessed from the standard file channel handler
 *  using the names "MODEM:" "PRINTER:" "COM1:" and "COM2:".
 *
 *  Baud rate, parity, data bits, and stop bits are configurable from the
 *  fconfigure command.
 *  Supports blocking and non-blocking modes
 *  Does Not Support flow control (XON/XOFF, DTR, CTS and their ilk)
 *
 * Copyright (c) 1999 Woods Design Services, and Jim Ingham.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclMacSerial.c,v 1.1.2.1 1999/03/22 17:39:08 rjohnson Exp $
 */


#include "tclInt.h"
#include "tclPort.h"
#include "tclMacInt.h"
#include <Aliases.h>
#include <Errors.h>
#include <Gestalt.h>
#include <Processes.h>
#include <Strings.h>
#include <string.h>
#include <devices.h>
#include <LowMem.h>
#include <CommResources.h>
#include <CRMSerialDevices.h>
#include <Serial.h>


/*
 * Tell if we have a serial port arbitrator for working around
 * problems with ARA
 */


enum {
    gestaltSerialPortArbitratorAttr = 'arb ',
    gestaltSerialPortArbitratorExists = 0
};


/*
 * The following variable is used to tell whether this module has been
 * initialized.
 */


static int initialized = 0;


/*
 * This structure describes per-instance state of a
 * macintosh serial based channel.
 */


typedef struct SerialState {
    short inputRef;     	/* Macintosh serial reference numbers. */
    short outputRef;       	/* Added because serial ports have both
				 * an input and output reference. */
    unsigned int serConfig; 	/* Port Configuration. */
    int     blocking;  		/* Enable/Disable Serial Blocking */
    Tcl_Channel serialChan; 	/* Pointer to the channel for this serial. */
    int watchMask;   		/* OR'ed set of flags indicating which events
				 * are being watched. */
    int pending;       		/* 1 if message is pending on queue. */
    struct SerialState *nextPtr;    /* Pointer to next registered serial. */
} SerialState;


/*
 * The following pointer refers to the head of the list of serials
 * managed that are being watched for serial events.
 */


static SerialState *firstSerialPtr;


/*
 * The following structure is what is added to the Tcl event queue when
 * serial events are generated.
 */


typedef struct SerialEvent {
    Tcl_Event header;     /* Information that is standard for
			   * all events. */
    SerialState *infoPtr;   /* Pointer to serial info structure.  Note
			     * that we still have to verify that the
			     * serial connection exists before
			     * dereferencing this pointer. */
} SerialEvent;


/*
 * This is used in tclMacChan.c to detect the special cookies for
 * the Macintosh serial ports.
 */


Tcl_Channel TclMacOpenSerialChannel _ANSI_ARGS_((char *fileName, int
	*errorCode));


/*
 * Static routines for this file
 */


static void     MacSerialWatch _ANSI_ARGS_((ClientData instanceData,
	int mask));
static int   MacSerialGetHandle _ANSI_ARGS_((ClientData
	instanceData,
	int direction, ClientData *handlePtr));
static int   MacSerialBlockMode _ANSI_ARGS_((ClientData
	instanceData,
	int mode));
static int   MacSerialInput _ANSI_ARGS_((ClientData instanceData,
	char *buf, int toRead, int *errorCode));
static int   MacSerialOutput _ANSI_ARGS_((ClientData instanceData,
	char *buf, int toWrite, int *errorCode));
static int   MacSerialClose _ANSI_ARGS_((ClientData instanceData,
	Tcl_Interp *interp));
static int
MacSerialGetOptionProc  _ANSI_ARGS_((ClientData
	instanceData,
	Tcl_Interp *interp,
	char *optionName, Tcl_DString *dsPtr));
static int   MacSerialSetOptionProc  _ANSI_ARGS_((ClientData
	instanceData,
	Tcl_Interp *interp,
	char *optionName, char *newVal));
/*
 * This set is from the Serial Driver Apocrypha by Apple DTS
 */


static OSErr      CloseSerialDrivers(SInt16 inRefNum, SInt16 outRefNum);
static Boolean      DriverIsOpen(ConstStr255Param driverName);
static OSErr      OpenOneSerialDriver(ConstStr255Param driverName,
	short *refNum);
static Tcl_Channel  OpenSerialChannel _ANSI_ARGS_((int port, int
	*errorCodePtr));
static OSErr      OpenSerialDrivers _ANSI_ARGS_((ConstStr255Param
	inName,
	ConstStr255Param outName,
	short *inRefNum, short *outRefNum));
static Boolean      SerialArbitrationExists(void);


/*
 * These are the Serial Channel functions
 */


static void     SerialChannelExitHandler _ANSI_ARGS_((
    ClientData clientData));
static void     SerialCheckProc _ANSI_ARGS_((ClientData clientData,
	int flags));
static int   SerialClose _ANSI_ARGS_((ClientData instanceData,
	Tcl_Interp *interp));
static int   SerialEventProc _ANSI_ARGS_((Tcl_Event *evPtr,
	int flags));
static void     SerialInit _ANSI_ARGS_((void));
static void     SerialSetupProc _ANSI_ARGS_((ClientData clientData,
	int flags));


/*
 * This variable describes the channel type structure for serial based IO.
 */


static Tcl_ChannelType serialChannelType = {
    "port",        		/* Type name. */
    MacSerialBlockMode,     	/* Set blocking or
                                 * non-blocking mode.*/
    MacSerialClose,     	/* Close proc. */
    MacSerialInput,     	/* Input proc. */
    MacSerialOutput,       	/* Output proc. */
    NULL,      			/* Seek proc. (none) */
    MacSerialSetOptionProc, 	/* Set option proc. */
    MacSerialGetOptionProc, 	/* Get option proc. */
    MacSerialWatch,     	/* Initialize notifier. */
    MacSerialGetHandle   	/* Get OS handles out of channel. */
};

/*
 *----------------------------------------------------------------------
 *
 * TclMacOpenSerialChannel --
 *
 *  This function opens a Mac serial channel event source.
 *      It uses the cookies MODEM:, PRINTER:, COM1: and COM2:
 *  to indicate which channel is to be opened.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates a new event source.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclMacOpenSerialChannel (
    char *fileName,    /* String which might contain a cookie for the
			  ports */
    int *errorCode)    /* The errorCode returned */
{
    Tcl_Channel chan = NULL;
    int port = 0;


    if ((*fileName == 'M') && (strcmp(fileName, "MODEM:") == 0)) {
	port = 1;
    } else if ((*fileName == 'P') && (strcmp(fileName,"PRINTER:") == 0)) {
	port = 2;
    } else if ((*fileName == 'C') && (strcmp(fileName,"COM1:") == 0)) {
	port = 3;
    } else if ((*fileName == 'C') && (strcmp(fileName,"COM2:") == 0)) {
	port = 4;
    }


    if (port > 0) {
	chan = OpenSerialChannel(port, errorCode);
    }


    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * SerialInit --
 *
 *  This function initializes the serial channel event source.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates a new event source.
 *
 *----------------------------------------------------------------------
 */

static void
SerialInit()
{
    initialized = 1;
    firstSerialPtr = NULL;
    Tcl_CreateEventSource(SerialSetupProc, SerialCheckProc, NULL);
    Tcl_CreateExitHandler(SerialChannelExitHandler, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * SerialChannelExitHandler --
 *
 *  This function is called to cleanup the channel driver before
 *  Tcl is unloaded.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Destroys the communication window.
 *
 *----------------------------------------------------------------------
 */

static void
SerialChannelExitHandler(
    ClientData clientData)  /* Old window proc */
{
    Tcl_DeleteEventSource(SerialSetupProc, SerialCheckProc, NULL);
    initialized = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * SerialSetupProc --
 *
 *  This procedure is invoked before Tcl_DoOneEvent blocks waiting
 *  for an event.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Adjusts the block time if needed.
 *
 *----------------------------------------------------------------------
 */

void
SerialSetupProc(
    ClientData data,       /* Not used. */
    int flags)       /* Event flags as passed to Tcl_DoOneEvent. */
{
    SerialState *infoPtr;
    Tcl_Time blockTime = { 0, 0 };


    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }


    /*
     * Check to see if there is a ready file.  If so, poll.
     */


    for (infoPtr = firstSerialPtr; infoPtr != NULL; infoPtr =
	     infoPtr->nextPtr) {
	if (infoPtr->watchMask) {
	    Tcl_SetMaxBlockTime(&blockTime);
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SerialCheckProc --
 *
 *  This procedure is called by Tcl_DoOneEvent to check the file
 *  event source for events.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  May queue an event.
 *
 *----------------------------------------------------------------------
 */

static void
SerialCheckProc(
    ClientData data,       /* Not used. */
    int flags)       /* Event flags as passed to Tcl_DoOneEvent. */
{
    SerialEvent *evPtr;
    SerialState *infoPtr;
    int sentMsg = 0;
    Tcl_Time blockTime = { 0, 0 };


    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }


    /*
     * Queue events for any ready files that don't already have events
     * queued (caused by persistent states that won't generate WinSock
     * events).
     */


    for (infoPtr = firstSerialPtr; infoPtr != NULL; infoPtr =
	     infoPtr->nextPtr) {
	if (infoPtr->watchMask && !infoPtr->pending) {
	    infoPtr->pending = 1;
	    evPtr = (SerialEvent *) ckalloc(sizeof(SerialEvent));
	    evPtr->header.proc = SerialEventProc;
	    evPtr->infoPtr = infoPtr;
	    Tcl_QueueEvent((Tcl_Event *) evPtr, TCL_QUEUE_TAIL);
	}
    }
}

/*----------------------------------------------------------------------
 *
 * SerialEventProc --
 *
 *  This function is invoked by Tcl_ServiceEvent when a serial event
 *  reaches the front of the event queue.  This procedure invokes
 *  Tcl_NotifyChannel on the port.
 *
 * Results:
 *  Returns 1 if the event was handled, meaning it should be removed
 *  from the queue.  Returns 0 if the event was not handled, meaning
 *  it should stay on the queue.  The only time the event isn't
 *  handled is if the TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *  Whatever the notifier callback does.
 *
 *----------------------------------------------------------------------
 */

static int
SerialEventProc(
    Tcl_Event *evPtr,     /* Event to service. */
    int flags)       /* Flags that indicate what events to
		      * handle, such as TCL_FILE_EVENTS. */
{
    SerialEvent *fileEvPtr = (SerialEvent *)evPtr;
    SerialState *infoPtr;


    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }


    /*
     * Search through the list of watched ports for the one whose handle
     * matches the event.  We do this rather than simply dereferencing
     * the handle in the event so that ports can be closed while the
     * event is in the queue.
     */


    for (infoPtr = firstSerialPtr; infoPtr != NULL; infoPtr =
	     infoPtr->nextPtr) {
	if (fileEvPtr->infoPtr == infoPtr) {
	    infoPtr->pending = 0;
	    Tcl_NotifyChannel(infoPtr->serialChan, infoPtr->watchMask);
	    break;
	}
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * OpenSerialChannel--
 *
 *  Opens a Macintosh driver and creates a Tcl channel to control it.
 *
 * Results:
 *  A Tcl channel.
 *
 * Side effects:
 *  Will open a Macintosh device.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Channel
OpenSerialChannel(
    int port,      /* Name of serial to open. */
    int *errorCodePtr)       /* Where to store error code. */
{
    int channelPermissions;
    Tcl_Channel chan;
    OSErr err;
    short inputRef, outputRef;
    SerialState *serialState;
    char channelName[64];
    SerShk serHShake;   /* Handshake Configuration */


    /* Serial Channels Will always be Read/Write */


    channelPermissions = (TCL_READABLE | TCL_WRITABLE);


    err = 0;
    switch (port) {
	case 1:
	    /*
	     * PowerBooks with internal modems do not have an AOut port,
	     * so try the internal modem port instead
	     */


	    err = OpenSerialDrivers("\p.AIn", "\p.AOut", &inputRef, &outputRef);
	    if (err != noErr) {
		err = OpenSerialDrivers("\p.InternalModemIn",
			"\p.InternalModemOut",
			&inputRef, &outputRef);
		if (err != noErr) {
		    goto Error;
		}
	    }
	    break;
	case 2:
	    err = OpenSerialDrivers("\p.BIn", "\p.BOut", &inputRef, &outputRef);
	    if (err != noErr) {
		goto Error;
	    }
	    break;
	case 3:
	    err = OpenSerialDrivers("\p.CIn", "\p.COut", &inputRef, &outputRef);
	    if (err != noErr) {
		goto Error;
	    }
	    break;
	case 4:
	    err = OpenSerialDrivers("\p.DIn", "\p.DOut", &inputRef, &outputRef);
	    if (err != noErr) {
		goto Error;
	    }
	    break;
    }


    sprintf(channelName, "port%d", (int) port);
    serialState = (SerialState *) ckalloc((unsigned) sizeof(SerialState));
    chan = Tcl_CreateChannel(&serialChannelType, channelName,
            (ClientData) serialState, channelPermissions);


    if (chan == (Tcl_Channel) NULL) {
	*errorCodePtr = errno = EFAULT;
	Tcl_SetErrno(errno);
	CloseSerialDrivers(inputRef, outputRef);
	ckfree((char *) serialState);
        return NULL;
    }


    /*
     * Default 9600, 8, 1, No Parity
     */


    serialState->serConfig = baud9600 | data8 | noParity | stop10;


    /*
     * No Flow Control!
     */


    serHShake.fXOn = 0;
    serHShake.fCTS = 0;
    serHShake.fInX = 0;
    serHShake.fDTR = 0;


    /*
     * Disable Hardware Errors
     */


    serHShake.errs = 0;
    serHShake.evts = 0;


    serialState->serialChan = chan;
    serialState->inputRef = inputRef;
    serialState->outputRef = outputRef;
    serialState->pending = 0;
    serialState->watchMask = 0;
    serialState->blocking = 0;


    /*
     * Clear Input Buffer
     */


    err = SerSetBuf(inputRef, 0, 0);
    if (err != noErr) {
        goto Error;
    }


    /*
     * Port Handshake Settings
     */


    err = Control(inputRef, 14, &serHShake);
    if (err != noErr) {
        goto Error;
    }


    err = Control(outputRef, 14, &serHShake);
    if (err != noErr) {
        goto Error;
    }


    /*
     * Port Communication Settings/Reset
     */


    err = SerReset(inputRef, serialState->serConfig);
    if (err != noErr) {
        goto Error;
    }
    err = SerReset(outputRef, serialState->serConfig);
    if (err != noErr) {
        goto Error;
    }


    return chan;


    Error:
    switch(err) {
	case controlErr:
	case statusErr:
	case readErr:
	case writErr:
	    *errorCodePtr = errno = EIO;
	    break;
	default:
	    *errorCodePtr = errno = TclMacOSErrorToPosixError(err);
    }
    Tcl_SetErrno(errno);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * MacSerialWatch --
 *
 *  Initialize the notifier to watch handles from this channel.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static void
MacSerialWatch(
    ClientData instanceData,       /* The serial state. */
    int mask)          /* Events of interest; an OR-ed
			* combination of TCL_READABLE,
			* TCL_WRITABLE and TCL_EXCEPTION. */
{
    SerialState **nextPtrPtr, *ptr;
    SerialState *infoPtr = (SerialState *) instanceData;
    int oldMask = infoPtr->watchMask;


    if (!initialized) {
	SerialInit();
    }


    infoPtr->watchMask = mask;
    if (infoPtr->watchMask) {
	if (!oldMask) {
	    infoPtr->nextPtr = firstSerialPtr;
	    firstSerialPtr = infoPtr;
	}
    } else {
	if (oldMask) {
	    /*
	     * Remove the serial from the list of watched serials.
	     */


	    for (nextPtrPtr = &firstSerialPtr, ptr = *nextPtrPtr;
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
 * MacSerialBlockMode --
 *
 *  Set blocking or non-blocking mode on a serial channel.
 *  Since this is done on the software level, never really errors out
 *
 * Results:
 *  0 if successful, errno when failed.
 *
 * Side effects:
 *  Sets the device into blocking or non-blocking mode.
 *
 *----------------------------------------------------------------------
 */

static int
MacSerialBlockMode(
    ClientData instanceData,       /* Unused. */
    int mode)          /* The mode to set. */
{
    /*
     * Seems that yes means no, 1 - nonblocking 0 - blocking
     */


    SerialState *serialState = (SerialState *) instanceData;
    serialState->blocking = !mode;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * MacSerialClose --
 *
 *  Closes the IO channel.
 *
 * Results:
 *  0 if successful, the value of errno if failed.
 *
 * Side effects:
 *  Closes the physical channel
 *
 *----------------------------------------------------------------------
 */

static int
MacSerialClose(
    ClientData instanceData,    /* Unused. */
    Tcl_Interp *interp)     /* Unused. */
{
    SerialState *serialState = (SerialState *) instanceData;
    int errorCode = 0;
    OSErr err;


    err = CloseSerialDrivers(serialState->inputRef, serialState->outputRef);


    if (err != noErr) {
	errorCode = errno = TclMacOSErrorToPosixError(err);
	panic("error during port close");
    }


    ckfree((char *) serialState);
    Tcl_SetErrno(errorCode);
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * MacSerialInput --
 *
 *  Reads input from the IO channel into the buffer given. Returns
 *  count of how many bytes were actually read, and an error indication.
 *
 * Results:
 *  A count of how many bytes were read is returned and an error
 *  indication is returned in an output argument.
 *
 * Side effects:
 *  Reads input from the actual channel.
 *
 *----------------------------------------------------------------------
 */

int
MacSerialInput(
    ClientData instanceData,    /* Unused. */
    char *buffer,     /* Where to store data read. */
    int bufSize,       /* How much space is available
			* in the buffer? */
    int *errorCodePtr)   /* Where to store error code. */
{
    SerialState *serialState = (SerialState *) instanceData;
    long length;
    OSErr err;


    if (serialState->blocking) {
        do  {
	    length = bufSize;


	    /*
	     * Wait until information is available
	     */


	    err = SerGetBuf(serialState->inputRef,&length);
	    Tcl_DoOneEvent(0);
	} while(length == 0);
    }


    err = SerGetBuf(serialState->inputRef,&length);


    if(length > 0) {
        if(bufSize > length) {
            bufSize = length;
        }
        length = bufSize;


	err = FSRead(serialState->inputRef, &length, buffer);
	if ((err == noErr) || (err == eofErr)) {
	    return length;
	} else {
	    *errorCodePtr = errno = TclMacOSErrorToPosixError(err);
	    Tcl_SetErrno(errno);
	    return -1;
	}
    } else {
	return 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MacSerialOutput--
 *
 *  Writes the given output on the IO channel. Returns count of how
 *  many characters were actually written, and an error indication.
 *
 * Results:
 *  A count of how many characters were written is returned and an
 *  error indication is returned in an output argument.
 *
 * Side effects:
 *  Writes output on the actual channel.
 *
 *----------------------------------------------------------------------
 */

static int
MacSerialOutput(
    ClientData instanceData,       /* Unused. */
    char *buffer,      /* The data buffer. */
    int toWrite,         /* How many bytes to write? */
    int *errorCodePtr)       /* Where to store error code. */
{
    SerialState *serialState = (SerialState *) instanceData;
    long length = toWrite;
    SerStaRec inSerStat, outSerStat;
    OSErr err;


    *errorCodePtr = 0;
    errno = 0;
    err = SerStatus(serialState->outputRef, &inSerStat);
    err = SerStatus(serialState->inputRef, &outSerStat);
    err = FSWrite(serialState->outputRef, &length, buffer);
    if (err != noErr) {
	switch(err) {
	    case controlErr:
	    case statusErr:
	    case readErr:
	    case writErr:
		*errorCodePtr = errno = EIO;
		break;
	    default:
		*errorCodePtr = errno = TclMacOSErrorToPosixError(err);


	}
	Tcl_SetErrno(errno);
	return -1;
    }
    return length;
}

/*
 *----------------------------------------------------------------------
 *
 * MacSerialGetHandle --
 *
 *  Called from Tcl_GetChannelFile to retrieve OS handles from inside
 *  a serial based channel.
 *
 * Results:
 *  The appropriate handle or NULL if not present.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static int
MacSerialGetHandle(
    ClientData instanceData,       /* The serial state. */
    int direction,       /* Which handle to retrieve? */
    ClientData *handlePtr)
{
    if (direction == TCL_READABLE) {
	*handlePtr = (ClientData) ((SerialState*)instanceData)->inputRef;
	return TCL_OK;
    }
    if (direction == TCL_WRITABLE) {
	*handlePtr = (ClientData) ((SerialState*)instanceData)->outputRef;
	return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * MacSerialGetOptionProc --
 *
 *  Computes an option value for a serial based channel, or a
 *  list of all options and their values.
 *
 *  Note: This code is based on code contributed by John Haxby.
 *
 * Results:
 *  A standard Tcl result. The value of the specified option or a
 *  list of all options and  their values is returned in the
 *  supplied DString.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static int
MacSerialSetOptionProc(
    ClientData instanceData,      /* Socket state. */
    Tcl_Interp *interp,                 /* For error reporting - can be NULL.*/
    char *optionName,        /* Name of the option to
			      * retrieve the value for, or
			      * NULL to get all options and
			      * their values. */
    char *newVal)      /* Where to store the computed
			* value; initialized by caller. */
{
    SerialState *serialState = (SerialState *) instanceData;
    unsigned int temp;
    int dataconv;
    OSErr err;


    temp = serialState->serConfig;


    /*
     * Determine which options we need to modify.
     */


    if (!strcmp(optionName, "-baud")) {
	int baud;
	/*
	 * Mask Out Baud Bits
	 */
	temp = temp & 0xFE00;
	if (Tcl_GetInt(NULL, newVal, &dataconv) != TCL_OK) {
            if (interp) {
                Tcl_AppendResult(interp, "bad value for -baud: ",
                        "must be an integer",
                        (char *) NULL);
            }
            return TCL_ERROR;
	}


	switch (dataconv) {
	    case 150:
		baud = baud150;
		break;
	    case 300:
		baud = baud300;
		break;
	    case 600:
		baud = baud600;
		break;
	    case 1200:
		baud = baud1200;
		break;
	    case 1800:
		baud = baud1800;
		break;
	    case 2400:
		baud = baud2400;
		break;
	    case 3600:
		baud = baud3600;
		break;
	    case 4800:
		baud = baud4800;
		break;
	    case 7200:
		baud = baud7200;
		break;
	    case 9600:
		baud = baud9600;
		break;
	    case 14400:
		baud = baud14400;
		break;
	    case 19200:
		baud = baud19200;
		break;
	    case 28800:
		baud = baud28800;
		break;
	    case 38400:
		baud = baud38400;
		break;
	    case 57600:
		baud = baud57600;
		break;
	    default:
		if (interp) {
		    Tcl_AppendResult(interp, "bad value for -baud: ",
			    "valid settings are",
			    "150 300 600 1200 ",
			    "1800 2400 3600 4800 ",
			    "7200 9600 14400 19200 ",
			    "28800 38400 57600",
			    (char *) NULL);
		}
	}
	temp = temp | baud;
    } else if (!strcmp(optionName, "-databits")) {
	int bits;
	temp = temp & 0xF3FF;
	if (Tcl_GetInt(NULL, newVal, &dataconv) != TCL_OK) {
	    if (interp) {
		Tcl_AppendResult(interp, "bav value for -databits: ",
			"must be an integer.",
			(char *) NULL);
	    }
	    return TCL_ERROR;
	}


	switch (dataconv) {
	    case 5:
		bits = data5;
		break;
	    case 6:
		bits = data6;
		break;
	    case 7:
		bits = data7;
		break;
	    case 8:
		bits = data8;
		break;
	    default:
		if (interp) {
		    Tcl_AppendResult(interp, "bad value for -databits: ",
			    "valid settings are",
			    "5 6 7 8 ",
			    (char *) NULL);
		}
		return TCL_ERROR;
	}
	temp = temp | bits;
    } else if (!strcmp(optionName, "-parity")) {
	temp = temp & 0xCFFF;
	if (!strcmp(newVal, "none")) {
	    temp = temp | noParity;
	} else if (!strcmp(newVal, "odd")) {
	    temp = temp | oddParity;
	} else if (!strcmp(newVal, "even")) {
	    temp = temp | evenParity;
	} else {
	    if (interp) {
                Tcl_AppendResult(interp, "bad value for -parity: ",
                        "valid settings are",
                        "none odd even ",
                        (char *) NULL);
            }
	}
        return TCL_ERROR;
    } else if (!strcmp(optionName, "-stopbits")) {
	temp = temp & 0x3FFF;


	if (!strcmp(newVal, "1")) {
	    temp = temp | 0x4000;
	} else if (!strcmp(newVal, "1.5")) {
	    temp = temp | 0x8000;
	} else if (!strcmp(newVal, "2")) {
	    temp = temp | 0xC000;
	} else {
	    if (interp) {
                Tcl_AppendResult(interp, "bad value for -stopbits: ",
                        "valid settings are",
                        "1 1.5 2 ",
                        (char *) NULL);
            }
	}
        return TCL_ERROR;
    } else {
	return Tcl_BadChannelOption(interp, optionName,
		"baud databits parity stopbits");
    }


    serialState->serConfig = temp;


    /*
     * Port Communication Settings/Reset
     */


    err = SerReset(serialState->inputRef, serialState->serConfig);


    if (err != noErr) {
        goto Error;
    }


    err = SerReset(serialState->outputRef, serialState->serConfig);
    if (err != noErr) {
        goto Error;
    }


    return TCL_OK;


    Error:
    switch(err) {
	case controlErr:
	case statusErr:
	case readErr:
	case writErr:
	    errno = EIO;
	    break;
	default:
	    errno = TclMacOSErrorToPosixError(err);
    }
    Tcl_SetErrno(errno);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * MacSerialGetOptionProc --
 *
 *  Computes an option value for a serial based channel, or a
 *  list of all options and their values.
 *
 *  Note: This code is based on code contributed by John Haxby.
 *
 * Results:
 *  A standard Tcl result. The value of the specified option or a
 *  list of all options and  their values is returned in the
 *  supplied DString.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static int
MacSerialGetOptionProc(
    ClientData instanceData,      /* Socket state. */
    Tcl_Interp *interp,                 /* For error reporting - can be NULL.*/
    char *optionName,        /* Name of the option to
			      * retrieve the value for, or
			      * NULL to get all options and
			      * their values. */
    Tcl_DString *dsPtr)        /* Where to store the computed
				* value; initialized by caller. */
{
    SerialState *serialState = (SerialState *) instanceData;


    int doAll=0,
	doBaud=0,
	doDataBits=0,
	doParity=0,
	doStopBits=0;
    char buffer[128];


    /*
     * Determine which options we need to do.  Do all of them
     * if optionName is NULL.
     */


    if (optionName == (char *) NULL || optionName[0] == '\0') {
        doAll = true;
    } else {
	if (!strcmp(optionName, "-baud")) {
	    doBaud = true;
	} else if (!strcmp(optionName, "-databits")) {
	    doDataBits = true;
	} else if (!strcmp(optionName, "-parity")) {
	    doParity = true;
	} else if (!strcmp(optionName, "-stopbits")) {
	    doStopBits = true;
	} else {
	    return Tcl_BadChannelOption(interp, optionName,
		    "baud databits parity stopbits");
	}
    }



    /*
     * Get the values for this channel.
     */


    if (doAll || doBaud) {
        int baud;


	if (doAll) {
	    Tcl_DStringAppendElement(dsPtr, "-baud");
	}


	switch (serialState->serConfig & 0x1FF) {
	    case baud150:
		baud = 150;
		break;
	    case baud300:
		baud = 300;
		break;
	    case baud600:
		baud = 600;
		break;
	    case baud1200:
		baud = 1200;
		break;
	    case baud1800:
		baud = 1800;
		break;
	    case baud2400:
		baud = 2400;
		break;
	    case baud3600:
		baud = 3600;
		break;
	    case baud4800:
		baud = 4800;
		break;
	    case baud7200:
		baud = 7200;
		break;
	    case baud9600:
		baud = 9600;
		break;
	    case baud14400:
		baud = 14400;
		break;
	    case baud19200:
		baud = 19200;
		break;
	    case baud28800:
		baud = 28800;
		break;
	    case baud38400:
		baud = 38400;
		break;
	    case baud57600:
		baud = 57600;
		break;
	}
	sprintf(buffer, "%d", baud);
	Tcl_DStringAppendElement(dsPtr, buffer);
    }


    if (doAll || doDataBits) {
	if (doAll) {
	    Tcl_DStringAppendElement(dsPtr, "-databits");
	}
	switch (serialState->serConfig & 0xC00) {
	    case data5:
		sprintf(buffer, "5"); break;
	    case data6:
		sprintf(buffer, "6"); break;
	    case data7:
		sprintf(buffer, "7"); break;
	    case data8:
		sprintf(buffer, "8"); break;
	}
	Tcl_DStringAppendElement(dsPtr, buffer);
    }


    if (doAll || doParity) {
	if (doAll) {
	    Tcl_DStringAppendElement(dsPtr, "-parity");
	}
	switch (serialState->serConfig & 0x3000) {
	    case noParity:
		sprintf(buffer, "none");
		break;
	    case oddParity:
		sprintf(buffer, "odd");
		break;
	    case evenParity:
		sprintf(buffer, "even");
		break;
	}
	Tcl_DStringAppendElement(dsPtr, buffer);
    }


    if (doAll || doStopBits) {
	if (doAll) {
	    Tcl_DStringAppendElement(dsPtr, "-stopbits");
	}
	switch (serialState->serConfig & 0xC000) {
	    case 0x4000:
		sprintf(buffer, "1"); break;
	    case 0x8000:
		sprintf(buffer, "1.5"); break;
	    case 0xC000:
		sprintf(buffer, "2"); break;
	    default:
		sprintf(buffer, "oops!");
	}
	Tcl_DStringAppendElement(dsPtr, buffer);
    }
    return TCL_OK;
}

/*
* The following routine from from Apple DTS.
*
* It opens both the input and output serial drivers, and returns their
* refNums.  Both refNums come back as an illegal value (0) if we
* can't open either of the drivers.
*
*/


/*
* The one true way of opening a serial driver.  This routine
* tests whether a serial port arbitrator exists.  If it does,
* it relies on the SPA to do the right thing when OpenDriver is called.
* If not, it uses the old mechanism, which is to walk the unit table
* to see whether the driver is already in use by another program.
*/


static OSErr OpenOneSerialDriver(ConstStr255Param driverName, short *refNum)
{
    OSErr err;


    if ( SerialArbitrationExists() ) {
        err = OpenDriver(driverName, refNum);
    } else {
        if ( DriverIsOpen(driverName) ) {
            err = portInUse;
        } else {
            err = OpenDriver(driverName, refNum);
        }
    }
    return err;
}


static OSErr OpenSerialDrivers(ConstStr255Param inName, ConstStr255Param
	outName,
	short *inRefNum, short *outRefNum)
{
    OSErr err;


    err = OpenOneSerialDriver(outName, outRefNum);
    if (err == noErr) {
        err = OpenOneSerialDriver(inName, inRefNum);
        if (err != noErr) {
            (void) CloseDriver(*outRefNum);
        }
    }
    if (err != noErr) {
        *inRefNum = 0;
        *outRefNum = 0;
    }
    return err;
}


static OSErr CloseSerialDrivers(SInt16 inRefNum, SInt16 outRefNum)
{
    OSErr err;


    (void) KillIO(inRefNum);
    err = CloseDriver(inRefNum);
    if (err == noErr) {
        (void) KillIO(outRefNum);
        (void) CloseDriver(outRefNum);
    }


    return err;
}


/*
* Test Gestalt to see if serial arbitration exists
* on this machine.
*/


static Boolean SerialArbitrationExists(void)
{
    Boolean result;
    long    response;


    result = ((Gestalt(gestaltSerialPortArbitratorAttr, &response) == noErr) &&
	    (response & (1 << gestaltSerialPortArbitratorExists) != 0));
    return result;
}


/*
 * Walks the unit table to determine whether the
 * given driver is marked as open in the table.
 * Returns false if the driver is closed, or does
 * not exist.
 */


static Boolean DriverIsOpen(ConstStr255Param driverName)
{
    Boolean     found;
    Boolean     isOpen;
    short       unit;
    DCtlHandle  dceHandle;
    StringPtr   namePtr;


    found = false;
    isOpen = false;


    unit = 0;
    while ( ! found && ( unit < LMGetUnitTableEntryCount() ) ) {


        /*
         *  Get handle to a device control entry.  GetDCtlEntry
         * takes a driver refNum, but we can convert between
         * a unit number and a driver refNum using bitwise not.
         */


        dceHandle = GetDCtlEntry( ~unit );


        if ( dceHandle != nil && (**dceHandle).dCtlDriver != nil ) {


            /*
             * If the driver is RAM based, dCtlDriver is a handle,
             * otherwise it's a pointer.  We have to do some fancy
             * casting to handle each case.  This would be so much
             * easier to read in Pascal )-:
             */


            if ( ((**dceHandle).dCtlFlags & dRAMBasedMask) != 0 ) {
                namePtr = & (**((DRVRHeaderHandle)
			(**dceHandle).dCtlDriver)).drvrName[0];
            } else {
                namePtr = & (*((DRVRHeaderPtr)
			(**dceHandle).dCtlDriver)).drvrName[0];
            }


            /*
             * Now that we have a pointer to the driver name, compare
             * it to the name we're looking for.  If we find it,
             * then we can test the flags to see whether it's open or
             * not.
             */


            if ( EqualString(driverName, namePtr, false, true) ) {
                found = true;
                isOpen = ((**dceHandle).dCtlFlags & dOpenedMask) != 0;
            }
        }
        unit += 1;
    }


    return isOpen;
}
