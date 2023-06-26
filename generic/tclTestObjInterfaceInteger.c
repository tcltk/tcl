/*
 * tclTestObjInterfce.c --
 *
 *	This file contains C command functions for the additional Tcl commands
 *	that are used for testing implementations of the Tcl object types.
 *	These commands are not normally included in Tcl applications; they're
 *	only used for testing.
 *
 * Copyright © 2021 Nathan Coulter
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tcl.h"
#include "tclInt.h"

/*
 * Prototypes for functions defined later in this file:
 */
int TestListInteger (
    ClientData, Tcl_Interp *interp, Tcl_Size argc, Tcl_Obj *const objv[]);
static Tcl_Obj* NewTestListInteger();
static void	DupTestListIntegerInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);

static void	FreeTestListIntegerInternalRep(Tcl_Obj *objPtr);
static int	SetTestListIntegerFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void	UpdateStringOfTestListInteger(Tcl_Obj *listPtr);

static int ListIntegerListStringIndex (tclObjTypeInterfaceArgsStringIndex);
static int ListIntegerListStringIndexEnd(tclObjTypeInterfaceArgsStringIndexEnd);
static Tcl_Size ListIntegerListStringLength(tclObjTypeInterfaceArgsStringLength);
/*
static int ListIntegerStringListIndexFromStringIndex(
    Tcl_Size *index, Tcl_Size *itemchars, Tcl_Size *totalitems);
*/
static Tcl_Obj* ListIntegerListStringRange(tclObjTypeInterfaceArgsStringRange);
static Tcl_Obj* ListIntegerListStringRangeEnd(tclObjTypeInterfaceArgsStringRangeEnd);

static int ListIntegerListObjGetElements(tclObjTypeInterfaceArgsListAll);
static int ListIntegerListObjAppendElement(tclObjTypeInterfaceArgsListAppend);
static int ListIntegerListObjAppendList(tclObjTypeInterfaceArgsListAppendList);
static int ListIntegerListObjIndex(tclObjTypeInterfaceArgsListIndex);
static int ListIntegerListObjIndexEnd (tclObjTypeInterfaceArgsListIndexEnd);
static int ListIntegerListObjIsSorted(tclObjTypeInterfaceArgsListIsSorted);
static int ListIntegerListObjLength(tclObjTypeInterfaceArgsListLength);
static Tcl_Obj* ListIntegerListObjRange(tclObjTypeInterfaceArgsListRange);
static Tcl_Obj* ListIntegerListObjRangeEnd(tclObjTypeInterfaceArgsListRangeEnd);
static int ListIntegerListObjReplace(tclObjTypeInterfaceArgsListReplace);
static int ListIntegerListObjReplaceList(tclObjTypeInterfaceArgsListReplaceList);
static int ListIntegerListObjSetElement(tclObjTypeInterfaceArgsListSet);
static Tcl_Obj * ListIntegerLsetFlat(tclObjTypeInterfaceArgsListSetList);

static int ErrorMaxElementsExceeded(Tcl_Interp *interp);


ObjInterface ListIntegerInterface = {
    1,
    {
	&ListIntegerListStringIndex,
	&ListIntegerListStringIndexEnd,
	&ListIntegerListStringLength,
	&ListIntegerListStringRange,
	&ListIntegerListStringRangeEnd
    },
    {
	&ListIntegerListObjGetElements,
	&ListIntegerListObjAppendElement,
	&ListIntegerListObjAppendList,
	&ListIntegerListObjIndex,
	&ListIntegerListObjIndexEnd,
	&ListIntegerListObjIsSorted,
	&ListIntegerListObjLength,
	&ListIntegerListObjRange,
	&ListIntegerListObjRangeEnd,
	&ListIntegerListObjReplace,
	&ListIntegerListObjReplaceList,
	NULL,
	&ListIntegerListObjSetElement,
	&ListIntegerLsetFlat
    }
};


typedef struct ListInteger {
    int refCount;
    int ownstring;
    int size;
    int used;
    int values[1];
} ListInteger;


static ListInteger* NewTestListIntegerIntrep();
static ListInteger* ListGetInternalRep(Tcl_Obj *listPtr);
static void ListIntegerDecrRefCount(ListInteger *listIntegerPtr);

const ObjectType testListIntegerType = {
    "testListInteger",
    FreeTestListIntegerInternalRep,	/* freeIntRepProc */
    DupTestListIntegerInternalRep,		/* dupIntRepProc */
    UpdateStringOfTestListInteger,		/* updateStringProc */
    SetTestListIntegerFromAny,		/* setFromAnyProc */
    2,
    (Tcl_ObjInterface *)&ListIntegerInterface
};

