/* 
 * tclParse.c --
 *
 *	This file contains procedures that parse Tcl scripts.  They
 *	do so in a general-purpose fashion that can be used for many
 *	different purposes, including compilation, direct execution,
 *	code analysis, etc.  
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 Ajuba Solutions.
 * Contributions from Don Porter, NIST, 2002. (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclParse.c,v 1.27.2.1 2003/05/22 19:12:07 dgp Exp $
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The following table provides parsing information about each possible
 * 8-bit character.  The table is designed to be referenced with either
 * signed or unsigned characters, so it has 384 entries.  The first 128
 * entries correspond to negative character values, the next 256 correspond
 * to positive character values.  The last 128 entries are identical to the
 * first 128.  The table is always indexed with a 128-byte offset (the 128th
 * entry corresponds to a character value of 0).
 *
 * The macro CHAR_TYPE is used to index into the table and return
 * information about its character argument.  The following return
 * values are defined.
 *
 * TYPE_NORMAL -        All characters that don't have special significance
 *                      to the Tcl parser.
 * TYPE_SPACE -         The character is a whitespace character other
 *                      than newline.
 * TYPE_COMMAND_END -   Character is newline or semicolon.
 * TYPE_SUBS -          Character begins a substitution or has other
 *                      special meaning in ParseTokens: backslash, dollar
 *                      sign, or open bracket.
 * TYPE_QUOTE -         Character is a double quote.
 * TYPE_CLOSE_PAREN -   Character is a right parenthesis.
 * TYPE_CLOSE_BRACK -   Character is a right square bracket.
 * TYPE_BRACE -         Character is a curly brace (either left or right).
 */

#define TYPE_NORMAL             0
#define TYPE_SPACE              0x1
#define TYPE_COMMAND_END        0x2
#define TYPE_SUBS               0x4
#define TYPE_QUOTE              0x8
#define TYPE_CLOSE_PAREN        0x10
#define TYPE_CLOSE_BRACK        0x20
#define TYPE_BRACE              0x40

#define CHAR_TYPE(c) (charTypeTable+128)[(int)(c)]

static CONST char charTypeTable[] = {
    /*
     * Negative character values, from -128 to -1:
     */

    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,

    /*
     * Positive character values, from 0-127:
     */

    TYPE_SUBS,        TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_SPACE,       TYPE_COMMAND_END, TYPE_SPACE,
    TYPE_SPACE,       TYPE_SPACE,       TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_SPACE,       TYPE_NORMAL,      TYPE_QUOTE,       TYPE_NORMAL,
    TYPE_SUBS,        TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_CLOSE_PAREN, TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_COMMAND_END,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_SUBS,
    TYPE_SUBS,        TYPE_CLOSE_BRACK, TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_BRACE,
    TYPE_NORMAL,      TYPE_BRACE,       TYPE_NORMAL,      TYPE_NORMAL,

    /*
     * Large unsigned character values, from 128-255:
     */

    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
    TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,      TYPE_NORMAL,
};

/* Set of parsing error messages */

CONST char *tclParseErrorMsg[] = {
    "",
    "extra characters after close-quote",
    "extra characters after close-brace",
    "missing close-brace",
    "missing close-bracket",
    "missing )",
    "missing \"",
    "missing close-brace for variable name",
    "syntax error in expression",
    "bad number in expression"
};

/*
 * Prototypes for local procedures defined in this file:
 */

static int		ParseBraces _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *string, int numBytes,
			    Tcl_Parse *parsePtr, int flags,
			    CONST char **termPtr));
static int		ParseCommand _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *string, int numBytes, int flags,
			    Tcl_Parse *parsePtr));
static int		ParseComment _ANSI_ARGS_((CONST char *src, int numBytes,
			    Tcl_Parse *parsePtr));
static int		ParseQuotedString _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *string, int numBytes,
			    Tcl_Parse *parsePtr, int flags,
			    CONST char **termPtr));
static int		ParseTokens _ANSI_ARGS_((CONST char *src, int numBytes,
			    int mask, int flags, Tcl_Parse *parsePtr));
static int		ParseVarName _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *string, int numBytes,
			    Tcl_Parse *parsePtr, int flags));

/*
 * Prototypes for the Tokens object type.
 */

static void             DupTokensInternalRep _ANSI_ARGS_((Tcl_Obj *objPtr,
                            Tcl_Obj *copyPtr));
static void             FreeTokensInternalRep _ANSI_ARGS_((
                            Tcl_Obj *objPtr));
static int              SetTokensFromAny _ANSI_ARGS_((Tcl_Interp *interp,
                            Tcl_Obj *objPtr));

/*
 * The structure below defines the "tokens" Tcl object type.
 */

Tcl_ObjType tclTokensType = {
    "tokens",                           /* name */
    FreeTokensInternalRep,              /* freeIntRepProc */
    DupTokensInternalRep,               /* dupIntRepProc */
    (Tcl_UpdateStringProc *) NULL,      /* updateStringProc */
    SetTokensFromAny                   /* setFromAnyProc */
};


/*
 *----------------------------------------------------------------------
 *
 * FreeTokensInternalRep --
 *
 *      Frees the resources associated with a tokens object's internal
 *      representation.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Frees the cached Tcl_Token array.
 *
 *----------------------------------------------------------------------
 */

static void
FreeTokensInternalRep(objPtr)
    Tcl_Obj *objPtr;
{
    /* Free the Tcl_Token array */
    ckfree((char *) objPtr->internalRep.twoPtrValue.ptr1);
}

