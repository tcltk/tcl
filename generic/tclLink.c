/*
 * tclLink.c --
 *
 *	This file implements linked variables (a C variable that is tied to a
 *	Tcl variable). The idea of linked variables was first suggested by
 *	Andreas Stolcke and this implementation is based heavily on a
 *	prototype implementation provided by him.
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * For each linked variable there is a data structure of the following type,
 * which describes the link and is the clientData for the trace set on the Tcl
 * variable.
 */

typedef struct Link {
    Tcl_Interp *interp;		/* Interpreter containing Tcl variable. */
    Tcl_Obj *varName;		/* Name of variable (must be global). This is
				 * needed during trace callbacks, since the
				 * actual variable may be aliased at that time
				 * via upvar. */
    char *addr;			/* Location of C variable. */
    size_t bytes;		/* Size of C variable array.
                                 * 0 when single variables
                                 * >0 used for array variables */
    int type;			/* Type of link (TCL_LINK_INT, etc.). */
    union {
	int i;
	unsigned int ui;
	Tcl_WideInt w;
	Tcl_WideUInt uw;
	float f;
	double d;
	long double ld;
	void *p;
    } lastValue;		/* Last known value of C variable; used to
				 * avoid string conversions. Pointer values
				 * will be used for array links. */
    int flags;			/* Miscellaneous one-bit values; see below for
				 * definitions. */
} Link;

/*
 * Definitions for flag bits:
 * LINK_READ_ONLY -		1 means errors should be generated if Tcl
 *				script attempts to write variable.
 * LINK_BEING_UPDATED -		1 means that a call to Tcl_UpdateLinkedVar is
 *				in progress for this variable, so trace
 *				callbacks on the variable should be ignored.
 * LINK_ALLOC_ADDR -		1 means linkPtr->addr was allocated on the heap
 * LINK_ALLOC_LAST -		1 means linkPtr->valueLast.p was allocated on
 * 				the heap
 */

#define LINK_READ_ONLY		1
#define LINK_BEING_UPDATED	2
#define LINK_ALLOC_ADDR		4
#define LINK_ALLOC_LAST		8

/*
 * Forward references to functions defined later in this file:
 */

static char *		LinkTraceProc(ClientData clientData,Tcl_Interp *interp,
			    const char *name1, const char *name2, int flags);
static Tcl_Obj *	ObjValue(Link *linkPtr);
static void		LinkFree(Link *linkPtr);
static float		S5ToFloat(unsigned int value);
static unsigned int	FloatToS5(float value);
static float		S5timeToFloat(unsigned short value);
static unsigned short	FloatToS5time(unsigned short basis, float value);
static int		Hex2Int(const char ch);
static int		HexTo8bit(const char *ch, unsigned char *ret);
static int		HexTo16bit(const char *ch, unsigned short *ret);
static int		HexTo32bit(const char *ch, unsigned int *ret);
static int		HexTo64bit(const char *ch, Tcl_WideUInt *ret);
static int		GetInvalidIntFromObj(Tcl_Obj *objPtr, int *intPtr);
static int		GetInvalidWideFromObj(Tcl_Obj *objPtr, Tcl_WideInt *widePtr);
static int		GetInvalidDoubleFromObj(Tcl_Obj *objPtr, double *doublePtr);

/*
 * Convenience macro for accessing the value of the C variable pointed to by a
 * link. Note that this macro produces something that may be regarded as an
 * lvalue or rvalue; it may be assigned to as well as read. Also note that
 * this macro assumes the name of the variable being accessed (linkPtr); this
 * is not strictly a good thing, but it keeps the code much shorter and
 * cleaner.
 */

#define LinkedVar(type) (*(type *) linkPtr->addr)

