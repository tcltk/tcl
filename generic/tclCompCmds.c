/*
 * tclCompCmds.c --
 *
 *	This file contains compilation procedures that compile various Tcl
 *	commands into a sequence of instructions ("bytecodes").
 *
 * Copyright © 1997-1998 Sun Microsystems, Inc.
 * Copyright © 2001 Kevin B. Kenny.  All rights reserved.
 * Copyright © 2002 ActiveState Corporation.
 * Copyright © 2004-2013 Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclCompile.h"

/*
 * Prototypes for procedures defined later in this file:
 */

static AuxDataDupProc	DupDictUpdateInfo;
static AuxDataFreeProc	FreeDictUpdateInfo;
static AuxDataPrintProc	PrintDictUpdateInfo;
static AuxDataPrintProc	DisassembleDictUpdateInfo;
static AuxDataDupProc	DupForeachInfo;
static AuxDataFreeProc	FreeForeachInfo;
static AuxDataPrintProc	PrintForeachInfo;
static AuxDataPrintProc	DisassembleForeachInfo;
static AuxDataPrintProc	PrintNewForeachInfo;
static AuxDataPrintProc	DisassembleNewForeachInfo;
static int		CompileEachloopCmd(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, Command *cmdPtr,
			    CompileEnv *envPtr, int collect);
static int		CompileDictEachCmd(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, Command *cmdPtr,
			    CompileEnv *envPtr, int collect);
static inline void	IssueDictWithEmpty(Tcl_Interp *interp,
			    Tcl_Size numWords, Tcl_Token *varTokenPtr,
			    CompileEnv *envPtr);
static inline void	IssueDictWithBodied(Tcl_Interp *interp,
			    Tcl_Size numWords, Tcl_Token *varTokenPtr,
			    CompileEnv *envPtr);

/*
 * The structures below define the AuxData types defined in this file.
 */

static const AuxDataType foreachInfoType = {
    "ForeachInfo",		/* name */
    DupForeachInfo,		/* dupProc */
    FreeForeachInfo,		/* freeProc */
    PrintForeachInfo,		/* printProc */
    DisassembleForeachInfo	/* disassembleProc */
};

static const AuxDataType newForeachInfoType = {
    "NewForeachInfo",		/* name */
    DupForeachInfo,		/* dupProc */
    FreeForeachInfo,		/* freeProc */
    PrintNewForeachInfo,	/* printProc */
    DisassembleNewForeachInfo	/* disassembleProc */
};

static const AuxDataType dictUpdateInfoType = {
    "DictUpdateInfo",		/* name */
    DupDictUpdateInfo,		/* dupProc */
    FreeDictUpdateInfo,		/* freeProc */
    PrintDictUpdateInfo,	/* printProc */
    DisassembleDictUpdateInfo	/* disassembleProc */
};

/*
 *----------------------------------------------------------------------
 *
 * TclGetAuxDataType --
 *
 *	This procedure looks up an Auxdata type by name.
 *
 * Results:
 *	If an AuxData type with name matching "typeName" is found, a pointer
 *	to its AuxDataType structure is returned; otherwise, NULL is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const AuxDataType *
TclGetAuxDataType(
    const char *typeName)	/* Name of AuxData type to look up. */
{
    if (!strcmp(typeName, foreachInfoType.name)) {
	return &foreachInfoType;
    } else if (!strcmp(typeName, newForeachInfoType.name)) {
	return &newForeachInfoType;
    } else if (!strcmp(typeName, dictUpdateInfoType.name)) {
	return &dictUpdateInfoType;
    } else if (!strcmp(typeName, tclJumptableInfoType.name)) {
	return &tclJumptableInfoType;
    } else if (!strcmp(typeName, tclJumptableNumericInfoType.name)) {
	return &tclJumptableNumericInfoType;
    }
    return NULL;
}

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
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *valueTokenPtr;
    int isScalar;
    Tcl_LVTIndex localIndex;
    Tcl_Size i, numWords = parsePtr->numWords;

    /* TODO: Consider support for compiling expanded args. */
    if (numWords == 1 || numWords > UINT_MAX) {
	return TCL_ERROR;
    } else if (numWords == 2) {
	/*
	 * append varName == set varName
	 */

	return TclCompileSetCmd(interp, parsePtr, cmdPtr, envPtr);
    } else if (numWords > 3) {
	/*
	 * APPEND instructions currently only handle one value, but we can
	 * handle some multi-value cases by stringing them together.
	 */

	goto appendMultiple;
    }

    /*
     * Decide if we can use a frame slot for the var/array name or if we need
     * to emit code to compute and push the name at runtime. We use a frame
     * slot (entry in the array of local vars) if we are compiling a procedure
     * body and if the name is simple text that does not include namespace
     * qualifiers.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    PushVarNameWord(varTokenPtr, 0, &localIndex, &isScalar, 1);

    /*
     * We are doing an assignment, otherwise TclCompileSetCmd was called, so
     * push the new value. This will need to be extended to push a value for
     * each argument.
     */

    valueTokenPtr = TokenAfter(varTokenPtr);
    PUSH_TOKEN(			valueTokenPtr, 2);

    /*
     * Emit instructions to set/get the variable.
     */

    if (isScalar) {
	if (localIndex < 0) {
	    OP(			APPEND_STK);
	} else {
	    OP4(		APPEND_SCALAR, localIndex);
	}
    } else {
	if (localIndex < 0) {
	    OP(			APPEND_ARRAY_STK);
	} else {
	    OP4(		APPEND_ARRAY, localIndex);
	}
    }

    return TCL_OK;

  appendMultiple:
    /*
     * Can only handle the case where we are appending to a local scalar when
     * there are multiple values to append.  Fortunately, this is common.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);

    localIndex = LocalScalarFromToken(varTokenPtr, envPtr);
    if (localIndex < 0) {
	return TCL_ERROR;
    }

    /*
     * Definitely appending to a local scalar; generate the words and append
     * them.
     */

    valueTokenPtr = TokenAfter(varTokenPtr);
    for (i = 2 ; i < numWords ; i++) {
	PUSH_TOKEN(		valueTokenPtr, i);
	valueTokenPtr = TokenAfter(valueTokenPtr);
    }
    OP4(			REVERSE, numWords - 2);
    for (i = 2 ; i < numWords ;) {
	OP4(			APPEND_SCALAR, localIndex);
	if (++i < numWords) {
	    OP(			POP);
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileArray*Cmd --
 *
 *	Functions called to compile "array" subcommands.
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    int isScalar;
    Tcl_LVTIndex localIndex;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PushVarNameWord(tokenPtr, TCL_NO_ELEMENT, &localIndex, &isScalar, 1);
    if (!isScalar || localIndex > UINT_MAX) {
	return TCL_ERROR;
    }

    if (localIndex >= 0) {
	OP4(			ARRAY_EXISTS_IMM, localIndex);
    } else {
	OP(			ARRAY_EXISTS_STK);
    }
    return TCL_OK;
}

int
TclCompileArraySetCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *dataTokenPtr;
    int isScalar, code = TCL_OK, isDataLiteral, isDataValid, isDataEven;
    Tcl_Size len;
    Tcl_LVTIndex keyVar, valVar, localIndex;
    Tcl_AuxDataRef infoIndex;
    Tcl_Obj *literalObj;
    ForeachInfo *infoPtr;
    Tcl_BytecodeLabel arrayMade, offsetBack;

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    dataTokenPtr = TokenAfter(varTokenPtr);
    TclNewObj(literalObj);
    isDataLiteral = TclWordKnownAtCompileTime(dataTokenPtr, literalObj);
    isDataValid = (isDataLiteral
	    && TclListObjLength(NULL, literalObj, &len) == TCL_OK);
    isDataEven = (isDataValid && (len & 1) == 0);

    /*
     * Special case: literal odd-length argument is always an error.
     */

    if (isDataValid && !isDataEven) {
	/* Abandon custom compile and let invocation raise the error */
	code = TclCompileBasic2ArgCmd(interp, parsePtr, cmdPtr, envPtr);
	goto done;

	/*
	 * We used to compile to the bytecode that would throw the error,
	 * but that was wrong because it would not invoke the array trace
	 * on the variable.
	 *
	PUSH(			"list must have an even number of elements");
	PUSH(			"-errorcode {TCL ARGUMENT FORMAT}");
	OP44(			RETURN_IMM, TCL_ERROR, 0);
	goto done;
	 *
	 */
    }

    /*
     * Except for the special "ensure array" case below, when we're not in
     * a proc, we cannot do a better compile than generic.
     */

    if ((varTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) ||
	    (!EnvIsProc(envPtr) && !(isDataEven && len == 0))) {
	code = TclCompileBasic2ArgCmd(interp, parsePtr, cmdPtr, envPtr);
	goto done;
    }

    PushVarNameWord(varTokenPtr, TCL_NO_ELEMENT, &localIndex, &isScalar, 1);
    if (!isScalar || localIndex > UINT_MAX) {
	code = TCL_ERROR;
	goto done;
    }

    /*
     * Special case: literal empty value argument is just an "ensure array"
     * operation.
     */

    if (isDataEven && len == 0) {
	if (localIndex >= 0) {
	    Tcl_BytecodeLabel haveArray;
	    OP4(		ARRAY_EXISTS_IMM, localIndex);
	    FWDJUMP(		JUMP_TRUE, haveArray);
	    OP4(		ARRAY_MAKE_IMM, localIndex);
	    FWDLABEL(	haveArray);
	} else {
	    Tcl_BytecodeLabel haveArray;
	    OP(			DUP);
	    OP(			ARRAY_EXISTS_STK);
	    FWDJUMP(		JUMP_TRUE, haveArray);
	    OP(			ARRAY_MAKE_STK);
	    FWDJUMP(		JUMP, arrayMade);

	    /* Each branch decrements stack depth, but we only take one. */
	    STKDELTA(+1);
	    FWDLABEL(	haveArray);
	    OP(			POP);
	    FWDLABEL(	arrayMade);
	}
	PUSH(			"");
	goto done;
    }

    if (localIndex < 0) {
	/*
	 * a non-local variable: upvar from a local one! This consumes the
	 * variable name that was left at stacktop.
	 */

	localIndex = TclFindCompiledLocal(varTokenPtr->start,
		varTokenPtr->size, 1, envPtr);
	PUSH(			"0");
	OP(			SWAP);
	OP4(			UPVAR, localIndex);
	OP(			POP);
    }

    /*
     * Prepare for the internal foreach.
     */

    keyVar = AnonymousLocal(envPtr);
    valVar = AnonymousLocal(envPtr);

    infoPtr = (ForeachInfo *)Tcl_Alloc(
	    offsetof(ForeachInfo, varLists) + sizeof(ForeachVarList *));
    infoPtr->numLists = 1;
    infoPtr->varLists[0] = (ForeachVarList *)Tcl_Alloc(
	    offsetof(ForeachVarList, varIndexes) + 2 * sizeof(Tcl_Size));
    infoPtr->varLists[0]->numVars = 2;
    infoPtr->varLists[0]->varIndexes[0] = keyVar;
    infoPtr->varLists[0]->varIndexes[1] = valVar;
    infoIndex = TclCreateAuxData(infoPtr, &newForeachInfoType, envPtr);

    /*
     * Start issuing instructions to write to the array.
     */

    OP4(			ARRAY_EXISTS_IMM, localIndex);
    FWDJUMP(			JUMP_TRUE, arrayMade);
    OP4(			ARRAY_MAKE_IMM, localIndex);
    FWDLABEL(		arrayMade);

    PUSH_TOKEN(			dataTokenPtr, 2);
    if (!isDataLiteral || !isDataValid) {
	/*
	 * Only need this safety check if we're handling a non-literal or list
	 * containing an invalid literal; with valid list literals, we've
	 * already checked (worth it because literals are a very common
	 * use-case with [array set]).
	 */

	Tcl_BytecodeLabel ok;
	OP(			DUP);
	OP(			LIST_LENGTH);
	PUSH(			"1");
	OP(			BITAND);
	FWDJUMP(		JUMP_FALSE, ok);
	PUSH(			"list must have an even number of elements");
	PUSH(			"-errorcode {TCL ARGUMENT FORMAT}");
	OP44(			RETURN_IMM, TCL_ERROR, 0);
	STKDELTA(-1);
	FWDLABEL(	ok);
    }

    OP4(			FOREACH_START, infoIndex);
    BACKLABEL(		offsetBack);
    OP4(			LOAD_SCALAR, keyVar);
    OP4(			LOAD_SCALAR, valVar);
    OP4(			STORE_ARRAY, localIndex);
    OP(				POP);
    infoPtr->loopCtTemp = offsetBack - CurrentOffset(envPtr); /*misuse */
    OP(			FOREACH_STEP);
    OP(			FOREACH_END);
    STKDELTA(-3);
    PUSH(			"");

  done:
    Tcl_DecrRefCount(literalObj);
    return code;
}

