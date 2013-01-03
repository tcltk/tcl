/*
 * tclAllocZippy.c --
 *
 *      This is a very flexible storage allocator for Tcl, for use with or
 *      without threads.
 *
 *      It is essentially the ex-tclThreadAlloc, aolserver's fast threaded
 *      allocator. Mods with respect to the original:
 *            - it is split into two: the freeObj list is part of Tcl itself,
 *              in tclAlloc.c, and the malloc() part is here
 *            - split blocks in the shared pool before mallocing again for
 *              improved cache usage
 *            - stats and Tcl_GetMemoryInfo are gone
 *            - adapt for unthreaded usage as replacement of the ex tclAlloc
 *            - (TODO!) build zippy as a pre-loadable library to use with a
 *              native build as a malloc replacement. Difficulty is to make it
 *              portable (easy enough on modern elf/unix, to be researched on
 *              win and mac . This would be the best option, instead of
 *              MULTI. It could be built in two versions (perf, debug/stats)
 *
 * The Initial Developer of the Original Code is America Online, Inc.
 * Portions created by AOL are Copyright (C) 1999 America Online, Inc.
 *
 * Copyright (c) 2008-2013 by Miguel Sofer. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclAlloc.h"

#undef TclpAlloc
#undef TclpRealloc
#undef TclpFree

/*
 * The following struct stores accounting information for each block including
 * two small magic numbers and a bucket number when in use or a next pointer
 * when free. The original requested size (not including the Block overhead)
 * is also maintained.
 */

/*
 * The following union stores accounting information for each block including
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
	    unsigned char inUse;	/* Block memory currently in use, used
					 * by realloc. */
	    unsigned char magic2;	/* Second magic number. */
	} s;
    } u;
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
 *     ALLOCALIGN           8     16       16
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
} Bucket;

/*
 * The following array specifies various per-bucket limits and locks. The
 * values are statically initialized to avoid calculating them repeatedly.
 */

static struct {
    size_t blockSize;		/* Bucket blocksize. */
    int shift;
    int maxBlocks;		/* Max blocks before move to share. */
    int numMove;		/* Num blocks to move to share. */
    Tcl_Mutex *lockPtr;		/* Share bucket lock. */
} bucketInfo[NBUCKETS];

/*
 * The Tcl_Obj per-thread cache, used by aNATIVE, aZIPPY and aMULTI.
 */

typedef struct Cache {
    Bucket buckets[NBUCKETS];	/* The buckets for this thread */
} Cache;

static Cache sharedCache;
#define sharedPtr (&sharedCache)

static void InitBucketInfo(void);
static inline char * Block2Ptr(Block *blockPtr,
	int bucket, unsigned int reqSize);
static inline Block * Ptr2Block(char *ptr);

static int  GetBlocks(Cache *cachePtr, int bucket);

static void PutBlocks(Cache *cachePtr, int bucket, int numMove);
static inline void LockBucket(int bucket);
static inline void UnlockBucket(int bucket);


/*
 *-------------------------------------------------------------------------
 *
 * TclXpInitAlloc --
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

	if (TCL_THREADED) {
	    /* TODO: clearer logic? Change move to keep? */
	    bucketInfo[i].maxBlocks = 1 << (NBUCKETS - 1 - i);
	    bucketInfo[i].numMove = i < NBUCKETS - 1 ?
		1 << (NBUCKETS - 2 - i) : 1;
	    bucketInfo[i].lockPtr = TclpNewAllocMutex();
	}
    }
}

void
TclXpInitAlloc(void)
{
    /*
     * Set the params for the correct allocator
     */

    if (TCL_THREADED) {
	InitBucketInfo();
	TclSetSharedAllocCache(sharedPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclXpFinalizeAlloc --
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
TclXpFinalizeAlloc(void)
{

    if (TCL_THREADED) {
	unsigned int i;

	for (i = 0; i < NBUCKETS; ++i) {
	    TclpFreeAllocMutex(bucketInfo[i].lockPtr);
	    bucketInfo[i].lockPtr = NULL;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclXpFreeAllocCache --
 *
 *	Flush and delete a cache
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
TclXpFreeAllocCache(
    void *arg)
{
    Cache *cachePtr = arg;

    register unsigned int bucket;

    /*
     * Flush blocks.
     */

    for (bucket = 0; bucket < NBUCKETS; ++bucket) {
	if (cachePtr->buckets[bucket].numFree > 0) {
	    PutBlocks(cachePtr, bucket, cachePtr->buckets[bucket].numFree);
	}
    }
    free(cachePtr);
}

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

static inline Cache *
GetAllocCache(void)
{
    Cache *cachePtr = TclGetAllocCache();
    if (cachePtr == NULL) {
	cachePtr = calloc(1, sizeof(Cache));			
	if (cachePtr == NULL) {					
	    Tcl_Panic("alloc: could not allocate new cache");	
	}								
	TclSetAllocCache(cachePtr);					
    }
    return cachePtr;
}


char *
TclpAlloc(
    unsigned int reqSize)
{
    Cache *cachePtr;
    Block *blockPtr;
    register int bucket;
    size_t size;

    if (TCL_PURIFY) {
	return (void *) malloc(reqSize);
    }

    cachePtr = GetAllocCache();

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

    if (TCL_PURIFY) {
	return free((char *) ptr);
    }
    
    if (ptr == NULL) {
	return;
    }

    blockPtr = Ptr2Block(ptr);
    
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

    cachePtr = GetAllocCache();

    blockPtr->nextBlock = cachePtr->buckets[bucket].firstPtr;
    cachePtr->buckets[bucket].firstPtr = blockPtr;
    cachePtr->buckets[bucket].numFree++;
    if (cachePtr != sharedPtr &&
	    cachePtr->buckets[bucket].numFree > bucketInfo[bucket].maxBlocks) {
	PutBlocks(cachePtr, bucket, bucketInfo[bucket].numMove);
    }
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

    if (TCL_PURIFY) {
	return (void *) realloc((char *) ptr, reqSize);
    }
    
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

static inline void
LockBucket(
    int bucket)
{
    Tcl_MutexLock(bucketInfo[bucket].lockPtr);
}

static inline void
UnlockBucket(
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

    LockBucket(bucket);
    lastPtr->nextBlock = sharedPtr->buckets[bucket].firstPtr;
    sharedPtr->buckets[bucket].firstPtr = firstPtr;
    sharedPtr->buckets[bucket].numFree += numMove;
    UnlockBucket(bucket);
}

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

    /*
     * First, atttempt to move blocks from the shared cache. Note the
     * potentially dirty read of numFree before acquiring the lock which is a
     * slight performance enhancement. The value is verified after the lock is
     * actually acquired.
     */

    if (cachePtr != sharedPtr && sharedPtr->buckets[bucket].numFree > 0) {
	LockBucket(bucket);
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
	UnlockBucket(bucket);
    }
    
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
	if (blockPtr == NULL) {
	    n = NBUCKETS;
	    size = 0; /* lint */
	    while (--n > bucket) {
		if (sharedPtr->buckets[n].numFree > 0) {
		    size = bucketInfo[n].blockSize;
		    LockBucket(n);
		    if (sharedPtr->buckets[n].numFree > 0) {
			blockPtr = sharedPtr->buckets[n].firstPtr;
			sharedPtr->buckets[n].firstPtr = blockPtr->nextBlock;
			sharedPtr->buckets[n].numFree--;
			UnlockBucket(n);
			break;		
		    }
		    UnlockBucket(n);
		}
	    }
	}
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
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
