/*
 * tclThreadStorage.c --
 *
 *	This file implements platform independent thread storage operations.
 *
 * Copyright (c) 2003-2004 by Joe Mistachkin
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclThreadStorage.c,v 1.4 2004/06/24 09:05:46 dkf Exp $
 */

#include "tclInt.h"

#if defined(TCL_THREADS) && defined(USE_THREAD_STORAGE)

/*
 * This is the thread storage cache array and it's accompanying mutex.
 * The elements are pairs of thread Id and an associated hash table
 * pointer; the hash table being pointed to contains the thread storage
 * for it's associated thread. The purpose of this cache is to minimize
 * the number of hash table lookups in the master thread storage hash
 * table.
 */

static Tcl_Mutex threadStorageLock;

/*
 * This is the struct used for a thread storage cache slot. It contains
 * the owning thread Id and the associated hash table pointer.
 */

typedef struct ThreadStorage {
    Tcl_ThreadId id;			/* the owning thread id */
    Tcl_HashTable *hashTablePtr;	/* the hash table for the thread */
} ThreadStorage;

/*
 * These are the prototypes for the custom hash table allocation
 * functions used by the thread storage subsystem.
 */

static Tcl_HashEntry *	AllocThreadStorageEntry _ANSI_ARGS_((
			    Tcl_HashTable *tablePtr, void *keyPtr));
static void		FreeThreadStorageEntry _ANSI_ARGS_((
			    Tcl_HashEntry *hPtr));

/*
 * This is the hash key type for thread storage. We MUST use this in
 * combination with the new hash key type flag TCL_HASH_KEY_SYSTEM_HASH
 * because these hash tables MAY be used by the threaded memory allocator.
 */
Tcl_HashKeyType tclThreadStorageHashKeyType = {
    TCL_HASH_KEY_TYPE_VERSION,		/* version */
    TCL_HASH_KEY_SYSTEM_HASH,		/* flags */
    NULL,				/* hashKeyProc */
    NULL,				/* compareKeysProc */
    AllocThreadStorageEntry,		/* allocEntryProc */
    FreeThreadStorageEntry		/* freeEntryProc */
};

/*
 * This is an invalid thread value.
 */

#define STORAGE_INVALID_THREAD  (Tcl_ThreadId)0

/*
 * This is the value for an invalid thread storage key.
 */

#define STORAGE_INVALID_KEY	0

/*
 * This is the first valid key for use by external callers.
 * All the values below this are RESERVED for future use.
 */

#define STORAGE_FIRST_KEY	101

/*
 * This is the default number of thread storage cache slots.
 * This define may need to be fine tuned for maximum performance.
 */

#define STORAGE_CACHE_SLOTS	97

/*
 * This is the master thread storage hash table. It is keyed on
 * thread Id and contains values that are hash tables for each thread.
 * The thread specific hash tables contain the actual thread storage.
 */

static Tcl_HashTable *threadStorageHashTablePtr = NULL;

/*
 * This is the next thread data key value to use. We increment this
 * everytime we "allocate" one. It is initially set to 1 in
 * TclThreadStorageInit.
 */

static int nextThreadStorageKey = STORAGE_INVALID_KEY;

/*
 * Have we initialized the thread storage mutex yet?
 */

static int initThreadStorage = 0;

/*
 * This is the master thread storage cache. Per kennykb's idea, this
 * prevents unnecessary lookups for threads that use a lot of thread
 * storage.
 */

static volatile ThreadStorage threadStorageCache[STORAGE_CACHE_SLOTS];

/*
 *----------------------------------------------------------------------
 *
 * TclThreadStorageLockInit
 *
 *	This procedure is used to initialize the lock that serializes
 *	creation of thread storage.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The master lock is acquired and possibly initialized for the
 *	first time.
 *
 *----------------------------------------------------------------------
 */

