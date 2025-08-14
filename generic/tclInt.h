/*
 * tclInt.h --
 *
 *	Declarations of things used internally by the Tcl interpreter.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1993-1997 Lucent Technologies.
 * Copyright (c) 1994-1998 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * Copyright (c) 2001, 2002 by Kevin B. Kenny.  All rights reserved.
 * Copyright (c) 2007 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright (c) 2006-2008 by Joe Mistachkin.  All rights reserved.
 * Copyright (c) 2008 by Miguel Sofer. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLINT
#define _TCLINT

/*
 * Some numerics configuration options.
 */

#undef ACCEPT_NAN

/*
 * Used to tag functions that are only to be visible within the module being
 * built and not outside it (where this is supported by the linker).
 * Also used in the platform-specific *Port.h files.
 */

#ifndef MODULE_SCOPE
#   ifdef __cplusplus
#	define MODULE_SCOPE extern "C"
#   else
#	define MODULE_SCOPE extern
#   endif
#endif

#ifndef JOIN
#  define JOIN(a,b) JOIN1(a,b)
#  define JOIN1(a,b) a##b
#endif

#if defined(__cplusplus)
#   define TCL_UNUSED(T) T
#elif defined(__GNUC__) && (__GNUC__ > 2)
#   define TCL_UNUSED(T) T JOIN(dummy, __LINE__) __attribute__((unused))
#else
#   define TCL_UNUSED(T) T JOIN(dummy, __LINE__)
#endif

/*
 * Common include files needed by most of the Tcl source files are included
 * here, so that system-dependent personalizations for the include files only
 * have to be made in once place. This results in a few extra includes, but
 * greater modularity. The order of the three groups of #includes is
 * important. For example, stdio.h is needed by tcl.h.
 */

#include "tclPort.h"

#include <stdio.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <locale.h>

/*
 * Ensure WORDS_BIGENDIAN is defined correctly:
 * Needs to happen here in addition to configure to work with fat compiles on
 * Darwin (where configure runs only once for multiple architectures).
 */

#ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#    include <sys/param.h>
#endif
#ifdef BYTE_ORDER
#    ifdef BIG_ENDIAN
#	 if BYTE_ORDER == BIG_ENDIAN
#	     undef WORDS_BIGENDIAN
#	     define WORDS_BIGENDIAN 1
#	 endif
#    endif
#    ifdef LITTLE_ENDIAN
#	 if BYTE_ORDER == LITTLE_ENDIAN
#	     undef WORDS_BIGENDIAN
#	 endif
#    endif
#endif

/*
 * Macros used to cast between pointers and integers (e.g. when storing an int
 * in ClientData), on 64-bit architectures they avoid gcc warning about "cast
 * to/from pointer from/to integer of different size".
 */

#if !defined(INT2PTR)
#   define INT2PTR(p) ((void *)(ptrdiff_t)(p))
#endif
#if !defined(PTR2INT)
#   define PTR2INT(p) ((ptrdiff_t)(p))
#endif
#if !defined(UINT2PTR)
#   define UINT2PTR(p) ((void *)(size_t)(p))
#endif
#if !defined(PTR2UINT)
#   define PTR2UINT(p) ((size_t)(p))
#endif

#if defined(_WIN32) && defined(_MSC_VER)
#   define vsnprintf _vsnprintf
#   define snprintf _snprintf
#endif

#if !defined(TCL_THREADS)
#   define TCL_THREADS 1
#endif
#if !TCL_THREADS
#   undef TCL_DECLARE_MUTEX
#   define TCL_DECLARE_MUTEX(name)
#   undef  Tcl_MutexLock
#   define Tcl_MutexLock(mutexPtr)
#   undef  Tcl_MutexUnlock
#   define Tcl_MutexUnlock(mutexPtr)
#   undef  Tcl_MutexFinalize
#   define Tcl_MutexFinalize(mutexPtr)
#   undef  Tcl_ConditionNotify
#   define Tcl_ConditionNotify(condPtr)
#   undef  Tcl_ConditionWait
#   define Tcl_ConditionWait(condPtr, mutexPtr, timePtr)
#   undef  Tcl_ConditionFinalize
#   define Tcl_ConditionFinalize(condPtr)
#endif

// A way to mark a code path as unreachable.
#ifndef TCL_UNREACHABLE
#if defined(__STDC__) && __STDC__ >= 202311L
#include <stddef.h>
#define TCL_UNREACHABLE()	unreachable()
#elif defined(__GNUC__)
#define TCL_UNREACHABLE()	__builtin_unreachable()
#elif defined(_MSC_VER)
#include <stdbool.h>
#define TCL_UNREACHABLE()	__assume(false)
#else
#define TCL_UNREACHABLE()	((void) 0)
#endif
#endif // TCL_UNREACHABLE

#ifndef TCL_FALLTHROUGH
#if defined(__STDC__) && __STDC__ >= 202311L
#define TCL_FALLTHROUGH()	[[fallthrough]]
#elif defined(__GNUC__)
#define TCL_FALLTHROUGH()	__attribute__((fallthrough))
#else
// Nothing documented as an alternative to the standard [[fallthrough]].
#define TCL_FALLTHROUGH()	((void) 0)
#endif
#endif // TCL_FALLTHROUGH

/*
 * The following procedures allow namespaces to be customized to support
 * special name resolution rules for commands/variables.
 */

struct Tcl_ResolvedVarInfo;

typedef Tcl_Var (Tcl_ResolveRuntimeVarProc)(Tcl_Interp *interp,
	struct Tcl_ResolvedVarInfo *vinfoPtr);

typedef void (Tcl_ResolveVarDeleteProc)(struct Tcl_ResolvedVarInfo *vinfoPtr);

/*
 * The following structure encapsulates the routines needed to resolve a
 * variable reference at runtime. Any variable specific state will typically
 * be appended to this structure.
 */

typedef struct Tcl_ResolvedVarInfo {
    Tcl_ResolveRuntimeVarProc *fetchProc;
    Tcl_ResolveVarDeleteProc *deleteProc;
} Tcl_ResolvedVarInfo;

typedef int (Tcl_ResolveCompiledVarProc)(Tcl_Interp *interp,
	const char *name, Tcl_Size length, Tcl_Namespace *context,
	Tcl_ResolvedVarInfo **rPtr);

typedef int (Tcl_ResolveVarProc)(Tcl_Interp *interp, const char *name,
	Tcl_Namespace *context, int flags, Tcl_Var *rPtr);

typedef int (Tcl_ResolveCmdProc)(Tcl_Interp *interp, const char *name,
	Tcl_Namespace *context, int flags, Tcl_Command *rPtr);

typedef struct Tcl_ResolverInfo {
    Tcl_ResolveCmdProc *cmdResProc;
				/* Procedure handling command name
				 * resolution. */
    Tcl_ResolveVarProc *varResProc;
				/* Procedure handling variable name resolution
				 * for variables that can only be handled at
				 * runtime. */
    Tcl_ResolveCompiledVarProc *compiledVarResProc;
				/* Procedure handling variable name resolution
				 * at compile time. */
} Tcl_ResolverInfo;

/*
 * This flag bit should not interfere with TCL_GLOBAL_ONLY,
 * TCL_NAMESPACE_ONLY, or TCL_LEAVE_ERR_MSG; it signals that the variable
 * lookup is performed for upvar (or similar) purposes, with slightly
 * different rules:
 *    - Bug #696893 - variable is either proc-local or in the current
 *	namespace; never follow the second (global) resolution path
 *    - Bug #631741 - do not use special namespace or interp resolvers
 */

#define TCL_AVOID_RESOLVERS 0x40000

/*
 *----------------------------------------------------------------
 * Data structures related to namespaces.
 *----------------------------------------------------------------
 */

typedef struct Tcl_Ensemble Tcl_Ensemble;
typedef struct NamespacePathEntry NamespacePathEntry;

/*
 * Special hashtable for variables:  This is just a Tcl_HashTable with nsPtr
 * and arrayPtr fields added at the end so that variables can find their
 * namespace and possibly containing array without having to copy a pointer in
 * their struct by accessing them via their hPtr->tablePtr.
 */

typedef struct TclVarHashTable {
    Tcl_HashTable table;	/* "Inherit" from Tcl_HashTable. */
    struct Namespace *nsPtr;	/* The namespace containing the variables. */
    struct Var *arrayPtr;	/* The array containing the variables, if they
				 * are variables in an array at all. */
} TclVarHashTable;

/*
 * Define this to reduce the amount of space that the average namespace
 * consumes by only allocating the table of child namespaces when necessary.
 * Defining it breaks compatibility for Tcl extensions (e.g., itcl) which
 * reach directly into the Namespace structure.
 */

#undef BREAK_NAMESPACE_COMPAT

/*
 * The structure below defines a namespace.
 * Note: the first five fields must match exactly the fields in a
 * Tcl_Namespace structure (see tcl.h). If you change one, be sure to change
 * the other.
 */

typedef struct Namespace {
    char *name;			/* The namespace's simple (unqualified) name.
				 * This contains no ::'s. The name of the
				 * global namespace is "" although "::" is an
				 * synonym. */
    char *fullName;		/* The namespace's fully qualified name. This
				 * starts with ::. */
    void *clientData;		/* An arbitrary value associated with this
				 * namespace. */
    Tcl_NamespaceDeleteProc *deleteProc;
				/* Procedure invoked when deleting the
				 * namespace to, e.g., free clientData. */
    struct Namespace *parentPtr;/* Points to the namespace that contains this
				 * one. NULL if this is the global
				 * namespace. */
#ifndef BREAK_NAMESPACE_COMPAT
    Tcl_HashTable childTable;	/* Contains any child namespaces. Indexed by
				 * strings; values have type (Namespace *). */
#else
    Tcl_HashTable *childTablePtr;
				/* Contains any child namespaces. Indexed by
				 * strings; values have type (Namespace *). If
				 * NULL, there are no children. */
#endif
    size_t nsId;		/* Unique id for the namespace. */
    Tcl_Interp *interp;		/* The interpreter containing this
				 * namespace. */
    int flags;			/* OR-ed combination of the namespace status
				 * flags NS_DYING and NS_DEAD listed below. */
    Tcl_Size activationCount;	/* Number of "activations" or active call
				 * frames for this namespace that are on the
				 * Tcl call stack. The namespace won't be
				 * freed until activationCount becomes zero. */
    Tcl_Size refCount;		/* Count of references by namespaceName
				 * objects. The namespace can't be freed until
				 * refCount becomes zero. */
    Tcl_HashTable cmdTable;	/* Contains all the commands currently
				 * registered in the namespace. Indexed by
				 * strings; values have type (Command *).
				 * Commands imported by Tcl_Import have
				 * Command structures that point (via an
				 * ImportedCmdRef structure) to the Command
				 * structure in the source namespace's command
				 * table. */
    TclVarHashTable varTable;	/* Contains all the (global) variables
				 * currently in this namespace. Indexed by
				 * strings; values have type (Var *). */
    char **exportArrayPtr;	/* Points to an array of string patterns
				 * specifying which commands are exported. A
				 * pattern may include "string match" style
				 * wildcard characters to specify multiple
				 * commands; however, no namespace qualifiers
				 * are allowed. NULL if no export patterns are
				 * registered. */
    Tcl_Size numExportPatterns;	/* Number of export patterns currently
				 * registered using "namespace export". */
    Tcl_Size maxExportPatterns;	/* Number of export patterns for which space
				 * is currently allocated. */
    Tcl_Size cmdRefEpoch;	/* Incremented if a newly added command
				 * shadows a command for which this namespace
				 * has already cached a Command* pointer; this
				 * causes all its cached Command* pointers to
				 * be invalidated. */
    Tcl_Size resolverEpoch;	/* Incremented whenever (a) the name
				 * resolution rules change for this namespace
				 * or (b) a newly added command shadows a
				 * command that is compiled to bytecodes. This
				 * invalidates all byte codes compiled in the
				 * namespace, causing the code to be
				 * recompiled under the new rules.*/
    Tcl_ResolveCmdProc *cmdResProc;
				/* If non-null, this procedure overrides the
				 * usual command resolution mechanism in Tcl.
				 * This procedure is invoked within
				 * Tcl_FindCommand to resolve all command
				 * references within the namespace. */
    Tcl_ResolveVarProc *varResProc;
				/* If non-null, this procedure overrides the
				 * usual variable resolution mechanism in Tcl.
				 * This procedure is invoked within
				 * Tcl_FindNamespaceVar to resolve all
				 * variable references within the namespace at
				 * runtime. */
    Tcl_ResolveCompiledVarProc *compiledVarResProc;
				/* If non-null, this procedure overrides the
				 * usual variable resolution mechanism in Tcl.
				 * This procedure is invoked within
				 * LookupCompiledLocal to resolve variable
				 * references within the namespace at compile
				 * time. */
    Tcl_Size exportLookupEpoch;	/* Incremented whenever a command is added to
				 * a namespace, removed from a namespace or
				 * the exports of a namespace are changed.
				 * Allows TIP#112-driven command lists to be
				 * validated efficiently. */
    Tcl_Ensemble *ensembles;	/* List of structures that contain the details
				 * of the ensembles that are implemented on
				 * top of this namespace. */
    Tcl_Obj *unknownHandlerPtr;	/* A script fragment to be used when command
				 * resolution in this namespace fails. TIP
				 * 181. */
    Tcl_Size commandPathLength;	/* The length of the explicit path. */
    NamespacePathEntry *commandPathArray;
				/* The explicit path of the namespace as an
				 * array. */
    NamespacePathEntry *commandPathSourceList;
				/* Linked list of path entries that point to
				 * this namespace. */
    Tcl_NamespaceDeleteProc *earlyDeleteProc;
				/* Just like the deleteProc field (and called
				 * with the same clientData) but called at the
				 * start of the deletion process, so there is
				 * a chance for code to do stuff inside the
				 * namespace before deletion completes. */
} Namespace;

/*
 * An entry on a namespace's command resolution path.
 */

struct NamespacePathEntry {
    Namespace *nsPtr;		/* What does this path entry point to? If it
				 * is NULL, this path entry points is
				 * redundant and should be skipped. */
    Namespace *creatorNsPtr;	/* Where does this path entry point from? This
				 * allows for efficient invalidation of
				 * references when the path entry's target
				 * updates its current list of defined
				 * commands. */
    NamespacePathEntry *prevPtr, *nextPtr;
				/* Linked list pointers or NULL at either end
				 * of the list that hangs off Namespace's
				 * commandPathSourceList field. */
};

/*
 * Flags used to represent the status of a namespace:
 *
 * NS_DYING -	1 means Tcl_DeleteNamespace has been called to delete the
 *		namespace.  There may still be active call frames on the Tcl
 *		stack that refer to the namespace. When the last call frame
 *		referring to it has been popped, its remaining variables and
 *		commands are destroyed and it is marked "dead" (NS_DEAD).
 * NS_TEARDOWN  -1 means that TclTeardownNamespace has already been called on
 *		this namespace and it should not be called again [Bug 1355942].
 * NS_DEAD -	1 means Tcl_DeleteNamespace has been called to delete the
 *		namespace and no call frames still refer to it. It is no longer
 *		accessible by name. Its variables and commands have already
 *		been destroyed.  When the last namespaceName object in any byte
 *		code unit that refers to the namespace has been freed (i.e.,
 *		when the namespace's refCount is 0), the namespace's storage
 *		will be freed.
 * NS_SUPPRESS_COMPILATION -
 *		Marks the commands in this namespace for not being compiled,
 *		forcing them to be looked up every time.
 */

#define NS_DYING	0x01
#define NS_DEAD		0x02
#define NS_TEARDOWN	0x04
#define NS_KILLED	0x04 /* Same as NS_TEARDOWN (Deprecated) */
#define NS_SUPPRESS_COMPILATION	0x08

/*
 * Flags passed to TclGetNamespaceForQualName:
 *
 * TCL_GLOBAL_ONLY		- (see tcl.h) Look only in the global ns.
 * TCL_NAMESPACE_ONLY		- (see tcl.h) Look only in the context ns.
 * TCL_CREATE_NS_IF_UNKNOWN	- Create unknown namespaces.
 * TCL_FIND_ONLY_NS		- The name sought is a namespace name.
 * TCL_FIND_IF_NOT_SIMPLE	- Retrieve last namespace even if the rest of
 *				  name is not simple name (contains ::).
 */

#define TCL_CREATE_NS_IF_UNKNOWN	0x800
#define TCL_FIND_ONLY_NS		0x1000
#define TCL_FIND_IF_NOT_SIMPLE		0x2000

/*
 * The client data for an ensemble command. This consists of the table of
 * commands that are actually exported by the namespace, and an epoch counter
 * that, combined with the exportLookupEpoch field of the namespace structure,
 * defines whether the table contains valid data or will need to be recomputed
 * next time the ensemble command is called.
 */

typedef struct EnsembleConfig {
    Namespace *nsPtr;		/* The namespace backing this ensemble up. */
    Tcl_Command token;		/* The token for the command that provides
				 * ensemble support for the namespace, or NULL
				 * if the command has been deleted (or never
				 * existed; the global namespace never has an
				 * ensemble command.) */
    Tcl_Size epoch;		/* The epoch at which this ensemble's table of
				 * exported commands is valid. */
    char **subcommandArrayPtr;	/* Array of ensemble subcommand names. At all
				 * consistent points, this will have the same
				 * number of entries as there are entries in
				 * the subcommandTable hash. */
    Tcl_HashTable subcommandTable;
				/* Hash table of ensemble subcommand names,
				 * which are its keys so this also provides
				 * the storage management for those subcommand
				 * names. The contents of the entry values are
				 * object version the prefix lists to use when
				 * substituting for the command/subcommand to
				 * build the ensemble implementation command.
				 * Has to be stored here as well as in
				 * subcommandDict because that field is NULL
				 * when we are deriving the ensemble from the
				 * namespace exports list. FUTURE WORK: use
				 * object hash table here. */
    struct EnsembleConfig *next;/* The next ensemble in the linked list of
				 * ensembles associated with a namespace. If
				 * this field points to this ensemble, the
				 * structure has already been unlinked from
				 * all lists, and cannot be found by scanning
				 * the list from the namespace's ensemble
				 * field. */
    int flags;			/* OR'ed combo of TCL_ENSEMBLE_PREFIX,
				 * ENSEMBLE_DEAD and ENSEMBLE_COMPILE. */

    /* OBJECT FIELDS FOR ENSEMBLE CONFIGURATION */

    Tcl_Obj *subcommandDict;	/* Dictionary providing mapping from
				 * subcommands to their implementing command
				 * prefixes, or NULL if we are to build the
				 * map automatically from the namespace
				 * exports. */
    Tcl_Obj *subcmdList;	/* List of commands that this ensemble
				 * actually provides, and whose implementation
				 * will be built using the subcommandDict (if
				 * present and defined) and by simple mapping
				 * to the namespace otherwise. If NULL,
				 * indicates that we are using the (dynamic)
				 * list of currently exported commands. */
    Tcl_Obj *unknownHandler;	/* Script prefix used to handle the case when
				 * no match is found (according to the rule
				 * defined by flag bit TCL_ENSEMBLE_PREFIX) or
				 * NULL to use the default error-generating
				 * behaviour. The script execution gets all
				 * the arguments to the ensemble command
				 * (including objv[0]) and will have the
				 * results passed directly back to the caller
				 * (including the error code) unless the code
				 * is TCL_CONTINUE in which case the
				 * subcommand will be re-parsed by the ensemble
				 * core, presumably because the ensemble
				 * itself has been updated. */
    Tcl_Obj *parameterList;	/* List of ensemble parameter names. */
    Tcl_Size numParameters;	/* Cached number of parameters. This is either
				 * 0 (if the parameterList field is NULL) or
				 * the length of the list in the parameterList
				 * field. */
} EnsembleConfig;

/*
 * Various bits for the EnsembleConfig.flags field.
 */

#define ENSEMBLE_DEAD	0x1	/* Flag value to say that the ensemble is dead
				 * and on its way out. */
#define ENSEMBLE_COMPILE 0x4	/* Flag to enable bytecode compilation of an
				 * ensemble. */

/*
 *----------------------------------------------------------------
 * Data structures related to variables. These are used primarily in tclVar.c
 *----------------------------------------------------------------
 */

/*
 * The following structure defines a variable trace, which is used to invoke a
 * specific C procedure whenever certain operations are performed on a
 * variable.
 */

typedef struct VarTrace {
    Tcl_VarTraceProc *traceProc;/* Procedure to call when operations given by
				 * flags are performed on variable. */
    void *clientData;		/* Argument to pass to proc. */
    int flags;			/* What events the trace procedure is
				 * interested in: OR-ed combination of
				 * TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS and TCL_TRACE_ARRAY. */
    struct VarTrace *nextPtr;	/* Next in list of traces associated with a
				 * particular variable. */
} VarTrace;

/*
 * The following structure defines a command trace, which is used to invoke a
 * specific C procedure whenever certain operations are performed on a
 * command.
 */

typedef struct CommandTrace {
    Tcl_CommandTraceProc *traceProc;
				/* Procedure to call when operations given by
				 * flags are performed on command. */
    void *clientData;		/* Argument to pass to proc. */
    int flags;			/* What events the trace procedure is
				 * interested in: OR-ed combination of
				 * TCL_TRACE_RENAME, TCL_TRACE_DELETE. */
    struct CommandTrace *nextPtr;
				/* Next in list of traces associated with a
				 * particular command. */
    Tcl_Size refCount;		/* Used to ensure this structure is not
				 * deleted too early. Keeps track of how many
				 * pieces of code have a pointer to this
				 * structure. */
} CommandTrace;

/*
 * When a command trace is active (i.e. its associated procedure is executing)
 * one of the following structures is linked into a list associated with the
 * command's interpreter. The information in the structure is needed in order
 * for Tcl to behave reasonably if traces are deleted while traces are active.
 */

typedef struct ActiveCommandTrace {
    struct Command *cmdPtr;	/* Command that's being traced. */
    struct ActiveCommandTrace *nextPtr;
				/* Next in list of all active command traces
				 * for the interpreter, or NULL if no more. */
    CommandTrace *nextTracePtr;	/* Next trace to check after current trace
				 * procedure returns; if this trace gets
				 * deleted, must update pointer to avoid using
				 * free'd memory. */
    int reverseScan;		/* Boolean set true when traces are scanning
				 * in reverse order. */
} ActiveCommandTrace;

/*
 * When a variable trace is active (i.e. its associated procedure is
 * executing) one of the following structures is linked into a list associated
 * with the variable's interpreter. The information in the structure is needed
 * in order for Tcl to behave reasonably if traces are deleted while traces
 * are active.
 */

typedef struct ActiveVarTrace {
    struct Var *varPtr;		/* Variable that's being traced. */
    struct ActiveVarTrace *nextPtr;
				/* Next in list of all active variable traces
				 * for the interpreter, or NULL if no more. */
    VarTrace *nextTracePtr;	/* Next trace to check after current trace
				 * procedure returns; if this trace gets
				 * deleted, must update pointer to avoid using
				 * free'd memory. */
} ActiveVarTrace;

/*
 * The structure below defines a variable, which associates a string name with
 * a Tcl_Obj value. These structures are kept in procedure call frames (for
 * local variables recognized by the compiler) or in the heap (for global
 * variables and any variable not known to the compiler). For each Var
 * structure in the heap, a hash table entry holds the variable name and a
 * pointer to the Var structure.
 */

typedef struct Var {
    int flags;			/* Miscellaneous bits of information about
				 * variable. See below for definitions. */
    union {
	Tcl_Obj *objPtr;	/* The variable's object value. Used for
				 * scalar variables and array elements. */
	TclVarHashTable *tablePtr;/* For array variables, this points to
				 * information about the hash table used to
				 * implement the associative array. Points to
				 * Tcl_Alloc-ed data. */
	struct Var *linkPtr;	/* If this is a global variable being referred
				 * to in a procedure, or a variable created by
				 * "upvar", this field points to the
				 * referenced variable's Var struct. */
    } value;
} Var;

typedef struct VarInHash {
    Var var;			/* "Inherit" from Var. */
    Tcl_Size refCount;		/* Counts number of active uses of this
				 * variable: 1 for the entry in the hash
				 * table, 1 for each additional variable whose
				 * linkPtr points here, 1 for each nested
				 * trace active on variable, and 1 if the
				 * variable is a namespace variable. This
				 * record can't be deleted until refCount
				 * becomes 0. */
    Tcl_HashEntry entry;	/* The hash table entry that refers to this
				 * variable. This is used to find the name of
				 * the variable and to delete it from its
				 * hash table if it is no longer needed. It
				 * also holds the variable's name. */
} VarInHash;

/*
 * Flag bits for variables. The first two (VAR_ARRAY and VAR_LINK) are
 * mutually exclusive and give the "type" of the variable. If none is set,
 * this is a scalar variable.
 *
 * VAR_ARRAY -			1 means this is an array variable rather than
 *				a scalar variable or link. The "tablePtr"
 *				field points to the array's hash table for its
 *				elements.
 * VAR_LINK -			1 means this Var structure contains a pointer
 *				to another Var structure that either has the
 *				real value or is itself another VAR_LINK
 *				pointer. Variables like this come about
 *				through "upvar" and "global" commands, or
 *				through references to variables in enclosing
 *				namespaces.
 * VAR_CONSTANT -		1 means this is a constant "variable", and
 *				cannot be written to by ordinary commands.
 *				Structurally, it's the same as a scalar when
 *				being read, but writes are rejected. Constants
 *				are not supported inside arrays.
 *
 * Flags that indicate the type and status of storage; none is set for
 * compiled local variables (Var structs).
 *
 * VAR_IN_HASHTABLE -		1 means this variable is in a hash table and
 *				the Var structure is malloc'ed. 0 if it is a
 *				local variable that was assigned a slot in a
 *				procedure frame by the compiler so the Var
 *				storage is part of the call frame.
 * VAR_DEAD_HASH		1 means that this var's entry in the hash table
 *				has already been deleted.
 * VAR_ARRAY_ELEMENT -		1 means that this variable is an array
 *				element, so it is not legal for it to be an
 *				array itself (the VAR_ARRAY flag had better
 *				not be set).
 * VAR_NAMESPACE_VAR -		1 means that this variable was declared as a
 *				namespace variable. This flag ensures it
 *				persists until its namespace is destroyed or
 *				until the variable is unset; it will persist
 *				even if it has not been initialized and is
 *				marked undefined. The variable's refCount is
 *				incremented to reflect the "reference" from
 *				its namespace.
 *
 * Flag values relating to the variable's trace and search status.
 *
 * VAR_TRACED_READ
 * VAR_TRACED_WRITE
 * VAR_TRACED_UNSET
 * VAR_TRACED_ARRAY
 * VAR_TRACE_ACTIVE -		1 means that trace processing is currently
 *				underway for a read or write access, so new
 *				read or write accesses should not cause trace
 *				procedures to be called and the variable can't
 *				be deleted.
 * VAR_SEARCH_ACTIVE
 *
 * The following additional flags are used with the CompiledLocal type defined
 * below:
 *
 * VAR_ARGUMENT -		1 means that this variable holds a procedure
 *				argument.
 * VAR_TEMPORARY -		1 if the local variable is an anonymous
 *				temporary variable. Temporaries have a NULL
 *				name.
 * VAR_RESOLVED -		1 if name resolution has been done for this
 *				variable.
 * VAR_IS_ARGS			1 if this variable is the last argument and is
 *				named "args".
 */

