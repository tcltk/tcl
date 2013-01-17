/*
 * tclExecute.c --
 *
 *	This file contains procedures that execute byte-compiled Tcl commands.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 * Copyright (c) 2001 by Kevin B. Kenny. All rights reserved.
 * Copyright (c) 2002-2010 by Miguel Sofer.
 * Copyright (c) 2005-2007 by Donal K. Fellows.
 * Copyright (c) 2007 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright (c) 2006-2008 by Joe Mistachkin.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclCompileInt.h"
#include "tclCompExpr.h"
#include "tommath.h"
#include <math.h>
#include "tclNRE.h"
/*
 * Hack to determine whether we may expect IEEE floating point. The hack is
 * formally incorrect in that non-IEEE platforms might have the same precision
 * and range, but VAX, IBM, and Cray do not; are there any other floating
 * point units that we might care about?
 */

#if (FLT_RADIX == 2) && (DBL_MANT_DIG == 53) && (DBL_MAX_EXP == 1024)
#define IEEE_FLOATING_POINT
#endif

/*
 * A mask (should be 2**n-1) that is used to work out when the bytecode engine
 * should call Tcl_AsyncReady() to see whether there is a signal that needs
 * handling.
 */

#ifndef ASYNC_CHECK_COUNT_MASK
#   define ASYNC_CHECK_COUNT_MASK	63
#endif /* !ASYNC_CHECK_COUNT_MASK */

/*
 * Boolean flag indicating whether the Tcl bytecode interpreter has been
 * initialized.
 */

static int cachedInExit = 0;

/*
 * Mapping from expression instruction opcodes to strings; used for error
 * messages. Note that these entries must match the order and number of the
 * expression opcodes (e.g., INST_BITOR) in tclCompile.h.
 *
 * Does not include the string for INST_EXPON (and beyond), as that is
 * disjoint for backward-compatability reasons.
 */

static const char *const operatorStrings[] = {
    "|", "^", "&", "==", "!=", "<", ">", "<=", ">=", "<<", ">>",
    "+", "-", "*", "/", "%", "+", "-", "~", "!",
    "**", "eq", "ne", "in", "ni"
};

/*
 * Mapping from Tcl result codes to strings; used for error and debugging
 * messages.
 */

/*
 * These are used by evalstats to monitor object usage in Tcl.
 */


/*
 * NR_TEBC
 * Helpers for NR - non-recursive calls to TEBC
 * Minimal data required to fully reconstruct the execution state.
 */

typedef struct {
    ByteCode *codePtr;		/* Constant until the BC returns */
				/* -----------------------------------------*/
    Tcl_Obj **tosPtr;
    const unsigned char *pc;	/* These fields are used on return TO this */
    int cleanup;		/* new codePtr was received for NR */
    Tcl_Obj *auxObjList;	/* execution. */
    unsigned int capacity;
    void *stack[1];		/* Start of the actual obj stack; the struct
				 * will be expanded as necessary */
} TEBCdata;

#define TEBC_YIELD() \
    do {								\
	Tcl_NRAddCallback(interp, TEBCresume, TD, INT2PTR(1), data[2], NULL); \
    } while (0)

#define TEBC_DATA_DIG() \
    do {					\
    } while (0)

#define PUSH_TAUX_OBJ(objPtr) \
    do {							\
	objPtr->internalRep.ptrAndLongRep.ptr = auxObjList;	\
	auxObjList = objPtr;					\
    } while (0)

#define POP_TAUX_OBJ() \
    do {							\
	tmpPtr = auxObjList;			\
	auxObjList = tmpPtr->internalRep.ptrAndLongRep.ptr;	\
	Tcl_DecrRefCount(tmpPtr);				\
    } while (0)

/*
 * These variable-access macros have to coincide with those in tclVar.c
 */

#define VarHashGetValue(hPtr) \
    ((Var *) ((char *)hPtr - TclOffset(VarInHash, entry)))

static inline Var *
VarHashCreateVar(
    TclVarHashTable *tablePtr,
    Tcl_Obj *key,
    int *newPtr)
{
    Tcl_HashEntry *hPtr = Tcl_CreateHashEntry(&tablePtr->table,
	    key, newPtr);

    if (!hPtr) {
	return NULL;
    }
    return VarHashGetValue(hPtr);
}

#define VarHashFindVar(tablePtr, key) \
    VarHashCreateVar((tablePtr), (key), NULL)

/*
 * The new macro for ending an instruction; note that a reasonable C-optimiser
 * will resolve all branches at compile time. (result) is always a constant;
 * the macro NEXT_INST_F handles constant (nCleanup), NEXT_INST_V is resolved
 * at runtime for variable (nCleanup).
 *
 * ARGUMENTS:
 *    pcAdjustment: how much to increment pc
 *    nCleanup: how many objects to remove from the stack
 *    resultHandling: 0 indicates no object should be pushed on the stack;
 *	otherwise, push objResultPtr. If (result < 0), objResultPtr already
 *	has the correct reference count.
 *
 * We use the new compile-time assertions to check that nCleanup is constant
 * and within range.
 */

/* Verify the stack depth, only when no expansion is in progress */

#define NEXT_INST_F(pcAdjustment, nCleanup, resultHandling)	\
    do {							\
	TCL_CT_ASSERT((nCleanup >= 0) && (nCleanup <= 2));	\
	if (nCleanup == 0) {					\
	    if (resultHandling != 0) {				\
		if ((resultHandling) > 0) {			\
		    PUSH_OBJECT(objResultPtr);			\
		} else {					\
		    *(++tosPtr) = objResultPtr;			\
		}						\
	    }							\
	    pc += (pcAdjustment);				\
	    goto cleanup0;					\
	} else if (resultHandling != 0) {			\
	    if ((resultHandling) > 0) {				\
		Tcl_IncrRefCount(objResultPtr);			\
	    }							\
	    pc += (pcAdjustment);				\
	    switch (nCleanup) {					\
	    case 1: goto cleanup1_pushObjResultPtr;		\
	    case 2: goto cleanup2_pushObjResultPtr;		\
	    }							\
	} else {						\
	    pc += (pcAdjustment);				\
	    switch (nCleanup) {					\
	    case 1: goto cleanup1;				\
	    case 2: goto cleanup2;				\
	    }							\
	}							\
    } while (0)

#define NEXT_INST_V(pcAdjustment, nCleanup, resultHandling)	\
    do {							\
	pc += (pcAdjustment);					\
	cleanup = (nCleanup);					\
	if (resultHandling) {					\
	    if ((resultHandling) > 0) {				\
		Tcl_IncrRefCount(objResultPtr);			\
	    }							\
	    goto cleanupV_pushObjResultPtr;			\
	} else {						\
	    goto cleanupV;					\
	}							\
    } while (0)

/*
 * Macros used to access items on the Tcl evaluation stack. PUSH_OBJECT
 * increments the object's ref count since it makes the stack have another
 * reference pointing to the object. However, POP_OBJECT does not decrement
 * the ref count. This is because the stack may hold the only reference to the
 * object, so the object would be destroyed if its ref count were decremented
 * before the caller had a chance to, e.g., store it in a variable. It is the
 * caller's responsibility to decrement the ref count when it is finished with
 * an object.
 *
 * WARNING! It is essential that objPtr only appear once in the PUSH_OBJECT
 * macro. The actual parameter might be an expression with side effects, and
 * this ensures that it will be executed only once.
 */

#define PUSH_OBJECT(objPtr) \
    Tcl_IncrRefCount(*(++tosPtr) = (objPtr))

#define POP_OBJECT()	*(tosPtr--)

#define OBJ_AT_TOS	*tosPtr

#define OBJ_UNDER_TOS	*(tosPtr-1)

#define OBJ_AT_DEPTH(n)	*(tosPtr-(n))

#define CURR_DEPTH	((ptrdiff_t) (tosPtr - initTosPtr))

/*
 * Macros used to trace instruction execution. The macros TRACE,
 * TRACE_WITH_OBJ, and O2S are only used inside TclNRExecuteByteCode. O2S is
 * only used in TRACE* calls to get a string from an object.
 */


/*
 * Macro used in this file to save a function call for common uses of
 * TclGetNumberFromObj(). The ANSI C "prototype" is:
 *
 * MODULE_SCOPE int GetNumberFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
 *			ClientData *ptrPtr, int *tPtr);
 */

#ifdef NO_WIDE_TYPE
#define GetNumberFromObj(interp, objPtr, ptrPtr, tPtr) \
    (((objPtr)->typePtr == &tclIntType)					\
	?	(*(tPtr) = TCL_NUMBER_LONG,				\
		*(ptrPtr) = (ClientData)				\
		    (&((objPtr)->internalRep.longValue)), TCL_OK) :	\
    ((objPtr)->typePtr == &tclDoubleType)				\
	?	(((TclIsNaN((objPtr)->internalRep.doubleValue))		\
		    ?	(*(tPtr) = TCL_NUMBER_NAN)			\
		    :	(*(tPtr) = TCL_NUMBER_DOUBLE)),			\
		*(ptrPtr) = (ClientData)				\
		    (&((objPtr)->internalRep.doubleValue)), TCL_OK) :	\
    ((((objPtr)->typePtr == NULL) && ((objPtr)->bytes == NULL)) ||	\
    (((objPtr)->bytes != NULL) && ((objPtr)->length == 0)))		\
	? TCL_ERROR :							\
    TclGetNumberFromObj((interp), (objPtr), (ptrPtr), (tPtr)))
#else /* !NO_WIDE_TYPE */
#define GetNumberFromObj(interp, objPtr, ptrPtr, tPtr) \
    (((objPtr)->typePtr == &tclIntType)					\
	?	(*(tPtr) = TCL_NUMBER_LONG,				\
		*(ptrPtr) = (ClientData)				\
		    (&((objPtr)->internalRep.longValue)), TCL_OK) :	\
    ((objPtr)->typePtr == &tclWideIntType)				\
	?	(*(tPtr) = TCL_NUMBER_WIDE,				\
		*(ptrPtr) = (ClientData)				\
		    (&((objPtr)->internalRep.wideValue)), TCL_OK) :	\
    ((objPtr)->typePtr == &tclDoubleType)				\
	?	(((TclIsNaN((objPtr)->internalRep.doubleValue))		\
		    ?	(*(tPtr) = TCL_NUMBER_NAN)			\
		    :	(*(tPtr) = TCL_NUMBER_DOUBLE)),			\
		*(ptrPtr) = (ClientData)				\
		    (&((objPtr)->internalRep.doubleValue)), TCL_OK) :	\
    ((((objPtr)->typePtr == NULL) && ((objPtr)->bytes == NULL)) ||	\
    (((objPtr)->bytes != NULL) && ((objPtr)->length == 0)))		\
	? TCL_ERROR :							\
    TclGetNumberFromObj((interp), (objPtr), (ptrPtr), (tPtr)))
#endif /* NO_WIDE_TYPE */

/*
 * Macro used in this file to save a function call for common uses of
 * Tcl_GetBooleanFromObj(). The ANSI C "prototype" is:
 *
 * MODULE_SCOPE int TclGetBooleanFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
 *			int *boolPtr);
 */

#define TclGetBooleanFromObj(interp, objPtr, boolPtr) \
    ((((objPtr)->typePtr == &tclIntType)				\
	|| ((objPtr)->typePtr == &tclBooleanType))			\
	? (*(boolPtr) = ((objPtr)->internalRep.longValue!=0), TCL_OK)	\
	: Tcl_GetBooleanFromObj((interp), (objPtr), (boolPtr)))

/*
 * Macro used in this file to save a function call for common uses of
 * Tcl_GetWideIntFromObj(). The ANSI C "prototype" is:
 *
 * MODULE_SCOPE int TclGetWideIntFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
 *			Tcl_WideInt *wideIntPtr);
 */

#ifdef NO_WIDE_TYPE
#define TclGetWideIntFromObj(interp, objPtr, wideIntPtr) \
    (((objPtr)->typePtr == &tclIntType)					\
	? (*(wideIntPtr) = (Tcl_WideInt)				\
		((objPtr)->internalRep.longValue), TCL_OK) :		\
	Tcl_GetWideIntFromObj((interp), (objPtr), (wideIntPtr)))
#else /* !NO_WIDE_TYPE */
#define TclGetWideIntFromObj(interp, objPtr, wideIntPtr)		\
    (((objPtr)->typePtr == &tclWideIntType)				\
	? (*(wideIntPtr) = (objPtr)->internalRep.wideValue, TCL_OK) :	\
    ((objPtr)->typePtr == &tclIntType)					\
	? (*(wideIntPtr) = (Tcl_WideInt)				\
		((objPtr)->internalRep.longValue), TCL_OK) :		\
	Tcl_GetWideIntFromObj((interp), (objPtr), (wideIntPtr)))
#endif /* NO_WIDE_TYPE */

/*
 * Macro used to make the check for type overflow more mnemonic. This works by
 * comparing sign bits; the rest of the word is irrelevant. The ANSI C
 * "prototype" (where inttype_t is any integer type) is:
 *
 * MODULE_SCOPE int Overflowing(inttype_t a, inttype_t b, inttype_t sum);
 *
 * Check first the condition most likely to fail in usual code (at least for
 * usage in [incr]: do the first summand and the sum have != signs?
 */

#define Overflowing(a,b,sum) ((((a)^(sum)) < 0) && (((a)^(b)) >= 0))

/*
 * Macro for checking whether the type is NaN, used when we're thinking about
 * throwing an error for supplying a non-number number.
 */

#ifndef ACCEPT_NAN
#define IsErroringNaNType(type)		((type) == TCL_NUMBER_NAN)
#else
#define IsErroringNaNType(type)		0
#endif

/*
 * Auxiliary tables used to compute powers of small integers.
 */

#if (LONG_MAX == 0x7fffffff)

/*
 * Maximum base that, when raised to powers 2, 3, ... 8, fits in a 32-bit
 * signed integer.
 */

static const long MaxBase32[] = {46340, 1290, 215, 73, 35, 21, 14};
static const size_t MaxBase32Size = sizeof(MaxBase32)/sizeof(long);

/*
 * Table giving 3, 4, ..., 11, raised to the powers 9, 10, ..., as far as they
 * fit in a 32-bit signed integer. Exp32Index[i] gives the starting index of
 * powers of i+3; Exp32Value[i] gives the corresponding powers.
 */

static const unsigned short Exp32Index[] = {
    0, 11, 18, 23, 26, 29, 31, 32, 33
};
static const size_t Exp32IndexSize =
    sizeof(Exp32Index) / sizeof(unsigned short);
static const long Exp32Value[] = {
    19683, 59049, 177147, 531441, 1594323, 4782969, 14348907, 43046721,
    129140163, 387420489, 1162261467, 262144, 1048576, 4194304,
    16777216, 67108864, 268435456, 1073741824, 1953125, 9765625,
    48828125, 244140625, 1220703125, 10077696, 60466176, 362797056,
    40353607, 282475249, 1977326743, 134217728, 1073741824, 387420489,
    1000000000
};
static const size_t Exp32ValueSize = sizeof(Exp32Value)/sizeof(long);
#endif /* LONG_MAX == 0x7fffffff -- 32 bit machine */

#if (LONG_MAX > 0x7fffffff) || !defined(TCL_WIDE_INT_IS_LONG)

/*
 * Maximum base that, when raised to powers 2, 3, ..., 16, fits in a
 * Tcl_WideInt.
 */

static const Tcl_WideInt MaxBase64[] = {
    (Tcl_WideInt)46340*65536+62259,	/* 3037000499 == isqrt(2**63-1) */
    (Tcl_WideInt)2097151, (Tcl_WideInt)55108, (Tcl_WideInt)6208,
    (Tcl_WideInt)1448, (Tcl_WideInt)511, (Tcl_WideInt)234, (Tcl_WideInt)127,
    (Tcl_WideInt)78, (Tcl_WideInt)52, (Tcl_WideInt)38, (Tcl_WideInt)28,
    (Tcl_WideInt)22, (Tcl_WideInt)18, (Tcl_WideInt)15
};
static const size_t MaxBase64Size = sizeof(MaxBase64)/sizeof(Tcl_WideInt);

/*
 * Table giving 3, 4, ..., 13 raised to powers greater than 16 when the
 * results fit in a 64-bit signed integer.
 */

static const unsigned short Exp64Index[] = {
    0, 23, 38, 49, 57, 63, 67, 70, 72, 74, 75, 76
};
static const size_t Exp64IndexSize =
    sizeof(Exp64Index) / sizeof(unsigned short);
