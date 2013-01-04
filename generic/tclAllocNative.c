/*
 * tclAllocNative.c --
 *
 *      This is the basic native allocator for Tcl, using zippy's per-thread
 *      free obj lists.
 *
 * Copyright (c) 2013 by Miguel Sofer. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#define OBJQ_ONLY 1
#include "tclAllocZippy.c"

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
