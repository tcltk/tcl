/* 
 * tclRegexp.c --
 *
 *	This file contains the public interfaces to the Tcl regular
 *	expression mechanism.
 *
 * Copyright (c) 1998 by Scriptics Corporation.
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclRegexp.c,v 1.1.2.6 1998/11/17 21:38:39 stanton Exp $
 */

#include "tclInt.h"
#include "tclPort.h"
#include "tclRegexp.h"

/*
 *----------------------------------------------------------------------
 * The routines in this file use Henry Spencer's regular expression
 * package contained in the following additional source files:
 *
 *	regc_color.c	regc_cvec.c	regc_lex.c
 *	regc_nfa.c	regcomp.c	regcustom.h
 *	rege_dfa.c	regerror.c	regerrs.h
 *	regex.h		regexec.c	regfree.c
 *	regfronts.c	regguts.h
 *
 * Copyright (c) 1998 Henry Spencer.  All rights reserved.
 * 
 * Development of this software was funded, in part, by Cray Research Inc.,
 * UUNET Communications Services Inc., Sun Microsystems Inc., and Scriptics
 * Corporation, none of whom are responsible for the results.  The author
 * thanks all of them. 
 * 
 * Redistribution and use in source and binary forms -- with or without
 * modification -- are permitted for any purpose, provided that
 * redistributions in source form retain this entire copyright notice and
 * indicate the origin and nature of any modifications.
 * 
 * I'd appreciate being given credit for this package in the documentation
 * of software which uses it, but that is not a requirement.
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
 * *** NOTE: this code has been altered slightly for use in Tcl: ***
 * *** 1. Names have been changed, e.g. from re_comp to		 ***
 * ***    TclRegComp, to avoid clashes with other 		 ***
 * ***    regexp implementations used by applications. 		 ***
 */

/*
 * Declarations for functions used only in this file.
 */

static void		DupRegexpInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static void		FreeRegexpInternalRep _ANSI_ARGS_((Tcl_Obj *regexpPtr));
static int		SetRegexpFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
static TclRegexp *	CompileRegexp _ANSI_ARGS_((Tcl_Interp *interp,
			    char *pattern, int length, int flags));

/*
 * The regular expression Tcl object type.  This serves as a cache
 * of the compiled form of the regular expression.
 */

Tcl_ObjType tclRegexpType = {
    "regexp",				/* name */
    FreeRegexpInternalRep,		/* freeIntRepProc */
    DupRegexpInternalRep,		/* dupIntRepProc */
    NULL,				/* updateStringProc */
    SetRegexpFromAny			/* setFromAnyProc */
};


/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpCompile --
 *
 *	Compile a regular expression into a form suitable for fast
 *	matching.  This procedure is DEPRECATED in favor of the
 *	object version of the command.
 *
 * Results:
 *	The return value is a pointer to the compiled form of string,
 *	suitable for passing to Tcl_RegExpExec.  This compiled form
 *	is only valid up until the next call to this procedure, so
 *	don't keep these around for a long time!  If an error occurred
 *	while compiling the pattern, then NULL is returned and an error
 *	message is left in the interp's result.
 *
 * Side effects:
 *	Updates the cache of compiled regexps.
 *
 *----------------------------------------------------------------------
 */