const Tcl_ObjType *testListIntegerTypePtr = (Tcl_ObjType *)&testListIntegerType;



int TcltestObjectInterfaceListIntegerInit(Tcl_Interp *interp) {
    Tcl_CreateObjCommand2(interp, "testlistinteger", TestListInteger, NULL, NULL);
    return TCL_OK;
}

int TestListInteger(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size argc,
    Tcl_Obj *const objv[])
{
    int status;
    if (argc != 2) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # arguments", -1));
	}
	return TCL_ERROR;
    }
    status = Tcl_ConvertToType(interp, objv[1], testListIntegerTypePtr);
    Tcl_SetObjResult(interp, objv[1]);
    return status;
}


Tcl_Obj*
NewTestListInteger() {
    Tcl_ObjInternalRep intrep;
    Tcl_Obj *listPtr = Tcl_NewObj();
    Tcl_InvalidateStringRep(listPtr);
    ListInteger *listIntegerPtr = NewTestListIntegerIntrep();
    intrep.twoPtrValue.ptr1 = listIntegerPtr;
    Tcl_StoreInternalRep(listPtr, testListIntegerTypePtr, &intrep);
    return listPtr;
}


ListInteger*
NewTestListIntegerIntrep() {
    ListInteger *listIntegerPtr = (ListInteger *)Tcl_Alloc(sizeof(ListInteger));
    listIntegerPtr->refCount = 1;
    listIntegerPtr->ownstring = 0;
    listIntegerPtr->size = 1;
    listIntegerPtr->used = 0;
    return listIntegerPtr;
}

static ListInteger* ListGetInternalRep(Tcl_Obj *listPtr) {
    return (ListInteger *)listPtr->internalRep.twoPtrValue.ptr1;
}




static void DupTestListIntegerInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr) {
    Tcl_ObjInternalRep intrep;
    ListInteger *listRepPtr = ListGetInternalRep(srcPtr);
    listRepPtr->refCount++;
    intrep.twoPtrValue.ptr1 = listRepPtr;
    Tcl_StoreInternalRep(copyPtr, testListIntegerTypePtr, &intrep);
    return;
}

static void FreeTestListIntegerInternalRep(Tcl_Obj *listPtr) {
    ListInteger *listRepPtr = ListGetInternalRep(listPtr);
    ListIntegerDecrRefCount(listRepPtr);
    return;
}

static int SetTestListIntegerFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr) {
    int status;
    Tcl_Size i, length;
    Tcl_Obj *itemPtr, *listPtr;
    Tcl_ObjInternalRep intrep;
    ListInteger *listRepPtr;
    if (TclHasInternalRep(objPtr, testListIntegerTypePtr)) {
	return TCL_OK;
    } else {
	status = Tcl_ListObjLength(interp, objPtr, &length);
	if (status != TCL_OK) {
	    return TCL_ERROR;
	}
	listPtr = NewTestListInteger();
	for (i = 0; i < length; i++) {
	    status = Tcl_ListObjIndex(interp, objPtr, i, &itemPtr);
	    if (status != TCL_OK) {
		Tcl_DecrRefCount(listPtr);
		return status;
	    }
	    status = ListIntegerListObjReplace(interp, listPtr, i, 0, 1, &itemPtr);
	    if (status != TCL_OK) {
		Tcl_DecrRefCount(listPtr);
		return status;
	    }
	}
	listRepPtr = ListGetInternalRep(listPtr);
	intrep.twoPtrValue.ptr1 = listRepPtr;
	listRepPtr->refCount++;
	Tcl_StoreInternalRep(objPtr, testListIntegerTypePtr, &intrep);
	Tcl_DecrRefCount(listPtr);
	return TCL_OK;
    }
}

