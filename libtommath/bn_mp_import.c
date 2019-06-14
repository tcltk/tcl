#include "tommath_private.h"
#ifdef BN_MP_IMPORT_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* based on gmp's mpz_import.
 * see http://gmplib.org/manual/Integer-Import-and-Export.html
 */
mp_err mp_import(mp_int *rop, size_t count, int order, size_t size,
                 int endian, size_t nails, const void *op)
{
   mp_err err;
   size_t odd_nails, nail_bytes, i, j;
   unsigned char odd_nail_mask;

   mp_zero(rop);

   if (endian == 0) {
      union {
         unsigned int i;
         char c[4];
      } lint;
      lint.i = 0x01020304;

      endian = (lint.c[0] == '\x04') ? -1 : 1;
   }

   odd_nails = (nails % 8u);
   odd_nail_mask = 0xff;
   for (i = 0; i < odd_nails; ++i) {
      odd_nail_mask ^= (unsigned char)(1u << (7u - i));
   }
   nail_bytes = nails / 8u;

   for (i = 0; i < count; ++i) {
      for (j = 0; j < (size - nail_bytes); ++j) {
         unsigned char byte = *((const unsigned char *)op +
                                (((order == 1) ? i : ((count - 1u) - i)) * size) +
                                ((endian == 1) ? (j + nail_bytes) : (((size - 1u) - j) - nail_bytes)));

         if ((err = mp_mul_2d(rop, (j == 0u) ? (int)(8u - odd_nails) : 8, rop)) != MP_OKAY) {
            return err;
         }

         rop->dp[0] |= (j == 0u) ? (mp_digit)(byte & odd_nail_mask) : (mp_digit)byte;
         rop->used  += 1;
      }
   }

   mp_clamp(rop);

   return MP_OKAY;
}

#endif
