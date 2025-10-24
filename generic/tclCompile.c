/*
 * tclCompile.c --
 *
 *	This file contains procedures that compile Tcl commands or parts of
 *	commands (like quoted strings or nested sub-commands) into a sequence
 *	of instructions ("bytecodes").
 *
 * Copyright © 1996-1998 Sun Microsystems, Inc.
 * Copyright © 2001 Kevin B. Kenny. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#define ALLOW_DEPRECATED_OPCODES
#include "tclCompile.h"

/*
 * Variable that controls whether compilation tracing is enabled and, if so,
 * what level of tracing is desired:
 *    0: no compilation tracing
 *    1: summarize compilation of top level cmds and proc bodies
 *    2: display all instructions of each ByteCode compiled
 * This variable is linked to the Tcl variable "tcl_traceCompile".
 */

#ifdef TCL_COMPILE_DEBUG
int tclTraceCompile = TCL_TRACE_BYTECODE_COMPILE_NONE;
static int traceInitialized = 0;
#endif

/*
 * Minor helpers for the table below. The compiler doesn't enforce
 * the deprecation here; that's not possible.
 */

#define TCL_INSTRUCTION_ENTRY(name,stack) \
    {name,1,stack,0,{OPERAND_NONE,OPERAND_NONE}}
#define TCL_INSTRUCTION_ENTRY1(name,size,stack,type1) \
    {name,size,stack,1,{type1,OPERAND_NONE}}
#define TCL_INSTRUCTION_ENTRY2(name,size,stack,type1,type2) \
    {name,size,stack,2,{type1,type2}}

/* TODO: Mark these differently when REMOVE_DEPRECATED_OPCODES is defined. */
#define DEPRECATED_INSTRUCTION_ENTRY(name,stack) \
    {name,1,stack,0,{OPERAND_NONE,OPERAND_NONE}}
#define DEPRECATED_INSTRUCTION_ENTRY1(name,size,stack,type1) \
    {name,size,stack,1,{type1,OPERAND_NONE}}
