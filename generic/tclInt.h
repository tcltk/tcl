/*
 * tclInt.h --
 *
 *	Declarations of things used internally by the Tcl interpreter.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1993-1997 Lucent Technologies.
 * Copyright (c) 1994-1998 Sun Microsystems, Inc.
 * Copyright (c) 1998-19/99 by Scriptics Corporation.
 * Copyright (c) 2001, 2002 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclInt.h,v 1.214.2.7 2005/04/11 09:11:45 msofer Exp $
 */

#ifndef _TCLINT
#define _TCLINT

/*
 * Common include files needed by most of the Tcl source files are
 * included here, so that system-dependent personalizations for the
 * include files only have to be made in once place.  This results
 * in a few extra includes, but greater modularity.  The order of
 * the three groups of #includes is important.	For example, stdio.h
 * is needed by tcl.h, and the _ANSI_ARGS_ declaration in tcl.h is
 * needed by stdlib.h in some configurations.
 */

#ifdef HAVE_TCL_CONFIG_H
#include "tclConfig.h"
#endif
#ifndef _TCL
#include "tcl.h"
#endif

#include <stdio.h>

#include <ctype.h>
#ifdef NO_LIMITS_H
#   include "../compat/limits.h"
#else
#   include <limits.h>
#endif
#ifdef NO_STDLIB_H
#   include "../compat/stdlib.h"
#else
#   include <stdlib.h>
#endif
#ifdef NO_STRING_H
#include "../compat/string.h"
#else
#include <string.h>
#endif
#ifdef STDC_HEADERS
#include <stddef.h>
#else
typedef int ptrdiff_t;
#endif

/*
 * Used to tag functions that are only to be visible within the module
 * being built and not outside it (where this is supported by the
 * linker).
 */

#ifndef MODULE_SCOPE
#   ifdef __cplusplus
#	define MODULE_SCOPE extern "C"
#   else
#	define MODULE_SCOPE extern
#   endif
#endif

/*
 * The following procedures allow namespaces to be customized to
 * support special name resolution rules for commands/variables.
 * 
 */

struct Tcl_ResolvedVarInfo;

typedef Tcl_Var (Tcl_ResolveRuntimeVarProc) _ANSI_ARGS_((
    Tcl_Interp* interp, struct Tcl_ResolvedVarInfo *vinfoPtr));

typedef void (Tcl_ResolveVarDeleteProc) _ANSI_ARGS_((
    struct Tcl_ResolvedVarInfo *vinfoPtr));

/*
 * The following structure encapsulates the routines needed to resolve a
 * variable reference at runtime.  Any variable specific state will typically
 * be appended to this structure.
 */


typedef struct Tcl_ResolvedVarInfo {
    Tcl_ResolveRuntimeVarProc *fetchProc;
    Tcl_ResolveVarDeleteProc *deleteProc;
} Tcl_ResolvedVarInfo;



typedef int (Tcl_ResolveCompiledVarProc) _ANSI_ARGS_((
    Tcl_Interp* interp, CONST84 char* name, int length,
    Tcl_Namespace *context, Tcl_ResolvedVarInfo **rPtr));

typedef int (Tcl_ResolveVarProc) _ANSI_ARGS_((
    Tcl_Interp* interp, CONST84 char* name, Tcl_Namespace *context,
    int flags, Tcl_Var *rPtr));

typedef int (Tcl_ResolveCmdProc) _ANSI_ARGS_((Tcl_Interp* interp,
    CONST84 char* name, Tcl_Namespace *context, int flags,
    Tcl_Command *rPtr));
 
typedef struct Tcl_ResolverInfo {
    Tcl_ResolveCmdProc *cmdResProc;	/* Procedure handling command name
					 * resolution. */
    Tcl_ResolveVarProc *varResProc;	/* Procedure handling variable name
					 * resolution for variables that
					 * can only be handled at runtime. */
    Tcl_ResolveCompiledVarProc *compiledVarResProc;
					/* Procedure handling variable name
					 * resolution at compile time. */
} Tcl_ResolverInfo;

/*
 *----------------------------------------------------------------
 * Data structures related to namespaces.
 *----------------------------------------------------------------
 */

typedef struct Tcl_Ensemble Tcl_Ensemble;

/*
 * The hash tables that store namespace variables get an extra field for
 * nsPtr, so that we can recover the namespace from its hash table. This is
 * used to avoid having to store an nsPtr in every variable. Note that all
 * fields (with the exception of the last) must correspond exactly to the
 * fields of Tcl_HashTable in tcl.h.
 */

typedef struct TclNSVarHashTable {
    Tcl_HashEntry **buckets;		/* Pointer to bucket array.  Each
					 * element points to first entry in
					 * bucket's hash chain, or NULL. */
    Tcl_HashEntry *staticBuckets[TCL_SMALL_HASH_TABLE];
					/* Bucket array used for small tables
					 * (to avoid mallocs and frees). */
    int numBuckets;			/* Total number of buckets allocated
					 * at **bucketPtr. */
    int numEntries;			/* Total number of entries present
					 * in table. */
    int rebuildSize;			/* Enlarge table when numEntries gets
					 * to be this large. */
    int downShift;			/* Shift count used in hashing
					 * function.  Designed to use high-
					 * order bits of randomized keys. */
    int mask;				/* Mask value used in hashing
					 * function. */
    int keyType;			/* Type of keys used in this table. 
					 * It's either TCL_CUSTOM_KEYS,
					 * TCL_STRING_KEYS, TCL_ONE_WORD_KEYS,
					 * or an integer giving the number of
					 * ints that is the size of the key.
					 */
#if TCL_PRESERVE_BINARY_COMPATABILITY
    Tcl_HashEntry *(*findProc) _ANSI_ARGS_((Tcl_HashTable *tablePtr,
	    CONST char *key));
    Tcl_HashEntry *(*createProc) _ANSI_ARGS_((Tcl_HashTable *tablePtr,
	    CONST char *key, int *newPtr));
#endif
    Tcl_HashKeyType *typePtr;		/* Type of the keys used in the
					 * Tcl_HashTable. */
    struct Namespace *nsPtr;		/* Points to the namespace that uses 
				         * this table to store variables. */  
} TclNSVarHashTable;

/*
 * The structure below defines a namespace.
 * Note: the first five fields must match exactly the fields in a
 * Tcl_Namespace structure (see tcl.h). If you change one, be sure to
 * change the other.
 */

typedef struct Namespace {
    char *name;			/* The namespace's simple (unqualified)
				 * name. This contains no ::'s. The name of
				 * the global namespace is "" although "::"
				 * is an synonym. */
    char *fullName;		/* The namespace's fully qualified name.
				 * This starts with ::. */
    ClientData clientData;	/* An arbitrary value associated with this
				 * namespace. */
    Tcl_NamespaceDeleteProc *deleteProc;
				/* Procedure invoked when deleting the
				 * namespace to, e.g., free clientData. */
    struct Namespace *parentPtr;/* Points to the namespace that contains
				 * this one. NULL if this is the global
				 * namespace. */
    Tcl_HashTable childTable;	/* Contains any child namespaces. Indexed
				 * by strings; values have type
				 * (Namespace *). */
    long nsId;			/* Unique id for the namespace. */
    Tcl_Interp *interp;		/* The interpreter containing this
				 * namespace. */
    int flags;			/* OR-ed combination of the namespace
				 * status flags NS_DYING and NS_DEAD
				 * listed below. */
    int activationCount;	/* Number of "activations" or active call
				 * frames for this namespace that are on
				 * the Tcl call stack. The namespace won't
				 * be freed until activationCount becomes
				 * zero. */
    int refCount;		/* Count of references by namespaceName *
				 * objects. The namespace can't be freed
				 * until refCount becomes zero. */
    Tcl_HashTable cmdTable;	/* Contains all the commands currently
				 * registered in the namespace. Indexed by
				 * strings; values have type (Command *).
				 * Commands imported by Tcl_Import have
				 * Command structures that point (via an
				 * ImportedCmdRef structure) to the
				 * Command structure in the source
				 * namespace's command table. */
    TclNSVarHashTable varTable;	/* Contains all the (global) variables
				 * currently in this namespace. Indexed
				 * by strings; values have type (Var *). */
    char **exportArrayPtr;	/* Points to an array of string patterns
				 * specifying which commands are exported.
				 * A pattern may include "string match"
				 * style wildcard characters to specify
				 * multiple commands; however, no namespace
				 * qualifiers are allowed. NULL if no
				 * export patterns are registered. */
    int numExportPatterns;	/* Number of export patterns currently
				 * registered using "namespace export". */
    int maxExportPatterns;	/* Mumber of export patterns for which
				 * space is currently allocated. */
    int cmdRefEpoch;		/* Incremented if a newly added command
				 * shadows a command for which this
				 * namespace has already cached a Command *
				 * pointer; this causes all its cached
				 * Command* pointers to be invalidated. */
    int resolverEpoch;		/* Incremented whenever (a) the name resolution
				 * rules change for this namespace or (b) a 
				 * newly added command shadows a command that
				 * is compiled to bytecodes.
				 * This invalidates all byte codes compiled
				 * in the namespace, causing the code to be
				 * recompiled under the new rules.*/
    Tcl_ResolveCmdProc *cmdResProc;
				/* If non-null, this procedure overrides
				 * the usual command resolution mechanism
				 * in Tcl.  This procedure is invoked
				 * within Tcl_FindCommand to resolve all
				 * command references within the namespace. */
    Tcl_ResolveVarProc *varResProc;
				/* If non-null, this procedure overrides
				 * the usual variable resolution mechanism
				 * in Tcl.  This procedure is invoked
				 * within Tcl_FindNamespaceVar to resolve all
				 * variable references within the namespace
				 * at runtime. */
    Tcl_ResolveCompiledVarProc *compiledVarResProc;
				/* If non-null, this procedure overrides
				 * the usual variable resolution mechanism
				 * in Tcl.  This procedure is invoked
				 * within LookupCompiledLocal to resolve
				 * variable references within the namespace
				 * at compile time. */
    int exportLookupEpoch;	/* Incremented whenever a command is added to
				 * a namespace, removed from a namespace or
				 * the exports of a namespace are changed.
				 * Allows TIP#112-driven command lists to be
				 * validated efficiently. */
    Tcl_Ensemble *ensembles;	/* List of structures that contain the details
				 * of the ensembles that are implemented on
				 * top of this namespace. */
} Namespace;

/*
 * Flags used to represent the status of a namespace:
 *
 * NS_DYING -	1 means Tcl_DeleteNamespace has been called to delete the
 *		namespace but there are still active call frames on the Tcl
 *		stack that refer to the namespace. When the last call frame
 *		referring to it has been popped, it's variables and command
 *		will be destroyed and it will be marked "dead" (NS_DEAD).
 *		The namespace can no longer be looked up by name.
 * NS_DEAD -	1 means Tcl_DeleteNamespace has been called to delete the
 *		namespace and no call frames still refer to it. Its
 *		variables and command have already been destroyed. This bit
 *		allows the namespace resolution code to recognize that the
 *		namespace is "deleted". When the last namespaceName object
 *		in any byte code unit that refers to the namespace has
 *		been freed (i.e., when the namespace's refCount is 0), the
 *		namespace's storage will be freed.
 */

#define NS_DYING	0x01
#define NS_DEAD		0x02

/*
 * Flags passed to TclGetNamespaceForQualName:
 *
 * TCL_GLOBAL_ONLY		- (see tcl.h) Look only in the global ns. 
 * TCL_NAMESPACE_ONLY		- (see tcl.h) Look only in the context ns.
 * TCL_CREATE_NS_IF_UNKNOWN	- Create unknown namespaces.
 * TCL_FIND_ONLY_NS		- The name sought is a namespace name.
 */

#define TCL_CREATE_NS_IF_UNKNOWN	0x800
#define TCL_FIND_ONLY_NS		0x1000

/*
 *----------------------------------------------------------------
 * Data structures related to variables.   These are used primarily
 * in tclVar.c
 *----------------------------------------------------------------
 */

/*
 * The following structure defines a variable trace, which is used to
 * invoke a specific C procedure whenever certain operations are performed
 * on a variable.
 */

typedef struct VarTrace {
    Tcl_VarTraceProc *traceProc;/* Procedure to call when operations given
				 * by flags are performed on variable. */
    ClientData clientData;	/* Argument to pass to proc. */
    int flags;			/* What events the trace procedure is
				 * interested in:  OR-ed combination of
				 * TCL_TRACE_READS, TCL_TRACE_WRITES,
				 * TCL_TRACE_UNSETS and TCL_TRACE_ARRAY. */
    struct VarTrace *nextPtr;	/* Next in list of traces associated with
				 * a particular variable. */
} VarTrace;

/*
 * The following structure defines a command trace, which is used to
 * invoke a specific C procedure whenever certain operations are performed
 * on a command.
 */

typedef struct CommandTrace {
    Tcl_CommandTraceProc *traceProc;/* Procedure to call when operations given
				     * by flags are performed on command. */
    ClientData clientData;	    /* Argument to pass to proc. */
    int flags;			    /* What events the trace procedure is
				     * interested in:  OR-ed combination of
				     * TCL_TRACE_RENAME, TCL_TRACE_DELETE. */
    struct CommandTrace *nextPtr;   /* Next in list of traces associated with
				     * a particular command. */
    int refCount;		    /* Used to ensure this structure is
				     * not deleted too early.  Keeps track
				     * of how many pieces of code have
				     * a pointer to this structure. */
} CommandTrace;

