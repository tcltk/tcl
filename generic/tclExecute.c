/* 
 * tclExecute.c --
 *
 *	This file contains procedures that execute byte-compiled Tcl
 *	commands.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 *
 ***********************************************************************
 * Experimental version of a new, hopefully faster bytecode engine (a 
 * previous version was announced under the nickname S4).
 * Some compiler-dependent optimisations are defined in a few macros in 
 * the accompanying file tclExecute.h
 ***********************************************************************
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclExecute.c,v 1.21.2.7 2001/05/19 22:05:45 msofer Exp $
 */

#include "tclInt.h"
#include "tclCompile.h"

#ifdef NO_FLOAT_H
#   include "../compat/float.h"
#else
#   include <float.h>
#endif
#ifndef TCL_NO_MATH

#include "tclMath.h"
#endif

/*
 * The stuff below is a bit of a hack so that this file can be used
 * in environments that include no UNIX, i.e. no errno.  Just define
 * errno here.
 */ 

#ifndef TCL_GENERIC_ONLY
#include "tclPort.h"
#else
#define NO_ERRNO_H
#endif

#ifdef NO_ERRNO_H
int errno;
#define EDOM 33
#define ERANGE 34
#endif

/*
 * Boolean flag indicating whether the Tcl bytecode interpreter has been
 * initialized.
 */

static int execInitialized = 0;
TCL_DECLARE_MUTEX(execMutex)

/*
 * Variable that controls whether execution tracing is enabled and, if so,
 * what level of tracing is desired:
 *    0: no execution tracing
 *    1: trace invocations of Tcl procs only
 *    2: trace invocations of all (not compiled away) commands
 *    3: display each instruction executed
 * This variable is linked to the Tcl variable "tcl_traceExec".
 */

int tclTraceExec = 0;

typedef struct ThreadSpecificData {
    /*
     * The following global variable is use to signal matherr that Tcl
     * is responsible for the arithmetic, so errors can be handled in a
     * fashion appropriate for Tcl.  Zero means no Tcl math is in
     * progress;  non-zero means Tcl is doing math.
     */
    
    int mathInProgress;

} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

/*
 * The variable below serves no useful purpose except to generate
 * a reference to matherr, so that the Tcl version of matherr is
 * linked in rather than the system version. Without this reference
 * the need for matherr won't be discovered during linking until after
 * libtcl.a has been processed, so Tcl's version won't be used.
 */

#ifdef NEED_MATHERR
extern int matherr();
int (*tclMatherrPtr)() = matherr;
#endif

/*
 * Mapping from expression instruction opcodes to strings; used for error
 * messages. Note that these entries must match the order and number of the
 * expression opcodes (e.g., INST_LOR) in tclCompile.h.
 */

static char *operatorStrings[] = {
    "||", "&&", "|", "^", "&", "==", "!=", "<", ">", "<=", ">=", "<<", ">>",
    "+", "-", "*", "/", "%", "+", "-", "~", "!",
    "BUILTIN FUNCTION", "FUNCTION",
    "", "", "", "", "", "", "", "", "eq", "ne",
};
    
/*
 * Mapping from Tcl result codes to strings; used for error and debugging
 * messages. 
 */

#ifdef TCL_COMPILE_DEBUG
static char *resultStrings[] = {
    "TCL_OK", "TCL_ERROR", "TCL_RETURN", "TCL_BREAK", "TCL_CONTINUE"
};
#endif

/*
 * These are used by evalstats to monitor object usage in Tcl.
 */

#ifdef TCL_COMPILE_STATS
long		tclObjsAlloced = 0;
long		tclObjsFreed   = 0;
#define TCL_MAX_SHARED_OBJ_STATS 5
long		tclObjsShared[TCL_MAX_SHARED_OBJ_STATS] = { 0, 0, 0, 0, 0 };
#endif /* TCL_COMPILE_STATS */

/*
 * Macros for testing floating-point values for certain special cases. Test
 * for not-a-number by comparing a value against itself; test for infinity
 * by comparing against the largest floating-point value.
 */

#define IS_NAN(v) ((v) != (v))
#ifdef DBL_MAX
#   define IS_INF(v) (((v) > DBL_MAX) || ((v) < -DBL_MAX))
#else
#   define IS_INF(v) 0
#endif


/*
 * Macros used to cache often-referenced Tcl evaluation stack information
 * in local variables. Note that a DECACHE_STACK_INFO()-CACHE_STACK_INFO()
 * pair must surround any call inside TclExecuteByteCode (and a few other
 * procedures that use this scheme) that could result in a recursive call
 * to TclExecuteByteCode.
 */

#define CACHE_STACK_INFO()   tosPtr = eePtr->tosPtr

#define DECACHE_STACK_INFO() eePtr->tosPtr = tosPtr

/*
 * Macros used to access items on the Tcl evaluation stack. PUSH_OBJECT
 * increments the object's ref count since it makes the stack have another
 * reference pointing to the object. However, POP_OBJECT does not decrement
 * the ref count. This is because the stack may hold the only reference to
 * the object, so the object would be destroyed if its ref count were
 * decremented before the caller had a chance to, e.g., store it in a
 * variable. It is the caller's responsibility to decrement the ref count
 * when it is finished with an object.
 *
 * WARNING! It is essential that objPtr only appear once in the PUSH_OBJECT
 * macro. The actual parameter might be an expression with side effects,
 * and this ensures that it will be executed only once. 
 */
    
#define PUSH_OBJECT(objPtr) \
    Tcl_IncrRefCount(*(++tosPtr) = (objPtr))

#define POP_OBJECT() \
    (*tosPtr--)

/* 
 * Set an object at stackTop, increase its refCount
 */
#define SET_TOS(objPtr) \
    Tcl_IncrRefCount(*tosPtr = (objPtr))

#define TOS \
    (*tosPtr)


/*
 * Macros used to trace instruction execution. The macros TRACE,
 * TRACE_WITH_OBJ, and O2S are only used inside TclExecuteByteCode.
 * O2S is only used in TRACE* calls to get a string from an object.
 */

#ifdef TCL_COMPILE_DEBUG
#define TRACE(a) \
    if (traceInstructions) { \
        fprintf(stdout, "%2d: %2d (%u) %s ", iPtr->numLevels, stackTop, \
	       (unsigned int)(pc - codePtr->codeStart), \
	       GetOpcodeName(pc)); \
	printf a; \
    }
#define TRACE_WITH_OBJ(a, objPtr) \
    if (traceInstructions) { \
        fprintf(stdout, "%2d: %2d (%u) %s ", iPtr->numLevels, stackTop, \
	       (unsigned int)(pc - codePtr->codeStart), \
	       GetOpcodeName(pc)); \
	printf a; \
        TclPrintObject(stdout, objPtr, 30); \
        fprintf(stdout, "\n"); \
    }
#define O2S(objPtr) \
    (objPtr ? Tcl_GetString(objPtr) : "")
#else
#define TRACE(a)
#define TRACE_WITH_OBJ(a, objPtr)
#define O2S(objPtr)
#endif /* TCL_COMPILE_DEBUG */


/*
 * Declarations for local procedures to this file:
 */

static void		CallTraceProcedure _ANSI_ARGS_((Tcl_Interp *interp,
			    Trace *tracePtr, Command *cmdPtr,
			    char *command, int numChars,
			    int objc, Tcl_Obj *objv[]));
static void		DupCmdNameInternalRep _ANSI_ARGS_((Tcl_Obj *objPtr,
			    Tcl_Obj *copyPtr));
static int		ExprAbsFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    ExecEnv *eePtr, ClientData clientData));
static int		ExprBinaryFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    ExecEnv *eePtr, ClientData clientData));
static int		ExprCallMathFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    ExecEnv *eePtr, int objc, Tcl_Obj **objv));
static int		ExprDoubleFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    ExecEnv *eePtr, ClientData clientData));
static int		ExprIntFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    ExecEnv *eePtr, ClientData clientData));
static int		ExprRandFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    ExecEnv *eePtr, ClientData clientData));
static int		ExprRoundFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    ExecEnv *eePtr, ClientData clientData));
static int		ExprSrandFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    ExecEnv *eePtr, ClientData clientData));
static int		ExprUnaryFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    ExecEnv *eePtr, ClientData clientData));
#ifdef TCL_COMPILE_STATS
static int              EvalStatsCmd _ANSI_ARGS_((ClientData clientData,
                            Tcl_Interp *interp, int argc, char **argv));
#endif
static void		FreeCmdNameInternalRep _ANSI_ARGS_((
    			    Tcl_Obj *objPtr));
#ifdef TCL_COMPILE_DEBUG
static char *		GetOpcodeName _ANSI_ARGS_((unsigned char *pc));
#endif
static ExceptionRange *	GetExceptRangeForPc _ANSI_ARGS_((unsigned char *pc,
			    int catchOnly, ByteCode* codePtr));
static char *		GetSrcInfoForPc _ANSI_ARGS_((unsigned char *pc,
        		    ByteCode* codePtr, int *lengthPtr));
static void		GrowEvaluationStack _ANSI_ARGS_((ExecEnv *eePtr));
static void		IllegalExprOperandType _ANSI_ARGS_((
			    Tcl_Interp *interp, unsigned char *pc,
			    Tcl_Obj *opndPtr));
static void		InitByteCodeExecution _ANSI_ARGS_((
			    Tcl_Interp *interp));
#ifdef TCL_COMPILE_DEBUG
static void		PrintByteCodeInfo _ANSI_ARGS_((ByteCode *codePtr));
#endif
static int		SetCmdNameFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
#ifdef TCL_COMPILE_DEBUG
static char *		StringForResultCode _ANSI_ARGS_((int result));
static void		ValidatePcAndStackTop _ANSI_ARGS_((
			    ByteCode *codePtr, unsigned char *pc,
			    int stackTop, int stackLowerBound,
			    int stackUpperBound));
#endif
static int		VerifyExprObjType _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));

/*
 * Table describing the built-in math functions. Entries in this table are
 * indexed by the values of the INST_CALL_BUILTIN_FUNC instruction's
 * operand byte.
 */

BuiltinFunc builtinFuncTable[] = {
#ifndef TCL_NO_MATH
    {"acos", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) acos},
    {"asin", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) asin},
    {"atan", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) atan},
    {"atan2", 2, {TCL_DOUBLE, TCL_DOUBLE}, ExprBinaryFunc, (ClientData) atan2},
    {"ceil", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) ceil},
    {"cos", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) cos},
    {"cosh", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) cosh},
    {"exp", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) exp},
    {"floor", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) floor},
    {"fmod", 2, {TCL_DOUBLE, TCL_DOUBLE}, ExprBinaryFunc, (ClientData) fmod},
    {"hypot", 2, {TCL_DOUBLE, TCL_DOUBLE}, ExprBinaryFunc, (ClientData) hypot},
    {"log", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) log},
    {"log10", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) log10},
    {"pow", 2, {TCL_DOUBLE, TCL_DOUBLE}, ExprBinaryFunc, (ClientData) pow},
    {"sin", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) sin},
    {"sinh", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) sinh},
    {"sqrt", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) sqrt},
    {"tan", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) tan},
    {"tanh", 1, {TCL_DOUBLE}, ExprUnaryFunc, (ClientData) tanh},
#endif
    {"abs", 1, {TCL_EITHER}, ExprAbsFunc, 0},
    {"double", 1, {TCL_EITHER}, ExprDoubleFunc, 0},
    {"int", 1, {TCL_EITHER}, ExprIntFunc, 0},
    {"rand", 0, {TCL_EITHER}, ExprRandFunc, 0},	/* NOTE: rand takes no args. */
    {"round", 1, {TCL_EITHER}, ExprRoundFunc, 0},
    {"srand", 1, {TCL_INT}, ExprSrandFunc, 0},
    {0},
};

/*
 * The structure below defines the command name Tcl object type by means of
 * procedures that can be invoked by generic object code. Objects of this
 * type cache the Command pointer that results from looking up command names
 * in the command hashtable. Such objects appear as the zeroth ("command
 * name") argument in a Tcl command.
 */