int
TclCompileArrayUnsetCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);
    int isScalar;
    Tcl_LVTIndex localIndex;
    Tcl_BytecodeLabel noSuchArray, end;

    if (parsePtr->numWords != 2) {
	return TclCompileBasic2ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    PushVarNameWord(tokenPtr, TCL_NO_ELEMENT, &localIndex, &isScalar, 1);
    if (!isScalar) {
	return TCL_ERROR;
    }

    if (localIndex >= 0) {
	OP4(			ARRAY_EXISTS_IMM, localIndex);
	FWDJUMP(		JUMP_FALSE, end);
	OP14(			UNSET_SCALAR, 1, localIndex);
    } else {
	OP(			DUP);
	OP(			ARRAY_EXISTS_STK);
	FWDJUMP(		JUMP_FALSE, noSuchArray);
	OP1(			UNSET_STK, 1);
	FWDJUMP(		JUMP, end);

	/* Each branch decrements stack depth, but we only take one. */
	STKDELTA(+1);
	FWDLABEL(	noSuchArray);
	OP(			POP);
    }
    FWDLABEL(		end);
    PUSH(			"");
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
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    ExceptionRange *rangePtr;
    ExceptionAux *auxPtr;

    if (parsePtr->numWords != 1) {
	return TCL_ERROR;
    }

    /*
     * Find the innermost exception range that contains this command.
     */

    rangePtr = TclGetInnermostExceptionRange(envPtr, TCL_BREAK, &auxPtr);
    if (rangePtr && rangePtr->type == LOOP_EXCEPTION_RANGE) {
	/*
	 * Found the target! No need for a nasty INST_BREAK here.
	 */

	TclCleanupStackForBreakContinue(envPtr, auxPtr);
	TclAddLoopBreakFixup(envPtr, auxPtr);
    } else {
	/*
	 * Emit a real break.
	 */

	OP(			BREAK);
    }
    STKDELTA(+1);

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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *cmdTokenPtr, *resultNameTokenPtr, *optsNameTokenPtr;
    int dropScript = 0;
    Tcl_LVTIndex resultIndex, optsIndex;
    Tcl_BytecodeLabel haveResultAndCode;
    Tcl_ExceptionRange range;
    Tcl_Size depth = TclGetStackDepth(envPtr), numWords = parsePtr->numWords;

    /*
     * If syntax does not match what we expect for [catch], do not compile.
     * Let runtime checks determine if syntax has changed.
     */

    if ((numWords < 2) || (numWords > 4)) {
	return TCL_ERROR;
    }

    /*
     * If variables were specified and the catch command is at global level
     * (not in a procedure), don't compile it inline: the payoff is too small.
     */

    if ((numWords >= 3) && !EnvHasLVT(envPtr)) {
	return TCL_ERROR;
    }

    /*
     * Make sure the variable names, if any, have no substitutions and just
     * refer to local scalars.
     */

    resultIndex = optsIndex = TCL_INDEX_NONE;
    cmdTokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (numWords >= 3) {
	resultNameTokenPtr = TokenAfter(cmdTokenPtr);
	/* DGP */
	resultIndex = LocalScalarFromToken(resultNameTokenPtr, envPtr);
	if (resultIndex < 0) {
	    return TCL_ERROR;
	}

	/* DKF */
	if (numWords == 4) {
	    optsNameTokenPtr = TokenAfter(resultNameTokenPtr);
	    optsIndex = LocalScalarFromToken(optsNameTokenPtr, envPtr);
	    if (optsIndex < 0) {
		return TCL_ERROR;
	    }
	}
    }

    /*
     * We will compile the catch command. Declare the exception range that it
     * uses.
     *
     * If the body is a simple word, compile a BEGIN_CATCH instruction,
     * followed by the instructions to eval the body.
     * Otherwise, compile instructions to substitute the body text before
     * starting the catch, then BEGIN_CATCH, and then EVAL_STK to evaluate the
     * substituted body.
     * Care has to be taken to make sure that substitution happens outside the
     * catch range so that errors in the substitution are not caught.
     * [Bug 219184]
     * The reason for duplicating the script is that EVAL_STK would otherwise
     * begin by underflowing the stack below the mark set by BEGIN_CATCH4.
     */

    range = MAKE_CATCH_RANGE();
    if (cmdTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	OP4(			BEGIN_CATCH, range);
	CATCH_RANGE(range) {
	    BODY(		cmdTokenPtr, 1);
	}
    } else {
	PUSH_TOKEN(		cmdTokenPtr, 1);
	OP4(			BEGIN_CATCH, range);
	OP(			DUP);
	CATCH_RANGE(range) {
	    INVOKE(		EVAL_STK);
	}
	/* drop the script */
	dropScript = 1;
	OP(			SWAP);
	OP(			POP);
    }

    /*
     * Emit the "no errors" epilogue: push "0" (TCL_OK) as the catch result,
     * and jump around the "error case" code.
     */

    TclCheckStackDepth(depth+1, envPtr);
    PUSH(			"0");
    OP(				SWAP);
    FWDJUMP(			JUMP, haveResultAndCode);

    /*
     * Emit the "error case" epilogue. Push the interpreter result and the
     * return code.
     */

    CATCH_TARGET(	range);
    TclSetStackDepth(depth + dropScript, envPtr);

    if (dropScript) {
	OP(			POP);
    }

    /* Stack at this point is empty */
    OP(				PUSH_RETURN_CODE);
    OP(				PUSH_RESULT);

    /* Stack at this point on both branches: returnCode result */

    FWDLABEL(		haveResultAndCode);

    /*
     * Push the return options if the caller wants them. This needs to happen
     * before INST_END_CATCH
     */

    if (optsIndex != TCL_INDEX_NONE) {
	OP(			PUSH_RETURN_OPTIONS);
    }

    /*
     * End the catch
     */

    OP(				END_CATCH);

    /*
     * Save the result and return options if the caller wants them. This needs
     * to happen after INST_END_CATCH (compile-3.6/7).
     */

    if (optsIndex != TCL_INDEX_NONE) {
	OP4(			STORE_SCALAR, optsIndex);
	OP(			POP);
    }
    if (resultIndex != TCL_INDEX_NONE) {
	OP4(			STORE_SCALAR, resultIndex);
    }
    OP(				POP);

    TclCheckStackDepth(depth+1, envPtr);
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * TclCompileClockClicksCmd --
 *
 *	Procedure called to compile the "tcl::clock::clicks" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to run time.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "clock clicks"
 *	command at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileClockClicksCmd(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token* tokenPtr;

    switch (parsePtr->numWords) {
    case 1:
	/*
	 * No args
	 */
	OP1(			CLOCK_READ, CLOCK_READ_CLICKS);
	break;
    case 2:
	/*
	 * -milliseconds or -microseconds
	 */
	tokenPtr = TokenAfter(parsePtr->tokenPtr);
	if (IS_TOKEN_PREFIX(tokenPtr, 3, "-microseconds")) {
	    OP1(		CLOCK_READ, CLOCK_READ_MICROS);
	} else if (IS_TOKEN_PREFIX(tokenPtr, 3, "-milliseconds")) {
	    OP1(		CLOCK_READ, CLOCK_READ_MILLIS);
	} else {
	    return TCL_ERROR;
	}
	break;
    default:
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * TclCompileClockReadingCmd --
 *
 *	Procedure called to compile the "tcl::clock::microseconds",
 *	"tcl::clock::milliseconds" and "tcl::clock::seconds" commands.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to run time.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "clock clicks"
 *	command at runtime.
 *
 * Client data is 1 for microseconds, 2 for milliseconds, 3 for seconds.
 *----------------------------------------------------------------------
 */

int
TclCompileClockReadingCmd(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    if (parsePtr->numWords != 1) {
	return TCL_ERROR;
    }

    OP1(			CLOCK_READ, PTR2INT(cmdPtr->objClientData));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileConcatCmd --
 *
 *	Procedure called to compile the "concat" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "concat" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileConcatCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Obj *objPtr, *listObj, **objs;
    Tcl_Size len, i, numWords = parsePtr->numWords;
    Tcl_Token *tokenPtr;

    /* TODO: Consider compiling expansion case. */
    if (numWords == 1) {
	/*
	 * [concat] without arguments just pushes an empty object.
	 */

	PUSH(			"");
	return TCL_OK;
    } else if (numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * Test if all arguments are compile-time known. If they are, we can
     * implement with a simple push of a literal.
     */

    TclNewObj(listObj);
    for (i = 1, tokenPtr = parsePtr->tokenPtr; i < numWords; i++) {
	tokenPtr = TokenAfter(tokenPtr);
	TclNewObj(objPtr);
	if (!TclWordKnownAtCompileTime(tokenPtr, objPtr)) {
	    Tcl_BounceRefCount(objPtr);
	    Tcl_BounceRefCount(listObj);
	    goto runtimeConcat;
	}
	(void) Tcl_ListObjAppendElement(NULL, listObj, objPtr);
    }

    TclListObjGetElements(NULL, listObj, &len, &objs);
    PUSH_OBJ(			Tcl_ConcatObj(len, objs));
    Tcl_BounceRefCount(listObj);
    return TCL_OK;

    /*
     * General case: do the concatenation at runtime.
     */

  runtimeConcat:
    for (i = 1, tokenPtr = parsePtr->tokenPtr; i < numWords; i++) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, i);
    }
    OP4(			CONCAT_STK, i - 1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileConstCmd --
 *
 *	Procedure called to compile the "const" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "const" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileConstCmd(
    Tcl_Interp *interp,		/* The interpreter. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *valueTokenPtr;
    int isScalar;
    Tcl_LVTIndex localIndex;

    /*
     * Need exactly two arguments.
     */
    if (parsePtr->numWords != 3) {
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
    PushVarNameWord(varTokenPtr, 0, &localIndex, &isScalar, 1);

    /*
     * If the user specified an array element, we don't bother handling
     * that.
     */
    if (!isScalar || localIndex > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * We are doing an assignment to set the value of the constant. This will
     * need to be extended to push a value for each argument.
     */

    valueTokenPtr = TokenAfter(varTokenPtr);
    PUSH_TOKEN(			valueTokenPtr, 2);

    if (localIndex < 0) {
	OP(			CONST_STK);
    } else {
	OP4(			CONST_IMM, localIndex);
    }

    /*
     * The const command's result is an empty string.
     */
    PUSH(			"");
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
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    ExceptionRange *rangePtr;
    ExceptionAux *auxPtr;

    /*
     * There should be no argument after the "continue".
     */

    if (parsePtr->numWords != 1) {
	return TCL_ERROR;
    }

    /*
     * See if we can find a valid continueOffset (i.e., not -1) in the
     * innermost containing exception range.
     */

    rangePtr = TclGetInnermostExceptionRange(envPtr, TCL_CONTINUE, &auxPtr);
    if (rangePtr && rangePtr->type == LOOP_EXCEPTION_RANGE) {
	/*
	 * Found the target! No need for a nasty INST_CONTINUE here.
	 */

	TclCleanupStackForBreakContinue(envPtr, auxPtr);
	TclAddLoopContinueFixup(envPtr, auxPtr);
    } else {
	/*
	 * Emit a real continue.
	 */

	OP(			CONTINUE);
    }
    STKDELTA(+1);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileDict*Cmd --
 *
 *	Functions called to compile "dict" subcommands.
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;
    Tcl_LVTIndex dictVarIndex;
    Tcl_Token *varTokenPtr;
    /* TODO: Consider support for compiling expanded args. */

    /*
     * There must be at least one argument after the command.
     */

    if (numWords < 4 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * The dictionary variable must be a local scalar that is knowable at
     * compile time; anything else exceeds the complexity of the opcode. So
     * discover what the index is.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    dictVarIndex = LocalScalarFromToken(varTokenPtr, envPtr);
    if (dictVarIndex < 0) {
	return TCL_ERROR;
    }

    /*
     * Remaining words (key path and value to set) can be handled normally.
     */

    tokenPtr = TokenAfter(varTokenPtr);
    for (i=2 ; i<numWords ; i++) {
	PUSH_TOKEN(		tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }

    /*
     * Now emit the instruction to do the dict manipulation.
     */

    OP44(			DICT_SET, numWords - 3, dictVarIndex);
    return TCL_OK;
}

int
TclCompileDictIncrCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *keyTokenPtr;
    Tcl_LVTIndex dictVarIndex;
    int incrAmount;

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
	Tcl_Token *incrTokenPtr = TokenAfter(keyTokenPtr);
	Tcl_Obj *intObj;
	int code;

	TclNewObj(intObj);
	if (!TclWordKnownAtCompileTime(incrTokenPtr, intObj)) {
	    Tcl_BounceRefCount(intObj);
	    return TclCompileBasic2Or3ArgCmd(interp, parsePtr,cmdPtr, envPtr);
	}
	code = TclGetIntFromObj(NULL, intObj, &incrAmount);
	Tcl_BounceRefCount(intObj);
	if (code != TCL_OK) {
	    return TclCompileBasic2Or3ArgCmd(interp, parsePtr,cmdPtr, envPtr);
	}
    } else {
	incrAmount = 1;
    }

    /*
     * The dictionary variable must be a local scalar that is knowable at
     * compile time; anything else exceeds the complexity of the opcode. So
     * discover what the index is.
     */

    dictVarIndex = LocalScalarFromToken(varTokenPtr, envPtr);
    if (dictVarIndex < 0) {
	return TclCompileBasic2Or3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    /*
     * Emit the key and the code to actually do the increment.
     */

    PUSH_TOKEN(			keyTokenPtr, 2);
    OP44(			DICT_INCR_IMM, incrAmount, dictVarIndex);
    return TCL_OK;
}

int
TclCompileDictGetCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;

    /*
     * There must be at least two arguments after the command (the single-arg
     * case is legal, but too special and magic for us to deal with here).
     */

    /* TODO: Consider support for compiling expanded args. */
    if (numWords < 3 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);

    /*
     * Only compile this because we need INST_DICT_GET anyway.
     */

    for (i=1 ; i<numWords ; i++) {
	PUSH_TOKEN(		tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }
    OP4(			DICT_GET, numWords - 2);
    return TCL_OK;
}

int
TclCompileDictGetWithDefaultCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;

    /*
     * There must be at least three arguments after the command.
     */

    /* TODO: Consider support for compiling expanded args. */
    if (numWords < 4 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);

    for (i=1 ; i<numWords ; i++) {
	PUSH_TOKEN(		tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }
    OP4(			DICT_GET_DEF, numWords - 3);
    return TCL_OK;
}

int
TclCompileDictExistsCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;

    /*
     * There must be at least two arguments after the command (the single-arg
     * case is legal, but too special and magic for us to deal with here).
     */

    /* TODO: Consider support for compiling expanded args. */
    if (numWords < 3 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);

    /*
     * Now we do the code generation.
     */

    for (i=1 ; i<numWords ; i++) {
	PUSH_TOKEN(		tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }
    OP4(			DICT_EXISTS, numWords - 2);
    return TCL_OK;
}

int
TclCompileDictReplaceCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Size i, numWords = parsePtr->numWords;
    Tcl_Token *tokenPtr;
    /* TODO: Consider support for compiling expanded args. */

    /*
     * Don't compile [dict replace $dict]; it's an edge case.
     */
    if (numWords <= 3 || numWords > UINT_MAX || (numWords % 1)) {
	return TCL_ERROR;
    }

    // Push starting dictionary
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);

    // Push the keys and values, and add them to the dictionary
    for (i=2; i<numWords; i+=2) {
	// Push key
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, i);
	// Push value
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, i + 1);
	OP(			DICT_PUT);
    }

    return TCL_OK;
}

int
TclCompileDictRemoveCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Size i, numWords = parsePtr->numWords;
    Tcl_Token *tokenPtr;
    /* TODO: Consider support for compiling expanded args. */

    /*
     * Don't compile [dict remove $dict]; it's an edge case.
     */
    if (numWords <= 2 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    // Push starting dictionary
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);

    // Push the keys, and remove them from the dictionary
    for (i=2; i<numWords; i++) {
	// Push key
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, i);
	OP(			DICT_REMOVE);
    }

    return TCL_OK;
}

int
TclCompileDictUnsetCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;
    Tcl_LVTIndex dictVarIndex;

    /*
     * There must be at least one argument after the variable name for us to
     * compile to bytecode.
     */

    /* TODO: Consider support for compiling expanded args. */
    if (numWords < 3 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * The dictionary variable must be a local scalar that is knowable at
     * compile time; anything else exceeds the complexity of the opcode. So
     * discover what the index is.
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    dictVarIndex = LocalScalarFromToken(tokenPtr, envPtr);
    if (dictVarIndex < 0) {
	return TclCompileBasicMin2ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    /*
     * Remaining words (the key path) can be handled normally.
     */

    for (i=2 ; i<numWords ; i++) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, i);
    }

    /*
     * Now emit the instruction to do the dict manipulation.
     */

    OP44(			DICT_UNSET, numWords - 2, dictVarIndex);
    return TCL_OK;
}

