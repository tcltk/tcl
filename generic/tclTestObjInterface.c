/*
 * Copyright © 2021-2024 Nathan Coulter
 *
 * You may distribute and/or modify this program under the terms of the GNU
 * Affero General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.

 * See the file "COPYING" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
*/

/*
 * tclTestObjInterface.c --
 *
 *	This file contains C command functions for the additional Tcl commands
 *	that are used for testing implementations of the Tcl object types.
 *	These commands are not normally included in Tcl applications; they're
 *	only used for testing.
 */

#include "tcl.h"
#include "tclInt.h"

/*
 * Prototypes for functions defined later in this file:
 */
typedef struct indexHex {
    int refCount;
    Tcl_Size offset;
} indexHex;


int TcltestObjectInterfaceInit(Tcl_Interp *interp);

int NewTestIndexHex (
    ClientData, Tcl_Interp *interp, Tcl_Size argc, Tcl_Obj *const objv[]);
static void		DupTestIndexHexInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void		FreeTestIndexHexInternalRep(Tcl_Obj *objPtr);
static int		SetTestIndexHexFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void		UpdateStringOfTestIndexHex(Tcl_Obj *listPtr);

static Tcl_ObjInterfaceStringIndexProc indexHexListStringIndex;
static Tcl_ObjInterfaceStringIndexEndProc indexHexListStringIndexEnd;
static Tcl_ObjInterfaceStringLengthProc indexHexListStringLength;
static int indexHexStringListIndexFromStringIndex(
    Tcl_Size *indexPtr, Tcl_Size *itemchars, Tcl_Size *totalitems);
static Tcl_ObjInterfaceStringRangeProc indexHexListStringRange;
static Tcl_ObjInterfaceStringRangeEndProc indexHexListStringRangeEnd;
static Tcl_ObjInterfaceListAllProc indexHexListObjGetElements;
static Tcl_ObjInterfaceListAppendProc indexHexListObjAppendElement;
static Tcl_ObjInterfaceListAppendlistProc indexHexListObjAppendList;
static Tcl_ObjInterfaceListIndexProc indexHexListObjIndex;
static Tcl_ObjInterfaceListIndexEndProc indexHexListObjIndexEnd;
static Tcl_ObjInterfaceListIsSortedProc indexHexListObjIsSorted;
static Tcl_ObjInterfaceListLengthProc indexHexListObjLength;
static Tcl_ObjInterfaceListRangeProc indexHexListObjRange;
static Tcl_ObjInterfaceListRangeEndProc indexHexListObjRangeEnd;
static Tcl_ObjInterfaceListReplaceProc indexHexListObjReplace;
static Tcl_ObjInterfaceListSetProc indexHexListObjSet;
static Tcl_ObjInterfaceListSetDeepProc indexHexListObjSetDeep;

static indexHex *GetTestIndexHexInternalRep(Tcl_Obj *objPtr);
static int indexHexListErrorIndeterminate (Tcl_Interp *interp);
static int indexHexListErrorReadOnly (Tcl_Interp *interp);


Tcl_ObjType *testIndexHexTypePtr;


