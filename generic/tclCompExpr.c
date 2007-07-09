/*
 * tclCompExpr.c --
 *
 *	This file contains the code to compile Tcl expressions.
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 * Contributions from Don Porter, NIST, 2006.  (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclCompExpr.c,v 1.14.2.26 2007/07/09 13:00:42 dgp Exp $
 */

#include "tclInt.h"
#include "tclCompile.h"		/* CompileEnv */

/*
 * Set of lexeme codes returned by ParseLexeme().
 *
 * First, each lexeme belongs to one of four categories, which determine
 * its place in the parse tree.  We use the two high bits of the
 * (unsigned char) value to store a NODE_TYPE code.
 */

#define NODE_TYPE	0xC0

/*
 * The four category values are LEAF, UNARY, and BINARY, explained below,
 * and "uncategorized", which is used either temporarily, until context
 * determines which of the other three categories is correct, or for
 * lexemes like INVALID, which aren't really lexemes at all, but indicators
 * of a parsing error.  Note that the codes must be distinct to distinguish
 * categories, but need not take the form of a bit array.
 */

#define BINARY		0x40	/* This lexeme is a binary operator.  An
				 * OpNode representing it should go into the
				 * parse tree, and two operands should be
				 * parsed for it in the expression.  */
#define UNARY		0x80	/* This lexeme is a unary operator.  An OpNode
				 * representing it should go into the parse
				 * tree, and one operand should be parsed for
				 * it in the expression. */
#define LEAF		0xC0	/* This lexeme is a leaf operand in the parse
				 * tree.  No OpNode will be placed in the tree
				 * for it.  Either a literal value will be
				 * appended to the list of literals in this
				 * expression, or appropriate Tcl_Tokens will
				 * be appended in a Tcl_Parse struct to 
				 * represent those leaves that require some
				 * form of substitution.
				 */

/* Uncategorized lexemes */

#define PLUS		1	/* Ambiguous.  Resolves to UNARY_PLUS or
				 * BINARY_PLUS according to context. */
#define MINUS		2	/* Ambiguous.  Resolves to UNARY_MINUS or
				 * BINARY_MINUS according to context. */
#define BAREWORD	3	/* Ambigous.  Resolves to BOOLEAN or to
				 * FUNCTION or a parse error according to
				 * context and value. */
#define INCOMPLETE	4	/* A parse error.  Used only when the single
				 * "=" is encountered.  */
#define INVALID		5	/* A parse error.  Used when any punctuation
				 * appears that's not a supported operator. */

/* Leaf lexemes */

#define NUMBER		( LEAF | 1)	/* For literal numbers */
#define SCRIPT		( LEAF | 2)	/* Command substitution; [foo] */
#define BOOLEAN		( LEAF | BAREWORD)	/* For literal booleans */
#define BRACED		( LEAF | 4)	/* Braced string; {foo bar} */
#define VARIABLE	( LEAF | 5)	/* Variable substitution; $x */
#define QUOTED		( LEAF | 6)	/* Quoted string; "foo $bar [soom]" */
#define EMPTY		( LEAF | 7)	/* Used only for an empty argument
					 * list to a function.  Represents
					 * the empty string within parens in
					 * the expression: rand() */

/* Unary operator lexemes */

#define UNARY_PLUS	( UNARY | PLUS)
#define UNARY_MINUS	( UNARY | MINUS)
#define FUNCTION	( UNARY | BAREWORD)	/* This is a bit of "creative
					 * interpretation" on the part of the
					 * parser.  A function call is parsed
					 * into the parse tree according to
					 * the perspective that the function
					 * name is a unary operator and its
					 * argument list, enclosed in parens,
					 * is its operand.  The additional
					 * requirements not implied generally
					 * by treatment as a unary operator --
					 * for example, the requirement that
					 * the operand be enclosed in parens --
					 * are hard coded in the relevant
					 * portions of ParseExpr().  We trade
					 * off the need to include such
					 * exceptional handling in the code
					 * against the need we would otherwise
					 * have for more lexeme categories. */
#define START		( UNARY | 4)	/* This lexeme isn't parsed from the
					 * expression text at all.  It
					 * represents the start of the
					 * expression and sits at the root of
					 * the parse tree where it serves as
					 * the start/end point of traversals. */
#define OPEN_PAREN	( UNARY | 5)	/* Another bit of creative
					 * interpretation, where we treat "("
					 * as a unary operator with the
					 * sub-expression between it and its
					 * matching ")" as its operand. See
					 * CLOSE_PAREN below. */
#define NOT		( UNARY | 6)
#define BIT_NOT		( UNARY | 7)

/* Binary operator lexemes */

#define BINARY_PLUS	( BINARY |  PLUS)
#define BINARY_MINUS	( BINARY |  MINUS)
#define COMMA		( BINARY |  3)	/* The "," operator is a low precedence
					 * binary operator that separates the
					 * arguments in a function call.  The
					 * additional constraint that this
					 * operator can only legally appear
					 * at the right places within a
					 * function call argument list are
					 * hard coded within ParseExpr().  */
#define MULT		( BINARY |  4)
#define DIVIDE		( BINARY |  5)
#define MOD		( BINARY |  6)
#define LESS		( BINARY |  7)
#define GREATER		( BINARY |  8)
#define BIT_AND		( BINARY |  9)
#define BIT_XOR		( BINARY | 10)
#define BIT_OR		( BINARY | 11)
#define QUESTION	( BINARY | 12)	/* These two lexemes make up the */
#define COLON		( BINARY | 13)	/* ternary conditional operator,
					 * $x ? $y : $z .  We treat them as
					 * two binary operators to avoid
					 * another lexeme category, and
					 * code the additional constraints
					 * directly in ParseExpr().  For
					 * instance, the right operand of
					 * a "?" operator must be a ":"
					 * operator. */
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
#define EXPON		( BINARY | 24)	/* Unlike the other binary operators,
					 * EXPON is right associative and this
					 * distinction is coded directly in
					 * ParseExpr(). */
#define IN_LIST		( BINARY | 25)
#define NOT_IN_LIST	( BINARY | 26)
#define CLOSE_PAREN	( BINARY | 27)	/**/
#define END		( BINARY | 28)	/**/

/*
 * Integer codes indicating the form of an operand of an operator.
 */

enum OperandTypes {
    OT_NONE = -4, OT_LITERAL = -3, OT_TOKENS = -2, OT_EMPTY = -1
};

/*
 * The OpNode structure represents one operator node in the parse tree
 * produced as an interim structure by the expression parser.
 */

typedef struct OpNode {
    unsigned char lexeme;	/* Code that identifies the operator. */
    int left;			/* Index of the left operand. Non-negative
				 * integer is an index into the parse tree,
				 * pointing to another operator. Value
				 * OT_LITERAL indicates operand is the next
				 * entry in the literal list. Value OT_TOKENS
				 * indicates the operand is the next word in
				 * the Tcl_Parse struct. Value OT_NONE
				 * indicates we haven't yet parsed the operand
				 * for this operator. */
    int right;			/* Index of the right operand. Same
				 * interpretation as left, with addition of
				 * OT_EMPTY meaning zero arguments. */
    int parent;			/* Index of the operator of this operand
				 * node. */
} OpNode;

typedef struct JumpList {
    JumpFixup jump;
    int depth;
    int offset;
    int convert;
    struct JumpList *next;
} JumpList;

/*
 * Declarations for local functions to this file:
 */

static int		ParseLexeme(const char *start, int numBytes,
			    unsigned char *lexemePtr, Tcl_Obj **literalPtr);
static int		ParseExpr(Tcl_Interp *interp, const char *start,
			    int numBytes, OpNode **opTreePtr,
			    Tcl_Obj *litList, Tcl_Obj *funcList,
			    Tcl_Parse *parsePtr);
