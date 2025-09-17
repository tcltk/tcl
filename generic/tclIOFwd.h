/*
 * tclIOFwd.h --
 *
 *	This file provides the generic portions of Tcl's IO forwarding.
 *
 * Copyright © 2004-2008 ActiveState.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * Event used to forward driver invocations to the thread actually managing
 * the channel. We cannot construct the command to execute and forward that.
 * Because then it will contain a mixture of Tcl_Obj's belonging to both the
 * command handler thread (CT), and the thread managing the channel (MT),
 * executed in CT. Tcl_Obj's are not allowed to cross thread boundaries. So we
 * forward an operation code, the argument details, and reference to results.
 * The command is assembled in the CT and belongs fully to that thread. No
 * sharing problems.
 *
 * The forwarding code is not in this file.
 */

typedef struct ForwardParamBase {
    int code;			/* O: Ok/Fail of the cmd handler */
    char *msgStr;		/* O: Error message for handler failure */
    int mustFree;		/* O: True if msgStr is allocated, false if
				 * otherwise (static). */
} ForwardParamBase;

/*
 * Common operations on types that extend ForwardParamBase.
 */

// Free the error message in the event.
#define FreeReceivedError(p) \
    do {							\
	if ((p)->base.mustFree) {				\
	    Tcl_Free((p)->base.msgStr);				\
	}							\
    } while (0)

// Transfer the error from the event to the interpreter.
#define PassReceivedErrorInterp(interp, p) \
    do {							\
	if ((interp) != NULL) {					\
	    Tcl_SetChannelErrorInterp((interp),			\
		    Tcl_NewStringObj((p)->base.msgStr, -1));	\
	}							\
	FreeReceivedError(p);					\
    } while (0)

// Transfer the error from the event to the channel.
#define PassReceivedError(chan, p) \
    do {							\
	Tcl_SetChannelError((chan),				\
		Tcl_NewStringObj((p)->base.msgStr, -1));	\
	FreeReceivedError(p);					\
    } while (0)

// Set the event to return a static error string.
// Assumes the string does not need to be freed.
#define ForwardSetStaticError(p, emsg) \
    do {							\
	(p)->base.code = TCL_ERROR;				\
	(p)->base.mustFree = 0;					\
	(p)->base.msgStr = (char *) (emsg);			\
    } while (0)

// Set the event to return an error string allocated with Tcl_Alloc().
// Takes responsibility for freeing the string.
#define ForwardSetDynamicError(p, emsg) \
    do {							\
	(p)->base.code = TCL_ERROR;				\
	(p)->base.mustFree = 1;					\
	(p)->base.msgStr = (char *) (emsg);			\
    } while (0)

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 */
