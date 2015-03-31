/*
 * tclHAMT.c --
 *
 *	This file contains an implementation of a hash array mapped trie
 *	(HAMT).  In the first draft, it is just an alternative hash table
 *	implementation, but later revisions may support concurrency much
 *	better.
 *
 * Contributions from Don Porter, NIST, 2015. (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclHAMT.h"
#include <assert.h>

#if defined(HAVE_INTRIN_H)
#    include <intrin.h>
#endif

/* These are values for 64-bit size_t */
#define LEAF_SHIFT 4
#define BRANCH_SHIFT 6
/* Alternate values for 32-bit:
#define LEAF_SHIFT 2
#define BRANCH_SHIFT 5
*/

#define LEAF_MASK ~(((size_t)1 << (LEAF_SHIFT - 1)) - 1)
#define BRANCH_MASK (((size_t)1<<BRANCH_SHIFT) - 1)

typedef struct ArrayMap {
    size_t	mask;
    size_t	id;
    size_t	map;
    void *	children;
} ArrayMap;

typedef struct KeyValue {
    struct KeyValue	*nextPtr;
    ClientData		key;
    ClientData 		value;
} KeyValue;

#define AM_SIZE(numChildren) \
	(sizeof(ArrayMap) + ((numChildren) - 1) * sizeof(void *))

typedef struct HAMT {
    const TclHAMTKeyType *keyTypePtr;	/* Custom key handling functions */
    const TclHAMTValType *valTypePtr;	/* Custom value handling functions */
    ArrayMap		 *amPtr;	/* Top level array map */
} HAMT;

static ArrayMap *	MakeLeafMap(const size_t hash,
			    const TclHAMTKeyType *ktPtr, const ClientData key,
			    const TclHAMTValType *vtPtr,
			    const ClientData value);
static inline int	NumBits(size_t value);
static ArrayMap *	GetSet(ArrayMap *amPtr, const size_t *hashPtr,
			    const TclHAMTKeyType *ktPtr, const ClientData key,
			    const TclHAMTValType *vtPtr,
			    const ClientData value, ClientData *valuePtr);
static KeyValue *	MakeKeyValue(const TclHAMTKeyType *ktPtr,
			    const ClientData key, const TclHAMTValType *vtPtr,
			    const ClientData value);
static void		DeleteKeyValue(const TclHAMTKeyType *ktPtr,
			    const TclHAMTValType *vtPtr, KeyValue *kvPtr);


