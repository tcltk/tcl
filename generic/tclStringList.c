/*
 * tclStringList.c --
 *
 *     This file contains the StringList concrete abstract list
 *     implementation. It implements a view of a string as a list of
 *     of characters.
 *
 * Copyright © 2023 Ashok P. Nadkarni
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tcl.h"
#include "tclInt.h"
#include <assert.h>
#include <math.h>

/*
 * The structure below defines the StringList Tcl_ObjType. Implementation
 * stores the target Tcl_Obj being split in the internal representation of the
 * abstract list Tcl_Obj. List operations are then mapped to the corresponding
 * string operations. This makes per character processing of the form
 *    foreach ch [split $str ""] {...}
 * less expensive for large strings.
 *
 * The source string is stored in the internalRep.twpPtrValue.ptr1 field.
 * The ptr2 field holds a StringListInfo struct with ancillary information
 * as below. If ptr2 is NULL, list covers all elements of source string.
 */
typedef struct StringListInfo {
    Tcl_Size startIndex; /* Starting index in source string where this list
                            begins. Used for fast range operations */
    Tcl_Size count;      /* Number of elements in list. If negative, count
                            is entire source string */
    int flags;
#define STRINGLIST_REVERSE 0x01 /* If set, reverse element order */
} StringListInfo;

/* -------------------------- StringList object ---------------------------- */

static int StringListObjIndex(TCL_UNUSED(Tcl_Interp *), Tcl_Obj *arithSeriesObj,
				  Tcl_Size index, Tcl_Obj **elemObj);

static Tcl_Size StringListObjLength(Tcl_Obj *strListObj);
static int StringListObjRange(Tcl_Interp *interp, Tcl_Obj *strListObj,
			    Tcl_Size fromIdx, Tcl_Size toIdx, Tcl_Obj **newObjPtr);
static int StringListObjReverse(Tcl_Interp *interp, Tcl_Obj *strListObj, Tcl_Obj **newObjPtr);
static void DupStringListInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void FreeStringListInternalRep(Tcl_Obj *strListObj);
static void UpdateStringOfStringList(Tcl_Obj *strListObj);

static const Tcl_ObjType stringListType = {
    "stringlist",               /* name */
    FreeStringListInternalRep, /* freeIntRepProc */
    DupStringListInternalRep,  /* dupIntRepProc */
    UpdateStringOfStringList,  /* updateStringProc */
    NULL,                      /* setFromAnyProc */
    TCL_OBJTYPE_V2(
    StringListObjLength,
    StringListObjIndex,
    StringListObjRange,
    StringListObjReverse,
    NULL, /* GetElements */
    NULL, /* SetElement */
    NULL) /* Replace */
};

/* Returns the source for a StringList, NULL if objPtr is not a StringList */
static inline Tcl_Obj *
StringListGetSourceObj(Tcl_Obj *objPtr)
{
    const Tcl_ObjInternalRep *irPtr;
    irPtr = TclFetchInternalRep((objPtr), &stringListType);
    return irPtr ? (Tcl_Obj *)irPtr->twoPtrValue.ptr1 : NULL;
}

/*
 * Returns the ancillary information about a StringList. May return an
 * NULL either because objPtr is not a StringList or because it has no
 * ancillary information. Use StringListGetSourceObj to distinguish.
 */
