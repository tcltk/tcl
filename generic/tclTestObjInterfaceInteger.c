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
    ClientData, Tcl_Interp *interp, int argc, Tcl_Obj *const objv[]);
static Tcl_Obj* NewTestListInteger();
static void	DupTestListIntegerInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);

static void	FreeTestListIntegerInternalRep(Tcl_Obj *objPtr);
static int	SetTestListIntegerFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void	UpdateStringOfTestListInteger(Tcl_Obj *listPtr);

static int ListIntegerListStringIndex (tclObjTypeInterfaceArgsStringIndex);
static int ListIntegerListStringIndexEnd(tclObjTypeInterfaceArgsStringIndexEnd);
static size_t ListIntegerListStringLength(tclObjTypeInterfaceArgsStringLength);
static int ListIntegerStringListIndexFromStringIndex(
    int *index, size_t *itemchars, size_t *totalitems);
static Tcl_Obj* ListIntegerListStringRange(tclObjTypeInterfaceArgsStringRange);
static Tcl_Obj* ListIntegerListStringRangeEnd(tclObjTypeInterfaceArgsStringRangeEnd);

static int ListIntegerListObjGetElements (tclObjTypeInterfaceArgsListAll);
static int ListIntegerListObjAppendElement (tclObjTypeInterfaceArgsListAppend);
static int ListIntegerListObjAppendList (tclObjTypeInterfaceArgsListAppendList);
static int ListIntegerListObjIndex (tclObjTypeInterfaceArgsListIndex);
static int ListIntegerListObjIndexEnd (tclObjTypeInterfaceArgsListIndexEnd);
static int ListIntegerListObjIsSorted(tclObjTypeInterfaceArgsListIsSorted);
static int ListIntegerListObjLength (tclObjTypeInterfaceArgsListLength);
static Tcl_Obj* ListIntegerListObjRange (tclObjTypeInterfaceArgsListRange);
static Tcl_Obj* ListIntegerListObjRangeEnd (tclObjTypeInterfaceArgsListRangeEnd);
static int ListIntegerListObjReplace (tclObjTypeInterfaceArgsListReplace);
static int listIntegerListObjReplaceList(tclObjTypeInterfaceArgsListReplaceList);
static int ListIntegerListObjSetElement (tclObjTypeInterfaceArgsListSet);
static Tcl_Obj * ListIntegerLsetFlat (tclObjTypeInterfaceArgsListSetList);

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
	&ListIntegerListObjSetElement,
	&ListIntegerLsetFlat
    }
};


typedef struct ListInteger {
    int refCount;
    int size;
    int used;
    int values[1];
} ListInteger;


static void ListIntegerDecrRefCount(ListInteger *listIntegerPtr);

const ObjectType testListIntegerType = {
    (char *)&TclObjectTypeType0,
    FreeTestListIntegerInternalRep,	/* freeIntRepProc */
    DupTestListIntegerInternalRep,		/* dupIntRepProc */
    UpdateStringOfTestListInteger,		/* updateStringProc */
    SetTestListIntegerFromAny,		/* setFromAnyProc */
    2,
    "testListInteger",
    (Tcl_ObjInterface *)&ListIntegerInterface
};

const Tcl_ObjType *testListIntegerTypePtr = (Tcl_ObjType *)&testListIntegerType;



int TcltestObjectInterfaceListIntegerInit(Tcl_Interp *interp) {
    Tcl_CreateObjCommand(interp, "testlistinteger", TestListInteger, NULL, NULL);
    return TCL_OK;
}

int TestListInteger(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int argc,
    Tcl_Obj *const objv[])
{
    if (argc > 2) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("too many arguments", -1));
	    return TCL_ERROR;
	}
    }
    Tcl_Obj *objPtr = NewTestListInteger();
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}

Tcl_Obj*
NewTestListInteger() {
    Tcl_ObjInternalRep intrep;
    Tcl_Obj *listPtr = Tcl_NewObj();
    Tcl_InvalidateStringRep(listPtr);
    ListInteger *listIntegerPtr = (ListInteger *)Tcl_Alloc(sizeof(ListInteger));
    listIntegerPtr->refCount = 1;
    listIntegerPtr->size = 1;
    listIntegerPtr->used = 0;
    intrep.twoPtrValue.ptr1 = listIntegerPtr;
    Tcl_StoreInternalRep(listPtr, testListIntegerTypePtr, &intrep);
    return listPtr;
}


static void DupTestListIntegerInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr) {
}

static void FreeTestListIntegerInternalRep(Tcl_Obj *listPtr) {
    ListInteger *structPtr = listPtr->internalRep.twoPtrValue.ptr1;
    ListIntegerDecrRefCount(structPtr);
    return;
}

static int SetTestListIntegerFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr) {
    if (TclHasInternalRep(objPtr, testListIntegerTypePtr)) {
	return TCL_OK;
    } else {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
		Tcl_NewStringObj("to do", -1));
	}
	return TCL_ERROR;
    }
}