static const Tcl_WideInt Exp64Value[] = {
    (Tcl_WideInt)243*243*243*3*3,
    (Tcl_WideInt)243*243*243*3*3*3,
    (Tcl_WideInt)243*243*243*3*3*3*3,
    (Tcl_WideInt)243*243*243*243,
    (Tcl_WideInt)243*243*243*243*3,
    (Tcl_WideInt)243*243*243*243*3*3,
    (Tcl_WideInt)243*243*243*243*3*3*3,
    (Tcl_WideInt)243*243*243*243*3*3*3*3,
    (Tcl_WideInt)243*243*243*243*243,
    (Tcl_WideInt)243*243*243*243*243*3,
    (Tcl_WideInt)243*243*243*243*243*3*3,
    (Tcl_WideInt)243*243*243*243*243*3*3*3,
    (Tcl_WideInt)243*243*243*243*243*3*3*3*3,
    (Tcl_WideInt)243*243*243*243*243*243,
    (Tcl_WideInt)243*243*243*243*243*243*3,
    (Tcl_WideInt)243*243*243*243*243*243*3*3,
    (Tcl_WideInt)243*243*243*243*243*243*3*3*3,
    (Tcl_WideInt)243*243*243*243*243*243*3*3*3*3,
    (Tcl_WideInt)243*243*243*243*243*243*243,
    (Tcl_WideInt)243*243*243*243*243*243*243*3,
    (Tcl_WideInt)243*243*243*243*243*243*243*3*3,
    (Tcl_WideInt)243*243*243*243*243*243*243*3*3*3,
    (Tcl_WideInt)243*243*243*243*243*243*243*3*3*3*3,
    (Tcl_WideInt)1024*1024*1024*4*4,
    (Tcl_WideInt)1024*1024*1024*4*4*4,
    (Tcl_WideInt)1024*1024*1024*4*4*4*4,
    (Tcl_WideInt)1024*1024*1024*1024,
    (Tcl_WideInt)1024*1024*1024*1024*4,
    (Tcl_WideInt)1024*1024*1024*1024*4*4,
    (Tcl_WideInt)1024*1024*1024*1024*4*4*4,
    (Tcl_WideInt)1024*1024*1024*1024*4*4*4*4,
    (Tcl_WideInt)1024*1024*1024*1024*1024,
    (Tcl_WideInt)1024*1024*1024*1024*1024*4,
    (Tcl_WideInt)1024*1024*1024*1024*1024*4*4,
    (Tcl_WideInt)1024*1024*1024*1024*1024*4*4*4,
    (Tcl_WideInt)1024*1024*1024*1024*1024*4*4*4*4,
    (Tcl_WideInt)1024*1024*1024*1024*1024*1024,
    (Tcl_WideInt)1024*1024*1024*1024*1024*1024*4,
    (Tcl_WideInt)3125*3125*3125*5*5,
    (Tcl_WideInt)3125*3125*3125*5*5*5,
    (Tcl_WideInt)3125*3125*3125*5*5*5*5,
    (Tcl_WideInt)3125*3125*3125*3125,
    (Tcl_WideInt)3125*3125*3125*3125*5,
    (Tcl_WideInt)3125*3125*3125*3125*5*5,
    (Tcl_WideInt)3125*3125*3125*3125*5*5*5,
    (Tcl_WideInt)3125*3125*3125*3125*5*5*5*5,
    (Tcl_WideInt)3125*3125*3125*3125*3125,
    (Tcl_WideInt)3125*3125*3125*3125*3125*5,
    (Tcl_WideInt)3125*3125*3125*3125*3125*5*5,
    (Tcl_WideInt)7776*7776*7776*6*6,
    (Tcl_WideInt)7776*7776*7776*6*6*6,
    (Tcl_WideInt)7776*7776*7776*6*6*6*6,
    (Tcl_WideInt)7776*7776*7776*7776,
    (Tcl_WideInt)7776*7776*7776*7776*6,
    (Tcl_WideInt)7776*7776*7776*7776*6*6,
    (Tcl_WideInt)7776*7776*7776*7776*6*6*6,
    (Tcl_WideInt)7776*7776*7776*7776*6*6*6*6,
    (Tcl_WideInt)16807*16807*16807*7*7,
    (Tcl_WideInt)16807*16807*16807*7*7*7,
    (Tcl_WideInt)16807*16807*16807*7*7*7*7,
    (Tcl_WideInt)16807*16807*16807*16807,
    (Tcl_WideInt)16807*16807*16807*16807*7,
    (Tcl_WideInt)16807*16807*16807*16807*7*7,
    (Tcl_WideInt)32768*32768*32768*8*8,
    (Tcl_WideInt)32768*32768*32768*8*8*8,
    (Tcl_WideInt)32768*32768*32768*8*8*8*8,
    (Tcl_WideInt)32768*32768*32768*32768,
    (Tcl_WideInt)59049*59049*59049*9*9,
    (Tcl_WideInt)59049*59049*59049*9*9*9,
    (Tcl_WideInt)59049*59049*59049*9*9*9*9,
    (Tcl_WideInt)100000*100000*100000*10*10,
    (Tcl_WideInt)100000*100000*100000*10*10*10,
    (Tcl_WideInt)161051*161051*161051*11*11,
    (Tcl_WideInt)161051*161051*161051*11*11*11,
    (Tcl_WideInt)248832*248832*248832*12*12,
    (Tcl_WideInt)371293*371293*371293*13*13
};
static const size_t Exp64ValueSize = sizeof(Exp64Value) / sizeof(Tcl_WideInt);
#endif /* (LONG_MAX > 0x7fffffff) || !defined(TCL_WIDE_INT_IS_LONG) */

/*
 * Markers for ExecuteExtendedBinaryMathOp.
 */

#define DIVIDED_BY_ZERO		((Tcl_Obj *) -1)
#define EXPONENT_OF_ZERO	((Tcl_Obj *) -2)
#define GENERAL_ARITHMETIC_ERROR ((Tcl_Obj *) -3)

/*
 * Declarations for local procedures to this file:
 */

