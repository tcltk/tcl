/*
 * tclLink.c --
 *
 *	This file implements linked variables (a C variable that is tied to a
 *	Tcl variable). The idea of linked variables was first suggested by
 *	Andreas Stolcke and this implementation is based heavily on a
 *	prototype implementation provided by him.
 *
 * Copyright © 1993 The Regents of the University of California.
 * Copyright © 1994-1997 Sun Microsystems, Inc.
 * Copyright © 2008 Rene Zaumseil
 * Copyright © 2019 Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclTomMath.h"
#include <math.h>

/*
 * For each linked variable there is a data structure of the following type,
 * which describes the link and is the clientData for the trace set on the Tcl
 * variable.
 */

typedef struct {
    Tcl_Interp *interp;		/* Interpreter containing Tcl variable. */
    Namespace *nsPtr;		/* Namespace containing Tcl variable */
    Tcl_Obj *varName;		/* Name of variable (must be global). This is
				 * needed during trace callbacks, since the
				 * actual variable may be aliased at that time
				 * via upvar. */
    void *addr;			/* Location of C variable. */
    Tcl_Size bytes;		/* Size of C variable array. This is 0 when
				 * single variables, and >0 used for array
				 * variables. */
    Tcl_Size numElems;		/* Number of elements in C variable array.
				 * Zero for single variables. */
    int type;			/* Type of link (TCL_LINK_INT, etc.). */
    union {
	char c;
	unsigned char uc;
	int i;
	unsigned int ui;
	short s;
	unsigned short us;
#if !defined(TCL_WIDE_INT_IS_LONG) && !defined(_WIN32) && !defined(__CYGWIN__)
	long l;
	unsigned long ul;
#endif
	Tcl_WideInt w;
	Tcl_WideUInt uw;
	float f;
	double d;
	void *aryPtr;		/* Generic array. */
	char *cPtr;		/* char array */
	unsigned char *ucPtr;	/* unsigned char array */
	short *sPtr;		/* short array */
	unsigned short *usPtr;	/* unsigned short array */
	int *iPtr;		/* int array */
	unsigned int *uiPtr;	/* unsigned int array */
	long *lPtr;		/* long array */
	unsigned long *ulPtr;	/* unsigned long array */
	Tcl_WideInt *wPtr;	/* wide (long long) array */
	Tcl_WideUInt *uwPtr;	/* unsigned wide (long long) array */
	float *fPtr;		/* float array */
	double *dPtr;		/* double array */
    } lastValue;		/* Last known value of C variable; used to
				 * avoid string conversions. */
    int flags;			/* Miscellaneous one-bit values; see below for
				 * definitions. */
} Link;

/*
 * Definitions for flag bits:
 */
enum LinkFlags {
    LINK_READ_ONLY = 1,		/* Errors should be generated if Tcl script
				 * attempts to write variable. */
    LINK_BEING_UPDATED = 2,	/* A call to Tcl_UpdateLinkedVar() is in
				 * progress for this variable, so trace
				 * callbacks on the variable should be
				 * ignored. */
    LINK_ALLOC_ADDR = 4,	/* linkPtr->addr was allocated on the heap. */
    LINK_ALLOC_LAST = 8		/* linkPtr->valueLast.p was allocated on the
				 * heap. */
};

/*
 * Forward references to functions defined later in this file:
 */

static char *		LinkTraceProc(void *clientData,Tcl_Interp *interp,
			    const char *name1, const char *name2, int flags);
static Tcl_Obj *	ObjValue(Link *linkPtr);
static void		LinkFree(Link *linkPtr);
static int		GetInvalidIntFromObj(Tcl_Obj *objPtr, int *intPtr);
static int		GetInvalidDoubleFromObj(Tcl_Obj *objPtr,
			    double *doublePtr);
static int		SetInvalidRealFromAny(Tcl_Interp *interp,
			    Tcl_Obj *objPtr);

/*
 * A marker type used to flag weirdnesses so we can pass them around right.
 */

static const Tcl_ObjType invalidRealType = {
    "invalidReal",			/* name */
    NULL,				/* freeIntRepProc */
    NULL,				/* dupIntRepProc */
    NULL,				/* updateStringProc */
    NULL,				/* setFromAnyProc */
    TCL_OBJTYPE_V1(TclLengthOne)
};

/*
 * Convenience macro for accessing the value of the C variable pointed to by a
 * link. Note that this macro produces something that may be regarded as an
 * lvalue or rvalue; it may be assigned to as well as read. Also note that
 * this macro assumes the name of the variable being accessed (linkPtr); this
 * is not strictly a good thing, but it keeps the code much shorter and
 * cleaner.
 */

