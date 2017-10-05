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
 * tricky things are that in any one list we will store at most one
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
 *	KVLNew		Node creation utility
 *	KVLFind		Find the tail starting with an equal key.
 *	KVLMerge	Create a new list, merger of two lists.
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
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    KVList l)
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
    KVLDisclaim(kt, vt, l->tail);
    l->tail = NULL;
    ckfree(l);
}

static
KVList KVLFind(
    const TclHAMTKeyType *kt,
    KVList l,
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
    return KVLFind(kt, l->tail, key);
}

static
KVList KVLNew(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    ClientData key,
    ClientData value,
    KVList tail)
{
    KVList new = ckalloc(sizeof(KVNode));
    new->claim = 0;
    if (kt && kt->makeRefProc) {
	kt->makeRefProc(key);
    }
    new->key = key;
    if (vt && vt->makeRefProc) {
	vt->makeRefProc(value);
    }
    new->value = value;
    if (tail) {
	KVLClaim(tail);
    }
    new->tail = tail;
    return new;
}

/*
 * Return a list that is merge of argument lists one and two.
 * When returning "one" itself is a correct answer, do that.
 * Otherwise, when returning "two" itself is a correct answer, do that.
 * These constraints help subdue creation of unnecessary copies.
 */
static
KVList KVLMerge(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    KVList one,
    KVList two,
    ClientData *valuePtr)
{
    KVList result = two;
    KVList l = one;
    int canReturnOne = 1;
    int canReturnTwo = 1;
    int numSame = 0;
    ClientData prevValue = NULL;

    if (one == two) {
	/* Merge into self yields self */
	return one;
    }
    while (l) {
	/* Check whether key from one is in two. */
	KVList found = KVLFind(kt, two, l->key);

	if (found) {
	    /*
	     * This merge includes an overwrite of a key in one by
	     * the same key in two.
	     */
	    if (found->value == l->value) {
		numSame++;
	    } else {
		/* This pair in one cannot be in the merge. */
		canReturnOne = 0;
	    }
	    prevValue = l->value;
	} else {
	    /*
	     * result needs to contain old key value from one that
	     * was not overwritten as well as the new key values
	     * from two.  We now know result can be neither one nor two.
	     */
	    result = KVLNew(kt, vt, l->key, l->value, result);
	    canReturnOne = 0;
	    canReturnTwo = 0;
	}
	l = l->tail;
    }
    if (valuePtr) {
	*valuePtr = prevValue;
    }
    if (canReturnOne) {
	l = two;
	while (numSame--) {
	    l = l->tail;
	}
	if (l == NULL) {
	    /* We discovered one and two were copies. */
	    return one;
	}
    }
    if (canReturnTwo) {
	return two;
    }
    return result;
}

static
KVList KVLInsert(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    KVList l,
    ClientData key,
    ClientData value,
    ClientData *valuePtr)
{
    KVList new = KVLNew(kt, vt, key, value, NULL);
    KVList result = KVLMerge(kt, vt, l, new, valuePtr);

    if (result == l) {
	/* No-op insert; discard new node */
	KVLClaim(new);
	KVLDisclaim(kt, vt, new);
    }
    return result;
}

static
KVList KVLRemove(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    KVList l,
    ClientData key,
    ClientData *valuePtr)
{
    KVList found = KVLFind(kt, l, key);

    if (found) {

	/*
	 * List l has a pair matching key
	 * Need to create new list without the found node.
	 * Make needed copies. Keep common tail.
	 */

	KVList result = found->tail;
	while (l != found) {
	    result = KVLNew(kt, vt, l->key, l->value, result);
	    l = l->tail;
	}

	if (valuePtr) {
	    *valuePtr = found->value;
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
 *	AMNew		allocator
 *	AMNewLeaf	Make leaf node from two NVLists.
 *	AMNewParent	Make node to contain two descendant nodes.
 *	AMNewBranch	Make branch mode from NVList and ArrayMap.
 *	AMFetch		Fetch value from node given a key.
 *	AMMergeList	Create new node, merging node and list.
 *	AMMergeDescendant	Merge two nodes where one is ancestor.
 *	AMMerge		Create new node, merging two nodes.
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
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    ArrayMap am)
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
	KVLDisclaim(kt, vt, am->slot[i]);
	am->slot[i] = NULL;
    }
    for (i = 2*numList; i < 2*numList + numSubnode; i++) {
	AMDisclaim(kt, vt, am->slot[i]);
	am->slot[i] = NULL;
    }

    ckfree(am);
}

