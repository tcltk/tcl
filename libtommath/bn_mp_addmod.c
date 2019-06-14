#include "tommath_private.h"
#ifdef BN_MP_ADDMOD_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* d = a + b (mod c) */
mp_err mp_addmod(const mp_int *a, const mp_int *b, const mp_int *c, mp_int *d)
{
   mp_err  err;
   mp_int  t;

   if ((err = mp_init(&t)) != MP_OKAY) {
      return err;
   }

   if ((err = mp_add(a, b, &t)) != MP_OKAY) {
      mp_clear(&t);
      return err;
   }
   err = mp_mod(&t, c, d);
   mp_clear(&t);
   return err;
}
#endif
