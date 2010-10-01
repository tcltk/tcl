#ifndef _TCL_ASSEMBLY
#define _TCL_ASSEMBLY 1

#include "tclCompile.h"

/* 
 * Structure that defines a basic block - a linear sequence of bytecode
 * instructions with no jumps in or out.
 */

typedef struct BasicBlock {

    int startOffset;		/* Instruction offset of the start of 
				 * the block */
    int startLine;		/* Line number in the input script of the
				 * instruction at the  start of the block */
    int jumpLine;	        /* Line number in the input script of the
				 * 'jump' instruction that ends the block,
				 * or -1 if there is no jump */
    int may_fall_thru;      	/* Flag == 1 if control passes from this
				 * block to its successor. */
    int visited;		/* Flag==1 if this block has been visited
				 * in the stack checker*/
    struct BasicBlock* predecessor;	
				/* Predecessor of this block in the
				 * spanning tree */
    struct BasicBlock * successor1;
				/* BasicBlock structure of the following 
				 * block:  NULL at the end of the bytecode 
				 * sequence or if the block ends in an
				 * unconditional jump */ 
    Tcl_Obj * jumpTarget;	/* Jump target label if the jump target 
				 * is unresolved */
    
    int initialStackDepth;	/* Absolute stack depth on entry */
    int minStackDepth;		/* Low-water relative stack depth */
    int maxStackDepth; 		/* High-water relative stack depth */
    int finalStackDepth;	/* Relative stack depth on exit */

} BasicBlock;

/* Source instruction type recognized by the assembler */

typedef enum TalInstType {

    ASSEM_1BYTE,    /* Fixed arity, 1-byte instruction */
    ASSEM_BOOL,	    /* One Boolean operand */
    ASSEM_BOOL_LVT4,/* One Boolean, one 4-byte LVT ref. */
    ASSEM_CONCAT1,  /* 1-byte unsigned-integer operand count, must be 
		     * strictly positive, consumes N, produces 1 */
    ASSEM_DICT_GET, /* 'dict get' and related - consumes N+1 operands,
		     * produces 1, N >= 0 */
    ASSEM_EVAL,	    /* 'eval' - evaluate a constant script (by compiling it
		     * in line with the assembly code! I love Tcl!) */
    ASSEM_INDEX,    /* 4 byte operand, integer or end-integer */
    ASSEM_INVOKE,   /* 1- or 4-byte operand count, must be strictly positive,
		     * consumes N, produces 1. */
    ASSEM_JUMP,	    /* Jump instructions */
    ASSEM_LABEL,    /* The assembly directive that defines a label */
    ASSEM_LINDEX_MULTI,
		    /* 4-byte operand count, must be strictly positive,
		     * consumes N, produces 1 */
    ASSEM_LIST,     /* 4-byte operand count, must be nonnegative, consumses N,
		     * produces 1 */
    ASSEM_LSET_FLAT,/* 4-byte operand count, must be >= 3, consumes N,
		     * produces 1 */
    ASSEM_LVT,      /* One operand that references a local variable */
    ASSEM_LVT1,     /* One 1-byte operand that references a local variable */
    ASSEM_LVT1_SINT1,
		    /* One 1-byte operand that references a local variable,
		     * one signed-integer 1-byte operand */
    ASSEM_LVT4,     /* One 4-byte operand that references a local variable */
    ASSEM_OVER,	    /* OVER: 4-byte operand count, consumes N+1, produces N+2 */
    ASSEM_PUSH,     /* one literal operand */
    ASSEM_REVERSE,  /* REVERSE: 4-byte operand count, consumes N, produces N */
    ASSEM_SINT1,    /* One 1-byte signed-integer operand (INCR_STK_IMM) */
} TalInstType;

/* Description of an instruction recognized by the assembler. */

typedef struct TalInstDesc {
    const char *name;		/* Name of instruction. */
    TalInstType instType; 	/* The type of instruction */
    int tclInstCode;		/* Instruction code. For instructions having
				 * 1- and 4-byte variables, tclInstCode is
				 * ((1byte)<<8) || (4byte) */
    int operandsConsumed;	/* Number of operands consumed by the
				 * operation, or INT_MIN if the operation
				 * is variadic */
    int operandsProduced;	/* Number of operands produced by the
				 * operation. If negative, the operation
				 * has a net stack effect of 
				 * -1-operandsProduced */
} TalInstDesc;

/* Description of a label in the assembly code */

typedef struct JumpLabel {
    int isDefined;		/* Flag == 1 if label is defined */
    int offset;			/* Offset in the code where the label starts,
				 * or head of a linked list of jump target
				 * addresses if the label is undefined */
    BasicBlock* basicBlock;	/* Basic block that begins at the label */
} JumpLabel;

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

    BasicBlock* head_bb;	/* First basic block in the code */
    BasicBlock* curr_bb;	/* Current basic block */

    int maxDepth;	     	/* Maximum stack depth encountered */
    int flags;			/* Compilation flags (TCL_EVAL_DIRECT) */
} AssembleEnv;

MODULE_SCOPE int TclAssembleCode(CompileEnv* compEnv, const char* codePtr,
				 int codeLen, int flags);

#endif 
