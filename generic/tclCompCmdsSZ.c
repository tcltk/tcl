/*
 * tclCompCmdsSZ.c --
 *
 *	This file contains compilation procedures that compile various Tcl
 *	commands (beginning with the letters 's' through 'z', except for
 *	[upvar] and [variable]) into a sequence of instructions ("bytecodes").
 *	Also includes the operator command compilers.
 *
 * Copyright © 1997-1998 Sun Microsystems, Inc.
 * Copyright © 2001 Kevin B. Kenny.  All rights reserved.
 * Copyright © 2002 ActiveState Corporation.
 * Copyright © 2004-2025 Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclCompile.h"
#include "tclStringTrim.h"

/*
 * Information about a single arm for [switch]. Used in an array to pass
 * information to the code-issuer functions.
 */
typedef struct SwitchArmInfo {
    Tcl_Token *valueToken;	// The value to match for the arm.
    Tcl_Token *bodyToken;	// The body of an arm; NULL if fall-through.
    Tcl_Size bodyLine;		// The line that the body starts on.
    Tcl_Size *bodyContLines;	// Continuations within the body.
} SwitchArmInfo;

/*
 * Information about a single handler for [try]. Used in an array to pass
 * information to the code-issuer functions.
 */
typedef struct TryHandlerInfo {
    Tcl_Token *tokenPtr;	// The handler body, or NULL for none.
    Tcl_Obj *matchClause;	// The [trap] clause, or NULL for none.
    int matchCode;		// The result code.
    Tcl_LVTIndex resultVar;	// The result variable index, or TCL_INDEX_NONE
    Tcl_LVTIndex optionVar;	// The option variable index, or TCL_INDEX_NONE
} TryHandlerInfo;

/*
 * Prototypes for procedures defined later in this file:
 */

static AuxDataDupProc	DupJumptableInfo;
static AuxDataFreeProc	FreeJumptableInfo;
static AuxDataPrintProc	PrintJumptableInfo;
static AuxDataPrintProc	DisassembleJumptableInfo;
static AuxDataDupProc	DupJumptableNumInfo;
static AuxDataPrintProc	PrintJumptableNumInfo;
static AuxDataPrintProc	DisassembleJumptableNumInfo;
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
static void		IssueSwitchChainedTests(Tcl_Interp *interp,
			    CompileEnv *envPtr, int mode, int noCase,
			    Tcl_Size numArms, SwitchArmInfo *arms);
static void		IssueSwitchJumpTable(Tcl_Interp *interp,
			    CompileEnv *envPtr, int noCase, Tcl_Size numArms,
			    SwitchArmInfo *arms);
static int		IssueTryClausesInstructions(Tcl_Interp *interp,
			    CompileEnv *envPtr, Tcl_Token *bodyToken,
			    Tcl_Size numHandlers, TryHandlerInfo *handlers);
static int		IssueTryTraplessClausesInstructions(Tcl_Interp *interp,
			    CompileEnv *envPtr, Tcl_Token *bodyToken,
			    Tcl_Size numHandlers, TryHandlerInfo *handlers);
static int		IssueTryClausesFinallyInstructions(Tcl_Interp *interp,
			    CompileEnv *envPtr, Tcl_Token *bodyToken,
			    Tcl_Size numHandlers, TryHandlerInfo *handlers,
			    Tcl_Token *finallyToken);
static int		IssueTryTraplessClausesFinallyInstructions(
			    Tcl_Interp *interp, CompileEnv *envPtr,
			    Tcl_Token *bodyToken,
			    Tcl_Size numHandlers, TryHandlerInfo *handlers,
			    Tcl_Token *finallyToken);
static int		IssueTryFinallyInstructions(Tcl_Interp *interp,
			    CompileEnv *envPtr, Tcl_Token *bodyToken,
			    Tcl_Token *finallyToken);

/*
 * The structures below define the AuxData types defined in this file.
 */

const AuxDataType tclJumptableInfoType = {
    "JumptableInfo",		/* name */
    DupJumptableInfo,		/* dupProc */
    FreeJumptableInfo,		/* freeProc */
    PrintJumptableInfo,		/* printProc */
    DisassembleJumptableInfo	/* disassembleProc */
};

const AuxDataType tclJumptableNumericInfoType = {
    "JumptableNumInfo",		/* name */
    DupJumptableNumInfo,	/* dupProc */
    FreeJumptableInfo,		/* freeProc */
    PrintJumptableNumInfo,	/* printProc */
    DisassembleJumptableNumInfo	/* disassembleProc */
};

/*
 *----------------------------------------------------------------------
 *
 * TclCompileSetCmd --
 *
 *	Procedure called to compile the "set" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "set" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileSetCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr, *valueTokenPtr;
    bool isAssignment, isScalar;
    Tcl_Size numWords = parsePtr->numWords;
    Tcl_LVTIndex localIndex;

    if ((numWords != 2) && (numWords != 3)) {
	return TCL_ERROR;
    }
    isAssignment = (numWords == 3);

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
     * If we are doing an assignment, push the new value.
     */

    if (isAssignment) {
	valueTokenPtr = TokenAfter(varTokenPtr);
	PUSH_TOKEN(		valueTokenPtr, 2);
    }

    /*
     * Emit instructions to set/get the variable.
     */

    if (isScalar) {
	if (localIndex < 0) {
	    if (isAssignment) {
		OP(		STORE_STK);
	    } else {
		OP(		LOAD_STK);
	    }
	} else {
	    if (isAssignment) {
		OP4(		STORE_SCALAR, localIndex);
	    } else {
		OP4(		LOAD_SCALAR, localIndex);
	    }
	}
    } else {
	if (localIndex < 0) {
	    if (isAssignment) {
		OP(		STORE_ARRAY_STK);
	    } else {
		OP(		LOAD_ARRAY_STK);
	    }
	} else {
	    if (isAssignment) {
		OP4(		STORE_ARRAY, localIndex);
	    } else {
		OP4(		LOAD_ARRAY, localIndex);
	    }
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileString*Cmd --
 *
 *	Procedures called to compile various subcommands of the "string"
 *	command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "string" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileStringCatCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Size i, numWords = parsePtr->numWords, numArgs;
    Tcl_Token *wordTokenPtr;
    Tcl_Obj *obj, *folded;
    /* TODO: Consider support for compiling expanded args. */

    /* Trivial case, no arg */

    if (numWords < 2) {
	PUSH(			"");
	return TCL_OK;
    }

    /* General case: issue CONCAT1's (by chunks of 254 if needed), folding
     * contiguous constants along the way */

    numArgs = 0;
    folded = NULL;
    wordTokenPtr = TokenAfter(parsePtr->tokenPtr);
    for (i = 1; i < numWords; i++) {
	TclNewObj(obj);
	if (TclWordKnownAtCompileTime(wordTokenPtr, obj)) {
	    if (folded) {
		Tcl_AppendObjToObj(folded, obj);
		Tcl_BounceRefCount(obj);
	    } else {
		folded = obj;
	    }
	} else {
	    Tcl_BounceRefCount(obj);
	    if (folded) {
		PUSH_OBJ(	folded);
		folded = NULL;
		numArgs++;
	    }
	    PUSH_TOKEN(		wordTokenPtr, i);
	    numArgs++;
	    if (numArgs >= 254) { /* 254 to take care of the possible +1 of "folded" above */
		OP1(		STR_CONCAT1, numArgs);
		numArgs = 1;	/* concat pushes 1 obj, the result */
	    }
	}
	wordTokenPtr = TokenAfter(wordTokenPtr);
    }
    if (folded) {
	PUSH_OBJ(		folded);
	folded = NULL;
	numArgs++;
    }
    if (numArgs > 1) {
	OP1(			STR_CONCAT1, numArgs);
    }

    return TCL_OK;
}

int
TclCompileStringCmpCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *aTokenPtr, *bTokenPtr;

    /*
     * We don't support any flags; the bytecode isn't that sophisticated.
     */

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    /*
     * Push the two operands onto the stack and then the test.
     */

    aTokenPtr = TokenAfter(parsePtr->tokenPtr);
    bTokenPtr = TokenAfter(aTokenPtr);
    PUSH_TOKEN(			aTokenPtr, 1);
    PUSH_TOKEN(			bTokenPtr, 2);
    OP(				STR_CMP);
    return TCL_OK;
}

int
TclCompileStringEqualCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *aTokenPtr, *bTokenPtr;

    /*
     * We don't support any flags; the bytecode isn't that sophisticated.
     */

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    /*
     * Push the two operands onto the stack and then the test.
     */

    aTokenPtr = TokenAfter(parsePtr->tokenPtr);
    bTokenPtr = TokenAfter(aTokenPtr);
    PUSH_TOKEN(			aTokenPtr, 1);
    PUSH_TOKEN(			bTokenPtr, 2);
    OP(				STR_EQ);
    return TCL_OK;
}

int
TclCompileStringFirstCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *needleToken, *haystackToken;

    /*
     * We don't support any flags; the bytecode isn't that sophisticated.
     */

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    /*
     * Push the two operands onto the stack and then the test.
     */

    needleToken = TokenAfter(parsePtr->tokenPtr);
    haystackToken = TokenAfter(needleToken);
    PUSH_TOKEN(			needleToken, 1);
    PUSH_TOKEN(			haystackToken, 2);
    OP(				STR_FIND);
    return TCL_OK;
}

int
TclCompileStringLastCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *needleToken, *haystackToken;

    /*
     * We don't support any flags; the bytecode isn't that sophisticated.
     */

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    /*
     * Push the two operands onto the stack and then the test.
     */

    needleToken = TokenAfter(parsePtr->tokenPtr);
    haystackToken = TokenAfter(needleToken);
    PUSH_TOKEN(			needleToken, 1);
    PUSH_TOKEN(			haystackToken, 2);
    OP(				STR_FIND_LAST);
    return TCL_OK;
}

int
TclCompileStringIndexCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *aTokenPtr, *bTokenPtr;

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    /*
     * Push the two operands onto the stack and then the index operation.
     */

    aTokenPtr = TokenAfter(parsePtr->tokenPtr);
    bTokenPtr = TokenAfter(aTokenPtr);
    PUSH_TOKEN(			aTokenPtr, 1);
    PUSH_TOKEN(			bTokenPtr, 2);
    OP(				STR_INDEX);
    return TCL_OK;
}

int
TclCompileStringInsertCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *strToken, *idxToken, *insToken;
    int idx;

    if (parsePtr->numWords != 4) {
	return TCL_ERROR;
    }

    strToken = TokenAfter(parsePtr->tokenPtr);

    /* See what can be discovered about index at compile time */
    idxToken = TokenAfter(strToken);
    if (TCL_OK != TclGetIndexFromToken(idxToken, TCL_INDEX_START,
	    TCL_INDEX_END, &idx)) {

	/* Nothing useful knowable - cease compile; let it direct eval */
	return TCL_ERROR;
    }

    insToken = TokenAfter(idxToken);

    PUSH_TOKEN(			strToken, 1);
    PUSH_TOKEN(			insToken, 3);
    if (idx == (int)TCL_INDEX_START) {
	/* Prepend the insertion string */
	OP(			SWAP);
	OP1(			STR_CONCAT1, 2);
    } else  if (idx == (int)TCL_INDEX_END) {
	/* Append the insertion string */
	OP1(			STR_CONCAT1, 2);
    } else {
	/* Prefix + insertion + suffix */
	if (idx < (int)TCL_INDEX_END) {
	    /* See comments in compiler for [linsert]. */
	    idx++;
	}
	OP4(			OVER, 1);
	OP44(			STR_RANGE_IMM, 0, idx - 1);
	OP4(			REVERSE, 3);
	OP44(			STR_RANGE_IMM, idx, TCL_INDEX_END);
	OP1(			STR_CONCAT1, 3);
    }

    return TCL_OK;
}

int
TclCompileStringIsCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);
    static const char *const isClasses[] = {
	"alnum",	"alpha",	"ascii",	"control",
	"boolean",	"dict",		"digit",	"double",
	"entier",	"false",	"graph",	"integer",
	"list",		"lower",	"print",	"punct",
	"space",	"true",		"upper",
	"wideinteger", "wordchar",	"xdigit",	NULL
    };
    enum isClassesEnum {
	STR_IS_ALNUM,	STR_IS_ALPHA,	STR_IS_ASCII,	STR_IS_CONTROL,
	STR_IS_BOOL,	STR_IS_DICT,	STR_IS_DIGIT,	STR_IS_DOUBLE,
	STR_IS_ENTIER,	STR_IS_FALSE,	STR_IS_GRAPH,	STR_IS_INT,
	STR_IS_LIST,	STR_IS_LOWER,	STR_IS_PRINT,	STR_IS_PUNCT,
	STR_IS_SPACE,	STR_IS_TRUE,	STR_IS_UPPER,
	STR_IS_WIDE,	STR_IS_WORD,	STR_IS_XDIGIT
    } t;
    bool allowEmpty = false;
    Tcl_ExceptionRange range;
    Tcl_BytecodeLabel end;
    InstStringClassType strClassType;
    Tcl_Obj *isClass;

    if (parsePtr->numWords < 3 || parsePtr->numWords > 6) {
	return TCL_ERROR;
    }
    TclNewObj(isClass);
    if (!TclWordKnownAtCompileTime(tokenPtr, isClass)) {
	Tcl_DecrRefCount(isClass);
	return TCL_ERROR;
    } else if (Tcl_GetIndexFromObj(interp, isClass, isClasses, "class", 0,
	    &t) != TCL_OK) {
	Tcl_DecrRefCount(isClass);
	TclCompileSyntaxError(interp, envPtr);
	return TCL_OK;
    }
    Tcl_DecrRefCount(isClass);

    /*
     * Cannot handle the -failindex option at all, and that's the only legal
     * way to have more than 4 arguments.
     */

    if (parsePtr->numWords != 3 && parsePtr->numWords != 4) {
	return TCL_ERROR;
    }

    tokenPtr = TokenAfter(tokenPtr);
    if (parsePtr->numWords == 3) {
	allowEmpty = true;
    } else {
	if (!IS_TOKEN_PREFIX(tokenPtr, 2, "-strict")) {
	    return TCL_ERROR;
	}
	tokenPtr = TokenAfter(tokenPtr);
    }

    /*
     * Compile the code. There are several main classes of check here.
     *	1. Character classes
     *	2. Booleans
     *	3. Integers
     *	4. Floats
     *	5. Lists
     */

    PUSH_TOKEN(			tokenPtr, parsePtr->numWords - 1);

    switch (t) {
    case STR_IS_ALNUM:
	strClassType = STR_CLASS_ALNUM;
	goto compileStrClass;
    case STR_IS_ALPHA:
	strClassType = STR_CLASS_ALPHA;
	goto compileStrClass;
    case STR_IS_ASCII:
	strClassType = STR_CLASS_ASCII;
	goto compileStrClass;
    case STR_IS_CONTROL:
	strClassType = STR_CLASS_CONTROL;
	goto compileStrClass;
    case STR_IS_DIGIT:
	strClassType = STR_CLASS_DIGIT;
	goto compileStrClass;
    case STR_IS_GRAPH:
	strClassType = STR_CLASS_GRAPH;
	goto compileStrClass;
    case STR_IS_LOWER:
	strClassType = STR_CLASS_LOWER;
	goto compileStrClass;
    case STR_IS_PRINT:
	strClassType = STR_CLASS_PRINT;
	goto compileStrClass;
    case STR_IS_PUNCT:
	strClassType = STR_CLASS_PUNCT;
	goto compileStrClass;
    case STR_IS_SPACE:
	strClassType = STR_CLASS_SPACE;
	goto compileStrClass;
    case STR_IS_UPPER:
	strClassType = STR_CLASS_UPPER;
	goto compileStrClass;
    case STR_IS_WORD:
	strClassType = STR_CLASS_WORD;
	goto compileStrClass;
    case STR_IS_XDIGIT:
	strClassType = STR_CLASS_XDIGIT;
    compileStrClass:
	if (allowEmpty) {
	    OP1(		STR_CLASS, strClassType);
	} else {
	    Tcl_BytecodeLabel over, over2;

	    OP(			DUP);
	    OP1(		STR_CLASS, strClassType);
	    FWDJUMP(		JUMP_TRUE, over);
	    OP(			POP);
	    PUSH(		"0");
	    FWDJUMP(		JUMP, over2);
	    FWDLABEL(	over);
	    OP(			IS_EMPTY);
	    OP(			LNOT);
	    FWDLABEL(	over2);
	}
	return TCL_OK;

    case STR_IS_BOOL:
    case STR_IS_FALSE:
    case STR_IS_TRUE:
	OP(			TRY_CVT_TO_BOOLEAN);
	switch (t) {
	    Tcl_BytecodeLabel over, over2;

	case STR_IS_BOOL:
	    if (allowEmpty) {
		FWDJUMP(	JUMP_TRUE, over);
		OP(		IS_EMPTY);
		FWDJUMP(	JUMP, over2);
		FWDLABEL(over);
		OP(		POP);
		PUSH(		"1");
		FWDLABEL(over2);
	    } else {
		OP(		SWAP);
		OP(		POP);
	    }
	    return TCL_OK;
	case STR_IS_TRUE:
	    FWDJUMP(		JUMP_TRUE, over);
	    if (allowEmpty) {
		OP(		IS_EMPTY);
	    } else {
		OP(		POP);
		PUSH(		"0");
	    }
	    FWDJUMP(		JUMP, over2);
	    FWDLABEL(	over);
	    // Normalize the boolean value
	    OP(			LNOT);
	    OP(			LNOT);
	    FWDLABEL(	over2);
	    return TCL_OK;
	case STR_IS_FALSE:
	    FWDJUMP(		JUMP_TRUE, over);
	    if (allowEmpty) {
		OP(		IS_EMPTY);
	    } else {
		OP(		POP);
		PUSH(		"0");
	    }
	    FWDJUMP(		JUMP, over2);
	    FWDLABEL(	over);
	    OP(			LNOT);
	    FWDLABEL(	over2);
	    return TCL_OK;
	default:
	    break;
	}
    break;

    case STR_IS_DOUBLE: {
	Tcl_BytecodeLabel satisfied, isEmpty;

	if (allowEmpty) {
	    OP(			DUP);
	    OP(			IS_EMPTY);
	    FWDJUMP(		JUMP_TRUE, isEmpty);
	    OP(			NUM_TYPE);
	    FWDJUMP(		JUMP_TRUE, satisfied);
	    PUSH(		"0");
	    FWDJUMP(		JUMP, end);
	    FWDLABEL(	isEmpty);
	    OP(			POP);
	    FWDLABEL(	satisfied);
	} else {
	    OP(			NUM_TYPE);
	    FWDJUMP(		JUMP_TRUE, satisfied);
	    PUSH(		"0");
	    FWDJUMP(		JUMP, end);
	    STKDELTA(-1);
	    FWDLABEL(	satisfied);
	}
	PUSH(			"1");
	FWDLABEL(	end);
	return TCL_OK;
    }

    case STR_IS_INT:
    case STR_IS_WIDE:
    case STR_IS_ENTIER:
	if (allowEmpty) {
	    Tcl_BytecodeLabel testNumType;

	    OP(			DUP);
	    OP(			NUM_TYPE);
	    OP(			DUP);
	    FWDJUMP(		JUMP_TRUE, testNumType);
	    OP(			POP);
	    OP(			IS_EMPTY);
	    FWDJUMP(		JUMP, end);
	    STKDELTA(+1);
	    FWDLABEL(	testNumType);
	    OP(			SWAP);
	    OP(			POP);
	} else {
	    OP(			NUM_TYPE);
	    OP(			DUP);
	    FWDJUMP(		JUMP_FALSE, end);
	}

	switch (t) {
	case STR_IS_WIDE:
	    PUSH(		"2");
	    OP(			LE);
	    break;
	case STR_IS_INT:
	case STR_IS_ENTIER:
	    PUSH(		"3");
	    OP(			LE);
	    break;
	default:
	    break;
	}
	FWDLABEL(	end);
	return TCL_OK;
    case STR_IS_DICT:
	range = MAKE_CATCH_RANGE();
	OP4(			BEGIN_CATCH, range);
	OP(			DUP);
	CATCH_RANGE(range) {
	    OP(			DICT_VERIFY);
	}
	CATCH_TARGET(	range);
	OP(			POP);
	OP(			PUSH_RETURN_CODE);
	OP(			END_CATCH);
	OP(			LNOT);
	return TCL_OK;
    case STR_IS_LIST:
	range = MAKE_CATCH_RANGE();
	OP4(			BEGIN_CATCH, range);
	OP(			DUP);
	CATCH_RANGE(range) {
	    OP(			LIST_LENGTH);
	}
	OP(			POP);
	CATCH_TARGET(	range);
	OP(			POP);
	OP(			PUSH_RETURN_CODE);
	OP(			END_CATCH);
	OP(			LNOT);
	return TCL_OK;
    }

    return TclCompileBasicMin0ArgCmd(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileStringMatchCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    bool exactMatch = false;
    int nocase = 0;
    Tcl_Size i, numWords = parsePtr->numWords;

    if (numWords < 3 || numWords > 4) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);

    /*
     * Check if we have a -nocase flag.
     */

    if (numWords == 4) {
	if (!IS_TOKEN_PREFIX(tokenPtr, 2, "-nocase")) {
	    /*
	     * Fail at run time, not in compilation.
	     */

	    return TclCompileBasic3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
	}
	nocase = 1;
	tokenPtr = TokenAfter(tokenPtr);
    }

    /*
     * Push the strings to match against each other.
     */

    for (i = 0; i < 2; i++) {
	if (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD && !nocase && (i == 0)) {
	    /*
	     * Trivial matches can be done by 'string equal'. If -nocase was
	     * specified, we can't do this because INST_STR_EQ has no support
	     * for nocase.
	     */

	    Tcl_Obj *copy = TokenToObj(tokenPtr);
	    exactMatch = TclMatchIsTrivial(TclGetString(copy));
	    Tcl_BounceRefCount(copy);
	}
	PUSH_TOKEN(		tokenPtr, i + 1 + nocase);
	tokenPtr = TokenAfter(tokenPtr);
    }

    /*
     * Push the matcher.
     */

    if (exactMatch) {
	OP(			STR_EQ);
    } else {
	OP1(			STR_MATCH, nocase);
    }
    return TCL_OK;
}