static ByteCode *	CompileExprObj(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void		DupExprCodeInternalRep(Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr);
MODULE_SCOPE int	TclCompareTwoNumbers(Tcl_Obj *valuePtr,
			    Tcl_Obj *value2Ptr);
static Tcl_Obj *	ExecuteExtendedBinaryMathOp(Tcl_Interp *interp,
			    int opcode, Tcl_Obj *valuePtr, Tcl_Obj *value2Ptr);
static Tcl_Obj *	ExecuteExtendedUnaryMathOp(int opcode,
			    Tcl_Obj *valuePtr);
static void		FreeExprCodeInternalRep(Tcl_Obj *objPtr);
static const char *	GetSrcInfoForPc(const unsigned char *pc,
			    ByteCode *codePtr, int *lengthPtr,
			    const unsigned char **pcBeg);
static void		IllegalExprOperandType(Tcl_Interp *interp,
			    const unsigned char *pc, Tcl_Obj *opndPtr);
static Tcl_NRPostProc	CopyCallback;
static Tcl_NRPostProc	ExprObjCallback;

static Tcl_NRPostProc   TEBCresume;
static Tcl_NRPostProc   TEBCcleanup;

/*
 * The structure below defines a bytecode Tcl object type to hold the
 * compiled bytecode for Tcl expressions.
 */

static const Tcl_ObjType exprCodeType = {
    "exprcode",
    FreeExprCodeInternalRep,	/* freeIntRepProc */
    DupExprCodeInternalRep,	/* dupIntRepProc */
    NULL,			/* updateStringProc */
    NULL			/* setFromAnyProc */
};


static void UpdateStringOfBcSource(Tcl_Obj *objPtr);

static const Tcl_ObjType bcSourceType = {
    "bcSource",			/* name */
    NULL,			/* freeIntRepProc */
    NULL,			/* dupIntRepProc */
    UpdateStringOfBcSource,	/* updateStringProc */
    NULL			/* setFromAnyProc */
};

static void
UpdateStringOfBcSource(
    Tcl_Obj *objPtr)
{
    int len;
    const char *bytes;
    unsigned char *pc = objPtr->internalRep.twoPtrValue.ptr1;
    ByteCode *codePtr = objPtr->internalRep.twoPtrValue.ptr2;

    bytes = GetSrcInfoForPc(pc, codePtr, &len, NULL);
    objPtr->bytes = (char *) ckalloc((unsigned) len + 1);
    memcpy(objPtr->bytes, bytes, len);
    objPtr->bytes[len] = '\0';
    objPtr->length = len;
}

static inline int
TclCodeIsStale(
    ByteCode *codePtr,
    Interp *iPtr)
{
    Namespace *namespacePtr = iPtr->varFramePtr->nsPtr;
    int check = (((Interp *) *codePtr->interpHandle != iPtr)
	    || (codePtr->nsPtr != namespacePtr)
	    || (codePtr->nsEpoch != namespacePtr->resolverEpoch)
	    || (codePtr->localCachePtr != iPtr->varFramePtr->localCachePtr));

    return check;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateExecEnv --
 *
 *	This procedure creates a new execution environment for Tcl bytecode
 *	execution. An ExecEnv points to a Tcl evaluation stack. An ExecEnv is
 *	typically created once for each Tcl interpreter (Interp structure) and
 *	recursively passed to TclNRExecuteByteCode to execute ByteCode sequences
 *	for nested commands.
 *
 * Results:
 *	A newly allocated ExecEnv is returned. This points to an empty
 *	evaluation stack of the standard initial size.
 *
 * Side effects:
 *	The bytecode interpreter is also initialized here, as this procedure
 *	will be called before any call to TclNRExecuteByteCode.
 *
 *----------------------------------------------------------------------
 */

ExecEnv *
TclCreateExecEnv(
    Tcl_Interp *interp,		/* Interpreter for which the execution
				 * environment is being created. */
    int size)			/* The initial stack size, in number of words
				 * [sizeof(Tcl_Obj*)] */
{
    ExecEnv *eePtr = ckalloc(sizeof(ExecEnv));

    eePtr->interp = interp;
    eePtr->callbackPtr = NULL;
    eePtr->corPtr = NULL;
    eePtr->rewind = 0;

    return eePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclDeleteExecEnv --
 *
 *	Frees the storage for an ExecEnv.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage for an ExecEnv and its contained storage (e.g. the evaluation
 *	stack) is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclDeleteExecEnv(
    ExecEnv *eePtr)		/* Execution environment to free. */
{
	cachedInExit = TclInExit();

    /*
     * Delete all stacks in this exec env.
     */

    if (eePtr->callbackPtr && !cachedInExit) {
	Tcl_Panic("Deleting execEnv with pending TEOV callbacks!");
    }
    if (eePtr->corPtr && !cachedInExit) {
	Tcl_Panic("Deleting execEnv with existing coroutine");
    }
    ckfree(eePtr);
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_ExprObj --
 *
 *	Evaluate an expression in a Tcl_Obj.
 *
 * Results:
 *	A standard Tcl object result. If the result is other than TCL_OK, then
 *	the interpreter's result contains an error message. If the result is
 *	TCL_OK, then a pointer to the expression's result value object is
 *	stored in resultPtrPtr. In that case, the object's ref count is
 *	incremented to reflect the reference returned to the caller; the
 *	caller is then responsible for the resulting object and must, for
 *	example, decrement the ref count when it is finished with the object.
 *
 * Side effects:
 *	Any side effects caused by subcommands in the expression, if any. The
 *	interpreter result is not modified unless there is an error.
 *
 *--------------------------------------------------------------
 */

int
Tcl_ExprObj(
    Tcl_Interp *interp,		/* Context in which to evaluate the
				 * expression. */
    register Tcl_Obj *objPtr,	/* Points to Tcl object containing expression
				 * to evaluate. */
    Tcl_Obj **resultPtrPtr)	/* Where the Tcl_Obj* that is the expression
				 * result is stored if no errors occur. */
{
    Tcl_Obj *resultPtr;

    TclNRSetRoot(interp);
    TclNewObj(resultPtr);
    Tcl_NRAddCallback(interp, CopyCallback, resultPtrPtr, resultPtr,
	    NULL, NULL);
    Tcl_NRExprObj(interp, objPtr, resultPtr);
    return TclNRRunCallbacks(interp, TCL_OK);
}

static int
CopyCallback(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    Tcl_Obj **resultPtrPtr = data[0];
    Tcl_Obj *resultPtr = data[1];

    if (result == TCL_OK) {
	*resultPtrPtr = resultPtr;
	Tcl_IncrRefCount(resultPtr);
    } else {
	Tcl_DecrRefCount(resultPtr);
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * Tcl_NRExprObj --
 *
 *	Request evaluation of the expression in a Tcl_Obj by the NR stack.
 *
 * Results:
 *	Returns TCL_OK.
 *
 * Side effects:
 *	Compiles objPtr as a Tcl expression and places callbacks on the
 *	NR stack to execute the bytecode and store the result in resultPtr.
 *	If bytecode execution raises an exception, nothing is written
 *	to resultPtr, and the exceptional return code flows up the NR
 *	stack.  If the exception is TCL_ERROR, an error message is left
 *	in the interp result and the interp's return options dictionary
 *	holds additional error information too.  Execution of the bytecode
 *	may have other side effects, depending on the expression.
 *
 *--------------------------------------------------------------
 */

int
Tcl_NRExprObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    Tcl_Obj *resultPtr)
{
    ByteCode *codePtr;

    /* TODO: consider saving whole state? */
    Tcl_Obj *saveObjPtr = Tcl_GetObjResult(interp);

    Tcl_IncrRefCount(saveObjPtr);

    codePtr = CompileExprObj(interp, objPtr);

    /* TODO: Confirm reset not required? */
    /*Tcl_ResetResult(interp);*/
    Tcl_NRAddCallback(interp, ExprObjCallback, saveObjPtr, resultPtr,
	    NULL, NULL);
    return TclNRExecuteByteCode(interp, codePtr);
}

static int
ExprObjCallback(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    Tcl_Obj *saveObjPtr = data[0];
    Tcl_Obj *resultPtr = data[1];

    if (result == TCL_OK) {
	TclSetDuplicateObj(resultPtr, Tcl_GetObjResult(interp));
	Tcl_SetObjResult(interp, saveObjPtr);
    }
    TclDecrRefCount(saveObjPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileExprObj --
 *	Compile a Tcl expression value into ByteCode.
 *
 * Results:
 *	A (ByteCode *) is returned pointing to the resulting ByteCode.
 *	The caller must manage its refCount and arrange for a call to
 *	TclCleanupByteCode() when the last reference disappears.
 *
 * Side effects:
 *	The Tcl_ObjType of objPtr is changed to the "bytecode" type,
 *	and the ByteCode is kept in the internal rep (along with context
 *	data for checking validity) for faster operations the next time
 *	CompileExprObj is called on the same value.
 *
 *----------------------------------------------------------------------
 */

static ByteCode *
CompileExprObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr)
{
    Interp *iPtr = (Interp *) interp;
    CompileEnv compEnv;		/* Compilation environment structure allocated
				 * in frame. */
    register ByteCode *codePtr = NULL;
				/* Tcl Internal type of bytecode. Initialized
				 * to avoid compiler warning. */

    /*
     * Get the expression ByteCode from the object. If it exists, make sure it
     * is valid in the current context.
     */
    if (objPtr->typePtr == &exprCodeType) {
	codePtr = objPtr->internalRep.otherValuePtr;
	if (TclCodeIsStale(codePtr, iPtr)) {
	    FreeExprCodeInternalRep(objPtr);
	}
    }
    if (objPtr->typePtr != &exprCodeType) {
	int length;
	const char *string = TclGetStringFromObj(objPtr, &length);

	TclInitCompileEnv(interp, &compEnv, string, length);
	TclCompileExpr(interp, string, length, &compEnv, 0);

	/*
	 * Successful compilation. If the expression yielded no instructions,
	 * push an zero object as the expression's result.
	 */

	if (compEnv.codeNext == compEnv.codeStart) {
	    TclEmitPush(TclRegisterNewLiteral(&compEnv, "0", 1),
		    &compEnv);
	}

	/*
	 * Add a "done" instruction as the last instruction and change the
	 * object into a ByteCode object. Ownership of the literal objects and
	 * aux data items is given to the ByteCode object.
	 */

	TclEmitOpcode(INST_DONE, &compEnv);
	TclInitByteCodeObj(objPtr, &compEnv);
	objPtr->typePtr = &exprCodeType;
	TclFreeCompileEnv(&compEnv);
	codePtr = objPtr->internalRep.otherValuePtr;
	if (iPtr->varFramePtr->localCachePtr) {
	    codePtr->localCachePtr = iPtr->varFramePtr->localCachePtr;
	    codePtr->localCachePtr->refCount++;
	}
    }
    return codePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * DupExprCodeInternalRep --
 *
 *	Part of the Tcl object type implementation for Tcl expression
 *	bytecode. We do not copy the bytecode intrep. Instead, we return
 *	without setting copyPtr->typePtr, so the copy is a plain string copy
 *	of the expression value, and if it is to be used as a compiled
 *	expression, it will just need a recompile.
 *
 *	This makes sense, because with Tcl's copy-on-write practices, the
 *	usual (only?) time Tcl_DuplicateObj() will be called is when the copy
 *	is about to be modified, which would invalidate any copied bytecode
 *	anyway. The only reason it might make sense to copy the bytecode is if
 *	we had some modifying routines that operated directly on the intrep,
 *	like we do for lists and dicts.
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
DupExprCodeInternalRep(
    Tcl_Obj *srcPtr,
    Tcl_Obj *copyPtr)
{
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeExprCodeInternalRep --
 *
 *	Part of the Tcl object type implementation for Tcl expression
 *	bytecode. Frees the storage allocated to hold the internal rep, unless
 *	ref counts indicate bytecode execution is still in progress.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May free allocated memory. Leaves objPtr untyped.
 *
 *----------------------------------------------------------------------
 */

static void
FreeExprCodeInternalRep(
    Tcl_Obj *objPtr)
{
    ByteCode *codePtr = objPtr->internalRep.otherValuePtr;

    objPtr->typePtr = NULL;
    objPtr->internalRep.otherValuePtr = NULL;
    codePtr->refCount--;
    if (codePtr->refCount <= 0) {
	TclCleanupByteCode(codePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileObj --
 *
 *	This procedure compiles the script contained in a Tcl_Obj.
 *
 * Results:
 *	A pointer to the corresponding ByteCode, never NULL.
 *
 * Side effects:
 *	The object is shimmered to bytecode type.
 *
 *----------------------------------------------------------------------
 */

ByteCode *
TclCompileObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr)
{
    register Interp *iPtr = (Interp *) interp;
    register ByteCode *codePtr;	/* Tcl Internal type of bytecode. */

    /*
     * If the object is not already of tclByteCodeType, compile it (and reset
     * the compilation flags in the interpreter; this should be done after any
     * compilation). Otherwise, check that it is "fresh" enough.
     */

    if (objPtr->typePtr == &tclByteCodeType) {
	/*
	 * Make sure the Bytecode hasn't been invalidated by, e.g., someone
	 * redefining a command with a compile procedure (this might make the
	 * compiled code wrong). The object needs to be recompiled if it was
	 * compiled in/for a different interpreter, or for a different
	 * namespace, or for the same namespace but with different name
	 * resolution rules. Precompiled objects, however, are immutable and
	 * therefore they are not recompiled, even if the epoch has changed.
	 *
	 * To be pedantically correct, we should also check that the
	 * originating procPtr is the same as the current context procPtr
	 * (assuming one exists at all - none for global level). This code is
	 * #def'ed out because [info body] was changed to never return a
	 * bytecode type object, which should obviate us from the extra checks
	 * here.
	 */

	codePtr = objPtr->internalRep.otherValuePtr;
	if (TclCodeIsStale(codePtr, iPtr)) {
	    goto recompileObj;
	}
	return codePtr;
    }

  recompileObj:
    iPtr->errorLine = 1;

    TclSetByteCodeFromAny(interp, objPtr, NULL, NULL);
    codePtr = objPtr->internalRep.otherValuePtr;
    if (iPtr->varFramePtr->localCachePtr) {
	codePtr->localCachePtr = iPtr->varFramePtr->localCachePtr;
	codePtr->localCachePtr->refCount++;
    }
    return codePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclIncrObj --
 *
 *	Increment an integeral value in a Tcl_Obj by an integeral value held
 *	in another Tcl_Obj. Caller is responsible for making sure we can
 *	update the first object.
 *
 * Results:
 *	TCL_ERROR if either object is non-integer, and TCL_OK otherwise. On
 *	error, an error message is left in the interpreter (if it is not NULL,
 *	of course).
 *
 * Side effects:
 *	valuePtr gets the new incrmented value.
 *
 *----------------------------------------------------------------------
 */

int
TclIncrObj(
    Tcl_Interp *interp,
    Tcl_Obj *valuePtr,
    Tcl_Obj *incrPtr)
{
    ClientData ptr1, ptr2;
    int type1, type2;
    mp_int value, incr;

    if (Tcl_IsShared(valuePtr)) {
	Tcl_Panic("%s called with shared object", "TclIncrObj");
    }

    if (GetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK) {
	/*
	 * Produce error message (reparse?!)
	 */

	return TclGetIntFromObj(interp, valuePtr, &type1);
    }
    if (GetNumberFromObj(NULL, incrPtr, &ptr2, &type2) != TCL_OK) {
	/*
	 * Produce error message (reparse?!)
	 */

	TclGetIntFromObj(interp, incrPtr, &type1);
	Tcl_AddErrorInfo(interp, "\n    (reading increment)");
	return TCL_ERROR;
    }

    if ((type1 == TCL_NUMBER_LONG) && (type2 == TCL_NUMBER_LONG)) {
	long augend = *((const long *) ptr1);
	long addend = *((const long *) ptr2);
	long sum = augend + addend;

	/*
	 * Overflow when (augend and sum have different sign) and (augend and
	 * addend have the same sign). This is encapsulated in the Overflowing
	 * macro.
	 */

	if (!Overflowing(augend, addend, sum)) {
	    TclSetLongObj(valuePtr, sum);
	    return TCL_OK;
	}
#ifndef NO_WIDE_TYPE
	{
	    Tcl_WideInt w1 = (Tcl_WideInt) augend;
	    Tcl_WideInt w2 = (Tcl_WideInt) addend;

	    /*
	     * We know the sum value is outside the long range, so we use the
	     * macro form that doesn't range test again.
	     */

	    TclSetWideIntObj(valuePtr, w1 + w2);
	    return TCL_OK;
	}
#endif
    }

    if ((type1 == TCL_NUMBER_DOUBLE) || (type1 == TCL_NUMBER_NAN)) {
	/*
	 * Produce error message (reparse?!)
	 */

	return TclGetIntFromObj(interp, valuePtr, &type1);
    }
    if ((type2 == TCL_NUMBER_DOUBLE) || (type2 == TCL_NUMBER_NAN)) {
	/*
	 * Produce error message (reparse?!)
	 */

	TclGetIntFromObj(interp, incrPtr, &type1);
	Tcl_AddErrorInfo(interp, "\n    (reading increment)");
	return TCL_ERROR;
    }

#ifndef NO_WIDE_TYPE
    if ((type1 != TCL_NUMBER_BIG) && (type2 != TCL_NUMBER_BIG)) {
	Tcl_WideInt w1, w2, sum;

	TclGetWideIntFromObj(NULL, valuePtr, &w1);
	TclGetWideIntFromObj(NULL, incrPtr, &w2);
	sum = w1 + w2;

	/*
	 * Check for overflow.
	 */

	if (!Overflowing(w1, w2, sum)) {
	    Tcl_SetWideIntObj(valuePtr, sum);
	    return TCL_OK;
	}
    }
#endif

    Tcl_TakeBignumFromObj(interp, valuePtr, &value);
    Tcl_GetBignumFromObj(interp, incrPtr, &incr);
    mp_add(&value, &incr, &value);
    mp_clear(&incr);
    Tcl_SetBignumObj(valuePtr, &value);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclNRExecuteByteCode --
 *
 *	This procedure executes the instructions of a ByteCode structure. It
 *	returns when a "done" instruction is executed or an error occurs.
 *
 * Results:
 *	The return value is one of the return codes defined in tcl.h (such as
 *	TCL_OK), and interp->objResultPtr refers to a Tcl object that either
 *	contains the result of executing the code or an error message.
 *
 * Side effects:
 *	Almost certainly, depending on the ByteCode's instructions.
 *
 *----------------------------------------------------------------------
 */
#define	initTosPtr	((Tcl_Obj **) (&(TD->stack[-1])))

/*
 * Make sure the execution stack is large enough to execute this ByteCode.
 */

#define capacity2size(cap)						\
    (offsetof(TEBCdata, stack) + sizeof(void *)*(cap))

int
TclNRExecuteByteCode(
    Tcl_Interp *interp,		/* Token for command interpreter. */
    ByteCode *codePtr)		/* The bytecode sequence to interpret. */
{
    Interp *iPtr = (Interp *) interp;
    TEBCdata *TD;
    void *update;
    
    if (iPtr->execEnvPtr->rewind) {
	return TCL_ERROR;
    }

    codePtr->refCount++;

    /*
     * Reserve the stack, setup the TEBCdataPtr (TD) and CallFrame
     */

    TD = ckalloc(capacity2size(codePtr->maxStackDepth));

    TD->codePtr     = codePtr;
    TD->tosPtr      = initTosPtr;
    TD->pc          = codePtr->codeStart;
    TD->cleanup     = 0;
    TD->auxObjList  = NULL;
    TD->capacity = codePtr->maxStackDepth;

    /*
     * Push the callback for bytecode execution
     */

    Tcl_NRAddCallback(interp, TEBCcleanup, TD, NULL, NULL, NULL);
    update = &(TOP_CB(interp)->data[0]);
    Tcl_NRAddCallback(interp, TEBCresume, TD, /*resume*/ INT2PTR(0),
	    update, NULL);
    return TCL_OK;
}

#define auxObjList	(TD->auxObjList)
#define codePtr		(TD->codePtr)
#define tosPtr		(TD->tosPtr)
#define pc		(TD->pc)
#define cleanup         (TD->cleanup)

#define iPtr ((Interp *) interp)


static int
TEBCcleanup(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    TEBCdata *TD = data[0];
    Tcl_Obj *tmpPtr;

    if ((result == TCL_ERROR) &&!(iPtr->flags & ERR_ALREADY_LOGGED)
	    && !iPtr->execEnvPtr->rewind ) {
	const unsigned char *Beg;
	const char *bytes;
	int length;
	
	bytes = GetSrcInfoForPc(pc, codePtr, &length, &Beg);
	Tcl_LogCommandInfo(interp, codePtr->source, bytes,
		bytes ? length : 0);
    }
    iPtr->flags &= ~ERR_ALREADY_LOGGED;
    
    /*
     * Clear all expansions and same-level NR calls.
     *
     * Note that expansion markers have a NULL type; avoid removing other
     * markers.
     */

    while (auxObjList) {
	POP_TAUX_OBJ();
    }
    while (tosPtr > initTosPtr) {
	tmpPtr = POP_OBJECT();
	Tcl_DecrRefCount(tmpPtr);
    }

    if (tosPtr < initTosPtr) {
	fprintf(stderr,
		"\nTclNRExecuteByteCode: abnormal return at pc %u: "
		"stack top %d < entry stack top %d\n",
		(unsigned)(pc - codePtr->codeStart),
		(unsigned) CURR_DEPTH, (unsigned) 0);
	Tcl_Panic("TclNRExecuteByteCode execution failure: end stack top < start stack top");
    }

    if (--codePtr->refCount <= 0) {
	TclCleanupByteCode(codePtr);
    }
    ckfree(TD);	/* free my stack */
    
    return result;
}

static int
TEBCresume(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    /*
     * Check just the read-traced/write-traced bit of a variable.
     */

#define ReadTraced(varPtr) ((varPtr)->flags & VAR_TRACED_READ)
#define WriteTraced(varPtr) ((varPtr)->flags & VAR_TRACED_WRITE)
#define UnsetTraced(varPtr) ((varPtr)->flags & VAR_TRACED_UNSET)

    /*
     * Bottom of allocated stack holds the NR data
     */

    /*
     * Constants: variables that do not change during the execution, used
     * sporadically: no special need for speed.
     */

    int instructionCount = 0;	/* Counter that is used to work out when to
				 * call Tcl_AsyncReady() */

    Var *compiledLocals = iPtr->varFramePtr->compiledLocals;

#define LOCAL(i)	(&compiledLocals[(i)])

    /*
     * These macros are just meant to save some global variables that are not
     * used too frequently
     */

    TEBCdata *TD = data[0];

    /*
     * Globals: variables that store state, must remain valid at all times.
     */

    /*
     * Transfer variables - needed only between opcodes, but not while
     * executing an instruction.
     */

    Tcl_Obj *objResultPtr;

    /*
     * Locals - variables that are used within opcodes or bounded sections of
     * the file (jumps between opcodes within a family).
     * NOTE: These are now mostly defined locally where needed.
     */

    Tcl_Obj *objPtr, *valuePtr, *value2Ptr, *part1Ptr, *part2Ptr, *tmpPtr;
    Tcl_Obj **objv;
    int objc = 0;
    int opnd, length, pcAdjustment;
    Var *varPtr, *arrayPtr;

    TEBC_DATA_DIG();

    if (data[1] /* resume from invocation */) {
	if (iPtr->execEnvPtr->rewind) {
	    result = TCL_ERROR;
	}
	if (codePtr->flags & TCL_BYTECODE_RECOMPILE) {
	    iPtr->flags |= ERR_ALREADY_LOGGED;
	    codePtr->flags &= ~TCL_BYTECODE_RECOMPILE;
	}

	if (result == TCL_OK) {
	    if (*pc == INST_POP) {
		NEXT_INST_V(1, cleanup, 0);
	    }

	    /*
	     * Push the call's object result and continue execution with the
	     * next instruction.
	     */

	    objResultPtr = Tcl_GetObjResult(interp);

	    /*
	     * Reset the interp's result to avoid possible duplications of
	     * large objects [Bug 781585]. We do not call Tcl_ResetResult to
	     * avoid any side effects caused by the resetting of errorInfo and
	     * errorCode [Bug 804681], which are not needed here. We chose
	     * instead to manipulate the interp's object result directly.
	     *
	     * Note that the result object is now in objResultPtr, it keeps
	     * the refCount it had in its role of iPtr->objResultPtr.
	     */

	    TclNewObj(objPtr);
	    Tcl_IncrRefCount(objPtr);
	    iPtr->objResultPtr = objPtr;
	    NEXT_INST_V(0, cleanup, -1);
	}

	/*
	 * Result not TCL_OK: fall through
	 */
    }

    if (iPtr->execEnvPtr->rewind) {
	return TCL_ERROR;
    }

    if (result != TCL_OK) {
	pc--;
	return result;
    }

    /*
     * Loop executing instructions until a "done" instruction, a TCL_RETURN,
     * or some error.
     */

    goto cleanup0;

    /*
     * Targets for standard instruction endings; unrolled for speed in the
     * most frequent cases (instructions that consume up to two stack
     * elements).
     *
     * This used to be a "for(;;)" loop, with each instruction doing its own
     * cleanup.
     */

  cleanupV_pushObjResultPtr:
    switch (cleanup) {
    case 0:
	*(++tosPtr) = (objResultPtr);
	goto cleanup0;
    default:
	cleanup -= 2;
	while (cleanup--) {
	    objPtr = POP_OBJECT();
	    TclDecrRefCount(objPtr);
	}
    case 2:
    cleanup2_pushObjResultPtr:
	objPtr = POP_OBJECT();
	TclDecrRefCount(objPtr);
    case 1:
    cleanup1_pushObjResultPtr:
	objPtr = OBJ_AT_TOS;
	TclDecrRefCount(objPtr);
    }
    OBJ_AT_TOS = objResultPtr;
    goto cleanup0;

  cleanupV:
    switch (cleanup) {
    default:
	cleanup -= 2;
	while (cleanup--) {
	    objPtr = POP_OBJECT();
	    TclDecrRefCount(objPtr);
	}
    case 2:
    cleanup2:
	objPtr = POP_OBJECT();
	TclDecrRefCount(objPtr);
    case 1:
    cleanup1:
	objPtr = POP_OBJECT();
	TclDecrRefCount(objPtr);
    case 0:
	/*
	 * We really want to do nothing now, but this is needed for some
	 * compilers (SunPro CC).
	 */

	break;
    }
  cleanup0:

#if 0
    {
	static int pcAll[200];

	if (pcAll[*pc] == 0) {
	    pcAll[*pc] = 1;	
	    fprintf(stderr, "~ %i ~\n", *pc);
	}
    }
#endif

    
    /*
     * Check for asynchronous handlers [Bug 746722]; we do the check every
     * ASYNC_CHECK_COUNT_MASK instruction, of the form (2**n-1).
     */

    if ((instructionCount++ & ASYNC_CHECK_COUNT_MASK) == 0) {
	if (TclAsyncReady(iPtr)) {
	    result = Tcl_AsyncInvoke(interp, result);
	    if (result == TCL_ERROR) {
		return TCL_ERROR;;
	    }
	}

	if (TclCanceled(iPtr)) {
	    if (Tcl_Canceled(interp, TCL_LEAVE_ERR_MSG) == TCL_ERROR) {
		return TCL_ERROR;;
	    }
	}

	if (TclLimitReady(iPtr->limit)) {
	    if (Tcl_LimitCheck(interp) == TCL_ERROR) {
		return TCL_ERROR;;
	    }
	}
    }

    switch (*pc) {
    case INST_SYNTAX: {
	int code = TclGetInt4AtPtr(pc+1);
	int level = TclGetUInt4AtPtr(pc+5);

	/*
	 * OBJ_AT_TOS is returnOpts, OBJ_UNDER_TOS is resultObjPtr.
	 */

	result = TclProcessReturn(interp, code, level, OBJ_AT_TOS);
	if (result == TCL_OK) {
	    NEXT_INST_F(9, 1, 0);
	}
	Tcl_SetObjResult(interp, OBJ_UNDER_TOS);
	if (*pc == INST_SYNTAX) {
	    iPtr->flags &= ~ERR_ALREADY_LOGGED;
	}
	cleanup = 2;
	return result;
    }

    case INST_DONE:
	if (tosPtr > initTosPtr) {
	    Tcl_SetObjResult(interp, OBJ_AT_TOS);
	    return result;
	}
	(void) POP_OBJECT();
	return result;

    case INST_PUSH4:
	objResultPtr = codePtr->objArrayPtr[TclGetUInt4AtPtr(pc+1)];
	NEXT_INST_F(5, 0, 1);

    case INST_REVERSE: {
	Tcl_Obj **a, **b;

	opnd = TclGetUInt4AtPtr(pc+1);
	a = tosPtr-(opnd-1);
	b = tosPtr;
	while (a<b) {
	    tmpPtr = *a;
	    *a = *b;
	    *b = tmpPtr;
	    a++; b--;
	}
	NEXT_INST_F(5, 0, 0);
    }

    case INST_CONCAT1: {
	int appendLen = 0;
	char *bytes, *p;
	Tcl_Obj **currPtr;
	int onlyb = 1;

	opnd = TclGetUInt1AtPtr(pc+1);

	/*
	 * Detect only-bytearray-or-null case.
	 */

	for (currPtr=&OBJ_AT_DEPTH(opnd-1); currPtr<=&OBJ_AT_TOS; currPtr++) {
	    if (((*currPtr)->typePtr != &tclByteArrayType)
		    && ((*currPtr)->bytes != tclEmptyStringRep)) {
		onlyb = 0;
		break;
	    } else if (((*currPtr)->typePtr == &tclByteArrayType) &&
		    ((*currPtr)->bytes != NULL)) {
		onlyb = 0;
		break;
	    }
	}

	/*
	 * Compute the length to be appended.
	 */

	if (onlyb) {
	    for (currPtr = &OBJ_AT_DEPTH(opnd-2);
		    appendLen >= 0 && currPtr <= &OBJ_AT_TOS; currPtr++) {
		if ((*currPtr)->bytes != tclEmptyStringRep) {
		    Tcl_GetByteArrayFromObj(*currPtr, &length);
		    appendLen += length;
		}
	    }
	} else {
	    for (currPtr = &OBJ_AT_DEPTH(opnd-2);
		    appendLen >= 0 && currPtr <= &OBJ_AT_TOS; currPtr++) {
		bytes = TclGetStringFromObj(*currPtr, &length);
		if (bytes != NULL) {
		    appendLen += length;
		}
	    }
	}

	if (appendLen < 0) {
	    /* TODO: convert panic to error ? */
	    Tcl_Panic("max size for a Tcl value (%d bytes) exceeded", INT_MAX);
	}

	/*
	 * If nothing is to be appended, just return the first object by
	 * dropping all the others from the stack; this saves both the
	 * computation and copy of the string rep of the first object,
	 * enabling the fast '$x[set x {}]' idiom for 'K $x [set x {}]'.
	 */

	if (appendLen == 0) {
	    NEXT_INST_V(2, (opnd-1), 0);
	}

	/*
	 * If the first object is shared, we need a new obj for the result;
	 * otherwise, we can reuse the first object. In any case, make sure it
	 * has enough room to accomodate all the concatenated bytes. Note that
	 * if it is unshared its bytes are copied by ckrealloc, so that we set
	 * the loop parameters to avoid copying them again: p points to the
	 * end of the already copied bytes, currPtr to the second object.
	 */

	objResultPtr = OBJ_AT_DEPTH(opnd-1);
	if (!onlyb) {
	    bytes = TclGetStringFromObj(objResultPtr, &length);
	    if (length + appendLen < 0) {
		/* TODO: convert panic to error ? */
		Tcl_Panic("max size for a Tcl value (%d bytes) exceeded",
			INT_MAX);
	    }
	    {
		p = ckalloc(length + appendLen + 1);
		TclNewObj(objResultPtr);
		objResultPtr->bytes = p;
		objResultPtr->length = length + appendLen;
		currPtr = &OBJ_AT_DEPTH(opnd - 1);
	    }

	    /*
	     * Append the remaining characters.
	     */

	    for (; currPtr <= &OBJ_AT_TOS; currPtr++) {
		bytes = TclGetStringFromObj(*currPtr, &length);
		if (bytes != NULL) {
		    memcpy(p, bytes, (size_t) length);
		    p += length;
		}
	    }
	    *p = '\0';
	} else {
	    bytes = (char *) Tcl_GetByteArrayFromObj(objResultPtr, &length);
	    if (length + appendLen < 0) {
		/* TODO: convert panic to error ? */
		Tcl_Panic("max size for a Tcl value (%d bytes) exceeded",
			INT_MAX);
	    }
	    {
		TclNewObj(objResultPtr);
		bytes = (char *) Tcl_SetByteArrayLength(objResultPtr,
			length + appendLen);
		p = bytes;
		currPtr = &OBJ_AT_DEPTH(opnd - 1);
	    }

	    /*
	     * Append the remaining characters.
	     */

	    for (; currPtr <= &OBJ_AT_TOS; currPtr++) {
		if ((*currPtr)->bytes != tclEmptyStringRep) {
		    bytes = (char *) Tcl_GetByteArrayFromObj(*currPtr,&length);
		    memcpy(p, bytes, (size_t) length);
		    p += length;
		}
	    }
	}

	NEXT_INST_V(2, opnd, 1);
    }

    case INST_EXPAND_START:
	/*
	 * Push an element to the auxObjList. This records the current
	 * stack depth - i.e., the point in the stack where the expanded
	 * command starts.
	 *
	 * Use a Tcl_Obj as linked list element; slight mem waste, but faster
	 * allocation than ckalloc. This also abuses the Tcl_Obj structure, as
	 * we do not define a special tclObjType for it. It is not dangerous
	 * as the obj is never passed anywhere, so that all manipulations are
	 * performed here and in INST_INVOKE_EXPANDED (in case of an expansion
	 * error, also in INST_EXPAND_STKTOP).
	 */

	TclNewObj(objPtr);
	objPtr->internalRep.ptrAndLongRep.value = CURR_DEPTH;
	PUSH_TAUX_OBJ(objPtr);
	NEXT_INST_F(1, 0, 0);

    case INST_EXPAND_STKTOP: {
	int i;
	unsigned int reqWords;

	objPtr = OBJ_AT_TOS;
	if (TclListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	    return TCL_ERROR;;
	}

	/*
	 * Make sure there is enough room in the stack to expand this list
	 * *and* process the rest of the command (at least up to the next
	 * argument expansion or command end). The operand is the current
	 * stack depth, as seen by the compiler.
	 */

	reqWords =
	    /* how many were needed originally */
	    codePtr->maxStackDepth
	    /* plus how many we already consumed in previous expansions */
	    + (CURR_DEPTH - TclGetInt4AtPtr(pc+1))
	    /* plus how many are needed for this expansion */
	    + objc - 1;

	(void) POP_OBJECT();
	if (reqWords > TD->capacity) {
	    ptrdiff_t depth;
	    unsigned int size = capacity2size(reqWords);

	    depth = tosPtr - initTosPtr;
	    TD = ckrealloc(TD, size);
	    TD->capacity = reqWords;
	    tosPtr = initTosPtr + depth;
	    *((TEBCdata **) data[2]) = TD;
	}

	/*
	 * Expand the list at stacktop onto the stack; free the list. Knowing
	 * that it has a freeIntRepProc we use Tcl_DecrRefCount().
	 */

	for (i = 0; i < objc; i++) {
	    PUSH_OBJECT(objv[i]);
	}

	Tcl_DecrRefCount(objPtr);
	NEXT_INST_F(5, 0, 0);
    }

    case INST_INVOKE_EXPANDED:
	CLANG_ASSERT(auxObjList);
	objc = CURR_DEPTH - auxObjList->internalRep.ptrAndLongRep.value;
	POP_TAUX_OBJ();
	if (objc) {
	    pcAdjustment = 1;
	    goto doInvocation;
	}

	/*
	 * Nothing was expanded, return {}.
	 */

	TclNewObj(objResultPtr);
	NEXT_INST_F(1, 0, 1);

    case INST_INVOKE_STK4:
	objc = TclGetUInt4AtPtr(pc+1);
	pcAdjustment = 5;

    doInvocation:
	
	objv = &OBJ_AT_DEPTH(objc-1);
	cleanup = objc;


	/*
	 * Finally, let TclEvalObjv handle the command.
	 */

	if (!(codePtr->flags & TCL_BYTECODE_PRECOMPILED)) {
	    Tcl_Obj *srcPtr = iPtr->cmdSourcePtr;
	    srcPtr->typePtr = &bcSourceType;
	    srcPtr->internalRep.twoPtrValue.ptr1 = (unsigned char *) pc;
	    srcPtr->internalRep.twoPtrValue.ptr2 = codePtr;
	}

	pc += pcAdjustment;
	TEBC_YIELD();
	return TclNREvalObjv(interp, objc, objv,
		TCL_EVAL_NOERR, NULL);

    /*
     * INST_CALL_BUILTIN_FUNC1 and INST_CALL_FUNC1 were made obsolete by the
     * changes to add a ::tcl::mathfunc namespace in 8.5.
     */

    /*
     * -----------------------------------------------------------------
     *	   Start of INST_LOAD instructions.
     *
     * WARNING: more 'goto' here than your doctor recommended! The different
     * instructions set the value of some variables and then jump to some
     * common execution code.
     */

    case INST_LOAD_SCALAR4:
	opnd = TclGetUInt4AtPtr(pc+1);
	varPtr = LOCAL(opnd);
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}
	if (TclIsVarDirectReadable(varPtr)) {
	    /*
	     * No errors, no traces: just get the value.
	     */

	    objResultPtr = varPtr->value.objPtr;
	    NEXT_INST_F(5, 0, 1);
	}
	pcAdjustment = 5;
	cleanup = 0;
	arrayPtr = NULL;
	part1Ptr = part2Ptr = NULL;
	goto doCallPtrGetVar;

    case INST_LOAD_ARRAY4:
	opnd = TclGetUInt4AtPtr(pc+1);
	pcAdjustment = 5;
	goto doLoadArray;

    doLoadArray:
	part1Ptr = NULL;
	part2Ptr = OBJ_AT_TOS;
	arrayPtr = LOCAL(opnd);
	while (TclIsVarLink(arrayPtr)) {
	    arrayPtr = arrayPtr->value.linkPtr;
	}
	if (TclIsVarArray(arrayPtr) && !ReadTraced(arrayPtr)) {
	    varPtr = VarHashFindVar(arrayPtr->value.tablePtr, part2Ptr);
	    if (varPtr && TclIsVarDirectReadable(varPtr)) {
		/*
		 * No errors, no traces: just get the value.
		 */

		objResultPtr = varPtr->value.objPtr;
		NEXT_INST_F(pcAdjustment, 1, 1);
	    }
	}
	varPtr = TclLookupArrayElement(interp, part1Ptr, part2Ptr,
		TCL_LEAVE_ERR_MSG, "read", 0, 1, arrayPtr, opnd);
	if (varPtr == NULL) {
	    return TCL_ERROR;;
	}
	cleanup = 1;
	goto doCallPtrGetVar;

    case INST_LOAD_ARRAY_STK:
	cleanup = 2;
	part2Ptr = OBJ_AT_TOS;		/* element name */
	objPtr = OBJ_UNDER_TOS;		/* array name */
	goto doLoadStk;

    case INST_LOAD_SCALAR_STK:
	cleanup = 1;
	part2Ptr = NULL;
	objPtr = OBJ_AT_TOS;		/* variable name */

    doLoadStk:
	part1Ptr = objPtr;
	varPtr = TclObjLookupVarEx(interp, part1Ptr, part2Ptr,
		TCL_LEAVE_ERR_MSG, "read", /*createPart1*/0, /*createPart2*/1,
		&arrayPtr);
	if (!varPtr) {
	    return TCL_ERROR;;
	}

	if (TclIsVarDirectReadable2(varPtr, arrayPtr)) {
	    /*
	     * No errors, no traces: just get the value.
	     */

	    objResultPtr = varPtr->value.objPtr;
	    NEXT_INST_V(1, cleanup, 1);
	}
	pcAdjustment = 1;
	opnd = -1;

    doCallPtrGetVar:
	/*
	 * There are either errors or the variable is traced: call
	 * TclPtrGetVar to process fully.
	 */

	objResultPtr = TclPtrGetVar(interp, varPtr, arrayPtr,
		part1Ptr, part2Ptr, TCL_LEAVE_ERR_MSG, opnd);
	if (!objResultPtr) {
	    return TCL_ERROR;;
	}
	NEXT_INST_V(pcAdjustment, cleanup, 1);

    /*
     *	   End of INST_LOAD instructions.
     * -----------------------------------------------------------------
     */

    case INST_JUMP4:
	opnd = TclGetInt4AtPtr(pc+1);
	NEXT_INST_F(opnd, 0, 0);

    {
	int jmpOffset[2], b;

	/* TODO: consider rewrite so we don't compute the offset we're not
	 * going to take. */
    case INST_JUMP_FALSE4:
	jmpOffset[0] = TclGetInt4AtPtr(pc+1);	/* FALSE offset */
	jmpOffset[1] = 5;			/* TRUE offset */
	goto doCondJump;

    case INST_JUMP_TRUE4:
	jmpOffset[0] = 5;
	jmpOffset[1] = TclGetInt4AtPtr(pc+1);
	goto doCondJump;

    doCondJump:
	valuePtr = OBJ_AT_TOS;

	/* TODO - check claim that taking address of b harms performance */
	/* TODO - consider optimization search for constants */
	if (TclGetBooleanFromObj(interp, valuePtr, &b) != TCL_OK) {
	    return TCL_ERROR;;
	}

	NEXT_INST_F(jmpOffset[b], 1, 0);
    }

    /*
     * -----------------------------------------------------------------
     *	   Start of general introspector instructions.
     */

    /*
     * -----------------------------------------------------------------
     *	   Start of INST_LIST and related instructions.
     */

    {
	int match, s1len, s2len;
	const char *s1, *s2;

    case INST_LIST_IN:
    case INST_LIST_NOT_IN:	/* Basic list containment operators. */
	value2Ptr = OBJ_AT_TOS;
	valuePtr = OBJ_UNDER_TOS;

	s1 = TclGetStringFromObj(valuePtr, &s1len);
	if (TclListObjLength(interp, value2Ptr, &length) != TCL_OK) {
	    return TCL_ERROR;;
	}
	match = 0;
	if (length > 0) {
	    int i = 0;
	    Tcl_Obj *o;

	    /*
	     * An empty list doesn't match anything.
	     */

	    do {
		Tcl_ListObjIndex(NULL, value2Ptr, i, &o);
		if (o != NULL) {
		    s2 = TclGetStringFromObj(o, &s2len);
		} else {
		    s2 = "";
		    s2len = 0;
		}
		if (s1len == s2len) {
		    match = (memcmp(s1, s2, s1len) == 0);
		}
		i++;
	    } while (i < length && match == 0);
	}

	if (*pc == INST_LIST_NOT_IN) {
	    match = !match;
	}

	/*
	 * Peep-hole optimisation: if you're about to jump, do jump from here.
	 * We're saving the effort of pushing a boolean value only to pop it
	 * for branching.
	 */

	pc++;
	TclNewIntObj(objResultPtr, match);
	NEXT_INST_F(0, 2, 1);

    case INST_STR_EQ:
    case INST_STR_NEQ:		/* String (in)equality check */
    stringCompare:
	value2Ptr = OBJ_AT_TOS;
	valuePtr = OBJ_UNDER_TOS;

	match = 0;
	if (valuePtr != value2Ptr) {
	    /*
	     * We only need to check (in)equality when we have equal length
	     * strings.  We can use memcmp in all (n)eq cases because we
	     * don't need to worry about lexical LE/BE variance.
	     */

	    typedef int (*memCmpFn_t)(const void*, const void*, size_t);
	    memCmpFn_t memCmpFn;
	    int checkEq = ((*pc == INST_EQ) || (*pc == INST_NEQ)
		    || (*pc == INST_STR_EQ) || (*pc == INST_STR_NEQ));

	    if (TclIsPureByteArray(valuePtr)
		    && TclIsPureByteArray(value2Ptr)) {
		s1 = (char *) Tcl_GetByteArrayFromObj(valuePtr, &s1len);
		s2 = (char *) Tcl_GetByteArrayFromObj(value2Ptr, &s2len);
		memCmpFn = memcmp;
	    } else if (((valuePtr->typePtr == &tclStringType)
		    && (value2Ptr->typePtr == &tclStringType))) {
		/*
		 * Do a unicode-specific comparison if both of the args are of
		 * String type. If the char length == byte length, we can do a
		 * memcmp. In benchmark testing this proved the most efficient
		 * check between the unicode and string comparison operations.
		 */

		s1len = Tcl_GetCharLength(valuePtr);
		s2len = Tcl_GetCharLength(value2Ptr);
		if ((s1len == valuePtr->length)
			&& (s2len == value2Ptr->length)) {
		    s1 = valuePtr->bytes;
		    s2 = value2Ptr->bytes;
		    memCmpFn = memcmp;
		} else {
		    s1 = (char *) Tcl_GetUnicode(valuePtr);
		    s2 = (char *) Tcl_GetUnicode(value2Ptr);
		    if (
#ifdef WORDS_BIGENDIAN
			1
#else
			checkEq
#endif
			) {
			memCmpFn = memcmp;
			s1len *= sizeof(Tcl_UniChar);
			s2len *= sizeof(Tcl_UniChar);
		    } else {
			memCmpFn = (memCmpFn_t) Tcl_UniCharNcmp;
		    }
		}
	    } else {
		/*
		 * strcmp can't do a simple memcmp in order to handle the
		 * special Tcl \xC0\x80 null encoding for utf-8.
		 */

		s1 = TclGetStringFromObj(valuePtr, &s1len);
		s2 = TclGetStringFromObj(value2Ptr, &s2len);
		if (checkEq) {
		    memCmpFn = memcmp;
		} else {
		    memCmpFn = (memCmpFn_t) TclpUtfNcmp2;
		}
	    }

	    if (checkEq && (s1len != s2len)) {
		match = 1;
	    } else {
		/*
		 * The comparison function should compare up to the minimum
		 * byte length only.
		 */
		match = memCmpFn(s1, s2,
			(size_t) ((s1len < s2len) ? s1len : s2len));
		if (match == 0) {
		    match = s1len - s2len;
		}
	    }
	}

	/*
	 * Make sure only -1,0,1 is returned
	 * TODO: consider peephole opt.
	 */

	/*
	 * Take care of the opcodes that goto'ed into here.
	 */
	
	switch (*pc) {
	    case INST_STR_EQ:
	    case INST_EQ:
		match = (match == 0);
		break;
	    case INST_STR_NEQ:
	    case INST_NEQ:
		match = (match != 0);
		break;
	    case INST_LT:
		match = (match < 0);
		break;
	    case INST_GT:
		match = (match > 0);
		break;
	    case INST_LE:
		match = (match <= 0);
		break;
	    case INST_GE:
		match = (match >= 0);
		break;
	}
	if (match < 0) {
	    TclNewIntObj(objResultPtr, -1);
	} else {
	    TclNewIntObj(objResultPtr, match);
	}
	NEXT_INST_F(1, 2, 1);

    }

    /*
     *	   End of string-related instructions.
     * -----------------------------------------------------------------
     *	   Start of numeric operator instructions.
     */

    {
	ClientData ptr1, ptr2;
	int type1, type2;
	long l1, l2, lResult;

    case INST_EQ:
    case INST_NEQ:
    case INST_LT:
    case INST_GT:
    case INST_LE:
    case INST_GE: {
	int iResult = 0, compare = 0;

	value2Ptr = OBJ_AT_TOS;
	valuePtr = OBJ_UNDER_TOS;

	if (GetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK) {
	    /*
	     * At least one non-numeric argument - compare as strings.
	     */

	    goto stringCompare;
	}
	if (type1 == TCL_NUMBER_NAN) {
	    /*
	     * NaN first arg: NaN != to everything, other compares are false.
	     */

	    iResult = (*pc == INST_NEQ);
	    goto foundResult;
	}
	if (valuePtr == value2Ptr) {
	    compare = MP_EQ;
	    goto convertComparison;
	}
	if (GetNumberFromObj(NULL, value2Ptr, &ptr2, &type2) != TCL_OK) {
	    /*
	     * At least one non-numeric argument - compare as strings.
	     */

	    goto stringCompare;
	}
	if (type2 == TCL_NUMBER_NAN) {
	    /*
	     * NaN 2nd arg: NaN != to everything, other compares are false.
	     */

	    iResult = (*pc == INST_NEQ);
	    goto foundResult;
	}
	if ((type1 == TCL_NUMBER_LONG) && (type2 == TCL_NUMBER_LONG)) {
	    l1 = *((const long *)ptr1);
	    l2 = *((const long *)ptr2);
	    compare = (l1 < l2) ? MP_LT : ((l1 > l2) ? MP_GT : MP_EQ);
	} else {
	    compare = TclCompareTwoNumbers(valuePtr, value2Ptr);
	}

	/*
	 * Turn comparison outcome into appropriate result for opcode.
	 */

    convertComparison:
	switch (*pc) {
	case INST_EQ:
	    iResult = (compare == MP_EQ);
	    break;
	case INST_NEQ:
	    iResult = (compare != MP_EQ);
	    break;
	case INST_LT:
	    iResult = (compare == MP_LT);
	    break;
	case INST_GT:
	    iResult = (compare == MP_GT);
	    break;
	case INST_LE:
	    iResult = (compare != MP_GT);
	    break;
	case INST_GE:
	    iResult = (compare != MP_LT);
	    break;
	}

    foundResult:
	pc++;
	TclNewIntObj(objResultPtr, iResult);
	NEXT_INST_F(0, 2, 1);
    }

    case INST_MOD:
    case INST_LSHIFT:
    case INST_RSHIFT:
    case INST_BITOR:
    case INST_BITXOR:
    case INST_BITAND:
	value2Ptr = OBJ_AT_TOS;
	valuePtr = OBJ_UNDER_TOS;

	if ((GetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK)
		|| (type1==TCL_NUMBER_DOUBLE) || (type1==TCL_NUMBER_NAN)) {
	    IllegalExprOperandType(interp, pc, valuePtr);
	    return TCL_ERROR;;
	}

	if ((GetNumberFromObj(NULL, value2Ptr, &ptr2, &type2) != TCL_OK)
		|| (type2==TCL_NUMBER_DOUBLE) || (type2==TCL_NUMBER_NAN)) {
	    IllegalExprOperandType(interp, pc, value2Ptr);
	    return TCL_ERROR;;
	}

	/*
	 * Check for common, simple case.
	 */

	if ((type1 == TCL_NUMBER_LONG) && (type2 == TCL_NUMBER_LONG)) {
	    l1 = *((const long *)ptr1);
	    l2 = *((const long *)ptr2);

	    switch (*pc) {
	    case INST_MOD:
		if (l2 == 0) {
		    goto divideByZero;
		} else if ((l2 == 1) || (l2 == -1)) {
		    /*
		     * Div. by |1| always yields remainder of 0.
		     */

		    TclNewIntObj(objResultPtr, 0);
		    NEXT_INST_F(1, 2, 1);
		} else if (l1 == 0) {
		    /*
		     * 0 % (non-zero) always yields remainder of 0.
		     */

		    TclNewIntObj(objResultPtr, 0);
		    NEXT_INST_F(1, 2, 1);
		} else {
		    lResult = l1 / l2;

		    /*
		     * Force Tcl's integer division rules.
		     * TODO: examine for logic simplification
		     */

		    if ((lResult < 0 || (lResult == 0 &&
			    ((l1 < 0 && l2 > 0) || (l1 > 0 && l2 < 0)))) &&
			    (lResult * l2 != l1)) {
			lResult -= 1;
		    }
		    lResult = l1 - l2*lResult;
		    goto longResultOfArithmetic;
		}

	    case INST_RSHIFT:
		if (l2 < 0) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "negative shift argument", -1));
		    return TCL_ERROR;;
		} else if (l1 == 0) {
		    TclNewIntObj(objResultPtr, 0);
		    NEXT_INST_F(1, 2, 1);
		} else {
		    /*
		     * Quickly force large right shifts to 0 or -1.
		     */

		    if (l2 >= (long)(CHAR_BIT*sizeof(long))) {
			/*
			 * We assume that INT_MAX is much larger than the
			 * number of bits in a long. This is a pretty safe
			 * assumption, given that the former is usually around
			 * 4e9 and the latter 32 or 64...
			 */

			if (l1 > 0L) {
			    TclNewIntObj(objResultPtr, 0);
			} else {
			    TclNewIntObj(objResultPtr, -1);
			}
			NEXT_INST_F(1, 2, 1);
		    }

		    /*
		     * Handle shifts within the native long range.
		     */

		    lResult = l1 >> ((int) l2);
		    goto longResultOfArithmetic;
		}

	    case INST_LSHIFT:
		if (l2 < 0) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "negative shift argument", -1));
		    return TCL_ERROR;;
		} else if (l1 == 0) {
		    TclNewIntObj(objResultPtr, 0);
		    NEXT_INST_F(1, 2, 1);
		} else if (l2 > (long) INT_MAX) {
		    /*
		     * Technically, we could hold the value (1 << (INT_MAX+1))
		     * in an mp_int, but since we're using mp_mul_2d() to do
		     * the work, and it takes only an int argument, that's a
		     * good place to draw the line.
		     */

		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "integer value too large to represent", -1));
		    return TCL_ERROR;;
		} else {
		    int shift = (int) l2;

		    /*
		     * Handle shifts within the native long range.
		     */

		    if ((size_t) shift < CHAR_BIT*sizeof(long) && (l1 != 0)
			    && !((l1>0 ? l1 : ~l1) &
				-(1L<<(CHAR_BIT*sizeof(long) - 1 - shift)))) {
			lResult = l1 << shift;
			goto longResultOfArithmetic;
		    }
		}

		/*
		 * Too large; need to use the broken-out function.
		 */

		break;

	    case INST_BITAND:
		lResult = l1 & l2;
		goto longResultOfArithmetic;
	    case INST_BITOR:
		lResult = l1 | l2;
		goto longResultOfArithmetic;
	    case INST_BITXOR:
		lResult = l1 ^ l2;
	    longResultOfArithmetic:
		if (Tcl_IsShared(valuePtr)) {
		    TclNewLongObj(objResultPtr, lResult);
		    NEXT_INST_F(1, 2, 1);
		}
		TclSetLongObj(valuePtr, lResult);
		NEXT_INST_F(1, 1, 0);
	    }
	}

	/*
	 * DO NOT MERGE THIS WITH THE EQUIVALENT SECTION LATER! That would
	 * encourage the compiler to inline ExecuteExtendedBinaryMathOp, which
	 * is highly undesirable due to the overall impact on size.
	 */

	objResultPtr = ExecuteExtendedBinaryMathOp(interp, *pc,
		valuePtr, value2Ptr);
	if (objResultPtr == DIVIDED_BY_ZERO) {
	    goto divideByZero;
	} else if (objResultPtr == GENERAL_ARITHMETIC_ERROR) {
	    return TCL_ERROR;;
	} else if (objResultPtr == NULL) {
	    NEXT_INST_F(1, 1, 0);
	} else {
	    NEXT_INST_F(1, 2, 1);
	}

    case INST_EXPON:
    case INST_ADD:
    case INST_SUB:
    case INST_DIV:
    case INST_MULT:
	value2Ptr = OBJ_AT_TOS;
	valuePtr = OBJ_UNDER_TOS;

	if ((GetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK)
		|| IsErroringNaNType(type1)) {
	    IllegalExprOperandType(interp, pc, valuePtr);
	    return TCL_ERROR;;
	}

#ifdef ACCEPT_NAN
	if (type1 == TCL_NUMBER_NAN) {
	    /*
	     * NaN first argument -> result is also NaN.
	     */

	    NEXT_INST_F(1, 1, 0);
	}
#endif

	if ((GetNumberFromObj(NULL, value2Ptr, &ptr2, &type2) != TCL_OK)
		|| IsErroringNaNType(type2)) {
	    IllegalExprOperandType(interp, pc, value2Ptr);
	    return TCL_ERROR;;
	}

#ifdef ACCEPT_NAN
	if (type2 == TCL_NUMBER_NAN) {
	    /*
	     * NaN second argument -> result is also NaN.
	     */

	    objResultPtr = value2Ptr;
	    NEXT_INST_F(1, 2, 1);
	}
#endif

	/*
	 * Handle (long,long) arithmetic as best we can without going out to
	 * an external function.
	 */

	if ((type1 == TCL_NUMBER_LONG) && (type2 == TCL_NUMBER_LONG)) {
	    Tcl_WideInt w1, w2, wResult;

	    l1 = *((const long *)ptr1);
	    l2 = *((const long *)ptr2);

	    switch (*pc) {
	    case INST_ADD:
		w1 = (Tcl_WideInt) l1;
		w2 = (Tcl_WideInt) l2;
		wResult = w1 + w2;
#ifdef NO_WIDE_TYPE
		/*
		 * Check for overflow.
		 */

		if (Overflowing(w1, w2, wResult)) {
		    goto overflow;
		}
#endif
		goto wideResultOfArithmetic;

	    case INST_SUB:
		w1 = (Tcl_WideInt) l1;
		w2 = (Tcl_WideInt) l2;
		wResult = w1 - w2;
#ifdef NO_WIDE_TYPE
		/*
		 * Must check for overflow. The macro tests for overflows in
		 * sums by looking at the sign bits. As we have a subtraction
		 * here, we are adding -w2. As -w2 could in turn overflow, we
		 * test with ~w2 instead: it has the opposite sign bit to w2
		 * so it does the job. Note that the only "bad" case (w2==0)
		 * is irrelevant for this macro, as in that case w1 and
		 * wResult have the same sign and there is no overflow anyway.
		 */

		if (Overflowing(w1, ~w2, wResult)) {
		    goto overflow;
		}
#endif
	    wideResultOfArithmetic:
		if (Tcl_IsShared(valuePtr)) {
		    objResultPtr = Tcl_NewWideIntObj(wResult);
		    NEXT_INST_F(1, 2, 1);
		}
		Tcl_SetWideIntObj(valuePtr, wResult);
		NEXT_INST_F(1, 1, 0);

	    case INST_DIV:
		if (l2 == 0) {
		    goto divideByZero;
		} else if ((l1 == LONG_MIN) && (l2 == -1)) {
		    /*
		     * Can't represent (-LONG_MIN) as a long.
		     */

		    goto overflow;
		}
		lResult = l1 / l2;

		/*
		 * Force Tcl's integer division rules.
		 * TODO: examine for logic simplification
		 */

		if (((lResult < 0) || ((lResult == 0) &&
			((l1 < 0 && l2 > 0) || (l1 > 0 && l2 < 0)))) &&
			((lResult * l2) != l1)) {
		    lResult -= 1;
		}
		goto longResultOfArithmetic;

	    case INST_MULT:
		if (((sizeof(long) >= 2*sizeof(int))
			&& (l1 <= INT_MAX) && (l1 >= INT_MIN)
			&& (l2 <= INT_MAX) && (l2 >= INT_MIN))
			|| ((sizeof(long) >= 2*sizeof(short))
			&& (l1 <= SHRT_MAX) && (l1 >= SHRT_MIN)
			&& (l2 <= SHRT_MAX) && (l2 >= SHRT_MIN))) {
		    lResult = l1 * l2;
		    goto longResultOfArithmetic;
		}
	    }

	    /*
	     * Fall through with INST_EXPON, INST_DIV and large multiplies.
	     */
	}

    overflow:
	objResultPtr = ExecuteExtendedBinaryMathOp(interp, *pc,
		valuePtr, value2Ptr);
	if (objResultPtr == DIVIDED_BY_ZERO) {
	    goto divideByZero;
	} else if (objResultPtr == EXPONENT_OF_ZERO) {
	    goto exponOfZero;
	} else if (objResultPtr == GENERAL_ARITHMETIC_ERROR) {
	    return TCL_ERROR;;
	} else if (objResultPtr == NULL) {
	    NEXT_INST_F(1, 1, 0);
	} else {
	    NEXT_INST_F(1, 2, 1);
	}

    case INST_LNOT: {
	int b;

	valuePtr = OBJ_AT_TOS;

	/* TODO - check claim that taking address of b harms performance */
	/* TODO - consider optimization search for constants */
	if (TclGetBooleanFromObj(NULL, valuePtr, &b) != TCL_OK) {
	    IllegalExprOperandType(interp, pc, valuePtr);
	    return TCL_ERROR;;
	}
	/* TODO: Consider peephole opt. */
	TclNewIntObj(objResultPtr, !b);
	NEXT_INST_F(1, 1, 1);
    }

    case INST_BITNOT:
	valuePtr = OBJ_AT_TOS;
	if ((GetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK)
		|| (type1==TCL_NUMBER_NAN) || (type1==TCL_NUMBER_DOUBLE)) {
	    /*
	     * ... ~$NonInteger => raise an error.
	     */

	    IllegalExprOperandType(interp, pc, valuePtr);
	    return TCL_ERROR;;
	}
	if (type1 == TCL_NUMBER_LONG) {
	    l1 = *((const long *) ptr1);
	    if (Tcl_IsShared(valuePtr)) {
		TclNewLongObj(objResultPtr, ~l1);
		NEXT_INST_F(1, 1, 1);
	    }
	    TclSetLongObj(valuePtr, ~l1);
	    NEXT_INST_F(1, 0, 0);
	}
	objResultPtr = ExecuteExtendedUnaryMathOp(*pc, valuePtr);
	if (objResultPtr != NULL) {
	    NEXT_INST_F(1, 1, 1);
	} else {
	    NEXT_INST_F(1, 0, 0);
	}

    case INST_UMINUS:
	valuePtr = OBJ_AT_TOS;
	if ((GetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK)
		|| IsErroringNaNType(type1)) {
	    IllegalExprOperandType(interp, pc, valuePtr);
	    return TCL_ERROR;;
	}
	switch (type1) {
	case TCL_NUMBER_NAN:
	    /* -NaN => NaN */
	    NEXT_INST_F(1, 0, 0);
	case TCL_NUMBER_LONG:
	    l1 = *((const long *) ptr1);
	    if (l1 != LONG_MIN) {
		if (Tcl_IsShared(valuePtr)) {
		    TclNewLongObj(objResultPtr, -l1);
		    NEXT_INST_F(1, 1, 1);
		}
		TclSetLongObj(valuePtr, -l1);
		NEXT_INST_F(1, 0, 0);
	    }
	    /* FALLTHROUGH */
	}
	objResultPtr = ExecuteExtendedUnaryMathOp(*pc, valuePtr);
	if (objResultPtr != NULL) {
	    NEXT_INST_F(1, 1, 1);
	} else {
	    NEXT_INST_F(1, 0, 0);
	}

    case INST_UPLUS:
    case INST_TRY_CVT_TO_NUMERIC:
	/*
	 * Try to convert the topmost stack object to numeric object. This is
	 * done in order to support [expr]'s policy of interpreting operands
	 * if at all possible as numbers first, then strings.
	 */

	valuePtr = OBJ_AT_TOS;

	if (GetNumberFromObj(NULL, valuePtr, &ptr1, &type1) != TCL_OK) {
	    if (*pc == INST_UPLUS) {
		/*
		 * ... +$NonNumeric => raise an error.
		 */

		IllegalExprOperandType(interp, pc, valuePtr);
		return TCL_ERROR;;
	    }

	    /* ... TryConvertToNumeric($NonNumeric) is acceptable */
	    NEXT_INST_F(1, 0, 0);
	}
	if (IsErroringNaNType(type1)) {
	    if (*pc == INST_UPLUS) {
		/*
		 * ... +$NonNumeric => raise an error.
		 */

		IllegalExprOperandType(interp, pc, valuePtr);
	    } else {
		/*
		 * Numeric conversion of NaN -> error.
		 */

		TclExprFloatError(interp, *((const double *) ptr1));
	    }
	    return TCL_ERROR;;
	}

	/*
	 * Ensure that the numeric value has a string rep the same as the
	 * formatted version of its internal rep. This is used, e.g., to make
	 * sure that "expr {0001}" yields "1", not "0001". We implement this
	 * by _discarding_ the string rep since we know it will be
	 * regenerated, if needed later, by formatting the internal rep's
	 * value.
	 */

	if (valuePtr->bytes == NULL) {
	    NEXT_INST_F(1, 0, 0);
	}
	if (Tcl_IsShared(valuePtr)) {
	    /*
	     * Here we do some surgery within the Tcl_Obj internals. We want
	     * to copy the intrep, but not the string, so we temporarily hide
	     * the string so we do not copy it.
	     */

	    char *savedString = valuePtr->bytes;

	    valuePtr->bytes = NULL;
	    objResultPtr = Tcl_DuplicateObj(valuePtr);
	    valuePtr->bytes = savedString;
	    NEXT_INST_F(1, 1, 1);
	}
	TclInvalidateStringRep(valuePtr);
	NEXT_INST_F(1, 0, 0);
    }
    
    /*
     * end of infinite loop dispatching on instructions.
     */
    
	default:
	    Tcl_Panic("TclNRExecuteByteCode: unrecognized opCode %u", *pc);
    } /* end of switch on opCode */
    Tcl_Panic("TclNRExecuteByteCode: this point should be unreachable");
    
    /*
     * Division by zero in an expression. Control only reaches this point
     * by "goto divideByZero".
     */

    divideByZero:
    Tcl_SetResult(interp, "divide by zero", TCL_STATIC);
    Tcl_SetErrorCode(interp, "ARITH", "DIVZERO", "divide by zero", NULL);
    return TCL_ERROR;;

    /*
     * Exponentiation of zero by negative number in an expression. Control
     * only reaches this point by "goto exponOfZero".
     */

    exponOfZero:
    Tcl_SetResult(interp, "exponentiation of zero by negative power",
	    TCL_STATIC);
    Tcl_SetErrorCode(interp, "ARITH", "DOMAIN",
	    "exponentiation of zero by negative power", NULL);
    return TCL_ERROR;;

}

#undef pc
#undef tosPtr
#undef codePtr
#undef iPtr
#undef initTosPtr
#undef auxObjList

/*
 *----------------------------------------------------------------------
 *
 * ExecuteExtendedBinaryMathOp, ExecuteExtendedUnaryMathOp --
 *
 *	These functions do advanced math for binary and unary operators
 *	respectively, so that the main TEBC code does not bear the cost of
 *	them.
 *
 * Results:
 *	A Tcl_Obj* result, or a NULL (in which case valuePtr is updated to
 *	hold the result value), or one of the special flag values
 *	GENERAL_ARITHMETIC_ERROR, EXPONENT_OF_ZERO or DIVIDED_BY_ZERO. The
 *	latter two signify a zero value raised to a negative power or a value
 *	divided by zero, respectively. With GENERAL_ARITHMETIC_ERROR, all
 *	error information will have already been reported in the interpreter
 *	result.
 *
 * Side effects:
 *	May update the Tcl_Obj indicated valuePtr if it is unshared. Will
 *	return a NULL when that happens.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ExecuteExtendedBinaryMathOp(
    Tcl_Interp *interp,		/* Where to report errors. */
    int opcode,			/* What operation to perform. */
    Tcl_Obj *valuePtr,		/* The first operand on the stack. */
    Tcl_Obj *value2Ptr)		/* The second operand on the stack. */
{
#define LONG_RESULT(l) \
    if (Tcl_IsShared(valuePtr)) {		\
	TclNewLongObj(objResultPtr, l);		\
	return objResultPtr;			\
    } else {					\
	Tcl_SetLongObj(valuePtr, l);		\
	return NULL;				\
    }
#define WIDE_RESULT(w) \
    if (Tcl_IsShared(valuePtr)) {		\
	return Tcl_NewWideIntObj(w);		\
    } else {					\
	Tcl_SetWideIntObj(valuePtr, w);		\
	return NULL;				\
    }
#define BIG_RESULT(b) \
    if (Tcl_IsShared(valuePtr)) {		\
	return Tcl_NewBignumObj(b);		\
    } else {					\
	Tcl_SetBignumObj(valuePtr, b);		\
	return NULL;				\
    }
#define DOUBLE_RESULT(d) \
    if (Tcl_IsShared(valuePtr)) {		\
	TclNewDoubleObj(objResultPtr, (d));	\
	return objResultPtr;			\
    } else {					\
	Tcl_SetDoubleObj(valuePtr, (d));	\
	return NULL;				\
    }

    int type1, type2;
    ClientData ptr1, ptr2;
    double d1, d2, dResult;
    long l1, l2, lResult;
    Tcl_WideInt w1, w2, wResult;
    mp_int big1, big2, bigResult, bigRemainder;
    Tcl_Obj *objResultPtr;
    int invalid, numPos, zero;
    long shift;

    (void) GetNumberFromObj(NULL, valuePtr, &ptr1, &type1);
    (void) GetNumberFromObj(NULL, value2Ptr, &ptr2, &type2);

    switch (opcode) {
    case INST_MOD:
	/* TODO: Attempts to re-use unshared operands on stack */

	l2 = 0;			/* silence gcc warning */
	if (type2 == TCL_NUMBER_LONG) {
	    l2 = *((const long *)ptr2);
	    if (l2 == 0) {
		return DIVIDED_BY_ZERO;
	    }
	    if ((l2 == 1) || (l2 == -1)) {
		/*
		 * Div. by |1| always yields remainder of 0.
		 */

		return Tcl_NewIntObj(0);
	    }
	}
#ifndef NO_WIDE_TYPE
	if (type1 == TCL_NUMBER_WIDE) {
	    w1 = *((const Tcl_WideInt *)ptr1);
	    if (type2 != TCL_NUMBER_BIG) {
		Tcl_WideInt wQuotient, wRemainder;
		Tcl_GetWideIntFromObj(NULL, value2Ptr, &w2);
		wQuotient = w1 / w2;

		/*
		 * Force Tcl's integer division rules.
		 * TODO: examine for logic simplification
		 */

		if (((wQuotient < (Tcl_WideInt) 0)
			|| ((wQuotient == (Tcl_WideInt) 0)
			&& ((w1 < (Tcl_WideInt)0 && w2 > (Tcl_WideInt)0)
			|| (w1 > (Tcl_WideInt)0 && w2 < (Tcl_WideInt)0))))
			&& (wQuotient * w2 != w1)) {
		    wQuotient -= (Tcl_WideInt) 1;
		}
		wRemainder = w1 - w2*wQuotient;
		WIDE_RESULT(wRemainder);
	    }

	    Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);

	    /* TODO: internals intrusion */
	    if ((w1 > ((Tcl_WideInt) 0)) ^ (big2.sign == MP_ZPOS)) {
		/*
		 * Arguments are opposite sign; remainder is sum.
		 */

		TclBNInitBignumFromWideInt(&big1, w1);
		mp_add(&big2, &big1, &big2);
		mp_clear(&big1);
		BIG_RESULT(&big2);
	    }

	    /*
	     * Arguments are same sign; remainder is first operand.
	     */

	    mp_clear(&big2);
	    return NULL;
	}
#endif
	Tcl_GetBignumFromObj(NULL, valuePtr, &big1);
	Tcl_GetBignumFromObj(NULL, value2Ptr, &big2);
	mp_init(&bigResult);
	mp_init(&bigRemainder);
	mp_div(&big1, &big2, &bigResult, &bigRemainder);
	if (!mp_iszero(&bigRemainder) && (bigRemainder.sign != big2.sign)) {
	    /*
	     * Convert to Tcl's integer division rules.
	     */

	    mp_sub_d(&bigResult, 1, &bigResult);
	    mp_add(&bigRemainder, &big2, &bigRemainder);
	}
	mp_copy(&bigRemainder, &bigResult);
	mp_clear(&bigRemainder);
	mp_clear(&big1);
	mp_clear(&big2);
	BIG_RESULT(&bigResult);

    case INST_LSHIFT:
    case INST_RSHIFT: {
	/*
	 * Reject negative shift argument.
	 */

	switch (type2) {
	case TCL_NUMBER_LONG:
	    invalid = (*((const long *)ptr2) < 0L);
	    break;
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
	    invalid = (*((const Tcl_WideInt *)ptr2) < (Tcl_WideInt)0);
	    break;
#endif
	case TCL_NUMBER_BIG:
	    Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);
	    invalid = (mp_cmp_d(&big2, 0) == MP_LT);
	    mp_clear(&big2);
	    break;
	default:
	    /* Unused, here to silence compiler warning */
	    invalid = 0;
	}
	if (invalid) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "negative shift argument", -1));
	    return GENERAL_ARITHMETIC_ERROR;
	}

	/*
	 * Zero shifted any number of bits is still zero.
	 */

	if ((type1==TCL_NUMBER_LONG) && (*((const long *)ptr1) == (long)0)) {
	    return Tcl_NewIntObj(0);
	}

	if (opcode == INST_LSHIFT) {
	    /*
	     * Large left shifts create integer overflow.
	     *
	     * BEWARE! Can't use Tcl_GetIntFromObj() here because that
	     * converts values in the (unsigned) range to their signed int
	     * counterparts, leading to incorrect results.
	     */

	    if ((type2 != TCL_NUMBER_LONG)
		    || (*((const long *)ptr2) > (long) INT_MAX)) {
		/*
		 * Technically, we could hold the value (1 << (INT_MAX+1)) in
		 * an mp_int, but since we're using mp_mul_2d() to do the
		 * work, and it takes only an int argument, that's a good
		 * place to draw the line.
		 */

		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"integer value too large to represent", -1));
		return GENERAL_ARITHMETIC_ERROR;
	    }
	    shift = (int)(*((const long *)ptr2));

	    /*
	     * Handle shifts within the native wide range.
	     */

	    if ((type1 != TCL_NUMBER_BIG)
		    && ((size_t)shift < CHAR_BIT*sizeof(Tcl_WideInt))) {
		TclGetWideIntFromObj(NULL, valuePtr, &w1);
		if (!((w1>0 ? w1 : ~w1)
			& -(((Tcl_WideInt)1)
			<< (CHAR_BIT*sizeof(Tcl_WideInt) - 1 - shift)))) {
		    WIDE_RESULT(w1 << shift);
		}
	    }
	} else {
	    /*
	     * Quickly force large right shifts to 0 or -1.
	     */

	    if ((type2 != TCL_NUMBER_LONG)
		    || (*(const long *)ptr2 > INT_MAX)) {
		/*
		 * Again, technically, the value to be shifted could be an
		 * mp_int so huge that a right shift by (INT_MAX+1) bits could
		 * not take us to the result of 0 or -1, but since we're using
		 * mp_div_2d to do the work, and it takes only an int
		 * argument, we draw the line there.
		 */

		switch (type1) {
		case TCL_NUMBER_LONG:
		    zero = (*(const long *)ptr1 > 0L);
		    break;
#ifndef NO_WIDE_TYPE
		case TCL_NUMBER_WIDE:
		    zero = (*(const Tcl_WideInt *)ptr1 > (Tcl_WideInt)0);
		    break;
#endif
		case TCL_NUMBER_BIG:
		    Tcl_TakeBignumFromObj(NULL, valuePtr, &big1);
		    zero = (mp_cmp_d(&big1, 0) == MP_GT);
		    mp_clear(&big1);
		    break;
		default:
		    /* Unused, here to silence compiler warning. */
		    zero = 0;
		}
		if (zero) {
		    return Tcl_NewIntObj(0);
		}
		LONG_RESULT(-1);
	    }
	    shift = (int)(*(const long *)ptr2);

#ifndef NO_WIDE_TYPE
	    /*
	     * Handle shifts within the native wide range.
	     */

	    if (type1 == TCL_NUMBER_WIDE) {
		w1 = *(const Tcl_WideInt *)ptr1;
		if ((size_t)shift >= CHAR_BIT*sizeof(Tcl_WideInt)) {
		    if (w1 >= (Tcl_WideInt)0) {
			return Tcl_NewIntObj(0);
		    }
		    LONG_RESULT(-1);
		}
		WIDE_RESULT(w1 >> shift);
	    }
#endif
	}

	Tcl_TakeBignumFromObj(NULL, valuePtr, &big1);

	mp_init(&bigResult);
	if (opcode == INST_LSHIFT) {
	    mp_mul_2d(&big1, shift, &bigResult);
	} else {
	    mp_init(&bigRemainder);
	    mp_div_2d(&big1, shift, &bigResult, &bigRemainder);
	    if (mp_cmp_d(&bigRemainder, 0) == MP_LT) {
		/*
		 * Convert to Tcl's integer division rules.
		 */

		mp_sub_d(&bigResult, 1, &bigResult);
	    }
	    mp_clear(&bigRemainder);
	}
	mp_clear(&big1);
	BIG_RESULT(&bigResult);
    }

    case INST_BITOR:
    case INST_BITXOR:
    case INST_BITAND:
	if ((type1 == TCL_NUMBER_BIG) || (type2 == TCL_NUMBER_BIG)) {
	    mp_int *First, *Second;

	    Tcl_TakeBignumFromObj(NULL, valuePtr, &big1);
	    Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);

	    /*
	     * Count how many positive arguments we have. If only one of the
	     * arguments is negative, store it in 'Second'.
	     */

	    if (mp_cmp_d(&big1, 0) != MP_LT) {
		numPos = 1 + (mp_cmp_d(&big2, 0) != MP_LT);
		First = &big1;
		Second = &big2;
	    } else {
		First = &big2;
		Second = &big1;
		numPos = (mp_cmp_d(First, 0) != MP_LT);
	    }
	    mp_init(&bigResult);

	    switch (opcode) {
	    case INST_BITAND:
		switch (numPos) {
		case 2:
		    /*
		     * Both arguments positive, base case.
		     */

		    mp_and(First, Second, &bigResult);
		    break;
		case 1:
		    /*
		     * First is positive; second negative:
		     * P & N = P & ~~N = P&~(-N-1) = P & (P ^ (-N-1))
		     */

		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_xor(First, Second, &bigResult);
		    mp_and(First, &bigResult, &bigResult);
		    break;
		case 0:
		    /*
		     * Both arguments negative:
		     * a & b = ~ (~a | ~b) = -(-a-1|-b-1)-1
		     */

		    mp_neg(First, First);
		    mp_sub_d(First, 1, First);
		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_or(First, Second, &bigResult);
		    mp_neg(&bigResult, &bigResult);
		    mp_sub_d(&bigResult, 1, &bigResult);
		    break;
		}
		break;

	    case INST_BITOR:
		switch (numPos) {
		case 2:
		    /*
		     * Both arguments positive, base case.
		     */

		    mp_or(First, Second, &bigResult);
		    break;
		case 1:
		    /*
		     * First is positive; second negative:
		     * N|P = ~(~N&~P) = ~((-N-1)&~P) = -((-N-1)&((-N-1)^P))-1
		     */

		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_xor(First, Second, &bigResult);
		    mp_and(Second, &bigResult, &bigResult);
		    mp_neg(&bigResult, &bigResult);
		    mp_sub_d(&bigResult, 1, &bigResult);
		    break;
		case 0:
		    /*
		     * Both arguments negative:
		     * a | b = ~ (~a & ~b) = -(-a-1&-b-1)-1
		     */

		    mp_neg(First, First);
		    mp_sub_d(First, 1, First);
		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_and(First, Second, &bigResult);
		    mp_neg(&bigResult, &bigResult);
		    mp_sub_d(&bigResult, 1, &bigResult);
		    break;
		}
		break;

	    case INST_BITXOR:
		switch (numPos) {
		case 2:
		    /*
		     * Both arguments positive, base case.
		     */

		    mp_xor(First, Second, &bigResult);
		    break;
		case 1:
		    /*
		     * First is positive; second negative:
		     * P^N = ~(P^~N) = -(P^(-N-1))-1
		     */

		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_xor(First, Second, &bigResult);
		    mp_neg(&bigResult, &bigResult);
		    mp_sub_d(&bigResult, 1, &bigResult);
		    break;
		case 0:
		    /*
		     * Both arguments negative:
		     * a ^ b = (~a ^ ~b) = (-a-1^-b-1)
		     */

		    mp_neg(First, First);
		    mp_sub_d(First, 1, First);
		    mp_neg(Second, Second);
		    mp_sub_d(Second, 1, Second);
		    mp_xor(First, Second, &bigResult);
		    break;
		}
		break;
	    }

	    mp_clear(&big1);
	    mp_clear(&big2);
	    BIG_RESULT(&bigResult);
	}

