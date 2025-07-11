/*
 * tclDictObj.c --
 *
 *	This file contains functions that implement the Tcl dict object type
 *	and its accessor command.
 *
 * Copyright © 2002-2010 Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclTomMath.h"
#include <assert.h>

/*
 * Forward declaration.
 */
struct Dict;

/*
 * Prototypes for functions defined later in this file:
 */

static void			DeleteDict(struct Dict *dict);
static Tcl_ObjCmdProc		DictAppendCmd;
static Tcl_ObjCmdProc		DictCreateCmd;
static Tcl_ObjCmdProc		DictExistsCmd;
static Tcl_ObjCmdProc		DictFilterCmd;
static Tcl_ObjCmdProc		DictGetCmd;
static Tcl_ObjCmdProc		DictGetDefCmd;
static Tcl_ObjCmdProc		DictIncrCmd;
static Tcl_ObjCmdProc		DictInfoCmd;
static Tcl_ObjCmdProc		DictKeysCmd;
static Tcl_ObjCmdProc		DictLappendCmd;
static Tcl_ObjCmdProc		DictMergeCmd;
static Tcl_ObjCmdProc		DictRemoveCmd;
static Tcl_ObjCmdProc		DictReplaceCmd;
static Tcl_ObjCmdProc		DictSetCmd;
static Tcl_ObjCmdProc		DictSizeCmd;
static Tcl_ObjCmdProc		DictUnsetCmd;
static Tcl_ObjCmdProc		DictUpdateCmd;
static Tcl_ObjCmdProc		DictValuesCmd;
static Tcl_ObjCmdProc		DictWithCmd;
static Tcl_DupInternalRepProc	DupDictInternalRep;
static Tcl_FreeInternalRepProc	FreeDictInternalRep;
static void			InvalidateDictChain(Tcl_Obj *dictObj);
static Tcl_SetFromAnyProc	SetDictFromAny;
static Tcl_UpdateStringProc	UpdateStringOfDict;
static Tcl_AllocHashEntryProc	AllocChainEntry;
static inline void		InitChainTable(struct Dict *dict);
static inline void		DeleteChainTable(struct Dict *dict);
static inline Tcl_HashEntry *	CreateChainEntry(struct Dict *dict,
					Tcl_Obj *keyPtr, int *newPtr);
static inline int		DeleteChainEntry(struct Dict *dict,
					Tcl_Obj *keyPtr);
static Tcl_NRPostProc		FinalizeDictUpdate;
static Tcl_NRPostProc		FinalizeDictWith;
static Tcl_ObjCmdProc		DictForNRCmd;
static Tcl_ObjCmdProc		DictMapNRCmd;
static Tcl_NRPostProc		DictForLoopCallback;
static Tcl_NRPostProc		DictMapLoopCallback;

/*
 * Table of dict subcommand names and implementations.
 */

static const EnsembleImplMap implementationMap[] = {
    {"append",	DictAppendCmd,	TclCompileDictAppendCmd, NULL, NULL, 0 },
    {"create",	DictCreateCmd,	TclCompileDictCreateCmd, NULL, NULL, 0 },
    {"exists",	DictExistsCmd,	TclCompileDictExistsCmd, NULL, NULL, 0 },
    {"filter",	DictFilterCmd,	NULL, NULL, NULL, 0 },
    {"for",	NULL,		TclCompileDictForCmd, DictForNRCmd, NULL, 0 },
    {"get",	DictGetCmd,	TclCompileDictGetCmd, NULL, NULL, 0 },
    {"getdef",	DictGetDefCmd,	TclCompileDictGetWithDefaultCmd, NULL,NULL,0},
    {"getwithdefault",	DictGetDefCmd,	TclCompileDictGetWithDefaultCmd,
	NULL, NULL, 0 },
    {"incr",	DictIncrCmd,	TclCompileDictIncrCmd, NULL, NULL, 0 },
    {"info",	DictInfoCmd,	TclCompileBasic1ArgCmd, NULL, NULL, 0 },
    {"keys",	DictKeysCmd,	TclCompileBasic1Or2ArgCmd, NULL, NULL, 0 },
    {"lappend",	DictLappendCmd,	TclCompileDictLappendCmd, NULL, NULL, 0 },
    {"map",	NULL,		TclCompileDictMapCmd, DictMapNRCmd, NULL, 0 },
    {"merge",	DictMergeCmd,	TclCompileDictMergeCmd, NULL, NULL, 0 },
    {"remove",	DictRemoveCmd,	TclCompileDictRemoveCmd, NULL, NULL, 0 },
    {"replace",	DictReplaceCmd, TclCompileDictReplaceCmd, NULL, NULL, 0 },
    {"set",	DictSetCmd,	TclCompileDictSetCmd, NULL, NULL, 0 },
    {"size",	DictSizeCmd,	TclCompileBasic1ArgCmd, NULL, NULL, 0 },
    {"unset",	DictUnsetCmd,	TclCompileDictUnsetCmd, NULL, NULL, 0 },
    {"update",	DictUpdateCmd,	TclCompileDictUpdateCmd, NULL, NULL, 0 },
    {"values",	DictValuesCmd,	TclCompileBasic1Or2ArgCmd, NULL, NULL, 0 },
    {"with",	DictWithCmd,	TclCompileDictWithCmd, NULL, NULL, 0 },
    {NULL, NULL, NULL, NULL, NULL, 0}
};

/*
 * Internal representation of the entries in the hash table that backs a
 * dictionary.
 */

typedef struct ChainEntry {
    Tcl_HashEntry entry;
    struct ChainEntry *prevPtr;
    struct ChainEntry *nextPtr;
} ChainEntry;

/*
 * Internal representation of a dictionary.
 *
 * The internal representation of a dictionary object is a hash table (with
 * Tcl_Objs for both keys and values), a reference count and epoch number for
 * detecting concurrent modifications of the dictionary, and a pointer to the
 * parent object (used when invalidating string reps of pathed dictionary
 * trees) which is NULL in normal use. The fact that hash tables know (with
 * appropriate initialisation) already about objects makes key management /so/
 * much easier!
 *
 * Reference counts are used to enable safe iteration across hashes while
 * allowing the type of the containing object to be modified.
 */

typedef struct Dict {
    Tcl_HashTable table;	/* Object hash table to store mapping in. */
    ChainEntry *entryChainHead;	/* Linked list of all entries in the
				 * dictionary. Used for doing traversal of the
				 * entries in the order that they are
				 * created. */
    ChainEntry *entryChainTail;	/* Other end of linked list of all entries in
				 * the dictionary. Used for doing traversal of
				 * the entries in the order that they are
				 * created. */
    size_t epoch;		/* Epoch counter */
    size_t refCount;		/* Reference counter (see above) */
    Tcl_Obj *chain;		/* Linked list used for invalidating the
				 * string representations of updated nested
				 * dictionaries. */
} Dict;

/*
 * The structure below defines the dictionary object type by means of
 * functions that can be invoked by generic object code.
 */

const Tcl_ObjType tclDictType = {
    "dict",
    FreeDictInternalRep,	/* freeIntRepProc */
    DupDictInternalRep,		/* dupIntRepProc */
    UpdateStringOfDict,		/* updateStringProc */
    SetDictFromAny,		/* setFromAnyProc */
    TCL_OBJTYPE_V0
};

#define DictSetInternalRep(objPtr, dictRepPtr)				\
    do {                                                                \
	Tcl_ObjInternalRep ir;						\
	ir.twoPtrValue.ptr1 = (dictRepPtr);                             \
	ir.twoPtrValue.ptr2 = NULL;                                     \
	Tcl_StoreInternalRep((objPtr), &tclDictType, &ir);		\
    } while (0)

#define DictGetInternalRep(objPtr, dictRepPtr)				\
    do {                                                                \
	const Tcl_ObjInternalRep *irPtr;				\
	irPtr = TclFetchInternalRep((objPtr), &tclDictType);		\
	(dictRepPtr) = irPtr ? (Dict *)irPtr->twoPtrValue.ptr1 : NULL;	\
    } while (0)

/*
 * The type of the specially adapted version of the Tcl_Obj*-containing hash
 * table defined in the tclObj.c code. This version differs in that it
 * allocates a bit more space in each hash entry in order to hold the pointers
 * used to keep the hash entries in a linked list.
 *
 * Note that this type of hash table is *only* suitable for direct use in
 * *this* file. Everything else should use the dict iterator API.
 */

static const Tcl_HashKeyType chainHashType = {
    TCL_HASH_KEY_TYPE_VERSION,
    TCL_HASH_KEY_DIRECT_COMPARE,	/* allows compare keys by pointers */
    TclHashObjKey,
    TclCompareObjKeys,
    AllocChainEntry,
    TclFreeObjEntry
};

/*
 * Structure used in implementation of 'dict map' to hold the state that gets
 * passed between parts of the implementation.
 */

typedef struct {
    Tcl_Obj *keyVarObj;		/* The name of the variable that will have
				 * keys assigned to it. */
    Tcl_Obj *valueVarObj;	/* The name of the variable that will have
				 * values assigned to it. */
    Tcl_DictSearch search;	/* The dictionary search structure. */
    Tcl_Obj *scriptObj;		/* The script to evaluate each time through
				 * the loop. */
    Tcl_Obj *accumulatorObj;	/* The dictionary used to accumulate the
				 * results. */
} DictMapStorage;

/***** START OF FUNCTIONS IMPLEMENTING DICT CORE API *****/

/*
 *----------------------------------------------------------------------
 *
 * AllocChainEntry --
 *
 *	Allocate space for a Tcl_HashEntry containing the Tcl_Obj * key, and
 *	which has a bit of extra space afterwards for storing pointers to the
 *	rest of the chain of entries (the extra pointers are left NULL).
 *
 * Results:
 *	The return value is a pointer to the created entry.
 *
 * Side effects:
 *	Increments the reference count on the object.
 *
 *----------------------------------------------------------------------
 */

static Tcl_HashEntry *
AllocChainEntry(
    TCL_UNUSED(Tcl_HashTable *),
    void *keyPtr)
{
    Tcl_Obj *objPtr = (Tcl_Obj *)keyPtr;
    ChainEntry *cPtr;

    cPtr = (ChainEntry *)Tcl_Alloc(sizeof(ChainEntry));
    cPtr->entry.key.objPtr = objPtr;
    Tcl_IncrRefCount(objPtr);
    Tcl_SetHashValue(&cPtr->entry, NULL);
    cPtr->prevPtr = cPtr->nextPtr = NULL;

    return &cPtr->entry;
}

/*
 * Helper functions that disguise most of the details relating to how the
 * linked list of hash entries is managed. In particular, these manage the
 * creation of the table and initializing of the chain, the deletion of the
 * table and chain, the adding of an entry to the chain, and the removal of an
 * entry from the chain.
 */

static inline void
InitChainTable(
    Dict *dict)
{
    Tcl_InitCustomHashTable(&dict->table, TCL_CUSTOM_PTR_KEYS,
	    &chainHashType);
    dict->entryChainHead = dict->entryChainTail = NULL;
}

static inline void
DeleteChainTable(
    Dict *dict)
{
    ChainEntry *cPtr;

    for (cPtr=dict->entryChainHead ; cPtr!=NULL ; cPtr=cPtr->nextPtr) {
	Tcl_Obj *valuePtr = (Tcl_Obj *)Tcl_GetHashValue(&cPtr->entry);

	TclDecrRefCount(valuePtr);
    }
    Tcl_DeleteHashTable(&dict->table);
}

static inline Tcl_HashEntry *
CreateChainEntry(
    Dict *dict,
    Tcl_Obj *keyPtr,
    int *newPtr)
{
    ChainEntry *cPtr = (ChainEntry *)
	    Tcl_CreateHashEntry(&dict->table, keyPtr, newPtr);

    /*
     * If this is a new entry in the hash table, stitch it into the chain.
     */

    if (*newPtr) {
	cPtr->nextPtr = NULL;
	if (dict->entryChainHead == NULL) {
	    cPtr->prevPtr = NULL;
	    dict->entryChainHead = cPtr;
	    dict->entryChainTail = cPtr;
	} else {
	    cPtr->prevPtr = dict->entryChainTail;
	    dict->entryChainTail->nextPtr = cPtr;
	    dict->entryChainTail = cPtr;
	}
    }

    return &cPtr->entry;
}

