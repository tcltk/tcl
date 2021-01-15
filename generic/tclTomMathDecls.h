/*
 *----------------------------------------------------------------------
 *
 * tclTomMathDecls.h --
 *
 *	This file contains the declarations for the 'libtommath'
 *	functions that are exported by the Tcl library.
 *
 * Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLTOMMATHDECLS
#define _TCLTOMMATHDECLS

#include "tcl.h"
#include <string.h>
#ifndef BN_H_
#include "tclTomMath.h"
#endif

/*
 * Define the version of the Stubs table that's exported for tommath
 */

#define TCLTOMMATH_EPOCH 0
#define TCLTOMMATH_REVISION 0

#define Tcl_TomMath_InitStubs(interp,version) \
    (TclTomMathInitializeStubs((interp),(version),\
                               TCLTOMMATH_EPOCH,TCLTOMMATH_REVISION))

/* Define custom memory allocation for libtommath */

/* MODULE_SCOPE void* TclBNAlloc( size_t ); */
#define TclBNAlloc(s) ((void*)ckalloc((size_t)(s)))
/* MODULE_SCOPE void* TclBNCalloc( size_t, size_t ); */
#define TclBNCalloc(m,s) memset(ckalloc((size_t)(m)*(size_t)(s)),0,(size_t)(m)*(size_t)(s))
/* MODULE_SCOPE void* TclBNRealloc( void*, size_t ); */
#define TclBNRealloc(x,s) ((void*)ckrealloc((char*)(x),(size_t)(s)))
/* MODULE_SCOPE void  TclBNFree( void* ); */
#define TclBNFree(x) (ckfree((char*)(x)))

#undef MP_MALLOC
#undef MP_CALLOC
#undef MP_REALLOC
#undef MP_FREE
#define MP_MALLOC(size)                   TclBNAlloc(size)
#define MP_CALLOC(nmemb, size)            TclBNCalloc(nmemb, size)
#define MP_REALLOC(mem, oldsize, newsize) TclBNRealloc(mem, newsize)
#define MP_FREE(mem, size)                TclBNFree(mem)

#ifndef MODULE_SCOPE
#   define MODULE_SCOPE extern
#endif

#ifdef __cplusplus
extern "C" {
#endif
MODULE_SCOPE mp_err	TclBN_s_mp_add_d(const mp_int *a, mp_digit b, mp_int *c);
MODULE_SCOPE mp_ord	TclBN_s_mp_cmp_d(const mp_int *a, mp_digit b);
MODULE_SCOPE mp_err	TclBN_s_mp_div_d(const mp_int *a, mp_digit b, mp_int *c, mp_digit *d);
MODULE_SCOPE mp_err	TclBN_s_mp_div_3(const mp_int *a, mp_int *c, mp_digit *b);
MODULE_SCOPE mp_err	TclBN_s_mp_expt_u32(const mp_int *a, uint32_t b, mp_int *c);
MODULE_SCOPE mp_err	TclBN_s_mp_init_set(mp_int *a, mp_digit b);
MODULE_SCOPE mp_err	TclBN_s_mp_mul_d(const mp_int *a, mp_digit b, mp_int *c);
MODULE_SCOPE void	TclBN_s_mp_reverse(unsigned char *s, size_t len);
MODULE_SCOPE void	TclBN_s_mp_set(mp_int *a, mp_digit b);
MODULE_SCOPE mp_err	TclBN_s_mp_sub_d(const mp_int *a, mp_digit b, mp_int *c);
#ifdef __cplusplus
}
#endif

/* Rename the global symbols in libtommath to avoid linkage conflicts */