void
TclThreadStorageLockInit()
{
    if (!initThreadStorage) {
	/*
	 * Mutexes in Tcl are self initializing, and we are taking
	 * advantage of that fact since this file cannot contain
	 * platform specific calls.
	 */
	initThreadStorage = 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadStorageLock
 *
 *	This procedure is used to grab a lock that serializes creation
 *	of thread storage.
 *
 *	This lock must be different than the initLock because the
 *	initLock is held during creation of syncronization objects.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Acquire the thread storage mutex.
 *
 *----------------------------------------------------------------------
 */

void
TclThreadStorageLock()
{
    TclThreadStorageLockInit();
    Tcl_MutexLock(&threadStorageLock);
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadStorageUnlock
 *
 *	This procedure is used to release a lock that serializes creation
 *	of thread storage.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Release the thread storage mutex.
 *
 *----------------------------------------------------------------------
 */

void
TclThreadStorageUnlock()
{
    Tcl_MutexUnlock(&threadStorageLock);
}

/*
 *----------------------------------------------------------------------
 *
 * AllocThreadStorageEntry --
 *
 *	Allocate space for a Tcl_HashEntry using TclpSysAlloc (not
 *	ckalloc). We do this because the threaded memory allocator MAY
 *	use the thread storage hash tables.
 *
 * Results:
 *	The return value is a pointer to the created entry.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_HashEntry *
AllocThreadStorageEntry(tablePtr, keyPtr)
    Tcl_HashTable *tablePtr;	/* Hash table. */
    void *keyPtr;		/* Key to store in the hash table entry. */
{
    Tcl_HashEntry *hPtr;

    hPtr = (Tcl_HashEntry *)TclpSysAlloc(sizeof(Tcl_HashEntry), 0);
    hPtr->key.oneWordValue = (char *)keyPtr;

    return hPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeThreadStorageEntry --
 *
 *	Frees space for a Tcl_HashEntry using TclpSysFree (not ckfree).
 *	We do this because the threaded memory allocator MAY use the
 *	thread storage hash tables.
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
FreeThreadStorageEntry(hPtr)
    Tcl_HashEntry *hPtr;	/* Hash entry to free. */
{
    TclpSysFree((char *)hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 *  TclThreadStoragePrint --
 *
 *	This procedure prints out the contents of the master thread
 *	storage hash table, the thread storage cache, and the next key
 *	value to the specified file.
 *
 *	This assumes that thread storage lock is held.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The thread storage lock is acquired and released.
 *
 *----------------------------------------------------------------------
 */

void
TclThreadStoragePrint(outFile, flags)
    FILE *outFile;		/* The file to print the information to. */
    int flags;			/* Reserved for future use. */
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int header, index;

    if (threadStorageHashTablePtr != NULL) {
	hPtr = Tcl_FirstHashEntry(threadStorageHashTablePtr, &search);

	if (hPtr != NULL) {
	    fprintf(outFile, "master thread storage hash table:\n");
	    for (; hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
		fprintf(outFile,
			"master entry ptr %p, thread %p, thread table ptr %p\n",
			hPtr, Tcl_GetHashKey(threadStorageHashTablePtr, hPtr),
			Tcl_GetHashValue(hPtr));
	    }
	} else {
	    fprintf(outFile,
		    "master thread storage hash table has no entries\n");
	}
    } else {
	fprintf(outFile,
		"master thread storage hash table not initialized\n");
    }

    header = 0; /* we have not output the header yet. */
    for (index = 0; index < STORAGE_CACHE_SLOTS; index++) {
	if (threadStorageCache[index].id != STORAGE_INVALID_THREAD) {
	    if (!header) {
		fprintf(outFile, "thread storage cache (%d total slots):\n",
			STORAGE_CACHE_SLOTS);
		header = 1;
	    }

	    fprintf(outFile, "slot %d, thread %p, thread table ptr %p\n",
		    index, threadStorageCache[index].id,
		    threadStorageCache[index].hashTablePtr);
#ifdef VERBOSE_THREAD_STORAGE_DEBUGGING
	    /*
	     * Currently not enabled by default due to Tcl_HashStats
	     * use of ckalloc and ckfree.  Please note that this can
	     * produce a LOT of output.
	     */
	    if (threadStorageCache[index].hashTablePtr != NULL) {
		CONST char *stats =
			Tcl_HashStats(threadStorageCache[index].hashTablePtr);
		if (stats != NULL) {
		    fprintf(outFile, "%s\n", stats);
		    ckfree((void *)stats);
		} else {
		    fprintf(outFile,
			    "could not get table statistics for slot %d\n",
			    index);
		}
	    }
#endif
	} else {
	    /* fprintf(outFile, "cache slot %d not used\n", index); */
	}
    }

    if (!header) {
	fprintf(outFile, "thread storage cache is empty (%d total slots)\n",
		STORAGE_CACHE_SLOTS);
	header = 1;
    }

    /*
     * Show the next data key value.
     */

    fprintf(outFile, "next data key value is: %d\n", nextThreadStorageKey);
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadStorageGetHashTable --
 *
 *	This procedure returns a hash table pointer to be used for thread
 *	storage for the specified thread.
 *
 *	This assumes that thread storage lock is held.
 *
 * Results:
 *	A hash table pointer for the specified thread, or NULL
 *	if the hash table has not been created yet.
 *
 * Side effects:
 *	May change an entry in the master thread storage cache to point
 *	to the specified thread and it's associated hash table.
 *
 *----------------------------------------------------------------------
 */

Tcl_HashTable *
TclThreadStorageGetHashTable(id)
    Tcl_ThreadId id;		/* Id of thread to get hash table for */
{
    int index = (unsigned int)id % STORAGE_CACHE_SLOTS;
    Tcl_HashEntry *hPtr;
    int new;

    /*
     * It's important that we pick up the hash table pointer BEFORE
     * comparing thread Id in case another thread is in the critical
     * region changing things out from under you.
     */

    Tcl_HashTable *hashTablePtr = threadStorageCache[index].hashTablePtr;

    if (threadStorageCache[index].id != id) {
	TclThreadStorageLock();

	/*
	 * Make sure the master hash table is initialized.
	 */

	TclThreadStorageInit(STORAGE_INVALID_THREAD, NULL);

	if (threadStorageHashTablePtr != NULL) {
	    /*
	     * It's not in the cache, so we look it up...
	     */

	    hPtr = Tcl_FindHashEntry(threadStorageHashTablePtr, (char *)id);

	    if (hPtr != NULL) {
		/*
		 * We found it, extract the hash table pointer.
		 */
		hashTablePtr = Tcl_GetHashValue(hPtr);
	    } else {
		/*
		 * The thread specific hash table is not found.
		 */
		hashTablePtr = NULL;
	    }

	    if (hashTablePtr == NULL) {
		hashTablePtr = (Tcl_HashTable *)
			TclpSysAlloc(sizeof(Tcl_HashTable), 0);

		if (hashTablePtr == NULL) {
		    Tcl_Panic("could not allocate thread specific hash "
			    "table, TclpSysAlloc failed from "
			    "TclThreadStorageGetHashTable!");
		}
		Tcl_InitCustomHashTable(hashTablePtr, TCL_CUSTOM_TYPE_KEYS,
			&tclThreadStorageHashKeyType);

		/*
		 * Add new thread storage hash table to the master
		 * hash table.
		 */

		hPtr = Tcl_CreateHashEntry(threadStorageHashTablePtr,
			(char *)id, &new);

		if (hPtr == NULL) {
		    Tcl_Panic("Tcl_CreateHashEntry failed from "
			    "TclThreadStorageInit!");
		}
		Tcl_SetHashValue(hPtr, hashTablePtr);
	    }

	    /*
	     * Now, we put it in the cache since it is highly likely
	     * it will be needed again shortly.
	     */

	    threadStorageCache[index].id = id;
	    threadStorageCache[index].hashTablePtr = hashTablePtr;
	} else {
	    /*
	     * We cannot look it up, the master hash table has not
	     * been initialized.
	     */
	    hashTablePtr = NULL;
	}
	TclThreadStorageUnlock();
    }

    return hashTablePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadStorageInit --
 *
 *	This procedure initializes a thread specific hash table for the
 *	current thread. It may also initialize the master hash table which
 *	stores all the thread specific hash tables.
 *
 *	This assumes that thread storage lock is held.
 *
 * Results:
 *	A hash table pointer for the specified thread, or NULL if we are
 *	be called to initialize the master hash table only.
 *
 * Side effects:
 *	The thread specific hash table may be initialized and added to the
 *	master hash table.
 *
 *----------------------------------------------------------------------
 */

Tcl_HashTable *
TclThreadStorageInit(id, reserved)
    Tcl_ThreadId id;		/* Id of thread to get hash table for */
    void *reserved;		/* reserved for future use */
{
#if 0 /* #ifdef TCL_THREAD_STORAGE_DEBUG */
    TclThreadStoragePrint(stderr, 0);
#endif

    if (threadStorageHashTablePtr == NULL) {
	/*
	 * Looks like we haven't created the outer hash table yet we
	 * can just do that now.
	 */

	threadStorageHashTablePtr = (Tcl_HashTable *)
		TclpSysAlloc(sizeof(Tcl_HashTable), 0);
	if (threadStorageHashTablePtr == NULL) {
	    Tcl_Panic("could not allocate master thread storage hash table, "
		    "TclpSysAlloc failed from TclThreadStorageInit!");
	}
	Tcl_InitCustomHashTable(threadStorageHashTablePtr,
		TCL_CUSTOM_TYPE_KEYS, &tclThreadStorageHashKeyType);

	/*
	 * We also initialize the cache.
	 */

	memset((ThreadStorage *)&threadStorageCache, 0,
		sizeof(ThreadStorage) * STORAGE_CACHE_SLOTS);

	/*
	 * Now, we set the first value to be used for a thread data key.
	 */

	nextThreadStorageKey = STORAGE_FIRST_KEY;
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadStorageDataKeyInit --
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
TclThreadStorageDataKeyInit(keyPtr)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk,
				 * really (int **) */
{
    int *indexPtr;
    int newKey;

    if (*keyPtr == NULL) {
	indexPtr = (int *)TclpSysAlloc(sizeof(int), 0);
	if (indexPtr == NULL) {
	    Tcl_Panic("TclpSysAlloc failed from TclThreadStorageDataKeyInit!");
	}

	/*
	 * We must call this now to make sure that
	 * nextThreadStorageKey has a well defined value.
	 */

	TclThreadStorageLock();

	/*
	 * Make sure the master hash table is initialized.
	 */

	TclThreadStorageInit(STORAGE_INVALID_THREAD, NULL);

	/*
	 * These data key values are sequentially assigned and we must
	 * use the storage lock to prevent serious problems here.
	 * Also note that the caller should NOT make any assumptions
	 * about the provided values. In particular, we may need to
	 * reserve some values in the future.
	 */

	newKey = nextThreadStorageKey++;
	TclThreadStorageUnlock();

	*indexPtr = newKey;
	*keyPtr = (Tcl_ThreadDataKey)indexPtr;
	TclRememberDataKey(keyPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadStorageDataKeyGet --
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

void *
TclThreadStorageDataKeyGet(keyPtr)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk,
				 * really (int **) */
{
    int *indexPtr = *(int **)keyPtr;

    if (indexPtr == NULL) {
	return NULL;
    } else {
	Tcl_HashTable *hashTablePtr =
		TclThreadStorageGetHashTable(Tcl_GetCurrentThread());
	Tcl_HashEntry *hPtr;

	if (hashTablePtr == NULL) {
	    Tcl_Panic("TclThreadStorageGetHashTable failed from "
		    "TclThreadStorageDataKeyGet!");
	}

	hPtr = Tcl_FindHashEntry(hashTablePtr, (char *)*indexPtr);

	if (hPtr == NULL) {
	    return NULL;
	}
	return (void *)Tcl_GetHashValue(hPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclThreadStorageDataKeySet --
 *
 *	This procedure sets the pointer to a block of thread local storage.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets up the thread so future calls to TclThreadStorageDataKeyGet
 *	with this key will return the data pointer.
 *
 *----------------------------------------------------------------------
 */

void
TclThreadStorageDataKeySet(keyPtr, data)
    Tcl_ThreadDataKey *keyPtr;	/* Identifier for the data chunk,
				 * really (pthread_key_t **) */
    void *data;			/* Thread local storage */
{
    int *indexPtr = *(int **)keyPtr;
    Tcl_HashTable *hashTablePtr;
    Tcl_HashEntry *hPtr;

    hashTablePtr = TclThreadStorageGetHashTable(Tcl_GetCurrentThread());
    if (hashTablePtr == NULL) {
	Tcl_Panic("TclThreadStorageGetHashTable failed from "
		"TclThreadStorageDataKeySet!");
    }

    hPtr = Tcl_FindHashEntry(hashTablePtr, (char *)*indexPtr);

    /*
     * Does the item need to be created?
     */
    if (hPtr == NULL) {
	int new;
	hPtr = Tcl_CreateHashEntry(hashTablePtr, (char *)*indexPtr, &new);
	if (hPtr == NULL) {
	    Tcl_Panic("could not create hash entry value from "
		    "TclThreadStorageDataKeySet");
	}
    }

    Tcl_SetHashValue(hPtr, data);
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeThreadStorageThread --
 *
 *	This procedure cleans up the thread storage hash table for the
 *	specified thread.
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
TclFinalizeThreadStorageThread(id)
    Tcl_ThreadId id;		/* Id of the thread to finalize */
{
    int index = (unsigned int)id % STORAGE_CACHE_SLOTS;
    Tcl_HashTable *hashTablePtr; /* Hash table for current thread */
    Tcl_HashEntry *hPtr;	/* Hash entry for current thread in master
				 * table */

    TclThreadStorageLock();

    if (threadStorageHashTablePtr != NULL) {
	hPtr = Tcl_FindHashEntry(threadStorageHashTablePtr, (char *)id);

	if (hPtr != NULL) {
	    /*
	     * We found it, extract the hash table pointer.
	     */

	    hashTablePtr = Tcl_GetHashValue(hPtr);

	    if (hashTablePtr != NULL) {
		/*
		 * Delete thread specific hash table and free the
		 * struct.
		 */

		Tcl_DeleteHashTable(hashTablePtr);
		TclpSysFree((char *)hashTablePtr);
	    }

	    /*
	     * Delete thread specific entry from master hash table.
	     */

	    Tcl_DeleteHashEntry(hPtr);
	}
    }

    /*
     * Make sure cache entry for this thread is NULL.
     */

    if (threadStorageCache[index].id == id) {
	/*
	 * We do not step on another thread's cache entry.  This is
	 * especially important if we are creating and exiting a lot
	 * of threads.
	 */

	threadStorageCache[index].id = STORAGE_INVALID_THREAD;
	threadStorageCache[index].hashTablePtr = NULL;
    }

    TclThreadStorageUnlock();
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeThreadStorage --
 *
 *	This procedure cleans up the master thread storage hash table,
 *	all thread specific hash tables, and the thread storage cache.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The master thread storage hash table and thread storage cache are
 *	reset to their initial (empty) state.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeThreadStorage()
{
    TclThreadStorageLock();

    if (threadStorageHashTablePtr != NULL) {
	Tcl_HashSearch search;		/* We need to hit every thread with
					 * this search. */
	Tcl_HashEntry *hPtr;		/* Hash entry for current thread in
					 * master table. */

	/*
	 * We are going to delete the hash table for every thread now.
	 * This hash table should be empty at this point, except for
	 * one entry for the current thread.
	 */

	for (hPtr = Tcl_FirstHashEntry(threadStorageHashTablePtr, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    Tcl_HashTable *hashTablePtr = Tcl_GetHashValue(hPtr);

	    if (hashTablePtr != NULL) {
		/*
		 * Delete thread specific hash table for the thread in
		 * question and free the struct.
		 */

		Tcl_DeleteHashTable(hashTablePtr);
		TclpSysFree((char *)hashTablePtr);
	    }

	    /*
	     * Delete thread specific entry from master hash table.
	     */

	    Tcl_SetHashValue(hPtr, NULL);
	}

	Tcl_DeleteHashTable(threadStorageHashTablePtr);
	TclpSysFree((char *)threadStorageHashTablePtr);

	/*
	 * Reset this so that next time around we know it's not valid.
	 */

	threadStorageHashTablePtr = NULL;
    }

    /*
     * Clear out the thread storage cache as well.
     */

    memset((ThreadStorage *)&threadStorageCache, 0,
	    sizeof(ThreadStorage) * STORAGE_CACHE_SLOTS);

    /*
     * Reset this to zero, it will be set to STORAGE_FIRST_KEY if the
     * thread storage subsystem gets reinitialized
     */

    nextThreadStorageKey = STORAGE_INVALID_KEY;

    TclThreadStorageUnlock();
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeThreadStorageData --
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
TclFinalizeThreadStorageData(keyPtr)
    Tcl_ThreadDataKey *keyPtr;
{
    if (*keyPtr != NULL) {
	Tcl_ThreadId id = Tcl_GetCurrentThread();
	Tcl_HashTable *hashTablePtr;	/* Hash table for current thread */
	Tcl_HashEntry *hPtr;		/* Hash entry for data key in current
					 * thread. */
	int *indexPtr = *(int **)keyPtr;

	hashTablePtr = TclThreadStorageGetHashTable(id);
	if (hashTablePtr == NULL) {
	    Tcl_Panic("TclThreadStorageGetHashTable failed from "
		    "TclFinalizeThreadStorageData!");
	}

	hPtr = Tcl_FindHashEntry(hashTablePtr, (char *)*indexPtr);
	if (hPtr != NULL) {
	    void *result = Tcl_GetHashValue(hPtr);

	    if (result != NULL) {
		/*
		 * This must be ckfree because tclThread.c allocates
		 * these using ckalloc.
		 */
		ckfree((char *)result);
	    }

	    Tcl_SetHashValue(hPtr, NULL);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeThreadStorageDataKey --
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
TclFinalizeThreadStorageDataKey(keyPtr)
    Tcl_ThreadDataKey *keyPtr;
{
    int *indexPtr;
    Tcl_HashTable *hashTablePtr;/* Hash table for current thread */
    Tcl_HashSearch search;	/* Need to hit every thread with this search */
    Tcl_HashEntry *hPtr;	/* Hash entry for current thread in master
				 * table. */
    Tcl_HashEntry *hDataPtr;	/* Hash entry for data key in current thread */

    if (*keyPtr != NULL) {
	indexPtr = *(int **)keyPtr;

	TclThreadStorageLock();

	if (threadStorageHashTablePtr != NULL) {
	    /*
	     * We are going to delete the specified data key entry
	     * from every thread.
	     */

	    for (hPtr = Tcl_FirstHashEntry(threadStorageHashTablePtr, &search);
		    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {

		/*
		 * Get the hash table corresponding to the thread in question.
		 */
		hashTablePtr = Tcl_GetHashValue(hPtr);

		if (hashTablePtr != NULL) {
		    /*
		     * Now find the entry for the specified data key.
		     */
		    hDataPtr = Tcl_FindHashEntry(hashTablePtr,
			    (char *)*indexPtr);

		    if (hDataPtr != NULL) {
			/*
			 * Delete the data key for this thread.
			 */
			Tcl_DeleteHashEntry(hDataPtr);
		    }
		}
	    }
	}

	TclThreadStorageUnlock();

	TclpSysFree((char *)indexPtr);
	*keyPtr = NULL;
    }
}

#else /* !defined(TCL_THREADS) || !defined(USE_THREAD_STORAGE) */

static void ThreadStoragePanic _ANSI_ARGS_((CONST char *message));

/*
 *----------------------------------------------------------------------
 *
 * ThreadStoragePanic --
 *
 *      Panic if Tcl was compiled without TCL_THREADS or without
 *      USE_THREAD_STORAGE and a thread storage function has been
 *      called.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	    None.
 *
 *----------------------------------------------------------------------
 */
static void ThreadStoragePanic(message)
    CONST char *message;	/* currently ignored */
{
#ifdef TCL_THREADS
#   ifdef USE_THREAD_STORAGE
    /*
     * Do nothing, everything is OK. However, this should never happen
     * because this function only gets called by the dummy thread
     * storage functions (used when one or both of these DEFINES are
     * not present).
     */
#   else
    Tcl_Panic("Tcl was not compiled with thread storage enabled.");
#   endif /* USE_THREAD_STORAGE */
#else
    Tcl_Panic("Tcl was not compiled with threads enabled.");
#endif /* TCL_THREADS */
}

/*
 * Stub functions that just call ThreadStoragePanic.
 */

void
TclThreadStorageLockInit()
{
    ThreadStoragePanic(NULL);
}

void
TclThreadStorageLock()
{
    ThreadStoragePanic(NULL);
}

void
TclThreadStorageUnlock()
{
    ThreadStoragePanic(NULL);
}

void
TclThreadStoragePrint(outFile, flags)
    FILE *outFile;
    int flags;
{
    ThreadStoragePanic(NULL);
}

Tcl_HashTable *
TclThreadStorageGetHashTable(id)
    Tcl_ThreadId id;
{
    ThreadStoragePanic(NULL);
    return NULL;
}

Tcl_HashTable *
TclThreadStorageInit(id, reserved)
    Tcl_ThreadId id;
    void *reserved;
{
    ThreadStoragePanic(NULL);
    return NULL;
}

void
TclThreadStorageDataKeyInit(keyPtr)
    Tcl_ThreadDataKey *keyPtr;
{
    ThreadStoragePanic(NULL);
}

void *
TclThreadStorageDataKeyGet(keyPtr)
    Tcl_ThreadDataKey *keyPtr;
{
    ThreadStoragePanic(NULL);
    return NULL;
}

void
TclThreadStorageDataKeySet(keyPtr, data)
    Tcl_ThreadDataKey *keyPtr;
    void *data;
{
    ThreadStoragePanic(NULL);
}

void
TclFinalizeThreadStorageThread(id)
    Tcl_ThreadId id;
{
    ThreadStoragePanic(NULL);
}

void
TclFinalizeThreadStorage()
{
    ThreadStoragePanic(NULL);
}

void
TclFinalizeThreadStorageData(keyPtr)
    Tcl_ThreadDataKey *keyPtr;
{
    ThreadStoragePanic(NULL);
}

void
TclFinalizeThreadStorageDataKey(keyPtr)
    Tcl_ThreadDataKey *keyPtr;
{
    ThreadStoragePanic(NULL);
}

#endif /* defined(TCL_THREADS) && defined(USE_THREAD_STORAGE) */