int
TclCompileDictCreateCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *keyToken, *valueToken;
    Tcl_Obj *keyObj, *valueObj, *dictObj;
    Tcl_Size i, numWords = parsePtr->numWords;
    /* TODO: Consider support for compiling expanded args. */

    if ((numWords & 1) == 0 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * See if we can build the value at compile time...
     */

    keyToken = TokenAfter(parsePtr->tokenPtr);
    TclNewObj(dictObj);
    for (i=1 ; i<numWords ; i+=2) {
	TclNewObj(keyObj);
	if (!TclWordKnownAtCompileTime(keyToken, keyObj)) {
	    Tcl_BounceRefCount(keyObj);
	    Tcl_BounceRefCount(dictObj);
	    goto nonConstant;
	}
	valueToken = TokenAfter(keyToken);
	TclNewObj(valueObj);
	if (!TclWordKnownAtCompileTime(valueToken, valueObj)) {
	    Tcl_BounceRefCount(keyObj);
	    Tcl_BounceRefCount(valueObj);
	    Tcl_BounceRefCount(dictObj);
	    goto nonConstant;
	}
	keyToken = TokenAfter(valueToken);
	Tcl_DictObjPut(NULL, dictObj, keyObj, valueObj);
	Tcl_BounceRefCount(keyObj);
    }

    /*
     * We did! Excellent. The "verifyDict" is to do type forcing.
     */

    PUSH_OBJ(			dictObj);
    OP(				DUP);
    OP(				DICT_VERIFY);
    return TCL_OK;

    /*
     * Otherwise, we've got to issue runtime code to do the building, which we
     * do by [dict set]ting into an unnamed local variable. This requires that
     * we are in a context with an LVT.
     */

  nonConstant:
    PUSH(			"");
    keyToken = TokenAfter(parsePtr->tokenPtr);
    for (i=1 ; i<numWords ; i+=2) {
	valueToken = TokenAfter(keyToken);
	PUSH_TOKEN(		keyToken, i);
	PUSH_TOKEN(		valueToken, i + 1);
	OP(			DICT_PUT);
	keyToken = TokenAfter(valueToken);
    }
    return TCL_OK;
}

