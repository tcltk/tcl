/* 
 * tclCompCmds.c --
 *
 *	This file contains compilation procedures that compile various
 *	Tcl commands into a sequence of instructions ("bytecodes"). 
 *
 * Copyright (c) 1997-1998 Sun Microsystems, Inc.
 * Copyright (c) 2001 by Kevin B. Kenny.  All rights reserved.
 * Copyright (c) 2002 ActiveState Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclCompCmds.c,v 1.59.4.14 2005/04/11 00:40:29 msofer Exp $
 */

#include "tclInt.h"
#include "tclCompile.h"

/*
 * Prototypes for procedures defined later in this file:
 */

static int              CompileSetCmdInternal _ANSI_ARGS_((Tcl_Interp *interp,
			Tcl_Parse *parsePtr, CompileEnv *envPtr, int varFlags));
static ClientData	DupForeachInfo _ANSI_ARGS_((ClientData clientData));
static void		FreeForeachInfo _ANSI_ARGS_((ClientData clientData));
static int		PushVarName _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_Token *varTokenPtr, CompileEnv *envPtr, int flags,
	int *localIndexPtr, int *simpleVarNamePtr, int *isScalarPtr));

/*
 * Flags bits used by PushVarName.
 */

#define TCL_CREATE_VAR     1 /* Create a compiled local if none is found */

/*
 * The structures below define the AuxData types defined in this file.
 */

AuxDataType tclForeachInfoType = {
    "ForeachInfo",				/* name */
    DupForeachInfo,				/* dupProc */
    FreeForeachInfo				/* freeProc */
};


