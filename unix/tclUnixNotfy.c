/*
 * tclUnixNotfy.c --
 *
 *	This file contains subroutines shared by all notifier backend
 *	implementations on *nix platforms. It is *included* by the epoll,
 *	kqueue and select notifier implementation files.
 *
 * Copyright © 1995-1997 Sun Microsystems, Inc.
 * Copyright © 2016 Lucio Andrés Illanes Albornoz <l.illanes@gmx.de>
 * Copyright © 2021 Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <poll.h>
#include "tclInt.h"
#ifndef HAVE_COREFOUNDATION	/* Darwin/Mac OS X CoreFoundation notifier is
				 * in tclMacOSXNotify.c */
#include <signal.h>

/*
 * This structure is used to keep track of the notifier info for a registered
 * file.
 */

typedef struct FileHandler {
    int fd;
    int mask;			/* Mask of desired events: TCL_READABLE,
				 * etc. */
    int readyMask;		/* Mask of events that have been seen since
				 * the last time file handlers were invoked
				 * for this file. */
    Tcl_FileProc *proc;		/* Function to call, in the style of
				 * Tcl_CreateFileHandler. */
    ClientData clientData;	/* Argument to pass to proc. */
    struct FileHandler *nextPtr;/* Next in list of all files we care about. */
} FileHandler;

/*
 * The following structure is what is added to the Tcl event queue when file
 * handlers are ready to fire.
 */

typedef struct {
    Tcl_Event header;		/* Information that is standard for all
				 * events. */
    int fd;			/* File descriptor that is ready. Used to find
				 * the FileHandler structure for the file
				 * (can't point directly to the FileHandler
				 * structure because it could go away while
				 * the event is queued). */
} FileHandlerEvent;

/*
 * The following structure contains a set of select() masks to track readable,
 * writable, and exception conditions.
 */

typedef struct {
    fd_set readable;
    fd_set writable;
    fd_set exception;
} SelectMasks;

/*
 * The following static structure contains the state information for the
 * select based implementation of the Tcl notifier. One of these structures is
 * created for each thread that is using the notifier.
 */

