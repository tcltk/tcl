/*
 * tclCompile.h --
 *
 * Copyright (c) 1996-1998 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 * Copyright (c) 2001 by Kevin B. Kenny. All rights reserved.
 * Copyright (c) 2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLCOMPILATIONINT
#define _TCLCOMPILATIONINT 1

#include "tclInt.h"
#include "tclCompile.h"

/*
 *------------------------------------------------------------------------
 * Variables related to compilation. These are used in tclCompile.c,
 * tclExecute.c, tclBasic.c, and their clients.
 *------------------------------------------------------------------------
 */


/*
 *------------------------------------------------------------------------
 * Data structures related to compilation.
 *------------------------------------------------------------------------
 */

/*
 * Structure used to map between instruction pc and source locations. It
 * defines for each compiled Tcl command its code's starting offset and its
 * source's starting offset and length. Note that the code offset increases
 * monotonically: that is, the table is sorted in code offset order. The
 * source offset is not monotonic.
 */

typedef struct {
    int codeOffset;		/* Offset of first byte of command code. */
    int numCodeBytes;		/* Number of bytes for command's code. */
    int srcOffset;		/* Offset of first char of the command. */
    int numSrcBytes;		/* Number of command source chars. */
} CmdLocation;

/*
 * Structure defining the compilation environment. After compilation, fields
 * describing bytecode instructions are copied out into the more compact
 * ByteCode structure defined below.
 */

#define COMPILEENV_INIT_CODE_BYTES    250
#define COMPILEENV_INIT_NUM_OBJECTS    60
#define COMPILEENV_INIT_CMD_MAP_SIZE   40
#define COMPILEENV_INIT_AUX_DATA_SIZE   5

typedef struct CompileEnv {
    Interp *iPtr;		/* Interpreter containing the code being
				 * compiled. Commands and their compile procs
				 * are specific to an interpreter so the code
				 * emitted will depend on the interpreter. */
    const char *source;		/* The source string being compiled by
				 * SetByteCodeFromAny. This pointer is not
				 * owned by the CompileEnv and must not be
				 * freed or changed by it. */
    int numSrcBytes;		/* Number of bytes in source. */
    Proc *procPtr;		/* If a procedure is being compiled, a pointer
				 * to its Proc structure; otherwise NULL. Used
				 * to compile local variables. Set from
				 * information provided by ObjInterpProc in
				 * tclProc.c. */
    int numCommands;		/* Number of commands compiled. */
    int maxStackDepth;		/* Maximum number of stack elements needed to
				 * execute the code. Set by compilation
				 * procedures before returning. */
    int currStackDepth;		/* Current stack depth. */
    LiteralTable localLitTable;	/* Contains LiteralEntry's describing all Tcl
				 * objects referenced by this compiled code.
				 * Indexed by the string representations of
				 * the literals. Used to avoid creating
				 * duplicate objects. */
    unsigned char *codeStart;	/* Points to the first byte of the code. */
    unsigned char *codeNext;	/* Points to next code array byte to use. */
    unsigned char *codeEnd;	/* Points just after the last allocated code
				 * array byte. */
    int mallocedCodeArray;	/* Set 1 if code array was expanded and
				 * codeStart points into the heap.*/
    LiteralEntry *literalArrayPtr;
    				/* Points to start of LiteralEntry array. */
    int literalArrayNext;	/* Index of next free object array entry. */
    int literalArrayEnd;	/* Index just after last obj array entry. */
    int mallocedLiteralArray;	/* 1 if object array was expanded and objArray
				 * points into the heap, else 0. */
    CmdLocation *cmdMapPtr;	/* Points to start of CmdLocation array.
				 * numCommands is the index of the next entry
				 * to use; (numCommands-1) is the entry index
				 * for the last command. */
    int cmdMapEnd;		/* Index after last CmdLocation entry. */
    int mallocedCmdMap;		/* 1 if command map array was expanded and
				 * cmdMapPtr points in the heap, else 0. */
    unsigned char staticCodeSpace[COMPILEENV_INIT_CODE_BYTES];
				/* Initial storage for code. */
    LiteralEntry staticLiteralSpace[COMPILEENV_INIT_NUM_OBJECTS];
				/* Initial storage of LiteralEntry array. */
    CmdLocation staticCmdMapSpace[COMPILEENV_INIT_CMD_MAP_SIZE];
				/* Initial storage for cmd location map. */
    int atCmdStart;		/* Flag to say whether an INST_START_CMD
				 * should be issued; they should never be
				 * issued repeatedly, as that is significantly
				 * inefficient. */
} CompileEnv;

