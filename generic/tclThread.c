/* 
 * tclThread.c --
 *
 *	This file implements   Platform independent thread operations.
 *	Most of the real work is done in the platform dependent files.
 *
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclThread.c 1.14 98/02/20 11:24:32
 */

#include "tclInt.h"

/*
 * There are three classes of synchronization objects:
 * mutexes, thread data keys, and condition variables.
 * The following are used to record the memory used for these
 * objects so they can be finalized.
 *
 * These statics are guarded by the mutex in the caller of
 * TclRememberThreadData, e.g., TclpThreadDataKeyInit
 */

typedef struct {
    int num;		/* Number of objects remembered */
    int max;		/* Max size of the array */
    char **list;	/* List of pointers */
} SyncObjRecord;

static SyncObjRecord keyRecord;
static SyncObjRecord mutexRecord;
static SyncObjRecord condRecord;
#ifdef notdef
static int numKeys = 0;
static int maxKeys = 0;
static Tcl_ThreadDataKey *keyList[] = NULL;

static int numMutexes = 0;
static int maxMutexes = 0;
static Tcl_Mutex *mutexList[] = NULL;

static int numConds = 0;
static int maxConds = 0;
static Tcl_Condition *condList[] = NULL;
#endif

/*
 * Prototypes of functions used only in this file
 */
 
void TclRememberSyncObject(char *objPtr, SyncObjRecord *recPtr);
void TclForgetSyncObject(char *objPtr, SyncObjRecord *recPtr);


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConditionWait --
 * Tcl_ConditionNotify --
 *	Temporary wrappers.
 *
 *----------------------------------------------------------------------
 */
#ifdef TCL_THREADS
void
Tcl_ConditionNotify(condPtr)
    Tcl_Condition *condPtr;
{
    TclpConditionNotify(condPtr);
}
void
Tcl_ConditionWait(condPtr, mutexPtr, timePtr)
    Tcl_Condition *condPtr;
    Tcl_Mutex *mutexPtr;
    Tcl_Time *timePtr;		/* Timeout on waiting period */
{
    TclpConditionWait(condPtr, mutexPtr, timePtr);
}
#endif /* TCL_THREADS */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetThreadData --
 *
 *	This procedure allocates and initializes a chunk of thread
 *	local storage.
 *
 * Results:
 *	A thread-specific pointer to the data structure.
 *
 * Side effects:
 *	Will allocate memory the first time this thread calls for
 *	this chunk of storage.
 *
 *----------------------------------------------------------------------
 */

