/* 
 * tclIORChan.c --
 *
 *	This file contains the implementation of Tcl's generic
 *	channel reflection code, which allows the implementation
 *	of Tcl channels in Tcl code.
 *
 *	Parts of this file are based on code contributed by 
 *	Jean-Claude Wippler.
 *
 *      See TIP #219 for the specification of this functionality.
 *
 * Copyright (c) 2004-2005 ActiveState, a divison of Sophos
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclIORChan.c,v 1.1.2.5 2005/09/15 20:58:39 dgp Exp $
 */

#include <tclInt.h>
#include <tclIO.h>
#include <assert.h>

#ifndef EINVAL
#define EINVAL 9
#endif
#ifndef EOK
#define EOK    0
#endif

/*
 * Signatures of all functions used in the C layer of the reflection.
 */

/* Required */
static int	RcClose _ANSI_ARGS_((ClientData clientData,
		   Tcl_Interp *interp));

/* Required, "read" is optional despite this. */
static int	RcInput _ANSI_ARGS_((ClientData clientData,
		    char *buf, int toRead, int *errorCodePtr));

/* Required, "write" is optional despite this. */
static int	RcOutput _ANSI_ARGS_((ClientData clientData,
	            CONST char *buf, int toWrite, int *errorCodePtr));

/* Required */
static void	RcWatch _ANSI_ARGS_((ClientData clientData, int mask));

/* NULL'able - "blocking", is optional */
static int	RcBlock _ANSI_ARGS_((ClientData clientData,
				       int mode));

/* NULL'able - "seek", is optional */
static Tcl_WideInt RcSeekWide _ANSI_ARGS_((ClientData clientData,
		    Tcl_WideInt offset,
		    int mode, int *errorCodePtr));

static int RcSeek _ANSI_ARGS_((ClientData clientData,
		    long offset, int mode, int *errorCodePtr));

/* NULL'able - "cget" / "cgetall", are optional */
static int	RcGetOption _ANSI_ARGS_((ClientData clientData,
				       Tcl_Interp* interp,
				       CONST char *optionName,
				       Tcl_DString *dsPtr));

/* NULL'able - "configure", is optional */
static int	RcSetOption _ANSI_ARGS_((ClientData clientData,
				       Tcl_Interp* interp,
				       CONST char *optionName,
				       CONST char *newValue));


/*
 * The C layer channel type/driver definition used by the reflection.
 * This is a version 3 structure.
 */

static Tcl_ChannelType tclRChannelType = {
  "tclrchannel",  /* Type name.                                    */
  TCL_CHANNEL_VERSION_3,
  RcClose,        /* Close channel, clean instance data            */
  RcInput,        /* Handle read request                           */
  RcOutput,       /* Handle write request                          */
  RcSeek,         /* Move location of access point.    NULL'able   */
  RcSetOption,    /* Set options.                      NULL'able   */
  RcGetOption,    /* Get options.                      NULL'able   */
  RcWatch,        /* Initialize notifier                           */
  NULL,           /* Get OS handle from the channel.   NULL'able   */
  NULL,           /* No close2 support.                NULL'able   */
  RcBlock,        /* Set blocking/nonblocking.         NULL'able   */
  NULL,           /* Flush channel. Not used by core.  NULL'able   */
  NULL,           /* Handle events.                    NULL'able   */
  RcSeekWide      /* Move access point (64 bit).       NULL'able   */
};

/*
 * Instance data for a reflected channel. ===========================
 */

typedef struct {
  Tcl_Channel chan;    /* Back reference to generic channel structure.
		        */
  Tcl_Interp* interp;  /* Reference to the interpreter containing the
		        * Tcl level part of the channel. */
#ifdef TCL_THREADS
  Tcl_ThreadId thread; /* Thread the 'interp' belongs to. */
#endif

  /* See [==] as well.
   * Storage for the command prefix and the additional words required
   * for the invocation of methods in the command handler.
   *
   * argv [0] ... [.] | [argc-2] [argc-1] | [argc]  [argc+2]
   *      cmd ... pfx | method   chan     | detail1 detail2
   *      ~~~~ CT ~~~            ~~ CT ~~
   *
   * CT = Belongs to the 'Command handler Thread'.
   */

  int       argc;       /* Number of preallocated words - 2 */
  Tcl_Obj** argv;       /* Preallocated array for calling the handler.
			 * args [0] is placeholder for cmd word.
			 * Followed by the arguments in the prefix,
			 * plus 4 placeholders for method, channel,
			 * and at most two varying (method specific)
			 * words.
			 */

  int methods;          /* Bitmask of supported methods */

  /* ---------------------------------------- */

  /* NOTE (9): Should we have predefined shared literals
   * NOTE (9): for the method names ?
   */

  /* ---------------------------------------- */

  int mode;             /* Mask of R/W mode */
  int interest;         /* Mask of events the channel is interested in. */

  /* Note regarding the usage of timers.
   *
   * Most channel implementations need a timer in the
   * C level to ensure that data in buffers is flushed
   * out through the generation of fake file events.
   *
   * See 'rechan', 'memchan', etc.
   *
   * Here this is _not_ required. Interest in events is
   * posted to the Tcl level via 'watch'. And posting of
   * events is possible from the Tcl level as well, via
   * 'chan postevent'. This means that the generation of
   * all events, fake or not, timer based or not, is
   * completely in the hands of the Tcl level. Therefore
   * no timer here.
   */

} ReflectingChannel;

/*
 * Event literals. ==================================================
 */

static CONST char *eventOptions[] = {
  "read", "write", (char *) NULL
};
typedef enum {
  EVENT_READ, EVENT_WRITE
} EventOption;

/*
 * Method literals. ==================================================
 */

static CONST char *methodNames[] = {
  "blocking",	/* OPT */
  "cget",	/* OPT \/ Together or none */
  "cgetall",	/* OPT /\ of these two     */
  "configure",	/* OPT */
  "finalize",	/*     */
  "initialize",	/*     */
  "read",	/* OPT */
  "seek",	/* OPT */
  "watch",	/*     */
  "write",	/* OPT */
  (char *) NULL
};
typedef enum {
  METH_BLOCKING,
  METH_CGET,
  METH_CGETALL,
  METH_CONFIGURE,
  METH_FINAL,
  METH_INIT,
  METH_READ,
  METH_SEEK,
  METH_WATCH,
  METH_WRITE,
} MethodName;

#define FLAG(m) (1 << (m))
#define REQUIRED_METHODS (FLAG (METH_INIT) | FLAG (METH_FINAL) | FLAG (METH_WATCH))
#define NULLABLE_METHODS (FLAG (METH_BLOCKING) | FLAG (METH_SEEK) | \
	FLAG (METH_CONFIGURE) | FLAG (METH_CGET) | FLAG (METH_CGETALL))

#define RANDW (TCL_READABLE|TCL_WRITABLE)

#define IMPLIES(a,b) ((!(a)) || (b))
#define NEGIMPL(a,b)
#define HAS(x,f) (x & FLAG(f))


#ifdef TCL_THREADS
/*
 * Thread specific types and structures.
 *
 * We are here essentially creating a very specific implementation of
 * 'thread send'.
 */

/*
 * Enumeration of all operations which can be forwarded.
 */

typedef enum {
  RcOpClose,
  RcOpInput,
  RcOpOutput,
  RcOpSeek,
  RcOpWatch,
  RcOpBlock,
  RcOpSetOpt,
  RcOpGetOpt,
  RcOpGetOptAll
} RcOperation;

/*
 * Event used to forward driver invocations to the thread actually
 * managing the channel. We cannot construct the command to execute
 * and forward that. Because then it will contain a mixture of
 * Tcl_Obj's belonging to both the command handler thread (CT), and
 * the thread managing the channel (MT), executed in CT. Tcl_Obj's are
 * not allowed to cross thread boundaries. So we forward an operation
 * code, the argument details ,and reference to results. The command
 * is assembled in the CT and belongs fully to that thread. No sharing
 * problems.
 */

typedef struct RcForwardParamBase {
  int   code; /* O: Ok/Fail of the cmd handler */
  char* msg;  /* O: Error message for handler failure */
  int   vol;  /* O: True - msg is allocated, False - msg is static */
} RcForwardParamBase;

/*
 * Operation specific parameter/result structures.
 */

typedef struct RcForwardParamClose {
  RcForwardParamBase b;
} RcForwardParamClose;

typedef struct RcForwardParamInput {
  RcForwardParamBase b;
  char* buf;    /* O: Where to store the read bytes */
  int   toRead; /* I: #bytes to read,
		 * O: #bytes actually read */
} RcForwardParamInput;

typedef struct RcForwardParamOutput {
  RcForwardParamBase b;
  CONST char* buf;     /* I: Where the bytes to write come from */
  int         toWrite; /* I: #bytes to write,
			* O: #bytes actually written */
} RcForwardParamOutput;

typedef struct RcForwardParamSeek {
  RcForwardParamBase b;
  int         seekMode; /* I: How to seek */
  Tcl_WideInt offset;   /* I: Where to seek,
			 * O: New location */
} RcForwardParamSeek;

typedef struct RcForwardParamWatch {
  RcForwardParamBase b;
  int mask; /* I: What events to watch for */
} RcForwardParamWatch;

typedef struct RcForwardParamBlock {
  RcForwardParamBase b;
  int nonblocking; /* I: What mode to activate */
} RcForwardParamBlock;

typedef struct RcForwardParamSetOpt {
  RcForwardParamBase b;
  CONST char* name;  /* Name of option to set */
  CONST char* value; /* Value to set */
} RcForwardParamSetOpt;

typedef struct RcForwardParamGetOpt {
  RcForwardParamBase b;
  CONST char*  name;  /* Name of option to get, maybe NULL */
  Tcl_DString* value; /* Result */
} RcForwardParamGetOpt;

/*
 * General event structure, with reference to
 * operation specific data.
 */