static void UpdateStringOfTestListInteger(Tcl_Obj *listPtr) {
    ListInteger *structPtr = listPtr->internalRep.twoPtrValue.ptr1;
    int i, num, used = structPtr->used;
    Tcl_Obj *strPtr, *numObjPtr;
    if (used > 0) {
	strPtr = Tcl_NewObj();
	Tcl_IncrRefCount(strPtr);
	num = structPtr->values[0];
	numObjPtr = Tcl_NewIntObj(num);
	Tcl_IncrRefCount(numObjPtr);
	Tcl_AppendFormatToObj(NULL, strPtr, "%d", 1, &numObjPtr);
	Tcl_DecrRefCount(numObjPtr);
	for (i = 1; i < used; i++) {
	    num = structPtr->values[i];
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
    return;
}

static void ListIntegerDecrRefCount(ListInteger *listIntegerPtr) {
    if (--listIntegerPtr->refCount <= 0) {
	Tcl_Free(listIntegerPtr);
    }
    return;
}

static int ListIntegerListStringIndex (tclObjTypeInterfaceArgsStringIndex) {
}

static int ListIntegerListStringIndexEnd(tclObjTypeInterfaceArgsStringIndexEnd) {
}

static size_t ListIntegerListStringLength(tclObjTypeInterfaceArgsStringLength) {
}

static int ListIntegerStringListIndexFromStringIndex(
    int *index, size_t *itemchars, size_t *totalitems) {
}

static Tcl_Obj* ListIntegerListStringRange(tclObjTypeInterfaceArgsStringRange) {
    return NULL;
}

static Tcl_Obj* ListIntegerListStringRangeEnd(tclObjTypeInterfaceArgsStringRangeEnd) {
    return NULL;
}

static int ListIntegerListObjGetElements (tclObjTypeInterfaceArgsListAll) {
    return TCL_ERROR;
}

static int ListIntegerListObjAppendElement (tclObjTypeInterfaceArgsListAppend) {
    return TCL_ERROR;
}

static int ListIntegerListObjAppendList (tclObjTypeInterfaceArgsListAppendList) {
    return TCL_ERROR;
}

static int ListIntegerListObjIndex (tclObjTypeInterfaceArgsListIndex) {
    ListInteger *structPtr = listPtr->internalRep.twoPtrValue.ptr1;
    int num;
    if (index >= 0 && index < structPtr->used) {
	num = structPtr->values[index];
	*objPtrPtr = Tcl_NewIntObj(num);
    } else {
	*objPtrPtr = NULL;
    }
    return TCL_OK;
}

static int ListIntegerListObjIndexEnd (tclObjTypeInterfaceArgsListIndexEnd) {
    return TCL_ERROR;
}

static int ListIntegerListObjIsSorted(tclObjTypeInterfaceArgsListIsSorted) {
    return TCL_ERROR;
}

static int ListIntegerListObjLength(tclObjTypeInterfaceArgsListLength) {
    ListInteger *structPtr = listPtr->internalRep.twoPtrValue.ptr1;
    *intPtr = structPtr->used;
    return TCL_OK;
}

static Tcl_Obj* ListIntegerListObjRange(tclObjTypeInterfaceArgsListRange) {
    ListInteger *structPtr = listPtr->internalRep.twoPtrValue.ptr1;
    int i, j, num, used = structPtr->used;
    Tcl_Obj *numObjPtr, *resPtr;

    if ((fromIdx == 0 && toIdx >= used - 1) || used == 0) {
	return listPtr;
    }

    if (Tcl_IsShared(listPtr) ||
	((ListRepPtr(listPtr)->refCount > 1))) {
	if (fromIdx >= used || toIdx < fromIdx) {
	    return Tcl_NewObj();
	} else {
	    resPtr = NewTestListInteger();
	    for (i = fromIdx, j = 0; i <= toIdx; i++, j++) {
		num = structPtr->values[i];
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
}


static Tcl_Obj* ListIntegerListObjRangeEnd(tclObjTypeInterfaceArgsListRangeEnd) {
    return NULL;
}


static int ListIntegerListObjReplace(tclObjTypeInterfaceArgsListReplace) {
    int i, status;
    Tcl_Obj *tmpListPtr = Tcl_NewObj();
    Tcl_IncrRefCount(tmpListPtr);
    for (i = 0; i < objc; i++) {
	status = Tcl_ListObjAppendElement(interp, listPtr, objv[i]);
	if (Tcl_ListObjAppendElement(interp, tmpListPtr, objv[i]) != TCL_OK) {
	    Tcl_DecrRefCount(tmpListPtr);
	    return status;
	}
    }
    status = listIntegerListObjReplaceList(
	interp, listPtr, first, count, tmpListPtr);
    Tcl_DecrRefCount(tmpListPtr);
    return status;
}


static int listIntegerListObjReplaceList(tclObjTypeInterfaceArgsListReplaceList) {
    ListInteger *structPtr = listPtr->internalRep.twoPtrValue.ptr1;
    ListInteger *newStructPtr;
    int i, itemInt, newmemsize, itemsLength, j, newsize, newtailindex, newused,
	    size, status, status2, tailindex, tailsize, used;
    size_t structsize;
    Tcl_Obj *itemPtr;
    size = structPtr->size;
    used = structPtr->used;
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

    /* to do:
     * Recode this routine to work with incoming of unbounded length
     */

    if (used > 0) {
	tailindex = first + count;
	if (INT_MAX - itemsLength + 1 < first) {
	    return ErrorMaxElementsExceeded(interp);
	}
	newtailindex = first + itemsLength;
	if (INT_MAX - tailsize - 1 < newtailindex) {
	    return ErrorMaxElementsExceeded(interp);
	}
	newused = newtailindex + tailsize;
	if (INT_MAX - itemsLength < newused) {
	    return ErrorMaxElementsExceeded(interp);
	}
    } else {
	tailindex = 0;
	newtailindex = 0;
	newused = itemsLength;
    }

    if (newused > size && newused > 1) {
	newsize = (newused + newused / 5 + 1);
	newmemsize = sizeof(ListInteger) + newsize * sizeof(int) - sizeof(int);
    } else {
	newsize = size;
    }
    if (structPtr->refCount > 1) {
	/* copy only the structure and the head of the old array */
	int movsize = sizeof(ListInteger)
	    + ((first + 1)  * sizeof(int) - sizeof(int));
	newStructPtr = Tcl_Alloc(newmemsize);
	structPtr->refCount--;
	memmove(newStructPtr, structPtr, movsize);
	newStructPtr->size = newsize;
	newStructPtr->refCount = 1;
	/* move the tail to its new location to make room for the new additions */
	memmove(newStructPtr->values + newtailindex, structPtr->values + tailindex, tailsize);
	Tcl_StoreInternalRep(listPtr, testListIntegerTypePtr, newStructPtr);
    } else {
	newStructPtr = structPtr;
	if (newsize > size && newused > 1) {
	    newStructPtr = Tcl_Realloc(newStructPtr, newmemsize);
	}
	newStructPtr->size = newsize;
	if (tailsize > 0 && tailindex != newtailindex) {
	    /* move the tail to its new location to make room for the new additions */
	    memmove(newStructPtr->values + newtailindex, structPtr->values + tailindex, tailsize);
	}
	listPtr->internalRep.twoPtrValue.ptr1 = newStructPtr;
    }

    i = -1;
    while (1) {
	i++;
	status = Tcl_ListObjIndex(interp, newItemsPtr, i, &itemPtr);
	if (status != TCL_OK) {
	    return status;
	}
	if (itemPtr == NULL) {
	    break;
	}
	if (Tcl_GetIntFromObj(interp, itemPtr, &itemInt)
	    == TCL_OK) {
	    newStructPtr->values[first + i] = itemInt;
	} else {
	    Tcl_Obj *realListPtr;
	    /* Fall back to normal list */
	    realListPtr = Tcl_NewListObj(newsize, NULL);
	    Tcl_IncrRefCount(realListPtr);
	    for (j = 0; j < i; j++) {
		itemPtr = Tcl_NewIntObj(structPtr->values[j]);
		status = Tcl_ListObjAppendElement(
		    interp, realListPtr, itemPtr);
		if (status != TCL_OK) {
		    Tcl_DecrRefCount(realListPtr);
		    return status;
		}
	    }
	    i--;
	    while (1) {
		i++;
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
	    }

	    for (i = tailindex; i < tailsize; i++) {
		itemPtr = Tcl_NewIntObj(structPtr->values[i + tailindex]);
		Tcl_ListObjAppendElement(interp, realListPtr, itemPtr);
		if (status != TCL_OK) {
		    Tcl_DecrRefCount(realListPtr);
		    return status;
		}
	    }
	    ListIntegerDecrRefCount(newStructPtr);
	    listPtr->internalRep = realListPtr->internalRep;
	    listPtr->typePtr = realListPtr->typePtr;
	    realListPtr->typePtr = NULL;
	    Tcl_DecrRefCount(realListPtr);
	    return TCL_OK;
	}
    }

    /* To make the operation transactional, update "used" only after all
     * elemnts have been succesfully added.
    */
    newStructPtr->used = newused;
    return TCL_OK;
}


static int ListIntegerListObjSetElement (tclObjTypeInterfaceArgsListSet) {
    return TCL_ERROR;
}


static Tcl_Obj * ListIntegerLsetFlat (tclObjTypeInterfaceArgsListSetList) {
    return NULL;
}


static int ErrorMaxElementsExceeded(Tcl_Interp *interp) {
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "max length of a Tcl list (%d elements) exceeded",
	    LIST_MAX));
    }
    return TCL_ERROR;
}