static inline int
DeleteChainEntry(
    Dict *dict,
    Tcl_Obj *keyPtr)
{
    ChainEntry *cPtr = (ChainEntry *)
	    Tcl_FindHashEntry(&dict->table, keyPtr);

    if (cPtr == NULL) {
	return 0;
    } else {
	Tcl_Obj *valuePtr = (Tcl_Obj *)Tcl_GetHashValue(&cPtr->entry);

	TclDecrRefCount(valuePtr);
    }

    /*
     * Unstitch from the chain.
     */

    if (cPtr->nextPtr) {
	cPtr->nextPtr->prevPtr = cPtr->prevPtr;
    } else {
	dict->entryChainTail = cPtr->prevPtr;
    }
    if (cPtr->prevPtr) {
	cPtr->prevPtr->nextPtr = cPtr->nextPtr;
    } else {
	dict->entryChainHead = cPtr->nextPtr;
    }

    Tcl_DeleteHashEntry(&cPtr->entry);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * DupDictInternalRep --
 *
 *	Initialize the internal representation of a dictionary Tcl_Obj to a
 *	copy of the internal representation of an existing dictionary object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"srcPtr"s dictionary internal rep pointer should not be NULL and we
 *	assume it is not NULL. We set "copyPtr"s internal rep to a pointer to
 *	a newly allocated dictionary rep that, in turn, points to "srcPtr"s
 *	key and value objects. Those objects are not actually copied but are
 *	shared between "srcPtr" and "copyPtr". The ref count of each key and
 *	value object is incremented.
 *
 *----------------------------------------------------------------------
 */

static void
DupDictInternalRep(
    Tcl_Obj *srcPtr,
    Tcl_Obj *copyPtr)
{
    Dict *oldDict, *newDict = (Dict *)Tcl_Alloc(sizeof(Dict));
    ChainEntry *cPtr;

    DictGetInternalRep(srcPtr, oldDict);

    /*
     * Copy values across from the old hash table.
     */

    InitChainTable(newDict);
    for (cPtr=oldDict->entryChainHead ; cPtr!=NULL ; cPtr=cPtr->nextPtr) {
	Tcl_Obj *key = (Tcl_Obj *)Tcl_GetHashKey(&oldDict->table, &cPtr->entry);
	Tcl_Obj *valuePtr = (Tcl_Obj *)Tcl_GetHashValue(&cPtr->entry);
	int n;
	Tcl_HashEntry *hPtr = CreateChainEntry(newDict, key, &n);

	/*
	 * Fill in the contents.
	 */

	Tcl_SetHashValue(hPtr, valuePtr);
	Tcl_IncrRefCount(valuePtr);
    }

    /*
     * Initialise other fields.
     */

    newDict->epoch = 1;
    newDict->chain = NULL;
    newDict->refCount = 1;

    /*
     * Store in the object.
     */

    DictSetInternalRep(copyPtr, newDict);
}

/*
 *----------------------------------------------------------------------
 *
 * FreeDictInternalRep --
 *
 *	Deallocate the storage associated with a dictionary object's internal
 *	representation.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Frees the memory holding the dictionary's internal hash table unless
 *	it is locked by an iteration going over it.
 *
 *----------------------------------------------------------------------
 */

static void
FreeDictInternalRep(
    Tcl_Obj *dictPtr)
{
    Dict *dict;

    DictGetInternalRep(dictPtr, dict);

    if (dict->refCount-- <= 1) {
	DeleteDict(dict);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteDict --
 *
 *	Delete the structure that is used to implement a dictionary's internal
 *	representation. Called when either the dictionary object loses its
 *	internal representation or when the last iteration over the dictionary
 *	completes.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Decrements the reference count of all key and value objects in the
 *	dictionary, which may free them.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteDict(
    Dict *dict)
{
    DeleteChainTable(dict);
    Tcl_Free(dict);
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfDict --
 *
 *	Update the string representation for a dictionary object. Note: This
 *	function does not invalidate an existing old string rep so storage
 *	will be lost if this has not already been done. This code is based on
 *	UpdateStringOfList in tclListObj.c
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from the
 *	dict-to-string conversion. This string will be empty if the dictionary
 *	has no key/value pairs. The dictionary internal representation should
 *	not be NULL and we assume it is not NULL.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfDict(
    Tcl_Obj *dictPtr)
{
#define LOCAL_SIZE 64
    char localFlags[LOCAL_SIZE], *flagPtr = NULL;
    Dict *dict;
    ChainEntry *cPtr;
    Tcl_Obj *keyPtr, *valuePtr;
    Tcl_Size i, length;
    size_t bytesNeeded = 0;
    const char *elem;
    char *dst;

    /*
     * This field is the most useful one in the whole hash structure, and it
     * is not exposed by any API function...
     */

    Tcl_Size numElems;

    DictGetInternalRep(dictPtr, dict);

    assert (dict != NULL);

    numElems = dict->table.numEntries * 2;

    /* Handle empty list case first, simplifies what follows */
    if (numElems == 0) {
	Tcl_InitStringRep(dictPtr, NULL, 0);
	return;
    }

    /*
     * Pass 1: estimate space, gather flags.
     */

    if (numElems <= LOCAL_SIZE) {
	flagPtr = localFlags;
    } else {
	flagPtr = (char *)Tcl_Alloc(numElems);
    }
    for (i=0,cPtr=dict->entryChainHead; i<numElems; i+=2,cPtr=cPtr->nextPtr) {
	/*
	 * Assume that cPtr is never NULL since we know the number of array
	 * elements already.
	 */

	flagPtr[i] = ( i ? TCL_DONT_QUOTE_HASH : 0 );
	keyPtr = (Tcl_Obj *)Tcl_GetHashKey(&dict->table, &cPtr->entry);
	elem = TclGetStringFromObj(keyPtr, &length);
	bytesNeeded += TclScanElement(elem, length, flagPtr+i);
	flagPtr[i+1] = TCL_DONT_QUOTE_HASH;
	valuePtr = (Tcl_Obj *)Tcl_GetHashValue(&cPtr->entry);
	elem = TclGetStringFromObj(valuePtr, &length);
	bytesNeeded += TclScanElement(elem, length, flagPtr+i+1);
    }
    bytesNeeded += numElems;

    /*
     * Pass 2: copy into string rep buffer.
     */

    dst = Tcl_InitStringRep(dictPtr, NULL, bytesNeeded - 1);
    TclOOM(dst, bytesNeeded);
    for (i=0,cPtr=dict->entryChainHead; i<numElems; i+=2,cPtr=cPtr->nextPtr) {
	if (i) {
	    flagPtr[i] |= TCL_DONT_QUOTE_HASH;
	}
	keyPtr = (Tcl_Obj *)Tcl_GetHashKey(&dict->table, &cPtr->entry);
	elem = TclGetStringFromObj(keyPtr, &length);
	dst += TclConvertElement(elem, length, dst, flagPtr[i]);
	*dst++ = ' ';

	flagPtr[i+1] |= TCL_DONT_QUOTE_HASH;
	valuePtr = (Tcl_Obj *)Tcl_GetHashValue(&cPtr->entry);
	elem = TclGetStringFromObj(valuePtr, &length);
	dst += TclConvertElement(elem, length, dst, flagPtr[i+1]);
	*dst++ = ' ';
    }
    /* Last space overwrote the terminating NUL; cal T_ISR again to restore */
    (void)Tcl_InitStringRep(dictPtr, NULL, bytesNeeded - 1);

    if (flagPtr != localFlags) {
	Tcl_Free(flagPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SetDictFromAny --
 *
 *	Convert a non-dictionary object into a dictionary object. This code is
 *	very closely related to SetListFromAny in tclListObj.c but does not
 *	actually guarantee that a dictionary object will have a string rep (as
 *	conversions from lists are handled with a special case.)
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	If the string can be converted, it loses any old internal
 *	representation that it had and gains a dictionary's internalRep.
 *
 *----------------------------------------------------------------------
 */

static int
SetDictFromAny(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr)
{
    Tcl_HashEntry *hPtr;
    int isNew;
    Dict *dict = (Dict *)Tcl_Alloc(sizeof(Dict));

    InitChainTable(dict);

    /*
     * Since lists and dictionaries have very closely-related string
     * representations (i.e. the same parsing code) we can safely special-case
     * the conversion from lists to dictionaries.
     */

    if (TclHasInternalRep(objPtr, &tclListType)) {
	Tcl_Size objc, i;
	Tcl_Obj **objv;

	/* Cannot fail, we already know the Tcl_ObjType is "list". */
	TclListObjGetElements(NULL, objPtr, &objc, &objv);
	if (objc & 1) {
	    goto missingValue;
	}

	for (i=0 ; i<objc ; i+=2) {

	    /* Store key and value in the hash table we're building. */
	    hPtr = CreateChainEntry(dict, objv[i], &isNew);
	    if (!isNew) {
		Tcl_Obj *discardedValue = (Tcl_Obj *)Tcl_GetHashValue(hPtr);

		/*
		 * Not really a well-formed dictionary as there are duplicate
		 * keys, so better get the string rep here so that we can
		 * convert back.
		 */

		(void) TclGetString(objPtr);

		TclDecrRefCount(discardedValue);
	    }
	    Tcl_SetHashValue(hPtr, objv[i+1]);
	    Tcl_IncrRefCount(objv[i+1]); /* Since hash now holds ref to it */
	}
    } else {
	Tcl_Size length;
	const char *nextElem = TclGetStringFromObj(objPtr, &length);
	const char *limit = (nextElem + length);

	while (nextElem < limit) {
	    Tcl_Obj *keyPtr, *valuePtr;
	    const char *elemStart;
	    Tcl_Size elemSize;
	    int literal;

	    if (TclFindDictElement(interp, nextElem, (limit - nextElem),
		    &elemStart, &nextElem, &elemSize, &literal) != TCL_OK) {
		goto errorInFindDictElement;
	    }
	    if (elemStart == limit) {
		break;
	    }
	    if (nextElem == limit) {
		goto missingValue;
	    }

	    if (literal) {
		TclNewStringObj(keyPtr, elemStart, elemSize);
	    } else {
		/* Avoid double copy */
		char *dst;

		TclNewObj(keyPtr);
		Tcl_InvalidateStringRep(keyPtr);
		dst = Tcl_InitStringRep(keyPtr, NULL, elemSize);
		TclOOM(dst, elemSize); /* Consider error */
		(void)Tcl_InitStringRep(keyPtr, NULL,
			TclCopyAndCollapse(elemSize, elemStart, dst));
	    }

	    if (TclFindDictElement(interp, nextElem, (limit - nextElem),
		    &elemStart, &nextElem, &elemSize, &literal) != TCL_OK) {
		TclDecrRefCount(keyPtr);
		goto errorInFindDictElement;
	    }

	    if (literal) {
		TclNewStringObj(valuePtr, elemStart, elemSize);
	    } else {
		/* Avoid double copy */
		char *dst;

		TclNewObj(valuePtr);
		Tcl_InvalidateStringRep(valuePtr);
		dst = Tcl_InitStringRep(valuePtr, NULL, elemSize);
		TclOOM(dst, elemSize); /* Consider error */
		(void)Tcl_InitStringRep(valuePtr, NULL,
			TclCopyAndCollapse(elemSize, elemStart, dst));
	    }

	    /* Store key and value in the hash table we're building. */
	    hPtr = CreateChainEntry(dict, keyPtr, &isNew);
	    if (!isNew) {
		Tcl_Obj *discardedValue = (Tcl_Obj *)Tcl_GetHashValue(hPtr);

		TclDecrRefCount(keyPtr);
		TclDecrRefCount(discardedValue);
	    }
	    Tcl_SetHashValue(hPtr, valuePtr);
	    Tcl_IncrRefCount(valuePtr); /* since hash now holds ref to it */
	}
    }

    /*
     * Free the old internalRep before setting the new one. We do this as late
     * as possible to allow the conversion code, in particular
     * Tcl_GetStringFromObj, to use that old internalRep.
     */

    dict->epoch = 1;
    dict->chain = NULL;
    dict->refCount = 1;
    DictSetInternalRep(objPtr, dict);
    return TCL_OK;

  missingValue:
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"missing value to go with key", -1));
	Tcl_SetErrorCode(interp, "TCL", "VALUE", "DICTIONARY", (char *)NULL);
    }
  errorInFindDictElement:
    DeleteChainTable(dict);
    Tcl_Free(dict);
    return TCL_ERROR;
}

static Dict *
GetDictFromObj(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr)
{
    Dict *dict;

    DictGetInternalRep(dictPtr, dict);
    if (dict == NULL) {
	if (SetDictFromAny(interp, dictPtr) != TCL_OK) {
	    return NULL;
	}
	DictGetInternalRep(dictPtr, dict);
    }
    return dict;
}

/*
 *----------------------------------------------------------------------
 *
 * TclTraceDictPath --
 *
 *	Trace through a tree of dictionaries using the array of keys given. If
 *	the flags argument has the DICT_PATH_UPDATE flag is set, a
 *	backward-pointing chain of dictionaries is also built (in the Dict's
 *	chain field) and the chained dictionaries are made into unshared
 *	dictionaries (if they aren't already.)
 *
 * Results:
 *	The object at the end of the path, or NULL if there was an error. Note
 *	that this it is an error for an intermediate dictionary on the path to
 *	not exist. If the flags argument has the DICT_PATH_EXISTS set, a
 *	non-existent path gives a DICT_PATH_NON_EXISTENT result.
 *
 * Side effects:
 *	If the flags argument is zero or DICT_PATH_EXISTS, there are no side
 *	effects (other than potential conversion of objects to dictionaries.)
 *	If the flags argument is DICT_PATH_UPDATE, the following additional
 *	side effects occur. Shared dictionaries along the path are converted
 *	into unshared objects, and a backward-pointing chain is built using
 *	the chain fields of the dictionaries (for easy invalidation of string
 *	representations using InvalidateDictChain). If the flags argument has
 *	the DICT_PATH_CREATE bits set (and not the DICT_PATH_EXISTS bit),
 *	non-extant keys will be inserted with a value of an empty
 *	dictionary, resulting in the path being built.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclTraceDictPath(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    Tcl_Size keyc,
    Tcl_Obj *const keyv[],
    int flags)
{
    Dict *dict, *newDict;
    Tcl_Size i;

    DictGetInternalRep(dictPtr, dict);
    if (dict == NULL) {
	if (SetDictFromAny(interp, dictPtr) != TCL_OK) {
	    return NULL;
	}
	DictGetInternalRep(dictPtr, dict);
    }
    if (flags & DICT_PATH_UPDATE) {
	dict->chain = NULL;
    }

    for (i=0 ; i<keyc ; i++) {
	Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&dict->table, keyv[i]);
	Tcl_Obj *tmpObj;

	if (hPtr == NULL) {
	    int isNew;			/* Dummy */

	    if (flags & DICT_PATH_EXISTS) {
		return DICT_PATH_NON_EXISTENT;
	    }
	    if ((flags & DICT_PATH_CREATE) != DICT_PATH_CREATE) {
		if (interp != NULL) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			    "key \"%s\" not known in dictionary",
			    TclGetString(keyv[i])));
		    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "DICT",
			    TclGetString(keyv[i]), (char *)NULL);
		}
		return NULL;
	    }

	    /*
	     * The next line should always set isNew to 1.
	     */

	    hPtr = CreateChainEntry(dict, keyv[i], &isNew);
	    tmpObj = Tcl_NewDictObj();
	    Tcl_IncrRefCount(tmpObj);
	    Tcl_SetHashValue(hPtr, tmpObj);
	} else {
	    tmpObj = (Tcl_Obj *)Tcl_GetHashValue(hPtr);

	    DictGetInternalRep(tmpObj, newDict);

	    if (newDict == NULL) {
		if (SetDictFromAny(interp, tmpObj) != TCL_OK) {
		    return NULL;
		}
	    }
	}

	DictGetInternalRep(tmpObj, newDict);
	if (flags & DICT_PATH_UPDATE) {
	    if (Tcl_IsShared(tmpObj)) {
		TclDecrRefCount(tmpObj);
		tmpObj = Tcl_DuplicateObj(tmpObj);
		Tcl_IncrRefCount(tmpObj);
		Tcl_SetHashValue(hPtr, tmpObj);
		dict->epoch++;
		DictGetInternalRep(tmpObj, newDict);
	    }

	    newDict->chain = dictPtr;
	}
	dict = newDict;
	dictPtr = tmpObj;
    }
    return dictPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * InvalidateDictChain --
 *
 *	Go through a dictionary chain (built by an updating invocation of
 *	TclTraceDictPath) and invalidate the string representations of all the
 *	dictionaries on the chain.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	String reps are invalidated and epoch counters (for detecting illegal
 *	concurrent modifications) are updated through the chain of updated
 *	dictionaries.
 *
 *----------------------------------------------------------------------
 */

