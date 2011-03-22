/*
 * tclAlloc.c --
 *
 *      This is a very flexible storage allocator for Tcl, for use with or
 *      without threads. Depending on the compile flags, it builds as:
 *
 *      (1) Build flags: TCL_ALLOC_NATIVE
 *          NATIVE: use the native malloc and a per-thread Tcl_Obj pool, with 
 *          inter-thread recycling of objects. The per-thread pool can be
 *          disabled at startup with an env var, thus providing the PURIFY
 *          behaviour that is useful for valgrind and similar tools. Note that
 *          the PURIFY costs are negligible when disabled, but when enabled
 *          Tcl_Obj allocs will be even slower than in a full PURIFY build
 *          NOTE: the obj pool shares all code with zippy's smallest allocs!
 *                It does look overcomplicated for this particular case, but
 *                keeping them together allows simpler maintenance and avoids
 *                the need for separate debugging
 *          TODO: in this case build ZIPPY as a preloadable malloc-replacement           
 *
 *      (2) Build flags: TCL_ALLOC_ZIPPY
 *          ZIPPY: use the ex-tclThreadAlloc, essentially aolserver's
 *          fast threaded allocator. Mods with respect to the original:
 *            - change in the block sizes, so that the smallest alloc is
 *              Tcl_Obj-sized
 *            - share the Tcl_Obj pool with the smallest allocs pool for
 *              improved cache usage
 *            - split blocks in the shared pool before mallocing again for
 *              improved cache usage
 *            - ?change in the number of blocks to move to/from the shared
 *              cache: it used to be a fixed number, it is now computed
 *              to leave a fixed number in the thread's pool. This improves
 *              sharing behaviour when one thread uses a lot of memory once
 *              and rarely again (eg, at startup), at the cost of slowing
 *              slightly threads that allocate/free large numbers of blocks
 *              repeatedly
 *            - stats and Tcl_GetMemoryInfo disabled per default, enable with
 *              -DZIPPY_STATS
 *            - adapt for unthreaded usage as replacement of the ex tclAlloc
 *            - -DHAVE_FAST_TSD: use fast TSD via __thread where available
 *            - (TODO!) build zippy as a pre-loadable library to use with a
 *              native build as a malloc replacement. Difficulties are:
 *                  (a) make that portable (easy enough on modern elf/unix, to
 *                      be researched on win and mac)
 *                  (b) coordinate the Tcl_Obj pool and the smallest allocs,
 *                      as they are now addressed from different files. This
 *                      might require a special Tcl build with no
 *                      TclSmallAlloc, and a separate preloadable for use with
 *                      native builds? Or else separate them again, but that's
 *                      not really good I think.
 *
 *               NOTES:
 *                  . this would be the best option, instead of MULTI. It
 *                    could be built in two versions (perf, debug/stats)
 *                  . would a preloaded zippy be slower than builtin?
 *                    Possibly, due to extra indirection.
 *
 *      (3) Build flags: TCL_ALLOC_MULTI
 *          MULTI: all of the above, selectable at startup with an env
 *          var. This build will be very slightly slower than the specific
 *          builds above, but is completely portable: it does not depend on
 *          any help from the loader or such.
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

/*
 * This macro is used to properly align the memory allocated by Tcl, giving
 * the same alignment as the native malloc.
 */

#if defined(__APPLE__)
#define TCL_ALLOCALIGN	16
#else
#define TCL_ALLOCALIGN	(2*sizeof(void *))
#endif

#undef TclpAlloc
#undef TclpRealloc
#undef TclpFree
#undef TclSmallAlloc
#undef TclSmallFree

#if (TCL_ALLOCATOR == aNATIVE) || (TCL_ALLOCATOR == aPURIFY)
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

#endif /* end of common code for PURIFY and NATIVE*/

#if TCL_ALLOCATOR != aPURIFY
/*
 * The rest of this file deals with ZIPPY and MULTI builds, as well as the
 * Tcl_Obj pools for NATIVE
 */

/*
 * Note: we rely on the optimizer to remove unneeded code, instead of setting
 * up a maze of #ifdefs all over the code.
 * We should insure that debug builds do at least this much optimization, right?
 */

#if TCL_ALLOCATOR == aZIPPY
#   define allocator aZIPPY
#   define ALLOCATOR_BASE aZIPPY
#elif TCL_ALLOCATOR == aNATIVE
/* Keep the option to switch PURIFY mode on! */
static int allocator = aNONE;
#   define ALLOCATOR_BASE aNATIVE
#   define RCHECK 0
#   undef ZIPPY_STATS
#else
/* MULTI */
    static int allocator = aNONE;
