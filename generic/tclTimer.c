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

typedef struct AfterInfo {
    struct AfterAssocData *assocPtr;
				/* Pointer to the "tclAfter" assocData for the
				 * interp in which command will be
				 * executed. */
    Tcl_Obj *commandPtr;	/* Command to execute. */
    Tcl_Obj *selfPtr;		/* Points to the handle object (self) */
    unsigned int id;		/* Integer identifier for command */
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
    Tcl_WideInt knownTime;	/* Last know time */
    size_t knownTimeJumpEpoch;	/* Epoch of the last time-jump */
    Tcl_WideInt relTimerBase;	/* Time base of the first known relative */
				/* timer, used to revert all events to the new
				 * base after possible time-jump (adjustment).*/
    TclTimerEntry *timerList;	/* First event in queue of timers. */
    TclTimerEntry *timerTail;	/* Last event in queue of timers. */
    TclTimerEntry *promptList;	/* First immediate event in queue. */
    TclTimerEntry *promptTail;	/* Last immediate event in queue. */
    size_t timerListEpoch;	/* Used for safe process of event queue (stop
    				 * the cycle after modifying of event queue) */
    int lastTimerId;		/* Timer identifier of most recently created
				 * timer event. */
    int timerPending;		/* 1 if a timer event is in the queue. */
    TclTimerEntry *idleList;	/* First in list of all idle handlers. */
    TclTimerEntry *idleTail;	/* Last in list (or NULL for empty list). */
    size_t timerGeneration;	/* Used to fill in the "generation" fields of */
    size_t idleGeneration;	/* timer or idle structures. Increments each
				 * time we place a new handler to queue inside,
				 * a new loop, so that all old handlers can be
				 * called without calling any of the new ones
				 * created by old ones. */
    unsigned int afterId;	/* For unique identifiers of after events. */
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

/*
 * Helper macros to wrap AfterInfo and handlers (and vice versa)
 */

#define TclpTimerEntry2AfterInfo(ptr)					\
	    ( (AfterInfo*)TclpTimerEntry2ClientData(ptr) )
#define TclpAfterInfo2TimerEntry(ptr)					\
	    TclpClientData2TimerEntry(ptr)

/*
 * Prototypes for functions referenced only in this file:
 */

static void		AfterCleanupProc(ClientData clientData,
			    Tcl_Interp *interp);
static int		AfterDelay(Tcl_Interp *interp, Tcl_WideInt usec,
			    int absolute);
static void		AfterProc(ClientData clientData);
static void		FreeAfterPtr(ClientData clientData);
static AfterInfo *	GetAfterEvent(AfterAssocData *assocPtr, Tcl_Obj *objPtr);
static ThreadSpecificData *InitTimer(void);
static void		TimerExitProc(ClientData clientData);
static void		TimerCheckProc(ClientData clientData, int flags);
static void		TimerSetupProc(ClientData clientData, int flags);

static void             AfterObj_DupInternalRep(Tcl_Obj *, Tcl_Obj *);
static void		AfterObj_FreeInternalRep(Tcl_Obj *);
static void		AfterObj_UpdateString(Tcl_Obj *);

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

    len = sprintf(buf, "after#%u", afterPtr->id);

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
	Tcl_CreateEventSource(TimerSetupProc, TimerCheckProc, tsdPtr);
	Tcl_CreateThreadExitHandler(TimerExitProc, NULL);
    }
    return tsdPtr;
}