/*
 * The structure defining the bytecode instructions resulting from compiling a
 * Tcl script. Note that this structure is variable length: a single heap
 * object is allocated to hold the ByteCode structure immediately followed by
 * the code bytes, the literal object array, the
 * CmdLocation map.
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

#define TCL_BYTECODE_RESOLVE_VARS		0x0002

#define TCL_BYTECODE_RECOMPILE			0x0004

typedef struct ByteCode {
    TclHandle interpHandle;	/* Handle for interpreter containing the
				 * compiled code. Commands and their compile
				 * procs are specific to an interpreter so the
				 * code emitted will depend on the
				 * interpreter. */
    Namespace *nsPtr;		/* Namespace context in which this code was
				 * compiled. If the code is executed if a
				 * different namespace, it must be
				 * recompiled. */
    int nsEpoch;		/* Value of nsPtr->resolverEpoch when this
				 * ByteCode was compiled. Used to invalidate
				 * code when new namespace resolution rules
				 * are put into effect. */
    int refCount;		/* Reference count: set 1 when created plus 1
				 * for each execution of the code currently
				 * active. This structure can be freed when
				 * refCount becomes zero. */
    unsigned int flags;		/* flags describing state for the codebyte.
				 * this variable holds ORed values from the
				 * TCL_BYTECODE_ masks defined above */
    const char *source;		/* The source string from which this ByteCode
				 * was compiled. Note that this pointer is not
				 * owned by the ByteCode and must not be freed
				 * or modified by it. */
    Proc *procPtr;		/* If the ByteCode was compiled from a
				 * procedure body, this is a pointer to its
				 * Proc structure; otherwise NULL. This
				 * pointer is also not owned by the ByteCode
				 * and must not be freed by it. */
    struct ExprData *exprData;  /* pointer to workspace for expressions
				 * contained in this bytecode */
    size_t structureSize;	/* Number of bytes in the ByteCode structure
				 * itself. Does not include heap space for
				 * literal Tcl objects. */
    int numCommands;		/* Number of commands compiled. */
    int numSrcBytes;		/* Number of source bytes compiled. */
    int numCodeBytes;		/* Number of code bytes. */
    int numLitObjects;		/* Number of objects in literal array. */
    int numCmdLocBytes;		/* Number of bytes needed for encoded command
				 * location information. */
    int maxStackDepth;		/* Maximum number of stack elements needed to
				 * execute the code. */
    unsigned char *codeStart;	/* Points to the first byte of the code. This
				 * is just after the final ByteCode member
				 * cmdMapPtr. */
    Tcl_Obj **objArrayPtr;	/* Points to the start of the literal object
				 * array. This is just after the last code
				 * byte. */
    unsigned char *codeDeltaStart;
				/* Points to the first of a sequence of bytes
				 * that encode the change in the starting
				 * offset of each command's code. If -127 <=
				 * delta <= 127, it is encoded as 1 byte,
				 * otherwise 0xFF (128) appears and the delta
				 * is encoded by the next 4 bytes. Code deltas
				 * are always positive. */
    unsigned char *codeLengthStart;
				/* Points to the first of a sequence of bytes
				 * that encode the length of each command's
				 * code. The encoding is the same as for code
				 * deltas. Code lengths are always positive.
				 * This sequence is just after the last entry
				 * in the code delta sequence. */
    unsigned char *srcDeltaStart;
				/* Points to the first of a sequence of bytes
				 * that encode the change in the starting
				 * offset of each command's source. The
				 * encoding is the same as for code deltas.
				 * Source deltas can be negative. This
				 * sequence is just after the last byte in the
				 * code length sequence. */
    unsigned char *srcLengthStart;
				/* Points to the first of a sequence of bytes
				 * that encode the length of each command's
				 * source. The encoding is the same as for
				 * code deltas. Source lengths are always
				 * positive. This sequence is just after the
				 * last byte in the source delta sequence. */
    LocalCache *localCachePtr;	/* Pointer to the start of the cached variable
				 * names and initialisation data for local
				 * variables. */
} ByteCode;


/*
 * Opcodes for the Tcl bytecode instructions. These must correspond to the
 * entries in the table of instruction descriptions, tclInstructionTable, in
 * tclCompile.c. Also, the order and number of the expression opcodes (e.g.,
 * INST_LOR) must match the entries in the array operatorStrings in
 * tclExecute.c.
 */

/* General Opcodes */
#define INST_DONE			0
#define INST_SYNTAX			1

#define INST_PUSH4			2
#define INST_POP			3

#define INST_CONCAT1			4
#define INST_INVOKE_STK4		5
#define INST_EXPAND_START		6
#define INST_EXPAND_STKTOP		7
#define INST_INVOKE_EXPANDED		8

#define INST_LOAD_SCALAR4		9
#define INST_LOAD_SCALAR_STK	        10
#define INST_LOAD_ARRAY4		11
#define INST_LOAD_ARRAY_STK		12

#define INST_EXPR                       13

/*
 * Table describing the Tcl bytecode instructions: their name (for displaying
 * code), total number of code bytes required (including operand bytes), and a
 * description of the type of each operand. These operand types include signed
 * and unsigned integers of length one and four bytes. The unsigned integers
 * are used for indexes or for, e.g., the count of objects to push in a "push"
 * instruction.
 */

#define MAX_INSTRUCTION_OPERANDS 2

typedef enum InstOperandType {
    OPERAND_NONE,
    OPERAND_INT1,		/* One byte signed integer. */
    OPERAND_INT4,		/* Four byte signed integer. */
    OPERAND_UINT1,		/* One byte unsigned integer. */
    OPERAND_UINT4,		/* Four byte unsigned integer. */
    OPERAND_IDX4,		/* Four byte signed index (actually an
				 * integer, but displayed differently.) */
    OPERAND_LVT1,		/* One byte unsigned index into the local
				 * variable table. */
    OPERAND_LVT4,		/* Four byte unsigned index into the local
				 * variable table. */
    OPERAND_AUX4		/* Four byte unsigned index into the aux data
				 * table. */
} InstOperandType;

