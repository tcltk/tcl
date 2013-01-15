/*
 * tclCompCmdsSZ.c --
 *
 *	This file contains compilation procedures that compile various Tcl
 *	commands (beginning with the letters 's' through 'z', except for
 *	[upvar] and [variable]) into a sequence of instructions ("bytecodes").
 *	Also includes the operator command compilers.
 *
 * Copyright (c) 1997-1998 Sun Microsystems, Inc.
 * Copyright (c) 2001 by Kevin B. Kenny.  All rights reserved.
 * Copyright (c) 2002 ActiveState Corporation.
 * Copyright (c) 2004-2010 by Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclCompileInt.h"

/*
 * Prototypes for procedures defined later in this file:
 */

static int		CompileAssociativeBinaryOpCmd(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, const char *identity,
			    int instruction, CompileEnv *envPtr);
static int		CompileComparisonOpCmd(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, int instruction,
			    CompileEnv *envPtr);
static int		CompileStrictlyBinaryOpCmd(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, int instruction,
			    CompileEnv *envPtr);
static int		CompileUnaryOpCmd(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, int instruction,
			    CompileEnv *envPtr);

/*
 * Macro that encapsulates an efficiency trick that avoids a function call for
 * the simplest of compiles. The ANSI C "prototype" for this macro is:
 *
 * static void		CompileWord(CompileEnv *envPtr, Tcl_Token *tokenPtr,
 *			    Tcl_Interp *interp, int word);
 */

#define CompileWord(envPtr, tokenPtr, interp, word) \
    if ((tokenPtr)->type == TCL_TOKEN_SIMPLE_WORD) {			\
	TclEmitPush(TclRegisterNewLiteral((envPtr), (tokenPtr)[1].start, \
		(tokenPtr)[1].size), (envPtr));				\
    } else {								\
	TclCompileTokens((interp), (tokenPtr)+1, (tokenPtr)->numComponents, \
		(envPtr));						\
    }

/*
 * Flags bits used by PushVarName.
 */

#define TCL_NO_LARGE_INDEX 1	/* Do not return localIndex value > 255 */

/*
 * Shorthand macros for instruction issuing.
 */