#ifndef NO_WIDE_TYPE
	if ((type1 == TCL_NUMBER_WIDE) || (type2 == TCL_NUMBER_WIDE)) {
	    TclGetWideIntFromObj(NULL, valuePtr, &w1);
	    TclGetWideIntFromObj(NULL, value2Ptr, &w2);

	    switch (opcode) {
	    case INST_BITAND:
		wResult = w1 & w2;
		break;
	    case INST_BITOR:
		wResult = w1 | w2;
		break;
	    case INST_BITXOR:
		wResult = w1 ^ w2;
		break;
	    default:
		/* Unused, here to silence compiler warning. */
		wResult = 0;
	    }
	    WIDE_RESULT(wResult);
	}
#endif
	l1 = *((const long *)ptr1);
	l2 = *((const long *)ptr2);

	switch (opcode) {
	case INST_BITAND:
	    lResult = l1 & l2;
	    break;
	case INST_BITOR:
	    lResult = l1 | l2;
	    break;
	case INST_BITXOR:
	    lResult = l1 ^ l2;
	    break;
	default:
	    /* Unused, here to silence compiler warning. */
	    lResult = 0;
	}
	LONG_RESULT(lResult);

    case INST_EXPON: {
	int oddExponent = 0, negativeExponent = 0;
	unsigned short base;

	if ((type1 == TCL_NUMBER_DOUBLE) || (type2 == TCL_NUMBER_DOUBLE)) {
	    Tcl_GetDoubleFromObj(NULL, valuePtr, &d1);
	    Tcl_GetDoubleFromObj(NULL, value2Ptr, &d2);

	    if (d1==0.0 && d2<0.0) {
		return EXPONENT_OF_ZERO;
	    }
	    dResult = pow(d1, d2);
	    goto doubleResult;
	}
	l1 = l2 = 0;
	if (type2 == TCL_NUMBER_LONG) {
	    l2 = *((const long *) ptr2);
	    if (l2 == 0) {
		/*
		 * Anything to the zero power is 1.
		 */

		return Tcl_NewIntObj(1);
	    } else if (l2 == 1) {
		/*
		 * Anything to the first power is itself
		 */

		return NULL;
	    }
	}

	switch (type2) {
	case TCL_NUMBER_LONG:
	    negativeExponent = (l2 < 0);
	    oddExponent = (int) (l2 & 1);
	    break;
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
	    w2 = *((const Tcl_WideInt *)ptr2);
	    negativeExponent = (w2 < 0);
	    oddExponent = (int) (w2 & (Tcl_WideInt)1);
	    break;
#endif
	case TCL_NUMBER_BIG:
	    Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);
	    negativeExponent = (mp_cmp_d(&big2, 0) == MP_LT);
	    mp_mod_2d(&big2, 1, &big2);
	    oddExponent = !mp_iszero(&big2);
	    mp_clear(&big2);
	    break;
	}

	if (type1 == TCL_NUMBER_LONG) {
	    l1 = *((const long *)ptr1);
	}
	if (negativeExponent) {
	    if (type1 == TCL_NUMBER_LONG) {
		switch (l1) {
		case 0:
		    /*
		     * Zero to a negative power is div by zero error.
		     */

		    return EXPONENT_OF_ZERO;
		case -1:
		    if (oddExponent) {
			LONG_RESULT(-1);
		    }
		    /* fallthrough */
		case 1:
		    /*
		     * 1 to any power is 1.
		     */

		    return Tcl_NewIntObj(1);
		}
	    }

	    /*
	     * Integers with magnitude greater than 1 raise to a negative
	     * power yield the answer zero (see TIP 123).
	     */

	    return Tcl_NewIntObj(0);
	}

	if (type1 == TCL_NUMBER_LONG) {
	    switch (l1) {
	    case 0:
		/*
		 * Zero to a positive power is zero.
		 */

		return Tcl_NewIntObj(0);
	    case 1:
		/*
		 * 1 to any power is 1.
		 */

		return Tcl_NewIntObj(1);
	    case -1:
		if (!oddExponent) {
		    return Tcl_NewIntObj(1);
		}
		LONG_RESULT(-1);
	    }
	}

	/*
	 * We refuse to accept exponent arguments that exceed one mp_digit
	 * which means the max exponent value is 2**28-1 = 0x0fffffff =
	 * 268435455, which fits into a signed 32 bit int which is within the
	 * range of the long int type. This means any numeric Tcl_Obj value
	 * not using TCL_NUMBER_LONG type must hold a value larger than we
	 * accept.
	 */

	if (type2 != TCL_NUMBER_LONG) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "exponent too large", -1));
	    return GENERAL_ARITHMETIC_ERROR;
	}

	if (type1 == TCL_NUMBER_LONG) {
	    if (l1 == 2) {
		/*
		 * Reduce small powers of 2 to shifts.
		 */

		if ((unsigned long) l2 < CHAR_BIT * sizeof(long) - 1) {
		    LONG_RESULT(1L << l2);
		}
#if !defined(TCL_WIDE_INT_IS_LONG)
		if ((unsigned long)l2 < CHAR_BIT*sizeof(Tcl_WideInt) - 1) {
		    WIDE_RESULT(((Tcl_WideInt) 1) << l2);
		}
#endif
		goto overflowExpon;
	    }
	    if (l1 == -2) {
		int signum = oddExponent ? -1 : 1;

		/*
		 * Reduce small powers of 2 to shifts.
		 */

		if ((unsigned long) l2 < CHAR_BIT * sizeof(long) - 1) {
		    LONG_RESULT(signum * (1L << l2));
		}
#if !defined(TCL_WIDE_INT_IS_LONG)
		if ((unsigned long)l2 < CHAR_BIT*sizeof(Tcl_WideInt) - 1){
		    WIDE_RESULT(signum * (((Tcl_WideInt) 1) << l2));
		}
#endif
		goto overflowExpon;
	    }
#if (LONG_MAX == 0x7fffffff)
	    if (l2 - 2 < (long)MaxBase32Size
		    && l1 <= MaxBase32[l2 - 2]
		    && l1 >= -MaxBase32[l2 - 2]) {
		/*
		 * Small powers of 32-bit integers.
		 */

		lResult = l1 * l1;		/* b**2 */
		switch (l2) {
		case 2:
		    break;
		case 3:
		    lResult *= l1;		/* b**3 */
		    break;
		case 4:
		    lResult *= lResult;		/* b**4 */
		    break;
		case 5:
		    lResult *= lResult;		/* b**4 */
		    lResult *= l1;		/* b**5 */
		    break;
		case 6:
		    lResult *= l1;		/* b**3 */
		    lResult *= lResult;		/* b**6 */
		    break;
		case 7:
		    lResult *= l1;		/* b**3 */
		    lResult *= lResult;		/* b**6 */
		    lResult *= l1;		/* b**7 */
		    break;
		case 8:
		    lResult *= lResult;		/* b**4 */
		    lResult *= lResult;		/* b**8 */
		    break;
		}
		LONG_RESULT(lResult);
	    }

	    if (l1 - 3 >= 0 && l1 -2 < (long)Exp32IndexSize
		    && l2 - 2 < (long)(Exp32ValueSize + MaxBase32Size)) {
		base = Exp32Index[l1 - 3]
			+ (unsigned short) (l2 - 2 - MaxBase32Size);
		if (base < Exp32Index[l1 - 2]) {
		    /*
		     * 32-bit number raised to intermediate power, done by
		     * table lookup.
		     */

		    LONG_RESULT(Exp32Value[base]);
		}
	    }
	    if (-l1 - 3 >= 0 && -l1 - 2 < (long)Exp32IndexSize
		    && l2 - 2 < (long)(Exp32ValueSize + MaxBase32Size)) {
		base = Exp32Index[-l1 - 3]
			+ (unsigned short) (l2 - 2 - MaxBase32Size);
		if (base < Exp32Index[-l1 - 2]) {
		    /*
		     * 32-bit number raised to intermediate power, done by
		     * table lookup.
		     */

		    lResult = (oddExponent) ?
			    -Exp32Value[base] : Exp32Value[base];
		    LONG_RESULT(lResult);
		}
	    }
#endif
	}
