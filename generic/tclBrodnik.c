/*
 * tclBrodnik.c --
 *
 *	This file contains the implementation of a BrodnikArray.
 *
 * Contributions from Don Porter, NIST, 2013. (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclBrodnik.h"

#if defined(HAVE_INTRIN_H)
#    include <intrin.h>
#ifdef _WIN64
#    pragma intrinsic(_BitScanReverse64)
#else
#    pragma intrinsic(_BitScanReverse)
#endif
#endif

/*
 *----------------------------------------------------------------------
 *
 * TclMSB --
 *
 *	Given a size_t non-zero value n, return the index of the most
 *	significant bit in n that is set.  This is equivalent to returning
 *	trunc(log2(n)).  It's also equivalent to the largest integer k
 *	such that 2^k <= n.
 *
 *	This routine is adapted from Andrej Brodnik, "Computation of the
 *	Least Significant Set Bit", pp 7-10, Proceedings of the 2nd
 *	Electrotechnical and Computer Science Conference, Portoroz,
 *	Slovenia, 1993.  The adaptations permit the computation to take
 *	place within size_t values without the need for double length
 *	buffers for calculation.  They also fill in a number of details
 *	the paper omits or leaves unclear.
 *
 * Results:
 *	The index of the most significant set bit in n, a value between
 *	0 and CHAR_BIT*sizeof(size_t) - 1, inclusive.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclMSB(
    size_t n)
{
    const int M = CHAR_BIT * sizeof(size_t);	/* Bits in a size_t */

    /*
     * TODO: This function corresponds to a processor instruction on
     * many platforms.  Add here the various platform and compiler
     * specific incantations to invoke those assembly instructions.
     */
#if defined(__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4)))
	return M - 1 - __builtin_clzll(n);
#endif

#if defined(_MSC_VER) && _MSCVER >= 1300
	unsigned long result;

#ifdef _WIN64
	(void) _BitScanReverse64(&result, n)
#else
	(void) _BitScanReverse(&result, n)
#endif

	return result;
