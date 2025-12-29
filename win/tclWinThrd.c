/*
 * tclWinThread.c --
 *
 *	This file implements the Windows-specific thread operations.
 *
 * Copyright © 1998 Sun Microsystems, Inc.
 * Copyright © 1999 Scriptics Corporation
 * Copyright © 2008 George Peter Staplin
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclWinInt.h"

/* Workaround for mingw versions which don't provide this in float.h */
#ifndef _MCW_EM
#   define	_MCW_EM		0x0008001F	/* Error masks */
#   define	_MCW_RC		0x00000300	/* Rounding */
#   define	_MCW_PC		0x00030000	/* Precision */
_CRTIMP unsigned int __cdecl _controlfp (unsigned int unNew, unsigned int unMask);
#endif

/*
 * This is the global lock used to serialize access to other serialization
 * data structures.
 */

static CRITICAL_SECTION globalLock;
static int initialized = 0;

/*
 * This is the global lock used to serialize initialization and finalization
 * of Tcl as a whole.
 */

static CRITICAL_SECTION initLock;

/*
 * allocLock is used by Tcl's version of malloc for synchronization. For
 * obvious reasons, cannot use any dynamically allocated storage.
 */

#if TCL_THREADS

/*
 * Although CRITICAL_SECTIONs can be nested, we need to keep track
 * of their lock counts for condition variables.
 */

typedef struct WMutex {
    CRITICAL_SECTION crit;
    volatile LONG thread;
    int counter;
} WMutex;

static struct WMutex allocLock;
static WMutex *allocLockPtr = &allocLock;
static int allocOnce = 0;

#endif /* TCL_THREADS */

/*
 * The joinLock serializes Create- and ExitThread. This is necessary to
 * prevent a race where a new joinable thread exits before the creating thread
 * had the time to create the necessary data structures in the emulation
 * layer.
 */

static CRITICAL_SECTION joinLock;

/*
 * Additions by AOL for specialized thread memory allocator.
 */

#ifdef USE_THREAD_ALLOC
static DWORD tlsKey;

typedef struct {
    Tcl_Mutex tlock;
    WMutex wm;
} allocMutex;
#endif /* USE_THREAD_ALLOC */

static void WMutexInit(WMutex *);
static void WMutexDestroy(WMutex *);

/*
 * The per thread data passed from TclpThreadCreate
 * to TclWinThreadStart.
 */

typedef struct {
    LPTHREAD_START_ROUTINE lpStartAddress;
				/* Original startup routine */
    LPVOID lpParameter;		/* Original startup data */
    unsigned int fpControl;	/* Floating point control word from the
				 * main thread */
} WinThread;