#define DEPRECATED_INSTRUCTION_ENTRY2(name,size,stack,type1,type2) \
    {name,size,stack,2,{type1,type2}}

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
    /*  Name	      Bytes stackEffect	  Operand types */
    TCL_INSTRUCTION_ENTRY(
	"done",			-1),
	/* Finish ByteCode execution and return stktop (top stack item) */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"push1",	  2,	+1,	  OPERAND_LIT1),
	/* Push object at ByteCode objArray[op1] */
    TCL_INSTRUCTION_ENTRY1(
	"push",		  5,	+1,	  OPERAND_LIT4),
	/* Push object at ByteCode objArray[op4] */
    TCL_INSTRUCTION_ENTRY(
	"pop",			-1),
	/* Pop the topmost stack object */
    TCL_INSTRUCTION_ENTRY(
	"dup",			+1),
	/* Duplicate the topmost stack object and push the result */
    TCL_INSTRUCTION_ENTRY1(
	"strcat",	  2,	INT_MIN,  OPERAND_UINT1),
	/* Concatenate the top op1 items and push result */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"invokeStk1",	  2,	INT_MIN,  OPERAND_UINT1),
	/* Invoke command named objv[0]; <objc,objv> = <op1,top op1> */
    TCL_INSTRUCTION_ENTRY1(
	"invokeStk",	  5,	INT_MIN,  OPERAND_UINT4),
	/* Invoke command named objv[0]; <objc,objv> = <op4,top op4> */
    TCL_INSTRUCTION_ENTRY(
	"evalStk",		0),
	/* Evaluate command in stktop using Tcl_EvalObj. */
    TCL_INSTRUCTION_ENTRY(
	"exprStk",		0),
	/* Execute expression in stktop using Tcl_ExprStringObj. */

    DEPRECATED_INSTRUCTION_ENTRY1(
	"loadScalar1",	  2,	1,	  OPERAND_LVT1),
	/* Load scalar variable at index op1 <= 255 in call frame */
    TCL_INSTRUCTION_ENTRY1(
	"loadScalar",	  5,	1,	  OPERAND_LVT4),
	/* Load scalar variable at index op1 >= 256 in call frame */
    TCL_INSTRUCTION_ENTRY(
	"loadScalarStk",	0),
	/* Load scalar variable; scalar's name is stktop */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"loadArray1",	  2,	0,	  OPERAND_LVT1),
	/* Load array element; array at slot op1<=255, element is stktop */
    TCL_INSTRUCTION_ENTRY1(
	"loadArray",	  5,	0,	  OPERAND_LVT4),
	/* Load array element; array at slot op1 > 255, element is stktop */
    TCL_INSTRUCTION_ENTRY(
	"loadArrayStk",		-1),
	/* Load array element; element is stktop, array name is stknext */
    TCL_INSTRUCTION_ENTRY(
	"loadStk",		0),
	/* Load general variable; unparsed variable name is stktop */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"storeScalar1",	  2,	0,	  OPERAND_LVT1),
	/* Store scalar variable at op1<=255 in frame; value is stktop */
    TCL_INSTRUCTION_ENTRY1(
	"storeScalar",	  5,	0,	  OPERAND_LVT4),
	/* Store scalar variable at op1 > 255 in frame; value is stktop */
    TCL_INSTRUCTION_ENTRY(
	"storeScalarStk",	-1),
	/* Store scalar; value is stktop, scalar name is stknext */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"storeArray1",	  2,	-1,	  OPERAND_LVT1),
	/* Store array element; array at op1<=255, value is top then elem */
    TCL_INSTRUCTION_ENTRY1(
	"storeArray",	  5,	-1,	  OPERAND_LVT4),
	/* Store array element; array at op1>=256, value is top then elem */
    TCL_INSTRUCTION_ENTRY(
	"storeArrayStk",	-2),
	/* Store array element; value is stktop, then elem, array names */
    TCL_INSTRUCTION_ENTRY(
	"storeStk",		-1),
	/* Store general variable; value is stktop, then unparsed name */

    DEPRECATED_INSTRUCTION_ENTRY1(
	"incrScalar1",	  2,	0,	  OPERAND_LVT1),
	/* Incr scalar at index op1<=255 in frame; incr amount is stktop */
    TCL_INSTRUCTION_ENTRY(
	"incrScalarStk",	-1),
	/* Incr scalar; incr amount is stktop, scalar's name is stknext */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"incrArray1",	  2,	-1,	  OPERAND_LVT1),
	/* Incr array elem; arr at slot op1<=255, amount is top then elem */
    TCL_INSTRUCTION_ENTRY(
	"incrArrayStk",		-2),
	/* Incr array element; amount is top then elem then array names */
    TCL_INSTRUCTION_ENTRY(
	"incrStk",		-1),
	/* Incr general variable; amount is stktop then unparsed var name */
    DEPRECATED_INSTRUCTION_ENTRY2(
	"incrScalar1Imm", 3,	+1,	  OPERAND_LVT1, OPERAND_INT1),
	/* Incr scalar at slot op1 <= 255; amount is 2nd operand byte */
    TCL_INSTRUCTION_ENTRY1(
	"incrScalarStkImm",2,	0,	  OPERAND_INT1),
	/* Incr scalar; scalar name is stktop; incr amount is op1 */
    DEPRECATED_INSTRUCTION_ENTRY2(
	"incrArray1Imm",  3,	0,	  OPERAND_LVT1, OPERAND_INT1),
	/* Incr array elem; array at slot op1 <= 255, elem is stktop,
	 * amount is 2nd operand byte */
    TCL_INSTRUCTION_ENTRY1(
	"incrArrayStkImm",2,	-1,	  OPERAND_INT1),
	/* Incr array element; elem is top then array name, amount is op1 */
    TCL_INSTRUCTION_ENTRY1(
	"incrStkImm",	  2,	0,	  OPERAND_INT1),
	/* Incr general variable; unparsed name is top, amount is op1 */

    DEPRECATED_INSTRUCTION_ENTRY1(
	"jump1",	  2,	0,	  OPERAND_OFFSET1),
	/* Jump relative to (pc + op1) */
    TCL_INSTRUCTION_ENTRY1(
	"jump",		  5,	0,	  OPERAND_OFFSET4),
	/* Jump relative to (pc + op4) */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"jumpTrue1",	  2,	-1,	  OPERAND_OFFSET1),
	/* Jump relative to (pc + op1) if stktop expr object is true */
    TCL_INSTRUCTION_ENTRY1(
	"jumpTrue",	  5,	-1,	  OPERAND_OFFSET4),
	/* Jump relative to (pc + op4) if stktop expr object is true */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"jumpFalse1",	  2,	-1,	  OPERAND_OFFSET1),
	/* Jump relative to (pc + op1) if stktop expr object is false */
    TCL_INSTRUCTION_ENTRY1(
	"jumpFalse",	  5,	-1,	  OPERAND_OFFSET4),
	/* Jump relative to (pc + op4) if stktop expr object is false */

    TCL_INSTRUCTION_ENTRY(
	"bitor",		-1),
	/* Bitwise or:	push (stknext | stktop) */
    TCL_INSTRUCTION_ENTRY(
	"bitxor",		-1),
	/* Bitwise xor	push (stknext ^ stktop) */
    TCL_INSTRUCTION_ENTRY(
	"bitand",		-1),
	/* Bitwise and:	push (stknext & stktop) */
    TCL_INSTRUCTION_ENTRY(
	"eq",			-1),
	/* Equal:	push (stknext == stktop) */
    TCL_INSTRUCTION_ENTRY(
	"neq",			-1),
	/* Not equal:	push (stknext != stktop) */
    TCL_INSTRUCTION_ENTRY(
	"lt",			-1),
	/* Less:	push (stknext < stktop) */
    TCL_INSTRUCTION_ENTRY(
	"gt",			-1),
	/* Greater:	push (stknext > stktop) */
    TCL_INSTRUCTION_ENTRY(
	"le",			-1),
	/* Less or equal: push (stknext <= stktop) */
    TCL_INSTRUCTION_ENTRY(
	"ge",			-1),
	/* Greater or equal: push (stknext >= stktop) */
    TCL_INSTRUCTION_ENTRY(
	"lshift",		-1),
	/* Left shift:	push (stknext << stktop) */
    TCL_INSTRUCTION_ENTRY(
	"rshift",		-1),
	/* Right shift:	push (stknext >> stktop) */
    TCL_INSTRUCTION_ENTRY(
	"add",			-1),
	/* Add:		push (stknext + stktop) */
    TCL_INSTRUCTION_ENTRY(
	"sub",			-1),
	/* Sub:		push (stkext - stktop) */
    TCL_INSTRUCTION_ENTRY(
	"mult",			-1),
	/* Multiply:	push (stknext * stktop) */
    TCL_INSTRUCTION_ENTRY(
	"div",			-1),
	/* Divide:	push (stknext / stktop) */
    TCL_INSTRUCTION_ENTRY(
	"mod",			-1),
	/* Mod:		push (stknext % stktop) */
    TCL_INSTRUCTION_ENTRY(
	"uplus",		0),
	/* Unary plus:	push +stktop */
    TCL_INSTRUCTION_ENTRY(
	"uminus",		0),
	/* Unary minus:	push -stktop */
    TCL_INSTRUCTION_ENTRY(
	"bitnot",		0),
	/* Bitwise not:	push ~stktop */
    TCL_INSTRUCTION_ENTRY(
	"not",			0),
	/* Logical not:	push !stktop */
    TCL_INSTRUCTION_ENTRY(
	"tryCvtToNumeric",	0),
	/* Try converting stktop to first int then double if possible. */

    TCL_INSTRUCTION_ENTRY(
	"break",		0),
	/* Abort closest enclosing loop; if none, return TCL_BREAK code. */
    TCL_INSTRUCTION_ENTRY(
	"continue",		0),
	/* Skip to next iteration of closest enclosing loop; if none, return
	 * TCL_CONTINUE code. */

    TCL_INSTRUCTION_ENTRY1(
	"beginCatch",	  5,	0,	  OPERAND_UINT4),
	/* Record start of catch with the operand's exception index. Push the
	 * current stack depth onto a special catch stack. */
    TCL_INSTRUCTION_ENTRY(
	"endCatch",		0),
	/* End of last catch. Pop the bytecode interpreter's catch stack. */
    TCL_INSTRUCTION_ENTRY(
	"pushResult",		+1),
	/* Push the interpreter's object result onto the stack. */
    TCL_INSTRUCTION_ENTRY(
	"pushReturnCode",	+1),
	/* Push interpreter's return code (e.g. TCL_OK or TCL_ERROR) as a new
	 * object onto the stack. */

    TCL_INSTRUCTION_ENTRY(
	"streq",		-1),
	/* Str Equal:	push (stknext eq stktop) */
    TCL_INSTRUCTION_ENTRY(
	"strneq",		-1),
	/* Str !Equal:	push (stknext neq stktop) */
    TCL_INSTRUCTION_ENTRY(
	"strcmp",		-1),
	/* Str Compare:	push (stknext cmp stktop) */
    TCL_INSTRUCTION_ENTRY(
	"strlen",		0),
	/* Str Length:	push (strlen stktop) */
    TCL_INSTRUCTION_ENTRY(
	"strindex",		-1),
	/* Str Index:	push (strindex stknext stktop) */
    TCL_INSTRUCTION_ENTRY1(
	"strmatch",	  2,	-1,	  OPERAND_INT1),
	/* Str Match:	push (strmatch stknext stktop) opnd == nocase */

    TCL_INSTRUCTION_ENTRY1(
	"list",		  5,	INT_MIN,  OPERAND_UINT4),
	/* List:	push (stk1 stk2 ... stktop) */
    TCL_INSTRUCTION_ENTRY(
	"listIndex",		-1),
	/* List Index:	push (listindex stknext stktop) */
    TCL_INSTRUCTION_ENTRY(
	"listLength",		0),
	/* List Len:	push (listlength stktop) */

    DEPRECATED_INSTRUCTION_ENTRY1(
	"appendScalar1",  2,	0,	  OPERAND_LVT1),
	/* Append scalar variable at op1<=255 in frame; value is stktop */
    TCL_INSTRUCTION_ENTRY1(
	"appendScalar",	  5,	0,	  OPERAND_LVT4),
	/* Append scalar variable at op1 > 255 in frame; value is stktop */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"appendArray1",	  2,	-1,	  OPERAND_LVT1),
	/* Append array element; array at op1<=255, value is top then elem */
    TCL_INSTRUCTION_ENTRY1(
	"appendArray",	  5,	-1,	  OPERAND_LVT4),
	/* Append array element; array at op1>=256, value is top then elem */
    TCL_INSTRUCTION_ENTRY(
	"appendArrayStk",	-2),
	/* Append array element; value is stktop, then elem, array names */
    TCL_INSTRUCTION_ENTRY(
	"appendStk",		-1),
	/* Append general variable; value is stktop, then unparsed name */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"lappendScalar1", 2,	0,	  OPERAND_LVT1),
	/* Lappend scalar variable at op1<=255 in frame; value is stktop */
    TCL_INSTRUCTION_ENTRY1(
	"lappendScalar",  5,	0,	  OPERAND_LVT4),
	/* Lappend scalar variable at op1 > 255 in frame; value is stktop */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"lappendArray1",  2,	-1,	  OPERAND_LVT1),
	/* Lappend array element; array at op1<=255, value is top then elem */
    TCL_INSTRUCTION_ENTRY1(
	"lappendArray",	  5,	-1,	  OPERAND_LVT4),
	/* Lappend array element; array at op1>=256, value is top then elem */
    TCL_INSTRUCTION_ENTRY(
	"lappendArrayStk",	-2),
	/* Lappend array element; value is stktop, then elem, array names */
    TCL_INSTRUCTION_ENTRY(
	"lappendStk",		-1),
	/* Lappend general variable; value is stktop, then unparsed name */

    TCL_INSTRUCTION_ENTRY1(
	"lindexMulti",	  5,	INT_MIN,  OPERAND_UINT4),
	/* Lindex with generalized args, operand is number of stacked objs
	 * used: (operand-1) entries from stktop are the indices; then list to
	 * process. */
    TCL_INSTRUCTION_ENTRY1(
	"over",		  5,	+1,	  OPERAND_UINT4),
	/* Duplicate the arg-th element from top of stack (TOS=0) */
    TCL_INSTRUCTION_ENTRY(
	"lsetList",		-2),
	/* Four-arg version of 'lset'. stktop is old value; next is new
	 * element value, next is the index list; pushes new value */
    TCL_INSTRUCTION_ENTRY1(
	"lsetFlat",	 5,	INT_MIN,  OPERAND_UINT4),
	/* Three- or >=5-arg version of 'lset', operand is number of stacked
	 * objs: stktop is old value, next is new element value, next come
	 * (operand-2) indices; pushes the new value. */

    TCL_INSTRUCTION_ENTRY2(
	"returnImm",	  9,	-1,	  OPERAND_INT4, OPERAND_UINT4),
	/* Compiled [return], code, level are operands; options and result
	 * are on the stack. */
    TCL_INSTRUCTION_ENTRY(
	"expon",		-1),
	/* Binary exponentiation operator: push (stknext ** stktop) */

    /*
     * NOTE: the stack effects of expandStkTop and invokeExpanded are wrong -
     * but it cannot be done right at compile time, the stack effect is only
     * known at run time. The value for invokeExpanded is estimated better at
     * compile time.
     * See the comments further down in this file, where INST_INVOKE_EXPANDED
     * is emitted.
     */
    TCL_INSTRUCTION_ENTRY(
	"expandStart",		0),
	/* Start of command with {*} (expanded) arguments */
    TCL_INSTRUCTION_ENTRY1(
	"expandStkTop",	  5,	0,	  OPERAND_UINT4),
	/* Expand the list at stacktop: push its elements on the stack */
    TCL_INSTRUCTION_ENTRY(
	"invokeExpanded",	0),
	/* Invoke the command marked by the last 'expandStart' */

    TCL_INSTRUCTION_ENTRY1(
	"listIndexImm",	  5,	0,	  OPERAND_IDX4),
	/* List Index:	push (lindex stktop op4) */
    TCL_INSTRUCTION_ENTRY2(
	"listRangeImm",	  9,	0,	  OPERAND_IDX4, OPERAND_IDX4),
	/* List Range:	push (lrange stktop op4 op4) */
    TCL_INSTRUCTION_ENTRY2(
	"startCommand",	  9,	0,	  OPERAND_OFFSET4, OPERAND_UINT4),
	/* Start of bytecoded command: op is the length of the cmd's code, op2
	 * is number of commands here */

    TCL_INSTRUCTION_ENTRY(
	"listIn",		-1),
	/* List containment: push [lsearch stktop stknext]>=0 */
    TCL_INSTRUCTION_ENTRY(
	"listNotIn",		-1),
	/* List negated containment: push [lsearch stktop stknext]<0 */

    TCL_INSTRUCTION_ENTRY(
	"pushReturnOpts",	+1),
	/* Push the interpreter's return option dictionary as an object on the
	 * stack. */
    TCL_INSTRUCTION_ENTRY(
	"returnStk",		-1),
	/* Compiled [return]; options and result are on the stack, code and
	 * level are in the options. */

    TCL_INSTRUCTION_ENTRY1(
	"dictGet",	 5,	INT_MIN,  OPERAND_UINT4),
	/* The top op4 words (min 1) are a key path into the dictionary just
	 * below the keys on the stack, and all those values are replaced by
	 * the value read out of that key-path (like [dict get]).
	 * Stack:  ... dict key1 ... keyN => ... value */
    TCL_INSTRUCTION_ENTRY2(
	"dictSet",	 9,	INT_MIN,  OPERAND_UINT4, OPERAND_LVT4),
	/* Update a dictionary value such that the keys are a path pointing to
	 * the value. op4#1 = numKeys, op4#2 = LVTindex
	 * Stack:  ... key1 ... keyN value => ... newDict */
    TCL_INSTRUCTION_ENTRY2(
	"dictUnset",	 9,	INT_MIN,  OPERAND_UINT4, OPERAND_LVT4),
	/* Update a dictionary value such that the keys are not a path pointing
	 * to any value. op4#1 = numKeys, op4#2 = LVTindex
	 * Stack:  ... key1 ... keyN => ... newDict */
    TCL_INSTRUCTION_ENTRY2(
	"dictIncrImm",	 9,	0,	  OPERAND_INT4, OPERAND_LVT4),
	/* Update a dictionary value such that the value pointed to by key is
	 * incremented by some value (or set to it if the key isn't in the
	 * dictionary at all). op4#1 = incrAmount, op4#2 = LVTindex
	 * Stack:  ... key => ... newDict */
    TCL_INSTRUCTION_ENTRY1(
	"dictAppend",	 5,	-1,	  OPERAND_LVT4),
	/* Update a dictionary value such that the value pointed to by key has
	 * some value string-concatenated onto it. op4 = LVTindex
	 * Stack:  ... key valueToAppend => ... newDict */
    TCL_INSTRUCTION_ENTRY1(
	"dictLappend",	 5,	-1,	  OPERAND_LVT4),
	/* Update a dictionary value such that the value pointed to by key has
	 * some value list-appended onto it. op4 = LVTindex
	 * Stack:  ... key valueToAppend => ... newDict */
    TCL_INSTRUCTION_ENTRY1(
	"dictFirst",	 5,	+2,	  OPERAND_LVT4),
	/* Begin iterating over the dictionary, using the local scalar
	 * indicated by op4 to hold the iterator state. The local scalar
	 * should not refer to a named variable as the value is not wholly
	 * managed correctly.
	 * Stack:  ... dict => ... value key doneBool */
    TCL_INSTRUCTION_ENTRY1(
	"dictNext",	 5,	+3,	  OPERAND_LVT4),
	/* Get the next iteration from the iterator in op4's local scalar.
	 * Stack:  ... => ... value key doneBool */
    TCL_INSTRUCTION_ENTRY2(
	"dictUpdateStart", 9,	0,	  OPERAND_LVT4, OPERAND_AUX4),
	/* Create the variables (described in the aux data referred to by the
	 * second immediate argument) to mirror the state of the dictionary in
	 * the variable referred to by the first immediate argument. The list
	 * of keys (top of the stack, not popped) must be the same length as
	 * the list of variables.
	 * Stack:  ... keyList => ... keyList */
    TCL_INSTRUCTION_ENTRY2(
	"dictUpdateEnd", 9,	-1,	  OPERAND_LVT4, OPERAND_AUX4),
	/* Reflect the state of local variables (described in the aux data
	 * referred to by the second immediate argument) back to the state of
	 * the dictionary in the variable referred to by the first immediate
	 * argument. The list of keys (popped from the stack) must be the same
	 * length as the list of variables.
	 * Stack:  ... keyList => ... */
    TCL_INSTRUCTION_ENTRY1(
	"jumpTable",	 5,	-1,	  OPERAND_AUX4),
	/* Jump according to the jump-table (in AuxData as indicated by the
	 * operand) and the argument popped from the list. Always executes the
	 * next instruction if no match against the table's entries was found.
	 * Keys are strings.
	 * Stack:  ... value => ...
	 * Note that the jump table contains offsets relative to the PC when
	 * it points to this instruction; the code is relocatable. */
    TCL_INSTRUCTION_ENTRY1(
	"upvar",	 5,	-1,	  OPERAND_LVT4),
	/* finds level and otherName in stack, links to local variable at
	 * index op1. Leaves the level on stack. */
    TCL_INSTRUCTION_ENTRY1(
	"nsupvar",	 5,	-1,	  OPERAND_LVT4),
	/* finds namespace and otherName in stack, links to local variable at
	 * index op1. Leaves the namespace on stack. */
    TCL_INSTRUCTION_ENTRY1(
	"variable",	 5,	-1,	  OPERAND_LVT4),
	/* finds namespace and otherName in stack, links to local variable at
	 * index op1. Leaves the namespace on stack. */
    TCL_INSTRUCTION_ENTRY2(
	"syntax",	 9,	-1,	  OPERAND_INT4, OPERAND_UINT4),
	/* Compiled bytecodes to signal syntax error. Equivalent to returnImm
	 * except for the ERR_ALREADY_LOGGED flag in the interpreter. */
    TCL_INSTRUCTION_ENTRY1(
	"reverse",	 5,	0,	  OPERAND_UINT4),
	/* Reverse the order of the arg elements at the top of stack */

    TCL_INSTRUCTION_ENTRY1(
	"regexp",	 2,	-1,	  OPERAND_INT1),
	/* Regexp:	push (regexp stknext stktop) opnd == nocase */

    TCL_INSTRUCTION_ENTRY1(
	"existScalar",	 5,	1,	  OPERAND_LVT4),
	/* Test if scalar variable at index op1 in call frame exists */
    TCL_INSTRUCTION_ENTRY1(
	"existArray",	 5,	0,	  OPERAND_LVT4),
	/* Test if array element exists; array at slot op1, element is
	 * stktop */
    TCL_INSTRUCTION_ENTRY(
	"existArrayStk",	-1),
	/* Test if array element exists; element is stktop, array name is
	 * stknext */
    TCL_INSTRUCTION_ENTRY(
	"existStk",		0),
	/* Test if general variable exists; unparsed variable name is stktop*/

    TCL_INSTRUCTION_ENTRY(
	"nop",			0),
	/* Do nothing */
    DEPRECATED_INSTRUCTION_ENTRY(
	"returnCodeBranch1",	-1),
	/* Jump to next instruction based on the return code on top of stack
	 * ERROR: +1;	RETURN: +3;	BREAK: +5;	CONTINUE: +7;
	 * Other non-OK: +9 */

    TCL_INSTRUCTION_ENTRY2(
	"unsetScalar",	 6,	0,	  OPERAND_UNSF1, OPERAND_LVT4),
	/* Make scalar variable at index op2 in call frame cease to exist;
	 * op1 is 1 for errors on problems, 0 otherwise */
    TCL_INSTRUCTION_ENTRY2(
	"unsetArray",	 6,	-1,	  OPERAND_UNSF1, OPERAND_LVT4),
	/* Make array element cease to exist; array at slot op2, element is
	 * stktop; op1 is 1 for errors on problems, 0 otherwise */
    TCL_INSTRUCTION_ENTRY1(
	"unsetArrayStk", 2,	-2,	  OPERAND_UNSF1),
	/* Make array element cease to exist; element is stktop, array name is
	 * stknext; op1 is 1 for errors on problems, 0 otherwise */
    TCL_INSTRUCTION_ENTRY1(
	"unsetStk",	 2,	-1,	  OPERAND_UNSF1),
	/* Make general variable cease to exist; unparsed variable name is
	 * stktop; op1 is 1 for errors on problems, 0 otherwise */

    TCL_INSTRUCTION_ENTRY(
	"dictExpand",		-1),
	/* Probe into a dict and extract it (or a subdict of it) into
	 * variables with matched names. Produces list of keys bound as
	 * result. Part of [dict with].
	 * Stack:  ... dict path => ... keyList */
    TCL_INSTRUCTION_ENTRY(
	"dictRecombineStk",	-3),
	/* Map variable contents back into a dictionary in a variable. Part of
	 * [dict with].
	 * Stack:  ... dictVarName path keyList => ... */
    TCL_INSTRUCTION_ENTRY1(
	"dictRecombineImm", 5,	-2,	  OPERAND_LVT4),
	/* Map variable contents back into a dictionary in the local variable
	 * indicated by the LVT index. Part of [dict with].
	 * Stack:  ... path keyList => ... */
    TCL_INSTRUCTION_ENTRY1(
	"dictExists",	 5,	INT_MIN,  OPERAND_UINT4),
	/* The top op4 words (min 1) are a key path into the dictionary just
	 * below the keys on the stack, and all those values are replaced by a
	 * boolean indicating whether it is possible to read out a value from
	 * that key-path (like [dict exists]).
	 * Stack:  ... dict key1 ... keyN => ... boolean */
    TCL_INSTRUCTION_ENTRY(
	"verifyDict",		-1),
	/* Verifies that the word on the top of the stack is a dictionary,
	 * popping it if it is and throwing an error if it is not.
	 * Stack:  ... value => ... */

    TCL_INSTRUCTION_ENTRY(
	"strmap",		-2),
	/* Simplified version of [string map] that only applies one change
	 * string, and only case-sensitively.
	 * Stack:  ... from to string => ... changedString */
    TCL_INSTRUCTION_ENTRY(
	"strfind",		-1),
	/* Find the first index of a needle string in a haystack string,
	 * producing the index (integer) or -1 if nothing found.
	 * Stack:  ... needle haystack => ... index */
    TCL_INSTRUCTION_ENTRY(
	"strrfind",		-1),
	/* Find the last index of a needle string in a haystack string,
	 * producing the index (integer) or -1 if nothing found.
	 * Stack:  ... needle haystack => ... index */
    TCL_INSTRUCTION_ENTRY2(
	"strrangeImm",	 9,	0,	  OPERAND_IDX4, OPERAND_IDX4),
	/* String Range: push (string range stktop op4 op4) */
    TCL_INSTRUCTION_ENTRY(
	"strrange",		-2),
	/* String Range with non-constant arguments.
	 * Stack:  ... string idxA idxB => ... substring */

    TCL_INSTRUCTION_ENTRY(
	"yield",		0),
	/* Makes the current coroutine yield the value at the top of the
	 * stack, and places the response back on top of the stack when it
	 * resumes.
	 * Stack:  ... valueToYield => ... resumeValue */
    TCL_INSTRUCTION_ENTRY(
	"coroName",		+1),
	/* Push the name of the interpreter's current coroutine as an object
	 * on the stack. */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"tailcall",	 2,	INT_MIN,  OPERAND_UINT1),
	/* Do a tailcall with the opnd items on the stack as the thing to
	 * tailcall to; opnd must be greater than 0 for the semantics to work
	 * right. */

    TCL_INSTRUCTION_ENTRY(
	"currentNamespace",	+1),
	/* Push the name of the interpreter's current namespace as an object
	 * on the stack. */
    TCL_INSTRUCTION_ENTRY(
	"infoLevelNumber",	+1),
	/* Push the stack depth (i.e., [info level]) of the interpreter as an
	 * object on the stack. */
    TCL_INSTRUCTION_ENTRY(
	"infoLevelArgs",	0),
	/* Push the argument words to a stack depth (i.e., [info level <n>])
	 * of the interpreter as an object on the stack.
	 * Stack:  ... depth => ... argList */
    TCL_INSTRUCTION_ENTRY(
	"resolveCmd",		0),
	/* Resolves the command named on the top of the stack to its fully
	 * qualified version, or produces the empty string if no such command
	 * exists. Never generates errors.
	 * Stack:  ... cmdName => ... fullCmdName */

    TCL_INSTRUCTION_ENTRY(
	"tclooSelf",		+1),
	/* Push the identity of the current TclOO object (i.e., the name of
	 * its current public access command) on the stack. */
    TCL_INSTRUCTION_ENTRY(
	"tclooClass",		0),
	/* Push the class of the TclOO object named at the top of the stack
	 * onto the stack.
	 * Stack:  ... object => ... class */
    TCL_INSTRUCTION_ENTRY(
	"tclooNamespace",	0),
	/* Push the namespace of the TclOO object named at the top of the
	 * stack onto the stack.
	 * Stack:  ... object => ... namespace */
    TCL_INSTRUCTION_ENTRY(
	"tclooIsObject",	0),
	/* Push whether the value named at the top of the stack is a TclOO
	 * object (i.e., a boolean). Can corrupt the interpreter result
	 * despite not throwing, so not safe for use in a post-exception
	 * context.
	 * Stack:  ... value => ... boolean */

    TCL_INSTRUCTION_ENTRY(
	"arrayExistsStk",	0),
	/* Looks up the element on the top of the stack and tests whether it
	 * is an array. Pushes a boolean describing whether this is the
	 * case. Also runs the whole-array trace on the named variable, so can
	 * throw anything.
	 * Stack:  ... varName => ... boolean */
    TCL_INSTRUCTION_ENTRY1(
	"arrayExistsImm", 5,	+1,	  OPERAND_LVT4),
	/* Looks up the variable indexed by opnd and tests whether it is an
	 * array. Pushes a boolean describing whether this is the case. Also
	 * runs the whole-array trace on the named variable, so can throw
	 * anything.
	 * Stack:  ... => ... boolean */
    TCL_INSTRUCTION_ENTRY(
	"arrayMakeStk",		-1),
	/* Forces the element on the top of the stack to be the name of an
	 * array.
	 * Stack:  ... varName => ... */
    TCL_INSTRUCTION_ENTRY1(
	"arrayMakeImm",	 5,	0,	  OPERAND_LVT4),
	/* Forces the variable indexed by opnd to be an array. Does not touch
	 * the stack. */

    TCL_INSTRUCTION_ENTRY2(
	"invokeReplace", 6, INT_MIN,	  OPERAND_UINT4, OPERAND_UINT1),
	/* Invoke command named objv[0], replacing the first two words with
	 * the op1 words at the top of the stack;
	 * <objc,objv> = <op4,top op4 after popping 1> */

    TCL_INSTRUCTION_ENTRY(
	"listConcat",		-1),
	/* Concatenates the two lists at the top of the stack into a single
	 * list and pushes that resulting list onto the stack.
	 * Stack: ... list1 list2 => ... [lconcat list1 list2] */

    TCL_INSTRUCTION_ENTRY(
	"expandDrop",		0),
	/* Drops an element from the auxiliary stack, popping stack elements
	 * until the matching stack depth is reached. */

    /* New foreach implementation */
    TCL_INSTRUCTION_ENTRY1(
	"foreach_start", 5,	+2,	  OPERAND_AUX4),
	/* Initialize execution of a foreach loop. Operand is aux data index
	 * of the ForeachInfo structure for the foreach command. It pushes 2
	 * elements which hold runtime params for foreach_step, they are later
	 * dropped by foreach_end together with the value lists. NOTE that the
	 * iterator-tracker and info reference must not be passed to bytecodes
	 * that handle normal Tcl values. NOTE that this instruction jumps to
	 * the foreach_step instruction paired with it; the stack info below
	 * is only nominal.
	 * Stack: ... listObjs... => ... listObjs... iterTracker info */
    TCL_INSTRUCTION_ENTRY(
	"foreach_step",		0),
	/* "Step" or begin next iteration of foreach loop. Assigns to foreach
	 * iteration variables. May jump to straight after the foreach_start
	 * that pushed the iterTracker and info values. MUST be followed
	 * immediately by a foreach_end.
	 * Stack: ... listObjs... iterTracker info =>
	 *				... listObjs... iterTracker info */
    TCL_INSTRUCTION_ENTRY(
	"foreach_end",		0),
	/* Clean up a foreach loop by dropping the info value, the tracker
	 * value and the lists that were being iterated over.
	 * Stack: ... listObjs... iterTracker info => ... */
    TCL_INSTRUCTION_ENTRY(
	"lmap_collect",		-1),
	/* Appends the value at the top of the stack to the list located on
	 * the stack the "other side" of the foreach-related values.
	 * Stack: ... collector listObjs... iterTracker info value =>
	 *			... collector listObjs... iterTracker info */

    TCL_INSTRUCTION_ENTRY(
	"strtrim",		-1),
	/* [string trim] core: removes the characters (designated by the value
	 * at the top of the stack) from both ends of the string and pushes
	 * the resulting string.
	 * Stack: ... string charset => ... trimmedString */
    TCL_INSTRUCTION_ENTRY(
	"strtrimLeft",		-1),
	/* [string trimleft] core: removes the characters (designated by the
	 * value at the top of the stack) from the left of the string and
	 * pushes the resulting string.
	 * Stack: ... string charset => ... trimmedString */
    TCL_INSTRUCTION_ENTRY(
	"strtrimRight",		-1),
	/* [string trimright] core: removes the characters (designated by the
	 * value at the top of the stack) from the right of the string and
	 * pushes the resulting string.
	 * Stack: ... string charset => ... trimmedString */

    TCL_INSTRUCTION_ENTRY1(
	"concatStk",	 5,	INT_MIN,  OPERAND_UINT4),
	/* Wrapper round Tcl_ConcatObj(), used for [concat] and [eval]. opnd
	 * is number of values to concatenate.
	 * Operation:	push concat(stk1 stk2 ... stktop) */

    TCL_INSTRUCTION_ENTRY(
	"strcaseUpper",		0),
	/* [string toupper] core: converts whole string to upper case using
	 * the default (extended "C" locale) rules.
	 * Stack: ... string => ... newString */
    TCL_INSTRUCTION_ENTRY(
	"strcaseLower",		0),
	/* [string tolower] core: converts whole string to upper case using
	 * the default (extended "C" locale) rules.
	 * Stack: ... string => ... newString */
    TCL_INSTRUCTION_ENTRY(
	"strcaseTitle",		0),
	/* [string totitle] core: converts whole string to upper case using
	 * the default (extended "C" locale) rules.
	 * Stack: ... string => ... newString */
    TCL_INSTRUCTION_ENTRY(
	"strreplace",		-3),
	/* [string replace] core: replaces a non-empty range of one string
	 * with the contents of another.
	 * Stack: ... string fromIdx toIdx replacement => ... newString */

    TCL_INSTRUCTION_ENTRY(
	"originCmd",		0),
	/* Reports which command was the origin (via namespace import chain)
	 * of the command named on the top of the stack.
	 * Stack:  ... cmdName => ... fullOriginalCmdName */

    DEPRECATED_INSTRUCTION_ENTRY1(
	"tclooNext",	 2,	INT_MIN,  OPERAND_UINT1),
	/* Call the next item on the TclOO call chain, passing opnd arguments
	 * (min 1, max 255, *includes* "next").  The result of the invoked
	 * method implementation will be pushed on the stack in place of the
	 * arguments (similar to invokeStk).
	 * Stack:  ... "next" arg2 arg3 -- argN => ... result */
    DEPRECATED_INSTRUCTION_ENTRY1(
	"tclooNextClass", 2,	INT_MIN,  OPERAND_UINT1),
	/* Call the following item on the TclOO call chain defined by class
	 * className, passing opnd arguments (min 2, max 255, *includes*
	 * "nextto" and the class name). The result of the invoked method
	 * implementation will be pushed on the stack in place of the
	 * arguments (similar to invokeStk).
	 * Stack:  ... "nextto" className arg3 arg4 -- argN => ... result */

    TCL_INSTRUCTION_ENTRY(
	"yieldToInvoke",	0),
	/* Makes the current coroutine yield the value at the top of the
	 * stack, invoking the given command/args with resolution in the given
	 * namespace (all packed into a list), and places the list of values
	 * that are the response back on top of the stack when it resumes.
	 * Stack:  ... [list ns cmd arg1 ... argN] => ... resumeList */

    TCL_INSTRUCTION_ENTRY(
	"numericType",		0),
	/* Pushes the numeric type code of the word at the top of the stack.
	 * Stack:  ... value => ... typeCode */
    TCL_INSTRUCTION_ENTRY(
	"tryCvtToBoolean",	+1),
	/* Try converting stktop to boolean if possible. No errors.
	 * Stack:  ... value => ... value isStrictBool */
    TCL_INSTRUCTION_ENTRY1(
	"strclass",	 2,	0,	  OPERAND_SCLS1),
	/* See if all the characters of the given string are a member of the
	 * specified (by opnd) character class. Note that an empty string will
	 * satisfy the class check (standard definition of "all").
	 * Stack:  ... stringValue => ... boolean */

    TCL_INSTRUCTION_ENTRY1(
	"lappendList",	 5,	0,	  OPERAND_LVT4),
	/* Lappend list to scalar variable at op4 in frame.
	 * Stack:  ... list => ... listVarContents */
    TCL_INSTRUCTION_ENTRY1(
	"lappendListArray", 5,	-1,	  OPERAND_LVT4),
	/* Lappend list to array element; array at op4.
	 * Stack:  ... elem list => ... listVarContents */
    TCL_INSTRUCTION_ENTRY(
	"lappendListArrayStk",	-2),
	/* Lappend list to array element.
	 * Stack:  ... arrayName elem list => ... listVarContents */
    TCL_INSTRUCTION_ENTRY(
	"lappendListStk",	-1),
	/* Lappend list to general variable.
	 * Stack:  ... varName list => ... listVarContents */

    TCL_INSTRUCTION_ENTRY1(
	"clockRead",	 2,	+1,	  OPERAND_CLK1),
	/* Read clock out to the stack. Operand is which clock to read
	 * 0=clicks, 1=microseconds, 2=milliseconds, 3=seconds.
	 * Stack: ... => ... time */

    TCL_INSTRUCTION_ENTRY1(
	"dictGetDef",	 5,	INT_MIN,  OPERAND_UINT4),
	/* The top word is the default, the next op4 words (min 1) are a key
	 * path into the dictionary just below the keys on the stack, and all
	 * those values are replaced by the value read out of that key-path
	 * (like [dict get]) except if there is no such key, when instead the
	 * default is pushed instead.
	 * Stack:  ... dict key1 ... keyN default => ... value */

    TCL_INSTRUCTION_ENTRY(
	"strlt",		-1),
	/* String Less:			push (stknext < stktop) */
    TCL_INSTRUCTION_ENTRY(
	"strgt",		-1),
	/* String Greater:		push (stknext > stktop) */
    TCL_INSTRUCTION_ENTRY(
	"strle",		-1),
	/* String Less or equal:	push (stknext <= stktop) */
    TCL_INSTRUCTION_ENTRY(
	"strge",		-1),
	/* String Greater or equal:	push (stknext >= stktop) */
    TCL_INSTRUCTION_ENTRY2(
	"lreplace",	  6,	INT_MIN,  OPERAND_UINT4, OPERAND_LRPL1),
	/* Operands: number of arguments, flags
	 * flags: Combination of TCL_LREPLACE_* flags
	 * Stack: ... listobj index1 ?index2? new1 ... newN => ... newlistobj
	 * where index2 is present only if TCL_LREPLACE_SINGLE_INDEX is not
	 * set in flags. */

    TCL_INSTRUCTION_ENTRY1(
	"constImm",	  5,	-1,	  OPERAND_LVT4),
	/* Create constant. Index into LVT is immediate, value is on stack.
	 * Stack: ... value => ... */
    TCL_INSTRUCTION_ENTRY(
	"constStk",		-2),
	/* Create constant. Variable name and value on stack.
	 * Stack: ... varName value => ... */

    TCL_INSTRUCTION_ENTRY1(
	"incrScalar",	  5,	0,	  OPERAND_LVT4),
	/* Incr scalar at index op1 in frame; incr amount is stktop */
    TCL_INSTRUCTION_ENTRY1(
	"incrArray",	  5,	-1,	  OPERAND_LVT4),
	/* Incr array elem; arr at slot op1, amount is top then elem */
    TCL_INSTRUCTION_ENTRY2(
	"incrScalarImm",  6,	+1,	  OPERAND_LVT4, OPERAND_INT1),
	/* Incr scalar at slot op1; amount is 2nd operand byte */
    TCL_INSTRUCTION_ENTRY2(
	"incrArrayImm",	  6,	0,	  OPERAND_LVT4, OPERAND_INT1),
	/* Incr array elem; array at slot op1, elem is stktop,
	 * amount is 2nd operand byte */
    TCL_INSTRUCTION_ENTRY1(
	"tailcall",	  5,	INT_MIN,  OPERAND_UINT4),
	/* Do a tailcall with the opnd items on the stack as the thing to
	 * tailcall to; opnd must be greater than 0 for the semantics to work
	 * right. */
    TCL_INSTRUCTION_ENTRY1(
	"tclooNext",	  5,	INT_MIN,  OPERAND_UINT4),
	/* Call the next item on the TclOO call chain, passing opnd arguments
	 * (min 1, *includes* "next").  The result of the invoked
	 * method implementation will be pushed on the stack in place of the
	 * arguments (similar to invokeStk).
	 * Stack:  ... "next" arg2 arg3 -- argN => ... result */
    TCL_INSTRUCTION_ENTRY1(
	"tclooNextClass", 5,	INT_MIN,  OPERAND_UINT4),
	/* Call the following item on the TclOO call chain defined by class
	 * className, passing opnd arguments (min 2, *includes*
	 * "nextto" and the class name). The result of the invoked method
	 * implementation will be pushed on the stack in place of the
	 * arguments (similar to invokeStk).
	 * Stack:  ... "nextto" className arg3 arg4 -- argN => ... result */

    TCL_INSTRUCTION_ENTRY(
	"swap",			0),
	/* Exchanges the top two items on the stack.
	 * Stack:  ... val1 val2 => ... val2 val1 */
    TCL_INSTRUCTION_ENTRY1(
	"errorPrefixEq",  5,	-1,	  OPERAND_UINT4),
	/* Compare the two lists at stack top for equality in the first opnd
	 * words. The words are themselves compared using string equality.
	 * As: [string equal [lrange list1 0 opnd] [lrange list2 0 opnd]]
	 * Stack:  ... list1 list2 => isEqual */
    TCL_INSTRUCTION_ENTRY(
	"tclooId",		0),
	/* Push the global ID of the TclOO object named at the top of the
	 * stack onto the stack.
	 * Stack:  ... object => ... id */
    TCL_INSTRUCTION_ENTRY(
	"dictPut",		-2),
	/* Modify the dict by replacing/creating the key/value pair given,
	 * pushing the result on the stack.
	 * Stack:  ... dict key value => ... updatedDict */
    TCL_INSTRUCTION_ENTRY(
	"dictRemove",		-1),
	/* Modify the dict by removing the key/value pair for the given key,
	 * pushing the result on the stack.
	 * Stack:  ... dict key => ... updatedDict */
    TCL_INSTRUCTION_ENTRY(
	"isEmpty",		0),
	/* Test if the value at the top of the stack is empty (via a call to
	 * Tcl_IsEmpty).
	 * Stack:  ... value => ... boolean */
    TCL_INSTRUCTION_ENTRY1(
	"jumpTableNum",	 5,	-1,	  OPERAND_AUX4),
	/* Jump according to the jump-table (in AuxData as indicated by the
	 * operand) and the argument popped from the list. Always executes the
	 * next instruction if no match against the table's entries was found.
	 * Keys are Tcl_WideInt.
	 * Stack:  ... value => ...
	 * Note that the jump table contains offsets relative to the PC when
	 * it points to this instruction; the code is relocatable. */
    TCL_INSTRUCTION_ENTRY(
	"tailcallList",		0),
	/* Do a tailcall with the words from the argument as the thing to
	 * tailcall to, and currNs is the namespace scope.
	 * Stack: ... {currNs words...} => ...[NOT REACHED] */
    TCL_INSTRUCTION_ENTRY(
	"tclooNextList",	0),
	/* Call the next item on the TclOO call chain, passing the arguments
	 * from argumentList (min 1, *includes* "next"). The result of the
	 * invoked method implementation will be pushed on the stack after the
	 * target returns.
	 * Stack:  ... argumentList => ... result */
    TCL_INSTRUCTION_ENTRY(
	"tclooNextClassList",	0),
	/* Call the following item on the TclOO call chain defined by class
	 * className, passing the arguments from argumentList (min 2,
	 * *includes* "nextto" and the class name). The result of the invoked
	 * method implementation will be pushed on the stack after the target
	 * returns.
	 * Stack:  ... argumentList => ... result */
    TCL_INSTRUCTION_ENTRY1(
	"arithSeries",	  2,	-3,	  OPERAND_UINT1),
	/* Push a new arithSeries object on the stack. The opnd is a bit mask
	 * stating which values are valid; bit 0 -> from, bit 1 -> to,
	 * bit 2 -> step, bit 3 -> count. Invalid values are passed to
	 * TclNewArithSeriesObj() as NULL (and the corresponding values on the
	 * stack simply are ignored).
	 * Stack:  ... from to step count => ... series */
    TCL_INSTRUCTION_ENTRY(
	"uplevel",		-1),
	/* Call the script in the given stack level, and stack the result.
	 * Stack:  ... level script => ... result */

    {NULL, 0, 0, 0, {OPERAND_NONE}}
};