static const int linkCompatTable[] = {
	0,
	TCL_LINK_INT,
	TCL_LINK_DOUBLE,
	TCL_LINK_BOOLEAN,
	TCL_LINK_STRING,
	TCL_LINK_WIDE_INT,
	TCL_LINK_CHAR,
	TCL_LINK_UCHAR,
	TCL_LINK_SHORT,
	TCL_LINK_USHORT,
	TCL_LINK_UINT,
	TCL_LINK_LONG,
	TCL_LINK_ULONG,
	TCL_LINK_FLOAT,
	TCL_LINK_WIDE_UINT
};

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
    char *addr,			/* Address of a C variable to be linked to
				 * varName. */
    int type)			/* Type of C variable: TCL_LINK_INT, etc. Also
				 * may have TCL_LINK_READ_ONLY OR'ed in. */
{
    int code;
    code = Tcl_LinkArray(interp, varName, addr, type, 1);
    if (code == TCL_OK) {/* Don't return address because of old behaviour */
	Tcl_ResetResult(interp);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LinkArray --
 *
 *	Link a C variable array to a Tcl variable so that changes to either one
 *	causes the other to change.
 *
 * Results:
 *	The return value is TCL_OK if everything went well or TCL_ERROR if an
 *	error occurred (the interp's result is also set after errors).
 *
 * Side effects:
 *	The value at *addr is linked to the Tcl variable "varName", using
 *	"type" to convert between string values for Tcl and binary values for
 *	*addr. If "size" is greater then 1 then link to a tcl list variable.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LinkArray(
    Tcl_Interp *interp,		/* Interpreter in which varName exists. */
    const char *varName,	/* Name of a global variable in interp. */
    char *addr,			/* Address of a C variable to be linked to
				 * varName. If NULL then the necessary space
				 * will be allocated and returned as the
				 * interpreter result. */
    int type,			/* Type of C variable: TCL_LINK_INT, etc. Also
				 * may have TCL_LINK_READ_ONLY OR'ed in. */
    size_t size)		/* Size of C variable array, >1 if array. */
{
    Tcl_Obj *objPtr;
    Link *linkPtr;
    int code;

    if (size < 1) {
	Tcl_AppendResult(interp, "wrong array size given", NULL);
	    return TCL_ERROR;
	}
	linkPtr = (Link *) Tcl_VarTraceInfo(interp,varName,TCL_GLOBAL_ONLY,
	LinkTraceProc, (ClientData) NULL);
	if (linkPtr != NULL) {
	    Tcl_AppendResult(interp, "variable is already linked", NULL);
	    return TCL_ERROR;
	}
	linkPtr = (Link *) ckalloc(sizeof(Link));
	linkPtr->type = type & ~TCL_LINK_READ_ONLY;
#if !defined(TCL_NO_DEPRECATED)
    if (linkPtr->type<15){
	linkPtr->type = linkCompatTable[linkPtr->type&15];
    }
#endif
    if (type & TCL_LINK_READ_ONLY) {
	linkPtr->flags = LINK_READ_ONLY;
    } else {
	linkPtr->flags = 0;
    }
    switch (linkPtr->type) {
    case TCL_TYPE_C(float):
    case TCL_TYPE_C(double):
    case TCL_TYPE_C(double)+2:
    case TCL_TYPE_C(double)+4:
    case TCL_TYPE_C(double)+8:
	size = size * 2;
	linkPtr->bytes = size * (linkPtr->type & 0x7f);
	break;
    case TCL_LINK_STRING:
	linkPtr->bytes = size * sizeof(char);
	size = 1;/* this is a variable length string, no need to check last value */
	/* if no address is given create one and use as address the
	 * not needed linkPtr->last */
	if (addr == NULL) {
	    linkPtr->lastValue.p = ckalloc(linkPtr->bytes);
	    linkPtr->flags |= LINK_ALLOC_LAST;
	    addr = (char *) &linkPtr->lastValue.p;
	}
	break;
    case TCL_LINK_BIT8:
    case TCL_LINK_BIT16:
    case TCL_LINK_BIT32:
    case TCL_LINK_BIT64:
	if (size > (linkPtr->type & 0x7f)) {
	    Tcl_AppendResult(interp, "size to big", NULL);
	    return TCL_ERROR;
	}
	linkPtr->bytes = size - 1;
	size = 1;
	break;
    default:
	linkPtr->bytes = size * (linkPtr->type & 0x7f);
	break;
    }

    /* allocate C variable space in case no address is given */
    if (addr == NULL) {
	linkPtr->addr = ckalloc(linkPtr->bytes);
	linkPtr->flags |= LINK_ALLOC_ADDR;
    } else {
	linkPtr->addr = addr;
    }
    /* if necessary create space for last used value */
    if (size > 1) {
	linkPtr->lastValue.p = ckalloc(linkPtr->bytes);
	linkPtr->flags |= LINK_ALLOC_LAST;
    }
    /* set common structure values */
    linkPtr->interp = interp;
    linkPtr->varName = Tcl_NewStringObj(varName, -1);
    Tcl_IncrRefCount(linkPtr->varName);
    objPtr = ObjValue(linkPtr);
    if (Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, objPtr,
	    TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG) == NULL) {
	Tcl_DecrRefCount(linkPtr->varName);
	LinkFree(linkPtr);
	return TCL_ERROR;
    }
    code = Tcl_TraceVar(interp, varName, TCL_GLOBAL_ONLY | TCL_TRACE_READS
	    | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, LinkTraceProc, (ClientData) linkPtr);
    if (code != TCL_OK) {
	Tcl_DecrRefCount(linkPtr->varName);
	LinkFree(linkPtr);
    } else {
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj((Tcl_WideInt) linkPtr->addr));
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
    Link * linkPtr)	/* Structure describing linked variable. */
{
    if (linkPtr->flags & LINK_ALLOC_ADDR) {
	ckfree(linkPtr->addr);
    }
    if (linkPtr->flags & LINK_ALLOC_LAST) {
	ckfree(linkPtr->lastValue.p);
    }
    ckfree((char *) linkPtr);
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
    Tcl_Obj *resultObj;
    int objc;
    static Tcl_Obj **objv = NULL;
    int i;
    int j;
    static const char hexdigit[] = "0123456789abcdef";
    char c[64];

    switch (linkPtr->type) {
    case TCL_LINK_CHAR:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    char *pc = linkPtr->lastValue.p;
	    memcpy(pc, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(char);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewIntObj(pc[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.i = LinkedVar(char);
	return Tcl_NewIntObj(linkPtr->lastValue.i);
    case TCL_LINK_SHORT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    short *ps = linkPtr->lastValue.p;
	    memcpy(ps, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(short);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewIntObj(ps[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.i = LinkedVar(short);
	return Tcl_NewIntObj(linkPtr->lastValue.i);
    case TCL_LINK_INT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    int *pi = linkPtr->lastValue.p;
	    memcpy(pi, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(int);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewIntObj(pi[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.i = LinkedVar(int);
	return Tcl_NewIntObj(linkPtr->lastValue.i);
    case TCL_LINK_WIDE_INT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    Tcl_WideInt *pw = linkPtr->lastValue.p;
	    memcpy(pw, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(Tcl_WideInt);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewWideIntObj(pw[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.w = LinkedVar(Tcl_WideInt);
	return Tcl_NewWideIntObj(linkPtr->lastValue.w);
    case TCL_LINK_UCHAR:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned char *puc = linkPtr->lastValue.p;
	    memcpy(puc, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned char);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewIntObj(puc[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.i = LinkedVar(unsigned char);
	return Tcl_NewIntObj(linkPtr->lastValue.i);
    case TCL_LINK_USHORT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned short *pus = linkPtr->lastValue.p;
	    memcpy(pus, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned short);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewIntObj(pus[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned short);
	return Tcl_NewIntObj((int)linkPtr->lastValue.ui);
    case TCL_LINK_UINT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned int *pui = linkPtr->lastValue.p;
	    memcpy(pui, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned int);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewWideIntObj(pui[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned int);
	return Tcl_NewWideIntObj((Tcl_WideInt) linkPtr->lastValue.ui);
    case TCL_LINK_WIDE_UINT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    Tcl_WideUInt *puw = linkPtr->lastValue.p;
	    memcpy(puw, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(Tcl_WideUInt);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewWideIntObj(puw[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.uw = LinkedVar(Tcl_WideUInt);
	return Tcl_NewWideIntObj((Tcl_WideUInt) linkPtr->lastValue.uw);
    case TCL_LINK_FLOAT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    float *pf = linkPtr->lastValue.p;
	    memcpy(pf, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(float);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewDoubleObj(pf[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.f = LinkedVar(float);
	return Tcl_NewDoubleObj(linkPtr->lastValue.f);
    case TCL_LINK_DOUBLE:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    double *pd = linkPtr->lastValue.p;
	    memcpy(pd, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(double);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewDoubleObj(pd[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.d = LinkedVar(double);
	return Tcl_NewDoubleObj(linkPtr->lastValue.d);
    case TCL_LINK_DOUBLE+2:
    case TCL_LINK_DOUBLE+4:
    case TCL_LINK_DOUBLE+8:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    long double *pld = linkPtr->lastValue.p;
	    memcpy(pld, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(long double);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewDoubleObj((double)pld[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ld = LinkedVar(long double);
	return Tcl_NewDoubleObj(linkPtr->lastValue.ld);
    case TCL_TYPE_C(float): {
	    float *pf = linkPtr->lastValue.p;
	    memcpy(pf, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(float);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewDoubleObj((double) pf[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
    case TCL_TYPE_C(double): {
	    double *pd = linkPtr->lastValue.p;
	    memcpy(pd, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(double);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewDoubleObj(pd[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
    case TCL_TYPE_C(double)+2:
    case TCL_TYPE_C(double)+4:
    case TCL_TYPE_C(double)+8: {
	    long double *pld = linkPtr->lastValue.p;
	    memcpy(pld, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(long double);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewDoubleObj((double)pld[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
    case TCL_LINK_STRING:
	p = (*(char **) linkPtr->addr);
	if (p == NULL) {
	    resultObj = Tcl_NewStringObj("NULL", 4);
	    return resultObj;
	}
	return Tcl_NewStringObj(p, -1);
    case TCL_LINK_CHARS:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    char *pc = linkPtr->lastValue.p;
	    memcpy(pc, linkPtr->addr, linkPtr->bytes);
	    pc[linkPtr->bytes - 1] = '\0';      /* take care of proper string end */
	    return Tcl_NewStringObj(pc, linkPtr->bytes);
	}
	linkPtr->lastValue.i = '\0';
	return Tcl_NewStringObj("", 1);
    case TCL_LINK_BINARY: {
	    unsigned char uc;
	    if (linkPtr->flags & LINK_ALLOC_LAST) {
		memcpy(linkPtr->lastValue.p, linkPtr->addr, linkPtr->bytes);
		return Tcl_NewByteArrayObj((unsigned char *) linkPtr->addr,
			linkPtr->bytes);
	    }
	    linkPtr->lastValue.i = LinkedVar(unsigned char);
	    uc = (unsigned char) linkPtr->lastValue.i;
	    return Tcl_NewByteArrayObj(&uc, 1);
	}
    case TCL_TYPE_X(char):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned char *puc = linkPtr->lastValue.p;
	    memcpy(puc, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned char);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    c[0] = '0';
	    c[1] = 'x';
	    for (i = 0; i < objc; i++) {
		c[3] = hexdigit[puc[i] & 0xf];
		c[2] = hexdigit[(puc[i] >> 4) & 0xf];
		objv[i] = Tcl_NewStringObj(c, 4);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.i = LinkedVar(unsigned char);
	c[0] = '0';
	c[1] = 'x';
	c[3] = hexdigit[linkPtr->lastValue.i & 0xf];
	c[2] = hexdigit[(linkPtr->lastValue.i >> 4) & 0xf];
	return Tcl_NewStringObj(c, 4);
    case TCL_TYPE_X(short):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned short *pus = linkPtr->lastValue.p;
	    memcpy(pus, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned short);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    c[0] = '0';
	    c[1] = 'x';
	    for (i = 0; i < objc; i++) {
		c[5] = hexdigit[pus[i] & 0xf];
		c[4] = hexdigit[(pus[i] >> 4) & 0xf];
		c[3] = hexdigit[(pus[i] >> 8) & 0xf];
		c[2] = hexdigit[(pus[i] >> 12) & 0xf];
		objv[i] = Tcl_NewStringObj(c, 6);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned short);
	c[0] = '0';
	c[1] = 'x';
	c[5] = hexdigit[linkPtr->lastValue.ui & 0xf];
	c[4] = hexdigit[(linkPtr->lastValue.ui >> 4) & 0xf];
	c[3] = hexdigit[(linkPtr->lastValue.ui >> 8) & 0xf];
	c[2] = hexdigit[(linkPtr->lastValue.ui >> 12) & 0xf];
	return Tcl_NewStringObj(c, 6);
    case TCL_TYPE_X(int):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned int *pui = linkPtr->lastValue.p;
	    memcpy(pui, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned int);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    c[0] = '0';
	    c[1] = 'x';
	    for (i = 0; i < objc; i++) {
		c[9] = hexdigit[pui[i] & 0xf];
		c[8] = hexdigit[(pui[i] >> 4) & 0xf];
		c[7] = hexdigit[(pui[i] >> 8) & 0xf];
		c[6] = hexdigit[(pui[i] >> 12) & 0xf];
		c[5] = hexdigit[(pui[i] >> 16) & 0xf];
		c[4] = hexdigit[(pui[i] >> 20) & 0xf];
		c[3] = hexdigit[(pui[i] >> 24) & 0xf];
		c[2] = hexdigit[(pui[i] >> 28) & 0xf];
		objv[i] = Tcl_NewStringObj(c, 10);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned int);
	c[0] = '0';
	c[1] = 'x';
	c[9] = hexdigit[linkPtr->lastValue.ui & 0xf];
	c[8] = hexdigit[(linkPtr->lastValue.ui >> 4) & 0xf];
	c[7] = hexdigit[(linkPtr->lastValue.ui >> 8) & 0xf];
	c[6] = hexdigit[(linkPtr->lastValue.ui >> 12) & 0xf];
	c[5] = hexdigit[(linkPtr->lastValue.ui >> 16) & 0xf];
	c[4] = hexdigit[(linkPtr->lastValue.ui >> 20) & 0xf];
	c[3] = hexdigit[(linkPtr->lastValue.ui >> 24) & 0xf];
	c[2] = hexdigit[(linkPtr->lastValue.ui >> 28) & 0xf];
	return Tcl_NewStringObj(c, 10);
    case TCL_TYPE_X(Tcl_WideInt):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    Tcl_WideUInt *puw = linkPtr->lastValue.p;
	    memcpy(puw, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(Tcl_WideUInt);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    c[0] = '0';
	    c[1] = 'x';
	    for (i = 0; i < objc; i++) {
		c[17] = hexdigit[puw[i] & 0xf];
		c[16] = hexdigit[(puw[i] >> 4) & 0xf];
		c[15] = hexdigit[(puw[i] >> 8) & 0xf];
		c[14] = hexdigit[(puw[i] >> 12) & 0xf];
		c[13] = hexdigit[(puw[i] >> 16) & 0xf];
		c[12] = hexdigit[(puw[i] >> 20) & 0xf];
		c[11] = hexdigit[(puw[i] >> 24) & 0xf];
		c[10] = hexdigit[(puw[i] >> 28) & 0xf];
		c[9] = hexdigit[(puw[i] >> 32) & 0xf];
		c[8] = hexdigit[(puw[i] >> 36) & 0xf];
		c[7] = hexdigit[(puw[i] >> 40) & 0xf];
		c[6] = hexdigit[(puw[i] >> 44) & 0xf];
		c[5] = hexdigit[(puw[i] >> 48) & 0xf];
		c[4] = hexdigit[(puw[i] >> 52) & 0xf];
		c[3] = hexdigit[(puw[i] >> 56) & 0xf];
		c[2] = hexdigit[(puw[i] >> 60) & 0xf];
		objv[i] = Tcl_NewStringObj(c, 18);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.uw = LinkedVar(Tcl_WideUInt);
	c[0] = '0';
	c[1] = 'x';
	c[17] = hexdigit[linkPtr->lastValue.uw & 0xf];
	c[16] = hexdigit[(linkPtr->lastValue.uw >> 4) & 0xf];
	c[15] = hexdigit[(linkPtr->lastValue.uw >> 8) & 0xf];
	c[14] = hexdigit[(linkPtr->lastValue.uw >> 12) & 0xf];
	c[13] = hexdigit[(linkPtr->lastValue.uw >> 16) & 0xf];
	c[12] = hexdigit[(linkPtr->lastValue.uw >> 20) & 0xf];
	c[11] = hexdigit[(linkPtr->lastValue.uw >> 24) & 0xf];
	c[10] = hexdigit[(linkPtr->lastValue.uw >> 28) & 0xf];
	c[9] = hexdigit[(linkPtr->lastValue.uw >> 32) & 0xf];
	c[8] = hexdigit[(linkPtr->lastValue.uw >> 36) & 0xf];
	c[7] = hexdigit[(linkPtr->lastValue.uw >> 40) & 0xf];
	c[6] = hexdigit[(linkPtr->lastValue.uw >> 44) & 0xf];
	c[5] = hexdigit[(linkPtr->lastValue.uw >> 48) & 0xf];
	c[4] = hexdigit[(linkPtr->lastValue.uw >> 52) & 0xf];
	c[3] = hexdigit[(linkPtr->lastValue.uw >> 56) & 0xf];
	c[2] = hexdigit[(linkPtr->lastValue.uw >> 60) & 0xf];
	return Tcl_NewStringObj(c, 18);
    case TCL_LINK_BITARRAY8:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned char *puc = linkPtr->lastValue.p;
	    memcpy(puc, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned char);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		for (j = 0; j < 8; j++) {
		    c[j] = (puc[i] & (1 << (7 - j))) ? '1' : '0';
		}
		objv[i] = Tcl_NewStringObj(c, 8);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.i = LinkedVar(unsigned char);
	for (j = 0; j < 8; j++) {
	    c[j] = (linkPtr->lastValue.i & (1 << (7 - j))) ? '1' : '0';
	}
	return Tcl_NewStringObj(c, 8);
    case TCL_LINK_BITARRAY16:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned short *pus = linkPtr->lastValue.p;
	    memcpy(pus, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned short);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		for (j = 0; j < 16; j++) {
		    c[j] = (pus[i] & (1 << (15 - j))) ? '1' : '0';
		}
		objv[i] = Tcl_NewStringObj(c, 16);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned short);
	for (j = 0; j < 16; j++) {
	    c[j] = (linkPtr->lastValue.ui & (1 << (15 - j))) ? '1' : '0';
	}
	return Tcl_NewStringObj(c, 16);
    case TCL_LINK_BITARRAY32:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned int *pui = linkPtr->lastValue.p;
	    memcpy(pui, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned int);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		for (j = 0; j < 32; j++) {
		    c[j] = (pui[i] & (1 << (31 - j))) ? '1' : '0';
		}
		objv[i] = Tcl_NewStringObj(c, 32);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned int);
	for (j = 0; j < 32; j++) {
	    c[j] = (linkPtr->lastValue.ui & (1 << (31 - j))) ? '1' : '0';
	}
	return Tcl_NewStringObj(c, 32);
    case TCL_LINK_BITARRAY64:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    Tcl_WideUInt *puw = linkPtr->lastValue.p;
	    memcpy(puw, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(Tcl_WideUInt);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		for (j = 0; j < 64; j++) {
		    c[j] = (puw[i] & (1 << (63 - j))) ? '1' : '0';
		}
		objv[i] = Tcl_NewStringObj(c, 64);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.uw = LinkedVar(Tcl_WideUInt);
	for (j = 0; j < 64; j++) {
	    c[j] = (linkPtr->lastValue.uw & (1 << (63 - j))) ? '1' : '0';
	}
	return Tcl_NewStringObj(c, 64);
    case TCL_TYPE_B(char):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned char *puc = linkPtr->lastValue.p;
	    memcpy(puc, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned char);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewBooleanObj(puc[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.i = LinkedVar(unsigned char);
	return Tcl_NewBooleanObj(linkPtr->lastValue.i);
    case TCL_TYPE_B(short):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned short *pus = linkPtr->lastValue.p;
	    memcpy(pus, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned short);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewBooleanObj(pus[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned short);
	return Tcl_NewBooleanObj(linkPtr->lastValue.ui);
    case TCL_TYPE_B(int):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned int *pui = linkPtr->lastValue.p;
	    memcpy(pui, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned int);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewBooleanObj(pui[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned int);
	return Tcl_NewBooleanObj(linkPtr->lastValue.ui);
    case TCL_TYPE_B(Tcl_WideInt):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    Tcl_WideUInt *puw = linkPtr->lastValue.p;
	    memcpy(puw, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(Tcl_WideUInt);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewBooleanObj(puw[i]);
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.uw = LinkedVar(Tcl_WideUInt);
	return Tcl_NewBooleanObj(linkPtr->lastValue.uw);
    case TCL_LINK_BIT8:
	linkPtr->lastValue.i = LinkedVar(unsigned char);
	return Tcl_NewIntObj((linkPtr->lastValue.i & (1 << linkPtr->bytes)) ? 1 : 0);
    case TCL_LINK_BIT16:
	linkPtr->lastValue.ui = LinkedVar(unsigned short);
	return Tcl_NewIntObj((linkPtr->lastValue.ui & (1 << linkPtr->bytes)) ? 1 : 0);
    case TCL_LINK_BIT32:
	linkPtr->lastValue.ui = LinkedVar(unsigned int);
	return Tcl_NewIntObj((linkPtr->lastValue.ui & (1 << linkPtr->bytes)) ? 1 : 0);
    case TCL_LINK_BIT64:
	linkPtr->lastValue.uw = LinkedVar(Tcl_WideUInt);
	return Tcl_NewIntObj((linkPtr->lastValue.uw & (1 << linkPtr->bytes)) ? 1 : 0);
    case TCL_LINK_S5FLOAT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned int *pui = linkPtr->lastValue.p;
	    memcpy(pui, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned int);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewDoubleObj(S5ToFloat(pui[i]));
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned int);
	return Tcl_NewDoubleObj(S5ToFloat(linkPtr->lastValue.ui));
    case TCL_LINK_S5TIME:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned short *pus = linkPtr->lastValue.p;
	    memcpy(pus, linkPtr->addr, linkPtr->bytes);
	    objc = linkPtr->bytes / sizeof(unsigned short);
	    objv = (Tcl_Obj **) ckrealloc((char *) objv, objc * sizeof(Tcl_Obj *));
	    for (i = 0; i < objc; i++) {
		objv[i] = Tcl_NewDoubleObj(S5timeToFloat(pus[i]));
	    }
	    return Tcl_NewListObj(objc, objv);
	}
	linkPtr->lastValue.ui = LinkedVar(unsigned short);
	return Tcl_NewDoubleObj(S5timeToFloat((unsigned short)linkPtr->lastValue.ui));
	/*
	 * This code only gets executed if the link type is unknown (shouldn't
	 * ever happen).
	 */
    default:
	resultObj = Tcl_NewStringObj("??", 2);
	return resultObj;
    }
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
    ClientData clientData,	/* Contains information about the link. */
    Tcl_Interp *interp,		/* Interpreter containing Tcl variable. */
    const char *name1,		/* First part of variable name. */
    const char *name2,		/* Second part of variable name. */
    int flags)			/* Miscellaneous additional information. */
{
    Link *linkPtr = clientData;
    int changed;
    size_t valueLength;
    const char *value;
    char **pp;
    Tcl_Obj *valueObj;
    int valueInt;
    Tcl_WideInt valueWide;
    double valueDouble;
    int objc;
    Tcl_Obj **objv;
    int i;
    int j;

    /*
     * If the variable is being unset, then just re-create it (with a trace)
     * unless the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if (Tcl_InterpDeleted(interp)) {
	    Tcl_DecrRefCount(linkPtr->varName);
	    LinkFree(linkPtr);
	} else if (flags & TCL_TRACE_DESTROYED) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    Tcl_TraceVar2(interp, Tcl_GetString(linkPtr->varName), NULL,
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
	/* variable arrays and TCL_TYPE_C() */
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    changed = memcmp(linkPtr->addr, linkPtr->lastValue.p, linkPtr->bytes);
	    /* single variables */
	} else {
	    switch (linkPtr->type) {
	    case TCL_LINK_CHARS:
	    case TCL_LINK_STRING:
	    case TCL_LINK_BINARY:
		changed = 1;
		break;
	    case TCL_LINK_CHAR:
		changed = (LinkedVar(char) != (char)linkPtr->lastValue.i);
		break;
	    case TCL_LINK_UCHAR:
	    case TCL_TYPE_X(char):
	    case TCL_LINK_BITARRAY8:
	    case TCL_TYPE_B(char):
		changed = (LinkedVar(unsigned char) != (unsigned char)linkPtr->lastValue.i);
		break;
	    case TCL_LINK_SHORT:
		changed = (LinkedVar(short) != (short)linkPtr->lastValue.i);
		break;
	    case TCL_LINK_USHORT:
	    case TCL_TYPE_X(short):
	    case TCL_LINK_BITARRAY16:
	    case TCL_TYPE_B(short):
	    case TCL_LINK_S5TIME:
		changed = (LinkedVar(unsigned short) != (unsigned short)linkPtr->lastValue.ui);
		break;
	    case TCL_LINK_INT:
		changed = (LinkedVar(int) != linkPtr->lastValue.i);
		break;
	    case TCL_LINK_UINT:
	    case TCL_TYPE_X(int):
	    case TCL_LINK_BITARRAY32:
	    case TCL_TYPE_B(int):
	    case TCL_LINK_S5FLOAT:
		changed = (LinkedVar(unsigned int) != linkPtr->lastValue.ui);
		break;
	    case TCL_LINK_WIDE_INT:
		changed = (LinkedVar(Tcl_WideInt) != linkPtr->lastValue.w);
		break;
	    case TCL_LINK_WIDE_UINT:
	    case TCL_TYPE_X(Tcl_WideInt):
	    case TCL_LINK_BITARRAY64:
	    case TCL_TYPE_B(Tcl_WideInt):
		changed = (LinkedVar(Tcl_WideUInt) != linkPtr->lastValue.uw);
		break;
	    case TCL_LINK_FLOAT:
		changed = (LinkedVar(float) != linkPtr->lastValue.f);
		break;
	    case TCL_LINK_DOUBLE:
		changed = (LinkedVar(double) != linkPtr->lastValue.d);
		break;
	    case TCL_LINK_DOUBLE+2:
	    case TCL_LINK_DOUBLE+4:
	    case TCL_LINK_DOUBLE+8:
		changed = (LinkedVar(long double) != linkPtr->lastValue.ld);
		break;
	    case TCL_LINK_BIT8:
		changed = (LinkedVar(unsigned char) | (1 << linkPtr->bytes)) !=
			((unsigned char)linkPtr->lastValue.i | (1 << linkPtr->bytes));
		break;
	    case TCL_LINK_BIT16:
		changed = (LinkedVar(unsigned short) | (1 << linkPtr->bytes)) !=
			((unsigned short)linkPtr->lastValue.ui | (1 << linkPtr->bytes));
		break;
	    case TCL_LINK_BIT32:
		changed = (LinkedVar(unsigned int) | (1 << linkPtr->bytes)) !=
			(linkPtr->lastValue.ui | (1 << linkPtr->bytes));
		break;
	    case TCL_LINK_BIT64:
		changed = (LinkedVar(Tcl_WideUInt) | (1 << linkPtr->bytes)) !=
			(linkPtr->lastValue.uw | (1 << linkPtr->bytes));
		break;
	    default:
		changed = 0;
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
	return (char *)"linked variable is read-only";
    }
    valueObj = Tcl_ObjGetVar2(interp, linkPtr->varName, NULL, TCL_GLOBAL_ONLY);
    if (valueObj == NULL) {
	/*
	 * This shouldn't ever happen.
	 */
	return (char *)"internal error: linked variable couldn't be read";
    }

    switch (linkPtr->type) {
    case TCL_LINK_CHAR:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    char *pc = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(char)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if ((Tcl_GetIntFromObj(NULL, objv[i], &valueInt) != TCL_OK
			|| valueInt < SCHAR_MIN || valueInt > SCHAR_MAX)
			&& GetInvalidIntFromObj(objv[i], &valueInt) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have char values";
		}
		pc[i] = (char) valueInt;
	    }
	    memcpy(linkPtr->addr, pc, linkPtr->bytes);
	    break;
	}
	if ((Tcl_GetIntFromObj(interp, valueObj, &valueInt) != TCL_OK
		|| valueInt < SCHAR_MIN || valueInt > SCHAR_MAX)
		&& GetInvalidIntFromObj(valueObj, &valueInt) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have char value";
	}
	LinkedVar(char) = linkPtr->lastValue.i = valueInt;
	break;
    case TCL_LINK_UCHAR:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned char *puc = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned char)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if ((Tcl_GetIntFromObj(NULL, objv[i], &valueInt) != TCL_OK
			|| valueInt < 0 || valueInt > UCHAR_MAX)
			 && GetInvalidIntFromObj(objv[i], &valueInt) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have unsigned char values";
		}
		puc[i] = (unsigned char) valueInt;
	    }
	    memcpy(linkPtr->addr, puc, linkPtr->bytes);
	    break;
	}
	if ((Tcl_GetIntFromObj(interp, valueObj, &valueInt) != TCL_OK
		|| valueInt < 0 || valueInt > UCHAR_MAX)
		&& GetInvalidIntFromObj(valueObj, &valueInt) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have unsigned char value";
	}
	linkPtr->lastValue.i = valueInt;
	LinkedVar(unsigned char) = (unsigned char)linkPtr->lastValue.i;
	break;
    case TCL_LINK_SHORT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    short *ps = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(short)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if ((Tcl_GetIntFromObj(NULL, objv[i], &valueInt) != TCL_OK
			|| valueInt < SHRT_MIN || valueInt > SHRT_MAX)
			&& GetInvalidIntFromObj(objv[i], &valueInt) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have short values";
		}
		ps[i] = (short) valueInt;
	    }
	    memcpy(linkPtr->addr, ps, linkPtr->bytes);
	    break;
	}
	if ((Tcl_GetIntFromObj(interp, valueObj, &valueInt) != TCL_OK
		|| valueInt < SHRT_MIN || valueInt > SHRT_MAX)
		&& GetInvalidIntFromObj(valueObj, &valueInt) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have short value";
	}
	linkPtr->lastValue.i = valueInt;
	LinkedVar(short) = (short) linkPtr->lastValue.i;
	break;
    case TCL_LINK_USHORT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned short *pus = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned short)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if ((Tcl_GetIntFromObj(NULL, objv[i], &valueInt) != TCL_OK
			|| valueInt < 0 || valueInt > USHRT_MAX)
			&& GetInvalidIntFromObj(objv[i], &valueInt) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have unsigned short values";
		}
		pus[i] = (unsigned short) valueInt;
	    }
	    memcpy(linkPtr->addr, pus, linkPtr->bytes);
	    break;
	}
	if ((Tcl_GetIntFromObj(interp, valueObj, &valueInt) != TCL_OK
		|| valueInt < 0 || valueInt > USHRT_MAX)
		&& GetInvalidIntFromObj(valueObj, &valueInt) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have unsigned short value";
	}
	linkPtr->lastValue.ui = valueInt;
	LinkedVar(unsigned short) = (unsigned short) linkPtr->lastValue.ui;
	break;
    case TCL_LINK_INT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    int *pi = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(int)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetIntFromObj(NULL, objv[i], &pi[i]) != TCL_OK
			&& GetInvalidIntFromObj(objv[i], &pi[i]) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have integer values";
		}
	    }
	    memcpy(linkPtr->addr, pi, linkPtr->bytes);
	    break;
	}
	if (Tcl_GetIntFromObj(NULL, valueObj, &linkPtr->lastValue.i) != TCL_OK
		&& GetInvalidIntFromObj(valueObj, &linkPtr->lastValue.i) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		TCL_GLOBAL_ONLY);
	    return (char *)"variable must have integer value";
	}
	LinkedVar(int) = linkPtr->lastValue.i;
	break;
    case TCL_LINK_UINT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned int *pui = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned int)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if ((Tcl_GetWideIntFromObj(NULL, objv[i], &valueWide) != TCL_OK
			|| valueWide < 0 || valueWide > UINT_MAX)
			&& GetInvalidWideFromObj(objv[i], &valueWide) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have unsigned integer values";
		}
		pui[i] = (unsigned int) valueWide;
	    }
	    memcpy(linkPtr->addr, pui, linkPtr->bytes);
	    break;
	}
	if ((Tcl_GetWideIntFromObj(interp, valueObj, &valueWide) != TCL_OK
		|| valueWide < 0 || valueWide > UINT_MAX)
		&& GetInvalidWideFromObj(valueObj, &valueWide) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have unsigned integer value";
	}
	linkPtr->lastValue.ui = (unsigned int) valueWide;
	LinkedVar(unsigned int) = linkPtr->lastValue.ui;
	break;
    case TCL_LINK_WIDE_INT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    Tcl_WideInt *pw = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(Tcl_WideInt)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetWideIntFromObj(NULL, objv[i], &pw[i]) != TCL_OK
			&& GetInvalidWideFromObj(objv[i], &pw[i]) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have integer values";
		}
	    }
	    memcpy(linkPtr->addr, pw, linkPtr->bytes);
	    break;
	}
	if (Tcl_GetWideIntFromObj(NULL, valueObj, &linkPtr->lastValue.w)
		&& GetInvalidWideFromObj(valueObj, &linkPtr->lastValue.w) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have integer value";
	}
	LinkedVar(Tcl_WideInt) = linkPtr->lastValue.w;
	break;
    case TCL_LINK_WIDE_UINT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    Tcl_WideUInt *puw = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(Tcl_WideUInt)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if ((Tcl_GetWideIntFromObj(NULL, objv[i], &valueWide) != TCL_OK
			|| valueWide < 0)
			&& GetInvalidWideFromObj(objv[i], &valueWide) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have Tcl_WideUInt values";
		}
		puw[i] = (Tcl_WideUInt) valueWide;
	    }
	    memcpy(linkPtr->addr, linkPtr->lastValue.p, linkPtr->bytes);
	    break;
	}
	if ((Tcl_GetWideIntFromObj(interp, valueObj, &valueWide) != TCL_OK
		|| valueWide < 0)
		&& GetInvalidWideFromObj(valueObj, &valueWide) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have Tcl_WideUInt value";
	}
	linkPtr->lastValue.uw = (Tcl_WideUInt) valueWide;
	LinkedVar(Tcl_WideUInt) = linkPtr->lastValue.uw;
	break;
    case TCL_LINK_FLOAT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    float *pf = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(float)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if ((Tcl_GetDoubleFromObj(interp, objv[i], &valueDouble) != TCL_OK
			|| valueDouble < -FLT_MAX || valueDouble > FLT_MAX)
			&& GetInvalidDoubleFromObj(objv[i], &valueDouble) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have float values";
		}
		pf[i] = (float) valueDouble;
	    }
	    memcpy(linkPtr->addr, linkPtr->lastValue.p, linkPtr->bytes);
	    break;
	}
	if ((Tcl_GetDoubleFromObj(interp, valueObj, &valueDouble) != TCL_OK
		|| valueDouble < -FLT_MAX || valueDouble > FLT_MAX)
		&& GetInvalidDoubleFromObj(valueObj, &valueDouble) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have float value";
	}
	linkPtr->lastValue.f = (float) valueDouble;
	LinkedVar(float) = linkPtr->lastValue.f;
	break;
    case TCL_LINK_DOUBLE:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    double *pd = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(double)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetDoubleFromObj(NULL, objv[i], &pd[i]) != TCL_OK
			&& GetInvalidDoubleFromObj(objv[i], &pd[i]) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
			    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have real values";
		}
	    }
	    memcpy(linkPtr->addr, pd, linkPtr->bytes);
	    break;
	}
	if (Tcl_GetDoubleFromObj(NULL, valueObj, &linkPtr->lastValue.d) != TCL_OK
		&& GetInvalidDoubleFromObj(valueObj, &linkPtr->lastValue.d) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL,
		    ObjValue(linkPtr), TCL_GLOBAL_ONLY);
	    return (char *)"variable must have real value";
	}
	LinkedVar(double) = linkPtr->lastValue.d;
	break;
    case TCL_LINK_DOUBLE+2:
    case TCL_LINK_DOUBLE+4:
    case TCL_LINK_DOUBLE+8:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    long double *pld = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(long double)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if ((Tcl_GetDoubleFromObj(interp, objv[i], &valueDouble) != TCL_OK)
			&& GetInvalidDoubleFromObj(objv[i], &valueDouble) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have double values";
		}
		pld[i] = (long double) valueDouble;
	    }
	    memcpy(linkPtr->addr, pld, linkPtr->bytes);
	    break;
	}
	if ((Tcl_GetDoubleFromObj(interp, valueObj, &valueDouble) != TCL_OK)
		&& GetInvalidDoubleFromObj(valueObj, &valueDouble) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have double values";
	}
	linkPtr->lastValue.ld = valueDouble;
	LinkedVar(long double) = linkPtr->lastValue.ld;
	break;
    case TCL_TYPE_C(float): {
	    float *pf = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(float)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetDoubleFromObj(interp, objv[i], &valueDouble) != TCL_OK
			|| valueDouble < -FLT_MAX || valueDouble > FLT_MAX) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have float values";
		}
		pf[i] = (float) valueDouble;
	    }
	    memcpy(linkPtr->addr, pf, linkPtr->bytes);
	    break;
	}
    case TCL_TYPE_C(double): {
	    double *pd = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(double)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetDoubleFromObj(interp, objv[i], &valueDouble) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have double values";
		}
		pd[i] = valueDouble;
	    }
	    memcpy(linkPtr->addr, pd, linkPtr->bytes);
	    break;
	}
    case TCL_TYPE_C(double)+2:
    case TCL_TYPE_C(double)+4:
    case TCL_TYPE_C(double)+8: {
	    long double *pld = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(long double)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetDoubleFromObj(interp, objv[i], &valueDouble) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have double values";
		}
		pld[i] = valueDouble;
	    }
	    memcpy(linkPtr->addr, pld, linkPtr->bytes);
	    break;
	}
    case TCL_LINK_STRING:
	value = TclGetString(valueObj);
	valueLength = valueObj->length + 1;
	pp = (char **) linkPtr->addr;
	*pp = ckrealloc(*pp, valueLength);
	memcpy(*pp, value, valueLength);
	break;
    case TCL_LINK_CHARS:
	value = (char *) Tcl_GetString(valueObj);
	valueLength = valueObj->length + 1; /* include end of string char */
	if (valueLength > linkPtr->bytes) {
	    return (char *)"wrong size of char* value";
	}
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.p, value, valueLength);
	    memcpy(linkPtr->addr, value, valueLength);
	} else {
	    LinkedVar(char) = linkPtr->lastValue.i = '\0';
	}
	break;
    case TCL_LINK_BINARY:
	value = (char *) Tcl_GetByteArrayFromObj(valueObj, &i);
	if ((size_t)i != linkPtr->bytes) {
	    return (char *)"wrong size of binary value";
	}
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    memcpy(linkPtr->lastValue.p, value, (size_t) i);
	    memcpy(linkPtr->addr, value, (size_t) i);
	} else {
	    linkPtr->lastValue.i = (unsigned char) *value;
	    LinkedVar(unsigned char) = (unsigned char) linkPtr->lastValue.i;
	}
	break;
    case TCL_TYPE_X(char): {
        unsigned char uc;
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned char *puc = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned char)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		value = TclGetString(objv[i]);
		valueLength = objv[i]->length;
		if (valueLength != 4 || value[0]!='0' || value[1]!='x'
			|| (HexTo8bit(value+2, &puc[i])) < 0) {
		    return (char *)"variable array must have '0x' and 2 hex chars";
		}
	    }
	    memcpy(linkPtr->addr, puc, linkPtr->bytes);
	    break;
	}
	value = TclGetString(valueObj);
	valueLength = valueObj->length;
	if (valueLength != 4 || value[0]!='0' || value[1]!='x'
		|| HexTo8bit(value+2, &uc) < 0) {
	      return (char *)"variable must have '0x' and 2 hex chars";
	}
	linkPtr->lastValue.i = uc;
	LinkedVar(unsigned char) = (unsigned char)linkPtr->lastValue.i;
	break;
	}
    case TCL_TYPE_X(short): {
        unsigned short us;
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned short *pus = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned short)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		value = TclGetString(objv[i]);
		valueLength = objv[i]->length;
		if (valueLength != 6 || value[0]!='0' || value[1]!='x'
			|| (HexTo16bit(value+2, &pus[i])) < 0) {
		    return (char *)"variable array must have '0x' and 4 hex chars";
		}
	    }
	    memcpy(linkPtr->addr, pus, linkPtr->bytes);
	    break;
	}
	value = TclGetString(valueObj);
	valueLength = valueObj->length;
	if (valueLength != 6 || value[0]!='0' || value[1]!='x'
		|| HexTo16bit(value+2, &us) < 0) {
	    return (char *)"variable must have '0x' and 4 hex chars";
	}
	linkPtr->lastValue.ui = us;
	LinkedVar(unsigned short) = (unsigned short)linkPtr->lastValue.ui;
	break;
	}
    case TCL_TYPE_X(int):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned int *pui = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned int)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		value = TclGetString(objv[i]);
		valueLength = objv[i]->length;
		if (valueLength != 10 || value[0]!='0' || value[1]!='x'
			|| (HexTo32bit(value+2, &pui[i])) < 0) {
		    return (char *)"variable array must have '0x' and 8 hex chars";
		}
	    }
	    memcpy(linkPtr->addr, pui, linkPtr->bytes);
	    break;
	}
	value = TclGetString(valueObj);
	valueLength = valueObj->length;
	if (valueLength != 10 || value[0]!='0' || value[1]!='x'
		|| HexTo32bit(value+2, &linkPtr->lastValue.ui) < 0) {
	    return (char *)"variable must have '0x' and 8 hex chars";
	}
	LinkedVar(unsigned int) = linkPtr->lastValue.ui;
	break;
    case TCL_TYPE_X(Tcl_WideInt):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    Tcl_WideUInt *puw = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(Tcl_WideUInt)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		value = TclGetString(objv[i]);
		valueLength = objv[i]->length;
		if (valueLength != 18 || value[0]!='0' || value[1]!='x'
			|| (HexTo64bit(value+2, &puw[i])) < 0) {
		    return (char *)"variable array must have '0x' and 16 hex chars";
		}
	    }
	    memcpy(linkPtr->addr, puw, linkPtr->bytes);
	    break;
	}
	value = TclGetString(valueObj);
	valueLength = valueObj->length;
	if (valueLength != 18 || value[0]!='0' || value[1]!='x'
		|| HexTo64bit(value+2, &linkPtr->lastValue.uw) < 0) {
	    return (char *)"variable must have '0x' and 16 hex chars";
	}
	LinkedVar(Tcl_WideUInt) = linkPtr->lastValue.uw;
	break;
    case TCL_LINK_BITARRAY8:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned char *puc = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned char)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		value = TclGetString(objv[i]);
		valueLength = objv[i]->length;
		if (valueLength != 8) {
		    return (char *)"variable array must be 8 chars long";
		}
		for (j = 0; j < 8; j++) {
		    if (value[j] == '0' || value[j] == 'f' || value[j] == 'F') {
			puc[i] &= ~(1 << (7 - j));
		    } else {
			puc[i] |= (1 << (7 - j));
		    }
		}
	    }
	    memcpy(linkPtr->addr, puc, linkPtr->bytes);
	    break;
	}
	value = Tcl_GetString(valueObj);
	valueLength = valueObj->length;
	if (valueLength != 8) {
	      return (char *)"variable array must be 8 chars long";
	}
	for (j = 0; j < 8; j++) {
	    if (value[j] == '0' || value[j] == 'f' || value[j] == 'F') {
		linkPtr->lastValue.i &= ~(1 << (7 - j));
	    } else {
		linkPtr->lastValue.i |= (1 << (7 - j));
	    }
	}
	LinkedVar(unsigned char) = (unsigned char)linkPtr->lastValue.i;
	break;
    case TCL_LINK_BITARRAY16:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned short *pus = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned short)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		value = TclGetString(objv[i]);
		valueLength = objv[i]->length;
		if (valueLength != 16) {
		    return (char *)"variable array must be 16 chars long";
		}
		for (j = 0; j < 16; j++) {
		    if (value[j] == '0' || value[j] == 'f' || value[j] == 'F') {
			pus[i] &= ~(1 << (15 - j));
		    } else {
			pus[i] |= (1 << (15 - j));
		    }
		}
	    }
	    memcpy(linkPtr->addr, pus, linkPtr->bytes);
	    break;
	}
	value = Tcl_GetString(valueObj);
	valueLength = valueObj->length;
	if (valueLength != 16) {
	      return (char *)"variable array must be 16 chars long";
	}
	for (j = 0; j < 16; j++) {
	    if (value[j] == '0' || value[j] == 'f' || value[j] == 'F') {
		linkPtr->lastValue.ui &= ~(1 << (15 - j));
	    } else {
		linkPtr->lastValue.ui |= (1 << (15 - j));
	    }
	}
	LinkedVar(unsigned short) = (unsigned short)linkPtr->lastValue.ui;
	break;
    case TCL_LINK_BITARRAY32:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned int *pui = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned int)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		value = Tcl_GetString(objv[i]);
		valueLength = objv[i]->length;
		if (valueLength != 32) {
		    return (char *)"variable array must be 32 chars long";
		}
		for (j = 0; j < 32; j++) {
		    if (value[j] == '0' || value[j] == 'f' || value[j] == 'F') {
			pui[i] &= ~(1 << (31 - j));
		    } else {
			pui[i] |= (1 << (31 - j));
		    }
		}
	    }
	    memcpy(linkPtr->addr, pui, linkPtr->bytes);
	    break;
	}
	value = Tcl_GetString(valueObj);
	valueLength = valueObj->length;
	if (valueLength != 32) {
	    return (char *)"variable array must be 32 chars long";
	}
	for (j = 0; j < 32; j++) {
	    if (value[j] == '0' || value[j] == 'f' || value[j] == 'F') {
		linkPtr->lastValue.ui &= ~(1 << (31 - j));
	    } else {
		linkPtr->lastValue.ui |= (1 << (31 - j));
	    }
	}
	LinkedVar(unsigned int) = linkPtr->lastValue.ui;
	break;
    case TCL_LINK_BITARRAY64:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    Tcl_WideUInt *puw = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned int)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		value = Tcl_GetString(objv[i]);
		valueLength = objv[i]->length;
		if (valueLength != 64) {
		    return (char *)"variable array must be 64 chars long";
		}
		for (j = 0; j < 64; j++) {
		    if (value[j] == '0' || value[j] == 'f' || value[j] == 'F') {
			puw[i] &= ~(1 << (63 - j));
		    } else {
			puw[i] |= (1 << (63 - j));
		    }
		}
	    }
	    memcpy(linkPtr->addr, puw, linkPtr->bytes);
	    break;
	}
	value = Tcl_GetString(valueObj);
	valueLength = valueObj->length;
	if (valueLength != 64) {
	    return (char *)"variable array must be 64 chars long";
	}
	for (j = 0; j < 64; j++) {
	    if (value[j] == '0' || value[j] == 'f' || value[j] == 'F') {
		linkPtr->lastValue.uw &= ~(1 << (63 - j));
	    } else {
		linkPtr->lastValue.uw |= (1 << (63 - j));
	    }
	}
	LinkedVar(Tcl_WideUInt) = linkPtr->lastValue.uw;
	break;
    case TCL_TYPE_B(char):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned char *puc = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned char)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetBooleanFromObj(NULL, objv[i], &valueInt) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have boolean values";
		}
		puc[i] = (unsigned char) valueInt;
	    }
	    memcpy(linkPtr->addr, puc, linkPtr->bytes);
	    break;
	}
	if (Tcl_GetBooleanFromObj(NULL, valueObj, &valueInt) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have boolean value";
	}
	linkPtr->lastValue.i =  valueInt;
	LinkedVar(unsigned char) = (unsigned char)linkPtr->lastValue.i;
	break;
    case TCL_TYPE_B(short):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned short *pus = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned short)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetBooleanFromObj(NULL, objv[i], &valueInt) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have boolean values";
		}
		pus[i] = (unsigned short) valueInt;
	    }
	    memcpy(linkPtr->addr, pus, linkPtr->bytes);
	    break;
	}
	if (Tcl_GetBooleanFromObj(NULL, valueObj, &valueInt) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have boolean value";
	}
	linkPtr->lastValue.ui = valueInt;
	LinkedVar(unsigned short) = (unsigned short) linkPtr->lastValue.ui;
	break;
    case TCL_TYPE_B(int):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned int *pui = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned int)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetBooleanFromObj(NULL, objv[i], &valueInt) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have boolean values";
		}
		pui[i] = (unsigned int) valueInt;
	    }
	    memcpy(linkPtr->addr, pui, linkPtr->bytes);
	    break;
	}
	if (Tcl_GetBooleanFromObj(NULL, valueObj, &valueInt) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have boolean value";
	}
	linkPtr->lastValue.ui = (unsigned int) valueInt;
	LinkedVar(unsigned int) = linkPtr->lastValue.ui;
	break;
    case TCL_TYPE_B(Tcl_WideInt):
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    Tcl_WideUInt *puw = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(Tcl_WideUInt)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetBooleanFromObj(NULL, objv[i], &valueInt) != TCL_OK) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have boolean values";
		}
		puw[i] = (Tcl_WideUInt) valueInt;
	    }
	    memcpy(linkPtr->addr, puw, linkPtr->bytes);
	    break;
	}
	if (Tcl_GetBooleanFromObj(NULL, valueObj, &valueInt) != TCL_OK) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		TCL_GLOBAL_ONLY);
	    return (char *)"variable must have boolean value";
	}
	linkPtr->lastValue.uw = (Tcl_WideUInt) valueInt;
	LinkedVar(Tcl_WideUInt) = linkPtr->lastValue.uw;
	break;
    case TCL_LINK_BIT8:
	if (Tcl_GetBooleanFromObj(NULL, valueObj, &changed)
		!= TCL_OK) {
	    return (char *)"variable must have boolean value";
	}
	if (changed) {
	    linkPtr->lastValue.i |= (1 << linkPtr->bytes);
	} else {
	    linkPtr->lastValue.i &= ~(1 << linkPtr->bytes);
	}
	LinkedVar(unsigned char) = (unsigned char)linkPtr->lastValue.i;
	break;
    case TCL_LINK_BIT16:
	if (Tcl_GetBooleanFromObj(NULL, valueObj, &changed)
		!= TCL_OK) {
	    return (char *)"variable must have boolean value";
	}
	if (changed) {
	    linkPtr->lastValue.ui |= (1 << linkPtr->bytes);
	} else {
	    linkPtr->lastValue.ui &= ~(1 << linkPtr->bytes);
	}
	LinkedVar(unsigned short) = (unsigned short)linkPtr->lastValue.ui;
	break;
    case TCL_LINK_BIT32:
	if (Tcl_GetBooleanFromObj(NULL, valueObj, &changed)
		!= TCL_OK) {
	    return (char *)"variable must have boolean value";
	}
	if (changed) {
	    linkPtr->lastValue.ui |= (1 << linkPtr->bytes);
	} else {
	    linkPtr->lastValue.ui &= ~(1 << linkPtr->bytes);
	}
	LinkedVar(unsigned int) = linkPtr->lastValue.ui;
	break;
    case TCL_LINK_BIT64:
	if (Tcl_GetBooleanFromObj(NULL, valueObj, &changed)
		!= TCL_OK) {
	    return (char *)"variable must have boolean value";
	}
	if (changed) {
	    linkPtr->lastValue.uw |= (1 << linkPtr->bytes);
	} else {
	    linkPtr->lastValue.uw &= ~(1 << linkPtr->bytes);
	}
	LinkedVar(Tcl_WideUInt) = linkPtr->lastValue.uw;
	break;
    case TCL_LINK_S5FLOAT:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned int *pui = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned int)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetDoubleFromObj(interp, objv[i], &valueDouble) != TCL_OK
			|| valueDouble < -FLT_MAX || valueDouble > FLT_MAX) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have float values";
		}
		pui[i] = FloatToS5((float) valueDouble);
	    }
	    memcpy(linkPtr->addr, pui, linkPtr->bytes);
	    break;
	}
	if (Tcl_GetDoubleFromObj(interp, valueObj, &valueDouble) != TCL_OK
		|| valueDouble < -FLT_MAX || valueDouble > FLT_MAX) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have float value";
	}
	linkPtr->lastValue.ui = FloatToS5((float) valueDouble);
	LinkedVar(unsigned int) = linkPtr->lastValue.ui;
	break;
    case TCL_LINK_S5TIME:
	if (linkPtr->flags & LINK_ALLOC_LAST) {
	    unsigned short *pus = linkPtr->lastValue.p;
	    if (Tcl_ListObjGetElements(interp, valueObj, &objc, &objv) == TCL_ERROR
		    || (size_t)objc != linkPtr->bytes / sizeof(unsigned short)) {
		return (char *)"wrong dimension";
	    }
	    for (i = 0; i < objc; i++) {
		if (Tcl_GetDoubleFromObj(interp, objv[i], &valueDouble) != TCL_OK
			|| valueDouble < -FLT_MAX || valueDouble > FLT_MAX) {
		    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
			    TCL_GLOBAL_ONLY);
		    return (char *)"variable array must have float values";
		}
		pus[i] = FloatToS5time(pus[i], (float) valueDouble);
	    }
	    memcpy(linkPtr->addr, pus, linkPtr->bytes);
	    break;
	}
	if (Tcl_GetDoubleFromObj(interp, valueObj, &valueDouble) != TCL_OK
		|| valueDouble < -FLT_MAX || valueDouble > FLT_MAX) {
	    Tcl_ObjSetVar2(interp, linkPtr->varName, NULL, ObjValue(linkPtr),
		    TCL_GLOBAL_ONLY);
	    return (char *)"variable must have float value";
	}
	linkPtr->lastValue.ui =
		FloatToS5time((unsigned short)linkPtr->lastValue.ui, (float) valueDouble);
	LinkedVar(unsigned short) = (unsigned short) linkPtr->lastValue.ui;
	break;
    default:
	    return (char *)"internal error: bad linked variable type";
    }
    return NULL;
}

