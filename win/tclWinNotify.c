/* 
 * tclWinNotify.c --
 *
 *	This file contains Windows-specific procedures for the notifier,
 *	which is the lowest-level part of the Tcl event loop.  This file
 *	works together with ../generic/tclNotify.c.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclWinNotify.c,v 1.1.2.3 1998/12/12 01:37:05 lfb Exp $
 */

#include "tclWinInt.h"
#include <winsock.h>

/*
 * The follwing static indicates whether this module has been initialized.
 */

static int initialized = 0;

#define INTERVAL_TIMER 1	/* Handle of interval timer. */

#define WM_WAKEUP WM_USER	/* Message that is send by
				 * TclpAlertNotifier. */
/*
 * The following static structure contains the state information for the
 * Windows implementation of the Tcl notifier.  One of these structures
 * is created for each thread that is using the notifier.  
 */

typedef struct ThreadSpecificData {
    CRITICAL_SECTION crit;	/* Monitor for this notifier. */
    int pending;		/* Alert message pending, this field is
				 * locked by the notifierMutex. */
    HWND hwnd;			/* Messaging window. */
    int timeout;		/* Current timeout value. */
    int timerActive;		/* 1 if interval timer is running. */
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

/*
 * The following static indicates the number of threads that have
 * initialized notifiers.
 *
 * You must hold the notifierMutex lock before accessing this variable.
 */

static int notifierCount = 0;

/*
 * The notifierMutex locks access to all of the global notifier state,
 * as well as the pending flag for any specific notifier.
 */

TCL_DECLARE_MUTEX(notifierMutex)

/*
 * Static routines defined in this file.
 */

static LRESULT CALLBACK	NotifierProc(HWND hwnd, UINT message,
			    WPARAM wParam, LPARAM lParam);
static void		UpdateTimer(ThreadSpecificData *tsdPtr, int timeout);


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
Tcl_InitNotifier()
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    WNDCLASS class;

    /*
     * Register Notifier window class if this is the first thread to
     * use this module.
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
	    panic("Unable to register TclNotifier window class");
	}
    }
    notifierCount++;

    tsdPtr->pending = 0;
    tsdPtr->timerActive = 0;

    /*
     * Create a window for communication with the notifier.
     */

    tsdPtr->hwnd = CreateWindowA("TclNotifier", "TclNotifier", WS_TILED,
	    0, 0, 0, 0, NULL, NULL, TclWinGetTclInstance(), NULL);


    Tcl_MutexUnlock(&notifierMutex);

    return (ClientData) tsdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FinalizeNotifier --
 *
 *	This function is called to cleanup the notifier state before
 *	a thread is terminated.
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
Tcl_FinalizeNotifier(clientData)
    ClientData clientData;	/* Pointer to notifier data. */
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *) clientData;

    Tcl_MutexLock(&notifierMutex);

    /*
     * Clean up the timer and messaging window for this thread.
     */

    KillTimer(tsdPtr->hwnd, INTERVAL_TIMER);
    DestroyWindow(tsdPtr->hwnd);

    /*
     * If this is the last thread to use the notifier, unregister
     * the notifier window class.
     */

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
 *	Wake up the specified notifier from any thread. This routine
 *	is called by the platform independent notifier code whenever
 *	the Tcl_ThreadAlert routine is called.  This routine is
 *	guaranteed not to be called on a given notifier after
 *	Tcl_FinalizeNotifier is called for that notifier.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sends a message to the messaging window for the notifier
 *	if there isn't already one pending.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AlertNotifier(clientData)
    ClientData clientData;	/* Pointer to thread data. */
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *) clientData;

    Tcl_MutexLock(&notifierMutex);
    if (!tsdPtr->pending) {
	PostMessage(tsdPtr->hwnd, WM_WAKEUP, 0, 0);
	tsdPtr->pending = 1;
    }
    Tcl_MutexUnlock(&notifierMutex);
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateTimer --
 *
 *	This function starts or stops the notifier interval timer.
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
UpdateTimer(
    ThreadSpecificData *tsdPtr,	/* Pointer to notifier state. */
    int timeout)		/* ms timeout, 0 means cancel timer */
{
    tsdPtr->timeout = timeout;
    if (timeout != 0) {
	tsdPtr->timerActive = 1;
	SetTimer(tsdPtr->hwnd, INTERVAL_TIMER,
		    (unsigned long) tsdPtr->timeout, NULL);
    } else {
	tsdPtr->timerActive = 0;
	KillTimer(tsdPtr->hwnd, INTERVAL_TIMER);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetTimer --
 *
 *	This procedure sets the current notifier timer value.  The
 *	notifier will ensure that Tcl_ServiceAll() is called after
 *	the specified interval, even if no events have occurred.
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
    UpdateTimer(tsdPtr, timeout);
}

/*
 *----------------------------------------------------------------------
 *
 * NotifierProc --
 *
 *	This procedure is invoked by Windows to process events on
 *	the notifier window.  Messages will be sent to this window
 *	in response to external timer events or calls to
 *	TclpAlertTsdPtr->
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
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (message == WM_USER) {
	Tcl_MutexLock(&notifierMutex);
	tsdPtr->pending = 0;
	Tcl_MutexUnlock(&notifierMutex);
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
 *----------------------------------------------------------------------
 *
 * Tcl_WaitForEvent --
 *
 *	This function is called by Tcl_DoOneEvent to wait for new
 *	events on the message queue.  If the block time is 0, then
 *	Tcl_WaitForEvent just polls the event queue without blocking.
 *
 * Results:
 *	Returns -1 if a WM_QUIT message is detected, returns 1 if
 *	a message was dispatched, otherwise returns 0.
 *
 * Side effects:
 *	Dispatches a message to a window procedure, which could do
 *	anything.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitForEvent(
    Tcl_Time *timePtr)		/* Maximum block time, or NULL. */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    MSG msg;
    int timeout;

    /*
     * Only use the interval timer for non-zero timeouts.  This avoids
     * generating useless messages when we really just want to poll.
     */

    if (timePtr) {
	timeout = timePtr->sec * 1000 + timePtr->usec / 1000;
    } else {
	timeout = 0;
    }
    UpdateTimer(tsdPtr, timeout);
	
    if (!timePtr || (timeout != 0)
	    || PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
	if (!GetMessage(&msg, NULL, 0, 0)) {

	    /*
	     * The application is exiting, so repost the quit message
	     * and start unwinding.
	     */

	    PostQuitMessage(msg.wParam);
	    return -1;
	}

	/*
	 * Handle timer expiration as a special case so we don't
	 * claim to be doing work when we aren't.
	 */

	if (msg.message == WM_TIMER && msg.hwnd == tsdPtr->hwnd) {
	    return 0;
	}

	TranslateMessage(&msg);
	DispatchMessage(&msg);
	return 1;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Sleep --
 *
 *	Delay execution for the specified number of milliseconds.
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
Tcl_Sleep(ms)
    int ms;			/* Number of milliseconds to sleep. */
{
    Sleep(ms);
}