#ifndef TCL_WITH_EXTERNAL_TOMMATH
#define bn_reverse TclBN_reverse
#define mp_add TclBN_mp_add
#define mp_add_d TclBN_s_mp_add_d
#define mp_and TclBN_mp_and
#define mp_clamp TclBN_mp_clamp
#define mp_clear TclBN_mp_clear
#define mp_clear_multi TclBN_mp_clear_multi
#define mp_cmp TclBN_mp_cmp
#define mp_cmp_d TclBN_s_mp_cmp_d
#define mp_cmp_mag TclBN_mp_cmp_mag
#define mp_cnt_lsb TclBN_mp_cnt_lsb
#define mp_copy TclBN_mp_copy
#define mp_count_bits TclBN_mp_count_bits
#define mp_div TclBN_mp_div
#define mp_div_d TclBN_s_mp_div_d
#define mp_div_2 TclBN_mp_div_2
#define mp_div_3 TclBN_s_mp_div_3
#define mp_div_2d TclBN_mp_div_2d
#define mp_exch TclBN_mp_exch
#define mp_expt_d TclBN_mp_expt_d
#define mp_expt_d_ex TclBN_mp_expt_d_ex
#define mp_expt_u32 TclBN_s_mp_expt_u32
#define mp_get_mag_u64 TclBN_mp_get_mag_u64
#define mp_grow TclBN_mp_grow
#define mp_init TclBN_mp_init
#define mp_init_copy TclBN_mp_init_copy
#define mp_init_i64 TclBN_mp_init_i64
#define mp_init_multi TclBN_mp_init_multi
#define mp_init_set TclBN_s_mp_init_set
#define mp_init_size TclBN_mp_init_size
#define mp_init_u64 TclBN_mp_init_u64
#define mp_lshd TclBN_mp_lshd
#define mp_mod TclBN_mp_mod
#define mp_mod_2d TclBN_mp_mod_2d
#define mp_mul TclBN_mp_mul
#define mp_mul_d TclBN_s_mp_mul_d
#define mp_mul_2 TclBN_mp_mul_2
#define mp_mul_2d TclBN_mp_mul_2d
#define mp_neg TclBN_mp_neg
#define mp_or TclBN_mp_or
#define mp_radix_size TclBN_mp_radix_size
#define mp_read_radix TclBN_mp_read_radix
#define mp_rshd TclBN_mp_rshd
#define mp_s_rmap TclBN_mp_s_rmap
#define mp_s_rmap_reverse TclBN_mp_s_rmap_reverse
#define mp_s_rmap_reverse_sz TclBN_mp_s_rmap_reverse_sz
#define mp_set TclBN_s_mp_set
#define mp_set_i64 TclBN_mp_set_i64
#define mp_set_u64 TclBN_mp_set_u64
#define mp_shrink TclBN_mp_shrink
#define mp_sqr TclBN_mp_sqr
#define mp_sqrt TclBN_mp_sqrt
#define mp_sub TclBN_mp_sub
#define mp_sub_d TclBN_s_mp_sub_d
#define mp_signed_rsh TclBN_mp_signed_rsh
#define mp_tc_and TclBN_mp_and
#define mp_tc_div_2d TclBN_mp_signed_rsh
#define mp_tc_or TclBN_mp_or
#define mp_tc_xor TclBN_mp_xor
#define mp_to_unsigned_bin TclBN_mp_to_unsigned_bin
#define mp_to_unsigned_bin_n TclBN_mp_to_unsigned_bin_n
#define mp_toradix_n TclBN_mp_toradix_n
#define mp_to_radix TclBN_mp_to_radix
#define mp_to_ubin TclBN_mp_to_ubin
#define mp_ubin_size TclBN_mp_ubin_size
#define mp_unpack TclBN_mp_unpack
#define mp_xor TclBN_mp_xor
#define mp_zero TclBN_mp_zero
#define s_mp_add TclBN_s_mp_add
#define s_mp_balance_mul TclBN_mp_balance_mul
#define s_mp_karatsuba_mul TclBN_mp_karatsuba_mul
#define s_mp_karatsuba_sqr TclBN_mp_karatsuba_sqr
#define s_mp_mul_digs TclBN_s_mp_mul_digs
#define s_mp_mul_digs_fast TclBN_s_mp_mul_digs_fast
#define s_mp_reverse TclBN_s_mp_reverse
#define s_mp_sqr TclBN_s_mp_sqr
#define s_mp_sqr_fast TclBN_s_mp_sqr_fast
#define s_mp_sub TclBN_s_mp_sub
#define s_mp_toom_mul TclBN_mp_toom_mul
#define s_mp_toom_sqr TclBN_mp_toom_sqr
#endif /* !TCL_WITH_EXTERNAL_TOMMATH */

#define mp_init_set_int(a,b) (MP_DEPRECATED_PRAGMA("replaced by mp_init_ul") TclBN_mp_init_u64(a,(unsigned int)(b)))
#define mp_set_int(a,b) (MP_DEPRECATED_PRAGMA("replaced by mp_set_ul") (TclBN_mp_set_u64((a),((unsigned int)(b))),MP_OKAY))
#define mp_set_long(a,b) (MP_DEPRECATED_PRAGMA("replaced by mp_set_ul") (TclBN_mp_set_u64((a),(long)(b)),MP_OKAY))
#define mp_set_long_long(a,b) (MP_DEPRECATED_PRAGMA("replaced by mp_set_u64") (TclBN_mp_set_u64((a),(b)),MP_OKAY))
#define mp_unsigned_bin_size(mp) (MP_DEPRECATED_PRAGMA("replaced by mp_ubin_size") (int)TclBN_mp_ubin_size(mp))

#undef TCL_STORAGE_CLASS
#ifdef BUILD_tcl
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   ifdef USE_TCL_STUBS
#      define TCL_STORAGE_CLASS
#   else
#      define TCL_STORAGE_CLASS DLLIMPORT
#   endif
#endif

/*
 * WARNING: This file is automatically generated by the tools/genStubs.tcl
 * script.  Any modifications to the function declarations below should be made
 * in the generic/tclInt.decls script.
 */

/* !BEGIN!: Do not edit below this line. */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Exported function declarations:
 */

/* 0 */
EXTERN int		TclBN_epoch(void) MP_WUR;
/* 1 */
EXTERN int		TclBN_revision(void) MP_WUR;
/* 2 */
EXTERN mp_err		TclBN_mp_add(const mp_int *a, const mp_int *b,
				mp_int *c) MP_WUR;
/* 3 */
EXTERN mp_err		TclBN_mp_add_d(const mp_int *a, unsigned int b,
				mp_int *c) MP_WUR;
/* 4 */
EXTERN mp_err		TclBN_mp_and(const mp_int *a, const mp_int *b,
				mp_int *c) MP_WUR;
/* 5 */
EXTERN void		TclBN_mp_clamp(mp_int *a);
/* 6 */
EXTERN void		TclBN_mp_clear(mp_int *a);
/* 7 */
EXTERN void		TclBN_mp_clear_multi(mp_int *a, ...);
/* 8 */
EXTERN mp_ord		TclBN_mp_cmp(const mp_int *a, const mp_int *b) MP_WUR;
/* 9 */
EXTERN mp_ord		TclBN_mp_cmp_d(const mp_int *a, unsigned int b) MP_WUR;
/* 10 */
EXTERN mp_ord		TclBN_mp_cmp_mag(const mp_int *a, const mp_int *b) MP_WUR;
/* 11 */
EXTERN mp_err		TclBN_mp_copy(const mp_int *a, mp_int *b) MP_WUR;
/* 12 */
EXTERN int		TclBN_mp_count_bits(const mp_int *a) MP_WUR;
/* 13 */
EXTERN mp_err		TclBN_mp_div(const mp_int *a, const mp_int *b,
				mp_int *q, mp_int *r) MP_WUR;
