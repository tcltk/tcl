/* 
 * tclUnicodeObj.c --
 *
 *	This file contains the implementation of the Unicode internal
 *	representation of Tcl objects.
 *
 * Copyright (c) 1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclUnicodeObj.c,v 1.2 1999/06/08 02:59:27 hershey Exp $
 */

#include <math.h>
#include "tclInt.h"
#include "tclPort.h"

/*
 * Prototypes for local procedures defined in this file:
 */

static void		DupUnicodeInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static void		FreeUnicodeInternalRep _ANSI_ARGS_((Tcl_Obj *objPtr));
static void		UpdateStringOfUnicode _ANSI_ARGS_((Tcl_Obj *objPtr));
static int		SetUnicodeFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));

static int		AllSingleByteChars _ANSI_ARGS_((Tcl_Obj *objPtr));
static void		TclAppendUniCharStrToObj _ANSI_ARGS_((
	    		    register Tcl_Obj *objPtr, Tcl_UniChar *unichars,
			    int numChars));
static Tcl_Obj *	TclNewUnicodeObj _ANSI_ARGS_((Tcl_UniChar *unichars,
			    int numChars));
static void		SetOptUnicodeFromAny _ANSI_ARGS_((Tcl_Obj *objPtr,
			    int numChars));

/*
 * The following object type represents a Unicode string.  A Unicode string
 * is an internationalized string.  Conceptually, a Unicode string is an
 * array of 16-bit quantities organized as a sequence of properly formed
 * UTF-8 characters.  There is a one-to-one map between Unicode and UTF
 * characters.  The Unicode ojbect is opitmized for the case where each UTF
 * char in a string is only one byte.  In this case, we store the value of
 * numChars, but we don't copy the bytes to the unicodeObj->chars.  Before
 * accessing obj->chars, check if unicodeObj->numChars == obj->length.
 */

Tcl_ObjType tclUnicodeType = {
    "unicode",
    FreeUnicodeInternalRep,
    DupUnicodeInternalRep,
    UpdateStringOfUnicode,
    SetUnicodeFromAny
};

/*
 * The following structure is the internal rep for a Unicode object.
 * Keeps track of how much memory has been used and how much has been
 * allocated for the Unicode to enable growing and shrinking of the
 * Unicode object with fewer mallocs.  
 */

typedef struct Unicode {
    int numChars;		/* The number of chars in the unicode
				 * string. */
    int used;			/* The number of bytes used in the unicode
				 * string. */
    int allocated;		/* The amount of space actually allocated
				 * minus 1 byte. */
    unsigned char chars[4];	/* The array of chars.  The actual size of
				 * this field depends on the 'allocated' field
				 * above. */
} Unicode;

#define UNICODE_SIZE(len)	\
		((unsigned) (sizeof(Unicode) - 4 + (len)))
#define GET_UNICODE(objPtr) \
		((Unicode *) (objPtr)->internalRep.otherValuePtr)
#define SET_UNICODE(objPtr, unicodePtr) \
		(objPtr)->internalRep.otherValuePtr = (VOID *) (unicodePtr)


/*
 *----------------------------------------------------------------------
 *
 * TclGetUnicodeLengthFromObj --
 *
 *	Get the length of the Unicode string from the Tcl object.  If
 *	the object is not already a Unicode object, an attempt will be
 *	made to convert it to one.
 *
 * Results:
 *	Pointer to unicode string representing the unicode object.
 *
 * Side effects:
 *	Frees old internal rep.  Allocates memory for new internal rep.
 *
 *----------------------------------------------------------------------
 */

