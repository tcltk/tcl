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
static Tcl_DupInternalRepProc	DupGraphemeListInternalRep;
static Tcl_FreeInternalRepProc	FreeGraphemeListInternalRep;
static Tcl_SetFromAnyProc	SetGraphemeListFromAny;
static Tcl_UpdateStringProc	UpdateStringOfGraphemeList;

static Tcl_UniChar *GraphemeNext(const Tcl_UniChar *uniPtr, Tcl_Size uniLen);

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
#if 0
    {"split",	GraphemeSplitCmd,	TclCompileGraphemeSplitCmd, NULL, NULL, 0},
#endif
    {NULL, NULL, NULL, NULL, NULL, 0}
};

/*
 * Graphemes have variable width (number of code points in the grapheme) and
 * no theoretical limit on it. To be able to iterate or locate the grapheme
 * a grapheme by its index (its *grapheme* index, not its Tcl_UniChar index)
 * requires a linear scan. To avoid repeated scans, we keep a lookup
 * structure making use of the fact that most graphemes will have a width of
 * one. So we use a run-length encoding (RLE) scheme where the GraphemeSegment
 * structure stores information about one grapheme run where each grapheme
 * in the run has the same width. The GraphemeOffsetMap is a table of these
 * structures that can then be searched to locate the offset for a grapheme
 * index.
 */
typedef struct GraphemeSegment {
    Tcl_Size width;		/* Width of each grapheme in the segment */
    Tcl_Size length;		/* Number of graphemes in this segment */
    Tcl_Size graphemeOffset;	/* Offset in graphemes of this segment */
    Tcl_Size uniOffset;		/* Offset in Tcl_UniChar's of this segment */
} GraphemeSegment;

typedef struct GraphemeOffsetMap {
    Tcl_Size capacity;				/* Number allocated */
    Tcl_Size used;				/* Count of segments in-use */
    Tcl_Size numGraphemes;			/* Total count of graphemes */
    GraphemeSegment segments[TCLFLEXARRAY];	/* Grapheme segment table */
} GraphemeOffsetMap;

/* Memory size needed for a GraphemeOffsetMap to hold numSegs_ segments */
#define GRAPHEME_OFFSET_MAP_SIZE(numSegs_)            \
    ((Tcl_Size)(offsetof(GraphemeOffsetMap, segments) \
		+ ((numSegs_) * sizeof(GraphemeSegment))))
/* Max number of segments possible in a map */
#define GRAPHEME_SEGMENT_MAX \
    ((Tcl_Size)(((size_t)TCL_SIZE_MAX - offsetof(GraphemeOffsetMap, segments))	\
	    / sizeof(GraphemeSegment)))

/*
 * The structure below defines the grapheme list object type by means of
 * functions that can be invoked by generic object code.
 */
#if 0
const Tcl_ObjType tclGraphemeListType = {
    "graphemeList",
    FreeGraphemeListInternalRep,
    DupGraphemeListInternalRep,
    UpdateStringOfGraphemeList,
    SetGraphemeListFromAny,
    TCL_OBJTYPE_V0
};
#endif

/*
 *------------------------------------------------------------------------
 *
 * GrowGraphemeOffsetMap --
 *
 *      Reallocates a GraphemeOffsetMap to a new size. Panics on failure. If
 *      increment is positive, capacity is increased by exactly that amount.
 *      Otherwise, capacity is grown heuristically but at least by one.
 *
 * Results:
 *	Pointer to reallocated GraphemeOffsetMap.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static GraphemeOffsetMap *
GrowGraphemeOffsetMap (
    GraphemeOffsetMap *mapPtr,	/* Allocation to resize */
    Tcl_Size increment		/* Capacity increment. If <= 0, heuristic */
)
{
    Tcl_Size newCapacity;
    if (increment > 0) {
	if (GRAPHEME_SEGMENT_MAX - mapPtr->capacity < increment) {
	    goto nomemory;
	}
	newCapacity = mapPtr->capacity + increment;
	mapPtr = (GraphemeOffsetMap *)Tcl_Realloc(mapPtr,
		GRAPHEME_OFFSET_MAP_SIZE(newCapacity));
    } else {
	if (mapPtr->capacity == (GRAPHEME_SEGMENT_MAX-1)) {
	    goto nomemory;
	}
	mapPtr = TclReallocElemsEx(mapPtr, mapPtr->capacity + 1,
		sizeof(GraphemeSegment), offsetof(GraphemeOffsetMap, segments),
		&newCapacity);
    }
    mapPtr->capacity = newCapacity;
    return mapPtr;

nomemory:
    Tcl_Panic("Max capacity of grapheme map exceeded.");
    return NULL;
}

