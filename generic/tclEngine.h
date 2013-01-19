#ifndef _TCLENGINE
#define _TCLENGINE 1

/*
 * ClientData type used by the math operator commands.
 */

typedef struct {
    const char *op;		/* Do not call it 'operator': C++ reserved */
    const char *expected;
    union {
	int numArgs;
	int identity;
    } i;
} TclOpCmdClientData;

MODULE_SCOPE int	TclSingleOpCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
MODULE_SCOPE int	TclSortingOpCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
MODULE_SCOPE int	TclVariadicOpCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
MODULE_SCOPE int	TclNoIdentOpCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);

MODULE_SCOPE void	TclReleaseLiteral(Tcl_Interp *interp, Tcl_Obj *objPtr);
MODULE_SCOPE Tcl_Obj *	TclCreateLiteral(Interp *iPtr, char *bytes,
			    int length, unsigned int hash, int *newPtr,
			    Namespace *nsPtr, int flags,
			    LiteralEntry **globalPtrPtr);

/*
 * The structure defining the bytecode instructions resulting from compiling a
 * Tcl script. Note that this structure is variable length: a single heap
 * object is allocated to hold the ByteCode structure immediately followed by
 * the code bytes, the literal object array, the ExceptionRange array, the
 * CmdLocation map, and the compilation AuxData array.
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
    int compileEpoch;		/* Value of iPtr->compileEpoch when this
				 * ByteCode was compiled. Used to invalidate
				 * code when, e.g., commands with compile
				 * procs are redefined. */
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
    LocalCache *localCachePtr;	/* Pointer to the start of the cached variable
				 * names and initialisation data for local
				 * variables. */
#ifdef TCL_COMPILE_STATS
    Tcl_Time createTime;	/* Absolute time when the ByteCode was
				 * created. */
#endif /* TCL_COMPILE_STATS */
    size_t structureSize;	/* Number of bytes in the ByteCode structure
				 * itself. Does not include heap space for
				 * literal Tcl objects or storage referenced
				 * by AuxData entries. */
    int numCommands;		/* Number of commands compiled. */
    int numSrcBytes;		/* Number of source bytes compiled. */
    int numCodeBytes;		/* Number of code bytes. */
    int numLitObjects;		/* Number of objects in literal array. */
    int numExceptRanges;	/* Number of ExceptionRange array elems. */
    int numAuxDataItems;	/* Number of AuxData items. */
    int numCmdLocBytes;		/* Number of bytes needed for encoded command
				 * location information. */
    int maxExceptDepth;		/* Maximum nesting level of ExceptionRanges;
				 * -1 if no ranges were compiled. */
    int maxStackDepth;		/* Maximum number of stack elements needed to
				 * execute the code. */
    unsigned char *codeStart;	/* Points to the first byte of the code. This
				 * is just after the final ByteCode member
				 * cmdMapPtr. */
    Tcl_Obj **objArrayPtr;	/* Points to the start of the literal object
				 * array. This is just after the last code
				 * byte. */
    struct ExceptionRange *exceptArrayPtr;
    				/* Points to the start of the ExceptionRange
				 * array. This is just after the last object
				 * in the object array. */
    struct AuxData *auxDataArrayPtr;	/* Points to the start of the auxiliary data
				 * array. This is just after the last entry in
				 * the ExceptionRange array. */
    unsigned char *codeDeltaStart;
				/* Points to the first of a sequence of bytes
				 * that encode the change in the starting
				 * offset of each command's code. If -127 <=
				 * delta <= 127, it is encoded as 1 byte,
				 * otherwise 0xFF (128) appears and the delta
				 * is encoded by the next 4 bytes. Code deltas
				 * are always positive. This sequence is just
				 * after the last entry in the AuxData
				 * array. */
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
} ByteCode;
    
/*
 * The type of procedure called from the compilation hook point in
 * SetByteCodeFromAny.
 */
typedef struct CompileEnv CompileEnv;
typedef int (CompileHookProc)(Tcl_Interp *interp,
	CompileEnv *compEnvPtr, ClientData clientData);

MODULE_SCOPE int TclCompileToCompiledCommand(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, int depth, Command *cmdPtr,
			    struct CompileEnv *envPtr);
MODULE_SCOPE void TclCompileToInvokedCommand(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, Tcl_Obj *replacements,
			    Command *cmdPtr, struct CompileEnv *envPtr);
MODULE_SCOPE int TclCompileBasicNArgCommand(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, Command *cmdPtr,
			    struct CompileEnv *envPtr);

typedef struct ByteCode ByteCode;		/* Forward declaration. */

MODULE_SCOPE int	TclNRExecuteByteCode(Tcl_Interp *interp,
			    ByteCode *codePtr);

MODULE_SCOPE int TclSetByteCodeFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr,
	CompileHookProc *hookProc, ClientData clientData);

MODULE_SCOPE ExecEnv *	TclCreateExecEnv(Tcl_Interp *interp, int size);
MODULE_SCOPE void	TclInitCompileEnv(Tcl_Interp *interp,
			    CompileEnv *envPtr, const char *string,
			    int numBytes);
MODULE_SCOPE void	TclDeleteExecEnv(ExecEnv *eePtr);

MODULE_SCOPE void	TclInvalidateCmdLiteral(Tcl_Interp *interp, 
			    const char *name, Namespace *nsPtr);
MODULE_SCOPE void	TclInitLiteralTable(LiteralTable *tablePtr);
MODULE_SCOPE void	TclDeleteLiteralTable(Tcl_Interp *interp,
			    LiteralTable *tablePtr);
/*
 *----------------------------------------------------------------
 * Procedures exported by the engine to be used by tclBasic.c
 *----------------------------------------------------------------
 */

MODULE_SCOPE ByteCode *	TclCompileObj(Tcl_Interp *interp, Tcl_Obj *objPtr);


#ifdef TCL_COMPILE_STATS
MODULE_SCOPE char *	TclLiteralStats(LiteralTable *tablePtr);
MODULE_SCOPE int	TclLog2(int value);
#endif

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

#endif