static void UpdateStringOfTestListInteger(Tcl_Obj *listPtr) {
    ListInteger *listRepPtr = ListGetInternalRep(listPtr);
    int i, num, used = listRepPtr->used;
    Tcl_Obj *strPtr, *numObjPtr;
    if (used > 0) {
	strPtr = Tcl_NewObj();
	Tcl_IncrRefCount(strPtr);
	num = listRepPtr->values[0];
	numObjPtr = Tcl_NewIntObj(num);
	Tcl_IncrRefCount(numObjPtr);
	Tcl_AppendFormatToObj(NULL, strPtr, "%d", 1, &numObjPtr);
	Tcl_DecrRefCount(numObjPtr);
	for (i = 1; i < used; i++) {
	    num = listRepPtr->values[i];
	    numObjPtr = Tcl_NewIntObj(num);
	    Tcl_IncrRefCount(numObjPtr);
	    Tcl_AppendFormatToObj(NULL, strPtr, " %d", 1, &numObjPtr);
	    Tcl_DecrRefCount(numObjPtr);
	}
	listPtr->bytes = strPtr->bytes;
	listPtr->length = strPtr->length;
	strPtr->bytes = 0;
	strPtr->length = 0;
	Tcl_DecrRefCount(strPtr);
    } else {
	Tcl_InitStringRep(listPtr, NULL, 0);
    }
    listRepPtr->ownstring = 1;
    return;
}

static void ListIntegerDecrRefCount(ListInteger *listIntegerPtr) {
    if (--listIntegerPtr->refCount <= 0) {
	Tcl_Free(listIntegerPtr);
    }
    return;
}

static int ListIntegerListStringIndex (
    TCL_UNUSED(Tcl_Interp *),/* Used to report errors if not NULL. */ \
    TCL_UNUSED(Tcl_Obj *),	/* List object to index into. */ \
    TCL_UNUSED(Tcl_Size),	/* Index of element to return. */ \
    TCL_UNUSED(Tcl_Obj **)/* The resulting Tcl_Obj* is stored here. */
)
{
    return TCL_ERROR;
}

static int ListIntegerListStringIndexEnd(
    TCL_UNUSED(Tcl_Interp *),/* Used to report errors if not NULL. */ \
    TCL_UNUSED(Tcl_Obj *),/* List object to index into. */ \
    TCL_UNUSED(Tcl_Size),/* Index of element to return. */ \
    TCL_UNUSED(Tcl_Obj **)/* The resulting Tcl_Obj* is stored here. */
) {
    return TCL_ERROR;
}

static Tcl_Size ListIntegerListStringLength(
    TCL_UNUSED(Tcl_Obj *);
) {
    return TCL_ERROR;
}

/*
static int ListIntegerStringListIndexFromStringIndex(
    TCL_UNUSEDVAR(Tcl_Size *index),
    TCL_UNUSEDVAR(Tcl_Size *itemchars),
    TCL_UNUSEDVAR(Tcl_Size *totalitems)
) {
    return TCL_ERROR;
}
*/

static Tcl_Obj* ListIntegerListStringRange(
    TCL_UNUSEDVAR(Tcl_Obj *objPtr),	/* The Tcl object to find the range of. */
    TCL_UNUSEDVAR(Tcl_Size first),	/* First index of the range. */
    TCL_UNUSEDVAR(Tcl_Size last)	/* Last index of the range. */
) {
    return NULL;
}

static Tcl_Obj* ListIntegerListStringRangeEnd(
    TCL_UNUSED(Tcl_Obj *),/* The Tcl object to find the range of. */
    TCL_UNUSED(Tcl_Size),/* First index of the range. */
    TCL_UNUSED(Tcl_Size) /* Last index of the range. */
) {
    return NULL;
}

static int ListIntegerListObjGetElements(
    TCL_UNUSEDVAR(Tcl_Interp *interp),	/* Used to report errors if not NULL. */
    TCL_UNUSEDVAR(Tcl_Obj *listPtr),	/* List object for which an element array
					 * is to be returned. */
    TCL_UNUSEDVAR(Tcl_Size *objcPtr),	/* Where to store the count of objects
					 * referenced by objv. */
    TCL_UNUSEDVAR(Tcl_Obj ***objvPtr)	/* Where to store the pointer to an
					 * array of */
) {
    ListInteger *listRepPtr;
    listRepPtr = ListGetInternalRep(listPtr);
    *objcPtr = listRepPtr->used;
    *objvPtr = listRepPtr->values;
    return TCL_OK;
}

static int ListIntegerListObjAppendElement(tclObjTypeInterfaceArgsListAppend) {
    int status;
    Tcl_Size length;
    status = Tcl_ListObjLength(interp, listPtr, &length);
    if (status != TCL_OK) {
	return TCL_ERROR;
    }
    return ListIntegerListObjReplace(interp, listPtr, length, 0, 1, &objPtr);
}