int
TclCompileStringLenCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;
    Tcl_Obj *objPtr;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    TclNewObj(objPtr);
    if (TclWordKnownAtCompileTime(tokenPtr, objPtr)) {
	/*
	 * Here someone is asking for the length of a static string (or
	 * something with backslashes). Just push the actual character (not
	 * byte) length.
	 */

	Tcl_Obj *objLen = Tcl_NewWideUIntObj(Tcl_GetCharLength(objPtr));
	PUSH_OBJ(		objLen);
    } else {
	SetLineInformation(1);
	CompileTokens(envPtr, tokenPtr, interp);
	OP(			STR_LEN);
    }
    TclDecrRefCount(objPtr);
    return TCL_OK;
}

int
TclCompileStringMapCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *mapTokenPtr, *stringTokenPtr;
    Tcl_Obj *mapObj, **objv;
    Tcl_Size len;

    /*
     * We only handle the case:
     *
     *    string map {foo bar} $thing
     *
     * That is, a literal two-element list (doesn't need to be brace-quoted,
     * but does need to be compile-time knowable) and any old argument (the
     * thing to map).
     */

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }
    mapTokenPtr = TokenAfter(parsePtr->tokenPtr);
    stringTokenPtr = TokenAfter(mapTokenPtr);
    TclNewObj(mapObj);
    Tcl_IncrRefCount(mapObj);
    if (!TclWordKnownAtCompileTime(mapTokenPtr, mapObj)) {
	Tcl_DecrRefCount(mapObj);
	return TclCompileBasic2ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    } else if (TclListObjGetElements(NULL, mapObj, &len, &objv) != TCL_OK) {
	Tcl_DecrRefCount(mapObj);
	return TclCompileBasic2ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    } else if (len != 2) {
	Tcl_DecrRefCount(mapObj);
	return TclCompileBasic2ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    /*
     * Now issue the opcodes. Note that in the case that we know that the
     * first word is an empty word, we don't issue the map at all. That is the
     * correct semantics for mapping.
     */

    if (!TclGetString(objv[0])[0]) {
	PUSH_TOKEN(		stringTokenPtr, 2);
    } else {
	PUSH_OBJ(		objv[0]);
	PUSH_OBJ(		objv[1]);
	PUSH_TOKEN(		stringTokenPtr, 2);
	OP(			STR_MAP);
    }
    Tcl_DecrRefCount(mapObj);
    return TCL_OK;
}

int
TclCompileStringRangeCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *stringTokenPtr, *fromTokenPtr, *toTokenPtr;
    int idx1, idx2;

    if (parsePtr->numWords != 4) {
	return TCL_ERROR;
    }
    stringTokenPtr = TokenAfter(parsePtr->tokenPtr);
    fromTokenPtr = TokenAfter(stringTokenPtr);
    toTokenPtr = TokenAfter(fromTokenPtr);

    /* Every path must push the string argument */
    PUSH_TOKEN(			stringTokenPtr, 1);

    /*
     * Parse the two indices.
     */

    if (TclGetIndexFromToken(fromTokenPtr, TCL_INDEX_START, TCL_INDEX_NONE,
	    &idx1) != TCL_OK) {
	goto nonConstantIndices;
    }
    /*
     * Token parsed as an index expression. We treat all indices before
     * the string the same as the start of the string.
     */

    if (idx1 == (int)TCL_INDEX_NONE) {
	/* [string range $s end+1 $last] must be empty string */
	OP(			POP);
	PUSH(			"");
	return TCL_OK;
    }

    if (TclGetIndexFromToken(toTokenPtr, TCL_INDEX_NONE, TCL_INDEX_END,
	    &idx2) != TCL_OK) {
	goto nonConstantIndices;
    }
    /*
     * Token parsed as an index expression. We treat all indices after
     * the string the same as the end of the string.
     */
    if (idx2 == (int)TCL_INDEX_NONE) {
	/* [string range $s $first -1] must be empty string */
	OP(			POP);
	PUSH(			"");
	return TCL_OK;
    }

    /*
     * Push the operand onto the stack and then the substring operation.
     */

    OP44(			STR_RANGE_IMM, idx1, idx2);
    return TCL_OK;

    /*
     * Push the operands onto the stack and then the substring operation.
     */

  nonConstantIndices:
    PUSH_TOKEN(			fromTokenPtr, 2);
    PUSH_TOKEN(			toTokenPtr, 3);
    OP(				STR_RANGE);
    return TCL_OK;
}

int
TclCompileStringReplaceCmd(
    Tcl_Interp *interp,		/* Tcl interpreter for context. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the
				 * command. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr, *valueTokenPtr;
    int first, last;

    if (parsePtr->numWords < 4 || parsePtr->numWords > 5) {
	return TCL_ERROR;
    }

    /* Bytecode to compute/push string argument being replaced */
    valueTokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			valueTokenPtr, 1);

    /*
     * Check for first index known and useful at compile time.
     */
    tokenPtr = TokenAfter(valueTokenPtr);
    if (TclGetIndexFromToken(tokenPtr, TCL_INDEX_START, TCL_INDEX_NONE,
	    &first) != TCL_OK) {
	goto genericReplace;
    }

    /*
     * Check for last index known and useful at compile time.
     */
    tokenPtr = TokenAfter(tokenPtr);
    if (TclGetIndexFromToken(tokenPtr, TCL_INDEX_NONE, TCL_INDEX_END,
	    &last) != TCL_OK) {
	goto genericReplace;
    }

    /*
     * [string replace] is an odd bird.  For many arguments it is
     * a conventional substring replacer.  However it also goes out
     * of its way to become a no-op for many cases where it would be
     * replacing an empty substring.  Precisely, it is a no-op when
     *
     *		(last < first)		OR
     *		(last < 0)		OR
     *		(end < first)
     *
     * For some compile-time values we can detect these cases, and
     * compile direct to bytecode implementing the no-op.
     */

    if ((last == (int)TCL_INDEX_NONE)		/* Know (last < 0) */
	    || (first == (int)TCL_INDEX_NONE)	/* Know (first > end) */

	/*
	 * Tricky to determine when runtime (last < first) can be
	 * certainly known based on the encoded values. Consider the
	 * cases...
	 *
	 * (first <= TCL_INDEX_END) &&
	 *	(last <= TCL_INDEX END) && (last < first) => ACCEPT
	 *	else => cannot tell REJECT
	 */
	    || ((first <= (int)TCL_INDEX_END) && (last <= (int)TCL_INDEX_END)
		&& (last < first))		/* Know (last < first) */
	/*
	 * (first == TCL_INDEX_NONE) &&
	 *	(last <= TCL_INDEX_END) => cannot tell REJECT
	 *	else		=> (first < last) REJECT
	 *
	 * else [[first >= TCL_INDEX_START]] &&
	 *	(last <= TCL_INDEX_END) => cannot tell REJECT
	 *	else [[last >= TCL_INDEX START]] && (last < first) => ACCEPT
	 */
	    || ((first >= (int)TCL_INDEX_START) && (last >= (int)TCL_INDEX_START)
		&& (last < first))) {		/* Know (last < first) */
	if (parsePtr->numWords == 5) {
	    tokenPtr = TokenAfter(tokenPtr);
	    PUSH_TOKEN(		tokenPtr, 4);
	    OP(			POP);		/* Pop newString */
	}
	/* Original string argument now on TOS as result */
	return TCL_OK;
    }

    if (parsePtr->numWords == 5) {
	/*
	 * When we have a string replacement, we have to take care about
	 * not replacing empty substrings that [string replace] promises
	 * not to replace
	 *
	 * The remaining index values might be suitable for conventional
	 * string replacement, but only if they cannot possibly meet the
	 * conditions described above at runtime. If there's a chance they
	 * might, we would have to emit bytecode to check and at that point
	 * we're paying more in bytecode execution time than would make
	 * things worthwhile. Trouble is we are very limited in
	 * how much we can detect that at compile time. After decoding,
	 * we need, first:
	 *
	 *		(first <= end)
	 *
	 * The encoded indices (first <= TCL_INDEX END) and
	 * (first == TCL_INDEX_NONE) always meets this condition, but
	 * any other encoded first index has some list for which it fails.
	 *
	 * We also need, second:
	 *
	 *		(last >= 0)
	 *
	 * The encoded index (last >= TCL_INDEX_START) always meet this
	 * condition but any other encoded last index has some list for
	 * which it fails.
	 *
	 * Finally we need, third:
	 *
	 *		(first <= last)
	 *
	 * Considered in combination with the constraints we already have,
	 * we see that we can proceed when (first == TCL_INDEX_NONE).
	 * These also permit simplification of the prefix|replace|suffix
	 * construction. The other constraints, though, interfere with
	 * getting a guarantee that first <= last.
	 */

	if ((first == (int)TCL_INDEX_START) && (last >= (int)TCL_INDEX_START)) {
	    /* empty prefix */
	    tokenPtr = TokenAfter(tokenPtr);
	    PUSH_TOKEN(		tokenPtr, 4);
	    OP(			SWAP);
	    if (last == INT_MAX) {
		OP(		POP);		/* Pop  original */
	    } else {
		OP44(		STR_RANGE_IMM, last + 1, (int)TCL_INDEX_END);
		OP1(		STR_CONCAT1, 2);
	    }
	    return TCL_OK;
	}

	if ((last == (int)TCL_INDEX_NONE) && (first <= (int)TCL_INDEX_END)) {
	    OP44(		STR_RANGE_IMM, 0, first-1);
	    tokenPtr = TokenAfter(tokenPtr);
	    PUSH_TOKEN(		tokenPtr, 4);
	    OP1(		STR_CONCAT1, 2);
	    return TCL_OK;
	}

	/* FLOW THROUGH TO genericReplace */
    } else {
	/*
	 * When we have no replacement string to worry about, we may
	 * have more luck, because the forbidden empty string replacements
	 * are harmless when they are replaced by another empty string.
	 */

	if (first == (int)TCL_INDEX_START) {
	    /* empty prefix - build suffix only */

	    if (last == (int)TCL_INDEX_END) {
		/* empty suffix too => empty result */
		OP(		POP);		/* Pop original */
		PUSH(		"");
		return TCL_OK;
	    }
	    OP44(		STR_RANGE_IMM, last + 1, TCL_INDEX_END);
	    return TCL_OK;
	} else {
	    if (last == (int)TCL_INDEX_END) {
		/* empty suffix - build prefix only */
		OP44(		STR_RANGE_IMM, 0, first - 1);
		return TCL_OK;
	    }
	    OP(			DUP);
	    OP44(		STR_RANGE_IMM, 0, first - 1);
	    OP(			SWAP);
	    OP44(		STR_RANGE_IMM, last + 1, TCL_INDEX_END);
	    OP1(		STR_CONCAT1, 2);
	    return TCL_OK;
	}
    }

  genericReplace:
    tokenPtr = TokenAfter(valueTokenPtr);
    PUSH_TOKEN(			tokenPtr, 2);
    tokenPtr = TokenAfter(tokenPtr);
    PUSH_TOKEN(			tokenPtr, 3);
    if (parsePtr->numWords == 5) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, 4);
    } else {
	PUSH(			"");
    }
    OP(				STR_REPLACE);
    return TCL_OK;
}

int
TclCompileStringTrimLCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;

    if (parsePtr->numWords != 2 && parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);
    if (parsePtr->numWords == 3) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, 2);
    } else {
	PUSH_STRING(		tclDefaultTrimSet);
    }
    OP(				STR_TRIM_LEFT);
    return TCL_OK;
}

int
TclCompileStringTrimRCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;

    if (parsePtr->numWords != 2 && parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);
    if (parsePtr->numWords == 3) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, 2);
    } else {
	PUSH_STRING(		tclDefaultTrimSet);
    }
    OP(				STR_TRIM_RIGHT);
    return TCL_OK;
}

int
TclCompileStringTrimCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;

    if (parsePtr->numWords != 2 && parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);
    if (parsePtr->numWords == 3) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, 2);
    } else {
	PUSH_STRING(		tclDefaultTrimSet);
    }
    OP(				STR_TRIM);
    return TCL_OK;
}

int
TclCompileStringToUpperCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;

    if (parsePtr->numWords != 2) {
	return TclCompileBasic1To3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);
    OP(				STR_UPPER);
    return TCL_OK;
}

