/*
 * tclAlloc.h --
 *
 *      This defines the interface for pluggable memory allocators for Tcl.
 *
 * Copyright (c) 2013 by Miguel Sofer. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLALLOC
#define _TCLALLOC

/*
 * These functions must be exported by the allocator. 
 */
 
char * TclpAlloc(unsigned int reqSize);
char * TclpRealloc(char *ptr, unsigned int reqSize);
void   TclpFree(char *ptr);
void * TclSmallAlloc(void);
void   TclSmallFree(void *ptr);

void   TclInitAlloc(void);
void   TclFinalizeAlloc(void);
void   TclFreeAllocCache(void *ptr);

/*
 * The allocator should allow for "purify mode" by checking the environment
 * variable TCL_PURIFY at initialization. If it is set to any value, it should
 * just shunt to plain malloc. This is used for debugging; the value can be
 * treated as a constant, it does not change in a running process.
 */

/*
 * This macro is used to properly align the memory allocated by Tcl, giving
 * the same alignment as the native malloc.
 */

#if defined(__APPLE__)
#define ALLOCALIGN	16
#else
#define ALLOCALIGN	(2*sizeof(void *))
#endif

#define ALIGN(x)    (((x) + ALLOCALIGN - 1) & ~(ALLOCALIGN - 1))

#endif
