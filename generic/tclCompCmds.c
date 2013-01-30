/*
 * tclCompCmds.c --
 *
 *	This file contains compilation procedures that compile various Tcl
 *	commands into a sequence of instructions ("bytecodes").
 *
 * Copyright (c) 1997-1998 Sun Microsystems, Inc.
 * Copyright (c) 2001 by Kevin B. Kenny.  All rights reserved.
 * Copyright (c) 2002 ActiveState Corporation.
 * Copyright (c) 2004-2006 by Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclCompile.h"
#include <assert.h>

/*
 * Prototypes for procedures defined later in this file:
 */

static ClientData	DupDictUpdateInfo(ClientData clientData);
static void		FreeDictUpdateInfo(ClientData clientData);
static void		PrintDictUpdateInfo(ClientData clientData,
			    Tcl_Obj *appendObj, ByteCode *codePtr,
			    unsigned int pcOffset);
static ClientData	DupForeachInfo(ClientData clientData);
static void		FreeForeachInfo(ClientData clientData);
static void		PrintForeachInfo(ClientData clientData,
			    Tcl_Obj *appendObj, ByteCode *codePtr,
			    unsigned int pcOffset);
static void		CompileReturnInternal(CompileEnv *envPtr,
			    unsigned char op, int code, int level,
			    Tcl_Obj *returnOpts);
static int		IndexTailVarIfKnown(Tcl_Interp *interp,
			    Tcl_Token *varTokenPtr, CompileEnv *envPtr);
static int		CompileEachloopCmd(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, Command *cmdPtr,
			    CompileEnv *envPtr, int collect);
static int		CompileDictEachCmd(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, Command *cmdPtr,
			    struct CompileEnv *envPtr, int collect);

/*
 * The structures below define the AuxData types defined in this file.
 */

const AuxDataType tclForeachInfoType = {
    "ForeachInfo",		/* name */
    DupForeachInfo,		/* dupProc */
    FreeForeachInfo,		/* freeProc */
    PrintForeachInfo		/* printProc */
};

const AuxDataType tclDictUpdateInfoType = {
    "DictUpdateInfo",		/* name */
    DupDictUpdateInfo,		/* dupProc */
    FreeDictUpdateInfo,		/* freeProc */
    PrintDictUpdateInfo		/* printProc */
};

/*
 *----------------------------------------------------------------------
 *
 * TclCompileAppendCmd --
 *
 *	Procedure called to compile the "append" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "append" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileAppendCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *varTokenPtr;
    int simpleVarName, isScalar, localIndex, numWords;
    DefineLineInformation;	/* TIP #280 */

    numWords = parsePtr->numWords;
    if (numWords == 1) {
	return TCL_ERROR;
    } else if (numWords == 2) {
	/*
	 * append varName == set varName
	 */

	return TclCompileSetCmd(interp, parsePtr, cmdPtr, envPtr);
    } else if (numWords > 3) {
	/*
	 * APPEND instructions currently only handle one value.
	 */

	return TCL_ERROR;
    }

    /*
     * Decide if we can use a frame slot for the var/array name or if we need
     * to emit code to compute and push the name at runtime. We use a frame
     * slot (entry in the array of local vars) if we are compiling a procedure
     * body and if the name is simple text that does not include namespace
     * qualifiers.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);

    PUSH_VAR(	varTokenPtr, 1,
	    &localIndex, &simpleVarName, &isScalar);

    /*
     * We are doing an assignment, otherwise TclCompileSetCmd was called, so
     * push the new value. This will need to be extended to push a value for
     * each argument.
     */

    if (numWords > 2) {
	PUSH_SUBST_WORD(TokenAfter(varTokenPtr), 2);
    }

    /*
     * Emit instructions to set/get the variable.
     */

    if (simpleVarName) {
	if (isScalar) {
	    if (localIndex < 0) {
		OP(	APPEND_STK);
	    } else {
		OP4(	APPEND_SCALAR, localIndex);
	    }
	} else {
	    if (localIndex < 0) {
		OP(	APPEND_ARRAY_STK);
	    } else {
		OP4(	APPEND_ARRAY, localIndex);
	    }
	}
    } else {
	OP(		APPEND_STK);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileArray*Cmd --
 *
 *	Functions called to compile "array" sucommands.
 *
 * Results:
 *	All return TCL_OK for a successful compile, and TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "array" subcommand at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileArrayExistsCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    int simpleVarName, isScalar, localIndex;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_VAR(	tokenPtr, 1,
	    &localIndex, &simpleVarName, &isScalar);
    if (!isScalar) {
	return TCL_ERROR;
    }

    if (localIndex >= 0) {
	OP4(	ARRAY_EXISTS_IMM, localIndex);
    } else {
	OP(	ARRAY_EXISTS_STK);
    }
    return TCL_OK;
}

int
TclCompileArraySetCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    int simpleVarName, isScalar, localIndex;
    int dataVar, iterVar, keyVar, valVar, infoIndex;
    int offsetBack, offsetFwd, savedStackDepth;
    ForeachInfo *infoPtr;

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_VAR(	tokenPtr, 1,
	    &localIndex, &simpleVarName, &isScalar);
    if (!isScalar) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(tokenPtr);

    /*
     * Special case: literal empty value argument is just an "ensure array"
     * operation.
     */

    if (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD && tokenPtr[1].size == 0) {
	if (localIndex >= 0) {
	    OP4(	ARRAY_EXISTS_IMM, localIndex);
	    OP4(	JUMP_TRUE, 10);
	    OP4(	ARRAY_MAKE_IMM, localIndex);
	} else {
	    OP(		DUP);
	    OP(		ARRAY_EXISTS_STK);
	    OP4(	JUMP_TRUE, 11);
	    savedStackDepth = envPtr->currStackDepth;
	    OP(		ARRAY_MAKE_STK);
	    OP4(	JUMP, 6);
	    envPtr->currStackDepth = savedStackDepth;
	    OP(		POP);
	}
	PUSH(		"");
	return TCL_OK;
    }

    /*
     * Prepare for the internal foreach.
     */

    if (envPtr->procPtr == NULL) {
	return TCL_ERROR;
    }
    dataVar = NewUnnamedLocal(envPtr);
    iterVar = NewUnnamedLocal(envPtr);
    keyVar = NewUnnamedLocal(envPtr);
    valVar = NewUnnamedLocal(envPtr);

    infoPtr = ckalloc(sizeof(ForeachInfo) + sizeof(ForeachVarList *));
    infoPtr->numLists = 1;
    infoPtr->firstValueTemp = dataVar;
    infoPtr->loopCtTemp = iterVar;
    infoPtr->varLists[0] = ckalloc(sizeof(ForeachVarList) * 2*sizeof(int));
    infoPtr->varLists[0]->numVars = 2;
    infoPtr->varLists[0]->varIndexes[0] = keyVar;
    infoPtr->varLists[0]->varIndexes[1] = valVar;
    infoIndex = TclCreateAuxData(infoPtr, &tclForeachInfoType, envPtr);

    /*
     * Start issuing instructions to write to the array.
     */

    PUSH_SUBST_WORD(	tokenPtr, 2);
    OP(			DUP);
    OP(			LIST_LENGTH);
    PUSH(		"1");
    OP(			BITAND);
    JUMP(offsetFwd,	JUMP_FALSE);
    savedStackDepth = envPtr->currStackDepth;
    PUSH(		"list must have an even number of elements");
    PUSH(		"-errorCode {TCL ARGUMENT FORMAT}");
    OP44(		RETURN_IMM, 1, 0);
    envPtr->currStackDepth = savedStackDepth;
    FIXJUMP(		offsetFwd);
    OP4(		STORE_SCALAR, dataVar);
    OP(			POP);

    if (localIndex >= 0) {
	OP4(		ARRAY_EXISTS_IMM, localIndex);
	OP4(		JUMP_TRUE, 10);
	OP4(		ARRAY_MAKE_IMM, localIndex);
	OP4(		FOREACH_START, infoIndex);
	LABEL(offsetBack);
	OP4(		FOREACH_STEP, infoIndex);
	JUMP(offsetFwd,	JUMP_FALSE);
	savedStackDepth = envPtr->currStackDepth;
	OP4(		LOAD_SCALAR, keyVar);
	OP4(		LOAD_SCALAR, valVar);
	OP4(		STORE_ARRAY, localIndex);
	OP(		POP);
	BACKJUMP(	offsetBack, JUMP);
	FIXJUMP(	offsetFwd);
	envPtr->currStackDepth = savedStackDepth;
    } else {
	OP(		DUP);
	OP(		ARRAY_EXISTS_STK);
	OP4(		JUMP_TRUE, 7);
	OP(		DUP);
	OP(		ARRAY_MAKE_STK);
	OP4(		FOREACH_START, infoIndex);
	LABEL(offsetBack);
	OP4(		FOREACH_STEP, infoIndex);
	JUMP(offsetFwd,	JUMP_FALSE);
	savedStackDepth = envPtr->currStackDepth;
	OP(		DUP);
	OP4(		LOAD_SCALAR, keyVar);
	OP4(		LOAD_SCALAR, valVar);
	OP(		STORE_ARRAY_STK);
	OP(		POP);
	BACKJUMP(	offsetBack, JUMP);
	FIXJUMP(	offsetFwd);
	envPtr->currStackDepth = savedStackDepth;
	OP(		POP);
    }
    OP14(		UNSET_SCALAR, 0, dataVar);
    PUSH(		"");
    return TCL_OK;
}

int
TclCompileArrayUnsetCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);
    int simpleVarName, isScalar, localIndex, savedStackDepth;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    PUSH_VAR(	tokenPtr, 1,
	    &localIndex, &simpleVarName, &isScalar);
    if (!isScalar) {
	return TCL_ERROR;
    }

    if (localIndex >= 0) {
	OP4(		ARRAY_EXISTS_IMM, localIndex);
	OP4(		JUMP_FALSE, 11);
	OP14(		UNSET_SCALAR, 1, localIndex);
    } else {
	OP(		DUP);
	OP(		ARRAY_EXISTS_STK);
	OP4(		JUMP_FALSE, 12);
	savedStackDepth = envPtr->currStackDepth;
	OP1(		UNSET_STK, 1);
	OP4(		JUMP, 6);
	envPtr->currStackDepth = savedStackDepth;
	OP(		POP);
    }
    PUSH(		"");
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileBreakCmd --
 *
 *	Procedure called to compile the "break" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "break" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileBreakCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    if (parsePtr->numWords != 1) {
	return TCL_ERROR;
    }

    /*
     * Emit a break instruction.
     */

    OP(		BREAK);
    PUSH(	"");		/* Evil hack! */
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileCatchCmd --
 *
 *	Procedure called to compile the "catch" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "catch" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileCatchCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    JumpFixup jumpFixup;
    Tcl_Token *cmdTokenPtr, *resultNameTokenPtr, *optsNameTokenPtr;
    const char *name;
    int resultIndex, optsIndex, nameChars, range;
    int initStackDepth = envPtr->currStackDepth;
    int savedStackDepth;
    DefineLineInformation;	/* TIP #280 */

    /*
     * If syntax does not match what we expect for [catch], do not compile.
     * Let runtime checks determine if syntax has changed.
     */

    if ((parsePtr->numWords < 2) || (parsePtr->numWords > 4)) {
	return TCL_ERROR;
    }

    /*
     * If variables were specified and the catch command is at global level
     * (not in a procedure), don't compile it inline: the payoff is too small.
     */

    if ((parsePtr->numWords >= 3) && !EnvHasLVT(envPtr)) {
	return TCL_ERROR;
    }

    /*
     * Make sure the variable names, if any, have no substitutions and just
     * refer to local scalars.
     */

    resultIndex = optsIndex = -1;
    cmdTokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (parsePtr->numWords >= 3) {
	resultNameTokenPtr = TokenAfter(cmdTokenPtr);
	/* DGP */
	if (resultNameTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    return TCL_ERROR;
	}

	name = resultNameTokenPtr[1].start;
	nameChars = resultNameTokenPtr[1].size;
	if (!TclIsLocalScalar(name, nameChars)) {
	    return TCL_ERROR;
	}
	resultIndex = TclFindCompiledLocal(resultNameTokenPtr[1].start,
		resultNameTokenPtr[1].size, /*create*/ 1, envPtr);
	if (resultIndex < 0) {
	    return TCL_ERROR;
	}

	/* DKF */
	if (parsePtr->numWords == 4) {
	    optsNameTokenPtr = TokenAfter(resultNameTokenPtr);
	    if (optsNameTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
		return TCL_ERROR;
	    }
	    name = optsNameTokenPtr[1].start;
	    nameChars = optsNameTokenPtr[1].size;
	    if (!TclIsLocalScalar(name, nameChars)) {
		return TCL_ERROR;
	    }
	    optsIndex = TclFindCompiledLocal(optsNameTokenPtr[1].start,
		    optsNameTokenPtr[1].size, /*create*/ 1, envPtr);
	    if (optsIndex < 0) {
		return TCL_ERROR;
	    }
	}
    }

    /*
     * We will compile the catch command. Declare the exception range that it
     * uses.
     */

    range = DeclareExceptionRange(envPtr, CATCH_EXCEPTION_RANGE);

    /*
     * If the body is a simple word, compile a BEGIN_CATCH instruction,
     * followed by the instructions to eval the body.
     * Otherwise, compile instructions to substitute the body text before
     * starting the catch, then BEGIN_CATCH, and then EVAL_STK to evaluate the
     * substituted body.
     * Care has to be taken to make sure that substitution happens outside the
     * catch range so that errors in the substitution are not caught.
     * [Bug 219184]
     * The reason for duplicating the script is that EVAL_STK would otherwise
     * begin by undeflowing the stack below the mark set by BEGIN_CATCH4.
     */

    if (cmdTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	savedStackDepth = envPtr->currStackDepth;
	OP4(	BEGIN_CATCH, range);
	ExceptionRangeStarts(envPtr, range);
	BODY(	cmdTokenPtr, 1);
    } else {
	PUSH_SUBST_WORD(cmdTokenPtr, 1);
	savedStackDepth = envPtr->currStackDepth;
	OP4(	BEGIN_CATCH, range);
	ExceptionRangeStarts(envPtr, range);
	OP(	DUP);
	OP(	EVAL_STK);
    }
    /* Stack at this point:
     *    nonsimple:  script <mark> result
     *    simple:            <mark> result
     */

    if (resultIndex == -1) {
	/*
	 * Special case when neither result nor options are being saved. In
	 * that case, we can skip quite a bit of the command epilogue; all we
	 * have to do is drop the result and push the return code (and, of
	 * course, finish the catch context).
	 */

	OP(	POP);
	PUSH(	"0");
	OP4(	JUMP, 6);
	envPtr->currStackDepth = savedStackDepth;
	ExceptionRangeTarget(envPtr, range, catchOffset);
	OP(	PUSH_RETURN_CODE);
	ExceptionRangeEnds(envPtr, range);
	OP(	END_CATCH);

	/*
	 * Stack at this point:
	 *    nonsimple:  script <mark> returnCode
	 *    simple:            <mark> returnCode
	 */

	goto dropScriptAtEnd;
    }

    /*
     * Emit the "no errors" epilogue: push "0" (TCL_OK) as the catch result,
     * and jump around the "error case" code.
     */

    PUSH(	"0");
    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpFixup);
    /* Stack at this point: ?script? <mark> result TCL_OK */

    /* 
     * Emit the "error case" epilogue. Push the interpreter result and the
     * return code.
     */

    envPtr->currStackDepth = savedStackDepth;
    ExceptionRangeTarget(envPtr, range, catchOffset);
    /* Stack at this point:  ?script? */
    OP(		PUSH_RESULT);
    OP(		PUSH_RETURN_CODE);

    /*
     * Update the target of the jump after the "no errors" code. 
     */

    /* Stack at this point: ?script? result returnCode */
    if (TclFixupForwardJumpToHere(envPtr, &jumpFixup, 127)) {
	Tcl_Panic("TclCompileCatchCmd: bad jump distance %d",
		(int)(CurrentOffset(envPtr) - jumpFixup.codeOffset));
    }

    /*
     * Push the return options if the caller wants them.
     */

    if (optsIndex != -1) {
	OP(	PUSH_RETURN_OPTIONS);
    }

    /*
     * End the catch
     */

    ExceptionRangeEnds(envPtr, range);
    OP(		END_CATCH);

    /*
     * At this point, the top of the stack is inconveniently ordered:
     *		?script? result returnCode ?returnOptions?
     * Reverse the stack to bring the result to the top.
     */

    if (optsIndex != -1) {
	OP4(	REVERSE, 3);
    } else {
	OP(	EXCH);
    }

    /*
     * Store the result and remove it from the stack.
     */

    OP4(	STORE_SCALAR, resultIndex);
    OP(		POP);

    /*
     * Stack is now ?script? ?returnOptions? returnCode.
     * If the options dict has been requested, it is buried on the stack under
     * the return code. Reverse the stack to bring it to the top, store it and
     * remove it from the stack.
     */

    if (optsIndex != -1) {
	OP(	EXCH);
	OP4(	STORE_SCALAR, optsIndex);
	OP(	POP);
    }

  dropScriptAtEnd:

    /*
     * Stack is now ?script? result. Get rid of the subst'ed script if it's
     * hanging arond.
     */

    if (cmdTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	OP(	EXCH);
	OP(	POP);
    }

    /* 
     * Result of all this, on either branch, should have been to leave one
     * operand -- the return code -- on the stack.
     */

    if (envPtr->currStackDepth != initStackDepth + 1) {
	Tcl_Panic("in TclCompileCatchCmd, currStackDepth = %d should be %d",
		  envPtr->currStackDepth, initStackDepth+1);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileContinueCmd --
 *
 *	Procedure called to compile the "continue" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "continue" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileContinueCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * There should be no argument after the "continue".
     */

    if (parsePtr->numWords != 1) {
	return TCL_ERROR;
    }

    /*
     * Emit a continue instruction.
     */

    OP(		CONTINUE);
    PUSH(	"");		/* Evil hack! */
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileDict*Cmd --
 *
 *	Functions called to compile "dict" sucommands.
 *
 * Results:
 *	All return TCL_OK for a successful compile, and TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "dict" subcommand at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileDictSetCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;
    int numWords, i;
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr;
    int dictVarIndex, nameChars;
    const char *name;

    /*
     * There must be at least one argument after the command.
     */

    if (parsePtr->numWords < 4) {
	return TCL_ERROR;
    }

    /*
     * The dictionary variable must be a local scalar that is knowable at
     * compile time; anything else exceeds the complexity of the opcode. So
     * discover what the index is.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (varTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }
    name = varTokenPtr[1].start;
    nameChars = varTokenPtr[1].size;
    if (!TclIsLocalScalar(name, nameChars)) {
	return TCL_ERROR;
    }
    dictVarIndex = TclFindCompiledLocal(name, nameChars, 1, envPtr);
    if (dictVarIndex < 0) {
	return TCL_ERROR;
    }

    /*
     * Remaining words (key path and value to set) can be handled normally.
     */

    tokenPtr = TokenAfter(varTokenPtr);
    numWords = parsePtr->numWords-1;
    for (i=1 ; i<numWords ; i++) {
	PUSH_SUBST_WORD(tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }

    /*
     * Now emit the instruction to do the dict manipulation.
     */

    OP44(	DICT_SET, numWords-2, dictVarIndex);
    TclAdjustStackDepth(-1, envPtr);
    return TCL_OK;
}

int
TclCompileDictIncrCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *keyTokenPtr;
    int dictVarIndex, nameChars, incrAmount;
    const char *name;

    /*
     * There must be at least two arguments after the command.
     */

    if (parsePtr->numWords < 3 || parsePtr->numWords > 4) {
	return TCL_ERROR;
    }
    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    keyTokenPtr = TokenAfter(varTokenPtr);

    /*
     * Parse the increment amount, if present.
     */

    if (parsePtr->numWords == 4) {
	const char *word;
	int numBytes, code;
	Tcl_Token *incrTokenPtr;
	Tcl_Obj *intObj;

	incrTokenPtr = TokenAfter(keyTokenPtr);
	if (incrTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    return TCL_ERROR;
	}
	word = incrTokenPtr[1].start;
	numBytes = incrTokenPtr[1].size;

	intObj = Tcl_NewStringObj(word, numBytes);
	Tcl_IncrRefCount(intObj);
	code = TclGetIntFromObj(NULL, intObj, &incrAmount);
	TclDecrRefCount(intObj);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	incrAmount = 1;
    }

    /*
     * The dictionary variable must be a local scalar that is knowable at
     * compile time; anything else exceeds the complexity of the opcode. So
     * discover what the index is.
     */

    if (varTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }
    name = varTokenPtr[1].start;
    nameChars = varTokenPtr[1].size;
    if (!TclIsLocalScalar(name, nameChars)) {
	return TCL_ERROR;
    }
    dictVarIndex = TclFindCompiledLocal(name, nameChars, 1, envPtr);
    if (dictVarIndex < 0) {
	return TCL_ERROR;
    }

    /*
     * Emit the key and the code to actually do the increment.
     */

    PUSH_SUBST_WORD(keyTokenPtr, 3);
    OP44(	DICT_INCR_IMM, incrAmount, dictVarIndex);
    return TCL_OK;
}

