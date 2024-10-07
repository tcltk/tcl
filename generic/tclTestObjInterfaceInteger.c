/*
 * Copyright © 2021 Nathan Coulter
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * Copyright © 2024 Nathan Coulter
 *
 * You may distribute and/or modify this program under the terms of the GNU
 * Affero General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.

 * See the file "COPYING" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
*/

/*
 * tclTestObjInterfce.c --
 *
 *	This file contains C command functions for the additional Tcl commands
 *	that are used for testing implementations of the Tcl object types.
 *	These commands are not normally included in Tcl applications; they're
 *	only used for testing.
 */

#include <math.h>

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

int TestListIntegerGetElements(TCL_UNUSED(void *), Tcl_Interp *interp,
    Tcl_Size argc, Tcl_Obj *const objv[]);

static Tcl_ObjInterfaceStringIndexProc ListIntegerListStringIndex;
static Tcl_ObjInterfaceStringIndexEndProc ListIntegerListStringIndexEnd;
static Tcl_ObjInterfaceStringLengthProc ListIntegerListStringLength;
/*
static int ListIntegerStringListIndexFromStringIndex(
    Tcl_Size *index, Tcl_Size *itemchars, Tcl_Size *totalitems);
*/
static Tcl_ObjInterfaceStringRangeProc ListIntegerListStringRange;
static Tcl_ObjInterfaceStringRangeEndProc ListIntegerListStringRangeEnd;

static Tcl_ObjInterfaceListAppendProc ListIntegerListObjAppendElement;
static Tcl_ObjInterfaceListAppendlistProc ListIntegerListObjAppendList;
static Tcl_ObjInterfaceListIndexProc ListIntegerListObjIndex;
static Tcl_ObjInterfaceListIndexEndProc ListIntegerListObjIndexEnd;
static Tcl_ObjInterfaceListIsSortedProc ListIntegerListObjIsSorted;
static Tcl_ObjInterfaceListLengthProc ListIntegerListObjLength;
static Tcl_ObjInterfaceListRangeProc ListIntegerListObjRange;
static Tcl_ObjInterfaceListRangeEndProc ListIntegerListObjRangeEnd;
static Tcl_ObjInterfaceListReplaceProc ListIntegerListObjReplace;
static Tcl_ObjInterfaceListReplaceListProc ListIntegerListObjReplaceList;
static Tcl_ObjInterfaceListSetProc ListIntegerLset;
static Tcl_ObjInterfaceListSetDeepProc ListIntegerListObjSetDeep;

static int ErrorMaxElementsExceeded(Tcl_Interp *interp);


typedef struct ListInteger {
    int refCount;
    int ownstring;
    Tcl_Size size;
    Tcl_Size used;
    int values[1];
} ListInteger;


static ListInteger* NewTestListIntegerIntrep();
static ListInteger* ListGetInternalRep(Tcl_Obj *listPtr);
static void ListIntegerDecrRefCount(ListInteger *listIntegerPtr);

static ObjectType testListIntegerType = {
    "testListInteger",
    FreeTestListIntegerInternalRep,	/* freeIntRepProc */
    DupTestListIntegerInternalRep,		/* dupIntRepProc */
    UpdateStringOfTestListInteger,		/* updateStringProc */
    SetTestListIntegerFromAny,		/* setFromAnyProc */
    2,
    NULL
};

Tcl_ObjType *testListIntegerTypePtr = (Tcl_ObjType *)&testListIntegerType;