/*
 * FLAGS RENUMBERED: everything breaks already, make things simpler.
 *
 * IMPORTANT: skip the values 0x10, 0x20, 0x40, 0x800 corresponding to
 * TCL_TRACE_(READS/WRITES/UNSETS/ARRAY): makes code simpler in tclTrace.c
 *
 * Keep the flag values for VAR_ARGUMENT and VAR_TEMPORARY so that old values
 * in precompiled scripts keep working.
 */

/* Type of value (0 is scalar) */
#define VAR_ARRAY		0x1
#define VAR_LINK		0x2
#define VAR_CONSTANT		0x10000

/* Type of storage (0 is compiled local) */
#define VAR_IN_HASHTABLE	0x4
#define VAR_DEAD_HASH		0x8
#define VAR_ARRAY_ELEMENT	0x1000
#define VAR_NAMESPACE_VAR	0x80	/* KEEP OLD VALUE for Itcl */

#define VAR_ALL_HASH \
	(VAR_IN_HASHTABLE|VAR_DEAD_HASH|VAR_NAMESPACE_VAR|VAR_ARRAY_ELEMENT)

/* Trace and search state. */

#define VAR_TRACED_READ		0x10	/* TCL_TRACE_READS */
#define VAR_TRACED_WRITE	0x20	/* TCL_TRACE_WRITES */
#define VAR_TRACED_UNSET	0x40	/* TCL_TRACE_UNSETS */
#define VAR_TRACED_ARRAY	0x800	/* TCL_TRACE_ARRAY */
#define VAR_TRACE_ACTIVE	0x2000
#define VAR_SEARCH_ACTIVE	0x4000
#define VAR_ALL_TRACES \
	(VAR_TRACED_READ|VAR_TRACED_WRITE|VAR_TRACED_ARRAY|VAR_TRACED_UNSET)

/* Special handling on initialisation (only CompiledLocal). */
#define VAR_ARGUMENT		0x100	/* KEEP OLD VALUE! See tclProc.c */
#define VAR_TEMPORARY		0x200	/* KEEP OLD VALUE! See tclProc.c */
#define VAR_IS_ARGS		0x400
#define VAR_RESOLVED		0x8000

#define TCL_HASH_FIND	((int *)-1)

/*
 * Macros to ensure that various flag bits are set properly for variables.
 * The ANSI C "prototypes" for these macros are:
 *
 * MODULE_SCOPE void	TclSetVarScalar(Var *varPtr);
 * MODULE_SCOPE void	TclSetVarArray(Var *varPtr);
 * MODULE_SCOPE void	TclSetVarLink(Var *varPtr);
 * MODULE_SCOPE void	TclSetVarConstant(Var *varPtr);
 * MODULE_SCOPE void	TclSetVarArrayElement(Var *varPtr);
 * MODULE_SCOPE void	TclSetVarUndefined(Var *varPtr);
 * MODULE_SCOPE void	TclClearVarUndefined(Var *varPtr);
 */

#define TclSetVarScalar(varPtr) \
    (varPtr)->flags &= ~(VAR_ARRAY|VAR_LINK|VAR_CONSTANT)

#define TclSetVarArray(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~VAR_LINK) | VAR_ARRAY

#define TclSetVarLink(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~VAR_ARRAY) | VAR_LINK

#define TclSetVarConstant(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~(VAR_ARRAY|VAR_LINK)) | VAR_CONSTANT

#define TclSetVarArrayElement(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~VAR_ARRAY) | VAR_ARRAY_ELEMENT

#define TclSetVarUndefined(varPtr) \
    (varPtr)->flags &= ~(VAR_ARRAY|VAR_LINK|VAR_CONSTANT);\
    (varPtr)->value.objPtr = NULL

#define TclClearVarUndefined(varPtr)

#define TclSetVarTraceActive(varPtr) \
    (varPtr)->flags |= VAR_TRACE_ACTIVE

#define TclClearVarTraceActive(varPtr) \
    (varPtr)->flags &= ~VAR_TRACE_ACTIVE

#define TclSetVarNamespaceVar(varPtr) \
    if (!TclIsVarNamespaceVar(varPtr)) {\
	(varPtr)->flags |= VAR_NAMESPACE_VAR;\
	if (TclIsVarInHash(varPtr)) {\
	    ((VarInHash *)(varPtr))->refCount++;\
	}\
    }

#define TclClearVarNamespaceVar(varPtr) \
    if (TclIsVarNamespaceVar(varPtr)) {\
	(varPtr)->flags &= ~VAR_NAMESPACE_VAR;\
	if (TclIsVarInHash(varPtr)) {\
	    ((VarInHash *)(varPtr))->refCount--;\
	}\
    }

/*
 * Macros to read various flag bits of variables.
 * The ANSI C "prototypes" for these macros are:
 *
 * MODULE_SCOPE int	TclIsVarScalar(Var *varPtr);
 * MODULE_SCOPE int	TclIsVarConstant(Var *varPtr);
 * MODULE_SCOPE int	TclIsVarLink(Var *varPtr);
 * MODULE_SCOPE int	TclIsVarArray(Var *varPtr);
 * MODULE_SCOPE int	TclIsVarUndefined(Var *varPtr);
 * MODULE_SCOPE int	TclIsVarArrayElement(Var *varPtr);
 * MODULE_SCOPE int	TclIsVarTemporary(Var *varPtr);
 * MODULE_SCOPE int	TclIsVarArgument(Var *varPtr);
 * MODULE_SCOPE int	TclIsVarResolved(Var *varPtr);
 */

#define TclVarFindHiddenArray(varPtr,arrayPtr)				\
    do {								\
	if ((arrayPtr == NULL) && TclIsVarInHash(varPtr) &&		\
		(TclVarParentArray(varPtr) != NULL)) {			\
	    arrayPtr = TclVarParentArray(varPtr);			\
	}								\
    } while(0)

#define TclIsVarScalar(varPtr) \
    !((varPtr)->flags & (VAR_ARRAY|VAR_LINK))

#define TclIsVarLink(varPtr) \
    ((varPtr)->flags & VAR_LINK)

#define TclIsVarArray(varPtr) \
    ((varPtr)->flags & VAR_ARRAY)

/* Implies scalar as well. */
#define TclIsVarConstant(varPtr) \
    ((varPtr)->flags & VAR_CONSTANT)

#define TclIsVarUndefined(varPtr) \
    ((varPtr)->value.objPtr == NULL)

#define TclIsVarArrayElement(varPtr) \
    ((varPtr)->flags & VAR_ARRAY_ELEMENT)

#define TclIsVarNamespaceVar(varPtr) \
    ((varPtr)->flags & VAR_NAMESPACE_VAR)

#define TclIsVarTemporary(varPtr) \
    ((varPtr)->flags & VAR_TEMPORARY)

#define TclIsVarArgument(varPtr) \
    ((varPtr)->flags & VAR_ARGUMENT)

#define TclIsVarResolved(varPtr) \
    ((varPtr)->flags & VAR_RESOLVED)

#define TclIsVarTraceActive(varPtr) \
    ((varPtr)->flags & VAR_TRACE_ACTIVE)

#define TclIsVarTraced(varPtr) \
    ((varPtr)->flags & VAR_ALL_TRACES)

#define TclIsVarInHash(varPtr) \
    ((varPtr)->flags & VAR_IN_HASHTABLE)

#define TclIsVarDeadHash(varPtr) \
    ((varPtr)->flags & VAR_DEAD_HASH)

#define TclGetVarNsPtr(varPtr) \
    (TclIsVarInHash(varPtr) \
	? ((TclVarHashTable *) ((((VarInHash *) (varPtr))->entry.tablePtr)))->nsPtr \
	: NULL)

#define TclVarParentArray(varPtr)					\
    ((TclVarHashTable *) ((VarInHash *) (varPtr))->entry.tablePtr)->arrayPtr

#define VarHashRefCount(varPtr) \
    ((VarInHash *) (varPtr))->refCount

#define VarHashGetKey(varPtr) \
    (((VarInHash *)(varPtr))->entry.key.objPtr)

/*
 * Macros for direct variable access by TEBC.
 */

#define TclIsVarTricky(varPtr,trickyFlags)				\
    (   ((varPtr)->flags & (VAR_ARRAY|VAR_LINK|trickyFlags))		\
	  || (TclIsVarInHash(varPtr)					\
		&& (TclVarParentArray(varPtr) != NULL)			\
		&& (TclVarParentArray(varPtr)->flags & (trickyFlags))))

#define TclIsVarDirectReadable(varPtr)					\
    (   (!TclIsVarTricky(varPtr,VAR_TRACED_READ))			\
	&& (varPtr)->value.objPtr)

#define TclIsVarDirectWritable(varPtr) \
    (!TclIsVarTricky(varPtr,VAR_TRACED_WRITE|VAR_DEAD_HASH|VAR_CONSTANT))

#define TclIsVarDirectUnsettable(varPtr) \
    (!TclIsVarTricky(varPtr,VAR_TRACED_READ|VAR_TRACED_WRITE|VAR_TRACED_UNSET|VAR_DEAD_HASH|VAR_CONSTANT))

#define TclIsVarDirectModifyable(varPtr) \
    (   (!TclIsVarTricky(varPtr,VAR_TRACED_READ|VAR_TRACED_WRITE|VAR_CONSTANT))	\
	&&  (varPtr)->value.objPtr)

#define TclIsVarDirectReadable2(varPtr, arrayPtr) \
    (TclIsVarDirectReadable(varPtr) &&\
	(!(arrayPtr) || !((arrayPtr)->flags & VAR_TRACED_READ)))

#define TclIsVarDirectWritable2(varPtr, arrayPtr) \
    (TclIsVarDirectWritable(varPtr) &&\
	(!(arrayPtr) || !((arrayPtr)->flags & VAR_TRACED_WRITE)))

#define TclIsVarDirectModifyable2(varPtr, arrayPtr) \
    (TclIsVarDirectModifyable(varPtr) &&\
	(!(arrayPtr) || !((arrayPtr)->flags & (VAR_TRACED_READ|VAR_TRACED_WRITE))))

/*
 *----------------------------------------------------------------
 * Data structures related to procedures. These are used primarily in
 * tclProc.c, tclCompile.c, and tclExecute.c.
 *----------------------------------------------------------------
 */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#   define TCLFLEXARRAY
#elif defined(__GNUC__) && (__GNUC__ > 2)
#   define TCLFLEXARRAY 0
#else
#   define TCLFLEXARRAY 1
#endif

/*
 * Forward declaration to prevent an error when the forward reference to
 * Command is encountered in the Proc and ImportRef types declared below.
 */

struct Command;

/*
 * The variable-length structure below describes a local variable of a
 * procedure that was recognized by the compiler. These variables have a name,
 * an element in the array of compiler-assigned local variables in the
 * procedure's call frame, and various other items of information. If the
 * local variable is a formal argument, it may also have a default value. The
 * compiler can't recognize local variables whose names are expressions (these
 * names are only known at runtime when the expressions are evaluated) or
 * local variables that are created as a result of an "upvar" or "uplevel"
 * command. These other local variables are kept separately in a hash table in
 * the call frame.
 */

typedef struct CompiledLocal {
    struct CompiledLocal *nextPtr;
				/* Next compiler-recognized local variable for
				 * this procedure, or NULL if this is the last
				 * local. */
    Tcl_Size nameLength;	/* The number of bytes in local variable's name.
				 * Among others used to speed up var lookups. */
    Tcl_Size frameIndex;	/* Index in the array of compiler-assigned
				 * variables in the procedure call frame. */
    Tcl_Obj *defValuePtr;	/* Pointer to the default value of an
				 * argument, if any. NULL if not an argument
				 * or, if an argument, no default value. */
    Tcl_ResolvedVarInfo *resolveInfo;
				/* Customized variable resolution info
				 * supplied by the Tcl_ResolveCompiledVarProc
				 * associated with a namespace. Each variable
				 * is marked by a unique tag during
				 * compilation, and that same tag is used to
				 * find the variable at runtime. */
    int flags;			/* Flag bits for the local variable. Same as
				 * the flags for the Var structure above,
				 * although only VAR_ARGUMENT, VAR_TEMPORARY,
				 * and VAR_RESOLVED make sense. */
    char name[TCLFLEXARRAY];	/* Name of the local variable starts here. If
				 * the name is NULL, this will just be '\0'.
				 * The actual size of this field will be large
				 * enough to hold the name. MUST BE THE LAST
				 * FIELD IN THE STRUCTURE! */
} CompiledLocal;

/*
 * The structure below defines a command procedure, which consists of a
 * collection of Tcl commands plus information about arguments and other local
 * variables recognized at compile time.
 */

typedef struct Proc {
    struct Interp *iPtr;	/* Interpreter for which this command is
				 * defined. */
    Tcl_Size refCount;		/* Reference count: 1 if still present in
				 * command table plus 1 for each call to the
				 * procedure that is currently active. This
				 * structure can be freed when refCount
				 * becomes zero. */
    struct Command *cmdPtr;	/* Points to the Command structure for this
				 * procedure. This is used to get the
				 * namespace in which to execute the
				 * procedure. */
    Tcl_Obj *bodyPtr;		/* Points to the ByteCode object for
				 * procedure's body command. */
    Tcl_Size numArgs;		/* Number of formal parameters. */
    Tcl_Size numCompiledLocals;	/* Count of local variables recognized by the
				 * compiler including arguments and
				 * temporaries. */
    CompiledLocal *firstLocalPtr;
				/* Pointer to first of the procedure's
				 * compiler-allocated local variables, or NULL
				 * if none. The first numArgs entries in this
				 * list describe the procedure's formal
				 * arguments. */
    CompiledLocal *lastLocalPtr;/* Pointer to the last allocated local
				 * variable or NULL if none. This has frame
				 * index (numCompiledLocals-1). */
} Proc;

/*
 * The type of functions called to process errors found during the execution
 * of a procedure (or lambda term or ...).
 */

typedef void (ProcErrorProc)(Tcl_Interp *interp, Tcl_Obj *procNameObj);

/*
 * The structure below defines a command trace. This is used to allow Tcl
 * clients to find out whenever a command is about to be executed.
 */

typedef struct Trace {
    Tcl_Size level;		/* Only trace commands at nesting level less
				 * than or equal to this. */
    Tcl_CmdObjTraceProc2 *proc;	/* Procedure to call to trace command. */
    void *clientData;		/* Arbitrary value to pass to proc. */
    struct Trace *nextPtr;	/* Next in list of traces for this interp. */
    int flags;			/* Flags governing the trace - see
				 * Tcl_CreateObjTrace for details. */
    Tcl_CmdObjTraceDeleteProc *delProc;
				/* Procedure to call when trace is deleted. */
} Trace;

/*
 * When an interpreter trace is active (i.e. its associated procedure is
 * executing), one of the following structures is linked into a list
 * associated with the interpreter. The information in the structure is needed
 * in order for Tcl to behave reasonably if traces are deleted while traces
 * are active.
 */

typedef struct ActiveInterpTrace {
    struct ActiveInterpTrace *nextPtr;
				/* Next in list of all active command traces
				 * for the interpreter, or NULL if no more. */
    Trace *nextTracePtr;	/* Next trace to check after current trace
				 * procedure returns; if this trace gets
				 * deleted, must update pointer to avoid using
				 * free'd memory. */
    int reverseScan;		/* Boolean set true when traces are scanning
				 * in reverse order. */
} ActiveInterpTrace;

/*
 * Flag values designating types of execution traces. See tclTrace.c for
 * related flag values.
 *
 * TCL_TRACE_ENTER_EXEC		- triggers enter/enterstep traces.
 *				- passed to Tcl_CreateObjTrace to set up
 *				  "enterstep" traces.
 * TCL_TRACE_LEAVE_EXEC		- triggers leave/leavestep traces.
 *				- passed to Tcl_CreateObjTrace to set up
 *				  "leavestep" traces.
 */

#define TCL_TRACE_ENTER_EXEC	1
#define TCL_TRACE_LEAVE_EXEC	2

#define TclObjTypeHasProc(objPtr, proc) (((objPtr)->typePtr \
	&& ((offsetof(Tcl_ObjType, proc) < offsetof(Tcl_ObjType, version)) \
	|| (offsetof(Tcl_ObjType, proc) < (objPtr)->typePtr->version))) ? \
	((objPtr)->typePtr)->proc : NULL)

MODULE_SCOPE Tcl_Size TclLengthOne(Tcl_Obj *);

/*
 * Abstract List
 *
 * This structure provides the functions used in List operations to emulate a
 * List for AbstractList types.
 */

static inline Tcl_Size
TclObjTypeLength(
    Tcl_Obj *objPtr)
{
    Tcl_ObjTypeLengthProc *proc = TclObjTypeHasProc(objPtr, lengthProc);
    return proc(objPtr);
}

static inline int
TclObjTypeIndex(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    Tcl_Size index,
    Tcl_Obj **elemObjPtr)
{
    Tcl_ObjTypeIndexProc *proc = TclObjTypeHasProc(objPtr, indexProc);
    return proc(interp, objPtr, index, elemObjPtr);
}

static inline int
TclObjTypeSlice(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    Tcl_Size fromIdx,
    Tcl_Size toIdx,
    Tcl_Obj **newObjPtr)
{
    Tcl_ObjTypeSliceProc *proc = TclObjTypeHasProc(objPtr, sliceProc);
    return proc(interp, objPtr, fromIdx, toIdx, newObjPtr);
}

static inline int
TclObjTypeReverse(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    Tcl_Obj **newObjPtr)
{
    Tcl_ObjTypeReverseProc *proc = TclObjTypeHasProc(objPtr, reverseProc);
    return proc(interp, objPtr, newObjPtr);
}

static inline int
TclObjTypeGetElements(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    Tcl_Size *objCPtr,
    Tcl_Obj ***objVPtr)
{
    Tcl_ObjTypeGetElements *proc = TclObjTypeHasProc(objPtr, getElementsProc);
    return proc(interp, objPtr, objCPtr, objVPtr);
}

static inline Tcl_Obj*
TclObjTypeSetElement(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    Tcl_Size indexCount,
    Tcl_Obj *const indexArray[],
    Tcl_Obj *valueObj)
{
    Tcl_ObjTypeSetElement *proc = TclObjTypeHasProc(objPtr, setElementProc);
    return proc(interp, objPtr, indexCount, indexArray, valueObj);
}

static inline int
TclObjTypeReplace(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    Tcl_Size first,
    Tcl_Size numToDelete,
    Tcl_Size numToInsert,
    Tcl_Obj *const insertObjs[])
{
    Tcl_ObjTypeReplaceProc *proc = TclObjTypeHasProc(objPtr, replaceProc);
    return proc(interp, objPtr, first, numToDelete, numToInsert, insertObjs);
}

static inline int
TclObjTypeInOperator(
    Tcl_Interp *interp,
    Tcl_Obj *valueObj,
    Tcl_Obj *listObj,
    int *boolResult)
{
    Tcl_ObjTypeInOperatorProc *proc = TclObjTypeHasProc(listObj, inOperProc);
    return proc(interp, valueObj, listObj, boolResult);
}

/*
 * The structure below defines an entry in the assocData hash table which is
 * associated with an interpreter. The entry contains a pointer to a function
 * to call when the interpreter is deleted, and a pointer to a user-defined
 * piece of data.
 */

typedef struct AssocData {
    Tcl_InterpDeleteProc *proc;	/* Proc to call when deleting. */
    void *clientData;		/* Value to pass to proc. */
} AssocData;

/*
 * The structure below defines a call frame. A call frame defines a naming
 * context for a procedure call: its local naming scope (for local variables)
 * and its global naming scope (a namespace, perhaps the global :: namespace).
 * A call frame can also define the naming context for a namespace eval or
 * namespace inscope command: the namespace in which the command's code should
 * execute. The Tcl_CallFrame structures exist only while procedures or
 * namespace eval/inscope's are being executed, and provide a kind of Tcl call
 * stack.
 *
 * WARNING!! The structure definition must be kept consistent with the
 * Tcl_CallFrame structure in tcl.h. If you change one, change the other.
 */

/*
 * Will be grown to contain: pointers to the varnames (allocated at the end),
 * plus the init values for each variable (suitable to be memcopied on init)
 */

typedef struct LocalCache {
    Tcl_Size refCount;		/* Reference count. */
    Tcl_Size numVars;		/* Number of variables. */
    Tcl_Obj *varName0;		/* First variable name. */
} LocalCache;

#define localName(framePtr, i) \
    ((&((framePtr)->localCachePtr->varName0))[(i)])

MODULE_SCOPE void	TclFreeLocalCache(Tcl_Interp *interp,
			    LocalCache *localCachePtr);

typedef struct CallFrame {
    Namespace *nsPtr;		/* Points to the namespace used to resolve
				 * commands and global variables. */
    int isProcCallFrame;	/* If 0, the frame was pushed to execute a
				 * namespace command and var references are
				 * treated as references to namespace vars;
				 * varTablePtr and compiledLocals are ignored.
				 * If FRAME_IS_PROC is set, the frame was
				 * pushed to execute a Tcl procedure and may
				 * have local vars. */
    Tcl_Size objc;		/* This and objv below describe the arguments
				 * for this procedure call. */
    Tcl_Obj *const *objv;	/* Array of argument objects. */
    struct CallFrame *callerPtr;/* Value of interp->framePtr when this
				 * procedure was invoked (i.e. next higher in
				 * stack of all active procedures). */
    struct CallFrame *callerVarPtr;
				/* Value of interp->varFramePtr when this
				 * procedure was invoked (i.e. determines
				 * variable scoping within caller). Same as
				 * callerPtr unless an "uplevel" command or
				 * something equivalent was active in the
				 * caller). */
    Tcl_Size level;		/* Level of this procedure, for "uplevel"
				 * purposes (i.e. corresponds to nesting of
				 * callerVarPtr's, not callerPtr's). 1 for
				 * outermost procedure, 0 for top-level. */
    Proc *procPtr;		/* Points to the structure defining the called
				 * procedure. Used to get information such as
				 * the number of compiled local variables
				 * (local variables assigned entries ["slots"]
				 * in the compiledLocals array below). */
    TclVarHashTable *varTablePtr;
				/* Hash table containing local variables not
				 * recognized by the compiler, or created at
				 * execution time through, e.g., upvar.
				 * Initially NULL and created if needed. */
    Tcl_Size numCompiledLocals;	/* Count of local variables recognized
				 * by the compiler including arguments. */
    Var *compiledLocals;	/* Points to the array of local variables
				 * recognized by the compiler. The compiler
				 * emits code that refers to these variables
				 * using an index into this array. */
    void *clientData;		/* Pointer to some context that is used by
				 * object systems. The meaning of the contents
				 * of this field is defined by the code that
				 * sets it, and it should only ever be set by
				 * the code that is pushing the frame. In that
				 * case, the code that sets it should also
				 * have some means of discovering what the
				 * meaning of the value is, which we do not
				 * specify. */
    LocalCache *localCachePtr;	/* Pointer to the start of the cached variable
				 * names and initialisation data for local
				 * variables. */
    Tcl_Obj *tailcallPtr;	/* NULL if no tailcall is scheduled */
} CallFrame;

#define FRAME_IS_PROC	0x1	/* Frame is a procedure body. */
#define FRAME_IS_LAMBDA 0x2	/* Frame is a lambda term body. */
#define FRAME_IS_METHOD	0x4	/* The frame is a method body, and the frame's
				 * clientData field contains a CallContext
				 * reference. Part of TIP#257. */
#define FRAME_IS_OO_DEFINE 0x8	/* The frame is part of the inside workings of
				 * the [oo::define] command; the clientData
				 * field contains an Object reference that has
				 * been confirmed to refer to a class. Part of
				 * TIP#257. */
#define FRAME_IS_PRIVATE_DEFINE 0x10
				/* Marks this frame as being used for private
				 * declarations with [oo::define]. Usually
				 * OR'd with FRAME_IS_OO_DEFINE. TIP#500. */

/*
 * TIP #280
 * The structure below defines a command frame. A command frame provides
 * location information for all commands executing a tcl script (source, eval,
 * uplevel, procedure bodies, ...). The runtime structure essentially contains
 * the stack trace as it would be if the currently executing command were to
 * throw an error.
 *
 * For commands where it makes sense it refers to the associated CallFrame as
 * well.
 *
 * The structures are chained in a single list, with the top of the stack
 * anchored in the Interp structure.
 *
 * Instances can be allocated on the C stack, or the heap, the former making
 * cleanup a bit simpler.
 */

typedef struct CmdFrame {
    /*
     * General data. Always available.
     */

    int type;			/* Values see below. */
    int level;			/* Number of frames in stack, prevent O(n)
				 * scan of list. */
    int *line;		/* Lines the words of the command start on. */
    Tcl_Size nline;		/* Number of lines in CmdFrame.line. */
    CallFrame *framePtr;	/* Procedure activation record, may be
				 * NULL. */
    struct CmdFrame *nextPtr;	/* Link to calling frame. */
    /*
     * Data needed for Eval vs TEBC
     *
     * EXECUTION CONTEXTS and usage of CmdFrame
     *
     * Field	  TEBC		  EvalEx
     * =======	  ====		  ======
     * level	  yes		  yes
     * type	  BC/PREBC	  SRC/EVAL
     * line0	  yes		  yes
     * framePtr	  yes		  yes
     * =======	  ====		  ======
     *
     * =======	  ====		  ========= union data
     * line1	  -		  yes
     * line3	  -		  yes
     * path	  -		  yes
     * -------	  ----		  ------
     * codePtr	  yes		  -
     * pc	  yes		  -
     * =======	  ====		  ======
     *
     * =======	  ====		  ========= union cmd
     * str.cmd	  yes		  yes
     * str.len	  yes		  yes
     * -------	  ----		  ------
     */

    union {
	struct {
	    Tcl_Obj *path;	/* Path of the sourced file the command is
				 * in. */
	} eval;
	struct {
	    const void *codePtr;/* Byte code currently executed... */
	    const char *pc;	/* ... and instruction pointer. */
	} tebc;
    } data;
    Tcl_Obj *cmdObj;
    const char *cmd;		/* The executed command, if possible... */
    Tcl_Size len;		/* ... and its length. */
    const struct CFWordBC *litarg;
				/* Link to set of literal arguments which have
				 * ben pushed on the lineLABCPtr stack by
				 * TclArgumentBCEnter(). These will be removed
				 * by TclArgumentBCRelease. */
} CmdFrame;

typedef struct CFWord {
    CmdFrame *framePtr;		/* CmdFrame to access. */
    Tcl_Size word;		/* Index of the word in the command. */
    Tcl_Size refCount;		/* Number of times the word is on the
				 * stack. */
} CFWord;

typedef struct CFWordBC {
    CmdFrame *framePtr;		/* CmdFrame to access. */
    Tcl_Size pc;		/* Instruction pointer of a command in
				 * ExtCmdLoc.loc[.] */
    Tcl_Size word;		/* Index of word in
				 * ExtCmdLoc.loc[cmd]->line[.] */
    struct CFWordBC *prevPtr;	/* Previous entry in stack for same Tcl_Obj. */
    struct CFWordBC *nextPtr;	/* Next entry for same command call. See
				 * CmdFrame litarg field for the list start. */
    Tcl_Obj *obj;		/* Back reference to hash table key */
} CFWordBC;

/*
 * Structure to record the locations of invisible continuation lines in
 * literal scripts, as character offset from the beginning of the script. Both
 * compiler and direct evaluator use this information to adjust their line
 * counters when tracking through the script, because when it is invoked the
 * continuation line marker as a whole has been removed already, meaning that
 * the \n which was part of it is gone as well, breaking regular line
 * tracking.
 *
 * These structures are allocated and filled by both the function
 * TclSubstTokens() in the file "tclParse.c" and its caller TclEvalEx() in the
 * file "tclBasic.c", and stored in the thread-global hash table "lineCLPtr" in
 * file "tclObj.c". They are used by the functions TclSetByteCodeFromAny() and
 * TclCompileScript(), both found in the file "tclCompile.c". Their memory is
 * released by the function TclFreeObj(), in the file "tclObj.c", and also by
 * the function TclThreadFinalizeObjects(), in the same file.
 */

