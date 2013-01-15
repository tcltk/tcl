/*
 * tclCompile.c --
 *
 *	This file contains procedures that compile Tcl commands or parts of
 *	commands (like quoted strings or nested sub-commands) into a sequence
 *	of instructions ("bytecodes").
 *
 * Copyright (c) 1996-1998 Sun Microsystems, Inc.
 * Copyright (c) 2001 by Kevin B. Kenny. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclCompileInt.h"

/*
 * Table of all AuxData types.
 */

static Tcl_HashTable auxDataTypeTable;
static int auxDataTypeTableInitialized; /* 0 means not yet initialized. */

TCL_DECLARE_MUTEX(tableMutex)

/*
 * Variable that controls whether compilation tracing is enabled and, if so,
 * what level of tracing is desired:
 *    0: no compilation tracing
 *    1: summarize compilation of top level cmds and proc bodies
 *    2: display all instructions of each ByteCode compiled
 * This variable is linked to the Tcl variable "tcl_traceCompile".
 */


/*
 * A table describing the Tcl bytecode instructions. Entries in this table
 * must correspond to the instruction opcode definitions in tclCompile.h. The
 * names "op1" and "op4" refer to an instruction's one or four byte first
 * operand. Similarly, "stktop" and "stknext" refer to the topmost and next to
 * topmost stack elements.
 *
 * Note that the load, store, and incr instructions do not distinguish local
 * from global variables; the bytecode interpreter at runtime uses the
 * existence of a procedure call frame to distinguish these.
 */

InstructionDesc const tclInstructionTable[] = {
    /* Name	      Bytes stackEffect #Opnds  Operand types */
    {"done",		  1,   -1,         0,	{OPERAND_NONE}},//0
    {"push4",		  5,   +1,         1,	{OPERAND_UINT4}},//1
    {"pop",		  1,   -1,         0,	{OPERAND_NONE}},//2
    {"concat1",		  2,   INT_MIN,    1,	{OPERAND_UINT1}},//3
    {"invokeStk4",	  5,   INT_MIN,    1,	{OPERAND_UINT4}},//4
    {"loadScalar4",	  5,   1,          1,	{OPERAND_LVT4}},//5
    {"loadScalarStk",	  1,   0,          0,	{OPERAND_NONE}},//6
    {"loadArray4",	  5,   0,          1,	{OPERAND_LVT4}},//7
    {"loadArrayStk",	  1,   -1,         0,	{OPERAND_NONE}},//8
    {"storeScalar4",	  5,   0,          1,	{OPERAND_LVT4}},//9    
    {"jump4",		  5,   0,          1,	{OPERAND_INT4}},//10
    {"jumpTrue4",	  5,   -1,         1,	{OPERAND_INT4}},//11
    {"jumpFalse4",	  5,   -1,         1,	{OPERAND_INT4}},//12
    {"bitor",		  1,   -1,         0,	{OPERAND_NONE}},//13
    {"bitxor",		  1,   -1,         0,	{OPERAND_NONE}},//14
    {"bitand",		  1,   -1,         0,	{OPERAND_NONE}},//15
    {"eq",		  1,   -1,         0,	{OPERAND_NONE}},//16
    {"neq",		  1,   -1,         0,	{OPERAND_NONE}},//17
    {"lt",		  1,   -1,         0,	{OPERAND_NONE}},//18
    {"gt",		  1,   -1,         0,	{OPERAND_NONE}},//19
    {"le",		  1,   -1,         0,	{OPERAND_NONE}},//20
    {"ge",		  1,   -1,         0,	{OPERAND_NONE}},//21
    {"lshift",		  1,   -1,         0,	{OPERAND_NONE}},//22
    {"rshift",		  1,   -1,         0,	{OPERAND_NONE}},//23
    {"add",		  1,   -1,         0,	{OPERAND_NONE}},//24
    {"sub",		  1,   -1,         0,	{OPERAND_NONE}},//25
    {"mult",		  1,   -1,         0,	{OPERAND_NONE}},//26
    {"div",		  1,   -1,         0,	{OPERAND_NONE}},//27
    {"mod",		  1,   -1,         0,	{OPERAND_NONE}},//28
    {"uplus",		  1,   0,          0,	{OPERAND_NONE}},//29
    {"uminus",		  1,   0,          0,	{OPERAND_NONE}},//30
    {"bitnot",		  1,   0,          0,	{OPERAND_NONE}},//31
    {"not",		  1,   0,          0,	{OPERAND_NONE}},//32
    {"tryCvtToNumeric",	  1,   0,          0,	{OPERAND_NONE}},//33
    {"streq",		  1,   -1,         0,	{OPERAND_NONE}},//34
    {"strneq",		  1,   -1,         0,	{OPERAND_NONE}},//35
    {"expon",		  1,   -1,	   0,	{OPERAND_NONE}},//36
    {"expandStart",       1,    0,          0,	{OPERAND_NONE}},//37
    {"expandStkTop",      5,    0,          1,	{OPERAND_UINT4}},//38
    {"invokeExpanded",    1,    0,          0,	{OPERAND_NONE}},//39
    {"listIn",		  1,	-1,	   0,	{OPERAND_NONE}},//40
    {"listNotIn",	  1,	-1,	   0,	{OPERAND_NONE}},//41
    {"syntax",		 9,   -1,         2,	{OPERAND_INT4, OPERAND_UINT4}},//42
    {"reverse",		 5,    0,         1,	{OPERAND_UINT4}},//43
    {"unsetScalar",	 6,    0,         2,	{OPERAND_UINT1, OPERAND_LVT4}},//44
    {"currentNamespace", 1,    +1,	  0,	{OPERAND_NONE}},//45
    {"tclooSelf",	 1,	+1,	  0,	{OPERAND_NONE}},//46
    {NULL, 0, 0, 0, {OPERAND_NONE}}
};

/*
 * Prototypes for procedures defined later in this file:
 */

static void		DupByteCodeInternalRep(Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr);
static unsigned char *	EncodeCmdLocMap(CompileEnv *envPtr,
			    ByteCode *codePtr, unsigned char *startPtr);
static void		EnterCmdExtentData(CompileEnv *envPtr,
			    int cmdNumber, int numSrcBytes, int numCodeBytes);
static void		EnterCmdStartData(CompileEnv *envPtr,
			    int cmdNumber, int srcOffset, int codeOffset);
static void		FreeByteCodeInternalRep(Tcl_Obj *objPtr);
static int		GetCmdLocEncodingSize(CompileEnv *envPtr);
static int		SetByteCodeFromAny(Tcl_Interp *interp,
			    Tcl_Obj *objPtr);

/*
 * The structure below defines the bytecode Tcl object type by means of
 * procedures that can be invoked by generic object code.
 */

const Tcl_ObjType tclByteCodeType = {
    "bytecode",			/* name */
    FreeByteCodeInternalRep,	/* freeIntRepProc */
    DupByteCodeInternalRep,	/* dupIntRepProc */
    NULL,			/* updateStringProc */
    SetByteCodeFromAny		/* setFromAnyProc */
};


/*
 *----------------------------------------------------------------------
 *
 * TclSetByteCodeFromAny --
 *
 *	Part of the bytecode Tcl object type implementation. Attempts to
 *	generate an byte code internal form for the Tcl object "objPtr" by
 *	compiling its string representation. This function also takes a hook
 *	procedure that will be invoked to perform any needed post processing
 *	on the compilation results before generating byte codes. interp is
 *	compilation context and may not be NULL.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during compilation, an error message is left in the interpreter's
 *	result.
 *
 * Side effects:
 *	Frees the old internal representation. If no error occurs, then the
 *	compiled code is stored as "objPtr"s bytecode representation. Also, if
 *	debugging, initializes the "tcl_traceCompile" Tcl variable used to
 *	trace compilations.
 *
 *----------------------------------------------------------------------
 */

int
TclSetByteCodeFromAny(
    Tcl_Interp *interp,		/* The interpreter for which the code is being
				 * compiled. Must not be NULL. */
    Tcl_Obj *objPtr,		/* The object to make a ByteCode object. */
    CompileHookProc *hookProc,	/* Procedure to invoke after compilation. */
    ClientData clientData)	/* Hook procedure private data. */
{
    CompileEnv compEnv;		/* Compilation environment structure allocated
				 * in frame. */
    register const AuxData *auxDataPtr;
    LiteralEntry *entryPtr;
    register int i;
    int length, result = TCL_OK;
    const char *stringPtr;

    stringPtr = TclGetStringFromObj(objPtr, &length);

    TclInitCompileEnv(interp, &compEnv, stringPtr, length);

    /*
     * Now we check if we have data about invisible continuation lines for the
     * script, and make it available to the compile environment, if so.
     *
     * It is not clear if the script Tcl_Obj* can be free'd while the compiler
     * is using it, leading to the release of the associated ContLineLoc
     * structure as well. To ensure that the latter doesn't happen we set a
     * lock on it. We release this lock in the function TclFreeCompileEnv(),
     * found in this file. The "lineCLPtr" hashtable is managed in the file
     * "tclObj.c".
     */

    TclCompileScript(interp, stringPtr, length, &compEnv);

    /*
     * Successful compilation. Add a "done" instruction at the end.
     */

    TclEmitOpcode(INST_DONE, &compEnv);

    /*
     * Invoke the compilation hook procedure if one exists.
     */

    if (hookProc) {
	result = hookProc(interp, &compEnv, clientData);
    }

    /*
     * Change the object into a ByteCode object. Ownership of the literal
     * objects and aux data items is given to the ByteCode object.
     */

    TclInitByteCodeObj(objPtr, &compEnv);
    if (result != TCL_OK) {
	/*
	 * Handle any error from the hookProc
	 */

	entryPtr = compEnv.literalArrayPtr;
	for (i = 0;  i < compEnv.literalArrayNext;  i++) {
	    TclReleaseLiteral(interp, entryPtr->objPtr);
	    entryPtr++;
	}

	auxDataPtr = compEnv.auxDataArrayPtr;
	for (i = 0;  i < compEnv.auxDataArrayNext;  i++) {
	    if (auxDataPtr->type->freeProc != NULL) {
		auxDataPtr->type->freeProc(auxDataPtr->clientData);
	    }
	    auxDataPtr++;
	}
    }

    TclFreeCompileEnv(&compEnv);
    return result;
}