#define OP(name)	TclEmitOpcode(INST_##name, envPtr)
#define OP1(name,val)	TclEmitInstInt1(INST_##name,(val),envPtr)
#define OP4(name,val)	TclEmitInstInt4(INST_##name,(val),envPtr)
#define OP14(name,val1,val2) \
    TclEmitInstInt1(INST_##name,(val1),envPtr);TclEmitInt4((val2),envPtr)
#define OP44(name,val1,val2) \
    TclEmitInstInt4(INST_##name,(val1),envPtr);TclEmitInt4((val2),envPtr)
#define BODY(token,index) \
    CompileBody(envPtr,(token),interp)
#define PUSH(str) \
    PushLiteral(envPtr,(str),strlen(str))
#define JUMP(var,name) \
    (var) = CurrentOffset(envPtr);TclEmitInstInt4(INST_##name,0,envPtr)
#define FIXJUMP(var) \
    TclStoreInt4AtPtr(CurrentOffset(envPtr)-(var),envPtr->codeStart+(var)+1)
#define LOAD(idx) \
    OP4(LOAD_SCALAR4,(idx))
#define STORE(idx) \
    OP4(STORE_SCALAR4,(idx))

/*
 *----------------------------------------------------------------------
 *
 * CompileUnaryOpCmd --
 *
 *	Utility routine to compile the unary operator commands.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the compiled command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileUnaryOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    int instruction,
    CompileEnv *envPtr)
{
    Tcl_Token *tokenPtr;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    CompileWord(envPtr, tokenPtr, interp, 1);
    TclEmitOpcode(instruction, envPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileAssociativeBinaryOpCmd --
 *
 *	Utility routine to compile the binary operator commands that accept an
 *	arbitrary number of arguments, and that are associative operations.
 *	Because of the associativity, we may combine operations from right to
 *	left, saving us any effort of re-ordering the arguments on the stack
 *	after substitutions are completed.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the compiled command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileAssociativeBinaryOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    const char *identity,
    int instruction,
    CompileEnv *envPtr)
{
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    int words;

    for (words=1 ; words<parsePtr->numWords ; words++) {
	tokenPtr = TokenAfter(tokenPtr);
	CompileWord(envPtr, tokenPtr, interp, words);
    }
    if (parsePtr->numWords <= 2) {
	PushLiteral(envPtr, identity, -1);
	words++;
    }
    if (words > 3) {
	/*
	 * Reverse order of arguments to get precise agreement with [expr] in
	 * calcuations, including roundoff errors.
	 */

	OP4(	REVERSE, words-1);
    }
    while (--words > 1) {
	TclEmitOpcode(instruction, envPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileStrictlyBinaryOpCmd --
 *
 *	Utility routine to compile the binary operator commands, that strictly
 *	accept exactly two arguments.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the compiled command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileStrictlyBinaryOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    int instruction,
    CompileEnv *envPtr)
{
    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }
    return CompileAssociativeBinaryOpCmd(interp, parsePtr,
	    NULL, instruction, envPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CompileComparisonOpCmd --
 *
 *	Utility routine to compile the n-ary comparison operator commands.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the compiled command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileComparisonOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    int instruction,
    CompileEnv *envPtr)
{
    Tcl_Token *tokenPtr;

    if (parsePtr->numWords < 3) {
	PushLiteral(envPtr, "1", 1);
    } else if (parsePtr->numWords == 3) {
	tokenPtr = TokenAfter(parsePtr->tokenPtr);
	CompileWord(envPtr, tokenPtr, interp, 1);
	tokenPtr = TokenAfter(tokenPtr);
	CompileWord(envPtr, tokenPtr, interp, 2);
	TclEmitOpcode(instruction, envPtr);
    } else if (envPtr->procPtr == NULL) {
	/*
	 * No local variable space!
	 */

	return TCL_ERROR;
    } else {
	int tmpIndex = TclFindCompiledLocal(NULL, 0, 1, envPtr);
	int words;

	tokenPtr = TokenAfter(parsePtr->tokenPtr);
	CompileWord(envPtr, tokenPtr, interp, 1);
	tokenPtr = TokenAfter(tokenPtr);
	CompileWord(envPtr, tokenPtr, interp, 2);
	STORE(tmpIndex);
	TclEmitOpcode(instruction, envPtr);
	for (words=3 ; words<parsePtr->numWords ;) {
	    LOAD(tmpIndex);
	    tokenPtr = TokenAfter(tokenPtr);
	    CompileWord(envPtr, tokenPtr, interp, words);
	    if (++words < parsePtr->numWords) {
		STORE(tmpIndex);
	    }
	    TclEmitOpcode(instruction, envPtr);
	}
	for (; words>3 ; words--) {
	    OP(	BITAND);
	}

	/*
	 * Drop the value from the temp variable; retaining that reference
	 * might be expensive elsewhere.
	 */

	OP14(	UNSET_SCALAR, 0, tmpIndex);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompile*OpCmd --
 *
 *	Procedures called to compile the corresponding "::tcl::mathop::*"
 *	commands. These are all wrappers around the utility operator command
 *	compiler functions, except for the compilers for subtraction and
 *	division, which are special.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the compiled command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileInvertOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileUnaryOpCmd(interp, parsePtr, INST_BITNOT, envPtr);
}

int
TclCompileNotOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileUnaryOpCmd(interp, parsePtr, INST_LNOT, envPtr);
}

int
TclCompileAddOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileAssociativeBinaryOpCmd(interp, parsePtr, "0", INST_ADD,
	    envPtr);
}

int
TclCompileMulOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileAssociativeBinaryOpCmd(interp, parsePtr, "1", INST_MULT,
	    envPtr);
}

int
TclCompileAndOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileAssociativeBinaryOpCmd(interp, parsePtr, "-1", INST_BITAND,
	    envPtr);
}

