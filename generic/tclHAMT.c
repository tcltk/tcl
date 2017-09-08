/*
 * tclHAMT.c --
 *
 *	This file contains an implementation of a hash array mapped trie
 *	(HAMT).  In the first draft, it is just an alternative hash table
 *	implementation, but later revisions may support concurrency much
 *	better.
 *
 * Contributions from Don Porter, NIST, 2015-2017. (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclHAMT.h"
#include <assert.h>

#if defined(HAVE_INTRIN_H)
#    include <intrin.h>
#endif

/*
 * All of our key/value pairs are to be stored in persistent lists, where
 * all the keys in a list produce the same hash value.  Given a quality
 * hash function, these lists should almost always hold a single key/value
 * pair.  For the rare case when we experience a hash collision, though, we
 * have to be prepared to make lists of arbitrary length.
 *
 * For the most part these are pretty standard linked lists.  The only
 * tricky things are that in anyy one list we will store at most one
 * key from each equivalence class of keys, as determined by the key type,
 * and we keep the collection of all lists, persistent and immutable, each 
 * until nothing has an interest in it any longer.  This means that distinct
 * lists can have common tails. The interest is maintained by
 * a claim count.  The current implementation of the claim mechanism
 * makes the overall structure suitable for only single-threaded operations.
 * Later refinements in development are intended to ease this constraint.
 */

typedef struct KVNode *KVList;

typeder struct KVNode {
    size_t	claim;	/* How many claims on this struct */
    KVList	tail;	/* The part of the list(s) following this pair */
    ClientData	key;	/* Key... */
    ClientData	value;	/* ...and Value of this pair */
} KVNode;

/*
 * The operations on a KVList:
 *	KVLClaim	Make a claim on the list.
 *	KVLDisclaim	Release a claim on the list.
 *	KVLFind		Find the tail starting with an equal key.
 *	KVLInsert	Create a new list, inserting new pair into old list.
 *	KVLRemove	Create a new list, with any pair matching key removed.
 */

static
void KVLClaim(
    KVList l)
{
    if (l != NULL) {
	l->claim++;
    }
}

static
void KVLDisclaim(
    KVList l,
    TclHAMTKeyType *kt,
    TclHAMTValueType *vt)
{
    if (l == NULL) {
	return;
    }
    l->claim--;
    if (l->claim) {
	return;
    }
    if (kt && kt->dropRefProc) {
	kt->dropRefProc(l->key);
    }
    l->key = NULL;
    if (vt && vt->dropRefProc) {
	vt->dropRefProc(l->value);
    }
    l->value = NULL;
    KVLDisclaim(l->tail);
    l->tail = NULL;
    ckfree(l);
}

static
KVList KVLFind(
    KVList l,
    TclHAMTKeyType *kt,
    ClientData key)
{
    if (l == NULL) {
	return NULL;
    }
    if (l->key == key) {
	return l;
    }
    if (kt && kt->isEqualProc) {
	if ( kt->isEqualProc( l->key, key) ) {
	    return l;
	}
    }
    return KVLFind(l->tail, kt, key);
}

static
void FillPair(
    KVList l,
    TclHAMTKeyType *kt,
    ClientData key,
    TclHAMTValueType *vt,
    ClientData value)
{
    if (kt && kt->makeRefProc) {
	kt->makeRefProc(key);
    }
    l->key = key;
    if (vt && vt->makeRefProc) {
	vt->makeRefProc(value);
    }
    l->value = value;
}