static void
InvalidateDictChain(
    Tcl_Obj *dictObj)
{
    Dict *dict;

    DictGetInternalRep(dictObj, dict);
    assert( dict != NULL);

    do {
	dict->refCount++;
	TclInvalidateStringRep(dictObj);
	TclFreeInternalRep(dictObj);
	DictSetInternalRep(dictObj, dict);

	dict->epoch++;
	dictObj = dict->chain;
	if (dictObj == NULL) {
	    break;
	}
	dict->chain = NULL;
	DictGetInternalRep(dictObj, dict);
    } while (dict != NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DictObjPut --
 *
 *	Add a key,value pair to a dictionary, or update the value for a key if
 *	that key already has a mapping in the dictionary.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The object pointed to by dictPtr is converted to a dictionary if it is
 *	not already one, and any string representation that it has is
 *	invalidated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DictObjPut(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    Tcl_Obj *keyPtr,
    Tcl_Obj *valuePtr)
{
    Dict *dict;
    Tcl_HashEntry *hPtr;
    int isNew;

    if (Tcl_IsShared(dictPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_DictObjPut");
    }

    dict = GetDictFromObj(interp, dictPtr);
    if (dict == NULL) {
	return TCL_ERROR;
    }

    TclInvalidateStringRep(dictPtr);
    hPtr = CreateChainEntry(dict, keyPtr, &isNew);
    dict->refCount++;
    TclFreeInternalRep(dictPtr)
    DictSetInternalRep(dictPtr, dict);
    Tcl_IncrRefCount(valuePtr);
    if (!isNew) {
	Tcl_Obj *oldValuePtr = (Tcl_Obj *)Tcl_GetHashValue(hPtr);

	TclDecrRefCount(oldValuePtr);
    }
    Tcl_SetHashValue(hPtr, valuePtr);
    dict->epoch++;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DictObjGet --
 *
 *	Given a key, get its value from the dictionary (or NULL if key is not
 *	found in dictionary.)
 *
 * Results:
 *	A standard Tcl result. The variable pointed to by valuePtrPtr is
 *	updated with the value for the key. Note that it is not an error for
 *	the key to have no mapping in the dictionary.
 *
 * Side effects:
 *	The object pointed to by dictPtr is converted to a dictionary if it is
 *	not already one.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DictObjGet(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    Tcl_Obj *keyPtr,
    Tcl_Obj **valuePtrPtr)
{
    Dict *dict;
    Tcl_HashEntry *hPtr;

    dict = GetDictFromObj(interp, dictPtr);
    if (dict == NULL) {
	*valuePtrPtr = NULL;
	return TCL_ERROR;
    }

    hPtr = Tcl_FindHashEntry(&dict->table, keyPtr);
    if (hPtr == NULL) {
	*valuePtrPtr = NULL;
    } else {
	*valuePtrPtr = (Tcl_Obj *)Tcl_GetHashValue(hPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DictObjRemove --
 *
 *	Remove the key,value pair with the given key from the dictionary; the
 *	key does not need to be present in the dictionary.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The object pointed to by dictPtr is converted to a dictionary if it is
 *	not already one, and any string representation that it has is
 *	invalidated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DictObjRemove(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    Tcl_Obj *keyPtr)
{
    Dict *dict;

    if (Tcl_IsShared(dictPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_DictObjRemove");
    }

    dict = GetDictFromObj(interp, dictPtr);
    if (dict == NULL) {
	return TCL_ERROR;
    }

    if (DeleteChainEntry(dict, keyPtr)) {
	TclInvalidateStringRep(dictPtr);
	dict->epoch++;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DictGetSize
 *
 *	Returns the size of dictPtr.  Caller must ensure that dictPtr has type
 *	'tclDicttype'.
 *
 *
 *----------------------------------------------------------------------
 */

Tcl_Size
TclDictGetSize(
    Tcl_Obj *dictPtr)
{
    Dict *dict;
    DictGetInternalRep(dictPtr, dict);
    return dict->table.numEntries;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DictObjSize --
 *
 *	How many key,value pairs are there in the dictionary?
 *
 * Results:
 *	A standard Tcl result. Updates the variable pointed to by sizePtr with
 *	the number of key,value pairs in the dictionary.
 *
 * Side effects:
 *	The dictPtr object is converted to a dictionary type if it is not a
 *	dictionary already.
 *
 *----------------------------------------------------------------------
 */

#undef Tcl_DictObjSize
int
Tcl_DictObjSize(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    Tcl_Size *sizePtr)
{
    Dict *dict;

    dict = GetDictFromObj(interp, dictPtr);
    if (dict == NULL) {
	return TCL_ERROR;
    }

    *sizePtr = dict->table.numEntries;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DictObjFirst --
 *
 *	Start a traversal of the dictionary. Caller must supply the search
 *	context, pointers for returning key and value, and a pointer to allow
 *	indication of whether the dictionary has been traversed (i.e. the
 *	dictionary is empty). The order of traversal is undefined.
 *
 * Results:
 *	A standard Tcl result. Updates the variables pointed to by keyPtrPtr,
 *	valuePtrPtr and donePtr. Either of keyPtrPtr and valuePtrPtr may be
 *	NULL, in which case the key/value is not made available to the caller.
 *
 * Side effects:
 *	The dictPtr object is converted to a dictionary type if it is not a
 *	dictionary already. The search context is initialised if the search
 *	has not finished. The dictionary's internal rep is Tcl_Preserve()d if
 *	the dictionary has at least one element.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DictObjFirst(
    Tcl_Interp *interp,		/* For error messages, or NULL if no error
				 * messages desired. */
    Tcl_Obj *dictPtr,		/* Dictionary to traverse. */
    Tcl_DictSearch *searchPtr,	/* Pointer to a dict search context. */
    Tcl_Obj **keyPtrPtr,	/* Pointer to a variable to have the first key
				 * written into, or NULL. */
    Tcl_Obj **valuePtrPtr,	/* Pointer to a variable to have the first
				 * value written into, or NULL.*/
    int *donePtr)		/* Pointer to a variable which will have a 1
				 * written into when there are no further
				 * values in the dictionary, or a 0
				 * otherwise. */
{
    Dict *dict;
    ChainEntry *cPtr;

    dict = GetDictFromObj(interp, dictPtr);
    if (dict == NULL) {
	return TCL_ERROR;
    }

    cPtr = dict->entryChainHead;
    if (cPtr == NULL) {
	searchPtr->epoch = 0;
	*donePtr = 1;
    } else {
	*donePtr = 0;
	searchPtr->dictionaryPtr = (Tcl_Dict) dict;
	searchPtr->epoch = dict->epoch;
	searchPtr->next = cPtr->nextPtr;
	dict->refCount++;
	if (keyPtrPtr != NULL) {
	    *keyPtrPtr = (Tcl_Obj *)Tcl_GetHashKey(&dict->table, &cPtr->entry);
	}
	if (valuePtrPtr != NULL) {
	    *valuePtrPtr = (Tcl_Obj *)Tcl_GetHashValue(&cPtr->entry);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DictObjNext --
 *
 *	Continue a traversal of a dictionary previously started with
 *	Tcl_DictObjFirst. This function is safe against concurrent
 *	modification of the underlying object (including type shimmering),
 *	treating such situations as if the search has terminated, though it is
 *	up to the caller to ensure that the object itself is not disposed
 *	until the search has finished. It is _not_ safe against modifications
 *	from other threads.
 *
 * Results:
 *	Updates the variables pointed to by keyPtrPtr, valuePtrPtr and
 *	donePtr. Either of keyPtrPtr and valuePtrPtr may be NULL, in which
 *	case the key/value is not made available to the caller.
 *
 * Side effects:
 *	Removes a reference to the dictionary's internal rep if the search
 *	terminates.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DictObjNext(
    Tcl_DictSearch *searchPtr,	/* Pointer to a hash search context. */
    Tcl_Obj **keyPtrPtr,	/* Pointer to a variable to have the first key
				 * written into, or NULL. */
    Tcl_Obj **valuePtrPtr,	/* Pointer to a variable to have the first
				 * value written into, or NULL.*/
    int *donePtr)		/* Pointer to a variable which will have a 1
				 * written into when there are no further
				 * values in the dictionary, or a 0
				 * otherwise. */
{
    ChainEntry *cPtr;

    /*
     * If the search is done; we do no work.
     */

    if (!searchPtr->epoch) {
	*donePtr = 1;
	return;
    }

    /*
     * Bail out if the dictionary has had any elements added, modified or
     * removed. This *shouldn't* happen, but...
     */

    if (((Dict *)searchPtr->dictionaryPtr)->epoch != searchPtr->epoch) {
	Tcl_Panic("concurrent dictionary modification and search");
    }

    cPtr = (ChainEntry *)searchPtr->next;
    if (cPtr == NULL) {
	Tcl_DictObjDone(searchPtr);
	*donePtr = 1;
	return;
    }

    searchPtr->next = cPtr->nextPtr;
    *donePtr = 0;
    if (keyPtrPtr != NULL) {
	*keyPtrPtr = (Tcl_Obj *)Tcl_GetHashKey(
		&((Dict *)searchPtr->dictionaryPtr)->table, &cPtr->entry);
    }
    if (valuePtrPtr != NULL) {
	*valuePtrPtr = (Tcl_Obj *)Tcl_GetHashValue(&cPtr->entry);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DictObjDone --
 *
 *	Call this if you want to stop a search before you reach the end of the
 *	dictionary (e.g. because of abnormal termination of the search). It
 *	need not be used if the search reaches its natural end (i.e. if either
 *	Tcl_DictObjFirst or Tcl_DictObjNext sets its donePtr variable to 1).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes a reference to the dictionary's internal rep.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DictObjDone(
    Tcl_DictSearch *searchPtr)	/* Pointer to a hash search context. */
{
    Dict *dict;

    if (searchPtr->epoch) {
	searchPtr->epoch = 0;
	dict = (Dict *) searchPtr->dictionaryPtr;
	if (dict->refCount-- <= 1) {
	    DeleteDict(dict);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DictObjPutKeyList --
 *
 *	Add a key...key,value pair to a dictionary tree. The main dictionary
 *	value must not be shared, though sub-dictionaries may be. All
 *	intermediate dictionaries on the path must exist.
 *
 * Results:
 *	A standard Tcl result. Note that in the error case, a message is left
 *	in interp unless that is NULL.
 *
 * Side effects:
 *	If the dictionary and any of its sub-dictionaries on the path have
 *	string representations, these are invalidated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DictObjPutKeyList(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    Tcl_Size keyc,
    Tcl_Obj *const keyv[],
    Tcl_Obj *valuePtr)
{
    Dict *dict;
    Tcl_HashEntry *hPtr;
    int isNew;

    if (Tcl_IsShared(dictPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_DictObjPutKeyList");
    }
    if (keyc < 1) {
	Tcl_Panic("%s called with empty key list", "Tcl_DictObjPutKeyList");
    }

    dictPtr = TclTraceDictPath(interp, dictPtr, keyc-1,keyv, DICT_PATH_CREATE);
    if (dictPtr == NULL) {
	return TCL_ERROR;
    }

    DictGetInternalRep(dictPtr, dict);
    assert(dict != NULL);
    hPtr = CreateChainEntry(dict, keyv[keyc-1], &isNew);
    Tcl_IncrRefCount(valuePtr);
    if (!isNew) {
	Tcl_Obj *oldValuePtr = (Tcl_Obj *)Tcl_GetHashValue(hPtr);

	TclDecrRefCount(oldValuePtr);
    }
    Tcl_SetHashValue(hPtr, valuePtr);
    InvalidateDictChain(dictPtr);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DictObjRemoveKeyList --
 *
 *	Remove a key...key,value pair from a dictionary tree (the value
 *	removed is implicit in the key path). The main dictionary value must
 *	not be shared, though sub-dictionaries may be. It is not an error if
 *	there is no value associated with the given key list, but all
 *	intermediate dictionaries on the key path must exist.
 *
 * Results:
 *	A standard Tcl result. Note that in the error case, a message is left
 *	in interp unless that is NULL.
 *
 * Side effects:
 *	If the dictionary and any of its sub-dictionaries on the key path have
 *	string representations, these are invalidated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DictObjRemoveKeyList(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    Tcl_Size keyc,
    Tcl_Obj *const keyv[])
{
    Dict *dict;

    if (Tcl_IsShared(dictPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_DictObjRemoveKeyList");
    }
    if (keyc < 1) {
	Tcl_Panic("%s called with empty key list", "Tcl_DictObjRemoveKeyList");
    }

    dictPtr = TclTraceDictPath(interp, dictPtr, keyc-1,keyv, DICT_PATH_UPDATE);
    if (dictPtr == NULL) {
	return TCL_ERROR;
    }

    DictGetInternalRep(dictPtr, dict);
    assert(dict != NULL);
    DeleteChainEntry(dict, keyv[keyc-1]);
    InvalidateDictChain(dictPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewDictObj --
 *
 *	This function is normally called when not debugging: i.e., when
 *	TCL_MEM_DEBUG is not defined. It creates a new dict object without any
 *	content.
 *
 *	When TCL_MEM_DEBUG is defined, this function just returns the result
 *	of calling the debugging version Tcl_DbNewDictObj.
 *
 * Results:
 *	A new dict object is returned; it has no keys defined in it. The new
 *	object's string representation is left NULL, and the ref count of the
 *	object is 0.
 *
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_NewDictObj(void)
{
#ifdef TCL_MEM_DEBUG
    return Tcl_DbNewDictObj("unknown", 0);
#else /* !TCL_MEM_DEBUG */

    Tcl_Obj *dictPtr;
    Dict *dict;

    TclNewObj(dictPtr);
    TclInvalidateStringRep(dictPtr);
    dict = (Dict *)Tcl_Alloc(sizeof(Dict));
    InitChainTable(dict);
    dict->epoch = 1;
    dict->chain = NULL;
    dict->refCount = 1;
    DictSetInternalRep(dictPtr, dict);
    return dictPtr;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbNewDictObj --
 *
 *	This function is normally called when debugging: i.e., when
 *	TCL_MEM_DEBUG is defined. It creates new dict objects. It is the same
 *	as the Tcl_NewDictObj function above except that it calls
 *	Tcl_DbCkalloc directly with the file name and line number from its
 *	caller. This simplifies debugging since then the [memory active]
 *	command will report the correct file name and line number when
 *	reporting objects that haven't been freed.
 *
 *	When TCL_MEM_DEBUG is not defined, this function just returns the
 *	result of calling Tcl_NewDictObj.
 *
 * Results:
 *	A new dict object is returned; it has no keys defined in it. The new
 *	object's string representation is left NULL, and the ref count of the
 *	object is 0.
 *
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
Tcl_Obj *
Tcl_DbNewDictObj(
    const char *file,
    int line)
{
    Tcl_Obj *dictPtr;
    Dict *dict;

    TclDbNewObj(dictPtr, file, line);
    TclInvalidateStringRep(dictPtr);
    dict = (Dict *)Tcl_DbCkalloc(sizeof(Dict), file, line);
    InitChainTable(dict);
    dict->epoch = 1;
    dict->chain = NULL;
    dict->refCount = 1;
    DictSetInternalRep(dictPtr, dict);
    return dictPtr;
}
#else /* !TCL_MEM_DEBUG */
Tcl_Obj *
Tcl_DbNewDictObj(
    TCL_UNUSED(const char *) /*file*/,
    TCL_UNUSED(int) /*line*/)
{
    return Tcl_NewDictObj();
}
#endif

/***** START OF FUNCTIONS ACTING AS HELPERS *****/

/*
 *----------------------------------------------------------------------
 *
 * TclDictGet --
 *
 *	Given a key, get its value from the dictionary (or NULL if key is not
 *	found in dictionary.)
 *
 * Results:
 *	A standard Tcl result. The variable pointed to by valuePtrPtr is
 *	updated with the value for the key. Note that it is not an error for
 *	the key to have no mapping in the dictionary.
 *
 * Side effects:
 *	The object pointed to by dictPtr is converted to a dictionary if it is
 *	not already one.
 *
 *----------------------------------------------------------------------
 */
int
TclDictGet(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    const char *key,		/* The key in a C string. */
    Tcl_Obj **valuePtrPtr)	/* Where to write the value. */
{
    Tcl_Obj *keyPtr = Tcl_NewStringObj(key, -1);
    int code;

    Tcl_IncrRefCount(keyPtr);
    code = Tcl_DictObjGet(interp, dictPtr, keyPtr, valuePtrPtr);
    Tcl_DecrRefCount(keyPtr);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * TclDictPut --
 *
 *	Add a key,value pair to a dictionary, or update the value for a key if
 *	that key already has a mapping in the dictionary.
 *
 *	If valuePtr is a zero-count object and is not written into the
 *	dictionary because of an error, it is freed by this routine. The caller
 *	does NOT need to do reference count management.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The object pointed to by dictPtr is converted to a dictionary if it is
 *	not already one, and any string representation that it has is
 *	invalidated.
 *
 *----------------------------------------------------------------------
 */
int
TclDictPut(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    const char *key,		/* The key in a C string. */
    Tcl_Obj *valuePtr)		/* The value to write in. */
{
    Tcl_Obj *keyPtr = Tcl_NewStringObj(key, -1);
    int code;

    Tcl_IncrRefCount(keyPtr);
    Tcl_IncrRefCount(valuePtr);
    code = Tcl_DictObjPut(interp, dictPtr, keyPtr, valuePtr);
    Tcl_DecrRefCount(keyPtr);
    Tcl_DecrRefCount(valuePtr);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * TclDictPutString --
 *
 *	Add a key,value pair to a dictionary, or update the value for a key if
 *	that key already has a mapping in the dictionary.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The object pointed to by dictPtr is converted to a dictionary if it is
 *	not already one, and any string representation that it has is
 *	invalidated.
 *
 *----------------------------------------------------------------------
 */
int
TclDictPutString(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    const char *key,		/* The key in a C string. */
    const char *value)		/* The value in a C string. */
{
    Tcl_Obj *keyPtr = Tcl_NewStringObj(key, -1);
    Tcl_Obj *valuePtr = Tcl_NewStringObj(value, -1);
    int code;

    Tcl_IncrRefCount(keyPtr);
    Tcl_IncrRefCount(valuePtr);
    code = Tcl_DictObjPut(interp, dictPtr, keyPtr, valuePtr);
    Tcl_DecrRefCount(keyPtr);
    Tcl_DecrRefCount(valuePtr);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * TclDictRemove --
 *
 *	Remove the key,value pair with the given key from the dictionary; the
 *	key does not need to be present in the dictionary.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The object pointed to by dictPtr is converted to a dictionary if it is
 *	not already one, and any string representation that it has is
 *	invalidated.
 *
 *----------------------------------------------------------------------
 */
int
TclDictRemove(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    const char *key)		/* The key in a C string. */
{
    Tcl_Obj *keyPtr = Tcl_NewStringObj(key, -1);
    int code;

    Tcl_IncrRefCount(keyPtr);
    code = Tcl_DictObjRemove(interp, dictPtr, keyPtr);
    Tcl_DecrRefCount(keyPtr);
    return code;
}

/***** START OF FUNCTIONS IMPLEMENTING TCL COMMANDS *****/

/*
 *----------------------------------------------------------------------
 *
 * DictCreateCmd --
 *
 *	This function implements the "dict create" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictCreateCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *dictObj;
    int i;

    /*
     * Must have an even number of arguments; note that number of preceding
     * arguments (i.e. "dict create" is also even, which makes this much
     * easier.)
     */

    if ((objc & 1) == 0) {
	Tcl_WrongNumArgs(interp, 1, objv, "?key value ...?");
	return TCL_ERROR;
    }

    dictObj = Tcl_NewDictObj();
    for (i=1 ; i<objc ; i+=2) {
	/*
	 * The next command is assumed to never fail...
	 */
	Tcl_DictObjPut(NULL, dictObj, objv[i], objv[i+1]);
    }
    Tcl_SetObjResult(interp, dictObj);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictGetCmd --
 *
 *	This function implements the "dict get" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictGetCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *dictPtr, *valuePtr = NULL;
    int result;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictionary ?key ...?");
	return TCL_ERROR;
    }

    /*
     * Test for the special case of no keys, which returns a *list* of all
     * key,value pairs. We produce a copy here because that makes subsequent
     * list handling more efficient.
     */

    if (objc == 2) {
	Tcl_Obj *keyPtr = NULL, *listPtr;
	Tcl_DictSearch search;
	int done;

	result = Tcl_DictObjFirst(interp, objv[1], &search,
		&keyPtr, &valuePtr, &done);
	if (result != TCL_OK) {
	    return result;
	}
	listPtr = Tcl_NewListObj(0, NULL);
	while (!done) {
	    /*
	     * Assume these won't fail as we have complete control over the
	     * types of things here.
	     */

	    Tcl_ListObjAppendElement(interp, listPtr, keyPtr);
	    Tcl_ListObjAppendElement(interp, listPtr, valuePtr);

	    Tcl_DictObjNext(&search, &keyPtr, &valuePtr, &done);
	}
	Tcl_SetObjResult(interp, listPtr);
	return TCL_OK;
    }

    /*
     * Loop through the list of keys, looking up the key at the current index
     * in the current dictionary each time. Once we've done the lookup, we set
     * the current dictionary to be the value we looked up (in case the value
     * was not the last one and we are going through a chain of searches.)
     * Note that this loop always executes at least once.
     */

    dictPtr = TclTraceDictPath(interp, objv[1], objc-3,objv+2, DICT_PATH_READ);
    if (dictPtr == NULL) {
	return TCL_ERROR;
    }
    result = Tcl_DictObjGet(interp, dictPtr, objv[objc-1], &valuePtr);
    if (result != TCL_OK) {
	return result;
    }
    if (valuePtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"key \"%s\" not known in dictionary",
		TclGetString(objv[objc-1])));
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "DICT",
		TclGetString(objv[objc-1]), (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, valuePtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictGetDefCmd --
 *
 *	This function implements the "dict getdef" and "dict getwithdefault"
 *	Tcl commands. See the user documentation for details on what it does,
 *	and TIP#342 for the formal specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictGetDefCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *dictPtr, *keyPtr, *valuePtr, *defaultPtr;
    Tcl_Obj *const *keyPath;
    int numKeys;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictionary ?key ...? key default");
	return TCL_ERROR;
    }

    /*
     * Give the bits of arguments names for clarity.
     */

    dictPtr = objv[1];
    keyPath = &objv[2];
    numKeys = objc - 4;		/* Number of keys in keyPath; there's always
				 * one extra key afterwards too. */
    keyPtr = objv[objc - 2];
    defaultPtr = objv[objc - 1];

    /*
     * Implement the getting-with-default operation.
     */

    dictPtr = TclTraceDictPath(interp, dictPtr, numKeys, keyPath,
	    DICT_PATH_EXISTS);
    if (dictPtr == NULL) {
	return TCL_ERROR;
    } else if (dictPtr == DICT_PATH_NON_EXISTENT) {
	Tcl_SetObjResult(interp, defaultPtr);
    } else if (Tcl_DictObjGet(interp, dictPtr, keyPtr, &valuePtr) != TCL_OK) {
	return TCL_ERROR;
    } else if (valuePtr == NULL) {
	Tcl_SetObjResult(interp, defaultPtr);
    } else {
	Tcl_SetObjResult(interp, valuePtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictReplaceCmd --
 *
 *	This function implements the "dict replace" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictReplaceCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *dictPtr;
    int i;

    if ((objc < 2) || (objc & 1)) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictionary ?key value ...?");
	return TCL_ERROR;
    }

    dictPtr = objv[1];
    if (GetDictFromObj(interp, dictPtr) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_IsShared(dictPtr)) {
	dictPtr = Tcl_DuplicateObj(dictPtr);
    }
    TclInvalidateStringRep(dictPtr);
    for (i=2 ; i<objc ; i+=2) {
	Tcl_DictObjPut(NULL, dictPtr, objv[i], objv[i+1]);
    }
    Tcl_SetObjResult(interp, dictPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictRemoveCmd --
 *
 *	This function implements the "dict remove" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictRemoveCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *dictPtr;
    int i;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictionary ?key ...?");
	return TCL_ERROR;
    }

    dictPtr = objv[1];
    if (GetDictFromObj(interp, dictPtr) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_IsShared(dictPtr)) {
	dictPtr = Tcl_DuplicateObj(dictPtr);
    }
    TclInvalidateStringRep(dictPtr);
    for (i=2 ; i<objc ; i++) {
	Tcl_DictObjRemove(NULL, dictPtr, objv[i]);
    }
    Tcl_SetObjResult(interp, dictPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictMergeCmd --
 *
 *	This function implements the "dict merge" Tcl command. See the user
 *	documentation for details on what it does, and TIP#163 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictMergeCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *targetObj, *keyObj = NULL, *valueObj = NULL;
    int done, allocatedDict = 0;
    int i;
    Tcl_DictSearch search;

    if (objc == 1) {
	/*
	 * No dictionary arguments; return default (empty value).
	 */

	return TCL_OK;
    }

    /*
     * Make sure first argument is a dictionary.
     */

    targetObj = objv[1];
    if (GetDictFromObj(interp, targetObj) == NULL) {
	return TCL_ERROR;
    }

    if (objc == 2) {
	/*
	 * Single argument, return it.
	 */

	Tcl_SetObjResult(interp, objv[1]);
	return TCL_OK;
    }

    /*
     * Normal behaviour: combining two (or more) dictionaries.
     */

    if (Tcl_IsShared(targetObj)) {
	targetObj = Tcl_DuplicateObj(targetObj);
	allocatedDict = 1;
    }
    for (i=2 ; i<objc ; i++) {
	if (Tcl_DictObjFirst(interp, objv[i], &search, &keyObj, &valueObj,
		&done) != TCL_OK) {
	    if (allocatedDict) {
		TclDecrRefCount(targetObj);
	    }
	    return TCL_ERROR;
	}
	while (!done) {
	    /*
	     * Next line can't fail; already know we have a dictionary in
	     * targetObj.
	     */

	    Tcl_DictObjPut(NULL, targetObj, keyObj, valueObj);
	    Tcl_DictObjNext(&search, &keyObj, &valueObj, &done);
	}
	Tcl_DictObjDone(&search);
    }
    Tcl_SetObjResult(interp, targetObj);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictKeysCmd --
 *
 *	This function implements the "dict keys" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictKeysCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *listPtr;
    const char *pattern = NULL;

    if (objc!=2 && objc!=3) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictionary ?pattern?");
	return TCL_ERROR;
    }

    /*
     * A direct check that we have a dictionary. We don't start the iteration
     * yet because that might allocate memory or set locks that we do not
     * need. [Bug 1705778, leak K04]
     */

    if (GetDictFromObj(interp, objv[1]) == NULL) {
	return TCL_ERROR;
    }

    if (objc == 3) {
	pattern = TclGetString(objv[2]);
    }
    listPtr = Tcl_NewListObj(0, NULL);
    if ((pattern != NULL) && TclMatchIsTrivial(pattern)) {
	Tcl_Obj *valuePtr = NULL;

	Tcl_DictObjGet(interp, objv[1], objv[2], &valuePtr);
	if (valuePtr != NULL) {
	    Tcl_ListObjAppendElement(NULL, listPtr, objv[2]);
	}
    } else {
	Tcl_DictSearch search;
	Tcl_Obj *keyPtr = NULL;
	int done = 0;

	/*
	 * At this point, we know we have a dictionary (or at least something
	 * that can be represented; it could theoretically have shimmered away
	 * when the pattern was fetched, but that shouldn't be damaging) so we
	 * can start the iteration process without checking for failures.
	 */

	Tcl_DictObjFirst(NULL, objv[1], &search, &keyPtr, NULL, &done);
	for (; !done ; Tcl_DictObjNext(&search, &keyPtr, NULL, &done)) {
	    if (!pattern || Tcl_StringMatch(TclGetString(keyPtr), pattern)) {
		Tcl_ListObjAppendElement(NULL, listPtr, keyPtr);
	    }
	}
	Tcl_DictObjDone(&search);
    }

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictValuesCmd --
 *
 *	This function implements the "dict values" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictValuesCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *valuePtr = NULL, *listPtr;
    Tcl_DictSearch search;
    int done;
    const char *pattern;

    if (objc!=2 && objc!=3) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictionary ?pattern?");
	return TCL_ERROR;
    }

    if (Tcl_DictObjFirst(interp, objv[1], &search, NULL, &valuePtr,
	    &done) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	pattern = TclGetString(objv[2]);
    } else {
	pattern = NULL;
    }
    listPtr = Tcl_NewListObj(0, NULL);
    for (; !done ; Tcl_DictObjNext(&search, NULL, &valuePtr, &done)) {
	if (pattern==NULL || Tcl_StringMatch(TclGetString(valuePtr),pattern)) {
	    /*
	     * Assume this operation always succeeds.
	     */

	    Tcl_ListObjAppendElement(interp, listPtr, valuePtr);
	}
    }
    Tcl_DictObjDone(&search);

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictSizeCmd --
 *
 *	This function implements the "dict size" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictSizeCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    int result;
    Tcl_Size size;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictionary");
	return TCL_ERROR;
    }
    result = Tcl_DictObjSize(interp, objv[1], &size);
    if (result == TCL_OK) {
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(size));
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclDictObjSmartRef --
 *
 *	This function returns new tcl-object with the smart reference to
 *	dictionary object.
 *
 *	Object returned with this function is a smart reference (pointer),
 *	so new object of type tclDictType, that directly references given
 *	dictionary object (with internally increased refCount).
 *
 *	The usage of such pointer objects allows to hold more as one
 *	reference to the same real dictionary object, allows to make a pointer
 *	to part of another dictionary, allows to change the dictionary without
 *	regarding of the "shared" state of the dictionary object.
 *
 *	Prevents "called with shared object" exception if object is multiple
 *	referenced.
 *
 * Results:
 *	The newly create object (contains smart reference) is returned.
 *	The returned object has a ref count of 0.
 *
 * Side effects:
 *	Increases ref count of the referenced dictionary.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclDictObjSmartRef(
    Tcl_Interp *interp,
    Tcl_Obj    *dictPtr)
{
    Tcl_Obj *result;
    Dict    *dict;

    if (!TclHasInternalRep(dictPtr, &tclDictType)
	    && SetDictFromAny(interp, dictPtr) != TCL_OK) {
	return NULL;
    }

    DictGetInternalRep(dictPtr, dict);

    result = Tcl_NewObj();
    DictSetInternalRep(result, dict);
    dict->refCount++;
    result->internalRep.twoPtrValue.ptr2 = NULL;
    result->typePtr = &tclDictType;

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DictExistsCmd --
 *
 *	This function implements the "dict exists" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictExistsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *dictPtr, *valuePtr;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictionary key ?key ...?");
	return TCL_ERROR;
    }

    dictPtr = TclTraceDictPath(NULL, objv[1], objc-3, objv+2,DICT_PATH_EXISTS);
    if (dictPtr == NULL || dictPtr == DICT_PATH_NON_EXISTENT ||
	    Tcl_DictObjGet(NULL, dictPtr, objv[objc-1], &valuePtr) != TCL_OK) {
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
    } else {
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(valuePtr != NULL));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictInfoCmd --
 *
 *	This function implements the "dict info" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictInfoCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Dict *dict;
    char *statsStr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictionary");
	return TCL_ERROR;
    }

    dict = GetDictFromObj(interp, objv[1]);
    if (dict == NULL) {
	return TCL_ERROR;
    }

    statsStr = Tcl_HashStats(&dict->table);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(statsStr, -1));
    Tcl_Free(statsStr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictIncrCmd --
 *
 *	This function implements the "dict incr" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictIncrCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    int code = TCL_OK;
    Tcl_Obj *dictPtr, *valuePtr = NULL;

    if (objc < 3 || objc > 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictVarName key ?increment?");
	return TCL_ERROR;
    }

    dictPtr = Tcl_ObjGetVar2(interp, objv[1], NULL, 0);
    if (dictPtr == NULL) {
	/*
	 * Variable didn't yet exist. Create new dictionary value.
	 */

	dictPtr = Tcl_NewDictObj();
    } else if (Tcl_DictObjGet(interp, dictPtr, objv[2], &valuePtr) != TCL_OK) {
	/*
	 * Variable contents are not a dict, report error.
	 */

	return TCL_ERROR;
    }
    if (Tcl_IsShared(dictPtr)) {
	/*
	 * A little internals surgery to avoid copying a string rep that will
	 * soon be no good.
	 */

	Tcl_Obj *oldPtr = dictPtr;

	TclNewObj(dictPtr);
	TclInvalidateStringRep(dictPtr);
	DupDictInternalRep(oldPtr, dictPtr);
    }
    if (valuePtr == NULL) {
	/*
	 * Key not in dictionary. Create new key with increment as value.
	 */

	if (objc == 4) {
	    /*
	     * Verify increment is an integer.
	     */

	    mp_int increment;

	    code = Tcl_GetBignumFromObj(interp, objv[3], &increment);
	    if (code != TCL_OK) {
		Tcl_AddErrorInfo(interp, "\n    (reading increment)");
	    } else {
		/*
		 * Remember to dispose with the bignum as we're not actually
		 * using it directly. [Bug 2874678]
		 */

		mp_clear(&increment);
		Tcl_DictObjPut(NULL, dictPtr, objv[2], objv[3]);
	    }
	} else {
	    Tcl_DictObjPut(NULL, dictPtr, objv[2], Tcl_NewWideIntObj(1));
	}
    } else {
	/*
	 * Key in dictionary. Increment its value with minimum dup.
	 */

	if (Tcl_IsShared(valuePtr)) {
	    valuePtr = Tcl_DuplicateObj(valuePtr);
	    Tcl_DictObjPut(NULL, dictPtr, objv[2], valuePtr);
	}
	if (objc == 4) {
	    code = TclIncrObj(interp, valuePtr, objv[3]);
	} else {
	    Tcl_Obj *incrPtr;

	    TclNewIntObj(incrPtr, 1);
	    Tcl_IncrRefCount(incrPtr);
	    code = TclIncrObj(interp, valuePtr, incrPtr);
	    TclDecrRefCount(incrPtr);
	}
    }
    if (code == TCL_OK) {
	TclInvalidateStringRep(dictPtr);
	valuePtr = Tcl_ObjSetVar2(interp, objv[1], NULL,
		dictPtr, TCL_LEAVE_ERR_MSG);
	if (valuePtr == NULL) {
	    code = TCL_ERROR;
	} else {
	    Tcl_SetObjResult(interp, valuePtr);
	}
    } else if (dictPtr->refCount == 0) {
	TclDecrRefCount(dictPtr);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * DictLappendCmd --
 *
 *	This function implements the "dict lappend" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictLappendCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *dictPtr, *valuePtr, *resultPtr;
    int allocatedDict = 0, allocatedValue = 0;
    int i;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictVarName key ?value ...?");
	return TCL_ERROR;
    }

    dictPtr = Tcl_ObjGetVar2(interp, objv[1], NULL, 0);
    if (dictPtr == NULL) {
	allocatedDict = 1;
	dictPtr = Tcl_NewDictObj();
    } else if (Tcl_IsShared(dictPtr)) {
	allocatedDict = 1;
	dictPtr = Tcl_DuplicateObj(dictPtr);
    }

    if (Tcl_DictObjGet(interp, dictPtr, objv[2], &valuePtr) != TCL_OK) {
	if (allocatedDict) {
	    TclDecrRefCount(dictPtr);
	}
	return TCL_ERROR;
    }

    if (valuePtr == NULL) {
	valuePtr = Tcl_NewListObj(objc-3, objv+3);
	allocatedValue = 1;
    } else {
	if (Tcl_IsShared(valuePtr)) {
	    allocatedValue = 1;
	    valuePtr = Tcl_DuplicateObj(valuePtr);
	}

	for (i=3 ; i<objc ; i++) {
	    if (Tcl_ListObjAppendElement(interp, valuePtr,
		    objv[i]) != TCL_OK) {
		if (allocatedValue) {
		    TclDecrRefCount(valuePtr);
		}
		if (allocatedDict) {
		    TclDecrRefCount(dictPtr);
		}
		return TCL_ERROR;
	    }
	}
    }

    if (allocatedValue) {
	Tcl_DictObjPut(NULL, dictPtr, objv[2], valuePtr);
    } else {
	TclInvalidateStringRep(dictPtr);
    }

    resultPtr = Tcl_ObjSetVar2(interp, objv[1], NULL, dictPtr,
	    TCL_LEAVE_ERR_MSG);
    if (resultPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictAppendCmd --
 *
 *	This function implements the "dict append" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictAppendCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *dictPtr, *valuePtr, *resultPtr;
    int allocatedDict = 0;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictVarName key ?value ...?");
	return TCL_ERROR;
    }

    dictPtr = Tcl_ObjGetVar2(interp, objv[1], NULL, 0);
    if (dictPtr == NULL) {
	allocatedDict = 1;
	dictPtr = Tcl_NewDictObj();
    } else if (Tcl_IsShared(dictPtr)) {
	allocatedDict = 1;
	dictPtr = Tcl_DuplicateObj(dictPtr);
    }

    if (Tcl_DictObjGet(interp, dictPtr, objv[2], &valuePtr) != TCL_OK) {
	if (allocatedDict) {
	    TclDecrRefCount(dictPtr);
	}
	return TCL_ERROR;
    }

    if ((objc > 3) || (valuePtr == NULL)) {
	/* Only go through append activites when something will change. */
	Tcl_Obj *appendObjPtr = NULL;

	if (objc > 3) {
	    /* Something to append */

	    if (objc == 4) {
		appendObjPtr = objv[3];
	    } else {
		appendObjPtr = TclStringCat(interp, objc-3, objv+3,
			TCL_STRING_IN_PLACE);
		if (appendObjPtr == NULL) {
		    return TCL_ERROR;
		}
	    }
	}

	if (appendObjPtr == NULL) {
	    /* => (objc == 3) => (valuePtr == NULL) */
	    TclNewObj(valuePtr);
	} else if (valuePtr == NULL) {
	    valuePtr = appendObjPtr;
	    appendObjPtr = NULL;
	}

	if (appendObjPtr) {
	    if (Tcl_IsShared(valuePtr)) {
		valuePtr = Tcl_DuplicateObj(valuePtr);
	    }

	    Tcl_IncrRefCount(appendObjPtr);
	    Tcl_AppendObjToObj(valuePtr, appendObjPtr);
	    Tcl_DecrRefCount(appendObjPtr);
	}

	Tcl_DictObjPut(NULL, dictPtr, objv[2], valuePtr);
    }

    /*
     * Even if nothing changed, we still overwrite so that variable
     * trace expectations are met.
     */

    resultPtr = Tcl_ObjSetVar2(interp, objv[1], NULL, dictPtr,
	    TCL_LEAVE_ERR_MSG);
    if (resultPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictForNRCmd --
 *
 *	These functions implement the "dict for" Tcl command.  See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictForNRCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Obj *scriptObj, *keyVarObj, *valueVarObj;
    Tcl_Obj **varv, *keyObj, *valueObj;
    Tcl_DictSearch *searchPtr;
    Tcl_Size varc;
    int done;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"{keyVarName valueVarName} dictionary script");
	return TCL_ERROR;
    }

    /*
     * Parse arguments.
     */

    if (TclListObjGetElements(interp, objv[1], &varc, &varv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (varc != 2) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"must have exactly two variable names", -1));
	Tcl_SetErrorCode(interp, "TCL", "SYNTAX", "dict", "for", (char *)NULL);
	return TCL_ERROR;
    }
    searchPtr = (Tcl_DictSearch *)TclStackAlloc(interp, sizeof(Tcl_DictSearch));
    if (Tcl_DictObjFirst(interp, objv[2], searchPtr, &keyObj, &valueObj,
	    &done) != TCL_OK) {
	TclStackFree(interp, searchPtr);
	return TCL_ERROR;
    }
    if (done) {
	TclStackFree(interp, searchPtr);
	return TCL_OK;
    }
    TclListObjGetElements(NULL, objv[1], &varc, &varv);
    keyVarObj = varv[0];
    valueVarObj = varv[1];
    scriptObj = objv[3];

    /*
     * Make sure that these objects (which we need throughout the body of the
     * loop) don't vanish. Note that the dictionary internal rep is locked
     * internally so that updates, shimmering, etc are not a problem.
     */

    Tcl_IncrRefCount(keyVarObj);
    Tcl_IncrRefCount(valueVarObj);
    Tcl_IncrRefCount(scriptObj);

    /*
     * Stop the value from getting hit in any way by any traces on the key
     * variable.
     */

    Tcl_IncrRefCount(valueObj);
    if (Tcl_ObjSetVar2(interp, keyVarObj, NULL, keyObj,
	    TCL_LEAVE_ERR_MSG) == NULL) {
	TclDecrRefCount(valueObj);
	goto error;
    }
    TclDecrRefCount(valueObj);
    if (Tcl_ObjSetVar2(interp, valueVarObj, NULL, valueObj,
	    TCL_LEAVE_ERR_MSG) == NULL) {
	goto error;
    }

    /*
     * Run the script.
     */

    TclNRAddCallback(interp, DictForLoopCallback, searchPtr, keyVarObj,
	    valueVarObj, scriptObj);
    return TclNREvalObjEx(interp, scriptObj, 0, iPtr->cmdFramePtr, 3);

    /*
     * For unwinding everything on error.
     */

  error:
    TclDecrRefCount(keyVarObj);
    TclDecrRefCount(valueVarObj);
    TclDecrRefCount(scriptObj);
    Tcl_DictObjDone(searchPtr);
    TclStackFree(interp, searchPtr);
    return TCL_ERROR;
}

static int
DictForLoopCallback(
    void *data[],
    Tcl_Interp *interp,
    int result)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_DictSearch *searchPtr = (Tcl_DictSearch *)data[0];
    Tcl_Obj *keyVarObj = (Tcl_Obj *)data[1];
    Tcl_Obj *valueVarObj = (Tcl_Obj *)data[2];
    Tcl_Obj *scriptObj = (Tcl_Obj *)data[3];
    Tcl_Obj *keyObj, *valueObj;
    int done;

    /*
     * Process the result from the previous execution of the script body.
     */

    if (result == TCL_CONTINUE) {
	result = TCL_OK;
    } else if (result != TCL_OK) {
	if (result == TCL_BREAK) {
	    Tcl_ResetResult(interp);
	    result = TCL_OK;
	} else if (result == TCL_ERROR) {
	    Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
		    "\n    (\"dict for\" body line %d)",
		    Tcl_GetErrorLine(interp)));
	}
	goto done;
    }

    /*
     * Get the next mapping from the dictionary.
     */

    Tcl_DictObjNext(searchPtr, &keyObj, &valueObj, &done);
    if (done) {
	Tcl_ResetResult(interp);
	goto done;
    }

    /*
     * Stop the value from getting hit in any way by any traces on the key
     * variable.
     */

    Tcl_IncrRefCount(valueObj);
    if (Tcl_ObjSetVar2(interp, keyVarObj, NULL, keyObj,
	    TCL_LEAVE_ERR_MSG) == NULL) {
	TclDecrRefCount(valueObj);
	result = TCL_ERROR;
	goto done;
    }
    TclDecrRefCount(valueObj);
    if (Tcl_ObjSetVar2(interp, valueVarObj, NULL, valueObj,
	    TCL_LEAVE_ERR_MSG) == NULL) {
	result = TCL_ERROR;
	goto done;
    }

    /*
     * Run the script.
     */

    TclNRAddCallback(interp, DictForLoopCallback, searchPtr, keyVarObj,
	    valueVarObj, scriptObj);
    return TclNREvalObjEx(interp, scriptObj, 0, iPtr->cmdFramePtr, 3);

    /*
     * For unwinding everything once the iterating is done.
     */

  done:
    TclDecrRefCount(keyVarObj);
    TclDecrRefCount(valueVarObj);
    TclDecrRefCount(scriptObj);
    Tcl_DictObjDone(searchPtr);
    TclStackFree(interp, searchPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DictMapNRCmd --
 *
 *	These functions implement the "dict map" Tcl command.  See the user
 *	documentation for details on what it does, and TIP#405 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictMapNRCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Obj **varv, *keyObj, *valueObj;
    DictMapStorage *storagePtr;
    Tcl_Size varc;
    int done;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"{keyVarName valueVarName} dictionary script");
	return TCL_ERROR;
    }

    /*
     * Parse arguments.
     */

    if (TclListObjGetElements(interp, objv[1], &varc, &varv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (varc != 2) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"must have exactly two variable names", -1));
	Tcl_SetErrorCode(interp, "TCL", "SYNTAX", "dict", "map", (char *)NULL);
	return TCL_ERROR;
    }
    storagePtr = (DictMapStorage *)TclStackAlloc(interp, sizeof(DictMapStorage));
    if (Tcl_DictObjFirst(interp, objv[2], &storagePtr->search, &keyObj,
	    &valueObj, &done) != TCL_OK) {
	TclStackFree(interp, storagePtr);
	return TCL_ERROR;
    }
    if (done) {
	/*
	 * Note that this exit leaves an empty value in the result (due to
	 * command calling conventions) but that is OK since an empty value is
	 * an empty dictionary.
	 */

	TclStackFree(interp, storagePtr);
	return TCL_OK;
    }
    TclNewObj(storagePtr->accumulatorObj);
    TclListObjGetElements(NULL, objv[1], &varc, &varv);
    storagePtr->keyVarObj = varv[0];
    storagePtr->valueVarObj = varv[1];
    storagePtr->scriptObj = objv[3];

    /*
     * Make sure that these objects (which we need throughout the body of the
     * loop) don't vanish. Note that the dictionary internal rep is locked
     * internally so that updates, shimmering, etc are not a problem.
     */

    Tcl_IncrRefCount(storagePtr->accumulatorObj);
    Tcl_IncrRefCount(storagePtr->keyVarObj);
    Tcl_IncrRefCount(storagePtr->valueVarObj);
    Tcl_IncrRefCount(storagePtr->scriptObj);

    /*
     * Stop the value from getting hit in any way by any traces on the key
     * variable.
     */

    Tcl_IncrRefCount(valueObj);
    if (Tcl_ObjSetVar2(interp, storagePtr->keyVarObj, NULL, keyObj,
	    TCL_LEAVE_ERR_MSG) == NULL) {
	TclDecrRefCount(valueObj);
	goto error;
    }
    if (Tcl_ObjSetVar2(interp, storagePtr->valueVarObj, NULL, valueObj,
	    TCL_LEAVE_ERR_MSG) == NULL) {
	TclDecrRefCount(valueObj);
	goto error;
    }
    TclDecrRefCount(valueObj);

    /*
     * Run the script.
     */

    TclNRAddCallback(interp, DictMapLoopCallback, storagePtr, NULL,NULL,NULL);
    return TclNREvalObjEx(interp, storagePtr->scriptObj, 0,
	    iPtr->cmdFramePtr, 3);

    /*
     * For unwinding everything on error.
     */

  error:
    TclDecrRefCount(storagePtr->keyVarObj);
    TclDecrRefCount(storagePtr->valueVarObj);
    TclDecrRefCount(storagePtr->scriptObj);
    TclDecrRefCount(storagePtr->accumulatorObj);
    Tcl_DictObjDone(&storagePtr->search);
    TclStackFree(interp, storagePtr);
    return TCL_ERROR;
}

static int
DictMapLoopCallback(
    void *data[],
    Tcl_Interp *interp,
    int result)
{
    Interp *iPtr = (Interp *) interp;
    DictMapStorage *storagePtr = (DictMapStorage *)data[0];
    Tcl_Obj *keyObj, *valueObj;
    int done;

    /*
     * Process the result from the previous execution of the script body.
     */

    if (result == TCL_CONTINUE) {
	result = TCL_OK;
    } else if (result != TCL_OK) {
	if (result == TCL_BREAK) {
	    Tcl_ResetResult(interp);
	    result = TCL_OK;
	} else if (result == TCL_ERROR) {
	    Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
		    "\n    (\"dict map\" body line %d)",
		    Tcl_GetErrorLine(interp)));
	}
	goto done;
    } else {
	keyObj = Tcl_ObjGetVar2(interp, storagePtr->keyVarObj, NULL,
		TCL_LEAVE_ERR_MSG);
	if (keyObj == NULL) {
	    result = TCL_ERROR;
	    goto done;
	}
	Tcl_DictObjPut(NULL, storagePtr->accumulatorObj, keyObj,
		Tcl_GetObjResult(interp));
    }

    /*
     * Get the next mapping from the dictionary.
     */

    Tcl_DictObjNext(&storagePtr->search, &keyObj, &valueObj, &done);
    if (done) {
	Tcl_SetObjResult(interp, storagePtr->accumulatorObj);
	goto done;
    }

    /*
     * Stop the value from getting hit in any way by any traces on the key
     * variable.
     */

    Tcl_IncrRefCount(valueObj);
    if (Tcl_ObjSetVar2(interp, storagePtr->keyVarObj, NULL, keyObj,
	    TCL_LEAVE_ERR_MSG) == NULL) {
	TclDecrRefCount(valueObj);
	result = TCL_ERROR;
	goto done;
    }
    if (Tcl_ObjSetVar2(interp, storagePtr->valueVarObj, NULL, valueObj,
	    TCL_LEAVE_ERR_MSG) == NULL) {
	TclDecrRefCount(valueObj);
	result = TCL_ERROR;
	goto done;
    }
    TclDecrRefCount(valueObj);

    /*
     * Run the script.
     */

    TclNRAddCallback(interp, DictMapLoopCallback, storagePtr, NULL,NULL,NULL);
    return TclNREvalObjEx(interp, storagePtr->scriptObj, 0,
	    iPtr->cmdFramePtr, 3);

    /*
     * For unwinding everything once the iterating is done.
     */

  done:
    TclDecrRefCount(storagePtr->keyVarObj);
    TclDecrRefCount(storagePtr->valueVarObj);
    TclDecrRefCount(storagePtr->scriptObj);
    TclDecrRefCount(storagePtr->accumulatorObj);
    Tcl_DictObjDone(&storagePtr->search);
    TclStackFree(interp, storagePtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DictSetCmd --
 *
 *	This function implements the "dict set" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictSetCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *dictPtr, *resultPtr;
    int result, allocatedDict = 0;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictVarName key ?key ...? value");
	return TCL_ERROR;
    }

    dictPtr = Tcl_ObjGetVar2(interp, objv[1], NULL, 0);
    if (dictPtr == NULL) {
	allocatedDict = 1;
	dictPtr = Tcl_NewDictObj();
    } else if (Tcl_IsShared(dictPtr)) {
	allocatedDict = 1;
	dictPtr = Tcl_DuplicateObj(dictPtr);
    }

    result = Tcl_DictObjPutKeyList(interp, dictPtr, objc-3, objv+2,
	    objv[objc-1]);
    if (result != TCL_OK) {
	if (allocatedDict) {
	    TclDecrRefCount(dictPtr);
	}
	return TCL_ERROR;
    }

    resultPtr = Tcl_ObjSetVar2(interp, objv[1], NULL, dictPtr,
	    TCL_LEAVE_ERR_MSG);
    if (resultPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictUnsetCmd --
 *
 *	This function implements the "dict unset" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictUnsetCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *dictPtr, *resultPtr;
    int result, allocatedDict = 0;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictVarName key ?key ...?");
	return TCL_ERROR;
    }

    dictPtr = Tcl_ObjGetVar2(interp, objv[1], NULL, 0);
    if (dictPtr == NULL) {
	allocatedDict = 1;
	dictPtr = Tcl_NewDictObj();
    } else if (Tcl_IsShared(dictPtr)) {
	allocatedDict = 1;
	dictPtr = Tcl_DuplicateObj(dictPtr);
    }

    result = Tcl_DictObjRemoveKeyList(interp, dictPtr, objc-2, objv+2);
    if (result != TCL_OK) {
	if (allocatedDict) {
	    TclDecrRefCount(dictPtr);
	}
	return TCL_ERROR;
    }

    resultPtr = Tcl_ObjSetVar2(interp, objv[1], NULL, dictPtr,
	    TCL_LEAVE_ERR_MSG);
    if (resultPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DictFilterCmd --
 *
 *	This function implements the "dict filter" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictFilterCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    static const char *const filters[] = {
	"key", "script", "value", NULL
    };
    enum FilterTypes {
	FILTER_KEYS, FILTER_SCRIPT, FILTER_VALUES
    } index;
    Tcl_Obj *scriptObj, *keyVarObj, *valueVarObj;
    Tcl_Obj **varv, *keyObj = NULL, *valueObj = NULL, *resultObj, *boolObj;
    Tcl_DictSearch search;
    int done, result, satisfied;
    Tcl_Size varc;
    const char *pattern;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictionary filterType ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[2], filters, "filterType",
	     0, &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
    case FILTER_KEYS:
	/*
	 * Create a dictionary whose keys all match a certain pattern.
	 */

	if (Tcl_DictObjFirst(interp, objv[1], &search,
		&keyObj, &valueObj, &done) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (objc == 3) {
	    /*
	     * Nothing to match, so return nothing (== empty dictionary).
	     */

	    Tcl_DictObjDone(&search);
	    return TCL_OK;
	} else if (objc == 4) {
	    pattern = TclGetString(objv[3]);
	    resultObj = Tcl_NewDictObj();
	    if (TclMatchIsTrivial(pattern)) {
		/*
		 * Must release the search lock here to prevent a memory leak
		 * since we are not exhausing the search. [Bug 1705778, leak
		 * K05]
		 */

		Tcl_DictObjDone(&search);
		Tcl_DictObjGet(interp, objv[1], objv[3], &valueObj);
		if (valueObj != NULL) {
		    Tcl_DictObjPut(NULL, resultObj, objv[3], valueObj);
		}
	    } else {
		while (!done) {
		    if (Tcl_StringMatch(TclGetString(keyObj), pattern)) {
			Tcl_DictObjPut(NULL, resultObj, keyObj, valueObj);
		    }
		    Tcl_DictObjNext(&search, &keyObj, &valueObj, &done);
		}
	    }
	} else {
	    /*
	     * Can't optimize this match for trivial globbing: would disturb
	     * order.
	     */

	    resultObj = Tcl_NewDictObj();
	    while (!done) {
		int i;

		for (i=3 ; i<objc ; i++) {
		    pattern = TclGetString(objv[i]);
		    if (Tcl_StringMatch(TclGetString(keyObj), pattern)) {
			Tcl_DictObjPut(NULL, resultObj, keyObj, valueObj);
			break;		/* stop inner loop */
		    }
		}
		Tcl_DictObjNext(&search, &keyObj, &valueObj, &done);
	    }
	}
	Tcl_SetObjResult(interp, resultObj);
	return TCL_OK;

    case FILTER_VALUES:
	/*
	 * Create a dictionary whose values all match a certain pattern.
	 */

	if (Tcl_DictObjFirst(interp, objv[1], &search,
		&keyObj, &valueObj, &done) != TCL_OK) {
	    return TCL_ERROR;
	}
	resultObj = Tcl_NewDictObj();
	while (!done) {
	    int i;

	    for (i=3 ; i<objc ; i++) {
		pattern = TclGetString(objv[i]);
		if (Tcl_StringMatch(TclGetString(valueObj), pattern)) {
		    Tcl_DictObjPut(NULL, resultObj, keyObj, valueObj);
		    break;		/* stop inner loop */
		}
	    }
	    Tcl_DictObjNext(&search, &keyObj, &valueObj, &done);
	}
	Tcl_SetObjResult(interp, resultObj);
	return TCL_OK;

    case FILTER_SCRIPT:
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 1, objv,
		    "dictionary script {keyVarName valueVarName} filterScript");
	    return TCL_ERROR;
	}

	/*
	 * Create a dictionary whose key,value pairs all satisfy a script
	 * (i.e. get a true boolean result from its evaluation). Massive
	 * copying from the "dict for" implementation has occurred!
	 */

	if (TclListObjGetElements(interp, objv[3], &varc, &varv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (varc != 2) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "must have exactly two variable names", -1));
	    Tcl_SetErrorCode(interp, "TCL", "SYNTAX", "dict", "filter", (char *)NULL);
	    return TCL_ERROR;
	}
	keyVarObj = varv[0];
	valueVarObj = varv[1];
	scriptObj = objv[4];

	/*
	 * Make sure that these objects (which we need throughout the body of
	 * the loop) don't vanish. Note that the dictionary internal rep is
	 * locked internally so that updates, shimmering, etc are not a
	 * problem.
	 */

	Tcl_IncrRefCount(keyVarObj);
	Tcl_IncrRefCount(valueVarObj);
	Tcl_IncrRefCount(scriptObj);

	result = Tcl_DictObjFirst(interp, objv[1],
		&search, &keyObj, &valueObj, &done);
	if (result != TCL_OK) {
	    TclDecrRefCount(keyVarObj);
	    TclDecrRefCount(valueVarObj);
	    TclDecrRefCount(scriptObj);
	    return TCL_ERROR;
	}

	resultObj = Tcl_NewDictObj();

	while (!done) {
	    /*
	     * Stop the value from getting hit in any way by any traces on the
	     * key variable.
	     */

	    Tcl_IncrRefCount(keyObj);
	    Tcl_IncrRefCount(valueObj);
	    if (Tcl_ObjSetVar2(interp, keyVarObj, NULL, keyObj,
		    TCL_LEAVE_ERR_MSG) == NULL) {
		Tcl_AddErrorInfo(interp,
			"\n    (\"dict filter\" filter script key variable)");
		result = TCL_ERROR;
		goto abnormalResult;
	    }
	    if (Tcl_ObjSetVar2(interp, valueVarObj, NULL, valueObj,
		    TCL_LEAVE_ERR_MSG) == NULL) {
		Tcl_AddErrorInfo(interp,
			"\n    (\"dict filter\" filter script value variable)");
		result = TCL_ERROR;
		goto abnormalResult;
	    }

	    /*
	     * TIP #280. Make invoking context available to loop body.
	     */

	    result = TclEvalObjEx(interp, scriptObj, 0, iPtr->cmdFramePtr, 4);
	    switch (result) {
	    case TCL_OK:
		boolObj = Tcl_GetObjResult(interp);
		Tcl_IncrRefCount(boolObj);
		Tcl_ResetResult(interp);
		if (Tcl_GetBooleanFromObj(interp, boolObj,
			&satisfied) != TCL_OK) {
		    TclDecrRefCount(boolObj);
		    result = TCL_ERROR;
		    goto abnormalResult;
		}
		TclDecrRefCount(boolObj);
		if (satisfied) {
		    Tcl_DictObjPut(NULL, resultObj, keyObj, valueObj);
		}
		break;
	    case TCL_BREAK:
		/*
		 * Force loop termination by calling Tcl_DictObjDone; this
		 * makes the next Tcl_DictObjNext say there is nothing more to
		 * do.
		 */

		Tcl_ResetResult(interp);
		Tcl_DictObjDone(&search);
		TCL_FALLTHROUGH();
	    case TCL_CONTINUE:
		result = TCL_OK;
		break;
	    case TCL_ERROR:
		Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
			"\n    (\"dict filter\" script line %d)",
			Tcl_GetErrorLine(interp)));
		TCL_FALLTHROUGH();
	    default:
		goto abnormalResult;
	    }

	    TclDecrRefCount(keyObj);
	    TclDecrRefCount(valueObj);

	    Tcl_DictObjNext(&search, &keyObj, &valueObj, &done);
	}

	/*
	 * Stop holding a reference to these objects.
	 */

	TclDecrRefCount(keyVarObj);
	TclDecrRefCount(valueVarObj);
	TclDecrRefCount(scriptObj);
	Tcl_DictObjDone(&search);

	if (result == TCL_OK) {
	    Tcl_SetObjResult(interp, resultObj);
	} else {
	    TclDecrRefCount(resultObj);
	}
	return result;
    }
    TCL_UNREACHABLE();

  abnormalResult:
    Tcl_DictObjDone(&search);
    TclDecrRefCount(keyObj);
    TclDecrRefCount(valueObj);
    TclDecrRefCount(keyVarObj);
    TclDecrRefCount(valueVarObj);
    TclDecrRefCount(scriptObj);
    TclDecrRefCount(resultObj);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DictUpdateCmd --
 *
 *	This function implements the "dict update" Tcl command. See the user
 *	documentation for details on what it does, and TIP#212 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictUpdateCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Obj *dictPtr, *objPtr;
    int i;
    Tcl_Size dummy;

    if (objc < 5 || !(objc & 1)) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"dictVarName key varName ?key varName ...? script");
	return TCL_ERROR;
    }

    dictPtr = Tcl_ObjGetVar2(interp, objv[1], NULL, TCL_LEAVE_ERR_MSG);
    if (dictPtr == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_DictObjSize(interp, dictPtr, &dummy) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_IncrRefCount(dictPtr);
    for (i=2 ; i+2<objc ; i+=2) {
	if (Tcl_DictObjGet(interp, dictPtr, objv[i], &objPtr) != TCL_OK) {
	    TclDecrRefCount(dictPtr);
	    return TCL_ERROR;
	}
	if (objPtr == NULL) {
	    /* ??? */
	    Tcl_UnsetVar2(interp, TclGetString(objv[i+1]), NULL, 0);
	} else if (Tcl_ObjSetVar2(interp, objv[i+1], NULL, objPtr,
		TCL_LEAVE_ERR_MSG) == NULL) {
	    TclDecrRefCount(dictPtr);
	    return TCL_ERROR;
	}
    }
    TclDecrRefCount(dictPtr);

    /*
     * Execute the body after setting up the NRE handler to process the
     * results.
     */

    objPtr = Tcl_NewListObj(objc-3, objv+2);
    Tcl_IncrRefCount(objPtr);
    Tcl_IncrRefCount(objv[1]);
    TclNRAddCallback(interp, FinalizeDictUpdate, objv[1], objPtr, NULL,NULL);

    return TclNREvalObjEx(interp, objv[objc-1], 0, iPtr->cmdFramePtr, objc-1);
}

static int
FinalizeDictUpdate(
    void *data[],
    Tcl_Interp *interp,
    int result)
{
    Tcl_Obj *dictPtr, *objPtr, **objv;
    Tcl_InterpState state;
    Tcl_Size i, objc;
    Tcl_Obj *varName = (Tcl_Obj *)data[0];
    Tcl_Obj *argsObj = (Tcl_Obj *)data[1];

    /*
     * ErrorInfo handling.
     */

    if (result == TCL_ERROR) {
	Tcl_AddErrorInfo(interp, "\n    (body of \"dict update\")");
    }

    /*
     * If the dictionary variable doesn't exist, drop everything silently.
     */

    dictPtr = Tcl_ObjGetVar2(interp, varName, NULL, 0);
    if (dictPtr == NULL) {
	TclDecrRefCount(varName);
	TclDecrRefCount(argsObj);
	return result;
    }

    /*
     * Double-check that it is still a dictionary.
     */

    state = Tcl_SaveInterpState(interp, result);
    if (Tcl_DictObjSize(interp, dictPtr, &objc) != TCL_OK) {
	Tcl_DiscardInterpState(state);
	TclDecrRefCount(varName);
	TclDecrRefCount(argsObj);
	return TCL_ERROR;
    }

    if (Tcl_IsShared(dictPtr)) {
	dictPtr = Tcl_DuplicateObj(dictPtr);
    }

    /*
     * Write back the values from the variables, treating failure to read as
     * an instruction to remove the key.
     */

    TclListObjGetElements(NULL, argsObj, &objc, &objv);
    for (i=0 ; i<objc ; i+=2) {
	objPtr = Tcl_ObjGetVar2(interp, objv[i+1], NULL, 0);
	if (objPtr == NULL) {
	    Tcl_DictObjRemove(NULL, dictPtr, objv[i]);
	} else if (objPtr == dictPtr) {
	    /*
	     * Someone is messing us around, trying to build a recursive
	     * structure. [Bug 1786481]
	     */

	    Tcl_DictObjPut(NULL, dictPtr, objv[i], Tcl_DuplicateObj(objPtr));
	} else {
	    /* Shouldn't fail */
	    Tcl_DictObjPut(NULL, dictPtr, objv[i], objPtr);
	}
    }
    TclDecrRefCount(argsObj);

    /*
     * Write the dictionary back to its variable.
     */

    if (Tcl_ObjSetVar2(interp, varName, NULL, dictPtr,
	    TCL_LEAVE_ERR_MSG) == NULL) {
	Tcl_DiscardInterpState(state);
	TclDecrRefCount(varName);
	return TCL_ERROR;
    }

    TclDecrRefCount(varName);
    return Tcl_RestoreInterpState(interp, state);
}

/*
 *----------------------------------------------------------------------
 *
 * DictWithCmd --
 *
 *	This function implements the "dict with" Tcl command. See the user
 *	documentation for details on what it does, and TIP#212 for the formal
 *	specification.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
DictWithCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Obj *dictPtr, *keysPtr, *pathPtr;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "dictVarName ?key ...? script");
	return TCL_ERROR;
    }

    /*
     * Get the dictionary to open out.
     */

    dictPtr = Tcl_ObjGetVar2(interp, objv[1], NULL, TCL_LEAVE_ERR_MSG);
    if (dictPtr == NULL) {
	return TCL_ERROR;
    }

    keysPtr = TclDictWithInit(interp, dictPtr, objc-3, objv+2);
    if (keysPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_IncrRefCount(keysPtr);

    /*
     * Execute the body, while making the invoking context available to the
     * loop body (TIP#280) and postponing the cleanup until later (NRE).
     */

    pathPtr = NULL;
    if (objc > 3) {
	pathPtr = Tcl_NewListObj(objc-3, objv+2);
	Tcl_IncrRefCount(pathPtr);
    }
    Tcl_IncrRefCount(objv[1]);
    TclNRAddCallback(interp, FinalizeDictWith, objv[1], keysPtr, pathPtr,
	    NULL);

    return TclNREvalObjEx(interp, objv[objc-1], 0, iPtr->cmdFramePtr, objc-1);
}

static int
FinalizeDictWith(
    void *data[],
    Tcl_Interp *interp,
    int result)
{
    Tcl_Obj **pathv;
    Tcl_Size pathc;
    Tcl_InterpState state;
    Tcl_Obj *varName = (Tcl_Obj *)data[0];
    Tcl_Obj *keysPtr = (Tcl_Obj *)data[1];
    Tcl_Obj *pathPtr = (Tcl_Obj *)data[2];
    Var *varPtr, *arrayPtr;

    if (result == TCL_ERROR) {
	Tcl_AddErrorInfo(interp, "\n    (body of \"dict with\")");
    }

    /*
     * Save the result state; TDWF doesn't guarantee to not modify that on
     * TCL_OK result.
     */

    state = Tcl_SaveInterpState(interp, result);
    if (pathPtr != NULL) {
	TclListObjGetElements(NULL, pathPtr, &pathc, &pathv);
    } else {
	pathc = 0;
	pathv = NULL;
    }

    /*
     * Pack from local variables back into the dictionary.
     */

    varPtr = TclObjLookupVarEx(interp, varName, NULL, TCL_LEAVE_ERR_MSG, "set",
	    /*createPart1*/ 1, /*createPart2*/ 1, &arrayPtr);
    if (varPtr == NULL) {
	result = TCL_ERROR;
    } else {
	result = TclDictWithFinish(interp, varPtr, arrayPtr, varName, NULL, -1,
		pathc, pathv, keysPtr);
    }

    /*
     * Tidy up and return the real result (unless we had an error).
     */

    TclDecrRefCount(varName);
    TclDecrRefCount(keysPtr);
    if (pathPtr != NULL) {
	TclDecrRefCount(pathPtr);
    }
    if (result != TCL_OK) {
	Tcl_DiscardInterpState(state);
	return TCL_ERROR;
    }
    return Tcl_RestoreInterpState(interp, state);
}

/*
 *----------------------------------------------------------------------
 *
 * TclDictWithInit --
 *
 *	Part of the core of [dict with]. Pokes into a dictionary and converts
 *	the mappings there into assignments to (presumably) local variables.
 *	Returns a list of all the names that were mapped so that removal of
 *	either the variable or the dictionary entry won't surprise us when we
 *	come to stuffing everything back.
 *
 * Result:
 *	List of mapped names, or NULL if there was an error.
 *
 * Side effects:
 *	Assigns to variables, so potentially legion due to traces.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclDictWithInit(
    Tcl_Interp *interp,
    Tcl_Obj *dictPtr,
    Tcl_Size pathc,
    Tcl_Obj *const pathv[])
{
    Tcl_DictSearch s;
    Tcl_Obj *keyPtr, *valPtr, *keysPtr;
    int done;

    if (pathc > 0) {
	dictPtr = TclTraceDictPath(interp, dictPtr, pathc, pathv,
		DICT_PATH_READ);
	if (dictPtr == NULL) {
	    return NULL;
	}
    }

    /*
     * Go over the list of keys and write each corresponding value to a
     * variable in the current context with the same name. Also keep a copy of
     * the keys so we can write back properly later on even if the dictionary
     * has been structurally modified.
     */

    if (Tcl_DictObjFirst(interp, dictPtr, &s, &keyPtr, &valPtr,
	    &done) != TCL_OK) {
	return NULL;
    }

    TclNewObj(keysPtr);

    for (; !done ; Tcl_DictObjNext(&s, &keyPtr, &valPtr, &done)) {
	Tcl_ListObjAppendElement(NULL, keysPtr, keyPtr);
	if (Tcl_ObjSetVar2(interp, keyPtr, NULL, valPtr,
		TCL_LEAVE_ERR_MSG) == NULL) {
	    TclDecrRefCount(keysPtr);
	    Tcl_DictObjDone(&s);
	    return NULL;
	}
    }

    return keysPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclDictWithFinish --
 *
 *	Part of the core of [dict with]. Reassembles the piece of the dict (in
 *	varName, location given by pathc/pathv) from the variables named in
 *	the keysPtr argument. NB, does not try to preserve errors or manage
 *	argument lifetimes.
 *
 * Result:
 *	TCL_OK if we succeeded, or TCL_ERROR if we failed.
 *
 * Side effects:
 *	Assigns to a variable, so potentially legion due to traces. Updates
 *	the dictionary in the named variable.
 *
 *----------------------------------------------------------------------
 */

int
TclDictWithFinish(
    Tcl_Interp *interp,		/* Command interpreter in which variable
				 * exists. Used for state management, traces
				 * and error reporting. */
    Var *varPtr,		/* Reference to the variable holding the
				 * dictionary. */
    Var *arrayPtr,		/* Reference to the array containing the
				 * variable, or NULL if the variable is a
				 * scalar. */
    Tcl_Obj *part1Ptr,		/* Name of an array (if part2 is non-NULL) or
				 * the name of a variable. NULL if the 'index'
				 * parameter is >= 0 */
    Tcl_Obj *part2Ptr,		/* If non-NULL, gives the name of an element
				 * in the array part1. */
    Tcl_Size index,			/* Index into the local variable table of the
				 * variable, or -1. Only used when part1Ptr is
				 * NULL. */
    Tcl_Size pathc,			/* The number of elements in the path into the
				 * dictionary. */
    Tcl_Obj *const pathv[],	/* The elements of the path to the subdict. */
    Tcl_Obj *keysPtr)		/* List of keys to be synchronized. This is
				 * the result value from TclDictWithInit. */
{
    Tcl_Obj *dictPtr, *leafPtr, *valPtr;
    Tcl_Size i, allocdict, keyc;
    Tcl_Obj **keyv;

    /*
     * If the dictionary variable doesn't exist, drop everything silently.
     */

    dictPtr = TclPtrGetVarIdx(interp, varPtr, arrayPtr, part1Ptr, part2Ptr,
	    TCL_LEAVE_ERR_MSG, index);
    if (dictPtr == NULL) {
	return TCL_OK;
    }

    /*
     * Double-check that it is still a dictionary.
     */

    if (Tcl_DictObjSize(interp, dictPtr, &i) != TCL_OK) {
	return TCL_ERROR;
    }

    if (Tcl_IsShared(dictPtr)) {
	dictPtr = Tcl_DuplicateObj(dictPtr);
	allocdict = 1;
    } else {
	allocdict = 0;
    }

    if (pathc > 0) {
	/*
	 * Want to get to the dictionary which we will update; need to do
	 * prepare-for-update unsharing along the path *but* avoid generating
	 * an error on a non-extant path (we'll treat that the same as a
	 * non-extant variable. Luckily, the unsharing operation isn't
	 * deeply damaging if we don't go on to update; it's just less than
	 * perfectly efficient (but no memory should be leaked).
	 */

	leafPtr = TclTraceDictPath(interp, dictPtr, pathc, pathv,
		DICT_PATH_EXISTS | DICT_PATH_UPDATE);
	if (leafPtr == NULL) {
	    if (allocdict) {
		TclDecrRefCount(dictPtr);
	    }
	    return TCL_ERROR;
	}
	if (leafPtr == DICT_PATH_NON_EXISTENT) {
	    if (allocdict) {
		TclDecrRefCount(dictPtr);
	    }
	    return TCL_OK;
	}
    } else {
	leafPtr = dictPtr;
    }

    /*
     * Now process our updates on the leaf dictionary.
     */

    TclListObjGetElements(NULL, keysPtr, &keyc, &keyv);
    for (i=0 ; i<keyc ; i++) {
	valPtr = Tcl_ObjGetVar2(interp, keyv[i], NULL, 0);
	if (valPtr == NULL) {
	    Tcl_DictObjRemove(NULL, leafPtr, keyv[i]);
	} else if (leafPtr == valPtr) {
	    /*
	     * Someone is messing us around, trying to build a recursive
	     * structure. [Bug 1786481]
	     */

	    Tcl_DictObjPut(NULL, leafPtr, keyv[i], Tcl_DuplicateObj(valPtr));
	} else {
	    Tcl_DictObjPut(NULL, leafPtr, keyv[i], valPtr);
	}
    }

    /*
     * Ensure that none of the dictionaries in the chain still have a string
     * rep.
     */

    if (pathc > 0) {
	InvalidateDictChain(leafPtr);
    }

    /*
     * Write back the outermost dictionary to the variable.
     */

    if (TclPtrSetVarIdx(interp, varPtr, arrayPtr, part1Ptr, part2Ptr,
	    dictPtr, TCL_LEAVE_ERR_MSG, index) == NULL) {
	if (allocdict) {
	    TclDecrRefCount(dictPtr);
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitDictCmd --
 *
 *	This function is create the "dict" Tcl command. See the user
 *	documentation for details on what it does, and TIP#111 for the formal
 *	specification.
 *
 * Results:
 *	A Tcl command handle.
 *
 * Side effects:
 *	May advance compilation epoch.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
TclInitDictCmd(
    Tcl_Interp *interp)
{
    return TclMakeEnsemble(interp, "dict", implementationMap);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
