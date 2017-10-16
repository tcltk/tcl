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
 * Each KVNode contains a key, value pair.
 *
 * It also holds a claim counter to support value sharing. The current
 * implementation of the claim mechanism makes the overall structure
 * suitable for only single-threaded operations. Later refinements in
 * development are intended to ease this constraint.
 *
 * A previous implementation kept lists of pairs to account for hash
 * collisions.  With 64-bit hashes and any reasonable hash function,
 * hash collisions are not just uncommon, they are essentially impossible.
 * Instead of making every KVNode capable of handling arbitrary numbers
 * of pairs, we let each handle only one pair, and shift the burden of
 * taking care of collisions to the overall HAMT structure.
 */

typedef struct KVNode *KVList;
typedef struct AMNode *ArrayMap;
typedef struct CNode *Collision;

typedef struct CNode {
    size_t	claim;
    Collision	next;	
    KVList	l;
} CNode;

typedef struct KVNode {
    size_t	claim;	/* How many claims on this struct */
    ClientData	key;	/* Key... */
    ClientData	value;	/* ...and Value of this pair */
} KVNode;

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
    Collision			overflow;
} HAMT;

/*
 * The operations on a KVList:
 *	KVLClaim	Make a claim on the pair.
 *	KVLDisclaim	Release a claim on the pair.
 *	KVLNew		Node creation utility
 *	KVLFind		Find the KV Pair containing an equal key.
 *	KVLMerge	Bring together two KV pairs with same hash.
 *	KVLInsert	Create a new pair, merging new pair onto old one.
 *	KVLRemove	Create a new pair, removing any pair matching key.
 */

static
void KVLClaim(
    KVList l)
{
    assert ( l != NULL );
    l->claim++;
}

static
void KVLDisclaim(
    HAMT *hamt,
    KVList l)
{
    const TclHAMTKeyType *kt = hamt->kt;
    const TclHAMTValueType *vt = hamt->vt;

    assert ( l != NULL );
    if (--l->claim) {
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
    ckfree(l);
}

static 
int KVLEqualKeys(
    HAMT *hamt,
    ClientData key1,
    ClientData key2)
{
    const TclHAMTKeyType *kt = hamt->kt;
    return (key1 == key2)
	    || (kt && kt->isEqualProc && kt->isEqualProc( key1, key2) );
}

static
KVList KVLFind(
    HAMT *hamt,
    KVList l,
    ClientData key)
{
    assert ( l != NULL);

    if (KVLEqualKeys(hamt, l->key, key)) {
	return l;
    }
    if (hamt->overflow) {
	Collision p = hamt->overflow;
	while (p) {
	    if (KVLEqualKeys(hamt, p->l->key, key)) {
		return p->l;
	    }
	}
    }
    return NULL;
}

static
KVList KVLNew(
    HAMT *hamt,
    ClientData key,
    ClientData value)
{
    const TclHAMTKeyType *kt = hamt->kt;
    const TclHAMTValueType *vt = hamt->vt;
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
    return new;
}

static
void CNClaim(
    Collision c)
{
    assert ( c != NULL);
    c->claim++;
}

static
void CNDisclaim(
    HAMT *hamt,
    Collision c)
{
    assert ( c != NULL);
    if (--c->claim) {
	return;
    }
    KVLDisclaim(hamt, c->l);
    c->l = NULL;
    if (c->next) {
	CNDisclaim(hamt, c->next);
    }
    c->next = NULL;
    ckfree(c);
}

static
Collision CNNew(
    KVList l,
    Collision next)
{
    Collision new = ckalloc(sizeof(CNode));
    new->claim = 0;
    if (next) {
	CNClaim(next);
    }
    new->next = next;
    KVLClaim(l);
    new->l = l;
    return new;
}


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
    HAMT *hamt,
    ClientData key)
{
    return (hamt->kt && hamt->kt->hashProc)
	    ? hamt->kt->hashProc(key) : (size_t) key;
}

/*
 * Return the merge of argument KV pairs one and two.
 * Caller asserts that the two pairs belong to the same hash.
 * It is extremely likely they hold the same key. In that case, the
 * result of the merge is to return two, and the overwritten value
 * is extracted from one.
 * An overwrite by an identical KVNode is detected and turned into
 * a no-op.
 */
