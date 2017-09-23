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

typedef struct KVNode {
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
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt)
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
    const TclHAMTKeyType *kt,
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
    const TclHAMTKeyType *kt,
    ClientData key,
    const TclHAMTValueType *vt,
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
    const TclHAMTKeyType *kt,
    ClientData key,
    const TclHAMTValueType *vt,
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
    const TclHAMTKeyType *kt,
    ClientData key,
    const TclHAMTValueType *vt,
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
    ClientData	slot[];	/* Resizable space for outward links */
} AMNode;

#define AMN_SIZE(numList, numSubnode) \
	(sizeof(AMNode) + (2*(numList) + (numSubnode)) * sizeof(void *))

/*
 * The branching factor of the tree is constrained by our map sizes
 * which is determined by the size of size_t.
 *
 * TODO: Implement way to easily configure these values to explore
 * impact of different branching factor.
 */

/* Bits in a size_t. Use as our branching factor. Max children per node. */
const int branchFactor = CHAR_BIT * sizeof(size_t);

/*
 * The operations on an ArrayMap:
 *	AMClaim		Make a claim on a node.
 *	AMDisclaim	Release a claim on a node.
 *	AMNewLeaf	Make leaf node from two NVLists.
 *	AMNewBranch	Make branch mode from NVList and ArrayMap.
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
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt)
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
 * AMNewBranch --
 *
 * 	Create an ArrayMap to serve as a container for
 * 	a new KVList and an existing ArrayMap subnode.
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
ArrayMap AMNewBranch(
    ArrayMap sub,
    size_t hash,
    KVList l)
{
    /* Mask used to carve out branch index. */
    const int branchMask = (branchFactor - 1);

    /* Bits in a index selecting a child of a node */
    const int branchShift = TclMSB(branchFactor);

    /* The depth of the tree for the node we must create.
     * Determine by lowest bit where hashes differ. */
    int depth = LSB(hash ^ sub->id) / branchShift;

    int idx1 = (hash >> (depth * branchShift)) & branchMask;
    int idx2 = (sub->id >> (depth * branchShift)) & branchMask;
    ArrayMap new = ckalloc(AMN_SIZE(1, 1));

    assert ( idx1 != idx2 );

    new->claim = 0;
    new->mask =  (1 << (depth * branchShift)) - 1;
    new->id = hash & new->mask;

    assert ( (sub->id & new->mask) == new->id );

    new->kvMap = (size_t)1 << idx1;
    new->amMap = (size_t)1 << idx2;

    KVLClaim(l);
    AMClaim(sub);

    new->slot[0] = (ClientData)hash;
    new->slot[1] = l;
    new->slot[2] = sub;

    return new;
}
/*
 *----------------------------------------------------------------------
 *
 * AMNewLeaf --
 *
 * 	Create an ArrayMap to serve as a container for
 * 	two KVLists given their hash values.
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
ArrayMap AMNewLeaf(
    size_t hash1,
    KVList l1,
    size_t hash2,
    KVList l2)
{
    /* Mask used to carve out branch index. */
    const int branchMask = (branchFactor - 1);

    /* Bits in a index selecting a child of a node */
    const int branchShift = TclMSB(branchFactor);

    size_t *hashes;
    KVList *lists;

    /* The depth of the tree for the node we must create.
     * Determine by lowest bit where hashes differ. */
    int depth = LSB(hash1 ^ hash2) / branchShift;

    int idx1 = (hash1 >> (depth * branchShift)) & branchMask;
    int idx2 = (hash2 >> (depth * branchShift)) & branchMask;

    ArrayMap new = ckalloc(AMN_SIZE(2, 0));

    assert ( idx1 != idx2 );

    new->claim = 0;
    new->mask =  (1 << (depth * branchShift)) - 1;
    new->id = hash1 & new->mask;

    assert ( (hash2 & new->mask) == new->id );

    new->kvMap = ((size_t)1 << idx1) | ((size_t)1 << idx2);
    new->amMap = 0;

    KVLClaim(l1);
    KVLClaim(l2);

    hashes = (size_t *)&(new->slot);
    lists = (KVList *) (hashes + 2);
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

