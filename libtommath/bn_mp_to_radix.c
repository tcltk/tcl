#include "tommath_private.h"
#ifdef BN_MP_TO_RADIX_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* stores a bignum as a ASCII string in a given radix (2..64)
 *
 * Stores upto "size - 1" chars and always a NULL byte, puts the number of characters
 * written, including the '\0', in "written".
 */
mp_err mp_to_radix(const mp_int *a, char *str, size_t maxlen, size_t *written, int radix)
{
   size_t  digs;
   mp_err  err;
   mp_int  t;
   mp_digit d;
   char   *_s = str;


   /* If we want to fill a bucket we need a bucket in the first place. */
   if (str == NULL) {
      return MP_VAL;
   }

   /* check range of radix and size*/
   if ((maxlen < 2u) || (radix < 2) || (radix > 64)) {
      return MP_VAL;
   }

   /* quick out if its zero */
   if (MP_IS_ZERO(a)) {
      *str++ = '0';
      *str = '\0';
      if (written != NULL) {
         *written = 2u;
      }
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
   digs = 0u;
   while (!MP_IS_ZERO(&t)) {
      if (--maxlen < 1u) {
         /* no more room */
         /* TODO: It could mimic mp_to_radix_n if that is not an error
                  or at least not this error (MP_ITER or a new one?). */
         err = MP_VAL;
         break;
      }
      if ((err = mp_div_d(&t, (mp_digit)radix, &t, &d)) != MP_OKAY) {
         goto LBL_ERR;
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
   digs++;
   if (written != NULL) {
      *written = (a->sign == MP_NEG) ? digs + 1u: digs;
   }

LBL_ERR:
   mp_clear(&t);
   return err;
}

#endif
