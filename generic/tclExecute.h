/*
 * tclExecute.h --
 *
 * This file contains the compiler-dependent macros that determine the 
 * instruction-threading method used by tclExecute.c
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclExecute.h,v 1.1.2.7 2001/05/19 16:12:18 msofer Exp $
 */


/*
 * An instruction-threading method has to define the following macros:
 *
 *  . _CASE_DECLS         declarations of special variables required
 *  . CHECK_OPCODES       0/1, if the opcodes have to be checked before RT
 *  . _CASE_START         start of the block containing instructions
 *  . _CASE(instruction)  the labelling method for instruction start
 *  . NEXT_INSTR          the jump to the next instruction
 *  . _CASE_END           end of the block containing instructions
 *
 *
 * Currently three different methods are implemented:
 * 
 * 1.  SWITCH  produces standard C code valid for any compiler;
 *                jumps from the end of an instruction to the switch 
 *                statement, from there to the next instruction after
 *                performing a bounds check
 * 2.  GCC     uses a gcc extension to jump directly from the end
 *                 of an instruction to the start of the next one, using
 *                 a jump table and the "labels as values" extension
 * 3.  MSVC    directs msvc to omit checking the bounds of the switch
 */


#define SWITCH 0
#define GCC    1
#define MSVC   2

/* 
 * Choose the threading method according to the compiler being used; if
 * TCL_BYTECODE_DEBUG is set, fallback to SWITCH for any compiler
 */

#ifdef  __GNUC__
#define    JUMP_version GCC
#elif defined( _MSC_VER ) && ( _MSC_VER >= 1200 ) && defined( _MSC_EXTENSIONS )
/* msvc with extensions */
#define    JUMP_version MSVC
#else /* standard C output */
#define    JUMP_version SWITCH
#endif  /* choice of method */


#ifdef TCL_BYTECODE_DEBUG 
# undef JUMP_version
# define JUMP_version SWITCH
#  define INIT_TRACE_CHECK \
      if (tclTraceExec == 3) {\
          fprintf(stdout, "%2d: %2d ", iPtr->numLevels, tosPtr - eePtr->stackPtr);\
          TclPrintInstruction(codePtr, pc);\
          fflush(stdout);\
      } 
#else /* not debugging */
#  define INIT_TRACE_CHECK
#endif /* debugging code */ 


/*
 * Define the macros for the different threading methods
 */

/**** SWITCH ****/
#if JUMP_version == SWITCH
#define _CASE_DECLS
#define CHECK_OPCODES 0
#  define _CASE_START \
    instructions_start: \
    INIT_TRACE_CHECK \
    switch (*pc) {
#define _CASE(instruction) case instruction
#define NEXT_INSTR goto instructions_start 
#define _CASE_END \
      default: \
          panic("TclExecuteByteCode: unrecognized opCode %u", *pc);\
      } /* end of switch on opCode */
#endif /* SWITCH methods */



/**** MSVC ****/
#if JUMP_version == MSVC
#define _CASE_DECLS
#define CHECK_OPCODES 1
#define _CASE_START \
    instructions_start: \
    switch (*pc) {
#define _CASE(instruction) case instruction
#define NEXT_INSTR goto instructions_start 
#define _CASE_END \
      default: \
	  __assume(0); /* do not check bounds of switch! */ \
      } /* end of switch on opCode */
#endif /* MSVC methods */


/**** GCC ****/
#if JUMP_version == GCC
#define _CASE_DECLS \
    static const void *jumpTable[] = _JUMP_TABLE;
#define CHECK_OPCODES 1
#define _CASE_START  NEXT_INSTR;
#define _CASE(instruction) _CASE_ ## instruction
#define NEXT_INSTR goto *jumpTable[*pc]
#define _CASE_END /* end of switch on opCode */
/* _CASE_DECLS uses the following jump table
 * IT HAS TO BE IN THE CORRECT ORDER, of course */
