#include <tommath_private.h>
#ifdef BN_MP_GET_INT_C
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
 *
 * Tom St Denis, tstdenis82@gmail.com, http://libtom.org
 */

/* get the lower 32-bits of an mp_int */
unsigned long mp_get_int(mp_int * a)
{
  int i;
  mp_min_u32 res;

  if (a->used == 0) {
     return 0;
  }

  /* get number of digits of the lsb we have to read */
  i = MIN(a->used,(int)(((sizeof(unsigned long) * CHAR_BIT) + DIGIT_BIT - 1) / DIGIT_BIT)) - 1;

  /* get most significant digit of result */
  res = DIGIT(a,i);

  while (--i >= 0) {
    res = (res << DIGIT_BIT) | DIGIT(a,i);
  }

  /* force result to 32-bits always so it is consistent on non 32-bit platforms */
  return res & 0xFFFFFFFFUL;
}
#endif

/* ref:         tag: v1.0.1, master */
/* git commit:  5953f62e42b24af93748b1ee5e1d062e242c2546 */
/* commit time: 2017-08-29 22:27:36 +0200 */
