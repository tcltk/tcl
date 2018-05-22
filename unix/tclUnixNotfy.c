/*
 * tclUnixNotfy.c --
 *
 *	This file contains subroutines shared by all notifier backend
 *	implementations on *nix platforms.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 2016 Lucio Andrés Illanes Albornoz <l.illanes@gmx.de>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <poll.h>

/*
 * Static routines defined in this file.
 */

static int	FileHandlerEventProc(Tcl_Event *evPtr, int flags);
#if defined(TCL_THREADS) && !TCL_THREADS
# undef NOTIFIER_EPOLL
# undef NOTIFIER_KQUEUE
# define NOTIFIER_SELECT
#elif !defined(NOTIFIER_EPOLL) && !defined(NOTIFIER_KQUEUE)
# define NOTIFIER_SELECT
static TCL_NORETURN void NotifierThreadProc(ClientData clientData);
# if defined(HAVE_PTHREAD_ATFORK)
static void	AtForkChild(void);
# endif /* HAVE_PTHREAD_ATFORK */

/*
 *----------------------------------------------------------------------
 *
 * StartNotifierThread --
 *
 *	Start a notifier thread and wait for the notifier pipe to be created.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Running Thread.
 *
 *----------------------------------------------------------------------
 */
static void
StartNotifierThread(const char *proc)
{
    if (!notifierThreadRunning) {
	pthread_mutex_lock(&notifierInitMutex);
	if (!notifierThreadRunning) {
	    if (TclpThreadCreate(&notifierThread, NotifierThreadProc, NULL,
		    TCL_THREAD_STACK_DEFAULT, TCL_THREAD_JOINABLE) != TCL_OK) {
		Tcl_Panic("%s: unable to start notifier thread", proc);
	    }

	    pthread_mutex_lock(&notifierMutex);
	    /*
	     * Wait for the notifier pipe to be created.
	     */

	    while (triggerPipe < 0) {
		pthread_cond_wait(&notifierCV, &notifierMutex);
	    }
	    pthread_mutex_unlock(&notifierMutex);

	    notifierThreadRunning = 1;
	}
	pthread_mutex_unlock(&notifierInitMutex);
    }
}
#endif /* NOTIFIER_SELECT */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AlertNotifier --
 *
 *	Wake up the specified notifier from any thread. This routine is called
 *	by the platform independent notifier code whenever the Tcl_ThreadAlert
 *	routine is called. This routine is guaranteed not to be called on a
 *	given notifier after Tcl_FinalizeNotifier is called for that notifier.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	select(2) notifier:
 *		signals the notifier condition variable for the specified
 *		notifier.
 *	epoll(7) notifier:
 *		write(2)s to the eventfd(2) of the specified thread.
 *	kqueue(2) notifier:
 *		write(2)s to the trigger pipe(2) of the specified thread.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AlertNotifier(
    ClientData clientData)
{
    if (tclNotifierHooks.alertNotifierProc) {
	tclNotifierHooks.alertNotifierProc(clientData);
	return;
    } else {
#ifdef NOTIFIER_SELECT
#if !defined(TCL_THREADS) || TCL_THREADS
	ThreadSpecificData *tsdPtr = clientData;

	pthread_mutex_lock(&notifierMutex);
	tsdPtr->eventReady = 1;

#   ifdef __CYGWIN__
	PostMessageW(tsdPtr->hwnd, 1024, 0, 0);
#   else
	pthread_cond_broadcast(&tsdPtr->waitCV);
#   endif /* __CYGWIN__ */
	pthread_mutex_unlock(&notifierMutex);
#endif /* TCL_THREADS */
#else
	ThreadSpecificData *tsdPtr = clientData;
#if defined(NOTIFIER_EPOLL) && defined(HAVE_EVENTFD)
	uint64_t eventFdVal = 1;
	if (write(tsdPtr->triggerEventFd, &eventFdVal,
		sizeof(eventFdVal)) != sizeof(eventFdVal)) {
	    Tcl_Panic("Tcl_AlertNotifier: unable to write to %p->triggerEventFd",
		(void *)tsdPtr);
#else
	if (write(tsdPtr->triggerPipe[1], "", 1) != 1) {
	    Tcl_Panic("Tcl_AlertNotifier: unable to write to %p->triggerPipe",
		(void *)tsdPtr);
#endif /* NOTIFIER_EPOLL && HAVE_EVENTFD */
	}
#endif /* NOTIFIER_SELECT */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetTimer --
 *
 *	This function sets the current notifier timer value. This interface is
 *	not implemented in this notifier because we are always running inside
 *	of Tcl_DoOneEvent.
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
Tcl_SetTimer(
    const Tcl_Time *timePtr)		/* Timeout value, may be NULL. */
{
    if (tclNotifierHooks.setTimerProc) {
	tclNotifierHooks.setTimerProc(timePtr);
	return;
    } else {
	/*
	 * The interval timer doesn't do anything in this implementation,
	 * because the only event loop is via Tcl_DoOneEvent, which passes
	 * timeout values to Tcl_WaitForEvent.
	 */
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
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ServiceModeHook(
    int mode)			/* Either TCL_SERVICE_ALL, or
				 * TCL_SERVICE_NONE. */
{
    if (tclNotifierHooks.serviceModeHookProc) {
	tclNotifierHooks.serviceModeHookProc(mode);
	return;
    } else if (mode == TCL_SERVICE_ALL) {
#ifdef NOTIFIER_SELECT
#if !defined(TCL_THREADS) || TCL_THREADS
	StartNotifierThread("Tcl_ServiceModeHook");
#endif
#endif /* NOTIFIER_SELECT */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FileHandlerEventProc --
 *
 *	This function is called by Tcl_ServiceEvent when a file event reaches
 *	the front of the event queue. This function is responsible for
 *	actually handling the event by invoking the callback for the file
 *	handler.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed from
 *	the queue. Returns 0 if the event was not handled, meaning it should
 *	stay on the queue. The only time the event isn't handled is if the
 *	TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the file handler's callback function does.
 *
 *----------------------------------------------------------------------
 */

static int
FileHandlerEventProc(
    Tcl_Event *evPtr,		/* Event to service. */
    int flags)			/* Flags that indicate what events to handle,
				 * such as TCL_FILE_EVENTS. */
{
    int mask;
    FileHandler *filePtr;
    FileHandlerEvent *fileEvPtr = (FileHandlerEvent *) evPtr;
    ThreadSpecificData *tsdPtr;

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }

    /*
     * Search through the file handlers to find the one whose handle matches
     * the event. We do this rather than keeping a pointer to the file handler
     * directly in the event, so that the handler can be deleted while the
     * event is queued without leaving a dangling pointer.
     */

    tsdPtr = TCL_TSD_INIT(&dataKey);

    for (filePtr = tsdPtr->firstFileHandlerPtr; filePtr != NULL;
	    filePtr = filePtr->nextPtr) {
	if (filePtr->fd != fileEvPtr->fd) {
	    continue;
	}

	/*
	 * The code is tricky for two reasons:
	 * 1. The file handler's desired events could have changed since the
	 *    time when the event was queued, so AND the ready mask with the
	 *    desired mask.
	 * 2. The file could have been closed and re-opened since the time
	 *    when the event was queued. This is why the ready mask is stored
	 *    in the file handler rather than the queued event: it will be
	 *    zeroed when a new file handler is created for the newly opened
	 *    file.
	 */

	mask = filePtr->readyMask & filePtr->mask;
	filePtr->readyMask = 0;
	if (mask != 0) {
	    filePtr->proc(filePtr->clientData, mask);
	}
	break;
    }
    return 1;
}

#ifdef NOTIFIER_SELECT
#if !defined(TCL_THREADS) || TCL_THREADS
/*
 *----------------------------------------------------------------------
 *
 * AlertSingleThread --
 *
 *	Notify a single thread that is waiting on a file descriptor to become
 *	readable or writable or to have an exception condition.
 *	notifierMutex must be held.
 *
 * Result:
 *	None.
 *
 * Side effects:
 *	The condition variable associated with the thread is broadcasted.
 *
 *----------------------------------------------------------------------
 */

static void
AlertSingleThread(
	ThreadSpecificData *tsdPtr)
{
    tsdPtr->eventReady = 1;
    if (tsdPtr->onList) {
        /*
         * Remove the ThreadSpecificData structure of this thread
         * from the waiting list. This prevents us from
         * continuously spinning on epoll_wait until the other
         * threads runs and services the file event.
         */

        if (tsdPtr->prevPtr) {
    	    tsdPtr->prevPtr->nextPtr = tsdPtr->nextPtr;
        } else {
    	    waitingListPtr = tsdPtr->nextPtr;
        }
        if (tsdPtr->nextPtr) {
    	    tsdPtr->nextPtr->prevPtr = tsdPtr->prevPtr;
        }
        tsdPtr->nextPtr = tsdPtr->prevPtr = NULL;
        tsdPtr->onList = 0;
        tsdPtr->pollState = 0;
    }
#ifdef __CYGWIN__
    PostMessageW(tsdPtr->hwnd, 1024, 0, 0);
#else /* __CYGWIN__ */
    pthread_cond_broadcast(&tsdPtr->waitCV);
#endif /* __CYGWIN__ */
}

#if defined(HAVE_PTHREAD_ATFORK)
/*
 *----------------------------------------------------------------------
 *
 * AtForkChild --
 *
 *	Unlock and reinstall the notifier in the child after a fork.
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
AtForkChild(void)
{
    if (notifierThreadRunning == 1) {
	pthread_cond_destroy(&notifierCV);
    }
    pthread_mutex_init(&notifierInitMutex, NULL);
    pthread_mutex_init(&notifierMutex, NULL);
    pthread_cond_init(&notifierCV, NULL);

    /*
     * notifierThreadRunning == 1: thread is running, (there might be data in notifier lists)
     * atForkInit == 0: InitNotifier was never called
     * notifierCount != 0: unbalanced  InitNotifier() / FinalizeNotifier calls
     * waitingListPtr != 0: there are threads currently waiting for events.
     */

    if (atForkInit == 1) {

	notifierCount = 0;
	if (notifierThreadRunning == 1) {
	    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
	    notifierThreadRunning = 0;

	    close(triggerPipe);
	    triggerPipe = -1;
	    /*
	     * The waitingListPtr might contain event info from multiple
	     * threads, which are invalid here, so setting it to NULL is not
	     * unreasonable.
	     */
	    waitingListPtr = NULL;

	    /*
	     * The tsdPtr from before the fork is copied as well.  But since
	     * we are paranoic, we don't trust its condvar and reset it.
	     */
#ifdef __CYGWIN__
	    DestroyWindow(tsdPtr->hwnd);
	    tsdPtr->hwnd = CreateWindowExW(NULL, className,
		    className, 0, 0, 0, 0, 0, NULL, NULL,
		    TclWinGetTclInstance(), NULL);
	    ResetEvent(tsdPtr->event);
#else
	    pthread_cond_destroy(&tsdPtr->waitCV);
	    pthread_cond_init(&tsdPtr->waitCV, NULL);
#endif

	    /*
	     * In case, we had multiple threads running before the fork,
	     * make sure, we don't try to reach out to their thread local data.
	     */
	    tsdPtr->nextPtr = tsdPtr->prevPtr = NULL;

	    /*
	     * The list of registered event handlers at fork time is in
	     * tsdPtr->firstFileHandlerPtr;
	     */
	}
    }

    Tcl_InitNotifier();
}
#endif /* HAVE_PTHREAD_ATFORK */

#endif /* TCL_THREADS */

#endif /* NOTIFIER_SELECT */
#ifndef HAVE_COREFOUNDATION	/* Darwin/Mac OS X CoreFoundation notifier is
				 * in tclMacOSXNotify.c */
/*
 *----------------------------------------------------------------------
 *
 * TclUnixWaitForFile --
 *
 *	This function waits synchronously for a file to become readable or
 *	writable, with an optional timeout.
 *
 * Results:
 *	The return value is an OR'ed combination of TCL_READABLE,
 *	TCL_WRITABLE, and TCL_EXCEPTION, indicating the conditions that are
 *	present on file at the time of the return. This function will not
 *	return until either "timeout" milliseconds have elapsed or at least
 *	one of the conditions given by mask has occurred for file (a return
 *	value of 0 means that a timeout occurred). No normal events will be
 *	serviced during the execution of this function.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

int
TclUnixWaitForFile(
    int fd,			/* Handle for file on which to wait. */
    int mask,			/* What to wait for: OR'ed combination of
				 * TCL_READABLE, TCL_WRITABLE, and
				 * TCL_EXCEPTION. */
    int timeout)		/* Maximum amount of time to wait for one of
				 * the conditions in mask to occur, in
				 * milliseconds. A value of 0 means don't wait
				 * at all, and a value of -1 means wait
				 * forever. */
{
    Tcl_Time abortTime = {0, 0}, now; /* silence gcc 4 warning */
    struct timeval blockTime, *timeoutPtr;
    struct pollfd pollFds[1];
    int numFound, result = 0, pollTimeout;

    /*
     * If there is a non-zero finite timeout, compute the time when we give
     * up.
     */

    if (timeout > 0) {
	Tcl_GetTime(&now);
	abortTime.sec = now.sec + timeout/1000;
	abortTime.usec = now.usec + (timeout%1000)*1000;
	if (abortTime.usec >= 1000000) {
	    abortTime.usec -= 1000000;
	    abortTime.sec += 1;
	}
	timeoutPtr = &blockTime;
    } else if (timeout == 0) {
	timeoutPtr = &blockTime;
	blockTime.tv_sec = 0;
	blockTime.tv_usec = 0;
    } else {
	timeoutPtr = NULL;
    }

    /*
     * Setup the pollfd structure for the fd.
     */

    pollFds[0].fd = fd;
    pollFds[0].events = pollFds[0].revents = 0;
    if (mask & TCL_READABLE) {
	pollFds[0].events |= (POLLIN | POLLHUP);
    }
    if (mask & TCL_WRITABLE) {
	pollFds[0].events |= POLLOUT;
    }
    if (mask & TCL_EXCEPTION) {
	pollFds[0].events |= POLLERR;
    }

    /*
     * Loop in a mini-event loop of our own, waiting for either the file to
     * become ready or a timeout to occur.
     */

    while (1) {
	if (timeout > 0) {
	    blockTime.tv_sec = abortTime.sec - now.sec;
	    blockTime.tv_usec = abortTime.usec - now.usec;
	    if (blockTime.tv_usec < 0) {
		blockTime.tv_sec -= 1;
		blockTime.tv_usec += 1000000;
	    }
	    if (blockTime.tv_sec < 0) {
		blockTime.tv_sec = 0;
		blockTime.tv_usec = 0;
	    }
	}

	/*
	 * Wait for the event or a timeout.
	 */

	if (!timeoutPtr) {
	    pollTimeout = -1;
	} else if (!timeoutPtr->tv_sec && !timeoutPtr->tv_usec) {
	    pollTimeout = 0;
	} else {
	    pollTimeout = (int)timeoutPtr->tv_sec * 1000;
	    if (timeoutPtr->tv_usec) {
		pollTimeout += ((int)timeoutPtr->tv_usec / 1000);
	    }
	}
	numFound = poll(pollFds, 1, pollTimeout);
	if (numFound == 1) {
	    result = 0;
	    if (pollFds[0].revents & (POLLIN | POLLHUP)) {
		result |= TCL_READABLE;
	    }
	    if (pollFds[0].revents & POLLOUT) {
		result |= TCL_WRITABLE;
	    }
	    if (pollFds[0].revents & POLLERR) {
		result |= TCL_EXCEPTION;
	    }
	    if (result) {
		break;
	    }
	}
	if (timeout == 0) {
	    break;
	}
	if (timeout < 0) {
	    continue;
	}

	/*
	 * The select returned early, so we need to recompute the timeout.
	 */

	Tcl_GetTime(&now);
	if ((abortTime.sec < now.sec)
		|| (abortTime.sec==now.sec && abortTime.usec<=now.usec)) {
	    break;
	}
    }
    return result;
}
#endif /* !HAVE_COREFOUNDATION */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
