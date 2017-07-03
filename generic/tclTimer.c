/*
 * tclTimer.c --
 *
 *	This file provides timer event management facilities for Tcl,
 *	including the "after" command.
 *
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * The data structure below is used by the "after" command to remember the
 * command to be executed later. All of the pending "after" commands for an
 * interpreter are linked together in a list.
 */

#define IDLE_EVENT  (1 << 1)	/* Mark idle event */

typedef struct AfterInfo {
    struct AfterAssocData *assocPtr;
				/* Pointer to the "tclAfter" assocData for the
				 * interp in which command will be
				 * executed. */
    Tcl_Obj *commandPtr;	/* Command to execute. */
    Tcl_Obj *selfPtr;		/* Points to the handle object (self) */
    size_t id;			/* Integer identifier for command */
    int			flags;	/* Flags (IDLE_EVENT) */
    struct AfterInfo *nextPtr;	/* Next in list of all "after" commands for
				 * this interpreter. */
    struct AfterInfo *prevPtr;	/* Prev in list of all "after" commands for
				 * this interpreter. */
} AfterInfo;

/*
 * One of the following structures is associated with each interpreter for
 * which an "after" command has ever been invoked. A pointer to this structure
 * is stored in the AssocData for the "tclAfter" key.
 */

typedef struct AfterAssocData {
    Tcl_Interp *interp;		/* The interpreter for which this data is
				 * registered. */
    AfterInfo *firstAfterPtr;	/* First in list of all "after" commands still
				 * pending for this interpreter, or NULL if
				 * none. */
    AfterInfo *lastAfterPtr;	/* Last in list of all "after" commands. */
} AfterAssocData;

/*
 * The timer and idle queues are per-thread because they are associated with
 * the notifier, which is also per-thread.
 *
 * All static variables used in this file are collected into a single instance
 * of the following structure. For multi-threaded implementations, there is
 * one instance of this structure for each thread.
 *
 * Notice that different structures with the same name appear in other files.
 * The structure defined below is used in this file only.
 */