/*
 *-----------------------------------------------------------------------
 *
 * SetByteCodeFromAny --
 *
 *	Part of the bytecode Tcl object type implementation. Attempts to
 *	generate an byte code internal form for the Tcl object "objPtr" by
 *	compiling its string representation.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during compilation, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	Frees the old internal representation. If no error occurs, then the
 *	compiled code is stored as "objPtr"s bytecode representation. Also, if
 *	debugging, initializes the "tcl_traceCompile" Tcl variable used to
 *	trace compilations.
 *
 *----------------------------------------------------------------------
 */

static int
SetByteCodeFromAny(
    Tcl_Interp *interp,		/* The interpreter for which the code is being
				 * compiled. Must not be NULL. */
    Tcl_Obj *objPtr)		/* The object to make a ByteCode object. */
{
    if (interp == NULL) {
	return TCL_ERROR;
    }
    TclSetByteCodeFromAny(interp, objPtr, NULL, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DupByteCodeInternalRep --
 *
 *	Part of the bytecode Tcl object type implementation. However, it does
 *	not copy the internal representation of a bytecode Tcl_Obj, but
 *	instead leaves the new object untyped (with a NULL type pointer).
 *	Code will be compiled for the new object only if necessary.
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
DupByteCodeInternalRep(
    Tcl_Obj *srcPtr,		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr)		/* Object with internal rep to set. */
{
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeByteCodeInternalRep --
 *
 *	Part of the bytecode Tcl object type implementation. Frees the storage
 *	associated with a bytecode object's internal representation unless its
 *	code is actively being executed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The bytecode object's internal rep is marked invalid and its code gets
 *	freed unless the code is actively being executed. In that case the
 *	cleanup is delayed until the last execution of the code completes.
 *
 *----------------------------------------------------------------------
 */

static void
FreeByteCodeInternalRep(
    register Tcl_Obj *objPtr)	/* Object whose internal rep to free. */
{
    register ByteCode *codePtr = objPtr->internalRep.otherValuePtr;

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
 * TclCleanupByteCode --
 *
 *	This procedure does all the real work of freeing up a bytecode
 *	object's ByteCode structure. It's called only when the structure's
 *	reference count becomes zero.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees objPtr's bytecode internal representation and sets its type and
 *	objPtr->internalRep.otherValuePtr NULL. Also releases its literals and
 *	frees its auxiliary data items.
 *
 *----------------------------------------------------------------------
 */

void
TclCleanupByteCode(
    register ByteCode *codePtr)	/* Points to the ByteCode to free. */
{
    Tcl_Interp *interp = (Tcl_Interp *) *codePtr->interpHandle;
    int numLitObjects = codePtr->numLitObjects;
    int numAuxDataItems = codePtr->numAuxDataItems;
    register Tcl_Obj **objArrayPtr, *objPtr;
    register const AuxData *auxDataPtr;
    int i;
    /*
     * A single heap object holds the ByteCode structure and its code, object,
     * command location, and auxiliary data arrays. This means we only need to
     * 1) decrement the ref counts of the LiteralEntry's in its literal array,
     * 2) call the free procs for the auxiliary data items, 3) free the
     * localCache if it is unused, and finally 4) free the ByteCode
     * structure's heap object.
     *
     * The case for TCL_BYTECODE_PRECOMPILED (precompiled ByteCodes, like
     * those generated from tbcload) is special, as they doesn't make use of
     * the global literal table. They instead maintain private references to
     * their literals which must be decremented.
     *
     * In order to insure a proper and efficient cleanup of the literal array
     * when it contains non-shared literals [Bug 983660], we also distinguish
     * the case of an interpreter being deleted (signaled by interp == NULL).
     * Also, as the interp deletion will remove the global literal table
     * anyway, we avoid the extra cost of updating it for each literal being
     * released.
     */

    if ((codePtr->flags & TCL_BYTECODE_PRECOMPILED) || (interp == NULL)) {

	objArrayPtr = codePtr->objArrayPtr;
	for (i = 0;  i < numLitObjects;  i++) {
	    objPtr = *objArrayPtr;
	    if (objPtr) {
		Tcl_DecrRefCount(objPtr);
	    }
	    objArrayPtr++;
	}
	codePtr->numLitObjects = 0;
    } else {
	objArrayPtr = codePtr->objArrayPtr;
	for (i = 0;  i < numLitObjects;  i++) {
	    /*
	     * TclReleaseLiteral sets a ByteCode's object array entry NULL to
	     * indicate that it has already freed the literal.
	     */

	    objPtr = *objArrayPtr;
	    if (objPtr != NULL) {
		TclReleaseLiteral(interp, objPtr);
	    }
	    objArrayPtr++;
	}
    }

    auxDataPtr = codePtr->auxDataArrayPtr;
    for (i = 0;  i < numAuxDataItems;  i++) {
	if (auxDataPtr->type->freeProc != NULL) {
	    auxDataPtr->type->freeProc(auxDataPtr->clientData);
	}
	auxDataPtr++;
    }

    if (codePtr->localCachePtr && (--codePtr->localCachePtr->refCount == 0)) {
	TclFreeLocalCache(interp, codePtr->localCachePtr);
    }

    TclHandleRelease(codePtr->interpHandle);
    ckfree(codePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitCompileEnv --
 *
 *	Initializes a CompileEnv compilation environment structure for the
 *	compilation of a string in an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The CompileEnv structure is initialized.
 *
 *----------------------------------------------------------------------
 */

void
TclInitCompileEnv(
    Tcl_Interp *interp,		/* The interpreter for which a CompileEnv
				 * structure is initialized. */
    register CompileEnv *envPtr,/* Points to the CompileEnv structure to
				 * initialize. */
    const char *stringPtr,	/* The source string to be compiled. */
    int numBytes)		/* Number of bytes in source string. */
{
    Interp *iPtr = (Interp *) interp;

    envPtr->iPtr = iPtr;
    envPtr->source = stringPtr;
    envPtr->numSrcBytes = numBytes;
    envPtr->procPtr = iPtr->compiledProcPtr;
    iPtr->compiledProcPtr = NULL;
    envPtr->numCommands = 0;
    envPtr->exceptDepth = 0;
    envPtr->maxExceptDepth = 0;
    envPtr->maxStackDepth = 0;
    envPtr->currStackDepth = 0;
    TclInitLiteralTable(&envPtr->localLitTable);

    envPtr->codeStart = envPtr->staticCodeSpace;
    envPtr->codeNext = envPtr->codeStart;
    envPtr->codeEnd = envPtr->codeStart + COMPILEENV_INIT_CODE_BYTES;
    envPtr->mallocedCodeArray = 0;

    envPtr->literalArrayPtr = envPtr->staticLiteralSpace;
    envPtr->literalArrayNext = 0;
    envPtr->literalArrayEnd = COMPILEENV_INIT_NUM_OBJECTS;
    envPtr->mallocedLiteralArray = 0;

    envPtr->exceptArrayPtr = envPtr->staticExceptArraySpace;
    envPtr->exceptArrayNext = 0;
    envPtr->exceptArrayEnd = COMPILEENV_INIT_EXCEPT_RANGES;
    envPtr->mallocedExceptArray = 0;

    envPtr->cmdMapPtr = envPtr->staticCmdMapSpace;
    envPtr->cmdMapEnd = COMPILEENV_INIT_CMD_MAP_SIZE;
    envPtr->mallocedCmdMap = 0;
    envPtr->atCmdStart = 1;


    envPtr->auxDataArrayPtr = envPtr->staticAuxDataArraySpace;
    envPtr->auxDataArrayNext = 0;
    envPtr->auxDataArrayEnd = COMPILEENV_INIT_AUX_DATA_SIZE;
    envPtr->mallocedAuxDataArray = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFreeCompileEnv --
 *
 *	Free the storage allocated in a CompileEnv compilation environment
 *	structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocated storage in the CompileEnv structure is freed. Note that its
 *	local literal table is not deleted and its literal objects are not
 *	released. In addition, storage referenced by its auxiliary data items
 *	is not freed. This is done so that, when compilation is successful,
 *	"ownership" of these objects and aux data items is handed over to the
 *	corresponding ByteCode structure.
 *
 *----------------------------------------------------------------------
 */

void
TclFreeCompileEnv(
    register CompileEnv *envPtr)/* Points to the CompileEnv structure. */
{
    if (envPtr->localLitTable.buckets != envPtr->localLitTable.staticBuckets){
	ckfree(envPtr->localLitTable.buckets);
	envPtr->localLitTable.buckets = envPtr->localLitTable.staticBuckets;
    }
    if (envPtr->mallocedCodeArray) {
	ckfree(envPtr->codeStart);
    }
    if (envPtr->mallocedLiteralArray) {
	ckfree(envPtr->literalArrayPtr);
    }
    if (envPtr->mallocedExceptArray) {
	ckfree(envPtr->exceptArrayPtr);
    }
    if (envPtr->mallocedCmdMap) {
	ckfree(envPtr->cmdMapPtr);
    }
    if (envPtr->mallocedAuxDataArray) {
	ckfree(envPtr->auxDataArrayPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclWordKnownAtCompileTime --
 *
 *	Test whether the value of a token is completely known at compile time.
 *
 * Results:
 *	Returns true if the tokenPtr argument points to a word value that is
 *	completely known at compile time. Generally, values that are known at
 *	compile time can be compiled to their values, while values that cannot
 *	be known until substitution at runtime must be compiled to bytecode
 *	instructions that perform that substitution. For several commands,
 *	whether or not arguments are known at compile time determine whether
 *	it is worthwhile to compile at all.
 *
 * Side effects:
 *	When returning true, appends the known value of the word to the
 *	unshared Tcl_Obj (*valuePtr), unless valuePtr is NULL.
 *
 *----------------------------------------------------------------------
 */

int
TclWordKnownAtCompileTime(
    Tcl_Token *tokenPtr,	/* Points to Tcl_Token we should check */
    Tcl_Obj *valuePtr)		/* If not NULL, points to an unshared Tcl_Obj
				 * to which we should append the known value
				 * of the word. */
{
    int numComponents = tokenPtr->numComponents;
    Tcl_Obj *tempPtr = NULL;

    if (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	if (valuePtr != NULL) {
	    Tcl_AppendToObj(valuePtr, tokenPtr[1].start, tokenPtr[1].size);
	}
	return 1;
    }
    if (tokenPtr->type != TCL_TOKEN_WORD) {
	return 0;
    }
    tokenPtr++;
    if (valuePtr != NULL) {
	tempPtr = Tcl_NewObj();
	Tcl_IncrRefCount(tempPtr);
    }
    while (numComponents--) {
	switch (tokenPtr->type) {
	case TCL_TOKEN_TEXT:
	    if (tempPtr != NULL) {
		Tcl_AppendToObj(tempPtr, tokenPtr->start, tokenPtr->size);
	    }
	    break;

	case TCL_TOKEN_BS:
	    if (tempPtr != NULL) {
		char utfBuf[TCL_UTF_MAX];
		int length = TclParseBackslash(tokenPtr->start,
			tokenPtr->size, NULL, utfBuf);

		Tcl_AppendToObj(tempPtr, utfBuf, length);
	    }
	    break;

	default:
	    if (tempPtr != NULL) {
		Tcl_DecrRefCount(tempPtr);
	    }
	    return 0;
	}
	tokenPtr++;
    }
    if (valuePtr != NULL) {
	Tcl_AppendObjToObj(valuePtr, tempPtr);
	Tcl_DecrRefCount(tempPtr);
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileScript --
 *
 *	Compile a Tcl script in a string.
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the script at runtime.
 *
 *----------------------------------------------------------------------
 */

void
TclCompileScript(
    Tcl_Interp *interp,		/* Used for error and status reporting. Also
				 * serves as context for finding and compiling
				 * commands. May not be NULL. */
    const char *script,		/* The source script to compile. */
    int numBytes,		/* Number of bytes in script. If < 0, the
				 * script consists of all bytes up to the
				 * first null character. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    int lastTopLevelCmdIndex = -1;
				/* Index of most recent toplevel command in
				 * the command location table. Initialized to
				 * avoid compiler warning. */
    int startCodeOffset = -1;	/* Offset of first byte of current command's
				 * code. Init. to avoid compiler warning. */
    unsigned char *entryCodeNext = envPtr->codeNext;
    const char *p, *next;
    Namespace *cmdNsPtr;
    Command *cmdPtr;
    Tcl_Token *tokenPtr;
    int bytesLeft, isFirstCmd, wordIdx, currCmdIndex, commandLength, objIndex;
    Tcl_DString ds;
    Tcl_Parse *parsePtr = ckalloc(sizeof(Tcl_Parse));

    Tcl_DStringInit(&ds);

    if (numBytes < 0) {
	numBytes = strlen(script);
    }
    Tcl_ResetResult(interp);
    isFirstCmd = 1;

    if (envPtr->procPtr != NULL) {
	cmdNsPtr = envPtr->procPtr->cmdPtr->nsPtr;
    } else {
	cmdNsPtr = NULL;	/* use current NS */
    }

    /*
     * Each iteration through the following loop compiles the next command
     * from the script.
     */

    p = script;
    bytesLeft = numBytes;
    do {
	if (Tcl_ParseCommand(interp, p, bytesLeft, 0, parsePtr) != TCL_OK) {
	    /*
	     * Compile bytecodes to report the parse error at runtime.
	     */

	    Tcl_LogCommandInfo(interp, script, parsePtr->commandStart,
		    /* Drop the command terminator (";","]") if appropriate */
		    (parsePtr->term ==
		    parsePtr->commandStart + parsePtr->commandSize - 1)?
		    parsePtr->commandSize - 1 : parsePtr->commandSize);
	    TclCompileSyntaxError(interp, envPtr);
	    break;
	}

	if (parsePtr->numWords > 0) {
	    int expand = 0;	/* Set if there are dynamic expansions to
				 * handle */

	    /*
	     * If not the first command, pop the previous command's result
	     * and, if we're compiling a top level command, update the last
	     * command's code size to account for the pop instruction.
	     */

	    if (!isFirstCmd) {
		TclEmitOpcode(INST_POP, envPtr);
		envPtr->cmdMapPtr[lastTopLevelCmdIndex].numCodeBytes =
			(envPtr->codeNext - envPtr->codeStart)
			- startCodeOffset;
	    }

	    /*
	     * Determine the actual length of the command.
	     */

	    commandLength = parsePtr->commandSize;
	    if (parsePtr->term == parsePtr->commandStart + commandLength-1) {
		/*
		 * The command terminator character (such as ; or ]) is the
		 * last character in the parsed command. Reduce the length by
		 * one so that the trace message doesn't include the
		 * terminator character.
		 */

		commandLength -= 1;
	    }

	    /*
	     * Check whether expansion has been requested for any of the
	     * words.
	     */

	    for (wordIdx = 0, tokenPtr = parsePtr->tokenPtr;
		    wordIdx < parsePtr->numWords;
		    wordIdx++, tokenPtr += tokenPtr->numComponents + 1) {
		if (tokenPtr->type == TCL_TOKEN_EXPAND_WORD) {
		    expand = 1;
		    break;
		}
	    }

	    envPtr->numCommands++;
	    currCmdIndex = envPtr->numCommands - 1;
	    lastTopLevelCmdIndex = currCmdIndex;
	    startCodeOffset = envPtr->codeNext - envPtr->codeStart;
	    EnterCmdStartData(envPtr, currCmdIndex,
		    parsePtr->commandStart - envPtr->source, startCodeOffset);

	    /*
	     * Should only start issuing instructions after the "command has
	     * started" so that the command range is correct in the bytecode.
	     */

	    if (expand) {
		TclEmitOpcode(INST_EXPAND_START, envPtr);
	    }

	    /*
	     * Each iteration of the following loop compiles one word from the
	     * command.
	     */

	    for (wordIdx = 0, tokenPtr = parsePtr->tokenPtr;
		    wordIdx < parsePtr->numWords; wordIdx++,
		    tokenPtr += tokenPtr->numComponents + 1) {

		if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
		    /*
		     * The word is not a simple string of characters.
		     */

		    TclCompileTokens(interp, tokenPtr+1,
			    tokenPtr->numComponents, envPtr);
		    if (tokenPtr->type == TCL_TOKEN_EXPAND_WORD) {
			TclEmitInstInt4(INST_EXPAND_STKTOP,
				envPtr->currStackDepth, envPtr);
		    }
		    continue;
		}

		/*
		 * This is a simple string of literal characters (i.e. we know
		 * it absolutely and can use it directly). If this is the
		 * first word and the command has a compile procedure, let it
		 * compile the command.
		 */

		if ((wordIdx == 0) && !expand) {
		    /*
		     * We copy the string before trying to find the command by
		     * name. We used to modify the string in place, but this
		     * is not safe because the name resolution handlers could
		     * have side effects that rely on the unmodified string.
		     */

		    TclDStringClear(&ds);
		    TclDStringAppendToken(&ds, &tokenPtr[1]);

		    cmdPtr = (Command *) Tcl_FindCommand(interp,
			    Tcl_DStringValue(&ds),
			    (Tcl_Namespace *) cmdNsPtr, /*flags*/ 0);

		    /*
		     * No compile procedure so push the word. If the command
		     * was found, push a CmdName object to reduce runtime
		     * lookups. Mark this as a command name literal to reduce
		     * shimmering. 
		     */

		    objIndex = TclRegisterNewCmdLiteral(envPtr,
			    tokenPtr[1].start, tokenPtr[1].size);
		    if (cmdPtr != NULL) {
			TclSetCmdNameObj(interp,
				envPtr->literalArrayPtr[objIndex].objPtr,
				cmdPtr);
		    }
		} else {
		    /*
		     * Simple argument word of a command. We reach this if and
		     * only if the command word was not compiled for whatever
		     * reason. Register the literal's location for use by
		     * uplevel, etc. commands, should they encounter it
		     * unmodified. We care only if the we are in a context
		     * which already allows absolute counting.
		     */

		    objIndex = TclRegisterNewLiteral(envPtr,
			    tokenPtr[1].start, tokenPtr[1].size);
		}
		TclEmitPush(objIndex, envPtr);
	    } /* for loop */

	    /*
	     * Emit an invoke instruction for the command. We skip this if a
	     * compile procedure was found for the command.
	     */

	    if (expand) {
		/*
		 * The stack depth during argument expansion can only be
		 * managed at runtime, as the number of elements in the
		 * expanded lists is not known at compile time. We adjust here
		 * the stack depth estimate so that it is correct after the
		 * command with expanded arguments returns.
		 *
		 * The end effect of this command's invocation is that all the
		 * words of the command are popped from the stack, and the
		 * result is pushed: the stack top changes by (1-wordIdx).
		 *
		 * Note that the estimates are not correct while the command
		 * is being prepared and run, INST_EXPAND_STKTOP is not
		 * stack-neutral in general.
		 */

		TclEmitOpcode(INST_INVOKE_EXPANDED, envPtr);
		TclAdjustStackDepth((1-wordIdx), envPtr);
	    } else if (wordIdx > 0) {
		TclEmitInstInt4(INST_INVOKE_STK4, wordIdx, envPtr);
	    }

	    /*
	     * Update the compilation environment structure and record the
	     * offsets of the source and code for the command.
	     */

	    EnterCmdExtentData(envPtr, currCmdIndex, commandLength,
		    (envPtr->codeNext-envPtr->codeStart) - startCodeOffset);
	    isFirstCmd = 0;

	} /* end if parsePtr->numWords > 0 */

	/*
	 * Advance to the next command in the script.
	 */

	next = parsePtr->commandStart + parsePtr->commandSize;
	bytesLeft -= next - p;
	p = next;
	Tcl_FreeParse(parsePtr);
    } while (bytesLeft > 0);

    /*
     * If the source script yielded no instructions (e.g., if it was empty),
     * push an empty string as the command's result.
     */

    if (envPtr->codeNext == entryCodeNext) {
	TclEmitPush(TclRegisterNewLiteral(envPtr, "", 0), envPtr);
    }

    envPtr->numSrcBytes = p - script;
    ckfree(parsePtr);
    Tcl_DStringFree(&ds);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileTokens --
 *
 *	Given an array of tokens parsed from a Tcl command (e.g., the tokens
 *	that make up a word) this procedure emits instructions to evaluate the
 *	tokens and concatenate their values to form a single result value on
 *	the interpreter's runtime evaluation stack.
 *
 * Results:
 *	The return value is a standard Tcl result. If an error occurs, an
 *	error message is left in the interpreter's result.
 *
 * Side effects:
 *	Instructions are added to envPtr to push and evaluate the tokens at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

void
TclCompileVarSubst(
    Tcl_Interp *interp,
    Tcl_Token *tokenPtr,
    CompileEnv *envPtr)
{
    const char *p, *name = tokenPtr[1].start;
    int nameBytes = tokenPtr[1].size;
    int i, localVar, localVarName = 1;

    /*
     * Determine how the variable name should be handled: if it contains any
     * namespace qualifiers it is not a local variable (localVarName=-1); if
     * it looks like an array element and the token has a single component, it
     * should not be created here [Bug 569438] (localVarName=0); otherwise,
     * the local variable can safely be created (localVarName=1).
     */

    for (i = 0, p = name;  i < nameBytes;  i++, p++) {
	if ((*p == ':') && (i < nameBytes-1) && (*(p+1) == ':')) {
	    localVarName = -1;
	    break;
	} else if ((*p == '(')
		&& (tokenPtr->numComponents == 1)
		&& (*(name + nameBytes - 1) == ')')) {
	    localVarName = 0;
	    break;
	}
    }

    /*
     * Either push the variable's name, or find its index in the array
     * of local variables in a procedure frame.
     */

    localVar = -1;
    if (localVarName != -1) {
	localVar = TclFindCompiledLocal(name, nameBytes, localVarName, envPtr);
    }
    if (localVar < 0) {
	TclEmitPush(TclRegisterNewLiteral(envPtr, name, nameBytes), envPtr);
    }

    /*
     * Emit instructions to load the variable.
     */

    if (tokenPtr->numComponents == 1) {
	if (localVar < 0) {
	    TclEmitOpcode(INST_LOAD_SCALAR_STK, envPtr);
	} else {
	    TclEmitInstInt4(INST_LOAD_SCALAR4, localVar, envPtr);
	}
    } else {
	TclCompileTokens(interp, tokenPtr+2, tokenPtr->numComponents-1, envPtr);
	if (localVar < 0) {
	    TclEmitOpcode(INST_LOAD_ARRAY_STK, envPtr);
	} else {
	    TclEmitInstInt4(INST_LOAD_ARRAY4, localVar, envPtr);
	}
    }
}

void
TclCompileTokens(
    Tcl_Interp *interp,		/* Used for error and status reporting. */
    Tcl_Token *tokenPtr,	/* Pointer to first in an array of tokens to
				 * compile. */
    int count,			/* Number of tokens to consider at tokenPtr.
				 * Must be at least 1. */
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    Tcl_DString textBuffer;	/* Holds concatenated chars from adjacent
				 * TCL_TOKEN_TEXT, TCL_TOKEN_BS tokens. */
    char buffer[TCL_UTF_MAX];
    int numObjsToConcat, length;
    unsigned char *entryCodeNext = envPtr->codeNext;

    Tcl_DStringInit(&textBuffer);
    numObjsToConcat = 0;
    for ( ;  count > 0;  count--, tokenPtr++) {
	switch (tokenPtr->type) {
	case TCL_TOKEN_TEXT:
	    TclDStringAppendToken(&textBuffer, tokenPtr);
	    break;

	case TCL_TOKEN_BS:
	    length = TclParseBackslash(tokenPtr->start, tokenPtr->size,
		    NULL, buffer);
	    Tcl_DStringAppend(&textBuffer, buffer, length);

	    if ((length == 1) && (buffer[0] == ' ') &&
		(tokenPtr->start[1] == '\n')) {
	    }
	    break;

	case TCL_TOKEN_COMMAND:
	    /*
	     * Push any accumulated chars appearing before the command.
	     */

	    if (Tcl_DStringLength(&textBuffer) > 0) {
		int literal = TclRegisterDStringLiteral(envPtr, &textBuffer);

		TclEmitPush(literal, envPtr);
		numObjsToConcat++;
		Tcl_DStringFree(&textBuffer);
	    }

	    TclCompileScript(interp, tokenPtr->start+1,
		    tokenPtr->size-2, envPtr);
	    numObjsToConcat++;
	    break;

	case TCL_TOKEN_VARIABLE:
	    /*
	     * Push any accumulated chars appearing before the $<var>.
	     */

	    if (Tcl_DStringLength(&textBuffer) > 0) {
		int literal;

		literal = TclRegisterDStringLiteral(envPtr, &textBuffer);
		TclEmitPush(literal, envPtr);
		numObjsToConcat++;
		Tcl_DStringFree(&textBuffer);
	    }

	    TclCompileVarSubst(interp, tokenPtr, envPtr);
	    numObjsToConcat++;
	    count -= tokenPtr->numComponents;
	    tokenPtr += tokenPtr->numComponents;
	    break;

	default:
	    Tcl_Panic("Unexpected token type in TclCompileTokens: %d; %.*s",
		    tokenPtr->type, tokenPtr->size, tokenPtr->start);
	}
    }

    /*
     * Push any accumulated characters appearing at the end.
     */

    if (Tcl_DStringLength(&textBuffer) > 0) {
	int literal = TclRegisterDStringLiteral(envPtr, &textBuffer);

	TclEmitPush(literal, envPtr);
	numObjsToConcat++;
    }

    /*
     * If necessary, concatenate the parts of the word.
     */

    while (numObjsToConcat > 255) {
	TclEmitInstInt1(INST_CONCAT1, 255, envPtr);
	numObjsToConcat -= 254;	/* concat pushes 1 obj, the result */
    }
    if (numObjsToConcat > 1) {
	TclEmitInstInt1(INST_CONCAT1, numObjsToConcat, envPtr);
    }

    /*
     * If the tokens yielded no instructions, push an empty string.
     */

    if (envPtr->codeNext == entryCodeNext) {
	TclEmitPush(TclRegisterNewLiteral(envPtr, "", 0), envPtr);
    }
    Tcl_DStringFree(&textBuffer);
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitByteCodeObj --
 *
 *	Create a ByteCode structure and initialize it from a CompileEnv
 *	compilation environment structure. The ByteCode structure is smaller
 *	and contains just that information needed to execute the bytecode
 *	instructions resulting from compiling a Tcl script. The resulting
 *	structure is placed in the specified object.
 *
 * Results:
 *	A newly constructed ByteCode object is stored in the internal
 *	representation of the objPtr.
 *
 * Side effects:
 *	A single heap object is allocated to hold the new ByteCode structure
 *	and its code, object, command location, and aux data arrays. Note that
 *	"ownership" (i.e., the pointers to) the Tcl objects and aux data items
 *	will be handed over to the new ByteCode structure from the CompileEnv
 *	structure.
 *
 *----------------------------------------------------------------------
 */

void
TclInitByteCodeObj(
    Tcl_Obj *objPtr,		/* Points object that should be initialized,
				 * and whose string rep contains the source
				 * code. */
    register CompileEnv *envPtr)/* Points to the CompileEnv structure from
				 * which to create a ByteCode structure. */
{
    register ByteCode *codePtr;
    size_t codeBytes, objArrayBytes, exceptArrayBytes, cmdLocBytes;
    size_t auxDataArrayBytes, structureSize;
    register unsigned char *p;
    int numLitObjects = envPtr->literalArrayNext;
    Namespace *namespacePtr;
    int i;
    Interp *iPtr;

    iPtr = envPtr->iPtr;

    codeBytes = envPtr->codeNext - envPtr->codeStart;
    objArrayBytes = envPtr->literalArrayNext * sizeof(Tcl_Obj *);
    exceptArrayBytes = envPtr->exceptArrayNext * sizeof(ExceptionRange);
    auxDataArrayBytes = envPtr->auxDataArrayNext * sizeof(AuxData);
    cmdLocBytes = GetCmdLocEncodingSize(envPtr);

    /*
     * Compute the total number of bytes needed for this bytecode.
     */

    structureSize = sizeof(ByteCode);
    structureSize += TCL_ALIGN(codeBytes);	  /* align object array */
    structureSize += TCL_ALIGN(objArrayBytes);	  /* align exc range arr */
    structureSize += TCL_ALIGN(exceptArrayBytes); /* align AuxData array */
    structureSize += auxDataArrayBytes;
    structureSize += cmdLocBytes;

    if (envPtr->iPtr->varFramePtr != NULL) {
	namespacePtr = envPtr->iPtr->varFramePtr->nsPtr;
    } else {
	namespacePtr = envPtr->iPtr->globalNsPtr;
    }

    p = ckalloc(structureSize);
    codePtr = (ByteCode *) p;
    codePtr->interpHandle = TclHandlePreserve(iPtr->handle);
    codePtr->nsPtr = namespacePtr;
    codePtr->nsEpoch = namespacePtr->resolverEpoch;
    codePtr->refCount = 1;
    if (namespacePtr->compiledVarResProc || iPtr->resolverPtr) {
	codePtr->flags = TCL_BYTECODE_RESOLVE_VARS;
    } else {
	codePtr->flags = 0;
    }
    codePtr->source = envPtr->source;
    codePtr->procPtr = envPtr->procPtr;

    codePtr->numCommands = envPtr->numCommands;
    codePtr->numSrcBytes = envPtr->numSrcBytes;
    codePtr->numCodeBytes = codeBytes;
    codePtr->numLitObjects = numLitObjects;
    codePtr->numExceptRanges = envPtr->exceptArrayNext;
    codePtr->numAuxDataItems = envPtr->auxDataArrayNext;
    codePtr->numCmdLocBytes = cmdLocBytes;
    codePtr->maxExceptDepth = envPtr->maxExceptDepth;
    codePtr->maxStackDepth = envPtr->maxStackDepth;

    p += sizeof(ByteCode);
    codePtr->codeStart = p;
    memcpy(p, envPtr->codeStart, (size_t) codeBytes);

    p += TCL_ALIGN(codeBytes);		/* align object array */
    codePtr->objArrayPtr = (Tcl_Obj **) p;
    for (i = 0;  i < numLitObjects;  i++) {
	if (objPtr == envPtr->literalArrayPtr[i].objPtr) {
	    /*
	     * Prevent circular reference where the bytecode intrep of
	     * a value contains a literal which is that same value.
	     * If this is allowed to happen, refcount decrements may not
	     * reach zero, and memory may leak.  Bugs 467523, 3357771
	     *
	     * NOTE:  [Bugs 3392070, 3389764] We make a copy based completely
	     * on the string value, and do not call Tcl_DuplicateObj() so we
             * can be sure we do not have any lingering cycles hiding in
	     * the intrep.
	     */
	    int numBytes;
	    const char *bytes = Tcl_GetStringFromObj(objPtr, &numBytes);

	    codePtr->objArrayPtr[i] = Tcl_NewStringObj(bytes, numBytes);
	    Tcl_IncrRefCount(codePtr->objArrayPtr[i]);
	    Tcl_DecrRefCount(objPtr);
	} else {
	    codePtr->objArrayPtr[i] = envPtr->literalArrayPtr[i].objPtr;
	}
    }

    p += TCL_ALIGN(objArrayBytes);	/* align exception range array */
    if (exceptArrayBytes > 0) {
	codePtr->exceptArrayPtr = (ExceptionRange *) p;
	memcpy(p, envPtr->exceptArrayPtr, (size_t) exceptArrayBytes);
    } else {
	codePtr->exceptArrayPtr = NULL;
    }

    p += TCL_ALIGN(exceptArrayBytes);	/* align AuxData array */
    if (auxDataArrayBytes > 0) {
	codePtr->auxDataArrayPtr = (AuxData *) p;
	memcpy(p, envPtr->auxDataArrayPtr, (size_t) auxDataArrayBytes);
    } else {
	codePtr->auxDataArrayPtr = NULL;
    }

    p += auxDataArrayBytes;
    EncodeCmdLocMap(envPtr, codePtr, (unsigned char *) p);

    /*
     * Record various compilation-related statistics about the new ByteCode
     * structure. Don't include overhead for statistics-related fields.
     */

    /*
     * Free the old internal rep then convert the object to a bytecode object
     * by making its internal rep point to the just compiled ByteCode.
     */

    TclFreeIntRep(objPtr);
    objPtr->internalRep.otherValuePtr = codePtr;
    objPtr->typePtr = &tclByteCodeType;

    codePtr->localCachePtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFindCompiledLocal --
 *
 *	This procedure is called at compile time to look up and optionally
 *	allocate an entry ("slot") for a variable in a procedure's array of
 *	local variables. If the variable's name is NULL, a new temporary
 *	variable is always created. (Such temporary variables can only be
 *	referenced using their slot index.)
 *
 * Results:
 *	If create is 0 and the name is non-NULL, then if the variable is
 *	found, the index of its entry in the procedure's array of local
 *	variables is returned; otherwise -1 is returned. If name is NULL, the
 *	index of a new temporary variable is returned. Finally, if create is 1
 *	and name is non-NULL, the index of a new entry is returned.
 *
 * Side effects:
 *	Creates and registers a new local variable if create is 1 and the
 *	variable is unknown, or if the name is NULL.
 *
 *----------------------------------------------------------------------
 */

int
TclFindCompiledLocal(
    register const char *name,	/* Points to first character of the name of a
				 * scalar or array variable. If NULL, a
				 * temporary var should be created. */
    int nameBytes,		/* Number of bytes in the name. */
    int create,			/* If 1, allocate a local frame entry for the
				 * variable if it is new. */
    CompileEnv *envPtr)		/* Points to the current compile environment*/
{
    register CompiledLocal *localPtr;
    int localVar = -1;
    register int i;
    Proc *procPtr;

    /*
     * If not creating a temporary, does a local variable of the specified
     * name already exist?
     */

    procPtr = envPtr->procPtr;

    if (procPtr == NULL) {
	/*
	 * Compiling a non-body script: give it read access to the LVT in the
	 * current localCache
	 */

	LocalCache *cachePtr = envPtr->iPtr->varFramePtr->localCachePtr;
	const char *localName;
	Tcl_Obj **varNamePtr;
	int len;

	if (!cachePtr || !name) {
	    return -1;
	}

	varNamePtr = &cachePtr->varName0;
	for (i=0; i < cachePtr->numVars; varNamePtr++, i++) {
	    if (*varNamePtr) {
		localName = Tcl_GetStringFromObj(*varNamePtr, &len);
		if ((len == nameBytes) && !strncmp(name, localName, len)) {
		    return i;
		}
	    }
	}
	return -1;
    }

    if (name != NULL) {
	int localCt = procPtr->numCompiledLocals;

	localPtr = procPtr->firstLocalPtr;
	for (i = 0;  i < localCt;  i++) {
	    if (!TclIsVarTemporary(localPtr)) {
		char *localName = localPtr->name;

		if ((nameBytes == localPtr->nameLength) &&
			(strncmp(name,localName,(unsigned)nameBytes) == 0)) {
		    return i;
		}
	    }
	    localPtr = localPtr->nextPtr;
	}
    }

    /*
     * Create a new variable if appropriate.
     */

    if (create || (name == NULL)) {
	localVar = procPtr->numCompiledLocals;
	localPtr = ckalloc(TclOffset(CompiledLocal, name) + nameBytes + 1);
	if (procPtr->firstLocalPtr == NULL) {
	    procPtr->firstLocalPtr = procPtr->lastLocalPtr = localPtr;
	} else {
	    procPtr->lastLocalPtr->nextPtr = localPtr;
	    procPtr->lastLocalPtr = localPtr;
	}
	localPtr->nextPtr = NULL;
	localPtr->nameLength = nameBytes;
	localPtr->frameIndex = localVar;
	localPtr->flags = 0;
	if (name == NULL) {
	    localPtr->flags |= VAR_TEMPORARY;
	}
	localPtr->defValuePtr = NULL;
	localPtr->resolveInfo = NULL;

	if (name != NULL) {
	    memcpy(localPtr->name, name, (size_t) nameBytes);
	}
	localPtr->name[nameBytes] = '\0';
	procPtr->numCompiledLocals++;
    }
    return localVar;
}

/*
 *----------------------------------------------------------------------
 *
 * TclExpandCodeArray --
 *
 *	Procedure that uses malloc to allocate more storage for a CompileEnv's
 *	code array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The byte code array in *envPtr is reallocated to a new array of double
 *	the size, and if envPtr->mallocedCodeArray is non-zero the old array
 *	is freed. Byte codes are copied from the old array to the new one.
 *
 *----------------------------------------------------------------------
 */

void
TclExpandCodeArray(
    void *envArgPtr)		/* Points to the CompileEnv whose code array
				 * must be enlarged. */
{
    CompileEnv *envPtr = envArgPtr;
				/* The CompileEnv containing the code array to
				 * be doubled in size. */

    /*
     * envPtr->codeNext is equal to envPtr->codeEnd. The currently defined
     * code bytes are stored between envPtr->codeStart and envPtr->codeNext-1
     * [inclusive].
     */

    size_t currBytes = envPtr->codeNext - envPtr->codeStart;
    size_t newBytes = 2 * (envPtr->codeEnd - envPtr->codeStart);

    if (envPtr->mallocedCodeArray) {
	envPtr->codeStart = ckrealloc(envPtr->codeStart, newBytes);
    } else {
	/*
	 * envPtr->codeStart isn't a ckalloc'd pointer, so we must code a
	 * ckrealloc equivalent for ourselves.
	 */

	unsigned char *newPtr = ckalloc(newBytes);

	memcpy(newPtr, envPtr->codeStart, currBytes);
	envPtr->codeStart = newPtr;
	envPtr->mallocedCodeArray = 1;
    }

    envPtr->codeNext = envPtr->codeStart + currBytes;
    envPtr->codeEnd = envPtr->codeStart + newBytes;
}

/*
 *----------------------------------------------------------------------
 *
 * EnterCmdStartData --
 *
 *	Registers the starting source and bytecode location of a command. This
 *	information is used at runtime to map between instruction pc and
 *	source locations.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Inserts source and code location information into the compilation
 *	environment envPtr for the command at index cmdIndex. The compilation
 *	environment's CmdLocation array is grown if necessary.
 *
 *----------------------------------------------------------------------
 */

static void
EnterCmdStartData(
    CompileEnv *envPtr,		/* Points to the compilation environment
				 * structure in which to enter command
				 * location information. */
    int cmdIndex,		/* Index of the command whose start data is
				 * being set. */
    int srcOffset,		/* Offset of first char of the command. */
    int codeOffset)		/* Offset of first byte of command code. */
{
    CmdLocation *cmdLocPtr;

    if ((cmdIndex < 0) || (cmdIndex >= envPtr->numCommands)) {
	Tcl_Panic("EnterCmdStartData: bad command index %d", cmdIndex);
    }

    if (cmdIndex >= envPtr->cmdMapEnd) {
	/*
	 * Expand the command location array by allocating more storage from
	 * the heap. The currently allocated CmdLocation entries are stored
	 * from cmdMapPtr[0] up to cmdMapPtr[envPtr->cmdMapEnd] (inclusive).
	 */

	size_t currElems = envPtr->cmdMapEnd;
	size_t newElems = 2 * currElems;
	size_t currBytes = currElems * sizeof(CmdLocation);
	size_t newBytes = newElems * sizeof(CmdLocation);

	if (envPtr->mallocedCmdMap) {
	    envPtr->cmdMapPtr = ckrealloc(envPtr->cmdMapPtr, newBytes);
	} else {
	    /*
	     * envPtr->cmdMapPtr isn't a ckalloc'd pointer, so we must code a
	     * ckrealloc equivalent for ourselves.
	     */

	    CmdLocation *newPtr = ckalloc(newBytes);

	    memcpy(newPtr, envPtr->cmdMapPtr, currBytes);
	    envPtr->cmdMapPtr = newPtr;
	    envPtr->mallocedCmdMap = 1;
	}
	envPtr->cmdMapEnd = newElems;
    }

    if (cmdIndex > 0) {
	if (codeOffset < envPtr->cmdMapPtr[cmdIndex-1].codeOffset) {
	    Tcl_Panic("EnterCmdStartData: cmd map not sorted by code offset");
	}
    }

    cmdLocPtr = &envPtr->cmdMapPtr[cmdIndex];
    cmdLocPtr->codeOffset = codeOffset;
    cmdLocPtr->srcOffset = srcOffset;
    cmdLocPtr->numSrcBytes = -1;
    cmdLocPtr->numCodeBytes = -1;
}

/*
 *----------------------------------------------------------------------
 *
 * EnterCmdExtentData --
 *
 *	Registers the source and bytecode length for a command. This
 *	information is used at runtime to map between instruction pc and
 *	source locations.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Inserts source and code length information into the compilation
 *	environment envPtr for the command at index cmdIndex. Starting source
 *	and bytecode information for the command must already have been
 *	registered.
 *
 *----------------------------------------------------------------------
 */

static void
EnterCmdExtentData(
    CompileEnv *envPtr,		/* Points to the compilation environment
				 * structure in which to enter command
				 * location information. */
    int cmdIndex,		/* Index of the command whose source and code
				 * length data is being set. */
    int numSrcBytes,		/* Number of command source chars. */
    int numCodeBytes)		/* Offset of last byte of command code. */
{
    CmdLocation *cmdLocPtr;

    if ((cmdIndex < 0) || (cmdIndex >= envPtr->numCommands)) {
	Tcl_Panic("EnterCmdExtentData: bad command index %d", cmdIndex);
    }

    if (cmdIndex > envPtr->cmdMapEnd) {
	Tcl_Panic("EnterCmdExtentData: missing start data for command %d",
		cmdIndex);
    }

    cmdLocPtr = &envPtr->cmdMapPtr[cmdIndex];
    cmdLocPtr->numSrcBytes = numSrcBytes;
    cmdLocPtr->numCodeBytes = numCodeBytes;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateExceptRange --
 *
 *	Procedure that allocates and initializes a new ExceptionRange
 *	structure of the specified kind in a CompileEnv.
 *
 * Results:
 *	Returns the index for the newly created ExceptionRange.
 *
 * Side effects:
 *	If there is not enough room in the CompileEnv's ExceptionRange array,
 *	the array in expanded: a new array of double the size is allocated, if
 *	envPtr->mallocedExceptArray is non-zero the old array is freed, and
 *	ExceptionRange entries are copied from the old array to the new one.
 *
 *----------------------------------------------------------------------
 */

int
TclCreateExceptRange(
    ExceptionRangeType type,	/* The kind of ExceptionRange desired. */
    register CompileEnv *envPtr)/* Points to CompileEnv for which to create a
				 * new ExceptionRange structure. */
{
    register ExceptionRange *rangePtr;
    int index = envPtr->exceptArrayNext;

    if (index >= envPtr->exceptArrayEnd) {
	/*
	 * Expand the ExceptionRange array. The currently allocated entries
	 * are stored between elements 0 and (envPtr->exceptArrayNext - 1)
	 * [inclusive].
	 */

	size_t currBytes =
		envPtr->exceptArrayNext * sizeof(ExceptionRange);
	int newElems = 2*envPtr->exceptArrayEnd;
	size_t newBytes = newElems * sizeof(ExceptionRange);

	if (envPtr->mallocedExceptArray) {
	    envPtr->exceptArrayPtr =
		    ckrealloc(envPtr->exceptArrayPtr, newBytes);
	} else {
	    /*
	     * envPtr->exceptArrayPtr isn't a ckalloc'd pointer, so we must
	     * code a ckrealloc equivalent for ourselves.
	     */

	    ExceptionRange *newPtr = ckalloc(newBytes);

	    memcpy(newPtr, envPtr->exceptArrayPtr, currBytes);
	    envPtr->exceptArrayPtr = newPtr;
	    envPtr->mallocedExceptArray = 1;
	}
	envPtr->exceptArrayEnd = newElems;
    }
    envPtr->exceptArrayNext++;

    rangePtr = &envPtr->exceptArrayPtr[index];
    rangePtr->type = type;
    rangePtr->nestingLevel = envPtr->exceptDepth;
    rangePtr->codeOffset = -1;
    rangePtr->numCodeBytes = -1;
    rangePtr->breakOffset = -1;
    rangePtr->continueOffset = -1;
    rangePtr->catchOffset = -1;
    return index;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateAuxData --
 *
 *	Procedure that allocates and initializes a new AuxData structure in a
 *	CompileEnv's array of compilation auxiliary data records. These
 *	AuxData records hold information created during compilation by
 *	CompileProcs and used by instructions during execution.
 *
 * Results:
 *	Returns the index for the newly created AuxData structure.
 *
 * Side effects:
 *	If there is not enough room in the CompileEnv's AuxData array, the
 *	AuxData array in expanded: a new array of double the size is
 *	allocated, if envPtr->mallocedAuxDataArray is non-zero the old array
 *	is freed, and AuxData entries are copied from the old array to the new
 *	one.
 *
 *----------------------------------------------------------------------
 */

int
TclCreateAuxData(
    ClientData clientData,	/* The compilation auxiliary data to store in
				 * the new aux data record. */
    const AuxDataType *typePtr,	/* Pointer to the type to attach to this
				 * AuxData */
    register CompileEnv *envPtr)/* Points to the CompileEnv for which a new
				 * aux data structure is to be allocated. */
{
    int index;			/* Index for the new AuxData structure. */
    register AuxData *auxDataPtr;
				/* Points to the new AuxData structure */

    index = envPtr->auxDataArrayNext;
    if (index >= envPtr->auxDataArrayEnd) {
	/*
	 * Expand the AuxData array. The currently allocated entries are
	 * stored between elements 0 and (envPtr->auxDataArrayNext - 1)
	 * [inclusive].
	 */

	size_t currBytes = envPtr->auxDataArrayNext * sizeof(AuxData);
	int newElems = 2*envPtr->auxDataArrayEnd;
	size_t newBytes = newElems * sizeof(AuxData);

	if (envPtr->mallocedAuxDataArray) {
	    envPtr->auxDataArrayPtr =
		    ckrealloc(envPtr->auxDataArrayPtr, newBytes);
	} else {
	    /*
	     * envPtr->auxDataArrayPtr isn't a ckalloc'd pointer, so we must
	     * code a ckrealloc equivalent for ourselves.
	     */

	    AuxData *newPtr = ckalloc(newBytes);

	    memcpy(newPtr, envPtr->auxDataArrayPtr, currBytes);
	    envPtr->auxDataArrayPtr = newPtr;
	    envPtr->mallocedAuxDataArray = 1;
	}
	envPtr->auxDataArrayEnd = newElems;
    }
    envPtr->auxDataArrayNext++;

    auxDataPtr = &envPtr->auxDataArrayPtr[index];
    auxDataPtr->clientData = clientData;
    auxDataPtr->type = typePtr;
    return index;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitJumpFixupArray --
 *
 *	Initializes a JumpFixupArray structure to hold some number of jump
 *	fixup entries.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The JumpFixupArray structure is initialized.
 *
 *----------------------------------------------------------------------
 */

void
TclInitJumpFixupArray(
    register JumpFixupArray *fixupArrayPtr)
				/* Points to the JumpFixupArray structure to
				 * initialize. */
{
    fixupArrayPtr->fixup = fixupArrayPtr->staticFixupSpace;
    fixupArrayPtr->next = 0;
    fixupArrayPtr->end = JUMPFIXUP_INIT_ENTRIES - 1;
    fixupArrayPtr->mallocedArray = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclExpandJumpFixupArray --
 *
 *	Procedure that uses malloc to allocate more storage for a jump fixup
 *	array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The jump fixup array in *fixupArrayPtr is reallocated to a new array
 *	of double the size, and if fixupArrayPtr->mallocedArray is non-zero
 *	the old array is freed. Jump fixup structures are copied from the old
 *	array to the new one.
 *
 *----------------------------------------------------------------------
 */

void
TclExpandJumpFixupArray(
    register JumpFixupArray *fixupArrayPtr)
				/* Points to the JumpFixupArray structure to
				 * enlarge. */
{
    /*
     * The currently allocated jump fixup entries are stored from fixup[0] up
     * to fixup[fixupArrayPtr->fixupNext] (*not* inclusive). We assume
     * fixupArrayPtr->fixupNext is equal to fixupArrayPtr->fixupEnd.
     */

    size_t currBytes = fixupArrayPtr->next * sizeof(JumpFixup);
    int newElems = 2*(fixupArrayPtr->end + 1);
    size_t newBytes = newElems * sizeof(JumpFixup);

    if (fixupArrayPtr->mallocedArray) {
	fixupArrayPtr->fixup = ckrealloc(fixupArrayPtr->fixup, newBytes);
    } else {
	/*
	 * fixupArrayPtr->fixup isn't a ckalloc'd pointer, so we must code a
	 * ckrealloc equivalent for ourselves.
	 */

	JumpFixup *newPtr = ckalloc(newBytes);

	memcpy(newPtr, fixupArrayPtr->fixup, currBytes);
	fixupArrayPtr->fixup = newPtr;
	fixupArrayPtr->mallocedArray = 1;
    }
    fixupArrayPtr->end = newElems;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFreeJumpFixupArray --
 *
 *	Free any storage allocated in a jump fixup array structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocated storage in the JumpFixupArray structure is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclFreeJumpFixupArray(
    register JumpFixupArray *fixupArrayPtr)
				/* Points to the JumpFixupArray structure to
				 * free. */
{
    if (fixupArrayPtr->mallocedArray) {
	ckfree(fixupArrayPtr->fixup);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclEmitForwardJump --
 *
 *	Procedure to emit a two-byte forward jump of kind "jumpType". Since
 *	the jump may later have to be grown to five bytes if the jump target
 *	is more than, say, 127 bytes away, this procedure also initializes a
 *	JumpFixup record with information about the jump.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The JumpFixup record pointed to by "jumpFixupPtr" is initialized with
 *	information needed later if the jump is to be grown. Also, a two byte
 *	jump of the designated type is emitted at the current point in the
 *	bytecode stream.
 *
 *----------------------------------------------------------------------
 */

void
TclEmitForwardJump(
    CompileEnv *envPtr,		/* Points to the CompileEnv structure that
				 * holds the resulting instruction. */
    TclJumpType jumpType,	/* Indicates the kind of jump: if true or
				 * false or unconditional. */
    JumpFixup *jumpFixupPtr)	/* Points to the JumpFixup structure to
				 * initialize with information about this
				 * forward jump. */
{
    /*
     * Initialize the JumpFixup structure:
     *    - codeOffset is offset of first byte of jump below
     *    - cmdIndex is index of the command after the current one
     *    - exceptIndex is the index of the first ExceptionRange after the
     *	    current one.
     */

    jumpFixupPtr->jumpType = jumpType;
    jumpFixupPtr->codeOffset = envPtr->codeNext - envPtr->codeStart;
    jumpFixupPtr->cmdIndex = envPtr->numCommands;
    jumpFixupPtr->exceptIndex = envPtr->exceptArrayNext;

    switch (jumpType) {
    case TCL_UNCONDITIONAL_JUMP:
	TclEmitInstInt4(INST_JUMP4, 0, envPtr);
	break;
    case TCL_TRUE_JUMP:
	TclEmitInstInt4(INST_JUMP_TRUE4, 0, envPtr);
	break;
    default:
	TclEmitInstInt4(INST_JUMP_FALSE4, 0, envPtr);
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclFixupForwardJump --
 *
 *	Procedure that updates a previously-emitted forward jump to jump a
 *	specified number of bytes, "jumpDist". If necessary, the jump is grown
 *	from two to five bytes; this is done if the jump distance is greater
 *	than "distThreshold" (normally 127 bytes). The jump is described by a
 *	JumpFixup record previously initialized by TclEmitForwardJump.
 *
 * Results:
 *	1 if the jump was grown and subsequent instructions had to be moved;
 *	otherwise 0. This result is returned to allow callers to update any
 *	additional code offsets they may hold.
 *
 * Side effects:
 *	The jump may be grown and subsequent instructions moved. If this
 *	happens, the code offsets for any commands and any ExceptionRange
 *	records between the jump and the current code address will be updated
 *	to reflect the moved code. Also, the bytecode instruction array in the
 *	CompileEnv structure may be grown and reallocated.
 *
 *----------------------------------------------------------------------
 */

int
TclFixupForwardJump(
    CompileEnv *envPtr,		/* Points to the CompileEnv structure that
				 * holds the resulting instruction. */
    JumpFixup *jumpFixupPtr,	/* Points to the JumpFixup structure that
				 * describes the forward jump. */
    int jumpDist,		/* Jump distance to set in jump instr. */
    int distThreshold)		/* Maximum distance before the two byte jump
				 * is grown to five bytes. */
{
    unsigned char *jumpPc;

    jumpPc = envPtr->codeStart + jumpFixupPtr->codeOffset;
    switch (jumpFixupPtr->jumpType) {
	case TCL_UNCONDITIONAL_JUMP:
	    TclUpdateInstInt4AtPc(INST_JUMP4, jumpDist, jumpPc);
	    break;
	case TCL_TRUE_JUMP:
	    TclUpdateInstInt4AtPc(INST_JUMP_TRUE4, jumpDist, jumpPc);
	    break;
	default:
	    TclUpdateInstInt4AtPc(INST_JUMP_FALSE4, jumpDist, jumpPc);
	    break;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetInstructionTable --
 *
 *	Returns a pointer to the table describing Tcl bytecode instructions.
 *	This procedure is defined so that clients can access the pointer from
 *	outside the TCL DLLs.
 *
 * Results:
 *	Returns a pointer to the global instruction table, same as the
 *	expression (&tclInstructionTable[0]).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const void * /* == InstructionDesc* == */
TclGetInstructionTable(void)
{
    return &tclInstructionTable[0];
}

/*
 *--------------------------------------------------------------
 *
 * TclRegisterAuxDataType --
 *
 *	This procedure is called to register a new AuxData type in the table
 *	of all AuxData types supported by Tcl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The type is registered in the AuxData type table. If there was already
 *	a type with the same name as in typePtr, it is replaced with the new
 *	type.
 *
 *--------------------------------------------------------------
 */

void
TclRegisterAuxDataType(
    const AuxDataType *typePtr)	/* Information about object type; storage must
				 * be statically allocated (must live forever;
				 * will not be deallocated). */
{
    register Tcl_HashEntry *hPtr;
    int isNew;

    Tcl_MutexLock(&tableMutex);
    if (!auxDataTypeTableInitialized) {
	TclInitAuxDataTypeTable();
    }

    /*
     * If there's already a type with the given name, remove it.
     */

    hPtr = Tcl_FindHashEntry(&auxDataTypeTable, typePtr->name);
    if (hPtr != NULL) {
	Tcl_DeleteHashEntry(hPtr);
    }

    /*
     * Now insert the new object type.
     */

    hPtr = Tcl_CreateHashEntry(&auxDataTypeTable, typePtr->name, &isNew);
    if (isNew) {
	Tcl_SetHashValue(hPtr, typePtr);
    }
    Tcl_MutexUnlock(&tableMutex);
}

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
    register Tcl_HashEntry *hPtr;
    const AuxDataType *typePtr = NULL;

    Tcl_MutexLock(&tableMutex);
    if (!auxDataTypeTableInitialized) {
	TclInitAuxDataTypeTable();
    }

    hPtr = Tcl_FindHashEntry(&auxDataTypeTable, typeName);
    if (hPtr != NULL) {
	typePtr = Tcl_GetHashValue(hPtr);
    }
    Tcl_MutexUnlock(&tableMutex);

    return typePtr;
}

/*
 *--------------------------------------------------------------
 *
 * TclInitAuxDataTypeTable --
 *
 *	This procedure is invoked to perform once-only initialization of the
 *	AuxData type table. It also registers the AuxData types defined in
 *	this file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Initializes the table of defined AuxData types "auxDataTypeTable" with
 *	builtin AuxData types defined in this file.
 *
 *--------------------------------------------------------------
 */

void
TclInitAuxDataTypeTable(void)
{
    /*
     * The table mutex must already be held before this routine is invoked.
     */

    auxDataTypeTableInitialized = 1;
    Tcl_InitHashTable(&auxDataTypeTable, TCL_STRING_KEYS);

    /*
     * There are only two AuxData type at this time, so register them here.
     */

}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeAuxDataTypeTable --
 *
 *	This procedure is called by Tcl_Finalize after all exit handlers have
 *	been run to free up storage associated with the table of AuxData
 *	types. This procedure is called by TclFinalizeExecution() which is
 *	called by Tcl_Finalize().
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes all entries in the hash table of AuxData types.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeAuxDataTypeTable(void)
{
    Tcl_MutexLock(&tableMutex);
    if (auxDataTypeTableInitialized) {
	Tcl_DeleteHashTable(&auxDataTypeTable);
	auxDataTypeTableInitialized = 0;
    }
    Tcl_MutexUnlock(&tableMutex);
}

/*
 *----------------------------------------------------------------------
 *
 * GetCmdLocEncodingSize --
 *
 *	Computes the total number of bytes needed to encode the command
 *	location information for some compiled code.
 *
 * Results:
 *	The byte count needed to encode the compiled location information.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetCmdLocEncodingSize(
    CompileEnv *envPtr)		/* Points to compilation environment structure
				 * containing the CmdLocation structure to
				 * encode. */
{
    register CmdLocation *mapPtr = envPtr->cmdMapPtr;
    int numCmds = envPtr->numCommands;
    int codeDelta, codeLen, srcDelta, srcLen;
    int codeDeltaNext, codeLengthNext, srcDeltaNext, srcLengthNext;
				/* The offsets in their respective byte
				 * sequences where the next encoded offset or
				 * length should go. */
    int prevCodeOffset, prevSrcOffset, i;

    codeDeltaNext = codeLengthNext = srcDeltaNext = srcLengthNext = 0;
    prevCodeOffset = prevSrcOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	codeDelta = mapPtr[i].codeOffset - prevCodeOffset;
	if (codeDelta < 0) {
	    Tcl_Panic("GetCmdLocEncodingSize: bad code offset");
	} else if (codeDelta <= 127) {
	    codeDeltaNext++;
	} else {
	    codeDeltaNext += 5;	/* 1 byte for 0xFF, 4 for positive delta */
	}
	prevCodeOffset = mapPtr[i].codeOffset;

	codeLen = mapPtr[i].numCodeBytes;
	if (codeLen < 0) {
	    Tcl_Panic("GetCmdLocEncodingSize: bad code length");
	} else if (codeLen <= 127) {
	    codeLengthNext++;
	} else {
	    codeLengthNext += 5;/* 1 byte for 0xFF, 4 for length */
	}

	srcDelta = mapPtr[i].srcOffset - prevSrcOffset;
	if ((-127 <= srcDelta) && (srcDelta <= 127) && (srcDelta != -1)) {
	    srcDeltaNext++;
	} else {
	    srcDeltaNext += 5;	/* 1 byte for 0xFF, 4 for delta */
	}
	prevSrcOffset = mapPtr[i].srcOffset;

	srcLen = mapPtr[i].numSrcBytes;
	if (srcLen < 0) {
	    Tcl_Panic("GetCmdLocEncodingSize: bad source length");
	} else if (srcLen <= 127) {
	    srcLengthNext++;
	} else {
	    srcLengthNext += 5;	/* 1 byte for 0xFF, 4 for length */
	}
    }

    return (codeDeltaNext + codeLengthNext + srcDeltaNext + srcLengthNext);
}

/*
 *----------------------------------------------------------------------
 *
 * EncodeCmdLocMap --
 *
 *	Encode the command location information for some compiled code into a
 *	ByteCode structure. The encoded command location map is stored as
 *	three adjacent byte sequences.
 *
 * Results:
 *	Pointer to the first byte after the encoded command location
 *	information.
 *
 * Side effects:
 *	The encoded information is stored into the block of memory headed by
 *	codePtr. Also records pointers to the start of the four byte sequences
 *	in fields in codePtr's ByteCode header structure.
 *
 *----------------------------------------------------------------------
 */

static unsigned char *
EncodeCmdLocMap(
    CompileEnv *envPtr,		/* Points to compilation environment structure
				 * containing the CmdLocation structure to
				 * encode. */
    ByteCode *codePtr,		/* ByteCode in which to encode envPtr's
				 * command location information. */
    unsigned char *startPtr)	/* Points to the first byte in codePtr's
				 * memory block where the location information
				 * is to be stored. */
{
    register CmdLocation *mapPtr = envPtr->cmdMapPtr;
    int numCmds = envPtr->numCommands;
    register unsigned char *p = startPtr;
    int codeDelta, codeLen, srcDelta, srcLen, prevOffset;
    register int i;

    /*
     * Encode the code offset for each command as a sequence of deltas.
     */

    codePtr->codeDeltaStart = p;
    prevOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	codeDelta = mapPtr[i].codeOffset - prevOffset;
	if (codeDelta < 0) {
	    Tcl_Panic("EncodeCmdLocMap: bad code offset");
	} else if (codeDelta <= 127) {
	    TclStoreInt1AtPtr(codeDelta, p);
	    p++;
	} else {
	    TclStoreInt1AtPtr(0xFF, p);
	    p++;
	    TclStoreInt4AtPtr(codeDelta, p);
	    p += 4;
	}
	prevOffset = mapPtr[i].codeOffset;
    }

    /*
     * Encode the code length for each command.
     */

    codePtr->codeLengthStart = p;
    for (i = 0;  i < numCmds;  i++) {
	codeLen = mapPtr[i].numCodeBytes;
	if (codeLen < 0) {
	    Tcl_Panic("EncodeCmdLocMap: bad code length");
	} else if (codeLen <= 127) {
	    TclStoreInt1AtPtr(codeLen, p);
	    p++;
	} else {
	    TclStoreInt1AtPtr(0xFF, p);
	    p++;
	    TclStoreInt4AtPtr(codeLen, p);
	    p += 4;
	}
    }

    /*
     * Encode the source offset for each command as a sequence of deltas.
     */

    codePtr->srcDeltaStart = p;
    prevOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	srcDelta = mapPtr[i].srcOffset - prevOffset;
	if ((-127 <= srcDelta) && (srcDelta <= 127) && (srcDelta != -1)) {
	    TclStoreInt1AtPtr(srcDelta, p);
	    p++;
	} else {
	    TclStoreInt1AtPtr(0xFF, p);
	    p++;
	    TclStoreInt4AtPtr(srcDelta, p);
	    p += 4;
	}
	prevOffset = mapPtr[i].srcOffset;
    }

    /*
     * Encode the source length for each command.
     */

    codePtr->srcLengthStart = p;
    for (i = 0;  i < numCmds;  i++) {
	srcLen = mapPtr[i].numSrcBytes;
	if (srcLen < 0) {
	    Tcl_Panic("EncodeCmdLocMap: bad source length");
	} else if (srcLen <= 127) {
	    TclStoreInt1AtPtr(srcLen, p);
	    p++;
	} else {
	    TclStoreInt1AtPtr(0xFF, p);
	    p++;
	    TclStoreInt4AtPtr(srcLen, p);
	    p += 4;
	}
    }

    return p;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * End:
 */