#define CLL_END		(-1)

typedef struct ContLineLoc {
    Tcl_Size num;		/* Number of entries in loc, not counting the
				 * final -1 marker entry. */
    Tcl_Size loc[TCLFLEXARRAY];	/* Table of locations, as character offsets.
				 * The table is allocated as part of the
				 * structure, extending behind the nominal end
				 * of the structure. An entry containing the
				 * value -1 is put after the last location, as
				 * end-marker/sentinel. */
} ContLineLoc;

/*
 * The following macros define the allowed values for the type field of the
 * CmdFrame structure above. Some of the values occur only in the extended
 * location data referenced via the 'baseLocPtr'.
 *
 * TCL_LOCATION_EVAL	  : Frame is for a script evaluated by EvalEx.
 * TCL_LOCATION_BC	  : Frame is for bytecode.
 * TCL_LOCATION_PREBC	  : Frame is for precompiled bytecode.
 * TCL_LOCATION_SOURCE	  : Frame is for a script evaluated by EvalEx, from a
 *			    sourced file.
 * TCL_LOCATION_PROC	  : Frame is for bytecode of a procedure.
 *
 * A TCL_LOCATION_BC type in a frame can be overridden by _SOURCE and _PROC
 * types, per the context of the byte code in execution.
 */

#define TCL_LOCATION_EVAL	(0) /* Location in a dynamic eval script. */
#define TCL_LOCATION_BC		(2) /* Location in byte code. */
#define TCL_LOCATION_PREBC	(3) /* Location in precompiled byte code, no
				     * location. */
#define TCL_LOCATION_SOURCE	(4) /* Location in a file. */
#define TCL_LOCATION_PROC	(5) /* Location in a dynamic proc. */
#define TCL_LOCATION_LAST	(6) /* Number of values in the enum. */

/*
 * Structure passed to describe procedure-like "procedures" that are not
 * procedures (e.g. a lambda) so that their details can be reported correctly
 * by [info frame]. Contains a sub-structure for each extra field.
 */

typedef Tcl_Obj * (GetFrameInfoValueProc)(void *clientData);
typedef struct {
    const char *name;		/* Name of this field. */
    GetFrameInfoValueProc *proc;/* Function to generate a Tcl_Obj* from the
				 * clientData, or just use the clientData
				 * directly (after casting) if NULL. */
    void *clientData;		/* Context for above function, or Tcl_Obj* if
				 * proc field is NULL. */
} ExtraFrameInfoField;
typedef struct {
    Tcl_Size length;		/* Length of array. */
    ExtraFrameInfoField fields[2];
				/* Really as long as necessary, but this is
				 * long enough for nearly anything. */
} ExtraFrameInfo;

/*
 *----------------------------------------------------------------
 * Data structures and procedures related to TclHandles, which are a very
 * lightweight method of preserving enough information to determine if an
 * arbitrary malloc'd block has been deleted.
 *----------------------------------------------------------------
 */

typedef void **TclHandle;

/*
 *----------------------------------------------------------------
 * Experimental flag value passed to Tcl_GetRegExpFromObj. Intended for use
 * only by Expect. It will probably go away in a later release.
 *----------------------------------------------------------------
 */

#define TCL_REG_BOSONLY 002000	/* Prepend \A to pattern so it only matches at
				 * the beginning of the string. */

/*
 * These are a thin layer over TclpThreadKeyDataGet and TclpThreadKeyDataSet
 * when threads are used, or an emulation if there are no threads. These are
 * really internal and Tcl clients should use Tcl_GetThreadData.
 */

MODULE_SCOPE void *	TclThreadDataKeyGet(Tcl_ThreadDataKey *keyPtr);
MODULE_SCOPE void	TclThreadDataKeySet(Tcl_ThreadDataKey *keyPtr,
			    void *data);

/*
 * This is a convenience macro used to initialize a thread local storage ptr.
 */

#define TCL_TSD_INIT(keyPtr) \
	(ThreadSpecificData *)Tcl_GetThreadData((keyPtr), sizeof(ThreadSpecificData))

/*
 *----------------------------------------------------------------
 * Data structures related to bytecode compilation and execution. These are
 * used primarily in tclCompile.c, tclExecute.c, and tclBasic.c.
 *----------------------------------------------------------------
 */

/*
 * Forward declaration to prevent errors when the forward references to
 * Tcl_Parse and CompileEnv are encountered in the procedure type CompileProc
 * declared below.
 */

struct CompileEnv;

/*
 * The type of procedures called by the Tcl bytecode compiler to compile
 * commands. Pointers to these procedures are kept in the Command structure
 * describing each command. The integer value returned by a CompileProc must
 * be one of the following:
 *
 * TCL_OK		Compilation completed normally.
 * TCL_ERROR		Compilation could not be completed. This can be just a
 *			judgment by the CompileProc that the command is too
 *			complex to compile effectively, or it can indicate
 *			that in the current state of the interp, the command
 *			would raise an error. The bytecode compiler will not
 *			do any error reporting at compiler time. Error
 *			reporting is deferred until the actual runtime,
 *			because by then changes in the interp state may allow
 *			the command to be successfully evaluated.
 */

typedef int (CompileProc)(Tcl_Interp *interp, Tcl_Parse *parsePtr,
	struct Command *cmdPtr, struct CompileEnv *compEnvPtr);

/*
 * The type of procedure called from the compilation hook point in
 * SetByteCodeFromAny.
 */

typedef int (CompileHookProc)(Tcl_Interp *interp,
	struct CompileEnv *compEnvPtr, void *clientData);

/*
 * The data structure for a (linked list of) execution stacks. Note that the
 * first word on a particular execution stack is NULL, which is used as a
 * marker to say "go to the previous stack in the list" when unwinding the
 * stack.
 */

typedef struct ExecStack {
    struct ExecStack *prevPtr;	/* Previous stack in list. */
    struct ExecStack *nextPtr;	/* Next stack in list. */
    Tcl_Obj **markerPtr;	/* The location of the NULL marker. */
    Tcl_Obj **endPtr;		/* Where the stack end is. */
    Tcl_Obj **tosPtr;		/* Where the stack top is. */
    Tcl_Obj *stackWords[TCLFLEXARRAY];
				/* The actual stack space, following this
				 * structure in memory. */
} ExecStack;

/*
 * Saved copies of the stack frame references from the interpreter. Have to be
 * restored into the interpreter to be used.
 */
typedef struct CorContext {
    CallFrame *framePtr;	/* See Interp.framePtr */
    CallFrame *varFramePtr;	/* See Interp.varFramePtr */
    CmdFrame *cmdFramePtr;	/* See Interp.cmdFramePtr */
    Tcl_HashTable *lineLABCPtr;	/* See Interp.lineLABCPtr */
} CorContext;

/*
 * The data structure defining the execution environment for ByteCode's.
 * There is one ExecEnv structure per Tcl interpreter. It holds the evaluation
 * stack that holds command operands and results. The stack grows towards
 * increasing addresses. The member stackPtr points to the stackItems of the
 * currently active execution stack.
 */

typedef struct CoroutineData {
    struct Command *cmdPtr;	/* The command handle for the coroutine. */
    struct ExecEnv *eePtr;	/* The special execution environment (stacks,
				 * etc.) for the coroutine. */
    struct ExecEnv *callerEEPtr;/* The execution environment for the caller of
				 * the coroutine, which might be the
				 * interpreter global environment or another
				 * coroutine. */
    CorContext caller;		/* Caller's saved execution context. */
    CorContext running;		/* This coroutine's saved execution context. */
    Tcl_HashTable *lineLABCPtr;	/* See Interp.lineLABCPtr */
    void *stackLevel;		/* C stack frame reference. Used to try to
				 * ensure we don't overflow that stack. */
    Tcl_Size auxNumLevels;	/* While the coroutine is running the
				 * numLevels of the create/resume command is
				 * stored here; for suspended coroutines it
				 * holds the nesting numLevels at yield. */
    Tcl_Size nargs;		/* Number of args required for resuming this
				 * coroutine; COROUTINE_ARGUMENTS_SINGLE_OPTIONAL
				 * means "0 or 1" (default),
				 * COROUTINE_ARGUMENTS_ARBITRARY means "any" */
    Tcl_Obj *yieldPtr;		/* The command to yield to.  Stored here in
				 * order to reset splice point in
				 * TclNRCoroutineActivateCallback if the
				 * coroutine is busy. */
} CoroutineData;

typedef struct ExecEnv {
    ExecStack *execStackPtr;	/* Points to the first item in the evaluation
				 * stack on the heap. */
    Tcl_Obj *constants[2];	/* Pointers to constant "0" and "1" objs. */
    struct Tcl_Interp *interp;	/* Owning interpreter. */
    struct NRE_callback *callbackPtr;
				/* Top callback in NRE's stack. */
    struct CoroutineData *corPtr;
				/* Current coroutine. */
    int rewind;			/* Set when exception trapping is disabled
				 * because a context is being deleted (e.g.,
				 * the current coroutine has been deleted). */
} ExecEnv;

#define COR_IS_SUSPENDED(corPtr) \
    ((corPtr)->stackLevel == NULL)

// The different types of yielded coroutine we have.
#define CORO_ACTIVATE_YIELD	NULL		// 0 or 1 argument expected
#define CORO_ACTIVATE_YIELDM	INT2PTR(1)	// Arbitrary arguments expected

/*
 * The definitions for the LiteralTable and LiteralEntry structures. Each
 * interpreter contains a LiteralTable. It is used to reduce the storage
 * needed for all the Tcl objects that hold the literals of scripts compiled
 * by the interpreter. A literal's object is shared by all the ByteCodes that
 * refer to the literal. Each distinct literal has one LiteralEntry entry in
 * the LiteralTable. A literal table is a specialized hash table that is
 * indexed by the literal's string representation, which may contain null
 * characters.
 *
 * Note that we reduce the space needed for literals by sharing literal
 * objects both within a ByteCode (each ByteCode contains a local
 * LiteralTable) and across all an interpreter's ByteCodes (with the
 * interpreter's global LiteralTable).
 */

typedef struct LiteralEntry {
    struct LiteralEntry *nextPtr;
				/* Points to next entry in this hash bucket or
				 * NULL if end of chain. */
    Tcl_Obj *objPtr;		/* Points to Tcl object that holds the
				 * literal's bytes and length. */
    Tcl_Size refCount;		/* If in an interpreter's global literal
				 * table, the number of ByteCode structures
				 * that share the literal object; the literal
				 * entry can be freed when refCount drops to
				 * 0. If in a local literal table, TCL_INDEX_NONE. */
    Namespace *nsPtr;		/* Namespace in which this literal is used. We
				 * try to avoid sharing literal non-FQ command
				 * names among different namespaces to reduce
				 * shimmering. */
} LiteralEntry;

typedef struct LiteralTable {
    LiteralEntry **buckets;	/* Pointer to bucket array. Each element
				 * points to first entry in bucket's hash
				 * chain, or NULL. */
    LiteralEntry *staticBuckets[TCL_SMALL_HASH_TABLE];
				/* Bucket array used for small tables to avoid
				 * mallocs and frees. */
    size_t numBuckets;	/* Total number of buckets allocated at
				 * **buckets. */
    size_t numEntries;	/* Total number of entries present in
				 * table. */
    size_t rebuildSize;	/* Enlarge table when numEntries gets to be
				 * this large. */
    size_t mask;		/* Mask value used in hashing function. */
} LiteralTable;

/*
 * The following structure defines for each Tcl interpreter various
 * statistics-related information about the bytecode compiler and
 * interpreter's operation in that interpreter.
 */

#ifdef TCL_COMPILE_STATS
typedef struct ByteCodeStats {
    size_t numExecutions;	/* Number of ByteCodes executed. */
    size_t numCompilations;	/* Number of ByteCodes created. */
    size_t numByteCodesFreed;	/* Number of ByteCodes destroyed. */
    size_t instructionCount[256];
				/* Number of times each instruction was
				 * executed. */

    double totalSrcBytes;	/* Total source bytes ever compiled. */
    double totalByteCodeBytes;	/* Total bytes for all ByteCodes. */
    double currentSrcBytes;	/* Src bytes for all current ByteCodes. */
    double currentByteCodeBytes;/* Code bytes in all current ByteCodes. */

    size_t srcCount[32];	/* Source size distribution: # of srcs of
				 * size [2**(n-1)..2**n), n in [0..32). */
    size_t byteCodeCount[32];	/* ByteCode size distribution. */
    size_t lifetimeCount[32];	/* ByteCode lifetime distribution (ms). */

    double currentInstBytes;	/* Instruction bytes-current ByteCodes. */
    double currentLitBytes;	/* Current literal bytes. */
    double currentExceptBytes;	/* Current exception table bytes. */
    double currentAuxBytes;	/* Current auxiliary information bytes. */
    double currentCmdMapBytes;	/* Current src<->code map bytes. */

    size_t numLiteralsCreated;	/* Total literal objects ever compiled. */
    double totalLitStringBytes;	/* Total string bytes in all literals. */
    double currentLitStringBytes;
				/* String bytes in current literals. */
    size_t literalCount[32];	/* Distribution of literal string sizes. */
} ByteCodeStats;
#endif /* TCL_COMPILE_STATS */

/*
 * Structure used in implementation of those core ensembles which are
 * partially compiled. Used as an array of these, with a terminating field
 * whose 'name' is NULL.
 */

typedef struct {
    const char *name;		/* The name of the subcommand. */
    Tcl_ObjCmdProc *proc;	/* The implementation of the subcommand. */
    CompileProc *compileProc;	/* The compiler for the subcommand. */
    Tcl_ObjCmdProc *nreProc;	/* NRE implementation of this command. */
    void *clientData;		/* Any clientData to give the command. */
    int unsafe;			/* Whether this command is to be hidden by
				 * default in a safe interpreter. */
} EnsembleImplMap;

/*
 *----------------------------------------------------------------
 * Data structures related to commands.
 *----------------------------------------------------------------
 */

/*
 * An imported command is created in an namespace when it imports a "real"
 * command from another namespace. An imported command has a Command structure
 * that points (via its ClientData value) to the "real" Command structure in
 * the source namespace's command table. The real command records all the
 * imported commands that refer to it in a list of ImportRef structures so
 * that they can be deleted when the real command is deleted.
 */

typedef struct ImportRef {
    struct Command *importedCmdPtr;
				/* Points to the imported command created in
				 * an importing namespace; this command
				 * redirects its invocations to the "real"
				 * command. */
    struct ImportRef *nextPtr;	/* Next element on the linked list of imported
				 * commands that refer to the "real" command.
				 * The real command deletes these imported
				 * commands on this list when it is
				 * deleted. */
} ImportRef;

/*
 * Data structure used as the ClientData of imported commands: commands
 * created in an namespace when it imports a "real" command from another
 * namespace.
 */

typedef struct ImportedCmdData {
    struct Command *realCmdPtr;	/* "Real" command that this imported command
				 * refers to. */
    struct Command *selfPtr;	/* Pointer to this imported command. Needed
				 * only when deleting it in order to remove it
				 * from the real command's linked list of
				 * imported commands that refer to it. */
} ImportedCmdData;

/*
 * A Command structure exists for each command in a namespace. The Tcl_Command
 * opaque type actually refers to these structures.
 */

typedef struct Command {
    Tcl_HashEntry *hPtr;	/* Pointer to the hash table entry that refers
				 * to this command. The hash table is either a
				 * namespace's command table or an
				 * interpreter's hidden command table. This
				 * pointer is used to get a command's name
				 * from its Tcl_Command handle. NULL means
				 * that the hash table entry has been removed
				 * already (this can happen if deleteProc
				 * causes the command to be deleted or
				 * recreated). */
    Namespace *nsPtr;		/* Points to the namespace containing this
				 * command. */
    Tcl_Size refCount;		/* 1 if in command hashtable plus 1 for each
				 * reference from a CmdName Tcl object
				 * representing a command's name in a ByteCode
				 * instruction sequence. This structure can be
				 * freed when refCount becomes zero. */
    Tcl_Size cmdEpoch;		/* Incremented to invalidate any references
				 * that point to this command when it is
				 * renamed, deleted, hidden, or exposed. */
    CompileProc *compileProc;	/* Procedure called to compile command. NULL
				 * if no compile proc exists for command. */
    Tcl_ObjCmdProc *objProc;	/* Object-based command procedure. */
    void *objClientData;	/* Arbitrary value passed to object proc. */
    Tcl_CmdProc *proc;		/* String-based command procedure. */
    void *clientData;		/* Arbitrary value passed to string proc. */
    Tcl_CmdDeleteProc *deleteProc;
				/* Procedure invoked when deleting command to,
				 * e.g., free all client data. */
    void *deleteData;		/* Arbitrary value passed to deleteProc. */
    int flags;			/* Miscellaneous bits of information about
				 * command. See below for definitions. */
    ImportRef *importRefPtr;	/* List of each imported Command created in
				 * another namespace when this command is
				 * imported. These imported commands redirect
				 * invocations back to this command. The list
				 * is used to remove all those imported
				 * commands when deleting this "real"
				 * command. */
    CommandTrace *tracePtr;	/* First in list of all traces set for this
				 * command. */
    Tcl_ObjCmdProc *nreProc;	/* NRE implementation of this command. */
} Command;

/*
 * Flag bits for commands.
 */
enum CommandFlags {
    CMD_DYING = 0x01,		/* The command is in the process of being
				 * deleted (its deleteProc is currently
				 * executing). Other attempts to delete the
				 * command should be ignored.*/
    CMD_TRACE_ACTIVE = 0x02,	/* The trace processing is currently underway
				 * for a rename/delete change. See the flags
				 * CMD_TRACE_RENAMING, CMD_TRACE_DELETING for
				 * which is currently being processed. */
    CMD_HAS_EXEC_TRACES = 0x04,	/* This command has at least one execution
				 * trace (as opposed to simple delete/rename
				 * traces) in its tracePtr list. */
    CMD_COMPILES_EXPANDED = 0x08,
				/* This command has a compiler that can handle
				 * expansion (provided it is not the first
				 * word). */
    CMD_REDEF_IN_PROGRESS = 0x10,
				/* Command is currently being redefined. It
				 * should not be deleted, though its ClientData
				 * may be purged. */
    CMD_VIA_RESOLVER = 0x20,	/* Command was located by resolver. Its literal
				 * should be marked unshared by the compiler. */
    CMD_DEAD = 0x40,		/* Command is at an advanced stage of being
				 * deleted, and is no longer in any hash tables
				 * but stale references may exist elsewhere. */
    CMD_TRACE_RENAMING = TCL_TRACE_RENAME,
				/* A rename trace is in progress. Further
				 * recursive renames will not be traced. */
    CMD_TRACE_DELETING = TCL_TRACE_DELETE
				/* A delete trace is in progress. Further
				 * recursive deletes will not be traced. */
};

/*
 *----------------------------------------------------------------
 * Data structures related to name resolution procedures.
 *----------------------------------------------------------------
 */

/*
 * The interpreter keeps a linked list of name resolution schemes. The scheme
 * for a namespace is consulted first, followed by the list of schemes in an
 * interpreter, followed by the default name resolution in Tcl. Schemes are
 * added/removed from the interpreter's list by calling Tcl_AddInterpResolver
 * and Tcl_RemoveInterpResolver.
 */

typedef struct ResolverScheme {
    char *name;			/* Name identifying this scheme. */
    Tcl_ResolveCmdProc *cmdResProc;
				/* Procedure handling command name
				 * resolution. */
    Tcl_ResolveVarProc *varResProc;
				/* Procedure handling variable name resolution
				 * for variables that can only be handled at
				 * runtime. */
    Tcl_ResolveCompiledVarProc *compiledVarResProc;
				/* Procedure handling variable name resolution
				 * at compile time. */

    struct ResolverScheme *nextPtr;
				/* Pointer to next record in linked list. */
} ResolverScheme;

/*
 * Forward declaration of the TIP#143 limit handler structure.
 */

typedef struct LimitHandler LimitHandler;

/*
 * TIP #268.
 * Values for the selection mode, i.e the package require preferences.
 */

enum PkgPreferOptions {
    PKG_PREFER_LATEST, PKG_PREFER_STABLE
};

/*
 *----------------------------------------------------------------
 * This structure shadows the first few fields of the memory cache for the
 * allocator defined in tclThreadAlloc.c; it has to be kept in sync with the
 * definition there.
 * Some macros require knowledge of some fields in the struct in order to
 * avoid hitting the TSD unnecessarily. In order to facilitate this, a pointer
 * to the relevant fields is kept in the allocCache field in struct Interp.
 *----------------------------------------------------------------
 */

typedef struct AllocCache {
    struct Cache *nextPtr;	/* Linked list of cache entries. */
    Tcl_ThreadId owner;		/* Which thread's cache is this? */
    Tcl_Obj *firstObjPtr;	/* List of free objects for thread. */
    size_t numObjects;		/* Number of objects for thread. */
} AllocCache;

/*
 *----------------------------------------------------------------
 * This structure defines an interpreter, which is a collection of commands
 * plus other state information related to interpreting commands, such as
 * variable storage. Primary responsibility for this data structure is in
 * tclBasic.c, but almost every Tcl source file uses something in here.
 *----------------------------------------------------------------
 */

