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

/*
 * Table of grapheme subcommand names and implementations.
 */

const EnsembleImplMap tclGraphemeImplMap[] = {
#if 0
    {"index",	GraphemeIndexCmd,	TclCompileGraphemeIndexCmd, NULL, NULL, 0},
    {"length",	GraphemeLengthCmd,	TclCompileGraphemeLengthCmd, NULL, NULL, 0},
#endif
    {"next",	GraphemeNextCmd,	TclCompileGraphemeNextCmd, NULL, NULL, 0},
    {"prev",	GraphemePrevCmd,	TclCompileGraphemePrevCmd, NULL, NULL, 0},
#if 0
    {"range",	GraphemeRangeCmd,	TclCompileGraphemeRangeCmd, NULL, NULL, 0},
    {"reverse",	GraphemeReverseCmd,	TclCompileGraphemeReverseCmd, NULL, NULL, 0},
    {"split",	GraphemeSplitCmd,	TclCompileGraphemeSplitCmd, NULL, NULL, 0},
#endif
    {NULL, NULL, NULL, NULL, NULL, 0}
};

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
 * IsPostcore --
 *
 *	Checks if the passed code point is a postcore code point as per
 *	the definition in UAX #29 Table 1c. A postcore code point is to be
 *	be combined with the preceding core code point, possibly with
 *	other intervening postcore code points.
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
IsPostcore (
    Tcl_UniChar cp)		/* Unicode code point */
{
    /* https://www.unicode.org/reports/tr29/tr29-47.html#Regex_Definitions*/
    switch (utf8proc_get_property(cp)->boundclass) {
    case UTF8PROC_BOUNDCLASS_EXTEND:
    case UTF8PROC_BOUNDCLASS_SPACINGMARK:
    case UTF8PROC_BOUNDCLASS_ZWJ:
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
 * IsPrecore --
 *
 *	Checks if the passed code point is a precore code point as per
 *	the definition in UAX #29 Table 1c. A pre code point is to be
 *	be combined with the next core code point, possibly with
 *	other intervening precore code points.
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
IsPrecore (
    Tcl_UniChar cp)		/* Unicode code point */
{
    utf8proc_property_t *propPtr;
    /* https://www.unicode.org/reports/tr29/tr29-47.html#Regex_Definitions*/
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
	Tcl_WrongNumArgs(interp, 1, objv, "STRING INDEXVAR");
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

    Tcl_Size grLen = 0;
    Tcl_UniChar *uniEndPtr = uniStartPtr + uniLen;
    if (strIndex < uniLen) {
	int state = 0;
	Tcl_UniChar *grStartPtr = uniStartPtr + strIndex;
	Tcl_UniChar *uniPtr = grStartPtr;
	while (++uniPtr < uniEndPtr) {
	    if (utf8proc_grapheme_break_stateful(uniPtr[-1],
			uniPtr[0], &state)) {
		break;
	    }
	}
	grLen = uniPtr - grStartPtr;
	Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(grStartPtr, grLen));
    }
    Tcl_ObjSetVar2(interp, objv[2], NULL, Tcl_NewWideIntObj(strIndex+grLen), 0);
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
	Tcl_WrongNumArgs(interp, 1, objv, "STRING INDEXVAR");
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
		IsPostcore(*uniPtr) || IsPrecore(uniPtr[-1]))) {
	uniPtr--;
    }

    /* Now scan forward. Note uniPtr may be < uniStartPtr if uniLen was 0 */
    Tcl_Size grLen = 0;
    int state = 0;
    if (uniPtr >= uniStartPtr) {
	Tcl_UniChar *grStartPtr = uniPtr;
	while (++uniPtr < uniEndPtr) {
	    if (utf8proc_grapheme_break_stateful(uniPtr[-1],
			uniPtr[0], &state)) {
		grStartPtr = uniPtr;
	    }
	}
	grLen = uniEndPtr - grStartPtr;
	Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(grStartPtr, grLen));
    }

    Tcl_ObjSetVar2(interp, objv[2], NULL, Tcl_NewWideIntObj(strIndex-grLen), 0);
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