int
TclCompileStringToLowerCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;

    if (parsePtr->numWords != 2) {
	return TclCompileBasic1To3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);
    OP(				STR_LOWER);
    return TCL_OK;
}

int
TclCompileStringToTitleCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;

    if (parsePtr->numWords != 2) {
	return TclCompileBasic1To3ArgCmd(interp, parsePtr, cmdPtr, envPtr);
    }

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);
    OP(				STR_TITLE);
    return TCL_OK;
}

/*
 * Support definitions for the [string is] compilation.
 */

static int
UniCharIsAscii(
    int character)
{
    return (character >= 0) && (character < 0x80);
}

static int
UniCharIsHexDigit(
    int character)
{
    return (character >= 0) && (character < 0x80) && isxdigit(UCHAR(character));
}

StringClassDesc const tclStringClassTable[] = {
    {"alnum",	Tcl_UniCharIsAlnum},
    {"alpha",	Tcl_UniCharIsAlpha},
    {"ascii",	UniCharIsAscii},
    {"control", Tcl_UniCharIsControl},
    {"digit",	Tcl_UniCharIsDigit},
    {"graph",	Tcl_UniCharIsGraph},
    {"lower",	Tcl_UniCharIsLower},
    {"print",	Tcl_UniCharIsPrint},
    {"punct",	Tcl_UniCharIsPunct},
    {"space",	Tcl_UniCharIsSpace},
    {"upper",	Tcl_UniCharIsUpper},
    {"word",	Tcl_UniCharIsWordChar},
    {"xdigit",	UniCharIsHexDigit},
    {"",	NULL}
};

/*
 *----------------------------------------------------------------------
 *
 * TclCompileSubstCmd --
 *
 *	Procedure called to compile the "subst" command.
 *
 * Results:
 *	Returns TCL_OK for successful compile, or TCL_ERROR to defer
 *	evaluation to runtime (either when it is too complex to get the
 *	semantics right, or when we know for sure that it is an error but need
 *	the error to happen at the right time).
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "subst" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileSubstCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Size numArgs = parsePtr->numWords - 1;
    Tcl_Size objc, numOpts = numArgs - 1;
    int flags = TCL_SUBST_ALL;
    Tcl_Obj **objv/*, *toSubst = NULL*/;
    Tcl_Token *wordTokenPtr = TokenAfter(parsePtr->tokenPtr);
    int code = TCL_ERROR;

    if (numArgs == 0 || numArgs > UINT_MAX) {
	return TCL_ERROR;
    }

    objv = (Tcl_Obj **)TclStackAlloc(interp, numOpts * sizeof(Tcl_Obj *));

    for (objc = 0; objc < numOpts; objc++) {
	TclNewObj(objv[objc]);
	Tcl_IncrRefCount(objv[objc]);
	if (!TclWordKnownAtCompileTime(wordTokenPtr, objv[objc])) {
	    objc++;
	    goto cleanup;
	}
	wordTokenPtr = TokenAfter(wordTokenPtr);
    }

/*
    if (TclSubstOptions(NULL, numOpts, objv, &flags) == TCL_OK) {
	toSubst = objv[numOpts];
	Tcl_IncrRefCount(toSubst);
    }
*/

    /* TODO: Figure out expansion to cover WordKnownAtCompileTime
     * The difficulty is that WKACT makes a copy, and if TclSubstParse
     * below parses the copy of the original source string, some deep
     * parts of the compile machinery get upset.  They want all pointers
     * stored in Tcl_Tokens to point back to the same original string.
     */
    if (wordTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	code = TclSubstOptions(NULL, numOpts, objv, &flags);
    }

  cleanup:
    while (--objc >= 0) {
	TclDecrRefCount(objv[objc]);
    }
    TclStackFree(interp, objv);
    if (/*toSubst == NULL*/ code != TCL_OK) {
	return TCL_ERROR;
    }

    SetLineInformation(numArgs);
    TclSubstCompile(interp, wordTokenPtr[1].start, wordTokenPtr[1].size,
	    flags, ExtCmdLocation.line[numArgs], envPtr);

/*    TclDecrRefCount(toSubst);*/
    return TCL_OK;
}