/* 14 */
EXTERN mp_err		TclBN_mp_div_d(const mp_int *a, unsigned int b,
				mp_int *q, unsigned int *r) MP_WUR;
/* 15 */
EXTERN mp_err		TclBN_mp_div_2(const mp_int *a, mp_int *q) MP_WUR;
/* 16 */
EXTERN mp_err		TclBN_mp_div_2d(const mp_int *a, int b, mp_int *q,
				mp_int *r) MP_WUR;
/* 17 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_mp_div_3(const mp_int *a, mp_int *q,
				unsigned int *r);
/* 18 */
EXTERN void		TclBN_mp_exch(mp_int *a, mp_int *b);
/* 19 */
EXTERN mp_err		TclBN_mp_expt_u32(const mp_int *a, unsigned int b,
				mp_int *c) MP_WUR;
/* 20 */
EXTERN mp_err		TclBN_mp_grow(mp_int *a, int size) MP_WUR;
/* 21 */
EXTERN mp_err		TclBN_mp_init(mp_int *a) MP_WUR;
/* 22 */
EXTERN mp_err		TclBN_mp_init_copy(mp_int *a, const mp_int *b) MP_WUR;
/* 23 */
EXTERN mp_err		TclBN_mp_init_multi(mp_int *a, ...) MP_WUR;
/* 24 */
EXTERN mp_err		TclBN_mp_init_set(mp_int *a, unsigned int b) MP_WUR;
/* 25 */
EXTERN mp_err		TclBN_mp_init_size(mp_int *a, int size) MP_WUR;
/* 26 */
EXTERN mp_err		TclBN_mp_lshd(mp_int *a, int shift) MP_WUR;
/* 27 */
EXTERN mp_err		TclBN_mp_mod(const mp_int *a, const mp_int *b,
				mp_int *r) MP_WUR;
/* 28 */
EXTERN mp_err		TclBN_mp_mod_2d(const mp_int *a, int b, mp_int *r) MP_WUR;
/* 29 */
EXTERN mp_err		TclBN_mp_mul(const mp_int *a, const mp_int *b,
				mp_int *p) MP_WUR;
/* 30 */
EXTERN mp_err		TclBN_mp_mul_d(const mp_int *a, unsigned int b,
				mp_int *p) MP_WUR;
/* 31 */
EXTERN mp_err		TclBN_mp_mul_2(const mp_int *a, mp_int *p) MP_WUR;
/* 32 */
EXTERN mp_err		TclBN_mp_mul_2d(const mp_int *a, int d, mp_int *p) MP_WUR;
/* 33 */
EXTERN mp_err		TclBN_mp_neg(const mp_int *a, mp_int *b) MP_WUR;
/* 34 */
EXTERN mp_err		TclBN_mp_or(const mp_int *a, const mp_int *b,
				mp_int *c) MP_WUR;
/* 35 */
EXTERN mp_err		TclBN_mp_radix_size(const mp_int *a, int radix,
				int *size) MP_WUR;
/* 36 */
EXTERN mp_err		TclBN_mp_read_radix(mp_int *a, const char *str,
				int radix) MP_WUR;
/* 37 */
EXTERN void		TclBN_mp_rshd(mp_int *a, int shift);
/* 38 */
EXTERN mp_err		TclBN_mp_shrink(mp_int *a) MP_WUR;
/* 39 */
TCL_DEPRECATED("macro calling mp_set_u64")
void			TclBN_mp_set(mp_int *a, unsigned int b);
/* 40 */
EXTERN mp_err		TclBN_mp_sqr(const mp_int *a, mp_int *b);
/* 41 */
EXTERN mp_err		TclBN_mp_sqrt(const mp_int *a, mp_int *b) MP_WUR;
/* 42 */
EXTERN mp_err		TclBN_mp_sub(const mp_int *a, const mp_int *b,
				mp_int *c) MP_WUR;
/* 43 */
EXTERN mp_err		TclBN_mp_sub_d(const mp_int *a, unsigned int b,
				mp_int *c) MP_WUR;
/* 44 */
TCL_DEPRECATED("Use mp_to_ubin")
mp_err			TclBN_mp_to_unsigned_bin(const mp_int *a,
				unsigned char *b);
/* 45 */
TCL_DEPRECATED("Use mp_to_ubin")
mp_err			TclBN_mp_to_unsigned_bin_n(const mp_int *a,
				unsigned char *b, unsigned long *outlen);
/* 46 */
TCL_DEPRECATED("Use mp_to_radix")
mp_err			TclBN_mp_toradix_n(const mp_int *a, char *str,
				int radix, int maxlen);
/* 47 */
EXTERN size_t		TclBN_mp_ubin_size(const mp_int *a);
/* 48 */
EXTERN mp_err		TclBN_mp_xor(const mp_int *a, const mp_int *b,
				mp_int *c) MP_WUR;