int
TclCompileOrOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileAssociativeBinaryOpCmd(interp, parsePtr, "0", INST_BITOR,
	    envPtr);
}

int
TclCompileXorOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileAssociativeBinaryOpCmd(interp, parsePtr, "0", INST_BITXOR,
	    envPtr);
}

int
TclCompilePowOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    /*
     * This one has its own implementation because the ** operator is the only
     * one with right associativity.
     */

    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    int words;

    for (words=1 ; words<parsePtr->numWords ; words++) {
	tokenPtr = TokenAfter(tokenPtr);
	CompileWord(envPtr, tokenPtr, interp, words);
    }
    if (parsePtr->numWords <= 2) {
	PushLiteral(envPtr, "1", 1);
	words++;
    }
    while (--words > 1) {
	TclEmitOpcode(INST_EXPON, envPtr);
    }
    return TCL_OK;
}

int
TclCompileLshiftOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_LSHIFT, envPtr);
}

int
TclCompileRshiftOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_RSHIFT, envPtr);
}

int
TclCompileModOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_MOD, envPtr);
}

int
TclCompileNeqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_NEQ, envPtr);
}

int
TclCompileStrneqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_STR_NEQ, envPtr);
}

int
TclCompileInOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_LIST_IN, envPtr);
}

int
TclCompileNiOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_LIST_NOT_IN,
	    envPtr);
}

int
TclCompileLessOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_LT, envPtr);
}

int
TclCompileLeqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_LE, envPtr);
}

int
TclCompileGreaterOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_GT, envPtr);
}

int
TclCompileGeqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_GE, envPtr);
}

int
TclCompileEqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_EQ, envPtr);
}

int
TclCompileStreqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_STR_EQ, envPtr);
}

int
TclCompileMinusOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    int words;

    if (parsePtr->numWords == 1) {
	/*
	 * Fallback to direct eval to report syntax error.
	 */

	return TCL_ERROR;
    }
    for (words=1 ; words<parsePtr->numWords ; words++) {
	tokenPtr = TokenAfter(tokenPtr);
	CompileWord(envPtr, tokenPtr, interp, words);
    }
    if (words == 2) {
	TclEmitOpcode(INST_UMINUS, envPtr);
	return TCL_OK;
    }
    if (words == 3) {
	TclEmitOpcode(INST_SUB, envPtr);
	return TCL_OK;
    }

    /*
     * Reverse order of arguments to get precise agreement with [expr] in
     * calcuations, including roundoff errors.
     */

    TclEmitInstInt4(INST_REVERSE, words-1, envPtr);
    while (--words > 1) {
	TclEmitInstInt4(INST_REVERSE, 2, envPtr);
	TclEmitOpcode(INST_SUB, envPtr);
    }
    return TCL_OK;
}

int
TclCompileDivOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    int words;

    if (parsePtr->numWords == 1) {
	/*
	 * Fallback to direct eval to report syntax error.
	 */

	return TCL_ERROR;
    }
    if (parsePtr->numWords == 2) {
	PushLiteral(envPtr, "1.0", 3);
    }
    for (words=1 ; words<parsePtr->numWords ; words++) {
	tokenPtr = TokenAfter(tokenPtr);
	CompileWord(envPtr, tokenPtr, interp, words);
    }
    if (words <= 3) {
	TclEmitOpcode(INST_DIV, envPtr);
	return TCL_OK;
    }

    /*
     * Reverse order of arguments to get precise agreement with [expr] in
     * calcuations, including roundoff errors.
     */

    TclEmitInstInt4(INST_REVERSE, words-1, envPtr);
    while (--words > 1) {
	TclEmitInstInt4(INST_REVERSE, 2, envPtr);
	TclEmitOpcode(INST_DIV, envPtr);
    }
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