int
TclCompileDictMergeCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;
    Tcl_LVTIndex infoIndex;
    Tcl_ExceptionRange outLoop;
    Tcl_BytecodeLabel end;

    /*
     * Deal with some special edge cases. Note that in the case with one
     * argument, the only thing to do is to verify the dict-ness.
     */

    /* TODO: Consider support for compiling expanded args. (less likely) */
    if (numWords < 2) {
	PUSH(			"");
	return TCL_OK;
    } else if (numWords == 2) {
	tokenPtr = TokenAfter(parsePtr->tokenPtr);
	PUSH_TOKEN(		tokenPtr, 1);
	OP(			DUP);
	OP(			DICT_VERIFY);
	return TCL_OK;
    }

    /*
     * There's real merging work to do.
     *
     * Allocate some working space. This means we'll only ever compile this
     * command when there's an LVT present.
     */

    if (!EnvIsProc(envPtr)) {
	return TclCompileBasicMin2ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }
    infoIndex = AnonymousLocal(envPtr);

    /*
     * Get the first dictionary and verify that it is so.
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);
    OP(				DUP);
    OP(				DICT_VERIFY);

    /*
     * For each of the remaining dictionaries...
     */

    outLoop = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, outLoop);
    CATCH_RANGE(outLoop) {
	for (i=2 ; i<numWords ; i++) {
	    Tcl_BytecodeLabel haveNext, noNext;
	    /*
	     * Get the dictionary, and merge its pairs into the first dict (using
	     * a small loop).
	     */

	    tokenPtr = TokenAfter(tokenPtr);
	    PUSH_TOKEN(		tokenPtr, i);
	    OP4(		DICT_FIRST, infoIndex);
	    FWDJUMP(		JUMP_TRUE, noNext);
	    BACKLABEL(	haveNext);
	    OP(			SWAP);
	    OP(			DICT_PUT);
	    OP4(		DICT_NEXT, infoIndex);
	    BACKJUMP(		JUMP_FALSE, haveNext);
	    FWDLABEL(	noNext);
	    OP(			POP);
	    OP(			POP);
	    OP14(		UNSET_SCALAR, 0, infoIndex);
	}
    }
    OP(				END_CATCH);


    /*
     * Clean up any state left over.
     */

    FWDJUMP(			JUMP, end);
    STKDELTA(-1);

    /*
     * If an exception happens when starting to iterate over the second (and
     * subsequent) dicts. This is strictly not necessary, but it is nice.
     */

    CATCH_TARGET(outLoop);
    OP(				PUSH_RETURN_OPTIONS);
    OP(				PUSH_RESULT);
    OP(				END_CATCH);
    OP14(			UNSET_SCALAR, 0, infoIndex);
    INVOKE(			RETURN_STK);
    FWDLABEL(		end);
    return TCL_OK;
}

