/*
 * tclAlloc.c --
 *
 *      This is the generic part of the Tcl allocator. It handles the
 *      freeObjLists and defines which main allocator will be used.
 *
 * Copyright (c) 2013 by Miguel Sofer. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclAlloc.h"

/*
 * Parameters for the per-thread Tcl_Obj cache:
 *   - if >NOBJHIGH free objects, move some to the shared cache
 *   - if no objects are available, create NOBJALLOC of them
 */

#define NOBJHIGH	1200
#define NOBJALLOC	((NOBJHIGH*2)/3)


/*
 * The Tcl_Obj per-thread cache.
 */

typedef struct Cache {
    Tcl_Obj *firstObjPtr;	/* List of free objects for thread */
    int numObjects;		/* Number of objects for thread */
    void *allocCachePtr;
} Cache;

static Cache sharedCache;
#define sharedPtr (&sharedCache)

#if defined(TCL_THREADS)
static Tcl_Mutex *objLockPtr;

static Cache *	GetCache(void);
static void	MoveObjs(Cache *fromPtr, Cache *toPtr, int numMove);

#if defined(HAVE_FAST_TSD)
static __thread Cache *tcachePtr;

# define GETCACHE(cachePtr)			\
    do {					\
	if (!tcachePtr) {			\
	    tcachePtr = GetCache();		\
	}					\
	(cachePtr) = tcachePtr;			\
    } while (0)

#else /* THREADS, not HAVE_FAST_TSD */
# define GETCACHE(cachePtr)			\
    do {					\
	(cachePtr) = TclpGetAllocCache();	\
	if ((cachePtr) == NULL) {		\
	    (cachePtr) = GetCache();		\
	}					\
    } while (0)
#endif /* FAST TSD */

#else /* NOT THREADS */
#define GETCACHE(cachePtr)			\
    (cachePtr) = (&sharedCache)
#endif /* THREADS */


/*
 *----------------------------------------------------------------------
 *
 * GetCache ---
 *
 *	Gets per-thread memory cache, allocating it if necessary.
 *
 * Results:
 *	Pointer to cache.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#if defined(TCL_THREADS)
static Cache *
GetCache(void)
{
    Cache *cachePtr;

    /*
     * Get this thread's cache, allocating if necessary.
     */

    cachePtr = TclpGetAllocCache();
    if (cachePtr == NULL) {
	cachePtr = calloc(1, sizeof(Cache));
	if (cachePtr == NULL) {
	    Tcl_Panic("alloc: could not allocate new cache");
	}
	cachePtr->allocCachePtr= NULL;
	TclpSetAllocCache(cachePtr);
    }
    return cachePtr;
}
#endif

/*
 * TclSetSharedAllocCache, TclSetAllocCache, TclGetAllocCache
 *
 * These are utility functions for the loadable allocator.
 */

void
TclSetSharedAllocCache(
    void *allocCachePtr)
{
    sharedPtr->allocCachePtr = allocCachePtr;
}

void
TclSetAllocCache(
    void *allocCachePtr)
{
    Cache *cachePtr;

    GETCACHE(cachePtr);
    cachePtr->allocCachePtr = allocCachePtr;
}

void *
TclGetAllocCache(void)
{
    Cache *cachePtr;

    GETCACHE(cachePtr);
    return cachePtr->allocCachePtr;
}

  
/*
 *-------------------------------------------------------------------------
 *
 * TclInitAlloc --
 *
 *	Initialize the memory system.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Initialize the mutex used to serialize obj allocations.
 *      Call the allocator-specific initialization.
 *
 *-------------------------------------------------------------------------
 */