/*
 *----------------------------------------------------------------------
 *
 * NumBits --
 *
 * Results:
 *	Number of set bits (aka Hamming weight, aka population count) in
 *	a size_t value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static inline int
NumBits(
    size_t value)
{
#if defined(__GNUC__) && ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2))
        return __builtin_popcountll((long long)value);
#else
#error  NumBits not implemented!
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * GetSet --
 *
 *	This is the central trie-traversing routine that is the core of
 *	all insert, delete, and fetch operations on a HAMT.  Look in
 *	the ArrayMap indicated by amPtr for the key.  The operation to
 *	perform is encoded in the values of value and valuePtr.  When
 *	both are NULL, we are to delete anything stored under the key.
 *	When value is NULL, but valuePtr is non-NULL, we are to fetch
 *	the value associated with key and write it to *valuePtr.  When
 *	value is non-NULL, we are to store it associated with the key.
 *	If valuePtr is also non-NULL, we write to it any old value that
 *	was associated with the key that we are now overwriting.  Whenever
 *	valuePtr is non-NULL and the key is not in the ArrayMap at the
 *	start of the operation, a NULL value is written to *valuePtr.
 *
 * Results:
 *	Pointer to the ArrayMap -- possibly revised -- after the requested
 *	operation is complete.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static ArrayMap *
GetSet(
    ArrayMap *amPtr,
    const size_t *hashPtr,
    const TclHAMTKeyType *ktPtr,
    const ClientData key,
    const TclHAMTValType *vtPtr,
    const ClientData value,
    ClientData *valuePtr)
{
    size_t hash;

    if (amPtr == NULL) {
	/* Empty array map */

	if (valuePtr != NULL) {
	    *valuePtr = NULL;
	}
	if (value == NULL) {
	    return amPtr;
	}
    }

    if (hashPtr) {
	hash = *hashPtr;
    } else {
	if (ktPtr && ktPtr->hashProc) {
	    hash = ktPtr->hashProc(key);
	} else {
	    hash = (size_t) key;
	}
	hashPtr = &hash;
    }

    if (amPtr == NULL) {
	return MakeLeafMap(hash, ktPtr, key, vtPtr, value);
    }

    if ((hash & (amPtr->mask << 1)) != amPtr->id) {
	/* Key doesn't belong in this array map */

	if (valuePtr != NULL) {
	    *valuePtr = NULL;
	}
	if (value == NULL) {
	    return amPtr;
	} else {
	    /* Make the map where the key does belong */
	    ArrayMap *newPtr = MakeLeafMap(hash, ktPtr, key, vtPtr, value);

	    /* Then connect it up to amPtr; Need common parent. */
	    ArrayMap *parentPtr = ckalloc(AM_SIZE(2));
	    ArrayMap **child = (ArrayMap **) &(parentPtr->children);
	    size_t mask = (~(
		    (1 <<
			(( (TclMSB(hash ^ amPtr->id) - LEAF_SHIFT)
			/ BRANCH_SHIFT) * BRANCH_SHIFT)
		    ) - 1)) << (LEAF_SHIFT - 1);
	    int shift = TclMSB(~mask) - LEAF_SHIFT;
	    size_t id = hash & (parentPtr->mask << 1);

	    assert(id == (mask << 1) && amPtr->id);

	    if (newPtr->id < amPtr->id) {
		child[0] = newPtr;
		child[1] = amPtr;
	    } else {
		child[0] = amPtr;
		child[1] = newPtr;
	    }

	    parentPtr->mask = mask;
	    parentPtr->id = id;
	    parentPtr->map = ((size_t)1 << ((amPtr->id >> shift)
		    & BRANCH_MASK)) | ((size_t)1 << ((newPtr->id >> shift)
		    & BRANCH_MASK));

	    return parentPtr;
	}
    }

    /* hash & (amPtr->mask << 1) == amPtr->id */
    /* Key goes into this array map ...*/

    if (amPtr->mask == LEAF_MASK) {
	/* ... and this is a leaf array map */
	KeyValue **src = (KeyValue **)&(amPtr->children);
	int size = NumBits(amPtr->map);
	int slot = hash & (~(LEAF_MASK << 1));
	size_t tally = (size_t)1 << slot;
	int idx = NumBits(amPtr->map & (tally - 1));

	if (tally & amPtr->map) {
	    /* Slot is already occupied.  Hash is right, but must check key. */
	    KeyValue *kvPtr = src[idx];
	    do {
		if (ktPtr && ktPtr->isEqualProc) {
		    if (ktPtr->isEqualProc(key, kvPtr->key)) {
			break;
		    }
		} else {
		    if (key == kvPtr->key) {
			break;
		    }
		}
		kvPtr = kvPtr->nextPtr;
	    } while (kvPtr);

	    if (kvPtr) {
		/* The key matches. */
		if (valuePtr != NULL) {
		    if (vtPtr && vtPtr->makeRefProc) {
			*valuePtr = vtPtr->makeRefProc(kvPtr->value);
		    } else {
			*valuePtr = kvPtr->value;
		    }
		}
		if (value == NULL) {
		    if (valuePtr != NULL) {
			/* No destructive fetch */
			return amPtr;
		    }

		    if (src[idx] == kvPtr) {
			src[idx] = kvPtr->nextPtr;
		    } else {
			KeyValue *ptr = src[idx];
			while (ptr->nextPtr != kvPtr) {
			    ptr = ptr->nextPtr;
			}
			ptr->nextPtr = kvPtr->nextPtr;
		    }
		    DeleteKeyValue(ktPtr, vtPtr, kvPtr);

		    if (src[idx]) {
			/* TODO: Persistence */
			return amPtr;
		    } else {
			ArrayMap *shrinkPtr = ckalloc(AM_SIZE(size - 1));
			KeyValue **dst = (KeyValue **)&(shrinkPtr->children);

			memcpy(src, dst, idx*sizeof(KeyValue *));
			memcpy(src+idx+1, dst+idx,
				(size - idx -1)*sizeof(KeyValue *));

			ckfree(amPtr);
			return shrinkPtr;
		    }

		} else {
		    /* Overwrite insertion */ 
		    if (vtPtr && vtPtr->dropRefProc) {
			vtPtr->dropRefProc(kvPtr->value);
		    }
		    if (vtPtr && vtPtr->makeRefProc) {
			kvPtr->value = vtPtr->makeRefProc(value);
		    } else {
			kvPtr->value = value;
		    }
		    /* TODO: Persistence! */
		    return amPtr;
		}
	    } else {
		if (valuePtr != NULL) {
		    *valuePtr = NULL;
		}
		if (value == NULL) {
		    return amPtr;
		} else {
		    /* Insert colliding key */
		    KeyValue *newPtr = MakeKeyValue(ktPtr, key, vtPtr, value);

		    newPtr->nextPtr = src[idx];
		    src[idx] = newPtr;
		    /* TODO: Persistence! */
		    return amPtr;
		}
	    }
	} else {
	    /* Slot is empty */

	    if (valuePtr != NULL) {
		*valuePtr = NULL;
	    }
	    if (value == NULL) {
		return amPtr;
	    } else {
		ArrayMap *growPtr = ckalloc(AM_SIZE(size + 1));
		KeyValue **dst = (KeyValue **)&(growPtr->children);

		memcpy(src, dst, idx*sizeof(KeyValue *));
		dst[idx] = MakeKeyValue(ktPtr, key, vtPtr, value);
		memcpy(src+idx, dst+idx+1, (size-idx)*sizeof(KeyValue *));

		ckfree(amPtr);
		return growPtr;
	    }
	}
    } else {
	/* ... and this is a branch array map */
	ArrayMap **src = (ArrayMap **)&(amPtr->children);
	int shift = TclMSB(~amPtr->mask) - LEAF_SHIFT;
	int slot = (hash >> shift) & BRANCH_MASK;
	size_t tally = (size_t)1 << slot;
	int idx = NumBits(amPtr->map & (tally - 1));

	if (tally & amPtr->map) {
	    /* Slot is already occupied. */

	    /* TODO: Persistence */
	    src[idx] = GetSet(src[idx], hashPtr, ktPtr, key,
		    vtPtr, value, valuePtr);
	    return amPtr;
	} else {
	    int size = NumBits(amPtr->map);
	    ArrayMap *growPtr = ckalloc(AM_SIZE(size + 1));
	    ArrayMap **dst = (ArrayMap **)&(growPtr->children);

	    memcpy(src, dst, idx*sizeof(KeyValue *));
	    dst[idx] = GetSet(NULL, hashPtr, ktPtr, key,
		    vtPtr, value, valuePtr);
	    memcpy(src+idx, dst+idx+1, (size-idx)*sizeof(KeyValue *));

	    ckfree(amPtr);
	    return growPtr;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTRemove--
 *
 * Results:
 *	New revised TclHAMT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclHAMT
TclHAMTRemove(
    TclHAMT hamt,
    ClientData key,
    ClientData *valuePtr)
{
    HAMT *hamtPtr = hamt;
    ClientData value;

    hamtPtr->amPtr = GetSet(hamtPtr->amPtr, NULL,
	    hamtPtr->keyTypePtr, key,
	    hamtPtr->valTypePtr, NULL, &value);
    hamtPtr->amPtr = GetSet(hamtPtr->amPtr, NULL,
	    hamtPtr->keyTypePtr, key,
	    hamtPtr->valTypePtr, NULL, NULL);
    if (valuePtr) {
	*valuePtr = value;
    }
    return hamtPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTFetch --
 *
 * Results:
 *	New revised TclHAMT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

ClientData
TclHAMTFetch(
    TclHAMT hamt,
    ClientData key)
{
    HAMT *hamtPtr = hamt;
    ClientData value;

    hamtPtr->amPtr = GetSet(hamtPtr->amPtr, NULL,
	    hamtPtr->keyTypePtr, key,
	    hamtPtr->valTypePtr, NULL, &value);
    return value;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTInsert--
 *
 * Results:
 *	New revised TclHAMT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclHAMT
TclHAMTInsert(
    TclHAMT hamt,
    ClientData key,
    ClientData value,
    ClientData *valuePtr)
{
    HAMT *hamtPtr = hamt;

    /* TODO: Persistence */
    hamtPtr->amPtr = GetSet(hamtPtr->amPtr, NULL,
	    hamtPtr->keyTypePtr, key,
	    hamtPtr->valTypePtr, value, valuePtr);
    return hamtPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTCreate --
 *
 *	Create and return a new empty TclHAMT, with key operations 
 *	governed by the TclHAMTType struct pointed to by hktPtr.
 *
 * Results:
 *	A new empty TclHAMT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclHAMT
TclHAMTCreate(
    const TclHAMTKeyType *ktPtr,	/* Custom key handling functions */
    const TclHAMTValType *vtPtr)	/* Custom value handling functions */
{
    HAMT *hamtPtr = ckalloc(sizeof(HAMT));

    hamtPtr->keyTypePtr = ktPtr;
    hamtPtr->valTypePtr = vtPtr;
    hamtPtr->amPtr = NULL;
    return hamtPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeLeafMap --
 *
 *	Make an ArrayMap that sits among the leaves of the tree.  Make
 *	the leaf suitable for the hash value, and create, store and
 *	return a pointer to a new KeyValue to store key.
 *
 * Results:
 *	Pointer to the new leaf ArrayMap.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static ArrayMap *
MakeLeafMap(
    const size_t hash,
    const TclHAMTKeyType *ktPtr,
    const ClientData key,
    const TclHAMTValType *vtPtr,
    const ClientData value)
{
    ArrayMap *amPtr = ckalloc(AM_SIZE(1));

    amPtr->mask = LEAF_MASK;
    amPtr->id = hash & (LEAF_MASK << 1);
    amPtr->map = 1 << (hash & ~(LEAF_MASK << 1));

    /* child[0] */
    amPtr->children = MakeKeyValue(ktPtr, key, vtPtr, value);
    return amPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeKeyValue --
 *
 *	Make a KeyValue struct to hold key value pair.
 *
 * Results:
 *	Pointer to the new KeyValue struct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static KeyValue *
MakeKeyValue(
    const TclHAMTKeyType *ktPtr,
    const ClientData key,
    const TclHAMTValType *vtPtr,
    const ClientData value)
{
    KeyValue *kvPtr = ckalloc(sizeof(KeyValue));

    kvPtr->nextPtr = NULL;
    if (ktPtr && ktPtr->makeRefProc) {
	kvPtr->key = ktPtr->makeRefProc(key);		
    } else {
	kvPtr->key = key;		
    }
    if (vtPtr && vtPtr->makeRefProc) {
	kvPtr->value = vtPtr->makeRefProc(value);
    } else {
	kvPtr->value = value;
    }
    return kvPtr;
}

static void
DeleteKeyValue(
    const TclHAMTKeyType *ktPtr,
    const TclHAMTValType *vtPtr,
    KeyValue *kvPtr)
{
    if (ktPtr && ktPtr->dropRefProc) {
	ktPtr->dropRefProc(kvPtr->key);		
    }
    if (vtPtr && vtPtr->dropRefProc) {
	vtPtr->dropRefProc(kvPtr->value);
    }
    ckfree(kvPtr);
}
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
