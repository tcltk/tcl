#include "tclInt.h"
#include "tclCompile.h"
#include "tclAssembly.h"
#include "tclOOInt.h"


/* Structure that holds the state of the assembler while generating code */

typedef struct AssembleEnv {
    CompileEnv* envPtr;		/* Compilation environment being used
				 * for code generation */
    Tcl_Parse* parsePtr;        /* Parse of the current line of source */
    Tcl_HashTable labelHash;	/* Hash table whose keys are labels and
				 * whose values are 'label' objects storing 
				 * the code offsets of the labels. */

    int cmdLine;		/* Current line number within the assembly 
				 * code */
    int* clNext;		/* Invisible continuation line for
				 * [info frame] */

    /* TODO: Next table could be a simple list keyed off memory address?
     * Or the BB pointers could be in the 'label' struct */
    Tcl_HashTable BBHash;	/* Hash table whose keys are labels and
				 * whose values are BasicBlock structure 
				 * pointers for the basic blocks at those
				 * locations */
    BasicBlock* head_bb;	/* First basic block in the code */
    BasicBlock* curr_bb;	/* Current basic block */

    int maxDepth;	     	/* Maximum stack depth encountered */
    int flags;			/* Compilation flags (TCL_EVAL_DIRECT) */
} AssembleEnv;

/* Static functions defined in this file */

static void AddBasicBlockRangeToErrorInfo(AssembleEnv*, BasicBlock*);
static void AddInstructionToErrorInfo(Tcl_Interp* interp, Tcl_Obj* bcList,
				      int index);
static BasicBlock * AllocBB(AssembleEnv*); 
static int AssembleOneLine(AssembleEnv* envPtr);
static int CheckNamespaceQualifiers(Tcl_Interp*, const char*, int);
static int CheckOneByte(Tcl_Interp*, int);
static int CheckSignedOneByte(Tcl_Interp*, int);
static int CheckStack(AssembleEnv*);
static ByteCode * CompileAssembleObj(Tcl_Interp *interp, Tcl_Obj *objPtr);
static int DefineLabel(AssembleEnv* envPtr, const char* label);
static int FindLocalVar(AssembleEnv* envPtr, Tcl_Token** tokenPtrPtr);
static int FinishAssembly(AssembleEnv*);
static void FreeAssembleCodeInternalRep(Tcl_Obj *objPtr);
static void FreeAssembleEnv(AssembleEnv*);
static int GetBooleanOperand(AssembleEnv*, Tcl_Token**, int*);
static int GetIntegerOperand(AssembleEnv*, Tcl_Token**, int*);
static int GetNextOperand(AssembleEnv*, Tcl_Token**, Tcl_Obj**);
static AssembleEnv* NewAssembleEnv(CompileEnv*, int);
static int StackCheckBasicBlock(AssembleEnv*, BasicBlock *, BasicBlock *, int);
static BasicBlock* StartBasicBlock(AssembleEnv*, int fallthrough,
				   const char* jumpLabel);
/* static int AdvanceIp(const unsigned char *pc); */
static int StackCheckBasicBlock(AssembleEnv*, BasicBlock *, BasicBlock *, int);
static void SyncStackDepth(AssembleEnv*);

/* Tcl_ObjType that describes bytecode emitted by the assembler */

static const Tcl_ObjType assembleCodeType = {
    "assemblecode",
    FreeAssembleCodeInternalRep, /* freeIntRepProc */
    NULL,			 /* dupIntRepProc */
    NULL,			 /* updateStringProc */
    NULL			 /* setFromAnyProc */
};

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
    {"eval",	ASSEM_EVAL,	INST_EVAL_STK,	0,	1},
    {"evalStk",	ASSEM_1BYTE,	INST_EVAL_STK,	1,	1},
    {"expr",	ASSEM_EVAL,	INST_EXPR_STK,	0,	1},
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
BBEmitOpcode(AssembleEnv* assemEnvPtr,
				/* Assembly environment */
	     int tblind,	/* Table index in talInstructionTable of op */
	     int count)		/* Operand count for variadic ops */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    BasicBlock* bbPtr = assemEnvPtr->curr_bb;
				/* Current basic block */
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
BBEmitInstInt1(AssembleEnv* assemEnvPtr,
				/* Assembly environment */
	       int tblind,	/* Index in talInstructionTable of op */
	       unsigned char opnd,
				/* 1-byte operand */
	       int count)	/* Operand count for variadic ops */
{
    BBEmitOpcode(assemEnvPtr, tblind, count);
    TclEmitInt1(opnd, assemEnvPtr->envPtr);
}
static void
BBEmitInstInt4(AssembleEnv* assemEnvPtr,
				/* Assembly environment */
	       int tblind,	/* Index in talInstructionTable of op */
	       int opnd,	/* 4-byte operand */
	       int count)	/* Operand count for variadic ops */
{
    BBEmitOpcode(assemEnvPtr, tblind, count);
    TclEmitInt4(opnd, assemEnvPtr->envPtr);
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
BBEmitInst1or4(AssembleEnv* assemEnvPtr,
				/* Assembly environment */
	       int tblind,	/* Index in talInstructionTable of op */
	       int param,	/* Variable-length parameter */
	       int count)	/* Arity if variadic */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    BasicBlock* bbPtr = assemEnvPtr->curr_bb;
				/* Current basic block */

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

/*
 *-----------------------------------------------------------------------------
 *
 * Tcl_AssembleObjCmd, TclNRAssembleObjCmd --
 *
 *	Direct evaluation path for tcl::unsupported::assemble
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Assembles the code in objv[1], and executes it, so side effects
 *	include whatever the code does.
 *
 *-----------------------------------------------------------------------------
 */

int
Tcl_AssembleObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    /* 
     * Boilerplate - make sure that there is an NRE trampoline on the
     * C stack because there needs to be one in place to execute bytecode.
     */
        
    return Tcl_NRCallObjProc(interp, TclNRAssembleObjCmd, dummy, objc, objv);
}
int
TclNRAssembleObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    ByteCode *codePtr;		/* Pointer to the bytecode to execute */
    Tcl_Obj* backtrace;		/* Object where extra error information
				 * is constructed. */

    /* Check args */

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "bytecodeList");
	return TCL_ERROR;
    }

    /* Assemble the source to bytecode */

    codePtr = CompileAssembleObj(interp, objv[1]);

    /* On failure, report error line */

    if (codePtr == NULL) {
	Tcl_AddErrorInfo(interp, "\n    (\"");
	Tcl_AddErrorInfo(interp, Tcl_GetString(objv[0]));
	Tcl_AddErrorInfo(interp, "\" body, line ");
	backtrace = Tcl_NewIntObj(Tcl_GetErrorLine(interp));
	Tcl_IncrRefCount(backtrace);
	Tcl_AddErrorInfo(interp, Tcl_GetString(backtrace));
	Tcl_DecrRefCount(backtrace);
	Tcl_AddErrorInfo(interp, ")");
	return TCL_ERROR;
    }

    /* Use NRE to evaluate the bytecode from the trampoline */

    Tcl_NRAddCallback(interp, NRCallTEBC, INT2PTR(TCL_NR_BC_TYPE), codePtr,
	    NULL, NULL);

    return TCL_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * CompileAssembleObj --
 *
 *	Sets up and assembles Tcl bytecode for the direct-execution path
 *	in the Tcl bytecode assembler.
 *
 * Results:
 *	Returns a pointer to the assembled code. Returns NULL if the
 *	assembly fails for any reason, with an appropriate error message
 *	in the interpreter.
 *
 *-----------------------------------------------------------------------------
 */
 