/* Internal ieee type. */
typedef union
{
    unsigned int	bits;
    float		value;
} _Ieee_t;

/*
 *----------------------------------------------------------------------
 *
 * S5ToFloat --
 *	Convert given Siemens S5 value in IEEE float format.
 *
 * Result:
 *	IEEE format float value.
 *
 * Side effects:
 * 	None
 *----------------------------------------------------------------------
 */

static float
S5ToFloat(
    unsigned int value)	/* Float value in Siemens S5 format to convert. */
{
    unsigned int	S5Bits;
    _Ieee_t		ieee;
    unsigned int	ieee_mantisse, ieee_exponent, ieee_sign;
    unsigned int	S5mantisse, S5exponent, S5exposign;

    S5Bits = ((unsigned int) (((*(unsigned char *) (&value) * 256 +
	*((unsigned char *) (&value) + 1)) * 256 +
	*((unsigned char *) (&value) + 2)) * 256 +
	*((unsigned char *) (&value) + 3)));
    if (S5Bits == 0x80000000u) {
	ieee.bits = 0x0u;
    } else {
	if (S5Bits != 0x00000000u) {/*special handling, otherwise we get "0.5"*/
	    S5exponent = (S5Bits & 0x7f000000u) >> 24;
	    S5exposign = S5Bits & 0x80000000u;
	    S5mantisse = S5Bits & 0x007fffffu;
	    ieee_sign = (S5Bits & 0x00800000u) << 8;
	    if (!S5mantisse && ieee_sign) {
		S5mantisse = 0x00000001u;
	    }
	    if (S5exposign) {
		S5exponent = -((~S5exponent + 1) & 0x0000007fu);
	    }
	    ieee_exponent = S5exponent + 126;
	    if (ieee_sign) {
		S5mantisse = (~S5mantisse + 1) & 0x007fffffu;
	    }
	    ieee_mantisse = (S5mantisse & 0xffbfffff) << 1;
	    ieee_exponent = (ieee_exponent & 0xff) << 23;
	    ieee.bits = ieee_sign | ieee_exponent | ieee_mantisse;
	} else { /* when we get only zeros */
	    ieee.value = 0.0;
	}
    }
/*TODO    if (isnanf(ieee.value)) {
	ieee.value = 0.0;
    }
*/
    return ieee.value;
}

