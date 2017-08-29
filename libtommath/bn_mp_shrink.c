#include <tommath_private.h>
#ifdef BN_MP_SHRINK_C
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

/* shrink a bignum */
int mp_shrink (mp_int * a)
{
  mp_digit *tmp;
  int used = 1;
  
  if(a->used > 0) {
    used = a->used;
  }
  
  if (a->alloc != used) {
    if ((tmp = OPT_CAST(mp_digit) XREALLOC (a->dp, sizeof (mp_digit) * used)) == NULL) {
      return MP_MEM;
    }
    a->dp    = tmp;
    a->alloc = used;
  }
  return MP_OKAY;
}
#endif

/* ref:         tag: v1.0.1, master */
/* git commit:  5953f62e42b24af93748b1ee5e1d062e242c2546 */
/* commit time: 2017-08-29 22:27:36 +0200 */
