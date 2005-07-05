/*
 *----------------------------------------------------------------------
 *
 * tclStrToD.c --
 *
 *	This file contains a TclStrToD procedure that handles conversion of
 *	string to double, with correct rounding even where extended precision
 *	is needed to achieve that.  It also contains a TclDoubleDigits
 *	procedure that handles conversion of double to string (at least the
 *	significand), and several utility functions for interconverting
 *	'double' and the integer types.
 *
 * Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclStrToD.c,v 1.4.2.3 2005/07/05 15:09:08 dgp Exp $
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
 * environments that include no UNIX, i.e. no errno: just arrange to use the
 * errno from tclExecute.c here.
 */

#ifdef TCL_GENERIC_ONLY
#   define NO_ERRNO_H
#endif

#ifdef NO_ERRNO_H
extern int errno;			/* Use errno from tclExecute.c. */
#   define ERANGE 34
#endif

#if (FLT_RADIX == 2) && (DBL_MANT_DIG == 53) && (DBL_MAX_EXP == 1024)
#   define IEEE_FLOATING_POINT
#endif

/*
 * gcc on x86 needs access to rounding controls.  It is tempting to include
 * fpu_control.h, but that file exists only on Linux; it is missing on Cygwin
 * and MinGW. Most gcc-isms and ix86-isms are factored out here.
 */

#if defined(__GNUC__) && defined(__i386)
typedef unsigned int fpu_control_t __attribute__ ((__mode__ (__HI__)));
#   define _FPU_GETCW(cw)	__asm__ ("fnstcw %0" : "=m" (*&cw))
#   define _FPU_SETCW(cw)	__asm__ ("fldcw %0" : : "m" (*&cw))
#   define FPU_IEEE_ROUNDING	0x027f
#   define ADJUST_FPU_CONTROL_WORD
#endif

/*
 * HP's PA_RISC architecture uses 7ff4000000000000 to represent a quiet NaN.
 * Everyone else uses 7ff8000000000000.  (Why, HP, why?)
 */

#ifdef __hppa
#   define NAN_START 0x7ff4
#   define NAN_MASK (((Tcl_WideUInt) 1) << 50)
#else
#   define NAN_START 0x7ff8
#   define NAN_MASK (((Tcl_WideUInt) 1) << 51)
#endif

/*
 * There now follows a lot of static variables that are shared across all
 * threads but which are not guarded by mutexes. This is OK, because they are
 * only ever assigned _ONCE_ during Tcl's library initialization sequence.
 */

