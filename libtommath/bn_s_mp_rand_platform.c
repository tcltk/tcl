#include "tommath_private.h"
#ifdef BN_S_MP_RAND_PLATFORM_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */

/* First the OS-specific special cases
 * - *BSD
 * - Windows
 */
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#  define MP_ARC4RANDOM
#endif

#if defined(_WIN32) || defined(_WIN32_WCE)
#define MP_WIN_CSP

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#ifdef _WIN32_WCE
#define UNDER_CE
#define ARM
#endif

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning (disable : 4668)
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#ifdef _MSC_VER
# pragma warning(pop)
#endif

static mp_err s_read_win_csp(void *p, size_t n)
{
   static HCRYPTPROV hProv = 0;
   if (hProv == 0) {
      HCRYPTPROV h = 0;
      if (!CryptAcquireContext(&h, NULL, MS_DEF_PROV, PROV_RSA_FULL,
                               (CRYPT_VERIFYCONTEXT | CRYPT_MACHINE_KEYSET)) &&
          !CryptAcquireContext(&h, NULL, MS_DEF_PROV, PROV_RSA_FULL,
                               CRYPT_VERIFYCONTEXT | CRYPT_MACHINE_KEYSET | CRYPT_NEWKEYSET)) {
         return MP_ERR;
      }
      hProv = h;
   }
   return CryptGenRandom(hProv, (DWORD)n, (BYTE *)p) == TRUE ? MP_OKAY : MP_ERR;
}
#endif /* WIN32 */

#if !defined(MP_WIN_CSP) && defined(__linux__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 25)
#define MP_GETRANDOM
#include <sys/random.h>
#include <errno.h>

static mp_err s_read_getrandom(void *p, size_t n)
{
   char *q = (char *)p;
   while (n > 0u) {
      ssize_t ret = getrandom(q, n, 0);
      if (ret < 0) {
         if (errno == EINTR) {
            continue;
         }
         return MP_ERR;
      }
      q += ret;
      n -= (size_t)ret;
   }
   return MP_OKAY;
}
#endif
#endif

/* We assume all platforms besides windows provide "/dev/urandom".
 * In case yours doesn't, define MP_NO_DEV_URANDOM at compile-time.
 */
#if !defined(MP_WIN_CSP) && !defined(MP_NO_DEV_URANDOM)
#ifndef MP_DEV_URANDOM
#define MP_DEV_URANDOM "/dev/urandom"
#endif
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

static mp_err s_read_dev_urandom(void *p, size_t n)
{
   int fd;
   char *q = (char *)p;

   do {
      fd = open(MP_DEV_URANDOM, O_RDONLY);
   } while ((fd == -1) && (errno == EINTR));
   if (fd == -1) return MP_ERR;

   while (n > 0u) {
      ssize_t ret = read(fd, p, n);
      if (ret < 0) {
         if (errno == EINTR) {
            continue;
         }
         close(fd);
         return MP_ERR;
      }
      q += ret;
      n -= (size_t)ret;
   }

   close(fd);
   return MP_OKAY;
}
#endif

#if defined(MP_PRNG_ENABLE_LTM_RNG)
unsigned long (*ltm_rng)(unsigned char *out, unsigned long outlen, void (*callback)(void));
void (*ltm_rng_callback)(void);

static mp_err s_read_ltm_rng(void *p, size_t n)
{
   unsigned long res;
   if (ltm_rng == NULL) return MP_ERR;
   res = ltm_rng(p, n, ltm_rng_callback);
   if (res != n) return MP_ERR;
   return MP_OKAY;
}
#endif

mp_err s_mp_rand_platform(void *p, size_t n)
{
#if defined(MP_ARC4RANDOM)
   arc4random_buf(p, n);
   return MP_OKAY;
#else

   mp_err res = MP_ERR;

#if defined(MP_WIN_CSP)
   res = s_read_win_csp(p, n);
   if (res == MP_OKAY) return res;
#endif

#if defined(MP_GETRANDOM)
   res = s_read_getrandom(p, n);
   if (res == MP_OKAY) return res;
#endif

#if defined(MP_DEV_URANDOM)
   res = s_read_dev_urandom(p, n);
   if (res == MP_OKAY) return res;
#endif

#if defined(MP_PRNG_ENABLE_LTM_RNG)
   res = s_read_ltm_rng(p, n);
   if (res == MP_OKAY) return res;
#endif

   return res;
#endif
}

#endif
