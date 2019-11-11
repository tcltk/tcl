#ifndef BN_TCL_H_
#define BN_TCL_H_

#ifdef MP_NO_STDINT
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#else
#  include "../compat/stdint.h"
#endif
#endif
#include <tommath.h>
#include "tclTomMathDecls.h"

#undef mp_iseven
#undef mp_isodd
#define mp_iseven(a) (!mp_isodd(a))
#define mp_isodd(a)  (((a)->used != 0 && (((a)->dp[0] & 1) != 0)) ? MP_YES : MP_NO)
#endif