static const double pow_10_2_n[] = {	/* Inexact higher powers of ten */
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
#define MAXPOW	22		/* Num of exactly representable powers of 10 */
static double pow10[MAXPOW+1];	/* The powers of ten that can be represented
				 * exactly as IEEE754 doubles. */
static int mmaxpow;		/* Largest power of ten that can be
				 * represented exactly in a 'double'. */
static int log2FLT_RADIX;	/* Logarithm of the floating point radix. */
static int mantBits;		/* Number of bits in a double's significand. */
static mp_int pow5[9];		/* Table of powers of 5**(2**n), up to
				 * 5**256. */
static double tiny;		/* The smallest representable double. */
static int maxDigits;		/* The maximum number of digits to the left of
				 * the decimal point of a double. */
static int minDigits;		/* The maximum number of digits to the right
				 * of the decimal point in a double. */
static int mantDIGIT;		/* Number of mp_digit's needed to hold the
				 * significand of a double. */

/* Static functions defined in this file */

static double	RefineResult(double approx, CONST char *start, int nDigits,
		    long exponent);
static double	ParseNaN(int signum, CONST char **end);
static double	SafeLdExp(double fraction, int exponent);

/*
 *----------------------------------------------------------------------
 *
 * TclStrToD --
 *
 *	Scans a double from a string.
 *
 * Results:
 *	Returns the scanned number. In the case of underflow, returns an
 *	appropriately signed zero; in the case of overflow, returns an
 *	appropriately signed HUGE_VAL.
 *
 * Side effects:
 *	Stores a pointer to the end of the scanned number in '*endPtr', if
 *	endPtr is not NULL. If '*endPtr' is equal to 's' on return from this
 *	function, it indicates that the input string could not be recognized
 *	as a number.  In the case of underflow or overflow, 'errno' is set to
 *	ERANGE.
 *
 *------------------------------------------------------------------------
 */

double
TclStrToD(CONST char *s,	/* String to scan. */
	  CONST char **endPtr)	/* Pointer to the end of the scanned number. */
{
    const char *p = s;
    const char *startOfSignificand = NULL;
				/* Start of the significand in the string. */
    int signum = 0;		/* Sign of the significand. */
    double exactSignificand = 0.0;
				/* Significand, represented exactly as a
				 * floating-point number. */
    int seenDigit = 0;		/* Flag == 1 if a digit has been seen. */
    int nSigDigs = 0;		/* Number of significant digits presented. */
    int nDigitsAfterDp = 0;	/* Number of digits after the decimal point. */
    int nTrailZero = 0;		/* Number of trailing zeros in the
				 * significand. */
    long exponent = 0;		/* Exponent. */
    int seenDp = 0;		/* Flag == 1 if decimal point has been seen. */
    char c;			/* One character extracted from the input. */
    volatile double v;		/* Scanned value; must be 'volatile double' on
				 * gc-ix86 to force correct rounding to IEEE
				 * double and not Intel double-extended. */
    int machexp;		/* Exponent of the machine rep of the scanned
				 * value. */
    int expt2;			/* Exponent for computing first approximation
				 * to the true value. */
    int i, j;

    /*
     * With gcc on x86, the floating point rounding mode is double-extended.
     * This causes the result of double-precision calculations to be rounded
     * twice: once to the precision of double-extended and then again to the
     * precision of double.  Double-rounding introduces gratuitous errors of
     * one ulp, so we need to change rounding mode to 53-bits.
     */

#ifdef ADJUST_FPU_CONTROL_WORD
    fpu_control_t roundTo53Bits = FPU_IEEE_ROUNDING;
    fpu_control_t oldRoundingMode;
    _FPU_GETCW(oldRoundingMode);
    _FPU_SETCW(roundTo53Bits);
#   define RestoreRoundingMode()	_FPU_SETCW(oldRoundingMode)
#else
#   define RestoreRoundingMode()	(void) 0 /* Do nothing */
#endif

    /*
     * Discard leading whitespace from input.
     */

    while (isspace(UCHAR(*p))) {
	++p;
    }

    /*
     * Determine the sign of the significand.
     */

    switch (*p) {
	case '-':
	    signum = 1;
	    /* FALLTHROUGH */
	case '+':
	    ++p;
    }

    /*
     * Discard leading zeroes from input.
     */

    while (*p == '0') {
	seenDigit = 1;
	++p;
    }

    /*
     * Scan digits from the significand. Simultaneously, keep track of the
     * number of digits after the decimal point. Maintain a pointer to the
     * start of the significand. Keep "exactSignificand" equal to the
     * conversion of the DBL_DIG most significant digits.
     */

    for (;;) {
	c = *p;
	if (c == '.' && !seenDp) {
	    seenDp = 1;
	    ++p;
	} else if (isdigit(UCHAR(c))) {
	    if (c == '0') {
		if (startOfSignificand != NULL) {
		    ++nTrailZero;
		}
	    } else {
		if (startOfSignificand == NULL) {
		    startOfSignificand = p;
		} else if (nTrailZero) {
		    if (nTrailZero + nSigDigs < DBL_DIG) {
			exactSignificand *= pow10[nTrailZero];
		    } else if (nSigDigs < DBL_DIG) {
			exactSignificand *= pow10[DBL_DIG - nSigDigs];
		    }
		    nSigDigs += nTrailZero;
		}
		if (nSigDigs < DBL_DIG) {
		    exactSignificand = 10. * exactSignificand + (c - '0');
		}
		++nSigDigs;
		nTrailZero = 0;
	    }
	    if (seenDp) {
		++nDigitsAfterDp;
	    }
	    seenDigit = 1;
	    ++p;
	} else {
	    break;
	}
    }

    /*
     * At this point, we've scanned the significand, and p points to the
     * character beyond it. "startOfSignificand" is the first non-zero
     * character in the significand. "nSigDigs" is the number of significant
     * digits of the significand, not including any trailing zeroes.
     * "exactSignificand" is a floating point number that represents, without
     * loss of precision, the first min(DBL_DIG,n) digits of the significand.
     * "nDigitsAfterDp" is the number of digits after the decimal point, again
     * excluding trailing zeroes.
     *
     * Now scan 'E' format
     */

    exponent = 0;
    if (seenDigit && (*p == 'e' || *p == 'E')) {
	const char* stringSave = p;
	++p;
	c = *p;
	if (isdigit(UCHAR(c)) || c == '+' || c == '-') {
	    errno = 0;
	    exponent = strtol(p, (char**)&p, 10);
	    if (errno == ERANGE) {
		if (exponent > 0) {
		    v = HUGE_VAL;
		} else {
		    v = 0.0;
		}
		*endPtr = p;
		goto returnValue;
	    }
	}
	if (p == stringSave+1) {
	    p = stringSave;
	    exponent = 0;
	}
    }
    exponent += nTrailZero - nDigitsAfterDp;

    /*
     * If we come here with no significant digits, we might still be looking
     * at Inf or NaN.  Go parse them.
     */

    if (!seenDigit) {
	/*
	 * Test for Inf or Infinity (in any case).
	 */

	if (c == 'I' || c == 'i') {
	    if ((p[1] == 'N' || p[1] == 'n')
		    && (p[2] == 'F' || p[2] == 'f')) {
		p += 3;
		if ((p[0] == 'I' || p[0] == 'i')
			&& (p[1] == 'N' || p[1] == 'n')
			&& (p[2] == 'I' || p[2] == 'i')
			&& (p[3] == 'T' || p[3] == 't')
			&& (p[4] == 'Y' || p[1] == 'y')) {
		    p += 5;
		}
		errno = ERANGE;
		v = HUGE_VAL;
		if (endPtr != NULL) {
		    *endPtr = p;
		}
		goto returnValue;
	    }

#ifdef IEEE_FLOATING_POINT
	    /*
	     * Only IEEE floating point supports NaN
	     */
	} else if ((c == 'N' || c == 'n')
		&& (sizeof(Tcl_WideUInt) == sizeof(double))) {
	    if ((p[1] == 'A' || p[1] == 'a')
		    && (p[2] == 'N' || p[2] == 'n')) {
		p += 3;

		if (endPtr != NULL) {
		    *endPtr = p;
		}

		/*
		 * Restore FPU mode word.
		 */

		RestoreRoundingMode();
		return ParseNaN(signum, endPtr);
	    }
#endif
	}

	goto error;
    }

    /*
     * We've successfully scanned; update the end-of-element pointer.
     */

    if (endPtr != NULL) {
	*endPtr = p;
    }

    /*
     * Test for zero.
     */

    if (nSigDigs == 0) {
	v = 0.0;
	goto returnValue;
    }

    /*
     * The easy cases are where we have an exact significand and the exponent
     * is small enough that we can compute the value with only one roundoff.
     * In addition to the cases where we can multiply or divide an
     * exact-integer significand by an exact-integer power of 10, there is
     * also David Gay's case where we can scale the significand by a power of
     * 10 (still keeping it exact) and then multiply by an exact power of 10.
     * The last case enables combinations like 83e25 that would otherwise
     * require high precision arithmetic.
     */

    if (nSigDigs <= DBL_DIG) {
	if (exponent >= 0) {
	    if (exponent <= mmaxpow) {
		v = exactSignificand * pow10[exponent];
		goto returnValue;
	    } else {
		int diff = DBL_DIG - nSigDigs;
		if (exponent - diff <= mmaxpow) {
		    volatile double factor = exactSignificand * pow10[diff];
		    v = factor * pow10[exponent - diff];
		    goto returnValue;
		}
	    }
	} else if (exponent >= -mmaxpow) {
	    v = exactSignificand / pow10[-exponent];
	    goto returnValue;
	}
    }

    /*
     * We don't have one of the easy cases, so we can't compute the scanned
     * number exactly, and have to do it in multiple precision.  Begin by
     * testing for obvious overflows and underflows.
     */

    if (nSigDigs + exponent - 1 > maxDigits) {
	v = HUGE_VAL;
	errno = ERANGE;
	goto returnValue;
    }
    if (nSigDigs + exponent - 1 < minDigits) {
	errno = ERANGE;
	v = 0.;
	goto returnValue;
    }

    /*
     * Nothing exceeds the boundaries of the tables, at least.  Compute an
     * approximate value for the number, with no possibility of overflow
     * because we manage the exponent separately.
     */

    if (nSigDigs > DBL_DIG) {
	expt2 = exponent + nSigDigs - DBL_DIG;
    } else {
	expt2 = exponent;
    }
    v = frexp(exactSignificand, &machexp);
    if (expt2 > 0) {
	v = frexp(v * pow10[expt2 & 0xf], &j);
	machexp += j;
	for (i=4 ; i<9 ; ++i) {
	    if (expt2 & (1 << i)) {
		v = frexp(v * pow_10_2_n[i], &j);
		machexp += j;
	    }
	}
    } else {
	v = frexp(v / pow10[(-expt2) & 0xf], &j);
	machexp += j;
	for (i=4 ; i<9 ; ++i) {
	    if ((-expt2) & (1 << i)) {
		v = frexp(v / pow_10_2_n[i], &j);
		machexp += j;
	    }
	}
    }

    /*
     * A first approximation is that the result will be v * 2 ** machexp. v is
     * greater than or equal to 0.5 and less than 1. If machexp >
     * DBL_MAX_EXP*log2(FLT_RADIX), there is an overflow. Constrain the result
     * to the smallest representible number to avoid premature underflow.
     */

    if (machexp > DBL_MAX_EXP * log2FLT_RADIX) {
	v = HUGE_VAL;
	errno = ERANGE;
	goto returnValue;
    }

    v = SafeLdExp(v, machexp);
    if (v < tiny) {
	v = tiny;
    }

    /*
     * We have a first approximation in v. Now we need to refine it.
     */

    v = RefineResult(v, startOfSignificand, nSigDigs, exponent);

    /*
     * In a very few cases, a second iteration is needed. e.g., 457e-102
     */

    v = RefineResult(v, startOfSignificand, nSigDigs, exponent);

    /*
     * Handle underflow.
     */

  returnValue:
    if (nSigDigs != 0 && v == 0.0) {
	errno = ERANGE;
    }

    /*
     * Return a number with correct sign.
     */

    if (signum) {
	v = -v;
    }

    /*
     * Restore FPU mode word and return.
     */

    RestoreRoundingMode();
    return v;

    /*
     * Come here on an invalid input.
     */

  error:
    if (endPtr != NULL) {
	*endPtr = s;
    }

    /*
     * Restore FPU mode word and return.
     */

    RestoreRoundingMode();
    return 0.0;
}

/*
 *----------------------------------------------------------------------
 *
 * RefineResult --
 *
 *	Given a poor approximation to a floating point number, returns a
 *	better one. (The better approximation is correct to within 1 ulp, and
 *	is entirely correct if the poor approximation is correct to 1 ulp.)
 *
 * Results:
 *	Returns the improved result.
 *
 *----------------------------------------------------------------------
 */

static double
RefineResult(double approxResult, /* Approximate result of conversion. */
	     CONST char* sigStart,
				/* Pointer to start of significand in input
				 * string. */
	     int nSigDigs,	/* Number of significant digits. */
	     long exponent)	/* Power of ten to multiply by significand. */
{
    int M2, M5;			/* Powers of 2 and of 5 needed to put the
				 * decimal and binary numbers over a common
				 * denominator. */
    double significand;		/* Sigificand of the binary number. */
    int binExponent;		/* Exponent of the binary number. */
    int msb;			/* Most significant bit position of an
				 * intermediate result. */
    int nDigits;		/* Number of mp_digit's in an intermediate
				 * result. */
    mp_int twoMv;		/* Approx binary value expressed as an exact
				 * integer scaled by the multiplier 2M. */
    mp_int twoMd;		/* Exact decimal value expressed as an exact
				 * integer scaled by the multiplier 2M. */
    int scale;			/* Scale factor for M. */
    int multiplier;		/* Power of two to scale M. */
    double num, den;		/* Numerator and denominator of the correction
				 * term. */
    double quot;		/* Correction term. */
    double minincr;		/* Lower bound on the absolute value of the
				 * correction term. */
    int i;
    const char* p;

    /*
     * The first approximation is always low. If we find that it's HUGE_VAL,
     * we're done.
     */

    if (approxResult == HUGE_VAL) {
	return approxResult;
    }

    /*
     * Find a common denominator for the decimal and binary fractions. The
     * common denominator will be 2**M2 + 5**M5.
     */

    significand = frexp(approxResult, &binExponent);
    i = mantBits - binExponent;
    if (i < 0) {
	M2 = 0;
    } else {
	M2 = i;
    }
    if (exponent > 0) {
	M5 = 0;
    } else {
	M5 = -exponent;
	if ((M5-1) > M2) {
	    M2 = M5-1;
	}
    }

    /*
     * The floating point number is significand*2**binExponent. The 2**-1 bit
     * of the significand (the most significant) corresponds to the
     * 2**(binExponent+M2 + 1) bit of 2*M2*v. Allocate enough digits to hold
     * that quantity, then convert the significand to a large integer, scaled
     * appropriately. Then multiply by the appropriate power of 5.
     */

    msb = binExponent + M2;	/* 1008 */
    nDigits = msb / DIGIT_BIT + 1;
    mp_init_size(&twoMv, nDigits);
    i = (msb % DIGIT_BIT + 1);
    twoMv.used = nDigits;
    significand *= SafeLdExp(1.0, i);
    while (--nDigits >= 0) {
	twoMv.dp[nDigits] = (mp_digit) significand;
	significand -= (mp_digit) significand;
	significand = SafeLdExp(significand, DIGIT_BIT);
    }
    for (i=0 ; i<=8 ; ++i) {
	if (M5 & (1 << i)) {
	    mp_mul(&twoMv, pow5+i, &twoMv);
	}
    }

    /*
     * Collect the decimal significand as a high precision integer. The least
     * significant bit corresponds to bit M2+exponent+1 so it will need to be
     * shifted left by that many bits after being multiplied by
     * 5**(M5+exponent).
     */

    mp_init(&twoMd);
    mp_zero(&twoMd);
    i = nSigDigs;
    for (p=sigStart ;; ++p) {
	char c = *p;
	if (isdigit(UCHAR(c))) {
	    mp_mul_d(&twoMd, (unsigned) 10, &twoMd);
	    mp_add_d(&twoMd, (unsigned) (c - '0'), &twoMd);
	    --i;
	    if (i == 0) {
		break;
	    }
	}
    }
    for (i=0 ; i<=8 ; ++i) {
	if ((M5+exponent) & (1 << i)) {
	    mp_mul(&twoMd, pow5+i, &twoMd);
	}
    }
    mp_mul_2d(&twoMd, M2+exponent+1, &twoMd);
    mp_sub(&twoMd, &twoMv, &twoMd);

    /*
     * The result, 2Mv-2Md, needs to be divided by 2M to yield a correction
     * term. Because 2M may well overflow a double, we need to scale the
     * denominator by a factor of 2**binExponent-mantBits
     */

    scale = binExponent - mantBits - 1;

    mp_set(&twoMv, 1);
    for (i=0 ; i<=8 ; ++i) {
	if (M5 & (1 << i)) {
	    mp_mul(&twoMv, pow5+i, &twoMv);
	}
    }
    multiplier = M2 + scale + 1;
    if (multiplier > 0) {
	mp_mul_2d(&twoMv, multiplier, &twoMv);
    } else if (multiplier < 0) {
	mp_div_2d(&twoMv, -multiplier, &twoMv, NULL);
    }

    /*
     * If the result is less than unity, the error is less than 1/2 unit in
     * the last place, so there's no correction to make.
     */

    if (mp_cmp_mag(&twoMd, &twoMv) == MP_LT) {
	return approxResult;
    }

    /*
     * Convert the numerator and denominator of the corrector term accurately
     * to floating point numbers.
     */

    num = TclBignumToDouble(&twoMd);
    den = TclBignumToDouble(&twoMv);

    quot = SafeLdExp(num/den, scale);
    minincr = SafeLdExp(1.0, binExponent - mantBits);

    if (quot<0. && quot>-minincr) {
	quot = -minincr;
    } else if (quot>0. && quot<minincr) {
	quot = minincr;
    }

    mp_clear(&twoMd);
    mp_clear(&twoMv);

    return approxResult + quot;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseNaN --
 *
 *	Parses a "not a number" from an input string, and returns the double
 *	precision NaN corresponding to it.
 *
 * Side effects:
 *	Advances endPtr to follow any (hex) in the input string.
 *
 *	If the NaN is followed by a left paren, a string of spaes and
 *	hexadecimal digits, and a right paren, endPtr is advanced to follow
 *	it.
 *
 *	The string of hexadecimal digits is OR'ed into the resulting NaN, and
 *	the signum is set as well.  Note that a signalling NaN is never
 *	returned.
 *
 *----------------------------------------------------------------------
 */

static double
ParseNaN(int signum,		/* Flag == 1 if minus sign has been seen in
				 * front of NaN. */
	 CONST char** endPtr)	/* Pointer-to-pointer to char following "NaN"
				 * in the input string. */
{
    const char* p = *endPtr;
    char c;
    union {
	Tcl_WideUInt iv;
	double dv;
    } theNaN;

    /*
     * Scan off a hex number in parentheses.  Embedded blanks are ok.
     */

    theNaN.iv = 0;
    if (*p == '(') {
	++p;
	for (;;) {
	    c = *p++;
	    if (isspace(UCHAR(c))) {
		continue;
	    } else if (c == ')') {
		*endPtr = p;
		break;
	    } else if (isdigit(UCHAR(c))) {
		c -= '0';
	    } else if (c >= 'A' && c <= 'F') {
		c -= 'A' + 10;
	    } else if (c >= 'a' && c <= 'f') {
		c -= 'a' + 10;
	    } else {
		theNaN.iv = (((Tcl_WideUInt) NAN_START) << 48)
			| (((Tcl_WideUInt) signum) << 63);
		return theNaN.dv;
	    }
	    theNaN.iv = (theNaN.iv << 4) | c;
	}
    }

    /*
     * Mask the hex number down to the least significant 51 bits.
     */

    theNaN.iv &= (((Tcl_WideUInt) 1) << 51) - 1;
    if (signum) {
	theNaN.iv |= ((Tcl_WideUInt) 0xfff8) << 48;
    } else {
	theNaN.iv |= ((Tcl_WideUInt) NAN_START) << 48;
    }

    *endPtr = p;
    return theNaN.dv;
}

/*
 *----------------------------------------------------------------------
 *
 * TclDoubleDigits --
 *
 *	Converts a double to a string of digits.
 *
 * Results:
 *	Returns the position of the character in the string after which the
 *	decimal point should appear.  Since the string contains only
 *	significant digits, the position may be less than zero or greater than
 *	the length of the string.
 *
 * Side effects:
 *	Stores the digits in the given buffer and sets 'signum' according to
 *	the sign of the number.
 *
 *----------------------------------------------------------------------
 */

int
TclDoubleDigits(char * strPtr,	/* Buffer in which to store the result, must
				 * have at least 18 chars. */
		double v,	/* Number to convert. Must be finite, and not
				 * NaN. */
		int *signum)	/* Output: 1 if the number is negative.
				 * Should handle -0 correctly on the IEEE
				 * architecture. */
{
    double f;			/* Significand of v. */
    int e;			/* Power of FLT_RADIX that satisfies
				 * v = f * FLT_RADIX**e */
    int lowOK, highOK;
    mp_int r;			/* Scaled significand. */
    mp_int s;			/* Divisor such that v = r / s */
    mp_int mplus;		/* Scaled epsilon: (r + 2* mplus) == v(+)
				 * where v(+) is the floating point successor
				 * of v. */
    mp_int mminus;		/* Scaled epsilon: (r - 2*mminus) == v(-)
				 * where v(-) is the floating point
				 * predecessor of v. */
    mp_int temp;
    int rfac2 = 0;		/* Powers of 2 and 5 by which large */
    int rfac5 = 0;		/* integers should be scaled.	    */
    int sfac2 = 0;
    int sfac5 = 0;
    int mplusfac2 = 0;
    int mminusfac2 = 0;
    double a;
    char c;
    int i, k, n;

    /*
     * Take the absolute value of the number, and report the number's sign.
     * Take special steps to preserve signed zeroes in IEEE floating point.
     * (We can't use fpclassify, because that's a C9x feature and we still
     * have to build on C89 compilers.)
     */

#ifndef IEEE_FLOATING_POINT
    if (v >= 0.0) {
	*signum = 0;
    } else {
	*signum = 1;
	v = -v;
    }
#else
    union {
	Tcl_WideUInt iv;
	double dv;
    } bitwhack;
    bitwhack.dv = v;
    if (bitwhack.iv & ((Tcl_WideUInt) 1 << 63)) {
	*signum = 1;
	bitwhack.iv &= ~((Tcl_WideUInt) 1 << 63);
	v = bitwhack.dv;
    } else {
	*signum = 0;
    }
#endif

    /*
     * Handle zero specially.
     */

    if (v == 0.0) {
	*strPtr++ = '0';
	*strPtr++ = '\0';
	return 1;
    }

    /*
     * Develop f and e such that v = f * FLT_RADIX**e, with
     * 1.0/FLT_RADIX <= f < 1.
     */

    f = frexp(v, &e);
    n = e % log2FLT_RADIX;
    if (n > 0) {
	n -= log2FLT_RADIX;
	e += 1;
    }
    f *= ldexp(1.0, n);
    e = (e - n) / log2FLT_RADIX;
    if (f == 1.0) {
	f = 1.0 / FLT_RADIX;
	e += 1;
    }

    /*
     * If the original number was denormalized, adjust e and f to be denormal
     * as well.
     */

    if (e < DBL_MIN_EXP) {
	n = mantBits + (e - DBL_MIN_EXP)*log2FLT_RADIX;
	f = ldexp(f, (e - DBL_MIN_EXP)*log2FLT_RADIX);
	e = DBL_MIN_EXP;
	n = (n + DIGIT_BIT - 1) / DIGIT_BIT;
    } else {
	n = mantDIGIT;
    }

    /*
     * Now extract the base-2**DIGIT_BIT digits of f into a multi-precision
     * integer r. Preserve the invariant v = r * 2**rfac2 * FLT_RADIX**e by
     * adjusting e.
     */

    a = f;
    n = mantDIGIT;
    mp_init_size(&r, n);
    r.used = n;
    r.sign = MP_ZPOS;
    i = (mantBits % DIGIT_BIT);
    if (i == 0) {
	i = DIGIT_BIT;
    }
    while (n > 0) {
	a *= ldexp(1.0, i);
	i = DIGIT_BIT;
	r.dp[--n] = (mp_digit) a;
	a -= (mp_digit) a;
    }
    e -= DBL_MANT_DIG;

    lowOK = highOK = (mp_iseven(&r));

    /*
     * We are going to want to develop integers r, s, mplus, and mminus such
     * that v = r / s, v(+)-v / 2 = mplus / s; v-v(-) / 2 = mminus / s and
     * then scale either s or r, mplus, mminus by an appropriate power of ten.
     *
     * We actually do this by keeping track of the powers of 2 and 5 by which
     * f is multiplied to yield v and by which 1 is multiplied to yield s,
     * mplus, and mminus.
     */

    if (e >= 0) {
	int bits = e * log2FLT_RADIX;

	if (f != 1.0/FLT_RADIX) {
	    /*
	     * Normal case, m+ and m- are both FLT_RADIX**e
	     */

	    rfac2 += bits + 1;
	    sfac2 = 1;
	    mplusfac2 = bits;
	    mminusfac2 = bits;
	} else {
	    /*
	     * If f is equal to the smallest significand, then we need another
	     * factor of FLT_RADIX in s to cope with stepping to the next
	     * smaller exponent when going to e's predecessor.
	     */

	    rfac2 += bits + log2FLT_RADIX - 1;
	    sfac2 = 1 + log2FLT_RADIX;
	    mplusfac2 = bits + log2FLT_RADIX;
	    mminusfac2 = bits;
	}
    } else {
	/*
	 * v has digits after the binary point
	 */

	if (e <= DBL_MIN_EXP-DBL_MANT_DIG || f != 1.0/FLT_RADIX) {
	    /*
	     * Either f isn't the smallest significand or e is the smallest
	     * exponent.  mplus and mminus will both be 1.
	     */

	    rfac2 += 1;
	    sfac2 = 1 - e * log2FLT_RADIX;
	    mplusfac2 = 0;
	    mminusfac2 = 0;
	} else {
	    /*
	     * f is the smallest significand, but e is not the smallest
	     * exponent. We need to scale by FLT_RADIX again to cope with the
	     * fact that v's predecessor has a smaller exponent.
	     */

	    rfac2 += 1 + log2FLT_RADIX;
	    sfac2 = 1 + log2FLT_RADIX * (1 - e);
	    mplusfac2 = FLT_RADIX;
	    mminusfac2 = 0;
	}
    }

    /*
     * Estimate the highest power of ten that will be needed to hold the
     * result.
     */

    k = (int) ceil(log(v) / log(10.));
    if (k >= 0) {
	sfac2 += k;
	sfac5 = k;
    } else {
	rfac2 -= k;
	mplusfac2 -= k;
	mminusfac2 -= k;
	rfac5 = -k;
    }

    /*
     * Scale r, s, mplus, mminus by the appropriate powers of 2 and 5.
     */

    mp_init_set(&mplus, 1);
    for (i=0 ; i<=8 ; ++i) {
	if (rfac5 & (1 << i)) {
	    mp_mul(&mplus, pow5+i, &mplus);
	}
    }
    mp_mul(&r, &mplus, &r);
    mp_mul_2d(&r, rfac2, &r);
    mp_init_copy(&mminus, &mplus);
    mp_mul_2d(&mplus, mplusfac2, &mplus);
    mp_mul_2d(&mminus, mminusfac2, &mminus);
    mp_init_set(&s, 1);
    for (i=0 ; i<=8 ; ++i) {
	if (sfac5 & (1 << i)) {
	    mp_mul(&s, pow5+i, &s);
	}
    }
    mp_mul_2d(&s, sfac2, &s);

    /*
     * It is possible for k to be off by one because we used an inexact
     * logarithm.
     */

    mp_init(&temp);
    mp_add(&r, &mplus, &temp);
    i = mp_cmp_mag(&temp, &s);
    if (i>0 || (highOK && i==0)) {
	mp_mul_d(&s, 10, &s);
	++k;
    } else {
	mp_mul_d(&temp, 10, &temp);
	i = mp_cmp_mag(&temp, &s);
	if (i<0 || (highOK && i==0)) {
	    mp_mul_d(&r, 10, &r);
	    mp_mul_d(&mplus, 10, &mplus);
	    mp_mul_d(&mminus, 10, &mminus);
	    --k;
	}
    }

    /*
     * At this point, k contains the power of ten by which we're scaling the
     * result. r/s is at least 1/10 and strictly less than ten, and v = r/s *
     * 10**k.  mplus and mminus give the rounding limits.
     */

    for (;;) {
	int tc1, tc2;

	mp_mul_d(&r, 10, &r);
	mp_div(&r, &s, &temp, &r);	/* temp = 10r / s; r = 10r mod s */
	i = temp.dp[0];
	mp_mul_d(&mplus, 10, &mplus);
	mp_mul_d(&mminus, 10, &mminus);
	tc1 = mp_cmp_mag(&r, &mminus);
	if (lowOK) {
	    tc1 = (tc1 <= 0);
	} else {
	    tc1 = (tc1 < 0);
	}
	mp_add(&r, &mplus, &temp);
	tc2 = mp_cmp_mag(&temp, &s);
	if (highOK) {
	    tc2 = (tc2 >= 0);
	} else {
	    tc2= (tc2 > 0);
	}
	if (!tc1) {
	    if (!tc2) {
		*strPtr++ = '0' + i;
	    } else {
		c = (char) (i + '1');
		break;
	    }
	} else {
	    if (!tc2) {
		c = (char) (i + '0');
	    } else {
		mp_mul_2d(&r, 1, &r);
		n = mp_cmp_mag(&r, &s);
		if (n < 0) {
		    c = (char) (i + '0');
		} else {
		    c = (char) (i + '1');
		}
	    }
	    break;
	}
    };
    *strPtr++ = c;
    *strPtr++ = '\0';

    /*
     * Free memory, and return.
     */

    mp_clear_multi(&r, &s, &mplus, &mminus, &temp, NULL);
    return k;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitDoubleConversion --
 *
 *	Initializes constants that are needed for conversions to and from
 *	'double'
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The log base 2 of the floating point radix, the number of bits in a
 *	double mantissa, and a table of the powers of five and ten are
 *	computed and stored.
 *
 *----------------------------------------------------------------------
 */

void
TclInitDoubleConversion(void)
{
    int i;
    int x;
    double d;

    if (frexp((double) FLT_RADIX, &log2FLT_RADIX) != 0.5) {
	Tcl_Panic("This code doesn't work on a decimal machine!");
    }
    --log2FLT_RADIX;
    mantBits = DBL_MANT_DIG * log2FLT_RADIX;
    d = 1.0;
    x = (int) (DBL_MANT_DIG * log((double) FLT_RADIX) / log(5.0));
    if (x < MAXPOW) {
	mmaxpow = x;
    } else {
	mmaxpow = MAXPOW;
    }
    for (i=0 ; i<=mmaxpow ; ++i) {
	pow10[i] = d;
	d *= 10.0;
    }
    for (i=0 ; i<9 ; ++i) {
	mp_init(pow5 + i);
    }
    mp_set(pow5, 5);
    for (i=0 ; i<8 ; ++i) {
	mp_sqr(pow5+i, pow5+i+1);
    }
    tiny = SafeLdExp(1.0, DBL_MIN_EXP * log2FLT_RADIX - mantBits);
    maxDigits = (int)
	    ((DBL_MAX_EXP * log((double) FLT_RADIX) + log(10.)/2) / log(10.));
    minDigits = (int)
	    floor((DBL_MIN_EXP-DBL_MANT_DIG)*log((double)FLT_RADIX)/log(10.));
    mantDIGIT = (mantBits + DIGIT_BIT - 1) / DIGIT_BIT;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeDoubleConversion --
 *
 *	Cleans up this file on exit.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Memory allocated by TclInitDoubleConversion is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeDoubleConversion()
{
    int i;
    for (i=0 ; i<9 ; ++i) {
	mp_clear(pow5 + i);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclBignumToDouble --
 *
 *	Convert an arbitrary-precision integer to a native floating point
 *	number.
 *
 * Results:
 *	Returns the converted number.  Sets errno to ERANGE if the number is
 *	too large to convert.
 *
 *----------------------------------------------------------------------
 */

double
TclBignumToDouble(mp_int *a)	/* Integer to convert. */
{
    mp_int b;
    int bits;
    int shift;
    int i;
    double r;

    /*
     * Determine how many bits we need, and extract that many from the input.
     * Round to nearest unit in the last place.
     */

    bits = mp_count_bits(a);
    if (bits > DBL_MAX_EXP*log2FLT_RADIX) {
	errno = ERANGE;
	return HUGE_VAL;
    }
    shift = mantBits + 1 - bits;
    mp_init(&b);
    if (shift > 0) {
	mp_mul_2d(a, shift, &b);
    } else if (shift < 0) {
	mp_div_2d(a, -shift, &b, NULL);
    } else {
	mp_copy(a, &b);
    }
    mp_add_d(&b, 1, &b);
    mp_div_2d(&b, 1, &b, NULL);

    /*
     * Accumulate the result, one mp_digit at a time.
     */

    r = 0.0;
    for (i=b.used-1 ; i>=0 ; --i) {
	r = ldexp(r, DIGIT_BIT) + b.dp[i];
    }
    mp_clear(&b);

    /*
     * Scale the result to the correct number of bits.
     */

    r = ldexp(r, bits - mantBits);

    /*
     * Return the result with the appropriate sign.
     */

    if (a->sign == MP_ZPOS) {
	return r;
    } else {
	return -r;
    }
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
 *	On some platforms, 'ldexp' fails when presented with a number too
 *	small to represent as a normalized double. This routine does 'ldexp'
 *	in two steps for those numbers, to return correctly denormalized
 *	values.
 *
 *----------------------------------------------------------------------
 */

static double
SafeLdExp(double fract, int expt)
{
    int minexpt = DBL_MIN_EXP * log2FLT_RADIX;
    volatile double a, b, retval;
    if (expt < minexpt) {
	a = ldexp(fract, expt - mantBits - minexpt);
	b = ldexp(1.0, mantBits + minexpt);
	retval = a * b;
    } else {
	retval = ldexp(fract, expt);
    }
    return retval;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFormatNaN --
 *
 *	Makes the string representation of a "Not a Number"
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores the string representation in the supplied buffer, which must be
 *	at least TCL_DOUBLE_SPACE characters.
 *
 *----------------------------------------------------------------------
 */

void
TclFormatNaN(double value,	/* The Not-a-Number to format. */
	     char *buffer)	/* String representation. */
{
#ifndef IEEE_FLOATING_POINT
    strcpy(buffer, "NaN");
    return;
#else
    union {
	double dv;
	Tcl_WideUInt iv;
    } bitwhack;

    bitwhack.dv = value;
    if (bitwhack.iv & ((Tcl_WideUInt) 1 << 63)) {
	bitwhack.iv &= ~ ((Tcl_WideUInt) 1 << 63);
	*buffer++ = '-';
    }
    *buffer++ = 'N';
    *buffer++ = 'a';
    *buffer++ = 'N';
    bitwhack.iv &= (((Tcl_WideUInt) 1) << 51) - 1;
    if (bitwhack.iv != 0) {
	sprintf(buffer, "(%" TCL_LL_MODIFIER "x)", bitwhack.iv);
    } else {
	*buffer = '\0';
    }
#endif /* IEEE_FLOATING_POINT */
}
