/*
 * tclListTypes.c --
 *
 *	This file contains functions that implement the Tcl abstract list
 *	object types.
 *
 * Copyright (c) 2025 Ashok P. Nadkarni.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <assert.h>
#include "tclInt.h"

/*
 * Since iterating is a little slower for abstract lists, we use a
 * threshold to decide when to use the abstract list type. This is
 * a tradeoff between memory usage and speed.
 */
#define LREVERSE_LENGTH_THRESHOLD 100
#define LREPEAT_LENGTH_THRESHOLD 100

static inline int
TclAbstractListLength(Tcl_Interp *interp, Tcl_Obj *objPtr, Tcl_Size *lengthPtr)
{
    int ret;
    if (TclObjTypeHasProc(objPtr, lengthProc)) {
	*lengthPtr = TclObjTypeLength(objPtr);
        ret = TCL_OK;
    } else {
	ret = TclListObjLength(interp, objPtr, lengthPtr);
    }
    return ret;
}

/*
 * TclObjArray stores a reference counted Tcl_Obj array.
 */
typedef struct TclObjArray {
    Tcl_Size refCount;   /* Reference count */
    Tcl_Size nelems;     /* Number of elements in the array */
    Tcl_Obj *elemPtrs[1]; /* Variable size array */
} TclObjArray;

/*
 * Allocate a new TclObjArray structure and initialize it with the
 * given Tcl_Obj elements, incrementing their reference counts.
 * The reference count of the array itself is initialized to 0.
 */
static inline TclObjArray *
TclObjArrayNew(size_t nelems, Tcl_Obj * const elemPtrs[])
{
    TclObjArray *arrayPtr = (TclObjArray *)Tcl_Alloc(
	sizeof(TclObjArray) + (nelems - 1) * sizeof(Tcl_Obj *));
    for (size_t i = 0; i < nelems; i++) {
        Tcl_IncrRefCount(elemPtrs[i]);
        arrayPtr->elemPtrs[i] = elemPtrs[i];
    }
    arrayPtr->refCount = 0;
    arrayPtr->nelems = nelems;
    return arrayPtr;
}

/* Add a reference to a TclObjArray */
static inline void
TclObjArrayRef(TclObjArray *arrayPtr)
{
    arrayPtr->refCount++;
}

/*
 * Remove a reference from an TclObjArray, freeing it if no more remain.
 * The reference count of the elements is decremented as well in that case.
 */
static inline void
TclObjArrayUnref(TclObjArray *arrayPtr)
{
    if (arrayPtr->refCount <= 1) {
	for (Tcl_Size i = 0; i < arrayPtr->nelems; i++) {
	    Tcl_DecrRefCount(arrayPtr->elemPtrs[i]);
	}
	Tcl_Free(arrayPtr);
    } else {
	arrayPtr->refCount--;
    }
}

/* Returns count of elements in array and pointer to them in objPtrPtr */
static inline Tcl_Size TclObjArrayElems(TclObjArray *arrayPtr, Tcl_Obj ***objPtrPtr)
{
    *objPtrPtr = arrayPtr->elemPtrs;
    return arrayPtr->nelems;
}

/*
 *------------------------------------------------------------------------
 *
 * TclAbstractListUpdateString --
 *
 *    Common function to update the string representation of an abstract list
 *    type. Adapted from UpdateStringOfList in tclListObj.c.
 *    Assumes no prior string representation exists.
*
 * Results:
 *    None.
 *
 * Side effects:
 *    The string representation of the object is updated.
 *
 *------------------------------------------------------------------------
 */
