/* 
 * tclRegexp.h --
 *
 * 	This file contains definitions used internally by Henry
 *	Spencer's regular expression code.
 *
 * Copyright (c) 1998 Henry Spencer.  All rights reserved.
 * 
 * Development of this software was funded, in part, by Cray Research Inc.,
 * UUNET Communications Services Inc., and Sun Microsystems Inc., none of
 * whom are responsible for the results.  The author thanks all of them.
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
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclRegexp.h,v 1.1.2.2 1998/09/24 23:59:02 stanton Exp $
 */

#ifndef _TCLREGEXP
#define _TCLREGEXP

#ifndef _TCLINT
#include "tclInt.h"
#endif

#ifdef BUILD_tcl
# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLEXPORT
#endif

/*
 * The following definitions were culled from wctype.h and wchar.h.
 * Those two header files are now gone.  Eventually we should replace all
 * instances of, e.g., iswalnum() with TclUniCharIsAlnum() in the regexp
 * code.
 */
 
#undef wint_t
#define wint_t int

#undef WEOF
#undef WCHAR_MIN
#undef WCHAR_MAX

#define WEOF		-1
#define WCHAR_MIN	0x0000
#define WCHAR_MAX	0xffff

#undef iswalnum
#undef iswalpha
#undef iswdigit
#undef iswspace

#define	iswalnum(x)	TclUniCharIsAlnum(x)
#define	iswalpha(x)	TclUniCharIsAlpha(x)
#define	iswdigit(x)	TclUniCharIsDigit(x)
#define	iswspace(x)	TclUniCharIsSpace(x)

#undef wcslen
#undef wcsncmp

#define	wcslen		TclUniCharLen
#define	wcsncmp		TclUniCharNcmp

/*
 * The following definitions were added by JO to make Tcl compile
 * under SunOS, where off_t and wchar_t aren't defined; perhaps all of
 * the code below can be collapsed into a few simple definitions?
 */

#ifndef __RE_REGOFF_T
#   define __RE_REGOFF_T int
#endif
#ifndef __RE_WCHAR_T
#   define __RE_WCHAR_T Tcl_UniChar
#endif

/*
 * regoff_t has to be large enough to hold either off_t or ssize_t,
 * and must be signed; it's only a guess that off_t is big enough, so we
 * offer an override.
 */
#ifdef __RE_REGOFF_T
typedef __RE_REGOFF_T regoff_t;		/* offset type for result reporting */
#else
typedef off_t regoff_t;
#endif

/*
 * We offer the option of using a non-wchar_t type in the w prototypes so
 * that <regex.h> can be included without first including (e.g.) <wchar.h>.
 * Note that __RE_WCHAR_T must in fact be the same type as wchar_t!
 */
#ifdef __RE_WCHAR_T
typedef __RE_WCHAR_T re_wchar;	/* internal name for the type */
#else
typedef wchar_t re_wchar;
#endif

#define	REMAGIC	0xfed7

/*
 * other interface types
 */

/* the biggie, a compiled RE (or rather, a front end to same) */
typedef struct {
	int re_magic;		/* magic number */
	size_t re_nsub;		/* number of subexpressions */
	int re_info;		/* information about RE */
#		define	REG_UBACKREF		000001
#		define	REG_ULOOKAHEAD		000002
#		define	REG_UBOUNDS		000004
#		define	REG_UBRACES		000010
#		define	REG_UBSALNUM		000020
#		define	REG_UPBOTCH		000040
#		define	REG_UBBS		000100
#		define	REG_UNONPOSIX		000200
#		define	REG_UUNSPEC		000400
#		define	REG_UUNPORT		001000
#		define	REG_ULOCALE		002000
#		define	REG_UEMPTYMATCH		004000
	int re_csize;		/* sizeof(character) */
	VOID *re_guts;		/* none of your business :-) */
	VOID *re_fns;		/* none of your business :-) */
} regex_t;

/* result reporting (may acquire more fields later) */
typedef struct {
	regoff_t rm_so;		/* start of substring */
	regoff_t rm_eo;		/* end of substring */
} regmatch_t;



