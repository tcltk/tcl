#include "tclEngineInt.h"

/*
 * How to compile a subcommand using its own command compiler. To do that, we
 * have to perform some trickery to rewrite the arguments, as compilers *must*
 * have parse tokens that refer to addresses in the original script.
 */

int
TclCompileToCompiledCommand(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    int depth,
    Command *cmdPtr,
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Parse synthetic;
    Tcl_Token *tokenPtr;
    int result, i;
    int savedNumCmds = envPtr->numCommands;
    int savedStackDepth = envPtr->currStackDepth;
    unsigned savedCodeNext = envPtr->codeNext - envPtr->codeStart;

    if (cmdPtr->compileProc == NULL) {
	return TCL_ERROR;
    }

    TclParseInit(interp, NULL, 0, &synthetic);
    synthetic.numWords = parsePtr->numWords - depth + 1;
    TclGrowParseTokenArray(&synthetic, 2);
    synthetic.numTokens = 2;

    /*
     * Now we have the space to work in, install something rewritten. The
     * first word will "officially" be the bytes of the structured ensemble
     * name. That's technically wrong, but nobody will care; we just need
     * *something* here...
     */

    synthetic.tokenPtr[0].type = TCL_TOKEN_SIMPLE_WORD;
    synthetic.tokenPtr[0].start = parsePtr->tokenPtr[0].start;
    synthetic.tokenPtr[0].numComponents = 1;
    synthetic.tokenPtr[1].type = TCL_TOKEN_TEXT;
    synthetic.tokenPtr[1].start = parsePtr->tokenPtr[0].start;
    synthetic.tokenPtr[1].numComponents = 0;
    for (i=0,tokenPtr=parsePtr->tokenPtr ; i<depth ; i++) {
	int sclen = (tokenPtr->start - synthetic.tokenPtr[0].start)
		+ tokenPtr->size;

	synthetic.tokenPtr[0].size = sclen;
	synthetic.tokenPtr[1].size = sclen;
	tokenPtr = TokenAfter(tokenPtr);
    }

    /*
     * Copy over the real argument tokens.
     */

    for (i=1; i<synthetic.numWords; i++) {
	int toCopy;

	toCopy = tokenPtr->numComponents + 1;
	TclGrowParseTokenArray(&synthetic, toCopy);
	memcpy(synthetic.tokenPtr + synthetic.numTokens, tokenPtr,
		sizeof(Tcl_Token) * toCopy);
	synthetic.numTokens += toCopy;
	tokenPtr = TokenAfter(tokenPtr);
    }

    /*
     * Hand off compilation to the subcommand compiler. At last!
     */

    result = cmdPtr->compileProc(interp, &synthetic, cmdPtr, envPtr);

    /*
     * If our target fails to compile, revert the number of commands and the
     * pointer to the place to issue the next instruction. [Bug 3600328]
     */

    if (result != TCL_OK) {
	envPtr->numCommands = savedNumCmds;
	envPtr->currStackDepth = savedStackDepth;
	envPtr->codeNext = envPtr->codeStart + savedCodeNext;
    }

    /*
     * Clean up if necessary.
     */

    Tcl_FreeParse(&synthetic);
    return result;
}

/*
 * How to compile a subcommand to a _replacing_ invoke of its implementation
 * command.
 */