Tcl_ObjType tclCmdNameType = {
    "cmdName",				/* name */
    FreeCmdNameInternalRep,		/* freeIntRepProc */
    DupCmdNameInternalRep,		/* dupIntRepProc */
    (Tcl_UpdateStringProc *) NULL,	/* updateStringProc */
    SetCmdNameFromAny			/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * InitByteCodeExecution --
 *
 *	This procedure is called once to initialize the Tcl bytecode
 *	interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This procedure initializes the array of instruction names. If
 *	compiling with the TCL_COMPILE_STATS flag, it initializes the
 *	array that counts the executions of each instruction and it
 *	creates the "evalstats" command. It also registers the command name
 *	Tcl_ObjType. It also establishes the link between the Tcl
 *	"tcl_traceExec" and C "tclTraceExec" variables.
 *
 *----------------------------------------------------------------------
 */

static void
InitByteCodeExecution(interp)
    Tcl_Interp *interp;		/* Interpreter for which the Tcl variable
				 * "tcl_traceExec" is linked to control
				 * instruction tracing. */
{
    Tcl_RegisterObjType(&tclCmdNameType);
    if (Tcl_LinkVar(interp, "tcl_traceExec", (char *) &tclTraceExec,
		    TCL_LINK_INT) != TCL_OK) {
	panic("InitByteCodeExecution: can't create link for tcl_traceExec variable");
    }
#ifdef TCL_COMPILE_STATS    
    Tcl_CreateCommand(interp, "evalstats", EvalStatsCmd,
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
#endif /* TCL_COMPILE_STATS */
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateExecEnv --
 *
 *	This procedure creates a new execution environment for Tcl bytecode
 *	execution. An ExecEnv points to a Tcl evaluation stack. An ExecEnv
 *	is typically created once for each Tcl interpreter (Interp
 *	structure) and recursively passed to TclExecuteByteCode to execute
 *	ByteCode sequences for nested commands.
 *
 * Results:
 *	A newly allocated ExecEnv is returned. This points to an empty
 *	evaluation stack of the standard initial size.
 *
 * Side effects:
 *	The bytecode interpreter is also initialized here, as this
 *	procedure will be called before any call to TclExecuteByteCode.
 *
 *----------------------------------------------------------------------
 */

#define TCL_STACK_INITIAL_SIZE 2000

ExecEnv *
TclCreateExecEnv(interp)
    Tcl_Interp *interp;		/* Interpreter for which the execution
				 * environment is being created. */
{
    ExecEnv *eePtr = (ExecEnv *) ckalloc(sizeof(ExecEnv));

    eePtr->stackPtr = (Tcl_Obj **)
	ckalloc((unsigned) (TCL_STACK_INITIAL_SIZE * sizeof(Tcl_Obj *)));
    eePtr->tosPtr = eePtr->stackPtr - 1;
    eePtr->stackEndPtr = eePtr->stackPtr + (TCL_STACK_INITIAL_SIZE - 1);

    Tcl_MutexLock(&execMutex);
    if (!execInitialized) {
	TclInitAuxDataTypeTable();
	InitByteCodeExecution(interp);
	execInitialized = 1;
    }
    Tcl_MutexUnlock(&execMutex);
    return eePtr;
}
#undef TCL_STACK_INITIAL_SIZE


/*
 *----------------------------------------------------------------------
 *
 * TclDeleteExecEnv --
 *
 *	Frees the storage for an ExecEnv.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage for an ExecEnv and its contained storage (e.g. the
 *	evaluation stack) is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclDeleteExecEnv(eePtr)
    ExecEnv *eePtr;		/* Execution environment to free. */
{
    Tcl_EventuallyFree((ClientData)eePtr->stackPtr, TCL_DYNAMIC);
    ckfree((char *) eePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeExecution --
 *
 *	Finalizes the execution environment setup so that it can be
 *	later reinitialized.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	After this call, the next time TclCreateExecEnv will be called
 *	it will call InitByteCodeExecution.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeExecution()
{
    Tcl_MutexLock(&execMutex);
    execInitialized = 0;
    Tcl_MutexUnlock(&execMutex);
    TclFinalizeAuxDataTypeTable();
}

/*
 *----------------------------------------------------------------------
 *
 * GrowEvaluationStack --
 *
 *	This procedure grows a Tcl evaluation stack stored in an ExecEnv.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size of the evaluation stack is doubled.
 *
 *----------------------------------------------------------------------
 */

static void
GrowEvaluationStack(eePtr)
    register ExecEnv *eePtr; /* Points to the ExecEnv with an evaluation
			      * stack to enlarge. */
{
    /*
     * The current Tcl stack elements are stored from eePtr->stackPtr
     * to eePtr->stackEndPtr (inclusive).
     */

    int currElems = (eePtr->stackEndPtr - eePtr->stackPtr) + 1;
    int newElems  = 2*currElems;
    int currBytes = currElems * sizeof(Tcl_Obj *);
    int newBytes  = 2*currBytes;
    int currStackDiff = (eePtr->tosPtr - eePtr->stackPtr);
    Tcl_Obj **stackPtr = (Tcl_Obj **) ckalloc((unsigned) newBytes);

    /*
     * Copy the existing stack items to the new stack space, free the old
     * storage if appropriate, and mark new space as malloc'ed.
     */
 
    memcpy((VOID *) stackPtr, (VOID *) eePtr->stackPtr,
	   (size_t) currBytes);
    Tcl_EventuallyFree((ClientData)eePtr->stackPtr, TCL_DYNAMIC);
    ckfree((char *) eePtr->stackPtr);

    eePtr->stackPtr = stackPtr;
    eePtr->stackEndPtr = stackPtr + (newElems - 1); /* i.e. index of last usable item */
    eePtr->tosPtr = stackPtr + currStackDiff;
}


/*
 * MACROS TO CREATE A STACK OF OBJECTS TO BE FREED
 * 
 * The stack is emptied after each intruction that may have set something 
 * in the stack, and also at (abnormalReturn:) and (processCatch:).
 *
 * The freeing of the stack is done by a loop on TclDecrRefCount (inline version).
 *
 * The code contains different manners of DecrRefCount.
 *   + The standard Tcl two (TclDecrRefCount, Tcl_DecrRefCount) maintain their meaning 
 *     as defined in tclInt.h and tcl.h. 
 *   + For each of these two, a new version with suffix _Q is added, 
 *     Suffix _Q stacks the requests. NEXT_Q is called at instructions 
 *     that may decrease the refCount of variables.
 * The main effect of the stack is to allow for fast processing (inline) without
 * object code bloating (~ 2K); the speed effect of the stack (versus inline processing 
 * at all places) seems negligible.
 *
 * The stack has a constant size (set below at 4, which is actually too large); 
 * BE CAREFUL not to stack objects for freeing from within a loop! You may well cause 
 * an overflow of the stack, with dire consequences ...
 *
 * A further remark on DecrRefCount: as the processing of a catch automatically 
 * frees objects on the tcl stack, we sometimes just increase the pointer to
 * get some objects to be freed included in that process. Look for instructions like
 * "tosPtr++" before a "goto checkForCatch".
 */
#define TclDecrRefCount_Q(objPtr) *decrRefQTop++ = (objPtr)
#define Tcl_DecrRefCount_Q(objPtr) TclDecrRefCount_Q(objPtr)
#define NEXT_INSTR_Q goto instructions_start_Q
#define DECR_REF_STACK_empty() \
     {\
        Tcl_Obj **locQTop = decrRefQTop;\
        while (locQTop > decrRefQ) {\
            Tcl_Obj *objPtr = *(--locQTop); \
            TclDecrRefCount(objPtr);\
	}\
        decrRefQTop = locQTop;\
     }


/* *********************************
 * Common code; out here for clarity
 * *********************************
 */
/* Use the object at TOS if it is not shared; otherwise, 
 * create a new one.
 */    
#define USE_OR_MAKE_THEN_SET(value,typeName) \
    {\
        Tcl_Obj *objPtr = TOS;\
        if (Tcl_IsShared(objPtr)) {\
	    /* If it is shared, just decrease the refCount ... */\
	    (objPtr)->refCount--;\
	    SET_TOS(Tcl_New ## typeName ## Obj(value));\
        } else {	/* reuse the valuePtr object */\
	    /* valuePtr now on stk top has right r.c. */\
	    Tcl_Set ## typeName ## Obj(objPtr, value);\
        }\
    }

union AuxPtr {
    Tcl_Obj *valuePtr;
    Var *varPtr;
};

union AuxVar {
    long   i;
    double d;
};

#define TRY_CONVERT_TO_NUM(valuePtr,X,tPtr) \
/* Tcl_Obj *valuePtr; X is an AuxVar union, tPtr points to the\
 * type of the object after conversion */   \
     { \
	 if ((tPtr) == &tclIntType) {\
	     (X).i = (valuePtr)->internalRep.longValue;\
         } else if (((tPtr) == &tclDoubleType) && ((valuePtr)->bytes == NULL)) {\
	    /*\
	     * We can only use the internal rep directly if there is\
	     * no string rep.  Otherwise the string rep might actually\
	     * look like an integer, which is preferred.\
	     */\
	     (X).d = (valuePtr)->internalRep.doubleValue;\
	 } else {\
	     if (TclLooksLikeInt(TclGetString(valuePtr),(valuePtr)->length)) {\
                 long XX;\
		 (void) Tcl_GetLongFromObj((Tcl_Interp *) NULL,\
			 (valuePtr), &XX);\
                 (X).i = XX;\
	     } else {\
		 double XX;\
		 (void) Tcl_GetDoubleFromObj((Tcl_Interp *) NULL,\
			 (valuePtr), &XX);\
		 (X).d = XX;\
	     }\
             (tPtr) = (valuePtr)->typePtr;\
	 }\
     }

/* 
 * INLINING from Tcl_GetByteArrayFromObj (tclBinary.c) requires this ...
 */

typedef struct ByteArray {
    int used;			/* The number of bytes used in the byte
				 * array. */
    int allocated;		/* The amount of space actually allocated
				 * minus 1 byte. */
    unsigned char bytes[4];	/* The array of bytes.  The actual size of
				 * this field depends on the 'allocated' field
				 * above. */
} ByteArray;


/*
 * Include the compiler-dependent macros that determine the 
 * instruction-threading method used by tclExecute.c
 * See tclExecute.h dor details.
 *
 * An instruction-threading method defines the following macros:
 *
 *  . _CASE_DECLS         declarations of special variables required
 *  . CHECK_OPCODES       0/1, if the opcodes have to be checked before RT
 *  . _CASE_START         start of the block containing instructions
 *  . _CASE(instruction)  the labelling method for instruction start
 *  . NEXT_INSTR          the jump to the next instruction
 *  . _CASE_END           end of the block containing instructions
 *
 *
 ********************************************************************
 * FOR DEBUGGING PURPOSES
 * Uncomment the following line to get the bytecode tracing functionality
 * triggered by setting tcl_traceExec >=2.
 */
/*#define TCL_BYTECODE_DEBUG 1*/

#include "tclExecute.h"


/*
 * TclVerifyOpcodes
 *
 * This function verifies that a given byteCode does not try to
 * call an inexistent opCode; it is NOT NECESSARY for SWITCH method,
 * as it performs the validity check at run time.
 *
 * If it is OK, it marks the bytecode as verified and returns 1; if there 
 * is a bad opCode, it panics with a message.
 */  
      
#if CHECK_OPCODES == 1
static void
TclVerifyOpcodes(codePtr)
    register ByteCode *codePtr;		/* The bytecode sequence to verify. */
{
    register unsigned char *pc = codePtr->codeStart;
    unsigned char *pcEnd;
    pcEnd = pc + codePtr->numCodeBytes;
    while (pc < pcEnd) {
	if (*pc <=  LAST_INST_OPCODE) {
	    pc += instructionTable[*pc].numBytes;
	} else {
	    panic("TclExecuteByteCode: unrecognized opCode %u", *pc);
	}
    }
    codePtr->flags |= TCL_BYTECODE_OPCODES_OK;
}
#endif 


/* 
 *----------------------------------------------------------------------
 *
 * TclExecuteByteCode --
 *
 *	This procedure executes the instructions of a ByteCode structure.
 *	It returns when a "done" instruction is executed or an error occurs.
 *
 * Results:
 *	The return value is one of the return codes defined in tcl.h
 *	(such as TCL_OK), and interp->objResultPtr refers to a Tcl object
 *	that either contains the result of executing the code or an
 *	error message.
 *
 * Side effects:
 *	Almost certainly, depending on the ByteCode's instructions.
 *
 *----------------------------------------------------------------------
 */

int
TclExecuteByteCode(interp, codePtr)
    Tcl_Interp *interp;		/* Token for command interpreter. */
    ByteCode *codePtr;		/* The bytecode sequence to interpret. */
{
    Interp *iPtr = (Interp *) interp;
    ExecEnv *eePtr = iPtr->execEnvPtr;      /* Points to the execution environment. */
    Tcl_Obj **tosPtr = eePtr->tosPtr;       /* Cached pointer to top of evaluation stack. */
    unsigned int initTos = tosPtr - eePtr->stackPtr; /* Stack top at start of execution. */
    unsigned char *pc = codePtr->codeStart; /* The current program counter. */
    int result = TCL_OK;	            /* Return code returned after execution. */
#define DECR_REF_STACK_SIZE 4
    Tcl_Obj *decrRefQ[DECR_REF_STACK_SIZE]; /* structure for objs to be decrRef'ed */
#undef DECR_REF_STACK_SIZE
    Tcl_Obj **decrRefQTop = decrRefQ;

    _CASE_DECLS /* DO NOT PUT A SEMICOLON HERE, it can be empty ! */

    /*
     * This procedure uses a stack to hold information about catch commands.
     * This information is the current operand stack top when starting to
     * execute the code for each catch command. It starts out with stack-
     * allocated space but uses dynamically-allocated storage if needed.
     */

#define STATIC_CATCH_STACK_SIZE 4
    unsigned int catchStackStorage[STATIC_CATCH_STACK_SIZE];
    unsigned int *catchStackPtr = catchStackStorage;
    unsigned int *catchTopPtr = catchStackStorage;

#ifdef TCL_COMPILE_STATS
    iPtr->stats.numExecutions++;
#endif

#if CHECK_OPCODES == 1
    /*
     * Make sure that the opcodes being called are all valid - not used
     * by methods that check that at runtime (e.g., SWITCH).
     *
     * We do it only once per bytecode: TclVerifyOpcodes caches the result
     * by setting a flag (codePtr->flags |= TCL_BYTECODE_OPCODES_OK)
     *
     *** Thanks to Donal Fellows for the idea! ***
     */

    if (!(codePtr->flags & TCL_BYTECODE_OPCODES_OK)) {
	TclVerifyOpcodes(codePtr);
    }
#endif

    /*
     * Make sure the catch stack is large enough to hold the maximum number
     * of catch commands that could ever be executing at the same time. This
     * will be no more than the exception range array's depth.
     */

    if (codePtr->maxExceptDepth > STATIC_CATCH_STACK_SIZE) {
	catchStackPtr = (unsigned int *)
	        ckalloc(codePtr->maxExceptDepth * sizeof(unsigned int));
    }
    catchTopPtr = catchStackPtr;

    /*
     * Make sure the stack has enough room to execute this ByteCode.
     */

    while ((tosPtr + codePtr->maxStackDepth) > eePtr->stackEndPtr) {
        GrowEvaluationStack(eePtr); 
        CACHE_STACK_INFO();
    }

    /*
     * Loop executing instructions until a "done" instruction, a TCL_RETURN,
     * or some error.
     */

    NEXT_INSTR; 

    instructions_start_Q: 
    DECR_REF_STACK_empty();  

    _CASE_START /* DO NOT PUT A SEMICOLON HERE, it can be a { ! */

    _CASE(INST_DONE):  /* tosPtr -= 1 */
    {
        /*
	 * Pop the topmost object from the stack, set the interpreter's
	 * object result to point to it, and return.
	 */
	Tcl_Obj *valuePtr = POP_OBJECT();
	Tcl_SetObjResult(interp, valuePtr);
	valuePtr->refCount--; /* result has a reference, IT IS SHARED! */

	{
	    unsigned int currTos = tosPtr - eePtr->stackPtr;
	    if (currTos != initTos) {
		/*
		 * if extra items in the stack, clean up the stack before return
		 */
		if (currTos > initTos) goto abnormalReturn;

		fprintf(stderr, "\nTclExecuteByteCode: done instruction at pc %u: stack top %d < entry stack top %d\n",
			(unsigned int)(pc - codePtr->codeStart),
			(unsigned int) (currTos),
			(unsigned int) (initTos));
		panic("TclExecuteByteCode execution failure: end stack top < start stack top");
	    }
	}

	goto done;
    }
	    
    _CASE(INST_PUSH1): /* tosPtr += 1 */
    {
        pc++;
	PUSH_OBJECT(codePtr->objArrayPtr[TclGetUInt1AtPtr(pc)]);
	pc++;
	NEXT_INSTR;
    }
	    
    _CASE(INST_PUSH4): /* tosPtr += 1 */
    {
        pc++;
	PUSH_OBJECT(codePtr->objArrayPtr[TclGetUInt4AtPtr(pc)]);
	pc += 4; 
	NEXT_INSTR;
    }

    _CASE(INST_POP): /* tosPtr -= 1 */
    {
	Tcl_Obj *valuePtr = POP_OBJECT();
	TclDecrRefCount(valuePtr); /* finished with pop'ed object. */
	pc++;
	NEXT_INSTR;
    }

    _CASE(INST_DUP): /* tosPtr += 1 */
    {
	Tcl_Obj *item = TOS;
	PUSH_OBJECT(Tcl_DuplicateObj(item));
	pc++;
	NEXT_INSTR;
    }

    _CASE(INST_CONCAT1): /* tosPtr -= (n-1) */
    {
        int totalLen = 0;
	Tcl_Obj **firstItem;
	
	pc++;
	firstItem = tosPtr - (TclGetUInt1AtPtr(pc) - 1);

	/*
	 * Concatenate strings (with no separators) from the top
	 * opnd items on the stack starting with the deepest item.
	 * First, determine how many characters are needed.
	 */
	{ 
	    Tcl_Obj **itemPtr;
	    for (itemPtr = firstItem;  itemPtr <= tosPtr;  itemPtr++) {
	        Tcl_Obj* item = *itemPtr;
		if (Tcl_GetString(item) != NULL) {
		    totalLen += item->length;
		}
	    }
	}

	/*
	 * Initialize the new append string object by appending the
	 * strings of the opnd stack objects. Also pop the objects. 
	 */

	{
	    Tcl_Obj *concatObjPtr;
	    TclNewObj(concatObjPtr);
	    if (totalLen > 0) {
	        char *p = (char *) ckalloc((unsigned) (totalLen + 1));
		Tcl_Obj **itemPtr;
		concatObjPtr->bytes = p;
		concatObjPtr->length = totalLen;
		for (itemPtr = firstItem;  itemPtr <= tosPtr;  itemPtr++) {
		    Tcl_Obj *item = *itemPtr;
		    if (item->bytes != NULL) {
		        memcpy((VOID *) p, (VOID *) item->bytes,
			        (size_t) item->length);
			p += item->length;
		    }
		    /* in a loop: do not _Q */
		    TclDecrRefCount(item);
		}
		*p = '\0';
	    } else {
	        for ( ;  tosPtr >= firstItem;  tosPtr--) {
		    Tcl_Obj *item = TOS;
		    /* in a loop: do not _Q */
		    Tcl_DecrRefCount(item);
		}
	    }
	    /* This pushes concatObjPtr */
	    tosPtr = firstItem;
	    SET_TOS(concatObjPtr);
	    pc++;
	    NEXT_INSTR;
	}
    }
	    
    _CASE(INST_INVOKE_STK4): /* tosPtr -= (n-1) */
    {
        int objc;
#ifdef TCL_BYTECODE_DEBUG /* need the reference for messages! */
	unsigned char *oldPc;
        oldPc = pc;
#endif
	pc++;
	objc = TclGetUInt4AtPtr(pc);
	pc += 4;
	goto doInvocation;

    _CASE(INST_INVOKE_STK1):
#ifdef TCL_BYTECODE_DEBUG
	oldPc = pc;
#endif
        pc++;
	objc = TclGetUInt1AtPtr(pc);
	pc++;
	    
	doInvocation:
	/*
	 * If the interpreter was deleted, return an error.
	 */
		
	if (iPtr->flags & DELETED) {
	    pc = pc--; /* to get back within the scope of the cmd */
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
	        "attempt to call eval in deleted interpreter", -1);
	    Tcl_SetErrorCode(interp, "CORE", "IDELETE",
	        "attempt to call eval in deleted interpreter",
			(char *) NULL);
	    result = TCL_ERROR;
	    goto checkForCatch;
	}
	{
	    Tcl_Obj **objv;  /* The array of argument objects. */
	    Command *cmdPtr; /* Points to command's Command struct. */    
	    Tcl_Obj **preservedStack;
	                     /* Reference to memory block containing
			      * objv array (must be kept live throughout
			      * trace and command invokations.) */

	    /*
	     * Find the procedure to execute this command. If the
	     * command is not found, handle it with the "unknown" proc.
	     */

	    objv = tosPtr - (objc-1);
	    /* ONLY CALL: maybe inline, maybe using gcc function inlining? OJO: ahora son dos ...*/
	    cmdPtr = (Command *) Tcl_GetCommandFromObj(interp, objv[0]);
	    if (cmdPtr == NULL) {	
		cmdPtr = (Command *) Tcl_FindCommand(interp, "unknown",
		            (Tcl_Namespace *) NULL, TCL_GLOBAL_ONLY);
                if (cmdPtr == NULL) {
		    pc = pc--;  /* to get back within the scope of the cmd */
		    Tcl_ResetResult(interp);
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			    "invalid command name \"",
			    Tcl_GetString(objv[0]), "\"",
			    (char *) NULL);
		    result = TCL_ERROR;
		    goto checkForCatch;
		}
		{
		    Tcl_Obj** item;
		    for (item = tosPtr;  item >= objv ; item--) {
			 item[1] = item[0];
		    }
		    tosPtr++; /* need room for new inserted objv[0] */
		}
		objc++;
                objv[0] = Tcl_NewStringObj("unknown", -1);
		Tcl_IncrRefCount(objv[0]);
	    }
		
            /*
	     * A reference to part of the stack vector itself
	     * escapes our control, so must use preserve/release
	     * to stop it from being deallocated by a recursive
	     * call to ourselves.  The extra variable is needed
	     * because all others are liable to change due to the
	     * trace procedures.
	     */
	    
	    preservedStack = eePtr->stackPtr;
	    Tcl_Preserve((ClientData) preservedStack);

	    /*
	     * Call any trace procedures.
	     */

	    if (iPtr->tracePtr != NULL) {
		Trace *tracePtr, *nextTracePtr;

		for (tracePtr = iPtr->tracePtr;  tracePtr != NULL;
		        tracePtr = nextTracePtr) {
		    nextTracePtr = tracePtr->nextPtr;
		    if (iPtr->numLevels <= tracePtr->level) {
			int numChars;
			char *cmd = GetSrcInfoForPc(pc--, codePtr, &numChars);
			if (cmd != NULL) {
			    DECACHE_STACK_INFO();
			    CallTraceProcedure(interp, tracePtr, cmdPtr,
				    cmd, numChars, objc, objv);
			    CACHE_STACK_INFO();
			    objv = tosPtr - (objc-1); /* ATTN: if stack grew, wrong ...*/

			}
		    }
		}
	    }
		
	    /*
	     * Finally, invoke the command's Tcl_ObjCmdProc. First reset
	     * the interpreter's string and object results to their
	     * default empty values since they could have gotten changed
	     * by earlier invocations.
	     */
		
	    Tcl_ResetResult(interp);

#ifdef TCL_BYTECODE_DEBUG
	    if (tclTraceExec >= 2) {
		fprintf(stdout, "%d: (%u) invoking %s\n", iPtr->numLevels,
			(unsigned int)(oldPc - codePtr->codeStart),
			Tcl_GetString(objv[0]));
	    }
#endif

	    iPtr->cmdCount++;
	    DECACHE_STACK_INFO();
	    result = (*cmdPtr->objProc)(cmdPtr->objClientData, interp,
					    objc, objv);
	    if (Tcl_AsyncReady()) {
		result = Tcl_AsyncInvoke(interp, result);
	    }
	    CACHE_STACK_INFO();


	    /*
	     * Pop the objc top stack elements and decrement their ref
	     * counts. 
	     */

	    objv = tosPtr - (objc-1); /* ATTN: if stack grew, value changed ... */
	    for (;  tosPtr >= objv;  tosPtr--) {
	        Tcl_Obj *objPtr = TOS;
		TclDecrRefCount(objPtr);
	    }

	    /*
	     * If the old stack is going to be released, it is
	     * safe to do so now, since no references to objv are
	     * going to be used from now on.
	     */
	    
	    Tcl_Release((ClientData)preservedStack);
	}
	if (result != TCL_OK) {
	    pc = pc--;  /* to get back within the scope of the cmd */
	    goto bad_return_from_invoke_or_eval;
	}
	/*
	 * If the interpreter has a non-empty string result, the
	 * result object is either empty or stale because some
	 * procedure set interp->result directly. If so, move the
	 * string result to the result object, then reset the
	 * string result.
	 */

	if (*(iPtr->result) != 0) {
	    PUSH_OBJECT(Tcl_GetObjResult(interp));
	    NEXT_INSTR;
	} else {
	    PUSH_OBJECT(iPtr->objResultPtr);
	    NEXT_INSTR;
	}
    }
	    
    _CASE(INST_EVAL_STK): /* tosPtr += 0 */
    {
	Tcl_Obj *objPtr = TOS;
	DECACHE_STACK_INFO();
	result = Tcl_EvalObjEx(interp, objPtr, 0);
	CACHE_STACK_INFO();
	TclDecrRefCount(objPtr);
	if (result != TCL_OK) {
	    tosPtr--; /* stack needs to be properly set here ! */
	    goto bad_return_from_invoke_or_eval;
	}
	pc++;
	if (*(iPtr->result) != 0) {
	    SET_TOS(Tcl_GetObjResult(interp));
	    NEXT_INSTR;
	} else {
	    SET_TOS(iPtr->objResultPtr);
	    NEXT_INSTR;
	}
    }

    _CASE(INST_EXPR_STK): /* tosPtr += 0 */
    {
	Tcl_Obj *objPtr = TOS;
	Tcl_Obj *valuePtr; 
	Tcl_ResetResult(interp);
	DECACHE_STACK_INFO();
	result = Tcl_ExprObj(interp, objPtr, &valuePtr);
	CACHE_STACK_INFO();
	if (result != TCL_OK) { 
	    goto checkForCatch; /* it will decrRefCt TOS */
	}
	TclDecrRefCount(objPtr);
	TOS = valuePtr; /* already has right refct */
	pc++;
	NEXT_INSTR;
    } 

    _CASE(INST_LOAD_SCALAR4): /* tosPtr += 1 */
    {
	int index;
	pc++;
	index = TclGetUInt4AtPtr(pc);
	pc += 4;
	goto doLoadScalar;

    _CASE(INST_LOAD_SCALAR1):
	pc++;
	index = TclGetUInt1AtPtr(pc);
        pc++;

	doLoadScalar:
	{ 
	    Tcl_Obj *valuePtr;
	    {	/* INLINING from TclGetIndexedScalar */    
		Var *compiledLocals = iPtr->varFramePtr->compiledLocals;
		Var *varPtr = &(compiledLocals[index]);		
		while (TclIsVarLink(varPtr)) varPtr = varPtr->value.linkPtr;
		if ((varPtr->tracePtr == NULL) && TclIsVarScalarDefined(varPtr)) {
		    valuePtr = varPtr->value.objPtr;
		} else {
		    /* original implementation */
		    DECACHE_STACK_INFO();
		    valuePtr = TclGetIndexedScalar(interp, index, TCL_LEAVE_ERR_MSG);
		    CACHE_STACK_INFO();
		}
	    }
	    if (valuePtr == NULL) {
		result = TCL_ERROR;
		pc--;  /* to get back within the scope of the cmd */
		goto checkForCatch;
	    }
	    PUSH_OBJECT(valuePtr);
	    NEXT_INSTR;
	}
    }

    _CASE(INST_LOAD_ARRAY_STK): /* tosPtr -= 1 */
    {
	Tcl_Obj *valuePtr, *elemPtr;

	elemPtr = POP_OBJECT(); 
	TclDecrRefCount_Q(elemPtr);
	goto doLoadStk;
	    
    _CASE(INST_LOAD_STK): 
    _CASE(INST_LOAD_SCALAR_STK): /* tosPtr += 0 */
	elemPtr = NULL;

        doLoadStk:
        {
	    Tcl_Obj *objPtr = TOS; 
	    DECACHE_STACK_INFO();
	    valuePtr = Tcl_ObjGetVar2(interp, objPtr, elemPtr, TCL_LEAVE_ERR_MSG);
	    CACHE_STACK_INFO();
       
	    if (valuePtr == NULL) {
		result = TCL_ERROR;
		goto checkForCatch;
	    } 
	    TclDecrRefCount_Q(objPtr);
	SET_TOS(valuePtr);
	pc++;
	NEXT_INSTR_Q;
    }

    _CASE(INST_LOAD_ARRAY4): /* tosPtr += 0 */
    {
	int index;
	pc++;
	index = TclGetUInt4AtPtr(pc);
	pc += 4;
	goto doLoadArray;

    _CASE(INST_LOAD_ARRAY1):
	pc++;
	index = TclGetUInt1AtPtr(pc);
	pc++;
	    
	doLoadArray:
	{
	    Tcl_Obj *elemPtr = TOS;
	    Tcl_Obj *valuePtr;

	    DECACHE_STACK_INFO();
	    valuePtr = TclGetElementOfIndexedArray(interp, index,
	            elemPtr, TCL_LEAVE_ERR_MSG);
	    CACHE_STACK_INFO();
	    if (valuePtr == NULL) {
		result = TCL_ERROR;
		pc--; /* to get back within the scope of the cmd */
		goto checkForCatch; /* will decreRefCt elemPtr at TOS */
	    }
	    TclDecrRefCount(elemPtr);
	    SET_TOS(valuePtr);
	    NEXT_INSTR;
	}
    }

    _CASE(INST_STORE_SCALAR4): /* tosPtr += 0 */
    {
	int index;
        pc++;
	index = TclGetUInt4AtPtr(pc);
	pc += 4;
	goto doStoreScalar;

    _CASE(INST_STORE_SCALAR1):
	pc++;
	index = TclGetUInt1AtPtr(pc);
	pc++;
	    
	doStoreScalar: 
	{
	    Tcl_Obj *valuePtr = TOS;
	    Tcl_Obj *value2Ptr;
	    /* INLINING from TclSetIndexedScalar */   
	    Var *compiledLocals = iPtr->varFramePtr->compiledLocals;
	    Var *varPtr = &(compiledLocals[index]);		
	    while (TclIsVarLink(varPtr)) varPtr = varPtr->value.linkPtr;
	    if ((varPtr->tracePtr == NULL) 
		&& !TclIsVarArrayDefined(varPtr)
		&& !((varPtr->flags & VAR_IN_HASHTABLE) && (varPtr->hPtr == NULL))) {
		value2Ptr = varPtr->value.objPtr; 
		TclSetVarScalarDefined(varPtr);
		if (valuePtr != value2Ptr) {        
		    varPtr->value.objPtr = valuePtr;
		    Tcl_IncrRefCount(valuePtr);      
		    if (value2Ptr != NULL) {
			TclDecrRefCount(value2Ptr);    
		    }
		}
	    } else {		 
		/* original implementation */
		DECACHE_STACK_INFO();
		value2Ptr = TclSetIndexedScalar(interp, index, valuePtr,
			TCL_LEAVE_ERR_MSG);
		CACHE_STACK_INFO();
		if (value2Ptr == NULL) {
		    result = TCL_ERROR;
		    pc--; /* to get back within the scope of the cmd */
		    goto checkForCatch; /* will decrRefCt valuePtr at TOS */			
		} else if (valuePtr != value2Ptr) {
		    Tcl_DecrRefCount(valuePtr);
		    SET_TOS(value2Ptr);
		}
	    }
	    /* REMARK: on return, TOS has correct value AND refcount!*/
	    NEXT_INSTR;
	}
    }

    _CASE(INST_APPEND_ARRAY_STK):
    {
        Tcl_Obj *valuePtr;
	Tcl_Obj *elemPtr;
	int varFlags;

	varFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE);
	goto doStoreArrayStk;

    _CASE(INST_LAPPEND_ARRAY_STK):
	varFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE | TCL_LIST_ELEMENT);
	goto doStoreArrayStk;

    _CASE(INST_STORE_ARRAY_STK): /* tosPtr -= 2 */
	varFlags = TCL_LEAVE_ERR_MSG;

    doStoreArrayStk:
	valuePtr = POP_OBJECT();
	elemPtr = POP_OBJECT(); 
	TclDecrRefCount_Q(elemPtr);	
	goto doStoreStk;

    _CASE(INST_APPEND_STK):
        varFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE);
        goto doStoreScalarStk;

    _CASE(INST_LAPPEND_STK):
	varFlags = (TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE | TCL_LIST_ELEMENT);
	goto doStoreScalarStk;

    _CASE(INST_STORE_STK): 
    _CASE(INST_STORE_SCALAR_STK): 
        varFlags = TCL_LEAVE_ERR_MSG;

    doStoreScalarStk:
	valuePtr = POP_OBJECT();
        elemPtr = NULL;

        doStoreStk:
	{ 
	    Tcl_Obj *objPtr = TOS;
	    Tcl_Obj *value2Ptr;
	    DECACHE_STACK_INFO();
	    value2Ptr = Tcl_ObjSetVar2(interp, objPtr, elemPtr, valuePtr, varFlags);
	    CACHE_STACK_INFO();
	    if (value2Ptr == NULL) {
		Tcl_DecrRefCount_Q(valuePtr);
		result = TCL_ERROR;
		goto checkForCatch; /* will decrRefCt objPtr at TOS */	
	    } else if (valuePtr != value2Ptr) {
		Tcl_DecrRefCount_Q(valuePtr);
		Tcl_IncrRefCount(value2Ptr);
	    }
	    TclDecrRefCount_Q(objPtr);
	    TOS = value2Ptr;
	}
	pc++;
	NEXT_INSTR_Q;
    }

    _CASE(INST_STORE_ARRAY4): /* tosPtr += 1 */
    {
        int index;
	pc++;
	index = TclGetUInt4AtPtr(pc);
	pc += 4;
	goto doStoreArray;

    _CASE(INST_STORE_ARRAY1): /* tosPtr += 1 */
	pc++;
	index = TclGetUInt1AtPtr(pc);
        pc++;
	    
	doStoreArray:
	{
	    Tcl_Obj *valuePtr = POP_OBJECT();
	    Tcl_Obj *elemPtr = TOS;
	    Tcl_Obj *value2Ptr;

	    DECACHE_STACK_INFO();
	    value2Ptr = TclSetElementOfIndexedArray(interp, index,
		    elemPtr, valuePtr, TCL_LEAVE_ERR_MSG);
	    CACHE_STACK_INFO();
	    if (value2Ptr == NULL) {
		tosPtr++; /* let checkForCatch decrRefCt elemPtr and valuePtr */
		result = TCL_ERROR;
		pc--; /* to get back within the scope of the cmd */
		goto checkForCatch;
	    } else if (value2Ptr != valuePtr) {
	        Tcl_DecrRefCount(valuePtr);
		Tcl_IncrRefCount(value2Ptr);
	    }
	    TclDecrRefCount(elemPtr);
	    TOS = value2Ptr;
	    NEXT_INSTR;
	}
    }

    /*
     * START APPEND INSTRUCTIONS
     */
    
    _CASE(INST_APPEND_SCALAR4):
    {
	int opnd;
	pc++;
	opnd = TclGetUInt4AtPtr(pc);
	pc += 4;
	goto doAppendScalar;

    _CASE(INST_APPEND_SCALAR1):
	pc++;
	opnd = TclGetUInt1AtPtr(pc);
        pc++;

        doAppendScalar:
	{
	    Tcl_Obj *valuePtr, *value2Ptr;

	    valuePtr = TOS;
	    DECACHE_STACK_INFO();
	    value2Ptr = TclSetIndexedScalar(interp, opnd, valuePtr,
			 TCL_LEAVE_ERR_MSG | TCL_APPEND_VALUE);
	    CACHE_STACK_INFO();
	    if (value2Ptr == NULL) {
		pc--; /* to get back within the scope of the cmd */
		result = TCL_ERROR;
		goto checkForCatch;
	    }
	    TclDecrRefCount(valuePtr);
	    SET_TOS(value2Ptr);
	    NEXT_INSTR;
	}
    }

    _CASE(INST_APPEND_ARRAY4):
    {
	int opnd;
	pc++;
	opnd = TclGetUInt4AtPtr(pc);
	pc += 4;
	goto doAppendArray;

    _CASE(INST_APPEND_ARRAY1):
	pc++;
	opnd = TclGetUInt1AtPtr(pc);
        pc++;
	    
        doAppendArray:
	{
	    Tcl_Obj *valuePtr, *value2Ptr, *elemPtr;
	    valuePtr = POP_OBJECT();
	    elemPtr = TOS;
	    DECACHE_STACK_INFO();
	    value2Ptr = TclSetElementOfIndexedArray(interp, opnd,
		    elemPtr, valuePtr, TCL_LEAVE_ERR_MSG|TCL_APPEND_VALUE);
	    CACHE_STACK_INFO();
	    if (value2Ptr == NULL) {
		pc--; /* to get back within the scope of the cmd */
		tosPtr++; /* to get checkForCatch to DecrRefCount of both */
		result = TCL_ERROR;
		goto checkForCatch;
	    }
	    TclDecrRefCount(elemPtr);
	    TclDecrRefCount(valuePtr);
	    SET_TOS(value2Ptr);
	    NEXT_INSTR;
	}
    }
    /*
     * END APPEND INSTRUCTIONS
     */

    /*
     * START LAPPEND INSTRUCTIONS
     */

    _CASE(INST_LAPPEND_SCALAR4):
    {
	int opnd;
	pc++;
	opnd = TclGetUInt4AtPtr(pc);
	pc += 4;
	goto doLappendScalar;

    _CASE(INST_LAPPEND_SCALAR1):
	pc++;
	opnd = TclGetUInt1AtPtr(pc);
        pc += 1;

    doLappendScalar:
	{
	    Tcl_Obj *valuePtr, *value2Ptr;
	    valuePtr = TOS;
	    DECACHE_STACK_INFO();
	    value2Ptr = TclSetIndexedScalar(interp, opnd, valuePtr,
	            TCL_LEAVE_ERR_MSG|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);
	    CACHE_STACK_INFO();
	    if (value2Ptr == NULL) {
		pc--; /* to get back within the scope of the cmd */
		result = TCL_ERROR;
		goto checkForCatch;
	    }
	    TclDecrRefCount(valuePtr);
	    SET_TOS(value2Ptr);
	    NEXT_INSTR;
	}
    }

    _CASE(INST_LAPPEND_ARRAY4):
    {
	int opnd;
	pc++;
	opnd = TclGetUInt4AtPtr(pc);
	pc += 4;
	goto doLappendArray;

    _CASE(INST_LAPPEND_ARRAY1):
	pc++;
	opnd = TclGetUInt1AtPtr(pc);
	pc++;

        doLappendArray:
	{
	    Tcl_Obj *valuePtr, *value2Ptr, *elemPtr;

	    valuePtr = POP_OBJECT();
	    elemPtr = TOS;
	    DECACHE_STACK_INFO();
	    value2Ptr = TclSetElementOfIndexedArray(interp, opnd,
		    elemPtr, valuePtr,
		    TCL_LEAVE_ERR_MSG|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);
	    CACHE_STACK_INFO();
	    if (value2Ptr == NULL) {
		pc--; /* to get back within the scope of the cmd */
		tosPtr++; /* to get checkForCatch to DecrRefCount of both */		
		result = TCL_ERROR;
		goto checkForCatch;
	    }
	    TclDecrRefCount(elemPtr);
	    TclDecrRefCount(valuePtr);
	    SET_TOS(value2Ptr);
	    NEXT_INSTR;
	}
    }
    /*
     * END (L)APPEND INSTRUCTIONS
     */


    _CASE(INST_INCR_SCALAR1): /* tosPtr += 0 */
    {
	long i;
	int index;
	{
	    Tcl_Obj *valuePtr = POP_OBJECT();

	    if (valuePtr->typePtr != &tclIntType) {
	        result = tclIntType.setFromAnyProc(interp, valuePtr);
	        if (result != TCL_OK) {
		    tosPtr++; /* it will decrRefCt valuePtr */
		    goto checkForCatch; 
		}
	    }
	    i = valuePtr->internalRep.longValue;
	    TclDecrRefCount(valuePtr);
	}
	pc++;
	index = TclGetUInt1AtPtr(pc);
	goto doIncrScalar;

    _CASE(INST_INCR_SCALAR1_IMM): /* tosPtr += 1 */
	pc++;
        index = TclGetUInt1AtPtr(pc);
	pc++;
	i = TclGetInt1AtPtr(pc);

        doIncrScalar:
	{
	    Tcl_Obj *valuePtr; 
            /* INLINING from TclIncrIndexedScalar */
	    Var *compiledLocals = iPtr->varFramePtr->compiledLocals;
	    Var *varPtr = &(compiledLocals[index]);		
	    while (TclIsVarLink(varPtr)) varPtr = varPtr->value.linkPtr;

	    if ((varPtr->tracePtr == NULL) && TclIsVarScalarDefined(varPtr)
		&& !((varPtr->flags & VAR_IN_HASHTABLE) && (varPtr->hPtr == NULL))) {
		long currVal;
		valuePtr = varPtr->value.objPtr;
		result = Tcl_GetLongFromObj(interp, valuePtr, &currVal);
		if (result != TCL_OK) goto doIncrScalarErrExit; 
		if (Tcl_IsShared(valuePtr)) {
		    (valuePtr->refCount)--;  
		    valuePtr = Tcl_NewLongObj (i + currVal);
		    Tcl_IncrRefCount(valuePtr);
		} else {
		    valuePtr->internalRep.longValue = i + currVal; 
		    Tcl_InvalidateStringRep(valuePtr);
		}
		varPtr->value.objPtr = valuePtr;
	    } else {
		/* original implementation */
		DECACHE_STACK_INFO();
		valuePtr = TclIncrIndexedScalar(interp, index, i);
		CACHE_STACK_INFO();
		if (valuePtr == NULL) goto doIncrScalarErrExit; 
	    }
	    PUSH_OBJECT(valuePtr);
	    pc++;
	    NEXT_INSTR;

	doIncrScalarErrExit:
	    result = TCL_ERROR;
	    goto checkForCatch;
	}
    }

    _CASE(INST_INCR_ARRAY_STK): /* tosPtr -= 2 */
    {
	Tcl_Obj *elemPtr;
	long i;
	{
	    Tcl_Obj *valuePtr;

	    valuePtr = POP_OBJECT();
	    elemPtr = POP_OBJECT();
	    TclDecrRefCount_Q(elemPtr);

	    goto doIncrStk;

    _CASE(INST_INCR_SCALAR_STK): /* tosPtr -= 1 */
    _CASE(INST_INCR_STK): 
	    valuePtr = POP_OBJECT();
	    elemPtr = NULL;

            doIncrStk:
	    TclDecrRefCount_Q(valuePtr);
	    if (valuePtr->typePtr != &tclIntType) {
	        result = tclIntType.setFromAnyProc(interp, valuePtr);
	        if (result != TCL_OK) {
		    goto checkForCatch;
		}
	    }
	    i = valuePtr->internalRep.longValue;
	}
	goto doIncrStkImm;

    _CASE(INST_INCR_ARRAY_STK_IMM): /* tosPtr -= 1 */
	elemPtr = POP_OBJECT();
        TclDecrRefCount_Q(elemPtr);
	pc++;
	i = TclGetInt1AtPtr(pc);
	goto doIncrStkImm;

    _CASE(INST_INCR_SCALAR_STK_IMM): /* tosPtr += 0 */
    _CASE(INST_INCR_STK_IMM): 
	elemPtr = NULL;
	pc++;
	i = TclGetInt1AtPtr(pc);

        doIncrStkImm:
	{   
	    Tcl_Obj *value2Ptr;
	    Tcl_Obj *objPtr = TOS; /* variable or array name */
	    DECACHE_STACK_INFO();
	    value2Ptr = TclIncrVar2(interp, objPtr, elemPtr, i, 
				    TCL_LEAVE_ERR_MSG);
	    CACHE_STACK_INFO();
	    if (value2Ptr == NULL) {
		result = TCL_ERROR;
		goto checkForCatch; /* will decrRefCt objPtr = TOS */
	    }
	    TclDecrRefCount_Q(objPtr);
	    SET_TOS(value2Ptr);
	    pc++;
	    NEXT_INSTR_Q;
	}
    }


    _CASE(INST_INCR_ARRAY1): /* tosPtr -= 1 */
    {
        long i;
	int index;

	{
	    Tcl_Obj *valuePtr = POP_OBJECT();

	    if (valuePtr->typePtr != &tclIntType) {
	        result = tclIntType.setFromAnyProc(interp, valuePtr);
	        if (result != TCL_OK) {
		    tosPtr++; /* will decrRefCount valuePtr */
		    goto checkForCatch;
		}
	    }
	    i = valuePtr->internalRep.longValue;
	    TclDecrRefCount(valuePtr);
	}
	pc++;
	index = TclGetUInt1AtPtr(pc);
	goto doIncrArray1;

    _CASE(INST_INCR_ARRAY1_IMM): /* tosPtr += 0 */
	pc++;
	index = TclGetUInt1AtPtr(pc);
	pc++;
	i = TclGetInt1AtPtr(pc);

        doIncrArray1:
	{ 
	    Tcl_Obj *elemPtr = TOS;
	    Tcl_Obj *value2Ptr;
	    DECACHE_STACK_INFO();
	    value2Ptr = TclIncrElementOfIndexedArray(interp, 
                    index, elemPtr, i);
	    CACHE_STACK_INFO();
	    if (value2Ptr == NULL) {
		result = TCL_ERROR;
		goto checkForCatch; /* will decrRefCt elemPtr = TOS */
	    }
	    TclDecrRefCount(elemPtr);
	    SET_TOS(value2Ptr);
	    pc++;
	    NEXT_INSTR;
	}
    }

    _CASE(INST_JUMP1): /* tosPtr += 0 */
    {
	pc += TclGetInt1AtPtr(pc+1); 
	NEXT_INSTR;
    }

    _CASE(INST_JUMP4): /* tosPtr += 0 */
    {
	pc += TclGetInt4AtPtr(pc+1); 
	NEXT_INSTR;
    }

    _CASE(INST_JUMP_FALSE4): /* tosPtr += 0 */
    {
        /* 
	 * adj0 is the pcAdjustment for "false"
	 * adj1 is the pcAdjustment for "true"
	 */
	int adj0 = TclGetInt4AtPtr(pc+1);
	int adj1 = 5;
	goto doJumpTrue;

    _CASE(INST_JUMP_FALSE1):
	adj0 = TclGetInt1AtPtr(pc+1);
	adj1 = 2;
	goto doJumpTrue;

    _CASE(INST_JUMP_TRUE4): 
	adj1 = TclGetInt4AtPtr(pc+1);
	adj0 = 5;
	goto doJumpTrue;

    _CASE(INST_JUMP_TRUE1):
	adj1 = TclGetInt1AtPtr(pc+1);
	adj0 = 2;
	    
	doJumpTrue: 
	{
	    Tcl_Obj *valuePtr = POP_OBJECT();
	    Tcl_ObjType *typePtr = valuePtr->typePtr;
	    int truth;

	    if (typePtr == &tclIntType) {
		truth = valuePtr->internalRep.longValue;
	    } else if (typePtr == &tclDoubleType) {
		truth = (valuePtr->internalRep.doubleValue != 0.0);
	    } else {
		result = Tcl_GetBooleanFromObj(interp, valuePtr, &truth);
		if (result != TCL_OK) {
		    tosPtr++; /* to decrRefCt valuePtr */
		    goto checkForCatch;
		}
	    }
	    TclDecrRefCount(valuePtr);
	    pc += (truth ? adj1 : adj0);
	    NEXT_INSTR;
	}
    }

	    
    _CASE(INST_LOR): /* tosPtr -= 1 */
    _CASE(INST_LAND):
    {
        /*
	 * Operands must be boolean or numeric. No int->double
	 * conversions are performed.
	 */
		
	int i1, i2;
		
	{
	    Tcl_Obj *value2Ptr = POP_OBJECT();
	    Tcl_ObjType *t2Ptr = value2Ptr->typePtr;

	    if ((t2Ptr == &tclIntType) || (t2Ptr == &tclBooleanType)) {
	        i2 = (value2Ptr->internalRep.longValue != 0);
	    } else if (t2Ptr == &tclDoubleType) {
	        i2 = (value2Ptr->internalRep.doubleValue != 0.0);
	    } else {
		if (TclLooksLikeInt(TclGetString(value2Ptr), value2Ptr->length)) {
	            long i;
		    result = Tcl_GetLongFromObj((Tcl_Interp *) NULL,
			    value2Ptr, &i);
		    i2 = (int) i;
		} else {
		    int i;
		    result = Tcl_GetBooleanFromObj((Tcl_Interp *) NULL,
		            value2Ptr, &i);
		    i2 = i;
		}
		if (result != TCL_OK) {
		    tosPtr++; /* to decrRefCt value2Ptr */
		    IllegalExprOperandType(interp, pc, value2Ptr);
		    goto checkForCatch;
		}
	    }
	    TclDecrRefCount(value2Ptr);
	}

	{
	    Tcl_Obj *valuePtr  = TOS;
	    Tcl_ObjType *t1Ptr = valuePtr->typePtr;
	    if ((t1Ptr == &tclIntType) || (t1Ptr == &tclBooleanType)) {
	        i1 = (valuePtr->internalRep.longValue != 0);
	    } else if (t1Ptr == &tclDoubleType) {
	        i1 = (valuePtr->internalRep.doubleValue != 0.0);
	    } else {
		if (TclLooksLikeInt(TclGetString(valuePtr), valuePtr->length)) {
		    long i;
	            result = Tcl_GetLongFromObj((Tcl_Interp *) NULL,
		        valuePtr, &i);
		    i1 = (int) i;
		} else {
		    int i;
		    result = Tcl_GetBooleanFromObj((Tcl_Interp *) NULL,
			    valuePtr, &i);
		    i1 = i;
		}
		if (result != TCL_OK) {
		    IllegalExprOperandType(interp, pc, valuePtr);
		    goto checkForCatch;
		}
	    }
	}
				
	/*
	 * Reuse the valuePtr object already on stack if possible.
	 */
	{
	    int i = (*pc++ == INST_LOR) ? (i1 || i2) : (i1 && i2);
	    USE_OR_MAKE_THEN_SET(i, Long);
	}
	NEXT_INSTR;
    }

    _CASE(INST_LIST): /* Placeholder to avoid compiler error */

    _CASE(INST_LIST_LENGTH):
    {
	Tcl_Obj *valuePtr = POP_OBJECT();
	int length;
	
	result = Tcl_ListObjLength(interp, valuePtr, &length);
	if (result != TCL_OK) {
	    TclDecrRefCount(valuePtr);
	    goto checkForCatch;
	}
	PUSH_OBJECT(Tcl_NewIntObj(length));
	pc++;
	NEXT_INSTR;
    }

    _CASE(INST_LIST_INDEX):
    {
	Tcl_Obj **elemPtrs, *value2Ptr, *objPtr, *valuePtr;
	int index, length;

	value2Ptr = POP_OBJECT();
	valuePtr  = POP_OBJECT();

	result = Tcl_ListObjGetElements(interp, valuePtr,
		      &length, &elemPtrs);
	if (result != TCL_OK) {
	    TclDecrRefCount(value2Ptr);
	    TclDecrRefCount(valuePtr);
	    goto checkForCatch;
	}

	result = TclGetIntForIndex(interp, value2Ptr, length - 1,
				   &index);
	if (result != TCL_OK) {
	    Tcl_DecrRefCount(value2Ptr);
	    Tcl_DecrRefCount(valuePtr);
	    goto checkForCatch;
	}

	if ((index < 0) || (index >= length)) {
	    objPtr = Tcl_NewObj();
	} else {
	    /*
	     * Make sure listPtr still refers to a list object. It
	     * might have been converted to an int above if the
	     * argument objects were shared.
	     */
	    
	    if (valuePtr->typePtr != &tclListType) {
		result = Tcl_ListObjGetElements(interp, valuePtr,
						&length, &elemPtrs);
		if (result != TCL_OK) {
		    TclDecrRefCount(value2Ptr);
		    TclDecrRefCount(valuePtr);
		    goto checkForCatch;
		}
	    }
	    objPtr = elemPtrs[index];
	}

	PUSH_OBJECT(objPtr);
	TclDecrRefCount(valuePtr);
	TclDecrRefCount(value2Ptr);
    }
    pc++;
    NEXT_INSTR;

    _CASE(INST_STR_EQ): /* tosPtr -= 1 */
    _CASE(INST_STR_NEQ): 
    {
	/*
	 * String (in)equality check
	 */

	Tcl_Obj *value2Ptr = POP_OBJECT();
	Tcl_Obj *valuePtr  = TOS;
	int iResult;

	if (valuePtr == value2Ptr) {
	  /*
	   * On the off-chance that the objects are the same,
	   * we don't really have to think hard about equality.
	   */
	  iResult = (*pc == INST_STR_EQ);
	} else {
	    char *str1 = TclGetString(valuePtr);
	    char *str2 = TclGetString(value2Ptr);
	  if (valuePtr->length == value2Ptr->length) {
	    /*
	     * We only need to check (in)equality when we have equal
	     * length strings.
	     */
	    int tmp = (strcmp(str1, str2));
	    iResult = ((*pc == INST_STR_NEQ) ? (tmp != 0) : (tmp == 0));
	  } else {
	    iResult = (*pc == INST_STR_NEQ);
	  }
	}
	TclDecrRefCount(value2Ptr);
	USE_OR_MAKE_THEN_SET(iResult,Int);
	pc++;
	NEXT_INSTR;
    }

    _CASE(INST_STR_CMP): /* tosPtr -= 1 */
    {
	/*
	 * String compare
	 */
	char *s1, *s2;
	int s1len, s2len, iResult;
	Tcl_Obj *valuePtr, *value2Ptr;
	
	value2Ptr = POP_OBJECT();
	valuePtr  = POP_OBJECT();

	/*
	 * The comparison function should compare up to the
	 * minimum byte length only.
	 */
	if ((valuePtr->typePtr == &tclByteArrayType) &&
	    (value2Ptr->typePtr == &tclByteArrayType)) {
	    s1 = Tcl_GetByteArrayFromObj(valuePtr, &s1len);
	    s2 = Tcl_GetByteArrayFromObj(value2Ptr, &s2len);
	    iResult = memcmp(s1, s2,
			     (size_t) ((s1len < s2len) ? s1len : s2len));
	} else {
#if 0
	    /*
	     * This solution is less mem intensive, but it is
	     * computationally expensive as the string grows.  The
	     * reason that we can't use a memcmp is that UTF-8 strings
	     * that contain a \u0000 can't be compared with memcmp.  If
	     * we knew that the string was ascii-7 or had no null byte,
	     * we could just do memcmp and save all the hassle.
	     */
	    s1 = Tcl_GetStringFromObj(valuePtr, &s1len);
	    s2 = Tcl_GetStringFromObj(value2Ptr, &s2len);
	    iResult = Tcl_UtfNcmp(s1, s2,
				  (size_t) ((s1len < s2len) ? s1len : s2len));
#else
	    /*
	     * The alternative is to break this into more code
	     * that does type sensitive comparison, as done in
	     * Tcl_StringObjCmd.
	     */
	    Tcl_UniChar *uni1, *uni2;
	    uni1 = Tcl_GetUnicodeFromObj(valuePtr, &s1len);
	    uni2 = Tcl_GetUnicodeFromObj(value2Ptr, &s2len);
	    iResult = Tcl_UniCharNcmp(uni1, uni2,
				      (unsigned) ((s1len < s2len) ? s1len : s2len));
#endif
	}

	/*
	 * Make sure only -1,0,1 is returned
	 */
	if (iResult == 0) {
	    iResult = s1len - s2len;
	}
	if (iResult < 0) {
	    iResult = -1;
	} else if (iResult > 0) {
	    iResult = 1;
	}
	
	PUSH_OBJECT(Tcl_NewIntObj(iResult));
	TclDecrRefCount(valuePtr);
	TclDecrRefCount(value2Ptr);
    }
    pc++;
    NEXT_INSTR;

    _CASE(INST_STR_LEN): /* tosPtr += 0 */
    {
	 int length1;		 
	 Tcl_Obj *valuePtr = TOS;

	 /* INLINING from Tcl_GetByteArrayFromObj (tclBinary.c) */
	 length1 = ((valuePtr->typePtr == &tclByteArrayType) ?
		    (((ByteArray *) (valuePtr)->internalRep.otherValuePtr)->used) :
		    Tcl_GetCharLength(valuePtr));
	 USE_OR_MAKE_THEN_SET(length1,Int);
	 pc++;
	 NEXT_INSTR;
    }
	    
    _CASE(INST_STR_INDEX): /* tosPtr -= 1 */
	{
	    Tcl_Obj *idxPtr = POP_OBJECT(); /* the index to look for */
	    Tcl_Obj *stringPtr  = TOS;      /* the string object */
	    Tcl_Obj *objPtr;
	    int index, length;

	    /*
	     * If we have a ByteArray object, avoid indexing in the
	     * Utf string since the byte array contains one byte per
	     * character.  Otherwise, use the Unicode string rep to
	     * get the index'th char.
	     */

	    if (stringPtr->typePtr == &tclByteArrayType) {
		/* INLINING from Tcl_GetByteArrayFromObj (tclBinary.c) */
		unsigned char *bytes;
		{
		    ByteArray *byteArr = (ByteArray *) (stringPtr)->internalRep.otherValuePtr;
		    bytes  = byteArr->bytes;
		    length = byteArr->used;
		}

		if (idxPtr->typePtr == &tclIntType) {
		    index = (int) idxPtr->internalRep.longValue;
		} else {
		    result = TclGetIntForIndex(interp, idxPtr, length - 1,
					       &index);
		    if (result != TCL_OK) {
			tosPtr++; /* to decrRefCt idxPtr */
			goto checkForCatch;
		    }
		}
		if ((index >= 0) && (index < length)) {
		    objPtr = Tcl_NewByteArrayObj(&bytes[index], 1);
		} else {
		    objPtr = Tcl_NewObj();
		}
	    } else {
		/*
		 * Get Unicode char length to calculate what 'end' means.
		 */
		length = Tcl_GetCharLength(stringPtr);

		result = TclGetIntForIndex(interp, idxPtr, length - 1,
					   &index);
		if (result != TCL_OK) {
		    tosPtr++; /* to decrRefCt idxPtr */
		    goto checkForCatch;
		}

		if ((index >= 0) && (index < length)) {
		    char buf[TCL_UTF_MAX];
		    Tcl_UniChar ch;

		    ch     = Tcl_GetUniChar(stringPtr, index);
		    length = Tcl_UniCharToUtf(ch, buf);
		    objPtr = Tcl_NewStringObj(buf, length);
		} else {
		    objPtr = Tcl_NewObj();
		}
	    }
	    TclDecrRefCount(stringPtr);
	    TclDecrRefCount(idxPtr);
	    SET_TOS(objPtr);
	    pc++;
	    NEXT_INSTR;
	}
    }

    _CASE(INST_STR_MATCH): /* tosPtr -= 2 */
    {
	 int nocase;
	 int match;
	 Tcl_Obj *valuePtr  = POP_OBJECT();	/* String */
	 Tcl_Obj *value2Ptr = POP_OBJECT();	/* Pattern */
	 Tcl_Obj *objPtr = TOS;	        /* Case Sensitivity */

	 Tcl_GetBooleanFromObj(interp, objPtr, &nocase); 
	 match = Tcl_UniCharCaseMatch(Tcl_GetUnicode(valuePtr),
		 Tcl_GetUnicode(value2Ptr), nocase);

	 TclDecrRefCount(valuePtr);
	 TclDecrRefCount(value2Ptr);
	 USE_OR_MAKE_THEN_SET(match,Int);
	 pc++;
	 NEXT_INSTR;
    }

    _CASE(INST_EQ):  /* tosPtr -= 1 */
    _CASE(INST_NEQ):
    _CASE(INST_LT):
    _CASE(INST_GT):
    _CASE(INST_LE):
    _CASE(INST_GE): 
    {
	 /*
	  * Any type is allowed but the two operands must have the
	  * same type. We will compute value op value2.
	  */

	 long iResult = 0;  /* Init. avoids compiler warning. */
	 union AuxVar A, B;
	 Tcl_Obj *valueBPtr = POP_OBJECT();
	 Tcl_Obj *valueAPtr = TOS;
	 Tcl_ObjType *tAPtr = valueAPtr->typePtr;
	 Tcl_ObjType *tBPtr = valueBPtr->typePtr;

	 /*
	  * We only want to coerce numeric validation if
	  * neither type is NULL.  A NULL type means the arg is
	  * essentially an empty object ("", {} or [list]).
	  */

	 if (!((((tAPtr == NULL) && (valueAPtr->bytes == NULL))
		    || (valueAPtr->bytes && (valueAPtr->length == 0)))
		    || (((tBPtr == NULL) && (valueBPtr->bytes == NULL))
			    || (valueBPtr->bytes && (valueBPtr->length == 0))))) {
	     TRY_CONVERT_TO_NUM(valueAPtr,A,tAPtr);
	     TRY_CONVERT_TO_NUM(valueBPtr,B,tBPtr);
	 }


         if ((tAPtr == &tclIntType) && (tBPtr == &tclIntType)) {
	     /* Compare as ints. */
	     switch (*pc) {
	     case INST_EQ:
	         iResult = (A.i == B.i);
		 break;
	     case INST_NEQ:
	         iResult = (A.i != B.i);
		 break;
	     case INST_LT:
	         iResult = (A.i < B.i);
		 break;
	     case INST_GT:
	         iResult = (A.i > B.i);
		 break;
	     case INST_LE:
	         iResult = (A.i <= B.i);
		 break;
	     case INST_GE:
	         iResult = (A.i >= B.i);
		 break;
	     }
	 } else if ((tAPtr == &tclDoubleType) && (tBPtr == &tclIntType)) {	 
	     B.d = (double) B.i;
	     goto compare_as_doubles;
	     /* UGLY, but effective ... */
	 } else if ((tAPtr == &tclIntType) && (tBPtr == &tclDoubleType)) {
	     A.d = (double) A.i;
	     goto compare_as_doubles;
	     /* UGLY, but effective ... */
	 } else if ((tAPtr == &tclDoubleType) && (tBPtr == &tclDoubleType)) {
	     compare_as_doubles:
	     switch (*pc) {
	     case INST_EQ:
	         iResult = (A.d == B.d);
		 break;
	     case INST_NEQ:
	         iResult = (A.d != B.d);
		 break;
	     case INST_LT:
	         iResult = (A.d < B.d);
		 break;
	     case INST_GT:
	         iResult = (A.d > B.d);
		 break;
	     case INST_LE:
	         iResult = (A.d <= B.d);
		 break;
	     case INST_GE:
	         iResult = (A.d >= B.d);
		 break;
	     }
	 } else {
	     /* One operand is not numeric. Compare as strings. */
	      int cmpValue;
	      cmpValue = strcmp(TclGetString(valueAPtr), TclGetString(valueBPtr));
	      switch (*pc) {
	      case INST_EQ:
		  iResult = (cmpValue == 0);
		  break;
	      case INST_NEQ:
		  iResult = (cmpValue != 0);
		  break;
	      case INST_LT:
		  iResult = (cmpValue < 0);
		  break;
	      case INST_GT:
		  iResult = (cmpValue > 0);
		  break;
	      case INST_LE:
		  iResult = (cmpValue <= 0);
		  break;
	      case INST_GE:
		  iResult = (cmpValue >= 0);
		  break;
	      }
	 }

	 /*
	  * Reuse the valuePtr object already on stack if possible.
	  */
		
	 TclDecrRefCount(valueBPtr);
	 USE_OR_MAKE_THEN_SET(iResult,Long);
	 pc++;
	 NEXT_INSTR;
    }
	    
    _CASE(INST_MOD):      /* tosPtr -= 1 */
    _CASE(INST_LSHIFT):
    _CASE(INST_RSHIFT):
    _CASE(INST_BITOR):
    _CASE(INST_BITXOR):
    _CASE(INST_BITAND): 
    {
	/*
	 * Only integers are allowed. We compute value op value2.
	 */

	long i1, i2;
	{
	    Tcl_Obj *value2Ptr = POP_OBJECT();	  
	    if (value2Ptr->typePtr == &tclIntType) {
	        i2 = value2Ptr->internalRep.longValue;
	    } else {
	        long i;
	        result = Tcl_GetLongFromObj((Tcl_Interp *) NULL,
		        value2Ptr, &i);
		if (result != TCL_OK) {
		    IllegalExprOperandType(interp, pc, value2Ptr);
		    tosPtr++; /* it will decrRefCt value2Ptr */
		    goto checkForCatch;
		} else {
		    i2 = i;
		}
	    }
	    TclDecrRefCount(value2Ptr);
	}
	{
	    Tcl_Obj *valuePtr  = TOS; 
	    if (valuePtr->typePtr == &tclIntType) {
	        i1 = valuePtr->internalRep.longValue;
	    } else {	/* try to convert to int */
	        long i;
		result = Tcl_GetLongFromObj((Tcl_Interp *) NULL,
		        valuePtr, &i);
		if (result != TCL_OK) {
		    IllegalExprOperandType(interp, pc, valuePtr);
		    goto checkForCatch;
		} else {
		    i1 = i;
		}
	    }
	}
	{
	    long iResult = 0; /* Init. avoids compiler warning. */		
	    switch (*pc) {
	    case INST_MOD:
		/*
		 * This code is tricky: C doesn't guarantee much about
		 * the quotient or remainder, but Tcl does. The
		 * remainder always has the same sign as the divisor and
		 * a smaller absolute value.
		 */
		if (i2 == 0) {
		    goto divideByZero;
		}
		if (i2 < 0) {
		    iResult = i1 % (-i2);
		    if (iResult > 0) {
		        iResult += i2;
		    }		    
		} else {
		    iResult = i1 % i2;
		    if (iResult < 0) {
		      iResult += i2;
		    }
		}
		break;
	    case INST_LSHIFT:
		iResult = i1 << i2;
		break;
	    case INST_RSHIFT:
		/*
		 * The following code is a bit tricky: it ensures that
		 * right shifts propagate the sign bit even on machines
		 * where ">>" won't do it by default.
		 */
		if (i1 < 0) {
		    iResult = ~((~i1) >> i2);
		} else {
		    iResult = i1 >> i2;
		}
		break;
	    case INST_BITOR:
		iResult = i1 | i2;
		break;
	    case INST_BITXOR:
		iResult = i1 ^ i2;
		break;
	    case INST_BITAND:
		iResult = i1 & i2;
		break;
	    }

	/*
	 * Reuse the valuePtr object already on stack if possible.
	 */
	
	    USE_OR_MAKE_THEN_SET(iResult,Long);
	    pc++;	
	    NEXT_INSTR;
	}
    }

    _CASE(INST_ADD): /* tosPtr -= 1 */
    _CASE(INST_SUB):
    _CASE(INST_MULT):
    _CASE(INST_DIV): 
    {
	 /*
	  * Operands must be numeric and ints get converted to floats
	  * if necessary. We compute value op value2.
	  */

	 Tcl_ObjType *tAPtr, *tBPtr;
	 union AuxVar A, B;
	 {
	     Tcl_Obj *valueBPtr = POP_OBJECT();
	     tBPtr = valueBPtr->typePtr;
	     TRY_CONVERT_TO_NUM(valueBPtr,B,tBPtr);
	     TclDecrRefCount_Q(valueBPtr);
	 }
	 {
	     Tcl_Obj *valueAPtr = *tosPtr;
	     tAPtr = valueAPtr->typePtr;
	     TRY_CONVERT_TO_NUM(valueAPtr,A,tAPtr);
	 }
	 {
	     union AuxVar R;

	     if ((tAPtr == &tclIntType) && (tBPtr == &tclIntType)) { 
		 /* Do integer arithmetic. */
		 switch (*pc++) {
		 case INST_ADD:
		     R.i = A.i + B.i;
		     break;
		 case INST_SUB:
		     R.i = A.i - B.i;
		     break;
		 case INST_MULT:
		     R.i = A.i * B.i;
		     break;
		 case INST_DIV:
		     /*
		      * This code is tricky: C doesn't guarantee much
		      * about the quotient or remainder, but Tcl does.
		      * The remainder always has the same sign as the
		      * divisor and a smaller absolute value.
		      */
		     if (B.i == 0) {
			 goto divideByZero;
		     }
		     if (B.i < 0) {
			 A.i = -A.i;
			 B.i = -B.i;
		     }
		     R.i = A.i / B.i;
		     if (A.i % B.i < 0) {
			 R.i -= 1;
		     }
		     break;
		 }
		 /* Reuse the valuePtr object already on stack if possible. */
		 USE_OR_MAKE_THEN_SET(R.i,Long);
		 NEXT_INSTR_Q;
	     } else if ((tAPtr == &tclDoubleType) && (tBPtr == &tclIntType)) {
		 B.d = (double) B.i;      /* promote value B to double */
		 goto do_double_arithmetic;
		 /* UGLY, but effective ... */
	     } else if ((tAPtr == &tclIntType) && (tBPtr == &tclDoubleType)) {
		 A.d = (double) A.i;       /* promote value A to double */
		 goto do_double_arithmetic;
		 /* UGLY, but effective ... */
	     } else if ((tAPtr == &tclDoubleType) && (tBPtr == &tclDoubleType)) {
	     do_double_arithmetic:
		 switch (*pc++) {
		 case INST_ADD:
		     R.d = A.d + B.d;
		     break;
		 case INST_SUB:
		     R.d = A.d - B.d;
		     break;
		 case INST_MULT:
		     R.d = A.d * B.d;
		     break;
		 case INST_DIV:
		     if (B.d == 0.0) {
			 goto divideByZero;
		     }
		     R.d = A.d / B.d;
		     break;
		 }
		    
		 /*
		  * Check now for IEEE floating-point error.
		  */
		    
		 if (IS_NAN(R.d) || IS_INF(R.d)) {
		     TclExprFloatError(interp, R.d);
		     result = TCL_ERROR;
		     goto checkForCatch;
		 }
		 /* Reuse the valuePtr object already on stack if possible. */
		 USE_OR_MAKE_THEN_SET(R.d,Double);
		 NEXT_INSTR_Q;
	     } else {
		 /* 
		  * at least one operand is not numeric: ERROR 
		  */

		 if ((tAPtr != &tclIntType) && (tAPtr != &tclDoubleType)) {
		     IllegalExprOperandType(interp, pc, *tosPtr);
		 } else {
		     /* THIS is why we need to queue the decrRefCts! */
		     IllegalExprOperandType(interp, pc, *(tosPtr+1));
		 }
		 result = TCL_ERROR;
		 goto checkForCatch;
	     }
	 }
    }
	    
    _CASE(INST_UPLUS): /* tosPtr += 0 */
    {
	/*
	 * Operand must be numeric.
	 */

	Tcl_Obj *valuePtr = TOS;
	Tcl_ObjType *tPtr = valuePtr->typePtr;
	union AuxVar A;

	if (valuePtr->bytes != NULL) {
	    TRY_CONVERT_TO_NUM(valuePtr,A,tPtr);
	}
	
	/*
	 * Ensure that the operand's string rep is the same as the
	 * formatted version of its internal rep. This makes sure
	 * that "expr +000123" yields "83", not "000123". We
	 * implement this by _discarding_ the string rep since we
	 * know it will be regenerated, if needed later, by
	 * formatting the internal rep's value.
	 */

	if (Tcl_IsShared(valuePtr)) {
	    /* If it is shared, just decrease the refCount ... */
	    valuePtr->refCount--;
            if (tPtr == &tclIntType) {
	        SET_TOS(Tcl_NewLongObj(valuePtr->internalRep.longValue));
	    } else if (tPtr == &tclDoubleType) {
	        SET_TOS(Tcl_NewDoubleObj(valuePtr->internalRep.doubleValue));
	    } else {
	        IllegalExprOperandType(interp, pc, valuePtr);
	        result = TCL_ERROR;
		tosPtr--; /* avoid second decrRefCt */
	        goto checkForCatch;
	    }
	} else {
	    if ((tPtr == &tclIntType) || (tPtr == &tclDoubleType)) {
	        Tcl_InvalidateStringRep(valuePtr);
	    } else {
	        IllegalExprOperandType(interp, pc, valuePtr);
	        result = TCL_ERROR;
	        goto checkForCatch;
	    }
	}
	pc++;
	NEXT_INSTR;
    }

    _CASE(INST_UMINUS):  /* tosPtr += 0 */
    _CASE(INST_LNOT): 
    {
	/*
	 * The operand must be numeric or a boolean string as
	 * accepted by Tcl_GetBooleanFromObj(). If the operand 
	 * object is unshared modify it directly, otherwise 
	 * create a copy to modify: this is "copy on write". 
	 * Free any old string representation since it is now 
	 * invalid.
	 */
		
	Tcl_Obj *valuePtr = TOS;
	Tcl_ObjType *tPtr = valuePtr->typePtr;
	union AuxVar X;

	if ((tPtr == &tclBooleanType) && (valuePtr->bytes == NULL)) {
	    valuePtr->typePtr = &tclIntType;
	}
	TRY_CONVERT_TO_NUM(valuePtr,X,tPtr);
		
	if (tPtr == &tclIntType) {
	    USE_OR_MAKE_THEN_SET(
                    ((*pc++ == INST_UMINUS) ? -X.i : !X.i), Long);
	    NEXT_INSTR;
	} else if (tPtr == &tclDoubleType) {
	    if (*pc++ == INST_UMINUS) {
	        USE_OR_MAKE_THEN_SET(-X.d,Double);
	    } else {
		/*
		 * Should be able to use "!d", but apparently
		 * some compilers can't handle it.
		 */
	        USE_OR_MAKE_THEN_SET(((X.d==0.0)? 1 : 0), Long);
	    }
	    NEXT_INSTR;
	} else if (*pc == INST_LNOT) {
	    int boolvar;
	    result = Tcl_GetBooleanFromObj((Tcl_Interp *)NULL,
					   valuePtr, &boolvar);
	    if (result == TCL_OK) {
		pc++;
		X.i = (long) boolvar; /* i is long, not int! */
		USE_OR_MAKE_THEN_SET(!X.i, Long);
		NEXT_INSTR;
	    }
	}
	/* 
	 * Only got here if operation not applicable 
	 */
	IllegalExprOperandType(interp, pc, valuePtr);
	result = TCL_ERROR;
	goto checkForCatch; /* this will decrrefCt valuePtr at TOS */
    }
	    
    _CASE(INST_BITNOT): /* tosPtr += 0 */
    {
	/*
	 * The operand must be an integer. If the operand object is
	 * unshared modify it directly, otherwise modify a copy. 
	 * Free any old string representation since it is now
	 * invalid.
	 */
		
	Tcl_Obj *valuePtr = TOS;
	long i;

	if (valuePtr->typePtr == &tclIntType) {
	    i = valuePtr->internalRep.longValue;
	} else {
	    long ii;
	    result = Tcl_GetLongFromObj((Tcl_Interp *) NULL,
		    valuePtr, &ii);
	    if (result != TCL_OK) {   /* try to convert to double */
		IllegalExprOperandType(interp, pc, valuePtr);
		goto checkForCatch; /* this will decrrefCt valuePtr at TOS */
	    } else {
	      i = ii;
	    }
	}
	USE_OR_MAKE_THEN_SET(~i, Long);		
	pc++;
	NEXT_INSTR;
    }

    _CASE(INST_CALL_BUILTIN_FUNC1): /* tosPtr += 0 */
    {
	/*
	 * Call one of the built-in Tcl math functions.
	 */

	int opnd;
	BuiltinFunc *mathFuncPtr;
	ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

	pc++;
	opnd = TclGetUInt1AtPtr(pc);
	if ((opnd < 0) || (opnd > LAST_BUILTIN_FUNC)) {
	    panic("TclExecuteByteCode: unrecognized builtin function code %d", opnd);
	}
	mathFuncPtr = &(builtinFuncTable[opnd]);
	DECACHE_STACK_INFO();
	tsdPtr->mathInProgress++;
	result = (*mathFuncPtr->proc)(interp, eePtr,
		mathFuncPtr->clientData);
	tsdPtr->mathInProgress--;
	CACHE_STACK_INFO();
	if (result != TCL_OK) {
	    goto checkForCatch;
	}
	pc++; 
	NEXT_INSTR;
    }
    
    _CASE(INST_CALL_FUNC1): /* tosPtr += 0 */
    {
	/*
	 * Call a non-builtin Tcl math function previously
	 * registered by a call to Tcl_CreateMathFunc.
	 */
		
	int objc;          /* Number of arguments. The function name
			    * is the 0-th argument. */
	Tcl_Obj **objv;    /* The array of arguments. The function
			    * name is objv[0]. */
	ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

	pc++;
	objc = TclGetUInt1AtPtr(pc);
	objv = tosPtr - (objc-1); /* "objv[0]" */
	DECACHE_STACK_INFO();
	tsdPtr->mathInProgress++;
	result = ExprCallMathFunc(interp, eePtr, objc, objv);
	tsdPtr->mathInProgress--;
	CACHE_STACK_INFO();
	if (result != TCL_OK) {
	    goto checkForCatch;
	}
	pc++; 
	NEXT_INSTR;
    }

    _CASE(INST_TRY_CVT_TO_NUMERIC):  /* tosPtr += 0 */
    {
	/*
	 * Try to convert the topmost stack object to an int or
	 * double object. This is done in order to support Tcl's
	 * policy of interpreting operands if at all possible as
	 * first integers, else floating-point numbers.
	 */
		
	Tcl_Obj *valuePtr = TOS;
	Tcl_ObjType *tPtr = valuePtr->typePtr;
	union AuxVar X;

	if ((tPtr == &tclBooleanType) && (valuePtr->bytes == NULL)) {
		valuePtr->typePtr = &tclIntType;
	}
	TRY_CONVERT_TO_NUM(valuePtr,X,tPtr);

	/*
	 * Ensure that the topmost stack object, if numeric, has a
	 * string rep the same as the formatted version of its
	 * internal rep. This is used, e.g., to make sure that "expr
	 * {0001}" yields "1", not "0001". We implement this by
	 * _discarding_ the string rep since we know it will be
	 * regenerated, if needed later, by formatting the internal
	 * rep's value. Also check if there has been an IEEE
	 * floating point error.
	 */

	if (tPtr == &tclIntType) {
	    if (Tcl_IsShared(valuePtr)) {
		if (valuePtr->bytes != NULL) {
		    /*
		     * We only need to make a copy of the object
		     * when it already had a string rep
		     */
		    SET_TOS(Tcl_NewLongObj(X.i));
	            /* If it is shared, just decrease the refCount ... */
	            valuePtr->refCount--;
		}
	    } else {
		Tcl_InvalidateStringRep(valuePtr);
	    }
	} else if (tPtr == &tclDoubleType) {
	    if (Tcl_IsShared(valuePtr)) {
		if (valuePtr->bytes != NULL) {
		    /*
		     * We only need to make a copy of the object
		     * when it already had a string rep
		     */
		    SET_TOS(Tcl_NewDoubleObj(X.d));
	            /* If it is shared, just decrease the refCount ... */
	            valuePtr->refCount--;
		}
	    } else {
		Tcl_InvalidateStringRep(valuePtr);
	    }

	    if (IS_NAN(X.d) || IS_INF(X.d)) {
		TclExprFloatError(interp, X.d);
		result = TCL_ERROR;
		goto checkForCatch; /* this will decrRefCt valuePtr at TOS */
	    }
	}
	pc++;
	NEXT_INSTR;
    }

    _CASE(INST_BREAK): /* tosPtr += 0 */
    {
	/*
	 * First reset the interpreter's result. Then find the closest
	 * enclosing loop or catch exception range, if any. If a loop is
	 * found, terminate its execution. If the closest is a catch
	 * exception range, jump to its catchOffset. If no enclosing
	 * range is found, stop execution and return TCL_BREAK.
	 */
        ExceptionRange *rangePtr;
	Tcl_ResetResult(interp);
        rangePtr = GetExceptRangeForPc(pc, /*catchOnly*/ 0, codePtr);
	if (rangePtr == NULL) {
	    result = TCL_BREAK;
	    goto abnormalReturn; /* no catch exists to check */
	}
	switch (rangePtr->type) {
	    case LOOP_EXCEPTION_RANGE:
		result = TCL_OK;
		pc = (codePtr->codeStart + rangePtr->breakOffset);
		NEXT_INSTR;	/* restart outer instruction loop at pc */
	    case CATCH_EXCEPTION_RANGE:
		result = TCL_BREAK;
		pc = (codePtr->codeStart + rangePtr->catchOffset);
		goto processCatch; /* (it will use rangePtr) NOT ANYMORE */
	    default:
		panic("TclExecuteByteCode: unrecognized ExceptionRange type %d\n", rangePtr->type);
	}
    }

    _CASE(INST_CONTINUE): /* tosPtr += 0 */
    {
        /*
	 * Find the closest enclosing loop or catch exception range,
	 * if any. If a loop is found, skip to its next iteration.
	 * If the closest is a catch exception range, jump to its
	 * catchOffset. If no enclosing range is found, stop
	 * execution and return TCL_CONTINUE.
	 */

        ExceptionRange *rangePtr;
	Tcl_ResetResult(interp);
	rangePtr = GetExceptRangeForPc(pc, /*catchOnly*/ 0, codePtr);
	if (rangePtr == NULL) {
	    result = TCL_CONTINUE;
	    goto abnormalReturn;
	}
	switch (rangePtr->type) {
	    case LOOP_EXCEPTION_RANGE:
		if (rangePtr->continueOffset == -1) {
		    goto checkForCatch;
		} else {
		    result = TCL_OK;
		    pc = (codePtr->codeStart + rangePtr->continueOffset);
		    NEXT_INSTR;	/* restart outer instruction loop at pc */
		}
	    case CATCH_EXCEPTION_RANGE:
		result = TCL_CONTINUE;
		pc = (codePtr->codeStart + rangePtr->catchOffset);
		goto processCatch; /* (it will use rangePtr) NOT ANYMORE */
	    default:
		panic("TclExecuteByteCode: unrecognized ExceptionRange type %d\n", rangePtr->type);
	}
    }

    _CASE(INST_FOREACH_START4): /* tosPtr += 0 */
    {
	/*
	 * Initialize the temporary local var that holds the count
	 * of the number of iterations of the loop body to -1.
	 */

        Var *iterVarPtr ;
	Tcl_Obj *oldValuePtr;
	pc++;
	{
	    int opnd = TclGetUInt4AtPtr(pc);
	    ForeachInfo *infoPtr = (ForeachInfo *)
	            codePtr->auxDataArrayPtr[opnd].clientData;
	    int iterTmpIndex = infoPtr->loopCtTemp;
	    Var *compiledLocals = iPtr->varFramePtr->compiledLocals;
	    iterVarPtr = &(compiledLocals[iterTmpIndex]);
	    oldValuePtr = iterVarPtr->value.objPtr;
	}
 	pc += 4; 
 	if (oldValuePtr == NULL) {
	    iterVarPtr->value.objPtr = Tcl_NewLongObj(-1);
	    Tcl_IncrRefCount(iterVarPtr->value.objPtr);
	} else {
	    Tcl_SetLongObj(oldValuePtr, -1);
	}
	TclSetVarScalarDefined(iterVarPtr);
	NEXT_INSTR;
    }
	
    _CASE(INST_FOREACH_STEP4): /* tosPtr += 0 */
    {
	/*
	 * "Step" a foreach loop (i.e., begin its next iteration) by
	 * assigning the next value list element to each loop var.
	 */
	ForeachInfo *infoPtr;
	int numLists;
	Var *compiledLocals = iPtr->varFramePtr->compiledLocals;
	int iterNum;
	pc++;
        {
	    Var *iterVarPtr;
	    Tcl_Obj *valuePtr;
	    int opnd = TclGetUInt4AtPtr(pc);
	    pc += 4; 

	    infoPtr = (ForeachInfo *)
	                 codePtr->auxDataArrayPtr[opnd].clientData;
	    iterVarPtr = &(compiledLocals[infoPtr->loopCtTemp]);
	    numLists = infoPtr->numLists;

	    /*
	     * Increment the temp holding the loop iteration number.
	     */
	    valuePtr = iterVarPtr->value.objPtr;
	    iterNum = (valuePtr->internalRep.longValue + 1);
	    Tcl_SetLongObj(valuePtr, iterNum);
        }
	/*
	 * Check whether all value lists are exhausted and we should
	 * stop the loop.
	 */

	{
	    int listTmpIndex = infoPtr->firstValueTemp;
	    long i;
	    int doneLoop = 1;
	    for (i = 0;  i < numLists;  (listTmpIndex++, i++) ) {
	        Var *listVarPtr = &(compiledLocals[listTmpIndex]); 
	        Tcl_Obj *listPtr = listVarPtr->value.objPtr;
		int minLen = iterNum * ((infoPtr->varLists[i])->numVars);
		int listLen;
		result = Tcl_ListObjLength(interp, listPtr, &listLen);
		if (result != TCL_OK) {
		    pc--; /* to get back within the scope of cmd */
		    goto checkForCatch;
		}
		if (listLen > minLen) {
		  doneLoop = 0;
		}
	    }
	    if (doneLoop) {
	        PUSH_OBJECT(Tcl_NewLongObj(0));
		NEXT_INSTR;
	    }
	}

	/*
	 * If some var in some var list still has a remaining list
	 * element iterate one more time. Assign to var the next
	 * element from its value list. We already checked above
	 * that each list temp holds a valid list object.
	 */
		
	{
	    int listTmpIndex = infoPtr->firstValueTemp;
	    long i;
	    for (i = 0;  i < numLists; (listTmpIndex++, i++) ) {
	        int j;	      
		ForeachVarList *varListPtr = infoPtr->varLists[i];
		int numVars = varListPtr->numVars;
		int valIndex = (iterNum * numVars);
		Var *listVarPtr = &(compiledLocals[listTmpIndex]); 
		Tcl_Obj *listPtr = listVarPtr->value.objPtr;	
		List *listRepPtr = (List *) listPtr->internalRep.otherValuePtr;
		int listLen = listRepPtr->elemCount;			

		for (j = 0;  j < numVars;  (valIndex++, j++)) {
		    Tcl_Obj *valuePtr;
		    int setEmptyStr;

		    if (valIndex >= listLen) {
			setEmptyStr = 1;
			valuePtr = Tcl_NewObj();
		    } else {
		        setEmptyStr = 0;
			valuePtr = listRepPtr->elements[valIndex];
		    }
			    
		    /* varIndex = varListPtr->varIndexes[j]; */
	            { 
			Tcl_Obj *value2Ptr;
			DECACHE_STACK_INFO();
			value2Ptr = TclSetIndexedScalar(interp,
			           varListPtr->varIndexes[j], valuePtr, TCL_LEAVE_ERR_MSG);
			CACHE_STACK_INFO();
			if (value2Ptr == NULL) {
			    if (setEmptyStr) {
				Tcl_DecrRefCount_Q(valuePtr);
			    }
			    result = TCL_ERROR;
			    pc--; /* to get back within the scope of cmd */
			    goto checkForCatch;
			}
		    }
		}
	    }
	}
	
	/*
	 * Push 1 if at least one value list had a remaining element
	 * and the loop should continue. Otherwise push 0.
	 */

	PUSH_OBJECT(Tcl_NewLongObj(1));
	NEXT_INSTR;
    }

    _CASE(INST_BEGIN_CATCH4): /* tosPtr += 0 */
    {
	/*
	 * Record start of the catch command with exception range index
	 * equal to the operand. Push the current stack depth onto the
	 * special catch stack.
	 */
	*catchTopPtr++ = (tosPtr - eePtr->stackPtr);
        pc += 5;
	NEXT_INSTR;
    }

    _CASE(INST_END_CATCH): /* tosPtr += 0 */
    {
	catchTopPtr--;
	result = TCL_OK;
        pc++;
	NEXT_INSTR;
    }

    _CASE(INST_PUSH_RESULT): /* tosPtr += 1 */
    {
	PUSH_OBJECT(Tcl_GetObjResult(interp));
	pc++;
	NEXT_INSTR;
    }

    _CASE(INST_PUSH_RETURN_CODE): /* tosPtr += 1 */
    {
	PUSH_OBJECT(Tcl_NewLongObj(result));
	pc++;
	NEXT_INSTR;
    }

     /* end of switch on opCode */
    _CASE_END /* DO NOT PUT A SEMICOLON HERE, it can be empty ! */


    bad_return_from_invoke_or_eval:
    {
	/*
	 * Process the result of the Tcl_ObjCmdProc call.
	 * Used by INST_INVOKE and INST_EVAL
	 */
        ExceptionRange *rangePtr;

	int newPcOffset = 0; /* New inst offset for break, continue. */
	switch (result) {
	    case TCL_BREAK:
	    case TCL_CONTINUE:
		/*
		 * The invoked command requested a break or continue.
		 * Find the closest enclosing loop or catch exception
		 * range, if any. If a loop is found, terminate its
		 * execution or skip to its next iteration. If the
		 * closest is a catch exception range, jump to its
		 * catchOffset. If no enclosing range is found, stop
		 * execution and return the TCL_BREAK or TCL_CONTINUE.
		 */
		rangePtr = GetExceptRangeForPc(pc, /*catchOnly*/ 0,
			codePtr);
		if (rangePtr == NULL) {
		    goto abnormalReturn; /* no catch exists to check */
		}
		switch (rangePtr->type) {
		case LOOP_EXCEPTION_RANGE:
		    if (result == TCL_BREAK) {
			newPcOffset = rangePtr->breakOffset;
		    } else if (rangePtr->continueOffset == -1) {
		        newPcOffset = 0; /* lint ...*/
			goto checkForCatch;
		    } else {
			newPcOffset = rangePtr->continueOffset;
		    }
		    result = TCL_OK;
		    pc = (codePtr->codeStart + newPcOffset);
		    NEXT_INSTR;	/* restart outer instruction loop at pc */
		case CATCH_EXCEPTION_RANGE:
		    pc = (codePtr->codeStart + rangePtr->catchOffset);
		    goto processCatch; /* (it will use rangePtr) NOT ANYMORE */
		default:
		    panic("TclExecuteByteCode: bad ExceptionRange type\n");
		}		    
	    default:
	        /* handles TCL_ERROR, TCL_RETURN and unknown codes */
		goto checkForCatch;
	}
    }

    /*
     * Division by zero in an expression. Control only reaches this
     * point by "goto divideByZero".
     */
	
    divideByZero:
    Tcl_ResetResult(interp);
    Tcl_AppendToObj(Tcl_GetObjResult(interp), "divide by zero", -1);
    Tcl_SetErrorCode(interp, "ARITH", "DIVZERO", "divide by zero",
		     (char *) NULL);
    result = TCL_ERROR;
	
    /*
     * Execution has generated an "exception" such as TCL_ERROR. If the
     * exception is an error, record information about what was being
     * executed when the error occurred. Find the closest enclosing
     * catch range, if any. If no enclosing catch range is found, stop
     * execution and return the "exception" code.
     */
	
    checkForCatch: 
    {
	int length;
	char *bytes;
        ExceptionRange *rangePtr;

	if ((result == TCL_ERROR) && !(iPtr->flags & ERR_ALREADY_LOGGED)) {
	    bytes = GetSrcInfoForPc(pc, codePtr, &length);
	    if (bytes != NULL) {
		Tcl_LogCommandInfo(interp, codePtr->source, bytes, length);
		iPtr->flags |= ERR_ALREADY_LOGGED;
	    }
        }
	rangePtr = GetExceptRangeForPc(pc, /*catchOnly*/ 1, codePtr);
	if (rangePtr == NULL) {
	    goto abnormalReturn;
	}
	/* this was previously done at processCatch ! */
	pc = (codePtr->codeStart + rangePtr->catchOffset);
    }

    /*
     * A catch exception range (rangePtr) was found to handle an
     * "exception". It was found either by checkForCatch just above or
     * by an instruction during break, continue, or error processing.
     * Jump to its catchOffset after unwinding the operand stack to
     * the depth it had when starting to execute the range's catch
     * command.
     */

    processCatch: 
    {
	Tcl_Obj **catchedTosPtr;
	catchedTosPtr =   eePtr->stackPtr + *(catchTopPtr-1);
	while (tosPtr > catchedTosPtr) {
	    Tcl_Obj *valuePtr = POP_OBJECT();
	    TclDecrRefCount(valuePtr);
	}
	/* This is now set before getting here
	 * pc = (codePtr->codeStart + rangePtr->catchOffset); 
	 */
	NEXT_INSTR_Q; /* empty decrRef stack and restart execution loop at pc */
    }

/* NO MORE INSTRUCTIONS CALLED AFTER HERE */

    /*
     * Abnormal return code. Restore the stack to state it had when starting
     * to execute the ByteCode.
     */

 abnormalReturn: 
    DECR_REF_STACK_empty();
    {
        Tcl_Obj **initTosPtr = eePtr->stackPtr + initTos;
        for ( ; tosPtr > initTosPtr ; tosPtr--) {
	    TclDecrRefCount(TOS);
	}
    }

    /*
     * Free the catch stack array if malloc'ed storage was used.
     */

    done:
    if (catchStackPtr != catchStackStorage) {
	ckfree((char *) catchStackPtr);
    }
    DECACHE_STACK_INFO();
    return result; 
#undef STATIC_CATCH_STACK_SIZE
}

#ifdef TCL_COMPILE_DEBUG
/*
 *----------------------------------------------------------------------
 *
 * PrintByteCodeInfo --
 *
 *	This procedure prints a summary about a bytecode object to stdout.
 *	It is called by TclExecuteByteCode when starting to execute the
 *	bytecode object if tclTraceExec has the value 2 or more.
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
PrintByteCodeInfo(codePtr)
    register ByteCode *codePtr;	/* The bytecode whose summary is printed
				 * to stdout. */
{
    Proc *procPtr = codePtr->procPtr;
    Interp *iPtr = (Interp *) *codePtr->interpHandle;

    fprintf(stdout, "\nExecuting ByteCode 0x%x, refCt %u, epoch %u, interp 0x%x (epoch %u)\n",
	    (unsigned int) codePtr, codePtr->refCount,
	    codePtr->compileEpoch, (unsigned int) iPtr,
	    iPtr->compileEpoch);
    
    fprintf(stdout, "  Source: ");
    TclPrintSource(stdout, codePtr->source, 60);

    fprintf(stdout, "\n  Cmds %d, src %d, inst %u, litObjs %u, aux %d, stkDepth %u, code/src %.2f\n",
            codePtr->numCommands, codePtr->numSrcBytes,
	    codePtr->numCodeBytes, codePtr->numLitObjects,
	    codePtr->numAuxDataItems, codePtr->maxStackDepth,
#ifdef TCL_COMPILE_STATS
	    (codePtr->numSrcBytes?
	            ((float)codePtr->structureSize)/((float)codePtr->numSrcBytes) : 0.0));
#else
	    0.0);
#endif
#ifdef TCL_COMPILE_STATS
    fprintf(stdout, "  Code %d = header %d+inst %d+litObj %d+exc %d+aux %d+cmdMap %d\n",
	    codePtr->structureSize,
	    (sizeof(ByteCode) - (sizeof(size_t) + sizeof(Tcl_Time))),
	    codePtr->numCodeBytes,
	    (codePtr->numLitObjects * sizeof(Tcl_Obj *)),
	    (codePtr->numExceptRanges * sizeof(ExceptionRange)),
	    (codePtr->numAuxDataItems * sizeof(AuxData)),
	    codePtr->numCmdLocBytes);
#endif /* TCL_COMPILE_STATS */
    if (procPtr != NULL) {
	fprintf(stdout,
		"  Proc 0x%x, refCt %d, args %d, compiled locals %d\n",
		(unsigned int) procPtr, procPtr->refCount,
		procPtr->numArgs, procPtr->numCompiledLocals);
    }
}
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * ValidatePcAndStackTop --
 *
 *	This procedure is called by TclExecuteByteCode when debugging to
 *	verify that the program counter and stack top are valid during
 *	execution.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Prints a message to stderr and panics if either the pc or stack
 *	top are invalid.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_COMPILE_DEBUG