/*
 *----------------------------------------------------------------------
 *
 * TclCompileAppendCmd --
 *
 *	Procedure called to compile the "append" command.
 *
 * Results:
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "append" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileAppendCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int numWords;
    int flags = TCL_APPEND_VALUE;

    numWords = parsePtr->numWords;
    if (numWords == 1) {
        return TCL_OUT_LINE_COMPILE;
    } else if (numWords == 2) {
	/*
	 * append varName == set varName
	 */
        return TclCompileSetCmd(interp, parsePtr, envPtr);
    } else if (numWords > 3) {
	/*
	 * APPEND instructions currently only handle one value
	 */
        return TCL_OUT_LINE_COMPILE;
    }

    return CompileSetCmdInternal(interp, parsePtr, envPtr, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileBreakCmd --
 *
 *	Procedure called to compile the "break" command.
 *
 * Results:
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "break" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileBreakCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    if (parsePtr->numWords != 1) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Emit a break instruction.
     */

    TclEmitInst1(INST_BREAK, envPtr->exceptArrayCurr, envPtr);
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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "catch" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileCatchCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *cmdTokenPtr, *nameTokenPtr;
    CONST char *name;
    int localIndex, nameChars, startOffset;
    int savedStackDepth = envPtr->currStackDepth;
    int savedOpenRange = envPtr->exceptArrayCurr;

    
    /*
     * If syntax does not match what we expect for [catch], do not
     * compile.  Let runtime checks determine if syntax has changed.
     */
    if ((parsePtr->numWords != 2) && (parsePtr->numWords != 3)) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * If a variable was specified and the catch command is at global level
     * (not in a procedure), don't compile it inline: the payoff is
     * too small.
     */

    if ((parsePtr->numWords == 3) && (envPtr->procPtr == NULL)) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Make sure the variable name, if any, has no substitutions and just
     * refers to a local scaler.
     */

    localIndex = -1;
    cmdTokenPtr = parsePtr->tokenPtr
	    + (parsePtr->tokenPtr->numComponents + 1);
    if (parsePtr->numWords == 3) {
	nameTokenPtr = cmdTokenPtr + (cmdTokenPtr->numComponents + 1);
	if (nameTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    name = nameTokenPtr[1].start;
	    nameChars = nameTokenPtr[1].size;
	    if (!TclIsLocalScalar(name, nameChars)) {
		return TCL_OUT_LINE_COMPILE;
	    }
	    localIndex = TclFindCompiledLocal(nameTokenPtr[1].start,
		    nameTokenPtr[1].size, /*create*/ 1, 
		    /*flags*/ 0, envPtr->procPtr);
	} else {
	   return TCL_OUT_LINE_COMPILE;
	}
    }

    /* 
     * If the body is not a simple word, compile the instructions
     * to generate it outside the catch range.
     */
    
    if (cmdTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	/*
	 * REMARK: this will store an off-by-one stack depth in the
	 * catchStack: we rely on INST_EVAL_STK to pop its argument before
	 * going to checkForCatch. 
	 */
	
	TclCompileTokens(interp, cmdTokenPtr+1,
	        cmdTokenPtr->numComponents, envPtr);
	TclSetStackDepth((savedStackDepth+1), envPtr);
    }

    /*
     * We will compile the catch command. Emit a beginCatch instruction at
     * the start of the catch body: the subcommand it controls.
     */

    envPtr->catchDepth++;
    envPtr->maxCatchDepth =
	TclMax(envPtr->catchDepth, envPtr->maxCatchDepth);
    envPtr->exceptArrayCurr = -2;
    
    /*
     * Emit the instructions to eval the body. The INST_BEGIN_CATCH
     * operand will be the set later to the distance to the INST_END_CATCH. 
     */
    
    startOffset = (envPtr->codeNext - envPtr->codeStart);
    TclEmitInst1(INST_BEGIN_CATCH, 0, envPtr);
    if (cmdTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	TclCompileCmdWord(interp, cmdTokenPtr+1, 1, envPtr);
    } else {
	TclEmitInst0(INST_EVAL_STK, envPtr);
    }
    TclSetStackDepth((savedStackDepth+1), envPtr);

    /*
     * Store the offset between INST_BEGIN_CATCH and INST_END_CATCH at the
     * BEGIN instruction, then emit the END instruction.
     */

    TclSetJumpTarget(envPtr, startOffset);
    TclEmitInst1(INST_END_CATCH, localIndex, envPtr);

    TclSetStackDepth((savedStackDepth+1), envPtr);
    envPtr->catchDepth--;
    envPtr->exceptArrayCurr = savedOpenRange;
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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "continue" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileContinueCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    /*
     * There should be no argument after the "continue".
     */

    if (parsePtr->numWords != 1) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Emit a continue instruction.
     */

    TclEmitInst1(INST_CONTINUE, envPtr->exceptArrayCurr, envPtr);
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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "expr" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileExprCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *firstWordPtr;

    if (parsePtr->numWords == 1) {
	return TCL_OUT_LINE_COMPILE;
    }

    firstWordPtr = parsePtr->tokenPtr
	    + (parsePtr->tokenPtr->numComponents + 1);
    TclCompileExprWords(interp, firstWordPtr, (parsePtr->numWords-1), envPtr);
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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "for" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */
int
TclCompileForCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *startTokenPtr, *testTokenPtr, *nextTokenPtr, *bodyTokenPtr;
    int jumpEvalCondOffset;
    int bodyCodeOffset, nextCodeOffset, jumpDist;
    int bodyRange, nextRange;
    int savedStackDepth = envPtr->currStackDepth;

    if (parsePtr->numWords != 5) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * If the test expression requires substitutions, don't compile the for
     * command inline. E.g., the expression might cause the loop to never
     * execute or execute forever, as in "for {} "$x > 5" {incr x} {}".
     */

    startTokenPtr = parsePtr->tokenPtr
	    + (parsePtr->tokenPtr->numComponents + 1);
    testTokenPtr = startTokenPtr + (startTokenPtr->numComponents + 1);
    if (testTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Bail out also if the body or the next expression require substitutions
     * in order to insure correct behaviour [Bug 219166]
     */

    nextTokenPtr = testTokenPtr + (testTokenPtr->numComponents + 1);
    bodyTokenPtr = nextTokenPtr + (nextTokenPtr->numComponents + 1);
    if ((nextTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) 
	    || (bodyTokenPtr->type != TCL_TOKEN_SIMPLE_WORD)) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Inline compile the initial command.
     */

    TclCompileCmdWord(interp, startTokenPtr+1,
	    startTokenPtr->numComponents, envPtr);
    TclEmitInst0(INST_POP, envPtr);

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

    TclEmitForwardJump(envPtr, INST_JUMP, jumpEvalCondOffset);

    /*
     * Compile the loop body.
     */

    bodyRange = TclBeginExceptRange(envPtr);
    bodyCodeOffset = (envPtr->codeNext - envPtr->codeStart);

    TclCompileCmdWord(interp, bodyTokenPtr+1,
	    bodyTokenPtr->numComponents, envPtr);

    TclEndExceptRange(bodyRange, envPtr);
    TclSetStackDepth((savedStackDepth+1), envPtr);
    TclEmitInst0(INST_POP, envPtr);


    /*
     * Compile the "next" subcommand.
     */

    nextRange = TclBeginExceptRange(envPtr);
    nextCodeOffset = (envPtr->codeNext - envPtr->codeStart);

    TclCompileCmdWord(interp, nextTokenPtr+1,
	    nextTokenPtr->numComponents, envPtr);

    TclEndExceptRange(nextRange, envPtr);
    TclSetStackDepth((savedStackDepth+1), envPtr);
    TclEmitInst0(INST_POP, envPtr);

    /*
     * Compile the test expression then emit the conditional jump that
     * terminates the for.
     */

    TclSetJumpTarget(envPtr, jumpEvalCondOffset);

    TclCompileExprWords(interp, testTokenPtr, 1, envPtr);
    TclSetStackDepth((savedStackDepth+1), envPtr);

    jumpDist = (envPtr->codeNext - envPtr->codeStart) - bodyCodeOffset;
    TclEmitInst1(INST_JUMP_TRUE, -jumpDist, envPtr);

    /*
     * Set the loop's break and continue targets.
     */

    envPtr->exceptArrayPtr[bodyRange].continueOffset = nextCodeOffset;

    envPtr->exceptArrayPtr[bodyRange].breakOffset =
            envPtr->exceptArrayPtr[nextRange].breakOffset =
	    (envPtr->codeNext - envPtr->codeStart);

    /*
     * The for command's result is an empty string.
     */

    TclSetStackDepth(savedStackDepth, envPtr);
    TclEmitPush(TclRegisterNewLiteral(envPtr, "", 0), envPtr);

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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "foreach" command
 *	at runtime.
 *
n*----------------------------------------------------------------------
 */

int
TclCompileForeachCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Proc *procPtr = envPtr->procPtr;
    ForeachInfo *infoPtr;	/* Points to the structure describing this
				 * foreach command. Stored in a AuxData
				 * record in the ByteCode. */
    int firstValueTemp;		/* Index of the first temp var in the frame
				 * used to point to a value list. */
    int loopCtTemp;		/* Index of temp var holding the loop's
				 * iteration count. */
    Tcl_Token *tokenPtr, *bodyTokenPtr;
    int infoIndex, range;
    int numWords, numLists, numVars, loopIndex, tempVar, i, j, code;
    int savedStackDepth = envPtr->currStackDepth;
    int bodyOffset;
    
    /*
     * We parse the variable list argument words and create two arrays:
     *    varcList[i] is number of variables in i-th var list
     *    varvList[i] points to array of var names in i-th var list
     */

#define STATIC_VAR_LIST_SIZE 5
    int varcListStaticSpace[STATIC_VAR_LIST_SIZE];
    CONST char **varvListStaticSpace[STATIC_VAR_LIST_SIZE];
    int *varcList = varcListStaticSpace;
    CONST char ***varvList = varvListStaticSpace;

    /*
     * If the foreach command isn't in a procedure, don't compile it inline:
     * the payoff is too small.
     */

    if (procPtr == NULL) {
	return TCL_OUT_LINE_COMPILE;
    }

    numWords = parsePtr->numWords;
    if ((numWords < 4) || (numWords%2 != 0)) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Bail out if the body requires substitutions
     * in order to insure correct behaviour [Bug 219166]
     */
    for (i = 0, tokenPtr = parsePtr->tokenPtr;
	    i < numWords-1;
	    i++, tokenPtr += (tokenPtr->numComponents + 1)) {
    }
    bodyTokenPtr = tokenPtr;
    if (bodyTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Allocate storage for the varcList and varvList arrays if necessary.
     */

    numLists = (numWords - 2)/2;
    if (numLists > STATIC_VAR_LIST_SIZE) {
        varcList = (int *) ckalloc(numLists * sizeof(int));
        varvList = (CONST char ***) ckalloc(numLists * sizeof(CONST char **));
    }
    for (loopIndex = 0;  loopIndex < numLists;  loopIndex++) {
        varcList[loopIndex] = 0;
        varvList[loopIndex] = NULL;
    }

    /*
     * Break up each var list and set the varcList and varvList arrays.
     * Don't compile the foreach inline if any var name needs substitutions
     * or isn't a scalar, or if any var list needs substitutions.
     */

    loopIndex = 0;
    for (i = 0, tokenPtr = parsePtr->tokenPtr;
	    i < numWords-1;
	    i++, tokenPtr += (tokenPtr->numComponents + 1)) {
	if (i%2 == 1) {
	    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
		code = TCL_OUT_LINE_COMPILE;
		goto done;
	    } else {
		/* Lots of copying going on here.  Need a ListObj wizard
		 * to show a better way. */

		Tcl_DString varList;

		Tcl_DStringInit(&varList);
		Tcl_DStringAppend(&varList, tokenPtr[1].start,
			tokenPtr[1].size);
		code = Tcl_SplitList(interp, Tcl_DStringValue(&varList),
			&varcList[loopIndex], &varvList[loopIndex]);
		Tcl_DStringFree(&varList);
		if (code != TCL_OK) {
		    code = TCL_OUT_LINE_COMPILE;
		    goto done;
		}
		numVars = varcList[loopIndex];
		for (j = 0;  j < numVars;  j++) {
		    CONST char *varName = varvList[loopIndex][j];
		    if (!TclIsLocalScalar(varName, (int) strlen(varName))) {
			code = TCL_OUT_LINE_COMPILE;
			goto done;
		    }
		}
	    }
	    loopIndex++;
	}
    }

    /*
     * We will compile the foreach command.
     * Reserve (numLists + 1) temporary variables:
     *    - numLists temps to hold each value list
     *    - 1 temp for the loop counter (index of next element in each list)
     * At this time we don't try to reuse temporaries; if there are two
     * nonoverlapping foreach loops, they don't share any temps.
     */

    code = TCL_OK;
    firstValueTemp = -1;
    for (loopIndex = 0;  loopIndex < numLists;  loopIndex++) {
	tempVar = TclFindCompiledLocal(NULL, /*nameChars*/ 0,
		/*create*/ 1, /*flags*/ 0, procPtr);
	if (loopIndex == 0) {
	    firstValueTemp = tempVar;
	}
    }
    loopCtTemp = TclFindCompiledLocal(NULL, /*nameChars*/ 0,
	    /*create*/ 1, /*flags*/ 0, procPtr);

    /*
     * Create and initialize the ForeachInfo and ForeachVarList data
     * structures describing this command. Then create a AuxData record
     * pointing to the ForeachInfo structure.
     */

    infoPtr = (ForeachInfo *) ckalloc((unsigned)
	    (sizeof(ForeachInfo) + (numLists * sizeof(ForeachVarList *))));
    infoPtr->numLists = numLists;
    infoPtr->firstValueTemp = firstValueTemp;
    infoPtr->loopCtTemp = loopCtTemp;
    for (loopIndex = 0;  loopIndex < numLists;  loopIndex++) {
	ForeachVarList *varListPtr;
	numVars = varcList[loopIndex];
	varListPtr = (ForeachVarList *) ckalloc((unsigned)
	        sizeof(ForeachVarList) + (numVars * sizeof(int)));
	varListPtr->numVars = numVars;
	for (j = 0;  j < numVars;  j++) {
	    CONST char *varName = varvList[loopIndex][j];
	    int nameChars = strlen(varName);
	    varListPtr->varIndexes[j] = TclFindCompiledLocal(varName,
		    nameChars, /*create*/ 1, /*flags*/ 0, procPtr);
	}
	infoPtr->varLists[loopIndex] = varListPtr;
    }
    infoIndex = TclCreateAuxData((ClientData) infoPtr, &tclForeachInfoType, envPtr);

    /*
     * Evaluate then store each value list in the associated temporary.
     */

    loopIndex = 0;
    for (i = 0, tokenPtr = parsePtr->tokenPtr;
	    i < numWords-1;
	    i++, tokenPtr += (tokenPtr->numComponents + 1)) {
	if ((i%2 == 0) && (i > 0)) {
	    TclCompileTokens(interp, tokenPtr+1,
		    tokenPtr->numComponents, envPtr);

	    tempVar = (firstValueTemp + loopIndex);
	    TclEmitInst2(INST_STORE, VM_VAR_OMIT_PUSH, tempVar, envPtr);
	    loopIndex++;
	}
    }

    /*
     * Initialize the temporary var that holds the count of loop
     * iterations. This jumps to the INST_FOREACH_STEP code after the body
     * (loop rotation optimisation).
     */

    TclEmitInst1(INST_FOREACH_START, infoIndex, envPtr);


    /*
     * Inline compile the loop body.
     */

    range = TclBeginExceptRange(envPtr);
    infoPtr->rangeIndex = range;
    
    bodyOffset = (envPtr->codeNext - envPtr->codeStart);

    TclCompileCmdWord(interp, bodyTokenPtr+1,
	    bodyTokenPtr->numComponents, envPtr);
    TclSetStackDepth((savedStackDepth+1), envPtr);

    TclEndExceptRange(range, envPtr);
    TclEmitInst0(INST_POP, envPtr);

    /*
     * Test for loop end, jump back to the top of the loop if not ended. 
     */

    envPtr->exceptArrayPtr[range].continueOffset
	    = (envPtr->codeNext - envPtr->codeStart);
    TclEmitInst1(INST_FOREACH_STEP, infoIndex, envPtr);

    /*
     * Set the loop's break target.
     */

    envPtr->exceptArrayPtr[range].breakOffset =
	    (envPtr->codeNext - envPtr->codeStart);

    /*
     * The foreach command's result is an empty string.
     */

    TclEmitPush(TclRegisterLiteral(envPtr, "", 0, /*onHeap*/ 0), envPtr);
    TclSetStackDepth((savedStackDepth+1), envPtr);

    done:
    for (loopIndex = 0;  loopIndex < numLists;  loopIndex++) {
	if (varvList[loopIndex] != (CONST char **) NULL) {
	    ckfree((char *) varvList[loopIndex]);
	}
    }
    if (varcList != varcListStaticSpace) {
	ckfree((char *) varcList);
        ckfree((char *) varvList);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * DupForeachInfo --
 *
 *	This procedure duplicates a ForeachInfo structure created as
 *	auxiliary data during the compilation of a foreach command.
 *
 * Results:
 *	A pointer to a newly allocated copy of the existing ForeachInfo
 *	structure is returned.
 *
 * Side effects:
 *	Storage for the copied ForeachInfo record is allocated. If the
 *	original ForeachInfo structure pointed to any ForeachVarList
 *	records, these structures are also copied and pointers to them
 *	are stored in the new ForeachInfo record.
 *
 *----------------------------------------------------------------------
 */

static ClientData
DupForeachInfo(clientData)
    ClientData clientData;	/* The foreach command's compilation
				 * auxiliary data to duplicate. */
{
    register ForeachInfo *srcPtr = (ForeachInfo *) clientData;
    ForeachInfo *dupPtr;
    register ForeachVarList *srcListPtr, *dupListPtr;
    int numLists = srcPtr->numLists;
    int numVars, i, j;

    dupPtr = (ForeachInfo *) ckalloc((unsigned)
	    (sizeof(ForeachInfo) + (numLists * sizeof(ForeachVarList *))));
    dupPtr->numLists = numLists;
    dupPtr->firstValueTemp = srcPtr->firstValueTemp;
    dupPtr->loopCtTemp = srcPtr->loopCtTemp;
    dupPtr->rangeIndex = srcPtr->rangeIndex;
    
    for (i = 0;  i < numLists;  i++) {
	srcListPtr = srcPtr->varLists[i];
	numVars = srcListPtr->numVars;
	dupListPtr = (ForeachVarList *) ckalloc((unsigned)
	        sizeof(ForeachVarList) + numVars*sizeof(int));
	dupListPtr->numVars = numVars;
	for (j = 0;  j < numVars;  j++) {
	    dupListPtr->varIndexes[j] =	srcListPtr->varIndexes[j];
	}
	dupPtr->varLists[i] = dupListPtr;
    }
    return (ClientData) dupPtr;
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
FreeForeachInfo(clientData)
    ClientData clientData;	/* The foreach command's compilation
				 * auxiliary data to free. */
{
    register ForeachInfo *infoPtr = (ForeachInfo *) clientData;
    register ForeachVarList *listPtr;
    int numLists = infoPtr->numLists;
    register int i;

    for (i = 0;  i < numLists;  i++) {
	listPtr = infoPtr->varLists[i];
	ckfree((char *) listPtr);
    }
    ckfree((char *) infoPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileIfCmd --
 *
 *	Procedure called to compile the "if" command.
 *
 * Results:
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "if" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */
int
TclCompileIfCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    JumpFixupArray jumpFalseFixupArray;
    				/* Used to fix the ifFalse jump after each
				 * test when its target PC is determined. */
    JumpFixupArray jumpEndFixupArray;
				/* Used to fix the jump after each "then"
				 * body to the end of the "if" when that PC
				 * is determined. */
    Tcl_Token *tokenPtr, *testTokenPtr;
    int jumpIndex = 0;          /* avoid compiler warning. */
    int numWords, wordIdx, numBytes, j, code;
    CONST char *word;
    int savedStackDepth = envPtr->currStackDepth;
                                /* Saved stack depth at the start of the first
				 * test; the envPtr current depth is restored
				 * to this value at the start of each test. */
    int realCond = 1;           /* set to 0 for static conditions: "if 0 {..}" */
    int boolVal;                /* value of static condition */
    int compileScripts = 1;            

    /*
     * Only compile the "if" command if all arguments are simple
     * words, in order to insure correct substitution [Bug 219166]
     */

    tokenPtr = parsePtr->tokenPtr;
    wordIdx = 0;
    numWords = parsePtr->numWords;

    for (wordIdx = 0; wordIdx < numWords; wordIdx++) {
	if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    return TCL_OUT_LINE_COMPILE;
	}
	tokenPtr += 2;
    }


    TclInitJumpFixupArray(&jumpFalseFixupArray);
    TclInitJumpFixupArray(&jumpEndFixupArray);
    code = TCL_OK;

    /*
     * Each iteration of this loop compiles one "if expr ?then? body"
     * or "elseif expr ?then? body" clause. 
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
	    tokenPtr += (tokenPtr->numComponents + 1);
	    wordIdx++;
	} else {
	    break;
	}
	if (wordIdx >= numWords) {
	    code = TCL_OUT_LINE_COMPILE;
	    goto done;
	}

	/*
	 * Compile the test expression then emit the conditional jump
	 * around the "then" part. 
	 */

	TclSetStackDepth((savedStackDepth), envPtr);
	testTokenPtr = tokenPtr;


	if (realCond) {
	    /*
	     * Find out if the condition is a constant. 
	     */

	    Tcl_Obj *boolObj = Tcl_NewStringObj(testTokenPtr[1].start,
		    testTokenPtr[1].size);
	    Tcl_IncrRefCount(boolObj);
	    code = Tcl_GetBooleanFromObj(NULL, boolObj, &boolVal);
	    Tcl_DecrRefCount(boolObj);
	    if (code == TCL_OK) {
		/*
		 * A static condition
		 */
		realCond = 0;
		if (!boolVal) {
		    compileScripts = 0;
		}
	    } else {
		Tcl_ResetResult(interp);
		TclCompileExprWords(interp, testTokenPtr, 1, envPtr);
		if (jumpFalseFixupArray.next >= jumpFalseFixupArray.end) {
		    TclExpandJumpFixupArray(&jumpFalseFixupArray);
		}
		jumpIndex = jumpFalseFixupArray.next;
		jumpFalseFixupArray.next++;
		TclEmitForwardJump(envPtr, INST_JUMP_FALSE,
			       (jumpFalseFixupArray.fixup[jumpIndex]));	    
	    }
	    code = TCL_OK;
	}


	/*
	 * Skip over the optional "then" before the then clause.
	 */

	tokenPtr = testTokenPtr + (testTokenPtr->numComponents + 1);
	wordIdx++;
	if (wordIdx >= numWords) {
	    code = TCL_OUT_LINE_COMPILE;
	    goto done;
	}
	if (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    word = tokenPtr[1].start;
	    numBytes = tokenPtr[1].size;
	    if ((numBytes == 4) && (strncmp(word, "then", 4) == 0)) {
		tokenPtr += (tokenPtr->numComponents + 1);
		wordIdx++;
		if (wordIdx >= numWords) {
		    code = TCL_OUT_LINE_COMPILE;
		    goto done;
		}
	    }
	}

	/*
	 * Compile the "then" command body.
	 */

	if (compileScripts) {
	    TclSetStackDepth((savedStackDepth), envPtr);
	    TclCompileCmdWord(interp, tokenPtr+1,
	            tokenPtr->numComponents, envPtr);
	}

	if (realCond) {
	    /*
	     * Jump to the end of the "if" command. Both jumpFalseFixupArray and
	     * jumpEndFixupArray are indexed by "jumpIndex".
	     */

	    if (jumpEndFixupArray.next >= jumpEndFixupArray.end) {
		TclExpandJumpFixupArray(&jumpEndFixupArray);
	    }
	    jumpEndFixupArray.next++;
	    TclEmitForwardJump(envPtr, INST_JUMP,
	            (jumpEndFixupArray.fixup[jumpIndex]));

	    /*
	     * Fix the target of the jumpFalse after the test. Generate a 4 byte
	     * jump if the distance is > 120 bytes. This is conservative, and
	     * ensures that we won't have to replace this jump if we later also
	     * need to replace the proceeding jump to the end of the "if" with a
	     * 4 byte jump.
	     */

	    TclSetJumpTarget(envPtr, (jumpFalseFixupArray.fixup[jumpIndex]));
	} else if (boolVal) {
	    /* 
	     *We were processing an "if 1 {...}"; stop compiling
	     * scripts
	     */

	    compileScripts = 0;
	} else {
	    /* 
	     *We were processing an "if 0 {...}"; reset so that
	     * the rest (elseif, else) is compiled correctly
	     */

	    realCond = 1;
	    compileScripts = 1;
	} 

	tokenPtr += (tokenPtr->numComponents + 1);
	wordIdx++;
    }

    /*
     * Restore the current stack depth in the environment; the 
     * "else" clause (or its default) will add 1 to this.
     */

    TclSetStackDepth((savedStackDepth), envPtr);

    /*
     * Check for the optional else clause. Do not compile
     * anything if this was an "if 1 {...}" case.
     */

    if ((wordIdx < numWords)
	    && (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD)) {
	/*
	 * There is an else clause. Skip over the optional "else" word.
	 */

	word = tokenPtr[1].start;
	numBytes = tokenPtr[1].size;
	if ((numBytes == 4) && (strncmp(word, "else", 4) == 0)) {
	    tokenPtr += (tokenPtr->numComponents + 1);
	    wordIdx++;
	    if (wordIdx >= numWords) {
		code = TCL_OUT_LINE_COMPILE;
		goto done;
	    }
	}

	if (compileScripts) {
	    /*
	     * Compile the else command body.
	     */

	    TclCompileCmdWord(interp, tokenPtr+1,
		    tokenPtr->numComponents, envPtr);
	}

	/*
	 * Make sure there are no words after the else clause.
	 */

	wordIdx++;
	if (wordIdx < numWords) {
	    code = TCL_OUT_LINE_COMPILE;
	    goto done;
	}
    } else {
	/*
	 * No else clause: the "if" command's result is an empty string.
	 */

	if (compileScripts) {
	    TclEmitPush(TclRegisterNewLiteral(envPtr, "", 0), envPtr);
	}
    }

    /*
     * Fix the unconditional jumps to the end of the "if" command.
     */

    for (j = jumpEndFixupArray.next;  j > 0;  j--) {
	jumpIndex = (j - 1);	/* i.e. process the closest jump first */
	TclSetJumpTarget(envPtr, (jumpEndFixupArray.fixup[jumpIndex]));
    }

    /*
     * Free the jumpFixupArray array if malloc'ed storage was used.
     */

    done:
    TclSetStackDepth((savedStackDepth+1), envPtr);
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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "incr" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileIncrCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *varTokenPtr, *incrTokenPtr;
    int simpleVarName, isScalar, localIndex;
    int stackDepth = envPtr->currStackDepth + 1;
    int valAndFlags = 0;
    
    if ((parsePtr->numWords != 2) && (parsePtr->numWords != 3)) {
	return TCL_OUT_LINE_COMPILE;
    }

    varTokenPtr = parsePtr->tokenPtr
	    + (parsePtr->tokenPtr->numComponents + 1);

    PushVarName(interp, varTokenPtr, envPtr, TCL_CREATE_VAR,
	    &localIndex, &simpleVarName, &isScalar);

    if (localIndex == -1) {
	localIndex = HPUINT_MAX;
    }

    /*
     * If an increment is given, push it, but see first if it's a small
     * integer.
     */

    if (parsePtr->numWords == 3) {
	incrTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
	if (incrTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    CONST char *word = incrTokenPtr[1].start;
	    int numBytes = incrTokenPtr[1].size;
	    int n;

	    /*
	     * Note there is a danger that modifying the string could have
	     * undesirable side effects.  In this case, TclLooksLikeInt has
	     * no dependencies on shared strings so we should be safe.
	     */

	    if (TclLooksLikeInt(word, numBytes)) {
		int code;
		Tcl_Obj *intObj = Tcl_NewStringObj(word, numBytes);
		Tcl_IncrRefCount(intObj);
		code = Tcl_GetIntFromObj(NULL, intObj, &n);
		Tcl_DecrRefCount(intObj);
		if ((code == TCL_OK)
			&& ((TclPSizedInt) HPINT_MIN < ((TclPSizedInt)n<<2))
			&& (((TclPSizedInt)n<<2) <= (TclPSizedInt)HPINT_MAX)) {
		    valAndFlags = (n << 2);
		}
	    }
	    if (!valAndFlags) {
		valAndFlags = HPINT_MIN;
		TclEmitPush(
			TclRegisterNewLiteral(envPtr, word, numBytes), envPtr);
		stackDepth--;
	    }
	} else {
	    valAndFlags = HPINT_MIN;
	    TclCompileTokens(interp, incrTokenPtr+1, 
	            incrTokenPtr->numComponents, envPtr);
	    stackDepth--;
	}
    } else {			/* no incr amount given so use 1 */
	valAndFlags = (1 << 2);
    }

    if (!isScalar) {
	valAndFlags |= VM_VAR_ARRAY;
    }

    /*
     * Emit the instruction to increment the variable.
     */

    TclEmitInst2(INST_INCR, valAndFlags, localIndex, envPtr);
    TclSetStackDepth((stackDepth+1), envPtr);
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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "lappend" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLappendCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    int numWords;
    int flags =(TCL_APPEND_VALUE|TCL_LIST_ELEMENT|TCL_TRACE_READS);    

    /*
     * If we're not in a procedure, don't compile.
     */
    if (envPtr->procPtr == NULL) {
	return TCL_OUT_LINE_COMPILE;
    }

    numWords = parsePtr->numWords;
    if (numWords == 1) {
	return TCL_OUT_LINE_COMPILE;
    }
    if (numWords != 3) {
	/*
	 * LAPPEND instructions currently only handle one value appends
	 */
        return TCL_OUT_LINE_COMPILE;
    }

    return CompileSetCmdInternal(interp, parsePtr, envPtr, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileLassignCmd --
 *
 *	Procedure called to compile the "lassign" command.
 *
 * Results:
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "lassign" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLassignCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;
    int simpleVarName, isScalar, localIndex, numWords, idx;

    numWords = parsePtr->numWords;
    /*
     * Check for command syntax error, but we'll punt that to runtime
     */
    if (numWords < 3) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Check that the number of variables to be assigned is small enough 
     */
    if (numWords > HPUINT_MAX) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Generate code to push list being taken apart by [lassign].
     */
    tokenPtr = parsePtr->tokenPtr + (parsePtr->tokenPtr->numComponents + 1);
    if (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	TclEmitPush(TclRegisterNewLiteral(envPtr, 
		tokenPtr[1].start, tokenPtr[1].size), envPtr);
    } else {
	TclCompileTokens(interp, tokenPtr+1, tokenPtr->numComponents, envPtr);
    }

    /*
     * Generate code to assign values from the list to variables
     */
    for (idx=0 ; idx<numWords-2 ; idx++) {
	int flags = (TCL_LEAVE_ERR_MSG|VM_VAR_OMIT_PUSH);
	tokenPtr += tokenPtr->numComponents + 1;
	/*
	 * Generate the next variable name
	 */
	PushVarName(interp, tokenPtr, envPtr, TCL_CREATE_VAR,
		&localIndex, &simpleVarName, &isScalar);

	if (localIndex < 0) {
	    localIndex = HPUINT_MAX;
	}
	if (isScalar || !simpleVarName) {
	    if ((localIndex & HP_MASK)  != HPUINT_MAX) {
		TclEmitInst0(INST_DUP, envPtr);
	    } else {
		TclEmitInst1(INST_OVER, 1, envPtr);
	    }
	} else {
	    flags |= VM_VAR_ARRAY;
	    if ((localIndex & HP_MASK)  != HPUINT_MAX) {
		TclEmitInst1(INST_OVER, 1, envPtr);
	    } else {
		TclEmitInst1(INST_OVER, 2, envPtr);
	    }
	}
	TclEmitInst1(INST_LIST_INDEX_IMM, idx, envPtr);
	TclEmitInst2(INST_STORE, flags, localIndex, envPtr);
    }    

    /*
     * Generate code to leave the rest of the list on the stack.
     * Note that -2 == "end" 
     */
    TclEmitInst2(INST_LIST_RANGE_IMM, -2, idx, envPtr);

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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "lindex" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLindexCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *varTokenPtr;
    int i, numWords;
    numWords = parsePtr->numWords;

    /*
     * Quit if too few args
     */

    if (numWords <= 1) {
	return TCL_OUT_LINE_COMPILE;
    }

    varTokenPtr = parsePtr->tokenPtr
	+ (parsePtr->tokenPtr->numComponents + 1);

    /*
     * Push the operands onto the stack.
     */

    for (i=1 ; i<numWords ; i++) {
	if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    TclEmitPush(
		    TclRegisterNewLiteral(envPtr, varTokenPtr[1].start,
		    varTokenPtr[1].size), envPtr);
	} else {
	    TclCompileTokens(interp, varTokenPtr+1,
		    varTokenPtr->numComponents, envPtr);
	}
	varTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
    }

    /*
     * Emit INST_LIST_INDEX if objc==3, or INST_LIST_INDEX_MULTI
     * if there are multiple index args.
     */

    if (numWords == 3) {
	TclEmitInst0(INST_LIST_INDEX, envPtr);
    } else {
 	TclEmitInst1(INST_LIST_INDEX_MULTI, numWords-1, envPtr);
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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "list" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileListCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    /*
     * If we're not in a procedure, don't compile.
     */
    if (envPtr->procPtr == NULL) {
	return TCL_OUT_LINE_COMPILE;
    }

    if (parsePtr->numWords == 1) {
	/*
	 * Empty args case
	 */

	TclEmitPush(TclRegisterNewLiteral(envPtr, "", 0), envPtr);
    } else {
	/*
	 * Push the all values onto the stack.
	 */
	Tcl_Token *valueTokenPtr;
	int i, numWords;

	numWords = parsePtr->numWords;

	valueTokenPtr = parsePtr->tokenPtr
	    + (parsePtr->tokenPtr->numComponents + 1);
	for (i = 1; i < numWords; i++) {
	    if (valueTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
		TclEmitPush(TclRegisterNewLiteral(envPtr,
			valueTokenPtr[1].start, valueTokenPtr[1].size), envPtr);
	    } else {
		TclCompileTokens(interp, valueTokenPtr+1,
			valueTokenPtr->numComponents, envPtr);
	    }
	    valueTokenPtr = valueTokenPtr + (valueTokenPtr->numComponents + 1);
	}
	TclEmitInst1(INST_LIST, numWords - 1, envPtr);
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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "llength" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLlengthCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *varTokenPtr;

    if (parsePtr->numWords != 2) {
	return TCL_OUT_LINE_COMPILE;
    }
    varTokenPtr = parsePtr->tokenPtr
	+ (parsePtr->tokenPtr->numComponents + 1);

    if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	/*
	 * We could simply count the number of elements here and push
	 * that value, but that is too rare a case to waste the code space.
	 */
	TclEmitPush(TclRegisterNewLiteral(envPtr, varTokenPtr[1].start,
		varTokenPtr[1].size), envPtr);
    } else {
	TclCompileTokens(interp, varTokenPtr+1,
		varTokenPtr->numComponents, envPtr);
    }
    TclEmitInst0(INST_LIST_LENGTH, envPtr);
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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "lset" command
 *	at runtime.
 *
 * The general template for execution of the "lset" command is:
 *	(1) Instructions to push the variable name, unless the
 *	    variable is local to the stack frame.
 *	(2) If the variable is an array element, instructions
 *	    to push the array element name.
 *	(3) Instructions to push each of zero or more "index" arguments
 *	    to the stack, followed with the "newValue" element.
 *	(4) Instructions to duplicate the variable name and/or array
 *	    element name onto the top of the stack, if either was
 *	    pushed at steps (1) and (2).
 *	(5) The appropriate INST_LOAD_* instruction to place the
 *	    original value of the list variable at top of stack.
 *	(6) At this point, the stack contains:
 *	     varName? arrayElementName? index1 index2 ... newValue oldList
 *	    The compiler emits one of INST_LSET_FLAT or INST_LSET_LIST
 *	    according as whether there is exactly one index element (LIST)
 *	    or either zero or else two or more (FLAT).  This instruction
 *	    removes everything from the stack except for the two names
 *	    and pushes the new value of the variable.
 *	(7) Finally, INST_STORE stores the new value in the variable
 *	    and cleans up the stack.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileLsetCmd(interp, parsePtr, envPtr)
    Tcl_Interp* interp;		/* Tcl interpreter for error reporting */
    Tcl_Parse* parsePtr;	/* Points to a parse structure for
				 * the command */
    CompileEnv* envPtr;		/* Holds the resulting instructions */
{
    int tempDepth;		/* Depth used for emitting one part
				 * of the code burst. */
    Tcl_Token* varTokenPtr;	/* Pointer to the Tcl_Token representing
				 * the parse of the variable name */
    int localIndex;		/* Index of var in local var table */
    int simpleVarName;		/* Flag == 1 if var name is simple */
    int isScalar;		/* Flag == 1 if scalar, 0 if array */
    int i;
    int varFlags = TCL_LEAVE_ERR_MSG;
    
    /* Check argument count */

    if (parsePtr->numWords < 3) {
	/* Fail at run time, not in compilation */
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Decide if we can use a frame slot for the var/array name or if we
     * need to emit code to compute and push the name at runtime. We use a
     * frame slot (entry in the array of local vars) if we are compiling a
     * procedure body and if the name is simple text that does not include
     * namespace qualifiers. 
     */

    varTokenPtr = parsePtr->tokenPtr
	    + (parsePtr->tokenPtr->numComponents + 1);
    PushVarName(interp, varTokenPtr, envPtr, TCL_CREATE_VAR,
	    &localIndex, &simpleVarName, &isScalar);

    if (localIndex < 0) {
	localIndex = HPUINT_MAX;
    }

    /* Push the "index" args and the new element value. */

    for (i=2 ; i<parsePtr->numWords ; ++i) {
	/* Advance to next arg */

	varTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);

	/* Push an arg */

	if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    TclEmitPush(TclRegisterNewLiteral(envPtr, varTokenPtr[1].start,
		    varTokenPtr[1].size), envPtr);
	} else {
	    TclCompileTokens(interp, varTokenPtr+1,
		    varTokenPtr->numComponents, envPtr);
	}
    }

    /*
     * Duplicate the variable name if it's been pushed.  
     */

    if (!simpleVarName || ((localIndex & HP_MASK) == HPUINT_MAX)) {
	if (!simpleVarName || isScalar) {
	    tempDepth = parsePtr->numWords - 2;
	} else {
	    tempDepth = parsePtr->numWords - 1;
	}
	TclEmitInst1(INST_OVER, tempDepth, envPtr);
    }

    /*
     * Duplicate an array index if one's been pushed
     */

    if (simpleVarName && !isScalar) {
	if ((localIndex & HP_MASK) == HPUINT_MAX) {
	    tempDepth = parsePtr->numWords - 1;
	} else {
	    tempDepth = parsePtr->numWords - 2;
	}
	TclEmitInst1(INST_OVER, tempDepth, envPtr);
	varFlags |= VM_VAR_ARRAY;
    }

    /*
     * Emit code to load the variable's value, the correct variety of 'lset'
     * instruction and put the value back in the variable.
     */

    TclEmitInst2(INST_LOAD, varFlags, localIndex, envPtr);
    if (parsePtr->numWords == 4) {
	TclEmitInst0(INST_LSET_LIST, envPtr);
    } else {
	TclEmitInst1(INST_LSET_FLAT, (parsePtr->numWords - 1), envPtr);
    }
    TclEmitInst2(INST_STORE, varFlags, localIndex, envPtr);

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
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "regexp" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileRegexpCmd(interp, parsePtr, envPtr)
    Tcl_Interp* interp;		/* Tcl interpreter for error reporting */
    Tcl_Parse* parsePtr;	/* Points to a parse structure for
				 * the command */
    CompileEnv* envPtr;		/* Holds the resulting instructions */
{
    Tcl_Token *varTokenPtr;	/* Pointer to the Tcl_Token representing
				 * the parse of the RE or string */
    int i, len, nocase, anchorLeft, anchorRight, start;
    char *str;

    /*
     * We are only interested in compiling simple regexp cases.
     * Currently supported compile cases are:
     *   regexp ?-nocase? ?--? staticString $var
     *   regexp ?-nocase? ?--? {^staticString$} $var
     */
    if (parsePtr->numWords < 3) {
	return TCL_OUT_LINE_COMPILE;
    }

    nocase = 0;
    varTokenPtr = parsePtr->tokenPtr;

    /*
     * We only look for -nocase and -- as options.  Everything else
     * gets pushed to runtime execution.  This is different than regexp's
     * runtime option handling, but satisfies our stricter needs.
     */
    for (i = 1; i < parsePtr->numWords - 2; i++) {
	varTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
	if (varTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    /* Not a simple string - punt to runtime. */
	    return TCL_OUT_LINE_COMPILE;
	}
	str = (char *) varTokenPtr[1].start;
	len = varTokenPtr[1].size;
	if ((len == 2) && (str[0] == '-') && (str[1] == '-')) {
	    i++;
	    break;
	} else if ((len > 1)
		&& (strncmp(str, "-nocase", (unsigned) len) == 0)) {
	    nocase = 1;
	} else {
	    /* Not an option we recognize. */
	    return TCL_OUT_LINE_COMPILE;
	}
    }

    if ((parsePtr->numWords - i) != 2) {
	/* We don't support capturing to variables */
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Get the regexp string.  If it is not a simple string, punt to runtime.
     * If it has a '-', it could be an incorrectly formed regexp command.
     */
    varTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
    str = (char *) varTokenPtr[1].start;
    len = varTokenPtr[1].size;
    if ((varTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) || (*str == '-')) {
	return TCL_OUT_LINE_COMPILE;
    }

    if (len == 0) {
	/*
	 * The semantics of regexp are always match on re == "".
	 */
	TclEmitPush(TclRegisterNewLiteral(envPtr, "1", 1), envPtr);
	return TCL_OK;
    }

    /*
     * Make a copy of the string that is null-terminated for checks which
     * require such.
     */
    str = (char *) ckalloc((unsigned) len + 1);
    strncpy(str, varTokenPtr[1].start, (size_t) len);
    str[len] = '\0';
    start = 0;

    /*
     * Check for anchored REs (ie ^foo$), so we can use string equal if
     * possible. Do not alter the start of str so we can free it correctly.
     */
    if (str[0] == '^') {
	start++;
	anchorLeft = 1;
    } else {
	anchorLeft = 0;
    }
    if ((str[len-1] == '$') && ((len == 1) || (str[len-2] != '\\'))) {
	anchorRight = 1;
	str[--len] = '\0';
    } else {
	anchorRight = 0;
    }

    /*
     * On the first (pattern) arg, check to see if any RE special characters
     * are in the word.  If not, this is the same as 'string equal'.
     */
    if ((len > (1+start)) && (str[start] == '.') && (str[start+1] == '*')) {
	start += 2;
	anchorLeft = 0;
    }
    if ((len > (2+start)) && (str[len-3] != '\\')
	    && (str[len-2] == '.') && (str[len-1] == '*')) {
	len -= 2;
	str[len] = '\0';
	anchorRight = 0;
    }

    /*
     * Don't do anything with REs with other special chars.  Also check if
     * this is a bad RE (do this at the end because it can be expensive).
     * If so, let it complain at runtime.
     */
    if ((strpbrk(str + start, "*+?{}()[].\\|^$") != NULL)
	    || (Tcl_RegExpCompile(NULL, str) == NULL)) {
	ckfree((char *) str);
	return TCL_OUT_LINE_COMPILE;
    }

    if (anchorLeft && anchorRight) {
	TclEmitPush(TclRegisterNewLiteral(envPtr, str+start, len-start),
		envPtr);
    } else {
	/*
	 * This needs to find the substring anywhere in the string, so
	 * use string match and *foo*, with appropriate anchoring.
	 */
	char *newStr  = ckalloc((unsigned) len + 3);
	len -= start;
	if (anchorLeft) {
	    strncpy(newStr, str + start, (size_t) len);
	} else {
	    newStr[0] = '*';
	    strncpy(newStr + 1, str + start, (size_t) len++);
	}
	if (!anchorRight) {
	    newStr[len++] = '*';
	}
	newStr[len] = '\0';
	TclEmitPush(TclRegisterNewLiteral(envPtr, newStr, len), envPtr);
	ckfree((char *) newStr);
    }
    ckfree((char *) str);

    /*
     * Push the string arg
     */
    varTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
    if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	TclEmitPush(TclRegisterNewLiteral(envPtr,
		varTokenPtr[1].start, varTokenPtr[1].size), envPtr);
    } else {
	TclCompileTokens(interp, varTokenPtr+1,
		varTokenPtr->numComponents, envPtr);
    }

    if (anchorLeft && anchorRight && !nocase) {
	TclEmitInst0(INST_STR_EQ, envPtr);
    } else {
	TclEmitInst1(INST_STR_MATCH, nocase, envPtr);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileReturnCmd --
 *
 *	Procedure called to compile the "return" command.
 *
 * Results:
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "return" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileReturnCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    /*
     * General syntax: [return ?-option value ...? ?result?]
     * An even number of words means an explicit result argument is present.
     */
    int level, code, status = TCL_OK;
    int numWords = parsePtr->numWords;
    int explicitResult = (0 == (numWords % 2));
    int numOptionWords = numWords - 1 - explicitResult;
    Tcl_Obj *returnOpts;
    Tcl_Token *wordTokenPtr = parsePtr->tokenPtr
		+ (parsePtr->tokenPtr->numComponents + 1);
#define NUM_STATIC_OBJS 20
    int objc;
    Tcl_Obj *staticObjArray[NUM_STATIC_OBJS], **objv;

    if (numOptionWords > NUM_STATIC_OBJS) {
	objv = (Tcl_Obj **) ckalloc(numOptionWords * sizeof(Tcl_Obj *));
    } else {
	objv = staticObjArray;
    }

    /* 
     * Scan through the return options.  If any are unknown at compile
     * time, there is no value in bytecompiling.  Save the option values
     * known in an objv array for merging into a return options dictionary.
     */

    for (objc = 0; objc < numOptionWords; objc++) {
	objv[objc] = Tcl_NewObj();
	Tcl_IncrRefCount(objv[objc]);
	if (!TclWordKnownAtCompileTime(wordTokenPtr, objv[objc])) {
	    objc++;
	    status = TCL_ERROR;
	    goto cleanup;
	}
	wordTokenPtr += wordTokenPtr->numComponents + 1;
    }
    status = TclMergeReturnOptions(interp, objc, objv,
	    &returnOpts, &code, &level);
cleanup:
    while (--objc >= 0) {
	Tcl_DecrRefCount(objv[objc]);
    }
    if (numOptionWords > NUM_STATIC_OBJS) {
	ckfree((char *)objv);
    }
    if (TCL_ERROR == status) {
	/*
	 * Something was bogus in the return options.  Clear the
	 * error message, and report back to the compiler that this
	 * must be interpreted at runtime.
	 */
	Tcl_ResetResult(interp);
	return TCL_OUT_LINE_COMPILE;
    }
    if ((HPINT_MIN > code) || (code > HPINT_MAX)
            || (level > HPUINT_MAX)) {
	Tcl_ResetResult(interp);
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * All options are known at compile time, so we're going to bytecompile.
     * Emit instructions to push the result on the stack
     */

    if (explicitResult) {
	if (wordTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    /* Simple word: compile quickly to a simple push */
	    TclEmitPush(TclRegisterNewLiteral(envPtr, wordTokenPtr[1].start,
			wordTokenPtr[1].size), envPtr);
	} else {
	    /* More complex tokens get compiled */
	    TclCompileTokens(interp, wordTokenPtr+1,
		    wordTokenPtr->numComponents, envPtr);
	}
    } else {
	/* No explict result argument, so default result is empty string */
	TclEmitPush(TclRegisterNewLiteral(envPtr, "", 0), envPtr);
    }

   /* 
    * Check for optimization:  When [return] is in a proc, and there's
    * no enclosing [catch], and there are no return options, then the
    * INST_DONE instruction is equivalent, and may be more efficient.
    */

    if (numOptionWords == 0) {
	/* We have default return options... */
	if (envPtr->procPtr != NULL) {
	/* ... and we're in a proc ... */
	    if (!envPtr->catchDepth) {
		/* ... and there is no enclosing catch. */	    
		Tcl_DecrRefCount(returnOpts);
		TclEmitInst0(INST_DONE, envPtr);
		return TCL_OK;
	    }
	}	
    } else if ((numOptionWords == 4) && (level == 0)) {
	if (code == TCL_BREAK) {
	    Tcl_DecrRefCount(returnOpts);
	    TclEmitInst1(INST_BREAK, envPtr->exceptArrayCurr, envPtr);
	    return TCL_OK;
	} else if (code == TCL_CONTINUE) {
	    Tcl_DecrRefCount(returnOpts);
	    TclEmitInst1(INST_CONTINUE, envPtr->exceptArrayCurr, envPtr);
	    return TCL_OK;
	}
    }

    /*
     * Could not use the optimization, so we push the return options
     * dictionary, and emit the INST_RETURN instruction with code
     * and level as operands.
     */

    TclEmitPush(TclAddLiteralObj(envPtr, returnOpts, NULL), envPtr);
    TclEmitInst2(INST_RETURN, code, level, envPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileSetCmd --
 *
 *	Procedure called to compile the "set" command.
 *
 * Results:
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "set" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileSetCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    return CompileSetCmdInternal(interp, parsePtr, envPtr, 0);
}

int
CompileSetCmdInternal(interp, parsePtr, envPtr, varFlags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
    int varFlags;
{
    Tcl_Token *varTokenPtr, *valueTokenPtr;
    int isAssignment, isScalar, simpleVarName, localIndex, numWords;

    numWords = parsePtr->numWords;
    if ((numWords != 2) && (numWords != 3)) {
	return TCL_OUT_LINE_COMPILE;
    }
    isAssignment = (numWords == 3);

    /*
     * Decide if we can use a frame slot for the var/array name or if we
     * need to emit code to compute and push the name at runtime. We use a
     * frame slot (entry in the array of local vars) if we are compiling a
     * procedure body and if the name is simple text that does not include
     * namespace qualifiers. 
     */

    varTokenPtr = parsePtr->tokenPtr
	    + (parsePtr->tokenPtr->numComponents + 1);

    PushVarName(interp, varTokenPtr, envPtr, TCL_CREATE_VAR,
	    &localIndex, &simpleVarName, &isScalar);

    if (isScalar) {
	varFlags |= TCL_LEAVE_ERR_MSG;
    } else {
	varFlags |= (TCL_LEAVE_ERR_MSG|VM_VAR_ARRAY);
    }
	
    if (localIndex < 0) {
	localIndex = HPUINT_MAX;
    }

    if (isAssignment) {
	/*
	 * If we are doing an assignment, push the new value and store it. 
	 */

	valueTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
	if (valueTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    TclEmitPush(TclRegisterNewLiteral(envPtr, valueTokenPtr[1].start,
		    valueTokenPtr[1].size), envPtr);
	} else {
	    TclCompileTokens(interp, valueTokenPtr+1,
	            valueTokenPtr->numComponents, envPtr);
	}
	TclEmitInst2(INST_STORE, varFlags, localIndex, envPtr);
    } else {
	/*
	 * Reading the variable's value.
	 */
	
	TclEmitInst2(INST_LOAD, varFlags, localIndex, envPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileStringCmd --
 *
 *	Procedure called to compile the "string" command.
 *
 * Results:
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "string" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileStringCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *opTokenPtr, *varTokenPtr;
    Tcl_Obj *opObj;
    int index;

    static CONST char *options[] = {
	"bytelength",	"compare",	"equal",	"first",
	"index",	"is",		"last",		"length",
	"map",		"match",	"range",	"repeat",
	"replace",	"tolower",	"toupper",	"totitle",
	"trim",		"trimleft",	"trimright",
	"wordend",	"wordstart",	(char *) NULL
    };
    enum options {
	STR_BYTELENGTH,	STR_COMPARE,	STR_EQUAL,	STR_FIRST,
	STR_INDEX,	STR_IS,		STR_LAST,	STR_LENGTH,
	STR_MAP,	STR_MATCH,	STR_RANGE,	STR_REPEAT,
	STR_REPLACE,	STR_TOLOWER,	STR_TOUPPER,	STR_TOTITLE,
	STR_TRIM,	STR_TRIMLEFT,	STR_TRIMRIGHT,
	STR_WORDEND,	STR_WORDSTART
    };	  

    if (parsePtr->numWords < 2) {
	/* Fail at run time, not in compilation */
	return TCL_OUT_LINE_COMPILE;
    }
    opTokenPtr = parsePtr->tokenPtr
	+ (parsePtr->tokenPtr->numComponents + 1);

    opObj = Tcl_NewStringObj(opTokenPtr->start, opTokenPtr->size);
    if (Tcl_GetIndexFromObj(interp, opObj, options, "option", 0,
	    &index) != TCL_OK) {
	Tcl_DecrRefCount(opObj);
	Tcl_ResetResult(interp);
	return TCL_OUT_LINE_COMPILE;
    }
    Tcl_DecrRefCount(opObj);

    varTokenPtr = opTokenPtr + (opTokenPtr->numComponents + 1);

    switch ((enum options) index) {
	case STR_BYTELENGTH:
	case STR_FIRST:
	case STR_IS:
	case STR_LAST:
	case STR_MAP:
	case STR_RANGE:
	case STR_REPEAT:
	case STR_REPLACE:
	case STR_TOLOWER:
	case STR_TOUPPER:
	case STR_TOTITLE:
	case STR_TRIM:
	case STR_TRIMLEFT:
	case STR_TRIMRIGHT:
	case STR_WORDEND:
	case STR_WORDSTART:
	    /*
	     * All other cases: compile out of line.
	     */
	    return TCL_OUT_LINE_COMPILE;

	case STR_COMPARE: 
	case STR_EQUAL: {
	    int i;
	    /*
	     * If there are any flags to the command, we can't byte compile it
	     * because the INST_STR_EQ bytecode doesn't support flags.
	     */

	    if (parsePtr->numWords != 4) {
		return TCL_OUT_LINE_COMPILE;
	    }

	    /*
	     * Push the two operands onto the stack.
	     */

	    for (i = 0; i < 2; i++) {
		if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
		    TclEmitPush(TclRegisterNewLiteral(envPtr,
			    varTokenPtr[1].start, varTokenPtr[1].size), envPtr);
		} else {
		    TclCompileTokens(interp, varTokenPtr+1,
			    varTokenPtr->numComponents, envPtr);
		}
		varTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
	    }

	    TclEmitInst0(((((enum options) index) == STR_COMPARE) ?
		    INST_STR_CMP : INST_STR_EQ), envPtr);
	    return TCL_OK;
	}
	case STR_INDEX: {
	    int i;

	    if (parsePtr->numWords != 4) {
		/* Fail at run time, not in compilation */
		return TCL_OUT_LINE_COMPILE;
	    }

	    /*
	     * Push the two operands onto the stack.
	     */

	    for (i = 0; i < 2; i++) {
		if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
		    TclEmitPush(TclRegisterNewLiteral(envPtr,
			    varTokenPtr[1].start, varTokenPtr[1].size), envPtr);
		} else {
		    TclCompileTokens(interp, varTokenPtr+1,
			    varTokenPtr->numComponents, envPtr);
		}
		varTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
	    }

	    TclEmitInst0(INST_STR_INDEX, envPtr);
	    return TCL_OK;
	}
	case STR_LENGTH: {
	    if (parsePtr->numWords != 3) {
		/* Fail at run time, not in compilation */
		return TCL_OUT_LINE_COMPILE;
	    }

	    if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
		/*
		 * Here someone is asking for the length of a static string.
		 * Just push the actual character (not byte) length.
		 */
		char buf[TCL_INTEGER_SPACE];
		int len = Tcl_NumUtfChars(varTokenPtr[1].start,
			varTokenPtr[1].size);
		len = sprintf(buf, "%d", len);
		TclEmitPush(TclRegisterNewLiteral(envPtr, buf, len), envPtr);
		return TCL_OK;
	    } else {
		TclCompileTokens(interp, varTokenPtr+1,
			varTokenPtr->numComponents, envPtr);
	    }
	    TclEmitInst0(INST_STR_LEN, envPtr);
	    return TCL_OK;
	}
	case STR_MATCH: {
	    int i, length, exactMatch = 0, nocase = 0;
	    CONST char *str;

	    if (parsePtr->numWords < 4 || parsePtr->numWords > 5) {
		/* Fail at run time, not in compilation */
		return TCL_OUT_LINE_COMPILE;
	    }

	    if (parsePtr->numWords == 5) {
		if (varTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
		    return TCL_OUT_LINE_COMPILE;
		}
		str    = varTokenPtr[1].start;
		length = varTokenPtr[1].size;
		if ((length > 1) &&
			strncmp(str, "-nocase", (size_t) length) == 0) {
		    nocase = 1;
		} else {
		    /* Fail at run time, not in compilation */
		    return TCL_OUT_LINE_COMPILE;
		}
		varTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
	    }

	    for (i = 0; i < 2; i++) {
		if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
		    str = varTokenPtr[1].start;
		    length = varTokenPtr[1].size;
		    if (!nocase && (i == 0)) {
			/*
			 * On the first (pattern) arg, check to see if any
			 * glob special characters are in the word '*[]?\\'.
			 * If not, this is the same as 'string equal'.  We
			 * can use strpbrk here because the glob chars are all
			 * in the ascii-7 range.  If -nocase was specified,
			 * we can't do this because INST_STR_EQ has no support
			 * for nocase.
			 */
			Tcl_Obj *copy = Tcl_NewStringObj(str, length);
			Tcl_IncrRefCount(copy);
			exactMatch = (strpbrk(Tcl_GetString(copy),
				"*[]?\\") == NULL);
			Tcl_DecrRefCount(copy);
		    }
		    TclEmitPush(
			    TclRegisterNewLiteral(envPtr, str, length), envPtr);
		} else {
		    TclCompileTokens(interp, varTokenPtr+1,
			    varTokenPtr->numComponents, envPtr);
		}
		varTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
	    }

	    if (exactMatch) {
		TclEmitInst0(INST_STR_EQ, envPtr);
	    } else {
		TclEmitInst1(INST_STR_MATCH, nocase, envPtr);
	    }
	    return TCL_OK;
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileSwitchCmd --
 *
 *	Procedure called to compile the "switch" command.
 *
 * Results:
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "switch" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */
int
TclCompileSwitchCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;	/* Pointer to tokens in command */
    Tcl_Token *valueTokenPtr;	/* Token for the value to switch on. */
    int foundDefault;		/* Flag to indicate whether a "default"
				 * clause is present. */
    enum {Switch_Exact, Switch_Glob} mode;
				/* What kind of switch are we doing? */
    int i, j;			/* Loop counter variables. */

    Tcl_DString bodyList;	/* Used for splitting the pattern list. */
    int argc;			/* Number of items in pattern list. */
    CONST char **argv;		/* Array of copies of items in pattern list. */
    Tcl_Token *bodyTokenArray;	/* Array of real pattern list items. */
    CONST char *tokenStartPtr;	/* Used as part of synthesizing tokens. */
    int isTokenBraced;

    int *fallThroughArray;	/* Array of forward-jump offsets for
				 * fall-through. */ 
    int *endOffsetArray;	/* Array of forward-jump offsets for jumps to
				 * the end. */
    int currentFallThroughs;    /* Counter for fall-throughs in process. */
    int endJumpCounter;         /* Counter for jumots to the end. */
    int lastFalseJump;          /* Offset of the last branch on match
				 * failure. */
    int savedStackDepth = envPtr->currStackDepth;

    tokenPtr = parsePtr->tokenPtr;

    /*
     * Only handle the following versions:
     *   switch        -- word {pattern body ...}
     *   switch -exact -- word {pattern body ...} 
     *   switch -glob  -- word {pattern body ...}
     */

    if (parsePtr->numWords != 5 &&
	parsePtr->numWords != 4) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * We don't care how the command's word was generated; we're
     * compiling it anyway!
     */
    tokenPtr += tokenPtr->numComponents + 1;

    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_OUT_LINE_COMPILE;
    } else {
	register int size = tokenPtr[1].size;
	register CONST char *chrs = tokenPtr[1].start;

	if (size < 2) {
	    return TCL_OUT_LINE_COMPILE;
	}
	if ((size <= 6) && (parsePtr->numWords == 5)
		&& !strncmp(chrs, "-exact", (unsigned) TclMin(size, 6))) {
	    mode = Switch_Exact;
	    tokenPtr += 2;
	} else if ((size <= 5) && (parsePtr->numWords == 5)
		&& !strncmp(chrs, "-glob", (unsigned) TclMin(size, 5))) {
	    mode = Switch_Glob;
	    tokenPtr += 2;
	} else if ((size == 2) && (parsePtr->numWords == 4)
		&& !strncmp(chrs, "--", 2)) {
	    /*
	     * If no control flag present, use exact matching (the default).
	     *
	     * We end up re-checking this word, but that's the way things are...
	     */
	    mode = Switch_Exact;
	} else {
	    return TCL_OUT_LINE_COMPILE;
	}
    }
    if ((tokenPtr->type != TCL_TOKEN_SIMPLE_WORD)
	|| (tokenPtr[1].size != 2) || strncmp(tokenPtr[1].start, "--", 2)) {
	return TCL_OUT_LINE_COMPILE;
    }
    tokenPtr += 2;

    /*
     * The value to test against is going to always get pushed on the
     * stack.  But not yet; we need to verify that the rest of the
     * command is compilable too.
     */

    valueTokenPtr = tokenPtr;
    tokenPtr += tokenPtr->numComponents + 1;

    /*
     * Test that we've got a suitable body list as a simple (i.e.
     * braced) word, and that the elements of the body are simple
     * words too.  This is really rather nasty indeed.
     */

    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_OUT_LINE_COMPILE;
    }
    Tcl_DStringInit(&bodyList);
    Tcl_DStringAppend(&bodyList, tokenPtr[1].start, tokenPtr[1].size);
    if (Tcl_SplitList(NULL, Tcl_DStringValue(&bodyList), &argc,
	    &argv) != TCL_OK) {
	Tcl_DStringFree(&bodyList);
	return TCL_OUT_LINE_COMPILE;
    }
    Tcl_DStringFree(&bodyList);
    if (argc == 0 || argc % 2) {
	ckfree((char *)argv);
	return TCL_OUT_LINE_COMPILE;
    }
    bodyTokenArray = (Tcl_Token *) ckalloc(sizeof(Tcl_Token) * argc);
    tokenStartPtr = tokenPtr[1].start;
    while (isspace(UCHAR(*tokenStartPtr))) {
	tokenStartPtr++;
    }
    if (*tokenStartPtr == '{') {
	tokenStartPtr++;
	isTokenBraced = 1;
    } else {
	isTokenBraced = 0;
    }
    for (i=0 ; i<argc ; i++) {
	bodyTokenArray[i].type = TCL_TOKEN_TEXT;
	bodyTokenArray[i].start = tokenStartPtr;
	bodyTokenArray[i].size = strlen(argv[i]);
	bodyTokenArray[i].numComponents = 0;
	tokenStartPtr += bodyTokenArray[i].size;
	/*
	 * Test to see if we have guessed the end of the word
	 * correctly; if not, we can't feed the real string to the
	 * sub-compilation engine, and we're then stuck and so have to
	 * punt out to doing everything at runtime.
	 */
	if (isTokenBraced && *(tokenStartPtr++) != '}') {
	    ckfree((char *)argv);
	    ckfree((char *)bodyTokenArray);
	    return TCL_OUT_LINE_COMPILE;
	}
	if ((tokenStartPtr < tokenPtr[1].start+tokenPtr[1].size)
		&& !isspace(UCHAR(*tokenStartPtr))) {
	    ckfree((char *)argv);
	    ckfree((char *)bodyTokenArray);
	    return TCL_OUT_LINE_COMPILE;
	}
	while (isspace(UCHAR(*tokenStartPtr))) {
	    tokenStartPtr++;
	    if (tokenStartPtr >= tokenPtr[1].start+tokenPtr[1].size) {
		break;
	    }
	}
	if (*tokenStartPtr == '{') {
	    tokenStartPtr++;
	    isTokenBraced = 1;
	} else {
	    isTokenBraced = 0;
	}
    }
    if (tokenStartPtr != tokenPtr[1].start+tokenPtr[1].size) {
	ckfree((char *)argv);
	ckfree((char *)bodyTokenArray);
	fprintf(stderr, "BAD ASSUMPTION\n");
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Complain if the last body is a continuation.  Note that this
     * check assumes that the list is non-empty!
     */

    if (argc>0 && argv[argc-1][0]=='-' && argv[argc-1]=='\0') {
	ckfree((char *)argv);
	ckfree((char *)bodyTokenArray);
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Now we commit to generating code; the parsing stage per se is
     * done.
     *
     * First, we push the value we're matching against on the stack.
     */

    if (valueTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	TclEmitPush(TclRegisterNewLiteral(envPtr, valueTokenPtr[1].start,
		valueTokenPtr[1].size), envPtr);
    } else {
	TclCompileTokens(interp, valueTokenPtr+1,
		valueTokenPtr->numComponents, envPtr);
    }
    
    /*
     * Generate a test for each arm.
     */

    fallThroughArray = (int *) ckalloc(sizeof(int) * argc);
    endOffsetArray = (int *) ckalloc(sizeof(int) * argc);
    lastFalseJump = -1;
    currentFallThroughs = -1;
    endJumpCounter = -1;
    foundDefault = 0;

    for (i=0 ; i<argc ; i+=2) {
	/*
	 * Generate the test for the arm.
	 */

	TclSetStackDepth((savedStackDepth+1), envPtr);
	if (lastFalseJump != -1) {
	    TclSetJumpTarget(envPtr, lastFalseJump);
	    lastFalseJump = -1;
	}
	if (argv[i][0]!='d' || strcmp(argv[i], "default") || i!=argc-2) {
	    switch (mode) {
	    case Switch_Exact:
		TclEmitInst0(INST_DUP, envPtr);
		TclEmitPush(TclRegisterNewLiteral(envPtr, argv[i],
			(int) strlen(argv[i])), envPtr);
		TclEmitInst0(INST_STR_EQ, envPtr);
		break;
	    case Switch_Glob:
		TclEmitPush(TclRegisterNewLiteral(envPtr, argv[i],
			(int) strlen(argv[i])), envPtr);
		TclEmitInst1(INST_OVER, 1, envPtr);
		TclEmitInst1(INST_STR_MATCH, /*nocase*/0, envPtr);
		break;
	    default:
		Tcl_Panic("unknown switch mode: %d",mode);
	    }
	    /*
	     * Process fall-through clauses here...
	     */
	    if (argv[i+1][0]=='-' && argv[i+1][1]=='\0') {
		currentFallThroughs++;
		TclEmitForwardJump(envPtr, INST_JUMP_TRUE,
			fallThroughArray[currentFallThroughs]);
		continue;
	    } else {
		TclEmitForwardJump(envPtr, INST_JUMP_FALSE, lastFalseJump);
	    }
	} else {
	    /*
	     * Got a default clause; set a flag.
	     */
	    foundDefault = 1;
	    /*
	     * Note that default clauses (which are always last
	     * clauses) cannot be fall-through clauses as well,
	     * because the last clause is never a fall-through clause.
	     */
	}

        /*
	 * Generate the body for the arm.  This is guaranteed not to
	 * be a fall-through case, but it might have preceding
	 * fall-through cases, so we must process those first. We also pop the
	 * the value we're matching against. 
	 */

	for (j=0 ; j<=currentFallThroughs; j++) {
	    TclSetJumpTarget(envPtr, fallThroughArray[j]);
	}
	currentFallThroughs = -1;

	TclEmitInst0(INST_POP, envPtr);
	TclSetStackDepth((savedStackDepth), envPtr);

	/*
	 * Now do the actual compilation.
	 */

	TclCompileCmdWord(interp, bodyTokenArray+i+1, 1, envPtr);

	if (!foundDefault) {
	    endJumpCounter++;
	    TclEmitForwardJump(envPtr, INST_JUMP,
		    endOffsetArray[endJumpCounter]);
	}
    }
    ckfree((char *)argv);
    ckfree((char *)bodyTokenArray);

    /*
     * Discard the value we are matching against unless we've had a
     * default clause (in which case it will already be gone) and make
     * the result of the command an empty string.
     */

    if (lastFalseJump != -1) {
	TclSetJumpTarget(envPtr, lastFalseJump);
    }

    TclSetStackDepth((savedStackDepth+1), envPtr);
    if (!foundDefault) {
	TclEmitInst0(INST_POP, envPtr);
	TclEmitPush(TclRegisterNewLiteral(envPtr, "", 0), envPtr);
    }

    /*
     * Fix the jumps to the end.
     */

    for (i=0 ; i<=endJumpCounter ; i++) {
	TclSetJumpTarget(envPtr, endOffsetArray[i]);
    }

    ckfree((char *)fallThroughArray);
    ckfree((char *)endOffsetArray);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileVariableCmd --
 *
 *	Procedure called to reserve the local variables for the 
 *      "variable" command. The command itself is *not* compiled.
 *
 * Results:
 *      Always returns TCL_OUT_LINE_COMPILE.
 *
 * Side effects:
 *      Indexed local variables are added to the environment.
 *
 *----------------------------------------------------------------------
 */
int
TclCompileVariableCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *varTokenPtr;
    int i, numWords;
    CONST char *varName, *tail;

    if (envPtr->procPtr == NULL) {
	return TCL_OUT_LINE_COMPILE;
    }

    numWords = parsePtr->numWords;

    varTokenPtr = parsePtr->tokenPtr
	+ (parsePtr->tokenPtr->numComponents + 1);
    for (i = 1; i < numWords; i += 2) {
	if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    varName = varTokenPtr[1].start;
	    tail = varName + varTokenPtr[1].size - 1;
	    if ((*tail == ')') || (tail < varName)) continue;
	    while ((tail > varName) && ((*tail != ':') || (*(tail-1) != ':'))) {
		tail--;
	    }
	    if ((*tail == ':') && (tail > varName)) {
		tail++;
	    }
	    (void) TclFindCompiledLocal(tail, (tail-varName+1),
		    /*create*/ 1, /*flags*/ 0, envPtr->procPtr);
	    varTokenPtr = varTokenPtr + (varTokenPtr->numComponents + 1);
	}
    }
    return TCL_OUT_LINE_COMPILE;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileWhileCmd --
 *
 *	Procedure called to compile the "while" command.
 *
 * Results:
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "while" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileWhileCmd(interp, parsePtr, envPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Parse *parsePtr;	/* Points to a parse structure for the
				 * command created by Tcl_ParseCommand. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
{
    Tcl_Token *testTokenPtr, *bodyTokenPtr;
    int jumpEvalCondOffset = 0; /* lint */
    int testCodeOffset, bodyCodeOffset, jumpDist;
    int range, code;
    int savedStackDepth = envPtr->currStackDepth;
    int loopMayEnd = 1;         /* This is set to 0 if it is recognized as
				 * an infinite loop. */
    Tcl_Obj *boolObj;
    int boolVal;

    if (parsePtr->numWords != 3) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * If the test expression requires substitutions, don't compile the
     * while command inline. E.g., the expression might cause the loop to
     * never execute or execute forever, as in "while "$x < 5" {}".
     *
     * Bail out also if the body expression requires substitutions
     * in order to insure correct behaviour [Bug 219166]
     */

    testTokenPtr = parsePtr->tokenPtr
	    + (parsePtr->tokenPtr->numComponents + 1);
    bodyTokenPtr = testTokenPtr + (testTokenPtr->numComponents + 1);
    if ((testTokenPtr->type != TCL_TOKEN_SIMPLE_WORD)
	    || (bodyTokenPtr->type != TCL_TOKEN_SIMPLE_WORD)) {
	return TCL_OUT_LINE_COMPILE;
    }

    /*
     * Find out if the condition is a constant. 
     */

    boolObj = Tcl_NewStringObj(testTokenPtr[1].start, testTokenPtr[1].size);
    Tcl_IncrRefCount(boolObj);
    code = Tcl_GetBooleanFromObj(NULL, boolObj, &boolVal);
    Tcl_DecrRefCount(boolObj);
    if (code == TCL_OK) {
	if (boolVal) {
	    /*
	     * it is an infinite loop 
	     */

	    loopMayEnd = 0;  
	} else {
	    /*
	     * This is an empty loop: "while 0 {...}" or such.
	     * Compile no bytecodes.
	     */

	    goto pushResult;
	}
    }

    /*
     * Jump to the evaluation of the condition. This code uses the "loop
     * rotation" optimisation (which eliminates one branch from the loop).
     * "while cond body" produces then:
     *       goto A
     *    B: body                : bodyCodeOffset
     *    A: cond -> result      : testCodeOffset, continueOffset
     *       if (result) goto B
     *
     * The infinite loop "while 1 body" produces:
     *    B: body                : all three offsets here
     *       goto B
     */

    if (loopMayEnd) {
	TclEmitForwardJump(envPtr, INST_JUMP, jumpEvalCondOffset);
	testCodeOffset = 0; /* avoid compiler warning */
    } else {
	testCodeOffset = (envPtr->codeNext - envPtr->codeStart);
    }

    /*
     * Compile the loop body.
     */

    range = TclBeginExceptRange(envPtr);
    bodyCodeOffset = (envPtr->codeNext - envPtr->codeStart);
    TclCompileCmdWord(interp, bodyTokenPtr+1,
	    bodyTokenPtr->numComponents, envPtr);
    TclSetStackDepth((savedStackDepth+1), envPtr);

    /*
     * Avoid compiling a PUSH/POP for loops like 'while 1 {}'
     */

    if (((envPtr->codeNext - envPtr->codeStart) == bodyCodeOffset + 1)
	    && (TclVMGetInstAtPtr(envPtr->codeNext-1) == INST_PUSH)) {
	envPtr->codeNext--;
	TclEndExceptRange(range, envPtr);
	if (!loopMayEnd) {
	    goto finish;
	}
    } else {
	TclEndExceptRange(range, envPtr);
	TclEmitInst0(INST_POP, envPtr);
    }

    /*
     * Compile the test expression then emit the conditional jump that
     * terminates the while. We already know it's a simple word.
     */

    if (loopMayEnd) {
	testCodeOffset = (envPtr->codeNext - envPtr->codeStart);
	TclSetJumpTarget(envPtr, jumpEvalCondOffset);
	TclSetStackDepth((savedStackDepth), envPtr);
	TclCompileExprWords(interp, testTokenPtr, 1, envPtr);
	TclSetStackDepth((savedStackDepth+1), envPtr);

	jumpDist = (envPtr->codeNext - envPtr->codeStart) - bodyCodeOffset;
	TclEmitInst1(INST_JUMP_TRUE, -jumpDist, envPtr);
    } else {
	jumpDist = (envPtr->codeNext - envPtr->codeStart) - bodyCodeOffset;
	TclEmitInst1(INST_JUMP, -jumpDist, envPtr);
    }


    /*
     * Set the loop's body, continue and break offsets.
     */

    finish:
    envPtr->exceptArrayPtr[range].continueOffset = testCodeOffset;
    envPtr->exceptArrayPtr[range].breakOffset =
	    (envPtr->codeNext - envPtr->codeStart);

    /*
     * The while command's result is an empty string.
     */

    pushResult:
    TclSetStackDepth((savedStackDepth), envPtr);
    TclEmitPush(TclRegisterNewLiteral(envPtr, "", 0), envPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PushVarName --
 *
 *	Procedure used in the compiling where pushing a variable name
 *	is necessary (append, lappend, set).
 *
 * Results:
 * 	Returns TCL_OK for a successful compile.
 * 	Returns TCL_OUT_LINE_COMPILE to defer evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "set" command
 *	at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
PushVarName(interp, varTokenPtr, envPtr, flags, localIndexPtr,
	simpleVarNamePtr, isScalarPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Token *varTokenPtr;	/* Points to a variable token. */
    CompileEnv *envPtr;		/* Holds resulting instructions. */
    int flags;			/* takes TCL_CREATE_VAR */
    int *localIndexPtr;		/* must not be NULL */
    int *simpleVarNamePtr;	/* must not be NULL */
    int *isScalarPtr;		/* must not be NULL */
{
    register CONST char *p;
    CONST char *name, *elName;
    register int i, n;
    int nameChars, elNameChars, simpleVarName, localIndex;

    Tcl_Token *elemTokenPtr = NULL;
    int elemTokenCount = 0;
    int allocedTokens = 0;
    int removedParen = 0;
    int stackDepth = envPtr->currStackDepth;

    /*
     * Decide if we can use a frame slot for the var/array name or if we
     * need to emit code to compute and push the name at runtime. We use a
     * frame slot (entry in the array of local vars) if we are compiling a
     * procedure body and if the name is simple text that does not include
     * namespace qualifiers. 
     */

    simpleVarName = 0;
    name = elName = NULL;
    nameChars = elNameChars = 0;
    localIndex = -1;

    /*
     * Check not only that the type is TCL_TOKEN_SIMPLE_WORD, but whether
     * curly braces surround the variable name.
     * This really matters for array elements to handle things like
     *    set {x($foo)} 5
     * which raises an undefined var error if we are not careful here.
     */

    if ((varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) &&
	    (varTokenPtr->start[0] != '{')) {
	/*
	 * A simple variable name. Divide it up into "name" and "elName"
	 * strings. If it is not a local variable, look it up at runtime.
	 */
	simpleVarName = 1;

	name = varTokenPtr[1].start;
	nameChars = varTokenPtr[1].size;
	if (name[nameChars-1] == ')') {
	    /* 
	     * last char is ')' => potential array reference.
	     */

	    for (i=0,p=name ; i<nameChars ; i++,p++) {
		if (*p == '(') {
		    elName = p + 1;
		    elNameChars = nameChars - i - 2;
		    nameChars = i;
		    break;
		}
	    }

	    if ((elName != NULL) && elNameChars) {
		/*
		 * An array element, the element name is a simple
		 * string: assemble the corresponding token.
		 */

		elemTokenPtr = (Tcl_Token *) ckalloc(sizeof(Tcl_Token));
		allocedTokens = 1;
		elemTokenPtr->type = TCL_TOKEN_TEXT;
		elemTokenPtr->start = elName;
		elemTokenPtr->size = elNameChars;
		elemTokenPtr->numComponents = 0;
		elemTokenCount = 1;
	    }
	}
    } else if (((n = varTokenPtr->numComponents) > 1)
	    && (varTokenPtr[1].type == TCL_TOKEN_TEXT)
            && (varTokenPtr[n].type == TCL_TOKEN_TEXT)
            && (varTokenPtr[n].start[varTokenPtr[n].size - 1] == ')')) {

        /*
	 * Check for parentheses inside first token
	 */

        simpleVarName = 0;
        for (i = 0, p = varTokenPtr[1].start; 
	     i < varTokenPtr[1].size; i++, p++) {
            if (*p == '(') {
                simpleVarName = 1;
                break;
            }
        }
        if (simpleVarName) {
	    int remainingChars;

	    /*
	     * Check the last token: if it is just ')', do not count
	     * it. Otherwise, remove the ')' and flag so that it is
	     * restored at the end.
	     */

	    if (varTokenPtr[n].size == 1) {
		--n;
	    } else {
		--varTokenPtr[n].size;
		removedParen = n;
	    }

            name = varTokenPtr[1].start;
            nameChars = p - varTokenPtr[1].start;
            elName = p + 1;
            remainingChars = (varTokenPtr[2].start - p) - 1;
            elNameChars = (varTokenPtr[n].start - p) + varTokenPtr[n].size - 2;

	    if (remainingChars) {
		/*
		 * Make a first token with the extra characters in the first 
		 * token.
		 */

		elemTokenPtr = (Tcl_Token *) ckalloc(n * sizeof(Tcl_Token));
		allocedTokens = 1;
		elemTokenPtr->type = TCL_TOKEN_TEXT;
		elemTokenPtr->start = elName;
		elemTokenPtr->size = remainingChars;
		elemTokenPtr->numComponents = 0;
		elemTokenCount = n;

		/*
		 * Copy the remaining tokens.
		 */

		memcpy((void *) (elemTokenPtr+1), (void *) (&varTokenPtr[2]),
			((n-1) * sizeof(Tcl_Token)));
	    } else {
		/*
		 * Use the already available tokens.
		 */

		elemTokenPtr = &varTokenPtr[2];
		elemTokenCount = n - 1;	    
	    }
	}
    }

    if (simpleVarName) {
	/*
	 * See whether name has any namespace separators (::'s).
	 */

	int hasNsQualifiers = 0;
	for (i = 0, p = name;  i < nameChars;  i++, p++) {
	    if ((*p == ':') && ((i+1) < nameChars) && (*(p+1) == ':')) {
		hasNsQualifiers = 1;
		break;
	    }
	}

	/*
	 * Look up the var name's index in the array of local vars in the
	 * proc frame. If retrieving the var's value and it doesn't already
	 * exist, push its name and look it up at runtime.
	 */

	if ((envPtr->procPtr != NULL) && !hasNsQualifiers) {
	    localIndex = TclFindCompiledLocal(name, nameChars,
		    /*create*/ (flags & TCL_CREATE_VAR),
                    /*flags*/ ((elName==NULL)? 0 : VAR_ARRAY),
		    envPtr->procPtr);
	    if (localIndex >= HPUINT_MAX) {
		/* we'll push the name */
		localIndex = -1;
	    }
	}
	if (localIndex < 0) {
	    TclEmitPush(TclRegisterNewLiteral(envPtr, name, nameChars), envPtr);
	    stackDepth++;
	}

	/*
	 * Compile the element script, if any.
	 */

	if (elName != NULL) {
	    if (elNameChars) {
		TclCompileTokens(interp, elemTokenPtr, elemTokenCount, envPtr);
	    } else {
		TclEmitPush(TclRegisterNewLiteral(envPtr, "", 0), envPtr);
	    }
	    stackDepth++;
	}
    } else {
	/*
	 * The var name isn't simple: compile and push it.
	 */

	TclCompileTokens(interp, varTokenPtr+1,
		varTokenPtr->numComponents, envPtr);
	stackDepth++;
    }

    if (removedParen) {
	++varTokenPtr[removedParen].size;
    }
    if (allocedTokens) {
        ckfree((char *) elemTokenPtr);
    }
    *localIndexPtr	= localIndex;
    *simpleVarNamePtr	= simpleVarName;
    *isScalarPtr	= (elName == NULL);
    TclSetStackDepth(stackDepth, envPtr);
    return TCL_OK;
}