static void TclAbstractListUpdateString (Tcl_Obj *objPtr)
{
   #define LOCAL_SIZE 64
    char localFlags[LOCAL_SIZE], *flagPtr = NULL;
    Tcl_Size numElems, i, length;
    size_t bytesNeeded = 0;
    const char *elem;
    char *start, *dst;
    int ret;

    ret = Tcl_ListObjLength(NULL, objPtr, &numElems);
    assert(ret == TCL_OK); // Should only be called for lists
    (void) ret; // Avoid compiler warning

    /* Handle empty list case first, so rest of the routine is simpler. */

    if (numElems == 0) {
	objPtr->bytes = (char *)Tcl_Alloc(1);
	objPtr->bytes[0] = '\0';
	objPtr->length = 0;
	return;
    }

    /* Pass 1: estimate space, gather flags. */
    if (numElems <= LOCAL_SIZE) {
        flagPtr = localFlags;
    }
    else {
        flagPtr = (char *)Tcl_Alloc(numElems);
    }
    for (i = 0; i < numElems; i++) {
        Tcl_Obj *elemObj;
        flagPtr[i] = (i ? TCL_DONT_QUOTE_HASH : 0);
        ret = Tcl_ListObjIndex(NULL, objPtr, i, &elemObj);
        assert(ret == TCL_OK);
        elem       = Tcl_GetStringFromObj(elemObj, &length);
        bytesNeeded += TclScanElement(elem, length, flagPtr + i);
        if (bytesNeeded > SIZE_MAX - numElems) {
            Tcl_Panic("max size for a Tcl value (%" TCL_Z_MODIFIER
                      "u bytes) exceeded",
                      SIZE_MAX);
        }
#if TCL_MAJOR_VERSION > 8
        Tcl_BounceRefCount(elemObj);
#endif
    }
    bytesNeeded += numElems; /* Including trailing nul */

    /*
     * Pass 2: copy into string rep buffer.
     */

    start = dst = (char *) Tcl_Alloc(bytesNeeded);
    for (i = 0; i < numElems; i++) {
        Tcl_Obj *elemObj;
        flagPtr[i] |= (i ? TCL_DONT_QUOTE_HASH : 0);
        ret = Tcl_ListObjIndex(NULL, objPtr, i, &elemObj);
        assert(ret == TCL_OK);
        elem = Tcl_GetStringFromObj(elemObj, &length);
        dst += TclConvertElement(elem, length, dst, flagPtr[i]);
        *dst++ = ' ';
    }
    dst[-1]         = '\0'; /* Overwrite last space */
    size_t finalLen = dst - start; /* Includes trailing nul */

    /* If we are wasting "too many" bytes, attempt a reallocation */
    if (bytesNeeded > 1000 && (bytesNeeded-finalLen) > (bytesNeeded/4)) {
        char *newBytes = (char *)Tcl_Realloc(start, finalLen);
        if (newBytes != NULL) {
            start = newBytes;
        }
    }
    objPtr->bytes = start;
    objPtr->length = finalLen-1; /* Exclude the trailing null */

    if (flagPtr != localFlags) {
        Tcl_Free(flagPtr);
    }
}

/*
 * ------------------------------------------------------------------------
 * lreverseType is an abstract list type that contains the same elements as a
 * given list but in reverse order. Implementation is straightforward with the
 * target list stored in ptrAndSize.ptr field. Indexing is then just a question
 * of mapping index of the reversed list to that of the original target.
 * The ptrAndSize.size field is used as a length cache.
 * ------------------------------------------------------------------------
 */

static void LreverseFreeIntrep(Tcl_Obj *objPtr);
static void LreverseDupIntrep(Tcl_Obj *srcObj, Tcl_Obj *dupObj);
static Tcl_ObjTypeLengthProc LreverseTypeLength;
static Tcl_ObjTypeIndexProc LreverseTypeIndex;
static Tcl_ObjTypeReverseProc LreverseTypeReverse;

/*
 * IMPORTANT - current implementation is read-only except for reverseProc.
 * That is, the functions below that set or modify elements must be NULL. If
 * you change this, be aware that both the object and internal
 * representation (targetObj) may be shared and must be checked before
 * modification.
 */
static const Tcl_ObjType lreverseType = {
    "lreverse",                         /* name */
    LreverseFreeIntrep,                 /* freeIntRepProc */
    LreverseDupIntrep,                  /* dupIntRepProc */
    TclAbstractListUpdateString,        /* updateStringProc */
    NULL,                               /* setFromAnyProc */
    TCL_OBJTYPE_V2(LreverseTypeLength,  /* lengthProc */
		   LreverseTypeIndex,   /* indexProc */
		   NULL,                /* sliceProc */
		   LreverseTypeReverse, /* reverseProc */
		   NULL,                /* getElementsProc */
		   NULL,                /* setElementProc - TODO */
		   NULL,                /* replaceProc - TODO */
		   NULL)                /* inOperProc - TODO */
};

