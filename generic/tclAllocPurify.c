/*
 * tclAllocPurify.c --
 *
 *      This is the native allocator for Tcl, suitable for preloading anything else.
 *
 * Copyright (c) 2013 by Miguel Sofer. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/* This is needed just for sizeof(Tcl_Obj) and malloc*/

#include "tclInt.h"

char *
TclpAlloc(
    unsigned int reqSize)
{
    return malloc(reqSize);
}

char *
TclpRealloc(  
    char *ptr,
    unsigned int reqSize)
{
    return realloc(ptr, reqSize);
}

void
TclpFree(
    char *ptr)
{
    free(ptr);
}

void *
TclSmallAlloc(void)
{
    return malloc(sizeof(Tcl_Obj));
}

void
TclSmallFree(
    void *ptr)
{
    free(ptr);
}

void
TclInitAlloc(void)
{
}

void
TclFinalizeAlloc(void)
{
}

void
TclFreeAllocCache(
    void *ptr)
{
}