typedef struct InstructionDesc {
    const char *name;		/* Name of instruction. */
    int numBytes;		/* Total number of bytes for instruction. */
    int stackEffect;		/* The worst-case balance stack effect of the
				 * instruction, used for stack requirements
				 * computations. The value INT_MIN signals
				 * that the instruction's worst case effect is
				 * (1-opnd1). */
    int numOperands;		/* Number of operands. */
    InstOperandType opTypes[MAX_INSTRUCTION_OPERANDS];
				/* The type of each operand. */
} InstructionDesc;

MODULE_SCOPE InstructionDesc const tclInstructionTable[];


/*
 *----------------------------------------------------------------
 * Procedures shared among Tcl bytecode compilation and execution modules but
 * not used outside:
 *----------------------------------------------------------------
 */

MODULE_SCOPE void	TclCleanupByteCode(ByteCode *codePtr);
MODULE_SCOPE void	TclCompileCmdWord(Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, int count,
			    CompileEnv *envPtr);
MODULE_SCOPE void	TclCompileExpr(Tcl_Interp *interp, const char *script,
			    int numBytes, CompileEnv *envPtr, int optimize);
MODULE_SCOPE void	TclCompileExprWords(Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, int numWords,
			    CompileEnv *envPtr);
MODULE_SCOPE void	TclCompileScript(Tcl_Interp *interp,
			    const char *script, int numBytes,
			    CompileEnv *envPtr);
MODULE_SCOPE void	TclCompileSyntaxError(Tcl_Interp *interp,
			    CompileEnv *envPtr);
MODULE_SCOPE void	TclCompileTokens(Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, int count,
			    CompileEnv *envPtr);
MODULE_SCOPE void	TclCompileVarSubst(Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, CompileEnv *envPtr);
MODULE_SCOPE Tcl_Obj *	TclCreateLiteral(Interp *iPtr, char *bytes,
			    int length, unsigned int hash, int *newPtr,
			    Namespace *nsPtr, int flags,
			    LiteralEntry **globalPtrPtr);
MODULE_SCOPE int	TclFindCompiledLocal(const char *name, int nameChars,
			    int create, CompileEnv *envPtr);
MODULE_SCOPE LiteralEntry * TclLookupLiteralEntry(Tcl_Interp *interp,
			    Tcl_Obj *objPtr);
MODULE_SCOPE void	TclFreeCompileEnv(CompileEnv *envPtr);
MODULE_SCOPE void	TclInitByteCodeObj(Tcl_Obj *objPtr,
			    CompileEnv *envPtr);
MODULE_SCOPE void	TclInitCompilation(void);
MODULE_SCOPE void	TclInitCompileEnv(Tcl_Interp *interp,
			    CompileEnv *envPtr, const char *string,
			    int numBytes);
MODULE_SCOPE int	TclPrintInstruction(ByteCode *codePtr,
			    const unsigned char *pc);
MODULE_SCOPE void	TclPrintObject(FILE *outFile,
			    Tcl_Obj *objPtr, int maxChars);
MODULE_SCOPE void	TclPrintSource(FILE *outFile,
			    const char *string, int maxChars);
MODULE_SCOPE int	TclRegisterLiteral(CompileEnv *envPtr,
			    char *bytes, int length, int flags);
MODULE_SCOPE void	TclReleaseLiteral(Tcl_Interp *interp, Tcl_Obj *objPtr);
MODULE_SCOPE int	TclWordKnownAtCompileTime(Tcl_Token *tokenPtr,
			    Tcl_Obj *valuePtr);

/*
 *----------------------------------------------------------------
 * Macros and flag values used by Tcl bytecode compilation and execution
 * modules inside the Tcl core but not used outside.
 *----------------------------------------------------------------
 */

#define LITERAL_ON_HEAP		0x01
#define LITERAL_CMD_NAME	0x02

/*
 * Form of TclRegisterLiteral with flags == 0. In that case, it is safe to
 * cast away constness, and it is cleanest to do that here, all in one place.
 *
 * int TclRegisterNewLiteral(CompileEnv *envPtr, const char *bytes,
 *			     int length);
 */

#define TclRegisterNewLiteral(envPtr, bytes, length) \
    TclRegisterLiteral(envPtr, (char *)(bytes), length, /*flags*/ 0)

/*
 * Form of TclRegisterLiteral with flags == LITERAL_CMD_NAME. In that case, it
 * is safe to cast away constness, and it is cleanest to do that here, all in
 * one place.
 *
 * int TclRegisterNewNSLiteral(CompileEnv *envPtr, const char *bytes,
 *			       int length);
 */

#define TclRegisterNewCmdLiteral(envPtr, bytes, length) \
    TclRegisterLiteral(envPtr, (char *)(bytes), length, LITERAL_CMD_NAME)

/*
 * Macro used to manually adjust the stack requirements; used in cases where
 * the stack effect cannot be computed from the opcode and its operands, but
 * is still known at compile time.
 *
 * void TclAdjustStackDepth(int delta, CompileEnv *envPtr);
 */

