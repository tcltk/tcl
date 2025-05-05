/*
 * tclCompUtils.h --
 *
 *	This file contains utility macros for generating Tcl bytecode.
 *
 * Copyright (c) 2025 Donal K. Fellows <dkf@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLCOMPUTILS
#define _TCLCOMPUTILS 1

#include "tclCompile.h"

/*
 * The type of "labels" used in FWDLABEL() and BACKLABEL(). 
 */
typedef Tcl_Size Tcl_BytecodeLabel;

/*
 * The type of "catch ranges" used in CATCH_RANGE(), etc.
 */
typedef Tcl_Size Tcl_ExceptionRange;

/*
 * The type of indices into the local variable table.
 */
typedef Tcl_Size Tcl_LVTIndex;

/*
 * The type of handles made by TclCreateAuxData()
 */
typedef Tcl_Size Tcl_AuxDataRef;

/*
 * Used to indicate that no jump is pending resolution.
 */
#define NO_PENDING_JUMP		((Tcl_Size) -1)

/*
 * Shorthand macros for instruction issuing.
 */

#define OP(name)	TclEmitOpcode(INST_##name, envPtr)
#define OP1(name,val)	TclEmitInstInt1(INST_##name,(val),envPtr)
#define OP4(name,val)	TclEmitInstInt4(INST_##name,(val),envPtr)

#define OP14(name,val1,val2) \
    TclEmitInstInt14(INST_##name,(val1),(val2),envPtr)
#define OP44(name,val1,val2) \
    TclEmitInstInt44(INST_##name,(val1),(val2),envPtr)
#define OP41(name,val1,val2) \
    TclEmitInstInt41(INST_##name,(val1),(val2),envPtr)

#define PUSH(str) \
    PushStringLiteral(envPtr, str)
#define PUSH_STRING(strVar) \
    PushLiteral(envPtr, (strVar), TCL_AUTO_LENGTH)
#define PUSH_SIMPLE_TOKEN(tokenPtr) \
    PushLiteral(envPtr, (tokenPtr)[1].start, (tokenPtr)[1].size)
#define PUSH_OBJ(objPtr) \
    TclEmitPush(TclAddLiteralObj(envPtr, (objPtr), NULL), envPtr)
#define PUSH_TOKEN(tokenPtr, index) \
    CompileWord(envPtr, (tokenPtr), interp, (index))
#define PUSH_EXPR_TOKEN(tokenPtr, index) \
    do {								\
	SetLineInformation(index);					\
	TclCompileExprWords(interp, (tokenPtr), 1, envPtr);		\
    } while (0)
#define BODY(tokenPtr, index) \
    do {								\
	SetLineInformation((index));					\
	TclCompileCmdWord(interp,					\
		(tokenPtr)+1, (tokenPtr)->numComponents,		\
		envPtr);						\
    } while (0)

#define BACKLABEL(var) \
    (var)=CurrentOffset(envPtr)
#define BACKJUMP(name, var) \
    TclEmitInstInt4(INST_##name,(var)-CurrentOffset(envPtr),envPtr)
#define FWDJUMP(name,var) \
    (var)=CurrentOffset(envPtr);TclEmitInstInt4(INST_##name,0,envPtr)
#define FWDLABEL(var) \
    TclStoreInt4AtPtr(CurrentOffset(envPtr)-(var),envPtr->codeStart+(var)+1)
#define INVOKE(name) \
    TclEmitInvoke(envPtr,INST_##name)

#define MAKE_CATCH_RANGE() \
    TclCreateExceptRange(CATCH_EXCEPTION_RANGE, envPtr)
#define MAKE_LOOP_RANGE() \
    TclCreateExceptRange(LOOP_EXCEPTION_RANGE, envPtr)
#define CATCH_RANGE_VAR(range,var) \
    for(int var=(ExceptionRangeStarts(envPtr,(range)), 0);		\
	    !var;							\
	    var=(ExceptionRangeEnds(envPtr,(range)), 1))
#define CATCH_RANGE(range) \
    CATCH_RANGE_VAR((range), JOIN(catchRange_, __LINE__))
#define CATCH_TARGET(range) \
    ExceptionRangeTarget(envPtr, (range), catchOffset)
#define BREAK_TARGET(range) \
    ExceptionRangeTarget(envPtr, (range), breakOffset)
#define CONTINUE_TARGET(range) \
    ExceptionRangeTarget(envPtr, (range), continueOffset)
#define FINALIZE_LOOP(range) \
    TclFinalizeLoopExceptionRange(envPtr, (range))

#define STKDELTA(delta) \
    TclAdjustStackDepth((delta), envPtr)

#define TokenToObj(tokenPtr) \
    Tcl_NewStringObj((tokenPtr)[1].start, (tokenPtr)[1].size)
#define LENGTH_OF(str) \
    ((Tcl_Size) sizeof(str "") - 1)
#define IS_TOKEN_LITERALLY(tokenPtr, str) \
    (((tokenPtr)->type == TCL_TOKEN_SIMPLE_WORD)			\
	    && ((tokenPtr)[1].size == LENGTH_OF(str))			\
	    && strncmp((tokenPtr)[1].start, str, LENGTH_OF(str)) == 0)
#define IS_TOKEN_PREFIX(tokenPtr, minLength, str) \
    (((tokenPtr)->type == TCL_TOKEN_SIMPLE_WORD)			\
	    && ((tokenPtr)[1].size >= (Tcl_Size)(minLength))		\
	    && ((tokenPtr)[1].size <= LENGTH_OF(str))			\
	    && strncmp((tokenPtr)[1].start, str, (tokenPtr)[1].size) == 0)
#define IS_TOKEN_PREFIXED_BY(tokenPtr, str) \
    (((tokenPtr)->type == TCL_TOKEN_SIMPLE_WORD)			\
	    && ((tokenPtr)[1].size > LENGTH_OF(str))			\
	    && strncmp((tokenPtr)[1].start, str, LENGTH_OF(str)) == 0)
#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