/*
 * When a command trace is active (i.e. its associated procedure is
 * executing), one of the following structures is linked into a list
 * associated with the command's interpreter.  The information in
 * the structure is needed in order for Tcl to behave reasonably
 * if traces are deleted while traces are active.
 */

typedef struct ActiveCommandTrace {
    struct Command *cmdPtr;	/* Command that's being traced. */
    struct ActiveCommandTrace *nextPtr;
				/* Next in list of all active command
				 * traces for the interpreter, or NULL
				 * if no more. */
    CommandTrace *nextTracePtr;	/* Next trace to check after current
				 * trace procedure returns;  if this
				 * trace gets deleted, must update pointer
				 * to avoid using free'd memory. */
} ActiveCommandTrace;

/*
 * When a variable trace is active (i.e. its associated procedure is
 * executing), one of the following structures is linked into a list
 * associated with the variable's interpreter.	The information in
 * the structure is needed in order for Tcl to behave reasonably
 * if traces are deleted while traces are active.
 */

typedef struct ActiveVarTrace {
    struct Var *varPtr;		/* Variable that's being traced. */
    struct ActiveVarTrace *nextPtr;
				/* Next in list of all active variable
				 * traces for the interpreter, or NULL
				 * if no more. */
    VarTrace *nextTracePtr;	/* Next trace to check after current
				 * trace procedure returns;  if this
				 * trace gets deleted, must update pointer
				 * to avoid using free'd memory. */
} ActiveVarTrace;

/*
 * The following structure describes an enumerative search in progress on
 * an array variable;  this are invoked with options to the "array"
 * command.
 */

typedef struct ArraySearch {
    int id;			/* Integer id used to distinguish among
				 * multiple concurrent searches for the
				 * same array. */
    struct Var *varPtr;		/* Pointer to array variable that's being
				 * searched. */
    Tcl_HashSearch search;	/* Info kept by the hash module about
				 * progress through the array. */
    Tcl_HashEntry *nextEntry;	/* Non-null means this is the next element
				 * to be enumerated (it's leftover from
				 * the Tcl_FirstHashEntry call or from
				 * an "array anymore" command).	 NULL
				 * means must call Tcl_NextHashEntry
				 * to get value to return. */
    struct ArraySearch *nextPtr;/* Next in list of all active searches
				 * for this variable, or NULL if this is
				 * the last one. */
} ArraySearch;

/*
 * The structure below defines a variable, which associates a string name
 * with a Tcl_Obj value. These structures are kept in procedure call frames
 * (for local variables recognized by the compiler) or in the heap (for
 * global variables and any variable not known to the compiler). For each
 * Var structure in the heap, a hash table entry holds the variable name and
 * a pointer to the Var structure.
 *
 * NOTE: the quantity and type of the elements have been designed to
 * allow the use of a Tcl_Obj to store a variable - so that we can profit from
 * the optimised allocator for Tcl_Objs. Any change to this will require
 * changing the memory management in tclVar.c - especially in NewVar and
 * CleanupVar. 
 */

typedef struct Var {
    int flags;			/* Miscellaneous bits of information about
				 * variable. See below for definitions. */
    union {
	Tcl_Obj *objPtr;	/* The variable's object value. Used for 
				 * scalar variables and array elements. */
	Tcl_HashTable *tablePtr;/* For array variables, this points to
				 * information about the hash table used
				 * to implement the associative array. 
				 * Points to malloc-ed data. */
	struct Var *linkPtr;	/* If this is a global variable being
				 * referred to in a procedure, or a variable
				 * created by "upvar", this field points to
				 * the referenced variable's Var struct. */
    } value;
    union {
	char *name;		/* Used for compiled locals, points to the
				 * variable's name. It is used, e.g., by
				 * TclLookupVar and "info locals". The storage
				 * for the characters of the name is not owned
				 * by the Var and must not be freed when
				 * freeing the Var. */
	Tcl_HashEntry *hPtr;	/* If variable is in a hashtable, either the
				 * hash table entry that refers to this
				 * variable or NULL if the variable has been
				 * detached from its hash table (e.g. an
				 * array is deleted, but some of its
				 * elements are still referred to in
				 * upvars). NULL if the variable is not in a
				 * hashtable. This is used to delete an
				 * variable from its hashtable if it is no
				 * longer needed. */
    } id;
    int refCount;		/* Counts number of active uses of this
				 * variable, not including its entry in the
				 * call frame or the hash table: 1 for each
				 * additional variable whose linkPtr points
				 * here, 1 for each nested trace active on
				 * variable, and 1 if the variable is a 
				 * namespace variable. This record can't be
				 * deleted until refCount becomes 0. */
    VarTrace *tracePtr;		/* First in list of all traces set for this
				 * variable. */
    ArraySearch *searchPtr;	/* First in list of all searches active
				 * for this variable, or NULL if none. */
} Var;

/*
 * Flag bits for variables. The first two (VAR_ARRAY and VAR_LINK) are
 * mutually exclusive and give the "type" of the variable; a scalar is
 * neither an array nor a link.
 * A variable with a NULL value is undefined: this means that the variable is
 * in the process of being deleted. An undefined variable logically does not
 * exist and survives only while it has a trace, or if it is a global variable
 * currently being used by some procedure.
 *
 * VAR_LINK -			1 means this Var structure contains a
 *				pointer to another Var structure that
 *				either has the real value or is itself
 *				another VAR_LINK pointer. Variables like
 *				this come about through "upvar" and "global"
 *				commands, or through references to variables
 *				in enclosing namespaces.
 * VAR_IN_HASHTABLE -		1 means this variable is in a hashtable and
 *				the Var structure is malloced. 0 if it is
 *				a local variable that was assigned a slot
 *				in a procedure frame by	the compiler so the
 *				Var storage is part of the call frame.
 * VAR_TRACE_ACTIVE -		1 means that trace processing is currently
 *				underway for a read or write access, so
 *				new read or write accesses should not cause
 *				trace procedures to be called and the
 *				variable can't be deleted.
 * VAR_ARRAY_ELEMENT -		1 means that this variable is an array
 *				element, so it is not legal for it to be
 *				an array itself (the VAR_ARRAY flag had
 *				better not be set).
 * VAR_NAMESPACE_VAR -		1 means that this variable was declared
 *				as a namespace variable. This flag ensures
 *				it persists until its namespace is
 *				destroyed or until the variable is unset;
 *				it will persist even if it has not been
 *				initialized and is marked undefined.
 *				The variable's refCount is incremented to
 *				reflect the "reference" from its namespace.
 *
 * The following additional flags are used with the CompiledLocal type
 * defined below:
 *
 * VAR_ARGUMENT -		1 means that this variable holds a procedure
 *				argument. 
 * VAR_TEMPORARY -		1 if the local variable is an anonymous
 *				temporary variable. Temporaries have a NULL
 *				name.
 * VAR_RESOLVED -		1 if name resolution has been done for this
 *				variable.
 *
 * The following additional flags are used to speed up variable access by
 * the bytecode engine. The information contained is already present in the
 * other flag values and/or the fields of the Var structure. There is special
 * code to maintain these flag values in synch with the rest.
 *
 * VAR_DIRECT_READABLE          1 means that TEBC can read this variable
 *                              directly:
 *                                - VAR_SCALAR is set
 *                                - VAR_UNDEFINED is not set
 *                                - tracePtr is NULL.
 * VAR_DIRECT_WRITABLE          1 means that TEBC can write this variable
 *                              directly:
 *                                - one of {VAR_SCALAR,VAR_UNDEFINED} is set 
 *                                - tracePtr is NULL
 *                                - VAR_IN_HASHTABLE is not set, or else hPtr
 *                                  is not NULL.
 */

#define VAR_ARRAY		0x2
#define VAR_LINK		0x4

#define VAR_IN_HASHTABLE	0x10
#define VAR_TRACE_ACTIVE	0x20
#define VAR_ARRAY_ELEMENT	0x40
#define VAR_NAMESPACE_VAR	0x80

#define VAR_ARGUMENT		0x100
#define VAR_TEMPORARY		0x200
#define VAR_RESOLVED		0x400	
#define VAR_IS_ARGS             0x800

#define VAR_DIRECT_READABLE     0x1000
#define VAR_DIRECT_WRITABLE     0x2000

/*
 * Macros to ensure that various flag bits are set properly for variables.
 * The ANSI C "prototypes" for these macros are:
 *
 * MODULE_SCOPE void	TclSetVarScalar _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE void	TclSetVarArray _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE void	TclSetVarLink _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE void	TclSetVarArrayElement _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE void	TclSetVarUndefined _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE void	TclClearVarUndefined _ANSI_ARGS_((Var *varPtr));
 */

#define TclSetVarDirectScalar(varPtr) \
    (varPtr)->flags = ((varPtr->flags) & ~(VAR_ARRAY|VAR_LINK)) \
        | (VAR_DIRECT_WRITABLE|VAR_DIRECT_READABLE)

#define TclSetVarScalar(varPtr) \
    (varPtr)->flags &= ~(VAR_ARRAY|VAR_LINK) 

#define TclSetVarArray(varPtr) \
    (varPtr)->flags = ((varPtr)->flags | VAR_ARRAY) \
        & ~(VAR_LINK|VAR_DIRECT_WRITABLE|VAR_DIRECT_READABLE)

#define TclSetVarLink(varPtr) \
    (varPtr)->flags = ((varPtr)->flags | VAR_LINK)\
        & ~(VAR_ARRAY|VAR_DIRECT_WRITABLE|VAR_DIRECT_READABLE)

#define TclSetVarArrayElement(varPtr) \
    (varPtr)->flags = ((varPtr)->flags & ~VAR_ARRAY) | VAR_ARRAY_ELEMENT

#define TclSetVarUndefined(varPtr) \
    (varPtr)->value.objPtr = NULL; \
    (varPtr)->flags &= ~(VAR_ARRAY|VAR_LINK|VAR_DIRECT_READABLE) 

#define TclSetVarTraceActive(varPtr) \
    (varPtr)->flags |= VAR_TRACE_ACTIVE

#define TclClearVarTraceActive(varPtr) \
    (varPtr)->flags &= ~VAR_TRACE_ACTIVE

#define TclSetVarNamespaceVar(varPtr) \
    (varPtr)->flags |= VAR_NAMESPACE_VAR

#define TclClearVarNamespaceVar(varPtr) \
    (varPtr)->flags &= ~VAR_NAMESPACE_VAR

/*
 * Macros to read various flag bits of variables.
 * The ANSI C "prototypes" for these macros are:
 *
 * MODULE_SCOPE int	TclIsVarScalar _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE int	TclIsVarLink _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE int	TclIsVarArray _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE int	TclIsVarUndefined _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE int	TclIsVarArrayElement _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE int	TclIsVarTemporary _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE int	TclIsVarArgument _ANSI_ARGS_((Var *varPtr));
 * MODULE_SCOPE int	TclIsVarResolved _ANSI_ARGS_((Var *varPtr));
 */
    
#define TclIsVarScalar(varPtr) \
    !((varPtr)->flags & (VAR_ARRAY|VAR_LINK))

#define TclIsVarLink(varPtr) \
    ((varPtr)->flags & VAR_LINK)

#define TclIsVarArray(varPtr) \
    ((varPtr)->flags & VAR_ARRAY)

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

#define TclIsVarUntraced(varPtr) \
    ((varPtr)->tracePtr == NULL)

/*
 * Macros for direct variable access by TEBC
 */

#define TclIsVarDirectReadable(varPtr) \
    ((varPtr)->flags & VAR_DIRECT_READABLE)

#define TclIsVarDirectWritable(varPtr) \
    ((varPtr)->flags & VAR_DIRECT_WRITABLE)

/*
 *----------------------------------------------------------------
 * Data structures related to procedures.  These are used primarily
 * in tclProc.c, tclCompile.c, and tclExecute.c.
 *----------------------------------------------------------------
 */

/*
 * Forward declaration to prevent an error when the forward reference to
 * Command is encountered in the Proc and ImportRef types declared below.
 */

struct Command;

/*
 * The variable-length structure below describes a local variable of a
 * procedure that was recognized by the compiler. These variables have a
 * name, an element in the array of compiler-assigned local variables in the
 * procedure's call frame, and various other items of information. If the
 * local variable is a formal argument, it may also have a default value.
 * The compiler can't recognize local variables whose names are
 * expressions (these names are only known at runtime when the expressions
 * are evaluated) or local variables that are created as a result of an
 * "upvar" or "uplevel" command. These other local variables are kept
 * separately in a hash table in the call frame.
 */

typedef struct CompiledLocal {
    struct CompiledLocal *nextPtr;
				/* Next compiler-recognized local variable
				 * for this procedure, or NULL if this is
				 * the last local. */
    int nameLength;		/* The number of characters in local
				 * variable's name. Used to speed up
				 * variable lookups. */
    int frameIndex;		/* Index in the array of compiler-assigned
				 * variables in the procedure call frame. */
    int flags;			/* Flag bits for the local variable. Same as
				 * the flags for the Var structure above,
				 * although only VAR_SCALAR, VAR_ARRAY, 
				 * VAR_LINK, VAR_ARGUMENT, VAR_TEMPORARY, and
				 * VAR_RESOLVED make sense. */
    Tcl_Obj *defValuePtr;	/* Pointer to the default value of an
				 * argument, if any. NULL if not an argument
				 * or, if an argument, no default value. */
    Tcl_ResolvedVarInfo *resolveInfo;
				/* Customized variable resolution info
				 * supplied by the Tcl_ResolveCompiledVarProc
				 * associated with a namespace. Each variable
				 * is marked by a unique ClientData tag
				 * during compilation, and that same tag
				 * is used to find the variable at runtime. */
    char name[4];		/* Name of the local variable starts here.
				 * If the name is NULL, this will just be
				 * '\0'. The actual size of this field will
				 * be large enough to hold the name. MUST
				 * BE THE LAST FIELD IN THE STRUCTURE! */
} CompiledLocal;