#define LinkedVar(type) (*(type *) linkPtr->addr)

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LinkVar --
 *
 *	Link a C variable to a Tcl variable so that changes to either one
 *	causes the other to change.
 *
 * Results:
 *	The return value is TCL_OK if everything went well or TCL_ERROR if an
 *	error occurred (the interp's result is also set after errors).
 *
 * Side effects:
 *	The value at *addr is linked to the Tcl variable "varName", using
 *	"type" to convert between string values for Tcl and binary values for
 *	*addr.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LinkVar(
    Tcl_Interp *interp,		/* Interpreter in which varName exists. */
    const char *varName,	/* Name of a global variable in interp. */
    void *addr,			/* Address of a C variable to be linked to
				 * varName. */
    int type)			/* Type of C variable: TCL_LINK_INT, etc. Also
				 * may have TCL_LINK_READ_ONLY OR'ed in. */
{
    Tcl_Obj *objPtr;
    Link *linkPtr;
    Namespace *dummy;
    const char *name;
    int code;

    linkPtr = (Link *) Tcl_VarTraceInfo2(interp, varName, NULL,
	    TCL_GLOBAL_ONLY, LinkTraceProc, NULL);
    if (linkPtr != NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"variable '%s' is already linked", varName));
	return TCL_ERROR;
    }

    linkPtr = (Link *)Tcl_Alloc(sizeof(Link));
    linkPtr->interp = interp;
    linkPtr->nsPtr = NULL;
    linkPtr->varName = Tcl_NewStringObj(varName, -1);
    Tcl_IncrRefCount(linkPtr->varName);
    linkPtr->addr = addr;
    linkPtr->type = type & ~TCL_LINK_READ_ONLY;
    if (type & TCL_LINK_READ_ONLY) {
	linkPtr->flags = LINK_READ_ONLY;
    } else {
	linkPtr->flags = 0;
    }
    linkPtr->bytes = 0;
    linkPtr->numElems = 0;
    objPtr = ObjValue(linkPtr);
    if (Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, objPtr,
	    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
	Tcl_DecrRefCount(linkPtr->varName);
	LinkFree(linkPtr);
	return TCL_ERROR;
    }

    TclGetNamespaceForQualName(interp, varName, NULL, TCL_GLOBAL_ONLY,
	    &(linkPtr->nsPtr), &dummy, &dummy, &name);
    linkPtr->nsPtr->refCount++;

    code = Tcl_TraceVar2(interp, varName, NULL,
	    TCL_GLOBAL_ONLY|TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	    LinkTraceProc, linkPtr);
    if (code != TCL_OK) {
	Tcl_DecrRefCount(linkPtr->varName);
	LinkFree(linkPtr);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LinkArray --
 *
 *	Link a C variable array to a Tcl variable so that changes to either
 *	one causes the other to change.
 *
 * Results:
 *	The return value is TCL_OK if everything went well or TCL_ERROR if an
 *	error occurred (the interp's result is also set after errors).
 *
 * Side effects:
 *	The value at *addr is linked to the Tcl variable "varName", using
 *	"type" to convert between string values for Tcl and binary values for
 *	*addr.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LinkArray(
    Tcl_Interp *interp,		/* Interpreter in which varName exists. */
    const char *varName,	/* Name of a global variable in interp. */
    void *addr,			/* Address of a C variable to be linked to
				 * varName. If NULL then the necessary space
				 * will be allocated and returned as the
				 * interpreter result. */
    int type,			/* Type of C variable: TCL_LINK_INT, etc. Also
				 * may have TCL_LINK_READ_ONLY OR'ed in. */
    Tcl_Size size)		/* Size of C variable array, >1 if array */
{
    Tcl_Obj *objPtr;
    Link *linkPtr;
    Namespace *dummy;
    const char *name;
    int code;

    if (size < 1) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"wrong array size given", -1));
	return TCL_ERROR;
    }

    linkPtr = (Link *)Tcl_Alloc(sizeof(Link));
    linkPtr->type = type & ~TCL_LINK_READ_ONLY;
    linkPtr->numElems = size;
    if (type & TCL_LINK_READ_ONLY) {
	linkPtr->flags = LINK_READ_ONLY;
    } else {
	linkPtr->flags = 0;
    }

    switch (linkPtr->type) {
    case TCL_LINK_INT:
    case TCL_LINK_BOOLEAN:
	linkPtr->bytes = size * sizeof(int);
	break;
    case TCL_LINK_DOUBLE:
	linkPtr->bytes = size * sizeof(double);
	break;
    case TCL_LINK_WIDE_INT:
	linkPtr->bytes = size * sizeof(Tcl_WideInt);
	break;
    case TCL_LINK_WIDE_UINT:
	linkPtr->bytes = size * sizeof(Tcl_WideUInt);
	break;
    case TCL_LINK_CHAR:
	linkPtr->bytes = size * sizeof(char);
	break;
    case TCL_LINK_UCHAR:
	linkPtr->bytes = size * sizeof(unsigned char);
	break;
    case TCL_LINK_SHORT:
	linkPtr->bytes = size * sizeof(short);
	break;
    case TCL_LINK_USHORT:
	linkPtr->bytes = size * sizeof(unsigned short);
	break;
    case TCL_LINK_UINT:
	linkPtr->bytes = size * sizeof(unsigned int);
	break;
    case TCL_LINK_FLOAT:
	linkPtr->bytes = size * sizeof(float);
	break;
    case TCL_LINK_STRING:
	linkPtr->bytes = size * sizeof(char);
	size = 1;		/* This is a variable length string, no need
				 * to check last value. */

	/*
	 * If no address is given create one and use as address the
	 * not needed linkPtr->lastValue
	 */

	if (addr == NULL) {
	    linkPtr->lastValue.aryPtr = Tcl_Alloc(linkPtr->bytes);
	    linkPtr->flags |= LINK_ALLOC_LAST;
	    addr = (char *) &linkPtr->lastValue.cPtr;
	}
	break;
    case TCL_LINK_CHARS:
    case TCL_LINK_BINARY:
	linkPtr->bytes = size * sizeof(char);
	break;
    default:
	LinkFree(linkPtr);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"bad linked array variable type", -1));
	return TCL_ERROR;
    }

    /*
     * Allocate C variable space in case no address is given
     */

    if (addr == NULL) {
	linkPtr->addr = Tcl_Alloc(linkPtr->bytes);
	linkPtr->flags |= LINK_ALLOC_ADDR;
    } else {
	linkPtr->addr = addr;
    }

    /*
     * If necessary create space for last used value.
     */

    if (size > 1) {
	linkPtr->lastValue.aryPtr = Tcl_Alloc(linkPtr->bytes);
	linkPtr->flags |= LINK_ALLOC_LAST;
    }

    /*
     * Initialize allocated space.
     */

    if (linkPtr->flags & LINK_ALLOC_ADDR) {
	memset(linkPtr->addr, 0, linkPtr->bytes);
    }
    if (linkPtr->flags & LINK_ALLOC_LAST) {
	memset(linkPtr->lastValue.aryPtr, 0, linkPtr->bytes);
    }

    /*
     * Set common structure values.
     */

    linkPtr->interp = interp;
    linkPtr->varName = Tcl_NewStringObj(varName, -1);
    Tcl_IncrRefCount(linkPtr->varName);

    TclGetNamespaceForQualName(interp, varName, NULL, TCL_GLOBAL_ONLY,
	    &(linkPtr->nsPtr), &dummy, &dummy, &name);
    linkPtr->nsPtr->refCount++;

    objPtr = ObjValue(linkPtr);
    if (Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, objPtr,
	    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
	Tcl_DecrRefCount(linkPtr->varName);
	LinkFree(linkPtr);
	return TCL_ERROR;
    }

    code = Tcl_TraceVar2(interp, varName, NULL,
	    TCL_GLOBAL_ONLY|TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	    LinkTraceProc, linkPtr);
    if (code != TCL_OK) {
	Tcl_DecrRefCount(linkPtr->varName);
	LinkFree(linkPtr);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UnlinkVar --
 *
 *	Destroy the link between a Tcl variable and a C variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If "varName" was previously linked to a C variable, the link is broken
 *	to make the variable independent. If there was no previous link for
 *	"varName" then nothing happens.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_UnlinkVar(
    Tcl_Interp *interp,		/* Interpreter containing variable to unlink */
    const char *varName)	/* Global variable in interp to unlink. */
{
    Link *linkPtr = (Link *) Tcl_VarTraceInfo2(interp, varName, NULL,
	    TCL_GLOBAL_ONLY, LinkTraceProc, NULL);

    if (linkPtr == NULL) {
	return;
    }
    Tcl_UntraceVar2(interp, varName, NULL,
	    TCL_GLOBAL_ONLY|TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	    LinkTraceProc, linkPtr);
    Tcl_DecrRefCount(linkPtr->varName);
    LinkFree(linkPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UpdateLinkedVar --
 *
 *	This function is invoked after a linked variable has been changed by C
 *	code. It updates the Tcl variable so that traces on the variable will
 *	trigger.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Tcl variable "varName" is updated from its C value, causing traces
 *	on the variable to trigger.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_UpdateLinkedVar(
    Tcl_Interp *interp,		/* Interpreter containing variable. */
    const char *varName)	/* Name of global variable that is linked. */
{
    Link *linkPtr = (Link *) Tcl_VarTraceInfo2(interp, varName, NULL,
	    TCL_GLOBAL_ONLY, LinkTraceProc, NULL);
    int savedFlag;

    if (linkPtr == NULL) {
	return;
    }
    savedFlag = linkPtr->flags & LINK_BEING_UPDATED;
    linkPtr->flags |= LINK_BEING_UPDATED;
    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
	    TCL_GLOBAL_ONLY);
    /*
     * Callback may have unlinked the variable. [Bug 1740631]
     */
    linkPtr = (Link *) Tcl_VarTraceInfo2(interp, varName, NULL,
	    TCL_GLOBAL_ONLY, LinkTraceProc, NULL);
    if (linkPtr != NULL) {
	linkPtr->flags = (linkPtr->flags & ~LINK_BEING_UPDATED) | savedFlag;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetInt, GetWide, GetUWide, GetDouble, EqualDouble, IsSpecial --
 *
 *	Helper functions for LinkTraceProc and ObjValue. These are all
 *	factored out here to make those functions simpler.
 *
 *----------------------------------------------------------------------
 */

static inline int
GetInt(
    Tcl_Obj *objPtr,
    int *intPtr)
{
    return (Tcl_GetIntFromObj(NULL, objPtr, intPtr) != TCL_OK
	    && GetInvalidIntFromObj(objPtr, intPtr) != TCL_OK);
}

static inline int
GetWide(
    Tcl_Obj *objPtr,
    Tcl_WideInt *widePtr)
{
    if (TclGetWideIntFromObj(NULL, objPtr, widePtr) != TCL_OK) {
	int intValue;

	if (GetInvalidIntFromObj(objPtr, &intValue) != TCL_OK) {
	    return 1;
	}
	*widePtr = intValue;
    }
    return 0;
}

static inline int
GetUWide(
    Tcl_Obj *objPtr,
    Tcl_WideUInt *uwidePtr)
{
    if (Tcl_GetWideUIntFromObj(NULL, objPtr, uwidePtr) != TCL_OK) {
	int intValue;

	if (GetInvalidIntFromObj(objPtr, &intValue) != TCL_OK) {
	    return 1;
	}
	*uwidePtr = intValue;
    }
    return 0;
}

static inline int
GetDouble(
    Tcl_Obj *objPtr,
    double *dblPtr)
{
    if (Tcl_GetDoubleFromObj(NULL, objPtr, dblPtr) == TCL_OK) {
	return 0;
    } else {
#ifdef ACCEPT_NAN
	Tcl_ObjInternalRep *irPtr = TclFetchInternalRep(objPtr, &tclDoubleType);

	if (irPtr != NULL) {
	    *dblPtr = irPtr->doubleValue;
	    return 0;
	}
#endif /* ACCEPT_NAN */
	return GetInvalidDoubleFromObj(objPtr, dblPtr) != TCL_OK;
    }
}

static inline int
EqualDouble(
    double a,
    double b)
{
    return (a == b)
#ifdef ACCEPT_NAN
	|| (isnan(a) && isnan(b))
#endif /* ACCEPT_NAN */
	;
}

static inline int
IsSpecial(
    double a)
{
    return isinf(a)
#ifdef ACCEPT_NAN
	|| isnan(a)
#endif /* ACCEPT_NAN */
	;
}

/*
 * Mark an object as holding a weird double.
 */

static int
SetInvalidRealFromAny(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Obj *objPtr)
{
    const char *str;
    const char *endPtr;
    Tcl_Size length;

    str = TclGetStringFromObj(objPtr, &length);
    if ((length == 1) && (str[0] == '.')) {
	objPtr->typePtr = &invalidRealType;
	objPtr->internalRep.doubleValue = 0.0;
	return TCL_OK;
    }
    if (TclParseNumber(NULL, objPtr, NULL, str, length, &endPtr,
	    TCL_PARSE_DECIMAL_ONLY) == TCL_OK) {
	/*
	 * If number is followed by [eE][+-]?, then it is an invalid
	 * double, but it could be the start of a valid double.
	 */

	if (*endPtr == 'e' || *endPtr == 'E') {
	    ++endPtr;
	    if (*endPtr == '+' || *endPtr == '-') {
		++endPtr;
	    }
	    if (*endPtr == 0) {
		double doubleValue = 0.0;

		Tcl_GetDoubleFromObj(NULL, objPtr, &doubleValue);
		TclFreeInternalRep(objPtr);
		objPtr->typePtr = &invalidRealType;
		objPtr->internalRep.doubleValue = doubleValue;
		return TCL_OK;
	    }
	}
    }
    return TCL_ERROR;
}

/*
 * This function checks for integer representations, which are valid
 * when linking with C variables, but which are invalid in other
 * contexts in Tcl. Handled are "+", "-", "", "0x", "0b", "0d" and "0o"
 * (upperand lowercase). See bug [39f6304c2e].
 */

static int
GetInvalidIntFromObj(
    Tcl_Obj *objPtr,
    int *intPtr)
{
    Tcl_Size length;
    const char *str = TclGetStringFromObj(objPtr, &length);

    if ((length == 0) || ((length == 2) && (str[0] == '0')
	    && strchr("xXbBoOdD", str[1]))) {
	*intPtr = 0;
	return TCL_OK;
    } else if ((length == 1) && strchr("+-", str[0])) {
	*intPtr = (str[0] == '+');
	return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 * This function checks for double representations, which are valid
 * when linking with C variables, but which are invalid in other
 * contexts in Tcl. Handled are "+", "-", "", ".", "0x", "0b" and "0o"
 * (upper- and lowercase) and sequences like "1e-". See bug [39f6304c2e].
 */

static int
GetInvalidDoubleFromObj(
    Tcl_Obj *objPtr,
    double *doublePtr)
{
    int intValue;

    if (TclHasInternalRep(objPtr, &invalidRealType)) {
	goto gotdouble;
    }
    if (GetInvalidIntFromObj(objPtr, &intValue) == TCL_OK) {
	*doublePtr = (double) intValue;
	return TCL_OK;
    }
    if (SetInvalidRealFromAny(NULL, objPtr) == TCL_OK) {
    gotdouble:
	*doublePtr = objPtr->internalRep.doubleValue;
	return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * LinkTraceProc --
 *
 *	This function is invoked when a linked Tcl variable is read, written,
 *	or unset from Tcl. It's responsible for keeping the C variable in sync
 *	with the Tcl variable.
 *
 * Results:
 *	If all goes well, NULL is returned; otherwise an error message is
 *	returned.
 *
 * Side effects:
 *	The C variable may be updated to make it consistent with the Tcl
 *	variable, or the Tcl variable may be overwritten to reject a
 *	modification.
 *
 *----------------------------------------------------------------------
 */

static char *
LinkTraceProc(
    void *clientData,		/* Contains information about the link. */
    Tcl_Interp *interp,		/* Interpreter containing Tcl variable. */
    TCL_UNUSED(const char *) /*name1*/,
    TCL_UNUSED(const char *) /*name2*/,
				/* Links can only be made to global variables,
				 * so we can find them with need to resolve
				 * caller-supplied name in caller context. */
    int flags)			/* Miscellaneous additional information. */
{
    Link *linkPtr = (Link *)clientData;
    int changed;
    Tcl_Size valueLength = 0;
    const char *value;
    char **pp;
    Tcl_Obj *valueObj;
    int valueInt;
    Tcl_WideInt valueWide;
    Tcl_WideUInt valueUWide;
    double valueDouble;
    Tcl_Size objc, i;
    Tcl_Obj **objv;

    /*
     * If the variable is being unset, then just re-create it (with a trace)
     * unless the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if (Tcl_InterpDeleted(interp) || TclNamespaceDeleted(linkPtr->nsPtr)) {
	    Tcl_DecrRefCount(linkPtr->varName);
	    LinkFree(linkPtr);
	} else if (flags & TCL_TRACE_DESTROYED) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    Tcl_TraceVar2(interp, TclGetString(linkPtr->varName), NULL,
		    TCL_GLOBAL_ONLY|TCL_TRACE_READS|TCL_TRACE_WRITES
		    |TCL_TRACE_UNSETS, LinkTraceProc, linkPtr);
	}
	return NULL;
    }

    /*
     * If we were invoked because of a call to Tcl_UpdateLinkedVar, then don't
     * do anything at all. In particular, we don't want to get upset that the
     * variable is being modified, even if it is supposed to be read-only.
     */

    if (linkPtr->flags & LINK_BEING_UPDATED) {
	return NULL;
    }

    /*
     * For read accesses, update the Tcl variable if the C variable has
     * changed since the last time we updated the Tcl variable.
     */

    if (flags & TCL_TRACE_READS) {
	/*
	 * Variable arrays
	 */

	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    changed = memcmp(linkPtr->addr, linkPtr->lastValue.aryPtr,
		    linkPtr->bytes);
	} else {
	    /* single variables */
	    switch (linkPtr->type) {
	    case TCL_LINK_INT:
	    case TCL_LINK_BOOLEAN:
		changed = (LinkedVar(int) != linkPtr->lastValue.i);
		break;
	    case TCL_LINK_DOUBLE:
		changed = !EqualDouble(LinkedVar(double), linkPtr->lastValue.d);
		break;
	    case TCL_LINK_WIDE_INT:
		changed = (LinkedVar(Tcl_WideInt) != linkPtr->lastValue.w);
		break;
	    case TCL_LINK_WIDE_UINT:
		changed = (LinkedVar(Tcl_WideUInt) != linkPtr->lastValue.uw);
		break;
	    case TCL_LINK_CHAR:
		changed = (LinkedVar(char) != linkPtr->lastValue.c);
		break;
	    case TCL_LINK_UCHAR:
		changed = (LinkedVar(unsigned char) != linkPtr->lastValue.uc);
		break;
	    case TCL_LINK_SHORT:
		changed = (LinkedVar(short) != linkPtr->lastValue.s);
		break;
	    case TCL_LINK_USHORT:
		changed = (LinkedVar(unsigned short) != linkPtr->lastValue.us);
		break;
	    case TCL_LINK_UINT:
		changed = (LinkedVar(unsigned int) != linkPtr->lastValue.ui);
		break;
	    case TCL_LINK_FLOAT:
		changed = !EqualDouble(LinkedVar(float), linkPtr->lastValue.f);
		break;
	    case TCL_LINK_STRING:
	    case TCL_LINK_CHARS:
	    case TCL_LINK_BINARY:
		changed = 1;
		break;
	    default:
		changed = 0;
		/* return (char *) "internal error: bad linked variable type"; */
	    }
	}
	if (changed) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	}
	return NULL;
    }

    /*
     * For writes, first make sure that the variable is writable. Then convert
     * the Tcl value to C if possible. If the variable isn't writable or can't
     * be converted, then restore the variable's old value and return an
     * error. Another tricky thing: we have to save and restore the interp's
     * result, since the variable access could occur when the result has been
     * partially set.
     */

    if (linkPtr->flags & LINK_READ_ONLY) {
	Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		TCL_GLOBAL_ONLY);
	return (char *) "linked variable is read-only";
    }
    valueObj = Tcl_ObjGetVar2(interp, linkPtr->varName,NULL, TCL_GLOBAL_ONLY);
    if (valueObj == NULL) {
	/*
	 * This shouldn't ever happen.
	 */

	return (char *) "internal error: linked variable couldn't be read";
    }

    /*
     * Special cases.
     */

    switch (linkPtr->type) {
    case TCL_LINK_STRING:
	value = TclGetStringFromObj(valueObj, &valueLength);
	pp = (char **) linkPtr->addr;

	*pp = (char *)Tcl_Realloc(*pp, ++valueLength);
	memcpy(*pp, value, valueLength);
	return NULL;

    case TCL_LINK_CHARS:
	value = (char *) TclGetStringFromObj(valueObj, &valueLength);
	valueLength++;		/* include end of string char */
	if (valueLength > linkPtr->bytes) {
	    return (char *) "wrong size of char* value";
	}
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, value, valueLength);
	    memcpy(linkPtr->addr, value, valueLength);
	} else {
	    linkPtr->lastValue.c = '\0';
	    LinkedVar(char) = linkPtr->lastValue.c;
	}
	return NULL;

    case TCL_LINK_BINARY:
	value = (char *) Tcl_GetBytesFromObj(NULL, valueObj, &valueLength);
	if (value == NULL) {
	    return (char *) "invalid binary value";
	} else if (valueLength != linkPtr->bytes) {
	    return (char *) "wrong size of binary value";
	}
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, value, valueLength);
	    memcpy(linkPtr->addr, value, valueLength);
	} else {
	    linkPtr->lastValue.uc = (unsigned char) *value;
	    LinkedVar(unsigned char) = linkPtr->lastValue.uc;
	}
	return NULL;
    }

    /*
     * A helper macro. Writing this as a function is messy because of type
     * variance.
     */

#define InRange(lowerLimit, value, upperLimit)			\
    ((value) >= (lowerLimit) && (value) <= (upperLimit))

    /*
     * If we're working with an array of numbers, extract the Tcl list.
     */

    if (linkPtr->flags & LINK_ALLOC_LAST) {
	if (TclListObjGetElements(NULL, (valueObj), &objc, &objv) == TCL_ERROR
		|| objc != linkPtr->numElems) {
	    return (char *) "wrong dimension";
	}
    }

    switch (linkPtr->type) {
    case TCL_LINK_INT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i = 0; i < objc; i++) {
		int *varPtr = &linkPtr->lastValue.iPtr[i];

		if (GetInt(objv[i], varPtr)) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *) "variable array must have integer values";
		}
	    }
	} else {
	    int *varPtr = &linkPtr->lastValue.i;

	    if (GetInt(valueObj, varPtr)) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *) "variable must have integer value";
	    }
	    LinkedVar(int) = *varPtr;
	}
	break;

    case TCL_LINK_WIDE_INT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i=0; i < objc; i++) {
		Tcl_WideInt *varPtr = &linkPtr->lastValue.wPtr[i];

		if (GetWide(objv[i], varPtr)) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *)
			    "variable array must have wide integer value";
		}
	    }
	} else {
	    Tcl_WideInt *varPtr = &linkPtr->lastValue.w;

	    if (GetWide(valueObj, varPtr)) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *) "variable must have wide integer value";
	    }
	    LinkedVar(Tcl_WideInt) = *varPtr;
	}
	break;

    case TCL_LINK_DOUBLE:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i=0; i < objc; i++) {
		if (GetDouble(objv[i], &linkPtr->lastValue.dPtr[i])) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *) "variable array must have real value";
		}
	    }
	} else {
	    double *varPtr = &linkPtr->lastValue.d;

	    if (GetDouble(valueObj, varPtr)) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *) "variable must have real value";
	    }
	    LinkedVar(double) = *varPtr;
	}
	break;

    case TCL_LINK_BOOLEAN:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i=0; i < objc; i++) {
		int *varPtr = &linkPtr->lastValue.iPtr[i];

		if (Tcl_GetBooleanFromObj(NULL, objv[i], varPtr) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *) "variable array must have boolean value";
		}
	    }
	} else {
	    int *varPtr = &linkPtr->lastValue.i;

	    if (Tcl_GetBooleanFromObj(NULL, valueObj, varPtr) != TCL_OK) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *) "variable must have boolean value";
	    }
	    LinkedVar(int) = *varPtr;
	}
	break;

    case TCL_LINK_CHAR:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i=0; i < objc; i++) {
		if (GetInt(objv[i], &valueInt)
			|| !InRange(SCHAR_MIN, valueInt, SCHAR_MAX)) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *) "variable array must have char value";
		}
		linkPtr->lastValue.cPtr[i] = (char) valueInt;
	    }
	} else {
	    if (GetInt(valueObj, &valueInt)
		    || !InRange(SCHAR_MIN, valueInt, SCHAR_MAX)) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *) "variable must have char value";
	    }
	    LinkedVar(char) = linkPtr->lastValue.c = (char) valueInt;
	}
	break;

    case TCL_LINK_UCHAR:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i=0; i < objc; i++) {
		if (GetInt(objv[i], &valueInt)
			|| !InRange(0, valueInt, (int)UCHAR_MAX)) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *)
			    "variable array must have unsigned char value";
		}
		linkPtr->lastValue.ucPtr[i] = (unsigned char) valueInt;
	    }
	} else {
	    if (GetInt(valueObj, &valueInt)
		    || !InRange(0, valueInt, (int)UCHAR_MAX)) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *) "variable must have unsigned char value";
	    }
	    LinkedVar(unsigned char) = linkPtr->lastValue.uc =
		    (unsigned char) valueInt;
	}
	break;

    case TCL_LINK_SHORT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i=0; i < objc; i++) {
		if (GetInt(objv[i], &valueInt)
			|| !InRange(SHRT_MIN, valueInt, SHRT_MAX)) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *) "variable array must have short value";
		}
		linkPtr->lastValue.sPtr[i] = (short) valueInt;
	    }
	} else {
	    if (GetInt(valueObj, &valueInt)
		    || !InRange(SHRT_MIN, valueInt, SHRT_MAX)) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *) "variable must have short value";
	    }
	    LinkedVar(short) = linkPtr->lastValue.s = (short) valueInt;
	}
	break;

    case TCL_LINK_USHORT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i=0; i < objc; i++) {
		if (GetInt(objv[i], &valueInt)
			|| !InRange(0, valueInt, (int)USHRT_MAX)) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *)
			"variable array must have unsigned short value";
		}
		linkPtr->lastValue.usPtr[i] = (unsigned short) valueInt;
	    }
	} else {
	    if (GetInt(valueObj, &valueInt)
		    || !InRange(0, valueInt, (int)USHRT_MAX)) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *) "variable must have unsigned short value";
	    }
	    LinkedVar(unsigned short) = linkPtr->lastValue.us =
		    (unsigned short) valueInt;
	}
	break;

    case TCL_LINK_UINT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i=0; i < objc; i++) {
		if (GetWide(objv[i], &valueWide)
			|| !InRange(0, valueWide, (Tcl_WideInt)UINT_MAX)) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *)
			    "variable array must have unsigned int value";
		}
		linkPtr->lastValue.uiPtr[i] = (unsigned int) valueWide;
	    }
	} else {
	    if (GetWide(valueObj, &valueWide)
		    || !InRange(0, valueWide, (Tcl_WideInt)UINT_MAX)) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *) "variable must have unsigned int value";
	    }
	    LinkedVar(unsigned int) = linkPtr->lastValue.ui =
		    (unsigned int) valueWide;
	}
	break;
    case TCL_LINK_WIDE_UINT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i=0; i < objc; i++) {
		if (GetUWide(objv[i], &valueUWide)) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *)
			    "variable array must have unsigned wide int value";
		}
		linkPtr->lastValue.uwPtr[i] = valueUWide;
	    }
	} else {
	    if (GetUWide(valueObj, &valueUWide)) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *) "variable must have unsigned wide int value";
	    }
	    LinkedVar(Tcl_WideUInt) = linkPtr->lastValue.uw = valueUWide;
	}
	break;

    case TCL_LINK_FLOAT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    for (i=0; i < objc; i++) {
		if (GetDouble(objv[i], &valueDouble)
			&& !InRange(FLT_MIN, fabs(valueDouble), FLT_MAX)
			&& !IsSpecial(valueDouble)) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have float value";
		}
		linkPtr->lastValue.fPtr[i] = (float) valueDouble;
	    }
	} else {
	    if (GetDouble(valueObj, &valueDouble)
		    && !InRange(FLT_MIN, fabs(valueDouble), FLT_MAX)
		    && !IsSpecial(valueDouble)) {
		Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		return (char *)"variable must have float value";
	    }
	    LinkedVar(float) = linkPtr->lastValue.f = (float) valueDouble;
	}
	break;

    default:
	return (char *) "internal error: bad linked variable type";
    }

    if (linkPtr->flags & LINK_ALLOC_LAST) {
	memcpy(linkPtr->addr, linkPtr->lastValue.aryPtr, linkPtr->bytes);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ObjValue --
 *
 *	Converts the value of a C variable to a Tcl_Obj* for use in a Tcl
 *	variable to which it is linked.
 *
 * Results:
 *	The return value is a pointer to a Tcl_Obj that represents the value
 *	of the C variable given by linkPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ObjValue(
    Link *linkPtr)		/* Structure describing linked variable. */
{
    char *p;
    Tcl_Obj *resultObj, **objv;
    Tcl_Size i;

    switch (linkPtr->type) {
    case TCL_LINK_INT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		TclNewIntObj(objv[i], linkPtr->lastValue.iPtr[i]);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.i = LinkedVar(int);
	return Tcl_NewWideIntObj(linkPtr->lastValue.i);
    case TCL_LINK_WIDE_INT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		TclNewIntObj(objv[i], linkPtr->lastValue.wPtr[i]);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.w = LinkedVar(Tcl_WideInt);
	return Tcl_NewWideIntObj(linkPtr->lastValue.w);
    case TCL_LINK_DOUBLE:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		TclNewDoubleObj(objv[i], linkPtr->lastValue.dPtr[i]);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.d = LinkedVar(double);
	return Tcl_NewDoubleObj(linkPtr->lastValue.d);
    case TCL_LINK_BOOLEAN:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		objv[i] = Tcl_NewBooleanObj(linkPtr->lastValue.iPtr[i] != 0);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.i = LinkedVar(int);
	return Tcl_NewBooleanObj(linkPtr->lastValue.i);
    case TCL_LINK_CHAR:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		TclNewIntObj(objv[i], linkPtr->lastValue.cPtr[i]);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.c = LinkedVar(char);
	return Tcl_NewWideIntObj(linkPtr->lastValue.c);
    case TCL_LINK_UCHAR:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		TclNewIntObj(objv[i], linkPtr->lastValue.ucPtr[i]);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.uc = LinkedVar(unsigned char);
	return Tcl_NewWideIntObj(linkPtr->lastValue.uc);
    case TCL_LINK_SHORT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		TclNewIntObj(objv[i], linkPtr->lastValue.sPtr[i]);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.s = LinkedVar(short);
	return Tcl_NewWideIntObj(linkPtr->lastValue.s);
    case TCL_LINK_USHORT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		TclNewIntObj(objv[i], linkPtr->lastValue.usPtr[i]);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.us = LinkedVar(unsigned short);
	return Tcl_NewWideIntObj(linkPtr->lastValue.us);
    case TCL_LINK_UINT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		TclNewIntObj(objv[i], linkPtr->lastValue.uiPtr[i]);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned int);
	return Tcl_NewWideIntObj((Tcl_WideInt) linkPtr->lastValue.ui);
    case TCL_LINK_FLOAT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		TclNewDoubleObj(objv[i], linkPtr->lastValue.fPtr[i]);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.f = LinkedVar(float);
	return Tcl_NewDoubleObj(linkPtr->lastValue.f);
    case TCL_LINK_WIDE_UINT: {
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    objv = (Tcl_Obj **)Tcl_Alloc(linkPtr->numElems * sizeof(Tcl_Obj *));
	    for (i=0; i < linkPtr->numElems; i++) {
		TclNewUIntObj(objv[i], linkPtr->lastValue.uwPtr[i]);
	    }
	    resultObj = Tcl_NewListObj(linkPtr->numElems, objv);
	    Tcl_Free(objv);
	    return resultObj;
	}
	linkPtr->lastValue.uw = LinkedVar(Tcl_WideUInt);
	Tcl_Obj *uwObj;
	TclNewUIntObj(uwObj, linkPtr->lastValue.uw);
	return uwObj;
    }

    case TCL_LINK_STRING:
	p = LinkedVar(char *);
	if (p == NULL) {
	    TclNewLiteralStringObj(resultObj, "NULL");
	    return resultObj;
	}
	return Tcl_NewStringObj(p, -1);

    case TCL_LINK_CHARS:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    linkPtr->lastValue.cPtr[linkPtr->bytes-1] = '\0';
	    /* take care of proper string end */
	    return Tcl_NewStringObj(linkPtr->lastValue.cPtr, linkPtr->bytes);
	}
	linkPtr->lastValue.c = '\0';
	return Tcl_NewStringObj(&linkPtr->lastValue.c, 1);

    case TCL_LINK_BINARY:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.aryPtr, linkPtr->addr, linkPtr->bytes);
	    return Tcl_NewByteArrayObj((unsigned char *) linkPtr->addr,
		    linkPtr->bytes);
	}
	linkPtr->lastValue.uc = LinkedVar(unsigned char);
	return Tcl_NewByteArrayObj(&linkPtr->lastValue.uc, 1);

    /*
     * This code only gets executed if the link type is unknown (shouldn't
     * ever happen).
     */

    default:
	TclNewLiteralStringObj(resultObj, "??");
	return resultObj;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * LinkFree --
 *
 *	Free's allocated space of given link and link structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
LinkFree(
    Link *linkPtr)		/* Structure describing linked variable. */
{
    if (linkPtr->nsPtr) {
	TclNsDecrRefCount(linkPtr->nsPtr);
    }
    if (linkPtr->flags & LINK_ALLOC_ADDR) {
	Tcl_Free(linkPtr->addr);
    }
    if (linkPtr->flags & LINK_ALLOC_LAST) {
	Tcl_Free(linkPtr->lastValue.aryPtr);
    }
    Tcl_Free(linkPtr);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
