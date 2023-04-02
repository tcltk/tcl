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

static struct tclWinGlobals {
    INIT_ONCE init_once;	/* must be first member,
		because of static initialziation */

    /*
     * This is the global lock used to serialize access to other serialization
     * data structures.
     */
    CRITICAL_SECTION globalLock;

    /*
     * This is the global lock used to serialize initialization and finalization
     * of Tcl as a whole.
     */

    CRITICAL_SECTION initLock;

    /* holds the TLS key for the Tcl Thread_ID */
    DWORD tls_thread_id;

    /*
     * allocLock is used by Tcl's version of malloc for synchronization. For
     * obvious reasons, cannot use any dyamically allocated storage.
     */

#if TCL_THREADS

    struct Tcl_Mutex_ {
	CRITICAL_SECTION crit;
    } allocLock;
    Tcl_Mutex allocLockPtr;

#endif /* TCL_THREADS */

} TclWinGlobals = {
    INIT_ONCE_STATIC_INIT
};

#if TCL_THREADS
typedef struct Tcl_Condition_ {
    CONDITION_VARIABLE cv;
} WinCondition;

#endif /* TCL_THREADS */


/*
 * The per thread data passed from TclpThreadCreate
 * to TclWinThreadStart.
 */

typedef struct {
  LPTHREAD_START_ROUTINE lpStartAddress; /* Original startup routine */
  LPVOID lpParameter;		/* Original startup data */
  unsigned int fpControl;	/* Floating point control word from the
				 * main thread */
  HANDLE tHandle;		/* used to wait on thread completion */ 
# if defined(_MSC_VER) || defined(__MSVCRT__)
  unsigned
# else
  DWORD
# endif
    thread_id;		/* initialized but unused,
			possibly useful for debugging */ 
} WinThread;

/* assuming alignment constraints result in lsb being constant 0 */
# define	TCL_WIN_THREAD_JOIN_FLAG	((size_t) 1 << 0)

# define	TCL_WIN_THREAD_ID(PTR, JOINABLE)\
	    ((Tcl_ThreadId) ((sizeof ((PTR) - (WinThread *) 0),\
	    (size_t) (PTR)) | ((JOINABLE)? TCL_WIN_THREAD_JOIN_FLAG: 0)))

# define	TCL_WIN_THREAD_PTR(ID)\
	    ((WinThread *) ((sizeof ((ID) == (Tcl_ThreadId) 0),\
	    (size_t) (ID) & ~TCL_WIN_THREAD_JOIN_FLAG)))

# define	TCL_WIN_THREAD_JOINABLE(ID)\
	    (((sizeof ((ID) == (Tcl_ThreadId) 0),\
	    (size_t) (ID) & TCL_WIN_THREAD_JOIN_FLAG)))


/*
 *----------------------------------------------------------------------
 *
 * TclWinFailure --
 *
 *  	Call this when an unexpected WIN32 error occurs
 *
 * Does not return
 *
 *----------------------------------------------------------------------
 */