int TcltestObjectInterfaceInit(Tcl_Interp *interp) {
    testIndexHexTypePtr = Tcl_NewObjType();
    Tcl_ObjTypeSetName(testIndexHexTypePtr ,(char *)"testindexHex");
    Tcl_ObjTypeSetFreeInternalRepProc(testIndexHexTypePtr , FreeTestIndexHexInternalRep);
    Tcl_ObjTypeSetDupInternalRepProc(testIndexHexTypePtr, DupTestIndexHexInternalRep);
    Tcl_ObjTypeSetUpdateStringProc(testIndexHexTypePtr, UpdateStringOfTestIndexHex);
    Tcl_ObjTypeSetSetFromAnyProc(testIndexHexTypePtr ,SetTestIndexHexFromAny);
    Tcl_ObjTypeSetVersion(testIndexHexTypePtr ,2);

    Tcl_ObjInterface * oiPtr = Tcl_NewObjInterface();
    Tcl_ObjInterfaceSetVersion(oiPtr ,1);

    Tcl_ObjInterfaceSetFnStringIndex(oiPtr ,indexHexListStringIndex);
    Tcl_ObjInterfaceSetFnStringIndexEnd(oiPtr ,indexHexListStringIndexEnd);
    Tcl_ObjInterfaceSetFnStringLength(oiPtr ,indexHexListStringLength);
    Tcl_ObjInterfaceSetFnStringRange(oiPtr ,indexHexListStringRange);
    Tcl_ObjInterfaceSetFnStringRangeEnd(oiPtr ,indexHexListStringRangeEnd);

    Tcl_ObjInterfaceSetFnListAll(oiPtr ,indexHexListObjGetElements);
    Tcl_ObjInterfaceSetFnListAppend(oiPtr ,indexHexListObjAppendElement);
    Tcl_ObjInterfaceSetFnListAppendList(oiPtr ,indexHexListObjAppendList);
    Tcl_ObjInterfaceSetFnListIndex(oiPtr ,indexHexListObjIndex);
    Tcl_ObjInterfaceSetFnListIndexEnd(oiPtr ,indexHexListObjIndexEnd);
    Tcl_ObjInterfaceSetFnListIsSorted(oiPtr ,indexHexListObjIsSorted);
    Tcl_ObjInterfaceSetFnListLength(oiPtr ,indexHexListObjLength);
    Tcl_ObjInterfaceSetFnListRange(oiPtr ,indexHexListObjRange);
    Tcl_ObjInterfaceSetFnListRangeEnd(oiPtr ,indexHexListObjRangeEnd);
    Tcl_ObjInterfaceSetFnListReplace(oiPtr ,indexHexListObjReplace);
    Tcl_ObjInterfaceSetFnListSet(oiPtr ,indexHexListObjSet);
    Tcl_ObjInterfaceSetFnListSetDeep(oiPtr ,indexHexListObjSetDeep);
    
    Tcl_ObjTypeSetInterface(testIndexHexTypePtr ,oiPtr);

    Tcl_CreateObjCommand2(interp, "testindexhex", NewTestIndexHex, NULL, NULL);
    return TCL_OK;
}



int NewTestIndexHex (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size argc,
    Tcl_Obj *const objv[])
{
    Tcl_WideInt offset;
    Tcl_ObjInternalRep intrep;
    if (argc > 2) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("too many arguments", -1));
	    return TCL_ERROR;
	}
    }
    if (argc == 2) {
	if (Tcl_GetWideIntFromObj(interp, objv[1], &offset) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (offset < 0) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("bad offset", -1));
	    return TCL_ERROR;
	}
    }  else {
	offset = 0;
    }
    Tcl_Obj *objPtr = Tcl_NewObj();
    Tcl_InvalidateStringRep(objPtr);

    indexHex *indexHexPtr = (indexHex *)Tcl_Alloc(sizeof(indexHex));
    indexHexPtr->refCount = 1;
    indexHexPtr->offset = offset;
    intrep.twoPtrValue.ptr1 = indexHexPtr;
    Tcl_StoreInternalRep(objPtr, testIndexHexTypePtr, &intrep);
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}


static void
DupTestIndexHexInternalRep(
    TCL_UNUSED(Tcl_Obj *),
    TCL_UNUSED(Tcl_Obj *))
{
    return;
}


static void
FreeTestIndexHexInternalRep(Tcl_Obj *objPtr)
{
    indexHex *indexHexPtr = GetTestIndexHexInternalRep(objPtr);
    if (--indexHexPtr->refCount == 0) {
	Tcl_Free(indexHexPtr);
    }
    return;
}


indexHex *
GetTestIndexHexInternalRep(Tcl_Obj *objPtr)
{
    indexHex *indexHexPtr = (indexHex *)objPtr->internalRep.twoPtrValue.ptr1;
    return indexHexPtr;
}


static int
SetTestIndexHexFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    if (TclHasInternalRep(objPtr, testIndexHexTypePtr)) {
	return TCL_OK;
    } else {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
		Tcl_NewStringObj("can not set an existing value to this type", -1));
	}
	return TCL_ERROR;
    }
}


static void
UpdateStringOfTestIndexHex(
    TCL_UNUSED(Tcl_Obj *))
{
    return;
}



