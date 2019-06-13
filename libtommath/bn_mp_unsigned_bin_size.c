#include "tommath_private.h"
#ifdef BN_MP_UNSIGNED_BIN_SIZE_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* get the size for an unsigned equivalent */
int mp_unsigned_bin_size(const mp_int *a)
{
   int     size = mp_count_bits(a);
   return (size / 8) + ((((unsigned)size & 7u) != 0u) ? 1 : 0);
}
#endif