typedef struct Interp {
    /*
     * The first two fields were named "result" and "freeProc" in earlier
     * versions of Tcl.  They are no longer used within Tcl, and are no
     * longer available to be accessed by extensions.  However, they cannot
     * be removed.  Why?  There is a deployed base of stub-enabled extensions
     * that query the value of iPtr->stubTable.  For them to continue to work,
     * the location of the field "stubTable" within the Interp struct cannot
     * change.  The most robust way to assure that is to leave all fields up to
     * that one undisturbed.
     */

    const char *legacyResult;
    void (*legacyFreeProc) (void);
    int errorLine;		/* When TCL_ERROR is returned, this gives the
				 * line number in the command where the error
				 * occurred (1 means first line). */
    const struct TclStubs *stubTable;
				/* Pointer to the exported Tcl stub table.  In
				 * ancient pre-8.1 versions of Tcl this was a
				 * pointer to the objResultPtr or a pointer to a
				 * buckets array in a hash table. Deployed stubs
				 * enabled extensions check for a NULL pointer value
				 * and for a TCL_STUBS_MAGIC value to verify they
				 * are not [load]ing into one of those pre-stubs
				 * interps. */

    TclHandle handle;		/* Handle used to keep track of when this
				 * interp is deleted. */

    Namespace *globalNsPtr;	/* The interpreter's global namespace. */
    Tcl_HashTable *hiddenCmdTablePtr;
				/* Hash table used by tclBasic.c to keep track
				 * of hidden commands on a per-interp
				 * basis. */
    void *interpInfo;		/* Information used by tclInterp.c to keep
				 * track of parent/child interps on a
				 * per-interp basis. */
    void (*optimizer)(void *envPtr);
				/* Reference to the bytecode optimizer, if one
				 * is set. */
    /*
     * Information related to procedures and variables. See tclProc.c and
     * tclVar.c for usage.
     */

    Tcl_Size numLevels;		/* Keeps track of how many nested calls to
				 * Tcl_Eval are in progress for this
				 * interpreter. It's used to delay deletion of
				 * the table until all Tcl_Eval invocations
				 * are completed. */
    Tcl_Size maxNestingDepth;	/* If numLevels exceeds this value then Tcl
				 * assumes that infinite recursion has
				 * occurred and it generates an error. */
    CallFrame *framePtr;	/* Points to top-most in stack of all nested
				 * procedure invocations. */
    CallFrame *varFramePtr;	/* Points to the call frame whose variables
				 * are currently in use (same as framePtr
				 * unless an "uplevel" command is
				 * executing). */
    ActiveVarTrace *activeVarTracePtr;
				/* First in list of active traces for interp,
				 * or NULL if no active traces. */
    int returnCode;		/* [return -code] parameter. */
    CallFrame *rootFramePtr;	/* Global frame pointer for this
				 * interpreter. */
    Namespace *lookupNsPtr;	/* Namespace to use ONLY on the next
				 * TCL_EVAL_INVOKE call to Tcl_EvalObjv. */


    /*
     * Information about packages. Used only in tclPkg.c.
     */

    Tcl_HashTable packageTable;	/* Describes all of the packages loaded in or
				 * available to this interpreter. Keys are
				 * package names, values are (Package *)
				 * pointers. */
    char *packageUnknown;	/* Command to invoke during "package require"
				 * commands for packages that aren't described
				 * in packageTable. Ckalloc'ed, may be
				 * NULL. */
    /*
     * Miscellaneous information:
     */

    Tcl_Size cmdCount;		/* Total number of times a command procedure
				 * has been called for this interpreter. */
    int evalFlags;		/* Flags to control next call to Tcl_Eval.
				 * Normally zero, but may be set before
				 * calling Tcl_Eval. See below for valid
				 * values. */
    LiteralTable literalTable;	/* Contains LiteralEntry's describing all Tcl
				 * objects holding literals of scripts
				 * compiled by the interpreter. Indexed by the
				 * string representations of literals. Used to
				 * avoid creating duplicate objects. */
    Tcl_Size compileEpoch;	/* Holds the current "compilation epoch" for
				 * this interpreter. This is incremented to
				 * invalidate existing ByteCodes when, e.g., a
				 * command with a compile procedure is
				 * redefined. */
    Proc *compiledProcPtr;	/* If a procedure is being compiled, a pointer
				 * to its Proc structure; otherwise, this is
				 * NULL. Set by ObjInterpProc in tclProc.c and
				 * used by tclCompile.c to process local
				 * variables appropriately. */
    ResolverScheme *resolverPtr;/* Linked list of name resolution schemes
				 * added to this interpreter. Schemes are
				 * added and removed by calling
				 * Tcl_AddInterpResolvers and
				 * Tcl_RemoveInterpResolver respectively. */
    Tcl_Obj *scriptFile;	/* NULL means there is no nested source
				 * command active; otherwise this points to
				 * pathPtr of the file being sourced. */
    int flags;			/* Various flag bits. See below. */
    long randSeed;		/* Seed used for rand() function. */
    Trace *tracePtr;		/* List of traces for this interpreter. */
    Tcl_HashTable *assocData;	/* Hash table for associating data with this
				 * interpreter. Cleaned up when this
				 * interpreter is deleted. */
    struct ExecEnv *execEnvPtr;	/* Execution environment for Tcl bytecode
				 * execution. Contains a pointer to the Tcl
				 * evaluation stack. */
    Tcl_Obj *emptyObjPtr;	/* Points to an object holding an empty
				 * string. Returned by Tcl_ObjSetVar2 when
				 * variable traces change a variable in a
				 * gross way. */
    Tcl_Obj *objResultPtr;	/* If the last command returned an object
				 * result, this points to it. Should not be
				 * accessed directly; see comment above. */
    Tcl_ThreadId threadId;	/* ID of thread that owns the interpreter. */

    ActiveCommandTrace *activeCmdTracePtr;
				/* First in list of active command traces for
				 * interp, or NULL if no active traces. */
    ActiveInterpTrace *activeInterpTracePtr;
				/* First in list of active traces for interp,
				 * or NULL if no active traces. */
    Tcl_Size tracesForbiddingInline;
				/* Count of traces (in the list headed by
				 * tracePtr) that forbid inline bytecode
				 * compilation. */

    /*
     * Fields used to manage extensible return options (TIP 90).
     */

    Tcl_Obj *returnOpts;	/* A dictionary holding the options to the
				 * last [return] command. */

    Tcl_Obj *errorInfo;		/* errorInfo value (now as a Tcl_Obj). */
    Tcl_Obj *eiVar;		/* cached ref to ::errorInfo variable. */
    Tcl_Obj *errorCode;		/* errorCode value (now as a Tcl_Obj). */
    Tcl_Obj *ecVar;		/* cached ref to ::errorInfo variable. */
    int returnLevel;		/* [return -level] parameter. */

    /*
     * Resource limiting framework support (TIP#143).
     */

    struct {
	int active;		/* Flag values defining which limits have been
				 * set. */
	int granularityTicker;	/* Counter used to determine how often to
				 * check the limits. */
	int exceeded;		/* Which limits have been exceeded, described
				 * as flag values the same as the 'active'
				 * field. */

	Tcl_Size cmdCount;	/* Limit for how many commands to execute in
				 * the interpreter. */
	LimitHandler *cmdHandlers;
				/* Handlers to execute when the limit is
				 * reached. */
	int cmdGranularity;	/* Mod factor used to determine how often to
				 * evaluate the limit check. */

	Tcl_Time time;		/* Time limit for execution within the
				 * interpreter. */
	LimitHandler *timeHandlers;
				/* Handlers to execute when the limit is
				 * reached. */
	int timeGranularity;	/* Mod factor used to determine how often to
				 * evaluate the limit check. */
	Tcl_TimerToken timeEvent;
				/* Handle for a timer callback that will occur
				 * when the time-limit is exceeded. */

	Tcl_HashTable callbacks;/* Mapping from (interp,type) pair to data
				 * used to install a limit handler callback to
				 * run in _this_ interp when the limit is
				 * exceeded. */
    } limit;

    /*
     * Information for improved default error generation from ensembles
     * (TIP#112).
     */

    struct {
	Tcl_Obj *const *sourceObjs;
				/* What arguments were actually input into the
				 * *root* ensemble command? (Nested ensembles
				 * don't rewrite this.) NULL if we're not
				 * processing an ensemble. */
	Tcl_Size numRemovedObjs;/* How many arguments have been stripped off
				 * because of ensemble processing. */
	Tcl_Size numInsertedObjs;
				/* How many of the current arguments were
				 * inserted by an ensemble. */
    } ensembleRewrite;

    /*
     * TIP #219: Global info for the I/O system.
     */

    Tcl_Obj *chanMsg;		/* Error message set by channel drivers, for
				 * the propagation of arbitrary Tcl errors.
				 * This information, if present (chanMsg not
				 * NULL), takes precedence over a POSIX error
				 * code returned by a channel operation. */

    /*
     * Source code origin information (TIP #280).
     */

    CmdFrame *cmdFramePtr;	/* Points to the command frame containing the
				 * location information for the current
				 * command. */
    const CmdFrame *invokeCmdFramePtr;
				/* Points to the command frame which is the
				 * invoking context of the bytecode compiler.
				 * NULL when the byte code compiler is not
				 * active. */
    Tcl_Size invokeWord;	/* Index of the word in the command which
				 * is getting compiled. */
    Tcl_HashTable *linePBodyPtr;/* This table remembers for each statically
				 * defined procedure the location information
				 * for its body. It is keyed by the address of
				 * the Proc structure for a procedure. The
				 * values are "struct CmdFrame*". */
    Tcl_HashTable *lineBCPtr;	/* This table remembers for each ByteCode
				 * object the location information for its
				 * body. It is keyed by the address of the
				 * Proc structure for a procedure. The values
				 * are "struct ExtCmdLoc*". (See
				 * tclCompile.h) */
    Tcl_HashTable *lineLABCPtr;	/* Tcl_Obj* (by exact pointer) -> CFWordBC* */
    Tcl_HashTable *lineLAPtr;	/* This table remembers for each argument of a
				 * command on the execution stack the index of
				 * the argument in the command, and the
				 * location data of the command. It is keyed
				 * by the address of the Tcl_Obj containing
				 * the argument. The values are "struct
				 * CFWord*" (See tclBasic.c). This allows
				 * commands like uplevel, eval, etc. to find
				 * location information for their arguments,
				 * if they are a proper literal argument to an
				 * invoking command. Alt view: An index to the
				 * CmdFrame stack keyed by command argument
				 * holders. */
    ContLineLoc *scriptCLLocPtr;/* This table points to the location data for
				 * invisible continuation lines in the script,
				 * if any. This pointer is set by the function
				 * TclEvalObjEx() in file "tclBasic.c", and
				 * used by function ...() in the same file.
				 * It does for the eval/direct path of script
				 * execution what CompileEnv.clLoc does for
				 * the bytecode compiler. */
    /*
     * TIP #268. The currently active selection mode, i.e. the package require
     * preferences.
     */

    int packagePrefer;		/* Current package selection mode. */

    /*
     * Hashtables for variable traces and searches.
     */

    Tcl_HashTable varTraces;	/* Hashtable holding the start of a variable's
				 * active trace list; varPtr is the key. */
    Tcl_HashTable varSearches;	/* Hashtable holding the start of a variable's
				 * active searches list; varPtr is the key. */
    /*
     * The thread-specific data ekeko: cache pointers or values that
     *  (a) do not change during the thread's lifetime
     *  (b) require access to TSD to determine at runtime
     *  (c) are accessed very often (e.g., at each command call)
     *
     * Note that these are the same for all interps in the same thread. They
     * just have to be initialised for the thread's parent interp, children
     * inherit the value.
     *
     * They are used by the macros defined below.
     */

    AllocCache *allocCache;	/* Allocator cache for stack frames. */
    void *pendingObjDataPtr;	/* Pointer to the Cache and PendingObjData
				 * structs for this interp's thread; see
				 * tclObj.c and tclThreadAlloc.c */
    int *asyncReadyPtr;		/* Pointer to the asyncReady indicator for
				 * this interp's thread; see tclAsync.c */
    /*
     * The pointer to the object system root ekeko. c.f. TIP #257.
     */
    void *objectFoundation;	/* Pointer to the Foundation structure of the
				 * object system, which contains things like
				 * references to key namespaces. See
				 * tclOOInt.h and tclOO.c for real definition
				 * and setup. */

    struct NRE_callback *deferredCallbacks;
				/* Callbacks that are set previous to a call
				 * to some Eval function but that actually
				 * belong to the command that is about to be
				 * called - i.e., they should be run *before*
				 * any tailcall is invoked. */

    /*
     * TIP #285, Script cancellation support.
     */

    Tcl_AsyncHandler asyncCancel;
				/* Async handler token for Tcl_CancelEval. */
    Tcl_Obj *asyncCancelMsg;	/* Error message set by async cancel handler
				 * for the propagation of arbitrary Tcl
				 * errors. This information, if present
				 * (asyncCancelMsg not NULL), takes precedence
				 * over the default error messages returned by
				 * a script cancellation operation. */

    /*
     * TIP #348 IMPLEMENTATION  -  Substituted error stack
     */
    Tcl_Obj *errorStack;	/* [info errorstack] value (as a Tcl_Obj). */
    Tcl_Obj *upLiteral;		/* "UP" literal for [info errorstack] */
    Tcl_Obj *callLiteral;	/* "CALL" literal for [info errorstack] */
    Tcl_Obj *innerLiteral;	/* "INNER" literal for [info errorstack] */
    Tcl_Obj *innerContext;	/* cached list for fast reallocation */
    int resetErrorStack;	/* controls cleaning up of ::errorStack */

#ifdef TCL_COMPILE_STATS
    /*
     * Statistical information about the bytecode compiler and interpreter's
     * operation. This should be the last field of Interp.
     */

    ByteCodeStats stats;	/* Holds compilation and execution statistics
				 * for this interpreter. */
#endif /* TCL_COMPILE_STATS */
} Interp;

/*
 * Macros that use the TSD-ekeko.
 */

#define TclAsyncReady(iPtr) \
    *((iPtr)->asyncReadyPtr)

/*
 * Macros for script cancellation support (TIP #285).
 */

#define TclCanceled(iPtr) \
    (((iPtr)->flags & CANCELED) || ((iPtr)->flags & TCL_CANCEL_UNWIND))

#define TclSetCancelFlags(iPtr, cancelFlags) \
    (iPtr)->flags |= CANCELED;			\
    if ((cancelFlags) & TCL_CANCEL_UNWIND) {	\
	(iPtr)->flags |= TCL_CANCEL_UNWIND;	\
    }

#define TclUnsetCancelFlags(iPtr) \
    (iPtr)->flags &= (~(CANCELED | TCL_CANCEL_UNWIND))

/*
 * Macros for splicing into and out of doubly linked lists. They assume
 * existence of struct items 'prevPtr' and 'nextPtr'.
 *
 * a = element to add or remove.
 * b = list head.
 *
 * TclSpliceIn adds to the head of the list.
 */

#define TclSpliceIn(a,b)			\
    (a)->nextPtr = (b);				\
    if ((b) != NULL) {				\
	(b)->prevPtr = (a);			\
    }						\
    (a)->prevPtr = NULL, (b) = (a);

#define TclSpliceOut(a,b)			\
    if ((a)->prevPtr != NULL) {			\
	(a)->prevPtr->nextPtr = (a)->nextPtr;	\
    } else {					\
	(b) = (a)->nextPtr;			\
    }						\
    if ((a)->nextPtr != NULL) {			\
	(a)->nextPtr->prevPtr = (a)->prevPtr;	\
    }

/*
 * EvalFlag bits for Interp structures:
 *
 * TCL_ALLOW_EXCEPTIONS	1 means it's OK for the script to terminate with a
 *			code other than TCL_OK or TCL_ERROR; 0 means codes
 *			other than these should be turned into errors.
 */

#define TCL_ALLOW_EXCEPTIONS		0x04
#define TCL_EVAL_FILE			0x02
#define TCL_EVAL_SOURCE_IN_FRAME	0x10
#define TCL_EVAL_NORESOLVE		0x20
#define TCL_EVAL_DISCARD_RESULT		0x40

/*
 * Flag bits for Interp structures:
 *
 * DELETED:		Non-zero means the interpreter has been deleted:
 *			don't process any more commands for it, and destroy
 *			the structure as soon as all nested invocations of
 *			Tcl_Eval are done.
 * ERR_ALREADY_LOGGED:	Non-zero means information has already been logged in
 *			iPtr->errorInfo for the current Tcl_Eval instance, so
 *			Tcl_Eval needn't log it (used to implement the "error
 *			message log" command).
 * DONT_COMPILE_CMDS_INLINE: Non-zero means that the bytecode compiler should
 *			not compile any commands into an inline sequence of
 *			instructions. This is set 1, for example, when command
 *			traces are requested.
 * RAND_SEED_INITIALIZED: Non-zero means that the randSeed value of the interp
 *			has not be initialized. This is set 1 when we first
 *			use the rand() or srand() functions.
 * SAFE_INTERP:		Non zero means that the current interp is a safe
 *			interp (i.e. it has only the safe commands installed,
 *			less privilege than a regular interp).
 * INTERP_DEBUG_FRAME:	Used for switching on various extra interpreter
 *			debug/info mechanisms (e.g. info frame eval/uplevel
 *			tracing) which are performance intensive.
 * INTERP_TRACE_IN_PROGRESS: Non-zero means that an interp trace is currently
 *			active; so no further trace callbacks should be
 *			invoked.
 * INTERP_ALTERNATE_WRONG_ARGS: Used for listing second and subsequent forms
 *			of the wrong-num-args string in Tcl_WrongNumArgs.
 *			Makes it append instead of replacing and uses
 *			different intermediate text.
 * CANCELED:		Non-zero means that the script in progress should be
 *			canceled as soon as possible. This can be checked by
 *			extensions (and the core itself) by calling
 *			Tcl_Canceled and checking if TCL_ERROR is returned.
 *			This is a one-shot flag that is reset immediately upon
 *			being detected; however, if the TCL_CANCEL_UNWIND flag
 *			is set Tcl_Canceled will continue to report that the
 *			script in progress has been canceled thereby allowing
 *			the evaluation stack for the interp to be fully
 *			unwound.
 */

#define DELETED				     1
#define ERR_ALREADY_LOGGED		     4
#define INTERP_DEBUG_FRAME		  0x10
#define DONT_COMPILE_CMDS_INLINE	  0x20
#define RAND_SEED_INITIALIZED		  0x40
#define SAFE_INTERP			  0x80
#define INTERP_TRACE_IN_PROGRESS	 0x200
#define INTERP_ALTERNATE_WRONG_ARGS	 0x400
#define ERR_LEGACY_COPY			 0x800
#define CANCELED			0x1000

/*
 * Maximum number of levels of nesting permitted in Tcl commands (used to
 * catch infinite recursion).
 */

#define MAX_NESTING_DEPTH	1000

/*
 * The macro below is used to modify a "char" value (e.g. by casting it to an
 * unsigned character) so that it can be used safely with macros such as
 * isspace.
 */

#define UCHAR(c) ((unsigned char) (c))

/*
 * This macro is used to properly align the memory allocated by Tcl, giving
 * the same alignment as the native malloc.
 */

#if defined(__APPLE__)
#define TCL_ALLOCALIGN	16
#else
#define TCL_ALLOCALIGN	(2*(int)sizeof(void *))
#endif

/*
 * TCL_ALIGN is used to determine the offset needed to safely allocate any
 * data structure in memory. Given a starting offset or size, it "rounds up"
 * or "aligns" the offset to the next aligned (typically 8-byte) boundary so
 * that any data structure can be placed at the resulting offset without fear
 * of an alignment error. Note this is clamped to a minimum of 8 for API
 * compatibility.
 *
 * WARNING!! DO NOT USE THIS MACRO TO ALIGN POINTERS: it will produce the
 * wrong result on platforms that allocate addresses that are divisible by a
 * non-trivial factor of this alignment. Only use it for offsets or sizes.
 *
 * This macro is only used by tclCompile.c in the core (Bug 926445). It
 * however not be made file static, as extensions that touch bytecodes
 * (notably tbcload) require it.
 */

struct TclMaxAlignment {
    char unalign[8];
    union {
	long long maxAlignLongLong;
	double maxAlignDouble;
	void *maxAlignPointer;
    } aligned;
};
#define TCL_ALIGN_BYTES \
	offsetof(struct TclMaxAlignment, aligned)
#define TCL_ALIGN(x) \
	(((x) + (TCL_ALIGN_BYTES - 1)) & ~(TCL_ALIGN_BYTES - 1))

/*
 * A common panic alert when memory allocation fails.
 */

#define TclOOM(ptr, size) \
    ((size) && ((ptr) || (Tcl_Panic(					\
	"unable to alloc %" TCL_Z_MODIFIER "u bytes", (size_t)(size)), 1)))

/*
 * The following enum values are used to specify the runtime platform setting
 * of the tclPlatform variable.
 */

typedef enum {
    TCL_PLATFORM_UNIX = 0,	/* Any Unix-like OS. */
    TCL_PLATFORM_WINDOWS = 2	/* Any Microsoft Windows OS. */
} TclPlatformType;

/*
 * The following enum values are used to indicate the translation of a Tcl
 * channel. Declared here so that each platform can define
 * TCL_PLATFORM_TRANSLATION to the native translation on that platform.
 */

typedef enum TclEolTranslation {
    TCL_TRANSLATE_AUTO,		/* Eol == \r, \n and \r\n. */
    TCL_TRANSLATE_CR,		/* Eol == \r. */
    TCL_TRANSLATE_LF,		/* Eol == \n. */
    TCL_TRANSLATE_CRLF		/* Eol == \r\n. */
} TclEolTranslation;

/*
 * Flags for TclInvoke:
 *
 * TCL_INVOKE_HIDDEN		Invoke a hidden command; if not set, invokes
 *				an exposed command.
 * TCL_INVOKE_NO_UNKNOWN	If set, "unknown" is not invoked if the
 *				command to be invoked is not found. Only has
 *				an effect if invoking an exposed command,
 *				i.e. if TCL_INVOKE_HIDDEN is not also set.
 * TCL_INVOKE_NO_TRACEBACK	Does not record traceback information if the
 *				invoked command returns an error. Used if the
 *				caller plans on recording its own traceback
 *				information.
 */

#define	TCL_INVOKE_HIDDEN	(1<<0)
#define TCL_INVOKE_NO_UNKNOWN	(1<<1)
#define TCL_INVOKE_NO_TRACEBACK	(1<<2)

/*
 * ListStore --
 *
 * A Tcl list's internal representation is defined through three structures.
 *
 * A ListStore struct is a structure that includes a variable size array that
 * serves as storage for a Tcl list. A contiguous sequence of slots in the
 * array, the "in-use" area, holds valid pointers to Tcl_Obj values that
 * belong to one or more Tcl lists. The unused slots before and after these
 * are free slots that may be used to prepend and append without having to
 * reallocate the struct. The ListStore may be shared amongst multiple lists
 * and reference counted.
 *
 * A ListSpan struct defines a sequence of slots within a ListStore. This sequence
 * always lies within the "in-use" area of the ListStore. Like ListStore, the
 * structure may be shared among multiple lists and is reference counted.
 *
 * A ListRep struct holds the internal representation of a Tcl list as stored
 * in a Tcl_Obj. It is composed of a ListStore and a ListSpan that together
 * define the content of the list. The ListSpan specifies the range of slots
 * within the ListStore that hold elements for this list. The ListSpan is
 * optional in which case the list includes all the "in-use" slots of the
 * ListStore.
 *
 */
typedef struct ListStore {
    Tcl_Size firstUsed;		/* Index of first slot in use within slots[] */
    Tcl_Size numUsed;		/* Number of slots in use (starting firstUsed) */
    Tcl_Size numAllocated;	/* Total number of slots[] array slots. */
    size_t refCount;		/* Number of references to this instance. */
    int flags;			/* LISTSTORE_* flags */
    Tcl_Obj *slots[TCLFLEXARRAY];
				/* Variable size array. Grown as needed */
} ListStore;
enum ListStoreFlags {
    LISTSTORE_CANONICAL = 1	/* All Tcl_Obj's referencing this
				 * store have their string representation
				 * derived from the list representation */
};

/* Max number of elements that can be contained in a list */
#define LIST_MAX \
    ((Tcl_Size)(((size_t)TCL_SIZE_MAX - offsetof(ListStore, slots))	\
	    / sizeof(Tcl_Obj *)))
/* Memory size needed for a ListStore to hold numSlots_ elements */
#define LIST_SIZE(numSlots_) \
    ((Tcl_Size)(offsetof(ListStore, slots)				\
	    + ((numSlots_) * sizeof(Tcl_Obj *))))

/*
 * ListSpan --
 * See comments above for ListStore
 */
typedef struct ListSpan {
    Tcl_Size spanStart;		/* Starting index of the span. */
    Tcl_Size spanLength;	/* Number of elements in the span. */
    size_t refCount;		/* Count of references to this span record. */
} ListSpan;
#ifndef LIST_SPAN_THRESHOLD	/* May be set on build line */
#define LIST_SPAN_THRESHOLD 101
#endif

/*
 * ListRep --
 * See comments above for ListStore
 */
typedef struct ListRep {
    ListStore *storePtr;	/* element array shared amongst different
				 * lists */
    ListSpan *spanPtr;		/* If not NULL, the span holds the range of
				 * slots within *storePtr that contain this
				 * list elements. */
} ListRep;

/*
 * Macros used to get access list internal representations.
 *
 * Naming conventions:
 * ListRep* - expect a pointer to a valid ListRep
 * ListObj* - expect a pointer to a Tcl_Obj whose internal type is known to
 *            be a list (tclListType). Will crash otherwise.
 * TclListObj* - expect a pointer to a Tcl_Obj whose internal type may or may not
 *            be tclListType. These will convert as needed and return error if
 *            conversion not possible.
 */

/* Returns the starting slot for this listRep in the contained ListStore */
#define ListRepStart(listRepPtr_) \
    ((listRepPtr_)->spanPtr						\
	? (listRepPtr_)->spanPtr->spanStart				\
	: (listRepPtr_)->storePtr->firstUsed)

/* Returns the number of elements in this listRep */
#define ListRepLength(listRepPtr_) \
    ((listRepPtr_)->spanPtr						\
	? (listRepPtr_)->spanPtr->spanLength				\
	: (listRepPtr_)->storePtr->numUsed)

/* Returns a pointer to the first slot containing this ListRep elements */
#define ListRepElementsBase(listRepPtr_) \
    (&(listRepPtr_)->storePtr->slots[ListRepStart(listRepPtr_)])

/* Stores the number of elements and base address of the element array */
#define ListRepElements(listRepPtr_, objc_, objv_) \
    (((objv_) = ListRepElementsBase(listRepPtr_)),			\
     ((objc_) = ListRepLength(listRepPtr_)))

/* Returns 1/0 whether the ListRep's ListStore is shared. */
#define ListRepIsShared(listRepPtr_) ((listRepPtr_)->storePtr->refCount > 1)

/* Returns a pointer to the ListStore component */
#define ListObjStorePtr(listObj_) \
    ((ListStore *)((listObj_)->internalRep.twoPtrValue.ptr1))

/* Returns a pointer to the ListSpan component */
#define ListObjSpanPtr(listObj_) \
    ((ListSpan *)((listObj_)->internalRep.twoPtrValue.ptr2))

/* Returns the ListRep internal representaton in a Tcl_Obj */
#define ListObjGetRep(listObj_, listRepPtr_) \
    do {								\
	(listRepPtr_)->storePtr = ListObjStorePtr(listObj_);		\
	(listRepPtr_)->spanPtr = ListObjSpanPtr(listObj_);		\
    } while (0)

/* Returns the length of the list */
#define ListObjLength(listObj_, len_) \
    ((len_) = ListObjSpanPtr(listObj_)					\
	? ListObjSpanPtr(listObj_)->spanLength				\
	: ListObjStorePtr(listObj_)->numUsed)

/* Returns the starting slot index of this list's elements in the ListStore */
#define ListObjStart(listObj_) \
    (ListObjSpanPtr(listObj_)						\
	? ListObjSpanPtr(listObj_)->spanStart				\
	: ListObjStorePtr(listObj_)->firstUsed)

/* Stores the element count and base address of this list's elements */
#define ListObjGetElements(listObj_, objc_, objv_) \
    (((objv_) = &ListObjStorePtr(listObj_)->slots[ListObjStart(listObj_)]), \
     (ListObjLength(listObj_, (objc_))))

/*
 * Returns 1/0 whether the internal representation (not the Tcl_Obj itself)
 * is shared.  Note by intent this only checks for sharing of ListStore,
 * not spans.
 */
#define ListObjRepIsShared(listObj_) \
    (ListObjStorePtr(listObj_)->refCount > 1)

/*
 * Certain commands like concat are optimized if an existing string
 * representation of a list object is known to be in canonical format (i.e.
 * generated from the list representation). There are three conditions when
 * this will be the case:
 * (1) No string representation exists which means it will obviously have
 * to be generated from the list representation when needed
 * (2) The ListStore flags is marked canonical. This is done at the time
 * the string representation is generated from the list under certain
 * conditions (see comments in UpdateStringOfList).
 * (3) The list representation does not have a span component. This is
 * because list Tcl_Obj's with spans are always created from existing lists
 * and never from strings (see SetListFromAny) and thus their string
 * representation will always be canonical.
 */
#define ListObjIsCanonical(listObj_) \
    (((listObj_)->bytes == NULL)					\
	|| (ListObjStorePtr(listObj_)->flags & LISTSTORE_CANONICAL)	\
	|| ListObjSpanPtr(listObj_) != NULL)

/*
 * Converts the Tcl_Obj to a list if it isn't one and stores the element
 * count and base address of this list's elements in objcPtr_ and objvPtr_.
 * Return TCL_OK on success or TCL_ERROR if the Tcl_Obj cannot be
 * converted to a list.
 */
#define TclListObjGetElements(interp_, listObj_, objcPtr_, objvPtr_) \
    ((TclHasInternalRep((listObj_), &tclListType))			\
	? ((ListObjGetElements((listObj_), *(objcPtr_), *(objvPtr_))),	\
	    TCL_OK)							\
	: Tcl_ListObjGetElements(					\
	    (interp_), (listObj_), (objcPtr_), (objvPtr_)))

/*
 * Converts the Tcl_Obj to a list if it isn't one and stores the element
 * count in lenPtr_.  Returns TCL_OK on success or TCL_ERROR if the
 * Tcl_Obj cannot be converted to a list.
 */
#define TclListObjLength(interp_, listObj_, lenPtr_) \
    ((TclHasInternalRep((listObj_), &tclListType))			\
	? ((ListObjLength((listObj_), *(lenPtr_))), TCL_OK)		\
	: Tcl_ListObjLength((interp_), (listObj_), (lenPtr_)))

#define TclListObjIsCanonical(listObj_) \
    ((TclHasInternalRep((listObj_), &tclListType))			\
	? ListObjIsCanonical((listObj_))				\
	: 0)

/*
 * Modes for collecting (or not) in the implementations of TclNRForeachCmd,
 * TclNRLmapCmd and their compilations.
 */

#define TCL_EACH_KEEP_NONE  0	/* Discard iteration result like [foreach] */
#define TCL_EACH_COLLECT    1	/* Collect iteration result like [lmap] */

/*
 * Macros providing a faster path to booleans and integers:
 * Tcl_GetBooleanFromObj, Tcl_GetLongFromObj, Tcl_GetIntFromObj
 * and Tcl_GetIntForIndex.
 *
 * WARNING: these macros eval their args more than once.
 */

#define TclGetBooleanFromObj(interp, objPtr, intPtr) \
    ((TclHasInternalRep((objPtr), &tclIntType)				\
	    || TclHasInternalRep((objPtr), &tclBooleanType))		\
	? (*(intPtr) = ((objPtr)->internalRep.wideValue!=0), TCL_OK)	\
	: Tcl_GetBooleanFromObj((interp), (objPtr), (intPtr)))

#ifdef TCL_WIDE_INT_IS_LONG
#define TclGetLongFromObj(interp, objPtr, longPtr) \
    ((TclHasInternalRep((objPtr), &tclIntType))				\
	? ((*(longPtr) = (objPtr)->internalRep.wideValue), TCL_OK)	\
	: Tcl_GetLongFromObj((interp), (objPtr), (longPtr)))
#else
#define TclGetLongFromObj(interp, objPtr, longPtr) \
    ((TclHasInternalRep((objPtr), &tclIntType)				\
	    && (objPtr)->internalRep.wideValue >= (Tcl_WideInt)(LONG_MIN) \
	    && (objPtr)->internalRep.wideValue <= (Tcl_WideInt)(LONG_MAX)) \
	? ((*(longPtr) = (long)(objPtr)->internalRep.wideValue), TCL_OK) \
	: Tcl_GetLongFromObj((interp), (objPtr), (longPtr)))
#endif

#define TclGetIntFromObj(interp, objPtr, intPtr) \
    ((TclHasInternalRep((objPtr), &tclIntType)				\
	    && (objPtr)->internalRep.wideValue >= (Tcl_WideInt)(INT_MIN) \
	    && (objPtr)->internalRep.wideValue <= (Tcl_WideInt)(INT_MAX)) \
	? ((*(intPtr) = (int)(objPtr)->internalRep.wideValue), TCL_OK)	\
	: Tcl_GetIntFromObj((interp), (objPtr), (intPtr)))
#define TclGetIntForIndexM(interp, objPtr, endValue, idxPtr) \
    (((TclHasInternalRep((objPtr), &tclIntType))			\
	    && ((objPtr)->internalRep.wideValue >= 0)			\
	    && ((objPtr)->internalRep.wideValue <= endValue))		\
	? ((*(idxPtr) = (objPtr)->internalRep.wideValue), TCL_OK)	\
	: Tcl_GetIntForIndex((interp), (objPtr), (endValue), (idxPtr)))