static
KVList KVLInsert(
    KVList l,
    TclHAMTKeyType *kt,
    ClientData key,
    TclHAMTValueType *vt,
    ClientData value,
    ClientData *valuePtr)
{
    KVList result, found = KVLFind(l, kt, key);

    if (found) {
	KVList copy, last = NULL;

	/* List l already has a pair matching key */

	if (valuePtr) {
	    *valuePtr = found->value;
	}

	if (found->value == value) {
	    /* ...and it already has the desired value, so make no
	     * change and return the unchanged list. */
	    return l;
	}

	/*
	 * Need to replace old value with desired one.  Lists are persistent
	 * so create new list with desired pair. Make needed copies. Keep
	 * common tail.
	 */

	/* Create copies of nodes before found to start new list. */

	while (l != found) {
	    copy = ckalloc(sizeof(KVNode));
	    copy->claim = 0;
	    if (last) {
		KVLClaim(copy);
		last->tail = copy;
	    } else {
		result = copy;
	    }

	    FillPair(copy, kt, l->key, vt, l->value);

	    last = copy;
	    l = l->tail;
	}

	/* Create a copy of *found to be the inserted node. */

	copy = ckalloc(sizeof(KVNode));
	copy->claim = 0;
	if (last) {
	    KVLClaim(copy);
	    last->tail = copy;
	} else {
	    result = copy;
	}

	FillPair(copy, kt, key, vt, value);

	/* Share tail of found as tail of copied/modified list */

	KVLClaim(found->tail);
	copy->tail = found->tail;

	return result;
    }

    /*
     * Did not find the desired key. Create new pair and place it at head.
     * Share whole prior list as tail of new node.
     */

    if (valuePtr) {
	*valuePtr = NULL;
    }

    result = ckalloc(sizeof(KVNode));
    result->claim = 0;

    FillPair(result, kt, key, vt, value);

    KVLClaim(l);
    result->tail = l;

    return result;
}

static
KVList KVLRemove(
    KVList l,
    TclHAMTKeyType *kt,
    ClientData key,
    TclHAMTValueType *vt,
    ClientData *valuePtr)
{
    KVList found = KVLFind(l, kt, key);

    if (found) {
	/* List l has a pair matching key */

	KVList copy, result = NULL, last = NULL;

	if (valuePtr) {
	    *valuePtr = found->value;
	}

	/*
	 * Need to create new list without the found node.
	 * Make needed copies. Keep common tail.
	 */

	/* Create copies of nodes before found to start new list. */

	while (l != found) {
	    copy = ckalloc(sizeof(KVNode));
	    copy->claim = 0;
	    if (last) {
		KVLClaim(copy);
		last->tail = copy;
	    } else {
		result = copy;
	    }

	    FillPair(copy, kt, l->key, vt, l->value);

	    last = copy;
	    l = l->tail;
	}

	/* Share tail of found as tail of copied/modified list */

	if (last) {
	    KVLClaim(found->tail);
	    last->tail = found->tail;
	} else {
	    result = found->tail;
	}

	return result;
    }

    if (valuePtr) {
	*valuePtr = NULL;
    }

    /* The key is not here. Nothing to remove. Return unchanged list. */
    return l;
}

/*
 * That completes the persistent KVLists.  They are the containers of
 * our Key/Value pairs that live in the leaves of our HAMT.  Now to
 * make the tree.
 */