/* 49 */
EXTERN void		TclBN_mp_zero(mp_int *a);
/* 50 */
TCL_DEPRECATED("is private function in libtommath")
void			TclBN_reverse(unsigned char *s, int len);
/* 51 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_s_mp_mul_digs_fast(const mp_int *a,
				const mp_int *b, mp_int *c, int digs);
/* 52 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_s_mp_sqr_fast(const mp_int *a, mp_int *b);
/* 53 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_mp_karatsuba_mul(const mp_int *a,
				const mp_int *b, mp_int *c);
/* 54 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_mp_karatsuba_sqr(const mp_int *a, mp_int *b);
/* 55 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_mp_toom_mul(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 56 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_mp_toom_sqr(const mp_int *a, mp_int *b);
/* 57 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_s_mp_add(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 58 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_s_mp_mul_digs(const mp_int *a, const mp_int *b,
				mp_int *c, int digs);
/* 59 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_s_mp_sqr(const mp_int *a, mp_int *b);
/* 60 */
TCL_DEPRECATED("is private function in libtommath")
mp_err			TclBN_s_mp_sub(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 61 */
TCL_DEPRECATED("macro calling mp_init_u64")
mp_err			TclBN_mp_init_ul(mp_int *a, unsigned long i);
/* 62 */
TCL_DEPRECATED("macro calling mp_set_u64")
void			TclBN_mp_set_ul(mp_int *a, unsigned long i);
/* 63 */
EXTERN int		TclBN_mp_cnt_lsb(const mp_int *a) MP_WUR;
/* 64 */
TCL_DEPRECATED("macro calling mp_init_i64")
int			TclBN_mp_init_l(mp_int *bignum, long initVal);
/* 65 */
EXTERN int		TclBN_mp_init_i64(mp_int *bignum, int64_t initVal) MP_WUR;
/* 66 */
EXTERN int		TclBN_mp_init_u64(mp_int *bignum, uint64_t initVal) MP_WUR;
/* 67 */
TCL_DEPRECATED("Use mp_expt_u32")
mp_err			TclBN_mp_expt_d_ex(const mp_int *a, unsigned int b,
				mp_int *c, int fast);
/* 68 */
EXTERN void		TclBN_mp_set_u64(mp_int *a, uint64_t i);
/* 69 */
EXTERN uint64_t		TclBN_mp_get_mag_u64(const mp_int *a) MP_WUR;
/* 70 */
EXTERN void		TclBN_mp_set_i64(mp_int *a, int64_t i);
/* 71 */
EXTERN mp_err		TclBN_mp_unpack(mp_int *rop, size_t count,
				mp_order order, size_t size,
				mp_endian endian, size_t nails,
				const void *op) MP_WUR;
/* Slot 72 is reserved */
/* 73 */
TCL_DEPRECATED("merged with mp_and")
mp_err			TclBN_mp_tc_and(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 74 */
TCL_DEPRECATED("merged with mp_or")
mp_err			TclBN_mp_tc_or(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 75 */
TCL_DEPRECATED("merged with mp_xor")
mp_err			TclBN_mp_tc_xor(const mp_int *a, const mp_int *b,
				mp_int *c);
/* 76 */
EXTERN mp_err		TclBN_mp_signed_rsh(const mp_int *a, int b,
				mp_int *c) MP_WUR;
/* Slot 77 is reserved */
/* 78 */
EXTERN int		TclBN_mp_to_ubin(const mp_int *a, unsigned char *buf,
				size_t maxlen, size_t *written) MP_WUR;
/* 79 */
EXTERN mp_err		TclBN_mp_div_ld(const mp_int *a, uint64_t b,
				mp_int *q, uint64_t *r) MP_WUR;
/* 80 */
EXTERN int		TclBN_mp_to_radix(const mp_int *a, char *str,
				size_t maxlen, size_t *written, int radix) MP_WUR;

typedef struct TclTomMathStubs {
    int magic;
    void *hooks;

    int (*tclBN_epoch) (void) MP_WUR; /* 0 */
    int (*tclBN_revision) (void) MP_WUR; /* 1 */
    mp_err (*tclBN_mp_add) (const mp_int *a, const mp_int *b, mp_int *c) MP_WUR; /* 2 */
    mp_err (*tclBN_mp_add_d) (const mp_int *a, unsigned int b, mp_int *c) MP_WUR; /* 3 */
    mp_err (*tclBN_mp_and) (const mp_int *a, const mp_int *b, mp_int *c) MP_WUR; /* 4 */
    void (*tclBN_mp_clamp) (mp_int *a); /* 5 */
    void (*tclBN_mp_clear) (mp_int *a); /* 6 */
    void (*tclBN_mp_clear_multi) (mp_int *a, ...); /* 7 */
    mp_ord (*tclBN_mp_cmp) (const mp_int *a, const mp_int *b) MP_WUR; /* 8 */
    mp_ord (*tclBN_mp_cmp_d) (const mp_int *a, unsigned int b) MP_WUR; /* 9 */
    mp_ord (*tclBN_mp_cmp_mag) (const mp_int *a, const mp_int *b) MP_WUR; /* 10 */
    mp_err (*tclBN_mp_copy) (const mp_int *a, mp_int *b) MP_WUR; /* 11 */
    int (*tclBN_mp_count_bits) (const mp_int *a) MP_WUR; /* 12 */
    mp_err (*tclBN_mp_div) (const mp_int *a, const mp_int *b, mp_int *q, mp_int *r) MP_WUR; /* 13 */
    mp_err (*tclBN_mp_div_d) (const mp_int *a, unsigned int b, mp_int *q, unsigned int *r) MP_WUR; /* 14 */
    mp_err (*tclBN_mp_div_2) (const mp_int *a, mp_int *q) MP_WUR; /* 15 */
    mp_err (*tclBN_mp_div_2d) (const mp_int *a, int b, mp_int *q, mp_int *r) MP_WUR; /* 16 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_mp_div_3) (const mp_int *a, mp_int *q, unsigned int *r); /* 17 */
    void (*tclBN_mp_exch) (mp_int *a, mp_int *b); /* 18 */
    mp_err (*tclBN_mp_expt_u32) (const mp_int *a, unsigned int b, mp_int *c) MP_WUR; /* 19 */
    mp_err (*tclBN_mp_grow) (mp_int *a, int size) MP_WUR; /* 20 */
    mp_err (*tclBN_mp_init) (mp_int *a) MP_WUR; /* 21 */
    mp_err (*tclBN_mp_init_copy) (mp_int *a, const mp_int *b) MP_WUR; /* 22 */
    mp_err (*tclBN_mp_init_multi) (mp_int *a, ...) MP_WUR; /* 23 */
    mp_err (*tclBN_mp_init_set) (mp_int *a, unsigned int b) MP_WUR; /* 24 */
    mp_err (*tclBN_mp_init_size) (mp_int *a, int size) MP_WUR; /* 25 */
    mp_err (*tclBN_mp_lshd) (mp_int *a, int shift) MP_WUR; /* 26 */
    mp_err (*tclBN_mp_mod) (const mp_int *a, const mp_int *b, mp_int *r) MP_WUR; /* 27 */
    mp_err (*tclBN_mp_mod_2d) (const mp_int *a, int b, mp_int *r) MP_WUR; /* 28 */
    mp_err (*tclBN_mp_mul) (const mp_int *a, const mp_int *b, mp_int *p) MP_WUR; /* 29 */
    mp_err (*tclBN_mp_mul_d) (const mp_int *a, unsigned int b, mp_int *p) MP_WUR; /* 30 */
    mp_err (*tclBN_mp_mul_2) (const mp_int *a, mp_int *p) MP_WUR; /* 31 */
    mp_err (*tclBN_mp_mul_2d) (const mp_int *a, int d, mp_int *p) MP_WUR; /* 32 */
    mp_err (*tclBN_mp_neg) (const mp_int *a, mp_int *b) MP_WUR; /* 33 */
    mp_err (*tclBN_mp_or) (const mp_int *a, const mp_int *b, mp_int *c) MP_WUR; /* 34 */
    mp_err (*tclBN_mp_radix_size) (const mp_int *a, int radix, int *size) MP_WUR; /* 35 */
    mp_err (*tclBN_mp_read_radix) (mp_int *a, const char *str, int radix) MP_WUR; /* 36 */
    void (*tclBN_mp_rshd) (mp_int *a, int shift); /* 37 */
    mp_err (*tclBN_mp_shrink) (mp_int *a) MP_WUR; /* 38 */
    TCL_DEPRECATED_API("macro calling mp_set_u64") void (*tclBN_mp_set) (mp_int *a, unsigned int b); /* 39 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_mp_sqr) (const mp_int *a, mp_int *b); /* 40 */
    mp_err (*tclBN_mp_sqrt) (const mp_int *a, mp_int *b) MP_WUR; /* 41 */
    mp_err (*tclBN_mp_sub) (const mp_int *a, const mp_int *b, mp_int *c) MP_WUR; /* 42 */
    mp_err (*tclBN_mp_sub_d) (const mp_int *a, unsigned int b, mp_int *c) MP_WUR; /* 43 */
    TCL_DEPRECATED_API("Use mp_to_ubin") mp_err (*tclBN_mp_to_unsigned_bin) (const mp_int *a, unsigned char *b); /* 44 */
    TCL_DEPRECATED_API("Use mp_to_ubin") mp_err (*tclBN_mp_to_unsigned_bin_n) (const mp_int *a, unsigned char *b, unsigned long *outlen); /* 45 */
    TCL_DEPRECATED_API("Use mp_to_radix") mp_err (*tclBN_mp_toradix_n) (const mp_int *a, char *str, int radix, int maxlen); /* 46 */
    size_t (*tclBN_mp_ubin_size) (const mp_int *a); /* 47 */
    mp_err (*tclBN_mp_xor) (const mp_int *a, const mp_int *b, mp_int *c) MP_WUR; /* 48 */
    void (*tclBN_mp_zero) (mp_int *a); /* 49 */
    TCL_DEPRECATED_API("is private function in libtommath") void (*tclBN_reverse) (unsigned char *s, int len); /* 50 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_s_mp_mul_digs_fast) (const mp_int *a, const mp_int *b, mp_int *c, int digs); /* 51 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_s_mp_sqr_fast) (const mp_int *a, mp_int *b); /* 52 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_mp_karatsuba_mul) (const mp_int *a, const mp_int *b, mp_int *c); /* 53 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_mp_karatsuba_sqr) (const mp_int *a, mp_int *b); /* 54 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_mp_toom_mul) (const mp_int *a, const mp_int *b, mp_int *c); /* 55 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_mp_toom_sqr) (const mp_int *a, mp_int *b); /* 56 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_s_mp_add) (const mp_int *a, const mp_int *b, mp_int *c); /* 57 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_s_mp_mul_digs) (const mp_int *a, const mp_int *b, mp_int *c, int digs); /* 58 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_s_mp_sqr) (const mp_int *a, mp_int *b); /* 59 */
    TCL_DEPRECATED_API("is private function in libtommath") mp_err (*tclBN_s_mp_sub) (const mp_int *a, const mp_int *b, mp_int *c); /* 60 */
    TCL_DEPRECATED_API("macro calling mp_init_u64") mp_err (*tclBN_mp_init_ul) (mp_int *a, unsigned long i); /* 61 */
    TCL_DEPRECATED_API("macro calling mp_set_u64") void (*tclBN_mp_set_ul) (mp_int *a, unsigned long i); /* 62 */
    int (*tclBN_mp_cnt_lsb) (const mp_int *a) MP_WUR; /* 63 */
    TCL_DEPRECATED_API("macro calling mp_init_i64") int (*tclBN_mp_init_l) (mp_int *bignum, long initVal); /* 64 */
    int (*tclBN_mp_init_i64) (mp_int *bignum, int64_t initVal) MP_WUR; /* 65 */
    int (*tclBN_mp_init_u64) (mp_int *bignum, uint64_t initVal) MP_WUR; /* 66 */
    TCL_DEPRECATED_API("Use mp_expt_u32") mp_err (*tclBN_mp_expt_d_ex) (const mp_int *a, unsigned int b, mp_int *c, int fast); /* 67 */
    void (*tclBN_mp_set_u64) (mp_int *a, uint64_t i); /* 68 */
    uint64_t (*tclBN_mp_get_mag_u64) (const mp_int *a) MP_WUR; /* 69 */
    void (*tclBN_mp_set_i64) (mp_int *a, int64_t i); /* 70 */
    mp_err (*tclBN_mp_unpack) (mp_int *rop, size_t count, mp_order order, size_t size, mp_endian endian, size_t nails, const void *op) MP_WUR; /* 71 */
    void (*reserved72)(void);
    TCL_DEPRECATED_API("merged with mp_and") mp_err (*tclBN_mp_tc_and) (const mp_int *a, const mp_int *b, mp_int *c); /* 73 */
    TCL_DEPRECATED_API("merged with mp_or") mp_err (*tclBN_mp_tc_or) (const mp_int *a, const mp_int *b, mp_int *c); /* 74 */
    TCL_DEPRECATED_API("merged with mp_xor") mp_err (*tclBN_mp_tc_xor) (const mp_int *a, const mp_int *b, mp_int *c); /* 75 */
    mp_err (*tclBN_mp_signed_rsh) (const mp_int *a, int b, mp_int *c) MP_WUR; /* 76 */
    void (*reserved77)(void);
    int (*tclBN_mp_to_ubin) (const mp_int *a, unsigned char *buf, size_t maxlen, size_t *written) MP_WUR; /* 78 */
    mp_err (*tclBN_mp_div_ld) (const mp_int *a, uint64_t b, mp_int *q, uint64_t *r) MP_WUR; /* 79 */
    int (*tclBN_mp_to_radix) (const mp_int *a, char *str, size_t maxlen, size_t *written, int radix) MP_WUR; /* 80 */
} TclTomMathStubs;

extern const TclTomMathStubs *tclTomMathStubsPtr;

#ifdef __cplusplus
}
#endif

#if defined(USE_TCL_STUBS)

/*
 * Inline function declarations:
 */

#define TclBN_epoch \
	(tclTomMathStubsPtr->tclBN_epoch) /* 0 */
#define TclBN_revision \
	(tclTomMathStubsPtr->tclBN_revision) /* 1 */
#define TclBN_mp_add \
	(tclTomMathStubsPtr->tclBN_mp_add) /* 2 */
#define TclBN_mp_add_d \
	(tclTomMathStubsPtr->tclBN_mp_add_d) /* 3 */
#define TclBN_mp_and \
	(tclTomMathStubsPtr->tclBN_mp_and) /* 4 */
#define TclBN_mp_clamp \
	(tclTomMathStubsPtr->tclBN_mp_clamp) /* 5 */
#define TclBN_mp_clear \
	(tclTomMathStubsPtr->tclBN_mp_clear) /* 6 */
#define TclBN_mp_clear_multi \
	(tclTomMathStubsPtr->tclBN_mp_clear_multi) /* 7 */
#define TclBN_mp_cmp \
	(tclTomMathStubsPtr->tclBN_mp_cmp) /* 8 */
#define TclBN_mp_cmp_d \
	(tclTomMathStubsPtr->tclBN_mp_cmp_d) /* 9 */
#define TclBN_mp_cmp_mag \
	(tclTomMathStubsPtr->tclBN_mp_cmp_mag) /* 10 */
#define TclBN_mp_copy \
	(tclTomMathStubsPtr->tclBN_mp_copy) /* 11 */
#define TclBN_mp_count_bits \
	(tclTomMathStubsPtr->tclBN_mp_count_bits) /* 12 */
#define TclBN_mp_div \
	(tclTomMathStubsPtr->tclBN_mp_div) /* 13 */
#define TclBN_mp_div_d \
	(tclTomMathStubsPtr->tclBN_mp_div_d) /* 14 */
#define TclBN_mp_div_2 \
	(tclTomMathStubsPtr->tclBN_mp_div_2) /* 15 */
#define TclBN_mp_div_2d \
	(tclTomMathStubsPtr->tclBN_mp_div_2d) /* 16 */
#define TclBN_mp_div_3 \
	(tclTomMathStubsPtr->tclBN_mp_div_3) /* 17 */
#define TclBN_mp_exch \
	(tclTomMathStubsPtr->tclBN_mp_exch) /* 18 */
#define TclBN_mp_expt_u32 \
	(tclTomMathStubsPtr->tclBN_mp_expt_u32) /* 19 */
#define TclBN_mp_grow \
	(tclTomMathStubsPtr->tclBN_mp_grow) /* 20 */
#define TclBN_mp_init \
	(tclTomMathStubsPtr->tclBN_mp_init) /* 21 */
#define TclBN_mp_init_copy \
	(tclTomMathStubsPtr->tclBN_mp_init_copy) /* 22 */
#define TclBN_mp_init_multi \
	(tclTomMathStubsPtr->tclBN_mp_init_multi) /* 23 */
#define TclBN_mp_init_set \
	(tclTomMathStubsPtr->tclBN_mp_init_set) /* 24 */
#define TclBN_mp_init_size \
	(tclTomMathStubsPtr->tclBN_mp_init_size) /* 25 */
#define TclBN_mp_lshd \
	(tclTomMathStubsPtr->tclBN_mp_lshd) /* 26 */
#define TclBN_mp_mod \
	(tclTomMathStubsPtr->tclBN_mp_mod) /* 27 */
#define TclBN_mp_mod_2d \
	(tclTomMathStubsPtr->tclBN_mp_mod_2d) /* 28 */
#define TclBN_mp_mul \
	(tclTomMathStubsPtr->tclBN_mp_mul) /* 29 */
#define TclBN_mp_mul_d \
	(tclTomMathStubsPtr->tclBN_mp_mul_d) /* 30 */
#define TclBN_mp_mul_2 \
	(tclTomMathStubsPtr->tclBN_mp_mul_2) /* 31 */
#define TclBN_mp_mul_2d \
	(tclTomMathStubsPtr->tclBN_mp_mul_2d) /* 32 */
#define TclBN_mp_neg \
	(tclTomMathStubsPtr->tclBN_mp_neg) /* 33 */
#define TclBN_mp_or \
	(tclTomMathStubsPtr->tclBN_mp_or) /* 34 */
#define TclBN_mp_radix_size \
	(tclTomMathStubsPtr->tclBN_mp_radix_size) /* 35 */
#define TclBN_mp_read_radix \
	(tclTomMathStubsPtr->tclBN_mp_read_radix) /* 36 */
#define TclBN_mp_rshd \
	(tclTomMathStubsPtr->tclBN_mp_rshd) /* 37 */
#define TclBN_mp_shrink \
	(tclTomMathStubsPtr->tclBN_mp_shrink) /* 38 */
#define TclBN_mp_set \
	(tclTomMathStubsPtr->tclBN_mp_set) /* 39 */
#define TclBN_mp_sqr \
	(tclTomMathStubsPtr->tclBN_mp_sqr) /* 40 */
#define TclBN_mp_sqrt \
	(tclTomMathStubsPtr->tclBN_mp_sqrt) /* 41 */
#define TclBN_mp_sub \
	(tclTomMathStubsPtr->tclBN_mp_sub) /* 42 */
#define TclBN_mp_sub_d \
	(tclTomMathStubsPtr->tclBN_mp_sub_d) /* 43 */
#define TclBN_mp_to_unsigned_bin \
	(tclTomMathStubsPtr->tclBN_mp_to_unsigned_bin) /* 44 */
#define TclBN_mp_to_unsigned_bin_n \
	(tclTomMathStubsPtr->tclBN_mp_to_unsigned_bin_n) /* 45 */
#define TclBN_mp_toradix_n \
	(tclTomMathStubsPtr->tclBN_mp_toradix_n) /* 46 */
#define TclBN_mp_ubin_size \
	(tclTomMathStubsPtr->tclBN_mp_ubin_size) /* 47 */
#define TclBN_mp_xor \
	(tclTomMathStubsPtr->tclBN_mp_xor) /* 48 */
#define TclBN_mp_zero \
	(tclTomMathStubsPtr->tclBN_mp_zero) /* 49 */
#define TclBN_reverse \
	(tclTomMathStubsPtr->tclBN_reverse) /* 50 */
#define TclBN_s_mp_mul_digs_fast \
	(tclTomMathStubsPtr->tclBN_s_mp_mul_digs_fast) /* 51 */
#define TclBN_s_mp_sqr_fast \
	(tclTomMathStubsPtr->tclBN_s_mp_sqr_fast) /* 52 */
#define TclBN_mp_karatsuba_mul \
	(tclTomMathStubsPtr->tclBN_mp_karatsuba_mul) /* 53 */
#define TclBN_mp_karatsuba_sqr \
	(tclTomMathStubsPtr->tclBN_mp_karatsuba_sqr) /* 54 */
#define TclBN_mp_toom_mul \
	(tclTomMathStubsPtr->tclBN_mp_toom_mul) /* 55 */
#define TclBN_mp_toom_sqr \
	(tclTomMathStubsPtr->tclBN_mp_toom_sqr) /* 56 */
#define TclBN_s_mp_add \
	(tclTomMathStubsPtr->tclBN_s_mp_add) /* 57 */
#define TclBN_s_mp_mul_digs \
	(tclTomMathStubsPtr->tclBN_s_mp_mul_digs) /* 58 */
#define TclBN_s_mp_sqr \
	(tclTomMathStubsPtr->tclBN_s_mp_sqr) /* 59 */
#define TclBN_s_mp_sub \
	(tclTomMathStubsPtr->tclBN_s_mp_sub) /* 60 */
#define TclBN_mp_init_ul \
	(tclTomMathStubsPtr->tclBN_mp_init_ul) /* 61 */
#define TclBN_mp_set_ul \
	(tclTomMathStubsPtr->tclBN_mp_set_ul) /* 62 */
#define TclBN_mp_cnt_lsb \
	(tclTomMathStubsPtr->tclBN_mp_cnt_lsb) /* 63 */
#define TclBN_mp_init_l \
	(tclTomMathStubsPtr->tclBN_mp_init_l) /* 64 */
#define TclBN_mp_init_i64 \
	(tclTomMathStubsPtr->tclBN_mp_init_i64) /* 65 */
#define TclBN_mp_init_u64 \
	(tclTomMathStubsPtr->tclBN_mp_init_u64) /* 66 */
#define TclBN_mp_expt_d_ex \
	(tclTomMathStubsPtr->tclBN_mp_expt_d_ex) /* 67 */
#define TclBN_mp_set_u64 \
	(tclTomMathStubsPtr->tclBN_mp_set_u64) /* 68 */
#define TclBN_mp_get_mag_u64 \
	(tclTomMathStubsPtr->tclBN_mp_get_mag_u64) /* 69 */
#define TclBN_mp_set_i64 \
	(tclTomMathStubsPtr->tclBN_mp_set_i64) /* 70 */
#define TclBN_mp_unpack \
	(tclTomMathStubsPtr->tclBN_mp_unpack) /* 71 */
/* Slot 72 is reserved */
#define TclBN_mp_tc_and \
	(tclTomMathStubsPtr->tclBN_mp_tc_and) /* 73 */
#define TclBN_mp_tc_or \
	(tclTomMathStubsPtr->tclBN_mp_tc_or) /* 74 */
#define TclBN_mp_tc_xor \
	(tclTomMathStubsPtr->tclBN_mp_tc_xor) /* 75 */
#define TclBN_mp_signed_rsh \
	(tclTomMathStubsPtr->tclBN_mp_signed_rsh) /* 76 */
/* Slot 77 is reserved */
#define TclBN_mp_to_ubin \
	(tclTomMathStubsPtr->tclBN_mp_to_ubin) /* 78 */
#define TclBN_mp_div_ld \
	(tclTomMathStubsPtr->tclBN_mp_div_ld) /* 79 */
#define TclBN_mp_to_radix \
	(tclTomMathStubsPtr->tclBN_mp_to_radix) /* 80 */

#endif /* defined(USE_TCL_STUBS) */

/* !END!: Do not edit above this line. */

#if defined(USE_TCL_STUBS)
#undef mp_add_d
#define mp_add_d TclBN_mp_add_d
#undef mp_cmp_d
#define mp_cmp_d TclBN_mp_cmp_d
#undef mp_div_d
#ifdef MP_64BIT
#define mp_div_d TclBN_mp_div_ld
#else
#define mp_div_d TclBN_mp_div_d
#endif
#undef mp_sub_d
#define mp_sub_d TclBN_mp_sub_d
#undef mp_init_set
#define mp_init_set TclBN_mp_init_set
#undef mp_mul_d
#define mp_mul_d TclBN_mp_mul_d
#undef mp_set
#define mp_set TclBN_mp_set
#undef mp_expt_u32
#define mp_expt_u32 TclBN_mp_expt_u32
#endif /* USE_TCL_STUBS */

#define TclBNInitBignumFromLong(a,b) \
	do { \
	    (a)->dp = NULL; \
	    (void)mp_init_i64((a),(b)); \
	    if ((a)->dp == NULL) { \
	    Tcl_Panic("initialization failure in TclBNInitBignumFromLong"); \
	    } \
	} while (0)
#undef TclBNInitBignumFromWideInt
#define TclBNInitBignumFromWideInt(a,b) \
	do { \
	    (a)->dp = NULL; \
	    (void)mp_init_i64((a),(b)); \
	    if ((a)->dp == NULL) { \
	    Tcl_Panic("initialization failure in TclBNInitBignumFromWideInt"); \
	    } \
	} while (0)
#undef TclBNInitBignumFromWideUInt
#define TclBNInitBignumFromWideUInt(a,b) \
	do { \
	    (a)->dp = NULL; \
	    (void)mp_init_u64((a),(b)); \
	    if ((a)->dp == NULL) { \
	    Tcl_Panic("initialization failure in TclBNInitBignumFromWideUInt"); \
	    } \
	} while (0)
#undef mp_get_ll
#define mp_get_ll(a) ((long long)mp_get_i64(a))
#undef mp_set_ll
#define mp_set_ll(a,b) mp_set_i64(a,b)
#undef mp_init_ll
#define mp_init_ll(a,b) mp_init_i64(a,b)
#undef mp_get_ull
#define mp_get_ull(a) ((unsigned long long)mp_get_i64(a))
#undef mp_set_ull
#define mp_set_ull(a,b) mp_set_u64(a,b)
#undef mp_init_ull
#define mp_init_ull(a,b) mp_init_u64(a,b)
#undef mp_set
#define mp_set(a,b) mp_set_i64((a),(int32_t)(b))
#define mp_set_i32(a,b) mp_set_i64((a),(int32_t)(b))
#define mp_set_l(a,b) mp_set_i64((a),(long)(b))
#define mp_set_u32(a,b) mp_set_u64((a),(uint32_t)(b))
#define mp_set_ul(a,b) mp_set_u64((a),(unsigned long)(b))
#define mp_init_i32(a,b) mp_init_i64((a),(int32_t)(b))
#define mp_init_l(a,b) mp_init_i64((a),(long)(b))
#define mp_init_u32(a,b) mp_init_u64((a),(uint32_t)(b))
#define mp_init_ul(a,b) mp_init_u64((a),(unsigned long)(b))
#undef mp_iseven
#undef mp_isodd
#define mp_iseven(a) (!mp_isodd(a))
#define mp_isodd(a)  (((a)->used != 0) && (((a)->dp[0] & 1) != 0))
#undef mp_sqr
#define mp_sqr(a,b) mp_mul(a,a,b)

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TCLINTDECLS */
