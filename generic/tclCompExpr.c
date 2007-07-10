/*
 * tclCompExpr.c --
 *
 *	This file contains the code to parse and compile Tcl expressions
 *	and implementations of the Tcl commands corresponding to expression
 *	operators, such as the command ::tcl::mathop::+ .
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 * Contributions from Don Porter, NIST, 2006.  (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclCompExpr.c,v 1.53.2.6 2007/07/10 21:42:55 dgp Exp $
 */

#include "tclInt.h"
#include "tclCompile.h"		/* CompileEnv */

/*
 * Expression parsing takes place in the routine ParseExpr().  It takes a
 * string as input, parses that string, and generates a representation of
 * the expression in the form of a tree of operators, a list of literals,
 * a list of function names, and an array of Tcl_Token's within a Tcl_Parse
 * struct.  The tree is composed of OpNodes.
 */

typedef struct OpNode {
    int left;			/* "Pointer" to the left operand. */
    int right;			/* "Pointer" to the right operand. */
    int parent;			/* "Pointer" to the parent operand. */
    unsigned char lexeme;	/* Code that identifies the operator. */
    unsigned char precedence;	/* Precedence of the operator */
} OpNode;

/*
 * The storage for the tree is dynamically allocated array of OpNodes.  The
 * array is grown as parsing needs dictate according to a scheme similar to
 * Tcl's string growth algorithm, so that the resizing costs are O(N) and so
 * that we use at least half the memory allocated as expressions get large.
 *
 * Each OpNode in the tree represents an operator in the expression, either
 * unary or binary.  When parsing is completed successfully, a binary operator
 * OpNode will have its left and right fields filled with "pointers" to its
 * left and right operands.  A unary operator OpNode will have its right field
 * filled with a pointer to its single operand.  When an operand is a
 * subexpression the "pointer" takes the form of the index -- a non-negative
 * integer -- into the OpNode storage array where the root of that
 * subexpression parse tree is found.  
 *
 * Non-operator elements of the expression do not get stored in the OpNode
 * tree.  They are stored in the other structures according to their type.
 * Literal values get appended to the literal list.  Elements that denote
 * forms of quoting or substitution known to the Tcl parser get stored as
 * Tcl_Tokens.  These non-operator elements of the expression are the
 * leaves of the completed parse tree.  When an operand of an OpNode is
 * one of these leaf elements, the following negative integer codes are used
 * to indicate which kind of elements it is.
 */

enum OperandTypes {
    OT_NONE = -4,	/* Operand not yet (or no longer) known */
    OT_LITERAL = -3,	/* Operand is a literal in the literal list */
    OT_TOKENS = -2,	/* Operand is sequence of Tcl_Tokens */
    OT_EMPTY = -1	/* "Operand" is an empty string.  This is a
			 * special case used only to represent the
			 * EMPTY lexeme.  See below. */
};

/*
 * Readable macros to test whether a "pointer" value points to an operator.
 * They operate on the "non-negative integer -> operator; negative integer ->
 * a non-operator OperandType" distinction.
 */

#define IsOperator(l)	((l) >= 0)
#define NotOperator(l)	((l) < 0)

/*
 * Note that it is sufficient to store in the tree just the type of leaf
 * operand, without any explicit pointer to which leaf.  This is true because
 * the inorder traversals of the completed tree we perform are known to visit
 * the leaves in the same order as the original parse.
 *
 * Those OpNodes that are themselves (roots of subexpression trees that are)
 * operands of some operator store in their parent field a "pointer" to the
 * OpNode of that operator.  The parent field permits a destructive inorder
 * traversal of the tree within a non-recursive routine (ConvertTreeToTokens()
 * and CompileExprTree()).  This means that even expression trees of great
 * depth pose no risk of blowing the C stack.
 *
 * The lexeme field is filled in with the lexeme of the operator that is
 * returned by the ParseLexeme() routine.  Only lexemes for unary and
 * binary operators get stored in an OpNode.  Other lexmes get different
 * treatement.
 *
 * Each lexeme belongs to one of four categories, which determine
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
#define SCRIPT		( LEAF | 2)	/* Script substitution; [foo] */
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
#define CLOSE_PAREN	( BINARY | 27)	/* By categorizing the CLOSE_PAREN
					 * lexeme as a BINARY operator, the
					 * normal parsing rules for binary
					 * operators assure that a close paren
					 * will not directly follow another
					 * operator, and the machinery already
					 * in place to connect operands to
					 * operators according to precedence
					 * performs most of the work of
					 * matching open and close parens for
					 * us.  In the end though, a close
					 * paren is not really a binary
					 * operator, and some special coding
					 * in ParseExpr() make sure we never
					 * put an actual CLOSE_PAREN node
					 * in the parse tree.   The
					 * sub-expression between parens
					 * becomes the single argument of
					 * the matching OPEN_PAREN unary
					 * operator. */
