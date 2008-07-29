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
 * RCS: @(#) $Id: tclNRE.h,v 1.8 2008/07/29 05:52:29 msofer Exp $
 */


#ifndef _TCLNONREC
#define _TCLNONREC

/*****************************************************************************
 * Stuff during devel
 *****************************************************************************/

#define USE_SMALL_ALLOC     1 /* perf is important for some of these things! */
#define ENABLE_ASSERTS      1

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
    
#define TOP_CB(iPtr) (((Interp *)(iPtr))->execEnvPtr->callbackPtr)

#define GET_TOSPTR(iPtr)			\
    (((Interp *)iPtr)->execEnvPtr->execStackPtr->tosPtr)

/*
 * Inline version of Tcl_NRAddCallback
 */

#define TclNRAddCallback(						\
    interp,								\
    postProcPtr,							\
    data0,								\
    data1,								\
    data2,								\
    data3)								\
    {									\
	TEOV_callback *callbackPtr;					\
	TCLNR_ALLOC((interp), (callbackPtr));				\
	callbackPtr->procPtr = (postProcPtr);				\
	callbackPtr->data[0] = (data0);					\
	callbackPtr->data[1] = (data1);					\
	callbackPtr->data[2] = (data2);					\
	callbackPtr->data[3] = (data3);					\
	callbackPtr->nextPtr = TOP_CB(interp);				\
	TOP_CB(interp) = callbackPtr;					\
    }

/*
 * Tailcalls!
 */

MODULE_SCOPE Tcl_ObjCmdProc TclTailcallObjCmd;


/*****************************************************************************
 * Stuff that goes away: temp during devel
 *****************************************************************************/

#if USE_SMALL_ALLOC
#define TCLNR_ALLOC(interp, ptr) TclSmallAlloc(sizeof(TEOV_callback), ptr)
#define TCLNR_FREE(interp, ptr)  TclSmallFree((ptr))
#else
#define TCLNR_ALLOC(interp, size, ptr) (ptr = ((ClientData) ckalloc(sizeof(TEOV_callback))))
#define TCLNR_FREE(interp, ptr)  ckfree((char *) (ptr))
#endif

#if ENABLE_ASSERTS
#include <assert.h>
#else
#define assert(expr)
#endif

#endif /* _TCLNONREC */
