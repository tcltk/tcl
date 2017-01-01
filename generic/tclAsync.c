/*
 * tclAsync.c --
 *
 *	This file provides low-level support needed to invoke signal handlers
 *	in a safe way. The code here doesn't actually handle signals, though.
 *	This code is based on proposals made by Mark Diekhans and Don Libes.
 *
 *	Extensions interact with this module through:
 *
 *	    Tcl_AsyncCreate() - create a new async token with a handler proc
 *	    Tcl_AsyncMark() - request that an async token be invoked ASAP
 *	    Tcl_AsyncDelete() - discard and invalidate an async token
 *
 *	Three constraints guide the implementation:
 *	  - Tcl_AsyncReady must be cheap - reading a single word
 *	  - async handlers must be invoked in the thread they were created in
 *	  - AsyncMark may be called at any time, by any thread, possibly in a
 *	    signal handler
 *
 *	To achieve cross-thread notifications within these constraints,
 *	a dedicated thread (AsyncThreadProc) listens for handler tokens
 *	from on a pipe.  Upon receiving a token, it does the appropriate
 *	mutex dance to look it up in the per-thread handler tables (to
 *	ensure liveness) and set asyncReady in the target thread.
 *
 *	This means Tcl_AsyncMark() only needs to write(2) (which is safe
 *	in signal handlers, unlike mutexes) for cross-thread notifications.
 *	Same-thread notifications can set asyncReady = 1 directly *without a
 *	mutex* because they are only ever contending with AsyncThreadProc,
 *	which also only ever sets asyncReady = 1.
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

#ifndef TCL_DEBUG_ASYNC
# define TCL_DEBUG_ASYNC 0
#endif
#if TCL_DEBUG_ASYNC
# define DEBUG(...) do { fprintf(stderr, "%s:%i:%s():", __FILE__, __LINE__, __func__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#else
# define DEBUG(...)
#endif


/* Forward declaration */
struct ThreadSpecificData;

/*
 * Structure describing a single asynchronous handler.  This
 * is part of a linked list, AsyncHandlerList
 */
typedef struct AsyncHandler {
    int ready;			/* Non-zero means this handler should be
				 * invoked in the next call to
				 * Tcl_AsyncInvoke. */
    struct AsyncHandler *nextPtr;
				/* Next in list of all handlers for the
				 * process. */
    Tcl_AsyncProc *proc;	/* Procedure to call when handler is
				 * invoked. */
    ClientData clientData;	/* Value to pass to handler when it is
				 * invoked. */
    struct ThreadSpecificData *originTsd;
				/* Used in Tcl_AsyncMark to modify thread-
				 * specific data from outside the thread it is
				 * associated to. */
    Tcl_ThreadId originThrdId;	/* Origin thread where this token was created
				 * and where it will be yielded. */
} AsyncHandler;

/*
 * The list of AsyncHandler* has a first and last pointer so we can
 * cheaply append.  One AsyncHandlerList is owned by each thread (in
 * ThreadSpecificData.handlers), and they are linked together for all
 * threads in SharedData.handlersList.
 */
typedef struct AsyncHandlerList {
    Tcl_Mutex mutex;
    AsyncHandler *first;
    AsyncHandler *last;
    struct AsyncHandlerList *next;
} AsyncHandlerList;