static inline StringListInfo *
StringListGetInfo(Tcl_Obj *objPtr)
{
    const Tcl_ObjInternalRep *irPtr;
    irPtr = TclFetchInternalRep((objPtr), &stringListType);
    return irPtr ? (StringListInfo *)irPtr->twoPtrValue.ptr2 : NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * DupStringListInternalRep --
 *
 *	Initialize the internal representation of a StringList Tcl_Obj to a
 *	copy of the internal representation of an existing one.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	We set "copyPtr"s internal rep to a newly allocated structure.
 *----------------------------------------------------------------------
 */

static void
DupStringListInternalRep(
    Tcl_Obj *srcPtr,		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr)		/* Object with internal rep to set. */
{
    /* Copy the source string and hold a reference */
    Tcl_Obj *srcStringObj = StringListGetSourceObj(srcPtr);
    copyPtr->internalRep.twoPtrValue.ptr1 = srcStringObj;
    Tcl_IncrRefCount(srcStringObj);

    /* Copy the ancillary information if there is any. */
    StringListInfo *srcInfoPtr = StringListGetInfo(srcPtr);
    if (srcInfoPtr) {
	StringListInfo *copyInfoPtr;
	copyInfoPtr = (StringListInfo *)Tcl_Alloc(sizeof(*copyInfoPtr));
	*copyInfoPtr = *srcInfoPtr;
	copyPtr->internalRep.twoPtrValue.ptr2 = copyInfoPtr;
    } else {
	    copyPtr->internalRep.twoPtrValue.ptr2 = NULL;
    }
    copyPtr->typePtr = &stringListType;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeStringListInternalRep --
 *
 *	Free any allocated memory in the StringList Rep
 *
 * Results:
 *	None.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */
static void
FreeStringListInternalRep(Tcl_Obj *strListObj)
{
    Tcl_Obj *srcStringObj = StringListGetSourceObj(strListObj);
    assert(srcStringObj);
    if (srcStringObj) {
	    Tcl_DecrRefCount(srcStringObj);
    }
    StringListInfo *infoPtr = StringListGetInfo(strListObj);
    if (infoPtr) {
	    Tcl_Free(infoPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclNewStringListObj --
 *
 *	Creates a new StringList object formed by splitting the source string
 *  object.
 *
 * Results:
 *
 * 	A Tcl_Obj pointer to the created StringList object with ref count 0.
 *
 * Side Effects:
 *
 * 	None.
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclNewStringListObj(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Obj *srcStringObj) /* String to split */
{
    Tcl_Obj *objPtr = Tcl_NewObj();
    Tcl_InvalidateStringRep(objPtr);
    objPtr->internalRep.twoPtrValue.ptr1 = srcStringObj;
    Tcl_IncrRefCount(srcStringObj);
    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    objPtr->typePtr = &stringListType;
    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * StringListGetExtent
 *
 *  Gets the starting index and length subrange within the source string
 *  that comprises this list.
 *
 * Results:
 *
 * 	Returns the length of the list.
 *
 * Side Effects:
 *
 *  The starting index in the source string is stored in startIndexPtr
 *  if it is not NULL.
 *----------------------------------------------------------------------
 */

static Tcl_Size
StringListGetExtent(Tcl_Obj *strListObj, Tcl_Size *startIndexPtr, Tcl_Size *numCharsPtr, int *reversedPtr)
{
    Tcl_Obj *srcStringObj;
    StringListInfo *infoPtr;
    Tcl_Size numChars;
    Tcl_Size listLen;
    Tcl_Size startIndex;
    int reversed;

    srcStringObj = StringListGetSourceObj(strListObj);
    infoPtr = StringListGetInfo(strListObj);

    assert(srcStringObj);
    numChars = Tcl_GetCharLength(srcStringObj);

    if (infoPtr) {
	/* The list is a subrange of the string */
	startIndex = infoPtr->startIndex;
	if (infoPtr->count >= 0) {
	    listLen = infoPtr->count;
	    assert(listLen <= numChars);
	}
	else {
	    listLen = numChars - startIndex;
	}
	reversed = (infoPtr->flags & STRINGLIST_REVERSE) != 0;
    } else {
	startIndex = 0;
	listLen = numChars;
	reversed = 0;
    }

    assert(listLen <= numChars);
    assert(startIndex == 0 || startIndex < numChars);
    assert(startIndex <= (numChars - listLen));

    if (startIndexPtr) {
	*startIndexPtr = startIndex;
    }
    if (numCharsPtr) {
	*numCharsPtr = numChars;
    }
    if (reversedPtr) {
	*reversedPtr = reversed;
    }
    return listLen;
}

/*
 *----------------------------------------------------------------------
 *
 * StringListObjIndex --
 *
 *	Returns the element with the specified index in the list. If index
 *  is out of range, an empty Tcl_Obj is returned.
 *
 * Results:
 *
 * 	TCL_OK on success.
 *
 * Side Effects:
 *
 * 	On success, the 
 *
 *----------------------------------------------------------------------
 */

int
StringListObjIndex(
    TCL_UNUSED(Tcl_Interp *),/* Used for error reporting if not NULL. */
    Tcl_Obj *strListObj,     /* List obj */
    Tcl_Size index,          /* index to element of interest */
    Tcl_Obj **elemObj)       /* Return value */
{
    Tcl_Size listLen;
    Tcl_Size startIndex;
    Tcl_Obj *srcStringObj;
    int reversed;
    Tcl_Size strIndex;

    if (index < 0) {
	goto outOfBounds;
    }

    srcStringObj = StringListGetSourceObj(strListObj);
    assert(srcStringObj);

    listLen = StringListGetExtent(strListObj, &startIndex, NULL, &reversed);
    if (index >= listLen) {
	goto outOfBounds;
    }

    /*
     * The index into the string depends on whether the list is only a
     * substring of the string and whether it is reversed.
     */
    if (reversed) {
	strIndex = startIndex + listLen - index - 1;
    } else {
	strIndex = startIndex + index;
    }

    char buf[TCL_UTF_MAX];
    int utfLen;

    assert(strIndex <
	   Tcl_GetCharLength(srcStringObj));
    Tcl_UniChar ch = Tcl_GetUniChar(srcStringObj, strIndex);
    utfLen = Tcl_UniCharToUtf(ch, buf);
    *elemObj = Tcl_NewStringObj(buf, utfLen);

    return TCL_OK;

outOfBounds:
    /* TODO - NULL or empty obj? */
    *elemObj = Tcl_NewObj();
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StringListObjLength
 *
 *	Returns the length of the StringList
 *
 * Results:
 *
 * 	The length of the list.
 *
 * Side Effects:
 *
 * 	None.
 *
 *----------------------------------------------------------------------
 */
Tcl_Size StringListObjLength(Tcl_Obj *strListObj)
{
    return StringListGetExtent(strListObj, NULL, NULL, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * StringListObjRange --
 *
 *	Makes a slice of an StringList.
 *
 * Results:
 *	Returns a pointer to the sliced series.
 *      This may be a new object or the same object if not shared.
 *
 * Side effects:
 *	?The possible conversion of the object referenced by listPtr?
 *	?to a list object.?
 *
 *----------------------------------------------------------------------
 */

int
StringListObjRange(
    TCL_UNUSED(Tcl_Interp *),   /* For error message(s) */
    Tcl_Obj *strListObj,	/* List object to take a range from. */
    Tcl_Size fromIdx,		/* Index of first element to include. */
    Tcl_Size toIdx,		/* Index of last element to include. */
    Tcl_Obj **newObjPtr)        /* return value */
{
    Tcl_Size listLen;
    Tcl_Size startIndex;
    Tcl_Obj *srcStringObj;
    int reversed;

    srcStringObj = StringListGetSourceObj(strListObj);
    assert(srcStringObj);

    listLen = StringListGetExtent(strListObj, &startIndex, NULL, &reversed);

    if (fromIdx < 0) {
	fromIdx = 0;
    }

    if (toIdx >= listLen) {
	toIdx = listLen - 1;
    } 
    
    if (fromIdx > toIdx ||
	fromIdx >= listLen) {
	TclNewObj(*newObjPtr);
	return TCL_OK;
    }

    assert(fromIdx >= 0);
    assert(toIdx >= fromIdx);
    assert(listLen > toIdx);

    /*
     * We could do a Tcl_Range to allocate a new string but instead
     * keep the old one and just add ancillary information specifying
     * range and reverse setting.
     */
    StringListInfo *infoPtr;
    infoPtr = (StringListInfo *) Tcl_Alloc(sizeof(*infoPtr));

    infoPtr->count = toIdx - fromIdx + 1;
    if (reversed) {
	infoPtr->startIndex = startIndex + listLen - toIdx - 1;
	infoPtr->flags = STRINGLIST_REVERSE;
    } else {
	infoPtr->startIndex = startIndex + fromIdx;
	infoPtr->flags = 0;
	assert(fromIdx < Tcl_GetCharLength(strListObj));
	assert(toIdx < Tcl_GetCharLength(strListObj));
    }

    Tcl_Obj *newObj = Tcl_NewObj();
    Tcl_InvalidateStringRep(newObj);
    newObj->internalRep.twoPtrValue.ptr1 = srcStringObj;
    Tcl_IncrRefCount(srcStringObj);
    newObj->internalRep.twoPtrValue.ptr2 = infoPtr;
    newObj->typePtr = &stringListType;

    *newObjPtr = newObj;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StringListObjReverse --
 *
 *	Reverse the order of the StringList value.
 *
 * Results:
 *      The result will be StringList in the reverse order.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */
int
StringListObjReverse(
    TCL_UNUSED(Tcl_Interp *),   /* For error message(s) */
    Tcl_Obj *strListObj,	/* List object to reverse. */
    Tcl_Obj **newObjPtr)
{
    Tcl_Obj *srcStringObj, *newObj;
    StringListInfo *infoPtr, *newInfoPtr;

    srcStringObj = StringListGetSourceObj(strListObj);
    assert(srcStringObj);

    newObj = Tcl_NewObj();
    Tcl_InvalidateStringRep(newObj);
    newObj->internalRep.twoPtrValue.ptr1 = srcStringObj;
    Tcl_IncrRefCount(srcStringObj);
    newObj->typePtr = &stringListType;
    
    infoPtr = StringListGetInfo(strListObj);
    if (infoPtr) {
	if (infoPtr->startIndex == 0 && infoPtr->count < 0 &&
	    (infoPtr->flags & STRINGLIST_REVERSE)) {
	    /* No need for ancillary information */
	    newInfoPtr = NULL;
	} else {
	    newInfoPtr = (StringListInfo *) Tcl_Alloc(sizeof(*newInfoPtr));
	    newInfoPtr->startIndex = infoPtr->startIndex;
	    newInfoPtr->count = infoPtr->count;
	    newInfoPtr->flags = infoPtr->flags ^ STRINGLIST_REVERSE;
	}
    } else {
	newInfoPtr = (StringListInfo *) Tcl_Alloc(sizeof(*newInfoPtr));
	newInfoPtr->startIndex = 0;
	newInfoPtr->count = -1; /* Entire string */
	newInfoPtr->flags = STRINGLIST_REVERSE;
    }

    newObj->internalRep.twoPtrValue.ptr2 = newInfoPtr;
    *newObjPtr = newObj;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfStringList --
 *
 *	Update the string representation for an StringList object.
 *	Note: This procedure does not invalidate an existing old string rep
 *	so storage will be lost if this has not already been done.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the list-to-string conversion. This string will be empty if the
 *	list has no elements. The list internal representation
 *	should be a StringList when this function is called.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfStringList(Tcl_Obj *strListObj)
{
    Tcl_Size listLen;
    Tcl_Size startIndex;
    Tcl_Size numChars;
    Tcl_Obj *srcStringObj;
    int reversed;

    assert(strListObj->bytes == NULL);

    srcStringObj = StringListGetSourceObj(strListObj);
    assert(srcStringObj);

    listLen =
	StringListGetExtent(strListObj, &startIndex, &numChars, &reversed);
    if (listLen == 0) {
	Tcl_InitStringRep(strListObj, NULL, 0);
	return;
    }

    if (reversed) {
	Tcl_Obj *tempObj = TclStringReverse(srcStringObj, TCL_STRING_IN_PLACE);
	/* NOTE: TclStringReverse may return same or different Tcl_Obj! */
	srcStringObj = tempObj;
	startIndex = numChars - (startIndex + listLen) - 1;
    }

    /* srcStringObj may be original or newly allocated so we have to decrref later */
    Tcl_IncrRefCount(srcStringObj);

    const char *utf;
    const char *utfStart = Tcl_GetString(srcStringObj);
#   define LOCAL_SIZE 64
    char localFlags[LOCAL_SIZE], *flagPtr = NULL;
    Tcl_Size i, bytesNeeded;
    int utfLen, uniChar;

    utfStart = Tcl_UtfAtIndex(utfStart, startIndex);

    /* Pass 1: estimate space, gather flags. */
    if (listLen <= LOCAL_SIZE) {
	flagPtr = localFlags;
    } else {
	/* We know numElems <= LIST_MAX, so this is safe. */
	flagPtr = (char *)Tcl_Alloc(listLen);
    }
    for (i = 0, bytesNeeded = 0, utf = utfStart; i < listLen; ++i) {
	flagPtr[i] = (i ? TCL_DONT_QUOTE_HASH : 0);
	utfLen = Tcl_UtfToUniChar(utf, &uniChar);
	bytesNeeded += TclScanElement(utf, utfLen, flagPtr+i);
	if (bytesNeeded > TCL_SIZE_MAX - listLen) {
	    Tcl_Panic("max size for a Tcl value (%" TCL_Z_MODIFIER "d bytes) exceeded", TCL_SIZE_MAX);
	}
	utf += utfLen;
    }
    bytesNeeded += listLen - 1; /* Room for space separators */

    /*
     * Pass 2: copy into string rep buffer.
     */

    char * const dstStart = Tcl_InitStringRep(strListObj, NULL, bytesNeeded);
    char *dst;
    TclOOM(dstStart, bytesNeeded);
    for (i = 0, dst = dstStart, utf = utfStart; i < listLen; ++i) {
	flagPtr[i] |= (i ? TCL_DONT_QUOTE_HASH : 0);
	utfLen = Tcl_UtfToUniChar(utf, &uniChar);
	dst += TclConvertElement(utf, utfLen, dst, flagPtr[i]);
	*dst++ = ' ';
	utf += utfLen;
    }

    /* Set the string length to what was actually written, the safe choice */
    (void) Tcl_InitStringRep(strListObj, NULL, dst - 1 - dstStart);

    Tcl_DecrRefCount(srcStringObj);
    if (flagPtr != localFlags) {
	Tcl_Free(flagPtr);
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
