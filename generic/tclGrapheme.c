/*
 * tclGrapheme.c --
 *
 *	This file contains functions that implement the Tcl grapheme
 *	accessor command and API.
 *
 * Copyright © 2026 Ashok P. Nadkarni.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "../utf8proc/utf8proc.h" /* Relative path to ignore system include */

/*
 * Implementation note:
 * The utf8proc library provides the utf8proc_map(CHARBOUND) method
 * for splitting into graphemes separated by 0xff. We do not use it
 * directly for multiple reasons
 * - the mapping can reorder the passed UTF-8 because it goes through
 * a decomposition step. Thus in the case of multiple combining marks,
 * it is not possible (without additional work) to map the offsets
 * returned into offsets into the original passed string
 * - lower performance - involves conversion from Tcl's internal TUTF-8
 * to standard UTF-8, then to UTF-32 (internal to utfproc), then further
 * conversion from the 0xFF segmented UTF-8 returned back to Tcl
 * TUTF-8, each involving allocations of the whole string.
 * - has no knowledge of profiles and a means of handling invalid
 * sequences which is always a possibility with Tcl internal reps.
 *
 * We therefore use the lower level utf8proc_grapheme_break routine,
 * iterating and maintaining state ourselves.
 */

/*
 * Prototypes for functions defined later in this file:
 */

static Tcl_ObjCmdProc2		GraphemeIndexCmd;
static Tcl_ObjCmdProc2		GraphemeLengthCmd;
static Tcl_ObjCmdProc2		GraphemeNextCmd;
static Tcl_ObjCmdProc2		GraphemePrevCmd;
static Tcl_ObjCmdProc2		GraphemeRangeCmd;
static Tcl_ObjCmdProc2		GraphemeReverseCmd;
static Tcl_ObjCmdProc2		GraphemeSplitCmd;

/*
 * Table of grapheme subcommand names and implementations.
 */

const EnsembleImplMap tclGraphemeImplMap[] = {
    {"index",	GraphemeIndexCmd,	TclCompileGraphemeIndexCmd, NULL, NULL, 0},
    {"length",	GraphemeLengthCmd,	TclCompileGraphemeLengthCmd, NULL, NULL, 0},
    {"next",	GraphemeNextCmd,	TclCompileGraphemeNextCmd, NULL, NULL, 0},
    {"prev",	GraphemePrevCmd,	TclCompileGraphemePrevCmd, NULL, NULL, 0},
    {"range",	GraphemeRangeCmd,	TclCompileGraphemeRangeCmd, NULL, NULL, 0},
    {"reverse",	GraphemeReverseCmd,	TclCompileGraphemeReverseCmd, NULL, NULL, 0},
    {"split",	GraphemeSplitCmd,	TclCompileGraphemeSplitCmd, NULL, NULL, 0},
    {NULL, NULL, NULL, NULL, NULL, 0}
};

/*
 * GraphemeListRep --
 *
 * Iterations over graphemes ("foreach gr [grapheme split string] {}") may
 * be implemented in one of several ways -
 * 1. Convert to a native Tcl list
 * 2. Maintain a lookup table mapping grapheme indices to Tcl_UniChar offsets
 * 3. Maintain an iterator that caches the last index access
 *
 * Option 1 potentially consumes a lot more memory than necessary for the
 * common case where the "gr" above is discarded on every iteration.
 * Option 2 is generally not memory intensive because the lookup table can
 * be quite small using a form of RLE but there is the performance cost
 * of constructing the lookup table that may only be used once. There is also
 * potential of memory growth in pathlogical cases where RLE is not effective.
 * Option 3, implemented below as an abstract list, depends on the fact
 * that most operations on lists of graphemes will be sequential iteration.
 * It therefore caches the offset of the last grapheme index accessed so
 * the next or previous index can be accessed quickly and without any cost
 * in memory. It does not however help with random access.
 *
 * Note the list is read-only, so writing will cause shimmer to a native list.
 */
