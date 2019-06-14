#include "tommath_private.h"
#ifdef BN_MP_TORADIX_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* stores a bignum as a ASCII string in a given radix (2..64) */
mp_err mp_toradix(const mp_int *a, char *str, int radix)
{
   mp_err  err;
   int digs;
   mp_int  t;
   mp_digit d;
   char   *_s = str;

   /* check range of the radix */
   if ((radix < 2) || (radix > 64)) {
      return MP_VAL;
   }

   /* quick out if its zero */
   if (MP_IS_ZERO(a)) {
      *str++ = '0';
      *str = '\0';
      return MP_OKAY;
   }

   if ((err = mp_init_copy(&t, a)) != MP_OKAY) {
      return err;
   }

   /* if it is negative output a - */
   if (t.sign == MP_NEG) {
      ++_s;
      *str++ = '-';
      t.sign = MP_ZPOS;
   }

   digs = 0;
   while (!MP_IS_ZERO(&t)) {
      if ((err = mp_div_d(&t, (mp_digit)radix, &t, &d)) != MP_OKAY) {
         mp_clear(&t);
         return err;
      }
      *str++ = mp_s_rmap[d];
      ++digs;
   }

   /* reverse the digits of the string.  In this case _s points
    * to the first digit [exluding the sign] of the number]
    */
   s_mp_reverse((unsigned char *)_s, digs);

   /* append a NULL so the string is properly terminated */
   *str = '\0';

   mp_clear(&t);
   return MP_OKAY;
}

#endif