static void
AttachTimerEntry(
    ThreadSpecificData *tsdPtr,
    TclTimerEntry *entryPtr)
{
    if (entryPtr->flags & TCL_PROMPT_EVENT) {
	/* use timer generation, because usually no differences between 
	 * call of "after 0" and "after 1" */
	entryPtr->generation = tsdPtr->timerGeneration;
	/* attach to the prompt queue */
	TclSpliceTailEx(entryPtr, tsdPtr->promptList, tsdPtr->promptTail);
	
	/* execute immediately: signal pending and set timer marker */
	tsdPtr->timerPending++;
	TclSetTimerEventMarker(0);
    } else {
	/* idle generation */
	entryPtr->generation = tsdPtr->idleGeneration;
	/* attach to the idle queue */
	TclSpliceTailEx(entryPtr, tsdPtr->idleList, tsdPtr->idleTail);
    }
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

    if (tsdPtr != NULL) {
	Tcl_DeleteEventSource(TimerSetupProc, TimerCheckProc, tsdPtr);

	while ((tsdPtr->promptTail) != NULL) {
	    TclpDeleteTimerEntry(tsdPtr->promptTail);
	}
	while ((tsdPtr->timerTail) != NULL) {
	    TclpDeleteTimerEntry(tsdPtr->timerTail);
	}
	while ((tsdPtr->idleTail) != NULL) {
	    TclpDeleteTimerEntry(tsdPtr->idleTail);
	}
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
    register TclTimerEntry *entryPtr;
    Tcl_WideInt usec;

    /*
     * Compute when the event should fire (avoid overflow).
     */

    if (milliseconds < 0x7FFFFFFFFFFFFFFFL / 1000) {
	usec = (Tcl_WideInt)milliseconds*1000;
    } else {
	usec = 0x7FFFFFFFFFFFFFFFL;
    }

    entryPtr = TclpCreateTimerHandlerEx(usec, proc, NULL, 0, 0);
    if (entryPtr == NULL) {
	return NULL;
    }
    entryPtr->clientData = clientData;

    return TclpTimerEntry2TimerHandler(entryPtr)->token;
}

/*
 *--------------------------------------------------------------
 *
 * TclpCreateTimerHandlerEx --
 *
 *	Arrange for a given function to be invoked at or in a particular time
 *	in the future (microseconds).
 *
 * Results:
 *	The return value is a handler entry of the timer event, which may be
 *	used to access the event entry, e. g. delete the event before it fires.
 *
 * Side effects:
 *	When the time or offset in timePtr has been reached, proc will be invoked
 *	exactly once.
 *
 *--------------------------------------------------------------
 */

TclTimerEntry*
TclpCreateTimerHandlerEx(
    Tcl_WideInt usec,		/* Time to be invoked (absolute/relative) */
    Tcl_TimerProc *proc,	/* Function to invoke */
    Tcl_TimerDeleteProc *deleteProc,/* Function to cleanup */
    size_t extraDataSize,	/* Size of extra data to allocate */
    int flags)			/* If TCL_ABSTMR_EVENT, time is absolute */
{
    register TclTimerEntry *entryPtr, *entryPtrPos;
    register TclTimerHandler *timerPtr;
    ThreadSpecificData *tsdPtr;

    tsdPtr = InitTimer();
    timerPtr = (TclTimerHandler *)ckalloc(
			sizeof(TclTimerHandler) + extraDataSize);
    if (timerPtr == NULL) {
	return NULL;
    }
    entryPtr = TclpTimerHandler2TimerEntry(timerPtr);

    /*
     * Fill in fields for the event.
     */

    entryPtr->proc = proc;
    entryPtr->deleteProc = deleteProc;
    entryPtr->clientData = TclpTimerEntry2ClientData(entryPtr);
    entryPtr->flags = flags & TCL_ABSTMR_EVENT;
    entryPtr->generation = tsdPtr->timerGeneration;
    tsdPtr->timerListEpoch++; /* signal-timer list was changed */
    tsdPtr->lastTimerId++;
    timerPtr->token = (Tcl_TimerToken) INT2PTR(tsdPtr->lastTimerId);

    if (!(flags & TCL_ABSTMR_EVENT)) {
	Tcl_WideInt now = TclpGetMicroseconds();

	/* 
	 * We should have the ability to ajust end-time of relative events,
	 * for possible time-jumps.
	 */
	if (tsdPtr->timerList) {
	    /* 
	     * end-time = now + usec
	     * Adjust value of usec relative current base (to now), so
	     * end-time = base + relative event-time, which corresponds 
	     * original end-time.
	     */
	    Tcl_WideInt diff;
	    if ( (diff = TclpGetLastTimeJump(&tsdPtr->knownTimeJumpEpoch)) != 0
	      || (diff = (tsdPtr->knownTime - now)) < 0
	    ) { /* jump recognized */
		tsdPtr->relTimerBase += diff; /* shift the base of relative events */
    	    }
	    usec += now - tsdPtr->relTimerBase;
	} else {
	    /* first event here - initial values (base/epoch) */
	    tsdPtr->relTimerBase = now;
	    tsdPtr->knownTimeJumpEpoch = TclpGetLastTimeJumpEpoch();
	}
	tsdPtr->knownTime = now;
    }

    timerPtr->time = usec;

    /*
     * Add the event to the queue in the correct position
     * (ordered by event firing time).
     */

    /* if before current first (e. g. "after 1" before first "after 1000") */
    if ( !(entryPtrPos = tsdPtr->timerList)
      || usec < TclpTimerEntry2TimerHandler(entryPtrPos)->time
    ) {
    	/* splice to the head */
	TclSpliceInEx(entryPtr, tsdPtr->timerList, tsdPtr->timerTail);
    } else {
    	/* search from end as long as one with time before not found */
	for (entryPtrPos = tsdPtr->timerTail; entryPtrPos != NULL;
	    entryPtrPos = entryPtrPos->prevPtr) {
	    if (usec >= TclpTimerEntry2TimerHandler(entryPtrPos)->time) {
		break;
	    }
	}
	/* normally it should be always true, because checked above, but ... */
	if (entryPtrPos != NULL) {
	    /* insert after found element (with time before new) */
	    entryPtr->prevPtr = entryPtrPos;
	    if ((entryPtr->nextPtr = entryPtrPos->nextPtr)) {
		entryPtrPos->nextPtr->prevPtr = entryPtr;
	    } else {
		tsdPtr->timerTail = entryPtr;
	    }
	    entryPtrPos->nextPtr = entryPtr;
	} else {
	    /* unexpected case, but ... splice to the head */
	    TclSpliceInEx(entryPtr, tsdPtr->timerList, tsdPtr->timerTail);
	}
    }

    return entryPtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclCreateAbsoluteTimerHandler --
 *
 *	Arrange for a given function to be invoked at a particular time in the
 *	future.
 *
 * Results:
 *	The return value is a token of the timer event, which
 *	may be used to delete the event before it fires.
 *
 * Side effects:
 *	When the time in timePtr has been reached, proc will be invoked
 *	exactly once.
 *
 *--------------------------------------------------------------
 */

Tcl_TimerToken
TclCreateAbsoluteTimerHandler(
    Tcl_Time *timePtr,
    Tcl_TimerProc *proc,
    ClientData clientData)
{
    register TclTimerEntry *entryPtr;
    Tcl_WideInt usec;

    /*
     * Compute when the event should fire (avoid overflow).
     */

    if (timePtr->sec < 0x7FFFFFFFFFFFFFFFL / 1000000) {
	usec = (((Tcl_WideInt)timePtr->sec) * 1000000) + timePtr->usec;
    } else {
	usec = 0x7FFFFFFFFFFFFFFFL;
    }

    entryPtr = TclpCreateTimerHandlerEx(usec, proc, NULL, 0, TCL_ABSTMR_EVENT);
    if (entryPtr == NULL) {
	return NULL;
    }
    entryPtr->clientData = clientData;

    return TclpTimerEntry2TimerHandler(entryPtr)->token;
}

/*
 *--------------------------------------------------------------
 *
 * TclCreateRelativeTimerHandler --
 *
 *	Arrange for a given function to be invoked in a particular time offset
 *	in the future.
 *
 * Results:
 *	The return value is token of the timer event, which
 *	may be used to delete the event before it fires.
 *
 * Side effects:
 *	In contrary to absolute timer functions operate on relative time.
 *
 *--------------------------------------------------------------
 */

Tcl_TimerToken
TclCreateRelativeTimerHandler(
    Tcl_Time *timePtr,
    Tcl_TimerProc *proc,
    ClientData clientData)
{
    register TclTimerEntry *entryPtr;
    Tcl_WideInt usec;

    /*
     * Compute when the event should fire (avoid overflow).
     */

    if (timePtr->sec < 0x7FFFFFFFFFFFFFFFL / 1000000) {
	usec = (((Tcl_WideInt)timePtr->sec) * 1000000) + timePtr->usec;
    } else {
	usec = 0x7FFFFFFFFFFFFFFFL;
    }

    entryPtr = TclpCreateTimerHandlerEx(usec, proc, NULL, 0, TCL_ABSTMR_EVENT);
    if (entryPtr == NULL) {
	return NULL;
    }
    entryPtr->clientData = clientData;

    return TclpTimerEntry2TimerHandler(entryPtr)->token;
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
    register TclTimerEntry *entryPtr;
    ThreadSpecificData *tsdPtr = InitTimer();

    if (token == NULL) {
	return;
    }

    for (entryPtr = tsdPtr->timerTail;
	entryPtr != NULL;
	entryPtr = entryPtr->prevPtr
    ) {
	if (TclpTimerEntry2TimerHandler(entryPtr)->token != token) {
	    continue;
	}

	TclpDeleteTimerEntry(entryPtr);
	return;
    }
}


/*
 *--------------------------------------------------------------
 *
 * TclpDeleteTimerEntry --
 *
 *	Delete a previously-registered prompt, timer or idle handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroy the timer callback, so that its associated function will
 *	not be called. If the callback has already fired this will be executed
 *	internally.
 *
 *--------------------------------------------------------------
 */

void
TclpDeleteTimerEntry(
    TclTimerEntry *entryPtr)	/* Result previously returned by */
	/* TclpCreateTimerHandlerEx or TclpCreateTimerEntryEx. */
{
    ThreadSpecificData *tsdPtr;

    if (entryPtr == NULL) {
	return;
    }
    if (entryPtr->flags & (TCL_EVENTST_EXECUTE|TCL_EVENTST_DELETE)) {
	/* do nothing - event will be automatically deleted hereafter */
	return;
    }

    tsdPtr = InitTimer();

    /* 
     * Mark this entry will be deleted, so it can avoid double delete and
     * caller can check in delete callback, the time entry handle is still
     * the same (was not overriden in some recursive async-envent).
     */
    entryPtr->flags |= TCL_EVENTST_DELETE;
    if (entryPtr->flags & TCL_PROMPT_EVENT) {
	/* prompt handler */
	TclSpliceOutEx(entryPtr, tsdPtr->promptList, tsdPtr->promptTail);
    } else if (entryPtr->flags & TCL_IDLE_EVENT) {
	/* idle handler */
	TclSpliceOutEx(entryPtr, tsdPtr->idleList, tsdPtr->idleTail);
    } else {
	/* timer event-handler */
	tsdPtr->timerListEpoch++; /* signal-timer list was changed */
	TclSpliceOutEx(entryPtr, tsdPtr->timerList, tsdPtr->timerTail);
    }

    /* free it via deleteProc or ckfree */
    if (entryPtr->deleteProc) {
    	entryPtr->flags |= TCL_EVENTST_DELETE;
	(*entryPtr->deleteProc)(entryPtr->clientData);
	/* if prolongation requested - reattached to tail */
	if (!(entryPtr->flags & TCL_EVENTST_DELETE)) {
	    return;
	}
    }

    if (entryPtr->flags & (TCL_PROMPT_EVENT|TCL_IDLE_EVENT)) {
	ckfree((char *)entryPtr);
    } else {
	/* shift to the allocated pointer */
	ckfree((char *)TclpTimerEntry2TimerHandler(entryPtr));
    }
}

void
TclpProlongTimerHandler(
    TclTimerEntry *entryPtr,
    Tcl_WideInt usec,
    int flags)
{
    ThreadSpecificData *tsdPtr = InitTimer();

    /* reset execution and deletion states */
    entryPtr->flags &= ~(TCL_EVENTST_EXECUTE|TCL_EVENTST_DELETE);
    /* attach to the queue again (new generation) */
    if (usec != 0 || flags != 0) {
	Tcl_Panic("NYI: ATM only prompt & idle");
	return;
    }
    AttachTimerEntry(tsdPtr, entryPtr);
}

/*
 *--------------------------------------------------------------
 *
 * TimerGetDueTime --
 *
 *	Find the execution time of first relative or absolute timer starting
 *	from given heads.
 *
 * Results:
 *	A wide integer representing the due time (as microseconds) of first
 *	timer event to execute.
 *
 * Side effects:
 *	If time-jump recognized, may adjust the base for relative timers.
 *
 *--------------------------------------------------------------
 */

static Tcl_WideInt
TimerGetDueTime(
    ThreadSpecificData *tsdPtr,
    TclTimerEntry *entryPtr,
    Tcl_WideInt now)
{
    Tcl_WideInt firstTime;

    /* 
     * Consider time-jump (especially back) - if the time jumped forwards (and
     * it recognized) the base can be shifted, but not badly needed, because
     * event will be nevertheless executed early as specified. But for backwards
     * jumps it is very important and we should adjust relative base to avoid 
     * too long waiting for relative events.
     */
    Tcl_WideInt diff;
    if ( (diff = TclpGetLastTimeJump(&tsdPtr->knownTimeJumpEpoch)) != 0
      || (diff = (now - tsdPtr->knownTime)) < 0 /* switched back */
    ) {
	/* 
	 * If the real jump is unknown (resp. too complex to retrieve
	 * accross all threads), we simply accept possible small increment
	 * of the real wait-time.
	 */
	tsdPtr->relTimerBase += diff; /* shift the base */
    }
    tsdPtr->knownTime = now;

    /* If absolute timer: end-time = absolute event-time */
    firstTime = TclpTimerEntry2TimerHandler(entryPtr)->time;
    if ((entryPtr->flags & TCL_ABSTMR_EVENT)) {
    	return firstTime;
    }
    /* end-time = base + relative event-time */
    return firstTime + tsdPtr->relTimerBase;
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
    ClientData data,		/* Specific data. */
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)data;

    if (tsdPtr == NULL) { tsdPtr = InitTimer(); };

    if ( ((flags & TCL_TIMER_EVENTS) && tsdPtr->timerPending)
      || ((flags & TCL_IDLE_EVENTS) && tsdPtr->idleList )
    ) {
	/*
	 * There is a pending timer event or an idle handler, so just poll.
	 */

	blockTime.sec = 0;
	blockTime.usec = 0;

    } else if (
	   (flags & TCL_TIMER_EVENTS) 
	&& (tsdPtr->timerList)
    ) {
	/*
	 * Compute the timeout for the next timer on the list.
	 */

	Tcl_WideInt now = TclpGetMicroseconds();
	Tcl_WideInt entryTime;

	entryTime = TimerGetDueTime(tsdPtr, tsdPtr->timerList, now) - now;

	#ifdef TMR_RES_TOLERANCE
	    /* consider timer resolution tolerance (avoid busy wait) */
	    entryTime -= ((entryTime <= 1000000) ? entryTime : 1000000) *
				TMR_RES_TOLERANCE / 100;
	#endif

	if (entryTime > 0) {
	    blockTime.sec = (long) (entryTime / 1000000);
	    blockTime.usec = (unsigned long) (entryTime % 1000000);

	} else {
	    blockTime.sec = 0;
	    blockTime.usec = 0;
	}
	
	/*
	* If the first timer has expired, stick an event on the queue right now.
	*/
	if (!tsdPtr->timerPending && entryTime <= 0) {
	    TclSetTimerEventMarker(0);
	    tsdPtr->timerPending = 1;
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
 *	source for events. This routine checks the first timer in the list.
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
    ClientData data,		/* Specific data. */
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    Tcl_WideInt now, entryTime;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)data;

    if (!(flags & TCL_TIMER_EVENTS)) {
    	return;
    }

    if (tsdPtr == NULL) { tsdPtr = InitTimer(); };

    /* If already pending (or no timer-events) */
    if (tsdPtr->timerPending || !tsdPtr->timerList) {
    	return;
    }

    /*
     * Verify the first timer on the queue.
     */
    now = TclpGetMicroseconds();
    entryTime = TimerGetDueTime(tsdPtr, tsdPtr->timerList, now) - now;
    
#ifdef TMR_RES_TOLERANCE
    /* consider timer resolution tolerance (avoid busy wait) */
    entryTime -= ((entryTime <= 1000000) ? entryTime : 1000000) *
			TMR_RES_TOLERANCE / 100;
#endif
   
    /*
    * If the first timer has expired, stick an event on the queue.
    */
    if (entryTime <= 0) {
	TclSetTimerEventMarker(0);
	tsdPtr->timerPending = 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclServiceTimerEvents --
 *
 *	This function is called by Tcl_ServiceEvent when a timer events should 
 *	be processed. This function handles the event by
 *	invoking the callbacks for all timers that are ready.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed from
 *	the queue. 
 *	Returns 0 if the event was not handled (no timer events).
 *	Returns -1 if pending timer events available, meaning the marker should
 *	stay on the head of queue.
 *
 * Side effects:
 *	Whatever the timer handler callback functions do.
 *
 *----------------------------------------------------------------------
 */

int
TclServiceTimerEvents(void)
{
    TclTimerEntry *entryPtr, *nextPtr;
    Tcl_WideInt now, entryTime;
    size_t currentGeneration, currentEpoch;
    int prevTmrPending;
    ThreadSpecificData *tsdPtr = InitTimer();

    if (!tsdPtr->timerPending) {
    	return 0; /* no timer events */
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

    /* First process all prompt (immediate) events */
    while ((entryPtr = tsdPtr->promptList) != NULL
	&& entryPtr->generation <= currentGeneration
    ) {
	/* detach entry from the owner's list */
	TclSpliceOutEx(entryPtr, tsdPtr->promptList, tsdPtr->promptTail);

	/* reset current timer pending (correct process nested wait event) */
	prevTmrPending = tsdPtr->timerPending;
	tsdPtr->timerPending = 0;
	/* execute event */
	entryPtr->flags |= TCL_EVENTST_EXECUTE;
	(*entryPtr->proc)(entryPtr->clientData);
	/* restore current timer pending */
	tsdPtr->timerPending += prevTmrPending;
	/* if prolongation requested - reattached to tail */
	if (!(entryPtr->flags & TCL_EVENTST_EXECUTE)) {
	    continue;
	}
	/* free it via deleteProc and ckfree */
	if (entryPtr->deleteProc) {
	    entryPtr->flags |= TCL_EVENTST_DELETE;
	    (*entryPtr->deleteProc)(entryPtr->clientData);
	    /* if prolongation requested - reattached to tail */
	    if (!(entryPtr->flags & TCL_EVENTST_DELETE)) {
	        continue;
	    }
	}
	ckfree((char *) entryPtr);
    }

    /* if stil pending prompt events (new generation) - repeat event cycle as 
     * soon as possible */
    if (tsdPtr->promptList) {
        tsdPtr->timerPending = 1;
    	return -1;
    }

    /* Hereafter all relative and absolute timer events with time before now */
    for (entryPtr = tsdPtr->timerList;
	 entryPtr != NULL;
	 entryPtr = nextPtr
    ) {
	nextPtr = entryPtr->nextPtr;
	now = TclpGetMicroseconds();
	entryTime = TimerGetDueTime(tsdPtr, entryPtr, now);

    	/* the same tolerance logic as in TimerSetupProc/TimerCheckProc */
    #ifdef TMR_RES_TOLERANCE
	entryTime -= ((entryTime <= 1000000) ? entryTime : 1000000) *
				TMR_RES_TOLERANCE / 100;
    #endif

	if (now < entryTime) {
	    break;
	}

	/*
	 * Bypass timers of newer generation.
	 */

	if (entryPtr->generation > currentGeneration) {
	    /* increase pending to signal repeat */
	    tsdPtr->timerPending++;
	    continue;
	}

	tsdPtr->timerListEpoch++; /* signal-timer list was changed */
	currentEpoch = tsdPtr->timerListEpoch;
	 
	/*
	 * Remove the handler from the queue before invoking it, to avoid
	 * potential reentrancy problems.
	 */
	TclSpliceOutEx(entryPtr, 
		tsdPtr->timerList, tsdPtr->timerTail);

	/* reset current timer pending (correct process nested wait event) */
	prevTmrPending = tsdPtr->timerPending;
	tsdPtr->timerPending = 0;
	/* invoke timer proc */
	entryPtr->flags |= TCL_EVENTST_EXECUTE;
	(*entryPtr->proc)(entryPtr->clientData);
	/* restore current timer pending */
	tsdPtr->timerPending += prevTmrPending;

	/* free it via deleteProc or ckfree */
	if (entryPtr->deleteProc) {
	    entryPtr->flags |= TCL_EVENTST_DELETE;
	    (*entryPtr->deleteProc)(entryPtr->clientData);
	}

	ckfree((char *) TclpTimerEntry2TimerHandler(entryPtr));

	/* be sure that timer-list was not changed inside the proc call */
	if (currentEpoch != tsdPtr->timerListEpoch) {
	    /* timer-list was changed - stop processing */
	    tsdPtr->timerPending++;
	    break;
	}
    }

    /* pending timer events, so mark (queue) timer events  */
    if (tsdPtr->timerPending > 1) {
    	tsdPtr->timerPending = 1;
    	return -1;
    }

    /* Reset generation if both timer queue are empty */
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
 * TclpCreateTimerEntryEx --
 *
 *	Arrange for proc to be invoked delayed (but prompt) as timer event,
 *	without time ("after 0").
 *	Or as idle event (the next time the system is idle i.e., just 
 *	before the next time that Tcl_DoOneEvent would have to wait for
 *	something to happen).
 *
 *	Providing the flag TCL_PROMPT_EVENT ensures that timer event-handler
 *	will be queued immediately to guarantee the execution of timer-event
 *	as soon as possible
 *
 * Results:
 *	Returns the created timer entry.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

TclTimerEntry *
TclpCreateTimerEntryEx(
    Tcl_TimerProc *proc,		/* Function to invoke. */
    Tcl_TimerDeleteProc *deleteProc,	/* Function to cleanup */
    size_t extraDataSize,
    int flags)
{
    register TclTimerEntry *entryPtr;
    ThreadSpecificData *tsdPtr = InitTimer();

    entryPtr = (TclTimerEntry *) ckalloc(sizeof(TclTimerEntry) + extraDataSize);
    if (entryPtr == NULL) {
	return NULL;
    }
    entryPtr->proc = proc;
    entryPtr->deleteProc = deleteProc;
    entryPtr->clientData = TclpTimerEntry2ClientData(entryPtr);
    entryPtr->flags = flags;

    AttachTimerEntry(tsdPtr, entryPtr);

    return entryPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_DoWhenIdle --
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
void
Tcl_DoWhenIdle(
    Tcl_IdleProc *proc,		/* Function to invoke. */
    ClientData clientData)	/* Arbitrary value to pass to proc. */
{
    TclTimerEntry *idlePtr = TclpCreateTimerEntryEx(proc, NULL, 0, TCL_IDLE_EVENT);

    if (idlePtr) {
    	idlePtr->clientData = clientData;
    }
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
    register TclTimerEntry *idlePtr, *nextPtr;
    ThreadSpecificData *tsdPtr = InitTimer();

    for (idlePtr = tsdPtr->idleList;
    	idlePtr != NULL;
	idlePtr = nextPtr
    ) {
    	nextPtr = idlePtr->nextPtr;
	if ((idlePtr->proc == proc)
		&& (idlePtr->clientData == clientData)) {
	    /* detach entry from the owner list */
	    TclSpliceOutEx(idlePtr, tsdPtr->idleList, tsdPtr->idleTail);

	    /* free it via deleteProc and ckfree */
	    if (idlePtr->deleteProc) {
	    	idlePtr->flags |= TCL_EVENTST_DELETE;
		(*idlePtr->deleteProc)(idlePtr->clientData);
	    }
	    ckfree((char *) idlePtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclServiceIdle -- , TclServiceIdleEx --
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
TclServiceIdleEx(
    int flags,
    int count)
{
    TclTimerEntry *idlePtr;
    size_t currentGeneration;
    ThreadSpecificData *tsdPtr = InitTimer();

    if ((idlePtr = tsdPtr->idleList) == NULL) {
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

    while (idlePtr->generation <= currentGeneration) {
	/* detach entry from the owner's list */
	TclSpliceOutEx(idlePtr, tsdPtr->idleList, tsdPtr->idleTail);

	/* execute event */
	idlePtr->flags |= TCL_EVENTST_EXECUTE;
	(*idlePtr->proc)(idlePtr->clientData);
	/* if prolongation requested - reattached to tail */
	if (!(idlePtr->flags & TCL_EVENTST_EXECUTE)) {
	    goto nextEvent;
	}
	/* free it via deleteProc and ckfree */
	if (idlePtr->deleteProc) {
	    idlePtr->flags |= TCL_EVENTST_DELETE;
	    (*idlePtr->deleteProc)(idlePtr->clientData);
	    /* if prolongation requested - reattached to tail */
	    if (!(idlePtr->flags & TCL_EVENTST_DELETE)) {
		goto nextEvent;
	    }
	}
	ckfree((char *) idlePtr);

    nextEvent:
	/*
	 * Stop processing idle if idle queue empty, count reached or other
	 * events queued (only if not idle events only to service).
	 */
	if ( (idlePtr = tsdPtr->idleList) == NULL
	  || !--count
	  || ((flags & TCL_ALL_EVENTS) != TCL_IDLE_EVENTS 
		&& TclPeekEventQueued(flags))
	) {
	    break;
	}
    }

    /* Reset generation */
    if (!tsdPtr->idleList) {
    	tsdPtr->idleGeneration = 0;
    }
    return 1;
}

int
TclServiceIdle(void)
{
    return TclServiceIdleEx(TCL_ALL_EVENTS, INT_MAX);
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetTimeFromObj --
 *
 *	This function converts numeric tcl-object contains decimal milliseconds,
 *	(using milliseconds base) to time offset in microseconds,
 *
 *	If input object contains double, the return time has microsecond 
 *	precision.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	If possible leaves internal representation unchanged (e. g. integer).
 *
 *----------------------------------------------------------------------
 */

int
TclpGetUTimeFromObj(
    Tcl_Interp	*interp,	/* Current interpreter or NULL. */
    Tcl_Obj	*objPtr,	/* Object to read numeric time (in milliseconds). */
    Tcl_WideInt	*timePtr)	/* Resulting time if converted (in microseconds). */
{
    if (objPtr->typePtr != &tclDoubleType) {
	Tcl_WideInt ms;
	if (Tcl_GetWideIntFromObj(NULL, objPtr, &ms) == TCL_OK) {
	    if (ms < 0x7FFFFFFFFFFFFFFFL / 1000) { /* avoid overflow */
		*timePtr = (ms * 1000);
		return TCL_OK;
	    }
	    *timePtr = 0x7FFFFFFFFFFFFFFFL;
	    return TCL_OK;
	}
    }
    if (1) {
	double ms;
	if (Tcl_GetDoubleFromObj(interp, objPtr, &ms) == TCL_OK) {
	    if (ms < 0x7FFFFFFFFFFFFFFFL / 1000) { /* avoid overflow */
		/* use precise as possible calculation by double (microseconds) */
		*timePtr = ((Tcl_WideInt)ms) * 1000 + (((long)(ms*1000)) % 1000);
		return TCL_OK;
	    }
	    *timePtr = 0x7FFFFFFFFFFFFFFFL;
	    return TCL_OK;
	}
    }
    return TCL_ERROR;
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
    Tcl_WideInt usec;		/* Number of microseconds to wait (or time to wakeup) */
    AfterInfo *afterPtr;
    AfterAssocData *assocPtr;
    int length;
    int index;
    static CONST char *afterSubCmds[] = {
	"at", "cancel", "idle", "info", NULL
    };
    enum afterSubCmds {AFTER_AT, AFTER_CANCEL, AFTER_IDLE, AFTER_INFO};
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

    index = -1;
    if ( ( TclObjIsIndexOfTable(objv[1], afterSubCmds)
	|| TclpGetUTimeFromObj(NULL, objv[1], &usec) != TCL_OK
      )
      && Tcl_GetIndexFromObj(NULL, objv[1], afterSubCmds, "", 0,
				 &index) != TCL_OK
    ) {
	Tcl_AppendResult(interp, "bad argument \"",
		Tcl_GetString(objv[1]),
		"\": must be at, cancel, idle, info, or a time", NULL);
	return TCL_ERROR;
    }

    /* 
     * At this point, either index = -1 and usec contains the time
     * to wait, or else index is the index of a subcommand.
     */

    switch (index) {
    case -1:
	/* usec already contains time-offset from objv[1] */
	/* relative time offset should be positive */
	if (usec < 0) {
	    usec = 0;
	}
	if (objc == 2) {
	    /* after <offset> */
	    return AfterDelay(interp, usec, 0);
	}
    case AFTER_AT: {
    	TclTimerEntry *entryPtr;
    	int flags = 0;
    	if (index == AFTER_AT) {
	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "time");
		return TCL_ERROR;
	    }
	    /* get time from objv[2] */
	    if (TclpGetUTimeFromObj(interp, objv[2], &usec) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (objc == 3) {
		/* after at <time> */
		return AfterDelay(interp, usec, flags);
	    }
	    flags = TCL_ABSTMR_EVENT;
	}

	if (usec || (index == AFTER_AT)) {
	    /* after ?at? <time|offset> <command> ... */
	    entryPtr = TclpCreateTimerHandlerEx(usec, AfterProc,
			    FreeAfterPtr, sizeof(AfterInfo), flags);
	} else {
	    /* after 0 <command> ... */
	    entryPtr = TclpCreateTimerEntryEx(AfterProc,
			    FreeAfterPtr, sizeof(AfterInfo), TCL_PROMPT_EVENT);
	}

	if (entryPtr == NULL) { /* error handled in panic */
	    return TCL_ERROR;
	}
	afterPtr = TclpTimerEntry2AfterInfo(entryPtr);

	/* attach to the list */
	afterPtr->assocPtr = assocPtr;
	TclSpliceTailEx(afterPtr, 
		assocPtr->firstAfterPtr, assocPtr->lastAfterPtr);
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
	if (afterPtr != NULL && afterPtr->assocPtr->interp == interp) {
	    TclpDeleteTimerEntry(TclpAfterInfo2TimerEntry(afterPtr));
	}
	break;
    }
    case AFTER_IDLE: {
    	TclTimerEntry *idlePtr;

	if (objc < 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "script script ...");
	    return TCL_ERROR;
	}

	idlePtr = TclpCreateTimerEntryEx(AfterProc,
			FreeAfterPtr, sizeof(AfterInfo), TCL_IDLE_EVENT);
	if (idlePtr == NULL) { /* error handled in panic */
	    return TCL_ERROR;
	}
	afterPtr = TclpTimerEntry2AfterInfo(idlePtr);

	/* attach to the list */
	afterPtr->assocPtr = assocPtr;
	TclSpliceTailEx(afterPtr, 
		assocPtr->firstAfterPtr, assocPtr->lastAfterPtr);
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
	if (afterPtr == NULL || afterPtr->assocPtr->interp != interp) {
	    Tcl_AppendResult(interp, "event \"", TclGetString(objv[2]),
		    "\" doesn't exist", NULL);
	    return TCL_ERROR;
	}
	resultListPtr = Tcl_NewObj();
	Tcl_ListObjAppendElement(interp, resultListPtr, afterPtr->commandPtr);
	Tcl_ListObjAppendElement(interp, resultListPtr, Tcl_NewStringObj(
 		(TclpAfterInfo2TimerEntry(afterPtr)->flags & TCL_IDLE_EVENT) ?
 		    "idle" : "timer", -1));
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
    Tcl_WideInt usec,
    int absolute)
{
    Interp *iPtr = (Interp *) interp;

    Tcl_WideInt endTime, now, lastNow, diff;
    long tolerance = 0;
    size_t timeJumpEpoch = 0;

    if (usec <= 0) {
	/* to cause a context switch only */
	Tcl_Sleep(0);
	return TCL_OK;
    }

    /* calculate possible maximal tolerance (in usec) of original wait-time */
#ifdef TMR_RES_TOLERANCE
    tolerance = ((usec < 1000000) ? usec : 1000000) * TMR_RES_TOLERANCE / 100;
#endif

    lastNow = now = TclpGetMicroseconds();
    endTime = usec;
    timeJumpEpoch = TclpGetLastTimeJumpEpoch();

    if (!absolute) {
	endTime += now;
	if (endTime < now) { /* overflow */
	    endTime = 0x7FFFFFFFFFFFFFFFL;
	}
    }

    do {
	if ( iPtr->limit.timeEvent != NULL
	  && now > TCL_TIME_TO_USEC(iPtr->limit.time)
	) {
	    iPtr->limit.granularityTicker = 0;
	    if (Tcl_LimitCheck(interp) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if ( iPtr->limit.timeEvent == NULL
	  || endTime < (diff = TCL_TIME_TO_USEC(iPtr->limit.time))
	) {
	    diff = endTime - now;
	    if (diff > 0) {
		TclpUSleep(diff);
		now = TclpGetMicroseconds();
	    }
	} else {
	    diff -= now;
	    if (diff > 0) {
		TclpUSleep(diff);
		now = TclpGetMicroseconds();
	    }
	    if (Tcl_LimitCheck(interp) != TCL_OK) {
		return TCL_ERROR;
	    }
	}

	/* 
	 * Note time can be switched backwards, certainly adjust end-time
	 * by possible time-jumps (for relative sleep).
	 */
	if (!absolute
	  && ( (diff = TclpGetLastTimeJump(&timeJumpEpoch)) != 0 
	    || (diff = (now - lastNow)) < 0
	  )
	) {
	    /* recognized time-jump - simply shift wakeup-time */
	    endTime += diff;
	}
	lastNow = now;

	/* consider timer resolution tolerance (avoid busy wait) */
    } while (now < endTime);
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
    TclpAfterInfo2TimerEntry(afterPtr)->deleteProc = NULL;

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

    while ( assocPtr->lastAfterPtr ) {
	TclpDeleteTimerEntry(TclpAfterInfo2TimerEntry(assocPtr->lastAfterPtr));
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
