#include "tclInt.h"
#include "tclCompile.h"
#include "tclAssembly.h"
#include "tclOOInt.h"

/* Static functions defined in this file */

static void AddBasicBlockRangeToErrorInfo(Tcl_Interp* interp, Tcl_Obj* bcList,
					  BasicBlock* bbPtr);
static void AddInstructionToErrorInfo(Tcl_Interp* interp, Tcl_Obj* bcList,
				      int index);
static BasicBlock * AllocBB(CompileEnv*, int); 
static int CheckNamespaceQualifiers(Tcl_Interp*, const char*);
static int CheckOneByte(Tcl_Interp*, int);
static int CheckSignedOneByte(Tcl_Interp*, int);
static int StackCheckBasicBlock(StackCheckerState* , BasicBlock *, BasicBlock *, int);
static BasicBlock* StartBasicBlock(CompileEnv* envPtr, 	Tcl_HashTable* BBHash,
				   BasicBlock* currBB, int fallsThrough,
				   int bcIndex, const char* jumpLabel);

static int CheckStack(Tcl_Interp*, CompileEnv*, BasicBlock *, Tcl_Obj*);
static void FreeAssembleCodeInternalRep(Tcl_Obj *objPtr);
static ByteCode * CompileAssembleObj(Tcl_Interp *interp, Tcl_Obj *objPtr);
static int DefineLabel(Tcl_Interp* interp, CompileEnv* envPtr,
		       const char* label, Tcl_HashTable* labelHash);

static const Tcl_ObjType assembleCodeType = {
    "assemblecode",
    FreeAssembleCodeInternalRep,	/* freeIntRepProc */
    NULL,	/* dupIntRepProc */
    NULL,			/* updateStringProc */
    NULL			/* setFromAnyProc */
};

/* static int AdvanceIp(const unsigned char *pc); */
static int StackCheckBasicBlock(StackCheckerState* , BasicBlock *, BasicBlock *, int);

/*
 * TIP #280: Remember the per-word line information of the current command. An
 * index is used instead of a pointer as recursive compilation may reallocate,
 * i.e. move, the array. This is also the reason to save the nuloc now, it may
 * change during the course of the function.
 *
 * Macro to encapsulate the variable definition and setup.
 */

#define DefineLineInformation \
    ExtCmdLoc *mapPtr = envPtr->extCmdMapPtr;				\
    int eclIndex = mapPtr->nuloc - 1

#define SetLineInformation(word) \
    envPtr->line = mapPtr->loc[eclIndex].line[(word)];			\
    envPtr->clNext = mapPtr->loc[eclIndex].next[(word)]

/*
 * Flags bits used by PushVarName.
 */

#define TCL_NO_LARGE_INDEX 1	/* Do not return localIndex value > 255 */