void
TclSubstCompile(
    Tcl_Interp *interp,
    const char *bytes,
    Tcl_Size numBytes,
    int flags,
    int line,
    CompileEnv *envPtr)
{
    Tcl_Token *endTokenPtr, *tokenPtr;
    Tcl_BytecodeLabel breakOffset = 0;
    Tcl_Size count = 0;
    int bline = line;
    Tcl_Parse parse;
    Tcl_InterpState state = NULL;

    TclSubstParse(interp, bytes, numBytes, flags, &parse, &state);
    if (state != NULL) {
	Tcl_ResetResult(interp);
    }

    /*
     * Tricky point! If the first token does not result in a *guaranteed* push
     * of a Tcl_Obj on the stack, we must push an empty object. Otherwise it
     * is possible to get to an INST_STR_CONCAT1 or INST_DONE without enough
     * values on the stack, resulting in a crash. Thanks to Joe Mistachkin for
     * identifying a script that could trigger this case.
     */

    tokenPtr = parse.tokenPtr;
    if (tokenPtr->type != TCL_TOKEN_TEXT && tokenPtr->type != TCL_TOKEN_BS) {
	PUSH(			"");
	count++;
    }

    for (endTokenPtr = tokenPtr + parse.numTokens;
	    tokenPtr < endTokenPtr; tokenPtr = TokenAfter(tokenPtr)) {
	Tcl_Size length;
	Tcl_ExceptionRange catchRange;
	Tcl_BytecodeLabel end, haveOk, haveOther, tableBase;
	JumptableNumInfo *retCodeTable;
	Tcl_AuxDataRef tableIdx;
	char buf[4] = "";

	switch (tokenPtr->type) {
	case TCL_TOKEN_TEXT:
	    PushLiteral(envPtr, tokenPtr->start, tokenPtr->size);
	    TclAdvanceLines(&bline, tokenPtr->start,
		    tokenPtr->start + tokenPtr->size);
	    count++;
	    continue;
	case TCL_TOKEN_BS:
	    length = TclParseBackslash(tokenPtr->start, tokenPtr->size,
		    NULL, buf);
	    PushLiteral(envPtr, buf, length);
	    count++;
	    continue;
	case TCL_TOKEN_VARIABLE:
	    /*
	     * Check for simple variable access; see if we can only generate
	     * TCL_OK or TCL_ERROR from the substituted variable read; if so,
	     * there is no need to generate elaborate exception-management
	     * code. Note that the first component of TCL_TOKEN_VARIABLE is
	     * always TCL_TOKEN_TEXT...
	     */

	    if (tokenPtr->numComponents > 1) {
		Tcl_Size i;
		int foundCommand = 0;

		for (i=2 ; i<=tokenPtr->numComponents ; i++) {
		    if (tokenPtr[i].type == TCL_TOKEN_COMMAND) {
			foundCommand = 1;
			break;
		    }
		}
		if (foundCommand) {
		    break;
		}
	    }

	    envPtr->line = bline;
	    TclCompileVarSubst(interp, tokenPtr, envPtr);
	    bline = envPtr->line;
	    count++;
	    continue;
	}

	while (count > 255) {
	    OP1(		STR_CONCAT1, 255);
	    count -= 254;
	}
	if (count > 1) {
	    OP1(		STR_CONCAT1, count);
	    count = 1;
	}

	if (breakOffset == 0) {
	    Tcl_BytecodeLabel start;
	    /* Jump to the start (jump over the jump to end) */
	    FWDJUMP(		JUMP, start);

	    /* Jump to the end (all BREAKs land here) */
	    FWDJUMP(		JUMP, breakOffset);

	    /* Start */
	    FWDLABEL(	start);
	}

	envPtr->line = bline;
	catchRange = MAKE_CATCH_RANGE();
	OP4(			BEGIN_CATCH, catchRange);
	CATCH_RANGE(catchRange) {
	    switch (tokenPtr->type) {
	    case TCL_TOKEN_COMMAND:
		TclCompileScript(interp, tokenPtr->start+1, tokenPtr->size-2,
			envPtr);
		count++;
		break;
	    case TCL_TOKEN_VARIABLE:
		TclCompileVarSubst(interp, tokenPtr, envPtr);
		count++;
		break;
	    default:
		Tcl_Panic("unexpected token type in TclCompileSubstCmd: %d",
			tokenPtr->type);
	    }
	}

	/* Substitution produced TCL_OK */
	OP(			END_CATCH);
	FWDJUMP(		JUMP, haveOk);
	STKDELTA(-1);

	/* Exceptional return codes processed here */
	CATCH_TARGET(	catchRange);
	OP(			PUSH_RETURN_CODE);

	retCodeTable = AllocJumptableNum();
	tableIdx = RegisterJumptableNum(retCodeTable, envPtr);
	tableBase = CurrentOffset(envPtr);
	OP4(			JUMP_TABLE_NUM, tableIdx);
	FWDJUMP(		JUMP, haveOther);

	/* ERROR -> reraise it; NB: can't require BREAK/CONTINUE handling */
	CreateJumptableNumEntryToHere(retCodeTable, TCL_ERROR, tableBase);
	OP(			PUSH_RETURN_OPTIONS);
	OP(			PUSH_RESULT);
	OP(			END_CATCH); // catchRange
	OP(			RETURN_STK);
	STKDELTA(-1);

	/* BREAK destination */
	CreateJumptableNumEntryToHere(retCodeTable, TCL_BREAK, tableBase);
	OP(			END_CATCH); // catchRange
	BACKJUMP(		JUMP, breakOffset);

	/* CONTINUE destination */
	CreateJumptableNumEntryToHere(retCodeTable, TCL_CONTINUE, tableBase);
	OP(			END_CATCH); // catchRange
	FWDJUMP(		JUMP, end);

	/* RETURN + other destination */
	FWDLABEL(	haveOther);
	OP(			PUSH_RESULT);
	OP(			END_CATCH); // catchRange

	/*
	 * Pull the result to top of stack, discard options dict.
	 */

	/* OK destination */
	FWDLABEL(	haveOk);
	if (count > 1) {
	    OP1(		STR_CONCAT1, count);
	    count = 1;
	}

	/* CONTINUE jump to here */
	FWDLABEL(		end);
	bline = envPtr->line;
    }

    while (count > 255) {
	OP1(			STR_CONCAT1, 255);
	count -= 254;
    }
    if (count > 1) {
	OP1(			STR_CONCAT1, count);
    }

    Tcl_FreeParse(&parse);

    if (state != NULL) {
	Tcl_RestoreInterpState(interp, state);
	TclCompileSyntaxError(interp, envPtr);
	STKDELTA(-1);
    }

    /* Final target of the multi-jump from all BREAKs */
    if (breakOffset > 0) {
	FWDLABEL(		breakOffset);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * HasDefaultClause, IsFallthroughToken, IsFallthroughArm,
 * SetSwitchLineInformation --
 *
 *	Support utilities for [switch] compilation.
 *
 *----------------------------------------------------------------------
 */

static inline bool
HasDefaultClause(
    Tcl_Size numArms,		/* Number of arms describing things the
				 * switch can match against and bodies to
				 * execute when the match succeeds. */
    const SwitchArmInfo *arms)	/* Array of body information. */
{
    const Tcl_Token *finalValue = arms[numArms - 1].valueToken;
    return (finalValue->size == 7) && !memcmp(finalValue->start, "default", 7);
}

static inline bool
IsFallthroughToken(
    const Tcl_Token *tokenPtr)	/* The token to check. */
{
    return (tokenPtr->size == 1) && (tokenPtr->start[0] == '-');
}

static inline bool
IsFallthroughArm(
    const SwitchArmInfo *arm)	/* Which arm to check. */
{
    return arm->bodyToken == NULL;
}

// SetLineInformation() for [switch] bodies
#define SetSwitchLineInformation(arm) \
    do {								\
	envPtr->line = (arm)->bodyLine;		/* TIP #280 */		\
	envPtr->clNext = (arm)->bodyContLines;	/* TIP #280 */		\
    } while (0)

/*
 *----------------------------------------------------------------------
 *
 * TclCompileSwitchCmd --
 *
 *	Procedure called to compile the "switch" command.
 *
 * Results:
 *	Returns TCL_OK for successful compile, or TCL_ERROR to defer
 *	evaluation to runtime (either when it is too complex to get the
 *	semantics right, or when we know for sure that it is an error but need
 *	the error to happen at the right time).
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "switch" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileSwitchCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;	/* Pointer to tokens in command. */
    Tcl_Size numWords;		/* Number of words in command. */

    Tcl_Token *valueTokenPtr;	/* Token for the value to switch on. */
    enum {Switch_Exact, Switch_Glob, Switch_Regexp} mode;
				/* What kind of switch are we doing? */

    Tcl_Token *bodyTokenArray;	/* Array of real pattern list items. */
    SwitchArmInfo *arms;	/* Array of information about switch arms. */
    int noCase;			/* Has the -nocase flag been given? */
    bool foundMode = false;	/* Have we seen a mode flag yet? */
    Tcl_Size i, valueIndex;
    int result = TCL_ERROR;
    Tcl_Size *clNext = envPtr->clNext;

    /*
     * Only handle the following versions:
     *   switch         ?--? word {pattern body ...}
     *   switch -exact  ?--? word {pattern body ...}
     *   switch -glob   ?--? word {pattern body ...}
     *   switch -regexp ?--? word {pattern body ...}
     *   switch         --   word simpleWordPattern simpleWordBody ...
     *   switch -exact  --   word simpleWordPattern simpleWordBody ...
     *   switch -glob   --   word simpleWordPattern simpleWordBody ...
     *   switch -regexp --   word simpleWordPattern simpleWordBody ...
     * When the mode is -glob, can also handle a -nocase flag.
     *
     * First off, we don't care how the command's word was generated; we're
     * compiling it anyway! So skip it...
     */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    valueIndex = 1;
    numWords = parsePtr->numWords - 1;
    if (numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * Check for options.
     */

    noCase = 0;
    mode = Switch_Exact;
    if (numWords == 2) {
	/*
	 * There's just the switch value and the bodies list. In that case, we
	 * can skip all option parsing and move on to consider switch values
	 * and the body list.
	 */

	goto finishedOptionParse;
    }

    /*
     * There must be at least one option, --, because without that there is no
     * way to statically avoid the problems you get from strings-to-be-matched
     * that start with a - (the interpreted code falls apart if it encounters
     * them, so we punt if we *might* encounter them as that is the easiest
     * way of emulating the behaviour).
     */

    for (; numWords>=3 ; tokenPtr=TokenAfter(tokenPtr),numWords--) {
	/*
	 * We only process literal options, and we assume that -e, -g and -n
	 * are unique prefixes of -exact, -glob and -nocase respectively (true
	 * at time of writing). Note that -exact and -glob may only be given
	 * at most once or we bail out (error case).
	 */

	if (IS_TOKEN_PREFIX(tokenPtr, 2, "-exact")) {
	    if (foundMode) {
		return TCL_ERROR;
	    }
	    mode = Switch_Exact;
	    foundMode = true;
	    valueIndex++;
	    continue;
	} else if (IS_TOKEN_PREFIX(tokenPtr, 2, "-glob")) {
	    if (foundMode) {
		return TCL_ERROR;
	    }
	    mode = Switch_Glob;
	    foundMode = true;
	    valueIndex++;
	    continue;
	} else if (IS_TOKEN_PREFIX(tokenPtr, 2, "-regexp")) {
	    if (foundMode) {
		return TCL_ERROR;
	    }
	    mode = Switch_Regexp;
	    foundMode = true;
	    valueIndex++;
	    continue;
	} else if (IS_TOKEN_PREFIX(tokenPtr, 2, "-nocase")) {
	    noCase = 1;
	    valueIndex++;
	    continue;
	} else if (IS_TOKEN_LITERALLY(tokenPtr, "--")) {
	    valueIndex++;
	    break;
	}

	/*
	 * The switch command has many flags we cannot compile at all (e.g.
	 * all the RE-related ones) which we must have encountered. Either
	 * that or we have run off the end. The action here is the same: punt
	 * to interpreted version.
	 */

	return TCL_ERROR;
    }
    if (numWords < 3) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(tokenPtr);
    numWords--;

    /*
     * The value to test against is going to always get pushed on the stack.
     * But not yet; we need to verify that the rest of the command is
     * compilable too.
     */

  finishedOptionParse:
    valueTokenPtr = tokenPtr;
    /* For valueIndex, see previous loop. */
    tokenPtr = TokenAfter(tokenPtr);
    numWords--;

    /*
     * Build an array of tokens for the matcher terms and script bodies. Note
     * that in the case of the quoted bodies, this is tricky as we cannot use
     * copies of the string from the input token for the generated tokens (it
     * causes a crash during exception handling). When multiple tokens are
     * available at this point, this is pretty easy.
     */

    if (numWords == 1) {
	const char *bytes;
	Tcl_Size maxLen, numBytes;
	int bline;		/* TIP #280: line of the pattern/action list,
				 * and start of list for when tracking the
				 * location. This list comes immediately after
				 * the value we switch on. */

	if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    return TCL_ERROR;
	}
	bytes = tokenPtr[1].start;
	numBytes = tokenPtr[1].size;

	/* Allocate enough space to work in. */
	maxLen = TclMaxListLength(bytes, numBytes, NULL);
	if (maxLen < 2)  {
	    return TCL_ERROR;
	}
	bodyTokenArray = (Tcl_Token *) TclStackAlloc(interp,
		sizeof(Tcl_Token) * maxLen);
	arms = (SwitchArmInfo *) TclStackAlloc(interp,
		sizeof(SwitchArmInfo) * maxLen / 2);

	bline = ExtCmdLocation.line[valueIndex + 1];
	numWords = 0;

	/*
	 * Need to be slightly careful; we're iterating over the words of the
	 * list, not the arms of the [switch]. This means we go round this loop
	 * twice per arm. 
	 */

	while (numBytes > 0) {
	    const char *prevBytes = bytes;
	    int literal;
	    bool isProcessingBody = (int)(numWords & 1);
	    SwitchArmInfo *arm = &arms[numWords >> 1];
	    Tcl_Token *fakeToken = &bodyTokenArray[numWords];

	    if (TCL_OK != TclFindElement(NULL, bytes, numBytes,
		    &fakeToken->start, &bytes, &fakeToken->size, &literal)
		    || !literal) {
		goto freeTemporaries;
	    }

	    fakeToken->type = TCL_TOKEN_TEXT;
	    fakeToken->numComponents = 0;
	    if (isProcessingBody) {
		if (IsFallthroughToken(fakeToken)) {
		    arm->bodyToken = NULL;
		} else {
		    arm->bodyToken = fakeToken;
		}
	    } else {
		arm->valueToken = fakeToken;
	    }

	    /*
	     * TIP #280: Now determine the line the list element starts on
	     * (there is no need to do it earlier, due to the possibility of
	     * aborting, see above).
	     * Don't need to record the information for the values; they're
	     * known to be compile-time literals.
	     */

	    TclAdvanceLines(&bline, prevBytes, fakeToken->start);
	    TclAdvanceContinuations(&bline, &clNext,
		    fakeToken->start - envPtr->source);
	    if (isProcessingBody) {
		arm->bodyLine = bline;
		arm->bodyContLines = clNext;
	    }
	    TclAdvanceLines(&bline, fakeToken->start, bytes);
	    TclAdvanceContinuations(&bline, &clNext, bytes - envPtr->source);

	    numBytes -= (bytes - prevBytes);
	    numWords++;
	}
	if (numWords % 2) {
	    goto freeTemporaries;
	}
    } else if (numWords % 2 || numWords == 0) {
	/*
	 * Odd number of words (>1) available, or no words at all available.
	 * Both are error cases, so punt and let the interpreted-version
	 * generate the error message. Note that the second case probably
	 * should get caught earlier, but it's easy to check here again anyway
	 * because it'd cause a nasty crash otherwise.
	 */

	return TCL_ERROR;
    } else {
	/*
	 * Multi-word definition of patterns & actions.
	 */

	bodyTokenArray = NULL;
	arms = (SwitchArmInfo *) TclStackAlloc(interp,
		sizeof(SwitchArmInfo) * numWords / 2);
	for (i=0 ; i<numWords ; i++) {
	    /*
	     * We only handle the very simplest case. Anything more complex is
	     * a good reason to go to the interpreted case anyway due to
	     * traces, etc.
	     */

	    bool isProcessingBody = (bool) (i & 1);
	    SwitchArmInfo *arm = &arms[i >> 1];

	    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
		goto freeTemporaries;
	    }

	    if (isProcessingBody) {
		if (IsFallthroughToken(tokenPtr)) {
		    arm->bodyToken = NULL;
		} else {
		    arm->bodyToken = tokenPtr + 1;
		}
		arm->bodyLine = ExtCmdLocation.line[valueIndex + 1 + i];
		arm->bodyContLines = ExtCmdLocation.next[valueIndex + 1 + i];
	    } else {
		arm->valueToken = tokenPtr + 1;
	    }
	    tokenPtr = TokenAfter(tokenPtr);
	}
    }

    /*
     * Fall back to interpreted if the last body is a continuation (it's
     * illegal, but this makes the error happen at the right time).
     */

    if (IsFallthroughArm(&arms[numWords / 2 - 1])) {
	goto freeTemporaries;
    }

    /*
     * Now we commit to generating code; the parsing stage per se is done.
     * Check if we can generate a jump table, since if so that's faster than
     * doing an explicit compare with each body. Note that we're definitely
     * over-conservative with determining whether we can do the jump table,
     * but it handles the most common case well enough.
     */

    /* Both methods push the value to match against onto the stack. */
    PUSH_TOKEN(			valueTokenPtr, valueIndex);

    if (mode == Switch_Exact) {
	IssueSwitchJumpTable(interp, envPtr, noCase, numWords/2, arms);
    } else {
	IssueSwitchChainedTests(interp, envPtr, mode, noCase, numWords/2, arms);
    }
    result = TCL_OK;

    /*
     * Clean up all our temporary space and return.
     */

  freeTemporaries:
    TclStackFree(interp, arms);
    if (bodyTokenArray != NULL) {
	TclStackFree(interp, bodyTokenArray);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * IssueSwitchChainedTests --
 *
 *	Generate instructions for a [switch] command that is to be compiled
 *	into a sequence of tests. This is the generic handle-everything mode
 *	that inherently has performance that is (on average) linear in the
 *	number of tests. It is the only mode that can handle -glob and -regexp
 *	matches, or anything that is case-insensitive. It does not handle the
 *	wild-and-wooly end of regexp matching (i.e., capture of match results)
 *	so that's when we spill to the interpreted version.
 *
 *	We assume (because it was checked by our caller) that there's at least
 *	one body, all tokens are literals, and all fallthroughs eventually hit
 *	something real.
 *
 *----------------------------------------------------------------------
 */

static void
IssueSwitchChainedTests(
    Tcl_Interp *interp,		/* Context for compiling script bodies. */
    CompileEnv *envPtr,		/* Holds resulting instructions. */
    int mode,			/* Exact, Glob or Regexp */
    int noCase,			/* Case-insensitivity flag. */
    Tcl_Size numArms,		/* Number of arms of the switch. */
    SwitchArmInfo *arms)	/* Array of arm descriptors. */
{
    enum {Switch_Exact, Switch_Glob, Switch_Regexp};
    bool foundDefault;		/* Flag to indicate whether a "default" clause
				 * is present. */
    Tcl_BytecodeLabel *fwdJumps;/* Array of forward-jump fixup locations. */
    Tcl_Size jumpCount;		/* Next cell to use in fwdJumps array. */
    Tcl_Size contJumpIdx;	/* Where the first of the jumps due to a group
				 * of continuation bodies starts, or -1 if
				 * there aren't any. */
    Tcl_Size contJumpCount;	/* Number of continuation bodies pointing to
				 * the current (or next) real body. */
    Tcl_Size nextArmFixupIndex;	/* Index of next issued arm to fix the jump to
				 * the next test for, or -1 if no fix pending. */
    int simple, exact;		/* For extracting the type of regexp. */
    Tcl_Size i, j;

    /*
     * Generate a test for each arm.
     */

    contJumpIdx = NO_PENDING_JUMP;
    contJumpCount = 0;
    fwdJumps = (Tcl_BytecodeLabel *)TclStackAlloc(interp,
	    sizeof(Tcl_BytecodeLabel) * numArms * 2);
    jumpCount = 0;
    foundDefault = false;
    for (i=0 ; i<numArms ; i++) {
	SwitchArmInfo *arm = &arms[i];

	nextArmFixupIndex = NO_PENDING_JUMP;
	if (i != numArms - 1 || !HasDefaultClause(numArms, arms)) {
	    /*
	     * Generate the test for the arm.
	     */

	    switch (mode) {
	    case Switch_Exact:
		OP(		DUP);
		TclCompileTokens(interp, arm->valueToken, 1,	envPtr);
		OP(		STR_EQ);
		break;
	    case Switch_Glob:
		TclCompileTokens(interp, arm->valueToken, 1,	envPtr);
		OP4(		OVER, 1);
		OP1(		STR_MATCH, noCase);
		break;
	    case Switch_Regexp:
		simple = exact = 0;

		/*
		 * Keep in sync with TclCompileRegexpCmd.
		 */

		if (arms[i].valueToken->type == TCL_TOKEN_TEXT) {
		    Tcl_DString ds;

		    if (arms[i].valueToken->size == 0) {
			/*
			 * The semantics of regexps are that they always match
			 * when the RE == "".
			 */

			PUSH(	"1");
			break;
		    }

		    /*
		     * Attempt to convert pattern to glob. If successful, push
		     * the converted pattern.
		     */

		    if (TclReToGlob(NULL, arm->valueToken->start,
			    arm->valueToken->size, &ds, &exact, NULL) == TCL_OK) {
			simple = 1;
			TclPushDString(envPtr, &ds);
			Tcl_DStringFree(&ds);
		    }
		}
		if (!simple) {
		    TclCompileTokens(interp, arm->valueToken, 1, envPtr);
		}

		OP4(		OVER, 1);
		if (!simple) {
		    /*
		     * Pass correct RE compile flags. We use only Int1
		     * (8-bit), but that handles all the flags we want to
		     * pass. Don't use TCL_REG_NOSUB as we may have backrefs
		     * or capture vars.
		     */

		    int cflags = TCL_REG_ADVANCED | (noCase ? TCL_REG_NOCASE : 0);

		    OP1(	REGEXP, cflags);
		} else if (exact && !noCase) {
		    OP(		STR_EQ);
		} else {
		    OP1(	STR_MATCH, noCase);
		}
		break;
	    default:
		Tcl_Panic("unknown switch mode: %d", mode);
	    }

	    /*
	     * In a fall-through case, we will jump on _true_ to the place
	     * where the body starts (generated later, with guarantee of this
	     * ensured earlier; the final body is never a fall-through).
	     */

	    if (IsFallthroughArm(arm)) {
		if (contJumpIdx == NO_PENDING_JUMP) {
		    contJumpIdx = jumpCount;
		    contJumpCount = 0;
		}
		FWDJUMP(	JUMP_TRUE, fwdJumps[contJumpIdx + contJumpCount]);
		jumpCount++;
		contJumpCount++;
		continue;
	    }

	    FWDJUMP(		JUMP_FALSE, fwdJumps[jumpCount]);
	    nextArmFixupIndex = jumpCount++;
	} else {
	    /*
	     * Got a default clause; set a flag to inhibit the generation of
	     * the jump after the body and the cleanup of the intermediate
	     * value that we are switching against.
	     *
	     * Note that default clauses (which are always terminal clauses)
	     * cannot be fall-through clauses as well, since the last clause
	     * is never a fall-through clause (which we have already
	     * verified).
	     */

	    foundDefault = true;
	}

	/*
	 * Generate the body for the arm. This is guaranteed not to be a
	 * fall-through case, but it might have preceding fall-through cases,
	 * so we must process those first.
	 */

	if (contJumpIdx != NO_PENDING_JUMP) {
	    for (j=0 ; j<contJumpCount ; j++) {
		FWDLABEL(   fwdJumps[contJumpIdx + j]);
		fwdJumps[contJumpIdx + j] = 0;
	    }
	    contJumpIdx = NO_PENDING_JUMP;
	}

	/*
	 * Now do the actual compilation. Note that we do not use BODY()
	 * because we may have synthesized the tokens in a non-standard
	 * pattern.
	 */

	OP(			POP);
	SetSwitchLineInformation(arm);
	TclCompileCmdWord(interp, arm->bodyToken, 1, envPtr);

	if (!foundDefault) {
	    FWDJUMP(		JUMP, fwdJumps[jumpCount]);
	    jumpCount++;
	    FWDLABEL(	fwdJumps[nextArmFixupIndex]);
	    fwdJumps[nextArmFixupIndex] = 0;
	}
    }

    /*
     * Discard the value we are matching against unless we've had a default
     * clause (in which case it will already be gone due to the code at the
     * start of processing an arm, guaranteed) and make the result of the
     * command an empty string.
     */

    if (!foundDefault) {
	OP(			POP);
	PUSH(			"");
    }

    /*
     * Do jump fixups for arms that were executed and that haven't already
     * been fixed.
     */

    for (j=0 ; j<jumpCount ; j++) {
	if (fwdJumps[j] != 0) {
	    FWDLABEL(	fwdJumps[j]);
	}
    }

    TclStackFree(interp, fwdJumps);
}

/*
 *----------------------------------------------------------------------
 *
 * IssueSwitchJumpTable --
 *
 *	Generate instructions for a [switch] command that is to be compiled
 *	into a jump table. This only handles the case where case-sensitive,
 *	exact matching is used, but this is actually the most common case in
 *	real code.
 *
 *	We assume (because it was checked by our caller) that there's at least
 *	one body, all tokens are literals, and all fallthroughs eventually hit
 *	something real.
 *
 *----------------------------------------------------------------------
 */

static void
IssueSwitchJumpTable(
    Tcl_Interp *interp,		/* Context for compiling script bodies. */
    CompileEnv *envPtr,		/* Holds resulting instructions. */
    int noCase,			/* Whether to do case-insensitive matches. */
    Tcl_Size numArms,		/* Number of arms of the switch. */
    SwitchArmInfo *arms)	/* Array of arm descriptors. */
{
    JumptableInfo *jtPtr;
    Tcl_AuxDataRef infoIndex;
    int isNew;
    bool mustGenerate, foundDefault;
    Tcl_Size numRealBodies = 0, i;
    Tcl_BytecodeLabel jumpLocation, jumpToDefault, *finalFixups;
    Tcl_DString buffer;

    /*
     * If doing case-insensitive matching, convert to lower case and then do
     * exact string matching.
     */
    if (noCase) {
	OP(			STR_LOWER);
    }

    /*
     * Compile the switch by using a jump table, which is basically a
     * hashtable that maps from literal values to match against to the offset
     * (relative to the INST_JUMP_TABLE instruction) to jump to. The jump
     * table itself is independent of any invocation of the bytecode, and as
     * such is stored in an auxData block.
     *
     * Start by allocating the jump table itself, plus some workspace.
     */

    jtPtr = AllocJumptable();
    infoIndex = RegisterJumptable(jtPtr, envPtr);
    finalFixups = (Tcl_BytecodeLabel *)TclStackAlloc(interp,
	    sizeof(Tcl_BytecodeLabel) * numArms);
    foundDefault = false;
    mustGenerate = true;

    /*
     * Next, issue the instruction to do the jump, together with what we want
     * to do if things do not work out (jump to either the default clause or
     * the "default" default, which just sets the result to empty). Note that
     * we will come back and rewrite the jump's offset parameter when we know
     * what it should be, and that all jumps we issue are of the wide kind
     * because that makes the code much easier to debug!
     */

    BACKLABEL(		jumpLocation);
    OP4(			JUMP_TABLE, infoIndex);
    FWDJUMP(			JUMP, jumpToDefault);

    for (i=0 ; i<numArms ; i++) {
	SwitchArmInfo *arm = &arms[i];

	/*
	 * For each arm, we must first work out what to do with the match
	 * term.
	 */

	if (i != numArms-1 || !HasDefaultClause(numArms, arms)) {
	    /*
	     * This is not a default clause, so insert the current location as
	     * a target in the jump table (assuming it isn't already there,
	     * which would indicate that this clause is probably masked by an
	     * earlier one). Note that we use a Tcl_DString here simply
	     * because the hash API does not let us specify the string length.
	     *
	     * If we're doing case-insensitive matching, we construct the table
	     * with all keys being lower case strings.
	     */

	    Tcl_DStringInit(&buffer);
	    TclDStringAppendToken(&buffer, arm->valueToken);
	    if (noCase) {
		/*
		 * We do case-insensitive matching by conversion to lower case.
		 */

		Tcl_Size slength = Tcl_UtfToLower(Tcl_DStringValue(&buffer));
		Tcl_DStringSetLength(&buffer, slength);
	    }
	    isNew = CreateJumptableEntry(jtPtr, Tcl_DStringValue(&buffer),
		    CurrentOffset(envPtr) - jumpLocation);
	    Tcl_DStringFree(&buffer);
	} else {
	    /*
	     * This is a default clause, so patch up the fallthrough from the
	     * INST_JUMP_TABLE instruction to here.
	     */

	    foundDefault = true;
	    isNew = 1;
	    FWDLABEL(	jumpToDefault);
	}

	/*
	 * Now, for each arm we must deal with the body of the clause.
	 *
	 * If this is a continuation body (never true of a final clause,
	 * whether default or not) we're done because the next jump target
	 * will also point here, so we advance to the next clause.
	 */

	if (IsFallthroughArm(arm)) {
	    mustGenerate = true;
	    continue;
	}

	/*
	 * Also skip this arm if its only match clause is masked. (We could
	 * probably be more aggressive about this, but that would be much more
	 * difficult to get right.)
	 */

	if (!isNew && !mustGenerate) {
	    continue;
	}
	mustGenerate = false;

	/*
	 * Compile the body of the arm.
	 */

	SetSwitchLineInformation(arm);
	TclCompileCmdWord(interp, arm->bodyToken, 1, envPtr);

	/*
	 * Compile a jump in to the end of the command if this body is
	 * anything other than a user-supplied default arm (to either skip
	 * over the remaining bodies or the code that generates an empty
	 * result).
	 */

	if (i < numArms-1 || !foundDefault) {
	    FWDJUMP(		JUMP, finalFixups[numRealBodies++]);
	    STKDELTA(-1);
	}
    }

    /*
     * We're at the end. If we've not already done so through the processing
     * of a user-supplied default clause, add in a "default" default clause
     * now.
     */

    if (!foundDefault) {
	FWDLABEL(	jumpToDefault);
	PUSH(			"");
    }

    /*
     * No more instructions to be issued; everything that needs to jump to the
     * end of the command is fixed up at this point.
     */

    for (i=0 ; i<numRealBodies ; i++) {
	FWDLABEL(	finalFixups[i]);
    }

    /*
     * Clean up all our temporary space and return.
     */

    TclStackFree(interp, finalFixups);
}

/*
 *----------------------------------------------------------------------
 *
 * DupJumptableInfo, FreeJumptableInfo, etc --
 *
 *	Functions to duplicate, release and print jump-tables created for use
 *	with the INST_JUMP_TABLE or INST_JUMP_TABLE_NUM instructions.
 *
 * Results:
 *	DupJumptableInfo: a copy of the jump-table
 *	FreeJumptableInfo: none
 *	PrintJumptableInfo: none
 *	DisassembleJumptableInfo: none
 *	DupJumptableNumInfo: a copy of the jump-table
 *	PrintJumptableNumInfo: none
 *	DisassembleJumptableNumInfo: none
 *
 * Side effects:
 *	DupJumptableInfo: allocates memory
 *	FreeJumptableInfo: releases memory
 *	PrintJumptableInfo: none
 *	DisassembleJumptableInfo: none
 *	DupJumptableNumInfo: allocates memory
 *	PrintJumptableNumInfo: none
 *	DisassembleJumptableNumInfo: none
 *
 *----------------------------------------------------------------------
 */

static void *
DupJumptableInfo(
    void *clientData)
{
    JumptableInfo *jtPtr = (JumptableInfo *)clientData;
    JumptableInfo *newJtPtr = AllocJumptable();
    Tcl_HashEntry *hPtr, *newHPtr;
    Tcl_HashSearch search;

    hPtr = Tcl_FirstHashEntry(&jtPtr->hashTable, &search);
    for (; hPtr ; hPtr = Tcl_NextHashEntry(&search)) {
	newHPtr = Tcl_CreateHashEntry(&newJtPtr->hashTable,
		Tcl_GetHashKey(&jtPtr->hashTable, hPtr), NULL);
	Tcl_SetHashValue(newHPtr, Tcl_GetHashValue(hPtr));
    }
    return newJtPtr;
}

static void
FreeJumptableInfo(
    void *clientData)
{
    JumptableInfo *jtPtr = (JumptableInfo *)clientData;

    Tcl_DeleteHashTable(&jtPtr->hashTable);
    Tcl_Free(jtPtr);
}

static void
PrintJumptableInfo(
    void *clientData,
    Tcl_Obj *appendObj,
    TCL_UNUSED(ByteCode *),
    size_t pcOffset)
{
    JumptableInfo *jtPtr = (JumptableInfo *)clientData;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    const char *keyPtr;
    size_t offset, i = 0;

    hPtr = Tcl_FirstHashEntry(&jtPtr->hashTable, &search);
    for (; hPtr ; hPtr = Tcl_NextHashEntry(&search)) {
	keyPtr = (const char *)Tcl_GetHashKey(&jtPtr->hashTable, hPtr);
	offset = PTR2INT(Tcl_GetHashValue(hPtr));

	if (i++) {
	    Tcl_AppendToObj(appendObj, ", ", TCL_AUTO_LENGTH);
	    if (i % 4 == 0) {
		Tcl_AppendToObj(appendObj, "\n\t\t", TCL_AUTO_LENGTH);
	    }
	}
	Tcl_AppendPrintfToObj(appendObj, "\"%s\"->pc %" TCL_Z_MODIFIER "u",
		keyPtr, pcOffset + offset);
    }
}

static void
DisassembleJumptableInfo(
    void *clientData,
    Tcl_Obj *dictObj,
    TCL_UNUSED(ByteCode *),
    TCL_UNUSED(size_t))
{
    JumptableInfo *jtPtr = (JumptableInfo *)clientData;
    Tcl_Obj *mapping;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    const char *keyPtr;
    size_t offset;

    TclNewObj(mapping);
    hPtr = Tcl_FirstHashEntry(&jtPtr->hashTable, &search);
    for (; hPtr ; hPtr = Tcl_NextHashEntry(&search)) {
	keyPtr = (const char *)Tcl_GetHashKey(&jtPtr->hashTable, hPtr);
	offset = PTR2INT(Tcl_GetHashValue(hPtr));
	TclDictPut(NULL, mapping, keyPtr, Tcl_NewWideIntObj(offset));
    }
    TclDictPut(NULL, dictObj, "mapping", mapping);
}

static void *
DupJumptableNumInfo(
    void *clientData)
{
    JumptableNumInfo *jtnPtr = (JumptableNumInfo *) clientData;
    JumptableNumInfo *newJtnPtr = AllocJumptableNum();
    Tcl_HashEntry *hPtr, *newHPtr;
    Tcl_HashSearch search;

    hPtr = Tcl_FirstHashEntry(&jtnPtr->hashTable, &search);
    for (; hPtr ; hPtr = Tcl_NextHashEntry(&search)) {
	newHPtr = Tcl_CreateHashEntry(&newJtnPtr->hashTable,
		Tcl_GetHashKey(&jtnPtr->hashTable, hPtr), NULL);
	Tcl_SetHashValue(newHPtr, Tcl_GetHashValue(hPtr));
    }
    return newJtnPtr;
}

// No FreeJumptableNumInfo; same as FreeJumptableInfo

static void
PrintJumptableNumInfo(
    void *clientData,
    Tcl_Obj *appendObj,
    TCL_UNUSED(ByteCode *),
    size_t pcOffset)
{
    JumptableNumInfo *jtnPtr = (JumptableNumInfo *)clientData;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_Size key;
    size_t offset, i = 0;

    hPtr = Tcl_FirstHashEntry(&jtnPtr->hashTable, &search);
    for (; hPtr ; hPtr = Tcl_NextHashEntry(&search)) {
	key = (Tcl_Size) Tcl_GetHashKey(&jtnPtr->hashTable, hPtr);
	offset = PTR2INT(Tcl_GetHashValue(hPtr));

	if (i++) {
	    Tcl_AppendToObj(appendObj, ", ", TCL_AUTO_LENGTH);
	    if (i%4==0) {
		Tcl_AppendToObj(appendObj, "\n\t\t", TCL_AUTO_LENGTH);
	    }
	}
	Tcl_AppendPrintfToObj(appendObj,
		"%" TCL_SIZE_MODIFIER "d->pc %" TCL_Z_MODIFIER "u",
		key, pcOffset + offset);
    }
}

static void
DisassembleJumptableNumInfo(
    void *clientData,
    Tcl_Obj *dictObj,
    TCL_UNUSED(ByteCode *),
    TCL_UNUSED(size_t))
{
    JumptableNumInfo *jtnPtr = (JumptableNumInfo *)clientData;
    Tcl_Obj *mapping;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_Size key;
    size_t offset;

    TclNewObj(mapping);
    hPtr = Tcl_FirstHashEntry(&jtnPtr->hashTable, &search);
    for (; hPtr ; hPtr = Tcl_NextHashEntry(&search)) {
	key = (Tcl_Size) Tcl_GetHashKey(&jtnPtr->hashTable, hPtr);
	offset = PTR2INT(Tcl_GetHashValue(hPtr));
	// Cannot fail: keys already known to be unique
	Tcl_DictObjPut(NULL, mapping, Tcl_NewWideIntObj(key),
		Tcl_NewWideIntObj(offset));
    }
    TclDictPut(NULL, dictObj, "mapping", mapping);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileTailcallCmd --
 *
 *	Procedure called to compile the "tailcall" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "tailcall" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileTailcallCmd(
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

    if (numWords < 2 || numWords > UINT_MAX || envPtr->procPtr == NULL) {
	return TCL_ERROR;
    }

    OP(				NS_CURRENT);
    for (i=1 ; i<numWords ; i++) {
	tokenPtr = TokenAfter(tokenPtr);
	// TODO: If the first token is a literal, add LITERAL_CMD_NAME to its flags
	PUSH_TOKEN(		tokenPtr, i);
    }
    OP4(			TAILCALL, numWords);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileThrowCmd --
 *
 *	Procedure called to compile the "throw" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "throw" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileThrowCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Size numWords = parsePtr->numWords;
    Tcl_Token *codeToken, *msgToken;
    Tcl_Obj *objPtr;
    bool codeKnown, codeIsList, codeIsValid;
    Tcl_Size len;

    if (numWords != 3) {
	return TCL_ERROR;
    }
    codeToken = TokenAfter(parsePtr->tokenPtr);
    msgToken = TokenAfter(codeToken);

    TclNewObj(objPtr);
    Tcl_IncrRefCount(objPtr);

    codeKnown = TclWordKnownAtCompileTime(codeToken, objPtr);

    /*
     * First we must emit the code to substitute the arguments.  This
     * must come first in case substitution raises errors.
     */
    if (!codeKnown) {
	PUSH_TOKEN(		codeToken, 1);
	PUSH(			"-errorcode");
    }
    PUSH_TOKEN(			msgToken, 2);

    codeIsList = codeKnown && (TCL_OK ==
	    TclListObjLength(interp, objPtr, &len));
    codeIsValid = codeIsList && (len != 0);

    if (codeIsValid) {
	Tcl_Obj *dictPtr;

	TclNewObj(dictPtr);
	TclDictPut(NULL, dictPtr, "-errorcode", objPtr);
	PUSH_OBJ(		dictPtr);
    }
    TclDecrRefCount(objPtr);

    /*
     * Simpler bytecodes when we detect invalid arguments at compile time.
     */
    if (codeKnown && !codeIsValid) {
	OP(			POP);
	if (codeIsList) {
	    /* Must be an empty list */
	    goto issueErrorForEmptyCode;
	}
	TclCompileSyntaxError(interp, envPtr);
	return TCL_OK;
    }

    if (!codeKnown) {
	Tcl_BytecodeLabel popForError;
	/*
	 * Argument validity checking has to be done by bytecode at
	 * run time.
	 */
	OP4(			REVERSE, 3);
	OP(			DUP);
	OP(			LIST_LENGTH);
	FWDJUMP(		JUMP_FALSE, popForError);
	OP4(			LIST, 2);
	OP44(			RETURN_IMM, TCL_ERROR, 0);

	STKDELTA(+2);
	FWDLABEL(	popForError);
	OP(			POP);
	OP(			POP);
	OP(			POP);
    issueErrorForEmptyCode:
	PUSH(			"type must be non-empty list");
	PUSH(			"-errorcode {TCL OPERATION THROW BADEXCEPTION}");
    }
    OP44(			RETURN_IMM, TCL_ERROR, 0);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileTryCmd --
 *
 *	Procedure called to compile the "try" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "try" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileTryCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Size numHandlers, numWords = parsePtr->numWords;
    int result = TCL_ERROR, anyTrapClauses = 0;
    Tcl_Token *bodyToken, *finallyToken, *tokenPtr;
    TryHandlerInfo staticHandler, *handlers = &staticHandler;
    Tcl_Size handlerIdx = 0;

    if (numWords < 2 || numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    bodyToken = TokenAfter(parsePtr->tokenPtr);
    numWords -= 2;
    tokenPtr = TokenAfter(bodyToken);

    /*
     * Extract information about what handlers there are.
     */

    numHandlers = numWords >> 2;
    numWords -= numHandlers * 4;
    if (numHandlers > 0) {
	if (numHandlers > 1) {
	    handlers = (TryHandlerInfo *)TclStackAlloc(interp,
		    sizeof(TryHandlerInfo) * numHandlers);
	} else {
	    handlers = &staticHandler;
	}

	for (; handlerIdx < numHandlers ; handlerIdx++) {
	    Tcl_Obj *tmpObj, **objv;
	    Tcl_Size objc;

	    if (IS_TOKEN_LITERALLY(tokenPtr, "trap")) {
		/*
		 * Parse the list of errorCode words to match against.
		 */

		handlers[handlerIdx].matchCode = TCL_ERROR;
		tokenPtr = TokenAfter(tokenPtr);
		TclNewObj(tmpObj);
		if (!TclWordKnownAtCompileTime(tokenPtr, tmpObj)
			|| TclListObjLength(NULL, tmpObj, &objc) != TCL_OK
			|| (objc == 0)) {
		    Tcl_BounceRefCount(tmpObj);
		    goto failedToCompile;
		}
		Tcl_ListObjReplace(NULL, tmpObj, 0, 0, 0, NULL);
		Tcl_IncrRefCount(tmpObj);
		handlers[handlerIdx].matchClause = tmpObj;
		anyTrapClauses = 1;
	    } else if (IS_TOKEN_LITERALLY(tokenPtr, "on")) {
		int code;

		/*
		 * Parse the result code to look for.
		 */

		tokenPtr = TokenAfter(tokenPtr);
		TclNewObj(tmpObj);
		if (!TclWordKnownAtCompileTime(tokenPtr, tmpObj)) {
		    Tcl_BounceRefCount(tmpObj);
		    goto failedToCompile;
		}
		if (TCL_ERROR == TclGetCompletionCodeFromObj(NULL, tmpObj, &code)) {
		    Tcl_BounceRefCount(tmpObj);
		    goto failedToCompile;
		}
		handlers[handlerIdx].matchCode = code;
		handlers[handlerIdx].matchClause = NULL;
		Tcl_BounceRefCount(tmpObj);
	    } else {
		goto failedToCompile;
	    }

	    /*
	     * Parse the variable binding.
	     */

	    tokenPtr = TokenAfter(tokenPtr);
	    TclNewObj(tmpObj);
	    if (!TclWordKnownAtCompileTime(tokenPtr, tmpObj)) {
		Tcl_BounceRefCount(tmpObj);
		goto failedToCompile;
	    }
	    if (TclListObjGetElements(NULL, tmpObj, &objc, &objv) != TCL_OK
		    || (objc > 2)) {
		Tcl_BounceRefCount(tmpObj);
		goto failedToCompile;
	    }
	    if (objc > 0) {
		Tcl_Size len;
		const char *varname = TclGetStringFromObj(objv[0], &len);

		handlers[handlerIdx].resultVar = LocalScalar(varname, len, envPtr);
		if (handlers[handlerIdx].resultVar < 0) {
		    Tcl_BounceRefCount(tmpObj);
		    goto failedToCompile;
		}
	    } else {
		handlers[handlerIdx].resultVar = TCL_INDEX_NONE;
	    }
	    if (objc == 2) {
		Tcl_Size len;
		const char *varname = TclGetStringFromObj(objv[1], &len);

		handlers[handlerIdx].optionVar = LocalScalar(varname, len, envPtr);
		if (handlers[handlerIdx].optionVar < 0) {
		    Tcl_BounceRefCount(tmpObj);
		    goto failedToCompile;
		}
	    } else {
		handlers[handlerIdx].optionVar = TCL_INDEX_NONE;
	    }
	    Tcl_BounceRefCount(tmpObj);

	    /*
	     * Extract the body for this handler.
	     */

	    tokenPtr = TokenAfter(tokenPtr);
	    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
		goto failedToCompile;
	    }
	    if (IS_TOKEN_LITERALLY(tokenPtr, "-")) {
		handlers[handlerIdx].tokenPtr = NULL;
	    } else {
		handlers[handlerIdx].tokenPtr = tokenPtr;
	    }

	    tokenPtr = TokenAfter(tokenPtr);
	}

	if (handlers[numHandlers - 1].tokenPtr == NULL) {
	    goto failedToCompile;
	}
    }

    /*
     * Parse the finally clause
     */

    if (numWords == 0) {
	finallyToken = NULL;
    } else if (numWords == 2) {
	if (!IS_TOKEN_LITERALLY(tokenPtr, "finally")) {
	    goto failedToCompile;
	}
	finallyToken = TokenAfter(tokenPtr);
	if (finallyToken->type != TCL_TOKEN_SIMPLE_WORD) {
	    goto failedToCompile;
	}
	// Special case: empty finally clause
	if (TclIsEmptyToken(finallyToken)) {
	    finallyToken = NULL;
	}
    } else {
	goto failedToCompile;
    }

    /*
     * Issue the bytecode.
     */

    if (!finallyToken && numHandlers == 0) {
	/*
	 * No handlers or finally; do nothing beyond evaluating the body.
	 */

	DefineLineInformation;	/* TIP #280 */
	BODY(			bodyToken, 1);
	result = TCL_OK;
    } else if (!finallyToken) {
	if (!anyTrapClauses) {
	    result = IssueTryTraplessClausesInstructions(interp, envPtr,
		    bodyToken, numHandlers, handlers);
	} else {
	    result = IssueTryClausesInstructions(interp, envPtr, bodyToken,
		    numHandlers, handlers);
	}
    } else if (numHandlers == 0) {
	result = IssueTryFinallyInstructions(interp, envPtr, bodyToken,
		finallyToken);
    } else {
	if (!anyTrapClauses) {
	    result = IssueTryTraplessClausesFinallyInstructions(interp, envPtr,
		    bodyToken, numHandlers, handlers, finallyToken);
	} else {
	    result = IssueTryClausesFinallyInstructions(interp, envPtr,
		    bodyToken, numHandlers, handlers, finallyToken);
	}
    }

    /*
     * Delete any temporary state and finish off.
     */

  failedToCompile:
    while (handlerIdx-- > 0) {
	if (handlers[handlerIdx].matchClause) {
	    TclDecrRefCount(handlers[handlerIdx].matchClause);
	}
    }
    if (handlers != &staticHandler) {
	TclStackFree(interp, handlers);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * IssueTryClausesInstructions, IssueTryTraplessClausesInstructions,
 * IssueTryClausesFinallyInstructions, IssueTryFinallyInstructions,
 * IssueTryTraplessClausesFinallyInstructions --
 *
 *	The code generators for [try]. Split from the parsing engine for
 *	reasons of developer sanity, and also split between no-finally,
 *	just-finally and with-finally cases because so many of the details of
 *	generation vary between the three.
 *
 *----------------------------------------------------------------------
 */

static int
IssueTryClausesInstructions(
    Tcl_Interp *interp,
    CompileEnv *envPtr,
    Tcl_Token *bodyToken,
    Tcl_Size numHandlers,	/* Min 1 */
    TryHandlerInfo *handlers)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_LVTIndex resultVar, optionsVar;
    Tcl_Size i, j, len;
    bool continuationsPending = false, trapZero = false;
    Tcl_ExceptionRange range;
    Tcl_BytecodeLabel afterBody = 0, pushReturnOptions = 0;
    Tcl_BytecodeLabel notCodeJumpSource, notECJumpSource, dontSpliceDuring;
    Tcl_BytecodeLabel *continuationJumps, *afterReturn0, *noError;

    resultVar = AnonymousLocal(envPtr);
    optionsVar = AnonymousLocal(envPtr);
    if (resultVar < 0 || optionsVar < 0) {
	return TCL_ERROR;
    }

    /*
     * Check if we're supposed to trap a normal TCL_OK completion of the body.
     * If not, we can handle that case much more efficiently.
     */

    for (i=0 ; i<numHandlers ; i++) {
	if (handlers[i].matchCode == 0) {
	    trapZero = true;
	    break;
	}
    }

    /*
     * Compile the body, trapping any error in it so that we can trap on it
     * and/or run a finally clause. Note that there must be at least one
     * on/trap clause; when none is present, this whole function is not called
     * (and it's never called when there's a finally clause).
     */

    range = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, range);
    CATCH_RANGE(range) {
	BODY(			bodyToken, 1);
    }
    if (!trapZero) {
	OP(			END_CATCH);
	FWDJUMP(		JUMP, afterBody);
	STKDELTA(-1);
    } else {
	PUSH(			"0");
	OP(			SWAP);
	FWDJUMP(		JUMP, pushReturnOptions);
	STKDELTA(-2);
    }
    CATCH_TARGET(	range);
    OP(				PUSH_RETURN_CODE);
    OP(				PUSH_RESULT);
    if (pushReturnOptions > 0) {
	FWDLABEL(	pushReturnOptions);
    }
    OP(				PUSH_RETURN_OPTIONS);
    OP(				END_CATCH);
    OP4(			STORE_SCALAR, optionsVar);
    OP(				POP);
    OP4(			STORE_SCALAR, resultVar);
    OP(				POP);

    /*
     * Now we handle all the registered 'on' and 'trap' handlers in order.
     * For us to be here, there must be at least one handler.
     *
     * Slight overallocation, but reduces size of this function.
     */

    afterReturn0 = (Tcl_BytecodeLabel *)TclStackAlloc(interp,
	    sizeof(Tcl_BytecodeLabel) * numHandlers * 3);
    continuationJumps = afterReturn0 + numHandlers;
    noError = continuationJumps + numHandlers;
    for (i=0; i<numHandlers*3; i++) {
	afterReturn0[i] = NO_PENDING_JUMP;
    }

    for (i=0 ; i<numHandlers ; i++) {
	OP(			DUP);
	PUSH_OBJ(		Tcl_NewIntObj(handlers[i].matchCode));
	OP(			EQ);
	FWDJUMP(		JUMP_FALSE, notCodeJumpSource);
	if (handlers[i].matchClause) {
	    TclListObjLength(NULL, handlers[i].matchClause, &len);

	    /*
	     * Match the errorcode according to try/trap rules.
	     */

	    OP4(		LOAD_SCALAR, optionsVar);
	    PUSH(		"-errorcode");
	    OP4(		DICT_GET, 1);
	    PUSH_OBJ(		handlers[i].matchClause);
	    OP4(		ERROR_PREFIX_EQ, len);
	    FWDJUMP(		JUMP_FALSE, notECJumpSource);
	} else {
	    notECJumpSource = NO_PENDING_JUMP;
	}
	OP(			POP);

	/*
	 * There is no finally clause, so we can avoid wrapping a catch
	 * context around the handler. That simplifies what instructions need
	 * to be issued a lot since we can let errors just fall through.
	 */

	if (handlers[i].resultVar != TCL_INDEX_NONE) {
	    OP4(		LOAD_SCALAR, resultVar);
	    OP4(		STORE_SCALAR, handlers[i].resultVar);
	    OP(			POP);
	}
	if (handlers[i].optionVar != TCL_INDEX_NONE) {
	    OP4(		LOAD_SCALAR, optionsVar);
	    OP4(		STORE_SCALAR, handlers[i].optionVar);
	    OP(			POP);
	}
	if (!handlers[i].tokenPtr) {
	    continuationsPending = true;
	    FWDJUMP(		JUMP, continuationJumps[i]);
	    STKDELTA(+1);
	} else {
	    if (continuationsPending) {
		continuationsPending = false;
		for (j=0 ; j<i ; j++) {
		    if (continuationJumps[j] != NO_PENDING_JUMP) {
			FWDLABEL(continuationJumps[j]);
		    }
		    continuationJumps[j] = NO_PENDING_JUMP;
		}
	    }

	    if (TclIsEmptyToken(handlers[i].tokenPtr)) {
		// Empty handler body; can't generate non-trivial result tuple
		PUSH(		"");
		FWDJUMP(	JUMP, noError[i]);
	    } else {
		range = MAKE_CATCH_RANGE();
		OP4(		BEGIN_CATCH, range);
		CATCH_RANGE(range) {
		    BODY(	handlers[i].tokenPtr, 5 + i*4);
		}
		OP(		END_CATCH);
		FWDJUMP(	JUMP, noError[i]);

		STKDELTA(-1);
		CATCH_TARGET(range);
		OP(		PUSH_RESULT);
		OP(		PUSH_RETURN_OPTIONS);
		OP(		PUSH_RETURN_CODE);
		OP(		END_CATCH);

		PUSH(		"1");
		OP(		EQ);
		FWDJUMP(	JUMP_FALSE, dontSpliceDuring);
		// Next bit isn't DICT_SET; alter which dict is in optionsVar
		PUSH(		"-during");
		OP4(		LOAD_SCALAR, optionsVar);
		OP(		DICT_PUT);
		FWDLABEL(	dontSpliceDuring);

		OP(		SWAP);
		INVOKE(		RETURN_STK);
		FWDJUMP(	JUMP, afterReturn0[i]);
	    }
	}

	if (handlers[i].matchClause) {
	    FWDLABEL(	notECJumpSource);
	}
	FWDLABEL(	notCodeJumpSource);
    }

    /*
     * Drop the result code since it didn't match any clause, and reissue the
     * exception. Note also that INST_RETURN_STK can proceed to the next
     * instruction.
     */

    OP(				POP);
    OP4(			LOAD_SCALAR, optionsVar);
    OP4(			LOAD_SCALAR, resultVar);
    INVOKE(			RETURN_STK);

    /*
     * Fix all the jumps from taken clauses to here (which is the end of the
     * [try]).
     */

    if (!trapZero) {
	FWDLABEL(	afterBody);
    }
    for (i=0 ; i<numHandlers ; i++) {
	if (afterReturn0[i] != NO_PENDING_JUMP) {
	    FWDLABEL(	afterReturn0[i]);
	}
	if (noError[i] != NO_PENDING_JUMP) {
	    FWDLABEL(	noError[i]);
	}
    }
    TclStackFree(interp, afterReturn0);
    return TCL_OK;
}

static int
IssueTryTraplessClausesInstructions(
    Tcl_Interp *interp,
    CompileEnv *envPtr,
    Tcl_Token *bodyToken,
    Tcl_Size numHandlers,	/* Min 1 */
    TryHandlerInfo *handlers)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_LVTIndex resultVar, optionsVar;
    Tcl_Size i, j;
    int continuationsPending = 0, trapZero = 0;
    Tcl_ExceptionRange range;
    Tcl_BytecodeLabel afterBody = 0, pushReturnOptions = 0;
    Tcl_BytecodeLabel dontSpliceDuring, tableBase, haveOther;
    Tcl_BytecodeLabel *continuationJumps, *afterReturn0, *noError;
    JumptableNumInfo *tablePtr;
    Tcl_AuxDataRef tableIdx;

    resultVar = AnonymousLocal(envPtr);
    optionsVar = AnonymousLocal(envPtr);
    if (resultVar < 0 || optionsVar < 0) {
	return TCL_ERROR;
    }
    afterReturn0 = (Tcl_BytecodeLabel *)TclStackAlloc(interp,
	    sizeof(Tcl_BytecodeLabel) * numHandlers * 3);
    continuationJumps = afterReturn0 + numHandlers;
    noError = continuationJumps + numHandlers;
    for (i=0; i<numHandlers*3; i++) {
	afterReturn0[i] = NO_PENDING_JUMP;
    }
    tablePtr = AllocJumptableNum();
    tableIdx = RegisterJumptableNum(tablePtr, envPtr);

    /*
     * Check if we're supposed to trap a normal TCL_OK completion of the body.
     * If not, we can handle that case much more efficiently.
     */

    for (i=0 ; i<numHandlers ; i++) {
	if (handlers[i].matchCode == 0) {
	    trapZero = 1;
	    break;
	}
    }

    /*
     * Compile the body, trapping any error in it so that we can trap on it
     * and/or run a finally clause. Note that there must be at least one
     * on/trap clause; when none is present, this whole function is not called
     * (and it's never called when there's a finally clause).
     */

    range = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, range);
    CATCH_RANGE(range) {
	BODY(			bodyToken, 1);
    }
    if (!trapZero) {
	OP(			END_CATCH);
	FWDJUMP(		JUMP, afterBody);
	STKDELTA(-1);
    } else {
	PUSH(			"0");
	OP(			SWAP);
	FWDJUMP(		JUMP, pushReturnOptions);
	STKDELTA(-2);
    }
    CATCH_TARGET(	range);
    OP(				PUSH_RETURN_CODE);
    OP(				PUSH_RESULT);
    if (pushReturnOptions > 0) {
	FWDLABEL(	pushReturnOptions);
    }
    OP(				PUSH_RETURN_OPTIONS);
    OP(				END_CATCH);
    OP4(			STORE_SCALAR, optionsVar);
    OP(				POP);
    OP4(			STORE_SCALAR, resultVar);
    OP(				POP);

    /*
     * Now we handle all the registered 'on' handlers.
     * For us to be here, there must be at least one handler.
     *
     * Slight overallocation, but reduces size of this function.
     */

    BACKLABEL(		tableBase);
    OP4(			JUMP_TABLE_NUM, tableIdx);
    FWDJUMP(		JUMP, haveOther);
    for (i=0 ; i<numHandlers ; i++) {
	CreateJumptableNumEntryToHere(tablePtr, handlers[i].matchCode, tableBase);

	/*
	 * There is no finally clause, so we can avoid wrapping a catch
	 * context around the handler. That simplifies what instructions need
	 * to be issued a lot since we can let errors just fall through.
	 */

	if (handlers[i].resultVar != TCL_INDEX_NONE) {
	    OP4(		LOAD_SCALAR, resultVar);
	    OP4(		STORE_SCALAR, handlers[i].resultVar);
	    OP(			POP);
	}
	if (handlers[i].optionVar != TCL_INDEX_NONE) {
	    OP4(		LOAD_SCALAR, optionsVar);
	    OP4(		STORE_SCALAR, handlers[i].optionVar);
	    OP(			POP);
	}
	if (!handlers[i].tokenPtr) {
	    continuationsPending = 1;
	    FWDJUMP(		JUMP, continuationJumps[i]);
	} else {
	    if (continuationsPending) {
		continuationsPending = 0;
		for (j=0 ; j<i ; j++) {
		    if (continuationJumps[j] != NO_PENDING_JUMP) {
			FWDLABEL(continuationJumps[j]);
		    }
		    continuationJumps[j] = NO_PENDING_JUMP;
		}
	    }

	    if (TclIsEmptyToken(handlers[i].tokenPtr)) {
		// Empty handler body; can't generate non-trivial result tuple
		PUSH(		"");
		FWDJUMP(	JUMP, noError[i]);
	    } else {
		range = MAKE_CATCH_RANGE();
		OP4(		BEGIN_CATCH, range);
		CATCH_RANGE(range) {
		    BODY(	handlers[i].tokenPtr, 5 + i*4);
		}
		OP(		END_CATCH);
		FWDJUMP(	JUMP, noError[i]);

		STKDELTA(-1);
		CATCH_TARGET(range);
		OP(		PUSH_RESULT);
		OP(		PUSH_RETURN_OPTIONS);
		OP(		PUSH_RETURN_CODE);
		OP(		END_CATCH);

		PUSH(		"1");
		OP(		EQ);
		FWDJUMP(	JUMP_FALSE, dontSpliceDuring);
		// Next bit isn't DICT_SET; alter which dict is in optionsVar
		PUSH(		"-during");
		OP4(		LOAD_SCALAR, optionsVar);
		OP(		DICT_PUT);
		FWDLABEL(	dontSpliceDuring);

		OP(		SWAP);
		INVOKE(		RETURN_STK);
		FWDJUMP(	JUMP, afterReturn0[i]);
	    }
	    STKDELTA(-1);
	}
    }

    /*
     * Drop the result code since it didn't match any clause, and reissue the
     * exception. Note also that INST_RETURN_STK can proceed to the next
     * instruction.
     */

    FWDLABEL(		haveOther);
    OP4(			LOAD_SCALAR, optionsVar);
    OP4(			LOAD_SCALAR, resultVar);
    INVOKE(			RETURN_STK);

    /*
     * Fix all the jumps from taken clauses to here (which is the end of the
     * [try]).
     */

    if (!trapZero) {
	FWDLABEL(	afterBody);
    }
    for (i=0 ; i<numHandlers ; i++) {
	if (afterReturn0[i] != NO_PENDING_JUMP) {
	    FWDLABEL(	afterReturn0[i]);
	}
	if (noError[i] != NO_PENDING_JUMP) {
	    FWDLABEL(	noError[i]);
	}
    }
    TclStackFree(interp, afterReturn0);
    return TCL_OK;
}

static int
IssueTryClausesFinallyInstructions(
    Tcl_Interp *interp,
    CompileEnv *envPtr,
    Tcl_Token *bodyToken,
    Tcl_Size numHandlers,	/* Min 1 */
    TryHandlerInfo *handlers,
    Tcl_Token *finallyToken)	/* Not NULL */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_LVTIndex resultLocal, optionsLocal;
    Tcl_Size i, j, len;
    bool forwardsNeedFixing = false, trapZero = false;
    Tcl_ExceptionRange range;
    Tcl_BytecodeLabel *addrsToFix, *forwardsToFix;
    Tcl_BytecodeLabel finalOK, dontSpliceDuring;
    Tcl_BytecodeLabel pushReturnOptions = 0, endCatch = 0, afterBody = 0;

    resultLocal = AnonymousLocal(envPtr);
    optionsLocal = AnonymousLocal(envPtr);
    if (resultLocal < 0 || optionsLocal < 0) {
	return TCL_ERROR;
    }

    /*
     * Check if we're supposed to trap a normal TCL_OK completion of the body.
     * If not, we can handle that case much more efficiently.
     */

    for (i=0 ; i<numHandlers ; i++) {
	if (handlers[i].matchCode == 0) {
	    trapZero = true;
	    break;
	}
    }

    /*
     * Compile the body, trapping any error in it so that we can trap on it
     * (if any trap matches) and run a finally clause.
     */

    range = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, range);
    CATCH_RANGE(range) {
	BODY(			bodyToken, 1);
    }
    if (!trapZero) {
	OP(			END_CATCH);
	OP4(			STORE_SCALAR, resultLocal);
	OP(			POP);
	PUSH(			"-level 0 -code 0");
	OP4(			STORE_SCALAR, optionsLocal);
	OP(			POP);
	FWDJUMP(		JUMP, afterBody);
    } else {
	/*
	 * Fake a return code to go with our result.
	 */
	PUSH(			"0");
	OP(			SWAP);
	FWDJUMP(		JUMP, pushReturnOptions);
	STKDELTA(-2);
    }
    CATCH_TARGET(	range);
    OP(				PUSH_RETURN_CODE);
    OP(				PUSH_RESULT);
    if (pushReturnOptions) {
	FWDLABEL(	pushReturnOptions);
    }
    OP(				PUSH_RETURN_OPTIONS);
    OP(				END_CATCH);
    OP4(			STORE_SCALAR, optionsLocal);
    OP(				POP);
    OP4(			STORE_SCALAR, resultLocal);
    OP(				POP);

    /*
     * Now we handle all the registered 'on' and 'trap' handlers in order.
     *
     * Slight overallocation, but reduces size of this function.
     */

    addrsToFix = (Tcl_BytecodeLabel *)TclStackAlloc(interp,
	    sizeof(Tcl_BytecodeLabel) * numHandlers * 2);
    forwardsToFix = addrsToFix + numHandlers;

    for (i=0 ; i<numHandlers ; i++) {
	Tcl_BytecodeLabel codeNotMatched, notErrorCodeMatched = NO_PENDING_JUMP;

	OP(			DUP);
	PUSH_OBJ(		Tcl_NewIntObj(handlers[i].matchCode));
	OP(			EQ);
	FWDJUMP(		JUMP_FALSE, codeNotMatched);
	if (handlers[i].matchClause) {
	    TclListObjLength(NULL, handlers[i].matchClause, &len);

	    /*
	     * Match the errorcode according to try/trap rules.
	     */

	    OP4(		LOAD_SCALAR, optionsLocal);
	    PUSH(		"-errorcode");
	    OP4(		DICT_GET, 1);
	    PUSH_OBJ(		handlers[i].matchClause);
	    OP4(		ERROR_PREFIX_EQ, len);
	    FWDJUMP(		JUMP_FALSE, notErrorCodeMatched);
	}
	OP(			POP);

	/*
	 * There is a finally clause, so we need a fairly complex sequence of
	 * instructions to deal with an on/trap handler because we must call
	 * the finally handler *and* we need to substitute the result from a
	 * failed trap for the result from the main script.
	 */

	if (handlers[i].resultVar != TCL_INDEX_NONE
		|| handlers[i].optionVar != TCL_INDEX_NONE
		|| handlers[i].tokenPtr) {
	    range = MAKE_CATCH_RANGE();
	    OP4(		BEGIN_CATCH, range);
	    ExceptionRangeStarts(envPtr, range);
	}
	if (handlers[i].resultVar != TCL_INDEX_NONE
		|| handlers[i].optionVar != TCL_INDEX_NONE) {
	    if (handlers[i].resultVar != TCL_INDEX_NONE) {
		OP4(		LOAD_SCALAR, resultLocal);
		OP4(		STORE_SCALAR, handlers[i].resultVar);
		OP(		POP);
	    }
	    if (handlers[i].optionVar != TCL_INDEX_NONE) {
		OP4(		LOAD_SCALAR, optionsLocal);
		OP4(		STORE_SCALAR, handlers[i].optionVar);
		OP(		POP);
	    }

	    if (!handlers[i].tokenPtr) {
		/*
		 * No handler. Will not be the last handler (that is a
		 * condition that is checked by the caller). Chain to the next
		 * one.
		 */

		ExceptionRangeEnds(envPtr, range);
		OP(		END_CATCH);
		forwardsNeedFixing = true;
		FWDJUMP(	JUMP, forwardsToFix[i]);
		goto finishTrapCatchHandling;
	    }
	} else if (!handlers[i].tokenPtr) {
	    /*
	     * No handler. Will not be the last handler (that condition is
	     * checked by the caller). Chain to the next one.
	     */

	    forwardsNeedFixing = true;
	    FWDJUMP(		JUMP, forwardsToFix[i]);
	    goto endOfThisArm;
	}

	/*
	 * Got a handler. Make sure that any pending patch-up actions from
	 * previous unprocessed handlers are dealt with now that we know where
	 * they are to jump to.
	 */

	if (forwardsNeedFixing) {
	    Tcl_BytecodeLabel bodyStart;
	    forwardsNeedFixing = false;
	    FWDJUMP(		JUMP, bodyStart);
	    for (j=0 ; j<i ; j++) {
		if (forwardsToFix[j] == NO_PENDING_JUMP) {
		    continue;
		}
		FWDLABEL(forwardsToFix[j]);
		forwardsToFix[j] = NO_PENDING_JUMP;
	    }
	    OP4(		BEGIN_CATCH, range);
	    FWDLABEL(	bodyStart);
	}
	// TODO: Simplify based on TclIsEmptyToken(handlers[i].tokenPtr)
	BODY(			handlers[i].tokenPtr, 5 + i*4);
	ExceptionRangeEnds(envPtr, range);
	PUSH(			"0");
	OP(			PUSH_RETURN_OPTIONS);
	OP4(			REVERSE, 3);
	FWDJUMP(		JUMP, endCatch);
	STKDELTA(-3);
	forwardsToFix[i] = NO_PENDING_JUMP;

	/*
	 * Error in handler or setting of variables; replace the stored
	 * exception with the new one. Note that we only push this if we have
	 * either a body or some variable setting here. Otherwise this code is
	 * unreachable.
	 */

    finishTrapCatchHandling:
	CATCH_TARGET(	range);
	OP(			PUSH_RETURN_OPTIONS);
	OP(			PUSH_RETURN_CODE);
	OP(			PUSH_RESULT);
	if (endCatch) {
	    FWDLABEL(	endCatch);
	}
	OP(			END_CATCH);
	OP4(			STORE_SCALAR, resultLocal);
	OP(			POP);

	PUSH(			"1");
	OP(			EQ);
	FWDJUMP(		JUMP_FALSE, dontSpliceDuring);
	// Next bit isn't DICT_SET; alter which dict is in optionsLocal
	PUSH(			"-during");
	OP4(			LOAD_SCALAR, optionsLocal);
	OP(			DICT_PUT);
	FWDLABEL(	dontSpliceDuring);

	OP4(			STORE_SCALAR, optionsLocal);

	/* Skip POP at end; can clean up with subsequent POP */
	if (i+1 < numHandlers) {
	    OP(			POP);
	}

    endOfThisArm:
	if (i+1 < numHandlers) {
	    FWDJUMP(		JUMP, addrsToFix[i]);
	    STKDELTA(+1);
	}
	if (handlers[i].matchClause) {
	    FWDLABEL(	notErrorCodeMatched);
	}
	FWDLABEL(	codeNotMatched);
    }

    /*
     * Drop the result code, and fix all the jumps from taken clauses - which
     * drop the result code as their first action - to point straight after
     * (i.e., to the start of the finally clause).
     */

    OP(				POP);
    for (i=0 ; i<numHandlers-1 ; i++) {
	FWDLABEL(	addrsToFix[i]);
    }
    TclStackFree(interp, addrsToFix);

    /*
     * Process the finally clause (at last!) Note that we do not wrap this in
     * error handlers because we would just rethrow immediately anyway. Then
     * (on normal success) we reissue the exception. Note also that
     * INST_RETURN_STK can proceed to the next instruction; that'll be the
     * next command (or some inter-command manipulation).
     */

    if (!trapZero) {
	FWDLABEL(	afterBody);
    }
    range = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, range);
    CATCH_RANGE(range) {
	BODY(			finallyToken, 3 + 4*numHandlers);
    }
    OP(				END_CATCH);
    OP(				POP);
    FWDJUMP(			JUMP, finalOK);

    CATCH_TARGET(	range);
    OP(				PUSH_RESULT);
    OP(				PUSH_RETURN_OPTIONS);
    OP(				PUSH_RETURN_CODE);
    OP(				END_CATCH);

    PUSH(			"1");
    OP(				EQ);
    FWDJUMP(			JUMP_FALSE, dontSpliceDuring);
    // Next bit isn't DICT_SET; alter which dict is in optionsLocal
    PUSH(			"-during");
    OP4(			LOAD_SCALAR, optionsLocal);
    OP(				DICT_PUT);
    FWDLABEL(		dontSpliceDuring);

    OP4(			STORE_SCALAR, optionsLocal);
    OP(				POP);

    OP4(			STORE_SCALAR, resultLocal);
    OP(				POP);

    FWDLABEL(		finalOK);
    OP4(			LOAD_SCALAR, optionsLocal);
    OP4(			LOAD_SCALAR, resultLocal);
    INVOKE(			RETURN_STK);

    return TCL_OK;
}

