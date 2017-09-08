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
    KVLDisclaim(l->tail, kt, vt);
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
 * Each interior node of the trie is an ArrayMap.
 *
 * In concept each ArrayMap stands for a single interior node in the
 * complete trie.  The mask and id fields identify which node it is.
 * The masks are set high bits followed by cleared low bits.  The number
 * of set bits is a multiple of the branch index size of the tree, and
 * the multiplier is the depth of the node in the complete tree.
 *
 * All hashes for which ( (hash & mask) == id ) are hashes with paths
 * that pass through the node identified by those mask and id values.
 *
 * Since we can name each node in this way, we don't have to store the
 * entire tree structure.  We can create and operate on only those nodes
 * where something consquential happens.  In particular we need only nodes
 * that have at least 2 children.  Entire sub-paths that have no real
 * branching are collapsed into single links.
 *
 * If we demand that each node contain 2 children, we have to permit
 * KVLists to be stored in any node. Otherwise, frequently a leaf node
 * would only hold one KVList.  Each ArrayMap can have 2 types of children,
 * KVLists and subnode ArrayMaps.  Since we are not limiting storage of
 * KVLists to leaf nodes, we cannot derive their hash values from where
 * they are stored. We must store the hash values.  This means the
 * ArrayMap subnode children are identified by a pointer alone, while
 * the KVList children need both a hash and a pointer.  To deal with
 * this we store the two children sets in separate portions of the slot
 * array with separate indexing.  That is the reason for separate kvMap
 * and amMap.
 *
 * The slot storage is structured with all hash values stored first,
 * as indexed by kvMap. Next comes parallel storage of the KVList pointers.
 * Last comes the storage of the subnode ArrayMap pointers, as indexed
 * by amMap.  The size of the AMNode struct must be set so that slot
 * has the space needed for all 3 collections.
 */


typedef struct AMNode *ArrayMap;

typedef struct AMNode {
    size_t	claim;	/* How many claims on this struct */
    size_t	mask;	/* The mask and id fields together identify the */
    size_t	id;	/* location of the node in the complete tree */
    size_t	kvMap;  /* Map to children containing a single KVList each */
    size_t	amMap;	/* Map to children that are subnodes */
    void *	slot;	/* Resizable space for outward links */
} AMNode;

#define AMN_SIZE(numList, numSubnode) \
	(sizeof(AMNode) + (2*(numList) + (numSubnode) - 1) * sizeof(void *))

/*
 * The branching factor of the tree is constrained by our map sizes
 * which is determined by the size of size_t.
 *
 * TODO: Implement way to easily configure these values to explore
 * impact of different branching factor.
 */

/* Bits in a size_t. Use as our branching factor. Max children per node. */
const int branchFactor = CHAR_BIT * sizeof(size_t);

/* Bits in a index selecting a child of a node */
const int branchShift = TclMSB(branchFactor);

/* Mask used to carve out branch index. */
const int branchMask = (branchFactor - 1)

/*
 * The operations on an ArrayMap:
 *	AMClaim		Make a claim on a node.
 *	AMDisclaim	Release a claim on a node.
 *	AMNew		Make initial node from two NVLists.
 *	AMFetch		Fetch value from node given a key.
 *	AMInsert	Create a new node, inserting new pair into old node.
 *	AMRemove	Create a new node, with any pair matching key removed.
 */

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
 * LSB --
 *
 * Results:
 *	Least significant set bit in a size_t value.
 *	AKA, number of trailing cleared bits in the value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static inline int
LSB(
    size_t value)
{
#if defined(__GNUC__) && ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2))
    return __builtin_ctzll(value);
#else
#error  LSB not implemented!
#endif
}

static
void AMClaim(
    ArrayMap am)
{
    if (am != NULL) {
	am->claim++;
    }
}

static
void AMDisclaim(
    ArrayMap am,
    TclHAMTKeyType *kt,
    TclHAMTValueType *vt)
{
    int i, numList, numSubnode;

    if (am == NULL) {
	return;
    }
    am->claim--;
    if (am->claim) {
	return;
    }

    numList = NumBits(am->kvMap);
    numSubnode = NumBits(am->amMap);

    am->mask = 0;
    am->id = 0;
    am->kvMap = 0;
    am->amMap = 0;

    for (i = 0; i < numList; i++) {
	am->slot[i] = NULL;
    }
    for (i = numList; i < 2*numList; i++) {
	KVLDisclaim(am->slot[i], kt, vt);
	am->slot[i] = NULL;
    }
    for (i = 2*numList; i < 2*numList + numSubnode; i++) {
	AMDisclaim(am->slot[i], kt, vt);
	am->slot[i] = NULL;
    }

    ckfree(am);
}

/*
 *----------------------------------------------------------------------
 *
 * AMNew --
 *
 * 	Create an ArrayMap to serve as a branching node distinguishing
 * 	the paths to two KVLists given their hash values.
 *
 * Results:
 *	The created ArrayMap.
 *
 * Side effects:
 * 	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static
ArrayMap AMNew(
    ClientData hash1,
    KVList l1,
    ClientData hash2,
    KVList l2)
{
    int depth, idx1, idx2;
    size_t *hashes;
    KVList *lists;
    ArrayMap new = ckalloc(AMN_SIZE(2, 0));

    new->claim = 0;

    /* The depth of the tree for the node we must create.
     * Determine by lowest bit where hashes differ. */

    depth = LSB(hash1 ^ hash2) / branchShift;

    new->mask =  (1 << (depth * branchShift)) - 1;
    new->id = hash1 & new->mask;

    assert ( (hash2 & new->mask) == new->id );

    idx1 = (hash1 >> (depth * branchShift)) & branchMask;
    idx2 = (hash2 >> (depth * branchShift)) & branchMask;

    assert ( idx1 != idx2 );

    new->kvMap = ((size_t)1 << idx1) | ((size_t)1 << idx2);
    new->amMap = 0;

    KVLClaim(l1);
    KVLCLaim(l2);

    hashes = (size_t *)&(new->slots);
    lists = (KVList *) (hashes + 2)
    if (idx1 < idx2) {
	assert ( hash1 < hash2 );

	hashes[0] = hash1;
	hashes[1] = hash2;
	lists[0] = l1;
	lists[1] = l2;
    } else {
	assert ( hash1 > hash2 );

	hashes[0] = hash2;
	hashes[1] = hash1;
	lists[0] = l2;
	lists[1] = l1;
    }

    return new;
}

/* Finally, the top level struct that puts all the pieces together */


typedef struct HAMT {
    size_t		 claim;	/* How many claims on this struct */
    const TclHAMTKeyType *kt;	/* Custom key handling functions */
    const TclHAMTValType *vt;	/* Custom value handling functions */
    KVList		 kvl;	/* When map stores a single KVList,
				 * just store it here (no tree) ... */
    union {
	size_t		 hash;	/* ...with its hash value. */
	ArrayMap	 am;	/* >1 KVList? Here's the tree root. */
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
    if (hPtr->kvl) {
	KVLDisclaim(hPtr->kvl, hPtr->kt, hPtr->vt);
	hPtr->kvl = NULL;
	hPtr->x.hash = 0;
    } else if (hPtr->x.am) {
	AMDisclaim(hPtr->x.am, hPtr->kt, hPtr->vt);
	hPtr->x.am = NULL;
    }
    hPtr->kt = NULL;
    hPtr->vt = NULL;
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
    ArrayMap am;

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
    ArrayMap am;

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
