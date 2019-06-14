#include "tommath_private.h"
#ifdef BN_MP_SIGNED_BIN_SIZE_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* get the size for an signed equivalent */
int mp_signed_bin_size(const mp_int *a)
{
   return 1 + mp_unsigned_bin_size(a);
}
#endif
