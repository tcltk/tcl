#include "tommath_private.h"
#ifdef BN_MP_PRIME_RABIN_MILLER_TRIALS_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

static const struct {
   int k, t;
} sizes[] = {
   {    80,    -1 }, /* Use deterministic algorithm for size <= 80 bits */
   {    81,    39 },
   {    96,    37 },
   {   128,    32 },
   {   160,    27 },
   {   192,    21 },
   {   256,    16 },
   {   384,    10 },
   {   512,     7 },
   {   640,     6 },
   {   768,     5 },
   {   896,     4 },
   {  1024,     4 },
   {  2048,     2 }  /* For bigger keysizes use always at least 2 Rounds */
};

/* returns # of RM trials required for a given bit size and max. error of 2^(-96)*/
int mp_prime_rabin_miller_trials(int size)
{
   int x;

   for (x = 0; x < (int)(sizeof(sizes)/(sizeof(sizes[0]))); x++) {
      if (sizes[x].k == size) {
         return sizes[x].t;
      } else if (sizes[x].k > size) {
         return (x == 0) ? sizes[0].t : sizes[x - 1].t;
      }
   }
   return sizes[x-1].t;
}


#endif
