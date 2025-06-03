/*
 * tclCompCmdsGR.c --
 *
 *	This file contains compilation procedures that compile various Tcl
 *	commands (beginning with the letters 'g' through 'r') into a sequence
 *	of instructions ("bytecodes").
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
#include <assert.h>

/*
 * Prototypes for procedures defined later in this file:
 */

static void		CompileReturnInternal(CompileEnv *envPtr,
			    unsigned char op, int code, int level,
			    Tcl_Obj *returnOpts);
static Tcl_LVTIndex	IndexTailVarIfKnown(Tcl_Interp *interp,
			    Tcl_Token *varTokenPtr, CompileEnv *envPtr);

/*
 *----------------------------------------------------------------------
 *
 * TclGetIndexFromToken --
 *
 *	Parse a token to determine if an index value is known at
 *	compile time.
 *
 * Returns:
 *	TCL_OK if parsing succeeded, and TCL_ERROR if it failed.
 *
 * Side effects:
 *	When TCL_OK is returned, the encoded index value is written
 *	to *index.
 *
 *----------------------------------------------------------------------
 */

int
TclGetIndexFromToken(
    Tcl_Token *tokenPtr,
    size_t before,
    size_t after,
    int *indexPtr)
{
    Tcl_Obj *tmpObj;
    int result = TCL_ERROR;

    TclNewObj(tmpObj);
    if (TclWordKnownAtCompileTime(tokenPtr, tmpObj)) {
	result = TclIndexEncode(NULL, tmpObj, (int)before, (int)after, indexPtr);
    }
    Tcl_DecrRefCount(tmpObj);
    return result;
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr;
    Tcl_LVTIndex localIndex;
    Tcl_Size i, numWords = parsePtr->numWords;

    if (numWords < 2 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * 'global' has no effect outside of proc bodies; handle that at runtime
     */

    if (envPtr->procPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Push the namespace
     */

    PUSH(			"::");

    /*
     * Loop over the variables.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    for (i=1; i<numWords; varTokenPtr = TokenAfter(varTokenPtr),i++) {
	localIndex = IndexTailVarIfKnown(interp, varTokenPtr, envPtr);

	if (localIndex < 0 || localIndex > INT_MAX) {
	    return TCL_ERROR;
	}

	/*
	 * TODO: Consider what value can pass through the
	 * IndexTailVarIfKnown() screen. Full CompileWord() likely does not
	 * apply here. Push known value instead.
	 */

	PUSH_TOKEN(		varTokenPtr, i);
	OP4(			NSUPVAR, localIndex);
    }

    /*
     * Pop the namespace, and set the result to empty
     */

    OP(				POP);
    PUSH(			"");
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    JumpFixupArray jumpFalseFixupArray;
				/* Used to fix the ifFalse jump after each
				 * test when its target PC is determined. */
    JumpFixupArray jumpEndFixupArray;
				/* Used to fix the jump after each "then" body
				 * to the end of the "if" when that PC is
				 * determined. */
    Tcl_Token *tokenPtr, *testTokenPtr;
    Tcl_Size jumpIndex = 0;	/* Avoid compiler warning. */
    Tcl_Size j, numWords, wordIdx;
    int code;
    int realCond = 1;		/* Set to 0 for static conditions:
				 * "if 0 {..}" */
    int boolVal;		/* Value of static condition. */
    int compileScripts = 1;

    /*
     * Only compile the "if" command if all arguments are simple words, in
     * order to ensure correct substitution [Bug 219166]
     */

    tokenPtr = parsePtr->tokenPtr;
    numWords = parsePtr->numWords;
    if (numWords > UINT_MAX) {
	return TCL_ERROR;
    }

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

	if ((tokenPtr == parsePtr->tokenPtr)
		|| IS_TOKEN_LITERALLY(tokenPtr, "elseif")) {
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

	testTokenPtr = tokenPtr;

	if (realCond) {
	    /*
	     * Find out if the condition is a constant.
	     */

	    Tcl_Obj *boolObj = TokenToObj(testTokenPtr);
	    code = Tcl_GetBooleanFromObj(NULL, boolObj, &boolVal);
	    Tcl_BounceRefCount(boolObj);
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
		PUSH_EXPR_TOKEN(testTokenPtr, wordIdx);
		if (jumpFalseFixupArray.next >= jumpFalseFixupArray.end) {
		    TclExpandJumpFixupArray(&jumpFalseFixupArray);
		}
		jumpIndex = jumpFalseFixupArray.next++;
		TclEmitForwardJump(envPtr, TCL_FALSE_JUMP,
			jumpFalseFixupArray.fixup + jumpIndex);
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
	if (IS_TOKEN_LITERALLY(tokenPtr, "then")) {
	    tokenPtr = TokenAfter(tokenPtr);
	    wordIdx++;
	    if (wordIdx >= numWords) {
		code = TCL_ERROR;
		goto done;
	    }
	}

	/*
	 * Compile the "then" command body.
	 */

	if (compileScripts) {
	    BODY(		tokenPtr, wordIdx);
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
		    jumpEndFixupArray.fixup + jumpIndex);

	    /*
	     * Fix the target of the jumpFalse after the test.
	     */

	    STKDELTA(-1);
	    TclFixupForwardJumpToHere(envPtr,
		    jumpFalseFixupArray.fixup + jumpIndex);
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
     * Check for the optional else clause. Do not compile anything if this was
     * an "if 1 {...}" case.
     */

    if ((wordIdx < numWords) && (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD)) {
	/*
	 * There is an else clause. Skip over the optional "else" word.
	 */

	if (IS_TOKEN_LITERALLY(tokenPtr, "else")) {
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

	    BODY(		tokenPtr, wordIdx);
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
	    PUSH(		"");
	}
    }

    /*
     * Fix the unconditional jumps to the end of the "if" command.
     */

    for (j = jumpEndFixupArray.next;  j > 0;  j--) {
	jumpIndex = (j - 1);	/* i.e. process the closest jump first. */
	TclFixupForwardJumpToHere(envPtr,
		jumpEndFixupArray.fixup + jumpIndex);
    }

    /*
     * Free the jumpFixupArray array if malloc'ed storage was used.
     */

  done:
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *incrTokenPtr;
    int isScalar, haveImmValue;
    Tcl_LVTIndex localIndex;
    Tcl_WideInt immValue;

    if ((parsePtr->numWords != 2) && (parsePtr->numWords != 3)) {
	return TCL_ERROR;
    }

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    PushVarNameWord(varTokenPtr, 0, &localIndex, &isScalar, 1);

    /*
     * If an increment is given, push it, but see first if it's a small
     * integer.
     */

    haveImmValue = 0;
    immValue = 1;
    if (parsePtr->numWords == 3) {
	Tcl_Obj *intObj;
	incrTokenPtr = TokenAfter(varTokenPtr);
	TclNewObj(intObj);
	if (TclWordKnownAtCompileTime(incrTokenPtr, intObj)) {
	    int code = TclGetWideIntFromObj(NULL, intObj, &immValue);
	    if ((code == TCL_OK) && (-127 <= immValue) && (immValue <= 127)) {
		haveImmValue = 1;
	    }
	}
	Tcl_BounceRefCount(intObj);
	if (!haveImmValue) {
	    SetLineInformation(2);
	    CompileTokens(envPtr, incrTokenPtr, interp);
	}
    } else {			/* No incr amount given so use 1. */
	haveImmValue = 1;
    }

    /*
     * Emit the instruction to increment the variable.
     */

    if (isScalar) {		/* Simple scalar variable. */
	if (localIndex >= 0) {
	    if (haveImmValue) {
		OP41(		INCR_SCALAR_IMM, localIndex, immValue);
	    } else {
		OP4(		INCR_SCALAR, localIndex);
	    }
	} else {
	    if (haveImmValue) {
		OP1(		INCR_STK_IMM, immValue);
	    } else {
		OP(		INCR_STK);
	    }
	}
    } else {			/* Simple array variable. */
	if (localIndex >= 0) {
	    if (haveImmValue) {
		OP41(		INCR_ARRAY_IMM, localIndex, immValue);
	    } else {
		OP4(		INCR_ARRAY, localIndex);
	    }
	} else {
	    if (haveImmValue) {
		OP1(		INCR_ARRAY_STK_IMM, immValue);
	    } else {
		OP(		INCR_ARRAY_STK);
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
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Obj *objPtr;
    const char *bytes;
    Tcl_BytecodeLabel isList;

    /*
     * We require one compile-time known argument for the case we can compile.
     */

    if (parsePtr->numWords == 1) {
	return TclCompileBasic0ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    } else if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    TclNewObj(objPtr);
    Tcl_IncrRefCount(objPtr);
    if (!TclWordKnownAtCompileTime(tokenPtr, objPtr)) {
	goto notCompilable;
    }
    bytes = TclGetString(objPtr);

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
     * Confirmed as a literal that will not frighten the horses. Compile.
     * The result must be made into a list.
     */

    /* TODO: Just push the known value */
    PUSH_TOKEN(			tokenPtr, 1);
    OP(				RESOLVE_COMMAND);
    OP(				DUP);
    OP(				STR_LEN);
    FWDJUMP(			JUMP_FALSE, isList);
    OP4(			LIST, 1);
    FWDLABEL(		isList);
    return TCL_OK;

  notCompilable:
    Tcl_DecrRefCount(objPtr);
    return TclCompileBasic1ArgCmd(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileInfoCoroutineCmd(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
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

    OP(				COROUTINE_NAME);
    return TCL_OK;
}

int
TclCompileInfoExistsCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
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

    /*
     * Decide if we can use a frame slot for the var/array name or if we need
     * to emit code to compute and push the name at runtime. We use a frame
     * slot (entry in the array of local vars) if we are compiling a procedure
     * body and if the name is simple text that does not include namespace
     * qualifiers.
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PushVarNameWord(tokenPtr, 0, &localIndex, &isScalar, 1);

    /*
     * Emit instruction to check the variable for existence.
     */

    if (isScalar) {
	if (localIndex < 0) {
	    OP(			EXIST_STK);
	} else {
	    OP4(		EXIST_SCALAR, localIndex);
	}
    } else {
	if (localIndex < 0) {
	    OP(			EXIST_ARRAY_STK);
	} else {
	    OP4(		EXIST_ARRAY, localIndex);
	}
    }

    return TCL_OK;
}

int
TclCompileInfoLevelCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Only compile [info level] without arguments or with a single argument.
     */

    if (parsePtr->numWords == 1) {
	/*
	 * Not much to do; we compile to a single instruction...
	 */

	OP(			INFO_LEVEL_NUM);
    } else if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    } else {
	DefineLineInformation;	/* TIP #280 */

	/*
	 * Compile the argument, then add the instruction to convert it into a
	 * list of arguments.
	 */

	PUSH_TOKEN(		TokenAfter(parsePtr->tokenPtr), 1);
	OP(			INFO_LEVEL_ARGS);
    }
    return TCL_OK;
}

int
TclCompileInfoObjectClassCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    PUSH_TOKEN(			tokenPtr, 1);
    OP(				TCLOO_CLASS);
    return TCL_OK;
}

int
TclCompileInfoObjectCreationIdCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    PUSH_TOKEN(			tokenPtr, 1);
    OP(				TCLOO_ID);
    return TCL_OK;
}

int
TclCompileInfoObjectIsACmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
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
    if (!IS_TOKEN_PREFIX(tokenPtr, 2, "object")) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(tokenPtr);

    /*
     * Issue the code.
     */

    PUSH_TOKEN(			tokenPtr, 2);
    OP(				TCLOO_IS_OBJECT);
    return TCL_OK;
}

int
TclCompileInfoObjectNamespaceCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    PUSH_TOKEN(			tokenPtr, 1);
    OP(				TCLOO_NS);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *valueTokenPtr;
    Tcl_Size numWords = parsePtr->numWords, i;
    int isScalar;
    Tcl_LVTIndex localIndex;

    /* TODO: Consider support for compiling expanded args. */
    if (numWords < 2 || numWords > UINT_MAX) {
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
    PushVarNameWord(varTokenPtr, 0, &localIndex, &isScalar, 1);

    /*
     * The weird cluster of bugs around INST_LAPPEND_STK without a LVT ought
     * to be sorted out. INST_LAPPEND_LIST_STK does the right thing.
     */
    if (numWords != 3 || !EnvHasLVT(envPtr)) {
	goto lappendMultiple;
    }

    /*
     * We are doing an assignment, so push the new value.
     */

    valueTokenPtr = TokenAfter(varTokenPtr);
    PUSH_TOKEN(			valueTokenPtr, 2);

    /*
     * Emit instructions to set/get the variable.
     *
     * The *_STK opcodes should be refactored to make better use of existing
     * LOAD/STORE instructions.
     */

    if (isScalar) {
	if (localIndex < 0) {
	    OP(			LAPPEND_STK);
	} else {
	    OP4(		LAPPEND_SCALAR, localIndex);
	}
    } else {
	if (localIndex < 0) {
	    OP(			LAPPEND_ARRAY_STK);
	} else {
	    OP4(		LAPPEND_ARRAY, localIndex);
	}
    }
    return TCL_OK;

    /*
     * In the cases where there's not a single value to append to the list in
     * the variable, we use a different strategy. This is to turn the arguments
     * into a list and then append that list's elements. The downside is that
     * this allocates a temporary working list, but at least it simplifies the
     * code issuing a lot.
     */

  lappendMultiple:

    /*
     * Concatenate all our remaining arguments into a list.
     * TODO: Turn this into an expand-handling list building sequence.
     */

    if (numWords == 2) {
	PUSH(			"");
    } else {
	valueTokenPtr = TokenAfter(varTokenPtr);
	for (i = 2 ; i < numWords ; i++) {
	    PUSH_TOKEN(		valueTokenPtr, i);
	    valueTokenPtr = TokenAfter(valueTokenPtr);
	}
	OP4(			LIST, numWords - 2);
    }

    /*
     * Append the items of the list to the variable. The implementation of
     * these opcodes handles all the special cases that [lappend] knows about.
     */

    if (isScalar) {
	if (localIndex < 0) {
	    OP(			LAPPEND_LIST_STK);
	} else {
	    OP4(		LAPPEND_LIST, localIndex);
	}
    } else {
	if (localIndex < 0) {
	    OP(			LAPPEND_LIST_ARRAY_STK);
	} else {
	    OP4(		LAPPEND_LIST_ARRAY, localIndex);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    int isScalar;
    Tcl_Size numWords = parsePtr->numWords, idx;
    Tcl_LVTIndex localIndex;
    /* TODO: Consider support for compiling expanded args. */

    /*
     * Check for command syntax error, but we'll punt that to runtime.
     */

    if (numWords < 3 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * Generate code to push list being taken apart by [lassign].
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);

    /*
     * Generate code to assign values from the list to variables.
     */

    for (idx=0 ; idx<numWords-2 ; idx++) {
	/*
	 * Generate the next variable name.
	 */

	tokenPtr = TokenAfter(tokenPtr);
	PushVarNameWord(tokenPtr, 0, &localIndex, &isScalar, idx + 2);

	/*
	 * Emit instructions to get the idx'th item out of the list value on
	 * the stack and assign it to the variable.
	 */

	if (isScalar) {
	    if (localIndex >= 0) {
		OP(		DUP);
		OP4(		LIST_INDEX_IMM, idx);
		OP4(		STORE_SCALAR, localIndex);
		OP(		POP);
	    } else {
		OP4(		OVER, 1);
		OP4(		LIST_INDEX_IMM, idx);
		OP(		STORE_STK);
		OP(		POP);
	    }
	} else {
	    if (localIndex >= 0) {
		OP4(		OVER, 1);
		OP4(		LIST_INDEX_IMM, idx);
		OP4(		STORE_ARRAY, localIndex);
		OP(		POP);
	    } else {
		OP4(		OVER, 2);
		OP4(		LIST_INDEX_IMM, idx);
		OP(		STORE_ARRAY_STK);
		OP(		POP);
	    }
	}
    }

    /*
     * Generate code to leave the rest of the list on the stack.
     */

    OP44(		LIST_RANGE_IMM, idx, TCL_INDEX_END);

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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *idxTokenPtr, *valTokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;
    int idx;

    /*
     * Quit if not enough args.
     */

    /* TODO: Consider support for compiling expanded args. */
    if (numWords <= 1 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    valTokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (numWords != 3) {
	goto emitComplexLindex;
    }

    idxTokenPtr = TokenAfter(valTokenPtr);
    if (TclGetIndexFromToken(idxTokenPtr, TCL_INDEX_NONE,
	    TCL_INDEX_NONE, &idx) == TCL_OK) {
	/*
	 * The idxTokenPtr parsed as a valid index value and was
	 * encoded as expected by INST_LIST_INDEX_IMM.
	 *
	 * NOTE: that we rely on indexing before a list producing the
	 * same result as indexing after a list.
	 */

	PUSH_TOKEN(		valTokenPtr, 1);
	OP4(			LIST_INDEX_IMM, idx);
	return TCL_OK;
    }

    /*
     * If the value was not known at compile time, the conversion failed or
     * the value was negative, we just keep on going with the more complex
     * compilation.
     */

    /*
     * Push the operands onto the stack.
     */

  emitComplexLindex:
    for (i=1 ; i<numWords ; i++) {
	PUSH_TOKEN(		valTokenPtr, i);
	valTokenPtr = TokenAfter(valTokenPtr);
    }

    /*
     * Emit INST_LIST_INDEX if objc==3, or INST_LIST_INDEX_MULTI if there are
     * multiple index args.
     */

    if (numWords == 3) {
	OP(			LIST_INDEX);
    } else {
	OP4(			LIST_INDEX_MULTI, numWords - 1);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileListCmd --
 *
 *	Procedure called to compile the "list" command.
 *	Handles argument expansion directly.
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *valueTokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;
    int concat, build;
    Tcl_Obj *listObj, *objPtr;

    if (numWords > UINT_MAX) {
	return TCL_ERROR;
    }
    if (numWords == 1) {
	/*
	 * [list] without arguments just pushes an empty object.
	 */

	PUSH(			"");
	return TCL_OK;
    }

    /*
     * Test if all arguments are compile-time known. If they are, we can
     * implement with a simple push.
     */

    valueTokenPtr = TokenAfter(parsePtr->tokenPtr);
    TclNewObj(listObj);
    for (i = 1; i < numWords && listObj != NULL; i++) {
	TclNewObj(objPtr);
	if (TclWordKnownAtCompileTime(valueTokenPtr, objPtr)) {
	    (void) Tcl_ListObjAppendElement(NULL, listObj, objPtr);
	} else {
	    Tcl_DecrRefCount(objPtr);
	    Tcl_DecrRefCount(listObj);
	    listObj = NULL;
	}
	valueTokenPtr = TokenAfter(valueTokenPtr);
    }
    if (listObj != NULL) {
	PUSH_OBJ(		listObj);
	return TCL_OK;
    }

    /*
     * Push the all values onto the stack.
     */

    valueTokenPtr = TokenAfter(parsePtr->tokenPtr);
    concat = build = 0;
    for (i = 1; i < numWords; i++) {
	if (valueTokenPtr->type == TCL_TOKEN_EXPAND_WORD && build > 0) {
	    OP4(		LIST, build);
	    if (concat) {
		OP(		LIST_CONCAT);
	    }
	    build = 0;
	    concat = 1;
	}
	PUSH_TOKEN(		valueTokenPtr, i);
	if (valueTokenPtr->type == TCL_TOKEN_EXPAND_WORD) {
	    if (concat) {
		OP(		LIST_CONCAT);
	    } else {
		concat = 1;
	    }
	} else {
	    build++;
	}
	valueTokenPtr = TokenAfter(valueTokenPtr);
    }
    if (build > 0) {
	OP4(			LIST, build);
	if (concat) {
	    OP(			LIST_CONCAT);
	}
    }

    /*
     * If there was just one expanded word, we must ensure that it is a list
     * at this point. We use an [lrange ... 0 end] for this (instead of
     * [llength], as with literals) as we must drop any string representation
     * that might be hanging around.
     */

    if (concat && numWords == 2) {
	OP44(			LIST_RANGE_IMM, 0, TCL_INDEX_END);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    varTokenPtr = TokenAfter(parsePtr->tokenPtr);

    PUSH_TOKEN(			varTokenPtr, 1);
    OP(				LIST_LENGTH);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr, *listTokenPtr;
    int idx1, idx2;

    if (parsePtr->numWords != 4) {
	return TCL_ERROR;
    }
    listTokenPtr = TokenAfter(parsePtr->tokenPtr);

    tokenPtr = TokenAfter(listTokenPtr);
    if ((TclGetIndexFromToken(tokenPtr, TCL_INDEX_START, TCL_INDEX_NONE,
	    &idx1) != TCL_OK) || (idx1 == (int)TCL_INDEX_NONE)) {
	return TCL_ERROR;
    }
    /*
     * Token was an index value, and we treat all "first" indices
     * before the list same as the start of the list.
     */

    tokenPtr = TokenAfter(tokenPtr);
    if (TclGetIndexFromToken(tokenPtr, TCL_INDEX_NONE, TCL_INDEX_END,
	    &idx2) != TCL_OK) {
	return TCL_ERROR;
    }
    /*
     * Token was an index value, and we treat all "last" indices
     * after the list same as the end of the list.
     */

    /*
     * Issue instructions. It's not safe to skip doing the LIST_RANGE, as
     * we've not proved that the 'list' argument is really a list. Not that it
     * is worth trying to do that given current knowledge.
     */

    PUSH_TOKEN(			listTokenPtr, 1);
    OP44(			LIST_RANGE_IMM, idx1, idx2);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLinsertCmd --
 *
 *	How to compile the "linsert" command. We only bother with the case
 *	where the index is constant.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLinsertCmd(
    Tcl_Interp *interp,		/* Tcl interpreter for context. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the
				 * command. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *listToken, *indexToken, *tokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;

    if (numWords < 3 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /* Push list, insertion index onto the stack */
    listToken = TokenAfter(parsePtr->tokenPtr);
    indexToken = TokenAfter(listToken);

    PUSH_TOKEN(			listToken, 1);
    PUSH_TOKEN(			indexToken, 2);

    /* Push new elements to be inserted */
    tokenPtr = TokenAfter(indexToken);
    for (i=3 ; i<numWords ; i++,tokenPtr=TokenAfter(tokenPtr)) {
	PUSH_TOKEN(		tokenPtr, i);
    }

    /*
     * First operand is count of arguments.
     * Second operand is bitmask
     *  TCL_LREPLACE4_END_IS_LAST - end refers to last element
     *  TCL_LREPLACE4_SINGLE_INDEX - second index is not present
     *     indicating this is a pure insert
     */
    OP41(			LREPLACE, numWords - 1,
					TCL_LREPLACE4_SINGLE_INDEX);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLreplaceCmd --
 *
 *	How to compile the "lreplace" command. We only bother with the case
 *	where the indices are constant.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLreplaceCmd(
    Tcl_Interp *interp,		/* Tcl interpreter for context. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the
				 * command. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *listToken, *firstToken, *lastToken, *tokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;

    if (numWords < 4 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /* Push list, first, last onto the stack */
    listToken = TokenAfter(parsePtr->tokenPtr);
    firstToken = TokenAfter(listToken);
    lastToken = TokenAfter(firstToken);

    PUSH_TOKEN(			listToken, 1);
    PUSH_TOKEN(			firstToken, 2);
    PUSH_TOKEN(			lastToken, 3);

    /* Push new elements to be inserted */
    tokenPtr = TokenAfter(lastToken);
    for (i=4; i<numWords; i++,tokenPtr=TokenAfter(tokenPtr)) {
	PUSH_TOKEN(		tokenPtr, i);
    }

    /*
     * First operand is count of arguments.
     * Second operand is bitmask
     *  TCL_LREPLACE4_END_IS_LAST - end refers to last element
     */
    OP41(			LREPLACE, numWords - 1,
					TCL_LREPLACE4_END_IS_LAST);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Size tempDepth;		/* Depth used for emitting one part of the
				 * code burst. */
    Tcl_Token *varTokenPtr;	/* Pointer to the Tcl_Token representing the
				 * parse of the variable name. */
    Tcl_LVTIndex localIndex;	/* Index of var in local var table. */
    int isScalar;		/* Flag == 1 if scalar, 0 if array. */
    Tcl_Size i, numWords = parsePtr->numWords;

    /*
     * Check argument count.
     */

    /* TODO: Consider support for compiling expanded args. */
    if (numWords < 3 || numWords > UINT_MAX) {
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
    PushVarNameWord(varTokenPtr, 0, &localIndex, &isScalar, 1);

    /*
     * Push the "index" args and the new element value.
     */

    for (i=2 ; i<numWords ; ++i) {
	varTokenPtr = TokenAfter(varTokenPtr);
	PUSH_TOKEN(		varTokenPtr, i);
    }

    /*
     * Duplicate the variable name if it's been pushed.
     */

    if (localIndex < 0) {
	tempDepth = numWords - (isScalar ? 2 : 1);
	OP4(			OVER, tempDepth);
    }

    /*
     * Duplicate an array index if one's been pushed.
     */

    if (!isScalar) {
	tempDepth = numWords - (localIndex >= 0 ? 2 : 1);
	OP4(			OVER, tempDepth);
    }

    /*
     * Emit code to load the variable's value.
     */

    if (isScalar) {
	if (localIndex < 0) {
	    OP(			LOAD_STK);
	} else {
	    OP4(		LOAD_SCALAR, localIndex);
	}
    } else {
	if (localIndex < 0) {
	    OP(			LOAD_ARRAY_STK);
	} else {
	    OP4(		LOAD_ARRAY, localIndex);
	}
    }

    /*
     * Emit the correct variety of 'lset' instruction.
     */

    if (numWords == 4) {
	OP(			LSET_LIST);
    } else {
	OP4(			LSET_FLAT, numWords - 1);
    }

    /*
     * Emit code to put the value back in the variable.
     */

    if (isScalar) {
	if (localIndex < 0) {
	    OP(			STORE_STK);
	} else {
	    OP4(		STORE_SCALAR, localIndex);
	}
    } else {
	if (localIndex < 0) {
	    OP(			STORE_ARRAY_STK);
	} else {
	    OP4(		STORE_ARRAY, localIndex);
	}
    }

    return TCL_OK;
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
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
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

    OP(				NS_CURRENT);
    return TCL_OK;
}

int
TclCompileNamespaceCodeCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;

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

    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD ||
	    IS_TOKEN_PREFIXED_BY(tokenPtr, "::namespace inscope ")) {
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

    PUSH(			"::namespace");
    PUSH(			"inscope");
    OP(				NS_CURRENT);
    PUSH_TOKEN(			tokenPtr, 1);
    OP4(			LIST, 4);
    return TCL_OK;
}

int
TclCompileNamespaceOriginCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);

    PUSH_TOKEN(			tokenPtr, 1);
    OP(				ORIGIN_COMMAND);
    return TCL_OK;
}

int
TclCompileNamespaceQualifiersCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);
    Tcl_BytecodeLabel off;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    PUSH_TOKEN(			tokenPtr, 1);
    PUSH(			"0");
    PUSH(			"::");
    OP4(			OVER, 2);
    OP(				STR_FIND_LAST);
    BACKLABEL(		off);
    PUSH(			"1");
    OP(				SUB);
    OP4(			OVER, 2);
    OP4(			OVER, 1);
    OP(				STR_INDEX);
    PUSH(			":");
    OP(				STR_EQ);
    BACKJUMP(			JUMP_TRUE, off);
    OP(				STR_RANGE);
    return TCL_OK;
}

int
TclCompileNamespaceTailCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);
    Tcl_BytecodeLabel dontSkipSeparator;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    /*
     * Take care; only add 2 to found index if the string was actually found.
     */

    PUSH_TOKEN(			tokenPtr, 1);
    PUSH(			"::");
    OP4(			OVER, 1);
    OP(				STR_FIND_LAST);
    OP(				DUP);
    PUSH(			"0");
    OP(				GE);
    FWDJUMP(			JUMP_FALSE, dontSkipSeparator);
    PUSH(			"2");
    OP(				ADD);
    FWDLABEL(		dontSkipSeparator);
    PUSH(			"end");
    OP(				STR_RANGE);
    return TCL_OK;
}

int
TclCompileNamespaceUpvarCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr, *otherTokenPtr, *localTokenPtr;
    Tcl_LVTIndex localIndex;
    Tcl_Size numWords = parsePtr->numWords, i;

    if (envPtr->procPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Only compile [namespace upvar ...]: needs an even number of args, >=4
     */

    if ((numWords % 2) || numWords < 4 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * Push the namespace
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);

    /*
     * Loop over the (otherVar, thisVar) pairs. If any of the thisVar is not a
     * local variable, return an error so that the non-compiled command will
     * be called at runtime.
     */

    localTokenPtr = tokenPtr;
    for (i=2; i<numWords; i+=2) {
	otherTokenPtr = TokenAfter(localTokenPtr);
	localTokenPtr = TokenAfter(otherTokenPtr);

	PUSH_TOKEN(		otherTokenPtr, i);
	localIndex = LocalScalarFromToken(localTokenPtr, envPtr);
	if (localIndex < 0) {
	    return TCL_ERROR;
	}
	OP4(			NSUPVAR, localIndex);
    }

    /*
     * Pop the namespace, and set the result to empty
     */

    OP(				POP);
    PUSH(			"");
    return TCL_OK;
}

int
TclCompileNamespaceWhichCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Size numWords = parsePtr->numWords, idx;

    if (numWords < 2 || numWords > 3) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    idx = 1;

    /*
     * If there's an option, check that it's "-command". We don't handle
     * "-variable" (currently) and anything else is an error.
     */

    if (numWords == 3) {
	if (!IS_TOKEN_PREFIX(tokenPtr, 2, "-command")) {
	    return TCL_ERROR;
	}
	tokenPtr = TokenAfter(tokenPtr);
	idx++;
    }

    /*
     * Issue the bytecode.
     */

    PUSH_TOKEN(			tokenPtr, idx);
    OP(				RESOLVE_COMMAND);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr;	/* Pointer to the Tcl_Token representing the
				 * parse of the RE or string. */
    size_t len;
    Tcl_Size i, numWords = parsePtr->numWords;
    int nocase, exact, sawLast, simple;
    const char *str;

    /*
     * We are only interested in compiling simple regexp cases. Currently
     * supported compile cases are:
     *   regexp ?-nocase? ?--? staticString $var
     *   regexp ?-nocase? ?--? {^staticString$} $var
     */

    if (numWords < 3 || numWords > UINT_MAX) {
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

    for (i = 1; i < numWords - 2; i++) {
	varTokenPtr = TokenAfter(varTokenPtr);
	if (IS_TOKEN_LITERALLY(varTokenPtr, "--")) {
	    sawLast++;
	    i++;
	    break;
	} else if (IS_TOKEN_PREFIX(varTokenPtr, 2, "-nocase")) {
	    nocase = 1;
	} else {
	    /*
	     * Not an option we recognize or something the compiler can't see.
	     */

	    return TCL_ERROR;
	}
    }

    if (numWords - i != 2) {
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

	    PUSH(		"1");
	    return TCL_OK;
	}

	/*
	 * Attempt to convert pattern to glob.  If successful, push the
	 * converted pattern as a literal.
	 */

	if (TclReToGlob(NULL, str, len, &ds, &exact, NULL) == TCL_OK) {
	    simple = 1;
	    TclPushDString(envPtr, &ds);
	    Tcl_DStringFree(&ds);
	}
    }

    if (!simple) {
	PUSH_TOKEN(		varTokenPtr, numWords - 2);
    }

    /*
     * Push the string arg.
     */

    varTokenPtr = TokenAfter(varTokenPtr);
    PUSH_TOKEN(			varTokenPtr, numWords - 1);

    if (simple) {
	if (exact && !nocase) {
	    OP(			STR_EQ);
	} else {
	    OP1(		STR_MATCH, nocase);
	}
    } else {
	/*
	 * Pass correct RE compile flags.  We use only Int1 (8-bit), but
	 * that handles all the flags we want to pass.
	 * Don't use TCL_REG_NOSUB as we may have backrefs.
	 */

	int cflags = TCL_REG_ADVANCED | (nocase ? TCL_REG_NOCASE : 0);

	OP1(			REGEXP, cflags);
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
    TCL_UNUSED(Command *),
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
    Tcl_Size numWords = parsePtr->numWords;
    Tcl_Token *tokenPtr, *stringTokenPtr;
    Tcl_Obj *patternObj = NULL, *replacementObj = NULL;
    Tcl_DString pattern;
    const char *bytes;
    int exact, quantified, result = TCL_ERROR;
    Tcl_Size len;

    if (numWords < 5 || numWords > 6) {
	return TCL_ERROR;
    }

    /*
     * Parse the "-all", which must be the first argument (other options not
     * supported, non-"-all" substitution we can't compile).
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (!IS_TOKEN_LITERALLY(tokenPtr, "-all")) {
	return TCL_ERROR;
    }

    /*
     * Get the pattern into patternObj, checking for "--" in the process.
     */

    Tcl_DStringInit(&pattern);
    tokenPtr = TokenAfter(tokenPtr);
    TclNewObj(patternObj);
    if (!TclWordKnownAtCompileTime(tokenPtr, patternObj)) {
	goto done;
    }
    if (TclGetString(patternObj)[0] == '-') {
	if (strcmp(TclGetString(patternObj), "--") != 0 || numWords == 5) {
	    goto done;
	}
	tokenPtr = TokenAfter(tokenPtr);
	Tcl_BounceRefCount(patternObj);
	TclNewObj(patternObj);
	if (!TclWordKnownAtCompileTime(tokenPtr, patternObj)) {
	    goto done;
	}
    } else if (numWords == 6) {
	goto done;
    }

    /*
     * Identify the code which produces the string to apply the substitution
     * to (stringTokenPtr), and the replacement string (into replacementObj).
     */

    stringTokenPtr = TokenAfter(tokenPtr);
    tokenPtr = TokenAfter(stringTokenPtr);
    TclNewObj(replacementObj);
    if (!TclWordKnownAtCompileTime(tokenPtr, replacementObj)) {
	goto done;
    }

    /*
     * Next, higher-level checks. Is the RE a very simple glob? Is the
     * replacement "simple"?
     */

    bytes = TclGetStringFromObj(patternObj, &len);
    if (TclReToGlob(NULL, bytes, len, &pattern, &exact, &quantified)
	    != TCL_OK || exact || quantified) {
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
		if (len + 2 > 2) {
		    goto isSimpleGlob;
		}

		/*
		 * The pattern is "**"! I believe that should be impossible,
		 * but we definitely can't handle that at all.
		 */
	    }
	    TCL_FALLTHROUGH();
	case '\0': case '?': case '[': case '\\':
	    goto done;
	}
	bytes++;
    }
  isSimpleGlob:
    for (bytes = TclGetString(replacementObj); *bytes; bytes++) {
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
    PUSH_OBJ(			replacementObj);
    PUSH_TOKEN(			stringTokenPtr, numWords - 2);
    OP(				STR_MAP);

  done:
    Tcl_DStringFree(&pattern);
    Tcl_BounceRefCount(patternObj);
    Tcl_BounceRefCount(replacementObj);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    /*
     * General syntax: [return ?-option value ...? ?result?]
     * An even number of words means an explicit result argument is present.
     */
    int level, code, objc, status = TCL_OK;
    Tcl_Size size;
    Tcl_Size numWords = parsePtr->numWords;
    int explicitResult = (0 == (numWords % 2));
    Tcl_Size numOptionWords = numWords - 1 - explicitResult;
    Tcl_Obj *returnOpts, **objv;
    Tcl_Token *wordTokenPtr = TokenAfter(parsePtr->tokenPtr);

    if (numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * Check for special case which can always be compiled:
     *	    return -options <opts> <msg>
     * Unlike the normal [return] compilation, this version does everything at
     * runtime so it can handle arbitrary words and not just literals. Note
     * that if INST_RETURN_STK wasn't already needed for something else
     * ('finally' clause processing) this piece of code would not be present.
     */

    if ((numWords == 4) && IS_TOKEN_LITERALLY(wordTokenPtr, "-options")) {
	Tcl_Token *optsTokenPtr = TokenAfter(wordTokenPtr);
	Tcl_Token *msgTokenPtr = TokenAfter(optsTokenPtr);

	PUSH_TOKEN(		optsTokenPtr, 2);
	PUSH_TOKEN(		msgTokenPtr, 3);
	INVOKE(			RETURN_STK);
	return TCL_OK;
    }

    /*
     * Allocate some working space.
     */

    objv = (Tcl_Obj **)TclStackAlloc(interp,
	    numOptionWords * sizeof(Tcl_Obj *));

    /*
     * Scan through the return options. If any are unknown at compile time,
     * there is no value in bytecompiling. Save the option values known in an
     * objv array for merging into a return options dictionary.
     *
     * TODO: There is potential for improvement if all option keys are known
     * at compile time and all option values relating to '-code' and '-level'
     * are known at compile time.
     */

    for (objc = 0; objc < numOptionWords; objc++) {
	TclNewObj(objv[objc]);
	Tcl_IncrRefCount(objv[objc]);
	if (!TclWordKnownAtCompileTime(wordTokenPtr, objv[objc])) {
	    /*
	     * Non-literal, so punt to run-time assembly of the dictionary.
	     */

	    for (; objc>=0 ; objc--) {
		TclDecrRefCount(objv[objc]);
	    }
	    TclStackFree(interp, objv);
	    goto issueRuntimeReturn;
	}
	wordTokenPtr = TokenAfter(wordTokenPtr);
    }
    status = TclMergeReturnOptions(interp, objc, objv,
	    &returnOpts, &code, &level);
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
	PUSH_TOKEN(		wordTokenPtr, numWords - 1);
    } else {
	/*
	 * No explict result argument, so default result is empty string.
	 */

	PUSH(			"");
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

	Tcl_ExceptionRange index = envPtr->exceptArrayNext - 1;
	int enclosingCatch = 0;

	while (index >= 0) {
	    const ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[index];

	    if ((rangePtr->type == CATCH_EXCEPTION_RANGE)
		    && (rangePtr->catchOffset == TCL_INDEX_NONE)) {
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
	    OP(			DONE);
	    STKDELTA(+1);
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

  issueRuntimeReturn:
    /*
     * Assemble the option dictionary (as a list as that's good enough).
     */

    wordTokenPtr = TokenAfter(parsePtr->tokenPtr);
    for (objc=1 ; objc<=numOptionWords ; objc++) {
	PUSH_TOKEN(		wordTokenPtr, objc);
	wordTokenPtr = TokenAfter(wordTokenPtr);
    }
    OP4(			LIST, numOptionWords);

    /*
     * Push the result.
     */

    if (explicitResult) {
	PUSH_TOKEN(		wordTokenPtr, numWords - 1);
    } else {
	PUSH(			"");
    }

    /*
     * Issue the RETURN itself.
     */

    INVOKE(			RETURN_STK);
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
    if (level == 0 && (code == TCL_BREAK || code == TCL_CONTINUE)) {
	ExceptionRange *rangePtr;
	ExceptionAux *exceptAux;

	rangePtr = TclGetInnermostExceptionRange(envPtr, code, &exceptAux);
	if (rangePtr && rangePtr->type == LOOP_EXCEPTION_RANGE) {
	    TclCleanupStackForBreakContinue(envPtr, exceptAux);
	    if (code == TCL_BREAK) {
		TclAddLoopBreakFixup(envPtr, exceptAux);
	    } else {
		TclAddLoopContinueFixup(envPtr, exceptAux);
	    }
	    Tcl_DecrRefCount(returnOpts);
	    return;
	}
    }

    PUSH_OBJ(			returnOpts);
    TclEmitInstInt44(op, code, level, envPtr);
}

void
TclCompileSyntaxError(
    Tcl_Interp *interp,
    CompileEnv *envPtr)
{
    Tcl_Obj *msg = Tcl_GetObjResult(interp);
    Tcl_Size numBytes;
    const char *bytes = TclGetStringFromObj(msg, &numBytes);

    TclErrorStackResetIf(interp, bytes, numBytes);
    PUSH_OBJ(			msg);
    CompileReturnInternal(envPtr, INST_SYNTAX, TCL_ERROR, 0,
	    TclNoErrorStack(interp, Tcl_GetReturnOptions(interp, TCL_ERROR)));
    Tcl_ResetResult(interp);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr, *otherTokenPtr, *localTokenPtr;
    Tcl_LVTIndex localIndex;
    Tcl_Size numWords = parsePtr->numWords, i;
    Tcl_Obj *objPtr;

    if (envPtr->procPtr == NULL) {
	return TCL_ERROR;
    }

    if (numWords < 3 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * Push the frame index if it is known at compile time
     */

    TclNewObj(objPtr);
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
	    if (numWords % 2) {
		return TCL_ERROR;
	    }
	    /* TODO: Push the known value instead? */
	    PUSH_TOKEN(		tokenPtr, 1);
	    otherTokenPtr = TokenAfter(tokenPtr);
	    i = 2;
	} else {
	    if (!(numWords % 2)) {
		return TCL_ERROR;
	    }
	    PUSH(		"1");
	    otherTokenPtr = tokenPtr;
	    i = 1;
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

    for (; i<numWords; i+=2, otherTokenPtr = TokenAfter(localTokenPtr)) {
	localTokenPtr = TokenAfter(otherTokenPtr);

	PUSH_TOKEN(		otherTokenPtr, i);
	localIndex = LocalScalarFromToken(localTokenPtr, envPtr);
	if (localIndex < 0) {
	    return TCL_ERROR;
	}
	OP4(			UPVAR, localIndex);
    }

    /*
     * Pop the frame index, and set the result to empty
     */

    OP(				POP);
    PUSH(			"");
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *valueTokenPtr;
    Tcl_LVTIndex localIndex;
    Tcl_Size numWords = parsePtr->numWords, i;

    if (numWords < 2 || numWords > UINT_MAX) {
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
    for (i=1; i<numWords; i+=2) {
	varTokenPtr = TokenAfter(valueTokenPtr);
	valueTokenPtr = TokenAfter(varTokenPtr);

	localIndex = IndexTailVarIfKnown(interp, varTokenPtr, envPtr);

	if (localIndex < 0) {
	    return TCL_ERROR;
	}

	/* TODO: Consider what value can pass through the
	 * IndexTailVarIfKnown() screen.  Full CompileWord()
	 * likely does not apply here.  Push known value instead. */
	PUSH_TOKEN(		varTokenPtr, i);
	OP4(			VARIABLE, localIndex);

	if (i + 1 < numWords) {
	    /*
	     * A value has been given: set the variable, pop the value
	     */

	    PUSH_TOKEN(		valueTokenPtr, i + 1);
	    OP4(		STORE_SCALAR, localIndex);
	    OP(			POP);
	}
    }

    /*
     * Set the result to empty
     */

    PUSH(			"");
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

static Tcl_LVTIndex
IndexTailVarIfKnown(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Token *varTokenPtr,	/* Token representing the variable name */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Obj *tailPtr;
    const char *tailName, *p;
    Tcl_Size n = varTokenPtr->numComponents, len;
    Tcl_Token *lastTokenPtr;
    int full;
    Tcl_LVTIndex localIndex;

    /*
     * Determine if the tail is (a) known at compile time, and (b) not an
     * array element. Should any of these fail, return an error so that the
     * non-compiled command will be called at runtime.
     *
     * In order for the tail to be known at compile time, the last token in
     * the word has to be constant and contain "::" if it is not the only one.
     */

    if (!EnvHasLVT(envPtr)) {
	return TCL_INDEX_NONE;
    }

    TclNewObj(tailPtr);
    if (TclWordKnownAtCompileTime(varTokenPtr, tailPtr)) {
	full = 1;
	lastTokenPtr = varTokenPtr;
    } else {
	full = 0;
	lastTokenPtr = varTokenPtr + n;

	if (lastTokenPtr->type != TCL_TOKEN_TEXT) {
	    Tcl_DecrRefCount(tailPtr);
	    return TCL_INDEX_NONE;
	}
	Tcl_SetStringObj(tailPtr, lastTokenPtr->start, lastTokenPtr->size);
    }

    tailName = TclGetStringFromObj(tailPtr, &len);

    if (len) {
	if (*(tailName + len - 1) == ')') {
	    /*
	     * Possible array: bail out
	     */

	    Tcl_DecrRefCount(tailPtr);
	    return TCL_INDEX_NONE;
	}

	/*
	 * Get the tail: immediately after the last '::'
	 */

	for (p = tailName + len - 1; p > tailName; p--) {
	    if ((p[0] == ':') && (p[- 1] == ':')) {
		p++;
		break;
	    }
	}
	if (!full && (p == tailName)) {
	    /*
	     * No :: in the last component.
	     */

	    Tcl_DecrRefCount(tailPtr);
	    return TCL_INDEX_NONE;
	}
	len -= p - tailName;
	tailName = p;
    }

    localIndex = TclFindCompiledLocal(tailName, len, 1, envPtr);
    Tcl_DecrRefCount(tailPtr);
    return localIndex;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclCompileObjectNextCmd, TclCompileObjectSelfCmd --
 *
 *	Compilations of the TclOO utility commands [next] and [self].
 *
 * ----------------------------------------------------------------------
 */

int
TclCompileObjectNextCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    Tcl_Size i;
    /* TODO: Consider support for compiling expanded args. */

    if (parsePtr->numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    for (i=0 ; i<parsePtr->numWords ; i++) {
	PUSH_TOKEN(		tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }
    INVOKE4(			TCLOO_NEXT, i);
    return TCL_OK;
}

int
TclCompileObjectNextToCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    Tcl_Size i, numWords = parsePtr->numWords;
    /* TODO: Consider support for compiling expanded args. */

    if (numWords < 2 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    for (i=0 ; i<numWords ; i++) {
	PUSH_TOKEN(		tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }
    INVOKE4(			TCLOO_NEXT_CLASS, i);
    return TCL_OK;
}

int
TclCompileObjectSelfCmd(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * We only handle [self], [self object] (which is the same operation) and
     * [self namespace]. These are the only very common operations on [self]
     * for which bytecoding is at all reasonable, with [self namespace] being
     * just because it is convenient with ops we already have.
     */

    if (parsePtr->numWords == 1) {
	goto compileSelfObject;
    } else if (parsePtr->numWords == 2) {
	const Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);

	if (IS_TOKEN_PREFIX(tokenPtr, 1, "object")) {
	    goto compileSelfObject;
	} else if (IS_TOKEN_PREFIX(tokenPtr, 1, "namespace")) {
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

    OP(				TCLOO_SELF);
    return TCL_OK;

  compileSelfNamespace:

    /*
     * This is formally only correct with TclOO methods as they are currently
     * implemented; it assumes that the current namespace is invariably when a
     * TclOO context is present is the object's namespace, and that's
     * technically only something that's a matter of current policy. But it
     * avoids creating another opcode, so that's all good!
     */

    OP(				TCLOO_SELF);
    OP(				POP);
    OP(				NS_CURRENT);
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
