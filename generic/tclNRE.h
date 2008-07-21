/*
 * tclNRE.h --
 *
 *	This file contains declarations for the infrastructure for
 *	non-recursive commands. Contents may or may not migrate to tcl.h,
 *	tcl.decls, tclInt.h and/or tclInt.decls
 *
 * Copyright (c) 2008 by Miguel Sofer
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * // FIXME: RCS numbering?
 * RCS: @(#) $Id: tclNRE.h,v 1.6 2008/07/21 16:26:08 msofer Exp $
 */


#ifndef _TCLNONREC
#define _TCLNONREC

/*****************************************************************************
 * Stuff during devel
 *****************************************************************************/

#define USE_SMALL_ALLOC     1 /* perf is important for some of these things! */
#define USE_STACK_ALLOC     1 /* good mainly for debugging, crashes at
			       * smallest timing error */
#define ENABLE_ASSERTS      1

/*
 *   IMPLEMENTED IN THIS VERSION - flags for partial enabling of the different
 *   parts, useful for debugging. May not work - meant to be used at "all ones"
 */

#define USE_NR_PROC         1 /* are procs defined as NR functions or not?
			       * Used for testing that the old interfaces
			       * still work, as they are used by TclOO and
			       * iTcl */
#define USE_NR_TEBC         1 /* does TEBC know about his special powers?
			       * with 1 TEBC remains on stack, TEOV gets
			       * evicted. */
#define USE_NR_ALIAS        1 /* First examples: my job */

#define USE_NR_IMPORTS      1 /* First examples: my job */

#define USE_NR_TAILCALLS    1 /* Incomplete implementation as
			       * tcl::unsupported::tailcall; best semantics
			       * are yet not 100% clear to me. */

#define USE_NR_NS_ENSEMBLE  1 /* snit!! */

/* Here to remind me of what's still missing: none of these do anything today */

#define USE_NR_EVAL         0 /* Tcl_EvalObj should be easy; the others may
			       * require some adapting of the parser. dgp? */
#define USE_NR_UPLEVEL      0 /* piece of cake, I think */
#define USE_NR_VAR_TRACES   0 /* require major redesign, I fear. About time
			       * for it too! */

#define USE_NR_CONTINUATIONS 0

#define MAKE_3X_FASTER       0
#define RULE_THE_WORLD       0

#define USE_NR_CMD_TRACES  /* NEVER?? Maybe ... enter traces on the way in,
			    * leave traces done in the callback? So a trace
			    * just needs to replace the procPtr and
			    * clientData, and TEOV needn't know about the
			    * whole s**t! Mmhhh */

/*****************************************************************************
 * Stuff for the public api: gone to the stubs table!
 *
 * Question: should we allow more callback requests during the callback
 * itself? Easy enough to either handle or block, nothing done yet. We could
 * also "lock" the Tcl stack during postProc, but it doesn't sound
 * reasonable. I think.
 *****************************************************************************/

/*****************************************************************************
 * Private api fo NRE
 *****************************************************************************/

/*
 * Main data struct for representing NR commands (generated at runtime). 
 */

struct ByteCode;

/* Fill up a SmallAlloc: 4 free ptrs for the user */
typedef struct TEOV_callback {
    Tcl_NRPostProc *procPtr;
    ClientData data[4];
    struct TEOV_callback *nextPtr;
} TEOV_callback;
    

/* Try to keep within SmallAlloc sizes! */
typedef struct TEOV_record {
    int type;
    Command *cmdPtr;
    TEOV_callback *callbackPtr;
    struct TEOV_record *nextPtr;
    union {
	struct ByteCode *codePtr;       /* TCL_NR_BC_TYPE       */
	struct {
	    Tcl_Obj *objPtr;
	    int flags;
	} obj;
	struct {
	    int objc;
	    Tcl_Obj **objv;
	} objcv;
    } data;
#if !USE_SMALL_ALLOC
    /* Extra checks: can disappear later */
    Tcl_Obj **tosPtr;
#endif
} TEOV_record;

/*
 * The types for records; we save the first bit to indicate that it stores an
 * obj, to indicate the necessary refCount management. That is, odd numbers
 * only for obj-carrying types
 */

#define TCL_NR_NO_TYPE             0  /* for internal (cleanup) use only */
#define TCL_NR_BC_TYPE             2  /* procs, lambdas, TclOO+Itcl sometime ... */ 
#define TCL_NR_CMDSWAP_TYPE        4  /* ns-imports (cmdd redirect) */
#define TCL_NR_TAILCALL_TYPE       6  
#define TCL_NR_TEBC_SWAPENV_TYPE   8  /* continuations, micro-threads !? */

#define TCL_NR_CMD_TYPE            1  /* i-alias, ns-ens use this */
#define TCL_NR_SCRIPT_TYPE         3  /* ns-eval, uplevel use this */

#define TCL_NR_HAS_OBJ(TYPE) ((TYPE) & 1)

#define TOP_RECORD(iPtr) (((Interp *)(iPtr))->execEnvPtr->recordPtr)

#define GET_TOSPTR(iPtr)			\
    (((Interp *)iPtr)->execEnvPtr->execStackPtr->tosPtr)