/*
 *----------------------------------------------------------------------
 *
 * AMFetch --
 *
 *	Lookup key in this subset of the key/value map.
 *
 * Results:
 *	The corresponding value, or NULL, if the key was not there.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static
ClientData AMFetch(
    ArrayMap am,
    const TclHAMTKeyType *kt,
    size_t hash,
    ClientData key)
{
    /* Mask used to carve out branch index. */
    const int branchMask = (branchFactor - 1);

    size_t tally;

    if ((am->mask & hash) != am->id) {
	/* Hash indicates key is not in this subtree */
	return NULL;
    }

    tally = (size_t)1 << ((hash >> LSB(am->mask + 1)) & branchMask);
    if (tally & am->kvMap) {
	KVList l;

	/* Hash is consistent with one of our KVList children... */
	int offset = NumBits(am->kvMap & (tally - 1));

	if (am->slot[offset] != (ClientData)hash) {
	    /* ...but does not actually match. */
	    return NULL;
	}

	l = KVLFind(am->slot[offset + NumBits(am->kvMap)], kt, key);
	return l ? l->value : NULL;
    }
    if (tally & am->amMap) {
	/* Hash is consistent with one of our subnode children... */

	return AMFetch(am->slot[2 * NumBits(am->kvMap)
		+ NumBits(am->amMap & (tally - 1))], kt, hash, key);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * AMInsert --
 *
 *	Insert new key, value pair into this subset of the key/value map.
 *
 * Results:
 *	A new revised subset containing the new pair.
 *
 * Side effects:
 *	If valuePtr is not NULL, write to *valuePtr the
 *	previous value associated with key in subset
 *	or NULL if the key was not there before.
 *----------------------------------------------------------------------
 */

static
ArrayMap AMInsert(
    ArrayMap am,
    size_t hash,
    const TclHAMTKeyType *kt,
    ClientData key,
    const TclHAMTValueType *vt,
    ClientData value,
    ClientData *valuePtr)
{
    /* Mask used to carve out branch index. */
    const int branchMask = (branchFactor - 1);

    size_t tally;
    int numList, numSubnode, loffset, soffset, i;
    ArrayMap new, sub;
    ClientData *src, *dst;

    if ((am->mask & hash) != am->id) {
	/* Hash indicates key is not in this subtree */

	/* Caller sent us here, because prefix+index said so,
	 * but we do not belong here.  We need to create a
	 * missing node between to hold reference to us and
	 * reference to the new (key, value).
	 */

	return AMNewBranch(am, hash,
		KVLInsert(NULL, kt, key, vt, value, valuePtr));
    }

    /* Hash indicates key should be descendant of am */
    numList = NumBits(am->kvMap);
    numSubnode = NumBits(am->amMap);

    tally = (size_t)1 << ((hash >> LSB(am->mask + 1)) & branchMask);
    if (tally & am->kvMap) {

	/* Hash is consistent with one of our KVList children... */
	loffset = NumBits(am->kvMap & (tally - 1));

	if (am->slot[loffset] != (ClientData)hash) {
	    /* ...but does not actually match.
	     * Need a new KVList to join with this one. */
	
	    sub = AMNewLeaf((size_t)am->slot[loffset],
		    am->slot[loffset + numList], hash,
		    KVLInsert(NULL, kt, key, vt, value, valuePtr));

	    /* Modified copy of am, - list + sub */

	    new = ckalloc(AMN_SIZE(numList-1, numSubnode+1));
	    new->claim = 0;
	    new->mask = am->mask;
	    new->id = am->id;

	    new->kvMap = am->kvMap & ~tally;
	    new->amMap = am->amMap | tally;

	    src = am->slot;
	    dst = new->slot;
	    /* Copy hashes (except one we're deleting) */
	    for (i = 0; i < loffset; i++) {
		*dst++ = *src++;
	    }
	    src++;
	    for (i = loffset + 1; i < numList; i++) {
		*dst++ = *src++;
	    }

	    /* Copy list (except one we're deleting) */
	    for (i = 0; i < loffset; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }
	    src++;
	    for (i = loffset + 1; i < numList; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }

	    /* Copy subnodes and add the new one */
	    soffset = NumBits(am->amMap & (tally - 1));
	    for (i = 0; i < soffset; i++) {
		AMClaim((ArrayMap) *src);
		*dst++ = *src++;
	    }
	    AMClaim(sub);
	    *dst++ = sub;
	    for (i = soffset; i < numSubnode; i++) {
		AMClaim((ArrayMap) *src);
		*dst++ = *src++;
	    }

	    return new;
	} else {
	    /* Found the right KVList. Now Insert the pair into it. */
	    KVList l = KVLInsert(am->slot[loffset + numList], kt, key,
		    vt, value, valuePtr);

	    if (l == am->slot[loffset + numList]) {
		/* List unchanged (overwrite same value) */
		/* Map unchanged, just return */
		return am;
	    }

	    /* Modified copy of am, list replaced. */
	    new = ckalloc(AMN_SIZE(numList, numSubnode));
	
	    new->claim = 0;
	    new->mask = am->mask;
	    new->id = am->id;
	    new->kvMap = am->kvMap;
	    new->amMap = am->amMap;

	    src = am->slot;
	    dst = new->slot;
	    /* Copy all hashes */
	    for (i = 0; i < numList; i++) {
		*dst++ = *src++;
	    }

	    /* Copy list (except one we're replacing) */
	    for (i = 0; i < loffset; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }
	    src++;
	    KVLClaim(l);
	    *dst++ = l;
	    for (i = loffset + 1; i < numList; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }

	    /* Copy all subnodes */
	    for (i = 0; i < numSubnode; i++) {
		AMClaim((ArrayMap) *src);
		*dst++ = *src++;
	    }
	    return new;
	}
    }
    if (tally & am->amMap) {
	/* Hash is consistent with one of our subnode children... */
	soffset = NumBits(am->amMap & (tally - 1));

	sub = AMInsert((ArrayMap)am->slot[2*numList + soffset], hash,
		kt, key, vt, value, valuePtr);

	if (sub == am->slot[2*numList + soffset]) {
	    /* Submap unchanged (overwrite same value) */
	    /* Map unchanged, just return */
	    return am;
	}

	/* Modified copy of am, subnode replaced. */
	new = ckalloc(AMN_SIZE(numList, numSubnode));
	
	new->claim = 0;
	new->mask = am->mask;
	new->id = am->id;
	new->kvMap = am->kvMap;
	new->amMap = am->amMap;

	src = am->slot;
	dst = new->slot;
	/* Copy all hashes */
	for (i = 0; i < numList; i++) {
	    *dst++ = *src++;
	}

	/* Copy all lists */
	for (i = 0; i < numList; i++) {
	    KVLClaim((KVList) *src);
	    *dst++ = *src++;
	}

	/* Copy subnodes (except the one we're replacing) */
	for (i = 0; i < soffset; i++) {
	    AMClaim((ArrayMap) *src);
	    *dst++ = *src++;
	}
	src++;
	AMClaim(sub);
	*dst++ = sub;
	for (i = soffset + 1; i < numSubnode; i++) {
	    AMClaim((ArrayMap) *src);
	    *dst++ = *src++;
	}

	return new;
    }

    /* Modified copy of am, list inserted. */
    new = ckalloc(AMN_SIZE(numList + 1, numSubnode));
	
    new->claim = 0;
    new->mask = am->mask;
    new->id = am->id;
    new->kvMap = am->kvMap | tally;
    new->amMap = am->amMap;

    src = am->slot;
    dst = new->slot;

    loffset = NumBits(am->kvMap & (tally - 1));
    /* Copy all hashes and insert one */
    for (i = 0; i < loffset; i++) {
	*dst++ = *src++;
    }
    *dst++ = (ClientData)hash;
    for (i = loffset; i < numList; i++) {
	*dst++ = *src++;
    }

    /* Copy all list and insert one */
    for (i = 0; i < loffset; i++) {
	KVLClaim((KVList) *src);
	*dst++ = *src++;
    }
    *dst = KVLInsert(NULL, kt, key, vt, value, valuePtr);
    KVLClaim(*dst);
    dst++;
    for (i = loffset; i < numList; i++) {
	KVLClaim((KVList) *src);
	*dst++ = *src++;
    }

    /* Copy all subnodes */
    for (i = 0; i < numSubnode; i++) {
	AMClaim((ArrayMap) *src);
	*dst++ = *src++;
    }

    return new;
}

/*
 *----------------------------------------------------------------------
 *
 * AMRemove --
 *
 *	Remove the key, value pair containing key from this subset of
 *	the key/value map, if present.
 *
 * Results:
 *	A new revised subset without a pair containing key.
 *		OR
 *	NULL, then hashPtr, listPtr have hash and KVList values
 *	written to them for the single list left.
 *
 * Side effects:
 *	If valuePtr is not NULL, write to *valuePtr the
 *	previous value associated with key in subset
 *	or NULL if the key was not there before.
 *----------------------------------------------------------------------
 */

static
ArrayMap AMRemove(
    ArrayMap am,
    size_t hash,
    const TclHAMTKeyType *kt,
    ClientData key,
    const TclHAMTValueType *vt,
    size_t *hashPtr,
    KVList *listPtr,
    ClientData *valuePtr)
{
    /* Mask used to carve out branch index. */
    const int branchMask = (branchFactor - 1);

    size_t tally;
    int numList, numSubnode, loffset, soffset, i;
    ArrayMap new, sub;
    ClientData *src, *dst;
    KVList l;

    if ((am->mask & hash) != am->id) {
	/* Hash indicates key is not in this subtree */

	if (valuePtr) {
	    *valuePtr = NULL;
	}
	return am;
    }

    /* Hash indicates key should be descendant of am */
    numList = NumBits(am->kvMap);
    numSubnode = NumBits(am->amMap);

    tally = (size_t)1 << ((hash >> LSB(am->mask + 1)) & branchMask);
    if (tally & am->kvMap) {

	/* Hash is consistent with one of our KVList children... */
	loffset = NumBits(am->kvMap & (tally - 1));

	if (am->slot[loffset] != (ClientData)hash) {
	    /* ...but does not actually match. Key not here. */

	    if (valuePtr) {
		*valuePtr = NULL;
	    }
	    return am;
	}

	/* Found the right KVList. Remove the pair from it. */
	l = KVLRemove(am->slot[loffset + numList], kt, key, vt, valuePtr);

	if (l == am->slot[loffset + numList]) {
	    /* list unchanged -> ArrayMap unchanged. */
	    return am;
	}

	/* Create new ArrayMap with removed KVList */

	if (l != NULL) {
	    /* Modified copy of am, list replaced. */
	    new = ckalloc(AMN_SIZE(numList, numSubnode));
	
	    new->claim = 0;
	    new->mask = am->mask;
	    new->id = am->id;
	    new->kvMap = am->kvMap;
	    new->amMap = am->amMap;

	    src = am->slot;
	    dst = new->slot;
	    /* Copy all hashes */
	    for (i = 0; i < numList; i++) {
		*dst++ = *src++;
	    }

	    /* Copy list (except one we're replacing) */
	    for (i = 0; i < loffset; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }
	    src++;
	    KVLClaim(l);
	    *dst++ = l;
	    for (i = loffset + 1; i < numList; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }

	    /* Copy all subnodes */
	    for (i = 0; i < numSubnode; i++) {
		AMClaim((ArrayMap) *src);
		*dst++ = *src++;
	    }
	    return new;
	}
	if (numList + numSubnode > 2) {
	    /* Modified copy of am, list removed. */
	    new = ckalloc(AMN_SIZE(numList-1, numSubnode));
	
	    new->claim = 0;
	    new->mask = am->mask;
	    new->id = am->id;
	    new->kvMap = am->kvMap & ~tally;
	    new->amMap = am->amMap;

	    src = am->slot;
	    dst = new->slot;

	    /* Copy hashes (except one we're deleting) */
	    for (i = 0; i < loffset; i++) {
		*dst++ = *src++;
	    }
	    src++;
	    for (i = loffset + 1; i < numList; i++) {
		*dst++ = *src++;
	    }

	    /* Copy list (except one we're deleting) */
	    for (i = 0; i < loffset; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }
	    src++;
	    for (i = loffset + 1; i < numList; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }

	    /* Copy all subnodes */
	    for (i = 0; i < numSubnode; i++) {
		AMClaim((ArrayMap) *src);
		*dst++ = *src++;
	    }
	    return new;
	}
	if (numSubnode) {
	    /* Removal will leave the subnode. */
	    return am->slot[2];
	} else {
	    /* Removal will leave a list. */
	    *hashPtr = (size_t) am->slot[1 - loffset];
	    *listPtr = (KVList) am->slot[3 - loffset];
	    return NULL;
	}
    }
    if (tally & am->amMap) {
	size_t subhash;

	/* Hash is consistent with one of our subnode children... */
	soffset = NumBits(am->amMap & (tally - 1));
	sub = AMRemove((ArrayMap)am->slot[2*numList + soffset], hash,
		kt, key, vt, &subhash, &l, valuePtr);

	if (sub) {
	    /* Modified copy of am, subnode replaced. */
	    new = ckalloc(AMN_SIZE(numList, numSubnode));
	
	    new->claim = 0;
	    new->mask = am->mask;
	    new->id = am->id;
	    new->kvMap = am->kvMap;
	    new->amMap = am->amMap;

	    src = am->slot;
	    dst = new->slot;
	    /* Copy all hashes */
	    for (i = 0; i < numList; i++) {
		*dst++ = *src++;
	    }

	    /* Copy all lists */
	    for (i = 0; i < numList; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }

	    /* Copy subnodes */
	    for (i = 0; i < soffset; i++) {
		AMClaim((ArrayMap) *src);
		*dst++ = *src++;
	    }
	    src++;
	    AMClaim(sub);
	    *dst++ = sub;
	    for (i = soffset + 1; i < numSubnode; i++) {
		AMClaim((ArrayMap) *src);
		*dst++ = *src++;
	    }
	    return new;
	} else {
	    /* Modified copy of am, + list - sub */

	    loffset = NumBits(am->kvMap & (tally - 1));

	    new = ckalloc(AMN_SIZE(numList+1, numSubnode-1));
	    new->claim = 0;
	    new->mask = am->mask;
	    new->id = am->id;

	    new->kvMap = am->kvMap | tally;
	    new->amMap = am->amMap & ~tally;

	    src = am->slot;
	    dst = new->slot;
	    /* Copy hashes (up to one we're inserting) */
	    for (i = 0; i < loffset; i++) {
		*dst++ = *src++;
	    }
	    *dst++ = (ClientData) subhash;
	    for (i = loffset; i < numList; i++) {
		*dst++ = *src++;
	    }

	    /* Copy list (except one we're deleting) */
	    for (i = 0; i < loffset; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }
	    KVLClaim(l);
	    *dst++ = l;
	    for (i = loffset; i < numList; i++) {
		KVLClaim((KVList) *src);
		*dst++ = *src++;
	    }

	    /* Copy subnodes and add the new one */
	    for (i = 0; i < soffset; i++) {
		AMClaim((ArrayMap) *src);
		*dst++ = *src++;
	    }
	    src++;
	    for (i = soffset + 1; i < numSubnode; i++) {
		AMClaim((ArrayMap) *src);
		*dst++ = *src++;
	    }

	    return new;
	}
    }

    /* Key is not here. */
    if (valuePtr) {
	*valuePtr = NULL;
    }
    return am;
}

/* Finally, the top level struct that puts all the pieces together */

typedef struct HAMT {
    size_t			claim;	/* How many claims on this struct */
    const TclHAMTKeyType	*kt;	/* Custom key handling functions */
    const TclHAMTValueType	*vt;	/* Custom value handling functions */
    KVList			kvl;	/* When map stores a single KVList,
					 * just store it here (no tree) ... */
    union {
	size_t			 hash;	/* ...with its hash value. */
	ArrayMap		 am;	/* >1 KVList? Here's the tree root. */
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
    return (hPtr->kt && hPtr->kt->hashProc)
	    ? hPtr->kt->hashProc(key) : (size_t) key;
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
    const TclHAMTValueType *vt)	/* Custom value handling functions */
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
    return AMFetch(hPtr->x.am, hPtr->kt, Hash(hPtr, key), key);
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
	size_t hash = Hash(hPtr, key);
	if (hPtr->x.hash == hash) {
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

	new = ckalloc(sizeof(HAMT));
	new->claim = 0;
	new->kt = hPtr->kt;
	new->vt = hPtr->vt;
	new->kvl = NULL;

	am = AMNewLeaf(hPtr->x.hash, hPtr->kvl, hash,
		KVLInsert(NULL, hPtr->kt, key, hPtr->vt, value, valuePtr));
	AMClaim(am);
	new->x.am = am;

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
    am = AMInsert(hPtr->x.am, Hash(hPtr, key), hPtr->kt, key,
	    hPtr->vt, value, valuePtr);
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
    size_t hash;
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

    /* Map has a tree. Remove from it. */
    new = ckalloc(sizeof(HAMT));
    new->claim = 0;
    new->kt = hPtr->kt;
    new->vt = hPtr->vt;

    am = AMRemove(hPtr->x.am, Hash(hPtr, key), hPtr->kt, key,
	    hPtr->vt, &hash, &l, valuePtr);
    if (am) {
	new->kvl = NULL;
	AMClaim(am);
	new->x.am = am;
    } else {
	KVLClaim(l);
	new->kvl = l;
	new->x.hash = hash;
    }
	
    return new;
}


/* A struct to hold our place as we iterate through a HAMT */

typedef struct Idx {
    TclHAMT	hamt;
    KVList	kvl;		/* Traverse the KVList */
    KVList	*kvlv;		/* Traverse a KVList array... */
    int		kvlc;		/* ... until no more. */
    ArrayMap	*top;		/* Active KVList array is in the */
    ArrayMap	stack[];	/* ArrayMap at top of stack. */
} Idx;


/*
 *----------------------------------------------------------------------
 *
 * TclHAMTFirst --
 *
 *	Starts an iteration through hamt.
 *
 * Results:
 *	If hamt is empty, returns NULL.
 *	If hamt is non-empty, returns a TclHAMTIdx that refers to the
 *	first key, value pair in an interation through hamt.
 *
 *----------------------------------------------------------------------
 */

TclHAMTIdx
TclHAMTFirst(
    TclHAMT hamt)
{
    const int branchShift = TclMSB(branchFactor);
    const int depth = branchFactor / branchShift;

    HAMT *hPtr = hamt;
    Idx *i;
    ArrayMap am;
    int n;

    assert ( hamt );

    if (hPtr->kvl == NULL && hPtr->x.am == NULL) {
	/* Empty */
	return NULL;
    }

    i = ckalloc(sizeof(Idx) + depth*sizeof(ArrayMap));

    /*
     * We claim an interest in hamt.  After that we need not claim any
     * interest in any of its sub-parts since it already takes care of
     * that for us.
     */

    TclHAMTClaim(hamt);
    i->hamt = hamt;
    i->top = i->stack;

    if (hPtr->kvl) {
	/* One bucket */
	/* Our place is the only place. Pointing at the sole bucket */
	i->kvlv = &(hPtr->kvl);
	i->kvlc = 0;
	i->kvl = i->kvlv[0];
	i->top[0] = NULL;
	return i;
    } 
    /* There's a tree. Must traverse it to leftmost KVList. */
    am = hamt->x.am;
    while ((n = NumBits(am->kvMap)) == 0) {
	/* No buckets in the ArrayMap; Must have subnodes. Go Left */
	i->top[0] = am;
	i->top++;
	am = (ArrayMap) am->slot[0];
    }
    i->top[0] = am;
    i->kvlv = (KVList *)(am->slot + n);
    i->kvlc = n - 1;
    i->kvl = i->kvlv[0];
    return i;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTNext --
 *
 *	Given a non-NULL *idxPtr referring to key, value pair in an
 *	iteration of a hamt, transform it to refer to the next key, value
 * 	pair in that iteration.
 * 	If there are no more key, value pairs in the iteration, release
 *	all claims and overwrite *idxPtr with a NULL idx.
 *
 * Results:
 *	None.
 *----------------------------------------------------------------------
 */

void
TclHAMTNext(
    TclHAMTIdx *idxPtr)
{
    Idx *i = *idxPtr;
    ArrayMap am, popme;
    ClientData *slotPtr;
    int n, seen = 0;

    assert ( i );
    assert ( i->kvl );

    if (i->kvl->tail) {
	/* There are more key, value pairs in this bucket. */
	i->kvl = i->kvl->tail;
	return;
    }

    /* On to the next KVList bucket. */
    if (i->kvlc) {
	i->kvlv++;
	i->kvlc--;
	i->kvl = i->kvlv[0];
	return;
    }

    /* On to the subnodes */
    if (i->top[0] == NULL) {
	/* Only get here for hamt of one key, value pair. */
	TclHAMTDone((TclHAMTIdx) i);
	*idxPtr = NULL;
	return;
    }

    /*
     * We're working through the subnodes. When we entered, i->kvlv
     * pointed to a KVList within ArrayMap i->top[0], and it must have
     * pointed to the last one. Either this ArrayMap has subnodes or
     * it doesn't.
     */
  while (1) {
    if (NumBits(i->top[0]->amMap) - seen) {
	/* There are subnodes. Traverse to the leftmost KVList. */
	am = (ArrayMap)(i->kvlv + 1);

	i->top++;
	while ((n = NumBits(am->kvMap)) == 0) {
	    /* No buckets in the ArrayMap; Must have subnodes. Go Left */
	    i->top[0] = am;
	    i->top++;
	    am = (ArrayMap) am->slot[0];
	}
	i->top[0] = am;
	i->kvlv = (KVList *)(am->slot + n);
	i->kvlc = n - 1;
	i->kvl = i->kvlv[0];
	return;
    }

    /* i->top[0] is an ArrayMap with no subnodes. We're done with it.
     * Need to pop. */
    if (i->top == i->stack) {
	/* Nothing left to pop. Stack exhausted. We're done. */
	TclHAMTDone((TclHAMTIdx) i);
	*idxPtr = NULL;
	return;
    }
    popme = i->top[0];
    i->top[0] = NULL;
    i->top--;
    am = i->top[0];
    slotPtr = am->slot + 2*NumBits(am->kvMap);
    seen = 1;
    while (slotPtr[0] != (ClientData)popme) {
	seen++;
	slotPtr++;
    }
    i->kvlv = (KVList *)slotPtr;
  }
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTGet --
 *
 *	Given an idx returned by TclHAMTFirst() or TclHAMTNext() and
 *	never passed to TclHAMtDone(), retrieve the key and value it
 *	refers to.
 *
 *----------------------------------------------------------------------
 */

void
TclHAMTGet(
    TclHAMTIdx idx,
    ClientData *keyPtr,
    ClientData *valuePtr)
{
    Idx *i = idx;

    assert ( i );
    assert ( i->kvl );
    assert ( keyPtr );
    assert ( valuePtr );

    *keyPtr = i->kvl->key;
    *valuePtr = i->kvl->value;
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTDone --
 *
 *	Given an idx returned by TclHAMTFirst() or TclHAMTNext() and
 *	never passed to TclHAMtDone(), release any claims.
 *
 *----------------------------------------------------------------------
 */

void
TclHAMTDone(
    TclHAMTIdx idx)
{
    Idx *i = idx;

    TclHAMTDisclaim(i->hamt);
    i->hamt = NULL;
    ckfree(i);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