/*
 *----------------------------------------------------------------------
 *
 * FloatToS5 --
 *	Convert given Siemens S5 value in IEEE float value in Siemens S5 float.
 *
 * Result:
 *	Siemens S5 float value.
 *
 * Side effects:
 * 	None
 *----------------------------------------------------------------------
 */

static unsigned int
FloatToS5(
    float value)	/* Float value to convert. */
{
    _Ieee_t		ieee;
    unsigned int	ieee_mantisse, ieee_exponent;
    unsigned int	S5Bits, S5sign, S5mantisse, S5exponent, S5exposign;
    unsigned int	RetVal;

    ieee.value = value;

    if (!ieee.bits) {
	S5Bits = 0x80000000u;
    } else {
	ieee_mantisse = ieee.bits & 0x007fffffu;
	ieee_exponent = (ieee.bits >> 23) & 0x000000ffu;
	S5sign = (ieee.bits >> 8) & 0x00800000u;

	if (!ieee_mantisse && S5sign) {
	    S5mantisse = 0x0u;
	    S5exponent = ieee_exponent + 1;
	} else {
	    S5mantisse = (ieee_mantisse >> 1) | 0x00400000u;
	    S5exponent = ieee_exponent + 2;
	    if (S5sign) {
		S5mantisse = (~S5mantisse + 1) & 0x007fffffu;
	    }
	}

	if (S5exponent & 0x00000080u) {
	    S5exposign = 0x0u;
	} else {
	    S5exposign = 0x80000000u;
	}

	S5exponent = (S5exponent << 24) & 0x7f000000u;

	S5Bits = S5exposign | S5exponent | S5sign | S5mantisse;
    }

    *(char *) (&RetVal) = (char) ((unsigned int) (S5Bits) / 0x1000000 & 0xff);
    *((char *) (&RetVal)+1) = (char) ((unsigned int) (S5Bits) / 0x10000 & 0xff);
    *((char *) (&RetVal)+2) = (char) ((unsigned int) (S5Bits) / 0x100 & 0xff);
    *((char *) (&RetVal)+3) = (char) ((S5Bits) & 0xff);

    return RetVal;
}