/*
 * The structure below defines a command procedure, which consists of a
 * collection of Tcl commands plus information about arguments and other
 * local variables recognized at compile time.
 */

typedef struct Proc {
    struct Interp *iPtr;	  /* Interpreter for which this command
				   * is defined. */
    int refCount;		  /* Reference count: 1 if still present
				   * in command table plus 1 for each call
				   * to the procedure that is currently
				   * active. This structure can be freed
				   * when refCount becomes zero. */
    struct Command *cmdPtr;	  /* Points to the Command structure for
				   * this procedure. This is used to get
				   * the namespace in which to execute
				   * the procedure. */
    Tcl_Obj *bodyPtr;		  /* Points to the ByteCode object for
				   * procedure's body command. */
    int numArgs;		  /* Number of formal parameters. */
    int numCompiledLocals;	  /* Count of local variables recognized by
				   * the compiler including arguments and
				   * temporaries. */
    CompiledLocal *firstLocalPtr; /* Pointer to first of the procedure's
				   * compiler-allocated local variables, or
				   * NULL if none. The first numArgs entries
				   * in this list describe the procedure's
				   * formal arguments. */
    CompiledLocal *lastLocalPtr;  /* Pointer to the last allocated local
				   * variable or NULL if none. This has
				   * frame index (numCompiledLocals-1). */
} Proc;

/*
 * The structure below defines a command trace.	 This is used to allow Tcl
 * clients to find out whenever a command is about to be executed.
 */

typedef struct Trace {
    int level;			/* Only trace commands at nesting level
				 * less than or equal to this. */
    Tcl_CmdObjTraceProc *proc;	/* Procedure to call to trace command. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
    struct Trace *nextPtr;	/* Next in list of traces for this interp. */
    int flags;			/* Flags governing the trace - see
				 * Tcl_CreateObjTrace for details */
    Tcl_CmdObjTraceDeleteProc* delProc;
				/* Procedure to call when trace is deleted */
} Trace;

/*
 * When an interpreter trace is active (i.e. its associated procedure
 * is executing), one of the following structures is linked into a list
 * associated with the interpreter.  The information in the structure
 * is needed in order for Tcl to behave reasonably if traces are
 * deleted while traces are active.
 */

typedef struct ActiveInterpTrace {
    struct ActiveInterpTrace *nextPtr;
				/* Next in list of all active command
				 * traces for the interpreter, or NULL
				 * if no more. */
    Trace *nextTracePtr;	/* Next trace to check after current
				 * trace procedure returns;  if this
				 * trace gets deleted, must update pointer
				 * to avoid using free'd memory. */
} ActiveInterpTrace;

/*
 * The structure below defines an entry in the assocData hash table which
 * is associated with an interpreter. The entry contains a pointer to a
 * function to call when the interpreter is deleted, and a pointer to
 * a user-defined piece of data.
 */

typedef struct AssocData {
    Tcl_InterpDeleteProc *proc;	/* Proc to call when deleting. */
    ClientData clientData;	/* Value to pass to proc. */
} AssocData;	

/*
 * The structure below defines a call frame. A call frame defines a naming
 * context for a procedure call: its local naming scope (for local
 * variables) and its global naming scope (a namespace, perhaps the global
 * :: namespace). A call frame can also define the naming context for a
 * namespace eval or namespace inscope command: the namespace in which the
 * command's code should execute. The Tcl_CallFrame structures exist only
 * while procedures or namespace eval/inscope's are being executed, and
 * provide a kind of Tcl call stack.
 * 
 * WARNING!! The structure definition must be kept consistent with the
 * Tcl_CallFrame structure in tcl.h. If you change one, change the other.
 */

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
    int objc;			/* This and objv below describe the
				 * arguments for this procedure call. */
    Tcl_Obj *CONST *objv;	/* Array of argument objects. */
    struct CallFrame *callerPtr;
				/* Value of interp->framePtr when this
				 * procedure was invoked (i.e. next higher
				 * in stack of all active procedures). */
    struct CallFrame *callerVarPtr;
				/* Value of interp->varFramePtr when this
				 * procedure was invoked (i.e. determines
				 * variable scoping within caller). Same
				 * as callerPtr unless an "uplevel" command
				 * or something equivalent was active in
				 * the caller). */
    int level;			/* Level of this procedure, for "uplevel"
				 * purposes (i.e. corresponds to nesting of
				 * callerVarPtr's, not callerPtr's). 1 for
				 * outermost procedure, 0 for top-level. */
    Proc *procPtr;		/* Points to the structure defining the
				 * called procedure. Used to get information
				 * such as the number of compiled local
				 * variables (local variables assigned
				 * entries ["slots"] in the compiledLocals
				 * array below). */
    Tcl_HashTable *varTablePtr;	/* Hash table containing local variables not
				 * recognized by the compiler, or created at
				 * execution time through, e.g., upvar.
				 * Initially NULL and created if needed. */
    int numCompiledLocals;	/* Count of local variables recognized by
				 * the compiler including arguments. */
    Var* compiledLocals;	/* Points to the array of local variables
				 * recognized by the compiler. The compiler
				 * emits code that refers to these variables
				 * using an index into this array. */
} CallFrame;

#define FRAME_IS_PROC 0x1

/*
 *----------------------------------------------------------------
 * Data structures and procedures related to TclHandles, which
 * are a very lightweight method of preserving enough information
 * to determine if an arbitrary malloc'd block has been deleted.
 *----------------------------------------------------------------
 */

typedef VOID **TclHandle;

/*
 *----------------------------------------------------------------
 * Data structures related to expressions.  These are used only in
 * tclExpr.c.
 *----------------------------------------------------------------
 */

/*
 * The data structure below defines a math function (e.g. sin or hypot)
 * for use in Tcl expressions.
 */

#define MAX_MATH_ARGS 5
typedef struct MathFunc {
    int builtinFuncIndex;	/* If this is a builtin math function, its
				 * index in the array of builtin functions.
				 * (tclCompilation.h lists these indices.)
				 * The value is -1 if this is a new function
				 * defined by Tcl_CreateMathFunc. The value
				 * is also -1 if a builtin function is
				 * replaced by a Tcl_CreateMathFunc call. */
    int numArgs;		/* Number of arguments for function. */
    Tcl_ValueType argTypes[MAX_MATH_ARGS];
				/* Acceptable types for each argument. */
    Tcl_MathProc *proc;		/* Procedure that implements this function.
				 * NULL if isBuiltinFunc is 1. */
    ClientData clientData;	/* Additional argument to pass to the
				 * function when invoking it. NULL if
				 * isBuiltinFunc is 1. */
} MathFunc;

/*
 * These are a thin layer over TclpThreadKeyDataGet and TclpThreadKeyDataSet
 * when threads are used, or an emulation if there are no threads.  These
 * are really internal and Tcl clients should use Tcl_GetThreadData.
 */

MODULE_SCOPE VOID *	TclThreadDataKeyGet _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr));
MODULE_SCOPE void	TclThreadDataKeySet _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr, VOID *data));

/*
 * This is a convenience macro used to initialize a thread local storage ptr.
 */
#define TCL_TSD_INIT(keyPtr)	(ThreadSpecificData *)Tcl_GetThreadData((keyPtr), sizeof(ThreadSpecificData))


/*
 *----------------------------------------------------------------
 * Data structures related to bytecode compilation and execution.
 * These are used primarily in tclCompile.c, tclExecute.c, and
 * tclBasic.c.
 *----------------------------------------------------------------
 */

/*
 * Forward declaration to prevent errors when the forward references to
 * Tcl_Parse and CompileEnv are encountered in the procedure type
 * CompileProc declared below.
 */

struct CompileEnv;

/*
 * The type of procedures called by the Tcl bytecode compiler to compile
 * commands. Pointers to these procedures are kept in the Command structure
 * describing each command.  The integer value returned by a CompileProc
 * must be one of the following:
 *
 * TCL_OK		Compilation completed normally.
 * TCL_OUT_LINE_COMPILE	Compilation could not be completed.  This can
 * 			be just a judgment by the CompileProc that the
 * 			command is too complex to compile effectively,
 * 			or it can indicate that in the current state of
 * 			the interp, the command would raise an error.
 * 			In the latter circumstance, we defer error reporting
 * 			until the actual runtime, because by then changes
 * 			in the interp state may allow the command to be
 * 			successfully evaluated.
 */

#define TCL_OUT_LINE_COMPILE	(TCL_CONTINUE + 1)

typedef int (CompileProc) _ANSI_ARGS_((Tcl_Interp *interp,
	Tcl_Parse *parsePtr, struct CompileEnv *compEnvPtr));

/*
 * The type of procedure called from the compilation hook point in
 * SetByteCodeFromAny.
 */

typedef int (CompileHookProc) _ANSI_ARGS_((Tcl_Interp *interp,
	struct CompileEnv *compEnvPtr, ClientData clientData));

/*
 * The data structure defining the execution environment for ByteCode's.
 * There is one ExecEnv structure per Tcl interpreter. It holds the
 * evaluation stack that holds command operands and results. The stack grows
 * towards increasing addresses. The "stackTop" member is cached by
 * TclExecuteByteCode in a local variable: it must be set before calling
 * TclExecuteByteCode and will be restored by TclExecuteByteCode before it
 * returns.
 */

typedef struct ExecEnv {
    Tcl_Obj **stackPtr;		/* Points to the first item in the
				 * evaluation stack on the heap. */
    Tcl_Obj **tosPtr;		/* Points to current top of stack; 
				 * (stackPtr-1) when the stack is empty. */
    Tcl_Obj **endPtr;		/* Points to last usable item in stack. */
    Tcl_Obj *constants[2];      /* Pointers to constant "0" and "1" objs. */
} ExecEnv;

/*
 * The definitions for the LiteralTable and LiteralEntry structures. Each
 * interpreter contains a LiteralTable. It is used to reduce the storage
 * needed for all the Tcl objects that hold the literals of scripts compiled
 * by the interpreter. A literal's object is shared by all the ByteCodes
 * that refer to the literal. Each distinct literal has one LiteralEntry
 * entry in the LiteralTable. A literal table is a specialized hash table
 * that is indexed by the literal's string representation, which may contain
 * null characters.
 *
 * Note that we reduce the space needed for literals by sharing literal
 * objects both within a ByteCode (each ByteCode contains a local
 * LiteralTable) and across all an interpreter's ByteCodes (with the
 * interpreter's global LiteralTable).
 */

typedef struct LiteralEntry {
    struct LiteralEntry *nextPtr;	/* Points to next entry in this
					 * hash bucket or NULL if end of
					 * chain. */
    Tcl_Obj *objPtr;			/* Points to Tcl object that
					 * holds the literal's bytes and
					 * length. */
    int refCount;			/* If in an interpreter's global
					 * literal table, the number of
					 * ByteCode structures that share
					 * the literal object; the literal
					 * entry can be freed when refCount
					 * drops to 0. If in a local literal
					 * table, -1. */
    Namespace *nsPtr;                    /* Namespace in which this literal is
					 * used. We try to avoid sharing
					 * literal non-FQ command names among
					 * different namespaces to reduce
					 * shimmering.*/ 
} LiteralEntry;

typedef struct LiteralTable {
    LiteralEntry **buckets;		/* Pointer to bucket array. Each
					 * element points to first entry in
					 * bucket's hash chain, or NULL. */
    LiteralEntry *staticBuckets[TCL_SMALL_HASH_TABLE];
					/* Bucket array used for small
					 * tables to avoid mallocs and
					 * frees. */
    int numBuckets;			/* Total number of buckets allocated
					 * at **buckets. */
    int numEntries;			/* Total number of entries present
					 * in table. */
    int rebuildSize;			/* Enlarge table when numEntries
					 * gets to be this large. */
    int mask;				/* Mask value used in hashing
					 * function. */
} LiteralTable;

/*
 * The following structure defines for each Tcl interpreter various
 * statistics-related information about the bytecode compiler and
 * interpreter's operation in that interpreter.
 */