talInstDesc talInstructionTable[] = {

    /* PUSH must be first, see the code near the end of TclAssembleCode */

    {"push",    ASSEM_PUSH  ,   (INST_PUSH1<<8
			         | INST_PUSH4), 0   ,   1},

    {"add",	ASSEM_1BYTE ,   INST_ADD    ,   2   ,   1},
    {"append",  ASSEM_LVT,	(INST_APPEND_SCALAR1<<8
	    			 | INST_APPEND_SCALAR4),
                                                1,      1},
    {"appendArray",
                ASSEM_LVT,	(INST_APPEND_ARRAY1<<8
	    			 | INST_APPEND_ARRAY4),
                                                2,      1},
    {"appendArrayStk", 
                ASSEM_1BYTE,    INST_APPEND_ARRAY_STK,
                                                3,      1}, 
    {"appendStk", 
                ASSEM_1BYTE,    INST_APPEND_STK, 
                                                2,      1}, 
    {"concat",	ASSEM_CONCAT1,	INST_CONCAT1,	INT_MIN,1},
    {"eval",	ASSEM_EVAL,	0,		0,	1},
    {"evalStk",	ASSEM_1BYTE,	INST_EVAL_STK,	1,	1},
    {"exprStk", ASSEM_1BYTE,	INST_EXPR_STK,	1,	1},
    {"exist",	ASSEM_LVT4,	INST_EXIST_SCALAR,
                                                0,      1},
    {"existArray",
		ASSEM_LVT4,	INST_EXIST_ARRAY,
                                                1,      1},
    {"existArrayStk",
		ASSEM_1BYTE,	INST_EXIST_ARRAY_STK,
						2,	1},
    {"existStk",
		ASSEM_1BYTE,	INST_EXIST_STK,	1,	1},
    {"bitand",  ASSEM_1BYTE ,   INST_BITAND ,   2   ,   1},
    {"bitnot",	ASSEM_1BYTE,    INST_BITNOT,    2,      1},
    {"bitor",   ASSEM_1BYTE ,   INST_BITOR  ,   2   ,   1},
    {"bitxor",  ASSEM_1BYTE ,   INST_BITXOR ,   2   ,   1},
    {"div",     ASSEM_1BYTE,    INST_DIV,       2,      1},
    {"dup",     ASSEM_1BYTE ,   INST_DUP    ,   1   ,   2}, 
    {"eq",      ASSEM_1BYTE ,   INST_EQ     ,   2   ,   1},
    {"expon",   ASSEM_1BYTE,    INST_EXPON,     2,      1},
    {"ge",      ASSEM_1BYTE ,   INST_GE     ,   2   ,   1},
    {"gt",      ASSEM_1BYTE ,   INST_GT     ,   2   ,   1},
    {"incr",    ASSEM_LVT1,     INST_INCR_SCALAR1,
                                                1,      1},
    {"incrArray", 
                ASSEM_LVT1,     INST_INCR_ARRAY1, 
                                                2,      1},
    {"incrArrayImm", 
                ASSEM_LVT1_SINT1,
                                INST_INCR_ARRAY1_IMM, 
                                                1,      1},
    {"incrArrayStk", ASSEM_1BYTE, INST_INCR_ARRAY_STK, 
                                                3,      1},
    {"incrArrayStkImm", 
		ASSEM_SINT1,    INST_INCR_ARRAY_STK_IMM, 
                                                2,      1},    
    {"incrImm", 
                ASSEM_LVT1_SINT1,
                                INST_INCR_SCALAR1_IMM, 
                                                0,      1},
    {"incrStk", ASSEM_1BYTE,    INST_INCR_SCALAR_STK, 
                                                2,      1},
    {"incrStkImm", 
                ASSEM_SINT1,    INST_INCR_SCALAR_STK_IMM, 
                                                1,      1},
    {"invokeStk",
                ASSEM_INVOKE,   (INST_INVOKE_STK1 << 8
		                 | INST_INVOKE_STK4),
                                                INT_MIN,1},
    {"jump",    ASSEM_JUMP,     (INST_JUMP1 << 8
		                 | INST_JUMP4), 0,      0},
    {"jumpFalse",
                ASSEM_JUMP,     (INST_JUMP_FALSE1 << 8
	                         | INST_JUMP_FALSE4), 
                                                1,      0},
    {"jumpTrue",ASSEM_JUMP,     (INST_JUMP_TRUE1 << 8
		                 | INST_JUMP_TRUE4), 
                                                1,      0},
    {"label",   ASSEM_LABEL,    0, 		0,	0}, 
    {"land",    ASSEM_1BYTE ,   INST_LAND   ,   2   ,   1},
    {"lappend",  ASSEM_LVT,	(INST_LAPPEND_SCALAR1<<8
	    			 | INST_LAPPEND_SCALAR4),
                                                1,      1},
    {"lappendArray",
                ASSEM_LVT,	(INST_LAPPEND_ARRAY1<<8
	    			 | INST_LAPPEND_ARRAY4),
                                                2,      1},
    {"lappendArrayStk", 
                ASSEM_1BYTE,    INST_LAPPEND_ARRAY_STK,
                                                3,      1}, 
    {"lappendStk", 
                ASSEM_1BYTE,    INST_LAPPEND_STK, 
                                                2,      1}, 
    {"le",      ASSEM_1BYTE ,   INST_LE     ,   2   ,   1},
    {"listIndex", 
     		ASSEM_1BYTE,    INST_LIST_INDEX,2,      1},
    {"listLength",
                ASSEM_1BYTE,    INST_LIST_LENGTH,
                                                1,    1},
    {"load",    ASSEM_LVT,      (INST_LOAD_SCALAR1 << 8
	                         | INST_LOAD_SCALAR4), 
                                                0,      1}, 
    {"loadArray",
                ASSEM_LVT,      (INST_LOAD_ARRAY1<<8
			         | INST_LOAD_ARRAY4),
                                                1,      1},
    {"loadArrayStk", 
                ASSEM_1BYTE,    INST_LOAD_ARRAY_STK,
                                                2,      1},
    {"loadStk", ASSEM_1BYTE,    INST_LOAD_SCALAR_STK,
                                                1,      1},
    {"lor",     ASSEM_1BYTE ,   INST_LOR    ,   2   ,   1},
    {"lsetList", 
                ASSEM_1BYTE,    INST_LSET_LIST, 3,      1},
    {"lshift",  ASSEM_1BYTE ,   INST_LSHIFT ,   2   ,   1},
    {"lt",      ASSEM_1BYTE ,   INST_LT     ,   2   ,   1},
    {"mod",     ASSEM_1BYTE,    INST_MOD,       2,      1},
    {"mult",    ASSEM_1BYTE ,   INST_MULT   ,   2   ,   1},
    {"neq",     ASSEM_1BYTE ,   INST_NEQ    ,   2   ,   1},
    {"not",     ASSEM_1BYTE,    INST_LNOT,      2,      1},
    {"over",    ASSEM_OVER,     INST_OVER,      INT_MIN, -1-1},
    {"pop",     ASSEM_1BYTE ,   INST_POP    ,   1   ,   0},
    {"reverse", ASSEM_REVERSE,  INST_REVERSE,   INT_MIN, -1-0},
    {"rshift",  ASSEM_1BYTE ,   INST_RSHIFT ,   2   ,   1},
    {"store",   ASSEM_LVT,      (INST_STORE_SCALAR1<<8
				      | INST_STORE_SCALAR4),
                                                1,      1}, 
    {"storeArray", 
                ASSEM_LVT,      (INST_STORE_ARRAY1<<8
                                 | INST_STORE_ARRAY4),
                                                2,      1}, 
    {"storeArrayStk", 
                ASSEM_1BYTE,    INST_STORE_ARRAY_STK,
                                                3,      1}, 
    {"storeStk", 
                ASSEM_1BYTE,    INST_STORE_SCALAR_STK, 
                                                2,      1}, 
    {"strcmp",  ASSEM_1BYTE,    INST_STR_CMP,   2,      1},
    {"streq",   ASSEM_1BYTE,    INST_STR_EQ,    2,      1},
    {"strindex", 
                ASSEM_1BYTE,    INST_STR_INDEX, 2,      1},
    {"strlen",  ASSEM_1BYTE,    INST_STR_LEN,   1,      1},
    {"strmatch",
                ASSEM_BOOL,     INST_STR_MATCH, 2,      1},
    {"strneq",  ASSEM_1BYTE,    INST_STR_NEQ,   2,      1},
    {"sub",     ASSEM_1BYTE ,   INST_SUB    ,   2   ,   1},
    {"uminus",  ASSEM_1BYTE,    INST_UMINUS,    1,      1},
    {"unset",	ASSEM_BOOL_LVT4,
				INST_UNSET_SCALAR,
                                                0,      0},
    {"unsetArray",
		ASSEM_BOOL_LVT4,
				INST_UNSET_ARRAY,
                                                1,      0},
    {"unsetArrayStk",
		ASSEM_BOOL,	INST_UNSET_ARRAY_STK,
						2,	0},
    {"unsetStk",
		ASSEM_BOOL,	INST_UNSET_STK,	1,	0},
    {"uplus",   ASSEM_1BYTE,    INST_UPLUS,     1,      1},
    {NULL, 0, 0,0}
};

/*
 *-----------------------------------------------------------------------------
 *
 * BBAdjustStackDepth --
 *
 *	When an opcode is emitted, adjusts the stack information in the
 *	basic block to reflect the number of operands produced and consumed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Updates minimum, maximum and final stack requirements in the
 *	basic block.
 *
 *-----------------------------------------------------------------------------
 */

static void
BBAdjustStackDepth(BasicBlock* bbPtr,
				/* Structure describing the basic block */
		   int consumed,
				/* Count of operands consumed by the 
				 * operation */
		   int produced)
				/* Count of operands produced by the
				 * operation */
{
    int depth = bbPtr->finalStackDepth;
    depth -= consumed;
    if (depth < bbPtr->minStackDepth) {
	bbPtr->minStackDepth = depth;
    }
    depth += produced;
    if (depth > bbPtr->maxStackDepth) {
	bbPtr->maxStackDepth = depth;
    }
    bbPtr->finalStackDepth = depth;
#if 0
    fprintf(stderr, "update bb: consumed %d produced %d"
	    " min %d max %d final %d\n",
	    consumed, produced, bbPtr->minStackDepth, bbPtr->maxStackDepth,
	    bbPtr->finalStackDepth);
    fflush(stderr);
#endif
}

