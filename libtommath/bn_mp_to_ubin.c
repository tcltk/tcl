#include "tommath_private.h"
#ifdef BN_MP_TO_UBIN_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* store in unsigned [big endian] format */
mp_err mp_to_ubin(const mp_int *a, unsigned char *buf, size_t maxlen, size_t *written)
{
   size_t    x;
   mp_err  err;
   mp_int  t;

   if (buf == NULL) {
      return MP_MEM;
   }

   if (maxlen == 0u) {
      return MP_VAL;
   }

   if ((err = mp_init_copy(&t, a)) != MP_OKAY) {
      return err;
   }

   x = 0u;
   while (!MP_IS_ZERO(&t)) {
      if (maxlen == 0u) {
         err = MP_VAL;
         goto LBL_ERR;
      }
      maxlen--;
#ifndef MP_8BIT
      buf[x++] = (unsigned char)(t.dp[0] & 255u);
#else
      buf[x++] = (unsigned char)(t.dp[0] | ((t.dp[1] & 1u) << 7));
#endif
      if ((err = mp_div_2d(&t, 8, &t, NULL)) != MP_OKAY) {
         goto LBL_ERR;
      }
   }
   s_mp_reverse(buf, x);

   if (written != NULL) {
      *written = x;
   }
LBL_ERR:
   mp_clear(&t);
   return err;
}
#endif
