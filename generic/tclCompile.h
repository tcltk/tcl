/*
 * tclCompile.h --
 *
 * Copyright (c) 1996-1998 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 * Copyright (c) 2001 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclCompile.h,v 1.53.2.11 2005/03/16 10:07:56 msofer Exp $
 */

#ifndef _TCLCOMPILATION
#define _TCLCOMPILATION 1

#include "tclInt.h"

/*
 *------------------------------------------------------------------------
 * Variables related to compilation. These are used in tclCompile.c,
 * tclExecute.c, tclBasic.c, and their clients.
 *------------------------------------------------------------------------
 */

#ifdef TCL_COMPILE_DEBUG
/*
 * Variable that controls whether compilation tracing is enabled and, if so,
 * what level of tracing is desired:
 *    0: no compilation tracing
 *    1: summarize compilation of top level cmds and proc bodies
 *    2: display all instructions of each ByteCode compiled
 * This variable is linked to the Tcl variable "tcl_traceCompile".
 */

MODULE_SCOPE int 	tclTraceCompile;
#endif

#ifdef TCL_COMPILE_DEBUG
/*
 * Variable that controls whether execution tracing is enabled and, if so,
 * what level of tracing is desired:
 *    0: no execution tracing
 *    1: trace invocations of Tcl procs only
 *    2: trace invocations of all (not compiled away) commands
 *    3: display each instruction executed
 * This variable is linked to the Tcl variable "tcl_traceExec".
 */

MODULE_SCOPE int 	tclTraceExec;
#endif

/*
 *------------------------------------------------------------------------
 * Data structures related to compilation.
 *------------------------------------------------------------------------
 */

/*
 * The structure used to implement Tcl "exceptions" (exceptional returns):
 * those generated in loops by the break and continue commands. This
 * ExceptionRange structure describes a range of code (a loop body), the kind
 * of exceptions (break or continue) that might occur, and the PC offsets to
 * jump to if a matching exception does occur. Exception ranges can nest, and
 * the code *assumes* that the latest-created active exception is the
 * innermost. In this manner, by scanning the exceptions from last-to-first,
 * the first match corresponds to the innermost active exception range. In
 * practical terms, this means that the compiler *must* define outer ranges
 * before inner ranges, which happens to be the natural thing to do.
 * These structures are also generated for the "next" subcommands of for loops 
 * since a break there terminates the for command. This means a for command
 * actually generates two LoopInfo structures.
 */

typedef struct ExceptionRange {
    int codeOffset;		/* Offset of the first instruction byte of
				 * the code range. */
    int numCodeWords;		/* Number of words in the code range. */
    int breakOffset;		/* If LOOP_EXCEPTION_RANGE, the target PC
				 * offset for a break command in the range. */
    int continueOffset;		/* If LOOP_EXCEPTION_RANGE and not -1, the
				 * target PC offset for a continue command in
				 * the code range. Otherwise, ignore this range
				 * when processing a continue command. */
} ExceptionRange;

/*
 * Structure used to map between instruction pc and source locations. It
 * defines for each compiled Tcl command its code's starting offset and 
 * its source's starting offset and length. Note that the code offset
 * increases monotonically: that is, the table is sorted in code offset
 * order. The source offset is not monotonic.
 */

typedef struct CmdLocation {
    int codeOffset;		/* Offset of first word of command code. */
    int numCodeWords;		/* Number of words for command's code. */
    int srcOffset;		/* Offset of first char of the command. */
    int numSrcBytes;		/* Number of command source chars. */
} CmdLocation;

/*
 * CompileProcs need the ability to record information during compilation
 * that can be used by bytecode instructions during execution. The AuxData
 * structure provides this "auxiliary data" mechanism. An arbitrary number
 * of these structures can be stored in the ByteCode record (during
 * compilation they are stored in a CompileEnv structure). Each AuxData
 * record holds one word of client-specified data (often a pointer) and is
 * given an index that instructions can later use to look up the structure
 * and its data.
 *
 * The following definitions declare the types of procedures that are called
 * to duplicate or free this auxiliary data when the containing ByteCode
 * objects are duplicated and freed. Pointers to these procedures are kept
 * in the AuxData structure.
 */

typedef ClientData (AuxDataDupProc)  _ANSI_ARGS_((ClientData clientData));
typedef void       (AuxDataFreeProc) _ANSI_ARGS_((ClientData clientData));

/*
 * We define a separate AuxDataType struct to hold type-related information
 * for the AuxData structure. This separation makes it possible for clients
 * outside of the TCL core to manipulate (in a limited fashion!) AuxData;
 * for example, it makes it possible to pickle and unpickle AuxData structs.
 */

typedef struct AuxDataType {
    char *name;			/* the name of the type. Types can be
				 * registered and found by name */
    AuxDataDupProc *dupProc;	/* Callback procedure to invoke when the
				 * aux data is duplicated (e.g., when the
				 * ByteCode structure containing the aux
				 * data is duplicated). NULL means just
				 * copy the source clientData bits; no
				 * proc need be called. */
    AuxDataFreeProc *freeProc;	/* Callback procedure to invoke when the
				 * aux data is freed. NULL means no
				 * proc need be called. */
} AuxDataType;

/*
 * The definition of the AuxData structure that holds information created
 * during compilation by CompileProcs and used by instructions during
 * execution.
 */

typedef struct AuxData {
    AuxDataType *type;		/* pointer to the AuxData type associated with
				 * this ClientData. */
    ClientData clientData;	/* The compilation data itself. */
} AuxData;

