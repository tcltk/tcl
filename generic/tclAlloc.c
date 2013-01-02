/*
 * tclAlloc.c --
 *
 *      This is a very flexible storage allocator for Tcl, for use with or
 *      without threads. Depending on the value of TCL_ALLOCATOR it compiles
 *      as: 
 *
 *      (1) aPURIFY
 *          A purify build, use the native malloc for all allocs including
 *          Tcl_Objs  
 *
 *      (2) aNATIVE
 *          Use the native malloc and a per-thread Tcl_Obj pool, with 
 *          inter-thread recycling of objects. 
 *          TODO: in this case build ZIPPY as a preloadable malloc-replacement?           
 *
 *      (3) aZIPPY
 *          use the ex-tclThreadAlloc, essentially aolserver's fast threaded
 *          allocator. Mods with respect to the original: 
 *            - split blocks in the shared pool before mallocing again for
 *              improved cache usage
 *            - stats and Tcl_GetMemoryInfo disabled per default, enable with
 *              -DZIPPY_STATS
 *            - adapt for unthreaded usage as replacement of the ex tclAlloc
 *            - -DHAVE_FAST_TSD: use fast TSD via __thread where available
 *            - (TODO!) build zippy as a pre-loadable library to use with a
 *              native build as a malloc replacement. Difficulty is to make it
 *              portable (easy enough on modern elf/unix, to be researched on
 *              win and mac . This would be the best option, instead of
 *              MULTI. It could be built in two versions (perf, debug/stats)
 *
 *      (4) aMULTI
 *          all of the above, selectable at startup with an env var. This
 *          build will be slightly slower than the specific builds above.
 *
 * All variants can be built for both threaded and unthreaded Tcl.
 *
 * The Initial Developer of the Original Code is America Online, Inc.
 * Portions created by AOL are Copyright (C) 1999 America Online, Inc.
 *
 * Copyright (c) 2008-2011 by Miguel Sofer. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

#undef TclpAlloc
#undef TclpRealloc
#undef TclpFree
#undef TclSmallAlloc
#undef TclSmallFree

#if !USE_ZIPPY
/*
 * Not much of this file is needed, most things are dealt with in the
 * macros. Just shunt the allocators for use by the library, the core
 * never calls this.
 *
 * This is all that is needed for a TCL_ALLOC_PURIFY build, a native build
 * needs the Tcl_Obj pools too.
 */
   
char *
TclpAlloc(
    unsigned int reqSize)
{
    return malloc(reqSize);
}

char *
TclpRealloc(  
    char *ptr,
    unsigned int reqSize)
{
    return realloc(ptr, reqSize);
}

void
TclpFree(
    char *ptr)
{
    free(ptr);
}

#endif /* !USE_ZIPPY, this is end of code for aPURIFY */


#if USE_OBJQ

/*
 * Parameters for the per-thread Tcl_Obj cache
 * Actual definition of NOBJHIGH moved to tclInt.h to be used in macros
 */

#define NOBJHIGH	ALLOC_NOBJHIGH
#define NOBJALLOC	((NOBJHIGH*2)/3)


/*
 * Advance some defs that are relevant for ZIPPY an MULTI, in order to use
 * them in macro and struct definitions
 */

#if USE_ZIPPY
/*
 * This macro is used to properly align the memory allocated by Tcl, giving
 * the same alignment as the native malloc.
 */

#if defined(__APPLE__)
#define TCL_ALLOCALIGN	16
#else
#define TCL_ALLOCALIGN	(2*sizeof(void *))
#endif

#define ALIGN(x)    (((x) + TCL_ALLOCALIGN - 1) & ~(TCL_ALLOCALIGN - 1))

/*
 * The following struct stores accounting information for each block including
 * two small magic numbers and a bucket number when in use or a next pointer
 * when free. The original requested size (not including the Block overhead)
 * is also maintained.
 */

typedef struct Block {
    union {
	struct Block *next;		/* Next in free list. */
	struct {
	    unsigned char magic1;	/* First magic number. */
	    unsigned char bucket;	/* Bucket block allocated from. */
	    unsigned char inUse;	/* Block memory currently in use, see
					 * details in TclpAlloc/Realloc. */
	    unsigned char magic2;	/* Second magic number. */
	} s;
    } u;
    long refCount;
} Block;

#define OFFSET      ALIGN(sizeof(Block))

