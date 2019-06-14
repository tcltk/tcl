#include "tommath_private.h"
#ifdef BN_MP_EXPORT_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* based on gmp's mpz_export.
 * see http://gmplib.org/manual/Integer-Import-and-Export.html
 */
mp_err mp_export(void *rop, size_t *countp, int order, size_t size,
                 int endian, size_t nails, const mp_int *op)
{
   mp_err err;
   size_t odd_nails, nail_bytes, i, j, bits, count;
   unsigned char odd_nail_mask;

   mp_int t;

   if ((err = mp_init_copy(&t, op)) != MP_OKAY) {
      return err;
   }

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

   bits = (size_t)mp_count_bits(&t);
   count = (bits / ((size * 8u) - nails)) + (((bits % ((size * 8u) - nails)) != 0u) ? 1u : 0u);

   for (i = 0; i < count; ++i) {
      for (j = 0; j < size; ++j) {
         unsigned char *byte = (unsigned char *)rop +
                               (((order == -1) ? i : ((count - 1u) - i)) * size) +
                               ((endian == -1) ? j : ((size - 1u) - j));

         if (j >= (size - nail_bytes)) {
            *byte = 0;
            continue;
         }

         *byte = (unsigned char)((j == ((size - nail_bytes) - 1u)) ? (t.dp[0] & odd_nail_mask) : (t.dp[0] & 0xFFuL));

         if ((err = mp_div_2d(&t, (j == ((size - nail_bytes) - 1u)) ? (int)(8u - odd_nails) : 8, &t, NULL)) != MP_OKAY) {
            mp_clear(&t);
            return err;
         }
      }
   }

   mp_clear(&t);

   if (countp != NULL) {
      *countp = count;
   }

   return MP_OKAY;
}

#endif