/*
 * Macros and ifdefs that define the width of instructions and operands. The 
 * objective is:
 *   - an instruction has the width of a pointer - in preparation for it
 *     really being a pointer to a jump target
 *   - an instruction's operands are pointer-sized, and can beone of : a
 *     pointer, a pointer-sized integer, or (one signed and one unsigned)
 *     integers of half-pointer size.
 *
 * Valuable properties of this design include:
 *   - the instruction stream is always aligned
 *   - it permits a simpler and faster code optimiser
 *   - it permits faster branching, storing absolute pointers as operands for
 *     jump instructions
 *   - it permits a faster TEBC (instructions as jump targets, at least using
 *     pointers-as-values in gcc/icc. May need some asm magic on other
 *     platforms).
 *
 * The downside is that the instruction stream is much larger. The increase in
 * memory traffic may or may not override the advantages, we'll measure that.
 *
 * Note that the instruction stream is not portable; not accross platforms,
 * not to a different process on the same platform (when using absolute
 * pointers). If this design is adopted, a portable compact code will be
 * designed. In principle it can be generated by the same compiler, with a
 * different second stage to replace the optimiser.
 */

#if defined(__WIN32_) 
#define TclPSizedInt      int
#define TclHalfPSizedInt  short
#elif defined(__WIN64__)
#define TclPSizedInt      Tcl_WideInt 
#define TclHalfPSizedInt  int
#else /* start of NOT WIN */
#if ((SIZEOF_LONG <= SIZEOF_VOID_P) && (SIZEOF_LONG != SIZEOF_INT))
#define TclPSizedInt      long
#elif (SIZEOF_INT <= SIZEOF_VOID_P)
#define TclPSizedInt      int
#elif (SIZEOF_SHORT <= SIZEOF_VOID_P)
#define TclPSizedInt      short
#else
    Should not happen
    (this text here to make the compiler barf)
#endif /* Define TclPSizedInt */
#if ((2*SIZEOF_LONG <= SIZEOF_VOID_P) && (SIZEOF_LONG != SIZEOF_INT))
#define TclHalfPSizedInt  long
#elif (2*SIZEOF_INT <= SIZEOF_VOID_P)
#define TclHalfPSizedInt  int
#elif (2*SIZEOF_SHORT <= SIZEOF_VOID_P)
#define TclHalfPSizedInt  short
#else
#define NO_HALFP_SIZED_INT    
#endif /* Define TclHaldPSizedInt */
#endif

typedef union TclVMOpnd {
    void *p;
    TclPSizedInt  i;
} TclVMOpnd;

typedef struct TclVMWord {
    TclPSizedInt inst;
    TclVMOpnd    opnd;
} TclVMWord;

/*
 * Macros to stash/extract one signed and one unsigned half-length ints
 * into/from a pointer-sized int.
 */
#define PINT_MAX (~(((TclPSizedInt)1)<<(8*sizeof(TclPSizedInt)-1)))
#define PINT_MIN (-PINT_MAX -1)

#define HP_SHIFT (4*sizeof(TclPSizedInt))
#define HP_MASK  (PINT_MAX>>(HP_SHIFT-1))

#define HPUINT_MAX HP_MASK
#define HPINT_MAX  (HP_MASK>>1)
#define HPINT_MIN  (-HPINT_MAX-1)

#define HP_STASH(full, n, u) \
    (full) = ((((TclPSizedInt) n) << HP_SHIFT) | (u))

#define HP_EXTRACT(full, n, u)\
    (n) = ((full) >> HP_SHIFT);\
    (u) = ((full) &  HP_MASK)


    
#define TclVMGetInstAtPtr(p) (*(p)).inst
#define TclVMGetOpndAtPtr(p) (*(p)).opnd.i

#define TclVMStoreInstAtPtr(instruction, p) \
    (*(p)).inst = (TclPSizedInt) (instruction)

#define TclVMStoreOpndAtPtr(operand, p) \
    (*(p)).opnd.i = (TclPSizedInt) (operand)


#define TclVMGetInstAndOpAtPtr(p, instruction, operand) \
    (instruction) = TclVMGetInstAtPtr(p);\
    (operand)     = TclVMGetOpndAtPtr(p) 

#define TclVMStoreWordAtPtr(instruction, operand, p)\
    (*(p)).inst   = (TclPSizedInt) (instruction);\
    (*(p)).opnd.i = (TclPSizedInt) (operand)



/*
 * Structure defining the entries in the runtime catch stack.
 */

typedef struct catchItem {
    int stackTop;
    TclVMWord *pc;
} catchItem;
#define CATCH_ITEM_SIZE 2

/*
 * Structure defining the compilation environment. After compilation, fields
 * describing bytecode instructions are copied out into the more compact
 * ByteCode structure defined below.
 */

#define COMPILEENV_INIT_CODE_WORDS    250
#define COMPILEENV_INIT_NUM_OBJECTS    60
#define COMPILEENV_INIT_EXCEPT_RANGES   5
#define COMPILEENV_INIT_CMD_MAP_SIZE   40
#define COMPILEENV_INIT_AUX_DATA_SIZE   5

