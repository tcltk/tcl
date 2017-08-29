#include <tommath_private.h>
#ifdef BN_MP_PRIME_IS_DIVISIBLE_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tstdenis82@gmail.com, http://libtom.org
 */

/* determines if an integers is divisible by one 
 * of the first PRIME_SIZE primes or not
 *
 * sets result to 0 if not, 1 if yes
 */
int mp_prime_is_divisible (mp_int * a, int *result)
{
  int     err, ix;
  mp_digit res;

  /* default to not */
  *result = MP_NO;

  for (ix = 0; ix < PRIME_SIZE; ix++) {
    /* what is a mod LBL_prime_tab[ix] */
    if ((err = mp_mod_d (a, ltm_prime_tab[ix], &res)) != MP_OKAY) {
      return err;
    }

    /* is the residue zero? */
    if (res == 0) {
      *result = MP_YES;
      return MP_OKAY;
    }
  }

  return MP_OKAY;
}
#endif

/* ref:         tag: v1.0.1, master */
/* git commit:  5953f62e42b24af93748b1ee5e1d062e242c2546 */
/* commit time: 2017-08-29 22:27:36 +0200 */
