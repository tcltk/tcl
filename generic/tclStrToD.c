/*
 *----------------------------------------------------------------------
 *
 * tclStrToD.c --
 *
 *	This file contains a TclStrToD procedure that handles conversion
 *	of string to double, with correct rounding even where extended
 *	precision is needed to achieve that.
 *
 * Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclStrToD.c,v 1.1.2.4 2005/02/06 03:43:42 kennykb Exp $
 *
 *----------------------------------------------------------------------
 */

#include <tclInt.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <tommath.h>

/*
 * The stuff below is a bit of a hack so that this file can be used in
 * environments that include no UNIX, i.e. no errno: just arrange to use
 * the errno from tclExecute.c here.
 */

#ifdef TCL_GENERIC_ONLY
#define NO_ERRNO_H
#endif

#ifdef NO_ERRNO_H
extern int errno;			/* Use errno from tclExecute.c. */
#define ERANGE 34
#endif

#if ( FLT_RADIX == 2 ) && ( DBL_MANT_DIG == 53 ) && ( DBL_MAX_EXP == 1024 )
#define IEEE_FLOATING_POINT
#endif

/*
 * gcc on x86 needs access to rounding controls.  It is tempting to
 * include fpu_control.h, but that file exists only on Linux; it is
 * missing on Cygwin and MinGW.
 */

#if defined(__GNUC__) && defined(__i386)
typedef unsigned int fpu_control_t __attribute__ ((__mode__ (__HI__)));
#define _FPU_GETCW(cw) __asm__ ("fnstcw %0" : "=m" (*&cw))
#define _FPU_SETCW(cw) __asm__ ("fldcw %0" : : "m" (*&cw))
#endif

TCL_DECLARE_MUTEX( initMutex );

/* The powers of ten that can be represented exactly as IEEE754 doubles. */

#define MAXPOW 22
static double pow10 [MAXPOW+1];

/* Inexact higher powers of ten */

static CONST double pow_10_2_n [] = {
    1.0,
    100.0,
    10000.0,
    1.0e+8,
    1.0e+16,
    1.0e+32,
    1.0e+64,
    1.0e+128,
    1.0e+256
};

/* Flag for whether the constants have been initialized */

static volatile int constantsInitialized = 0;

/* Logarithm of the floating point radix. */

static int log2FLT_RADIX;

/* Number of bits in a double's significand */

static int mantBits;

/* Table of powers of 5**(2**n), up to 5**256 */

static mp_int pow5[9];

/* Static functions defined in this file */

static void InitializeConstants _ANSI_ARGS_((void));
static void FreeConstants _ANSI_ARGS_((ClientData));
static double RefineResult _ANSI_ARGS_((double approx, CONST char* start,
					int nDigits, long exponent));
static double BignumToDouble _ANSI_ARGS_(( mp_int* a ));
static double ParseNaN _ANSI_ARGS_(( int signum, CONST char** end ));
static double SafeLdExp _ANSI_ARGS_(( double fraction, int exponent ));

/*
 *----------------------------------------------------------------------
 *
 * TclStrToD --
 *
 *	Scans a double from a string.
 *
 * Results:
 *	Returns the scanned number. In the case of underflow, returns
 *	an appropriately signed zero; in the case of overflow, returns
 *	an appropriately signed HUGE_VAL.
 *
 * Side effects:
 *	Stores a pointer to the end of the scanned number in '*endPtr',
 *	if endPtr is not NULL. If '*endPtr' is equal to 's' on return from
 *	this function, it indicates that the input string could not be
 *	recognized as a number.
 *	In the case of underflow or overflow, 'errno' is set to ERANGE.
 *
 *------------------------------------------------------------------------
 */