static void
ValidatePcAndStackTop(codePtr, pc, stackTop, stackLowerBound,
        stackUpperBound)
    register ByteCode *codePtr; /* The bytecode whose summary is printed
				 * to stdout. */
    unsigned char *pc;		/* Points to first byte of a bytecode
				 * instruction. The program counter. */
    int stackTop;		/* Current stack top. Must be between
				 * stackLowerBound and stackUpperBound
				 * (inclusive). */
    int stackLowerBound;	/* Smallest legal value for stackTop. */
    int stackUpperBound;	/* Greatest legal value for stackTop. */
{
    unsigned int relativePc = (unsigned int) (pc - codePtr->codeStart);
    unsigned int codeStart = (unsigned int) codePtr->codeStart;
    unsigned int codeEnd = (unsigned int)
	    (codePtr->codeStart + codePtr->numCodeBytes);
    unsigned char opCode = *pc;

    if (((unsigned int) pc < codeStart) || ((unsigned int) pc > codeEnd)) {
	fprintf(stderr, "\nBad instruction pc 0x%x in TclExecuteByteCode\n",
		(unsigned int) pc);
	panic("TclExecuteByteCode execution failure: bad pc");
    }
    if ((unsigned int) opCode > LAST_INST_OPCODE) {
	fprintf(stderr, "\nBad opcode %d at pc %u in TclExecuteByteCode\n",
		(unsigned int) opCode, relativePc);
	panic("TclExecuteByteCode execution failure: bad opcode");
    }
    if ((stackTop < stackLowerBound) || (stackTop > stackUpperBound)) {
	int numChars;
	char *cmd = GetSrcInfoForPc(pc, codePtr, &numChars);
	char *ellipsis = "";
	
	fprintf(stderr, "\nBad stack top %d at pc %u in TclExecuteByteCode",
		stackTop, relativePc);
	if (cmd != NULL) {
	    if (numChars > 100) {
		numChars = 100;
		ellipsis = "...";
	    }
	    fprintf(stderr, "\n executing %.*s%s\n", numChars, cmd,
		    ellipsis);
	} else {
	    fprintf(stderr, "\n");
	}
	panic("TclExecuteByteCode execution failure: bad stack top");
    }
}
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * IllegalExprOperandType --
 *
 *	Used by TclExecuteByteCode to add an error message to errorInfo
 *	when an illegal operand type is detected by an expression
 *	instruction. The argument opndPtr holds the operand object in error.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An error message is appended to errorInfo.
 *
 *----------------------------------------------------------------------
 */