static int ListIntegerListObjAppendList(
    TCL_UNUSEDVAR(Tcl_Interp *interp),	    /* Used to report errors if not NULL. */ \
    TCL_UNUSEDVAR(Tcl_Obj *listPtr),	    /* List object to append elements to. */ \
    TCL_UNUSEDVAR(Tcl_Obj *elemListPtr)    /* List obj with elements to append. */
) {
    return TCL_ERROR;
}

static int ListIntegerListObjIndex(
    TCL_UNUSED(Tcl_Interp *),/* Used to report errors if not NULL. */ \
    Tcl_Obj * listObj,/* List object to index into. */ \
    Tcl_Size index,	/* Index of element to return. */ \
    Tcl_Obj **objPtrPtr	/* The resulting Tcl_Obj* is stored here. */
) {
    ListInteger *listRepPtr = ListGetInternalRep(listObj);
    Tcl_Size num;
    if (index >= 0 && index < listRepPtr->used) {
	num = listRepPtr->values[index];
	*objPtrPtr = Tcl_NewLongObj(num);
    } else {
	*objPtrPtr = NULL;
    }
    return TCL_OK;
}

static int ListIntegerListObjIndexEnd(
    TCL_UNUSED(Tcl_Interp *),/* Used to report errors if not NULL. */ \
    TCL_UNUSED(Tcl_Obj *),/* List object to index into. */ \
    TCL_UNUSED(Tcl_Size),/* Index of element to return. */ \
    TCL_UNUSED(Tcl_Obj **)/* The resulting Tcl_Obj* is stored here. */
) {
    return TCL_ERROR;
}

static int ListIntegerListObjIsSorted(
    TCL_UNUSED(Tcl_Interp *), /* Used to report errors */
    TCL_UNUSED(Tcl_Obj *),	/* The list in question */
    TCL_UNUSED(size_t) /* flags */
) {
    return TCL_ERROR;
}

static int ListIntegerListObjLength(
    TCL_UNUSED(Tcl_Interp *),	/* Used to report errors if not NULL. */
    Tcl_Obj * listObj,	/* List object whose #elements to return. */
    Tcl_Size *lenPtr		/* The resulting length is stored here. */
) {
    ListInteger *listRepPtr = ListGetInternalRep(listObj);
    *lenPtr = listRepPtr->used;
    return TCL_OK;
}

static Tcl_Obj* ListIntegerListObjRange(tclObjTypeInterfaceArgsListRange) {
    ListInteger *listRepPtr = ListGetInternalRep(listObj);
    Tcl_Size i, j, num, used = listRepPtr->used;
    Tcl_Obj *numObjPtr, *resPtr;

    if ((rangeStart == 0 && rangeEnd >= used - 1) || used == 0) {
	return listObj;
    }

    if (Tcl_IsShared(listObj) ||
	((listRepPtr->refCount > 1))) {
	if (rangeStart >= used || rangeEnd < rangeStart) {
	    return Tcl_NewObj();
	} else {
	    resPtr = NewTestListInteger();
	    for (i = rangeStart, j = 0; i <= rangeEnd; i++, j++) {
		num = listRepPtr->values[i];
		numObjPtr = Tcl_NewIntObj(num);
		Tcl_IncrRefCount(numObjPtr);
		if (ListIntegerListObjReplace(
		    interp, resPtr, j , 0 , 1 ,&numObjPtr) != TCL_OK) {
		    Tcl_DecrRefCount(resPtr);
		    Tcl_DecrRefCount(numObjPtr);
		    return NULL;
		}
		Tcl_DecrRefCount(numObjPtr);
	    }
	    return resPtr;
	}
    }
    return NULL;
}


static Tcl_Obj* ListIntegerListObjRangeEnd(
    TCL_UNUSEDVAR(Tcl_Interp * interp), /* Used to report errors */ \
    TCL_UNUSEDVAR(Tcl_Obj *listPtr),	/* List object to take a range from. */ \
    TCL_UNUSEDVAR(Tcl_Size fromAnchor),/* 0 for start and 1 for end */ \
    TCL_UNUSEDVAR(Tcl_Size fromIdx),	/* Index of first element to include. */ \
    TCL_UNUSEDVAR(Tcl_Size toAnchor),	/* 0 for start and 1 for end */  \
    TCL_UNUSEDVAR(Tcl_Size toIdx)	/* Index of last element to include. */
) {
    return NULL;
}