typedef struct ThreadSpecificData {
    FileHandler *firstFileHandlerPtr;
				/* Pointer to head of file handler list. */
    SelectMasks checkMasks;	/* This structure is used to build up the
				 * masks to be used in the next call to
				 * select. Bits are set in response to calls
				 * to Tcl_CreateFileHandler. */
    SelectMasks readyMasks;	/* This array reflects the readable/writable
				 * conditions that were found to exist by the
				 * last call to select. */
    int numFdBits;		/* Number of valid bits in checkMasks (one
				 * more than highest fd for which
				 * Tcl_WatchFile has been called). */
    int onList;			/* True if it is in this list */
    unsigned int pollState;	/* pollState is used to implement a polling
				 * handshake between each thread and the
				 * notifier thread. Bits defined below. */
    struct ThreadSpecificData *nextPtr, *prevPtr;
				/* All threads that are currently waiting on
				 * an event have their ThreadSpecificData
				 * structure on a doubly-linked listed formed
				 * from these pointers. You must hold the
				 * notifierMutex lock before accessing these
				 * fields. */
#ifdef __CYGWIN__
    void *event;     /* Any other thread alerts a notifier
	 * that an event is ready to be processed
	 * by sending this event. */
    void *hwnd;			/* Messaging window. */
#else /* !__CYGWIN__ */
    pthread_cond_t waitCV;	/* Any other thread alerts a notifier that an
				 * event is ready to be processed by signaling
				 * this condition variable. */
#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK)
    int useMonoTime;		/* When true use CLOCK_MONOTONIC */
#endif
#endif /* __CYGWIN__ */
    int waitCVinitialized;	/* Variable to flag initialization of the structure */
    int eventReady;		/* True if an event is ready to be processed.
				 * Used as condition flag together with waitCV
				 * above. */
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

/*
 * The following static indicates the number of threads that have initialized
 * notifiers.
 *
 * You must hold the notifierMutex lock before accessing this variable.
 */

static int notifierCount = 0;

/*
 * The following variable points to the head of a doubly-linked list of
 * ThreadSpecificData structures for all threads that are currently waiting on
 * an event.
 *
 * You must hold the notifierMutex lock before accessing this list.
 */

static ThreadSpecificData *waitingListPtr = NULL;

/*
 * The notifier thread spends all its time in select() waiting for a file
 * descriptor associated with one of the threads on the waitingListPtr list to
 * do something interesting. But if the contents of the waitingListPtr list
 * ever changes, we need to wake up and restart the select() system call. You
 * can wake up the notifier thread by writing a single byte to the file
 * descriptor defined below. This file descriptor is the input-end of a pipe
 * and the notifier thread is listening for data on the output-end of the same
 * pipe. Hence writing to this file descriptor will cause the select() system
 * call to return and wake up the notifier thread.
 *
 * You must hold the notifierMutex lock before writing to the pipe.
 */

static int triggerPipe = -1;

/*
 * The notifierMutex locks access to all of the global notifier state.
 */

static pthread_mutex_t notifierInitMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t notifierMutex     = PTHREAD_MUTEX_INITIALIZER;
/*
 * The following static indicates if the notifier thread is running.
 *
 * You must hold the notifierInitMutex before accessing this variable.
 */

static int notifierThreadRunning = 0;

/*
 * The notifier thread signals the notifierCV when it has finished
 * initializing the triggerPipe and right before the notifier thread
 * terminates.
 */

static pthread_cond_t notifierCV = PTHREAD_COND_INITIALIZER;

/*
 * The pollState bits
 *	POLL_WANT is set by each thread before it waits on its condition
 *		variable. It is checked by the notifier before it does select.
 *	POLL_DONE is set by the notifier if it goes into select after seeing
 *		POLL_WANT. The idea is to ensure it tries a select with the
 *		same bits the initial thread had set.
 */

#define POLL_WANT	0x1
#define POLL_DONE	0x2

/*
 * This is the thread ID of the notifier thread that does select.
 */

static Tcl_ThreadId notifierThread;


/*
 * Static routines defined in this file.
 */

static int		FileHandlerEventProc(Tcl_Event *evPtr, int flags);
#if !TCL_THREADS
# undef NOTIFIER_EPOLL
# undef NOTIFIER_KQUEUE
# define NOTIFIER_SELECT
#elif !defined(NOTIFIER_EPOLL) && !defined(NOTIFIER_KQUEUE)
# define NOTIFIER_SELECT
static TCL_NORETURN void NotifierThreadProc(void *clientData);
# if defined(HAVE_PTHREAD_ATFORK)
static void		AtForkChild(void);
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
StartNotifierThread(
    const char *proc)
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

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InitNotifier --
 *
 *	Initializes the platform specific notifier state.
 *
 * Results:
 *	Returns a handle to the notifier state for this thread.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_InitNotifier(void)
{
    if (tclNotifierHooks.initNotifierProc) {
	return tclNotifierHooks.initNotifierProc();
    } else {
	ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

	tsdPtr->eventReady = 0;

	/*
	 * Initialize thread specific condition variable for this thread.
	 */
	if (tsdPtr->waitCVinitialized == 0) {
#ifdef __CYGWIN__
	    WNDCLASS class;

	    class.style = 0;
	    class.cbClsExtra = 0;
	    class.cbWndExtra = 0;
	    class.hInstance = TclWinGetTclInstance();
	    class.hbrBackground = NULL;
	    class.lpszMenuName = NULL;
	    class.lpszClassName = className;
	    class.lpfnWndProc = NotifierProc;
	    class.hIcon = NULL;
	    class.hCursor = NULL;

	    RegisterClassW(&class);
	    tsdPtr->hwnd = CreateWindowExW(NULL, class.lpszClassName,
		    class.lpszClassName, 0, 0, 0, 0, 0, NULL, NULL,
		    TclWinGetTclInstance(), NULL);
	    tsdPtr->event = CreateEventW(NULL, 1 /* manual */,
		    0 /* !signaled */, NULL);
#else
#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK)
	    pthread_condattr_t attr;

	    pthread_condattr_init(&attr);
	    tsdPtr->useMonoTime =
		    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) == 0;
	    if (tsdPtr->useMonoTime) {
		if (pthread_cond_init(&tsdPtr->waitCV, &attr)) {
		    tsdPtr->useMonoTime = 0;
		    pthread_cond_init(&tsdPtr->waitCV, NULL);
		}
	    } else {
		pthread_cond_init(&tsdPtr->waitCV, NULL);
	    }
	    pthread_condattr_destroy(&attr);
#else
	    pthread_cond_init(&tsdPtr->waitCV, NULL);
#endif
#endif /* __CYGWIN__ */
	    tsdPtr->waitCVinitialized = 1;
	}

	pthread_mutex_lock(&notifierInitMutex);
#if defined(HAVE_PTHREAD_ATFORK)
	/*
	 * Install pthread_atfork handlers to clean up the notifier in the
	 * child of a fork.
	 */

	if (!atForkInit) {
	    int result = pthread_atfork(NULL, NULL, AtForkChild);

	    if (result) {
		Tcl_Panic("Tcl_InitNotifier: pthread_atfork failed");
	    }
	    atForkInit = 1;
	}
#endif /* HAVE_PTHREAD_ATFORK */

	notifierCount++;

	pthread_mutex_unlock(&notifierInitMutex);

	return tsdPtr;
    }
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
 *	May terminate the background notifier thread if this is the last
 *	notifier instance.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FinalizeNotifier(
    ClientData clientData)		/* Not used. */
{
    if (tclNotifierHooks.finalizeNotifierProc) {
	tclNotifierHooks.finalizeNotifierProc(clientData);
	return;
    } else {
	ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

	pthread_mutex_lock(&notifierInitMutex);
	notifierCount--;

	/*
	 * If this is the last thread to use the notifier, close the notifier
	 * pipe and wait for the background thread to terminate.
	 */

	if (notifierCount == 0) {

	    if (triggerPipe != -1) {
		if (write(triggerPipe, "q", 1) != 1) {
		    Tcl_Panic("Tcl_FinalizeNotifier: %s",
			    "unable to write q to triggerPipe");
		}
		close(triggerPipe);
		pthread_mutex_lock(&notifierMutex);
		while(triggerPipe != -1) {
		    pthread_cond_wait(&notifierCV, &notifierMutex);
		}
		pthread_mutex_unlock(&notifierMutex);
		if (notifierThreadRunning) {
		    int result = pthread_join((pthread_t) notifierThread, NULL);

		    if (result) {
			Tcl_Panic("Tcl_FinalizeNotifier: unable to join notifier "
				"thread");
		    }
		    notifierThreadRunning = 0;
		}
	    }
	}

	/*
	 * Clean up any synchronization objects in the thread local storage.
	 */

#ifdef __CYGWIN__
	DestroyWindow(tsdPtr->hwnd);
	CloseHandle(tsdPtr->event);
#else /* __CYGWIN__ */
	pthread_cond_destroy(&tsdPtr->waitCV);
#endif /* __CYGWIN__ */
	tsdPtr->waitCVinitialized = 0;

	pthread_mutex_unlock(&notifierInitMutex);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AlertNotifier --
#endif /* NOTIFIER_SELECT */

/*
 *----------------------------------------------------------------------
 *
 * TclpAlertNotifier --
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
TclpAlertNotifier(
    void *clientData)
{
#ifdef NOTIFIER_SELECT
#if TCL_THREADS
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *) clientData;

    pthread_mutex_lock(&notifierMutex);
    tsdPtr->eventReady = 1;

#   ifdef __CYGWIN__
    PostMessageW(tsdPtr->hwnd, 1024, 0, 0);
#   else
    pthread_cond_broadcast(&tsdPtr->waitCV);
#   endif /* __CYGWIN__ */
    pthread_mutex_unlock(&notifierMutex);
#else
    (void)clientData;
#endif /* TCL_THREADS */
#else /* !NOTIFIER_SELECT */
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *) clientData;
#if defined(NOTIFIER_EPOLL) && defined(HAVE_EVENTFD)
    uint64_t eventFdVal = 1;

    if (write(tsdPtr->triggerEventFd, &eventFdVal,
	    sizeof(eventFdVal)) != sizeof(eventFdVal)) {
	Tcl_Panic("Tcl_AlertNotifier: unable to write to %p->triggerEventFd",
		tsdPtr);
    }
#else
    if (write(tsdPtr->triggerPipe[1], "", 1) != 1) {
	Tcl_Panic("Tcl_AlertNotifier: unable to write to %p->triggerPipe",
		tsdPtr);
    }
#endif /* NOTIFIER_EPOLL && HAVE_EVENTFD */
#endif /* NOTIFIER_SELECT */
}

/*
 *----------------------------------------------------------------------
 *
 * LookUpFileHandler --
 *
 *	Look up the file handler structure (and optionally the previous one in
 *	the chain) associated with a file descriptor.
 *
 * Returns:
 *	A pointer to the file handler, or NULL if it can't be found.
 *
 * Side effects:
 *	If prevPtrPtr is non-NULL, it will be written to if the file handler
 *	is found.
 *
 *----------------------------------------------------------------------
 */

static inline FileHandler *
LookUpFileHandler(
    ThreadSpecificData *tsdPtr,	/* Where to look things up. */
    int fd,			/* What we are looking for. */
    FileHandler **prevPtrPtr)	/* If non-NULL, where to report the previous
				 * pointer. */
{
    FileHandler *filePtr, *prevPtr;

    /*
     * Find the entry for the given file (and return if there isn't one).
     */

    for (prevPtr = NULL, filePtr = tsdPtr->firstFileHandlerPtr; ;
	    prevPtr = filePtr, filePtr = filePtr->nextPtr) {
	if (filePtr == NULL) {
	    return NULL;
	}
	if (filePtr->fd == fd) {
	    break;
	}
    }

    /*
     * Report what we've found to our caller.
     */

    if (prevPtrPtr) {
	*prevPtrPtr = prevPtr;
    }
    return filePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpSetTimer --
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
TclpSetTimer(
    TCL_UNUSED(const Tcl_Time *))		/* Timeout value, may be NULL. */
{
    /*
     * The interval timer doesn't do anything in this implementation, because
     * the only event loop is via Tcl_DoOneEvent, which passes timeout values
     * to Tcl_WaitForEvent.
     */
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
TclpServiceModeHook(
    int mode)			/* Either TCL_SERVICE_ALL, or
				 * TCL_SERVICE_NONE. */
{
    if (mode == TCL_SERVICE_ALL) {
#ifdef NOTIFIER_SELECT
#if TCL_THREADS
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

#if defined(__CYGWIN__)

static DWORD __stdcall
NotifierProc(
    void *hwnd,
    unsigned int message,
    void *wParam,
    void *lParam)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (message != 1024) {
	return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    /*
     * Process all of the runnable events.
     */

    tsdPtr->eventReady = 1;
    Tcl_ServiceAll();
    return 0;
}
#endif /* __CYGWIN__ */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitForEvent --
 *
 *	This function is called by Tcl_DoOneEvent to wait for new events on
 *	the message queue. If the block time is 0, then Tcl_WaitForEvent just
 *	polls without blocking.
 *
 * Results:
 *	Returns -1 if the select would block forever, otherwise returns 0.
 *
 * Side effects:
 *	Queues file events that are detected by the select.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitForEvent(
    const Tcl_Time *timePtr)		/* Maximum block time, or NULL. */
{
    if (tclNotifierHooks.waitForEventProc) {
	return tclNotifierHooks.waitForEventProc(timePtr);
    } else {
	FileHandler *filePtr;
	int mask;
	Tcl_Time vTime;
	int waitForFiles;
#   ifdef __CYGWIN__
	MSG msg;
#   endif /* __CYGWIN__ */
	ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

	/*
	 * Set up the timeout structure. Note that if there are no events to
	 * check for, we return with a negative result rather than blocking
	 * forever.
	 */

	if (timePtr != NULL) {
	    /*
	     * TIP #233 (Virtualized Time). Is virtual time in effect? And do
	     * we actually have something to scale? If yes to both then we
	     * call the handler to do this scaling.
	     */

	    if (timePtr->sec != 0 || timePtr->usec != 0) {
		vTime = *timePtr;
		tclScaleTimeProcPtr(&vTime, tclTimeClientData);
		timePtr = &vTime;
	    }
	}

	/*
	 * Start notifier thread and place this thread on the list of
	 * interested threads, signal the notifier thread, and wait for a
	 * response or a timeout.
	 */
	StartNotifierThread("Tcl_WaitForEvent");

	pthread_mutex_lock(&notifierMutex);

	if (timePtr != NULL && timePtr->sec == 0 && (timePtr->usec == 0
#if defined(__APPLE__) && defined(__LP64__)
		/*
		 * On 64-bit Darwin, pthread_cond_timedwait() appears to have
		 * a bug that causes it to wait forever when passed an
		 * absolute time which has already been exceeded by the system
		 * time; as a workaround, when given a very brief timeout,
		 * just do a poll. [Bug 1457797]
		 */
		|| timePtr->usec < 10
#endif /* __APPLE__ && __LP64__ */
		)) {
	    /*
	     * Cannot emulate a polling select with a polling condition
	     * variable. Instead, pretend to wait for files and tell the
	     * notifier thread what we are doing. The notifier thread makes
	     * sure it goes through select with its select mask in the same
	     * state as ours currently is. We block until that happens.
	     */

	    waitForFiles = 1;
	    tsdPtr->pollState = POLL_WANT;
	    timePtr = NULL;
	} else {
	    waitForFiles = (tsdPtr->numFdBits > 0);
	    tsdPtr->pollState = 0;
	}

	if (waitForFiles) {
	    /*
	     * Add the ThreadSpecificData structure of this thread to the list
	     * of ThreadSpecificData structures of all threads that are
	     * waiting on file events.
	     */

	    tsdPtr->nextPtr = waitingListPtr;
	    if (waitingListPtr) {
		waitingListPtr->prevPtr = tsdPtr;
	    }
	    tsdPtr->prevPtr = 0;
	    waitingListPtr = tsdPtr;
	    tsdPtr->onList = 1;

	    if ((write(triggerPipe, "", 1) == -1) && (errno != EAGAIN)) {
		Tcl_Panic("Tcl_WaitForEvent: %s",
			"unable to write to triggerPipe");
	    }
	}

	FD_ZERO(&tsdPtr->readyMasks.readable);
	FD_ZERO(&tsdPtr->readyMasks.writable);
	FD_ZERO(&tsdPtr->readyMasks.exception);

	if (!tsdPtr->eventReady) {
#ifdef __CYGWIN__
	    if (!PeekMessageW(&msg, NULL, 0, 0, 0)) {
		DWORD timeout;

		if (timePtr) {
		    timeout = timePtr->sec * 1000 + timePtr->usec / 1000;
		} else {
		    timeout = 0xFFFFFFFF;
		}
		pthread_mutex_unlock(&notifierMutex);
		MsgWaitForMultipleObjects(1, &tsdPtr->event, 0, timeout, 1279);
		pthread_mutex_lock(&notifierMutex);
	    }
#else
	    if (timePtr != NULL) {
		struct timespec ptime;

#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_PTHREAD_CONDATTR_SETCLOCK)
		if (tsdPtr->useMonoTime) {
		    clock_gettime(CLOCK_MONOTONIC, &ptime);
		} else {
		    clock_gettime(CLOCK_REALTIME, &ptime);
		}
		ptime.tv_sec += timePtr->sec +
		    (timePtr->usec * 1000 + ptime.tv_nsec) / 1000000000;
		ptime.tv_nsec = (timePtr->usec * 1000 + ptime.tv_nsec) %
		    1000000000;
#else
		Tcl_Time now;

		Tcl_GetTime(&now);
		ptime.tv_sec = timePtr->sec + now.sec +
		    (timePtr->usec + now.usec) / 1000000;
		ptime.tv_nsec = 1000 * ((timePtr->usec + now.usec) % 1000000);
#endif
		pthread_cond_timedwait(&tsdPtr->waitCV, &notifierMutex, &ptime);
	    } else {
		pthread_cond_wait(&tsdPtr->waitCV, &notifierMutex);
	    }
#endif /* __CYGWIN__ */
	}
	tsdPtr->eventReady = 0;

#ifdef __CYGWIN__
	while (PeekMessageW(&msg, NULL, 0, 0, 0)) {
	    /*
	     * Retrieve and dispatch the message.
	     */

	    DWORD result = GetMessageW(&msg, NULL, 0, 0);

	    if (result == 0) {
		PostQuitMessage(msg.wParam);
		/* What to do here? */
	    } else if (result != (DWORD) -1) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	    }
	}
	ResetEvent(tsdPtr->event);
#endif /* __CYGWIN__ */

	if (waitForFiles && tsdPtr->onList) {
	    /*
	     * Remove the ThreadSpecificData structure of this thread from the
	     * waiting list. Alert the notifier thread to recompute its select
	     * masks - skipping this caused a hang when trying to close a pipe
	     * which the notifier thread was still doing a select on.
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
	    if ((write(triggerPipe, "", 1) == -1) && (errno != EAGAIN)) {
		Tcl_Panic("Tcl_WaitForEvent: %s",
			"unable to write to triggerPipe");
	    }
	}

	/*
	 * Queue all detected file events before returning.
	 */

	for (filePtr = tsdPtr->firstFileHandlerPtr; (filePtr != NULL);
		filePtr = filePtr->nextPtr) {
	    mask = 0;
	    if (FD_ISSET(filePtr->fd, &tsdPtr->readyMasks.readable)) {
		mask |= TCL_READABLE;
	    }
	    if (FD_ISSET(filePtr->fd, &tsdPtr->readyMasks.writable)) {
		mask |= TCL_WRITABLE;
	    }
	    if (FD_ISSET(filePtr->fd, &tsdPtr->readyMasks.exception)) {
		mask |= TCL_EXCEPTION;
	    }

	    if (!mask) {
		continue;
	    }

	    /*
	     * Don't bother to queue an event if the mask was previously
	     * non-zero since an event must still be on the queue.
	     */

	    if (filePtr->readyMask == 0) {
		FileHandlerEvent *fileEvPtr =
			ckalloc(sizeof(FileHandlerEvent));

		fileEvPtr->header.proc = FileHandlerEventProc;
		fileEvPtr->fd = filePtr->fd;
		Tcl_QueueEvent((Tcl_Event *) fileEvPtr, TCL_QUEUE_TAIL);
	    }
	    filePtr->readyMask = mask;
	}
	pthread_mutex_unlock(&notifierMutex);
	return 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * NotifierThreadProc --
 *
 *	This routine is the initial (and only) function executed by the
 *	special notifier thread. Its job is to wait for file descriptors to
 *	become readable or writable or to have an exception condition and then
 *	to notify other threads who are interested in this information by
 *	signalling a condition variable. Other threads can signal this
 *	notifier thread of a change in their interests by writing a single
 *	byte to a special pipe that the notifier thread is monitoring.
 *
 * Result:
 *	None. Once started, this routine never exits. It dies with the overall
 *	process.
 *
 * Side effects:
 *	The trigger pipe used to signal the notifier thread is created when
 *	the notifier thread first starts.
 *
 *----------------------------------------------------------------------
 */

static TCL_NORETURN void
NotifierThreadProc(
    ClientData clientData)	/* Not used. */
{
    ThreadSpecificData *tsdPtr;
    fd_set readableMask;
    fd_set writableMask;
    fd_set exceptionMask;
    int fds[2];
    int i, numFdBits = 0, receivePipe;
    long found;
    struct timeval poll = {0., 0.}, *timePtr;
    char buf[2];

    if (pipe(fds) != 0) {
	Tcl_Panic("NotifierThreadProc: %s", "could not create trigger pipe");
    }

    receivePipe = fds[0];

    if (TclUnixSetBlockingMode(receivePipe, TCL_MODE_NONBLOCKING) < 0) {
	Tcl_Panic("NotifierThreadProc: %s",
		"could not make receive pipe non blocking");
    }
    if (TclUnixSetBlockingMode(fds[1], TCL_MODE_NONBLOCKING) < 0) {
	Tcl_Panic("NotifierThreadProc: %s",
		"could not make trigger pipe non blocking");
    }
    if (fcntl(receivePipe, F_SETFD, FD_CLOEXEC) < 0) {
	Tcl_Panic("NotifierThreadProc: %s",
		"could not make receive pipe close-on-exec");
    }
    if (fcntl(fds[1], F_SETFD, FD_CLOEXEC) < 0) {
	Tcl_Panic("NotifierThreadProc: %s",
		"could not make trigger pipe close-on-exec");
    }

    /*
     * Install the write end of the pipe into the global variable.
     */

    pthread_mutex_lock(&notifierMutex);
    triggerPipe = fds[1];

    /*
     * Signal any threads that are waiting.
     */

    pthread_cond_broadcast(&notifierCV);
    pthread_mutex_unlock(&notifierMutex);

    /*
     * Look for file events and report them to interested threads.
     */

    while (1) {
	FD_ZERO(&readableMask);
	FD_ZERO(&writableMask);
	FD_ZERO(&exceptionMask);

	/*
	 * Compute the logical OR of the select masks from all the waiting
	 * notifiers.
	 */

	pthread_mutex_lock(&notifierMutex);
	timePtr = NULL;
	for (tsdPtr = waitingListPtr; tsdPtr; tsdPtr = tsdPtr->nextPtr) {
	    for (i = tsdPtr->numFdBits-1; i >= 0; --i) {
		if (FD_ISSET(i, &tsdPtr->checkMasks.readable)) {
		    FD_SET(i, &readableMask);
		}
		if (FD_ISSET(i, &tsdPtr->checkMasks.writable)) {
		    FD_SET(i, &writableMask);
		}
		if (FD_ISSET(i, &tsdPtr->checkMasks.exception)) {
		    FD_SET(i, &exceptionMask);
		}
	    }
	    if (tsdPtr->numFdBits > numFdBits) {
		numFdBits = tsdPtr->numFdBits;
	    }
	    if (tsdPtr->pollState & POLL_WANT) {
		/*
		 * Here we make sure we go through select() with the same mask
		 * bits that were present when the thread tried to poll.
		 */

		tsdPtr->pollState |= POLL_DONE;
		timePtr = &poll;
	    }
	}
	pthread_mutex_unlock(&notifierMutex);

	/*
	 * Set up the select mask to include the receive pipe.
	 */

	if (receivePipe >= numFdBits) {
	    numFdBits = receivePipe + 1;
	}
	FD_SET(receivePipe, &readableMask);

	if (select(numFdBits, &readableMask, &writableMask, &exceptionMask,
		timePtr) == -1) {
	    /*
	     * Try again immediately on an error.
	     */

	    continue;
	}

	/*
	 * Alert any threads that are waiting on a ready file descriptor.
	 */

	pthread_mutex_lock(&notifierMutex);
	for (tsdPtr = waitingListPtr; tsdPtr; tsdPtr = tsdPtr->nextPtr) {
	    found = 0;

	    for (i = tsdPtr->numFdBits-1; i >= 0; --i) {
		if (FD_ISSET(i, &tsdPtr->checkMasks.readable)
			&& FD_ISSET(i, &readableMask)) {
		    FD_SET(i, &tsdPtr->readyMasks.readable);
		    found = 1;
		}
		if (FD_ISSET(i, &tsdPtr->checkMasks.writable)
			&& FD_ISSET(i, &writableMask)) {
		    FD_SET(i, &tsdPtr->readyMasks.writable);
		    found = 1;
		}
		if (FD_ISSET(i, &tsdPtr->checkMasks.exception)
			&& FD_ISSET(i, &exceptionMask)) {
		    FD_SET(i, &tsdPtr->readyMasks.exception);
		    found = 1;
		}
	    }

	    if (found || (tsdPtr->pollState & POLL_DONE)) {
		tsdPtr->eventReady = 1;
		if (tsdPtr->onList) {
		    /*
		     * Remove the ThreadSpecificData structure of this thread
		     * from the waiting list. This prevents us from
		     * continuously spining on select until the other threads
		     * runs and services the file event.
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
	}
	pthread_mutex_unlock(&notifierMutex);

	/*
	 * Consume the next byte from the notifier pipe if the pipe was
	 * readable. Note that there may be multiple bytes pending, but to
	 * avoid a race condition we only read one at a time.
	 */

	if (FD_ISSET(receivePipe, &readableMask)) {
	    i = read(receivePipe, buf, 1);

	    if ((i == 0) || ((i == 1) && (buf[0] == 'q'))) {
		/*
		 * Someone closed the write end of the pipe or sent us a Quit
		 * message [Bug: 4139] and then closed the write end of the
		 * pipe so we need to shut down the notifier thread.
		 */

		break;
	    }
	}
    }

    /*
     * Clean up the read end of the pipe and signal any threads waiting on
     * termination of the notifier thread.
     */

    close(receivePipe);
    pthread_mutex_lock(&notifierMutex);
    triggerPipe = -1;
    pthread_cond_broadcast(&notifierCV);
    pthread_mutex_unlock(&notifierMutex);

    TclpThreadExit(0);
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

#ifdef NOTIFIER_SELECT
    asyncPending = 0;
#endif

    /*
     * notifierThreadRunning == 1: thread is running, (there might be data in
     *		notifier lists)
     * atForkInit == 0: InitNotifier was never called
     * notifierCount != 0: unbalanced InitNotifier() / FinalizeNotifier calls
     * waitingListPtr != 0: there are threads currently waiting for events.
     */

    if (atForkInit == 1) {

	notifierCount = 0;
	if (notifierThreadRunning == 1) {
	    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
	    notifierThreadRunning = 0;

	    close(triggerPipe);
	    triggerPipe = -1;
#ifdef NOTIFIER_SELECT
	    close(otherPipe);
	    otherPipe = -1;
#endif
	    /*
	     * The waitingListPtr might contain event info from multiple
	     * threads, which are invalid here, so setting it to NULL is not
	     * unreasonable.
	     */
	    waitingListPtr = NULL;

	    /*
	     * The tsdPtr from before the fork is copied as well. But since we
	     * are paranoiac, we don't trust its condvar and reset it.
	     */
#ifdef __CYGWIN__
	    DestroyWindow(tsdPtr->hwnd);
	    tsdPtr->hwnd = CreateWindowExW(NULL, className,
		    className, 0, 0, 0, 0, 0, NULL, NULL,
		    TclWinGetTclInstance(), NULL);
	    ResetEvent(tsdPtr->event);
#else /* !__CYGWIN__ */
	    pthread_cond_destroy(&tsdPtr->waitCV);
	    pthread_cond_init(&tsdPtr->waitCV, NULL);
#endif /* __CYGWIN__ */

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

#ifdef NOTIFIER_SELECT
    /*
     * Restart the notifier thread for signal handling.
     */

    StartNotifierThread("AtForkChild");
#endif
}
#endif /* HAVE_PTHREAD_ATFORK */
#endif /* TCL_THREADS */
#endif /* NOTIFIER_SELECT */

/*
 *----------------------------------------------------------------------
 *
 * TclpNotifierData --
 *
 *	This function returns a void pointer to be associated
 *	with a Tcl_AsyncHandler.
 *
 * Results:
 *	For the epoll and kqueue notifiers, this function returns the
 *	thread specific data. Otherwise NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void *
TclpNotifierData(void)
{
#if defined(NOTIFIER_EPOLL) || defined(NOTIFIER_KQUEUE)
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    return tsdPtr;
#else
    return NULL;
#endif
}

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

#ifndef HAVE_COREFOUNDATION	/* Darwin/Mac OS X CoreFoundation notifier is
				 * in tclMacOSXNotify.c */

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
	abortTime.sec = now.sec + timeout / 1000;
	abortTime.usec = now.usec + (timeout % 1000) * 1000;
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

    do {
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
	    pollTimeout = (int) timeoutPtr->tv_sec * 1000;
	    if (timeoutPtr->tv_usec) {
		pollTimeout += (int) timeoutPtr->tv_usec / 1000;
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
    } while ((abortTime.sec > now.sec)
	    || (abortTime.sec == now.sec && abortTime.usec > now.usec));
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