#define TclAdjustStackDepth(delta, envPtr) \
    do {								\
	if ((delta) < 0) {						\
	    if ((envPtr)->maxStackDepth < (envPtr)->currStackDepth) {	\
		(envPtr)->maxStackDepth = (envPtr)->currStackDepth;	\
	    }								\
	}								\
	(envPtr)->currStackDepth += (delta);				\
    } while (0)

/*
 * Macro used to update the stack requirements. It is called by the macros
 * TclEmitOpCode, TclEmitInst1 and TclEmitInst4.
 * Remark that the very last instruction of a bytecode always reduces the
 * stack level: INST_DONE or INST_POP, so that the maxStackdepth is always
 * updated.
 *
 * void TclUpdateStackReqs(unsigned char op, int i, CompileEnv *envPtr);
 */

#define TclUpdateStackReqs(op, i, envPtr) \
    do {							\
	int delta = tclInstructionTable[(op)].stackEffect;	\
	if (delta) {						\
	    if (delta == INT_MIN) {				\
		delta = 1 - (i);				\
	    }							\
	    TclAdjustStackDepth(delta, envPtr);			\
	}							\
    } while (0)

/*
 * Macro to emit an opcode byte into a CompileEnv's code array. The ANSI C
 * "prototype" for this macro is:
 *
 * void TclEmitOpcode(unsigned char op, CompileEnv *envPtr);
 */

#define TclEmitOpcode(op, envPtr) \
    do {							\
	if ((envPtr)->codeNext == (envPtr)->codeEnd) {		\
	    TclExpandCodeArray(envPtr);				\
	}							\
	*(envPtr)->codeNext++ = (unsigned char) (op);		\
	TclUpdateStackReqs(op, 0, envPtr);			\
    } while (0)

/*
 * Macros to emit an integer operand. The ANSI C "prototype" for these macros
 * are:
 *
 * void TclEmitInt1(int i, CompileEnv *envPtr);
 * void TclEmitInt4(int i, CompileEnv *envPtr);
 */

#define TclEmitInt1(i, envPtr) \
    do {								\
	if ((envPtr)->codeNext == (envPtr)->codeEnd) {			\
	    TclExpandCodeArray(envPtr);					\
	}								\
	*(envPtr)->codeNext++ = (unsigned char) ((unsigned int) (i));	\
    } while (0)

#define TclEmitInt4(i, envPtr) \
    do {								\
	if (((envPtr)->codeNext + 4) > (envPtr)->codeEnd) {		\
	    TclExpandCodeArray(envPtr);					\
	}								\
	*(envPtr)->codeNext++ =						\
		(unsigned char) ((unsigned int) (i) >> 24);		\
	*(envPtr)->codeNext++ =						\
		(unsigned char) ((unsigned int) (i) >> 16);		\
	*(envPtr)->codeNext++ =						\
		(unsigned char) ((unsigned int) (i) >>  8);		\
	*(envPtr)->codeNext++ =						\
		(unsigned char) ((unsigned int) (i)      );		\
    } while (0)

/*
 * Macros to emit an instruction with signed or unsigned integer operands.
 * Four byte integers are stored in "big-endian" order with the high order
 * byte stored at the lowest address. The ANSI C "prototypes" for these macros
 * are:
 *
 * void TclEmitInstInt1(unsigned char op, int i, CompileEnv *envPtr);
 * void TclEmitInstInt4(unsigned char op, int i, CompileEnv *envPtr);
 */

#define TclEmitInstInt1(op, i, envPtr) \
    do {								\
	if (((envPtr)->codeNext + 2) > (envPtr)->codeEnd) {		\
	    TclExpandCodeArray(envPtr);					\
	}								\
	*(envPtr)->codeNext++ = (unsigned char) (op);			\
	*(envPtr)->codeNext++ = (unsigned char) ((unsigned int) (i));	\
	TclUpdateStackReqs(op, i, envPtr);				\
    } while (0)

#define TclEmitInstInt4(op, i, envPtr) \
    do {							\
	if (((envPtr)->codeNext + 5) > (envPtr)->codeEnd) {	\
	    TclExpandCodeArray(envPtr);				\
	}							\
	*(envPtr)->codeNext++ = (unsigned char) (op);		\
	*(envPtr)->codeNext++ =					\
		(unsigned char) ((unsigned int) (i) >> 24);	\
	*(envPtr)->codeNext++ =					\
		(unsigned char) ((unsigned int) (i) >> 16);	\
	*(envPtr)->codeNext++ =					\
		(unsigned char) ((unsigned int) (i) >>  8);	\
	*(envPtr)->codeNext++ =					\
		(unsigned char) ((unsigned int) (i)      );	\
	TclUpdateStackReqs(op, i, envPtr);			\
    } while (0)

/*
 * Macro to push a Tcl object onto the Tcl evaluation stack. It emits the
 * object's one or four byte array index into the CompileEnv's code array.
 * These support, respectively, a maximum of 256 (2**8) and 2**32 objects in a
 * CompileEnv. The ANSI C "prototype" for this macro is:
 *
 * void	TclEmitPush(int objIndex, CompileEnv *envPtr);
 */