/*
 * Each interior node of the trie is an ArrayMap.
 * 
 * We can conceptualize the trie as a complete tree with each node
 * branching so that it has 2^k child nodes.  An index of k bits
 * is needed to select one child among all of them.  Each node in the
 * complete tree can be labeled by the sequence of k-bit indices followed
 * to reach it.  Since the overall index into our trie is a size_t hash
 * value, the depth of the tree need be no larger than the number of bits
 * in a size_t divided by the k bits consumed by each index taking is 
 * another step deeper into the tree.
 * 
 * For a concrete example, consider k=6, so a 64-branching tree.  To
 * follow a 64-bit hash value through the tree to its KVList down in
 * a leaf, we take 10 6-bit steps to a new child node, and end up in
 * a leaf node holding 16 (2^(64 - 10 * 6)) KVLists.  This is in
 * fact what a trie would have to look like, were it actually filled up
 * with all 2^64 possible hash values a size_t hash can hold.
 *
 * The idea is that we follow a path down this complete tree as directed
 * by our hash value to find the one leaf node containing the KVList we
 * need. When thinking of that walk through the complete tree, we know
 * where we are on our journey by examining the hash value. and counting
 * our depth.  All we need the nodes themselves to provide are the pointers
 * to the children.
 *
 * However, we are never going to store anything close to 2^64 pairs
 * in our structure.  The leaf nodes are not going to be full.
 * Any leaf node will not actually hold its capacity of 16 KVLists.
 * There are 2^60 of them!  They will almost all be empty!
 * Some will hold 1 KVList, and some even smaller fraction may hold 2,
 * with truly rare examples of anything more.  This is true even if
 * we are storing what we think of as a very large mapping of 2^30
 * KVLists.  That remains the tiniest fraction of the space available.
 *
 * Also, the paths through the tree that actually lead to a stored
 * KVList are also quite sparse, and full of uninteresting segments.
 * At many interior nodes there will be only one path in and one path
 * out.  The node itself serves no function in terms of making a choice
 * to go one way or another.  We see that we can throw away huge portions
 * of the complete tree if we strip away all the empty and uselessly
 * trivial bits and keep only those portions where there is structurally
 * actual decisive branching taking place.  This is one of the key
 * accomplishments of this data type.
 *
 * The catch is that if we throw away the nonessentials, we can no longer
 * know where we are in the tree just by counting steps and consulting
 * our hash value as a map.  We need to store identifying information
 * in each node itself, and that's what the mask and id values do for us.
 *
 * Although we will only create and make use of a small fraction of the
 * possible interior nodes, those that we do use still have a place in
 * the complete structure, and we record within them the name of that place.
 * We label each node with an id that is the prefix of all hash values
 * that are stored in children of that node.  The node exists only if
 * there are at least 2 such children.  The node also stores a mask to
 * be applied to the hash value so that comparison to the node id only
 * takes into account the proper prefix length.
 *
 * ZZ

 * Because we implement the trie as another persistent, immutable
 * structure, we also use a claim counting system to manage lifetimes
 * for now to get something functional, with intent to change to other
 * less constraining techniques later on.
 *
 It starts with four
 * fields, each a size_t, then we have anywhere from one to 64 pointers.
 * So the size of an interior node ranges from 5 to 68 pointers, or
 * from 40 to 544 bytes.
 */

typedef struct ArrayMap {
    size_t	claim;	/* How many claims on this struct */
    size_t	mask;	/* What mask we should apply to a hash value
			 * before comparing to our id value.  This value
			 * determines the depth of the node when its place
			 * is considered in the conception of the complete
			 * tree. In a complete tree, the mask of the root
			 * node is 0.  In a complete tree, the children of
			 * the root node have a mask with enough high bits
			 * set to select one branching index.  The next
			 * level o
			 * 

			 * be a multiple of the branching shift of set
			 * high bits followed by all the low bits cleared.
			 *
			 * 1..1 1..1 ... 1..1 0..0 ... 0..0
			 * ----
			 * ^- Branch shift * depth
			 */ Branch
    size_t	id;
    size_t	map;
    void *	children;
} ArrayMap;

/*
 *
 * We will use a size_t value to hold the bitmap directing internal nodes.
 * This means our branching factor can be no bigger than our pointer size.
 * We can have up to 32-wide branching on 32 bit systems.  On 64 bit systems,
 * it is possible to go up to 64-wide branching.  The impact on performance
 * plays out in both the benefits from shallow, wide trees and the potential
 * cost of internal nodes that outgrow practical cache sizes.  Since we only
 * grow our nodes as needed, many nodes are not actually as large as they
 * potentially can become.  We may need to experiment to make a final choice.
 * 
 * On 64 bit systems we have the potential to benefit from the immensely
 * wider space made possible by 64 bit hash values.
 */









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

#define AM_SIZE(numChildren) \
	(sizeof(ArrayMap) + ((numChildren) - 1) * sizeof(void *))

static inline int	NumBits(size_t value);
static ArrayMap *	GetSet(ArrayMap *amPtr, const size_t *hashPtr,
			    const TclHAMTKeyType *ktPtr, const ClientData key,
			    const TclHAMTValType *vtPtr,
			    const ClientData value, ClientData *valuePtr);


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


