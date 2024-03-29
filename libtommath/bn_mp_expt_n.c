#include "tommath_private.h"
#ifdef BN_MP_EXPT_N_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

#ifdef BN_MP_EXPT_U32_C
mp_err mp_expt_u32(const mp_int *a, uint32_t b, mp_int *c)
{
   if (b > MP_MIN(MP_DIGIT_MAX, INT_MAX)) {
      return MP_VAL;
   }
   return mp_expt_n(a, (int)b, c);
}
#endif

/* calculate c = a**b  using a square-multiply algorithm */
mp_err mp_expt_n(const mp_int *a, int b, mp_int *c)
{
   mp_err err;
   mp_int  g;

   if ((err = mp_init_copy(&g, a)) != MP_OKAY) {
      return err;
   }

   /* set initial result */
   mp_set(c, 1uL);

   while (b > 0) {
      /* if the bit is set multiply */
      if ((b & 1) != 0) {
         if ((err = mp_mul(c, &g, c)) != MP_OKAY) {
            goto LBL_ERR;
         }
      }

      /* square */
      if (b > 1) {
         if ((err = mp_sqr(&g, &g)) != MP_OKAY) {
            goto LBL_ERR;
         }
      }

      /* shift to next bit */
      b >>= 1;
   }

LBL_ERR:
   mp_clear(&g);
   return err;
}

#endif
