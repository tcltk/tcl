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
 * RCS: @(#) $Id: tclCompExpr.c,v 1.36 2006/11/13 08:23:07 das Exp $
 */

#include "tclInt.h"
#include "tclCompile.h"

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
#define NUM_STATIC_NODES 64
    ExprNode staticNodes[NUM_STATIC_NODES];
    ExprNode *lastOrphanPtr, *nodes = staticNodes;
    int nodesAvailable = NUM_STATIC_NODES;
    int nodesUsed = 0;
    Tcl_Parse scratch;		/* Parsing scratch space */
    Tcl_Obj *msg = NULL, *post = NULL;
    int scanned = 0, code = TCL_OK, insertMark = 0;
    CONST char *mark = "_@_";
    CONST int limit = 25;
    static CONST unsigned char prec[80] = {
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
		msg = TclObjPrintf(
			"invalid character \"%.*s\"", scanned, start);
		code = TCL_ERROR;
		continue;
	    case INCOMPLETE:
		msg = TclObjPrintf(
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
			msg = TclObjPrintf(
				"invalid bareword \"%.*s%s\"",
				(scanned < limit) ? scanned : limit - 3, start,
				(scanned < limit) ? "" : "...");
			post = TclObjPrintf(
				"should be \"$%.*s%s\" or \"{%.*s%s}\"",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...",
				(scanned < limit) ? scanned : limit - 3,
				start, (scanned < limit) ? "" : "...");
			TclAppendPrintfToObj(post,
				" or \"%.*s%s(...)\" or ...",
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
	case LEAF: {
	    CONST char *end;

	    if ((NODE_TYPE & lastNodePtr->lexeme) == LEAF) {
		CONST char *operand =
			scratch.tokenPtr[lastNodePtr->token].start;

		msg = TclObjPrintf("missing operator at %s", mark);
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

	    if (scratch.numTokens+1 >= scratch.tokensAvailable) {
		TclExpandTokenArray(&scratch);
	    }
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
	}

	case UNARY:
	    if ((NODE_TYPE & lastNodePtr->lexeme) == LEAF) {
		msg = TclObjPrintf("missing operator at %s", mark);
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }
	    nodePtr->left = -1;
	    nodePtr->right = -1;
	    nodePtr->parent = -1;

	    if (scratch.numTokens >= scratch.tokensAvailable) {
		TclExpandTokenArray(&scratch);
	    }
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
	    ExprNode *otherPtr = NULL;
	    unsigned char precedence = prec[nodePtr->lexeme];

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
		msg = TclObjPrintf("empty subexpression at %s", mark);
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }


	    if ((NODE_TYPE & lastNodePtr->lexeme) != LEAF) {
		if (prec[lastNodePtr->lexeme] > precedence) {
		    if (lastNodePtr->lexeme == OPEN_PAREN) {
			msg = Tcl_NewStringObj("unbalanced open paren", -1);
		    } else if (lastNodePtr->lexeme == COMMA) {
			msg = TclObjPrintf( 
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
			msg = TclObjPrintf(
				"missing function argument at %s", mark);
			scanned = 0;
			insertMark = 1;
		    }
		}
		if (msg == NULL) {
		    msg = TclObjPrintf("missing operand at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		}
		code = TCL_ERROR;
		continue;
	    }

	    while (1) {

		if (lastOrphanPtr->parent >= 0) {
		    otherPtr = nodes + lastOrphanPtr->parent;
		} else if (lastOrphanPtr->left >= 0) {
		    Tcl_Panic("Tcl_ParseExpr: left closure programming error");
		} else {
		    lastOrphanPtr->parent = lastOrphanPtr - nodes;
		    otherPtr = lastOrphanPtr;
		}
		otherPtr--;

		if (prec[otherPtr->lexeme] < precedence) {
		    break;
		}

		if (prec[otherPtr->lexeme] == precedence) {
		    /* Special association rules for the ternary operators. */
		    if ((otherPtr->lexeme == QUESTION) 
			    && (lastOrphanPtr->lexeme != COLON)) {
			break;
		    }
		    if ((otherPtr->lexeme == COLON)
			    && (nodePtr->lexeme == QUESTION)) {
			break;
		    }
		    /* Right association rules for exponentiation. */
		    if (nodePtr->lexeme == EXPON) {
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
		    msg = TclObjPrintf(
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

	    if (scratch.numTokens >= scratch.tokensAvailable) {
		TclExpandTokenArray(&scratch);
	    }
	    nodePtr->token = scratch.numTokens;
	    tokenPtr = scratch.tokenPtr + nodePtr->token;
	    tokenPtr->type = TCL_TOKEN_OPERATOR;
	    tokenPtr->start = start;
	    tokenPtr->size = scanned;
	    tokenPtr->numComponents = 0;
	    scratch.numTokens++;

	    nodePtr->left = lastOrphanPtr - nodes;
	    nodePtr->parent = lastOrphanPtr->parent;
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
	    TclAppendPrintfToObj(msg, "\nin expression \"%s%.*s%.*s%s%s%.*s%s\"",
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
	    TclAppendObjToErrorInfo(interp, TclObjPrintf(
		    "\n    (parsing expression \"%.*s%s\")",
		    (numBytes < limit) ? numBytes : limit - 3,
		    scratch.string, (numBytes < limit) ? "" : "..."));
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
		    if (parsePtr->numTokens + 1 >= parsePtr->tokensAvailable) {
			TclExpandTokenArray(parsePtr);
		    }
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
		    if (parsePtr->numTokens + 1 >= parsePtr->tokensAvailable) {
			TclExpandTokenArray(parsePtr);
		    }
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
		    while (parsePtr->numTokens + toCopy + 1
			    >= parsePtr->tokensAvailable) {
			TclExpandTokenArray(parsePtr);
		    }
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
		while (parsePtr->numTokens + toCopy - 1
			>= parsePtr->tokensAvailable) {
		    TclExpandTokenArray(parsePtr);
		}
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

/*
 * Boolean variable that controls whether expression compilation tracing is
 * enabled.
 */

#ifdef TCL_COMPILE_DEBUG
static int traceExprComp = 0;
#endif /* TCL_COMPILE_DEBUG */

/*
 * Definitions of numeric codes representing each expression operator.  The
 * order of these must match the entries in the operatorTable below.  Also the
 * codes for the relational operators (OP_LESS, OP_GREATER, OP_LE, OP_GE,
 * OP_EQ, and OP_NE) must be consecutive and in that order.  Note that OP_PLUS
 * and OP_MINUS represent both unary and binary operators.
 */

#define OP_MULT		0
#define OP_DIVIDE	1
#define OP_MOD		2
#define OP_PLUS		3
#define OP_MINUS	4
#define OP_LSHIFT	5
#define OP_RSHIFT	6
#define OP_LESS		7
#define OP_GREATER	8
#define OP_LE		9
#define OP_GE		10
#define OP_EQ		11
#define OP_NEQ		12
#define OP_BITAND	13
#define OP_BITXOR	14
#define OP_BITOR	15
#define OP_LAND		16
#define OP_LOR		17
#define OP_QUESTY	18
#define OP_LNOT		19
#define OP_BITNOT	20
#define OP_STREQ	21
#define OP_STRNEQ	22
#define OP_EXPON	23
#define OP_IN_LIST	24
#define OP_NOT_IN_LIST	25

/*
 * Table describing the expression operators. Entries in this table must
 * correspond to the definitions of numeric codes for operators just above.
 */

static int opTableInitialized = 0; /* 0 means not yet initialized. */

TCL_DECLARE_MUTEX(opMutex)

typedef struct OperatorDesc {
    char *name;			/* Name of the operator. */
    int numOperands;		/* Number of operands. 0 if the operator
				 * requires special handling. */
    int instruction;		/* Instruction opcode for the operator.
				 * Ignored if numOperands is 0. */
} OperatorDesc;

static OperatorDesc operatorTable[] = {
    {"*",   2,  INST_MULT},
    {"/",   2,  INST_DIV},
    {"%",   2,  INST_MOD},
    {"+",   0},
    {"-",   0},
    {"<<",  2,  INST_LSHIFT},
    {">>",  2,  INST_RSHIFT},
    {"<",   2,  INST_LT},
    {">",   2,  INST_GT},
    {"<=",  2,  INST_LE},
    {">=",  2,  INST_GE},
    {"==",  2,  INST_EQ},
    {"!=",  2,  INST_NEQ},
    {"&",   2,  INST_BITAND},
    {"^",   2,  INST_BITXOR},
    {"|",   2,  INST_BITOR},
    {"&&",  0},
    {"||",  0},
    {"?",   0},
    {"!",   1,  INST_LNOT},
    {"~",   1,  INST_BITNOT},
    {"eq",  2,  INST_STR_EQ},
    {"ne",  2,  INST_STR_NEQ},
    {"**",  2,	INST_EXPON},
    {"in",  2,	INST_LIST_IN},
    {"ni",  2,	INST_LIST_NOT_IN},
    {NULL}
};

/*
 * Hashtable used to map the names of expression operators to the index of
 * their OperatorDesc description.
 */

static Tcl_HashTable opHashTable;

/*
 * Declarations for local procedures to this file:
 */

static void		CompileCondExpr(Tcl_Interp *interp,
			    Tcl_Token *exprTokenPtr, int *convertPtr,
			    CompileEnv *envPtr);
static void		CompileLandOrLorExpr(Tcl_Interp *interp,
			    Tcl_Token *exprTokenPtr, int opIndex,
			    CompileEnv *envPtr);
static void		CompileMathFuncCall(Tcl_Interp *interp,
			    Tcl_Token *exprTokenPtr, CONST char *funcName,
			    CompileEnv *envPtr);
static void		CompileSubExpr(Tcl_Interp *interp,
			    Tcl_Token *exprTokenPtr, int *convertPtr,
			    CompileEnv *envPtr);

/*
 * Macro used to debug the execution of the expression compiler.
 */

#ifdef TCL_COMPILE_DEBUG
#define TRACE(exprBytes, exprLength, tokenBytes, tokenLength) \
    if (traceExprComp) { \
	fprintf(stderr, "CompileSubExpr: \"%.*s\", token \"%.*s\"\n", \
		(exprLength), (exprBytes), (tokenLength), (tokenBytes)); \
    }
#else
#define TRACE(exprBytes, exprLength, tokenBytes, tokenLength)
#endif /* TCL_COMPILE_DEBUG */

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
    CONST char *script,		/* The source script to compile. */
    int numBytes,		/* Number of bytes in script. If < 0, the
				 * string consists of all bytes up to the
				 * first null character. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Parse parse;
    int needsNumConversion = 1;

    /*
     * If this is the first time we've been called, initialize the table of
     * expression operators.
     */

    if (numBytes < 0) {
	numBytes = (script? strlen(script) : 0);
    }
    if (!opTableInitialized) {
	Tcl_MutexLock(&opMutex);
	if (!opTableInitialized) {
	    int i;
	    Tcl_InitHashTable(&opHashTable, TCL_STRING_KEYS);
	    for (i = 0;  operatorTable[i].name != NULL;  i++) {
		int new;
		Tcl_HashEntry *hPtr = Tcl_CreateHashEntry(&opHashTable,
			operatorTable[i].name, &new);
		if (new) {
		    Tcl_SetHashValue(hPtr, (ClientData) INT2PTR(i));
		}
	    }
	    opTableInitialized = 1;
	}
	Tcl_MutexUnlock(&opMutex);
    }

    /*
     * Parse the expression then compile it.
     */

    if (TCL_OK != Tcl_ParseExpr(interp, script, numBytes, &parse)) {
	return TCL_ERROR;
    }
    CompileSubExpr(interp, parse.tokenPtr, &needsNumConversion, envPtr);

    if (needsNumConversion) {
	/*
	 * Attempt to convert the primary's object to an int or double.  This
	 * is done in order to support Tcl's policy of interpreting operands
	 * if at all possible as first integers, else floating-point numbers.
	 */

	TclEmitOpcode(INST_TRY_CVT_TO_NUMERIC, envPtr);
    }
    Tcl_FreeParse(&parse);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeCompilation --
 *
 *	Clean up the compilation environment so it can later be properly
 *	reinitialized. This procedure is called by Tcl_Finalize().
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Cleans up the compilation environment. At the moment, just the table
 *	of expression operators is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeCompilation(void)
{
    Tcl_MutexLock(&opMutex);
    if (opTableInitialized) {
	Tcl_DeleteHashTable(&opHashTable);
	opTableInitialized = 0;
    }
    Tcl_MutexUnlock(&opMutex);
}

/*
 *----------------------------------------------------------------------
 *
 * CompileSubExpr --
 *
 *	Given a pointer to a TCL_TOKEN_SUB_EXPR token describing a
 *	subexpression, this procedure emits instructions to evaluate the
 *	subexpression at runtime.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the subexpression.
 *
 *----------------------------------------------------------------------
 */

static void
CompileSubExpr(
    Tcl_Interp *interp,		/* Interp in which to compile expression */
    Tcl_Token *exprTokenPtr,	/* Points to TCL_TOKEN_SUB_EXPR token to
				 * compile. */
    int *convertPtr,		/* Writes 0 here if it is determined the
				 * final INST_TRY_CVT_TO_NUMERIC is
				 * not needed */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /* Switch on the type of the first token after the subexpression token. */
    Tcl_Token *tokenPtr = exprTokenPtr+1;
    TRACE(exprTokenPtr->start, exprTokenPtr->size,
	    tokenPtr->start, tokenPtr->size);
    switch (tokenPtr->type) {
    case TCL_TOKEN_WORD:
	TclCompileTokens(interp, tokenPtr+1, tokenPtr->numComponents, envPtr);
	break;

    case TCL_TOKEN_TEXT:
	TclEmitPush(TclRegisterNewLiteral(envPtr,
		tokenPtr->start, tokenPtr->size), envPtr);
	break;

    case TCL_TOKEN_BS: {
	char buffer[TCL_UTF_MAX];
	int length = Tcl_UtfBackslash(tokenPtr->start, NULL, buffer);
	TclEmitPush(TclRegisterNewLiteral(envPtr, buffer, length), envPtr);
	break;
    }

    case TCL_TOKEN_COMMAND:
	TclCompileScript(interp, tokenPtr->start+1, tokenPtr->size-2, envPtr);
	break;

    case TCL_TOKEN_VARIABLE:
	TclCompileTokens(interp, tokenPtr, 1, envPtr);
	break;

    case TCL_TOKEN_SUB_EXPR:
	CompileSubExpr(interp, tokenPtr, convertPtr, envPtr);
	break;

    case TCL_TOKEN_OPERATOR: {
	/*
	 * Look up the operator.  If the operator isn't found, treat it as a
	 * math function.
	 */
	OperatorDesc *opDescPtr;
	Tcl_HashEntry *hPtr;
	CONST char *operator;
	Tcl_DString opBuf;
	int opIndex;

	Tcl_DStringInit(&opBuf);
	operator = Tcl_DStringAppend(&opBuf, tokenPtr->start, tokenPtr->size);
	hPtr = Tcl_FindHashEntry(&opHashTable, operator);
	if (hPtr == NULL) {
	    CompileMathFuncCall(interp, exprTokenPtr, operator, envPtr);
	    Tcl_DStringFree(&opBuf);
	    break;
	}
	Tcl_DStringFree(&opBuf);
	opIndex = PTR2INT(Tcl_GetHashValue(hPtr));
	opDescPtr = &(operatorTable[opIndex]);

	/*
	 * If the operator is "normal", compile it using information from the
	 * operator table.
	 */

	if (opDescPtr->numOperands > 0) {
	    tokenPtr++;
	    CompileSubExpr(interp, tokenPtr, convertPtr, envPtr);
	    tokenPtr += (tokenPtr->numComponents + 1);

	    if (opDescPtr->numOperands == 2) {
		CompileSubExpr(interp, tokenPtr, convertPtr, envPtr);
	    }
	    TclEmitOpcode(opDescPtr->instruction, envPtr);
	    *convertPtr = 0;
	    break;
	}

	/*
	 * The operator requires special treatment, and is either "+" or "-",
	 * or one of "&&", "||" or "?".
	 */

	switch (opIndex) {
	case OP_PLUS:
	case OP_MINUS: {
	    Tcl_Token *afterSubexprPtr = exprTokenPtr
		    + exprTokenPtr->numComponents+1;
	    tokenPtr++;
	    CompileSubExpr(interp, tokenPtr, convertPtr, envPtr);
	    tokenPtr += (tokenPtr->numComponents + 1);

	    /*
	     * Check whether the "+" or "-" is unary.
	     */

	    if (tokenPtr == afterSubexprPtr) {
		TclEmitOpcode(((opIndex==OP_PLUS)? INST_UPLUS : INST_UMINUS),
			envPtr);
		break;
	    }

	    /*
	     * The "+" or "-" is binary.
	     */

	    CompileSubExpr(interp, tokenPtr, convertPtr, envPtr);
	    TclEmitOpcode(((opIndex==OP_PLUS)? INST_ADD : INST_SUB), envPtr);
	    *convertPtr = 0;
	    break;
	}

	case OP_LAND:
	case OP_LOR:
	    CompileLandOrLorExpr(interp, exprTokenPtr, opIndex, envPtr);
	    *convertPtr = 0;
	    break;

	case OP_QUESTY:
	    CompileCondExpr(interp, exprTokenPtr, convertPtr, envPtr);
	    break;

	default:
	    Tcl_Panic("CompileSubExpr: unexpected operator %d "
		    "requiring special treatment", opIndex);
	} /* end switch on operator requiring special treatment */
	break;

    }

    default:
	Tcl_Panic("CompileSubExpr: unexpected token type %d", tokenPtr->type);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CompileLandOrLorExpr --
 *
 *	This procedure compiles a Tcl logical and ("&&") or logical or ("||")
 *	subexpression.
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
CompileLandOrLorExpr(
    Tcl_Interp *interp,		/* Interp in which compile takes place */
    Tcl_Token *exprTokenPtr,	/* Points to TCL_TOKEN_SUB_EXPR token
				 * containing the "&&" or "||" operator. */
    int opIndex,		/* A code describing the expression operator:
				 * either OP_LAND or OP_LOR. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    JumpFixup shortCircuitFixup;/* Used to fix up the short circuit jump after
				 * the first subexpression. */
    JumpFixup shortCircuitFixup2;
				/* Used to fix up the second jump to the
				 * short-circuit target. */
    JumpFixup endFixup;		/* Used to fix up jump to the end. */
    int convert = 0;
    int savedStackDepth = envPtr->currStackDepth;
    Tcl_Token *tokenPtr = exprTokenPtr+2;

    /*
     * Emit code for the first operand.
     */

    CompileSubExpr(interp, tokenPtr, &convert, envPtr);
    tokenPtr += (tokenPtr->numComponents + 1);

    /*
     * Emit the short-circuit jump.
     */

    TclEmitForwardJump(envPtr,
	    ((opIndex==OP_LAND)? TCL_FALSE_JUMP : TCL_TRUE_JUMP),
	    &shortCircuitFixup);

    /*
     * Emit code for the second operand.
     */

    CompileSubExpr(interp, tokenPtr, &convert, envPtr);

    /*
     * The result is the boolean value of the second operand. We code this in
     * a somewhat contorted manner to be able to reuse the shortCircuit value
     * and save one INST_JUMP.
     */

    TclEmitForwardJump(envPtr,
	    ((opIndex==OP_LAND)? TCL_FALSE_JUMP : TCL_TRUE_JUMP),
	    &shortCircuitFixup2);

    if (opIndex == OP_LAND) {
	TclEmitPush(TclRegisterNewLiteral(envPtr, "1", 1), envPtr);
    } else {
	TclEmitPush(TclRegisterNewLiteral(envPtr, "0", 1), envPtr);
    }
    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &endFixup);

    /*
     * Fixup the short-circuit jumps and push the shortCircuit value.  Note
     * that shortCircuitFixup2 is always a short jump.
     */

    TclFixupForwardJumpToHere(envPtr, &shortCircuitFixup2, 127);
    if (TclFixupForwardJumpToHere(envPtr, &shortCircuitFixup, 127)) {
	/*
	 * shortCircuit jump grown by 3 bytes: update endFixup.
	 */

	 endFixup.codeOffset += 3;
    }

    if (opIndex == OP_LAND) {
	TclEmitPush(TclRegisterNewLiteral(envPtr, "0", 1), envPtr);
    } else {
	TclEmitPush(TclRegisterNewLiteral(envPtr, "1", 1), envPtr);
    }

    TclFixupForwardJumpToHere(envPtr, &endFixup, 127);
    envPtr->currStackDepth = savedStackDepth + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileCondExpr --
 *
 *	This procedure compiles a Tcl conditional expression:
 *	condExpr ::= lorExpr ['?' condExpr ':' condExpr]
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
CompileCondExpr(
    Tcl_Interp *interp,		/* Interp in which compile takes place */
    Tcl_Token *exprTokenPtr,	/* Points to TCL_TOKEN_SUB_EXPR token
				 * containing the "?" operator. */
    int *convertPtr,		/* Describes the compilation state for the
				 * expression being compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    JumpFixup jumpAroundThenFixup, jumpAroundElseFixup;
				/* Used to update or replace one-byte jumps
				 * around the then and else expressions when
				 * their target PCs are determined. */
    Tcl_Token *tokenPtr = exprTokenPtr+2;
    int elseCodeOffset, dist, convert = 0;
    int convertThen = 1, convertElse = 1;
    int savedStackDepth = envPtr->currStackDepth;

    /*
     * Emit code for the test.
     */

    CompileSubExpr(interp, tokenPtr, &convert, envPtr);
    tokenPtr += (tokenPtr->numComponents + 1);

    /*
     * Emit the jump to the "else" expression if the test was false.
     */

    TclEmitForwardJump(envPtr, TCL_FALSE_JUMP, &jumpAroundThenFixup);

    /*
     * Compile the "then" expression. Note that if a subexpression is only a
     * primary, we need to try to convert it to numeric. We do this to support
     * Tcl's policy of interpreting operands if at all possible as first
     * integers, else floating-point numbers.
     */

    CompileSubExpr(interp, tokenPtr, &convertThen, envPtr);
    tokenPtr += (tokenPtr->numComponents + 1);

    /*
     * Emit an unconditional jump around the "else" condExpr.
     */

    TclEmitForwardJump(envPtr, TCL_UNCONDITIONAL_JUMP, &jumpAroundElseFixup);

    /*
     * Compile the "else" expression.
     */

    envPtr->currStackDepth = savedStackDepth;
    elseCodeOffset = (envPtr->codeNext - envPtr->codeStart);
    CompileSubExpr(interp, tokenPtr, &convertElse, envPtr);

    /*
     * Fix up the second jump around the "else" expression.
     */

    dist = (envPtr->codeNext - envPtr->codeStart)
	    - jumpAroundElseFixup.codeOffset;
    if (TclFixupForwardJump(envPtr, &jumpAroundElseFixup, dist, 127)) {
	/*
	 * Update the else expression's starting code offset since it moved
	 * down 3 bytes too.
	 */

	elseCodeOffset += 3;
    }

    /*
     * Fix up the first jump to the "else" expression if the test was false.
     */

    dist = (elseCodeOffset - jumpAroundThenFixup.codeOffset);
    TclFixupForwardJump(envPtr, &jumpAroundThenFixup, dist, 127);
    *convertPtr = convertThen || convertElse;

    envPtr->currStackDepth = savedStackDepth + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * CompileMathFuncCall --
 *
 *	This procedure compiles a call on a math function in an expression:
 *	mathFuncCall ::= funcName '(' [condExpr {',' condExpr}] ')'
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds instructions to envPtr to evaluate the math function at
 *	runtime.
 *
 *----------------------------------------------------------------------
 */

static void
CompileMathFuncCall(
    Tcl_Interp *interp,		/* Interp in which compile takes place */
    Tcl_Token *exprTokenPtr,	/* Points to TCL_TOKEN_SUB_EXPR token
				 * containing the math function call. */
    CONST char *funcName,	/* Name of the math function. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_DString cmdName;
    int objIndex;
    Tcl_Token *tokenPtr, *afterSubexprPtr;
    int argCount;

    /*
     * Prepend "tcl::mathfunc::" to the function name, to produce the name of
     * a command that evaluates the function.  Push that command name on the
     * stack, in a literal registered to the namespace so that resolution can
     * be cached.
     */

    Tcl_DStringInit(&cmdName);
    Tcl_DStringAppend(&cmdName, "tcl::mathfunc::", -1);
    Tcl_DStringAppend(&cmdName, funcName, -1);
    objIndex = TclRegisterNewNSLiteral(envPtr, Tcl_DStringValue(&cmdName),
	    Tcl_DStringLength(&cmdName));
    TclEmitPush(objIndex, envPtr);
    Tcl_DStringFree(&cmdName);

    /*
     * Compile any arguments for the function.
     */

    argCount = 1;
    tokenPtr = exprTokenPtr+2;
    afterSubexprPtr = exprTokenPtr + (exprTokenPtr->numComponents + 1);
    while (tokenPtr != afterSubexprPtr) {
	int convert = 0;
	++argCount;
	CompileSubExpr(interp, tokenPtr, &convert, envPtr);
	tokenPtr += (tokenPtr->numComponents + 1);
    }

    /* Invoke the function */

    if (argCount < 255) {
	TclEmitInstInt1(INST_INVOKE_STK1, argCount, envPtr);
    } else {
	TclEmitInstInt4(INST_INVOKE_STK4, argCount, envPtr);
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
