#include "tommath_private.h"
#ifdef BN_MP_TORADIX_N_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* stores a bignum as a ASCII string in a given radix (2..64)
 *
 * Stores upto maxlen-1 chars and always a NULL byte
 */
mp_err mp_toradix_n(const mp_int *a, char *str, int radix, int maxlen)
{
   int     digs;
   mp_err  err;
   mp_int  t;
   mp_digit d;
   char   *_s = str;

   /* check range of the maxlen, radix */
   if ((maxlen < 2) || (radix < 2) || (radix > 64)) {
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
      /* we have to reverse our digits later... but not the - sign!! */
      ++_s;

      /* store the flag and mark the number as positive */
      *str++ = '-';
      t.sign = MP_ZPOS;

      /* subtract a char */
      --maxlen;
   }

   digs = 0;
   while (!MP_IS_ZERO(&t)) {
      if (--maxlen < 1) {
         /* no more room */
         break;
      }
      if ((err = mp_div_d(&t, (mp_digit)radix, &t, &d)) != MP_OKAY) {
         mp_clear(&t);
         return err;
      }
      *str++ = mp_s_rmap[d];
      ++digs;
   }

   /* reverse the digits of the string.  In this case _s points
    * to the first digit [exluding the sign] of the number
    */
   s_mp_reverse((unsigned char *)_s, digs);

   /* append a NULL so the string is properly terminated */
   *str = '\0';

   mp_clear(&t);
   return MP_OKAY;
}

#endif