/*
 *-----------------------------------------------------------------------------
 *
 * BBUpdateStackReqs --
 *
 *	Updates the stack requirements of a basic block, given the opcode
 *	being emitted and an operand count.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Updates min, max and final stack requirements in the basic block.
 *
 * Notes:
 *	This function must not be called for instructions such as REVERSE
 *	and OVER that are variadic but do not consume all their operands.
 *	Instead, BBAdjustStackDepth should be called directly.
 *
 *	count should be provided only for variadic operations. For
 *	operations with known arity, count should be 0.
 *
 *-----------------------------------------------------------------------------
 */

static void
BBUpdateStackReqs(BasicBlock* bbPtr,
				/* Structure describing the basic block */
		  int tblind,   /* Index in talInstructionTable of the
				 * operation being assembled */
		  int count)	/* Count of operands for variadic insts */
{
    int consumed = talInstructionTable[tblind].operandsConsumed;
    int produced = talInstructionTable[tblind].operandsProduced;
    if (consumed == INT_MIN) {
	/* The instruction is variadic; it consumes 'count' operands. */
	consumed = count;
    }
    if (produced < 0) {
	/* The instruction leaves some of its operations on the stack,
	 * with net stack effect of '-1-produced' */
	produced = consumed - produced - 1;
    }
    BBAdjustStackDepth(bbPtr, consumed, produced);
}

/*
 *-----------------------------------------------------------------------------
 *
 * BBEmitOpcode, BBEmitInstInt1, BBEmitInstInt4 --
 *
 *	Emit the opcode part of an instruction, or the entirety of an
 *      instruction with a 1- or 4-byte operand, and adjust stack requirements.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores instruction and operand in the operand stream, and
 *	adjusts the stack.
 *
 *-----------------------------------------------------------------------------
 */

static void
BBEmitOpcode(CompileEnv* envPtr,/* Compilation environment */
	     BasicBlock* bbPtr,	/* Basic block to which the op belongs */
	     int tblind,	/* Table index in talInstructionTable of op */
	     int count)		/* Operand count for variadic ops */
{
    int op = talInstructionTable[tblind].tclInstCode & 0xff;
#if 0
    fprintf(stderr, "Emit %s (%d)\n", tclInstructionTable[op].name, count);
    fflush(stderr);
#endif
    TclEmitInt1(op, envPtr);
    envPtr->atCmdStart = ((op) == INST_START_CMD);
    BBUpdateStackReqs(bbPtr, tblind, count);
}
static void
BBEmitInstInt1(CompileEnv* envPtr,
				/* Compilation environment */
	       BasicBlock* bbPtr,
				/* basic block to which the op belongs */
	       int tblind,	/* Index in talInstructionTable of op */
	       unsigned char opnd,
				/* 1-byte operand */
	       int count)	/* Operand count for variadic ops */
{
    BBEmitOpcode(envPtr, bbPtr, tblind, count);
    TclEmitInt1(opnd, envPtr);
}
static void
BBEmitInstInt4(CompileEnv* envPtr,
				/* Compilation environment */
	       BasicBlock* bbPtr,
				/* basic block to which the op belongs */
	       int tblind,	/* Index in talInstructionTable of op */
	       int opnd,	/* 4-byte operand */
	       int count)	/* Operand count for variadic ops */
{
    BBEmitOpcode(envPtr, bbPtr, tblind, count);
    TclEmitInt4(opnd, envPtr);
}

/*
 *-----------------------------------------------------------------------------
 *
 * BBEmitInst1or4 --
 *
 *	Emits a 1- or 4-byte operation according to the magnitude of the
 *	operand
 *
 *-----------------------------------------------------------------------------
 */

static void
BBEmitInst1or4(CompileEnv* envPtr,
				/* Compilation environment */
	       BasicBlock* bbPtr,
				/* Basic block under construction */
	       int tblind,	/* Index in talInstructionTable of op */
	       int param,	/* Variable-length parameter */
	       int count)	/* Arity if variadic */
{

    int op = talInstructionTable[tblind].tclInstCode;
    if (param <= 0xff) {
	op >>= 8;
    } else {
	op &= 0xff;
    }
#if 0
    fprintf(stderr, "Emit %s (%d)\n", tclInstructionTable[op].name, count);
    fflush(stderr);
#endif
    TclEmitInt1(op, envPtr);
    if (param <= 0xff) {
	TclEmitInt1(param, envPtr);
    } else {
	TclEmitInt4(param, envPtr);
    }
    envPtr->atCmdStart = ((op) == INST_START_CMD);
    BBUpdateStackReqs(bbPtr, tblind, count);
}

int
Tcl_AssembleObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    return Tcl_NRCallObjProc(interp, TclNRAssembleObjCmd, dummy, objc, objv);
}

int
TclNRAssembleObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Obj *objPtr;
    ByteCode *codePtr;

#if 0
    int i;
    fprintf(stderr, "TclNRAssembleObjCmd:");
    for (i=0; i < objc; ++i) {
	fprintf(stderr, " {%s}", Tcl_GetString(objv[i]));
    }
    fprintf(stderr, "\n"); fflush(stderr);
#endif

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "bytecodeList");
	return TCL_ERROR;
    }

    objPtr = objv[1];
    codePtr = CompileAssembleObj(interp, objPtr);
    if (codePtr == NULL) {
	return TCL_ERROR;
    }

#if 0
    fprintf(stderr, "bytecode: %p\n", codePtr);
#endif
    Tcl_NRAddCallback(interp, NRCallTEBC, INT2PTR(TCL_NR_BC_TYPE), codePtr,
	    NULL, NULL);

    return TCL_OK;
}