/*
 *------------------------------------------------------------------------
 *
 * CreateGraphemeOffsetMap --
 *
 *	Allocates an returns a lookup table mapping grapheme indices to
 *
 *
 * Results:
 *	Pointer to an allocated grapheme lookup table.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static GraphemeOffsetMap *
CreateGraphemeOffsetMap (
    Tcl_UniChar *uniPtr,	/* Tcl_UniChar string */
    Tcl_Size uniLen)		/* Number of Tcl_UniChar's in string. -1 if
				 * nul terminated */
{
    GraphemeOffsetMap *mapPtr;
    mapPtr = (GraphemeOffsetMap *)Tcl_Alloc(GRAPHEME_OFFSET_MAP_SIZE(8));
    mapPtr->capacity = 8;
    mapPtr->used = 0;
    mapPtr->numGraphemes = 0;

    if (uniLen < 0) {
	uniLen = Tcl_UniCharLen(uniPtr);
    }
    if (uniLen == 0) {
	return mapPtr;
    }

    const Tcl_UniChar *endPtr = uniPtr + uniLen;

    /* Handling first grapheme separately avoids a check in loop */
    const Tcl_UniChar *grPtr = GraphemeNext(uniPtr, uniLen);
    assert(grPtr > uniPtr); /* Since we checked for 0 length above */

    GraphemeSegment segment = {grPtr - uniPtr, 0, 0, 0};

    while (grPtr < endPtr) {
	assert(segment.length > 0); /* already seen a grapheme in segment */
	const Tcl_UniChar *nextGrPtr = GraphemeNext(grPtr, endPtr - grPtr);
	assert(nextGrPtr > grPtr);
	Tcl_Size grWidth = nextGrPtr - grPtr;
	if (grWidth == segment.width) {
	    /* Grapheme same width as current run */
	    segment.length++;
	} else {
	    /*
	     * Grapheme has a different width than the current run. Store the
	     * current run length segment and start a new one.
	     */
	    if (mapPtr->used == mapPtr->capacity) {
		mapPtr = GrowGraphemeOffsetMap(mapPtr, 0);
	    }
	    GraphemeSegment *segmentPtr = &mapPtr->segments[mapPtr->used];
	    /* Store the just completed segment and init the next one */
	    mapPtr->segments[mapPtr->used] = segment;

	    /* Initialize for next segment */
	    segment.graphemeOffset += segment.length;
	    segment.uniOffset += segment.length * segment.width;
	    segment.length = 1;
	    segment.width = grWidth;

	    mapPtr->used++;
	}
	grPtr = nextGrPtr;
    }

    /* Store final segment */
    if (mapPtr->used == mapPtr->capacity) {
	mapPtr = GrowGraphemeOffsetMap(mapPtr, 1);
    }
    mapPtr->segments[mapPtr->used] = segment;
    mapPtr->numGraphemes = segment.graphemeOffset + segment.length;
    mapPtr->used++;

    return mapPtr;
}

/*
 *------------------------------------------------------------------------
 *
 * LookupGraphemeOffsetMap --
 *
 *	Given a grapheme index, looks up the corresponding Tcl_UniChar
 *	index in the passed grapheme offset map.
 *
 * Results:
 *	Returns the starting offset of the grapheme or -1 if the index
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static Tcl_Size
LookupGraphemeOffsetMap(
    const GraphemeOffsetMap *mapPtr,	/* Grapheme offset map */
    Tcl_Size grIndex)			/* Grapheme index to look up */
{

    if (grIndex < 0 || grIndex >= mapPtr->numGraphemes) {
	return -1;
    }

    Tcl_Size low, high;
    low = 0;
    high = mapPtr->used - 1;
    while (low < high) {
	Tcl_Size mid = (low + high + 1) / 2;
	if (mapPtr->segments[mid].graphemeOffset <= grIndex) {
	    low = mid;
	} else {
	    high = mid - 1;
	}
    }

    /* low identifies the segment in which the grapheme index falls */
    Tcl_Size grIndexInSegment = grIndex - mapPtr->segments[low].graphemeOffset;
    return mapPtr->segments[low].uniOffset
	 + (grIndexInSegment * mapPtr->segments[low].width);
}

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
 *	1 if the code point is postcore, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static inline