#define TclEmitPush(objIndex, envPtr) \
    do {							 \
	register int objIndexCopy = (objIndex);			 \
	TclEmitInstInt4(INST_PUSH4, objIndexCopy, (envPtr));	 \
    } while (0)

/*
 * Macros to update a (signed or unsigned) integer starting at a pointer. The
 * two variants depend on the number of bytes. The ANSI C "prototypes" for
 * these macros are:
 *
 * void TclStoreInt1AtPtr(int i, unsigned char *p);
 * void TclStoreInt4AtPtr(int i, unsigned char *p);
 */

#define TclStoreInt1AtPtr(i, p) \
    *(p)   = (unsigned char) ((unsigned int) (i))

#define TclStoreInt4AtPtr(i, p) \
    do {							\
	*(p)   = (unsigned char) ((unsigned int) (i) >> 24);	\
	*(p+1) = (unsigned char) ((unsigned int) (i) >> 16);	\
	*(p+2) = (unsigned char) ((unsigned int) (i) >>  8);	\
	*(p+3) = (unsigned char) ((unsigned int) (i)      );	\
    } while (0)

/*
 * Macros to update instructions at a particular pc with a new op code and a
 * (signed or unsigned) int operand. The ANSI C "prototypes" for these macros
 * are:
 *
 * void TclUpdateInstInt1AtPc(unsigned char op, int i, unsigned char *pc);
 * void TclUpdateInstInt4AtPc(unsigned char op, int i, unsigned char *pc);
 */

#define TclUpdateInstInt1AtPc(op, i, pc) \
    do {					\
	*(pc) = (unsigned char) (op);		\
	TclStoreInt1AtPtr((i), ((pc)+1));	\
    } while (0)

#define TclUpdateInstInt4AtPc(op, i, pc) \
    do {					\
	*(pc) = (unsigned char) (op);		\
	TclStoreInt4AtPtr((i), ((pc)+1));	\
    } while (0)


/*
 * Macros to get a signed integer (GET_INT{1,2}) or an unsigned int
 * (GET_UINT{1,2}) from a pointer. There are two variants for each return type
 * that depend on the number of bytes fetched. The ANSI C "prototypes" for
 * these macros are:
 *
 * int TclGetInt1AtPtr(unsigned char *p);
 * int TclGetInt4AtPtr(unsigned char *p);
 * unsigned int TclGetUInt1AtPtr(unsigned char *p);
 * unsigned int TclGetUInt4AtPtr(unsigned char *p);
 */

/*
 * The TclGetInt1AtPtr macro is tricky because we want to do sign extension on
 * the 1-byte value. Unfortunately the "char" type isn't signed on all
 * platforms so sign-extension doesn't always happen automatically. Sometimes
 * we can explicitly declare the pointer to be signed, but other times we have
 * to explicitly sign-extend the value in software.
 */

#ifndef __CHAR_UNSIGNED__
#   define TclGetInt1AtPtr(p) ((int) *((char *) p))
#elif defined(HAVE_SIGNED_CHAR)
#   define TclGetInt1AtPtr(p) ((int) *((signed char *) p))
#else
#   define TclGetInt1AtPtr(p) \
    (((int) *((char *) p)) | ((*(p) & 0200) ? (-256) : 0))
#endif

#define TclGetInt4AtPtr(p) \
    (((int) TclGetInt1AtPtr(p) << 24) |				\
		     (*((p)+1) << 16) |				\
		     (*((p)+2) <<  8) |				\
		     (*((p)+3)))

#define TclGetUInt1AtPtr(p) \
    ((unsigned int) *(p))
#define TclGetUInt4AtPtr(p) \
    ((unsigned int) (*(p)     << 24) |				\
		    (*((p)+1) << 16) |				\
		    (*((p)+2) <<  8) |				\
		    (*((p)+3)))

/*
 * Macros used to compute the minimum and maximum of two integers. The ANSI C
 * "prototypes" for these macros are:
 *
 * int TclMin(int i, int j);
 * int TclMax(int i, int j);
 */

#define TclMin(i, j)	((((int) i) < ((int) j))? (i) : (j))
#define TclMax(i, j)	((((int) i) > ((int) j))? (i) : (j))

/*
 * Convenience macro for use when compiling bodies of commands. The ANSI C
 * "prototype" for this macro is:
 *
 * static void		CompileBody(CompileEnv *envPtr, Tcl_Token *tokenPtr,
 *			    Tcl_Interp *interp);
 */

#define CompileBody(envPtr, tokenPtr, interp) \
    TclCompileCmdWord((interp), (tokenPtr)+1, (tokenPtr)->numComponents, \
	    (envPtr))

/*
 * Convenience macro for use when compiling tokens to be pushed. The ANSI C
 * "prototype" for this macro is:
 *
 * static void		CompileTokens(CompileEnv *envPtr, Tcl_Token *tokenPtr,
 *			    Tcl_Interp *interp);
 */

#define CompileTokens(envPtr, tokenPtr, interp) \
    TclCompileTokens((interp), (tokenPtr)+1, (tokenPtr)->numComponents, \
	    (envPtr));
/*
 * Convenience macro for use when pushing literals. The ANSI C "prototype" for
 * this macro is:
 *
 * static void		PushLiteral(CompileEnv *envPtr,
 *			    const char *string, int length);
 */