#ifdef TCL_COMPILE_STATS
typedef struct ByteCodeStats {
    long numExecutions;		  /* Number of ByteCodes executed. */
    long numCompilations;	  /* Number of ByteCodes created. */
    long numByteCodesFreed;	  /* Number of ByteCodes destroyed. */
    long instructionCount[256];	  /* Number of times each instruction was
				   * executed. */

    double totalSrcBytes;	  /* Total source bytes ever compiled. */
    double totalByteCodeBytes;	  /* Total bytes for all ByteCodes. */
    double currentSrcBytes;	  /* Src bytes for all current ByteCodes. */
    double currentByteCodeBytes;  /* Code bytes in all current ByteCodes. */

    long srcCount[32];		  /* Source size distribution: # of srcs of
				   * size [2**(n-1)..2**n), n in [0..32). */
    long byteCodeCount[32];	  /* ByteCode size distribution. */
    long lifetimeCount[32];	  /* ByteCode lifetime distribution (ms). */
    
    double currentInstBytes;	  /* Instruction bytes-current ByteCodes. */
    double currentLitBytes;	  /* Current literal bytes. */
    double currentExceptBytes;	  /* Current exception table bytes. */
    double currentAuxBytes;	  /* Current auxiliary information bytes. */
    double currentCmdMapBytes;	  /* Current src<->code map bytes. */
    
    long numLiteralsCreated;	  /* Total literal objects ever compiled. */
    double totalLitStringBytes;	  /* Total string bytes in all literals. */
    double currentLitStringBytes; /* String bytes in current literals. */
    long literalCount[32];	  /* Distribution of literal string sizes. */
} ByteCodeStats;
#endif /* TCL_COMPILE_STATS */

/*
 *----------------------------------------------------------------
 * Data structures related to commands.
 *----------------------------------------------------------------
 */

/*
 * An imported command is created in an namespace when it imports a "real"
 * command from another namespace. An imported command has a Command
 * structure that points (via its ClientData value) to the "real" Command
 * structure in the source namespace's command table. The real command
 * records all the imported commands that refer to it in a list of ImportRef
 * structures so that they can be deleted when the real command is deleted.  */

typedef struct ImportRef {
    struct Command *importedCmdPtr;
				/* Points to the imported command created in
				 * an importing namespace; this command
				 * redirects its invocations to the "real"
				 * command. */
    struct ImportRef *nextPtr;	/* Next element on the linked list of
				 * imported commands that refer to the
				 * "real" command. The real command deletes
				 * these imported commands on this list when
				 * it is deleted. */
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
				 * only when deleting it in order to remove
				 * it from the real command's linked list of
				 * imported commands that refer to it. */
} ImportedCmdData;

/*
 * A Command structure exists for each command in a namespace. The
 * Tcl_Command opaque type actually refers to these structures.
 */

typedef struct Command {
    Tcl_HashEntry *hPtr;	/* Pointer to the hash table entry that
				 * refers to this command. The hash table is
				 * either a namespace's command table or an
				 * interpreter's hidden command table. This
				 * pointer is used to get a command's name
				 * from its Tcl_Command handle. NULL means
				 * that the hash table entry has been
				 * removed already (this can happen if
				 * deleteProc causes the command to be
				 * deleted or recreated). */
    Namespace *nsPtr;		/* Points to the namespace containing this
				 * command. */
    int refCount;		/* 1 if in command hashtable plus 1 for each
				 * reference from a CmdName Tcl object
				 * representing a command's name in a
				 * ByteCode instruction sequence. This
				 * structure can be freed when refCount
				 * becomes zero. */
    int cmdEpoch;		/* Incremented to invalidate any references
				 * that point to this command when it is
				 * renamed, deleted, hidden, or exposed. */
    CompileProc *compileProc;	/* Procedure called to compile command. NULL
				 * if no compile proc exists for command. */
    Tcl_ObjCmdProc *objProc;	/* Object-based command procedure. */
    ClientData objClientData;	/* Arbitrary value passed to object proc. */
    Tcl_CmdProc *proc;		/* String-based command procedure. */
    ClientData clientData;	/* Arbitrary value passed to string proc. */
    Tcl_CmdDeleteProc *deleteProc;
				/* Procedure invoked when deleting command
				 * to, e.g., free all client data. */
    ClientData deleteData;	/* Arbitrary value passed to deleteProc. */
    int flags;			/* Miscellaneous bits of information about
				 * command. See below for definitions. */
    ImportRef *importRefPtr;	/* List of each imported Command created in
				 * another namespace when this command is
				 * imported. These imported commands
				 * redirect invocations back to this
				 * command. The list is used to remove all
				 * those imported commands when deleting
				 * this "real" command. */
    CommandTrace *tracePtr;	/* First in list of all traces set for this
				 * command. */
} Command;

/*
 * Flag bits for commands. 
 *
 * CMD_IS_DELETED -		Means that the command is in the process
 *				of being deleted (its deleteProc is
 *				currently executing). Other attempts to
 *				delete the command should be ignored.
 * CMD_TRACE_ACTIVE -		1 means that trace processing is currently
 *				underway for a rename/delete change.
 *				See the two flags below for which is
 *				currently being processed.
 * CMD_HAS_EXEC_TRACES -	1 means that this command has at least
 *				one execution trace (as opposed to simple
 *				delete/rename traces) in its tracePtr list.
 * TCL_TRACE_RENAME -		A rename trace is in progress. Further
 *				recursive renames will not be traced.
 * TCL_TRACE_DELETE -		A delete trace is in progress. Further 
 *				recursive deletes will not be traced.
 * (these last two flags are defined in tcl.h)
 */
#define CMD_IS_DELETED		0x1
#define CMD_TRACE_ACTIVE	0x2
#define CMD_HAS_EXEC_TRACES	0x4

/*
 *----------------------------------------------------------------
 * Data structures related to name resolution procedures.
 *----------------------------------------------------------------
 */

/*
 * The interpreter keeps a linked list of name resolution schemes.
 * The scheme for a namespace is consulted first, followed by the
 * list of schemes in an interpreter, followed by the default
 * name resolution in Tcl.  Schemes are added/removed from the
 * interpreter's list by calling Tcl_AddInterpResolver and
 * Tcl_RemoveInterpResolver.
 */

typedef struct ResolverScheme {
    char *name;			/* Name identifying this scheme. */
    Tcl_ResolveCmdProc *cmdResProc;
				/* Procedure handling command name
				 * resolution. */
    Tcl_ResolveVarProc *varResProc;
				/* Procedure handling variable name
				 * resolution for variables that
				 * can only be handled at runtime. */
    Tcl_ResolveCompiledVarProc *compiledVarResProc;
				/* Procedure handling variable name
				 * resolution at compile time. */

    struct ResolverScheme *nextPtr;
				/* Pointer to next record in linked list. */
} ResolverScheme;

/*
 * Forward declaration of the TIP#143 limit handler structure.
 */

typedef struct LimitHandler LimitHandler;

/*
 *----------------------------------------------------------------
 * This structure defines an interpreter, which is a collection of
 * commands plus other state information related to interpreting
 * commands, such as variable storage. Primary responsibility for
 * this data structure is in tclBasic.c, but almost every Tcl
 * source file uses something in here.
 *----------------------------------------------------------------
 */

typedef struct Interp {

    /*
     * Note:  the first three fields must match exactly the fields in
     * a Tcl_Interp struct (see tcl.h).	 If you change one, be sure to
     * change the other.
     *
     * The interpreter's result is held in both the string and the
     * objResultPtr fields. These fields hold, respectively, the result's
     * string or object value. The interpreter's result is always in the
     * result field if that is non-empty, otherwise it is in objResultPtr.
     * The two fields are kept consistent unless some C code sets
     * interp->result directly. Programs should not access result and
     * objResultPtr directly; instead, they should always get and set the
     * result using procedures such as Tcl_SetObjResult, Tcl_GetObjResult,
     * and Tcl_GetStringResult. See the SetResult man page for details.
     */

    char *result;		/* If the last command returned a string
				 * result, this points to it. Should not be
				 * accessed directly; see comment above. */
    Tcl_FreeProc *freeProc;	/* Zero means a string result is statically
				 * allocated. TCL_DYNAMIC means string
				 * result was allocated with ckalloc and
				 * should be freed with ckfree. Other values
				 * give address of procedure to invoke to
				 * free the string result. Tcl_Eval must
				 * free it before executing next command. */
    int errorLine;		/* When TCL_ERROR is returned, this gives
				 * the line number in the command where the
				 * error occurred (1 means first line). */
    struct TclStubs *stubTable;
				/* Pointer to the exported Tcl stub table.
				 * On previous versions of Tcl this is a
				 * pointer to the objResultPtr or a pointer
				 * to a buckets array in a hash table. We
				 * therefore have to do some careful checking
				 * before we can use this. */

    TclHandle handle;		/* Handle used to keep track of when this
				 * interp is deleted. */

    Namespace *globalNsPtr;	/* The interpreter's global namespace. */
    Tcl_HashTable *hiddenCmdTablePtr;
				/* Hash table used by tclBasic.c to keep
				 * track of hidden commands on a per-interp
				 * basis. */
    ClientData interpInfo;	/* Information used by tclInterp.c to keep
				 * track of master/slave interps on
				 * a per-interp basis. */
    Tcl_HashTable mathFuncTable;/* Contains all the math functions currently
				 * defined for the interpreter.	 Indexed by
				 * strings (function names); values have
				 * type (MathFunc *). */



    /*
     * Information related to procedures and variables. See tclProc.c
     * and tclVar.c for usage.
     */

    int numLevels;		/* Keeps track of how many nested calls to
				 * Tcl_Eval are in progress for this
				 * interpreter.	 It's used to delay deletion
				 * of the table until all Tcl_Eval
				 * invocations are completed. */
    int maxNestingDepth;	/* If numLevels exceeds this value then Tcl
				 * assumes that infinite recursion has
				 * occurred and it generates an error. */
    CallFrame *framePtr;	/* Points to top-most in stack of all nested
				 * procedure invocations.  NULL means there
				 * are no active procedures. */
    CallFrame *varFramePtr;	/* Points to the call frame whose variables
				 * are currently in use (same as framePtr
				 * unless an "uplevel" command is
				 * executing). NULL means no procedure is
				 * active or "uplevel 0" is executing. */
    ActiveVarTrace *activeVarTracePtr;
				/* First in list of active traces for
				 * interp, or NULL if no active traces. */
    int returnCode;		/* [return -code] parameter */
    char *unused3;		/* No longer used (was errorInfo) */
    char *unused4;		/* No longer used (was errorCode) */

    /*
     * Information used by Tcl_AppendResult to keep track of partial
     * results.	 See Tcl_AppendResult code for details.
     */

    char *appendResult;		/* Storage space for results generated
				 * by Tcl_AppendResult.	 Malloc-ed.  NULL
				 * means not yet allocated. */
    int appendAvl;		/* Total amount of space available at
				 * partialResult. */
    int appendUsed;		/* Number of non-null bytes currently
				 * stored at partialResult. */

    /*
     * Information about packages.  Used only in tclPkg.c.
     */

    Tcl_HashTable packageTable;	/* Describes all of the packages loaded
				 * in or available to this interpreter.
				 * Keys are package names, values are
				 * (Package *) pointers. */
    char *packageUnknown;	/* Command to invoke during "package
				 * require" commands for packages that
				 * aren't described in packageTable. 
				 * Malloc'ed, may be NULL. */

    /*
     * Miscellaneous information:
     */

    int cmdCount;		/* Total number of times a command procedure
				 * has been called for this interpreter. */
    int evalFlags;		/* Flags to control next call to Tcl_Eval.
				 * Normally zero, but may be set before
				 * calling Tcl_Eval.  See below for valid
				 * values. */
    int unused1;		/* No longer used (was termOffset) */
    LiteralTable literalTable;	/* Contains LiteralEntry's describing all
				 * Tcl objects holding literals of scripts
				 * compiled by the interpreter. Indexed by
				 * the string representations of literals.
				 * Used to avoid creating duplicate
				 * objects. */
    int compileEpoch;		/* Holds the current "compilation epoch"
				 * for this interpreter. This is
				 * incremented to invalidate existing
				 * ByteCodes when, e.g., a command with a
				 * compile procedure is redefined. */
    Proc *compiledProcPtr;	/* If a procedure is being compiled, a
				 * pointer to its Proc structure; otherwise,
				 * this is NULL. Set by ObjInterpProc in
				 * tclProc.c and used by tclCompile.c to
				 * process local variables appropriately. */
    ResolverScheme *resolverPtr;
				/* Linked list of name resolution schemes
				 * added to this interpreter.  Schemes
				 * are added/removed by calling
				 * Tcl_AddInterpResolvers and
				 * Tcl_RemoveInterpResolver. */
    Tcl_Obj *scriptFile;	/* NULL means there is no nested source
				 * command active;  otherwise this points to
				 * pathPtr of the file being sourced. */
    int flags;			/* Various flag bits.  See below. */
    long randSeed;		/* Seed used for rand() function. */
    Trace *tracePtr;		/* List of traces for this interpreter. */
    Tcl_HashTable *assocData;	/* Hash table for associating data with
				 * this interpreter. Cleaned up when
				 * this interpreter is deleted. */
    struct ExecEnv *execEnvPtr;	/* Execution environment for Tcl bytecode
				 * execution. Contains a pointer to the
				 * Tcl evaluation stack. */
    Tcl_Obj *emptyObjPtr;	/* Points to an object holding an empty
				 * string. Returned by Tcl_ObjSetVar2 when
				 * variable traces change a variable in a
				 * gross way. */
    char resultSpace[TCL_RESULT_SIZE+1];
				/* Static space holding small results. */
    Tcl_Obj *objResultPtr;	/* If the last command returned an object
				 * result, this points to it. Should not be
				 * accessed directly; see comment above. */
    Tcl_ThreadId threadId;	/* ID of thread that owns the interpreter */

    ActiveCommandTrace *activeCmdTracePtr;
				/* First in list of active command traces for
				 * interp, or NULL if no active traces. */
    ActiveInterpTrace *activeInterpTracePtr;
				/* First in list of active traces for
				 * interp, or NULL if no active traces. */

    int tracesForbiddingInline; /* Count of traces (in the list headed by
				 * tracePtr) that forbid inline bytecode
				 * compilation */

    /* Fields used to manage extensible return options (TIP 90) */
    Tcl_Obj *returnOpts;	/* A dictionary holding the options to the
				 * last [return] command */

    Tcl_Obj *errorInfo;		/* errorInfo value (now as a Tcl_Obj) */
    Tcl_Obj *eiVar;		/* cached ref to ::errorInfo variable */
    Tcl_Obj *errorCode;		/* errorCode value (now as a Tcl_Obj) */
    Tcl_Obj *ecVar;		/* cached ref to ::errorInfo variable */
    int returnLevel;		/* [return -level] parameter */

    /*
     * Resource limiting framework support (TIP#143).
     */

    struct {
	int active;		/* Flag values defining which limits have
				 * been set. */
	int granularityTicker;	/* Counter used to determine how often to
				 * check the limits. */
	int exceeded;		/* Which limits have been exceeded, described
				 * as flag values the same as the 'active'
				 * field. */

	int cmdCount;		/* Limit for how many commands to execute
				 * in the interpreter. */
	LimitHandler *cmdHandlers; /* Handlers to execute when the limit
				    * is reached. */
	int cmdGranularity;	/* Mod factor used to determine how often
				 * to evaluate the limit check. */

	Tcl_Time time;		/* Time limit for execution within the
				 * interpreter. */
	LimitHandler *timeHandlers; /* Handlers to execute when the limit
				     * is reached. */
	int timeGranularity;	/* Mod factor used to determine how often
				 * to evaluate the limit check. */
	Tcl_TimerToken timeEvent; /* Handle for a timer callback that will
				   * occur when the time-limit is exceeded. */

	Tcl_HashTable callbacks; /* Mapping from (interp,type) pair to data
				  * used to install a limit handler callback
				  * to run in _this_ interp when the limit
				  * is exceeded. */
    } limit;

    /*
     * Information for improved default error generation from
     * ensembles (TIP#112).
     */

    struct {
	Tcl_Obj * CONST *sourceObjs;
				/* What arguments were actually input into
				 * the *root* ensemble command?  (Nested
				 * ensembles don't rewrite this.)  NULL if
				 * we're not processing an ensemble. */
	int numRemovedObjs;	/* How many arguments have been stripped off
				 * because of ensemble processing. */
	int numInsertedObjs;	/* How many of the current arguments were
				 * inserted by an ensemble. */
    } ensembleRewrite;

    /*
     * Statistical information about the bytecode compiler and interpreter's
     * operation.
     */

#ifdef TCL_COMPILE_STATS
    ByteCodeStats stats;	/* Holds compilation and execution
				 * statistics for this interpreter. */
#endif /* TCL_COMPILE_STATS */	  
} Interp;