typedef struct GraphemeListRep {
    Tcl_Obj *objPtr;		/* The target string */
    Tcl_Size refCount;		/* Shared amongst Tcl_Obj's */
    Tcl_Size graphemeIndex;     /* Cached grapheme index */
    Tcl_Size graphemeOffset;	/* Corresponding Tcl_UniChar offset */
} GraphemeListRep;

static Tcl_DupInternalRepProc	GraphemeListDupProc;
static Tcl_FreeInternalRepProc	GraphemeListFreeProc;
static Tcl_ObjTypeLengthProc	GraphemeListLengthProc;
static Tcl_ObjTypeIndexProc	GraphemeListIndexProc;
const Tcl_ObjType tclGraphemeListType = {
    "graphemeList",
    GraphemeListFreeProc,
    GraphemeListDupProc,
    TclAbstractListUpdateString,
    NULL,			/* SetFromAny */
    TCL_OBJTYPE_V2(
	GraphemeListLengthProc,
	GraphemeListIndexProc,
	NULL,			/* Slice */
	NULL,			/* Reverse */
	NULL,			/* GetElements */
	NULL,			/* SetElement */
	NULL,			/* Replace */
	NULL)			/* In */
};

/*
 *------------------------------------------------------------------------
 *
 * IsPossibleContinuation --
 *
 *	Checks if the passed code point is possibly a continuation of
 *	of a preceding base character possibly separated by intervening
 *	continuations.
 *
 * Results:
 *	true if the code point is postcore, false otherwise.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static inline bool
IsPossibleContinuation (
    Tcl_UniChar cp)		/* Unicode code point */
{
    /*
     * https://www.unicode.org/reports/tr29/tr29-47.html
     */
    switch (utf8proc_get_property(cp)->boundclass) {
		/* postcore */
    case UTF8PROC_BOUNDCLASS_EXTEND:
    case UTF8PROC_BOUNDCLASS_SPACINGMARK:
    case UTF8PROC_BOUNDCLASS_ZWJ:
	   	/* hangul-syllable */
    case UTF8PROC_BOUNDCLASS_L:
    case UTF8PROC_BOUNDCLASS_V:
    case UTF8PROC_BOUNDCLASS_T:
    case UTF8PROC_BOUNDCLASS_LV:
    case UTF8PROC_BOUNDCLASS_LVT:
	return true;
    default:
	return false;
    }
}