typedef struct CompileEnv {
    Interp *iPtr;		/* Interpreter containing the code being
				 * compiled. Commands and their compile
				 * procs are specific to an interpreter so
				 * the code emitted will depend on the
				 * interpreter. */
    char *source;		/* The source string being compiled by
				 * SetByteCodeFromAny. This pointer is not
				 * owned by the CompileEnv and must not be
				 * freed or changed by it. */
    int numSrcBytes;		/* Number of bytes in source. */
    Proc *procPtr;		/* If a procedure is being compiled, a
				 * pointer to its Proc structure; otherwise
				 * NULL. Used to compile local variables.
				 * Set from information provided by
				 * ObjInterpProc in tclProc.c. */
    int numCommands;		/* Number of commands compiled. */
    int catchDepth;		/* Current catch range nesting level;
				 * 0 if not in any range currently. */
    int maxCatchDepth;		/* Max nesting level of catch ranges;
				 * 0 if no ranges have been compiled. */
    int maxStackDepth;		/* Maximum number of stack elements needed
				 * to execute the code. Set by compilation
				 * procedures before returning. */
    int currStackDepth;		/* Current stack depth. */
    LiteralTable localLitTable;	/* Contains LiteralEntry's describing
				 * all Tcl objects referenced by this
				 * compiled code. Indexed by the string
				 * representations of the literals. Used to
				 * avoid creating duplicate objects. */
    TclVMWord *codeStart;	/* Points to the first byte of the code. */
    TclVMWord *codeNext;	/* Points to next code array byte to use. */
    TclVMWord *codeEnd;	/* Points just after the last allocated
				 * code array byte. */
    int mallocedCodeArray;	/* Set 1 if code array was expanded 
				 * and codeStart points into the heap.*/
    LiteralEntry *literalArrayPtr;
    				/* Points to start of LiteralEntry array. */
    int literalArrayNext;	/* Index of next free object array entry. */
    int literalArrayEnd;	/* Index just after last obj array entry. */
    int mallocedLiteralArray;	/* 1 if object array was expanded and
				 * objArray points into the heap, else 0. */
    ExceptionRange *exceptArrayPtr;
    				/* Points to start of the ExceptionRange
				 * array. */
    int exceptArrayCurr;	/* Innermost currently active ExceptionRange
				 * array index; -1 if no range is active, -2 
				 * if the innermost range corresponds to a
				 * catch and not a loop.*/ 
    int exceptArrayNext;	/* Next free ExceptionRange array index.
				 * exceptArrayNext is the number of ranges
				 * and (exceptArrayNext-1) is the index of
				 * the current range's array entry. */
    int exceptArrayEnd;		/* Index after the last ExceptionRange
				 * array entry. */
    int mallocedExceptArray;	/* 1 if ExceptionRange array was expanded
				 * and exceptArrayPtr points in heap,
				 * else 0. */
    CmdLocation *cmdMapPtr;	/* Points to start of CmdLocation array.
				 * numCommands is the index of the next
				 * entry to use; (numCommands-1) is the
				 * entry index for the last command. */
    int cmdMapEnd;		/* Index after last CmdLocation entry. */
    int mallocedCmdMap;		/* 1 if command map array was expanded and
				 * cmdMapPtr points in the heap, else 0. */
    AuxData *auxDataArrayPtr;	/* Points to auxiliary data array start. */
    int auxDataArrayNext;	/* Next free compile aux data array index.
				 * auxDataArrayNext is the number of aux
				 * data items and (auxDataArrayNext-1) is
				 * index of current aux data array entry. */
    int auxDataArrayEnd;	/* Index after last aux data array entry. */
    int mallocedAuxDataArray;	/* 1 if aux data array was expanded and
				 * auxDataArrayPtr points in heap else 0. */
    TclVMWord staticCodeSpace[COMPILEENV_INIT_CODE_WORDS];
				/* Initial storage for code. */
    LiteralEntry staticLiteralSpace[COMPILEENV_INIT_NUM_OBJECTS];
				/* Initial storage of LiteralEntry array. */
    ExceptionRange staticExceptArraySpace[COMPILEENV_INIT_EXCEPT_RANGES];
				/* Initial ExceptionRange array storage. */
    CmdLocation staticCmdMapSpace[COMPILEENV_INIT_CMD_MAP_SIZE];
				/* Initial storage for cmd location map. */
    AuxData staticAuxDataArraySpace[COMPILEENV_INIT_AUX_DATA_SIZE];
				/* Initial storage for aux data array. */
} CompileEnv;

/*
 * The structure defining the bytecode instructions resulting from compiling
 * a Tcl script. Note that this structure is variable length: a single heap
 * object is allocated to hold the ByteCode structure immediately followed
 * by the code bytes, the literal object array, the ExceptionRange array,
 * the CmdLocation map, and the compilation AuxData array.
 */

/*
 * A PRECOMPILED bytecode struct is one that was generated from a compiled
 * image rather than implicitly compiled from source
 */
#define TCL_BYTECODE_PRECOMPILED		0x0001


/*
 * When a bytecode is compiled, interp or namespace resolvers have not been
 * applied yet: this is indicated by the TCL_BYTECODE_RESOLVE_VARS flag.
 */

#define TCL_BYTECODE_RESOLVE_VARS               0x0002