static ByteCode *
CompileAssembleObj(
    Tcl_Interp *interp,		/* Tcl interpreter */
    Tcl_Obj *objPtr)		/* Source code to assemble */
{
    Interp *iPtr = (Interp *) interp;
				/* Internals of the interpreter */
    CompileEnv compEnv;		/* Compilation environment structure */
    register ByteCode *codePtr = NULL;
				/* Bytecode resulting from the assembly */
    Namespace* namespacePtr;	/* Namespace in which variable and
				 * command names in the bytecode resolve */
    int status;			/* Status return from Tcl_AssembleCode */
    const char* source;		/* String representation of the
				 * source code */
    int sourceLen;			/* Length of the source code in bytes */

    /*
     * Get the expression ByteCode from the object. If it exists, make sure it
     * is valid in the current context.
     */
     
    if (objPtr->typePtr == &assembleCodeType) {
	namespacePtr = iPtr->varFramePtr->nsPtr;
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

	/* Set up the compilation environment, and assemble the code */

	source = TclGetStringFromObj(objPtr, &sourceLen);
	TclInitCompileEnv(interp, &compEnv, source, sourceLen, NULL, 0);
	status = TclAssembleCode(&compEnv, source, sourceLen, TCL_EVAL_DIRECT);
	if (status != TCL_OK) {

	    /* Assembly failed. Clean up and report the error */

	    TclFreeCompileEnv(&compEnv);
	    return NULL;
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

	/*
	 * Record the local variable context to which the bytecode pertains
	 */

	codePtr = (ByteCode *) objPtr->internalRep.otherValuePtr;
	if (iPtr->varFramePtr->localCachePtr) {
	    codePtr->localCachePtr = iPtr->varFramePtr->localCachePtr;
	    codePtr->localCachePtr->refCount++;
	}

	/* Report on what the assembler did. */

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

    status = TclAssembleCode(envPtr, tokenPtr[1].start, tokenPtr[1].size, 0);
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
TclAssembleCode(CompileEnv *envPtr, 	
				/* Compilation environment that is to
				 * receive the generated bytecode */
		const char* codePtr,
				/* Assembly-language code to be processed */
		int codeLen,	/* Length of the code */
		int flags)	/* OR'ed combination of flags */
{
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */
    /* 
     * Walk through the assembly script using the Tcl parser.
     * Each 'command' will be an instruction or assembly directive.
     */

    const char* instPtr = codePtr;
				/* Where to start looking for a line of code */
    int instLen;		/* Length in bytes of the current line of 
				 * code */
    const char* nextPtr;	/* Pointer to the end of the line of code */
    int bytesLeft = codeLen;	/* Number of bytes of source code remaining 
				 * to be parsed */
    int status;			/* Tcl status return */

    AssembleEnv* assemEnvPtr = NewAssembleEnv(envPtr, flags);
    Tcl_Parse* parsePtr = assemEnvPtr->parsePtr;

    do {

	/* Parse out one command line from the assembly script */

	status = Tcl_ParseCommand(interp, instPtr, bytesLeft, 0, parsePtr);
	instLen = parsePtr->commandSize;
	if (parsePtr->term == parsePtr->commandStart + instLen - 1) {
	    --instLen;
	}

	/* Report errors in the parse */

	if (status != TCL_OK) {
	    if (flags & TCL_EVAL_DIRECT) {
		Tcl_LogCommandInfo(interp, codePtr, parsePtr->commandStart, 
				   instLen);
	    }
	    FreeAssembleEnv(assemEnvPtr);
	    return TCL_ERROR;
	}

	/* Advance the pointers around any leading commentary */

	TclAdvanceLines(&assemEnvPtr->cmdLine, instPtr, parsePtr->commandStart);
	TclAdvanceContinuations(&assemEnvPtr->cmdLine, &assemEnvPtr->clNext, 
				parsePtr->commandStart - envPtr->source);

	/* Process the line of code  */

	if (parsePtr->numWords > 0) {

	    /* If tracing, show each line assembled as it happens */

#ifdef TCL_COMPILE_DEBUG
	    if ((tclTraceCompile >= 1) && (envPtr->procPtr == NULL)) {
		printf("  Assembling: ");
		TclPrintSource(stdout, parsePtr->commandStart,
			       TclMin(instLen, 55));
		printf("\n");
	    }
#endif
	    if (AssembleOneLine(assemEnvPtr) != TCL_OK) {
		if (flags & TCL_EVAL_DIRECT) {
		    Tcl_LogCommandInfo(interp, codePtr, parsePtr->commandStart, 
				       instLen);
		}
		Tcl_FreeParse(parsePtr);
		FreeAssembleEnv(assemEnvPtr);
		return TCL_ERROR;
	    }
	}

	/* Advance to the next line of code */

	nextPtr = parsePtr->commandStart + parsePtr->commandSize;
	bytesLeft -= (nextPtr - instPtr);
	instPtr = nextPtr;
	TclAdvanceLines(&assemEnvPtr->cmdLine, parsePtr->commandStart, instPtr);
	TclAdvanceContinuations(&assemEnvPtr->cmdLine, &assemEnvPtr->clNext,
				instPtr - envPtr->source);
	Tcl_FreeParse(parsePtr);
    } while (bytesLeft > 0);

    /* Done with parsing the code */

    status = FinishAssembly(assemEnvPtr);
    FreeAssembleEnv(assemEnvPtr);
    return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * NewAssembleEnv --
 *
 *	Creates an environment for the assembler to run in.
 *
 * Results:
 *	Allocates, initialises and returns an assembler environment
 *
 *-----------------------------------------------------------------------------
 */

static AssembleEnv*
NewAssembleEnv(CompileEnv* envPtr,
				/* Compilation environment being used
				 * for code generation*/
	       int flags)	/* Compilation flags (TCL_EVAL_DIRECT) */
{
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */
    AssembleEnv* assemEnvPtr = TclStackAlloc(interp, sizeof(AssembleEnv));
				/* Assembler environment under construction */
    Tcl_Parse* parsePtr = TclStackAlloc(interp, sizeof(Tcl_Parse));
				/* Parse of one line of assembly code */

    assemEnvPtr->envPtr = envPtr;
    assemEnvPtr->parsePtr = parsePtr;
    assemEnvPtr->cmdLine = envPtr->line;
    assemEnvPtr->clNext = envPtr->clNext;

    /* Make the hashtables that store symbol resolution */

    Tcl_InitHashTable(&assemEnvPtr->labelHash, TCL_STRING_KEYS);
    Tcl_InitHashTable(&assemEnvPtr->BBHash, TCL_STRING_KEYS);

    /* Start the first basic block */

    assemEnvPtr->head_bb = AllocBB(assemEnvPtr);
    assemEnvPtr->curr_bb = assemEnvPtr->head_bb;

    /* Stash compilation flags */

    assemEnvPtr->flags = flags;

    return assemEnvPtr;
}

/*
 *-----------------------------------------------------------------------------
 *
 * FreeAssembleEnv --
 *
 *	Cleans up the assembler environment when assembly is complete.
 *
 *-----------------------------------------------------------------------------
 */

static void
FreeAssembleEnv(AssembleEnv* assemEnvPtr)
				/* Environment to free */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment being used
				 * for code generation */
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */

    BasicBlock* thisBB;		/* Pointer to a basic block being deleted */
    BasicBlock* nextBB;		/* Pointer to a deleted basic block's 
				 * successor */
    Tcl_HashEntry* hashEntry;
    Tcl_HashSearch hashSearch;
    label* labelPtr;

    /* Free the basic block hash index and all the basic block structures */
    Tcl_DeleteHashTable(&assemEnvPtr->BBHash);
    for (thisBB = assemEnvPtr->head_bb; thisBB != NULL; thisBB = nextBB) {
	nextBB = thisBB->successor1;
	ckfree((char*)thisBB);
    }

    /* Free all the labels */
    while ((hashEntry = Tcl_FirstHashEntry(&assemEnvPtr->labelHash,
					    &hashSearch)) != NULL) {
	labelPtr = (label*) Tcl_GetHashValue(hashEntry);
	ckfree((char*) labelPtr);
	Tcl_DeleteHashEntry(hashEntry);
    }
    Tcl_DeleteHashTable(&assemEnvPtr->labelHash);

    TclStackFree(interp, assemEnvPtr->parsePtr);
    TclStackFree(interp, assemEnvPtr);
}

/*
 *-----------------------------------------------------------------------------
 *
 * AssembleOneLine --
 *
 *	Assembles a single command from an assembly language source.
 *
 * Results:
 *	Returns TCL_ERROR with an appropriate error message if the
 *	assembly fails. Returns TCL_OK if the assembly succeeds. Updates
 *	the assembly environment with the state of the assembly.
 *
 *-----------------------------------------------------------------------------
 */

static int
AssembleOneLine(AssembleEnv* assemEnvPtr)
				/* State of the assembly */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment being used for
				 * code gen */
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */
    Tcl_Parse* parsePtr = assemEnvPtr->parsePtr;
				/* Parse of the line of code */
    Tcl_Token* tokenPtr;	/* Current token within the line of code */
    Tcl_Obj* instNameObj = NULL;
				/* Name of the instruction */
    int tblind;			/* Index in talInstructionTable of the 
				 * instruction */
    enum talInstType instType;	/* Type of the instruction */
    Tcl_Obj* operand1Obj = NULL;
    				/* First operand to the instruction */
    const char* operand1;	/* String rep of the operand */
    int operand1Len;		/* String length of the operand  */
    Tcl_HashEntry* entry;	/* Hash entry from label and basic
				 * block operations */
    int isNew;		    	/* Flag indicating that a new hash entry
				 * has been created */
    label* l;		        /* Structure descibing a label in the
				 * assembly code */
    int opnd;			/* Integer representation of an operand */
    int litIndex;		/* Literal pool index of a constant */
    int localVar;		/* LVT index of a local variable */
    int status = TCL_ERROR;	/* Return value from this function */
    
    /* Make sure that the instruction name is known at compile time. */

    tokenPtr = parsePtr->tokenPtr;
    instNameObj = Tcl_NewObj();
    Tcl_IncrRefCount(instNameObj);
    if (GetNextOperand(assemEnvPtr, &tokenPtr, &instNameObj) != TCL_OK) {
	return TCL_ERROR;
    }

    /* Look up the instruction name */
    
    if (Tcl_GetIndexFromObjStruct(interp, instNameObj,
				  &talInstructionTable[0].name,
				  sizeof(talInstDesc), "instruction",
				  TCL_EXACT, &tblind) != TCL_OK) {
	return TCL_ERROR;
    }

    /* Vector on the type of instruction being processed */

    instType = talInstructionTable[tblind].instType;
    switch (instType) {

    case ASSEM_PUSH:
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "value");
	    goto cleanup;
	}
	if (GetNextOperand(assemEnvPtr, &tokenPtr, &operand1Obj) != TCL_OK) {
	    goto cleanup;
	}
	operand1 = Tcl_GetStringFromObj(operand1Obj, &operand1Len);
	litIndex = TclRegisterNewLiteral(envPtr, operand1, operand1Len);
	BBEmitInst1or4(assemEnvPtr, tblind, litIndex, 0);
	break;

    case ASSEM_1BYTE:
	if (parsePtr->numWords != 1) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "");
	    goto cleanup;
	}
	BBEmitOpcode(assemEnvPtr, tblind, 0);
	break;

    case ASSEM_BOOL:
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "boolean");
	    goto cleanup;
	}
	if (GetBooleanOperand(assemEnvPtr, &tokenPtr, &opnd) != TCL_OK) {
	    goto cleanup;
	}
	BBEmitInstInt1(assemEnvPtr, tblind, opnd, 0);
	break;

    case ASSEM_BOOL_LVT4:
	if (parsePtr->numWords != 3) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "boolean varName");
	    goto cleanup;
	}
	if (GetBooleanOperand(assemEnvPtr, &tokenPtr, &opnd) != TCL_OK
	    || (localVar = FindLocalVar(assemEnvPtr, &tokenPtr)) < 0) {
	    goto cleanup;
	}
	BBEmitInstInt1(assemEnvPtr, tblind, opnd, 0);
	TclEmitInt4(localVar, envPtr);
	break;

    case ASSEM_CONCAT1:
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "imm8");
	    goto cleanup;
	}
	if (GetNextOperand(assemEnvPtr, &tokenPtr, &operand1Obj) != TCL_OK
	    || Tcl_GetIntFromObj(interp, operand1Obj, &opnd) != TCL_OK) {
	    goto cleanup;
	}
	if (CheckOneByte(interp, opnd)) {
	    goto cleanup;
	}
	BBEmitInstInt1(assemEnvPtr, tblind, opnd, opnd);
	break;

    case ASSEM_EVAL:
	/* TODO - Refactor this stuff into a subroutine
	 * that takes the inst code, the message ("script" or "expression")
	 * and an evaluator callback that calls TclCompileScript or
	 * TclCompileExpr. 
	 */
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, 
			     ((talInstructionTable[tblind].tclInstCode
			       == INST_EVAL_STK) ? "script" : "expression"));
	    goto cleanup;
	}
	if (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    /*
	     * The expression or script is not only known at compile time,
	     * but actually a "simple word". It can be compiled inline by
	     * invoking the compiler recursively.
	     */
	    int savedStackDepth = envPtr->currStackDepth;
	    int savedMaxStackDepth = envPtr->maxStackDepth;
	    envPtr->currStackDepth = 0;
	    envPtr->maxStackDepth = 0;
	    switch(talInstructionTable[tblind].tclInstCode) {
	    case INST_EVAL_STK:
		TclCompileScript(interp, tokenPtr[1].start,
				 tokenPtr[1].size, envPtr);
		break;
	    case INST_EXPR_STK:
		TclCompileExpr(interp, tokenPtr[1].start,
			       tokenPtr[1].size, envPtr, 1);
		break;
	    default:
		Tcl_Panic("no ASSEM_EVAL case for %s (%d), can't happen",
			  talInstructionTable[tblind].name,
			  talInstructionTable[tblind].tclInstCode);
	    }
	    SyncStackDepth(assemEnvPtr);
	    envPtr->currStackDepth = savedStackDepth;
	    envPtr->maxStackDepth = savedMaxStackDepth;
	} else if (GetNextOperand(assemEnvPtr, &tokenPtr, &operand1Obj)
		   != TCL_OK) {
	    goto cleanup;
	} else {
	    operand1 = Tcl_GetStringFromObj(operand1Obj, &operand1Len);
	    litIndex = TclRegisterNewLiteral(envPtr, operand1, operand1Len);
	    /* Assumes that PUSH is the first slot! */
	    BBEmitInst1or4(assemEnvPtr, 0, litIndex, 0);
	    BBEmitOpcode(assemEnvPtr, tblind, 0);
	}
	break;

    case ASSEM_INVOKE:
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "count");
	    goto cleanup;
	}
	if (GetNextOperand(assemEnvPtr, &tokenPtr, &operand1Obj) != TCL_OK
	    || Tcl_GetIntFromObj(interp, operand1Obj, &opnd) != TCL_OK) {
	    goto cleanup;
	}
	BBEmitInst1or4(assemEnvPtr, tblind, opnd, opnd);
	break;
		 
    case ASSEM_JUMP: 
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "label");
	    goto cleanup;
	}
	if (GetNextOperand(assemEnvPtr, &tokenPtr, &operand1Obj) != TCL_OK) {
	    goto cleanup;
	}
	entry = Tcl_CreateHashEntry(&assemEnvPtr->labelHash,
				    Tcl_GetString(operand1Obj), &isNew);
	if (isNew) {
	    l = (label *) ckalloc(sizeof(label));
	    l -> isDefined = 0;
	    l -> offset = -1;
	    Tcl_SetHashValue(entry, l);
	} else {
	    l = Tcl_GetHashValue(entry);
	}
	if (l -> isDefined) {
	    BBEmitInst1or4(assemEnvPtr, tblind, 
			   (l->offset + envPtr->codeStart
			    - envPtr->codeNext), 0);
	} else {
	    int here = envPtr->codeNext - envPtr->codeStart;
	    BBEmitInstInt4(assemEnvPtr, tblind, l->offset, 0);
	    l->offset = here;
	}   
                    
	/* Start a new basic block at the instruction following the jump */

	StartBasicBlock(assemEnvPtr,
			talInstructionTable[tblind].operandsConsumed,
			Tcl_GetString(operand1Obj));

	break;
                             
    case ASSEM_LABEL:

	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "name");
	    goto cleanup;
	}
	if (GetNextOperand(assemEnvPtr, &tokenPtr, &operand1Obj) != TCL_OK) {
	    goto cleanup;
	}
	/* Add the (label_name, address) pair to the hash table */
	if (DefineLabel(assemEnvPtr, Tcl_GetString(operand1Obj)) != TCL_OK) {
	    goto cleanup;
	}
	StartBasicBlock(assemEnvPtr, 1, NULL);

	/* Attach the label to the new basic block */

	/* TODO - do this in DefineLable and make the BB a field in the
	 * label struct */
	entry = Tcl_CreateHashEntry(&assemEnvPtr->BBHash,
				    Tcl_GetString(operand1Obj), &opnd);
	Tcl_SetHashValue(entry, assemEnvPtr->curr_bb);
	break;

    case ASSEM_LVT:
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "varname");
	    goto cleanup;
	}
	if ((localVar = FindLocalVar(assemEnvPtr, &tokenPtr)) < 0) {
	    goto cleanup;
	}
	BBEmitInst1or4(assemEnvPtr, tblind, localVar, 0);
	break;

    case ASSEM_LVT1:
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "varname");
	    goto cleanup;
	}
	if ((localVar = FindLocalVar(assemEnvPtr, &tokenPtr)) < 0
	    || CheckOneByte(interp, localVar)) {
	    goto cleanup;
	}
	BBEmitInstInt1(assemEnvPtr, tblind, localVar, 0);
	break;

    case ASSEM_LVT1_SINT1:
	if (parsePtr->numWords != 3) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "varName imm8");
	    goto cleanup;
	}
	if ((localVar = FindLocalVar(assemEnvPtr, &tokenPtr)) < 0
	    || CheckOneByte(interp, localVar)
	    || GetIntegerOperand(assemEnvPtr, &tokenPtr, &opnd) != TCL_OK
	    || CheckSignedOneByte(interp, opnd)) {
	    goto cleanup;
	}
	BBEmitInstInt1(assemEnvPtr, tblind, localVar, 0);
	TclEmitInt1(opnd, envPtr);
	break;

    case ASSEM_LVT4:
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "varname");
	    goto cleanup;
	}
	if ((localVar = FindLocalVar(assemEnvPtr, &tokenPtr)) < 0) {
	    goto cleanup;
	}
	BBEmitInstInt4(assemEnvPtr, tblind, localVar, 0);
	break;

    case ASSEM_OVER:
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "count");
	    goto cleanup;
	}
	if (GetIntegerOperand(assemEnvPtr, &tokenPtr, &opnd) != TCL_OK) {
	    goto cleanup;
	}
	BBEmitInstInt4(assemEnvPtr, tblind, opnd, opnd+1);
	break;

    case ASSEM_REVERSE:
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "count");
	    goto cleanup;
	}
	if (GetIntegerOperand(assemEnvPtr, &tokenPtr, &opnd) != TCL_OK) {
	    goto cleanup;
	}
	BBEmitInstInt4(assemEnvPtr, tblind, opnd, opnd);
	break;
		 
    case ASSEM_SINT1:
	if (parsePtr->numWords != 2) {
	    Tcl_WrongNumArgs(interp, 1, &instNameObj, "imm8");
	    goto cleanup;
	}
	if (GetIntegerOperand(assemEnvPtr, &tokenPtr, &opnd) != TCL_OK
	    || CheckSignedOneByte(interp, opnd) != TCL_OK) {
	    goto cleanup;
	}
	BBEmitInstInt1(assemEnvPtr, tblind, opnd, 0);
	break;

    default:
	Tcl_Panic("Instruction \"%s\" could not be found, can't happen\n",
		  Tcl_GetString(instNameObj));
    }

    status = TCL_OK;
 cleanup:
    if (instNameObj) {
	Tcl_DecrRefCount(instNameObj);
    }
    if (operand1Obj) {
	Tcl_DecrRefCount(operand1Obj);
    }
    return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * GetNextOperand --
 *
 *	Retrieves the next operand in sequence from an assembly
 *	instruction, and makes sure that its value is known at
 *	compile time.
 *
 * Results:
 *	If successful, returns TCL_OK and leaves a Tcl_Obj with
 *	the operand text in *operandObjPtr. In case of failure,
 *	returns TCL_ERROR and leaves *operandObjPtr untouched.
 *
 * Side effects:
 *	Advances *tokenPtrPtr around the token just processed.
 *
 *-----------------------------------------------------------------------------
 */

