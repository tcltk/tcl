#include "tommath_private.h"
#ifdef BN_MP_TO_UNSIGNED_BIN_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* store in unsigned [big endian] format */
mp_err mp_to_unsigned_bin(const mp_int *a, unsigned char *b)
{
   int     x;
   mp_err  err;
   mp_int  t;

   if ((err = mp_init_copy(&t, a)) != MP_OKAY) {
      return err;
   }

   x = 0;
   while (!MP_IS_ZERO(&t)) {
#ifndef MP_8BIT
      b[x++] = (unsigned char)(t.dp[0] & 255u);
#else
      b[x++] = (unsigned char)(t.dp[0] | ((t.dp[1] & 1u) << 7));
#endif
      if ((err = mp_div_2d(&t, 8, &t, NULL)) != MP_OKAY) {
         mp_clear(&t);
         return err;
      }
   }
   s_mp_reverse(b, x);
   mp_clear(&t);
   return MP_OKAY;
}
#endif