#   define ALLOCATOR_BASE aZIPPY
#endif

#if TCL_ALLOCATOR != aZIPPY
static void ChooseAllocator();
#endif


/*
 * If range checking is enabled, an additional byte will be allocated to store
 * the magic number at the end of the requested memory.
 */

#ifndef RCHECK
#  ifdef  NDEBUG
#    define RCHECK		0
#  else
#    define RCHECK		1
#  endif
#endif

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
	    unsigned char unused;	/* Padding. */
	    unsigned char magic2;	/* Second magic number. */
	} s;
    } u;
    size_t reqSize;			/* Requested allocation size. */
} Block;

#define ALIGN(x)    (((x) + TCL_ALLOCALIGN - 1) & ~(TCL_ALLOCALIGN - 1))
#define OFFSET      ALIGN(sizeof(Block))

#define nextBlock	u.next
#define sourceBucket	u.s.bucket
#define magicNum1	u.s.magic1
#define magicNum2	u.s.magic2
#define MAGIC		0xEF

/*
 * The following defines the minimum and maximum block sizes and the number
 * of buckets in the bucket cache.
 *                        32b    64b    Apple-32b
 *     TCL_ALLOCALIGN       8     16       16
 *     sizeof(Block)        8     16       16
 *     OFFSET               8     16       16
 *     sizeof(Tcl_Obj)     24     48       24
 *     ALLOCBASE           24     48       24
 *     MINALLOC            24     48       24
 *     NBUCKETS            11     10       11
 *     MAXALLOC         24576  24576    24576
 *     small allocs      1024    512     1024
 *        at a time
 */

#if TCL_ALLOCATOR == aNATIVE
#define MINALLOC    MAX(OFFSET, sizeof(Tcl_Obj))
#else
#define MINALLOC    ALIGN(MAX(OFFSET+8, sizeof(Tcl_Obj)))
#endif

#define NBUCKETS    10 /* previously (11 - (MINALLOC >> 5)) */
#define MAXALLOC    (MINALLOC << (NBUCKETS - 1))

#if TCL_ALLOCATOR == aNATIVE
#  define NBUCKETS_0 1
#  define nBuckets   1
#else
#  define NBUCKETS_0 NBUCKETS
#  if TCL_ALLOCATOR == aZIPPY
#    define nBuckets NBUCKETS
#  else
     static int nBuckets = NBUCKETS;
#  endif
#endif

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
    long totalAssigned;		/* Total space assigned to bucket */
#endif
} Bucket;

/*
 * The following structure defines a cache of buckets, at most one per
 * thread. 
 */

typedef struct Cache {
#if defined(TCL_THREADS)
    struct Cache *nextPtr;	/* Linked list of cache entries */
#ifdef ZIPPY_STATS
    Tcl_ThreadId owner;		/* Which thread's cache is this? */
#endif
#endif
#ifdef ZIPPY_STATS
    int totalAssigned;		/* Total space assigned to thread */
#endif
    Bucket buckets[1];	        /* The buckets for this thread */
} Cache;


/*
 * The following array specifies various per-bucket limits and locks. The
 * values are statically initialized to avoid calculating them repeatedly.
 */

static struct {
    size_t blockSize;		/* Bucket blocksize. */
#if defined(TCL_THREADS)
    int maxBlocks;		/* Max blocks before move to share. */
    int numMove;		/* Num blocks to move to share. */
    Tcl_Mutex *lockPtr;		/* Share bucket lock. */
#endif
} bucketInfo[NBUCKETS_0];

/*
 * Static functions defined in this file.
 */

static Cache *	GetCache(void);
static int	GetBlocks(Cache *cachePtr, int bucket);
static inline Block *	Ptr2Block(char *ptr);
static inline char *	Block2Ptr(Block *blockPtr, int bucket, unsigned int reqSize);

#if defined(TCL_THREADS)

static Cache *firstCachePtr = NULL;
static Cache *sharedPtr = NULL;

static Tcl_Mutex *listLockPtr;
static Tcl_Mutex *objLockPtr;

static void	LockBucket(Cache *cachePtr, int bucket);
static void	UnlockBucket(Cache *cachePtr, int bucket);
static void	PutBlocks(Cache *cachePtr, int bucket, int numMove);

#if defined(HAVE_FAST_TSD)
static __thread Cache *tcachePtr;

# define GETCACHE(cachePtr)			\
    do {					\
	if (!tcachePtr) {			\
	    tcachePtr = GetCache();		\
	}					\
	(cachePtr) = tcachePtr;			\
    } while (0)
