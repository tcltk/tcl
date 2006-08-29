/*
 * tclParseExpr.c --
 *
 *	This file contains functions that parse Tcl expressions. They do so in
 *	a general-purpose fashion that can be used for many different
 *	purposes, including compilation, direct execution, code analysis, etc.
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 * Contributions from Don Porter, NIST, 2002.  (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclParseExpr.c,v 1.17.4.17 2006/08/29 16:19:30 dgp Exp $
 */

#define OLD_EXPR_PARSER 0
#if OLD_EXPR_PARSER

#include "tclInt.h"

/*
 * Boolean variable that controls whether expression parse tracing is enabled.
 */

#ifdef TCL_COMPILE_DEBUG
static int traceParseExpr = 0;
#endif /* TCL_COMPILE_DEBUG */

/*
 * The ParseInfo structure holds state while parsing an expression. A pointer
 * to an ParseInfo record is passed among the routines in this module.
 */

typedef struct ParseInfo {
    Tcl_Parse *parsePtr;	/* Points to structure to fill in with
				 * information about the expression. */
    int lexeme;			/* Type of last lexeme scanned in expr.  See
				 * below for definitions. Corresponds to size
				 * characters beginning at start. */
    CONST char *start;		/* First character in lexeme. */
    int size;			/* Number of bytes in lexeme. */
    CONST char *next;		/* Position of the next character to be
				 * scanned in the expression string. */
    CONST char *prevEnd;	/* Points to the character just after the last
				 * one in the previous lexeme. Used to compute
				 * size of subexpression tokens. */
    CONST char *originalExpr;	/* Points to the start of the expression
				 * originally passed to Tcl_ParseExpr. */
    CONST char *lastChar;	/* Points just after last byte of expr. */
    int useInternalTokens;	/* Boolean indicating whether internal
				 * token types are acceptable */
} ParseInfo;

/*
 * Definitions of the different lexemes that appear in expressions. The order
 * of these must match the corresponding entries in the operatorStrings array
 * below.
 *
 * Basic lexemes:
 */

#define LITERAL		0
#define FUNC_NAME	1
#define OPEN_BRACKET	2
#define OPEN_BRACE	3
#define OPEN_PAREN	4
#define CLOSE_PAREN	5
#define DOLLAR		6
#define QUOTE		7
#define COMMA		8
#define END		9
#define UNKNOWN		10
#define UNKNOWN_CHAR	11

/*
 * Binary numeric operators:
 */

#define MULT		12
#define DIVIDE		13
#define MOD		14
#define PLUS		15
#define MINUS		16
#define LEFT_SHIFT	17
#define RIGHT_SHIFT	18
#define LESS		19
#define GREATER		20
#define LEQ		21
#define GEQ		22
#define EQUAL		23
#define NEQ		24
#define BIT_AND		25
#define BIT_XOR		26
#define BIT_OR		27
#define AND		28
#define OR		29
#define QUESTY		30
#define COLON		31

/*
 * Unary operators. Unary minus and plus are represented by the (binary)
 * lexemes MINUS and PLUS.
 */

#define NOT		32
#define BIT_NOT		33

/*
 * Binary string operators:
 */

#define STREQ		34
#define STRNEQ		35

/*
 * Exponentiation operator:
 */

#define EXPON		36

/*
 * List containment operators
 */

#define IN_LIST		37
#define NOT_IN_LIST	38

/*
 * Mapping from lexemes to strings; used for debugging messages. These entries
 * must match the order and number of the lexeme definitions above.
 */

static char *lexemeStrings[] = {
    "LITERAL", "FUNCNAME",
    "[", "{", "(", ")", "$", "\"", ",", "END", "UNKNOWN", "UNKNOWN_CHAR",
    "*", "/", "%", "+", "-",
    "<<", ">>", "<", ">", "<=", ">=", "==", "!=",
    "&", "^", "|", "&&", "||", "?", ":",
    "!", "~", "eq", "ne", "**", "in", "ni"
};

/*
 * Declarations for local functions to this file:
 */

static int		GetLexeme(ParseInfo *infoPtr);
static void		LogSyntaxError(ParseInfo *infoPtr,
			    CONST char *extraInfo);
static int		ParseAddExpr(ParseInfo *infoPtr);
static int		ParseBitAndExpr(ParseInfo *infoPtr);
static int		ParseBitOrExpr(ParseInfo *infoPtr);
static int		ParseBitXorExpr(ParseInfo *infoPtr);
static int		ParseCondExpr(ParseInfo *infoPtr);
static int		ParseEqualityExpr(ParseInfo *infoPtr);
static int		ParseLandExpr(ParseInfo *infoPtr);
static int		ParseLorExpr(ParseInfo *infoPtr);
static int		ParseMultiplyExpr(ParseInfo *infoPtr);
static int		ParsePrimaryExpr(ParseInfo *infoPtr);
static int		ParseRelationalExpr(ParseInfo *infoPtr);
static int		ParseShiftExpr(ParseInfo *infoPtr);
static int		ParseExponentialExpr(ParseInfo *infoPtr);
static int		ParseUnaryExpr(ParseInfo *infoPtr);
static void		PrependSubExprTokens(CONST char *op, int opBytes,
			    CONST char *src, int srcBytes, int firstIndex,
			    ParseInfo *infoPtr);

/*
 * Macro used to debug the execution of the recursive descent parser used to
 * parse expressions.
 */

#ifdef TCL_COMPILE_DEBUG
#define HERE(production, level) \
    if (traceParseExpr) { \
	fprintf(stderr, "%*s%s: lexeme=%s, next=\"%.20s\"\n", \
		(level), " ", (production), \
		lexemeStrings[infoPtr->lexeme], infoPtr->next); \
    }
#else
#define HERE(production, level)
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ParseExpr --
 *
 *	Given a string, this function parses the first Tcl expression in the
 *	string and returns information about the structure of the expression.
 *	This function is the top-level interface to the the expression parsing
 *	module. No more than numBytes bytes will be scanned.
 *
 *	Note that this parser is a LL(1) parser; the operator precedence rules
 *	are completely hard coded in the recursive structure of the parser
 *	itself.
 *
 * Results:
 *	The return value is TCL_OK if the command was parsed successfully and
 *	TCL_ERROR otherwise. If an error occurs and interp isn't NULL then an
 *	error message is left in its result. On a successful return, parsePtr
 *	is filled in with information about the expression that was parsed.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the expression, then additional space is malloc-ed. If the
 *	function returns TCL_OK then the caller must eventually invoke
 *	Tcl_FreeParse to release any additional space that was allocated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ParseExpr(
    Tcl_Interp *interp,		/* Used for error reporting. */
    CONST char *start,		/* Start of source string to parse. */
    int numBytes,		/* Number of bytes in string. If < 0, the
				 * string consists of all bytes up to the
				 * first null character. */
    Tcl_Parse *parsePtr)	/* Structure to fill with information about
				 * the parsed expression; any previous
				 * information in the structure is ignored. */
{
    int code = TclParseExpr(interp, start, numBytes, 0, parsePtr);
    if (code == TCL_ERROR) {
	Tcl_FreeParse(parsePtr);
    }
    return code;
}

