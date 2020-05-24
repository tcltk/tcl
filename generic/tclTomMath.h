#ifndef BN_TCL_H_
#define BN_TCL_H_

#ifdef MP_NO_STDINT
#   ifdef HAVE_STDINT_H
#	include <stdint.h>
#else
#	include "../compat/stdint.h"
#   endif
#endif
#ifndef BN_H_ /* If BN_H_ already defined, don't try to include tommath.h again. */
#   include "tommath.h"
#endif
#include "tclTomMathDecls.h"

#endif