#endif

    if (M == 64) {

	/*
	 * For a byte, consider two masks, C1 = 10000000 selecting just
	 * the high bit, and C2 = 01111111 selecting all other bits.
	 * Then for any byte value n, the computation
	 *	LEAD(n) = C1 & (n | (C2 + (n & C2)))
	 * will leave all bits but the high bit unset, and will have the
	 * high bit set iff n!=0.  The whole thing is an 8-bit test
	 * for being non-zero.  For an 8-byte size_t, each byte can have
	 * the test applied all at once, with combined masks.
	 */
	const size_t C1 = 0x8080808080808080;
	const size_t C2 = 0x7F7F7F7F7F7F7F7F;
#define LEAD(n) (C1 & (n | (C2 + (n & C2))))

	/*
	 * To shift a bit to a new place, multiplication by 2^k will do.
	 * To shift the top 7 bits produced by the LEAD test to the high
	 * 7 bits of the entire size_t, multiply by the right sum of
	 * powers of 2.  In this case
	 * Q = 1 + 2^7 + 2^14 + 2^21 + 2^28 + 2^35 + 2^42
	 * Then shift those 7 bits down to the low 7 bits of the size_t.
	 * The key to making this work is that none of the shifted bits
	 * collide with each other in the top 7-bit destination.
	 * Note that we lose the bit that indicates whether the low byte
	 * is non-zero.  That doesn't matter because we require the original
	 * value n to be non-zero, so if all other bytes signal to be zero,
	 * we know the low byte is non-zero, and if one of the other bytes
	 * signals non-zero, we just don't care what the low byte is.
	 */
	const size_t  Q = 0x0000040810204081;

	/*
	 * To place a copy of a 7-bit value in each of 7 bytes in
	 * a size_t, just multply by the right value.  In this case
	 *  P = 0x00 01 01 01 01 01 01 01
	 * We don't put a copy in the high byte since analysis of the
	 * remaining steps in the algorithm indicates we do not need it.
	 */
	const size_t  P = 0x0001010101010101;

	/*
	 * With 7 copies of the LEAD value, we can now apply 7 masks
	 * to it in a single step by an & against the right value.
	 * B =	00000000 01111111 01111110 01111100
	 *	01111000 01110000 01100000 01000000
	 * The higher the MSB of the copied value is, the more of the
	 * B-masked bytes stored in t will be non-zero.
	 */
	const size_t  B = 0x007F7E7C78706040;
	size_t t = B & P * (LEAD(n) * Q >> 57);

	/*
	 * We want to get a count of the non-zero bytes stored in t.
	 * First use LEAD(t) to create a set of high bits signaling
	 * non-zero values as before.  Call this value
	 * X = x6*2^55 +x5*2^47 +x4*2^39 +x3*2^31 +x2*2^23 +x1*2^15 +x0*2^7
	 * Then notice what multiplication by
	 * P =    2^48 +   2^40 +   2^32 +   2^24 +   2^16 +   2^8 + 1
	 * produces:
	 * P*X = x0*2^7 + (x0 + x1)*2^15 + ...
	 *       ... + (x0 + x1 + x2 + x3 + x4 + x5 + x6) * 2^55 + ...
	 *	 ... + (x5 + x6)*2^95 + x6*2^103
	 * The high terms of this product are going to overflow the size_t
	 * and get lost, but we don't care about them.  What we care is that
	 * the 2^55 term is exactly the sum we seek.  We shift the product
	 * down by 55 bits and then mask away all but the bottom 3 bits
	 * (Max sum can be 7) we get exactly the count of non-zero B-masked
	 * bytes.  By design of the mask, this count is the index of the
	 * MSB of the LEAD value.  It indicates which byte of the original
	 * value contains the MSB of the original value.
	 */
#define SUM(t) (0x7 & (int)(LEAD(t) * P >> 55));

	/*
	 * Multiply by 8 to get the number of bits to shift to place
	 * that MSB-containing byte in the low byte.
	 */
	int k = 8 * SUM(t);

	/*
	 * Shift the MSB byte to the low byte.  Then shift one more bit.
	 * Since we know the MSB byte is non-zero we only need to compute
	 * the MSB of the top 7 bits.  If all top 7 bits are zero, we know
	 * the bottom bit is the 1 and the correct index is 0.  Compute the
	 * MSB of that value by the same steps we did before.
	 */
	t = B & P * (n >> k >> 1);

	/*
	 * Add the index of the MSB of the byte to the index of the low
	 * bit of that byte computed before to get the final answer.
	 */
	return k + SUM(t);

	/* Total operations: 33
	 * 10 bit-ands, 6 multiplies, 4 adds, 5 rightshifts,
	 * 3 assignments, 3 bit-ors, 2 typecasts.
	 *
	 * The whole task is one direct computation.
	 * No branches. No loops.
	 *
	 * 33 operations cannot beat one instruction, so assembly
	 * wins and should be used wherever possible, but this isn't bad.
	 */

#undef SUM
    } else if (M == 32) {

	/* Same scheme as above, with adjustments to the 32-bit size */
	const size_t C1 = 0xA0820820;
	const size_t C2 = 0x5F7DF7DF;
	const size_t C3 = 0xC0820820;
	const size_t C4 = 0x20000000;
	const size_t Q  = 0x00010841;
	const size_t P = 0x01041041;
	const size_t B = 0x1F79C610;

#define SUM(t) (0x7 & (LEAD(t) * P >> 29));

	size_t t = B & P * ((C3 & (LEAD(n) + C4)) * Q >> 27);
	int k = 6 * SUM(t);

	t = B & P * (n >> k >> 1);
	return k + SUM(t);

	/* Total operations: 33
	 * 11 bit-ands, 6 multiplies, 5 adds, 5 rightshifts,
	 * 3 assignments, 3 bit-ors.
	 */

#undef SUM
#undef LEAD

    } else {
	/* Simple and slow fallback for cases we haven't done yet. */
	int k = 0;

	while (n >>= 1) {
	    k++;
	}
	return k;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclBAConvertIndices --
 *
 *	Given a size_t index into the sequence, convert into the
 *	corresponding index pair of the store[] of a BrodnikArray.
 *
 *	store[] is an array of arrays, and as the total size grows
 *	larger, the size of the arrays store[i] grow in a way that
 *	each new array is of about size sqrt(N), yet the index conversion
 *	routine remains relatively simple to calculate.
 *
 * Results:
 *	The index pair is written to *hiPtr and *loPtr, which may not
 *	be NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclBAConvertIndices(
    size_t	index,
    size_t	*hiPtr,
    size_t	*loPtr)
{
    size_t	r = index + 1;
    int		k = TclMSB(r);
    int		shift = (k + 1) >> 1;
    size_t	lobits = (((size_t)1) << shift) - 1;
    size_t	hibits = ((size_t)1) << (k - shift);

    *hiPtr	= (lobits << 1) - ((k & 1) * hibits)
			+ ((r >> shift) & (hibits - 1));
    *loPtr	= r & lobits;
}