void
TclCompileToInvokedCommand(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Tcl_Obj *replacements,
    Command *cmdPtr,
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokPtr;
    Tcl_Obj *objPtr, **words;
    char *bytes;
    int length, i, numWords, cmdLit;

    /*
     * Push the words of the command. Take care; the command words may be
     * scripts that have backslashes in them, and [info frame 0] can see the
     * difference. Hence the call to TclContinuationsEnterDerived...
     */

    Tcl_ListObjGetElements(NULL, replacements, &numWords, &words);
    for (i=0,tokPtr=parsePtr->tokenPtr ; i<parsePtr->numWords ; i++) {
	if (i > 0 && i < numWords+1) {
	    bytes = Tcl_GetStringFromObj(words[i-1], &length);
	    PushLiteral(envPtr, bytes, length);
	} else if (tokPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    int literal = TclRegisterNewLiteral(envPtr,
		    tokPtr[1].start, tokPtr[1].size);

	    TclEmitPush(literal, envPtr);
	} else {
	    CompileTokens(envPtr, tokPtr, interp);
	}
	tokPtr = TokenAfter(tokPtr);
    }

    /*
     * Push the name of the command we're actually dispatching to as part of
     * the implementation.
     */

    objPtr = Tcl_NewObj();
    Tcl_GetCommandFullName(interp, (Tcl_Command) cmdPtr, objPtr);
    bytes = Tcl_GetStringFromObj(objPtr, &length);
    cmdLit = TclRegisterNewCmdLiteral(envPtr, bytes, length);
    TclSetCmdNameObj(interp, envPtr->literalArrayPtr[cmdLit].objPtr, cmdPtr);
    TclEmitPush(cmdLit, envPtr);
    TclDecrRefCount(objPtr);

    /*
     * Do the replacing dispatch.
     */

    TclEmitInstInt4(INST_INVOKE_REPLACE, parsePtr->numWords, envPtr);
    TclEmitInt1(numWords+1, envPtr);
    TclAdjustStackDepth(-1, envPtr);	/* Correction to stack depth calcs. */
}

/*
 * Helpers that do issuing of instructions for commands that "don't have
 * compilers" (well, they do; these). They all work by just generating base
 * code to invoke the command; they're intended for ensemble subcommands so
 * that the costs of INST_INVOKE_REPLACE can be avoided where we can work out
 * that they're not needed.
 *
 * Note that these are NOT suitable for commands where there's an argument
 * that is a script, as an [info level] or [info frame] in the inner context
 * can see the difference.
 */

int
TclCompileBasicNArgCommand(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;
    Tcl_Obj *objPtr;
    char *bytes;
    int length, i, literal;

    /*
     * Push the name of the command we're actually dispatching to as part of
     * the implementation.
     */

    objPtr = Tcl_NewObj();
    Tcl_GetCommandFullName(interp, (Tcl_Command) cmdPtr, objPtr);
    bytes = Tcl_GetStringFromObj(objPtr, &length);
    literal = TclRegisterNewCmdLiteral(envPtr, bytes, length);
    TclSetCmdNameObj(interp, envPtr->literalArrayPtr[literal].objPtr, cmdPtr);
    TclEmitPush(literal, envPtr);
    TclDecrRefCount(objPtr);

    /*
     * Push the words of the command.
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    for (i=1 ; i<parsePtr->numWords ; i++) {
	if (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    PushLiteral(envPtr, tokenPtr[1].start, tokenPtr[1].size);
	} else {
	    CompileTokens(envPtr, tokenPtr, interp);
	}
	tokenPtr = TokenAfter(tokenPtr);
    }

    /*
     * Do the standard dispatch.
     */

    if (i <= 255) {
	TclEmitInstInt1(INST_INVOKE_STK1, i, envPtr);
    } else {
	TclEmitInstInt4(INST_INVOKE_STK4, i, envPtr);
    }
    return TCL_OK;
}