#else
# define GETCACHE(cachePtr)			\
    do {					\
	(cachePtr) = TclpGetAllocCache();	\
	if ((cachePtr) == NULL) {		\
	    (cachePtr) = GetCache();		\
	}					\
    } while (0)
#endif
#else /* NOT THREADS! */

#define TclpSetAllocCache()
#define PutBlocks(cachePtr, bucket, numMove) 
#define firstCachePtr sharedCachePtr

# define GETCACHE(cachePtr)			\
    do {					\
	if (!sharedPtr) {			\
	    GetCache();				\
	}					\
	(cachePtr) = sharedPtr;			\
    } while (0)

static void *
TclpGetAllocCache(void)
{
    if (!allocInitialized) {
	allocInitialized = 1;
	GetCache();
    }
    return sharedPtr;
}
#endif


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
    blockPtr->sourceBucket = bucket;
    blockPtr->reqSize = reqSize;
    ptr = (void *) (((char *)blockPtr) + OFFSET);
#if RCHECK
    ((unsigned char *)(ptr))[reqSize] = MAGIC;
#endif
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
#if RCHECK
    if (((unsigned char *) ptr)[blockPtr->reqSize] != MAGIC) {
	Tcl_Panic("alloc: invalid block: %p: %x %x %x",
		blockPtr, blockPtr->magicNum1, blockPtr->magicNum2,
		((unsigned char *) ptr)[blockPtr->reqSize]);
    }
#endif
    return blockPtr;
}

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

static Cache *
GetCache(void)
{
    Cache *cachePtr;
    unsigned int i;
#if TCL_ALLOCATOR == aZIPPY
#define allocSize (sizeof(Cache) + (NBUCKETS -1)*sizeof(Bucket))
#elif TCL_ALLOCATOR == aNATIVE
#define allocSize sizeof(Cache)
#else
    unsigned int allocSize;
#endif
    
    /*
     * Set the params for the correct allocator
     */
    
#if TCL_ALLOCATOR != aZIPPY
    if (allocator == aNONE) {
	/* This insures that it is set just once, as any changes after
	 * initialization guarantee a hard crash
	 */
	
	ChooseAllocator();
    }

#if TCL_ALLOCATOR == aMULTI
    if (allocator == aZIPPY) {
	allocSize = (sizeof(Cache) + (NBUCKETS -1)*sizeof(Bucket));
	nBuckets = NBUCKETS;
    } else {
	allocSize = sizeof(Cache);
	nBuckets = 1;
    }
#endif
#endif

    /*
     * Check for first-time initialization.
     */

#if defined(TCL_THREADS)
    if (listLockPtr == NULL) {
	Tcl_Mutex *initLockPtr;
	initLockPtr = Tcl_GetAllocMutex();
	Tcl_MutexLock(initLockPtr);
	if (listLockPtr == NULL) {
	    listLockPtr = TclpNewAllocMutex();
	    objLockPtr = TclpNewAllocMutex();
#endif
	    for (i = 0; i < nBuckets; ++i) {
		bucketInfo[i].blockSize = MINALLOC << i;
#if defined(TCL_THREADS)
		/* TODO: clearer logic? Change move to keep? */
		bucketInfo[i].maxBlocks = 1 << (NBUCKETS - 1 - i);
		bucketInfo[i].numMove = i < NBUCKETS - 1 ?
			1 << (NBUCKETS - 2 - i) : 1;
		bucketInfo[i].lockPtr = TclpNewAllocMutex();
#endif
	    }
#if defined(TCL_THREADS)
	    sharedPtr = calloc(1, allocSize);
	    firstCachePtr = sharedPtr;
	}
	Tcl_MutexUnlock(initLockPtr);
    }
#endif

    if (allocator == aPURIFY) {
	bucketInfo[0].maxBlocks = 0;
    }
    
    /*
     * Get this thread's cache, allocating if necessary.
     */

    cachePtr = TclpGetAllocCache();
    if (cachePtr == NULL) {
	cachePtr = calloc(1, allocSize);
	if (cachePtr == NULL) {
	    Tcl_Panic("alloc: could not allocate new cache");
	}
#if defined(TCL_THREADS)
	Tcl_MutexLock(listLockPtr);
	cachePtr->nextPtr = firstCachePtr;
	firstCachePtr = cachePtr;
	Tcl_MutexUnlock(listLockPtr);
#ifdef ZIPPY_STATS
	cachePtr->owner = Tcl_GetCurrentThread();
#endif
	TclpSetAllocCache(cachePtr);
#endif
    }
    return cachePtr;
}

