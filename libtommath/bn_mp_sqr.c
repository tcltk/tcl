#include "tommath_private.h"
#ifdef BN_MP_SQR_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* computes b = a*a */
mp_err mp_sqr(const mp_int *a, mp_int *b)
{
   mp_err err;

#ifdef BN_S_MP_TOOM_SQR_C
   /* use Toom-Cook? */
   if (a->used >= MP_TOOM_SQR_CUTOFF) {
      err = s_mp_toom_sqr(a, b);
      /* Karatsuba? */
   } else
#endif
#ifdef BN_S_MP_KARATSUBA_SQR_C
      if (a->used >= MP_KARATSUBA_SQR_CUTOFF) {
         err = s_mp_karatsuba_sqr(a, b);
      } else
#endif
      {
#ifdef BN_S_MP_SQR_FAST_C
         /* can we use the fast comba multiplier? */
         if ((((a->used * 2) + 1) < MP_WARRAY) &&
             (a->used < (MP_MAXFAST / 2))) {
            err = s_mp_sqr_fast(a, b);
         } else
#endif
         {
#ifdef BN_S_MP_SQR_C
            err = s_mp_sqr(a, b);
#else
            err = MP_VAL;
#endif
         }
      }
   b->sign = MP_ZPOS;
   return err;
}
#endif
