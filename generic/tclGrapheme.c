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
#include "tclStringRep.h"	/* Need to check if Unicode rep exists  */

/* Specify relative path to ignore system include */
#include "../utf8proc/utf8proc.h"

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
static Tcl_ObjCmdProc2		GraphemeOffsetCmd;
static Tcl_ObjCmdProc2		GraphemePrevCmd;
static Tcl_ObjCmdProc2		GraphemeRangeCmd;
static Tcl_ObjCmdProc2		GraphemeReverseCmd;
static Tcl_ObjCmdProc2		GraphemeSplitCmd;

/*
 * Table of grapheme subcommand names and implementations.
 */

const EnsembleImplMap tclGraphemeImplMap[] = {
    {"index",	GraphemeIndexCmd,	NULL, NULL, NULL, 0},
    {"length",	GraphemeLengthCmd,	NULL, NULL, NULL, 0},
    {"next",	GraphemeNextCmd,	NULL, NULL, NULL, 0},
    {"offset",	GraphemeOffsetCmd,	NULL, NULL, NULL, 0},
    {"prev",	GraphemePrevCmd,	NULL, NULL, NULL, 0},
    {"range",	GraphemeRangeCmd,	NULL, NULL, NULL, 0},
    {"reverse",	GraphemeReverseCmd,	NULL, NULL, NULL, 0},
    {"split",	GraphemeSplitCmd,	NULL, NULL, NULL, 0},
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
    Tcl_Size graphemeIndex;	/* Cached grapheme index */
    Tcl_Size graphemeOffset;	/* Corresponding Tcl_UniChar offset */
} GraphemeListRep;

