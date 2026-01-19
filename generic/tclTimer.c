/*
 * tclTimer.c --
 *
 *	This file provides timer event management facilities for Tcl,
 *	including the "after" command.
 *
 * Copyright © 1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * For each timer callback that's pending there is one record of the following
 * type. The normal handlers (created by Tcl_CreateTimerHandler) are chained
 * together in a list sorted by time (earliest event first).
 */

typedef struct TimerHandler {
    long long time;		/* When timer is to fire. */
    Tcl_TimerProc *proc;	/* Function to call. */
    void *clientData;		/* Argument to pass to proc. */
    Tcl_TimerToken token;	/* Identifies handler so it can be deleted.
				 * NULL for [after/timer idle]. */
    struct TimerHandler *nextPtr;
				/* Next event in queue, or NULL for end of
				 * queue. */
} TimerHandler;

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
    int id;			/* Integer identifier for command; used to
				 * cancel it. */
    Tcl_TimerToken token;	/* Used to cancel the "after" command. NULL
				 * means that the command is run as an idle
				 * handler rather than as a timer handler. */
    struct AfterInfo *nextPtr;	/* Next in list of all "after" commands for
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
    AfterInfo *firstAfterPtr;	/* First in list of all "monotonic", "wallclock"
				 * or "idle"
				 * commands still pending for this interpreter, or
				 * NULL if none. */
} AfterAssocData;

/* Associated data key used to look up the AfterAssocData for an interp. */
#define ASSOC_KEY "tclAfter"

/*
 * There is one of the following structures for each of the handlers declared
 * in a call to Tcl_DoWhenIdle. All of the currently-active handlers are
 * linked together into a list.
 */

typedef struct IdleHandler {
    Tcl_IdleProc *proc;		/* Function to call. */
    void *clientData;		/* Value to pass to proc. */
    int generation;		/* Used to distinguish older handlers from
				 * recently-created ones. */
    struct IdleHandler *nextPtr;/* Next in list of active handlers. */
} IdleHandler;

/*
 * Create an enum to index the firstTimeHandlerPtr array below.
 * There are 3 queues: monotonic, wall clock and idle.
 * The idle queue is (still) handled separately, but may eventually be added here.
 */

enum timeHandlerType {
    timerHandlerMonotonic = 0,
    timerHandlerWallclock = 1
};

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
    TimerHandler *firstTimerHandlerPtr[2];
				/* [0] First event in monotonic queue. */
				/* [1]: First event in wallclock queue. */
    int lastTimerId;		/* Timer identifier of most recently created
				 * timer. */
    int lastTimerIdQueue[2];	/* Last timer ID for each queue. This is
				 * compared to fired event to check, if there
				 * were newer events added. */
    int timerPendingQueue[2];	/* 1 if a timer event is in the given queue. */
    IdleHandler *idleList;	/* First in list of all idle handlers. */
    IdleHandler *lastIdlePtr;	/* Last in list (or NULL for empty list). */
    int idleGeneration;		/* Used to fill in the "generation" fields of
				 * IdleHandler structures. Increments each
				 * time Tcl_DoOneEvent starts calling idle
				 * handlers, so that all old handlers can be
				 * called without calling any of the new ones
				 * created by old ones. */
    int afterId;		/* For unique identifiers of after events. */
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

/*
 * time point and distance unit strings and values.
 * Used for argument parsing.
 */

static const char *const unitItems[] = {
    "us",
    "microseconds",
    "milliseconds",
    "ms",
    "s",
    "seconds",
    NULL
};
enum unitEnum {
    UNIT_US,
    UNIT_MICROSECONDS,
    UNIT_MILLISECONDS,
    UNIT_MS,
    UNIT_S,
    UNIT_SECONDS
};

// Microseconds per second.
#define US_PER_S	1000000
// Milliseconds per second.
#define MS_PER_S	1000
// Microseconds per millisecond.
#define US_PER_MS	1000

// Convert a Tcl_Time timestamp to microseconds.
#define TCL_TIME_US(time) \
	((time).sec * US_PER_S + (time).usec)

/*
 * Sleeps under that number of microseconds don't get double-checked
 * and are done in exactly one Tcl_Sleep(). This to limit gettimeofday()s.
 */

#define SLEEP_OFFLOAD_GETTIMEOFDAY 20000

/*
 * The maximum number of microseconds for each Tcl_Sleep call in TimerDelay.
 * This is used to limit the maximum lag between interp limit and script
 * cancellation checks.
 */

#define TCL_TIME_MAXIMUM_SLICE 500000

/*
 * Prototypes for functions referenced only in this file:
 */

static void		AfterCleanupProc(void *clientData,
			    Tcl_Interp *interp);
static void		AfterProc(void *clientData);
static void		FreeAfterPtr(AfterInfo *afterPtr);
static AfterInfo *	GetAfterEvent(AfterAssocData *assocPtr,
			    Tcl_Obj *commandPtr);
static ThreadSpecificData *InitTimer(void);
static void		TimerExitProc(void *clientData);
static int		TimerHandlerEventProc(Tcl_Event *evPtr, int flags);
static void		TimerCheckProc(void *clientData, int flags);
static void		TimerSetupProc(void *clientData, int flags);
static Tcl_ObjCmdProc2	TimerAtCmd;
static Tcl_ObjCmdProc2	TimerInCmd;
static Tcl_ObjCmdProc2	TimerSleepCmd;
static int		TimerSleepForCmd(Tcl_Interp *interp,
			    long long sleepArgUS);
static int		TimerSleepUntilCmd(Tcl_Interp *interp,
			    long long sleepArgUS);
static int		TimerDelay(Tcl_Interp *interp, long long endTime);
static int		TimerDelayMonotonic(Tcl_Interp *interp,
			    long long endTimeUS);
static Tcl_ObjCmdProc2	TimerCancelCmd;
static int		TimerCancelDo(Tcl_Interp *interp,
			    Tcl_Obj *const objArg, bool notFoundError);
static AfterAssocData *	TimerAssocDataGet(Tcl_Interp *interp);
static Tcl_ObjCmdProc2	TimerIdleCmd;
static int		TimerIdleDo(Tcl_Interp *interp,
			    Tcl_Obj *const objCmd);
static Tcl_ObjCmdProc2	TimerInfoCmd;
static void		TimeTooFarError(Tcl_Interp *interp);
static int		ParseTimeUnit(Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[],
			    bool useDefaultUnit, enum unitEnum defaultUnit,
			    long long *wakeupPtr);
static int		TimerInfoDo(Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[], bool isAfter);
static Tcl_TimerToken	CreateTimerHandler(long long time,
			    Tcl_TimerProc *proc, void *clientData,
			    bool monotonic);

/*
 * How to construct the ensembles.
 */