/*
 * Macro used to save a function call for common uses of
 * Tcl_GetWideIntFromObj(). The ANSI C "prototype" is:
 *
 * MODULE_SCOPE int TclGetWideIntFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
 *			Tcl_WideInt *wideIntPtr);
 */

#define TclGetWideIntFromObj(interp, objPtr, wideIntPtr) \
    ((TclHasInternalRep((objPtr), &tclIntType))				\
	? (*(wideIntPtr) = ((objPtr)->internalRep.wideValue), TCL_OK)	\
	: Tcl_GetWideIntFromObj((interp), (objPtr), (wideIntPtr)))

/*
 * Flag values for TclTraceDictPath().
 *
 * DICT_PATH_READ indicates that all entries on the path must exist but no
 * updates will be needed.
 *
 * DICT_PATH_UPDATE indicates that we are going to be doing an update at the
 * tip of the path, so duplication of shared objects should be done along the
 * way.
 *
 * DICT_PATH_EXISTS indicates that we are performing an existence test and a
 * lookup failure should therefore not be an error. If (and only if) this flag
 * is set, TclTraceDictPath() will return the special value
 * DICT_PATH_NON_EXISTENT if the path is not traceable.
 *
 * DICT_PATH_CREATE (which also requires the DICT_PATH_UPDATE bit to be set)
 * indicates that we are to create non-existent dictionaries on the path.
 */

#define DICT_PATH_READ		0
#define DICT_PATH_UPDATE	1
#define DICT_PATH_EXISTS	2
#define DICT_PATH_CREATE	5

#define DICT_PATH_NON_EXISTENT	((Tcl_Obj *) (void *) 1)

/*
 *----------------------------------------------------------------
 * Data structures related to the filesystem internals
 *----------------------------------------------------------------
 */

/*
 * The version_2 filesystem is private to Tcl. As and when these changes have
 * been thoroughly tested and investigated a new public filesystem interface
 * will be released. The aim is more versatile virtual filesystem interfaces,
 * more efficiency in 'path' manipulation and usage, and cleaner filesystem
 * code internally.
 */

#define TCL_FILESYSTEM_VERSION_2	((Tcl_FSVersion) 0x2)
typedef void *(TclFSGetCwdProc2)(void *clientData);
typedef int (Tcl_FSLoadFileProc2) (Tcl_Interp *interp, Tcl_Obj *pathPtr,
	Tcl_LoadHandle *handlePtr, Tcl_FSUnloadFileProc **unloadProcPtr,
	int flags);

/*
 * The following types are used for getting and storing platform-specific file
 * attributes in tclFCmd.c and the various platform-versions of that file.
 * This is done to have as much common code as possible in the file attributes
 * code. For more information about the callbacks, see TclFileAttrsCmd in
 * tclFCmd.c.
 */

typedef int (TclGetFileAttrProc)(Tcl_Interp *interp, int objIndex,
	Tcl_Obj *fileName, Tcl_Obj **attrObjPtrPtr);
typedef int (TclSetFileAttrProc)(Tcl_Interp *interp, int objIndex,
	Tcl_Obj *fileName, Tcl_Obj *attrObjPtr);

typedef struct TclFileAttrProcs {
    TclGetFileAttrProc *getProc;/* The procedure for getting attrs. */
    TclSetFileAttrProc *setProc;/* The procedure for setting attrs. */
} TclFileAttrProcs;

/*
 * Opaque handle used in pipeline routines to encapsulate platform-dependent
 * state.
 */

typedef struct TclFile_ *TclFile;

typedef enum Tcl_PathPart {
    TCL_PATH_DIRNAME,
    TCL_PATH_TAIL,
    TCL_PATH_EXTENSION,
    TCL_PATH_ROOT
} Tcl_PathPart;

/*
 *----------------------------------------------------------------
 * Data structures related to obsolete filesystem hooks
 *----------------------------------------------------------------
 */

typedef int (TclStatProc_)(const char *path, struct stat *buf);
typedef int (TclAccessProc_)(const char *path, int mode);
typedef Tcl_Channel (TclOpenFileChannelProc_)(Tcl_Interp *interp,
	const char *fileName, const char *modeString, int permissions);

/*
 *----------------------------------------------------------------
 * Data structures for process-global values.
 *----------------------------------------------------------------
 */

typedef void (TclInitProcessGlobalValueProc)(char **valuePtr,
	size_t *lengthPtr,
	Tcl_Encoding *encodingPtr);

#ifdef _WIN32
/* On Windows, all Unicode (except surrogates) are valid. */
#   define TCLFSENCODING tclUtf8Encoding
#else
/* On Non-Windows, use the system encoding for validation checks. */
#   define TCLFSENCODING NULL
#endif

/*
 * A ProcessGlobalValue struct exists for each internal value in Tcl that is
 * to be shared among several threads. Each thread sees a (Tcl_Obj) copy of
 * the value, and the gobal value is kept as a counted string, with epoch and
 * mutex control. Each ProcessGlobalValue struct should be a static variable in
 * some file.
 */

typedef struct ProcessGlobalValue {
    Tcl_Size epoch;		/* Epoch counter to detect changes in the
				 * global value. */
    size_t numBytes;		/* Length of the global string. */
    char *value;		/* The global string value. */
    Tcl_Encoding encoding;	/* system encoding when global string was
				 * initialized. */
    TclInitProcessGlobalValueProc *proc;
				/* A procedure to initialize the global string
				 * copy when a "get" request comes in before
				 * any "set" request has been received. */
    Tcl_Mutex mutex;		/* Enforce orderly access from multiple
				 * threads. */
    Tcl_ThreadDataKey key;	/* Key for per-thread data holding the
				 * (Tcl_Obj) copy for each thread. */
} ProcessGlobalValue;

/*
 *----------------------------------------------------------------------
 * Flags for TclParseNumber
 *----------------------------------------------------------------------
 */

#define TCL_PARSE_DECIMAL_ONLY	1
				/* Leading zero doesn't denote octal or
				 * hex. */
#define TCL_PARSE_OCTAL_ONLY	2
				/* Parse octal even without prefix. */
#define TCL_PARSE_HEXADECIMAL_ONLY	4
				/* Parse hexadecimal even without prefix. */
#define TCL_PARSE_INTEGER_ONLY	8
				/* Disable floating point parsing. */
#define TCL_PARSE_SCAN_PREFIXES	16
				/* Use [scan] rules dealing with 0?
				 * prefixes. */
#define TCL_PARSE_NO_WHITESPACE	32
				/* Reject leading/trailing whitespace. */
#define TCL_PARSE_BINARY_ONLY	64
				/* Parse binary even without prefix. */
#define TCL_PARSE_NO_UNDERSCORE	128
				/* Reject underscore digit separator */

/*
 *----------------------------------------------------------------------
 * Internal convenience macros for manipulating encoding flags. See
 * TCL_ENCODING_PROFILE_* in tcl.h
 *----------------------------------------------------------------------
 */

#define ENCODING_PROFILE_MASK     0xFF000000
#define ENCODING_PROFILE_GET(flags_) \
    ((flags_) & ENCODING_PROFILE_MASK)
#define ENCODING_PROFILE_SET(flags_, profile_) \
    do {								\
	(flags_) &= ~ENCODING_PROFILE_MASK;				\
	(flags_) |= ((profile_) & ENCODING_PROFILE_MASK);		\
    } while (0)

/*
 *----------------------------------------------------------------------
 * Common functions for calculating overallocation. Trivial but allows for
 * experimenting with growth factors without having to change code in
 * multiple places. See TclAttemptAllocElemsEx and similar for usage
 * examples. Best to use those functions. Direct use of TclUpsizeAlloc /
 * TclResizeAlloc is needed in special cases such as when total size of
 * memory block is limited to less than TCL_SIZE_MAX.
 *----------------------------------------------------------------------
 */

static inline Tcl_Size
TclUpsizeAlloc(
    TCL_UNUSED(Tcl_Size),	/* oldSize. For future experiments with
				 * some growth algorithms that use this
				 * information. */
    Tcl_Size needed,
    Tcl_Size limit)
{
    /* assert (oldCapacity < needed <= limit) */
    if (needed < (limit - needed/2)) {
	return needed + needed / 2;
    } else {
	return limit;
    }
}

static inline Tcl_Size
TclUpsizeRetry(
    Tcl_Size needed,
    Tcl_Size lastAttempt)
{
    /* assert(needed < lastAttempt); */
    if (needed < lastAttempt - 1) {
	/* (needed+lastAttempt)/2 but that formula may overflow Tcl_Size */
	return needed + (lastAttempt - needed) / 2;
    } else {
	return needed;
    }
}

MODULE_SCOPE void *	TclAllocElemsEx(Tcl_Size elemCount, Tcl_Size elemSize,
			    Tcl_Size leadSize, Tcl_Size *capacityPtr);
MODULE_SCOPE void *	TclReallocElemsEx(void *oldPtr, Tcl_Size elemCount,
			    Tcl_Size elemSize, Tcl_Size leadSize,
			    Tcl_Size *capacityPtr);
MODULE_SCOPE void *	TclAttemptReallocElemsEx(void *oldPtr,
			    Tcl_Size elemCount, Tcl_Size elemSize,
			    Tcl_Size leadSize, Tcl_Size *capacityPtr);

/* Alloc elemCount elements of size elemSize with leadSize header
 * returning actual capacity (in elements) in *capacityPtr. */
static inline void *
TclAttemptAllocElemsEx(
    Tcl_Size elemCount,
    Tcl_Size elemSize,
    Tcl_Size leadSize,
    Tcl_Size *capacityPtr)
{
    return TclAttemptReallocElemsEx(
	    NULL, elemCount, elemSize, leadSize, capacityPtr);
}

/* Alloc numByte bytes, returning actual capacity in *capacityPtr. */
static inline void *
TclAllocEx(
    Tcl_Size numBytes,
    Tcl_Size *capacityPtr)
{
    return TclAllocElemsEx(numBytes, 1, 0, capacityPtr);
}

/* Alloc numByte bytes, returning actual capacity in *capacityPtr. */
static inline void *
TclAttemptAllocEx(
    Tcl_Size numBytes,
    Tcl_Size *capacityPtr)
{
    return TclAttemptAllocElemsEx(numBytes, 1, 0, capacityPtr);
}

/* Realloc numByte bytes, returning actual capacity in *capacityPtr. */
static inline void *
TclReallocEx(
    void *oldPtr,
    Tcl_Size numBytes,
    Tcl_Size *capacityPtr)
{
    return TclReallocElemsEx(oldPtr, numBytes, 1, 0, capacityPtr);
}

/* Realloc numByte bytes, returning actual capacity in *capacityPtr. */
static inline void *
TclAttemptReallocEx(
    void *oldPtr,
    Tcl_Size numBytes,
    Tcl_Size *capacityPtr)
{
    return TclAttemptReallocElemsEx(oldPtr, numBytes, 1, 0, capacityPtr);
}

/*
 *----------------------------------------------------------------
 * Variables shared among Tcl modules but not used by the outside world.
 *----------------------------------------------------------------
 */

MODULE_SCOPE char *tclNativeExecutableName;
MODULE_SCOPE int tclFindExecutableSearchDone;
MODULE_SCOPE char *tclMemDumpFileName;
MODULE_SCOPE TclPlatformType tclPlatform;

/*
 * Declarations related to internal encoding functions.
 */

MODULE_SCOPE Tcl_Encoding tclIdentityEncoding;
MODULE_SCOPE Tcl_Encoding tclUtf8Encoding;
MODULE_SCOPE int	TclEncodingProfileNameToId(Tcl_Interp *interp,
			    const char *profileName,
			    int *profilePtr);
MODULE_SCOPE const char *TclEncodingProfileIdToName(Tcl_Interp *interp,
			    int profileId);
MODULE_SCOPE void	TclGetEncodingProfiles(Tcl_Interp *interp);

/*
 * TIP #233 (Virtualized Time)
 * Data for the time hooks, if any.
 */

MODULE_SCOPE Tcl_GetTimeProc *tclGetTimeProcPtr;
MODULE_SCOPE Tcl_ScaleTimeProc *tclScaleTimeProcPtr;
MODULE_SCOPE void *tclTimeClientData;

/*
 * Variables denoting the Tcl object types defined in the core.
 */

MODULE_SCOPE const Tcl_ObjType tclBignumType;
MODULE_SCOPE const Tcl_ObjType tclBooleanType;
MODULE_SCOPE const Tcl_ObjType tclByteCodeType;
MODULE_SCOPE const Tcl_ObjType tclDoubleType;
MODULE_SCOPE const Tcl_ObjType tclExprCodeType;
MODULE_SCOPE const Tcl_ObjType tclIntType;
MODULE_SCOPE const Tcl_ObjType tclIndexType;
MODULE_SCOPE const Tcl_ObjType tclListType;
MODULE_SCOPE const Tcl_ObjType tclDictType;
MODULE_SCOPE const Tcl_ObjType tclProcBodyType;
MODULE_SCOPE const Tcl_ObjType tclStringType;
MODULE_SCOPE const Tcl_ObjType tclEnsembleCmdType;
MODULE_SCOPE const Tcl_ObjType tclRegexpType;
MODULE_SCOPE Tcl_ObjType tclCmdNameType;

/*
 * Variables denoting the hash key types defined in the core.
 */

MODULE_SCOPE const Tcl_HashKeyType tclArrayHashKeyType;
MODULE_SCOPE const Tcl_HashKeyType tclOneWordHashKeyType;
MODULE_SCOPE const Tcl_HashKeyType tclStringHashKeyType;
MODULE_SCOPE const Tcl_HashKeyType tclObjHashKeyType;

/*
 * The head of the list of free Tcl objects, and the total number of Tcl
 * objects ever allocated and freed.
 */

MODULE_SCOPE Tcl_Obj *	tclFreeObjList;

#ifdef TCL_COMPILE_STATS
MODULE_SCOPE size_t	tclObjsAlloced;
MODULE_SCOPE size_t	tclObjsFreed;
#define TCL_MAX_SHARED_OBJ_STATS 5
MODULE_SCOPE size_t	tclObjsShared[TCL_MAX_SHARED_OBJ_STATS];
#endif /* TCL_COMPILE_STATS */

/*
 * Pointer to a heap-allocated string of length zero that the Tcl core uses as
 * the value of an empty string representation for an object. This value is
 * shared by all new objects allocated by Tcl_NewObj.
 */

MODULE_SCOPE char	tclEmptyString;

enum CheckEmptyStringResult {
	TCL_EMPTYSTRING_UNKNOWN = -1, TCL_EMPTYSTRING_NO, TCL_EMPTYSTRING_YES
};

/*
 *----------------------------------------------------------------
 * Procedures shared among Tcl modules but not used by the outside world,
 * introduced by/for NRE.
 *----------------------------------------------------------------
 */

MODULE_SCOPE Tcl_ObjCmdProc TclNRApplyObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNREvalObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRCatchObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRExprObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRForObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRForeachCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRIfObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRLmapCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRPackageObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRSourceObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRSubstObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRSwitchObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRTryObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRUplevelObjCmd;
MODULE_SCOPE Tcl_NRPostProc TclUplevelCallback;
MODULE_SCOPE Tcl_ObjCmdProc TclNRWhileObjCmd;

MODULE_SCOPE Tcl_NRPostProc TclNRForIterCallback;
MODULE_SCOPE Tcl_NRPostProc TclNRCoroutineActivateCallback;
MODULE_SCOPE Tcl_ObjCmdProc TclNRTailcallObjCmd;
MODULE_SCOPE Tcl_NRPostProc TclNRTailcallEval;
MODULE_SCOPE Tcl_ObjCmdProc TclNRCoroutineObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRYieldObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRYieldmObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRYieldToObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRInvoke;
MODULE_SCOPE Tcl_NRPostProc TclNRPostInvoke;
MODULE_SCOPE Tcl_NRPostProc TclNRReleaseValues;

MODULE_SCOPE void	TclSetTailcall(Tcl_Interp *interp,
			    Tcl_Obj *tailcallPtr);
MODULE_SCOPE void	TclPushTailcallPoint(Tcl_Interp *interp);

/* These two can be considered for the public api */
MODULE_SCOPE void	TclMarkTailcall(Tcl_Interp *interp);
MODULE_SCOPE void	TclSkipTailcall(Tcl_Interp *interp);

/*
 * This structure holds the data for the various iteration callbacks used to
 * NRE the 'for' and 'while' commands. We need a separate structure because we
 * have more than the 4 client data entries we can provide directly thorugh
 * the callback API. It is the 'word' information which puts us over the
 * limit. It is needed because the loop body is argument 4 of 'for' and
 * argument 2 of 'while'. Not providing the correct index confuses the #280
 * code. We TclSmallAlloc/Free this.
 */

typedef struct ForIterData {
    Tcl_Obj *cond;		/* Loop condition expression. */
    Tcl_Obj *body;		/* Loop body. */
    Tcl_Obj *next;		/* Loop step script, NULL for 'while'. */
    const char *msg;		/* Error message part. */
    Tcl_Size word;		/* Index of the body script in the command */
} ForIterData;

/* TIP #357 - Structure doing the bookkeeping of handles for Tcl_LoadFile
 *            and Tcl_FindSymbol. This structure corresponds to an opaque
 *            typedef in tcl.h */

typedef void* TclFindSymbolProc(Tcl_Interp* interp, Tcl_LoadHandle loadHandle,
	const char* symbol);
struct Tcl_LoadHandle_ {
    void *clientData;		/* Client data is the load handle in the
				 * native filesystem if a module was loaded
				 * there, or an opaque pointer to a structure
				 * for further bookkeeping on load-from-VFS
				 * and load-from-memory */
    TclFindSymbolProc* findSymbolProcPtr;
				/* Procedure that resolves symbols in a
				 * loaded module */
    Tcl_FSUnloadFileProc* unloadFileProcPtr;
				/* Procedure that unloads a loaded module */
};

/* Flags for conversion of doubles to digit strings */

#define TCL_DD_E_FORMAT 0x2	/* Use a fixed-length string of digits,
				 * suitable for E format*/
#define TCL_DD_F_FORMAT 0x3	/* Use a fixed number of digits after the
				 * decimal point, suitable for F format */
#define TCL_DD_SHORTEST 0x4	/* Use the shortest possible string */
#define TCL_DD_NO_QUICK 0x8	/* Debug flag: forbid quick FP conversion */

#define TCL_DD_CONVERSION_TYPE_MASK	0x3
				/* Mask to isolate the conversion type */

/*
 * Clock operations, communicated from command definitions to the bytecode
 * compiler.
 */
enum ClockOps {
    CLOCK_READ_CLICKS = 0,	/* Read the click counter. */
    CLOCK_READ_MICROS = 1,	/* Time in microseconds. */
    CLOCK_READ_MILLIS = 2,	/* Time in milliseconds. */
    CLOCK_READ_SECS = 3		/* Time in seconds. */
};

/*
 *----------------------------------------------------------------
 * Procedures shared among Tcl modules but not used by the outside world:
 *----------------------------------------------------------------
 */

MODULE_SCOPE void	TclAdvanceContinuations(int *line, Tcl_Size **next,
			    Tcl_Size loc);
MODULE_SCOPE void	TclAdvanceLines(int *line, const char *start,
			    const char *end);
MODULE_SCOPE void	TclAppendBytesToByteArray(Tcl_Obj *objPtr,
			    const unsigned char *bytes, Tcl_Size len);
MODULE_SCOPE void	TclAppendUtfToUtf(Tcl_Obj *objPtr,
			    const char *bytes, Tcl_Size numBytes);
MODULE_SCOPE void	TclArgumentEnter(Tcl_Interp *interp,
			    Tcl_Obj *objv[], Tcl_Size objc, CmdFrame *cf);
MODULE_SCOPE void	TclArgumentRelease(Tcl_Interp *interp,
			    Tcl_Obj *objv[], Tcl_Size objc);
MODULE_SCOPE void	TclArgumentBCEnter(Tcl_Interp *interp,
			    Tcl_Obj *objv[], Tcl_Size objc,
			    void *codePtr, CmdFrame *cfPtr, Tcl_Size cmd,
			    Tcl_Size pc);
MODULE_SCOPE void	TclArgumentBCRelease(Tcl_Interp *interp,
			    CmdFrame *cfPtr);
MODULE_SCOPE void	TclArgumentGet(Tcl_Interp *interp, Tcl_Obj *obj,
			    CmdFrame **cfPtrPtr, int *wordPtr);
MODULE_SCOPE int	TclAsyncNotifier(int sigNumber, Tcl_ThreadId threadId,
			    void *clientData, int *flagPtr, int value);
MODULE_SCOPE void	TclAsyncMarkFromNotifier(void);
MODULE_SCOPE double	TclBignumToDouble(const void *bignum);
MODULE_SCOPE int	TclByteArrayMatch(const unsigned char *string,
			    Tcl_Size strLen, const unsigned char *pattern,
			    Tcl_Size ptnLen, int flags);
MODULE_SCOPE double	TclCeil(const void *a);
MODULE_SCOPE void	TclChannelPreserve(Tcl_Channel chan);
MODULE_SCOPE void	TclChannelRelease(Tcl_Channel chan);
MODULE_SCOPE int	TclChannelGetBlockingMode(Tcl_Channel chan);
MODULE_SCOPE int	TclCheckArrayTraces(Tcl_Interp *interp, Var *varPtr,
			    Var *arrayPtr, Tcl_Obj *name, Tcl_Size index);
MODULE_SCOPE int	TclCheckEmptyString(Tcl_Obj *objPtr);
MODULE_SCOPE int	TclChanCaughtErrorBypass(Tcl_Interp *interp,
			    Tcl_Channel chan);
MODULE_SCOPE Tcl_ObjCmdProc TclChannelNamesCmd;
MODULE_SCOPE int	TclChanIsBinary(Tcl_Channel chan);
MODULE_SCOPE Tcl_NRPostProc TclClearRootEnsemble;
MODULE_SCOPE int	TclCompareTwoNumbers(Tcl_Obj *valuePtr,
			    Tcl_Obj *value2Ptr);
MODULE_SCOPE ContLineLoc *TclContinuationsEnter(Tcl_Obj *objPtr, Tcl_Size num,
			    Tcl_Size *loc);
MODULE_SCOPE void	TclContinuationsEnterDerived(Tcl_Obj *objPtr,
			    Tcl_Size start, Tcl_Size *clNext);
MODULE_SCOPE ContLineLoc *TclContinuationsGet(Tcl_Obj *objPtr);
MODULE_SCOPE void	TclContinuationsCopy(Tcl_Obj *objPtr,
			    Tcl_Obj *originObjPtr);
MODULE_SCOPE Tcl_Size	TclConvertElement(const char *src, Tcl_Size length,
			    char *dst, int flags);
MODULE_SCOPE Tcl_Command TclCreateObjCommandInNs(Tcl_Interp *interp,
			    const char *cmdName, Tcl_Namespace *nsPtr,
			    Tcl_ObjCmdProc *proc, void *clientData,
			    Tcl_CmdDeleteProc *deleteProc);
MODULE_SCOPE Tcl_Command TclCreateEnsembleInNs(Tcl_Interp *interp,
			    const char *name, Tcl_Namespace *nameNamespacePtr,
			    Tcl_Namespace *ensembleNamespacePtr, int flags);
MODULE_SCOPE void	TclDeleteNamespaceVars(Namespace *nsPtr);
MODULE_SCOPE void	TclDeleteNamespaceChildren(Namespace *nsPtr);
MODULE_SCOPE Tcl_Size	TclDictGetSize(Tcl_Obj *dictPtr);
MODULE_SCOPE int	TclFindDictElement(Tcl_Interp *interp,
			    const char *dict, Tcl_Size dictLength,
			    const char **elementPtr, const char **nextPtr,
			    Tcl_Size *sizePtr, int *literalPtr);
MODULE_SCOPE Tcl_Obj *	TclDictObjSmartRef(Tcl_Interp *interp, Tcl_Obj *);
MODULE_SCOPE int	TclDictGet(Tcl_Interp *interp, Tcl_Obj *dictPtr,
			    const char *key, Tcl_Obj **valuePtrPtr);
MODULE_SCOPE int	TclDictPut(Tcl_Interp *interp, Tcl_Obj *dictPtr,
			    const char *key, Tcl_Obj *valuePtr);
MODULE_SCOPE int	TclDictPutString(Tcl_Interp *interp, Tcl_Obj *dictPtr,
			    const char *key, const char *value);
MODULE_SCOPE int	TclDictRemove(Tcl_Interp *interp, Tcl_Obj *dictPtr,
			    const char *key);
/* TIP #280 - Modified token based evaluation, with line information. */
MODULE_SCOPE int	TclEvalEx(Tcl_Interp *interp, const char *script,
			    Tcl_Size numBytes, int flags, int line,
			    Tcl_Size *clNextOuter, const char *outerScript);
MODULE_SCOPE Tcl_ObjCmdProc TclFileAttrsCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclFileCopyCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclFileDeleteCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclFileLinkCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclFileMakeDirsCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclFileReadLinkCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclFileRenameCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclFileTempDirCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclFileTemporaryCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclFileHomeCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclFileTildeExpandCmd;
MODULE_SCOPE void	TclCreateLateExitHandler(Tcl_ExitProc *proc,
			    void *clientData);
MODULE_SCOPE void	TclDeleteLateExitHandler(Tcl_ExitProc *proc,
			    void *clientData);
MODULE_SCOPE char *	TclDStringAppendObj(Tcl_DString *dsPtr,
			    Tcl_Obj *objPtr);
MODULE_SCOPE char *	TclDStringAppendDString(Tcl_DString *dsPtr,
			    Tcl_DString *toAppendPtr);
MODULE_SCOPE Tcl_Obj *const *TclFetchEnsembleRoot(Tcl_Interp *interp,
			    Tcl_Obj *const *objv, Tcl_Size objc,
			    Tcl_Size *objcPtr);
MODULE_SCOPE Tcl_Obj *const *TclEnsembleGetRewriteValues(Tcl_Interp *interp);
MODULE_SCOPE Tcl_Namespace *TclEnsureNamespace(Tcl_Interp *interp,
			    Tcl_Namespace *namespacePtr);
MODULE_SCOPE void	TclFinalizeAllocSubsystem(void);
MODULE_SCOPE void	TclFinalizeAsync(void);
MODULE_SCOPE void	TclFinalizeDoubleConversion(void);
MODULE_SCOPE void	TclFinalizeEncodingSubsystem(void);
MODULE_SCOPE void	TclFinalizeEnvironment(void);
MODULE_SCOPE void	TclFinalizeEvaluation(void);
MODULE_SCOPE void	TclFinalizeExecution(void);
MODULE_SCOPE void	TclFinalizeIOSubsystem(void);
MODULE_SCOPE void	TclFinalizeFilesystem(void);
MODULE_SCOPE void	TclResetFilesystem(void);
MODULE_SCOPE void	TclFinalizeLoad(void);
MODULE_SCOPE void	TclFinalizeLock(void);
MODULE_SCOPE void	TclFinalizeMemorySubsystem(void);
MODULE_SCOPE void	TclFinalizeNotifier(void);
MODULE_SCOPE void	TclFinalizeObjects(void);
MODULE_SCOPE void	TclFinalizePreserve(void);
MODULE_SCOPE void	TclFinalizeSynchronization(void);
MODULE_SCOPE void	TclInitThreadAlloc(void);
MODULE_SCOPE void	TclFinalizeThreadAlloc(void);
MODULE_SCOPE void	TclFinalizeThreadAllocThread(void);
MODULE_SCOPE void	TclFinalizeThreadData(int quick);
MODULE_SCOPE void	TclFinalizeThreadObjects(void);
MODULE_SCOPE double	TclFloor(const void *a);
MODULE_SCOPE void	TclFormatNaN(double value, char *buffer);
MODULE_SCOPE int	TclFSFileAttrIndex(Tcl_Obj *pathPtr,
			    const char *attributeName, Tcl_Size *indexPtr);