static Tcl_DupInternalRepProc	GraphemeListDupProc;
static Tcl_FreeInternalRepProc	GraphemeListFreeProc;
static Tcl_ObjTypeLengthProc	GraphemeListLengthProc;
static Tcl_ObjTypeIndexProc	GraphemeListIndexProc;
static const Tcl_ObjType tclGraphemeListType = {
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
 * UseUniChars --
 *
 *	Determines whether to use the Tcl_UniChar based grapheme
 *	implementation or the TUTF-8 based one. Primary purpose is to avoid
 *	shimmering and generation of multiple representations.
 *
 * Results:
 *	A true if the Tcl_UniChar implementation is to be preferred, false
 *	for TUTF8.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static inline bool
UseUniChars(Tcl_Obj *objPtr)
{
    if (TclHasInternalRep(objPtr, &tclStringType)) {
	String *stringPtr = GET_STRING(objPtr);
	return stringPtr->hasUnicode != 0;
    }
    return false;
}

/*
 *------------------------------------------------------------------------
 *
 * PrevCodePoint --
 *
 * Results:
 *	Returns the start of the code point preceding endPtr.
 *
 * Side effects:
 *	Stores the code point in *ucPtr
 *
 *------------------------------------------------------------------------
 */
static const char *
PrevCodePoint(
    const char *endPtr,	/* End of TUTF8 code point */
    const char *startPtr,   /* Beginning of TUTF8 byte sequence */
    utf8proc_int32_t *ucPtr)
{
    assert(endPtr > startPtr);
    const char *prevPtr;
    prevPtr = Tcl_UtfPrev(endPtr, startPtr);

    if (utf8proc_iterate((utf8proc_uint8_t *)prevPtr, endPtr - prevPtr, ucPtr)
	    < 0) {
	if (UCHAR(prevPtr[0]) == 0xC0 && (endPtr - prevPtr) == 2
		&& UCHAR(prevPtr[1]) == 0x80) {
	    /* Tcl's nul representation */
	    *ucPtr = 0;
	} else {
	    /* Invalid encoding */
	    *ucPtr = -1;
	}
    }
    return prevPtr;
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
 *	true if the code point is postcore, false otherwise.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
static inline bool
IsPossibleContinuation(
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
IsPossiblePrefix(
    Tcl_UniChar cp)		/* Unicode code point */
{
    const utf8proc_property_t *propPtr = utf8proc_get_property(cp);

    return propPtr->boundclass == UTF8PROC_BOUNDCLASS_PREPEND
	|| propPtr->boundclass == UTF8PROC_BOUNDCLASS_ZWJ
	|| propPtr->boundclass == UTF8PROC_BOUNDCLASS_REGIONAL_INDICATOR
	|| propPtr->category == UTF8PROC_CATEGORY_MN
	|| propPtr->category == UTF8PROC_CATEGORY_MC
	|| propPtr->category == UTF8PROC_CATEGORY_ME
	|| propPtr->indic_conjunct_break == UTF8PROC_INDIC_CONJUNCT_BREAK_LINKER
	|| propPtr->indic_conjunct_break == UTF8PROC_INDIC_CONJUNCT_BREAK_EXTEND
	;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeNext, GraphemeUniCharNext --
 *
 *	Returns the start of the next grapheme. numBytes/uniLen must be > 0
 *
 * Results:
 *	Pointer to start of next grapheme.
 *
 * Side effects:
 *	Stores number of code points in the grapheme in *grWidthPtr.
 *
 *------------------------------------------------------------------------
 */
static char *
GraphemeNext(
    const char *tutf8Ptr,	/* Start of a grapheme */
    Tcl_Size numBytes,		/* Number of bytes, > 0! */
    Tcl_Size *grWidthPtr)	/* If not NULL, location to store grapheme
	 			 * width in code point units */
{
    assert(numBytes > 0);
    utf8proc_uint8_t *grPtr = (utf8proc_uint8_t *)tutf8Ptr;
    const utf8proc_uint8_t *endPtr = numBytes + grPtr;
    int state = 0;
    utf8proc_ssize_t utf8len;
    utf8proc_int32_t ucPre;
    utf8len = utf8proc_iterate(grPtr, endPtr - grPtr, &ucPre);
    if (utf8len < 0) {
	/*
	 * Invalid encoded sequence. Treat byte as individual grapheme except
	 * the special C080 sequence which is Tcl's internal \0 encoding.
	 */
	if (*grPtr++ == 0xC0 && grPtr < endPtr && grPtr[0] == 0x80) {
	    ++grPtr;
	}
	if (grWidthPtr) {
	    *grWidthPtr = 1;
	}
	return (char *) grPtr;
    }
    grPtr += utf8len;
    Tcl_Size grWidth = 1;
    while (grPtr < endPtr) {
	utf8proc_int32_t ucPost;
	utf8len = utf8proc_iterate(grPtr, endPtr - grPtr, &ucPost);
        if (utf8len < 0) {
	    /*
	     * Either invalid encoded sequence or C080. Either way
	     * terminates grapheme cluster
	     */
	    break;
	}
	if (utf8proc_grapheme_break_stateful(ucPre, ucPost, &state)) {
	    break;
	}
	++grWidth;
	grPtr += utf8len;
	ucPre = ucPost;
    }
    if (grWidthPtr) {
	*grWidthPtr = grWidth;
    }
    return (char *) grPtr;
}
static Tcl_UniChar *
GraphemeUniCharNext(
    const Tcl_UniChar *uniPtr,	/* Start of a grapheme */
    Tcl_Size uniLen,		/* Number of Tcl_UniChar's, > 0! */
    Tcl_Size *grWidthPtr)	/* If not NULL, location to store grapheme
	 			 * width in code point units */
{
    assert(uniLen > 0);
    const Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
    int state = 0;
    Tcl_Size grWidth = 1;
    while (++uniPtr < uniEndPtr) {
	if (utf8proc_grapheme_break_stateful(uniPtr[-1], uniPtr[0], &state)) {
	    break;
	}
	++grWidth;
    }
    if (grWidthPtr) {
	*grWidthPtr = grWidth;
    }
    return (Tcl_UniChar *) uniPtr;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemePrev, GraphemeUniCharPrev --
 *
 *	Returns the start of the previous grapheme. numBytes/uniLen must be >0
 *
 * Results:
 *	Pointer to start of previous grapheme.
 *
 * Side effects:
 *	Stores number of code points in the grapheme in *grWidthPtr.
 *
 *------------------------------------------------------------------------
 */
static char *
GraphemePrev(
    const char *endPtr,	/* Points beyond byte sequence */
    Tcl_Size numBytes,		/* Number of preceding valid bytes
				 * must be > 0! */
    Tcl_Size *grWidthPtr)	/* If not NULL, location to store grapheme
	 			 * width in code point units */
{
    assert(numBytes > 0);

    /*
     * The utf8proc grapheme functions only maintain correct state when
     * traversing in the forward direction. We have to therefore first go
     * back to a place that is guaranteed to not be in the middle of a
     * grapheme and work forward from there. The reason for having to work
     * forward is that the backward scan is a conservative heuristic and
     * may go further back than it needs to. Therefore we have to work
     * forward again to skip over the extra scan.
     */
    const char *startPtr = endPtr - numBytes;
    const char *grPtr = endPtr;
    utf8proc_int32_t uc = -1; /* Code point at grPtr, -1 if invalid */

    assert(grPtr > startPtr);	/* Since numBytes > 0 assert above */
    grPtr = PrevCodePoint(grPtr, startPtr, &uc);
    while (uc >= 0 && grPtr > startPtr) {
	utf8proc_int32_t prevUc;
	const char *prevPtr
		= PrevCodePoint(grPtr, startPtr, &prevUc);

	/*
	 * Stop going back only if all following conditions met
	 *   - current code point is not a continuation code point
	 *   - current code point is not the LF in a CRLF pair
	 *   - the preceding code point is not a prefix
	 */
	if (!IsPossibleContinuation(uc) && !(uc == '\n' && prevUc == '\r')
		&& !(prevUc >= 0 && IsPossiblePrefix(prevUc))) {
	    break;
	}
	grPtr = prevPtr;
	uc = prevUc;
    }

    /* Now scan forward. Note uniPtr may be < uniStartPtr if uniLen was 0 */
    const char *nextPtr = (char *) grPtr;
    assert(endPtr > grPtr);
    Tcl_Size grWidth;
    do {
	grPtr = nextPtr;
	nextPtr = GraphemeNext(nextPtr, endPtr - nextPtr, &grWidth);
    } while (nextPtr < endPtr);

    if (grWidthPtr) {
	*grWidthPtr = grWidth;
    }
    return (char *) grPtr;
}

static Tcl_UniChar *
GraphemeUniCharPrev(
    const Tcl_UniChar *endPtr,	/* Points beyond grapheme to return */
    Tcl_Size uniLen,		/* Number of preceding Tcl_UniChar's,
				 * must be > 0! */
    Tcl_Size *grWidthPtr)	/* If not NULL, location to store grapheme
	 			 * width in code point units */
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
    const Tcl_UniChar *uniStartPtr = endPtr - uniLen;
    const Tcl_UniChar *uniPtr = endPtr - 1;
    /*
     * Keep going back if any of following conditions met
     *   - current code point is the LF in a CRLF pair
     *   - current code point is a continuation code point
     *   - the preceding code point is a prefix
     */

    while (uniPtr > uniStartPtr
	    && ((*uniPtr == '\n' && uniPtr[-1] == '\r')
		|| IsPossibleContinuation(*uniPtr)
		|| IsPossiblePrefix(uniPtr[-1]))) {
	uniPtr--;
    }

    /* Now scan forward. */
    const Tcl_UniChar *grPtr;
    assert(endPtr > uniPtr);
    Tcl_Size grWidth;
    do {
	grPtr = uniPtr;
	uniPtr = GraphemeUniCharNext(uniPtr, endPtr - uniPtr, &grWidth);
    } while (uniPtr < endPtr);

    if (grWidthPtr) {
	*grWidthPtr = grWidth;
    }
    return (Tcl_UniChar *) grPtr;
}

/*
 *------------------------------------------------------------------------
 *
 * Tcl_GraphemeLength, Tcl_GraphemeUniCharLength --
 *
 *	Returns the number of graphemes
 *
 * Results:
 *	Count of graphemes in the sequence.
 *
 * Side effects:
 *	None.
 *
 *------------------------------------------------------------------------
 */
Tcl_Size
Tcl_GraphemeLength(
    const char *tutf8Ptr,	/* TUTF8 sequence */
    Tcl_Size numBytes)		/* Number of bytes in sequence.
				 * If < 0, nul terminated sequence */
{
    if (numBytes < 0) {
	numBytes = strlen(tutf8Ptr);
    }
    const char *endPtr = tutf8Ptr + numBytes;
    Tcl_Size count = 0;
    while (tutf8Ptr < endPtr) {
	tutf8Ptr = GraphemeNext(tutf8Ptr, endPtr - tutf8Ptr, NULL);
	++count;
    }
    return count;
}

Tcl_Size
Tcl_GraphemeUniCharLength(
    const Tcl_UniChar *uniPtr,	/* Tcl_UniChar sequence */
    Tcl_Size uniLen)		/* Number of Tcl_UniChar's in sequence.
				 * If < 0, nul terminated sequence */
{
    if (uniLen < 0) {
	uniLen = Tcl_UniCharLen(uniPtr);
    }
    const Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
    Tcl_Size count = 0;
    while (uniPtr < uniEndPtr) {
	uniPtr = GraphemeUniCharNext(uniPtr, uniEndPtr - uniPtr, NULL);
	++count;
    }
    return count;
}

/*
 *------------------------------------------------------------------------
 *
 * Tcl_GraphemeIndex, Tcl_GraphemeUniCharIndex --
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
char *
Tcl_GraphemeIndex(
    const char *tutf8Ptr,	/* Tcl_UniChar string */
    Tcl_Size numBytes,		/* Number of bytes in string. -1 if
				 * nul terminated */
    Tcl_Size grIndex,		/* Index of desired grapheme */
    Tcl_Size *grWidthPtr,	/* Location for grapheme width, may be NULL */
    Tcl_Size *grNumBytesPtr)	/* If not NULL, location to number of bytes
	 			 * comprising the grapheme*/
{
    if (grIndex >= 0) {
	if (numBytes < 0) {
	    numBytes = strlen(tutf8Ptr);
	}
	const char *endPtr = tutf8Ptr + numBytes;
	const char *grPtr;
	Tcl_Size i, grWidth;
	for (i = 0, grPtr = tutf8Ptr; grPtr < endPtr; ++i) {
	    tutf8Ptr = GraphemeNext(grPtr, endPtr - grPtr, &grWidth);
	    if (i == grIndex) {
		if (grWidthPtr != NULL) {
		    *grWidthPtr = grWidth;
		}
		if (grNumBytesPtr != NULL) {
		    *grNumBytesPtr = tutf8Ptr - grPtr;
		}
		return (char *) grPtr;
	    }
	    grPtr = tutf8Ptr;
	}
    }
    if (grWidthPtr != NULL) {
	*grWidthPtr = 0;
    }
    if (grNumBytesPtr) {
	*grNumBytesPtr = 0;
    }
    return NULL;
}

Tcl_UniChar *
Tcl_GraphemeUniCharIndex(
    const Tcl_UniChar *uniPtr,	/* Tcl_UniChar string */
    Tcl_Size uniLen,		/* Number of Tcl_UniChar's in string. -1 if
				 * nul terminated */
    Tcl_Size grIndex,		/* Index of desired grapheme */
    Tcl_Size *grWidthPtr,	/* Location for grapheme width, may be NULL */
    Tcl_Size *grNumBytesPtr)	/* If not NULL, location to number of bytes
	 			 * comprising the grapheme*/
{
    if (grIndex >= 0) {
	if (uniLen < 0) {
	    uniLen = Tcl_UniCharLen(uniPtr);
	}
	const Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
	const Tcl_UniChar *grPtr;
	Tcl_Size i, grWidth;
	for (i = 0, grPtr = uniPtr; grPtr < uniEndPtr; ++i) {
	    uniPtr = GraphemeUniCharNext(grPtr, uniEndPtr - grPtr, &grWidth);
	    if (i == grIndex) {
		if (grWidthPtr != NULL) {
		    *grWidthPtr = grWidth;
		}
		if (grNumBytesPtr) {
		    *grNumBytesPtr = sizeof(Tcl_UniChar) * grWidth;
		}
		return (Tcl_UniChar *) grPtr;
	    }
	    grPtr = uniPtr;
	}
    }
    if (grWidthPtr != NULL) {
	*grWidthPtr = 0;
    }
    if (grNumBytesPtr) {
	*grNumBytesPtr = 0;
    }
    return NULL;
}

/*
 *------------------------------------------------------------------------
 *
 * Tcl_GraphemeRange, Tcl_GraphemeUniCharRange --
 *
 *	Returns pointers delimiting a range of graphemes.
 *
 * Results:
 *	Pointer to the first grapheme in range. If the entire range is
 *	outside the sequence extent, returns pointer to location beyond
 *	the sequence.
 *
 * Side effects:
 *	Stores pointer to the location after the last grapheme in range
 *	in *endPtrPtr.
 *
 *------------------------------------------------------------------------
 */
char *
Tcl_GraphemeRange(
    const char *tutf8Ptr,	/* TUTF8 byte sequence */
    Tcl_Size numBytes,		/* Number of bytes in sequence. < 0 if
				 * nul terminated */
    Tcl_Size grFirst,		/* Starting index of grapheme range */
    Tcl_Size grLast,		/* Ending index of grapheme range */
    char **endPtrPtr)		/* Location to store pointer beyond range */
{
    if (numBytes < 0) {
	numBytes = strlen(tutf8Ptr);
    }
    if (grFirst < 0) {
	grFirst = 0;
    }
    /* Negative grLast -> end of string, for consistency with Tcl_GetRange */
    if (grLast < 0) {
	grLast = TCL_SIZE_MAX;
    }

    char *rangeFirstPtr;
    rangeFirstPtr
	    = Tcl_GraphemeIndex(tutf8Ptr, numBytes, grFirst, NULL, NULL);
    if (rangeFirstPtr == NULL) {
	/* Range lies outside passed sequence length */
	*endPtrPtr = numBytes + (char *) tutf8Ptr;
	return *endPtrPtr;
    }
    assert(rangeFirstPtr >= tutf8Ptr);
    assert((rangeFirstPtr - tutf8Ptr) < numBytes);
    if (grLast < grFirst) {
	*endPtrPtr = rangeFirstPtr;
	return rangeFirstPtr;
    }

    char *rangeLastPtr;
    Tcl_Size lastNumBytes;
    rangeLastPtr = Tcl_GraphemeIndex(rangeFirstPtr,
	    numBytes - (rangeFirstPtr - tutf8Ptr), grLast - grFirst, NULL,
	    &lastNumBytes);
    if (rangeLastPtr == NULL) {
	*endPtrPtr = numBytes + (char *) tutf8Ptr;
	return rangeFirstPtr;
    }

    *endPtrPtr = rangeLastPtr + lastNumBytes;
    return rangeFirstPtr;
}

Tcl_UniChar *
Tcl_GraphemeUniCharRange(
    const Tcl_UniChar *uniPtr,	/* Tcl_UniChar string */
    Tcl_Size uniLen,		/* Number of Tcl_UniChar's in string. -1 if
				 * nul terminated */
    Tcl_Size grFirst,		/* Starting index of grapheme range */
    Tcl_Size grLast,		/* Ending index of grapheme range */
    Tcl_UniChar **endPtrPtr)	/* Location to store pointer beyond range */
{
    if (uniLen < 0) {
	uniLen = Tcl_UniCharLen(uniPtr);
    }
    if (grFirst < 0) {
	grFirst = 0;
    }
    /* Negative grLast -> end of string, for consistency with Tcl_GetRange */
    if (grLast < 0) {
	grLast = TCL_SIZE_MAX;
    }

    Tcl_UniChar *rangeFirstPtr;
    rangeFirstPtr
	    = Tcl_GraphemeUniCharIndex(uniPtr, uniLen, grFirst, NULL, NULL);
    if (rangeFirstPtr == NULL) {
	/* Range lies outside passed sequence length */
	*endPtrPtr = uniLen + (Tcl_UniChar *) uniPtr;
	return *endPtrPtr;
    }
    assert(rangeFirstPtr >= uniPtr);
    assert((rangeFirstPtr - uniPtr) < uniLen);
    if (grLast < grFirst) {
	*endPtrPtr = rangeFirstPtr;
	return rangeFirstPtr;
    }

    Tcl_UniChar *rangeLastPtr;
    Tcl_Size lastWidth;
    rangeLastPtr = Tcl_GraphemeUniCharIndex(rangeFirstPtr,
	    uniLen - (rangeFirstPtr - uniPtr), grLast - grFirst, &lastWidth, NULL);
    if (rangeLastPtr == NULL) {
	*endPtrPtr = uniLen + (Tcl_UniChar *) uniPtr;
	return rangeFirstPtr;
    }

    *endPtrPtr = rangeLastPtr + lastWidth;
    return rangeFirstPtr;
}

/*
 *------------------------------------------------------------------------
 *
 * Tcl_GraphemeReverse, Tcl_GraphemeUniCharReverse --
 *
 *	Stores graphemes in reverse order into the output buffer.
 *
 * Results:
 *	Number of graphemes stored in output buffer or -1 if the
 *	the output buffer is not large enough.
 *
 * Side effects:
 *	Modifies the output buffer.
 *
 *------------------------------------------------------------------------
 */
Tcl_Size
Tcl_GraphemeReverse(
    const char *tutf8Ptr,	/* TUTF8 sequence */
    Tcl_Size numBytes,		/* Number of bytes in sequence. < 0 if
    				 * nul terminated */
    char *outPtr,		/* Output buffer */ 
    Tcl_Size outCapacity)	/* Size of output buffer in bytes */
{
    if (numBytes < 0) {
	numBytes = strlen(tutf8Ptr);
    }
    if (numBytes == 0) {
	return 0;
    }
    if (outCapacity < numBytes) {
	return -1;
    }
    const char *endPtr = tutf8Ptr + numBytes;
    char *toPtr = outPtr + numBytes;
    Tcl_Size grCount;
    for (grCount = 0; tutf8Ptr < endPtr; ++grCount) {
	const char *grPtr = GraphemeNext(tutf8Ptr, endPtr - tutf8Ptr, NULL);
	assert(tutf8Ptr < grPtr);
	Tcl_Size grNumBytes = grPtr - tutf8Ptr;
	toPtr -= grNumBytes;
	assert(toPtr >= outPtr);
	memcpy(toPtr, tutf8Ptr, grNumBytes);
	tutf8Ptr = grPtr;
    }
    assert(toPtr == outPtr);
    return grCount;
}

Tcl_Size
Tcl_GraphemeUniCharReverse(
    const Tcl_UniChar *uniPtr,	/* Tcl_UniChar string */
    Tcl_Size uniLen,		/* Number of Tcl_UniChar's in string. -1 if
    				 * nul terminated */
    Tcl_UniChar *outPtr,	/* Output buffer */
    Tcl_Size outCapacity)	/* Size of output buffer in Tcl_UniChar's */
{
    if (uniLen < 0) {
	uniLen = Tcl_UniCharLen(uniPtr);
    }
    if (uniLen == 0) {
	return 0;
    }
    if (outCapacity < uniLen) {
	return -1;
    }
    const Tcl_UniChar *uniEndPtr = uniPtr + uniLen;
    Tcl_UniChar *toPtr = outPtr + uniLen;
    Tcl_Size grCount = 0;
    for (const Tcl_UniChar *grPtr = uniPtr; uniPtr < uniEndPtr; ++grCount) {
	Tcl_Size grWidth;
	uniPtr = GraphemeUniCharNext(uniPtr, uniEndPtr - uniPtr, &grWidth);
	toPtr -= grWidth;
	assert(toPtr >= outPtr);
	memcpy(toPtr, grPtr, sizeof(Tcl_UniChar) * grWidth);
	grPtr = uniPtr;
    }
    assert(toPtr == outPtr);
    return grCount;
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
GraphemeNextCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "string indexVar");
	return TCL_ERROR;
    }

    Tcl_Size grWidth = 0;
    Tcl_Obj *indexObj = Tcl_ObjGetVar2(interp, objv[2], NULL, 0);
    Tcl_Size uniLen = Tcl_GetCharLength(objv[1]);
    Tcl_Size strIndex = 0;

    if (indexObj) {
	if (TclGetIntForIndexM(interp, indexObj, uniLen, &strIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (strIndex < 0) {
	    strIndex = 0;
	}
    }

    if (strIndex < uniLen) {
	if (UseUniChars(objv[1])) {
	    const Tcl_UniChar *grStartPtr
		    = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
	    grStartPtr += strIndex;
	    (void) GraphemeUniCharNext(grStartPtr, uniLen - strIndex, &grWidth);
	    Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(grStartPtr, grWidth));
	} else {
	    Tcl_Size numBytes;
	    const char *tutf8Ptr = TclGetStringFromObj(objv[1], &numBytes);
	    const char *grStartPtr = Tcl_UtfAtIndex(tutf8Ptr, strIndex);
	    Tcl_Size offset = grStartPtr - tutf8Ptr;
	    assert(numBytes > offset); /* Since strIndex < uniLen above */
	    const char *grEndPtr
		    = GraphemeNext(grStartPtr, numBytes - offset, &grWidth);
	    numBytes = grEndPtr - grStartPtr;
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(grStartPtr, numBytes));
	}
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
GraphemePrevCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "string indexVar");
	return TCL_ERROR;
    }

    Tcl_Size grWidth = 0;
    Tcl_Obj *indexObj = Tcl_ObjGetVar2(interp, objv[2], NULL, 0);
    Tcl_Size uniLen = Tcl_GetCharLength(objv[1]);
    Tcl_Size strIndex = uniLen;

    if (indexObj) {
	if (TclGetIntForIndexM(interp, indexObj, uniLen, &strIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (strIndex < 0) {
	    strIndex = 0;
	} else if (strIndex > uniLen) {
	    strIndex = uniLen;
	}
    }

    if (strIndex > 0) {
	if (UseUniChars(objv[1])) {
	    const Tcl_UniChar *startPtr;
	    startPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
	    Tcl_UniChar *grPtr = GraphemeUniCharPrev(startPtr + strIndex,
		    strIndex, &grWidth);
	    Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(grPtr, grWidth));
	} else {
	    Tcl_Size numBytes;
	    const char *startPtr = TclGetStringFromObj(objv[1], &numBytes);
	    const char *endPtr = Tcl_UtfAtIndex(startPtr, strIndex);
	    Tcl_Size offset = endPtr - startPtr;
	    assert(offset > 0); /* Since strIndex > 0 above */
	    const char *grStartPtr = GraphemePrev(endPtr, offset, &grWidth);
	    numBytes = endPtr - grStartPtr;
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(grStartPtr, numBytes));
	}
    }
    Tcl_ObjSetVar2(interp, objv[2], NULL, Tcl_NewWideIntObj(strIndex-grWidth), 0);
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
GraphemeLengthCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "string");
	return TCL_ERROR;
    }
    Tcl_Size len;
    if (UseUniChars(objv[1])) {
	Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(objv[1], &len);
	Tcl_SetObjResult(interp,
		Tcl_NewWideIntObj(Tcl_GraphemeUniCharLength(uniPtr, len)));
    } else {
	const char *tutf8Ptr;
	tutf8Ptr = TclGetStringFromObj(objv[1], &len);
	Tcl_SetObjResult(interp,
		Tcl_NewWideIntObj(Tcl_GraphemeLength(tutf8Ptr, len)));
    }

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
GraphemeIndexCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "string grIndex");
	return TCL_ERROR;
    }

    Tcl_Size grIndex, grLen;
    if (UseUniChars(objv[1])) {
	Tcl_Size grWidth;
	Tcl_Size uniLen;
	const Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
	grLen = Tcl_GraphemeUniCharLength(uniPtr,
		uniLen); /* Need for end, end-1 etc. */
	if (TclGetIntForIndexM(interp, objv[2], grLen - 1, &grIndex)
		!= TCL_OK) {
	    return TCL_ERROR;
	}

	if (grIndex < 0 || grIndex >= grLen) {
	    return TCL_OK;
	}
	const Tcl_UniChar *grPtr = Tcl_GraphemeUniCharIndex(uniPtr, uniLen,
		grIndex, &grWidth, NULL);
	assert(grPtr); /* Since we already checked length above */
	Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(grPtr, grWidth));
    } else {
	Tcl_Size numBytes;
	const char *tutf8Ptr = TclGetStringFromObj(objv[1], &numBytes);
	grLen = Tcl_GraphemeLength(tutf8Ptr, numBytes); /* for end-1 etc. */
	if (TclGetIntForIndexM(interp, objv[2], grLen - 1, &grIndex)
		!= TCL_OK) {
	    return TCL_ERROR;
	}

	if (grIndex < 0 || grIndex >= grLen) {
	    return TCL_OK;
	}
	const char *grPtr = Tcl_GraphemeIndex(tutf8Ptr, numBytes, grIndex, NULL,
		&numBytes);
	assert(grPtr); /* Since we already checked length above */
	Tcl_SetObjResult(interp, Tcl_NewStringObj(grPtr, numBytes));
    }

    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeOffsetCmd --
 *
 *	Implements the Tcl "grapheme offset" command. Stores the offset of
 *	the specified grapheme index into the interpreter result.
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
GraphemeOffsetCmd(
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
    grLen = Tcl_GraphemeUniCharLength(uniPtr, uniLen); /* Need for end, end-1 etc. */
    if (TclGetIntForIndexM(interp, objv[2], grLen - 1, &grIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    if (grIndex < 0 || grIndex >= grLen) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
    } else {
	const Tcl_UniChar *grPtr;
	grPtr = Tcl_GraphemeUniCharIndex(uniPtr, uniLen, grIndex, NULL, NULL);
	assert(grPtr); /* Since we already checked length above */
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(grPtr - uniPtr));
    }

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
GraphemeRangeCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "string grFirst grLast");
	return TCL_ERROR;
    }

    Tcl_Size grFirst, grLast, grLen;
    if (UseUniChars(objv[1])) {
	Tcl_Size uniLen;
	Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
	grLen = Tcl_GraphemeUniCharLength(uniPtr, uniLen);
	if (TclGetIntForIndexM(interp, objv[2], grLen - 1, &grFirst) != TCL_OK
		|| TclGetIntForIndexM(interp, objv[3], grLen - 1, &grLast)
			   != TCL_OK) {
	    return TCL_ERROR;
	}

	if (grLast >= 0) {
	    Tcl_UniChar *rangeFirstPtr;
	    Tcl_UniChar *rangeEndPtr;
	    rangeFirstPtr = Tcl_GraphemeUniCharRange(uniPtr, uniLen, grFirst,
		    grLast, &rangeEndPtr);
	    assert(rangeEndPtr >= rangeFirstPtr);
	    Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(rangeFirstPtr,
					     rangeEndPtr - rangeFirstPtr));
	}
    } else {
	Tcl_Size numBytes;
	const char *tutf8Ptr = TclGetStringFromObj(objv[1], &numBytes);
	grLen = Tcl_GraphemeLength(tutf8Ptr, numBytes);
	if (TclGetIntForIndexM(interp, objv[2], grLen - 1, &grFirst) != TCL_OK
		|| TclGetIntForIndexM(interp, objv[3], grLen - 1, &grLast)
			   != TCL_OK) {
	    return TCL_ERROR;
	}

	if (grLast >= 0) {
	    char *rangeFirstPtr;
	    char *rangeEndPtr;
	    rangeFirstPtr = Tcl_GraphemeRange(tutf8Ptr, numBytes, grFirst,
		    grLast, &rangeEndPtr);
	    assert(rangeEndPtr >= rangeFirstPtr);
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(rangeFirstPtr,
					     rangeEndPtr - rangeFirstPtr));
	}
    }
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
GraphemeReverseCmd(
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
     * Graphemes can vary in size with no theoretical limit so reversing in
     * place in unshared object case is not straightforward. Just allocate a
     * new object.
     *
     * TODO - use Tcl unicode string internals rather than appending
     * one at a time with a temporary buffer.
     */

    if (UseUniChars(objv[1])) {
	Tcl_Size uniLen;
	const Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
	if (uniLen > 0) {
	    Tcl_UniChar *tempBufPtr
		    = (Tcl_UniChar *)Tcl_Alloc(uniLen * sizeof(*tempBufPtr));
	    Tcl_GraphemeUniCharReverse(uniPtr, uniLen, tempBufPtr, uniLen);
	    Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(tempBufPtr, uniLen));
	    Tcl_Free(tempBufPtr);
	}
    } else {
	Tcl_Size numBytes;
	const char *tutf8Ptr = TclGetStringFromObj(objv[1], &numBytes);
	if (numBytes > 0) {
	    char *tempBufPtr = (char *)Tcl_Alloc(numBytes);
	    Tcl_GraphemeReverse(tutf8Ptr, numBytes, tempBufPtr, numBytes);
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(tempBufPtr, numBytes));
	    Tcl_Free(tempBufPtr);
	}
    }
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
GraphemeListRepDecrRefs(
    GraphemeListRep *glrPtr)
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
 * GetGraphemeListRep --
 *
 *	Get the internal representation of a grapheme list from a Tcl_Obj.
 *	Assumes that the object is of the right type already.
 *
 * Results:
 *	The internal representation.
 *
 *----------------------------------------------------------------------
 */