typedef struct ByteCode {
    TclHandle interpHandle;	/* Handle for interpreter containing the
				 * compiled code.  Commands and their compile
				 * procs are specific to an interpreter so the
				 * code emitted will depend on the
				 * interpreter. */
    int compileEpoch;		/* Value of iPtr->compileEpoch when this
				 * ByteCode was compiled. Used to invalidate
				 * code when, e.g., commands with compile
				 * procs are redefined. */
    Namespace *nsPtr;		/* Namespace context in which this code
				 * was compiled. If the code is executed
				 * if a different namespace, it must be
				 * recompiled. */
    int nsEpoch;		/* Value of nsPtr->resolverEpoch when this
				 * ByteCode was compiled. Used to invalidate
				 * code when new namespace resolution rules
				 * are put into effect. */
    int refCount;		/* Reference count: set 1 when created
				 * plus 1 for each execution of the code
				 * currently active. This structure can be
				 * freed when refCount becomes zero. */
    unsigned int flags;		/* flags describing state for the codebyte.
				 * this variable holds ORed values from the
				 * TCL_BYTECODE_ masks defined above */
    char *source;		/* The source string from which this
				 * ByteCode was compiled. Note that this
				 * pointer is not owned by the ByteCode and
				 * must not be freed or modified by it. */
    Proc *procPtr;		/* If the ByteCode was compiled from a
				 * procedure body, this is a pointer to its
				 * Proc structure; otherwise NULL. This
				 * pointer is also not owned by the ByteCode
				 * and must not be freed by it. */
    size_t structureSize;	/* Number of bytes in the ByteCode structure
				 * itself. Does not include heap space for
				 * literal Tcl objects or storage referenced
				 * by AuxData entries. */
    int numCommands;		/* Number of commands compiled. */
    int numSrcBytes;		/* Number of source bytes compiled. */
    int numCodeWords;		/* Number of code words. */
    int numLitObjects;		/* Number of objects in literal array. */
    int numExceptRanges;	/* Number of ExceptionRange array elems. */
    int numAuxDataItems;	/* Number of AuxData items. */
    int numCmdLocBytes;		/* Number of bytes needed for encoded
				 * command location information. */
    int maxCatchDepth;		/* Maximum nesting level of catch ranges;
				 * 0 if no ranges were compiled. */
    int maxStackDepth;		/* Maximum number of stack elements needed
				 * to execute the code. */
    TclVMWord *codeStart;	/* Points to the first byte of the code.
				 * This is just after the final ByteCode
				 * member cmdMapPtr. */
    Tcl_Obj **objArrayPtr;	/* Points to the start of the literal
				 * object array. This is just after the
				 * last code byte. */
    ExceptionRange *exceptArrayPtr;
    				/* Points to the start of the ExceptionRange
				 * array. This is just after the last
				 * object in the object array. */
    AuxData *auxDataArrayPtr;	/* Points to the start of the auxiliary data
				 * array. This is just after the last entry
				 * in the ExceptionRange array. */
    unsigned char *codeDeltaStart;
				/* Points to the first of a sequence of
				 * bytes that encode the change in the
				 * starting offset of each command's code.
				 * If -127<=delta<=127, it is encoded as 1
				 * byte, otherwise 0xFF (128) appears and
				 * the delta is encoded by the next 4 bytes.
				 * Code deltas are always positive. This
				 * sequence is just after the last entry in
				 * the AuxData array. */
    unsigned char *codeLengthStart;
				/* Points to the first of a sequence of
				 * bytes that encode the length of each
				 * command's code. The encoding is the same
				 * as for code deltas. Code lengths are
				 * always positive. This sequence is just
				 * after the last entry in the code delta
				 * sequence. */
    unsigned char *srcDeltaStart;
				/* Points to the first of a sequence of
				 * bytes that encode the change in the
				 * starting offset of each command's source.
				 * The encoding is the same as for code
				 * deltas. Source deltas can be negative.
				 * This sequence is just after the last byte
				 * in the code length sequence. */
    unsigned char *srcLengthStart;
				/* Points to the first of a sequence of
				 * bytes that encode the length of each
				 * command's source. The encoding is the
				 * same as for code deltas. Source lengths
				 * are always positive. This sequence is
				 * just after the last byte in the source
				 * delta sequence. */
#ifdef TCL_COMPILE_STATS
    Tcl_Time createTime;	/* Absolute time when the ByteCode was
				 * created. */
#endif /* TCL_COMPILE_STATS */
} ByteCode;

/*
 * Opcodes for the Tcl bytecode instructions. These must correspond to
 * the entries in the table of instruction descriptions,
 * tclInstructionTable, in tclCompile.c. Also, the order and number of
 * the expression opcodes (e.g., INST_LOR) must match the entries in
 * the array operatorStrings in tclExecute.c.
 */

/* Opcodes 0 to 7 */
#define INST_DONE			0
#define INST_PUSH			1
#define INST_POP			2
#define INST_DUP			3
#define INST_CONCAT			4
#define INST_INVOKE_STK		        5
#define INST_EVAL_STK			6
#define INST_EXPR_STK			7

/* Opcodes 8 to 17 */
#define INST_LOAD_SCALAR		8
#define INST_LOAD_SCALAR_STK		9
#define INST_LOAD_ARRAY		        10
#define INST_LOAD_ARRAY_STK		11
#define INST_LOAD_STK			12
#define INST_STORE_SCALAR		13
#define INST_STORE_SCALAR_STK		14
#define INST_STORE_ARRAY		15
#define INST_STORE_ARRAY_STK		16
#define INST_STORE_STK			17

/* Opcodes 18 to 27 */
#define INST_INCR_SCALAR		18
#define INST_INCR_SCALAR_STK		19
#define INST_INCR_ARRAY		        20
#define INST_INCR_ARRAY_STK		21
#define INST_INCR_STK			22
#define INST_INCR_SCALAR_IMM		23
#define INST_INCR_SCALAR_STK_IMM	24
#define INST_INCR_ARRAY_IMM		25
#define INST_INCR_ARRAY_STK_IMM		26
#define INST_INCR_STK_IMM		27

/* Opcodes 28 to 30 */
#define INST_JUMP			28
#define INST_JUMP_TRUE			29
#define INST_JUMP_FALSE		        30

/* Opcodes 31 to 53 */
#define FIRST_OPERATOR_INST             31
#define INST_BITOR			31
#define INST_BITXOR			32
#define INST_BITAND			33
#define INST_EQ				34
#define INST_NEQ			35
#define INST_LT				36
#define INST_GT				37
#define INST_LE				38
#define INST_GE				39
#define INST_LSHIFT			40
#define INST_RSHIFT			41
#define INST_ADD			42
#define INST_SUB			43
#define INST_MULT			44
#define INST_DIV			45
#define INST_MOD			46
#define INST_UPLUS			47
#define INST_UMINUS			48
#define INST_BITNOT			49
#define INST_LNOT			50
#define INST_CALL_BUILTIN_FUNC		51
#define INST_CALL_FUNC			52
#define INST_TRY_CVT_TO_NUMERIC		53