int
TclCompileDictGetCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;
    int numWords, i;
    DefineLineInformation;	/* TIP #280 */

    /*
     * There must be at least two arguments after the command (the single-arg
     * case is legal, but too special and magic for us to deal with here).
     */

    if (parsePtr->numWords < 3) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    numWords = parsePtr->numWords-1;

    /*
     * Only compile this because we need INST_DICT_GET anyway.
     */

    for (i=0 ; i<numWords ; i++) {
	PUSH_SUBST_WORD(tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }
    OP4(	DICT_GET, numWords-1);
    TclAdjustStackDepth(-1, envPtr);
    return TCL_OK;
}

int
TclCompileDictExistsCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;
    int numWords, i;
    DefineLineInformation;	/* TIP #280 */

    /*
     * There must be at least two arguments after the command (the single-arg
     * case is legal, but too special and magic for us to deal with here).
     */

    if (parsePtr->numWords < 3) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    numWords = parsePtr->numWords-1;

    /*
     * Now we do the code generation.
     */

    for (i=0 ; i<numWords ; i++) {
	PUSH_SUBST_WORD(tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }
    OP4(	DICT_EXISTS, numWords-1);
    TclAdjustStackDepth(-1, envPtr);
    return TCL_OK;
}

int
TclCompileDictUnsetCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;
    DefineLineInformation;	/* TIP #280 */
    int i, dictVarIndex, nameChars;
    const char *name;

    /*
     * There must be at least one argument after the variable name for us to
     * compile to bytecode.
     */

    if (parsePtr->numWords < 3) {
	return TCL_ERROR;
    }

    /*
     * The dictionary variable must be a local scalar that is knowable at
     * compile time; anything else exceeds the complexity of the opcode. So
     * discover what the index is.
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }
    name = tokenPtr[1].start;
    nameChars = tokenPtr[1].size;
    if (!TclIsLocalScalar(name, nameChars)) {
	return TCL_ERROR;
    }
    dictVarIndex = TclFindCompiledLocal(name, nameChars, 1, envPtr);
    if (dictVarIndex < 0) {
	return TCL_ERROR;
    }

    /*
     * Remaining words (the key path) can be handled normally.
     */

    for (i=2 ; i<parsePtr->numWords ; i++) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_SUBST_WORD(tokenPtr, i);
    }

    /*
     * Now emit the instruction to do the dict manipulation.
     */

    OP44(	DICT_UNSET, parsePtr->numWords-2, dictVarIndex);
    return TCL_OK;
}

int
TclCompileDictCreateCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    int worker;			/* Temp var for building the value in. */
    Tcl_Token *tokenPtr;
    Tcl_Obj *keyObj, *valueObj, *dictObj;
    int i;

    if ((parsePtr->numWords & 1) == 0) {
	return TCL_ERROR;
    }

    /*
     * See if we can build the value at compile time...
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    dictObj = Tcl_NewObj();
    Tcl_IncrRefCount(dictObj);
    for (i=1 ; i<parsePtr->numWords ; i+=2) {
	keyObj = Tcl_NewObj();
	Tcl_IncrRefCount(keyObj);
	if (!TclWordKnownAtCompileTime(tokenPtr, keyObj)) {
	    Tcl_DecrRefCount(keyObj);
	    Tcl_DecrRefCount(dictObj);
	    goto nonConstant;
	}
	tokenPtr = TokenAfter(tokenPtr);
	valueObj = Tcl_NewObj();
	Tcl_IncrRefCount(valueObj);
	if (!TclWordKnownAtCompileTime(tokenPtr, valueObj)) {
	    Tcl_DecrRefCount(keyObj);
	    Tcl_DecrRefCount(valueObj);
	    Tcl_DecrRefCount(dictObj);
	    goto nonConstant;
	}
	tokenPtr = TokenAfter(tokenPtr);
	Tcl_DictObjPut(NULL, dictObj, keyObj, valueObj);
	Tcl_DecrRefCount(keyObj);
	Tcl_DecrRefCount(valueObj);
    }

    /*
     * We did! Excellent. The "verifyDict" is to do type forcing.
     */

    PUSH_OBJ(	dictObj);
    OP(		DUP);
    OP(		DICT_VERIFY);
    Tcl_DecrRefCount(dictObj);
    return TCL_OK;

    /*
     * Otherwise, we've got to issue runtime code to do the building, which we
     * do by [dict set]ting into an unnamed local variable. This requires that
     * we are in a context with an LVT.
     */

  nonConstant:
    worker = NewUnnamedLocal(envPtr);
    if (worker < 0) {
	return TCL_ERROR;
    }

    PUSH(	"");
    OP4(	STORE_SCALAR, worker);
    OP(		POP);
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    for (i=1 ; i<parsePtr->numWords ; i+=2) {
	PUSH_SUBST_WORD(tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_SUBST_WORD(tokenPtr, i+1);
	tokenPtr = TokenAfter(tokenPtr);
	OP44(	DICT_SET, 1, worker);
	TclAdjustStackDepth(-1, envPtr);
	OP(	POP);
    }
    OP4(	LOAD_SCALAR, worker);
    OP14(	UNSET_SCALAR, 0, worker);
    return TCL_OK;
}

int
TclCompileDictMergeCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    int i, workerIndex, infoIndex, outLoop;

    /*
     * Deal with some special edge cases. Note that in the case with one
     * argument, the only thing to do is to verify the dict-ness.
     */

    if (parsePtr->numWords < 2) {
	PUSH(	"");
	return TCL_OK;
    } else if (parsePtr->numWords == 2) {
	tokenPtr = TokenAfter(parsePtr->tokenPtr);
	PUSH_SUBST_WORD(tokenPtr, 1);
	OP(	DUP);
	OP(	DICT_VERIFY);
	return TCL_OK;
    }

    /*
     * There's real merging work to do.
     *
     * Allocate some working space. This means we'll only ever compile this
     * command when there's an LVT present.
     */

    workerIndex = NewUnnamedLocal(envPtr);
    if (workerIndex < 0) {
	return TCL_ERROR;
    }
    infoIndex = NewUnnamedLocal(envPtr);

    /*
     * Get the first dictionary and verify that it is so.
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_SUBST_WORD(tokenPtr, 1);
    OP(		DUP);
    OP(		DICT_VERIFY);
    OP4(	STORE_SCALAR, workerIndex);
    OP(		POP);

    /*
     * For each of the remaining dictionaries...
     */

    outLoop = DeclareExceptionRange(envPtr, CATCH_EXCEPTION_RANGE);
    OP4(	BEGIN_CATCH, outLoop);
    ExceptionRangeStarts(envPtr, outLoop);
    for (i=2 ; i<parsePtr->numWords ; i++) {
	int endloop, loop;

	/*
	 * Get the dictionary, and merge its pairs into the first dict (using
	 * a small loop).
	 */

	tokenPtr = TokenAfter(tokenPtr);
	PUSH_SUBST_WORD(tokenPtr, i);
	OP4(	DICT_FIRST, infoIndex);
	JUMP(endloop, JUMP_TRUE);
	LABEL(loop);
	OP(	EXCH);
	OP44(	DICT_SET, 1, workerIndex);
	TclAdjustStackDepth(-1, envPtr);
	OP(	POP);
	OP4(	DICT_NEXT, infoIndex);
	BACKJUMP(loop, JUMP_FALSE);
	FIXJUMP(endloop);
	OP(	POP);
	OP(	POP);
	OP14(	UNSET_SCALAR, 0, infoIndex);
    }
    ExceptionRangeEnds(envPtr, outLoop);
    OP(		END_CATCH);

    /*
     * Clean up any state left over.
     */

    OP4(	LOAD_SCALAR, workerIndex);
    OP14(	UNSET_SCALAR, 0, workerIndex);
    OP4(	JUMP, 21);

    /*
     * If an exception happens when starting to iterate over the second (and
     * subsequent) dicts. This is strictly not necessary, but it is nice.
     */

    ExceptionRangeTarget(envPtr, outLoop, catchOffset);
    OP(		PUSH_RETURN_OPTIONS);
    OP(		PUSH_RESULT);
    OP(		END_CATCH);
    OP14(	UNSET_SCALAR, 0, workerIndex);
    OP14(	UNSET_SCALAR, 0, infoIndex);
    OP(		RETURN_STK);

    return TCL_OK;
}

int
TclCompileDictForCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    return CompileDictEachCmd(interp, parsePtr, cmdPtr, envPtr,
	    TCL_EACH_KEEP_NONE);
}

int
TclCompileDictMapCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    return CompileDictEachCmd(interp, parsePtr, cmdPtr, envPtr,
	    TCL_EACH_COLLECT);
}

int
CompileDictEachCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr,		/* Holds resulting instructions. */
    int collect)		/* Flag == TCL_EACH_COLLECT to collect and
				 * construct a new dictionary with the loop
				 * body result. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varsTokenPtr, *dictTokenPtr, *bodyTokenPtr;
    int keyVarIndex, valueVarIndex, nameChars, loopRange, catchRange, numVars;
    int infoIndex, bodyTargetOffset, emptyTargetOffset, endTargetOffset;
    int collectVar = -1;	/* Index of temp var holding the result
				 * dict. */
    int savedStackDepth = envPtr->currStackDepth;
				/* Needed because jumps confuse the stack
				 * space calculator. */
    const char **argv;
    Tcl_DString buffer;

    /*
     * There must be at least three argument after the command.
     */

    if (parsePtr->numWords != 4) {
	return TCL_ERROR;
    }

    varsTokenPtr = TokenAfter(parsePtr->tokenPtr);
    dictTokenPtr = TokenAfter(varsTokenPtr);
    bodyTokenPtr = TokenAfter(dictTokenPtr);
    if (varsTokenPtr->type != TCL_TOKEN_SIMPLE_WORD ||
	    bodyTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }

    /*
     * Create temporary variable to capture return values from loop body when
     * we're collecting results.
     */

    if (collect == TCL_EACH_COLLECT) {
	collectVar = NewUnnamedLocal(envPtr);
	if (collectVar < 0) {
	    return TCL_ERROR;
	}
    }

    /*
     * Check we've got a pair of variables and that they are local variables.
     * Then extract their indices in the LVT.
     */

    Tcl_DStringInit(&buffer);
    TclDStringAppendToken(&buffer, &varsTokenPtr[1]);
    if (Tcl_SplitList(NULL, Tcl_DStringValue(&buffer), &numVars,
	    &argv) != TCL_OK) {
	Tcl_DStringFree(&buffer);
	return TCL_ERROR;
    }
    Tcl_DStringFree(&buffer);
    if (numVars != 2) {
	ckfree(argv);
	return TCL_ERROR;
    }

    nameChars = strlen(argv[0]);
    if (!TclIsLocalScalar(argv[0], nameChars)) {
	ckfree(argv);
	return TCL_ERROR;
    }
    keyVarIndex = TclFindCompiledLocal(argv[0], nameChars, 1, envPtr);

    nameChars = strlen(argv[1]);
    if (!TclIsLocalScalar(argv[1], nameChars)) {
	ckfree(argv);
	return TCL_ERROR;
    }
    valueVarIndex = TclFindCompiledLocal(argv[1], nameChars, 1, envPtr);
    ckfree(argv);

    if ((keyVarIndex < 0) || (valueVarIndex < 0)) {
	return TCL_ERROR;
    }

    /*
     * Allocate a temporary variable to store the iterator reference. The
     * variable will contain a Tcl_DictSearch reference which will be
     * allocated by INST_DICT_FIRST and disposed when the variable is unset
     * (at which point it should also have been finished with).
     */

    infoIndex = NewUnnamedLocal(envPtr);
    if (infoIndex < 0) {
	return TCL_ERROR;
    }

    /*
     * Preparation complete; issue instructions. Note that this code issues
     * fixed-sized jumps. That simplifies things a lot!
     *
     * First up, initialize the accumulator dictionary if needed.
     */

    if (collect == TCL_EACH_COLLECT) {
	PUSH(	"");
	OP4(	STORE_SCALAR, collectVar);
	OP(	POP);
    }

    /*
     * Get the dictionary and start the iteration. No catching of errors at
     * this point.
     */

    PUSH_SUBST_WORD(dictTokenPtr, 3);
    OP4(	DICT_FIRST, infoIndex);
    JUMP(emptyTargetOffset, JUMP_TRUE);

    /*
     * Now we catch errors from here on so that we can finalize the search
     * started by Tcl_DictObjFirst above.
     */

    catchRange = DeclareExceptionRange(envPtr, CATCH_EXCEPTION_RANGE);
    OP4(	BEGIN_CATCH, catchRange);
    ExceptionRangeStarts(envPtr, catchRange);

    /*
     * Inside the iteration, write the loop variables.
     */

    LABEL(bodyTargetOffset);
    OP4(	STORE_SCALAR, keyVarIndex);
    OP(		POP);
    OP4(	STORE_SCALAR, valueVarIndex);
    OP(		POP);

    /*
     * Set up the loop exception targets.
     */

    loopRange = DeclareExceptionRange(envPtr, LOOP_EXCEPTION_RANGE);
    ExceptionRangeStarts(envPtr, loopRange);

    /*
     * Compile the loop body itself. It should be stack-neutral.
     */

    BODY(	bodyTokenPtr, 3);
    if (collect == TCL_EACH_COLLECT) {
	OP4(	LOAD_SCALAR, keyVarIndex);
	OP(	UNDER);
	OP44(	DICT_SET, 1, collectVar);
	TclAdjustStackDepth(-1, envPtr);
	OP(	POP);
    }
    OP(		POP);

    /*
     * Both exception target ranges (error and loop) end here.
     */

    ExceptionRangeEnds(envPtr, loopRange);
    ExceptionRangeEnds(envPtr, catchRange);

    /*
     * Continue (or just normally process) by getting the next pair of items
     * from the dictionary and jumping back to the code to write them into
     * variables if there is another pair.
     */

    ExceptionRangeTarget(envPtr, loopRange, continueOffset);
    OP4(	DICT_NEXT, infoIndex);
    BACKJUMP(bodyTargetOffset, JUMP_FALSE);
    OP(		POP);
    OP(		POP);

    /*
     * Now do the final cleanup for the no-error case (this is where we break
     * out of the loop to) by force-terminating the iteration (if not already
     * terminated), ditching the exception info and jumping to the last
     * instruction for this command. In theory, this could be done using the
     * "finally" clause (next generated) but this is faster.
     */

    ExceptionRangeTarget(envPtr, loopRange, breakOffset);
    OP14(	UNSET_SCALAR, 0, infoIndex);
    OP(		END_CATCH);
    JUMP(endTargetOffset, JUMP);

    /*
     * Error handler "finally" clause, which force-terminates the iteration
     * and rethrows the error.
     */

    ExceptionRangeTarget(envPtr, catchRange, catchOffset);
    OP(		PUSH_RETURN_OPTIONS);
    OP(		PUSH_RESULT);
    OP14(	UNSET_SCALAR, 0, infoIndex);
    OP(		END_CATCH);
    if (collect == TCL_EACH_COLLECT) {
	OP14(	UNSET_SCALAR, 0, collectVar);
    }
    OP(		RETURN_STK);

    /*
     * Otherwise we're done (the jump after the DICT_FIRST points here) and we
     * need to pop the bogus key/value pair (pushed to keep stack calculations
     * easy!) Note that we skip the END_CATCH. [Bug 1382528]
     */

    envPtr->currStackDepth = savedStackDepth + 2;
    FIXJUMP(	emptyTargetOffset);
    OP(		POP);
    OP(		POP);
    OP14(	UNSET_SCALAR, 0, infoIndex);

    /*
     * Final stage of the command (normal case) is that we push an empty
     * object (or push the accumulator as the result object). This is done
     * last to promote peephole optimization when it's dropped immediately.
     */

    FIXJUMP(	endTargetOffset);
    if (collect == TCL_EACH_COLLECT) {
	OP4(	LOAD_SCALAR, collectVar);
	OP14(	UNSET_SCALAR, 0, collectVar);
    } else {
	PUSH(	"");
    }
    return TCL_OK;
}