static void
IllegalExprOperandType(interp, pc, opndPtr)
    Tcl_Interp *interp;		/* Interpreter to which error information
				 * pertains. */
    unsigned char *pc;		/* Points to the instruction being executed
				 * when the illegal type was found. */
    Tcl_Obj *opndPtr;		/* Points to the operand holding the value
				 * with the illegal type. */
{
    unsigned char opCode = *pc;
    
    Tcl_ResetResult(interp);
    if ((opndPtr->bytes == NULL) || (opndPtr->length == 0)) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"can't use empty string as operand of \"",
		operatorStrings[opCode - INST_LOR], "\"", (char *) NULL);
    } else {
	char *msg = "non-numeric string";
	if (opndPtr->typePtr != &tclDoubleType) {
	    /*
	     * See if the operand can be interpreted as a double in order to
	     * improve the error message.
	     */

	    char *s = Tcl_GetString(opndPtr);
	    double d;

	    if (Tcl_GetDouble((Tcl_Interp *) NULL, s, &d) == TCL_OK) {
		/*
		 * Make sure that what appears to be a double
		 * (ie 08) isn't really a bad octal
		 */
		if (TclCheckBadOctal(NULL, Tcl_GetString(opndPtr))) {
		    msg = "invalid octal number";
		} else {
		    msg = "floating-point value";
		}
	    }
	}
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "can't use ",
		msg, " as operand of \"", operatorStrings[opCode - INST_LOR],
		"\"", (char *) NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CallTraceProcedure --
 *
 *	Invokes a trace procedure registered with an interpreter. These
 *	procedures trace command execution. Currently this trace procedure
 *	is called with the address of the string-based Tcl_CmdProc for the
 *	command, not the Tcl_ObjCmdProc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Those side effects made by the trace procedure.
 *
 *----------------------------------------------------------------------
 */

static void
CallTraceProcedure(interp, tracePtr, cmdPtr, command, numChars, objc, objv)
    Tcl_Interp *interp;		/* The current interpreter. */
    register Trace *tracePtr;	/* Describes the trace procedure to call. */
    Command *cmdPtr;		/* Points to command's Command struct. */
    char *command;		/* Points to the first character of the
				 * command's source before substitutions. */
    int numChars;		/* The number of characters in the
				 * command's source. */
    register int objc;		/* Number of arguments for the command. */
    Tcl_Obj *objv[];		/* Pointers to Tcl_Obj of each argument. */
{
    Interp *iPtr = (Interp *) interp;
    register char **argv;
    register int i;
    int length;
    char *p;

    /*
     * Get the string rep from the objv argument objects and place their
     * pointers in argv. First make sure argv is large enough to hold the
     * objc args plus 1 extra word for the zero end-of-argv word.
     */
    
    argv = (char **) ckalloc((unsigned)(objc + 1) * sizeof(char *));
    for (i = 0;  i < objc;  i++) {
	argv[i] = Tcl_GetStringFromObj(objv[i], &length);
    }
    argv[objc] = 0;

    /*
     * Copy the command characters into a new string.
     */

    p = (char *) ckalloc((unsigned) (numChars + 1));
    memcpy((VOID *) p, (VOID *) command, (size_t) numChars);
    p[numChars] = '\0';
    
    /*
     * Call the trace procedure then free allocated storage.
     */
    
    (*tracePtr->proc)(tracePtr->clientData, interp, iPtr->numLevels,
                      p, cmdPtr->proc, cmdPtr->clientData, objc, argv);

    ckfree((char *) argv);
    ckfree((char *) p);
}

/*
 *----------------------------------------------------------------------
 *
 * GetSrcInfoForPc --
 *
 *	Given a program counter value, finds the closest command in the
 *	bytecode code unit's CmdLocation array and returns information about
 *	that command's source: a pointer to its first byte and the number of
 *	characters.
 *
 * Results:
 *	If a command is found that encloses the program counter value, a
 *	pointer to the command's source is returned and the length of the
 *	source is stored at *lengthPtr. If multiple commands resulted in
 *	code at pc, information about the closest enclosing command is
 *	returned. If no matching command is found, NULL is returned and
 *	*lengthPtr is unchanged.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
GetSrcInfoForPc(pc, codePtr, lengthPtr)
    unsigned char *pc;		/* The program counter value for which to
				 * return the closest command's source info.
				 * This points to a bytecode instruction
				 * in codePtr's code. */
    ByteCode *codePtr;		/* The bytecode sequence in which to look
				 * up the command source for the pc. */
    int *lengthPtr;		/* If non-NULL, the location where the
				 * length of the command's source should be
				 * stored. If NULL, no length is stored. */
{
    register int pcOffset = (pc - codePtr->codeStart);
    int numCmds = codePtr->numCommands;
    unsigned char *codeDeltaNext, *codeLengthNext;
    unsigned char *srcDeltaNext, *srcLengthNext;
    int codeOffset, codeLen, codeEnd, srcOffset, srcLen, delta, i;
    int bestDist = INT_MAX;	/* Distance of pc to best cmd's start pc. */
    int bestSrcOffset = -1;	/* Initialized to avoid compiler warning. */
    int bestSrcLength = -1;	/* Initialized to avoid compiler warning. */

    if ((pcOffset < 0) || (pcOffset >= codePtr->numCodeBytes)) {
	return NULL;
    }

    /*
     * Decode the code and source offset and length for each command. The
     * closest enclosing command is the last one whose code started before
     * pcOffset.
     */

    codeDeltaNext = codePtr->codeDeltaStart;
    codeLengthNext = codePtr->codeLengthStart;
    srcDeltaNext  = codePtr->srcDeltaStart;
    srcLengthNext = codePtr->srcLengthStart;
    codeOffset = srcOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	if ((unsigned int) (*codeDeltaNext) == (unsigned int) 0xFF) {
	    codeDeltaNext++;
	    delta = TclGetInt4AtPtr(codeDeltaNext);
	    codeDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(codeDeltaNext);
	    codeDeltaNext++;
	}
	codeOffset += delta;

	if ((unsigned int) (*codeLengthNext) == (unsigned int) 0xFF) {
	    codeLengthNext++;
	    codeLen = TclGetInt4AtPtr(codeLengthNext);
	    codeLengthNext += 4;
	} else {
	    codeLen = TclGetInt1AtPtr(codeLengthNext);
	    codeLengthNext++;
	}
	codeEnd = (codeOffset + codeLen - 1);

	if ((unsigned int) (*srcDeltaNext) == (unsigned int) 0xFF) {
	    srcDeltaNext++;
	    delta = TclGetInt4AtPtr(srcDeltaNext);
	    srcDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(srcDeltaNext);
	    srcDeltaNext++;
	}
	srcOffset += delta;

	if ((unsigned int) (*srcLengthNext) == (unsigned int) 0xFF) {
	    srcLengthNext++;
	    srcLen = TclGetInt4AtPtr(srcLengthNext);
	    srcLengthNext += 4;
	} else {
	    srcLen = TclGetInt1AtPtr(srcLengthNext);
	    srcLengthNext++;
	}
	
	if (codeOffset > pcOffset) {      /* best cmd already found */
	    break;
	} else if (pcOffset <= codeEnd) { /* this cmd's code encloses pc */
	    int dist = (pcOffset - codeOffset);
	    if (dist <= bestDist) {
		bestDist = dist;
		bestSrcOffset = srcOffset;
		bestSrcLength = srcLen;
	    }
	}
    }

    if (bestDist == INT_MAX) {
	return NULL;
    }
    
    if (lengthPtr != NULL) {
	*lengthPtr = bestSrcLength;
    }
    return (codePtr->source + bestSrcOffset);
}