/* Opcodes 54 to 55 */
#define INST_BREAK			54
#define INST_CONTINUE			55

/* Opcodes 56 to 57 */
#define INST_FOREACH_START		56
#define INST_FOREACH_STEP		57

/* Opcodes 58 to 61 */
#define INST_BEGIN_CATCH		58
#define INST_END_CATCH			59
#define INST_PUSH_RESULT		60
#define INST_PUSH_RETURN_CODE		61

/* Opcodes 62 to 67 */
#define INST_STR_EQ			62
#define INST_STR_NEQ			63
#define INST_STR_CMP			64
#define INST_STR_LEN			65
#define INST_STR_INDEX			66
#define INST_STR_MATCH			67

/* Opcodes 68 to 70 */
#define INST_LIST			68
#define INST_LIST_INDEX			69
#define INST_LIST_LENGTH		70

/* Opcodes 71 to 74 */
#define INST_APPEND_SCALAR		71
#define INST_APPEND_ARRAY		72
#define INST_APPEND_ARRAY_STK		73
#define INST_APPEND_STK			74

/* Opcodes 75 to 78 */
#define INST_LAPPEND_SCALAR		75
#define INST_LAPPEND_ARRAY		76
#define INST_LAPPEND_ARRAY_STK		77
#define INST_LAPPEND_STK		78

/* TIP #22 - LINDEX operator with flat arg list */

#define INST_LIST_INDEX_MULTI		79

/*
 * TIP #33 - 'lset' command.  Code gen also required a Forth-like
 *	     OVER operation.
 */

#define INST_OVER			80
#define INST_LSET_LIST			81
#define INST_LSET_FLAT			82

/* TIP#90 - 'return' command. */

#define INST_RETURN			83

/* TIP#123 - exponentiation operator. */

#define INST_EXPON			84

/* TIP #157 - {expand}... language syntax support. */

#define INST_EXPAND_START               85
#define INST_EXPAND_STKTOP              86
#define INST_INVOKE_EXPANDED            87

/*
 * TIP #57 - 'lassign' command.  Code generation requires immediate
 *	     LINDEX and LRANGE operators.
 */

#define INST_LIST_INDEX_IMM		88
#define INST_LIST_RANGE_IMM		89

#define INST_START_CMD                  90

#define INST_LIST_IN			91
#define INST_LIST_NOT_IN		92

/* The last opcode */
#define LAST_INST_OPCODE		92

/*
 * Table describing the Tcl bytecode instructions: their name (for
 * displaying code), their stack effect, the number and type of operands. 
 * These operand types include signed and unsigned integers (the length is
 * determined by the quantity of operands: TclPSizedInt if it is one operand,
 * half that if there are two). The unsigned integers are used for indexes or
 * for, e.g., the count of objects to push in a "push" instruction.
 * Note that every instruction+operands is emitted taking 2*sizeof(void *)
 * bytes, even if it has no operands, in order to simplify the optimizer's
 * algorithm. The optimizer may later choose to eliminate those redundant
 * words (assuming the executor is prepared for that).
 */

#define MAX_INSTRUCTION_OPERANDS 2

typedef enum InstOperandType {
    OPERAND_NONE,
    OPERAND_INT,		/* Four byte signed integer. */
    OPERAND_UINT,		/* Four byte unsigned integer. */
    OPERAND_IDX		        /* Four byte signed index (actually an
				 * integer, but displayed differently.) */
} InstOperandType;

typedef struct InstructionDesc {
    char *name;			/* Name of instruction. */
    int stackEffect;		/* The worst-case balance stack effect of the 
				 * instruction, used for stack requirements 
				 * computations. The value INT_MIN signals
				 * that the instruction's worst case effect
				 * is (1-opnd1).
				 */
    int numOperands;		/* Number of operands. */
    InstOperandType opTypes[MAX_INSTRUCTION_OPERANDS];
				/* The type of each operand. */
} InstructionDesc;

MODULE_SCOPE InstructionDesc tclInstructionTable[];

/*
 * Definitions of the values of the INST_CALL_BUILTIN_FUNC instruction's
 * operand byte. Each value denotes a builtin Tcl math function. These
 * values must correspond to the entries in the tclBuiltinFuncTable array
 * below and to the values stored in the tclInt.h MathFunc structure's
 * builtinFuncIndex field.
 */

#define BUILTIN_FUNC_ACOS		0
#define BUILTIN_FUNC_ASIN		1
#define BUILTIN_FUNC_ATAN		2
#define BUILTIN_FUNC_ATAN2		3
#define BUILTIN_FUNC_CEIL		4
#define BUILTIN_FUNC_COS		5
#define BUILTIN_FUNC_COSH		6
#define BUILTIN_FUNC_EXP		7
#define BUILTIN_FUNC_FLOOR		8
#define BUILTIN_FUNC_FMOD		9
#define BUILTIN_FUNC_HYPOT		10
#define BUILTIN_FUNC_LOG		11
#define BUILTIN_FUNC_LOG10		12
#define BUILTIN_FUNC_POW		13
#define BUILTIN_FUNC_SIN		14
#define BUILTIN_FUNC_SINH		15
#define BUILTIN_FUNC_SQRT		16
#define BUILTIN_FUNC_TAN		17
#define BUILTIN_FUNC_TANH		18
#define BUILTIN_FUNC_ABS		19
#define BUILTIN_FUNC_DOUBLE		20
#define BUILTIN_FUNC_INT		21
#define BUILTIN_FUNC_RAND		22
#define BUILTIN_FUNC_ROUND		23
#define BUILTIN_FUNC_SRAND		24
#define BUILTIN_FUNC_WIDE		25