int
TclCompileDictUpdateCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    const char *name;
    int i, nameChars, dictIndex, numVars, range, infoIndex;
    Tcl_Token **keyTokenPtrs, *dictVarTokenPtr, *bodyTokenPtr, *tokenPtr;
    int savedStackDepth = envPtr->currStackDepth;
    DictUpdateInfo *duiPtr;
    JumpFixup jumpFixup;

    /*
     * There must be at least one argument after the command.
     */

    if (parsePtr->numWords < 5) {
	return TCL_ERROR;
    }

    /*
     * Parse the command. Expect the following:
     *   dict update <lit(eral)> <any> <lit> ?<any> <lit> ...? <lit>
     */

    if ((parsePtr->numWords - 1) & 1) {
	return TCL_ERROR;
    }
    numVars = (parsePtr->numWords - 3) / 2;

    /*
     * The dictionary variable must be a local scalar that is knowable at
     * compile time; anything else exceeds the complexity of the opcode. So
     * discover what the index is.
     */

    dictVarTokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (dictVarTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }
    name = dictVarTokenPtr[1].start;
    nameChars = dictVarTokenPtr[1].size;
    if (!TclIsLocalScalar(name, nameChars)) {
	return TCL_ERROR;
    }
    dictIndex = TclFindCompiledLocal(name, nameChars, 1, envPtr);
    if (dictIndex < 0) {
	return TCL_ERROR;
    }

    /*
     * Assemble the instruction metadata. This is complex enough that it is
     * represented as auxData; it holds an ordered list of variable indices
     * that are to be used.
     */

    duiPtr = ckalloc(sizeof(DictUpdateInfo) + sizeof(int) * (numVars - 1));
    duiPtr->length = numVars;
    keyTokenPtrs = TclStackAlloc(interp,
	    sizeof(Tcl_Token *) * numVars);
    tokenPtr = TokenAfter(dictVarTokenPtr);

    for (i=0 ; i<numVars ; i++) {
	/*
	 * Put keys to one side for later compilation to bytecode.
	 */

	keyTokenPtrs[i] = tokenPtr;

	/*
	 * Variables first need to be checked for sanity.
	 */

	tokenPtr = TokenAfter(tokenPtr);
	if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    goto failedUpdateInfoAssembly;
	}
	name = tokenPtr[1].start;
	nameChars = tokenPtr[1].size;
	if (!TclIsLocalScalar(name, nameChars)) {
	    goto failedUpdateInfoAssembly;
	}

	/*
	 * Stash the index in the auxiliary data.
	 */

	duiPtr->varIndices[i] =
		TclFindCompiledLocal(name, nameChars, 1, envPtr);
	if (duiPtr->varIndices[i] < 0) {
	    goto failedUpdateInfoAssembly;
	}
	tokenPtr = TokenAfter(tokenPtr);
    }
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
    failedUpdateInfoAssembly:
	ckfree(duiPtr);
	TclStackFree(interp, keyTokenPtrs);
	return TCL_ERROR;
    }
    bodyTokenPtr = tokenPtr;

    /*
     * The list of variables to bind is stored in auxiliary data so that it
     * can't be snagged by literal sharing and forced to shimmer dangerously.
     */

    infoIndex = TclCreateAuxData(duiPtr, &tclDictUpdateInfoType, envPtr);

    for (i=0 ; i<numVars ; i++) {
	PUSH_SUBST_WORD(keyTokenPtrs[i], i);
    }
    OP4(	LIST, numVars);
    OP44(	DICT_UPDATE_START, dictIndex, infoIndex);

    range = DeclareExceptionRange(envPtr, CATCH_EXCEPTION_RANGE);
    OP4(	BEGIN_CATCH, range);

    ExceptionRangeStarts(envPtr, range);
    envPtr->currStackDepth++;
    BODY(	bodyTokenPtr, parsePtr->numWords - 1);
    envPtr->currStackDepth = savedStackDepth;
    ExceptionRangeEnds(envPtr, range);

    /*
     * Normal termination code: the stack has the key list below the result of
     * the body evaluation: swap them and finish the update code.
     */

    OP(		END_CATCH);
    OP(		EXCH);
    OP44(	DICT_UPDATE_END, dictIndex, infoIndex);

    /*
     * Jump around the exceptional termination code.
     */

    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpFixup);

    /*
     * Termination code for non-ok returns: stash the result and return
     * options in the stack, bring up the key list, finish the update code,
     * and finally return with the catched return data
     */

    ExceptionRangeTarget(envPtr, range, catchOffset);
    OP(		PUSH_RESULT);
    OP(		PUSH_RETURN_OPTIONS);
    OP(		END_CATCH);
    OP4(	REVERSE, 3);

    OP44(	DICT_UPDATE_END, dictIndex, infoIndex);
    OP(		RETURN_STK);

    if (TclFixupForwardJumpToHere(envPtr, &jumpFixup, 127)) {
	Tcl_Panic("TclCompileDictCmd(update): bad jump distance %d",
		(int) (CurrentOffset(envPtr) - jumpFixup.codeOffset));
    }
    TclStackFree(interp, keyTokenPtrs);
    envPtr->currStackDepth = savedStackDepth + 1;
    return TCL_OK;
}

int
TclCompileDictAppendCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    int i, dictVarIndex;

    /*
     * There must be at least two argument after the command. And we impose an
     * (arbirary) safe limit; anyone exceeding it should stop worrying about
     * speed quite so much. ;-)
     */

    if (parsePtr->numWords<4 || parsePtr->numWords>100) {
	return TCL_ERROR;
    }

    /*
     * Get the index of the local variable that we will be working with.
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    } else {
	register const char *name = tokenPtr[1].start;
	register int nameChars = tokenPtr[1].size;

	if (!TclIsLocalScalar(name, nameChars)) {
	    return TCL_ERROR;
	}
	dictVarIndex = TclFindCompiledLocal(name, nameChars, 1, envPtr);
	if (dictVarIndex < 0) {
	    return TCL_ERROR;
	}
    }

    /*
     * Produce the string to concatenate onto the dictionary entry.
     */

    tokenPtr = TokenAfter(tokenPtr);
    for (i=2 ; i<parsePtr->numWords ; i++) {
	PUSH_SUBST_WORD(tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }
    if (parsePtr->numWords > 4) {
	OP1(	CONCAT, parsePtr->numWords-3);
    }

    /*
     * Do the concatenation.
     */

    OP4(	DICT_APPEND, dictVarIndex);
    return TCL_OK;
}

int
TclCompileDictLappendCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *keyTokenPtr, *valueTokenPtr;
    int dictVarIndex, nameChars;
    const char *name;

    /*
     * There must be three arguments after the command.
     */

    if (parsePtr->numWords != 4) {
	return TCL_ERROR;
    }

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    keyTokenPtr = TokenAfter(varTokenPtr);
    valueTokenPtr = TokenAfter(keyTokenPtr);
    if (varTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }
    name = varTokenPtr[1].start;
    nameChars = varTokenPtr[1].size;
    if (!TclIsLocalScalar(name, nameChars)) {
	return TCL_ERROR;
    }
    dictVarIndex = TclFindCompiledLocal(name, nameChars, 1, envPtr);
    if (dictVarIndex < 0) {
	return TCL_ERROR;
    }
    PUSH_SUBST_WORD(keyTokenPtr, 3);
    PUSH_SUBST_WORD(valueTokenPtr, 4);
    OP4(	DICT_LAPPEND, dictVarIndex);
    return TCL_OK;
}