/*
 *----------------------------------------------------------------------
 *
 * GetExceptRangeForPc --
 *
 *	Given a program counter value, return the closest enclosing
 *	ExceptionRange.
 *
 * Results:
 *	In the normal case, catchOnly is 0 (false) and this procedure
 *	returns a pointer to the most closely enclosing ExceptionRange
 *	structure regardless of whether it is a loop or catch exception
 *	range. This is appropriate when processing a TCL_BREAK or
 *	TCL_CONTINUE, which will be "handled" either by a loop exception
 *	range or a closer catch range. If catchOnly is nonzero, this
 *	procedure ignores loop exception ranges and returns a pointer to the
 *	closest catch range. If no matching ExceptionRange is found that
 *	encloses pc, a NULL is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static ExceptionRange *
GetExceptRangeForPc(pc, catchOnly, codePtr)
    unsigned char *pc;		/* The program counter value for which to
				 * search for a closest enclosing exception
				 * range. This points to a bytecode
				 * instruction in codePtr's code. */
    int catchOnly;		/* If 0, consider either loop or catch
				 * ExceptionRanges in search. If nonzero
				 * consider only catch ranges (and ignore
				 * any closer loop ranges). */
    ByteCode* codePtr;		/* Points to the ByteCode in which to search
				 * for the enclosing ExceptionRange. */
{
    ExceptionRange *rangeArrayPtr;
    int numRanges = codePtr->numExceptRanges;
    register ExceptionRange *rangePtr;
    int pcOffset = (pc - codePtr->codeStart);
    register int i, level;

    if (numRanges == 0) {
	return NULL;
    }
    rangeArrayPtr = codePtr->exceptArrayPtr;

    for (level = codePtr->maxExceptDepth;  level >= 0;  level--) {
	for (i = 0;  i < numRanges;  i++) {
	    rangePtr = &(rangeArrayPtr[i]);
	    if (rangePtr->nestingLevel == level) {
		int start = rangePtr->codeOffset;
		int end   = (start + rangePtr->numCodeBytes);
		if ((start <= pcOffset) && (pcOffset < end)) {
		    if ((!catchOnly)
			    || (rangePtr->type == CATCH_EXCEPTION_RANGE)) {
			return rangePtr;
		    }
		}
	    }
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOpcodeName --
 *
 *	This procedure is called by the TRACE and TRACE_WITH_OBJ macros
 *	used in TclExecuteByteCode when debugging. It returns the name of
 *	the bytecode instruction at a specified instruction pc.
 *
 * Results:
 *	A character string for the instruction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_COMPILE_DEBUG
static char *
GetOpcodeName(pc)
    unsigned char *pc;		/* Points to the instruction whose name
				 * should be returned. */
{
    unsigned char opCode = *pc;
    
    return instructionTable[opCode].name;
}
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * VerifyExprObjType --
 *
 *	This procedure is called by the math functions to verify that
 *	the object is either an int or double, coercing it if necessary.
 *	If an error occurs during conversion, an error message is left
 *	in the interpreter's result unless "interp" is NULL.
 *
 * Results:
 *	TCL_OK if it was int or double, TCL_ERROR otherwise
 *
 * Side effects:
 *	objPtr is ensured to be either tclIntType of tclDoubleType.
 *
 *----------------------------------------------------------------------
 */

static int
VerifyExprObjType(interp, objPtr)
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * function. */
    Tcl_Obj *objPtr;		/* Points to the object to type check. */
{
    if ((objPtr->typePtr == &tclIntType) ||
	    (objPtr->typePtr == &tclDoubleType)) {
	return TCL_OK;
    } else {
	int length, result = TCL_OK;
	char *s = Tcl_GetStringFromObj(objPtr, &length);
	
	if (TclLooksLikeInt(s, length)) {
	    long i;
	    result = Tcl_GetLongFromObj((Tcl_Interp *) NULL, objPtr, &i);
	} else {
	    double d;
	    result = Tcl_GetDoubleFromObj((Tcl_Interp *) NULL, objPtr, &d);
	}
	if ((result != TCL_OK) && (interp != NULL)) {
	    Tcl_ResetResult(interp);
	    if (TclCheckBadOctal((Tcl_Interp *) NULL, s)) {
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
			"argument to math function was an invalid octal number",
			-1);
	    } else {
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
			"argument to math function didn't have numeric value",
			-1);
	    }
	}
	return result;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Math Functions --
 *
 *	This page contains the procedures that implement all of the
 *	built-in math functions for expressions.
 *
 * Results:
 *	Each procedure returns TCL_OK if it succeeds and pushes an
 *	Tcl object holding the result. If it fails it returns TCL_ERROR
 *	and leaves an error message in the interpreter's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ExprUnaryFunc(interp, eePtr, clientData)
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * function. */
    ExecEnv *eePtr;		/* Points to the environment for executing
				 * the function. */
    ClientData clientData;	/* Contains the address of a procedure that
				 * takes one double argument and returns a
				 * double result. */
{
    register Tcl_Obj **tosPtr;	/* Cached top index of evaluation stack. */
    register Tcl_Obj *valuePtr;
    double d, dResult;
    int result;
    
    double (*func) _ANSI_ARGS_((double)) =
	(double (*)_ANSI_ARGS_((double))) clientData;

    /*
     * tosPtr from eePtr.
     */

    result = TCL_OK;
    CACHE_STACK_INFO();

    /*
     * Pop the function's argument from the evaluation stack. Convert it
     * to a double if necessary.
     */

    valuePtr = POP_OBJECT();

    if (VerifyExprObjType(interp, valuePtr) != TCL_OK) {
	result = TCL_ERROR;
	goto done;
    }
    
    if (valuePtr->typePtr == &tclIntType) {
	d = (double) valuePtr->internalRep.longValue;
    } else {
	d = valuePtr->internalRep.doubleValue;
    }

    errno = 0;
    dResult = (*func)(d);
    if ((errno != 0) || IS_NAN(dResult) || IS_INF(dResult)) {
	TclExprFloatError(interp, dResult);
	result = TCL_ERROR;
	goto done;
    }
    
    /*
     * Push a Tcl object holding the result.
     */

    PUSH_OBJECT(Tcl_NewDoubleObj(dResult));
    
    /*
     * Reflect the change to tosPtr back in eePtr.
     */

    done:
    TclDecrRefCount(valuePtr);
    DECACHE_STACK_INFO();
    return result;
}

static int
ExprBinaryFunc(interp, eePtr, clientData)
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * function. */
    ExecEnv *eePtr;		/* Points to the environment for executing
				 * the function. */
    ClientData clientData;	/* Contains the address of a procedure that
				 * takes two double arguments and
				 * returns a double result. */
{
    register Tcl_Obj **tosPtr;	/* Cached top index of evaluation stack. */
    register Tcl_Obj *valuePtr, *value2Ptr;
    double d1, d2, dResult;
    int result;
    
    double (*func) _ANSI_ARGS_((double, double))
	= (double (*)_ANSI_ARGS_((double, double))) clientData;

    /*
     * Set tosPtr from eePtr.
     */

    result = TCL_OK;
    CACHE_STACK_INFO();

    /*
     * Pop the function's two arguments from the evaluation stack. Convert
     * them to doubles if necessary.
     */

    value2Ptr = POP_OBJECT();
    valuePtr  = POP_OBJECT();

    if ((VerifyExprObjType(interp, valuePtr) != TCL_OK) ||
	    (VerifyExprObjType(interp, value2Ptr) != TCL_OK)) {
	result = TCL_ERROR;
	goto done;
    }

    if (valuePtr->typePtr == &tclIntType) {
	d1 = (double) valuePtr->internalRep.longValue;
    } else {
	d1 = valuePtr->internalRep.doubleValue;
    }

    if (value2Ptr->typePtr == &tclIntType) {
	d2 = (double) value2Ptr->internalRep.longValue;
    } else {
	d2 = value2Ptr->internalRep.doubleValue;
    }

    errno = 0;
    dResult = (*func)(d1, d2);
    if ((errno != 0) || IS_NAN(dResult) || IS_INF(dResult)) {
	TclExprFloatError(interp, dResult);
	result = TCL_ERROR;
	goto done;
    }

    /*
     * Push a Tcl object holding the result.
     */

    PUSH_OBJECT(Tcl_NewDoubleObj(dResult));
    
    /*
     * Reflect the change to tosPtr back in eePtr.
     */

    done:
    Tcl_DecrRefCount(valuePtr);
    Tcl_DecrRefCount(value2Ptr);
    DECACHE_STACK_INFO();
    return result;
}

static int
ExprAbsFunc(interp, eePtr, clientData)
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * function. */
    ExecEnv *eePtr;		/* Points to the environment for executing
				 * the function. */
    ClientData clientData;	/* Ignored. */
{
    register Tcl_Obj **tosPtr;	/* Cached top index of evaluation stack. */
    register Tcl_Obj *valuePtr;
    long i, iResult;
    double d, dResult;
    int result;

    /*
     * Set tosPtr from eePtr.
     */

    result = TCL_OK;
    CACHE_STACK_INFO();

    /*
     * Pop the argument from the evaluation stack.
     */

    valuePtr = POP_OBJECT();

    if (VerifyExprObjType(interp, valuePtr) != TCL_OK) {
	result = TCL_ERROR;
	goto done;
    }

    /*
     * Push a Tcl object with the result.
     */
    if (valuePtr->typePtr == &tclIntType) {
	i = valuePtr->internalRep.longValue;
	if (i < 0) {
	    iResult = -i;
	    if (iResult < 0) {
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "integer value too large to represent", -1);
		Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW",
			"integer value too large to represent", (char *) NULL);
		result = TCL_ERROR;
		goto done;
	    }
	} else {
	    iResult = i;
	}	    
	PUSH_OBJECT(Tcl_NewLongObj(iResult));
    } else {
	d = valuePtr->internalRep.doubleValue;
	if (d < 0.0) {
	    dResult = -d;
	} else {
	    dResult = d;
	}
	if (IS_NAN(dResult) || IS_INF(dResult)) {
	    TclExprFloatError(interp, dResult);
	    result = TCL_ERROR;
	    goto done;
	}
	PUSH_OBJECT(Tcl_NewDoubleObj(dResult));
    }

    /*
     * Reflect the change to tosPtr back in eePtr.
     */

    done:
    Tcl_DecrRefCount(valuePtr);
    DECACHE_STACK_INFO();
    return result;
}

static int
ExprDoubleFunc(interp, eePtr, clientData)
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * function. */
    ExecEnv *eePtr;		/* Points to the environment for executing
				 * the function. */
    ClientData clientData;	/* Ignored. */
{
    register Tcl_Obj **tosPtr;  /* Cached top index of evaluation stack. */
    register Tcl_Obj *valuePtr;
    double dResult;
    int result;

    /*
     * Set tosPtr from eePtr.
     */

    result = TCL_OK;
    CACHE_STACK_INFO();

    /*
     * Pop the argument from the evaluation stack.
     */

    valuePtr = POP_OBJECT();

    if (VerifyExprObjType(interp, valuePtr) != TCL_OK) {
	result = TCL_ERROR;
	goto done;
    }

    if (valuePtr->typePtr == &tclIntType) {
	dResult = (double) valuePtr->internalRep.longValue;
    } else {
	dResult = valuePtr->internalRep.doubleValue;
    }

    /*
     * Push a Tcl object with the result.
     */

    PUSH_OBJECT(Tcl_NewDoubleObj(dResult));

    /*
     * Reflect the change to tosPtr back in eePtr.
     */

    done:
    Tcl_DecrRefCount(valuePtr);
    DECACHE_STACK_INFO();
    return result;
}

static int
ExprIntFunc(interp, eePtr, clientData)
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * function. */
    ExecEnv *eePtr;		/* Points to the environment for executing
				 * the function. */
    ClientData clientData;	/* Ignored. */
{
    register Tcl_Obj **tosPtr;  /* Cached top index of evaluation stack. */
    register Tcl_Obj *valuePtr;
    long iResult;
    double d;
    int result;

    /*
     * Set tosPtr from eePtr.
     */

    result = TCL_OK;
    CACHE_STACK_INFO();

    /*
     * Pop the argument from the evaluation stack.
     */

    valuePtr = POP_OBJECT();
    
    if (VerifyExprObjType(interp, valuePtr) != TCL_OK) {
	result = TCL_ERROR;
	goto done;
    }
    
    if (valuePtr->typePtr == &tclIntType) {
	iResult = valuePtr->internalRep.longValue;
    } else {
	d = valuePtr->internalRep.doubleValue;
	if (d < 0.0) {
	    if (d < (double) (long) LONG_MIN) {
		tooLarge:
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "integer value too large to represent", -1);
		Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW",
			"integer value too large to represent", (char *) NULL);
		result = TCL_ERROR;
		goto done;
	    }
	} else {
	    if (d > (double) LONG_MAX) {
		goto tooLarge;
	    }
	}
	if (IS_NAN(d) || IS_INF(d)) {
	    TclExprFloatError(interp, d);
	    result = TCL_ERROR;
	    goto done;
	}
	iResult = (long) d;
    }

    /*
     * Push a Tcl object with the result.
     */
    
    PUSH_OBJECT(Tcl_NewLongObj(iResult));

    /*
     * Reflect the change to tosPtr back in eePtr.
     */

    done:
    Tcl_DecrRefCount(valuePtr);
    DECACHE_STACK_INFO();
    return result;
}