static ByteCode *
CompileAssembleObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr)
{
    Interp *iPtr = (Interp *) interp;
    CompileEnv compEnv;		/* Compilation environment structure allocated
				 * in frame. */
    register ByteCode *codePtr = NULL;
				/* Tcl Internal type of bytecode. Initialized
				 * to avoid compiler warning. */
    int status;			/* Status return from Tcl_AssembleCode */

    /*
     * Get the expression ByteCode from the object. If it exists, make sure it
     * is valid in the current context.
     */
     
    if (objPtr->typePtr == &assembleCodeType) {
	Namespace *namespacePtr = iPtr->varFramePtr->nsPtr;

	codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
	if (((Interp *) *codePtr->interpHandle != iPtr)
	    || (codePtr->compileEpoch != iPtr->compileEpoch)
	    || (codePtr->nsPtr != namespacePtr)
	    || (codePtr->nsEpoch != namespacePtr->resolverEpoch)
	    || (codePtr->localCachePtr != iPtr->varFramePtr->localCachePtr)) {
		    
	    FreeAssembleCodeInternalRep(objPtr);
	}
    }
    if (objPtr->typePtr != &assembleCodeType) {

	int length;
		
	const char *string = TclGetStringFromObj(objPtr, &length);
	TclInitCompileEnv(interp, &compEnv, string, length, NULL, 0);
#if 0
	fprintf(stderr, "assembling: %s\n", string); fflush(stderr);
#endif
	status = TclAssembleCode(interp, objPtr, &compEnv, TCL_EVAL_DIRECT);
	if (status != TCL_OK) {
#if 0
	    fprintf(stderr, "assembly failed: %s\n", 
		    Tcl_GetString(Tcl_GetObjResult(interp)));
	    fflush(stderr);
#endif
	    /* FIXME - there's memory to clean up */
	    return NULL;
	}

	/*
	 * Successful compilation. If the expression yielded no instructions,
	 * push an zero object as the expression's result.
	 */

	if (compEnv.codeNext == compEnv.codeStart) {
	    fprintf(stderr, "empty bytecode, why did this happen?\n");
	    fflush(stderr);
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
	objPtr->typePtr = &assembleCodeType;
	TclFreeCompileEnv(&compEnv);
	codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
	if (iPtr->varFramePtr->localCachePtr) {
	    codePtr->localCachePtr = iPtr->varFramePtr->localCachePtr;
	    codePtr->localCachePtr->refCount++;
	}
#ifdef TCL_COMPILE_DEBUG
	if (tclTraceCompile >= 2) {
	    TclPrintByteCodeObj(interp, objPtr);
	    fflush(stdout);
	}
#endif /* TCL_COMPILE_DEBUG */
    }
    return codePtr;
}

/*
 *-----------------------------------------------------------------------------
 *
 * TclCompileAssembleCmd --
 *
 *	Compilation procedure for the '::tcl::unsupported::assemble' command.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Puts the result of assembling the code into the bytecode stream
 *	in 'compileEnv'.
 *
 * This procedure makes sure that the command has a single arg, which is
 * constant. If that condition is met, the procedure calls TclAssembleCode
 * to produce bytecode for the given assembly code, and returns any error
 * resulting from the assembly.
 *
 *-----------------------------------------------------------------------------
 */

int TclCompileAssembleCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to defintion of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{ 
    Tcl_Token *tokenPtr;	/* Token in the input script */
    Tcl_Obj *bcList;		/* List of assembly instructions to process */
    int status;			/* Status return from assembling the code */

    /* Make sure that the command has a single arg */

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    /* Make sure that the arg is a simple word */

    tokenPtr = TokenAfter(parsePtr->tokenPtr);
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	return TCL_ERROR;
    }

    /* Compile the code and return any error from the compilation */

    bcList = Tcl_NewStringObj(tokenPtr[1].start, tokenPtr[1].size);
    Tcl_IncrRefCount(bcList);
    status = TclAssembleCode(interp, bcList, envPtr, 0);
    Tcl_DecrRefCount(bcList);
    return status;

}

/*
 *-----------------------------------------------------------------------------
 *
 * TclAssembleCode --
 *
 *	Take a list of instructions in a Tcl_Obj, and assemble them to
 *	Tcl bytecodes
 *
 * Results:
 *	Returns TCL_OK on success, TCL_ERROR on failure.
 *	If 'flags' includes TCL_EVAL_DIRECT, places an error message
 *	in the interpreter result.
 *
 * Side effects:
 *	Adds byte codes to the compile environment, and updates the
 *	environment's stack depth.
 *
 *-----------------------------------------------------------------------------
 */