#if !USE_SMALL_ALLOC
#define STORE_EXTRA(iPtr, recordPtr)		\
    recordPtr->tosPtr = GET_TOSPTR(iPtr)
#else
#define STORE_EXTRA(iPtr, recordPtr)
#endif

/* A SINGLE record being pushed is what is detected as an NRE request by TEOV */ 

#define PUSH_RECORD(iPtr, recordPtr)				\
    TCLNR_ALLOC(interp, recordPtr);	\
    recordPtr->nextPtr = TOP_RECORD(iPtr);			\
    STORE_EXTRA(iPtr, recordPtr);				\
    TOP_RECORD(iPtr) = recordPtr;				\
    recordPtr->type = TCL_NR_NO_TYPE;                             \
    recordPtr->cmdPtr = NULL;            			\
    recordPtr->callbackPtr = NULL

#define TEBC_CALL(iPtr)				\
    (((Interp *)iPtr)->execEnvPtr->tebcCall)

#define TclNRAddCallback(\
    interp,\
    postProcPtr,\
    data0,\
    data1,\
    data2,\
    data3)					\
    {						\
	TEOV_record *recordPtr;			\
	TEOV_callback *callbackPtr;		\
						\
	recordPtr = TOP_RECORD(interp);					\
	TclSmallAlloc(sizeof(TEOV_callback), callbackPtr);		\
									\
	callbackPtr->procPtr = (postProcPtr);				\
	callbackPtr->data[0] = (data0);					\
	callbackPtr->data[1] = (data1);					\
	callbackPtr->data[2] = (data2);					\
	callbackPtr->data[3] = (data3);					\
									\
	callbackPtr->nextPtr = recordPtr->callbackPtr;			\
	recordPtr->callbackPtr = callbackPtr;				\
    }
	


/*
 * These are only used by TEOV; here for ease of ref. They should move to
 * tclBasic.c later on.
 */

#define COMPLETE_RECORD(recordPtr)			\
    /* accesses variables by name, careful */		\
    recordPtr->cmdPtr = cmdPtr;				\

#if !USE_SMALL_ALLOC
#define CHECK_EXTRA(iPtr, recordPtr)		\
    (recordPtr->tosPtr == GET_TOSPTR(iPtr))
#else
#define CHECK_EXTRA(iPtr, recordPtr) 1
#endif

#define POP_RECORD(iPtr, recordPtr)		\
    {						\
	recordPtr = TOP_RECORD(iPtr);		\
	TOP_RECORD(iPtr) = recordPtr->nextPtr;	\
    }
				 

#define FREE_RECORD(iPtr, recordPtr)					\
    {									\
	TEOV_callback *callbackPtr = recordPtr->callbackPtr;		\
	if (TCL_NR_HAS_OBJ(recordPtr->type)) {		\
	    Tcl_DecrRefCount(recordPtr->data.obj.objPtr);		\
	}								\
	while (callbackPtr) {						\
	    callbackPtr = callbackPtr->nextPtr;				\
	    TclSmallFree(recordPtr->callbackPtr);			\
	}								\
	TCLNR_FREE(((Tcl_Interp *)iPtr), recordPtr);			\
    }

#define CHECK_VALID_RETURN(iPtr, recordPtr)			\
    ((TOP_RECORD(iPtr) == recordPtr)  &&			\
	    CHECK_EXTRA(iPtr, recordPtr))

#define READ_OBJV_RECORD(recordPtr)   /* TBD? Or read by hand (braille?) */


/*
 * functions
 */

#if 0
/* built as static inline in tclProc.c. Do TclOO/Itcl need this? */
MODULE_SCOPE int Tcl_NRBC  (Tcl_Interp * interp, ByteCode *codePtr,
                           Tcl_NRPostProc *postProcPtr, ClientData clientData);
#endif

/* The following starts purges the stack popping TclStackAllocs down to where
 * tosPtr has the requested value. Panics on failure.*/
MODULE_SCOPE void TclStackPurge(Tcl_Interp *interp, Tcl_Obj **tosPtr);

/*
 * Tailcalls!
 */

MODULE_SCOPE Tcl_ObjCmdProc TclNRApplyObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclNRUplevelObjCmd;
MODULE_SCOPE Tcl_ObjCmdProc TclTailcallObjCmd;


/*****************************************************************************
 * Stuff that goes away: temp during devel
 *****************************************************************************/

#if USE_SMALL_ALLOC
#define TCLNR_ALLOC(interp, ptr) TclSmallAlloc(sizeof(TEOV_record), ptr)
#define TCLNR_FREE(interp, ptr)  TclSmallFree((ptr))
#elif USE_STACK_ALLOC
#define TCLNR_ALLOC(interp, ptr) (ptr = TclStackAlloc(interp, sizeof(TEOV_record)))
#define TCLNR_FREE(interp, ptr)  TclStackFree(interp, (ptr))
#else
#define TCLNR_ALLOC(interp, size, ptr) (ptr = ((ClientData) ckalloc(sizeof(TEOV_record))))
#define TCLNR_FREE(interp, ptr)  ckfree((char *) (ptr))
#endif

#if ENABLE_ASSERTS
#include <assert.h>
#else
#define assert(expr)
#endif

#endif /* _TCLNONREC */
