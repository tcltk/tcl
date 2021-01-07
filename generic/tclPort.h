/*
 * tclPort.h --
 *
 *	This header file handles porting issues that occur because
 *	of differences between systems.  It reads in platform specific
 *	portability files.
 *
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLPORT
#define _TCLPORT

#ifdef HAVE_TCL_CONFIG_H
#include "tclConfig.h"
#endif
#if defined(_WIN32)
#   include "tclWinPort.h"
#else
#   include "tclUnixPort.h"
#endif
#include "tcl.h"

#define UWIDE_MAX ULLONG_MAX
#define WIDE_MAX LLONG_MAX
#define WIDE_MIN LLONG_MIN

#endif /* _TCLPORT */
