/* 
 * tclRegexp.h --
 *
 * 	This file contains definitions used internally by Henry
 *	Spencer's regular expression code.
 *
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclRegexp.h,v 1.10 1999/06/17 19:32:15 stanton Exp $
 */

#ifndef _TCLREGEXP
#define _TCLREGEXP

#include "regex.h"

#ifdef BUILD_tcl
# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLEXPORT
#endif

/*
 * The TclRegexp structure encapsulates a compiled regex_t,
 * the flags that were used to compile it, and an array of pointers
 * that are used to indicate subexpressions after a call to Tcl_RegExpExec.
 * Note that the string and objPtr are mutually exclusive.  These values
 * are needed by Tcl_RegExpRange in order to return pointers into the
 * original string.
 */

typedef struct TclRegexp {
    int flags;			/* Regexp compile flags. */
    regex_t re;			/* Compiled re, includes number of
				 * subexpressions. */
    CONST char *string;		/* Last string passed to Tcl_RegExpExec. */
    Tcl_Obj *objPtr;		/* Last object passed to Tcl_RegExpExecObj. */
    regmatch_t *matches;	/* Array of indices into the Tcl_UniChar
				 * representation of the last string matched
				 * with this regexp to indicate the location
				 * of subexpressions. */
    rm_detail_t details;	/* Detailed information on match (currently
				 * used only for REG_EXPECT). */
    int refCount;		/* Count of number of references to this
				 * compiled regexp. */
} TclRegexp;

/*
 * Functions exported for use within the rest of Tcl.
 */

EXTERN int		TclRegAbout _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_RegExp re));
EXTERN int		TclRegExpMatchObj _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, Tcl_Obj *patObj));
EXTERN void		TclRegExpRangeUniChar _ANSI_ARGS_((Tcl_RegExp re,
			    int index, int *startPtr, int *endPtr));

/*
 * Functions exported from the regexp package for the test package to use.
 */

EXTERN void		TclRegError _ANSI_ARGS_((Tcl_Interp *interp, char *msg,
			    int status));

#endif /* _TCLREGEXP */