Tcl_RegExp
Tcl_RegExpCompile(interp, string)
    Tcl_Interp *interp;			/* For use in error reporting. */
    char *string;			/* String for which to produce
					 * compiled regular expression. */
{
    Interp *iPtr = (Interp *) interp;
    int i, length;
    TclRegexp *result;

    length = strlen(string);
    for (i = 0; i < NUM_REGEXPS; i++) {
	if ((length == iPtr->patLengths[i])
		&& (strcmp(string, iPtr->patterns[i]) == 0)) {
	    /*
	     * Move the matched pattern to the first slot in the
	     * cache and shift the other patterns down one position.
	     */

	    if (i != 0) {
		int j;
		char *cachedString;

		cachedString = iPtr->patterns[i];
		result = iPtr->regexps[i];
		for (j = i-1; j >= 0; j--) {
		    iPtr->patterns[j+1] = iPtr->patterns[j];
		    iPtr->patLengths[j+1] = iPtr->patLengths[j];
		    iPtr->regexps[j+1] = iPtr->regexps[j];
		}
		iPtr->patterns[0] = cachedString;
		iPtr->patLengths[0] = length;
		iPtr->regexps[0] = result;
	    }
	    return (Tcl_RegExp) iPtr->regexps[0];
	}
    }

    /*
     * No match in the cache.  Compile the string and add it to the
     * cache.
     */

    result = CompileRegexp(interp, string, length, REG_ADVANCED);
    if (!result) {
	return NULL;
    }

    /*
     * We successfully compiled the expression, so add it to the cache.
     */

    if (iPtr->patterns[NUM_REGEXPS-1] != NULL) {
	ckfree(iPtr->patterns[NUM_REGEXPS-1]);
	TclReFree(&(iPtr->regexps[NUM_REGEXPS-1]->re));
	ckfree((char *) iPtr->regexps[NUM_REGEXPS-1]);
    }
    for (i = NUM_REGEXPS - 2; i >= 0; i--) {
	iPtr->patterns[i+1] = iPtr->patterns[i];
	iPtr->patLengths[i+1] = iPtr->patLengths[i];
	iPtr->regexps[i+1] = iPtr->regexps[i];
    }
    iPtr->patterns[0] = (char *) ckalloc((unsigned) (length+1));
    strcpy(iPtr->patterns[0], string);
    iPtr->patLengths[0] = length;
    iPtr->regexps[0] = result;
    return (Tcl_RegExp) result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpExec --
 *
 *	Execute the regular expression matcher using a compiled form
 *	of a regular expression and save information about any match
 *	that is found.
 *
 * Results:
 *	If an error occurs during the matching operation then -1
 *	is returned and the interp's result contains an error message.
 *	Otherwise the return value is 1 if a matching range is
 *	found and 0 if there is no matching range.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RegExpExec(interp, re, string, start)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tcl_RegExp re;		/* Compiled regular expression;  must have
				 * been returned by previous call to
				 * Tcl_RegExpCompile or TclRegCompObj. */
    CONST char *string;		/* String against which to match re. */
    CONST char *start;		/* If string is part of a larger string,
				 * this identifies beginning of larger
				 * string, so that "^" won't match. */
{
    int result, numChars;
    Tcl_DString stringBuffer;
    Tcl_UniChar *uniString;

    TclRegexp *regexpPtr = (TclRegexp *) re;

    /*
     * Remember the UTF-8 string so Tcl_RegExpRange() can convert the
     * matches from character to byte offsets.
     */

    regexpPtr->string = string;

    Tcl_DStringInit(&stringBuffer);
    uniString = TclUtfToUniCharDString(string, -1, &stringBuffer);
    numChars = Tcl_DStringLength(&stringBuffer) / sizeof(Tcl_UniChar);

    /*
     * Perform the regexp match.
     */

    result = TclRegExpExecUniChar(interp, re, uniString, numChars, -1,
	    ((string > start) ? REG_NOTBOL : 0));

    Tcl_DStringFree(&stringBuffer);

    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_RegExpRange --
 *
 *	Returns pointers describing the range of a regular expression match,
 *	or one of the subranges within the match.
 *
 * Results:
 *	The variables at *startPtr and *endPtr are modified to hold the
 *	addresses of the endpoints of the range given by index.  If the
 *	specified range doesn't exist then NULLs are returned.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

void
Tcl_RegExpRange(re, index, startPtr, endPtr)
    Tcl_RegExp re;		/* Compiled regular expression that has
				 * been passed to Tcl_RegExpExec. */
    int index;			/* 0 means give the range of the entire
				 * match, > 0 means give the range of
				 * a matching subrange. */
    char **startPtr;		/* Store address of first character in
				 * (sub-) range here. */
    char **endPtr;		/* Store address of character just after last
				 * in (sub-) range here. */
{
    TclRegexp *regexpPtr = (TclRegexp *) re;

    if ((size_t) index > regexpPtr->re.re_nsub) {
	*startPtr = *endPtr = NULL;
    } else if (regexpPtr->matches[index].rm_so < 0) {
	*startPtr = *endPtr = NULL;
    } else {
	*startPtr = Tcl_UtfAtIndex(regexpPtr->string,
		regexpPtr->matches[index].rm_so);
	*endPtr = Tcl_UtfAtIndex(regexpPtr->string,
		regexpPtr->matches[index].rm_eo);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * TclRegExpExecUniChar --
 *
 *	Execute the regular expression matcher using a compiled form of a
 *	regular expression and save information about any match that is
 *	found.
 *
 * Results:
 *	If an error occurs during the matching operation then -1 is
 *	returned and an error message is left in interp's result.
 *	Otherwise the return value is 1 if a matching range was found or
 *	0 if there was no matching range.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclRegExpExecUniChar(interp, re, wString, numChars, nmatches, flags)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tcl_RegExp re;		/* Compiled regular expression; returned by
				 * a previous call to Tcl_RegExpCompile() or
				 * TclRegCompObj(). */
    CONST Tcl_UniChar *wString;	/* String against which to match re. */
    int numChars;		/* Length of string in Tcl_UniChars (must
				 * be >= 0). */
    int nmatches;		/* How many subexpression matches (counting
				 * the whole match as subexpression 0) are
				 * of interest.  -1 means "don't know". */
    int flags;			/* Regular expression flags. */
{
    int status;
    TclRegexp *regexpPtr = (TclRegexp *) re;
    size_t nm = regexpPtr->re.re_nsub + 1;

    if (nmatches >= 0 && (size_t) nmatches < nm)
	nm = (size_t) nmatches;

    status = TclReExec(&regexpPtr->re, wString, (size_t) numChars,
	    (rm_detail_t *)NULL, nm, regexpPtr->matches, flags);

    /*
     * Check for errors.
     */

    if (status != REG_OKAY) {
	if (status == REG_NOMATCH) {
	    return 0;
	}
	if (interp != NULL) {
	    TclRegError(interp, "error while matching regular expression: ",
		    status);
	}
	return -1;
    }
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclRegExpRangeUniChar --
 *
 *	Returns pointers describing the range of a regular expression match,
 *	or one of the subranges within the match.
 *
 * Results:
 *	The variables at *startPtr and *endPtr are modified to hold the
 *	addresses of the endpoints of the range given by index.  If the
 *	specified range doesn't exist then NULLs are returned.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

void
TclRegExpRangeUniChar(re, index, startPtr, endPtr)
    Tcl_RegExp re;		/* Compiled regular expression that has
				 * been passed to Tcl_RegExpExec. */
    int index;			/* 0 means give the range of the entire
				 * match, > 0 means give the range of
				 * a matching subrange. */
    int *startPtr;		/* Store address of first character in
				 * (sub-) range here. */
    int *endPtr;		/* Store address of character just after last
				 * in (sub-) range here. */
{
    TclRegexp *regexpPtr = (TclRegexp *) re;

    if ((size_t) index > regexpPtr->re.re_nsub) {
	*startPtr = -1;
	*endPtr = -1;
    } else {
	*startPtr = regexpPtr->matches[index].rm_so;
	*endPtr = regexpPtr->matches[index].rm_eo;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpMatch --
 *
 *	See if a string matches a regular expression.
 *
 * Results:
 *	If an error occurs during the matching operation then -1
 *	is returned and the interp's result contains an error message.
 *	Otherwise the return value is 1 if "string" matches "pattern"
 *	and 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RegExpMatch(interp, string, pattern)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* String. */
    char *pattern;		/* Regular expression to match against
				 * string. */
{
    Tcl_RegExp re;

    re = Tcl_RegExpCompile(interp, pattern);
    if (re == NULL) {
	return -1;
    }
    return Tcl_RegExpExec(interp, re, string, string);
}

/*
 *----------------------------------------------------------------------
 *
 * TclRegExpMatchObj --
 *
 *	See if a string matches a regular expression pattern object.
 *
 * Results:
 *	If an error occurs during the matching operation then -1
 *	is returned and the interp's result contains an error message.
 *	Otherwise the return value is 1 if "string" matches "pattern"
 *	and 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclRegExpMatchObj(interp, string, patObj)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* String. */
    Tcl_Obj *patObj;		/* Regular expression to match against
				 * string. */
{
    Tcl_RegExp re;

    re = TclRegCompObj(interp, patObj, REG_ADVANCED);
    if (re == NULL) {
	return -1;
    }
    return Tcl_RegExpExec(interp, re, string, string);
}

/*
 *----------------------------------------------------------------------
 *
 * TclRegCompObj --
 *
 *	Compile a regular expression into a form suitable for fast
 *	matching.  This procedure caches the result in a Tcl_Obj.
 *
 * Results:
 *	The return value is a pointer to the compiled form of string,
 *	suitable for passing to Tcl_RegExpExec.  If an error occurred
 *	while compiling the pattern, then NULL is returned and an error
 *	message is left in the interp's result.
 *
 * Side effects:
 *	Updates the native rep of the Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

Tcl_RegExp
TclRegCompObj(interp, objPtr, flags)
    Tcl_Interp *interp;		/* For use in error reporting. */
    Tcl_Obj *objPtr;		/* Object whose string rep contains regular
				 * expression pattern.  Internal rep will be
				 * changed to compiled form of this regular
				 * expression. */
    int flags;			/* Regular expression compilation flags. */
{
    int length;
    Tcl_ObjType *typePtr;
    TclRegexp *regexpPtr;
    char *pattern;

    typePtr = objPtr->typePtr;
    regexpPtr = (TclRegexp *) objPtr->internalRep.otherValuePtr;

    if ((typePtr != &tclRegexpType) || (regexpPtr->flags != flags)) {
	pattern = Tcl_GetStringFromObj(objPtr, &length);
	regexpPtr = CompileRegexp(interp, pattern, length, flags);
	if (regexpPtr == NULL) {
	    return NULL;
	}

	/*
	 * Free the old representation and set our type.
	 */

	if ((typePtr != NULL) && (typePtr->freeIntRepProc != NULL)) {
	    (*typePtr->freeIntRepProc)(objPtr);
	}
	objPtr->internalRep.otherValuePtr = (VOID *) regexpPtr;
	objPtr->typePtr = &tclRegexpType;
    }
    return (Tcl_RegExp) regexpPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclRegAbout --
 *
 *	Return information about a compiled regular expression.
 *
 * Results:
 *	The return value is -1 for failure, 0 for success, although at
 *	the moment there's nothing that could fail.  On success, a list
 *	is left in the interp's result:  first element is the subexpression
 *	count, second is a list of re_info bit names.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclRegAbout(interp, re)
    Tcl_Interp *interp;		/* For use in variable assignment. */
    Tcl_RegExp re;		/* The compiled regular expression. */
{
    TclRegexp *regexpPtr = (TclRegexp *)re;
    char buf[TCL_INTEGER_SPACE];
    static struct infoname {
	int bit;
	char *text;
    } infonames[] = {
	{REG_UBACKREF,		"REG_UBACKREF"},
	{REG_ULOOKAHEAD,	"REG_ULOOKAHEAD"},
	{REG_UBOUNDS,		"REG_UBOUNDS"},
	{REG_UBRACES,		"REG_UBRACES"},
	{REG_UBSALNUM,		"REG_UBSALNUM"},
	{REG_UPBOTCH,		"REG_UPBOTCH"},
	{REG_UBBS,		"REG_UBBS"},
	{REG_UNONPOSIX,		"REG_UNONPOSIX"},
	{REG_UUNSPEC,		"REG_UUNSPEC"},
	{REG_UUNPORT,		"REG_UUNPORT"},
	{REG_ULOCALE,		"REG_ULOCALE"},
	{REG_UEMPTYMATCH,	"REG_UEMPTYMATCH"},
	{REG_UIMPOSSIBLE,	"REG_UIMPOSSIBLE"},
	 {0,			""}
    };
    struct infoname *inf;
    int n;

    Tcl_ResetResult(interp);

    sprintf(buf, "%u", (unsigned)(regexpPtr->re.re_nsub));
    Tcl_AppendElement(interp, buf);

    /*
     * Must count bits before generating list, because we must know
     * whether {} are needed before we start appending names.
     */
    n = 0;
    for (inf = infonames; inf->bit != 0; inf++) {
	if (regexpPtr->re.re_info&inf->bit) {
	    n++;
	}
    }
    if (n != 1) {
	Tcl_AppendResult(interp, " {", NULL);
    }
    for (inf = infonames; inf->bit != 0; inf++) {
	if (regexpPtr->re.re_info&inf->bit) {
	    Tcl_AppendElement(interp, inf->text);
	}
    }
    if (n != 1) {
	Tcl_AppendResult(interp, "}", NULL);
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclRegError --
 *
 *	Generate an error message based on the regexp status code.
 *
 * Results:
 *	Places an error in the interpreter.
 *
 * Side effects:
 *	Sets errorCode as well.
 *
 *----------------------------------------------------------------------
 */

void
TclRegError(interp, msg, status)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    char *msg;			/* Message to prepend to error. */
    int status;			/* Status code to report. */
{
    char buf[100];		/* ample in practice */
    char cbuf[100];		/* lots in practice */
    size_t n;
    char *p;

    Tcl_ResetResult(interp);
    n = TclReError(status, (regex_t *)NULL, buf, sizeof(buf));
    p = (n > sizeof(buf)) ? "..." : "";
    Tcl_AppendResult(interp, msg, buf, p, NULL);

    sprintf(cbuf, "%d", status);
    (VOID) TclReError(REG_ITOA, (regex_t *)NULL, cbuf, sizeof(cbuf));
    Tcl_SetErrorCode(interp, "REGEXP", cbuf, buf, NULL);
}


/*
 *----------------------------------------------------------------------
 *
 * FreeRegexpInternalRep --
 *
 *	Deallocate the storage associated with a regexp object's internal
 *	representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the compiled regular expression.
 *
 *----------------------------------------------------------------------
 */

static void
FreeRegexpInternalRep(objPtr)
    Tcl_Obj *objPtr;		/* Regexp object with internal rep to free. */
{
    TclRegexp *regexpRepPtr = (TclRegexp *) objPtr->internalRep.otherValuePtr;

    TclReFree(&regexpRepPtr->re);
    if (regexpRepPtr->matches) {
	ckfree((char *) regexpRepPtr->matches);
    }
    ckfree((char *) regexpRepPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DupRegexpInternalRep --
 *
 *	It is way too hairy to copy a regular expression, so we punt
 *	and revert the object back to a vanilla string. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the type back to string.
 *
 *----------------------------------------------------------------------
 */

static void
DupRegexpInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr;		/* Object with internal rep to set. */
{
    copyPtr->internalRep.longValue = (long)copyPtr->length;
    copyPtr->typePtr = &tclStringType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetRegexpFromAny --
 *
 *	Attempt to generate a compiled regular expression for the Tcl object
 *	"objPtr".
 *
 * Results:
 *	The return value is TCL_OK or TCL_ERROR. If an error occurs during
 *	conversion, an error message is left in the interpreter's result
 *	unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, a regular expression is stored as "objPtr"s internal
 *	representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetRegexpFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
{
    if (TclRegCompObj(interp, objPtr, REG_ADVANCED) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * CompileRegexp --
 *
 *	Attempt to compile the given regexp pattern
 *
 * Results:
 *	The return value is a pointer to a newly allocated TclRegexp
 *	that represents the compiled pattern, or NULL if the pattern
 *	could not be compiled.  If NULL is returned, an error message is
 *	left in the interp's result.
 *
 * Side effects:
 *	Memory allocated.
 *
 *----------------------------------------------------------------------
 */

static TclRegexp *
CompileRegexp(interp, string, length, flags)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    char *string;		/* The regexp to compile (UTF-8). */
    int length;			/* The length of the string in bytes. */
    int flags;			/* Compilation flags. */
{
    TclRegexp *regexpPtr;
    Tcl_UniChar *uniString;
    int numChars;
    Tcl_DString stringBuf;
    int status;

    regexpPtr = (TclRegexp *) ckalloc(sizeof(TclRegexp));

    /*
     * Get the up-to-date string representation and map to unicode.
     */

    Tcl_DStringInit(&stringBuf);
    uniString = TclUtfToUniCharDString(string, length, &stringBuf);
    numChars = Tcl_DStringLength(&stringBuf) / sizeof(Tcl_UniChar);

    /*
     * Compile the string and check for errors.
     */

    regexpPtr->flags = flags;
    status = TclReComp(&regexpPtr->re, uniString, (size_t) numChars, flags);
    Tcl_DStringFree(&stringBuf);

    if (status != REG_OKAY) {
	/*
	 * Clean up and report errors in the interpreter, if possible.
	 */
	ckfree((char *)regexpPtr);
	if (interp) {
	    TclRegError(interp,
		    "couldn't compile regular expression pattern: ",
		    status);
	}
	return NULL;
    }

    /*
     * Allocate enough space for all of the subexpressions, plus one
     * extra for the entire pattern.
     */

    regexpPtr->matches = (regmatch_t *) ckalloc(
	    sizeof(regmatch_t) * (regexpPtr->re.re_nsub + 1));

    return regexpPtr;
}