#define LAST_BUILTIN_FUNC		25

/*
 * Table describing the built-in math functions. Entries in this table are
 * indexed by the values of the INST_CALL_BUILTIN_FUNC instruction's
 * operand byte.
 */

typedef int (CallBuiltinFuncProc) _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_Obj **tosPtr, ClientData clientData));

typedef struct {
    char *name;			/* Name of function. */
    int numArgs;		/* Number of arguments for function. */
    Tcl_ValueType argTypes[MAX_MATH_ARGS];
				/* Acceptable types for each argument. */
    CallBuiltinFuncProc *proc;	/* Procedure implementing this function. */
    ClientData clientData;	/* Additional argument to pass to the
				 * function when invoking it. */
} BuiltinFunc;

MODULE_SCOPE BuiltinFunc tclBuiltinFuncTable[];

/*
 * Compilation of some Tcl constructs such as if commands and the logical or
 * (||) and logical and (&&) operators in expressions requires the
 * generation of forward jumps. Since the PC target of these jumps isn't
 * known when the jumps are emitted, we provide a pair of functions that
 * emit the jump, and then fixup the target. The function to emit the
 * forward jump needs to know what kind of jump it is.
 */

#define JUMPFIXUP_INIT_ENTRIES    10

typedef struct JumpFixupArray {
    int *fixup;		        /* Points to start of jump fixup array. */
    int next;			/* Index of next free array entry. */
    int end;			/* Index of last usable entry in array. */
    int mallocedArray;		/* 1 if array was expanded and fixups points
				 * into the heap, else 0. */
    int staticFixupSpace[JUMPFIXUP_INIT_ENTRIES];
				/* Initial storage for jump fixup array. */
} JumpFixupArray;

/*
 * The structure describing one variable list of a foreach command. Note
 * that only foreach commands inside procedure bodies are compiled inline so
 * a ForeachVarList structure always describes local variables. Furthermore,
 * only scalar variables are supported for inline-compiled foreach loops.
 */

typedef struct ForeachVarList {
    int numVars;		/* The number of variables in the list. */
    int varIndexes[1];		/* An array of the indexes ("slot numbers")
				 * for each variable in the procedure's
				 * array of local variables. Only scalar
				 * variables are supported. The actual
				 * size of this field will be large enough
				 * to numVars indexes. THIS MUST BE THE
				 * LAST FIELD IN THE STRUCTURE! */
} ForeachVarList;

/*
 * Structure used to hold information about a foreach command that is needed
 * during program execution. These structures are stored in CompileEnv and
 * ByteCode structures as auxiliary data.
 */

typedef struct ForeachInfo {
    int numLists;		/* The number of both the variable and value
				 * lists of the foreach command. */
    int firstValueTemp;		/* Index of the first temp var in a proc
				 * frame used to point to a value list. */
    int loopCtTemp;		/* Index of temp var in a proc frame
				 * holding the loop's iteration count. Used
				 * to determine next value list element to
				 * assign each loop var. */
    int restartOffset;          /* Offset of the loop body, immediately after
				 * the INST_FOREACH_START instruction. */
    ForeachVarList *varLists[1];/* An array of pointers to ForeachVarList
				 * structures describing each var list. The
				 * actual size of this field will be large
				 * enough to numVars indexes. THIS MUST BE
				 * THE LAST FIELD IN THE STRUCTURE! */
} ForeachInfo;

MODULE_SCOPE AuxDataType		tclForeachInfoType;


/*
 *----------------------------------------------------------------
 * Procedures exported by tclBasic.c to be used within the engine.
 *----------------------------------------------------------------
 */

MODULE_SCOPE int	TclEvalObjvInternal _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[],
			    CONST char *command, int length, int flags));
MODULE_SCOPE int	TclInterpReady _ANSI_ARGS_((Tcl_Interp *interp));


/*
 *----------------------------------------------------------------
 * Procedures exported by the engine to be used by tclBasic.c
 *----------------------------------------------------------------
 */

/*
 * Declaration moved to the internal stubs table
 *
MODULE_SCOPE int	TclCompEvalObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
*/

/*
 *----------------------------------------------------------------
 * Procedures shared among Tcl bytecode compilation and execution
 * modules but not used outside:
 *----------------------------------------------------------------
 */

EXTERN int		TclBeginExceptRange _ANSI_ARGS_((CompileEnv *envPtr));
MODULE_SCOPE void	TclCleanupByteCode _ANSI_ARGS_((ByteCode *codePtr));
MODULE_SCOPE void	TclCompileCmdWord _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, int count,
			    CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileExpr _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *script, int numBytes,
			    CompileEnv *envPtr));
MODULE_SCOPE void	TclCompileExprWords _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, int numWords,
			    CompileEnv *envPtr));
MODULE_SCOPE void	TclCompileScript _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *script, int numBytes,
			    CompileEnv *envPtr));
MODULE_SCOPE void	TclCompileTokens _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, int count,
			    CompileEnv *envPtr));
MODULE_SCOPE int	TclCreateAuxData _ANSI_ARGS_((ClientData clientData,
			    AuxDataType *typePtr, CompileEnv *envPtr));
MODULE_SCOPE ExecEnv *	TclCreateExecEnv _ANSI_ARGS_((Tcl_Interp *interp));
MODULE_SCOPE void	TclDeleteExecEnv _ANSI_ARGS_((ExecEnv *eePtr));
MODULE_SCOPE void	TclDeleteLiteralTable _ANSI_ARGS_((
			    Tcl_Interp *interp, LiteralTable *tablePtr));
MODULE_SCOPE ExceptionRange * TclGetExceptionRangeForPc _ANSI_ARGS_((
			    unsigned char *pc, int catchOnly,
			    ByteCode* codePtr));