typedef struct {
    TimerHandler *timerList;	/* First event in queue. */
    TimerHandler *lastTimerPtr;	/* Last event in queue. */
    size_t timerListEpoch;	/* Used for safe process of event queue (stop
    				 * the cycle after modifying of event queue) */
    int lastTimerId;		/* Timer identifier of most recently created
				 * timer. */
    int timerPending;		/* 1 if a timer event is in the queue. */
    IdleHandler *idleList;	/* First in list of all idle handlers. */
    IdleHandler *lastIdlePtr;	/* Last in list (or NULL for empty list). */
    size_t timerGeneration;	/* Used to fill in the "generation" fields of */
    size_t idleGeneration;	/* timer or idle structures. Increments each
				 * time we place a new handler to queue inside,
				 * a new loop, so that all old handlers can be
				 * called without calling any of the new ones
				 * created by old ones. */
    size_t afterId;		/* For unique identifiers of after events. */
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

/*
 * Helper macros to wrap AfterInfo and handlers (and vice versa)
 */

#define TimerHandler2AfterInfo(ptr)					\
	    ( (AfterInfo*)TimerHandler2ClientData(ptr) )
#define AfterInfo2TimerHandler(ptr)					\
	    ClientData2TimerHandler(ptr)

#define IdleHandler2AfterInfo(ptr)					\
	    ( (AfterInfo*)IdleHandler2ClientData(ptr) )
#define AfterInfo2IdleHandler(ptr)					\
	    ClientData2IdleHandler(ptr)

/*
 * Helper macros for working with times. TCL_TIME_BEFORE encodes how to write
 * the ordering relation on (normalized) times, and TCL_TIME_DIFF_MS computes
 * the number of milliseconds difference between two times. Both macros use
 * both of their arguments multiple times, so make sure they are cheap and
 * side-effect free. The "prototypes" for these macros are:
 *
 * static int	TCL_TIME_BEFORE(Tcl_Time t1, Tcl_Time t2);
 * static long	TCL_TIME_DIFF_MS(Tcl_Time t1, Tcl_Time t2);
 */

#define TCL_TIME_BEFORE(t1, t2) \
    (((t1).sec<(t2).sec) || ((t1).sec==(t2).sec && (t1).usec<(t2).usec))

#define TCL_TIME_DIFF_MS(t1, t2) \
    (1000*((Tcl_WideInt)(t1).sec - (Tcl_WideInt)(t2).sec) + \
	    ((long)(t1).usec - (long)(t2).usec)/1000)

/*
 * Prototypes for functions referenced only in this file:
 */

static void		AfterCleanupProc(ClientData clientData,
			    Tcl_Interp *interp);
static int		AfterDelay(Tcl_Interp *interp, Tcl_WideInt ms);
static void		AfterProc(ClientData clientData);
static void		FreeAfterPtr(ClientData clientData);
static AfterInfo *	GetAfterEvent(AfterAssocData *assocPtr, Tcl_Obj *objPtr);
static ThreadSpecificData *InitTimer(void);
static void		TimerExitProc(ClientData clientData);
static int		TimerHandlerEventProc(Tcl_Event *evPtr, int flags);
static void		TimerCheckProc(ClientData clientData, int flags);
static void		TimerSetupProc(ClientData clientData, int flags);

static void             AfterObj_DupInternalRep(Tcl_Obj *, Tcl_Obj *);
static void		AfterObj_FreeInternalRep(Tcl_Obj *);
static void		AfterObj_UpdateString(Tcl_Obj *);

static inline void
QueueTimerHandlerEvent()
{
    Tcl_Event *timerEvPtr;

    timerEvPtr = (Tcl_Event *) ckalloc(sizeof(Tcl_Event));
    timerEvPtr->proc = TimerHandlerEventProc;
    Tcl_QueueEvent(timerEvPtr, TCL_QUEUE_TAIL);
}

/*
 * Type definition.
 */

Tcl_ObjType afterObjType = {
    "after",                    /* name */
    AfterObj_FreeInternalRep,   /* freeIntRepProc */
    AfterObj_DupInternalRep,    /* dupIntRepProc */
    AfterObj_UpdateString,      /* updateStringProc */
    NULL                        /* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 */
static void
AfterObj_DupInternalRep(srcPtr, dupPtr)
    Tcl_Obj *srcPtr;
    Tcl_Obj *dupPtr;
{
    /* 
     * Because we should have only a single reference to the after event,
     * we'll copy string representation only.
     */
    if (dupPtr->bytes == NULL) {
	if (srcPtr->bytes == NULL) {
	    AfterObj_UpdateString(srcPtr);
	}
	if (srcPtr->bytes != tclEmptyStringRep) {
	    TclInitStringRep(dupPtr, srcPtr->bytes, srcPtr->length);
	} else {
	    dupPtr->bytes = tclEmptyStringRep;
	}
    }
}
/*
 *----------------------------------------------------------------------
 */
static void
AfterObj_FreeInternalRep(objPtr)
    Tcl_Obj *objPtr;
{
    /* 
     * Because we should always have a reference by active after event,
     * so it is a triggered / canceled event - just reset type and pointers
     */
    objPtr->internalRep.twoPtrValue.ptr1 = NULL;
    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    objPtr->typePtr = NULL;

    /* prevent no string representation bug */
    if (objPtr->bytes == NULL) {
	objPtr->length = 0;
	objPtr->bytes = tclEmptyStringRep;
    }
}
/*
 *----------------------------------------------------------------------
 */
static void
AfterObj_UpdateString(objPtr)
    Tcl_Obj  *objPtr;
{
    char buf[16 + TCL_INTEGER_SPACE];
    int len;
 
    AfterInfo *afterPtr = (AfterInfo*)objPtr->internalRep.twoPtrValue.ptr1;

    /* if already triggered / canceled - equivalent not found, we can use empty */
    if (!afterPtr) {
	objPtr->length = 0;
	objPtr->bytes = tclEmptyStringRep;
	return;
    }

    len = sprintf(buf, "after#%d", afterPtr->id);

    objPtr->length = len;
    objPtr->bytes = ckalloc((size_t)++len);
    if (objPtr->bytes)
	memcpy(objPtr->bytes, buf, len);

}
/*
 *----------------------------------------------------------------------
 */
Tcl_Obj*
GetAfterObj(
    AfterInfo *afterPtr)
{
    Tcl_Obj * objPtr = afterPtr->selfPtr;
    
    if (objPtr != NULL) {
	return objPtr;
    }
    
    TclNewObj(objPtr);
    objPtr->typePtr = &afterObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.twoPtrValue.ptr1 = afterPtr;
    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    Tcl_IncrRefCount(objPtr);
    afterPtr->selfPtr = objPtr;

    return objPtr;
};

/*
 *----------------------------------------------------------------------
 *
 * InitTimer --
 *
 *	This function initializes the timer module.
 *
 * Results:
 *	A pointer to the thread specific data.
 *
 * Side effects:
 *	Registers the idle and timer event sources.
 *
 *----------------------------------------------------------------------
 */

static ThreadSpecificData *
InitTimer(void)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    TclThreadDataKeyGet(&dataKey);

    if (tsdPtr == NULL) {
	tsdPtr = TCL_TSD_INIT(&dataKey);
	Tcl_CreateEventSource(TimerSetupProc, TimerCheckProc, NULL);
	Tcl_CreateThreadExitHandler(TimerExitProc, NULL);
    }
    return tsdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerExitProc --
 *
 *	This function is call at exit or unload time to remove the timer and
 *	idle event sources.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes the timer and idle event sources and remaining events.
 *
 *----------------------------------------------------------------------
 */

static void
TimerExitProc(
    ClientData clientData)	/* Not used. */
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    TclThreadDataKeyGet(&dataKey);

    Tcl_DeleteEventSource(TimerSetupProc, TimerCheckProc, NULL);
    if (tsdPtr != NULL) {
	register TimerHandler *timerHandlerPtr;

	while ((timerHandlerPtr = tsdPtr->timerList) != NULL) {
	    tsdPtr->timerList = timerHandlerPtr->nextPtr;
	    ckfree((char *) timerHandlerPtr);
	}
	tsdPtr->lastTimerPtr = NULL;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_CreateTimerHandler --
 *
 *	Arrange for a given function to be invoked at a particular time in the
 *	future.
 *
 * Results:
 *	The return value is a token for the timer event, which may be used to
 *	delete the event before it fires.
 *
 * Side effects:
 *	When milliseconds have elapsed, proc will be invoked exactly once.
 *
 *--------------------------------------------------------------
 */

Tcl_TimerToken
Tcl_CreateTimerHandler(
    int milliseconds,		/* How many milliseconds to wait before
				 * invoking proc. */
    Tcl_TimerProc *proc,	/* Function to invoke. */
    ClientData clientData)	/* Arbitrary data to pass to proc. */
{
    register TimerHandler *timerHandlerPtr;
    Tcl_Time time;

    /*
     * Compute when the event should fire.
     */

    Tcl_GetTime(&time);
    time.sec += milliseconds/1000;
    time.usec += (milliseconds%1000)*1000;
    if (time.usec >= 1000000) {
	time.usec -= 1000000;
	time.sec += 1;
    }

    timerHandlerPtr = TclCreateAbsoluteTimerHandlerEx(&time, proc, NULL, 0);
    if (timerHandlerPtr == NULL) {
    	return NULL;
    }
    timerHandlerPtr->clientData = clientData;

    return timerHandlerPtr->token;
}

/*
 *--------------------------------------------------------------
 *
 * TclCreateAbsoluteTimerHandlerEx -- , TclCreateAbsoluteTimerHandler --
 *
 *	Arrange for a given function to be invoked at a particular time in the
 *	future.
 *
 *	Specifying the timePtr as NULL ensures that timer event-handler will
 *	be queued immediately to guarantee the execution of timer-event as 
 *	soon as possible
 *
 * Results:
 *	The return value is a handler or token of the timer event, which may
 *	be used to delete the event before it fires.
 *
 * Side effects:
 *	When the time in timePtr has been reached, proc will be invoked
 *	exactly once.
 *
 *--------------------------------------------------------------
 */

TimerHandler*
TclCreateAbsoluteTimerHandlerEx(
    Tcl_Time *timePtr,			/* Time to be invoked */
    Tcl_TimerProc *proc,		/* Function to invoke */
    Tcl_TimerDeleteProc *deleteProc,	/* Function to cleanup */
    size_t extraDataSize)
{
    register TimerHandler *timerHandlerPtr, *thPtrPos;
    ThreadSpecificData *tsdPtr;

    tsdPtr = InitTimer();
    timerHandlerPtr = (TimerHandler *) ckalloc(sizeof(TimerHandler) + extraDataSize);
    if (timerHandlerPtr == NULL) {
	return NULL;
    }

    /*
     * Fill in fields for the event.
     */

    if (timePtr) {
	memcpy((void *)&timerHandlerPtr->time, (void *)timePtr,
		sizeof(timerHandlerPtr->time));
    } else {
	memset((void *)&timerHandlerPtr->time, 0,
		sizeof(timerHandlerPtr->time));
    }
    timerHandlerPtr->proc = proc;
    timerHandlerPtr->deleteProc = deleteProc;
    timerHandlerPtr->clientData = TimerHandler2ClientData(timerHandlerPtr);
    timerHandlerPtr->generation = tsdPtr->timerGeneration;
    tsdPtr->timerListEpoch++; /* signal-timer list was changed */
    tsdPtr->lastTimerId++;
    timerHandlerPtr->token = (Tcl_TimerToken) INT2PTR(tsdPtr->lastTimerId);

    /*
     * Add the event to the queue in the correct position
     * (ordered by event firing time).
     */

    /* if before current first (e. g. "after 0" before first "after 1000") */
    if ( !(thPtrPos = tsdPtr->timerList)
      || TCL_TIME_BEFORE(timerHandlerPtr->time, thPtrPos->time)
    ) {
    	/* splice to the head */
	TclSpliceInEx(timerHandlerPtr, 
		tsdPtr->timerList, tsdPtr->lastTimerPtr);
    } else {
    	/* search from end as long as one with time before not found */
	for (thPtrPos = tsdPtr->lastTimerPtr; thPtrPos != NULL;
	    thPtrPos = thPtrPos->prevPtr) {
	    if (!TCL_TIME_BEFORE(timerHandlerPtr->time, thPtrPos->time)) {
		break;
	    }
	}
	/* normally it should be always true, because checked above, but ... */
	if (thPtrPos != NULL) {
	    /* insert after found element (with time before new) */
	    timerHandlerPtr->prevPtr = thPtrPos;
	    if ((timerHandlerPtr->nextPtr = thPtrPos->nextPtr)) {
		thPtrPos->nextPtr->prevPtr = timerHandlerPtr;
	    } else {
		tsdPtr->lastTimerPtr = timerHandlerPtr;
	    }
	    thPtrPos->nextPtr = timerHandlerPtr;
	} else {
	    /* unexpected case, but ... splice to the head */
	    TclSpliceInEx(timerHandlerPtr, 
		tsdPtr->timerList, tsdPtr->lastTimerPtr);
	}
    }

    if (!timePtr) {
	/* execute immediately: queue handler event right now */
	if (!tsdPtr->timerPending) {
	    QueueTimerHandlerEvent();
	}
	tsdPtr->timerPending++;
    }

    return timerHandlerPtr;
}

Tcl_TimerToken
TclCreateAbsoluteTimerHandler(
    Tcl_Time *timePtr,
    Tcl_TimerProc *proc,
    ClientData clientData)
{
    register TimerHandler *timerHandlerPtr;

    timerHandlerPtr = TclCreateAbsoluteTimerHandlerEx(timePtr, proc, NULL, 0);
    if (timerHandlerPtr == NULL) {
    	return NULL;
    }
    timerHandlerPtr->clientData = clientData;

    return timerHandlerPtr->token;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_DeleteTimerHandler --
 *
 *	Delete a previously-registered timer handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroy the timer callback identified by TimerToken, so that its
 *	associated function will not be called. If the callback has already
 *	fired, or if the given token doesn't exist, then nothing happens.
 *
 *--------------------------------------------------------------
 */

void
Tcl_DeleteTimerHandler(
    Tcl_TimerToken token)	/* Result previously returned by
				 * Tcl_CreateTimerHandler. */
{
    register TimerHandler *timerHandlerPtr;
    ThreadSpecificData *tsdPtr = InitTimer();

    if (token == NULL) {
	return;
    }

    for (timerHandlerPtr = tsdPtr->lastTimerPtr;
	timerHandlerPtr != NULL;
	timerHandlerPtr = timerHandlerPtr->prevPtr
    ) {
	if (timerHandlerPtr->token != token) {
	    continue;
	}

	TclDeleteTimerHandler(timerHandlerPtr);
	return;
    }
}

void
TclDeleteTimerHandler(
    TimerHandler *timerHandlerPtr)	/* Result previously returned by */
					/* TclCreateAbsoluteTimerHandlerEx. */
{
    ThreadSpecificData *tsdPtr;

    if (timerHandlerPtr == NULL) {
	return;
    }

    tsdPtr = InitTimer();

    tsdPtr->timerListEpoch++; /* signal-timer list was changed */
    TclSpliceOutEx(timerHandlerPtr, tsdPtr->timerList, tsdPtr->lastTimerPtr);

    /* free it via deleteProc or ckfree */
    if (timerHandlerPtr->deleteProc) {
	(*timerHandlerPtr->deleteProc)(timerHandlerPtr->clientData);
    }

    ckfree((char *)timerHandlerPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerSetupProc --
 *
 *	This function is called by Tcl_DoOneEvent to setup the timer event
 *	source for before blocking. This routine checks both the idle and
 *	after timer lists.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May update the maximum notifier block time.
 *
 *----------------------------------------------------------------------
 */

static void
TimerSetupProc(
    ClientData data,		/* Not used. */
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    if (((flags & TCL_IDLE_EVENTS) && tsdPtr->idleList)
	    || ((flags & TCL_TIMER_EVENTS) && tsdPtr->timerPending)) {
	/*
	 * There is an idle handler or a pending timer event, so just poll.
	 */

	blockTime.sec = 0;
	blockTime.usec = 0;

    } else if ((flags & TCL_TIMER_EVENTS) && tsdPtr->timerList) {
	/*
	 * Compute the timeout for the next timer on the list.
	 */

	Tcl_GetTime(&blockTime);
	blockTime.sec = tsdPtr->timerList->time.sec - blockTime.sec;
	blockTime.usec = tsdPtr->timerList->time.usec -
		blockTime.usec;
	if (blockTime.usec < 0) {
	    blockTime.sec -= 1;
	    blockTime.usec += 1000000;
	}
	if (blockTime.sec < 0) {
	    blockTime.sec = 0;
	    blockTime.usec = 0;
	}
    } else {
	return;
    }

    Tcl_SetMaxBlockTime(&blockTime);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerCheckProc --
 *
 *	This function is called by Tcl_DoOneEvent to check the timer event
 *	source for events. This routine checks both the idle and after timer
 *	lists.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May queue an event and update the maximum notifier block time.
 *
 *----------------------------------------------------------------------
 */

static void
TimerCheckProc(
    ClientData data,		/* Not used. */
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    if ((flags & TCL_TIMER_EVENTS) && tsdPtr->timerList
      && !tsdPtr->timerPending
    ) {
	/*
	 * Compute the timeout for the next timer on the list.
	 */

	Tcl_GetTime(&blockTime);
	blockTime.sec = tsdPtr->timerList->time.sec - blockTime.sec;
	blockTime.usec = tsdPtr->timerList->time.usec -
		blockTime.usec;
	if (blockTime.usec < 0) {
	    blockTime.sec -= 1;
	    blockTime.usec += 1000000;
	}
	if (blockTime.sec < 0) {
	    blockTime.sec = 0;
	    blockTime.usec = 0;
	}

	/*
	 * If the first timer has expired, stick an event on the queue.
	 */

	if (blockTime.sec == 0 && blockTime.usec == 0) {
	    tsdPtr->timerPending = 1;
	    QueueTimerHandlerEvent();
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TimerHandlerEventProc --
 *
 *	This function is called by Tcl_ServiceEvent when a timer event reaches
 *	the front of the event queue. This function handles the event by
 *	invoking the callbacks for all timers that are ready.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed from
 *	the queue. Returns 0 if the event was not handled, meaning it should
 *	stay on the queue. The only time the event isn't handled is if the
 *	TCL_TIMER_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the timer handler callback functions do.
 *
 *----------------------------------------------------------------------
 */

static int
TimerHandlerEventProc(
    Tcl_Event *evPtr,		/* Event to service. */
    int flags)			/* Flags that indicate what events to handle,
				 * such as TCL_FILE_EVENTS. */
{
    TimerHandler *timerHandlerPtr, *nextPtr;
    Tcl_Time time;
    size_t currentGeneration, currentEpoch;
    ThreadSpecificData *tsdPtr = InitTimer();

    /*
     * Do nothing if timers aren't enabled. This leaves the event on the
     * queue, so we will get to it as soon as ServiceEvents() is called with
     * timers enabled.
     */

    if (!(flags & TCL_TIMER_EVENTS)) {
	return 0;
    }

    /*
     * The code below is trickier than it may look, for the following reasons:
     *
     * 1. New handlers can get added to the list while the current one is
     *	  being processed. If new ones get added, we don't want to process
     *	  them during this pass through the list to avoid starving other event
     *	  sources. This is implemented using check of the generation epoch.
     * 2. The handler can call Tcl_DoOneEvent, so we have to remove the
     *	  handler from the list before calling it. Otherwise an infinite loop
     *	  could result.
     * 3. Tcl_DeleteTimerHandler can be called to remove an element from the
     *	  list while a handler is executing, so the list could change
     *	  structure during the call.
     * 4. Because we only fetch the current time before entering the loop, the
     *	  only way a new timer will even be considered runnable is if its
     *	  expiration time is within the same millisecond as the current time.
     *	  This is fairly likely on Windows, since it has a course granularity
     *	  clock. Since timers are placed on the queue in time order with the
     *	  most recently created handler appearing after earlier ones with the
     *	  same expiration time, we don't have to worry about newer generation
     *	  timers appearing before later ones.
     */

    currentGeneration = tsdPtr->timerGeneration++;
    Tcl_GetTime(&time);
    for (timerHandlerPtr = tsdPtr->timerList; 
	 timerHandlerPtr != NULL;
	 timerHandlerPtr = nextPtr
    ) {
    	nextPtr = timerHandlerPtr->nextPtr;

	if (TCL_TIME_BEFORE(time, timerHandlerPtr->time)) {
	    break;
	}

	/*
	 * Bypass timers of newer generation.
	 */

	if (timerHandlerPtr->generation > currentGeneration) {
	    tsdPtr->timerPending++;
	    continue;
	}

	tsdPtr->timerListEpoch++; /* signal-timer list was changed */
	 
	/*
	 * Remove the handler from the queue before invoking it, to avoid
	 * potential reentrancy problems.
	 */

	TclSpliceOutEx(timerHandlerPtr, 
	    tsdPtr->timerList, tsdPtr->lastTimerPtr);

	currentEpoch = tsdPtr->timerListEpoch;

	/* invoke timer proc */
	(*timerHandlerPtr->proc)(timerHandlerPtr->clientData);
	/* free it via deleteProc or ckfree */
	if (timerHandlerPtr->deleteProc) {
	    (*timerHandlerPtr->deleteProc)(timerHandlerPtr->clientData);
	}

	ckfree((char *) timerHandlerPtr);

	/* be sure that timer-list was not changed inside the proc call */
	if (currentEpoch != tsdPtr->timerListEpoch) {
	    /* timer-list was changed - stop processing */
	    tsdPtr->timerPending++;
	    break;
	}
    }

    /* don't need to queue event again by pending timer events */
    if (tsdPtr->timerPending > 1) {
    	tsdPtr->timerPending = 1;
    	return 0; /* leave handler event in the queue */
    }

    /* Reset generation */
    if (!tsdPtr->timerList) {
    	tsdPtr->timerGeneration = 0;
    }

    /* Compute the next timeout (later via TimerSetupProc using the first timer). */
    tsdPtr->timerPending = 0;

    return 1; /* processing done, again later via TimerCheckProc */
}

/*
 *--------------------------------------------------------------
 *
 * TclCreateIdleHandlerEx --, Tcl_DoWhenIdle --
 *
 *	Arrange for proc to be invoked the next time the system is idle (i.e.,
 *	just before the next time that Tcl_DoOneEvent would have to wait for
 *	something to happen).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Proc will eventually be called, with clientData as argument. See the
 *	manual entry for details.
 *
 *--------------------------------------------------------------
 */

IdleHandler *
TclCreateIdleHandlerEx(
    Tcl_IdleProc *proc,			/* Function to invoke. */
    Tcl_IdleDeleteProc *deleteProc,	/* Function to cleanup */
    size_t extraDataSize)
{
    register IdleHandler *idlePtr;
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    idlePtr = (IdleHandler *) ckalloc(sizeof(IdleHandler) + extraDataSize);
    if (idlePtr == NULL) {
	return NULL;
    }
    idlePtr->proc = proc;
    idlePtr->deleteProc = deleteProc;
    idlePtr->clientData = IdleHandler2ClientData(idlePtr);
    idlePtr->generation = tsdPtr->idleGeneration;

    /* attach to the idle queue */
    TclSpliceTailEx(idlePtr, tsdPtr->idleList, tsdPtr->lastIdlePtr);

    /* reset next block time */
    blockTime.sec = 0;
    blockTime.usec = 0;
    Tcl_SetMaxBlockTime(&blockTime);

    return idlePtr;
}

void
Tcl_DoWhenIdle(
    Tcl_IdleProc *proc,		/* Function to invoke. */
    ClientData clientData)	/* Arbitrary value to pass to proc. */
{
    register IdleHandler *idlePtr = TclCreateIdleHandlerEx(proc, NULL, 0);

    if (idlePtr) {
    	idlePtr->clientData = clientData;
    }
}

/*
 *--------------------------------------------------------------
 *
 * TclDeleteIdleHandler --
 *
 *	Delete a previously-registered idle handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
TclDeleteIdleHandler(
    IdleHandler *idlePtr)	/* Result previously returned by */
				/* TclCreateIdleHandlerEx */
{
    ThreadSpecificData *tsdPtr;

    if (idlePtr == NULL) {
	return;
    }

    tsdPtr = InitTimer();

    /* detach entry from the owner list */
    TclSpliceOutEx(idlePtr, tsdPtr->idleList, tsdPtr->lastIdlePtr);

    /* free it via deleteProc and ckfree */
    if (idlePtr->deleteProc) {
	(*idlePtr->deleteProc)(idlePtr->clientData);
    }
    ckfree((char *)idlePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CancelIdleCall --
 *
 *	If there are any when-idle calls requested to a given function with
 *	given clientData, cancel all of them.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the proc/clientData combination were on the when-idle list, they
 *	are removed so that they will never be called.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_CancelIdleCall(
    Tcl_IdleProc *proc,		/* Function that was previously registered. */
    ClientData clientData)	/* Arbitrary value to pass to proc. */
{
    register IdleHandler *idlePtr, *nextPtr;
    ThreadSpecificData *tsdPtr = InitTimer();

    for (idlePtr = tsdPtr->idleList;
    	idlePtr != NULL;
	idlePtr = nextPtr
    ) {
    	nextPtr = idlePtr->nextPtr;
	if ((idlePtr->proc == proc)
		&& (idlePtr->clientData == clientData)) {
	    /* detach entry from the owner list */
	    TclSpliceOutEx(idlePtr, tsdPtr->idleList, tsdPtr->lastIdlePtr);

	    /* free it via deleteProc and ckfree */
	    if (idlePtr->deleteProc) {
		(*idlePtr->deleteProc)(idlePtr->clientData);
	    }
	    ckfree((char *) idlePtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclServiceIdle --
 *
 *	This function is invoked by the notifier when it becomes idle. It will
 *	invoke all idle handlers that are present at the time the call is
 *	invoked, but not those added during idle processing.
 *
 * Results:
 *	The return value is 1 if TclServiceIdle found something to do,
 *	otherwise return value is 0.
 *
 * Side effects:
 *	Invokes all pending idle handlers.
 *
 *----------------------------------------------------------------------
 */

int
TclServiceIdle(void)
{
    IdleHandler *idlePtr;
    size_t currentGeneration;
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    if (tsdPtr->idleList == NULL) {
	return 0;
    }

    currentGeneration = tsdPtr->idleGeneration++;

    /*
     * The code below is trickier than it may look, for the following reasons:
     *
     * 1. New handlers can get added to the list while the current one is
     *	  being processed. If new ones get added, we don't want to process
     *	  them during this pass through the list (want to check for other work
     *	  to do first). This is implemented using the generation number in the
     *	  handler: new handlers will have a different generation than any of
     *	  the ones currently on the list.
     * 2. The handler can call Tcl_DoOneEvent, so we have to remove the
     *	  handler from the list before calling it. Otherwise an infinite loop
     *	  could result.
     * 3. Tcl_CancelIdleCall can be called to remove an element from the list
     *	  while a handler is executing, so the list could change structure
     *	  during the call.
     */

    while ((idlePtr = tsdPtr->idleList) != NULL
	&& idlePtr->generation <= currentGeneration
    ) {
	/* detach entry from the owner's list */
	TclSpliceOutEx(idlePtr, tsdPtr->idleList, tsdPtr->lastIdlePtr);

	/* execute event */
	(*idlePtr->proc)(idlePtr->clientData);

	/* free it via deleteProc and ckfree */
	if (idlePtr->deleteProc) {
	    (*idlePtr->deleteProc)(idlePtr->clientData);
	}
	ckfree((char *) idlePtr);
    }
    if (tsdPtr->idleList) {
	blockTime.sec = 0;
	blockTime.usec = 0;
	Tcl_SetMaxBlockTime(&blockTime);
    }
    /* Reset generation */
    if (!tsdPtr->idleList) {
    	tsdPtr->idleGeneration = 0;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AfterObjCmd --
 *
 *	This function is invoked to process the "after" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_AfterObjCmd(
    ClientData clientData,	/* Unused */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[])	/* Argument objects. */
{
    Tcl_WideInt ms;		/* Number of milliseconds to wait */
    Tcl_Time wakeup;
    AfterInfo *afterPtr;
    AfterAssocData *assocPtr;
    int length;
    int index;
    static CONST char *afterSubCmds[] = {
	"cancel", "idle", "info", NULL
    };
    enum afterSubCmds {AFTER_CANCEL, AFTER_IDLE, AFTER_INFO};
    ThreadSpecificData *tsdPtr = InitTimer();

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    /*
     * Create the "after" information associated for this interpreter, if it
     * doesn't already exist.
     */

    assocPtr = Tcl_GetAssocData(interp, "tclAfter", NULL);
    if (assocPtr == NULL) {
	assocPtr = (AfterAssocData *) ckalloc(sizeof(AfterAssocData));
	assocPtr->interp = interp;
	assocPtr->firstAfterPtr = NULL;
	assocPtr->lastAfterPtr = NULL;
	Tcl_SetAssocData(interp, "tclAfter", AfterCleanupProc,
		(ClientData) assocPtr);
    }

    /*
     * First lets see if the command was passed a number as the first argument.
     */

    if (objv[1]->typePtr == &tclIntType
#ifndef NO_WIDE_TYPE
	|| objv[1]->typePtr == &tclWideIntType
#endif
	|| objv[1]->typePtr == &tclBignumType
	|| ( Tcl_GetIndexFromObj(NULL, objv[1], afterSubCmds, "", 0, 
				 &index) != TCL_OK )) {
	index = -1;
	if (Tcl_GetWideIntFromObj(NULL, objv[1], &ms) != TCL_OK) {
	    Tcl_AppendResult(interp, "bad argument \"",
			     Tcl_GetString(objv[1]),
			     "\": must be cancel, idle, info, or an integer",
			     NULL);
	    return TCL_ERROR;
	}
    }

    /* 
     * At this point, either index = -1 and ms contains the number of ms
     * to wait, or else index is the index of a subcommand.
     */

    switch (index) {
    case -1: {
    	Tcl_Time *timePtr = NULL;
    	TimerHandler *timerPtr;
	if (ms < 0) {
	    ms = 0;
	}
	if (objc == 2) {
	    return AfterDelay(interp, ms);
	}

	if (ms) {
	    timePtr = &wakeup;
	    Tcl_GetTime(&wakeup);
	    wakeup.sec += (long)(ms / 1000);
	    wakeup.usec += ((long)(ms % 1000)) * 1000;
	    if (wakeup.usec > 1000000) {
		wakeup.sec++;
		wakeup.usec -= 1000000;
	    }
	}

	timerPtr = TclCreateAbsoluteTimerHandlerEx(timePtr, AfterProc,
			FreeAfterPtr, sizeof(AfterInfo));
	if (timerPtr == NULL) { /* error handled in panic */
	    return TCL_ERROR;
	}
	afterPtr = TimerHandler2AfterInfo(timerPtr);

	/* attach to the list */
	afterPtr->assocPtr = assocPtr;
	TclSpliceTailEx(afterPtr, 
		assocPtr->firstAfterPtr, assocPtr->lastAfterPtr);
	afterPtr->flags = 0;
	afterPtr->selfPtr = NULL;

	if (objc == 3) {
	    afterPtr->commandPtr = objv[2];
	} else {
 	    afterPtr->commandPtr = Tcl_ConcatObj(objc-2, objv+2);
	}
	Tcl_IncrRefCount(afterPtr->commandPtr);

	/*
	 * The variable below is used to generate unique identifiers for after
	 * commands. This id can wrap around, which can potentially cause
	 * problems. However, there are not likely to be problems in practice,
	 * because after commands can only be requested to about a month in
	 * the future, and wrap-around is unlikely to occur in less than about
	 * 1-10 years. Thus it's unlikely that any old ids will still be
	 * around when wrap-around occurs.
	 */

	afterPtr->id = tsdPtr->afterId++;

	Tcl_SetObjResult(interp, GetAfterObj(afterPtr));
	return TCL_OK;
    }
    case AFTER_CANCEL: {
	Tcl_Obj *commandPtr;
	char *command, *tempCommand;
	int tempLength;

	if (objc < 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "id|command");
	    return TCL_ERROR;
	}

	afterPtr = NULL;
	if (objc == 3) {
	    commandPtr = objv[2];
	} else {
	    commandPtr = Tcl_ConcatObj(objc-2, objv+2);;
	}
	if (commandPtr->typePtr == &afterObjType) {
	    afterPtr = (AfterInfo*)commandPtr->internalRep.twoPtrValue.ptr1;
	} else {
	    command = Tcl_GetStringFromObj(commandPtr, &length);
	    for (afterPtr = assocPtr->lastAfterPtr;
		 afterPtr != NULL;
		 afterPtr = afterPtr->prevPtr
	    ) {
		tempCommand = Tcl_GetStringFromObj(afterPtr->commandPtr,
			&tempLength);
		if ((length == tempLength)
			&& (memcmp((void*) command, (void*) tempCommand,
				(unsigned) length) == 0)) {
		    break;
		}
	    }
	    if (afterPtr == NULL) {
		afterPtr = GetAfterEvent(assocPtr, commandPtr);
	    }
	    if (objc != 3) {
		Tcl_DecrRefCount(commandPtr);
	    }
	}
	if (afterPtr != NULL) {
	    if (!(afterPtr->flags & IDLE_EVENT)) {
		TclDeleteTimerHandler(AfterInfo2TimerHandler(afterPtr));
	    } else {
		TclDeleteIdleHandler(AfterInfo2IdleHandler(afterPtr));
	    }
	}
	break;
    }
    case AFTER_IDLE: {
    	IdleHandler *idlePtr;

	if (objc < 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "script script ...");
	    return TCL_ERROR;
	}

	idlePtr = TclCreateIdleHandlerEx(AfterProc,
			FreeAfterPtr, sizeof(AfterInfo));
	if (idlePtr == NULL) { /* error handled in panic */
	    return TCL_ERROR;
	}
	afterPtr = IdleHandler2AfterInfo(idlePtr);

	/* attach to the list */
	afterPtr->assocPtr = assocPtr;
	TclSpliceTailEx(afterPtr, 
		assocPtr->firstAfterPtr, assocPtr->lastAfterPtr);
	afterPtr->flags = IDLE_EVENT;
	afterPtr->selfPtr = NULL;

	if (objc == 3) {
	    afterPtr->commandPtr = objv[2];
	} else {
	    afterPtr->commandPtr = Tcl_ConcatObj(objc-2, objv+2);
	}
	Tcl_IncrRefCount(afterPtr->commandPtr);
	
	afterPtr->id = tsdPtr->afterId++;
	
	Tcl_SetObjResult(interp, GetAfterObj(afterPtr));

	return TCL_OK;
    };
    case AFTER_INFO: {
	Tcl_Obj *resultListPtr;

	if (objc == 2) {
	    /* return list of all after-events */
	    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);
	    for (afterPtr = assocPtr->lastAfterPtr;
		 afterPtr != NULL;
		 afterPtr = afterPtr->prevPtr
	    ) {
		if (assocPtr->interp != interp) {
		    continue;
		}
		
		Tcl_ListObjAppendElement(NULL, listPtr, GetAfterObj(afterPtr));
	    }

	    Tcl_SetObjResult(interp, listPtr);
	    return TCL_OK;
	}
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?id?");
	    return TCL_ERROR;
	}

	afterPtr = GetAfterEvent(assocPtr, objv[2]);
	if (afterPtr == NULL) {
	    Tcl_AppendResult(interp, "event \"", TclGetString(objv[2]),
		    "\" doesn't exist", NULL);
	    return TCL_ERROR;
	}
	resultListPtr = Tcl_NewObj();
	Tcl_ListObjAppendElement(interp, resultListPtr, afterPtr->commandPtr);
	Tcl_ListObjAppendElement(interp, resultListPtr, Tcl_NewStringObj(
 		(afterPtr->flags & IDLE_EVENT) ? "idle" : "timer", -1));
	Tcl_SetObjResult(interp, resultListPtr);
	break;
    }
    default:
	Tcl_Panic("Tcl_AfterObjCmd: bad subcommand index to afterSubCmds");
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AfterDelay --
 *
 *	Implements the blocking delay behaviour of [after $time]. Tricky
 *	because it has to take into account any time limit that has been set.
 *
 * Results:
 *	Standard Tcl result code (with error set if an error occurred due to a
 *	time limit being exceeded).
 *
 * Side effects:
 *	May adjust the time limit granularity marker.
 *
 *----------------------------------------------------------------------
 */

static int
AfterDelay(
    Tcl_Interp *interp,
    Tcl_WideInt ms)
{
    Interp *iPtr = (Interp *) interp;

    Tcl_Time endTime, now;
    Tcl_WideInt diff;

    Tcl_GetTime(&endTime);
    endTime.sec += (long)(ms/1000);
    endTime.usec += ((int)(ms%1000))*1000;
    if (endTime.usec >= 1000000) {
	endTime.sec++;
	endTime.usec -= 1000000;
    }

    do {
	Tcl_GetTime(&now);
	if (iPtr->limit.timeEvent != NULL
	    && TCL_TIME_BEFORE(iPtr->limit.time, now)) {
	    iPtr->limit.granularityTicker = 0;
	    if (Tcl_LimitCheck(interp) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (iPtr->limit.timeEvent == NULL
	    || TCL_TIME_BEFORE(endTime, iPtr->limit.time)) {
	    diff = TCL_TIME_DIFF_MS(endTime, now);
#ifndef TCL_WIDE_INT_IS_LONG
	    if (diff > LONG_MAX) {
		diff = LONG_MAX;
	    }
#endif
	    if (diff > 0) {
		Tcl_Sleep((long)diff);
	    }
	} else {
	    diff = TCL_TIME_DIFF_MS(iPtr->limit.time, now);
#ifndef TCL_WIDE_INT_IS_LONG
	    if (diff > LONG_MAX) {
		diff = LONG_MAX;
	    }
#endif
	    if (diff > 0) {
		Tcl_Sleep((long)diff);
	    }
	    if (Tcl_LimitCheck(interp) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    } while (TCL_TIME_BEFORE(now, endTime));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetAfterEvent --
 *
 *	This function parses an "after" id such as "after#4" and returns a
 *	pointer to the AfterInfo structure.
 *
 * Results:
 *	The return value is either a pointer to an AfterInfo structure, if one
 *	is found that corresponds to "cmdString" and is for interp, or NULL if
 *	no corresponding after event can be found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static AfterInfo *
GetAfterEvent(
    AfterAssocData *assocPtr,	/* Points to "after"-related information for
				 * this interpreter. */
    Tcl_Obj *objPtr)
{
    char *cmdString;		/* Textual identifier for after event, such as
				 * "after#6". */
    AfterInfo *afterPtr;
    int id;
    char *end;

    if (objPtr->typePtr == &afterObjType) {
	return (AfterInfo*)objPtr->internalRep.twoPtrValue.ptr1;
    }

    cmdString = TclGetString(objPtr);
    if (strncmp(cmdString, "after#", 6) != 0) {
	return NULL;
    }
    cmdString += 6;
    id = strtoul(cmdString, &end, 10);
    if ((end == cmdString) || (*end != 0)) {
	return NULL;
    }
    for (afterPtr = assocPtr->lastAfterPtr; afterPtr != NULL;
	    afterPtr = afterPtr->prevPtr) {
	if (afterPtr->id == id) {
	    return afterPtr;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * AfterProc --
 *
 *	Timer callback to execute commands registered with the "after"
 *	command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Executes whatever command was specified. If the command returns an
 *	error, then the command "bgerror" is invoked to process the error; if
 *	bgerror fails then information about the error is output on stderr.
 *
 *----------------------------------------------------------------------
 */

static void
AfterProc(
    ClientData clientData)	/* Describes command to execute. */
{
    AfterInfo *afterPtr = (AfterInfo *) clientData;
    AfterAssocData *assocPtr = afterPtr->assocPtr;
    int result;
    Tcl_Interp *interp;

    /*
     * First remove the callback from our list of callbacks; otherwise someone
     * could delete the callback while it's being executed, which could cause
     * a core dump.
     */

    /* remove delete proc from handler (we'll do cleanup here) */
    if (!(afterPtr->flags & IDLE_EVENT)) {
	AfterInfo2TimerHandler(afterPtr)->deleteProc = NULL;
    } else {
	AfterInfo2IdleHandler(afterPtr)->deleteProc = NULL;
    }

    /* release object (mark it was triggered) */
    if (afterPtr->selfPtr) {
	if (afterPtr->selfPtr->typePtr == &afterObjType) {
	    afterPtr->selfPtr->internalRep.twoPtrValue.ptr1 = NULL;
	}
	Tcl_DecrRefCount(afterPtr->selfPtr);
	afterPtr->selfPtr = NULL;
    }

    /* detach after-entry from the owner's list */
    TclSpliceOutEx(afterPtr, assocPtr->firstAfterPtr, assocPtr->lastAfterPtr);

    /*
     * Execute the callback.
     */

    interp = assocPtr->interp;
    Tcl_Preserve((ClientData) interp);
    result = Tcl_EvalObjEx(interp, afterPtr->commandPtr, TCL_EVAL_GLOBAL);
    if (result != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (\"after\" script)");
	TclBackgroundException(interp, result);
    }
    Tcl_Release((ClientData) interp);

    /*
     * Free the memory for the callback.
     */

    Tcl_DecrRefCount(afterPtr->commandPtr);

}

/*
 *----------------------------------------------------------------------
 *
 * FreeAfterPtr --
 *
 *	This function removes an "after" command from the list of those that
 *	are pending and frees its resources. This function does *not* cancel
 *	the timer handler; if that's needed, the caller must do it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The memory associated with afterPtr is not released (owned by handler).
 *
 *----------------------------------------------------------------------
 */

static void
FreeAfterPtr(
    ClientData clientData)		/* Command to be deleted. */
{
    AfterInfo *afterPtr = (AfterInfo *) clientData;
    AfterAssocData *assocPtr = afterPtr->assocPtr;

    /* release object (mark it was removed) */
    if (afterPtr->selfPtr) {
	if (afterPtr->selfPtr->typePtr == &afterObjType) {
	    afterPtr->selfPtr->internalRep.twoPtrValue.ptr1 = NULL;
	}
	Tcl_DecrRefCount(afterPtr->selfPtr);
	afterPtr->selfPtr = NULL;
    }

    /* detach after-entry from the owner's list */
    TclSpliceOutEx(afterPtr, assocPtr->firstAfterPtr, assocPtr->lastAfterPtr);

    /* free command of entry */
    Tcl_DecrRefCount(afterPtr->commandPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * AfterCleanupProc --
 *
 *	This function is invoked whenever an interpreter is deleted
 *	to cleanup the AssocData for "tclAfter".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	After commands are removed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
AfterCleanupProc(
    ClientData clientData,	/* Points to AfterAssocData for the
				 * interpreter. */
    Tcl_Interp *interp)		/* Interpreter that is being deleted. */
{
    AfterAssocData *assocPtr = (AfterAssocData *) clientData;
    AfterInfo *afterPtr;

    while ( (afterPtr = assocPtr->lastAfterPtr) ) {
	if (!(afterPtr->flags & IDLE_EVENT)) {
	    TclDeleteTimerHandler(AfterInfo2TimerHandler(afterPtr));
	} else {
	    TclDeleteIdleHandler(AfterInfo2IdleHandler(afterPtr));
	}
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