const EnsembleImplMap tclTimerImplMap[] = {
    {"at",	TimerAtCmd,	TclCompileBasic3ArgCmd, NULL, NULL, 0},
    {"in",	TimerInCmd,	TclCompileBasic3ArgCmd, NULL, NULL, 0},
    {"cancel",	TimerCancelCmd,	TclCompileBasic1ArgCmd, NULL, NULL, 0},
    {"idle",	TimerIdleCmd,	TclCompileBasic1ArgCmd, NULL, NULL, 0},
    {"info",	TimerInfoCmd,	TclCompileBasic0Or1ArgCmd, NULL, NULL, 0},
    {"sleep",	TimerSleepCmd,	TclCompileBasic2Or3ArgCmd, NULL, NULL, 0},
    {NULL, NULL, NULL, NULL, NULL, 0}
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
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);

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
    TCL_UNUSED(void *))
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);

    Tcl_DeleteEventSource(TimerSetupProc, TimerCheckProc, NULL);
    if (tsdPtr != NULL) {
	/*
	 * Loop over wallclock and monotonic clock queue
	 */

	for (int timerHandlerIndex = timerHandlerMonotonic;
		timerHandlerIndex <=timerHandlerWallclock; timerHandlerIndex++) {
	    TimerHandler *timerHandlerPtr =
		    tsdPtr->firstTimerHandlerPtr[timerHandlerIndex];
	    while (timerHandlerPtr != NULL) {
		tsdPtr->firstTimerHandlerPtr[timerHandlerIndex] = timerHandlerPtr->nextPtr;
		Tcl_Free(timerHandlerPtr);
		timerHandlerPtr = tsdPtr->firstTimerHandlerPtr[timerHandlerIndex];
	    }
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_CreateTimerHandlerMicroSeconds --
 *
 *	Arrange for a given function to be invoked after a given monotonic
 *	time interval in micro seconds.
 *
 * Results:
 *	The return value is a token for the timer event, which may be used to
 *	delete the event before it fires.
 *
 * Side effects:
 *	When micro-seconds have elapsed, proc will be invoked exactly once.
 *
 *--------------------------------------------------------------
 */

Tcl_TimerToken
Tcl_CreateTimerHandlerMicroSeconds(
    long long microSeconds,	/* How many micro-seconds to wait before
				 * invoking proc. */
    Tcl_TimerProc *proc,	/* Function to invoke. */
    void *clientData)		/* Arbitrary data to pass to proc. */
{
    long long timeUS;

    /*
     * Compute when the event should fire.
     */

    if (microSeconds <= 0) {
	timeUS = 0;
    } else {
	timeUS = Tcl_GetMonotonicTime();
	if (LLONG_MAX - microSeconds < timeUS) {
	    return NULL;
	}
	timeUS += microSeconds;
    }
    return CreateTimerHandler(timeUS, proc, clientData, true);
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_CreateTimerHandler --
 *
 *	Arrange for a given function to be invoked after a given monotonic
 *	time interval.
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
    void *clientData)		/* Arbitrary data to pass to proc. */
{
    long long timeUS;
    long long delayUS;

    delayUS = milliseconds * US_PER_MS;
    timeUS = Tcl_GetMonotonicTime();

    /*
     * The max value for delayUS is 2^31*1000.
     * This is far away from LLONG_MAX.
     * Nevertheless, use the upper limit in case of overflow.
     * Alternate possibility would be to return NULL, what is
     * currently not foreseen.
     */

    if (delayUS > LLONG_MAX - timeUS) {
	timeUS = LLONG_MAX;
    } else {
	timeUS += delayUS;
    }

    return CreateTimerHandler(timeUS, proc, clientData, true);
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
 *	The return value is a token for the timer event, which may be used to
 *	delete the event before it fires.
 *
 * Side effects:
 *	When the time in timePtr has been reached, proc will be invoked
 *	exactly once.
 *
 *--------------------------------------------------------------
 */

Tcl_TimerToken
TclCreateAbsoluteTimerHandler(
    long long time,
    Tcl_TimerProc *proc,
    void *clientData)
{
    return CreateTimerHandler(time, proc, clientData, false);
}

/*
 *--------------------------------------------------------------
 *
 * TclCreateMonotonicTimerHandler --
 *
 *	Arrange for a given function to be invoked at a particular time in the
 *	future.
 *	The parameter 'monotonic' controls, if the monotonic clock or
 *	the wall clock is used.
 *
 * Results:
 *	The return value is a token for the timer event, which may be used to
 *	delete the event before it fires.
 *
 * Side effects:
 *	When the time in timePtr has been reached, proc will be invoked
 *	exactly once.
 *
 *--------------------------------------------------------------
 */

Tcl_TimerToken
TclCreateMonotonicTimerHandler(
    long long time,
    Tcl_TimerProc *proc,
    void *clientData)
{
    return CreateTimerHandler(time, proc, clientData, true);
}

/*
 *--------------------------------------------------------------
 *
 * CreateTimerHandler --
 *
 *	Arrange for a given function to be invoked at a particular time in the
 *	future.
 *	The parameter 'monotonic' controls, if the monotonic clock or
 *	the wall clock is used.
 *
 * Results:
 *	The return value is a token for the timer event, which may be used to
 *	delete the event before it fires.
 *
 * Side effects:
 *	When the time in timePtr has been reached, proc will be invoked
 *	exactly once.
 *
 *--------------------------------------------------------------
 */

static Tcl_TimerToken
CreateTimerHandler(
    long long time,
    Tcl_TimerProc *proc,
    void *clientData,
    bool monotonic)
{
    TimerHandler *timerHandlerPtr, *tPtr2, *prevPtr;
    ThreadSpecificData *tsdPtr = InitTimer();
    enum timeHandlerType timerHandlerIndex = (monotonic ? timerHandlerMonotonic
	    : timerHandlerWallclock);

    timerHandlerPtr = (TimerHandler *)Tcl_Alloc(sizeof(TimerHandler));

    /*
     * Fill in fields for the event.
     */
    timerHandlerPtr->time = time;
    timerHandlerPtr->proc = proc;
    timerHandlerPtr->clientData = clientData;
    tsdPtr->lastTimerId++;
    tsdPtr->lastTimerIdQueue[timerHandlerIndex] = tsdPtr->lastTimerId;
    timerHandlerPtr->token = (Tcl_TimerToken) INT2PTR(tsdPtr->lastTimerId);

    /*
     * Add the event to the corresponding queue in the correct position
     * (ordered by event firing time).
     */

    tPtr2 = tsdPtr->firstTimerHandlerPtr[timerHandlerIndex];
    for (prevPtr = NULL; tPtr2 != NULL;
	    prevPtr = tPtr2, tPtr2 = tPtr2->nextPtr) {
	if (timerHandlerPtr->time < tPtr2->time) {
	    break;
	}
    }
    timerHandlerPtr->nextPtr = tPtr2;
    if (prevPtr == NULL) {
	tsdPtr->firstTimerHandlerPtr[timerHandlerIndex] = timerHandlerPtr;
    } else {
	prevPtr->nextPtr = timerHandlerPtr;
    }

    TimerSetupProc(NULL, TCL_ALL_EVENTS);

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
				 * Tcl_DeleteTimerHandler. */
{
    TimerHandler *timerHandlerPtr, *prevPtr;
    ThreadSpecificData *tsdPtr = InitTimer();

    if (token == NULL) {
	return;
    }

    for (int timerHandlerIndex = timerHandlerMonotonic;
	    timerHandlerIndex <= timerHandlerWallclock; timerHandlerIndex++) {
	for (timerHandlerPtr = tsdPtr->firstTimerHandlerPtr[timerHandlerIndex], prevPtr = NULL;
		timerHandlerPtr != NULL; prevPtr = timerHandlerPtr,
		timerHandlerPtr = timerHandlerPtr->nextPtr) {
	    if (timerHandlerPtr->token != token) {
		continue;
	    }
	    if (prevPtr == NULL) {
		tsdPtr->firstTimerHandlerPtr[timerHandlerIndex] = timerHandlerPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = timerHandlerPtr->nextPtr;
	    }
	    Tcl_Free(timerHandlerPtr);
	    return;
	}
    }
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
    TCL_UNUSED(void *),
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    if (((flags & TCL_IDLE_EVENTS) && tsdPtr->idleList)
	    || ((flags & TCL_TIMER_EVENTS)
		&& (tsdPtr->timerPendingQueue[timerHandlerMonotonic]
		    || tsdPtr->timerPendingQueue[timerHandlerWallclock]))) {
	/*
	 * There is an idle handler or a pending timer event, so just poll.
	 */

	blockTime.sec = 0;
	blockTime.usec = 0;
    } else if ((flags & TCL_TIMER_EVENTS) &&
	    (tsdPtr->firstTimerHandlerPtr[timerHandlerMonotonic]
	    || tsdPtr->firstTimerHandlerPtr[timerHandlerWallclock])) {
	long long myBlockTimeUS;
	long long blockTimeUS;
	bool blockTimePresent = false;
	/*
	 * Find the earlier timeout of monotonic or wall clock
	 * of the next wallclock and/or monotonic timer on the list.
	 */

	for (int timerHandlerIndex = timerHandlerMonotonic;
		timerHandlerIndex <= timerHandlerWallclock; timerHandlerIndex++) {
	    long long myTimeUS;
	    TimerHandler *firstTimerHandlerPtrCur =
		    tsdPtr->firstTimerHandlerPtr[timerHandlerIndex];
	    if (firstTimerHandlerPtrCur == NULL) {
		continue;
	    }
	    if (timerHandlerIndex ==timerHandlerMonotonic) {
		myTimeUS = Tcl_GetMonotonicTime();
	    } else {
		myTimeUS = TclpGetMicroseconds();
	    }
	    myBlockTimeUS = firstTimerHandlerPtrCur->time - myTimeUS;
	    if (myBlockTimeUS < 0) {
		myBlockTimeUS = 0;
	    }
	    if (!blockTimePresent) {
		blockTimeUS = myBlockTimeUS;
		blockTimePresent = true;
	    } else {
		if (myBlockTimeUS < blockTimeUS) {
		    blockTimeUS = myBlockTimeUS;
		}
	    }
	}
	blockTime.sec = blockTimeUS / US_PER_S;
	blockTime.usec = blockTimeUS % US_PER_S;
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
    TCL_UNUSED(void *),
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    long long blockTimeUS;
    ThreadSpecificData *tsdPtr = InitTimer();

    if ((flags & TCL_TIMER_EVENTS) &&
		(tsdPtr->firstTimerHandlerPtr[timerHandlerMonotonic]
		|| tsdPtr->firstTimerHandlerPtr[timerHandlerWallclock])) {
	bool queueEvent = false;
	/*
	 * Compute the timeout for the next timer on one of the lists.
	 * Try wallclock and monotonic list.
	 */

	for (int timerHandlerIndex = timerHandlerMonotonic;
		timerHandlerIndex <= timerHandlerWallclock; timerHandlerIndex++ ) {
	    TimerHandler *firstTimerHandlerPtrCur =
		    tsdPtr->firstTimerHandlerPtr[timerHandlerIndex];
	    if (firstTimerHandlerPtrCur == NULL) {
		continue;
	    }
	    if (timerHandlerIndex == timerHandlerMonotonic) {
		blockTimeUS = Tcl_GetMonotonicTime();
	    } else {
		blockTimeUS = TclpGetMicroseconds();
	    }
	    blockTimeUS = firstTimerHandlerPtrCur->time - blockTimeUS;
	    if (blockTimeUS < 0) {
		blockTimeUS = 0;
	    }

	    /*
	     * If the first timer has expired, stick an event on the queue.
	     */

	    if (blockTimeUS == 0 &&
		    !tsdPtr->timerPendingQueue[timerHandlerIndex]) {
		tsdPtr->timerPendingQueue[timerHandlerIndex] = 1;
		queueEvent = true;
	    }
	}
	if (queueEvent) {
	    Tcl_Event *timerEvPtr = (Tcl_Event *)Tcl_Alloc(sizeof(Tcl_Event));
	    timerEvPtr->proc = TimerHandlerEventProc;
	    Tcl_QueueEvent(timerEvPtr, TCL_QUEUE_TAIL);
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
    TCL_UNUSED(Tcl_Event *),
    int flags)			/* Flags that indicate what events to handle,
				 * such as TCL_FILE_EVENTS. */
{
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
     *	  sources. This is implemented using the token number in the handler:
     *	  new handlers will have a newer token than any of the ones currently
     *	  on the list.
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

    for (int timerHandlerIndex = timerHandlerMonotonic;
	    timerHandlerIndex <= timerHandlerWallclock; timerHandlerIndex++) {
	long long timeUS;
	TimerHandler *timerHandlerPtr, **nextPtrPtr;
	int currentTimerId;

	if (!tsdPtr->timerPendingQueue[timerHandlerIndex]) {
	    continue;
	}
	tsdPtr->timerPendingQueue[timerHandlerIndex] = 0;

	/*
	 * As the queues are independent, but the timer ids are used for both,
	 * its creation time should be compared per queue.
	 * Thus, use the queues latest id.
	 */

	currentTimerId = tsdPtr->lastTimerIdQueue[timerHandlerIndex];

	if (timerHandlerIndex == timerHandlerMonotonic) {
	    timeUS = Tcl_GetMonotonicTime();
	} else {
	    timeUS = TclpGetMicroseconds();
	}
	while (1) {
	    nextPtrPtr = &tsdPtr->firstTimerHandlerPtr[timerHandlerIndex];
	    timerHandlerPtr = tsdPtr->firstTimerHandlerPtr[timerHandlerIndex];
	    if (timerHandlerPtr == NULL) {
		break;
	    }

	    if (timeUS < timerHandlerPtr->time) {
		break;
	    }

	    /*
	     * Bail out if the next timer is of a newer generation.
	     */

	    if ((currentTimerId - PTR2INT(timerHandlerPtr->token)) < 0) {
		break;
	    }

	    /*
	     * Remove the handler from the queue before invoking it, to avoid
	     * potential reentrancy problems.
	     */

	    *nextPtrPtr = timerHandlerPtr->nextPtr;
	    timerHandlerPtr->proc(timerHandlerPtr->clientData);
	    Tcl_Free(timerHandlerPtr);
	}
    }
    TimerSetupProc(NULL, TCL_TIMER_EVENTS);
    return 1;
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
    void *clientData)		/* Arbitrary value to pass to proc. */
{
    IdleHandler *idlePtr;
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    idlePtr = (IdleHandler *)Tcl_Alloc(sizeof(IdleHandler));
    idlePtr->proc = proc;
    idlePtr->clientData = clientData;
    idlePtr->generation = tsdPtr->idleGeneration;
    idlePtr->nextPtr = NULL;
    if (tsdPtr->lastIdlePtr == NULL) {
	tsdPtr->idleList = idlePtr;
    } else {
	tsdPtr->lastIdlePtr->nextPtr = idlePtr;
    }
    tsdPtr->lastIdlePtr = idlePtr;

    blockTime.sec = 0;
    blockTime.usec = 0;
    Tcl_SetMaxBlockTime(&blockTime);
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
    void *clientData)		/* Arbitrary value to pass to proc. */
{
    IdleHandler *idlePtr, *prevPtr, *nextPtr;
    ThreadSpecificData *tsdPtr = InitTimer();

    for (prevPtr = NULL, idlePtr = tsdPtr->idleList; idlePtr != NULL;
	    prevPtr = idlePtr, idlePtr = idlePtr->nextPtr) {
	while ((idlePtr->proc == proc)
		&& (idlePtr->clientData == clientData)) {
	    nextPtr = idlePtr->nextPtr;
	    Tcl_Free(idlePtr);
	    idlePtr = nextPtr;
	    if (prevPtr == NULL) {
		tsdPtr->idleList = idlePtr;
	    } else {
		prevPtr->nextPtr = idlePtr;
	    }
	    if (idlePtr == NULL) {
		tsdPtr->lastIdlePtr = prevPtr;
		return;
	    }
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
    int oldGeneration;
    Tcl_Time blockTime;
    ThreadSpecificData *tsdPtr = InitTimer();

    if (tsdPtr->idleList == NULL) {
	return 0;
    }

    oldGeneration = tsdPtr->idleGeneration;
    tsdPtr->idleGeneration++;

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

    for (idlePtr = tsdPtr->idleList;
	    ((idlePtr != NULL)
		    && ((oldGeneration - idlePtr->generation) >= 0));
	    idlePtr = tsdPtr->idleList) {
	tsdPtr->idleList = idlePtr->nextPtr;
	if (tsdPtr->idleList == NULL) {
	    tsdPtr->lastIdlePtr = NULL;
	}
	idlePtr->proc(idlePtr->clientData);
	Tcl_Free(idlePtr);
    }
    if (tsdPtr->idleList) {
	blockTime.sec = 0;
	blockTime.usec = 0;
	Tcl_SetMaxBlockTime(&blockTime);
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

int
Tcl_AfterObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_WideInt ms = 0;		/* Number of milliseconds to wait */
    int index = -1;
    static const char *const afterSubCmds[] = {
	"cancel", "idle", "info", NULL
    };
    enum afterSubCmdsEnum {AFTER_CANCEL, AFTER_IDLE, AFTER_INFO};
    Tcl_Obj *cmdObj;
    int res;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * First lets see if the command was passed a number as the first argument.
     */

    if (TclGetWideIntFromObj(NULL, objv[1], &ms) != TCL_OK) {
	if (Tcl_GetIndexFromObj(NULL, objv[1], afterSubCmds, "", 0, &index)
		!= TCL_OK) {
	    const char *arg = TclGetString(objv[1]);

	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "bad argument \"%s\": must be"
		    " cancel, idle, info, or an integer", arg));
	    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "INDEX", "argument",
		    arg, (char *)NULL);
	    return TCL_ERROR;
	}
    }

    /*
     * At this point, either index = -1 and ms contains the number of ms
     * to wait, or else index is the index of a subcommand.
     */

    switch (index) {
    case -1: {
	AfterInfo *afterPtr;
	AfterAssocData *assocPtr;
	ThreadSpecificData *tsdPtr;
	long long microSeconds;

	if (ms < 0) {
	    ms = 0;
	}

	long long wakeupUS;
	wakeupUS = Tcl_GetMonotonicTime();

	if (ms >= LLONG_MAX / US_PER_MS) {
	    TimeTooFarError(interp);
	    return TCL_ERROR;
	}
	microSeconds = ms * US_PER_MS;

	if (LLONG_MAX - microSeconds < wakeupUS) {
	    TimeTooFarError(interp);
	    return TCL_ERROR;
	}

	wakeupUS += microSeconds;

	if (objc == 2) {
	    /*
	     * No command given: wait the given monotonic time.
	     */

	    return TimerDelayMonotonic(interp, wakeupUS);
	}

	/*
	 * Invoke command after given monotonic time distance.
	 */

	assocPtr = TimerAssocDataGet(interp);
	tsdPtr = InitTimer();
	afterPtr = (AfterInfo *)Tcl_Alloc(sizeof(AfterInfo));
	afterPtr->assocPtr = assocPtr;
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

	afterPtr->id = tsdPtr->afterId;
	tsdPtr->afterId += 1;
	afterPtr->token = CreateTimerHandler(wakeupUS,
		AfterProc, afterPtr, true);
	afterPtr->nextPtr = assocPtr->firstAfterPtr;
	assocPtr->firstAfterPtr = afterPtr;
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("after#%d", afterPtr->id));
	return TCL_OK;
    }
    case AFTER_CANCEL:
	if (objc < 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "id|script ?script?");
	    return TCL_ERROR;
	}
	if (objc == 3) {
	    res = TimerCancelDo(interp, objv[2], false);
	} else {
	    cmdObj = Tcl_ConcatObj(objc-2, objv+2);
	    res = TimerCancelDo(interp, cmdObj, false);

	    /*
	     * When Tcl_ConcatObj was used, the created object is only
	     * decremented in this case, not in the other 3 cases in this
	     * function. I don't know why.
	     */

	    Tcl_DecrRefCount(cmdObj);
	}
	return res;
    case AFTER_IDLE:
	if (objc < 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "script ?script ...?");
	    return TCL_ERROR;
	}
	if (objc == 3) {
	    cmdObj = objv[2];
	} else {
	    cmdObj = Tcl_ConcatObj(objc-2, objv+2);
	}
	return TimerIdleDo(interp, cmdObj);
    case AFTER_INFO:
	if (objc < 2 || objc > 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?id?");
	    return TCL_ERROR;
	}
	return TimerInfoDo(interp, objc-1, objv+1, true);
    default:
	TCL_UNREACHABLE();
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerDelay --
 *
 *	Implements the blocking delay behaviour of [timer at $time],
 *	[timer in $delay] and [after $delay].
 *	Tricky because it has to take into account any time limit that has
 *	been set.
 *
 * Results:
 *	Standard Tcl result code (with error set if an error occurred due to a
 *	time limit being exceeded or being canceled).
 *
 * Side effects:
 *	May adjust the time limit granularity marker.
 *
 *----------------------------------------------------------------------
 */

static int
TimerDelay(
    Tcl_Interp *interp,
    long long endTimeUS)
{
    Interp *iPtr = (Interp *) interp;

    long long nowLimitUS;
    long long nowEventUS;
    long long diffUS, diffLimitUS;
    bool limitDiff;

    /*
     * Interpreter limits are expressed in wallclock time
     */

    nowLimitUS = TclpGetMicroseconds();
    nowEventUS = nowLimitUS;

    do {
	if (Tcl_AsyncReady()) {
	    if (Tcl_AsyncInvoke(interp, TCL_OK) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (Tcl_Canceled(interp, TCL_LEAVE_ERR_MSG) == TCL_ERROR) {
	    return TCL_ERROR;
	}
	if (iPtr->limit.timeEvent != NULL) {
	    long long limitUS = TCL_TIME_US(iPtr->limit.time);
	    if (limitUS < nowLimitUS) {
		iPtr->limit.granularityTicker = 0;
		if (Tcl_LimitCheck(interp) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	}

	/*
	 * Find event delay in micro-seconds
	 */

	diffUS = endTimeUS - nowEventUS;

	/*
	 * Flag if interp limit will fire
	 */

	limitDiff = false;

	/*
	 * Check for interpreter wall clock time limit
	 */

	if (iPtr->limit.timeEvent != NULL) {
	    long long limitUS = TCL_TIME_US(iPtr->limit.time);
	    diffLimitUS = limitUS - nowLimitUS;
	    if (diffLimitUS < diffUS) {
		/*
		 * Interpreter limit time will fire before the event limit.
		 * Update waiting time and remember, that the interpreter
		 * limit was the reason.
		 */

		/*
		 * If event timing was overwritten by limit, we wait at least 1ms.
		 */

		if (diffLimitUS < 1000) {
		    if (diffUS > 1000) {
			diffUS = 1000;
		    }
		} else {
		    diffUS = diffLimitUS;
		}
		limitDiff = true;
	    }
	}

	/*
	 * We recheck each 200 ms if wall clock may have jumped
	 */

	if (diffUS > TCL_TIME_MAXIMUM_SLICE) {
	    diffUS = TCL_TIME_MAXIMUM_SLICE;
	}

	if (diffUS > 0) {
	    Tcl_SleepMicroSeconds(diffUS);
	}

	if (limitDiff) {
	    /*
	     * Interpreter time limit limited the sleep time.
	     * Normally, we should exit below for the limit check.
	     */

	    if (Tcl_AsyncReady()) {
		if (Tcl_AsyncInvoke(interp, TCL_OK) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (Tcl_Canceled(interp, TCL_LEAVE_ERR_MSG) == TCL_ERROR) {
		return TCL_ERROR;
	    }
	    if (Tcl_LimitCheck(interp) != TCL_OK) {
		return TCL_ERROR;
	    }
	} else {
	    /*
	     * Event limit gave the sleep time.
	     * Check, if we slept correctly only, if sleep time is above
	     * this limit.
	     * This is for performance reasons to not call the time functions
	     * again below for no gain.
	     */

	    if (diffUS < SLEEP_OFFLOAD_GETTIMEOFDAY) {
		break;
	    }
	}

	/*
	 * We slept. Get new time base, to be compared below.
	 */

	nowLimitUS = TclpGetMicroseconds();
	nowEventUS = nowLimitUS;
    } while (nowEventUS < endTimeUS);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerDelayMonotonic --
 *
 *	Implements the blocking delay behaviour of [timer at $time],
 *	[timer in $delay] and [after $delay].
 *	Tricky because it has to take into account any time limit that has
 *	been set.
 *
 * Results:
 *	Standard Tcl result code (with error set if an error occurred due to a
 *	time limit being exceeded or being canceled).
 *
 * Side effects:
 *	May adjust the time limit granularity marker.
 *
 *----------------------------------------------------------------------
 */

static int
TimerDelayMonotonic(
    Tcl_Interp *interp,
    long long endTimeUS)
{
    Interp *iPtr = (Interp *) interp;
    long long nowLimitUS;
    long long nowEventUS;
    long long diffUS, diffLimitUS;
    bool limitDiff;

    /*
     * Interpreter limits are expressed in wallclock time
     */

    nowLimitUS = TclpGetMicroseconds();
    nowEventUS = Tcl_GetMonotonicTime();

    do {
	if (Tcl_AsyncReady()) {
	    if (Tcl_AsyncInvoke(interp, TCL_OK) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (Tcl_Canceled(interp, TCL_LEAVE_ERR_MSG) == TCL_ERROR) {
	    return TCL_ERROR;
	}
	if (iPtr->limit.timeEvent != NULL) {
	    long long limitUS = TCL_TIME_US(iPtr->limit.time);
	    if (limitUS < nowLimitUS) {
		iPtr->limit.granularityTicker = 0;
		if (Tcl_LimitCheck(interp) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	}

	/*
	 * Find event delay in micro-seconds
	 */

	diffUS = endTimeUS - nowEventUS;

	/*
	 * Remember, that the event limit is active
	 */

	limitDiff = false;

	/*
	 * Check for interpreter wall clock time limit
	 */

	if (iPtr->limit.timeEvent != NULL) {
	    long long limitUS = TCL_TIME_US(iPtr->limit.time);
	    diffLimitUS = limitUS - nowLimitUS;
	    if (diffLimitUS < diffUS) {
		/*
		 * Interpreter limit time will fire before the event limit.
		 * Update waiting time and remember, that the interpreter
		 * limit was the reason.
		 */

		/*
		 * If event timing was overwritten by limit, we wait at least 1ms.
		 */

		if (diffLimitUS < 1000) {
		    if (diffUS > 1000) {
			diffUS = 1000;
		    }
		} else {
		    diffUS = diffLimitUS;
		}
		limitDiff = true;
	    }

	    /*
	     * We recheck each 200 ms if wall clock may have jumped
	     */

	    if (diffUS > TCL_TIME_MAXIMUM_SLICE) {
		diffUS = TCL_TIME_MAXIMUM_SLICE;
	    }
	}

	if (diffUS > 0) {
	    Tcl_SleepMicroSeconds(diffUS);
	}

	if (limitDiff) {
	    /*
	     * Interpreter time limit limited the sleep time.
	     * Normally, we should exit below for the limit check.
	     */

	    if (Tcl_AsyncReady()) {
		if (Tcl_AsyncInvoke(interp, TCL_OK) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    if (Tcl_Canceled(interp, TCL_LEAVE_ERR_MSG) == TCL_ERROR) {
		return TCL_ERROR;
	    }
	    if (Tcl_LimitCheck(interp) != TCL_OK) {
		return TCL_ERROR;
	    }
	}

	/*
	 * We slept. Get new time base, to be compared below.
	 */

	nowLimitUS = TclpGetMicroseconds();
	nowEventUS = Tcl_GetMonotonicTime();
    } while (nowEventUS < endTimeUS);
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
    Tcl_Obj *commandPtr)
{
    const char *cmdString;	/* Textual identifier for after event, such as
				 * "after#6". */
    AfterInfo *afterPtr;
    int id;
    char *end;

    cmdString = TclGetString(commandPtr);
    if (strncmp(cmdString, "after#", 6) != 0) {
	return NULL;
    }
    cmdString += 6;
    id = (int)strtoul(cmdString, &end, 10);
    if ((end == cmdString) || (*end != 0)) {
	return NULL;
    }
    for (afterPtr = assocPtr->firstAfterPtr; afterPtr != NULL;
	    afterPtr = afterPtr->nextPtr) {
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
    void *clientData)		/* Describes command to execute. */
{
    AfterInfo *afterPtr = (AfterInfo *)clientData;
    AfterAssocData *assocPtr = afterPtr->assocPtr;
    AfterInfo *prevPtr;
    int result;
    Tcl_Interp *interp;

    /*
     * First remove the callback from our list of callbacks; otherwise someone
     * could delete the callback while it's being executed, which could cause
     * a core dump.
     */

    if (assocPtr->firstAfterPtr == afterPtr) {
	assocPtr->firstAfterPtr = afterPtr->nextPtr;
    } else {
	for (prevPtr = assocPtr->firstAfterPtr; prevPtr->nextPtr != afterPtr;
		prevPtr = prevPtr->nextPtr) {
	    /* Empty loop body. */
	}
	prevPtr->nextPtr = afterPtr->nextPtr;
    }

    /*
     * Execute the callback.
     */

    interp = assocPtr->interp;
    Tcl_Preserve(interp);
    result = Tcl_EvalObjEx(interp, afterPtr->commandPtr, TCL_EVAL_GLOBAL);
    if (result != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (\"after\" script)");
	Tcl_BackgroundException(interp, result);
    }
    Tcl_Release(interp);

    /*
     * Free the memory for the callback.
     */

    Tcl_DecrRefCount(afterPtr->commandPtr);
    Tcl_Free(afterPtr);
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
 *	The memory associated with afterPtr is released.
 *
 *----------------------------------------------------------------------
 */

static void
FreeAfterPtr(
    AfterInfo *afterPtr)	/* Command to be deleted. */
{
    AfterAssocData *assocPtr = afterPtr->assocPtr;

    if (assocPtr->firstAfterPtr == afterPtr) {
	assocPtr->firstAfterPtr = afterPtr->nextPtr;
    } else {
	AfterInfo *prevPtr;
	for (prevPtr = assocPtr->firstAfterPtr; prevPtr->nextPtr != afterPtr;
		prevPtr = prevPtr->nextPtr) {
	    /* Empty loop body. */
	}
	prevPtr->nextPtr = afterPtr->nextPtr;
    }
    Tcl_DecrRefCount(afterPtr->commandPtr);
    Tcl_Free(afterPtr);
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

static void
AfterCleanupProc(
    void *clientData,		/* Points to AfterAssocData for the
				 * interpreter. */
    TCL_UNUSED(Tcl_Interp *))
{
    AfterAssocData *assocPtr = (AfterAssocData *)clientData;

    while (assocPtr->firstAfterPtr != NULL) {
	AfterInfo *afterPtr = assocPtr->firstAfterPtr;
	assocPtr->firstAfterPtr = afterPtr->nextPtr;
	if (afterPtr->token != NULL) {
	    Tcl_DeleteTimerHandler(afterPtr->token);
	} else {
	    Tcl_CancelIdleCall(AfterProc, afterPtr);
	}
	Tcl_DecrRefCount(afterPtr->commandPtr);
	Tcl_Free(afterPtr);
    }
    Tcl_Free(assocPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ParseTimeUnit --
 *
 *	Parse a value and unit time argument and return it as Tcl_Time.
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	On TCL_ERROR, an error message is left in the interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
ParseTimeUnit(
    Tcl_Interp *interp,		/* Current interpreter. */
    int timeIndex,		/* Index of time value in objv. */
    Tcl_Obj *const objv[],	/* Argument objects. */
    bool useDefaultUnit,	/* True, if the default unit is used.
				 * False, if unit object follows the time value */
    enum unitEnum defaultUnit,	/* Default unit value */
    long long *wakeupPtr)	/* Output time */
{
    Tcl_WideInt timeArg;
    int unitIndex;

    /*
     * Get the time point to wait
     */

    if (TclGetWideIntFromObj(interp, objv[timeIndex], &timeArg) != TCL_OK) {
	return TCL_ERROR;
    }
    if (timeArg < 0) {
	timeArg = 0;
    }

    /*
     * Get the unit of the time point. Allow abbreviations.
     */

    if (useDefaultUnit) {
	unitIndex = defaultUnit;
    } else {
	if (Tcl_GetIndexFromObj(interp, objv[timeIndex+1], unitItems, "unit", 0,
		&unitIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    /*
     * Create time value by applying the unit to the value
     */

    switch(unitIndex) {
    case UNIT_MICROSECONDS:
    case UNIT_US:
	*wakeupPtr = timeArg;
	break;
    case UNIT_MILLISECONDS:
    case UNIT_MS:
	if (timeArg >= LLONG_MAX / US_PER_MS) {
	    TimeTooFarError(interp);
	    return TCL_ERROR;
	}
	*wakeupPtr = timeArg * US_PER_MS;
	break;
    default:
	if (timeArg >= LLONG_MAX / US_PER_S) {
	    TimeTooFarError(interp);
	    return TCL_ERROR;
	}
	*wakeupPtr = timeArg * US_PER_S;
	break;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerAtCmd --
 *
 *	This procedure implements the "timer at" Tcl command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerAtCmd(
    TCL_UNUSED(void *),		/* Client data. */
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    long long timeArgUS;
    AfterInfo *afterPtr;
    AfterAssocData *assocPtr = TimerAssocDataGet(interp);
    ThreadSpecificData *tsdPtr = InitTimer();

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "time unit script");
	return TCL_ERROR;
    }

    if (TCL_OK != ParseTimeUnit(interp, 1, objv, false, UNIT_US, &timeArgUS)) {
	return TCL_ERROR;
    }

    afterPtr = (AfterInfo *)Tcl_Alloc(sizeof(AfterInfo));
    afterPtr->assocPtr = assocPtr;
    afterPtr->commandPtr = objv[3];
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

    afterPtr->id = tsdPtr->afterId;
    tsdPtr->afterId += 1;
    afterPtr->token = CreateTimerHandler(timeArgUS,
	    AfterProc, afterPtr, false);
    afterPtr->nextPtr = assocPtr->firstAfterPtr;
    assocPtr->firstAfterPtr = afterPtr;
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("after#%d", afterPtr->id));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TimeTooFarError --
 *
 *	Set the time too far error in the interpreter.
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
TimeTooFarError(
    Tcl_Interp *interp)		/* Current interpreter. */
{
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"time too far away", -1));
	Tcl_SetErrorCode(interp, "TCL","TIME","OVERFLOW", (char *)NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TimerInCmd --
 *
 *	This procedure implements the "timer in" Tcl command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerInCmd(
    TCL_UNUSED(void *),		/* Client data. */
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    long long wakeupArgUS;
    AfterInfo *afterPtr;
    AfterAssocData *assocPtr = TimerAssocDataGet(interp);
    ThreadSpecificData *tsdPtr = InitTimer();

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "time unit script");
	return TCL_ERROR;
    }

    if (TCL_OK != ParseTimeUnit(interp, 1, objv, false, UNIT_US, &wakeupArgUS)) {
	return TCL_ERROR;
    }

    /*
     * Sum current time and time argument
     */

    long long wakeupUS;
    wakeupUS = Tcl_GetMonotonicTime();

    if (LLONG_MAX - wakeupUS < wakeupArgUS) {
	TimeTooFarError(interp);
	return TCL_ERROR;
    }
    wakeupUS += wakeupArgUS;

    if (objc < 4) {
	/*
	 * No script given - just wait the monotonic time
	 */

	return TimerDelayMonotonic(interp, wakeupUS);
    }

    afterPtr = (AfterInfo *)Tcl_Alloc(sizeof(AfterInfo));
    afterPtr->assocPtr = assocPtr;
    afterPtr->commandPtr = objv[3];
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

    afterPtr->id = tsdPtr->afterId;
    tsdPtr->afterId += 1;
    afterPtr->token = CreateTimerHandler(wakeupUS,
	    AfterProc, afterPtr, true);
    afterPtr->nextPtr = assocPtr->firstAfterPtr;
    assocPtr->firstAfterPtr = afterPtr;
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("after#%d", afterPtr->id));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerSleepCmd --
 *
 *	This procedure implements the "timer sleep" Tcl command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerSleepCmd(
    TCL_UNUSED(void *),		/* Client data. */
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    long long sleepArgUS;
    static const char *const sleepOptionItems[] = {
	    "for", "until", NULL};
    enum sleepOptionEnum {MODE_FOR, MODE_UNTIL};
    int optionIndex;

    if (objc < 3 || objc > 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "option time ?unit?");
	return TCL_ERROR;
    }

    /*
     * Get the sleep mode
     */

    if (Tcl_GetIndexFromObj(interp, objv[1], sleepOptionItems, "option", 1,
	    &optionIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Parse the unit. The default unit is milli-seconds for "sleep for"
     * and seconds for "sleep until".
     */

    if (TCL_OK != ParseTimeUnit(interp, 2, objv, objc == 3,
	    optionIndex==MODE_FOR? UNIT_MS: UNIT_S,
	    &sleepArgUS)) {
	return TCL_ERROR;
    }

    switch (optionIndex) {
    case MODE_FOR:
	return TimerSleepForCmd(interp, sleepArgUS);
    default:
	return TimerSleepUntilCmd(interp, sleepArgUS);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TimerSleepForCmd --
 *
 *	This procedure implements the "timer sleep for" Tcl command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerSleepForCmd(
    Tcl_Interp *interp,		/* Current interpreter. */
    long long sleepArgUS)	/* Sleeping time distance in us. */
{
    /*
     * Sum current time and time argument
     */

    long long monotonicTimeUS = Tcl_GetMonotonicTime();

    if (LLONG_MAX - monotonicTimeUS < sleepArgUS) {
	TimeTooFarError(interp);
	return TCL_ERROR;
    }
    monotonicTimeUS += sleepArgUS;

    return TimerDelayMonotonic(interp, monotonicTimeUS);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerSleepUntilCmd --
 *
 *	This procedure implements the "timer sleep until" Tcl command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerSleepUntilCmd(
    Tcl_Interp *interp,		/* Current interpreter. */
    long long timeArgUS)	/* Sleeping time point in us. */
{
    return TimerDelay(interp, timeArgUS);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerAssocDataGet --
 *
 *	Create the "timer/after" information associated for this interpreter,
 *	if it doesn't already exist.
 *
 * Results:
 *	A pointer to the associated data.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static AfterAssocData *
TimerAssocDataGet(
    Tcl_Interp *interp)		/* Current interpreter. */
{
    AfterAssocData *assocPtr = (AfterAssocData *)
	    Tcl_GetAssocData(interp, ASSOC_KEY, NULL);
    if (assocPtr == NULL) {
	assocPtr = (AfterAssocData *)Tcl_Alloc(sizeof(AfterAssocData));
	assocPtr->interp = interp;
	assocPtr->firstAfterPtr = NULL;
	Tcl_SetAssocData(interp, ASSOC_KEY, AfterCleanupProc, assocPtr);
    }
    return assocPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerCancelCmd --
 *
 *	This function is invoked to process the "timer cancel" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerCancelCmd(
    TCL_UNUSED(void *),		/* Client data. */
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "id|script");
	return TCL_ERROR;
    }

    return TimerCancelDo(interp, objv[1], true);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerCancelDo --
 *
 *	Execute "after cancel" and "timer cancel".
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerCancelDo(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *const objArg,	/* Id or command. */
    bool isTimerCancel)		/* true, if "timer cancel" and not "after cancel" */
{
    const char *command, *tempCommand;
    Tcl_Size tempLength;

    AfterInfo *afterPtr = NULL;
    AfterAssocData *assocPtr = TimerAssocDataGet(interp);
    Tcl_Size length;

    /*
     * "after cancel" also searches for the command name
     */

    if (!isTimerCancel) {
	command = TclGetStringFromObj(objArg, &length);
	for (afterPtr = assocPtr->firstAfterPtr;  afterPtr != NULL;
		afterPtr = afterPtr->nextPtr) {
	    tempCommand = TclGetStringFromObj(afterPtr->commandPtr,
		    &tempLength);
	    if ((length == tempLength)
		    && !memcmp(command, tempCommand, length)) {
		break;
	    }
	}
    }

    /*
     * Search for the after Id
     */

    if (afterPtr == NULL) {
	afterPtr = GetAfterEvent(assocPtr, objArg);
    }

    /*
     * Delete the after event if found
     */

    if (afterPtr != NULL) {
	if (afterPtr->token != NULL) {
	    Tcl_DeleteTimerHandler(afterPtr->token);
	} else {
	    Tcl_CancelIdleCall(AfterProc, afterPtr);
	}
	FreeAfterPtr(afterPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerIdleCmd --
 *
 *	This function is invoked to process the "after/timer idle" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerIdleCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "script");
	return TCL_ERROR;
    }
    return TimerIdleDo(interp, objv[1]);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerIdleDo--
 *
 *	Execute the "after/timer idle" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerIdleDo(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *const objCmd)	/* Idle command. */
{
    AfterInfo *afterPtr;
    AfterAssocData *assocPtr = TimerAssocDataGet(interp);
    ThreadSpecificData *tsdPtr = InitTimer();

    afterPtr = (AfterInfo *)Tcl_Alloc(sizeof(AfterInfo));
    afterPtr->assocPtr = assocPtr;
    afterPtr->commandPtr = objCmd;
    Tcl_IncrRefCount(afterPtr->commandPtr);
    afterPtr->id = tsdPtr->afterId;
    tsdPtr->afterId += 1;
    afterPtr->token = NULL;
    afterPtr->nextPtr = assocPtr->firstAfterPtr;
    assocPtr->firstAfterPtr = afterPtr;
    Tcl_DoWhenIdle(AfterProc, afterPtr);
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("after#%d", afterPtr->id));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerInfoCmd --
 *
 *	This function is invoked to process the "timer info" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerInfoCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc < 1 || objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?id?");
	return TCL_ERROR;
    }
    return TimerInfoDo(interp, objc, objv, false);
}

/*
 *----------------------------------------------------------------------
 *
 * TimerInfoDo --
 *
 *	Implement "after info" and "timer info". The argument "isAfter"
 *	is true, if called from "after info", otherwise false.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
TimerInfoDo(
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[],	/* Argument objects. */
    bool isAfter)
{
    AfterInfo *afterPtr;
    AfterAssocData *assocPtr = TimerAssocDataGet(interp);

    if (objc == 1) {
	Tcl_Obj *resultObj;

	/*
	 * Return the list of the IDs
	 */

	TclNewObj(resultObj);
	for (afterPtr = assocPtr->firstAfterPtr; afterPtr != NULL;
		afterPtr = afterPtr->nextPtr) {
	    if (assocPtr->interp == interp) {
		Tcl_ListObjAppendElement(NULL, resultObj, Tcl_ObjPrintf(
			"after#%d", afterPtr->id));
	    }
	}
	Tcl_SetObjResult(interp, resultObj);
	return TCL_OK;
    }

    /*
     * Return information on the given event id string
     */

    afterPtr = GetAfterEvent(assocPtr, objv[1]);
    if (afterPtr == NULL) {
	const char *eventStr = TclGetString(objv[1]);

	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"event \"%s\" doesn't exist", eventStr));
	Tcl_SetErrorCode(interp, "TCL","LOOKUP","EVENT", eventStr, (char *)NULL);
	return TCL_ERROR;
    }

    Tcl_Obj *resultListPtr;

    TclNewObj(resultListPtr);
    Tcl_ListObjAppendElement(interp, resultListPtr,
	    afterPtr->commandPtr);
    if (NULL == afterPtr->token) {
	/*
	 * Type: idle: the return value is identical for "after info"
	 * and "timer info"
	 */

	Tcl_ListObjAppendElement(interp, resultListPtr,
		Tcl_NewStringObj("idle", -1));
    } else {
	/*
	 * wall clock and monotonic timers have different return values for
	 * after info or timer info:
	 * after info: Cmd "timer"
	 * timer info: Cmd "monotonic/real" us
	 */

	if (isAfter) {
	    Tcl_ListObjAppendElement(interp, resultListPtr,
		    Tcl_NewStringObj("timer", -1));
	} else {
	    bool found = false;
	    TimerHandler *timerHandlerPtr;
	    ThreadSpecificData *tsdPtr = InitTimer();

	    /*
	     * Walk through the queues to find this token
	     */

	    for (int timerHandlerIndex = timerHandlerMonotonic;
		    timerHandlerIndex <=timerHandlerWallclock && ! found;
		    timerHandlerIndex++) {
		timerHandlerPtr = tsdPtr->firstTimerHandlerPtr[timerHandlerIndex];
		while (timerHandlerPtr != NULL) {
		    if (timerHandlerPtr->token == afterPtr->token) {
			Tcl_ListObjAppendElement(interp, resultListPtr,
				Tcl_NewStringObj(
				    (timerHandlerIndex == timerHandlerMonotonic ?
				    "monotonic" : "wallclock"),
				    TCL_AUTO_LENGTH));

			Tcl_ListObjAppendElement(interp, resultListPtr,
				Tcl_NewWideIntObj(timerHandlerPtr->time));
			found = true;
			break;
		    }
		}
	    }

	    if (!found) {
		/*
		 * Token not found -> this should not happen
		 */

		Tcl_Panic("Timer token not fount!");
	    }
	}
    }
    Tcl_SetObjResult(interp, resultListPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetTimeProc --
 *
 *	TIP #233 (Virtualized Time): Registers two handlers for the
 *	virtualization of Tcl's access to time information.
 *	Not supported, so call panic.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Remembers the handlers, alters core behaviour.
 *
 *----------------------------------------------------------------------
 */

#ifndef TCL_NO_DEPRECATED
void
Tcl_SetTimeProc(
    TCL_UNUSED(Tcl_GetTimeProc *),
    TCL_UNUSED(Tcl_ScaleTimeProc *),
    TCL_UNUSED(void *))
{
    Tcl_Panic("Tcl_SetTimeProc is not supported in TCL 9.1");
}
#endif /* TCL_NO_DEPRECATED */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_QueryTimeProc --
 *
 *	TIP #233 (Virtualized Time): Query which time handlers are registered.
 *	Not supported, so call panic.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifndef TCL_NO_DEPRECATED
void
Tcl_QueryTimeProc(
    TCL_UNUSED(Tcl_GetTimeProc **),
    TCL_UNUSED(Tcl_ScaleTimeProc **),
    TCL_UNUSED(void **))
{
    Tcl_Panic("Tcl_QueryTimeProc is not supported in TCL 9.1");
}
#endif /* TCL_NO_DEPRECATED */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 */
