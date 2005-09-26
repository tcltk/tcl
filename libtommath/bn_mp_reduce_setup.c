#include <tommath.h>
#ifdef BN_MP_REDUCE_SETUP_C
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
 * Tom St Denis, tomstdenis@iahu.ca, http://math.libtomcrypt.org
 */

/* pre-calculate the value required for Barrett reduction
 * For a given modulus "b" it calulates the value required in "a"
 */
int mp_reduce_setup (mp_int * a, mp_int * b)
{
  int     res;
  
  if ((res = mp_2expt (a, b->used * 2 * DIGIT_BIT)) != MP_OKAY) {
    return res;
  }
  return mp_div (a, b, a, NULL);
}
#endif

/* $Source: /root/tcl/repos-to-convert/tcl/libtommath/bn_mp_reduce_setup.c,v $ */
/* $Revision: 1.1.1.1.2.1 $ */
/* $Date: 2005/09/26 20:16:53 $ */