typedef struct RcForwardingEvent {
  Tcl_Event                  event; /* Basic event data, has to be first item */
  struct RcForwardingResult* resultPtr;

  RcOperation               op;    /* Forwarded driver operation */
  ReflectingChannel*        rcPtr; /* Channel instance */
  CONST RcForwardParamBase* param; /* Arguments, a RcForwardParamXXX pointer */
} RcForwardingEvent;

/*
 * Structure to manage the result of the forwarding.  This is not the
 * result of the operation itself, but about the success of the
 * forward event itself. The event can be successful, even if the
 * operation which was forwarded failed. It is also there to manage
 * the synchronization between the involved threads.
 */

typedef struct RcForwardingResult {

  Tcl_ThreadId  src;    /* Originating thread. */
  Tcl_ThreadId  dst;    /* Thread the op was forwarded to. */
  Tcl_Condition done;   /* Condition variable the forwarder blocks on. */
  int           result; /* TCL_OK or TCL_ERROR */

  struct RcForwardingEvent*  evPtr; /* Event the result belongs to. */

  struct RcForwardingResult* prevPtr; /* Links into the list of pending */
  struct RcForwardingResult* nextPtr; /* forwarded results. */

} RcForwardingResult;

/*
 * List of forwarded operations which have not completed yet, plus the
 * mutex to protect the access to this process global list.
 */

static RcForwardingResult* forwardList = (RcForwardingResult*) NULL;
TCL_DECLARE_MUTEX (rcForwardMutex)

/*
 * Function containing the generic code executing a forward, and
 * wrapper macros for the actual operations we wish to forward.
 */

static void
RcForwardOp _ANSI_ARGS_ ((ReflectingChannel* rcPtr, RcOperation op,
			  Tcl_ThreadId dst, CONST VOID* param));

/*
 * The event function executed by the thread receiving a forwarding
 * event. Executes the appropriate function and collects the result,
 * if any.
 */

static int
RcForwardProc _ANSI_ARGS_ ((Tcl_Event *evPtr, int mask));

/*
 * Helpers which intercept when threads are going away, and clean up
 * after pending forwarding events. Different actions depending on
 * which thread went away, originator (src), or receiver (dst).
 */

static void
RcSrcExitProc _ANSI_ARGS_ ((ClientData clientData));

static void
RcDstExitProc _ANSI_ARGS_ ((ClientData clientData));

#define RcFreeReceivedError(pb) \
	if ((pb).vol) {ckfree ((pb).msg);}

#define RcPassReceivedErrorInterp(i,pb) \
	if ((i)) {Tcl_SetChannelErrorInterp ((i), Tcl_NewStringObj ((pb).msg,-1));} \
	RcFreeReceivedError (pb)

#define RcPassReceivedError(c,pb) \
	Tcl_SetChannelError ((c), Tcl_NewStringObj ((pb).msg,-1)); \
	RcFreeReceivedError (pb)

#define RcForwardSetStaticError(p,emsg) \
       (p)->code = TCL_ERROR; (p)->vol  = 0; (p)->msg  = (char*) (emsg);

#define RcForwardSetDynError(p,emsg) \
       (p)->code = TCL_ERROR; (p)->vol  = 1; (p)->msg  = (char*) (emsg);

static void
RcForwardSetObjError _ANSI_ARGS_ ((RcForwardParamBase* p,
				   Tcl_Obj*            obj));

#endif /* TCL_THREADS */

#define RcSetChannelErrorStr(c,msg) \
	Tcl_SetChannelError ((c), Tcl_NewStringObj ((msg),-1))

static Tcl_Obj* RcErrorMarshall _ANSI_ARGS_ ((Tcl_Interp *interp));
static void     RcErrorReturn   _ANSI_ARGS_ ((Tcl_Interp* interp, Tcl_Obj* msg));



/*
 * Static functions for this file:
 */

static int RcEncodeEventMask _ANSI_ARGS_((Tcl_Interp* interp,
		 CONST char* objName, Tcl_Obj* obj,
		 int* mask));

static Tcl_Obj* RcDecodeEventMask _ANSI_ARGS_ ((int mask));

static ReflectingChannel* RcNew _ANSI_ARGS_ ((Tcl_Interp* interp,
	     Tcl_Obj* cmdpfxObj, int mode,
	     Tcl_Obj* id));

static Tcl_Obj* RcNewHandle _ANSI_ARGS_ ((void));

static void RcFree _ANSI_ARGS_ ((ReflectingChannel* rcPtr));

static void
RcInvokeTclMethod _ANSI_ARGS_((ReflectingChannel* rcPtr,
       CONST char* method, Tcl_Obj* argone, Tcl_Obj* argtwo,
       int* result, Tcl_Obj** resultObj, int capture));

#define NO_CAPTURE (0)
#define DO_CAPTURE (1)

/*
 * Global constant strings (messages). ==================
 * These string are used directly as bypass errors, thus they have to be valid
 * Tcl lists where the last element is the message itself. Hence the
 * list-quoting to keep the words of the message together. See also [x].
 */

static CONST char* msg_read_unsup       = "{read not supported by Tcl driver}";
static CONST char* msg_read_toomuch     = "{read delivered more than requested}";
static CONST char* msg_write_unsup      = "{write not supported by Tcl driver}";
static CONST char* msg_write_toomuch    = "{write wrote more than requested}";
static CONST char* msg_seek_beforestart = "{Tried to seek before origin}";

#ifdef TCL_THREADS
static CONST char* msg_send_originlost  = "{Origin thread lost}";
static CONST char* msg_send_dstlost     = "{Destination thread lost}";
#endif /* TCL_THREADS */

/*
 * Main methods to plug into the 'chan' ensemble'. ==================
 */

/*
 *----------------------------------------------------------------------
 *
 * TclChanCreateObjCmd --
 *
 *	This procedure is invoked to process the "chan create" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *	The handle of the new channel is placed in the interp result.
 *
 * Side effects:
 *	Creates a new channel.
 *
 *----------------------------------------------------------------------
 */

int
TclChanCreateObjCmd (/*ignored*/ clientData, interp, objc, objv)
     ClientData      clientData;
     Tcl_Interp*     interp;
     int             objc;
     Tcl_Obj* CONST* objv;
{
    ReflectingChannel* rcPtr;       /* Instance data of the new channel */
    Tcl_Obj*           rcId;        /* Handle of the new channel */
    int                mode;        /* R/W mode of new channel. Has to
				     * match abilities of handler commands */
    Tcl_Obj*           cmdObj;      /* Command prefix, list of words */
    Tcl_Obj*           cmdNameObj;  /* Command name */
    Tcl_Channel        chan;        /* Token for the new channel */
    Tcl_Obj*           modeObj;     /* mode in obj form for method call */
    int                listc;       /* Result of 'initialize', and of */
    Tcl_Obj**          listv;       /* its sublist in the 2nd element */
    int                methIndex;   /* Encoded method name */
    int                res;         /* Result code for 'initialize' */
    Tcl_Obj*           resObj;      /* Result data for 'initialize' */
    int                methods;     /* Bitmask for supported methods. */
    Channel*           chanPtr;     /* 'chan' resolved to internal struct. */

    /* Syntax:   chan create MODE CMDPREFIX
     *           [0]  [1]    [2]  [3]
     *
     * Actually: rCreate MODE CMDPREFIX
     *           [0]     [1]  [2]
     */

#define MODE (1)
#define CMD  (2)

    /* Number of arguments ... */

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "mode cmdprefix");
	return TCL_ERROR;
    }

    /* First argument is a list of modes. Allowed entries are "read",
     * "write". Expect at least one list element.  Abbreviations are
     * ok.
     */

    modeObj = objv [MODE];
    if (RcEncodeEventMask (interp, "mode", objv [MODE], &mode) != TCL_OK) {
        return TCL_ERROR;
    }

    /* Second argument is command prefix, i.e. list of words, first
     * word is name of handler command, other words are fixed
     * arguments. Run 'initialize' method to get the list of supported
     * methods. Validate this.
     */

    cmdObj = objv [CMD];

    /* Basic check that the command prefix truly is a list. */

    if (Tcl_ListObjIndex(interp, cmdObj, 0, &cmdNameObj) != TCL_OK) {
        return TCL_ERROR;
    }

    /* Now create the channel.
     */

    rcId  = RcNewHandle ();
    rcPtr = RcNew (interp, cmdObj, mode, rcId);
    chan  = Tcl_CreateChannel (&tclRChannelType,
			       Tcl_GetString (rcId),
			       rcPtr, mode);
    rcPtr->chan = chan;
    chanPtr = (Channel*) chan;

    /* Invoke 'initialize' and validate that the handler
     * is present and ok. Squash the channel if not.
     */

    /* Note: The conversion of 'mode' back into a Tcl_Obj ensures that
     * 'initialize' is invoked with canonical mode names, and no
     * abbreviations. Using modeObj directly could feed abbreviations
     * into the handler, and the handler is not specified to handle
     * such.
     */

    modeObj = RcDecodeEventMask (mode);
    RcInvokeTclMethod (rcPtr, "initialize", modeObj, NULL,
		       &res, &resObj, NO_CAPTURE);
    Tcl_DecrRefCount (modeObj);

    if (res != TCL_OK) {
        Tcl_Obj* err = Tcl_NewStringObj ("Initialize failure: ",-1);

	Tcl_AppendObjToObj(err,resObj);
	Tcl_SetObjResult (interp,err);
	Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	goto error;
    }

    /* Verify the result.
     * - List, of method names. Convert to mask.
     *   Check for non-optionals through the mask.
     *   Compare open mode against optional r/w.
     */

    Tcl_AppendResult (interp, "Initialize failure: ", (char*) NULL);

    if (Tcl_ListObjGetElements (interp, resObj,
			&listc, &listv) != TCL_OK) {
        /* The function above replaces my prefix in case of an error,
	 * so more work for us to get the prefix back into the error
	 * message
	 */

        Tcl_Obj* err = Tcl_NewStringObj ("Initialize failure: ",-1);

	Tcl_AppendObjToObj(err,Tcl_GetObjResult (interp));
	Tcl_SetObjResult (interp,err);
	goto error;
    }

    methods = 0;
    while (listc > 0) {
        if (Tcl_GetIndexFromObj (interp, listv [listc-1],
				 methodNames, "method", TCL_EXACT, &methIndex) != TCL_OK) {
	    Tcl_Obj* err = Tcl_NewStringObj ("Initialize failure: ",-1);

	    Tcl_AppendObjToObj(err,Tcl_GetObjResult (interp));
	    Tcl_SetObjResult (interp,err);
	    goto error;
	}

	methods |= FLAG (methIndex);
	listc --;
    }

    if ((REQUIRED_METHODS & methods) != REQUIRED_METHODS) {
        Tcl_AppendResult (interp, "Not all required methods supported",
			  (char*) NULL);
	goto error;
    }

    if ((mode & TCL_READABLE) && !HAS(methods,METH_READ)) {
        Tcl_AppendResult (interp, "Reading not supported, but requested",
			  (char*) NULL);
	goto error;
    }

    if ((mode & TCL_WRITABLE) && !HAS(methods,METH_WRITE)) {
        Tcl_AppendResult (interp, "Writing not supported, but requested",
			  (char*) NULL);
	goto error;
    }

    if (!IMPLIES (HAS(methods,METH_CGET), HAS(methods,METH_CGETALL))) {
        Tcl_AppendResult (interp, "'cgetall' not supported, but should be, as 'cget' is",
			  (char*) NULL);
	goto error;
    }

    if (!IMPLIES (HAS(methods,METH_CGETALL),HAS(methods,METH_CGET))) {
        Tcl_AppendResult (interp, "'cget' not supported, but should be, as 'cgetall' is",
			  (char*) NULL);
	goto error;
    }

    Tcl_ResetResult (interp);

    /* Everything is fine now */

    rcPtr->methods = methods;

    if ((methods & NULLABLE_METHODS) != NULLABLE_METHODS) {
        /* Some of the nullable methods are not supported.  We clone
	 * the channel type, null the associated C functions, and use
	 * the result as the actual channel type.
	 */

        Tcl_ChannelType* clonePtr = (Tcl_ChannelType*) ckalloc (sizeof (Tcl_ChannelType));
	if (clonePtr == (Tcl_ChannelType*) NULL) {
	    Tcl_Panic ("Out of memory in Tcl_RcCreate");
	}

	memcpy (clonePtr, &tclRChannelType, sizeof (Tcl_ChannelType));

	if (!(methods & FLAG (METH_CONFIGURE))) {
	  clonePtr->setOptionProc = NULL;
	}

	if (
	    !(methods & FLAG (METH_CGET)) &&
	    !(methods & FLAG (METH_CGETALL))
	    ) {
	    clonePtr->getOptionProc = NULL;
	}
	if (!(methods & FLAG (METH_BLOCKING))) {
	    clonePtr->blockModeProc = NULL;
	}
	if (!(methods & FLAG (METH_SEEK))) {
	    clonePtr->seekProc     = NULL;
	    clonePtr->wideSeekProc = NULL;
	}

	chanPtr->typePtr = clonePtr;
    }

    Tcl_RegisterChannel (interp, chan);

    /* Return handle as result of command */

    Tcl_SetObjResult (interp, rcId);
    return TCL_OK;

 error:
    /* Signal to RcClose to not call 'finalize' */
    rcPtr->methods = 0;
    Tcl_Close (interp, chan);
    return TCL_ERROR;