int
TclCompileDictWithCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    int i, range, varNameTmp, pathTmp, keysTmp, gotPath, dictVar = -1;
    int bodyIsEmpty = 1;
    Tcl_Token *varTokenPtr, *tokenPtr;
    int savedStackDepth = envPtr->currStackDepth;
    JumpFixup jumpFixup;
    const char *ptr, *end;

    /*
     * There must be at least one argument after the command.
     */

    if (parsePtr->numWords < 3) {
	return TCL_ERROR;
    }

    /*
     * Parse the command (trivially). Expect the following:
     *   dict with <any (varName)> ?<any> ...? <literal>
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    tokenPtr = TokenAfter(varTokenPtr);
    for (i=3 ; i<parsePtr->numWords ; i++) {
	tokenPtr = TokenAfter(tokenPtr);
    }
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }

    /*
     * Test if the last word is an empty script; if so, we can compile it in
     * all cases, but if it is non-empty we need local variable table entries
     * to hold the temporary variables (used to keep stack usage simple).
     */

    for (ptr=tokenPtr[1].start,end=ptr+tokenPtr[1].size ; ptr!=end ; ptr++) {
	if (*ptr!=' ' && *ptr!='\t' && *ptr!='\n' && *ptr!='\r') {
	    if (envPtr->procPtr == NULL) {
		return TCL_ERROR;
	    }
	    bodyIsEmpty = 0;
	    break;
	}
    }

    /*
     * Determine if we're manipulating a dict in a simple local variable.
     */

    gotPath = (parsePtr->numWords > 3);
    if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD &&
	    TclIsLocalScalar(varTokenPtr[1].start, varTokenPtr[1].size)) {
	dictVar = TclFindCompiledLocal(varTokenPtr[1].start,
		varTokenPtr[1].size, 1, envPtr);
    }

    /*
     * Special case: an empty body means we definitely have no need to issue
     * try-finally style code or to allocate local variable table entries for
     * storing temporaries. Still need to do both INST_DICT_EXPAND and
     * INST_DICT_RECOMBINE_* though, because we can't determine if we're free
     * of traces.
     */

    if (bodyIsEmpty) {
	if (dictVar >= 0) {
	    if (gotPath) {
		/*
		 * Case: Path into dict in LVT with empty body.
		 */

		tokenPtr = TokenAfter(varTokenPtr);
		for (i=2 ; i<parsePtr->numWords-1 ; i++) {
		    PUSH_SUBST_WORD(tokenPtr, i-1);
		    tokenPtr = TokenAfter(tokenPtr);
		}
		OP4(	LIST, parsePtr->numWords-3);
		OP4(	LOAD_SCALAR, dictVar);
		OP(	UNDER);
		OP(	DICT_EXPAND);
		OP4(	DICT_RECOMBINE_IMM, dictVar);
		PUSH(	"");
	    } else {
		/*
		 * Case: Direct dict in LVT with empty body.
		 */

		PUSH(	"");
		OP4(	LOAD_SCALAR, dictVar);
		PUSH(	"");
		OP(	DICT_EXPAND);
		OP4(	DICT_RECOMBINE_IMM, dictVar);
		PUSH(	"");
	    }
	} else {
	    if (gotPath) {
		/*
		 * Case: Path into dict in non-simple var with empty body.
		 */

		tokenPtr = varTokenPtr;
		for (i=1 ; i<parsePtr->numWords-1 ; i++) {
		    PUSH_SUBST_WORD(tokenPtr, i-1);
		    tokenPtr = TokenAfter(tokenPtr);
		}
		OP4(	LIST, parsePtr->numWords-3);
		OP(	UNDER);
		OP(	LOAD_STK);
		OP(	UNDER);
		OP(	DICT_EXPAND);
		OP(	DICT_RECOMBINE_STK);
		PUSH(	"");
	    } else {
		/*
		 * Case: Direct dict in non-simple var with empty body.
		 */

		PUSH_SUBST_WORD(varTokenPtr, 0);
		OP(	DUP);
		OP(	LOAD_STK);
		PUSH(	"");
		OP(	DICT_EXPAND);
		PUSH(	"");
		OP(	EXCH);
		OP(	DICT_RECOMBINE_STK);
		PUSH(	"");
	    }
	}
	envPtr->currStackDepth = savedStackDepth + 1;
	return TCL_OK;
    }

    /*
     * OK, we have a non-trivial body. This means that the focus is on
     * generating a try-finally structure where the INST_DICT_RECOMBINE_* goes
     * in the 'finally' clause.
     *
     * Start by allocating local (unnamed, untraced) working variables.
     */

    if (dictVar == -1) {
	varNameTmp = NewUnnamedLocal(envPtr);
    } else {
	varNameTmp = -1;
    }
    if (gotPath) {
	pathTmp = NewUnnamedLocal(envPtr);
    } else {
	pathTmp = -1;
    }
    keysTmp = NewUnnamedLocal(envPtr);

    /*
     * Issue instructions. First, the part to expand the dictionary.
     */

    if (varNameTmp > -1) {
	PUSH_SUBST_WORD(varTokenPtr, 0);
	OP4(	STORE_SCALAR, varNameTmp);
    }
    tokenPtr = TokenAfter(varTokenPtr);
    if (gotPath) {
	for (i=2 ; i<parsePtr->numWords-1 ; i++) {
	    PUSH_SUBST_WORD(tokenPtr, i-1);
	    tokenPtr = TokenAfter(tokenPtr);
	}
	OP4(	LIST, parsePtr->numWords-3);
	OP4(	STORE_SCALAR, pathTmp);
	OP(	POP);
    }
    if (dictVar == -1) {
	OP(	LOAD_STK);
    } else {
	OP4(	LOAD_SCALAR, dictVar);
    }
    if (gotPath) {
	OP4(	LOAD_SCALAR, pathTmp);
    } else {
	PUSH(	"");
    }
    OP(		DICT_EXPAND);
    OP4(	STORE_SCALAR, keysTmp);
    OP(		POP);

    /*
     * Now the body of the [dict with].
     */

    range = DeclareExceptionRange(envPtr, CATCH_EXCEPTION_RANGE);
    OP4(	BEGIN_CATCH, range);

    ExceptionRangeStarts(envPtr, range);
    envPtr->currStackDepth++;
    BODY(	tokenPtr, parsePtr->numWords-1);
    envPtr->currStackDepth = savedStackDepth;
    ExceptionRangeEnds(envPtr, range);

    /*
     * Now fold the results back into the dictionary in the OK case.
     */

    OP(		END_CATCH);
    if (varNameTmp > -1) {
	OP4(	LOAD_SCALAR, varNameTmp);
    }
    if (gotPath) {
	OP4(	LOAD_SCALAR, pathTmp);
    } else {
	PUSH(	"");
    }
    OP4(	LOAD_SCALAR, keysTmp);
    if (dictVar == -1) {
	OP(	DICT_RECOMBINE_STK);
    } else {
	OP4(	DICT_RECOMBINE_IMM, dictVar);
    }
    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpFixup);

    /*
     * Now fold the results back into the dictionary in the exception case.
     */

    ExceptionRangeTarget(envPtr, range, catchOffset);
    OP(		PUSH_RETURN_OPTIONS);
    OP(		PUSH_RESULT);
    OP(		END_CATCH);
    if (varNameTmp > -1) {
	OP4(	LOAD_SCALAR, varNameTmp);
    }
    if (parsePtr->numWords > 3) {
	OP4(	LOAD_SCALAR, pathTmp);
    } else {
	PUSH(	"");
    }
    OP4(	LOAD_SCALAR, keysTmp);
    if (dictVar == -1) {
	OP(	DICT_RECOMBINE_STK);
    } else {
	OP4(	DICT_RECOMBINE_IMM, dictVar);
    }
    OP(		RETURN_STK);

    /*
     * Prepare for the start of the next command.
     */

    envPtr->currStackDepth = savedStackDepth + 1;
    if (TclFixupForwardJumpToHere(envPtr, &jumpFixup, 127)) {
	Tcl_Panic("TclCompileDictCmd(update): bad jump distance %d",
		(int) (CurrentOffset(envPtr) - jumpFixup.codeOffset));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DupDictUpdateInfo, FreeDictUpdateInfo --
 *
 *	Functions to duplicate, release and print the aux data created for use
 *	with the INST_DICT_UPDATE_START and INST_DICT_UPDATE_END instructions.
 *
 * Results:
 *	DupDictUpdateInfo: a copy of the auxiliary data
 *	FreeDictUpdateInfo: none
 *	PrintDictUpdateInfo: none
 *
 * Side effects:
 *	DupDictUpdateInfo: allocates memory
 *	FreeDictUpdateInfo: releases memory
 *	PrintDictUpdateInfo: none
 *
 *----------------------------------------------------------------------
 */

static ClientData
DupDictUpdateInfo(
    ClientData clientData)
{
    DictUpdateInfo *dui1Ptr, *dui2Ptr;
    unsigned len;

    dui1Ptr = clientData;
    len = sizeof(DictUpdateInfo) + sizeof(int) * (dui1Ptr->length - 1);
    dui2Ptr = ckalloc(len);
    memcpy(dui2Ptr, dui1Ptr, len);
    return dui2Ptr;
}

static void
FreeDictUpdateInfo(
    ClientData clientData)
{
    ckfree(clientData);
}

static void
PrintDictUpdateInfo(
    ClientData clientData,
    Tcl_Obj *appendObj,
    ByteCode *codePtr,
    unsigned int pcOffset)
{
    DictUpdateInfo *duiPtr = clientData;
    int i;

    for (i=0 ; i<duiPtr->length ; i++) {
	if (i) {
	    Tcl_AppendToObj(appendObj, ", ", -1);
	}
	Tcl_AppendPrintfToObj(appendObj, "%%v%u", duiPtr->varIndices[i]);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileErrorCmd --
 *
 *	Procedure called to compile the "error" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "error" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileErrorCmd(
    Tcl_Interp *interp,		/* Used for context. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * General syntax: [error message ?errorInfo? ?errorCode?]
     * However, we only deal with the case where there is just a message.
     */
    Tcl_Token *messageTokenPtr;
    int savedStackDepth = envPtr->currStackDepth;
    DefineLineInformation;	/* TIP #280 */

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    messageTokenPtr = TokenAfter(parsePtr->tokenPtr);

    PUSH(	"-code error -level 0");
    PUSH_SUBST_WORD(messageTokenPtr, 1);
    OP(		RETURN_STK);
    envPtr->currStackDepth = savedStackDepth + 1;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileExprCmd --
 *
 *	Procedure called to compile the "expr" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "expr" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileExprCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *firstWordPtr;

    if (parsePtr->numWords == 1) {
	return TCL_ERROR;
    }

    /*
     * TIP #280: Use the per-word line information of the current command.
     */

    envPtr->line = envPtr->extCmdMapPtr->loc[
	    envPtr->extCmdMapPtr->nuloc-1].line[1];

    firstWordPtr = TokenAfter(parsePtr->tokenPtr);
    TclCompileExprWords(interp, firstWordPtr, parsePtr->numWords-1, envPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileForCmd --
 *
 *	Procedure called to compile the "for" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "for" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileForCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *startTokenPtr, *testTokenPtr, *nextTokenPtr, *bodyTokenPtr;
    JumpFixup jumpEvalCondFixup;
    int testCodeOffset, bodyCodeOffset, nextCodeOffset, jumpDist;
    int bodyRange, nextRange;
    int savedStackDepth = envPtr->currStackDepth;
    DefineLineInformation;	/* TIP #280 */

    if (parsePtr->numWords != 5) {
	return TCL_ERROR;
    }

    /*
     * If the test expression requires substitutions, don't compile the for
     * command inline. E.g., the expression might cause the loop to never
     * execute or execute forever, as in "for {} "$x > 5" {incr x} {}".
     */

    startTokenPtr = TokenAfter(parsePtr->tokenPtr);
    testTokenPtr = TokenAfter(startTokenPtr);
    if (testTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }

    /*
     * Bail out also if the body or the next expression require substitutions
     * in order to insure correct behaviour [Bug 219166]
     */

    nextTokenPtr = TokenAfter(testTokenPtr);
    bodyTokenPtr = TokenAfter(nextTokenPtr);
    if ((nextTokenPtr->type != TCL_TOKEN_SIMPLE_WORD)
	    || (bodyTokenPtr->type != TCL_TOKEN_SIMPLE_WORD)) {
	return TCL_ERROR;
    }

    /*
     * Create ExceptionRange records for the body and the "next" command. The
     * "next" command's ExceptionRange supports break but not continue (and
     * has a -1 continueOffset).
     */

    bodyRange = DeclareExceptionRange(envPtr, LOOP_EXCEPTION_RANGE);
    nextRange = TclCreateExceptRange(LOOP_EXCEPTION_RANGE, envPtr);

    /*
     * Inline compile the initial command.
     */

    BODY(	startTokenPtr, 1);
    OP(		POP);

    /*
     * Jump to the evaluation of the condition. This code uses the "loop
     * rotation" optimisation (which eliminates one branch from the loop).
     * "for start cond next body" produces then:
     *       start
     *       goto A
     *    B: body                : bodyCodeOffset
     *       next                : nextCodeOffset, continueOffset
     *    A: cond -> result      : testCodeOffset
     *       if (result) goto B
     */

    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpEvalCondFixup);

    /*
     * Compile the loop body.
     */

    bodyCodeOffset = ExceptionRangeStarts(envPtr, bodyRange);
    BODY(	bodyTokenPtr, 4);
    ExceptionRangeEnds(envPtr, bodyRange);
    envPtr->currStackDepth = savedStackDepth + 1;
    OP(		POP);

    /*
     * Compile the "next" subcommand.
     */

    envPtr->currStackDepth = savedStackDepth;
    nextCodeOffset = ExceptionRangeStarts(envPtr, nextRange);
    BODY(	nextTokenPtr, 3);
    ExceptionRangeEnds(envPtr, nextRange);
    envPtr->currStackDepth = savedStackDepth + 1;
    OP(		POP);
    envPtr->currStackDepth = savedStackDepth;

    /*
     * Compile the test expression then emit the conditional jump that
     * terminates the for.
     */

    LABEL(	testCodeOffset);

    jumpDist = testCodeOffset - jumpEvalCondFixup.codeOffset;
    if (TclFixupForwardJump(envPtr, &jumpEvalCondFixup, jumpDist, 127)) {
	bodyCodeOffset += 3;
	nextCodeOffset += 3;
	testCodeOffset += 3;
    }

    envPtr->currStackDepth = savedStackDepth;
    PUSH_EXPR_WORD(testTokenPtr, 2);
    envPtr->currStackDepth = savedStackDepth + 1;

    BACKJUMP(	bodyCodeOffset, JUMP_TRUE);

    /*
     * Fix the starting points of the exception ranges (may have moved due to
     * jump type modification) and set where the exceptions target.
     */

    envPtr->exceptArrayPtr[bodyRange].codeOffset = bodyCodeOffset;
    envPtr->exceptArrayPtr[bodyRange].continueOffset = nextCodeOffset;

    envPtr->exceptArrayPtr[nextRange].codeOffset = nextCodeOffset;

    ExceptionRangeTarget(envPtr, bodyRange, breakOffset);
    ExceptionRangeTarget(envPtr, nextRange, breakOffset);

    /*
     * The for command's result is an empty string.
     */

    envPtr->currStackDepth = savedStackDepth;
    PUSH(	"");

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileForeachCmd --
 *
 *	Procedure called to compile the "foreach" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "foreach" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileForeachCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    return CompileEachloopCmd(interp, parsePtr, cmdPtr, envPtr,
	    TCL_EACH_KEEP_NONE);
}

/*
 *----------------------------------------------------------------------
 *
 * CompileEachloopCmd --
 *
 *	Procedure called to compile the "foreach" and "lmap" commands.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "foreach" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

static int
CompileEachloopCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr,		/* Holds resulting instructions. */
    int collect)		/* Select collecting or accumulating mode
				 * (TCL_EACH_*) */
{
    Proc *procPtr = envPtr->procPtr;
    ForeachInfo *infoPtr;	/* Points to the structure describing this
				 * foreach command. Stored in a AuxData
				 * record in the ByteCode. */
    int firstValueTemp;		/* Index of the first temp var in the frame
				 * used to point to a value list. */
    int loopCtTemp;		/* Index of temp var holding the loop's
				 * iteration count. */
    int collectVar = -1;	/* Index of temp var holding the result var
				 * index. */

    Tcl_Token *tokenPtr, *bodyTokenPtr;
    JumpFixup jumpFalseFixup;
    int infoIndex, range, bodyIndex;
    int numWords, numLists, numVars, loopIndex, tempVar, i, j, code;
    int savedStackDepth = envPtr->currStackDepth;
    DefineLineInformation;	/* TIP #280 */

    /*
     * We parse the variable list argument words and create two arrays:
     *    varcList[i] is number of variables in i-th var list.
     *    varvList[i] points to array of var names in i-th var list.
     */

    int *varcList;
    const char ***varvList;

    /*
     * If the foreach command isn't in a procedure, don't compile it inline:
     * the payoff is too small.
     */

    if (procPtr == NULL) {
	return TCL_ERROR;
    }

    numWords = parsePtr->numWords;
    if ((numWords < 4) || (numWords%2 != 0)) {
	return TCL_ERROR;
    }

    /*
     * Bail out if the body requires substitutions in order to insure correct
     * behaviour. [Bug 219166]
     */

    for (i = 0, tokenPtr = parsePtr->tokenPtr; i < numWords-1; i++) {
	tokenPtr = TokenAfter(tokenPtr);
    }
    bodyTokenPtr = tokenPtr;
    if (bodyTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }

    bodyIndex = i-1;

    /*
     * Allocate storage for the varcList and varvList arrays if necessary.
     */

    numLists = (numWords - 2)/2;
    varcList = TclStackAlloc(interp, numLists * sizeof(int));
    memset(varcList, 0, numLists * sizeof(int));
    varvList = (const char ***) TclStackAlloc(interp,
	    numLists * sizeof(const char **));
    memset((char*) varvList, 0, numLists * sizeof(const char **));

    /*
     * Break up each var list and set the varcList and varvList arrays. Don't
     * compile the foreach inline if any var name needs substitutions or isn't
     * a scalar, or if any var list needs substitutions.
     */

    loopIndex = 0;
    for (i = 0, tokenPtr = parsePtr->tokenPtr;
	    i < numWords-1;
	    i++, tokenPtr = TokenAfter(tokenPtr)) {
	Tcl_DString varList;

	if (i%2 != 1) {
	    continue;
	}
	if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    code = TCL_ERROR;
	    goto done;
	}

	/*
	 * Lots of copying going on here. Need a ListObj wizard to show a
	 * better way.
	 */

	Tcl_DStringInit(&varList);
	TclDStringAppendToken(&varList, &tokenPtr[1]);
	code = Tcl_SplitList(interp, Tcl_DStringValue(&varList),
		&varcList[loopIndex], &varvList[loopIndex]);
	Tcl_DStringFree(&varList);
	if (code != TCL_OK) {
	    code = TCL_ERROR;
	    goto done;
	}
	numVars = varcList[loopIndex];

	/*
	 * If the variable list is empty, we can enter an infinite loop when
	 * the interpreted version would not. Take care to ensure this does
	 * not happen. [Bug 1671138]
	 */

	if (numVars == 0) {
	    code = TCL_ERROR;
	    goto done;
	}

	for (j = 0;  j < numVars;  j++) {
	    const char *varName = varvList[loopIndex][j];

	    if (!TclIsLocalScalar(varName, (int) strlen(varName))) {
		code = TCL_ERROR;
		goto done;
	    }
	}
	loopIndex++;
    }

    if (collect == TCL_EACH_COLLECT) {
	collectVar = NewUnnamedLocal(envPtr);
	if (collectVar < 0) {
	    return TCL_ERROR;
	}
    }
	    
    /*
     * We will compile the foreach command. Reserve (numLists + 1) temporary
     * variables:
     *    - numLists temps to hold each value list
     *    - 1 temp for the loop counter (index of next element in each list)
     *
     * At this time we don't try to reuse temporaries; if there are two
     * nonoverlapping foreach loops, they don't share any temps.
     */

    code = TCL_OK;
    firstValueTemp = -1;
    for (loopIndex = 0;  loopIndex < numLists;  loopIndex++) {
	tempVar = NewUnnamedLocal(envPtr);
	if (loopIndex == 0) {
	    firstValueTemp = tempVar;
	}
    }
    loopCtTemp = NewUnnamedLocal(envPtr);

    /*
     * Create and initialize the ForeachInfo and ForeachVarList data
     * structures describing this command. Then create a AuxData record
     * pointing to the ForeachInfo structure.
     */

    infoPtr = ckalloc(sizeof(ForeachInfo)
	    + numLists * sizeof(ForeachVarList *));
    infoPtr->numLists = numLists;
    infoPtr->firstValueTemp = firstValueTemp;
    infoPtr->loopCtTemp = loopCtTemp;
    for (loopIndex = 0;  loopIndex < numLists;  loopIndex++) {
	ForeachVarList *varListPtr;

	numVars = varcList[loopIndex];
	varListPtr = ckalloc(sizeof(ForeachVarList)
		+ numVars * sizeof(int));
	varListPtr->numVars = numVars;
	for (j = 0;  j < numVars;  j++) {
	    const char *varName = varvList[loopIndex][j];
	    int nameChars = strlen(varName);

	    varListPtr->varIndexes[j] = TclFindCompiledLocal(varName,
		    nameChars, /*create*/ 1, envPtr);
	}
	infoPtr->varLists[loopIndex] = varListPtr;
    }
    infoIndex = TclCreateAuxData(infoPtr, &tclForeachInfoType, envPtr);

    /*
     * Create an exception record to handle [break] and [continue].
     */

    range = DeclareExceptionRange(envPtr, LOOP_EXCEPTION_RANGE);

    /*
     * Evaluate then store each value list in the associated temporary.
     */

    loopIndex = 0;
    for (i = 0, tokenPtr = parsePtr->tokenPtr;
	    i < numWords-1;
	    i++, tokenPtr = TokenAfter(tokenPtr)) {
	if ((i%2 == 0) && (i > 0)) {
	    PUSH_SUBST_WORD(tokenPtr, i);
	    tempVar = (firstValueTemp + loopIndex);
	    OP4(STORE_SCALAR, tempVar);
	    OP(	POP);
	    loopIndex++;
	}
    }

    /*
     * Create temporary variable to capture return values from loop body.
     */
     
    if (collect == TCL_EACH_COLLECT) {
	PUSH(	"");
	OP4(	STORE_SCALAR, collectVar);
	OP(	POP);
    }

    /*
     * Initialize the temporary var that holds the count of loop iterations.
     */

    OP4(	FOREACH_START, infoIndex);

    /*
     * Top of loop code: assign each loop variable and check whether
     * to terminate the loop.
     */

    ExceptionRangeTarget(envPtr, range, continueOffset);
    OP4(	FOREACH_STEP, infoIndex);
    TclEmitForwardJump(envPtr, TCL_FALSE_JUMP, &jumpFalseFixup);

    /*
     * Inline compile the loop body.
     */

    ExceptionRangeStarts(envPtr, range);
    BODY(	bodyTokenPtr, bodyIndex);
    ExceptionRangeEnds(envPtr, range);
    envPtr->currStackDepth = savedStackDepth + 1;

    if (collect == TCL_EACH_COLLECT) {
	OP4(	LAPPEND_SCALAR, collectVar);
    }
    OP(		POP);

    /*
     * Jump back to the test at the top of the loop. Generate a 4 byte jump if
     * the distance to the test is > 120 bytes. This is conservative and
     * ensures that we won't have to replace this jump if we later need to
     * replace the ifFalse jump with a 4 byte jump.
     */

    BACKJUMP(envPtr->exceptArrayPtr[range].continueOffset, JUMP);

    /*
     * Fix the target of the jump after the foreach_step test.
     */

    if (TclFixupForwardJumpToHere(envPtr, &jumpFalseFixup, 127)) {
	/*
	 * Update the loop body's starting PC offset since it moved down.
	 */

	envPtr->exceptArrayPtr[range].codeOffset += 3;
    }

    /*
     * Set the loop's break target.
     */

    ExceptionRangeTarget(envPtr, range, breakOffset);

    /*
     * The command's result is an empty string if not collecting, or the
     * list of results from evaluating the loop body.
     */

    envPtr->currStackDepth = savedStackDepth;
    if (collect == TCL_EACH_COLLECT) {
	OP4(	LOAD_SCALAR, collectVar);
	OP14(	UNSET_SCALAR, 0, collectVar);
    } else {
	PUSH(	"");
    }
    envPtr->currStackDepth = savedStackDepth + 1;

  done:
    for (loopIndex = 0;  loopIndex < numLists;  loopIndex++) {
	if (varvList[loopIndex] != NULL) {
	    ckfree(varvList[loopIndex]);
	}
    }
    TclStackFree(interp, (void *)varvList);
    TclStackFree(interp, varcList);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * DupForeachInfo --
 *
 *	This procedure duplicates a ForeachInfo structure created as auxiliary
 *	data during the compilation of a foreach command.
 *
 * Results:
 *	A pointer to a newly allocated copy of the existing ForeachInfo
 *	structure is returned.
 *
 * Side effects:
 *	Storage for the copied ForeachInfo record is allocated. If the
 *	original ForeachInfo structure pointed to any ForeachVarList records,
 *	these structures are also copied and pointers to them are stored in
 *	the new ForeachInfo record.
 *
 *----------------------------------------------------------------------
 */

static ClientData
DupForeachInfo(
    ClientData clientData)	/* The foreach command's compilation auxiliary
				 * data to duplicate. */
{
    register ForeachInfo *srcPtr = clientData;
    ForeachInfo *dupPtr;
    register ForeachVarList *srcListPtr, *dupListPtr;
    int numVars, i, j, numLists = srcPtr->numLists;

    dupPtr = ckalloc(sizeof(ForeachInfo)
	    + numLists * sizeof(ForeachVarList *));
    dupPtr->numLists = numLists;
    dupPtr->firstValueTemp = srcPtr->firstValueTemp;
    dupPtr->loopCtTemp = srcPtr->loopCtTemp;

    for (i = 0;  i < numLists;  i++) {
	srcListPtr = srcPtr->varLists[i];
	numVars = srcListPtr->numVars;
	dupListPtr = ckalloc(sizeof(ForeachVarList)
		+ numVars * sizeof(int));
	dupListPtr->numVars = numVars;
	for (j = 0;  j < numVars;  j++) {
	    dupListPtr->varIndexes[j] =	srcListPtr->varIndexes[j];
	}
	dupPtr->varLists[i] = dupListPtr;
    }
    return dupPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeForeachInfo --
 *
 *	Procedure to free a ForeachInfo structure created as auxiliary data
 *	during the compilation of a foreach command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage for the ForeachInfo structure pointed to by the ClientData
 *	argument is freed as is any ForeachVarList record pointed to by the
 *	ForeachInfo structure.
 *
 *----------------------------------------------------------------------
 */

static void
FreeForeachInfo(
    ClientData clientData)	/* The foreach command's compilation auxiliary
				 * data to free. */
{
    register ForeachInfo *infoPtr = clientData;
    register ForeachVarList *listPtr;
    int numLists = infoPtr->numLists;
    register int i;

    for (i = 0;  i < numLists;  i++) {
	listPtr = infoPtr->varLists[i];
	ckfree(listPtr);
    }
    ckfree(infoPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PrintForeachInfo --
 *
 *	Function to write a human-readable representation of a ForeachInfo
 *	structure to stdout for debugging.
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
PrintForeachInfo(
    ClientData clientData,
    Tcl_Obj *appendObj,
    ByteCode *codePtr,
    unsigned int pcOffset)
{
    register ForeachInfo *infoPtr = clientData;
    register ForeachVarList *varsPtr;
    int i, j;

    Tcl_AppendToObj(appendObj, "data=[", -1);

    for (i=0 ; i<infoPtr->numLists ; i++) {
	if (i) {
	    Tcl_AppendToObj(appendObj, ", ", -1);
	}
	Tcl_AppendPrintfToObj(appendObj, "%%v%u",
		(unsigned) (infoPtr->firstValueTemp + i));
    }
    Tcl_AppendPrintfToObj(appendObj, "], loop=%%v%u",
	    (unsigned) infoPtr->loopCtTemp);
    for (i=0 ; i<infoPtr->numLists ; i++) {
	if (i) {
	    Tcl_AppendToObj(appendObj, ",", -1);
	}
	Tcl_AppendPrintfToObj(appendObj, "\n\t\t it%%v%u\t[",
		(unsigned) (infoPtr->firstValueTemp + i));
	varsPtr = infoPtr->varLists[i];
	for (j=0 ; j<varsPtr->numVars ; j++) {
	    if (j) {
		Tcl_AppendToObj(appendObj, ", ", -1);
	    }
	    Tcl_AppendPrintfToObj(appendObj, "%%v%u",
		    (unsigned) varsPtr->varIndexes[j]);
	}
	Tcl_AppendToObj(appendObj, "]", -1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileFormatCmd --
 *
 *	Procedure called to compile the "format" command. Handles cases that
 *	can be done as constants or simple string concatenation only.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "format" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileFormatCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    Tcl_Obj **objv, *formatObj, *tmpObj;
    char *bytes, *start;
    int i, j, len;

    /*
     * Don't handle any guaranteed-error cases.
     */

    if (parsePtr->numWords < 2) {
	return TCL_ERROR;
    }

    /*
     * Check if the argument words are all compile-time-known literals; that's
     * a case we can handle by compiling to a constant.
     */

    formatObj = Tcl_NewObj();
    Tcl_IncrRefCount(formatObj);
    tokenPtr = TokenAfter(tokenPtr);
    if (!TclWordKnownAtCompileTime(tokenPtr, formatObj)) {
	Tcl_DecrRefCount(formatObj);
	return TCL_ERROR;
    }

    objv = ckalloc((parsePtr->numWords-2) * sizeof(Tcl_Obj *));
    for (i=0 ; i+2 < parsePtr->numWords ; i++) {
	tokenPtr = TokenAfter(tokenPtr);
	objv[i] = Tcl_NewObj();
	Tcl_IncrRefCount(objv[i]);
	if (!TclWordKnownAtCompileTime(tokenPtr, objv[i])) {
	    goto checkForStringConcatCase;
	}
    }

    /*
     * Everything is a literal, so the result is constant too (or an error if
     * the format is broken). Do the format now.
     */

    tmpObj = Tcl_Format(interp, Tcl_GetString(formatObj),
	    parsePtr->numWords-2, objv);
    for (; --i>=0 ;) {
	Tcl_DecrRefCount(objv[i]);
    }
    ckfree(objv);
    Tcl_DecrRefCount(formatObj);
    if (tmpObj == NULL) {
	return TCL_ERROR;
    }

    /*
     * Not an error, always a constant result, so just push the result as a
     * literal. Job done.
     */

    PUSH_OBJ(tmpObj);
    Tcl_DecrRefCount(tmpObj);
    return TCL_OK;

  checkForStringConcatCase:
    /*
     * See if we can generate a sequence of things to concatenate. This
     * requires that all the % sequences be %s or %%, as everything else is
     * sufficiently complex that we don't bother.
     *
     * First, get the state of the system relatively sensible (cleaning up
     * after our attempt to spot a literal).
     */

    for (; --i>=0 ;) {
	Tcl_DecrRefCount(objv[i]);
    }
    ckfree(objv);
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    tokenPtr = TokenAfter(tokenPtr);
    i = 0;

    /*
     * Now scan through and check for non-%s and non-%% substitutions.
     */

    for (bytes = Tcl_GetString(formatObj) ; *bytes ; bytes++) {
	if (*bytes == '%') {
	    bytes++;
	    if (*bytes == 's') {
		i++;
		continue;
	    } else if (*bytes == '%') {
		continue;
	    }
	    Tcl_DecrRefCount(formatObj);
	    return TCL_ERROR;
	}
    }

    /*
     * Check if the number of things to concatenate will fit in a byte.
     */

    if (i+2 != parsePtr->numWords || i > 125) {
	Tcl_DecrRefCount(formatObj);
	return TCL_ERROR;
    }

    /*
     * Generate the pushes of the things to concatenate, a sequence of
     * literals and compiled tokens (of which at least one is non-literal or
     * we'd have the case in the first half of this function) which we will
     * concatenate.
     */

    i = 0;			/* The count of things to concat. */
    j = 2;			/* The index into the argument tokens, for
				 * TIP#280 handling. */
    start = Tcl_GetString(formatObj);
				/* The start of the currently-scanned literal
				 * in the format string. */
    tmpObj = Tcl_NewObj();	/* The buffer used to accumulate the literal
				 * being built. */
    for (bytes = start ; *bytes ; bytes++) {
	if (*bytes == '%') {
	    Tcl_AppendToObj(tmpObj, start, bytes - start);
	    if (*++bytes == '%') {
		Tcl_AppendToObj(tmpObj, "%", 1);
	    } else {
		(void) Tcl_GetStringFromObj(tmpObj, &len);

		/*
		 * If there is a non-empty literal from the format string,
		 * push it and reset.
		 */

		if (len > 0) {
		    PUSH_OBJ(tmpObj);
		    Tcl_DecrRefCount(tmpObj);
		    tmpObj = Tcl_NewObj();
		    i++;
		}

		/*
		 * Push the code to produce the string that would be
		 * substituted with %s, except we'll be concatenating
		 * directly.
		 */

		PUSH_SUBST_WORD(tokenPtr, j);
		tokenPtr = TokenAfter(tokenPtr);
		j++;
		i++;
	    }
	    start = bytes + 1;
	}
    }

    /*
     * Handle the case of a trailing literal.
     */

    Tcl_AppendToObj(tmpObj, start, bytes - start);
    (void) Tcl_GetStringFromObj(tmpObj, &len);
    if (len > 0) {
	PUSH_OBJ(tmpObj);
	i++;
    }
    Tcl_DecrRefCount(tmpObj);
    Tcl_DecrRefCount(formatObj);

    if (i > 1) {
	/*
	 * Do the concatenation, which produces the result.
	 */

	OP1(	CONCAT, i);
    } else {
	/*
	 * EVIL HACK! Force there to be a string representation in the case
	 * where there's just a "%s" in the format; case covered by the test
	 * format-20.1 (and it is horrible...)
	 */

	OP(	DUP);
	PUSH(	"");
	OP(	STR_EQ);
	OP(	POP);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileGlobalCmd --
 *
 *	Procedure called to compile the "global" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "global" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileGlobalCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *varTokenPtr;
    int localIndex, numWords, i;
    DefineLineInformation;	/* TIP #280 */

    numWords = parsePtr->numWords;
    if (numWords < 2) {
	return TCL_ERROR;
    }

    /*
     * 'global' has no effect outside of proc bodies; handle that at runtime
     */

    if (envPtr->procPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Push the namespace.
     */

    PUSH(	"::");

    /*
     * Loop over the variables.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    for (i=2; i<=numWords; varTokenPtr = TokenAfter(varTokenPtr),i++) {
	localIndex = IndexTailVarIfKnown(interp, varTokenPtr, envPtr);

	if (localIndex < 0) {
	    return TCL_ERROR;
	}

	PUSH_SUBST_WORD(varTokenPtr, 1);
	OP4(	NSUPVAR, localIndex);
    }

    /*
     * Pop the namespace, and set the result to empty
     */

    OP(		POP);
    PUSH(	"");
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileIfCmd --
 *
 *	Procedure called to compile the "if" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "if" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileIfCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    JumpFixupArray jumpFalseFixupArray;
				/* Used to fix the ifFalse jump after each
				 * test when its target PC is determined. */
    JumpFixupArray jumpEndFixupArray;
				/* Used to fix the jump after each "then" body
				 * to the end of the "if" when that PC is
				 * determined. */
    Tcl_Token *tokenPtr, *testTokenPtr;
    int jumpIndex = 0;		/* Avoid compiler warning. */
    int jumpFalseDist, numWords, wordIdx, numBytes, j, code;
    const char *word;
    int savedStackDepth = envPtr->currStackDepth;
				/* Saved stack depth at the start of the first
				 * test; the envPtr current depth is restored
				 * to this value at the start of each test. */
    int realCond = 1;		/* Set to 0 for static conditions:
				 * "if 0 {..}" */
    int boolVal;		/* Value of static condition. */
    int compileScripts = 1;
    DefineLineInformation;	/* TIP #280 */

    /*
     * Only compile the "if" command if all arguments are simple words, in
     * order to insure correct substitution [Bug 219166]
     */

    tokenPtr = parsePtr->tokenPtr;
    wordIdx = 0;
    numWords = parsePtr->numWords;

    for (wordIdx = 0; wordIdx < numWords; wordIdx++) {
	if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    return TCL_ERROR;
	}
	tokenPtr = TokenAfter(tokenPtr);
    }

    TclInitJumpFixupArray(&jumpFalseFixupArray);
    TclInitJumpFixupArray(&jumpEndFixupArray);
    code = TCL_OK;

    /*
     * Each iteration of this loop compiles one "if expr ?then? body" or
     * "elseif expr ?then? body" clause.
     */

    tokenPtr = parsePtr->tokenPtr;
    wordIdx = 0;
    while (wordIdx < numWords) {
	/*
	 * Stop looping if the token isn't "if" or "elseif".
	 */

	word = tokenPtr[1].start;
	numBytes = tokenPtr[1].size;
	if ((tokenPtr == parsePtr->tokenPtr)
		|| ((numBytes == 6) && (strncmp(word, "elseif", 6) == 0))) {
	    tokenPtr = TokenAfter(tokenPtr);
	    wordIdx++;
	} else {
	    break;
	}
	if (wordIdx >= numWords) {
	    code = TCL_ERROR;
	    goto done;
	}

	/*
	 * Compile the test expression then emit the conditional jump around
	 * the "then" part.
	 */

	envPtr->currStackDepth = savedStackDepth;
	testTokenPtr = tokenPtr;

	if (realCond) {
	    /*
	     * Find out if the condition is a constant.
	     */

	    Tcl_Obj *boolObj = Tcl_NewStringObj(testTokenPtr[1].start,
		    testTokenPtr[1].size);

	    Tcl_IncrRefCount(boolObj);
	    code = Tcl_GetBooleanFromObj(NULL, boolObj, &boolVal);
	    TclDecrRefCount(boolObj);
	    if (code == TCL_OK) {
		/*
		 * A static condition.
		 */

		realCond = 0;
		if (!boolVal) {
		    compileScripts = 0;
		}
	    } else {
		Tcl_ResetResult(interp);
		PUSH_EXPR_WORD(testTokenPtr, wordIdx);
		if (jumpFalseFixupArray.next >= jumpFalseFixupArray.end) {
		    TclExpandJumpFixupArray(&jumpFalseFixupArray);
		}
		jumpIndex = jumpFalseFixupArray.next;
		jumpFalseFixupArray.next++;
		TclEmitForwardJump(envPtr, TCL_FALSE_JUMP,
			jumpFalseFixupArray.fixup+jumpIndex);
	    }
	    code = TCL_OK;
	}

	/*
	 * Skip over the optional "then" before the then clause.
	 */

	tokenPtr = TokenAfter(testTokenPtr);
	wordIdx++;
	if (wordIdx >= numWords) {
	    code = TCL_ERROR;
	    goto done;
	}
	if (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    word = tokenPtr[1].start;
	    numBytes = tokenPtr[1].size;
	    if ((numBytes == 4) && (strncmp(word, "then", 4) == 0)) {
		tokenPtr = TokenAfter(tokenPtr);
		wordIdx++;
		if (wordIdx >= numWords) {
		    code = TCL_ERROR;
		    goto done;
		}
	    }
	}

	/*
	 * Compile the "then" command body.
	 */

	if (compileScripts) {
	    envPtr->currStackDepth = savedStackDepth;
	    BODY(	tokenPtr, wordIdx);
	}

	if (realCond) {
	    /*
	     * Jump to the end of the "if" command. Both jumpFalseFixupArray
	     * and jumpEndFixupArray are indexed by "jumpIndex".
	     */

	    if (jumpEndFixupArray.next >= jumpEndFixupArray.end) {
		TclExpandJumpFixupArray(&jumpEndFixupArray);
	    }
	    jumpEndFixupArray.next++;
	    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP,
		    jumpEndFixupArray.fixup+jumpIndex);

	    /*
	     * Fix the target of the jumpFalse after the test. Generate a 4
	     * byte jump if the distance is > 120 bytes. This is conservative,
	     * and ensures that we won't have to replace this jump if we later
	     * also need to replace the proceeding jump to the end of the "if"
	     * with a 4 byte jump.
	     */

	    if (TclFixupForwardJumpToHere(envPtr,
		    jumpFalseFixupArray.fixup+jumpIndex, 120)) {
		/*
		 * Adjust the code offset for the proceeding jump to the end
		 * of the "if" command.
		 */

		jumpEndFixupArray.fixup[jumpIndex].codeOffset += 3;
	    }
	} else if (boolVal) {
	    /*
	     * We were processing an "if 1 {...}"; stop compiling scripts.
	     */

	    compileScripts = 0;
	} else {
	    /*
	     * We were processing an "if 0 {...}"; reset so that the rest
	     * (elseif, else) is compiled correctly.
	     */

	    realCond = 1;
	    compileScripts = 1;
	}

	tokenPtr = TokenAfter(tokenPtr);
	wordIdx++;
    }

    /*
     * Restore the current stack depth in the environment; the "else" clause
     * (or its default) will add 1 to this.
     */

    envPtr->currStackDepth = savedStackDepth;

    /*
     * Check for the optional else clause. Do not compile anything if this was
     * an "if 1 {...}" case.
     */

    if ((wordIdx < numWords) && (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD)) {
	/*
	 * There is an else clause. Skip over the optional "else" word.
	 */

	word = tokenPtr[1].start;
	numBytes = tokenPtr[1].size;
	if ((numBytes == 4) && (strncmp(word, "else", 4) == 0)) {
	    tokenPtr = TokenAfter(tokenPtr);
	    wordIdx++;
	    if (wordIdx >= numWords) {
		code = TCL_ERROR;
		goto done;
	    }
	}

	if (compileScripts) {
	    /*
	     * Compile the else command body.
	     */

	    BODY(tokenPtr, wordIdx);
	}

	/*
	 * Make sure there are no words after the else clause.
	 */

	wordIdx++;
	if (wordIdx < numWords) {
	    code = TCL_ERROR;
	    goto done;
	}
    } else {
	/*
	 * No else clause: the "if" command's result is an empty string.
	 */

	if (compileScripts) {
	    PUSH(	"");
	}
    }

    /*
     * Fix the unconditional jumps to the end of the "if" command.
     */

    for (j = jumpEndFixupArray.next;  j > 0;  j--) {
	jumpIndex = (j - 1);	/* i.e. process the closest jump first. */
	if (TclFixupForwardJumpToHere(envPtr,
		jumpEndFixupArray.fixup+jumpIndex, 127)) {
	    /*
	     * Adjust the immediately preceeding "ifFalse" jump. We moved it's
	     * target (just after this jump) down three bytes.
	     */

	    unsigned char *ifFalsePc = envPtr->codeStart
		    + jumpFalseFixupArray.fixup[jumpIndex].codeOffset;
	    unsigned char opCode = *ifFalsePc;

	    if (opCode == INST_JUMP_FALSE) {
		jumpFalseDist = TclGetInt4AtPtr(ifFalsePc + 1);
		jumpFalseDist += 3;
		TclStoreInt4AtPtr(jumpFalseDist, (ifFalsePc + 1));
	    } else {
		Tcl_Panic("TclCompileIfCmd: unexpected opcode \"%d\" updating ifFalse jump", (int) opCode);
	    }
	}
    }

    /*
     * Free the jumpFixupArray array if malloc'ed storage was used.
     */

  done:
    envPtr->currStackDepth = savedStackDepth + 1;
    TclFreeJumpFixupArray(&jumpFalseFixupArray);
    TclFreeJumpFixupArray(&jumpEndFixupArray);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileIncrCmd --
 *
 *	Procedure called to compile the "incr" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "incr" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileIncrCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *varTokenPtr, *incrTokenPtr;
    int simpleVarName, isScalar, localIndex, haveImmValue, immValue;
    DefineLineInformation;	/* TIP #280 */

    if ((parsePtr->numWords != 2) && (parsePtr->numWords != 3)) {
	return TCL_ERROR;
    }

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_VAR(	varTokenPtr, 1,
	    &localIndex, &simpleVarName, &isScalar);

    /*
     * If an increment is given, push it, but see first if it's a small
     * integer.
     */

    haveImmValue = 0;
    immValue = 1;
    if (parsePtr->numWords == 3) {
	incrTokenPtr = TokenAfter(varTokenPtr);
	if (incrTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    const char *word = incrTokenPtr[1].start;
	    int numBytes = incrTokenPtr[1].size;
	    int code;
	    Tcl_Obj *intObj = Tcl_NewStringObj(word, numBytes);

	    Tcl_IncrRefCount(intObj);
	    code = TclGetIntFromObj(NULL, intObj, &immValue);
	    TclDecrRefCount(intObj);
	    if ((code == TCL_OK) && (-127 <= immValue) && (immValue <= 127)) {
		haveImmValue = 1;
	    }
	    if (!haveImmValue) {
		PushLiteral(envPtr, word, numBytes);
	    }
	} else {
	    PUSH_SUBST_WORD(incrTokenPtr, 2);
	}
    } else {			/* No incr amount given so use 1. */
	haveImmValue = 1;
    }

    /*
     * Emit the instruction to increment the variable.
     */

    if (!simpleVarName) {
	if (haveImmValue) {
	    OP1(	INCR_STK_IMM, immValue);
	} else {
	    OP(		INCR_STK);
	}
    } else if (isScalar) {	/* Simple scalar variable. */
	if (localIndex >= 0) {
	    if (haveImmValue) {
		OP41(	INCR_SCALAR_IMM, localIndex, immValue);
	    } else {
		OP4(	INCR_SCALAR, localIndex);
	    }
	} else {
	    if (haveImmValue) {
		OP1(	INCR_STK_IMM, immValue);
	    } else {
		OP(	INCR_STK);
	    }
	}
    } else {			/* Simple array variable. */
	if (localIndex >= 0) {
	    if (haveImmValue) {
		OP41(	INCR_ARRAY_IMM, localIndex, immValue);
	    } else {
		OP4(	INCR_ARRAY, localIndex);
	    }
	} else {
	    if (haveImmValue) {
		OP1(	INCR_ARRAY_STK_IMM, immValue);
	    } else {
		OP(	INCR_ARRAY_STK);
	    }
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileInfo*Cmd --
 *
 *	Procedures called to compile "info" subcommands.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "info" subcommand at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileInfoCommandsCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Obj *objPtr;
    char *bytes;

    /*
     * We require one compile-time known argument for the case we can compile.
     */

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    objPtr = Tcl_NewObj();
    Tcl_IncrRefCount(objPtr);
    if (!TclWordKnownAtCompileTime(tokenPtr, objPtr)) {
	goto notCompilable;
    }
    bytes = Tcl_GetString(objPtr);

    /*
     * We require that the argument start with "::" and not have any of "*\[?"
     * in it. (Theoretically, we should look in only the final component, but
     * the difference is so slight given current naming practices.)
     */

    if (bytes[0] != ':' || bytes[1] != ':' || !TclMatchIsTrivial(bytes)) {
	goto notCompilable;
    }
    Tcl_DecrRefCount(objPtr);

    /*
     * Confirmed as a literal that will not frighten the horses. Compile. Note
     * that the result needs to be list-ified.
     */

    PUSH_SUBST_WORD(tokenPtr, 1);
    OP(		RESOLVE_COMMAND);
    OP(		DUP);
    OP(		STR_LEN);
    OP4(	JUMP_FALSE, 10);
    OP4(	LIST, 1);
    return TCL_OK;

  notCompilable:
    Tcl_DecrRefCount(objPtr);
    return TCL_ERROR;
}

int
TclCompileInfoCoroutineCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Only compile [info coroutine] without arguments.
     */

    if (parsePtr->numWords != 1) {
	return TCL_ERROR;
    }

    /*
     * Not much to do; we compile to a single instruction...
     */

    OP(		COROUTINE_NAME);
    return TCL_OK;
}

int
TclCompileInfoExistsCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;
    int isScalar, simpleVarName, localIndex;
    DefineLineInformation;	/* TIP #280 */

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    /*
     * Decide if we can use a frame slot for the var/array name or if we need
     * to emit code to compute and push the name at runtime. We use a frame
     * slot (entry in the array of local vars) if we are compiling a procedure
     * body and if the name is simple text that does not include namespace
     * qualifiers.
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_VAR(	tokenPtr, 1,
	    &localIndex, &simpleVarName, &isScalar);

    /*
     * Emit instruction to check the variable for existence.
     */

    if (!simpleVarName) {
	OP(	EXIST_STK);
    } else if (isScalar) {
	if (localIndex < 0) {
	    OP(	EXIST_STK);
	} else {
	    OP4(EXIST_SCALAR, localIndex);
	}
    } else {
	if (localIndex < 0) {
	    OP(	EXIST_ARRAY_STK);
	} else {
	    OP4(EXIST_ARRAY, localIndex);
	}
    }

    return TCL_OK;
}

int
TclCompileInfoLevelCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Only compile [info level] without arguments or with a single argument.
     */

    if (parsePtr->numWords == 1) {
	/*
	 * Not much to do; we compile to a single instruction...
	 */

	OP(	INFO_LEVEL_NUM);
    } else if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    } else {
	DefineLineInformation;	/* TIP #280 */

	/*
	 * Compile the argument, then add the instruction to convert it into a
	 * list of arguments.
	 */

	PUSH_SUBST_WORD(TokenAfter(parsePtr->tokenPtr), 1);
	OP(	INFO_LEVEL_ARGS);
    }
    return TCL_OK;
}

int
TclCompileInfoObjectClassCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    PUSH_SUBST_WORD(tokenPtr, 1);
    OP(		TCLOO_CLASS);
    return TCL_OK;
}

int
TclCompileInfoObjectIsACmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);

    /*
     * We only handle [info object isa object <somevalue>]. The first three
     * words are compressed to a single token by the ensemble compilation
     * engine.
     */

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD || tokenPtr[1].size < 1
	    || strncmp(tokenPtr[1].start, "object", tokenPtr[1].size)) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(tokenPtr);

    /*
     * Issue the code.
     */

    PUSH_SUBST_WORD(tokenPtr, 2);
    OP(		TCLOO_IS_OBJECT);
    return TCL_OK;
}

int
TclCompileInfoObjectNamespaceCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    PUSH_SUBST_WORD(tokenPtr, 1);
    OP(		TCLOO_NS);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLappendCmd --
 *
 *	Procedure called to compile the "lappend" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "lappend" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLappendCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *varTokenPtr;
    int simpleVarName, isScalar, localIndex, numWords;
    DefineLineInformation;	/* TIP #280 */

    /*
     * If we're not in a procedure, don't compile.
     */

    if (envPtr->procPtr == NULL) {
	return TCL_ERROR;
    }

    numWords = parsePtr->numWords;
    if (numWords == 1) {
	return TCL_ERROR;
    }
    if (numWords != 3) {
	/*
	 * LAPPEND instructions currently only handle one value appends.
	 */

	return TCL_ERROR;
    }

    /*
     * Decide if we can use a frame slot for the var/array name or if we
     * need to emit code to compute and push the name at runtime. We use a
     * frame slot (entry in the array of local vars) if we are compiling a
     * procedure body and if the name is simple text that does not include
     * namespace qualifiers.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_VAR(	varTokenPtr, 1,
	    &localIndex, &simpleVarName, &isScalar);

    /*
     * If we are doing an assignment, push the new value. In the no values
     * case, create an empty object.
     */

    if (numWords > 2) {
	PUSH_SUBST_WORD(TokenAfter(varTokenPtr), 2);
    }

    /*
     * Emit instructions to set/get the variable.
     */

    /*
     * The *_STK opcodes should be refactored to make better use of existing
     * LOAD/STORE instructions.
     */

    if (!simpleVarName) {
	OP(	LAPPEND_STK);
    } else if (isScalar) {
	if (localIndex < 0) {
	    OP(	LAPPEND_STK);
	} else {
	    OP4(LAPPEND_SCALAR, localIndex);
	}
    } else {
	if (localIndex < 0) {
	    OP(	LAPPEND_ARRAY_STK);
	} else {
	    OP4(LAPPEND_ARRAY, localIndex);
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLassignCmd --
 *
 *	Procedure called to compile the "lassign" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "lassign" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLassignCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;
    int simpleVarName, isScalar, localIndex, numWords, idx;
    DefineLineInformation;	/* TIP #280 */

    numWords = parsePtr->numWords;

    /*
     * Check for command syntax error, but we'll punt that to runtime.
     */

    if (numWords < 3) {
	return TCL_ERROR;
    }

    /*
     * Generate code to push list being taken apart by [lassign].
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_SUBST_WORD(tokenPtr, 1);

    /*
     * Generate code to assign values from the list to variables.
     */

    for (idx=0 ; idx<numWords-2 ; idx++) {
	tokenPtr = TokenAfter(tokenPtr);

	/*
	 * Generate the next variable name.
	 */

	PUSH_VAR(	tokenPtr, idx+2,
		&localIndex, &simpleVarName, &isScalar);

	/*
	 * Emit instructions to get the idx'th item out of the list value on
	 * the stack and assign it to the variable.
	 */

	if (!simpleVarName) {
	    OP(		UNDER);
	    OP4(	LIST_INDEX_IMM, idx);
	    OP(		STORE_STK);
	    OP(		POP);
	} else if (isScalar) {
	    if (localIndex >= 0) {
		OP(	DUP);
		OP4(	LIST_INDEX_IMM, idx);
		OP4(	STORE_SCALAR, localIndex);
		OP(	POP);
	    } else {
		OP(	UNDER);
		OP4(	LIST_INDEX_IMM, idx);
		OP(	STORE_SCALAR_STK);
		OP(	POP);
	    }
	} else {
	    if (localIndex >= 0) {
		OP(	UNDER);
		OP4(	LIST_INDEX_IMM, idx);
		OP4(	STORE_ARRAY, localIndex);
		OP(	POP);
	    } else {
		OP4(	OVER, 2);
		OP4(	LIST_INDEX_IMM, idx);
		OP(	STORE_ARRAY_STK);
		OP(	POP);
	    }
	}
    }

    /*
     * Generate code to leave the rest of the list on the stack.
     */

    OP44(		LIST_RANGE_IMM, idx, -2 /* == "end" */);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLindexCmd --
 *
 *	Procedure called to compile the "lindex" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "lindex" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLindexCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *idxTokenPtr, *valTokenPtr;
    int i, numWords = parsePtr->numWords;
    DefineLineInformation;	/* TIP #280 */

    /*
     * Quit if too few args.
     */

    if (numWords <= 1) {
	return TCL_ERROR;
    }

    valTokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (numWords != 3) {
	goto emitComplexLindex;
    }

    idxTokenPtr = TokenAfter(valTokenPtr);
    if (idxTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	Tcl_Obj *tmpObj;
	int idx, result;

	tmpObj = Tcl_NewStringObj(idxTokenPtr[1].start, idxTokenPtr[1].size);
	result = TclGetIntFromObj(NULL, tmpObj, &idx);
	if (result == TCL_OK) {
	    if (idx < 0) {
		result = TCL_ERROR;
	    }
	} else {
	    result = TclGetIntForIndexM(NULL, tmpObj, -2, &idx);
	    if (result == TCL_OK && idx > -2) {
		result = TCL_ERROR;
	    }
	}
	TclDecrRefCount(tmpObj);

	if (result == TCL_OK) {
	    /*
	     * All checks have been completed, and we have exactly one of
	     * these constructs:
	     *	 lindex <arbitraryValue> <posInt>
	     *	 lindex <arbitraryValue> end-<posInt>
	     * This is best compiled as a push of the arbitrary value followed
	     * by an "immediate lindex" which is the most efficient variety.
	     */

	    PUSH_SUBST_WORD(valTokenPtr, 1);
	    OP4(LIST_INDEX_IMM, idx);
	    return TCL_OK;
	}

	/*
	 * If the conversion failed or the value was negative, we just keep on
	 * going with the more complex compilation.
	 */
    }

    /*
     * Push the operands onto the stack.
     */

  emitComplexLindex:
    for (i=1 ; i<numWords ; i++) {
	PUSH_SUBST_WORD(valTokenPtr, i);
	valTokenPtr = TokenAfter(valTokenPtr);
    }

    /*
     * Emit INST_LIST_INDEX if objc==3, or INST_LIST_INDEX_MULTI if there are
     * multiple index args.
     */

    if (numWords == 3) {
	OP(	LIST_INDEX);
    } else {
	OP4(	LIST_INDEX_MULTI, numWords-1);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileListCmd --
 *
 *	Procedure called to compile the "list" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "list" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileListCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *valueTokenPtr;
    int i, numWords;

    /*
     * If we're not in a procedure, don't compile.
     */

    if (envPtr->procPtr == NULL) {
	return TCL_ERROR;
    }

    if (parsePtr->numWords == 1) {
	/*
	 * [list] without arguments just pushes an empty object.
	 */

	PUSH(	"");
    } else {
	/*
	 * Push the all values onto the stack.
	 */

	numWords = parsePtr->numWords;
	valueTokenPtr = TokenAfter(parsePtr->tokenPtr);
	for (i = 1; i < numWords; i++) {
	    PUSH_SUBST_WORD(valueTokenPtr, i);
	    valueTokenPtr = TokenAfter(valueTokenPtr);
	}
	OP4(	LIST, numWords-1);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLlengthCmd --
 *
 *	Procedure called to compile the "llength" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "llength" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLlengthCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *varTokenPtr;
    DefineLineInformation;	/* TIP #280 */

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    varTokenPtr = TokenAfter(parsePtr->tokenPtr);

    PUSH_SUBST_WORD(varTokenPtr, 1);
    OP(		LIST_LENGTH);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLrangeCmd --
 *
 *	How to compile the "lrange" command. We only bother because we needed
 *	the opcode anyway for "lassign".
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLrangeCmd(
    Tcl_Interp *interp,		/* Tcl interpreter for context. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the
				 * command. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    Tcl_Token *tokenPtr, *listTokenPtr;
    DefineLineInformation;	/* TIP #280 */
    Tcl_Obj *tmpObj;
    int idx1, idx2, result;

    if (parsePtr->numWords != 4) {
	return TCL_ERROR;
    }
    listTokenPtr = TokenAfter(parsePtr->tokenPtr);

    /*
     * Parse the first index. Will only compile if it is constant and not an
     * _integer_ less than zero (since we reserve negative indices here for
     * end-relative indexing).
     */

    tokenPtr = TokenAfter(listTokenPtr);
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }
    tmpObj = Tcl_NewStringObj(tokenPtr[1].start, tokenPtr[1].size);
    result = TclGetIntFromObj(NULL, tmpObj, &idx1);
    if (result == TCL_OK) {
	if (idx1 < 0) {
	    result = TCL_ERROR;
	}
    } else {
	result = TclGetIntForIndexM(NULL, tmpObj, -2, &idx1);
	if (result == TCL_OK && idx1 > -2) {
	    result = TCL_ERROR;
	}
    }
    TclDecrRefCount(tmpObj);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Parse the second index. Will only compile if it is constant and not an
     * _integer_ less than zero (since we reserve negative indices here for
     * end-relative indexing).
     */

    tokenPtr = TokenAfter(tokenPtr);
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }
    tmpObj = Tcl_NewStringObj(tokenPtr[1].start, tokenPtr[1].size);
    result = TclGetIntFromObj(NULL, tmpObj, &idx2);
    if (result == TCL_OK) {
	if (idx2 < 0) {
	    result = TCL_ERROR;
	}
    } else {
	result = TclGetIntForIndexM(NULL, tmpObj, -2, &idx2);
	if (result == TCL_OK && idx2 > -2) {
	    result = TCL_ERROR;
	}
    }
    TclDecrRefCount(tmpObj);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Issue instructions. It's not safe to skip doing the LIST_RANGE, as
     * we've not proved that the 'list' argument is really a list. Not that it
     * is worth trying to do that given current knowledge.
     */

    PUSH_SUBST_WORD(listTokenPtr, 1);
    OP44(	LIST_RANGE_IMM, idx1, idx2);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLreplaceCmd --
 *
 *	How to compile the "lreplace" command. We only bother with the case
 *	where there are no elements to insert and where both the 'first' and
 *	'last' arguments are constant and one can be deterined to be at the
 *	end of the list. (This is the case that could also be written with
 *	"lrange".)
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLreplaceCmd(
    Tcl_Interp *interp,		/* Tcl interpreter for context. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the
				 * command. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    Tcl_Token *tokenPtr, *listTokenPtr;
    DefineLineInformation;	/* TIP #280 */
    Tcl_Obj *tmpObj;
    int idx1, idx2, result, guaranteedDropAll = 0;

    if (parsePtr->numWords != 4) {
	return TCL_ERROR;
    }
    listTokenPtr = TokenAfter(parsePtr->tokenPtr);

    /*
     * Parse the first index. Will only compile if it is constant and not an
     * _integer_ less than zero (since we reserve negative indices here for
     * end-relative indexing).
     */

    tokenPtr = TokenAfter(listTokenPtr);
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }
    tmpObj = Tcl_NewStringObj(tokenPtr[1].start, tokenPtr[1].size);
    result = TclGetIntFromObj(NULL, tmpObj, &idx1);
    if (result == TCL_OK) {
	if (idx1 < 0) {
	    result = TCL_ERROR;
	}
    } else {
	result = TclGetIntForIndexM(NULL, tmpObj, -2, &idx1);
	if (result == TCL_OK && idx1 > -2) {
	    result = TCL_ERROR;
	}
    }
    TclDecrRefCount(tmpObj);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Parse the second index. Will only compile if it is constant and not an
     * _integer_ less than zero (since we reserve negative indices here for
     * end-relative indexing).
     */

    tokenPtr = TokenAfter(tokenPtr);
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }
    tmpObj = Tcl_NewStringObj(tokenPtr[1].start, tokenPtr[1].size);
    result = TclGetIntFromObj(NULL, tmpObj, &idx2);
    if (result == TCL_OK) {
	if (idx2 < 0) {
	    result = TCL_ERROR;
	}
    } else {
	result = TclGetIntForIndexM(NULL, tmpObj, -2, &idx2);
	if (result == TCL_OK && idx2 > -2) {
	    result = TCL_ERROR;
	}
    }
    TclDecrRefCount(tmpObj);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Sanity check: can only issue when we're removing a range at one or
     * other end of the list. If we're at one end or the other, convert the
     * indices into the equivalent for an [lrange].
     */

    if (idx1 == 0) {
	if (idx2 == -2) {
	    guaranteedDropAll = 1;
	}
	idx1 = idx2 + 1;
	idx2 = -2;
    } else if (idx2 == -2) {
	idx2 = idx1 - 1;
	idx1 = 0;
    } else {
	return TCL_ERROR;
    }

    /*
     * Issue instructions. It's not safe to skip doing the LIST_RANGE, as
     * we've not proved that the 'list' argument is really a list. Not that it
     * is worth trying to do that given current knowledge.
     */

    PUSH_SUBST_WORD(listTokenPtr, 1);
    if (guaranteedDropAll) {
	OP(	LIST_LENGTH);
	OP(	POP);
	PUSH(	"");
    } else {
	OP44(	LIST_RANGE_IMM, idx1, idx2);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLsetCmd --
 *
 *	Procedure called to compile the "lset" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "lset" command at
 *	runtime.
 *
 * The general template for execution of the "lset" command is:
 *	(1) Instructions to push the variable name, unless the variable is
 *	    local to the stack frame.
 *	(2) If the variable is an array element, instructions to push the
 *	    array element name.
 *	(3) Instructions to push each of zero or more "index" arguments to the
 *	    stack, followed with the "newValue" element.
 *	(4) Instructions to duplicate the variable name and/or array element
 *	    name onto the top of the stack, if either was pushed at steps (1)
 *	    and (2).
 *	(5) The appropriate INST_LOAD_* instruction to place the original
 *	    value of the list variable at top of stack.
 *	(6) At this point, the stack contains:
 *		varName? arrayElementName? index1 index2 ... newValue oldList
 *	    The compiler emits one of INST_LSET_FLAT or INST_LSET_LIST
 *	    according as whether there is exactly one index element (LIST) or
 *	    either zero or else two or more (FLAT). This instruction removes
 *	    everything from the stack except for the two names and pushes the
 *	    new value of the variable.
 *	(7) Finally, INST_STORE_* stores the new value in the variable and
 *	    cleans up the stack.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLsetCmd(
    Tcl_Interp *interp,		/* Tcl interpreter for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the
				 * command. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    int tempDepth;		/* Depth used for emitting one part of the
				 * code burst. */
    Tcl_Token *varTokenPtr;	/* Pointer to the Tcl_Token representing the
				 * parse of the variable name. */
    int localIndex;		/* Index of var in local var table. */
    int simpleVarName;		/* Flag == 1 if var name is simple. */
    int isScalar;		/* Flag == 1 if scalar, 0 if array. */
    int i;
    DefineLineInformation;	/* TIP #280 */

    /*
     * Check argument count.
     */

    if (parsePtr->numWords < 3) {
	/*
	 * Fail at run time, not in compilation.
	 */

	return TCL_ERROR;
    }

    /*
     * Decide if we can use a frame slot for the var/array name or if we need
     * to emit code to compute and push the name at runtime. We use a frame
     * slot (entry in the array of local vars) if we are compiling a procedure
     * body and if the name is simple text that does not include namespace
     * qualifiers.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_VAR(	varTokenPtr, 1,
	    &localIndex, &simpleVarName, &isScalar);

    /*
     * Push the "index" args and the new element value.
     */

    for (i=2 ; i<parsePtr->numWords ; ++i) {
	varTokenPtr = TokenAfter(varTokenPtr);
	PUSH_SUBST_WORD(varTokenPtr, i);
    }

    /*
     * Duplicate the variable name if it's been pushed.
     */

    if (!simpleVarName || localIndex < 0) {
	if (!simpleVarName || isScalar) {
	    tempDepth = parsePtr->numWords - 2;
	} else {
	    tempDepth = parsePtr->numWords - 1;
	}
	OP4(	OVER, tempDepth);
    }

    /*
     * Duplicate an array index if one's been pushed.
     */

    if (simpleVarName && !isScalar) {
	if (localIndex < 0) {
	    tempDepth = parsePtr->numWords - 1;
	} else {
	    tempDepth = parsePtr->numWords - 2;
	}
	OP4(	OVER, tempDepth);
    }

    /*
     * Emit code to load the variable's value.
     */

    if (!simpleVarName) {
	OP(	LOAD_STK);
    } else if (isScalar) {
	if (localIndex < 0) {
	    OP(	LOAD_SCALAR_STK);
	} else {
	    OP4(LOAD_SCALAR, localIndex);
	}
    } else {
	if (localIndex < 0) {
	    OP(	LOAD_ARRAY_STK);
	} else {
	    OP4(LOAD_ARRAY, localIndex);
	}
    }

    /*
     * Emit the correct variety of 'lset' instruction.
     */

    if (parsePtr->numWords == 4) {
	OP(	LSET_LIST);
    } else {
	OP4(	LSET_FLAT, parsePtr->numWords-1);
    }

    /*
     * Emit code to put the value back in the variable.
     */

    if (!simpleVarName) {
	OP(	STORE_STK);
    } else if (isScalar) {
	if (localIndex < 0) {
	    OP(	STORE_SCALAR_STK);
	} else {
	    OP4(STORE_SCALAR, localIndex);
	}
    } else {
	if (localIndex < 0) {
	    OP(	STORE_ARRAY_STK);
	} else {
	    OP4(STORE_ARRAY, localIndex);
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLmapCmd --
 *
 *	Procedure called to compile the "lmap" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "lmap" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLmapCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    return CompileEachloopCmd(interp, parsePtr, cmdPtr, envPtr,
	    TCL_EACH_COLLECT);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileNamespace*Cmd --
 *
 *	Procedures called to compile the "namespace" command; currently, only
 *	the subcommands "namespace current" and "namespace upvar" are compiled
 *	to bytecodes, and the latter only inside a procedure(-like) context.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "namespace upvar"
 *	command at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileNamespaceCurrentCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Only compile [namespace current] without arguments.
     */

    if (parsePtr->numWords != 1) {
	return TCL_ERROR;
    }

    /*
     * Not much to do; we compile to a single instruction...
     */

    OP(		NS_CURRENT);
    return TCL_OK;
}

int
TclCompileNamespaceCodeCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;
    DefineLineInformation;	/* TIP #280 */

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);

    /*
     * The specification of [namespace code] is rather shocking, in that it is
     * supposed to check if the argument is itself the result of [namespace
     * code] and not apply itself in that case. Which is excessively cautious,
     * but what the test suite checks for.
     */

    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD || (tokenPtr[1].size > 20
	    && strncmp(tokenPtr[1].start, "::namespace inscope ", 20) == 0)) {
	/*
	 * Technically, we could just pass a literal '::namespace inscope '
	 * term through, but that's something which really shouldn't be
	 * occurring as something that the user writes so we'll just punt it.
	 */

	return TCL_ERROR;
    }

    /*
     * Now we can compile using the same strategy as [namespace code]'s normal
     * implementation does internally. Note that we can't bind the namespace
     * name directly here, because TclOO plays complex games with namespaces;
     * the value needs to be determined at runtime for safety.
     */

    PUSH(	"::namespace");
    PUSH(	"inscope");
    OP(		NS_CURRENT);
    PUSH_SUBST_WORD(tokenPtr, 1);
    OP4(	LIST, 4);
    return TCL_OK;
}

int
TclCompileNamespaceQualifiersCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);
    DefineLineInformation;	/* TIP #280 */
    int off;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    PUSH_SUBST_WORD(tokenPtr, 1);
    PUSH(	"0");
    PUSH(	"::");
    OP4(	OVER, 2);
    OP(		STR_FIND_LAST);
    LABEL(off);
    PUSH(	"1");
    OP(		SUB);
    OP4(	OVER, 2);
    OP(		UNDER);
    OP(		STR_INDEX);
    PUSH(	":");
    OP(		STR_EQ);
    BACKJUMP(off, JUMP_TRUE);
    OP(		STR_RANGE);
    return TCL_OK;
}

int
TclCompileNamespaceTailCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);
    DefineLineInformation;	/* TIP #280 */
    JumpFixup jumpFixup;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    /*
     * Take care; only add 2 to found index if the string was actually found.
     */

    PUSH_SUBST_WORD(tokenPtr, 1);
    PUSH(	"::");
    OP(		UNDER);
    OP(		STR_FIND_LAST);
    OP(		DUP);
    PUSH(	"0");
    OP(		GE);
    TclEmitForwardJump(envPtr, TCL_FALSE_JUMP, &jumpFixup);
    PUSH(	"2");
    OP(		ADD);
    TclFixupForwardJumpToHere(envPtr, &jumpFixup, 127);
    PUSH(	"end");
    OP(		STR_RANGE);
    return TCL_OK;
}