int
TclGetUnicodeLengthFromObj(objPtr)
    Tcl_Obj *objPtr;		/* The Unicode object. */
{
    int length;
    Unicode *unicodePtr;
    
    SetUnicodeFromAny(NULL, objPtr);
    unicodePtr = GET_UNICODE(objPtr);

    length = unicodePtr->numChars;
    return length;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetUniCharFromObj --
 *
 *	Get the index'th Unicode character from the Unicode object.  If
 *	the object is not already a Unicode object, an attempt will be
 *	made to convert it to one.  The index is assumed to be in the
 *	appropriate range.
 *
 * Results:
 *	Returns the index'th Unicode character in the Object.
 *
 * Side effects:
 *	Fills unichar with the index'th Unicode character.
 *
 *----------------------------------------------------------------------
 */

Tcl_UniChar
TclGetUniCharFromObj(objPtr, index)
    Tcl_Obj *objPtr;		/* The Unicode object. */
    int index;			/* Get the index'th character. */
{
    Tcl_UniChar *unicharPtr, unichar;
    Unicode *unicodePtr;
    int length;
    
    SetUnicodeFromAny(NULL, objPtr);
    unicodePtr = GET_UNICODE(objPtr);
    length = objPtr->length;
    
    if (AllSingleByteChars(objPtr)) {
	int length;
	char *str;

	/*
	 * All of the characters in the Utf string are 1 byte chars,
	 * so we don't store the unicode char.  We get the Utf string
	 * and convert the index'th byte to a Unicode character.
	 */
	
	str = Tcl_GetStringFromObj(objPtr, &length);
	Tcl_UtfToUniChar(&str[index], &unichar);	
    } else {
	unicharPtr = (Tcl_UniChar *)unicodePtr->chars;
	unichar = unicharPtr[index];
    }
    return unichar;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetRangeFromObj --
 *
 *	Create a Tcl Object that contains the chars between first and
 *	last of the object indicated by "objPtr".  If the object is not
 *	already a Unicode object, an attempt will be made to convert it
 *	to one.  The first and last indices are assumed to be in the
 *	appropriate range.
 *
 * Results:
 *	Returns a new Tcl Object of either "string" or "unicode" type,
 *	containing the range of chars.
 *
 * Side effects:
 *	Changes the internal rep of "objPtr" to unicode.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
TclGetRangeFromObj(objPtr, first, last)
   
 Tcl_Obj *objPtr;		/* The Tcl object to find the range of. */
    int first;			/* First index of the range. */
    int last;			/* Last index of the range. */
{
    Tcl_Obj *newObjPtr;		/* The Tcl object to find the range of. */
    Tcl_UniChar *unicharPtr;
    Unicode *unicodePtr;
    int length;
    
    SetUnicodeFromAny(NULL, objPtr);
    unicodePtr = GET_UNICODE(objPtr);
    length = objPtr->length;
    
    if (unicodePtr->numChars != length) {
	unicharPtr = (Tcl_UniChar *)unicodePtr->chars;
	newObjPtr = TclNewUnicodeObj(&unicharPtr[first], last-first+1);
    } else {
	int length;
	char *str;

	/*
	 * All of the characters in the Utf string are 1 byte chars,
	 * so we don't store the unicode char.  Create a new string
	 * object containing the specified range of chars.
	 */
	
	str = Tcl_GetStringFromObj(objPtr, &length);
	newObjPtr = Tcl_NewStringObj(&str[first], last-first+1);	
    }
    return newObjPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclAppendObjToUnicodeObj --
 *
 *	This procedure appends the contest of "srcObjPtr" to the Unicode
 *	object "destPtr".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If srcObjPtr doesn't have an internal rep, then it is given a
 *	Unicode internal rep.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclAppendObjToUnicodeObj(targetObjPtr, srcObjPtr)
    register Tcl_Obj *targetObjPtr;	/* Points to the object to
					 * append to. */
    register Tcl_Obj *srcObjPtr;	/* Points to the object to
					 * append from. */
{
    int numBytes, numChars;
    Tcl_Obj *resultObjPtr;
    char *utfSrcStr;
    Tcl_UniChar *unicharSrcStr;
    Unicode *unicodePtr;
    Tcl_DString dsPtr;
    
    /*
     * Duplicate the target if it is shared.
     * Change the result's internal rep to Unicode object.
     */
    
    if (Tcl_IsShared(targetObjPtr)) {
	resultObjPtr = Tcl_DuplicateObj(targetObjPtr);
    } else {
	resultObjPtr = targetObjPtr;
    }
    SetUnicodeFromAny(NULL, resultObjPtr);

    /*
     * Case where target chars are 1 byte long:
     * If src obj is of "string" or null type, then convert it to "unicode"
     * type.  Src objs of other types (such as int) are left in tact to keep
     * them from shimmering between types.  If the src obj is a unichar obj,
     * and all src chars are also 1 byte long, the src string is appended to
     * the target "unicode" obj, and the target obj maintains its "optimized"
     * status.
     */

    if (AllSingleByteChars(resultObjPtr)) {

	int length;
	char *stringRep;

	if (srcObjPtr->typePtr == &tclStringType
		|| srcObjPtr->typePtr == NULL) {
	    SetUnicodeFromAny(NULL, srcObjPtr);
	}

	stringRep = Tcl_GetStringFromObj(srcObjPtr, &length);
	Tcl_AppendToObj(resultObjPtr, stringRep, length);

	if ((srcObjPtr->typePtr == &tclUnicodeType)
		&& (AllSingleByteChars(srcObjPtr))) {
	    SetOptUnicodeFromAny(resultObjPtr, resultObjPtr->length);
	}
	return resultObjPtr;
    }

    /*
     * Extract a unicode string from "unicode" or "string" type objects.
     * Extract the utf string from non-unicode objects, and convert the
     * utf string to unichar string locally.
     * If the src obj is a "string" obj, convert it to "unicode" type.
     * Src objs of other types (such as int) are left in tact to keep
     * them from shimmering between types.
     */

    Tcl_DStringInit(&dsPtr);
    if (srcObjPtr->typePtr == &tclStringType || srcObjPtr->typePtr == NULL) {
	SetUnicodeFromAny(NULL, srcObjPtr);
    }
    if (srcObjPtr->typePtr == &tclUnicodeType) {
	if (AllSingleByteChars(srcObjPtr)) {

	    unicodePtr = GET_UNICODE(srcObjPtr);
	    numChars = unicodePtr->numChars;

	    utfSrcStr = Tcl_GetStringFromObj(srcObjPtr, &numBytes);
	    unicharSrcStr = (Tcl_UniChar *)Tcl_UtfToUniCharDString(utfSrcStr,
		    numBytes, &dsPtr);
	} else {
	    unicodePtr = GET_UNICODE(srcObjPtr);
	    numChars = unicodePtr->numChars;
	    unicharSrcStr = (Tcl_UniChar *)unicodePtr->chars;
	}
    } else {
	utfSrcStr = Tcl_GetStringFromObj(srcObjPtr, &numBytes);
	numChars = Tcl_NumUtfChars(utfSrcStr, numBytes);
	unicharSrcStr = (Tcl_UniChar *)Tcl_UtfToUniCharDString(utfSrcStr,
		numBytes, &dsPtr);
    }
    if (numChars == 0) {
	return resultObjPtr;
    }

    /*
     * Append the unichar src string to the result object.
     */

    TclAppendUniCharStrToObj(resultObjPtr, unicharSrcStr, numChars);
    Tcl_DStringFree(&dsPtr);
    return resultObjPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclAppendUniCharStrToObj --
 *
 *	This procedure appends the contents of "srcObjPtr" to the
 *	Unicode object "objPtr".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If srcObjPtr doesn't have an internal rep, then it is given a
 *	Unicode internal rep.
 *
 *----------------------------------------------------------------------
 */

void
TclAppendUniCharStrToObj(objPtr, unichars, numNewChars)
    register Tcl_Obj *objPtr;	/* Points to the object to append to. */
    Tcl_UniChar *unichars;	/* The unicode string to append to the
			         * object. */
    int numNewChars;		/* Number of chars in "unichars". */
{
    Unicode *unicodePtr;
    int usedBytes, numNewBytes, totalNumBytes, totalNumChars;

    /*
     * Invalidate the StringRep.
     */

    Tcl_InvalidateStringRep(objPtr);

    unicodePtr = GET_UNICODE(objPtr);
    
    usedBytes = unicodePtr->used;
    totalNumChars = numNewChars + unicodePtr->numChars;
    totalNumBytes = totalNumChars * sizeof(Tcl_UniChar);
    numNewBytes = numNewChars * sizeof(Tcl_UniChar);
    
    if (unicodePtr->allocated < totalNumBytes) {
	int allocatedBytes = totalNumBytes * 2;
    
	/*
	 * There isn't currently enough space in the Unicode
	 * representation so allocate additional space.  In fact,
	 * overallocate so that there is room for future growth without
	 * having to reallocate again.
	 */

	unicodePtr = (Unicode *) ckrealloc(unicodePtr,
		UNICODE_SIZE(allocatedBytes));
	memcpy((VOID *) (unicodePtr->chars + usedBytes),
		(VOID *) unichars, (size_t) numNewBytes);

	unicodePtr->allocated = allocatedBytes;	
	unicodePtr = SET_UNICODE(objPtr, unicodePtr);
    }
    
    memcpy((VOID *) (unicodePtr->chars + usedBytes),
	    (VOID *) unichars, (size_t) numNewBytes);
    unicodePtr->used = totalNumBytes;
    unicodePtr->numChars = totalNumChars;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclNewUnicodeObj --
 *
 *	This procedure is creates a new Unicode object and initializes
 *	it from the given Utf String.  If the Utf String is the same size
 *	as the Unicode string, don't duplicate the data.
 *
 * Results:
 *	The newly created object is returned.  This object will have no
 *	initial string representation.  The returned object has a ref count
 *	of 0.
 *
 * Side effects:
 *	Memory allocated for new object and copy of Unicode argument.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
TclNewUnicodeObj(unichars, numChars)
    Tcl_UniChar *unichars;	/* The unicode string used to initialize
				 * the new object. */
    int numChars;		/* Number of characters in the unicode
				 * string. */
{
    Tcl_Obj *objPtr;
    Unicode *unicodePtr;
    int numBytes;

    numBytes = numChars * sizeof(Tcl_UniChar);
    
    TclNewObj(objPtr);
    objPtr->bytes = NULL;
    objPtr->typePtr = &tclUnicodeType;

    unicodePtr = (Unicode *) ckalloc(UNICODE_SIZE(numBytes));
    unicodePtr->used = numBytes;
    unicodePtr->numChars = numChars;
    unicodePtr->allocated = numBytes;
    memcpy((VOID *) unicodePtr->chars, (VOID *) unichars, (size_t) numBytes);
    SET_UNICODE(objPtr, unicodePtr);
    return objPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclAllSingleByteChars --
 *
 *	Initialize the internal representation of a Unicode Tcl_Obj
 *	to a copy of the internal representation of an existing Unicode
 *	object. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocates memory.
 *
 *---------------------------------------------------------------------------
 */

static int
AllSingleByteChars(objPtr)
    Tcl_Obj *objPtr;		/* Object whose char lengths to check. */
{
    Unicode *unicodePtr;
    int numBytes, numChars;

    unicodePtr = GET_UNICODE(objPtr);
    numChars = unicodePtr->numChars;
    numBytes = objPtr->length;

    if (numChars == numBytes) {
	return 1;
    } else {
	return 0;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * DupUnicodeInternalRep --
 *
 *	Initialize the internal representation of a Unicode Tcl_Obj
 *	to a copy of the internal representation of an existing Unicode
 *	object. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocates memory.
 *
 *---------------------------------------------------------------------------
 */

static void
DupUnicodeInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr;		/* Object with internal rep to set. */
{
    Unicode *srcUnicodePtr = GET_UNICODE(srcPtr);
    Unicode *copyUnicodePtr; /*GET_UNICODE(copyPtr);*/
    
    /*
     * If the src obj is a string of 1-byte Utf chars, then copy the
     * string rep of the source object and create an "empty" Unicode
     * internal rep for the new object.  Otherwise, copy Unicode
     * internal rep, and invalidate the string rep of the new object.
     */
    
    if (AllSingleByteChars(srcPtr)) {
	copyUnicodePtr = (Unicode *) ckalloc(UNICODE_SIZE(4));
    } else {
	int used = srcUnicodePtr->used;
	int allocated = srcUnicodePtr->allocated;
	Tcl_UniChar *unichars;

	unichars = (Tcl_UniChar *)srcUnicodePtr->chars;

	copyUnicodePtr = (Unicode *) ckalloc(UNICODE_SIZE(allocated));

	copyUnicodePtr->used = used;	
	copyUnicodePtr->allocated = allocated;
	memcpy((VOID *) copyUnicodePtr->chars,
		(VOID *) srcUnicodePtr->chars, (size_t) used);
    }
    copyUnicodePtr->numChars = srcUnicodePtr->numChars;
    SET_UNICODE(copyPtr, copyUnicodePtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclSetUnicodeObj --
 *
 *	Modify an object to be a Unicode object and to have the specified
 *	unicode string as its value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's old string rep and internal rep is freed.
 *	Memory allocated for copy of unicode argument.
 *
 *----------------------------------------------------------------------
 */

void
TclSetUnicodeObj(objPtr, chars, length)
    Tcl_Obj *objPtr;		/* Object to initialize as a Unicode obj. */
    unsigned char *chars;	/* The unicode string to use as the new
				 * value. */
    int length;			/* Length of the unicode string, which must
				 * be >= 0. */
{
    Tcl_ObjType *typePtr;
    Unicode *unicodePtr;

    if (Tcl_IsShared(objPtr)) {
	panic("TclSetUnicodeObj called with shared object");
    }
    typePtr = objPtr->typePtr;
    if ((typePtr != NULL) && (typePtr->freeIntRepProc != NULL)) {
	(*typePtr->freeIntRepProc)(objPtr);
    }
    Tcl_InvalidateStringRep(objPtr);

    unicodePtr = (Unicode *) ckalloc(UNICODE_SIZE(length));
    unicodePtr->used = length;
    unicodePtr->allocated = length;
    memcpy((VOID *) unicodePtr->chars, (VOID *) chars, (size_t) length);

    objPtr->typePtr = &tclUnicodeType;
    SET_UNICODE(objPtr, unicodePtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * UpdateStringOfUnicode --
 *
 *	Update the string representation for a Unicode data object.
 *	Note: This procedure does not invalidate an existing old string rep
 *	so storage will be lost if this has not already been done. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the Unicode-to-string conversion.
 *
 *	The object becomes a string object -- the internal rep is
 *	discarded and the typePtr becomes NULL.
 *
 *---------------------------------------------------------------------------
 */

static void
UpdateStringOfUnicode(objPtr)
    Tcl_Obj *objPtr;		/* Unicode object whose string rep to
				 * update. */
{
    int i, length, size;
    Tcl_UniChar *src;
    char dummy[TCL_UTF_MAX];
    char *dst;
    Unicode *unicodePtr;

    unicodePtr = GET_UNICODE(objPtr);
    src = (Tcl_UniChar *) unicodePtr->chars;
    length = unicodePtr->used;

    /*
     * How much space will string rep need?
     */
     
    size = 0;
    for (i = 0; i < unicodePtr->numChars; i++) {
	size += Tcl_UniCharToUtf((int) src[i], dummy);
    }

    dst = (char *) ckalloc((unsigned) (size + 1));
    objPtr->bytes = dst;
    objPtr->length = size;

    for (i = 0; i < unicodePtr->numChars; i++) {
	dst += Tcl_UniCharToUtf(src[i], dst);
    }
    *dst = '\0';
}

/*
 *---------------------------------------------------------------------------
 *
 * SetOptUnicodeFromAny --
 *
 *	Generate the Unicode internal rep from the string rep.
 *
 * Results:
 *	The return value is always TCL_OK.
 *
 * Side effects:
 *	A Unicode object is stored as the internal rep of objPtr.  The Unicode
 * ojbect is opitmized for the case where each UTF char in a string is only
 * one byte.  In this case, we store the value of numChars, but we don't copy
 * the bytes to the unicodeObj->chars.  Before accessing obj->chars, check if
 * all chars are 1 byte long.
 *
 *---------------------------------------------------------------------------
 */

static void
SetOptUnicodeFromAny(objPtr, numChars)
    Tcl_Obj *objPtr;		/* The object to convert to type Unicode. */
    int numChars;
{
    Tcl_ObjType *typePtr;
    Unicode *unicodePtr;
    
    unicodePtr = (Unicode *) ckalloc(UNICODE_SIZE(4));
    unicodePtr->numChars = numChars;

    typePtr = objPtr->typePtr;
    if ((typePtr != NULL) && (typePtr->freeIntRepProc) != NULL) {
	(*typePtr->freeIntRepProc)(objPtr);
    }
    objPtr->typePtr = &tclUnicodeType;
    SET_UNICODE(objPtr, unicodePtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * SetUnicodeFromAny --
 *
 *	Generate the Unicode internal rep from the string rep.
 *
 * Results:
 *	The return value is always TCL_OK.
 *
 * Side effects:
 *	A Unicode object is stored as the internal rep of objPtr.  The Unicode
 * ojbect is opitmized for the case where each UTF char in a string is only
 * one byte.  In this case, we store the value of numChars, but we don't copy
 * the bytes to the unicodeObj->chars.  Before accessing obj->chars, check if
 * all chars are 1 byte long.
 *
 *---------------------------------------------------------------------------
 */

static int
SetUnicodeFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Not used. */
    Tcl_Obj *objPtr;		/* The object to convert to type Unicode. */
{
    Tcl_ObjType *typePtr;
    int numBytes, numChars;
    char *src, *srcEnd;
    Unicode *unicodePtr;
    unsigned char *dst;
    
    typePtr = objPtr->typePtr;
    if (typePtr != &tclUnicodeType) {
	src = Tcl_GetStringFromObj(objPtr, &numBytes);

	numChars = Tcl_NumUtfChars(src, numBytes);
	if (numChars == numBytes) {
	    SetOptUnicodeFromAny(objPtr, numChars);
	} else {
	    unicodePtr = (Unicode *) ckalloc(UNICODE_SIZE(numChars
		    * sizeof(Tcl_UniChar)));
	    srcEnd = src + numBytes;
	
	    for (dst = unicodePtr->chars; src < srcEnd;
		 dst += sizeof(Tcl_UniChar)) {
		src += Tcl_UtfToUniChar(src, (Tcl_UniChar *) dst);
	    }

	    unicodePtr->used = numChars * sizeof(Tcl_UniChar);
	    unicodePtr->numChars = numChars;
	    unicodePtr->allocated = numChars * sizeof(Tcl_UniChar);	

	    if ((typePtr != NULL) && (typePtr->freeIntRepProc) != NULL) {
		(*typePtr->freeIntRepProc)(objPtr);
	    }
	    objPtr->typePtr = &tclUnicodeType;
	    SET_UNICODE(objPtr, unicodePtr);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeUnicodeInternalRep --
 *
 *	Deallocate the storage associated with a Unicode data object's
 *	internal representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees memory. 
 *
 *----------------------------------------------------------------------
 */

static void
FreeUnicodeInternalRep(objPtr)
    Tcl_Obj *objPtr;		/* Object with internal rep to free. */
{
    ckfree((char *) GET_UNICODE(objPtr));
}