/*
 *------------------------------------------------------------------------
 *
 * IsPossiblePrefix --
 *
 *	Checks if the passed code point might be a prefix to a base core
 *	character, that is combined with the next core code point, possibly
 *	with other intervening prefix code points. Note this is a loose check,
 *	not exact, so it may return true even in cases where the character
 *	does not meet the necessary conditions.
 *
 * Results:
 *	true if the code point is precore, false otherwise.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static inline bool
IsPossiblePrefix (
    Tcl_UniChar cp)		/* Unicode code point */
{
    const utf8proc_property_t *propPtr;
    propPtr = utf8proc_get_property(cp);
    return propPtr->boundclass == UTF8PROC_BOUNDCLASS_PREPEND
	|| propPtr->boundclass == UTF8PROC_BOUNDCLASS_ZWJ
	|| propPtr->boundclass == UTF8PROC_BOUNDCLASS_REGIONAL_INDICATOR
	|| propPtr->category == UTF8PROC_CATEGORY_MN
	|| propPtr->category == UTF8PROC_CATEGORY_MC
	|| propPtr->category == UTF8PROC_CATEGORY_ME;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeNext --
 *
 *	Returns the start of the next grapheme. uniLen must be positive!
 *
 * Results:
 *	Pointer to start of next grapheme.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static const Tcl_UniChar *
GraphemeNext(
    const Tcl_UniChar *uniPtr,	/* Start of a grapheme */
    Tcl_Size uniLen)		/* Number of Tcl_UniChar's, > 0! */
{
    assert(uniLen > 0);
    const Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
    int state = 0;
    while (++uniPtr < uniEndPtr) {
	if (utf8proc_grapheme_break_stateful(uniPtr[-1], uniPtr[0], &state)) {
	    break;
	}
    }
    return uniPtr;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemePrev --
 *
 *	Returns the start of the previous grapheme. uniLen must be positive!
 *
 * Results:
 *	Pointer to start of previous grapheme.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static const Tcl_UniChar *
GraphemePrev(
    const Tcl_UniChar *uniEndPtr,	/* Points beyond grapheme to return */
    Tcl_Size uniLen)			/* Number of preceding Tcl_UniChar's,
					 * must be > 0! */
{
    assert(uniLen > 0);

    /*
     * The utf8proc grapheme functions only maintain correct state when
     * traversing in the forward direction. We have to therefore first go
     * back to a place that is guaranteed to not be in the middle of a
     * grapheme and work forward from there. The reason for having to work
     * forward is that the backward scan is a conservative heuristic and
     * may go further back than it needs to. Therefore we have to work
     * forward again to skip over the extra scan.
     */
    const Tcl_UniChar *uniStartPtr = uniEndPtr - uniLen;
    const Tcl_UniChar *uniPtr = uniEndPtr - 1;
    while (uniPtr > uniStartPtr
	    && ((*uniPtr == '\n' && uniPtr[-1] == '\r') ||
		IsPossibleContinuation(*uniPtr) || IsPossiblePrefix(uniPtr[-1]))) {
	uniPtr--;
    }

    /* Now scan forward. Note uniPtr may be < uniStartPtr if uniLen was 0 */
    const Tcl_UniChar *grPtr;
    assert(uniEndPtr > uniPtr);
    do {
	grPtr = uniPtr;
	uniPtr = GraphemeNext(uniPtr, uniEndPtr - uniPtr);
    } while (uniPtr < uniEndPtr);
    return grPtr;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeLength --
 *
 *	Counts the number of graphemes in the passed string.
 *
 * Results:
 *	Number of graphemes in the string.
 *
 *------------------------------------------------------------------------
 */
static Tcl_Size
GraphemeLength (
    const Tcl_UniChar *uniPtr,	/* Tcl_UniChar string */
    Tcl_Size uniLen)		/* Number of Tcl_UniChar's in string. -1 if
				 * nul terminated */
{
    if (uniLen < 0) {
	uniLen = Tcl_UniCharLen(uniPtr);
    }
    const Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
    Tcl_Size count = 0;
    while (uniPtr < uniEndPtr) {
	uniPtr = GraphemeNext(uniPtr, uniEndPtr - uniPtr);
	++count;
    }
    return count;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeIndex --
 *
 *	Locates the grapheme at an index and returns a pointer to it.
 *	If grWidthPtr is not NULL, the number of code points in the
 *	grapheme is returned in it.
 *
 * Results:
 *	Pointer to the grapheme at specified grapheme index. NULL if the
 *	index is out of range and stores 0 in grWidthPtr.
 *
 *------------------------------------------------------------------------
 */
static const Tcl_UniChar *
GraphemeIndex(
    const Tcl_UniChar *uniPtr,	/* Tcl_UniChar string */
    Tcl_Size uniLen,		/* Number of Tcl_UniChar's in string. -1 if
				 * nul terminated */
    Tcl_Size grIndex,		/* Index of desired grapheme */
    Tcl_Size *grWidthPtr)	/* Length of grapheme */
{
    if (grIndex >= 0) {
	if (uniLen < 0) {
	    uniLen = Tcl_UniCharLen(uniPtr);
	}
	const Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
	const Tcl_UniChar *grPtr;
	Tcl_Size i;
	for (i = 0, grPtr = uniPtr; grPtr < uniEndPtr; ++i) {
	    uniPtr = GraphemeNext(grPtr, uniEndPtr - grPtr);
	    if (i == grIndex) {
		if (grWidthPtr != NULL) {
		    *grWidthPtr = uniPtr - grPtr;
		}
		return grPtr;
	    }
	    grPtr = uniPtr;
	}
    }
    if (grWidthPtr != NULL) {
	*grWidthPtr = 0;
    }
    return NULL;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeNextCmd --
 *
 *	Implements the Tcl "grapheme next" command. Sets the interpreter
 *	result to the grapheme in the string objv[1] at the string
 *	index passed via the variable named in objv[2]. The variable is
 *	set to the string index following the returned grapheme.
 *
 * Results:
 *	A standard Tcl result code.
 *
 * Side effects:
 *	Modifies interpreter result and variable passed in objv[2].
 *
 *------------------------------------------------------------------------
 */
static int
GraphemeNextCmd (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,	/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "string strIndex");
	return TCL_ERROR;
    }

    Tcl_Size strIndex = 0;
    Tcl_Size uniLen;
    Tcl_UniChar *uniStartPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
    Tcl_Obj *indexObj = Tcl_ObjGetVar2(interp, objv[2], NULL, 0);

    if (indexObj) {
	if (TclGetIntForIndexM(interp, indexObj, uniLen, &strIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (strIndex < 0) {
	    strIndex = 0;
	}
    }

    Tcl_Size grWidth = 0;
    if (strIndex < uniLen) {
	const Tcl_UniChar *grStartPtr = uniStartPtr + strIndex;
	const Tcl_UniChar *grEndPtr = GraphemeNext(grStartPtr, uniLen - strIndex);
	grWidth = grEndPtr - grStartPtr;
	Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(grStartPtr, grWidth));
    }
    Tcl_ObjSetVar2(interp, objv[2], NULL,
	    Tcl_NewWideIntObj(strIndex + grWidth), 0);
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemePrevCmd --
 *
 *	Implements the Tcl "grapheme prev" command. Sets the interpreter
 *	result to the grapheme in the string objv[1] preceding the string
 *	index passed via the variable named in objv[2]. The variable is
 *	set to the string index preceding the returned grapheme.
 *
 * Results:
 *	A standard Tcl result code.
 *
 * Side effects:
 *	Modifies interpreter result and variable passed in objv[2].
 *
 *------------------------------------------------------------------------
 */
static int
GraphemePrevCmd (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,	/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "string strIndex");
	return TCL_ERROR;
    }

    Tcl_Size uniLen;
    const Tcl_UniChar *uniStartPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
    Tcl_Size strIndex = uniLen;

    Tcl_Obj *indexObj = Tcl_ObjGetVar2(interp, objv[2], NULL, 0);
    if (indexObj) {
	if (TclGetIntForIndexM(interp, indexObj, uniLen, &strIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (strIndex < 0) {
	strIndex = 0;
    } else if (strIndex > uniLen) {
	strIndex = uniLen;
    }

    /*
     * The utf8proc grapheme functions only maintain correct state when
     * traversing in the forward direction. We have to therefore first go
     * back to a place that is guaranteed to not be in the middle of a
     * grapheme and work forward from there. The reason for having to work
     * forward is that the backward scan is a conservative heuristic and
     * may go further back than it needs to. Therefore we have to work
     * forward again to skip over the extra scan.
     */

    /* Scan backward to a safe starting point for the forward scan */
    const Tcl_UniChar *uniEndPtr = uniStartPtr + strIndex;
    const Tcl_UniChar *uniPtr = uniEndPtr - 1;

    while (uniPtr > uniStartPtr
	    && ((*uniPtr == '\n' && uniPtr[-1] == '\r') ||
		IsPossibleContinuation(*uniPtr) || IsPossiblePrefix(uniPtr[-1]))) {
	uniPtr--;
    }

    /* Now scan forward. Note uniPtr may be < uniStartPtr if uniLen was 0 */
    Tcl_Size grLen = 0;
    if (uniPtr >= uniStartPtr) {
	const Tcl_UniChar *grStartPtr;
	assert(uniEndPtr > uniPtr);
	do {
	    grStartPtr = uniPtr;
	    uniPtr = GraphemeNext(uniPtr, uniEndPtr - uniPtr);
	} while (uniPtr < uniEndPtr);
	grLen = uniEndPtr - grStartPtr;
	Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(grStartPtr, grLen));
    }

    Tcl_ObjSetVar2(interp, objv[2], NULL, Tcl_NewWideIntObj(strIndex-grLen), 0);
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeLengthCmd --
 *
 *	Implements the Tcl "grapheme length" command. Stores the number
 *	of graphemes in the passed string into the interpreter result.
 *
 * Results:
 *	A standard Tcl result code.
 *
 * Side effects:
 *	Modifies interpreter result.
 *
 *------------------------------------------------------------------------
 */
static int
GraphemeLengthCmd (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,	/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "string");
	return TCL_ERROR;
    }

    Tcl_Size uniLen;
    Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(GraphemeLength(uniPtr, uniLen)));
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeIndexCmd --
 *
 *	Implements the Tcl "grapheme index" command. Stores the grapheme
 *	at the specified grapheme index into the interpreter result.
 *
 * Results:
 *	A standard Tcl result code.
 *
 * Side effects:
 *	Modifies interpreter result.
 *
 *------------------------------------------------------------------------
 */
static int
GraphemeIndexCmd (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,	/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "string grIndex");
	return TCL_ERROR;
    }

    Tcl_Size grIndex, grLen, uniLen;
    Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
    grLen = GraphemeLength(uniPtr, uniLen); /* Need for end, end-1 etc. */
    if (TclGetIntForIndexM(interp, objv[2], grLen - 1, &grIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    if (grIndex < 0 || grIndex >= grLen) {
	return TCL_OK;
    }
    Tcl_Size grWidth;
    const Tcl_UniChar *grPtr = GraphemeIndex(uniPtr, uniLen, grIndex, &grWidth);
    assert(grPtr);	/* Since we already checked length above */
    Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(grPtr, grWidth));

    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeRangeCmd --
 *
 *	Implements the Tcl "grapheme range" command. Stores the graphemes
 *	in at the specified grapheme index range into the interpreter result.
 *
 * Results:
 *	A standard Tcl result code.
 *
 * Side effects:
 *	Modifies interpreter result.
 *
 *------------------------------------------------------------------------
 */
static int
GraphemeRangeCmd (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,	/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "string grFirst grLast");
	return TCL_ERROR;
    }

    Tcl_Size grFirst, grLast, grLen, uniLen;
    Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
    grLen = GraphemeLength(uniPtr, uniLen); /* Need for end, end-1 etc. */
    if (TclGetIntForIndexM(interp, objv[2], grLen-1, &grFirst) != TCL_OK ||
	    TclGetIntForIndexM(interp, objv[3], grLen-1, &grLast) != TCL_OK) {
	return TCL_ERROR;
    }

    if (grFirst < 0) {
	grFirst = 0;
    }
    if (grLast >= grLen) {
	grLast = grLen - 1;
    }
    if (grFirst >= grLen || grLast < grFirst) {
	return TCL_OK;		/* Empty result */
    }

    const Tcl_UniChar *rangeFirstPtr;
    const Tcl_UniChar *rangeLastPtr;
    Tcl_Size lastWidth;
    Tcl_Obj *resultObj;
    rangeFirstPtr = GraphemeIndex(uniPtr, uniLen, grFirst, NULL);
    assert(rangeFirstPtr);
    rangeLastPtr = GraphemeIndex(rangeFirstPtr,
	    uniLen - (rangeFirstPtr - uniPtr), grLast - grFirst, &lastWidth);
    assert(rangeLastPtr);

    resultObj = Tcl_NewUnicodeObj(rangeFirstPtr,
			rangeLastPtr - rangeFirstPtr + lastWidth);
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeReverseCmd --
 *
 *	Implements the Tcl "grapheme reverse" command. Stores the graphemes
 *	in reverse order into the interpreter result.
 *
 * Results:
 *	A standard Tcl result code.
 *
 * Side effects:
 *	Modifies interpreter result.
 *
 *------------------------------------------------------------------------
 */
static int
GraphemeReverseCmd (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,	/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "string");
	return TCL_ERROR;
    }

    /*
     * Graphemes can vary in size with no theoritical limit so reversing in
     * place in unshared object case is not straightforward. Just allocate a
     * new object.
     *
     * TODO - use Tcl unicode string internals rather than appending
     * one at a time with a temporary buffer.
     */

    Tcl_Size uniLen;
    const Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
    if (uniLen == 0) {
	return TCL_OK;
    }
    const Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
    Tcl_UniChar *tempBufPtr
	    = (Tcl_UniChar *)Tcl_Alloc(uniLen * sizeof(*tempBufPtr));
    Tcl_UniChar *tempPtr = tempBufPtr + uniLen;
    for (const Tcl_UniChar *grPtr = uniPtr; uniPtr < uniEndPtr; ) {
	Tcl_Size grWidth;
	uniPtr = GraphemeNext(uniPtr, uniEndPtr - uniPtr);
	grWidth = uniPtr - grPtr;
	tempPtr -= grWidth;
	assert(tempPtr >= tempBufPtr);
	memcpy(tempPtr, grPtr, sizeof(Tcl_UniChar) * grWidth);
	grPtr = uniPtr;
    }
    assert(tempPtr == tempBufPtr);
    Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(tempBufPtr, uniLen));
    Tcl_Free(tempBufPtr);
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeListRepNew --
 *
 *	Allocates a new GraphemeListRep internal representation for the
 *	graphemeList type. The returned GraphemeListRep has a reference
 *	count of 0.
 *
 * Results:
 *	Pointer to the allocated GraphemeListRep.
 *
 *------------------------------------------------------------------------
 */
static inline GraphemeListRep *
GraphemeListRepNew(
    Tcl_Obj *objPtr)		/* Target Tcl_Obj containing graphemes */
{
    GraphemeListRep *glrPtr = (GraphemeListRep *) Tcl_Alloc(sizeof(*glrPtr));
    glrPtr->objPtr = objPtr;
    Tcl_IncrRefCount(objPtr);
    glrPtr->refCount = 0;
    glrPtr->graphemeIndex = 0;
    glrPtr->graphemeOffset = 0;
    return glrPtr;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeListRepUnref --
 *
 *	Decrements the reference count of a GraphemeListRep, freeing
 *	it if not positive.
 *
 * Results:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static inline void
GraphemeListRepDecrRefs(GraphemeListRep *glrPtr)
{
    if (glrPtr->refCount <= 1) {
	if (glrPtr->objPtr != NULL) {
	    Tcl_DecrRefCount(glrPtr->objPtr);
	}
	Tcl_Free(glrPtr);
    } else {
	glrPtr->refCount--;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GraphemeListFreeProc --
 *
 *      Frees resources associated with a GraphemeList object's
 *      internal representation.
 *
 * Results:
 *	None
 *
 *----------------------------------------------------------------------
 */

static void
GraphemeListFreeProc(
    Tcl_Obj *objPtr)
{
    GraphemeListRepDecrRefs(
	    (GraphemeListRep *)objPtr->internalRep.otherValuePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * GraphemeListDupProc --
 *
 *	Initialize the internal representation of a new Tcl_Obj to a copy of
 *	the internal representation of an existing GraphemeList object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	copyPtr's internal rep is set to a copy of srcPtr's internal
 *	representation.
 *
 *----------------------------------------------------------------------
 */
static void
GraphemeListDupProc(
    Tcl_Obj *srcPtr,		/* Object with internal rep to copy. Must have
				 * an internal rep of type "graphemeList". */
    Tcl_Obj *copyPtr)		/* Object with internal rep to set. Must not
				 * currently have an internal rep.*/
{
    copyPtr->internalRep.otherValuePtr = srcPtr->internalRep.otherValuePtr;
    ((GraphemeListRep *)srcPtr->internalRep.otherValuePtr)->refCount++;
    copyPtr->typePtr = srcPtr->typePtr;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeListLengthProc --
 *
 *	Returns the number of elements in a GraphemeList.
 *
 * Results:
 *	Count of graphemes.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
Tcl_Size
GraphemeListLengthProc (
    Tcl_Obj *objPtr
)
{
    GraphemeListRep *glrPtr = (GraphemeListRep *) objPtr->internalRep.otherValuePtr;
    Tcl_Size uniLen;
    Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(glrPtr->objPtr, &uniLen);
    return GraphemeLength(uniPtr, uniLen);
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeListIndexProc --
 *
 *	Retrieve the grapheme at the specified index.
 *
 * Results:
 *	Pointer to the Tcl_Obj containing the grapheme.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
int
GraphemeListIndexProc (
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Obj *objPtr,		/* Source list */
    Tcl_Size index,		/* Element index */
    Tcl_Obj **elemPtrPtr)	/* Returned element */
{
    GraphemeListRep *glrPtr = (GraphemeListRep *) objPtr->internalRep.otherValuePtr;
    Tcl_Size uniLen;
    const Tcl_UniChar *uniStartPtr = Tcl_GetUnicodeFromObj(glrPtr->objPtr, &uniLen);
    const Tcl_UniChar *grPtr = NULL;
    Tcl_Size grWidth = 0;

    /*
     * We will use the cached index or start from 0 depending on
     * - the requested index is greater than the cached index - use the cache
     * - the requested index is smaller than the cached index - use the cache
     *   iff (index > 2*(cachedIndex-index)). This is because moving backwards
     *   involves both backward and forward scans and is therefore slower.
     */
    assert(glrPtr->graphemeOffset <= uniLen);
    if (index >= glrPtr->graphemeIndex) {
	/* Move forward from the cached index */
	grPtr = GraphemeIndex(glrPtr->graphemeOffset + uniStartPtr,
		uniLen - glrPtr->graphemeOffset, index - glrPtr->graphemeIndex,
		&grWidth);
    } else if (index > 2*(glrPtr->graphemeIndex - index)) {
	/* index is sufficiently closer to cached index */
	assert(glrPtr->graphemeOffset > 0);
	assert(glrPtr->graphemeOffset <= uniLen);
	const Tcl_UniChar *uniEndPtr = uniStartPtr + glrPtr->graphemeOffset;
	Tcl_Size currentIndex = glrPtr->graphemeIndex;
	while (uniEndPtr > uniStartPtr) {
	    const Tcl_UniChar *uniPtr;
	    uniPtr = GraphemePrev(uniEndPtr, uniEndPtr - uniStartPtr);
	    if (--currentIndex == index) {
		grPtr = uniPtr;
		grWidth = uniEndPtr - uniPtr;
		break;
	    }
	    uniEndPtr = uniPtr;
	}
    } else {
	/* Start from the beginning */
	grPtr = GraphemeIndex(uniStartPtr, uniLen, index, &grWidth);
    }
    if (grPtr != NULL) {
	/* Update the cached values */
	assert(index >= 0);
	assert(grPtr >= uniStartPtr);
	assert((grPtr - uniStartPtr) < uniLen);
	glrPtr->graphemeIndex = index;
	glrPtr->graphemeOffset = grPtr - uniStartPtr;
	*elemPtrPtr = grWidth ? Tcl_NewUnicodeObj(grPtr, grWidth) : NULL;
    } else {
	/* Index out of bounds */
	*elemPtrPtr = NULL;
    }
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeSplitCmd --
 *
 *	Implements the Tcl command "grapheme split". Returns a list object
 *	each element of which is a single grapheme of the string passed
 *	in objv[1].
 *
 * Results:
 *	TCL_OK    - Success.
 *	TCL_ERROR - Error.
 *
 * Side effects:
 *	Interpreter result holds result or error message.
 *
 *------------------------------------------------------------------------
 */
int
GraphemeSplitCmd (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "string");
	return TCL_ERROR;
    }

    Tcl_Obj *objPtr;
    TclNewObj(objPtr);
    objPtr->typePtr = &tclGraphemeListType;
    GraphemeListRep *glrPtr = GraphemeListRepNew(objv[1]);
    glrPtr->refCount = 1;
    objPtr->internalRep.otherValuePtr = glrPtr;
    Tcl_InvalidateStringRep(objPtr);
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
