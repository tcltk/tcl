/*
 * tclWinNotify.c --
 *
 *	This file contains Windows-specific procedures for the notifier, which
 *	is the lowest-level part of the Tcl event loop. This file works
 *	together with ../generic/tclNotify.c.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * The follwing static indicates whether this module has been initialized.
 */

#define INTERVAL_TIMER	1	/* Handle of interval timer. */

#define WM_WAKEUP	WM_USER	/* Message that is send by
				 * Tcl_AlertNotifier. */

/*
 * The following static structure contains the state information for the
 * Windows implementation of the Tcl notifier. One of these structures is
 * created for each thread that is using the notifier.
 */

typedef struct ThreadSpecificData {
    CRITICAL_SECTION crit;	/* Monitor for this notifier. */
    DWORD thread;		/* Identifier for thread associated with this
				 * notifier. */
    HANDLE event;		/* Event object used to wake up the notifier
				 * thread. */
    int pending;		/* Alert message pending, this field is locked
				 * by the notifierMutex. */
    HWND hwnd;			/* Messaging window. */
    int timeout;		/* Current timeout value. */
    int timerActive;		/* 1 if interval timer is running. */
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

extern TclStubs tclStubs;
extern Tcl_NotifierProcs tclOriginalNotifier;

/*
 * The following static indicates the number of threads that have initialized
 * notifiers. It controls the lifetime of the TclNotifier window class.
 *
 * You must hold the notifierMutex lock before accessing this variable.
 */

static int notifierCount = 0;
TCL_DECLARE_MUTEX(notifierMutex)

/*
 * Static routines defined in this file.
 */

static LRESULT CALLBACK		NotifierProc(HWND hwnd, UINT message,
				    WPARAM wParam, LPARAM lParam);

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InitNotifier --
 *
 *	Initializes the platform specific notifier state.
 *
 * Results:
 *	Returns a handle to the notifier state for this thread..
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_InitNotifier(void)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    WNDCLASS class;

    /*
     * Register Notifier window class if this is the first thread to use this
     * module.
     */

    Tcl_MutexLock(&notifierMutex);
    if (notifierCount == 0) {
	class.style = 0;
	class.cbClsExtra = 0;
	class.cbWndExtra = 0;
	class.hInstance = TclWinGetTclInstance();
	class.hbrBackground = NULL;
	class.lpszMenuName = NULL;
	class.lpszClassName = "TclNotifier";
	class.lpfnWndProc = NotifierProc;
	class.hIcon = NULL;
	class.hCursor = NULL;

	if (!RegisterClassA(&class)) {
	    Tcl_Panic("Unable to register TclNotifier window class");
	}
    }
    notifierCount++;
    Tcl_MutexUnlock(&notifierMutex);

    tsdPtr->pending = 0;
    tsdPtr->timerActive = 0;

    InitializeCriticalSection(&tsdPtr->crit);

    tsdPtr->hwnd = NULL;
    tsdPtr->thread = GetCurrentThreadId();
    tsdPtr->event = CreateEvent(NULL, FALSE /* !manual */,
	    FALSE /* !signaled */, NULL);

    return (ClientData) tsdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FinalizeNotifier --
 *
 *	This function is called to cleanup the notifier state before a thread
 *	is terminated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May dispose of the notifier window and class.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FinalizeNotifier(
    ClientData clientData)	/* Pointer to notifier data. */
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *) clientData;

    /*
     * Only finalize the notifier if a notifier was installed in the current
     * thread; there is a route in which this is not guaranteed to be true
     * (when tclWin32Dll.c:DllMain() is called with the flag
     * DLL_PROCESS_DETACH by the OS, which could be doing so from a thread
     * that's never previously been involved with Tcl, e.g. the task manager)
     * so this check is important.
     *
     * Fixes Bug #217982 reported by Hugh Vu and Gene Leache.
     */

    if (tsdPtr == NULL) {
	return;
    }

    DeleteCriticalSection(&tsdPtr->crit);
    CloseHandle(tsdPtr->event);

    /*
     * Clean up the timer and messaging window for this thread.
     */

    if (tsdPtr->hwnd) {
	KillTimer(tsdPtr->hwnd, INTERVAL_TIMER);
	DestroyWindow(tsdPtr->hwnd);
    }

    /*
     * If this is the last thread to use the notifier, unregister the notifier
     * window class.
     */

    Tcl_MutexLock(&notifierMutex);
    notifierCount--;
    if (notifierCount == 0) {
	UnregisterClassA("TclNotifier", TclWinGetTclInstance());
    }
    Tcl_MutexUnlock(&notifierMutex);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AlertNotifier --
 *
 *	Wake up the specified notifier from any thread. This routine is called
 *	by the platform independent notifier code whenever the Tcl_ThreadAlert
 *	routine is called. This routine is guaranteed not to be called on a
 *	given notifier after Tcl_FinalizeNotifier is called for that notifier.
 *	This routine is typically called from a thread other than the
 *	notifier's thread.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sends a message to the messaging window for the notifier if there
 *	isn't already one pending.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AlertNotifier(
    ClientData clientData)	/* Pointer to thread data. */
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *) clientData;

    /*
     * Note that we do not need to lock around access to the hwnd because the
     * race condition has no effect since any race condition implies that the
     * notifier thread is already awake.
     */

    if (tsdPtr->hwnd) {
	/*
	 * We do need to lock around access to the pending flag.
	 */

	EnterCriticalSection(&tsdPtr->crit);
	if (!tsdPtr->pending) {
	    PostMessage(tsdPtr->hwnd, WM_WAKEUP, 0, 0);
	}
	tsdPtr->pending = 1;
	LeaveCriticalSection(&tsdPtr->crit);
    } else {
	SetEvent(tsdPtr->event);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetTimer --
 *
 *	This procedure sets the current notifier timer value. The notifier
 *	will ensure that Tcl_ServiceAll() is called after the specified
 *	interval, even if no events have occurred.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Replaces any previous timer.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetTimer(
    Tcl_Time *timePtr)		/* Maximum block time, or NULL. */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    UINT timeout;

    /*
     * Allow the notifier to be hooked. This may not make sense on Windows,
     * but mirrors the UNIX hook.
     */

    if (tclStubs.tcl_SetTimer != tclOriginalNotifier.setTimerProc) {
	tclStubs.tcl_SetTimer(timePtr);
	return;
    }

    /*
     * We only need to set up an interval timer if we're being called from an
     * external event loop. If we don't have a window handle then we just
     * return immediately and let Tcl_WaitForEvent handle timeouts.
     */

    if (!tsdPtr->hwnd) {
	return;
    }

    if (!timePtr) {
	timeout = 0;
    } else {
	/*
	 * Make sure we pass a non-zero value into the timeout argument.
	 * Windows seems to get confused by zero length timers.
	 */

	timeout = timePtr->sec * 1000 + timePtr->usec / 1000;
	if (timeout == 0) {
	    timeout = 1;
	}
    }
    tsdPtr->timeout = timeout;
    if (timeout != 0) {
	tsdPtr->timerActive = 1;
	SetTimer(tsdPtr->hwnd, INTERVAL_TIMER, (unsigned long) tsdPtr->timeout,
		NULL);
    } else {
	tsdPtr->timerActive = 0;
	KillTimer(tsdPtr->hwnd, INTERVAL_TIMER);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ServiceModeHook --
 *
 *	This function is invoked whenever the service mode changes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If this is the first time the notifier is set into TCL_SERVICE_ALL,
 *	then the communication window is created.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ServiceModeHook(
    int mode)			/* Either TCL_SERVICE_ALL, or
				 * TCL_SERVICE_NONE. */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    /*
     * If this is the first time that the notifier has been used from a modal
     * loop, then create a communication window. Note that after this point,
     * the application needs to service events in a timely fashion or Windows
     * will hang waiting for the window to respond to synchronous system
     * messages. At some point, we may want to consider destroying the window
     * if we leave the modal loop, but for now we'll leave it around.
     */

    if (mode == TCL_SERVICE_ALL && !tsdPtr->hwnd) {
	tsdPtr->hwnd = CreateWindowA("TclNotifier", "TclNotifier", WS_TILED,
		0, 0, 0, 0, NULL, NULL, TclWinGetTclInstance(), NULL);

	/*
	 * Send an initial message to the window to ensure that we wake up the
	 * notifier once we get into the modal loop. This will force the
	 * notifier to recompute the timeout value and schedule a timer if one
	 * is needed.
	 */

	Tcl_AlertNotifier((ClientData)tsdPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * NotifierProc --
 *
 *	This procedure is invoked by Windows to process events on the notifier
 *	window. Messages will be sent to this window in response to external
 *	timer events or calls to TclpAlertTsdPtr->
 *
 * Results:
 *	A standard windows result.
 *
 * Side effects:
 *	Services any pending events.
 *
 *----------------------------------------------------------------------
 */

static LRESULT CALLBACK
NotifierProc(
    HWND hwnd,			/* Passed on... */
    UINT message,		/* What messsage is this? */
    WPARAM wParam,		/* Passed on... */
    LPARAM lParam)		/* Passed on... */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (message == WM_WAKEUP) {
	EnterCriticalSection(&tsdPtr->crit);
	tsdPtr->pending = 0;
	LeaveCriticalSection(&tsdPtr->crit);
    } else if (message != WM_TIMER) {
	return DefWindowProc(hwnd, message, wParam, lParam);
    }

    /*
     * Process all of the runnable events.
     */

    Tcl_ServiceAll();
    return 0;
}

/*
 * Timer resolution primitives
 */

typedef int (CALLBACK* LPFN_NtQueryTimerResolution)(PULONG,PULONG,PULONG);
typedef int (CALLBACK* LPFN_NtSetTimerResolution)(ULONG,BOOLEAN,PULONG);

static LPFN_NtQueryTimerResolution NtQueryTimerResolution = NULL;
static LPFN_NtSetTimerResolution NtSetTimerResolution = NULL;

#define TMR_RES_MICROSEC (1000 / 100)

static struct {
    int   available;		/* Availability of timer resolution functions */
    ULONG minRes;		/* Lowest possible resolution (in 100-ns) */
    ULONG maxRes;		/* Highest possible resolution (in 100-ns) */
    ULONG curRes;		/* Current resolution (in 100-ns units). */
    ULONG resRes;		/* Resolution to be restored (delayed restore) */
    LONG  minDelay;		/* Lowest delay by max resolution (in microsecs) */
    LONG  maxDelay;		/* Highest delay by min resolution (in microsecs) */
    size_t count;		/* Waiter count (used to restore the resolution) */
    Tcl_WideInt timeToReset;	/* Time to reset resolution (typically now + 5s) */
    CRITICAL_SECTION cs;	/* Mutex guarding this structure. */
} timerResolution = {
    -1,
    15600 * TMR_RES_MICROSEC, 500 * TMR_RES_MICROSEC, 
    15600 * TMR_RES_MICROSEC, 0,
    500, 15600,
    0,
    (Tcl_WideInt)0x7FFFFFFFFFFFFFFFL
};

/*
 *----------------------------------------------------------------------
 *
 * InitTimerResolution --
 *
 *	This function initializes the timer resolution functionality.
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
InitTimerResolution(void)
{
    if (timerResolution.available == -1) {
    	TclpInitLock();
    	if (timerResolution.available == -1) {
    	    HMODULE hLib = GetModuleHandle("Ntdll");
	    if (hLib) {
		NtQueryTimerResolution = 
		    (LPFN_NtQueryTimerResolution)GetProcAddress(hLib, 
		    	"NtQueryTimerResolution");
		NtSetTimerResolution = 
		    (LPFN_NtSetTimerResolution)GetProcAddress(hLib, 
		    	"NtSetTimerResolution");

		if ( NtSetTimerResolution && NtQueryTimerResolution
		  && NtQueryTimerResolution(&timerResolution.minRes,
				&timerResolution.maxRes, &timerResolution.curRes) == 0
		) {
		    InitializeCriticalSection(&timerResolution.cs);
		    timerResolution.resRes = timerResolution.curRes;
		    timerResolution.minRes -= (timerResolution.minRes % TMR_RES_MICROSEC);
		    timerResolution.minDelay = timerResolution.maxRes / TMR_RES_MICROSEC;
		    timerResolution.maxDelay = timerResolution.minRes / TMR_RES_MICROSEC;
		    if (timerResolution.maxRes <= 1000 * TMR_RES_MICROSEC) {
		    	timerResolution.available = 1;
		    }
		}
	    }
	    if (timerResolution.available <= 0) {
	    	/* not available, set min/max to typical values on windows */ 
		timerResolution.minRes = 15600 * TMR_RES_MICROSEC;
		timerResolution.maxRes = 500 * TMR_RES_MICROSEC;
		timerResolution.minDelay = timerResolution.maxRes / TMR_RES_MICROSEC;
		timerResolution.maxDelay = timerResolution.minRes / TMR_RES_MICROSEC;
		timerResolution.available = 0;
	    }
	}
	TclpInitUnlock();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SetTimerResolution --
 *
 *	This is called by Tcl_WaitForEvent to increase timer resolution if wait
 *	for events with time smaller as the typical windows value (ca. 15ms).
 *
 * Results:
 *	Returns previous value of timer resolution, used for restoring with
 *	RestoreTimerResolution.
 *
 * Side effects:
 *	Note that timer resolution takes affect for the whole process (accross
 *	all threads).
 *
 *----------------------------------------------------------------------
 */

static unsigned long
SetTimerResolution(
    unsigned long newResolution,
    unsigned long actualResolution
) {
    /* if available */
    if (timerResolution.available > 0)
    {
	if (newResolution < timerResolution.maxRes) {
	    newResolution = timerResolution.maxRes;
	}
	EnterCriticalSection(&timerResolution.cs);
	if (!actualResolution) {
	    timerResolution.count++;
	    actualResolution = timerResolution.curRes;
	}
	if (newResolution < timerResolution.curRes) {
	    ULONG curRes;
	    if (NtSetTimerResolution(newResolution, TRUE, &curRes) == 0) {
		timerResolution.curRes = curRes;
	    }
	}
	LeaveCriticalSection(&timerResolution.cs);
	return actualResolution;
    }

    /* resolution unchanged (and counter not increased) */
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * RestoreTimerResolution --
 *
 *	This is called by Tcl_WaitForEvent to restore timer resolution to
 *	previous value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Because timer resolution takes affect for the whole process, it can
 *	remain max resolution after execution of this function (if some thread
 *	still waits with the highest timer resolution).
 *
 *----------------------------------------------------------------------
 */

static void
RestoreTimerResolution(
    unsigned long newResolution
) {
    /* if available */
    if (timerResolution.available > 0 && newResolution) {
	EnterCriticalSection(&timerResolution.cs);
	if (timerResolution.count-- <= 1) {
	    timerResolution.resRes = newResolution;
	    /* prolong time to reset resolution */
	    timerResolution.timeToReset = TclpGetMicroseconds() + 1000000;
	}
	LeaveCriticalSection(&timerResolution.cs);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinResetTimerResolution --
 *
 *	This is called to delayed reset (after 1 second) of the timer resolution
 *	to original value, if no more waiting workers using precise resolution.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclWinResetTimerResolution(void)
{
    if ( timerResolution.available > 0
      && timerResolution.count == 0 && timerResolution.resRes > timerResolution.curRes
      && TclpGetMicroseconds() >= timerResolution.timeToReset
    ) {
	EnterCriticalSection(&timerResolution.cs);
	if (timerResolution.count == 0 && timerResolution.resRes > timerResolution.curRes) {
	    ULONG curRes;
	    if (NtSetTimerResolution(timerResolution.resRes, TRUE, &curRes) == 0) {
		timerResolution.curRes = curRes;
	    };
	    timerResolution.timeToReset = 0x7FFFFFFFFFFFFFFFL;
	}
	LeaveCriticalSection(&timerResolution.cs);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitForEvent --
 *
 *	This function is called by Tcl_DoOneEvent to wait for new events on
 *	the message queue. If the block time is 0, then Tcl_WaitForEvent just
 *	polls the event queue without blocking.
 *
 * Results:
 *	Returns -1 if a WM_QUIT message is detected, returns 1 if a message
 *	was dispatched, otherwise returns 0.
 *
 * Side effects:
 *	Dispatches a message to a window procedure, which could do anything.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitForEvent(
    Tcl_Time *timePtr)		/* Maximum block time, or NULL. */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    MSG msg;
    DWORD timeout = INFINITE, result = WAIT_TIMEOUT;
    int status = 0;
    Tcl_WideInt waitTime = 0;
    Tcl_WideInt endTime = 0;
    long tolerance = 0;
    unsigned long actualResolution = 0;

    /*
     * Allow the notifier to be hooked. This may not make sense on windows,
     * but mirrors the UNIX hook.
     */

    if (tclStubs.tcl_WaitForEvent != tclOriginalNotifier.waitForEventProc) {
	return tclStubs.tcl_WaitForEvent(timePtr);
    }

    /*
     * Compute the timeout in milliseconds.
     */

    if (timePtr) {

	waitTime = TCL_TIME_TO_USEC(*timePtr);

	/* if no wait */
	if (waitTime <= 0) {
	    result = 0;
	    goto peek;
	}

	/*
	 * Note the time can be switched (time-jump), so use monotonic time here.
	 */
	endTime = TclpGetUTimeMonotonic() + waitTime;

	if (timerResolution.available == -1) {
	    InitTimerResolution();
	}
	
    #ifdef TMR_RES_TOLERANCE
	/* calculate possible maximal tolerance (in usec) of original wait-time */
	tolerance = ((waitTime <= 1000000) ? waitTime : 1000000) *
			TMR_RES_TOLERANCE / 100;
	if (tolerance > timerResolution.maxDelay) {
	    tolerance = timerResolution.maxDelay;
	}
    #endif

    repeat:
	/*
	* TIP #233 (Virtualized Time). Convert virtual domain delay to
	* real-time.
	*/
	TclpScaleUTime(&waitTime);

	/* No wait if timeout too small (because windows may wait too long) */
	if (waitTime < (long)timerResolution.minDelay) {
	    timeout = 0;
	    /* prevent busy wait */
	    if (waitTime >= 10) {
		goto wait;
	    }
	    Sleep(0);
	    goto peek;
	}
	
	if (timerResolution.available) {
	    long overhead = (tolerance < 100 ? tolerance/2 : 50);
	    Tcl_WideInt waitTimeWithOverhead = waitTime + overhead;
	    if (waitTimeWithOverhead > timerResolution.maxDelay) {
		/* floor (truncate) using max delay as base (follow timeout better) */
		timeout = (waitTimeWithOverhead
				/ timerResolution.maxDelay)
				* timerResolution.maxDelay / 1000;
	    } else {
		/* calculate resolution up to 1000 microseconds
		 * (don't use highest, because of too large CPU load) */
		ULONG res;
		if (waitTimeWithOverhead >= 10000) {
		    res = 10000 * TMR_RES_MICROSEC;
		} else {
		    res = 1000 * TMR_RES_MICROSEC;
		}
		timeout = waitTimeWithOverhead / 1000;
		/* set more precise timer resolution for minimal delay */
		if (!actualResolution || res < timerResolution.curRes) {
		    actualResolution = SetTimerResolution(
			res, actualResolution);
		}
	    }
	} else {
	    timeout = waitTime / 1000;
	}
    }

    /*
     * Check to see if there are any messages in the queue before waiting
     * because MsgWaitForMultipleObjects will not wake up if there are events
     * currently sitting in the queue.
     */
  wait:
    if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
    	goto get;
    }

    /*
     * Wait for something to happen (a signal from another thread, a
     * message, or timeout) or loop servicing asynchronous procedure calls
     * queued to this thread.
     */
    
  again:
    result = MsgWaitForMultipleObjectsEx(1, &tsdPtr->event, timeout,
		QS_ALLINPUT, MWMO_ALERTABLE);
    if (result == WAIT_IO_COMPLETION) {
	goto again;
    }
    if (result == WAIT_FAILED) {
	status = -1;
	goto end;
    }

    /*
     * Check to see if there are any messages to process.
     */
  peek:
    if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
	/*
	 * Retrieve and dispatch the first message.
	 */

  get:
	result = GetMessage(&msg, NULL, 0, 0);
	if (result == 0) {
	    /*
	     * We received a request to exit this thread (WM_QUIT), so
	     * propagate the quit message and start unwinding.
	     */

	    PostQuitMessage((int) msg.wParam);
	    status = -1;
	} else if (result == (DWORD)-1) {
	    /*
	     * We got an error from the system. I have no idea why this would
	     * happen, so we'll just unwind.
	     */

	    status = -1;
	} else {
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	    status = 1;
	}
    }
    else
    if (result == WAIT_TIMEOUT && timeout != INFINITE) {
	/* Check the wait should be repeated, and correct time for wait */

	waitTime = endTime - TclpGetUTimeMonotonic();
	if (waitTime <= tolerance) {
	    goto end;
	}
	/* Repeat wait with more precise timer resolution (or using sleep) */
	goto repeat;
    }

  end:

    /* restore timer resolution */
    if (actualResolution) {
	RestoreTimerResolution(actualResolution);
    }
    /* todo: move it to the service-thread (if available at some point) */
    TclWinResetTimerResolution();
    return status;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpUSleep --
 *
 *	Delay execution for the specified number of microseconds.
 *
 *	TclpUSleep in contrast to Tcl_Sleep is more precise (microseconds).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

void
TclpUSleep(
    Tcl_WideInt usec)		/* Number of microseconds to sleep. */
{
    /*
     * Simply calling 'Sleep' for the requisite number of milliseconds can
     * make the process appear to wake up early because it isn't synchronized
     * with the CPU performance counter that is used in tclWinTime.c. This
     * behavior is probably benign, but messes up some of the corner cases in
     * the test suite. We get around this problem by repeating the 'Sleep'
     * call as many times as necessary to make the clock advance by the
     * requisite amount.
     */

    Tcl_WideInt desired;	/* Desired wakeup time. */
    DWORD sleepTime;		/* Time to sleep, real-time */
    long tolerance = 0;
    unsigned long actualResolution = 0;

    if (usec <= 9) { /* too short to start whole sleep process */
	do {
	    /* causes context switch only (shortest waiting) */
	    Sleep(0);
	} while (--usec > 0);
	return;
    }

    if (timerResolution.available == -1) {
	InitTimerResolution();
    }

    /*
     * Note the time can be switched (time-jump), so use monotonic time here.
     */
    desired = TclpGetUTimeMonotonic() + usec;

  #ifdef TMR_RES_TOLERANCE
    /* calculate possible maximal tolerance (in usec) of original wait-time */
    tolerance = ((usec <= 1000000) ? usec : 1000000) *
			TMR_RES_TOLERANCE / 100;
    if (tolerance > timerResolution.maxDelay) {
	tolerance = timerResolution.maxDelay;
    }
  #endif

    for (;;) {
	
	/*
	 * TIP #233: Scale delay from virtual to real-time.
	 */
	TclpScaleUTime(&usec);

	/* No wait if sleep time too small (because windows may wait too long) */
	if (usec < (long)timerResolution.minDelay) {
	    sleepTime = 0;
	    goto wait;
	}

	if (timerResolution.available) {
	    if (usec > timerResolution.maxDelay) {
		/* floor (truncate) using max delay as base (follow timeout better) */
		sleepTime = (usec
				/ timerResolution.maxDelay)
				* timerResolution.maxDelay / 1000;
	    } else {
	    	/* calculate resolution up to 1000 microseconds
	    	 * (don't use highest, because of too large CPU load) */
		ULONG res;
		if (usec >= 10000) {
		    res = 10000 * TMR_RES_MICROSEC;
		} else {
		    res = 1000 * TMR_RES_MICROSEC;
		}
		sleepTime = usec / 1000;
		/* set more precise timer resolution for minimal delay */
		if (!actualResolution || res < timerResolution.curRes) {
		    actualResolution = SetTimerResolution(
			res, actualResolution);
		}
	    }
	} else {
	    sleepTime = usec / 1000;
	}

    wait:
	Sleep(sleepTime);

	/* new difference to now (monotonic base) */
	usec = desired - TclpGetUTimeMonotonic();
	
	if (usec <= tolerance) {
	    break;
	}
    }

    /* restore timer resolution */
    if (actualResolution) {
	RestoreTimerResolution(actualResolution); 
    }
    /* todo: move it to the service-thread (if available at some point) */
    TclWinResetTimerResolution();
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