/*
 *----------------------------------------------------------------------
 *
 * TclWinThreadStart --
 *
 *	This procedure is the entry point for all new threads created
 *	by Tcl on Windows.
 *
 * Results:
 *	Various, depending on the result of the wrapped thread start
 *	routine.
 *
 * Side effects:
 *	Arbitrary, since user code is executed.
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
TclWinThreadStart(
    LPVOID lpParameter)		/* The WinThread structure pointer passed
				 * from TclpThreadCreate */
{
    WinThread *winThreadPtr = (WinThread *) lpParameter;
    LPTHREAD_START_ROUTINE lpOrigStartAddress;
    LPVOID lpOrigParameter;

    if (!winThreadPtr) {
	return TCL_ERROR;
    }

    _controlfp(winThreadPtr->fpControl, _MCW_EM | _MCW_RC | 0x03000000 /* _MCW_DN */
#if !defined(_WIN64)
	    | _MCW_PC
#endif
    );

    lpOrigStartAddress = winThreadPtr->lpStartAddress;
    lpOrigParameter = winThreadPtr->lpParameter;

    Tcl_Free(winThreadPtr);
    return lpOrigStartAddress(lpOrigParameter);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpThreadCreate --
 *
 *	This procedure creates a new thread.
 *
 * Results:
 *	TCL_OK if the thread could be created. The thread ID is returned in a
 *	parameter.
 *
 * Side effects:
 *	A new thread is created.
 *
 *----------------------------------------------------------------------
 */

int
TclpThreadCreate(
    Tcl_ThreadId *idPtr,	/* Return, the ID of the thread. */
    Tcl_ThreadCreateProc *proc,	/* Main() function of the thread. */
    void *clientData,		/* The one argument to Main(). */
    size_t stackSize,		/* Size of stack for the new thread. */
    int flags)			/* Flags controlling behaviour of the new
				 * thread. */
{
    WinThread *winThreadPtr;	/* Per-thread startup info */
    HANDLE tHandle;

    winThreadPtr = (WinThread *)Tcl_Alloc(sizeof(WinThread));
    winThreadPtr->lpStartAddress = (LPTHREAD_START_ROUTINE) proc;
    winThreadPtr->lpParameter = clientData;
    winThreadPtr->fpControl = _controlfp(0, 0);

    EnterCriticalSection(&joinLock);

    *idPtr = 0;		/* must initialize as Tcl_Thread is a pointer and
			 * on WIN64 sizeof void* != sizeof unsigned */

#if defined(_MSC_VER) || defined(__MSVCRT__)
    tHandle = (HANDLE) _beginthreadex(NULL, (unsigned)stackSize,
	    (Tcl_ThreadCreateProc*) TclWinThreadStart, winThreadPtr,
	    0, (unsigned *)idPtr);
#else
    tHandle = CreateThread(NULL, (DWORD)stackSize,
	    TclWinThreadStart, winThreadPtr, 0, (LPDWORD)idPtr);
#endif

    if (tHandle == NULL) {
	LeaveCriticalSection(&joinLock);
	return TCL_ERROR;
    } else {
	if (flags & TCL_THREAD_JOINABLE) {
	    TclRememberJoinableThread(*idPtr);
	}

	/*
	 * The only purpose of this is to decrement the reference count so the
	 * OS resources will be reacquired when the thread closes.
	 */

	CloseHandle(tHandle);
	LeaveCriticalSection(&joinLock);
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_JoinThread --
 *
 *	This procedure waits upon the exit of the specified thread.
 *
 * Results:
 *	TCL_OK if the wait was successful, TCL_ERROR else.
 *
 * Side effects:
 *	The result area is set to the exit code of the thread we
 *	waited upon.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_JoinThread(
    Tcl_ThreadId threadId,	/* Id of the thread to wait upon */
    int *result)		/* Reference to the storage the result of the
				 * thread we wait upon will be written into. */
{
    return TclJoinThread(threadId, result);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpThreadExit --
 *
 *	This procedure terminates the current thread.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This procedure terminates the current thread.
 *
 *----------------------------------------------------------------------
 */

TCL_NORETURN void
TclpThreadExit(
    int status)
{
    EnterCriticalSection(&joinLock);
    TclSignalExitThread(Tcl_GetCurrentThread(), status);
    LeaveCriticalSection(&joinLock);

#if defined(_MSC_VER) || defined(__MSVCRT__)
    _endthreadex((unsigned) status);
#else
    ExitThread((DWORD) status);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetCurrentThread --
 *
 *	This procedure returns the ID of the currently running thread.
 *
 * Results:
 *	A thread ID.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_ThreadId
Tcl_GetCurrentThread(void)
{
    return (Tcl_ThreadId)INT2PTR(GetCurrentThreadId());
}

/*
 *----------------------------------------------------------------------
 *
 * TclpInitLock
 *
 *	This procedure is used to grab a lock that serializes initialization
 *	and finalization of Tcl. On some platforms this may also initialize
 *	the mutex used to serialize creation of more mutexes and thread local
 *	storage keys.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Acquire the initialization mutex.
 *
 *----------------------------------------------------------------------
 */

void
TclpInitLock(void)
{
    if (!initialized) {
	/*
	 * There is a fundamental race here that is solved by creating the
	 * first Tcl interpreter in a single threaded environment. Once the
	 * interpreter has been created, it is safe to create more threads
	 * that create interpreters in parallel.
	 */

	initialized = 1;
	InitializeCriticalSection(&joinLock);
	InitializeCriticalSection(&initLock);
	InitializeCriticalSection(&globalLock);
    }
    EnterCriticalSection(&initLock);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpInitUnlock
 *
 *	This procedure is used to release a lock that serializes
 *	initialization and finalization of Tcl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Release the initialization mutex.
 *
 *----------------------------------------------------------------------
 */

void
TclpInitUnlock(void)
{
    LeaveCriticalSection(&initLock);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGlobalLock
 *
 *	This procedure is used to grab a lock that serializes creation of
 *	mutexes, condition variables, and thread local storage keys.
 *
 *	This lock must be different than the initLock because the initLock is
 *	held during creation of synchronization objects.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Acquire the global mutex.
 *
 *----------------------------------------------------------------------
 */

void
TclpGlobalLock(void)
{
    if (!initialized) {
	/*
	 * There is a fundamental race here that is solved by creating the
	 * first Tcl interpreter in a single threaded environment. Once the
	 * interpreter has been created, it is safe to create more threads
	 * that create interpreters in parallel.
	 */

	initialized = 1;
	InitializeCriticalSection(&joinLock);
	InitializeCriticalSection(&initLock);
	InitializeCriticalSection(&globalLock);
    }
    EnterCriticalSection(&globalLock);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGlobalUnlock
 *
 *	This procedure is used to release a lock that serializes creation and
 *	deletion of synchronization objects.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Release the global mutex.
 *
 *----------------------------------------------------------------------
 */

void
TclpGlobalUnlock(void)
{
    LeaveCriticalSection(&globalLock);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetAllocMutex
 *
 *	This procedure returns a pointer to a statically initialized mutex for
 *	use by the memory allocator. The allocator must use this lock, because
 *	all other locks are allocated...
 *
 * Results:
 *	A pointer to a mutex that is suitable for passing to Tcl_MutexLock and
 *	Tcl_MutexUnlock.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Mutex *
Tcl_GetAllocMutex(void)
{
#if TCL_THREADS
    if (!allocOnce) {
	WMutexInit(&allocLock);
	allocOnce = 1;
    }
    return (Tcl_Mutex *) &allocLockPtr;
#else
    return NULL;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeLock
 *
 *	This procedure is used to destroy all private resources used in this
 *	file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys everything private. TclpInitLock must be held entering this
 *	function.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeLock(void)
{
    TclpGlobalLock();
    DeleteCriticalSection(&joinLock);

    /*
     * Destroy the critical section that we are holding!
     */

    DeleteCriticalSection(&globalLock);
    initialized = 0;

#if TCL_THREADS
    if (allocOnce) {
	WMutexDestroy(&allocLock);
	allocOnce = 0;
    }
#endif

    LeaveCriticalSection(&initLock);

    /*
     * Destroy the critical section that we were holding.
     */

    DeleteCriticalSection(&initLock);
}

#if TCL_THREADS

static void
WMutexInit(
    WMutex *wmPtr)
{
    wmPtr->thread = 0;
    wmPtr->counter = 0;
    InitializeCriticalSection(&wmPtr->crit);
}

static void
WMutexDestroy(
    WMutex *wmPtr)
{
    DeleteCriticalSection(&wmPtr->crit);
    assert(wmPtr->thread == 0 && wmPtr->counter == 0);
}

static void
WMutexLock(
    WMutex *wmPtr)
{
    LONG mythread = GetCurrentThreadId();

    if (wmPtr->thread == mythread) {
	// We owned the lock already, so it's recursive.
	wmPtr->counter++;
    } else {
	// We don't own the lock, so we can safely lock it. Then we own it.
	EnterCriticalSection(&wmPtr->crit);
	wmPtr->thread = mythread;
    }
}

static void
WMutexUnlock(
    WMutex *wmPtr)
{
    assert(wmPtr->thread == GetCurrentThreadId());
    if (wmPtr->counter) {
	// It's recursive
	wmPtr->counter--;
    } else {
	wmPtr->thread = 0;
	LeaveCriticalSection(&wmPtr->crit);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_MutexLock --
 *
 *	This procedure is invoked to lock a mutex. This is a self initializing
 *	mutex that is automatically finalized during Tcl_Finalize.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May block the current thread. The mutex is acquired when this returns.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_MutexLock(
    Tcl_Mutex *mutexPtr)	/* Really (WMutex **) */
{
    WMutex *wmPtr;

    if (*mutexPtr == NULL) {
	TclpGlobalLock();

	/*
	 * Double inside global lock check to avoid a race.
	 */

	if (*mutexPtr == NULL) {
	    wmPtr = (WMutex *) Tcl_Alloc(sizeof(WMutex));
	    WMutexInit(wmPtr);
	    *mutexPtr = (Tcl_Mutex) wmPtr;
	    TclRememberMutex(mutexPtr);
	}
	TclpGlobalUnlock();
    }
    wmPtr = *((WMutex **)mutexPtr);
    WMutexLock(wmPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MutexUnlock --
 *
 *	This procedure is invoked to unlock a mutex.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The mutex is released when this returns.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_MutexUnlock(
    Tcl_Mutex *mutexPtr)	/* Really (WMutex **) */
{
    WMutex *wmPtr = *((WMutex **)mutexPtr);
    WMutexUnlock(wmPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpFinalizeMutex --
 *
 *	This procedure is invoked to clean up one mutex. This is only safe to
 *	call at the end of time.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The mutex list is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TclpFinalizeMutex(
    Tcl_Mutex *mutexPtr)	/* Really (WMutex **) */
{
    WMutex *wmPtr = *(WMutex **)mutexPtr;

    if (wmPtr != NULL) {
	WMutexDestroy(wmPtr);
	Tcl_Free(wmPtr);
	*mutexPtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConditionWait --
 *
 *	This procedure is invoked to wait on a condition variable. The mutex
 *	is atomically released as part of the wait, and automatically grabbed
 *	when the condition is signaled.
 *
 *	The mutex must be held when this procedure is called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May block the current thread. The mutex is acquired when this returns.
 *	Will allocate memory for a CONDITION_VARIABLE and initialize the first
 *	time this Tcl_Condition is used.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ConditionWait(
    Tcl_Condition *condPtr,	/* Really (WinCondition **) */
    Tcl_Mutex *mutexPtr,	/* Really (CRITICAL_SECTION **) */
    const Tcl_Time *timePtr)	/* Timeout on waiting period */
{
    CONDITION_VARIABLE *cvPtr;	/* Per-condition queue head */
    WMutex *wmPtr;		/* Caller's Mutex, after casting */
    DWORD wtime;		/* Windows time value */

    if (timePtr == NULL) {
	wtime = INFINITE;
    } else {
	wtime = (DWORD)timePtr->sec * 1000 + (DWORD)timePtr->usec / 1000;
    }

    if (*condPtr == NULL) {
	TclpGlobalLock();
	if (*condPtr == NULL) {
	    cvPtr = (CONDITION_VARIABLE *)Tcl_Alloc(sizeof(*cvPtr));
	    InitializeConditionVariable(cvPtr);
	    *condPtr = (Tcl_Condition) cvPtr;
	    TclRememberCondition(condPtr);
	}
	TclpGlobalUnlock();
    }
    wmPtr = *((WMutex **)mutexPtr);
    cvPtr = *((CONDITION_VARIABLE **)condPtr);

    int counter = wmPtr->counter;
    wmPtr->counter = 0;
    LONG mythread = GetCurrentThreadId();
    assert(wmPtr->thread == mythread);
    wmPtr->thread = 0;
    if (SleepConditionVariableCS(cvPtr,
	    &wmPtr->crit, wtime) == 0) {
	DWORD err = GetLastError();
	if (err != ERROR_TIMEOUT) {
	    Tcl_Panic(
		"Tcl_ConditionWait: SleepConditionVariableCS error %lu",
		err);
	}
    }

    wmPtr->counter = counter;
    wmPtr->thread = mythread;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConditionNotify --
 *
 *	This procedure is invoked to signal a condition variable.
 *
 *	The mutex must be held during this call to avoid races, but this
 *	interface does not enforce that.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May unblock another thread.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ConditionNotify(
    Tcl_Condition *condPtr)
{
    CONDITION_VARIABLE *cvPtr;

    /* If uninitialized, no could be waiting on the condition variable */
    if (*condPtr != NULL) {
	cvPtr = *((CONDITION_VARIABLE **)condPtr);

	if (cvPtr) {
	    WakeAllConditionVariable(cvPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpFinalizeCondition --
 *
 *	This procedure is invoked to clean up a condition variable. This is
 *	only safe to call at the end of time.
 *
 *	This assumes the Global Lock is held.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The condition variable is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TclpFinalizeCondition(
    Tcl_Condition *condPtr)
{
    CONDITION_VARIABLE *cvPtr = *(CONDITION_VARIABLE **)condPtr;

    if (cvPtr) {
	Tcl_Free(cvPtr);
	*condPtr = NULL;
    }
}

/*
 * Additions by AOL for specialized thread memory allocator.
 */
#ifdef USE_THREAD_ALLOC

Tcl_Mutex *
TclpNewAllocMutex(void)
{
    allocMutex *lockPtr;

    lockPtr = (allocMutex *)malloc(sizeof(allocMutex));
    if (lockPtr == NULL) {
	Tcl_Panic("could not allocate lock");
    }
    lockPtr->tlock = (Tcl_Mutex)&lockPtr->wm;
    WMutexInit(&lockPtr->wm);
    return &lockPtr->tlock;
}

void
TclpFreeAllocMutex(
    Tcl_Mutex *mutex)		/* The alloc mutex to free. */
{
    allocMutex *lockPtr = (allocMutex *) mutex;

    if (!lockPtr || !lockPtr->tlock) {
	return;
    }
    lockPtr->tlock = NULL;
    WMutexDestroy(&lockPtr->wm);
    free(lockPtr);
}

void
TclpInitAllocCache(void)
{
    /*
     * We need to make sure that TclpFreeAllocCache is called on each
     * thread that calls this, but only on threads that call this.
     */

    tlsKey = TlsAlloc();
    if (tlsKey == TLS_OUT_OF_INDEXES) {
	Tcl_Panic("could not allocate thread local storage");
    }
}

void *
TclpGetAllocCache(void)
{
    void *result;
    result = TlsGetValue(tlsKey);
    if ((result == NULL) && (GetLastError() != NO_ERROR)) {
	Tcl_Panic("TlsGetValue failed from TclpGetAllocCache");
    }
    return result;
}

void
TclpSetAllocCache(
    void *ptr)
{
    BOOL success;
    success = TlsSetValue(tlsKey, ptr);
    if (!success) {
	Tcl_Panic("TlsSetValue failed from TclpSetAllocCache");
    }
}

void
TclpFreeAllocCache(
    void *ptr)
{
    BOOL success;

    if (ptr != NULL) {
	/*
	 * Called by TclFinalizeThreadAlloc() and
	 * TclFinalizeThreadAllocThread() during Tcl_Finalize() or
	 * Tcl_FinalizeThread(). This function destroys the tsd key which
	 * stores allocator caches in thread local storage.
	 */

	TclFreeAllocCache(ptr);
	success = TlsSetValue(tlsKey, NULL);
	if (!success) {
	    Tcl_Panic("TlsSetValue failed from TclpFreeAllocCache");
	}
    } else {
	/*
	 * Called by us in TclFinalizeThreadAlloc() during the library
	 * finalization initiated from Tcl_Finalize()
	 */

	success = TlsFree(tlsKey);
	if (!success) {
	    Tcl_Panic("TlsFree failed from TclpFreeAllocCache");
	}
    }
}
#endif /* USE_THREAD_ALLOC */

void *
TclpThreadCreateKey(void)
{
    DWORD *key;

    key = (DWORD *)TclpSysAlloc(sizeof *key);
    if (key == NULL) {
	Tcl_Panic("unable to allocate thread key!");
    }

    *key = TlsAlloc();

    if (*key == TLS_OUT_OF_INDEXES) {
	Tcl_Panic("unable to allocate thread-local storage");
    }

    return key;
}

void
TclpThreadDeleteKey(
    void *keyPtr)
{
    DWORD *key = (DWORD *)keyPtr;

    if (!TlsFree(*key)) {
	Tcl_Panic("unable to delete key");
    }

    TclpSysFree(keyPtr);
}

void
TclpThreadSetGlobalTSD(
    void *tsdKeyPtr,
    void *ptr)
{
    DWORD *key = (DWORD *)tsdKeyPtr;

    if (!TlsSetValue(*key, ptr)) {
	Tcl_Panic("unable to set global TSD value");
    }
}

void *
TclpThreadGetGlobalTSD(
    void *tsdKeyPtr)
{
    DWORD *key = (DWORD *)tsdKeyPtr;

    return TlsGetValue(*key);
}

#endif /* TCL_THREADS */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