/*
 *----------------------------------------------------------------------
 *
 * S5timeToFloat --
 * 	Convert Siemens S5 time value in IEEE float value.
 *
 * Result:
 *	Float time value in seconds.
 *
 * Side effects:
 * 	None.
 *----------------------------------------------------------------------
 */

static float
S5timeToFloat(
    unsigned short value) /*S5 format time value to convert. */
{
    float RetVal;

    switch ((value & 0x3000) >> 12) {
    case 0:
	RetVal = 0.01 * (float) (value & 0x03ff);
	break;
    case 1:
	RetVal = 0.1 * (float) (value & 0x03ff);
	break;
    case 2:
	RetVal = (float) (value & 0x03ff);
	break;
    default: /*=3*/
	RetVal = 10.0 * (float) (value & 0x03ff);
	break;
    }
    return RetVal;
}

/*
 *----------------------------------------------------------------------
 *
 * FloatToS5time --
 * 	Convert IEEE float value in Siemens S5 time value.
 *
 * Result:
 *	S5 format time value.
 *
 * Side effects:
 * 	None.
 *----------------------------------------------------------------------
 */

static unsigned short
FloatToS5time(
    unsigned short basis, /* S5 format time value used to detect time range. */
    float value)		/* Float time value in seconds to convert. */
{
    unsigned short	RetVal = (basis & 0xf000);
    unsigned short	shortvalue;

    switch ((RetVal & 0x3000) >> 12) {
    case 0:
	if (value < 0.01) return RetVal;
	shortvalue = (unsigned short) (value * 100.);
	break;
    case 1:
	if (value < 0.1) return RetVal;
	shortvalue = (unsigned short) (value * 10.);
	break;
    case 2:
	if (value < 1.) return RetVal;
    shortvalue = (unsigned short) value;
    break;
    default: /*=3*/
	if (value < 10.) return RetVal;
	shortvalue = (unsigned short) (value / 10.);
	break;
    }
    RetVal |= (shortvalue & 0x03ff);
    return (RetVal);
}