static int
IssueTryTraplessClausesFinallyInstructions(
    Tcl_Interp *interp,
    CompileEnv *envPtr,
    Tcl_Token *bodyToken,
    Tcl_Size numHandlers,	/* Min 1 */
    TryHandlerInfo *handlers,
    Tcl_Token *finallyToken)	/* Not NULL */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_LVTIndex resultLocal, optionsLocal;
    Tcl_Size i, j;
    int forwardsNeedFixing = 0, trapZero = 0;
    Tcl_ExceptionRange range;
    Tcl_BytecodeLabel *addrsToFix, *forwardsToFix;
    Tcl_BytecodeLabel finalOK, dontSpliceDuring, tableBase, haveOther;
    Tcl_BytecodeLabel pushReturnOptions = 0, afterBody = 0;
    JumptableNumInfo *tablePtr;
    Tcl_AuxDataRef tableIdx;

    resultLocal = AnonymousLocal(envPtr);
    optionsLocal = AnonymousLocal(envPtr);
    if (resultLocal < 0 || optionsLocal < 0) {
	return TCL_ERROR;
    }
    addrsToFix = (Tcl_BytecodeLabel *)TclStackAlloc(interp,
	    sizeof(Tcl_BytecodeLabel) * numHandlers * 2);
    forwardsToFix = addrsToFix + numHandlers;
    for (i=0; i < numHandlers * 2; i++) {
	addrsToFix[i] = NO_PENDING_JUMP;
    }
    tablePtr = AllocJumptableNum();
    tableIdx = RegisterJumptableNum(tablePtr, envPtr);

    /*
     * Check if we're supposed to trap a normal TCL_OK completion of the body.
     * If not, we can handle that case much more efficiently.
     */

    for (i=0 ; i<numHandlers ; i++) {
	if (handlers[i].matchCode == 0) {
	    trapZero = 1;
	    break;
	}
    }

    /*
     * Compile the body, trapping any error in it so that we can trap on it
     * (if any trap matches) and run a finally clause.
     */

    range = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, range);
    CATCH_RANGE(range) {
	BODY(			bodyToken, 1);
    }
    if (!trapZero) {
	OP(			END_CATCH);
	OP4(			STORE_SCALAR, resultLocal);
	OP(			POP);
	PUSH(			"-level 0 -code 0");
	OP4(			STORE_SCALAR, optionsLocal);
	OP(			POP);
	FWDJUMP(		JUMP, afterBody);
    } else {
	/*
	 * Fake a return code to go with our result.
	 */
	PUSH(			"0");
	OP(			SWAP);
	FWDJUMP(		JUMP, pushReturnOptions);
	STKDELTA(-2);
    }
    CATCH_TARGET(	range);
    OP(				PUSH_RETURN_CODE);
    OP(				PUSH_RESULT);
    if (pushReturnOptions) {
	FWDLABEL(	pushReturnOptions);
    }
    OP(				PUSH_RETURN_OPTIONS);
    OP(				END_CATCH);
    OP4(			STORE_SCALAR, optionsLocal);
    OP(				POP);
    OP4(			STORE_SCALAR, resultLocal);
    OP(				POP);

    /*
     * Now we handle all the registered 'on' and 'trap' handlers in order.
     */

    BACKLABEL(		tableBase);
    OP4(			JUMP_TABLE_NUM, tableIdx);
    FWDJUMP(			JUMP, haveOther);
    for (i=0 ; i<numHandlers ; i++) {
	Tcl_BytecodeLabel endCatch = 0;
	CreateJumptableNumEntryToHere(tablePtr, handlers[i].matchCode, tableBase);

	/*
	 * There is a finally clause, so we need a fairly complex sequence of
	 * instructions to deal with an on/trap handler because we must call
	 * the finally handler *and* we need to substitute the result from a
	 * failed trap for the result from the main script.
	 */

	if (handlers[i].resultVar != TCL_INDEX_NONE
		|| handlers[i].optionVar != TCL_INDEX_NONE
		|| handlers[i].tokenPtr) {
	    range = MAKE_CATCH_RANGE();
	    OP4(		BEGIN_CATCH, range);
	    ExceptionRangeStarts(envPtr, range);
	}
	if (handlers[i].resultVar != TCL_INDEX_NONE
		|| handlers[i].optionVar != TCL_INDEX_NONE) {
	    if (handlers[i].resultVar != TCL_INDEX_NONE) {
		OP4(		LOAD_SCALAR, resultLocal);
		OP4(		STORE_SCALAR, handlers[i].resultVar);
		OP(		POP);
	    }
	    if (handlers[i].optionVar != TCL_INDEX_NONE) {
		OP4(		LOAD_SCALAR, optionsLocal);
		OP4(		STORE_SCALAR, handlers[i].optionVar);
		OP(		POP);
	    }

	    if (!handlers[i].tokenPtr) {
		/*
		 * No handler. Will not be the last handler (that is a
		 * condition that is checked by the caller). Chain to the next
		 * one.
		 */

		ExceptionRangeEnds(envPtr, range);
		OP(		END_CATCH);
		forwardsNeedFixing = 1;
		endCatch = 0;
		FWDJUMP(	JUMP, forwardsToFix[i]);
		goto finishTrapCatchHandling;
	    }
	} else if (!handlers[i].tokenPtr) {
	    /*
	     * No handler. Will not be the last handler (that condition is
	     * checked by the caller). Chain to the next one.
	     */

	    forwardsNeedFixing = 1;
	    FWDJUMP(		JUMP, forwardsToFix[i]);
	    goto endOfThisArm;
	}

	/*
	 * Got a handler. Make sure that any pending patch-up actions from
	 * previous unprocessed handlers are dealt with now that we know where
	 * they are to jump to.
	 */

	if (forwardsNeedFixing) {
	    Tcl_BytecodeLabel bodyStart;
	    forwardsNeedFixing = 0;
	    FWDJUMP(		JUMP, bodyStart);
	    for (j=0 ; j<i ; j++) {
		if (forwardsToFix[j] == NO_PENDING_JUMP) {
		    continue;
		}
		FWDLABEL(forwardsToFix[j]);
		forwardsToFix[j] = NO_PENDING_JUMP;
	    }
	    OP4(		BEGIN_CATCH, range);
	    FWDLABEL(	bodyStart);
	}
	// TODO: Simplfy based on TclIsEmptyToken(handlers[i].tokenPtr)
	BODY(			handlers[i].tokenPtr, 5 + i*4);
	ExceptionRangeEnds(envPtr, range);
	OP(			PUSH_RETURN_OPTIONS);
	OP(			END_CATCH);
	FWDJUMP(		JUMP, endCatch);
	STKDELTA(-2);

	/*
	 * Error in handler or setting of variables; replace the stored
	 * exception with the new one. Note that we only push this if we have
	 * either a body or some variable setting here. Otherwise this code is
	 * unreachable.
	 */

    finishTrapCatchHandling:
	CATCH_TARGET(	range);
	OP(			PUSH_RESULT);
	OP(			PUSH_RETURN_OPTIONS);
	OP(			PUSH_RETURN_CODE);
	OP(			END_CATCH);

	PUSH(			"1");
	OP(			EQ);
	FWDJUMP(		JUMP_FALSE, dontSpliceDuring);
	// Next bit isn't DICT_SET; alter which dict is in optionsLocal
	PUSH(			"-during");
	OP4(			LOAD_SCALAR, optionsLocal);
	OP(			DICT_PUT);
	FWDLABEL(	dontSpliceDuring);

	if (endCatch) {
	    FWDLABEL(	endCatch);
	}
	OP4(			STORE_SCALAR, optionsLocal);
	OP(			POP);
	OP4(			STORE_SCALAR, resultLocal);
	OP(			POP);

    endOfThisArm:
	if (i+1 < numHandlers) {
	    FWDJUMP(		JUMP, addrsToFix[i]);
	}
    }

    /*
     * Fix all the jumps from taken clauses and the jump from after the jump
     * table to point to the start of the finally processing.
     */

    for (i=0 ; i<numHandlers-1 ; i++) {
	FWDLABEL(	addrsToFix[i]);
    }
    TclStackFree(interp, addrsToFix);
    FWDLABEL(		haveOther);
    if (!trapZero) {
	FWDLABEL(	afterBody);
    }

    /*
     * Process the finally clause (at last!) Note that we do not wrap this in
     * error handlers because we would just rethrow immediately anyway. Then
     * (on normal success) we reissue the exception. Note also that
     * INST_RETURN_STK can proceed to the next instruction; that'll be the
     * next command (or some inter-command manipulation).
     */

    range = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, range);
    CATCH_RANGE(range) {
	BODY(			finallyToken, 3 + 4*numHandlers);
    }
    OP(				END_CATCH);
    OP(				POP);
    FWDJUMP(			JUMP, finalOK);

    CATCH_TARGET(	range);
    OP(				PUSH_RESULT);
    OP(				PUSH_RETURN_OPTIONS);
    OP(				PUSH_RETURN_CODE);
    OP(				END_CATCH);

    PUSH(			"1");
    OP(				EQ);
    FWDJUMP(			JUMP_FALSE, dontSpliceDuring);
    // Next bit isn't DICT_SET; alter which dict is in optionsLocal
    PUSH(			"-during");
    OP4(			LOAD_SCALAR, optionsLocal);
    OP(				DICT_PUT);
    FWDLABEL(		dontSpliceDuring);

    OP4(			STORE_SCALAR, optionsLocal);
    OP(				POP);

    OP4(			STORE_SCALAR, resultLocal);
    OP(				POP);

    FWDLABEL(		finalOK);
    OP4(			LOAD_SCALAR, optionsLocal);
    OP4(			LOAD_SCALAR, resultLocal);
    INVOKE(			RETURN_STK);

    return TCL_OK;
}