double
TclStrToD( CONST char* s, 
				/* String to scan */
	   CONST char ** endPtr )
				/* Pointer to the end of the scanned number */
{

    CONST char* p = s;
    CONST char* startOfSignificand = NULL;
				/* Start of the significand in the
				 * string */
    int signum = 0;		/* Sign of the significand */
    double exactSignificand = 0.0;
				/* Significand, represented exactly
				 * as a floating-point number */
    int seenDigit = 0;		/* Flag == 1 if a digit has been seen */
    int nSigDigs = 0;		/* Number of significant digits presented */
    int nDigitsAfterDp = 0;	/* Number of digits after the decimal point */
    int nTrailZero = 0;		/* Number of trailing zeros in the 
				 * significand */
    long exponent = 0;		/* Exponent */
    int seenDp = 0;		/* Flag == 1 if decimal point has been seen */

    char c;			/* One character extracted from the input */

    static int mmaxpow = 0;	/* Largest power of ten that can be
				 * represented exactly in a 'double'. */

    /* 
     * v must be 'volatile double' on gc-ix86 to force correct rounding
     * to IEEE double and not Intel double-extended.
     */

    volatile double v;		/* Scanned value */
    int machexp;		/* Exponent of the machine rep of the
				 * scanned value */
    int expt2;			/* Exponent for computing first
				 * approximation to the true value */
    int i, j;

    /*
     * With gcc on x86, the floating point rounding mode is double-extended.
     * This causes the result of double-precision calculations to be rounded
     * twice: once to the precision of double-extended and then again to the
     * precision of double.  Double-rounding introduces gratuitous errors of
     * 1 ulp, so we need to change rounding mode to 53-bits.
     */

#if defined(__GNUC__) && defined(__i386)
    fpu_control_t roundTo53Bits = 0x027f;
    fpu_control_t oldRoundingMode;
    _FPU_GETCW( oldRoundingMode );
    _FPU_SETCW( roundTo53Bits );
#endif

    InitializeConstants();

    if ( mmaxpow == 0 ) {
	int x = (int) (DBL_MANT_DIG * log((double) FLT_RADIX) / log( 5.0 ));
	if ( x < MAXPOW ) {
	    mmaxpow = x;
	} else {
	    mmaxpow = MAXPOW;
	}
    }

    /* Discard leading whitespace */

    while ( isspace( *p ) ) {
	++p;
    }

    /* Determine the sign of the significand */

    switch( *p ) {
	case '-':
	    signum = 1;
	    /* FALLTHROUGH */
	case '+':
	    ++p;
    }

    /* Discard leading zeroes */

    while ( *p == '0' ) {
	seenDigit = 1;
	++p;
    }

    /* 
     * Scan digits from the significand. Simultaneously, keep track
     * of the number of digits after the decimal point. Maintain
     * a pointer to the start of the significand. Keep "exactSignificand"
     * equal to the conversion of the DBL_DIG most significant digits.
     */

    for ( ; ; ) {
	c = *p;
	if ( c == '.' && !seenDp ) {
	    seenDp = 1;
	    ++p;
	} else if ( isdigit(c) ) {
	    if ( c == '0' ) {
		if ( startOfSignificand != NULL ) {
		    ++nTrailZero;
		}
	    } else {
		if ( startOfSignificand == NULL ) {
		    startOfSignificand = p;
		} else if ( nTrailZero ) {
		    if ( nTrailZero + nSigDigs < DBL_DIG ) {
			exactSignificand *= pow10[ nTrailZero ];
		    } else if ( nSigDigs < DBL_DIG ) {
			exactSignificand *= pow10[ DBL_DIG - nSigDigs ];
		    }
		    nSigDigs += nTrailZero;
		}
		if ( nSigDigs < DBL_DIG ) {
		    exactSignificand = 10. * exactSignificand + (c - '0');
		}
		++nSigDigs;
		nTrailZero = 0;
	    }
	    if ( seenDp ) {
		++nDigitsAfterDp;
	    }
	    seenDigit = 1;
	    ++p;
	} else {
	    break;
	}
    }

    /*
     * At this point, we've scanned the significand, and p points
     * to the character beyond it.  "startOfSignificand" is the first
     * non-zero character in the significand. "nSigDigs" is the number
     * of significant digits of the significand, not including any
     * trailing zeroes. "exactSignificand" is a floating point number
     * that represents, without loss of precision, the first
     * min(DBL_DIG,n) digits of the significand.  "nDigitsAfterDp"
     * is the number of digits after the decimal point, again excluding
     * trailing zeroes.
     *
     * Now scan 'E' format
     */

    exponent = 0;
    if ( seenDigit && ( *p == 'e' || *p == 'E' ) ) {
	CONST char* stringSave = p;
	++p;
	c = *p;
	if ( isdigit( c ) || c == '+' || c == '-' ) {
	    errno = 0;
	    exponent = strtol( p, (char**)&p, 10 );
	    if ( errno == ERANGE ) {
		if ( exponent > 0 ) {
		    v = HUGE_VAL;
		} else {
		    v = 0.0;
		}
		*endPtr = p;
		goto returnValue;
	    }
	}
	if ( p == stringSave + 1 ) {
	    p = stringSave;
	    exponent = 0;
	}
    }
    exponent = exponent + nTrailZero - nDigitsAfterDp;

    /*
     * If we come here with no significant digits, we might still be
     * looking at Inf or NaN.  Go parse them.
     */

    if ( !seenDigit ) {

	/* Test for Inf */

	if ( c == 'I' || c == 'i' ) {

	    if ( ( p[1] == 'N' || p[1] == 'n' )
		 && ( p[2] == 'F' || p[2] == 'f' ) ) {
		p += 3;
		if ( ( p[0] == 'I' || p[0] == 'i' )
		     && ( p[1] == 'N' || p[1] == 'n' )
		     && ( p[2] == 'I' || p[2] == 'i' )
		     && ( p[3] == 'T' || p[3] == 't' )
		     && ( p[4] == 'Y' || p[1] == 'y' ) ) {
		    p += 5;
		}
		errno = ERANGE;
		v = HUGE_VAL;
		if ( endPtr != NULL ) {
		    *endPtr = p;
		}
		goto returnValue;
	    }


#ifdef IEEE_FLOATING_POINT

	    /* IEEE floating point supports NaN */

	} else if ( (c == 'N' || c == 'n' )
		    && ( sizeof(Tcl_WideUInt) == sizeof( double ) ) ) {
	    
	    if ( ( p[1] == 'A' || p[1] == 'a' )
		 && ( p[2] == 'N' || p[2] == 'n' ) ) {
		p += 3;
		
	        if ( endPtr != NULL ) {
		    *endPtr = p;
		}
		
		/* Restore FPU mode word */

#if defined(__GNUC__) && defined(__i386)
		_FPU_SETCW( oldRoundingMode );
#endif
		return ParseNaN( signum, endPtr );

	    }
#endif

	}

	goto error;
    }

    /*
     * We've successfully scanned; update the end-of-element pointer.
     */

    if ( endPtr != NULL ) {
	*endPtr = p;
    }

    /* Test for zero. */

    if ( nSigDigs == 0 ) {
	v = 0.0;
	goto returnValue;
    }

    /*
     * The easy cases are where we have an exact significand and
     * the exponent is small enough that we can compute the value
     * with only one roundoff.  In addition to the cases where we
     * can multiply or divide an exact-integer significand by an
     * exact-integer power of 10, there is also David Gay's case
     * where we can scale the significand by a power of 10 (still
     * keeping it exact) and then multiply by an exact power of 10.
     * The last case enables combinations like 83e25 that would
     * otherwise require high precision arithmetic.
     */

    if ( nSigDigs <= DBL_DIG ) {
	if ( exponent >= 0 ) {
	    if ( exponent <= mmaxpow ) {
		v = exactSignificand * pow10[ exponent ];
		goto returnValue;
	    } else {
		int diff = DBL_DIG - nSigDigs;
		if ( exponent - diff <= mmaxpow ) {
		    volatile double factor = exactSignificand * pow10[ diff ];
		    v = factor * pow10[ exponent - diff ];
		    goto returnValue;
		}
	    }
	} else {
	    if ( exponent >= -mmaxpow ) {
		v = exactSignificand / pow10[ -exponent ];
		goto returnValue;
	    }
	}
    }

    /* 
     * We don't have one of the easy cases, so we can't compute the
     * scanned number exactly, and have to do it in multiple precision.
     * Begin by testing for obvious overflows and underflows.
     */

    if ( nSigDigs + exponent - 1
	 > DBL_MAX_EXP * log( (double) FLT_RADIX ) / log( 10. ) ) {
	v = HUGE_VAL;
	errno = ERANGE;
	goto returnValue;
    }
    if ( nSigDigs + exponent - 1
	 < floor ( ( DBL_MIN_EXP - DBL_MANT_DIG )
		   * log( (double) FLT_RADIX ) / log( 10. ) ) ) {
	errno = ERANGE;
	v = 0.;
	goto returnValue;
    }

    /*
     * Nothing exceeds the boundaries of the tables, at least.
     * Compute an approximate value for the number, with
     * no possibility of overflow because we manage the exponent
     * separately.
     */

    if ( nSigDigs > DBL_DIG ) {
	expt2 = exponent + nSigDigs - DBL_DIG;
    } else {
	expt2 = exponent;
    }
    v = frexp( exactSignificand, &machexp );
    if ( expt2 > 0 ) {
	v = frexp( v * pow10[ expt2 & 0xf ], &j );
	machexp += j;
	for ( i = 4; i < 9; ++i ) {
	    if ( expt2 & ( 1 << i ) ) {
		v = frexp( v * pow_10_2_n[ i ], &j );
		machexp += j;
	    }
	}
    } else {
	v = frexp( v / pow10[ (-expt2) & 0xf ], &j );
	machexp += j;
	for ( i = 4; i < 9; ++i ) {
	    if ( (-expt2) & ( 1 << i ) ) {
		v = frexp( v / pow_10_2_n[ i ], &j );
		machexp += j;
	    }
	}
    }

    /*
     * A first approximation is that the result will be v * 2 ** machexp.
     * v is greater than or equal to 0.5 and less than 1.
     * If machexp > DBL_MAX_EXP * log2(FLT_RADIX), there is an overflow.
     * If the result of SafeLdExp is zero, there may be an underflow; start
     * from the smallest representible number in that case.
     */

    if ( machexp > DBL_MAX_EXP * log2FLT_RADIX ) {
	v = HUGE_VAL;
	errno = ERANGE;
	goto returnValue;
    }
	
    v = SafeLdExp( v, machexp );
    if ( v == 0.0 ) {
	v = SafeLdExp( 1.0, DBL_MIN_EXP * log2FLT_RADIX - mantBits );
    }

    /* We have a first approximation in v. Now we need to refine it. */

    v = RefineResult( v, startOfSignificand, nSigDigs, exponent );

    /* In a very few cases, a second iteration is needed. e.g., 457e-102 */
    
    v = RefineResult( v, startOfSignificand, nSigDigs, exponent );

    /* Handle underflow */

  returnValue:
    if ( nSigDigs != 0 && v == 0.0 ) {
	errno = ERANGE;
    }

    /* Return a number with correct sign */

    if ( signum ) {
	v = -v;
    }

    /* Restore FPU mode word */
    
#if defined(__GNUC__) && defined(__i386)
    _FPU_SETCW( oldRoundingMode );
#endif

    return v;	
    
    /* Come here on an invalid input */

  error:
    if ( endPtr != NULL ) {
	*endPtr = s;
    }

    /* Restore FPU mode word */
    
#if defined(__GNUC__) && defined(__i386)
    _FPU_SETCW( oldRoundingMode );
#endif
    return 0.0;

}