int
TclCompileDictForCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
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
    Command *cmdPtr,		/* Points to definition of command being
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
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr,		/* Holds resulting instructions. */
    int collect)		/* Flag == TCL_EACH_COLLECT to collect and
				 * construct a new dictionary with the loop
				 * body result. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varsTokenPtr, *dictTokenPtr, *bodyTokenPtr;
    Tcl_LVTIndex keyVarIndex, valueVarIndex, infoIndex;
    Tcl_LVTIndex collectVar = TCL_INDEX_NONE;
    Tcl_Size nameChars, numVars;
    Tcl_ExceptionRange loopRange, catchRange;
    Tcl_BytecodeLabel bodyTarget, emptyTarget, endTarget;
    const char **argv;
    Tcl_DString buffer;

    /*
     * There must be three arguments after the command.
     */

    if (parsePtr->numWords != 4) {
	return TclCompileBasic3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }
    if (!EnvIsProc(envPtr)) {
	return TclCompileBasic3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    varsTokenPtr = TokenAfter(parsePtr->tokenPtr);
    dictTokenPtr = TokenAfter(varsTokenPtr);
    bodyTokenPtr = TokenAfter(dictTokenPtr);
    if (varsTokenPtr->type != TCL_TOKEN_SIMPLE_WORD ||
	    bodyTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TclCompileBasic3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    /*
     * Create temporary variable to capture return values from loop body when
     * we're collecting results.
     */

    if (collect == TCL_EACH_COLLECT) {
	collectVar = AnonymousLocal(envPtr);
	if (collectVar < 0) {
	    return TclCompileBasic3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
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
	return TclCompileBasic3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }
    Tcl_DStringFree(&buffer);
    if (numVars != 2) {
	Tcl_Free((void *)argv);
	return TclCompileBasic3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    nameChars = strlen(argv[0]);
    keyVarIndex = LocalScalar(argv[0], nameChars, envPtr);
    nameChars = strlen(argv[1]);
    valueVarIndex = LocalScalar(argv[1], nameChars, envPtr);
    Tcl_Free((void *)argv);

    if ((keyVarIndex < 0) || (valueVarIndex < 0)) {
	return TclCompileBasic3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    /*
     * Allocate a temporary variable to store the iterator reference. The
     * variable will contain a Tcl_DictSearch reference which will be
     * allocated by INST_DICT_FIRST and disposed when the variable is unset
     * (at which point it should also have been finished with).
     */

    infoIndex = AnonymousLocal(envPtr);
    if (infoIndex < 0) {
	return TclCompileBasic3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    /*
     * Preparation complete; issue instructions. Note that this code issues
     * fixed-sized jumps. That simplifies things a lot!
     *
     * First up, initialize the accumulator dictionary if needed.
     */

    if (collect == TCL_EACH_COLLECT) {
	PUSH(			"");
	OP4(			STORE_SCALAR, collectVar);
	OP(			POP);
    }

    /*
     * Get the dictionary and start the iteration. No catching of errors at
     * this point.
     */

    PUSH_TOKEN(			dictTokenPtr, 2);

    /*
     * Now we catch errors from here on so that we can finalize the search
     * started by Tcl_DictObjFirst above.
     */

    catchRange = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, catchRange);
    CATCH_RANGE(catchRange) {
	OP4(			DICT_FIRST, infoIndex);
	FWDJUMP(		JUMP_TRUE, emptyTarget);

	/*
	 * Inside the iteration, write the loop variables.
	 */

	BACKLABEL(	bodyTarget);
	OP4(			STORE_SCALAR, keyVarIndex);
	OP(			POP);
	OP4(			STORE_SCALAR, valueVarIndex);
	OP(			POP);

	/*
	 * Set up the loop exception targets.
	 */

	loopRange = MAKE_LOOP_RANGE();

	/*
	 * Compile the loop body itself. It should be stack-neutral.
	 */

	CATCH_RANGE(loopRange) {
	    BODY(		bodyTokenPtr, 3);
	    if (collect == TCL_EACH_COLLECT) {
		OP4(		LOAD_SCALAR, keyVarIndex);
		OP(		SWAP);
		OP44(		DICT_SET, 1, collectVar);
	    }
	    OP(			POP);
	}
    }

    /*
     * Continue (or just normally process) by getting the next pair of items
     * from the dictionary and jumping back to the code to write them into
     * variables if there is another pair.
     */

    CONTINUE_TARGET(	loopRange);
    OP4(			DICT_NEXT, infoIndex);
    BACKJUMP(			JUMP_FALSE, bodyTarget);
    FWDJUMP(			JUMP, endTarget);
    STKDELTA(-1);

    /*
     * Error handler "finally" clause, which force-terminates the iteration
     * and re-throws the error.
     */

    CATCH_TARGET(	catchRange);
    OP(				PUSH_RETURN_OPTIONS);
    OP(				PUSH_RESULT);
    OP(				END_CATCH);
    OP14(			UNSET_SCALAR, 0, infoIndex);
    if (collect == TCL_EACH_COLLECT) {
	OP14(			UNSET_SCALAR, 0, collectVar);
    }
    INVOKE(			RETURN_STK);

    /*
     * Otherwise we're done (the jump after the DICT_FIRST points here) and we
     * need to pop the bogus key/value pair (pushed to keep stack calculations
     * easy!) Note that we skip the END_CATCH. [Bug 1382528]
     */

    FWDLABEL(		emptyTarget);
    FWDLABEL(		endTarget);
    OP(				POP);
    OP(				POP);
    BREAK_TARGET(	loopRange);
    FINALIZE_LOOP(loopRange);
    OP(				END_CATCH);

    /*
     * Final stage of the command (normal case) is that we push an empty
     * object (or push the accumulator as the result object). This is done
     * last to promote peephole optimization when it's dropped immediately.
     */

    OP14(			UNSET_SCALAR, 0, infoIndex);
    if (collect == TCL_EACH_COLLECT) {
	OP4(			LOAD_SCALAR, collectVar);
	OP14(			UNSET_SCALAR, 0, collectVar);
    } else {
	PUSH(			"");
    }
    return TCL_OK;
}

int
TclCompileDictUpdateCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Size i, numVars, numWords = parsePtr->numWords;
    Tcl_AuxDataRef infoIndex;
    Tcl_LVTIndex dictIndex;
    Tcl_ExceptionRange range;
    Tcl_BytecodeLabel done;
    Tcl_Token **keyTokenPtrs, *dictVarTokenPtr, *bodyTokenPtr, *tokenPtr;
    DictUpdateInfo *duiPtr;

    /*
     * There must be at least one argument after the command.
     */

    if (numWords < 5 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * Parse the command. Expect the following:
     *   dict update <lit(eral)> <any> <lit> ?<any> <lit> ...? <lit>
     */

    if ((numWords - 1) & 1) {
	return TCL_ERROR;
    }
    numVars = (numWords - 3) / 2;

    /*
     * The dictionary variable must be a local scalar that is knowable at
     * compile time; anything else exceeds the complexity of the opcode. So
     * discover what the index is.
     */

    dictVarTokenPtr = TokenAfter(parsePtr->tokenPtr);
    dictIndex = LocalScalarFromToken(dictVarTokenPtr, envPtr);
    if (dictIndex < 0) {
	goto issueFallback;
    }

    /*
     * Assemble the instruction metadata. This is complex enough that it is
     * represented as auxData; it holds an ordered list of variable indices
     * that are to be used.
     */

    duiPtr = (DictUpdateInfo *)Tcl_Alloc(
	    offsetof(DictUpdateInfo, varIndices) + sizeof(size_t) * numVars);
    duiPtr->length = numVars;
    keyTokenPtrs = (Tcl_Token **)TclStackAlloc(interp,
	    sizeof(Tcl_Token *) * numVars);
    tokenPtr = TokenAfter(dictVarTokenPtr);

    for (i=0 ; i<numVars ; i++) {
	/*
	 * Put keys to one side for later compilation to bytecode.
	 */

	keyTokenPtrs[i] = tokenPtr;
	tokenPtr = TokenAfter(tokenPtr);

	/*
	 * Stash the index in the auxiliary data (if it is indeed a local
	 * scalar that is resolvable at compile-time).
	 */

	duiPtr->varIndices[i] = LocalScalarFromToken(tokenPtr, envPtr);
	if (duiPtr->varIndices[i] == TCL_INDEX_NONE) {
	    goto failedUpdateInfoAssembly;
	}
	tokenPtr = TokenAfter(tokenPtr);
    }
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	goto failedUpdateInfoAssembly;
    }
    bodyTokenPtr = tokenPtr;

    /*
     * The list of variables to bind is stored in auxiliary data so that it
     * can't be snagged by literal sharing and forced to shimmer dangerously.
     */

    infoIndex = TclCreateAuxData(duiPtr, &dictUpdateInfoType, envPtr);

    for (i=0 ; i<numVars ; i++) {
	PUSH_TOKEN(		keyTokenPtrs[i], 2*i + 2);
    }
    OP4(			LIST, numVars);
    OP44(			DICT_UPDATE_START, dictIndex, infoIndex);

    range = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, range);
    CATCH_RANGE(range) {
	BODY(			bodyTokenPtr, numWords - 1);
    }

    /*
     * Normal termination code: the stack has the key list below the result of
     * the body evaluation: swap them and finish the update code.
     */

    OP(				END_CATCH);
    OP(				SWAP);
    OP44(			DICT_UPDATE_END, dictIndex, infoIndex);

    /*
     * Jump around the exceptional termination code.
     */

    FWDJUMP(			JUMP, done);

    /*
     * Termination code for non-ok returns: stash the result and return
     * options in the stack, bring up the key list, finish the update code,
     * and finally return with the caught return data
     */

    CATCH_TARGET(	range);
    OP(				PUSH_RESULT);
    OP(				PUSH_RETURN_OPTIONS);
    OP(				END_CATCH);
    OP4(			REVERSE, 3);

    OP44(			DICT_UPDATE_END, dictIndex, infoIndex);
    INVOKE(			RETURN_STK);
    FWDLABEL(		done);

    TclStackFree(interp, keyTokenPtrs);
    return TCL_OK;

    /*
     * Clean up after a failure to create the DictUpdateInfo structure.
     */

  failedUpdateInfoAssembly:
    Tcl_Free(duiPtr);
    TclStackFree(interp, keyTokenPtrs);
  issueFallback:
    return TclCompileBasicMin2ArgCmd(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileDictAppendCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;
    Tcl_LVTIndex dictVarIndex;

    /*
     * There must be at least two argument after the command. And we impose an
     * (arbitrary) safe limit; anyone exceeding it should stop worrying about
     * speed quite so much. ;-)
     * TODO: Raise the limit...
     */

    /* TODO: Consider support for compiling expanded args. */
    if (numWords < 4 || numWords > 100) {
	return TCL_ERROR;
    }

    /*
     * Get the index of the local variable that we will be working with.
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    dictVarIndex = LocalScalarFromToken(tokenPtr, envPtr);
    if (dictVarIndex < 0) {
	return TclCompileBasicMin2ArgCmd(interp, parsePtr,cmdPtr, envPtr);
    }

    /*
     * Produce the string to concatenate onto the dictionary entry.
     */

    tokenPtr = TokenAfter(tokenPtr);
    for (i=2 ; i<numWords ; i++) {
	PUSH_TOKEN(		tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }
    if (numWords > 4) {
	OP1(			STR_CONCAT1, numWords - 3);
    }

    /*
     * Do the concatenation.
     */

    OP4(			DICT_APPEND, dictVarIndex);
    return TCL_OK;
}

int
TclCompileDictLappendCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *keyTokenPtr, *valueTokenPtr;
    Tcl_LVTIndex dictVarIndex;

    /*
     * There must be three arguments after the command.
     */

    /* TODO: Consider support for compiling expanded args. */
    /* Probably not.  Why is INST_DICT_LAPPEND limited to one value? */
    if (parsePtr->numWords != 4) {
	return TCL_ERROR;
    }

    /*
     * Parse the arguments.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    keyTokenPtr = TokenAfter(varTokenPtr);
    valueTokenPtr = TokenAfter(keyTokenPtr);
    dictVarIndex = LocalScalarFromToken(varTokenPtr, envPtr);
    if (dictVarIndex < 0) {
	return TclCompileBasic3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    /*
     * Issue the implementation.
     */

    PUSH_TOKEN(			keyTokenPtr, 2);
    PUSH_TOKEN(			valueTokenPtr, 3);
    OP4(			DICT_LAPPEND, dictVarIndex);
    return TCL_OK;
}

/* Compile [dict with]. Delegates code issuing to IssueDictWithEmpty() and
 * IssueDictWithBodied(). */
int
TclCompileDictWithCmd(
    Tcl_Interp *interp,		/* Used for looking up stuff. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    int bodyIsEmpty = 1;
    Tcl_Size i, numWords = parsePtr->numWords;
    Tcl_Token *varTokenPtr, *tokenPtr;

    /*
     * There must be at least one argument after the command.
     */

    /* TODO: Consider support for compiling expanded args. */
    if (numWords < 3 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * Parse the command (trivially). Expect the following:
     *   dict with <any (varName)> ?<any> ...? <literal>
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    tokenPtr = TokenAfter(varTokenPtr);
    for (i=3 ; i<numWords ; i++) {
	tokenPtr = TokenAfter(tokenPtr);
    }
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TclCompileBasicMin2ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    /*
     * Test if the last word is an empty script; if so, we can compile it in
     * all cases, but if it is non-empty we need local variable table entries
     * to hold the temporary variables (used to keep stack usage simple).
     */

    if (!TclIsEmptyToken(tokenPtr)) {
	if (!EnvIsProc(envPtr)) {
	    return TclCompileBasicMin2ArgCmd(interp, parsePtr, cmdPtr,
		    envPtr);
	}
	bodyIsEmpty = 0;
    }

    /* Now we commit to issuing code. */

    if (bodyIsEmpty) {
	/*
	 * Special case: an empty body means we definitely have no need to issue
	 * try-finally style code or to allocate local variable table entries
	 * for storing temporaries. Still need to do both INST_DICT_EXPAND and
	 * INST_DICT_RECOMBINE_* though, because we can't determine if we're
	 * free of traces.
	 */

	IssueDictWithEmpty(interp, numWords, varTokenPtr, envPtr);
    } else {
	/*
	 * OK, we have a non-trivial body. This means that the focus is on
	 * generating a try-finally structure where the INST_DICT_RECOMBINE_*
	 * goes in the 'finally' clause.
	 */

	IssueDictWithBodied(interp, numWords, varTokenPtr, envPtr);
    }
    return TCL_OK;
}

/*
 * Issue code for a special case of [dict with]: an empty body means we
 * definitely have no need to issue try-finally style code or to allocate local
 * variable table entries for storing temporaries. Still need to do both
 * INST_DICT_EXPAND and INST_DICT_RECOMBINE_* though, because we can't
 * determine if we're free of traces.
 */
static inline void
IssueDictWithEmpty(
    Tcl_Interp *interp,
    Tcl_Size numWords,
    Tcl_Token *varTokenPtr,
    CompileEnv *envPtr)
{
    Tcl_Token *tokenPtr;
    DefineLineInformation;	/* TIP #280 */
    int gotPath;
    Tcl_Size i;
    Tcl_LVTIndex dictVar;

    /*
     * Determine if we're manipulating a dict in a simple local variable.
     */

    gotPath = (numWords > 3);
    dictVar = LocalScalarFromToken(varTokenPtr, envPtr);

    if (dictVar >= 0) {
	if (gotPath) {
	    /*
	     * Case: Path into dict in LVT with empty body.
	     */

	    tokenPtr = TokenAfter(varTokenPtr);
	    for (i=2 ; i<numWords-1 ; i++) {
		PUSH_TOKEN(	tokenPtr, i);
		tokenPtr = TokenAfter(tokenPtr);
	    }
	    OP4(		LIST, numWords - 3);
	    OP4(		LOAD_SCALAR, dictVar);
	    OP4(		OVER, 1);
	    OP(			DICT_EXPAND);
	    OP4(		DICT_RECOMBINE_IMM, dictVar);
	} else {
	    /*
	     * Case: Direct dict in LVT with empty body.
	     */

	    PUSH(		"");
	    OP4(		LOAD_SCALAR, dictVar);
	    PUSH(		"");
	    OP(			DICT_EXPAND);
	    OP4(		DICT_RECOMBINE_IMM, dictVar);
	}
    } else {
	if (gotPath) {
	    /*
	     * Case: Path into dict in non-simple var with empty body.
	     */

	    tokenPtr = varTokenPtr;
	    for (i=1 ; i<numWords-1 ; i++) {
		PUSH_TOKEN(	tokenPtr, i);
		tokenPtr = TokenAfter(tokenPtr);
	    }
	    OP4(		LIST, numWords - 3);
	    OP4(		OVER, 1);
	    OP(			LOAD_STK);
	    OP4(		OVER, 1);
	    OP(			DICT_EXPAND);
	    OP(			DICT_RECOMBINE_STK);
	} else {
	    /*
	     * Case: Direct dict in non-simple var with empty body.
	     */

	    PUSH_TOKEN(		varTokenPtr, 1);
	    OP(			DUP);
	    OP(			LOAD_STK);
	    PUSH(		"");
	    OP(			DICT_EXPAND);
	    PUSH(		"");
	    OP(			SWAP);
	    OP(			DICT_RECOMBINE_STK);
	}
    }
    PUSH(			"");
}

/*
 * Issue code for a [dict with] that has a non-trivial body. The focus is on
 * generating a try-finally structure where the INST_DICT_RECOMBINE_* goes
 * in the 'finally' clause.
 */
static inline void
IssueDictWithBodied(
    Tcl_Interp *interp,
    Tcl_Size numWords,
    Tcl_Token *varTokenPtr,
    CompileEnv *envPtr)
{
    /*
     * Start by allocating local (unnamed, untraced) working variables.
     */

    Tcl_LVTIndex dictVar, keysTmp;
    Tcl_LVTIndex varNameTmp = TCL_INDEX_NONE, pathTmp = TCL_INDEX_NONE;
    int gotPath;
    Tcl_Size i;
    Tcl_BytecodeLabel done;
    Tcl_ExceptionRange range;
    Tcl_Token *tokenPtr;
    DefineLineInformation;	/* TIP #280 */

    /*
     * Determine if we're manipulating a dict in a simple local variable.
     */

    gotPath = (numWords > 3);
    dictVar = LocalScalarFromToken(varTokenPtr, envPtr);

    if (dictVar == TCL_INDEX_NONE) {
	varNameTmp = AnonymousLocal(envPtr);
    }
    if (gotPath) {
	pathTmp = AnonymousLocal(envPtr);
    }
    keysTmp = AnonymousLocal(envPtr);

    /*
     * Issue instructions. First, the part to expand the dictionary.
     */

    if (dictVar == TCL_INDEX_NONE) {
	PUSH_TOKEN(		varTokenPtr, 1);
	OP4(			STORE_SCALAR, varNameTmp);
    }
    tokenPtr = TokenAfter(varTokenPtr);
    if (gotPath) {
	for (i=2 ; i<numWords-1 ; i++) {
	    PUSH_TOKEN(		tokenPtr, i);
	    tokenPtr = TokenAfter(tokenPtr);
	}
	OP4(			LIST, numWords - 3);
	OP4(			STORE_SCALAR, pathTmp);
	OP(			POP);
	if (dictVar == TCL_INDEX_NONE) {
	    OP(			LOAD_STK);
	} else {
	    OP4(		LOAD_SCALAR, dictVar);
	}
	OP4(			LOAD_SCALAR, pathTmp);
    } else {
	if (dictVar == TCL_INDEX_NONE) {
	    OP(			LOAD_STK);
	} else {
	    OP4(		LOAD_SCALAR, dictVar);
	}
	PUSH(			"");
    }
    OP(				DICT_EXPAND);
    OP4(			STORE_SCALAR, keysTmp);
    OP(				POP);

    /*
     * Now the body of the [dict with].
     */

    range = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, range);
    CATCH_RANGE(range) {
	BODY(			tokenPtr, numWords - 1);
    }
    OP(				END_CATCH);

    /*
     * Now fold the results back into the dictionary in the OK case.
     */

    if (dictVar == TCL_INDEX_NONE) {
	OP4(			LOAD_SCALAR, varNameTmp);
	if (gotPath) {
	    OP4(		LOAD_SCALAR, pathTmp);
	} else {
	    PUSH(		"");
	}
	OP4(			LOAD_SCALAR, keysTmp);
	OP(			DICT_RECOMBINE_STK);
    } else {
	if (gotPath) {
	    OP4(		LOAD_SCALAR, pathTmp);
	} else {
	    PUSH(		"");
	}
	OP4(			LOAD_SCALAR, keysTmp);
	OP4(			DICT_RECOMBINE_IMM, dictVar);
    }
    FWDJUMP(			JUMP, done);
    STKDELTA(-1);

    /*
     * Now fold the results back into the dictionary in the exception case.
     */

    CATCH_TARGET(	range);
    OP(				PUSH_RETURN_OPTIONS);
    OP(				PUSH_RESULT);
    OP(				END_CATCH);
    if (dictVar == TCL_INDEX_NONE) {
	OP4(			LOAD_SCALAR, varNameTmp);
	if (numWords > 3) {
	    OP4(		LOAD_SCALAR, pathTmp);
	} else {
	    PUSH(		"");
	}
	OP4(			LOAD_SCALAR, keysTmp);
	OP(			DICT_RECOMBINE_STK);
    } else {
	if (numWords > 3) {
	    OP4(		LOAD_SCALAR, pathTmp);
	} else {
	    PUSH(		"");
	}
	OP4(			LOAD_SCALAR, keysTmp);
	OP4(			DICT_RECOMBINE_IMM, dictVar);
    }
    INVOKE(			RETURN_STK);

    /*
     * Prepare for the start of the next command.
     */

    FWDLABEL(		done);
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
 *	DisassembleDictUpdateInfo: none
 *
 * Side effects:
 *	DupDictUpdateInfo: allocates memory
 *	FreeDictUpdateInfo: releases memory
 *	PrintDictUpdateInfo: none
 *	DisassembleDictUpdateInfo: none
 *
 *----------------------------------------------------------------------
 */

static void *
DupDictUpdateInfo(
    void *clientData)
{
    DictUpdateInfo *dui1Ptr, *dui2Ptr;
    size_t len;

    dui1Ptr = (DictUpdateInfo *)clientData;
    len = offsetof(DictUpdateInfo, varIndices)
	    + sizeof(size_t) * dui1Ptr->length;
    dui2Ptr = (DictUpdateInfo *)Tcl_Alloc(len);
    memcpy(dui2Ptr, dui1Ptr, len);
    return dui2Ptr;
}

static void
FreeDictUpdateInfo(
    void *clientData)
{
    Tcl_Free(clientData);
}

static void
PrintDictUpdateInfo(
    void *clientData,
    Tcl_Obj *appendObj,
    TCL_UNUSED(ByteCode *),
    TCL_UNUSED(size_t))
{
    DictUpdateInfo *duiPtr = (DictUpdateInfo *)clientData;
    Tcl_Size i;

    for (i=0 ; i<duiPtr->length ; i++) {
	Tcl_AppendPrintfToObj(appendObj, "%s%%v%" TCL_Z_MODIFIER "u",
		(i ? ", " : ""),
		duiPtr->varIndices[i]);
    }
}

static void
DisassembleDictUpdateInfo(
    void *clientData,
    Tcl_Obj *dictObj,
    TCL_UNUSED(ByteCode *),
    TCL_UNUSED(size_t))
{
    DictUpdateInfo *duiPtr = (DictUpdateInfo *)clientData;
    Tcl_Size i;
    Tcl_Obj *variables;

    TclNewObj(variables);
    for (i=0 ; i<duiPtr->length ; i++) {
	Tcl_ListObjAppendElement(NULL, variables,
		Tcl_NewWideIntObj(duiPtr->varIndices[i]));
    }
    TclDictPut(NULL, dictObj, "variables", variables);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Size numWords = parsePtr->numWords;

    /*
     * General syntax: [error message ?errorInfo? ?errorCode?]
     */

    if (numWords < 2 || numWords > 4) {
	return TCL_ERROR;
    }

    /*
     * Handle the message.
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);

    /*
     * Construct the options. Note that -code and -level are not here.
     */

    PUSH(			"");
    if (numWords > 2) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH(			"-errorinfo");
	PUSH_TOKEN(		tokenPtr, 2);
	OP(			DICT_PUT);
	if (numWords > 3) {
	    tokenPtr = TokenAfter(tokenPtr);
	    PUSH(		"-errorcode");
	    PUSH_TOKEN(		tokenPtr, 3);
	    OP(			DICT_PUT);
	}
    }

    /*
     * Issue the error via 'returnImm error 0'.
     */

    OP44(			RETURN_IMM, TCL_ERROR, 0);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *firstWordPtr;

    if (parsePtr->numWords == 1 || parsePtr->numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * TIP #280: Use the per-word line information of the current command.
     */

    envPtr->line = envPtr->extCmdMapPtr->loc[
	    envPtr->extCmdMapPtr->nuloc - 1].line[1];

    firstWordPtr = TokenAfter(parsePtr->tokenPtr);
    TclCompileExprWords(interp, firstWordPtr, parsePtr->numWords - 1, envPtr);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *startTokenPtr, *testTokenPtr, *nextTokenPtr, *bodyTokenPtr;
    Tcl_ExceptionRange bodyRange, nextRange = -1;
    Tcl_BytecodeLabel evalBody, testCondition;

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
     * Inline compile the initial command.
     */

    BODY(			startTokenPtr, 1);
    OP(				POP);

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

    FWDJUMP(			JUMP, testCondition);

    /*
     * Compile the loop body.
     */

    bodyRange = MAKE_LOOP_RANGE();
    BACKLABEL(		evalBody);
    CATCH_RANGE(bodyRange) {
	BODY(			bodyTokenPtr, 4);
    }
    OP(				POP);

    /*
     * Compile the "next" subcommand. Note that this exception range will not
     * have a continueOffset (other than -1) connected to it; it won't trap
     * TCL_CONTINUE but rather just TCL_BREAK.
     */

    CONTINUE_TARGET(	bodyRange);
    if (!TclIsEmptyToken(nextTokenPtr)) {
	nextRange = MAKE_LOOP_RANGE();
	envPtr->exceptAuxArrayPtr[nextRange].supportsContinue = 0;
	CATCH_RANGE(nextRange) {
	    BODY(		nextTokenPtr, 3);
	}
	OP(			POP);
    }

    /*
     * Compile the test expression then emit the conditional jump that
     * terminates the for.
     */

    FWDLABEL(		testCondition);
    PUSH_EXPR_TOKEN(		testTokenPtr, 2);
    BACKJUMP(			JUMP_TRUE, evalBody);

    /*
     * Fix the starting points of the exception ranges (may have moved due to
     * jump type modification) and set where the exceptions target.
     */

    BREAK_TARGET(	bodyRange);
    FINALIZE_LOOP(bodyRange);
    if (nextRange != -1) {
	BREAK_TARGET(	nextRange);
	FINALIZE_LOOP(nextRange);
    }

    /*
     * The for command's result is an empty string.
     */

    PUSH(			"");
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
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    return CompileEachloopCmd(interp, parsePtr, cmdPtr, envPtr,
	    TCL_EACH_KEEP_NONE);
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
    Command *cmdPtr,		/* Points to the definition of the command
				 *  being compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    return CompileEachloopCmd(interp, parsePtr, cmdPtr, envPtr,
	    TCL_EACH_COLLECT);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr,		/* Holds resulting instructions. */
    int collect)		/* Select collecting or accumulating mode
				 * (TCL_EACH_*) */
{
    DefineLineInformation;	/* TIP #280 */
    ForeachInfo *infoPtr=NULL;	/* Points to the structure describing this
				 * foreach command. Stored in a AuxData
				 * record in the ByteCode. */

    Tcl_Token *tokenPtr, *bodyTokenPtr;
    Tcl_Size jumpBackOffset, numWords, numLists, i, j;
    Tcl_AuxDataRef infoIndex;
    Tcl_ExceptionRange range;
    int code = TCL_OK;
    Tcl_Obj *varListObj = NULL;

    /*
     * If the foreach command isn't in a procedure, don't compile it inline:
     * the payoff is too small.
     */

    if (!EnvIsProc(envPtr)) {
	return TCL_ERROR;
    }

    numWords = parsePtr->numWords;
    if ((numWords < 4) || (numWords > UINT_MAX) || (numWords%2 != 0)) {
	return TCL_ERROR;
    }

    /*
     * Bail out if the body requires substitutions in order to ensure correct
     * behaviour. [Bug 219166]
     */

    for (i = 0, tokenPtr = parsePtr->tokenPtr; i < numWords-1; i++) {
	tokenPtr = TokenAfter(tokenPtr);
    }
    bodyTokenPtr = tokenPtr;
    if (bodyTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }

    /*
     * Create and initialize the ForeachInfo and ForeachVarList data
     * structures describing this command. Then create a AuxData record
     * pointing to the ForeachInfo structure.
     */

    numLists = (numWords - 2)/2;
    infoPtr = (ForeachInfo *)Tcl_Alloc(offsetof(ForeachInfo, varLists)
	    + numLists * sizeof(ForeachVarList *));
    infoPtr->numLists = 0;	/* Count this up as we go */

    /*
     * Parse each var list into sequence of var names.  Don't
     * compile the foreach inline if any var name needs substitutions or isn't
     * a scalar, or if any var list needs substitutions.
     */

    TclNewObj(varListObj);
    for (i = 0, tokenPtr = parsePtr->tokenPtr;
	    i < numWords-1;
	    i++, tokenPtr = TokenAfter(tokenPtr)) {
	ForeachVarList *varListPtr;
	Tcl_Size numVars;

	if (i%2 != 1) {
	    continue;
	}

	/*
	 * If the variable list is empty, we can enter an infinite loop when
	 * the interpreted version would not.  Take care to ensure this does
	 * not happen.  [Bug 1671138]
	 */

	if (!TclWordKnownAtCompileTime(tokenPtr, varListObj) ||
		TCL_OK != TclListObjLength(NULL, varListObj, &numVars) ||
		numVars == 0) {
	    code = TCL_ERROR;
	    goto done;
	}

	varListPtr = (ForeachVarList *)Tcl_Alloc(offsetof(ForeachVarList, varIndexes)
		+ numVars * sizeof(varListPtr->varIndexes[0]));
	varListPtr->numVars = numVars;
	infoPtr->varLists[i / 2] = varListPtr;
	infoPtr->numLists++;

	for (j = 0;  j < numVars;  j++) {
	    Tcl_Obj *varNameObj;
	    const char *bytes;
	    Tcl_LVTIndex varIndex;
	    Tcl_Size length;

	    Tcl_ListObjIndex(NULL, varListObj, j, &varNameObj);
	    bytes = TclGetStringFromObj(varNameObj, &length);
	    varIndex = LocalScalar(bytes, length, envPtr);
	    if (varIndex < 0) {
		code = TCL_ERROR;
		goto done;
	    }
	    varListPtr->varIndexes[j] = varIndex;
	}
	Tcl_SetObjLength(varListObj, 0);
    }

    /*
     * We will compile the foreach command.
     */

    infoIndex = TclCreateAuxData(infoPtr, &newForeachInfoType, envPtr);

    /*
     * Create the collecting object, unshared.
     */

    if (collect == TCL_EACH_COLLECT) {
	OP4(			LIST, 0);
    }

    /*
     * Evaluate each value list and leave it on stack.
     */

    for (i = 0, tokenPtr = parsePtr->tokenPtr;
	    i < numWords-1;
	    i++, tokenPtr = TokenAfter(tokenPtr)) {
	if ((i%2 == 0) && (i > 0)) {
	    PUSH_TOKEN(		tokenPtr, i);
	}
    }

    OP4(			FOREACH_START, infoIndex);

    /*
     * Inline compile the loop body.
     */

    range = MAKE_LOOP_RANGE();

    CATCH_RANGE(range) {
	BODY(			bodyTokenPtr, numWords - 1);
    }

    if (collect == TCL_EACH_COLLECT) {
	OP(			LMAP_COLLECT);
    } else {
	OP(			POP);
    }

    /*
     * Bottom of loop code: assign each loop variable and check whether
     * to terminate the loop. Set the loop's break target.
     */

    CONTINUE_TARGET(	range);
    OP(				FOREACH_STEP);
    BREAK_TARGET(	range);
    FINALIZE_LOOP(range);
    OP(				FOREACH_END);
    STKDELTA(-(numLists + 2));

    /*
     * Set the jumpback distance from INST_FOREACH_STEP to the start of the
     * body's code. Misuse loopCtTemp for storing the jump size.
     */

    jumpBackOffset = envPtr->exceptArrayPtr[range].continueOffset -
	    envPtr->exceptArrayPtr[range].codeOffset;
    infoPtr->loopCtTemp = -jumpBackOffset;

    /*
     * The command's result is an empty string if not collecting. If
     * collecting, it is automatically left on stack after FOREACH_END.
     */

    if (collect != TCL_EACH_COLLECT) {
	PUSH(			"");
    }

  done:
    if (code == TCL_ERROR) {
	FreeForeachInfo(infoPtr);
    }
    Tcl_DecrRefCount(varListObj);
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

static void *
DupForeachInfo(
    void *clientData)		/* The foreach command's compilation auxiliary
				 * data to duplicate. */
{
    ForeachInfo *srcPtr = (ForeachInfo *)clientData;
    ForeachInfo *dupPtr;
    ForeachVarList *srcListPtr, *dupListPtr;
    Tcl_Size numVars, i, j, numLists = srcPtr->numLists;

    dupPtr = (ForeachInfo *)Tcl_Alloc(offsetof(ForeachInfo, varLists)
	    + numLists * sizeof(ForeachVarList *));
    dupPtr->numLists = numLists;
    dupPtr->firstValueTemp = srcPtr->firstValueTemp;
    dupPtr->loopCtTemp = srcPtr->loopCtTemp;

    for (i = 0;  i < numLists;  i++) {
	srcListPtr = srcPtr->varLists[i];
	numVars = srcListPtr->numVars;
	dupListPtr = (ForeachVarList *)Tcl_Alloc(offsetof(ForeachVarList, varIndexes)
		+ numVars * sizeof(size_t));
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
    void *clientData)		/* The foreach command's compilation auxiliary
				 * data to free. */
{
    ForeachInfo *infoPtr = (ForeachInfo *)clientData;
    ForeachVarList *listPtr;
    Tcl_Size i, numLists = infoPtr->numLists;

    for (i = 0;  i < numLists;  i++) {
	listPtr = infoPtr->varLists[i];
	Tcl_Free(listPtr);
    }
    Tcl_Free(infoPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PrintForeachInfo, DisassembleForeachInfo --
 *
 *	Functions to write a human-readable or script-readablerepresentation
 *	of a ForeachInfo structure to a Tcl_Obj for debugging.
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
    void *clientData,
    Tcl_Obj *appendObj,
    TCL_UNUSED(ByteCode *),
    TCL_UNUSED(size_t))
{
    ForeachInfo *infoPtr = (ForeachInfo *)clientData;
    ForeachVarList *varsPtr;
    Tcl_Size i, j;

    Tcl_AppendToObj(appendObj, "data=[", TCL_AUTO_LENGTH);

    for (i=0 ; i<infoPtr->numLists ; i++) {
	if (i) {
	    Tcl_AppendToObj(appendObj, ", ", TCL_AUTO_LENGTH);
	}
	Tcl_AppendPrintfToObj(appendObj, "%%v%" TCL_Z_MODIFIER "u",
		(infoPtr->firstValueTemp + i));
    }
    Tcl_AppendPrintfToObj(appendObj, "], loop=%%v%" TCL_Z_MODIFIER "u",
	    infoPtr->loopCtTemp);
    for (i=0 ; i<infoPtr->numLists ; i++) {
	if (i) {
	    Tcl_AppendToObj(appendObj, ",", TCL_AUTO_LENGTH);
	}
	Tcl_AppendPrintfToObj(appendObj, "\n\t\t it%%v%" TCL_Z_MODIFIER "u\t[",
		(infoPtr->firstValueTemp + i));
	varsPtr = infoPtr->varLists[i];
	for (j=0 ; j<varsPtr->numVars ; j++) {
	    if (j) {
		Tcl_AppendToObj(appendObj, ", ", TCL_AUTO_LENGTH);
	    }
	    Tcl_AppendPrintfToObj(appendObj, "%%v%" TCL_Z_MODIFIER "u",
		    varsPtr->varIndexes[j]);
	}
	Tcl_AppendToObj(appendObj, "]", TCL_AUTO_LENGTH);
    }
}

static void
PrintNewForeachInfo(
    void *clientData,
    Tcl_Obj *appendObj,
    TCL_UNUSED(ByteCode *),
    TCL_UNUSED(size_t))
{
    ForeachInfo *infoPtr = (ForeachInfo *)clientData;
    ForeachVarList *varsPtr;
    Tcl_Size i, j;

    Tcl_AppendPrintfToObj(appendObj, "jumpOffset=%+" TCL_Z_MODIFIER "d, vars=",
	    infoPtr->loopCtTemp);
    for (i=0 ; i<infoPtr->numLists ; i++) {
	if (i) {
	    Tcl_AppendToObj(appendObj, ",", TCL_AUTO_LENGTH);
	}
	Tcl_AppendToObj(appendObj, "[", TCL_AUTO_LENGTH);
	varsPtr = infoPtr->varLists[i];
	for (j=0 ; j<varsPtr->numVars ; j++) {
	    if (j) {
		Tcl_AppendToObj(appendObj, ",", TCL_AUTO_LENGTH);
	    }
	    Tcl_AppendPrintfToObj(appendObj, "%%v%" TCL_Z_MODIFIER "u",
		    varsPtr->varIndexes[j]);
	}
	Tcl_AppendToObj(appendObj, "]", TCL_AUTO_LENGTH);
    }
}

static void
DisassembleForeachInfo(
    void *clientData,
    Tcl_Obj *dictObj,
    TCL_UNUSED(ByteCode *),
    TCL_UNUSED(size_t))
{
    ForeachInfo *infoPtr = (ForeachInfo *)clientData;
    ForeachVarList *varsPtr;
    Tcl_Size i, j;
    Tcl_Obj *objPtr, *innerPtr;

    /*
     * Data stores.
     */

    TclNewObj(objPtr);
    for (i=0 ; i<infoPtr->numLists ; i++) {
	Tcl_ListObjAppendElement(NULL, objPtr,
		Tcl_NewWideIntObj(infoPtr->firstValueTemp + i));
    }
    TclDictPut(NULL, dictObj, "data", objPtr);

    /*
     * Loop counter.
     */

    TclDictPut(NULL, dictObj, "loop", Tcl_NewWideIntObj(infoPtr->loopCtTemp));

    /*
     * Assignment targets.
     */

    TclNewObj(objPtr);
    for (i=0 ; i<infoPtr->numLists ; i++) {
	TclNewObj(innerPtr);
	varsPtr = infoPtr->varLists[i];
	for (j=0 ; j<varsPtr->numVars ; j++) {
	    Tcl_ListObjAppendElement(NULL, innerPtr,
		    Tcl_NewWideIntObj(varsPtr->varIndexes[j]));
	}
	Tcl_ListObjAppendElement(NULL, objPtr, innerPtr);
    }
    TclDictPut(NULL, dictObj, "assign", objPtr);
}

static void
DisassembleNewForeachInfo(
    void *clientData,
    Tcl_Obj *dictObj,
    TCL_UNUSED(ByteCode *),
    TCL_UNUSED(size_t))
{
    ForeachInfo *infoPtr = (ForeachInfo *)clientData;
    ForeachVarList *varsPtr;
    Tcl_Size i, j;
    Tcl_Obj *objPtr, *innerPtr;

    /*
     * Jump offset.
     */

    TclDictPut(NULL, dictObj, "jumpOffset",
	    Tcl_NewWideIntObj(infoPtr->loopCtTemp));

    /*
     * Assignment targets.
     */

    TclNewObj(objPtr);
    for (i=0 ; i<infoPtr->numLists ; i++) {
	TclNewObj(innerPtr);
	varsPtr = infoPtr->varLists[i];
	for (j=0 ; j<varsPtr->numVars ; j++) {
	    Tcl_ListObjAppendElement(NULL, innerPtr,
		    Tcl_NewWideIntObj(varsPtr->varIndexes[j]));
	}
	Tcl_ListObjAppendElement(NULL, objPtr, innerPtr);
    }
    TclDictPut(NULL, dictObj, "assign", objPtr);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    Tcl_Obj **objv, *formatObj, *tmpObj;
    const char *bytes, *start;
    Tcl_Size i, j, numWords = parsePtr->numWords;
    /* TODO: Consider support for compiling expanded args. */

    /*
     * Don't handle any guaranteed-error cases.
     */

    if (numWords < 2 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * Check if the argument words are all compile-time-known literals; that's
     * a case we can handle by compiling to a constant.
     */

    TclNewObj(formatObj);
    Tcl_IncrRefCount(formatObj);
    tokenPtr = TokenAfter(tokenPtr);
    if (!TclWordKnownAtCompileTime(tokenPtr, formatObj)) {
	Tcl_DecrRefCount(formatObj);
	return TCL_ERROR;
    }

    objv = (Tcl_Obj **)TclStackAlloc(interp,
	    (numWords - 2) * sizeof(Tcl_Obj *));
    for (i=0 ; i+2 < numWords ; i++) {
	tokenPtr = TokenAfter(tokenPtr);
	TclNewObj(objv[i]);
	Tcl_IncrRefCount(objv[i]);
	if (!TclWordKnownAtCompileTime(tokenPtr, objv[i])) {
	    goto checkForStringConcatCase;
	}
    }

    /*
     * Everything is a literal, so the result is constant too (or an error if
     * the format is broken). Do the format now.
     */

    tmpObj = Tcl_Format(interp, TclGetString(formatObj), numWords - 2, objv);
    for (; --i>=0 ;) {
	Tcl_DecrRefCount(objv[i]);
    }
    TclStackFree(interp, objv);
    Tcl_DecrRefCount(formatObj);
    if (tmpObj == NULL) {
	TclCompileSyntaxError(interp, envPtr);
	return TCL_OK;
    }

    /*
     * Not an error, always a constant result, so just push the result as a
     * literal. Job done.
     */

    PUSH_OBJ(			tmpObj);
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

    for (; i>=0 ; i--) {
	Tcl_DecrRefCount(objv[i]);
    }
    TclStackFree(interp, objv);
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    tokenPtr = TokenAfter(tokenPtr);
    i = 0;

    /*
     * Now scan through and check for non-%s and non-%% substitutions.
     */

    for (bytes = TclGetString(formatObj) ; *bytes ; bytes++) {
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

    if (i+2 != numWords || i > 125) {
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
    start = TclGetString(formatObj);
				/* The start of the currently-scanned literal
				 * in the format string. */
    TclNewObj(tmpObj);		/* The buffer used to accumulate the literal
				 * being built. */
    for (bytes = start ; *bytes ; bytes++) {
	if (*bytes == '%') {
	    Tcl_AppendToObj(tmpObj, start, bytes - start);
	    if (*++bytes == '%') {
		Tcl_AppendToObj(tmpObj, "%", 1);
	    } else {
		/*
		 * If there is a non-empty literal from the format string,
		 * push it and reset.
		 */

		if (TclGetString(tmpObj)[0]) {
		    PUSH_OBJ(	tmpObj);
		    TclNewObj(tmpObj);
		    i++;
		}

		/*
		 * Push the code to produce the string that would be
		 * substituted with %s, except we'll be concatenating
		 * directly.
		 */

		PUSH_TOKEN(	tokenPtr, j);
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
    if (TclGetString(tmpObj)[0]) {
	PUSH_OBJ(		tmpObj);
	i++;
    }
    Tcl_BounceRefCount(tmpObj);
    Tcl_DecrRefCount(formatObj);

    if (i > 1) {
	/*
	 * Do the concatenation, which produces the result.
	 */

	OP1(			STR_CONCAT1, i);
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