#if defined(TCL_THREADS)
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

void
TclFreeAllocCache(
    void *arg)
{
    Cache *cachePtr = arg;
    Cache **nextPtrPtr;
    register unsigned int bucket;

    /*
     * Flush blocks.
     */

    for (bucket = 0; bucket < nBuckets; ++bucket) {
	if (cachePtr->buckets[bucket].numFree > 0) {
	    PutBlocks(cachePtr, bucket, cachePtr->buckets[bucket].numFree);
	}
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

#if TCL_ALLOCATOR != aNATIVE
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

    if (allocator < aNONE) {
	return (void *) malloc(reqSize);
    }
    
    GETCACHE(cachePtr);

#ifndef __LP64__
    if (sizeof(int) >= sizeof(size_t)) {
	/* An unsigned int overflow can also be a size_t overflow */
	const size_t zero = 0;
	const size_t max = ~zero;

	if (((size_t) reqSize) > max - OFFSET - RCHECK) {
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
#if RCHECK
    size++;
#endif
    if (size > MAXALLOC) {
	bucket = nBuckets;
	blockPtr = malloc(size);
#ifdef ZIPPY_STATS
	if (blockPtr != NULL) {
	    cachePtr->totalAssigned += reqSize;
	}
#endif
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
	    cachePtr->buckets[bucket].totalAssigned += reqSize;
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

    if (ptr == NULL) {
	return;
    }

    if (allocator < aNONE) {
	return free((char *) ptr);
    }

#ifdef ZIPPY_STATS
    GETCACHE(cachePtr);
#endif

    /*
     * Get the block back from the user pointer and call system free directly
     * for large blocks. Otherwise, push the block back on the bucket and move
     * blocks to the shared cache if there are now too many free.
     */

    blockPtr = Ptr2Block(ptr);
    bucket = blockPtr->sourceBucket;
    if (bucket == nBuckets) {
#ifdef ZIPPY_STATS
	cachePtr->totalAssigned -= blockPtr->reqSize;
#endif
	free(blockPtr);
	return;
    }

#ifndef ZIPPY_STATS
    GETCACHE(cachePtr);
#endif

#ifdef ZIPPY_STATS
    cachePtr->buckets[bucket].totalAssigned -= blockPtr->reqSize;
#endif
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
    Cache *cachePtr;
    Block *blockPtr;
    void *newPtr;
    size_t size, min;
    int bucket;

    if (allocator < aNONE) {
	return (void *) realloc((char *) ptr, reqSize);
    }

    GETCACHE(cachePtr);

    if (ptr == NULL) {
	return TclpAlloc(reqSize);
    }

#ifndef __LP64__
    if (sizeof(int) >= sizeof(size_t)) {
	/* An unsigned int overflow can also be a size_t overflow */
	const size_t zero = 0;
	const size_t max = ~zero;

	if (((size_t) reqSize) > max - OFFSET - RCHECK) {
	    /* Requested allocation exceeds memory */
	    return NULL;
	}
    }
#endif

    /*
     * If the block is not a system block and fits in place, simply return the
     * existing pointer. Otherwise, if the block is a system block and the new
     * size would also require a system block, call realloc() directly.
     */

    blockPtr = Ptr2Block(ptr);
    size = reqSize + OFFSET;
#if RCHECK
    size++;
#endif
    bucket = blockPtr->sourceBucket;
    if (bucket != nBuckets) {
	if (bucket > 0) {
	    min = bucketInfo[bucket-1].blockSize;
	} else {
	    min = 0;
	}
	if (size > min && size <= bucketInfo[bucket].blockSize) {
#ifdef ZIPPY_STATS
	    cachePtr->buckets[bucket].totalAssigned -= blockPtr->reqSize;
	    cachePtr->buckets[bucket].totalAssigned += reqSize;
#endif
	    return Block2Ptr(blockPtr, bucket, reqSize);
	}
    } else if (size > MAXALLOC) {
#ifdef ZIPPY_STATS
	cachePtr->totalAssigned -= blockPtr->reqSize;
	cachePtr->totalAssigned += reqSize;
#endif
	blockPtr = realloc(blockPtr, size);
	if (blockPtr == NULL) {
	    return NULL;
	}
	return Block2Ptr(blockPtr, nBuckets, reqSize);
    }

    /*
     * Finally, perform an expensive malloc/copy/free.
     */

    newPtr = TclpAlloc(reqSize);
    if (newPtr != NULL) {
	if (reqSize > blockPtr->reqSize) {
	    reqSize = blockPtr->reqSize;
	}
	memcpy(newPtr, ptr, reqSize);
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
    size_t oldSize, newSize;

    if (allocator < aNONE) {
	/*
	 * No info, return UINT_MAX as a signal.
	 */
	
	return UINT_MAX;
    }
    
    blockPtr = Ptr2Block(ptr);
    bucket = blockPtr->sourceBucket;
    
    if (bucket == nBuckets) {
	/*
	 * System malloc'ed: no info
	 */
	
	return UINT_MAX;
    }

    oldSize = blockPtr->reqSize;
    newSize = bucketInfo[bucket].blockSize - OFFSET - RCHECK;
    blockPtr->reqSize = newSize;
#if RCHECK
    ((unsigned char *)(ptr))[newSize] = MAGIC;
#endif
#ifdef ZIPPY_STATS
    {
	Cache *cachePtr;
	GETCACHE(cachePtr);
	cachePtr->buckets[bucket].totalAssigned += (newSize - oldSize);
    }
#endif
    return newSize;
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
	for (n = 0; n < nBuckets; ++n) {
	    sprintf(buf, "%lu %ld %ld %ld %ld %ld %ld",
		    (unsigned long) bucketInfo[n].blockSize,
		    cachePtr->buckets[n].numFree,
		    cachePtr->buckets[n].numRemoves,
		    cachePtr->buckets[n].numInserts,
		    cachePtr->buckets[n].totalAssigned,
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
#endif /* code above only for NATIVE allocator */

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
    Cache *cachePtr;
    Block *blockPtr;
    Bucket *bucketPtr;

    GETCACHE(cachePtr);
    bucketPtr = &cachePtr->buckets[0];

    blockPtr = bucketPtr->firstPtr;
    if (bucketPtr->numFree || GetBlocks(cachePtr, 0)) {
        blockPtr = bucketPtr->firstPtr;
 	bucketPtr->firstPtr = blockPtr->nextBlock;
 	bucketPtr->numFree--;
#ifdef ZIPPY_STATS
 	bucketPtr->numRemoves++;
 	bucketPtr->totalAssigned += sizeof(Tcl_Obj);
#endif
    }
    return blockPtr;
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
    Block *blockPtr = ptr;
    Bucket *bucketPtr;

    GETCACHE(cachePtr);
    bucketPtr = &cachePtr->buckets[0];

#ifdef ZIPPY_STATS
    bucketPtr->totalAssigned -= sizeof(Tcl_Obj);
#endif
    blockPtr->nextBlock = bucketPtr->firstPtr;
    bucketPtr->firstPtr = blockPtr;
    bucketPtr->numFree++;
#ifdef ZIPPY_STATS
    bucketPtr->numInserts++;
#endif
    
    if (bucketPtr->numFree > bucketInfo[0].maxBlocks) {
	if (allocator == aPURIFY) {
	    /* undo */
	    bucketPtr->numFree = 0;
	    bucketPtr->firstPtr = NULL;
	    free((char *) blockPtr);
	    return;
	}
#if defined(TCL_THREADS)
	PutBlocks(cachePtr, 0, bucketInfo[0].numMove);
#endif
    }
}

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

    if (allocator == aPURIFY) {
	if (bucket) {
	    Tcl_Panic("purify mode asking for blocks?");
	}
	cachePtr->buckets[0].firstPtr = (Block *) calloc(1, MINALLOC);
	cachePtr->buckets[0].numFree = 1;
	return 1;
    }

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

#if TCL_ALLOCATOR != aNATIVE
	if (allocator == aZIPPY) {
	    /*
	     * If no blocks could be moved from shared, first look for a larger
	     * block in this cache OR the shared cache to split up.
	     */
	    
	    n = nBuckets;
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
		n = nBuckets;
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

void
TclInitAlloc(void)
{
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
    unsigned int i;

    for (i = 0; i < nBuckets; ++i) {
	TclpFreeAllocMutex(bucketInfo[i].lockPtr);
	bucketInfo[i].lockPtr = NULL;
    }

    TclpFreeAllocMutex(objLockPtr);
    objLockPtr = NULL;

    TclpFreeAllocMutex(listLockPtr);
    listLockPtr = NULL;

    TclpFreeAllocCache(NULL);
#endif
}

#if TCL_ALLOCATOR != aZIPPY
static void
ChooseAllocator()
{
    char *choice = getenv("TCL_ALLOCATOR");

    /*
     * This is only called with ALLOCATOR_BASE aZIPPY (when compiled with 
     * aMULTI) or aNATIVE (when compiled with aNATIVE).
     */
    
    allocator = ALLOCATOR_BASE;

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

#endif /* end of !PURIFY */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
