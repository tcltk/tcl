/* 
 * tclWinThread.c --
 *
 *	This file implements the Windows-specific thread operations.
 *
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS:  @(#) tclWinThrd.c 1.13 98/02/18 14:00:23
 */

#include "tclWinInt.h"

#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>

/*
 * This is the master lock used to serialize access to other
 * serialization data structures.
 */

static CRITICAL_SECTION masterLock;
static int init = 0;
#define MASTER_LOCK  EnterCriticalSection(&masterLock)
#define MASTER_UNLOCK  LeaveCriticalSection(&masterLock)

/*
 * This is the master lock used to serialize initialization and finalization
 * of Tcl as a whole.
 */

static CRITICAL_SECTION initLock;

/*
 * This is a preallocated lock for use by memory allocators.
 */

static CRITICAL_SECTION allocLock;
static Tcl_Mutex allocMutex;


/*
 *----------------------------------------------------------------------
 *
 * TclpThreadCreate --
 *
 *	This procedure creates a new thread.
 *
 * Results:
 *	TCL_OK if the thread could be created.  The thread ID is
 *	returned in a parameter.
 *
 * Side effects:
 *	A new thread is created.
 *
 *----------------------------------------------------------------------
 */

int
TclpThreadCreate(idPtr, proc, clientData)
    Tcl_ThreadId *idPtr;		/* Return, the ID of the thread */
    Tcl_ThreadCreateProc proc;		/* Main() function of the thread */
    ClientData clientData;		/* The one argument to Main() */
{
    HANDLE tHandle;

    tHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) proc,
		(DWORD *)clientData, 0, (DWORD *)idPtr);
    if (tHandle == NULL) {
	return TCL_ERROR;
    } else {
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
TclpThreadExit(status)
    int status;
{
    ExitThread((DWORD)status);
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
Tcl_GetCurrentThread()
{
    return (Tcl_ThreadId)GetCurrentThreadId();
}


/*
 *----------------------------------------------------------------------
 *
 * TclpInitLock
 *
 *	This procedure is used to grab a lock that serializes initialization
 *	and finalization of Tcl.  On some platforms this may also initialize
 *	the mutex used to serialize creation of more mutexes and thread
 *	local storage keys.
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
TclpInitLock()
{
    if (!init) {
	/*
	 * There is a fundamental race here that is solved by creating
	 * the first Tcl interpreter in a single threaded environment.
	 * Once the interpreter has been created, it is safe to create
	 * more threads that create interpreters in parallel.
	 */
	init = 1;
	InitializeCriticalSection(&initLock);
	InitializeCriticalSection(&masterLock);
    }
    EnterCriticalSection(&initLock);
}


/*
 *----------------------------------------------------------------------
 *
 * TclpInitUnlock
 *
 *	This procedure is used to release a lock that serializes initialization
 *	and finalization of Tcl.
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
TclpInitUnlock()
{
    LeaveCriticalSection(&initLock);
}


/*
 *----------------------------------------------------------------------
 *
 * TclpMasterLock
 *
 *	This procedure is used to grab a lock that serializes creation
 *	of mutexes, condition variables, and thread local storage keys.
 *
 *	This lock must be different than the initLock because the
 *	initLock is held during creation of syncronization objects.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Acquire the master mutex.
 *
 *----------------------------------------------------------------------
 */

void
TclpMasterLock()
{
    if (!init) {
	/*
	 * There is a fundamental race here that is solved by creating
	 * the first Tcl interpreter in a single threaded environment.
	 * Once the interpreter has been created, it is safe to create
	 * more threads that create interpreters in parallel.
	 */
	init = 1;
	InitializeCriticalSection(&initLock);
	InitializeCriticalSection(&masterLock);
    }
    EnterCriticalSection(&masterLock);
}


/*
 *----------------------------------------------------------------------
 *
 * TclpMasterUnlock
 *
 *	This procedure is used to release a lock that serializes creation
 *	and deletion of synchronization objects.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Release the master mutex.
 *
 *----------------------------------------------------------------------
 */

void
TclpMasterUnlock()
{
    LeaveCriticalSection(&masterLock);
}

#ifdef TCL_THREADS

/*
 *----------------------------------------------------------------------
 *
 * TclpMutexInit --
 * TclpMutexLock --
 * TclpMutexUnlock --
 *
 *	These procedures use an explicitly initialized mutex.
 *	These are used by memory allocators for their own mutex.
 * 
 * Results:
 *	None.
 *
 * Side effects:
 *	Initialize, Lock, and Unlock the mutex.
 *
 *----------------------------------------------------------------------
 */