static int
ExprRandFunc(interp, eePtr, clientData)
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * function. */
    ExecEnv *eePtr;		/* Points to the environment for executing
				 * the function. */
    ClientData clientData;	/* Ignored. */
{
    register Tcl_Obj **tosPtr;           /* Cached evaluation stack top pointer. */
    Interp *iPtr = (Interp *) interp;
    double dResult;
    long tmp;			/* Algorithm assumes at least 32 bits.
				 * Only long guarantees that.  See below. */

    if (!(iPtr->flags & RAND_SEED_INITIALIZED)) {
	iPtr->flags |= RAND_SEED_INITIALIZED;

	/* 
 	 * Take into consideration the thread this interp is running in order
 	 * to insure different seeds in different threads (bug #416643)
 	 */
 
 	iPtr->randSeed = TclpGetClicks() + ((long) Tcl_GetCurrentThread() << 12);

	/*
	 * Make sure 1 <= randSeed <= (2^31) - 2.  See below.
	 */

        iPtr->randSeed &= (unsigned long) 0x7fffffff;
	if ((iPtr->randSeed == 0) || (iPtr->randSeed == 0x7fffffff)) {
	    iPtr->randSeed ^= 123459876;
	}
    }
    
    /*
     * Set tosPtr from eePtr.
     */
    
    CACHE_STACK_INFO();

    /*
     * Generate the random number using the linear congruential
     * generator defined by the following recurrence:
     *		seed = ( IA * seed ) mod IM
     * where IA is 16807 and IM is (2^31) - 1.  The recurrence maps
     * a seed in the range [1, IM - 1] to a new seed in that same range.
     * The recurrence maps IM to 0, and maps 0 back to 0, so those two
     * values must not be allowed as initial values of seed.
     *
     * In order to avoid potential problems with integer overflow, the
     * recurrence is implemented in terms of additional constants
     * IQ and IR such that
     *		IM = IA*IQ + IR
     * None of the operations in the implementation overflows a 32-bit
     * signed integer, and the C type long is guaranteed to be at least
     * 32 bits wide.
     *
     * For more details on how this algorithm works, refer to the following
     * papers: 
     *
     *	S.K. Park & K.W. Miller, "Random number generators: good ones
     *	are hard to find," Comm ACM 31(10):1192-1201, Oct 1988
     *
     *	W.H. Press & S.A. Teukolsky, "Portable random number
     *	generators," Computers in Physics 6(5):522-524, Sep/Oct 1992.
     */

#define RAND_IA		16807
#define RAND_IM		2147483647
#define RAND_IQ		127773
#define RAND_IR		2836
#define RAND_MASK	123459876

    tmp = iPtr->randSeed/RAND_IQ;
    iPtr->randSeed = RAND_IA*(iPtr->randSeed - tmp*RAND_IQ) - RAND_IR*tmp;
    if (iPtr->randSeed < 0) {
	iPtr->randSeed += RAND_IM;
    }

    /*
     * Since the recurrence keeps seed values in the range [1, RAND_IM - 1],
     * dividing by RAND_IM yields a double in the range (0, 1).
     */

    dResult = iPtr->randSeed * (1.0/RAND_IM);

    /*
     * Push a Tcl object with the result.
     */

    PUSH_OBJECT(Tcl_NewDoubleObj(dResult));
    
    /*
     * Reflect the change to stackTop back in eePtr.
     */

    DECACHE_STACK_INFO();
    return TCL_OK;
}