void
LreverseFreeIntrep(Tcl_Obj *objPtr)
{
    Tcl_DecrRefCount((Tcl_Obj *)objPtr->internalRep.ptrAndSize.ptr);
}

void
LreverseDupIntrep(Tcl_Obj *srcObj, Tcl_Obj *dupObj)
{
    Tcl_Obj *targetObj = (Tcl_Obj *)srcObj->internalRep.ptrAndSize.ptr;
    Tcl_IncrRefCount(targetObj);
    dupObj->internalRep.ptrAndSize.ptr = targetObj;
    dupObj->internalRep.ptrAndSize.size = srcObj->internalRep.ptrAndSize.size;
    dupObj->typePtr = srcObj->typePtr;
}

/* Implementation of Tcl_ObjType.lengthProc for lreverseType */
Tcl_Size
LreverseTypeLength(Tcl_Obj *objPtr)
{
    return objPtr->internalRep.ptrAndSize.size;
}

/* Implementation of Tcl_ObjType.indexProc for lreverseType */
int
LreverseTypeIndex(Tcl_Interp *interp,
    Tcl_Obj *objPtr, /* Source list */
    Tcl_Size index,  /* Element index */
    Tcl_Obj **elemPtrPtr) /* Returned element */
{
    Tcl_Obj *targetPtr = (Tcl_Obj *)objPtr->internalRep.ptrAndSize.ptr;
    Tcl_Size len = objPtr->internalRep.ptrAndSize.size;
    if (index < 0 || index >= len) {
        *elemPtrPtr = NULL;
        return TCL_OK;
    }
    index = len - index - 1; /* Reverse the index */
    return Tcl_ListObjIndex(interp, targetPtr, index, elemPtrPtr);
}

