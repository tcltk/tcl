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
 * RCS: @(#) $Id: tclUnicodeObj.c,v 1.5 1999/06/10 04:28:51 stanton Exp $
 */

#include <math.h>
#include "tclInt.h"
#include "tclPort.h"

/*
 * Prototypes for local procedures defined in this file:
 */

static int		AllSingleByteChars _ANSI_ARGS_((Tcl_Obj *objPtr));
static void		AppendUniCharStrToObj _ANSI_ARGS_((Tcl_Obj *objPtr,
			    Tcl_UniChar *unichars, int numNewChars));	
static void		DupUnicodeInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static void		FreeUnicodeInternalRep _ANSI_ARGS_((Tcl_Obj *objPtr));
static void		UpdateStringOfUnicode _ANSI_ARGS_((Tcl_Obj *objPtr));
static void		SetOptUnicodeFromAny _ANSI_ARGS_((Tcl_Obj *objPtr,
			    int numChars));
static void		SetFullUnicodeFromAny _ANSI_ARGS_((Tcl_Obj *objPtr,
			    char *src, int numBytes, int numChars));
static int		SetUnicodeFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));

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
    size_t allocated;		/* The amount of space actually allocated. */
    Tcl_UniChar chars[2];	/* The array of chars.  The actual size of
				 * this field depends on the 'allocated' field
				 * above. */
} Unicode;

#define UNICODE_SIZE(len)	\
		((unsigned) (sizeof(Unicode) - (sizeof(Tcl_UniChar)*2) + (len)))
#define GET_UNICODE(objPtr) \
		((Unicode *) (objPtr)->internalRep.otherValuePtr)
#define SET_UNICODE(objPtr, unicodePtr) \
		(objPtr)->internalRep.otherValuePtr = (VOID *) (unicodePtr)


/*
 *----------------------------------------------------------------------
 *
 * TclGetUnicodeFromObj --
 *
 *	Get the index'th Unicode character from the Unicode object.  If
 *	the object is not already a Unicode object, an attempt will be
 *	made to convert it to one.  The index is assumed to be in the
 *	appropriate range.
 *
 * Results:
 *	Returns a pointer to the object's internal unicode string.
 *
 * Side effects:
 *	Converts the object to have the Unicode internal rep.
 *
 *----------------------------------------------------------------------
 */

