/*
 *----------------------------------------------------------------------
 *
 * tclTomMathInterface.c --
 *
 *	This file contains procedures that are used as a 'glue'
 *	layer between Tcl and libtommath.
 *
 * Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclTomMathInterface.c,v 1.1.2.1 2005/01/20 19:13:54 kennykb Exp $
 */

#include "tclInt.h"
#include "tommath.h"
#include <limits.h>

/*
 *----------------------------------------------------------------------
 *
 * TclBNAlloc --
 *
 *	Allocate memory for libtommath.
 *
 * Results:
 *	Returns a pointer to the allocated block.
 *
 * This procedure is a wrapper around Tcl_Alloc, needed because of
 * a mismatched type signature between Tcl_Alloc and malloc.
 *
 *----------------------------------------------------------------------
 */	

extern void *
TclBNAlloc( size_t x )
{
    return (void*) Tcl_Alloc( (unsigned int) x );
}

/*
 *----------------------------------------------------------------------
 *
 * TclBNAlloc --
 *
 *	Change the size of an allocated block of memory in libtommath
 *
 * Results:
 *	Returns a pointer to the allocated block.
 *
 * This procedure is a wrapper around Tcl_Realloc, needed because of
 * a mismatched type signature between Tcl_Realloc and realloc.
 *
 *----------------------------------------------------------------------
 */	

extern void *
TclBNRealloc( void* p, size_t s  )
{
    return (void*) Tcl_Realloc( (char*) p, (unsigned int) s );
}

/*
 *----------------------------------------------------------------------
 *
 * TclBNFree --
 *
 *	Free allocated memory in libtommath.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 * This function is simply a wrapper around Tcl_Free, needed in
 * libtommath because of a type mismatch between free and Tcl_Free.
 *
 *----------------------------------------------------------------------
 */

extern void
TclBNFree( void* p )
{
    Tcl_Free( (char*) p);
}

/*
 *----------------------------------------------------------------------
 *
 * TclBNInitBignumFromLong --
 *
 *	Allocate and initialize a 'bignum' from a native 'long'.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The 'bignum' is constructed.
 *
 *----------------------------------------------------------------------
 */

extern void
TclBNInitBignumFromLong( mp_int* a, long initVal )
{

    int status;
    unsigned long v;
    mp_digit* p;

    /*
     * Allocate enough memory to hold the largest possible long
     */

    status = mp_init_size( a, ( ( CHAR_BIT * sizeof( long ) + DIGIT_BIT - 1 )
				/ DIGIT_BIT ) );
    if ( status != MP_OKAY ) {
	Tcl_Panic( "initialization failure in TclBNInitBignumFromLong" );
    }
    
    /* Convert arg to sign and magnitude */

    if ( initVal < 0 ) {
	a->sign = MP_NEG;
	v = -initVal;
    } else {
	a->sign = MP_ZPOS;
	v = initVal;
    }

    /* Store the magnitude in the bignum. */

    p = a->dp;
    while ( v ) {
	*p++ = (mp_digit) ( v & MP_MASK );
	v >>= MP_DIGIT_BIT;
    }
    a->used = p - a->dp;
    
}