MODULE_SCOPE int
TclAssembleCode(Tcl_Interp *interp, 
				/* Tcl interpreter */
		Tcl_Obj * bcList, 
				/* List of assembly instructions */
		CompileEnv *envPtr, 	
				/* Compilation environment that is to
				 * receive the generated bytecode */
		int flags)	/* OR'ed combination of flags */
{

    int bcListLen = 0;		/* Length of the assembly code list */
    Tcl_HashTable labelHash;	/* Hashtable storing information about
				 * labels in the assembly code */
    Tcl_HashTable BBHash;	/* Hashtable storing information about
				 * the basic blocks in the bytecode */

    BasicBlock* curr_bb = NULL;	/* Structure describing the current basic 
				 * block */
    BasicBlock* head_bb = NULL;	/* Structure describing the first basic
				 * block in the code */
    int ind;			/* Index in the list of instructions */
    int result;			/* Return value from this function */
    Tcl_Obj* bc;		/* One assembly instruction from the list */
    int bcSize = 0;		/* Length of the instruction sublist */
    Tcl_Obj ** bcArgs;		/* Arguments to the instruction */
    char * instName; 		/* Name of the instruction */
    enum talInstType instType;	/* Type of the current assembly instruction */
    unsigned char instCode;	/* Opcode of the current assembly instruction */
    const char* operand1;	/* First operand passed to the instruction */
    int operand1Len;		/* Length of the first operand */
    int tblind = 0;		/* Index in the instruction table of the
				 * current instruction */
    int isNew;			/* Flag == 1 if a JUMP is the first
				 * occurrence of its associated label */
    Tcl_Obj* resultObj;		/* Error message */
    int savedMaxStackDepth;	/* Max stack depth saved around compilation
				 * calls */
    int savedCurrStackDepth;	/* Current stack depth saved around
				 * compilation calls. */

    int localVar, opnd = 0;
    label *l;
    Tcl_HashEntry * entry;
    int litIndex;
     
    DefineLineInformation;	/* TIP #280 */ /* eclIndex? */
     
#if 0
    fprintf(stderr, "Assembling: %s\n", Tcl_GetString(bcList));
    fflush(stderr);
#endif

    /* Test that the bytecode that we're given is a well formed list */

    if (Tcl_ListObjLength(interp, bcList, &bcListLen) == TCL_ERROR) {
	return TCL_ERROR;
    }
  
    /* Initialize the symbol table and the table of basic blocks */

    Tcl_InitHashTable(&labelHash, TCL_STRING_KEYS);
    Tcl_InitHashTable(&BBHash, TCL_STRING_KEYS);

    /* Allocate a structure to describe the first basic block */

    curr_bb = AllocBB(envPtr, 0);
    head_bb = curr_bb;
     
    /* 
     * Index through the assembly directives and instructions, generating code.
     */

    for (ind = 0; ind < bcListLen; ind++) {

	/* Extract the instruction name from a list element */

	result = TCL_OK;
	if (Tcl_ListObjIndex(interp, bcList, ind, &bc) != TCL_OK
	    || Tcl_ListObjGetElements(interp, bc, &bcSize, &bcArgs) != TCL_OK) {
	    goto cleanup;
	}
	if (bcSize == 0) {
	    continue;
	}
	instName = Tcl_GetStringFromObj(bcArgs[0], NULL);
#if 0
	fprintf(stderr, "[%d] %s\n",
		envPtr->codeNext - envPtr->codeStart, instName); 
	fflush(stderr);
#endif

	/* 
	 * Extract the first operand, if there is one, and get its string
       	 * representation 
	 */

	if (bcSize >= 2) {
	    operand1 = Tcl_GetStringFromObj(bcArgs[1], &operand1Len);
	} else {
	    operand1 = NULL; 
	    operand1Len = 0;
	}

	/* Look up the instruction in the table of instructions */

	if (Tcl_GetIndexFromObjStruct(interp, bcArgs[0],
				      &talInstructionTable[0].name,
				      sizeof(talInstDesc), "instruction",
				      TCL_EXACT, &tblind) != TCL_OK) {
	    goto cleanup;
	}

	/* Vector on the type of instruction being processed */

	instType = talInstructionTable[tblind].instType;
	instCode = talInstructionTable[tblind].tclInstCode;
	switch (instType) {

	case ASSEM_LABEL:

	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "name");
		goto cleanup;
	    }
	    /* Add the (label_name, address) pair to the hash table */
	    if (DefineLabel(interp, envPtr, operand1, &labelHash) != TCL_OK) {
		goto cleanup;
	    }
                    
	    /* End the current basic block and start a new one */ 

	    curr_bb = StartBasicBlock(envPtr, &BBHash, curr_bb, 1, ind, NULL);

	    /* Attach the label to the new basic block */

	    entry = Tcl_CreateHashEntry(&BBHash, operand1, &opnd);
	    Tcl_SetHashValue(entry, curr_bb);
                    
	    break;
                
	case ASSEM_1BYTE:
	    if (bcSize != 1) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "");
		goto cleanup;
	    }
	    BBEmitOpcode(envPtr, curr_bb, tblind, 0);
	    break;

	case ASSEM_INVOKE:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "count");
		goto cleanup;
	    }
	    if (Tcl_GetIntFromObj(interp, bcArgs[1], &opnd) != TCL_OK) {
		goto cleanup;
	    }
	    BBEmitInst1or4(envPtr, curr_bb, tblind, opnd, opnd);
	    break;
		 
	case ASSEM_JUMP: 
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "label");
		goto cleanup;
	    }
	    entry = Tcl_CreateHashEntry(&labelHash, operand1, &isNew);
	    if (isNew) {
		l = (label *) ckalloc(sizeof(label));
		l -> isDefined = 0;
		l -> offset = -1;
		Tcl_SetHashValue(entry, l);
	    } else {
		l = Tcl_GetHashValue(entry);
	    }
	    if (l -> isDefined) {
		BBEmitInst1or4(envPtr, curr_bb, tblind, 
			       l->offset + envPtr->codeStart
			       - envPtr->codeNext, 0);
	    } else {
		int here = envPtr->codeNext - envPtr->codeStart;
		BBEmitInstInt4(envPtr, curr_bb, tblind,
			       l->offset, 0);
#if 0
		fprintf(stderr, "forward ref to %s, prev at %d, link %d\n",
			operand1, l->offset, here);
#endif
		l->offset = here;
	    }   
                    
	    /* Start a new basic block at the instruction following the jump */

	    curr_bb = 
		StartBasicBlock(envPtr, &BBHash, curr_bb,
				talInstructionTable[tblind].operandsConsumed,
				ind+1, operand1);

	    break;
                             
	case ASSEM_LVT:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "varname");
		goto cleanup;
	    }
	    if (CheckNamespaceQualifiers(interp, operand1)) {
		goto cleanup;
	    }
	    localVar = TclFindCompiledLocal(operand1, operand1Len, 1, envPtr);
	    if (localVar == -1) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("cannot use this instruction"
						  " in non-proc context", -1));
		Tcl_SetErrorCode(interp, "TCL", "ASSEM", "LVT", NULL);
		goto cleanup;
	    }
	    fprintf(stderr, "operand %s in slot %d\n", operand1, localVar);
	    BBEmitInst1or4(envPtr, curr_bb, tblind, localVar, 0);
	    break;

	case ASSEM_LVT1:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "varname");
		goto cleanup;
	    }
	    if (CheckNamespaceQualifiers(interp, operand1)) {
		goto cleanup;
	    }
	    localVar = TclFindCompiledLocal(operand1, operand1Len, 1, envPtr);
	    if (localVar == -1) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("cannot use this instruction"
						  " in non-proc context", -1));
		Tcl_SetErrorCode(interp, "TCL", "ASSEM", "LVT", NULL);
		goto cleanup;
	    }
	    if (CheckOneByte(interp, localVar)) {
		goto cleanup;
	    }
	    BBEmitInstInt1(envPtr, curr_bb, tblind, localVar, 0);
	    break;

	case ASSEM_LVT4:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "varname");
		goto cleanup;
	    }
	    if (CheckNamespaceQualifiers(interp, operand1)) {
		goto cleanup;
	    }
	    localVar = TclFindCompiledLocal(operand1, operand1Len, 1, envPtr);
	    if (localVar == -1) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("cannot use this instruction"
						  " in non-proc context", -1));
		Tcl_SetErrorCode(interp, "TCL", "ASSEM", "LVT", NULL);
		goto cleanup;
	    }
	    BBEmitInstInt4(envPtr, curr_bb, tblind, localVar, 0);
	    break;

	case ASSEM_BOOL:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "boolean");
		goto cleanup;
	    }
	    if (Tcl_GetBooleanFromObj(interp, bcArgs[1], &opnd) != TCL_OK) {
		goto cleanup;
	    }
	    BBEmitInstInt1(envPtr, curr_bb, tblind, opnd, 0);
	    break;

	case ASSEM_BOOL_LVT4:
	    if (bcSize != 3) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "boolean varName");
		goto cleanup;
	    }
	    if (Tcl_GetBooleanFromObj(interp, bcArgs[1], &opnd) != TCL_OK) {
		goto cleanup;
	    }
	    operand1 = Tcl_GetStringFromObj(bcArgs[2], &operand1Len);
	    if (CheckNamespaceQualifiers(interp, operand1)) {
		goto cleanup;
	    }
	    localVar = TclFindCompiledLocal(operand1, operand1Len, 1, envPtr);
	    if (localVar == -1) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("cannot use this instruction"
						  " in non-proc context", -1));
		Tcl_SetErrorCode(interp, "TCL", "ASSEM", "LVT", NULL);
		goto cleanup;
	    }
	    BBEmitInstInt1(envPtr, curr_bb, tblind, opnd, 0);
	    TclEmitInt4(localVar, envPtr);
	    break;

        case ASSEM_LVT1_SINT1:
	    if (bcSize != 3) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "varName imm8");
		goto cleanup;
	    }
	    if (CheckNamespaceQualifiers(interp, operand1)) {
		goto cleanup;
	    }
	    localVar = TclFindCompiledLocal(operand1, operand1Len, 1, envPtr);
	    if (localVar == -1) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("cannot use this instruction"
						  " in non-proc context", -1));
		Tcl_SetErrorCode(interp, "TCL", "ASSEM", "LVT", NULL);
		goto cleanup;
	    }
	    if (CheckOneByte(interp, localVar)) {
		goto cleanup;
	    }
	    if (Tcl_GetIntFromObj(interp, bcArgs[2], &opnd) != TCL_OK) {
		goto cleanup;
	    }
	    if (CheckSignedOneByte(interp, opnd)) {
		goto cleanup;
	    }
	    BBEmitInstInt1(envPtr, curr_bb, tblind, localVar, 0);
	    TclEmitInt1(opnd, envPtr);
	    break;

	case ASSEM_OVER:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "count");
		goto cleanup;
	    }
	    if (Tcl_GetIntFromObj(interp, bcArgs[1], &opnd) != TCL_OK) {
		goto cleanup;
	    }
	    BBEmitInstInt4(envPtr, curr_bb, tblind, opnd, opnd+1);
	    break;

	case ASSEM_PUSH:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "value");
		goto cleanup;
	    }
	    litIndex = TclRegisterNewLiteral(envPtr, operand1, operand1Len);
	    BBEmitInst1or4(envPtr, curr_bb, tblind, litIndex, 0);
	    break;

	case ASSEM_REVERSE:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "count");
		goto cleanup;
	    }
	    if (Tcl_GetIntFromObj(interp, bcArgs[1], &opnd) != TCL_OK) {
		goto cleanup;
	    }
	    BBEmitInstInt4(envPtr, curr_bb, tblind, opnd, opnd);
	    break;
		 
        case ASSEM_SINT1:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "imm8");
		goto cleanup;
	    }
	    if (Tcl_GetIntFromObj(interp, bcArgs[1], &opnd) != TCL_OK) {
		goto cleanup;
	    }
	    if (CheckSignedOneByte(interp, opnd)) {
		goto cleanup;
	    }
	    BBEmitInstInt1(envPtr, curr_bb, tblind, opnd, 0);
	    break;

        case ASSEM_CONCAT1:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "imm8");
		goto cleanup;
	    }
	    if (Tcl_GetIntFromObj(interp, bcArgs[1], &opnd) != TCL_OK) {
		goto cleanup;
	    }
	    if (CheckOneByte(interp, opnd)) {
		goto cleanup;
	    }
	    BBEmitInstInt1(envPtr, curr_bb, tblind, opnd, opnd);
	    break;

	case ASSEM_EVAL:
	    if (bcSize != 2) {
		Tcl_WrongNumArgs(interp, 1, bcArgs, "script");
		goto cleanup;
	    }
	    fprintf(stderr, "compiling: %s\n", operand1); fflush(stderr);
	    savedMaxStackDepth = envPtr->maxStackDepth;
	    savedCurrStackDepth = envPtr->currStackDepth;
	    envPtr->maxStackDepth = 0;
	    envPtr->currStackDepth = 0;
	    TclCompileScript(interp, operand1, operand1Len, envPtr);
	    if (curr_bb->finalStackDepth + envPtr->maxStackDepth
		> curr_bb->maxStackDepth) {
		curr_bb->maxStackDepth =
		    curr_bb->finalStackDepth + envPtr->maxStackDepth;
	    }
	    curr_bb->finalStackDepth += envPtr->currStackDepth;
	    envPtr->maxStackDepth = savedMaxStackDepth;
	    envPtr->currStackDepth = savedCurrStackDepth;
	    fprintf(stderr, "compilation returns\n"); fflush(stderr);
	    break;

	default:
	    Tcl_Panic("Instruction \"%s\" could not be found, can't happen\n",
		      instName);
	}
            
    }

    /* Tie off the last basic block */

    curr_bb->may_fall_thru = 0;
    curr_bb->jumpTargetLabelHashEntry = NULL;
    result = CheckStack(interp, envPtr, head_bb, bcList);
    if (result != TCL_OK) {
	goto cleanup;
    }
    if (curr_bb->visited) {
	int depth = curr_bb->finalStackDepth + curr_bb->initialStackDepth;
	if (depth == 0) {
	    /* Emit a 'push' of the empty literal */
	    litIndex = TclRegisterNewLiteral(envPtr, "", 0);
	    /* Assumes that 'push' is at slot 0 in talInstructionTable */
	    BBEmitInst1or4(envPtr, curr_bb, 0, litIndex, 0);
	    ++depth;
	}
	if (depth != 1) {
	    Tcl_Obj* depthObj = Tcl_NewIntObj(depth);
	    Tcl_IncrRefCount(depthObj);
	    resultObj = Tcl_NewStringObj("stack is unbalanced on exit "
					 "from the code (depth=", -1);
	    Tcl_AppendObjToObj(resultObj, depthObj);
	    Tcl_DecrRefCount(depthObj);
	    Tcl_AppendToObj(resultObj, ")", -1);
	    Tcl_SetObjResult(interp, resultObj);
	    goto cleanup;
	}
#if 0
	fprintf(stderr, "before: stackDepth %d\n", envPtr->currStackDepth);
#endif
	envPtr->currStackDepth += depth;
#if 0
	fprintf(stderr, "after: stackDepth %d\n", envPtr->currStackDepth);
#endif
	fflush(stderr);
    }

    Tcl_DeleteHashTable(&labelHash); // Actually, we need to free each label as well.

    return result;

 cleanup:
    
    /* FIXME: Need to make sure that allocated memory gets freed. */

    if (ind >= 0 && ind < bcSize) {
	Tcl_AddErrorInfo(interp, "\n    processing ");
	AddInstructionToErrorInfo(interp, bcList, ind);
    }

    /* TODO: If ind != -1, add error info indicating where in the
     * instruction stream things went wrong */

    return TCL_ERROR;

}