void TCL_NORETURN WINAPI
TclWinFailure (
    char const *const routine)
{
    Tcl_Panic ("%s failed, last error = %#x\n", routine, GetLastError ());
}

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
    WinThread *const winThreadPtr = (WinThread *) lpParameter;
    static DWORD result;

    if (!winThreadPtr) {
	return TCL_ERROR;
    }

    _controlfp(winThreadPtr->fpControl, _MCW_EM | _MCW_RC | 0x03000000 /* _MCW_DN */
#if !defined(_WIN64)
	    | _MCW_PC
#endif
    );

    if (!TlsSetValue (TclWinGlobals.tls_thread_id, winThreadPtr))
	TclWinFailure ("TlsSetValue");
    result = winThreadPtr->lpStartAddress (winThreadPtr->lpParameter);
    if (!winThreadPtr->tHandle)
	TclpSysFree (winThreadPtr);
    return result;
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
    void *clientData,	/* The one argument to Main(). */
    size_t stackSize,		/* Size of stack for the new thread. */
    int flags)			/* Flags controlling behaviour of the new
				 * thread. */
{
    WinThread *winThreadPtr;		/* Per-thread startup info */
    HANDLE tHandle;

    winThreadPtr = (WinThread *) TclpSysAlloc (sizeof *winThreadPtr);
    if (!winThreadPtr)
	TclWinFailure ("memory allocation");
    winThreadPtr->lpStartAddress = (LPTHREAD_START_ROUTINE) proc;
    winThreadPtr->lpParameter = clientData;
    winThreadPtr->fpControl = _controlfp(0, 0);

#if defined(_MSC_VER) || defined(__MSVCRT__)
    tHandle = (HANDLE) _beginthreadex(NULL, (unsigned) stackSize,
	    (Tcl_ThreadCreateProc*) TclWinThreadStart, winThreadPtr,
	    0 /* flags */,  &winThreadPtr->thread_id);
#else
    tHandle = CreateThread(NULL, (DWORD) stackSize,
	    TclWinThreadStart, winThreadPtr, 0 /* flags */,
	    &winThreadPtr->thread_id);
#endif

    if (!tHandle) {
	TclpSysFree (winThreadPtr);
# if	1
	TclWinFailure ("CreateThread");
# else
	return TCL_ERROR;
# endif
    } else {
	if (!(flags & TCL_THREAD_JOINABLE)) {
	    /*
	     * The only purpose of this is to decrement the reference count
	     * so the OS resources will be reacquired when the thread closes.
	     */
	    if (!CloseHandle (tHandle))
		TclWinFailure ("ClosseHandle");
	    /* note: the WIN32 OpenHandle() can be used to open a HANDLE
	    from the given threadID. However, if the thread
	    terminates with a HANDLE count of zero, the kernel may clean up
	    the thread, in particular its return status, immediately.
	    Hence, joining the thread requires keeping a HANDLE open
	    until the thread has been successfully joined. */ 

	    tHandle = 0;
	}
	winThreadPtr->tHandle = tHandle;

	*idPtr = TCL_WIN_THREAD_ID (winThreadPtr, 0 != tHandle);
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
    if (!TCL_WIN_THREAD_JOINABLE (threadId))
	return TCL_ERROR;

    WinThread *winThreadPtr = TCL_WIN_THREAD_PTR (threadId);
    HANDLE const tHandle = winThreadPtr->tHandle;
    switch (WaitForSingleObject (tHandle, INFINITE)) {
	DWORD exit_code;
    default:
	return TCL_ERROR;
    case WAIT_FAILED:
	TclWinFailure ("WaitForSingleObject");
    case WAIT_OBJECT_0:
	if (!GetExitCodeThread (tHandle, &exit_code))
	    TclWinFailure ("GetExitCodeThread");
	*result = exit_code;
	if (!CloseHandle (tHandle))
	    TclWinFailure ("CloseHandle");
	TclpSysFree (winThreadPtr);
	return TCL_OK;
    }
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

void
TclpThreadExit(
    int status)
{
    WinThread *winThreadPtr;
    if (winThreadPtr = (WinThread *) TlsGetValue (
	    TclWinGlobals.tls_thread_id)) {
	if (!winThreadPtr->tHandle)
	    TclpSysFree (winThreadPtr);
    }
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
    WinThread *winThreadPtr;
    if (!(winThreadPtr = (WinThread *) TlsGetValue (
	    TclWinGlobals.tls_thread_id))) {
	if (ERROR_SUCCESS != GetLastError ())
	    TclWinFailure ("TlsGetValue in Tcl_GetCurrentThread");
	/* this is an initial thread */
	/* can't use TclAlloc, as the USE_THREAD_ALLOC per-thread allocator
	calls Tcl_GetCurrentThread() */
	if (!(winThreadPtr = (WinThread *) TclpSysAlloc (sizeof *winThreadPtr)))
	    TclWinFailure ("memory allocation");
	winThreadPtr->tHandle = 0;
	winThreadPtr->thread_id = GetCurrentThreadId ();
	if (!TlsSetValue (TclWinGlobals.tls_thread_id, winThreadPtr))
	    TclWinFailure ("TlsSetValue");
    }
    return TCL_WIN_THREAD_ID (winThreadPtr, 0 != winThreadPtr->tHandle);
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinInitOnce
 *
 *	Perform module initialization exactly once
 *	storage keys.
 *
 * Results:
 *	None.
 *
 *
 *----------------------------------------------------------------------
 */
static BOOL
TclWinInitOnce (
    INIT_ONCE *init_once,
    void *parameter,
    void *context)
{
    struct tclWinGlobals *const twg = parameter;

    InitializeCriticalSection(&twg->initLock);
    InitializeCriticalSection(&twg->globalLock);
    twg->tls_thread_id = TlsAlloc ();

#if TCL_THREADS
    InitializeCriticalSection(&twg->allocLock.crit);
    twg->allocLockPtr = &twg->allocLock;
#endif /* TCL_THREADS */

    return TRUE;
}

# define	TCL_WIN_INITIALIZE_ONCE()\
	InitOnceExecuteOnce (&TclWinGlobals.init_once,\
	    TclWinInitOnce,\
	    &TclWinGlobals,\
	    0)


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
    if (!TCL_WIN_INITIALIZE_ONCE ())
	TclWinFailure ("InitOnceExecuteOnce");
    EnterCriticalSection(&TclWinGlobals.initLock);
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
    LeaveCriticalSection(&TclWinGlobals.initLock);
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
    if (!TCL_WIN_INITIALIZE_ONCE ())
	TclWinFailure ("InitOnceExecuteOnce");
    EnterCriticalSection(&TclWinGlobals.globalLock);
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
    LeaveCriticalSection(&TclWinGlobals.globalLock);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetAllocMutex
 *
 *	This procedure returns a pointer to a statically initialized mutex for
 *	use by the memory allocator. The alloctor must use this lock, because
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
    if (!TCL_WIN_INITIALIZE_ONCE ())
	TclWinFailure ("InitOnceExecuteOnce");
    return &TclWinGlobals.allocLockPtr;
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
    struct tclWinGlobals *const twg = &TclWinGlobals;
    TclpGlobalLock();

    if (!TlsFree (twg->tls_thread_id))
	TclWinFailure ("TlsFree");

    /*
     * Destroy the critical section that we are holding!
     */

    DeleteCriticalSection(&twg->globalLock);

#if TCL_THREADS
    DeleteCriticalSection(&twg->allocLock.crit);
#endif

    LeaveCriticalSection(&twg->initLock);

    /*
     * Destroy the critical section that we were holding.
     */

    DeleteCriticalSection(&twg->initLock);

    InitOnceInitialize (&twg->init_once);	/* assuming we want to
	re-initialize at some point -- dubious */
}

#if TCL_THREADS

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
    Tcl_Mutex *mutexPtr)	/* The lock */
{
    struct Tcl_Mutex_ *mp;

    if (*mutexPtr == NULL) {
	TclpGlobalLock();

	/*
	 * Double inside global lock check to avoid a race.
	 * BROKEN: does work in relaxed memory models
	 */

	if (*mutexPtr == NULL) {
	    mp = Tcl_Alloc (sizeof *mp);
	    InitializeCriticalSection (&mp->crit);
	    *mutexPtr = mp;
	    TclRememberMutex(mutexPtr);
	}
	TclpGlobalUnlock();
    }
    EnterCriticalSection (&(*mutexPtr)->crit);
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
    Tcl_Mutex *mutexPtr)	/* The lock */
{
    LeaveCriticalSection (&(*mutexPtr)->crit);
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
    Tcl_Mutex *mutexPtr)
{
    if (*mutexPtr) {
	DeleteCriticalSection(&(*mutexPtr)->crit);
	Tcl_Free(*mutexPtr);
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
 *	Will allocate memory for a HANDLE and initialize this the first time
 *	this Tcl_Condition is used.
 *
 *----------------------------------------------------------------------
 */
void
Tcl_ConditionWait(
    Tcl_Condition *const condPtr,
    Tcl_Mutex *const mutexPtr,
    const Tcl_Time *timePtr) /* Timeout on waiting period */
{

    /* BROKEN API: double-checked locking, which is broken, but needs changes
    the Tcl C API to fix
    "correct" double-checked locking needs an import memory barrier here */
    if (*condPtr == NULL) {
	TclpGlobalLock();

	if (*condPtr == NULL) {
	    WinCondition *winCondPtr = Tcl_Alloc(sizeof *winCondPtr);
	    InitializeConditionVariable (&winCondPtr->cv);
	    *condPtr = winCondPtr;
	    TclRememberCondition (condPtr);
	}
	TclpGlobalUnlock();
    }
    /* mutexPtr is not checkd for lazy initialization,
    but the caller is supposed to be holding it */

    if (!SleepConditionVariableCS (&(*condPtr)->cv, &(*mutexPtr)->crit,
	    !timePtr? INFINITE: timePtr->sec * 1000 + timePtr->usec / 1000)) {
	switch (GetLastError ()) {
	default:
	    TclWinFailure ("SleepConditionVariableCS");
	case ERROR_TIMEOUT:
	    /* the TCL Thread API does not report timeouts */
	    EnterCriticalSection (&(*mutexPtr)->crit);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConditionNotify --
 *
 *	This procedure is invoked to signal a condition variable.
 *
 *	The mutex does not need to be held during this call to avoid races
 *	and should not be, to minimize the number of context switches
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
    /* double checked locking, i.e., BROKEN */
    if (*condPtr == NULL) {
	TclpGlobalLock();

	if (*condPtr == NULL) {
	    WinCondition *winCondPtr;
	    winCondPtr = (WinCondition *)Tcl_Alloc(sizeof *winCondPtr);
	    InitializeConditionVariable (&winCondPtr->cv);
	    *condPtr = winCondPtr;
	}
	TclpGlobalUnlock();
    }
    WakeAllConditionVariable (&(*condPtr)->cv);
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
    WinCondition *winCondPtr = *condPtr;

    /*
     * Note - this is called long after the thread-local storage is reclaimed.
     * The per-thread condition waiting event is reclaimed earlier in a
     * per-thread exit handler, which is called before thread local storage is
     * reclaimed.
     */

    if (winCondPtr != NULL) {
	Tcl_Free(winCondPtr);
	*condPtr = NULL;
    }
}




/*
 * Additions by AOL for specialized thread memory allocator.
 */
#ifdef USE_THREAD_ALLOC

static DWORD tlsKey;

typedef struct {
    Tcl_Mutex	     tlock;
    CRITICAL_SECTION wlock;
} allocMutex;

Tcl_Mutex *
TclpNewAllocMutex(void)
{
    allocMutex *lockPtr;

    lockPtr = (allocMutex *)malloc(sizeof(allocMutex));
    if (lockPtr == NULL) {
	Tcl_Panic("could not allocate lock");
    }
    lockPtr->tlock = (Tcl_Mutex) &lockPtr->wlock;
    InitializeCriticalSection(&lockPtr->wlock);
    return &lockPtr->tlock;
}

void
TclpFreeAllocMutex(
    Tcl_Mutex *mutex)		/* The alloc mutex to free. */
{
    allocMutex *lockPtr = (allocMutex *) mutex;

    if (!lockPtr) {
	return;
    }
    DeleteCriticalSection(&lockPtr->wlock);
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