#if (LONG_MAX > 0x7fffffff) || !defined(TCL_WIDE_INT_IS_LONG)
	if (type1 == TCL_NUMBER_LONG) {
	    w1 = l1;
#ifndef NO_WIDE_TYPE
	} else if (type1 == TCL_NUMBER_WIDE) {
	    w1 = *((const Tcl_WideInt *) ptr1);
#endif
	} else {
	    goto overflowExpon;
	}
	if (l2 - 2 < (long)MaxBase64Size
		&& w1 <=  MaxBase64[l2 - 2]
		&& w1 >= -MaxBase64[l2 - 2]) {
	    /*
	     * Small powers of integers whose result is wide.
	     */

	    wResult = w1 * w1;		/* b**2 */
	    switch (l2) {
	    case 2:
		break;
	    case 3:
		wResult *= l1;		/* b**3 */
		break;
	    case 4:
		wResult *= wResult;	/* b**4 */
		break;
	    case 5:
		wResult *= wResult;	/* b**4 */
		wResult *= w1;		/* b**5 */
		break;
	    case 6:
		wResult *= w1;		/* b**3 */
		wResult *= wResult;	/* b**6 */
		break;
	    case 7:
		wResult *= w1;		/* b**3 */
		wResult *= wResult;	/* b**6 */
		wResult *= w1;		/* b**7 */
		break;
	    case 8:
		wResult *= wResult;	/* b**4 */
		wResult *= wResult;	/* b**8 */
		break;
	    case 9:
		wResult *= wResult;	/* b**4 */
		wResult *= wResult;	/* b**8 */
		wResult *= w1;		/* b**9 */
		break;
	    case 10:
		wResult *= wResult;	/* b**4 */
		wResult *= w1;		/* b**5 */
		wResult *= wResult;	/* b**10 */
		break;
	    case 11:
		wResult *= wResult;	/* b**4 */
		wResult *= w1;		/* b**5 */
		wResult *= wResult;	/* b**10 */
		wResult *= w1;		/* b**11 */
		break;
	    case 12:
		wResult *= w1;		/* b**3 */
		wResult *= wResult;	/* b**6 */
		wResult *= wResult;	/* b**12 */
		break;
	    case 13:
		wResult *= w1;		/* b**3 */
		wResult *= wResult;	/* b**6 */
		wResult *= wResult;	/* b**12 */
		wResult *= w1;		/* b**13 */
		break;
	    case 14:
		wResult *= w1;		/* b**3 */
		wResult *= wResult;	/* b**6 */
		wResult *= w1;		/* b**7 */
		wResult *= wResult;	/* b**14 */
		break;
	    case 15:
		wResult *= w1;		/* b**3 */
		wResult *= wResult;	/* b**6 */
		wResult *= w1;		/* b**7 */
		wResult *= wResult;	/* b**14 */
		wResult *= w1;		/* b**15 */
		break;
	    case 16:
		wResult *= wResult;	/* b**4 */
		wResult *= wResult;	/* b**8 */
		wResult *= wResult;	/* b**16 */
		break;
	    }
	    WIDE_RESULT(wResult);
	}

	/*
	 * Handle cases of powers > 16 that still fit in a 64-bit word by
	 * doing table lookup.
	 */

	if (w1 - 3 >= 0 && w1 - 2 < (long)Exp64IndexSize
		&& l2 - 2 < (long)(Exp64ValueSize + MaxBase64Size)) {
	    base = Exp64Index[w1 - 3]
		    + (unsigned short) (l2 - 2 - MaxBase64Size);
	    if (base < Exp64Index[w1 - 2]) {
		/*
		 * 64-bit number raised to intermediate power, done by
		 * table lookup.
		 */

		WIDE_RESULT(Exp64Value[base]);
	    }
	}

	if (-w1 - 3 >= 0 && -w1 - 2 < (long)Exp64IndexSize
		&& l2 - 2 < (long)(Exp64ValueSize + MaxBase64Size)) {
	    base = Exp64Index[-w1 - 3]
		    + (unsigned short) (l2 - 2 - MaxBase64Size);
	    if (base < Exp64Index[-w1 - 2]) {
		/*
		 * 64-bit number raised to intermediate power, done by
		 * table lookup.
		 */

		wResult = oddExponent ? -Exp64Value[base] : Exp64Value[base];
		WIDE_RESULT(wResult);
	    }
	}