#undef MODE
#undef CMD
}

/*
 *----------------------------------------------------------------------
 *
 * TclChanPostEventObjCmd --
 *
 *	This procedure is invoked to process the "chan postevent"
 *	Tcl command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Posts events to a reflected channel, invokes event handlers.
 *	The latter implies that arbitrary side effects are possible.
 *
 *----------------------------------------------------------------------
 */

int
TclChanPostEventObjCmd (/*ignored*/ clientData, interp, objc, objv)
     ClientData      clientData;
     Tcl_Interp*     interp;
     int             objc;
     Tcl_Obj* CONST* objv;
{
    /* Syntax:   chan postevent CHANNEL EVENTSPEC
     *           [0]  [1]       [2]     [3]
     *
     * Actually: rPostevent CHANNEL EVENTSPEC
     *           [0]        [1]     [2]
     *
     *         where EVENTSPEC = {read write ...} (Abbreviations allowed as well.
     */

#define CHAN  (1)
#define EVENT (2)

    CONST char*        chanId;      /* Tcl level channel handle */
    Tcl_Channel        chan;        /* Channel associated to the handle */
    Tcl_ChannelType*   chanTypePtr; /* Its associated driver structure */
    ReflectingChannel* rcPtr;       /* Associated instance data */
    int                mode;        /* Dummy, r|w mode of the channel */
    int                events;      /* Mask of events to post */

    /* Number of arguments ... */

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "channel eventspec");
	return TCL_ERROR;
    }

    /* First argument is a channel, a reflected channel, and the call
     * of this command is done from the interp defining the channel
     * handler cmd.
     */

    chanId = Tcl_GetString (objv [CHAN]);
    chan   = Tcl_GetChannel(interp, chanId, &mode);

    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }

    chanTypePtr = Tcl_GetChannelType (chan);

    /* We use a function referenced by the channel type as our cookie
     * to detect calls to non-reflecting channels. The channel type
     * itself is not suitable, as it might not be the static
     * definition in this file, but a clone thereof. And while we have
     * reserved the name of the type nothing in the core checks
     * against violation, so someone else might have created a channel
     * type using our name, clashing with ourselves.
     */

    if (chanTypePtr->watchProc != &RcWatch) {
        Tcl_AppendResult(interp, "channel \"", chanId,
			 "\" is not a reflected channel",
			 (char *) NULL);
	return TCL_ERROR;
    }

    rcPtr = (ReflectingChannel*) Tcl_GetChannelInstanceData (chan);

    if (rcPtr->interp != interp) {
        Tcl_AppendResult(interp, "postevent for channel \"", chanId,
			 "\" called from outside interpreter",
			 (char *) NULL);
	return TCL_ERROR;
    }

    /* Second argument is a list of events. Allowed entries are
     * "read", "write". Expect at least one list element.
     * Abbreviations are ok.
     */

    if (RcEncodeEventMask (interp, "event", objv [EVENT], &events) != TCL_OK) {
        return TCL_ERROR;
    }
    
    /* Check that the channel is actually interested in the provided
     * events.
     */

    if (events & ~rcPtr->interest) {
        Tcl_AppendResult(interp, "tried to post events channel \"", chanId,
			 "\" is not interested in",
			 (char *) NULL);
	return TCL_ERROR;
    }

    /* We have the channel and the events to post.
     */

    Tcl_NotifyChannel (chan, events);

    /* Squash interp results left by the event script.
     */

    Tcl_ResetResult (interp);
    return TCL_OK;

#undef CHAN
#undef EVENT
}


static Tcl_Obj*
RcErrorMarshall (interp)
     Tcl_Interp *interp;
{
    /* Capture the result status of the interpreter into a string.
     * => List of options and values, followed by the error message.
     * The result has refCount 0.
     */
    
    Tcl_Obj* returnOpt = Tcl_GetReturnOptions (interp, TCL_ERROR);

    /* => returnOpt.refCount == 0. We can append directly.
     */

    Tcl_ListObjAppendElement (NULL, returnOpt, Tcl_GetObjResult (interp));
    return returnOpt;
}

static void
RcErrorReturn (interp, msg)
     Tcl_Interp *interp;
     Tcl_Obj    *msg;
{
    int       res;
    int       lc;
    Tcl_Obj** lv;
    int       explicitResult;
    int       numOptions;

    /* Process the caught message.
     *
     * Syntax = (option value)... ?message?
     *
     * Bad syntax causes a panic. Because the other side uses
     * Tcl_GetReturnOptions and list construction functions to marshall the
     * information.
     */

    res = Tcl_ListObjGetElements (interp, msg, &lc, &lv);
    if (res != TCL_OK) {
	Tcl_Panic ("TclChanCaughtErrorBypass: Bad syntax of caught result");
    }

    explicitResult = (1 == (lc % 2));
    numOptions     = lc - explicitResult;

    if (explicitResult) {
	Tcl_SetObjResult (interp, lv [lc-1]);
    }

    (void) Tcl_SetReturnOptions(interp, Tcl_NewListObj (numOptions, lv));
}

int
TclChanCaughtErrorBypass (interp, chan)
     Tcl_Interp *interp;
     Tcl_Channel chan;
{
    Tcl_Obj* msgc = NULL;
    Tcl_Obj* msgi = NULL;
    Tcl_Obj* msg  = NULL;

    /* Get a bypassed error message from channel and/or interpreter, save the
     * reference, then kill the returned objects, if there were any. If there
     * are messages in both the channel has preference.
     */

    if ((chan == NULL) && (interp == NULL)) {
	return 0;
    }

    if (chan != NULL) {
	Tcl_GetChannelError       (chan,   &msgc);
    }
    if (interp != NULL) {
	Tcl_GetChannelErrorInterp (interp, &msgi);
    }

    if (msgc != NULL) {
	msg = msgc;
	Tcl_IncrRefCount (msg);
    } else if (msgi != NULL) {
	msg = msgi;
	Tcl_IncrRefCount (msg);
    }

    if (msgc != NULL) {
	Tcl_DecrRefCount (msgc);
    }
    if (msgi != NULL) {
	Tcl_DecrRefCount (msgi);
    }

    /* No message returned, nothing caught.
     */

    if (msg == NULL) {
	return 0;
    }

    RcErrorReturn (interp, msg);

    Tcl_DecrRefCount (msg);
    return 1;
}

/*
 * Driver functions. ================================================
 */

