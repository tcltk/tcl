#include "tommath_private.h"
#ifdef BN_MP_MUL_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* high level multiplication (handles sign) */
mp_err mp_mul(const mp_int *a, const mp_int *b, mp_int *c)
{
   mp_err  err;
   mp_sign neg;
#ifdef BN_S_MP_BALANCE_MUL_C
   int len_b, len_a;
#endif
   neg = (a->sign == b->sign) ? MP_ZPOS : MP_NEG;
#ifdef BN_S_MP_BALANCE_MUL_C
   len_a = a->used;
   len_b = b->used;

   if (len_a == len_b) {
      goto GO_ON;
   }
   /*
    * Check sizes. The smaller one needs to be larger than the Karatsuba cut-off.
    * The bigger one needs to be at least about one KARATSUBA_MUL_CUTOFF bigger
    * to make some sense, but it depends on architecture, OS, position of the
    * stars... so YMMV.
    * Using it to cut the input into slices small enough for fast_s_mp_mul_digs
    * was actually slower on the author's machine, but YMMV.
    */
   if ((MP_MIN(len_a, len_b) < MP_KARATSUBA_MUL_CUTOFF)
       || ((MP_MAX(len_a, len_b) / 2) < MP_KARATSUBA_MUL_CUTOFF)) {
      goto GO_ON;
   }
   /*
    * Not much effect was observed below a ratio of 1:2, but again: YMMV.
    */
   if ((MP_MAX(len_a, len_b) /  MP_MIN(len_a, len_b)) < 2) {
      goto GO_ON;
   }

   err = s_mp_balance_mul(a,b,c);
   goto END;

GO_ON:
#endif

   /* use Toom-Cook? */
#ifdef BN_S_MP_TOOM_MUL_C
   if (MP_MIN(a->used, b->used) >= MP_TOOM_MUL_CUTOFF) {
      err = s_mp_toom_mul(a, b, c);
   } else
#endif
#ifdef BN_S_MP_KARATSUBA_MUL_C
      /* use Karatsuba? */
      if (MP_MIN(a->used, b->used) >= MP_KARATSUBA_MUL_CUTOFF) {
         err = s_mp_karatsuba_mul(a, b, c);
      } else
#endif
      {
         /* can we use the fast multiplier?
          *
          * The fast multiplier can be used if the output will
          * have less than MP_WARRAY digits and the number of
          * digits won't affect carry propagation
          */
         int     digs = a->used + b->used + 1;

#ifdef BN_S_MP_MUL_DIGS_FAST_C
         if ((digs < MP_WARRAY) &&
             (MP_MIN(a->used, b->used) <= MP_MAXFAST)) {
            err = s_mp_mul_digs_fast(a, b, c, digs);
         } else
#endif
         {
#ifdef BN_S_MP_MUL_DIGS_C
            err = s_mp_mul_digs(a, b, c, a->used + b->used + 1);
#else
            err = MP_VAL;
#endif
         }
      }
END:
   c->sign = (c->used > 0) ? neg : MP_ZPOS;
   return err;
}
#endif