static int ListIntegerListObjReplace(tclObjTypeInterfaceArgsListReplace) {
    int i, status;
    Tcl_Obj *tmpListPtr = Tcl_NewObj();
    Tcl_IncrRefCount(tmpListPtr);
    for (i = 0; i < numToInsert; i++) {
	status = Tcl_ListObjAppendElement(interp, tmpListPtr, insertObjs[i]);
	if (status != TCL_OK) {
	    Tcl_DecrRefCount(tmpListPtr);
	    return status;
	}
    }
    status = ListIntegerListObjReplaceList(
	interp, listObj, first, numToDelete, tmpListPtr);
    Tcl_DecrRefCount(tmpListPtr);
    return status;
}


static int ListIntegerListObjReplaceList(tclObjTypeInterfaceArgsListReplaceList) {
    ListInteger *listRepPtr = ListGetInternalRep(listPtr);
    ListInteger *newListRepPtr;
    int changed = 0, itemInt, status;
    Tcl_Size i, index, newmemsize, itemsLength, j, newsize,
	     newtailindex, newused, size, newtailend, tailindex, tailsize,
	     used;
    Tcl_Obj *itemPtr;
    size = listRepPtr->size;
    used = listRepPtr->used;
    if (first < used) {
	tailsize = used - first;
    } else {
	tailsize = 0;
    }

    status = Tcl_ListObjLength(interp, newItemsPtr, &itemsLength);
    if (status != TCL_OK) {
	return TCL_ERROR;
    }

    /* Currently this duplicates checks found in of Tcl_ListObjReplace, but
     * maybe in the future Tcl remove those checks
    */

    if (first >= used) {
	first = used;
    } else if (first < 0) {
	first = 0;
    }

    if (count > tailsize) {
	count = tailsize;
    }

    /* If count == 0 and itemsLength == 0 this routine is logically a no-op,
     * but any non-canonical string representation must still be invalidated.
     */ 

    /* to do:
     * Recode this routine to work with incoming of unbounded length
     */

    if (used > 0) {
	tailindex = first + count;
	newtailindex = first + itemsLength;
	if (INT_MAX - tailsize - 1 < newtailindex) {
	    return ErrorMaxElementsExceeded(interp);
	}
	newused = newtailindex + tailsize;
	if (itemsLength > 0 && INT_MAX - itemsLength < newused) {
	    return ErrorMaxElementsExceeded(interp);
	}
    } else {
	tailindex = 0;
	newtailindex = 0;
	newused = itemsLength;
    }

    if (newused > size && newused > 1) {
	newsize = (newused + newused / 5 + 1);
	if (newsize < size) {
	    return ErrorMaxElementsExceeded(interp);
	}
    } else {
	newsize = size;
    }

    if (!listRepPtr->ownstring) {
	/* schedule canonicalization of the string rep */
	Tcl_InvalidateStringRep(listPtr);
	listRepPtr->ownstring = 1;
    }

    if (newused < used) {
	Tcl_InvalidateStringRep(listPtr);
    }


    newmemsize = sizeof(ListInteger) + newsize * sizeof(int) - sizeof(int);
    if (listRepPtr->refCount > 1) {
	Tcl_ObjInternalRep intrep;
	/* copy only the structure and the head of the old array */
	int movsize = sizeof(ListInteger)
	    + ((first + 1)  * sizeof(int) - sizeof(int));
	newListRepPtr = (ListInteger *)Tcl_Alloc(newmemsize);
	memmove(newListRepPtr, listRepPtr, movsize);
	newListRepPtr->size = newsize;
	newListRepPtr->refCount = 1;
	/* move the tail to its new location to make room for the new additions
	*/
	memmove(newListRepPtr->values + newtailindex,
	    listRepPtr->values + tailindex, tailsize * sizeof(int));
	intrep.twoPtrValue.ptr1 = newListRepPtr;
	Tcl_StoreInternalRep(listPtr, testListIntegerTypePtr, &intrep);
    } else {
	if (newsize > size && newused > 1) {
	    newListRepPtr = (ListInteger *)Tcl_Realloc(listRepPtr, newmemsize);
	} else {
	    newListRepPtr = listRepPtr;
	}
	newListRepPtr->size = newsize;
	if (tailsize > 0 && tailindex != newtailindex) {
	    /* move the tail to its new location to make room for the new
	     * additions */
	    memmove(newListRepPtr->values + newtailindex,
		    newListRepPtr->values + tailindex, tailsize);
	}
	listPtr->internalRep.twoPtrValue.ptr1 = newListRepPtr;
    }

    i = -1;
    while (1) {
	i++;
	index = first + i;
	status = Tcl_ListObjIndex(interp, newItemsPtr, i, &itemPtr);
	if (status != TCL_OK) {
	    return status;
	}
	if (itemPtr == NULL) {
	    break;
	}
	if (Tcl_GetIntFromObj(interp, itemPtr, &itemInt)
	    == TCL_OK) {
	    if (newListRepPtr->values[index] != itemInt) {
		changed = 1;
		newListRepPtr->values[index] = itemInt;
	    }
	    newListRepPtr->values[index] = itemInt;
	} else {
	    Tcl_Obj *realListPtr;
	    /* Fall back to normal list */
	    realListPtr = Tcl_NewListObj(newsize, NULL);
	    Tcl_IncrRefCount(realListPtr);

	    for (j = 0; j < index; j++) {
		itemPtr = Tcl_NewIntObj(newListRepPtr->values[j]);
		status = Tcl_ListObjAppendElement(
		    interp, realListPtr, itemPtr);
		if (status != TCL_OK) {
		    Tcl_DecrRefCount(realListPtr);
		    return status;
		}
	    }
	    while (1) {
		if (itemsLength == TCL_LENGTH_NONE) {
		    status = Tcl_ListObjLength(interp, newItemsPtr, &itemsLength);
		    if (status != TCL_OK) {
			Tcl_DecrRefCount(realListPtr);
			return status;
		    }
		}
		if (itemsLength != TCL_LENGTH_NONE && i >= itemsLength) {
		    break;
		}
		status = Tcl_ListObjIndex(interp, newItemsPtr, i, &itemPtr);
		if (status != TCL_OK) {
		    Tcl_DecrRefCount(realListPtr);
		    return status;
		}
		if (itemPtr == NULL) {
		    break;
		}
		status = Tcl_ListObjAppendElement(
		    interp, realListPtr, itemPtr);
		if (status != TCL_OK) {
		    Tcl_DecrRefCount(realListPtr);
		    return status;
		}
		i++;
	    }

	    newtailend = newtailindex + tailsize;
	    for (i = newtailindex; i < newtailend; i++) {
		itemPtr = Tcl_NewIntObj(newListRepPtr->values[i]);
		Tcl_ListObjAppendElement(interp, realListPtr, itemPtr);
		if (status != TCL_OK) {
		    Tcl_DecrRefCount(realListPtr);
		    return status;
		}
	    }

	    ListIntegerDecrRefCount(newListRepPtr);
	    listPtr->internalRep = realListPtr->internalRep;
	    listPtr->typePtr = realListPtr->typePtr;
	    realListPtr->typePtr = NULL;
	    Tcl_DecrRefCount(realListPtr);
	    /* this might not always be necessary, but probably the best that
	     * can be done in this case */
	    Tcl_InvalidateStringRep(listPtr);
	    return TCL_OK;
	}
    }

    if (changed) {
	Tcl_InvalidateStringRep(listPtr);
    }

    /* To make the operation transactional, update "used" only after all
     * elemnts have been succesfully added.
    */
    newListRepPtr->used = newused;
    return TCL_OK;
}