/*
 * Prototypes for procedures defined later in this file:
 */

static void		CleanupByteCode(ByteCode *codePtr);
static ByteCode *	CompileSubstObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
			    int flags);
static void		DupByteCodeInternalRep(Tcl_Obj *, Tcl_Obj *);
static unsigned char *	EncodeCmdLocMap(CompileEnv *envPtr,
			    ByteCode *codePtr, unsigned char *startPtr);
static void		EnterCmdExtentData(CompileEnv *envPtr,
			    Tcl_Size cmdNumber, Tcl_Size numSrcBytes,
			    Tcl_Size numCodeBytes);
static void		EnterCmdStartData(CompileEnv *envPtr,
			    Tcl_Size cmdNumber, Tcl_Size srcOffset,
			    Tcl_Size codeOffset);
static void		FreeByteCodeInternalRep(Tcl_Obj *objPtr);
static void		FreeSubstCodeInternalRep(Tcl_Obj *objPtr);
static int		GetCmdLocEncodingSize(CompileEnv *envPtr);
static int		IsCompactibleCompileEnv(CompileEnv *envPtr);
static void		PreventCycle(Tcl_Obj *objPtr, CompileEnv *envPtr);
#ifdef TCL_COMPILE_STATS
static void		RecordByteCodeStats(ByteCode *codePtr);
#endif /* TCL_COMPILE_STATS */
static int		SetByteCodeFromAny(Tcl_Interp *interp,
			    Tcl_Obj *objPtr);
static void		StartExpanding(CompileEnv *envPtr);

/*
 * TIP #280: Helper for building the per-word line information of all compiled
 * commands.
 */
static void		EnterCmdWordData(ExtCmdLoc *eclPtr, Tcl_Size srcOffset,
			    Tcl_Token *tokenPtr, const char *cmd,
			    Tcl_Size numWords, int line,
			    Tcl_Size *clNext, int **lines,
			    CompileEnv *envPtr);
static void		ReleaseCmdWordData(ExtCmdLoc *eclPtr);

/*
 * tclByteCodeType provides the standard type management procedures for the
 * bytecode type.
 */

const Tcl_ObjType tclByteCodeType = {
    "bytecode",			/* name */
    FreeByteCodeInternalRep,	/* freeIntRepProc */
    DupByteCodeInternalRep,	/* dupIntRepProc */
    NULL,			/* updateStringProc */
    SetByteCodeFromAny,		/* setFromAnyProc */
    TCL_OBJTYPE_V0
};

/*
 * substCodeType provides the standard type management procedures for the
 * substcode type, which represents substitution within a Tcl value.
 */

static const Tcl_ObjType substCodeType = {
    "substcode",		/* name */
    FreeSubstCodeInternalRep,	/* freeIntRepProc */
    DupByteCodeInternalRep,	/* dupIntRepProc - shared with bytecode */
    NULL,			/* updateStringProc */
    NULL,			/* setFromAnyProc */
    TCL_OBJTYPE_V0
};
#define SubstFlags(objPtr) (objPtr)->internalRep.twoPtrValue.ptr2

/*
 * Helper macros.
 */

#define TclIncrUInt4AtPtr(ptr, delta) \
    TclStoreInt4AtPtr(TclGetUInt4AtPtr(ptr)+(delta), (ptr))