typedef struct HAMT {
    size_t		 claim;	/* How many claims on this struct */
    const TclHAMTKeyType *kt;	/* Custom key handling functions */
    const TclHAMTValType *vt;	/* Custom value handling functions */
    KVList		 kvl;	/* When map stores a single KVList,
				 * just store it here (no tree) ... */
    union {
	size_t		 hash;	/* ...with its hash value. */
	ArrayMap	 *am;	/* >1 KVList? Here's the tree root. */
    } x;
} HAMT;

/*
 *----------------------------------------------------------------------
 *
 * Hash --
 *
 *	Factored out utility routine to compute the hash of a key for
 *	a particular HAMT.
 *
 * Results:
 *	The computed hash value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static
size_t Hash(
    HAMT *hPtr,
    ClientData key)
{
    return (hPtr->kt && hPtr->kt->hashProc) ? hPtr->ky->hashProc(key) : key;
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
    const TclHAMTKeyType *kt,	/* Custom key handling functions */
    const TclHAMTValType *vt)	/* Custom value handling functions */
{
    HAMT *hPtr = ckalloc(sizeof(HAMT));

    hPtr->claim = 0;
    hPtr->kt = kt;
    hPtr->vt = vt;
    hPtr->kvl = NULL;
    hPtr->x.am = NULL;
    return hPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTClaim--
 *
 *	Record a claim on a TclHAMT.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	HAMT is claimed.
 *
 *----------------------------------------------------------------------
 */

void
TclHAMTClaim(
    TclHAMT hamt)
{
    HAMT *hPtr = hamt;

    if (hPtr == NULL) {
	return;
    }
    hPtr->claim++;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTDisclaim--
 *
 *	Release a claim on a TclHAMT.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	HAMT is released. Other claims may release. Memory may free.
 *
 *----------------------------------------------------------------------
 */

void
TclHAMTDisclaim(
    TclHAMT hamt)
{
    HAMT *hPtr = hamt;

    if (hPtr == NULL) {
	return;
    }
    hPtr->claim--;
    if (hPtr->claim) {
	return;
    }
    hPtr->kt = NULL;
    hPtr->vt = NULL;
    if (hPtr->kvl) {
	KVLDisclaim(hPtr->kvl);
	hPtr->kvl = NULL;
	hPtr->x.hash = 0;
    } else if (hPtr->x.am) {
	AMDisclaim(hPtr->x.am);
	hPtr->x.am = NULL;
    }
    ckfree(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTFetch --
 *
 *	Lookup key in the key/value map.
 *
 * Results:
 *	The corresponding value, or NULL, if the key was not in the map.
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
    HAMT *hPtr = hamt;

    if (hPtr->kvl) {
	/* Map holds a single KVList. Is it for the right hash? */
	if (hPtr->x.hash == Hash(hPtr, key)) {
	    /* Yes, try to find key in it. */
	    KVList l = KVLFind(hPtr->kvl, hPtr->kt, key);
	    return l ? l->value : NULL;
	}
	/* No. Map doesn't hold the key. */
	return NULL;
    }
    if (hPtr->x.am == NULL) {
	/* Map is empty. No key is in it. */
	return NULL;
    }
    /* Map has a tree. Fetch from it. */
    return AMFetch(hPtr->x.am, ..., /* hashPtr */ NULL, key);
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTInsert--
 *
 *	Insert new key, value pair into the TclHAMT.
 *
 * Results:
 *	The new revised TclHAMT.
 *
 * Side effects:
 *	If valuePtr is not NULL, write to *valuePtr the
 *	previous value associated with key in the TclHAMT,
 *	or NULL if the key is new to the map.
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
    HAMT *new, *hPtr = hamt;
    KVList l;
    ArrayMap *am;

    if (hPtr->kvl) {
	/* Map holds a single KVList. Is it for the same hash? */
	if (hPtr->x.hash == Hash(hPtr, key)) {
	    /* Yes. Indeed we have a hash collision! This is the right
	     * KVList to insert our pair into. */
	    l = KVLInsert(hPtr->kvl, hPtr->kt, key, hPtr->vt, value, valuePtr);
	
	    if (l == hPtr->kvl) {
		/* list unchanged -> HAMT unchanged. */
		return hamt;
	    }

	    /* Construct a new HAMT with a new kvl */
	    new = ckalloc(sizeof(HAMT));
	    new->claim = 0;
	    new->kt = hPtr->kt;
	    new->vt = hPtr->vt;
	    KVLClaim(l);
	    new->kvl = l;
	    new->x.am = NULL;

	    return new;
	}
	/* No. Insertion should not be into the singleton KVList.
	 * We get to build a tree out of the singleton KVList and
	 * a new list holding our new pair. */

/* TODO TODO TODO */
	new = ckalloc(sizeof(HAMT));
	new->claim = 0;
	new->kt = hPtr->kt;
	new->vt = hPtr->vt;
	new->kvl = NULL;
	am = AMInsert(...) ;
	AMClaim(am);
	new->x.am = am;
/* TODO TODO TODO */

	return new;
    }
    if (hPtr->x.am == NULL) {
	/* Map is empty. No key is in it. Create singleton KVList
	 * out of new pair. */

	new = ckalloc(sizeof(HAMT));
	new->claim = 0;
	new->kt = hPtr->kt;
	new->vt = hPtr->vt;
	l = KVLInsert(NULL, hPtr->kt, key, hPtr->vt, value, valuePtr);
	KVLClaim(l);
	new->kvl = l;
	new->x.hash = Hash(hPtr, key);
	
	return new;
    }
    /* Map has a tree. Insert into it. */
    new = ckalloc(sizeof(HAMT));
    new->claim = 0;
    new->kt = hPtr->kt;
    new->vt = hPtr->vt;
    new->kvl = NULL;
    am = AMInsert(hPtr->x.am,  ..., /* hashPtr */ NULL,
	hPtr->kt, key, vt, value, valuePtr);
    AMClaim(am);
    new->x.am = am;
	
    return new;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTRemove--
 *
 *	Remove the key, value pair from hamt that contains the
 *	given key, if any.
 *
 * Results:
 *	The new revised TclHAMT.
 *
 * Side effects:
 *	If valuePtr is not NULL, write to *valuePtr the
 *	value of the removed key, value pair, or NULL if
 *	no pair was removed.
 *
 *----------------------------------------------------------------------
 */

TclHAMT
TclHAMTRemove(
    TclHAMT hamt,
    ClientData key,
    ClientData *valuePtr)
{
    HAMT *new, *hPtr = hamt;
    KVList l;
    ArrayMap *am;

    if (hPtr->kvl) {
	/* Map holds a single KVList. Is it for the same hash? */
	if (hPtr->x.hash == Hash(hPtr, key)) {
	    /* Yes. Indeed we have a hash collision! This is the right
	     * KVList to remove our pair from. */

	    l = KVLRemove(hPtr->kvl, hPtr->kt, key, hPtr->vt, valuePtr);
	
	    if (l == hPtr->kvl) {
		/* list unchanged -> HAMT unchanged. */
		return hamt;
	    }

	    /* TODO: Implement a shared empty HAMT ? */
	    /* Construct a new HAMT with a new kvl */
	    new = ckalloc(sizeof(HAMT));
	    new->claim = 0;
	    new->kt = hPtr->kt;
	    new->vt = hPtr->vt;
	    if (l) {
		KVLClaim(l);
	    }
	    new->kvl = l;
	    new->x.am = NULL;

	    return new;
	}

	/* The key is not in the only KVList we have. */
	if (valuePtr) {
	    *valuePtr = NULL;
	}
	return hamt;
    }
    if (hPtr->x.am == NULL) {
	/* Map is empty. No key is in it. */
	if (valuePtr) {
	    *valuePtr = NULL;
	}
	return hamt;
    }

/* TODO TODO TODO *
 * Sort out case where AM drops to singleton. */
    /* Map has a tree. Remove from it. */
    new = ckalloc(sizeof(HAMT));
    new->claim = 0;
    new->kt = hPtr->kt;
    new->vt = hPtr->vt;
    new->kvl = NULL;
    am = AMRemove(hPtr->x.am,  ..., /* hashPtr */ NULL,
	hPtr->kt, key, vt, value, valuePtr);
    AMClaim(am);
    new->x.am = am;
	
    return new;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