static void		ConvertTreeToTokens(Tcl_Interp *interp,
			    const char *start, int numBytes, OpNode *nodes,
			    Tcl_Obj *litList, Tcl_Token *tokenPtr,
			    Tcl_Parse *parsePtr);
static int		GenerateTokensForLiteral(const char *script,
			    int numBytes, Tcl_Obj *litList, int nextLiteral,
			    Tcl_Parse *parsePtr);
static int		CopyTokens(Tcl_Token *sourcePtr, Tcl_Parse *parsePtr);
static void		CompileExprTree(Tcl_Interp *interp, OpNode *nodes,
				Tcl_Obj *const litObjv[], Tcl_Obj *funcList,
				Tcl_Token *tokenPtr, int *convertPtr,
				CompileEnv *envPtr);


/*
 *----------------------------------------------------------------------
 *
 * ParseExpr --
 *
 *	Given a string, the numBytes bytes starting at start, this function
 *	parses it as a Tcl expression and stores information about the
 *	structure of the expression in the Tcl_Parse struct indicated by the
 *	caller.
 *
 * Results:
 *	If the string is successfully parsed as a valid Tcl expression, TCL_OK
 *	is returned, and data about the expression structure is written to
 *	*parsePtr. If the string cannot be parsed as a valid Tcl expression,
 *	TCL_ERROR is returned, and if interp is non-NULL, an error message is
 *	written to interp.
 *
 * Side effects:
 *	If there is insufficient space in parsePtr to hold all the information
 *	about the expression, then additional space is malloc-ed. If the
 *	function returns TCL_OK then the caller must eventually invoke
 *	Tcl_FreeParse to release any additional space that was allocated.
 *
 *----------------------------------------------------------------------
 */