/*
 *----------------------------------------------------------------------
 *
 * DupTokensInternalRep --
 *
 *      Do not copy the internal Tcl_Token array, because it contains
 *      pointers into the original string rep.  Instead, leave the copied
 *      Tcl_Obj untyped with only the string value.  If the new copied
 *      value gets used as a script, new parsing will be done to produce
 *      a new Tcl_Token array intrep tied to the copied string.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
DupTokensInternalRep(srcPtr, dupPtr)
    Tcl_Obj *srcPtr;            /* Object with internal rep to copy. */
    Tcl_Obj *dupPtr;            /* Object with internal rep to set. */
{
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * SetTokensFromAny --
 *
 *      Generates an internal representation, an array of Tcl_Token's,
 *      by parsing the string representation as a Tcl script.
 *
 * Results:
 *      Returns TCL_OK.  (Parsing always succeeds, in the sense that
 *      a sequence of Tcl_Token's is always generated.  Parse errors
 *      get represented by a special Tcl_Token type.)
 *
 * Side effects:
 *      Frees the old internal representation.  Sets the first pointer
 *      of the twoPtrValue field of the internal rep to a (Tcl_Token *)
 *      pointing to an array of Tcl_Token's from the parse, and the
 *      second pointer to point to the last token in the array.
 *
 *----------------------------------------------------------------------
 */

static int
SetTokensFromAny (interp, objPtr)
    Tcl_Interp *interp;	/* Not used. */
    Tcl_Obj *objPtr;	/* Value for which to generate Tcl_Token array by
			 * parsing the string value */
{
    int numBytes;
    CONST char *script = Tcl_GetStringFromObj(objPtr, &numBytes);

    /*
     * Free the old internal rep, parse the string as a Tcl script, and
     * save the Tcl_Token array as the new internal rep
     */

    if ((objPtr->typePtr != NULL) 
	    && (objPtr->typePtr->freeIntRepProc != NULL)) {
	(*objPtr->typePtr->freeIntRepProc)(objPtr);
    }
    objPtr->internalRep.twoPtrValue.ptr1 = 
	    (VOID *) TclParseScript(script, numBytes, 0, 
	    (Tcl_Token **) &(objPtr->internalRep.twoPtrValue.ptr2), NULL);
    objPtr->typePtr = &tclTokensType;
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * TclGetTokensFromObj --
 *
 *      Returns a Tcl_Token sequence derived from parsing a Tcl_Obj.
 *
 * Results:
 *      Parses the string rep of the Tcl_Obj, if not already done.
 *
 * Side effects:
 *      Initializes the table of defined object types "typeTable" with
 *      builtin object types defined in this file.
 *
 *-------------------------------------------------------------------------
 */

Tcl_Token *
TclGetTokensFromObj(objPtr,lastTokenPtrPtr)
    Tcl_Obj *objPtr;   		 /* Value to parse and return tokens for */
    Tcl_Token **lastTokenPtrPtr; /* If not NULL, fill with pointer to last
				  * token in the token array */
{
    if (objPtr->typePtr != &tclTokensType) {
	SetTokensFromAny(NULL, objPtr);
    }
    if (lastTokenPtrPtr != NULL) {
	*lastTokenPtrPtr = (Tcl_Token *) objPtr->internalRep.twoPtrValue.ptr2;
    }
    return (Tcl_Token *) objPtr->internalRep.twoPtrValue.ptr1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclParseScript --
 *
 * Results:
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

Tcl_Token *
TclParseScript(script, numBytes, flags, lastTokenPtrPtr, termPtr)
    CONST char *script;		/* The string to parse */
    int numBytes;		/* Length of string in bytes */
    int flags;			/* Bit flags that control parsing details. */
    Tcl_Token **lastTokenPtrPtr;/* Return pointer to last token */
    CONST char **termPtr;	/* Return the terminating character in string */
{
    Tcl_Token tokens[NUM_STATIC_TOKENS];
    int tokensAvailable = NUM_STATIC_TOKENS;
    Tcl_Token *tokensPtr = tokens;
    Tcl_Token *result;
    int numTokens = 0;
    Tcl_Parse parse;
    CONST char *p, *end;
    int nested = (flags & PARSE_NESTED);
    if (numBytes < 0) {
	numBytes = strlen(script);
    }

    tokens[0].type = TCL_TOKEN_SCRIPT;
    tokens[0].start = script;
    tokens[0].size = numBytes;
    tokens[0].numComponents = 0;
    numTokens++;

    p = script;
    end = p + numBytes;
    parse.term = end;
    parse.tokenPtr = parse.staticTokens;
    parse.errorType = nested ? TCL_PARSE_MISSING_BRACKET : TCL_PARSE_SUCCESS;
    while ((p < end) && (TCL_OK == ParseCommand((Tcl_Interp *)NULL, p, end - p,
		flags | PARSE_USE_INTERNAL_TOKENS, &parse))) {

	/*
	 * Check for missing close-brace for nested script substitution.
	 * If close-brace is missing, blame it on the last command parsed,
	 * and do not add it to the token array.
	 */

	if (nested && (parse.term >= end)) {
	    parse.errorType = TCL_PARSE_MISSING_BRACKET;
	    break;
	}

	/*
	 * Copy the Tcl_Tokens for the parsed command into the
	 * Tcl_Token array.  Expand as needed.
	 */

	tokensPtr[0].numComponents++;	/* Another command parsed */

	TclGrowTokenArray(tokensPtr, numTokens, tokensAvailable,
		parse.numTokens+1, tokens);
	tokensPtr[numTokens].type = TCL_TOKEN_CMD;
	tokensPtr[numTokens].start = parse.commandStart;
	if (parse.commandStart + parse.commandSize == parse.term) {
	    tokensPtr[numTokens].size = parse.commandSize;
	} else {
	    tokensPtr[numTokens].size = parse.commandSize - 1;
	}
	tokensPtr[numTokens].numComponents = parse.numWords;
	numTokens++;

	memcpy(&(tokensPtr[numTokens]), parse.tokenPtr,
		(size_t) (parse.numTokens * sizeof(Tcl_Token)));
	numTokens += parse.numTokens;

	p = parse.commandStart + parse.commandSize;
	Tcl_FreeParse(&parse);

	if (nested && (*parse.term == ']') && (parse.term < end)) {
	    break;
	}
    }

    if ((parse.errorType != TCL_PARSE_SUCCESS)) {
	Tcl_FreeParse(&parse);
	TclGrowTokenArray(tokensPtr, numTokens, tokensAvailable, 1, tokens);
	tokensPtr[numTokens].type = TCL_TOKEN_ERROR;
	tokensPtr[numTokens].start = parse.commandStart;
	tokensPtr[numTokens].size = end - parse.commandStart;
	tokensPtr[numTokens].numComponents = parse.errorType;
	numTokens++;
    }

    if (termPtr != NULL) {
	*termPtr = parse.term;
    }
    if (tokensPtr != tokens) {
	result = (Tcl_Token *) ckrealloc((VOID *)tokensPtr,
		(unsigned int) (numTokens * sizeof(Tcl_Token)));
    } else {
	result = (Tcl_Token *)
		ckalloc((unsigned int) (numTokens * sizeof(Tcl_Token)));
	memcpy(result, tokensPtr, (size_t) (numTokens * sizeof(Tcl_Token)));
    }

    if (lastTokenPtrPtr != NULL) {
	*lastTokenPtrPtr = &(result[numTokens - 1]);
    }
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ParseCommand --
 *
 *	Given a string, this procedure parses the first Tcl command
 *	in the string and returns information about the structure of
 *	the command.
 *
 * Results:
 *	The return value is TCL_OK if the command was parsed
 *	successfully and TCL_ERROR otherwise.  If an error occurs
 *	and interp isn't NULL then an error message is left in
 *	its result.  On a successful return, parsePtr is filled in
 *	with information about the command that was parsed.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the
 *	information about the command, then additional space is
 *	malloc-ed.  If the procedure returns TCL_OK then the caller must
 *	eventually invoke Tcl_FreeParse to release any additional space
 *	that was allocated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ParseCommand(interp, string, numBytes, nested, parsePtr)
    Tcl_Interp *interp;		/* See ParseCommand */
    CONST char *string;		/* See ParseCommand */
    register int numBytes;	/* See ParseCommand */
    int nested;			/* Non-zero means this is a nested command:
				 * close bracket should be considered
				 * a command terminator. If zero, then close
				 * bracket has no special meaning. */
    register Tcl_Parse *parsePtr;
    				/* See ParseCommand */
{
    int code = ParseCommand(interp, string, numBytes,
	    (nested != 0) ? PARSE_NESTED : 0, parsePtr);
    if (code == TCL_ERROR) {
	Tcl_FreeParse(parsePtr);
    }
    return code;
}

int
ParseCommand(interp, string, numBytes, flags, parsePtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting;
				 * if NULL, then no error message is
				 * provided. */
    CONST char *string;		/* First character of string containing
				 * one or more Tcl commands. */
    register int numBytes;	/* Total number of bytes in string.  If < 0,
				 * the script consists of all bytes up to 
				 * the first null character. */
    int flags;			/* Bit flags to control details of the parsing.
				 * Only the PARSE_NESTED flag has an effect
				 * here.  Other flags are passed along. */
    register Tcl_Parse *parsePtr;
    				/* Structure to fill in with information
				 * about the parsed command; any previous
				 * information in the structure is
				 * ignored. */
{
    register CONST char *src;	/* Points to current character
				 * in the command. */
    char type;			/* Result returned by CHAR_TYPE(*src). */
    Tcl_Token *tokenPtr;	/* Pointer to token being filled in. */
    int wordIndex;		/* Index of word token for current word. */
    int terminators;		/* CHAR_TYPE bits that indicate the end
				 * of a command. */
    CONST char *termPtr;	/* Set by Tcl_ParseBraces/QuotedString to
				 * point to char after terminating one. */
    int scanned;
    int nested = (flags & PARSE_NESTED);
    
    if ((string == NULL) && (numBytes>0)) {
	if (interp != NULL) {
	    Tcl_SetResult(interp, "can't parse a NULL pointer", TCL_STATIC);
	}
	return TCL_ERROR;
    }
    if (numBytes < 0) {
	numBytes = strlen(string);
    }
    parsePtr->commentStart = NULL;
    parsePtr->commentSize = 0;
    parsePtr->commandStart = NULL;
    parsePtr->commandSize = 0;
    parsePtr->numWords = 0;
    parsePtr->tokenPtr = parsePtr->staticTokens;
    parsePtr->numTokens = 0;
    parsePtr->tokensAvailable = NUM_STATIC_TOKENS;
    parsePtr->string = string;
    parsePtr->end = string + numBytes;
    parsePtr->term = parsePtr->end;
    parsePtr->interp = interp;
    parsePtr->incomplete = 0;
    parsePtr->errorType = TCL_PARSE_SUCCESS;
    if (nested != 0) {
	terminators = TYPE_COMMAND_END | TYPE_CLOSE_BRACK;
    } else {
	terminators = TYPE_COMMAND_END;
    }

    /*
     * Parse any leading space and comments before the first word of the
     * command.
     */

    scanned = ParseComment(string, numBytes, parsePtr);
    src = (string + scanned); numBytes -= scanned;
    if (numBytes == 0) {
	if (nested) {
	    parsePtr->incomplete = nested;
	}
    }

    /*
     * The following loop parses the words of the command, one word
     * in each iteration through the loop.
     */

    parsePtr->commandStart = src;
    while (1) {
	/*
	 * Create the token for the word.
	 */

	TclGrowParseTokenArray(parsePtr,1);
	wordIndex = parsePtr->numTokens;
	tokenPtr = &parsePtr->tokenPtr[wordIndex];
	tokenPtr->type = TCL_TOKEN_WORD;

	/*
	 * Skip white space before the word. Also skip a backslash-newline
	 * sequence: it should be treated just like white space.
	 */

	scanned = TclParseWhiteSpace(src, numBytes, parsePtr, &type);
	src += scanned; numBytes -= scanned;
	if (numBytes == 0) {
	    parsePtr->term = src;
	    break;
	}
	if ((type & terminators) != 0) {
	    parsePtr->term = src;
	    src++;
	    break;
	}
	tokenPtr->start = src;
	parsePtr->numTokens++;
	parsePtr->numWords++;

	/*
	 * At this point the word can have one of three forms: something
	 * enclosed in quotes, something enclosed in braces, or an
	 * unquoted word (anything else).
	 */

	if (*src == '"') {
	    if (ParseQuotedString(interp, src, numBytes,
		    parsePtr, flags | PARSE_APPEND, &termPtr) != TCL_OK) {
		goto error;
	    }
	    src = termPtr; numBytes = parsePtr->end - src;
	} else if (*src == '{') {
	    if (ParseBraces(interp, src, numBytes,
		    parsePtr, flags | PARSE_APPEND, &termPtr) != TCL_OK) {
		goto error;
	    }
	    src = termPtr; numBytes = parsePtr->end - src;
	} else {
	    /*
	     * This is an unquoted word.  Call ParseTokens and let it do
	     * all of the work.
	     */

	    if (ParseTokens(src, numBytes, TYPE_SPACE|terminators,
		    flags | TCL_SUBST_ALL, parsePtr) != TCL_OK) {
		goto error;
	    }
	    src = parsePtr->term; numBytes = parsePtr->end - src;
	}

	/*
	 * Finish filling in the token for the word and check for the
	 * special case of a word consisting of a single range of
	 * literal text.
	 */

	tokenPtr = &parsePtr->tokenPtr[wordIndex];
	tokenPtr->size = src - tokenPtr->start;
	tokenPtr->numComponents = parsePtr->numTokens - (wordIndex + 1);
	if ((tokenPtr->numComponents == 1)
		&& (tokenPtr[1].type == TCL_TOKEN_TEXT)) {
	    tokenPtr->type = TCL_TOKEN_SIMPLE_WORD;
	}

	/*
	 * Do two additional checks: (a) make sure we're really at the
	 * end of a word (there might have been garbage left after a
	 * quoted or braced word), and (b) check for the end of the
	 * command.
	 */

	scanned = TclParseWhiteSpace(src, numBytes, parsePtr, &type);
	if (scanned) {
	    src += scanned; numBytes -= scanned;
	    continue;
	}

	if (numBytes == 0) {
	    parsePtr->term = src;
	    break;
	}
	if ((type & terminators) != 0) {
	    parsePtr->term = src;
	    src++; 
	    break;
	}
	if (src[-1] == '"') { 
	    if (interp != NULL) {
		Tcl_SetResult(interp, "extra characters after close-quote",
			TCL_STATIC);
	    }
	    parsePtr->errorType = TCL_PARSE_QUOTE_EXTRA;
	} else {
	    if (interp != NULL) {
		Tcl_SetResult(interp, "extra characters after close-brace",
			TCL_STATIC);
	    }
	    parsePtr->errorType = TCL_PARSE_BRACE_EXTRA;
	}
	parsePtr->term = src;
	goto error;
    }

    parsePtr->commandSize = src - parsePtr->commandStart;
    return TCL_OK;

    error:
    if (parsePtr->commandStart == NULL) {
	parsePtr->commandStart = string;
    }
    parsePtr->commandSize = parsePtr->end - parsePtr->commandStart;
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclParseWhiteSpace --
 *
 *	Scans up to numBytes bytes starting at src, consuming white
 *	space as defined by Tcl's parsing rules.  
 *
 * Results:
 *	Returns the number of bytes recognized as white space.  Records
 *	at parsePtr, information about the parse.  Records at typePtr
 *	the character type of the non-whitespace character that terminated
 *	the scan.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
TclParseWhiteSpace(src, numBytes, parsePtr, typePtr)
    CONST char *src;		/* First character to parse. */
    register int numBytes;	/* Max number of bytes to scan. */
    Tcl_Parse *parsePtr;	/* Information about parse in progress.
				 * Updated if parsing indicates
				 * an incomplete command. */
    char *typePtr;		/* Points to location to store character
				 * type of character that ends run
				 * of whitespace */
{
    register char type = TYPE_NORMAL;
    register CONST char *p = src;

    while (1) {
	while (numBytes && ((type = CHAR_TYPE(*p)) & TYPE_SPACE)) {
	    numBytes--; p++;
	}
	if (numBytes && (type & TYPE_SUBS)) {
	    if (*p != '\\') {
		break;
	    }
	    if (--numBytes == 0) {
		break;
	    }
	    if (p[1] != '\n') {
		break;
	    }
	    p+=2;
	    if (--numBytes == 0) {
		parsePtr->incomplete = 1;
		break;
	    }
	    continue;
	}
	break;
    }
    *typePtr = type;
    return (p - src);
}

/*
 *----------------------------------------------------------------------
 *
 * TclParseHex --
 *
 *	Scans a hexadecimal number as a Tcl_UniChar value.
 *	(e.g., for parsing \x and \u escape sequences).
 *	At most numBytes bytes are scanned.
 *
 * Results:
 *	The numeric value is stored in *resultPtr.
 *	Returns the number of bytes consumed.
 *
 * Notes:
 *	Relies on the following properties of the ASCII
 *	character set, with which UTF-8 is compatible:
 *
 *	The digits '0' .. '9' and the letters 'A' .. 'Z' and 'a' .. 'z' 
 *	occupy consecutive code points, and '0' < 'A' < 'a'.
 *
 *----------------------------------------------------------------------
 */
int
TclParseHex(src, numBytes, resultPtr)
    CONST char *src;		/* First character to parse. */
    int numBytes;		/* Max number of byes to scan */
    Tcl_UniChar *resultPtr;	/* Points to storage provided by
				 * caller where the Tcl_UniChar
				 * resulting from the conversion is
				 * to be written. */
{
    Tcl_UniChar result = 0;
    register CONST char *p = src;

    while (numBytes--) {
	unsigned char digit = UCHAR(*p);

	if (!isxdigit(digit))
	    break;

	++p;
	result <<= 4;

	if (digit >= 'a') {
	    result |= (10 + digit - 'a');
	} else if (digit >= 'A') {
	    result |= (10 + digit - 'A');
	} else {
	    result |= (digit - '0');
	}
    }

    *resultPtr = result;
    return (p - src);
}

/*
 *----------------------------------------------------------------------
 *
 * TclParseBackslash --
 *
 *	Scans up to numBytes bytes starting at src, consuming a
 *	backslash sequence as defined by Tcl's parsing rules.  
 *
 * Results:
 * 	Records at readPtr the number of bytes making up the backslash
 * 	sequence.  Records at dst the UTF-8 encoded equivalent of
 * 	that backslash sequence.  Returns the number of bytes written
 * 	to dst, at most TCL_UTF_MAX.  Either readPtr or dst may be
 * 	NULL, if the results are not needed, but the return value is
 * 	the same either way.
 *
 * Side effects:
 * 	None.
 *
 *----------------------------------------------------------------------
 */
int
TclParseBackslash(src, numBytes, readPtr, dst)
    CONST char * src;	/* Points to the backslash character of a
			 * a backslash sequence */
    int numBytes;	/* Max number of bytes to scan */
    int *readPtr;	/* NULL, or points to storage where the
			 * number of bytes scanned should be written. */
    char *dst;		/* NULL, or points to buffer where the UTF-8
			 * encoding of the backslash sequence is to be
			 * written.  At most TCL_UTF_MAX bytes will be
			 * written there. */
{
    register CONST char *p = src+1;
    Tcl_UniChar result;
    int count;
    char buf[TCL_UTF_MAX];

    if (numBytes == 0) {
	if (readPtr != NULL) {
	    *readPtr = 0;
	}
	return 0;
    }

    if (dst == NULL) {
        dst = buf;
    }

    if (numBytes == 1) {
	/* Can only scan the backslash.  Return it. */
	result = '\\';
	count = 1;
	goto done;
    }

    count = 2;
    switch (*p) {
        /*
         * Note: in the conversions below, use absolute values (e.g.,
         * 0xa) rather than symbolic values (e.g. \n) that get converted
         * by the compiler.  It's possible that compilers on some
         * platforms will do the symbolic conversions differently, which
         * could result in non-portable Tcl scripts.
         */

        case 'a':
            result = 0x7;
            break;
        case 'b':
            result = 0x8;
            break;
        case 'f':
            result = 0xc;
            break;
        case 'n':
            result = 0xa;
            break;
        case 'r':
            result = 0xd;
            break;
        case 't':
            result = 0x9;
            break;
        case 'v':
            result = 0xb;
            break;
        case 'x':
	    count += TclParseHex(p+1, numBytes-1, &result);
	    if (count == 2) {
		/* No hexadigits -> This is just "x". */
		result = 'x';
	    } else {
		/* Keep only the last byte (2 hex digits) */
		result = (unsigned char) result;
	    }
            break;
        case 'u':
	    count += TclParseHex(p+1, (numBytes > 5) ? 4 : numBytes-1, &result);
	    if (count == 2) {
		/* No hexadigits -> This is just "u". */
		result = 'u';
	    }
            break;
        case '\n':
            count--;
            do {
                p++; count++;
            } while ((count < numBytes) && ((*p == ' ') || (*p == '\t')));
            result = ' ';
            break;
        case 0:
            result = '\\';
            count = 1;
            break;
        default:
            /*
             * Check for an octal number \oo?o?
             */
            if (isdigit(UCHAR(*p)) && (UCHAR(*p) < '8')) { /* INTL: digit */
                result = (unsigned char)(*p - '0');
                p++;
                if ((numBytes == 2) || !isdigit(UCHAR(*p)) /* INTL: digit */
			|| (UCHAR(*p) >= '8')) { 
                    break;
                }
                count = 3;
                result = (unsigned char)((result << 3) + (*p - '0'));
                p++;
                if ((numBytes == 3) || !isdigit(UCHAR(*p)) /* INTL: digit */
			|| (UCHAR(*p) >= '8')) {
                    break;
                }
                count = 4;
                result = (unsigned char)((result << 3) + (*p - '0'));
                break;
            }
            /*
             * We have to convert here in case the user has put a
             * backslash in front of a multi-byte utf-8 character.
             * While this means nothing special, we shouldn't break up
             * a correct utf-8 character. [Bug #217987] test subst-3.2
             */
	    if (Tcl_UtfCharComplete(p, numBytes - 1)) {
	        count = Tcl_UtfToUniChar(p, &result) + 1; /* +1 for '\' */
	    } else {
		char utfBytes[TCL_UTF_MAX];
		memcpy(utfBytes, p, (size_t) (numBytes - 1));
		utfBytes[numBytes - 1] = '\0';
	        count = Tcl_UtfToUniChar(utfBytes, &result) + 1;
	    }
            break;
    }

    done:
    if (readPtr != NULL) {
        *readPtr = count;
    }
    return Tcl_UniCharToUtf((int) result, dst);
}

/*
 *----------------------------------------------------------------------
 *
 * ParseComment --
 *
 *	Scans up to numBytes bytes starting at src, consuming a
 *	Tcl comment as defined by Tcl's parsing rules.  
 *
 * Results:
 * 	Records in parsePtr information about the parse.  Returns the
 * 	number of bytes consumed.
 *
 * Side effects:
 * 	None.
 *
 *----------------------------------------------------------------------
 */
static int
ParseComment(src, numBytes, parsePtr)
    CONST char *src;		/* First character to parse. */
    register int numBytes;	/* Max number of bytes to scan. */
    Tcl_Parse *parsePtr;	/* Information about parse in progress.
				 * Updated if parsing indicates
				 * an incomplete command. */
{
    register CONST char *p = src;
    while (numBytes) {
	char type;
	int scanned;
	do {
	    scanned = TclParseWhiteSpace(p, numBytes, parsePtr, &type);
	    p += scanned; numBytes -= scanned;
	} while (numBytes && (*p == '\n') && (p++,numBytes--));
	if ((numBytes == 0) || (*p != '#')) {
	    break;
	}
	if (parsePtr->commentStart == NULL) {
	    parsePtr->commentStart = p;
	}
	while (numBytes) {
	    if (*p == '\\') {
		scanned = TclParseWhiteSpace(p, numBytes, parsePtr, &type);
		if (scanned) {
		    p += scanned; numBytes -= scanned;
		} else {
		    /*
		     * General backslash substitution in comments isn't
		     * part of the formal spec, but test parse-15.47
		     * and history indicate that it has been the de facto
		     * rule.  Don't change it now.
		     */
		    TclParseBackslash(p, numBytes, &scanned, NULL);
		    p += scanned; numBytes -= scanned;
		}
	    } else {
		p++; numBytes--;
		if (p[-1] == '\n') {
		    break;
		}
	    }
	}
	parsePtr->commentSize = p - parsePtr->commentStart;
    }
    return (p - src);
}

/*
 *----------------------------------------------------------------------
 *
 * ParseTokens --
 *
 *	This procedure forms the heart of the Tcl parser.  It parses one
 *	or more tokens from a string, up to a termination point
 *	specified by the caller.  This procedure is used to parse
 *	unquoted command words (those not in quotes or braces), words in
 *	quotes, and array indices for variables.  No more than numBytes
 *	bytes will be scanned.
 *
 * Results:
 *	Tokens are added to parsePtr and parsePtr->term is filled in
 *	with the address of the character that terminated the parse (the
 *	first one whose CHAR_TYPE matched mask or the character at
 *	parsePtr->end).  The return value is TCL_OK if the parse
 *	completed successfully and TCL_ERROR otherwise.  If a parse
 *	error occurs and parsePtr->interp isn't NULL, then an error
 *	message is left in the interpreter's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ParseTokens(src, numBytes, mask, flags, parsePtr)
    register CONST char *src;	/* First character to parse. */
    register int numBytes;	/* Max number of bytes to scan. */
    int flags;			/* OR-ed bits indicating what substitutions
				   to perform: TCL_SUBST_COMMANDS,
				   TCL_SUBST_VARIABLES, and 
				   TCL_SUBST_BACKSLASHES */
    int mask;			/* Specifies when to stop parsing.  The
				 * parse stops at the first unquoted
				 * character whose CHAR_TYPE contains
				 * any of the bits in mask. */
    Tcl_Parse *parsePtr;	/* Information about parse in progress.
				 * Updated with additional tokens and
				 * termination information. */
{
    char type; 
    int originalTokens, varToken;
    int noSubstCmds = !(flags & TCL_SUBST_COMMANDS);
    int noSubstVars = !(flags & TCL_SUBST_VARIABLES);
    int noSubstBS = !(flags & TCL_SUBST_BACKSLASHES);
    int useInternalTokens = (flags & PARSE_USE_INTERNAL_TOKENS);
    Tcl_Token *tokenPtr;
    Tcl_Parse nested;

    /*
     * Each iteration through the following loop adds one token of
     * type TCL_TOKEN_TEXT, TCL_TOKEN_BS, TCL_TOKEN_COMMAND, or
     * TCL_TOKEN_VARIABLE to parsePtr.  For TCL_TOKEN_VARIABLE tokens,
     * additional tokens are added for the parsed variable name.
     */

    originalTokens = parsePtr->numTokens;
    while (numBytes && !((type = CHAR_TYPE(*src)) & mask)) {
	TclGrowParseTokenArray(parsePtr,1);
	tokenPtr = &parsePtr->tokenPtr[parsePtr->numTokens];
	tokenPtr->start = src;
	tokenPtr->numComponents = 0;

	if ((type & TYPE_SUBS) == 0) {
	    /*
	     * This is a simple range of characters.  Scan to find the end
	     * of the range.
	     */

	    while ((++src, --numBytes) 
		    && !(CHAR_TYPE(*src) & (mask | TYPE_SUBS))) {
		/* empty loop */
	    }
	    tokenPtr->type = TCL_TOKEN_TEXT;
	    tokenPtr->size = src - tokenPtr->start;
	    parsePtr->numTokens++;
	} else if (*src == '$') {
	    if (noSubstVars) {
		tokenPtr->type = TCL_TOKEN_TEXT;
		tokenPtr->size = 1;
		parsePtr->numTokens++;
		src++; numBytes--;
		continue;
	    }
	    /*
	     * This is a variable reference.  Call ParseVarName to do
	     * all the dirty work of parsing the name.
	     */

	    varToken = parsePtr->numTokens;
	    if (ParseVarName(parsePtr->interp, src, numBytes,
		    parsePtr, flags | PARSE_APPEND) != TCL_OK) {
		return TCL_ERROR;
	    }
	    src += parsePtr->tokenPtr[varToken].size;
	    numBytes -= parsePtr->tokenPtr[varToken].size;
	} else if (*src == '[') {
	    if (noSubstCmds) {
		tokenPtr->type = TCL_TOKEN_TEXT;
		tokenPtr->size = 1;
		parsePtr->numTokens++;
		src++; numBytes--;
		continue;
	    }
	    /*
	     * Command substitution.  Call Tcl_ParseCommand recursively
	     * (and repeatedly) to parse the nested command(s).  If
	     * internal tokens are acceptable, keep all the parsing
	     * information; otherwise, throw away the nested parse information.
	     */

	    src++; numBytes--;
	    if (useInternalTokens) {
		CONST char *term;
		Tcl_Token *lastTokenPtr;
		Tcl_Token *appendTokens = TclParseScript(src, numBytes,
			flags | PARSE_NESTED, &lastTokenPtr, &term);
		int numTokens = 1 + (int)(lastTokenPtr - appendTokens);

		TclGrowParseTokenArray(parsePtr,numTokens+1);
		tokenPtr = &parsePtr->tokenPtr[parsePtr->numTokens];
		tokenPtr->type = TCL_TOKEN_SCRIPT_SUBST;
		tokenPtr->size = term - src + 2;
		tokenPtr->numComponents = numTokens;

		memcpy(tokenPtr+1, appendTokens, 
			(size_t) (numTokens * sizeof(Tcl_Token)));
		parsePtr->numTokens += (numTokens + 1);
	
		if (lastTokenPtr->type == TCL_TOKEN_ERROR) {

		    parsePtr->errorType = lastTokenPtr->numComponents;
		    parsePtr->term = term;
		    parsePtr->incomplete = 1;

		    ckfree((char *) appendTokens);
		    return TCL_ERROR;
		}
		ckfree((char *) appendTokens);
		src = term + 1;
		numBytes = parsePtr->end - src;
		continue;
	    }

	    while (1) {
		if (ParseCommand(parsePtr->interp, src,
			numBytes, flags | PARSE_NESTED, &nested) != TCL_OK) {
		    parsePtr->errorType = nested.errorType;
		    parsePtr->term = nested.term;
		    parsePtr->incomplete = nested.incomplete;
		    return TCL_ERROR;
		}
		src = nested.commandStart + nested.commandSize;
		numBytes = parsePtr->end - src;
		Tcl_FreeParse(&nested);

		/*
		 * Check for the closing ']' that ends the command
		 * substitution.  It must have been the last character of
		 * the parsed command.
		 */

		if ((nested.term < parsePtr->end) && (*nested.term == ']')
			&& !nested.incomplete) {
		    break;
		}
		if (numBytes == 0) {
		    if (parsePtr->interp != NULL) {
			Tcl_SetResult(parsePtr->interp,
			    "missing close-bracket", TCL_STATIC);
		    }
		    parsePtr->errorType = TCL_PARSE_MISSING_BRACKET;
		    parsePtr->term = tokenPtr->start;
		    parsePtr->incomplete = 1;
		    return TCL_ERROR;
		}
	    }
	    tokenPtr->type = TCL_TOKEN_COMMAND;
	    tokenPtr->size = src - tokenPtr->start;
	    parsePtr->numTokens++;
	} else if (*src == '\\') {
	    if (noSubstBS) {
		tokenPtr->type = TCL_TOKEN_TEXT;
		tokenPtr->size = 1;
		parsePtr->numTokens++;
		src++; numBytes--;
		continue;
	    }
	    /*
	     * Backslash substitution.
	     */
	    TclParseBackslash(src, numBytes, &tokenPtr->size, NULL);

	    if (tokenPtr->size == 1) {
		/* Just a backslash, due to end of string */
		tokenPtr->type = TCL_TOKEN_TEXT;
		parsePtr->numTokens++;
		src++; numBytes--;
		continue;
	    }

	    if (src[1] == '\n') {
		if (numBytes == 2) {
		    parsePtr->incomplete = 1;
		}

		/*
		 * Note: backslash-newline is special in that it is
		 * treated the same as a space character would be.  This
		 * means that it could terminate the token.
		 */

		if (mask & TYPE_SPACE) {
		    if (parsePtr->numTokens == originalTokens) {
			goto finishToken;
		    }
		    break;
		}
	    }

	    tokenPtr->type = TCL_TOKEN_BS;
	    parsePtr->numTokens++;
	    src += tokenPtr->size;
	    numBytes -= tokenPtr->size;
	} else if (*src == 0) {
	    tokenPtr->type = TCL_TOKEN_TEXT;
	    tokenPtr->size = 1;
	    parsePtr->numTokens++;
	    src++; numBytes--;
	} else {
	    panic("ParseTokens encountered unknown character");
	}
    }
    if (parsePtr->numTokens == originalTokens) {
	/*
	 * There was nothing in this range of text.  Add an empty token
	 * for the empty range, so that there is always at least one
	 * token added.
	 */
	TclGrowParseTokenArray(parsePtr,1);
	tokenPtr = &parsePtr->tokenPtr[parsePtr->numTokens];
	tokenPtr->start = src;
	tokenPtr->numComponents = 0;

	finishToken:
	tokenPtr->type = TCL_TOKEN_TEXT;
	tokenPtr->size = 0;
	parsePtr->numTokens++;
    }
    parsePtr->term = src;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FreeParse --
 *
 *	This procedure is invoked to free any dynamic storage that may
 *	have been allocated by a previous call to Tcl_ParseCommand.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there is any dynamically allocated memory in *parsePtr,
 *	it is freed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FreeParse(parsePtr)
    Tcl_Parse *parsePtr;	/* Structure that was filled in by a
				 * previous call to Tcl_ParseCommand. */
{
    if (parsePtr->tokenPtr != parsePtr->staticTokens) {
	ckfree((char *) parsePtr->tokenPtr);
	parsePtr->tokenPtr = parsePtr->staticTokens;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ParseVarName --
 *
 *	Given a string starting with a $ sign, parse off a variable
 *	name and return information about the parse.  No more than
 *	numBytes bytes will be scanned.
 *
 * Results:
 *	The return value is TCL_OK if the command was parsed
 *	successfully and TCL_ERROR otherwise.  If an error occurs and
 *	interp isn't NULL then an error message is left in its result. 
 *	On a successful return, tokenPtr and numTokens fields of
 *	parsePtr are filled in with information about the variable name
 *	that was parsed.  The "size" field of the first new token gives
 *	the total number of bytes in the variable name.  Other fields in
 *	parsePtr are undefined.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the
 *	information about the command, then additional space is
 *	malloc-ed.  If the procedure returns TCL_OK then the caller must
 *	eventually invoke Tcl_FreeParse to release any additional space
 *	that was allocated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ParseVarName(interp, string, numBytes, parsePtr, append)
    Tcl_Interp *interp;		/* See ParseVarName */
    CONST char *string;		/* See ParseVarName */
    register int numBytes;	/* See ParseVarName */
    Tcl_Parse *parsePtr;	/* See ParseVarName */
    int append;			/* Non-zero means append tokens to existing
				 * information in parsePtr; zero means ignore
				 * existing tokens in parsePtr and reinitialize
				 * it. */
{
    int code = ParseVarName(interp, string, numBytes, parsePtr,
	    (append != 0) ? PARSE_APPEND : 0);
    if (code == TCL_ERROR) {
	Tcl_FreeParse(parsePtr);
    }
    return code;
}

int
ParseVarName(interp, string, numBytes, parsePtr, flags)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting;
				 * if NULL, then no error message is
				 * provided. */
    CONST char *string;		/* String containing variable name.  First
				 * character must be "$". */
    register int numBytes;	/* Total number of bytes in string.  If < 0,
				 * the string consists of all bytes up to the
				 * first null character. */
    Tcl_Parse *parsePtr;	/* Structure to fill in with information
				 * about the variable name. */
    int flags;			/* Bit flags to control details of the parsing.
				 * Only the PARSE_APPEND flag has an effect
				 * here.  Other flags are passed along. */
{
    Tcl_Token *tokenPtr;
    register CONST char *src;
    unsigned char c;
    int varIndex, offset;
    Tcl_UniChar ch;
    unsigned array;
    int append = (flags & PARSE_APPEND);

    if ((numBytes == 0) || (string == NULL)) {
	return TCL_ERROR;
    }
    if (numBytes < 0) {
	numBytes = strlen(string);
    }

    if (!append) {
	parsePtr->numWords = 0;
	parsePtr->tokenPtr = parsePtr->staticTokens;
	parsePtr->numTokens = 0;
	parsePtr->tokensAvailable = NUM_STATIC_TOKENS;
	parsePtr->string = string;
	parsePtr->end = (string + numBytes);
	parsePtr->interp = interp;
	parsePtr->errorType = TCL_PARSE_SUCCESS;
	parsePtr->incomplete = 0;
    }

    /*
     * Generate one token for the variable, an additional token for the
     * name, plus any number of additional tokens for the index, if
     * there is one.
     */

    src = string;
    TclGrowParseTokenArray(parsePtr,2);
    tokenPtr = &parsePtr->tokenPtr[parsePtr->numTokens];
    tokenPtr->type = TCL_TOKEN_VARIABLE;
    tokenPtr->start = src;
    varIndex = parsePtr->numTokens;
    parsePtr->numTokens++;
    tokenPtr++;
    src++; numBytes--;
    if (numBytes == 0) {
	goto justADollarSign;
    }
    tokenPtr->type = TCL_TOKEN_TEXT;
    tokenPtr->start = src;
    tokenPtr->numComponents = 0;

    /*
     * The name of the variable can have three forms:
     * 1. The $ sign is followed by an open curly brace.  Then 
     *    the variable name is everything up to the next close
     *    curly brace, and the variable is a scalar variable.
     * 2. The $ sign is not followed by an open curly brace.  Then
     *    the variable name is everything up to the next
     *    character that isn't a letter, digit, or underscore.
     *    :: sequences are also considered part of the variable
     *    name, in order to support namespaces. If the following
     *    character is an open parenthesis, then the information
     *    between parentheses is the array element name.
     * 3. The $ sign is followed by something that isn't a letter,
     *    digit, or underscore:  in this case, there is no variable
     *    name and the token is just "$".
     */

    if (*src == '{') {
	src++; numBytes--;
	tokenPtr->type = TCL_TOKEN_TEXT;
	tokenPtr->start = src;
	tokenPtr->numComponents = 0;

	while (numBytes && (*src != '}')) {
	    numBytes--; src++;
	}
	if (numBytes == 0) {
	    if (interp != NULL) {
		Tcl_SetResult(interp, "missing close-brace for variable name",
			TCL_STATIC);
	    }
	    parsePtr->errorType = TCL_PARSE_MISSING_VAR_BRACE;
	    parsePtr->term = tokenPtr->start-1;
	    parsePtr->incomplete = 1;

	    goto error;
	}
	tokenPtr->size = src - tokenPtr->start;
	tokenPtr[-1].size = src - tokenPtr[-1].start;
	parsePtr->numTokens++;
	src++;
    } else {
	tokenPtr->type = TCL_TOKEN_TEXT;
	tokenPtr->start = src;
	tokenPtr->numComponents = 0;
	while (numBytes) {
	    if (Tcl_UtfCharComplete(src, numBytes)) {
	        offset = Tcl_UtfToUniChar(src, &ch);
	    } else {
		char utfBytes[TCL_UTF_MAX];
		memcpy(utfBytes, src, (size_t) numBytes);
		utfBytes[numBytes] = '\0';
	        offset = Tcl_UtfToUniChar(utfBytes, &ch);
	    }
	    c = UCHAR(ch);
	    if (isalnum(c) || (c == '_')) { /* INTL: ISO only, UCHAR. */
		src += offset;  numBytes -= offset;
		continue;
	    }
	    if ((c == ':') && (numBytes != 1) && (src[1] == ':')) {
		src += 2; numBytes -= 2;
		while (numBytes && (*src == ':')) {
		    src++; numBytes--; 
		}
		continue;
	    }
	    break;
	}

	/*
	 * Support for empty array names here.
	 */
	array = (numBytes && (*src == '('));
	tokenPtr->size = src - tokenPtr->start;
	if ((tokenPtr->size == 0) && !array) {
	    goto justADollarSign;
	}
	parsePtr->numTokens++;
	if (array) {
	    /*
	     * This is a reference to an array element.  Call
	     * ParseTokens recursively to parse the element name,
	     * since it could contain any number of substitutions.
	     */

	    if (TCL_OK != ParseTokens(src+1, numBytes-1, TYPE_CLOSE_PAREN,
		    flags | TCL_SUBST_ALL, parsePtr)) {
		goto error;
	    }
	    if ((parsePtr->term == (src + numBytes)) 
		    || (*parsePtr->term != ')')) { 
		if (parsePtr->interp != NULL) {
		    Tcl_SetResult(parsePtr->interp, "missing )",
			    TCL_STATIC);
		}
		parsePtr->errorType = TCL_PARSE_MISSING_PAREN;
		parsePtr->term = src;
		parsePtr->incomplete = 1;
		goto error;
	    }
	    src = parsePtr->term + 1;
	}
    }
    tokenPtr = &parsePtr->tokenPtr[varIndex];
    tokenPtr->size = src - tokenPtr->start;
    tokenPtr->numComponents = parsePtr->numTokens - (varIndex + 1);
    return TCL_OK;

    /*
     * The dollar sign isn't followed by a variable name.
     * replace the TCL_TOKEN_VARIABLE token with a
     * TCL_TOKEN_TEXT token for the dollar sign.
     */

    justADollarSign:
    tokenPtr = &parsePtr->tokenPtr[varIndex];
    tokenPtr->type = TCL_TOKEN_TEXT;
    tokenPtr->size = 1;
    tokenPtr->numComponents = 0;
    return TCL_OK;

    error:
    /* Convert variable substitution token to error token */
    tokenPtr = &parsePtr->tokenPtr[varIndex];
    tokenPtr->type = TCL_TOKEN_ERROR;
    tokenPtr->numComponents = parsePtr->errorType;
    tokenPtr->size = parsePtr->end - tokenPtr->start;
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ParseVar --
 *
 *	Given a string starting with a $ sign, parse off a variable
 *	name and return its value.
 *
 * Results:
 *	The return value is the contents of the variable given by
 *	the leading characters of string.  If termPtr isn't NULL,
 *	*termPtr gets filled in with the address of the character
 *	just after the last one in the variable specifier.  If the
 *	variable doesn't exist, then the return value is NULL and
 *	an error message will be left in interp's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

CONST char *
Tcl_ParseVar(interp, string, termPtr)
    Tcl_Interp *interp;			/* Context for looking up variable. */
    register CONST char *string;	/* String containing variable name.
					 * First character must be "$". */
    CONST char **termPtr;		/* If non-NULL, points to word to fill
					 * in with character just after last
					 * one in the variable specifier. */
{
    Tcl_Parse parse;
    register Tcl_Obj *objPtr;
    int code;

    if (Tcl_ParseVarName(interp, string, -1, &parse, 0) != TCL_OK) {
	return NULL;
    }

    if (termPtr != NULL) {
	*termPtr = string + parse.tokenPtr->size;
    }
    if (parse.numTokens == 1) {
	/*
	 * There isn't a variable name after all: the $ is just a $.
	 */

	return "$";
    }

    code = TclSubstTokens(interp, parse.tokenPtr, parse.numTokens, NULL,0);
    if (code != TCL_OK) {
	return NULL;
    }
    objPtr = Tcl_GetObjResult(interp);

    /*
     * At this point we should have an object containing the value of
     * a variable.  Just return the string from that object.
     *
     * This should have returned the object for the user to manage, but
     * instead we have some weak reference to the string value in the
     * object, which is why we make sure the object exists after resetting
     * the result.  This isn't ideal, but it's the best we can do with the
     * current documented interface. -- hobbs
     */

    if (!Tcl_IsShared(objPtr)) {
	Tcl_IncrRefCount(objPtr);
    }
    Tcl_ResetResult(interp);
    return TclGetString(objPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ParseBraces --
 *
 *	Given a string in braces such as a Tcl command argument or a string
 *	value in a Tcl expression, this procedure parses the string and
 *	returns information about the parse.  No more than numBytes bytes
 *	will be scanned.
 *
 * Results:
 *	The return value is TCL_OK if the string was parsed successfully and
 *	TCL_ERROR otherwise. If an error occurs and interp isn't NULL then
 *	an error message is left in its result. On a successful return,
 *	tokenPtr and numTokens fields of parsePtr are filled in with
 *	information about the string that was parsed. Other fields in
 *	parsePtr are undefined. termPtr is set to point to the character
 *	just after the last one in the braced string.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the
 *	information about the command, then additional space is
 *	malloc-ed. If the procedure returns TCL_OK then the caller must
 *	eventually invoke Tcl_FreeParse to release any additional space
 *	that was allocated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ParseBraces(interp, string, numBytes, parsePtr, append, termPtr)
    Tcl_Interp *interp;		/* See ParseBraces */
    CONST char *string;		/* See ParseBraces */
    register int numBytes;	/* See ParseBraces */
    register Tcl_Parse *parsePtr;
    				/* See ParseBraces */
    int append;			/* Non-zero means append tokens to existing
				 * information in parsePtr; zero means
				 * ignore existing tokens in parsePtr and
				 * reinitialize it. */
    CONST char **termPtr;	/* See ParseBraces */

{
    int code = ParseBraces(interp, string, numBytes, parsePtr,
	    (append != 0) ? PARSE_APPEND : 0, termPtr);
    if (code == TCL_ERROR) {
	Tcl_FreeParse(parsePtr);
    }
    return code;
}

static int
ParseBraces(interp, string, numBytes, parsePtr, flags, termPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting;
				 * if NULL, then no error message is
				 * provided. */
    CONST char *string;		/* String containing the string in braces. 
				 * The first character must be '{'. */
    register int numBytes;	/* Total number of bytes in string. If < 0,
				 * the string consists of all bytes up to
				 * the first null character. */
    register Tcl_Parse *parsePtr;
    				/* Structure to fill in with information
				 * about the string. */
    int flags;			/* Bit flags to control details of the parsing.
				 * Only the PARSE_APPEND flag has an effect
				 * here.  Other flags are passed along. */
    CONST char **termPtr;	/* If non-NULL, points to word in which to
				 * store a pointer to the character just
				 * after the terminating '}' if the parse
				 * was successful. */
{
    Tcl_Token *tokenPtr;
    register CONST char *src;
    int startIndex, level, length;
    int append = (flags & PARSE_APPEND);

    if ((numBytes == 0) || (string == NULL)) {
	return TCL_ERROR;
    }
    if (numBytes < 0) {
	numBytes = strlen(string);
    }

    if (!append) {
	parsePtr->numWords = 0;
	parsePtr->tokenPtr = parsePtr->staticTokens;
	parsePtr->numTokens = 0;
	parsePtr->tokensAvailable = NUM_STATIC_TOKENS;
	parsePtr->string = string;
	parsePtr->end = (string + numBytes);
	parsePtr->interp = interp;
	parsePtr->errorType = TCL_PARSE_SUCCESS;
    }

    src = string;
    startIndex = parsePtr->numTokens;

    TclGrowParseTokenArray(parsePtr,1);
    tokenPtr = &parsePtr->tokenPtr[startIndex];
    tokenPtr->type = TCL_TOKEN_TEXT;
    tokenPtr->start = src+1;
    tokenPtr->numComponents = 0;
    level = 1;
    while (1) {
	while (++src, --numBytes) {
	    if (CHAR_TYPE(*src) != TYPE_NORMAL) {
		break;
	    }
	}
	if (numBytes == 0) {
	    register int openBrace = 0;

	    parsePtr->errorType = TCL_PARSE_MISSING_BRACE;
	    parsePtr->term = string;
	    parsePtr->incomplete = 1;
	    if (interp == NULL) {
		/*
		 * Skip straight to the exit code since we have no
		 * interpreter to put error message in.
		 */
		goto error;
	    }

	    Tcl_SetResult(interp, "missing close-brace", TCL_STATIC);

	    /*
	     *  Guess if the problem is due to comments by searching
	     *  the source string for a possible open brace within the
	     *  context of a comment.  Since we aren't performing a
	     *  full Tcl parse, just look for an open brace preceded
	     *  by a '<whitespace>#' on the same line.
	     */

	    for (; src > string; src--) {
		switch (*src) {
		    case '{':
			openBrace = 1;
			break;
		    case '\n':
			openBrace = 0;
			break;
		    case '#' :
			if (openBrace && (isspace(UCHAR(src[-1])))) {
			    Tcl_AppendResult(interp,
				    ": possible unbalanced brace in comment",
				    (char *) NULL);
			    goto error;
			}
			break;
		}
	    }

	    error:
	    return TCL_ERROR;
	}
	switch (*src) {
	    case '{':
		level++;
		break;
	    case '}':
		if (--level == 0) {

		    /*
		     * Decide if we need to finish emitting a
		     * partially-finished token.  There are 3 cases:
		     *     {abc \newline xyz} or {xyz}
		     *		- finish emitting "xyz" token
		     *     {abc \newline}
		     *		- don't emit token after \newline
		     *     {}	- finish emitting zero-sized token
		     *
		     * The last case ensures that there is a token
		     * (even if empty) that describes the braced string.
		     */
    
		    if ((src != tokenPtr->start)
			    || (parsePtr->numTokens == startIndex)) {
			tokenPtr->size = (src - tokenPtr->start);
			parsePtr->numTokens++;
		    }
		    if (termPtr != NULL) {
			*termPtr = src+1;
		    }
		    return TCL_OK;
		}
		break;
	    case '\\':
		TclParseBackslash(src, numBytes, &length, NULL);
		if ((length > 1) && (src[1] == '\n')) {
		    /*
		     * A backslash-newline sequence must be collapsed, even
		     * inside braces, so we have to split the word into
		     * multiple tokens so that the backslash-newline can be
		     * represented explicitly.
		     */
		
		    if (numBytes == 2) {
			parsePtr->incomplete = 1;
		    }
		    tokenPtr->size = (src - tokenPtr->start);
		    if (tokenPtr->size != 0) {
			parsePtr->numTokens++;
		    }
		    TclGrowParseTokenArray(parsePtr,2);
		    tokenPtr = &parsePtr->tokenPtr[parsePtr->numTokens];
		    tokenPtr->type = TCL_TOKEN_BS;
		    tokenPtr->start = src;
		    tokenPtr->size = length;
		    tokenPtr->numComponents = 0;
		    parsePtr->numTokens++;
		
		    src += length - 1;
		    numBytes -= length - 1;
		    tokenPtr++;
		    tokenPtr->type = TCL_TOKEN_TEXT;
		    tokenPtr->start = src + 1;
		    tokenPtr->numComponents = 0;
		} else {
		    src += length - 1;
		    numBytes -= length - 1;
		}
		break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ParseQuotedString --
 *
 *	Given a double-quoted string such as a quoted Tcl command argument
 *	or a quoted value in a Tcl expression, this procedure parses the
 *	string and returns information about the parse.  No more than
 *	numBytes bytes will be scanned.
 *
 * Results:
 *	The return value is TCL_OK if the string was parsed successfully and
 *	TCL_ERROR otherwise. If an error occurs and interp isn't NULL then
 *	an error message is left in its result. On a successful return,
 *	tokenPtr and numTokens fields of parsePtr are filled in with
 *	information about the string that was parsed. Other fields in
 *	parsePtr are undefined. termPtr is set to point to the character
 *	just after the quoted string's terminating close-quote.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the
 *	information about the command, then additional space is
 *	malloc-ed. If the procedure returns TCL_OK then the caller must
 *	eventually invoke Tcl_FreeParse to release any additional space
 *	that was allocated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ParseQuotedString(interp, string, numBytes, parsePtr, append, termPtr)
    Tcl_Interp *interp;		/* See ParseQuotedString */
    CONST char *string;		/* See ParseQuotedString */
    int numBytes;		/* See ParseQuotedString */
    Tcl_Parse *parsePtr;
    				/* See ParseQuotedString */
    int append;			/* Non-zero means append tokens to existing
				 * information in parsePtr; zero means
				 * ignore existing tokens in parsePtr and
				 * reinitialize it. */
    CONST char **termPtr;	/* See ParseQuotedString */
{
    int code = ParseQuotedString(interp, string, numBytes, parsePtr,
	    (append != 0) ? PARSE_APPEND : 0, termPtr);
    if (code == TCL_ERROR) {
	Tcl_FreeParse(parsePtr);
    }
    return code;
}

int
ParseQuotedString(interp, string, numBytes, parsePtr, flags, termPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting;
				 * if NULL, then no error message is
				 * provided. */
    CONST char *string;		/* String containing the quoted string. 
				 * The first character must be '"'. */
    register int numBytes;	/* Total number of bytes in string. If < 0,
				 * the string consists of all bytes up to
				 * the first null character. */
    register Tcl_Parse *parsePtr;
    				/* Structure to fill in with information
				 * about the string. */
    int flags;			/* Bit flags to control details of the parsing.
				 * Only the PARSE_APPEND flag has an effect
				 * here.  Other flags are passed along. */
    CONST char **termPtr;	/* If non-NULL, points to word in which to
				 * store a pointer to the character just
				 * after the quoted string's terminating
				 * close-quote if the parse succeeds. */
{
    int append = (flags * PARSE_APPEND);

    if ((numBytes == 0) || (string == NULL)) {
	return TCL_ERROR;
    }
    if (numBytes < 0) {
	numBytes = strlen(string);
    }

    if (!append) {
	parsePtr->numWords = 0;
	parsePtr->tokenPtr = parsePtr->staticTokens;
	parsePtr->numTokens = 0;
	parsePtr->tokensAvailable = NUM_STATIC_TOKENS;
	parsePtr->string = string;
	parsePtr->end = (string + numBytes);
	parsePtr->interp = interp;
	parsePtr->errorType = TCL_PARSE_SUCCESS;
    }
    
    if (TCL_OK != ParseTokens(string+1, numBytes-1, TYPE_QUOTE,
	    flags | TCL_SUBST_ALL, parsePtr)) {
	goto error;
    }
    if (*parsePtr->term != '"') {
	if (interp != NULL) {
	    Tcl_SetResult(parsePtr->interp, "missing \"", TCL_STATIC);
	}
	parsePtr->errorType = TCL_PARSE_MISSING_QUOTE;
	parsePtr->term = string;
	parsePtr->incomplete = 1;
	goto error;
    }
    if (termPtr != NULL) {
	*termPtr = (parsePtr->term + 1);
    }
    return TCL_OK;

    error:
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SubstObj --
 *
 *      This function performs the substitutions specified on the
 *      given string as described in the user documentation for the
 *      "subst" Tcl command.       
 *
 * Results:
 *      A Tcl_Obj* containing the substituted string, or NULL to
 *      indicate that an error occurred.
 *
 * Side effects:
 *      See the user documentation.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_SubstObj(interp, objPtr, flags)
    Tcl_Interp *interp;	/* Interpreter in which substitution occurs */
    Tcl_Obj *objPtr;	/* The value to be substituted */
    int flags;		/* What substitutions to do */
{
    int length, tokensLeft, code;
    Tcl_Parse parse;
    Tcl_Token *endTokenPtr;
    Tcl_Obj *result;
    Tcl_Obj *errMsg = NULL;
    CONST char *p = Tcl_GetStringFromObj(objPtr, &length);

    parse.tokenPtr = parse.staticTokens;
    parse.numTokens = 0;
    parse.tokensAvailable = NUM_STATIC_TOKENS;
    parse.string = p;
    parse.end = p + length;
    parse.term = parse.end;
    parse.interp = interp;
    parse.incomplete = 0;
    parse.errorType = TCL_PARSE_SUCCESS;

    /*
     * First parse the string rep of objPtr, as if it were enclosed
     * as a "-quoted word in a normal Tcl command.  Honor flags that
     * selectively inhibit types of substitution.
     */

    flags &= TCL_SUBST_ALL;
    flags |= PARSE_USE_INTERNAL_TOKENS;
    ParseTokens(p, length, /* mask */ 0, flags, &parse);

    /* Next, substitute the parsed tokens just as in normal Tcl evaluation */
    endTokenPtr = parse.tokenPtr + parse.numTokens;
    tokensLeft = parse.numTokens;
    code = TclSubstTokens(interp, endTokenPtr - tokensLeft, tokensLeft,
	    &tokensLeft,0);
    if (code == TCL_OK) {
	Tcl_FreeParse(&parse);
	if (errMsg != NULL) {
	    Tcl_SetObjResult(interp, errMsg);
	    Tcl_DecrRefCount(errMsg);
	    return NULL;
	}
	return Tcl_GetObjResult(interp);
    }
    result = Tcl_NewObj();
    while (1) {
	switch (code) {
	    case TCL_ERROR:
		Tcl_FreeParse(&parse);
		Tcl_DecrRefCount(result);
		if (errMsg != NULL) {
		    Tcl_DecrRefCount(errMsg);
		}
		return NULL;
	    case TCL_BREAK:
		tokensLeft = 0;		/* Halt substitution */
	    default:
		Tcl_AppendObjToObj(result, Tcl_GetObjResult(interp));
	}

	if (tokensLeft == 0) {
	    Tcl_FreeParse(&parse);
	    if (errMsg != NULL) {
		if (code != TCL_BREAK) {
		    Tcl_SetObjResult(interp, errMsg);
		    Tcl_DecrRefCount(errMsg);
		    return NULL;
		}
		Tcl_DecrRefCount(errMsg);
	    }
	    return result;
	}

	code = TclSubstTokens(interp, endTokenPtr - tokensLeft, tokensLeft,
		&tokensLeft,0);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclSubstTokens --
 *
 *	Accepts an array of count Tcl_Token's, and creates a result
 *	value in the interp from concatenating the results of 
 *	performing Tcl substitution on each Tcl_Token.  Substitution
 *	is interrupted if any non-TCL_OK completion code arises.
 *
 * Results:
 * 	The return value is a standard Tcl completion code.  The
 * 	result in interp is the substituted value, or an error message
 * 	if TCL_ERROR is returned.  If tokensLeftPtr is not NULL, then
 * 	it points to an int where the number of tokens remaining to
 * 	be processed is written.
 *
 * Side effects:
 * 	Can be anything, depending on the types of substitution done.
 *
 *----------------------------------------------------------------------
 */

int
TclSubstTokens(interp, tokenPtr, count, tokensLeftPtr, flags)
    Tcl_Interp *interp;         /* Interpreter in which to lookup
                                 * variables, execute nested commands,
                                 * and report errors. */
    Tcl_Token *tokenPtr;        /* Pointer to first in an array of tokens
                                 * to evaluate and concatenate. */
    int count;                  /* Number of tokens to consider at tokenPtr.
                                 * Must be at least 1. */
    int *tokensLeftPtr;		/* If not NULL, points to memory where an
				 * integer representing the number of tokens
				 * left to be substituted will be written */
{
    Tcl_Obj *result;
    int code = TCL_OK;

    /*
     * Each pass through this loop will substitute one token, and its
     * components, if any.  The only thing tricky here is that we go to
     * some effort to pass Tcl_Obj's through untouched, to avoid string
     * copying and Tcl_Obj creation if possible, to aid performance and
     * limit shimmering.
     *
     * Further optimization opportunities might be to check for the
     * equivalent of Tcl_SetObjResult(interp, Tcl_GetObjResult(interp))
     * and omit them.
     */

    result = NULL;
    for ( ; (count > 0) && (code == TCL_OK); count--, tokenPtr++) {
	Tcl_Obj *appendObj = NULL;
	CONST char *append = NULL;
	int appendByteLength = 0;
	char utfCharBytes[TCL_UTF_MAX];

	switch (tokenPtr->type) {
	    case TCL_TOKEN_TEXT:
		append = tokenPtr->start;
		appendByteLength = tokenPtr->size;
		break;

	    case TCL_TOKEN_BS: {
		appendByteLength = Tcl_UtfBackslash(tokenPtr->start,
			(int *) NULL, utfCharBytes);
		append = utfCharBytes;
		break;
	    }

	    case TCL_TOKEN_COMMAND:
		code = Tcl_EvalEx(interp, tokenPtr->start+1, tokenPtr->size-2,
			flags);
		appendObj = Tcl_GetObjResult(interp);
		break;

	    case TCL_TOKEN_VARIABLE: {
		Tcl_Obj *arrayIndex = NULL;
		Tcl_Obj *varName = NULL;
		if (count <= tokenPtr->numComponents) {
		    Tcl_Panic("token components overflow token array");
		}
		if (tokenPtr->numComponents > 1) {
		    /* Subst the index part of an array variable reference */
		    code = TclSubstTokens(interp, tokenPtr+2,
			    tokenPtr->numComponents - 1, NULL,flags);
		    arrayIndex = Tcl_GetObjResult(interp);
		    Tcl_IncrRefCount(arrayIndex);
		}

		if (code == TCL_OK) {
		    varName = Tcl_NewStringObj(tokenPtr[1].start,
			    tokenPtr[1].size);
		    appendObj = Tcl_ObjGetVar2(interp, varName, arrayIndex,
			    TCL_LEAVE_ERR_MSG);
		    Tcl_DecrRefCount(varName);
		    if (appendObj == NULL) {
			code = TCL_ERROR;
		    }
		}

		switch (code) {
		    case TCL_OK:	/* Got value */
		    case TCL_ERROR:	/* Already have error message */
		    case TCL_BREAK:	/* Will not substitute anyway */
		    case TCL_CONTINUE:	/* Will not substitute anyway */
			break;
		    default:
			/* All other return codes, we will subst the
			 * result from the code-throwing evaluation */
			appendObj = Tcl_GetObjResult(interp);
		}

		if (arrayIndex != NULL) {
		    Tcl_DecrRefCount(arrayIndex);
		}
		count -= tokenPtr->numComponents;
		tokenPtr += tokenPtr->numComponents;
		break;
	    }

	    case TCL_TOKEN_SCRIPT_SUBST:
		if (count <= tokenPtr->numComponents) {
		    Tcl_Panic("token components overflow token array");
		}
		code = TclEvalScriptTokens(interp, tokenPtr+1,
			tokenPtr->numComponents,flags);
		appendObj = Tcl_GetObjResult(interp);
		count -= tokenPtr->numComponents;
		tokenPtr += tokenPtr->numComponents;
		if (tokenPtr->type == TCL_TOKEN_ERROR) {
		    count++; tokenPtr--;
		}
		break;

	    case TCL_TOKEN_ERROR:
		Tcl_SetResult(interp, (char *)
			tclParseErrorMsg[tokenPtr->numComponents], TCL_STATIC);
		code = TCL_ERROR;
		break;

	    default:
		Tcl_Panic("unexpected token type in TclSubstTokens: %d",
			tokenPtr->type);
	}

	if ((code == TCL_BREAK) || (code == TCL_CONTINUE)) {
	    /* Inhibit substitution */
	    continue;
	}

	if (result == NULL) {
	    /* 
	     * First pass through.  If we have a Tcl_Obj, just use it.
	     * If not, create one from our string. 
	     */

	    if (appendObj != NULL) {
		result = appendObj;
	    } else {
		result = Tcl_NewStringObj(append, appendByteLength);;
	    }
	    Tcl_IncrRefCount(result);
	} else {
	    /* Subsequent passes.  Append to result. */
	    if (Tcl_IsShared(result)) {
		Tcl_DecrRefCount(result);
		result = Tcl_DuplicateObj(result);
		Tcl_IncrRefCount(result);
	    }
	    if (appendObj != NULL) {
		Tcl_AppendObjToObj(result, appendObj);
	    } else {
		Tcl_AppendToObj(result, append, appendByteLength);
	    }
	}
    }

    if (code != TCL_ERROR) {	/* Keep error message in result! */
	if (result != NULL) {
	    Tcl_SetObjResult(interp, result);
	} else {
	    Tcl_ResetResult(interp);
	}
    }
    if (tokensLeftPtr != NULL) {
	*tokensLeftPtr = count;
    }
    if (result != NULL) {
	Tcl_DecrRefCount(result);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * CommandComplete --
 *
 *      This procedure is shared by TclCommandComplete and
 *      Tcl_ObjCommandcoComplete; it does all the real work of seeing
 *      whether a script is complete
 *
 * Results:
 *      1 is returned if the script is complete, 0 if there are open
 *      delimiters such as " or (. 1 is also returned if there is a
 *      parse error in the script other than unmatched delimiters.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
CommandComplete(script, numBytes)
    CONST char *script;                 /* Script to check. */
    int numBytes;                       /* Number of bytes in script. */
{
    Tcl_Parse parse;
    CONST char *p, *end;

    /*
     * NOTE: This set of routines should not be converted to make use of
     * TclParseScript, because [info complete] is defined to operate only
     * one parsing level deep, while TclParseScript digs out parsing errors
     * in nested script substitutions.  See test parse-6.8, etc.
     */

    p = script;
    end = p + numBytes;
    parse.incomplete = 0;
    while ((p < end) && (TCL_OK == 
	    Tcl_ParseCommand((Tcl_Interp *) NULL, p, end - p, 0, &parse))) {
        p = parse.commandStart + parse.commandSize;
        Tcl_FreeParse(&parse);
    }
    return (parse.incomplete == 0);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CommandComplete --
 *
 *	Given a partial or complete Tcl script, this procedure
 *	determines whether the script is complete in the sense
 *	of having matched braces and quotes and brackets.
 *
 * Results:
 *	1 is returned if the script is complete, 0 otherwise.
 *	1 is also returned if there is a parse error in the script
 *	other than unmatched delimiters.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_CommandComplete(script)
    CONST char *script;			/* Script to check. */
{
    return CommandComplete(script, (int) strlen(script));
}

/*
 *----------------------------------------------------------------------
 *
 * TclObjCommandComplete --
 *
 *	Given a partial or complete Tcl command in a Tcl object, this
 *	procedure determines whether the command is complete in the sense of
 *	having matched braces and quotes and brackets.
 *
 * Results:
 *	1 is returned if the command is complete, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclObjCommandComplete(objPtr)
    Tcl_Obj *objPtr;			/* Points to object holding script
					 * to check. */
{
    CONST char *script;
    int length;

    script = Tcl_GetStringFromObj(objPtr, &length);
    return CommandComplete(script, length);
}

/*
 *----------------------------------------------------------------------
 *
 * TclIsLocalScalar --
 *
 *	Check to see if a given string is a legal scalar variable
 *	name with no namespace qualifiers or substitutions.
 *
 * Results:
 *	Returns 1 if the variable is a local scalar.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclIsLocalScalar(src, len)
    CONST char *src;
    int len;
{
    CONST char *p;
    CONST char *lastChar = src + (len - 1);

    for (p = src; p <= lastChar; p++) {
	if ((CHAR_TYPE(*p) != TYPE_NORMAL) &&
		(CHAR_TYPE(*p) != TYPE_COMMAND_END)) {
	    /*
	     * TCL_COMMAND_END is returned for the last character
	     * of the string.  By this point we know it isn't
	     * an array or namespace reference.
	     */

	    return 0;
	}
	if  (*p == '(') {
	    if (*lastChar == ')') { /* we have an array element */
		return 0;
	    }
	} else if (*p == ':') {
	    if ((p != lastChar) && *(p+1) == ':') { /* qualified name */
		return 0;
	    }
	}
    }
	
    return 1;
}