/* Implementation of Tcl_ObjType.reverseProc for lreverseType */
int
LreverseTypeReverse(Tcl_Interp *interp,
    Tcl_Obj *objPtr,          /* Operand */
    Tcl_Obj **reversedPtrPtr) /* Result */
{
    (void)interp; /* Unused */
    /* Simple return the original */
    *reversedPtrPtr = (Tcl_Obj *) objPtr->internalRep.ptrAndSize.ptr;
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * Tcl_ListObjReverse --
 *
 *    Returns a Tcl_Obj containing a list with the same elements as the
 *    source list with elements in reverse order.
 *
 * Results:
 *    Standard Tcl result.
 *
 * Side effects:
 *    Stores the result in *reversedPtrPtr. This may be the same as objPtr,
 *    a new allocation, or a pointer to an internally stored object. In
 *    all cases, the reference count of the returned object is not
 *    incremented to account for the returned reference to it.
 *
 *------------------------------------------------------------------------
 */
int
Tcl_ListObjReverse(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,          /* Source whose elements are to be reversed */
    Tcl_Obj **reversedPtrPtr) /* Location to store result object */
{
    /* If the list is an AbstractList with a specialized reverse, use it. */
    if (TclObjTypeHasProc(objPtr, reverseProc)) {
	if (TclObjTypeReverse(interp, objPtr, reversedPtrPtr) == TCL_OK) {
	    return TCL_OK;
	}
	/* Specialization does not work for this case. Try default path */
    }

    Tcl_Size elemc;

    /* Verify target is a list or can be converted to one */
    if (TclObjTypeHasProc(objPtr, lengthProc)) {
	elemc = TclObjTypeLength(objPtr);
    } else {
	if (TclListObjLength(interp, objPtr, &elemc) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    /* If the list is empty, just return it. [Bug 1876793] */
    if (elemc == 0) {
	*reversedPtrPtr = objPtr;
	return TCL_OK;
    }

    Tcl_Obj *resultPtr;
    if (elemc >= LREVERSE_LENGTH_THRESHOLD || objPtr->typePtr != &tclListType) {
	resultPtr = Tcl_NewObj();
	TclInvalidateStringRep(resultPtr);

	Tcl_IncrRefCount(objPtr);
	resultPtr->internalRep.ptrAndSize.ptr = objPtr;
	resultPtr->internalRep.ptrAndSize.size = elemc;
	resultPtr->typePtr = &lreverseType;
	*reversedPtrPtr = resultPtr;
	return TCL_OK;
    }

    /* Non-abstract list small enough to copy. */

    Tcl_Obj **elemv;

    if (TclListObjGetElements(interp, objPtr, &elemc, &elemv) != TCL_OK) {
	return TCL_ERROR;
    }
    resultPtr = Tcl_NewListObj(elemc, NULL);
    Tcl_Obj **dataArray = NULL;
    ListRep listRep;
    ListObjGetRep(resultPtr, &listRep);
    dataArray = ListRepElementsBase(&listRep);
    CLANG_ASSERT(dataArray);
    listRep.storePtr->numUsed = elemc;
    if (listRep.spanPtr) {
	/* Future proofing in case Tcl_NewListObj returns a span */
	listRep.spanPtr->spanStart = listRep.storePtr->firstUsed;
	listRep.spanPtr->spanLength = listRep.storePtr->numUsed;
    }
    for (Tcl_Size i = 0; i < elemc; i++) {
        Tcl_IncrRefCount(elemv[i]);
        dataArray[elemc - i - 1] = elemv[i];
    }

    *reversedPtrPtr = resultPtr;
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 * lrepeatType is an abstract list type that repeated elements.
 * Implementation is straightforward with the elements stored in
 * list stored in ptrAndSize.ptr and number of repetitions in
 * ptrAndSize.size fields. Indexing is then just a question
 * of mapping index of modulo length of list of repeated elements.
 * ------------------------------------------------------------------------
 */

static void LrepeatFreeIntrep(Tcl_Obj *objPtr);
static void LrepeatDupIntrep(Tcl_Obj *srcObj, Tcl_Obj *dupObj);
static Tcl_ObjTypeLengthProc LrepeatTypeLength;
static Tcl_ObjTypeIndexProc LrepeatTypeIndex;

/*
 * IMPORTANT - current implementation is read-only. That is, the
 * functions below that set or modify elements are not NULL. If you change
 * this, be aware that both the object and internal representation
 * may be shared must be checked before modification.
 */
static const Tcl_ObjType lrepeatType = {
    "lrepeat",                        /* name */
    LrepeatFreeIntrep,                /* freeIntRepProc */
    LrepeatDupIntrep,                 /* dupIntRepProc */
    TclAbstractListUpdateString,      /* updateStringProc */
    NULL,                             /* setFromAnyProc */
    TCL_OBJTYPE_V2(LrepeatTypeLength, /* lengthProc */
		   LrepeatTypeIndex,  /* indexProc */
		   NULL,              /* sliceProc */
		   NULL,              /* Must be NULL - see above comment */
		   NULL,              /* getElementsProc */
		   NULL,              /* Must be NULL - see above comment */
		   NULL,              /* Must be NULL - see above comment */
		   NULL)              /* inOperProc - TODO */
};

void
LrepeatFreeIntrep(Tcl_Obj *objPtr)
{
    TclObjArrayUnref((TclObjArray *)objPtr->internalRep.ptrAndSize.ptr);
}

void
LrepeatDupIntrep(Tcl_Obj *srcObj, Tcl_Obj *dupObj)
{
    TclObjArray *arrayPtr = (TclObjArray *)srcObj->internalRep.ptrAndSize.ptr;
    TclObjArrayRef(arrayPtr);
    dupObj->internalRep.ptrAndSize.ptr = arrayPtr;
    dupObj->internalRep.ptrAndSize.size = srcObj->internalRep.ptrAndSize.size;
    dupObj->typePtr = srcObj->typePtr;
}

/* Implementation of Tcl_ObjType.lengthProc for lrepeatType */
Tcl_Size
LrepeatTypeLength(Tcl_Obj *objPtr)
{
    return objPtr->internalRep.ptrAndSize.size;
}

/* Implementation of Tcl_ObjType.indexProc for lrepeatType */
int
LrepeatTypeIndex(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr, /* Source list */
    Tcl_Size index,  /* Element index */
    Tcl_Obj **elemPtrPtr) /* Returned element */
{
    (void) interp; /* Unused */
    Tcl_Size len = objPtr->internalRep.ptrAndSize.size;
    if (index < 0 || index >= len) {
        *elemPtrPtr = NULL;
        return TCL_OK;
    }
    TclObjArray *arrayPtr = (TclObjArray *)objPtr->internalRep.ptrAndSize.ptr;
    Tcl_Obj **elems;
    Tcl_Size arraySize = TclObjArrayElems(arrayPtr, &elems);
    index = index % arraySize; /* Modulo the size of the array */
    *elemPtrPtr = arrayPtr->elemPtrs[index];
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * Tcl_ListObjRepeat --
 *
 *    Returns a Tcl_Obj containing a list whose elements are the same as the
 *    passed items repeated a given number of times.
 *
 * Results:
 *    Standard Tcl result.
 *
 * Side effects:
 *    Stores the result in *reversedPtrPtr. This may be the same as objPtr,
 *    a new allocation, or a pointer to an internally stored object. In
 *    all cases, the reference count of the returned object is not
 *    incremented to account for the returned reference to it.
 *
 *------------------------------------------------------------------------
 */
int
Tcl_ListObjRepeat(
    Tcl_Interp *interp,
    Tcl_Size repeatCount,   /* Number of times to repeat */
    Tcl_Size objc,          /* Number of elements in objv */
    Tcl_Obj *const objv[],  /* Source whose elements are to be repeated */
    Tcl_Obj **resultPtrPtr) /* Location to store result object */
{
    if (repeatCount < 0) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"bad count \"%" TCL_SIZE_MODIFIER "d\": must be integer >= 0", repeatCount));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "LREPEAT", "NEGARG",
		(char *)NULL);
	return TCL_ERROR;
    }

    Tcl_Size totalElems = objc * repeatCount;
    if (totalElems == 0) {
	*resultPtrPtr = Tcl_NewObj();
	return TCL_OK;
    }

    /* Final sanity check. Do not exceed limits on max list length. */
    if (objc > LIST_MAX/repeatCount) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"max length of a Tcl list (%" TCL_SIZE_MODIFIER "d elements) exceeded", LIST_MAX));
	Tcl_SetErrorCode(interp, "TCL", "MEMORY", (char *)NULL);
	return TCL_ERROR;
    }

    Tcl_Obj *resultPtr;
    if (totalElems >= LREPEAT_LENGTH_THRESHOLD) {
	TclObjArray *arrayPtr = TclObjArrayNew(objc, objv);
	resultPtr = Tcl_NewObj();
	arrayPtr->refCount++;
	TclInvalidateStringRep(resultPtr);
	resultPtr->internalRep.ptrAndSize.ptr = arrayPtr;
	resultPtr->internalRep.ptrAndSize.size = totalElems;
	resultPtr->typePtr = &lrepeatType;
	*resultPtrPtr = resultPtr;
	return TCL_OK;
    }

    /* For small lists, create a copy as indexing is slightly faster */
    resultPtr = Tcl_NewListObj(totalElems, NULL);
    Tcl_Obj **dataArray = NULL;
    if (totalElems) {
	ListRep listRep;
	ListObjGetRep(resultPtr, &listRep);
	dataArray = ListRepElementsBase(&listRep);
	listRep.storePtr->numUsed = totalElems;
	if (listRep.spanPtr) {
	    /* Future proofing in case Tcl_NewListObj returns a span */
	    listRep.spanPtr->spanStart = listRep.storePtr->firstUsed;
	    listRep.spanPtr->spanLength = listRep.storePtr->numUsed;
	}
    }

    /*
     * Set the elements. Note that we handle the common degenerate case of a
     * single value being repeated separately to permit the compiler as much
     * room as possible to optimize a loop that might be run a very large
     * number of times.
     */

    CLANG_ASSERT(dataArray || totalElems == 0 );
    if (objc == 1) {
	Tcl_Obj *tmpPtr = objv[0];

	tmpPtr->refCount += repeatCount;
	for (Tcl_Size i=0 ; i<totalElems ; i++) {
	    dataArray[i] = tmpPtr;
	}
    } else {
	for (Tcl_Size i = 0, k = 0; i < repeatCount; i++) {
	    for (Tcl_Size j=0 ; j<objc ; j++) {
		Tcl_IncrRefCount(objv[j]);
		dataArray[k++] = objv[j];
	    }
	}
    }
    *resultPtrPtr = resultPtr;
    return TCL_OK;
}
