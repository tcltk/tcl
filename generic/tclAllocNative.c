/*
 * tclAlloc.c --
 *
 *      This is the basic native allocator for Tcl.
 *
 * Copyright (c) 2013 by Miguel Sofer. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <stdlib.h>
#include "tclAlloc.h"

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

void
TclXpInitAlloc(void)
{
}

void
TclXpFinalizeAlloc(void)
{
}

void
TclXpFreeAllocCache(
    void *ptr)
{
}