EXTERN void		TclEndExceptRange _ANSI_ARGS_((
			    int index, CompileEnv *envPtr));
EXTERN void		TclExpandJumpFixupArray _ANSI_ARGS_((
                            JumpFixupArray *fixupArrayPtr));
MODULE_SCOPE void	TclFinalizeAuxDataTypeTable _ANSI_ARGS_((void));
MODULE_SCOPE int	TclFindCompiledLocal _ANSI_ARGS_((CONST char *name, 
			    int nameChars, int create, int flags,
			    Proc *procPtr));
MODULE_SCOPE LiteralEntry * TclLookupLiteralEntry _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *objPtr));
MODULE_SCOPE void	TclFreeCompileEnv _ANSI_ARGS_((CompileEnv *envPtr));
EXTERN void		TclFreeJumpFixupArray _ANSI_ARGS_((
  			    JumpFixupArray *fixupArrayPtr));
MODULE_SCOPE void	TclInitAuxDataTypeTable _ANSI_ARGS_((void));
MODULE_SCOPE void	TclInitByteCodeObj _ANSI_ARGS_((Tcl_Obj *objPtr,
			    CompileEnv *envPtr));
MODULE_SCOPE void	TclInitCompilation _ANSI_ARGS_((void));
MODULE_SCOPE void	TclInitCompileEnv _ANSI_ARGS_((Tcl_Interp *interp,
			    CompileEnv *envPtr, char *string,
			    int numBytes));
EXTERN void		TclInitJumpFixupArray _ANSI_ARGS_((
			    JumpFixupArray *fixupArrayPtr));
MODULE_SCOPE void	TclInitLiteralTable _ANSI_ARGS_((
			    LiteralTable *tablePtr));
#ifdef TCL_COMPILE_STATS
MODULE_SCOPE char *	TclLiteralStats _ANSI_ARGS_((
			    LiteralTable *tablePtr));
MODULE_SCOPE int	TclLog2 _ANSI_ARGS_((int value));
#endif
#ifdef TCL_COMPILE_DEBUG
MODULE_SCOPE void	TclPrintByteCodeObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
#endif
MODULE_SCOPE int	TclPrintInstruction _ANSI_ARGS_((ByteCode* codePtr,
			    TclVMWord *pc));
MODULE_SCOPE void	TclPrintObject _ANSI_ARGS_((FILE *outFile,
			    Tcl_Obj *objPtr, int maxChars));
MODULE_SCOPE void	TclPrintSource _ANSI_ARGS_((FILE *outFile,
			    CONST char *string, int maxChars));
MODULE_SCOPE void	TclRegisterAuxDataType _ANSI_ARGS_((AuxDataType *typePtr));
MODULE_SCOPE int	TclRegisterLiteral _ANSI_ARGS_((CompileEnv *envPtr,
			    char *bytes, int length, int flags));
MODULE_SCOPE void	TclReleaseLiteral _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
MODULE_SCOPE void	TclSetCmdNameObj _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr, Command *cmdPtr));
#ifdef TCL_COMPILE_DEBUG
MODULE_SCOPE void	TclVerifyGlobalLiteralTable _ANSI_ARGS_((
			    Interp *iPtr));
MODULE_SCOPE void	TclVerifyLocalLiteralTable _ANSI_ARGS_((
			    CompileEnv *envPtr));
#endif
MODULE_SCOPE int	TclCompileVariableCmd _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Parse *parsePtr, CompileEnv *envPtr));
MODULE_SCOPE int	TclWordKnownAtCompileTime _ANSI_ARGS_((
			    Tcl_Token *tokenPtr, Tcl_Obj *valuePtr));

/*
 *----------------------------------------------------------------
 * Macros and flag values used by Tcl bytecode compilation and execution
 * modules inside the Tcl core but not used outside.
 *----------------------------------------------------------------
 */

#define LITERAL_ON_HEAP    0x01
#define LITERAL_NS_SCOPE   0x02
/*
 * Form of TclRegisterLiteral with onHeap == 0.
 * In that case, it is safe to cast away CONSTness, and it
 * is cleanest to do that here, all in one place.
 */

#define TclRegisterNewLiteral(envPtr, bytes, length) \
	TclRegisterLiteral(envPtr, (char *)(bytes), length, \
                /*flags*/ 0)

/*
 * Form of TclRegisterNSLiteral with onHeap == 0.
 * In that case, it is safe to cast away CONSTness, and it
 * is cleanest to do that here, all in one place.
 */

#define TclRegisterNewNSLiteral(envPtr, bytes, length) \
	TclRegisterLiteral(envPtr, (char *)(bytes), length, \
                /*flags*/ LITERAL_NS_SCOPE)


/*
 * Macro used to manually adjust the stack requirements; used
 * in cases where the stack effect cannot be computed from
 * the opcode and its operands, but is still known at
 * compile time.
 */

#define TclAdjustStackDepth(delta, envPtr) \
    if ((delta) < 0) {\
	if((envPtr)->maxStackDepth < (envPtr)->currStackDepth) {\
	    (envPtr)->maxStackDepth = (envPtr)->currStackDepth;\
	}\
    }\
    (envPtr)->currStackDepth += (delta)

/*
 * Macro used to update the stack requirements.
 * It is called by the macros TclEmitOpCode, TclEmitInst1 and
 * TclEmitInst.
 * Remark that the very last instruction of a bytecode always
 * reduces the stack level: INST_DONE or INST_POP, so that the 
 * maxStackdepth is always updated.
 */

#define TclUpdateStackReqs(op, i, envPtr) \
    {\
	int delta = tclInstructionTable[(op)].stackEffect;\
	if (delta) {\
	    if (delta == INT_MIN) {\
		delta = 1 - (i);\
	    }\
            TclAdjustStackDepth(delta, envPtr);\
        }\
    }

	
