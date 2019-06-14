#include "tommath_private.h"
#ifdef BN_MP_SQRMOD_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* c = a * a (mod b) */
mp_err mp_sqrmod(const mp_int *a, const mp_int *b, mp_int *c)
{
   mp_err  err;
   mp_int  t;

   if ((err = mp_init(&t)) != MP_OKAY) {
      return err;
   }

   if ((err = mp_sqr(a, &t)) != MP_OKAY) {
      mp_clear(&t);
      return err;
   }
   err = mp_mod(&t, b, c);
   mp_clear(&t);
   return err;
}
#endif