VOID *
Tcl_GetThreadData(keyPtr, size)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk */
    int size;			/* Size of storage block */
{
    VOID *result;
#ifdef TCL_THREADS

    /*
     * See if this is the first thread to init this key.
     */

    if (*keyPtr == NULL) {
	TclpThreadDataKeyInit(keyPtr);
    }

    /*
     * Initialize the key for this thread.
     */

    result = TclpThreadDataKeyGet(keyPtr);
    if (result == NULL) {
	result  = (VOID *)ckalloc(size);
	memset(result, 0, size);
	TclpThreadDataKeySet(keyPtr, result);
    }
#else
    if (*keyPtr == NULL) {
	result = (VOID *)ckalloc(size);
	memset((char *)result, 0, size);
	*keyPtr = (Tcl_ThreadDataKey)result;
	TclRememberDataKey(keyPtr);
    }
    result = *(VOID **)keyPtr;
#endif
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadDataKeyGet --
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
TclThreadDataKeyGet(keyPtr)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk,
				 * really (pthread_key_t **) */
{
#ifdef TCL_THREADS
    return (VOID *)TclpThreadDataKeyGet(keyPtr);
#else
    char *result = *(char **)keyPtr;
    return (VOID *)result;
#endif /* TCL_THREADS */
}


/*
 *----------------------------------------------------------------------
 *
 * TclThreadDataKeySet --
 *
 *	This procedure sets a thread local storage pointer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The assigned value will be returned by Tcl_ThreadDataKeyGet.
 *
 *----------------------------------------------------------------------
 */

void
TclThreadDataKeySet(keyPtr, data)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk,
				 * really (pthread_key_t **) */
    VOID *data;			/* Thread local storage */
{
#ifdef TCL_THREADS
    if (*keyPtr == NULL) {
	TclpThreadDataKeyInit(keyPtr);
    }
    TclpThreadDataKeySet(keyPtr, data);
#else
    *keyPtr = (Tcl_ThreadDataKey)data;
#endif /* TCL_THREADS */
}



/*
 *----------------------------------------------------------------------
 *
 * TclRememberSyncObject
 *
 *      Keep a list of (mutexes/condition variable/data key)
 *	used during finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Add to the appropriate list.
 *
 *----------------------------------------------------------------------
 */

void
TclRememberSyncObject(objPtr, recPtr)
    char *objPtr;		/* Pointer to sync object */
    SyncObjRecord *recPtr;	/* Record of sync objects */
{
    char **newList;
    int i;

    /*
     * Save the pointer to the allocated object so it can be finalized.
     * Grow the list of pointers if necessary.
     */

    if (recPtr->num >= recPtr->max) {
	recPtr->max += 8;
	newList = (char **)ckalloc(recPtr->max * sizeof(char *));
	for (i=0 ; i<recPtr->num ; i++) {
	    newList[i] = recPtr->list[i];
	}
	if (recPtr->list != NULL) {
	    ckfree((char *)recPtr->list);
	}
	recPtr->list = newList;
    }
    recPtr->list[recPtr->num] = objPtr;
    recPtr->num++;
}

/*
 *----------------------------------------------------------------------
 *
 * TclForgetSyncObject
 *
 *      Remove a single object from the list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Remove from the appropriate list.
 *
 *----------------------------------------------------------------------
 */

void
TclForgetSyncObject(objPtr, recPtr)
    char *objPtr;		/* Pointer to sync object */
    SyncObjRecord *recPtr;	/* Record of sync objects */
{
    int i;

    for (i=0 ; i<recPtr->num ; i++) {
	if (objPtr == recPtr->list[i]) {
	    recPtr->list[i] = NULL;
	    return;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclRememberMutex
 *
 *      Keep a list of mutexes used during finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Add to the mutex list.
 *
 *----------------------------------------------------------------------
 */

void
TclRememberMutex(mutexPtr)
    Tcl_Mutex *mutexPtr;
{
    TclRememberSyncObject((char *)mutexPtr, &mutexRecord);
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeMutex
 *
 *      Finalize a single mutex and remove it from the
 *	list of remembered objects.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Remove the mutex from the list.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeMutex(mutexPtr)
    Tcl_Mutex *mutexPtr;
{
#ifdef TCL_THREADS
    TclpFinalizeMutex(mutexPtr);
#endif
    TclForgetSyncObject((char *)mutexPtr, &mutexRecord);
}

/*
 *----------------------------------------------------------------------
 *
 * TclRememberDataKey
 *
 *      Keep a list of thread data keys used during finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Add to the key list.
 *
 *----------------------------------------------------------------------
 */

void
TclRememberDataKey(keyPtr)
    Tcl_ThreadDataKey *keyPtr;
{
    TclRememberSyncObject((char *)keyPtr, &keyRecord);
}

/*
 *----------------------------------------------------------------------
 *
 * TclRememberCondition
 *
 *      Keep a list of condition variables used during finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Add to the condition variable list.
 *
 *----------------------------------------------------------------------
 */

void
TclRememberCondition(condPtr)
    Tcl_Condition *condPtr;
{
    TclRememberSyncObject((char *)condPtr, &condRecord);
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeCondition
 *
 *      Finalize a single condition variable and remove it from the
 *	list of remembered objects.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Remove the condition variable from the list.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeCondition(condPtr)
    Tcl_Condition *condPtr;
{
#ifdef TCL_THREADS
    TclpFinalizeCondition(condPtr);
#endif
    TclForgetSyncObject((char *)condPtr, &condRecord);
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeThreadData --
 *
 *	This procedure cleans up the thread-local storage.  This is
 *	called once for each thread.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees up all thread local storage.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeThreadData()
{
    int i;
    Tcl_ThreadDataKey *keyPtr;

    TclpMasterLock();
    for (i=0 ; i<keyRecord.num ; i++) {
	keyPtr = (Tcl_ThreadDataKey *) keyRecord.list[i];
#ifdef TCL_THREADS
	TclpFinalizeThreadData(keyPtr);
#else
	if (*keyPtr != NULL) {
	    ckfree((char *)*keyPtr);
	    *keyPtr = NULL;
	}
#endif
    }
    TclpMasterUnlock();
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeSyncronization --
 *
 *      This procedure cleans up all synchronization objects:
 *      mutexes, condition variables, and thread-local storage.
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
TclFinalizeSynchronization()
{
#ifdef TCL_THREADS
    Tcl_ThreadDataKey *keyPtr;
    Tcl_Mutex *mutexPtr;
    Tcl_Condition *condPtr;
    int i;

    TclpMasterLock();
    for (i=0 ; i<keyRecord.num ; i++) {
	keyPtr = (Tcl_ThreadDataKey *)keyRecord.list[i];
	TclpFinalizeThreadDataKey(keyPtr);
    }
    if (keyRecord.list != NULL) {
	ckfree((char *)keyRecord.list);
	keyRecord.list = NULL;
    }
    keyRecord.max = 0;
    keyRecord.num = 0;

    for (i=0 ; i<mutexRecord.num ; i++) {
	mutexPtr = (Tcl_Mutex *)mutexRecord.list[i];
	if (mutexPtr != NULL) {
	    TclpFinalizeMutex(mutexPtr);
	}
    }
    if (mutexRecord.list != NULL) {
	ckfree((char *)mutexRecord.list);
	mutexRecord.list = NULL;
    }
    mutexRecord.max = 0;
    mutexRecord.num = 0;

    for (i=0 ; i<condRecord.num ; i++) {
	condPtr = (Tcl_Condition *)condRecord.list[i];
	if (condPtr != NULL) {
	    TclpFinalizeCondition(condPtr);
	}
    }
    if (condRecord.list != NULL) {
	ckfree((char *)condRecord.list);
	condRecord.list = NULL;
    }
    condRecord.max = 0;
    condRecord.num = 0;

    TclpMasterUnlock();
#else
    if (keyRecord.list != NULL) {
	ckfree((char *)keyRecord.list);
	keyRecord.list = NULL;
    }
    keyRecord.max = 0;
    keyRecord.num = 0;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ExitThread --
 *
 *	This procedure is called to terminate the current thread.
 *	This should be used by extensions that create threads with
 *	additional interpreters in them.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All thread exit handlers are invoked, then the thread dies.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ExitThread(status)
    int status;
{
    Tcl_FinalizeThread();
#ifdef TCL_THREADS
    TclpThreadExit(status);
#endif
}