void
TclpMutexInit(mPtr)
    TclpMutex *mPtr;
{
    InitializeCriticalSection((CRITICAL_SECTION *)mPtr);
}
void
TclpMutexLock(mPtr)
    TclpMutex *mPtr;
{
    EnterCriticalSection((CRITICAL_SECTION *)mPtr);
}
void
TclpMutexUnlock(mPtr)
    TclpMutex *mPtr;
{
    LeaveCriticalSection((CRITICAL_SECTION *)mPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_MutexLock --
 *
 *	This procedure is invoked to lock a mutex.  This is a self 
 *	initializing mutex that is automatically finalized during
 *	Tcl_Finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May block the current thread.  The mutex is aquired when
 *	this returns.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_MutexLock(mutexPtr)
    Tcl_Mutex *mutexPtr;	/* The lock */
{
    CRITICAL_SECTION *csPtr;
    if (*mutexPtr == NULL) {
	MASTER_LOCK;

	/* 
	 * Double inside master lock check to avoid a race.
	 */

	if (*mutexPtr == NULL) {
	    csPtr = (CRITICAL_SECTION *)ckalloc(sizeof(CRITICAL_SECTION));
	    InitializeCriticalSection(csPtr);
	    *mutexPtr = (Tcl_Mutex)csPtr;
	    TclRememberMutex(mutexPtr);
	}
	MASTER_UNLOCK;
    }
    csPtr = *((CRITICAL_SECTION **)mutexPtr);
    EnterCriticalSection(csPtr);
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
Tcl_MutexUnlock(mutexPtr)
    Tcl_Mutex *mutexPtr;	/* The lock */
{
    CRITICAL_SECTION *csPtr = *((CRITICAL_SECTION **)mutexPtr);
    LeaveCriticalSection(csPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * TclpFinalizeMutex --
 *
 *	This procedure is invoked to clean up one mutex.  This is only
 *	safe to call at the end of time.
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
TclpFinalizeMutex(mutexPtr)
    Tcl_Mutex *mutexPtr;
{
    CRITICAL_SECTION *csPtr = *(CRITICAL_SECTION **)mutexPtr;
    if (csPtr != NULL) {
	ckfree((char *)csPtr);
	*mutexPtr = NULL;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * TclpThreadDataKeyInit --
 *
 *	This procedure initializes a thread specific data block key.
 *	Each thread has table of pointers to thread specific data.
 *	all threads agree on which table entry is used by each module.
 *	this is remembered in a "data key", that is just an index into
 *	this table.  To allow self initialization, the interface
 *	passes a pointer to this key and the first thread to use
 *	the key fills in the pointer to the key.  The key should be
 *	a process-wide static.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Will allocate memory the first time this process calls for
 *	this key.  In this case it modifies its argument
 *	to hold the pointer to information about the key.
 *
 *----------------------------------------------------------------------
 */

void
TclpThreadDataKeyInit(keyPtr)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk,
				 * really (pthread_key_t **) */
{
    DWORD *indexPtr;

    MASTER_LOCK;
    if (*keyPtr == NULL) {
	indexPtr = (DWORD *)ckalloc(sizeof(DWORD));
	*indexPtr = TlsAlloc();
	*keyPtr = (Tcl_ThreadDataKey)indexPtr;
	TclRememberDataKey(keyPtr);
    }
    MASTER_UNLOCK;
}


/*
 *----------------------------------------------------------------------
 *
 * TclpThreadDataKeyGet --
 *
 *	This procedure returns a pointer to a block of thread local storage.
 *
 * Results:
 *	A thread-specific pointer to the data structure, or NULL
 *	if the memory has not been assigned to this key for this thread.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

VOID *
TclpThreadDataKeyGet(keyPtr)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk,
				 * really (DWORD **) */
{
    DWORD *indexPtr = *(DWORD **)keyPtr;
    if (indexPtr == NULL) {
	return NULL;
    } else {
	return (VOID *) TlsGetValue(*indexPtr);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * TclpThreadDataKeySet --
 *
 *	This procedure sets the pointer to a block of thread local storage.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets up the thread so future calls to TclpThreadDataKeyGet with
 *	this key will return the data pointer.
 *
 *----------------------------------------------------------------------
 */

void
TclpThreadDataKeySet(keyPtr, data)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk,
				 * really (pthread_key_t **) */
    VOID *data;			/* Thread local storage */
{
    DWORD *indexPtr = *(DWORD **)keyPtr;
    TlsSetValue(*indexPtr, (void *)data);
}


/*
 *----------------------------------------------------------------------
 *
 * TclpFinalizeThreadData --
 *
 *	This procedure cleans up the thread-local storage.  This is
 *	called once for each thread.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees up the memory.
 *
 *----------------------------------------------------------------------
 */

void
TclpFinalizeThreadData(keyPtr)
    Tcl_ThreadDataKey *keyPtr;
{
    VOID *result;
    DWORD *indexPtr;

    if (*keyPtr != NULL) {
	indexPtr = *(DWORD **)keyPtr;
	result = (VOID *)TlsGetValue(*indexPtr);
	if (result != NULL) {
	    ckfree((char *)result);
	    TlsSetValue(*indexPtr, (void *)NULL);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpFinalizeThreadDataKey --
 *
 *	This procedure is invoked to clean up one key.  This is a
 *	process-wide storage identifier.  The thread finalization code
 *	cleans up the thread local storage itself.
 *
 *	This assumes the master lock is held.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The key is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TclpFinalizeThreadDataKey(keyPtr)
    Tcl_ThreadDataKey *keyPtr;
{
    DWORD *indexPtr;
    if (*keyPtr != NULL) {
	indexPtr = *(DWORD **)keyPtr;
	TlsFree(*indexPtr);
	ckfree((char *)indexPtr);
	*keyPtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpConditionWait --
 *
 *	This procedure is invoked to wait on a condition variable.
 *	The mutex is automically released as part of the wait, and
 *	automatically grabbed when the condition is signaled.
 *
 *	The mutex must be held when this procedure is called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May block the current thread.  The mutex is aquired when
 *	this returns.  Will allocate memory for a HANDLE
 *	and initialize this the first time this Tcl_Condition is used.
 *
 *----------------------------------------------------------------------
 */

void
TclpConditionWait(condPtr, mutexPtr, timePtr)
    Tcl_Condition *condPtr;	/* Really (pthread_cond_t **) */
    Tcl_Mutex *mutexPtr;	/* Really (pthread_mutex_t **) */
    Tcl_Time *timePtr;		/* Timeout on waiting period */
{
    HANDLE *eventPtr;
    CRITICAL_SECTION *csPtr;
    DWORD wtime;

    if (*condPtr == NULL) {
	MASTER_LOCK;

	/* 
	 * Double check inside mutex to avoid race,
	 * then initialize condition variable if necessary.
	 */

	if (*condPtr == NULL) {
	    eventPtr = (HANDLE *)ckalloc(sizeof(HANDLE));
	    *eventPtr = CreateEvent(NULL, TRUE /* manual reset */,
			FALSE /* non signaled */, NULL);
	    *condPtr = (Tcl_Condition)eventPtr;
	    TclRememberCondition(condPtr);
	}
	MASTER_UNLOCK;
    }
    csPtr = *((CRITICAL_SECTION **)mutexPtr);
    eventPtr = *((HANDLE **)condPtr);
    if (timePtr == NULL) {
	wtime = INFINITE;
    } else {
	wtime = timePtr->sec * 1000 + timePtr->usec / 1000;
    }

    /*
     * Clear the event it case there are old notifies.
     */

    ResetEvent(*eventPtr);
    LeaveCriticalSection(csPtr);

    /*
     * This point is a race with a notification, but this is handled
     * by the "stickiness" of the event.  If a notification occurs here,
     * then WaitForSingleObject will not block.
     */

    WaitForSingleObject(*eventPtr, wtime);

    /*
     * This point is a race with other waiters.  Someone else can grab
     * the mutex first.  This is why our caller must check its invariant
     * and perhaps wait again.
     */

    EnterCriticalSection(csPtr);

    /*
     * "Consume" the event - hmm - this may not be necessary because it
     * will be done before the next wait.
     */

    ResetEvent(*eventPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * TclpConditionNotify --
 *
 *	This procedure is invoked to signal a condition variable.
 *
 *	The mutex must be held during this call to avoid races,
 *	but this interface does not enforce that.
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
TclpConditionNotify(condPtr)
    Tcl_Condition *condPtr;
{
    HANDLE *eventPtr;
    if (condPtr != NULL) {
	eventPtr = *((HANDLE **)condPtr);

	/*
	 * The PulseEvent may not be necessary, but it's documentation says
	 * it releases all waiting processes, which is what we want.  However,
	 * it also clears the signal, which is not good because of the race
	 * in ConditionWait.  The SetEvent makes sure the signal remains
	 * even if there are no waiters, but we are not sure that it really
	 * marks all waiters as runnable.  So we do both.
	 */

	PulseEvent(*eventPtr);
	SetEvent(*eventPtr);
    } else {
	/*
	 * Noone has used the condition variable, so there are no waiters.
	 */
    }
}


/*
 *----------------------------------------------------------------------
 *
 * TclpFinalizeCondition --
 *
 *	This procedure is invoked to clean up a condition variable.
 *	This is only safe to call at the end of time.
 *
 *	This assumes the Master Lock is held.
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
TclpFinalizeCondition(condPtr)
    Tcl_Condition *condPtr;
{
    HANDLE *eventPtr = *(HANDLE **)condPtr;
    if (eventPtr != NULL) {
	CloseHandle(*eventPtr);
	ckfree((char *)eventPtr);
	*condPtr = NULL;
    }
}
#endif TCL_THREADS