int TcltestObjectInterfaceListIntegerInit(Tcl_Interp *interp) {
    Tcl_ObjInterface *oiPtr;
    oiPtr = Tcl_NewObjInterface();
    Tcl_ObjInterfaceSetFnStringIndex(oiPtr ,ListIntegerListStringIndex);
    Tcl_ObjInterfaceSetFnStringIndexEnd(oiPtr ,ListIntegerListStringIndexEnd);
    Tcl_ObjInterfaceSetFnStringLength(oiPtr ,ListIntegerListStringLength);
    Tcl_ObjInterfaceSetFnStringRange(oiPtr ,ListIntegerListStringRange);
    Tcl_ObjInterfaceSetFnStringRangeEnd(oiPtr ,ListIntegerListStringRangeEnd);
    Tcl_ObjInterfaceSetFnListAppend(oiPtr ,ListIntegerListObjAppendElement);
    Tcl_ObjInterfaceSetFnListAppendList(oiPtr ,ListIntegerListObjAppendList);
    Tcl_ObjInterfaceSetFnListIndex(oiPtr ,ListIntegerListObjIndex);
    Tcl_ObjInterfaceSetFnListIndexEnd(oiPtr ,ListIntegerListObjIndexEnd);
    Tcl_ObjInterfaceSetFnListIsSorted(oiPtr ,ListIntegerListObjIsSorted);
    Tcl_ObjInterfaceSetFnListLength(oiPtr ,ListIntegerListObjLength);
    Tcl_ObjInterfaceSetFnListRange(oiPtr ,ListIntegerListObjRange);
    Tcl_ObjInterfaceSetFnListRangeEnd(oiPtr ,ListIntegerListObjRangeEnd);
    Tcl_ObjInterfaceSetFnListReplace(oiPtr ,ListIntegerListObjReplace);
    Tcl_ObjInterfaceSetFnListReplaceList(oiPtr ,ListIntegerListObjReplaceList);
    Tcl_ObjInterfaceSetFnListSet(oiPtr , ListIntegerLset);
    Tcl_ObjInterfaceSetFnListSetDeep(oiPtr ,ListIntegerListObjSetDeep);
    Tcl_ObjTypeSetInterface(testListIntegerTypePtr,oiPtr);


    Tcl_CreateObjCommand2(interp, "testlistinteger", TestListInteger, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "testlistintegergetelements", TestListIntegerGetElements, NULL, NULL);
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

int TestListIntegerGetElements(
    TCL_UNUSED(void *),
    TCL_UNUSED(Tcl_Interp *),
    TCL_UNUSED(Tcl_Size),
    TCL_UNUSED(Tcl_Obj * const *))
{
    return 0;
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
	    status = TCL_OK;
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

static int ListIntegerListStringLength(
    TCL_UNUSED(Tcl_Obj *)
    ,Tcl_Size *lengthPtr
) {
    *lengthPtr = -1;
    return TCL_OK;
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

static int ListIntegerListStringRange(
    Tcl_Interp * interp,
    TCL_UNUSED(Tcl_Obj *),	/* The Tcl object to find the range of. */ \
    Tcl_Size fromIdx,	/* First index of the range. */ \
    Tcl_Size toIdx,	/* Last index of the range. */ \
    Tcl_Obj **resPtrPtr	/* The resulting Tcl_Obj* is stored here. */
) {
    char *valPtr;
    Tcl_Obj *resPtr, *numPtr ,*rangePtr;
    Tcl_Size cursor = 0 ,digits = 1 ,incr = 1 ,length ,more ,movlen
	,needed ,nextchars = 0 ,chars = 0 ,top = incr * 10;
    

    while (chars < fromIdx) {
	more = digits * incr;
	/* spaces */
	nextchars = chars + more;
	nextchars += incr * 1;
	if (nextchars > fromIdx) {
	    incr /= 10;
	    if (incr == 0) {
		break;
	    }
	} else {
	    cursor += incr;
	    chars = nextchars;
	    if (cursor >= top) {
		incr *= 10;
		top = incr * 10;
		digits +=1;
	    }
	}
    }

    resPtr = Tcl_ObjPrintf("%lu cursor" ,cursor);
    Tcl_AppendPrintfToObj(resPtr ,"\n%lu fromIdx" ,fromIdx);
    Tcl_AppendPrintfToObj(resPtr ,"\n%lu chars" ,chars);
    Tcl_AppendPrintfToObj(resPtr ,"\n%lu digits" ,digits);
    Tcl_AppendPrintfToObj(resPtr ,"\n%lu incr" ,incr);
    Tcl_AppendPrintfToObj(resPtr ,"\n%lu more\n" ,digits * incr);
    rangePtr = Tcl_NewObj();

    numPtr = Tcl_ObjPrintf("%lu" ,cursor);
    valPtr = Tcl_GetStringFromObj(numPtr ,&length); 
    needed = toIdx - fromIdx + 1;
    cursor +=1;
    while (chars < fromIdx) {
	if (length == 0) {
	    if (interp) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "failed to find beginning list index for string range"));
	    }
	    return TCL_ERROR;
	}
	valPtr++;
	chars++;
	length--;
    }
    if (length > 0) {
	movlen = needed > length ? length : needed;
	Tcl_AppendToObj(rangePtr ,valPtr ,movlen);
	needed -= movlen;
    }
    Tcl_DecrRefCount(numPtr);

    while (needed > 0) {
	Tcl_AppendToObj(rangePtr ," ",1);
	needed--;
	if (needed > 0) {
	    numPtr = Tcl_ObjPrintf("%lu" ,cursor);
	    valPtr = Tcl_GetStringFromObj(numPtr ,&length); 
	    movlen = length > needed ? needed : length;
	    Tcl_AppendToObj(rangePtr ,valPtr ,movlen);
	    needed -= movlen;
	    Tcl_DecrRefCount(numPtr);
	}
	cursor +=1;
    }

    /*
    Tcl_AppendObjToObj(resPtr ,rangePtr);
    Tcl_DecrRefCount(resPtr);
    */
    *resPtrPtr = rangePtr;
    return TCL_OK;
}

static int ListIntegerListStringRangeEnd(
    Tcl_Interp * interp,
    TCL_UNUSED(Tcl_Obj *),	/* The Tcl object to find the range of. */ \
    TCL_UNUSED(Tcl_Size),	/* First index of the range. */ \
    TCL_UNUSED(Tcl_Size),	/* Last index of the range. */ \
    Tcl_Obj **resPtrPtr	/* The resulting Tcl_Obj* is stored here. */)
{
    /* suppress unused parameter warning */
    (void) interp;
    *resPtrPtr = NULL;
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
    Tcl_Obj **resPtrPtr	/* The resulting Tcl_Obj* is stored here. */
) {
    ListInteger *listRepPtr = ListGetInternalRep(listObj);
    Tcl_Size num;
    if (index >= 0 && index < listRepPtr->used) {
	num = listRepPtr->values[index];
	*resPtrPtr = Tcl_NewLongObj(num);
    } else {
	*resPtrPtr = NULL;
    }
    return TCL_OK;
}