#define nextBlock	u.next
#define sourceBucket	u.s.bucket
#define magicNum1	u.s.magic1
#define magicNum2	u.s.magic2
#define used	        u.s.inUse
#define MAGIC		0xEF

/*
 * The following defines the minimum and maximum block sizes and the number
 * of buckets in the bucket cache.
 *                        32b    64b    Apple-32b(?)
 *     TCL_ALLOCALIGN       8     16       16
 *     sizeof(Block)        8     16       16
 *     OFFSET               8     16       16
 *     MINALLOC            16     32       32
 *     NBUCKETS            11     10       10
 *     MAXALLOC         16384  16384    16384
 *     small allocs      1024    512     1024
 *        at a time
 */

#define MINALLOC    ALIGN(OFFSET+8)
#define NBUCKETS    (11 - (MINALLOC >> 5))
#define MAXALLOC    (MINALLOC << (NBUCKETS - 1))

/*
 * The following structure defines a bucket of blocks, optionally with various
 * accounting and statistics information.
 */

typedef struct Bucket {
    Block *firstPtr;		/* First block available */
    long numFree;		/* Number of blocks available */
#ifdef ZIPPY_STATS
    /* All fields below for accounting only */

    long numRemoves;		/* Number of removes from bucket */
    long numInserts;		/* Number of inserts into bucket */
    long numWaits;		/* Number of waits to acquire a lock */
    long numLocks;		/* Number of locks acquired */
#endif
} Bucket;

/*
 * The following array specifies various per-bucket limits and locks. The
 * values are statically initialized to avoid calculating them repeatedly.
 */

static struct {
    size_t blockSize;		/* Bucket blocksize. */
    int shift;
#if defined(TCL_THREADS)
    int maxBlocks;		/* Max blocks before move to share. */
    int numMove;		/* Num blocks to move to share. */
    Tcl_Mutex *lockPtr;		/* Share bucket lock. */
#endif
} bucketInfo[NBUCKETS];

#endif /* Advanced USE_ZIPPY definitions, back to common code */


/*
 * The Tcl_Obj per-thread cache, used by aNATIVE, aZIPPY and aMULTI.
 */

typedef struct Cache {
    Tcl_Obj *firstObjPtr;	/* List of free objects for thread */
    int numObjects;		/* Number of objects for thread */
#if defined(TCL_THREADS)
    struct Cache *nextPtr;	/* Linked list of cache entries */
#endif
#if USE_ZIPPY
#if defined(TCL_THREADS)
    Tcl_ThreadId owner;		/* Which thread's cache is this? */
#endif
    Bucket buckets[NBUCKETS];	/* The buckets for this thread */
#endif /* USE_ZIPPY, ie TCL_ALLOCATOR != aNATIVE */
} Cache;

static Cache sharedCache;
#define sharedPtr (&sharedCache)

#if defined(TCL_THREADS)
static Tcl_Mutex *objLockPtr;
static Tcl_Mutex *listLockPtr;
static Cache *firstCachePtr = &sharedCache;

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

#if USE_ZIPPY
static void InitBucketInfo(void);
static inline char * Block2Ptr(Block *blockPtr,
	int bucket, unsigned int reqSize);
static inline Block * Ptr2Block(char *ptr);

static int  GetBlocks(Cache *cachePtr, int bucket);

#if (TCL_THREADS)
static void PutBlocks(Cache *cachePtr, int bucket, int numMove);
static void LockBucket(Cache *cachePtr, int bucket);
static void UnlockBucket(Cache *cachePtr, int bucket);
#else
#define PutBlocks(cachePtr, bucket, numMove)
#endif

#if TCL_ALLOCATOR == aMULTI
static int allocator;
static void ChooseAllocator();
#endif

#endif /* USE_ZIPPY */

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
	Tcl_MutexLock(listLockPtr);
	cachePtr->nextPtr = firstCachePtr;
	firstCachePtr = cachePtr;
	Tcl_MutexUnlock(listLockPtr);
#if  USE_ZIPPY && defined(ZIPPY_STATS)
	cachePtr->owner = Tcl_GetCurrentThread();
#endif
	TclpSetAllocCache(cachePtr);
    }
    return cachePtr;
}
#endif

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
 *	Initialize the mutex used to serialize allocations.
 *
 *-------------------------------------------------------------------------
 */

