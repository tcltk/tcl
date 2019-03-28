/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * SPDX-License-Identifier: Unlicense
 */
#ifndef TOMMATH_PRIV_H_
#define TOMMATH_PRIV_H_

#include <tommath.h>
#include <ctype.h>

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifdef __cplusplus
extern "C" {

/* C++ compilers don't like assigning void * to mp_digit * */
#define OPT_CAST(x) (x *)

#else

/* C on the other hand doesn't care */
#define OPT_CAST(x)

#endif

/* define heap macros */
#ifndef XMALLOC
/* default to libc stuff */
#   define XMALLOC   malloc
#   define XFREE     free
#   define XREALLOC  realloc
#elif 0
/* prototypes for our heap functions */
extern void *XMALLOC(size_t n);
extern void *XREALLOC(void *p, size_t n);
extern void XFREE(void *p);
#endif

#if defined(MP_8BIT)
typedef unsigned short       mp_word;
#elif defined(MP_16BIT)
typedef unsigned int         mp_word;
#elif defined(MP_64BIT)
/* for GCC only on supported platforms */
typedef unsigned long        mp_word __attribute__((mode(TI)));
#elif _WIN32
typedef unsigned __int64     mp_word;
#else
typedef unsigned long long   mp_word;
#endif

/* you'll have to tune these... */
#define KARATSUBA_MUL_CUTOFF 80      /* Min. number of digits before Karatsuba multiplication is used. */
#define KARATSUBA_SQR_CUTOFF 120     /* Min. number of digits before Karatsuba squaring is used. */
#define TOOM_MUL_CUTOFF      350     /* no optimal values of these are known yet so set em high */
#define TOOM_SQR_CUTOFF      400

/* size of comba arrays, should be at least 2 * 2**(BITS_PER_WORD - BITS_PER_DIGIT*2) */
#define MP_WARRAY               (1u << (((sizeof(mp_word) * CHAR_BIT) - (2 * DIGIT_BIT)) + 1))

/* lowlevel functions, do not call! */
int s_mp_add(const mp_int *a, const mp_int *b, mp_int *c);
int s_mp_sub(const mp_int *a, const mp_int *b, mp_int *c);
#define s_mp_mul(a, b, c) s_mp_mul_digs(a, b, c, (a)->used + (b)->used + 1)
int fast_s_mp_mul_digs(const mp_int *a, const mp_int *b, mp_int *c, int digs);
int s_mp_mul_digs(const mp_int *a, const mp_int *b, mp_int *c, int digs);
int fast_s_mp_mul_high_digs(const mp_int *a, const mp_int *b, mp_int *c, int digs);
int s_mp_mul_high_digs(const mp_int *a, const mp_int *b, mp_int *c, int digs);
int fast_s_mp_sqr(const mp_int *a, mp_int *b);
int s_mp_sqr(const mp_int *a, mp_int *b);
int mp_karatsuba_mul(const mp_int *a, const mp_int *b, mp_int *c);
int mp_toom_mul(const mp_int *a, const mp_int *b, mp_int *c);
int mp_karatsuba_sqr(const mp_int *a, mp_int *b);
int mp_toom_sqr(const mp_int *a, mp_int *b);
int fast_mp_invmod(const mp_int *a, const mp_int *b, mp_int *c);
int mp_invmod_slow(const mp_int *a, const mp_int *b, mp_int *c);
int fast_mp_montgomery_reduce(mp_int *x, const mp_int *n, mp_digit rho);
int mp_exptmod_fast(const mp_int *G, const mp_int *X, const mp_int *P, mp_int *Y, int redmode);
int s_mp_exptmod(const mp_int *G, const mp_int *X, const mp_int *P, mp_int *Y, int redmode);
void bn_reverse(unsigned char *s, int len);

extern const char *const mp_s_rmap;
extern const unsigned char mp_s_rmap_reverse[];
extern const size_t mp_s_rmap_reverse_sz;

/* Fancy macro to set an MPI from another type.
 * There are several things assumed:
 *  x is the counter and unsigned
 *  a is the pointer to the MPI
 *  b is the original value that should be set in the MPI.
 */
#define MP_SET_XLONG(func_name, type)                    \
int func_name (mp_int * a, type b)                       \
{                                                        \
  unsigned int  x;                                       \
  int           res;                                     \
                                                         \
  mp_zero (a);                                           \
                                                         \
  /* set four bits at a time */                          \
  for (x = 0; x < (sizeof(type) * 2u); x++) {            \
    /* shift the number up four bits */                  \
    if ((res = mp_mul_2d (a, 4, a)) != MP_OKAY) {        \
      return res;                                        \
    }                                                    \
                                                         \
    /* OR in the top four bits of the source */          \
    a->dp[0] |= (mp_digit)(b >> ((sizeof(type) * 8u) - 4u)) & 15uL;\
                                                         \
    /* shift the source up to the next four bits */      \
    b <<= 4;                                             \
                                                         \
    /* ensure that digits are not clamped off */         \
    a->used += 1;                                        \
  }                                                      \
  mp_clamp (a);                                          \
  return MP_OKAY;                                        \
}

#ifdef __cplusplus
}
#endif

#endif


/* ref:         $Format:%D$ */
/* git commit:  $Format:%H$ */
/* commit time: $Format:%ai$ */
