#include <tommath_private.h>
#ifdef BN_MP_LCM_C
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

/* computes least common multiple as |a*b|/(a, b) */
int mp_lcm (mp_int * a, mp_int * b, mp_int * c)
{
  int     res;
  mp_int  t1, t2;


  if ((res = mp_init_multi (&t1, &t2, NULL)) != MP_OKAY) {
    return res;
  }

  /* t1 = get the GCD of the two inputs */
  if ((res = mp_gcd (a, b, &t1)) != MP_OKAY) {
    goto LBL_T;
  }

  /* divide the smallest by the GCD */
  if (mp_cmp_mag(a, b) == MP_LT) {
     /* store quotient in t2 such that t2 * b is the LCM */
     if ((res = mp_div(a, &t1, &t2, NULL)) != MP_OKAY) {
        goto LBL_T;
     }
     res = mp_mul(b, &t2, c);
  } else {
     /* store quotient in t2 such that t2 * a is the LCM */
     if ((res = mp_div(b, &t1, &t2, NULL)) != MP_OKAY) {
        goto LBL_T;
     }
     res = mp_mul(a, &t2, c);
  }

  /* fix the sign to positive */
  c->sign = MP_ZPOS;

LBL_T:
  mp_clear_multi (&t1, &t2, NULL);
  return res;
}
#endif

/* ref:         tag: v1.0.1, master */
/* git commit:  5953f62e42b24af93748b1ee5e1d062e242c2546 */
/* commit time: 2017-08-29 22:27:36 +0200 */