#define PushLiteral(envPtr, string, length) \
    TclEmitPush(TclRegisterNewLiteral((envPtr), (string), (length)), (envPtr))

/*
 * Macro to advance to the next token; it is more mnemonic than the address
 * arithmetic that it replaces. The ANSI C "prototype" for this macro is:
 *
 * static Tcl_Token *	TokenAfter(Tcl_Token *tokenPtr);
 */

#define TokenAfter(tokenPtr) \
    ((tokenPtr) + ((tokenPtr)->numComponents + 1))

/*
 * Macro to get the offset to the next instruction to be issued. The ANSI C
 * "prototype" for this macro is:
 *
 * static int	CurrentOffset(CompileEnv *envPtr);
 */

#define CurrentOffset(envPtr) \
    ((envPtr)->codeNext - (envPtr)->codeStart)

/*
 * Check if there is an LVT for compiled locals
 */

#define EnvHasLVT(envPtr) \
    (envPtr->procPtr || envPtr->iPtr->varFramePtr->localCachePtr)

/*
 * Macros for making it easier to deal with tokens and DStrings.
 */

#define TclDStringAppendToken(dsPtr, tokenPtr) \
    Tcl_DStringAppend((dsPtr), (tokenPtr)->start, (tokenPtr)->size)
#define TclRegisterDStringLiteral(envPtr, dsPtr) \
    TclRegisterLiteral(envPtr, Tcl_DStringValue(dsPtr), \
	    Tcl_DStringLength(dsPtr), /*flags*/ 0)

/*
 * DTrace probe macros (NOPs if DTrace support is not enabled).
 */

/*
 * Define the following macros to enable debug logging of the DTrace proc,
 * cmd, and inst probes. Note that this does _not_ require a platform with
 * DTrace, it simply logs all probe output to /tmp/tclDTraceDebug-[pid].log.
 *
 * If the second macro is defined, logging to file starts immediately,
 * otherwise only after the first call to [tcl::dtrace]. Note that the debug
 * probe data is always computed, even when it is not logged to file.
 *
 * Defining the third macro enables debug logging of inst probes (disabled
 * by default due to the significant performance impact).
 */

/*
#define TCL_DTRACE_DEBUG 1
#define TCL_DTRACE_DEBUG_LOG_ENABLED 1
#define TCL_DTRACE_DEBUG_INST_PROBES 1
*/

#if !(defined(TCL_DTRACE_DEBUG) && defined(__GNUC__))

#ifdef USE_DTRACE

#if defined(__GNUC__) && __GNUC__ > 2
/*
 * Use gcc branch prediction hint to minimize cost of DTrace ENABLED checks.
 */
#define unlikely(x) (__builtin_expect((x), 0))
#else
#define unlikely(x) (x)
#endif