/*
 * EvalFlag bits for Interp structures:
 *
 * TCL_ALLOW_EXCEPTIONS	1 means it's OK for the script to terminate with
 *			a code other than TCL_OK or TCL_ERROR;	0 means
 *			codes other than these should be turned into errors.
 */

#define TCL_ALLOW_EXCEPTIONS	  4

/*
 * Flag bits for Interp structures:
 *
 * DELETED:		Non-zero means the interpreter has been deleted:
 *			don't process any more commands for it, and destroy
 *			the structure as soon as all nested invocations of
 *			Tcl_Eval are done.
 * ERR_ALREADY_LOGGED:	Non-zero means information has already been logged
 *			in iPtr->errorInfo for the current Tcl_Eval instance,
 *			so Tcl_Eval needn't log it (used to implement the
 *			"error message log" command).
 * DONT_COMPILE_CMDS_INLINE: Non-zero means that the bytecode compiler
 *			should not compile any commands into an inline
 *			sequence of instructions. This is set 1, for
 *			example, when command traces are requested.
 * RAND_SEED_INITIALIZED: Non-zero means that the randSeed value of the
 *			interp has not be initialized.	This is set 1
 *			when we first use the rand() or srand() functions.
 * SAFE_INTERP:		Non zero means that the current interp is a
 *			safe interp (ie it has only the safe commands
 *			installed, less priviledge than a regular interp).
 * INTERP_TRACE_IN_PROGRESS: Non-zero means that an interp trace is currently
 *			active; so no further trace callbacks should be
 *			invoked.
 *
 * WARNING: For the sake of some extensions that have made use of former
 * internal values, do not re-use the flag values 2 (formerly ERR_IN_PROGRESS)
 * or 8 (formerly ERROR_CODE_SET).
 */

#define DELETED				    1
#define ERR_ALREADY_LOGGED		    4
#define DONT_COMPILE_CMDS_INLINE	 0x20
#define RAND_SEED_INITIALIZED		 0x40
#define SAFE_INTERP			 0x80
#define INTERP_TRACE_IN_PROGRESS	0x200

/*
 * Maximum number of levels of nesting permitted in Tcl commands (used
 * to catch infinite recursion).
 */

#define MAX_NESTING_DEPTH	1000

/*
 * TIP#143 limit handler internal representation.
 */

struct LimitHandler {
    int flags;			/* The state of this particular handler. */
    Tcl_LimitHandlerProc *handlerProc; /* The handler callback. */
    ClientData clientData;	/* Opaque argument to the handler callback. */
    Tcl_LimitHandlerDeleteProc *deleteProc; /* How to delete the clientData */
    LimitHandler *prevPtr;	/* Previous item in linked list of handlers */
    LimitHandler *nextPtr;	/* Next item in linked list of handlers */
};

/*
 * Values for the LimitHandler flags field.
 *	LIMIT_HANDLER_ACTIVE - Whether the handler is currently being
 *		processed; handlers are never to be entered reentrantly.
 *	LIMIT_HANDLER_DELETED - Whether the handler has been deleted.  This
 *		should not normally be observed because when a handler is
 *		deleted it is also spliced out of the list of handlers, but
 *		even so we will be careful.
 */

#define LIMIT_HANDLER_ACTIVE	0x01
#define LIMIT_HANDLER_DELETED	0x02

/*
 * The macro below is used to modify a "char" value (e.g. by casting
 * it to an unsigned character) so that it can be used safely with
 * macros such as isspace.
 */

#define UCHAR(c) ((unsigned char) (c))

/*
 * This macro is used to determine the offset needed to safely allocate any
 * data structure in memory. Given a starting offset or size, it "rounds up"
 * or "aligns" the offset to the next 8-byte boundary so that any data
 * structure can be placed at the resulting offset without fear of an
 * alignment error.
 *
 * WARNING!! DO NOT USE THIS MACRO TO ALIGN POINTERS: it will produce
 * the wrong result on platforms that allocate addresses that are divisible
 * by 4 or 2. Only use it for offsets or sizes.
 *
 * This macro is only used by tclCompile.c in the core (Bug 926445). It
 * however not be made file static, as extensions that touch bytecodes
 * (notably tbcload) require it.
 */

#define TCL_ALIGN(x) (((int)(x) + 7) & ~7)


/*
 * The following enum values are used to specify the runtime platform
 * setting of the tclPlatform variable.
 */

typedef enum {
    TCL_PLATFORM_UNIX = 0,	/* Any Unix-like OS. */
    TCL_PLATFORM_WINDOWS = 2	/* Any Microsoft Windows OS. */
} TclPlatformType;

/*
 *  The following enum values are used to indicate the translation
 *  of a Tcl channel.  Declared here so that each platform can define
 *  TCL_PLATFORM_TRANSLATION to the native translation on that platform
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
 * TCL_INVOKE_HIDDEN		Invoke a hidden command; if not set,
 *				invokes an exposed command.
 * TCL_INVOKE_NO_UNKNOWN	If set, "unknown" is not invoked if
 *				the command to be invoked is not found.
 *				Only has an effect if invoking an exposed
 *				command, i.e. if TCL_INVOKE_HIDDEN is not
 *				also set.
 * TCL_INVOKE_NO_TRACEBACK	Does not record traceback information if
 *				the invoked command returns an error.  Used
 *				if the caller plans on recording its own
 *				traceback information.
 */

#define	TCL_INVOKE_HIDDEN	(1<<0)
#define TCL_INVOKE_NO_UNKNOWN	(1<<1)
#define TCL_INVOKE_NO_TRACEBACK	(1<<2)

/*
 * The structure used as the internal representation of Tcl list
 * objects. This struct is grown (reallocated and copied) as necessary to hold
 * all the list's element pointers. The struct might contain more slots than
 * currently used to hold all element pointers. This is done to make append
 * operations faster.
 */

typedef struct List {
    int refCount;
    int maxElemCount;		/* Total number of element array slots. */
    int elemCount;		/* Current number of list elements. */
    Tcl_Obj *elements;		/* First list element; the struct is grown to
				 * accomodate all elements. */
} List;

/*
 * Macro used to get the elements of a list object - do NOT forget to verify
 * that it is of list type before using!
 */

#define TclListObjGetElements(listPtr, objc, objv) \
    { \
	List *listRepPtr = \
	    (List *) (listPtr)->internalRep.twoPtrValue.ptr1;\
	(objc) = listRepPtr->elemCount;\
	(objv) = &listRepPtr->elements;\
    }

/*
 *----------------------------------------------------------------
 * Data structures related to the filesystem internals
 *----------------------------------------------------------------
 */


/* 
 * The version_2 filesystem is private to Tcl.  As and when these
 * changes have been thoroughly tested and investigated a new public
 * filesystem interface will be released.  The aim is more versatile
 * virtual filesystem interfaces, more efficiency in 'path' manipulation
 * and usage, and cleaner filesystem code internally.
 */
#define TCL_FILESYSTEM_VERSION_2	((Tcl_FSVersion) 0x2)
typedef ClientData (TclFSGetCwdProc2) _ANSI_ARGS_((ClientData clientData));

/*
 * The following types are used for getting and storing platform-specific
 * file attributes in tclFCmd.c and the various platform-versions of
 * that file. This is done to have as much common code as possible
 * in the file attributes code. For more information about the callbacks,
 * see TclFileAttrsCmd in tclFCmd.c.
 */

typedef int (TclGetFileAttrProc) _ANSI_ARGS_((Tcl_Interp *interp,
	int objIndex, Tcl_Obj *fileName, Tcl_Obj **attrObjPtrPtr));
typedef int (TclSetFileAttrProc) _ANSI_ARGS_((Tcl_Interp *interp,
	int objIndex, Tcl_Obj *fileName, Tcl_Obj *attrObjPtr));

typedef struct TclFileAttrProcs {
    TclGetFileAttrProc *getProc;	/* The procedure for getting attrs. */
    TclSetFileAttrProc *setProc;	/* The procedure for setting attrs. */
} TclFileAttrProcs;

/*
 * Opaque handle used in pipeline routines to encapsulate platform-dependent
 * state. 
 */

typedef struct TclFile_ *TclFile;
    
/*
 * The "globParameters" argument of the function TclGlob is an
 * or'ed combination of the following values:
 */

#define TCL_GLOBMODE_NO_COMPLAIN	1
#define TCL_GLOBMODE_JOIN		2
#define TCL_GLOBMODE_DIR		4
#define TCL_GLOBMODE_TAILS		8

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

typedef int (TclStatProc_) _ANSI_ARGS_((CONST char *path, struct stat *buf));
typedef int (TclAccessProc_) _ANSI_ARGS_((CONST char *path, int mode));
typedef Tcl_Channel (TclOpenFileChannelProc_) _ANSI_ARGS_((Tcl_Interp *interp,
	CONST char *fileName, CONST char *modeString,
	int permissions));


/*
 *----------------------------------------------------------------
 * Data structures related to procedures
 *----------------------------------------------------------------
 */

typedef Tcl_CmdProc *TclCmdProcType;
typedef Tcl_ObjCmdProc *TclObjCmdProcType;

/*
 *----------------------------------------------------------------
 * Data structures for process-global values.
 *----------------------------------------------------------------
 */

typedef void (TclInitProcessGlobalValueProc) _ANSI_ARGS_((char **valuePtr,
	int *lengthPtr, Tcl_Encoding *encodingPtr));

/*
 * A ProcessGlobalValue struct exists for each internal value in
 * Tcl that is to be shared among several threads.  Each thread
 * sees a (Tcl_Obj) copy of the value, and the master is kept as
 * a counted string, with epoch and mutex control.  Each ProcessGlobalValue
 * struct should be a static variable in some file.
 */
typedef struct ProcessGlobalValue {
    int epoch;			/* Epoch counter to detect changes
				 * in the master value */
    int numBytes;		/* Length of the master string */
    char *value;		/* The master string value */
    Tcl_Encoding encoding;	/* system encoding when master string
				 * was initialized */
    TclInitProcessGlobalValueProc *proc;
    				/* A procedure to initialize the
				 * master string copy when a "get"
				 * request comes in before any
				 * "set" request has been received. */
    Tcl_Mutex mutex;		/* Enforce orderly access from
				 * multiple threads */
    Tcl_ThreadDataKey key;	/* Key for per-thread data holding
				 * the (Tcl_Obj) copy for each thread */
} ProcessGlobalValue;

/*
 *----------------------------------------------------------------
 * Variables shared among Tcl modules but not used by the outside world.
 *----------------------------------------------------------------
 */