static int
IssueTryFinallyInstructions(
    Tcl_Interp *interp,
    CompileEnv *envPtr,
    Tcl_Token *bodyToken,
    Tcl_Token *finallyToken)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_ExceptionRange bodyRange, finallyRange;
    Tcl_BytecodeLabel jumpOK, dontSpliceDuring, endCatch;

    /*
     * Note that this one is simple enough that we can issue it without
     * needing a local variable table, making it a universal compilation.
     */

    bodyRange = MAKE_CATCH_RANGE();

    // Body
    OP4(			BEGIN_CATCH, bodyRange);
    CATCH_RANGE(bodyRange) {
	BODY(			bodyToken, 1);
    }
    FWDJUMP(			JUMP, endCatch);
    STKDELTA(-1);

    CATCH_TARGET(	bodyRange);
    OP(				PUSH_RESULT);
    FWDLABEL(		endCatch);
    // Cannot avoid this next op: test-case error-15.9.0.0.2
    OP(				PUSH_RETURN_OPTIONS);
    OP(				END_CATCH);

    // Finally
    finallyRange = MAKE_CATCH_RANGE();
    OP4(			BEGIN_CATCH, finallyRange);
    CATCH_RANGE(finallyRange) {
	BODY(			finallyToken, 3);
    }
    OP(				END_CATCH);
    OP(				POP);
    OP(				SWAP);
    FWDJUMP(			JUMP, jumpOK);

    CATCH_TARGET(	finallyRange);
    OP(				PUSH_RESULT);
    OP(				PUSH_RETURN_OPTIONS);
    OP(				PUSH_RETURN_CODE);
    OP(				END_CATCH);

    // Don't forget original error
    PUSH(			"1");
    OP(				EQ);
    FWDJUMP(			JUMP_FALSE, dontSpliceDuring);
    PUSH(			"-during");
    OP4(			OVER, 3);
    OP(				DICT_PUT);
    FWDLABEL(		dontSpliceDuring);

    OP4(			REVERSE, 4);
    OP(				POP);
    OP(				POP);

    // Re-raise
    FWDLABEL(		jumpOK);
    // Cannot avoid this next op: test-case error-15.9.0.0.2
    INVOKE(			RETURN_STK);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileUnsetCmd --
 *
 *	Procedure called to compile the "unset" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "unset" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileUnsetCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *varTokenPtr;
    bool isScalar;
    int flags = 1;
    Tcl_Size haveFlags = 0, i, varCount = 0;
    Tcl_LVTIndex localIndex;

    /* TODO: Consider support for compiling expanded args. */

    if (parsePtr->numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * Verify that all words - except the first non-option one - are known at
     * compile time so that we can handle them without needing to do a nasty
     * push/rotate. [Bug 3970f54c4e]
     */

    for (i=1,varTokenPtr=parsePtr->tokenPtr ; i<parsePtr->numWords ; i++) {
	Tcl_Obj *leadingWord;

	TclNewObj(leadingWord);
	varTokenPtr = TokenAfter(varTokenPtr);
	if (!TclWordKnownAtCompileTime(varTokenPtr, leadingWord)) {
	    TclDecrRefCount(leadingWord);

	    /*
	     * We can tolerate non-trivial substitutions in the first variable
	     * to be unset. If a '--' or '-nocomplain' was present, anything
	     * goes in that one place! (All subsequent variable names must be
	     * constants since we don't want to have to push them all first.)
	     */

	    if (varCount == 0) {
		if (haveFlags) {
		    continue;
		}

		/*
		 * In fact, we're OK as long as we're the first argument *and*
		 * we provably don't start with a '-'. If that is true, then
		 * even if everything else is varying, we still can't be a
		 * flag. Otherwise we'll spill to runtime to place a limit on
		 * the trickiness.
		 */

		if (varTokenPtr->type == TCL_TOKEN_WORD
			&& varTokenPtr[1].type == TCL_TOKEN_TEXT
			&& varTokenPtr[1].size > 0
			&& varTokenPtr[1].start[0] != '-') {
		    continue;
		}
	    }
	    return TCL_ERROR;
	}
	if (varCount == 0) {
	    const char *bytes;
	    Tcl_Size len;

	    bytes = TclGetStringFromObj(leadingWord, &len);
	    if (i == 1 && len == 11 && !strncmp("-nocomplain", bytes, 11)) {
		flags = 0;
		haveFlags++;
	    } else if (i == (2 - flags) && len == 2 && !strncmp("--", bytes, 2)) {
		haveFlags++;
	    } else {
		varCount++;
	    }
	} else {
	    varCount++;
	}
	TclDecrRefCount(leadingWord);
    }

    /*
     * Issue instructions to unset each of the named variables.
     */

    varTokenPtr = TokenAfter(parsePtr->tokenPtr);
    for (i=0; i<haveFlags;i++) {
	varTokenPtr = TokenAfter(varTokenPtr);
    }
    for (i=1+haveFlags ; i<parsePtr->numWords ; i++) {
	/*
	 * Decide if we can use a frame slot for the var/array name or if we
	 * need to emit code to compute and push the name at runtime. We use a
	 * frame slot (entry in the array of local vars) if we are compiling a
	 * procedure body and if the name is simple text that does not include
	 * namespace qualifiers.
	 */

	PushVarNameWord(varTokenPtr, 0, &localIndex, &isScalar, i);

	/*
	 * Emit instructions to unset the variable.
	 */

	if (isScalar) {
	    if (localIndex < 0) {
		OP1(		UNSET_STK, flags);
	    } else {
		OP14(		UNSET_SCALAR, flags, localIndex);
	    }
	} else {
	    if (localIndex < 0) {
		OP1(		UNSET_ARRAY_STK, flags);
	    } else {
		OP14(		UNSET_ARRAY, flags, localIndex);
	    }
	}

	varTokenPtr = TokenAfter(varTokenPtr);
    }
    PUSH(			"");
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileWhileCmd --
 *
 *	Procedure called to compile the "while" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "while" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileWhileCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *testTokenPtr, *bodyTokenPtr;
    Tcl_BytecodeLabel jumpEvalCond, bodyCodeOffset;
    Tcl_ExceptionRange range;
    int code, boolVal;
    bool loopMayEnd = true;	/* This is set to 0 if it is recognized as an
				 * infinite loop. */
    Tcl_Obj *boolObj;

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    /*
     * If the test expression requires substitutions, don't compile the while
     * command inline. E.g., the expression might cause the loop to never
     * execute or execute forever, as in "while "$x < 5" {}".
     *
     * Bail out also if the body expression requires substitutions in order to
     * insure correct behaviour [Bug 219166]
     */

    testTokenPtr = TokenAfter(parsePtr->tokenPtr);
    bodyTokenPtr = TokenAfter(testTokenPtr);

    if (bodyTokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }
    TclNewObj(boolObj);
    if (!TclWordKnownAtCompileTime(testTokenPtr, boolObj)) {
	Tcl_BounceRefCount(boolObj);
	return TCL_ERROR;
    }

    /*
     * Find out if the condition is a constant.
     */

    code = Tcl_GetBooleanFromObj(NULL, boolObj, &boolVal);
    Tcl_BounceRefCount(boolObj);
    if (code == TCL_OK) {
	if (boolVal) {
	    /*
	     * It is an infinite loop; flag it so that we generate a more
	     * efficient body.
	     */

	    loopMayEnd = false;
	} else {
	    /*
	     * This is an empty loop: "while 0 {...}" or such. Compile no
	     * bytecodes.
	     */

	    goto pushResult;
	}
    }

    /*
     * Create a ExceptionRange record for the loop body. This is used to
     * implement break and continue.
     */

    range = MAKE_LOOP_RANGE();

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
	FWDJUMP(		JUMP, jumpEvalCond);
    } else {
	/*
	 * Make sure that the first command in the body is preceded by an
	 * INST_START_CMD, and hence counted properly. [Bug 1752146]
	 */

	envPtr->atCmdStart &= ~1;
	CONTINUE_TARGET(range);
    }

    /*
     * Compile the loop body.
     */

    BACKLABEL(		bodyCodeOffset);
    CATCH_RANGE(range) {
	BODY(			bodyTokenPtr, 2);
    }
    OP(				POP);

    /*
     * Compile the test expression then emit the conditional jump that
     * terminates the while. We already know it's a simple word.
     */

    if (loopMayEnd) {
	FWDLABEL(	jumpEvalCond);
	CONTINUE_TARGET(range);
	PUSH_EXPR_TOKEN(	testTokenPtr, 1);
	BACKJUMP(		JUMP_TRUE, bodyCodeOffset);
    } else {
	BACKJUMP(		JUMP, bodyCodeOffset);
    }

    /*
     * Set the loop's break offset.
     */

    BREAK_TARGET(	range);
    FINALIZE_LOOP(range);

    /*
     * The while command's result is an empty string.
     */

  pushResult:
    PUSH(			"");
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileYieldCmd --
 *
 *	Procedure called to compile the "yield" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "yield" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileYieldCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    if (parsePtr->numWords < 1 || parsePtr->numWords > 2) {
	return TCL_ERROR;
    }

    if (parsePtr->numWords == 1) {
	PUSH(			"");
    } else {
	DefineLineInformation;	/* TIP #280 */
	Tcl_Token *valueTokenPtr = TokenAfter(parsePtr->tokenPtr);

	PUSH_TOKEN(		valueTokenPtr, 1);
    }
    INVOKE(			YIELD);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileYieldToCmd --
 *
 *	Procedure called to compile the "yieldto" command.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the "yieldto" command at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileYieldToCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);
    Tcl_Size i;

    /* TODO: Consider support for compiling expanded args. */
    if (parsePtr->numWords < 2 || parsePtr->numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    OP(				NS_CURRENT);
    for (i = 1 ; i < parsePtr->numWords ; i++) {
	PUSH_TOKEN(		tokenPtr, i);
	tokenPtr = TokenAfter(tokenPtr);
    }
    OP4(			LIST, i);
    INVOKE(			YIELD_TO_INVOKE);
    return TCL_OK;
}

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
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }
    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    PUSH_TOKEN(			tokenPtr, 1);
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
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    Tcl_Size words;

    /* TODO: Consider support for compiling expanded args. */
    if (parsePtr->numWords > UINT_MAX) {
	return TCL_ERROR;
    }
    for (words=1 ; words<parsePtr->numWords ; words++) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, words);
    }
    if (parsePtr->numWords <= 2) {
	PUSH_STRING(		identity);
	words++;
    }
    if (words > 3) {
	/*
	 * Reverse order of arguments to get precise agreement with [expr] in
	 * calculations, including roundoff errors.
	 */

	OP4(			REVERSE, words - 1);
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
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr;

    /* TODO: Consider support for compiling expanded args. */
    if (parsePtr->numWords > UINT_MAX) {
	return TCL_ERROR;
    }
    if (parsePtr->numWords < 3) {
	PUSH(			"1");
    } else if (parsePtr->numWords == 3) {
	tokenPtr = TokenAfter(parsePtr->tokenPtr);
	PUSH_TOKEN(		tokenPtr, 1);
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, 2);
	TclEmitOpcode(instruction, envPtr);
    } else if (!EnvHasLVT(envPtr)) {
	/*
	 * No local variable space!
	 */

	return TCL_ERROR;
    } else {
	Tcl_LVTIndex tmpIndex = AnonymousLocal(envPtr);
	Tcl_Size words;

	tokenPtr = TokenAfter(parsePtr->tokenPtr);
	PUSH_TOKEN(		tokenPtr, 1);
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, 2);
	OP4(			STORE_SCALAR, tmpIndex);
	TclEmitOpcode(instruction, envPtr);
	for (words=3 ; words<parsePtr->numWords ;) {
	    OP4(		LOAD_SCALAR, tmpIndex);
	    tokenPtr = TokenAfter(tokenPtr);
	    PUSH_TOKEN(		tokenPtr, words);
	    if (++words < parsePtr->numWords) {
		OP4(		STORE_SCALAR, tmpIndex);
	    }
	    TclEmitOpcode(instruction, envPtr);
	}
	for (; words>3 ; words--) {
	    OP(			BITAND);
	}

	/*
	 * Drop the value from the temp variable; retaining that reference
	 * might be expensive elsewhere.
	 */

	OP14(			UNSET_SCALAR, 0, tmpIndex);
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
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileUnaryOpCmd(interp, parsePtr, INST_BITNOT, envPtr);
}