#define _JUMP_TABLE {\
   &&_CASE(INST_DONE),\
   &&_CASE(INST_PUSH1),\
   &&_CASE(INST_PUSH4),\
   &&_CASE(INST_POP),\
   &&_CASE(INST_DUP),\
   &&_CASE(INST_CONCAT1),\
   &&_CASE(INST_INVOKE_STK1),\
   &&_CASE(INST_INVOKE_STK4),\
   &&_CASE(INST_EVAL_STK),\
   &&_CASE(INST_EXPR_STK),\
   &&_CASE(INST_LOAD_SCALAR1),\
   &&_CASE(INST_LOAD_SCALAR4),\
   &&_CASE(INST_LOAD_SCALAR_STK),\
   &&_CASE(INST_LOAD_ARRAY1),\
   &&_CASE(INST_LOAD_ARRAY4),\
   &&_CASE(INST_LOAD_ARRAY_STK),\
   &&_CASE(INST_LOAD_STK),\
   &&_CASE(INST_STORE_SCALAR1),\
   &&_CASE(INST_STORE_SCALAR4),\
   &&_CASE(INST_STORE_SCALAR_STK),\
   &&_CASE(INST_STORE_ARRAY1),\
   &&_CASE(INST_STORE_ARRAY4),\
   &&_CASE(INST_STORE_ARRAY_STK),\
   &&_CASE(INST_STORE_STK),\
   &&_CASE(INST_INCR_SCALAR1),\
   &&_CASE(INST_INCR_SCALAR_STK),\
   &&_CASE(INST_INCR_ARRAY1),\
   &&_CASE(INST_INCR_ARRAY_STK),\
   &&_CASE(INST_INCR_STK),\
   &&_CASE(INST_INCR_SCALAR1_IMM),\
   &&_CASE(INST_INCR_SCALAR_STK_IMM),\
   &&_CASE(INST_INCR_ARRAY1_IMM),\
   &&_CASE(INST_INCR_ARRAY_STK_IMM),\
   &&_CASE(INST_INCR_STK_IMM),\
   &&_CASE(INST_JUMP1),\
   &&_CASE(INST_JUMP4),\
   &&_CASE(INST_JUMP_TRUE1),\
   &&_CASE(INST_JUMP_TRUE4),\
   &&_CASE(INST_JUMP_FALSE1),\
   &&_CASE(INST_JUMP_FALSE4),\
   &&_CASE(INST_LOR),\
   &&_CASE(INST_LAND),\
   &&_CASE(INST_BITOR),\
   &&_CASE(INST_BITXOR),\
   &&_CASE(INST_BITAND),\
   &&_CASE(INST_EQ),\
   &&_CASE(INST_NEQ),\
   &&_CASE(INST_LT),\
   &&_CASE(INST_GT),\
   &&_CASE(INST_LE),\
   &&_CASE(INST_GE),\
   &&_CASE(INST_LSHIFT),\
   &&_CASE(INST_RSHIFT),\
   &&_CASE(INST_ADD),\
   &&_CASE(INST_SUB),\
   &&_CASE(INST_MULT),\
   &&_CASE(INST_DIV),\
   &&_CASE(INST_MOD),\
   &&_CASE(INST_UPLUS),\
   &&_CASE(INST_UMINUS),\
   &&_CASE(INST_BITNOT),\
   &&_CASE(INST_LNOT),\
   &&_CASE(INST_CALL_BUILTIN_FUNC1),\
   &&_CASE(INST_CALL_FUNC1),\
   &&_CASE(INST_TRY_CVT_TO_NUMERIC),\
   &&_CASE(INST_BREAK),\
   &&_CASE(INST_CONTINUE),\
   &&_CASE(INST_FOREACH_START4),\
   &&_CASE(INST_FOREACH_STEP4),\
   &&_CASE(INST_BEGIN_CATCH4),\
   &&_CASE(INST_END_CATCH),\
   &&_CASE(INST_PUSH_RESULT),\
   &&_CASE(INST_PUSH_RETURN_CODE),\
   &&_CASE(INST_STR_EQ),\
   &&_CASE(INST_STR_NEQ),\
   &&_CASE(INST_STR_CMP),\
   &&_CASE(INST_STR_LEN),\
   &&_CASE(INST_STR_INDEX),\
   &&_CASE(INST_STR_MATCH),\
   &&_CASE(INST_LIST),\
   &&_CASE(INST_LIST_INDEX),\
   &&_CASE(INST_LIST_LENGTH),\
   &&_CASE(INST_APPEND_SCALAR1),\
   &&_CASE(INST_APPEND_SCALAR4),\
   &&_CASE(INST_APPEND_ARRAY1),\
   &&_CASE(INST_APPEND_ARRAY4),\
   &&_CASE(INST_APPEND_ARRAY_STK),\
   &&_CASE(INST_APPEND_STK),\
   &&_CASE(INST_LAPPEND_SCALAR1),\
   &&_CASE(INST_LAPPEND_SCALAR4),\
   &&_CASE(INST_LAPPEND_ARRAY1),\
   &&_CASE(INST_LAPPEND_ARRAY4),\
   &&_CASE(INST_LAPPEND_ARRAY_STK),\
   &&_CASE(INST_LAPPEND_STK)\
 }
#endif /* GCC methods */
/* End of method definitions */

   
#undef SWITCH 
#undef GCC   
#undef MSVC 
#undef JUMP_version
   