#endif

    overflowExpon:
	Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);
	if (big2.used > 1) {
	    mp_clear(&big2);
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "exponent too large", -1));
	    return GENERAL_ARITHMETIC_ERROR;
	}
	Tcl_TakeBignumFromObj(NULL, valuePtr, &big1);
	mp_init(&bigResult);
	mp_expt_d(&big1, big2.dp[0], &bigResult);
	mp_clear(&big1);
	mp_clear(&big2);
	BIG_RESULT(&bigResult);
    }

    case INST_ADD:
    case INST_SUB:
    case INST_MULT:
    case INST_DIV:
	if ((type1 == TCL_NUMBER_DOUBLE) || (type2 == TCL_NUMBER_DOUBLE)) {
	    /*
	     * At least one of the values is floating-point, so perform
	     * floating point calculations.
	     */

	    Tcl_GetDoubleFromObj(NULL, valuePtr, &d1);
	    Tcl_GetDoubleFromObj(NULL, value2Ptr, &d2);

	    switch (opcode) {
	    case INST_ADD:
		dResult = d1 + d2;
		break;
	    case INST_SUB:
		dResult = d1 - d2;
		break;
	    case INST_MULT:
		dResult = d1 * d2;
		break;
	    case INST_DIV:
#ifndef IEEE_FLOATING_POINT
		if (d2 == 0.0) {
		    return DIVIDED_BY_ZERO;
		}
#endif
		/*
		 * We presume that we are running with zero-divide unmasked if
		 * we're on an IEEE box. Otherwise, this statement might cause
		 * demons to fly out our noses.
		 */

		dResult = d1 / d2;
		break;
	    default:
		/* Unused, here to silence compiler warning. */
		dResult = 0;
	    }

	doubleResult:
#ifndef ACCEPT_NAN
	    /*
	     * Check now for IEEE floating-point error.
	     */

	    if (TclIsNaN(dResult)) {
		TclExprFloatError(interp, dResult);
		return GENERAL_ARITHMETIC_ERROR;
	    }
#endif
	    DOUBLE_RESULT(dResult);
	}
	if ((type1 != TCL_NUMBER_BIG) && (type2 != TCL_NUMBER_BIG)) {
	    TclGetWideIntFromObj(NULL, valuePtr, &w1);
	    TclGetWideIntFromObj(NULL, value2Ptr, &w2);

	    switch (opcode) {
	    case INST_ADD:
		wResult = w1 + w2;
#ifndef NO_WIDE_TYPE
		if ((type1 == TCL_NUMBER_WIDE) || (type2 == TCL_NUMBER_WIDE))
#endif
		{
		    /*
		     * Check for overflow.
		     */

		    if (Overflowing(w1, w2, wResult)) {
			goto overflowBasic;
		    }
		}
		break;

	    case INST_SUB:
		wResult = w1 - w2;
#ifndef NO_WIDE_TYPE
		if ((type1 == TCL_NUMBER_WIDE) || (type2 == TCL_NUMBER_WIDE))
#endif
		{
		    /*
		     * Must check for overflow. The macro tests for overflows
		     * in sums by looking at the sign bits. As we have a
		     * subtraction here, we are adding -w2. As -w2 could in
		     * turn overflow, we test with ~w2 instead: it has the
		     * opposite sign bit to w2 so it does the job. Note that
		     * the only "bad" case (w2==0) is irrelevant for this
		     * macro, as in that case w1 and wResult have the same
		     * sign and there is no overflow anyway.
		     */

		    if (Overflowing(w1, ~w2, wResult)) {
			goto overflowBasic;
		    }
		}
		break;

	    case INST_MULT:
		if ((type1 != TCL_NUMBER_LONG) || (type2 != TCL_NUMBER_LONG)
			|| (sizeof(Tcl_WideInt) < 2*sizeof(long))) {
		    goto overflowBasic;
		}
		wResult = w1 * w2;
		break;

	    case INST_DIV:
		if (w2 == 0) {
		    return DIVIDED_BY_ZERO;
		}

		/*
		 * Need a bignum to represent (LLONG_MIN / -1)
		 */

		if ((w1 == LLONG_MIN) && (w2 == -1)) {
		    goto overflowBasic;
		}
		wResult = w1 / w2;

		/*
		 * Force Tcl's integer division rules.
		 * TODO: examine for logic simplification
		 */

		if (((wResult < 0) || ((wResult == 0) &&
			((w1 < 0 && w2 > 0) || (w1 > 0 && w2 < 0)))) &&
			(wResult*w2 != w1)) {
		    wResult -= 1;
		}
		break;

	    default:
		/*
		 * Unused, here to silence compiler warning.
		 */

		wResult = 0;
	    }

	    WIDE_RESULT(wResult);
	}

    overflowBasic:
	Tcl_TakeBignumFromObj(NULL, valuePtr, &big1);
	Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);
	mp_init(&bigResult);
	switch (opcode) {
	case INST_ADD:
	    mp_add(&big1, &big2, &bigResult);
	    break;
	case INST_SUB:
	    mp_sub(&big1, &big2, &bigResult);
	    break;
	case INST_MULT:
	    mp_mul(&big1, &big2, &bigResult);
	    break;
	case INST_DIV:
	    if (mp_iszero(&big2)) {
		mp_clear(&big1);
		mp_clear(&big2);
		mp_clear(&bigResult);
		return DIVIDED_BY_ZERO;
	    }
	    mp_init(&bigRemainder);
	    mp_div(&big1, &big2, &bigResult, &bigRemainder);
	    /* TODO: internals intrusion */
	    if (!mp_iszero(&bigRemainder)
		    && (bigRemainder.sign != big2.sign)) {
		/*
		 * Convert to Tcl's integer division rules.
		 */

		mp_sub_d(&bigResult, 1, &bigResult);
		mp_add(&bigRemainder, &big2, &bigRemainder);
	    }
	    mp_clear(&bigRemainder);
	    break;
	}
	mp_clear(&big1);
	mp_clear(&big2);
	BIG_RESULT(&bigResult);
    }

    Tcl_Panic("unexpected opcode");
    return NULL;
}