static int
ParseExpr(
    Tcl_Interp *interp,		/* Used for error reporting. */
    const char *start,		/* Start of source string to parse. */
    int numBytes,		/* Number of bytes in string. If < 0, the
				 * string consists of all bytes up to the
				 * first null character. */
    OpNode **opTreePtr,		/* Points to space where a pointer to the
				 * allocated OpNode tree should go. */
    Tcl_Obj *litList,		/* List to append literals to. */
    Tcl_Obj *funcList,		/* List to append function names to. */
    Tcl_Parse *parsePtr)	/* Structure to fill with tokens representing
				 * those operands that require run time
				 * substitutions. */
{
    OpNode *nodes = NULL;
    int nodesAvailable = 64, nodesUsed = 0;
    int code = TCL_OK;
    int numLiterals = 0, numFuncs = 0;
    int scanned = 0, insertMark = 0;
    int lastOpen = 0, lastWas = 0;
    unsigned char lexeme = START;
    Tcl_Obj *msg = NULL, *post = NULL;
    const int limit = 25;
    const char *mark = "_@_";
    static const unsigned char prec[] = {
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  15,	15, 5,	16, 16,	16, 13,	13, 11,	10, 9,	6,  6,	14, 14,
	13, 13, 12, 12,	8,  7,	12, 12,	17, 12,	12, 3,	1,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  18,	18, 18,	2,  4,	18, 18,	0,  0,	0,  0,	0,  0,	0,  0,
    };

    if (numBytes < 0) {
	numBytes = (start ? strlen(start) : 0);
    }

    TclParseInit(interp, start, numBytes, parsePtr);

    nodes = (OpNode *) attemptckalloc(nodesAvailable * sizeof(OpNode));
    if (nodes == NULL) {
	TclNewLiteralStringObj(msg, "not enough memory to parse expression");
	code = TCL_ERROR;
    } else {
	/*
	 * Initialize the parse tree with the special "START" node.
	 */

	nodes->lexeme = lexeme;
	nodes->left = OT_NONE;
	nodes->right = OT_NONE;
	nodes->parent = -1;
	nodesUsed++;
    }

    while ((code == TCL_OK) && (lexeme != END)) {
	OpNode *nodePtr;
	Tcl_Token *tokenPtr = NULL;
	Tcl_Obj *literal = NULL;
	const char *lastStart = start - scanned;

	/*
	 * Each pass through this loop adds one more OpNode. Allocate space
	 * for one if required.
	 */

	if (nodesUsed >= nodesAvailable) {
	    int size = nodesUsed * 2;
	    OpNode *newPtr;

	    do {
		newPtr = (OpNode *) attemptckrealloc((char *) nodes,
			(unsigned int) size * sizeof(OpNode));
	    } while ((newPtr == NULL)
		    && ((size -= (size - nodesUsed) / 2) > nodesUsed));
	    if (newPtr == NULL) {
		TclNewLiteralStringObj(msg,
			"not enough memory to parse expression");
		code = TCL_ERROR;
		continue;
	    }
	    nodesAvailable = size;
	    nodes = newPtr;
	}
	nodePtr = nodes + nodesUsed;

	/*
	 * Skip white space between lexemes.
	 */

	scanned = TclParseAllWhiteSpace(start, numBytes);
	start += scanned;
	numBytes -= scanned;

	scanned = ParseLexeme(start, numBytes, &lexeme, &literal);

	/*
	 * Use context to categorize the lexemes that are ambiguous.
	 */

	if ((NODE_TYPE & lexeme) == 0) {
	    switch (lexeme) {
	    case INVALID:
		msg = Tcl_ObjPrintf(
			"invalid character \"%.*s\"", scanned, start);
		code = TCL_ERROR;
		continue;
	    case INCOMPLETE:
		msg = Tcl_ObjPrintf(
			"incomplete operator \"%.*s\"", scanned, start);
		code = TCL_ERROR;
		continue;
	    case BAREWORD:
		if (start[scanned+TclParseAllWhiteSpace(
			start+scanned, numBytes-scanned)] == '(') {
		    lexeme = FUNCTION;
		    Tcl_ListObjAppendElement(NULL, funcList, literal);
		    numFuncs++;
		} else {
		    int b;
		    if (Tcl_GetBooleanFromObj(NULL, literal, &b) == TCL_OK) {
			lexeme = BOOLEAN;
		    } else {
			Tcl_DecrRefCount(literal);
			msg = Tcl_ObjPrintf(
				"invalid bareword \"%.*s%s\"",
				(scanned < limit) ? scanned : limit - 3, start,
				(scanned < limit) ? "" : "...");
			post = Tcl_ObjPrintf(
				"should be \"$%.*s%s\" or \"{%.*s%s}\"",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...");
			Tcl_AppendPrintfToObj(post,
				" or \"%.*s%s(...)\" or ...",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...");
			code = TCL_ERROR;
			continue;
		    }
		}
		break;
	    case PLUS:
	    case MINUS:
		if (lastWas < 0) {
		    lexeme |= BINARY;
		} else {
		    lexeme |= UNARY;
		}
	    }
	}

	/*
	 * Add node to parse tree based on category.
	 */

	switch (NODE_TYPE & lexeme) {
	case LEAF: {
	    const char *end;
	    int wordIndex;

	    /*
	     * Store away any literals on the list now, so they'll
	     * be available for our caller to free if we error out
	     * of this routine.  [Bug 1705778, leak K23]
	     */

	    switch (lexeme) {
	    case NUMBER:
	    case BOOLEAN:
		Tcl_ListObjAppendElement(NULL, litList, literal);
		numLiterals++;
		break;
	    default:
		break;
	    }

	    if (lastWas < 0) {
		msg = Tcl_ObjPrintf("missing operator at %s", mark);
		if (lastStart[0] == '0') {
		    Tcl_Obj *copy = Tcl_NewStringObj(lastStart,
			    start + scanned - lastStart);
		    if (TclCheckBadOctal(NULL, Tcl_GetString(copy))) {
			TclNewLiteralStringObj(post,
				"looks like invalid octal number");
		    }
		    Tcl_DecrRefCount(copy);
		}
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }

	    switch (lexeme) {
	    case NUMBER:
	    case BOOLEAN:
		lastWas = OT_LITERAL;
		start += scanned;
		numBytes -= scanned;
		continue;
	    default:
		break;
	    }

	    /*
	     * Make room for at least 2 more tokens.
	     */

	    TclGrowParseTokenArray(parsePtr, 2);
	    wordIndex = parsePtr->numTokens;
	    tokenPtr = parsePtr->tokenPtr + wordIndex;
	    tokenPtr->type = TCL_TOKEN_WORD;
	    tokenPtr->start = start;
	    parsePtr->numTokens++;

	    switch (lexeme) {
	    case QUOTED:
		code = Tcl_ParseQuotedString(interp, start, numBytes,
			parsePtr, 1, &end);
		if (code != TCL_OK) {
		    scanned = parsePtr->term - start;
		    scanned += (scanned < numBytes);
		    continue;
		}
		scanned = end - start;
		break;

	    case BRACED:
		code = Tcl_ParseBraces(interp, start, numBytes,
			    parsePtr, 1, &end);
		if (code != TCL_OK) {
		    continue;
		}
		scanned = end - start;
		break;

	    case VARIABLE:
		code = Tcl_ParseVarName(interp, start, numBytes, parsePtr, 1);
		if (code != TCL_OK) {
		    scanned = parsePtr->term - start;
		    scanned += (scanned < numBytes);
		    continue;
		}
		tokenPtr = parsePtr->tokenPtr + wordIndex + 1;
		if (tokenPtr->type != TCL_TOKEN_VARIABLE) {
		    TclNewLiteralStringObj(msg, "invalid character \"$\"");
		    code = TCL_ERROR;
		    continue;
		}
		scanned = tokenPtr->size;
		break;

	    case SCRIPT: {
		Tcl_Parse *nestedPtr =
			(Tcl_Parse *) TclStackAlloc(interp, sizeof(Tcl_Parse));

		tokenPtr = parsePtr->tokenPtr + parsePtr->numTokens;
		tokenPtr->type = TCL_TOKEN_COMMAND;
		tokenPtr->start = start;
		tokenPtr->numComponents = 0;

		end = start + numBytes;
		start++;
		while (1) {
		    code = Tcl_ParseCommand(interp, start, (end - start), 1,
			    nestedPtr);
		    if (code != TCL_OK) {
			parsePtr->term = nestedPtr->term;
			parsePtr->errorType = nestedPtr->errorType;
			parsePtr->incomplete = nestedPtr->incomplete;
			break;
		    }
		    start = (nestedPtr->commandStart + nestedPtr->commandSize);
		    Tcl_FreeParse(nestedPtr);
		    if ((nestedPtr->term < end) && (*(nestedPtr->term) == ']')
			    && !(nestedPtr->incomplete)) {
			break;
		    }

		    if (start == end) {
			TclNewLiteralStringObj(msg, "missing close-bracket");
			parsePtr->term = tokenPtr->start;
			parsePtr->errorType = TCL_PARSE_MISSING_BRACKET;
			parsePtr->incomplete = 1;
			code = TCL_ERROR;
			break;
		    }
		}
		TclStackFree(interp, nestedPtr);
		end = start;
		start = tokenPtr->start;
		if (code != TCL_OK) {
		    scanned = parsePtr->term - start;
		    scanned += (scanned < numBytes);
		    continue;
		}
		scanned = end - start;
		tokenPtr->size = scanned;
		parsePtr->numTokens++;
		break;
	    }
	    }

	    tokenPtr = parsePtr->tokenPtr + wordIndex;
	    tokenPtr->size = scanned;
	    tokenPtr->numComponents = parsePtr->numTokens - wordIndex - 1;
	    if ((lexeme == QUOTED) || (lexeme == BRACED)) {
		literal = Tcl_NewObj();
		/* TODO: allow all compile-time known words */
		if (tokenPtr->numComponents == 1
			&& tokenPtr[1].type == TCL_TOKEN_TEXT
			&& TclWordKnownAtCompileTime(tokenPtr, literal)) {
		    Tcl_ListObjAppendElement(NULL, litList, literal);
		    numLiterals++;
		    lastWas = OT_LITERAL;
		    parsePtr->numTokens = wordIndex;
		    break;
		}
		Tcl_DecrRefCount(literal);
	    }
	    lastWas = OT_TOKENS;
	    break;
	}

	case UNARY:
	    if (lastWas < 0) {
		msg = Tcl_ObjPrintf("missing operator at %s", mark);
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }
	    lastWas = nodesUsed;
	    nodePtr->lexeme = lexeme;
	    nodePtr->left = OT_NONE;
	    nodePtr->right = OT_NONE;
	    nodePtr->parent = nodePtr - nodes - 1;
	    nodesUsed++;
	    break;

	case BINARY: {
	    OpNode *otherPtr = NULL;
	    unsigned char precedence = prec[lexeme];

	    if (lastWas >= 0) {
		if ((lexeme == CLOSE_PAREN)
			&& (nodePtr[-1].lexeme == OPEN_PAREN)) {
		    if (nodePtr[-2].lexeme == FUNCTION) {
			/*
			 * Normally, "()" is a syntax error, but as a special
			 * case accept it as an argument list for a function.
			 */

			scanned = 0;
			lastWas = OT_EMPTY;
			nodePtr[-1].left--;
			break;
		    }
		    msg = Tcl_ObjPrintf("empty subexpression at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    code = TCL_ERROR;
		    continue;
		}

		if (prec[nodePtr[-1].lexeme] > precedence) {
		    if (nodePtr[-1].lexeme == OPEN_PAREN) {
			TclNewLiteralStringObj(msg, "unbalanced open paren");
		    } else if (nodePtr[-1].lexeme == COMMA) {
			msg = Tcl_ObjPrintf(
				"missing function argument at %s", mark);
			scanned = 0;
			insertMark = 1;
		    } else if (nodePtr[-1].lexeme == START) {
			TclNewLiteralStringObj(msg, "empty expression");
		    }
		} else {
		    if (lexeme == CLOSE_PAREN) {
			TclNewLiteralStringObj(msg, "unbalanced close paren");
		    } else if ((lexeme == COMMA)
			    && (nodePtr[-1].lexeme == OPEN_PAREN)
			    && (nodePtr[-2].lexeme == FUNCTION)) {
			msg = Tcl_ObjPrintf(
				"missing function argument at %s", mark);
			scanned = 0;
			insertMark = 1;
		    }
		}
		if (msg == NULL) {
		    msg = Tcl_ObjPrintf("missing operand at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		}
		code = TCL_ERROR;
		continue;
	    }

	    if (lastWas == OT_NONE) {
		otherPtr = nodes + lastOpen - 1;
		lastWas = lastOpen;
	    } else {
		otherPtr = nodePtr - 1;
	    }
	    while (1) {
		/*
		 * lastWas is "index" of item to be linked. otherPtr points to
		 * competing operator.
		 */

		if (prec[otherPtr->lexeme] < precedence) {
		    break;
		}

		if (prec[otherPtr->lexeme] == precedence) {
		    /*
		     * Right association rules for exponentiation.
		     */

		    if (lexeme == EXPON) {
			break;
		    }

		    /*
		     * Special association rules for the ternary operators.
		     * The "?" and ":" operators have equal precedence, but
		     * must be linked up in sensible pairs.
		     */

		    if ((otherPtr->lexeme == QUESTION) && ((lastWas < 0)
			    || (nodes[lastWas].lexeme != COLON))) {
			break;
		    }
		    if ((otherPtr->lexeme == COLON) && (lexeme == QUESTION)) {
			break;
		    }
		}

		/*
		 * We should link the lastWas item to the otherPtr as its
		 * right operand. First make some syntax checks.
		 */

		if ((otherPtr->lexeme == OPEN_PAREN)
			&& (lexeme != CLOSE_PAREN)) {
		    TclNewLiteralStringObj(msg, "unbalanced open paren");
		    code = TCL_ERROR;
		    break;
		}
		if ((otherPtr->lexeme == QUESTION) && ((lastWas < 0)
			|| (nodes[lastWas].lexeme != COLON))) {
		    msg = Tcl_ObjPrintf(
			    "missing operator \":\" at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    code = TCL_ERROR;
		    break;
		}
		if ((lastWas >= 0) && (nodes[lastWas].lexeme == COLON)
			&& (otherPtr->lexeme != QUESTION)) {
		    TclNewLiteralStringObj(msg,
			    "unexpected operator \":\" without preceding \"?\"");
		    code = TCL_ERROR;
		    break;
		}

		/*
		 * Link orphan as right operand of otherPtr.
		 */

		otherPtr->right = lastWas;
		if (lastWas >= 0) {
		    nodes[lastWas].parent = otherPtr - nodes;
		}
		lastWas = otherPtr - nodes;

		if (otherPtr->lexeme == OPEN_PAREN) {
		    /*
		     * CLOSE_PAREN can only close one OPEN_PAREN.
		     */

		    break;
		}
		if (otherPtr->lexeme == START) {
		    /*
		     * Don't backtrack beyond the start.
		     */

		    break;
		}
		otherPtr = nodes + otherPtr->parent;
	    }
	    if (code != TCL_OK) {
		continue;
	    }

	    if (lexeme == CLOSE_PAREN) {
		if (otherPtr->lexeme == START) {
		    TclNewLiteralStringObj(msg, "unbalanced close paren");
		    code = TCL_ERROR;
		    continue;
		}
		lastWas = OT_NONE;
		lastOpen = otherPtr - nodes;
		otherPtr->left++;

		/*
		 * Create no node for a CLOSE_PAREN lexeme.
		 */

		break;
	    }
	    if (lexeme == COMMA) {
		if  ((otherPtr->lexeme != OPEN_PAREN)
			|| (otherPtr[-1].lexeme != FUNCTION)) {
		    TclNewLiteralStringObj(msg,
			    "unexpected \",\" outside function argument list");
		    code = TCL_ERROR;
		    continue;
		}
		otherPtr->left++;
	    }
	    if ((lastWas >= 0) && (nodes[lastWas].lexeme == COLON)) {
		TclNewLiteralStringObj(msg,
			"unexpected operator \":\" without preceding \"?\"");
		code = TCL_ERROR;
		continue;
	    }

	    /*
	     * Link orphan as left operand of new node.
	     */

	    nodePtr->lexeme = lexeme;
	    nodePtr->right = -1;
	    nodePtr->left = lastWas;
	    if (lastWas < 0) {
		nodePtr->parent = nodePtr - nodes - 1;
	    } else {
		nodePtr->parent = nodes[lastWas].parent;
		nodes[lastWas].parent = nodePtr - nodes;
	    }
	    lastWas = nodesUsed;
	    nodesUsed++;
	    break;
	}
	}

	start += scanned;
	numBytes -= scanned;
    }

    if (code != TCL_OK && nodes != NULL) {
	ckfree((char*) nodes);
    }
    if (code == TCL_OK) {
	*opTreePtr = nodes;
    } else if (interp == NULL) {
	if (msg) {
	    Tcl_DecrRefCount(msg);
	}
    } else {
	if (msg == NULL) {
	    msg = Tcl_GetObjResult(interp);
	}
	Tcl_AppendPrintfToObj(msg, "\nin expression \"%s%.*s%.*s%s%s%.*s%s\"",
		((start - limit) < parsePtr->string) ? "" : "...",
		((start - limit) < parsePtr->string)
			? (start - parsePtr->string) : limit - 3,
		((start - limit) < parsePtr->string)
			? parsePtr->string : start - limit + 3,
		(scanned < limit) ? scanned : limit - 3, start,
		(scanned < limit) ? "" : "...", insertMark ? mark : "",
		(start + scanned + limit > parsePtr->end)
			? parsePtr->end - (start + scanned) : limit-3,
		start + scanned,
		(start + scanned + limit > parsePtr->end) ? "" : "...");
	if (post != NULL) {
	    Tcl_AppendToObj(msg, ";\n", -1);
	    Tcl_AppendObjToObj(msg, post);
	    Tcl_DecrRefCount(post);
	}
	Tcl_SetObjResult(interp, msg);
	numBytes = parsePtr->end - parsePtr->string;
	Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
		"\n    (parsing expression \"%.*s%s\")",
		(numBytes < limit) ? numBytes : limit - 3,
		parsePtr->string, (numBytes < limit) ? "" : "..."));
    }

    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateTokensForLiteral --
 *
 * Results:
 *	Number of bytes scanned.
 *
 * Side effects:
 *	The Tcl_Parse *parsePtr is filled with Tcl_Tokens representing the
 *	literal.
 *
 *----------------------------------------------------------------------
 */

static int
GenerateTokensForLiteral(
    const char *script,
    int numBytes,
    Tcl_Obj *litList,
    int nextLiteral,
    Tcl_Parse *parsePtr)
{
    int scanned, closer = 0;
    const char *start = script;
    Tcl_Token *destPtr;
    unsigned char lexeme;

    /*
     * Have to reparse to get pointers into source string.
     */

    scanned = TclParseAllWhiteSpace(start, numBytes);
    start +=scanned;
    scanned = ParseLexeme(start, numBytes-scanned, &lexeme, NULL);
    if ((lexeme != NUMBER) && (lexeme != BAREWORD)) {
	Tcl_Obj *literal;
	const char *bytes;

	Tcl_ListObjIndex(NULL, litList, nextLiteral, &literal);
	bytes = Tcl_GetStringFromObj(literal, &scanned);
	start++;
	if (memcmp(bytes, start, (size_t) scanned) == 0) {
	    closer = 1;
	} else {
	    /* TODO */
	    Tcl_Panic("figure this out");
	}
    }

    TclGrowParseTokenArray(parsePtr, 2);
    destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
    destPtr->type = TCL_TOKEN_SUB_EXPR;
    destPtr->start = start-closer;
    destPtr->size = scanned+2*closer;
    destPtr->numComponents = 1;
    destPtr++;
    destPtr->type = TCL_TOKEN_TEXT;
    destPtr->start = start;
    destPtr->size = scanned;
    destPtr->numComponents = 0;
    parsePtr->numTokens += 2;

    return (start + scanned + closer - script);
}

/*
 *----------------------------------------------------------------------
 *
 * CopyTokens --
 *
 * Results:
 *	Number of bytes scanned.
 *
 * Side effects:
 *	The Tcl_Parse *parsePtr is filled with Tcl_Tokens representing the
 *	literal.
 *
 *----------------------------------------------------------------------
 */

static int
CopyTokens(
    Tcl_Token *sourcePtr,
    Tcl_Parse *parsePtr)
{
    int toCopy = sourcePtr->numComponents + 1;
    Tcl_Token *destPtr;

    if (sourcePtr->numComponents == sourcePtr[1].numComponents + 1) {
	TclGrowParseTokenArray(parsePtr, toCopy);
	destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
	memcpy(destPtr, sourcePtr, (size_t) toCopy * sizeof(Tcl_Token));
	destPtr->type = TCL_TOKEN_SUB_EXPR;
	parsePtr->numTokens += toCopy;
    } else {
	TclGrowParseTokenArray(parsePtr, toCopy - 1);
	destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
	*destPtr = *sourcePtr;
	destPtr->type = TCL_TOKEN_SUB_EXPR;
	destPtr->numComponents++;
	destPtr++;
	memcpy(destPtr, sourcePtr, (size_t) toCopy * sizeof(Tcl_Token));
	parsePtr->numTokens += toCopy + 1;
    }
    return toCopy;
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertTreeToTokens --
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Tcl_Parse *parsePtr is filled with Tcl_Tokens representing the
 *	parsed expression.
 *
 *----------------------------------------------------------------------
 */

static void
ConvertTreeToTokens(
    Tcl_Interp *interp,
    const char *start,
    int numBytes,
    OpNode *nodes,
    Tcl_Obj *litList,
    Tcl_Token *tokenPtr,
    Tcl_Parse *parsePtr)
{
    OpNode *nodePtr = nodes;
    int nextLiteral = 0;
    int scanned, copied, tokenIdx;
    unsigned char lexeme;
    Tcl_Token *destPtr;

    while (1) {
	switch (NODE_TYPE & nodePtr->lexeme) {
	case UNARY:
	    if (nodePtr->right > OT_NONE) {
		int right = nodePtr->right;

		nodePtr->right = OT_NONE;
		if (nodePtr->lexeme != START) {
		    /*
		     * Find operator in string.
		     */

		    scanned = TclParseAllWhiteSpace(start, numBytes);
		    start +=scanned;
		    numBytes -= scanned;
		    scanned = ParseLexeme(start, numBytes, &lexeme, NULL);
		    if (lexeme != nodePtr->lexeme) {
			if (lexeme != (nodePtr->lexeme & ~NODE_TYPE)) {
			    Tcl_Panic("lexeme mismatch");
			}
		    }
		    if (nodePtr->lexeme != OPEN_PAREN) {
			TclGrowParseTokenArray(parsePtr, 2);
			nodePtr->right = OT_NONE - parsePtr->numTokens;
			destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
			destPtr->type = TCL_TOKEN_SUB_EXPR;
			destPtr->start = start;
			destPtr++;
			destPtr->type = TCL_TOKEN_OPERATOR;
			destPtr->start = start;
			destPtr->size = scanned;
			destPtr->numComponents = 0;
			parsePtr->numTokens += 2;
		    }
		    start +=scanned;
		    numBytes -= scanned;
		}
		switch (right) {
		case OT_EMPTY:
		    break;
		case OT_LITERAL:
		    scanned = GenerateTokensForLiteral(start, numBytes,
			    litList, nextLiteral++, parsePtr);
		    start +=scanned;
		    numBytes -= scanned;
		    break;
		case OT_TOKENS:
		    copied = CopyTokens(tokenPtr, parsePtr);
		    scanned = tokenPtr->start + tokenPtr->size - start;
		    start +=scanned;
		    numBytes -= scanned;
		    tokenPtr += copied;
		    break;
		default:
		    nodePtr = nodes + right;
		}
	    } else {
		if (nodePtr->lexeme == START) {
		    /*
		     * We're done.
		     */

		    return;
		}
		if (nodePtr->lexeme == OPEN_PAREN) {
		    /*
		     * Skip past matching close paren.
		     */

		    scanned = TclParseAllWhiteSpace(start, numBytes);
		    start +=scanned;
		    numBytes -= scanned;
		    scanned = ParseLexeme(start, numBytes, &lexeme, NULL);
		    start +=scanned;
		    numBytes -= scanned;
		} else {
		    tokenIdx = OT_NONE - nodePtr->right;
		    nodePtr->right = OT_NONE;
		    destPtr = parsePtr->tokenPtr + tokenIdx;
		    destPtr->size = start - destPtr->start;
		    destPtr->numComponents = parsePtr->numTokens - tokenIdx - 1;
		}
		nodePtr = nodes + nodePtr->parent;
	    }
	    break;
	case BINARY:
	    if (nodePtr->left > OT_NONE) {
		int left = nodePtr->left;

		nodePtr->left = OT_NONE;
		scanned = TclParseAllWhiteSpace(start, numBytes);
		start +=scanned;
		numBytes -= scanned;
		if ((nodePtr->lexeme != COMMA) && (nodePtr->lexeme != COLON)) {
		    TclGrowParseTokenArray(parsePtr, 2);
		    nodePtr->left = OT_NONE - parsePtr->numTokens;
		    destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
		    destPtr->type = TCL_TOKEN_SUB_EXPR;
		    destPtr->start = start;
		    destPtr++;
		    destPtr->type = TCL_TOKEN_OPERATOR;
		    parsePtr->numTokens += 2;
		}
		switch (left) {
		case OT_LITERAL:
		    scanned = GenerateTokensForLiteral(start, numBytes,
			    litList, nextLiteral++, parsePtr);
		    start +=scanned;
		    numBytes -= scanned;
		    break;
		case OT_TOKENS:
		    copied = CopyTokens(tokenPtr, parsePtr);
		    scanned = tokenPtr->start + tokenPtr->size - start;
		    start +=scanned;
		    numBytes -= scanned;
		    tokenPtr += copied;
		    break;
		default:
		    nodePtr = nodes + left;
		}
	    } else if (nodePtr->right > OT_NONE) {
		int right = nodePtr->right;

		nodePtr->right = OT_NONE;
		scanned = TclParseAllWhiteSpace(start, numBytes);
		start +=scanned;
		numBytes -= scanned;
		scanned = ParseLexeme(start, numBytes, &lexeme, NULL);
		if (lexeme != nodePtr->lexeme) {
		    if (lexeme != (nodePtr->lexeme & ~NODE_TYPE)) {
			Tcl_Panic("lexeme mismatch");
		    }
		}

		if ((nodePtr->lexeme != COMMA) && (nodePtr->lexeme != COLON)) {
		    tokenIdx = OT_NONE - nodePtr->left;
		    destPtr = parsePtr->tokenPtr + tokenIdx + 1;
		    destPtr->start = start;
		    destPtr->size = scanned;
		    destPtr->numComponents = 0;
		}
		start +=scanned;
		numBytes -= scanned;
		switch (right) {
		case OT_LITERAL:
		    scanned = GenerateTokensForLiteral(start, numBytes,
			    litList, nextLiteral++, parsePtr);
		    start +=scanned;
		    numBytes -= scanned;
		    break;
		case OT_TOKENS:
		    copied = CopyTokens(tokenPtr, parsePtr);
		    scanned = tokenPtr->start + tokenPtr->size - start;
		    start +=scanned;
		    numBytes -= scanned;
		    tokenPtr += copied;
		    break;
		default:
		    nodePtr = nodes + right;
		}
	    } else {
		if ((nodePtr->lexeme != COMMA) && (nodePtr->lexeme != COLON)) {
		    tokenIdx = OT_NONE - nodePtr->left;
		    nodePtr->left = OT_NONE;
		    destPtr = parsePtr->tokenPtr + tokenIdx;
		    destPtr->size = start - destPtr->start;
		    destPtr->numComponents = parsePtr->numTokens-tokenIdx-1;
		}
		nodePtr = nodes + nodePtr->parent;
	    }
	    break;
	}
    }
}

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
 *	If the string is successfully parsed as a valid Tcl expression, TCL_OK
 *	is returned, and data about the expression structure is written to
 *	*parsePtr. If the string cannot be parsed as a valid Tcl expression,
 *	TCL_ERROR is returned, and if interp is non-NULL, an error message is
 *	written to interp.
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
    const char *start,		/* Start of source string to parse. */
    int numBytes,		/* Number of bytes in string. If < 0, the
				 * string consists of all bytes up to the
				 * first null character. */
    Tcl_Parse *parsePtr)	/* Structure to fill with information about
				 * the parsed expression; any previous
				 * information in the structure is ignored. */
{
    OpNode *opTree = NULL;	/* Will point to the tree of operators */
    Tcl_Obj *litList = Tcl_NewObj();	/* List to hold the literals */
    Tcl_Obj *funcList = Tcl_NewObj();	/* List to hold the functon names*/
    Tcl_Parse *exprParsePtr =
	    (Tcl_Parse *) TclStackAlloc(interp, sizeof(Tcl_Parse));
				/* Holds the Tcl_Tokens of substitutions */
    int code = ParseExpr(interp, start, numBytes, &opTree, litList,
	    funcList, exprParsePtr);

    if (numBytes < 0) {
	numBytes = (start ? strlen(start) : 0);
    }

    TclParseInit(interp, start, numBytes, parsePtr);
    if (code == TCL_OK) {
	ConvertTreeToTokens(interp, start, numBytes, opTree, litList,
		exprParsePtr->tokenPtr, parsePtr);
    } else {
	/* TODO: copy over any error info to *parsePtr */
    }

    Tcl_FreeParse(exprParsePtr);
    TclStackFree(interp, exprParsePtr);
    Tcl_DecrRefCount(funcList);
    Tcl_DecrRefCount(litList);
    ckfree((char *) opTree);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseLexeme --
 *
 *	Parse a single lexeme from the start of a string, scanning no more
 *	than numBytes bytes.
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
    const char *start,		/* Start of lexeme to parse. */
    int numBytes,		/* Number of bytes in string. */
    unsigned char *lexemePtr,	/* Write code of parsed lexeme to this
				 * storage. */
    Tcl_Obj **literalPtr)	/* Write corresponding literal value to this
				   storage, if non-NULL. */
{
    const char *end;
    int scanned;
    Tcl_UniChar ch;
    Tcl_Obj *literal = NULL;

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

    literal = Tcl_NewObj();
    if (TclParseNumber(NULL, literal, NULL, start, numBytes, &end,
	    TCL_PARSE_NO_WHITESPACE) == TCL_OK) {
	TclInitStringRep(literal, start, end-start);
	*lexemePtr = NUMBER;
	if (literalPtr) {
	    *literalPtr = literal;
	} else {
	    Tcl_DecrRefCount(literal);
	}
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
	Tcl_DecrRefCount(literal);
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
    if (literalPtr) {
	Tcl_SetStringObj(literal, start, (int) (end-start));
	*literalPtr = literal;
    } else {
	Tcl_DecrRefCount(literal);
    }
    return (end-start);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileExpr --
 *
 *	This procedure compiles a string containing a Tcl expression into Tcl
 *	bytecodes. This procedure is the top-level interface to the the
 *	expression compilation module, and is used by such public procedures
 *	as Tcl_ExprString, Tcl_ExprStringObj, Tcl_ExprLong, Tcl_ExprDouble,
 *	Tcl_ExprBoolean, and Tcl_ExprBooleanObj.
 *
 * Results:
 *	The return value is TCL_OK on a successful compilation and TCL_ERROR
 *	on failure. If TCL_ERROR is returned, then the interpreter's result
 *	contains an error message.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileExpr(
    Tcl_Interp *interp,		/* Used for error reporting. */
    const char *script,		/* The source script to compile. */
    int numBytes,		/* Number of bytes in script. If < 0, the
				 * string consists of all bytes up to the
				 * first null character. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    OpNode *opTree = NULL;	/* Will point to the tree of operators */
    Tcl_Obj *litList = Tcl_NewObj();	/* List to hold the literals */
    Tcl_Obj *funcList = Tcl_NewObj();	/* List to hold the functon names*/
    Tcl_Parse *parsePtr =
	    (Tcl_Parse *) TclStackAlloc(interp, sizeof(Tcl_Parse));
				/* Holds the Tcl_Tokens of substitutions */

    int code = ParseExpr(interp, script, numBytes, &opTree, litList,
	    funcList, parsePtr);

    if (code == TCL_OK) {
	int litObjc, needsNumConversion = 1;
	Tcl_Obj **litObjv;

	/* TIP #280 : Track Lines within the expression */
	TclAdvanceLines(&envPtr->line, script,
		script + TclParseAllWhiteSpace(script, numBytes));

	/*
	 * Valid parse; compile the tree.
	 */

	Tcl_ListObjGetElements(NULL, litList, &litObjc, &litObjv);
	CompileExprTree(interp, opTree, litObjv, funcList, parsePtr->tokenPtr,
		&needsNumConversion, envPtr);
	if (needsNumConversion) {
	    /*
	     * Attempt to convert the expression result to an int or double.
	     * This is done in order to support Tcl's policy of interpreting
	     * operands if at all possible as first integers, else
	     * floating-point numbers.
	     */

	    TclEmitOpcode(INST_TRY_CVT_TO_NUMERIC, envPtr);
	}
    }

    Tcl_FreeParse(parsePtr);
    TclStackFree(interp, parsePtr);
    Tcl_DecrRefCount(funcList);
    Tcl_DecrRefCount(litList);
    ckfree((char *) opTree);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileExprTree --
 *	[???]
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the expression at runtime.
 *
 *----------------------------------------------------------------------
 */

static void
CompileExprTree(
    Tcl_Interp *interp,
    OpNode *nodes,
    Tcl_Obj *const litObjv[],
    Tcl_Obj *funcList,
    Tcl_Token *tokenPtr,
    int *convertPtr,
    CompileEnv *envPtr)
{
    OpNode *nodePtr = nodes;
    int nextFunc = 0;
    JumpList *freePtr, *jumpPtr = NULL;
    static const int instruction[] = {
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,		INST_ADD,	INST_SUB,	0, /* COMMA */
	INST_MULT,	INST_DIV,	INST_MOD,	INST_LT,
	INST_GT,	INST_BITAND,	INST_BITXOR,	INST_BITOR,
	0, /* QUESTION */	0, /* COLON */
	INST_LSHIFT,	INST_RSHIFT,	INST_LE,	INST_GE,
	INST_EQ,	INST_NEQ,	0, /* AND */	0, /* OR */
	INST_STR_EQ,	INST_STR_NEQ,	INST_EXPON,	INST_LIST_IN,
	INST_LIST_NOT_IN,	0, /* CLOSE_PAREN */	0, /* END */
	0,		0,		0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,	0,  0,
	0,		INST_UPLUS,	INST_UMINUS,	0, /* FUNCTION */
	0, /* START */	0, /* OPEN_PAREN */
	INST_LNOT,	INST_BITNOT
    };

    while (1) {
	switch (NODE_TYPE & nodePtr->lexeme) {
	case UNARY:
	    if (nodePtr->right > OT_NONE) {
		int right = nodePtr->right;

		nodePtr->right = OT_NONE;
		if (nodePtr->lexeme == FUNCTION) {
		    Tcl_DString cmdName;
		    Tcl_Obj *funcName;
		    const char *p;
		    int length;

		    Tcl_DStringInit(&cmdName);
		    Tcl_DStringAppend(&cmdName, "tcl::mathfunc::", -1);
		    Tcl_ListObjIndex(NULL, funcList, nextFunc++, &funcName);
		    p = Tcl_GetStringFromObj(funcName, &length);
		    Tcl_DStringAppend(&cmdName, p, length);
		    TclEmitPush(TclRegisterNewNSLiteral(envPtr,
			    Tcl_DStringValue(&cmdName),
			    Tcl_DStringLength(&cmdName)), envPtr);
		    Tcl_DStringFree(&cmdName);
		}
		switch (right) {
		case OT_EMPTY:
		    break;
		case OT_LITERAL:
		    /* TODO: reduce constant expressions */
		    TclEmitPush( TclAddLiteralObj(
			    envPtr, *litObjv++, NULL), envPtr);
		    break;
		case OT_TOKENS:
		    if (tokenPtr->type != TCL_TOKEN_WORD) {
			Tcl_Panic("unexpected token type %d\n",
				tokenPtr->type);
		    }
		    TclCompileTokens(interp, tokenPtr+1,
			    tokenPtr->numComponents, envPtr);
		    tokenPtr += tokenPtr->numComponents + 1;
		    break;
		default:
		    nodePtr = nodes + right;
		}
	    } else {
		if (nodePtr->lexeme == START) {
		    /* We're done */
		    return;
		}
		if (nodePtr->lexeme == OPEN_PAREN) {
		    /* do nothing */
		} else if (nodePtr->lexeme == FUNCTION) {
		    int numWords = (nodePtr[1].left - OT_NONE) + 1;
		    if (numWords < 255) {
			TclEmitInstInt1(INST_INVOKE_STK1, numWords, envPtr);
		    } else {
			TclEmitInstInt4(INST_INVOKE_STK4, numWords, envPtr);
		    }
		    *convertPtr = 1;
		} else {
		    TclEmitOpcode(instruction[nodePtr->lexeme], envPtr);
		    *convertPtr = 0;
		}
		nodePtr = nodes + nodePtr->parent;
	    }
	    break;
	case BINARY:
	    if (nodePtr->left > OT_NONE) {
		int left = nodePtr->left;
		nodePtr->left = OT_NONE;
		/* TODO: reduce constant expressions */
		if (nodePtr->lexeme == QUESTION) {
		    JumpList *newJump = (JumpList *)
			    TclStackAlloc(interp, sizeof(JumpList));
		    newJump->next = jumpPtr;
		    jumpPtr = newJump;
		    newJump = (JumpList *)
			    TclStackAlloc(interp, sizeof(JumpList));
		    newJump->next = jumpPtr;
		    jumpPtr = newJump;
		    jumpPtr->depth = envPtr->currStackDepth;
		    *convertPtr = 1;
		} else if (nodePtr->lexeme == AND || nodePtr->lexeme == OR) {
		    JumpList *newJump = (JumpList *)
			    TclStackAlloc(interp, sizeof(JumpList));
		    newJump->next = jumpPtr;
		    jumpPtr = newJump;
		    newJump = (JumpList *)
			    TclStackAlloc(interp, sizeof(JumpList));
		    newJump->next = jumpPtr;
		    jumpPtr = newJump;
		    newJump =  (JumpList *)
			    TclStackAlloc(interp, sizeof(JumpList));
		    newJump->next = jumpPtr;
		    jumpPtr = newJump;
		    jumpPtr->depth = envPtr->currStackDepth;
		}
		switch (left) {
		case OT_LITERAL:
		    TclEmitPush(TclAddLiteralObj(envPtr, *litObjv++, NULL),
			    envPtr);
		    break;
		case OT_TOKENS:
		    if (tokenPtr->type != TCL_TOKEN_WORD) {
			Tcl_Panic("unexpected token type %d\n",
				tokenPtr->type);
		    }
		    TclCompileTokens(interp, tokenPtr+1,
			    tokenPtr->numComponents, envPtr);
		    tokenPtr += tokenPtr->numComponents + 1;
		    break;
		default:
		    nodePtr = nodes + left;
		}
	    } else if (nodePtr->right > OT_NONE) {
		int right = nodePtr->right;

		nodePtr->right = OT_NONE;
		if (nodePtr->lexeme == QUESTION) {
		    TclEmitForwardJump(envPtr, TCL_FALSE_JUMP,
			    &(jumpPtr->jump));
		} else if (nodePtr->lexeme == COLON) {
		    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP,
			    &(jumpPtr->next->jump));
		    envPtr->currStackDepth = jumpPtr->depth;
		    jumpPtr->offset = (envPtr->codeNext - envPtr->codeStart);
		    jumpPtr->convert = *convertPtr;
		    *convertPtr = 1;
		} else if (nodePtr->lexeme == AND) {
		    TclEmitForwardJump(envPtr, TCL_FALSE_JUMP,
			    &(jumpPtr->jump));
		} else if (nodePtr->lexeme == OR) {
		    TclEmitForwardJump(envPtr, TCL_TRUE_JUMP,
			    &(jumpPtr->jump));
		}
		switch (right) {
		case OT_LITERAL:
		    TclEmitPush(TclAddLiteralObj(envPtr, *litObjv++, NULL),
			    envPtr);
		    break;
		case OT_TOKENS:
		    if (tokenPtr->type != TCL_TOKEN_WORD) {
			Tcl_Panic("unexpected token type %d\n",
				tokenPtr->type);
		    }
		    TclCompileTokens(interp, tokenPtr+1,
			    tokenPtr->numComponents, envPtr);
		    tokenPtr += tokenPtr->numComponents + 1;
		    break;
		default:
		    nodePtr = nodes + right;
		}
	    } else {
		if (nodePtr->lexeme == COMMA || nodePtr->lexeme == QUESTION) {
		    /* do nothing */
		} else if (nodePtr->lexeme == COLON) {
		    if (TclFixupForwardJump(envPtr, &(jumpPtr->next->jump),
			    (envPtr->codeNext - envPtr->codeStart)
			    - jumpPtr->next->jump.codeOffset, 127)) {
			jumpPtr->offset += 3;
		    }
		    TclFixupForwardJump(envPtr, &(jumpPtr->jump),
			    jumpPtr->offset - jumpPtr->jump.codeOffset, 127);
		    *convertPtr |= jumpPtr->convert;
		    envPtr->currStackDepth = jumpPtr->depth + 1;
		    freePtr = jumpPtr;
		    jumpPtr = jumpPtr->next;
		    TclStackFree(interp, freePtr);
		    freePtr = jumpPtr;
		    jumpPtr = jumpPtr->next;
		    TclStackFree(interp, freePtr);
		} else if (nodePtr->lexeme == AND) {
		    TclEmitForwardJump(envPtr, TCL_FALSE_JUMP,
			    &(jumpPtr->next->jump));
		    TclEmitPush(TclRegisterNewLiteral(envPtr, "1", 1), envPtr);
		} else if (nodePtr->lexeme == OR) {
		    TclEmitForwardJump(envPtr, TCL_TRUE_JUMP,
			    &(jumpPtr->next->jump));
		    TclEmitPush(TclRegisterNewLiteral(envPtr, "0", 1), envPtr);
		} else {
		    TclEmitOpcode(instruction[nodePtr->lexeme], envPtr);
		    *convertPtr = 0;
		}
		if ((nodePtr->lexeme == AND) || (nodePtr->lexeme == OR)) {
		    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP,
			    &(jumpPtr->next->next->jump));
		    TclFixupForwardJumpToHere(envPtr,
			    &(jumpPtr->next->jump), 127);
		    if (TclFixupForwardJumpToHere(envPtr,
			    &(jumpPtr->jump), 127)) {
			jumpPtr->next->next->jump.codeOffset += 3;
		    }
		    TclEmitPush(TclRegisterNewLiteral(envPtr,
			    (nodePtr->lexeme == AND) ? "0" : "1", 1), envPtr);
		    TclFixupForwardJumpToHere(envPtr,
			    &(jumpPtr->next->next->jump), 127);
		    *convertPtr = 0;
		    envPtr->currStackDepth = jumpPtr->depth + 1;
		    freePtr = jumpPtr;
		    jumpPtr = jumpPtr->next;
		    TclStackFree(interp, freePtr);
		    freePtr = jumpPtr;
		    jumpPtr = jumpPtr->next;
		    TclStackFree(interp, freePtr);
		    freePtr = jumpPtr;
		    jumpPtr = jumpPtr->next;
		    TclStackFree(interp, freePtr);
		}
		nodePtr = nodes + nodePtr->parent;
	    }
	    break;
	}
    }
}

static int
OpCmd(
    Tcl_Interp *interp,
    OpNode *nodes,
    Tcl_Obj * const litObjv[])
{
    CompileEnv *compEnvPtr;
    ByteCode *byteCodePtr;
    int code, tmp=1;
    Tcl_Obj *byteCodeObj = Tcl_NewObj();

    /*
     * Note we are compiling an expression with literal arguments. This means
     * there can be no [info frame] calls when we execute the resulting
     * bytecode, so there's no need to tend to TIP 280 issues.
     */

    compEnvPtr = (CompileEnv *) TclStackAlloc(interp, sizeof(CompileEnv));
    TclInitCompileEnv(interp, compEnvPtr, NULL, 0, NULL, 0);
    CompileExprTree(interp, nodes, litObjv, NULL, NULL, &tmp, compEnvPtr);
    TclEmitOpcode(INST_DONE, compEnvPtr);
    Tcl_IncrRefCount(byteCodeObj);
    TclInitByteCodeObj(byteCodeObj, compEnvPtr);
    TclFreeCompileEnv(compEnvPtr);
    TclStackFree(interp, compEnvPtr);
    byteCodePtr = (ByteCode *) byteCodeObj->internalRep.otherValuePtr;
    code = TclExecuteByteCode(interp, byteCodePtr);
    Tcl_DecrRefCount(byteCodeObj);
    return code;
}

int
TclSingleOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    TclOpCmdClientData *occdPtr = (TclOpCmdClientData *)clientData;
    unsigned char lexeme;
    OpNode nodes[2];

    if (objc != 1+occdPtr->numArgs) {
	Tcl_WrongNumArgs(interp, 1, objv, occdPtr->expected);
	return TCL_ERROR;
    }

    ParseLexeme(occdPtr->operator, strlen(occdPtr->operator), &lexeme, NULL);
    nodes[0].lexeme = START;
    nodes[0].right = 1;
    nodes[1].lexeme = lexeme;
    nodes[1].left = OT_LITERAL;
    nodes[1].right = OT_LITERAL;
    nodes[1].parent = 0;

    return OpCmd(interp, nodes, objv+1);
}

int
TclSortingOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int code = TCL_OK;

    if (objc < 3) {
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
    } else {
	TclOpCmdClientData *occdPtr = (TclOpCmdClientData *)clientData;
	Tcl_Obj **litObjv = (Tcl_Obj **) TclStackAlloc(interp,
		2*(objc-2)*sizeof(Tcl_Obj *));
	OpNode *nodes = (OpNode *) TclStackAlloc(interp,
		2*(objc-2)*sizeof(OpNode));
	unsigned char lexeme;
	int i, lastAnd = 1;

	ParseLexeme(occdPtr->operator, strlen(occdPtr->operator),
		&lexeme, NULL);

	litObjv[0] = objv[1];
	nodes[0].lexeme = START;
	for (i=2; i<objc-1; i++) {
	    litObjv[2*(i-1)-1] = objv[i];
	    nodes[2*(i-1)-1].lexeme = lexeme;
	    nodes[2*(i-1)-1].left = OT_LITERAL;
	    nodes[2*(i-1)-1].right = OT_LITERAL;

	    litObjv[2*(i-1)] = objv[i];
	    nodes[2*(i-1)].lexeme = AND;
	    nodes[2*(i-1)].left = lastAnd;
	    nodes[lastAnd].parent = 2*(i-1);

	    nodes[2*(i-1)].right = 2*(i-1)+1;
	    nodes[2*(i-1)+1].parent= 2*(i-1);

	    lastAnd = 2*(i-1);
	}
	litObjv[2*(objc-2)-1] = objv[objc-1];

	nodes[2*(objc-2)-1].lexeme = lexeme;
	nodes[2*(objc-2)-1].left = OT_LITERAL;
	nodes[2*(objc-2)-1].right = OT_LITERAL;

	nodes[0].right = lastAnd;
	nodes[lastAnd].parent = 0;

	code = OpCmd(interp, nodes, litObjv);

	TclStackFree(interp, nodes);
	TclStackFree(interp, litObjv);
    }
    return code;
}

int
TclVariadicOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    TclOpCmdClientData *occdPtr = (TclOpCmdClientData *)clientData;
    unsigned char lexeme;
    int code;

    if (objc < 2) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(occdPtr->numArgs));
	return TCL_OK;
    }

    ParseLexeme(occdPtr->operator, strlen(occdPtr->operator), &lexeme, NULL);
    lexeme |= BINARY;

    if (objc == 2) {
	Tcl_Obj *litObjv[2];
	OpNode nodes[2];
	int decrMe = 0;

	if (lexeme == EXPON) {
	    litObjv[1] = Tcl_NewIntObj(occdPtr->numArgs);
	    Tcl_IncrRefCount(litObjv[1]);
	    decrMe = 1;
	    litObjv[0] = objv[1];
	    nodes[0].lexeme = START;
	    nodes[0].right = 1;
	    nodes[1].lexeme = lexeme;
	    nodes[1].left = OT_LITERAL;
	    nodes[1].right = OT_LITERAL;
	    nodes[1].parent = 0;
	} else {
	    if (lexeme == DIVIDE) {
		litObjv[0] = Tcl_NewDoubleObj(1.0);
	    } else {
		litObjv[0] = Tcl_NewIntObj(occdPtr->numArgs);
	    }
	    Tcl_IncrRefCount(litObjv[0]);
	    litObjv[1] = objv[1];
	    nodes[0].lexeme = START;
	    nodes[0].right = 1;
	    nodes[1].lexeme = lexeme;
	    nodes[1].left = OT_LITERAL;
	    nodes[1].right = OT_LITERAL;
	    nodes[1].parent = 0;
	}

	code = OpCmd(interp, nodes, litObjv);

	Tcl_DecrRefCount(litObjv[decrMe]);
	return code;
    } else {
	OpNode *nodes = (OpNode *) TclStackAlloc(interp,
		(objc-1)*sizeof(OpNode));
	int i, lastOp = OT_LITERAL;

	nodes[0].lexeme = START;
	if (lexeme == EXPON) {
	    for (i=objc-2; i>0; i-- ) {
		nodes[i].lexeme = lexeme;
		nodes[i].left = OT_LITERAL;
		nodes[i].right = lastOp;
		if (lastOp >= 0) {
		    nodes[lastOp].parent = i;
		}
		lastOp = i;
	    }
	} else {
	    for (i=1; i<objc-1; i++ ) {
		nodes[i].lexeme = lexeme;
		nodes[i].left = lastOp;
		if (lastOp >= 0) {
		    nodes[lastOp].parent = i;
		}
		nodes[i].right = OT_LITERAL;
		lastOp = i;
	    }
	}
	nodes[0].right = lastOp;
	nodes[lastOp].parent = 0;

	code = OpCmd(interp, nodes, objv+1);

	TclStackFree(interp, nodes);

	return code;
    }
}

int
TclNoIdentOpCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    TclOpCmdClientData *occdPtr = (TclOpCmdClientData *)clientData;
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, occdPtr->expected);
	return TCL_ERROR;
    }
    return TclVariadicOpCmd(clientData, interp, objc, objv);
}
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
