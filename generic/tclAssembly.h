#ifndef _TCL_ASSEMBLY
#define _TCL_ASSEMBLY 1

#include "tclCompile.h"

/* State identified for a basic block's catch context */

typedef enum BasicBlockCatchState {
    BBCS_UNKNOWN = 0,		/* Catch context has not yet been identified */
    BBCS_NONE,			/* Block is outside of any catch */
    BBCS_INCATCH,		/* Block is within a catch context */
    BBCS_DONECATCH,		/* Block is nominally within a catch context
				 * but has passed a 'doneCatch' directive
				 * and wants exceptions to propagate. */
    BBCS_CAUGHT,		/* Block is within a catch context and
				 * may be executed after an exception fires */
} BasicBlockCatchState;

typedef struct CodeRange {
    int startOffset;		/* Start offset in the bytecode array */
    int endOffset;		/* End offset in the bytecode array */
} CodeRange;

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

    enum BasicBlockCatchState catchState;
				/* State of the block for 'catch' analysis */
    int catchDepth;		/* Number of nested catches in which the
				 * basic block appears */
    struct BasicBlock* enclosingCatch;
				/* BasicBlock structure of the last
				 * startCatch executed on a path to this 
				 * block, or NULL if there is no
				 * enclosing catch */

    int foreignExceptionBase;	/* Base index of foreign exceptions */
    int foreignExceptionCount;	/* Count of foreign exceptions */
    ExceptionRange* foreignExceptions;
				/* ExceptionRange structures for
				 * exception ranges belonging to embedded
				 * scripts and expressions in this block */

    int flags;			/* Boolean flags */

} BasicBlock;

/* Flags that pertain to a basic block */

enum BasicBlockFlags {
    BB_VISITED = (1 << 0),	/* Block has been visited in the current
				 * traversal */
    BB_FALLTHRU = (1 << 1),	/* Control may pass from this block to
				 * a successor */
    BB_BEGINCATCH = (1 << 2),	/* Block ends with a 'beginCatch' instruction,
				 * marking it as the start of a 'catch' 
				 * sequence. The 'jumpTarget' is the exception
				 * exit from the catch block. */
    BB_DONECATCH = (1 << 3),	/* Block commences with a 'doneCatch'
				 * directive, indicating that the program
				 * is finished with the body of a catch block.
				 */
    BB_ENDCATCH = (1 << 4),	/* Block ends with an 'endCatch' instruction,
				 * unwinding the catch from the exception 
				 * stack. */
};

/* Source instruction type recognized by the assembler */

typedef enum TalInstType {

    ASSEM_1BYTE,    /* Fixed arity, 1-byte instruction */
    ASSEM_BEGIN_CATCH,
		    /* Begin catch: one 4-byte jump offset to be converted
		     * to appropriate exception ranges */
    ASSEM_BOOL,	    /* One Boolean operand */
    ASSEM_BOOL_LVT4,/* One Boolean, one 4-byte LVT ref. */
    ASSEM_CONCAT1,  /* 1-byte unsigned-integer operand count, must be 
		     * strictly positive, consumes N, produces 1 */
    ASSEM_DICT_GET, /* 'dict get' and related - consumes N+1 operands,
		     * produces 1, N > 0 */
    ASSEM_DICT_SET, /* specifies key count and LVT index, consumes N+1 operands,
		     * produces 1, N > 0 */
    ASSEM_DICT_UNSET,
		    /* specifies key count and LVT index, consumes N operands,
		     * produces 1, N > 0 */
    ASSEM_DONECATCH,/* Directive indicating that the body of a catch block
		     * is complete. Generates no instructions, affects only
		     * the exception ranges. */
    ASSEM_END_CATCH,/* End catch. No args. Exception range popped from stack
		     * and stack pointer restored. */
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
    ASSEM_REGEXP,   /* One Boolean operand, but weird mapping to call flags */
    ASSEM_REVERSE,  /* REVERSE: 4-byte operand count, consumes N, produces N */
    ASSEM_SINT1,    /* One 1-byte signed-integer operand (INCR_STK_IMM) */
    ASSEM_SINT4_LVT4,
                    /* Signed 4-byte integer operand followed by LVT entry. 
		     * Fixed arity */
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

    int curCatchDepth;		/* Current depth of catches */
    int maxCatchDepth;		/* Maximum depth of catches encountered */

    int flags;			/* Compilation flags (TCL_EVAL_DIRECT) */
} AssembleEnv;

MODULE_SCOPE int TclAssembleCode(CompileEnv* compEnv, const char* codePtr,
				 int codeLen, int flags);

#endif 