static int ListIntegerListObjIndexEnd(
    TCL_UNUSED(Tcl_Interp *),/* Used to report errors if not NULL. */ \
    Tcl_Obj * listObj,/* List object to index into. */ \
    Tcl_Size index,/* Index of element to return. */ \
    Tcl_Obj **resPtrPtr /* The resulting Tcl_Obj* is stored here. */
) {
    int num;
    ListInteger *listRepPtr = ListGetInternalRep(listObj);
    if (index > TCL_SIZE_MAX) {
	*resPtrPtr = NULL;
	return TCL_ERROR;
    }
    Tcl_Size idx = listRepPtr->used - 1 - index;
    if (idx > listRepPtr->used) {
	*resPtrPtr = NULL;
	return TCL_ERROR;
    }
    if (idx < 0) {
	*resPtrPtr = NULL;
	return TCL_ERROR;
    }
    num = listRepPtr->values[idx];
    *resPtrPtr = Tcl_NewLongObj(num);
    return TCL_OK;
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

static int ListIntegerListObjRange(tclObjTypeInterfaceArgsListRange) {
    ListInteger *listRepPtr = ListGetInternalRep(listPtr);
    Tcl_Size i, j, num, used = listRepPtr->used;
    Tcl_Obj *numObjPtr, *resPtr;

    if ((fromIdx == 0 && toIdx >= used - 1) || used == 0) {
	*resPtrPtr = listPtr;
	return TCL_OK;
    }

    if (Tcl_IsShared(listPtr) ||
	((listRepPtr->refCount > 1))) {
	if (fromIdx >= used || toIdx < fromIdx) {
	    *resPtrPtr = Tcl_NewObj();
	    return TCL_OK;
	} else {
	    resPtr = NewTestListInteger();
	    for (i = fromIdx, j = 0; i <= toIdx; i++, j++) {
		num = listRepPtr->values[i];
		numObjPtr = Tcl_NewIntObj(num);
		Tcl_IncrRefCount(numObjPtr);
		if (ListIntegerListObjReplace(
		    interp, resPtr, j , 0 , 1 ,&numObjPtr) != TCL_OK) {
		    Tcl_DecrRefCount(resPtr);
		    Tcl_DecrRefCount(numObjPtr);
		    *resPtrPtr = NULL;
		    return TCL_OK;
		}
		Tcl_DecrRefCount(numObjPtr);
	    }
	    *resPtrPtr = resPtr;
	    return TCL_OK;
	}
    }
    *resPtrPtr = NULL;
    return TCL_OK;
}


static int ListIntegerListObjRangeEnd(
    TCL_UNUSEDVAR(Tcl_Interp * interp), /* Used to report errors */ \
    TCL_UNUSEDVAR(Tcl_Obj *listPtr),	/* List object to take a range from. */ \
    TCL_UNUSEDVAR(Tcl_Size fromAnchor),/* 0 for start and 1 for end */ \
    TCL_UNUSEDVAR(Tcl_Size fromIdx),	/* Index of first element to include. */ \
    TCL_UNUSEDVAR(Tcl_Size toAnchor),	/* 0 for start and 1 for end */  \
    TCL_UNUSEDVAR(Tcl_Size toIdx),	/* Index of last element to include. */
    Tcl_Obj **resPtrPtr
) {
    *resPtrPtr = NULL;
    return TCL_OK;
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

    /* Currently this duplicates checks found in Tcl_ListObjReplace, but
     * could be removed in that function in the future.
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


static int ListIntegerListObjSetDeep(
    TCL_UNUSED(Tcl_Interp *),	    /* Tcl interpreter. */ \
    TCL_UNUSED(Tcl_Obj *),	    /* Pointer to the list being modified. */ \
    TCL_UNUSED(Tcl_Size),    /* Number of index args. */ \
    TCL_UNUSED(Tcl_Obj *const *),    /* Index args. */ \
    TCL_UNUSED(Tcl_Obj *),/* Value arg to 'lset' or NULL to 'lpop'. */
    Tcl_Obj **resPtrPtr)
{
    *resPtrPtr = NULL;
    return TCL_ERROR;
}


static int ListIntegerLset(
    TCL_UNUSED(Tcl_Interp *),
    TCL_UNUSED(Tcl_Obj *),
    TCL_UNUSED(Tcl_Size),
    TCL_UNUSED(Tcl_Obj *))
{
    return TCL_ERROR;
}


static int ErrorMaxElementsExceeded(Tcl_Interp *interp) {
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "max length of a Tcl list (%" TCL_T_MODIFIER "d elements) exceeded",
	    LIST_MAX));
    }
    return TCL_ERROR;
}