static int
ExprRoundFunc(interp, eePtr, clientData)
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * function. */
    ExecEnv *eePtr;		/* Points to the environment for executing
				 * the function. */
    ClientData clientData;	/* Ignored. */
{
    register Tcl_Obj **tosPtr;  /* Cached top index of evaluation stack. */
    Tcl_Obj *valuePtr;
    long iResult;
    double d, temp;
    int result;

    /*
     * Set stackPtr and tosPtr from eePtr.
     */

    result = TCL_OK;
    CACHE_STACK_INFO();

    /*
     * Pop the argument from the evaluation stack.
     */

    valuePtr = POP_OBJECT();

    if (VerifyExprObjType(interp, valuePtr) != TCL_OK) {
	result = TCL_ERROR;
	goto done;
    }
    
    if (valuePtr->typePtr == &tclIntType) {
	iResult = valuePtr->internalRep.longValue;
    } else {
	d = valuePtr->internalRep.doubleValue;
	if (d < 0.0) {
	    if (d <= (((double) (long) LONG_MIN) - 0.5)) {
		tooLarge:
		Tcl_ResetResult(interp);
		Tcl_AppendToObj(Tcl_GetObjResult(interp),
		        "integer value too large to represent", -1);
		Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW",
			"integer value too large to represent",
			(char *) NULL);
		result = TCL_ERROR;
		goto done;
	    }
	    temp = (long) (d - 0.5);
	} else {
	    if (d >= (((double) LONG_MAX + 0.5))) {
		goto tooLarge;
	    }
	    temp = (long) (d + 0.5);
	}
	if (IS_NAN(temp) || IS_INF(temp)) {
	    TclExprFloatError(interp, temp);
	    result = TCL_ERROR;
	    goto done;
	}
	iResult = (long) temp;
    }

    /*
     * Push a Tcl object with the result.
     */
    
    PUSH_OBJECT(Tcl_NewLongObj(iResult));

    /*
     * Reflect the change to tosPtr back in eePtr.
     */

    done:
    Tcl_DecrRefCount(valuePtr);
    DECACHE_STACK_INFO();
    return result;
}

static int
ExprSrandFunc(interp, eePtr, clientData)
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * function. */
    ExecEnv *eePtr;		/* Points to the environment for executing
				 * the function. */
    ClientData clientData;	/* Ignored. */
{
    Tcl_Obj **tosPtr;        /* Cached evaluation stack top pointer. */
    Interp *iPtr = (Interp *) interp;
    Tcl_Obj *valuePtr;
    long i = 0;			/* Initialized to avoid compiler warning. */
    int result;

    /*
     * Set tosPtr from eePtr.
     */
    
    CACHE_STACK_INFO();

    /*
     * Pop the argument from the evaluation stack.  Use the value
     * to reset the random number seed.
     */

    valuePtr = POP_OBJECT();

    if (VerifyExprObjType(interp, valuePtr) != TCL_OK) {
	result = TCL_ERROR;
	goto badValue;
    }

    if (valuePtr->typePtr == &tclIntType) {
	i = valuePtr->internalRep.longValue;
    } else {
	/*
	 * At this point, the only other possible type is double
	 */
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"can't use floating-point value as argument to srand",
		(char *) NULL);
	badValue:
	Tcl_DecrRefCount(valuePtr);
	DECACHE_STACK_INFO();
	return TCL_ERROR;
    }
    
    /*
     * Reset the seed.  Make sure 1 <= randSeed <= 2^31 - 2.
     * See comments in ExprRandFunc() for more details.
     */

    iPtr->flags |= RAND_SEED_INITIALIZED;
    iPtr->randSeed = i;
    iPtr->randSeed &= (unsigned long) 0x7fffffff;
    if ((iPtr->randSeed == 0) || (iPtr->randSeed == 0x7fffffff)) {
	iPtr->randSeed ^= 123459876;
    }

    /*
     * To avoid duplicating the random number generation code we simply
     * clean up our state and call the real random number function. That
     * function will always succeed.
     */
    
    Tcl_DecrRefCount(valuePtr);
    DECACHE_STACK_INFO();

    ExprRandFunc(interp, eePtr, clientData);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExprCallMathFunc --
 *
 *	This procedure is invoked to call a non-builtin math function
 *	during the execution of an expression. 
 *
 * Results:
 *	TCL_OK is returned if all went well and the function's value
 *	was computed successfully. If an error occurred, TCL_ERROR
 *	is returned and an error message is left in the interpreter's
 *	result.	After a successful return this procedure pushes a Tcl object
 *	holding the result. 
 *
 * Side effects:
 *	None, unless the called math function has side effects.
 *
 *----------------------------------------------------------------------
 */

