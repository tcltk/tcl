/* headers (which also pick up the standard ones, or equivalents) */
#include "tclInt.h"
#include "tclPort.h"

/* overrides for regguts.h definitions */
/* function-pointer declarations */
#define	FUNCPTR(name, args)	(*name) _ANSI_ARGS_(args)
#define	MALLOC(n)		ckalloc(n)
#define	FREE(p)			ckfree(VS(p))
#define	REALLOC(p,n)		ckrealloc(VS(p),n)



/*
 * Do not insert extras between the "begin" and "end" lines -- this
 * chunk is automatically extracted to be fitted into regex.h.
 */
/* --- begin --- */
/* ensure certain things don't sneak in from system headers */
#ifdef __REG_WIDE_T
#undef __REG_WIDE_T
#endif
#ifdef __REG_WIDE_COMPILE
#undef __REG_WIDE_COMPILE
#endif
#ifdef __REG_WIDE_EXEC
#undef __REG_WIDE_EXEC
#endif
#ifdef __REG_REGOFF_T
#undef __REG_REGOFF_T
#endif
#ifdef __REG_VOID_T
#undef __REG_VOID_T
#endif
#ifdef __REG_CONST
#undef __REG_CONST
#endif
/* interface types */
#define	__REG_WIDE_T	Tcl_UniChar
#define	__REG_WIDE_COMPILE	re_ucomp
#define	__REG_WIDE_EXEC		re_uexec
#define	__REG_REGOFF_T	long	/* not really right, but good enough... */
#define	__REG_VOID_T	VOID
#define	__REG_CONST	CONST
#ifndef __REG_NOFRONT
#define	__REG_NOFRONT		/* don't want regcomp() and regexec() */
#endif
#ifndef __REG_NOCHAR
#define	__REG_NOCHAR		/* or the char versions */
#endif
/* --- end --- */



/* internal character type and related */
typedef Tcl_UniChar chr;	/* the type itself */
typedef int pchr;		/* what it promotes to */
typedef unsigned uchr;		/* unsigned type that will hold a chr */
typedef int celt;		/* type to hold chr, MCCE number, or NOCELT */
#define	NOCELT	(-1)		/* celt value which is not valid chr or MCCE */
#define	CHR(c)	(UCHAR(c))	/* turn char literal into chr literal */
#define	DIGITVAL(c)	((c)-'0')	/* turn chr digit into its value */
#define	CHRBITS	16		/* bits in a chr; must not use sizeof */
#define	CHR_MIN	0x0000		/* smallest and largest chr; the value */
#define	CHR_MAX	0xffff		/*  CHR_MAX-CHR_MIN+1 should fit in uchr */

/* functions operating on chr */
#define	iscalnum(x)	TclUniCharIsAlnum(x)
#define	iscalpha(x)	TclUniCharIsAlpha(x)
#define	iscdigit(x)	TclUniCharIsDigit(x)
#define	iscspace(x)	TclUniCharIsSpace(x)

/* name the external functions */
#define	compile		re_ucomp
#define	exec		re_uexec
#ifdef notdef
#define	regfree		re_ufree
#define	regerror	re_uerror
#endif

/*
 * Implement a mistake in the original POSIX.2:  in EREs, and only in EREs
 * (AREs do not support this botch), an unbalanced right parenthesis is an
 * ordinary character rather than an error.  This was unintentional, and
 * will be fixed someday.
 */
#define	POSIX_MISTAKE	/* sigh */

/* and pick up the standard header */
#include "regex.h"