MODULE_SCOPE Tcl_Command TclNRCreateCommandInNs(Tcl_Interp *interp,
			    const char *cmdName, Tcl_Namespace *nsPtr,
			    Tcl_ObjCmdProc *proc, Tcl_ObjCmdProc *nreProc,
			    void *clientData, Tcl_CmdDeleteProc *deleteProc);
MODULE_SCOPE int	TclNREvalFile(Tcl_Interp *interp, Tcl_Obj *pathPtr,
			    const char *encodingName);
MODULE_SCOPE int *	TclGetAsyncReadyPtr(void);
MODULE_SCOPE Tcl_Obj *	TclGetBgErrorHandler(Tcl_Interp *interp);
MODULE_SCOPE int	TclGetChannelFromObj(Tcl_Interp *interp,
			    Tcl_Obj *objPtr, Tcl_Channel *chanPtr,
			    int *modePtr, int flags);
MODULE_SCOPE CmdFrame *	TclGetCmdFrameForProcedure(Proc *procPtr);
MODULE_SCOPE int	TclGetCompletionCodeFromObj(Tcl_Interp *interp,
			    Tcl_Obj *value, int *code);
MODULE_SCOPE Proc *	TclGetLambdaFromObj(Tcl_Interp *interp,
			    Tcl_Obj *objPtr, Tcl_Obj **nsObjPtrPtr);
MODULE_SCOPE Tcl_Obj *	TclGetProcessGlobalValue(ProcessGlobalValue *pgvPtr);
MODULE_SCOPE Tcl_Obj *	TclGetSourceFromFrame(CmdFrame *cfPtr, Tcl_Size objc,
			    Tcl_Obj *const objv[]);
MODULE_SCOPE char *	TclGetStringStorage(Tcl_Obj *objPtr,
			    Tcl_Size *sizePtr);
MODULE_SCOPE int	TclGetLoadedLibraries(Tcl_Interp *interp,
				const char *targetName,
				const char *prefix);
MODULE_SCOPE int	TclGetWideBitsFromObj(Tcl_Interp *, Tcl_Obj *,
				Tcl_WideInt *);
MODULE_SCOPE int	TclCompareStringKeys(void *keyPtr, Tcl_HashEntry *hPtr);
MODULE_SCOPE size_t	TclHashStringKey(Tcl_HashTable *tablePtr, void *keyPtr);
MODULE_SCOPE int	TclIncrObj(Tcl_Interp *interp, Tcl_Obj *valuePtr,
			    Tcl_Obj *incrPtr);
MODULE_SCOPE Tcl_Obj *	TclIncrObjVar2(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
			    Tcl_Obj *part2Ptr, Tcl_Obj *incrPtr, int flags);
MODULE_SCOPE Tcl_ObjCmdProc TclInfoExistsCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclInfoCoroutineCmd;
MODULE_SCOPE Tcl_Obj *	TclInfoFrame(Tcl_Interp *interp, CmdFrame *framePtr);
MODULE_SCOPE Tcl_ObjCmdProc TclInfoGlobalsCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclInfoLocalsCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclInfoVarsCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclInfoConstsCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclInfoConstantCmd;
MODULE_SCOPE void	TclInitAlloc(void);
MODULE_SCOPE void	TclInitDbCkalloc(void);
MODULE_SCOPE void	TclInitDoubleConversion(void);
MODULE_SCOPE void	TclInitEmbeddedConfigurationInformation(
			    Tcl_Interp *interp);
MODULE_SCOPE void	TclInitEncodingSubsystem(void);
MODULE_SCOPE void	TclInitIOSubsystem(void);
MODULE_SCOPE void	TclInitLimitSupport(Tcl_Interp *interp);
MODULE_SCOPE void	TclInitNamespaceSubsystem(void);
MODULE_SCOPE void	TclInitNotifier(void);
MODULE_SCOPE void	TclInitObjSubsystem(void);
MODULE_SCOPE int	TclInterpReady(Tcl_Interp *interp);
MODULE_SCOPE int	TclIsBareword(int byte);
MODULE_SCOPE Tcl_Obj *	TclJoinPath(Tcl_Size elements, Tcl_Obj * const objv[],
			    int forceRelative);
MODULE_SCOPE Tcl_Obj *	TclGetHomeDirObj(Tcl_Interp *interp, const char *user);
MODULE_SCOPE Tcl_Obj *	TclResolveTildePath(Tcl_Interp *interp,
			    Tcl_Obj *pathObj);
MODULE_SCOPE Tcl_Obj *	TclResolveTildePathList(Tcl_Obj *pathsObj);
MODULE_SCOPE int	TclJoinThread(Tcl_ThreadId id, int *result);
MODULE_SCOPE void	TclLimitRemoveAllHandlers(Tcl_Interp *interp);
MODULE_SCOPE Tcl_Obj *	TclLindexList(Tcl_Interp *interp,
			    Tcl_Obj *listPtr, Tcl_Obj *argPtr);
MODULE_SCOPE Tcl_Obj *	TclLindexFlat(Tcl_Interp *interp, Tcl_Obj *listPtr,
			    Tcl_Size indexCount, Tcl_Obj *const indexArray[]);
MODULE_SCOPE Tcl_Obj *	TclListObjGetElement(Tcl_Obj *listObj, Tcl_Size index);
/* TIP #280 */
MODULE_SCOPE void	TclListLines(Tcl_Obj *listObj, int line, Tcl_Size n,
			    int *lines, Tcl_Obj *const *elems);
MODULE_SCOPE Tcl_Obj *	TclListObjCopy(Tcl_Interp *interp, Tcl_Obj *listPtr);
MODULE_SCOPE int	TclListObjAppendElements(Tcl_Interp *interp,
			    Tcl_Obj *toObj, Tcl_Size elemCount,
			    Tcl_Obj *const elemObjv[]);
MODULE_SCOPE Tcl_Obj *	TclListObjRange(Tcl_Interp *interp, Tcl_Obj *listPtr,
			    Tcl_Size fromIdx, Tcl_Size toIdx);
MODULE_SCOPE Tcl_Obj *	TclLsetList(Tcl_Interp *interp, Tcl_Obj *listPtr,
			    Tcl_Obj *indexPtr, Tcl_Obj *valuePtr);
MODULE_SCOPE Tcl_Obj *	TclLsetFlat(Tcl_Interp *interp, Tcl_Obj *listPtr,
			    Tcl_Size indexCount, Tcl_Obj *const indexArray[],
			    Tcl_Obj *valuePtr);
MODULE_SCOPE Tcl_Command TclMakeEnsemble(Tcl_Interp *interp, const char *name,
			    const EnsembleImplMap map[]);
MODULE_SCOPE Tcl_Size	TclMaxListLength(const char *bytes, Tcl_Size numBytes,
			    const char **endPtr);
MODULE_SCOPE int	TclMergeReturnOptions(Tcl_Interp *interp, Tcl_Size objc,
			    Tcl_Obj *const objv[], Tcl_Obj **optionsPtrPtr,
			    int *codePtr, int *levelPtr);
MODULE_SCOPE Tcl_Obj *	TclNoErrorStack(Tcl_Interp *interp, Tcl_Obj *options);
MODULE_SCOPE int	TclNokia770Doubles(void);
MODULE_SCOPE void	TclNsDecrRefCount(Namespace *nsPtr);
MODULE_SCOPE int	TclNamespaceDeleted(Namespace *nsPtr);
MODULE_SCOPE void	TclObjVarErrMsg(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
			    Tcl_Obj *part2Ptr, const char *operation,
			    const char *reason, Tcl_Size index);
MODULE_SCOPE int	TclObjInvokeNamespace(Tcl_Interp *interp,
			    Tcl_Size objc, Tcl_Obj *const objv[],
			    Tcl_Namespace *nsPtr, int flags);
MODULE_SCOPE int	TclObjUnsetVar2(Tcl_Interp *interp,
			    Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr, int flags);
MODULE_SCOPE Tcl_Size TclParseBackslash(const char *src,
			    Tcl_Size numBytes, Tcl_Size *readPtr, char *dst);
MODULE_SCOPE int	TclParseNumber(Tcl_Interp *interp, Tcl_Obj *objPtr,
			    const char *expected, const char *bytes,
			    Tcl_Size numBytes, const char **endPtrPtr, int flags);
MODULE_SCOPE void	TclParseInit(Tcl_Interp *interp, const char *string,
			    Tcl_Size numBytes, Tcl_Parse *parsePtr);
MODULE_SCOPE Tcl_Size	TclParseAllWhiteSpace(const char *src, Tcl_Size numBytes);
MODULE_SCOPE int	TclProcessReturn(Tcl_Interp *interp,
			    int code, int level, Tcl_Obj *returnOpts);
MODULE_SCOPE void	TclUndoRefCount(Tcl_Obj *objPtr);
MODULE_SCOPE int	TclpObjLstat(Tcl_Obj *pathPtr, Tcl_StatBuf *buf);
MODULE_SCOPE Tcl_Obj *	TclpTempFileName(void);
MODULE_SCOPE Tcl_Obj *	TclpTempFileNameForLibrary(Tcl_Interp *interp,
			    Tcl_Obj* pathPtr);
MODULE_SCOPE Tcl_Obj *	TclNewArithSeriesObj(Tcl_Interp *interp,
			    int useDoubles, Tcl_Obj *startObj, Tcl_Obj *endObj,
			    Tcl_Obj *stepObj, Tcl_Obj *lenObj);
MODULE_SCOPE Tcl_Obj *	TclNewFSPathObj(Tcl_Obj *dirPtr, const char *addStrRep,
			    Tcl_Size len);
MODULE_SCOPE Tcl_Obj *	TclNewNamespaceObj(Tcl_Namespace *namespacePtr);
MODULE_SCOPE void	TclpAlertNotifier(void *clientData);
MODULE_SCOPE void *	TclpNotifierData(void);
MODULE_SCOPE void	TclpServiceModeHook(int mode);
MODULE_SCOPE void	TclpSetTimer(const Tcl_Time *timePtr);
MODULE_SCOPE int	TclpWaitForEvent(const Tcl_Time *timePtr);
MODULE_SCOPE void	TclpCreateFileHandler(int fd, int mask,
			    Tcl_FileProc *proc, void *clientData);
MODULE_SCOPE int	TclpDeleteFile(const void *path);
MODULE_SCOPE void	TclpDeleteFileHandler(int fd);
MODULE_SCOPE void	TclpFinalizeCondition(Tcl_Condition *condPtr);
MODULE_SCOPE void	TclpFinalizeMutex(Tcl_Mutex *mutexPtr);
MODULE_SCOPE void	TclpFinalizeNotifier(void *clientData);
MODULE_SCOPE void	TclpFinalizePipes(void);
MODULE_SCOPE void	TclpFinalizeSockets(void);
#ifdef _WIN32
MODULE_SCOPE void	TclInitSockets(void);
#else
#define TclInitSockets() /* do nothing */
#endif
struct addrinfo; /* forward declaration, needed for TclCreateSocketAddress */
MODULE_SCOPE int	TclCreateSocketAddress(Tcl_Interp *interp,
			    struct addrinfo **addrlist,
			    const char *host, int port, int willBind,
			    const char **errorMsgPtr);
MODULE_SCOPE int	TclpThreadCreate(Tcl_ThreadId *idPtr,
			    Tcl_ThreadCreateProc *proc, void *clientData,
			    size_t stackSize, int flags);
MODULE_SCOPE Tcl_Size	TclpFindVariable(const char *name, Tcl_Size *lengthPtr);
MODULE_SCOPE void	TclpInitLibraryPath(char **valuePtr,
			    size_t *lengthPtr, Tcl_Encoding *encodingPtr);
MODULE_SCOPE void	TclpInitLock(void);
MODULE_SCOPE void *	TclpInitNotifier(void);
MODULE_SCOPE void	TclpInitPlatform(void);
MODULE_SCOPE void	TclpInitUnlock(void);
MODULE_SCOPE Tcl_Obj *	TclpObjListVolumes(void);
MODULE_SCOPE void	TclpGlobalLock(void);
MODULE_SCOPE void	TclpGlobalUnlock(void);
MODULE_SCOPE int	TclpObjNormalizePath(Tcl_Interp *interp,
			    Tcl_Obj *pathPtr, int nextCheckpoint);
MODULE_SCOPE void	TclpNativeJoinPath(Tcl_Obj *prefix, const char *joining);
MODULE_SCOPE Tcl_Obj *	TclpNativeSplitPath(Tcl_Obj *pathPtr, Tcl_Size *lenPtr);
MODULE_SCOPE Tcl_PathType TclpGetNativePathType(Tcl_Obj *pathPtr,
			    Tcl_Size *driveNameLengthPtr, Tcl_Obj **driveNameRef);
MODULE_SCOPE int	TclCrossFilesystemCopy(Tcl_Interp *interp,
			    Tcl_Obj *source, Tcl_Obj *target);
MODULE_SCOPE int	TclpMatchInDirectory(Tcl_Interp *interp,
			    Tcl_Obj *resultPtr, Tcl_Obj *pathPtr,
			    const char *pattern, Tcl_GlobTypeData *types);
MODULE_SCOPE void	*TclpGetNativeCwd(void *clientData);
MODULE_SCOPE Tcl_FSDupInternalRepProc TclNativeDupInternalRep;
MODULE_SCOPE Tcl_Obj *	TclpObjLink(Tcl_Obj *pathPtr, Tcl_Obj *toPtr,
			    int linkType);
MODULE_SCOPE int	TclpObjChdir(Tcl_Obj *pathPtr);
MODULE_SCOPE Tcl_Channel TclpOpenTemporaryFile(Tcl_Obj *dirObj,
			    Tcl_Obj *basenameObj, Tcl_Obj *extensionObj,
			    Tcl_Obj *resultingNameObj);
MODULE_SCOPE void	TclPkgFileSeen(Tcl_Interp *interp,
			    const char *fileName);
MODULE_SCOPE void *	TclInitPkgFiles(Tcl_Interp *interp);
MODULE_SCOPE Tcl_Obj *	TclPathPart(Tcl_Interp *interp, Tcl_Obj *pathPtr,
			    Tcl_PathPart portion);
MODULE_SCOPE char *	TclpReadlink(const char *fileName,
			    Tcl_DString *linkPtr);
MODULE_SCOPE void	TclpSetVariables(Tcl_Interp *interp);
MODULE_SCOPE void *	TclThreadStorageKeyGet(Tcl_ThreadDataKey *keyPtr);
MODULE_SCOPE void	TclThreadStorageKeySet(Tcl_ThreadDataKey *keyPtr,
			    void *data);
MODULE_SCOPE TCL_NORETURN void TclpThreadExit(int status);
MODULE_SCOPE void	TclRememberCondition(Tcl_Condition *mutex);
MODULE_SCOPE void	TclRememberJoinableThread(Tcl_ThreadId id);
MODULE_SCOPE void	TclRememberMutex(Tcl_Mutex *mutex);
MODULE_SCOPE void	TclRemoveScriptLimitCallbacks(Tcl_Interp *interp);
MODULE_SCOPE int	TclReToGlob(Tcl_Interp *interp, const char *reStr,
			    Tcl_Size reStrLen, Tcl_DString *dsPtr, int *flagsPtr,
			    int *quantifiersFoundPtr);
MODULE_SCOPE Tcl_Size	TclScanElement(const char *string, Tcl_Size length,
			    char *flagPtr);
MODULE_SCOPE void	TclSetBgErrorHandler(Tcl_Interp *interp,
			    Tcl_Obj *cmdPrefix);
MODULE_SCOPE void	TclSetBignumInternalRep(Tcl_Obj *objPtr,
			    void *bignumValue);
MODULE_SCOPE int	TclSetBooleanFromAny(Tcl_Interp *interp,
			    Tcl_Obj *objPtr);
MODULE_SCOPE void	TclSetCmdNameObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
			    Command *cmdPtr);
MODULE_SCOPE void	TclSetDuplicateObj(Tcl_Obj *dupPtr, Tcl_Obj *objPtr);
MODULE_SCOPE void	TclSetProcessGlobalValue(ProcessGlobalValue *pgvPtr,
			    Tcl_Obj *newValue);
MODULE_SCOPE void	TclSignalExitThread(Tcl_ThreadId id, int result);
MODULE_SCOPE void	TclSpellFix(Tcl_Interp *interp,
			    Tcl_Obj *const *objv, Tcl_Size objc, Tcl_Size subIdx,
			    Tcl_Obj *bad, Tcl_Obj *fix);
MODULE_SCOPE void *	TclStackRealloc(Tcl_Interp *interp, void *ptr,
			    size_t numBytes);
typedef int (*memCmpFn_t)(const void*, const void*, size_t);
MODULE_SCOPE int	TclStringCmp(Tcl_Obj *value1Ptr, Tcl_Obj *value2Ptr,
			    int checkEq, int nocase, Tcl_Size reqlength);
MODULE_SCOPE int	TclStringMatch(const char *str, Tcl_Size strLen,
			    const char *pattern, int ptnLen, int flags);
MODULE_SCOPE int	TclStringMatchObj(Tcl_Obj *stringObj,
			    Tcl_Obj *patternObj, int flags);
MODULE_SCOPE void	TclSubstCompile(Tcl_Interp *interp, const char *bytes,
			    Tcl_Size numBytes, int flags, int line,
			    struct CompileEnv *envPtr);
MODULE_SCOPE int	TclSubstOptions(Tcl_Interp *interp, Tcl_Size numOpts,
			    Tcl_Obj *const opts[], int *flagPtr);
MODULE_SCOPE void	TclSubstParse(Tcl_Interp *interp, const char *bytes,
			    Tcl_Size numBytes, int flags, Tcl_Parse *parsePtr,
			    Tcl_InterpState *statePtr);
MODULE_SCOPE int	TclSubstTokens(Tcl_Interp *interp, Tcl_Token *tokenPtr,
			    Tcl_Size count, Tcl_Size *tokensLeftPtr, int line,
			    Tcl_Size *clNextOuter, const char *outerScript);
MODULE_SCOPE Tcl_Size	TclTrim(const char *bytes, Tcl_Size numBytes,
			    const char *trim, Tcl_Size numTrim,
			    Tcl_Size *trimRight);
MODULE_SCOPE Tcl_Size	TclTrimLeft(const char *bytes, Tcl_Size numBytes,
			    const char *trim, Tcl_Size numTrim);
MODULE_SCOPE Tcl_Size	TclTrimRight(const char *bytes, Tcl_Size numBytes,
			    const char *trim, Tcl_Size numTrim);
