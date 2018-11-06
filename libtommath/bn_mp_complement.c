#include "tommath_private.h"
#ifdef BN_MP_COMPLEMENT_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 */

/* b = ~a */
int mp_complement(const mp_int *a, mp_int *b)
{
   int res = mp_neg(a, b);
   return (res == MP_OKAY) ? mp_sub_d(b, 1uL, b) : res;
}
#endif

/* ref:         $Format:%D$ */
/* git commit:  $Format:%H$ */
/* commit time: $Format:%ai$ */