static int
GetNextOperand(AssembleEnv* assemEnvPtr,
				/* Assembler environment */
	       Tcl_Token** tokenPtrPtr,
				/* INPUT/OUTPUT: Pointer to the token
				 * holding the operand */
	       Tcl_Obj** operandObjPtr)
				/* OUTPUT: Tcl object holding the
				 * operand text with \-substitutions 
				 * done. */
{
    Tcl_Interp* interp = (Tcl_Interp*) assemEnvPtr->envPtr->iPtr;
    Tcl_Obj* operandObj = Tcl_NewObj();
    if (!TclWordKnownAtCompileTime(*tokenPtrPtr, operandObj)) {
	Tcl_DecrRefCount(operandObj);
	if (assemEnvPtr->flags & TCL_EVAL_DIRECT) {
	    Tcl_SetObjResult(interp,
			     Tcl_NewStringObj("assembly code may not "
					      "contain substitutions", -1));
	    Tcl_SetErrorCode(interp, "TCL", "ASSEM", "NOSUBST", NULL);
	}
	return TCL_ERROR;
    }
    *tokenPtrPtr = TokenAfter(*tokenPtrPtr);
    Tcl_IncrRefCount(operandObj);
    *operandObjPtr = operandObj;
    return TCL_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * GetBooleanOperand --
 *
 *	Retrieves a Boolean operand from the input stream and advances
 *	the token pointer.
 *
 * Results:
 *	Returns a standard Tcl result (with an error message in the
 *	interpreter on failure).
 *
 * Side effects:
 *	Stores the Boolean value in (*result) and advances (*tokenPtrPtr)
 *	to the next token.
 *
 *-----------------------------------------------------------------------------
 */

static int
GetBooleanOperand(AssembleEnv* assemEnvPtr,
				/* Assembly environment */
		  Tcl_Token** tokenPtrPtr,
				/* Current token from the parser */
		  int* result)
				/* OUTPUT: Integer extracted from the token */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */
    Tcl_Token* tokenPtr = *tokenPtrPtr;
				/* INOUT: Pointer to the next token
				 * in the source code */
    Tcl_Obj* intObj = Tcl_NewObj();
				/* Integer from the source code */
    int status;			/* Tcl status return */

    /* Extract the next token as a string */

    Tcl_IncrRefCount(intObj);
    if (GetNextOperand(assemEnvPtr, tokenPtrPtr, &intObj) != TCL_OK) {
	Tcl_DecrRefCount(intObj);
	return TCL_ERROR;
    }
    
    /* Convert to an integer, advance to the next token and return */

    status = Tcl_GetBooleanFromObj(interp, intObj, result);
    Tcl_DecrRefCount(intObj);
    *tokenPtrPtr = TokenAfter(tokenPtr);
    return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * GetIntegerOperand --
 *
 *	Retrieves an integer operand from the input stream and advances
 *	the token pointer.
 *
 * Results:
 *	Returns a standard Tcl result (with an error message in the
 *	interpreter on failure).
 *
 * Side effects:
 *	Stores the integer value in (*result) and advances (*tokenPtrPtr)
 *	to the next token.
 *
 *-----------------------------------------------------------------------------
 */

static int
GetIntegerOperand(AssembleEnv* assemEnvPtr,
				/* Assembly environment */
		  Tcl_Token** tokenPtrPtr,
				/* Current token from the parser */
		  int* result)
				/* OUTPUT: Integer extracted from the token */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */
    Tcl_Token* tokenPtr = *tokenPtrPtr;
				/* INOUT: Pointer to the next token
				 * in the source code */
    Tcl_Obj* intObj = Tcl_NewObj();
				/* Integer from the source code */
    int status;			/* Tcl status return */

    /* Extract the next token as a string */

    Tcl_IncrRefCount(intObj);
    if (GetNextOperand(assemEnvPtr, tokenPtrPtr, &intObj) != TCL_OK) {
	Tcl_DecrRefCount(intObj);
	return TCL_ERROR;
    }
    
    /* Convert to an integer, advance to the next token and return */

    status = Tcl_GetIntFromObj(interp, intObj, result);
    Tcl_DecrRefCount(intObj);
    *tokenPtrPtr = TokenAfter(tokenPtr);
    return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * FindLocalVar --
 *
 *	Gets the name of a local variable from the input stream and advances
 *	the token pointer.
 *
 * Results:
 *	Returns the LVT index of the local variable.  Returns -1 if
 *	the variable is non-local, not known at compile time, or
 *	cannot be installed in the LVT (leaving an error message in
 *	the interpreter result if necessary).
 *
 * Side effects:
 *	Advances the token pointer.  May define a new LVT slot if the
 *	variable has not yet been seen and the execution context allows
 *	for it.
 *
 *-----------------------------------------------------------------------------
 */

static int
FindLocalVar(AssembleEnv* assemEnvPtr,
	     Tcl_Token** tokenPtrPtr)
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */
    Tcl_Token* tokenPtr = *tokenPtrPtr;
				/* INOUT: Pointer to the next token
				 * in the source code */
    Tcl_Obj* varNameObj = Tcl_NewObj();
				/* Name of the variable */
    const char* varNameStr;
    int varNameLen;
    int localVar;		/* Index of the variable in the LVT */

    Tcl_IncrRefCount(varNameObj);
    if (GetNextOperand(assemEnvPtr, tokenPtrPtr, &varNameObj) != TCL_OK) {
	Tcl_DecrRefCount(varNameObj);
	return -1;
    }
    varNameStr = Tcl_GetStringFromObj(varNameObj, &varNameLen);
    if (CheckNamespaceQualifiers(interp, varNameStr, varNameLen)) {
	return -1;
    }
    localVar = TclFindCompiledLocal(varNameStr, varNameLen, 1, envPtr);
    Tcl_DecrRefCount(varNameObj);
    if (localVar == -1) {
	if (assemEnvPtr->flags & TCL_EVAL_DIRECT) {
	    Tcl_SetObjResult(interp,
			     Tcl_NewStringObj("cannot use this instruction"
					      " in non-proc context", -1));
	    Tcl_SetErrorCode(interp, "TCL", "ASSEM", "LVT", NULL);
	}
	return -1;
    }
    *tokenPtrPtr = TokenAfter(tokenPtr);
    return localVar;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncStackDepth --
 *
 *	Copies the stack depth from the compile environment to a basic
 *	block.
 *
 * Side effects:
 *	Current and max stack depth in the current basic block are
 *	adjusted.
 *
 * This procedure is called on return from invoking the compiler for
 * the 'eval' and 'expr' operations. It adjusts the stack depth of the
 * current basic block to reflect the stack required by the just-compiled
 * code.
 *
 *-----------------------------------------------------------------------------
 */

static void
SyncStackDepth(AssembleEnv* assemEnvPtr)
				/* Assembly environment */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    BasicBlock* curr_bb = assemEnvPtr->curr_bb;
				/* Current basic block */
    int maxStackDepth = curr_bb->finalStackDepth + envPtr->maxStackDepth;
				/* Max stack depth in the basic block */

    if (maxStackDepth > curr_bb->maxStackDepth) {
	curr_bb->maxStackDepth = maxStackDepth;
    }
    curr_bb->finalStackDepth += envPtr->currStackDepth;
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
			 const char* name,
				/* Variable name to check */
			 int nameLen)
				/* Length of the variable */
{
    Tcl_Obj* result;		/* Error message */
    const char* p;
    for (p = name; p+2 < name+nameLen;  p++) {      
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
DefineLabel(AssembleEnv* assemEnvPtr,	/* Assembly environment */
	    const char* labelName)	/* Label being defined */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */
    Tcl_HashEntry* entry;	/* Label's entry in the symbol table */
    int isNew;			/* Flag == 1 iff the label was previously
				 * undefined */
    label* l;			/*  */
    Tcl_Obj* result;

    /* Look up the newly-defined label in the symbol table */

    entry = Tcl_CreateHashEntry(&assemEnvPtr->labelHash, labelName, &isNew);
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
StartBasicBlock(AssembleEnv* assemEnvPtr,
				/* Assembly environment */
		int fallsThrough,
				/* 1 if execution falls through into
				 * the following block, 0 otherwise */
		const char* jumpLabel)
				/* Label of the location that the
				 * block jumps to, or NULL if the block
				 * does not jump */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    int isNew;	       		/* Unused return from Tcl_CreateHashEntry */
    BasicBlock* newBB;	 	/* BasicBlock structure for the new block */
    BasicBlock* currBB = assemEnvPtr->curr_bb;

    /* Coalesce zero-length blocks */

    if (currBB->start == envPtr->codeNext) {
	return currBB;
    }

    /* Make the new basic block */

    newBB = AllocBB(assemEnvPtr);

    /* Record the jump target if there is one. */

    if (jumpLabel) {
	currBB->jumpTargetLabelHashEntry =
	    Tcl_CreateHashEntry(&assemEnvPtr->BBHash, jumpLabel, &isNew);
    } else {
	currBB->jumpTargetLabelHashEntry = NULL;
    }

    /* Record the fallthrough if there is one. */

    currBB->may_fall_thru = fallsThrough;

    /* Record the successor block */

    currBB->successor1 = newBB;
    assemEnvPtr->curr_bb = newBB;
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
AllocBB(AssembleEnv* assemEnvPtr)
				/* Assembly environment */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
    BasicBlock * bb = (BasicBlock *) ckalloc(sizeof(BasicBlock));

    bb->start = envPtr->codeNext;
    bb->startLine = assemEnvPtr->cmdLine;
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

/*
 *-----------------------------------------------------------------------------
 *
 * FinishAssembly --
 *
 *	Postprocessing after all bytecode has been generated for a block
 *	of assembly code.
 *
 * Results:
 *	Returns a standard Tcl result, with an error message left in the
 *	interpreter if appropriate.
 *
 * Side effects:
 *	The program is checked to see if any undefined labels remain.
 *	The initial stack depth of all the basic blocks in the flow graph
 *	is calculated and saved.  The stack balance on exit is computed,
 *	checked and saved.
 *
 *-----------------------------------------------------------------------------
 */

static int 
FinishAssembly(AssembleEnv* assemEnvPtr)
				/* Assembly environment */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */
    BasicBlock* curr_bb = assemEnvPtr->curr_bb;
				/* Last basic block in the program */
    Tcl_Obj* depthObj;		/* Depth of the stack on exit */
    Tcl_Obj* resultObj;		/* Error message from this function */
    int litIndex;		/* Index of the empty literal {} */

    /* TODO - Check for undefined labels */

    /* Tie off the last basic block */

    curr_bb->may_fall_thru = 0;
    curr_bb->jumpTargetLabelHashEntry = NULL;

    /* Compute stack balance throughout the program */

    if (CheckStack(assemEnvPtr) != TCL_OK) {
	return TCL_ERROR;
    }

    /* TODO - Check for unreachable code */

    /* If the exit is reachable, make sure that the program exits with
     * 1 operand on the stack. */

    if (curr_bb->visited) {

	/* Exit with no operands; push an empty one. */

	int depth = curr_bb->finalStackDepth + curr_bb->initialStackDepth;
	if (depth == 0) {
	    /* Emit a 'push' of the empty literal */
	    litIndex = TclRegisterNewLiteral(envPtr, "", 0);
	    /* Assumes that 'push' is at slot 0 in talInstructionTable */
	    BBEmitInst1or4(assemEnvPtr, 0, litIndex, 0);
	    ++depth;
	}

	/* Exit with unbalanced stack */

	if (depth != 1) {
	    if (assemEnvPtr->flags & TCL_EVAL_DIRECT) {
		depthObj = Tcl_NewIntObj(depth);
		Tcl_IncrRefCount(depthObj);
		resultObj = Tcl_NewStringObj("stack is unbalanced on exit "
					     "from the code (depth=", -1);
		Tcl_AppendObjToObj(resultObj, depthObj);
		Tcl_DecrRefCount(depthObj);
		Tcl_AppendToObj(resultObj, ")", -1);
		Tcl_SetObjResult(interp, resultObj);
		Tcl_SetErrorCode(interp, "TCL", "ASSEM", "BADSTACK", NULL);
	    }
	    return TCL_ERROR;
	}

	/* Record stack usage */

	envPtr->currStackDepth += depth;
    }
    return TCL_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * CheckStack --
 *
 *	Audit stack usage in a block of assembly code.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Updates stack depth on entry for all basic blocks in the flowgraph.
 *	Calculates the max stack depth used in the program, and updates the
 *	compilation environment to reflect it.
 *
 *-----------------------------------------------------------------------------
 */

static int
CheckStack(AssembleEnv* assemEnvPtr)
				/* Assembly environment */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    int maxDepth;		/* Maximum stack depth overall */

    /* Checking the head block will check all the other blocks  recursively. */

    assemEnvPtr->maxDepth = 0;
    if(StackCheckBasicBlock(assemEnvPtr,
			    assemEnvPtr->head_bb, NULL, 0) == TCL_ERROR) {
        return TCL_ERROR;
    }

    /* Post the max stack depth back to the compilation environment */

    maxDepth = assemEnvPtr->maxDepth + envPtr->currStackDepth;
    if (maxDepth > envPtr->maxStackDepth) {
	envPtr->maxStackDepth = maxDepth;
    }

    return TCL_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * StackCheckBasicBlock --
 *
 *	Checks stack consumption for a basic block (and recursively for
 *	its successors).
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Updates initial stack depth for the basic block and its
 *	successors. (Final and maximum stack depth are relative to
 *	initial, and are not touched).
 *
 * This procedure eventually checks, for the entire flow graph, whether
 * stack balance is consistent.  It is an error for a given basic block
 * to be reachable along multiple flow paths with different stack depths.
 *
 *-----------------------------------------------------------------------------
 */

static int
StackCheckBasicBlock(AssembleEnv* assemEnvPtr,
				/* Assembly environment */
		     BasicBlock* blockPtr,
				/* Pointer to the basic block being checked */
		     BasicBlock* predecessor,
				/* Pointer to the block that passed control
				 * to this one. */
		     int initialStackDepth) 
				/* Stack depth on entry to the block */
{ 
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */
    int stackDepth;		/* Current stack depth */
    int maxDepth;		/* Maximum stack depth so far */
    int result;			/* Tcl status return */

    if (blockPtr->visited) {

	/* 
	 * If the block is already visited, check stack depth for consistency
	 * among the paths that reach it.
	 */
        if (blockPtr->initialStackDepth != initialStackDepth) {
	    if (assemEnvPtr->flags & TCL_EVAL_DIRECT) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("inconsistent stack depths "
						  "on two execution paths",
						  -1));
		/* TODO - add execution trace of both paths */
		Tcl_SetErrorLine(interp, blockPtr->startLine);
		Tcl_SetErrorCode(interp, "TCL", "ASSEM", "BADSTACK", NULL);
	    }
            return TCL_ERROR;
        } else {
            return TCL_OK;
        }
    }

    /*
     * If the block is not already visited, set the 'predecessor'
     * link to indicate how control got to it. Set the initial stack
     * depth to the current stack depth in the flow of control.
     * Calculate max and min stack depth, flag an error if the
     * block underflows the stack, and update max stack depth in the
     * assembly environment.
     */
    blockPtr->visited = 1;
    blockPtr->predecessor = predecessor;
    blockPtr->initialStackDepth = initialStackDepth;
    if (initialStackDepth + blockPtr->minStackDepth < 0) {
	if (assemEnvPtr->flags & TCL_EVAL_DIRECT) {
	    Tcl_SetObjResult(interp,
			     Tcl_NewStringObj("stack underflow", -1));
	    Tcl_SetErrorCode(interp, "TCL", "ASSEM", "BADSTACK", NULL);
	    AddBasicBlockRangeToErrorInfo(assemEnvPtr, blockPtr);
	    Tcl_SetErrorLine(interp, blockPtr->startLine);
	}
	return TCL_ERROR;
    }
    maxDepth = initialStackDepth + blockPtr->maxStackDepth;
    if (maxDepth > assemEnvPtr->maxDepth) {
	assemEnvPtr->maxDepth = maxDepth;
    }
    
    /*
     * Calculate stack depth on exit from the block, and invoke this
     * procedure recursively to check successor blocks
     */

    stackDepth = initialStackDepth + blockPtr->finalStackDepth;
    result = TCL_OK;
    if (blockPtr->may_fall_thru) {
        result = StackCheckBasicBlock(assemEnvPtr, blockPtr->successor1,
				      blockPtr, stackDepth);        
        
    }
    if (result == TCL_OK && blockPtr->jumpTargetLabelHashEntry != NULL) {
        BasicBlock * targetBlock = (BasicBlock *)
	    Tcl_GetHashValue(blockPtr->jumpTargetLabelHashEntry);
        result = StackCheckBasicBlock(assemEnvPtr, targetBlock, blockPtr,
				      stackDepth);
    }
    return result;
}