MODULE_SCOPE const char*TclGetCommandTypeName(Tcl_Command command);
MODULE_SCOPE int	TclObjInterpProc(void *clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
MODULE_SCOPE void	TclRegisterCommandTypeName(
			    Tcl_ObjCmdProc *implementationProc,
			    const char *nameStr);
MODULE_SCOPE int	TclUtfCmp(const char *cs, const char *ct);
MODULE_SCOPE int	TclUtfCasecmp(const char *cs, const char *ct);
MODULE_SCOPE int	TclUtfCount(int ch);
MODULE_SCOPE Tcl_Obj *	TclpNativeToNormalized(void *clientData);
MODULE_SCOPE Tcl_Obj *	TclpFilesystemPathType(Tcl_Obj *pathPtr);
MODULE_SCOPE int	TclpDlopen(Tcl_Interp *interp, Tcl_Obj *pathPtr,
			    Tcl_LoadHandle *loadHandle,
			    Tcl_FSUnloadFileProc **unloadProcPtr, int flags);
MODULE_SCOPE int	TclpUtime(Tcl_Obj *pathPtr, struct utimbuf *tval);
#ifdef TCL_LOAD_FROM_MEMORY
MODULE_SCOPE void *	TclpLoadMemoryGetBuffer(size_t size);
MODULE_SCOPE int	TclpLoadMemory(void *buffer, size_t size,
			    Tcl_Size codeSize,  const char *path, Tcl_LoadHandle *loadHandle,
			    Tcl_FSUnloadFileProc **unloadProcPtr, int flags);
#endif
MODULE_SCOPE void	TclInitThreadStorage(void);
MODULE_SCOPE void	TclFinalizeThreadDataThread(void);
MODULE_SCOPE void	TclFinalizeThreadStorage(void);

#ifdef TCL_WIDE_CLICKS
MODULE_SCOPE long long	TclpGetWideClicks(void);
MODULE_SCOPE double	TclpWideClicksToNanoseconds(long long clicks);
MODULE_SCOPE double	TclpWideClickInMicrosec(void);
#else
#   ifdef _WIN32
#	define TCL_WIDE_CLICKS 1
MODULE_SCOPE long long	TclpGetWideClicks(void);
MODULE_SCOPE double	TclpWideClickInMicrosec(void);
#	define TclpWideClicksToNanoseconds(clicks) \
		((double)(clicks) * TclpWideClickInMicrosec() * 1000)
#   endif
#endif
MODULE_SCOPE long long	TclpGetMicroseconds(void);

MODULE_SCOPE int	TclZlibInit(Tcl_Interp *interp);
MODULE_SCOPE void *	TclpThreadCreateKey(void);
MODULE_SCOPE void	TclpThreadDeleteKey(void *keyPtr);
MODULE_SCOPE void	TclpThreadSetGlobalTSD(void *tsdKeyPtr, void *ptr);
MODULE_SCOPE void *	TclpThreadGetGlobalTSD(void *tsdKeyPtr);
MODULE_SCOPE void	TclErrorStackResetIf(Tcl_Interp *interp,
			    const char *msg, Tcl_Size length);
/* Tip 430 */
MODULE_SCOPE int	TclZipfs_Init(Tcl_Interp *interp);
MODULE_SCOPE int	TclIsZipfsPath(const char *path);
MODULE_SCOPE void	TclZipfsFinalize(void);
MODULE_SCOPE int 	TclZipfsLocateTclLibrary(void);

/*
 * Many parsing tasks need a common definition of whitespace.
 * Use this routine and macro to achieve that and place
 * optimization (fragile on changes) in one place.
 */

MODULE_SCOPE int	TclIsSpaceProc(int byte);
#define TclIsSpaceProcM(byte) \
    (((unsigned)(byte) > 0x20) ? 0 : TclIsSpaceProc(byte))

/*
 *----------------------------------------------------------------
 * Command procedures in the generic core:
 *----------------------------------------------------------------
 */

MODULE_SCOPE Tcl_ObjCmdProc Tcl_AfterObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_AppendObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ApplyObjCmd;
MODULE_SCOPE Tcl_Command TclInitArrayCmd(Tcl_Interp *interp);
MODULE_SCOPE Tcl_Command TclInitBinaryCmd(Tcl_Interp *interp);
MODULE_SCOPE Tcl_ObjCmdProc Tcl_BreakObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_CatchObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_CdObjCmd;
MODULE_SCOPE Tcl_Command TclInitChanCmd(Tcl_Interp *interp);
MODULE_SCOPE Tcl_ObjCmdProc TclChanCreateObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclChanPostEventObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclChanPopObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclChanPushObjCmd;
MODULE_SCOPE void	TclClockInit(Tcl_Interp *interp);
MODULE_SCOPE Tcl_ObjCmdProc TclClockOldscanObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_CloseObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ConcatObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ConstObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ContinueObjCmd;
MODULE_SCOPE Tcl_TimerToken TclCreateAbsoluteTimerHandler(
			    Tcl_Time *timePtr, Tcl_TimerProc *proc,
			    void *clientData);
MODULE_SCOPE Tcl_ObjCmdProc TclDefaultBgErrorHandlerObjCmd;
MODULE_SCOPE Tcl_Command TclInitDictCmd(Tcl_Interp *interp);
MODULE_SCOPE int	TclDictWithFinish(Tcl_Interp *interp, Var *varPtr,
			    Var *arrayPtr, Tcl_Obj *part1Ptr,
			    Tcl_Obj *part2Ptr, Tcl_Size index, Tcl_Size pathc,
			    Tcl_Obj *const pathv[], Tcl_Obj *keysPtr);
MODULE_SCOPE Tcl_Obj *	TclDictWithInit(Tcl_Interp *interp, Tcl_Obj *dictPtr,
			    Tcl_Size pathc, Tcl_Obj *const pathv[]);
MODULE_SCOPE Tcl_ObjCmdProc Tcl_DisassembleObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclLoadIcuObjCmd;

/* Assemble command function */
MODULE_SCOPE Tcl_ObjCmdProc Tcl_AssembleObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRAssembleObjCmd;
MODULE_SCOPE Tcl_Command TclInitEncodingCmd(Tcl_Interp *interp);
MODULE_SCOPE Tcl_ObjCmdProc Tcl_EofObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ErrorObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_EvalObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ExecObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ExitObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ExprObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_FblockedObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_FconfigureObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_FcopyObjCmd;
MODULE_SCOPE Tcl_Command TclInitFileCmd(Tcl_Interp *interp);
MODULE_SCOPE Tcl_ObjCmdProc Tcl_FileEventObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_FlushObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ForObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ForeachObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_FormatObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_GetsObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_GlobalObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_GlobObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_IfObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_IncrObjCmd;
MODULE_SCOPE Tcl_Command TclInitInfoCmd(Tcl_Interp *interp);
MODULE_SCOPE Tcl_ObjCmdProc Tcl_InterpObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_JoinObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LappendObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LassignObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LeditObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LindexObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LinsertObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LlengthObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ListObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LmapObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LoadObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LpopObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LrangeObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LremoveObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LrepeatObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LreplaceObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LreverseObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LsearchObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LseqObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LsetObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_LsortObjCmd;
MODULE_SCOPE Tcl_Command TclInitNamespaceCmd(Tcl_Interp *interp);
MODULE_SCOPE Tcl_ObjCmdProc TclNamespaceEnsembleCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_OpenObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_PackageObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_PidObjCmd;
MODULE_SCOPE Tcl_Command TclInitPrefixCmd(Tcl_Interp *interp);
MODULE_SCOPE Tcl_ObjCmdProc Tcl_PutsObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_PwdObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ReadObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_RegexpObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_RegsubObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_RenameObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_RepresentationCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ReturnObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ScanObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_SeekObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_SetObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_SplitObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_SocketObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_SourceObjCmd;
MODULE_SCOPE Tcl_Command TclInitStringCmd(Tcl_Interp *interp);
MODULE_SCOPE Tcl_ObjCmdProc Tcl_SubstObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_SwitchObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_TellObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_ThrowObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_TimeObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_TimeRateObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_TraceObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_TryObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_UnloadObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_UnsetObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_UpdateObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_UplevelObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_UpvarObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_VariableObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_VwaitObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc Tcl_WhileObjCmd;

/*
 *----------------------------------------------------------------
 * Compilation procedures for commands in the generic core:
 *----------------------------------------------------------------
 */

MODULE_SCOPE CompileProc TclCompileAppendCmd;
MODULE_SCOPE CompileProc TclCompileArrayExistsCmd;
MODULE_SCOPE CompileProc TclCompileArraySetCmd;
MODULE_SCOPE CompileProc TclCompileArrayUnsetCmd;
MODULE_SCOPE CompileProc TclCompileBreakCmd;
MODULE_SCOPE CompileProc TclCompileCatchCmd;
MODULE_SCOPE CompileProc TclCompileClockClicksCmd;
MODULE_SCOPE CompileProc TclCompileClockReadingCmd;
MODULE_SCOPE CompileProc TclCompileConcatCmd;
MODULE_SCOPE CompileProc TclCompileConstCmd;
MODULE_SCOPE CompileProc TclCompileContinueCmd;
MODULE_SCOPE CompileProc TclCompileDictAppendCmd;
MODULE_SCOPE CompileProc TclCompileDictCreateCmd;
MODULE_SCOPE CompileProc TclCompileDictExistsCmd;
MODULE_SCOPE CompileProc TclCompileDictForCmd;
MODULE_SCOPE CompileProc TclCompileDictGetCmd;
MODULE_SCOPE CompileProc TclCompileDictGetWithDefaultCmd;
MODULE_SCOPE CompileProc TclCompileDictIncrCmd;
MODULE_SCOPE CompileProc TclCompileDictLappendCmd;
MODULE_SCOPE CompileProc TclCompileDictMapCmd;
MODULE_SCOPE CompileProc TclCompileDictMergeCmd;
MODULE_SCOPE CompileProc TclCompileDictRemoveCmd;
MODULE_SCOPE CompileProc TclCompileDictReplaceCmd;
MODULE_SCOPE CompileProc TclCompileDictSetCmd;
MODULE_SCOPE CompileProc TclCompileDictUnsetCmd;
MODULE_SCOPE CompileProc TclCompileDictUpdateCmd;
MODULE_SCOPE CompileProc TclCompileDictWithCmd;
MODULE_SCOPE CompileProc TclCompileEnsemble;
MODULE_SCOPE CompileProc TclCompileErrorCmd;
MODULE_SCOPE CompileProc TclCompileExprCmd;
MODULE_SCOPE CompileProc TclCompileForCmd;
MODULE_SCOPE CompileProc TclCompileForeachCmd;
MODULE_SCOPE CompileProc TclCompileFormatCmd;
MODULE_SCOPE CompileProc TclCompileGlobalCmd;
MODULE_SCOPE CompileProc TclCompileIfCmd;
MODULE_SCOPE CompileProc TclCompileInfoCommandsCmd;
MODULE_SCOPE CompileProc TclCompileInfoCoroutineCmd;
MODULE_SCOPE CompileProc TclCompileInfoExistsCmd;
MODULE_SCOPE CompileProc TclCompileInfoLevelCmd;
MODULE_SCOPE CompileProc TclCompileInfoObjectClassCmd;
MODULE_SCOPE CompileProc TclCompileInfoObjectCreationIdCmd;
MODULE_SCOPE CompileProc TclCompileInfoObjectIsACmd;
MODULE_SCOPE CompileProc TclCompileInfoObjectNamespaceCmd;
MODULE_SCOPE CompileProc TclCompileIncrCmd;
MODULE_SCOPE CompileProc TclCompileLappendCmd;
MODULE_SCOPE CompileProc TclCompileLassignCmd;
MODULE_SCOPE CompileProc TclCompileLeditCmd;
MODULE_SCOPE CompileProc TclCompileLindexCmd;
MODULE_SCOPE CompileProc TclCompileLinsertCmd;
MODULE_SCOPE CompileProc TclCompileListCmd;
MODULE_SCOPE CompileProc TclCompileLlengthCmd;
MODULE_SCOPE CompileProc TclCompileLmapCmd;
MODULE_SCOPE CompileProc TclCompileLpopCmd;
MODULE_SCOPE CompileProc TclCompileLrangeCmd;
MODULE_SCOPE CompileProc TclCompileLreplaceCmd;
MODULE_SCOPE CompileProc TclCompileLseqCmd;
MODULE_SCOPE CompileProc TclCompileLsetCmd;
MODULE_SCOPE CompileProc TclCompileNamespaceCodeCmd;
MODULE_SCOPE CompileProc TclCompileNamespaceCurrentCmd;
MODULE_SCOPE CompileProc TclCompileNamespaceOriginCmd;
MODULE_SCOPE CompileProc TclCompileNamespaceQualifiersCmd;
MODULE_SCOPE CompileProc TclCompileNamespaceTailCmd;
MODULE_SCOPE CompileProc TclCompileNamespaceUpvarCmd;
MODULE_SCOPE CompileProc TclCompileNamespaceWhichCmd;
MODULE_SCOPE CompileProc TclCompileNoOp;
MODULE_SCOPE CompileProc TclCompileObjectNextCmd;
MODULE_SCOPE CompileProc TclCompileObjectNextToCmd;
MODULE_SCOPE CompileProc TclCompileObjectSelfCmd;
MODULE_SCOPE CompileProc TclCompileRegexpCmd;
MODULE_SCOPE CompileProc TclCompileRegsubCmd;
MODULE_SCOPE CompileProc TclCompileReturnCmd;
MODULE_SCOPE CompileProc TclCompileSetCmd;
MODULE_SCOPE CompileProc TclCompileStringCatCmd;
MODULE_SCOPE CompileProc TclCompileStringCmpCmd;
MODULE_SCOPE CompileProc TclCompileStringEqualCmd;
MODULE_SCOPE CompileProc TclCompileStringFirstCmd;
MODULE_SCOPE CompileProc TclCompileStringIndexCmd;
MODULE_SCOPE CompileProc TclCompileStringInsertCmd;
MODULE_SCOPE CompileProc TclCompileStringIsCmd;
MODULE_SCOPE CompileProc TclCompileStringLastCmd;
MODULE_SCOPE CompileProc TclCompileStringLenCmd;
MODULE_SCOPE CompileProc TclCompileStringMapCmd;
MODULE_SCOPE CompileProc TclCompileStringMatchCmd;
MODULE_SCOPE CompileProc TclCompileStringRangeCmd;
MODULE_SCOPE CompileProc TclCompileStringReplaceCmd;
MODULE_SCOPE CompileProc TclCompileStringToLowerCmd;
MODULE_SCOPE CompileProc TclCompileStringToTitleCmd;
MODULE_SCOPE CompileProc TclCompileStringToUpperCmd;
MODULE_SCOPE CompileProc TclCompileStringTrimCmd;
MODULE_SCOPE CompileProc TclCompileStringTrimLCmd;
MODULE_SCOPE CompileProc TclCompileStringTrimRCmd;
MODULE_SCOPE CompileProc TclCompileSubstCmd;
MODULE_SCOPE CompileProc TclCompileSwitchCmd;
MODULE_SCOPE CompileProc TclCompileTailcallCmd;
MODULE_SCOPE CompileProc TclCompileThrowCmd;
MODULE_SCOPE CompileProc TclCompileTryCmd;
MODULE_SCOPE CompileProc TclCompileUnsetCmd;
MODULE_SCOPE CompileProc TclCompileUplevelCmd;
MODULE_SCOPE CompileProc TclCompileUpvarCmd;
MODULE_SCOPE CompileProc TclCompileVariableCmd;
MODULE_SCOPE CompileProc TclCompileWhileCmd;
MODULE_SCOPE CompileProc TclCompileYieldCmd;
MODULE_SCOPE CompileProc TclCompileYieldToCmd;
MODULE_SCOPE CompileProc TclCompileBasic0ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasic1ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasic2ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasic3ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasic0Or1ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasic1Or2ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasic2Or3ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasic0To2ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasic1To3ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasicMin0ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasicMin1ArgCmd;
MODULE_SCOPE CompileProc TclCompileBasicMin2ArgCmd;

MODULE_SCOPE Tcl_ObjCmdProc TclInvertOpCmd;
MODULE_SCOPE CompileProc TclCompileInvertOpCmd;

MODULE_SCOPE Tcl_ObjCmdProc TclNotOpCmd;
MODULE_SCOPE CompileProc TclCompileNotOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclAddOpCmd;
MODULE_SCOPE CompileProc TclCompileAddOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclMulOpCmd;
MODULE_SCOPE CompileProc TclCompileMulOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclAndOpCmd;
MODULE_SCOPE CompileProc TclCompileAndOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclOrOpCmd;
MODULE_SCOPE CompileProc TclCompileOrOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclXorOpCmd;
MODULE_SCOPE CompileProc TclCompileXorOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclPowOpCmd;
MODULE_SCOPE CompileProc TclCompilePowOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclLshiftOpCmd;
MODULE_SCOPE CompileProc TclCompileLshiftOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclRshiftOpCmd;
MODULE_SCOPE CompileProc TclCompileRshiftOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclModOpCmd;
MODULE_SCOPE CompileProc TclCompileModOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNeqOpCmd;
MODULE_SCOPE CompileProc TclCompileNeqOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclStrneqOpCmd;
MODULE_SCOPE CompileProc TclCompileStrneqOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclInOpCmd;
MODULE_SCOPE CompileProc TclCompileInOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNiOpCmd;
MODULE_SCOPE CompileProc TclCompileNiOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclMinusOpCmd;
MODULE_SCOPE CompileProc TclCompileMinusOpCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclDivOpCmd;
MODULE_SCOPE CompileProc TclCompileDivOpCmd;
MODULE_SCOPE CompileProc TclCompileLessOpCmd;
MODULE_SCOPE CompileProc TclCompileLeqOpCmd;
MODULE_SCOPE CompileProc TclCompileGreaterOpCmd;
MODULE_SCOPE CompileProc TclCompileGeqOpCmd;
MODULE_SCOPE CompileProc TclCompileEqOpCmd;
MODULE_SCOPE CompileProc TclCompileStreqOpCmd;
MODULE_SCOPE CompileProc TclCompileStrLtOpCmd;
MODULE_SCOPE CompileProc TclCompileStrLeOpCmd;
MODULE_SCOPE CompileProc TclCompileStrGtOpCmd;
MODULE_SCOPE CompileProc TclCompileStrGeOpCmd;

MODULE_SCOPE CompileProc TclCompileAssembleCmd;

/*
 * Routines that provide the [string] ensemble functionality. Possible
 * candidates for public interface.
 */

MODULE_SCOPE Tcl_Obj *	TclStringCat(Tcl_Interp *interp, Tcl_Size objc,
			    Tcl_Obj *const objv[], int flags);
MODULE_SCOPE Tcl_Obj *	TclStringFirst(Tcl_Obj *needle, Tcl_Obj *haystack,
			    Tcl_Size start);
MODULE_SCOPE Tcl_Obj *	TclStringLast(Tcl_Obj *needle, Tcl_Obj *haystack,
			    Tcl_Size last);
MODULE_SCOPE Tcl_Obj *	TclStringRepeat(Tcl_Interp *interp, Tcl_Obj *objPtr,
			    Tcl_Size count, int flags);
MODULE_SCOPE Tcl_Obj *	TclStringReplace(Tcl_Interp *interp, Tcl_Obj *objPtr,
			    Tcl_Size first, Tcl_Size count, Tcl_Obj *insertPtr,
			    int flags);
MODULE_SCOPE Tcl_Obj *	TclStringReverse(Tcl_Obj *objPtr, int flags);

/* Flag values for the [string] ensemble functions. */
enum StringOpFlags {
    TCL_STRING_MATCH_NOCASE = TCL_MATCH_NOCASE, /* (1<<0) in tcl.h */
    TCL_STRING_IN_PLACE = (1<<1)	/* Do in-place surgery on Tcl_Obj */
};

/*
 * Functions defined in generic/tclVar.c and currently exported only for use
 * by the bytecode compiler and engine. Some of these could later be placed in
 * the public interface.
 */

MODULE_SCOPE Var *	TclObjLookupVarEx(Tcl_Interp * interp,
			    Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr, int flags,
			    const char *msg, int createPart1,
			    int createPart2, Var **arrayPtrPtr);
MODULE_SCOPE Var *	TclLookupArrayElement(Tcl_Interp *interp,
			    Tcl_Obj *arrayNamePtr, Tcl_Obj *elNamePtr,
			    int flags, const char *msg,
			    int createPart1, int createPart2,
			    Var *arrayPtr, Tcl_Size index);
MODULE_SCOPE Tcl_Obj *	TclPtrGetVarIdx(Tcl_Interp *interp,
			    Var *varPtr, Var *arrayPtr, Tcl_Obj *part1Ptr,
			    Tcl_Obj *part2Ptr, int flags, Tcl_Size index);
MODULE_SCOPE Tcl_Obj *	TclPtrSetVarIdx(Tcl_Interp *interp,
			    Var *varPtr, Var *arrayPtr, Tcl_Obj *part1Ptr,
			    Tcl_Obj *part2Ptr, Tcl_Obj *newValuePtr,
			    int flags, Tcl_Size index);
MODULE_SCOPE Tcl_Obj *	TclPtrIncrObjVarIdx(Tcl_Interp *interp,
			    Var *varPtr, Var *arrayPtr, Tcl_Obj *part1Ptr,
			    Tcl_Obj *part2Ptr, Tcl_Obj *incrPtr,
			    int flags, Tcl_Size index);
MODULE_SCOPE int	TclPtrObjMakeUpvarIdx(Tcl_Interp *interp,
			    Var *otherPtr, Tcl_Obj *myNamePtr, int myFlags,
			    Tcl_Size index);
MODULE_SCOPE int	TclPtrUnsetVarIdx(Tcl_Interp *interp, Var *varPtr,
			    Var *arrayPtr, Tcl_Obj *part1Ptr,
			    Tcl_Obj *part2Ptr, int flags,
			    Tcl_Size index);
MODULE_SCOPE void	TclInvalidateNsPath(Namespace *nsPtr);
MODULE_SCOPE void	TclFindArrayPtrElements(Var *arrayPtr,
			    Tcl_HashTable *tablePtr);

/*
 * The new extended interface to the variable traces.
 */

MODULE_SCOPE int	TclObjCallVarTraces(Interp *iPtr, Var *arrayPtr,
			    Var *varPtr, Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr,
			    int flags, int leaveErrMsg, Tcl_Size index);

/*
 * So tclObj.c and tclDictObj.c can share these implementations.
 */

MODULE_SCOPE int	TclCompareObjKeys(void *keyPtr, Tcl_HashEntry *hPtr);
MODULE_SCOPE void	TclFreeObjEntry(Tcl_HashEntry *hPtr);
MODULE_SCOPE size_t TclHashObjKey(Tcl_HashTable *tablePtr, void *keyPtr);

MODULE_SCOPE int	TclFullFinalizationRequested(void);

/*
 * TIP #542
 */

MODULE_SCOPE size_t	TclUniCharLen(const Tcl_UniChar *uniStr);
MODULE_SCOPE int	TclUniCharNcmp(const Tcl_UniChar *ucs,
			    const Tcl_UniChar *uct, size_t numChars);
MODULE_SCOPE int	TclUniCharNcasecmp(const Tcl_UniChar *ucs,
			    const Tcl_UniChar *uct, size_t numChars);
MODULE_SCOPE int	TclUniCharCaseMatch(const Tcl_UniChar *uniStr,
			    const Tcl_UniChar *uniPattern, int nocase);

/*
 * Just for the purposes of command-type registration.
 */

MODULE_SCOPE Tcl_ObjCmdProc TclEnsembleImplementationCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclAliasObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclLocalAliasObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclChildObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclInvokeImportedCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclOOPublicObjectCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclOOPrivateObjectCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclOOMyClassObjCmd;

/*
 * TIP #462.
 */

/*
 * The following enum values give the status of a spawned process.
 */

typedef enum TclProcessWaitStatus {
    TCL_PROCESS_ERROR = -1,	/* Error waiting for process to exit */
    TCL_PROCESS_UNCHANGED = 0,	/* No change since the last call. */
    TCL_PROCESS_EXITED = 1,	/* Process has exited. */
    TCL_PROCESS_SIGNALED = 2,	/* Child killed because of a signal. */
    TCL_PROCESS_STOPPED = 3,	/* Child suspended because of a signal. */
    TCL_PROCESS_UNKNOWN_STATUS = 4
				/* Child wait status didn't make sense. */
} TclProcessWaitStatus;

MODULE_SCOPE Tcl_Command TclInitProcessCmd(Tcl_Interp *interp);
MODULE_SCOPE void	TclProcessCreated(Tcl_Pid pid);
MODULE_SCOPE TclProcessWaitStatus TclProcessWait(Tcl_Pid pid, int options,
			    int *codePtr, Tcl_Obj **msgObjPtr,
			    Tcl_Obj **errorObjPtr);
MODULE_SCOPE int	TclClose(Tcl_Interp *, Tcl_Channel chan);

/*
 * TIP #508: [array default]
 */

MODULE_SCOPE void	TclInitArrayVar(Var *arrayPtr);
MODULE_SCOPE Tcl_Obj *	TclGetArrayDefault(Var *arrayPtr);

/*
 * Utility routines for encoding index values as integers. Used by both
 * some of the command compilers and by [lsort] and [lsearch].
 */

MODULE_SCOPE int	TclIndexEncode(Tcl_Interp *interp, Tcl_Obj *objPtr,
			    int before, int after, int *indexPtr);
MODULE_SCOPE Tcl_Size	TclIndexDecode(int encoded, Tcl_Size endValue);

/*
 * Error message utility functions
 */
MODULE_SCOPE int	TclCommandWordLimitError(Tcl_Interp *interp,
			    Tcl_Size count);
MODULE_SCOPE int	TclListLimitExceededError(Tcl_Interp *interp);

/* Constants used in index value encoding routines. */
#define TCL_INDEX_END	((Tcl_Size)-2)
#define TCL_INDEX_START	((Tcl_Size)0)

/*
 *----------------------------------------------------------------------
 *
 * TclScaleTime --
 *
 *	TIP #233 (Virtualized Time): Wrapper around the time virutalisation
 *	rescale function to hide the binding of the clientData.
 *
 *	This is static inline code; it's like a macro, but a function. It's
 *	used because this is a piece of code that ends up in places that are a
 *	bit performance sensitive.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Updates the time structure (given as an argument) with what the time
 *	should be after virtualisation.
 *
 *----------------------------------------------------------------------
 */

static inline void
TclScaleTime(
    Tcl_Time *timePtr)
{
    if (timePtr != NULL) {
	tclScaleTimeProcPtr(timePtr, tclTimeClientData);
    }
}

/*
 *----------------------------------------------------------------
 * Macros used by the Tcl core to create and release Tcl objects.
 * TclNewObj(objPtr) creates a new object denoting an empty string.
 * TclDecrRefCount(objPtr) decrements the object's reference count, and frees
 * the object if its reference count is zero. These macros are inline versions
 * of Tcl_NewObj() and Tcl_DecrRefCount(). Notice that the names differ in not
 * having a "_" after the "Tcl". Notice also that these macros reference their
 * argument more than once, so you should avoid calling them with an
 * expression that is expensive to compute or has side effects. The ANSI C
 * "prototypes" for these macros are:
 *
 * MODULE_SCOPE void	TclNewObj(Tcl_Obj *objPtr);
 * MODULE_SCOPE void	TclDecrRefCount(Tcl_Obj *objPtr);
 *
 * These macros are defined in terms of two macros that depend on memory
 * allocator in use: TclAllocObjStorage, TclFreeObjStorage. They are defined
 * below.
 *----------------------------------------------------------------
 */

/*
 * DTrace object allocation probe macros.
 */

#ifdef USE_DTRACE
#ifndef _TCLDTRACE_H
#include "tclDTrace.h"
#endif
#define	TCL_DTRACE_OBJ_CREATE(objPtr)	TCL_OBJ_CREATE(objPtr)
#define	TCL_DTRACE_OBJ_FREE(objPtr)	TCL_OBJ_FREE(objPtr)
#else /* USE_DTRACE */
#define	TCL_DTRACE_OBJ_CREATE(objPtr)	{}
#define	TCL_DTRACE_OBJ_FREE(objPtr)	{}
#endif /* USE_DTRACE */

#ifdef TCL_COMPILE_STATS
#  define TclIncrObjsAllocated() \
    tclObjsAlloced++
#  define TclIncrObjsFreed() \
    tclObjsFreed++
#else
#  define TclIncrObjsAllocated()
#  define TclIncrObjsFreed()
#endif /* TCL_COMPILE_STATS */

#  define TclAllocObjStorage(objPtr) \
	TclAllocObjStorageEx(NULL, (objPtr))

#  define TclFreeObjStorage(objPtr) \
	TclFreeObjStorageEx(NULL, (objPtr))

#ifndef TCL_MEM_DEBUG
# define TclNewObj(objPtr) \
    TclIncrObjsAllocated();						\
    TclAllocObjStorage(objPtr);						\
    (objPtr)->refCount = 0;						\
    (objPtr)->bytes    = &tclEmptyString;				\
    (objPtr)->length   = 0;						\
    (objPtr)->typePtr  = NULL;						\
    TCL_DTRACE_OBJ_CREATE(objPtr)

/*
 * Invalidate the string rep first so we can use the bytes value for our
 * pointer chain, and signal an obj deletion (as opposed to shimmering) with
 * 'length == TCL_INDEX_NONE'.
 * Use empty 'if ; else' to handle use in unbraced outer if/else conditions.
 */

# define TclDecrRefCount(objPtr) \
    if ((objPtr)->refCount-- > 1) ; else {				\
	if (!(objPtr)->typePtr || !(objPtr)->typePtr->freeIntRepProc) {	\
	    TCL_DTRACE_OBJ_FREE(objPtr);				\
	    if ((objPtr)->bytes						\
		    && ((objPtr)->bytes != &tclEmptyString)) {		\
		Tcl_Free((objPtr)->bytes);				\
	    }								\
	    (objPtr)->length = TCL_INDEX_NONE;				\
	    TclFreeObjStorage(objPtr);					\
	    TclIncrObjsFreed();						\
	} else {							\
	    TclFreeObj(objPtr);						\
	}								\
    }

#if TCL_THREADS && !defined(USE_THREAD_ALLOC)
#   define USE_THREAD_ALLOC 1
#endif

#if defined(PURIFY)

/*
 * The PURIFY mode is like the regular mode, but instead of doing block
 * Tcl_Obj allocation and keeping a freed list for efficiency, it always
 * allocates and frees a single Tcl_Obj so that tools like Purify can better
 * track memory leaks.
 */

#  define TclAllocObjStorageEx(interp, objPtr) \
	(objPtr) = (Tcl_Obj *)Tcl_Alloc(sizeof(Tcl_Obj))

#  define TclFreeObjStorageEx(interp, objPtr) \
	Tcl_Free(objPtr)

#undef USE_THREAD_ALLOC
#undef USE_TCLALLOC
#elif TCL_THREADS && defined(USE_THREAD_ALLOC)

/*
 * The TCL_THREADS mode is like the regular mode but allocates Tcl_Obj's from
 * per-thread caches.
 */

MODULE_SCOPE Tcl_Obj *	TclThreadAllocObj(void);
MODULE_SCOPE void	TclThreadFreeObj(Tcl_Obj *);
MODULE_SCOPE Tcl_Mutex *TclpNewAllocMutex(void);
MODULE_SCOPE void	TclFreeAllocCache(void *);
MODULE_SCOPE void *	TclpGetAllocCache(void);
MODULE_SCOPE void	TclpSetAllocCache(void *);
MODULE_SCOPE void	TclpFreeAllocMutex(Tcl_Mutex *mutex);
MODULE_SCOPE void	TclpInitAllocCache(void);
MODULE_SCOPE void	TclpFreeAllocCache(void *);

/*
 * These macros need to be kept in sync with the code of TclThreadAllocObj()
 * and TclThreadFreeObj().
 *
 * Note that the optimiser should resolve the case (interp==NULL) at compile
 * time.
 */

#  define ALLOC_NOBJHIGH 1200

#  define TclAllocObjStorageEx(interp, objPtr)				\
    do {								\
	AllocCache *cachePtr;						\
	if (((interp) == NULL) ||					\
		((cachePtr = ((Interp *)(interp))->allocCache),		\
			(cachePtr->numObjects == 0))) {			\
	    (objPtr) = TclThreadAllocObj();				\
	} else {							\
	    (objPtr) = cachePtr->firstObjPtr;				\
	    cachePtr->firstObjPtr = (Tcl_Obj *)(objPtr)->internalRep.twoPtrValue.ptr1; \
	    --cachePtr->numObjects;					\
	}								\
    } while (0)

#  define TclFreeObjStorageEx(interp, objPtr)				\
    do {								\
	AllocCache *cachePtr;						\
	if (((interp) == NULL) ||					\
		((cachePtr = ((Interp *)(interp))->allocCache),		\
			((cachePtr->numObjects == 0) ||			\
			(cachePtr->numObjects >= ALLOC_NOBJHIGH)))) {	\
	    TclThreadFreeObj(objPtr);					\
	} else {							\
	    (objPtr)->internalRep.twoPtrValue.ptr1 = cachePtr->firstObjPtr; \
	    cachePtr->firstObjPtr = objPtr;				\
	    ++cachePtr->numObjects;					\
	}								\
    } while (0)

#else /* not PURIFY or USE_THREAD_ALLOC */

#if defined(USE_TCLALLOC) && USE_TCLALLOC
    MODULE_SCOPE void TclFinalizeAllocSubsystem();
    MODULE_SCOPE void TclInitAlloc();
#else
#   define USE_TCLALLOC 0
#endif

#if TCL_THREADS
/* declared in tclObj.c */
MODULE_SCOPE Tcl_Mutex	tclObjMutex;
#endif

#  define TclAllocObjStorageEx(interp, objPtr) \
    do {								\
	Tcl_MutexLock(&tclObjMutex);					\
	if (tclFreeObjList == NULL) {					\
	    TclAllocateFreeObjects();					\
	}								\
	(objPtr) = tclFreeObjList;					\
	tclFreeObjList = (Tcl_Obj *)					\
		tclFreeObjList->internalRep.twoPtrValue.ptr1;		\
	Tcl_MutexUnlock(&tclObjMutex);					\
    } while (0)

#  define TclFreeObjStorageEx(interp, objPtr) \
    do {								\
	Tcl_MutexLock(&tclObjMutex);					\
	(objPtr)->internalRep.twoPtrValue.ptr1 = (void *) tclFreeObjList; \
	tclFreeObjList = (objPtr);					\
	Tcl_MutexUnlock(&tclObjMutex);					\
    } while (0)
#endif

#else /* TCL_MEM_DEBUG */
MODULE_SCOPE void	TclDbInitNewObj(Tcl_Obj *objPtr, const char *file,
			    int line);

# define TclDbNewObj(objPtr, file, line) \
    do { \
	TclIncrObjsAllocated();						\
	(objPtr) = (Tcl_Obj *)						\
		Tcl_DbCkalloc(sizeof(Tcl_Obj), (file), (line));		\
	TclDbInitNewObj((objPtr), (file), (line));			\
	TCL_DTRACE_OBJ_CREATE(objPtr);					\
    } while (0)

# define TclNewObj(objPtr) \
    TclDbNewObj(objPtr, __FILE__, __LINE__);

# define TclDecrRefCount(objPtr) \
    Tcl_DbDecrRefCount(objPtr, __FILE__, __LINE__)

#undef USE_THREAD_ALLOC
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------
 * Macros used by the Tcl core to set a Tcl_Obj's string representation to a
 * copy of the "len" bytes starting at "bytePtr". The value of "len" must
 * not be negative.  When "len" is 0, then it is acceptable to pass
 * "bytePtr" = NULL.  When "len" > 0, "bytePtr" must not be NULL, and it
 * must point to a location from which "len" bytes may be read.  These
 * constraints are not checked here.  The validity of the bytes copied
 * as a value string representation is also not verififed.  This macro
 * must not be called while "objPtr" is being freed or when "objPtr"
 * already has a string representation.  The caller must use
 * this macro properly.  Improper use can lead to dangerous results.
 * Because "len" is referenced multiple times, take care that it is an
 * expression with the same value each use.
 *
 * The ANSI C "prototypes" for these macros are:
 *
 * MODULE_SCOPE void TclInitEmptyStringRep(Tcl_Obj *objPtr);
 * MODULE_SCOPE void TclInitStringRep(Tcl_Obj *objPtr, char *bytePtr, size_t len);
 * MODULE_SCOPE const char *TclAttemptInitStringRep(Tcl_Obj *objPtr, char *bytePtr, size_t len);
 *
 *----------------------------------------------------------------
 */

#define TclInitEmptyStringRep(objPtr) \
    ((objPtr)->length = (((objPtr)->bytes = &tclEmptyString), 0))

#define TclInitStringRep(objPtr, bytePtr, len) \
    if ((len) == 0) {							\
	TclInitEmptyStringRep(objPtr);					\
    } else {								\
	(objPtr)->bytes = (char *)Tcl_Alloc((len) + 1U);		\
	memcpy((objPtr)->bytes, (bytePtr) ? (bytePtr) : &tclEmptyString, (len)); \
	(objPtr)->bytes[len] = '\0';					\
	(objPtr)->length = (len);					\
    }

#define TclAttemptInitStringRep(objPtr, bytePtr, len) \
    ((((len) == 0) ? (							\
	TclInitEmptyStringRep(objPtr)					\
    ) : (								\
	(objPtr)->bytes = (char *)Tcl_AttemptAlloc((len) + 1U),		\
	(objPtr)->length = ((objPtr)->bytes) ?				\
		(memcpy((objPtr)->bytes, (bytePtr) ? (bytePtr) : &tclEmptyString, (len)), \
		(objPtr)->bytes[len] = '\0', (Tcl_Size)(len)) : (-1)		\
    )), (objPtr)->bytes)

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to get the string representation's byte array
 * pointer from a Tcl_Obj. This is an inline version of Tcl_GetString(). The
 * macro's expression result is the string rep's byte pointer which might be
 * NULL. The bytes referenced by this pointer must not be modified by the
 * caller. The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE char *	TclGetString(Tcl_Obj *objPtr);
 *----------------------------------------------------------------
 */

#define TclGetString(objPtr) \
    ((objPtr)->bytes? (objPtr)->bytes : Tcl_GetString(objPtr))

#define TclGetStringFromObj(objPtr, lenPtr) \
    ((objPtr)->bytes							\
	    ? (*(lenPtr) = (objPtr)->length, (objPtr)->bytes)		\
	    : (Tcl_GetStringFromObj)((objPtr), (lenPtr)))

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to clean out an object's internal
 * representation. Does not actually reset the rep's bytes. The ANSI C
 * "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclFreeInternalRep(Tcl_Obj *objPtr);
 *----------------------------------------------------------------
 */

#define TclFreeInternalRep(objPtr) \
    if ((objPtr)->typePtr != NULL) {					\
	if ((objPtr)->typePtr->freeIntRepProc != NULL) {		\
	    (objPtr)->typePtr->freeIntRepProc(objPtr);			\
	}								\
	(objPtr)->typePtr = NULL;					\
    }

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to clean out an object's string representation.
 * The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclInvalidateStringRep(Tcl_Obj *objPtr);
 *----------------------------------------------------------------
 */

#define TclInvalidateStringRep(objPtr) \
    do {								\
	Tcl_Obj *_isobjPtr = (Tcl_Obj *)(objPtr);			\
	if (_isobjPtr->bytes != NULL) {					\
	    if (_isobjPtr->bytes != &tclEmptyString) {			\
		Tcl_Free((void *)_isobjPtr->bytes);			\
	    }								\
	    _isobjPtr->bytes = NULL;					\
	}								\
    } while (0)

/*
 * These form part of the native filesystem support. They are needed here
 * because we have a few native filesystem functions (which are the same for
 * win/unix) in this file.
 */

#ifdef __cplusplus
extern "C" {
#endif
MODULE_SCOPE const char *const		tclpFileAttrStrings[];
MODULE_SCOPE const TclFileAttrProcs	tclpFileAttrProcs[];
#ifdef __cplusplus
}
#endif

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to test whether an object has a
 * string representation (or is a 'pure' internal value).
 * The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE int	TclHasStringRep(Tcl_Obj *objPtr);
 *----------------------------------------------------------------
 */

#define TclHasStringRep(objPtr) \
    ((objPtr)->bytes != NULL)

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to get the bignum out of the bignum
 * representation of a Tcl_Obj.
 * The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclUnpackBignum(Tcl_Obj *objPtr, mp_int bignum);
 *----------------------------------------------------------------
 */

#define TclUnpackBignum(objPtr, bignum) \
    do {								\
	Tcl_Obj *bignumObj = (objPtr);					\
	int bignumPayload =						\
		(int)PTR2INT(bignumObj->internalRep.twoPtrValue.ptr2);	\
	if (bignumPayload == -1) {					\
	    (bignum) = *((mp_int *) bignumObj->internalRep.twoPtrValue.ptr1); \
	} else {							\
	    (bignum).dp = (mp_digit *)bignumObj->internalRep.twoPtrValue.ptr1;	\
	    (bignum).sign = bignumPayload >> 30;			\
	    (bignum).alloc = (bignumPayload >> 15) & 0x7FFF;		\
	    (bignum).used = bignumPayload & 0x7FFF;			\
	}								\
    } while (0)

/*
 *----------------------------------------------------------------
 * Macros used by the Tcl core to grow Tcl_Token arrays. They use the same
 * growth algorithm as used in tclStringObj.c for growing strings. The ANSI C
 * "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclGrowTokenArray(Tcl_Token *tokenPtr, int used,
 *				int available, int append,
 *				Tcl_Token *staticPtr);
 * MODULE_SCOPE void	TclGrowParseTokenArray(Tcl_Parse *parsePtr,
 *				int append);
 *----------------------------------------------------------------
 */

/* General tuning for minimum growth in Tcl growth algorithms */
#ifndef TCL_MIN_GROWTH
#  ifdef TCL_GROWTH_MIN_ALLOC
     /* Support for any legacy tuners */
#    define TCL_MIN_GROWTH TCL_GROWTH_MIN_ALLOC
#  else
#    define TCL_MIN_GROWTH 1024
#  endif
#endif

/* Token growth tuning, default to the general value. */
#ifndef TCL_MIN_TOKEN_GROWTH
#define TCL_MIN_TOKEN_GROWTH TCL_MIN_GROWTH/sizeof(Tcl_Token)
#endif

/* TODO - code below does not check for integer overflow */
#define TclGrowTokenArray(tokenPtr, used, available, append, staticPtr)	\
    do {								\
	Tcl_Size _needed = (used) + (append);				\
	if (_needed > (available)) {					\
	    Tcl_Size allocated = 2 * _needed;				\
	    Tcl_Token *oldPtr = (tokenPtr);				\
	    Tcl_Token *newPtr;						\
	    if (oldPtr == (staticPtr)) {				\
		oldPtr = NULL;						\
	    }								\
	    newPtr = (Tcl_Token *)Tcl_AttemptRealloc((char *) oldPtr,	\
		    allocated * sizeof(Tcl_Token));			\
	    if (newPtr == NULL) {					\
		allocated = _needed + (append) + TCL_MIN_TOKEN_GROWTH;	\
		newPtr = (Tcl_Token *)Tcl_Realloc((char *) oldPtr,	\
			allocated * sizeof(Tcl_Token));			\
	    }								\
	    (available) = allocated;					\
	    if (oldPtr == NULL) {					\
		memcpy(newPtr, staticPtr,				\
			(used) * sizeof(Tcl_Token));			\
	    }								\
	    (tokenPtr) = newPtr;					\
	}								\
    } while (0)

#define TclGrowParseTokenArray(parsePtr, append)			\
    TclGrowTokenArray((parsePtr)->tokenPtr, (parsePtr)->numTokens,	\
	    (parsePtr)->tokensAvailable, (append),			\
	    (parsePtr)->staticTokens)

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core get a unicode char from a utf string. It checks
 * to see if we have a one-byte utf char before calling the real
 * Tcl_UtfToUniChar, as this will save a lot of time for primarily ASCII
 * string handling. The macro's expression result is 1 for the 1-byte case or
 * the result of Tcl_UtfToUniChar. The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE int	TclUtfToUniChar(const char *string, Tcl_UniChar *ch);
 *----------------------------------------------------------------
 */

#define TclUtfToUniChar(str, chPtr) \
	(((UCHAR(*(str))) < 0x80) ?					\
	    ((*(chPtr) = UCHAR(*(str))), 1)				\
	    : Tcl_UtfToUniChar(str, chPtr))

/*
 *----------------------------------------------------------------
 * Macro counterpart of the Tcl_NumUtfChars() function. To be used in speed-
 * -sensitive points where it pays to avoid a function call in the common case
 * of counting along a string of all one-byte characters.  The ANSI C
 * "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclNumUtfCharsM(Tcl_Size numChars, const char *bytes,
 *				Tcl_Size numBytes);
 * numBytes must be >= 0
 *----------------------------------------------------------------
 */

#define TclNumUtfCharsM(numChars, bytes, numBytes) \
    do {								\
	Tcl_Size _count, _i = (numBytes);				\
	unsigned char *_str = (unsigned char *) (bytes);		\
	while (_i > 0 && (*_str < 0xC0)) { _i--; _str++; }		\
	_count = (numBytes) - _i;					\
	if (_i) {							\
	    _count += Tcl_NumUtfChars((bytes) + _count, _i);		\
	}								\
	(numChars) = _count;						\
    } while (0);

/*
 *----------------------------------------------------------------
 * Macro that encapsulates the logic that determines when it is safe to
 * interpret a string as a byte array directly. In summary, the object must be
 * a byte array and must not have a string representation (as the operations
 * that it is used in are defined on strings, not byte arrays). Theoretically
 * it is possible to also be efficient in the case where the object's bytes
 * field is filled by generation from the byte array (c.f. list canonicality)
 * but we don't do that at the moment since this is purely about efficiency.
 * The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE int	TclIsPureByteArray(Tcl_Obj *objPtr);
 *----------------------------------------------------------------
 */

MODULE_SCOPE int	TclIsPureByteArray(Tcl_Obj *objPtr);
#define TclIsPureDict(objPtr) \
    (((objPtr)->bytes == NULL) && TclHasInternalRep((objPtr), &tclDictType))
#define TclHasInternalRep(objPtr, type) \
    ((objPtr)->typePtr == (type))
#define TclFetchInternalRep(objPtr, type) \
    (TclHasInternalRep((objPtr), (type)) ? &(objPtr)->internalRep : NULL)

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to increment a namespace's export epoch
 * counter. The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclInvalidateNsCmdLookup(Namespace *nsPtr);
 *----------------------------------------------------------------
 */

#define TclInvalidateNsCmdLookup(nsPtr) \
    if ((nsPtr)->numExportPatterns) {		\
	(nsPtr)->exportLookupEpoch++;		\
    }						\
    if ((nsPtr)->commandPathLength) {		\
	(nsPtr)->cmdRefEpoch++;			\
    }

/*
 *----------------------------------------------------------------------
 *
 * Core procedure added to libtommath for bignum manipulation.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE Tcl_LibraryInitProc TclTommath_Init;

/*
 *----------------------------------------------------------------------
 *
 * External (platform specific) initialization routine, these declarations
 * explicitly don't use EXTERN since this code does not get compiled into the
 * library:
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE Tcl_LibraryInitProc TclplatformtestInit;
MODULE_SCOPE Tcl_LibraryInitProc TclObjTest_Init;
MODULE_SCOPE Tcl_LibraryInitProc TclThread_Init;
MODULE_SCOPE Tcl_LibraryInitProc Procbodytest_Init;
MODULE_SCOPE Tcl_LibraryInitProc Procbodytest_SafeInit;
MODULE_SCOPE Tcl_LibraryInitProc Tcl_ABSListTest_Init;

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to check whether a pattern has any characters
 * special to [string match]. The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE int	TclMatchIsTrivial(const char *pattern);
 *----------------------------------------------------------------
 */

#define TclMatchIsTrivial(pattern) \
    (strpbrk((pattern), "*[?\\") == NULL)

/*
 *----------------------------------------------------------------
 * Macros used by the Tcl core to set a Tcl_Obj's numeric representation
 * avoiding the corresponding function calls in time critical parts of the
 * core. They should only be called on unshared objects. The ANSI C
 * "prototypes" for these macros are:
 *
 * MODULE_SCOPE void	TclSetIntObj(Tcl_Obj *objPtr, Tcl_WideInt w);
 * MODULE_SCOPE void	TclSetDoubleObj(Tcl_Obj *objPtr, double d);
 *----------------------------------------------------------------
 */

#define TclSetIntObj(objPtr, i) \
    do {							\
	Tcl_ObjInternalRep ir;					\
	ir.wideValue = (Tcl_WideInt) i;				\
	TclInvalidateStringRep(objPtr);				\
	Tcl_StoreInternalRep(objPtr, &tclIntType, &ir);		\
    } while (0)

#define TclSetDoubleObj(objPtr, d) \
    do {							\
	Tcl_ObjInternalRep ir;					\
	ir.doubleValue = (double) d;				\
	TclInvalidateStringRep(objPtr);				\
	Tcl_StoreInternalRep(objPtr, &tclDoubleType, &ir);	\
    } while (0)

/*
 *----------------------------------------------------------------
 * Macros used by the Tcl core to create and initialise objects of standard
 * types, avoiding the corresponding function calls in time critical parts of
 * the core. The ANSI C "prototypes" for these macros are:
 *
 * MODULE_SCOPE void	TclNewIntObj(Tcl_Obj *objPtr, Tcl_WideInt w);
 * MODULE_SCOPE void	TclNewDoubleObj(Tcl_Obj *objPtr, double d);
 * MODULE_SCOPE void	TclNewStringObj(Tcl_Obj *objPtr, const char *s, Tcl_Size len);
 * MODULE_SCOPE void	TclNewLiteralStringObj(Tcl_Obj*objPtr, const char *sLiteral);
 *
 *----------------------------------------------------------------
 */

#ifndef TCL_MEM_DEBUG
#define TclNewIntObj(objPtr, w) \
    do {								\
	TclIncrObjsAllocated();						\
	TclAllocObjStorage(objPtr);					\
	(objPtr)->refCount = 0;						\
	(objPtr)->bytes = NULL;						\
	(objPtr)->internalRep.wideValue = (Tcl_WideInt)(w);		\
	(objPtr)->typePtr = &tclIntType;				\
	TCL_DTRACE_OBJ_CREATE(objPtr);					\
    } while (0)

#define TclNewUIntObj(objPtr, uw) \
    do {								\
	TclIncrObjsAllocated();						\
	TclAllocObjStorage(objPtr);					\
	(objPtr)->refCount = 0;						\
	(objPtr)->bytes = NULL;						\
	Tcl_WideUInt uw_ = (uw);					\
	if (uw_ > WIDE_MAX) {						\
	    mp_int bignumValue_;					\
	    if (mp_init_u64(&bignumValue_, uw_) != MP_OKAY) {		\
		Tcl_Panic("%s: memory overflow", "TclNewUIntObj");	\
	    }								\
	    TclSetBignumInternalRep((objPtr), &bignumValue_);		\
	} else {							\
	    (objPtr)->internalRep.wideValue = (Tcl_WideInt)(uw_);	\
	    (objPtr)->typePtr = &tclIntType;				\
	}								\
	TCL_DTRACE_OBJ_CREATE(objPtr);					\
    } while (0)

#define TclNewIndexObj(objPtr, w) \
    TclNewIntObj(objPtr, w)

#define TclNewDoubleObj(objPtr, d) \
    do {								\
	TclIncrObjsAllocated();						\
	TclAllocObjStorage(objPtr);					\
	(objPtr)->refCount = 0;						\
	(objPtr)->bytes = NULL;						\
	(objPtr)->internalRep.doubleValue = (double)(d);		\
	(objPtr)->typePtr = &tclDoubleType;				\
	TCL_DTRACE_OBJ_CREATE(objPtr);					\
    } while (0)

#define TclNewStringObj(objPtr, s, len) \
    do {								\
	TclIncrObjsAllocated();						\
	TclAllocObjStorage(objPtr);					\
	(objPtr)->refCount = 0;						\
	TclInitStringRep((objPtr), (s), (len));				\
	(objPtr)->typePtr = NULL;					\
	TCL_DTRACE_OBJ_CREATE(objPtr);					\
    } while (0)

#else /* TCL_MEM_DEBUG */
#define TclNewIntObj(objPtr, w) \
    (objPtr) = Tcl_NewWideIntObj(w)

#define TclNewUIntObj(objPtr, uw) \
    do {								\
	Tcl_WideUInt uw_ = (uw);					\
	if (uw_ > WIDE_MAX) {						\
	    mp_int bignumValue_;					\
	    if (mp_init_u64(&bignumValue_, uw_) == MP_OKAY) {		\
		(objPtr) = Tcl_NewBignumObj(&bignumValue_);		\
	    } else {							\
		(objPtr) = NULL;					\
	    }								\
	} else {							\
	    (objPtr) = Tcl_NewWideIntObj(uw_);				\
	}								\
    } while (0)

#define TclNewIndexObj(objPtr, w) \
    TclNewIntObj(objPtr, w)

#define TclNewDoubleObj(objPtr, d) \
    (objPtr) = Tcl_NewDoubleObj(d)

#define TclNewStringObj(objPtr, s, len) \
    (objPtr) = Tcl_NewStringObj((s), (len))
#endif /* TCL_MEM_DEBUG */

/*
 * The sLiteral argument *must* be a string literal; the incantation with
 * sizeof(sLiteral "") will fail to compile otherwise.
 */
#define TclNewLiteralStringObj(objPtr, sLiteral) \
    TclNewStringObj((objPtr), (sLiteral), sizeof(sLiteral "") - 1)

/*
 *----------------------------------------------------------------
 * Convenience macros for DStrings.
 * The ANSI C "prototypes" for these macros are:
 *
 * MODULE_SCOPE char * TclDStringAppendLiteral(Tcl_DString *dsPtr,
 *			const char *sLiteral);
 * MODULE_SCOPE void   TclDStringClear(Tcl_DString *dsPtr);
 */

#define TclDStringAppendLiteral(dsPtr, sLiteral) \
    Tcl_DStringAppend((dsPtr), (sLiteral), sizeof(sLiteral "") - 1)
#define TclDStringClear(dsPtr) \
    Tcl_DStringSetLength((dsPtr), 0)

/*
 *----------------------------------------------------------------
 * Inline version of Tcl_GetCurrentNamespace and Tcl_GetGlobalNamespace.
 */

#define TclGetCurrentNamespace(interp) \
    (Tcl_Namespace *) ((Interp *)(interp))->varFramePtr->nsPtr

#define TclGetGlobalNamespace(interp) \
    (Tcl_Namespace *) ((Interp *)(interp))->globalNsPtr

/*
 *----------------------------------------------------------------
 * Inline version of TclCleanupCommand; still need the function as it is in
 * the internal stubs, but the core can use the macro instead.
 */

#define TclCleanupCommandMacro(cmdPtr) \
    do {					\
	if ((cmdPtr)->refCount-- <= 1) {	\
	    Tcl_Free(cmdPtr);			\
	}					\
    } while (0)

/*
 * inside this routine crement refCount first incase cmdPtr is replacing itself
 */
#define TclRoutineAssign(location, cmdPtr) \
    do {								\
	(cmdPtr)->refCount++;						\
	if ((location) != NULL						\
		&& (location--) <= 1) {					\
	    Tcl_Free(((location)));					\
	}								\
	(location) = (cmdPtr);						\
    } while (0)

#define TclRoutineHasName(cmdPtr) \
    ((cmdPtr)->hPtr != NULL)

/*
 *----------------------------------------------------------------
 * Inline versions of Tcl_LimitReady() and Tcl_LimitExceeded to limit number
 * of calls out of the critical path. Note that this code isn't particularly
 * readable; the non-inline version (in tclInterp.c) is much easier to
 * understand. Note also that these macros takes different args (iPtr->limit)
 * to the non-inline version.
 */

#define TclLimitExceeded(limit) \
    ((limit).exceeded != 0)

#define TclLimitReady(limit) \
    (((limit).active == 0) ? 0 :					\
    (++(limit).granularityTicker,					\
    ((((limit).active & TCL_LIMIT_COMMANDS) &&				\
	    (((limit).cmdGranularity == 1) ||				\
	    ((limit).granularityTicker % (limit).cmdGranularity == 0)))	\
	    ? 1 :							\
    (((limit).active & TCL_LIMIT_TIME) &&				\
	    (((limit).timeGranularity == 1) ||				\
	    ((limit).granularityTicker % (limit).timeGranularity == 0)))\
	    ? 1 : 0)))

/*
 * Compile-time assertions: these produce a compile time error if the
 * expression is not known to be true at compile time. If the assertion is
 * known to be false, the compiler (or optimizer?) will error out with
 * "division by zero". If the assertion cannot be evaluated at compile time,
 * the compiler will error out with "non-static initializer".
 *
 * Adapted with permission from
 * http://www.pixelbeat.org/programming/gcc/static_assert.html
 */

#define TCL_CT_ASSERT(e) \
    {enum { ct_assert_value = 1/(!!(e)) };}

/*
 *----------------------------------------------------------------
 * Allocator for small structs (<=sizeof(Tcl_Obj)) using the Tcl_Obj pool.
 * Only checked at compile time.
 *
 * ONLY USE FOR CONSTANT nBytes.
 *
 * DO NOT LET THEM CROSS THREAD BOUNDARIES
 *----------------------------------------------------------------
 */

#define TclSmallAlloc(nbytes, memPtr) \
    TclSmallAllocEx(NULL, (nbytes), (memPtr))

#define TclSmallFree(memPtr) \
    TclSmallFreeEx(NULL, (memPtr))

#ifndef TCL_MEM_DEBUG
#define TclSmallAllocEx(interp, nbytes, memPtr) \
    do {								\
	Tcl_Obj *_objPtr;						\
	TCL_CT_ASSERT((nbytes)<=sizeof(Tcl_Obj));			\
	TclIncrObjsAllocated();						\
	TclAllocObjStorageEx((interp), (_objPtr));			\
	*(void **)&(memPtr) = (void *) (_objPtr);			\
    } while (0)

#define TclSmallFreeEx(interp, memPtr) \
    do {								\
	TclFreeObjStorageEx((interp), (Tcl_Obj *)(memPtr));		\
	TclIncrObjsFreed();						\
    } while (0)

#else    /* TCL_MEM_DEBUG */
#define TclSmallAllocEx(interp, nbytes, memPtr) \
    do {								\
	Tcl_Obj *_objPtr;						\
	TCL_CT_ASSERT((nbytes)<=sizeof(Tcl_Obj));			\
	TclNewObj(_objPtr);						\
	*(void **)&(memPtr) = (void *)_objPtr;				\
    } while (0)

#define TclSmallFreeEx(interp, memPtr) \
    do {								\
	Tcl_Obj *_objPtr = (Tcl_Obj *)(memPtr);				\
	_objPtr->bytes = NULL;						\
	_objPtr->typePtr = NULL;					\
	_objPtr->refCount = 1;						\
	TclDecrRefCount(_objPtr);					\
    } while (0)
#endif   /* TCL_MEM_DEBUG */

/*
 * Support for Clang Static Analyzer <http://clang-analyzer.llvm.org>
 */

#if defined(PURIFY) && defined(__clang__)
#if __has_feature(attribute_analyzer_noreturn) && \
	!defined(Tcl_Panic) && defined(Tcl_Panic_TCL_DECLARED)
void Tcl_Panic(const char *, ...) __attribute__((analyzer_noreturn));
#endif
#if !defined(CLANG_ASSERT)
#include <assert.h>
#define CLANG_ASSERT(x) assert(x)
#endif
#elif !defined(CLANG_ASSERT)
#define CLANG_ASSERT(x)
#endif /* PURIFY && __clang__ */

/*
 *----------------------------------------------------------------
 * Parameters, structs and macros for the non-recursive engine (NRE)
 *----------------------------------------------------------------
 */

#define NRE_USE_SMALL_ALLOC	1  /* Only turn off for debugging purposes. */
#ifndef NRE_ENABLE_ASSERTS
#define NRE_ENABLE_ASSERTS	0
#endif

/*
 * This is the main data struct for representing NR commands. It is designed
 * to fit in sizeof(Tcl_Obj) in order to exploit the fastest memory allocator
 * available.
 */

typedef struct NRE_callback {
    Tcl_NRPostProc *procPtr;
    void *data[4];
    struct NRE_callback *nextPtr;
} NRE_callback;

#define TOP_CB(iPtr) \
    (((Interp *)(iPtr))->execEnvPtr->callbackPtr)

/*
 * Inline version of Tcl_NRAddCallback.
 */

#define TclNRAddCallback(interp,postProcPtr,data0,data1,data2,data3) \
    do {								\
	NRE_callback *_callbackPtr;					\
	TCLNR_ALLOC((interp), (_callbackPtr));				\
	_callbackPtr->procPtr = (postProcPtr);				\
	_callbackPtr->data[0] = (void *)(data0);			\
	_callbackPtr->data[1] = (void *)(data1);			\
	_callbackPtr->data[2] = (void *)(data2);			\
	_callbackPtr->data[3] = (void *)(data3);			\
	_callbackPtr->nextPtr = TOP_CB(interp);				\
	TOP_CB(interp) = _callbackPtr;					\
    } while (0)

#if NRE_USE_SMALL_ALLOC
#define TCLNR_ALLOC(interp, ptr) \
    TclSmallAllocEx(interp, sizeof(NRE_callback), (ptr))
#define TCLNR_FREE(interp, ptr)  TclSmallFreeEx((interp), (ptr))
#else
#define TCLNR_ALLOC(interp, ptr) \
    ((ptr) = Tcl_Alloc(sizeof(NRE_callback)))
#define TCLNR_FREE(interp, ptr)  Tcl_Free(ptr)
#endif

#if NRE_ENABLE_ASSERTS
#define NRE_ASSERT(expr) assert((expr))
#else
#define NRE_ASSERT(expr)
#endif

#include "tclIntDecls.h"
#include "tclIntPlatDecls.h"

#if !defined(USE_TCL_STUBS) && !defined(TCL_MEM_DEBUG)
#define Tcl_AttemptAlloc	TclpAlloc
#define Tcl_AttemptRealloc	TclpRealloc
#define Tcl_Free		TclpFree
#endif

/*
 * Special hack for macOS, where the static linker (technically the 'ar'
 * command) hates empty object files, and accepts no flags to make it shut up.
 *
 * These symbols are otherwise completely useless.
 *
 * They can't be written to or written through. They can't be seen by any
 * other code. They use a separate attribute (supported by all macOS
 * compilers, which are derivatives of clang or gcc) to stop the compilation
 * from moaning. They will be excluded during the final linking stage.
 *
 * Other platforms get nothing at all. That's good.
 */

#ifdef MAC_OSX_TCL
#define TCL_MAC_EMPTY_FILE(name) \
    static __attribute__((used)) const void *const TclUnusedFile_ ## name = NULL;
#else
#define TCL_MAC_EMPTY_FILE(name)
#endif /* MAC_OSX_TCL */

/*
 * Other externals.
 */

MODULE_SCOPE size_t TclEnvEpoch;	/* Epoch of the tcl environment
					 * (if changed with tcl-env). */

#endif /* _TCLINT */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