int
TclCompileBasic0ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 1) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic1ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic2ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic3ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 4) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic0Or1ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 1 && parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic1Or2ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 2 && parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic2Or3ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 3 && parsePtr->numWords != 4) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic0To2ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords < 1 || parsePtr->numWords > 3) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic1To3ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords < 2 || parsePtr->numWords > 4) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasicMin0ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords < 1) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasicMin1ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords < 2) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasicMin2ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords < 3) {
	return TCL_ERROR;
    }

    return TclCompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * TclCompileEnsemble --
 *
 *	Procedure called to compile an ensemble command. Note that most
 *	ensembles are not compiled, since modifying a compiled ensemble causes
 *	a invalidation of all existing bytecode (expensive!) which is not
 *	normally warranted.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the subcommands of the
 *	ensemble at runtime if a compile-time mapping is possible.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileEnsemble(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);
    Tcl_Obj *mapObj, *subcmdObj, *targetCmdObj, *listObj, **elems;
    Tcl_Obj *replaced = Tcl_NewObj(), *replacement;
    Tcl_Command ensemble = (Tcl_Command) cmdPtr;
    Command *oldCmdPtr = cmdPtr, *newCmdPtr;
    int len, result, flags = 0, i, depth = 1, invokeAnyway = 0;
    int ourResult = TCL_ERROR;
    unsigned numBytes;
    const char *word;

    Tcl_IncrRefCount(replaced);

    /*
     * This is where we return to if we are parsing multiple nested compiled
     * ensembles. [info object] is such a beast.
     */

  checkNextWord:
    if (parsePtr->numWords < depth + 1) {
	goto failed;
    }
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	/*
	 * Too hard.
	 */

	goto failed;
    }

    word = tokenPtr[1].start;
    numBytes = tokenPtr[1].size;

    /*
     * There's a sporting chance we'll be able to compile this. But now we
     * must check properly. To do that, check that we're compiling an ensemble
     * that has a compilable command as its appropriate subcommand.
     */

    if (Tcl_GetEnsembleMappingDict(NULL, ensemble, &mapObj) != TCL_OK
	    || mapObj == NULL) {
	/*
	 * Either not an ensemble or a mapping isn't installed. Crud. Too hard
	 * to proceed.
	 */

	goto failed;
    }

    /*
     * Also refuse to compile anything that uses a formal parameter list for
     * now, on the grounds that it is too complex.
     */

    if (Tcl_GetEnsembleParameterList(NULL, ensemble, &listObj) != TCL_OK
	    || listObj != NULL) {
	/*
	 * Figuring out how to compile this has become too much. Bail out.
	 */

	goto failed;
    }

    /*
     * Next, get the flags. We need them on several code paths so that we can
     * know whether we're to do prefix matching.
     */

    (void) Tcl_GetEnsembleFlags(NULL, ensemble, &flags);

    /*
     * Check to see if there's also a subcommand list; must check to see if
     * the subcommand we are calling is in that list if it exists, since that
     * list filters the entries in the map.
     */

    (void) Tcl_GetEnsembleSubcommandList(NULL, ensemble, &listObj);
    if (listObj != NULL) {
	int sclen;
	const char *str;
	Tcl_Obj *matchObj = NULL;

	if (Tcl_ListObjGetElements(NULL, listObj, &len, &elems) != TCL_OK) {
	    goto failed;
	}
	for (i=0 ; i<len ; i++) {
	    str = Tcl_GetStringFromObj(elems[i], &sclen);
	    if ((sclen == (int) numBytes) && !memcmp(word, str, numBytes)) {
		/*
		 * Exact match! Excellent!
		 */

		result = Tcl_DictObjGet(NULL, mapObj,elems[i], &targetCmdObj);
		if (result != TCL_OK || targetCmdObj == NULL) {
		    goto failed;
		}
		replacement = elems[i];
		goto doneMapLookup;
	    }

	    /*
	     * Check to see if we've got a prefix match. A single prefix match
	     * is fine, and allows us to refine our dictionary lookup, but
	     * multiple prefix matches is a Bad Thing and will prevent us from
	     * making progress. Note that we cannot do the lookup immediately
	     * in the prefix case; might be another entry later in the list
	     * that causes things to fail.
	     */

	    if ((flags & TCL_ENSEMBLE_PREFIX)
		    && strncmp(word, str, numBytes) == 0) {
		if (matchObj != NULL) {
		    goto failed;
		}
		matchObj = elems[i];
	    }
	}
	if (matchObj == NULL) {
	    goto failed;
	}
	result = Tcl_DictObjGet(NULL, mapObj, matchObj, &targetCmdObj);
	if (result != TCL_OK || targetCmdObj == NULL) {
	    goto failed;
	}
	replacement = matchObj;
    } else {
	Tcl_DictSearch s;
	int done, matched;
	Tcl_Obj *tmpObj;

	/*
	 * No map, so check the dictionary directly.
	 */

	TclNewStringObj(subcmdObj, word, (int) numBytes);
	result = Tcl_DictObjGet(NULL, mapObj, subcmdObj, &targetCmdObj);
	if (result == TCL_OK && targetCmdObj != NULL) {
	    /*
	     * Got it. Skip the fiddling around with prefixes.
	     */

	    replacement = subcmdObj;
	    goto doneMapLookup;
	}
	TclDecrRefCount(subcmdObj);

	/*
	 * We've not literally got a valid subcommand. But maybe we have a
	 * prefix. Check if prefix matches are allowed.
	 */

	if (!(flags & TCL_ENSEMBLE_PREFIX)) {
	    goto failed;
	}

	/*
	 * Iterate over the keys in the dictionary, checking to see if we're a
	 * prefix.
	 */

	Tcl_DictObjFirst(NULL, mapObj, &s, &subcmdObj, &tmpObj, &done);
	matched = 0;
	replacement = NULL;		/* Silence, fool compiler! */
	while (!done) {
	    if (strncmp(TclGetString(subcmdObj), word, numBytes) == 0) {
		if (matched++) {
		    /*
		     * Must have matched twice! Not unique, so no point
		     * looking further.
		     */

		    break;
		}
		replacement = subcmdObj;
		targetCmdObj = tmpObj;
	    }
	    Tcl_DictObjNext(&s, &subcmdObj, &tmpObj, &done);
	}
	Tcl_DictObjDone(&s);

	/*
	 * If we have anything other than a single match, we've failed the
	 * unique prefix check.
	 */

	if (matched != 1) {
	    invokeAnyway = 1;
	    goto failed;
	}
    }

    /*
     * OK, we definitely map to something. But what?
     *
     * The command we map to is the first word out of the map element. Note
     * that we also reject dealing with multi-element rewrites if we are in a
     * safe interpreter, as there is otherwise a (highly gnarly!) way to make
     * Tcl crash open to exploit.
     */

  doneMapLookup:
    Tcl_ListObjAppendElement(NULL, replaced, replacement);
    if (Tcl_ListObjGetElements(NULL, targetCmdObj, &len, &elems) != TCL_OK) {
	goto failed;
    } else if (len != 1) {
	/*
	 * Note that at this point we know we can't issue any special
	 * instruction sequence as the mapping isn't one that we support at
	 * the compiled level.
	 */

	goto cleanup;
    }
    targetCmdObj = elems[0];

    oldCmdPtr = cmdPtr;
    Tcl_IncrRefCount(targetCmdObj);
    newCmdPtr = (Command *) Tcl_GetCommandFromObj(interp, targetCmdObj);
    TclDecrRefCount(targetCmdObj);
    if (newCmdPtr == NULL || Tcl_IsSafe(interp)
	    || newCmdPtr->nsPtr->flags & NS_SUPPRESS_COMPILATION
	    || newCmdPtr->flags & CMD_HAS_EXEC_TRACES
	    || ((Interp *)interp)->flags & DONT_COMPILE_CMDS_INLINE) {
	/*
	 * Maps to an undefined command or a command without a compiler.
	 * Cannot compile.
	 */

	goto cleanup;
    }
    cmdPtr = newCmdPtr;
    depth++;

    /*
     * See whether we have a nested ensemble. If we do, we can go round the
     * mulberry bush again, consuming the next word.
     */

    if (cmdPtr->compileProc == TclCompileEnsemble) {
	tokenPtr = TokenAfter(tokenPtr);
	ensemble = (Tcl_Command) cmdPtr;
	goto checkNextWord;
    }

    /*
     * Now we've done the mapping process, can now actually try to compile.
     * If there is a subcommand compiler and that successfully produces code,
     * we'll use that. Otherwise, we fall back to generating opcodes to do the
     * invoke at runtime.
     */

    invokeAnyway = 1;
    if (TclCompileToCompiledCommand(interp, parsePtr, depth, cmdPtr,
	    envPtr) == TCL_OK) {
	ourResult = TCL_OK;
	goto cleanup;
    }

    /*
     * Failed to do a full compile for some reason. Try to do a direct invoke
     * instead of going through the ensemble lookup process again.
     */

  failed:
    if (depth < 250) {
	if (depth > 1) {
	    if (!invokeAnyway) {
		cmdPtr = oldCmdPtr;
		depth--;
	    }
	    (void) Tcl_ListObjReplace(NULL, replaced, depth, 2, 0, NULL);
	}
	TclCompileToInvokedCommand(interp, parsePtr, replaced, cmdPtr, envPtr);
	ourResult = TCL_OK;
    }

    /*
     * Release the memory we allocated. If we've got here, we've either done
     * something useful or we're in a case that we can't compile at all and
     * we're just giving up.
     */

  cleanup:
    Tcl_DecrRefCount(replaced);
    return ourResult;
}