int
TclParseExpr(interp, start, numBytes, useInternalTokens, parsePtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    CONST char *start;		/* The source string to parse. */
    int numBytes;		/* Number of bytes in string. If < 0, the
				 * string consists of all bytes up to the
				 * first null character. */
    int useInternalTokens;	/* Boolean indicating whether internal
				 * token types are acceptable */
    Tcl_Parse *parsePtr;	/* Structure to fill with information about
				 * the parsed expression; any previous
				 * information in the structure is
				 * ignored. */
{
    ParseInfo info;
    int code;

    if (numBytes < 0) {
	numBytes = (start? strlen(start) : 0);
    }
#ifdef TCL_COMPILE_DEBUG
    if (traceParseExpr) {
	fprintf(stderr, "Tcl_ParseExpr: string=\"%.*s\"\n",
		numBytes, start);
    }
#endif /* TCL_COMPILE_DEBUG */

    TclParseInit(interp, start, numBytes, parsePtr);

    /*
     * Initialize the ParseInfo structure that holds state while parsing the
     * expression.
     */

    info.parsePtr = parsePtr;
    info.lexeme = UNKNOWN;
    info.start = NULL;
    info.size = 0;
    info.next = start;
    info.prevEnd = start;
    info.originalExpr = start;
    info.lastChar = (start + numBytes); /* just after last char of expr */
    info.useInternalTokens = useInternalTokens;

    /*
     * Get the first lexeme then parse the expression.
     */

    code = GetLexeme(&info);
    if (code != TCL_OK) {
	return TCL_ERROR;
    }
    code = ParseCondExpr(&info);
    /*
    if (useInternalTokens && (code == TCL_ERROR)
	    && (parsePtr->tokenPtr[parsePtr->numTokens - 1].type
	    == TCL_TOKEN_ERROR)) {
	return TCL_OK;
    }
    */
    if (code != TCL_OK) {
	return TCL_ERROR;
    }
    if (info.lexeme != END) {
	LogSyntaxError(&info, "extra tokens at end of expression");
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseCondExpr --
 *
 *	This function parses a Tcl conditional expression:
 *	condExpr ::= lorExpr ['?' condExpr ':' condExpr]
 *
 *	Note that this is the topmost recursive-descent parsing routine used
 *	by Tcl_ParseExpr to parse expressions. This avoids an extra function
 *	call since such a function would only return the result of calling
 *	ParseCondExpr. Other recursive-descent functions that need to parse
 *	complete expressions also call ParseCondExpr.
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseCondExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    Tcl_Token *tokenPtr, *firstTokenPtr, *condTokenPtr;
    int firstIndex, numToMove, code;
    CONST char *srcStart;

    HERE("condExpr", 1);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseLorExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    if (infoPtr->lexeme == QUESTY) {
	/*
	 * Emit two tokens: one TCL_TOKEN_SUB_EXPR token for the entire
	 * conditional expression, and a TCL_TOKEN_OPERATOR token for the "?"
	 * operator. Note that these two tokens must be inserted before the
	 * LOR operand tokens generated above.
	 */

	TclGrowParseTokenArray(parsePtr,2);
	firstTokenPtr = &parsePtr->tokenPtr[firstIndex];
	tokenPtr = (firstTokenPtr + 2);
	numToMove = (parsePtr->numTokens - firstIndex);
	memmove((VOID *) tokenPtr, (VOID *) firstTokenPtr,
		(size_t) (numToMove * sizeof(Tcl_Token)));
	parsePtr->numTokens += 2;

	tokenPtr = firstTokenPtr;
	tokenPtr->type = TCL_TOKEN_SUB_EXPR;
	tokenPtr->start = srcStart;

	tokenPtr++;
	tokenPtr->type = TCL_TOKEN_OPERATOR;
	tokenPtr->start = infoPtr->start;
	tokenPtr->size = 1;
	tokenPtr->numComponents = 0;

	/*
	 * Skip over the '?'.
	 */

	code = GetLexeme(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Parse the "then" expression.
	 */

	code = ParseCondExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}
	if (infoPtr->lexeme != COLON) {
	    LogSyntaxError(infoPtr, "missing colon from ternary conditional");
	    return TCL_ERROR;
	}
	code = GetLexeme(infoPtr); /* skip over the ':' */
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Parse the "else" expression.
	 */

	code = ParseCondExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Now set the size-related fields in the '?' subexpression token.
	 */

	condTokenPtr = &parsePtr->tokenPtr[firstIndex];
	condTokenPtr->size = (infoPtr->prevEnd - srcStart);
	condTokenPtr->numComponents = parsePtr->numTokens - (firstIndex+1);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseLorExpr --
 *
 *	This function parses a Tcl logical or expression:
 *	lorExpr ::= landExpr {'||' landExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseLorExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, code;
    CONST char *srcStart, *operator;

    HERE("lorExpr", 2);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseLandExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    while (infoPtr->lexeme == OR) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr); /* skip over the '||' */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseLandExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the LOR subexpression and the '||' operator.
	 */

	PrependSubExprTokens(operator, 2, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseLandExpr --
 *
 *	This function parses a Tcl logical and expression:
 *	landExpr ::= bitOrExpr {'&&' bitOrExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseLandExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, code;
    CONST char *srcStart, *operator;

    HERE("landExpr", 3);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseBitOrExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    while (infoPtr->lexeme == AND) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr); /* skip over the '&&' */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseBitOrExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the LAND subexpression and the '&&' operator.
	 */

	PrependSubExprTokens(operator, 2, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseBitOrExpr --
 *
 *	This function parses a Tcl bitwise or expression:
 *	bitOrExpr ::= bitXorExpr {'|' bitXorExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseBitOrExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, code;
    CONST char *srcStart, *operator;

    HERE("bitOrExpr", 4);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseBitXorExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    while (infoPtr->lexeme == BIT_OR) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr); /* skip over the '|' */
	if (code != TCL_OK) {
	    return code;
	}

	code = ParseBitXorExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the BITOR subexpression and the '|' operator.
	 */

	PrependSubExprTokens(operator, 1, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseBitXorExpr --
 *
 *	This function parses a Tcl bitwise exclusive or expression:
 *	bitXorExpr ::= bitAndExpr {'^' bitAndExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseBitXorExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, code;
    CONST char *srcStart, *operator;

    HERE("bitXorExpr", 5);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseBitAndExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    while (infoPtr->lexeme == BIT_XOR) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr); /* skip over the '^' */
	if (code != TCL_OK) {
	    return code;
	}

	code = ParseBitAndExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the XOR subexpression and the '^' operator.
	 */

	PrependSubExprTokens(operator, 1, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseBitAndExpr --
 *
 *	This function parses a Tcl bitwise and expression:
 *	bitAndExpr ::= equalityExpr {'&' equalityExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseBitAndExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, code;
    CONST char *srcStart, *operator;

    HERE("bitAndExpr", 6);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseEqualityExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    while (infoPtr->lexeme == BIT_AND) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr); /* skip over the '&' */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseEqualityExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the BITAND subexpression and '&' operator.
	 */

	PrependSubExprTokens(operator, 1, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseEqualityExpr --
 *
 *	This function parses a Tcl equality (inequality) expression:
 *	equalityExpr ::= relationalExpr
 *		{('==' | '!=' | 'ne' | 'eq') relationalExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseEqualityExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, lexeme, code;
    CONST char *srcStart, *operator;

    HERE("equalityExpr", 7);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseRelationalExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    lexeme = infoPtr->lexeme;
    while (lexeme == EQUAL || lexeme == NEQ || lexeme == NOT_IN_LIST ||
	    lexeme == IN_LIST || lexeme == STREQ || lexeme == STRNEQ) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr); /* skip over ==, !=, 'eq' or 'ne'  */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseRelationalExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the subexpression and '==', '!=', 'eq' or 'ne'
	 * operator.
	 */

	PrependSubExprTokens(operator, 2, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
	lexeme = infoPtr->lexeme;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseRelationalExpr --
 *
 *	This function parses a Tcl relational expression:
 *	relationalExpr ::= shiftExpr {('<' | '>' | '<=' | '>=') shiftExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseRelationalExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, lexeme, operatorSize, code;
    CONST char *srcStart, *operator;

    HERE("relationalExpr", 8);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseShiftExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    lexeme = infoPtr->lexeme;
    while ((lexeme == LESS) || (lexeme == GREATER) || (lexeme == LEQ)
	    || (lexeme == GEQ)) {
	operator = infoPtr->start;
	if ((lexeme == LEQ) || (lexeme == GEQ)) {
	    operatorSize = 2;
	} else {
	    operatorSize = 1;
	}
	code = GetLexeme(infoPtr); /* skip over the operator */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseShiftExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the subexpression and the operator.
	 */

	PrependSubExprTokens(operator, operatorSize, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
	lexeme = infoPtr->lexeme;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseShiftExpr --
 *
 *	This function parses a Tcl shift expression:
 *	shiftExpr ::= addExpr {('<<' | '>>') addExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseShiftExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, lexeme, code;
    CONST char *srcStart, *operator;

    HERE("shiftExpr", 9);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseAddExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    lexeme = infoPtr->lexeme;
    while ((lexeme == LEFT_SHIFT) || (lexeme == RIGHT_SHIFT)) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr);	/* skip over << or >> */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseAddExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the subexpression and '<<' or '>>' operator.
	 */

	PrependSubExprTokens(operator, 2, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
	lexeme = infoPtr->lexeme;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseAddExpr --
 *
 *	This function parses a Tcl addition expression:
 *	addExpr ::= multiplyExpr {('+' | '-') multiplyExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseAddExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, lexeme, code;
    CONST char *srcStart, *operator;

    HERE("addExpr", 10);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseMultiplyExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    lexeme = infoPtr->lexeme;
    while ((lexeme == PLUS) || (lexeme == MINUS)) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr); /* skip over + or - */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseMultiplyExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the subexpression and '+' or '-' operator.
	 */

	PrependSubExprTokens(operator, 1, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
	lexeme = infoPtr->lexeme;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseMultiplyExpr --
 *
 *	This function parses a Tcl multiply expression:
 *	multiplyExpr ::= exponentialExpr {('*' | '/' | '%') exponentialExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseMultiplyExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, lexeme, code;
    CONST char *srcStart, *operator;

    HERE("multiplyExpr", 11);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseExponentialExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    lexeme = infoPtr->lexeme;
    while ((lexeme == MULT) || (lexeme == DIVIDE) || (lexeme == MOD)) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr);	/* skip over * or / or % */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseExponentialExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the subexpression and * or / or % operator.
	 */

	PrependSubExprTokens(operator, 1, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
	lexeme = infoPtr->lexeme;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseExponentialExpr --
 *
 *	This function parses a Tcl exponential expression:
 *	exponentialExpr ::= unaryExpr {'**' unaryExpr}
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseExponentialExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, lexeme, code;
    CONST char *srcStart, *operator;

    HERE("exponentiateExpr", 12);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    code = ParseUnaryExpr(infoPtr);
    if (code != TCL_OK) {
	return code;
    }

    lexeme = infoPtr->lexeme;
    while (lexeme == EXPON) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr);	/* skip over ** */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseUnaryExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the subexpression and ** operator.
	 */

	PrependSubExprTokens(operator, 2, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
	lexeme = infoPtr->lexeme;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseUnaryExpr --
 *
 *	This function parses a Tcl unary expression:
 *	unaryExpr ::= ('+' | '-' | '~' | '!') unaryExpr | primaryExpr
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParseUnaryExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    int firstIndex, lexeme, code;
    CONST char *srcStart, *operator;

    HERE("unaryExpr", 13);
    srcStart = infoPtr->start;
    firstIndex = parsePtr->numTokens;

    lexeme = infoPtr->lexeme;
    if ((lexeme == PLUS) || (lexeme == MINUS) || (lexeme == BIT_NOT)
	    || (lexeme == NOT)) {
	operator = infoPtr->start;
	code = GetLexeme(infoPtr); /* skip over the unary operator */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseUnaryExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}

	/*
	 * Generate tokens for the subexpression and the operator.
	 */

	PrependSubExprTokens(operator, 1, srcStart,
		(infoPtr->prevEnd - srcStart), firstIndex, infoPtr);
    } else {			/* must be a primaryExpr */
	code = ParsePrimaryExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ParsePrimaryExpr --
 *
 *	This function parses a Tcl primary expression:
 *	primaryExpr ::= literal | varReference | quotedString |
 *		'[' command ']' | mathFuncCall | '(' condExpr ')'
 *
 * Results:
 *	The return value is TCL_OK on a successful parse and TCL_ERROR on
 *	failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static int
ParsePrimaryExpr(
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    Tcl_Interp *interp = parsePtr->interp;
    Tcl_Token *tokenPtr, *exprTokenPtr;
    Tcl_Parse nested;
    CONST char *dollarPtr, *stringStart, *termPtr, *src;
    int lexeme, exprIndex, firstIndex, numToMove, code;

    /*
     * We simply recurse on parenthesized subexpressions.
     */

    HERE("primaryExpr", 14);
    lexeme = infoPtr->lexeme;
    if (lexeme == OPEN_PAREN) {
	code = GetLexeme(infoPtr); /* skip over the '(' */
	if (code != TCL_OK) {
	    return code;
	}
	code = ParseCondExpr(infoPtr);
	if (code != TCL_OK) {
	    return code;
	}
	if (infoPtr->lexeme != CLOSE_PAREN) {
	    LogSyntaxError(infoPtr, "looking for close parenthesis");
	    return TCL_ERROR;
	}
	code = GetLexeme(infoPtr); /* skip over the ')' */
	if (code != TCL_OK) {
	    return code;
	}
	return TCL_OK;
    }

    /*
     * Start a TCL_TOKEN_SUB_EXPR token for the primary.
     */

    TclGrowParseTokenArray(parsePtr,1);
    exprIndex = parsePtr->numTokens;
    exprTokenPtr = &parsePtr->tokenPtr[exprIndex];
    exprTokenPtr->type = TCL_TOKEN_SUB_EXPR;
    exprTokenPtr->start = infoPtr->start;
    parsePtr->numTokens++;

    /*
     * Process the primary then finish setting the fields of the
     * TCL_TOKEN_SUB_EXPR token. Note that we can't use the pointer now stored
     * in "exprTokenPtr" in the code below since the token array might be
     * reallocated.
     */

    firstIndex = parsePtr->numTokens;
    switch (lexeme) {
    case LITERAL:
	/*
	 * Int or double number.
	 */
	
    tokenizeLiteral:
	TclGrowParseTokenArray(parsePtr,1);
	tokenPtr = &parsePtr->tokenPtr[parsePtr->numTokens];
	tokenPtr->type = TCL_TOKEN_TEXT;
	tokenPtr->start = infoPtr->start;
	tokenPtr->size = infoPtr->size;
	tokenPtr->numComponents = 0;
	parsePtr->numTokens++;

	exprTokenPtr = &parsePtr->tokenPtr[exprIndex];
	exprTokenPtr->size = infoPtr->size;
	exprTokenPtr->numComponents = 1;
	break;

    case DOLLAR:
	/*
	 * $var variable reference.
	 */

	dollarPtr = (infoPtr->next - 1);
	code = Tcl_ParseVarName(interp, dollarPtr,
		(infoPtr->lastChar - dollarPtr), parsePtr, 1);
	if (code != TCL_OK) {
	    return code;
	}
	infoPtr->next = dollarPtr + parsePtr->tokenPtr[firstIndex].size;

	exprTokenPtr = &parsePtr->tokenPtr[exprIndex];
	exprTokenPtr->size = parsePtr->tokenPtr[firstIndex].size;
	exprTokenPtr->numComponents =
		(parsePtr->tokenPtr[firstIndex].numComponents + 1);
	break;

    case QUOTE:
	/*
	 * '"' string '"'
	 */

	stringStart = infoPtr->next;
	code = Tcl_ParseQuotedString(interp, infoPtr->start,
		(infoPtr->lastChar - stringStart), parsePtr, 1, &termPtr);
	if (code != TCL_OK) {
	    return code;
	}
	infoPtr->next = termPtr;

	exprTokenPtr = &parsePtr->tokenPtr[exprIndex];
	exprTokenPtr->size = (termPtr - exprTokenPtr->start);
	exprTokenPtr->numComponents = parsePtr->numTokens - firstIndex;

	/*
	 * If parsing the quoted string resulted in more than one token,
	 * insert a TCL_TOKEN_WORD token before them. This indicates that the
	 * quoted string represents a concatenation of multiple tokens.
	 */

	if (exprTokenPtr->numComponents > 1) {
	    TclGrowParseTokenArray(parsePtr,1);
	    tokenPtr = &parsePtr->tokenPtr[firstIndex];
	    numToMove = (parsePtr->numTokens - firstIndex);
	    memmove((VOID *) (tokenPtr + 1), (VOID *) tokenPtr,
		    (size_t) (numToMove * sizeof(Tcl_Token)));
	    parsePtr->numTokens++;

	    exprTokenPtr = &parsePtr->tokenPtr[exprIndex];
	    exprTokenPtr->numComponents++;

	    tokenPtr->type = TCL_TOKEN_WORD;
	    tokenPtr->start = exprTokenPtr->start;
	    tokenPtr->size = exprTokenPtr->size;
	    tokenPtr->numComponents = (exprTokenPtr->numComponents - 1);
	}
	break;

    case OPEN_BRACKET:
	/*
	 * '[' command {command} ']'
	 */

	/*
	 * Call TclParseScript, or Tcl_ParseCommand repeatedly, to parse
	 * the nested command(s).  If internal tokens are acceptable, keep
	 * all the parsing info; otherwise, throw it away.
	 */
	
	src = infoPtr->next;
	if (infoPtr->useInternalTokens) {
	    CONST char *term;
	    Tcl_Token *lastTokenPtr;
	    Tcl_Token *appendTokens = TclParseScript(src,
		    (int) (parsePtr->end - src), PARSE_NESTED,
		    &lastTokenPtr, &term);
	    int numTokens = 1 + (int) (lastTokenPtr - appendTokens);

	    TclGrowParseTokenArray(parsePtr,numTokens+1);
	    tokenPtr = &parsePtr->tokenPtr[parsePtr->numTokens];
	    tokenPtr->type = TCL_TOKEN_SCRIPT_SUBST;
	    tokenPtr->size = term - src + 2;
	    tokenPtr->numComponents = numTokens;

	    memcpy(tokenPtr+1, appendTokens,
		    (size_t) (numTokens * sizeof(Tcl_Token)));
	    parsePtr->numTokens += (numTokens + 1);
	    lastTokenPtr = parsePtr->tokenPtr + parsePtr->numTokens - 1;
	    ckfree((char *) appendTokens);

	    exprTokenPtr = &parsePtr->tokenPtr[exprIndex];
	    exprTokenPtr->size = (src - tokenPtr->start);
	    exprTokenPtr->numComponents = 1 + numTokens;

	    if (lastTokenPtr->type == TCL_TOKEN_ERROR) {
		parsePtr->errorType = lastTokenPtr->numComponents;
		parsePtr->term = term;
		parsePtr->incomplete = 1;
		infoPtr->next = parsePtr->end;
		if (interp != NULL) {
		    TclSubstTokens(interp, lastTokenPtr, 1, NULL, 0);
		}
		return TCL_ERROR;
	    } else {
		infoPtr->next = term + 1;
	    }
	    break;
	}

	TclGrowParseTokenArray(parsePtr,1);
	tokenPtr = &parsePtr->tokenPtr[parsePtr->numTokens];
	tokenPtr->type = TCL_TOKEN_COMMAND;
	tokenPtr->start = infoPtr->start;
	tokenPtr->numComponents = 0;
	parsePtr->numTokens++;

	while (1) {
	    if (Tcl_ParseCommand(interp, src, (parsePtr->end - src), 1,
		    &nested) != TCL_OK) {
		parsePtr->term = nested.term;
		parsePtr->errorType = nested.errorType;
		parsePtr->incomplete = nested.incomplete;
		return TCL_ERROR;
	    }
	    src = (nested.commandStart + nested.commandSize);

	    /*
	     * This is equivalent to Tcl_FreeParse(&nested), but presumably
	     * inlined here for sake of runtime optimization
	     */

	    if (nested.tokenPtr != nested.staticTokens) {
		ckfree((char *) nested.tokenPtr);
	    }

	    /*
	     * Check for the closing ']' that ends the command substitution.
	     * It must have been the last character of the parsed command.
	     */

	    if ((nested.term < parsePtr->end) && (*nested.term == ']')
		    && !nested.incomplete) {
		break;
	    }
	    if (src == parsePtr->end) {
		if (parsePtr->interp != NULL) {
		    Tcl_SetResult(interp, "missing close-bracket",
			    TCL_STATIC);
		}
		parsePtr->term = tokenPtr->start;
		parsePtr->errorType = TCL_PARSE_MISSING_BRACKET;
		parsePtr->incomplete = 1;
		return TCL_ERROR;
	    }
	}
	tokenPtr->size = (src - tokenPtr->start);
	infoPtr->next = src;

	exprTokenPtr = &parsePtr->tokenPtr[exprIndex];
	exprTokenPtr->size = (src - tokenPtr->start);
	exprTokenPtr->numComponents = 1;
	break;

    case OPEN_BRACE:
	/*
	 * '{' string '}'
	 */

	code = Tcl_ParseBraces(interp, infoPtr->start,
		(infoPtr->lastChar - infoPtr->start), parsePtr, 1, &termPtr);
	if (code != TCL_OK) {
	    return code;
	}
	infoPtr->next = termPtr;

	exprTokenPtr = &parsePtr->tokenPtr[exprIndex];
	exprTokenPtr->size = (termPtr - infoPtr->start);
	exprTokenPtr->numComponents = parsePtr->numTokens - firstIndex;

	/*
	 * If parsing the braced string resulted in more than one token,
	 * insert a TCL_TOKEN_WORD token before them. This indicates that the
	 * braced string represents a concatenation of multiple tokens.
	 */

	if (exprTokenPtr->numComponents > 1) {
	    TclGrowParseTokenArray(parsePtr,1);
	    tokenPtr = &parsePtr->tokenPtr[firstIndex];
	    numToMove = (parsePtr->numTokens - firstIndex);
	    memmove((VOID *) (tokenPtr + 1), (VOID *) tokenPtr,
		    (size_t) (numToMove * sizeof(Tcl_Token)));
	    parsePtr->numTokens++;

	    exprTokenPtr = &parsePtr->tokenPtr[exprIndex];
	    exprTokenPtr->numComponents++;

	    tokenPtr->type = TCL_TOKEN_WORD;
	    tokenPtr->start = exprTokenPtr->start;
	    tokenPtr->size = exprTokenPtr->size;
	    tokenPtr->numComponents = exprTokenPtr->numComponents-1;
	}
	break;

    case STREQ:
    case STRNEQ:
    case IN_LIST:
    case NOT_IN_LIST:
    case FUNC_NAME: {
	/*
	 * math_func '(' expr {',' expr} ')'
	 */
	ParseInfo savedInfo = *infoPtr;

	code = GetLexeme(infoPtr);	/* skip over function name */
	if (code != TCL_OK) {
	    return code;
	}
	if (infoPtr->lexeme != OPEN_PAREN) {

	    int code;
	    Tcl_Obj *errMsg, *objPtr =
		    Tcl_NewStringObj(savedInfo.start, savedInfo.size);

	    /*
	     * Check for boolean literals (true, false, yes, no, on, off).
	     */

	    Tcl_IncrRefCount(objPtr);
	    code = Tcl_ConvertToType(NULL, objPtr, &tclBooleanType);
	    Tcl_DecrRefCount(objPtr);
	    if (code == TCL_OK) {
		*infoPtr = savedInfo;
		goto tokenizeLiteral;
	    }

	    /*
	     * Either there's a math function without a (, or a variable name
	     * without a '$'.
	     */

	    errMsg = Tcl_NewStringObj( "syntax error in expression \"", -1 );
	    TclAppendLimitedToObj(errMsg, infoPtr->originalExpr,
		    (int) (infoPtr->lastChar - infoPtr->originalExpr),
		    63, NULL);
	    Tcl_AppendToObj(errMsg, "\": the word \"", -1);
	    Tcl_AppendToObj(errMsg, savedInfo.start, savedInfo.size);
	    Tcl_AppendToObj(errMsg,
		    "\" requires a preceding $ if it's a variable ", -1);
	    Tcl_AppendToObj(errMsg,
		    "or function arguments if it's a function", -1);
	    Tcl_SetObjResult(infoPtr->parsePtr->interp, errMsg);
	    infoPtr->parsePtr->errorType = TCL_PARSE_SYNTAX;
	    infoPtr->parsePtr->term = infoPtr->start;
	    return TCL_ERROR;

	}

	TclGrowParseTokenArray(parsePtr,1);
	tokenPtr = &parsePtr->tokenPtr[parsePtr->numTokens];
	tokenPtr->type = TCL_TOKEN_OPERATOR;
	tokenPtr->start = savedInfo.start;
	tokenPtr->size = savedInfo.size;
	tokenPtr->numComponents = 0;
	parsePtr->numTokens++;

	code = GetLexeme(infoPtr);	/* skip over '(' */
	if (code != TCL_OK) {
	    return code;
	}

	while (infoPtr->lexeme != CLOSE_PAREN) {
	    code = ParseCondExpr(infoPtr);
	    if (code != TCL_OK) {
		return code;
	    }

	    if (infoPtr->lexeme == COMMA) {
		code = GetLexeme(infoPtr); /* skip over , */
		if (code != TCL_OK) {
		    return code;
		}
	    } else if (infoPtr->lexeme != CLOSE_PAREN) {
		LogSyntaxError(infoPtr,
			"missing close parenthesis at end of function call");
		return TCL_ERROR;
	    }
	}

	exprTokenPtr = &parsePtr->tokenPtr[exprIndex];
	exprTokenPtr->size = (infoPtr->next - exprTokenPtr->start);
	exprTokenPtr->numComponents = parsePtr->numTokens - firstIndex;
	break;
    }

    case COMMA:
	LogSyntaxError(infoPtr,
		"commas can only separate function arguments");
	return TCL_ERROR;
    case END:
	LogSyntaxError(infoPtr, "premature end of expression");
	return TCL_ERROR;
    case UNKNOWN:
	LogSyntaxError(infoPtr,
		"single equality character not legal in expressions");
	return TCL_ERROR;
    case UNKNOWN_CHAR:
	LogSyntaxError(infoPtr, "character not legal in expressions");
	return TCL_ERROR;
    case QUESTY:
	LogSyntaxError(infoPtr, "unexpected ternary 'then' separator");
	return TCL_ERROR;
    case COLON:
	LogSyntaxError(infoPtr, "unexpected ternary 'else' separator");
	return TCL_ERROR;
    case CLOSE_PAREN:
	LogSyntaxError(infoPtr, "unexpected close parenthesis");
	return TCL_ERROR;

    default:
	{
	    char buf[64];

	    sprintf(buf, "unexpected operator %s", lexemeStrings[lexeme]);
	    LogSyntaxError(infoPtr, buf);
	    return TCL_ERROR;
	}
    }

    /*
     * Advance to the next lexeme before returning.
     */

    code = GetLexeme(infoPtr);
    if (code != TCL_OK) {
	return code;
    }
    parsePtr->term = infoPtr->next;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetLexeme --
 *
 *	Lexical scanner for Tcl expressions: scans a single operator or other
 *	syntactic element from an expression string.
 *
 * Results:
 *	TCL_OK is returned unless an error occurred. In that case a standard
 *	Tcl error code is returned and, if infoPtr->parsePtr->interp is
 *	non-NULL, the interpreter's result is set to hold an error message.
 *	TCL_ERROR is returned if an integer overflow, or a floating-point
 *	overflow or underflow occurred while reading in a number. If the
 *	lexical analysis is successful, infoPtr->lexeme refers to the next
 *	symbol in the expression string, and infoPtr->next is advanced past
 *	the lexeme. Also, if the lexeme is a LITERAL or FUNC_NAME, then
 *	infoPtr->start is set to the first character of the lexeme; otherwise
 *	it is set NULL.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the subexpression, then additional space is malloc-ed..
 *
 *----------------------------------------------------------------------
 */

static int
GetLexeme(
    ParseInfo *infoPtr)		/* Holds state needed to parse the expr,
				 * including the resulting lexeme. */
{
    register CONST char *src;	/* Points to current source char. */
    char c;
    int offset, length, numBytes;
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    Tcl_UniChar ch;

    /*
     * Record where the previous lexeme ended. Since we always read one lexeme
     * ahead during parsing, this helps us know the source length of
     * subexpression tokens.
     */

    infoPtr->prevEnd = infoPtr->next;

    /*
     * Scan over leading white space at the start of a lexeme.
     */

    src = infoPtr->next;
    numBytes = parsePtr->end - src;

    length = TclParseAllWhiteSpace(src, numBytes);
    src += length;
    numBytes -= length;

    parsePtr->term = src;
    if (numBytes == 0) {
	infoPtr->lexeme = END;
	infoPtr->next = src;
	return TCL_OK;
    }

    /*
     * Try to parse the lexeme first as an integer or floating-point number.
     * Don't check for a number if the first character c is "+" or "-". If we
     * did, we might treat a binary operator as unary by mistake, which would
     * eventually cause a syntax error.
     */

    c = *src;
    if ((c != '+') && (c != '-')) {
	CONST char *end = infoPtr->lastChar;
	CONST char* end2;
	int code = TclParseNumber(NULL, NULL, NULL, src, (int)(end-src),
		&end2, TCL_PARSE_NO_WHITESPACE);

	if (code == TCL_OK) {
	    length = end2-src;
	    if (length > 0) {
		infoPtr->lexeme = LITERAL;
		infoPtr->start = src;
		infoPtr->size = length;
		infoPtr->next = (src + length);
		parsePtr->term = infoPtr->next;
		return TCL_OK;
	    }
	}
    }

    /*
     * Not an integer or double literal. Initialize the lexeme's fields
     * assuming the common case of a single character lexeme.
     */

    infoPtr->start = src;
    infoPtr->size = 1;
    infoPtr->next = src+1;
    parsePtr->term = infoPtr->next;

    switch (*src) {
    case '[':
	infoPtr->lexeme = OPEN_BRACKET;
	return TCL_OK;

    case '{':
	infoPtr->lexeme = OPEN_BRACE;
	return TCL_OK;

    case '(':
	infoPtr->lexeme = OPEN_PAREN;
	return TCL_OK;

    case ')':
	infoPtr->lexeme = CLOSE_PAREN;
	return TCL_OK;

    case '$':
	infoPtr->lexeme = DOLLAR;
	return TCL_OK;

    case '\"':
	infoPtr->lexeme = QUOTE;
	return TCL_OK;

    case ',':
	infoPtr->lexeme = COMMA;
	return TCL_OK;

    case '*':
	infoPtr->lexeme = MULT;
	if ((infoPtr->lastChar - src)>1  &&  src[1]=='*') {
	    infoPtr->lexeme = EXPON;
	    infoPtr->size = 2;
	    infoPtr->next = src+2;
	    parsePtr->term = infoPtr->next;
	}
	return TCL_OK;

    case '/':
	infoPtr->lexeme = DIVIDE;
	return TCL_OK;

    case '%':
	infoPtr->lexeme = MOD;
	return TCL_OK;

    case '+':
	infoPtr->lexeme = PLUS;
	return TCL_OK;

    case '-':
	infoPtr->lexeme = MINUS;
	return TCL_OK;

    case '?':
	infoPtr->lexeme = QUESTY;
	return TCL_OK;

    case ':':
	infoPtr->lexeme = COLON;
	return TCL_OK;

    case '<':
	infoPtr->lexeme = LESS;
	if ((infoPtr->lastChar - src) > 1) {
	    switch (src[1]) {
	    case '<':
		infoPtr->lexeme = LEFT_SHIFT;
		infoPtr->size = 2;
		infoPtr->next = src+2;
		break;
	    case '=':
		infoPtr->lexeme = LEQ;
		infoPtr->size = 2;
		infoPtr->next = src+2;
		break;
	    }
	}
	parsePtr->term = infoPtr->next;
	return TCL_OK;

    case '>':
	infoPtr->lexeme = GREATER;
	if ((infoPtr->lastChar - src) > 1) {
	    switch (src[1]) {
	    case '>':
		infoPtr->lexeme = RIGHT_SHIFT;
		infoPtr->size = 2;
		infoPtr->next = src+2;
		break;
	    case '=':
		infoPtr->lexeme = GEQ;
		infoPtr->size = 2;
		infoPtr->next = src+2;
		break;
	    }
	}
	parsePtr->term = infoPtr->next;
	return TCL_OK;

    case '=':
	infoPtr->lexeme = UNKNOWN;
	if ((src[1] == '=') && ((infoPtr->lastChar - src) > 1)) {
	    infoPtr->lexeme = EQUAL;
	    infoPtr->size = 2;
	    infoPtr->next = src+2;
	}
	parsePtr->term = infoPtr->next;
	return TCL_OK;

    case '!':
	infoPtr->lexeme = NOT;
	if ((src[1] == '=') && ((infoPtr->lastChar - src) > 1)) {
	    infoPtr->lexeme = NEQ;
	    infoPtr->size = 2;
	    infoPtr->next = src+2;
	}
	parsePtr->term = infoPtr->next;
	return TCL_OK;

    case '&':
	infoPtr->lexeme = BIT_AND;
	if ((src[1] == '&') && ((infoPtr->lastChar - src) > 1)) {
	    infoPtr->lexeme = AND;
	    infoPtr->size = 2;
	    infoPtr->next = src+2;
	}
	parsePtr->term = infoPtr->next;
	return TCL_OK;

    case '^':
	infoPtr->lexeme = BIT_XOR;
	return TCL_OK;

    case '|':
	infoPtr->lexeme = BIT_OR;
	if ((src[1] == '|') && ((infoPtr->lastChar - src) > 1)) {
	    infoPtr->lexeme = OR;
	    infoPtr->size = 2;
	    infoPtr->next = src+2;
	}
	parsePtr->term = infoPtr->next;
	return TCL_OK;

    case '~':
	infoPtr->lexeme = BIT_NOT;
	return TCL_OK;

    case 'e':
	if ((src[1] == 'q') && ((infoPtr->lastChar - src) > 1) &&
		(infoPtr->lastChar-src==2 || !isalpha(UCHAR(src[2])))) {
	    infoPtr->lexeme = STREQ;
	    infoPtr->size = 2;
	    infoPtr->next = src+2;
	    parsePtr->term = infoPtr->next;
	    return TCL_OK;
	} else {
	    goto checkFuncName;
	}

    case 'n':
	if ((src[1] == 'e') && ((infoPtr->lastChar - src) > 1) &&
		(infoPtr->lastChar-src==2 || !isalpha(UCHAR(src[2])))) {
	    infoPtr->lexeme = STRNEQ;
	    infoPtr->size = 2;
	    infoPtr->next = src+2;
	    parsePtr->term = infoPtr->next;
	    return TCL_OK;
	} else if ((src[1] == 'i') && ((infoPtr->lastChar - src) > 1) &&
		(infoPtr->lastChar-src==2 || !isalpha(UCHAR(src[2])))) {
	    infoPtr->lexeme = NOT_IN_LIST;
	    infoPtr->size = 2;
	    infoPtr->next = src+2;
	    parsePtr->term = infoPtr->next;
	    return TCL_OK;
	} else {
	    goto checkFuncName;
	}

    case 'i':
	if ((src[1] == 'n') && ((infoPtr->lastChar - src) > 1) &&
		(infoPtr->lastChar-src==2 || !isalpha(UCHAR(src[2])))) {
	    infoPtr->lexeme = IN_LIST;
	    infoPtr->size = 2;
	    infoPtr->next = src+2;
	    parsePtr->term = infoPtr->next;
	    return TCL_OK;
	} else {
	    goto checkFuncName;
	}

    default:
    checkFuncName:
	length = (infoPtr->lastChar - src);
	if (Tcl_UtfCharComplete(src, length)) {
	    offset = Tcl_UtfToUniChar(src, &ch);
	} else {
	    char utfBytes[TCL_UTF_MAX];

	    memcpy(utfBytes, src, (size_t) length);
	    utfBytes[length] = '\0';
	    offset = Tcl_UtfToUniChar(utfBytes, &ch);
	}
	c = UCHAR(ch);
	if (isalpha(UCHAR(c))) {			/* INTL: ISO only. */
	    infoPtr->lexeme = FUNC_NAME;
	    while (isalnum(UCHAR(c)) || (c == '_')) {	/* INTL: ISO only. */
		src += offset;
		length -= offset;
		if (Tcl_UtfCharComplete(src, length)) {
		    offset = Tcl_UtfToUniChar(src, &ch);
		} else {
		    char utfBytes[TCL_UTF_MAX];

		    memcpy(utfBytes, src, (size_t) length);
		    utfBytes[length] = '\0';
		    offset = Tcl_UtfToUniChar(utfBytes, &ch);
		}
		c = UCHAR(ch);
	    }
	    infoPtr->size = (src - infoPtr->start);
	    infoPtr->next = src;
	    parsePtr->term = infoPtr->next;
	    return TCL_OK;
	}
	infoPtr->lexeme = UNKNOWN_CHAR;
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PrependSubExprTokens --
 *
 *	This function is called after the operands of an subexpression have
 *	been parsed. It generates two tokens: a TCL_TOKEN_SUB_EXPR token for
 *	the subexpression, and a TCL_TOKEN_OPERATOR token for its operator.
 *	These two tokens are inserted before the operand tokens.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold the new tokens,
 *	additional space is malloc-ed.
 *
 *----------------------------------------------------------------------
 */

static void
PrependSubExprTokens(
    CONST char *op,		/* Points to first byte of the operator in the
				 * source script. */
    int opBytes,		/* Number of bytes in the operator. */
    CONST char *src,		/* Points to first byte of the subexpression
				 * in the source script. */
    int srcBytes,		/* Number of bytes in subexpression's
				 * source. */
    int firstIndex,		/* Index of first token already emitted for
				 * operator's first (or only) operand. */
    ParseInfo *infoPtr)		/* Holds the parse state for the expression
				 * being parsed. */
{
    Tcl_Parse *parsePtr = infoPtr->parsePtr;
    Tcl_Token *tokenPtr, *firstTokenPtr;
    int numToMove;

    TclGrowParseTokenArray(parsePtr,2);
    firstTokenPtr = &parsePtr->tokenPtr[firstIndex];
    tokenPtr = (firstTokenPtr + 2);
    numToMove = (parsePtr->numTokens - firstIndex);
    memmove((VOID *) tokenPtr, (VOID *) firstTokenPtr,
	    (size_t) (numToMove * sizeof(Tcl_Token)));
    parsePtr->numTokens += 2;

    tokenPtr = firstTokenPtr;
    tokenPtr->type = TCL_TOKEN_SUB_EXPR;
    tokenPtr->start = src;
    tokenPtr->size = srcBytes;
    tokenPtr->numComponents = parsePtr->numTokens - (firstIndex + 1);

    tokenPtr++;
    tokenPtr->type = TCL_TOKEN_OPERATOR;
    tokenPtr->start = op;
    tokenPtr->size = opBytes;
    tokenPtr->numComponents = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * LogSyntaxError --
 *
 *	This function is invoked after an error occurs when parsing an
 *	expression. It sets the interpreter result to an error message
 *	describing the error.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the interpreter result to an error message describing the
 *	expression that was being parsed when the error occurred, and why the
 *	parser considers that to be a syntax error at all.
 *
 *----------------------------------------------------------------------
 */

static void
LogSyntaxError(
    ParseInfo *infoPtr,		/* Holds the parse state for the expression
				 * being parsed. */
    CONST char *extraInfo)	/* String to provide extra information about
				 * the syntax error. */
{
    Tcl_Obj *result =
	    Tcl_NewStringObj("syntax error in expression \"", -1);

    TclAppendLimitedToObj(result, infoPtr->originalExpr,
	    (int)(infoPtr->lastChar - infoPtr->originalExpr), 63, NULL);
    Tcl_AppendStringsToObj(result, "\": ", extraInfo, NULL);
    Tcl_SetObjResult(infoPtr->parsePtr->interp, result);
    infoPtr->parsePtr->errorType = TCL_PARSE_SYNTAX;
    infoPtr->parsePtr->term = infoPtr->start;
}
#else


#include "tclInt.h"

/*
 * The ExprNode structure represents one node of the parse tree produced
 * as an interim structure by the expression parser.
 */

typedef struct ExprNode {
    unsigned char lexeme;	/* Code that identifies the type of this node */
    int left;		/* Index of the left operand of this operator node */
    int right;		/* Index of the right operand of this operator node */
    int parent;		/* Index of the operator of this operand node */
    int token;		/* Index of the Tcl_Tokens of this leaf node */
} ExprNode;

/*
 * Set of lexeme codes stored in ExprNode structs to label and categorize
 * the lexemes found.
 */

#define LEAF		(1<<7)
#define UNARY		(1<<6)
#define BINARY		(1<<5)

#define NODE_TYPE	( LEAF | UNARY | BINARY)

#define PLUS		1
#define MINUS		2
#define BAREWORD	3
#define INCOMPLETE	4
#define INVALID		5

#define NUMBER		( LEAF | 1)
#define SCRIPT		( LEAF | 2)
#define BOOLEAN		( LEAF | BAREWORD)
#define BRACED		( LEAF | 4)
#define VARIABLE	( LEAF | 5)
#define QUOTED		( LEAF | 6)
#define EMPTY		( LEAF | 7)

#define UNARY_PLUS	( UNARY | PLUS)
#define UNARY_MINUS	( UNARY | MINUS)
#define FUNCTION	( UNARY | BAREWORD)
#define START		( UNARY | 4)
#define OPEN_PAREN	( UNARY | 5)
#define NOT		( UNARY | 6)
#define BIT_NOT		( UNARY | 7)

#define BINARY_PLUS	( BINARY |  PLUS)
#define BINARY_MINUS	( BINARY |  MINUS)
#define COMMA		( BINARY |  3)
#define MULT		( BINARY |  4)
#define DIVIDE		( BINARY |  5)
#define MOD		( BINARY |  6)
#define LESS		( BINARY |  7)
#define GREATER		( BINARY |  8)
#define BIT_AND		( BINARY |  9)
#define BIT_XOR		( BINARY | 10)
#define BIT_OR		( BINARY | 11)
#define QUESTION	( BINARY | 12)
#define COLON		( BINARY | 13)
#define LEFT_SHIFT	( BINARY | 14)
#define RIGHT_SHIFT	( BINARY | 15)
#define LEQ		( BINARY | 16)
#define GEQ		( BINARY | 17)
#define EQUAL		( BINARY | 18)
#define NEQ		( BINARY | 19)
#define AND		( BINARY | 20)
#define OR		( BINARY | 21)
#define STREQ		( BINARY | 22)
#define STRNEQ		( BINARY | 23)
#define EXPON		( BINARY | 24)
#define IN_LIST		( BINARY | 25)
#define NOT_IN_LIST	( BINARY | 26)
#define CLOSE_PAREN	( BINARY | 27)
#define END		( BINARY | 28)

/*
 * Declarations for local functions to this file:
 */

static void		GenerateTokens(ExprNode *nodes, Tcl_Parse *scratchPtr,
				Tcl_Parse *parsePtr);
static int		ParseLexeme(CONST char *start, int numBytes,
				unsigned char *lexemePtr);


/*
 *----------------------------------------------------------------------
 *
 * Tcl_ParseExpr --
 *
 *	Given a string, the numBytes bytes starting at start, this function
 *	parses it as a Tcl expression and stores information about the
 *	structure of the expression in the Tcl_Parse struct indicated by the
 *	caller.
 *
 * Results:
 * 	If the string is successfully parsed as a valid Tcl expression,
 *	TCL_OK is returned, and data about the expression structure is
 *	written to *parsePtr.  If the string cannot be parsed as a valid
 *	Tcl expression, TCL_ERROR is returned, and if interp is non-NULL,
 *	an error message is written to interp.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the expression, then additional space is malloc-ed. If the
 *	function returns TCL_OK then the caller must eventually invoke
 *	Tcl_FreeParse to release any additional space that was allocated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ParseExpr(
    Tcl_Interp *interp,		/* Used for error reporting. */
    CONST char *start,		/* Start of source string to parse. */
    int numBytes,		/* Number of bytes in string. If < 0, the
				 * string consists of all bytes up to the
				 * first null character. */
    Tcl_Parse *parsePtr)	/* Structure to fill with information about
				 * the parsed expression; any previous
				 * information in the structure is ignored. */
{
    int code = TclParseExpr(interp, start, numBytes, 0, parsePtr);
    return code;
}

int
TclParseExpr(
    Tcl_Interp *interp,		/* Used for error reporting. */
    CONST char *start,		/* Start of source string to parse. */
    int numBytes,		/* Number of bytes in string. If < 0, the
				 * string consists of all bytes up to the
				 * first null character. */
    int useInternalTokens,	/* Boolean indicating whether internal
				 * token types are acceptable */
    Tcl_Parse *parsePtr)	/* Structure to fill with information about
				 * the parsed expression; any previous
				 * information in the structure is ignored. */
{
#define NUM_STATIC_NODES 64
    ExprNode staticNodes[NUM_STATIC_NODES];
    ExprNode *lastOrphanPtr, *nodes = staticNodes;
    int nodesAvailable = NUM_STATIC_NODES;
    int nodesUsed = 0;
    Tcl_Parse scratch;		/* Parsing scratch space */
    Tcl_Obj *msg = NULL, *post = NULL;
    unsigned char precedence;
    CONST char *end, *mark = "_@_";
    int scanned = 0, code = TCL_OK, insertMark = 0;
    CONST int limit = 25;
    static unsigned char prec[80] = {
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  15,	15, 5,	16, 16,	16, 13,	13, 11,	10, 9,	6,  6,	14, 14,
	13, 13, 12, 12,	8,  7,	12, 12,	17, 12,	12, 3,	1,  0,	0,  0,
	0,  18,	18, 18,	2,  4,	18, 18,	0,  0,	0,  0,	0,  0,	0,  0,
    };

    if (numBytes < 0) {
	numBytes = (start ? strlen(start) : 0);
    }

    TclParseInit(interp, start, numBytes, &scratch);
    TclParseInit(interp, start, numBytes, parsePtr);

    /* Initialize the parse tree with the special "START" node */

    nodes->lexeme = START;
    nodes->left = -1;
    nodes->right = -1;
    nodes->parent = -1;
    nodes->token = -1;
    lastOrphanPtr = nodes;
    nodesUsed++;

    while ((code == TCL_OK) && (lastOrphanPtr->lexeme != END)) {
	ExprNode *nodePtr, *lastNodePtr;
	Tcl_Token *tokenPtr;

	/*
	 * Each pass through this loop adds one more ExprNode.
	 * Allocate space for one if required.
	 */
	if (nodesUsed >= nodesAvailable) {
	    int lastOrphanIdx = lastOrphanPtr - nodes;
	    int size = nodesUsed * 2;
	    ExprNode *newPtr;

	    if (nodes == staticNodes) {
		nodes = NULL;
	    }
	    do {
		newPtr = (ExprNode *) attemptckrealloc( (char *) nodes,
			(unsigned int) (size * sizeof(ExprNode)) );
	    } while ((newPtr == NULL)
		    && ((size -= (size - nodesUsed) / 2) > nodesUsed));
	    if (newPtr == NULL) {
		msg = Tcl_NewStringObj(
			"not enough memory to parse expression", -1);
		code = TCL_ERROR;
		continue;
	    }
	    nodesAvailable = size;
	    if (nodes == NULL) {
		memcpy((VOID *) newPtr, (VOID *) staticNodes,
			(size_t) (nodesUsed * sizeof(ExprNode)));
	    }
	    nodes = newPtr;
	    lastOrphanPtr = nodes + lastOrphanIdx;
	}
	nodePtr = nodes + nodesUsed;
	lastNodePtr = nodePtr - 1;

	/* Skip white space between lexemes */

	scanned = TclParseAllWhiteSpace(start, numBytes);
	start += scanned;
	numBytes -= scanned;

	scanned = ParseLexeme(start, numBytes, &(nodePtr->lexeme));

	/* Use context to categorize the lexemes that are ambiguous */

	if ((NODE_TYPE & nodePtr->lexeme) == 0) {
	    switch (nodePtr->lexeme) {
	    case INVALID:
		msg = Tcl_NewObj();
		TclObjPrintf(NULL, msg,
			"invalid character \"%.*s\"", scanned, start);
		code = TCL_ERROR;
		continue;
	    case INCOMPLETE:
		msg = Tcl_NewObj();
		TclObjPrintf(NULL, msg,
			"incomplete operator \"%.*s\"", scanned, start);
		code = TCL_ERROR;
		continue;
	    case BAREWORD:
		if (start[scanned+TclParseAllWhiteSpace(
			start+scanned, numBytes-scanned)] == '(') {
		    nodePtr->lexeme = FUNCTION;
		} else {
		    Tcl_Obj *objPtr = Tcl_NewStringObj(start, scanned);
		    Tcl_IncrRefCount(objPtr);
		    code = Tcl_ConvertToType(NULL, objPtr, &tclBooleanType);
		    Tcl_DecrRefCount(objPtr);
		    if (code == TCL_OK) {
			nodePtr->lexeme = BOOLEAN;
		    } else {
			msg = Tcl_NewObj();
			TclObjPrintf(NULL, msg, "invalid bareword \"%.*s%s\"",
				(scanned < limit) ? scanned : limit - 3, start,
				(scanned < limit) ? "" : "...");
			post = Tcl_NewObj();
			TclObjPrintf(NULL, post,
				"should be \"$%.*s%s\" or \"{%.*s%s}\"",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...");
			TclObjPrintf(NULL, post, " or \"%.*s%s(...)\" or ...",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...");
			continue;
		    }
		}
		break;
	    case PLUS:
	    case MINUS:
		if ((NODE_TYPE & lastNodePtr->lexeme) == LEAF) {
		    nodePtr->lexeme |= BINARY;
		} else {
		    nodePtr->lexeme |= UNARY;
		}
	    }
	}

	/* Add node to parse tree based on category */

	switch (NODE_TYPE & nodePtr->lexeme) {
	case LEAF:

	    if ((NODE_TYPE & lastNodePtr->lexeme) == LEAF) {
		CONST char *operand =
			scratch.tokenPtr[lastNodePtr->token].start;

		msg = Tcl_NewObj();
		TclObjPrintf(NULL, msg, "missing operator at %s", mark);
		if (operand[0] == '0') {
		    Tcl_Obj *copy = Tcl_NewStringObj(operand,
			    start + scanned - operand);
		    if (TclCheckBadOctal(NULL, Tcl_GetString(copy))) {
			post = Tcl_NewStringObj(
				"looks like invalid octal number", -1);
		    }
		    Tcl_DecrRefCount(copy);
		}
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }

	    TclGrowParseTokenArray(&scratch,2);
	    nodePtr->token = scratch.numTokens;
	    tokenPtr = scratch.tokenPtr + nodePtr->token;
	    tokenPtr->type = TCL_TOKEN_SUB_EXPR;
	    tokenPtr->start = start;
	    scratch.numTokens++;

	    switch (nodePtr->lexeme) {
	    case NUMBER:
	    case BOOLEAN:
		tokenPtr = scratch.tokenPtr + scratch.numTokens;
		tokenPtr->type = TCL_TOKEN_TEXT;
		tokenPtr->start = start;
		tokenPtr->size = scanned;
		tokenPtr->numComponents = 0;
		scratch.numTokens++;

		break;

	    case QUOTED:
		code = Tcl_ParseQuotedString(interp, start, numBytes,
			&scratch, 1, &end);
		if (code != TCL_OK) {
		    scanned = scratch.term - start;
		    scanned += (scanned < numBytes);
		    continue;
		}
		scanned = end - start;
		break;

	    case BRACED:
		code = Tcl_ParseBraces(interp, start, numBytes,
			&scratch, 1, &end);
		if (code != TCL_OK) {
		    continue;
		}
		scanned = end - start;
		break;

	    case VARIABLE:
		code = Tcl_ParseVarName(interp, start, numBytes, &scratch, 1);
		if (code != TCL_OK) {
		    scanned = scratch.term - start;
		    scanned += (scanned < numBytes);
		    continue;
		}
		tokenPtr = scratch.tokenPtr + nodePtr->token + 1;
		if (tokenPtr->type != TCL_TOKEN_VARIABLE) {
		    msg = Tcl_NewStringObj("invalid character \"$\"", -1);
		    code = TCL_ERROR;
		    continue;
		}
		scanned = tokenPtr->size;
		break;

	    case SCRIPT:
		if (useInternalTokens) {
		    CONST char *term;
		    Tcl_Token *lastTokenPtr;
		    Tcl_Token *appendTokens = TclParseScript(start+scanned,
			    numBytes-scanned, PARSE_NESTED, &lastTokenPtr,
			    &term);
		    int numTokens = 1 + (int) (lastTokenPtr - appendTokens);

		    TclGrowParseTokenArray(&scratch, numTokens + 1);
		    tokenPtr = scratch.tokenPtr + scratch.numTokens;
		    tokenPtr->type = TCL_TOKEN_SCRIPT_SUBST;
		    tokenPtr->size = term - (start + scanned) + 2;
		    tokenPtr->numComponents = numTokens;

		    memcpy(tokenPtr+1, appendTokens,
			    (size_t) (numTokens * sizeof(Tcl_Token)));
		    scratch.numTokens += numTokens + 1;
		    lastTokenPtr = scratch.tokenPtr + scratch.numTokens - 1;
		    ckfree((char *) appendTokens);

		    if (lastTokenPtr->type == TCL_TOKEN_ERROR) {
			parsePtr->errorType = lastTokenPtr->numComponents;
			parsePtr->term = term;
			parsePtr->incomplete = 1;
			scanned = numBytes;
			if (interp != NULL) {
			    TclSubstTokens(interp, lastTokenPtr, 1, NULL, 0);
			}
			code = TCL_ERROR;
			continue;
		    } else {
			scanned = term + 1 - start;
		    }
		    break;
		}

		tokenPtr = scratch.tokenPtr + scratch.numTokens;
		tokenPtr->type = TCL_TOKEN_COMMAND;
		tokenPtr->start = start;
		tokenPtr->numComponents = 0;

		end = start + numBytes;
		start++;
		while (1) {
		    Tcl_Parse nested;
		    code = Tcl_ParseCommand(interp,
			    start, (end - start), 1, &nested);
		    if (code != TCL_OK) {
			parsePtr->term = nested.term;
			parsePtr->errorType = nested.errorType;
			parsePtr->incomplete = nested.incomplete;
			break;
		    }
		    start = (nested.commandStart + nested.commandSize);
		    Tcl_FreeParse(&nested);
		    if ((nested.term < end) && (*nested.term == ']')
			    && !nested.incomplete) {
			break;
		    }

		    if (start == end) {
			msg = Tcl_NewStringObj("missing close-bracket", -1);
			parsePtr->term = tokenPtr->start;
			parsePtr->errorType = TCL_PARSE_MISSING_BRACKET;
			parsePtr->incomplete = 1;
			code = TCL_ERROR;
			break;
		    }
		}
		end = start;
		start = tokenPtr->start;
		if (code != TCL_OK) {
		    scanned = parsePtr->term - start;
		    scanned += (scanned < numBytes);
		    continue;
		}
		scanned = end - start;
		tokenPtr->size = scanned;
		scratch.numTokens++;
		break;
	    }

	    tokenPtr = scratch.tokenPtr + nodePtr->token;
	    tokenPtr->size = scanned;
	    tokenPtr->numComponents = scratch.numTokens - nodePtr->token - 1;

	    nodePtr->left = -1;
	    nodePtr->right = -1;
	    nodePtr->parent = -1;
	    lastOrphanPtr = nodePtr;
	    nodesUsed++;
	    break;

	case UNARY:
	    if ((NODE_TYPE & lastNodePtr->lexeme) == LEAF) {
		msg = Tcl_NewObj();
		TclObjPrintf(NULL, msg, "missing operator at %s", mark);
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }
	    nodePtr->left = -1;
	    nodePtr->right = -1;
	    nodePtr->parent = -1;

	    TclGrowParseTokenArray(&scratch,1);
	    nodePtr->token = scratch.numTokens;
	    tokenPtr = scratch.tokenPtr + nodePtr->token;
	    tokenPtr->type = TCL_TOKEN_OPERATOR;
	    tokenPtr->start = start;
	    tokenPtr->size = scanned;
	    tokenPtr->numComponents = 0;
	    scratch.numTokens++;

	    lastOrphanPtr = nodePtr;
	    nodesUsed++;
	    break;

	case BINARY: {
	    ExprNode *otherPtr;

	    if ((nodePtr->lexeme == CLOSE_PAREN)
		    && (lastNodePtr->lexeme == OPEN_PAREN)) {
		if (lastNodePtr[-1].lexeme == FUNCTION) {
		    /* Normally, "()" is a syntax error, but as a special
		     * case accept it as an argument list for a function */
		    scanned = 0;
		    nodePtr->lexeme = EMPTY;
		    nodePtr->left = -1;
		    nodePtr->right = -1;
		    nodePtr->parent = -1;
		    nodePtr->token = -1;

		    lastOrphanPtr = nodePtr;
		    nodesUsed++;
		    break;

		}
		msg = Tcl_NewObj();
		TclObjPrintf(NULL, msg, "empty subexpression at %s", mark);
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }

	    precedence = prec[nodePtr->lexeme];

	    if ((NODE_TYPE & lastNodePtr->lexeme) != LEAF) {
		if (prec[lastNodePtr->lexeme] > precedence) {
		    if (lastNodePtr->lexeme == OPEN_PAREN) {
			msg = Tcl_NewStringObj("unbalanced open paren", -1);
		    } else if (lastNodePtr->lexeme == COMMA) {
			msg = Tcl_NewObj();
			TclObjPrintf(NULL, msg,
				"missing function argument at %s", mark);
			scanned = 0;
			insertMark = 1;
		    } else if (lastNodePtr->lexeme == START) {
			msg = Tcl_NewStringObj("empty expression", -1);
		    }
		} else {
		    if (nodePtr->lexeme == CLOSE_PAREN) {
			msg = Tcl_NewStringObj("unbalanced close paren", -1);
		    } else if ((nodePtr->lexeme == COMMA)
			    && (lastNodePtr->lexeme == OPEN_PAREN)
			    && (lastNodePtr[-1].lexeme == FUNCTION)) {
			msg = Tcl_NewObj();
			TclObjPrintf(NULL, msg,
				"missing function argument at %s", mark);
			scanned = 0;
			insertMark = 1;
		    }
		}
		if (msg == NULL) {
		    msg = Tcl_NewObj();
		    TclObjPrintf(NULL, msg, "missing operand at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		}
		code = TCL_ERROR;
		continue;
	    }

	    while (1) {
		otherPtr = lastOrphanPtr;
		while (otherPtr->left >= 0) {
		    otherPtr = nodes + otherPtr->left;
		}
		otherPtr--;

		if (prec[otherPtr->lexeme] < precedence) {
		    break;
		}

		/* Special association rules for the ternary operators */
		if (prec[otherPtr->lexeme] == precedence) {
		    if ((otherPtr->lexeme == QUESTION) 
			    && (lastOrphanPtr->lexeme != COLON)) {
			break;
		    }
		    if ((otherPtr->lexeme == COLON)
			    && (nodePtr->lexeme == QUESTION)) {
			break;
		    }
		}

		/* Some checks before linking */
		if ((otherPtr->lexeme == OPEN_PAREN)
			&& (nodePtr->lexeme != CLOSE_PAREN)) {
		    lastOrphanPtr = otherPtr;
		    msg = Tcl_NewStringObj("unbalanced open paren", -1);
		    code = TCL_ERROR;
		    break;
		}
		if ((otherPtr->lexeme == QUESTION)
			&& (lastOrphanPtr->lexeme != COLON)) {
		    msg = Tcl_NewObj();
		    TclObjPrintf(NULL, msg,
			    "missing operator \":\" at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    code = TCL_ERROR;
		    break;
		}
		if ((lastOrphanPtr->lexeme == COLON)
			&& (otherPtr->lexeme != QUESTION)) {
		    msg = Tcl_NewStringObj(
			    "unexpected operator \":\" without preceding \"?\"",
			    -1);
		    code = TCL_ERROR;
		    break;
		}

		/* Link orphan as right operand of otherPtr */
		otherPtr->right = lastOrphanPtr - nodes;
		lastOrphanPtr->parent = otherPtr - nodes;
		lastOrphanPtr = otherPtr;

		if (otherPtr->lexeme == OPEN_PAREN) {
		    /* CLOSE_PAREN can only close one OPEN_PAREN */
		    tokenPtr = scratch.tokenPtr + otherPtr->token;
		    tokenPtr->size = start + scanned - tokenPtr->start;
		    break;
		}
		if (otherPtr->lexeme == START) {
		    /* Don't backtrack beyond the start */
		    break;
		}
	    }
	    if (code != TCL_OK) {
		continue;
	    }

	    if (nodePtr->lexeme == CLOSE_PAREN) {
		if (otherPtr->lexeme == START) {
		    msg = Tcl_NewStringObj("unbalanced close paren", -1);
		    code = TCL_ERROR;
		    continue;
		}
		/* Create no node for a CLOSE_PAREN lexeme */
		break;
	    }

	    if ((nodePtr->lexeme == COMMA) && ((otherPtr->lexeme != OPEN_PAREN)
		    || (otherPtr[-1].lexeme != FUNCTION))) {
		msg = Tcl_NewStringObj(
			"unexpected \",\" outside function argument list", -1);
		code = TCL_ERROR;
		continue;
	    }

	    if (lastOrphanPtr->lexeme == COLON) {
		msg = Tcl_NewStringObj(
			"unexpected operator \":\" without preceding \"?\"",
			-1);
		code = TCL_ERROR;
		continue;
	    }

	    /* Link orphan as left operand of new node */
	    nodePtr->right = -1;
	    nodePtr->parent = -1;

	    TclGrowParseTokenArray(&scratch,1);
	    nodePtr->token = scratch.numTokens;
	    tokenPtr = scratch.tokenPtr + nodePtr->token;
	    tokenPtr->type = TCL_TOKEN_OPERATOR;
	    tokenPtr->start = start;
	    tokenPtr->size = scanned;
	    tokenPtr->numComponents = 0;
	    scratch.numTokens++;

	    nodePtr->left = lastOrphanPtr - nodes;
	    lastOrphanPtr->parent = nodePtr - nodes;
	    lastOrphanPtr = nodePtr;
	    nodesUsed++;
	    break;
	}
	}

	start += scanned;
	numBytes -= scanned;
    }

    if (code == TCL_OK) {
	/* Shift tokens from scratch space to caller space */
	GenerateTokens(nodes, &scratch, parsePtr);
    } else {
	if (parsePtr->errorType == TCL_PARSE_SUCCESS) {
	    parsePtr->errorType = TCL_PARSE_SYNTAX;
	    parsePtr->term = start;
	}
	if (interp == NULL) {
	    if (msg) {
		Tcl_DecrRefCount(msg);
	    }
	} else {
	    if (msg == NULL) {
		msg = Tcl_GetObjResult(interp);
	    }
	    TclObjPrintf(NULL, msg, "\nin expression \"%s%.*s%.*s%s%s%.*s%s\"",
		    ((start - limit) < scratch.string) ? "" : "...",
		    ((start - limit) < scratch.string)
		    ? (start - scratch.string) : limit - 3,
		    ((start - limit) < scratch.string) 
		    ? scratch.string : start - limit + 3,
		    (scanned < limit) ? scanned : limit - 3, start,
		    (scanned < limit) ? "" : "...",
		    insertMark ? mark : "",
		    (start + scanned + limit > scratch.end)
		    ? scratch.end - (start + scanned) : limit-3, 
		    start + scanned,
		    (start + scanned + limit > scratch.end) ? "" : "..."
		    );
	    if (post != NULL) {
		Tcl_AppendToObj(msg, ";\n", -1);
		Tcl_AppendObjToObj(msg, post);
		Tcl_DecrRefCount(post);
	    }
	    Tcl_SetObjResult(interp, msg);
	    numBytes = scratch.end - scratch.string;
	    TclFormatToErrorInfo(interp,
		    "\n    (parsing expression \"%.*s%s\")",
		    (numBytes < limit) ? numBytes : limit - 3,
		    scratch.string, (numBytes < limit) ? "" : "...");
	}
    }

    if (nodes != staticNodes) {
	ckfree((char *)nodes);
    }
    Tcl_FreeParse(&scratch);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateTokens --
 *
 * 	Routine that generates Tcl_Tokens that represent a Tcl expression
 * 	and writes them to *parsePtr.  The parse tree of the expression
 * 	is in the array of ExprNodes, nodes.  Some of the Tcl_Tokens are
 *	copied from scratch space at *scratchPtr, where the parsing pass
 *	that constructed the parse tree left them.
 *
 *----------------------------------------------------------------------
 */

static void
GenerateTokens(
    ExprNode *nodes,
    Tcl_Parse *scratchPtr,
    Tcl_Parse *parsePtr)
{
    ExprNode *nodePtr = nodes + nodes->right;
    Tcl_Token *sourcePtr, *destPtr, *tokenPtr = scratchPtr->tokenPtr;
    int toCopy;
    CONST char *end = tokenPtr->start + tokenPtr->size;

    while (nodePtr->lexeme != START) {
	switch (NODE_TYPE & nodePtr->lexeme) {
	case BINARY:
	    if (nodePtr->left >= 0) {
		if ((nodePtr->lexeme != COMMA) && (nodePtr->lexeme != COLON)) {
		    sourcePtr = scratchPtr->tokenPtr + nodePtr->token;
		    TclGrowParseTokenArray(parsePtr,2);
		    destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
		    nodePtr->token = parsePtr->numTokens;
		    destPtr->type = TCL_TOKEN_SUB_EXPR;
		    destPtr->start = tokenPtr->start;
		    destPtr++;
		    *destPtr = *sourcePtr;
		    parsePtr->numTokens += 2;
		}
		nodePtr = nodes + nodePtr->left;
		nodes[nodePtr->parent].left = -1;
	    } else if (nodePtr->right >= 0) {
		tokenPtr += tokenPtr->numComponents + 1;
		nodePtr = nodes + nodePtr->right;
		nodes[nodePtr->parent].right = -1;
	    } else {
		if ((nodePtr->lexeme != COMMA) && (nodePtr->lexeme != COLON)) {
		    destPtr = parsePtr->tokenPtr + nodePtr->token;
		    destPtr->size = end - destPtr->start;
		    destPtr->numComponents =
			    parsePtr->numTokens - nodePtr->token - 1;
		}
		nodePtr = nodes + nodePtr->parent;
	    }
	    break;

	case UNARY:
	    if (nodePtr->right >= 0) {
		sourcePtr = scratchPtr->tokenPtr + nodePtr->token;
		if (nodePtr->lexeme != OPEN_PAREN) {
		    TclGrowParseTokenArray(parsePtr,2);
		    destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
		    nodePtr->token = parsePtr->numTokens;
		    destPtr->type = TCL_TOKEN_SUB_EXPR;
		    destPtr->start = tokenPtr->start;
		    destPtr++;
		    *destPtr = *sourcePtr;
		    parsePtr->numTokens += 2;
		}
		if (tokenPtr == sourcePtr) {
		    tokenPtr += tokenPtr->numComponents + 1;
		}
		nodePtr = nodes + nodePtr->right;
		nodes[nodePtr->parent].right = -1;
	    } else {
		if (nodePtr->lexeme != OPEN_PAREN) {
		    destPtr = parsePtr->tokenPtr + nodePtr->token;
		    destPtr->size = end - destPtr->start;
		    destPtr->numComponents =
			    parsePtr->numTokens - nodePtr->token - 1;
		} else {
		    sourcePtr = scratchPtr->tokenPtr + nodePtr->token;
		    end = sourcePtr->start + sourcePtr->size;
		}
		nodePtr = nodes + nodePtr->parent;
	    }
	    break;

	case LEAF:
	    switch (nodePtr->lexeme) {
	    case EMPTY:
		break;

	    case BRACED:
	    case QUOTED:
		sourcePtr = scratchPtr->tokenPtr + nodePtr->token;
		end = sourcePtr->start + sourcePtr->size;
		if (sourcePtr->numComponents > 1) {
		    toCopy = sourcePtr->numComponents;
		    if (tokenPtr == sourcePtr) {
			tokenPtr += toCopy + 1;
		    }
		    sourcePtr->numComponents++;
		    TclGrowParseTokenArray(parsePtr,toCopy+2);
		    destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
		    *destPtr++ = *sourcePtr;
		    *destPtr = *sourcePtr++;
		    destPtr->type = TCL_TOKEN_WORD;
		    destPtr->numComponents = toCopy;
		    destPtr++;
		    memcpy((VOID *) destPtr, (VOID *) sourcePtr,
			    (size_t) (toCopy * sizeof(Tcl_Token)));
		    parsePtr->numTokens += toCopy + 2;
		    break;
		}

	    default:
		sourcePtr = scratchPtr->tokenPtr + nodePtr->token;
		end = sourcePtr->start + sourcePtr->size;
		toCopy = sourcePtr->numComponents + 1;
		if (tokenPtr == sourcePtr) {
		    tokenPtr += toCopy;
		}
		TclGrowParseTokenArray(parsePtr,toCopy);
		destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
		memcpy((VOID *) destPtr, (VOID *) sourcePtr,
			(size_t) (toCopy * sizeof(Tcl_Token)));
		parsePtr->numTokens += toCopy;
		break;

	    }
	    nodePtr = nodes + nodePtr->parent;
	    break;

	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ParseLexeme --
 *
 *	Parse a single lexeme from the start of a string, scanning no
 *	more than numBytes bytes.  
 *
 * Results:
 *	Returns the number of bytes scanned to produce the lexeme.
 *
 * Side effects:
 *	Code identifying lexeme parsed is writen to *lexemePtr.
 *
 *----------------------------------------------------------------------
 */

static int
ParseLexeme(
    CONST char *start,		/* Start of lexeme to parse. */
    int numBytes,		/* Number of bytes in string. */
    unsigned char *lexemePtr)	/* Write code of parsed lexeme to this
				 * storage. */
{
    CONST char *end;
    int scanned;
    Tcl_UniChar ch;

    if (numBytes == 0) {
	*lexemePtr = END;
	return 0;
    }
    switch (*start) {
    case '[':
	*lexemePtr = SCRIPT;
	return 1;

    case '{':
	*lexemePtr = BRACED;
	return 1;

    case '(':
	*lexemePtr = OPEN_PAREN;
	return 1;

    case ')':
	*lexemePtr = CLOSE_PAREN;
	return 1;

    case '$':
	*lexemePtr = VARIABLE;
	return 1;

    case '\"':
	*lexemePtr = QUOTED;
	return 1;

    case ',':
	*lexemePtr = COMMA;
	return 1;

    case '/':
	*lexemePtr = DIVIDE;
	return 1;

    case '%':
	*lexemePtr = MOD;
	return 1;

    case '+':
	*lexemePtr = PLUS;
	return 1;

    case '-':
	*lexemePtr = MINUS;
	return 1;

    case '?':
	*lexemePtr = QUESTION;
	return 1;

    case ':':
	*lexemePtr = COLON;
	return 1;

    case '^':
	*lexemePtr = BIT_XOR;
	return 1;

    case '~':
	*lexemePtr = BIT_NOT;
	return 1;

    case '*':
	if ((numBytes > 1) && (start[1] == '*')) {
	    *lexemePtr = EXPON;
	    return 2;
	}
	*lexemePtr = MULT;
	return 1;

    case '=':
	if ((numBytes > 1) && (start[1] == '=')) {
	    *lexemePtr = EQUAL;
	    return 2;
	}
	*lexemePtr = INCOMPLETE;
	return 1;

    case '!':
	if ((numBytes > 1) && (start[1] == '=')) {
	    *lexemePtr = NEQ;
	    return 2;
	}
	*lexemePtr = NOT;
	return 1;

    case '&':
	if ((numBytes > 1) && (start[1] == '&')) {
	    *lexemePtr = AND;
	    return 2;
	}
	*lexemePtr = BIT_AND;
	return 1;

    case '|':
	if ((numBytes > 1) && (start[1] == '|')) {
	    *lexemePtr = OR;
	    return 2;
	}
	*lexemePtr = BIT_OR;
	return 1;

    case '<':
	if (numBytes > 1) {
	    switch (start[1]) {
	    case '<':
		*lexemePtr = LEFT_SHIFT;
		return 2;
	    case '=':
		*lexemePtr = LEQ;
		return 2;
	    }
	}
	*lexemePtr = LESS;
	return 1;

    case '>':
	if (numBytes > 1) {
	    switch (start[1]) {
	    case '>':
		*lexemePtr = RIGHT_SHIFT;
		return 2;
	    case '=':
		*lexemePtr = GEQ;
		return 2;
	    }
	}
	*lexemePtr = GREATER;
	return 1;

    case 'i':
	if ((numBytes > 1) && (start[1] == 'n')
		&& ((numBytes == 2) || !isalpha(UCHAR(start[2])))) {
	    /*
	     * Must make this check so we can tell the difference between
	     * the "in" operator and the "int" function name and the
	     * "infinity" numeric value.
	     */
	    *lexemePtr = IN_LIST;
	    return 2;
	}
	break;

    case 'e':
	if ((numBytes > 1) && (start[1] == 'q')
		&& ((numBytes == 2) || !isalpha(UCHAR(start[2])))) {
	    *lexemePtr = STREQ;
	    return 2;
	}
	break;

    case 'n':
	if ((numBytes > 1) && ((numBytes == 2) || !isalpha(UCHAR(start[2])))) {
	    switch (start[1]) {
	    case 'e':
		*lexemePtr = STRNEQ;
		return 2;
	    case 'i':
		*lexemePtr = NOT_IN_LIST;
		return 2;
	    }
	}
    }

    if (TclParseNumber(NULL, NULL, NULL, start, numBytes, &end,
	    TCL_PARSE_NO_WHITESPACE) == TCL_OK) {
	*lexemePtr = NUMBER;
	return (end-start);
    }

    if (Tcl_UtfCharComplete(start, numBytes)) {
	scanned = Tcl_UtfToUniChar(start, &ch);
    } else {
	char utfBytes[TCL_UTF_MAX];
	memcpy(utfBytes, start, (size_t) numBytes);
	utfBytes[numBytes] = '\0';
	scanned = Tcl_UtfToUniChar(utfBytes, &ch);
    }
    if (!isalpha(UCHAR(ch))) {
	*lexemePtr = INVALID;
	return scanned;
    }
    end = start;
    while (isalnum(UCHAR(ch)) || (UCHAR(ch) == '_')) {
	end += scanned;
	numBytes -= scanned;
	if (Tcl_UtfCharComplete(end, numBytes)) {
	    scanned = Tcl_UtfToUniChar(end, &ch);
	} else {
	    char utfBytes[TCL_UTF_MAX];
	    memcpy(utfBytes, end, (size_t) numBytes);
	    utfBytes[numBytes] = '\0';
	    scanned = Tcl_UtfToUniChar(utfBytes, &ch);
	}
    }
    *lexemePtr = BAREWORD;
    return (end-start);
}
#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
