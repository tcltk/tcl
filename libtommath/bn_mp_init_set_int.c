#include <tommath.h>
#ifdef BN_MP_INIT_SET_INT_C
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

/* initialize and set a digit */
int mp_init_set_int (mp_int * a, unsigned long b)
{
  int err;
  if ((err = mp_init(a)) != MP_OKAY) {
     return err;
  }
  return mp_set_int(a, b);
}
#endif

/* $Source: /root/tcl/repos-to-convert/tcl/libtommath/bn_mp_init_set_int.c,v $ */
/* $Revision: 1.1.1.1.2.1 $ */
/* $Date: 2005/09/26 20:16:53 $ */