MODULE_SCOPE char *	tclNativeExecutableName;
MODULE_SCOPE int	tclFindExecutableSearchDone;
MODULE_SCOPE char *	tclMemDumpFileName;
MODULE_SCOPE TclPlatformType tclPlatform;
MODULE_SCOPE Tcl_NotifierProcs tclOriginalNotifier;

/* TIP #233 (Virtualized Time)
 * Data for the time hooks, if any.
 */

MODULE_SCOPE Tcl_GetTimeProc*   tclGetTimeProcPtr;
MODULE_SCOPE Tcl_ScaleTimeProc* tclScaleTimeProcPtr;
MODULE_SCOPE ClientData         tclTimeClientData;

/*
 * Variables denoting the Tcl object types defined in the core.
 */

MODULE_SCOPE Tcl_ObjType tclBooleanType;
MODULE_SCOPE Tcl_ObjType tclByteArrayType;
MODULE_SCOPE Tcl_ObjType tclByteCodeType;
MODULE_SCOPE Tcl_ObjType tclDoubleType;
MODULE_SCOPE Tcl_ObjType tclEndOffsetType;
MODULE_SCOPE Tcl_ObjType tclIntType;
MODULE_SCOPE Tcl_ObjType tclListType;
MODULE_SCOPE Tcl_ObjType tclDictType;
MODULE_SCOPE Tcl_ObjType tclProcBodyType;
MODULE_SCOPE Tcl_ObjType tclStringType;
MODULE_SCOPE Tcl_ObjType tclArraySearchType;
MODULE_SCOPE Tcl_ObjType tclIndexType;
MODULE_SCOPE Tcl_ObjType tclNsNameType;
MODULE_SCOPE Tcl_ObjType tclEnsembleCmdType;
MODULE_SCOPE Tcl_ObjType tclWideIntType;
MODULE_SCOPE Tcl_ObjType tclLocalVarNameType;
MODULE_SCOPE Tcl_ObjType tclRegexpType;
MODULE_SCOPE Tcl_ObjType tclLevelReferenceType;

/*
 * Variables denoting the hash key types defined in the core.
 */

MODULE_SCOPE Tcl_HashKeyType tclArrayHashKeyType;
MODULE_SCOPE Tcl_HashKeyType tclOneWordHashKeyType;
MODULE_SCOPE Tcl_HashKeyType tclStringHashKeyType;
MODULE_SCOPE Tcl_HashKeyType tclObjHashKeyType;

/*
 * The head of the list of free Tcl objects, and the total number of Tcl
 * objects ever allocated and freed.
 */

MODULE_SCOPE Tcl_Obj *	tclFreeObjList;

#ifdef TCL_COMPILE_STATS
MODULE_SCOPE long	tclObjsAlloced;
MODULE_SCOPE long	tclObjsFreed;
#define TCL_MAX_SHARED_OBJ_STATS 5
MODULE_SCOPE long	tclObjsShared[TCL_MAX_SHARED_OBJ_STATS];
#endif /* TCL_COMPILE_STATS */

/*
 * Pointer to a heap-allocated string of length zero that the Tcl core uses
 * as the value of an empty string representation for an object. This value
 * is shared by all new objects allocated by Tcl_NewObj.
 */

MODULE_SCOPE char *	tclEmptyStringRep;
MODULE_SCOPE char	tclEmptyString;

/*
 *----------------------------------------------------------------
 * Procedures shared among Tcl modules but not used by the outside
 * world:
 *----------------------------------------------------------------
 */

MODULE_SCOPE void	TclAppendLimitedToObj _ANSI_ARGS_((Tcl_Obj *objPtr, 
			    CONST char *bytes, int length, int limit,
			    CONST char *ellipsis));
MODULE_SCOPE void	TclAppendObjToErrorInfo _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *objPtr));
MODULE_SCOPE int	TclArraySet _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *arrayNameObj, Tcl_Obj *arrayElemObj));
MODULE_SCOPE int	TclCheckBadOctal _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *value));
MODULE_SCOPE void	TclCleanupLiteralTable _ANSI_ARGS_((
			    Tcl_Interp* interp, LiteralTable* tablePtr));
MODULE_SCOPE void	TclExpandTokenArray _ANSI_ARGS_((
			    Tcl_Parse *parsePtr));
MODULE_SCOPE int	TclFileAttrsCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	TclFileCopyCmd _ANSI_ARGS_((Tcl_Interp *interp, 
			    int objc, Tcl_Obj *CONST objv[])) ;
MODULE_SCOPE int	TclFileDeleteCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	TclFileMakeDirsCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[])) ;
MODULE_SCOPE int	TclFileRenameCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[])) ;
MODULE_SCOPE void	TclFinalizeAllocSubsystem _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeCompExecEnv _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeCompilation _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeEncodingSubsystem _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeEnvironment _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeExecution _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeIOSubsystem _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeFilesystem _ANSI_ARGS_((void));
MODULE_SCOPE void	TclResetFilesystem _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeLoad _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeMemorySubsystem _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeNotifier _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeAsync _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeSynchronization _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeLock _ANSI_ARGS_((void));
MODULE_SCOPE void	TclFinalizeThreadData _ANSI_ARGS_((void));
MODULE_SCOPE int	TclFSFileAttrIndex _ANSI_ARGS_((Tcl_Obj *pathPtr,
			    CONST char *attributeName, int *indexPtr));
MODULE_SCOPE Tcl_Obj *	TclGetBgErrorHandler _ANSI_ARGS_((Tcl_Interp *interp));
MODULE_SCOPE int        TclGetNamespaceFromObj _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *objPtr,
			    Tcl_Namespace **nsPtrPtr));

MODULE_SCOPE Tcl_Obj *	TclGetProcessGlobalValue _ANSI_ARGS_ ((
			    ProcessGlobalValue *pgvPtr));
MODULE_SCOPE int	TclGlob _ANSI_ARGS_((Tcl_Interp *interp,
			    char *pattern, Tcl_Obj *unquotedPrefix, 
			    int globFlags, Tcl_GlobTypeData* types));
MODULE_SCOPE void	TclInitAlloc _ANSI_ARGS_((void));
MODULE_SCOPE void	TclInitDbCkalloc _ANSI_ARGS_((void));
MODULE_SCOPE void	TclInitEmbeddedConfigurationInformation 
			    _ANSI_ARGS_((Tcl_Interp *interp));
MODULE_SCOPE void	TclInitEncodingSubsystem _ANSI_ARGS_((void));
MODULE_SCOPE void	TclInitIOSubsystem _ANSI_ARGS_((void));
MODULE_SCOPE void	TclInitLimitSupport _ANSI_ARGS_((Tcl_Interp *interp));
MODULE_SCOPE void	TclInitNamespaceSubsystem _ANSI_ARGS_((void));
MODULE_SCOPE void	TclInitNotifier _ANSI_ARGS_((void));
MODULE_SCOPE void	TclInitObjSubsystem _ANSI_ARGS_((void));
MODULE_SCOPE void	TclInitSubsystems ();
MODULE_SCOPE int	TclIsLocalScalar _ANSI_ARGS_((CONST char *src,
			    int len));
MODULE_SCOPE int	TclJoinThread _ANSI_ARGS_((Tcl_ThreadId id,
			    int* result));
MODULE_SCOPE void	TclLimitRemoveAllHandlers _ANSI_ARGS_((
			    Tcl_Interp *interp));
MODULE_SCOPE Tcl_Obj *	TclLindexList _ANSI_ARGS_((Tcl_Interp* interp,
			    Tcl_Obj* listPtr, Tcl_Obj* argPtr));
MODULE_SCOPE Tcl_Obj *	TclLindexFlat _ANSI_ARGS_((Tcl_Interp* interp,
			    Tcl_Obj* listPtr, int indexCount,
			    Tcl_Obj *CONST indexArray[]));
MODULE_SCOPE int	TclLoadFile _ANSI_ARGS_((Tcl_Interp* interp,
			    Tcl_Obj *pathPtr, int symc,
			    CONST char *symbols[],
			    Tcl_PackageInitProc **procPtrs[],
			    Tcl_LoadHandle *handlePtr,
			    ClientData *clientDataPtr,
			    Tcl_FSUnloadFileProc **unloadProcPtr));
MODULE_SCOPE Tcl_Obj *	TclLsetList _ANSI_ARGS_((Tcl_Interp* interp,
			    Tcl_Obj* listPtr, Tcl_Obj* indexPtr,
			    Tcl_Obj* valuePtr));
MODULE_SCOPE Tcl_Obj *	TclLsetFlat _ANSI_ARGS_((Tcl_Interp* interp,
			    Tcl_Obj* listPtr, int indexCount,
			    Tcl_Obj *CONST indexArray[], Tcl_Obj* valuePtr));
MODULE_SCOPE int	TclMergeReturnOptions _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[],
			    Tcl_Obj **optionsPtrPtr, int *codePtr,
			    int *levelPtr));
MODULE_SCOPE int	TclObjInvokeNamespace _ANSI_ARGS_((Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[],
			    Tcl_Namespace *nsPtr, int flags));
MODULE_SCOPE int	TclParseBackslash _ANSI_ARGS_((CONST char *src,
			    int numBytes, int *readPtr, char *dst));
MODULE_SCOPE int	TclParseHex _ANSI_ARGS_((CONST char *src, int numBytes,
			    Tcl_UniChar *resultPtr));
MODULE_SCOPE void	TclParseInit _ANSI_ARGS_ ((Tcl_Interp *interp,
			    CONST char *string, int numBytes,
			    Tcl_Parse *parsePtr));
MODULE_SCOPE int	TclParseInteger _ANSI_ARGS_((CONST char *string,
			    int numBytes));
MODULE_SCOPE int	TclParseWhiteSpace _ANSI_ARGS_((CONST char *src,
			    int numBytes, Tcl_Parse *parsePtr, char *typePtr));
MODULE_SCOPE int	TclProcessReturn _ANSI_ARGS_((Tcl_Interp *interp,
			    int code, int level, Tcl_Obj *returnOpts));
MODULE_SCOPE int	TclpObjLstat _ANSI_ARGS_((Tcl_Obj *pathPtr, 
			    Tcl_StatBuf *buf));
MODULE_SCOPE int	TclpCheckStackSpace _ANSI_ARGS_((void));
MODULE_SCOPE Tcl_Obj *	TclpTempFileName _ANSI_ARGS_((void));
MODULE_SCOPE Tcl_Obj *	TclNewFSPathObj _ANSI_ARGS_((Tcl_Obj *dirPtr, 
			    CONST char *addStrRep, int len));
MODULE_SCOPE int	TclpDeleteFile _ANSI_ARGS_((CONST char *path));
MODULE_SCOPE void	TclpFinalizeCondition _ANSI_ARGS_((
			    Tcl_Condition *condPtr));
MODULE_SCOPE void	TclpFinalizeMutex _ANSI_ARGS_((Tcl_Mutex *mutexPtr));
MODULE_SCOPE void	TclpFinalizeThreadData _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr));
MODULE_SCOPE int	TclpThreadCreate _ANSI_ARGS_((
			    Tcl_ThreadId *idPtr,
			    Tcl_ThreadCreateProc proc,
			    ClientData clientData,
			    int stackSize, int flags));
MODULE_SCOPE void	TclpFinalizeThreadDataKey _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr));
MODULE_SCOPE int	TclpFindVariable _ANSI_ARGS_((CONST char *name,
			    int *lengthPtr));
MODULE_SCOPE void	TclpInitLibraryPath _ANSI_ARGS_((char **valuePtr,
			    int *lengthPtr, Tcl_Encoding *encodingPtr));
MODULE_SCOPE void	TclpInitLock _ANSI_ARGS_((void));
MODULE_SCOPE void	TclpInitPlatform _ANSI_ARGS_((void));
MODULE_SCOPE void	TclpInitUnlock _ANSI_ARGS_((void));
MODULE_SCOPE int	TclpLoadFile _ANSI_ARGS_((Tcl_Interp *interp, 
			    Tcl_Obj *pathPtr, CONST char *sym1,
			    CONST char *sym2, Tcl_PackageInitProc **proc1Ptr,
			    Tcl_PackageInitProc **proc2Ptr, 
			    ClientData *clientDataPtr,
			    Tcl_FSUnloadFileProc **unloadProcPtr));
MODULE_SCOPE Tcl_Obj *	TclpObjListVolumes _ANSI_ARGS_((void));
MODULE_SCOPE void	TclpMasterLock _ANSI_ARGS_((void));
MODULE_SCOPE void	TclpMasterUnlock _ANSI_ARGS_((void));
MODULE_SCOPE int	TclpMatchFiles _ANSI_ARGS_((Tcl_Interp *interp,
			    char *separators, Tcl_DString *dirPtr,
			    char *pattern, char *tail));
MODULE_SCOPE int	TclpObjNormalizePath _ANSI_ARGS_((Tcl_Interp *interp, 
			    Tcl_Obj *pathPtr, int nextCheckpoint));
MODULE_SCOPE void	TclpNativeJoinPath _ANSI_ARGS_((Tcl_Obj *prefix, 
			    char *joining));
MODULE_SCOPE Tcl_Obj *	TclpNativeSplitPath _ANSI_ARGS_((Tcl_Obj *pathPtr, 
			    int *lenPtr));
MODULE_SCOPE Tcl_PathType TclpGetNativePathType _ANSI_ARGS_((Tcl_Obj *pathPtr,
			    int *driveNameLengthPtr, Tcl_Obj **driveNameRef));