/*
 *----------------------------------------------------------------------
 *
 * TclSetByteCodeFromAny --
 *
 *	Part of the bytecode Tcl object type implementation. Attempts to
 *	compile the string representation of the objPtr into bytecode.  Accepts
 *	a hook routine that is invoked to perform any needed post-processing on
 *	the compilation results before generating byte codes.  interp is the
 *	compilation context and may not be NULL.
 *
 * Results:
 *	A standard Tcl object result. If an error occurs during compilation, an
 *	error message is left in the interpreter's result.
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
    void *clientData)		/* Hook procedure private data. */
{
    Interp *iPtr = (Interp *) interp;
    CompileEnv compEnv;		/* Compilation environment structure allocated
				 * in frame. */
    Tcl_Size length;
    int result = TCL_OK;
    const char *stringPtr;
    Proc *procPtr = iPtr->compiledProcPtr;
    ContLineLoc *clLocPtr;

#ifdef TCL_COMPILE_DEBUG
    if (!traceInitialized) {
	if (Tcl_LinkVar(interp, "tcl_traceCompile",
		&tclTraceCompile, TCL_LINK_INT) != TCL_OK) {
	    Tcl_Panic("SetByteCodeFromAny: "
		    "unable to create link for tcl_traceCompile variable");
	}
	traceInitialized = 1;
    }
#endif

    stringPtr = TclGetStringFromObj(objPtr, &length);

    /*
     * TIP #280: Pick up the CmdFrame in which the BC compiler was invoked, and
     * use to initialize the tracking in the compiler. This information was
     * stored by TclCompEvalObj and ProcCompileProc.
     */

    TclInitCompileEnv(interp, &compEnv, stringPtr, length,
	    iPtr->invokeCmdFramePtr, iPtr->invokeWord);

    /*
     * Make available to the compilation environment any data about invisible
     * continuation lines for the script.
     *
     * It is not clear if the script Tcl_Obj* can be free'd while the compiler
     * is using it, leading to the release of the associated ContLineLoc
     * structure as well. To ensure that the latter doesn't happen set a lock
     * on it, which is released in TclFreeCompileEnv().  The "lineCLPtr"
     * hashtable tclObj.c.
     */

    clLocPtr = TclContinuationsGet(objPtr);
    if (clLocPtr) {
	compEnv.clNext = &clLocPtr->loc[0];
    }

    TclCompileScript(interp, stringPtr, length, &compEnv);

    /*
     * Compilation succeeded. Add a "done" instruction at the end.
     */

    TclEmitOpcode(		INST_DONE,			&compEnv);

    /*
     * Check for optimizations!
     *
     * If the generated code is free of most hazards, recompile with generation
     * of INST_START_CMD disabled to produce code that more compact in many
     * cases, and also sometimes more performant.
     */

    if (Tcl_GetParent(interp) == NULL &&
	    !Tcl_LimitTypeEnabled(interp, TCL_LIMIT_COMMANDS|TCL_LIMIT_TIME)
	    && IsCompactibleCompileEnv(&compEnv)) {
	TclFreeCompileEnv(&compEnv);
	iPtr->compiledProcPtr = procPtr;
	TclInitCompileEnv(interp, &compEnv, stringPtr, length,
		iPtr->invokeCmdFramePtr, iPtr->invokeWord);
	if (clLocPtr) {
	    compEnv.clNext = &clLocPtr->loc[0];
	}
	compEnv.atCmdStart = 2;		/* The disabling magic. */
	TclCompileScript(interp, stringPtr, length, &compEnv);
	assert (compEnv.atCmdStart > 1);
	TclEmitOpcode(		INST_DONE,			&compEnv);
	assert (compEnv.atCmdStart > 1);
    }

    /*
     * Apply some peephole optimizations that can cross specific/generic
     * instruction generator boundaries.
     */

    if (iPtr->optimizer) {
	(iPtr->optimizer)(&compEnv);
    }

    /*
     * Invoke the compilation hook procedure if there is one.
     */

    if (hookProc) {
	result = hookProc(interp, &compEnv, clientData);
    }

    /*
     * After optimization is all done, check that byte code length limits
     * are not exceeded. Bug [27b3ce2997].
     */
    if (CurrentOffset(&compEnv) > INT_MAX) {
	/*
	 * Cannot just return TCL_ERROR as callers ignore return value.
	 * TODO - May be use TclCompileSyntaxError here?
	 */
	Tcl_Panic("Maximum byte code length %d exceeded.", INT_MAX);
    }

    /*
     * Change the object into a ByteCode object. Ownership of the literal
     * objects and aux data items passes to the ByteCode object.
     */

#ifdef TCL_COMPILE_DEBUG
    TclVerifyLocalLiteralTable(&compEnv);
#endif /*TCL_COMPILE_DEBUG*/

    if (result == TCL_OK) {
	(void) TclInitByteCodeObj(objPtr, &tclByteCodeType, &compEnv);
	TclDebugPrintByteCodeObj(objPtr);
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
 *	A standard Tcl object result. If an error occurs during compilation and
 *	"interp" is not null, an error message is left in the interpreter's
 *	result.
 *
 * Side effects:
 *	Frees the old internal representation. If no error occurs then the
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
    Tcl_Obj *objPtr)		/* The object to compile to bytecode */
{
    if (interp == NULL) {
	return TCL_ERROR;
    }
    return TclSetByteCodeFromAny(interp, objPtr, NULL, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * DupByteCodeInternalRep --
 *
 *	Part of the bytecode Tcl object type implementation. However, it does
 *	not copy the internal representation of a bytecode Tcl_Obj, instead
 *	assigning NULL to the type pointer of the new object.  Code is compiled
 *	for the new object only if necessary.
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
    TCL_UNUSED(Tcl_Obj *) /*srcPtr*/,
    TCL_UNUSED(Tcl_Obj *) /*copyPtr*/)
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
 *	The bytecode object's internal rep is invalidated and its code is freed
 *	unless the code is actively being executed, in which case cleanup is
 *	delayed until the last execution of the code completes.
 *
 *----------------------------------------------------------------------
 */

static void
FreeByteCodeInternalRep(
    Tcl_Obj *objPtr)		/* Object whose internal rep to free. */
{
    ByteCode *codePtr;

    ByteCodeGetInternalRep(objPtr, &tclByteCodeType, codePtr);
    assert(codePtr != NULL);

    TclReleaseByteCode(codePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclReleaseByteCode --
 *
 *	Does all the real work of freeing up a bytecode object's ByteCode
 *	structure. Called only when the structure's reference count
 *	is zero.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees objPtr's bytecode internal representation and sets its type to
 *	NULL.  Also releases its literals and frees its auxiliary data items.
 *
 *----------------------------------------------------------------------
 */

void
TclPreserveByteCode(
    ByteCode *codePtr)
{
    codePtr->refCount++;
}

void
TclReleaseByteCode(
    ByteCode *codePtr)
{
    if (codePtr->refCount-- > 1) {
	return;
    }

    /* Just dropped to refcount==0.  Clean up. */
    CleanupByteCode(codePtr);
}

static void
CleanupByteCode(
    ByteCode *codePtr)		/* Points to the ByteCode to free. */
{
    Tcl_Interp *interp = (Tcl_Interp *) *codePtr->interpHandle;
    Interp *iPtr = (Interp *) interp;
    Tcl_Size numLitObjects = codePtr->numLitObjects;
    Tcl_Size numAuxDataItems = codePtr->numAuxDataItems;
    Tcl_Obj **objArrayPtr, *objPtr;
    const AuxData *auxDataPtr;
    int i;
#ifdef TCL_COMPILE_STATS

    if (interp != NULL) {
	ByteCodeStats *statsPtr;
	Tcl_Time destroyTime;
	long long lifetimeSec, lifetimeMicroSec;

	statsPtr = &iPtr->stats;

	statsPtr->numByteCodesFreed++;
	statsPtr->currentSrcBytes -= (double)codePtr->numSrcBytes;
	statsPtr->currentByteCodeBytes -= (double) codePtr->structureSize;

	statsPtr->currentInstBytes -= (double) codePtr->numCodeBytes;
	statsPtr->currentLitBytes -= (double)
		numLitObjects * sizeof(Tcl_Obj *);
	statsPtr->currentExceptBytes -= (double)
		codePtr->numExceptRanges * sizeof(ExceptionRange);
	statsPtr->currentAuxBytes -= (double)
		codePtr->numAuxDataItems * sizeof(AuxData);
	statsPtr->currentCmdMapBytes -= (double) codePtr->numCmdLocBytes;

	Tcl_GetTime(&destroyTime);
	lifetimeSec = destroyTime.sec - codePtr->createTime.sec;
	lifetimeMicroSec = 1000000 * lifetimeSec +
		(destroyTime.usec - codePtr->createTime.usec);
	statsPtr->lifetimeCount[TclLog2(lifetimeMicroSec)]++;
    }
#endif /* TCL_COMPILE_STATS */

    /*
     * A single heap object holds the ByteCode structure and its code, object,
     * command location, and auxiliary data arrays. This means we only need to
     * 1) decrement the ref counts of each LiteralEntry in the literal array,
     * 2) call the free procedures for the auxiliary data items, 3) free the
     * localCache if it is unused, and finally 4) free the ByteCode
     * structure's heap object.
     *
     * The case for TCL_BYTECODE_PRECOMPILED (precompiled ByteCodes, like
     * those generated from tbcload) is special, as they doesn't make use of
     * the global literal table. They instead maintain private references to
     * their literals which must be decremented.
     *
     * In order to ensure proper and efficient cleanup of the literal array
     * when it contains non-shared literals [Bug 983660], distinguish the case
     * of an interpreter being deleted, which is signaled by interp == NULL.
     * Also, as the interp deletion will remove the global literal table
     * anyway, avoid the extra cost of updating it for each literal being
     * released.
     */

    if (codePtr->flags & TCL_BYTECODE_PRECOMPILED) {

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
	while (numLitObjects--) {
	    /* TclReleaseLiteral calls Tcl_DecrRefCount() for us */
	    TclReleaseLiteral(interp, *objArrayPtr++);
	}
    }

    auxDataPtr = codePtr->auxDataArrayPtr;
    for (i = 0;  i < numAuxDataItems;  i++) {
	if (auxDataPtr->type->freeProc != NULL) {
	    auxDataPtr->type->freeProc(auxDataPtr->clientData);
	}
	auxDataPtr++;
    }

    /*
     * TIP #280. Release the location data associated with this bytecode
     * structure, if any. The associated interp may be gone already, and the
     * data with it.
     *
     * See also tclBasic.c, DeleteInterpProc
     */

    if (iPtr) {
	Tcl_HashEntry *hePtr = Tcl_FindHashEntry(iPtr->lineBCPtr,
		codePtr);

	if (hePtr) {
	    ReleaseCmdWordData((ExtCmdLoc *)Tcl_GetHashValue(hePtr));
	    Tcl_DeleteHashEntry(hePtr);
	}
    }

    if (codePtr->localCachePtr && (codePtr->localCachePtr->refCount-- <= 1)) {
	TclFreeLocalCache(interp, codePtr->localCachePtr);
    }

    TclHandleRelease(codePtr->interpHandle);
    Tcl_Free(codePtr);
}

/*
 * ---------------------------------------------------------------------
 *
 * IsCompactibleCompileEnv --
 *
 *	Determines whether some basic compaction optimizations may be applied
 *	to a piece of bytecode. Idempotent.
 *
 * ---------------------------------------------------------------------
 */

static int
IsCompactibleCompileEnv(
    CompileEnv *envPtr)
{
    unsigned char *pc;
    int size;

    /*
     * Special: procedures in the '::tcl' namespace (or its children) are
     * considered to be well-behaved, so compaction can be applied to them even
     * if it would otherwise be invalid.
     */

    if (EnvIsProc(envPtr) && envPtr->procPtr->cmdPtr) {
	Namespace *nsPtr = envPtr->procPtr->cmdPtr->nsPtr;

	if (nsPtr && (strcmp(nsPtr->fullName, "::tcl") == 0
		|| strncmp(nsPtr->fullName, "::tcl::", 7) == 0)) {
	    return 1;
	}
    }

    /*
     * Go through and ensure that no operation involved can cause a desired
     * change of bytecode sequence during its execution. This comes down to
     * ensuring that there are no mapped variables (due to traces) or calls to
     * external commands (traces, [uplevel] trickery). This is actually a very
     * conservative check.  It turns down a lot of code that is OK in practice.
     */

    for (pc = envPtr->codeStart ; pc < envPtr->codeNext ; pc += size) {
	switch (*pc) {
	    /* Invokes */
	case INST_INVOKE_STK1:
	case INST_INVOKE_STK:
	case INST_INVOKE_EXPANDED:
	case INST_INVOKE_REPLACE:
	    return 0;
	    /* Runtime evals */
	case INST_EVAL_STK:
	case INST_EXPR_STK:
	case INST_YIELD:
	case INST_YIELD_TO_INVOKE:
	    return 0;
	    /* Upvars */
	case INST_UPVAR:
	case INST_NSUPVAR:
	case INST_VARIABLE:
	    return 0;
	    /* TclOO::next is NOT a problem: puts stack frame out of way.
	     * There's a way to do it, but it's beneath the threshold of
	     * likelihood. */
	case INST_TCLOO_NEXT:
	case INST_TCLOO_NEXT_CLASS:
	default:
	    size = tclInstructionTable[*pc].numBytes;
	    assert (size > 0);
	    break;
	}
    }

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SubstObj --
 *
 *	Performs substitutions on the given string as described in the user
 *	documentation for "subst".
 *
 * Results:
 *	A Tcl_Obj* containing the substituted string, or NULL to indicate that
 *	an error occurred.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_SubstObj(
    Tcl_Interp *interp,		/* Interpreter in which substitution occurs */
    Tcl_Obj *objPtr,		/* The value to be substituted. */
    int flags)			/* What substitutions to do. */
{
    NRE_callback *rootPtr = TOP_CB(interp);

    if (TclNRRunCallbacks(interp, Tcl_NRSubstObj(interp, objPtr, flags),
	    rootPtr) != TCL_OK) {
	return NULL;
    }
    return Tcl_GetObjResult(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NRSubstObj --
 *
 *	Adds substitution within the value of objPtr to the NR execution stack.
 *
 * Results:
 *	TCL_OK.
 *
 * Side effects:
 *	Compiles objPtr into bytecode that performs the substitutions as
 *	governed by flags, adds a callback to the NR execution stack to execute
 *	the bytecode and store the result in the interp.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_NRSubstObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    int flags)
{
    ByteCode *codePtr = CompileSubstObj(interp, objPtr, flags);

    /* TODO: Confirm we do not need this. */
    /* Tcl_ResetResult(interp); */
    return TclNRExecuteByteCode(interp, codePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CompileSubstObj --
 *
 *	Compiles a value into bytecode that performs substitution within the
 *	value, as governed by flags.
 *
 * Results:
 *	A (ByteCode *) is pointing to the resulting ByteCode.
 *
 * Side effects:
 *	The Tcl_ObjType of objPtr is changed to the "substcode" type, and the
 *	ByteCode and governing flags value are kept in the internal rep for
 *	faster operations the next time CompileSubstObj is called on the same
 *	value.
 *
 *----------------------------------------------------------------------
 */

static ByteCode *
CompileSubstObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    int flags)
{
    Interp *iPtr = (Interp *) interp;
    ByteCode *codePtr = NULL;

    ByteCodeGetInternalRep(objPtr, &substCodeType, codePtr);

    if (codePtr != NULL) {
	Namespace *nsPtr = iPtr->varFramePtr->nsPtr;

	if (flags != PTR2INT(SubstFlags(objPtr))
		|| ((Interp *) *codePtr->interpHandle != iPtr)
		|| (codePtr->compileEpoch != iPtr->compileEpoch)
		|| (codePtr->nsPtr != nsPtr)
		|| (codePtr->nsEpoch != nsPtr->resolverEpoch)
		|| (codePtr->localCachePtr !=
		iPtr->varFramePtr->localCachePtr)) {
	    Tcl_StoreInternalRep(objPtr, &substCodeType, NULL);
	    codePtr = NULL;
	}
    }
    if (codePtr == NULL) {
	CompileEnv compEnv;
	Tcl_Size numBytes;
	const char *bytes = TclGetStringFromObj(objPtr, &numBytes);

	/* TODO: Check for more TIP 280 */
	TclInitCompileEnv(interp, &compEnv, bytes, numBytes, NULL, 0);

	TclSubstCompile(interp, bytes, numBytes, flags, 1, &compEnv);

	TclEmitOpcode(		INST_DONE,			&compEnv);
	codePtr = TclInitByteCodeObj(objPtr, &substCodeType, &compEnv);
	TclFreeCompileEnv(&compEnv);

	SubstFlags(objPtr) = INT2PTR(flags);
	if (iPtr->varFramePtr->localCachePtr) {
	    codePtr->localCachePtr = iPtr->varFramePtr->localCachePtr;
	    codePtr->localCachePtr->refCount++;
	}
	TclDebugPrintByteCodeObj(objPtr);
    }
    return codePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeSubstCodeInternalRep --
 *
 *	Part of the "substcode" Tcl object type implementation. Frees the
 *	storage associated with the substcode internal representation of a
 *	Tcl_Obj unless its code is actively being executed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The substcode object's internal rep is marked invalid and its code
 *	gets freed unless the code is actively being executed. In that case
 *	the cleanup is delayed until the last execution of the code completes.
 *
 *----------------------------------------------------------------------
 */

static void
FreeSubstCodeInternalRep(
    Tcl_Obj *objPtr)		/* Object whose internal rep to free. */
{
    ByteCode *codePtr;

    ByteCodeGetInternalRep(objPtr, &substCodeType, codePtr);
    assert(codePtr != NULL);

    TclReleaseByteCode(codePtr);
}

static void
ReleaseCmdWordData(
    ExtCmdLoc *eclPtr)
{
    Tcl_Size i;

    if (eclPtr->type == TCL_LOCATION_SOURCE) {
	Tcl_DecrRefCount(eclPtr->path);
    }
    for (i=0 ; i<eclPtr->nuloc ; i++) {
	Tcl_Free(eclPtr->loc[i].line);
    }

    if (eclPtr->loc != NULL) {
	Tcl_Free(eclPtr->loc);
    }

    Tcl_Free(eclPtr);
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
    CompileEnv *envPtr,/* Points to the CompileEnv structure to
				 * initialize. */
    const char *stringPtr,	/* The source string to be compiled. */
    size_t numBytes,		/* Number of bytes in source string. */
    const CmdFrame *invoker,	/* Location context invoking the bcc */
    Tcl_Size word)			/* Index of the word in that context getting
				 * compiled */
{
    Interp *iPtr = (Interp *) interp;

    assert(tclInstructionTable[LAST_INST_OPCODE].name == NULL);

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
    envPtr->exceptAuxArrayPtr = envPtr->staticExAuxArraySpace;
    envPtr->exceptArrayNext = 0;
    envPtr->exceptArrayEnd = COMPILEENV_INIT_EXCEPT_RANGES;
    envPtr->mallocedExceptArray = 0;

    envPtr->cmdMapPtr = envPtr->staticCmdMapSpace;
    envPtr->cmdMapEnd = COMPILEENV_INIT_CMD_MAP_SIZE;
    envPtr->mallocedCmdMap = 0;
    envPtr->atCmdStart = 1;
    envPtr->expandCount = 0;

    /*
     * TIP #280: Set up the extended command location information, based on
     * the context invoking the byte code compiler. This structure is used to
     * keep the per-word line information for all compiled commands.
     *
     * See also tclBasic.c, TclEvalObjEx, for the equivalent code in the
     * non-compiling evaluator
     */

    envPtr->extCmdMapPtr = (ExtCmdLoc *)Tcl_Alloc(sizeof(ExtCmdLoc));
    envPtr->extCmdMapPtr->loc = NULL;
    envPtr->extCmdMapPtr->nloc = 0;
    envPtr->extCmdMapPtr->nuloc = 0;
    envPtr->extCmdMapPtr->path = NULL;

    if (invoker == NULL) {
	/*
	 * Initialize the compiler for relative counting in case of a
	 * dynamic context.
	 */

	envPtr->line = 1;
	if (iPtr->evalFlags & TCL_EVAL_FILE) {
	    iPtr->evalFlags &= ~TCL_EVAL_FILE;
	    envPtr->extCmdMapPtr->type = TCL_LOCATION_SOURCE;

	    if (iPtr->scriptFile) {
		/*
		 * Normalization here, to have the correct pwd. Should have
		 * negligible impact on performance, as the norm should have
		 * been done already by the 'source' invoking us, and it
		 * caches the result.
		 */

		Tcl_Obj *norm =
			Tcl_FSGetNormalizedPath(interp, iPtr->scriptFile);

		if (norm == NULL) {
		    /*
		     * Error message in the interp result. No place to put it.
		     * And no place to serve the error itself to either. Fake
		     * a path, empty string.
		     */

		    TclNewLiteralStringObj(envPtr->extCmdMapPtr->path, "");
		} else {
		    envPtr->extCmdMapPtr->path = norm;
		}
	    } else {
		TclNewLiteralStringObj(envPtr->extCmdMapPtr->path, "");
	    }

	    Tcl_IncrRefCount(envPtr->extCmdMapPtr->path);
	} else {
	    envPtr->extCmdMapPtr->type =
		(EnvIsProc(envPtr) ? TCL_LOCATION_PROC : TCL_LOCATION_BC);
	}
    } else {
	/*
	 * Initialize the compiler using the context, making counting absolute
	 * to that context. Note that the context can be byte code execution.
	 * In that case we have to fill out the missing pieces (line, path,
	 * ...) which may make change the type as well.
	 */

	CmdFrame *ctxPtr = (CmdFrame *)TclStackAlloc(interp, sizeof(CmdFrame));
	int pc = 0;

	*ctxPtr = *invoker;
	if (invoker->type == TCL_LOCATION_BC) {
	    /*
	     * Note: Type BC => ctx.data.eval.path    is not used.
	     *			ctx.data.tebc.codePtr is used instead.
	     */

	    TclGetSrcInfoForPc(ctxPtr);
	    pc = 1;
	}

	if ((ctxPtr->nline <= word) || (ctxPtr->line[word] < 0)) {
	    /*
	     * Word is not a literal, relative counting.
	     */

	    envPtr->line = 1;
	    envPtr->extCmdMapPtr->type =
		    (EnvIsProc(envPtr) ? TCL_LOCATION_PROC : TCL_LOCATION_BC);

	    if (pc && (ctxPtr->type == TCL_LOCATION_SOURCE)) {
		/*
		 * The reference made by 'TclGetSrcInfoForPc' is dead.
		 */

		Tcl_DecrRefCount(ctxPtr->data.eval.path);
	    }
	} else {
	    envPtr->line = ctxPtr->line[word];
	    envPtr->extCmdMapPtr->type = ctxPtr->type;

	    if (ctxPtr->type == TCL_LOCATION_SOURCE) {
		envPtr->extCmdMapPtr->path = ctxPtr->data.eval.path;

		if (pc) {
		    /*
		     * The reference 'TclGetSrcInfoForPc' made is transfered.
		     */

		    ctxPtr->data.eval.path = NULL;
		} else {
		    /*
		     * We have a new reference here.
		     */

		    Tcl_IncrRefCount(envPtr->extCmdMapPtr->path);
		}
	    }
	}

	TclStackFree(interp, ctxPtr);
    }

    envPtr->extCmdMapPtr->start = envPtr->line;

    /*
     * Initialize the data about invisible continuation lines as empty, i.e.
     * not used. The caller (TclSetByteCodeFromAny) will set this up, if such
     * data is available.
     */

    envPtr->clNext = NULL;

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
 *	Frees the storage allocated in a CompileEnv compilation environment
 *	structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocated storage in the CompileEnv structure is freed, although its
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
    CompileEnv *envPtr)/* Points to the CompileEnv structure. */
{
    if (envPtr->localLitTable.buckets != envPtr->localLitTable.staticBuckets){
	Tcl_Free(envPtr->localLitTable.buckets);
	envPtr->localLitTable.buckets = envPtr->localLitTable.staticBuckets;
    }
    if (envPtr->iPtr) {
	/*
	 * We never converted to Bytecode, so free the things we would
	 * have transferred to it.
	 */

	Tcl_Size i;
	LiteralEntry *entryPtr = envPtr->literalArrayPtr;
	AuxData *auxDataPtr = envPtr->auxDataArrayPtr;

	for (i = 0;  i < envPtr->literalArrayNext;  i++) {
	    TclReleaseLiteral((Tcl_Interp *)envPtr->iPtr, entryPtr->objPtr);
	    entryPtr++;
	}

#ifdef TCL_COMPILE_DEBUG
	TclVerifyGlobalLiteralTable(envPtr->iPtr);
#endif /*TCL_COMPILE_DEBUG*/

	for (i = 0;  i < envPtr->auxDataArrayNext;  i++) {
	    if (auxDataPtr->type->freeProc != NULL) {
		auxDataPtr->type->freeProc(auxDataPtr->clientData);
	    }
	    auxDataPtr++;
	}
    }
    if (envPtr->mallocedCodeArray) {
	Tcl_Free(envPtr->codeStart);
    }
    if (envPtr->mallocedLiteralArray) {
	Tcl_Free(envPtr->literalArrayPtr);
    }
    if (envPtr->mallocedExceptArray) {
	Tcl_Free(envPtr->exceptArrayPtr);
	Tcl_Free(envPtr->exceptAuxArrayPtr);
    }
    if (envPtr->mallocedCmdMap) {
	Tcl_Free(envPtr->cmdMapPtr);
    }
    if (envPtr->mallocedAuxDataArray) {
	Tcl_Free(envPtr->auxDataArrayPtr);
    }
    if (envPtr->extCmdMapPtr) {
	ReleaseCmdWordData(envPtr->extCmdMapPtr);
	envPtr->extCmdMapPtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclWordKnownAtCompileTime --
 *
 *	Determines whether the value of a token is completely known at compile
 *	time.
 *
 * Results:
 *	True if the tokenPtr argument points to a word value that is
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
 *	NB: Does *NOT* manipulate the refCount of valuePtr.
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
    Tcl_Size numComponents = tokenPtr->numComponents;
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
	TclNewObj(tempPtr);
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
		char utfBuf[4] = "";
		size_t length = TclParseBackslash(tokenPtr->start,
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
 *	Compiles a Tcl script in a string.
 *
 * Results:
 *
 *	A standard Tcl result. If an error occurs, an
 *	error message is left in the interpreter's result.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the script at runtime.
 *
 *----------------------------------------------------------------------
 */

static int
ExpandRequested(
    Tcl_Token *tokenPtr,
    Tcl_Size numWords)
{
    /* Determine whether any words of the command require expansion */
    while (numWords--) {
	if (tokenPtr->type == TCL_TOKEN_EXPAND_WORD) {
	    return 1;
	}
	tokenPtr = TokenAfter(tokenPtr);
    }
    return 0;
}

static void
CompileCmdLiteral(
    Tcl_Interp *interp,
    Tcl_Obj *cmdObj,
    CompileEnv *envPtr)
{
    Command *cmdPtr;
    int cmdLitIdx, extraLiteralFlags = LITERAL_CMD_NAME;

    cmdPtr = (Command *) Tcl_GetCommandFromObj(interp, cmdObj);
    if ((cmdPtr != NULL) && (cmdPtr->flags & CMD_VIA_RESOLVER)) {
	extraLiteralFlags |= LITERAL_UNSHARED;
    }

    cmdLitIdx = PUSH_OBJ_FLAGS(cmdObj, extraLiteralFlags);
    if (cmdPtr && TclRoutineHasName(cmdPtr)) {
	TclSetCmdNameObj(interp, TclFetchLiteral(envPtr, cmdLitIdx), cmdPtr);
    }
}

void
TclCompileInvocation(
    Tcl_Interp *interp,
    Tcl_Token *tokenPtr,
    Tcl_Obj *cmdObj,
    size_t numWords,
    CompileEnv *envPtr)
{
    DefineLineInformation;
    size_t wordIdx = 0;
    Tcl_Size depth = TclGetStackDepth(envPtr);

    if (cmdObj) {
	CompileCmdLiteral(interp, cmdObj, envPtr);
	wordIdx = 1;
	tokenPtr = TokenAfter(tokenPtr);
    }

    for (; wordIdx < numWords; wordIdx++, tokenPtr = TokenAfter(tokenPtr)) {
	int objIdx;

	SetLineInformation(wordIdx);

	if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    CompileTokens(envPtr, tokenPtr, interp);
	    continue;
	}

	objIdx = PUSH_SIMPLE_TOKEN(tokenPtr);
	if (envPtr->clNext) {
	    TclContinuationsEnterDerived(TclFetchLiteral(envPtr, objIdx),
		    tokenPtr[1].start - envPtr->source, envPtr->clNext);
	}
    }

    INVOKE4(			INVOKE_STK, wordIdx);
    TclCheckStackDepth(depth+1, envPtr);
}

static void
CompileExpanded(
    Tcl_Interp *interp,
    Tcl_Token *tokenPtr,
    Tcl_Obj *cmdObj,
    Tcl_Size numWords,
    CompileEnv *envPtr)
{
    DefineLineInformation;
    Tcl_Size wordIdx = 0;
    Tcl_Size depth = TclGetStackDepth(envPtr);

    StartExpanding(envPtr);
    if (cmdObj) {
	CompileCmdLiteral(interp, cmdObj, envPtr);
	wordIdx = 1;
	tokenPtr = TokenAfter(tokenPtr);
    }

    for (; wordIdx < numWords; wordIdx++, tokenPtr = TokenAfter(tokenPtr)) {
	int objIdx;

	SetLineInformation(wordIdx);

	if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    CompileTokens(envPtr, tokenPtr, interp);
	    if (tokenPtr->type == TCL_TOKEN_EXPAND_WORD) {
		OP4(		EXPAND_STKTOP, envPtr->currStackDepth);
	    }
	    continue;
	}

	objIdx = PUSH_SIMPLE_TOKEN(tokenPtr);
	if (envPtr->clNext) {
	    TclContinuationsEnterDerived(TclFetchLiteral(envPtr, objIdx),
		    tokenPtr[1].start - envPtr->source, envPtr->clNext);
	}
    }

    /*
     * The stack depth during argument expansion can only be managed at
     * runtime, as the number of elements in the expanded lists is not known
     * at compile time.  Adjust the stack depth estimate here so that it is
     * correct after the command with expanded arguments returns.
     *
     * The end effect of this command's invocation is that all the words of
     * the command are popped from the stack and the result is pushed: The
     * stack top changes by (1-wordIdx).
     *
     * The estimates are not correct while the command is being
     * prepared and run, INST_EXPAND_STKTOP is not stack-neutral in general.
     */

    INVOKE4(			INVOKE_EXPANDED, wordIdx);
    TclCheckStackDepth(depth + 1, envPtr);
}

static int
CompileCmdCompileProc(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Command *cmdPtr,
    CompileEnv *envPtr)
{
    DefineLineInformation;
    int unwind = 0;
    Tcl_Size incrOffset = -1;
    Tcl_Size depth = TclGetStackDepth(envPtr);

    /*
     * Emission of the INST_START_CMD instruction is controlled by the value of
     * envPtr->atCmdStart:
     *
     * atCmdStart == 2	: Don't use the INST_START_CMD instruction.
     * atCmdStart == 1	: INST_START_CMD was the last instruction emitted,
     *			: so no need to emit another.  Instead
     *			: increment the number of cmds started at it, except
     *			: for the special case at the start of a script.
     * atCmdStart == 0	: The last instruction was something else.
     *			: Emit INST_START_CMD here.
     */

    switch (envPtr->atCmdStart) {
    case 0:
	unwind = tclInstructionTable[INST_START_CMD].numBytes;
	incrOffset = CurrentOffset(envPtr) + 5;
	OP44(			START_CMD, 0, 0);
	break;
    case 1:
	if (envPtr->codeNext > envPtr->codeStart) {
	    incrOffset = CurrentOffset(envPtr) - 4;
	}
	break;
    case 2:
	/* Nothing to do */
	;
    }

    if (TCL_OK == TclAttemptCompileProc(interp, parsePtr, 1, cmdPtr, envPtr)) {
	if (incrOffset >= 0) {
	    /*
	     * Command compiled succesfully.  Increment the number of
	     * commands that start at the currently active INST_START_CMD.
	     */

	    unsigned char *incrPtr = envPtr->codeStart + incrOffset;
	    unsigned char *startPtr = incrPtr - 5;

	    TclIncrUInt4AtPtr(incrPtr, 1);
	    if (unwind) {
		/* We started the INST_START_CMD.  Record the code length. */
		TclStoreInt4AtPtr(envPtr->codeNext - startPtr, startPtr + 1);
	    }
	}
	TclCheckStackDepth(depth+1, envPtr);
	return TCL_OK;
    }

    envPtr->codeNext -= unwind; /* Unwind INST_START_CMD */

    /*
     * Throw out any line information generated by the failed compile attempt.
     * Reset the index of next command.  Toss out any from failed nested
     * partial compiles.
     */

    ClearFailedCompile(envPtr);
    return TCL_ERROR;
}

void
TclClearFailedCompile(
    CompileEnv *envPtr,
    LineInformation *lineInfoPtr)
{
    /*
     * Throw out any line information generated by the failed compile attempt.
     */

    while (lineInfoPtr->mapPtr->nuloc - 1 > lineInfoPtr->eclIndex) {
	ECL *eclPtr = &lineInfoPtr->mapPtr->loc[--lineInfoPtr->mapPtr->nuloc];
	Tcl_Free(eclPtr->line);
	eclPtr->line = NULL;
    }

    /*
     * Reset the index of next command.  Toss out any from failed nested
     * partial compiles.
     */

    envPtr->numCommands = lineInfoPtr->mapPtr->nuloc;
}

static Tcl_Size
CompileCommandTokens(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    CompileEnv *envPtr)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    ExtCmdLoc *eclPtr = envPtr->extCmdMapPtr;
    Tcl_Obj *cmdObj;
    Command *cmdPtr = NULL;
    int code = TCL_ERROR, expand = -1;
    int cmdKnown;
    int *wlines;
    Tcl_Size wlineat, numWords = parsePtr->numWords;
    int cmdLine = envPtr->line;
    Tcl_Size *clNext = envPtr->clNext;
    Tcl_Size cmdIdx = envPtr->numCommands;
    Tcl_Size startCodeOffset = CurrentOffset(envPtr);
    Tcl_Size depth = TclGetStackDepth(envPtr);

    assert (numWords > 0);

    /* Precompile */

    TclNewObj(cmdObj);
    envPtr->numCommands++;
    EnterCmdStartData(envPtr, cmdIdx,
	    parsePtr->commandStart - envPtr->source, startCodeOffset);

    /*
     * TIP #280. Scan the words and compute the extended location information.
     * At first the map first contains full per-word line information for use
     * by the compiler. This is later replaced by a reduced form which signals
     * non-literal words, stored in 'wlines'.
     */

    EnterCmdWordData(eclPtr, parsePtr->commandStart - envPtr->source,
	    parsePtr->tokenPtr, parsePtr->commandStart, numWords, cmdLine,
	    clNext, &wlines, envPtr);
    wlineat = eclPtr->nuloc - 1;

    envPtr->line = eclPtr->loc[wlineat].line[0];
    envPtr->clNext = eclPtr->loc[wlineat].next[0];

    /* Do we know the command word? */
    Tcl_IncrRefCount(cmdObj);
    tokenPtr = parsePtr->tokenPtr;
    cmdKnown = TclWordKnownAtCompileTime(tokenPtr, cmdObj);

    /* Is this a command we should (try to) compile with a compileProc ? */
    if (cmdKnown && !(iPtr->flags & DONT_COMPILE_CMDS_INLINE)) {
	cmdPtr = (Command *) Tcl_GetCommandFromObj(interp, cmdObj);
	if (cmdPtr) {
	    /*
	     * Found a command.  Test the ways we can be told not to attempt
	     * to compile it.
	     */
	    if ((cmdPtr->compileProc == NULL)
		    || (cmdPtr->nsPtr->flags & NS_SUPPRESS_COMPILATION)
		    || (cmdPtr->flags & CMD_HAS_EXEC_TRACES)) {
		cmdPtr = NULL;
	    }
	}
	if (cmdPtr && !(cmdPtr->flags & CMD_COMPILES_EXPANDED)) {
	    expand = ExpandRequested(parsePtr->tokenPtr, numWords);
	    if (expand) {
		/* We need to expand, but compileProc cannot. */
		cmdPtr = NULL;
	    }
	}
    }

    /* If cmdPtr != NULL, try to call cmdPtr->compileProc */
    if (cmdPtr) {
	code = CompileCmdCompileProc(interp, parsePtr, cmdPtr, envPtr);
    }

    if (code == TCL_ERROR) {
	/*
	 * We might have a failure to compile an expansion-aware command. If
	 * that's happened, expand will still be -1 and should be determined
	 * to be its true value now.
	 */
	if (expand < 0) {
	    expand = ExpandRequested(parsePtr->tokenPtr, numWords);
	}

	if (expand) {
	    CompileExpanded(interp, parsePtr->tokenPtr,
		    cmdKnown ? cmdObj : NULL, numWords, envPtr);
	} else {
	    TclCompileInvocation(interp, parsePtr->tokenPtr,
		    cmdKnown ? cmdObj : NULL, numWords, envPtr);
	}
    }

    Tcl_DecrRefCount(cmdObj);

    OP(				POP);
    EnterCmdExtentData(envPtr, cmdIdx,
	    parsePtr->term - parsePtr->commandStart,
	    CurrentOffset(envPtr) - startCodeOffset);

    /*
     * TIP #280: Free the full form of per-word line data and insert the
     * reduced form now.
     */

    envPtr->line = cmdLine;
    envPtr->clNext = clNext;
    Tcl_Free(eclPtr->loc[wlineat].line);
    Tcl_Free(eclPtr->loc[wlineat].next);
    eclPtr->loc[wlineat].line = wlines;
    eclPtr->loc[wlineat].next = NULL;

    TclCheckStackDepth(depth, envPtr);
    return cmdIdx;
}

void
TclCompileScript(
    Tcl_Interp *interp,		/* Used for error and status reporting. Also
				 * serves as context for finding and compiling
				 * commands. May not be NULL. */
    const char *script,		/* The source script to compile. */
    Tcl_Size numBytes,		/* Number of bytes in script. If < 0, the
				 * script consists of all bytes up to the
				 * first null character. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Size lastCmdIdx = TCL_INDEX_NONE;
				/* Index into envPtr->cmdMapPtr of the last
				 * command this routine compiles into bytecode.
				 * Initial value of TCL_INDEX_NONE indicates
				 * this routine has not yet generated any
				 * bytecode. */
    const char *p = script;	/* Where we are in our compile. */
    Tcl_Size depth = TclGetStackDepth(envPtr);
    Interp *iPtr = (Interp *) interp;

    if (envPtr->iPtr == NULL) {
	Tcl_Panic("TclCompileScript() called on uninitialized CompileEnv");
    }
    /*
     * Check depth to avoid overflow of the C execution stack by too many
     * nested calls of TclCompileScript, considering interp recursionlimit.
     * Use factor 5/4 (1.25) to avoid being too mistaken when recognizing the
     * limit during "mixed" evaluation and compilation process (nested
     * eval+compile) and is good enough for default recursionlimit (1000).
     */
    if (iPtr->numLevels / 5 > iPtr->maxNestingDepth / 4) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"too many nested compilations (infinite loop?)",
		TCL_AUTO_LENGTH));
	Tcl_SetErrorCode(interp, "TCL", "LIMIT", "STACK", (char *)NULL);
	TclCompileSyntaxError(interp, envPtr);
	return;
    }

    if (numBytes < 0) {
	numBytes = strlen(script);
    }

    /* Each iteration compiles one command from the script. */

    if (numBytes > 0) {
	if (numBytes >= INT_MAX) {
	    /*
	     * Note this gets -errorline as 1. Not worth figuring out which line
	     * crosses the limit to get -errorline for this error case.
	     */
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "Script length %" TCL_SIZE_MODIFIER
		    "d exceeds max permitted length %d.",
		    numBytes, INT_MAX - 1));
	    Tcl_SetErrorCode(interp, "TCL", "LIMIT", "SCRIPTLENGTH", (char *)NULL);
	    TclCompileSyntaxError(interp, envPtr);
	    return;
	}
	/*
	 * Don't use system stack (size of Tcl_Parse is ca. 400 bytes), so
	 * many nested compilations (body enclosed in body) can cause abnormal
	 * program termination with a stack overflow exception, bug [fec0c17d39].
	 */
	Tcl_Parse *parsePtr = (Tcl_Parse *)Tcl_Alloc(sizeof(Tcl_Parse));

	do {
	    const char *next;

	    if (TCL_OK != Tcl_ParseCommand(interp, p, numBytes, 0, parsePtr)) {
		/*
		* Compile bytecodes to report the parsePtr error at runtime.
		*/

		Tcl_LogCommandInfo(interp, script, parsePtr->commandStart,
			parsePtr->term + 1 - parsePtr->commandStart);
		TclCompileSyntaxError(interp, envPtr);
		Tcl_Free(parsePtr);
		return;
	    }

#ifdef TCL_COMPILE_DEBUG
	    /*
	     * If tracing, print a line for each top level command compiled.
	     * TODO: Suppress when numWords == 0 ?
	     */

	    if ((tclTraceCompile >= TCL_TRACE_BYTECODE_COMPILE_SUMMARY)
		    && !EnvIsProc(envPtr)) {
		int commandLength = parsePtr->term - parsePtr->commandStart;
		fprintf(stdout, "  Compiling: ");
		TclPrintSource(stdout, parsePtr->commandStart,
			TclMin(commandLength, 55));
		fprintf(stdout, "\n");
	    }
#endif

	    /*
	     * TIP #280: Count newlines before the command start.
	     * (See test info-30.33).
	     */

	    TclAdvanceLines(&envPtr->line, p, parsePtr->commandStart);
	    TclAdvanceContinuations(&envPtr->line, &envPtr->clNext,
		    parsePtr->commandStart - envPtr->source);

	    /*
	     * Advance parser to the next command in the script.
	     */

	    next = parsePtr->commandStart + parsePtr->commandSize;
	    numBytes -= next - p;
	    p = next;

	    if (parsePtr->numWords == 0) {
		/*
		 * The "command" parsed has no words.  In this case we can skip
		 * the rest of the loop body.  With no words, clearly
		 * CompileCommandTokens() has nothing to do.  Since the parser
		 * aggressively sucks up leading comment and white space,
		 * including newlines, parsePtr->commandStart must be pointing at
		 * either the end of script, or a command-terminating semi-colon.
		 * In either case, the TclAdvance*() calls have nothing to do.
		 * Finally, when no words are parsed, no tokens have been
		 * allocated at parsePtr->tokenPtr so there's also nothing for
		 * Tcl_FreeParse() to do.
		 *
		 * The advantage of this shortcut is that CompileCommandTokens()
		 * can be written with an assumption that parsePtr->numWords > 0, with
		 * the implication the CCT() always generates bytecode.
		 */
		continue;
	    }

	    /*
	     * Avoid stack exhaustion by too many nested calls of TclCompileScript
	     * (considering interp recursionlimit).
	     */
	    iPtr->numLevels++;

	    lastCmdIdx = CompileCommandTokens(interp, parsePtr, envPtr);

	    iPtr->numLevels--;

	    /*
	     * TIP #280: Track lines in the just compiled command.
	     */

	    TclAdvanceLines(&envPtr->line, parsePtr->commandStart, p);
	    TclAdvanceContinuations(&envPtr->line, &envPtr->clNext,
		    p - envPtr->source);
	    Tcl_FreeParse(parsePtr);
	} while (numBytes > 0);

	Tcl_Free(parsePtr);
    }

    if (lastCmdIdx == TCL_INDEX_NONE) {
	/*
	 * Compiling the script yielded no bytecode.  The script must be all
	 * whitespace, comments, and empty commands.  Such scripts are defined
	 * to successfully produce the empty string result, so we emit the
	 * simple bytecode that makes that happen.
	 */

	PUSH(			"");
    } else {
	/*
	 * We compiled at least one command to bytecode.  The routine
	 * CompileCommandTokens() follows the bytecode of each compiled
	 * command with an INST_POP, so that stack balance is maintained when
	 * several commands are in sequence.  (The result of each command is
	 * thrown away before moving on to the next command).  For the last
	 * command compiled, we need to undo that INST_POP so that the result
	 * of the last command becomes the result of the script.  The code
	 * here removes that trailing INST_POP.
	 */

	envPtr->cmdMapPtr[lastCmdIdx].numCodeBytes--;
	envPtr->codeNext--;
	envPtr->currStackDepth++;
    }
    TclCheckStackDepth(depth+1, envPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileTokens --
 *
 *	Given an array of tokens parsed from a Tcl command, e.g. the tokens
 *	that make up a word, emits instructions to evaluate the
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
    Tcl_Size i, nameBytes = tokenPtr[1].size;
    Tcl_Size localVar;
    int localVarName = 1;

    /*
     * Determine how the variable name should be handled: if it contains any
     * namespace qualifiers it is not a local variable (localVarName=-1); if
     * it looks like an array element and the token has a single component, it
     * should not be created here [Bug 569438] (localVarName=0); otherwise,
     * the local variable can safely be created (localVarName=1).
     */

    for (i = 0, p = name;  i < nameBytes;  i++, p++) {
	if ((p[0] == ':') && (i < nameBytes-1) && (p[1] == ':')) {
	    localVarName = -1;
	    break;
	} else if ((p[0] == '(')
		&& (tokenPtr->numComponents == 1)
		&& (name[nameBytes - 1] == ')')) {
	    localVarName = 0;
	    break;
	}
    }

    /*
     * Either push the variable's name, or find its index in the array
     * of local variables in a procedure frame.
     */

    localVar = TCL_INDEX_NONE;
    if (localVarName != -1) {
	localVar = TclFindCompiledLocal(name, nameBytes, localVarName, envPtr);
    }
    if (localVar < 0) {
	PushLiteral(envPtr, name, nameBytes);
    }

    /*
     * Emit instructions to load the variable.
     */

    TclAdvanceLines(&envPtr->line, tokenPtr[1].start,
	    tokenPtr[1].start + tokenPtr[1].size);

    if (tokenPtr->numComponents == 1) {
	if (localVar < 0) {
	    OP(			LOAD_STK);
	} else {
	    OP4(		LOAD_SCALAR, localVar);
	}
    } else {
	TclCompileTokens(interp, tokenPtr+2, tokenPtr->numComponents-1, envPtr);
	if (localVar < 0) {
	    OP(			LOAD_ARRAY_STK);
	} else {
	    OP4(		LOAD_ARRAY, localVar);
	}
    }
}

void
TclCompileTokens(
    Tcl_Interp *interp,		/* Used for error and status reporting. */
    Tcl_Token *tokenPtr,	/* Pointer to first in an array of tokens to
				 * compile. */
    Tcl_Size count,		/* Number of tokens to consider at tokenPtr.
				 * Must be at least 1. */
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    Tcl_DString textBuffer;	/* Holds concatenated chars from adjacent
				 * TCL_TOKEN_TEXT, TCL_TOKEN_BS tokens. */
    char buffer[4] = "";
    Tcl_Size i, numObjsToConcat, adjust;
    Tcl_Size length;
    unsigned char *entryCodeNext = envPtr->codeNext;
#define NUM_STATIC_POS 20
    int isLiteral;
    Tcl_Size maxNumCL, numCL;
    Tcl_Size *clPosition = NULL;
    Tcl_Size depth = TclGetStackDepth(envPtr);

    /*
     * If this is actually a literal, handle continuation lines by
     * preallocating a small table to store the locations of any continuation
     * lines found in this literal.  The table is extended if needed.
     *
     * Note: In contrast with the analagous code in 'TclSubstTokens()' the
     * 'adjust' variable seems unneeded here.  The code which merges
     * continuation line information of multiple words which concat'd at
     * runtime also seems unneeded. Either that or I have not managed to find a
     * test case for these two possibilities yet.  It might be a difference
     * between compile- versus run-time processing.
     */

    numCL = 0;
    maxNumCL = 0;
    isLiteral = 1;
    for (i=0 ; i < count; i++) {
	if ((tokenPtr[i].type != TCL_TOKEN_TEXT)
		&& (tokenPtr[i].type != TCL_TOKEN_BS)) {
	    isLiteral = 0;
	    break;
	}
    }

    if (isLiteral) {
	maxNumCL = NUM_STATIC_POS;
	clPosition = (Tcl_Size *)Tcl_Alloc(maxNumCL * sizeof(Tcl_Size));
    }

    adjust = 0;
    Tcl_DStringInit(&textBuffer);
    numObjsToConcat = 0;
    for ( ;  count > 0;  count--, tokenPtr++) {
	switch (tokenPtr->type) {
	case TCL_TOKEN_TEXT:
	    TclDStringAppendToken(&textBuffer, tokenPtr);
	    TclAdvanceLines(&envPtr->line, tokenPtr->start,
		    tokenPtr->start + tokenPtr->size);
	    break;

	case TCL_TOKEN_BS:
	    length = TclParseBackslash(tokenPtr->start, tokenPtr->size,
		    NULL, buffer);
	    Tcl_DStringAppend(&textBuffer, buffer, length);

	    /*
	     * If the identified backslash sequence is in a literal and
	     * represented a continuation line, compute and store its
	     * location (as char offset to the beginning of the _result_
	     * script). We may have to extend the table of locations.
	     *
	     * The continuation line information is relevant even if the word
	     * being processed is not a literal, as it can affect nested
	     * commands. See the branch below for TCL_TOKEN_COMMAND, where the
	     * adjustment being tracked here is taken into account. The good
	     * thing is a table of everything is not needed, just the number of
	     * lines to add as correction.
	     */

	    if ((length == 1) && (buffer[0] == ' ') &&
		    (tokenPtr->start[1] == '\n')) {
		if (isLiteral) {
		    Tcl_Size clPos = Tcl_DStringLength(&textBuffer);

		    if (numCL >= maxNumCL) {
			maxNumCL *= 2;
			clPosition = (Tcl_Size *)Tcl_Realloc(clPosition,
				maxNumCL * sizeof(Tcl_Size));
		    }
		    clPosition[numCL] = clPos;
		    numCL ++;
		}
		adjust++;
	    }
	    break;

	case TCL_TOKEN_COMMAND:
	    /*
	     * Push any accumulated chars appearing before the command.
	     */

	    if (Tcl_DStringLength(&textBuffer) > 0) {
		int literal = TclPushDString(envPtr, &textBuffer);

		numObjsToConcat++;
		Tcl_DStringFree(&textBuffer);
		if (numCL) {
		    TclContinuationsEnter(TclFetchLiteral(envPtr, literal),
			    numCL, clPosition);
		}
		numCL = 0;
	    }

	    envPtr->line += adjust;
	    TclCompileScript(interp, tokenPtr->start+1,
		    tokenPtr->size-2, envPtr);
	    envPtr->line -= adjust;
	    numObjsToConcat++;
	    break;

	case TCL_TOKEN_VARIABLE:
	    /*
	     * Push any accumulated chars appearing before the $<var>.
	     */

	    if (Tcl_DStringLength(&textBuffer) > 0) {
		TclPushDString(envPtr, &textBuffer);
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
		    tokenPtr->type, (int)tokenPtr->size, tokenPtr->start);
	}
    }

    /*
     * Push any accumulated characters appearing at the end.
     */

    if (Tcl_DStringLength(&textBuffer) > 0) {
	int literal = TclPushDString(envPtr, &textBuffer);

	numObjsToConcat++;
	if (numCL) {
	    TclContinuationsEnter(TclFetchLiteral(envPtr, literal),
		    numCL, clPosition);
	}
	numCL = 0;
    }

    /*
     * If necessary, concatenate the parts of the word.
     */

    while (numObjsToConcat > 255) {
	OP1(			STR_CONCAT1, 255);
	numObjsToConcat -= 254;	/* concat pushes 1 obj, the result */
    }
    if (numObjsToConcat > 1) {
	OP1(			STR_CONCAT1, numObjsToConcat);
    }

    /*
     * If the tokens yielded no instructions, push an empty string.
     */

    if (envPtr->codeNext == entryCodeNext) {
	PUSH(			"");
    }
    Tcl_DStringFree(&textBuffer);

    /*
     * Release the temp table we used to collect the locations of continuation
     * lines, if any.
     */

    if (maxNumCL) {
	Tcl_Free(clPosition);
    }
    TclCheckStackDepth(depth+1, envPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileCmdWord --
 *
 *	Given an array of parse tokens for a word containing one or more Tcl
 *	commands, emits inline instructions to execute them.  In contrast with
 *	TclCompileTokens, a simple word such as a loop body enclosed in braces
 *	is not just pushed as a string, but is itself parsed into tokens and
 *	compiled.
 *
 * Results:
 *	A standard Tcl result. If an error occurs, an
 *	error message is left in the interpreter's result.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the tokens at runtime.
 *
 *----------------------------------------------------------------------
 */

void
TclCompileCmdWord(
    Tcl_Interp *interp,		/* Used for error and status reporting. */
    Tcl_Token *tokenPtr,	/* Pointer to first in an array of tokens for
				 * a command word to compile inline. */
    Tcl_Size count,		/* Number of tokens to consider at tokenPtr.
				 * Must be at least 1. */
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{

    if ((count == 1) && (tokenPtr->type == TCL_TOKEN_TEXT)) {
	/*
	 * The common case that there is a single text token. Compile it
	 * into an inline sequence of instructions.
	 */

	TclCompileScript(interp, tokenPtr->start, tokenPtr->size, envPtr);
    } else {
	/*
	 * Either there are multiple tokens, or the single token involves
	 * substitutions. Emit instructions to invoke the eval command
	 * procedure at runtime on the result of evaluating the tokens.
	 */

	TclCompileTokens(interp, tokenPtr, count, envPtr);
	INVOKE(			EVAL_STK);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileExprWords --
 *
 *	Given an array of parse tokens representing one or more words that
 *	contain a Tcl expression, emits inline instructions to execute the
 *	expression. In contrast with TclCompileExpr, supports Tcl's two-level
 *	substitution semantics for an expression that appears as command words.
 *
 * Results:
 *	A standard Tcl result. If an error occurs, an
 *	error message is left in the interpreter's result.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the expression.
 *
 *----------------------------------------------------------------------
 */

void
TclCompileExprWords(
    Tcl_Interp *interp,		/* Used for error and status reporting. */
    Tcl_Token *tokenPtr,	/* Points to first in an array of word tokens
				 * for the expression to compile inline. */
    size_t numWords,		/* Number of word tokens starting at tokenPtr.
				 * Must be at least 1. Each word token
				 * contains one or more subtokens. */
    CompileEnv *envPtr)		/* Holds the resulting instructions. */
{
    Tcl_Token *wordPtr;
    size_t i, concatItems;

    /*
     * If the expression is a single word that doesn't require substitutions,
     * just compile its string into inline instructions.
     */

    if ((numWords == 1) && (tokenPtr->type == TCL_TOKEN_SIMPLE_WORD)) {
	TclCompileExpr(interp, tokenPtr[1].start,tokenPtr[1].size, envPtr, 1);
	return;
    }

    /*
     * Emit code to call the expr command proc at runtime. Concatenate the
     * (already substituted once) expr tokens with a space between each.
     */

    wordPtr = tokenPtr;
    for (i = 0;  i < numWords;  i++) {
	CompileTokens(envPtr, wordPtr, interp);
	if (i + 1 < numWords) {
	    PUSH(		" ");
	}
	wordPtr += wordPtr->numComponents + 1;
    }
    concatItems = 2*numWords - 1;
    while (concatItems > 255) {
	OP1(			STR_CONCAT1, 255);
	concatItems -= 254;
    }
    if (concatItems > 1) {
	OP1(			STR_CONCAT1, concatItems);
    }
    OP(				EXPR_STK);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileNoOp --
 *
 *	Compiles no-op's
 *
 * Results:
 *	TCL_OK if completion was successful.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute a no-op at runtime. No
 *	result is pushed onto the stack: the compiler has to take care of this
 *	itself if the last compiled command is a NoOp.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileNoOp(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    TCL_UNUSED(Command *),
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Token *tokenPtr;
    Tcl_Size i;

    tokenPtr = parsePtr->tokenPtr;
    for (i = 1; i < parsePtr->numWords; i++) {
	tokenPtr = tokenPtr + tokenPtr->numComponents + 1;

	if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    CompileTokens(envPtr, tokenPtr, interp);
	    OP(			POP);
	}
    }
    PUSH(			"");
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitByteCodeObj --
 *
 *	Creates a ByteCode structure and initializes it from a CompileEnv
 *	compilation environment structure. The ByteCode structure is smaller
 *	and contains just that information needed to execute the bytecode
 *	instructions resulting from compiling a Tcl script. The resulting
 *	structure is placed in the specified object.
 *
 * Results:
 *	A newly-constructed ByteCode object is stored in the internal
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

static void
PreventCycle(
    Tcl_Obj *objPtr,
    CompileEnv *envPtr)
{
    Tcl_Size i;

    for (i = 0;  i < envPtr->literalArrayNext; i++) {
	if (objPtr == TclFetchLiteral(envPtr, i)) {
	    /*
	     * Prevent circular reference where the bytecode internalrep of
	     * a value contains a literal which is that same value.
	     * If this is allowed to happen, refcount decrements may not
	     * reach zero, and memory may leak.  Bugs 467523, 3357771
	     *
	     * NOTE:  [Bugs 3392070, 3389764] We make a copy based completely
	     * on the string value, and do not call Tcl_DuplicateObj() so we
	     * can be sure we do not have any lingering cycles hiding in
	     * the internalrep.
	     */
	    Tcl_Size numBytes;
	    const char *bytes = TclGetStringFromObj(objPtr, &numBytes);
	    Tcl_Obj *copyPtr = Tcl_NewStringObj(bytes, numBytes);

	    Tcl_IncrRefCount(copyPtr);
	    TclReleaseLiteral((Tcl_Interp *)envPtr->iPtr, objPtr);

	    envPtr->literalArrayPtr[i].objPtr = copyPtr;
	}
    }
}

ByteCode *
TclInitByteCode(
    CompileEnv *envPtr)		/* Points to the CompileEnv structure from
				 * which to create a ByteCode structure. */
{
    ByteCode *codePtr;
    size_t codeBytes, objArrayBytes, exceptArrayBytes, cmdLocBytes;
    size_t auxDataArrayBytes, structureSize;
    unsigned char *p;
#ifdef TCL_COMPILE_DEBUG
    unsigned char *nextPtr;
#endif
    Tcl_Size i, numLitObjects = envPtr->literalArrayNext;
    Namespace *namespacePtr;
    Interp *iPtr;

    if (envPtr->iPtr == NULL) {
	Tcl_Panic("TclInitByteCodeObj() called on uninitialized CompileEnv");
    }

    iPtr = envPtr->iPtr;

    codeBytes = CurrentOffset(envPtr);
    objArrayBytes = envPtr->literalArrayNext * sizeof(Tcl_Obj *);
    exceptArrayBytes = envPtr->exceptArrayNext * sizeof(ExceptionRange);
    auxDataArrayBytes = envPtr->auxDataArrayNext * sizeof(AuxData);
    cmdLocBytes = GetCmdLocEncodingSize(envPtr);

    /*
     * Compute the total number of bytes needed for this bytecode.
     *
     * Note that code bytes need not be aligned but since later elements are we
     * need to pad anyway, either directly after ByteCode or after codeBytes,
     * and it's easier and more consistent to do the former.
     */

    structureSize = TCL_ALIGN(sizeof(ByteCode));  /* align code bytes */
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

    p = (unsigned char *)Tcl_Alloc(structureSize);
    codePtr = (ByteCode *) p;
    codePtr->interpHandle = TclHandlePreserve(iPtr->handle);
    codePtr->compileEpoch = iPtr->compileEpoch;
    codePtr->nsPtr = namespacePtr;
    codePtr->nsEpoch = namespacePtr->resolverEpoch;
    codePtr->refCount = 0;
    TclPreserveByteCode(codePtr);
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

    p += TCL_ALIGN(sizeof(ByteCode));	/* align code bytes */
    codePtr->codeStart = p;
    memcpy(p, envPtr->codeStart, codeBytes);

    p += TCL_ALIGN(codeBytes);		/* align object array */
    codePtr->objArrayPtr = (Tcl_Obj **) p;
    for (i = 0;  i < numLitObjects;  i++) {
	codePtr->objArrayPtr[i] = TclFetchLiteral(envPtr, i);
    }

    p += TCL_ALIGN(objArrayBytes);	/* align exception range array */
    if (exceptArrayBytes > 0) {
	codePtr->exceptArrayPtr = (ExceptionRange *) p;
	memcpy(p, envPtr->exceptArrayPtr, exceptArrayBytes);
    } else {
	codePtr->exceptArrayPtr = NULL;
    }

    p += TCL_ALIGN(exceptArrayBytes);	/* align AuxData array */
    if (auxDataArrayBytes > 0) {
	codePtr->auxDataArrayPtr = (AuxData *) p;
	memcpy(p, envPtr->auxDataArrayPtr, auxDataArrayBytes);
    } else {
	codePtr->auxDataArrayPtr = NULL;
    }

    p += auxDataArrayBytes;
#ifndef TCL_COMPILE_DEBUG
    EncodeCmdLocMap(envPtr, codePtr, (unsigned char *) p);
#else
    nextPtr = EncodeCmdLocMap(envPtr, codePtr, (unsigned char *) p);
    if (((size_t)(nextPtr - p)) != cmdLocBytes) {
	Tcl_Panic("TclInitByteCodeObj: "
		"encoded cmd location bytes %lu != expected size %lu",
		(unsigned long)(nextPtr - p), (unsigned long)cmdLocBytes);
    }
#endif

    /*
     * Record various compilation-related statistics about the new ByteCode
     * structure. Don't include overhead for statistics-related fields.
     */

#ifdef TCL_COMPILE_STATS
    codePtr->structureSize = structureSize
	    - (sizeof(size_t) + sizeof(Tcl_Time));
    Tcl_GetTime(&codePtr->createTime);

    RecordByteCodeStats(codePtr);
#endif /* TCL_COMPILE_STATS */

    /*
     * TIP #280. Associate the extended per-word line information with the
     * byte code object (internal rep), for use with the bc compiler.
     */

    Tcl_SetHashValue(Tcl_CreateHashEntry(iPtr->lineBCPtr, codePtr,
	    NULL), envPtr->extCmdMapPtr);
    envPtr->extCmdMapPtr = NULL;

    /* We've used up the CompileEnv.  Mark as uninitialized. */
    envPtr->iPtr = NULL;

    codePtr->localCachePtr = NULL;
    return codePtr;
}

ByteCode *
TclInitByteCodeObj(
    Tcl_Obj *objPtr,		/* Points object that should be initialized,
				 * and whose string rep contains the source
				 * code. */
    const Tcl_ObjType *typePtr,
    CompileEnv *envPtr)		/* Points to the CompileEnv structure from
				 * which to create a ByteCode structure. */
{
    ByteCode *codePtr;

    PreventCycle(objPtr, envPtr);

    codePtr = TclInitByteCode(envPtr);

    /*
     * Free the old internal rep then convert the object to a bytecode object
     * by making its internal rep point to the just compiled ByteCode.
     */

    ByteCodeSetInternalRep(objPtr, typePtr, codePtr);
    return codePtr;
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

Tcl_LVTIndex
TclFindCompiledLocal(
    const char *name,		/* Points to first character of the name of a
				 * scalar or array variable. If NULL, a
				 * temporary var should be created. */
    Tcl_Size nameBytes,		/* Number of bytes in the name. */
    int create,			/* If 1, allocate a local frame entry for the
				 * variable if it is new. */
    CompileEnv *envPtr)		/* Points to the current compile environment*/
{
    CompiledLocal *localPtr;
    Tcl_Size localVar = TCL_INDEX_NONE;
    Tcl_Size i;
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
	Tcl_Size len;

	if (!cachePtr || !name) {
	    return TCL_INDEX_NONE;
	}

	varNamePtr = &cachePtr->varName0;
	for (i=0; i < cachePtr->numVars; varNamePtr++, i++) {
	    if (*varNamePtr) {
		localName = TclGetStringFromObj(*varNamePtr, &len);
		if ((len == nameBytes) && !strncmp(name, localName, len)) {
		    return i;
		}
	    }
	}
	return TCL_INDEX_NONE;
    }

    if (name != NULL) {
	Tcl_Size localCt = procPtr->numCompiledLocals;

	localPtr = procPtr->firstLocalPtr;
	for (i = 0;  i < localCt;  i++) {
	    if (!TclIsVarTemporary(localPtr)) {
		char *localName = localPtr->name;

		if ((nameBytes == localPtr->nameLength) &&
			(strncmp(name,localName,nameBytes) == 0)) {
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
	localPtr = (CompiledLocal *)Tcl_Alloc(
		offsetof(CompiledLocal, name) + 1U + nameBytes);
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
	    memcpy(localPtr->name, name, nameBytes);
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
 *	Uses malloc to allocate more storage for a CompileEnv's code array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size of the bytecode array is doubled. If envPtr->mallocedCodeArray
 *	is non-zero the old array is freed. Byte codes are copied from the old
 *	array to the new one.
 *
 *----------------------------------------------------------------------
 */

void
TclExpandCodeArray(
    void *envArgPtr)		/* Points to the CompileEnv whose code array
				 * must be enlarged. */
{
    CompileEnv *envPtr = (CompileEnv *)envArgPtr;
				/* The CompileEnv containing the code array to
				 * be doubled in size. */

    /*
     * envPtr->codeNext is equal to envPtr->codeEnd. The currently defined
     * code bytes are stored between envPtr->codeStart and envPtr->codeNext-1
     * [inclusive].
     */

    size_t currBytes = CurrentOffset(envPtr);
    size_t newBytes = 2 * (envPtr->codeEnd - envPtr->codeStart);

    if (envPtr->mallocedCodeArray) {
	envPtr->codeStart = (unsigned char *)Tcl_Realloc(envPtr->codeStart, newBytes);
    } else {
	/*
	 * envPtr->exceptArrayPtr isn't a Tcl_Alloc'd pointer, so
	 * perform the equivalent of Tcl_Realloc directly.
	 */

	unsigned char *newPtr = (unsigned char *)Tcl_Alloc(newBytes);

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
    Tcl_Size cmdIndex,		/* Index of the command whose start data is
				 * being set. */
    Tcl_Size srcOffset,		/* Offset of first char of the command. */
    Tcl_Size codeOffset)	/* Offset of first byte of command code. */
{
    CmdLocation *cmdLocPtr;

    if (cmdIndex < 0 || cmdIndex >= envPtr->numCommands) {
	Tcl_Panic("EnterCmdStartData: bad command index %" TCL_Z_MODIFIER "u",
		cmdIndex);
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
	    envPtr->cmdMapPtr = (CmdLocation *)Tcl_Realloc(envPtr->cmdMapPtr,
		    newBytes);
	} else {
	    /*
	     * envPtr->cmdMapPtr isn't a Tcl_Alloc'd pointer, so we must code a
	     * Tcl_Realloc equivalent for ourselves.
	     */

	    CmdLocation *newPtr = (CmdLocation *)Tcl_Alloc(newBytes);

	    memcpy(newPtr, envPtr->cmdMapPtr, currBytes);
	    envPtr->cmdMapPtr = newPtr;
	    envPtr->mallocedCmdMap = 1;
	}
	envPtr->cmdMapEnd = newElems;
    }

    if (cmdIndex > 0) {
	if (codeOffset < envPtr->cmdMapPtr[cmdIndex - 1].codeOffset) {
	    Tcl_Panic("EnterCmdStartData: cmd map not sorted by code offset");
	}
    }

    cmdLocPtr = &envPtr->cmdMapPtr[cmdIndex];
    cmdLocPtr->codeOffset = codeOffset;
    cmdLocPtr->srcOffset = srcOffset;
    cmdLocPtr->numSrcBytes = TCL_INDEX_NONE;
    cmdLocPtr->numCodeBytes = TCL_INDEX_NONE;
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
    Tcl_Size cmdIndex,		/* Index of the command whose source and code
				 * length data is being set. */
    Tcl_Size numSrcBytes,	/* Number of command source chars. */
    Tcl_Size numCodeBytes)	/* Offset of last byte of command code. */
{
    CmdLocation *cmdLocPtr;

    if (cmdIndex < 0 || cmdIndex >= envPtr->numCommands) {
	Tcl_Panic("EnterCmdExtentData: bad command index %" TCL_Z_MODIFIER "u",
		cmdIndex);
    }

    if (cmdIndex > envPtr->cmdMapEnd) {
	Tcl_Panic("EnterCmdExtentData: "
		"missing start data for command %" TCL_Z_MODIFIER "u",
		cmdIndex);
    }

    cmdLocPtr = &envPtr->cmdMapPtr[cmdIndex];
    cmdLocPtr->numSrcBytes = numSrcBytes;
    cmdLocPtr->numCodeBytes = numCodeBytes;
}

/*
 *----------------------------------------------------------------------
 * TIP #280
 *
 * EnterCmdWordData --
 *
 *	Registers the lines for the words of a command. This information is
 *	used at runtime by 'info frame'.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Inserts word location information into the compilation environment
 *	envPtr for the command at index cmdIndex. The compilation
 *	environment's ExtCmdLoc.ECL array is grown if necessary.
 *
 *----------------------------------------------------------------------
 */

static void
EnterCmdWordData(
    ExtCmdLoc *eclPtr,		/* Points to the map environment structure in
				 * which to enter command location
				 * information. */
    Tcl_Size srcOffset,		/* Offset of first char of the command. */
    Tcl_Token *tokenPtr,
    const char *cmd,
    Tcl_Size numWords,
    int line,
    Tcl_Size *clNext,
    int **wlines,
    CompileEnv *envPtr)
{
    ECL *ePtr;
    const char *last;
    Tcl_Size wordIdx;
    int wordLine;
    int *wwlines;
    Tcl_Size *wordNext;

    if (eclPtr->nuloc >= eclPtr->nloc) {
	/*
	 * Expand the ECL array by allocating more storage from the heap. The
	 * currently allocated ECL entries are stored from eclPtr->loc[0] up
	 * to eclPtr->loc[eclPtr->nuloc - 1] (inclusive).
	 */

	size_t currElems = eclPtr->nloc;
	size_t newElems = (currElems ? 2*currElems : 1);
	size_t newBytes = newElems * sizeof(ECL);

	eclPtr->loc = (ECL *)Tcl_Realloc(eclPtr->loc, newBytes);
	eclPtr->nloc = newElems;
    }

    ePtr = &eclPtr->loc[eclPtr->nuloc];
    ePtr->srcOffset = srcOffset;
    ePtr->line = (int *)Tcl_Alloc(numWords * sizeof(int));
    ePtr->next = (Tcl_Size **)Tcl_Alloc(numWords * sizeof(Tcl_Size *));
    ePtr->nline = numWords;
    wwlines = (int *)Tcl_Alloc(numWords * sizeof(int));

    last = cmd;
    wordLine = line;
    wordNext = clNext;
    for (wordIdx=0 ; wordIdx<numWords;
	    wordIdx++, tokenPtr += tokenPtr->numComponents + 1) {
	TclAdvanceLines(&wordLine, last, tokenPtr->start);
	TclAdvanceContinuations(&wordLine, &wordNext,
		tokenPtr->start - envPtr->source);
	/* See Ticket 4b61afd660 */
	wwlines[wordIdx] =
		((wordIdx == 0) || TclWordKnownAtCompileTime(tokenPtr, NULL))
		? wordLine : -1;
	ePtr->line[wordIdx] = wordLine;
	ePtr->next[wordIdx] = wordNext;
	last = tokenPtr->start;
    }

    *wlines = wwlines;
    eclPtr->nuloc ++;
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

Tcl_ExceptionRange
TclCreateExceptRange(
    ExceptionRangeType type,	/* The kind of ExceptionRange desired. */
    CompileEnv *envPtr)		/* Points to CompileEnv for which to create a
				 * new ExceptionRange structure. */
{
    ExceptionRange *rangePtr;
    ExceptionAux *auxPtr;
    Tcl_Size index = envPtr->exceptArrayNext;

    if (index >= envPtr->exceptArrayEnd) {
	/*
	 * Expand the ExceptionRange array. The currently allocated entries
	 * are stored between elements 0 and (envPtr->exceptArrayNext - 1)
	 * [inclusive].
	 */

	size_t currBytes =
		envPtr->exceptArrayNext * sizeof(ExceptionRange);
	size_t currBytes2 = envPtr->exceptArrayNext * sizeof(ExceptionAux);
	size_t newElems = 2*envPtr->exceptArrayEnd;
	size_t newBytes = newElems * sizeof(ExceptionRange);
	size_t newBytes2 = newElems * sizeof(ExceptionAux);

	if (envPtr->mallocedExceptArray) {
	    envPtr->exceptArrayPtr = (ExceptionRange *)
		    Tcl_Realloc(envPtr->exceptArrayPtr, newBytes);
	    envPtr->exceptAuxArrayPtr = (ExceptionAux *)
		    Tcl_Realloc(envPtr->exceptAuxArrayPtr, newBytes2);
	} else {
	    /*
	     * envPtr->exceptArrayPtr isn't a Tcl_Alloc'd pointer, so we must
	     * code a Tcl_Realloc equivalent for ourselves.
	     */

	    ExceptionRange *newPtr = (ExceptionRange *)Tcl_Alloc(newBytes);
	    ExceptionAux *newPtr2 = (ExceptionAux *)Tcl_Alloc(newBytes2);

	    memcpy(newPtr, envPtr->exceptArrayPtr, currBytes);
	    memcpy(newPtr2, envPtr->exceptAuxArrayPtr, currBytes2);
	    envPtr->exceptArrayPtr = newPtr;
	    envPtr->exceptAuxArrayPtr = newPtr2;
	    envPtr->mallocedExceptArray = 1;
	}
	envPtr->exceptArrayEnd = newElems;
    }
    envPtr->exceptArrayNext++;

    rangePtr = &envPtr->exceptArrayPtr[index];
    rangePtr->type = type;
    rangePtr->nestingLevel = envPtr->exceptDepth;
    rangePtr->codeOffset = TCL_INDEX_NONE;
    rangePtr->numCodeBytes = TCL_INDEX_NONE;
    rangePtr->breakOffset = TCL_INDEX_NONE;
    rangePtr->continueOffset = TCL_INDEX_NONE;
    rangePtr->catchOffset = TCL_INDEX_NONE;
    auxPtr = &envPtr->exceptAuxArrayPtr[index];
    auxPtr->supportsContinue = 1;
    auxPtr->stackDepth = envPtr->currStackDepth;
    auxPtr->expandTarget = envPtr->expandCount;
    auxPtr->expandTargetDepth = TCL_INDEX_NONE;
    auxPtr->numBreakTargets = 0;
    auxPtr->breakTargets = NULL;
    auxPtr->allocBreakTargets = 0;
    auxPtr->numContinueTargets = 0;
    auxPtr->continueTargets = NULL;
    auxPtr->allocContinueTargets = 0;
    return index;
}

/*
 * ---------------------------------------------------------------------
 *
 * TclGetInnermostExceptionRange --
 *
 *	Returns the innermost exception range that covers the current code
 *	creation point, and optionally the stack depth that is expected at
 *	that point. Relies on the fact that the range has a numCodeBytes = -1
 *	when it is being populated and that inner ranges come after outer
 *	ranges.
 *
 * ---------------------------------------------------------------------
 */

ExceptionRange *
TclGetInnermostExceptionRange(
    CompileEnv *envPtr,
    int returnCode,
    ExceptionAux **auxPtrPtr)
{
    size_t i = envPtr->exceptArrayNext;
    ExceptionRange *rangePtr = envPtr->exceptArrayPtr + i;

    while (i > 0) {
	rangePtr--;
	i--;

	if (CurrentOffset(envPtr) >= rangePtr->codeOffset &&
		(rangePtr->numCodeBytes == TCL_INDEX_NONE || CurrentOffset(envPtr) <
			rangePtr->codeOffset+rangePtr->numCodeBytes) &&
		(returnCode != TCL_CONTINUE ||
			envPtr->exceptAuxArrayPtr[i].supportsContinue)) {

	    if (auxPtrPtr) {
		*auxPtrPtr = envPtr->exceptAuxArrayPtr + i;
	    }
	    return rangePtr;
	}
    }
    return NULL;
}

/*
 * ---------------------------------------------------------------------
 *
 * TclAddLoopBreakFixup, TclAddLoopContinueFixup --
 *
 *	Adds a place that wants to break/continue to the loop exception range
 *	tracking that will be fixed up once the loop can be finalized. These
 *	functions generate an INST_JUMP that is fixed up during the
 *	loop finalization.
 *
 * ---------------------------------------------------------------------
 */

void
TclAddLoopBreakFixup(
    CompileEnv *envPtr,
    ExceptionAux *auxPtr)
{
    Tcl_Size range = auxPtr - envPtr->exceptAuxArrayPtr;

    if (envPtr->exceptArrayPtr[range].type != LOOP_EXCEPTION_RANGE) {
	Tcl_Panic("trying to add 'break' fixup to full exception range");
    }

    if (++auxPtr->numBreakTargets > auxPtr->allocBreakTargets) {
	auxPtr->allocBreakTargets *= 2;
	auxPtr->allocBreakTargets += 2;
	if (auxPtr->breakTargets) {
	    auxPtr->breakTargets = (size_t *)Tcl_Realloc(auxPtr->breakTargets,
		    sizeof(size_t) * auxPtr->allocBreakTargets);
	} else {
	    auxPtr->breakTargets = (size_t *)Tcl_Alloc(
		    sizeof(size_t) * auxPtr->allocBreakTargets);
	}
    }
    auxPtr->breakTargets[auxPtr->numBreakTargets - 1] = CurrentOffset(envPtr);
    OP4(			JUMP, 0);
}

void
TclAddLoopContinueFixup(
    CompileEnv *envPtr,
    ExceptionAux *auxPtr)
{
    Tcl_Size range = auxPtr - envPtr->exceptAuxArrayPtr;

    if (envPtr->exceptArrayPtr[range].type != LOOP_EXCEPTION_RANGE) {
	Tcl_Panic("trying to add 'continue' fixup to full exception range");
    }

    if (++auxPtr->numContinueTargets > auxPtr->allocContinueTargets) {
	auxPtr->allocContinueTargets *= 2;
	auxPtr->allocContinueTargets += 2;
	if (auxPtr->continueTargets) {
	    auxPtr->continueTargets = (size_t *)Tcl_Realloc(auxPtr->continueTargets,
		    sizeof(size_t) * auxPtr->allocContinueTargets);
	} else {
	    auxPtr->continueTargets = (size_t *)Tcl_Alloc(
		    sizeof(size_t) * auxPtr->allocContinueTargets);
	}
    }
    auxPtr->continueTargets[auxPtr->numContinueTargets - 1] =
	    CurrentOffset(envPtr);
    OP4(			JUMP, 0);
}

/*
 * ---------------------------------------------------------------------
 *
 * TclCleanupStackForBreakContinue --
 *
 *	Removes the extra elements from the auxiliary stack and the main stack.
 *	How this is done depends on whether there are any elements on
 *	the auxiliary stack to pop.
 *
 * ---------------------------------------------------------------------
 */

void
TclCleanupStackForBreakContinue(
    CompileEnv *envPtr,
    ExceptionAux *auxPtr)
{
    size_t savedStackDepth = envPtr->currStackDepth;
    Tcl_Size toPop = envPtr->expandCount - auxPtr->expandTarget;

    if (toPop > 0) {
	while (toPop --> 0) {
	    OP(			EXPAND_DROP);
	}
	STKDELTA(auxPtr->expandTargetDepth - envPtr->currStackDepth);
	envPtr->currStackDepth = auxPtr->expandTargetDepth;
    }
    toPop = envPtr->currStackDepth - auxPtr->stackDepth;
    while (toPop --> 0) {
	OP(			POP);
    }
    envPtr->currStackDepth = savedStackDepth;
}

/*
 * ---------------------------------------------------------------------
 *
 * StartExpanding --
 *
 *	Pushes an INST_EXPAND_START and does some additional housekeeping so
 *	that the [break] and [continue] compilers can use an exception-free
 *	issue to discard it.
 *
 * ---------------------------------------------------------------------
 */

static void
StartExpanding(
    CompileEnv *envPtr)
{
    Tcl_Size i;

    OP(				EXPAND_START);

    /*
     * Update inner exception ranges with information about the environment
     * where this expansion started.
     */

    for (i=0 ; i<envPtr->exceptArrayNext ; i++) {
	ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[i];
	ExceptionAux *auxPtr = &envPtr->exceptAuxArrayPtr[i];

	/*
	 * Ignore loops unless they're still being built.
	 */

	if (rangePtr->codeOffset > CurrentOffset(envPtr)) {
	    continue;
	}
	if (rangePtr->numCodeBytes != TCL_INDEX_NONE) {
	    continue;
	}

	/*
	 * Adequate condition: loops further out and exceptions further in
	 * don't actually need this information.
	 */

	if (auxPtr->expandTarget == envPtr->expandCount) {
	    auxPtr->expandTargetDepth = envPtr->currStackDepth;
	}
    }

    /*
     * One more expansion is now being processed on the auxiliary stack.
     */

    envPtr->expandCount++;
}

/*
 * ---------------------------------------------------------------------
 *
 * TclFinalizeLoopExceptionRange --
 *
 *	Finalizes a loop exception range, binding the registered [break] and
 *	[continue] implementations so that they jump to the correct place.
 *	This must be called only after *all* the exception range
 *	target offsets have been set.
 *
 * ---------------------------------------------------------------------
 */

void
TclFinalizeLoopExceptionRange(
    CompileEnv *envPtr,
    Tcl_ExceptionRange range)
{
    ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[range];
    ExceptionAux *auxPtr = &envPtr->exceptAuxArrayPtr[range];
    Tcl_Size i, offset;
    unsigned char *site;

    if (rangePtr->type != LOOP_EXCEPTION_RANGE) {
	Tcl_Panic("trying to finalize a loop exception range");
    }

    /*
     * Do the jump fixups. Note that these are always issued as INST_JUMP so
     * there is no need to fuss around with updating code offsets.
     */

    for (i=0 ; i<auxPtr->numBreakTargets ; i++) {
	site = envPtr->codeStart + auxPtr->breakTargets[i];
	offset = rangePtr->breakOffset - auxPtr->breakTargets[i];
	TclUpdateInstInt4AtPc(INST_JUMP, offset, site);
    }
    for (i=0 ; i<(int)auxPtr->numContinueTargets ; i++) {
	site = envPtr->codeStart + auxPtr->continueTargets[i];
	if (rangePtr->continueOffset == TCL_INDEX_NONE) {
	    int j;

	    /*
	     * WTF? Can't bind, so revert to an INST_CONTINUE. Not enough
	     * space to do anything else.
	     */

	    *site = INST_CONTINUE;
	    for (j=0 ; j<4 ; j++) {
		*++site = INST_NOP;
	    }
	} else {
	    offset = rangePtr->continueOffset - auxPtr->continueTargets[i];
	    TclUpdateInstInt4AtPc(INST_JUMP, offset, site);
	}
    }

    /*
     * Drop the arrays we were holding the only reference to.
     */

    if (auxPtr->breakTargets) {
	Tcl_Free(auxPtr->breakTargets);
	auxPtr->breakTargets = NULL;
	auxPtr->numBreakTargets = 0;
    }
    if (auxPtr->continueTargets) {
	Tcl_Free(auxPtr->continueTargets);
	auxPtr->continueTargets = NULL;
	auxPtr->numContinueTargets = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateAuxData --
 *
 *	Allocates and initializes a new AuxData structure in a
 *	CompileEnv's array of compilation auxiliary data records. These
 *	AuxData records hold information created during compilation by
 *	CompileProcs and used by instructions during execution.
 *
 * Results:
 *	The index of the newly-created AuxData structure in the array.
 *
 * Side effects:
 *	If there is not enough room in the CompileEnv's AuxData array, its size
 *	is doubled.
 *----------------------------------------------------------------------
 */

Tcl_AuxDataRef
TclCreateAuxData(
    void *clientData,		/* The compilation auxiliary data to store in
				 * the new aux data record. */
    const AuxDataType *typePtr,	/* Pointer to the type to attach to this
				 * AuxData */
    CompileEnv *envPtr)		/* Points to the CompileEnv for which a new
				 * aux data structure is to be allocated. */
{
    Tcl_Size index;		/* Index for the new AuxData structure. */
    AuxData *auxDataPtr;	/* Points to the new AuxData structure */

    index = envPtr->auxDataArrayNext;
    if (index >= envPtr->auxDataArrayEnd) {
	/*
	 * Expand the AuxData array. The currently allocated entries are
	 * stored between elements 0 and (envPtr->auxDataArrayNext - 1)
	 * [inclusive].
	 */

	size_t currBytes = envPtr->auxDataArrayNext * sizeof(AuxData);
	size_t newElems = 2*envPtr->auxDataArrayEnd;
	size_t newBytes = newElems * sizeof(AuxData);

	if (envPtr->mallocedAuxDataArray) {
	    envPtr->auxDataArrayPtr =
		    (AuxData *)Tcl_Realloc(envPtr->auxDataArrayPtr, newBytes);
	} else {
	    /*
	     * envPtr->auxDataArrayPtr isn't a Tcl_Alloc'd pointer, so we must
	     * code a Tcl_Realloc equivalent for ourselves.
	     */

	    AuxData *newPtr = (AuxData *)Tcl_Alloc(newBytes);

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
    JumpFixupArray *fixupArrayPtr)
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
 *	Uses malloc to allocate more storage for a jump fixup array.
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
    JumpFixupArray *fixupArrayPtr)
				/* Points to the JumpFixupArray structure to
				 * enlarge. */
{
    /*
     * The currently allocated jump fixup entries are stored from fixup[0] up
     * to fixup[fixupArrayPtr->fixupNext] (*not* inclusive). We assume
     * fixupArrayPtr->fixupNext is equal to fixupArrayPtr->fixupEnd.
     */

    size_t currBytes = fixupArrayPtr->next * sizeof(JumpFixup);
    size_t newElems = 2*(fixupArrayPtr->end + 1);
    size_t newBytes = newElems * sizeof(JumpFixup);

    if (fixupArrayPtr->mallocedArray) {
	fixupArrayPtr->fixup = (JumpFixup *)Tcl_Realloc(fixupArrayPtr->fixup,
		newBytes);
    } else {
	/*
	 * fixupArrayPtr->fixup isn't a Tcl_Alloc'd pointer, so we must code a
	 * Tcl_Realloc equivalent for ourselves.
	 */

	JumpFixup *newPtr = (JumpFixup *)Tcl_Alloc(newBytes);

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
    JumpFixupArray *fixupArrayPtr)
				/* Points to the JumpFixupArray structure to
				 * free. */
{
    if (fixupArrayPtr->mallocedArray) {
	Tcl_Free(fixupArrayPtr->fixup);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclEmitForwardJump --
 *
 *	Emits a five-byte forward jump of kind "jumpType".  Also initializes a
 *	JumpFixup record with information about the jump.
 *
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The JumpFixup record pointed to by "jumpFixupPtr" is initialized with
 *	information needed later. Also, a five byte jump of the designated type
 *	is emitted at the current point in the bytecode stream.
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
    jumpFixupPtr->codeOffset = CurrentOffset(envPtr);
    jumpFixupPtr->cmdIndex = envPtr->numCommands;
    jumpFixupPtr->exceptIndex = envPtr->exceptArrayNext;

    switch (jumpType) {
    case TCL_UNCONDITIONAL_JUMP:
	OP4(			JUMP, 0);
	break;
    case TCL_TRUE_JUMP:
	OP4(			JUMP_TRUE, 0);
	break;
    default: // TCL_FALSE_JUMP
	OP4(			JUMP_FALSE, 0);
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclFixupForwardJump --
 *
 *	Modifies a previously-emitted forward jump to jump a specified number
 *	of bytes, "jumpDist". The jump is described by a JumpFixup record
 *	previously initialized by TclEmitForwardJump.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

void
TclFixupForwardJump(
    CompileEnv *envPtr,		/* Points to the CompileEnv structure that
				 * holds the resulting instruction. */
    JumpFixup *jumpFixupPtr,	/* Points to the JumpFixup structure that
				 * describes the forward jump. */
    Tcl_Size jumpDist)		/* Jump distance to set in jump instr. */
{
    unsigned char *jumpPc = envPtr->codeStart + jumpFixupPtr->codeOffset;

    switch (jumpFixupPtr->jumpType) {
    case TCL_UNCONDITIONAL_JUMP:
	TclUpdateInstInt4AtPc(	INST_JUMP, jumpDist, jumpPc);
	break;
    case TCL_TRUE_JUMP:
	TclUpdateInstInt4AtPc(	INST_JUMP_TRUE, jumpDist, jumpPc);
	break;
    default: // TCL_FALSE_JUMP
	TclUpdateInstInt4AtPc(	INST_JUMP_FALSE, jumpDist, jumpPc);
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclEmitInvoke --
 *
 *	Emits one of the invoke-related instructions, wrapping it if necessary
 *	in code that ensures that any break or continue operation passing
 *	through it gets the stack unwinding correct, converting it into an
 *	internal jump if in an appropriate context.
 *
 *	Handles the instructions that can generate TCL_BREAK or TCL_CONTINUE.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Issues the jump with all correct stack management. May create another
 *	loop exception range.  Pointers to ExceptionRange and ExceptionAux
 *	structures should not be held across this call.
 *
 *----------------------------------------------------------------------
 */

void
TclEmitInvoke(
    CompileEnv *envPtr,
    int opcode,
    ...)
{
    va_list argList;
    ExceptionRange *rangePtr;
    ExceptionAux *auxBreakPtr, *auxContinuePtr;
    Tcl_Size arg1, arg2, wordCount = 0, expandCount = 0;
    Tcl_ExceptionRange loopRange = 0, breakRange = 0, continueRange = 0;
    Tcl_Size cleanup, depth = TclGetStackDepth(envPtr);

    /*
     * Parse the arguments.
     */

    va_start(argList, opcode);
    switch (opcode) {
#ifndef TCL_NO_DEPRECATED
    case INST_TCLOO_NEXT1:
    case INST_TCLOO_NEXT_CLASS1:
    case INST_INVOKE_STK1:
	wordCount = arg1 = cleanup = va_arg(argList, int);
	arg2 = 0;
	break;
#endif
    case INST_TCLOO_NEXT:
    case INST_TCLOO_NEXT_CLASS:
    case INST_INVOKE_STK:
	wordCount = arg1 = cleanup = va_arg(argList, int);
	arg2 = 0;
	break;
    case INST_INVOKE_REPLACE:
	arg1 = va_arg(argList, int);
	arg2 = va_arg(argList, int);
	wordCount = arg1 + arg2 - 1;
	cleanup = arg1 + 1;
	break;
    case INST_YIELD:
    case INST_YIELD_TO_INVOKE:
    case INST_EVAL_STK:
    case INST_TCLOO_NEXT_LIST:
    case INST_TCLOO_NEXT_CLASS_LIST:
	wordCount = cleanup = 1;
	arg1 = arg2 = 0;
	break;
    case INST_RETURN_STK:
    case INST_UPLEVEL:
	wordCount = cleanup = 2;
	arg1 = arg2 = 0;
	break;
    case INST_INVOKE_EXPANDED:
	wordCount = arg1 = cleanup = va_arg(argList, int);
	arg2 = 0;
	expandCount = 1;
	break;
    default:
	Tcl_Panic("opcode %s not handled by TclEmitInvoke()",
		tclInstructionTable[opcode].name);
    }
    va_end(argList);

    /*
     * If the exceptions is for break or continue handle it with special
     * handling exception range so the stack may be correctly unwound.
     *
     * These must be done separately since they can be different, especially
     * for calls from inside a [for] increment clause.
     */

    rangePtr = TclGetInnermostExceptionRange(envPtr, TCL_CONTINUE,
	    &auxContinuePtr);
    if (rangePtr == NULL || rangePtr->type != LOOP_EXCEPTION_RANGE) {
	auxContinuePtr = NULL;
    } else if (auxContinuePtr->stackDepth == envPtr->currStackDepth-wordCount
	    && (auxContinuePtr->expandTarget+expandCount == envPtr->expandCount)) {
	auxContinuePtr = NULL;
    } else {
	continueRange = auxContinuePtr - envPtr->exceptAuxArrayPtr;
    }

    rangePtr = TclGetInnermostExceptionRange(envPtr, TCL_BREAK, &auxBreakPtr);
    if (rangePtr == NULL || rangePtr->type != LOOP_EXCEPTION_RANGE) {
	auxBreakPtr = NULL;
    } else if (auxContinuePtr == NULL
	    && auxBreakPtr->stackDepth+wordCount == envPtr->currStackDepth
	    && auxBreakPtr->expandTarget+expandCount == envPtr->expandCount) {
	auxBreakPtr = NULL;
    } else {
	breakRange = auxBreakPtr - envPtr->exceptAuxArrayPtr;
    }

    if (auxBreakPtr != NULL || auxContinuePtr != NULL) {
	loopRange = MAKE_LOOP_RANGE();
	ExceptionRangeStarts(envPtr, loopRange);
    }

    /*
     * Issue the invoke itself.
     */

    switch (opcode) {
#ifndef TCL_NO_DEPRECATED
    case INST_INVOKE_STK1:
	OP1(			INVOKE_STK1, arg1);
	break;
#endif
    case INST_INVOKE_STK:
	OP4(			INVOKE_STK, arg1);
	break;
    case INST_INVOKE_EXPANDED:
	OP(			INVOKE_EXPANDED);
	envPtr->expandCount--;
	STKDELTA(1 - arg1);
	break;
    case INST_EVAL_STK:
	OP(			EVAL_STK);
	break;
    case INST_RETURN_STK:
	OP(			RETURN_STK);
	break;
    case INST_INVOKE_REPLACE:
	OP41(			INVOKE_REPLACE, arg1, arg2);
	break;
#ifndef TCL_NO_DEPRECATED
    case INST_TCLOO_NEXT1:
	OP1(			TCLOO_NEXT1, arg1);
	break;
    case INST_TCLOO_NEXT_CLASS1:
	OP1(			TCLOO_NEXT_CLASS1, arg1);
	break;
#endif
    case INST_TCLOO_NEXT:
	OP4(			TCLOO_NEXT, arg1);
	break;
    case INST_TCLOO_NEXT_LIST:
	OP(			TCLOO_NEXT_LIST);
	break;
    case INST_TCLOO_NEXT_CLASS:
	OP4(			TCLOO_NEXT_CLASS, arg1);
	break;
    case INST_TCLOO_NEXT_CLASS_LIST:
	OP(			TCLOO_NEXT_CLASS_LIST);
	break;
    case INST_UPLEVEL:
	OP(			UPLEVEL);
	break;
    case INST_YIELD:
	OP(			YIELD);
	break;
    case INST_YIELD_TO_INVOKE:
	OP(			YIELD_TO_INVOKE);
	break;
    default:
	Tcl_Panic("opcode %s not handled by TclEmitInvoke()",
		tclInstructionTable[opcode].name);
    }

    /*
     * If we're generating a special wrapper exception range, we need to
     * finish that up now.
     */

    if (auxBreakPtr != NULL || auxContinuePtr != NULL) {
	size_t savedStackDepth = envPtr->currStackDepth;
	size_t savedExpandCount = envPtr->expandCount;
	Tcl_BytecodeLabel noTrap;

	if (auxBreakPtr != NULL) {
	    auxBreakPtr = envPtr->exceptAuxArrayPtr + breakRange;
	}
	if (auxContinuePtr != NULL) {
	    auxContinuePtr = envPtr->exceptAuxArrayPtr + continueRange;
	}

	ExceptionRangeEnds(envPtr, loopRange);
	FWDJUMP(		JUMP, noTrap);

	/*
	 * Careful! When generating these stack unwinding sequences, the depth
	 * of stack in the cases where they are taken is not the same as if
	 * the exception is not taken.
	 */

	if (auxBreakPtr != NULL) {
	    STKDELTA(-1);

	    BREAK_TARGET(	loopRange);
	    TclCleanupStackForBreakContinue(envPtr, auxBreakPtr);
	    TclAddLoopBreakFixup(envPtr, auxBreakPtr);
	    STKDELTA(1);

	    envPtr->currStackDepth = savedStackDepth;
	    envPtr->expandCount = savedExpandCount;
	}

	if (auxContinuePtr != NULL) {
	    STKDELTA(-1);

	    CONTINUE_TARGET(	loopRange);
	    TclCleanupStackForBreakContinue(envPtr, auxContinuePtr);
	    TclAddLoopContinueFixup(envPtr, auxContinuePtr);
	    STKDELTA(1);

	    envPtr->currStackDepth = savedStackDepth;
	    envPtr->expandCount = savedExpandCount;
	}

	FINALIZE_LOOP(		loopRange);
	FWDLABEL(		noTrap);
    }
    TclCheckStackDepth(depth+1-cleanup, envPtr);
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
    CmdLocation *mapPtr = envPtr->cmdMapPtr;
    Tcl_Size numCmds = envPtr->numCommands;
    Tcl_Size codeDelta, codeLen, srcDelta, srcLen;
    int codeDeltaNext, codeLengthNext, srcDeltaNext, srcLengthNext;
				/* The offsets in their respective byte
				 * sequences where the next encoded offset or
				 * length should go. */
    Tcl_Size prevCodeOffset, prevSrcOffset, i;

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
 *	Encodes the command location information for some compiled code into a
 *	ByteCode structure. The encoded command location map is stored as
 *	three-adjacent-byte sequences.
 *
 * Results:
 *	A pointer to the first byte after the encoded command location
 *	information.
 *
 * Side effects:
 *	Stores encoded information into the block of memory headed by
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
    CmdLocation *mapPtr = envPtr->cmdMapPtr;
    Tcl_Size i, codeDelta, codeLen, srcLen, prevOffset;
    Tcl_Size numCmds = envPtr->numCommands;
    unsigned char *p = startPtr;
    Tcl_Size srcDelta;

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
 *----------------------------------------------------------------------
 *
 * TclIsEmptyToken --
 *
 *	Test if the token is empty.
 *
 *	We don't test if it's just comments. Fixes are welcome.
 *
 * Results:
 *	True iff the token (assumed a TCL_TOKEN_SIMPLE_TEXT) only contains
 *	whitespace of the kind that never results in code being generated.
 *	False otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
TclIsEmptyToken(
    const Tcl_Token *tokenPtr)
{
    const char *ptr, *end;
    int ucs4;
    Tcl_Size chLen = 0;

    end = tokenPtr[1].start + tokenPtr[1].size;
    for (ptr = tokenPtr[1].start; ptr < end; ptr += chLen) {
	chLen = TclUtfToUniChar(ptr, &ucs4);
	// Can't use Tcl_UniCharIsSpace; see test dict-22.24
	if (!TclIsSpaceProcM((unsigned) ucs4)) {
	    return 0;
	}
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclLocalScalarFromToken --
 *
 *	Get the index into the table of compiled locals that corresponds
 *	to a local scalar variable name.
 *
 * Results:
 *	Returns the non-negative integer index value into the table of
 *	compiled locals corresponding to a local scalar variable name.
 *	If the arguments passed in do not identify a local scalar variable
 *	then return TCL_INDEX_NONE.
 *
 * Side effects:
 *	May add an entry into the table of compiled locals.
 *
 *----------------------------------------------------------------------
 */

Tcl_Size
TclLocalScalarFromToken(
    Tcl_Token *tokenPtr,
    CompileEnv *envPtr)
{
    int isScalar;
    Tcl_LVTIndex index;

    TclPushVarName(NULL, tokenPtr, envPtr, TCL_NO_ELEMENT, &index, &isScalar);
    if (!isScalar) {
	index = TCL_INDEX_NONE;
    }
    return index;
}

Tcl_Size
TclLocalScalar(
    const char *bytes,
    size_t numBytes,
    CompileEnv *envPtr)
{
    Tcl_Token token[2] = {
	{TCL_TOKEN_SIMPLE_WORD, NULL, 0, 1},
	{TCL_TOKEN_TEXT, NULL, 0, 0}
    };

    token[1].start = bytes;
    token[1].size = numBytes;
    return TclLocalScalarFromToken(token, envPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclPushVarName --
 *
 *	Procedure used in the compiling where pushing a variable name is
 *	necessary (append, lappend, set).
 *
 * Results:
 *	The values written to *localIndexPtr and *isScalarPtr signal to
 *	the caller what the instructions emitted by this routine will do:
 *
 *	*isScalarPtr	(*localIndexPtr < 0)
 *	1		1	Push the varname on the stack. (Stack +1)
 *	1		0	*localIndexPtr is the index of the compiled
 *				local for this varname.  No instructions
 *				emitted.	(Stack +0)
 *	0		1	Push part1 and part2 names of array element
 *				on the stack.	(Stack +2)
 *	0		0	*localIndexPtr is the index of the compiled
 *				local for this array.  Element name is pushed
 *				on the stack.	(Stack +1)
 *
 * Side effects:
 *	Instructions are added to envPtr.
 *
 *----------------------------------------------------------------------
 */

void
TclPushVarName(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Token *varTokenPtr,	/* Points to a variable token. */
    CompileEnv *envPtr,		/* Holds resulting instructions. */
    int flags,			/* TCL_NO_ELEMENT. */
    Tcl_Size *localIndexPtr,	/* Must not be NULL. */
    int *isScalarPtr)		/* Must not be NULL. */
{
    const char *p;
    const char *last, *name, *elName;
    Tcl_Token *elemTokenPtr = NULL;
    Tcl_Size nameLen, elNameLen, n;
    int simpleVarName = 0, allocedTokens = 0;
    Tcl_Size elemTokenCount = 0, removedParen = 0;
    Tcl_LVTIndex localIndex;

    /*
     * Decide if we can use a frame slot for the var/array name or if we need
     * to emit code to compute and push the name at runtime. We use a frame
     * slot (entry in the array of local vars) if we are compiling a procedure
     * body and if the name is simple text that does not include namespace
     * qualifiers.
     */

    name = elName = NULL;
    nameLen = elNameLen = 0;
    localIndex = TCL_INDEX_NONE;

    if (varTokenPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	/*
	 * A simple variable name. Divide it up into "name" and "elName"
	 * strings. If it is not a local variable, look it up at runtime.
	 */

	simpleVarName = 1;

	name = varTokenPtr[1].start;
	nameLen = varTokenPtr[1].size;
	if (nameLen > 0 && name[nameLen - 1] == ')') {
	    /*
	     * last char is ')' => potential array reference.
	     */
	    last = &name[nameLen - 1];

	    if (*last == ')') {
		for (p = name;  p < last;  p++) {
		    if (*p == '(') {
			elName = p + 1;
			elNameLen = last - elName;
			nameLen = p - name;
			break;
		    }
		}
	    }

	    if (!(flags & TCL_NO_ELEMENT) && elNameLen) {
		/*
		 * An array element, the element name is a simple string:
		 * assemble the corresponding token.
		 */

		elemTokenPtr = (Tcl_Token *)TclStackAlloc(interp, sizeof(Tcl_Token));
		allocedTokens = 1;
		elemTokenPtr->type = TCL_TOKEN_TEXT;
		elemTokenPtr->start = elName;
		elemTokenPtr->size = elNameLen;
		elemTokenPtr->numComponents = 0;
		elemTokenCount = 1;
	    }
	}
    } else if (interp && ((n = varTokenPtr->numComponents) > 1)
	    && (varTokenPtr[1].type == TCL_TOKEN_TEXT)
	    && (varTokenPtr[n].type == TCL_TOKEN_TEXT)
	    && (varTokenPtr[n].start[varTokenPtr[n].size - 1] == ')')) {
	/*
	 * Check for parentheses inside first token.
	 */

	simpleVarName = 0;
	for (p = varTokenPtr[1].start, last = p + varTokenPtr[1].size;
		p < last;  p++) {
	    if (*p == '(') {
		simpleVarName = 1;
		break;
	    }
	}
	if (simpleVarName) {
	    size_t remainingLen;

	    /*
	     * Check the last token: if it is just ')', do not count it.
	     * Otherwise, remove the ')' and flag so that it is restored at
	     * the end.
	     */

	    if (varTokenPtr[n].size == 1) {
		n--;
	    } else {
		varTokenPtr[n].size--;
		removedParen = n;
	    }

	    name = varTokenPtr[1].start;
	    nameLen = p - varTokenPtr[1].start;
	    elName = p + 1;
	    remainingLen = (varTokenPtr[2].start - p) - 1;
	    elNameLen = (varTokenPtr[n].start-p) + varTokenPtr[n].size - 1;

	    if (!(flags & TCL_NO_ELEMENT)) {
		if (remainingLen) {
		    /*
		     * Make a first token with the extra characters in the
		     * first token.
		     */

		    elemTokenPtr = (Tcl_Token *)TclStackAlloc(interp,
			    n * sizeof(Tcl_Token));
		    allocedTokens = 1;
		    elemTokenPtr->type = TCL_TOKEN_TEXT;
		    elemTokenPtr->start = elName;
		    elemTokenPtr->size = remainingLen;
		    elemTokenPtr->numComponents = 0;
		    elemTokenCount = n;

		    /*
		     * Copy the remaining tokens.
		     */

		    memcpy(elemTokenPtr + 1, varTokenPtr + 2,
			    (n - 1) * sizeof(Tcl_Token));
		} else {
		    /*
		     * Use the already available tokens.
		     */

		    elemTokenPtr = &varTokenPtr[2];
		    elemTokenCount = n - 1;
		}
	    }
	}
    }

    if (simpleVarName) {
	/*
	 * See whether name has any namespace separators (::'s).
	 */

	int hasNsQualifiers = 0;

	for (p = name, last = p + nameLen-1;  p < last;  p++) {
	    if ((p[0] == ':') && (p[1] == ':')) {
		hasNsQualifiers = 1;
		break;
	    }
	}

	/*
	 * Look up the var name's index in the array of local vars in the proc
	 * frame. If retrieving the var's value and it doesn't already exist,
	 * push its name and look it up at runtime.
	 */

	if (!hasNsQualifiers) {
	    localIndex = TclFindCompiledLocal(name, nameLen, 1, envPtr);
	}
	if (interp && localIndex < 0) {
	    PushLiteral(envPtr, name, nameLen);
	}

	/*
	 * Compile the element script, if any, and only if not inhibited. [Bug
	 * 3600328]
	 */

	if (elName != NULL && !(flags & TCL_NO_ELEMENT)) {
	    if (elNameLen) {
		TclCompileTokens(interp, elemTokenPtr, elemTokenCount,
			envPtr);
	    } else {
		PUSH(		"");
	    }
	}
    } else if (interp) {
	/*
	 * The var name isn't simple: compile and push it.
	 */

	CompileTokens(envPtr, varTokenPtr, interp);
    }

    if (removedParen) {
	varTokenPtr[removedParen].size++;
    }
    if (allocedTokens) {
	TclStackFree(interp, elemTokenPtr);
    }
    *localIndexPtr = localIndex;
    *isScalarPtr = (elName == NULL);
}

#ifdef TCL_COMPILE_STATS
/*
 *----------------------------------------------------------------------
 *
 * RecordByteCodeStats --
 *
 *	Accumulates compilation-related statistics for each newly-compiled
 *	ByteCode. Called by the TclInitByteCodeObj when Tcl is compiled with
 *	the -DTCL_COMPILE_STATS flag
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Accumulates aggregate code-related statistics in the interpreter's
 *	ByteCodeStats structure. Records statistics specific to a ByteCode in
 *	its ByteCode structure.
 *
 *----------------------------------------------------------------------
 */

void
RecordByteCodeStats(
    ByteCode *codePtr)		/* Points to ByteCode structure with info
				 * to add to accumulated statistics. */
{
    Interp *iPtr = (Interp *) *codePtr->interpHandle;
    ByteCodeStats *statsPtr;

    if (iPtr == NULL) {
	/* Avoid segfaulting in case we're called in a deleted interp */
	return;
    }
    statsPtr = &(iPtr->stats);

    statsPtr->numCompilations++;
    statsPtr->totalSrcBytes += (double)codePtr->numSrcBytes;
    statsPtr->totalByteCodeBytes += (double) codePtr->structureSize;
    statsPtr->currentSrcBytes += (double) (int)codePtr->numSrcBytes;
    statsPtr->currentByteCodeBytes += (double) codePtr->structureSize;

    statsPtr->srcCount[TclLog2(codePtr->numSrcBytes)]++;
    statsPtr->byteCodeCount[TclLog2(codePtr->structureSize)]++;

    statsPtr->currentInstBytes += (double) codePtr->numCodeBytes;
    statsPtr->currentLitBytes += (double)
	    codePtr->numLitObjects * sizeof(Tcl_Obj *);
    statsPtr->currentExceptBytes += (double)
	    codePtr->numExceptRanges * sizeof(ExceptionRange);
    statsPtr->currentAuxBytes += (double)
	    codePtr->numAuxDataItems * sizeof(AuxData);
    statsPtr->currentCmdMapBytes += (double) codePtr->numCmdLocBytes;
}
#endif /* TCL_COMPILE_STATS */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * End:
 */