#define END		( BINARY | 28)	/* This lexeme represents the end of
					 * the string being parsed.  Treating
					 * it as a binary operator follows the
					 * same logic as the CLOSE_PAREN lexeme
					 * and END pairs with START, in the
					 * same way that CLOSE_PAREN pairs with
					 * OPEN_PAREN. */
/*
 * When ParseExpr() builds the parse tree it must choose which operands to
 * connect to which operators.  This is done according to operator precedence.
 * The greater an operator's precedence the greater claim it has to link to
 * an available operand.  The Precedence enumeration lists the precedence
 * values used by Tcl expression operators, from lowest to highest claim.
 * Each precedence level is commented with the operators that hold that
 * precedence.
 */

enum Precedence {
    PREC_END = 1,	/* END */
    PREC_START,		/* START */
    PREC_CLOSE_PAREN,	/* ")" */
    PREC_OPEN_PAREN,	/* "(" */
    PREC_COMMA,		/* "," */
    PREC_CONDITIONAL,	/* "?", ":" */
    PREC_OR,		/* "||" */
    PREC_AND,		/* "&&" */
    PREC_BIT_OR,	/* "|" */
    PREC_BIT_XOR,	/* "^" */
    PREC_BIT_AND,	/* "&" */
    PREC_EQUAL,		/* "==", "!=", "eq", "ne", "in", "ni" */
    PREC_COMPARE,	/* "<", ">", "<=", ">=" */
    PREC_SHIFT,		/* "<<", ">>" */
    PREC_ADD,		/* "+", "-" */
    PREC_MULT,		/* "*", "/", "%" */
    PREC_EXPON,		/* "**" */
    PREC_UNARY		/* "+", "-", FUNCTION, "!", "~" */
};

/*
 * Here the same information contained in the comments above is stored
 * in inverted form, so that given a lexeme, one can quickly look up 
 * its precedence value.
 */

static const unsigned char prec[] = {
    /* Non-operator lexemes */
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,
    /* Binary operator lexemes */
    PREC_ADD,		/* BINARY_PLUS */
    PREC_ADD,		/* BINARY_MINUS */
    PREC_COMMA,		/* COMMA */
    PREC_MULT,		/* MULT */
    PREC_MULT,		/* DIVIDE */
    PREC_MULT,		/* MOD */
    PREC_COMPARE,	/* LESS */
    PREC_COMPARE,	/* GREATER */
    PREC_BIT_AND,	/* BIT_AND */
    PREC_BIT_XOR,	/* BIT_XOR */
    PREC_BIT_OR,	/* BIT_OR */
    PREC_CONDITIONAL,	/* QUESTION */
    PREC_CONDITIONAL,	/* COLON */
    PREC_SHIFT,		/* LEFT_SHIFT */
    PREC_SHIFT,		/* RIGHT_SHIFT */
    PREC_COMPARE,	/* LEQ */
    PREC_COMPARE,	/* GEQ */
    PREC_EQUAL,		/* EQUAL */
    PREC_EQUAL,		/* NEQ */
    PREC_AND,		/* AND */
    PREC_OR,		/* OR */
    PREC_EQUAL,		/* STREQ */
    PREC_EQUAL,		/* STRNEQ */
    PREC_EXPON,		/* EXPON */
    PREC_EQUAL,		/* IN_LIST */
    PREC_EQUAL,		/* NOT_IN_LIST */
    PREC_CLOSE_PAREN,	/* CLOSE_PAREN */
    PREC_END,		/* END */
    /* Expansion room for more binary operators */
    0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  
    /* Unary operator lexemes */
    PREC_UNARY,		/* UNARY_PLUS */
    PREC_UNARY,		/* UNARY_MINUS */
    PREC_UNARY,		/* FUNCTION */
    PREC_START,		/* START */
    PREC_OPEN_PAREN,	/* OPEN_PAREN */
    PREC_UNARY,		/* NOT*/
    PREC_UNARY,		/* BIT_NOT*/
    0,  0,  0,  0,  0,  0,  0,  0,
};

