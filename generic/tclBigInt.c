/*
 *----------------------------------------------------------------------
 *
 * tclBigInt.c --
 *
 *	Procedures that manipulate arbitrary-precision integers within
 *	the Tcl core.
 *
 * Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
 *
 * Based loosely on various files from the 'mpexpr' module, which
 * is copyright (c) 1994 by David I. Bell. (Note that all of the
 * code here has been reimplemented; none of Bell's original expression
 * remains.)
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclBigInt.c,v 1.1.2.1 2004/12/09 22:59:26 kennykb Exp $"
 *
 *----------------------------------------------------------------------
 */

#include "tclInt.h"

/*
 * The 'BigInt' structure holds an arbitrary-precision integer.
 */

typedef struct BigInt {

    Tcl_NarrowUInt *v;		/* Pointer to little-endian array containing
				 * the absolute value of the integer */
    size_t len;			/* Number of values in the array */
    int signum;			/* Sign of the integer */

} BigInt;

/*
 * Check assumptions about widths of various values and fail if they
 * are incorrect.
 */

#if ( SIZEOF_INT % TCL_SIZEOF_NARROW_INT != 0 )
#error "narrow integers are smaller than ordinary ones and do not divide them"
#endif
#if (TCL_SIZEOF_WIDE_INT > TCL_SIZEOF_NARROW_INT ) && ( TCL_SIZEOF_WIDE_INT % TCL_SIZEOF_NARROW_INT != 0 )
#error "narrow integers are smaller than wide ones and do not divide them"
#endif

#define NARROW_UINT_MAX ( (Tcl_WideUInt) ~ (Tcl_NarrowUInt) 0 )
#define NARROW_INT_MAX ( (Tcl_NarrowUInt) (NARROW_UINT_MAX/2 + 1) )
#define NARROW_UINT_BITS ( 8 * sizeof( Tcl_NarrowUInt ) )
#define NARROW_UINT_PER_INT \
    ( ( sizeof( int ) + sizeof( Tcl_NarrowUInt ) - 1 ) \
      / sizeof( Tcl_NarrowUInt ) )
#define NARROW_UINT_PER_WIDE_INT \
    ( ( sizeof( Tcl_WideUInt ) + sizeof( Tcl_NarrowUInt ) - 1 ) \
      / sizeof( Tcl_NarrowUInt ) )

#define NewBigInt(n) ( (BigInt*) ( ckalloc( sizeof( BigInt ) ) ) )
#define AllocVal(p,n) \
    do { \
        (p)->v \
            = (Tcl_NarrowUInt*) ( ckalloc( (n) * sizeof( Tcl_NarrowUInt ) ) );\
    } while (0)

/*
 *----------------------------------------------------------------------
 *
 * Tcl_BigIntFromInt --
 *
 *	Creates a big integer from a native integer.
 *
 * Results:
 *	Returns the big integer just created.
 *
 * This function creates a big integer from a native integer. Refer
 * to the user documentation for details.
 *
 *----------------------------------------------------------------------
 */

Tcl_BigInt
Tcl_BigIntFromInt( int intVal )	/* Value to place in the big integer */
{

    BigInt* retVal = NewBigInt();
    if ( intVal < 0 ) {
	intVal = -intVal;
	retVal->signum = 1;
    } else {
	retVal->signum = 0;
    }
#if ( SIZEOF_INT <= TCL_SIZEOF_NARROW_INT )

    /* Common case - an integer fits in a single word of retval->v */

    retVal->v = (Tcl_NarrowUInt*) ckalloc( sizeof( Tcl_NarrowUInt ) );
    retVal->v[0] = intVal;
    retVal->len = 1;

#else

    /* Hairy case - we have to pack the integer into multiple words.
     * Check for the easy single-word case first */

    if ( intVal <= NARROW_UINT_MAX ) {
	AllocVal(retVal,1);
	retVal->v[0] = intVal;
	retVal->len = 1;
    } else {
	size_t bytes, i;
	AllocVal( retVal, NARROW_UINT_PER_INT );
	i = 0;
	for ( bytes = 0;
	      bytes < sizeof( int );
	      bytes += sizeof( Tcl_NarrowUInt ) ) {
	    retVal->v[ i++ ] = (Tcl_NarrowUInt) ( intVal & NARROW_UINT_MAX );
	    intVal >>= NARROW_UINT_BITS;
	    if ( intVal == 0 ) break;
	}
	retVal->len = i;
    }	    

#endif

    return (Tcl_BigInt) retVal;

}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_BigIntFromWideInt --
 *
 *	Creates a big integer from a native integer.
 *
 * Results:
 *	Returns the big integer just created.
 *
 * This function creates a big integer from a native integer. Refer
 * to the user documentation for details.
 *
 *----------------------------------------------------------------------
 */