static int ListIntegerListObjSetElement(
    TCL_UNUSED(Tcl_Interp *),	/* Tcl interpreter; used for error reporting
				 * if not NULL. */
    TCL_UNUSED(Tcl_Obj *),	/* List object in which element should be
				 * stored. */
    TCL_UNUSED(Tcl_Size), 	/* Index of element to store. */
    TCL_UNUSED(Tcl_Obj *)	/* Tcl object to store in the designated list 
				 * element. */
) {
    return TCL_ERROR;
}


static Tcl_Obj * ListIntegerLsetFlat(
    TCL_UNUSED(Tcl_Interp *),	    /* Tcl interpreter. */ \
    TCL_UNUSED(Tcl_Obj *),	    /* Pointer to the list being modified. */ \
    TCL_UNUSED(Tcl_Size),    /* Number of index args. */ \
    TCL_UNUSEDVAR(Tcl_Obj *const indexArray[]),    /* Index args. */ \
    TCL_UNUSED(Tcl_Obj *)/* Value arg to 'lset' or NULL to 'lpop'. */
) {
    return NULL;
}


static int ErrorMaxElementsExceeded(Tcl_Interp *interp) {
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "max length of a Tcl list (%" TCL_T_MODIFIER "d elements) exceeded",
	    LIST_MAX));
    }
    return TCL_ERROR;
}