/*
 *----------------------------------------------------------------------
 *
 * InitializeConstants --
 *
 *	Initializes constants that are needed for string-to-double
 *      conversion.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The log base 2 of the floating point radix, the number of
 *	bits in a double mantissa, and a table of the powers of five
 *	and ten are computed and stored.
 *
 *----------------------------------------------------------------------
 */

static void
InitializeConstants( void )
{
    int i;
    double d;
    if ( !constantsInitialized ) {
	Tcl_MutexLock( &initMutex );
	if ( !constantsInitialized ) {
	    frexp( (double) FLT_RADIX, &log2FLT_RADIX );
	    --log2FLT_RADIX;
	    mantBits = DBL_MANT_DIG * log2FLT_RADIX;
	    d = 1.0;
	    for ( i = 0; i <= MAXPOW; ++i ) {
		pow10[i] = d;
		d *= 10.0;
	    }
	    for ( i = 0; i < 9; ++i ) {
		mp_init( pow5 + i );
	    }
	    mp_set( pow5, 5 );
	    for ( i = 0; i < 8; ++i ) {
		mp_sqr( pow5+i, pow5+i+1 );
	    }
	    Tcl_CreateExitHandler( FreeConstants, (ClientData) NULL );
	}
	constantsInitialized = 1;
	Tcl_MutexUnlock( &initMutex );
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FreeConstants --
 *
 *	Cleans up this file on exit.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Memory allocated by InitializeConstants is freed.
 *
 *----------------------------------------------------------------------
 */

static void
FreeConstants( ClientData unused )
{
    int i;
    Tcl_MutexLock( &initMutex );
    constantsInitialized = 0;
    for ( i = 0; i < 9; ++i ) {
	mp_clear( pow5 + i );
    }
    Tcl_MutexUnlock( &initMutex );
}

/*
 *----------------------------------------------------------------------
 *
 * RefineResult --
 *
 *	Given a poor approximation to a floating point number, returns
 *	a better one (The better approximation is correct to within
 *	1 ulp, and is entirely correct if the poor approximation is
 *	correct to 1 ulp.)
 *
 * Results:
 *	Returns the improved result.
 *
 *----------------------------------------------------------------------
 */

static double
RefineResult( double approxResult, 
				/* Approximate result of conversion */
	      CONST char* sigStart,
				/* Pointer to start of significand in
				 * input string. */
	      int nSigDigs,	/* Number of significant digits */
	      long exponent )	/* Power of ten to multiply by significand */
{

    int M2, M5;			/*  Powers of 2 and of 5 needed to put
				 * the decimal and binary numbers over
				 * a common denominator. */
    double significand;		/* Sigificand of the binary number */
    int binExponent;		/* Exponent of the binary number */

    int msb;			/* Most significant bit position of an
				 * intermediate result */
    int nDigits;		/* Number of mp_digit's in an intermediate
				 * result */
    mp_int twoMv;		/* Approx binary value expressed as an
				 * exact integer scaled by the multiplier 2M */
    mp_int twoMd;		/* Exact decimal value expressed as an
				 * exact integer scaled by the multiplier 2M */
    int scale;			/* Scale factor for M */
    int multiplier;		/* Power of two to scale M */
    double num, den;		/* Numerator and denominator of the
				 * correction term */
    double quot;		/* Correction term */
    double minincr;		/* Lower bound on the absolute value
				 * of the correction term. */
    int i;
    CONST char* p;

    /*
     * Find a common denominator for the decimal and binary fractions.
     * The common denominator will be 2**M2 + 5**M5.
     */

    significand = frexp( approxResult, &binExponent );
    i = mantBits - binExponent;
    if ( i < 0 ) {
	M2 = 0;
    } else {
	M2 = i;
    }
    if ( exponent > 0 ) {
	M5 = 0;
    } else {
	M5 = -exponent;
	if ( (M5-1) > M2 ) {
	    M2 = M5-1;
	}
    }

    /* 
     * The floating point number is significand*2**binExponent.
     * The 2**-1 bit of the significand (the most significant) 
     * corresponds to the 2**(binExponent+M2 + 1) bit of 2*M2*v.
     * Allocate enough digits to hold that quantity, then
     * convert the significand to a large integer, scaled
     * appropriately. Then multiply by the appropriate power of 5.
     */

    msb = binExponent + M2;  /* 1008 */
    nDigits = msb / DIGIT_BIT + 1;
    mp_init_size( &twoMv, nDigits );
    i = ( msb % DIGIT_BIT + 1 ); 
    twoMv.used = nDigits;
    significand *= SafeLdExp( 1.0, i );
    while ( -- nDigits >= 0 ) {
	twoMv.dp[nDigits] = (mp_digit) significand;
	significand -= (mp_digit) significand;
	significand = SafeLdExp( significand, DIGIT_BIT );
    }
    for ( i = 0; i <= 8; ++i ) {
	if ( M5 & ( 1 << i ) ) {
	    mp_mul( &twoMv, pow5+i, &twoMv );
	}
    }
    
    /* 
     * Collect the decimal significand as a high precision integer.
     * The least significant bit corresponds to bit M2+exponent+1
     * so it will need to be shifted left by that many bits after
     * being multiplied by 5**(M5+exponent).
     */

    mp_init( &twoMd ); mp_zero( &twoMd );
    i = nSigDigs;
    for ( p = sigStart ; ; ++p ) {
	char c = *p;
	if ( isdigit( c ) ) {
	    mp_mul_d( &twoMd, (unsigned) 10, &twoMd );
	    mp_add_d( &twoMd, (unsigned) (c - '0'), &twoMd );
	    --i;
	    if ( i == 0 ) break;
	}
    }
    for ( i = 0; i <= 8; ++i ) {
	if ( (M5+exponent) & ( 1 << i ) ) {
	    mp_mul( &twoMd, pow5+i, &twoMd );
	}
    }
    mp_mul_2d( &twoMd, M2+exponent+1, &twoMd );
    mp_sub( &twoMd, &twoMv, &twoMd );

    /*
     * The result, 2Mv-2Md, needs to be divided by 2M to yield a correction
     * term. Because 2M may well overflow a double, we need to scale the
     * denominator by a factor of 2**binExponent-mantBits
     */

    scale = binExponent - mantBits - 1;

    mp_set( &twoMv, 1 );
    for ( i = 0; i <= 8; ++i ) {
	if ( M5 & ( 1 << i ) ) {
	    mp_mul( &twoMv, pow5+i, &twoMv );
	}
    }
    multiplier = M2 + scale + 1;
    if ( multiplier > 0 ) {
	mp_mul_2d( &twoMv, multiplier, &twoMv );
    } else if ( multiplier < 0 ) {
	mp_div_2d( &twoMv, -multiplier, &twoMv, NULL );
    }

    /*
     * If the result is less than unity, the error is less than 1/2 unit
     * in the last place, so there's no correction to make.
     */

    if ( mp_cmp_mag( &twoMd, &twoMv ) == MP_LT ) {
	return approxResult;
    }

    /* 
     * Convert the numerator and denominator of the corrector term
     * accurately to floating point numbers.
     */

    num = BignumToDouble( &twoMd );
    den = BignumToDouble( &twoMv );

    quot = SafeLdExp( num/den, scale );
    minincr = SafeLdExp( 1.0, binExponent - mantBits );

    if ( quot < 0. && quot > -minincr ) {
	quot = -minincr;
    } else if ( quot > 0. && quot < minincr ) {
	quot = minincr;
    }

    mp_clear( &twoMd );
    mp_clear( &twoMv );

    
    return approxResult + quot;
}

/*
 *----------------------------------------------------------------------
 *
 * BignumToDouble --
 *
 *	Convert an arbitrary-precision integer to a native floating 
 *	point number.
 *
 * Results:
 *	Returns the converted number.  Sets errno to ERANGE if the
 *	number is too large to convert.
 *
 *----------------------------------------------------------------------
 */

static double
BignumToDouble( mp_int* a )
				/* Integer to convert */
{
    mp_int b;
    int bits;
    int shift;
    int i;
    double r;

    /* Determine how many bits we need, and extract that many from 
     * the input. Round to nearest unit in the last place. */

    bits = mp_count_bits( a );
    shift = mantBits + 1 - bits;
    mp_init( &b );
    if ( shift > 0 ) {
	mp_mul_2d( a, shift, &b );
    } else if ( shift < 0 ) {
	mp_div_2d( a, -shift, &b, NULL );
    } else {
	mp_copy( a, &b );
    }
    mp_add_d( &b, 1, &b );
    mp_div_2d( &b, 1, &b, NULL );

    /* Accumulate the result, one mp_digit at a time */

    r = 0.0;
    for ( i = b.used-1; i >= 0; --i ) {
	r = ldexp( r, DIGIT_BIT ) + b.dp[i];
    }
    mp_clear( &b );

    /* 
     * Test for overflow, and scale the result to the correct number 
     * of bits. 
     */

    if ( bits / log2FLT_RADIX > DBL_MAX_EXP ) {
	errno = ERANGE;
	r = HUGE_VAL;
    } else {
	r = ldexp( r, bits - mantBits );
    }

    /* Return the result with the appropriate sign. */

    if ( a->sign == MP_ZPOS ) {
	return r;
    } else {
	return -r;
    }
}		

/*
 *----------------------------------------------------------------------
 *
 * ParseNaN --
 *
 *	Parses a "not a number" from an input string, and returns the
 *	double precision NaN corresponding to it.
 *
 * Side effects:
 *	Advances endPtr to follow any (hex) in the input string.
 *
 *	If the NaN is followed by a left paren, a string of spaes
 *	and hexadecimal digits, and a right paren, endPtr is advanced
 *	to follow it.
 *
 * The string of hexadecimal digits is OR'ed into the resulting
 * NaN, and the signum is set as well.  Note that a signalling NaN
 * is never returned.
 *
 *----------------------------------------------------------------------
 */

double
ParseNaN( int signum,		/* Flag == 1 if minus sign has been
				 * seen in front of NaN */
	  CONST char** endPtr )
				/* Pointer-to-pointer to char following "NaN" 
				 * in the input string */
{
    CONST char* p = *endPtr;
    char c;
    union {
	Tcl_WideUInt iv;
	double dv;
    } theNaN;

    /* Scan off a hex number in parentheses.  Embedded blanks are ok. */

    theNaN.iv = 0;
    if ( *p == '(' ) {
	++p;
	for ( ; ; ) {
	    c = *p++;
	    if ( isspace(c) ) {
		continue;
	    } else if ( c == ')' ) {
		*endPtr = p;
		break;
	    } else if ( isdigit(c) ) {
		c -= '0';
	    } else if ( c >= 'A' && c <= 'F' ) {
		c = c - 'A' + 10;
	    } else if ( c >= 'a' && c <= 'f' ) {
		c = c - 'a' + 10;
	    } else {
		theNaN.iv = ( ((Tcl_WideUInt) 0x7ff8) << 48 )
		    | ( ((Tcl_WideUInt) signum) << 63 );
		return theNaN.dv;
	    }
	    theNaN.iv = (theNaN.iv << 4) | c;
	}
    }

    /* 
     * Mask the hex number down to the least significant 51 bits.
     *
     * If the result is zero, make it 1 so that we don't return Inf
     * instead of NaN 
     */

    theNaN.iv &= ( ((Tcl_WideUInt) 1) << 51 ) - 1;
    if ( theNaN.iv == 0 ) {
	theNaN.iv = 1;
    }
    if ( signum ) {
	theNaN.iv |= ((Tcl_WideUInt) 0xfff8) << 48;
    } else {
	theNaN.iv |= ((Tcl_WideUInt) 0x7ff8) << 48;
    }

    *endPtr = p;
    return theNaN.dv;
}

/*
 *----------------------------------------------------------------------
 *
 * SafeLdExp --
 *
 *	Do an 'ldexp' operation, but handle denormals gracefully.
 *
 * Results:
 *	Returns the appropriately scaled value.
 *
 * On some platforms, 'ldexp' fails when presented with a number
 * too small to represent as a normalized double.  This routine
 * does 'ldexp' in two steps for those numbers, to return correctly
 * denormalized values.
 *
 *----------------------------------------------------------------------
 */

static double
SafeLdExp( double fract, int expt )
{
    int minexpt = DBL_MIN_EXP * log2FLT_RADIX;
    volatile double retval;
    if ( expt < minexpt ) {
	retval = ldexp( fract, expt - mantBits - minexpt );
	retval *= ldexp( 1.0, mantBits + minexpt );
    } else {
	retval = ldexp( fract, expt );
    }
    return retval;
}