static int indexHexListStringIndex(tclObjTypeInterfaceArgsStringIndex) {
    Tcl_Obj *hexPtr;
    int status;
    Tcl_Size itemchars, totalitems;

    status = indexHexStringListIndexFromStringIndex(
	&index, &itemchars, &totalitems);
    if (status != TCL_OK) {
	return TCL_ERROR;
    }

    status = indexHexListObjIndex(interp, objPtr, totalitems, &hexPtr);
    if (status != TCL_OK) {
	return TCL_ERROR;
    }

    if (index == itemchars - 1) {
	/* index refers to the space delimiter after the item. */
	*resPtrPtr = Tcl_NewStringObj(" ", -1);
    } else {
	*resPtrPtr = Tcl_GetRange(hexPtr, index, index);
    }
    Tcl_DecrRefCount(hexPtr);
    return status;
}


static int indexHexListErrorIndeterminate (Tcl_Interp *interp) {
    Tcl_SetObjResult(interp,
	Tcl_NewStringObj("list length indeterminate", -1));
    Tcl_SetErrorCode(interp, "TCL", "VALUE", "INDEX", "INDETERMINATE", NULL);
    return TCL_ERROR;
}


static int indexHexListErrorReadOnly (Tcl_Interp *interp) {
    if (interp != NULL) {
	Tcl_SetObjResult(interp,
	    Tcl_NewStringObj("list length indeterminate", -1));
	Tcl_SetErrorCode(interp, "TCL", "VALUE", "INDEX", "INTERFACE",
	    "READONLY", NULL);
    }
    return TCL_ERROR;
}


static int indexHexStringListIndexFromStringIndex(
    Tcl_Size *indexPtr, Tcl_Size *itemcharsPtr, Tcl_Size *totalitemsPtr)
{
    Tcl_Size itemoffset, last = 0, power = 1, lasttotalchars = 0, newitems,
       top, totalchars = 0;

    /* add 1 for the space after the item */
    *itemcharsPtr = power + 1;
    *totalitemsPtr = 0;

    /* Count the number of characters in the items that contain fewer
     * characters than the item containing the requested index. */
    while (1) {
	top = 1u << (4 * power);
	if (top < 1u << (4 * (power - 1))) {
	    /* operation wrapped around */
	    power -= 1;
	    break;
	}
	newitems = top - last;
	lasttotalchars = totalchars;
	totalchars += newitems * *itemcharsPtr;
	last = top;
	if (*indexPtr < totalchars) {
	    break;
	}
	power += 1;
	*itemcharsPtr += 1;
	*totalitemsPtr += newitems;
    }

    *indexPtr -= lasttotalchars;
    /* Determine how many items containing the same number of characters
     * precede the requested item. */
    itemoffset = *indexPtr / *itemcharsPtr ;
    *indexPtr = *indexPtr % *itemcharsPtr;
    /* Add the number of new characters. */
    *totalitemsPtr += itemoffset;
    return TCL_OK;
}


static int indexHexListStringIndexEnd(
    Tcl_Interp *interp,	
    TCL_UNUSED(Tcl_Obj *),
    TCL_UNUSED(Tcl_Size),
    TCL_UNUSED(Tcl_Obj **)
) {
    return indexHexListErrorIndeterminate(interp);
}

static int indexHexListStringLength(
    TCL_UNUSED(Tcl_Obj *)
    ,Tcl_Size *length
) {
    *length = -1;
    return TCL_OK;
}

static int indexHexListStringRange(tclObjTypeInterfaceArgsStringRange) {
    /* suppress unused parameter warning */
    (void) interp;

    Tcl_Obj *itemPtr, *item2Ptr, *resPtr;
    Tcl_Size index = first, status;
    Tcl_Size itemchars, needed, rangeLength, newStringLength,
       stringLength, totalitems;

    if (last < first) {
	*resPtrPtr = Tcl_NewStringObj("", -1);
	return TCL_OK;
    }

    status = indexHexStringListIndexFromStringIndex(
	&index, &itemchars, &totalitems);
    if (status != TCL_OK) {
	return status;
    }

    status = indexHexListObjIndex(NULL, objPtr, totalitems, &itemPtr);
    if (status != TCL_OK) {
	return status;
    }

    rangeLength = last - first + 1;

    resPtr = Tcl_GetRange(itemPtr, index, index + rangeLength - 1);
    Tcl_DecrRefCount(itemPtr);

    stringLength = Tcl_GetCharLength(resPtr);
    if (stringLength < rangeLength) {
	needed = rangeLength - stringLength;
	while (needed > 0) {
	    totalitems++;
	    status = indexHexListObjIndex(NULL, objPtr, totalitems, &itemPtr);
	    if (status != TCL_OK) {
		return status;
	    }
	    Tcl_AppendToObj(resPtr, " ", 1);
	    stringLength += newStringLength;
	    needed -= 1;
	    if (needed > 0) {
		newStringLength = Tcl_GetCharLength(itemPtr);
		if (newStringLength >= needed) {
		    item2Ptr = Tcl_GetRange(itemPtr, 0, needed-1);
		    newStringLength = Tcl_GetCharLength(item2Ptr);
		    Tcl_AppendObjToObj(resPtr, item2Ptr);
		    Tcl_DecrRefCount(item2Ptr);
		} else {
		    Tcl_AppendObjToObj(resPtr, itemPtr);
		}
		stringLength += newStringLength;
		needed -= newStringLength;
	    }
	    Tcl_DecrRefCount(itemPtr);
	}
    }
    *resPtrPtr = resPtr;
    return TCL_OK;
}