/*
 *-----------------------------------------------------------------------------
 *
 * CheckNamespaceQualifiers --
 *
 *	Verify that a variable name has no namespace qualifiers before
 *	attempting to install it in the LVT.
 *
 * Results:
 *	On success, returns TCL_OK. On failure, returns TCL_ERROR and
 *	stores an error message in the interpreter result.
 *
 *-----------------------------------------------------------------------------
 */

static int
CheckNamespaceQualifiers(Tcl_Interp* interp,
				/* Tcl interpreter for error reporting */
			 const char* name)
				/* Variable name to check */
{
    Tcl_Obj* result;		/* Error message */
    const char* p;
    for (p = name; *p;  p++) {      
	if ((*p == ':') && (p[1] == ':')) {
	    result = Tcl_NewStringObj("variable \"", -1);
	    Tcl_AppendToObj(result, name, -1);
	    Tcl_AppendToObj(result, "\" is not local", -1);
	    Tcl_SetObjResult(interp, result);
	    Tcl_SetErrorCode(interp, "TCL", "ASSEMBLE", "NONLOCAL", name,
			     NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * CheckOneByte --
 *
 *	Verify that a constant fits in a single byte in the instruction stream.
 *
 * Results:
 *	On success, returns TCL_OK. On failure, returns TCL_ERROR and
 *	stores an error message in the interpreter result.
 *
 * This code is here primarily to verify that instructions like INCR_SCALAR1
 * are possible on a given local variable. The fact that there is no
 * INCR_SCALAR4 is puzzling.
 *
 *-----------------------------------------------------------------------------
 */

static int
CheckOneByte(Tcl_Interp* interp,
				/* Tcl interpreter for error reporting */
	     int value)		/* Value to check */
{
    Tcl_Obj* result;		/* Error message */
    if (value < 0 || value > 0xff) {
	result = Tcl_NewStringObj("operand does not fit in 1 byte", -1);
	Tcl_SetObjResult(interp, result);
	Tcl_SetErrorCode(interp, "TCL", "ASSEMBLE", "1BYTE", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * CheckSignedOneByte --
 *
 *	Verify that a constant fits in a single signed byte in the instruction
 *      stream.
 *
 * Results:
 *	On success, returns TCL_OK. On failure, returns TCL_ERROR and
 *	stores an error message in the interpreter result.
 *
 * This code is here primarily to verify that instructions like INCR_SCALAR1
 * are possible on a given local variable. The fact that there is no
 * INCR_SCALAR4 is puzzling.
 *
 *-----------------------------------------------------------------------------
 */

static int
CheckSignedOneByte(Tcl_Interp* interp,
				/* Tcl interpreter for error reporting */
	           int value)	/* Value to check */
{
    Tcl_Obj* result;		/* Error message */
    if (value > 0x7f || value < -0x80) {
	result = Tcl_NewStringObj("operand does not fit in 1 byte", -1);
	Tcl_SetObjResult(interp, result);
	Tcl_SetErrorCode(interp, "TCL", "ASSEMBLE", "1BYTE", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * DefineLabel --
 *
 *	Defines a label appearing in the assembly sequence.
 *
 * Results:
 *	Returns a standard Tcl result. Returns TCL_OK and an empty result
 *	if the definition succeeds; returns TCL_ERROR and an appropriate
 *	message if a duplicate definition is found.
 *
 *-----------------------------------------------------------------------------
 */

static int
DefineLabel(Tcl_Interp* interp, 	/* Tcl interpreter */
	    CompileEnv* envPtr,		/* Compilation environment */
	    const char* labelName,	/* Label being defined */
	    Tcl_HashTable* labelHash)	/* Symbol table */
{
    Tcl_HashEntry* entry;	/* Label's entry in the symbol table */
    int isNew;			/* Flag == 1 iff the label was previously
				 * undefined */
    label* l;			/*  */
    Tcl_Obj* result;

    /* Look up the newly-defined label in the symbol table */

    entry = Tcl_CreateHashEntry(labelHash, labelName, &isNew);
    if (isNew) {

	/* This is the first appearance of the label in the code */

	l = (label *)ckalloc(sizeof(label));
	l->isDefined = 1;
	l->offset = envPtr->codeNext - envPtr->codeStart; 
	Tcl_SetHashValue(entry, l);

    } else {

	/* The label has appeared earlier. Make sure that it's not defined. */

	l = (label *) Tcl_GetHashValue(entry);
	if (l->isDefined) {
	    result = Tcl_NewStringObj("duplicate definition of label \"", -1);
	    Tcl_AppendToObj(result, labelName, -1);
	    Tcl_AppendToObj(result, "\"", -1);
	    Tcl_SetObjResult(interp, result);
	    return TCL_ERROR;
	} else {

	    /* 
	     * Walk the linked list of previous references to the label 
	     * and fix them up. 
	     */

	    int jump = l->offset;
	    while (jump >= 0) {
		int prevJump = TclGetInt4AtPtr(envPtr->codeStart + jump + 1);
#if 0
		fprintf(stderr, "fixup jump at %d to refer to %d\n",
			jump, envPtr->codeNext - envPtr->codeStart);
#endif
		TclStoreInt4AtPtr(envPtr->codeNext - envPtr->codeStart - jump,
				  envPtr->codeStart + jump + 1);
		jump = prevJump;
	    }
	    l->offset = envPtr->codeNext - envPtr->codeStart;
	    l->isDefined = 1;
	}   
    }
    return TCL_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * StartBasicBlock --
 *
 *	Starts a new basic block when a label or jump is encountered.
 *
 * Results:
 *	Returns a pointer to the BasicBlock structure of the new
 *	basic block.
 *
 *-----------------------------------------------------------------------------
 */

static BasicBlock*
StartBasicBlock(CompileEnv* envPtr, 
				/* Compilation environment */
		Tcl_HashTable* BBHashPtr,
				/* Hash table where basic blocks are recorded */
		BasicBlock* currBB,
				/* Pointer to the BasicBlock structure
				 * of the block being closed. */
		int fallsThrough,
				/* 1 if execution falls through into
				 * the following block, 0 otherwise */
		int bcIndex,	/* Index of the current insn in the
				 * assembly stream */
		const char* jumpLabel)
				/* Label of the location that the
				 * block jumps to, or NULL if the block
				 * does not jump */
{
    int isNew;		 /* Unused return from Tcl_CreateHashEntry */
    BasicBlock* newBB;	 /* BasicBlock structure for the new basic block */

    /* Coalesce zero-length blocks */

    if (currBB->start == envPtr->codeNext) {
	return currBB;
    }

    /* Make the new basic block */

    newBB = AllocBB(envPtr, bcIndex);

    /* Record the jump target if there is one. */

    if (jumpLabel) {
	currBB->jumpTargetLabelHashEntry =
	    Tcl_CreateHashEntry(BBHashPtr, jumpLabel, &isNew);
    } else {
	currBB->jumpTargetLabelHashEntry = NULL;
    }

    /* Record the fallthrough if there is one. */

    currBB->may_fall_thru = fallsThrough;

    /* Record the successor block */

    currBB->successor1 = newBB;
    return newBB;
}

/*
 *-----------------------------------------------------------------------------
 *
 * AllocBB --
 *
 *	Allocates a new basic block
 *
 * Results:
 *	Returns a pointer to the newly allocated block, which is initialized
 *	to contain no code and begin at the current instruction pointer.
 *
 *-----------------------------------------------------------------------------
 */

static BasicBlock * 
AllocBB(CompileEnv* envPtr,	/* Compile environment containing the
				 * current instruction pointer */
	int bcIndex)		/* Current index in the list of
				 * assembly instructions */
{
    BasicBlock * bb = (BasicBlock *) ckalloc(sizeof(BasicBlock));

    bb->start = envPtr->codeNext;
    bb->bcIndex = bcIndex;
    bb->initialStackDepth = 0;
    bb->minStackDepth = 0;
    bb->maxStackDepth = 0;
    bb->finalStackDepth = 0;

    bb->visited = 0;

    bb->predecessor = NULL;
    bb->jumpTargetLabelHashEntry = NULL;
    bb->successor1 = NULL;

    return bb;
}

static int
CheckStack(Tcl_Interp* interp,
	   CompileEnv* envPtr,
	   BasicBlock * head,
	   Tcl_Obj* bcList) {
    StackCheckerState st;
    st.interp = interp;
    st.maxDepth = 0;
    st.envPtr = envPtr;
    st.bcList = bcList;
    if(StackCheckBasicBlock(&st, head, NULL, 0) == TCL_ERROR) {
        return TCL_ERROR;
    }
#if 0
    fprintf(stderr, "Max stack anywhere is %d\n", st->maxDepth);
#endif
    if (st.maxDepth + envPtr->currStackDepth > envPtr->maxStackDepth) {
	envPtr->maxStackDepth = st.maxDepth + envPtr->currStackDepth;
    }
    return TCL_OK;
}

static int
StackCheckBasicBlock(StackCheckerState *st, BasicBlock * blockPtr, BasicBlock * predecessor, int initialStackDepth) { 
#if 0
    CompileEnv* envPtr = st->envPtr;
    fprintf(stderr, "stack check basic block %p at depth %d\n", 
	    blockPtr, initialStackDepth);
    fprintf(stderr, "  start %d may_fall_thru %d visited %d\n",
	    blockPtr->start - envPtr->codeStart,
	    blockPtr->may_fall_thru, blockPtr->visited);
    fprintf(stderr, "  predecessor %p successor1 %p\n", 
	    blockPtr->predecessor, blockPtr->successor1);
    fprintf(stderr, "  stack: init %d min %d max %d final %d\n",
	    blockPtr->initialStackDepth, blockPtr->minStackDepth,
	    blockPtr->maxStackDepth, blockPtr->finalStackDepth);
    fflush(stderr);
#endif    
    if (blockPtr->visited) {
        if (blockPtr->initialStackDepth != initialStackDepth) {
	    Tcl_SetObjResult(st->interp, Tcl_NewStringObj("inconsistent stack depths on two execution paths", -1));
            /* Trace the offending BasicBlock */
	    Tcl_AddErrorInfo(st->interp, "\n    to ");
	    AddInstructionToErrorInfo(st->interp, st->bcList, 
				      blockPtr->bcIndex);
	    /* TODO - add execution trace of both paths */
            return TCL_ERROR;
        } else {
            return TCL_OK;
        }
    } else {

        blockPtr->visited = 1;
        blockPtr->predecessor = predecessor;
        blockPtr->initialStackDepth = initialStackDepth;
        if (initialStackDepth + blockPtr->minStackDepth < 0) {
	    Tcl_SetObjResult(st->interp,
			     Tcl_NewStringObj("stack underflow", -1));
	    AddBasicBlockRangeToErrorInfo(st->interp, st->bcList, blockPtr);
            return TCL_ERROR;
        }
        if (initialStackDepth + blockPtr->maxStackDepth > st->maxDepth) {
            st->maxDepth = initialStackDepth + blockPtr->maxStackDepth;
        }
    }
    int stackDepth = initialStackDepth + blockPtr->finalStackDepth;
    int result = TCL_OK;
#if 0
    fprintf(stderr, "on exit from block, depth will be %d\n", stackDepth);
    fflush(stderr);
#endif
    if (blockPtr->may_fall_thru) {
        result = StackCheckBasicBlock(st, blockPtr->successor1, blockPtr, stackDepth);        
        
    }
    /* FIXME Have we checked for undefined labels yet ? */
    if (result == TCL_OK && blockPtr->jumpTargetLabelHashEntry != NULL) {
        BasicBlock * targetBlock = (BasicBlock *) Tcl_GetHashValue(blockPtr->jumpTargetLabelHashEntry);
        result = StackCheckBasicBlock(st, targetBlock, blockPtr, stackDepth);
    }
    return result;
    
}

/*
 *----------------------------------------------------------------------
 *
 * FreeAssembleCodeInternalRep --
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
FreeAssembleCodeInternalRep(
    Tcl_Obj *objPtr)
{
    ByteCode *codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;

    codePtr->refCount--;
    if (codePtr->refCount <= 0) {
	TclCleanupByteCode(codePtr);
    }
    objPtr->typePtr = NULL;
    objPtr->internalRep.otherValuePtr = NULL;
}

static void
AddBasicBlockRangeToErrorInfo(Tcl_Interp* interp,
			      Tcl_Obj* bcList,
			      BasicBlock* bbPtr)
{
    Tcl_AddErrorInfo(interp, "\n    between ");
    AddInstructionToErrorInfo(interp, bcList, bbPtr->bcIndex);
    Tcl_AddErrorInfo(interp, "\n    and ");
    if (bbPtr->successor1 != NULL) {
	AddInstructionToErrorInfo(interp, bcList,
				  bbPtr->successor1->bcIndex);
    } else {
	Tcl_AddErrorInfo(interp, "end of assembly code");
    }
}

static void
AddInstructionToErrorInfo(Tcl_Interp* interp,
			  Tcl_Obj* bcList,
			  int bcIndex)
{
    Tcl_Obj* msgObj;
    int msgLen;
    const char* msgPtr;

    Tcl_AddErrorInfo(interp, "source instruction at list index ");
    msgObj = Tcl_NewIntObj(bcIndex);
    Tcl_IncrRefCount(msgObj);
    msgPtr = Tcl_GetStringFromObj(msgObj, &msgLen);
    Tcl_AddObjErrorInfo(interp, msgPtr, msgLen);
    Tcl_DecrRefCount(msgObj);
    Tcl_AddErrorInfo(interp, " (\"");
    Tcl_ListObjIndex(NULL, bcList, bcIndex, &msgObj);
    msgPtr = Tcl_GetStringFromObj(msgObj, &msgLen);
    Tcl_AddObjErrorInfo(interp, msgPtr, msgLen);
    Tcl_AddErrorInfo(interp, "\")");
}	    
	    