typedef struct ThreadSpecificData {
    /*
     * The variables below maintain a list of all existing handlers specific
     * to the calling thread.
     */
    int asyncReady;		/* This is set to 1 whenever a handler becomes
				 * ready and it is cleared to zero whenever
				 * Tcl_AsyncInvoke is called. It can be
				 * checked elsewhere in the application by
				 * calling Tcl_AsyncReady to see if
				 * Tcl_AsyncInvoke should be invoked. */
    int asyncActive;		/* Indicates whether Tcl_AsyncInvoke is
				 * currently working. If so then we won't set
				 * asyncReady again until Tcl_AsyncInvoke
				 * returns. */
    Tcl_Mutex asyncMutex;	/* asyncReady mutex */
    AsyncHandlerList handlers;	/* Private list of async handlers, member of a
				 * shared linked list */
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;


/*
 * Common data to all threads.  There is only one asyncThread, which reads
 * from readChan and scans handlersList to validate tokens.
 */
typedef struct SharedData {
    Tcl_Mutex mutex;			/* mutex used to guard initialisation */
    Tcl_ThreadId asyncThreadId;		/* identifier of the AsyncThreadProc thread */
    AsyncHandlerList * handlersList;	/* linked list of each thread's handlers */
    Tcl_Mutex handlersListMutex;	/* mutex for ^ */
    Tcl_Channel readChan, writeChan;	/* ends of the pipe for writing AsyncMark
					 * requests to the thread */
} SharedData;

static SharedData sharedData = { };

/*
 * The message sent over the async pipe.  This should be smaller than
 * 512 bytes (<= PIPE_BUF) to ensure atomic write() everywhere.
 */
typedef struct AsyncPacket {
    AsyncHandler * token;
} AsyncPacket;

/*
 * Lightweight IO on Tcl_Channels
 */
static int
TclChanWrite(Tcl_Channel chan, const char * bytes, size_t len) {
    const Tcl_ChannelType * typePtr;
    Tcl_DriverOutputProc * outputProc;
    ClientData instanceData;
    int errCode, rc;
    typePtr = Tcl_GetChannelType(chan);
    outputProc = Tcl_ChannelOutputProc(typePtr);
    instanceData = Tcl_GetChannelInstanceData(chan);
    rc = outputProc(instanceData, bytes, len, &errCode);
    Tcl_SetErrno(errCode);
    return rc;
}

static int
TclChanRead(Tcl_Channel chan, char * bytes, size_t len) {
    const Tcl_ChannelType * typePtr;
    Tcl_DriverInputProc * inputProc;
    ClientData instanceData;
    int errCode, rc;
    typePtr = Tcl_GetChannelType(chan);
    inputProc = Tcl_ChannelInputProc(typePtr);
    instanceData = Tcl_GetChannelInstanceData(chan);
    rc = inputProc(instanceData, bytes, len, &errCode);
    Tcl_SetErrno(errCode);
    return rc;
}

/*
 *----------------------------------------------------------------------
 *
 * AsyncThreadProc
 *
 * 	The async thread is responsible for delivering Tcl_AsyncMark
 * 	requests made from the "wrong" thread - ie, not the one from
 * 	which token was Tcl_AsyncCreate()d.
 *
 * 	It (blocking) read()s requests from a pipe(), and updates
 * 	async flags in the target thread under a mutex.  It needs to
 * 	first ensure token is a valid pointer into a live thread, so
 * 	some dancing through the linked lists is required.
 *
 *----------------------------------------------------------------------
 */
static Tcl_ThreadCreateType
AsyncThreadProc(ClientData clientData) {
    AsyncPacket packet;
    AsyncHandlerList ** listPtr;
    AsyncHandler ** asyncPtr;
    AsyncHandlerList * list;
    AsyncHandler * token;
    int rc;
    while (1) {
	DEBUG("Entering read(%d)", sizeof(packet));
	rc = TclChanRead(sharedData.readChan, (char *) &packet, sizeof(packet));
	if (rc != sizeof(packet)) {
	    break;
	}
	token = packet.token;
	DEBUG("Read token %p", token);

	/*
	 * first ensure it's not a dead token, by finding it in a running thread's
	 * handler list
	 */
	Tcl_MutexLock(&sharedData.handlersListMutex);
	// traverse the shared list of private lists ..
	for (listPtr = &sharedData.handlersList;
	     *listPtr;
	     listPtr = &(*listPtr)->next) {
	    Tcl_MutexLock(&(*listPtr)->mutex);
	    // traverse each private list ..
	    for (asyncPtr = &(*listPtr)->first;
		 *asyncPtr;
		 asyncPtr = &(*asyncPtr)->nextPtr) {
		if (*asyncPtr == token) break;
	    }
	    if (*asyncPtr == token) break;
	    Tcl_MutexUnlock(&(*listPtr)->mutex);
	}
	list = *listPtr;	// copy before unlocking the mutex!
	Tcl_MutexUnlock(&sharedData.handlersListMutex);
	/*
	 * (*listPtr)->mutex is still locked unless *asyncPtr == NULL,
	 * in which case the token is invalid or no longer exists.
	 */
	if (*asyncPtr == NULL) {
	    DEBUG("No such token!");
	    return;
	}

	/*
	 * Now set ready, asyncReady and alert the origin thread
	 */
	DEBUG("Setting ready for thread %d token %d", token->originTsd, token);
	token->ready = 1;
	Tcl_MutexLock(&token->originTsd->asyncMutex);
	if (!token->originTsd->asyncActive) {
	    token->originTsd->asyncReady = 1;
	    Tcl_ThreadAlert(token->originThrdId);
	}
	Tcl_MutexUnlock(&token->originTsd->asyncMutex);
	Tcl_MutexUnlock(&list->mutex);
    }
    DEBUG("fell out of async thread loop! (rc=%d, errno=%d)", rc, errno);
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitializeAsync --
 *
 * 	Starts the async thread if not already running.
 *
 * Results:
 * 	none
 *
 * Side effects:
 * 	creates a new thread with a pipe through which Tcl_AsyncMark
 * 	will write tokens.  The new thread is never stopped.
 *
 *----------------------------------------------------------------------
 */
void
TclInitializeAsync(void) {
    int pipefds[2];
    Tcl_Channel readChan, writeChan;
    Tcl_Interp * interp = NULL;
    int code;
    if (sharedData.asyncThreadId) {
	return;
    }
    DEBUG("Initialize\n");
    Tcl_MutexLock(&sharedData.mutex);
    /*
     * If any of these fail, we can't recover
     */
    if (pipe(pipefds) != 0) goto panic;
    fcntl(pipefds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipefds[1], F_SETFD, FD_CLOEXEC);
    readChan = Tcl_MakeFileChannel((ClientData)pipefds[0], TCL_READABLE);
    writeChan = Tcl_MakeFileChannel((ClientData)pipefds[1], TCL_WRITABLE);
    Tcl_RegisterChannel(interp, readChan);
    Tcl_RegisterChannel(interp, writeChan);
    code = Tcl_SetChannelOption(interp, writeChan, "-blocking", "0");
    if (code != TCL_OK) goto panic;
    code = Tcl_SetChannelOption(interp, readChan, "-blocking", "1");
    if (code != TCL_OK) goto panic;

    sharedData.writeChan = writeChan;
    sharedData.readChan = readChan;

    code = Tcl_CreateThread(&sharedData.asyncThreadId,
		    AsyncThreadProc, NULL,
		    TCL_THREAD_STACK_DEFAULT, TCL_THREAD_NOFLAGS);
    if (code != TCL_OK) goto panic;
    Tcl_MutexUnlock(&sharedData.mutex);
    return;
panic:
    Tcl_MutexUnlock(&sharedData.mutex);
    Tcl_Panic("TclInitializeAsync: unexpected failure setting up async pipe");
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeAsync --
 *
 * 	Clean up per-thread async data structures.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Async handler tokens associated with this thread will no longer
 *	be valid.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeAsync(void)
{
    ThreadSpecificData *tsdPtr;
    AsyncHandlerList ** listPtr;
    AsyncHandlerList * list;
    AsyncHandler ** asyncPtr;
    AsyncHandler * token;

    if (!sharedData.asyncThreadId) {
	return;
    }
    DEBUG("Finalize");
    tsdPtr = TCL_TSD_INIT(&dataKey);

    Tcl_MutexLock(&tsdPtr->handlers.mutex);
    /*
     * First, unlink our handlers from the shared list
     */
    Tcl_MutexLock(&sharedData.handlersListMutex);
    list = NULL;
    for (listPtr = &sharedData.handlersList;
	 *listPtr;
	 listPtr = &(*listPtr)->next) {
	if (*listPtr == &tsdPtr->handlers) {
	    list = *listPtr;			// take a copy
	    *listPtr = (*listPtr)->next;	// unlink it from the list
	    break;
	}
    }
    Tcl_MutexUnlock(&sharedData.handlersListMutex);
    if (list == NULL) {
	/*
	 * This is okay:  AsyncCreate may never have been called in this thread
	 */
	DEBUG("Can't find our handler list in sharedData!");
    }
    /* do not attempt to ckfree(list)! It is part of *tsdPtr */
    /*
     * Then, clean up all our handlers
     */
    asyncPtr = &tsdPtr->handlers.first;
    while (*asyncPtr) {
	/* .. finding elements that belong to us .. */
	if ((*asyncPtr)->originTsd == tsdPtr) {
	    /* .. and delete them */
	    token = *asyncPtr;
	    *asyncPtr = (*asyncPtr)->nextPtr;
	    ckfree((char *) token);
	    continue;
	}
	asyncPtr = &(*asyncPtr)->nextPtr;
    }
    Tcl_MutexUnlock(&tsdPtr->handlers.mutex);

    Tcl_MutexFinalize(&tsdPtr->handlers.mutex);
    if (tsdPtr->asyncMutex != NULL) {
	Tcl_MutexFinalize(&tsdPtr->asyncMutex);
	tsdPtr->asyncMutex = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AsyncCreate --
 *
 * 	Sets up a new asynchronous handler, setting up thread-private
 * 	and -shared resources so that it can be activated later without
 * 	requiring allocation or mutexes in Tcl_AsyncMark.  Handlers are
 * 	always run in the same thread they were created in.  Creation
 * 	order determines priority.
 *
 * 	Calls should be paired with Tcl_AsyncDelete to clean up resources.
 *
 * Results:
 *	The return value is a token for the handler, which can be used to
 *	activate it later on.
 *
 * Side effects:
 *	Information about the handler is recorded.  The async handler
 *	thread may be created (once per process), and this thread's
 *	handler list may be added to the shared list of handler lists
 *	(once per thread).
 *
 *----------------------------------------------------------------------
 */

Tcl_AsyncHandler
Tcl_AsyncCreate(
    Tcl_AsyncProc *proc,	/* Procedure to call when handler is
				 * invoked. */
    ClientData clientData)	/* Argument to pass to handler. */
{
    AsyncHandler * token;
    AsyncHandlerList ** listPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    TclInitializeAsync();
    /*
     * First ensure that tsdPtr->handlers is on sharedData->handlersList
     * FIXME: set a flag in sharedData to indicate this is done
     */
    Tcl_MutexLock(&sharedData.handlersListMutex);
    for (listPtr = &sharedData.handlersList;
	 *listPtr;
	 listPtr = &(*listPtr)->next) {
	if (*listPtr == &tsdPtr->handlers) break;
    }
    if (*listPtr == NULL) {
	*listPtr = &tsdPtr->handlers;
    }
    Tcl_MutexUnlock(&sharedData.handlersListMutex);

    /*
     * Create the new token and add it to tsdPtr->handlers
     */
    token = (AsyncHandler *) ckalloc(sizeof(AsyncHandler));
    DEBUG("Create %p %p\n", tsdPtr, token);
    token->ready = 0;
    token->nextPtr = NULL;
    token->proc = proc;
    token->clientData = clientData;
    token->originTsd = tsdPtr;
    token->originThrdId = Tcl_GetCurrentThread();

    Tcl_MutexLock(&tsdPtr->handlers.mutex);
    if (tsdPtr->handlers.first == NULL) {
	tsdPtr->handlers.first = token;
    } else {
	tsdPtr->handlers.last->nextPtr = token;
    }
    tsdPtr->handlers.last = token;
    Tcl_MutexUnlock(&tsdPtr->handlers.mutex);
    return (Tcl_AsyncHandler) token;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AsyncMark --
 *
 * 	Called to with an async token to request that its handler
 * 	be invoked as soon as possible, in the correct thread.  If the
 * 	handler belongs to the current thread, the very next call to
 * 	Tcl_AsyncReady will return true and this handler will be invoked
 * 	in the next call to Tcl_AsyncInvoke.  Handlers belonging to other
 * 	threads will be handled "soon" (see AsyncThreadProc).
 *
 * 	AsyncMark is designed to be used in an interrupt or signal handler
 * 	where it isn't safe to allocate or use mutexes.  For cross-thread
 * 	alerts it uses write(2).
 *
 * 	Calling AsyncMark with an invalid token, or a token that has
 * 	since been AsyncDelete'd (or had its thread exit) will silently
 * 	do nothing.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The handler gets marked for invocation later.  AsyncThreadProc
 *	may be woken up.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AsyncMark(
    Tcl_AsyncHandler asyncToken)	/* Token for handler. */
{
    AsyncHandler ** asyncPtr;
    AsyncPacket packet;
    int rc;
    AsyncHandler * token = (AsyncHandler *) asyncToken;

    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    DEBUG("Mark %p %p\n", tsdPtr, token);

    if (!sharedData.asyncThreadId) {
	Tcl_Panic("Tcl_AsyncMark called before Tcl_AsyncCreate");
    }

    /*
     * Check if the handler belongs to the current thread, in which case
     * we can set the ready flags immediately.  Otherwise, a message needs
     * to go to the async thread.
     *
     * We can get away with traversing the private handler list without
     * a mutex in here because the current thread is interrupted, and
     * the only activity from other threads might be setting ->ready=1.
     */
    asyncPtr = &tsdPtr->handlers.first;
    for (asyncPtr = &tsdPtr->handlers.first;
	 *asyncPtr;
	 asyncPtr = &(*asyncPtr)->nextPtr) {
	if (*asyncPtr == token) {
	    /* token belongs to this thread - set asyncReady immediately! */
	    token->ready = 1;
	    if (!tsdPtr->asyncActive) {
		tsdPtr->asyncReady = 1;
	    }
	    Tcl_ThreadAlert(token->originThrdId);	/* alert self, in case we're in eg WaitForEvent */
	    DEBUG("Marked my own token!");
	    return;
	}
    }

    /*
     * If we got here, the token belongs to another thread.  Send
     * it to AsyncThreadProc!
     */
    packet.token = (AsyncHandler *) token;

    rc = TclChanWrite(sharedData.writeChan, (char *) &packet, sizeof(packet));
    if (rc != sizeof(packet)) {
	Tcl_Panic("Tcl_AsyncMark: write error or short write to async pipe rc = %d[%d], errno = %d", rc, sizeof(packet), errno);
	return;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AsyncInvoke --
 *
 *	This procedure is called at a "safe" time at background level to
 *	invoke any active asynchronous handlers.
 *
 * Results:
 *	The return value is a normal Tcl result, which will replace
 *	the code argument as the current completion code for interp.
 *
 * Side effects:
 * 	All marked (asyncReady) handlers will be called in priority
 * 	order.  If more handlers become ready while these are executing,
 * 	they will be merged into the queue without interrupting the
 * 	handler already running.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AsyncInvoke(
    Tcl_Interp *interp,		/* If invoked from Tcl_Eval just after
				 * completing a command, points to
				 * interpreter. Otherwise it is NULL. */
    int code)			/* If interp is non-NULL, this gives
				 * completion code from command that just
				 * completed. */
{
    AsyncHandler *token;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    DEBUG("Invoke %p %d (%d)\n", tsdPtr, code, tsdPtr->asyncReady);

    Tcl_MutexLock(&tsdPtr->asyncMutex);

    if (tsdPtr->asyncReady == 0) {
	Tcl_MutexUnlock(&tsdPtr->asyncMutex);
	return code;
    }
    tsdPtr->asyncReady = 0;
    tsdPtr->asyncActive = 1;
    if (interp == NULL) {
	code = 0;
    }

    /*
     * Make one or more passes over the list of handlers, invoking at most one
     * handler in each pass. After invoking a handler, go back to the start of
     * the list again so that (a) if a new higher-priority handler gets marked
     * while executing a lower priority handler, we execute the higher-
     * priority handler next, and (b) if a handler gets deleted during the
     * execution of a handler, then the list structure may change so it isn't
     * safe to continue down the list anyway.
     */

    while (1) {
	Tcl_MutexLock(&tsdPtr->handlers.mutex);
	for (token = tsdPtr->handlers.first; token != NULL;
		token = token->nextPtr) {
	    if (token->ready) {
		break;
	    }
	}
	Tcl_MutexUnlock(&tsdPtr->handlers.mutex);
	if (token == NULL) {
	    break;
	}
	token->ready = 0;
	Tcl_MutexUnlock(&tsdPtr->asyncMutex);
	DEBUG("Invoke %p -> %p\n", tsdPtr, token);
	code = token->proc(token->clientData, interp, code);
	Tcl_MutexLock(&tsdPtr->asyncMutex);
    }
    tsdPtr->asyncActive = 0;
    Tcl_MutexUnlock(&tsdPtr->asyncMutex);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AsyncDelete --
 *
 *	Frees up all the state for an asynchronous handler set up
 *	with Tcl_AsyncCreate, which must have been called in the
 *	same thread.  Subsequent calls to Tcl_AsyncMark() using
 *	a deleted token will have no effect.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The state associated with the handler is deleted.
 *
 *	Attempting to delete a token from a thread other than the
 *	one it was Tcl_AsyncCreate()d in, or attempting to delete
 *	a token more than once, will result in panic.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AsyncDelete(
    Tcl_AsyncHandler async)		/* Token for handler to delete. */
{
    AsyncHandler *token = (AsyncHandler *) async;
    AsyncHandler *prevPtr, *thisPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    DEBUG("Delete %p %p\n", tsdPtr, token);

    /*
     * Assure early handling of the constraint
     */

    if (token->originThrdId != Tcl_GetCurrentThread()) {
	Tcl_Panic("Tcl_AsyncDelete: async handler deleted by the wrong thread");
    }

    /*
     * If we come to this point when TSD's for the current
     * thread have already been garbage-collected, we are
     * in the _serious_ trouble. OTOH, we tolerate calling
     * with already cleaned-up handler list (should we?).
     */

    /*
     * Remove `token` from handlers list.  Iterate with two
     * pointers, so we can fix up lastPtr if removing the
     * last element.
     */
    Tcl_MutexLock(&tsdPtr->handlers.mutex);
    if (tsdPtr->handlers.first != NULL) {
	prevPtr = thisPtr = tsdPtr->handlers.first;
	while (thisPtr != NULL && thisPtr != token) {
	    prevPtr = thisPtr;
	    thisPtr = thisPtr->nextPtr;
	}
	if (thisPtr == NULL) {
	    Tcl_Panic("Tcl_AsyncDelete: cannot find async handler");
	}
	if (token == tsdPtr->handlers.first) {
	    tsdPtr->handlers.first = token->nextPtr;
	} else {
	    prevPtr->nextPtr = token->nextPtr;
	}
	if (token == tsdPtr->handlers.last) {
	    tsdPtr->handlers.last = prevPtr;
	}
    }
    Tcl_MutexUnlock(&tsdPtr->handlers.mutex);
    ckfree((char *) token);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AsyncReady --
 *
 *	This procedure can be used to tell whether Tcl_AsyncInvoke needs
 *	to be called. This procedure is the external interface for
 *	checking the thread-specific asyncReady variable.
 *
 * Results:
 *	Return 1 whenever a handler is ready and is 0 when no handlers
 *	are ready.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AsyncReady(void)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    DEBUG("Ready %p (%d)\n", tsdPtr, tsdPtr->asyncReady);
    return tsdPtr->asyncReady;
}

int *
TclGetAsyncReadyPtr(void)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    return &(tsdPtr->asyncReady);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