static
KVList KVLMerge(
    HAMT *hamt,
    KVList one,
    KVList two,
    Collision *scratchPtr,
    ClientData *valuePtr)
{
    assert ( two != NULL );
    if (one == NULL) {
	if (valuePtr) {
	    *valuePtr = NULL;
	}	
	return two;
    }
    if ( (one == two) || KVLEqualKeys(hamt, one->key, two->key) ) {
	if (valuePtr) {
	    *valuePtr = one->value;
	}
	return (one->value == two->value) ? one : two;
    }

    /* Argument two holds the KV pair we wish to store. It belongs
     * where one is, but one is occupying the slot, and one has a
     * different key.  two will have to wait in the overflow waiting
     * area until the slot opens up. We have to take a scan through
     * the overflow to see if we are overwriting.
     */

    *scratchPtr = CNNew(two, *scratchPtr);

    if (hamt->overflow) {
	Collision p = hamt->overflow;
	while (p) {
	    if ( (p->l == two) || KVLEqualKeys(hamt, p->l->key, two->key)) {
		if (valuePtr) {
		    *valuePtr = p->l->value;
		}
		return one;
	    }
	}
    }
    if (valuePtr) {
	*valuePtr = NULL;
    }
    return one;
}

static
KVList KVLInsert(
    HAMT *hamt,
    KVList l,
    ClientData key,
    ClientData value,
    Collision *scratchPtr,
    ClientData *valuePtr)
{
    KVList new = KVLNew(hamt, key, value);
    KVList result = KVLMerge(hamt, l, new, scratchPtr, valuePtr);

    if (result == l) {
	/* Two possibilities here. 1) "new" was an exact duplicate
	 * of "l", so overwriting did nothing. In that case, the
	 * lines below will discard "new" as it is not stored.
	 * 2) "new" had a different key which is a hash collision
	 * with the key in "l". The storage of "new" got shifted off
	 * to scratchPtr, and will go into the collision list. In that
	 * case, the lines below are a harmless toggle of a counter.
	 * An examination of scratchPtr could eliminate that if there's
	 * reason to think it matters. */
	KVLClaim(new);
	KVLDisclaim(hamt, new);
    }
    return result;
}

/*
 * Caller asserts that hash of key is same as hash of KVNode l.
 * It is extremely likely they hold the same key. In that case, the
 * result is NULL as we remove the KV Node matching key, and the
 * disappearing value is pulled from l.
 */
static
KVList KVLRemove(
    HAMT *hamt,
    KVList l,
    ClientData key,
    Collision *scratchPtr,
    ClientData *valuePtr)
{
    Collision p;
    size_t hash;

    assert ( l != NULL );

    if (KVLEqualKeys(hamt, l->key, key)) {
	if (valuePtr) {
	    *valuePtr = l->value;
	}
	if (hamt->overflow) {
	    /* We're opening up a slot. Make a pass through the
	     * overflow waiting area to see if anything is waiting
	     * for it. */
	    hash = Hash(hamt, key);
	    p = hamt->overflow;
	    while (p) {
		size_t compare = Hash(hamt, p->l->key);

		if (hash == compare) {
		    *scratchPtr = CNNew(p->l, *scratchPtr);
		    return p->l;
		}
		p = p->next;
	    }
	}
	return NULL;
    }

    /* The key we were seeking for removal did not match the key that was
     * stored in the slot.  This is a kind of hash collision.  We need to
     * look in the overflow area in case they key we seek to remove might
     * be waiting there. */
     
    if (hamt->overflow) {
	p = hamt->overflow;
	while (p) {
	    if (KVLEqualKeys(hamt, p->l->key, key)) {
		if (valuePtr) {
		    *valuePtr = p->l->value;
		}
		*scratchPtr = CNNew(p->l, *scratchPtr);
		return l;
	    }
	    p = p->next;
	}
    }

    /* The key is not here. Nothing to remove. Return unchanged list. */
    if (valuePtr) {
	*valuePtr = NULL;
    }
    return l;
}

