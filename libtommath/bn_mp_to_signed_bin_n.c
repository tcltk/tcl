#include "tommath_private.h"
#ifdef BN_MP_TO_SIGNED_BIN_N_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* store in signed [big endian] format */
mp_err mp_to_signed_bin_n(const mp_int *a, unsigned char *b, unsigned long *outlen)
{
   if (*outlen < (unsigned long)mp_signed_bin_size(a)) {
      return MP_VAL;
   }
   *outlen = (unsigned long)mp_signed_bin_size(a);
   return mp_to_signed_bin(a, b);
}
#endif