/*
 *----------------------------------------------------------------------
 *
 * AMNew --
 *
 * 	Create an ArrayMap with space for numList lists and
 *	numSubnode subnodes, and initialize with mask and id.
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
    int numList,
    int numSubnode,
    size_t mask,
    size_t id)
{
    ArrayMap new = ckalloc(AMN_SIZE(numList, numSubnode));

    new->claim = 0;
    new->mask = mask;
    new->id = id;
    return new;
}
/*
 *----------------------------------------------------------------------
 *
 * AMNewParent --
 *
 * 	Create an ArrayMap to serve as a container for
 * 	two ArrayMap subnodes.
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
ArrayMap AMNewParent(
    ArrayMap one,
    ArrayMap two)
{
    /* Mask used to carve out branch index. */
    const int branchMask = (branchFactor - 1);

    /* Bits in a index selecting a child of a node */
    const int branchShift = TclMSB(branchFactor);

    /* The depth of the tree for the node we must create.
     * Determine by lowest bit where hashes differ. */
    int depth = LSB(one->id ^ two->id) / branchShift;

    /* Compute the mask for all nodes at this depth */
    size_t mask = ((size_t)1 << (depth * branchShift)) - 1;

    int idx1 = (one->id >> (depth * branchShift)) & branchMask;
    int idx2 = (two->id >> (depth * branchShift)) & branchMask;
    ArrayMap new = AMNew(0, 2, mask, one->id & mask);

    assert ( idx1 != idx2 );
    assert ( (two->id & mask) == new->id );

    new->kvMap = 0;
    new->amMap = ((size_t)1 << idx1) | ((size_t)1 << idx2);

    AMClaim(one);
    AMClaim(two);

    if (idx1 < idx2) {
	new->slot[0] = one;
	new->slot[1] = two;
    } else {
	new->slot[0] = two;
	new->slot[1] = one;
    }
    return new;
}
/*
 *----------------------------------------------------------------------
 *
 * AMNewBranch --
 *
 * 	Create an ArrayMap to serve as a container for
 * 	one KVList and one ArrayMap subnode.
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

    /* Compute the mask for all nodes at this depth */
    size_t mask = ((size_t)1 << (depth * branchShift)) - 1;

    int idx1 = (hash >> (depth * branchShift)) & branchMask;
    int idx2 = (sub->id >> (depth * branchShift)) & branchMask;
    ArrayMap new = AMNew(1, 1, mask, hash & mask);

    assert ( idx1 != idx2 );
    assert ( (sub->id & mask) == new->id );

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

    /* Compute the mask for all nodes at this depth */
    size_t mask = ((size_t)1 << (depth * branchShift)) - 1;

    int idx1 = (hash1 >> (depth * branchShift)) & branchMask;
    int idx2 = (hash2 >> (depth * branchShift)) & branchMask;

    ArrayMap new = AMNew(2, 0, mask, hash1 & mask);

    assert ( idx1 != idx2 );
    assert ( (hash2 & mask) == new->id );

    new->kvMap = ((size_t)1 << idx1) | ((size_t)1 << idx2);
    new->amMap = 0;

    KVLClaim(l1);
    KVLClaim(l2);

    hashes = (size_t *)&(new->slot);
    lists = (KVList *) (hashes + 2);
    if (idx1 < idx2) {
	hashes[0] = hash1;
	hashes[1] = hash2;
	lists[0] = l1;
	lists[1] = l2;
    } else {
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
    const TclHAMTKeyType *kt,
    ArrayMap am,
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

	l = KVLFind(kt, am->slot[offset + NumBits(am->kvMap)], key);
	return l ? l->value : NULL;
    }
    if (tally & am->amMap) {
	/* Hash is consistent with one of our subnode children... */

	return AMFetch(kt, am->slot[2 * NumBits(am->kvMap)
		+ NumBits(am->amMap & (tally - 1))], hash, key);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * AMMergeList --
 *	Merge a node and list together to make a node.
 *
 * Results:
 *	The node created by the merge.
 *
 *----------------------------------------------------------------------
 */

static
ArrayMap AMMergeList(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    ArrayMap am,
    size_t hash,
    KVList kvl,
    ClientData *valuePtr,
    int listIsFirst)
{
    /* Mask used to carve out branch index. */
    const int branchMask = (branchFactor - 1);

    size_t tally;
    int numList, numSubnode, loffset, soffset, i;
    ClientData *src, *dst;
    ArrayMap new, sub;

    if ((am->mask & hash) != am->id) {
        /* Hash indicates list does not belong in this subtree */
        /* Create a new subtree to be parent of both am and kvl. */
        return AMNewBranch(am, hash, kvl);
    }

    /* Hash indicates key should be descendant of am, which? */
    tally = (size_t)1 << ((hash >> LSB(am->mask + 1)) & branchMask);

    numList = NumBits(am->kvMap);
    numSubnode = NumBits(am->amMap);
    loffset = NumBits(am->kvMap & (tally - 1));
    soffset = NumBits(am->amMap & (tally - 1));
    src = am->slot;

    if (tally & am->kvMap) {
	/* Hash consistent with existing list child */

	if (am->slot[loffset] == (ClientData)hash) {
	    /* Hash of list child matches. Merge to it. */
	    KVList l;

	    if (listIsFirst) {
		l = KVLMerge(kt, vt, kvl, am->slot[loffset + numList], valuePtr);
	    } else {
		l = KVLMerge(kt, vt, am->slot[loffset + numList], kvl, valuePtr);
	    }
	    if (l == am->slot[loffset + numList]) {
		return am;
	    }

	    new = AMNew(numList, numSubnode, am->mask, am->id);

	    new->kvMap = am->kvMap;
	    new->amMap = am->amMap;
	    dst = new->slot;

	    /* Copy all hashes */
	    for (i = 0; i < numList; i++) {
		*dst++ = *src++;
	    }

	    /* Copy all lists and insert one */
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
	/* Hashes do not match. Create Leaf to hold both. */
	sub = AMNewLeaf((size_t)am->slot[loffset], 
		(KVList)am->slot[loffset + numList], hash, kvl);

	/* Remove the list, Insert the leaf subnode */
	new = AMNew(numList - 1, numSubnode + 1, am->mask, am->id);

	new->kvMap = am->kvMap & ~tally;
	new->amMap = am->amMap | tally;
	dst = new->slot;
	/* hashes */
	for (i = 0; i < loffset; i++) {
	    *dst++ = *src++;
	}
	src++;
	for (i = loffset + 1; i < numList; i++) {
	    *dst++ = *src++;
	}
	/* lists */
	for (i = 0; i < loffset; i++) {
	    KVLClaim((KVList) *src);
	    *dst++ = *src++;
	}
	src++;
	for (i = loffset + 1; i < numList; i++) {
	    KVLClaim((KVList) *src);
	    *dst++ = *src++;
	}
	/* subnodes */
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
    }
    if (tally & am->amMap) {
	/* Hash consistent with existing subnode child */

	/* Merge the list into that subnode child... */
	sub = AMMergeList(kt, vt, (ArrayMap)am->slot[2*numList + soffset],
		hash, kvl, valuePtr, listIsFirst);
	if (sub == am->slot[2*numList + soffset]) {
	    /* Subnode unchanged, map unchanged, just return */
	    return am;
	}

	/* Copy slots, replacing the subnode with the merge result */
	new = AMNew(numList, numSubnode, am->mask, am->id);

	new->kvMap = am->kvMap;
	new->amMap = am->amMap;
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

	/* Copy all subnodes, replacing one */
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

    /* am is not using this tally; copy am and add it. */
    new = AMNew(numList + 1, numSubnode, am->mask, am->id);

    new->kvMap = am->kvMap | tally;
    new->amMap = am->amMap;
    dst = new->slot;

    /* Copy all hashes and insert one */
    for (i = 0; i < loffset; i++) {
	*dst++ = *src++;
    }
    *dst++ = (ClientData)hash;
    for (i = loffset; i < numList; i++) {
	*dst++ = *src++;
    }

    /* Copy all lists and insert one */
    for (i = 0; i < loffset; i++) {
	KVLClaim((KVList) *src);
	*dst++ = *src++;
    }
    KVLClaim(kvl);
    *dst++ = kvl;
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

/* Forward declaration for mutual recursion */
static ArrayMap		AMMerge(const TclHAMTKeyType *kt,
			    const TclHAMTValueType *vt, ArrayMap one,
			    ArrayMap two);


/*
 *----------------------------------------------------------------------
 *
 * AMMergeContents --
 *	Merge the contents of two nodes with same id together to make
 *	a single node with the union of children.
 *
 * Results:
 *	The node created by the merge.
 *
 *----------------------------------------------------------------------
 */

static
ArrayMap AMMergeContents(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    ArrayMap one,
    ArrayMap two)
{
    ArrayMap new;
    int numList, numSubnode;

    /* If either tree has a particular subnode, the merger must too */
    size_t amMap = one->amMap | two->amMap;

    /* If only one of two has list child, the merge will too, providing
     * there's not already a subnode claiming the slot. */
    size_t kvMap = (one->kvMap ^ two->kvMap) & ~amMap;

    size_t tally;
    ClientData *src1= one->slot;
    ClientData *src2 = two->slot;
    ClientData *dst;

    int numList1 = NumBits(one->kvMap);
    int numList2 = NumBits(two->kvMap);

    for (tally = (size_t)1; tally; tally = tally << 1) {
	if (!(tally & (amMap | kvMap))) {
	    assert ((one->amMap & tally)== 0);	/* would be in amMap */
	    assert ((two->amMap & tally)== 0);	/* would be in amMap */
	    assert ((one->kvMap & tally) == (two->kvMap & tally));

	    /* Remaining case is when both have list child @ tally ... */
	    if (tally & one->kvMap) {
		if (*src1 == *src2) {
	    /* When hashes same, this will make a merged list in merge. */
		    kvMap = kvMap | tally;
		} else {
	    /* When hashes differ, this will make a subnode child in merge. */
		    amMap = amMap | tally;
		}
	    }
	}
	if (tally & one->kvMap) {
	    src1++;
	}
	if (tally & two->kvMap) {
	    src2++;
	}
    }

    /* We now have the maps for the merged node. */
    numList = NumBits(kvMap);
    numSubnode = NumBits(amMap);
    new = AMNew(numList, numSubnode, one->mask, one->id);

    new->kvMap = kvMap;
    new->amMap = amMap;
    dst = new->slot;

    /* Copy the hashes */
    src1 = one->slot;
    src2 = two->slot;
    for (tally = (size_t)1; tally; tally = tally << 1) {
	if (tally & kvMap) {
	    if (tally & one->kvMap) {
		*dst = *src1;
	    }
	    if (tally & two->kvMap) {
		*dst = *src2;
	    }
	    dst++;
	}
	if (tally & one->kvMap) {
	    src1++;
	}
	if (tally & two->kvMap) {
	    src2++;
	}
    }

assert( src1 == one->slot + NumBits(one->kvMap) );
assert( src2 == two->slot + NumBits(two->kvMap) );

    /* Copy/merge the lists */
    for (tally = (size_t)1; tally; tally = tally << 1) {
	if (tally & kvMap) {
	    if ((tally & one->kvMap) && (tally & two->kvMap)) {
		KVList l = KVLMerge(kt, vt, *src1, *src2, NULL);
		KVLClaim(l);
		*dst++ = l;
	    } else if (tally & one->kvMap) {
		KVLClaim(*src1);
		*dst++ = *src1;
	    } else {
		assert (tally & two->kvMap);
		KVLClaim(*src2);
		*dst++ = *src2;
	    }
	}
	if (tally & one->kvMap) {
	    src1++;
	}
	if (tally & two->kvMap) {
	    src2++;
	}
    }

assert( src1 == one->slot + 2*NumBits(one->kvMap) );
assert( src2 == two->slot + 2*NumBits(two->kvMap) );

    /* Copy/merge the subnodes */
    for (tally = (size_t)1; tally; tally = tally << 1) {
	if (tally & amMap) {
	    ArrayMap am;
	    int loffset1, loffset2;
	    size_t hash1, hash2;
	    KVList l1, l2;

	    if ((tally & one->amMap) && (tally & two->amMap)) {
		am = AMMerge(kt, vt, *src1++, *src2++);
		AMClaim(am);
		*dst++ = am;
	    } else if (tally & one->amMap) {
		if (tally & two->kvMap) {
		    loffset2 = NumBits(two->kvMap & (tally - 1));
		    hash2 = (size_t)two->slot[loffset2];
		    l2 = two->slot[loffset2 + numList2];

		    am = AMMergeList(kt, vt, *src1++, hash2, l2, NULL, 0);
		} else {
		    am = *src1++;
		}
		AMClaim(am);
		*dst++ = am;
	    } else if (tally & two->amMap) {
		if (tally & one->kvMap) {
		    loffset1 = NumBits(one->kvMap & (tally - 1));
		    hash1 = (size_t)one->slot[loffset1];
		    l1 = one->slot[loffset1 + numList1];
		    am = AMMergeList(kt, vt, *src2++, hash1, l1, NULL, 0);
		} else {
		    am = *src2++;
		}
		AMClaim(am);
		*dst++ = am;
	    } else {
		/* TRICKY!!! Have to create node from two lists. */
		loffset1 = NumBits(one->kvMap & (tally - 1));
		loffset2 = NumBits(two->kvMap & (tally - 1));
		hash1 = (size_t)one->slot[loffset1];
		hash2 = (size_t)two->slot[loffset2];
		l1 = one->slot[loffset1 + numList1];
		l2 = two->slot[loffset2 + numList2];

		am = AMNewLeaf(hash1, l1, hash2, l2);
		AMClaim(am);
		*dst++ = am;
	    }
	}
    }
    return new;
}

/*
 *----------------------------------------------------------------------
 *
 * AMMergeDescendant --
 *	Merge two nodes together where one is an ancestor.
 *
 * Results:
 *	The node created by the merge.
 *
 *----------------------------------------------------------------------
 */

static
ArrayMap AMMergeDescendant(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    ArrayMap ancestor,
    ArrayMap descendant,
    int ancestorIsFirst)
{
    const int branchMask = (branchFactor - 1);
    int numList = NumBits(ancestor->kvMap);
    int numSubnode = NumBits(ancestor->amMap);
    size_t tally = (size_t)1 << (
	    (descendant->id >> LSB(ancestor->mask + 1)) & branchMask);
    int i;
    int loffset = NumBits(ancestor->kvMap & (tally - 1));
    int soffset = NumBits(ancestor->amMap & (tally - 1));
    ClientData *dst, *src = ancestor->slot;

    ArrayMap new, sub;

    if (tally & ancestor->kvMap) {
	/* Already have list child there. Must merge them. */
	sub = AMMergeList(kt, vt, descendant, (size_t)ancestor->slot[loffset],
		ancestor->slot[loffset + numList], NULL, ancestorIsFirst);
	new = AMNew(numList - 1, numSubnode + 1, ancestor->mask, ancestor->id);

	new->kvMap = ancestor->kvMap & ~tally;
	new->amMap = ancestor->amMap | tally;
	dst = new->slot;

	/* hashes */
	for (i = 0; i < loffset; i++) {
	    *dst++ = *src++;
	}
	src++;
	for (i = loffset + 1; i < numList; i++) {
	    *dst++ = *src++;
	}
	/* lists */
	for (i = 0; i < loffset; i++) {
	    KVLClaim((KVList) *src);
	    *dst++ = *src++;
	}
	src++;
	/* lists */
	for (i = loffset + 1; i < numList; i++) {
	    KVLClaim((KVList) *src);
	    *dst++ = *src++;
	}
	/* subnodes */
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
    }
    if (tally & ancestor->amMap) {
	/* Already have node child there. Must merge them. */
	if (ancestorIsFirst) {
	    sub = AMMerge(kt, vt,
		    ancestor->slot[2*numList + soffset], descendant);
	} else {
	    sub = AMMerge(kt, vt, descendant,
		    ancestor->slot[2*numList + soffset]);
	}
	if (sub == ancestor->slot[2*numList + soffset]) {
	    return ancestor;
	}
	new = AMNew(numList, numSubnode, ancestor->mask, ancestor->id);

	new->kvMap = ancestor->kvMap;
	new->amMap = ancestor->amMap;
	dst = new->slot;

	/* hashes */
	for (i = 0; i < numList; i++) {
	    *dst++ = *src++;
	}
	/* lists */
	for (i = 0; i < numList; i++) {
	    KVLClaim((KVList) *src);
	    *dst++ = *src++;
	}
	/* subnodes */
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
    /* Nothing in the way. Make modified copy with new subnode */
    new = AMNew(numList, numSubnode + 1, ancestor->mask, ancestor->id);

    new->kvMap = ancestor->kvMap;
    new->amMap = ancestor->amMap | tally;
    dst = new->slot;

    /* hashes */
    for (i = 0; i < numList; i++) {
	*dst++ = *src++;
    }
    /* lists */
    for (i = 0; i < numList; i++) {
	KVLClaim((KVList) *src);
	*dst++ = *src++;
    }
    /* subnodes */
    for (i = 0; i < soffset; i++) {
	AMClaim((ArrayMap) *src);
	*dst++ = *src++;
    }
    AMClaim(descendant);
    *dst++ = descendant;
    for (i = soffset; i < numSubnode; i++) {
	AMClaim((ArrayMap) *src);
	*dst++ = *src++;
    }
    return new;
}

/*
 *----------------------------------------------------------------------
 *
 * AMMerge --
 *	Merge two nodes together to make a node.
 *
 * Results:
 *	The node created by the merge.
 *
 *----------------------------------------------------------------------
 */

static
ArrayMap AMMerge(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    ArrayMap one,
    ArrayMap two)
{
    if (one->mask == two->mask) {
	/* Nodes at the same depth ... */
	if (one->id == two->id) {
	    /* ... and the same id; merge contents */
	    return AMMergeContents(kt, vt, one, two);
	}
	/* ... but are not same. Create parent to contain both. */
	return AMNewParent(one, two);
    }
    if (one->mask < two->mask) {
	/* two is deeper than one... */
	if ((one->mask & two->id) == one->id) {
	    /* ... and is a descendant of one. */
	    return AMMergeDescendant(kt, vt, one, two, 1);
	}
	/* ...but is not a descendant. Create parent to contain both. */
	return AMNewParent(one, two);
    }
    /* one is deeper than two... */
    if ((two->mask & one->id) == two->id) {
	/* ... and is a descendant of two. */
	return AMMergeDescendant(kt, vt, two, one, 0);
    }
    return AMNewParent(one, two);
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
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    ArrayMap am,
    size_t hash,
    ClientData key,
    ClientData value,
    ClientData *valuePtr)
{
    KVList new = KVLInsert(kt, vt, NULL, key, value, valuePtr);
    ArrayMap result = AMMergeList(kt, vt, am, hash, new, valuePtr, 0);

    if (result == am) {
	/* No-op insert; discard new node */
	KVLClaim(new);
	KVLDisclaim(kt, vt, new);
    }
    return result;
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
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    ArrayMap am,
    size_t hash,
    ClientData key,
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
	l = KVLRemove(kt, vt, am->slot[loffset + numList], key, valuePtr);

	if (l == am->slot[loffset + numList]) {
	    /* list unchanged -> ArrayMap unchanged. */
	    return am;
	}

	/* Create new ArrayMap with removed KVList */

	if (l != NULL) {
	    /* Modified copy of am, list replaced. */
	    new = AMNew(numList, numSubnode, am->mask, am->id);

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
	    new = AMNew(numList - 1, numSubnode, am->mask, am->id);

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
	sub = AMRemove(kt, vt, (ArrayMap)am->slot[2*numList + soffset],
		hash, key, &subhash, &l, valuePtr);

	if (sub) {
	    /* Modified copy of am, subnode replaced. */
	    new = AMNew(numList, numSubnode, am->mask, am->id);

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

	    new = AMNew(numList + 1, numSubnode - 1, am->mask, am->id);

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

static
HAMT *HAMTNewList(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    KVList l,
    size_t hash)
{
    HAMT *new = TclHAMTCreate(kt, vt);
    KVLClaim(l);
    new->kvl = l;
    new->x.hash = hash;
    return new;
}

static
HAMT *HAMTNewRoot(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    ArrayMap am)
{
    HAMT *new = TclHAMTCreate(kt, vt);
    AMClaim(am);
    new->x.am = am;
    return new;
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
	KVLDisclaim(hPtr->kt, hPtr->vt, hPtr->kvl);
	hPtr->kvl = NULL;
	hPtr->x.hash = 0;
    } else if (hPtr->x.am) {
	AMDisclaim(hPtr->kt, hPtr->vt, hPtr->x.am);
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
 *	NOTE: This design is using NULL to indicate "not found".
 *	The implication is that NULL cannot be in the valid value range.
 *	This limits the value types this HAMT design can support.
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
	    KVList l = KVLFind(hPtr->kt, hPtr->kvl, key);
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
    return AMFetch(hPtr->kt, hPtr->x.am, Hash(hPtr, key), key);
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
    HAMT *hPtr = hamt;
    KVList l;
    ArrayMap am;

    if (hPtr->kvl) {
	/* Map holds a single KVList. Is it for the same hash? */
	size_t hash = Hash(hPtr, key);
	if (hPtr->x.hash == hash) {
	    /* Yes. Indeed we have a hash collision! This is the right
	     * KVList to insert our pair into. */
	    l = KVLInsert(hPtr->kt, hPtr->vt, hPtr->kvl, key, value, valuePtr);
	
	    if (l == hPtr->kvl) {
		/* list unchanged -> HAMT unchanged. */
		return hamt;
	    }

	    /* Construct a new HAMT with a new kvl */
	    return HAMTNewList(hPtr->kt, hPtr->vt, l, hash);
	}
	/* No. Insertion should not be into the singleton KVList.
	 * We get to build a tree out of the singleton KVList and
	 * a new list holding our new pair. */


	return HAMTNewRoot(hPtr->kt, hPtr->vt,
		AMNewLeaf(hPtr->x.hash, hPtr->kvl, hash,
		KVLInsert(hPtr->kt, hPtr->vt, NULL, key, value, valuePtr)));
    }
    if (hPtr->x.am == NULL) {
	/* Map is empty. No key is in it. Create singleton KVList
	 * out of new pair. */
	return HAMTNewList(hPtr->kt, hPtr->vt,
		KVLInsert(hPtr->kt, hPtr->vt, NULL, key, value, valuePtr),
		Hash(hPtr, key));
    }
    /* Map has a tree. Insert into it. */
    am = AMInsert(hPtr->kt, hPtr->vt, hPtr->x.am,
	    Hash(hPtr, key), key, value, valuePtr);
    if (am == hPtr->x.am) {
	/* Map did not change (overwrite same value) */
	return hamt;
    }
    return HAMTNewRoot(hPtr->kt, hPtr->vt, am);
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
    HAMT *hPtr = hamt;
    size_t hash;
    KVList l;
    ArrayMap am;

    if (hPtr->kvl) {
	/* Map holds a single KVList. Is it for the same hash? */
	if (hPtr->x.hash == Hash(hPtr, key)) {
	    /* Yes. Indeed we have a hash collision! This is the right
	     * KVList to remove our pair from. */

	    l = KVLRemove(hPtr->kt, hPtr->vt, hPtr->kvl, key, valuePtr);
	
	    if (l == hPtr->kvl) {
		/* list unchanged -> HAMT unchanged. */
		return hamt;
	    }

	    /* Construct a new HAMT with a new kvl */
	    if (l) {
		return HAMTNewList(hPtr->kt, hPtr->vt, l, hPtr->x.hash);
	    }
	    /* TODO: Implement a shared empty HAMT ? */
	    /* CAUTION: would need one for each set of key & value types */
	    return TclHAMTCreate(hPtr->kt, hPtr->vt);
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
    am = AMRemove(hPtr->kt, hPtr->vt, hPtr->x.am,
	    Hash(hPtr, key), key, &hash, &l, valuePtr);

    if (am == hPtr->x.am) {
	/* Removal was no-op. */
	return hamt;
    }

    if (am) {
	return HAMTNewRoot(hPtr->kt, hPtr->vt, am);
    }
    return HAMTNewList(hPtr->kt, hPtr->vt, l, hash);
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTMerge --
 *
 *	Merge two TclHAMTS.
 *
 * Results:
 *	The new TclHAMT, merger of two arguments.
 *
 *----------------------------------------------------------------------
 */

TclHAMT
TclHAMTMerge(
    TclHAMT one,
    TclHAMT two)
{
    ArrayMap am;

    /* Sanity check for incompatible customizations. */
    if ((one->kt != two->kt) || (one->vt != two->vt)) {
	/* For now, panic. Consider returning empty. */
	Tcl_Panic("Cannot merge incompatible HAMTs");
    }

    /* Merge to self */
    if (one == two) {
	return one;
    }

    /* Merge any into empty -> any */
    if ((one->kvl == NULL) && (one->x.am == NULL)) {
	return two;
    }

    /* Merge empty into any -> any */
    if ((two->kvl == NULL) && (two->x.am == NULL)) {
	return one;
    }

    /* Neither empty */
    if (one->kvl) {
	if (two->kvl) {
	    /* Both are lists. */
	    if (one->x.hash == two->x.hash) {
		/* Same hash -> merge the lists */
		KVList l = KVLMerge(one->kt, one->vt,
			one->kvl, two->kvl, NULL);

		if (l == one->kvl) {
		    return one;
		}
		if (l == two->kvl) {
		    return two;
		}
		return HAMTNewList(one->kt, one->vt, l, one->x.hash);
	    }
	    /* Different hashes; create leaf to hold pair */
	    return HAMTNewRoot(one->kt, one->vt,
		    AMNewLeaf(one->x.hash, one->kvl, two->x.hash, two->kvl));
	}
	/* two has a tree */
	am = AMMergeList(one->kt, one->vt,
		two->x.am, one->x.hash, one->kvl, NULL, 1);
	if (am == two->x.am) {
	    /* Merge gave back same tree. Avoid a copy. */
	    return two;
	}
	return HAMTNewRoot(one->kt, one->vt, am);
    }

    /* one has a tree */
    if (two->kvl) {
	am = AMMergeList(one->kt, one->vt,
		one->x.am, two->x.hash, two->kvl, NULL, 0);
	if (am == one->x.am) {
	    /* Merge gave back same tree. Avoid a copy. */
	    return one;
	}
	return HAMTNewRoot(one->kt, one->vt, am);
    }

    /* Merge two trees */
    am = AMMerge(one->kt, one->vt, one->x.am, two->x.am);
    if (am == one->x.am) {
	return one;
    }
    if (am == two->x.am) {
	return two;
    }
    return HAMTNewRoot(one->kt, one->vt, am);
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
    const int depth = CHAR_BIT * sizeof(size_t) / branchShift;

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
	am = (ArrayMap)(i->kvlv[1]);

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