/*
 *----------------------------------------------------------------------
 *
 * RcClose --
 *
 *	This function is invoked when the channel is closed, to delete
 *	the driver specific instance data.
 *
 * Results:
 *	A posix error.
 *
 * Side effects:
 *	Releases memory. Arbitrary, as it calls upon a script.
 *
 *----------------------------------------------------------------------
 */

static int
RcClose (clientData, interp)
     ClientData  clientData;
     Tcl_Interp* interp;
{
    ReflectingChannel* rcPtr = (ReflectingChannel*) clientData;
    int                res;         /* Result code for 'close' */
    Tcl_Obj*           resObj;      /* Result data for 'close' */

    if (interp == (Tcl_Interp*) NULL) {
        /* This call comes from TclFinalizeIOSystem. There are no
	 * interpreters, and therefore we cannot call upon the handler
	 * command anymore. Threading is irrelevant as well.  We
	 * simply clean up all our C level data structures and leave
	 * the Tcl level to the other finalization functions.
	 */

      /* THREADED => Forward this to the origin thread */
      /* Note: Have a thread delete handler for the origin
       * thread. Use this to clean up the structure!
       */

#ifdef TCL_THREADS
        /* Are we in the correct thread ?
	 */

        if (rcPtr->thread != Tcl_GetCurrentThread ()) {
	    RcForwardParamClose p;

	    RcForwardOp (rcPtr, RcOpClose, rcPtr->thread, &p);
	    res = p.b.code;

	    /* RcFree is done in the forwarded operation!,
	     * in the other thread. rcPtr here is gone!
	     */

	    if (res != TCL_OK) {
	        RcFreeReceivedError (p.b);
	    }
	} else {
#endif
	    RcFree (rcPtr);
#ifdef TCL_THREADS
	}
#endif
	return EOK;
    }

    /* -------- */

    /* -- No -- ASSERT rcPtr->methods & FLAG (METH_FINAL) */

    /* A cleaned method mask here implies that the channel creation
     * was aborted, and "finalize" must not be called.
     */

    if (rcPtr->methods == 0) {
        RcFree (rcPtr);
        return EOK;
    } else {
#ifdef TCL_THREADS
        /* Are we in the correct thread ?
	 */

        if (rcPtr->thread != Tcl_GetCurrentThread ()) {
	    RcForwardParamClose p;

	    RcForwardOp (rcPtr, RcOpClose, rcPtr->thread, &p);
	    res = p.b.code;

	    /* RcFree is done in the forwarded operation!,
	     * in the other thread. rcPtr here is gone!
	     */

	    if (res != TCL_OK) {
	        RcPassReceivedErrorInterp (interp, p.b);
	    }
	} else {
#endif
	    RcInvokeTclMethod (rcPtr, "finalize", NULL, NULL,
			       &res, &resObj, DO_CAPTURE);

	    if ((res != TCL_OK) && (interp != NULL)) {
	        Tcl_SetChannelErrorInterp (interp, resObj);
	    }

	    Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
#ifdef TCL_THREADS
	    RcFree (rcPtr);
	}
#endif
	return (res == TCL_OK) ? EOK : EINVAL;
    }

    Tcl_Panic ("Should not be reached");
    return EINVAL;
}

/*
 *----------------------------------------------------------------------
 *
 * RcInput --
 *
 *	This function is invoked when more data is requested from the
 *	channel.
 *
 * Results:
 *	The number of bytes read.
 *
 * Side effects:
 *	Allocates memory. Arbitrary, as it calls upon a script.
 *
 *----------------------------------------------------------------------
 */

static int
RcInput (clientData, buf, toRead, errorCodePtr)
     ClientData clientData;
     char* buf;
     int toRead;
     int* errorCodePtr;
{
    ReflectingChannel* rcPtr = (ReflectingChannel*) clientData;
    Tcl_Obj*           toReadObj;
    int                bytec;       /* Number of returned bytes */
    unsigned char*     bytev;       /* Array of returned bytes */
    int                res;         /* Result code for 'read' */
    Tcl_Obj*           resObj;      /* Result data for 'read' */

    /* The following check can be done before thread redirection,
     * because we are reading from an item which is readonly, i.e.
     * will never change during the lifetime of the channel.
     */

    if (!(rcPtr->methods & FLAG (METH_READ))) {
        RcSetChannelErrorStr (rcPtr->chan, msg_read_unsup);
	*errorCodePtr = EINVAL;
	return -1;
    }

#ifdef TCL_THREADS
    /* Are we in the correct thread ?
     */

    if (rcPtr->thread != Tcl_GetCurrentThread ()) {
        RcForwardParamInput p;

	p.buf    = buf;
	p.toRead = toRead;

	RcForwardOp (rcPtr, RcOpInput, rcPtr->thread, &p);

	if (p.b.code != TCL_OK) {
	    RcPassReceivedError (rcPtr->chan, p.b);
	    *errorCodePtr = EINVAL;
	} else {
	    *errorCodePtr = EOK;
	}

	return p.toRead;
    }
#endif

    /* -------- */

    /* ASSERT: rcPtr->method & FLAG (METH_READ) */
    /* ASSERT: rcPtr->mode & TCL_READABLE */

    toReadObj = Tcl_NewIntObj(toRead);
    if (toReadObj == (Tcl_Obj*) NULL) {
        Tcl_Panic ("Out of memory in RcInput");
    }

    RcInvokeTclMethod (rcPtr, "read", toReadObj, NULL,
		       &res, &resObj, DO_CAPTURE);

    if (res != TCL_OK) {
	Tcl_SetChannelError (rcPtr->chan, resObj);
        Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	*errorCodePtr = EINVAL;
	return -1;
    }

    bytev = Tcl_GetByteArrayFromObj(resObj, &bytec);

    if (toRead < bytec) {
        Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	RcSetChannelErrorStr (rcPtr->chan, msg_read_toomuch);
	*errorCodePtr = EINVAL;
	return -1;
    }

    *errorCodePtr = EOK;

    if (bytec > 0) {
        memcpy (buf, bytev, bytec);
    }

    Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
    return bytec;
}

/*
 *----------------------------------------------------------------------
 *
 * RcOutput --
 *
 *	This function is invoked when data is writen to the
 *	channel.
 *
 * Results:
 *	The number of bytes actually written.
 *
 * Side effects:
 *	Allocates memory. Arbitrary, as it calls upon a script.
 *
 *----------------------------------------------------------------------
 */

static int
RcOutput (clientData, buf, toWrite, errorCodePtr)
     ClientData clientData;
     CONST char* buf;
     int toWrite;
     int* errorCodePtr;
{
    ReflectingChannel* rcPtr = (ReflectingChannel*) clientData;
    Tcl_Obj*       bufObj;
    int            res;         /* Result code for 'write' */
    Tcl_Obj*       resObj;      /* Result data for 'write' */
    int            written;

    /* The following check can be done before thread redirection,
     * because we are reading from an item which is readonly, i.e.
     * will never change during the lifetime of the channel.
     */

    if (!(rcPtr->methods & FLAG (METH_WRITE))) {
        RcSetChannelErrorStr (rcPtr->chan, msg_write_unsup);
        *errorCodePtr = EINVAL;
	return -1;
    }

#ifdef TCL_THREADS
    /* Are we in the correct thread ?
     */

    if (rcPtr->thread != Tcl_GetCurrentThread ()) {
      RcForwardParamOutput p;

      p.buf     = buf;
      p.toWrite = toWrite;

      RcForwardOp (rcPtr, RcOpOutput, rcPtr->thread, &p);

      if (p.b.code != TCL_OK) {
	  RcPassReceivedError (rcPtr->chan, p.b);
	  *errorCodePtr = EINVAL;
      } else {
	  *errorCodePtr = EOK;
      }

      return p.toWrite;
    }
#endif

    /* -------- */

    /* ASSERT: rcPtr->method & FLAG (METH_WRITE) */
    /* ASSERT: rcPtr->mode & TCL_WRITABLE */
    
    bufObj = Tcl_NewByteArrayObj((unsigned char*) buf, toWrite);
    if (bufObj == (Tcl_Obj*) NULL) {
        Tcl_Panic ("Out of memory in RcOutput");
    }

    RcInvokeTclMethod (rcPtr, "write", bufObj, NULL,
		       &res, &resObj, DO_CAPTURE);

    if (res != TCL_OK) {
	Tcl_SetChannelError (rcPtr->chan, resObj);
	Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	*errorCodePtr = EINVAL;
	return -1;
    }

    res = Tcl_GetIntFromObj (rcPtr->interp, resObj, &written);
    if (res != TCL_OK) {
        Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	Tcl_SetChannelError (rcPtr->chan, RcErrorMarshall (rcPtr->interp));
	*errorCodePtr = EINVAL;
	return -1;
    }

    Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */

    if ((written == 0) || (toWrite < written)) {
        /* The handler claims to have written more than it was given.
	 * That is bad. Note that the I/O core would crash if we were
	 * to return this information, trying to write -nnn bytes in
	 * the next iteration.
	 */

        RcSetChannelErrorStr (rcPtr->chan, msg_write_toomuch);
	*errorCodePtr = EINVAL;
	return -1;
    }

    *errorCodePtr = EOK;
    return written;
}

/*
 *----------------------------------------------------------------------
 *
 * RcSeekWide / RcSeek --
 *
 *	This function is invoked when the user wishes to seek on
 *	the channel.
 *
 * Results:
 *	The new location of the access point.
 *
 * Side effects:
 *	Allocates memory. Arbitrary, as it calls upon a script.
 *
 *----------------------------------------------------------------------
 */