MODULE_SCOPE int 	TclCrossFilesystemCopy _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_Obj *source,
			    Tcl_Obj *target));
MODULE_SCOPE int	TclpMatchInDirectory _ANSI_ARGS_((Tcl_Interp *interp, 
			    Tcl_Obj *resultPtr, Tcl_Obj *pathPtr, 
			    CONST char *pattern, Tcl_GlobTypeData *types));
MODULE_SCOPE ClientData	TclpGetNativeCwd _ANSI_ARGS_((ClientData clientData));
MODULE_SCOPE Tcl_FSDupInternalRepProc TclNativeDupInternalRep;
MODULE_SCOPE Tcl_Obj*	TclpObjLink _ANSI_ARGS_((Tcl_Obj *pathPtr, 
			    Tcl_Obj *toPtr, int linkType));
MODULE_SCOPE int	TclpObjChdir _ANSI_ARGS_((Tcl_Obj *pathPtr));
MODULE_SCOPE Tcl_Obj *	TclPathPart _ANSI_ARGS_((Tcl_Interp *interp, 
			    Tcl_Obj *pathPtr, Tcl_PathPart portion));
MODULE_SCOPE void	TclpPanic _ANSI_ARGS_(TCL_VARARGS(CONST char *,
			    format));
MODULE_SCOPE char *	TclpReadlink _ANSI_ARGS_((CONST char *fileName,
			    Tcl_DString *linkPtr));
MODULE_SCOPE void	TclpReleaseFile _ANSI_ARGS_((TclFile file));
MODULE_SCOPE void	TclpSetInterfaces ();
MODULE_SCOPE void	TclpSetVariables _ANSI_ARGS_((Tcl_Interp *interp));
MODULE_SCOPE void	TclpUnloadFile _ANSI_ARGS_((
			    Tcl_LoadHandle loadHandle));
MODULE_SCOPE VOID *	TclpThreadDataKeyGet _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr));
MODULE_SCOPE void	TclpThreadDataKeyInit _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr));
MODULE_SCOPE void	TclpThreadDataKeySet _ANSI_ARGS_((
			    Tcl_ThreadDataKey *keyPtr, VOID *data));
MODULE_SCOPE void	TclpThreadExit _ANSI_ARGS_((int status));
MODULE_SCOPE int	TclpThreadGetStackSize _ANSI_ARGS_((void));
MODULE_SCOPE void	TclRememberCondition _ANSI_ARGS_((
			    Tcl_Condition *mutex));
MODULE_SCOPE void	TclRememberDataKey _ANSI_ARGS_((
			    Tcl_ThreadDataKey *mutex));
MODULE_SCOPE VOID	TclRememberJoinableThread _ANSI_ARGS_((
			    Tcl_ThreadId id));
MODULE_SCOPE void	TclRememberMutex _ANSI_ARGS_((Tcl_Mutex *mutex));
MODULE_SCOPE void	TclRemoveScriptLimitCallbacks _ANSI_ARGS_((
			    Tcl_Interp *interp));
MODULE_SCOPE void	TclSetBgErrorHandler _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *cmdPrefix));
MODULE_SCOPE void	TclSetProcessGlobalValue _ANSI_ARGS_ ((
			    ProcessGlobalValue *pgvPtr, Tcl_Obj *newValue,
			    Tcl_Encoding encoding));
MODULE_SCOPE VOID	TclSignalExitThread _ANSI_ARGS_((Tcl_ThreadId id,
			    int result));
MODULE_SCOPE int	TclSubstTokens _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Token *tokenPtr, int count,
			    int *tokensLeftPtr));
MODULE_SCOPE void	TclTransferResult _ANSI_ARGS_((
			    Tcl_Interp *sourceInterp, int result,
			    Tcl_Interp *targetInterp));
MODULE_SCOPE Tcl_Obj *	TclpNativeToNormalized _ANSI_ARGS_((
			    ClientData clientData));
MODULE_SCOPE Tcl_Obj *	TclpFilesystemPathType _ANSI_ARGS_((
			    Tcl_Obj* pathPtr));
MODULE_SCOPE Tcl_PackageInitProc* TclpFindSymbol _ANSI_ARGS_((
			    Tcl_Interp *interp, Tcl_LoadHandle loadHandle,
			    CONST char *symbol));
MODULE_SCOPE int	TclpDlopen _ANSI_ARGS_((Tcl_Interp *interp, 
			    Tcl_Obj *pathPtr, Tcl_LoadHandle *loadHandle, 
			    Tcl_FSUnloadFileProc **unloadProcPtr));
MODULE_SCOPE int	TclpUtime _ANSI_ARGS_((Tcl_Obj *pathPtr,
			    struct utimbuf *tval));

/*
 *----------------------------------------------------------------
 * Command procedures in the generic core:
 *----------------------------------------------------------------
 */

MODULE_SCOPE int	Tcl_AfterObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_AppendObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ArrayObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_BinaryObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_BreakObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_CaseObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_CatchObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_CdObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	TclClockClicksObjCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	TclClockGetenvObjCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	TclClockMicrosecondsObjCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	TclClockMillisecondsObjCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	TclClockSecondsObjCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	TclClockLocaltimeObjCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	TclClockMktimeObjCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	TclClockOldscanObjCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_CloseObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ConcatObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ContinueObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE Tcl_TimerToken TclCreateAbsoluteTimerHandler _ANSI_ARGS_((
			    Tcl_Time *timePtr, Tcl_TimerProc *proc,
			    ClientData clientData));
MODULE_SCOPE int	TclDefaultBgErrorHandlerObjCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_DictObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_EncodingObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_EofObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ErrorObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_EvalObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ExecObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ExitObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ExprObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_FblockedObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_FconfigureObjCmd _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_FcopyObjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_FileObjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_FileEventObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_FlushObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ForObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ForeachObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_FormatObjCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_GetsObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_GlobalObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_GlobObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_IfObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_IncrObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_InfoObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_InterpObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_JoinObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LappendObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LassignObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LindexObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LinsertObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LlengthObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ListObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LoadObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LrangeObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LrepeatObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LreplaceObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LsearchObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LsetObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp* interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_LsortObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_NamespaceObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_OpenObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_PackageObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_PidObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_PutsObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_PwdObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ReadObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_RegexpObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_RegsubObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_RenameObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ReturnObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_ScanObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_SeekObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_SetObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_SplitObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_SocketObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_SourceObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_StringObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_SubstObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_SwitchObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_TellObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_TimeObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_TraceObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_UnloadObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_UnsetObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_UpdateObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_UplevelObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_UpvarObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_VariableObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_VwaitObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));
MODULE_SCOPE int	Tcl_WhileObjCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]));

/*
 *----------------------------------------------------------------
 * Compilation procedures for commands in the generic core:
 *----------------------------------------------------------------
 */

MODULE_SCOPE int	TclCompileAppendCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileBreakCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileCatchCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileContinueCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileExprCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileForCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileForeachCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileIfCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileIncrCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileLappendCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileLassignCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileLindexCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileListCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileLlengthCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileLsetCmd _ANSI_ARGS_((Tcl_Interp* interp,
			    Tcl_Parse* parsePtr, struct CompileEnv* envPtr));
MODULE_SCOPE int	TclCompileRegexpCmd _ANSI_ARGS_((Tcl_Interp* interp,
			    Tcl_Parse* parsePtr, struct CompileEnv* envPtr));
MODULE_SCOPE int	TclCompileReturnCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileSetCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileStringCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileSwitchCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));
MODULE_SCOPE int	TclCompileWhileCmd _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, struct CompileEnv *envPtr));

/*
 * Functions defined in generic/tclVar.c and currenttly exported only 
 * for use by the bytecode compiler and engine. Some of these could later 
 * be placed in the public interface.
 */

MODULE_SCOPE Var *	TclLookupArrayElement _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *arrayName, CONST char *elName,
			    CONST int flags, CONST char *msg,
			    CONST int createPart1, CONST int createPart2,
			    Var *arrayPtr));	
MODULE_SCOPE Var *	TclObjLookupVar _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *part1Ptr, CONST char *part2, int flags,
			    CONST char *msg, CONST int createPart1,
			    CONST int createPart2, Var **arrayPtrPtr));
MODULE_SCOPE Tcl_Obj *	TclPtrGetVar _ANSI_ARGS_((Tcl_Interp *interp,
			    Var *varPtr, Var *arrayPtr, CONST char *part1,
			    CONST char *part2, CONST int flags));
MODULE_SCOPE Tcl_Obj *	TclPtrSetVar _ANSI_ARGS_((Tcl_Interp *interp,
			    Var *varPtr, Var *arrayPtr, CONST char *part1,
			    CONST char *part2, Tcl_Obj *newValuePtr,
			    CONST int flags));
MODULE_SCOPE Tcl_Obj *	TclPtrIncrVar _ANSI_ARGS_((Tcl_Interp *interp,
			    Var *varPtr, Var *arrayPtr, CONST char *part1,
			    CONST char *part2, CONST long i, CONST int flags));
MODULE_SCOPE Tcl_Obj *	TclPtrIncrWideVar _ANSI_ARGS_((Tcl_Interp *interp,
			    Var *varPtr, Var *arrayPtr, CONST char *part1,
			    CONST char *part2, CONST Tcl_WideInt i,
			    CONST int flags));

/*
 *----------------------------------------------------------------
 * Macros used by the Tcl core to create and release Tcl objects.
 * TclNewObj(objPtr) creates a new object denoting an empty string.
 * TclDecrRefCount(objPtr) decrements the object's reference count,
 * and frees the object if its reference count is zero.
 * These macros are inline versions of Tcl_NewObj() and
 * Tcl_DecrRefCount(). Notice that the names differ in not having
 * a "_" after the "Tcl". Notice also that these macros reference
 * their argument more than once, so you should avoid calling them
 * with an expression that is expensive to compute or has
 * side effects. The ANSI C "prototypes" for these macros are:
 *
 * MODULE_SCOPE void	TclNewObj _ANSI_ARGS_((Tcl_Obj *objPtr));
 * MODULE_SCOPE void	TclDecrRefCount _ANSI_ARGS_((Tcl_Obj *objPtr));
 *
 * These macros are defined in terms of two macros that depend on 
 * memory allocator in use: TclAllocObjStorage, TclFreeObjStorage.
 * They are defined below.
 *----------------------------------------------------------------
 */

#ifdef TCL_COMPILE_STATS
#  define TclIncrObjsAllocated() \
    tclObjsAlloced++
#  define TclIncrObjsFreed() \
    tclObjsFreed++
#else
#  define TclIncrObjsAllocated()
#  define TclIncrObjsFreed()
#endif /* TCL_COMPILE_STATS */

#ifndef TCL_MEM_DEBUG
# define TclNewObj(objPtr) \
    TclIncrObjsAllocated(); \
    TclAllocObjStorage(objPtr); \
    (objPtr)->refCount = 0; \
    (objPtr)->bytes    = tclEmptyStringRep; \
    (objPtr)->length   = 0; \
    (objPtr)->typePtr  = NULL

# define TclDecrRefCount(objPtr) \
    if (--(objPtr)->refCount <= 0) { \
	if ((objPtr)->typePtr && (objPtr)->typePtr->freeIntRepProc) { \
	    TclFreeObj(objPtr); \
	} else { \
	    if ((objPtr)->bytes \
                    && ((objPtr)->bytes != tclEmptyStringRep)) { \
		ckfree((char *) (objPtr)->bytes); \
	    } \
	    TclFreeObjStorage(objPtr); \
	    TclIncrObjsFreed(); \
	} \
    }
	    
#if defined(PURIFY)

/*
 * The PURIFY mode is like the regular mode, but instead of doing block
 * Tcl_Obj allocation and keeping a freed list for efficiency, it always
 * allocates and frees a single Tcl_Obj so that tools like Purify can
 * better track memory leaks
 */

#  define TclAllocObjStorage(objPtr) \
	(objPtr) = (Tcl_Obj *) Tcl_Alloc(sizeof(Tcl_Obj))

#  define TclFreeObjStorage(objPtr) \
	ckfree((char *) (objPtr))

#elif defined(TCL_THREADS) && defined(USE_THREAD_ALLOC)

/*
 * The TCL_THREADS mode is like the regular mode but allocates Tcl_Obj's
 * from per-thread caches.
 */

MODULE_SCOPE Tcl_Obj *	TclThreadAllocObj _ANSI_ARGS_((void));
MODULE_SCOPE void	TclThreadFreeObj _ANSI_ARGS_((Tcl_Obj *));
MODULE_SCOPE Tcl_Mutex *TclpNewAllocMutex _ANSI_ARGS_((void));
MODULE_SCOPE void *	TclpGetAllocCache _ANSI_ARGS_((void));
MODULE_SCOPE void	TclpSetAllocCache _ANSI_ARGS_((void *));
MODULE_SCOPE void	TclFinalizeThreadAlloc _ANSI_ARGS_((void));
MODULE_SCOPE void	TclpFreeAllocMutex _ANSI_ARGS_((Tcl_Mutex* mutex));

#  define TclAllocObjStorage(objPtr) \
	(objPtr) = TclThreadAllocObj()

