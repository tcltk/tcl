#include <tommath_private.h>
#ifdef BN_MP_FWRITE_C
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

#ifndef LTM_NO_FILE
int mp_fwrite(mp_int *a, int radix, FILE *stream)
{
   char *buf;
   int err, len, x;
   
   if ((err = mp_radix_size(a, radix, &len)) != MP_OKAY) {
      return err;
   }

   buf = OPT_CAST(char) XMALLOC (len);
   if (buf == NULL) {
      return MP_MEM;
   }
   
   if ((err = mp_toradix(a, buf, radix)) != MP_OKAY) {
      XFREE (buf);
      return err;
   }
   
   for (x = 0; x < len; x++) {
       if (fputc(buf[x], stream) == EOF) {
          XFREE (buf);
          return MP_VAL;
       }
   }
   
   XFREE (buf);
   return MP_OKAY;
}
#endif

#endif

/* ref:         tag: v1.0.1, master */
/* git commit:  5953f62e42b24af93748b1ee5e1d062e242c2546 */
/* commit time: 2017-08-29 22:27:36 +0200 */