static Tcl_Obj *
ExecuteExtendedUnaryMathOp(
    int opcode,			/* What operation to perform. */
    Tcl_Obj *valuePtr)		/* The operand on the stack. */
{
    ClientData ptr;
    int type;
    Tcl_WideInt w;
    mp_int big;
    Tcl_Obj *objResultPtr;

    (void) GetNumberFromObj(NULL, valuePtr, &ptr, &type);

    switch (opcode) {
    case INST_BITNOT:
#ifndef NO_WIDE_TYPE
	if (type == TCL_NUMBER_WIDE) {
	    w = *((const Tcl_WideInt *) ptr);
	    WIDE_RESULT(~w);
	}
#endif
	Tcl_TakeBignumFromObj(NULL, valuePtr, &big);
	/* ~a = - a - 1 */
	mp_neg(&big, &big);
	mp_sub_d(&big, 1, &big);
	BIG_RESULT(&big);
    case INST_UMINUS:
	switch (type) {
	case TCL_NUMBER_DOUBLE:
	    DOUBLE_RESULT(-(*((const double *) ptr)));
	case TCL_NUMBER_LONG:
	    w = (Tcl_WideInt) (*((const long *) ptr));
	    if (w != LLONG_MIN) {
		WIDE_RESULT(-w);
	    }
	    TclBNInitBignumFromLong(&big, *(const long *) ptr);
	    break;
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
	    w = *((const Tcl_WideInt *) ptr);
	    if (w != LLONG_MIN) {
		WIDE_RESULT(-w);
	    }
	    TclBNInitBignumFromWideInt(&big, w);
	    break;
#endif
	default:
	    Tcl_TakeBignumFromObj(NULL, valuePtr, &big);
	}
	mp_neg(&big, &big);
	BIG_RESULT(&big);
    }

    Tcl_Panic("unexpected opcode");
    return NULL;
}
#undef LONG_RESULT
#undef WIDE_RESULT
#undef BIG_RESULT
#undef DOUBLE_RESULT

