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
 * RCS: @(#) $Id: tclBigInt.c,v 1.1.2.2 2004/12/10 21:17:42 kennykb Exp $"
 *
 *----------------------------------------------------------------------
 */

#include "tclInt.h"

/*
 * The 'BigInt' structure holds an arbitrary-precision integer.
 */

typedef struct BigInt {

    size_t len;			/* Number of values in the array */
    int signum;			/* Sign of the integer */
    Tcl_NarrowUInt v[0];	/* Little-endian array containing
				 * the absolute value of the integer */

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
#define NARROW_INT_MAX ( (Tcl_NarrowUInt) (NARROW_UINT_MAX/2) )
#define NARROW_UINT_BITS ( 8 * sizeof( Tcl_NarrowUInt ) )
#define NARROW_UINT_PER_INT \
    ( ( sizeof( int ) + sizeof( Tcl_NarrowUInt ) - 1 ) \
      / sizeof( Tcl_NarrowUInt ) )
#define NARROW_UINT_PER_WIDE_INT \
    ( ( sizeof( Tcl_WideUInt ) + sizeof( Tcl_NarrowUInt ) - 1 ) \
      / sizeof( Tcl_NarrowUInt ) )

#define NewBigInt(n) \
    ( (BigInt*) ( ckalloc( sizeof( BigInt ) \
                  + (n) * sizeof( Tcl_NarrowUInt ) ) ) )

/*
 * Static functions defined in this file
 */

static BigInt* AddAbsValues( BigInt* z1, BigInt* z2 );
				/* Add the absolute values of two large 
				 * integers */
static BigInt* SubtractAbsValues( BigInt* z1, BigInt* z2 );
				/* Subtract the absolute values of two
				 * large integers */


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

    BigInt* retVal = NewBigInt(NARROW_UINT_PER_INT);
#if ( SIZEOF_INT > TCL_SIZEOF_NARROW_INT )
    size_t i;
#endif

    if ( intVal < 0 ) {
	intVal = -intVal;
	retVal->signum = 1;
    } else {
	retVal->signum = 0;
    }

#if ( SIZEOF_INT <= TCL_SIZEOF_NARROW_INT )

    /* Common case - an integer fits in a single word of retval->v */

    retVal->v[0] = intVal;
    retVal->len = 1;

#else

    /* Hairy case - we have to pack the integer into multiple words. */

    i = 0;
    do {
	retval -> v[ i++ ] = (Tcl_NarrowUInt) ( intVal & NARROW_UINT_MAX );
	intVal >>= NARROW_UINT_BITS;
    } while ( intVal != 0 && i < NARROW_UINT_PER_INT );

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
    BigInt* retVal = NewBigInt( NARROW_UINT_PER_WIDE_INT );
    size_t i;

    if ( intVal < 0 ) {
	intVal = -intVal;
	retVal->signum = 1;
    } else {
	retVal->signum = 0;
    }

    i = 0;
    do {
	retVal -> v[ i++ ] = (Tcl_NarrowUInt) ( intVal & NARROW_UINT_MAX );
	intVal >>= NARROW_UINT_BITS;
    } while ( intVal != 0 && i < NARROW_UINT_PER_WIDE_INT );

    return (Tcl_BigInt) retVal;

}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CopyBigInt --
 *
 *	Make a deep copy of an arbitrary-precision integer.
 *
 * Results:
 *	Returns the copy.
 *
 *----------------------------------------------------------------------
 */

Tcl_BigInt
Tcl_CopyBigInt( Tcl_BigInt bigVal )
				/* Value to copy */
{
    BigInt* v = (BigInt*) bigVal;
    BigInt* retVal = NewBigInt( v->len );
    memcpy( retVal, v, sizeof( BigInt ) + v->len * sizeof( Tcl_NarrowUInt ) );
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
#if SIZEOF_INT <= TCL_SIZEOF_NARROW_INT
    retval = (unsigned int) (z->v[0]);
#else
    int i = z->len-1;
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
    int i = z->len-1;
    if ( i >= NARROW_UINT_PER_WIDE_INT ) {
	i = NARROW_UINT_PER_WIDE_INT - 1;
    }
    retval = (Tcl_WideUInt) (z->v[i--]);
    while ( i >= 0 ) {
	retval = ( retval << ( 8 * sizeof( Tcl_NarrowUInt ) ) )
	    | (Tcl_WideUInt) (z->v[i--]);
    }
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
     * first, and return when one number does not equal the other. */

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

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AddBigInt --
 *
 *	Adds together two large integers.
 *
 * Results:
 *	Returns the sum.
 *
 *----------------------------------------------------------------------
 */

Tcl_BigInt
Tcl_AddBigInt( Tcl_BigInt bigVal1, Tcl_BigInt bigVal2 )
{
    BigInt* z1 = (BigInt*) bigVal1;
    BigInt* z2 = (BigInt*) bigVal2;
    BigInt* retVal;

    /* 
     * Add numbers of opposite sign by subtracting the absolute
     * value of the negative number from the positive one.
     */

    if (z1->signum && !z2->signum) {
	retVal = SubtractAbsValues( z2, z1 );
    } else if (z2->signum && !z1->signum) {
	retVal = SubtractAbsValues( z1, z2 );
    } else {

	/* 
	 * Add numbers of like sign by adding their absolute values
	 * and correcting the sign.
	 */
	
	retVal = AddAbsValues( z1, z2 );
	retVal->signum = z1->signum; 
    }

    return (Tcl_BigInt) retVal;

}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SubtractBigInt --
 *
 *	Subtracts one large integer from another.
 *
 * Results:
 *	Returns the difference (bigVal1-bigVal2).
 *
 *----------------------------------------------------------------------
 */

Tcl_BigInt
Tcl_SubtractBigInt( Tcl_BigInt bigVal1, Tcl_BigInt bigVal2 )
{
    BigInt* z1 = (BigInt*) bigVal1;
    BigInt* z2 = (BigInt*) bigVal2;
    BigInt* retVal;

    if ( z1->signum != z2->signum ) {
	/* 
	 * Subtracting two numbers of opposite sign is done by adding
	 * their absolute values.
	 */
	retVal = AddAbsValues( z1, z2 );
	retVal->signum = z1->signum;
    } else {
	/* 
	 * Subtracting two numbers of like sign is done by subtracting
	 * absolute values and correcting the sign.
	 */
	retVal = SubtractAbsValues( z1, z2 );
	if ( z1->signum && ( retVal->len > 1 || retVal->v[0] != 0 ) ) {
	    retVal->signum = ! (retVal->signum);
	}
    }
    return (Tcl_BigInt) retVal;
}

/*
 *----------------------------------------------------------------------
 *
 * AddAbsValues --
 *
 *	Adds the absolute values of two arbitrary-precision integers.
 *
 * Results:
 *	Returns an arbitrary-precision integer whose value is the
 *	sum of the absolute values of the arguments.
 *
 *----------------------------------------------------------------------
 */

static BigInt* 
AddAbsValues( BigInt* z1, BigInt* z2 )
{

    BigInt* retVal;
    Tcl_NarrowUInt *p1, *p2, *pd;
    Tcl_WideUInt carry;
    size_t len1, len2;

    /* Determine which argument is the longer */

    if ( z2->len > z1->len ) {
	retVal = NewBigInt( z2->len + 1 );
	p1 = z1->v;
	p2 = z2->v;
	len1 = z1->len;
	len2 = z2->len - z1->len;
    } else {
	retVal = NewBigInt( z1->len + 1 );
	p1 = z1->v;
	p2 = z2->v;
	len1 = z2->len;
	len2 = z1->len - z2->len;
    }

    retVal->signum = 0;
    carry = 0;
    pd = retVal->v;

    /* Add the two arguments */

    while ( len1-- ) {
	carry += (Tcl_WideUInt) (*p1++) + (Tcl_WideUInt) (*p2++);
	*pd++ = (Tcl_NarrowUInt) (carry & NARROW_UINT_MAX);
	carry >>= NARROW_UINT_BITS;
    }

    /* Propagate carries */

    while ( len2-- ) {
	carry += (Tcl_WideUInt) (*p2++);
	*pd++ = (Tcl_NarrowUInt) (carry & NARROW_UINT_MAX);
	carry >>= NARROW_UINT_BITS;
    }

    /* 
     * If carrying into the most significant word, store the carry;
     * otherwise, correct the length.
     */

    if ( carry ) {
	*pd = (Tcl_NarrowUInt) carry;
    } else {
	--( retVal->len );
    }

    return retVal;
    
}

/*
 *----------------------------------------------------------------------
 *
 * SubtractAbsValues --
 *
 *	Subtracts the absolute value of the large integer z2
 *	from the absolute value of the large integer z1.
 *
 * Results:
 *	Returns the difference.
 *
 *----------------------------------------------------------------------
 */

static BigInt*
SubtractAbsValues( BigInt* z1, BigInt* z2 )
{
    BigInt* retVal;
    size_t len1, len2, t;
    int bigger2;
    Tcl_NarrowUInt *p1, *p2, *pd;
    Tcl_WideUInt carry;
    
    /*
     * We need to compute the size of the result before we can
     * allocate it.  If one operand is longer than the other, the
     * size of the result will be the size of the longer operand.
     * If the operands are of equal length, we compare them,
     * most significant word first, and stop when we find words that
     * are different.  The size of the result will be one more than
     * the position of the first word that differs.
     */

    len1 = z1->len;
    len2 = z2->len;
    if ( len1 != len2 ) {
	bigger2 = (len1 < len2 );
    } else {
	p1 = z1->v + ( len1 - 1 );
	p2 = z2->v + ( len2 - 1 );
	while ( ( len1 != 0 ) && ( *p1 == *p2 ) ) {
	    p1--;
	    p2--;
	    len1--;
	}
	if ( len1 == 0 ) {
	    retVal = NewBigInt(1);
	    retVal->signum = 0;
	    retVal->v[0] = 0;
	    return retVal;
	}
	len2 = len1;
	bigger2 = ( *p1 < *p2 );
    }

    /* 
     * At this point, len1 and len2 have the effective lengths of
     * operands 1 and 2, respectively. bigger2 is true if the result
     * will be negative, and indicates that the absolute value of the
     * result will be computed by reversing the sense of the subtraction
     * (subtracting z1 from z2).
     */

    if ( bigger2 ) {
	retVal = NewBigInt( len2 );
	p1 = z2->v;
	p2 = z1->v;
	len2 = (len2 - len1);
    } else {
	retVal = NewBigInt( len1 );
	p1 = z1->v;
	p2 = z2->v;
	t = (len1 - len2);
	len1 = len2;
	len2 = t;
    }
    retVal->signum = bigger2;
    pd = retVal->v;
    carry = 1;
    
    /*
     * Subtract the two operands.  
     */

    while ( len1-- ) {
	carry = carry + NARROW_UINT_MAX + *p1++ - *p2++;
	*pd++ = (Tcl_NarrowUInt) ( carry & NARROW_UINT_MAX );
	carry >>= NARROW_UINT_BITS;
    }
    while ( len2-- ) {
	carry = carry + NARROW_UINT_MAX + *p1++;
	*pd++ = (Tcl_NarrowUInt) ( carry & NARROW_UINT_MAX );
	carry >>= NARROW_UINT_BITS;
    }

    /* Clean up any zeroes at the more significant end */

    while ( pd > retVal->v ) {
	if ( *pd != 0 ) break;
	--pd;
    }
    retVal->len = pd + 1 - retVal->v;

    return retVal;
    
}