static int
ExprCallMathFunc(interp, eePtr, objc, objv)
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * function. */
    ExecEnv *eePtr;		/* Points to the environment for executing
				 * the function. */
    int objc;			/* Number of arguments. The function name is
				 * the 0-th argument. */
    Tcl_Obj **objv;		/* The array of arguments. The function name
				 * is objv[0]. */
{
    Interp *iPtr = (Interp *) interp;
    register Tcl_Obj **tosPtr;  /* Cached top index of evaluation stack. */
    char *funcName;
    Tcl_HashEntry *hPtr;
    MathFunc *mathFuncPtr;	/* Information about math function. */
    Tcl_Value args[MAX_MATH_ARGS]; /* Arguments for function call. */
    Tcl_Value funcResult;	/* Result of function call as Tcl_Value. */
    register Tcl_Obj *valuePtr;
    long i;
    double d;
    int j, k, result;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    Tcl_ResetResult(interp);

    /*
     * Set stackPtr and tosPtr from eePtr.
     */
    
    CACHE_STACK_INFO();

    /*
     * Look up the MathFunc record for the function.
     */

    funcName = Tcl_GetString(objv[0]);
    hPtr = Tcl_FindHashEntry(&iPtr->mathFuncTable, funcName);
    if (hPtr == NULL) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"unknown math function \"", funcName, "\"", (char *) NULL);
	result = TCL_ERROR;
	goto done;
    }
    mathFuncPtr = (MathFunc *) Tcl_GetHashValue(hPtr);
    if (mathFuncPtr->numArgs != (objc-1)) {
	panic("ExprCallMathFunc: expected number of args %d != actual number %d",
	        mathFuncPtr->numArgs, objc);
	result = TCL_ERROR;
	goto done;
    }

    /*
     * Collect the arguments for the function, if there are any, into the
     * array "args". Note that args[0] will have the Tcl_Value that
     * corresponds to objv[1].
     */

    for (j = 1, k = 0;  j < objc;  j++, k++) {
	valuePtr = objv[j];

	if (VerifyExprObjType(interp, valuePtr) != TCL_OK) {
	    result = TCL_ERROR;
	    goto done;
	}

	/*
	 * Copy the object's numeric value to the argument record,
	 * converting it if necessary. 
	 */

	if (valuePtr->typePtr == &tclIntType) {
	    i = valuePtr->internalRep.longValue;
	    if (mathFuncPtr->argTypes[k] == TCL_DOUBLE) {
		args[k].type = TCL_DOUBLE;
		args[k].doubleValue = i;
	    } else {
		args[k].type = TCL_INT;
		args[k].intValue = i;
	    }
	} else {
	    d = valuePtr->internalRep.doubleValue;
	    if (mathFuncPtr->argTypes[k] == TCL_INT) {
		args[k].type = TCL_INT;
		args[k].intValue = (long) d;
	    } else {
		args[k].type = TCL_DOUBLE;
		args[k].doubleValue = d;
	    }
	}
    }

    /*
     * Invoke the function and copy its result back into valuePtr.
     */

    tsdPtr->mathInProgress++;
    result = (*mathFuncPtr->proc)(mathFuncPtr->clientData, interp, args,
	    &funcResult);
    tsdPtr->mathInProgress--;
    if (result != TCL_OK) {
	goto done;
    }

    /*
     * Pop the objc top stack elements and decrement their ref counts.
     */

    {		
    Tcl_Obj **i = (tosPtr - (objc-1));
        while (i <= tosPtr) {
	    valuePtr = *i;
	    Tcl_DecrRefCount(valuePtr);
	    i++;
	}
    }
    tosPtr -= objc;
    
    /*
     * Push the call's object result.
     */
    
    if (funcResult.type == TCL_INT) {
	PUSH_OBJECT(Tcl_NewLongObj(funcResult.intValue));
    } else {
	d = funcResult.doubleValue;
	if (IS_NAN(d) || IS_INF(d)) {
	    TclExprFloatError(interp, d);
	    result = TCL_ERROR;
	    goto done;
	}
	PUSH_OBJECT(Tcl_NewDoubleObj(d));
    }

    /*
     * Reflect the change to tosPtr back in eePtr.
     */

    done:
    DECACHE_STACK_INFO();
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclExprFloatError --
 *
 *	This procedure is called when an error occurs during a
 *	floating-point operation. It reads errno and sets
 *	interp->objResultPtr accordingly.
 *
 * Results:
 *	interp->objResultPtr is set to hold an error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclExprFloatError(interp, value)
    Tcl_Interp *interp;		/* Where to store error message. */
    double value;		/* Value returned after error;  used to
				 * distinguish underflows from overflows. */
{
    char *s;

    Tcl_ResetResult(interp);
    if ((errno == EDOM) || (value != value)) {
	s = "domain error: argument not in valid range";
	Tcl_AppendToObj(Tcl_GetObjResult(interp), s, -1);
	Tcl_SetErrorCode(interp, "ARITH", "DOMAIN", s, (char *) NULL);
    } else if ((errno == ERANGE) || IS_INF(value)) {
	if (value == 0.0) {
	    s = "floating-point value too small to represent";
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), s, -1);
	    Tcl_SetErrorCode(interp, "ARITH", "UNDERFLOW", s, (char *) NULL);
	} else {
	    s = "floating-point value too large to represent";
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), s, -1);
	    Tcl_SetErrorCode(interp, "ARITH", "OVERFLOW", s, (char *) NULL);
	}
    } else {
	char msg[64 + TCL_INTEGER_SPACE];
	
	sprintf(msg, "unknown floating-point error, errno = %d", errno);
	Tcl_AppendToObj(Tcl_GetObjResult(interp), msg, -1);
	Tcl_SetErrorCode(interp, "ARITH", "UNKNOWN", msg, (char *) NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclMathInProgress --
 *
 *	This procedure is called to find out if Tcl is doing math
 *	in this thread.
 *
 * Results:
 *	0 or 1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclMathInProgress()
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    return tsdPtr->mathInProgress;
}

#ifdef TCL_COMPILE_STATS
/*
 *----------------------------------------------------------------------
 *
 * TclLog2 --
 *
 *	Procedure used while collecting compilation statistics to determine
 *	the log base 2 of an integer.
 *
 * Results:
 *	Returns the log base 2 of the operand. If the argument is less
 *	than or equal to zero, a zero is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclLog2(value)
    register int value;		/* The integer for which to compute the
				 * log base 2. */
{
    register int n = value;
    register int result = 0;

    while (n > 1) {
	n = n >> 1;
	result++;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * EvalStatsCmd --
 *
 *	Implements the "evalstats" command that prints instruction execution
 *	counts to stdout.
 *
 * Results:
 *	Standard Tcl results.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
EvalStatsCmd(unused, interp, argc, argv)
    ClientData unused;		/* Unused. */
    Tcl_Interp *interp;		/* The current interpreter. */
    int argc;			/* The number of arguments. */
    char **argv;		/* The argument strings. */
{
    Interp *iPtr = (Interp *) interp;
    LiteralTable *globalTablePtr = &(iPtr->literalTable);
    ByteCodeStats *statsPtr = &(iPtr->stats);
    double totalCodeBytes, currentCodeBytes;
    double totalLiteralBytes, currentLiteralBytes;
    double objBytesIfUnshared, strBytesIfUnshared, sharingBytesSaved;
    double strBytesSharedMultX, strBytesSharedOnce;
    double numInstructions, currentHeaderBytes;
    long numCurrentByteCodes, numByteCodeLits;
    long refCountSum, literalMgmtBytes, sum;
    int numSharedMultX, numSharedOnce;
    int decadeHigh, minSizeDecade, maxSizeDecade, length, i;
    char *litTableStats;
    LiteralEntry *entryPtr;

    numInstructions = 0.0;
    for (i = 0;  i < 256;  i++) {
        if (statsPtr->instructionCount[i] != 0) {
            numInstructions += statsPtr->instructionCount[i];
        }
    }

    totalLiteralBytes = sizeof(LiteralTable)
	    + iPtr->literalTable.numBuckets * sizeof(LiteralEntry *)
	    + (statsPtr->numLiteralsCreated * sizeof(LiteralEntry))
	    + (statsPtr->numLiteralsCreated * sizeof(Tcl_Obj))
	    + statsPtr->totalLitStringBytes;
    totalCodeBytes = statsPtr->totalByteCodeBytes + totalLiteralBytes;

    numCurrentByteCodes =
	    statsPtr->numCompilations - statsPtr->numByteCodesFreed;
    currentHeaderBytes = numCurrentByteCodes
	    * (sizeof(ByteCode) - (sizeof(size_t) + sizeof(Tcl_Time)));
    literalMgmtBytes = sizeof(LiteralTable)
	    + (iPtr->literalTable.numBuckets * sizeof(LiteralEntry *))
	    + (iPtr->literalTable.numEntries * sizeof(LiteralEntry));
    currentLiteralBytes = literalMgmtBytes
	    + iPtr->literalTable.numEntries * sizeof(Tcl_Obj)
	    + statsPtr->currentLitStringBytes;
    currentCodeBytes = statsPtr->currentByteCodeBytes + currentLiteralBytes;
    
    /*
     * Summary statistics, total and current source and ByteCode sizes.
     */

    fprintf(stdout, "\n----------------------------------------------------------------\n");
    fprintf(stdout,
	    "Compilation and execution statistics for interpreter 0x%x\n",
	    (unsigned int) iPtr);

    fprintf(stdout, "\nNumber ByteCodes executed	%ld\n",
	    statsPtr->numExecutions);
    fprintf(stdout, "Number ByteCodes compiled	%ld\n",
	    statsPtr->numCompilations);
    fprintf(stdout, "  Mean executions/compile	%.1f\n",
	    ((float)statsPtr->numExecutions) / ((float)statsPtr->numCompilations));
    
    fprintf(stdout, "\nInstructions executed		%.0f\n",
	    numInstructions);
    fprintf(stdout, "  Mean inst/compile		%.0f\n",
	    numInstructions / statsPtr->numCompilations);
    fprintf(stdout, "  Mean inst/execution		%.0f\n",
	    numInstructions / statsPtr->numExecutions);

    fprintf(stdout, "\nTotal ByteCodes			%ld\n",
	    statsPtr->numCompilations);
    fprintf(stdout, "  Source bytes			%.6g\n",
	    statsPtr->totalSrcBytes);
    fprintf(stdout, "  Code bytes			%.6g\n",
	    totalCodeBytes);
    fprintf(stdout, "    ByteCode bytes		%.6g\n",
	    statsPtr->totalByteCodeBytes);
    fprintf(stdout, "    Literal bytes		%.6g\n",
	    totalLiteralBytes);
    fprintf(stdout, "      table %d + bkts %d + entries %ld + objects %ld + strings %.6g\n",
	    sizeof(LiteralTable),
	    iPtr->literalTable.numBuckets * sizeof(LiteralEntry *),
	    statsPtr->numLiteralsCreated * sizeof(LiteralEntry),
	    statsPtr->numLiteralsCreated * sizeof(Tcl_Obj),
	    statsPtr->totalLitStringBytes);
    fprintf(stdout, "  Mean code/compile		%.1f\n",
	    totalCodeBytes / statsPtr->numCompilations);
    fprintf(stdout, "  Mean code/source		%.1f\n",
	    totalCodeBytes / statsPtr->totalSrcBytes);

    fprintf(stdout, "\nCurrent (active) ByteCodes	%ld\n",
	    numCurrentByteCodes);
    fprintf(stdout, "  Source bytes			%.6g\n",
	    statsPtr->currentSrcBytes);
    fprintf(stdout, "  Code bytes			%.6g\n",
	    currentCodeBytes);
    fprintf(stdout, "    ByteCode bytes		%.6g\n",
	    statsPtr->currentByteCodeBytes);
    fprintf(stdout, "    Literal bytes		%.6g\n",
	    currentLiteralBytes);
    fprintf(stdout, "      table %d + bkts %d + entries %d + objects %d + strings %.6g\n",
	    sizeof(LiteralTable),
	    iPtr->literalTable.numBuckets * sizeof(LiteralEntry *),
	    iPtr->literalTable.numEntries * sizeof(LiteralEntry),
	    iPtr->literalTable.numEntries * sizeof(Tcl_Obj),
	    statsPtr->currentLitStringBytes);
    fprintf(stdout, "  Mean code/source		%.1f\n",
	    currentCodeBytes / statsPtr->currentSrcBytes);
    fprintf(stdout, "  Code + source bytes		%.6g (%0.1f mean code/src)\n",
	    (currentCodeBytes + statsPtr->currentSrcBytes),
	    (currentCodeBytes / statsPtr->currentSrcBytes) + 1.0);

    /*
     * Tcl_IsShared statistics check
     *
     * This gives the refcount of each obj as Tcl_IsShared was called
     * for it.  Shared objects must be duplicated before they can be
     * modified.
     */

    numSharedMultX = 0;
    fprintf(stdout, "\nTcl_IsShared object check (all objects):\n");
    fprintf(stdout, "  Object had refcount <=1 (not shared)	%ld\n",
	    tclObjsShared[1]);
    for (i = 2;  i < TCL_MAX_SHARED_OBJ_STATS;  i++) {
	fprintf(stdout, "  refcount ==%d		%ld\n",
		i, tclObjsShared[i]);
	numSharedMultX += tclObjsShared[i];
    }
    fprintf(stdout, "  refcount >=%d		%ld\n",
	    i, tclObjsShared[0]);
    numSharedMultX += tclObjsShared[0];
    fprintf(stdout, "  Total shared objects			%d\n",
	    numSharedMultX);

    /*
     * Literal table statistics.
     */

    numByteCodeLits = 0;
    refCountSum = 0;
    numSharedMultX = 0;
    numSharedOnce  = 0;
    objBytesIfUnshared  = 0.0;
    strBytesIfUnshared  = 0.0;
    strBytesSharedMultX = 0.0;
    strBytesSharedOnce  = 0.0;
    for (i = 0;  i < globalTablePtr->numBuckets;  i++) {
	for (entryPtr = globalTablePtr->buckets[i];  entryPtr != NULL;
	        entryPtr = entryPtr->nextPtr) {
	    if (entryPtr->objPtr->typePtr == &tclByteCodeType) {
		numByteCodeLits++;
	    }
	    (void) Tcl_GetStringFromObj(entryPtr->objPtr, &length);
	    refCountSum += entryPtr->refCount;
	    objBytesIfUnshared += (entryPtr->refCount * sizeof(Tcl_Obj));
	    strBytesIfUnshared += (entryPtr->refCount * (length+1));
	    if (entryPtr->refCount > 1) {
		numSharedMultX++;
		strBytesSharedMultX += (length+1);
	    } else {
		numSharedOnce++;
		strBytesSharedOnce += (length+1);
	    }
	}
    }
    sharingBytesSaved = (objBytesIfUnshared + strBytesIfUnshared)
	    - currentLiteralBytes;

    fprintf(stdout, "\nTotal objects (all interps)	%ld\n",
	    tclObjsAlloced);
    fprintf(stdout, "Current objects			%ld\n",
	    (tclObjsAlloced - tclObjsFreed));
    fprintf(stdout, "Total literal objects		%ld\n",
	    statsPtr->numLiteralsCreated);

    fprintf(stdout, "\nCurrent literal objects		%d (%0.1f%% of current objects)\n",
	    globalTablePtr->numEntries,
	    (globalTablePtr->numEntries * 100.0) / (tclObjsAlloced-tclObjsFreed));
    fprintf(stdout, "  ByteCode literals	 	%ld (%0.1f%% of current literals)\n",
	    numByteCodeLits,
	    (numByteCodeLits * 100.0) / globalTablePtr->numEntries);
    fprintf(stdout, "  Literals reused > 1x	 	%d\n",
	    numSharedMultX);
    fprintf(stdout, "  Mean reference count	 	%.2f\n",
	    ((double) refCountSum) / globalTablePtr->numEntries);
    fprintf(stdout, "  Mean len, str reused >1x 	%.2f\n",
	    (numSharedMultX? (strBytesSharedMultX/numSharedMultX) : 0.0));
    fprintf(stdout, "  Mean len, str used 1x	 	%.2f\n",
	    (numSharedOnce? (strBytesSharedOnce/numSharedOnce) : 0.0));
    fprintf(stdout, "  Total sharing savings	 	%.6g (%0.1f%% of bytes if no sharing)\n",
	    sharingBytesSaved,
	    (sharingBytesSaved * 100.0) / (objBytesIfUnshared + strBytesIfUnshared));
    fprintf(stdout, "    Bytes with sharing		%.6g\n",
	    currentLiteralBytes);
    fprintf(stdout, "      table %d + bkts %d + entries %d + objects %d + strings %.6g\n",
	    sizeof(LiteralTable),
	    iPtr->literalTable.numBuckets * sizeof(LiteralEntry *),
	    iPtr->literalTable.numEntries * sizeof(LiteralEntry),
	    iPtr->literalTable.numEntries * sizeof(Tcl_Obj),
	    statsPtr->currentLitStringBytes);
    fprintf(stdout, "    Bytes if no sharing		%.6g = objects %.6g + strings %.6g\n",
	    (objBytesIfUnshared + strBytesIfUnshared),
	    objBytesIfUnshared, strBytesIfUnshared);
    fprintf(stdout, "  String sharing savings 	%.6g = unshared %.6g - shared %.6g\n",
	    (strBytesIfUnshared - statsPtr->currentLitStringBytes),
	    strBytesIfUnshared, statsPtr->currentLitStringBytes);
    fprintf(stdout, "  Literal mgmt overhead	 	%ld (%0.1f%% of bytes with sharing)\n",
	    literalMgmtBytes,
	    (literalMgmtBytes * 100.0) / currentLiteralBytes);
    fprintf(stdout, "    table %d + buckets %d + entries %d\n",
	    sizeof(LiteralTable),
	    iPtr->literalTable.numBuckets * sizeof(LiteralEntry *),
	    iPtr->literalTable.numEntries * sizeof(LiteralEntry));

    /*
     * Breakdown of current ByteCode space requirements.
     */
    
    fprintf(stdout, "\nBreakdown of current ByteCode requirements:\n");
    fprintf(stdout, "                         Bytes      Pct of    Avg per\n");
    fprintf(stdout, "                                     total    ByteCode\n");
    fprintf(stdout, "Total             %12.6g     100.00%%   %8.1f\n",
	    statsPtr->currentByteCodeBytes,
	    statsPtr->currentByteCodeBytes / numCurrentByteCodes);
    fprintf(stdout, "Header            %12.6g   %8.1f%%   %8.1f\n",
	    currentHeaderBytes,
	    ((currentHeaderBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    currentHeaderBytes / numCurrentByteCodes);
    fprintf(stdout, "Instructions      %12.6g   %8.1f%%   %8.1f\n",
	    statsPtr->currentInstBytes,
	    ((statsPtr->currentInstBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    statsPtr->currentInstBytes / numCurrentByteCodes);
    fprintf(stdout, "Literal ptr array %12.6g   %8.1f%%   %8.1f\n",
	    statsPtr->currentLitBytes,
	    ((statsPtr->currentLitBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    statsPtr->currentLitBytes / numCurrentByteCodes);
    fprintf(stdout, "Exception table   %12.6g   %8.1f%%   %8.1f\n",
	    statsPtr->currentExceptBytes,
	    ((statsPtr->currentExceptBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    statsPtr->currentExceptBytes / numCurrentByteCodes);
    fprintf(stdout, "Auxiliary data    %12.6g   %8.1f%%   %8.1f\n",
	    statsPtr->currentAuxBytes,
	    ((statsPtr->currentAuxBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    statsPtr->currentAuxBytes / numCurrentByteCodes);
    fprintf(stdout, "Command map       %12.6g   %8.1f%%   %8.1f\n",
	    statsPtr->currentCmdMapBytes,
	    ((statsPtr->currentCmdMapBytes * 100.0) / statsPtr->currentByteCodeBytes),
	    statsPtr->currentCmdMapBytes / numCurrentByteCodes);

    /*
     * Detailed literal statistics.
     */
    
    fprintf(stdout, "\nLiteral string sizes:\n");
    fprintf(stdout, "	 Up to length		Percentage\n");
    maxSizeDecade = 0;
    for (i = 31;  i >= 0;  i--) {
        if (statsPtr->literalCount[i] > 0) {
            maxSizeDecade = i;
	    break;
        }
    }
    sum = 0;
    for (i = 0;  i <= maxSizeDecade;  i++) {
	decadeHigh = (1 << (i+1)) - 1;
	sum += statsPtr->literalCount[i];
        fprintf(stdout,	"	%10d		%8.0f%%\n",
		decadeHigh, (sum * 100.0) / statsPtr->numLiteralsCreated);
    }

    litTableStats = TclLiteralStats(globalTablePtr);
    fprintf(stdout, "\nCurrent literal table statistics:\n%s\n",
            litTableStats);
    ckfree((char *) litTableStats);

    /*
     * Source and ByteCode size distributions.
     */

    fprintf(stdout, "\nSource sizes:\n");
    fprintf(stdout, "	 Up to size		Percentage\n");
    minSizeDecade = maxSizeDecade = 0;
    for (i = 0;  i < 31;  i++) {
        if (statsPtr->srcCount[i] > 0) {
	    minSizeDecade = i;
	    break;
        }
    }
    for (i = 31;  i >= 0;  i--) {
        if (statsPtr->srcCount[i] > 0) {
            maxSizeDecade = i;
	    break;
        }
    }
    sum = 0;
    for (i = minSizeDecade;  i <= maxSizeDecade;  i++) {
	decadeHigh = (1 << (i+1)) - 1;
	sum += statsPtr->srcCount[i];
        fprintf(stdout,	"	%10d		%8.0f%%\n",
		decadeHigh, (sum * 100.0) / statsPtr->numCompilations);
    }

    fprintf(stdout, "\nByteCode sizes:\n");
    fprintf(stdout, "	 Up to size		Percentage\n");
    minSizeDecade = maxSizeDecade = 0;
    for (i = 0;  i < 31;  i++) {
        if (statsPtr->byteCodeCount[i] > 0) {
	    minSizeDecade = i;
	    break;
        }
    }
    for (i = 31;  i >= 0;  i--) {
        if (statsPtr->byteCodeCount[i] > 0) {
            maxSizeDecade = i;
	    break;
        }
    }
    sum = 0;
    for (i = minSizeDecade;  i <= maxSizeDecade;  i++) {
	decadeHigh = (1 << (i+1)) - 1;
	sum += statsPtr->byteCodeCount[i];
        fprintf(stdout,	"	%10d		%8.0f%%\n",
		decadeHigh, (sum * 100.0) / statsPtr->numCompilations);
    }

    fprintf(stdout, "\nByteCode longevity (excludes Current ByteCodes):\n");
    fprintf(stdout, "	       Up to ms		Percentage\n");
    minSizeDecade = maxSizeDecade = 0;
    for (i = 0;  i < 31;  i++) {
        if (statsPtr->lifetimeCount[i] > 0) {
	    minSizeDecade = i;
	    break;
        }
    }
    for (i = 31;  i >= 0;  i--) {
        if (statsPtr->lifetimeCount[i] > 0) {
            maxSizeDecade = i;
	    break;
        }
    }
    sum = 0;
    for (i = minSizeDecade;  i <= maxSizeDecade;  i++) {
	decadeHigh = (1 << (i+1)) - 1;
	sum += statsPtr->lifetimeCount[i];
        fprintf(stdout,	"	%12.3f		%8.0f%%\n",
		decadeHigh / 1000.0,
		(sum * 100.0) / statsPtr->numByteCodesFreed);
    }

    /*
     * Instruction counts.
     */

    fprintf(stdout, "\nInstruction counts:\n");
    for (i = 0;  i <= LAST_INST_OPCODE;  i++) {
        if (statsPtr->instructionCount[i]) {
            fprintf(stdout, "%20s %8ld %6.1f%%\n",
		    instructionTable[i].name,
		    statsPtr->instructionCount[i],
		    (statsPtr->instructionCount[i]*100.0) / numInstructions);
        }
    }

    fprintf(stdout, "\nInstructions NEVER executed:\n");
    for (i = 0;  i <= LAST_INST_OPCODE;  i++) {
        if (statsPtr->instructionCount[i] == 0) {
            fprintf(stdout, "%20s\n",
		    instructionTable[i].name);
        }
    }

#ifdef TCL_MEM_DEBUG
    fprintf(stdout, "\nHeap Statistics:\n");
    TclDumpMemoryInfo(stdout);
#endif
    fprintf(stdout, "\n----------------------------------------------------------------\n");
    return TCL_OK;
}
#endif /* TCL_COMPILE_STATS */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetCommandFromObj --
 *
 *      Returns the command specified by the name in a Tcl_Obj.
 *
 * Results:
 *	Returns a token for the command if it is found. Otherwise, if it
 *	can't be found or there is an error, returns NULL.
 *
 * Side effects:
 *      May update the internal representation for the object, caching
 *      the command reference so that the next time this procedure is
 *	called with the same object, the command can be found quickly.
 *
 *----------------------------------------------------------------------
 */
Tcl_Command
Tcl_GetCommandFromObj(interp, objPtr)
    Tcl_Interp *interp;		/* The interpreter in which to resolve the
				 * command and to report errors. */
    register Tcl_Obj *objPtr;	/* The object containing the command's
				 * name. If the name starts with "::", will
				 * be looked up in global namespace. Else,
				 * looked up first in the current namespace
				 * if contextNsPtr is NULL, then in global
				 * namespace. */
{
    Interp *iPtr = (Interp *) interp;
    register ResolvedCmdName *resPtr;
    register Command *cmdPtr;
    Namespace *currNsPtr;
    int result;

    /*
     * Get the internal representation, converting to a command type if
     * needed. The internal representation is a ResolvedCmdName that points
     * to the actual command.
     */
    
    if (objPtr->typePtr != &tclCmdNameType) {
        result = tclCmdNameType.setFromAnyProc(interp, objPtr);
        if (result != TCL_OK) {
            return (Tcl_Command) NULL;
        } 
	resPtr = (ResolvedCmdName *) objPtr->internalRep.otherValuePtr;
	if (resPtr != NULL) return (Tcl_Command) resPtr->cmdPtr;
    }

    resPtr = (ResolvedCmdName *) objPtr->internalRep.otherValuePtr;

    /*
     * Get the current namespace.
     */
	
    if (iPtr->varFramePtr != NULL) {
	currNsPtr = iPtr->varFramePtr->nsPtr;
    } else {
	currNsPtr = iPtr->globalNsPtr;
    }
	
    /*
     * Check the context namespace and the namespace epoch of the resolved
     * symbol to make sure that it is fresh. If not, then force another
     * conversion to the command type, to discard the old rep and create a
     * new one. Note that we verify that the namespace id of the context
     * namespace is the same as the one we cached; this insures that the
     * namespace wasn't deleted and a new one created at the same address
     * with the same command epoch.
     */
    
    if ((resPtr != NULL)
	&& (resPtr->refNsPtr == currNsPtr)
	&& (resPtr->refNsId == currNsPtr->nsId)
	&& (resPtr->refNsCmdEpoch == currNsPtr->cmdRefEpoch)) {
	cmdPtr = resPtr->cmdPtr;
	if (cmdPtr->cmdEpoch == resPtr->cmdEpoch) {
	    return (Tcl_Command) cmdPtr;
	} 
    }

    result = tclCmdNameType.setFromAnyProc(interp, objPtr);
    if (result != TCL_OK) {
	return (Tcl_Command) NULL;
    }
    resPtr = (ResolvedCmdName *) objPtr->internalRep.otherValuePtr;
    if (resPtr != NULL) {
	return (Tcl_Command) resPtr->cmdPtr;
    } else {
	return (Tcl_Command) NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclSetCmdNameObj --
 *
 *	Modify an object to be an CmdName object that refers to the argument
 *	Command structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's old internal rep is freed. It's string rep is not
 *	changed. The refcount in the Command structure is incremented to
 *	keep it from being freed if the command is later deleted until
 *	TclExecuteByteCode has a chance to recognize that it was deleted.
 *
 *----------------------------------------------------------------------
 */

void
TclSetCmdNameObj(interp, objPtr, cmdPtr)
    Tcl_Interp *interp;		/* Points to interpreter containing command
				 * that should be cached in objPtr. */
    register Tcl_Obj *objPtr;	/* Points to Tcl object to be changed to
				 * a CmdName object. */
    Command *cmdPtr;		/* Points to Command structure that the
				 * CmdName object should refer to. */
{
    Interp *iPtr = (Interp *) interp;
    register ResolvedCmdName *resPtr;
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    register Namespace *currNsPtr;

    if (oldTypePtr == &tclCmdNameType) {
	return;
    }
    
    /*
     * Get the current namespace.
     */
    
    if (iPtr->varFramePtr != NULL) {
	currNsPtr = iPtr->varFramePtr->nsPtr;
    } else {
	currNsPtr = iPtr->globalNsPtr;
    }
    
    cmdPtr->refCount++;
    resPtr = (ResolvedCmdName *) ckalloc(sizeof(ResolvedCmdName));
    resPtr->cmdPtr = cmdPtr;
    resPtr->refNsPtr = currNsPtr;
    resPtr->refNsId  = currNsPtr->nsId;
    resPtr->refNsCmdEpoch = currNsPtr->cmdRefEpoch;
    resPtr->cmdEpoch = cmdPtr->cmdEpoch;
    resPtr->refCount = 1;
    
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    objPtr->internalRep.twoPtrValue.ptr1 = (VOID *) resPtr;
    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    objPtr->typePtr = &tclCmdNameType;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeCmdNameInternalRep --
 *
 *	Frees the resources associated with a cmdName object's internal
 *	representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Decrements the ref count of any cached ResolvedCmdName structure
 *	pointed to by the cmdName's internal representation. If this is 
 *	the last use of the ResolvedCmdName, it is freed. This in turn
 *	decrements the ref count of the Command structure pointed to by 
 *	the ResolvedSymbol, which may free the Command structure.
 *
 *----------------------------------------------------------------------
 */

static void
FreeCmdNameInternalRep(objPtr)
    register Tcl_Obj *objPtr;	/* CmdName object with internal
				 * representation to free. */
{
    register ResolvedCmdName *resPtr =
	(ResolvedCmdName *) objPtr->internalRep.otherValuePtr;

    if (resPtr != NULL) {
	/*
	 * Decrement the reference count of the ResolvedCmdName structure.
	 * If there are no more uses, free the ResolvedCmdName structure.
	 */
    
        resPtr->refCount--;
        if (resPtr->refCount == 0) {
            /*
	     * Now free the cached command, unless it is still in its
             * hash table or if there are other references to it
             * from other cmdName objects.
	     */
	    
            Command *cmdPtr = resPtr->cmdPtr;
            TclCleanupCommand(cmdPtr);
            ckfree((char *) resPtr);
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DupCmdNameInternalRep --
 *
 *	Initialize the internal representation of an cmdName Tcl_Obj to a
 *	copy of the internal representation of an existing cmdName object. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"copyPtr"s internal rep is set to point to the ResolvedCmdName
 *	structure corresponding to "srcPtr"s internal rep. Increments the
 *	ref count of the ResolvedCmdName structure pointed to by the
 *	cmdName's internal representation.
 *
 *----------------------------------------------------------------------
 */

static void
DupCmdNameInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    register Tcl_Obj *copyPtr;	/* Object with internal rep to set. */
{
    register ResolvedCmdName *resPtr =
        (ResolvedCmdName *) srcPtr->internalRep.otherValuePtr;

    copyPtr->internalRep.twoPtrValue.ptr1 = (VOID *) resPtr;
    copyPtr->internalRep.twoPtrValue.ptr2 = NULL;
    if (resPtr != NULL) {
        resPtr->refCount++;
    }
    copyPtr->typePtr = &tclCmdNameType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetCmdNameFromAny --
 *
 *	Generate an cmdName internal form for the Tcl object "objPtr".
 *
 * Results:
 *	The return value is a standard Tcl result. The conversion always
 *	succeeds and TCL_OK is returned.
 *
 * Side effects:
 *	A pointer to a ResolvedCmdName structure that holds a cached pointer
 *	to the command with a name that matches objPtr's string rep is
 *	stored as objPtr's internal representation. This ResolvedCmdName
 *	pointer will be NULL if no matching command was found. The ref count
 *	of the cached Command's structure (if any) is also incremented.
 *
 *----------------------------------------------------------------------
 */

static int
SetCmdNameFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr;	/* The object to convert. */
{
    Interp *iPtr = (Interp *) interp;
    char *name;
    Tcl_Command cmd;
    register Command *cmdPtr;
    Namespace *currNsPtr;
    register ResolvedCmdName *resPtr;

    /*
     * Get "objPtr"s string representation. Make it up-to-date if necessary.
     */

    name = objPtr->bytes;
    if (name == NULL) {
	name = Tcl_GetString(objPtr);
    }

    /*
     * Find the Command structure, if any, that describes the command called
     * "name". Build a ResolvedCmdName that holds a cached pointer to this
     * Command, and bump the reference count in the referenced Command
     * structure. A Command structure will not be deleted as long as it is
     * referenced from a CmdName object.
     */

    cmd = Tcl_FindCommand(interp, name, (Tcl_Namespace *) NULL,
	    /*flags*/ 0);
    cmdPtr = (Command *) cmd;
    if (cmdPtr != NULL) {
	/*
	 * Get the current namespace.
	 */
	
	if (iPtr->varFramePtr != NULL) {
	    currNsPtr = iPtr->varFramePtr->nsPtr;
	} else {
	    currNsPtr = iPtr->globalNsPtr;
	}
	
	cmdPtr->refCount++;
        resPtr = (ResolvedCmdName *) ckalloc(sizeof(ResolvedCmdName));
        resPtr->cmdPtr        = cmdPtr;
        resPtr->refNsPtr      = currNsPtr;
        resPtr->refNsId       = currNsPtr->nsId;
        resPtr->refNsCmdEpoch = currNsPtr->cmdRefEpoch;
        resPtr->cmdEpoch      = cmdPtr->cmdEpoch;
        resPtr->refCount      = 1;
    } else {
	resPtr = NULL;	/* no command named "name" was found */
    }

    /*
     * Free the old internalRep before setting the new one. We do this as
     * late as possible to allow the conversion code, in particular
     * GetStringFromObj, to use that old internalRep. If no Command
     * structure was found, leave NULL as the cached value.
     */

    if ((objPtr->typePtr != NULL)
	    && (objPtr->typePtr->freeIntRepProc != NULL)) {
	objPtr->typePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.twoPtrValue.ptr1 = (VOID *) resPtr;
    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    objPtr->typePtr = &tclCmdNameType;
    return TCL_OK;
}

#ifdef TCL_COMPILE_DEBUG
/*
 *----------------------------------------------------------------------
 *
 * StringForResultCode --
 *
 *	Procedure that returns a human-readable string representing a
 *	Tcl result code such as TCL_ERROR. 
 *
 * Results:
 *	If the result code is one of the standard Tcl return codes, the
 *	result is a string representing that code such as "TCL_ERROR".
 *	Otherwise, the result string is that code formatted as a
 *	sequence of decimal digit characters. Note that the resulting
 *	string must not be modified by the caller.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
StringForResultCode(result)
    int result;			/* The Tcl result code for which to
				 * generate a string. */
{
    static char buf[TCL_INTEGER_SPACE];
    
    if ((result >= TCL_OK) && (result <= TCL_CONTINUE)) {
	return resultStrings[result];
    }
    TclFormatInt(buf, result);
    return buf;
}
#endif /* TCL_COMPILE_DEBUG */