/*
 *-----------------------------------------------------------------------------
 *
 * AddBasicBlockRangeToErrorInfo --
 *
 *	Updates the error info of the Tcl interpreter to show a given
 *	basic block in the code.
 *
 * This procedure is used to label the callstack with source location
 * information when reporting an error in stack checking.
 *
 *-----------------------------------------------------------------------------
 */

static void
AddBasicBlockRangeToErrorInfo(AssembleEnv* assemEnvPtr,
				/* Assembly environment */
			      BasicBlock* bbPtr)
				/* Basic block in which the error is
				 * found */
{
    CompileEnv* envPtr = assemEnvPtr->envPtr;
				/* Compilation environment */
    Tcl_Interp* interp = (Tcl_Interp*) envPtr->iPtr;
				/* Tcl interpreter */
    Tcl_Obj* lineNo;		/* Line number in the source */

    Tcl_AddErrorInfo(interp, "\n    in assembly code between lines ");
    lineNo = Tcl_NewIntObj(bbPtr->startLine);
    Tcl_IncrRefCount(lineNo);
    Tcl_AddErrorInfo(interp, Tcl_GetString(lineNo));
    Tcl_AddErrorInfo(interp, " and ");
    if (bbPtr->successor1 != NULL) {
	Tcl_SetIntObj(lineNo, bbPtr->successor1->startLine);
	Tcl_AddErrorInfo(interp, Tcl_GetString(lineNo));
    } else {
	Tcl_AddErrorInfo(interp, "end of assembly code");
    }
    Tcl_DecrRefCount(lineNo);
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
	    