int
TclCompileNotOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileUnaryOpCmd(interp, parsePtr, INST_LNOT, envPtr);
}

int
TclCompileAddOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileAssociativeBinaryOpCmd(interp, parsePtr, "0", INST_ADD,
	    envPtr);
}

int
TclCompileMulOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileAssociativeBinaryOpCmd(interp, parsePtr, "1", INST_MULT,
	    envPtr);
}

int
TclCompileAndOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileAssociativeBinaryOpCmd(interp, parsePtr, "-1", INST_BITAND,
	    envPtr);
}

int
TclCompileOrOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileAssociativeBinaryOpCmd(interp, parsePtr, "0", INST_BITOR,
	    envPtr);
}

int
TclCompileXorOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileAssociativeBinaryOpCmd(interp, parsePtr, "0", INST_BITXOR,
	    envPtr);
}

int
TclCompilePowOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    Tcl_Size words;

    if (parsePtr->numWords > UINT_MAX) {
	return TCL_ERROR;
    }

    /*
     * This one has its own implementation because the ** operator is the only
     * one with right associativity.
     */

    for (words=1 ; words<parsePtr->numWords ; words++) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, words);
    }
    if (parsePtr->numWords <= 2) {
	PUSH(			"1");
	words++;
    }
    while (--words > 1) {
	OP(			EXPON);
    }
    return TCL_OK;
}