int
TclCompileNamespaceUpvarCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr, *otherTokenPtr, *localTokenPtr;
    int simpleVarName, isScalar, localIndex, numWords, i;
    DefineLineInformation;	/* TIP #280 */

    if (envPtr->procPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Only compile [namespace upvar ...]: needs an even number of args, >=4
     */

    numWords = parsePtr->numWords;
    if ((numWords % 2) || (numWords < 4)) {
	return TCL_ERROR;
    }

    /*
     * Push the namespace
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_SUBST_WORD(tokenPtr, 1);

    /*
     * Loop over the (otherVar, thisVar) pairs. If any of the thisVar is not a
     * local variable, return an error so that the non-compiled command will
     * be called at runtime.
     */

    localTokenPtr = tokenPtr;
    for (i=3; i<=numWords; i+=2) {
	otherTokenPtr = TokenAfter(localTokenPtr);
	localTokenPtr = TokenAfter(otherTokenPtr);

	PUSH_SUBST_WORD(otherTokenPtr, i-1);
	PUSH_VAR(localTokenPtr, i,
		&localIndex, &simpleVarName, &isScalar);

	if ((localIndex < 0) || !isScalar) {
	    return TCL_ERROR;
	}
	OP4(	NSUPVAR, localIndex);
    }

    /*
     * Pop the namespace, and set the result to empty
     */

    OP(		POP);
    PUSH(	"");
    return TCL_OK;
}