Tcl_BigInt
Tcl_BigIntFromWideInt( Tcl_WideInt intVal )
				/* Value to place in the big integer */
{
    BigInt* retVal = NewBigInt();
    if ( intVal < 0 ) {
	intVal = -intVal;
	retVal->signum = 1;
    } else {
	retVal->signum = 0;
    }

    if ( intVal <= NARROW_UINT_MAX ) {
	AllocVal(retVal,1);
	retVal->v[0] = intVal;
	retVal->len = 1;
    } else {
	size_t bytes, i;
	AllocVal( retVal, NARROW_UINT_PER_WIDE_INT );
	i = 0;
	for ( bytes = 0;
	      bytes < sizeof( int );
	      bytes += sizeof( Tcl_NarrowUInt ) ) {
	    retVal->v[ i++ ] = (Tcl_NarrowUInt) ( intVal & NARROW_UINT_MAX );
	    intVal >>= NARROW_UINT_BITS;
	    if ( intVal == 0 ) break;
	}
	retVal->len = i;
    }	    

    return (Tcl_BigInt) retVal;

}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FreeBigInt --
 *
 *	Frees an arbitrary-precision integer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Integer is freed.
 *
 * The Tcl_FreeBigInt function frees an arbitrary-precision integer.
 * Refer to the user documentation for details.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FreeBigInt( Tcl_BigInt bigVal )
				/* Value to free */
{
    BigInt* val = (BigInt*) bigVal;
    ckfree( (void*) val->v );
    ckfree( (void*) val );
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_BigIntIsEven, Tcl_BigIntIsOdd, Tcl_BigIntIsZero,
 * Tcl_BigIntIsNegative, Tcl_BigIntIsPositive, Tcl_BigIntIsUnit,
 * Tcl_BigIntIsOne, Tcl_BigIntIsMinusOne,
 * Tcl_BigIntIsInt, Tcl_BigIntIsWideInt --
 *
 *	Simple predicates applying to Tcl's arbitrary precision integers.
 *
 * Results:
 *	Return values of 1 or 0 according to whether the given condition
 *	is true.
 *
 * These procedures all do simple tests on a single big-integer value.
 * Refer to the user documentation for details.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_BigIntIsEven( Tcl_BigInt bigVal )
				/* Value to test */
{
    BigInt* z = (BigInt*) bigVal;
    return ( (z->v[0] & 1) == 0 );
}
int
Tcl_BigIntIsOdd( Tcl_BigInt bigVal )
				/* Value to test */
{
    BigInt* z = (BigInt*) bigVal;
    return ( (z->v[0] & 1) != 0 );
}
int
Tcl_BigIntIsZero( Tcl_BigInt bigVal )
				/* Value to test */
{
    BigInt* z = (BigInt*) bigVal;
    return ( z->len == 1 && z->v[0] == 0 );
}
int
Tcl_BigIntIsNegative( Tcl_BigInt bigVal )
				/* Value to test */
{
    BigInt* z = (BigInt*) bigVal;
    return ( z->signum );
}
int
Tcl_BigIntIsPositive( Tcl_BigInt bigVal )
				/* Value to test */
{
    BigInt* z = (BigInt*) bigVal;
    return ( z->signum == 0 && ( z->len > 1 || z->v[0] != 0 ) );
}
int
Tcl_BigIntIsUnit( Tcl_BigInt bigVal )
				/* Value to test */
{
    BigInt* z = (BigInt*) bigVal;
    return ( z->len == 1 && z->v[0] == 1 );
}
int
Tcl_BigIntIsOne( Tcl_BigInt bigVal )
				/* Value to test */
{
    BigInt* z = (BigInt*) bigVal;
    return ( z->len == 1 && z->v[0] == 1 && !z->signum );
}
int
Tcl_BigIntIsMinusOne( Tcl_BigInt bigVal )
				/* Value to test */
{
    BigInt* z = (BigInt*) bigVal;
    return ( z->len == 1 && z->v[0] == 1 && z->signum );
}
int
Tcl_BigIntIsInt( Tcl_BigInt bigVal )
{
    /*
     * ASSUMPTION: This code assumes that the size of Tcl_NarrowUInt
     *             divides sizeof(int) evenly.  The assumption is tested
     *             in #ifdefs at the head of this file.
     */
    BigInt* z = (BigInt*) bigVal;
    int mostSigWord;
    if ( z->len > NARROW_UINT_PER_INT ) {
	return 0;
    }
    if ( z->len < NARROW_UINT_PER_INT ) {
	return 1;
    }
    mostSigWord = z->v[ NARROW_UINT_PER_INT - 1 ];
    if ( z->signum ) {
	return ( mostSigWord-1 <= NARROW_INT_MAX );
    } else {
	return ( mostSigWord <= NARROW_INT_MAX );
    }
}
int
Tcl_BigIntIsWideInt( Tcl_BigInt bigVal )
{
    BigInt* z = (BigInt*) bigVal;
    int mostSigWord;
    if ( z->len > NARROW_UINT_PER_WIDE_INT ) {
	return 0;
    }
    if ( z->len < NARROW_UINT_PER_WIDE_INT ) {
	return 1;
    }
    mostSigWord = z->v[ NARROW_UINT_PER_WIDE_INT - 1 ];
    if ( z->signum ) {
	return ( mostSigWord-1 <= NARROW_INT_MAX );
    } else {
	return ( mostSigWord <= NARROW_INT_MAX );
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetIntFromBigInt, Tcl_GetWideIntFromBigInt --
 *
 *	Convert an arbitrary-precision integer to a native integer.
 *
 * Results:
 *	Returns the converted value.
 *
 * These two functions convert an arbitrary-precision integer to a
 * native one.  If the conversion results in an overflow, the
 * native integer will be the least significant bits of the
 * arbitrary-precision one.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetIntFromBigInt( Tcl_BigInt bigVal )
{
    BigInt* z = (BigInt*) bigVal;
    unsigned int retval;
#if SIZEOF_INT <= TCL_SIZEOF_NARROW_UINT
    retval = (unsigned int) (z->v[0]);
#else
    int i = z->len;
    if ( i >= NARROW_UINT_PER_INT ) {
	i = NARROW_UINT_PER_INT - 1;
    }
    retval = (unsigned int) (z->v[i--]);
    while ( i >= 0 ) {
	retval = ( retval << ( 8 * sizeof( Tcl_NarrowUInt ) ) )
	    | (unsigned int) (z->v[i--]);
    }
#endif
    if ( z->signum ) {
	return - (int) retval;
    } else {
	return (int) retval;
    }
}
Tcl_WideInt
Tcl_GetWideIntFromBigInt( Tcl_BigInt bigVal )
{
    BigInt* z = (BigInt*) bigVal;
    Tcl_WideUInt retval;
#if SIZEOF_INT <= TCL_SIZEOF_NARROW_UINT
    retval = (Tcl_WideUInt) (z->v[0]);
#else
    int i = z->len;
    if ( i >= NARROW_UINT_PER_INT ) {
	i = NARROW_UINT_PER_INT - 1;
    }
    retval = (Tcl_WideUInt) (z->v[i--]);
    while ( i >= 0 ) {
	retval = ( retval << ( 8 * sizeof( Tcl_NarrowUInt ) ) )
	    | (Tcl_WideUInt) (z->v[i--]);
    }
#endif
    if ( z->signum ) {
	return - (Tcl_WideInt) retval;
    } else {
	return (Tcl_WideInt) retval;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_BigIntCompare --
 *
 *	Compares two Tcl_BigInt values to see which is larger.
 *
 * Results:
 *	Returns -1 if bigVal1 is smaller, 0 if they are equal, 1
 *	if bigVal2 is smaller.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_CompareBigInt( Tcl_BigInt bigVal1, Tcl_BigInt bigVal2 )
{
    BigInt* z1 = (BigInt*) bigVal1;
    BigInt* z2 = (BigInt*) bigVal2;
    int sign;
    size_t i;

    /* Non-negative numbers are greater than negative ones. */

    if ( z1->signum < z2->signum ) {
	return 1;
    } else if ( z2->signum < z1->signum ) {
	return -1;
    }

    /* If both numbers are negative, reverse the sense of the comparison */

    if ( z2->signum ) {
	sign = -1;
    } else {
	sign = 1;
    }

    /* Longer numbers are greater than shorter ones. */

    if ( z1->len > z2->len ) {
	return sign;
    } else if ( z1->len < z2->len ) {
	return -sign;
    }

    /*
     * Compare the individual words of the numbers, most significant parts
     * first, and return when one number does not equal the other.

    for ( i = z2->len - 1; i >= 0; --i ) {
	Tcl_NarrowUInt part1 = z1->v[i];
	Tcl_NarrowUInt part2 = z2->v[i];
	if ( part1 > part2 ) {
	    return sign;
	} else if ( part1 < part2 ) {
	    return -sign;
	}
    }

    /* Two numbers are equal. */

    return 0;
}
				       