Tcl_UniChar *
TclGetUnicodeFromObj(objPtr)
    Tcl_Obj *objPtr;	/* The object to find the unicode string for. */
{
    Unicode *unicodePtr;
    int numBytes;
    char *src;
    
    SetUnicodeFromAny(NULL, objPtr);
    unicodePtr = GET_UNICODE(objPtr);
    
    if (unicodePtr->allocated == 0) {

	/*
	 * If all of the characters in the Utf string are 1 byte chars,
	 * we don't normally store the unicode str.  Since this
	 * function must return a unicode string, and one has not yet
	 * been stored, force the Unicode to be calculated and stored
	 * now.
	 */
	
	src = Tcl_GetStringFromObj(objPtr, &numBytes);
	SetFullUnicodeFromAny(objPtr, src, numBytes, unicodePtr->numChars);

	/*
	 * We need to fetch the pointer again because we have just
	 * reallocated the structure to make room for the Unicode data.
	 */
	
	unicodePtr = GET_UNICODE(objPtr);
    }
    return unicodePtr->chars;
}

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
    Tcl_UniChar unichar;
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
	unichar = unicodePtr->chars[index];
    }
    return unichar;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetRangeFromObj --
 *
 *	Create a Tcl Object that contains the chars between first and last
 *	of the object indicated by "objPtr".  If the object is not already
 *	a Unicode object, an attempt will be made to convert it to one.
 *	The first and last indices are assumed to be in the appropriate
 *	range.
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
    Unicode *unicodePtr;
    int length;
    
    SetUnicodeFromAny(NULL, objPtr);
    unicodePtr = GET_UNICODE(objPtr);
    length = objPtr->length;
    
    if (unicodePtr->numChars != length) {
	newObjPtr = TclNewUnicodeObj(unicodePtr->chars + first, last-first+1);
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
 *	This procedure appends the contents of "srcObjPtr" to the Unicode
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
	    unicharSrcStr = unicodePtr->chars;
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

    AppendUniCharStrToObj(resultObjPtr, unicharSrcStr, numChars);
    Tcl_DStringFree(&dsPtr);
    return resultObjPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * AppendUniCharStrToObj --
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

static void
AppendUniCharStrToObj(objPtr, unichars, numNewChars)
    register Tcl_Obj *objPtr;	/* Points to the object to append to. */
    Tcl_UniChar *unichars;	/* The unicode string to append to the
			         * object. */
    int numNewChars;		/* Number of chars in "unichars". */
{
    Unicode *unicodePtr;
    int numChars;
    size_t numBytes;

    SetUnicodeFromAny(NULL, objPtr);
    unicodePtr = GET_UNICODE(objPtr);
    
    numChars = numNewChars + unicodePtr->numChars;
    numBytes = (numChars + 1) * sizeof(Tcl_UniChar);
    
    if (unicodePtr->allocated < numBytes) {
	int allocatedBytes = numBytes * 2;
    
	/*
	 * There isn't currently enough space in the Unicode
	 * representation so allocate additional space.  In fact,
	 * overallocate so that there is room for future growth without
	 * having to reallocate again.
	 */

	unicodePtr = (Unicode *) ckrealloc((char*) unicodePtr,
		UNICODE_SIZE(allocatedBytes));
	unicodePtr->allocated = allocatedBytes;	
	unicodePtr = SET_UNICODE(objPtr, unicodePtr);
    }
    memcpy((VOID *) (unicodePtr->chars + unicodePtr->numChars),
	    (VOID *) unichars, (size_t) numNewChars * sizeof(Tcl_UniChar));
    unicodePtr->chars[numChars] = 0;
    unicodePtr->numChars = numChars;

    /*
     * Invalidate the StringRep.
     */

    Tcl_InvalidateStringRep(objPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclAppendUnicodeToObj --
 *
 *	This procedure appends a Unicode string to an object in the
 *	most efficient manner possible.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Invalidates the string rep and creates a new Unicode string.
 *
 *----------------------------------------------------------------------
 */

void
TclAppendUnicodeToObj(objPtr, unichars, length)
    register Tcl_Obj *objPtr;	/* Points to the object to append to. */
    Tcl_UniChar *unichars;	/* The unicode string to append to the
			         * object. */
    int length;			/* Number of chars in "unichars". */
{
    Unicode *unicodePtr;
    int numChars, i;
    size_t newSize;
    char *src;
    Tcl_UniChar *dst;

    if (Tcl_IsShared(objPtr)) {
	panic("TclAppendUnicodeToObj called with shared object");
    }

    SetUnicodeFromAny(NULL, objPtr);
    unicodePtr = GET_UNICODE(objPtr);
    
    /*
     * Make the buffer big enough for the result.
     */

    numChars = unicodePtr->numChars + length;
    newSize = (numChars + 1) * sizeof(Tcl_UniChar);

    if (newSize > unicodePtr->allocated) {
	int allocated = newSize * 2;
    
	unicodePtr = (Unicode *) ckrealloc((char*)unicodePtr,
		UNICODE_SIZE(allocated));

	if (unicodePtr->allocated == 0) {
	    /*
	     * If the original string was not in Unicode form, add it to the
	     * beginning of the buffer.
	     */

	    src = objPtr->bytes;
	    dst = unicodePtr->chars;
	    for (i = 0; i < unicodePtr->numChars; i++) {
		src += Tcl_UtfToUniChar(src, dst++);
	    }
	}
	unicodePtr->allocated = allocated;
    }

    /*
     * Copy the new string onto the end of the old string, then add the
     * trailing null.
     */

    memcpy((VOID*) (unicodePtr->chars + unicodePtr->numChars), unichars,
	    length * sizeof(Tcl_UniChar));
    unicodePtr->numChars = numChars;
    unicodePtr->chars[numChars] = 0;

    SET_UNICODE(objPtr, unicodePtr);

    Tcl_InvalidateStringRep(objPtr);
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
    int numBytes, allocated;

    numBytes = numChars * sizeof(Tcl_UniChar);

    /*
     * Allocate extra space for the null character
     */

    allocated = numBytes + sizeof(Tcl_UniChar);
    
    TclNewObj(objPtr);
    objPtr->bytes = NULL;
    objPtr->typePtr = &tclUnicodeType;

    unicodePtr = (Unicode *) ckalloc(UNICODE_SIZE(allocated));
    unicodePtr->numChars = numChars;
    unicodePtr->allocated = allocated;
    memcpy((VOID *) unicodePtr->chars, (VOID *) unichars, (size_t) numBytes);
    unicodePtr->chars[numChars] = 0;
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
	copyUnicodePtr = (Unicode *) ckalloc(sizeof(Unicode));
	copyUnicodePtr->allocated = 0;
    } else {
	int allocated = srcUnicodePtr->allocated;

	copyUnicodePtr = (Unicode *) ckalloc(UNICODE_SIZE(allocated));

	copyUnicodePtr->allocated = allocated;
	memcpy((VOID *) copyUnicodePtr->chars,
		(VOID *) srcUnicodePtr->chars,
		(size_t) (srcUnicodePtr->numChars + 1) * sizeof(Tcl_UniChar));
    }
    copyUnicodePtr->numChars = srcUnicodePtr->numChars;
    SET_UNICODE(copyPtr, copyUnicodePtr);
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
    src = unicodePtr->chars;
    length = unicodePtr->numChars * sizeof(Tcl_UniChar);

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
 *	Generate the optimized Unicode internal rep from the string rep.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Unicode ojbect is opitmized for the case where each UTF char in
 *	a string is only one byte.  In this case, we store the value of
 *	numChars, but we don't copy the bytes to the unicodeObj->chars.
 *	Before accessing obj->chars, check if all chars are 1 byte long.
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

    typePtr = objPtr->typePtr;
    if ((typePtr != NULL) && (typePtr->freeIntRepProc) != NULL) {
	(*typePtr->freeIntRepProc)(objPtr);
    }
    objPtr->typePtr = &tclUnicodeType;

    /*
     * Allocate enough space for the basic Unicode structure.
     */

    unicodePtr = (Unicode *) ckalloc(sizeof(Unicode));
    unicodePtr->numChars = numChars;
    unicodePtr->allocated = 0;
    SET_UNICODE(objPtr, unicodePtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * SetFullUnicodeFromAny --
 *
 *	Generate the full (non-optimized) Unicode internal rep from the
 *	string rep.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Unicode internal rep will contain a copy of the string "src" in
 *	unicode format.
 *
 *---------------------------------------------------------------------------
 */

static void
SetFullUnicodeFromAny(objPtr, src, numBytes, numChars)
    Tcl_Obj *objPtr;		/* The object to convert to type Unicode. */
    char *src;
    int numBytes;
    int numChars;
{
    Tcl_ObjType *typePtr;
    Unicode *unicodePtr;
    char *srcEnd;
    Tcl_UniChar *dst;
    size_t length = (numChars + 1) * sizeof(Tcl_UniChar);
    
    unicodePtr = (Unicode *) ckalloc(UNICODE_SIZE(length));
    srcEnd = src + numBytes;
	
    for (dst = unicodePtr->chars; src < srcEnd; dst++) {
	src += Tcl_UtfToUniChar(src, dst);
    }
    *dst = 0;

    unicodePtr->numChars = numChars;
    unicodePtr->allocated = length;
    
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
 *	object is opitmized for the case where each UTF char in a string is
 *	only one byte.  In this case, we store the value of numChars, but we
 *	don't copy the bytes to the unicodeObj->chars.  Before accessing
 *	obj->chars, check if all chars are 1 byte long.
 *
 *---------------------------------------------------------------------------
 */

static int
SetUnicodeFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Not used. */
    Tcl_Obj *objPtr;		/* The object to convert to type Unicode. */
{
    int numBytes, numChars;
    char *src;
    
    if (objPtr->typePtr != &tclUnicodeType) {
	src = Tcl_GetStringFromObj(objPtr, &numBytes);

	numChars = Tcl_NumUtfChars(src, numBytes);
	if (numChars == numBytes) {
	    SetOptUnicodeFromAny(objPtr, numChars);
	} else {
	    SetFullUnicodeFromAny(objPtr, src, numBytes, numChars);
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