#  define TclFreeObjStorage(objPtr) \
	TclThreadFreeObj((objPtr))

#else /* not PURIFY or USE_THREAD_ALLOC */

#ifdef TCL_THREADS
/* declared in tclObj.c */
MODULE_SCOPE Tcl_Mutex	tclObjMutex;
#endif

#  define TclAllocObjStorage(objPtr) \
	Tcl_MutexLock(&tclObjMutex); \
	if (tclFreeObjList == NULL) { \
	    TclAllocateFreeObjects(); \
	} \
	(objPtr) = tclFreeObjList; \
	tclFreeObjList = (Tcl_Obj *) \
		tclFreeObjList->internalRep.otherValuePtr; \
	Tcl_MutexUnlock(&tclObjMutex)

#  define TclFreeObjStorage(objPtr) \
	Tcl_MutexLock(&tclObjMutex); \
	(objPtr)->internalRep.otherValuePtr = (VOID *) tclFreeObjList; \
	tclFreeObjList = (objPtr); \
	Tcl_MutexUnlock(&tclObjMutex)
#endif

#else /* TCL_MEM_DEBUG */
MODULE_SCOPE void	TclDbInitNewObj _ANSI_ARGS_((Tcl_Obj *objPtr));

# define TclDbNewObj(objPtr, file, line) \
    TclIncrObjsAllocated(); \
    (objPtr) = (Tcl_Obj *) Tcl_DbCkalloc(sizeof(Tcl_Obj), (file), (line)); \
    TclDbInitNewObj(objPtr);

# define TclNewObj(objPtr) \
    TclDbNewObj(objPtr, __FILE__, __LINE__);

# define TclDecrRefCount(objPtr) \
    Tcl_DbDecrRefCount(objPtr, __FILE__, __LINE__)

# define TclNewListObjDirect(objc, objv) \
    TclDbNewListObjDirect(objc, objv, __FILE__, __LINE__)

#define TclAllocObjStorage(objPtr) \
    TclNewObj(objPtr)

#define TclFreeObjStorage(anyPtr) \
    {\
	Tcl_Obj *objPtr = (Tcl_Obj *) (anyPtr);\
	objPtr->refCount = 1;\
	objPtr->bytes = tclEmptyStringRep;\
	objPtr->length = 0;\
	objPtr->typePtr = NULL;\
	TclDecrRefCount(objPtr);\
    }

#undef USE_THREAD_ALLOC
#endif /* TCL_MEM_DEBUG */

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to set a Tcl_Obj's string representation
 * to a copy of the "len" bytes starting at "bytePtr". This code
 * works even if the byte array contains NULLs as long as the length
 * is correct. Because "len" is referenced multiple times, it should
 * be as simple an expression as possible. The ANSI C "prototype" for
 * this macro is:
 *
 * MODULE_SCOPE void	TclInitStringRep _ANSI_ARGS_((
 *			    Tcl_Obj *objPtr, char *bytePtr, int len));
 *
 * This macro should only be called on an unshared objPtr where
 *  objPtr->typePtr->freeIntRepProc == NULL
 *----------------------------------------------------------------
 */

#define TclInitStringRep(objPtr, bytePtr, len) \
    if ((len) == 0) { \
	(objPtr)->bytes	 = tclEmptyStringRep; \
	(objPtr)->length = 0; \
    } else { \
	(objPtr)->bytes = (char *) ckalloc((unsigned) ((len) + 1)); \
	memcpy((VOID *) (objPtr)->bytes, (VOID *) (bytePtr), \
		(unsigned) (len)); \
	(objPtr)->bytes[len] = '\0'; \
	(objPtr)->length = (len); \
    }

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to get the string representation's
 * byte array pointer from a Tcl_Obj. This is an inline version
 * of Tcl_GetString(). The macro's expression result is the string
 * rep's byte pointer which might be NULL. The bytes referenced by 
 * this pointer must not be modified by the caller.
 * The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE char *	TclGetString _ANSI_ARGS_((Tcl_Obj *objPtr));
 *----------------------------------------------------------------
 */

#define TclGetString(objPtr) \
    ((objPtr)->bytes? (objPtr)->bytes : Tcl_GetString((objPtr)))

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to clean out an object's internal
 * representation.  Does not actually reset the rep's bytes.
 * The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclFreeIntRep _ANSI_ARGS_((Tcl_Obj *objPtr));
 *----------------------------------------------------------------
 */

#define TclFreeIntRep(objPtr) \
    if ((objPtr)->typePtr != NULL && \
	    (objPtr)->typePtr->freeIntRepProc != NULL) { \
	(objPtr)->typePtr->freeIntRepProc(objPtr); \
    }

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to clean out an object's string
 * representation.  The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclInvalidateStringRep _ANSI_ARGS_((Tcl_Obj *objPtr));
 *----------------------------------------------------------------
 */

#define TclInvalidateStringRep(objPtr) \
    if (objPtr->bytes != NULL) { \
	if (objPtr->bytes != tclEmptyStringRep) {\
	    ckfree((char *) objPtr->bytes);\
	}\
	objPtr->bytes = NULL;\
    }\


/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to get a Tcl_WideInt value out of
 * a Tcl_Obj of the "wideInt" type.  Different implementation on
 * different platforms depending whether TCL_WIDE_INT_IS_LONG.
 *----------------------------------------------------------------
 */

#ifdef TCL_WIDE_INT_IS_LONG
#    define TclGetWide(resultVar, objPtr) \
	(resultVar) = (objPtr)->internalRep.longValue
#    define TclGetLongFromWide(resultVar, objPtr) \
	(resultVar) = (objPtr)->internalRep.longValue
#else
#    define TclGetWide(resultVar, objPtr) \
	(resultVar) = (objPtr)->internalRep.wideValue
#    define TclGetLongFromWide(resultVar, objPtr) \
	(resultVar) = Tcl_WideAsLong((objPtr)->internalRep.wideValue)
#endif

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core get a unicode char from a utf string.
 * It checks to see if we have a one-byte utf char before calling
 * the real Tcl_UtfToUniChar, as this will save a lot of time for
 * primarily ascii string handling. The macro's expression result
 * is 1 for the 1-byte case or the result of Tcl_UtfToUniChar.
 * The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE int	TclUtfToUniChar _ANSI_ARGS_((
 *			    CONST char *string, Tcl_UniChar *ch));
 *----------------------------------------------------------------
 */

#define TclUtfToUniChar(str, chPtr) \
	((((unsigned char) *(str)) < 0xC0) ? \
	    ((*(chPtr) = (Tcl_UniChar) *(str)), 1) \
	    : Tcl_UtfToUniChar(str, chPtr))

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to compare Unicode strings.  On
 * big-endian systems we can use the more efficient memcmp, but
 * this would not be lexically correct on little-endian systems.
 * The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE int	TclUniCharNcmp _ANSI_ARGS_((
 *			    CONST Tcl_UniChar *cs,
 *			    CONST Tcl_UniChar *ct, unsigned long n));
 *----------------------------------------------------------------
 */

#ifdef WORDS_BIGENDIAN
#   define TclUniCharNcmp(cs,ct,n) memcmp((cs),(ct),(n)*sizeof(Tcl_UniChar))
#else /* !WORDS_BIGENDIAN */
#   define TclUniCharNcmp Tcl_UniCharNcmp
#endif /* WORDS_BIGENDIAN */

/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to increment a namespace's export
 * export epoch counter.
 * The ANSI C "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclInvalidateNsCmdLookup _ANSI_ARGS_((
 *			    Namespace *nsPtr));
 *----------------------------------------------------------------
 */

#define TclInvalidateNsCmdLookup(nsPtr) \
    if ((nsPtr)->numExportPatterns) { \
	(nsPtr)->exportLookupEpoch++; \
    }

/*
 *----------------------------------------------------------------
 * Macros used by the Tcl core to set a Tcl_Obj's numeric representation
 * avoiding the corresponding function calls in time critical parts of the
 * core. They should only be called on unshared objects. The ANSI C
 * "prototypes" for these macros are:  
 *
 * MODULE_SCOPE void	TclSetIntObj _ANSI_ARGS_((Tcl_Obj *objPtr,
 *                           int intValue));
 * MODULE_SCOPE void	TclSetLongObj _ANSI_ARGS_((Tcl_Obj *objPtr,
 *                           long longValue));
 * MODULE_SCOPE void	TclSetBooleanObj _ANSI_ARGS_((Tcl_Obj *objPtr,
 *                           long boolValue));
 * MODULE_SCOPE void	TclSetWideIntObj _ANSI_ARGS_((Tcl_Obj *objPtr,
 *                          Tcl_WideInt w));
 * MODULE_SCOPE void	TclSetDoubleObj _ANSI_ARGS_((Tcl_Obj *objPtr,
 *                          double d));
 *
 *----------------------------------------------------------------
 */

#define TclSetIntObj(objPtr, i) \
    TclInvalidateStringRep(objPtr);\
    TclFreeIntRep(objPtr); \
    (objPtr)->internalRep.longValue = (long)(i); \
    (objPtr)->typePtr = &tclIntType

#define TclSetLongObj(objPtr, l) \
    TclSetIntObj((objPtr), (l))

#define TclSetBooleanObj(objPtr, b) \
    TclSetIntObj((objPtr), ((b)? 1 : 0));\
    (objPtr)->typePtr = &tclBooleanType

#define TclSetWideIntObj(objPtr, w) \
    TclInvalidateStringRep(objPtr);\
    TclFreeIntRep(objPtr); \
    (objPtr)->internalRep.wideValue = (Tcl_WideInt)(w); \
    (objPtr)->typePtr = &tclWideIntType

#define TclSetDoubleObj(objPtr, d) \
    TclInvalidateStringRep(objPtr);\
    TclFreeIntRep(objPtr); \
    (objPtr)->internalRep.doubleValue = (double)(d); \
    (objPtr)->typePtr = &tclDoubleType

/*
 *----------------------------------------------------------------
 * Macros used by the Tcl core to create and initialise objects of
 * standard types, avoiding the corresponding function calls in time
 * critical parts of the core. The ANSI C "prototypes" for these
 * macros are: 
 *
 * MODULE_SCOPE void	TclNewIntObj _ANSI_ARGS_((Tcl_Obj *objPtr,
 *                          int i));
 * MODULE_SCOPE void	TclNewLongObj _ANSI_ARGS_((Tcl_Obj *objPtr,
 *                          long l));
 * MODULE_SCOPE void	TclNewBooleanObj _ANSI_ARGS_((Tcl_Obj *objPtr,
 *                          int b));
 * MODULE_SCOPE void	TclNewWideObj _ANSI_ARGS_((Tcl_Obj *objPtr,
 *                          Tcl_WideInt w));
 * MODULE_SCOPE void	TclNewDoubleObj _ANSI_ARGS_((Tcl_Obj *objPtr),
 *                          double d);
 * MODULE_SCOPE void	TclNewStringObj _ANSI_ARGS_((Tcl_Obj *objPtr)
 *                          char *s, int len);
 *
 *----------------------------------------------------------------
 */
#ifndef TCL_MEM_DEBUG
#define TclNewIntObj(objPtr, i) \
    TclIncrObjsAllocated(); \
    TclAllocObjStorage(objPtr); \
    (objPtr)->refCount = 0; \
    (objPtr)->bytes = NULL; \
    (objPtr)->typePtr = &tclIntType; \
    (objPtr)->internalRep.longValue = (long)(i)

#define TclNewLongObj(objPtr, l) \
    TclNewIntObj((objPtr), (l))

#define TclNewBooleanObj(objPtr, b) \
    TclNewIntObj((objPtr), ((b)? 1 : 0));\
    (objPtr)->typePtr = &tclBooleanType

#define TclNewWideIntObj(objPtr, w) \
    TclIncrObjsAllocated(); \
    TclAllocObjStorage(objPtr); \
    (objPtr)->refCount = 0; \
    (objPtr)->bytes = NULL; \
    (objPtr)->typePtr = &tclWideIntType; \
    (objPtr)->internalRep.wideValue = (Tcl_WideInt)(w)

#define TclNewDoubleObj(objPtr, d) \
    TclIncrObjsAllocated(); \
    TclAllocObjStorage(objPtr); \
    (objPtr)->refCount = 0; \
    (objPtr)->bytes = NULL; \
    (objPtr)->typePtr = &tclDoubleType; \
    (objPtr)->internalRep.doubleValue = (double)(d)

#define TclNewStringObj(objPtr, s, len) \
    TclNewObj(objPtr); \
    TclInitStringRep((objPtr), (s), (len))

#else /* TCL_MEM_DEBUG */
#define TclNewIntObj(objPtr, i)   \
    (objPtr) = Tcl_NewIntObj(i)

#define TclNewLongObj(objPtr, l) \
    (objPtr) = Tcl_NewLongObj(l)

#define TclNewBooleanObj(objPtr, b) \
    (objPtr) = Tcl_NewBooleanObj(b)

#define TclNewWideIntObj(objPtr, w)\
    (objPtr) = Tcl_NewWideIntObj(w)

#define TclNewDoubleObj(objPtr, d) \
    (objPtr) = Tcl_NewDoubleObj(d)

#define TclNewStringObj(objPtr, s, len) \
    (objPtr) = Tcl_NewStringObj((s), (len))
#endif /* TCL_MEM_DEBUG */

#include "tclPort.h"
#include "tclIntDecls.h"
#include "tclIntPlatDecls.h"

#endif /* _TCLINT */