/*
 *----------------------------------------------------------------------
 *
 * HexToInt --
 * 	Get integer value of given hex string value.
 *
 * Result:
 *	Integer value of given hex character. -1 in case of error.
 *
 * Side effects:
 * 	None.
 *----------------------------------------------------------------------
 */

static int
Hex2Int(
    const char ch)	/* hex string value to convert. */
{
    switch (ch) {
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'A':
	case 'a': return 10;
	case 'B':
	case 'b': return 11;
	case 'C':
	case 'c': return 12;
	case 'D':
	case 'd': return 13;
	case 'E':
	case 'e': return 14;
	case 'F':
	case 'f': return 15;
	default: return -1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * HexTo8bit --
 * 	Convert 2 hex chars in unsigned char integer value.
 *
 * Result:
 *	Return 0 on success and negative position in case of error.
 *
 * Side effects:
 * 	None.
 *----------------------------------------------------------------------
 */

static int
HexTo8bit(
    const char *ch,	/* 2 hex chars. */
    unsigned char *ret)	/* Pointer with converted value. */
{
    int Tmp0, Tmp1;

    if ((Tmp0 = Hex2Int(ch[0])) < 0) return -1;
    if ((Tmp1 = Hex2Int(ch[1])) < 0) return -2;
    *ret = (unsigned char) (Tmp1 + (Tmp0 << 4));
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * HexTo16bit --
 * 	Convert 4 hex chars in unsigned short integer value.
 *
 * Result:
 *	Return 0 on success and negative position in case of error.
 *
 * Side effects:
 * 	None.
 *----------------------------------------------------------------------
 */

static int
HexTo16bit(
    const char *ch,	/* 4 hex chars. */
    unsigned short *ret)	/* Pointer with converted value. */
{
    int			i;
    int			Tmp;
    unsigned short	Ret = 0;

    for (i = 0; i < 4; i++) {
	if ((Tmp = Hex2Int(ch[i])) < 0) return (-1 - i);
	Ret += (Tmp << (12 - 4 * i));
    }
    *ret = Ret;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * HexTo32bit --
 *
 * Result:
 *	Return 0 on success and negative position in case of error.
 *
 * Side effects:
 * 	None.
 *----------------------------------------------------------------------
 */

static int
HexTo32bit(
    const char *ch,	/* 8 hex chars. */
    unsigned int *ret)	/* Pointer with converted value. */
{
    int			i;
    int			Tmp;
    unsigned int	Ret = 0;

    for (i = 0; i < 8; i++) {
	if ((Tmp = Hex2Int(ch[i])) < 0) return (-1 - i);
	Ret += (Tmp << (28 - 4 * i));
    }
    *ret = Ret;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * HexTo64bit --
 *
 * Result:
 *	Return 0 on success and negative position in case of error.
 *
 * Side effects:
 * 	None.
 *----------------------------------------------------------------------
 */

static int
HexTo64bit(
    const char *ch,	/* 16 hex chars. */
    Tcl_WideUInt *ret)	/* Pointer with converted value. */
{
    int			i;
    int			Tmp;
    Tcl_WideUInt	Ret = 0;

    for (i = 0; i < 16; i++) {
	if ((Tmp = Hex2Int(ch[i])) < 0) return (-1 - i);
	Ret += (Tmp << (60 - 4 * i));
    }
    *ret = Ret;
    return 0;
}


static int SetInvalidRealFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);

static Tcl_ObjType invalidRealType = {
    "invalidReal",			/* name */
    NULL,				/* freeIntRepProc */
    NULL,				/* dupIntRepProc */
    NULL,				/* updateStringProc */
    SetInvalidRealFromAny		/* setFromAnyProc */
};

static int
SetInvalidRealFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr) {
    const char *str;
    const char *endPtr;

    str = TclGetString(objPtr);
    if ((objPtr->length == 1) && (str[0] == '.')){
	objPtr->typePtr = &invalidRealType;
	objPtr->internalRep.doubleValue = 0.0;
	return TCL_OK;
    }
    if (TclParseNumber(NULL, objPtr, NULL, str, objPtr->length, &endPtr,
	    TCL_PARSE_DECIMAL_ONLY) == TCL_OK) {
	/* If number is followed by [eE][+-]?, then it is an invalid
	 * double, but it could be the start of a valid double. */
	if (*endPtr == 'e' || *endPtr == 'E') {
	    ++endPtr;
	    if (*endPtr == '+' || *endPtr == '-') ++endPtr;
	    if (*endPtr == 0) {
		double doubleValue = 0.0;
		Tcl_GetDoubleFromObj(NULL, objPtr, &doubleValue);
		TclFreeIntRep(objPtr);
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
int
GetInvalidIntFromObj(Tcl_Obj *objPtr, int *intPtr)
{
    const char *str = TclGetString(objPtr);

    if ((objPtr->length == 0) ||
	    ((objPtr->length == 2) && (str[0] == '0') && strchr("xXbBoOdD", str[1]))) {
	*intPtr = 0;
	return TCL_OK;
    } else if ((objPtr->length == 1) && strchr("+-", str[0])) {
	*intPtr = (str[0] == '+');
	return TCL_OK;
    }
    return TCL_ERROR;
}

int
GetInvalidWideFromObj(Tcl_Obj *objPtr, Tcl_WideInt *widePtr)
{
    int intValue;

    if (GetInvalidIntFromObj(objPtr, &intValue) != TCL_OK) {
	return TCL_ERROR;
    }
    *widePtr = intValue;
    return TCL_OK;
}

/*
 * This function checks for double representations, which are valid
 * when linking with C variables, but which are invalid in other
 * contexts in Tcl. Handled are "+", "-", "", ".", "0x", "0b" and "0o"
 * (upper- and lowercase) and sequences like "1e-". See bug [39f6304c2e].
 */
int
GetInvalidDoubleFromObj(Tcl_Obj *objPtr, double *doublePtr)
{
    int intValue;

    if (objPtr->typePtr == &invalidRealType) {
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
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
