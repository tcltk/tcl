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
 * These 5 functions MUST be defined by the allocator. 
 */
 
char * TclpAlloc(unsigned int reqSize);
char * TclpRealloc(char *ptr, unsigned int reqSize);
void   TclpFree(char *ptr);
void   TclXpInitAlloc(void);
void   TclXpFinalizeAlloc(void);
void   TclXpFreeAllocCache(void *ptr);


/*
 * The allocator should allow for "purify mode" by checking this variable. If
 * it is set to !0, it should just shunt to plain malloc.
 * This is used for debugging; the value can be treated as a constant, it does
 * not change in a running process.
 */

int TCL_PURIFY;
int TCL_THREADED;

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

/*
 * These are utility functions (defined in tclAlloc.c) to give access to
 * either per-thread or per-interp caches. They will return a pointer to which
 * the allocator should attach the proper structure that it wishes to
 * maintain.
 *
 * If the content is NULL, it means that the value has not been initialized for
 * this interp or thread and the corresponding Set function should be called.
 */

void TclSetSharedAllocCache(void *allocCachePtr);
void TclSetAllocCache(void *allocCachePtr);
void *TclGetAllocCache(void);

#endif