static int indexHexListStringRangeEnd(
    Tcl_Interp *interp,
    TCL_UNUSED(Tcl_Obj *),/* The Tcl object to find the range of. */
    TCL_UNUSED(Tcl_Size),/* First index of the range. */
    TCL_UNUSED(Tcl_Size), /* Last index of the range. */
    Tcl_Obj **resultPtr
) {
    (void) interp;
    *resultPtr = NULL;
    return TCL_OK;
}



static int
indexHexListObjGetElements(
    Tcl_Interp *interp,	/* Used to report errors if not NULL. */
    TCL_UNUSED(Tcl_Obj *),/* List object for which an element array
			   * is to be returned. */
    TCL_UNUSED(Tcl_Size *),/* Where to store the count of objects
			    * referenced by objv. */
    TCL_UNUSED(Tcl_Obj ***)/* Where to store the pointer to an
			    * array of */
)
{
    if (interp != NULL) {
	Tcl_SetObjResult(interp,Tcl_NewStringObj("infinite list", -1));
    }
    return TCL_ERROR;
}


static int
indexHexListObjAppendElement(
    Tcl_Interp *interp,	/* Used to report errors if not NULL. */
    TCL_UNUSED(Tcl_Obj *),/* List object to append objPtr to. */
    TCL_UNUSED(Tcl_Obj *)/* Object to append to listPtr's list. */
)
{
    indexHexListErrorReadOnly(interp);
    return TCL_ERROR;
}


static int
indexHexListObjAppendList(
    Tcl_Interp *interp,	    /* Used to report errors if not NULL. */
    TCL_UNUSED(Tcl_Obj *),  /* List object to append elements to. */
    TCL_UNUSED(Tcl_Obj *)   /* List obj with elements to append. */
)
{
    indexHexListErrorReadOnly(interp);
    return TCL_ERROR;
}


static int
indexHexListObjIndex(
    TCL_UNUSED(Tcl_Interp *),/* Used to report errors if not NULL. */ \
    TCL_UNUSED(Tcl_Obj *),	/* List object to index into. */ \
    Tcl_Size index,	/* Index of element to return. */ \
    Tcl_Obj **objPtrPtr	/* The resulting Tcl_Obj* is stored here. */
)
{
    Tcl_Obj *resPtr;
    if (index > -1 && index < TCL_SIZE_MAX-2) {
	resPtr = Tcl_ObjPrintf("%" TCL_T_MODIFIER "x", index);
	*objPtrPtr = resPtr;
    } else {
	*objPtrPtr = NULL;
    }
    return TCL_OK;
}


static int
indexHexListObjIndexEnd(
    TCL_UNUSED(Tcl_Interp *),/* Used to report errors if not NULL. */ \
    TCL_UNUSED(Tcl_Obj *),/* List object to index into. */ \
    Tcl_Size index,/* Index of element to return. */ \
    Tcl_Obj **resPtrPtr	/* The resulting Tcl_Obj* is stored here. */
)
{

    Tcl_Obj *resPtr;
    Tcl_Size index2;
    index2 = TCL_SIZE_MAX - 1 - index;
    if (index2 < 0 || index >= TCL_SIZE_MAX) {
	*resPtrPtr = NULL;
    } else {
	resPtr = Tcl_ObjPrintf("%" TCL_T_MODIFIER "x", index2);
	*resPtrPtr = resPtr;
    }
    return TCL_OK;
}


static int indexHexListObjIsSorted(
    TCL_UNUSED(Tcl_Interp *), /* Used to report errors */
    TCL_UNUSED(Tcl_Obj *),	/* The list in question */
    TCL_UNUSED(size_t) /* flags */
)
{
    return 1;
}