int
TclCompileNamespaceWhichCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr, *opt;
    int idx;

    if (parsePtr->numWords < 2 || parsePtr->numWords > 3) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    idx = 1;

    /*
     * If there's an option, check that it's "-command". We don't handle
     * "-variable" (currently) and anything else is an error.
     */

    if (parsePtr->numWords == 3) {
	if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    return TCL_ERROR;
	}
	opt = tokenPtr + 1;
	if (opt->size < 2 || opt->size > 8
		|| strncmp(opt->start, "-command", opt->size) != 0) {
	    return TCL_ERROR;
	}
	tokenPtr = TokenAfter(tokenPtr);
	idx++;
    }

    /*
     * Issue the bytecode.
     */

    PUSH_SUBST_WORD(tokenPtr, idx);
    OP(		RESOLVE_COMMAND);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileRegexpCmd --
 *
 *	Procedure called to compile the "regexp" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "regexp" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileRegexpCmd(
    Tcl_Interp *interp,		/* Tcl interpreter for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the
				 * command. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    Tcl_Token *varTokenPtr;	/* Pointer to the Tcl_Token representing the
				 * parse of the RE or string. */
    int i, len, nocase, exact, sawLast, simple;
    const char *str;
    DefineLineInformation;	/* TIP #280 */

    /*
     * We are only interested in compiling simple regexp cases. Currently
     * supported compile cases are:
     *   regexp ?-nocase? ?--? staticString $var
     *   regexp ?-nocase? ?--? {^staticString$} $var
     */

    if (parsePtr->numWords < 3) {
	return TCL_ERROR;
    }

    simple = 0;
    nocase = 0;
    sawLast = 0;
    varTokenPtr = parsePtr->tokenPtr;

    /*
     * We only look for -nocase and -- as options. Everything else gets pushed
     * to runtime execution. This is different than regexp's runtime option
     * handling, but satisfies our stricter needs.
     */

    for (i = 1; i < parsePtr->numWords - 2; i++) {
	varTokenPtr = TokenAfter(varTokenPtr);
	if (varTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    /*
	     * Not a simple string, so punt to runtime.
	     */

	    return TCL_ERROR;
	}
	str = varTokenPtr[1].start;
	len = varTokenPtr[1].size;
	if ((len == 2) && (str[0] == '-') && (str[1] == '-')) {
	    sawLast++;
	    i++;
	    break;
	} else if ((len > 1) && (strncmp(str,"-nocase",(unsigned)len) == 0)) {
	    nocase = 1;
	} else {
	    /*
	     * Not an option we recognize.
	     */

	    return TCL_ERROR;
	}
    }

    if ((parsePtr->numWords - i) != 2) {
	/*
	 * We don't support capturing to variables.
	 */

	return TCL_ERROR;
    }

    /*
     * Get the regexp string. If it is not a simple string or can't be
     * converted to a glob pattern, push the word for the INST_REGEXP.
     * Keep changes here in sync with TclCompileSwitchCmd Switch_Regexp.
     */

    varTokenPtr = TokenAfter(varTokenPtr);

    if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	Tcl_DString ds;

	str = varTokenPtr[1].start;
	len = varTokenPtr[1].size;

	/*
	 * If it has a '-', it could be an incorrectly formed regexp command.
	 */

	if ((*str == '-') && !sawLast) {
	    return TCL_ERROR;
	}

	if (len == 0) {
	    /*
	     * The semantics of regexp are always match on re == "".
	     */

	    PUSH(	"1");
	    return TCL_OK;
	}

	/*
	 * Attempt to convert pattern to glob.  If successful, push the
	 * converted pattern as a literal.
	 */

	if (TclReToGlob(NULL, varTokenPtr[1].start, len, &ds, &exact)
		== TCL_OK) {
	    simple = 1;
	    PUSH_DSTRING(&ds);
	    Tcl_DStringFree(&ds);
	}
    }

    if (!simple) {
	PUSH_SUBST_WORD(varTokenPtr, parsePtr->numWords-2);
    }

    /*
     * Push the string arg.
     */

    varTokenPtr = TokenAfter(varTokenPtr);
    PUSH_SUBST_WORD(varTokenPtr, parsePtr->numWords-1);

    if (simple) {
	if (exact && !nocase) {
	    OP(	STR_EQ);
	} else {
	    OP1(STR_MATCH, nocase);
	}
    } else {
	/*
	 * Pass correct RE compile flags.  We use only Int1 (8-bit), but
	 * that handles all the flags we want to pass.
	 * Don't use TCL_REG_NOSUB as we may have backrefs.
	 */

	int cflags = TCL_REG_ADVANCED | (nocase ? TCL_REG_NOCASE : 0);

	OP1(	REGEXP, cflags);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileRegsubCmd --
 *
 *	Procedure called to compile the "regsub" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "regsub" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileRegsubCmd(
    Tcl_Interp *interp,		/* Tcl interpreter for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the
				 * command. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    /*
     * We only compile the case with [regsub -all] where the pattern is both
     * known at compile time and simple (i.e., no RE metacharacters). That is,
     * the pattern must be translatable into a glob like "*foo*" with no other
     * glob metacharacters inside it; there must be some "foo" in there too.
     * The substitution string must also be known at compile time and free of
     * metacharacters ("\digit" and "&"). Finally, there must not be a
     * variable mentioned in the [regsub] to write the result back to (because
     * we can't get the count of substitutions that would be the result in
     * that case). The key is that these are the conditions under which a
     * [string map] could be used instead, in particular a [string map] of the
     * form we can compile to bytecode.
     *
     * In short, we look for:
     *
     *   regsub -all [--] simpleRE string simpleReplacement
     *
     * The only optional part is the "--", and no other options are handled.
     */

    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr, *stringTokenPtr;
    Tcl_Obj *patternObj = NULL, *replacementObj = NULL;
    Tcl_DString pattern;
    const char *bytes;
    int len, exact, result = TCL_ERROR;

    if (parsePtr->numWords < 5 || parsePtr->numWords > 6) {
	return TCL_ERROR;
    }

    /*
     * Parse the "-all", which must be the first argument (other options not
     * supported, non-"-all" substitution we can't compile).
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD || tokenPtr[1].size != 4
	    || strncmp(tokenPtr[1].start, "-all", 4)) {
	return TCL_ERROR;
    }

    /*
     * Get the pattern into patternObj, checking for "--" in the process.
     */

    Tcl_DStringInit(&pattern);
    tokenPtr = TokenAfter(tokenPtr);
    patternObj = Tcl_NewObj();
    if (!TclWordKnownAtCompileTime(tokenPtr, patternObj)) {
	goto done;
    }
    if (Tcl_GetString(patternObj)[0] == '-') {
	if (strcmp(Tcl_GetString(patternObj), "--") != 0
		|| parsePtr->numWords == 5) {
	    goto done;
	}
	tokenPtr = TokenAfter(tokenPtr);
	Tcl_DecrRefCount(patternObj);
	patternObj = Tcl_NewObj();
	if (!TclWordKnownAtCompileTime(tokenPtr, patternObj)) {
	    goto done;
	}
    } else if (parsePtr->numWords == 6) {
	goto done;
    }

    /*
     * Identify the code which produces the string to apply the substitution
     * to (stringTokenPtr), and the replacement string (into replacementObj).
     */

    stringTokenPtr = TokenAfter(tokenPtr);
    tokenPtr = TokenAfter(stringTokenPtr);
    replacementObj = Tcl_NewObj();
    if (!TclWordKnownAtCompileTime(tokenPtr, replacementObj)) {
	goto done;
    }

    /*
     * Next, higher-level checks. Is the RE a very simple glob? Is the
     * replacement "simple"?
     */

    bytes = Tcl_GetStringFromObj(patternObj, &len);
    if (TclReToGlob(NULL, bytes, len, &pattern, &exact) != TCL_OK || exact) {
	goto done;
    }
    bytes = Tcl_DStringValue(&pattern);
    if (*bytes++ != '*') {
	goto done;
    }
    while (1) {
	switch (*bytes) {
	case '*':
	    if (bytes[1] == '\0') {
		/*
		 * OK, we've proved there are no metacharacters except for the
		 * '*' at each end.
		 */

		len = Tcl_DStringLength(&pattern) - 2;
		if (len > 0) {
		    goto isSimpleGlob;
		}

		/*
		 * The pattern is "**"! I believe that should be impossible,
		 * but we definitely can't handle that at all.
		 */
	    }
	case '\0': case '?': case '[': case '\\':
	    goto done;
	}
	bytes++;
    }
  isSimpleGlob:
    for (bytes = Tcl_GetString(replacementObj); *bytes; bytes++) {
	switch (*bytes) {
	case '\\': case '&':
	    goto done;
	}
    }

    /*
     * Proved the simplicity constraints! Time to issue the code.
     */

    result = TCL_OK;
    bytes = Tcl_DStringValue(&pattern) + 1;
    PushLiteral(envPtr,	bytes, len);
    PUSH_OBJ(replacementObj);
    PUSH_SUBST_WORD(stringTokenPtr, parsePtr->numWords-2);
    OP(		STR_MAP);

  done:
    Tcl_DStringFree(&pattern);
    if (patternObj) {
	Tcl_DecrRefCount(patternObj);
    }
    if (replacementObj) {
	Tcl_DecrRefCount(replacementObj);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileReturnCmd --
 *
 *	Procedure called to compile the "return" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "return" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileReturnCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * General syntax: [return ?-option value ...? ?result?]
     * An even number of words means an explicit result argument is present.
     */
    int level, code, objc, size, status = TCL_OK;
    int numWords = parsePtr->numWords;
    int explicitResult = (0 == (numWords % 2));
    int numOptionWords = numWords - 1 - explicitResult;
    int savedStackDepth = envPtr->currStackDepth;
    Tcl_Obj *returnOpts, **objv;
    Tcl_Token *wordTokenPtr = TokenAfter(parsePtr->tokenPtr);
    DefineLineInformation;	/* TIP #280 */

    /*
     * Check for special case which can always be compiled:
     *	    return -options <opts> <msg>
     * Unlike the normal [return] compilation, this version does everything at
     * runtime so it can handle arbitrary words and not just literals. Note
     * that if INST_RETURN_STK wasn't already needed for something else
     * ('finally' clause processing) this piece of code would not be present.
     */

    if ((numWords == 4) && (wordTokenPtr->type == TCL_TOKEN_SIMPLE_WORD)
	    && (wordTokenPtr[1].size == 8)
	    && (strncmp(wordTokenPtr[1].start, "-options", 8) == 0)) {
	Tcl_Token *optsTokenPtr = TokenAfter(wordTokenPtr);
	Tcl_Token *msgTokenPtr = TokenAfter(optsTokenPtr);

	PUSH_SUBST_WORD(optsTokenPtr, 2);
	PUSH_SUBST_WORD(msgTokenPtr, 3);
	OP(	RETURN_STK);
	envPtr->currStackDepth = savedStackDepth + 1;
	return TCL_OK;
    }

    /*
     * Allocate some working space.
     */

    objv = TclStackAlloc(interp, numOptionWords * sizeof(Tcl_Obj *));

    /*
     * Scan through the return options. If any are unknown at compile time,
     * there is no value in bytecompiling. Save the option values known in an
     * objv array for merging into a return options dictionary.
     */

    for (objc = 0; objc < numOptionWords; objc++) {
	objv[objc] = Tcl_NewObj();
	Tcl_IncrRefCount(objv[objc]);
	if (!TclWordKnownAtCompileTime(wordTokenPtr, objv[objc])) {
	    objc++;
	    status = TCL_ERROR;
	    goto cleanup;
	}
	wordTokenPtr = TokenAfter(wordTokenPtr);
    }
    status = TclMergeReturnOptions(interp, objc, objv,
	    &returnOpts, &code, &level);
  cleanup:
    while (--objc >= 0) {
	TclDecrRefCount(objv[objc]);
    }
    TclStackFree(interp, objv);
    if (TCL_ERROR == status) {
	/*
	 * Something was bogus in the return options. Clear the error message,
	 * and report back to the compiler that this must be interpreted at
	 * runtime.
	 */

	Tcl_ResetResult(interp);
	return TCL_ERROR;
    }

    /*
     * All options are known at compile time, so we're going to bytecompile.
     * Emit instructions to push the result on the stack.
     */

    if (explicitResult) {
	PUSH_SUBST_WORD(wordTokenPtr, numWords-1);
    } else {
	/*
	 * No explict result argument, so default result is empty string.
	 */

	PUSH(	"");
    }

    /*
     * Check for optimization: When [return] is in a proc, and there's no
     * enclosing [catch], and there are no return options, then the INST_DONE
     * instruction is equivalent, and may be more efficient.
     */

    if (numOptionWords == 0 && envPtr->procPtr != NULL) {
	/*
	 * We have default return options and we're in a proc ...
	 */

	int index = envPtr->exceptArrayNext - 1;
	int enclosingCatch = 0;

	while (index >= 0) {
	    ExceptionRange range = envPtr->exceptArrayPtr[index];

	    if ((range.type == CATCH_EXCEPTION_RANGE)
		    && (range.catchOffset == -1)) {
		enclosingCatch = 1;
		break;
	    }
	    index--;
	}
	if (!enclosingCatch) {
	    /*
	     * ... and there is no enclosing catch. Issue the maximally
	     * efficient exit instruction.
	     */

	    Tcl_DecrRefCount(returnOpts);
	    OP(	DONE);
	    return TCL_OK;
	}
    }

    /* Optimize [return -level 0 $x]. */
    Tcl_DictObjSize(NULL, returnOpts, &size);
    if (size == 0 && level == 0 && code == TCL_OK) {
	Tcl_DecrRefCount(returnOpts);
	return TCL_OK;
    }

    /*
     * Could not use the optimization, so we push the return options dict, and
     * emit the INST_RETURN_IMM instruction with code and level as operands.
     */

    CompileReturnInternal(envPtr, INST_RETURN_IMM, code, level, returnOpts);
    return TCL_OK;
}

static void
CompileReturnInternal(
    CompileEnv *envPtr,
    unsigned char op,
    int code,
    int level,
    Tcl_Obj *returnOpts)
{
    TclEmitPush(TclAddLiteralObj(envPtr, returnOpts, NULL), envPtr);
    TclEmitInstInt4(op, code, envPtr);
    TclEmitInt4(level, envPtr);
}

void
TclCompileSyntaxError(
    Tcl_Interp *interp,
    CompileEnv *envPtr)
{
    Tcl_Obj *msg = Tcl_GetObjResult(interp);
    int numBytes;
    const char *bytes = TclGetStringFromObj(msg, &numBytes);

    TclErrorStackResetIf(interp, bytes, numBytes);
    TclEmitPush(TclRegisterNewLiteral(envPtr, bytes, numBytes), envPtr);
    CompileReturnInternal(envPtr, INST_SYNTAX, TCL_ERROR, 0,
	    TclNoErrorStack(interp, Tcl_GetReturnOptions(interp, TCL_ERROR)));
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileUpvarCmd --
 *
 *	Procedure called to compile the "upvar" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "upvar" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileUpvarCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr, *otherTokenPtr, *localTokenPtr;
    int simpleVarName, isScalar, localIndex, numWords, i;
    DefineLineInformation;	/* TIP #280 */
    Tcl_Obj *objPtr = Tcl_NewObj();

    if (envPtr->procPtr == NULL) {
	Tcl_DecrRefCount(objPtr);
	return TCL_ERROR;
    }

    numWords = parsePtr->numWords;
    if (numWords < 3) {
	Tcl_DecrRefCount(objPtr);
	return TCL_ERROR;
    }

    /*
     * Push the frame index if it is known at compile time
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (TclWordKnownAtCompileTime(tokenPtr, objPtr)) {
	CallFrame *framePtr;
	const Tcl_ObjType *newTypePtr, *typePtr = objPtr->typePtr;

	/*
	 * Attempt to convert to a level reference. Note that TclObjGetFrame
	 * only changes the obj type when a conversion was successful.
	 */

	TclObjGetFrame(interp, objPtr, &framePtr);
	newTypePtr = objPtr->typePtr;
	Tcl_DecrRefCount(objPtr);

	if (newTypePtr != typePtr) {
	    if (numWords%2) {
		return TCL_ERROR;
	    }
	    PUSH_SUBST_WORD(tokenPtr, 1);
	    otherTokenPtr = TokenAfter(tokenPtr);
	    i = 4;
	} else {
	    if (!(numWords%2)) {
		return TCL_ERROR;
	    }
	    PUSH(	"1");
	    otherTokenPtr = tokenPtr;
	    i = 3;
	}
    } else {
	Tcl_DecrRefCount(objPtr);
	return TCL_ERROR;
    }

    /*
     * Loop over the (otherVar, thisVar) pairs. If any of the thisVar is not a
     * local variable, return an error so that the non-compiled command will
     * be called at runtime.
     */

    for (; i<=numWords; i+=2, otherTokenPtr = TokenAfter(localTokenPtr)) {
	localTokenPtr = TokenAfter(otherTokenPtr);

	PUSH_SUBST_WORD(otherTokenPtr, 1);
	PUSH_VAR(localTokenPtr, 1,
		&localIndex, &simpleVarName, &isScalar);

	if ((localIndex < 0) || !isScalar) {
	    return TCL_ERROR;
	}
	OP4(	UPVAR, localIndex);
    }

    /*
     * Pop the frame index, and set the result to empty
     */

    OP(		POP);
    PUSH(	"");
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileVariableCmd --
 *
 *	Procedure called to compile the "variable" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "variable" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileVariableCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *varTokenPtr, *valueTokenPtr;
    int localIndex, numWords, i;
    DefineLineInformation;	/* TIP #280 */

    numWords = parsePtr->numWords;
    if (numWords < 2) {
	return TCL_ERROR;
    }

    /*
     * Bail out if not compiling a proc body
     */

    if (envPtr->procPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Loop over the (var, value) pairs.
     */

    valueTokenPtr = parsePtr->tokenPtr;
    for (i=2; i<=numWords; i+=2) {
	varTokenPtr = TokenAfter(valueTokenPtr);
	valueTokenPtr = TokenAfter(varTokenPtr);

	localIndex = IndexTailVarIfKnown(interp, varTokenPtr, envPtr);

	if (localIndex < 0) {
	    return TCL_ERROR;
	}

	PUSH_SUBST_WORD(varTokenPtr, i-1);
	OP4(	VARIABLE, localIndex);

	if (i != numWords) {
	    /*
	     * A value has been given: set the variable, pop the value
	     */

	    PUSH_SUBST_WORD(valueTokenPtr, i);
	    OP4(STORE_SCALAR, localIndex);
	    OP(	POP);
	}
    }

    /*
     * Set the result to empty
     */

    PUSH(	"");
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IndexTailVarIfKnown --
 *
 *	Procedure used in compiling [global] and [variable] commands. It
 *	inspects the variable name described by varTokenPtr and, if the tail
 *	is known at compile time, defines a corresponding local variable.
 *
 * Results:
 *	Returns the variable's index in the table of compiled locals if the
 *	tail is known at compile time, or -1 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
IndexTailVarIfKnown(
    Tcl_Interp *interp,
    Tcl_Token *varTokenPtr,	/* Token representing the variable name */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Obj *tailPtr;
    const char *tailName, *p;
    int len, n = varTokenPtr->numComponents;
    Tcl_Token *lastTokenPtr;
    int full, localIndex;

    /*
     * Determine if the tail is (a) known at compile time, and (b) not an
     * array element. Should any of these fail, return an error so that the
     * non-compiled command will be called at runtime.
     *
     * In order for the tail to be known at compile time, the last token in
     * the word has to be constant and contain "::" if it is not the only one.
     */

    if (!EnvHasLVT(envPtr)) {
	return -1;
    }

    TclNewObj(tailPtr);
    if (TclWordKnownAtCompileTime(varTokenPtr, tailPtr)) {
	full = 1;
	lastTokenPtr = varTokenPtr;
    } else {
	full = 0;
	lastTokenPtr = varTokenPtr + n;
	if (!TclWordKnownAtCompileTime(lastTokenPtr, tailPtr)) {
	    Tcl_DecrRefCount(tailPtr);
	    return -1;
	}
    }

    tailName = TclGetStringFromObj(tailPtr, &len);

    if (len) {
	if (*(tailName+len-1) == ')') {
	    /*
	     * Possible array: bail out
	     */

	    Tcl_DecrRefCount(tailPtr);
	    return -1;
	}

	/*
	 * Get the tail: immediately after the last '::'
	 */

	for (p = tailName + len -1; p > tailName; p--) {
	    if ((*p == ':') && (*(p-1) == ':')) {
		p++;
		break;
	    }
	}
	if (!full && (p == tailName)) {
	    /*
	     * No :: in the last component.
	     */

	    Tcl_DecrRefCount(tailPtr);
	    return -1;
	}
	len -= p - tailName;
	tailName = p;
    }

    localIndex = TclFindCompiledLocal(tailName, len, 1, envPtr);
    Tcl_DecrRefCount(tailPtr);
    return localIndex;
}

int
TclCompileObjectSelfCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * We only handle [self] and [self object] (which is the same operation).
     * These are the only very common operations on [self] for which
     * bytecoding is at all reasonable.
     */

    if (parsePtr->numWords == 1) {
	goto compileSelfObject;
    } else if (parsePtr->numWords == 2) {
	Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr), *subcmd;

	if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD || tokenPtr[1].size==0) {
	    return TCL_ERROR;
	}

	subcmd = tokenPtr + 1;
	if (strncmp(subcmd->start, "object", subcmd->size) == 0) {
	    goto compileSelfObject;
	} else if (strncmp(subcmd->start, "namespace", subcmd->size) == 0) {
	    goto compileSelfNamespace;
	}
    }

    /*
     * Can't compile; handle with runtime call.
     */

    return TCL_ERROR;

  compileSelfObject:

    /*
     * This delegates the entire problem to a single opcode.
     */

    OP(		TCLOO_SELF);
    return TCL_OK;

  compileSelfNamespace:

    /*
     * This is formally only correct with TclOO methods as they are currently
     * implemented; it assumes that the current namespace is invariably when a
     * TclOO context is present is the object's namespace, and that's
     * technically only something that's a matter of current policy. But it
     * avoids creating another opcode, so that's all good!
     */

    OP(		TCLOO_SELF);
    OP(		POP);
    OP(		NS_CURRENT);
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