#define TCL_DTRACE_PROC_ENTRY_ENABLED()	    unlikely(TCL_PROC_ENTRY_ENABLED())
#define TCL_DTRACE_PROC_RETURN_ENABLED()    unlikely(TCL_PROC_RETURN_ENABLED())
#define TCL_DTRACE_PROC_RESULT_ENABLED()    unlikely(TCL_PROC_RESULT_ENABLED())
#define TCL_DTRACE_PROC_ARGS_ENABLED()	    unlikely(TCL_PROC_ARGS_ENABLED())
#define TCL_DTRACE_PROC_INFO_ENABLED()	    unlikely(TCL_PROC_INFO_ENABLED())
#define TCL_DTRACE_PROC_ENTRY(a0, a1, a2)   TCL_PROC_ENTRY(a0, a1, a2)
#define TCL_DTRACE_PROC_RETURN(a0, a1)	    TCL_PROC_RETURN(a0, a1)
#define TCL_DTRACE_PROC_RESULT(a0, a1, a2, a3) TCL_PROC_RESULT(a0, a1, a2, a3)
#define TCL_DTRACE_PROC_ARGS(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
	TCL_PROC_ARGS(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define TCL_DTRACE_PROC_INFO(a0, a1, a2, a3, a4, a5, a6, a7) \
	TCL_PROC_INFO(a0, a1, a2, a3, a4, a5, a6, a7)

#define TCL_DTRACE_CMD_ENTRY_ENABLED()	    unlikely(TCL_CMD_ENTRY_ENABLED())
#define TCL_DTRACE_CMD_RETURN_ENABLED()	    unlikely(TCL_CMD_RETURN_ENABLED())
#define TCL_DTRACE_CMD_RESULT_ENABLED()	    unlikely(TCL_CMD_RESULT_ENABLED())
#define TCL_DTRACE_CMD_ARGS_ENABLED()	    unlikely(TCL_CMD_ARGS_ENABLED())
#define TCL_DTRACE_CMD_INFO_ENABLED()	    unlikely(TCL_CMD_INFO_ENABLED())
#define TCL_DTRACE_CMD_ENTRY(a0, a1, a2)    TCL_CMD_ENTRY(a0, a1, a2)
#define TCL_DTRACE_CMD_RETURN(a0, a1)	    TCL_CMD_RETURN(a0, a1)
#define TCL_DTRACE_CMD_RESULT(a0, a1, a2, a3) TCL_CMD_RESULT(a0, a1, a2, a3)
#define TCL_DTRACE_CMD_ARGS(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
	TCL_CMD_ARGS(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define TCL_DTRACE_CMD_INFO(a0, a1, a2, a3, a4, a5, a6, a7) \
	TCL_CMD_INFO(a0, a1, a2, a3, a4, a5, a6, a7)

#define TCL_DTRACE_INST_START_ENABLED()	    unlikely(TCL_INST_START_ENABLED())
#define TCL_DTRACE_INST_DONE_ENABLED()	    unlikely(TCL_INST_DONE_ENABLED())
#define TCL_DTRACE_INST_START(a0, a1, a2)   TCL_INST_START(a0, a1, a2)
#define TCL_DTRACE_INST_DONE(a0, a1, a2)    TCL_INST_DONE(a0, a1, a2)

#define TCL_DTRACE_TCL_PROBE_ENABLED()	    unlikely(TCL_TCL_PROBE_ENABLED())
#define TCL_DTRACE_TCL_PROBE(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
	TCL_TCL_PROBE(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9)

#define TCL_DTRACE_DEBUG_LOG()

MODULE_SCOPE void	TclDTraceInfo(Tcl_Obj *info, const char **args,
			    int *argsi);

#else /* USE_DTRACE */

#define TCL_DTRACE_PROC_ENTRY_ENABLED()	    0
#define TCL_DTRACE_PROC_RETURN_ENABLED()    0
#define TCL_DTRACE_PROC_RESULT_ENABLED()    0
#define TCL_DTRACE_PROC_ARGS_ENABLED()	    0
#define TCL_DTRACE_PROC_INFO_ENABLED()	    0
#define TCL_DTRACE_PROC_ENTRY(a0, a1, a2)   {if (a0) {}}
#define TCL_DTRACE_PROC_RETURN(a0, a1)	    {if (a0) {}}
#define TCL_DTRACE_PROC_RESULT(a0, a1, a2, a3) {if (a0) {}; if (a3) {}}
#define TCL_DTRACE_PROC_ARGS(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) {}
#define TCL_DTRACE_PROC_INFO(a0, a1, a2, a3, a4, a5, a6, a7) {}

#define TCL_DTRACE_CMD_ENTRY_ENABLED()	    0
#define TCL_DTRACE_CMD_RETURN_ENABLED()	    0
#define TCL_DTRACE_CMD_RESULT_ENABLED()	    0
#define TCL_DTRACE_CMD_ARGS_ENABLED()	    0
#define TCL_DTRACE_CMD_INFO_ENABLED()	    0
#define TCL_DTRACE_CMD_ENTRY(a0, a1, a2)    {}
#define TCL_DTRACE_CMD_RETURN(a0, a1)	    {}
#define TCL_DTRACE_CMD_RESULT(a0, a1, a2, a3) {}
#define TCL_DTRACE_CMD_ARGS(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) {}
#define TCL_DTRACE_CMD_INFO(a0, a1, a2, a3, a4, a5, a6, a7) {}

#define TCL_DTRACE_INST_START_ENABLED()	    0
#define TCL_DTRACE_INST_DONE_ENABLED()	    0
#define TCL_DTRACE_INST_START(a0, a1, a2)   {}
#define TCL_DTRACE_INST_DONE(a0, a1, a2)    {}

#define TCL_DTRACE_TCL_PROBE_ENABLED()	    0
#define TCL_DTRACE_TCL_PROBE(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) {}

#define TclDTraceInfo(info, args, argsi)    {*args = ""; *argsi = 0;}

#endif /* USE_DTRACE */

#else /* TCL_DTRACE_DEBUG */

#define USE_DTRACE 1

#if !defined(TCL_DTRACE_DEBUG_LOG_ENABLED) || !(TCL_DTRACE_DEBUG_LOG_ENABLED)
#undef TCL_DTRACE_DEBUG_LOG_ENABLED
#define TCL_DTRACE_DEBUG_LOG_ENABLED 0
#endif

#if !defined(TCL_DTRACE_DEBUG_INST_PROBES) || !(TCL_DTRACE_DEBUG_INST_PROBES)
#undef TCL_DTRACE_DEBUG_INST_PROBES
#define TCL_DTRACE_DEBUG_INST_PROBES 0
#endif

MODULE_SCOPE int tclDTraceDebugEnabled, tclDTraceDebugIndent;
MODULE_SCOPE FILE *tclDTraceDebugLog;
MODULE_SCOPE void TclDTraceOpenDebugLog(void);
MODULE_SCOPE void TclDTraceInfo(Tcl_Obj *info, const char **args, int *argsi);

#define TCL_DTRACE_DEBUG_LOG() \
    int tclDTraceDebugEnabled = TCL_DTRACE_DEBUG_LOG_ENABLED;	\
    int tclDTraceDebugIndent = 0;				\
    FILE *tclDTraceDebugLog = NULL;				\
    void TclDTraceOpenDebugLog(void) {				\
	char n[35];						\
	sprintf(n, "/tmp/tclDTraceDebug-%lu.log",		\
		(unsigned long) getpid());			\
	tclDTraceDebugLog = fopen(n, "a");			\
    }

#define TclDTraceDbgMsg(p, m, ...) \
    do {								\
	if (tclDTraceDebugEnabled) {					\
	    int _l, _t = 0;						\
	    if (!tclDTraceDebugLog) { TclDTraceOpenDebugLog(); }	\
	    fprintf(tclDTraceDebugLog, "%.12s:%.4d:%n",			\
		    strrchr(__FILE__, '/')+1, __LINE__, &_l); _t += _l; \
	    fprintf(tclDTraceDebugLog, " %.*s():%n",			\
		    (_t < 18 ? 18 - _t : 0) + 18, __func__, &_l); _t += _l; \
	    fprintf(tclDTraceDebugLog, "%*s" p "%n",			\
		    (_t < 40 ? 40 - _t : 0) + 2 * tclDTraceDebugIndent, \
		    "", &_l); _t += _l;					\
	    fprintf(tclDTraceDebugLog, "%*s" m "\n",			\
		    (_t < 64 ? 64 - _t : 1), "", ##__VA_ARGS__);	\
	    fflush(tclDTraceDebugLog);					\
	}								\
    } while (0)

#define TCL_DTRACE_PROC_ENTRY_ENABLED()	    1
#define TCL_DTRACE_PROC_RETURN_ENABLED()    1
#define TCL_DTRACE_PROC_RESULT_ENABLED()    1
#define TCL_DTRACE_PROC_ARGS_ENABLED()	    1
#define TCL_DTRACE_PROC_INFO_ENABLED()	    1
#define TCL_DTRACE_PROC_ENTRY(a0, a1, a2) \
	tclDTraceDebugIndent++; \
	TclDTraceDbgMsg("-> proc-entry", "%s %d %p", a0, a1, a2)
#define TCL_DTRACE_PROC_RETURN(a0, a1) \
	TclDTraceDbgMsg("<- proc-return", "%s %d", a0, a1); \
	tclDTraceDebugIndent--
#define TCL_DTRACE_PROC_RESULT(a0, a1, a2, a3) \
	TclDTraceDbgMsg(" | proc-result", "%s %d %s %p", a0, a1, a2, a3)
#define TCL_DTRACE_PROC_ARGS(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
	TclDTraceDbgMsg(" | proc-args", "%s %s %s %s %s %s %s %s %s %s", a0, \
		a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define TCL_DTRACE_PROC_INFO(a0, a1, a2, a3, a4, a5, a6, a7) \
	TclDTraceDbgMsg(" | proc-info", "%s %s %s %s %d %d %s %s", a0, a1, \
		a2, a3, a4, a5, a6, a7)

#define TCL_DTRACE_CMD_ENTRY_ENABLED()	    1
#define TCL_DTRACE_CMD_RETURN_ENABLED()	    1
#define TCL_DTRACE_CMD_RESULT_ENABLED()	    1
#define TCL_DTRACE_CMD_ARGS_ENABLED()	    1
#define TCL_DTRACE_CMD_INFO_ENABLED()	    1
#define TCL_DTRACE_CMD_ENTRY(a0, a1, a2) \
	tclDTraceDebugIndent++; \
	TclDTraceDbgMsg("-> cmd-entry", "%s %d %p", a0, a1, a2)
#define TCL_DTRACE_CMD_RETURN(a0, a1) \
	TclDTraceDbgMsg("<- cmd-return", "%s %d", a0, a1); \
	tclDTraceDebugIndent--
#define TCL_DTRACE_CMD_RESULT(a0, a1, a2, a3) \
	TclDTraceDbgMsg(" | cmd-result", "%s %d %s %p", a0, a1, a2, a3)
#define TCL_DTRACE_CMD_ARGS(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
	TclDTraceDbgMsg(" | cmd-args", "%s %s %s %s %s %s %s %s %s %s", a0, \
		a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define TCL_DTRACE_CMD_INFO(a0, a1, a2, a3, a4, a5, a6, a7) \
	TclDTraceDbgMsg(" | cmd-info", "%s %s %s %s %d %d %s %s", a0, a1, \
		a2, a3, a4, a5, a6, a7)

#define TCL_DTRACE_INST_START_ENABLED()	    TCL_DTRACE_DEBUG_INST_PROBES
#define TCL_DTRACE_INST_DONE_ENABLED()	    TCL_DTRACE_DEBUG_INST_PROBES
#define TCL_DTRACE_INST_START(a0, a1, a2) \
	TclDTraceDbgMsg(" | inst-start", "%s %d %p", a0, a1, a2)
#define TCL_DTRACE_INST_DONE(a0, a1, a2) \
	TclDTraceDbgMsg(" | inst-end", "%s %d %p", a0, a1, a2)

#define TCL_DTRACE_TCL_PROBE_ENABLED()	    1
#define TCL_DTRACE_TCL_PROBE(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
    do {								\
	tclDTraceDebugEnabled = 1;					\
	TclDTraceDbgMsg(" | tcl-probe", "%s %s %s %s %s %s %s %s %s %s", a0, \
		a1, a2, a3, a4, a5, a6, a7, a8, a9);			\
    } while (0)

#endif /* TCL_DTRACE_DEBUG */

#endif /* _TCLCOMPILATIONINT */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
