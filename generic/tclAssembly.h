#ifndef _TCL_ASSEMBLY
#define _TCL_ASSEMBLY 1

#include "tclCompile.h"

typedef struct StackCheckerState {
    Tcl_Interp* interp;
    CompileEnv* envPtr;
    int maxDepth;
    Tcl_Obj* bcList;
} StackCheckerState;

typedef struct BasicBlock {
    /* FIXME: start needs to be an offset from envPtr->codeStart */
    unsigned char * start;	/* Instruction address of the start
				 * of the block */
    int bcIndex;		/* Index in the input instruction
				 * list of the start of the block */
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
    Tcl_HashEntry * jumpTargetLabelHashEntry;
				/* Jump target label if the jump target 
				 * is unresolved */
    
    int initialStackDepth;	/* Absolute stack depth on entry */
    int minStackDepth;		/* Low-water relative stack depth */
    int maxStackDepth; 		/* High-water relative stack depth */
    int finalStackDepth;	/* Relative stack depth on exit */

} BasicBlock;

typedef enum talInstType {

    ASSEM_1BYTE,    /* The instructions that are directly mapped to tclInstructionTable in tclCompile.c*/
    ASSEM_BOOL,	    /* One Boolean operand */
    ASSEM_BOOL_LVT4,/* One Boolean, one 4-byte LVT ref. */
    ASSEM_CONCAT1,  /* One 1-byte unsigned-integer operand count (CONCAT1) */
    ASSEM_EVAL,	    /* 'eval' - evaluate a constant script (by compiling it
		     * in line with the assembly code! I love Tcl!) */
    ASSEM_INVOKE,   /* Command invocation, 1- or 4-byte unsigned operand
		     * count */
    ASSEM_JUMP,	    /* Jump instructions */
    ASSEM_LABEL,    /* The assembly directive that defines a label */
    ASSEM_LVT,      /* One operand that references a local variable */
    ASSEM_LVT1,     /* One 1-byte operand that references a local variable */
    ASSEM_LVT1_SINT1,
		    /* One 1-byte operand that references a local variable,
		     * one signed-integer 1-byte operand */
    ASSEM_LVT4,     /* One 4-byte operand that references a local variable */
    ASSEM_OVER,	    /* OVER: consumes n+1 operands and produces n+2 */
    ASSEM_PUSH,     /* These instructions will be looked up from talInstructionTable */
    ASSEM_REVERSE,  /* REVERSE: consumes n operands and produces n */
    ASSEM_SINT1,    /* One 1-byte signed-integer operand (INCR_STK_IMM) */
} talInstType;

typedef struct talInstDesc {
    const char *name;		/* Name of instruction. */
    talInstType instType; 	/* The type of instruction */
    int tclInstCode;
    int operandsConsumed;
    int operandsProduced;

} talInstDesc;

typedef struct label {
    int isDefined;
    int offset;    
} label;

MODULE_SCOPE int TclAssembleCode(Tcl_Interp* interp, Tcl_Obj* code,
				 CompileEnv* compEnv, int flags);

#endif 
