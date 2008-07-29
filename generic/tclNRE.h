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
 * RCS: @(#) $Id: tclNRE.h,v 1.10 2008/07/29 20:53:22 msofer Exp $
 */


#ifndef _TCLNONREC
#define _TCLNONREC

/*****************************************************************************
 * Stuff during devel
 *****************************************************************************/

#define ENABLE_ASSERTS      0
#define USE_SMALL_ALLOC     1  /* Only turn off for debugging purposes. */

/*
 * TEOV_callback -
 *
 * Main data struct for representing NR commands. It is designed to fit in
 * sizeof(Tcl_Obj) in order to exploit the fastest memory allocator available.  
 */

typedef struct TEOV_callback {
    Tcl_NRPostProc *procPtr;
    ClientData data[4];
    struct TEOV_callback *nextPtr;
} TEOV_callback;
    
#define TOP_CB(iPtr) (((Interp *)(iPtr))->execEnvPtr->callbackPtr)

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
#define TCLNR_ALLOC(interp, ptr) TclSmallAllocEx(interp, sizeof(TEOV_callback), (ptr))
#define TCLNR_FREE(interp, ptr)  TclSmallFreeEx((interp), (ptr))
#else
#define TCLNR_ALLOC(interp, ptr) (ptr = ((ClientData) ckalloc(sizeof(TEOV_callback))))
#define TCLNR_FREE(interp, ptr)  ckfree((char *) (ptr))
#endif

#if ENABLE_ASSERTS
#include <assert.h>
#else
#define assert(expr)
#endif

#endif /* _TCLNONREC */