void
TclInitAlloc(void)
{
    /*
     * Set the params for the correct allocator
     */

#if defined(TCL_THREADS)
    Tcl_Mutex *initLockPtr;

    TCL_THREADED = 1;
    initLockPtr = Tcl_GetAllocMutex();
    Tcl_MutexLock(initLockPtr);
    objLockPtr = TclpNewAllocMutex();
    TclXpInitAlloc();
    Tcl_MutexUnlock(initLockPtr);
#else
    TCL_THREADED = 0;
    TclXpInitAlloc();
#endif /* THREADS */

#ifdef PURIFY
    TCL_PURIFY = 1;
#else
    TCL_PURIFY   = (getenv("TCL_PURIFY") != NULL);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeAlloc --
 *
 *	This procedure is used to destroy all private resources used in this
 *	file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *      Call the allocator-specific finalization.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeAlloc(void)
{
#if defined(TCL_THREADS)

    TclpFreeAllocMutex(objLockPtr);
    objLockPtr = NULL;

    TclpFreeAllocCache(NULL);
#endif
    TclXpFinalizeAlloc();
}

/*
 *----------------------------------------------------------------------
 *
 * TclFreeAllocCache --
 *
 *	Flush and delete a cache, removing from list of caches.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#if defined(TCL_THREADS)
void
TclFreeAllocCache(
    void *arg)
{
    Cache *cachePtr = arg;
    
    /*
     * Flush objs.
     */

    if (cachePtr->numObjects > 0) {
	Tcl_MutexLock(objLockPtr);
	MoveObjs(cachePtr, sharedPtr, cachePtr->numObjects);
	Tcl_MutexUnlock(objLockPtr);
    }

    /*
     * Flush the external allocator cache
     */

    TclXpFreeAllocCache(cachePtr->allocCachePtr);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TclSmallAlloc --
 *
 *	Allocate a Tcl_Obj sized block from the per-thread cache.
 *
 * Results:
 *	Pointer to uninitialized memory.
 *
 * Side effects:
 *	May move blocks from shared cached or allocate new blocks if
 *	list is empty.
 *
 *----------------------------------------------------------------------
 */

void *
TclSmallAlloc(void)
{
    register Cache *cachePtr;
    register Tcl_Obj *objPtr;
    int numMove;
    Tcl_Obj *newObjsPtr;
   
    GETCACHE(cachePtr);

    /*
     * Pop the first object.
     */

    if(cachePtr->firstObjPtr) {
	haveObj:
	objPtr = cachePtr->firstObjPtr;
	cachePtr->firstObjPtr = objPtr->internalRep.otherValuePtr;
	cachePtr->numObjects--;
	return objPtr;
    }

    /*
     * Do it AFTER looking at the queue, so that it doesn't slow down
     * non-purify small allocs.
     */
    
    if (TCL_PURIFY) {
	Tcl_Obj *objPtr = (Tcl_Obj *) TclpAlloc(sizeof(Tcl_Obj));
	if (objPtr == NULL) {
	    Tcl_Panic("alloc: could not allocate a new object");
	}
	return objPtr;
    }
    
    /*
     * Get this thread's obj list structure and move or allocate new objs if
     * necessary.
     */

#if defined(TCL_THREADS)
    Tcl_MutexLock(objLockPtr);
    numMove = sharedPtr->numObjects;
    if (numMove > 0) {
	if (numMove > NOBJALLOC) {
	    numMove = NOBJALLOC;
	}
	MoveObjs(sharedPtr, cachePtr, numMove);
    }
    Tcl_MutexUnlock(objLockPtr);
    if (cachePtr->firstObjPtr) {
	goto haveObj;
    }
#endif	
    cachePtr->numObjects = numMove = NOBJALLOC;
    newObjsPtr = malloc(sizeof(Tcl_Obj) * numMove);
    if (newObjsPtr == NULL) {
	Tcl_Panic("alloc: could not allocate %d new objects", numMove);
    }
    while (--numMove >= 0) {
	objPtr = &newObjsPtr[numMove];
	objPtr->internalRep.otherValuePtr = cachePtr->firstObjPtr;
	cachePtr->firstObjPtr = objPtr;
    }
    goto haveObj;
}


/*
 *----------------------------------------------------------------------
 *
 * TclSmallFree --
 *
 *	Return a free Tcl_Obj-sized block to the per-thread cache.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May move free blocks to shared list upon hitting high water mark.
 *
 *----------------------------------------------------------------------
 */

void
TclSmallFree(
    void *ptr)
{
    Cache *cachePtr;
    Tcl_Obj *objPtr = ptr;
    
    if (TCL_PURIFY) {
	TclpFree((char *) ptr);
	return;
    }
    
    GETCACHE(cachePtr);

    /*
     * Get this thread's list and push on the free Tcl_Obj.
     */

    objPtr->internalRep.otherValuePtr = cachePtr->firstObjPtr;
    cachePtr->firstObjPtr = objPtr;
    cachePtr->numObjects++;

#if defined(TCL_THREADS)
    /*
     * If the number of free objects has exceeded the high water mark, move
     * some blocks to the shared list.
     */

    if (cachePtr->numObjects > NOBJHIGH) {
	Tcl_MutexLock(objLockPtr);
	MoveObjs(cachePtr, sharedPtr, NOBJALLOC);
	Tcl_MutexUnlock(objLockPtr);
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * MoveObjs --
 *
 *	Move Tcl_Obj's between caches.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#if defined(TCL_THREADS)
static void
MoveObjs(
    Cache *fromPtr,
    Cache *toPtr,
    int numMove)
{
    register Tcl_Obj *objPtr = fromPtr->firstObjPtr;
    Tcl_Obj *fromFirstObjPtr = objPtr;

    toPtr->numObjects += numMove;
    fromPtr->numObjects -= numMove;

    /*
     * Find the last object to be moved; set the next one (the first one not
     * to be moved) as the first object in the 'from' cache.
     */

    while (--numMove) {
	objPtr = objPtr->internalRep.otherValuePtr;
    }
    fromPtr->firstObjPtr = objPtr->internalRep.otherValuePtr;

    /*
     * Move all objects as a block - they are already linked to each other, we
     * just have to update the first and last.
     */

    objPtr->internalRep.otherValuePtr = toPtr->firstObjPtr;
    toPtr->firstObjPtr = fromFirstObjPtr;
}
#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