int
TclCompileLshiftOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_LSHIFT, envPtr);
}

int
TclCompileRshiftOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_RSHIFT, envPtr);
}

int
TclCompileModOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_MOD, envPtr);
}

int
TclCompileNeqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_NEQ, envPtr);
}

int
TclCompileStrneqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_STR_NEQ, envPtr);
}

int
TclCompileInOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_LIST_IN, envPtr);
}

int
TclCompileNiOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileStrictlyBinaryOpCmd(interp, parsePtr, INST_LIST_NOT_IN,
	    envPtr);
}

int
TclCompileLessOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_LT, envPtr);
}

int
TclCompileLeqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_LE, envPtr);
}

int
TclCompileGreaterOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_GT, envPtr);
}

int
TclCompileGeqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_GE, envPtr);
}

int
TclCompileEqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_EQ, envPtr);
}

int
TclCompileStreqOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_STR_EQ, envPtr);
}

int
TclCompileStrLtOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_STR_LT, envPtr);
}

int
TclCompileStrLeOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_STR_LE, envPtr);
}

int
TclCompileStrGtOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_STR_GT, envPtr);
}

int
TclCompileStrGeOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    return CompileComparisonOpCmd(interp, parsePtr, INST_STR_GE, envPtr);
}

int
TclCompileMinusOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    Tcl_Size words;

    /* TODO: Consider support for compiling expanded args. */
    if (parsePtr->numWords == 1 || parsePtr->numWords > UINT_MAX) {
	/*
	 * Fallback to direct eval to report syntax error.
	 */

	return TCL_ERROR;
    }
    for (words=1 ; words<parsePtr->numWords ; words++) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, words);
    }
    if (words == 2) {
	OP(			UMINUS);
	return TCL_OK;
    }
    if (words == 3) {
	OP(			SUB);
	return TCL_OK;
    }

    /*
     * Reverse order of arguments to get precise agreement with [expr] in
     * calculations, including roundoff errors.
     */

    OP4(			REVERSE, words - 1);
    while (--words > 1) {
	OP(			SWAP);
	OP(			SUB);
    }
    return TCL_OK;
}

int
TclCompileDivOpCmd(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)
{
    DefineLineInformation;	/* TIP #280 */
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    Tcl_Size words;

    /* TODO: Consider support for compiling expanded args. */
    if (parsePtr->numWords == 1 || parsePtr->numWords > UINT_MAX) {
	/*
	 * Fallback to direct eval to report syntax error.
	 */

	return TCL_ERROR;
    }
    if (parsePtr->numWords == 2) {
	PUSH(			"1.0");
    }
    for (words=1 ; words<parsePtr->numWords ; words++) {
	tokenPtr = TokenAfter(tokenPtr);
	PUSH_TOKEN(		tokenPtr, words);
    }
    if (words <= 3) {
	OP(			DIV);
	return TCL_OK;
    }

    /*
     * Reverse order of arguments to get precise agreement with [expr] in
     * calculations, including roundoff errors.
     */

    OP4(			REVERSE, words - 1);
    while (--words > 1) {
	OP(			SWAP);
	OP(			DIV);
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