static Tcl_WideInt
RcSeekWide (clientData, offset, seekMode, errorCodePtr)
     ClientData  clientData;
     Tcl_WideInt offset;
     int         seekMode;
     int*        errorCodePtr;
{
    ReflectingChannel* rcPtr = (ReflectingChannel*) clientData;
    Tcl_Obj*       offObj;
    Tcl_Obj*       baseObj;
    int            res;         /* Result code for 'seek' */
    Tcl_Obj*       resObj;      /* Result data for 'seek' */
    Tcl_WideInt    newLoc;

#ifdef TCL_THREADS
    /* Are we in the correct thread ?
     */

    if (rcPtr->thread != Tcl_GetCurrentThread ()) {
        RcForwardParamSeek p;

	p.seekMode = seekMode;
	p.offset   = offset;

	RcForwardOp (rcPtr, RcOpSeek, rcPtr->thread, &p);

	if (p.b.code != TCL_OK) {
	    RcPassReceivedError (rcPtr->chan, p.b);
	    *errorCodePtr = EINVAL;
	} else {
	    *errorCodePtr = EOK;
	}

	return p.offset;
    }
#endif

    /* -------- */

    /* ASSERT: rcPtr->method & FLAG (METH_SEEK) */

    offObj = Tcl_NewWideIntObj(offset);
    if (offObj == (Tcl_Obj*) NULL) {
        Tcl_Panic ("Out of memory in RcSeekWide");
    }

    baseObj = Tcl_NewStringObj((seekMode == SEEK_SET) ?
			       "start" :
			       ((seekMode == SEEK_CUR) ?
				"current" :
				"end"), -1);

    if (baseObj == (Tcl_Obj*) NULL) {
        Tcl_Panic ("Out of memory in RcSeekWide");
    }

    RcInvokeTclMethod (rcPtr, "seek", offObj, baseObj,
		       &res, &resObj, DO_CAPTURE);

    if (res != TCL_OK) {
	Tcl_SetChannelError (rcPtr->chan, resObj);
        Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	*errorCodePtr = EINVAL;
	return -1;
    }

    res = Tcl_GetWideIntFromObj (rcPtr->interp, resObj, &newLoc);
    if (res != TCL_OK) {
        Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	Tcl_SetChannelError (rcPtr->chan, RcErrorMarshall (rcPtr->interp));
	*errorCodePtr = EINVAL;
	return -1;
    }

    Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */

    if (newLoc < Tcl_LongAsWide (0)) {
        RcSetChannelErrorStr (rcPtr->chan, msg_seek_beforestart);
        *errorCodePtr = EINVAL;
	return -1;
    }

    *errorCodePtr = EOK;
    return newLoc;
}