/*
 * The JumpList struct is used to create a stack of data needed for the
 * TclEmitForwardJump() and TclFixupForwardJump() calls that are performed
 * when compiling the short-circuiting operators QUESTION/COLON, AND, and OR.
 * Keeping a stack permits the CompileExprTree() routine to be non-recursive.
 */

typedef struct JumpList {
    JumpFixup jump;		/* Pass this argument to matching calls of
				 * TclEmitForwardJump() and 
				 * TclFixupForwardJump(). */
    int depth;			/* Remember the currStackDepth of the
				 * CompileEnv here. */
    int offset;			/* Data used to compute jump lengths to pass
				 * to TclFixupForwardJump() */
    int convert;		/* Temporary storage used to compute whether
				 * numeric conversion will be needed following
				 * the operator we're compiling. */
    struct JumpList *next;	/* Point to next item on the stack */
} JumpList;

/*
 * Declarations for local functions to this file:
 */

static void		CompileExprTree(Tcl_Interp *interp, OpNode *nodes,
			    Tcl_Obj *const litObjv[], Tcl_Obj *funcList,
			    Tcl_Token *tokenPtr, int *convertPtr,
			    CompileEnv *envPtr);
static void		ConvertTreeToTokens(Tcl_Interp *interp,
			    const char *start, int numBytes, OpNode *nodes,
			    Tcl_Obj *litList, Tcl_Token *tokenPtr,
			    Tcl_Parse *parsePtr);
static int		CopyTokens(Tcl_Token *sourcePtr, Tcl_Parse *parsePtr);
static int		GenerateTokensForLiteral(const char *script,
			    int numBytes, Tcl_Obj *litList, int nextLiteral,
			    Tcl_Parse *parsePtr);
static int		ParseExpr(Tcl_Interp *interp, const char *start,
			    int numBytes, OpNode **opTreePtr,
			    Tcl_Obj *litList, Tcl_Obj *funcList,
			    Tcl_Parse *parsePtr);
static int		ParseLexeme(const char *start, int numBytes,
			    unsigned char *lexemePtr, Tcl_Obj **literalPtr);


