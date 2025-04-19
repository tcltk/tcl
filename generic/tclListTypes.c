/*
 * tclListTypes.c --
 *
 *	This file contains functions that implement the Tcl abstract list
 *	object types.
 *
 * Copyright © 2025 Ashok P. Nadkarni.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <assert.h>
#include "tclInt.h"

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
    int localFlags[LOCAL_SIZE], *flagPtr = NULL;
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
        flagPtr = (int *)Tcl_Alloc(numElems);
    }
    for (i = 0; i < numElems; i++) {
        Tcl_Obj *elemObj;
        flagPtr[i] = (i ? TCL_DONT_QUOTE_HASH : 0);
        ret = Tcl_ListObjIndex(NULL, objPtr, i, &elemObj);
        assert(ret == TCL_OK);
        elem       = Tcl_GetStringFromObj(elemObj, &length);
        bytesNeeded += Tcl_ScanCountedElement(elem, length, flagPtr + i);
        if (bytesNeeded > SIZE_MAX - numElems) {
            Tcl_Panic("max size for a Tcl value (%" TCL_Z_MODIFIER
                      "u bytes) exceeded",
                      SIZE_MAX);
        }
#if TCL_MAJOR_VERSION > 8
        Tcl_BounceRefCount(elemObj);
#endif
    }
    bytesNeeded += numElems - 1;

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
        dst += Tcl_ConvertCountedElement(elem, length, dst, flagPtr[i]);
        *dst++ = ' ';
    }
    dst[-1]         = '\0'; // Overwrite last space
    size_t finalLen = dst - start;

    /* If we are wasting "too many" bytes, attempt a reallocation */
    if (bytesNeeded > 1000 && (bytesNeeded-finalLen) > (bytesNeeded/4)) {
        char *newBytes = (char *)Tcl_Realloc(start, finalLen);
        if (newBytes != NULL) {
            start = newBytes;
        }
    }
    objPtr->bytes = start;
    objPtr->length = finalLen-1; // Exclude the trailing null

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

    Tcl_Obj *resultPtr = Tcl_NewObj();
    Tcl_InvalidateStringRep(resultPtr);

    Tcl_IncrRefCount(objPtr);
    resultPtr->internalRep.ptrAndSize.ptr = objPtr;
    resultPtr->internalRep.ptrAndSize.size = elemc;
    resultPtr->typePtr = &lreverseType;
    *reversedPtrPtr = resultPtr;
    return TCL_OK;
}