/*
 * Macros to emit an instruction with integer operands.
 */

#define TclEmitInst1(op, n, envPtr) \
    if (((envPtr)->codeNext + 1) > (envPtr)->codeEnd) { \
	TclExpandCodeArray(envPtr); \
    } \
    TclVMStoreWordAtPtr((op), (n), (envPtr)->codeNext);\
    (envPtr)->codeNext++;\
    TclUpdateStackReqs((op), (n), envPtr)

#define TclEmitInst2(op, n, u, envPtr)\
    {\
        TclPSizedInt z;\
        HP_STASH(z, (n), (u));\
        TclEmitInst1((op), z, envPtr);\
    }

#define  TclEmitInst0(op, envPtr) \
    TclEmitInst1((op), 0, envPtr) 

/*
 * Macro to push a Tcl object onto the Tcl evaluation stack. It emits the
 * object's array index into the CompileEnv's code array. 
 */

#define TclEmitPush(objIndex, envPtr) \
    TclEmitInst1(INST_PUSH, (objIndex), (envPtr))

/*
 * Compilation of some Tcl constructs such as if commands and the logical or
 * (||) and logical and (&&) operators in expressions requires the
 * generation of forward jumps. Since the PC target of these jumps isn't
 * known when the jumps are emitted, we record the offset of each jump with
 * the macro TclEmitForwardJump. When we learn the target PC, we update the
 * jumps with the correct distance using the macro TclSetJumpTarget.
 */

#define TclEmitForwardJump(envPtr, inst, fixOffset) \
   (fixOffset) = (envPtr->codeNext - envPtr->codeStart);\
   TclEmitInst0((inst), (envPtr))

#define TclSetJumpTarget(envPtr, fixOffset) \
{\
    ptrdiff_t jumpDist =\
        (envPtr->codeNext - envPtr->codeStart) - (fixOffset);\
    TclVMWord *fixPc = envPtr->codeStart + fixOffset;\
    TclVMStoreOpndAtPtr(jumpDist, fixPc);\
}


/*
 * Macros to update a (signed or unsigned) integer starting at a pointer.
 * The two variants depend on the number of bytes. The ANSI C "prototypes"
 * for these macros are:
 *
 * MODULE_SCOPE void	TclStoreInt1AtPtr _ANSI_ARGS_((int i, unsigned char *p));
 * MODULE_SCOPE void	TclStoreInt4AtPtr _ANSI_ARGS_((int i, unsigned char *p));
 */

#define TclStoreInt1AtPtr(i, p) \
    *(p)   = (unsigned char) ((unsigned int) (i))

#define TclStoreInt4AtPtr(i, p) \
    *(p)   = (unsigned char) ((unsigned int) (i) >> 24); \
    *(p+1) = (unsigned char) ((unsigned int) (i) >> 16); \
    *(p+2) = (unsigned char) ((unsigned int) (i) >>  8); \
    *(p+3) = (unsigned char) ((unsigned int) (i)      )

    
/*
 * Macros to get a signed integer (GET_INT{1,2}) or an unsigned int
 * (GET_UINT{1,2}) from a pointer. There are two variants for each
 * return type that depend on the number of bytes fetched.
 * The ANSI C "prototypes" for these macros are:
 *
 * MODULE_SCOPE int		TclGetInt1AtPtr  _ANSI_ARGS_((unsigned char *p));
 * MODULE_SCOPE int		TclGetInt4AtPtr  _ANSI_ARGS_((unsigned char *p));
 * MODULE_SCOPE unsigned int	TclGetUInt1AtPtr _ANSI_ARGS_((unsigned char *p));
 * MODULE_SCOPE unsigned int	TclGetUInt4AtPtr _ANSI_ARGS_((unsigned char *p));
 */

/*
 * The TclGetInt1AtPtr macro is tricky because we want to do sign
 * extension on the 1-byte value. Unfortunately the "char" type isn't
 * signed on all platforms so sign-extension doesn't always happen
 * automatically. Sometimes we can explicitly declare the pointer to be
 * signed, but other times we have to explicitly sign-extend the value
 * in software.
 */

#ifndef __CHAR_UNSIGNED__
#   define TclGetInt1AtPtr(p) ((int) *((char *) p))
#else
#   ifdef HAVE_SIGNED_CHAR
#	define TclGetInt1AtPtr(p) ((int) *((signed char *) p))
#    else
#	define TclGetInt1AtPtr(p) (((int) *((char *) p)) \
		| ((*(p) & 0200) ? (-256) : 0))
#    endif
#endif

#define TclGetUInt1AtPtr(p) ((unsigned int) *(p))

#define TclGetUInt1AtPtr(p) ((unsigned int) *(p))

#define TclGetInt4AtPtr(p) (((int) TclGetInt1AtPtr(p) << 24) | \
					    (*((p)+1) << 16) | \
				  	    (*((p)+2) <<  8) | \
				  	    (*((p)+3)))

#define TclGetUInt4AtPtr(p) ((unsigned int) (*(p)     << 24) | \
					    (*((p)+1) << 16) | \
					    (*((p)+2) <<  8) | \
					    (*((p)+3)))
    
/*
 * Macros used to compute the minimum and maximum of two integers.
 * The ANSI C "prototypes" for these macros are:
 *
 * MODULE_SCOPE int  TclMin _ANSI_ARGS_((int i, int j));
 * MODULE_SCOPE int  TclMax _ANSI_ARGS_((int i, int j));
 */

#define TclMin(i, j)   ((((int) i) < ((int) j))? (i) : (j))
#define TclMax(i, j)   ((((int) i) > ((int) j))? (i) : (j))


#endif /* _TCLCOMPILATION */
