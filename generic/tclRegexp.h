/* 
 * tclRegexp.h --
 *
 * 	This file contains definitions used internally by Henry
 *	Spencer's regular expression code.
 *
 * Copyright (c) 1998 Henry Spencer.  All rights reserved.
 * 
 * Development of this software was funded, in part, by Cray Research Inc.,
 * UUNET Communications Services Inc., Sun Microsystems Inc., and
 * Scriptics Corporation, none of whom are responsible for the results.
 * The author thanks all of them.
 * 
 * Redistribution and use in source and binary forms -- with or without
 * modification -- are permitted for any purpose, provided that
 * redistributions in source form retain this entire copyright notice and
 * indicate the origin and nature of any modifications. 
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * HENRY SPENCER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclRegexp.h,v 1.1.2.5 1999/04/05 22:20:31 rjohnson Exp $
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
    CONST char *string;		/* Last string matched with this regexp
				 * (UTF-8), so Tcl_RegExpRange() can convert
				 * the matches from character indices to UTF-8
				 * byte offsets. */
    regmatch_t *matches;	/* Array of indices into the Tcl_UniChar
				 * representation of the last string matched
				 * with this regexp to indicate the location
				 * of subexpressions. */
} TclRegexp;

/*
 * Functions exported for use within the rest of Tcl.
 */

EXTERN int		TclRegAbout _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_RegExp re));
EXTERN VOID		TclRegXflags _ANSI_ARGS_((char *string, int length,
			    int *cflagsPtr, int *eflagsPtr));
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
