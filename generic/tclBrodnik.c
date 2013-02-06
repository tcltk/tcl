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

#include "tclInt.h"

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
 * The routines that follow implement a BrodnikArray of T elements, where
 * T is the type of the elements to be stored.  Here T is char, but simple
 * text substitution should allow any type at all.  The idea is to convert
 * these routines into either preprocessor macros, or the ouput of some
 * code-generating template operation.
 *
 * The BroknikArray data structure is adapted from Andrej Brodnik et al.,
 * "Resizable Arrays in Optimal Time and Space", from the Proceedings of
 * the 1999 Workshop on Algorithms and Data Structures, Lecture Notes
 * in Computer Science (LNCS) vol. 1663, pp. 37-48.
 *
 * The BrodnikArray is an indexed sequence of values.  The routines
 * Append() and Detach() permit inserting and deleting an element at
 * the end of the sequence, with allocated memory growing and shrinking
 * as needed.  The At(index) routine returns a (T *) pointing to the
 * index'th element in the sequence.  Indices are size_t values and
 * array elements have indices starting with 0.  Through the pointer
 * elements may be read or (over)written.  These routines provide
 * stack-like access as well as random access to the elements stored
 * in the array.
 *
 * The memory allocation management is such that for N elements stored
 * in the array, the amount of memory allocated, but not used
 * is O(sqrt(N)).  This is more efficient than the traditional Tcl
 * array growth algorithm that has O(N) memory waste.  The longest
 * contiguous span of allocated memory is also O(sqrt(N)) so longer
 * sequences should be possible without failure due to lack of a
 * long enough contiguous span of memory.  The main drawback of the
 * BrodnikArray is that it is a two-level storage structure, so two
 * pointer derefences are required to get an element, when a plain
 * array gets the job done in only one.  The other potential trouble
 * is the need for frequent reallocs as small arrays grow, though that's
 * not hard to remedy if it's a problem in practice.
 */

/*
 *----------------------------------------------------------------------
 *
 * BA_ConvertIndices --
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

static void
BA_ConvertIndices(
    size_t	index,
    size_t	*hiPtr,
    size_t	*loPtr)
{
    size_t	r = index + 1;
    int		k = TclMSB(r);
    int		shift = (k + 1) >> 1;
    size_t	lobits = (1 << shift) - 1;
    size_t	hibits = 1 << (k - shift);

    *hiPtr	= (lobits << 1) - ((k & 1) * hibits)
			+ ((r >> shift) & (hibits - 1));
    *loPtr	= r & lobits;
}

/*
 * The rest of the BrodnikArray routines have to care about what the
 * element type is, since C has little  provision for generic programming.
 */

typedef char T;

typedef struct BrodnikArray_T BA_T;

struct BrodnikArray_T {
    size_t	used;
    size_t	avail;
    size_t	dbused;
    size_t	dbavail;
    T *		store[1];
};

/*
 * Initial creation makes a BA with no elements in it, but room for 1
 * before a need to call Grow().
 */
BA_T *
BA_T_Create()
{
    BA_T *newPtr = ckalloc(sizeof(BA_T));

    newPtr->used = 0;
    newPtr->avail = 1;
    newPtr->dbused = 1;
    newPtr->dbavail = 1;
    newPtr->store[0] = ckalloc(sizeof(T));
    return newPtr;
}

void
BA_T_Destroy(
    BA_T *a)
{
    size_t i = a->dbused;

    while (i--) {
	ckfree(a->store[i]);
    }
    ckfree(a);
}

BA_T *
BA_T_Grow(
    BA_T *a)
{
    size_t dbsize = 1 << ((TclMSB(a->avail + 1) + 1) >> 1);

    if (a->dbused == a->dbavail) {
	/* Have to grow the index block */
	a->dbavail *= 2;
	a = ckrealloc(a, sizeof(BA_T) + (a->dbavail - 1) * sizeof(T *));
    }
    a->store[a->dbused] = ckalloc(dbsize * sizeof(T));
    a->dbused++;
    a->avail += dbsize;
    return a;
}

BA_T *
BA_T_Shrink(
    BA_T *a)
{
    a->dbused--;
    ckfree(a->store[a->dbused]);
    if (a->dbavail / a->dbused >= 4) {
	a->dbavail /= 2;
	a = ckrealloc(a, sizeof(BA_T) + (a->dbavail - 1) * sizeof(T *));
    }
    return a;
}

BA_T *
BA_T_Append(
    BA_T *a,
    T *elemPtr)
{
    size_t hi, lo;

    if (a->used == a->avail) {
	a = BA_T_Grow(a);
    }
    BA_ConvertIndices(a->used, &hi, &lo);
    a->store[hi][lo] = *elemPtr;
    a->used++;
    return a;
}

BA_T *
BA_T_Detach(
    BA_T *a,
    T *elemPtr)
{
    size_t hi, lo;

    if (a->used == 0) {
	/*
	 * Detaching the last element from an empty array is not
	 * well defined.  Here we do nothing.  Might consider a
	 * panic, or some error-raising convention.
	 */
	return a;
    }
    a->used--;
    BA_ConvertIndices(a->used, &hi, &lo);
    *elemPtr = a->store[hi][lo];
    if (lo || (hi == a->dbused - 1)) {
	return a;
    }
    return BA_T_Shrink(a);
}

T *
BA_T_At(
    BA_T *a,
    size_t index)
{
    size_t hi, lo;

    if (index >= a->used) {
	return NULL;
    }
    BA_ConvertIndices(index, &hi, &lo);
    return a->store[hi] + lo;
}