/*
 *----------------------------------------------------------------------
 *
 * CompareTwoNumbers --
 *
 *	This function compares a pair of numbers in Tcl_Objs. Each argument
 *	must already be known to be numeric and not NaN.
 *
 * Results:
 *	One of MP_LT, MP_EQ or MP_GT, depending on whether valuePtr is less
 *	than, equal to, or greater than value2Ptr (respectively).
 *
 * Side effects:
 *	None, provided both values are numeric.
 *
 *----------------------------------------------------------------------
 */

int
TclCompareTwoNumbers(
    Tcl_Obj *valuePtr,
    Tcl_Obj *value2Ptr)
{
    int type1, type2, compare;
    ClientData ptr1, ptr2;
    mp_int big1, big2;
    double d1, d2, tmp;
    long l1, l2;
#ifndef NO_WIDE_TYPE
    Tcl_WideInt w1, w2;
#endif

    (void) GetNumberFromObj(NULL, valuePtr, &ptr1, &type1);
    (void) GetNumberFromObj(NULL, value2Ptr, &ptr2, &type2);

    switch (type1) {
    case TCL_NUMBER_LONG:
	l1 = *((const long *)ptr1);
	switch (type2) {
	case TCL_NUMBER_LONG:
	    l2 = *((const long *)ptr2);
	longCompare:
	    return (l1 < l2) ? MP_LT : ((l1 > l2) ? MP_GT : MP_EQ);
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
	    w2 = *((const Tcl_WideInt *)ptr2);
	    w1 = (Tcl_WideInt)l1;
	    goto wideCompare;
#endif
	case TCL_NUMBER_DOUBLE:
	    d2 = *((const double *)ptr2);
	    d1 = (double) l1;

	    /*
	     * If the double has a fractional part, or if the long can be
	     * converted to double without loss of precision, then compare as
	     * doubles.
	     */

	    if (DBL_MANT_DIG > CHAR_BIT*sizeof(long) || l1 == (long) d1
		    || modf(d2, &tmp) != 0.0) {
		goto doubleCompare;
	    }

	    /*
	     * Otherwise, to make comparision based on full precision, need to
	     * convert the double to a suitably sized integer.
	     *
	     * Need this to get comparsions like
	     *	  expr 20000000000000003 < 20000000000000004.0
	     * right. Converting the first argument to double will yield two
	     * double values that are equivalent within double precision.
	     * Converting the double to an integer gets done exactly, then
	     * integer comparison can tell the difference.
	     */

	    if (d2 < (double)LONG_MIN) {
		return MP_GT;
	    }
	    if (d2 > (double)LONG_MAX) {
		return MP_LT;
	    }
	    l2 = (long) d2;
	    goto longCompare;
	case TCL_NUMBER_BIG:
	    Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);
	    if (mp_cmp_d(&big2, 0) == MP_LT) {
		compare = MP_GT;
	    } else {
		compare = MP_LT;
	    }
	    mp_clear(&big2);
	    return compare;
	}

#ifndef NO_WIDE_TYPE
    case TCL_NUMBER_WIDE:
	w1 = *((const Tcl_WideInt *)ptr1);
	switch (type2) {
	case TCL_NUMBER_WIDE:
	    w2 = *((const Tcl_WideInt *)ptr2);
	wideCompare:
	    return (w1 < w2) ? MP_LT : ((w1 > w2) ? MP_GT : MP_EQ);
	case TCL_NUMBER_LONG:
	    l2 = *((const long *)ptr2);
	    w2 = (Tcl_WideInt)l2;
	    goto wideCompare;
	case TCL_NUMBER_DOUBLE:
	    d2 = *((const double *)ptr2);
	    d1 = (double) w1;
	    if (DBL_MANT_DIG > CHAR_BIT*sizeof(Tcl_WideInt)
		    || w1 == (Tcl_WideInt) d1 || modf(d2, &tmp) != 0.0) {
		goto doubleCompare;
	    }
	    if (d2 < (double)LLONG_MIN) {
		return MP_GT;
	    }
	    if (d2 > (double)LLONG_MAX) {
		return MP_LT;
	    }
	    w2 = (Tcl_WideInt) d2;
	    goto wideCompare;
	case TCL_NUMBER_BIG:
	    Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);
	    if (mp_cmp_d(&big2, 0) == MP_LT) {
		compare = MP_GT;
	    } else {
		compare = MP_LT;
	    }
	    mp_clear(&big2);
	    return compare;
	}
#endif

    case TCL_NUMBER_DOUBLE:
	d1 = *((const double *)ptr1);
	switch (type2) {
	case TCL_NUMBER_DOUBLE:
	    d2 = *((const double *)ptr2);
	doubleCompare:
	    return (d1 < d2) ? MP_LT : ((d1 > d2) ? MP_GT : MP_EQ);
	case TCL_NUMBER_LONG:
	    l2 = *((const long *)ptr2);
	    d2 = (double) l2;
	    if (DBL_MANT_DIG > CHAR_BIT*sizeof(long) || l2 == (long) d2
		    || modf(d1, &tmp) != 0.0) {
		goto doubleCompare;
	    }
	    if (d1 < (double)LONG_MIN) {
		return MP_LT;
	    }
	    if (d1 > (double)LONG_MAX) {
		return MP_GT;
	    }
	    l1 = (long) d1;
	    goto longCompare;
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
	    w2 = *((const Tcl_WideInt *)ptr2);
	    d2 = (double) w2;
	    if (DBL_MANT_DIG > CHAR_BIT*sizeof(Tcl_WideInt)
		    || w2 == (Tcl_WideInt) d2 || modf(d1, &tmp) != 0.0) {
		goto doubleCompare;
	    }
	    if (d1 < (double)LLONG_MIN) {
		return MP_LT;
	    }
	    if (d1 > (double)LLONG_MAX) {
		return MP_GT;
	    }
	    w1 = (Tcl_WideInt) d1;
	    goto wideCompare;
#endif
	case TCL_NUMBER_BIG:
	    if (TclIsInfinite(d1)) {
		return (d1 > 0.0) ? MP_GT : MP_LT;
	    }
	    Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);
	    if ((d1 < (double)LONG_MAX) && (d1 > (double)LONG_MIN)) {
		if (mp_cmp_d(&big2, 0) == MP_LT) {
		    compare = MP_GT;
		} else {
		    compare = MP_LT;
		}
		mp_clear(&big2);
		return compare;
	    }
	    if (DBL_MANT_DIG > CHAR_BIT*sizeof(long)
		    && modf(d1, &tmp) != 0.0) {
		d2 = TclBignumToDouble(&big2);
		mp_clear(&big2);
		goto doubleCompare;
	    }
	    Tcl_InitBignumFromDouble(NULL, d1, &big1);
	    goto bigCompare;
	}

    case TCL_NUMBER_BIG:
	Tcl_TakeBignumFromObj(NULL, valuePtr, &big1);
	switch (type2) {
#ifndef NO_WIDE_TYPE
	case TCL_NUMBER_WIDE:
#endif
	case TCL_NUMBER_LONG:
	    compare = mp_cmp_d(&big1, 0);
	    mp_clear(&big1);
	    return compare;
	case TCL_NUMBER_DOUBLE:
	    d2 = *((const double *)ptr2);
	    if (TclIsInfinite(d2)) {
		compare = (d2 > 0.0) ? MP_LT : MP_GT;
		mp_clear(&big1);
		return compare;
	    }
	    if ((d2 < (double)LONG_MAX) && (d2 > (double)LONG_MIN)) {
		compare = mp_cmp_d(&big1, 0);
		mp_clear(&big1);
		return compare;
	    }
	    if (DBL_MANT_DIG > CHAR_BIT*sizeof(long)
		    && modf(d2, &tmp) != 0.0) {
		d1 = TclBignumToDouble(&big1);
		mp_clear(&big1);
		goto doubleCompare;
	    }
	    Tcl_InitBignumFromDouble(NULL, d2, &big2);
	    goto bigCompare;
	case TCL_NUMBER_BIG:
	    Tcl_TakeBignumFromObj(NULL, value2Ptr, &big2);
	bigCompare:
	    compare = mp_cmp(&big1, &big2);
	    mp_clear(&big1);
	    mp_clear(&big2);
	    return compare;
	}
    default:
	Tcl_Panic("unexpected number type");
	return TCL_ERROR;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * IllegalExprOperandType --
 *
 *	Used by TclNRExecuteByteCode to append an error message to the interp
 *	result when an illegal operand type is detected by an expression
 *	instruction. The argument opndPtr holds the operand object in error.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An error message is appended to the interp result.
 *
 *----------------------------------------------------------------------
 */

static void
IllegalExprOperandType(
    Tcl_Interp *interp,		/* Interpreter to which error information
				 * pertains. */
    const unsigned char *pc, /* Points to the instruction being executed
				 * when the illegal type was found. */
    Tcl_Obj *opndPtr)		/* Points to the operand holding the value
				 * with the illegal type. */
{
    ClientData ptr;
    int type;
    const unsigned char opcode = *pc;
    const char *description, *operator = operatorStrings[opcode - INST_BITOR];

    if (GetNumberFromObj(NULL, opndPtr, &ptr, &type) != TCL_OK) {
	int numBytes;
	const char *bytes = Tcl_GetStringFromObj(opndPtr, &numBytes);

	if (numBytes == 0) {
	    description = "empty string";
	} else if (TclCheckBadOctal(NULL, bytes)) {
	    description = "invalid octal number";
	} else {
	    description = "non-numeric string";
	}
    } else if (type == TCL_NUMBER_NAN) {
	description = "non-numeric floating-point value";
    } else if (type == TCL_NUMBER_DOUBLE) {
	description = "floating-point value";
    } else {
	/* TODO: No caller needs this. Eliminate? */
	description = "(big) integer";
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "can't use %s as operand of \"%s\"", description, operator));
    Tcl_SetErrorCode(interp, "ARITH", "DOMAIN", description, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetSrcInfoForPc, GetSrcInfoForPc, TclGetSrcInfoForCmd --
 *
 *	Given a program counter value, finds the closest command in the
 *	bytecode code unit's CmdLocation array and returns information about
 *	that command's source: a pointer to its first byte and the number of
 *	characters.
 *
 * Results:
 *	If a command is found that encloses the program counter value, a
 *	pointer to the command's source is returned and the length of the
 *	source is stored at *lengthPtr. If multiple commands resulted in code
 *	at pc, information about the closest enclosing command is returned. If
 *	no matching command is found, NULL is returned and *lengthPtr is
 *	unchanged.
 *
 * Side effects:
 *	The CmdFrame at *cfPtr is updated.
 *
 *----------------------------------------------------------------------
 */

static const char *
GetSrcInfoForPc(
    const unsigned char *pc,	/* The program counter value for which to
				 * return the closest command's source info.
				 * This points within a bytecode instruction
				 * in codePtr's code. */
    ByteCode *codePtr,		/* The bytecode sequence in which to look up
				 * the command source for the pc. */
    int *lengthPtr,		/* If non-NULL, the location where the length
				 * of the command's source should be stored.
				 * If NULL, no length is stored. */
    const unsigned char **pcBeg)/* If non-NULL, the bytecode location
				 * where the current instruction starts.
				 * If NULL; no pointer is stored. */
{
    register int pcOffset = (pc - codePtr->codeStart);
    int numCmds = codePtr->numCommands;
    unsigned char *codeDeltaNext, *codeLengthNext;
    unsigned char *srcDeltaNext, *srcLengthNext;
    int codeOffset, codeLen, codeEnd, srcOffset, srcLen, delta, i;
    int bestDist = INT_MAX;	/* Distance of pc to best cmd's start pc. */
    int bestSrcOffset = -1;	/* Initialized to avoid compiler warning. */
    int bestSrcLength = -1;	/* Initialized to avoid compiler warning. */

    if ((pcOffset < 0) || (pcOffset >= codePtr->numCodeBytes)) {
	if (pcBeg != NULL) *pcBeg = NULL;
	return NULL;
    }

    /*
     * Decode the code and source offset and length for each command. The
     * closest enclosing command is the last one whose code started before
     * pcOffset.
     */

    codeDeltaNext = codePtr->codeDeltaStart;
    codeLengthNext = codePtr->codeLengthStart;
    srcDeltaNext = codePtr->srcDeltaStart;
    srcLengthNext = codePtr->srcLengthStart;
    codeOffset = srcOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	if ((unsigned) *codeDeltaNext == (unsigned) 0xFF) {
	    codeDeltaNext++;
	    delta = TclGetInt4AtPtr(codeDeltaNext);
	    codeDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(codeDeltaNext);
	    codeDeltaNext++;
	}
	codeOffset += delta;

	if ((unsigned) *codeLengthNext == (unsigned) 0xFF) {
	    codeLengthNext++;
	    codeLen = TclGetInt4AtPtr(codeLengthNext);
	    codeLengthNext += 4;
	} else {
	    codeLen = TclGetInt1AtPtr(codeLengthNext);
	    codeLengthNext++;
	}
	codeEnd = (codeOffset + codeLen - 1);

	if ((unsigned) *srcDeltaNext == (unsigned) 0xFF) {
	    srcDeltaNext++;
	    delta = TclGetInt4AtPtr(srcDeltaNext);
	    srcDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(srcDeltaNext);
	    srcDeltaNext++;
	}
	srcOffset += delta;

	if ((unsigned) *srcLengthNext == (unsigned) 0xFF) {
	    srcLengthNext++;
	    srcLen = TclGetInt4AtPtr(srcLengthNext);
	    srcLengthNext += 4;
	} else {
	    srcLen = TclGetInt1AtPtr(srcLengthNext);
	    srcLengthNext++;
	}

	if (codeOffset > pcOffset) {	/* Best cmd already found */
	    break;
	}
	if (pcOffset <= codeEnd) {	/* This cmd's code encloses pc */
	    int dist = (pcOffset - codeOffset);

	    if (dist <= bestDist) {
		bestDist = dist;
		bestSrcOffset = srcOffset;
		bestSrcLength = srcLen;
	    }
	}
    }

    if (pcBeg != NULL) {
	const unsigned char *curr, *prev;

	/*
	 * Walk from beginning of command or BC to pc, by complete
	 * instructions. Stop when crossing pc; keep previous.
	 */

	curr = ((bestDist == INT_MAX) ? codePtr->codeStart : pc - bestDist);
	prev = curr;
	while (curr <= pc) {
	    prev = curr;
	    curr += tclInstructionTable[*curr].numBytes;
	}
	*pcBeg = prev;
    }

    if (bestDist == INT_MAX) {
	return NULL;
    }

    if (lengthPtr != NULL) {
	*lengthPtr = bestSrcLength;
    }

    return (codePtr->source + bestSrcOffset);
}

/*
 *----------------------------------------------------------------------
 *
 * TclExprFloatError --
 *
 *	This procedure is called when an error occurs during a floating-point
 *	operation. It reads errno and sets interp->objResultPtr accordingly.
 *
 * Results:
 *	interp->objResultPtr is set to hold an error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclExprFloatError(
    Tcl_Interp *interp,		/* Where to store error message. */
    double value)		/* Value returned after error; used to
				 * distinguish underflows from overflows. */
{
    const char *s;

    if ((errno == EDOM) || TclIsNaN(value)) {
	s = "domain error: argument not in valid range";
	Tcl_SetObjResult(interp, Tcl_NewStringObj(s, -1));
	Tcl_SetErrorCode(interp, "ARITH", "DOMAIN", s, NULL);
    } else if ((errno == ERANGE) || TclIsInfinite(value)) {
	if (value == 0.0) {
	    s = "floating-point value too small to represent";
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(s, -1));
	    Tcl_SetErrorCode(interp, "ARITH", "UNDERFLOW", s, NULL);
	} else {
	    s = "floating-point value too large to represent";
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(s, -1));
	    Tcl_SetErrorCode(interp, "ARITH", "OVERFLOW", s, NULL);
	}
    } else {
	Tcl_Obj *objPtr = Tcl_ObjPrintf(
		"unknown floating-point error, errno = %d", errno);

	Tcl_SetErrorCode(interp, "ARITH", "UNKNOWN",
		Tcl_GetString(objPtr), NULL);
	Tcl_SetObjResult(interp, objPtr);
    }
}



/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