/*
 * Each interior node of the trie is an ArrayMap.
 *
 * In concept each ArrayMap stands for a single interior node in the
 * complete trie.  The mask and id fields identify which node it is.
 * The masks are cleared high bits followed by set low bits.  The number
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

typedef struct AMNode {
    size_t	claim;	/* How many claims on this struct */
    size_t	size;	/* How many pairs stored under this node? */
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
    HAMT *hamt,
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
	KVLDisclaim(hamt, am->slot[i]);
	am->slot[i] = NULL;
    }
    for (i = 2*numList; i < 2*numList + numSubnode; i++) {
	AMDisclaim(hamt, am->slot[i]);
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
    new->size = 0;
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

    new->size = one->size + two->size;
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

    new->size = sub->size + 1;
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

    new->size = 2;
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
    HAMT *hamt,
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

	l = KVLFind(hamt, am->slot[offset + NumBits(am->kvMap)], key);
	return l ? l->value : NULL;
    }
    if (tally & am->amMap) {
	/* Hash is consistent with one of our subnode children... */

	return AMFetch(hamt, am->slot[2 * NumBits(am->kvMap)
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
    HAMT *hamt,
    ArrayMap am,
    size_t hash,
    KVList kvl,
    Collision *scratchPtr,
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
	    KVList l, child = (KVList) am->slot[loffset + numList];

	    if (kvl == child) {
		if (valuePtr) {
		    *valuePtr = kvl->value;
		}
		return am;
	    }

	    if (listIsFirst) {
		l = KVLMerge(hamt, kvl, child, scratchPtr, valuePtr);
		if ((l == kvl) && (kvl->key == child->key)
			&& (kvl->value == child->value)) {
		    KVLClaim(kvl);
		    KVLDisclaim(hamt, child);
		    am->slot[loffset + numList] = kvl;
		}
	    } else {
		l = KVLMerge(hamt, child, kvl, scratchPtr, valuePtr);
		if ((l == child) && (kvl->key == child->key)
			&& (kvl->value == child->value)) {
		    KVLClaim(kvl);
		    KVLDisclaim(hamt, child);
		    am->slot[loffset + numList] = kvl;
		}
	    }
	    if (l == child) {
		return am;
	    }

	    new = AMNew(numList, numSubnode, am->mask, am->id);

	    new->size = am->size;
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

	new->size = am->size + 1;

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
	ArrayMap child = (ArrayMap)am->slot[2*numList + soffset];

	/* Merge the list into that subnode child... */
	sub = AMMergeList(hamt, child, hash, kvl,
		scratchPtr, valuePtr, listIsFirst);
	if (sub == child) {
	    /* Subnode unchanged, map unchanged, just return */
	    return am;
	}

	/* Copy slots, replacing the subnode with the merge result */
	new = AMNew(numList, numSubnode, am->mask, am->id);

	new->size = am->size - child->size + sub->size;

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

    new->size = am->size + 1;
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
static ArrayMap		AMMerge(HAMT *hamt, ArrayMap one, ArrayMap two,
			    Collision *scratchPtr);


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
    HAMT *hamt,
    ArrayMap one,
    ArrayMap two,
    Collision *scratchPtr)
{
    ArrayMap new;
    int numList, numSubnode;
    size_t size = 0;

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

    assert ( one != two );

    /* TODO: Detect cases where one or two can be returned as the
     * merge result, and do so.  If identical parts are detected,
     * eliminate duplication. */

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

    /* Check for case where merge is same as one */
    src1 = one->slot;
    src2 = two->slot;
    if ((kvMap == one->kvMap) && (amMap = one->amMap)) {
	src1 += numList1;
	src2 += numList2;
	for (tally = (size_t)1; tally; tally = tally << 1) {
	    KVList l;

	    if ((tally & kvMap) == 0) {
		continue;
	    }
	    if ((tally & two->kvMap) == 0) {
		/* Merge empty to one -> one */
		src1++;
		continue;
	    }
	    if (*src1 == *src2) {
		/* Merge one to one -> one */
		src1++;
		src2++;
		continue;
	    }

	    /* General merge - check result */
	    l = KVLMerge(hamt, *src1, *src2, scratchPtr, NULL);
	    if (l != *src1) {
		goto notOne;
	    }
	    /* Check whether *src1 and *src2 are identical values.
	     * If so, Make them the same value. */
	    if ((l->key == ((KVList)(*src2))->key)
		    && (l->value == ((KVList)(*src2))->value)) {
		KVLClaim(l);
		KVLDisclaim(hamt, *src2);
		*src2 = l;
	    }
	    src1++;
	    src2++;
	}

	for (tally = (size_t)1; tally; tally = tally << 1) {
	    ArrayMap am;

	    if ((tally & amMap) == 0) {
		continue;
	    }
	    if (tally & two->amMap) {
		if (*src1 == *src2) {
		    /* Merge one into one -> one */
		    src1++;
		    src2++;
		    continue;
		}
		am = AMMerge(hamt, *src1, *src2, scratchPtr);
		if (am != *src1) {
		    goto notOne;
		}
		/* TODO: Check identity and replace */

	    } else if (tally & two->kvMap) {
		int loffset = NumBits(two->kvMap & (tally - 1));
		size_t hash = (size_t)two->slot[loffset];
		KVList l = two->slot[loffset + numList2];

		am = AMMergeList(hamt, *src1, hash, l, scratchPtr, NULL, 0);
		if (am != *src1) {
		    goto notOne;
		}
	    }
	    src1++;
	    src2++;
	}
	/* If you get here, congrats! the merge is same as one */
	return one;
    }

  notOne:
    /* src1 points to first failed slot */
    /* TODO: copy over known stuff */

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
	    if ((tally & one->kvMap) && (tally & two->kvMap)
		    && (*src1 != *src2)) {
		KVList l = KVLMerge(hamt, *src1, *src2, scratchPtr, NULL);
		if (l == *src1) {
		    /* Check whether *src1 and *src2 are identical values.
		     * If so, Make them the same value. */
		    if ((l->key == ((KVList)(*src2))->key)
			    && (l->value == ((KVList)(*src2))->value)) {
			KVLClaim(l);
			KVLDisclaim(hamt, *src2);
			*src2 = l;
		    }
		}
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
		am = AMMerge(hamt, *src1++, *src2++, scratchPtr);
		AMClaim(am);
		*dst++ = am;
	    } else if (tally & one->amMap) {
		if (tally & two->kvMap) {
		    loffset2 = NumBits(two->kvMap & (tally - 1));
		    hash2 = (size_t)two->slot[loffset2];
		    l2 = two->slot[loffset2 + numList2];

		    am = AMMergeList(hamt, *src1++, hash2, l2,
			    scratchPtr, NULL, 0);
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
		    am = AMMergeList(hamt, *src2++, hash1, l1,
			    scratchPtr, NULL, 1);
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
	    size += am->size;
	}
    }
    new->size = size + numList;
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
    HAMT *hamt,
    ArrayMap ancestor,
    ArrayMap descendant,
    Collision *scratchPtr,
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
	sub = AMMergeList(hamt, descendant, (size_t)ancestor->slot[loffset],
		ancestor->slot[loffset + numList],
		scratchPtr, NULL, ancestorIsFirst);
	new = AMNew(numList - 1, numSubnode + 1, ancestor->mask, ancestor->id);

	new->size = ancestor->size - 1 + sub->size;
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
	ArrayMap child = ancestor->slot[2*numList + soffset];

	/* Already have node child there. Must merge them. */
	if (ancestorIsFirst) {
	    sub = AMMerge(hamt, child, descendant, scratchPtr);
	} else {
	    sub = AMMerge(hamt, descendant, child, scratchPtr);
	}
	if (sub == child) {
	    return ancestor;
	}
	new = AMNew(numList, numSubnode, ancestor->mask, ancestor->id);

	new->size = ancestor->size - child->size + sub->size;
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

    new->size = ancestor->size + descendant->size;
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
    HAMT *hamt,
    ArrayMap one,
    ArrayMap two,
    Collision *scratchPtr)
{
    if (one->mask == two->mask) {
	/* Nodes at the same depth ... */
	if (one->id == two->id) {
	    /* ... and the same id; merge contents */
	    if (one == two) {
		return one;
	    }
	    return AMMergeContents(hamt, one, two, scratchPtr);
	}
	/* ... but are not same. Create parent to contain both. */
	return AMNewParent(one, two);
    }
    if (one->mask < two->mask) {
	/* two is deeper than one... */
	if ((one->mask & two->id) == one->id) {
	    /* ... and is a descendant of one. */
	    return AMMergeDescendant(hamt, one, two, scratchPtr, 1);
	}
	/* ...but is not a descendant. Create parent to contain both. */
	return AMNewParent(one, two);
    }
    /* one is deeper than two... */
    if ((two->mask & one->id) == two->id) {
	/* ... and is a descendant of two. */
	return AMMergeDescendant(hamt, two, one, scratchPtr, 0);
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
    HAMT *hamt,
    ArrayMap am,
    size_t hash,
    ClientData key,
    ClientData value,
    Collision *scratchPtr,
    ClientData *valuePtr)
{
    KVList new = KVLInsert(hamt, NULL, key, value, NULL, valuePtr);
    ArrayMap result = AMMergeList(hamt, am, hash, new, scratchPtr, valuePtr, 0);

    if (result == am) {
	/* No-op insert; discard new node */
	KVLClaim(new);
	KVLDisclaim(hamt, new);
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
    HAMT *hamt,
    ArrayMap am,
    size_t hash,
    ClientData key,
    Collision *scratchPtr,
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
	l = KVLRemove(hamt, am->slot[loffset + numList], key,
		scratchPtr, valuePtr);

	if (l == am->slot[loffset + numList]) {
	    /* list unchanged -> ArrayMap unchanged. */
	    return am;
	}

	/* Create new ArrayMap with removed KVList */

	if (l != NULL) {
	    /* Modified copy of am, list replaced. */
	    new = AMNew(numList, numSubnode, am->mask, am->id);

	    new->size = am->size - 1;
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

	    new->size = am->size - 1;
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
	ArrayMap child;

	/* Hash is consistent with one of our subnode children... */
	soffset = NumBits(am->amMap & (tally - 1));
	child = am->slot[2*numList + soffset];
	sub = AMRemove(hamt, child, hash, key,
		scratchPtr, &subhash, &l, valuePtr);

	if (sub) {
	    /* Modified copy of am, subnode replaced. */
	    new = AMNew(numList, numSubnode, am->mask, am->id);

	    new->size = am->size - child->size + sub->size;
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

	    new->size = am->size - child->size + 1;

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
    HAMT *hamt = ckalloc(sizeof(HAMT));

    hamt->claim = 0;
    hamt->kt = kt;
    hamt->vt = vt;
    hamt->kvl = NULL;
    hamt->x.am = NULL;
    hamt->overflow = NULL;
    return hamt;
}

static
HAMT *HAMTNewList(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    KVList l,
    size_t hash,
    Collision overflow)
{
    HAMT *new = TclHAMTCreate(kt, vt);
    KVLClaim(l);
    new->kvl = l;
    new->x.hash = hash;
    if (overflow) {
	CNClaim(overflow);
    }
    new->overflow = overflow;
    return new;
}

static
HAMT *HAMTNewRoot(
    const TclHAMTKeyType *kt,
    const TclHAMTValueType *vt,
    ArrayMap am,
    Collision overflow)
{
    HAMT *new = TclHAMTCreate(kt, vt);
    AMClaim(am);
    new->x.am = am;
    if (overflow) {
	CNClaim(overflow);
    }
    new->overflow = overflow;
    return new;
}

/*
 * This routine is some post-processing after a merge or insert operation
 * on one and possibly two has resulted in a hash collision leaving some
 * KVPair(s) in the scratch list.  We must produce the collision overflow
 * for the hamt result of the merge or insert operation.
 */
static
Collision CollisionMerge(
    HAMT *one,
    HAMT *two,
    Collision scratch)
{
    /* Everything on the scratch list will be in the final overflow list */
    Collision p, result = scratch;

    /* A pair on the second overflow list will be on the final list unless
     * its key is already on the scratch list. */
    if (two && two->overflow) {
	if (scratch == NULL) {
	    result = two->overflow;
	} else {
	p = two->overflow;
	while (p) {
	    Collision q = scratch;
	    while (q) {
		if (KVLEqualKeys(one, p->l->key, q->l->key)) {
		    break;
		}
		q = q->next;
	    }
	    if (q == NULL) {
		result = CNNew(p->l, result);
	    }
	    p = p->next;
	}
	}
	scratch = result;
    }
    /* A pair on the first overflow list will be on the final list unless
     * its key is already on the scratch list. */
    if (one && one->overflow) {
	if (scratch == NULL) {
	    result = one->overflow;
	} else {
	p = one->overflow;
	while (p) {
	    Collision q = scratch;
	    while (q) {
		if (KVLEqualKeys(one, p->l->key, q->l->key)) {
		    break;
		}
		q = q->next;
	    }
	    if (q == NULL) {
		result = CNNew(p->l, result);
	    }
	    p = p->next;
	}
	}
    }

    return result;
}

/*
 * This routine is some post-processing after a remove operation
 * on hamt has resulted in a hash collision leaving some
 * KVPair(s) in the scratch list.  We must produce the collision overflow
 * left when the item on the scratch list is removed from hamt->overflow.
 */
static
Collision CollisionRemove(
    HAMT *hamt,
    Collision scratch)
{
    Collision result, last, p = hamt->overflow;
    if (scratch == NULL) {
	return hamt->overflow;
    }
    if (scratch->next != NULL) {
	Tcl_Panic("Remove op put multiple items on the scratch list");
    }
    if (p == NULL) {
	Tcl_Panic("Empty collision list, but item on scratch");
    }
    if (p->l == scratch->l) {
	return p->next;
    }
    result = last = CNNew(p->l, NULL);
    while (p->next) {
	p = p->next;
	if (p->l == scratch->l) {
	    last->next = p->next;
	    return result;
	}
	last->next = CNNew(p->l, NULL);
	last = last->next;
    }
    Tcl_Panic("Scratch item not on the collision list");
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
    assert ( hamt != NULL );
    hamt->claim++;
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
    if (hamt == NULL) {
	return;
    }
    hamt->claim--;
    if (hamt->claim) {
	return;
    }
    if (hamt->kvl) {
	KVLDisclaim(hamt, hamt->kvl);
	hamt->kvl = NULL;
	hamt->x.hash = 0;
    } else if (hamt->x.am) {
	AMDisclaim(hamt, hamt->x.am);
	hamt->x.am = NULL;
    }
    if (hamt->overflow) {
	CNDisclaim(hamt, hamt->overflow);
    }
    hamt->kt = NULL;
    hamt->vt = NULL;
    ckfree(hamt);
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
    if (hamt->kvl) {
	/* Map holds a single KVList. Is it for the right hash? */
	if (hamt->x.hash == Hash(hamt, key)) {
	    /* Yes, try to find key in it. */
	    KVList l = KVLFind(hamt, hamt->kvl, key);
	    return l ? l->value : NULL;
	}
	/* No. Map doesn't hold the key. */
	return NULL;
    }
    if (hamt->x.am == NULL) {
	/* Map is empty. No key is in it. */
	return NULL;
    }
    /* Map has a tree. Fetch from it. */
    return AMFetch(hamt, hamt->x.am, Hash(hamt, key), key);
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
    KVList l;
    ArrayMap am;
    Collision scratch = NULL;

    if (hamt->kvl) {
	/* Map holds a single KVList. Is it for the same hash? */
	size_t hash = Hash(hamt, key);
	if (hamt->x.hash == hash) {
	    /* Yes. Indeed we have a hash collision! This is the right
	     * KVList to insert our pair into. */
	    l = KVLInsert(hamt, hamt->kvl, key, value, &scratch, valuePtr);
	    scratch = CollisionMerge(hamt, NULL, scratch);
	    if ((l == hamt->kvl) && (scratch == hamt->overflow)) {
		/* list unchanged -> HAMT unchanged. */
		return hamt;
	    }

	    /* Construct a new HAMT with a new kvl */
	    return HAMTNewList(hamt->kt, hamt->vt, l, hash, scratch);
	}
	/* No. Insertion should not be into the singleton KVList.
	 * We get to build a tree out of the singleton KVList and
	 * a new list holding our new pair. */


	return HAMTNewRoot(hamt->kt, hamt->vt,
		AMNewLeaf(hamt->x.hash, hamt->kvl, hash,
		KVLInsert(hamt, NULL, key, value, NULL, valuePtr)),
		hamt->overflow);
    }
    if (hamt->x.am == NULL) {
	/* Map is empty. No key is in it. Create singleton KVList
	 * out of new pair. */
	return HAMTNewList(hamt->kt, hamt->vt,
		KVLInsert(hamt, NULL, key, value, NULL, valuePtr),
		Hash(hamt, key), NULL);
    }
    /* Map has a tree. Insert into it. */
    am = AMInsert(hamt, hamt->x.am,
	    Hash(hamt, key), key, value, &scratch, valuePtr);
    scratch = CollisionMerge(hamt, NULL, scratch);
    if ((am == hamt->x.am) && (scratch == hamt->overflow)) {
	/* Map did not change (overwrite same value) */
	return hamt;
    }
    return HAMTNewRoot(hamt->kt, hamt->vt, am, scratch);
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
    size_t hash;
    KVList l;
    ArrayMap am;
    Collision scratch = NULL;

    if (hamt->kvl) {
	/* Map holds a single KVList. Is it for the same hash? */
	if (hamt->x.hash == Hash(hamt, key)) {
	    /* Yes. Indeed we have a hash collision! This is the right
	     * KVList to remove our pair from. */

	    l = KVLRemove(hamt, hamt->kvl, key, &scratch, valuePtr);
	
	    scratch = CollisionRemove(hamt, scratch);
	    if ((l == hamt->kvl) && (scratch == hamt->overflow)) {
		/* list unchanged -> HAMT unchanged. */
		return hamt;
	    }

	    /* Construct a new HAMT with a new kvl */
	    if (l) {
		return HAMTNewList(hamt->kt, hamt->vt,
			l, hamt->x.hash, scratch);
	    }
	    /* TODO: Implement a shared empty HAMT ? */
	    /* CAUTION: would need one for each set of key & value types */
	    return TclHAMTCreate(hamt->kt, hamt->vt);
	}

	/* The key is not in the only KVList we have. */
	if (valuePtr) {
	    *valuePtr = NULL;
	}
	return hamt;
    }
    if (hamt->x.am == NULL) {
	/* Map is empty. No key is in it. */
	if (valuePtr) {
	    *valuePtr = NULL;
	}
	return hamt;
    }

    /* Map has a tree. Remove from it. */
    am = AMRemove(hamt, hamt->x.am, Hash(hamt, key), key,
	    &scratch, &hash, &l, valuePtr);
    scratch = CollisionRemove(hamt, scratch);
    if ((am == hamt->x.am) && (scratch == hamt->overflow)) {
	/* Removal was no-op. */
	return hamt;
    }

    if (am) {
	return HAMTNewRoot(hamt->kt, hamt->vt, am, scratch);
    }
    return HAMTNewList(hamt->kt, hamt->vt, l, hash, scratch);
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
    Collision scratch = NULL;

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
		KVList l;
		KVList l1 = one->kvl;
		KVList l2 = two->kvl;

		if ((l1 == l2) && (two->overflow == NULL)) {
		    return one;
		}
		l = KVLMerge(one, l1, l2, &scratch, NULL);
		scratch = CollisionMerge(one, two, scratch);
		if (l == l1) {
		    /* Good possibility that one->kvl and two->kvl are
		     * identical values.  If so, make them the same value.
		     * NOTE: Do this only for identical values, not just
		     * equal values.  Equal, but distinct, keys should be
		     * preserved. */
		    if (l != l2 && (l->key == l2->key)
			    && (l->value == l2->value)) {
			/* Convert identical values to same values */
			KVLClaim(l);
			KVLDisclaim(one, l2);
			two->kvl = l;
		    }
		    if (scratch == one->overflow) {
			return one;
		    }
		}
		if ((l == l2) && (scratch == two->overflow)) {
		    return two;
		}

		return HAMTNewList(one->kt, one->vt, l, one->x.hash, scratch);
	    }
	    /* Different hashes; create leaf to hold pair */
	    return HAMTNewRoot(one->kt, one->vt,
		    AMNewLeaf(one->x.hash, one->kvl, two->x.hash, two->kvl),
		    CollisionMerge(one, two, NULL));
	}
	/* two has a tree */
	am = AMMergeList(one, two->x.am, one->x.hash, one->kvl,
		&scratch, NULL, 1);

	scratch = CollisionMerge(one, two, scratch);
	if ((am == two->x.am) && (scratch == two->overflow)) {
	    /* Merge gave back same tree. Avoid a copy. */
	    return two;
	}
	return HAMTNewRoot(one->kt, one->vt, am, scratch);
    }

    /* one has a tree */
    if (two->kvl) {
	am = AMMergeList(one, one->x.am, two->x.hash, two->kvl,
		&scratch, NULL, 0);
	scratch = CollisionMerge(one, two, scratch);
	if ((am == one->x.am) && (scratch == one->overflow)) {
	    /* Merge gave back same tree. Avoid a copy. */
	    return one;
	}
	return HAMTNewRoot(one->kt, one->vt, am, scratch);
    }

    /* Merge two trees */
    am = AMMerge(one, one->x.am, two->x.am, &scratch);
    scratch = CollisionMerge(one, two, scratch);
    if ((am == one->x.am) && (scratch == one->overflow)) {
	return one;
    }
    if ((am == two->x.am) && (scratch == two->overflow)) {
	return two;
    }
    return HAMTNewRoot(one->kt, one->vt, am, scratch);
}

/*
 *----------------------------------------------------------------------
 *
 * TclHAMTSize --
 *
 *	Size of a TclHAMT.
 *
 * Results:
 *	A size_t value holding the number of key value pairs in hamt.
 *
 *----------------------------------------------------------------------
 */

size_t
TclHAMTSize(
    TclHAMT hamt)
{
    size_t size = 0;
    if (hamt->kvl) {
	size = 1;
    } else if (hamt->x.am) {
	size = hamt->x.am->size;
    }
    if (hamt->overflow) {
	Collision p = hamt->overflow;
	while (p) {
	    size++;
	    p = p->next;
	}
    }
    return size;
}


/* A struct to hold our place as we iterate through a HAMT */

typedef struct Idx {
    TclHAMT	hamt;
    Collision	overflow;
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

    Idx *i;
    ArrayMap am;
    int n;

    assert ( hamt );

    if (hamt->kvl == NULL && hamt->x.am == NULL) {
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
    i->overflow = hamt->overflow;
    i->top = i->stack;

    if (hamt->kvl) {
	/* One bucket */
	/* Our place is the only place. Pointing at the sole bucket */
	i->kvlv = &(hamt->kvl);
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

static void
HAMTNext(
    TclHAMTIdx *idxPtr,
    size_t *histPtr)
{
    Idx *i = *idxPtr;
    ArrayMap am, popme;
    ClientData *slotPtr;
    int n, seen = 0;

    assert ( i );
    assert ( i->kvl );

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
	if (i->overflow) {
	    i->kvl = i->overflow->l;
	    i->overflow = i->overflow->next;
	    return;
	}
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
    popme = i->top[0];
    /* Account for nodes as they pop */
    if (histPtr) {
	histPtr[2*NumBits(popme->kvMap) + NumBits(popme->amMap)]++;
    }
 
    if (i->top == i->stack) {
	/* Nothing left to pop. Stack exhausted. We're done. */
	if (i->overflow) {
	    i->kvl = i->overflow->l;
	    i->overflow = i->overflow->next;
	    i->top[0] = NULL;
	    return;
	}
	TclHAMTDone((TclHAMTIdx) i);
	*idxPtr = NULL;
	return;
    }
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

void
TclHAMTNext(
    TclHAMTIdx *idxPtr)
{
    HAMTNext(idxPtr, NULL);
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
 *----------------------------------------------------------------------
 *
 * TclHAMTInfo --
 *
 *	Statistical information on a hamt.
 *
 * Results:
 *	A Tcl_Obj holding text description of storage stats.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclHAMTInfo(
    TclHAMT hamt)
{
    const int branchShift = TclMSB(branchFactor);
    const int depth = CHAR_BIT * sizeof(size_t) / branchShift;
    size_t *accum = ckalloc(depth*sizeof(size_t));
    size_t size = TclHAMTSize(hamt);
    double avg = 0.0;
    int i, collisions = 0;
    size_t nodes = 0;
    size_t numBytes = 0;
    size_t slots = 0;
    size_t hist[129];

    TclHAMTIdx idx = TclHAMTFirst(hamt);
    Tcl_Obj *result = Tcl_NewStringObj("hamt holds ", -1);
    Tcl_Obj *count = Tcl_NewWideIntObj((Tcl_WideInt)size);

    Tcl_AppendObjToObj(result, count);
    Tcl_AppendToObj(result, " pairs", -1);
    Tcl_DecrRefCount(count);

    for (i = 0; i < depth; i++) {
	accum[i] = 0;
    }
    for (i = 0; i < 129; i++) {
	hist[i] = 0;
    }

    while (idx) {
	accum[ idx->top - idx->stack ]++;
	HAMTNext(&idx, hist);
    }

    if (hamt->overflow) {
	Collision p = hamt->overflow;
	while (p) {
	    collisions++;
	    p = p->next;
	}
    }
    Tcl_AppendPrintfToObj(result, "\nnumber of collisions: %d", collisions);

    for (i = 0; i < 129; i++) {

	if (hist[i] == 0) continue;

	Tcl_AppendPrintfToObj(result, "\nnumber of nodes with %2d slots: ", i);
	count = Tcl_NewWideIntObj((Tcl_WideInt)hist[i]);
	Tcl_AppendObjToObj(result, count);
	Tcl_DecrRefCount(count);

	nodes += hist[i];
	slots += hist[i] * i;
	numBytes += hist[i] * ((i * sizeof(void *)) + sizeof(AMNode));
    }
    if (nodes) {
    Tcl_AppendToObj(result, "\nnumber of nodes: ", -1);
    count = Tcl_NewWideIntObj((Tcl_WideInt)nodes);
    Tcl_AppendObjToObj(result, count);
    Tcl_DecrRefCount(count);
    Tcl_AppendToObj(result, "\namount of node memory: ", -1);
    count = Tcl_NewWideIntObj((Tcl_WideInt)numBytes);
    Tcl_AppendObjToObj(result, count);
    Tcl_DecrRefCount(count);
	Tcl_AppendPrintfToObj(result, "\naverage slots per node: %g",
		(1.0 * slots)/nodes);
	Tcl_AppendPrintfToObj(result, "\naverage bytes per node: %g",
		(1.0 * numBytes)/nodes);
	Tcl_AppendPrintfToObj(result, "\naverage pairs per node: %g",
		(1.0 * size)/nodes);
    }

    if (size) {
    for (i = 0; i < depth; i++) {
	double fraction;

	if (accum[i] == 0) continue;
	Tcl_AppendPrintfToObj(result, "\nnumber of leafs at %2d hops: ", i);
	count = Tcl_NewWideIntObj((Tcl_WideInt)accum[i]);
	Tcl_AppendObjToObj(result, count);
	Tcl_DecrRefCount(count);

	fraction = (1.0 * accum[i]) / size;
	avg += i * fraction;
    }
    Tcl_AppendPrintfToObj(result, "\naverage hops: %g ", avg);
    }
    ckfree(accum);

    return result;
}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
