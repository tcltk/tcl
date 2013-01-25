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
    /*
     * TODO: This function corresponds to a processor instruction on
     * many platforms.  Add here the various platform and compiler
     * specific incantations to invoke those assembly instructions.
     */

    const int M = CHAR_BIT * sizeof(size_t);	/* Bits in a size_t */

    if (M == 64) {

	/* First the formula.  Continue on for the explanation. */
	const size_t C1 = 0xC020100804020100;
	const size_t C2 = 0x3FDFEFF7FBFDFEFF;
	const size_t  Q = 0x0001010101010101;
	const size_t  P = 0x0040201008040201;
	const size_t  B = 0x3F9F8F8783818080;

#define LEAD(n) (C1 & (n | (C2 + (n & C2))))
#define SUM(t) (0x7 & (int)((LEAD(t) >> 8) * P >> 54));

	size_t b, t = B & P * (Q * LEAD(n) >> 56);
	int k = 1 + 9 * SUM(t);

	b = n >> k & 0xff;
	t = B & P * b;
	return k - (b == 0) + SUM(t);

	/* Total operations:
	 * 11 bit-ands, 6 multiplies, 6 adds, 6 rightshifts,
	 * 4 assignments, 3 bit-ors, 2 typecasts, and 1 test for zero.
	 *
	 * The whole task is one direct computation.
	 * No branches. No loops.
	 *
	 * 39 operations cannot beat one instruction, so assembly
	 * wins and should be used wherever possible, but this isn't bad.
	 */

#undef SUM
#undef LEAD

#if 0
	/*
	 * Let S = ceil(1 + sqrt(M)), that is, S = 9.
	 * Let T = ceil(M/S), that is, T = 8.
	 * Masks to view a size_t as T blocks of up to S bits each.
	 * Whitespace separates blocks. Pipes separate bytes.
	 * C1 =	1 1000000|00 100000|000 10000|0000
	 *	1000|00000 100|000000 10|0000000 1|00000000
	 * C2 =	0 0111111|11 011111|111 01111|1111
	 *	0111|11111 011|111111 01|1111111 0|11111111
	 */
	const size_t C1 = 0xC020100804020100;
	const size_t C2 = 0x3FDFEFF7FBFDFEFF;	/* Complement of C1 */

	/*
	 * Compute one lead bit per block that is set iff the block
	 * is non-zero.
	 */
	size_t lead = C1 & (n | (C2 + (n & C2)));

	/*
	 * Tricky multiply and shift to put all the lead bits 
	 * together as one T-bit value held in block 0.
	 *
	 * Multiply by sum 2^(i*(s-1)) for i=0 to T-2
	 * = Q = 1 + 2^8 + 2^16 + 2^24 + 2^32 + 2^40 + 2^48
	 * to move all C1 bits into the leftmost T bits.
	 * Then shift right by M-T to set them in the rightmost T bits.
	 */
	const size_t Q = 0x0001010101010101;
	size_t compress = (Q * lead) >> 56 /* M - T */;

	/*
	 * Now compute the MSB of T-bit value compress to determine
	 * which block holds the MSB of M-bit value n.
	 * Make a copy of compress in each full block by another tricky
	 * multiply.
	 * P =	0 0000000|01 000000|001 00000|0001
	 *	0000|00001 000|000001 00|0000001 0|00000001
	 */
	const size_t P = 0x0040201008040201; /* C1 >> T (mostly) */
	size_t copies = P * compress;

	/*
	 * With T-1 copies we can filter on T-1 masks at once.
	 * In block i (0 <= i < T-1), we compute whether the
	 * MSB of compress is >= T-1-i.  Mask away all but the
	 * top i+1 bits and apply the same indicator computation
	 * that produced lead.  Filter mask:
	 * B =	0 0111111|10 011111|100 01111|1000
	 *	0111|10000 011|100000 01|1000000 0|10000000
	 */
	const size_t B = 0x3f9f8f8783818080;
	size_t filtered = B & copies;
	size_t tally = C1 & (filtered | (C2 + (filtered & C2)));

	/* 
	 * Now the sum of all the tally bits is the index
	 * of the MSB of compress.  Use another tricky multiply
	 * and shift to compute it in one go.  How this works:
	 * The multiply by P is a multiply by a sum of bits, each
	 * causing a left shift.  Written out like a long multiplication
	 * by hand, we get several columns to sum, and they are separated
	 * by design so there are no carries.  This means one block of
	 * the result of the multiply is the sum we seek, and we just
	 * shift it into place and mask it.
	 */
	int sum = 0x7 /* T - 1 */ & (int)
		(((tally >> 8 /* T */) * P) >> 54 /* S*(T-2) */);

	/*
	 * Compute corresponding bit shift in n to get the block holding
	 * MSB into block 0.  Note the one extra bit of shift so we put
	 * the top T bits out of the S in the block into the bottom T bits.
	 * This is required since we are only equipped to compute MSB over
	 * T bits, not S (without assuming compute registers larger than
	 * size_t).  We can handle the corner case where we leave ourselves
	 * seeking the MSB of 0.
	 */
	int k = 1 + 9 /* S */ * sum;

	/* Shift and mask to do so. */
	size_t block = (n >> k) & 0xff /* (2^T) - 1 */;

	/*
	 * Now compute MSB of the T-bit value in block 0.
	 * Do it just like we did before.
	 */

	copies = P * block;
	filtered = B & copies;
	tally = C1 & (filtered | (C2 + (filtered & C2)));
	sum = 0x7 & (int) (((tally >> 8) * P) >> 54);

	/*
	 * When (block == 0) our extra shift went a step too far, and
	 * we have to take it back.  In that case sum==0 so adding it
	 * does no harm.  When (block!=0), the value k+sum is correct.
	 */
	return k - (block == 0) + sum;
#endif
    } else if (M == 32) {

	/* S = 6; T = 6;
	 * C1 =	10 100000 |100000 10|0000 1000|00 100000
	 * C2 =	01 011111 |011111 01|1111 0111|11 011111
	 */
	const size_t C1 = 0xA0820820;
	const size_t C2 = 0x5F7DF7DF;	/* Complement of C1 */

	size_t lead = C1 & (n | (C2 + (n & C2)));

	/* Q = 1 + 2^6 + 2^11 + 2^16 */
	/* Q =	00 000000 |000000 01|0000 1000|01 000001 */
	const size_t Q  = 0x00010841;
	const size_t C3 = 0xC0820820;
	size_t compress = (Q * (C3 & (lead+(1<<29))))>>27;

	/*
	 * Now compute the MSB of T-bit value compress to determine
	 * which block holds the MSB of M-bit value n.
	 * Make a copy of compress in each full block by another tricky
	 * multiply.
	 * P =	00 000001 |000001 00|0001 0000|01 000001
	 */
	const size_t P = 0x01041041;
	size_t copies = P * compress;

	/*
	 * With T-1 copies we can filter on T-1 masks at once.
	 * In block i (0 <= i < T-1), we compute whether the
	 * MSB of compress is >= T-1-i.  Mask away all but the
	 * top i+1 bits and apply the same indicator computation
	 * that produced lead.  Filter mask:
	 * B =	00 011111 |011110 01|1100 0110|00 010000
	 */
	const size_t B = 0x1F79C610;
	size_t filtered = B & copies;
	size_t tally = C1 & (filtered | (C2 + (filtered & C2)));

	/* 
	 * Now the sum of all the tally bits is the index
	 * of the MSB of compress.  Use another tricky multiply
	 * and shift to compute it in one go.  How this works:
	 * The multiply by P is a multiply by a sum of bits, each
	 * causing a left shift.  Written out like a long multiplication
	 * by hand, we get several columns to sum, and they are separated
	 * by design so there are no carries.  This means one block of
	 * the result of the multiply is the sum we seek, and we just
	 * shift it into place and mask it.
	 */
	int sum = 0x7 /* T - 1 */ & (int)
		(((tally >> 5 /* S - 1 */) * P) >> 24 /* S*(T-2) */);

	/*
	 * Compute corresponding bit shift in n to get the block holding
	 * MSB into block 0.  Note the one extra bit of shift so we put
	 * the top T bits out of the S in the block into the bottom T bits.
	 * This is required since we are only equipped to compute MSB over
	 * T bits, not S (without assuming compute registers larger than
	 * size_t).  We can handle the corner case where we leave ourselves
	 * seeking the MSB of 0.
	 */
	int k = 6 /* S */ * sum;

	/* Shift and mask to do so. */
	size_t block = ((n >> k) >> 1) & 0x1f /* (2^(S-1)) - 1 */;

	/*
	 * Now compute MSB of the T-bit value in block 0.
	 * Do it just like we did before.
	 */
	copies = P * block;
	filtered = B & copies;
	tally = C1 & (filtered | (C2 + (filtered & C2)));
	sum = 0x7 & (int)
		(((tally >> 5 /* S - 1 */) * P) >> 24 /* S*(T-2) */);

	return k + sum;

    } else {
	/* Simple and slow fallback for cases we haven't done yet. */
	int k = 0;

	while (n >>= 1) {
	    k++;
	}
	return k;
    }
}

