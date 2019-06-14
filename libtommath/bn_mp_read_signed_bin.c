#include "tommath_private.h"
#ifdef BN_MP_READ_SIGNED_BIN_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* read signed bin, big endian, first byte is 0==positive or 1==negative */
mp_err mp_read_signed_bin(mp_int *a, const unsigned char *b, int c)
{
   mp_err err;

   /* read magnitude */
   if ((err = mp_read_unsigned_bin(a, b + 1, c - 1)) != MP_OKAY) {
      return err;
   }

   /* first byte is 0 for positive, non-zero for negative */
   if (b[0] == (unsigned char)0) {
      a->sign = MP_ZPOS;
   } else {
      a->sign = MP_NEG;
   }

   return MP_OKAY;
}
#endif