#if USE_ZIPPY
static void
InitBucketInfo ()
{
    int i;
    int shift = 0;
    
    for (i = 0; i < NBUCKETS; ++i) {
	bucketInfo[i].blockSize = MINALLOC << i;
	while (((bucketInfo[i].blockSize -OFFSET) >> shift) > 255) {
	    ++shift;
	}
	bucketInfo[i].shift = shift;
#if defined(TCL_THREADS)
	/* TODO: clearer logic? Change move to keep? */
	bucketInfo[i].maxBlocks = 1 << (NBUCKETS - 1 - i);
	bucketInfo[i].numMove = i < NBUCKETS - 1 ?
	    1 << (NBUCKETS - 2 - i) : 1;
	bucketInfo[i].lockPtr = TclpNewAllocMutex();
#endif
    }
}
#endif

void
TclInitAlloc(void)
{
    /*
     * Set the params for the correct allocator
     */

#if defined(TCL_THREADS)
    if (listLockPtr == NULL) {
	Tcl_Mutex *initLockPtr;
	initLockPtr = Tcl_GetAllocMutex();
	Tcl_MutexLock(initLockPtr);
	if (listLockPtr == NULL) {
	    listLockPtr = TclpNewAllocMutex();
	    objLockPtr = TclpNewAllocMutex();
#if USE_ZIPPY
	    InitBucketInfo();
#endif
	}
	Tcl_MutexUnlock(initLockPtr);
    }
#elif USE_ZIPPY
    InitBucketInfo();
#endif /* THREADS */

#if TCL_ALLOCATOR == aMULTI
    ChooseAllocator();
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
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeAlloc(void)
{
#if defined(TCL_THREADS)

#if USE_ZIPPY
    unsigned int i;

    for (i = 0; i < NBUCKETS; ++i) {
	TclpFreeAllocMutex(bucketInfo[i].lockPtr);
	bucketInfo[i].lockPtr = NULL;
    }
#endif
    
    TclpFreeAllocMutex(objLockPtr);
    objLockPtr = NULL;

    TclpFreeAllocMutex(listLockPtr);
    listLockPtr = NULL;

    TclpFreeAllocCache(NULL);
#endif
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
    Cache **nextPtrPtr;
#if USE_ZIPPY
    register unsigned int bucket;

    /*
     * Flush blocks.
     */

    for (bucket = 0; bucket < NBUCKETS; ++bucket) {
	if (cachePtr->buckets[bucket].numFree > 0) {
	    PutBlocks(cachePtr, bucket, cachePtr->buckets[bucket].numFree);
	}
    }
#endif
    
    /*
     * Flush objs.
     */

    if (cachePtr->numObjects > 0) {
	Tcl_MutexLock(objLockPtr);
	MoveObjs(cachePtr, sharedPtr, cachePtr->numObjects);
	Tcl_MutexUnlock(objLockPtr);
    }

    /*
     * Remove from pool list.
     */

    Tcl_MutexLock(listLockPtr);
    nextPtrPtr = &firstCachePtr;
    while (*nextPtrPtr != cachePtr) {
	nextPtrPtr = &(*nextPtrPtr)->nextPtr;
    }
    *nextPtrPtr = cachePtr->nextPtr;
    cachePtr->nextPtr = NULL;
    Tcl_MutexUnlock(listLockPtr);
    free(cachePtr);
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

#if TCL_ALLOCATOR == aMULTI
    /*
     * Do it AFTER looking at the queue, so that it doesn't slow down
     * non-purify small allocs.
     */
    
    if (allocator == aPURIFY) {
	return (Tcl_Obj *) malloc(sizeof(Tcl_Obj));
    }
#endif
    
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
    
#if TCL_ALLOCATOR == aMULTI
    if (allocator == aPURIFY) {
	free((char *) ptr);
	return;
    }
#endif
    
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
#endif /* end of code for aNATIVE */

#if USE_ZIPPY
/*
 * The rest of this file deals with aZIPPY and aMULTI builds
 */

/*
 * If range checking is enabled, an additional byte will be allocated to store
 * the magic number at the end of the requested memory.
 */


/*
 *----------------------------------------------------------------------
 *
 * Block2Ptr, Ptr2Block --
 *
 *	Convert between internal blocks and user pointers.
 *
 * Results:
 *	User pointer or internal block.
 *
 * Side effects:
 *	Invalid blocks will abort the server.
 *
 *----------------------------------------------------------------------
 */

static inline char *
Block2Ptr(
    Block *blockPtr,
    int bucket,
    unsigned int reqSize)
{
    register void *ptr;
    
    blockPtr->magicNum1 = blockPtr->magicNum2 = MAGIC;
    if (bucket == NBUCKETS) {
	blockPtr->used = 255;
    } else {
	blockPtr->used = (reqSize >> bucketInfo[bucket].shift);
    }
    blockPtr->sourceBucket = bucket;
    ptr = (void *) (((char *)blockPtr) + OFFSET);
    blockPtr->refCount = 0;
    return (char *) ptr;
}

static inline Block *
Ptr2Block(
    char *ptr)
{
    register Block *blockPtr;

    blockPtr = (Block *) (((char *) ptr) - OFFSET);
    if (blockPtr->magicNum1 != MAGIC || blockPtr->magicNum2 != MAGIC) {
	Tcl_Panic("alloc: invalid block: %p: %x %x",
		blockPtr, blockPtr->magicNum1, blockPtr->magicNum2);
    }
    return blockPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpAlloc --
 *
 *	Allocate memory.
 *
 * Results:
 *	Pointer to memory just beyond Block pointer.
 *
 * Side effects:
 *	May allocate more blocks for a bucket.
 *
 *----------------------------------------------------------------------
 */

char *
TclpAlloc(
    unsigned int reqSize)
{
    Cache *cachePtr;
    Block *blockPtr;
    register int bucket;
    size_t size;

#if TCL_ALLOCATOR == aMULTI
    if (allocator < aNONE) {
	return (void *) malloc(reqSize);
    }
#endif
    
    GETCACHE(cachePtr);

#ifndef __LP64__
    if (sizeof(int) >= sizeof(size_t)) {
	/* An unsigned int overflow can also be a size_t overflow */
	const size_t zero = 0;
	const size_t max = ~zero;

	if (((size_t) reqSize) > max - OFFSET) {
	    /* Requested allocation exceeds memory */
	    return NULL;
	}
    }
#endif

    /*
     * Increment the requested size to include room for the Block structure.
     * Call malloc() directly if the required amount is greater than the
     * largest block, otherwise pop the smallest block large enough,
     * allocating more blocks if necessary.
     */

    size = reqSize + OFFSET;
    if (size > MAXALLOC) {
	bucket = NBUCKETS;
	blockPtr = malloc(size);
    } else {
	blockPtr = NULL;
	bucket = 0;
	while (bucketInfo[bucket].blockSize < size) {
	    bucket++;
	}
	if (cachePtr->buckets[bucket].numFree || GetBlocks(cachePtr, bucket)) {
	    blockPtr = cachePtr->buckets[bucket].firstPtr;
	    cachePtr->buckets[bucket].firstPtr = blockPtr->nextBlock;
	    cachePtr->buckets[bucket].numFree--;
#ifdef ZIPPY_STATS
	    cachePtr->buckets[bucket].numRemoves++;
#endif
	}
	if (blockPtr == NULL) {
	    return NULL;
	}
    }
    return Block2Ptr(blockPtr, bucket, reqSize);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpFree --
 *
 *	Return blocks to the thread block cache.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May move blocks to shared cache.
 *
 *----------------------------------------------------------------------
 */

void
TclpFree(
    char *ptr)
{
    Cache *cachePtr;
    Block *blockPtr;
    int bucket;

#if TCL_ALLOCATOR == aMULTI
    if (allocator < aNONE) {
	return free((char *) ptr);
    }
#endif
    
    if (ptr == NULL) {
	return;
    }

    blockPtr = Ptr2Block(ptr);
    if (blockPtr->refCount != 0) {
	/* Tcl_Panic("trying to free a preserved pointer");*/
    }
    
    /*
     * Get the block back from the user pointer and call system free directly
     * for large blocks. Otherwise, push the block back on the bucket and move
     * blocks to the shared cache if there are now too many free.
     */

    bucket = blockPtr->sourceBucket;
    if (bucket == NBUCKETS) {
	free(blockPtr);
	return;
    }

    GETCACHE(cachePtr);

    blockPtr->nextBlock = cachePtr->buckets[bucket].firstPtr;
    cachePtr->buckets[bucket].firstPtr = blockPtr;
    cachePtr->buckets[bucket].numFree++;
#ifdef ZIPPY_STATS
    cachePtr->buckets[bucket].numInserts++;
#endif
#if defined(TCL_THREADS)
    if (cachePtr != sharedPtr &&
	    cachePtr->buckets[bucket].numFree > bucketInfo[bucket].maxBlocks) {
	PutBlocks(cachePtr, bucket, bucketInfo[bucket].numMove);
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TclpRealloc --
 *
 *	Re-allocate memory to a larger or smaller size.
 *
 * Results:
 *	Pointer to memory just beyond Block pointer.
 *
 * Side effects:
 *	Previous memory, if any, may be freed.
 *
 *----------------------------------------------------------------------
 */

char *
TclpRealloc(  
    char *ptr,
    unsigned int reqSize)
{
    Block *blockPtr;
    void *newPtr;
    size_t size, min;
    int bucket;

#if TCL_ALLOCATOR == aMULTI
    if (allocator < aNONE) {
	return (void *) realloc((char *) ptr, reqSize);
    }
#endif
    
    if (ptr == NULL) {
	return TclpAlloc(reqSize);
    }

#ifndef __LP64__
    if (sizeof(int) >= sizeof(size_t)) {
	/* An unsigned int overflow can also be a size_t overflow */
	const size_t zero = 0;
	const size_t max = ~zero;

	if (((size_t) reqSize) > max - OFFSET) {
	    /* Requested allocation exceeds memory */
	    return NULL;
	}
    }
#endif

    /*
     * If the block is not a system block and belongs in the same block,
     * simply return the existing pointer. Otherwise, if the block is a system
     * block and the new size would also require a system block, call
     * realloc() directly. 
     */

    blockPtr = Ptr2Block(ptr);
    if (blockPtr->refCount != 0) {
	Tcl_Panic("trying to realloc a preserved pointer");
    }
    
    size = reqSize + OFFSET;
    bucket = blockPtr->sourceBucket;
    if (bucket != NBUCKETS) {
	if (bucket > 0) {
	    min = bucketInfo[bucket-1].blockSize;
	} else {
	    min = 0;
	}
	if (size > min && size <= bucketInfo[bucket].blockSize) {
	    return Block2Ptr(blockPtr, bucket, reqSize);
	}
    } else if (size > MAXALLOC) {
	blockPtr = realloc(blockPtr, size);
	if (blockPtr == NULL) {
	    return NULL;
	}
	return Block2Ptr(blockPtr, NBUCKETS, reqSize);
    }

    /*
     * Finally, perform an expensive malloc/copy/free.
     */

    newPtr = TclpAlloc(reqSize);
    if (newPtr != NULL) {
	size_t maxSize = bucketInfo[bucket].blockSize - OFFSET;
	size_t toCopy = ((blockPtr->used + 1) << bucketInfo[bucket].shift);

	if (toCopy > maxSize) {
	    toCopy = maxSize;
	}
	if (toCopy > reqSize) {
	    toCopy = reqSize;
	}
	
	memcpy(newPtr, ptr, toCopy);
	TclpFree(ptr);
    }
    return newPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclAllocMaximize --
 *
 * Given a TclpAlloc'ed pointer, it returns the maximal size that can be used
 * by the allocated memory. This is almost always larger than the requested
 * size, as it corresponds to the bucket's size.
 *
 * Results:
 *	New size.
 *
 *----------------------------------------------------------------------
 */
 unsigned int
 TclAllocMaximize(
     void *ptr)
{
    Block *blockPtr;
    int bucket;
    size_t size;

	return UINT_MAX;
#if TCL_ALLOCATOR == aMULTI
    if (allocator < aNONE) {
	/*
	 * No info, return UINT_MAX as a signal.
	 */
	
	return UINT_MAX;
    }
#endif
    
    blockPtr = Ptr2Block(ptr);
    bucket = blockPtr->sourceBucket;
    
    if (bucket == NBUCKETS) {
	/*
	 * System malloc'ed: no info
	 */
	
	return UINT_MAX;
    }

    size = bucketInfo[bucket].blockSize - OFFSET;
    blockPtr->used = 255;
    return size;
}

#ifdef ZIPPY_STATS

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetMemoryInfo --
 *
 *	Return a list-of-lists of memory stats.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	List appended to given dstring.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_GetMemoryInfo(
    Tcl_DString *dsPtr)
{
    Cache *cachePtr;
    char buf[200];
    unsigned int n;

    Tcl_MutexLock(listLockPtr);
    cachePtr = firstCachePtr;
    while (cachePtr != NULL) {
	Tcl_DStringStartSublist(dsPtr);
#if defined(TCL_THREADS)
	if (cachePtr == sharedPtr) {
	    Tcl_DStringAppendElement(dsPtr, "shared");
	} else {
	    sprintf(buf, "thread%p", cachePtr->owner);
	    Tcl_DStringAppendElement(dsPtr, buf);
	}
#else
	Tcl_DStringAppendElement(dsPtr, "unthreaded");	    
#endif
	for (n = 0; n < NBUCKETS; ++n) {
	    sprintf(buf, "%lu %ld %ld %ld %ld %ld %ld",
		    (unsigned long) bucketInfo[n].blockSize,
		    cachePtr->buckets[n].numFree,
		    cachePtr->buckets[n].numRemoves,
		    cachePtr->buckets[n].numInserts,
		    cachePtr->buckets[n].numLocks,
		    cachePtr->buckets[n].numWaits);
	    Tcl_DStringAppendElement(dsPtr, buf);
	}
	Tcl_DStringEndSublist(dsPtr);
#if defined(TCL_THREADS)
	cachePtr = cachePtr->nextPtr;
#else
	cachePtr = NULL;
#endif
    }
    Tcl_MutexUnlock(listLockPtr);
}
#endif /* ZIPPY_STATS */

#if defined(TCL_THREADS)
/*
 *----------------------------------------------------------------------
 *
 * LockBucket, UnlockBucket --
 *
 *	Set/unset the lock to access a bucket in the shared cache.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Lock activity and contention are monitored globally and on a per-cache
 *	basis.
 *
 *----------------------------------------------------------------------
 */

static void
LockBucket(
    Cache *cachePtr,
    int bucket)
{
    Tcl_MutexLock(bucketInfo[bucket].lockPtr);
#ifdef ZIPPY_STATS
    cachePtr->buckets[bucket].numLocks++;
    sharedPtr->buckets[bucket].numLocks++;
#endif
}

static void
UnlockBucket(
    Cache *cachePtr,
    int bucket)
{
    Tcl_MutexUnlock(bucketInfo[bucket].lockPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PutBlocks --
 *
 *	Return unused blocks to the shared cache.
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
PutBlocks(
    Cache *cachePtr,
    int bucket,
    int numMove)
{
    register Block *lastPtr, *firstPtr;
    register int n = numMove;

    /*
     * Before acquiring the lock, walk the block list to find the last block
     * to be moved.
     */

    firstPtr = lastPtr = cachePtr->buckets[bucket].firstPtr;
    while (--n > 0) {
	lastPtr = lastPtr->nextBlock;
    }
    cachePtr->buckets[bucket].firstPtr = lastPtr->nextBlock;
    cachePtr->buckets[bucket].numFree -= numMove;

    /*
     * Aquire the lock and place the list of blocks at the front of the shared
     * cache bucket.
     */

    LockBucket(cachePtr, bucket);
    lastPtr->nextBlock = sharedPtr->buckets[bucket].firstPtr;
    sharedPtr->buckets[bucket].firstPtr = firstPtr;
    sharedPtr->buckets[bucket].numFree += numMove;
    UnlockBucket(cachePtr, bucket);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * GetBlocks --
 *
 *	Get more blocks for a bucket.
 *
 * Results:
 *	1 if blocks where allocated, 0 otherwise.
 *
 * Side effects:
 *	Cache may be filled with available blocks.
 *
 *----------------------------------------------------------------------
 */

static int
GetBlocks(
    Cache *cachePtr,
    int bucket)
{
    register Block *blockPtr = NULL;
    register int n;

#if defined(TCL_THREADS)
    /*
     * First, atttempt to move blocks from the shared cache. Note the
     * potentially dirty read of numFree before acquiring the lock which is a
     * slight performance enhancement. The value is verified after the lock is
     * actually acquired.
     */

    if (cachePtr != sharedPtr && sharedPtr->buckets[bucket].numFree > 0) {
	LockBucket(cachePtr, bucket);
	if (sharedPtr->buckets[bucket].numFree > 0) {

	    /*
	     * Either move the entire list or walk the list to find the last
	     * block to move.
	     */

	    n = bucketInfo[bucket].numMove;
	    if (n >= sharedPtr->buckets[bucket].numFree) {
		cachePtr->buckets[bucket].firstPtr =
			sharedPtr->buckets[bucket].firstPtr;
		cachePtr->buckets[bucket].numFree =
			sharedPtr->buckets[bucket].numFree;
		sharedPtr->buckets[bucket].firstPtr = NULL;
		sharedPtr->buckets[bucket].numFree = 0;
	    } else {
		blockPtr = sharedPtr->buckets[bucket].firstPtr;
		cachePtr->buckets[bucket].firstPtr = blockPtr;
		sharedPtr->buckets[bucket].numFree -= n;
		cachePtr->buckets[bucket].numFree = n;
		while (--n > 0) {
		    blockPtr = blockPtr->nextBlock;
		}
		sharedPtr->buckets[bucket].firstPtr = blockPtr->nextBlock;
		blockPtr->nextBlock = NULL;
	    }
	}
	UnlockBucket(cachePtr, bucket);
    }
#endif
    
    if (cachePtr->buckets[bucket].numFree == 0) {
	register size_t size;

	/*
	 * If no blocks could be moved from shared, first look for a larger
	 * block in this cache OR the shared cache to split up.
	 */
	
	n = NBUCKETS;
	size = 0; /* lint */
	while (--n > bucket) {
	    if (cachePtr->buckets[n].numFree > 0) {
		size = bucketInfo[n].blockSize;
		blockPtr = cachePtr->buckets[n].firstPtr;
		cachePtr->buckets[n].firstPtr = blockPtr->nextBlock;
		cachePtr->buckets[n].numFree--;
		break;
	    }
	}
#if defined(TCL_THREADS)
	if (blockPtr == NULL) {
	    n = NBUCKETS;
	    size = 0; /* lint */
	    while (--n > bucket) {
		if (sharedPtr->buckets[n].numFree > 0) {
		    size = bucketInfo[n].blockSize;
		    LockBucket(cachePtr, n);
		    if (sharedPtr->buckets[n].numFree > 0) {
			blockPtr = sharedPtr->buckets[n].firstPtr;
			sharedPtr->buckets[n].firstPtr = blockPtr->nextBlock;
			sharedPtr->buckets[n].numFree--;
			UnlockBucket(cachePtr, n);
			break;		
		    }
		    UnlockBucket(cachePtr, n);
		}
	    }
	}
#endif
	/*
	 * Otherwise, allocate a big new block directly.
	 */

	if (blockPtr == NULL) {
	    size = MAXALLOC;
	    blockPtr = malloc(size);
	    if (blockPtr == NULL) {
		return 0;
	    }
	}

	/*
	 * Split the larger block into smaller blocks for this bucket.
	 */

	n = size / bucketInfo[bucket].blockSize;
	cachePtr->buckets[bucket].numFree = n;
	cachePtr->buckets[bucket].firstPtr = blockPtr;
	while (--n > 0) {
	    blockPtr->nextBlock = (Block *)
		((char *) blockPtr + bucketInfo[bucket].blockSize);
	    blockPtr = blockPtr->nextBlock;
	}
	blockPtr->nextBlock = NULL;
    }
    return 1;
}

#if TCL_ALLOCATOR == aMULTI
static void
ChooseAllocator()
{
    char *choice = getenv("TCL_ALLOCATOR");

    /*
     * This is only called when compiled with aMULTI
     */
    
    allocator = aZIPPY;

    if (choice) {
	/*
	 * Only override the base when requesting native or purify
	 */
	
	if (!strcmp(choice, "aNATIVE")) {
	    allocator = aNATIVE;
	} else if (!strcmp(choice, "aPURIFY")) {
	    allocator = aPURIFY;
	}
    }
}
#endif

#if USE_NEW_PRESERVE

typedef struct PreserveData {
    struct PreserveData *nextPtr;
    char *ptr;
    Tcl_FreeProc *freeProc;	/* Function to call to free. */
} PreserveData;

static PreserveData *pdataPtr = NULL;
TCL_DECLARE_MUTEX(preserveMutex) /* To protect the above statics */


/*
 *----------------------------------------------------------------------
 *
 * TclFinalizePreserve --
 *
 *	Called during exit processing to clean up the reference array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the storage of the reference array.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
void
TclFinalizePreserve(void)
{
    PreserveData *thisPtr;
    
    while (pdataPtr) {
	Tcl_MutexLock(&preserveMutex);
	if (!pdataPtr) {
	    Tcl_MutexUnlock(&preserveMutex);
	    break;
	}   
	thisPtr = pdataPtr;
	pdataPtr = pdataPtr->nextPtr;
	Tcl_MutexUnlock(&preserveMutex);
	
	thisPtr->freeProc(thisPtr->ptr);
	TclSmallFree(thisPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Preserve --
 *
 *	This function is used by a function to declare its interest in a
 *	particular block of memory, so that the block will not be reallocated
 *	until a matching call to Tcl_Release has been made.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information is retained so that the block of memory will not be freed
 *	until at least the matching call to Tcl_Release.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Preserve(
    ClientData clientData)	/* Pointer to malloc'ed block of memory. */
{
    char *ptr = (char *) clientData;
    Block *blockPtr;
    long refCount;

    blockPtr = Ptr2Block(ptr);
    refCount = blockPtr->refCount;
    
    if (refCount > 0) {
	++blockPtr->refCount;
    } else if (refCount < 0) {
	/*
	 * TclEventuallyFree has already been called on this with
	 * (freeProc != TCL_DYNAMIC)
	 */
	
	--blockPtr->refCount;
    } else {
	/*
	 * First preserve call: add one refcount to signal that EventuallyFree
	 * has not yet been called (role of the old '!mustFree')
	 */
  
	blockPtr->refCount = 2;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Release --
 *
 *	This function is called to cancel a previous call to Tcl_Preserve,
 *	thereby allowing a block of memory to be freed (if no one else cares
 *	about it).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If Tcl_EventuallyFree has been called for clientData, and if no other
 *	call to Tcl_Preserve is still in effect, the block of memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Release(
    ClientData clientData)	/* Pointer to malloc'ed block of memory. */
{
    char *ptr = (char *) clientData;
    Block *blockPtr = Ptr2Block(ptr);
    long refCount = blockPtr->refCount;
    int hasFreeProc = (refCount < 0);
    
    if (refCount > 0) {
	refCount = --blockPtr->refCount;
    } else if (refCount < 0) {
	refCount = ++blockPtr->refCount;
    } else {
	Tcl_Panic("Tcl_Release couldn't find reference for %p", clientData);
    }
    
    if (refCount == 0) {
	if (!hasFreeProc) {
	    TclpFree(ptr);
	} else {
	    PreserveData *thisPtr = pdataPtr, *lastPtr = NULL;
	    Tcl_MutexLock(&preserveMutex);
	    while (thisPtr && (thisPtr->ptr != ptr)) {
		lastPtr = thisPtr;
		thisPtr = thisPtr->nextPtr;
	    }
	    if (!thisPtr) {
		Tcl_Panic("Tcl_Release couldn't find reference for %p", clientData);
	    }
	    if (lastPtr) {
		lastPtr->nextPtr = thisPtr->nextPtr;
	    } else {
		pdataPtr = thisPtr->nextPtr;
	    }
	    Tcl_MutexUnlock(&preserveMutex);
	    
	    thisPtr->freeProc(clientData);
	    TclSmallFree(thisPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_EventuallyFree --
 *
 *	Free up a block of memory, unless a call to Tcl_Preserve is in effect
 *	for that block. In this case, defer the free until all calls to
 *	Tcl_Preserve have been undone by matching calls to Tcl_Release.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Ptr may be released by calling free().
 *
 *----------------------------------------------------------------------
 */

void
Tcl_EventuallyFree(
    ClientData clientData,	/* Pointer to malloc'ed block of memory. */
    Tcl_FreeProc *freeProc)	/* Function to actually do free. */
{
    char *ptr = (char *) clientData;
    Block *blockPtr = Ptr2Block(ptr);
    PreserveData *thisPtr;
    long refCount = blockPtr->refCount;
    int hasFreeProc = (refCount < 0);

    if (hasFreeProc) {
	Tcl_Panic("Tcl_EventuallyFree called twice for %p", clientData);
    }

    if (refCount < 2) {
	/*
	 * No other reference for this block.  Free it now.
	 */

	blockPtr->refCount = 0;
	if (freeProc == TCL_DYNAMIC) {
	    ckfree(clientData);
	} else {
	    freeProc(clientData);
	}
	return;
    }

    --blockPtr->refCount;
    
    if (freeProc != TCL_DYNAMIC) {	
	blockPtr->refCount = -blockPtr->refCount;
	TclCkSmallAlloc(sizeof(PreserveData), thisPtr);

	thisPtr->ptr = ptr;
	thisPtr->freeProc = freeProc;	
	Tcl_MutexLock(&preserveMutex);
	thisPtr->nextPtr = pdataPtr;
	pdataPtr = thisPtr;
	Tcl_MutexUnlock(&preserveMutex);
    }
}
#endif /* USE_NEW_PRESERVE */

#endif /* USE_ZIPPY */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