static inline GraphemeListRep *
GetGraphemeListRep(
    Tcl_Obj *objPtr)
{
    return (GraphemeListRep *) objPtr->internalRep.otherValuePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * SetGraphemeListRep --
 *
 *	Set the type and internal representation of a Tcl_Obj to a grapheme
 *	list. Doesn't manipulate the reference count of the internal rep or
 *	clear any old internal representation from the Tcl_Obj.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static inline void
SetGraphemeListRep(
    Tcl_Obj *objPtr,
    GraphemeListRep *glrPtr)
{
    objPtr->internalRep.otherValuePtr = glrPtr;
    objPtr->typePtr = &tclGraphemeListType;
}

/*
 *----------------------------------------------------------------------
 *
 * GraphemeListFreeProc --
 *
 *	Frees resources associated with a GraphemeList object's
 *	internal representation.
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
    GraphemeListRepDecrRefs(GetGraphemeListRep(objPtr));
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
    SetGraphemeListRep(copyPtr, GetGraphemeListRep(srcPtr));
    GetGraphemeListRep(srcPtr)->refCount++;
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
GraphemeListLengthProc(
    Tcl_Obj *objPtr)
{
    GraphemeListRep *glrPtr = GetGraphemeListRep(objPtr);
    Tcl_Size uniLen;
    Tcl_UniChar *uniPtr = Tcl_GetUnicodeFromObj(glrPtr->objPtr, &uniLen);
    return Tcl_GraphemeUniCharLength(uniPtr, uniLen);
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
GraphemeListIndexProc(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Obj *objPtr,		/* Source list */
    Tcl_Size index,		/* Element index */
    Tcl_Obj **elemPtrPtr)	/* Returned element */
{
    GraphemeListRep *glrPtr = GetGraphemeListRep(objPtr);
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
	grPtr = Tcl_GraphemeUniCharIndex(glrPtr->graphemeOffset + uniStartPtr,
		uniLen - glrPtr->graphemeOffset, index - glrPtr->graphemeIndex,
		&grWidth, NULL);
    } else if (index > 2*(glrPtr->graphemeIndex - index)) {
	/* index is sufficiently closer to cached index */
	assert(glrPtr->graphemeOffset > 0);
	assert(glrPtr->graphemeOffset <= uniLen);
	const Tcl_UniChar *uniEndPtr = uniStartPtr + glrPtr->graphemeOffset;
	Tcl_Size currentIndex = glrPtr->graphemeIndex;
	while (uniEndPtr > uniStartPtr) {
	    const Tcl_UniChar *uniPtr;
	    uniPtr = GraphemeUniCharPrev(uniEndPtr, uniEndPtr - uniStartPtr,
		    &grWidth);
	    if (--currentIndex == index) {
		grPtr = uniPtr;
		break;
	    }
	    uniEndPtr = uniPtr;
	}
    } else {
	/* Start from the beginning */
	grPtr = Tcl_GraphemeUniCharIndex(uniStartPtr, uniLen, index, &grWidth, NULL);
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
GraphemeSplitCmd(
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
    GraphemeListRep *glrPtr = GraphemeListRepNew(objv[1]);
    glrPtr->refCount = 1;
    SetGraphemeListRep(objPtr, glrPtr);
    Tcl_InvalidateStringRep(objPtr);
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * Tcl_GraphemeNext, Tcl_GraphemeUniCharNext --
 *
 *	Locate start of next grapheme
 *
 * Results:
 *	Returns a pointer to the location following the grapheme at
 *	uniPtr. If numBytes/uniLen is 0, a null pointer is returned.
 *
 * Side effects:
 *	Stores number of code points in the grapheme in *grWidthPtr.
 *
 *------------------------------------------------------------------------
 */
char *
Tcl_GraphemeNext(
    const char *tutf8Ptr,	/* TUTF8 byte sequence */
    Tcl_Size numBytes,		/* Length, nul-terminated if < 0 */
    Tcl_Size *grWidthPtr)	/* If not NULL, location to store grapheme
	 			 * width in code point units */
{
    if (numBytes < 0) {
	numBytes = strlen(tutf8Ptr);
    }
    if (numBytes == 0) {
	if (grWidthPtr) {
	    *grWidthPtr = 0;
	}
	return NULL;
    }
    return GraphemeNext(tutf8Ptr, numBytes, grWidthPtr);
}

Tcl_UniChar *
Tcl_GraphemeUniCharNext(
    const Tcl_UniChar *uniPtr,	/* Sequence of Unicode code points */
    Tcl_Size uniLen,		/* Length, nul-terminated if < 0 */
    Tcl_Size *grWidthPtr)	/* If not NULL, location to store grapheme
	 			 * width in code point units */
{
    if (uniLen < 0) {
	uniLen = Tcl_UniCharLen(uniPtr);
    }
    if (uniLen == 0) {
	if (grWidthPtr) {
	    *grWidthPtr = 0;
	}
	return NULL;
    }
    return GraphemeUniCharNext(uniPtr, uniLen, grWidthPtr);
}

/*
 *------------------------------------------------------------------------
 *
 * Tcl_GraphemePrev, Tcl_GraphemeUniCharPrev --
 *
 *	Locate start of previous grapheme
 *
 * Results:
 *	Returns a pointer to the start of the preceding grapheme.
 *	If numBytes/uniLen is <= 0, a null pointer is returned.
 *
 * Side effects:
 *	Stores number of code points in the grapheme in *grWidthPtr.
 *
 *------------------------------------------------------------------------
 */
char *
Tcl_GraphemePrev(
    const char *tutf8Ptr,	/* Sequence of TUTF8 bytes */
    Tcl_Size numBytes,		/* Length, must be > 0 */
    Tcl_Size *grWidthPtr)	/* If not NULL, location to store grapheme
	 			 * width in code point units */
{
    if (numBytes <= 0) {
	if (grWidthPtr) {
	    *grWidthPtr = 0;
	}
	return NULL;
    }
    return GraphemePrev(tutf8Ptr, numBytes, grWidthPtr);
}

Tcl_UniChar *
Tcl_GraphemeUniCharPrev(
    const Tcl_UniChar *uniPtr,	/* Sequence of Unicode code points */
    Tcl_Size uniLen,		/* Length, must be > 0 */
    Tcl_Size *grWidthPtr)	/* If not NULL, location to store grapheme
	 			 * width in code point units */
{
    if (uniLen <= 0) {
	if (grWidthPtr) {
	    *grWidthPtr = 0;
	}
	return NULL;
    }
    return GraphemeUniCharPrev(uniPtr, uniLen, grWidthPtr);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
