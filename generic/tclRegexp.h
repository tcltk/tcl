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
 * RCS: @(#) $Id: tclRegexp.h,v 1.9 1999/06/10 04:28:51 stanton Exp $
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
 */

typedef struct TclRegexp {
    int flags;			/* Regexp compile flags. */
    regex_t re;			/* Compiled re, includes number of
				 * subexpressions. */
    Tcl_Obj *objPtr;		/* Last object match with this regexp, so
				 * Tcl_RegExpRange() can convert the matches
				 * from character indices to UTF-8 byte
				 * offsets. */
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
EXTERN int		TclRegExpExecUniChar _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_RegExp re, CONST Tcl_UniChar *uniString,
			    int numChars, int nmatches, int flags));
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