static int
RcSeek (clientData, offset, seekMode, errorCodePtr)
     ClientData  clientData;
     long        offset;
     int         seekMode;
     int*        errorCodePtr;
{
  /* This function can be invoked from a transformation which is based
   * on standard seeking, i.e. non-wide. Because o this we have to
   * implement it, a dummy is not enough. We simply delegate the call
   * to the wide routine.
   */

  return (int) RcSeekWide (clientData, Tcl_LongAsWide (offset),
			   seekMode, errorCodePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * RcWatch --
 *
 *	This function is invoked to tell the channel what events
 *	the I/O system is interested in.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocates memory. Arbitrary, as it calls upon a script.
 *
 *----------------------------------------------------------------------
 */

static void
RcWatch (clientData, mask)
     ClientData clientData;
     int mask;
{
    ReflectingChannel* rcPtr = (ReflectingChannel*) clientData;
    Tcl_Obj* maskObj;

    /* ASSERT rcPtr->methods & FLAG (METH_WATCH) */

    /* We restrict the interest to what the channel can support
     * IOW there will never be write events for a channel which is
     * not writable. Analoguous for read events.
     */

    mask = mask & rcPtr->mode; 

    if (mask == rcPtr->interest) {
        /* Same old, same old, why should we do something ? */
      return;
    }

    rcPtr->interest = mask;

#ifdef TCL_THREADS
    /* Are we in the correct thread ?
     */

    if (rcPtr->thread != Tcl_GetCurrentThread ()) {
        RcForwardParamWatch p;

	p.mask = mask;

	RcForwardOp (rcPtr, RcOpWatch, rcPtr->thread, &p);

	/* Any failure from the forward is ignored. We have no place to
	 * put this.
	 */
	return;
    }
#endif

    /* -------- */

    maskObj = RcDecodeEventMask (mask);
    RcInvokeTclMethod (rcPtr, "watch", maskObj, NULL,
		       NULL, NULL, NO_CAPTURE);
    Tcl_DecrRefCount (maskObj);
}

/*
 *----------------------------------------------------------------------
 *
 * RcBlock --
 *
 *	This function is invoked to tell the channel which blocking
 *	behaviour is required of it.
 *
 * Results:
 *	A posix error number.
 *
 * Side effects:
 *	Allocates memory. Arbitrary, as it calls upon a script.
 *
 *----------------------------------------------------------------------
 */

static int
RcBlock (clientData, nonblocking)
     ClientData clientData;
     int nonblocking;
{
    ReflectingChannel* rcPtr = (ReflectingChannel*) clientData;
    Tcl_Obj*           blockObj;
    int                res;         /* Result code for 'blocking' */
    Tcl_Obj*           resObj;      /* Result data for 'blocking' */

#ifdef TCL_THREADS
    /* Are we in the correct thread ?
     */

    if (rcPtr->thread != Tcl_GetCurrentThread ()) {
        RcForwardParamBlock p;

	p.nonblocking = nonblocking;

	RcForwardOp (rcPtr, RcOpBlock, rcPtr->thread, &p);

	if (p.b.code != TCL_OK) {
	    RcPassReceivedError (rcPtr->chan, p.b);
	    return EINVAL;
	} else {
	    return EOK;
	}
    }
#endif

    /* -------- */

    blockObj = Tcl_NewBooleanObj(!nonblocking);
    if (blockObj == (Tcl_Obj*) NULL) {
        Tcl_Panic ("Out of memory in RcBlock");
    }

    RcInvokeTclMethod (rcPtr, "blocking", blockObj, NULL,
		       &res, &resObj, DO_CAPTURE);

    if (res != TCL_OK) {
	Tcl_SetChannelError (rcPtr->chan, resObj);
	res = EINVAL;
    } else {
        res = EOK;
    }

    Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
    return res;
}

/*
 *----------------------------------------------------------------------
 *
 * RcSetOption --
 *
 *	This function is invoked to configure a channel option.
 *
 * Results:
 *	A standard Tcl result code.
 *
 * Side effects:
 *	Arbitrary, as it calls upon a Tcl script.
 *
 *----------------------------------------------------------------------
 */

static int
RcSetOption (clientData, interp, optionName, newValue)
     ClientData   clientData;	/* Channel to query */
     Tcl_Interp   *interp;	/* Interpreter to leave error messages in */
     CONST char *optionName;	/* Name of requested option */
     CONST char *newValue;	/* The new value */
{
    ReflectingChannel* rcPtr = (ReflectingChannel*) clientData;
    Tcl_Obj*           optionObj;
    Tcl_Obj*           valueObj;
    int                res;         /* Result code for 'configure' */
    Tcl_Obj*           resObj;      /* Result data for 'configure' */

#ifdef TCL_THREADS
    /* Are we in the correct thread ?
     */

    if (rcPtr->thread != Tcl_GetCurrentThread ()) {
        RcForwardParamSetOpt p;

	p.name  = optionName;
	p.value = newValue;

	RcForwardOp (rcPtr, RcOpSetOpt, rcPtr->thread, &p);

	if (p.b.code != TCL_OK) {
	    Tcl_Obj* err = Tcl_NewStringObj (p.b.msg, -1);

	    RcErrorReturn (interp, err);

	    Tcl_DecrRefCount (err);
	    if (p.b.vol) {ckfree (p.b.msg);}
	}

	return p.b.code;
    }
#endif

  /* -------- */

  optionObj = Tcl_NewStringObj(optionName,-1);
  if (optionObj == (Tcl_Obj*) NULL) {
    Tcl_Panic ("Out of memory in RcSetOption");
  }

  valueObj = Tcl_NewStringObj(newValue,-1);
  if (valueObj == (Tcl_Obj*) NULL) {
    Tcl_Panic ("Out of memory in RcSetOption");
  }

  RcInvokeTclMethod (rcPtr, "configure", optionObj, valueObj,
		     &res, &resObj, DO_CAPTURE);

  if (res != TCL_OK) {
    RcErrorReturn (interp, resObj);
  }

  Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
  return res;
}

/*
 *----------------------------------------------------------------------
 *
 * RcGetOption --
 *
 *	This function is invoked to retrieve all or a channel option.
 *
 * Results:
 *	A standard Tcl result code.
 *
 * Side effects:
 *	Arbitrary, as it calls upon a Tcl script.
 *
 *----------------------------------------------------------------------
 */

static int
RcGetOption (clientData, interp, optionName, dsPtr)
     ClientData   clientData;	/* Channel to query */
     Tcl_Interp*  interp;	/* Interpreter to leave error messages in */
     CONST char* optionName;	/* Name of reuqested option */
     Tcl_DString* dsPtr;	/* String to place the result into */
{
    /* This code is special. It has regular passing of Tcl result, and
     * errors. The bypass functions are not required.
     */

    ReflectingChannel* rcPtr = (ReflectingChannel*) clientData;
    Tcl_Obj*           optionObj;
    int                res;         /* Result code for 'configure' */
    Tcl_Obj*           resObj;      /* Result data for 'configure' */
    int                listc;
    Tcl_Obj**          listv;
    const char*        method;

#ifdef TCL_THREADS
    /* Are we in the correct thread ?
     */

    if (rcPtr->thread != Tcl_GetCurrentThread ()) {
        int opcode;
        RcForwardParamGetOpt p;

	p.name  = optionName;
	p.value = dsPtr;

	if (optionName == (char*) NULL) {
	    opcode = RcOpGetOptAll;
	} else {
	    opcode = RcOpGetOpt;
	}

	RcForwardOp (rcPtr, opcode, rcPtr->thread, &p);

	if (p.b.code != TCL_OK) {
	    Tcl_Obj* err = Tcl_NewStringObj (p.b.msg, -1);

	    RcErrorReturn (interp, err);

	    Tcl_DecrRefCount (err);
	    if (p.b.vol) {ckfree (p.b.msg);}
	}

	return p.b.code;
    }
#endif

    /* -------- */

    if (optionName == (char*) NULL) {
        /* Retrieve all options. */
        method    = "cgetall";
	optionObj = NULL;
    } else {
        /* Retrieve the value of one option */
      
        method    = "cget";
	optionObj = Tcl_NewStringObj(optionName,-1);
	if (optionObj == (Tcl_Obj*) NULL) {
	    Tcl_Panic ("Out of memory in RcGetOption");
	}
    }

    RcInvokeTclMethod (rcPtr, method, optionObj, NULL,
			 &res, &resObj, DO_CAPTURE);

    if (res != TCL_OK) {
        RcErrorReturn (interp, resObj);
	Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	return res;
    }

    /* The result has to go into the 'dsPtr' for propagation to the
     * caller of the driver.
     */

    if (optionObj != NULL) {
        Tcl_DStringAppend (dsPtr, Tcl_GetString (resObj), -1);
	Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	return res;
    }

    /* Extract the list and append each item as element.
     */

    /* NOTE (4): If we extract the string rep we can assume a
     * NOTE (4): properly quoted string. Together with a separating
     * NOTE (4): space this way of simply appending the whole string
     * NOTE (4): rep might be faster. It also doesn't check if the
     * NOTE (4): result is a valid list. Nor that the list has an
     * NOTE (4): even number elements.
     * NOTE (4): ---
     */

    res = Tcl_ListObjGetElements (interp, resObj, &listc, &listv);

    if (res != TCL_OK) {
        Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	return res;
    }

    if ((listc % 2) == 1) {
        /* Odd number of elements is wrong.
	 */
	Tcl_Obj *objPtr = Tcl_NewObj();
	Tcl_ResetResult(interp);
	TclObjPrintf(NULL, objPtr, "Expected list with even number of "
		"elements, got %d element%s instead", listc, 
		(listc == 1 ? "" : "s"));
	Tcl_SetObjResult(interp, objPtr);
	Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
	return TCL_ERROR;
    }


    {
        int len;
	char* str = Tcl_GetStringFromObj (resObj, &len);

	if (len) {
	    Tcl_DStringAppend (dsPtr, " ", 1);
	    Tcl_DStringAppend (dsPtr, str, len);
	}
    }
    Tcl_DecrRefCount (resObj); /* Remove reference we held from the invoke */
    return res;
}

/*
 * Helpers. =========================================================
 */

/*
 *----------------------------------------------------------------------
 *
 * RcEncodeEventMask --
 *
 *	This function takes a list of event items and constructs the
 *	equivalent internal bitmask. The list has to contain at
 *	least one element. Elements are "read", "write", or any unique
 *	abbreviation thereof. Note that the bitmask is not changed if
 *	problems are encountered.
 *
 * Results:
 *	A standard Tcl error code. A bitmask where TCL_READABLE
 *	and/or TCL_WRITABLE can be set.
 *
 * Side effects:
 *	May shimmer 'obj' to a list representation. May place an
 *	error message into the interp result.
 *
 *----------------------------------------------------------------------
 */

static int
RcEncodeEventMask (interp, objName, obj, mask)
     Tcl_Interp* interp;
     CONST char* objName;
     Tcl_Obj*    obj;
     int*        mask;
{
    int        events;	/* Mask of events to post */
    int        listc;     /* #elements in eventspec list */
    Tcl_Obj**  listv;     /* Elements of eventspec list */
    int        evIndex;   /* Id of event for an element of the
			 * eventspec list */

    if (Tcl_ListObjGetElements (interp, obj,
				&listc, &listv) != TCL_OK) {
        return TCL_ERROR;
    }

    if (listc < 1) {
        Tcl_AppendResult(interp, "bad ", objName, " list: is empty",
			 (char *) NULL);
	return TCL_ERROR;
    }

    events = 0;
    while (listc > 0) {
        if (Tcl_GetIndexFromObj (interp, listv [listc-1],
				 eventOptions, objName, 0, &evIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (evIndex) {
	    case EVENT_READ:  events |= TCL_READABLE; break;
	    case EVENT_WRITE: events |= TCL_WRITABLE; break;
	}
	listc --;
    }

    *mask = events;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RcDecodeEventMask --
 *
 *	This function takes an internal bitmask of events and
 *	constructs the equivalent list of event items.
 *
 * Results:
 *	A Tcl_Obj reference. The object will have a refCount of
 *	one. The user has to decrement it to release the object.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
RcDecodeEventMask (mask)
     int mask;
{
    Tcl_Obj* evObj = Tcl_NewStringObj (((mask & RANDW) == RANDW) ?
				       "read write" :
				       ((mask & TCL_READABLE) ?
					"read" :
					((mask & TCL_WRITABLE) ?
					 "write" : "")), -1);
    if (evObj == (Tcl_Obj*) NULL) {
        Tcl_Panic ("Out of memory in RcDecodeEventMask");
    }

    Tcl_IncrRefCount (evObj);
    return evObj;
}

/*
 *----------------------------------------------------------------------
 *
 * RcNew --
 *
 *	This function is invoked to allocate and initialize the
 *	instance data of a new reflected channel.
 *
 * Results:
 *	A heap-allocated channel instance.
 *
 * Side effects:
 *	Allocates memory.
 *
 *----------------------------------------------------------------------
 */

static ReflectingChannel*
RcNew (interp, cmdpfxObj, mode, id)
       Tcl_Interp* interp;
       Tcl_Obj*    cmdpfxObj;
       int         mode;
       Tcl_Obj*    id;
{
    ReflectingChannel* rcPtr;
    int                listc;
    Tcl_Obj**          listv;
    Tcl_Obj*           word;
    int                i;

    rcPtr = (ReflectingChannel*) ckalloc (sizeof(ReflectingChannel));

    /* rcPtr->chan    : Assigned by caller. Dummy data here. */
    /* rcPtr->methods : Assigned by caller. Dummy data here. */

    rcPtr->chan     = (Tcl_Channel) NULL;
    rcPtr->methods  = 0;
    rcPtr->interp   = interp;
#ifdef TCL_THREADS
    rcPtr->thread   = Tcl_GetCurrentThread ();
#endif
    rcPtr->mode     = mode;
    rcPtr->interest = 0; /* Initially no interest registered */

    /* Method placeholder */

    /* ASSERT: cmdpfxObj is a Tcl List */

    Tcl_ListObjGetElements (interp, cmdpfxObj, &listc, &listv);

    /* See [==] as well.
     * Storage for the command prefix and the additional words required
     * for the invocation of methods in the command handler.
     *
     * listv [0] [listc-1] | [listc]  [listc+1] |
     * argv  [0]   ... [.] | [argc-2] [argc-1]  | [argc]  [argc+2]
     *       cmd   ... pfx | method   chan      | detail1 detail2
     */

    rcPtr->argc = listc + 2;
    rcPtr->argv = (Tcl_Obj**) ckalloc (sizeof (Tcl_Obj*) * (listc+4));

    for (i = 0; i < listc ; i++) {
        word = rcPtr->argv [i] = listv [i];
        Tcl_IncrRefCount (word);
    }

    i++; /* Skip placeholder for method */

    rcPtr->argv [i] = id ; Tcl_IncrRefCount (id);

    /* The next two objects are kept empty, varying arguments */

    /* Initialization complete */
    return rcPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RcNewHandle --
 *
 *	This function is invoked to generate a channel handle for
 *	a new reflected channel.
 *
 * Results:
 *	A Tcl_Obj containing the string of the new channel handle.
 *	The refcount of the returned object is -- zero --.
 *
 * Side effects:
 *	May allocate memory. Mutex protected critical section
 *	locks out other threads for a short time.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
RcNewHandle ()
{
    /* Count number of generated reflected channels.  Used for id
     * generation. Ids are never reclaimed and there is no dealing
     * with wrap around. On the other hand, "unsigned long" should be
     * big enough except for absolute longrunners (generate a 100 ids
     * per second => overflow will occur in 1 1/3 years).
     */

#ifdef TCL_THREADS
    TCL_DECLARE_MUTEX (rcCounterMutex)
#endif
    static unsigned long rcCounter = 0;

    Tcl_Obj* res = Tcl_NewObj ();

#ifdef TCL_THREADS
    Tcl_MutexLock (&rcCounterMutex);
#endif

    TclObjPrintf(NULL, res, "rc%lu", rcCounter);
    rcCounter ++;

#ifdef TCL_THREADS
    Tcl_MutexUnlock (&rcCounterMutex);
#endif

    return res;
}


static void
RcFree (rcPtr)
     ReflectingChannel* rcPtr;
{
    Channel* chanPtr = (Channel*) rcPtr->chan;
    int      i, n;

    if (chanPtr->typePtr != &tclRChannelType) {
        /* Delete a cloned ChannelType structure. */
        ckfree ((char*) chanPtr->typePtr);
    }

    n = rcPtr->argc - 2;
    for (i = 0; i < n; i++) {
        Tcl_DecrRefCount (rcPtr->argv[i]);
    }

    ckfree ((char*) rcPtr->argv);
    ckfree ((char*) rcPtr);
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * RcInvokeTclMethod --
 *
 *	This function is used to invoke the Tcl level of a reflected
 *	channel. It handles all the command assembly, invokation, and
 *	generic state and result mgmt.
 *
 * Results:
 *      Result code and data as returned by the method.
 *
 * Side effects:
 *	Arbitrary, as it calls upo na Tcl script.
 *
 *----------------------------------------------------------------------
 */

static void
RcInvokeTclMethod (rcPtr, method, argone, argtwo, result, resultObj, capture)
     ReflectingChannel* rcPtr;
     CONST char*        method;
     Tcl_Obj*           argone;    /* NULL'able */
     Tcl_Obj*           argtwo;    /* NULL'able */
     int*               result;    /* NULL'able */
     Tcl_Obj**          resultObj; /* NULL'able */
     int                capture;
{
    /* Thread redirection was done by higher layers */
    /* ASSERT: Tcl_GetCurrentThread () == rcPtr->thread */

    int             cmdc;           /* #words in constructed command */
    Tcl_Obj*        methObj = NULL; /* Method name in object form */
    Tcl_InterpState sr;             /* State of handler interp */
    int             res;            /* Result code of method invokation */
    Tcl_Obj*        resObj  = NULL; /* Result of method invokation. */

    /* NOTE (5): Decide impl. issue: Cache objects with method names ?
     * NOTE (5): Requires TSD data as reflections can be created in
     * NOTE (5): many different threads.
     * NOTE (5): ---
     */

    /* Insert method into the pre-allocated area, after the command
     * prefix, before the channel id.
     */

    methObj = Tcl_NewStringObj (method, -1);
    if (methObj == (Tcl_Obj*) NULL) {
        Tcl_Panic ("Out of memory in RcInvokeTclMethod");
    }
    Tcl_IncrRefCount (methObj);
    rcPtr->argv [rcPtr->argc - 2] = methObj;

    /* Append the additional argument containing method specific
     * details behind the channel id. If specified.
     */

    cmdc = rcPtr->argc ;
    if (argone) {
        Tcl_IncrRefCount (argone);
	rcPtr->argv [cmdc] = argone;
	cmdc++;
    }
    if (argtwo) {
        Tcl_IncrRefCount (argtwo);
	rcPtr->argv [cmdc] = argtwo;
	cmdc++;
    }

    /* And run the handler ...  This is done in auch a manner which
     * leaves any existing state intact.
     */

    sr  = Tcl_SaveInterpState (rcPtr->interp, 0 /* Dummy */);
    res = Tcl_EvalObjv        (rcPtr->interp, cmdc, rcPtr->argv, TCL_EVAL_GLOBAL);

    /* We do not try to extract the result information if the caller has no
     * interest in it. I.e. there is no need to put effort into creating
     * something which is discarded immediately after.
     */

    if (resultObj) {
	if ((res == TCL_OK) || !capture) {
	    /* Ok result taken as is, also if the caller requests that there
	     * is no capture.
	     */

	    resObj = Tcl_GetObjResult (rcPtr->interp);
	} else {
	    /* Non-ok ressult is always treated as an error.
	     * We have to capture the full state of the result,
	     * including additional options.
	     */

	    res    = TCL_ERROR;
	    resObj = RcErrorMarshall (rcPtr->interp);
	}
	Tcl_IncrRefCount(resObj);
    }
    Tcl_RestoreInterpState (rcPtr->interp, sr);

    /* ... */

    /* Cleanup of the dynamic parts of the command */

    Tcl_DecrRefCount (methObj);
    if (argone) {Tcl_DecrRefCount (argone);}
    if (argtwo) {Tcl_DecrRefCount (argtwo);}

    /* The resObj has a ref count of 1 at this location.  This means
     * that the caller of RcInvoke has to dispose of it (but only if
     * it was returned to it).
     */

    if (result) {
        *result = res;
    }
    if (resultObj) {
        *resultObj = resObj;
    }
    /* There no need to handle the case where nothing is returned, because for
     * that case resObj was not set anyway.
     */
}

#ifdef TCL_THREADS
static void
RcForwardOp (rcPtr, op, dst, param)
  ReflectingChannel* rcPtr; /* Channel instance */
  RcOperation        op;    /* Forwarded driver operation */
  Tcl_ThreadId       dst;   /* Destination thread */
  CONST VOID*        param; /* Arguments */
{
    RcForwardingEvent*  evPtr;
    RcForwardingResult* resultPtr;
    int                 result;

    /* Create and initialize the event and data structures */

    evPtr     = (RcForwardingEvent*)  ckalloc (sizeof (RcForwardingEvent));
    resultPtr = (RcForwardingResult*) ckalloc (sizeof (RcForwardingResult));

    evPtr->event.proc = RcForwardProc;
    evPtr->resultPtr  = resultPtr;
    evPtr->op         = op;
    evPtr->rcPtr      = rcPtr;
    evPtr->param      = param;

    resultPtr->src    = Tcl_GetCurrentThread ();
    resultPtr->dst    = dst;
    resultPtr->done   = (Tcl_Condition) NULL;
    resultPtr->result = -1;
    resultPtr->evPtr  = evPtr;

    /* Now execute the forward */

    Tcl_MutexLock(&rcForwardMutex);
    TclSpliceIn(resultPtr, forwardList);

    /*
     * Ensure cleanup of the event if any of the two involved threads
     * exits while this event is pending or in progress.
     */

    Tcl_CreateThreadExitHandler(RcSrcExitProc, (ClientData) evPtr);
    Tcl_CreateThreadExitHandler(RcDstExitProc, (ClientData) evPtr);

    /*
     * Queue the event and poke the other thread's notifier.
     */

    Tcl_ThreadQueueEvent(dst, (Tcl_Event*)evPtr, TCL_QUEUE_TAIL);
    Tcl_ThreadAlert(dst);

    /*
     * (*) Block until the other thread has either processed the transfer
     * or rejected it.
     */

    while (resultPtr->result < 0) {
        /* NOTE (1): Is it possible that the current thread goes away while waiting here ?
	 * NOTE (1): IOW Is it possible that "RcSrcExitProc" is called while we are here ?
	 * NOTE (1): See complementary note (2) in "RcSrcExitProc"
	 * NOTE (1): ---
	 */

        Tcl_ConditionWait(&resultPtr->done, &rcForwardMutex, NULL);
    }

    /*
     * Unlink result from the forwarder list.
     */

    TclSpliceOut(resultPtr, forwardList);

    resultPtr->nextPtr  = NULL;
    resultPtr->prevPtr  = NULL;

    Tcl_MutexUnlock(&rcForwardMutex);
    Tcl_ConditionFinalize(&resultPtr->done);

    /*
     * Kill the cleanup handlers now, and the result structure as well,
     * before returning the success code.
     *
     * Note: The event structure has already been deleted.
     */

    Tcl_DeleteThreadExitHandler(RcSrcExitProc, (ClientData) evPtr);
    Tcl_DeleteThreadExitHandler(RcDstExitProc, (ClientData) evPtr);
    
    result =  resultPtr->result;
    ckfree ((char*) resultPtr);
}

static int
RcForwardProc (evGPtr, mask)
     Tcl_Event *evGPtr; 
     int mask;
{
    /* Notes regarding access to the referenced data.
     *
     * In principle the data belongs to the originating thread (see
     * evPtr->src), however this thread is currently blocked at (*),
     * i.e. quiescent. Because of this we can treat the data as
     * belonging to us, without fear of race conditions. I.e. we can
     * read and write as we like.
     *
     * The only thing we cannot be sure of is the resultPtr. This can be
     * be NULLed if the originating thread went away while the event
     * is handled here now.
     */

    RcForwardingEvent*  evPtr     = (RcForwardingEvent*) evGPtr;
    RcForwardingResult* resultPtr = evPtr->resultPtr;
    ReflectingChannel*  rcPtr     = evPtr->rcPtr;
    Tcl_Interp*         interp    = rcPtr->interp;
    RcForwardParamBase* paramPtr  = (RcForwardParamBase*) evPtr->param;
    int                 res       = TCL_OK; /* Result code   of RcInvokeTclMethod */
    Tcl_Obj*            resObj    = NULL;   /* Interp result of RcInvokeTclMethod */

    /* Ignore the event if no one is waiting for its result anymore.
     */

    if (!resultPtr) {
        return 1;
    }

    paramPtr->code = TCL_OK;
    paramPtr->msg  = NULL;
    paramPtr->vol  = 0;

    switch (evPtr->op) {
      /* The destination thread for the following operations is
       * rcPtr->thread, which contains rcPtr->interp, the interp
       * we have to call upon for the driver.
       */

    case RcOpClose:
      {
	  /* No parameters/results */
	  RcInvokeTclMethod (rcPtr, "finalize", NULL, NULL,
			     &res, &resObj, DO_CAPTURE);

	  if (res != TCL_OK) {
	     RcForwardSetObjError (paramPtr, resObj);
	  }

	  /* Freeing is done here, in the origin thread, because the
	   * argv[] objects belong to this thread. Deallocating them
	   * in a different thread is not allowed
	   */

	  RcFree (rcPtr);
      }
      break;

    case RcOpInput:
      {
	  RcForwardParamInput* p = (RcForwardParamInput*) paramPtr;
	  Tcl_Obj*     toReadObj = Tcl_NewIntObj (p->toRead);

	  if (toReadObj == (Tcl_Obj*) NULL) {
	      Tcl_Panic ("Out of memory in RcInput");
	  }

	  RcInvokeTclMethod (rcPtr, "read", toReadObj, NULL,
			     &res, &resObj, DO_CAPTURE);

	  if (res != TCL_OK) {
	      RcForwardSetObjError (paramPtr, resObj);
	      p->toRead = -1;
	  } else {
	      /* Process a regular result. */

	      int            bytec; /* Number of returned bytes */
	      unsigned char* bytev; /* Array of returned bytes */

	      bytev = Tcl_GetByteArrayFromObj(resObj, &bytec);

	      if (p->toRead < bytec) {
		  RcForwardSetStaticError (paramPtr, msg_read_toomuch);
		  p->toRead   = -1;

	      } else {
	          if (bytec > 0) {
		      memcpy (p->buf, bytev, bytec);
		  }

		  p->toRead = bytec;
	      }
	  }
      }
      break;

    case RcOpOutput:
      {
	  RcForwardParamOutput* p = (RcForwardParamOutput*) paramPtr;
	  Tcl_Obj*         bufObj = Tcl_NewByteArrayObj((unsigned char*) p->buf, p->toWrite);

	  if (bufObj == (Tcl_Obj*) NULL) {
	      Tcl_Panic ("Out of memory in RcOutput");
	  }

	  RcInvokeTclMethod (rcPtr, "write", bufObj, NULL,
			     &res, &resObj, DO_CAPTURE);

	  if (res != TCL_OK) {
	      RcForwardSetObjError (paramPtr, resObj);
	      p->toWrite = -1;
	  } else {
	      /* Process a regular result. */

	      int written;

	      res = Tcl_GetIntFromObj (interp, resObj, &written);
	      if (res != TCL_OK) {

		  RcForwardSetObjError (paramPtr, RcErrorMarshall (interp));
		  p->toWrite = -1;

	      } else if ((written == 0) || (p->toWrite < written)) {

		  RcForwardSetStaticError (paramPtr, msg_write_toomuch);
		  p->toWrite = -1;

	      } else {
		  p->toWrite = written;
	      }
	  }
      }
      break;

    case RcOpSeek:
      {
	  RcForwardParamSeek* p = (RcForwardParamSeek*) paramPtr;

	  Tcl_Obj*       offObj;
	  Tcl_Obj*       baseObj;

	  offObj = Tcl_NewWideIntObj(p->offset);
	  if (offObj == (Tcl_Obj*) NULL) {
	      Tcl_Panic ("Out of memory in RcSeekWide");
	  }

	  baseObj = Tcl_NewStringObj((p->seekMode == SEEK_SET) ?
				     "start" :
				     ((p->seekMode == SEEK_CUR) ?
				      "current" :
				      "end"), -1);

	  if (baseObj == (Tcl_Obj*) NULL) {
	      Tcl_Panic ("Out of memory in RcSeekWide");
	  }

	  RcInvokeTclMethod (rcPtr, "seek", offObj, baseObj,
			     &res, &resObj, DO_CAPTURE);

	  if (res != TCL_OK) {
	      RcForwardSetObjError (paramPtr, resObj);
	      p->offset = -1;
	  } else {
	      /* Process a regular result. If the type is wrong this
	       * may change into an error.
	       */

	      Tcl_WideInt newLoc;
	      res = Tcl_GetWideIntFromObj (interp, resObj, &newLoc);

	      if (res == TCL_OK) {
		  if (newLoc < Tcl_LongAsWide (0)) {
		      RcForwardSetStaticError (paramPtr, msg_seek_beforestart);
		      p->offset = -1;
		  } else {
		      p->offset = newLoc;
		  }
	      } else {
		  RcForwardSetObjError (paramPtr, RcErrorMarshall (interp));
		  p->offset = -1;
	      }
	  }
      }
      break;

    case RcOpWatch:
      {
	  RcForwardParamWatch* p = (RcForwardParamWatch*) paramPtr;

	  Tcl_Obj* maskObj = RcDecodeEventMask (p->mask);
	  RcInvokeTclMethod (rcPtr, "watch", maskObj, NULL,
			     NULL, NULL, NO_CAPTURE);
	  Tcl_DecrRefCount (maskObj);
      }
    break;

    case RcOpBlock:
      {
	  RcForwardParamBlock* p = (RcForwardParamBlock*) evPtr->param;
	  Tcl_Obj*      blockObj = Tcl_NewBooleanObj(!p->nonblocking);

	  if (blockObj == (Tcl_Obj*) NULL) {
	      Tcl_Panic ("Out of memory in RcBlock");
	  }

	  RcInvokeTclMethod (rcPtr, "blocking", blockObj, NULL,
			     &res, &resObj, DO_CAPTURE);

	  if (res != TCL_OK) {
	      RcForwardSetObjError (paramPtr, resObj);
	  }
      }
      break;

    case RcOpSetOpt:
      {
	  RcForwardParamSetOpt* p = (RcForwardParamSetOpt*) paramPtr;
	  Tcl_Obj* optionObj;
	  Tcl_Obj* valueObj;

	  optionObj = Tcl_NewStringObj(p->name,-1);
	  if (optionObj == (Tcl_Obj*) NULL) {
	      Tcl_Panic ("Out of memory in RcSetOption");
	  }

	  valueObj = Tcl_NewStringObj(p->value,-1);
	  if (valueObj == (Tcl_Obj*) NULL) {
	      Tcl_Panic ("Out of memory in RcSetOption");
	  }

	  RcInvokeTclMethod (rcPtr, "configure", optionObj, valueObj,
			     &res, &resObj, DO_CAPTURE);

	  if (res != TCL_OK) {
	      RcForwardSetObjError (paramPtr, resObj);
	  }
      }
      break;

    case RcOpGetOpt:
      {
	  /* Retrieve the value of one option */

	  RcForwardParamGetOpt* p = (RcForwardParamGetOpt*) paramPtr;
	  Tcl_Obj*           optionObj;

	  optionObj = Tcl_NewStringObj(p->name,-1);
	  if (optionObj == (Tcl_Obj*) NULL) {
	      Tcl_Panic ("Out of memory in RcGetOption");
	  }

	  RcInvokeTclMethod (rcPtr, "cget", optionObj, NULL,
			     &res, &resObj, DO_CAPTURE);

	  if (res != TCL_OK) {
	      RcForwardSetObjError (paramPtr, resObj);
	  } else {
	      Tcl_DStringAppend (p->value, Tcl_GetString (resObj), -1);
	  }
      }
      break;

    case RcOpGetOptAll:
      {
	  /* Retrieve all options. */

	  RcForwardParamGetOpt* p = (RcForwardParamGetOpt*) paramPtr;

	  RcInvokeTclMethod (rcPtr, "cgetall", NULL, NULL,
			     &res, &resObj, DO_CAPTURE);

	  if (res != TCL_OK) {
	      RcForwardSetObjError (paramPtr, resObj);
	  } else {
	      /* Extract list, validate that it is a list, and
	       * #elements. See NOTE (4) as well.
	       */

	      int       listc;
	      Tcl_Obj** listv;

	      res = Tcl_ListObjGetElements (interp, resObj, &listc, &listv);
	      if (res != TCL_OK) {
		  RcForwardSetObjError (paramPtr, RcErrorMarshall (interp));

	      } else if ((listc % 2) == 1) {
	          /* Odd number of elements is wrong.
		   * [x].
		   */

	          char* buf = ckalloc (200);
		  sprintf (buf,
			   "{Expected list with even number of elements, got %d %s instead}",
			   listc,
			   (listc == 1 ? "element" : "elements"));
		  
		  RcForwardSetDynError (paramPtr, buf);
	      } else {
		  int len;
		  char* str = Tcl_GetStringFromObj (resObj, &len);

		  if (len) {
		      Tcl_DStringAppend (p->value, " ", 1);
		      Tcl_DStringAppend (p->value, str, len);
		  }
	      }
	  }
      }
      break;

    default:
      /* Bad operation code */
      Tcl_Panic ("Bad operation code in RcForwardProc");
      break;
    }

    /* Remove the reference we held on the result of the invoke, if we had
     * such
     */
    if (resObj != NULL) {
	Tcl_DecrRefCount (resObj);
    }

    if (resultPtr) {
        /*
	 * Report the forwarding result synchronously to the waiting
	 * caller. This unblocks (*) as well. This is wrapped into a
	 * conditional because the caller may have exited in the mean
	 * time.
	 */

        Tcl_MutexLock(&rcForwardMutex);
	resultPtr->result = TCL_OK;
	Tcl_ConditionNotify(&resultPtr->done);
	Tcl_MutexUnlock(&rcForwardMutex);
    }

    return 1;
}


static void
RcSrcExitProc (clientData)
     ClientData clientData;
{
  RcForwardingEvent*  evPtr = (RcForwardingEvent*) clientData;
  RcForwardingResult* resultPtr;
  RcForwardParamBase* paramPtr;

  /* NOTE (2): Can this handler be called with the originator blocked ?
   * NOTE (2): ---
   */

  /* The originator for the event exited. It is not sure if this
   * can happen, as the originator should be blocked at (*) while
   * the event is in transit/pending.
   */

  /*
   * We make sure that the event cannot refer to the result anymore,
   * remove it from the list of pending results and free the
   * structure. Locking the access ensures that we cannot get in
   * conflict with "RcForwardProc", should it already execute the
   * event.
   */

  Tcl_MutexLock(&rcForwardMutex);

  resultPtr = evPtr->resultPtr;
  paramPtr  = (RcForwardParamBase*) evPtr->param;

  evPtr->resultPtr  = NULL;
  resultPtr->evPtr  = NULL;
  resultPtr->result = TCL_ERROR;

  RcForwardSetStaticError (paramPtr, msg_send_originlost);

  /* See below: TclSpliceOut(resultPtr, forwardList); */

  Tcl_MutexUnlock(&rcForwardMutex);

  /*
   * This unlocks (*). The structure will be spliced out and freed by
   * "RcForwardProc". Maybe.
   */

  Tcl_ConditionNotify(&resultPtr->done);
}


static void
RcDstExitProc (clientData)
     ClientData clientData;
{
  RcForwardingEvent*  evPtr     = (RcForwardingEvent*) clientData;
  RcForwardingResult* resultPtr = evPtr->resultPtr;
  RcForwardParamBase* paramPtr  = (RcForwardParamBase*) evPtr->param;

  /* NOTE (3): It is not clear if the event still exists when this handler is called..
   * NOTE (3): We might have to use 'resultPtr' as our clientData instead.
   * NOTE (3): ---
   */

  /* The receiver for the event exited, before processing the
   * event. We detach the result now, wake the originator up
   * and signal failure.
   */

  evPtr->resultPtr = NULL;
  resultPtr->evPtr  = NULL;
  resultPtr->result = TCL_ERROR;

  RcForwardSetStaticError (paramPtr, msg_send_dstlost);

  Tcl_ConditionNotify(&resultPtr->done);
}


static void
RcForwardSetObjError (p,obj)
     RcForwardParamBase* p;
     Tcl_Obj*            obj;
{
    int   len;
    char* msg;

    msg = Tcl_GetStringFromObj (obj, &len);

    p->code = TCL_ERROR;
    p->vol  = 1;
    p->msg  = strcpy(ckalloc (1+len), msg);
}
#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
