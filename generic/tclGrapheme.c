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
	   #if 0
    {"prev",	GraphemePrevCmd,	TclCompileGraphemePrevCmd, NULL, NULL, 0},
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
    /* https://www.unicode.org/reports/tr29/tr29-47.html#Regex_Definitions*/
    return utf8proc_get_property(cp)->boundclass == UTF8PROC_BOUNDCLASS_PREPEND;
}

/*
 *------------------------------------------------------------------------
 *
 * GraphemeNextCmd --
 *
 *	Implements the Tcl command GraphemeNextCmd
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
GraphemeNextCmd (
	ClientData notUsed,
	Tcl_Interp *interp,	/* Current interpreter. */
	Tcl_Size objc,		/* Number of arguments. */
	Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "STRING INDEXVAR");
	return TCL_ERROR;
    }

    Tcl_Size strIndex = 0;
    Tcl_Obj *indexObj = Tcl_ObjGetVar2(interp, objv[2], NULL, 0);
    if (indexObj) {
	if (Tcl_GetSizeIntFromObj(interp, indexObj, &strIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    Tcl_Size grLen = 0;
    Tcl_Size uniLen;
    Tcl_UniChar *uniStartPtr = Tcl_GetUnicodeFromObj(objv[1], &uniLen);
    Tcl_UniChar *uniEndPtr = uniStartPtr + uniLen;
    if (strIndex < uniLen) {
	int state = 0;
	uniStartPtr += strIndex;
	Tcl_UniChar *uniPtr = uniStartPtr;
	while (++uniPtr < uniEndPtr) {
	    if (utf8proc_grapheme_break_stateful(uniPtr[-1],
			uniPtr[0], &state)) {
		break;
	    }
	}
	grLen = uniPtr - uniStartPtr;
	Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(uniStartPtr, grLen));
    }
    Tcl_ObjSetVar2(interp, objv[2], NULL, Tcl_NewWideIntObj(grLen), 0);
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