/*
 * compilation
 ^ int regcomp(regex_t *, const char *, int);
 ^ int re_comp(regex_t *, const char *, size_t, int);
 ^ #ifndef __RE_NOWIDE
 ^ int re_wcomp(regex_t *, const re_wchar *, size_t, int);
 ^ #endif
 */

#define	REG_DUMP	004000	/* none of your business :-) */
#define	REG_FAKE	010000	/* none of your business :-) */
#define	REG_PROGRESS	020000	/* none of your business :-) */



/*
 * execution
 ^ int regexec(regex_t *, const char *, size_t, regmatch_t [], int);
 ^ int re_exec(regex_t *, const char *, size_t, size_t, regmatch_t [], int);
 ^ #ifndef __RE_NOWIDE
 ^ int re_wexec(regex_t *, const re_wchar *, size_t, size_t, regmatch_t [], int);
 ^ #endif
 */
#define	REG_FTRACE	0010	/* none of your business */
#define	REG_MTRACE	0020	/* none of your business */
#define	REG_SMALL	0040	/* none of your business */

/*
 * error reporting
 * Be careful if modifying the list of error codes -- the table used by
 * regerror() is generated automatically from this file!
 *
 * Note that there is no wchar_t variant of regerror at this time; what
 * kind of character is used for error reports is independent of what kind
 * is used in matching.
 *
 ^ extern size_t regerror(int, const regex_t *, char *, size_t);
 */
#define	REG_OKAY	 0	/* no errors detected */
#define	REG_NOMATCH	 1	/* regexec() failed to match */
#define	REG_BADPAT	 2	/* invalid regular expression */
#define	REG_ECOLLATE	 3	/* invalid collating element */
#define	REG_ECTYPE	 4	/* invalid character class */
#define	REG_EESCAPE	 5	/* invalid escape \ sequence */
#define	REG_ESUBREG	 6	/* invalid backreference number */
#define	REG_EBRACK	 7	/* brackets [] not balanced */
#define	REG_EPAREN	 8	/* parentheses () not balanced */
#define	REG_EBRACE	 9	/* braces {} not balanced */
#define	REG_BADBR	10	/* invalid repetition count(s) */
#define	REG_ERANGE	11	/* invalid character range */
#define	REG_ESPACE	12	/* out of memory */
#define	REG_BADRPT	13	/* quantifier operand invalid */
#define	REG_EMPTY	14	/* empty regular expression */
#define	REG_ASSERT	15	/* "can't happen" -- you found a bug */
#define	REG_INVARG	16	/* invalid argument to regex routine */
#define	REG_MIXED	17	/* char RE applied to wchar_t string (etc.) */
#define	REG_BADOPT	18	/* invalid embedded option */
#define	REG_IMPOSS	19	/* can never match */
/* two specials for debugging and testing */
#define	REG_ATOI	101	/* convert error-code name to number */
#define	REG_ITOA	102	/* convert error-code number to name */



/*
 * the prototypes, as possibly munched by fwd
 */
/* =====^!^===== begin forwards =====^!^===== */
/* automatically gathered by fwd; do not hand-edit */
/* === regex.h === */
EXTERN int	re_ucomp _ANSI_ARGS_((regex_t *, const Tcl_UniChar *,
		    size_t, int));
EXTERN int	re_uexec _ANSI_ARGS_((regex_t *, const Tcl_UniChar *,
		    size_t, size_t, regmatch_t [], int));
EXTERN VOID	regfree _ANSI_ARGS_((regex_t *));
EXTERN size_t	regerror _ANSI_ARGS_((int, const regex_t *, char *, size_t));
/* automatically gathered by fwd; do not hand-edit */
/* =====^!^===== end forwards =====^!^===== */

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
 * Functions exported from the regexp package for the test package to use.
 */

EXTERN void		TclRegError _ANSI_ARGS_((Tcl_Interp *interp, char *msg,
			    int status));

#endif /* _TCLREGEXP */