static int
indexHexListObjLength(
    TCL_UNUSED(Tcl_Interp *),	/* Used to report errors if not NULL. */
    TCL_UNUSED(Tcl_Obj *),	/* List object whose #elements to return. */
    Tcl_Size *lenPtr		/* The resulting length is stored here. */
)
{
    *lenPtr = -1;
    return TCL_OK;
}


static int 
indexHexListObjRange(tclObjTypeInterfaceArgsListRange)
{
    Tcl_Obj *itemPtr, *resPtr;
    indexHex *indexHexPtr = GetTestIndexHexInternalRep(listPtr);
    Tcl_Size fromIdxOffset ,toIdxOffset;
    resPtr = Tcl_NewListObj(0, NULL);
    *resPtrPtr = resPtr;
    if (TCL_SIZE_MAX - fromIdx < indexHexPtr->offset) {
	/* fromIdx is beyonde the last index */
	return TCL_OK;
    } else {
	fromIdxOffset = fromIdx + indexHexPtr->offset;
    }
    if (TCL_SIZE_MAX - toIdx < indexHexPtr->offset) {
	toIdxOffset = TCL_SIZE_MAX - 1;
    } else {
	toIdxOffset = toIdx + indexHexPtr->offset;
	if (toIdxOffset >= TCL_SIZE_MAX) {
	    toIdxOffset = TCL_SIZE_MAX-1;
	}
    }
    while (fromIdxOffset <= toIdxOffset) {
	indexHexListObjIndex(interp, listPtr, fromIdxOffset, &itemPtr);
	if (itemPtr == NULL) {
	    break;
	}
	if (
	    Tcl_ListObjAppendElement(interp, resPtr, itemPtr) != TCL_OK
	) {
	    Tcl_DecrRefCount(resPtr);
	    *resPtrPtr = NULL;
	    return TCL_OK;
	}
	fromIdxOffset++;
    }
    return TCL_OK;
}


static int
indexHexListObjRangeEnd(tclObjTypeInterfaceArgsListRangeEnd) {
    if (fromAnchor == 1 || toAnchor == 1) {
	indexHexListErrorIndeterminate(interp);
	*resPtrPtr = NULL;
	return TCL_ERROR;
    }
    return indexHexListObjRange(interp, listPtr, fromIdx, toIdx, resPtrPtr);
}


static int
indexHexListObjReplace(
    Tcl_Interp *interp, /* Used for error reporting if not NULL. */ \
    TCL_UNUSED(Tcl_Obj *),   /* List object whose elements to replace. */ \
    TCL_UNUSED(Tcl_Size),     /* Index of first element to replace. */ \
    TCL_UNUSED(Tcl_Size),	/* Number of elements to replace. */ \
    TCL_UNUSED(Tcl_Size),	/* Number of objects to insert. */ \
        		/* An array of objc pointers to Tcl \
        		 * objects to insert. */ \
    TCL_UNUSEDVAR(Tcl_Obj *const insertObjs[])
)
{
    indexHexListErrorReadOnly(interp);
    return TCL_ERROR;
}


static int
indexHexListObjSet(
    Tcl_Interp *interp,		/* Tcl interpreter; used for error reporting
				 * if not NULL. */
    TCL_UNUSED(Tcl_Obj *),	/* List object in which element should be
				 * stored. */
    TCL_UNUSED(Tcl_Size),	/* Index of element to store. */
    TCL_UNUSED(Tcl_Obj *)/* Tcl object to store in the designated list 
				 * element. */
)
{
    indexHexListErrorReadOnly(interp);
    return TCL_ERROR;
}


static int indexHexListObjSetDeep (
    Tcl_Interp *interp,		/* Tcl interpreter. */ \
    TCL_UNUSED(Tcl_Obj *),	/* Pointer to the list being modified. */ \
    TCL_UNUSED(Tcl_Size),	/* Number of index args. */ \
    TCL_UNUSEDVAR(Tcl_Obj *const indexArray[]),	/* Index args. */ \
    TCL_UNUSED(Tcl_Obj *),   /* Value arg to 'lset' or NULL to 'lpop'. */
    Tcl_Obj **resPtrPtr)
{
    indexHexListErrorReadOnly(interp);
    *resPtrPtr = NULL;
    return TCL_ERROR;
}