/*
 *----------------------------------------------------------------------
 *
 * ParseExpr --
 *
 *	Given a string, the numBytes bytes starting at start, this function
 *	parses it as a Tcl expression and constructs a tree representing
 *	the structure of the expression.  The caller must pass in empty
 * 	lists as the funcList and litList arguments.  The elements of the
 *	parsed expression are returned to the caller as that tree, a list of
 *	literal values, a list of function names, and in Tcl_Tokens
 *	added to a Tcl_Parse struct passed in by the caller.
 *
 * Results:
 *	If the string is successfully parsed as a valid Tcl expression, TCL_OK
 *	is returned, and data about the expression structure is written to
 *	the last four arguments.  If the string cannot be parsed as a valid
 *	Tcl expression, TCL_ERROR is returned, and if interp is non-NULL, an
 *	error message is written to interp.
 *
 * Side effects:
 *	Memory will be allocated.  If TCL_OK is returned, the caller must
 *	clean up the returned data structures.  The (OpNode *) value written
 *	to opTreePtr should be passed to ckfree() and the parsePtr argument
 *	should be passed to Tcl_FreeParse().  The elements appended to the
 *	litList and funcList will automatically be freed whenever the
 *	refcount on those lists indicates they can be freed.
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
    OpNode *nodes = NULL;	/* Pointer to the OpNode storage array where
				 * we build the parse tree. */
    int nodesAvailable = 64;	/* Initial size of the storage array.  This
				 * value establishes a minimum tree memory cost
				 * of only about 1 kibyte, and is large enough
				 * for most expressions to parse with no need
				 * for array growth and reallocation. */
    int nodesUsed = 0;		/* Number of OpNodes filled. */
    int code = TCL_OK;		/* Return code */
    int scanned = 0;		/* Capture number of byte scanned by 
				 * parsing routines. */

    /* These variables hold the state of the parser */
    unsigned char lexeme = START;	/* Most recent lexeme parsed. */
    int lastOpen = 0;		/* Index of the OpNode of the OPEN_PAREN
				 * operator we most recently matched. */
    int lastParsed = 0;		/* Stores info about what the lexeme parsed
				 * the previous pass through the parsing loop
				 * was.  If it was an operator, lastParsed is
				 * the index of the OpNode for that operator.
				 * If it was not and operator, lastParsed holds
				 * an OperandTypes value encoding what we
				 * need to know about it.  The initial value
				 * is 0 indicating that as we start the "last
				 * thing we parsed" was the START lexeme stored
				 * in node 0. */

    /* These variables control generation of the error message. */
    Tcl_Obj *msg = NULL;	/* The error message. */
    Tcl_Obj *post = NULL;	/* In a few cases, an additional postscript
				 * for the error message, supplying more
				 * information after the error msg and
				 * location have been reported. */
    const char *mark = "_@_";	/* In the portion of the complete error message
				 * where the error location is reported, this
				 * "mark" substring is inserted into the
				 * string being parsed to aid in pinpointing
				 * the location of the syntax error in the
				 * expression. */
    int insertMark = 0;		/* A boolean controlling whether the "mark"
				 * should be inserted. */
    const int limit = 25;	/* Portions of the error message are
				 * constructed out of substrings of the
				 * original expression.  In order to keep the
				 * error message readable, we impose this limit
				 * on the substring size we extract. */

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
	nodes->precedence = prec[lexeme];
	nodes->left = OT_NONE;
	nodes->right = OT_NONE;
	nodes->parent = -1;
	nodesUsed++;
    }

    while ((code == TCL_OK) && (lexeme != END)) {
	OpNode *nodePtr;	/* Points to the OpNode we may fill this
				 * pass through the loop. */
	Tcl_Obj *literal;	/* Filled by the ParseLexeme() call when
				 * a literal is parsed that has a Tcl_Obj
				 * rep worth preserving. */
	const char *lastStart = start - scanned;
				/* Compute where the lexeme parsed the
				 * previous pass through the loop began.
				 * This is helpful for detecting invalid
				 * octals and providing more complete error
				 * messages. */

	/*
	 * Each pass through this loop adds up to one more OpNode. Allocate
	 * space for one if required.
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
		/*
		 * Most barewords in an expression are a syntax error.
		 * The exceptions are that when a bareword is followed by
		 * an open paren, it might be a function call, and when the
		 * bareword is a legal literal boolean value, we accept that 
		 * as well.
		 */
		if (start[scanned+TclParseAllWhiteSpace(
			start+scanned, numBytes-scanned)] == '(') {
		    lexeme = FUNCTION;

		    /*
		     * When we compile the expression we'll need the function
		     * name, and there's no place in the parse tree to store
		     * it, so we keep a separate list of all the function
		     * names we've parsed in the order we found them.
		     */
		    Tcl_ListObjAppendElement(NULL, funcList, literal);
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
		if (IsOperator(lastParsed)) {
		    /*
		     * A "+" or "-" coming just after another operator
		     * must be interpreted as a unary operator.
		     */
		    lexeme |= UNARY;
		} else {
		    lexeme |= BINARY;
		}
	    }
	}

	/*
	 * Handle lexeme based on its category.
	 */

	switch (NODE_TYPE & lexeme) {

	/*
	 * Each LEAF results in either a literal getting appended to the
	 * litList, or a sequence of Tcl_Tokens representing a Tcl word
	 * getting appended to the parsePtr->tokens.  No OpNode is filled
	 * for this lexeme.
	 */

	case LEAF: {
	    Tcl_Token *tokenPtr;
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
		break;
	    default:
		break;
	    }

	    if (NotOperator(lastParsed)) {
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
		parsePtr->errorType = TCL_PARSE_BAD_NUMBER;
		code = TCL_ERROR;
		continue;
	    }

	    switch (lexeme) {
	    case NUMBER:
	    case BOOLEAN:
		lastParsed = OT_LITERAL;
		start += scanned;
		numBytes -= scanned;
		continue;
	    default:
		break;
	    }

	    /*
	     * Remaining LEAF cases may involve filling Tcl_Tokens, so
	     * make room for at least 2 more tokens.
	     */

	    if (parsePtr->numTokens+1 >= parsePtr->tokensAvailable) {
		TclExpandTokenArray(parsePtr);
	    }
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
		    /* TODO: This adjustment of scanned is untested and
		     * and uncommented.  Correct that.  Its only possible
		     * purpose is to influence the error message. */
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
		    /* TODO: This adjustment of scanned is untested and
		     * and uncommented.  Correct that.  Its only possible
		     * purpose is to influence the error message. */
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
		    /* TODO: This adjustment of scanned is untested and
		     * and uncommented.  Correct that.  Its only possible
		     * purpose is to influence the error message. */
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

		/*
		 * When a braced or quoted word within an expression
		 * is simple enough, we can store it as a literal rather
		 * than in its tokenized form.  This is an advantage since
		 * the compiled bytecode is going to need the argument in
		 * Tcl_Obj form eventually, so it's to our advantage to just
		 * get there now, and avoid the need to convert from Tcl_Token
		 * form again later.  Currently we only store literals
		 * for things parsed as single TEXT tokens (known as
		 * TCL_TOKEN_SIMPLE_WORD in other contexts).  In this
		 * simple case, the literal string we store is identical
		 * to a substring of the original expression.
		 *
		 * TODO: We ought to be able to store as a literal any
		 * word which is known at compile-time, including those that
		 * contain backslash substitution.  This can be helpful to
		 * store multi-line strings that include escaped newlines,
		 * or strings that include multi-byte characters expressed
		 * in \uHHHH form.  Removing the first two tests here is
		 * sufficient to make that change, but will lead to a
		 * Tcl_Panic() in GenerateTokensForLiteral() until that routine
		 * is revised to handle such literals.
		 */

		literal = Tcl_NewObj();
		if (tokenPtr->numComponents == 1
			&& tokenPtr[1].type == TCL_TOKEN_TEXT
			&& TclWordKnownAtCompileTime(tokenPtr, literal)) {
		    Tcl_ListObjAppendElement(NULL, litList, literal);
		    lastParsed = OT_LITERAL;
		    parsePtr->numTokens = wordIndex;
		    break;
		}
		Tcl_DecrRefCount(literal);
	    }
	    lastParsed = OT_TOKENS;
	    break;
	}

	case UNARY:
	    if (NotOperator(lastParsed)) {
		msg = Tcl_ObjPrintf("missing operator at %s", mark);
		scanned = 0;
		insertMark = 1;
		code = TCL_ERROR;
		continue;
	    }
	    lastParsed = nodesUsed;
	    nodePtr->lexeme = lexeme;
	    nodePtr->precedence = prec[lexeme];
	    nodePtr->left = OT_NONE;
	    nodePtr->right = OT_NONE;
	    nodePtr->parent = nodePtr - nodes - 1;
	    nodesUsed++;
	    break;

	case BINARY: {
	    OpNode *otherPtr = NULL;
	    unsigned char precedence = prec[lexeme];

	    if (IsOperator(lastParsed)) {
		if ((lexeme == CLOSE_PAREN)
			&& (nodePtr[-1].lexeme == OPEN_PAREN)) {
		    if (nodePtr[-2].lexeme == FUNCTION) {
			/*
			 * Normally, "()" is a syntax error, but as a special
			 * case accept it as an argument list for a function.
			 */

			scanned = 0;
			lastParsed = OT_EMPTY;
			nodePtr[-1].left--;
			break;
		    }
		    msg = Tcl_ObjPrintf("empty subexpression at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    code = TCL_ERROR;
		    continue;
		}

		if (nodePtr[-1].precedence > precedence) {
		    if (nodePtr[-1].lexeme == OPEN_PAREN) {
			TclNewLiteralStringObj(msg, "unbalanced open paren");
			parsePtr->errorType = TCL_PARSE_MISSING_PAREN;
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

	    if (lastParsed == OT_NONE) {
		otherPtr = nodes + lastOpen - 1;
		lastParsed = lastOpen;
	    } else {
		otherPtr = nodePtr - 1;
	    }
	    while (1) {
		/*
		 * lastParsed is "index" of item to be linked.
		 * otherPtr points to competing operator.
		 */

		if (otherPtr->precedence < precedence) {
		    break;
		}

		if (otherPtr->precedence == precedence) {
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

		    if ((otherPtr->lexeme == QUESTION)
			    && (NotOperator(lastParsed)
			    || (nodes[lastParsed].lexeme != COLON))) {
			break;
		    }
		    if ((otherPtr->lexeme == COLON) && (lexeme == QUESTION)) {
			break;
		    }
		}

		/*
		 * We should link the lastParsed item to the otherPtr as its
		 * right operand. First make some syntax checks.
		 */

		if ((otherPtr->lexeme == OPEN_PAREN)
			&& (lexeme != CLOSE_PAREN)) {
		    TclNewLiteralStringObj(msg, "unbalanced open paren");
		    parsePtr->errorType = TCL_PARSE_MISSING_PAREN;
		    code = TCL_ERROR;
		    break;
		}
		if ((otherPtr->lexeme == QUESTION)
			&& (NotOperator(lastParsed)
			|| (nodes[lastParsed].lexeme != COLON))) {
		    msg = Tcl_ObjPrintf(
			    "missing operator \":\" at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    code = TCL_ERROR;
		    break;
		}
		if (IsOperator(lastParsed)
			&& (nodes[lastParsed].lexeme == COLON)
			&& (otherPtr->lexeme != QUESTION)) {
		    TclNewLiteralStringObj(msg,
			    "unexpected operator \":\" without preceding \"?\"");
		    code = TCL_ERROR;
		    break;
		}

		/*
		 * Link orphan as right operand of otherPtr.
		 */

		otherPtr->right = lastParsed;
		if (lastParsed >= 0) {
		    nodes[lastParsed].parent = otherPtr - nodes;
		}
		lastParsed = otherPtr - nodes;

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
		lastParsed = OT_NONE;
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
	    if (IsOperator(lastParsed) && (nodes[lastParsed].lexeme == COLON)) {
		TclNewLiteralStringObj(msg,
			"unexpected operator \":\" without preceding \"?\"");
		code = TCL_ERROR;
		continue;
	    }
	    if (lexeme == END) {
		continue;
	    }

	    /*
	     * Link orphan as left operand of new node.
	     */

	    nodePtr->lexeme = lexeme;
	    nodePtr->precedence = precedence;
	    nodePtr->right = -1;
	    nodePtr->left = lastParsed;
	    if (lastParsed < 0) {
		nodePtr->parent = nodePtr - nodes - 1;
	    } else {
		nodePtr->parent = nodes[lastParsed].parent;
		nodes[lastParsed].parent = nodePtr - nodes;
	    }
	    lastParsed = nodesUsed;
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

    if (code != TCL_OK && parsePtr->errorType == TCL_PARSE_SUCCESS) {
	parsePtr->errorType = TCL_PARSE_SYNTAX;
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

    if (parsePtr->numTokens + 1 >= parsePtr->tokensAvailable) {
	TclExpandTokenArray(parsePtr);
    }
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
	while (parsePtr->numTokens + toCopy - 1 >= parsePtr->tokensAvailable) {
	    TclExpandTokenArray(parsePtr);
	}
	destPtr = parsePtr->tokenPtr + parsePtr->numTokens;
	memcpy(destPtr, sourcePtr, (size_t) toCopy * sizeof(Tcl_Token));
	destPtr->type = TCL_TOKEN_SUB_EXPR;
	parsePtr->numTokens += toCopy;
    } else {
	while (parsePtr->numTokens + toCopy >= parsePtr->tokensAvailable) {
	    TclExpandTokenArray(parsePtr);
	}
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
			if (parsePtr->numTokens + 1
				>= parsePtr->tokensAvailable) {
			    TclExpandTokenArray(parsePtr);
			}
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
		    if (parsePtr->numTokens + 1 >= parsePtr->tokensAvailable) {
			TclExpandTokenArray(parsePtr);
		    }
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
    int errorType = exprParsePtr->errorType;
    const char* term = exprParsePtr->term;

    if (numBytes < 0) {
	numBytes = (start ? strlen(start) : 0);
    }

    TclParseInit(interp, start, numBytes, parsePtr);
    if (code == TCL_OK) {
	ConvertTreeToTokens(interp, start, numBytes, opTree, litList,
		exprParsePtr->tokenPtr, parsePtr);
    } else {
	parsePtr->term = term;
	parsePtr->errorType = errorType;
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