IsPossibleContinuation(
    Tcl_UniChar cp) /* Unicode code point */
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
	return 1;
    default:
	return 0;
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
 *	1 if the code point is precore, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static inline
IsPossiblePrefix (
    Tcl_UniChar cp)		/* Unicode code point */
{
    utf8proc_property_t *propPtr;
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
static Tcl_UniChar *
GraphemeNext(
    const Tcl_UniChar *uniPtr,	/* Start of a grapheme */
    Tcl_Size uniLen)		/* Number of Tcl_UniChar's, > 0! */
{
    assert(uniLen > 0);
    Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
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
    Tcl_UniChar *uniPtr,	/* Tcl_UniChar string */
    Tcl_Size uniLen)		/* Number of Tcl_UniChar's in string. -1 if
				 * nul terminated */
{
    if (uniLen < 0) {
	uniLen = Tcl_UniCharLen(uniPtr);
    }
    Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
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
static Tcl_UniChar *
GraphemeIndex(
    Tcl_UniChar *uniPtr,	/* Tcl_UniChar string */
    Tcl_Size uniLen,		/* Number of Tcl_UniChar's in string. -1 if
				 * nul terminated */
    Tcl_Size grIndex,		/* Index of desired grapheme */
    Tcl_Size *grWidthPtr)	/* Length of grapheme */
{
    if (grIndex >= 0) {
	if (uniLen < 0) {
	    uniLen = Tcl_UniCharLen(uniPtr);
	}
	Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
	Tcl_UniChar *grPtr;
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
    Tcl_Interp *interp,		/* Current interpreter. */
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
	int state = 0;
	Tcl_UniChar *grStartPtr = uniStartPtr + strIndex;
	Tcl_UniChar *grEndPtr = GraphemeNext(grStartPtr, uniLen - strIndex);
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
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "string strIndex");
	return TCL_ERROR;
    }

    Tcl_Size uniLen;
    Tcl_UniChar *uniStartPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
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
    Tcl_UniChar *uniEndPtr = uniStartPtr + strIndex;
    Tcl_UniChar *uniPtr = uniEndPtr - 1;

    while (uniPtr > uniStartPtr
	    && ((*uniPtr == '\n' && uniPtr[-1] == '\r') ||
		IsPossibleContinuation(*uniPtr) || IsPossiblePrefix(uniPtr[-1]))) {
	uniPtr--;
    }

    /* Now scan forward. Note uniPtr may be < uniStartPtr if uniLen was 0 */
    Tcl_Size grLen = 0;
    int state = 0;
    if (uniPtr >= uniStartPtr) {
	Tcl_UniChar *grStartPtr;
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
    Tcl_Interp *interp,		/* Current interpreter. */
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
    Tcl_Interp *interp,		/* Current interpreter. */
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
    Tcl_UniChar *grPtr = GraphemeIndex(uniPtr, uniLen, grIndex, &grWidth);
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
    Tcl_Interp *interp,		/* Current interpreter. */
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

    Tcl_UniChar *rangeFirstPtr;
    Tcl_UniChar *rangeLastPtr;
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
    Tcl_Interp *interp,		/* Current interpreter. */
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
    Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
    if (uniLen == 0) {
	return TCL_OK;
    }
    Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
    Tcl_UniChar *tempBufPtr = Tcl_Alloc(uniLen * sizeof(*tempBufPtr));
    Tcl_UniChar *tempPtr = tempBufPtr + uniLen;
    Tcl_Obj *resultObj;
    TclNewObj(resultObj);
    for (Tcl_UniChar *grPtr = uniPtr; uniPtr < uniEndPtr; ) {
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
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
