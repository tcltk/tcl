/* 
 * tclRegexp.c --
 *
 *	This file contains the public interfaces to the Tcl regular
 *	expression mechanism.
 *
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclRegexp.c 1.12 98/01/28 20:45:04
 */

#include "tclInt.h"
#include "tclPort.h"
#include "tclRegexp.h"

/*
 *----------------------------------------------------------------------
 * The routines in this file use Henry Spencer's regular expression
 * package contained in the following additional source files:
 *
 *	chr.h		tclRegexp.h	lex.c
 *	guts.h		color.c		locale.c
 *	wchar.h		compile.c	nfa.c
 *	wctype.h	exec.c
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 *
 * *** NOTE: this code has been altered slightly for use in Tcl: ***
 * *** 1. Use ckalloc, ckfree, and ckrealloc instead of malloc,	 ***
 * ***    free, and realloc.					 ***
 * *** 2. Add extra argument to regexp to specify the real	 ***
 * ***    start of the string separately from the start of the	 ***
 * ***    current search. This is needed to search for multiple	 ***
 * ***    matches within a string.				 ***
 * *** 3. Names have been changed, e.g. from re_comp to		 ***
 * ***    TclRegComp, to avoid clashes with other 		 ***
 * ***    regexp implementations used by applications. 		 ***
 * *** 4. Various lint-like things, such as casting arguments	 ***
 * ***	  in procedure calls, removing debugging code and	 ***
 * ***	  unused vars and procs.	 	 	 	 ***
 * *** 5. Removed "backward-compatibility kludges" such as	 ***
 * ***	  REG_PEND and REG_STARTEND flags, the re_endp field in	 ***
 * ***	  the regex_t struct, and the fronts.c layer.		 ***
 * *** 6. Changed char to Tcl_UniChar.				 ***
 * *** 7. Removed -DPOSIX_MISTAKE compile-time flag.		 ***
 * ***	  This is now the default.				 ***
 * *** 8. For performance considerations, created new Unicode    ***
 * ***    interfaces to avoid having to convert between UTF and  ***
 * ***	  Unicode when we already had the Unicode string.  For   ***
 * ***    example, in a "regsub -all" we were converting the     ***
 * ***    match string to Unicode N times, where N is the number ***
 * ***    of bytes in the source string.                         ***
 * *** 9. Changed/widened some of the interfaces to allow	 ***
 * ***	  explicit passing of flags so that tclTest.c could      ***
 * ***    test the full range of acceptable flags.               ***
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
	regfree(&(iPtr->regexps[NUM_REGEXPS-1]->re));
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

    result = TclRegExpExecUniChar(interp, re, uniString, numChars,
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
TclRegExpExecUniChar(interp, re, wString, numChars, flags)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tcl_RegExp re;		/* Compiled regular expression; returned by
				 * a previous call to Tcl_RegExpCompile() or
				 * TclRegCompObj(). */
    CONST Tcl_UniChar *wString;	/* String against which to match re. */
    int numChars;		/* Length of string in Tcl_UniChars (must
				 * be >= 0). */
    int flags;			/* Regular expression flags. */
{
    int status;
    TclRegexp *regexpPtr = (TclRegexp *) re;

    status = re_uexec(&regexpPtr->re, wString, (size_t) numChars,
	    regexpPtr->re.re_nsub + 1, regexpPtr->matches, flags);

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
 * TclRegError --
 *
 *	Generate an error message based on the regexp status code.
 *
 * Results:
 *	Places an error in the interpreter.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclRegError(interp, msg, status)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    char *msg;			/* Message to prepend to error. */
    int status;			/* Status code to report. */
{
    char *errMsg;

    switch(status) {
	case REG_BADPAT:
	    errMsg = "invalid regular expression";
	    break;
	case REG_ECOLLATE:
	    errMsg = "invalid collating element";
	    break;
	case REG_ECTYPE:
	    errMsg = "invalid character class";
	    break;
	case REG_EESCAPE:
	    errMsg = "invalid escape sequence";
	    break;
	case REG_ESUBREG:
	    errMsg = "invalid backreference number";
	    break;
	case REG_EBRACK:
	    errMsg = "unmatched []";
	    break;
	case REG_EPAREN:
	    errMsg = "unmatched ()";
	    break;
	case REG_EBRACE:
	    errMsg = "unmatched {}";
	    break;
	case REG_BADBR:
	    errMsg = "invalid repetition count(s)";
	    break;
	case REG_ERANGE:
	    errMsg = "invalid character range";
	    break;
	case REG_ESPACE:
	    errMsg = "out of memory";
	    break;
	case REG_BADRPT:
	    errMsg = "?+* follows nothing";
	    break;
	case REG_ASSERT:
	    errMsg = "\"can't happen\" -- you found a bug";
	    break;
	case REG_INVARG:
	    errMsg = "invalid argument to regex routine";
	    break;
	case REG_MIXED:
	    errMsg = "char RE applied to wchar_t string (etc.)";
	    break;
	case REG_BADOPT:
	    errMsg = "invalid embedded option";
	    break;
	case REG_IMPOSS:
	    errMsg = "can never match";
	    break;
	default:
	    errMsg = "\"can't happen\" -- you found an undefined error code";
	    break;
    }
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, msg, errMsg, NULL);
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

    regfree(&regexpRepPtr->re);
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
 *	It is way to hairy to copy a regular expression, so we punt
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
    status = re_ucomp(&regexpPtr->re, uniString, (size_t) numChars, flags);
    Tcl_DStringFree(&stringBuf);

    if (status != REG_OKAY) {
	/*
	 * Warning, the following is a hack to allow empty regexp.
	 * The goal is to compile a non-empty regexp that will always
	 * find one empty match.  If you use "(?:)" (an empty pair of
	 * non-capturing parentheses) instead, that will avoid both the
	 * overhead and the subexpression report.
	 */
	
	if (status == REG_EMPTY) {
	    static Tcl_UniChar uniEmpty[] = {'(', '?', ':', ')', '\0'};
	    
	    uniString = uniEmpty;
	    numChars = 4;
	    status = re_ucomp(&regexpPtr->re, uniString, (size_t) numChars,
		    REG_ADVANCED);
	}

	/*
	 * Clean up and report errors in the interpreter, if possible.
	 */

	if (status != REG_OKAY) {
	    regfree(&regexpPtr->re);
	    ckfree((char *)regexpPtr);
	    if (interp) {
		TclRegError(interp,
			"couldn't compile regular expression pattern: ",
			status);
	    }
	    return NULL;
	}
    }

    /*
     * Allocate enough space for all of the subexpressions, plus one
     * extra for the entire pattern.
     */

    regexpPtr->matches = (regmatch_t *) ckalloc(
	    sizeof(regmatch_t) * (regexpPtr->re.re_nsub + 1));

    return regexpPtr;
}

