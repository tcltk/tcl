/*
 * tclThreadStorage.c --
 *
 *	This file implements platform independent thread storage operations.
 *
 * Copyright (c) 2003-2004 by Joe Mistachkin
 * Copyright (c) 2008 by George Peter Staplin
 *
 * The primary idea is that we create one platform-specific TSD slot, and
 * use it for storing a table pointer.
 *
 * Each Tcl_ThreadDataKey has an offset into the table of TSD values.
 *
 * We don't use more than 1 platform-specific TSD slot, because there is
 * a hard limit on the number of TSD slots.
 *
 * Valid key offsets are > 0.  0 is for the initialized Tcl_ThreadDataKey.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclThreadStorage.c,v 1.4.2.9 2008/05/11 04:22:48 dgp Exp $
 */

#include "tclInt.h"
#include <signal.h>

#if defined(TCL_THREADS)

static void *tclTsdKey = NULL;
static Tcl_Mutex tclTsdMutex;
static sig_atomic_t tclTsdCounter = 0;

typedef struct TSDTable {
    sig_atomic_t allocated;
    void **table;
} TSDTable;

typedef union {
    void *ptr;
    volatile sig_atomic_t offset;
} TSDUnion;

static TSDTable *TSDTableCreate(void);
static void TSDTableDelete(TSDTable *t);
static void TSDTableGrow(TSDTable *t, sig_atomic_t atLeast);


static TSDTable *
TSDTableCreate(void) {
    TSDTable *t;
    sig_atomic_t i;

    t = TclpSysAlloc(sizeof *t, 0);
    if (NULL == t) {
	Tcl_Panic("unable to allocate TSDTable");
    }

    t->allocated = 8;
    t->table = TclpSysAlloc(sizeof (*(t->table)) * t->allocated, 0);
    if (NULL == t->table) {
	Tcl_Panic("unable to allocate TSDTable");
    }

    for (i = 0; i < t->allocated; ++i) {
	t->table[i] = NULL;
    }

    return t;
}

static void
TSDTableDelete(TSDTable *t) {
    TclpSysFree(t->table);
    TclpSysFree(t);
}

/*
 *----------------------------------------------------------------------
 *
 * TSDTableGrow --
 *
 *    This procedure makes the passed TSDTable grow to fit the atLeast value.
 *
 * Side effects: The table is enlarged.
 *
 *----------------------------------------------------------------------
 */
static void
TSDTableGrow(TSDTable *t, sig_atomic_t atLeast) {
    sig_atomic_t newAllocated = t->allocated * 2;
    void **newTablePtr;
    sig_atomic_t i;

    if (newAllocated <= atLeast) {
	newAllocated = atLeast + 10;
    }

    newTablePtr = TclpSysRealloc(t->table, sizeof (*newTablePtr) * newAllocated);

    if (NULL == newTablePtr) {
	Tcl_Panic("unable to reallocate TSDTable");
    }
    
    for (i = t->allocated; i < newAllocated; ++i) {
	newTablePtr[i] = NULL;
    }
    
    t->allocated = newAllocated;
    t->table = newTablePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadStorageKeyGet --
 *
 *    This procedure gets the value associated with the passed key.
 *
 *  Results: A pointer value associated with the Tcl_ThreadDataKey or NULL.
 *
 *----------------------------------------------------------------------
 */
void *
TclThreadStorageKeyGet(Tcl_ThreadDataKey *dataKeyPtr) {
    TSDTable *t = TclpThreadGetMasterTSD(tclTsdKey);
    void *resultPtr = NULL;
    TSDUnion *keyPtr = (TSDUnion *)dataKeyPtr;
    sig_atomic_t offset = keyPtr->offset;
     
    if (t == NULL) {
	return NULL;
    }

    if (offset && offset > 0 && offset < t->allocated) {
	resultPtr = t->table[offset];
    }

    return resultPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadStorageKeySet --
 *
 *     This procedure set an association of value with the key passed.
 *     The associated value may be retrieved with TclThreadDataKeyGet().
 * 
 *  Side effects: The thread-specific table may be created or reallocated. 
 *
 *----------------------------------------------------------------------
 */

void
TclThreadStorageKeySet(Tcl_ThreadDataKey *dataKeyPtr, void *value) {
    TSDTable *t = TclpThreadGetMasterTSD(tclTsdKey);
    TSDUnion *keyPtr = (TSDUnion *)dataKeyPtr;

    if (NULL == t) {
	t = TSDTableCreate();
	TclpThreadSetMasterTSD(tclTsdKey, t);
    }

    Tcl_MutexLock(&tclTsdMutex);

    if (0 == keyPtr->offset) {
	/* 
	 * The Tcl_ThreadDataKey hasn't been used yet.
	 */
	++tclTsdCounter;

	if (tclTsdCounter >= t->allocated) {
	    TSDTableGrow(t, tclTsdCounter);
	}

	keyPtr->offset = tclTsdCounter;

	t->table[tclTsdCounter] = value;
    } else {

	if (keyPtr->offset >= t->allocated) {
	    /*
	     * This is the first time this Tcl_ThreadDataKey has been
	     * used with the current thread.
	     */
	    TSDTableGrow(t, keyPtr->offset);
	}

	t->table[keyPtr->offset] = value;
    }

    Tcl_MutexUnlock(&tclTsdMutex);
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeThreadDataThread --
 *
 *     This procedure finalizes the data for a single thread.
 *  
 *
 *  Side effects: The TSDTable is deleted/freed.
 *----------------------------------------------------------------------
 */
void 
TclFinalizeThreadDataThread(void) {
    TSDTable *t = TclpThreadGetMasterTSD(tclTsdKey);

    if (NULL == t) {
	return;
    }

    TSDTableDelete(t);
    
    TclpThreadSetMasterTSD(tclTsdKey, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitializeThreadStorage --
 *
 *      This procedure initializes the TSD subsystem with per-platform
 *      code.  This should be called before any Tcl threads are created.
 *
 *----------------------------------------------------------------------
 */
void
TclInitThreadStorage(void) {
    tclTsdKey = TclpThreadCreateKey();
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeThreadStorage --
 *
 *      This procedure cleans up the thread storage data key for all
 *      threads.
 *
 * IMPORTANT: All Tcl threads must be finalized before calling this!
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Releases the thread data key.
 *
 *----------------------------------------------------------------------
 */
void
TclFinalizeThreadStorage(void) {
    TclpThreadDeleteKey(tclTsdKey);
    tclTsdKey = NULL;
}

#else /* !defined(TCL_THREADS) */

/*
 * Stub functions for non-threaded builds
 */

void
TclInitThreadStorage(void)
{
}

void
TclFinalizeThreadDataThread(void)
{
}

void
TclFinalizeThreadStorage(void)
{
}

#endif /* defined(TCL_THREADS) && defined(USE_THREAD_STORAGE) */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
