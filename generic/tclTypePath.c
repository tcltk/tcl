/*
 * tclTypePath.c --
 *
 *	Accès aux éléments des valeurs typées : $P(x), $poly(0:end.x),
 *	set P(x) 5, set L(1,2) a b (spécification §10).
 *
 *	Le langage d'index est celui de la page « A better way to do
 *	indexation » (opérateurs . , :), analysé et « compilé » sur le modèle
 *	de generic/tclCompExpr.c, comme tclTypeParse.c l'est pour les types :
 *
 *	  tclCompExpr.c            ici
 *	  ParseExpr                ParseIndex
 *	  ParseLexeme              ParseIndexLexeme
 *	  CompileExprTree          CompileIndexTree
 *	  CompileEnv               IndexEnv
 *	  table instruction[]      traitement de ApplyIndexOperator
 *
 *	OPÉRANDES (feuilles)
 *	  nom         mot nu : champ, clé de dict, nom de variante, ou index de
 *	              liste (entier, end, end-1). Le TYPE du conteneur tranche.
 *	  'clé'       toujours une clé (ne vaut jamais un index : 'end' est la
 *	              clé « end »)
 *	  "étiquette" copiée telle quelle dans le résultat ; les substitutions
 *	              y sont faites
 *	  $var        substitution de variable
 *	  [script]    script évalué, son résultat sert de nom
 *	  [( expr )]  substitution arithmétique (TIP 759) : l'expression est
 *	              évaluée ; une liste de plusieurs éléments donne autant
 *	              d'alternatives, comme (a,b,c)
 *
 *	OPÉRATEURS, du plus faible au plus fort
 *	  ,    collecte (binaire)       a,b
 *	  .    imbrication (binaire)    a.b      (a.(b,c) = a.b,a.c)
 *	  :    intervalle de liste      0:end-1
 *	  ( )  groupement
 *
 *	Les substitutions ($var, [script], [( )]) sont faites à la
 *	compilation du chemin, dans le contexte de l'appelant ; elles ne
 *	servent donc qu'à un chemin qui arrive non substitué (par exemple
 *	entre accolades). Dans $P($i) c'est déjà Tcl qui a substitué.
 *
 *	RÉSULTAT DE LA COMPILATION. L'équivalent du bytecode est un
 *	ENSEMBLE DE CHEMINS : chaque chemin est une suite de pas (nom, clé,
 *	intervalle, étiquette). "." fait le produit des chemins de ses
 *	opérandes, "," les réunit. La lecture applique chaque chemin au
 *	conteneur ; l'écriture exige des chemins de pas simples, un par
 *	valeur.
 *
 *	Limites : les résultats multiples sont aplatis (pas de niveaux
 *	imbriqués) ; une étiquette est un chemin à elle seule ; pas
 *	d'opérateurs arithmétiques sur les index ; écriture sans intervalles
 *	ni étiquettes ; pas de cache du vecteur de positions.
 */

#include "tclTypes.h"
#include "tclCompile.h"
#include <stdlib.h>

#ifndef TCL_CONTINUE
#define TCL_CONTINUE 4
#endif

#ifndef UCHAR
#define UCHAR(c)	((unsigned char) (c))
#endif

/* --------------------------------------------------------------------
 * L'arbre : OpNode, OperandTypes, Marks (comme tclCompExpr.c)
 * -------------------------------------------------------------------- */

typedef struct OpNode {
    int left;			/* "Pointeur" vers l'opérande gauche. */
    int right;			/* "Pointeur" vers l'opérande droit. */
    union {
	int parent;		/* "Pointeur" vers le parent. */
	int prev;		/* Chaîne des arbres incomplets (en cours). */
    };
    unsigned char lexeme;	/* Code de l'opérateur. */
    unsigned char precedence;	/* Précédence de l'opérateur. */
    unsigned char mark;		/* Marque de parcours. */
} OpNode;

/*
 * Un opérande qui n'est pas un sous-arbre est un code négatif. Le texte de
 * la feuille est dans leafList, dans l'ordre du texte source ; les
 * parcours visitent les feuilles dans cet ordre.
 */

enum OperandTypes {
    OT_ARITH = -7,		/* [( expr )] */
    OT_SCRIPT = -6,		/* [script] */
    OT_VARIABLE = -5,		/* $var */
    OT_TAG = -4,		/* "étiquette" */
    OT_KEY = -3,		/* 'clé' */
    OT_NAME = -2		/* nom nu */
};

#define IsOperator(l)	((l) >= 0)
#define NotOperator(l)	((l) < 0)

enum Marks {
    MARK_LEFT,			/* Prochain pas : visiter le sous-arbre gauche */
    MARK_RIGHT,			/* Prochain pas : visiter le sous-arbre droit */
    MARK_PARENT			/* Prochain pas : remonter au parent */
};

/* --------------------------------------------------------------------
 * Les lexèmes
 * -------------------------------------------------------------------- */

enum LexemeTypes {
    NODE_TYPE = 0xC0,
    BINARY = 0x40,
    UNARY = 0x80,
    LEAF = 0xC0
};

enum LexemeCodes {
    /* Lexèmes non catégorisés (erreurs) */

    INVALID = 5,		/* caractère non supporté */
    UNCLOSED = 7,		/* ' ou [ sans fermeture */

    /* Feuilles */

    NAMEWORD = LEAF | 1,
    KEYWORD = LEAF | 2,
    TAGWORD = LEAF | 3,
    VARIABLE = LEAF | 4,
    SCRIPT = LEAF | 5,
    ARITH = LEAF | 6,

    /* Unaires */

    START = UNARY | 4,
    OPEN_PAREN = UNARY | 5,

    /* Binaires */

    COMMA = BINARY | 3,
    DOT = BINARY | 4,
    RANGE = BINARY | 5,
    CLOSE_PAREN = BINARY | 6,
    END = BINARY | 7
};

enum Precedence {
    PREC_END = 1,
    PREC_START,
    PREC_CLOSE_PAREN,
    PREC_OPEN_PAREN,
    PREC_COMMA,
    PREC_DOT,
    PREC_RANGE
};

static const unsigned char prec[] = {
    [COMMA] = PREC_COMMA,
    [DOT] = PREC_DOT,
    [RANGE] = PREC_RANGE,
    [CLOSE_PAREN] = PREC_CLOSE_PAREN,
    [END] = PREC_END,
    [START] = PREC_START,
    [OPEN_PAREN] = PREC_OPEN_PAREN
};

/* Octets qui ne peuvent pas faire partie d'un nom nu. */

static int
IsDelimiter(int c)
{
    return c == ',' || c == '.' || c == ':' || c == '(' || c == ')'
	    || c == '[' || c == ']' || c == '$' || c == '"' || c == '\''
	    || c == '\\' || c == '{' || c == '}' || c == ' ' || c == '\t'
	    || c == '\n' || c == '\r';
}

/* --------------------------------------------------------------------
 * ParseIndexLexeme --
 *
 *	L'analogue de ParseLexeme : reconnaît le lexème au début de start et
 *	renvoie le nombre d'octets lus. Les feuilles « longues » ("...", $,
 *	[ ]) sont mesurées par ParseIndex avec les API du parseur de Tcl.
 * -------------------------------------------------------------------- */

static Tcl_Size
ParseIndexLexeme(
    const char *start,
    Tcl_Size numBytes,
    unsigned char *lexemePtr,
    Tcl_Obj **literalPtr)	/* texte d'un nom ou d'une 'clé' */
{
    const char *end, *limit = start + numBytes;

    *literalPtr = NULL;
    if (numBytes == 0) {
	*lexemePtr = END;
	return 0;
    }
    switch (*start) {
    case ',': *lexemePtr = COMMA; return 1;
    case '.': *lexemePtr = DOT; return 1;
    case ':': *lexemePtr = RANGE; return 1;
    case '(': *lexemePtr = OPEN_PAREN; return 1;
    case ')': *lexemePtr = CLOSE_PAREN; return 1;
    case '"': *lexemePtr = TAGWORD; return 1;
    case '$': *lexemePtr = VARIABLE; return 1;
    case '[': *lexemePtr = SCRIPT; return 1;
    case '\'':
	for (end = start + 1; end < limit && *end != '\''; end++) {
	    /* vide */
	}
	if (end >= limit) {
	    *lexemePtr = UNCLOSED;
	    return 1;
	}
	*lexemePtr = KEYWORD;
	*literalPtr = Tcl_NewStringObj(start + 1, end - start - 1);
	return end + 1 - start;
    }
    for (end = start; end < limit && !IsDelimiter(UCHAR(*end)); end++) {
	/* vide */
    }
    if (end == start) {
	*lexemePtr = INVALID;
	return 1;
    }
    *lexemePtr = NAMEWORD;
    *literalPtr = Tcl_NewStringObj(start, end - start);
    return end - start;
}

/* --------------------------------------------------------------------
 * "étiquette", $variable, [script] : lus avec les API du parseur de Tcl,
 * comme ParseExpr le fait pour QUOTED, VARIABLE et SCRIPT. Le texte est
 * conservé tel quel et substitué à la compilation du chemin.
 * -------------------------------------------------------------------- */

static int
ParseIndexQuoted(
    const char *start,		/* sur le guillemet */
    Tcl_Size numBytes,
    Tcl_Obj **valuePtr,		/* contenu entre guillemets (refcount 0) */
    Tcl_Size *scannedPtr)
{
    Tcl_Parse parse;
    const char *end = start;
    int code;

    TclParseInit(NULL, start, numBytes, &parse);
    TclGrowParseTokenArray(&parse, 2);
    parse.tokenPtr->type = TCL_TOKEN_WORD;
    parse.tokenPtr->start = start;
    parse.numTokens++;
    code = Tcl_ParseQuotedString(NULL, start, numBytes, &parse, 1, &end);
    Tcl_FreeParse(&parse);
    if (code != TCL_OK) {
	*scannedPtr = 1;
	return TCL_ERROR;
    }
    *scannedPtr = end - start;
    *valuePtr = Tcl_NewStringObj(start + 1, end - start - 2);
    return TCL_OK;
}

static int
ParseIndexVariable(
    const char *start,		/* sur le "$" */
    Tcl_Size numBytes,
    Tcl_Obj **valuePtr,		/* le texte source "$..." (refcount 0) */
    Tcl_Size *scannedPtr)
{
    Tcl_Parse parse;

    if (Tcl_ParseVarName(NULL, start, numBytes, &parse, 0) != TCL_OK
	    || parse.numTokens < 2
	    || parse.tokenPtr[0].type != TCL_TOKEN_VARIABLE) {
	Tcl_FreeParse(&parse);
	*scannedPtr = 1;
	return TCL_ERROR;
    }
    *scannedPtr = parse.tokenPtr[0].size;
    *valuePtr = Tcl_NewStringObj(start, parse.tokenPtr[0].size);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

/*
 * [script] : Tcl_ParseCommand en mode imbriqué, jusqu'au "]" apparié.
 * *arithPtr vaut 1 pour [( expr )] : le « script » est alors l'expression
 * parenthésée elle-même.
 */

static int
ParseIndexScript(
    const char *start,		/* sur le "[" */
    Tcl_Size numBytes,
    Tcl_Obj **valuePtr,		/* le texte entre crochets (refcount 0) */
    Tcl_Size *scannedPtr,
    int *arithPtr)
{
    const char *p = start + 1, *end = start + numBytes;

    while (p < end) {
	Tcl_Parse parse;
	int code, closed;

	code = Tcl_ParseCommand(NULL, p, end - p, 1, &parse);
	if (code != TCL_OK) {
	    *scannedPtr = 1;
	    Tcl_FreeParse(&parse);
	    return TCL_ERROR;
	}
	closed = (parse.term < end) && (*parse.term == ']');
	if (closed) {
	    const char *last = parse.term;

	    *valuePtr = Tcl_NewStringObj(start + 1, parse.term - start - 1);
	    *scannedPtr = parse.term + 1 - start;
	    while (last > start + 1 && (last[-1] == ' ' || last[-1] == '\t'
		    || last[-1] == '\n')) {
		last--;
	    }
	    *arithPtr = (start[1] == '(' && last > start + 1
		    && last[-1] == ')');
	    Tcl_FreeParse(&parse);
	    return TCL_OK;
	}
	p = parse.commandStart + parse.commandSize;
	Tcl_FreeParse(&parse);
    }
    *scannedPtr = 1;
    return TCL_ERROR;
}

static void
AppendIndexContext(
    Tcl_Obj *msg,
    const char *text,
    const char *textEnd,
    const char *start,
    Tcl_Size scanned,
    int insertMark,
    const char *mark,
    Tcl_Size limit)
{
    const char *from = ((start - text) > limit) ? start - limit + 3 : text;
    const char *after = start + scanned;
    const char *to = ((textEnd - after) > limit) ? after + limit - 3 : textEnd;

    if (scanned > limit) {
	scanned = limit - 3;
    }
    Tcl_AppendPrintfToObj(msg, "\nin index \"%s%.*s%.*s%s%.*s%s\"",
	    (from > text) ? "..." : "", (int) (start - from), from,
	    (int) scanned, start, insertMark ? mark : "",
	    (int) (to - after), after, (to < textEnd) ? "..." : "");
}

/* --------------------------------------------------------------------
 * ParseIndex --
 *
 *	Même algorithme que ParseExpr : analyse le texte d'un index et
 *	construit l'arbre d'opérateurs ; les textes des feuilles vont dans
 *	leafList, dans l'ordre du texte. L'appelant passe une liste vide.
 *
 * Résultat :
 *	TCL_OK avec *opTreePtr rempli (à libérer par Tcl_Free), ou TCL_ERROR
 *	et un message dans interp si interp != NULL.
 * -------------------------------------------------------------------- */

static int
ParseIndex(
    Tcl_Interp *interp,
    const char *start,
    Tcl_Size numBytes,
    OpNode **opTreePtr,
    Tcl_Obj *leafList)
{
    OpNode *nodes = NULL;
    unsigned int nodesAvailable = 64;
    unsigned int nodesUsed = 0;
    Tcl_Size scanned = 0;
    int lastParsed;		/* Dernier lexème : index d'opérateur ou code
				 * OperandTypes. */
    int incomplete;		/* Sommet de la pile des arbres incomplets. */
    int complete = OT_NAME;	/* Arbre complet courant. */
    const char *const text = start;
    const char *const textEnd = start + numBytes;
    Tcl_Obj *literal = NULL;
    Tcl_Obj *msg = NULL;
    const char *errCode = NULL;
    const char *const mark = "_@_";
    int insertMark = 0;
    const Tcl_Size limit = 25;

    nodes = (OpNode *) Tcl_AttemptAlloc(nodesAvailable * sizeof(OpNode));
    if (nodes == NULL) {
	msg = Tcl_ObjPrintf("not enough memory to parse index");
	errCode = "NOMEM";
	goto error;
    }

    /* Le noeud START, racine de l'arbre. */

    nodes->lexeme = START;
    nodes->precedence = prec[START];
    nodes->mark = MARK_RIGHT;
    incomplete = lastParsed = nodesUsed;
    nodesUsed++;

    while (1) {
	OpNode *nodePtr;
	unsigned char lexeme;

	if (nodesUsed >= nodesAvailable) {
	    unsigned int size = nodesUsed * 2;
	    OpNode *newPtr = (OpNode *) Tcl_AttemptRealloc(nodes,
		    size * sizeof(OpNode));

	    if (newPtr == NULL) {
		msg = Tcl_ObjPrintf("not enough memory to parse index");
		errCode = "NOMEM";
		goto error;
	    }
	    nodesAvailable = size;
	    nodes = newPtr;
	}
	nodePtr = nodes + nodesUsed;

	scanned = TclParseAllWhiteSpace(start, numBytes);
	start += scanned;
	numBytes -= scanned;

	literal = NULL;
	scanned = ParseIndexLexeme(start, numBytes, &lexeme, &literal);

	if ((NODE_TYPE & lexeme) == 0) {
	    switch (lexeme) {
	    case INVALID:
		msg = Tcl_ObjPrintf("invalid character \"%.*s\"",
			(int) scanned, start);
		errCode = "BADCHAR";
		goto error;
	    case UNCLOSED:
		msg = Tcl_ObjPrintf("missing close-quote");
		errCode = "UNBALANCED";
		goto error;
	    }
	}

	switch (NODE_TYPE & lexeme) {
	case LEAF: {
	    Tcl_Obj *value = NULL;
	    int arith = 0;

	    /* Une feuille juste après une feuille ou une ")" : erreur. */

	    if (NotOperator(lastParsed)) {
		msg = Tcl_ObjPrintf("missing operator at %s", mark);
		errCode = "MISSING";
		scanned = 0;
		insertMark = 1;
		goto error;
	    }

	    switch (lexeme) {
	    case NAMEWORD:
	    case KEYWORD:
		Tcl_ListObjAppendElement(NULL, leafList, literal);
		literal = NULL;
		complete = lastParsed = (lexeme == NAMEWORD) ? OT_NAME
			: OT_KEY;
		break;
	    case TAGWORD:
		if (ParseIndexQuoted(start, numBytes, &value, &scanned)
			!= TCL_OK) {
		    msg = Tcl_ObjPrintf("missing close-quote");
		    errCode = "UNBALANCED";
		    goto error;
		}
		Tcl_ListObjAppendElement(NULL, leafList, value);
		complete = lastParsed = OT_TAG;
		break;
	    case VARIABLE:
		if (ParseIndexVariable(start, numBytes, &value, &scanned)
			!= TCL_OK) {
		    msg = Tcl_ObjPrintf("invalid variable reference");
		    errCode = "VARIABLE";
		    goto error;
		}
		Tcl_ListObjAppendElement(NULL, leafList, value);
		complete = lastParsed = OT_VARIABLE;
		break;
	    case SCRIPT:
		if (ParseIndexScript(start, numBytes, &value, &scanned,
			&arith) != TCL_OK) {
		    msg = Tcl_ObjPrintf("missing close-bracket");
		    errCode = "UNBALANCED";
		    goto error;
		}
		Tcl_ListObjAppendElement(NULL, leafList, value);
		complete = lastParsed = arith ? OT_ARITH : OT_SCRIPT;
		break;
	    }
	    break;
	}

	case UNARY:
	    if (NotOperator(lastParsed)) {
		msg = Tcl_ObjPrintf("missing operator at %s", mark);
		errCode = "MISSING";
		scanned = 0;
		insertMark = 1;
		goto error;
	    }
	    nodePtr->lexeme = lexeme;
	    nodePtr->precedence = prec[lexeme];
	    nodePtr->mark = MARK_RIGHT;
	    nodePtr->prev = incomplete;
	    incomplete = lastParsed = nodesUsed;
	    nodesUsed++;
	    break;

	case BINARY: {
	    OpNode *incompletePtr;
	    unsigned char precedence = prec[lexeme];

	    /* Un binaire juste après un opérateur : il manque un opérande. */

	    if (IsOperator(lastParsed)) {
		if ((lexeme == CLOSE_PAREN)
			&& (nodePtr[-1].lexeme == OPEN_PAREN)) {
		    msg = Tcl_ObjPrintf("empty group at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    errCode = "EMPTY";
		    goto error;
		}
		if (nodePtr[-1].precedence > precedence) {
		    if (nodePtr[-1].lexeme == OPEN_PAREN) {
			msg = Tcl_ObjPrintf("unbalanced open parenthesis");
			errCode = "UNBALANCED";
		    } else if (nodePtr[-1].lexeme == START) {
			msg = Tcl_ObjPrintf("empty index expression");
			errCode = "EMPTY";
		    }
		} else if (lexeme == CLOSE_PAREN) {
		    msg = Tcl_ObjPrintf("unbalanced close parenthesis");
		    errCode = "UNBALANCED";
		}
		if (msg == NULL) {
		    msg = Tcl_ObjPrintf("missing operand at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    errCode = "MISSING";
		}
		goto error;
	    }

	    /*
	     * L'arbre se referme : selon les précédences, l'arbre complet
	     * devient l'opérande droit du dernier incomplet (et on
	     * recommence) ou l'opérande gauche du nouvel opérateur. Tous les
	     * binaires sont associatifs à gauche.
	     */

	    while (1) {
		incompletePtr = nodes + incomplete;

		if (incompletePtr->precedence < precedence) {
		    break;
		}

		/* Les parenthèses doivent s'apparier. */

		if ((incompletePtr->lexeme == OPEN_PAREN)
			&& (lexeme != CLOSE_PAREN)) {
		    msg = Tcl_ObjPrintf("unbalanced open parenthesis");
		    errCode = "UNBALANCED";
		    goto error;
		}

		incompletePtr->right = complete;
		if (IsOperator(complete)) {
		    nodes[complete].parent = incomplete;
		}

		if (incompletePtr->lexeme == START) {
		    *opTreePtr = nodes;
		    return TCL_OK;
		}

		complete = incomplete;
		incomplete = incompletePtr->prev;

		/* ")" ne referme qu'une "(". */
		if (incompletePtr->lexeme == OPEN_PAREN) {
		    break;
		}
	    }

	    if (lexeme == CLOSE_PAREN) {
		if (incompletePtr->lexeme != OPEN_PAREN) {
		    msg = Tcl_ObjPrintf("unbalanced close parenthesis");
		    errCode = "UNBALANCED";
		    goto error;
		}

		/* Aucun noeud pour ")". */
		break;
	    }

	    nodePtr->lexeme = lexeme;
	    nodePtr->precedence = precedence;
	    nodePtr->mark = MARK_LEFT;
	    nodePtr->left = complete;
	    if (IsOperator(complete)) {
		nodes[complete].parent = nodesUsed;
	    }

	    /* Opérande droit manquant : nouvel arbre incomplet. */

	    nodePtr->prev = incomplete;
	    incomplete = lastParsed = nodesUsed;
	    nodesUsed++;
	    break;
	}
	}

	start += scanned;
	numBytes -= scanned;
    }

  error:
    if (literal != NULL) {
	TclDiscard(literal);
    }
    if (nodes != NULL) {
	Tcl_Free(nodes);
    }
    if (interp == NULL) {
	if (msg != NULL) {
	    TclDiscard(msg);
	}
    } else {
	if (msg == NULL) {
	    msg = Tcl_ObjPrintf("syntax error in index");
	}
	AppendIndexContext(msg, text, textEnd, start, scanned, insertMark,
		mark, limit);
	Tcl_SetObjResult(interp, msg);
	if (errCode != NULL) {
	    Tcl_SetErrorCode(interp, "TCL", "PARSE", "INDEX", errCode,
		    (char *) NULL);
	}
    }
    return TCL_ERROR;
}

/* --------------------------------------------------------------------
 * Le résultat de la compilation : un ENSEMBLE DE CHEMINS
 *
 *	L'analogue de la pile d'évaluation du bytecode : les feuilles y
 *	poussent leur ensemble, chaque opérateur dépile ses opérandes et
 *	repousse son résultat.
 * -------------------------------------------------------------------- */

typedef enum { IS_NAME, IS_KEY, IS_RANGE, IS_TAG } StepKind;

typedef struct IndexStep {
    StepKind kind;
    Tcl_Obj *a;			/* nom, clé, début d'intervalle, étiquette */
    Tcl_Obj *b;			/* fin d'intervalle */
} IndexStep;

typedef struct IndexPath {
    IndexStep *step;
    Tcl_Size n;
} IndexPath;

typedef struct PathSet {
    IndexPath *path;
    Tcl_Size n;
    int multi;			/* résultat multiple : liste, même à 1 élément */
} PathSet;

typedef struct IndexEnv {
    Tcl_Interp *interp;
    PathSet *stack;
    Tcl_Size nStack, capStack;
} IndexEnv;

static void
PathFree(IndexPath *p)
{
    Tcl_Size i;

    for (i = 0; i < p->n; i++) {
	if (p->step[i].a != NULL) {
	    Tcl_DecrRefCount(p->step[i].a);
	}
	if (p->step[i].b != NULL) {
	    Tcl_DecrRefCount(p->step[i].b);
	}
    }
    if (p->step != NULL) {
	Tcl_Free(p->step);
    }
    p->step = NULL;
    p->n = 0;
}

static void
SetFree(PathSet *s)
{
    Tcl_Size i;

    for (i = 0; i < s->n; i++) {
	PathFree(&s->path[i]);
    }
    if (s->path != NULL) {
	Tcl_Free(s->path);
    }
    memset(s, 0, sizeof(*s));
}

static void
SetAddPath(PathSet *s, IndexPath p)
{
    s->path = (IndexPath *) Tcl_Realloc(s->path, (s->n + 1) * sizeof(IndexPath));
    s->path[s->n++] = p;
}

/* Un ensemble d'un seul chemin d'un seul pas. */

static PathSet
OneStep(StepKind kind, Tcl_Obj *a, Tcl_Obj *b)
{
    PathSet s;
    IndexPath p;

    memset(&s, 0, sizeof(s));
    p.n = 1;
    p.step = (IndexStep *) Tcl_Alloc(sizeof(IndexStep));
    p.step[0].kind = kind;
    p.step[0].a = a;
    p.step[0].b = b;
    if (a != NULL) {
	Tcl_IncrRefCount(a);
    }
    if (b != NULL) {
	Tcl_IncrRefCount(b);
    }
    SetAddPath(&s, p);
    return s;
}

static void
PushSet(IndexEnv *env, PathSet s)
{
    if (env->nStack == env->capStack) {
	env->capStack = env->capStack ? 2 * env->capStack : 16;
	env->stack = (PathSet *) Tcl_Realloc(env->stack,
		env->capStack * sizeof(PathSet));
    }
    env->stack[env->nStack++] = s;
}

static PathSet
PopSet(IndexEnv *env)
{
    return env->stack[--env->nStack];
}

static void
ClearStack(IndexEnv *env)
{
    while (env->nStack > 0) {
	PathSet s = PopSet(env);

	SetFree(&s);
    }
    if (env->stack != NULL) {
	Tcl_Free(env->stack);
	env->stack = NULL;
    }
    env->capStack = 0;
}

static void
PathErr(Tcl_Interp *interp, const char *fmt, const char *a, const char *b);

static int
HasTag(const IndexPath *p)
{
    Tcl_Size i;

    for (i = 0; i < p->n; i++) {
	if (p->step[i].kind == IS_TAG) {
	    return 1;
	}
    }
    return 0;
}

/* "." : produit des chemins (a.(b,c) = a.b,a.c ; (a,b).c = a.c,b.c). */

static int
ApplyDot(IndexEnv *env)
{
    PathSet r = PopSet(env), l = PopSet(env), out;
    Tcl_Size i, j, k;

    memset(&out, 0, sizeof(out));
    out.multi = l.multi || r.multi;
    for (i = 0; i < l.n; i++) {
	for (j = 0; j < r.n; j++) {
	    IndexPath p;

	    if (HasTag(&l.path[i]) || HasTag(&r.path[j])) {
		PathErr(env->interp, "a tag can't be indexed or used inside "
			"a path%s%s", "", "");
		SetFree(&l);
		SetFree(&r);
		SetFree(&out);
		return TCL_ERROR;
	    }
	    p.n = l.path[i].n + r.path[j].n;
	    p.step = (IndexStep *) Tcl_Alloc(p.n * sizeof(IndexStep));
	    for (k = 0; k < p.n; k++) {
		p.step[k] = (k < l.path[i].n) ? l.path[i].step[k]
			: r.path[j].step[k - l.path[i].n];
		if (p.step[k].a != NULL) {
		    Tcl_IncrRefCount(p.step[k].a);
		}
		if (p.step[k].b != NULL) {
		    Tcl_IncrRefCount(p.step[k].b);
		}
	    }
	    SetAddPath(&out, p);
	}
    }
    SetFree(&l);
    SetFree(&r);
    PushSet(env, out);
    return TCL_OK;
}

/* "," : réunion des chemins. */

static void
ApplyComma(IndexEnv *env)
{
    PathSet r = PopSet(env), l = PopSet(env);
    Tcl_Size i;

    l.path = (IndexPath *) Tcl_Realloc(l.path,
	    (l.n + r.n) * sizeof(IndexPath));
    for (i = 0; i < r.n; i++) {
	l.path[l.n++] = r.path[i];		/* la propriété passe à l */
    }
    r.n = 0;
    SetFree(&r);
    l.multi = 1;
    PushSet(env, l);
}

/* ":" : intervalle ; les deux bornes sont des index simples. */

static int
ApplyRange(IndexEnv *env)
{
    PathSet r = PopSet(env), l = PopSet(env), out;
    int ok = (l.n == 1 && r.n == 1 && l.path[0].n == 1 && r.path[0].n == 1
	    && l.path[0].step[0].kind <= IS_KEY
	    && r.path[0].step[0].kind <= IS_KEY);

    if (!ok) {
	PathErr(env->interp, "the bounds of a range must be single indices%s%s",
		"", "");
	SetFree(&l);
	SetFree(&r);
	return TCL_ERROR;
    }
    out = OneStep(IS_RANGE, l.path[0].step[0].a, r.path[0].step[0].a);
    out.multi = 1;
    SetFree(&l);
    SetFree(&r);
    PushSet(env, out);
    return TCL_OK;
}

static int
ApplyIndexOperator(IndexEnv *env, unsigned char lexeme)
{
    switch (lexeme) {
    case COMMA:
	ApplyComma(env);
	return TCL_OK;
    case DOT:
	return ApplyDot(env);
    case RANGE:
	return ApplyRange(env);
    default:			/* START, OPEN_PAREN */
	return TCL_OK;
    }
}

/*
 * Évaluation d'une feuille à substitution, à la compilation du chemin,
 * dans le contexte de l'appelant.
 */

static int
EvalLeaf(IndexEnv *env, int kind, Tcl_Obj *text)
{
    Tcl_Interp *interp = env->interp;
    Tcl_Obj *res = NULL;
    PathSet s;

    switch (kind) {
    case OT_NAME:
	s = OneStep(IS_NAME, text, NULL);
	break;
    case OT_KEY:
	s = OneStep(IS_KEY, text, NULL);
	break;
    case OT_TAG:
	res = Tcl_SubstObj(interp, text, TCL_SUBST_ALL);
	if (res == NULL) {
	    return TCL_ERROR;
	}
	Tcl_IncrRefCount(res);
	s = OneStep(IS_TAG, res, NULL);
	Tcl_DecrRefCount(res);
	break;
    case OT_VARIABLE:
	res = Tcl_SubstObj(interp, text, TCL_SUBST_VARIABLES);
	if (res == NULL) {
	    return TCL_ERROR;
	}
	Tcl_IncrRefCount(res);
	s = OneStep(IS_NAME, res, NULL);
	Tcl_DecrRefCount(res);
	break;
    case OT_SCRIPT:
	if (Tcl_EvalObjEx(interp, text, 0) != TCL_OK) {
	    return TCL_ERROR;
	}
	res = Tcl_NewStringObj(Tcl_GetString(Tcl_GetObjResult(interp)), -1);
	Tcl_IncrRefCount(res);
	s = OneStep(IS_NAME, res, NULL);
	Tcl_DecrRefCount(res);
	break;
    default: {			/* OT_ARITH : [( expr )], TIP 759 */
	Tcl_Obj **elems;
	Tcl_Size n, i;

	if (Tcl_ExprObj(interp, text, &res) != TCL_OK) {
	    return TCL_ERROR;
	}
	/* Une liste de plusieurs éléments : autant d'alternatives. */
	if (Tcl_ListObjGetElements(NULL, res, &n, &elems) != TCL_OK || n < 2) {
	    s = OneStep(IS_NAME, res, NULL);
	} else {
	    memset(&s, 0, sizeof(s));
	    for (i = 0; i < n; i++) {
		PathSet one = OneStep(IS_NAME, elems[i], NULL);

		SetAddPath(&s, one.path[0]);
		Tcl_Free(one.path);
	    }
	    s.multi = 1;
	}
	Tcl_DecrRefCount(res);
	break;
    }
    }
    PushSet(env, s);
    return TCL_OK;
}

/* --------------------------------------------------------------------
 * CompileIndexTree --
 *
 *	Même parcours que CompileExprTree : on descend à gauche puis à
 *	droite (marques MARK_LEFT, MARK_RIGHT), les feuilles poussent leur
 *	ensemble de chemins, et en remontant (MARK_PARENT) chaque opérateur
 *	applique son traitement. *leafObjvPtr avance dans leafList, dans
 *	l'ordre du texte.
 * -------------------------------------------------------------------- */

static int
CompileIndexTree(
    OpNode *nodes,
    Tcl_Size index,
    Tcl_Obj *const **leafObjvPtr,
    IndexEnv *envPtr)
{
    OpNode *nodePtr = nodes + index;
    OpNode *rootPtr = nodePtr;

    while (1) {
	int next;

	if (nodePtr->mark == MARK_LEFT) {
	    next = nodePtr->left;
	} else if (nodePtr->mark == MARK_RIGHT) {
	    next = nodePtr->right;
	} else {
	    /* Retour au parent : on applique l'opérateur. */

	    if (ApplyIndexOperator(envPtr, nodePtr->lexeme) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (nodePtr == rootPtr) {
		return TCL_OK;
	    }
	    nodePtr = nodes + nodePtr->parent;
	    continue;
	}

	nodePtr->mark++;
	switch (next) {
	case OT_NAME:
	case OT_KEY:
	case OT_TAG:
	case OT_VARIABLE:
	case OT_SCRIPT:
	case OT_ARITH:
	    if (EvalLeaf(envPtr, next, **leafObjvPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    (*leafObjvPtr)++;
	    break;
	default:
	    nodePtr = nodes + next;		/* sous-arbre : on descend */
	}
    }
}

/* --------------------------------------------------------------------
 * CompileIndex --
 *
 *	Analyse un index puis le « compile » : *setPtr reçoit l'ensemble de
 *	chemins (à libérer par SetFree). Équivalent de TclCompileExpr.
 * -------------------------------------------------------------------- */

static int
CompileIndex(
    Tcl_Interp *interp,
    const char *text,
    PathSet *setPtr)
{
    OpNode *opTree = NULL;
    Tcl_Obj *leafList;
    IndexEnv env;
    int code;

    memset(&env, 0, sizeof(env));
    env.interp = interp;
    memset(setPtr, 0, sizeof(*setPtr));
    TclNewObj(leafList);
    Tcl_IncrRefCount(leafList);

    code = ParseIndex(interp, text, strlen(text), &opTree, leafList);
    if (code == TCL_OK) {
	Tcl_Size objc;
	Tcl_Obj **objv;
	Tcl_Obj *const *lv;

	Tcl_ListObjGetElements(NULL, leafList, &objc, &objv);
	lv = objv;
	code = CompileIndexTree(opTree, 0, &lv, &env);
	if (code == TCL_OK) {
	    *setPtr = PopSet(&env);		/* la racine : un seul ensemble */
	}
	Tcl_Free(opTree);
    }
    ClearStack(&env);
    Tcl_DecrRefCount(leafList);
    return code;
}

/* --------------------------------------------------------------------
 * Un pas dans une valeur typée
 * -------------------------------------------------------------------- */

static void
PathErr(Tcl_Interp *interp, const char *fmt, const char *a, const char *b)
{
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(fmt, a, b));
    Tcl_SetErrorCode(interp, "TCL", "TYPE", "PATH", (char *) NULL);
}

/* Enveloppe le contenu d'une case dans son type (somme : variante déduite
 * par revalidation, la balise d'un élément imbriqué n'est pas conservée). */
static Tcl_Obj *
WrapSlot(Tcl_Interp *interp, TclType *tp, Tcl_Obj *content)
{
    int variant = -1, ownVariant = -1;
    TclType *ownType = NULL;
    Tcl_Obj *inner = NULL;

    /* list.any : l'élément garde sa propre annotation. */
    if (TclTypeIsAny(tp)
	    && TclAnnotInfo(content, &inner, &ownType, &ownVariant, NULL) == 1) {
	return TclNewAnnotated(interp, ownType, ownVariant, inner);
    }
    /* Un lien de frontière : l'adresse de la cible garde son annotation. */
    if (tp->binLink != NULL && tp->binLink->ref != NULL
	    && TclAnnotInfo(content, &inner, &ownType, &ownVariant, NULL) == 1
	    && ownType == tp->binLink->ref) {
	return TclNewAnnotated(interp, ownType, ownVariant, inner);
    }
    /* Un élément déjà annoté de ce type (balise explicite {i}5) garde sa
     * variante. */
    if (tp->kind == TCLT_SUM
	    && TclAnnotInfo(content, &inner, &ownType, &ownVariant, NULL) == 1
	    && ownType == tp && ownVariant >= 0) {
	return TclNewAnnotated(interp, tp, ownVariant, inner);
    }
    if (tp->kind == TCLT_SUM
	    && TclTypeValidate(interp, tp, content, -1, &variant) != TCL_OK) {
	return NULL;
    }
    return TclNewAnnotated(interp, tp, variant, content);
}

static int
IsIntegerName(const char *s)
{
    char *end;

    if (*s == '\0') {
	return 0;
    }
    (void) strtol(s, &end, 10);
    return *end == '\0';
}

/* Index de liste ou de produit (entier, end, end-N). */
static int
ListIndex(Tcl_Interp *interp, const char *name, Tcl_Size n, Tcl_Size *idxPtr)
{
    Tcl_Obj *nameObj = Tcl_NewStringObj(name, TCL_AUTO_LENGTH);
    int rc = TclGetIntForIndexM(interp, nameObj, n - 1, idxPtr);

    TclDiscard(nameObj);
    return rc;
}

/*
 * bitarray(n, ordre) : un indice i désigne un bit ; lsb0 : i = 0 est le bit
 * de droite, msb0 : i = 0 est le bit de gauche. BitsPos rend la position
 * correspondante depuis le poids faible ; BitsSlice transforme i:j en
 * (lo, len) depuis le poids faible.
 */
static int
BitsPos(Tcl_Interp *interp, const TclType *tp, const char *name,
	Tcl_Size *loPtr)
{
    Tcl_Size i;

    if (ListIndex(interp, name, tp->bitWidth, &i) != TCL_OK) {
	return TCL_ERROR;
    }
    if (i < 0 || i >= tp->bitWidth) {
	PathErr(interp, "bit index \"%s\" out of range for %s", name,
		tp->name);
	return TCL_ERROR;
    }
    *loPtr = tp->bitMsb0 ? tp->bitWidth - 1 - i : i;
    return TCL_OK;
}

static int
BitsSlice(Tcl_Interp *interp, const TclType *tp, const char *from,
	const char *to, Tcl_Size *loPtr, Tcl_Size *lenPtr)
{
    Tcl_Size i, j;

    if (ListIndex(interp, from, tp->bitWidth, &i) != TCL_OK
	    || ListIndex(interp, to, tp->bitWidth, &j) != TCL_OK) {
	return TCL_ERROR;
    }
    if (i < 0 || j >= tp->bitWidth || i > j) {
	PathErr(interp, "bit range \"%s:%s\" is empty or out of range",
		from, to);
	return TCL_ERROR;
    }
    *loPtr = tp->bitMsb0 ? tp->bitWidth - 1 - j : i;
    *lenPtr = j - i + 1;
    return TCL_OK;
}

/* Le type d'une tranche de len bits du même sens que tp. */
static TclType *
BitsSliceType(Tcl_Interp *interp, const TclType *tp, Tcl_Size len)
{
    char text[64];

    snprintf(text, sizeof(text), "bitarray(%lld,%s)", (long long) len,
	    tp->bitMsb0 ? "msb0" : "lsb0");
    return TclTypeParse(interp, text, (Tcl_Size) strlen(text));
}

/* --------------------------------------------------------------------
 * Liens var : une case de type var.T contient le NOM d'une variable. La
 * lecture traverse le lien (la variable est lue à chaque fois : la valeur
 * n'est jamais copiée dans le conteneur) ; l'écriture modifie la variable
 * et laisse le conteneur inchangé.
 * -------------------------------------------------------------------- */

/* Valeur courante de la variable visée, annotée de son type (celui de la
 * variable si elle est typée, sinon T après validation). Objet neuf. */
static Tcl_Obj *
DerefSlot(Tcl_Interp *interp, TclType *vt, Tcl_Obj *name)
{
    Tcl_Obj *val, *content;
    TclType *tp;
    int variant = -1;

    if (TclVarRefGet(interp, Tcl_GetString(name), &val) != TCL_OK) {
	return NULL;
    }
    if (TclAnnotInfo(val, &content, &tp, &variant, NULL) == 1) {
	if (!TclTypeIncludes(tp, vt->elem)) {
	    char msg[300];

	    snprintf(msg, sizeof(msg), "variable \"%s\" holds a %s, which is "
		    "not included in %s", Tcl_GetString(name), tp->name,
		    vt->elem->name);
	    PathErr(interp, "%s%s", msg, "");
	    return NULL;
	}
	return TclNewAnnotated(interp, tp, variant, content);
    }
    if (TclTypeValidate(interp, vt->elem, val, -1, &variant) != TCL_OK) {
	return NULL;
    }
    return TclNewAnnotated(interp, vt->elem, variant, val);
}

/*
 * type deref : remplace chaque lien var d'une valeur typée par la valeur
 * actuelle de la variable visée. Tout ou rien : la valeur d'origine n'est
 * jamais modifiée, et une variable manquante arrête tout avec un message
 * qui nomme la case. On ne suit pas les liens que contient la valeur d'une
 * variable visée (un second type deref le fait).
 */
static void
DerefWhere(Tcl_DString *path, Tcl_Size keep, const char *sep, const char *name)
{
    Tcl_DStringSetLength(path, keep);
    if (keep > 0) {
	Tcl_DStringAppend(path, sep, 1);
    }
    Tcl_DStringAppend(path, name, TCL_AUTO_LENGTH);
}

static Tcl_Obj *
DerefWalk(Tcl_Interp *interp, Tcl_Obj *a, Tcl_DString *path)
{
    Tcl_Obj *v, *newV = NULL, *res, *child, *r, *c;
    TclType *tp, *rt, *rtp;
    int variant;
    Tcl_Size n, i, keep = Tcl_DStringLength(path);
    char num[32];

    if (TclAnnotInfo(a, &v, &tp, &variant, NULL) != 1) {
	PathErr(interp, "type deref needs a typed value%s%s", "", "");
	return NULL;
    }
    if (!TclTypeHasLink(tp)) {
	return TclNewAnnotated(interp, tp, variant, v);
    }
    if (tp->kind == TCLT_VAR) {
	res = DerefSlot(interp, tp, v);
	if (res == NULL) {
	    /* ajoute la case au message de DerefSlot / TclVarRefGet */

	    Tcl_Obj *msg = Tcl_GetObjResult(interp), *full;

	    Tcl_IncrRefCount(msg);
	    full = Tcl_ObjPrintf("type deref: %s%s%s", keep > 0 ? "at " : "",
		    keep > 0 ? Tcl_DStringValue(path) : "",
		    keep > 0 ? ": " : "");
	    Tcl_AppendObjToObj(full, msg);
	    Tcl_DecrRefCount(msg);
	    Tcl_SetObjResult(interp, full);
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "DEREF", (char *) NULL);
	}
	return res;
    }
    rt = TclTypeDerefType(interp, tp);
    if (rt == NULL) {
	return NULL;
    }
    if (tp->kind == TCLT_SUM) {
	/* le contenu est celui de la variante en cours, dans le nouveau
	 * type : l'ordre des variantes a pu changer */

	TclType *ft;
	Tcl_Size j = -1;

	if (variant < 0 || variant >= tp->nfields) {
	    PathErr(interp, "type deref: the value of %s has no variant%s",
		    tp->name, "");
	    return NULL;
	}
	ft = tp->fields[variant].type;
	if (tp->fields[variant].name != NULL) {
	    j = TclTypeFieldIndex(rt, Tcl_GetString(tp->fields[variant].name));
	} else {
	    TclType *want = (ft != NULL) ? TclTypeDerefType(interp, ft) : NULL;

	    for (i = 0; i < rt->nfields; i++) {
		if (rt->fields[i].name == NULL && rt->fields[i].type == want
			&& (want != NULL || strcmp(
			Tcl_GetString(rt->fields[i].literal),
			Tcl_GetString(tp->fields[variant].literal)) == 0)) {
		    j = i;
		    break;
		}
	    }
	}
	if (j < 0) {
	    PathErr(interp, "type deref: can't find the variant of %s "
		    "after following the links%s", tp->name, "");
	    return NULL;
	}
	if (ft == NULL) {			/* variante littérale */
	    return TclNewAnnotated(interp, rt, (int) j, v);
	}
	DerefWhere(path, keep, ".", tp->fields[variant].name != NULL
		? Tcl_GetString(tp->fields[variant].name) : "?");
	child = WrapSlot(interp, ft, v);
	if (child == NULL) {
	    return NULL;
	}
	r = DerefWalk(interp, child, path);
	TclDiscard(child);
	Tcl_DStringSetLength(path, keep);
	if (r == NULL) {
	    return NULL;
	}
	TclAnnotInfo(r, &c, NULL, NULL, NULL);
	res = TclNewAnnotated(interp, rt, (int) j, c);
	TclDiscard(r);
	return res;
    }
    newV = (tp->kind == TCLT_DICT) ? Tcl_NewDictObj() : Tcl_NewListObj(0, NULL);
    switch (tp->kind) {
    case TCLT_LIST:
    case TCLT_PRODUCT: {
	Tcl_Obj **elems;

	if (Tcl_ListObjGetElements(interp, v, &n, &elems) != TCL_OK) {
	    goto fail;
	}
	for (i = 0; i < n; i++) {
	    TclType *slot;

	    if (tp->kind == TCLT_PRODUCT) {
		slot = tp->fields[i].type;
		if (tp->fields[i].name != NULL) {
		    snprintf(num, sizeof(num), "%.30s",
			    Tcl_GetString(tp->fields[i].name));
		} else {
		    snprintf(num, sizeof(num), "%lld", (long long) i);
		}
	    } else {
		slot = tp->elem;
		snprintf(num, sizeof(num), "%lld", (long long) i);
	    }
	    DerefWhere(path, keep, ".", num);
	    child = WrapSlot(interp, slot, elems[i]);
	    if (child == NULL) {
		goto fail;
	    }
	    r = DerefWalk(interp, child, path);
	    TclDiscard(child);
	    if (r == NULL) {
		goto fail;
	    }
	    TclAnnotInfo(r, &c, &rtp, NULL, NULL);
	    /* un élément de type somme garde son annotation (sa variante) */
	    Tcl_ListObjAppendElement(NULL, newV,
		    (rtp->kind == TCLT_SUM) ? r : c);
	    TclDiscard(r);
	}
	break;
    }
    case TCLT_DICT: {
	Tcl_DictSearch search;
	Tcl_Obj *k, *val;
	int done;

	if (Tcl_DictObjFirst(interp, v, &search, &k, &val, &done) != TCL_OK) {
	    goto fail;
	}
	for (; !done; Tcl_DictObjNext(&search, &k, &val, &done)) {
	    DerefWhere(path, keep, ".", Tcl_GetString(k));
	    child = WrapSlot(interp, tp->elem, val);
	    if (child == NULL) {
		Tcl_DictObjDone(&search);
		goto fail;
	    }
	    r = DerefWalk(interp, child, path);
	    TclDiscard(child);
	    if (r == NULL) {
		Tcl_DictObjDone(&search);
		goto fail;
	    }
	    TclAnnotInfo(r, &c, &rtp, NULL, NULL);
	    Tcl_DictObjPut(NULL, newV, k, (rtp->kind == TCLT_SUM) ? r : c);
	    TclDiscard(r);
	}
	Tcl_DictObjDone(&search);
	break;
    }
    default:
	PathErr(interp, "type deref: can't follow links inside %s%s",
		tp->name, "");
	goto fail;
    }
    Tcl_DStringSetLength(path, keep);
    res = TclNewAnnotated(interp, rt, variant, newV);
    TclDiscard(newV);
    return res;

  fail:
    Tcl_DStringSetLength(path, keep);
    TclDiscard(newV);
    return NULL;
}

int
TclValueDeref(Tcl_Interp *interp, Tcl_Obj *value, Tcl_Obj **resultPtr)
{
    Tcl_DString path;
    Tcl_Obj *r;

    Tcl_DStringInit(&path);
    r = DerefWalk(interp, value, &path);
    Tcl_DStringFree(&path);
    if (r == NULL) {
	return TCL_ERROR;
    }
    Tcl_IncrRefCount(r);
    *resultPtr = r;
    return TCL_OK;
}

/* Un lien ne se traverse (pas supplémentaire dans le chemin) que si sa case
 * est typée : var.T avec T autre que string, ou var(typed). Un var brut se
 * lit au bout d'un chemin mais ne se traverse pas. */
static int
LinkTraversable(Tcl_Interp *interp, TclType *vt, const char *name)
{
    TclType *s = TclTypeParse(interp, "string", 6);

    if (vt->varTyped || vt->elem != s) {
	return 1;
    }
    PathErr(interp, "can't go through the untyped link %s with \"%s\": "
	    "declare var.T or var(typed)", vt->name, name);
    return 0;
}

/* Affectations de variables différées : l'écriture par chemin est « tout
 * ou rien » pour le conteneur, puis on applique les liens. */
typedef struct PendingWrites {
    TclObjVec names, values;
} PendingWrites;

static int
QueueWrite(PendingWrites *pw, Tcl_Obj *name, Tcl_Obj *value)
{
    TclVecAdd(&pw->names, name);
    TclVecAdd(&pw->values, value);
    return TCL_OK;
}

static Tcl_Obj *
Step(Tcl_Interp *interp, Tcl_Obj *a, const char *name, int quoted)
{
    Tcl_Obj *v, *elem = NULL, *keyObj;
    TclType *tp;
    int variant;
    Tcl_Size idx, n;

    if (TclAnnotInfo(a, &v, &tp, &variant, NULL) != 1) {
	PathErr(interp, "can't index an untyped value with \"%s\"%s", name, "");
	return NULL;
    }
    if (tp->kind == TCLT_VAR) {
	/* le lien est transparent : on indexe la valeur de la variable */

	Tcl_Obj *target, *r;

	if (!LinkTraversable(interp, tp, name)) {
	    return NULL;
	}
	target = DerefSlot(interp, tp, v);
	if (target == NULL) {
	    return NULL;
	}
	r = Step(interp, target, name, quoted);
	TclDiscard(target);
	return r;
    }
    switch (tp->kind) {
    case TCLT_PRODUCT:
	idx = TclTypeFieldIndex(tp, name);
	if (idx < 0 && !quoted && IsIntegerName(name)) {
	    idx = atol(name);
	}
	if (idx < 0 || idx >= tp->nfields) {
	    PathErr(interp, "%s has no field \"%s\"", tp->name, name);
	    return NULL;
	}
	if (Tcl_ListObjIndex(interp, v, idx, &elem) != TCL_OK) {
	    return NULL;
	}
	return WrapSlot(interp, tp->fields[idx].type, elem);
    case TCLT_LIST:
	if (quoted) {
	    PathErr(interp, "a list can't be indexed with the key '%s'%s",
		    name, "");
	    return NULL;
	}
	if (Tcl_ListObjLength(interp, v, &n) != TCL_OK
		|| ListIndex(interp, name, n, &idx) != TCL_OK) {
	    return NULL;
	}
	if (idx < 0 || idx >= n) {
	    PathErr(interp, "list index \"%s\" out of range%s", name, "");
	    return NULL;
	}
	if (Tcl_ListObjIndex(interp, v, idx, &elem) != TCL_OK) {
	    return NULL;
	}
	return WrapSlot(interp, tp->elem, elem);
    case TCLT_DICT:
	if (!quoted && strcmp(name, "end") == 0) {
	    PathErr(interp, "\"end\" is a list index: write 'end' for the "
		    "dictionary key%s", "", "");
	    return NULL;
	}
	keyObj = Tcl_NewStringObj(name, TCL_AUTO_LENGTH);
	if (TclTypeValidate(interp, tp->key, keyObj, -1, NULL) != TCL_OK
		|| Tcl_DictObjGet(interp, v, keyObj, &elem) != TCL_OK) {
	    TclDiscard(keyObj);
	    return NULL;
	}
	TclDiscard(keyObj);
	if (elem == NULL) {
	    PathErr(interp, "key \"%s\" not known in dictionary%s", name, "");
	    return NULL;
	}
	return WrapSlot(interp, tp->elem, elem);
    case TCLT_SUM: {
	TclType *ft;
	Tcl_Obj *res;

	idx = TclTypeFieldIndex(tp, name);
	if (idx < 0 || tp->fields[idx].type == NULL) {
	    PathErr(interp, "%s has no variant \"%s\"", tp->name, name);
	    return NULL;
	}
	ft = tp->fields[idx].type;
	if (idx == variant) {
	    return TclNewAnnotated(interp, ft, -1, v);
	}
	/* Autre variante : le nom demandé vaut cast explicite (§10). */
	if (TclTypeCast(interp, a, ft, &res) != TCL_OK) {
	    return NULL;
	}
	return res;
    }
    case TCLT_INT:
	if (tp->bitWidth > 0) {
	    /* un bit : un entier 0 ou 1 */

	    Tcl_Obj *bit;
	    Tcl_Size lo;

	    if (quoted) {
		PathErr(interp, "a bitarray can't be indexed with the key "
			"'%s'%s", name, "");
		return NULL;
	    }
	    if (BitsPos(interp, tp, name, &lo) != TCL_OK
		    || TclBitsGet(interp, v, lo, 1, &bit) != TCL_OK) {
		return NULL;
	    }
	    elem = TclNewAnnotated(interp, TclTypeParse(interp, "int", 3), -1,
		    bit);
	    TclDiscard(bit);
	    return elem;
	}
	/* FALLTHRU */
    default:
	PathErr(interp, "can't index a value of type %s with \"%s\"",
		tp->name, name);
	return NULL;
    }
}


typedef struct Items {
    TclObjVec vec;
    int multi;
    int bitSlice;		/* a:b sur un bitarray : UN entier, pas une liste */
} Items;

static void
ItemsInit(Items *it)
{
    memset(it, 0, sizeof(*it));
}



static int
StepRange(Tcl_Interp *interp, Tcl_Obj *a, const char *from, const char *to,
	Items *out)
{
    Tcl_Obj *v, *elem;
    TclType *tp;
    int variant;
    Tcl_Size n, i, lo, hi;

    if (TclAnnotInfo(a, &v, &tp, &variant, NULL) == 1
	    && tp->kind == TCLT_INT && tp->bitWidth > 0) {
	/* tranche de bits b(i:j) : un bitarray de j-i+1 bits */

	Tcl_Obj *bits, *w;
	Tcl_Size blo, blen;

	if (BitsSlice(interp, tp, from, to, &blo, &blen) != TCL_OK
		|| TclBitsGet(interp, v, blo, blen, &bits) != TCL_OK) {
	    return TCL_ERROR;
	}
	w = TclNewAnnotated(interp, BitsSliceType(interp, tp, blen), -1, bits);
	TclDiscard(bits);
	if (w == NULL) {
	    return TCL_ERROR;
	}
	TclVecAdd(&out->vec, w);
	out->bitSlice = 1;
	return TCL_OK;
    }
    if (TclAnnotInfo(a, &v, &tp, &variant, NULL) != 1
	    || tp->kind != TCLT_LIST) {
	PathErr(interp, "a range needs a list value, not \"%s\"%s",
		(TclAnnotInfo(a, NULL, &tp, NULL, NULL) == 1) ? tp->name
		: "an untyped value", "");
	return TCL_ERROR;
    }
    if (Tcl_ListObjLength(interp, v, &n) != TCL_OK
	    || ListIndex(interp, from, n, &lo) != TCL_OK
	    || ListIndex(interp, to, n, &hi) != TCL_OK) {
	return TCL_ERROR;
    }
    if (lo < 0) {
	lo = 0;
    }
    if (hi >= n) {
	hi = n - 1;
    }
    for (i = lo; i <= hi; i++) {
	Tcl_Obj *w;

	Tcl_ListObjIndex(interp, v, i, &elem);
	w = WrapSlot(interp, tp->elem, elem);
	if (w == NULL) {
	    return TCL_ERROR;
	}
	TclVecAdd(&out->vec, w);
    }
    return TCL_OK;
}

/* --------------------------------------------------------------------
 * Lecture : application des chemins au conteneur
 * -------------------------------------------------------------------- */

static int
ReadSteps(Tcl_Interp *interp, Tcl_Obj *a, const IndexPath *p, Tcl_Size i,
	Items *out)
{
    const IndexStep *step;
    Tcl_Obj *r;
    int rc;

    if (i == p->n) {
	TclVecAdd(&out->vec, a);
	return TCL_OK;
    }
    step = &p->step[i];
    switch (step->kind) {
    case IS_TAG: {
	/* Une étiquette est un chemin à elle seule (garanti à la
	 * compilation) : elle est copiée dans le résultat. */

	TclType *s = TclTypeParse(interp, "string", 6);

	r = TclNewAnnotated(interp, s, -1, step->a);
	if (r == NULL) {
	    return TCL_ERROR;
	}
	TclVecAdd(&out->vec, r);
	return TCL_OK;
    }
    case IS_RANGE: {
	Items part;
	Tcl_Size k;

	ItemsInit(&part);
	if (StepRange(interp, a, Tcl_GetString(step->a),
		Tcl_GetString(step->b), &part) != TCL_OK) {
	    TclVecFree(&part.vec);
	    return TCL_ERROR;
	}
	rc = TCL_OK;
	if (part.bitSlice) {
	    out->bitSlice = 1;
	}
	for (k = 0; k < part.vec.n && rc == TCL_OK; k++) {
	    rc = ReadSteps(interp, part.vec.v[k], p, i + 1, out);
	}
	TclVecFree(&part.vec);
	return rc;
    }
    default:
	r = Step(interp, a, Tcl_GetString(step->a), step->kind == IS_KEY);
	if (r == NULL) {
	    return TCL_ERROR;
	}
	rc = ReadSteps(interp, r, p, i + 1, out);
	TclDiscard(r);
	return rc;
    }
}

/*
 * Lecture : *resultPtr reçoit une valeur typée, avec une référence de
 * plus que l'appelant doit rendre (Tcl_DecrRefCount). Un résultat unique
 * garde le type du champ ; une collecte donne une liste {list}T, T étant
 * le type commun des éléments (§10).
 */
int
TclTypePathRead(Tcl_Interp *interp, Tcl_Obj *containerPtr, const char *path,
	Tcl_Obj **resultPtr)
{
    PathSet set;
    Items out;
    Tcl_Size k;
    int rc = TCL_ERROR;

    if (CompileIndex(interp, path, &set) != TCL_OK) {
	return TCL_ERROR;
    }
    ItemsInit(&out);
    for (k = 0; k < set.n; k++) {
	if (ReadSteps(interp, containerPtr, &set.path[k], 0, &out)
		!= TCL_OK) {
	    goto done;
	}
    }
    out.multi = set.multi;

    /* Une case var en fin de chemin se lit à travers son lien. */
    for (k = 0; k < out.vec.n; k++) {
	Tcl_Obj *content, *target;
	TclType *vt;

	if (TclAnnotInfo(out.vec.v[k], &content, &vt, NULL, NULL) == 1
		&& vt->kind == TCLT_VAR) {
	    target = DerefSlot(interp, vt, content);
	    if (target == NULL) {
		goto done;
	    }
	    Tcl_IncrRefCount(target);
	    Tcl_DecrRefCount(out.vec.v[k]);
	    out.vec.v[k] = target;
	}
    }
    if ((!out.multi || out.bitSlice) && out.vec.n == 1) {
	*resultPtr = out.vec.v[0];
	Tcl_IncrRefCount(*resultPtr);
    } else {
	TclObjVec contents = {NULL, 0, 0};
	TclType *common = NULL, *tp;
	Tcl_Obj *content, *list, *a;
	Tcl_Size i;

	for (i = 0; i < out.vec.n; i++) {
	    TclAnnotInfo(out.vec.v[i], &content, &tp, NULL, NULL);
	    common = (common == NULL) ? tp : TclTypeCommon(interp, common, tp);
	    TclVecAdd(&contents, content);
	}
	if (common == NULL) {
	    common = TclTypeParse(interp, "string", 6);
	}
	list = Tcl_NewListObj(contents.n, contents.v);
	TclVecFree(&contents);
	a = TclNewAnnotated(interp, TclTypeListOf(interp, common), -1, list);
	TclDiscard(list);
	Tcl_IncrRefCount(a);
	*resultPtr = a;
    }
    rc = TCL_OK;

  done:
    TclVecFree(&out.vec);
    SetFree(&set);
    return rc;
}

/* --------------------------------------------------------------------
 * Écriture : reconstruit les objets le long du chemin (valeurs
 * immuables), le reste est partagé.
 * -------------------------------------------------------------------- */

static Tcl_Obj *
SetIn(Tcl_Interp *interp, Tcl_Obj *a, const IndexPath *chain, Tcl_Size i,
	Tcl_Obj *val, PendingWrites *pw)
{
    const IndexStep *step = &chain->step[i];
    int last = (i == chain->n - 1);
    const char *name = Tcl_GetString(step->a);
    Tcl_Obj *v, *child = NULL, *childContent = NULL, *newV, *res = NULL;
    Tcl_Obj **elems;
    TclType *tp, *slotType = NULL;
    int variant;
    Tcl_Size idx = -1, n;

    if (TclAnnotInfo(a, &v, &tp, &variant, NULL) != 1) {
	PathErr(interp, "can't assign into an untyped value with \"%s\"%s",
		name, "");
	return NULL;
    }

    /* Un lien est transparent : le pas s'applique à la variable visée, que
     * l'on réaffecte ensuite ; le conteneur ne change pas. */
    if (tp->kind == TCLT_VAR) {
	Tcl_Obj *target, *nc, *nct;

	if (!LinkTraversable(interp, tp, name)) {
	    return NULL;
	}
	target = DerefSlot(interp, tp, v);
	if (target == NULL) {
	    return NULL;
	}
	nc = SetIn(interp, target, chain, i, val, pw);
	TclDiscard(target);
	if (nc == NULL) {
	    return NULL;
	}
	TclAnnotInfo(nc, &nct, NULL, NULL, NULL);
	QueueWrite(pw, v, nct);
	TclDiscard(nc);
	return TclNewAnnotated(interp, tp, variant, v);
    }

    /* bitarray : écriture d'un bit b(i) ou d'une tranche b(i:j) */
    if (tp->kind == TCLT_INT && tp->bitWidth > 0) {
	Tcl_Obj *nv;
	Tcl_Size lo, len;

	if (!last || step->kind == IS_KEY || step->kind == IS_TAG) {
	    PathErr(interp, "can't assign into a bit of %s with \"%s\"",
		    tp->name, name);
	    return NULL;
	}
	if (step->kind == IS_RANGE) {
	    if (BitsSlice(interp, tp, name, Tcl_GetString(step->b), &lo, &len)
		    != TCL_OK) {
		return NULL;
	    }
	} else {
	    len = 1;
	    if (BitsPos(interp, tp, name, &lo) != TCL_OK) {
		return NULL;
	    }
	}
	if (TclBitsPut(interp, v, lo, len, val, &nv) != TCL_OK) {
	    return NULL;
	}
	res = TclNewAnnotated(interp, tp, -1, nv);
	TclDiscard(nv);
	return res;
    }
    if (step->kind == IS_RANGE) {
	PathErr(interp, "only simple names can be assigned, not ranges or "
		"tags%s%s", "", "");
	return NULL;
    }

    /* 1. Repérer la case. */
    switch (tp->kind) {
    case TCLT_PRODUCT:
	idx = TclTypeFieldIndex(tp, name);
	if (idx < 0 && step->kind != IS_KEY && IsIntegerName(name)) {
	    idx = atol(name);
	}
	if (idx < 0 || idx >= tp->nfields) {
	    PathErr(interp, "%s has no field \"%s\"", tp->name, name);
	    return NULL;
	}
	slotType = tp->fields[idx].type;
	if (Tcl_ListObjGetElements(interp, v, &n, &elems) != TCL_OK) {
	    return NULL;
	}
	break;
    case TCLT_LIST:
	if (Tcl_ListObjGetElements(interp, v, &n, &elems) != TCL_OK
		|| ListIndex(interp, name, n, &idx) != TCL_OK) {
	    return NULL;
	}
	if (idx < 0 || idx > n || (idx == n && !last)) {
	    PathErr(interp, "list index \"%s\" out of range%s", name, "");
	    return NULL;
	}
	/* Arité : ajouter à la fin d'une liste pleine, list(2), est refusé. */
	if (idx == n && tp->arity >= 0 && n + 1 > tp->arity) {
	    char num[32];

	    snprintf(num, sizeof(num), "%lld", (long long) tp->arity);
	    PathErr(interp, "%s cannot grow beyond %s elements", tp->name, num);
	    return NULL;
	}
	slotType = tp->elem;
	break;
    case TCLT_DICT:
	/* Clés : dict(x, y) n'admet que x et y. */
	if (!TclTypeDictKeyAllowed(tp, name)) {
	    PathErr(interp, "key \"%s\" is not allowed in %s", name, tp->name);
	    return NULL;
	}
	slotType = tp->elem;
	break;
    case TCLT_SUM:
	idx = TclTypeFieldIndex(tp, name);
	if (idx < 0 || tp->fields[idx].type == NULL) {
	    PathErr(interp, "%s has no variant \"%s\"", tp->name, name);
	    return NULL;
	}
	slotType = tp->fields[idx].type;
	break;
    default:
	PathErr(interp, "can't assign into a value of type %s with \"%s\"",
		tp->name, name);
	return NULL;
    }

    /* 2. Nouveau contenu de la case. */
    if (last && slotType->kind == TCLT_VAR) {
	/* Affectation à travers un lien : on écrit dans la variable. */

	Tcl_Obj *old = NULL, *c;

	if (tp->kind == TCLT_SUM) {
	    if (idx != variant) {
		PathErr(interp, "the current variant of %s is not \"%s\"",
			tp->name, name);
		return NULL;
	    }
	    old = v;
	} else if (tp->kind == TCLT_DICT) {
	    Tcl_Obj *key = Tcl_NewStringObj(name, TCL_AUTO_LENGTH);

	    if (Tcl_DictObjGet(interp, v, key, &old) != TCL_OK) {
		TclDiscard(key);
		return NULL;
	    }
	    TclDiscard(key);
	} else if (idx < n) {
	    old = elems[idx];
	}
	if (old == NULL) {
	    PathErr(interp, "no variable is linked at \"%s\"%s", name, "");
	    return NULL;
	}
	c = TclTypedConform(interp, slotType->elem, val);
	if (c == NULL) {
	    return NULL;
	}
	QueueWrite(pw, old, c);
	TclDiscard(c);
	childContent = Tcl_DuplicateObj(old);
    } else if (last) {
	childContent = TclTypedConform(interp, slotType, val);
	if (childContent == NULL) {
	    return NULL;
	}
    } else {
	Tcl_Obj *old = NULL, *nc;

	if (tp->kind == TCLT_SUM) {
	    if (idx != variant) {
		PathErr(interp, "the current variant of %s is not \"%s\"",
			tp->name, name);
		return NULL;
	    }
	    old = v;
	} else if (tp->kind == TCLT_DICT) {
	    Tcl_Obj *key = Tcl_NewStringObj(name, TCL_AUTO_LENGTH);

	    if (Tcl_DictObjGet(interp, v, key, &old) != TCL_OK || old == NULL) {
		TclDiscard(key);
		PathErr(interp, "key \"%s\" not known in dictionary%s", name,
			"");
		return NULL;
	    }
	    TclDiscard(key);
	} else {
	    old = elems[idx];
	}
	child = WrapSlot(interp, slotType, old);
	if (child == NULL) {
	    return NULL;
	}
	nc = SetIn(interp, child, chain, i + 1, val, pw);
	TclDiscard(child);
	if (nc == NULL) {
	    return NULL;
	}
	TclAnnotInfo(nc, &childContent, NULL, NULL, NULL);
	childContent = Tcl_DuplicateObj(childContent);
	TclDiscard(nc);
    }

    /* 3. Reconstruire le conteneur. */
    switch (tp->kind) {
    case TCLT_SUM:
	res = TclNewAnnotated(interp, tp, (int) idx, childContent);
	break;
    case TCLT_DICT: {
	Tcl_Obj *key = Tcl_NewStringObj(name, TCL_AUTO_LENGTH);

	newV = Tcl_DuplicateObj(v);
	if (TclTypeValidate(interp, tp->key, key, -1, NULL) != TCL_OK
		|| Tcl_DictObjPut(interp, newV, key, childContent)
			!= TCL_OK) {
	    TclDiscard(newV);
	    TclDiscard(key);
	    TclDiscard(childContent);
	    return NULL;
	}
	res = TclNewAnnotated(interp, tp, variant, newV);
	TclDiscard(newV);
	break;
    }
    default:				/* produit, liste */
	newV = Tcl_DuplicateObj(v);
	if (Tcl_ListObjReplace(interp, newV, idx, (idx < n) ? 1 : 0, 1,
		&childContent) != TCL_OK) {
	    TclDiscard(newV);
	    TclDiscard(childContent);
	    return NULL;
	}
	res = TclNewAnnotated(interp, tp, variant, newV);
	TclDiscard(newV);
	break;
    }
    TclDiscard(childContent);
    return res;
}

/*
 * Écriture : une cible par valeur (set L(1,2) a b). Toutes les cibles
 * s'appliquent sur le même conteneur, qui n'est remplacé qu'à la fin :
 * tout ou rien. *newPtr porte une référence de plus que l'appelant rend.
 */
int
TclTypePathWrite(Tcl_Interp *interp, Tcl_Obj *containerPtr, const char *path,
	Tcl_Size nvals, Tcl_Obj *const vals[], Tcl_Obj **newPtr)
{
    PathSet set;
    Tcl_Size i, j;
    Tcl_Obj *cur, *next;
    PendingWrites pw;
    int rc = TCL_ERROR;

    if (CompileIndex(interp, path, &set) != TCL_OK) {
	return TCL_ERROR;
    }
    memset(&pw, 0, sizeof(pw));
    if (set.n != nvals) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("wrong # args: %"
		TCL_SIZE_MODIFIER "d targets but %" TCL_SIZE_MODIFIER
		"d values", set.n, nvals));
	Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
	goto done;
    }
    for (i = 0; i < set.n; i++) {
	for (j = 0; j < set.path[i].n; j++) {
	    /* une tranche a:b est admise en dernier pas : elle vise un
	     * bitarray (SetIn refuse les autres conteneurs) */
	    if (set.path[i].step[j].kind == IS_RANGE
		    && j == set.path[i].n - 1) {
		continue;
	    }
	    if (set.path[i].step[j].kind > IS_KEY) {
		PathErr(interp, "only simple names can be assigned, not "
			"ranges or tags%s%s", "", "");
		goto done;
	    }
	}
    }
    cur = containerPtr;
    Tcl_IncrRefCount(cur);
    for (i = 0; i < set.n; i++) {
	next = SetIn(interp, cur, &set.path[i], 0, vals[i], &pw);
	Tcl_DecrRefCount(cur);
	if (next == NULL) {
	    goto done;
	}
	cur = next;
	Tcl_IncrRefCount(cur);
    }
    /* Le conteneur est prêt : on applique les affectations de variables. */

    for (i = 0; i < pw.names.n; i++) {
	if (TclVarRefSet(interp, Tcl_GetString(pw.names.v[i]),
		pw.values.v[i]) != TCL_OK) {
	    Tcl_DecrRefCount(cur);
	    goto done;
	}
    }
    *newPtr = cur;			/* référence conservée pour l'appelant */
    rc = TCL_OK;

  done:
    TclVecFree(&pw.names);
    TclVecFree(&pw.values);
    SetFree(&set);
    return rc;
}



/*
 * Appelée en tête de Tcl_SetObjCmd quand objv[1] ressemble à nom(chemin) :
 *	set P(x)          lecture ;  set P(x) v      écriture
 *	set L(1,2) a b    écriture groupée
 * Renvoie TCL_CONTINUE si la variable n'existe pas, n'est pas scalaire ou
 * n'est pas typée : le comportement habituel de Tcl est alors conservé
 * (set a(1,2) x reste l'idiome du tableau à deux dimensions).
 */
int
TclSetPathCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const char *name = Tcl_GetString(objv[1]);
    const char *open = strchr(name, '(');
    size_t len = strlen(name);
    Tcl_Obj *baseObj, *cur, *res = NULL, *newA = NULL;
    char *path;
    Var *varPtr, *arrayPtr = NULL;
    int rc;

    if (open == NULL || len == 0 || name[len - 1] != ')' || open == name) {
	return TCL_CONTINUE;
    }
    baseObj = Tcl_NewStringObj(name, open - name);
    Tcl_IncrRefCount(baseObj);
    varPtr = TclObjLookupVarEx(interp, baseObj, NULL, 0, "set", 0, 0,
	    &arrayPtr);
    if (varPtr != NULL) {
	while (TclIsVarLink(varPtr)) {
	    varPtr = varPtr->value.linkPtr;
	}
    }
    if (varPtr == NULL || arrayPtr != NULL || TclIsVarArray(varPtr)
	    || !(varPtr->flags & VAR_TYPED) || varPtr->value.objPtr == NULL) {
	Tcl_DecrRefCount(baseObj);
	return TCL_CONTINUE;
    }
    cur = varPtr->value.objPtr;
    path = (char *) Tcl_Alloc(len);
    memcpy(path, open + 1, len - (open - name) - 2);
    path[len - (open - name) - 2] = '\0';

    if (objc == 2) {
	rc = TclTypePathRead(interp, cur, path, &res);
	if (rc == TCL_OK) {
	    Tcl_SetObjResult(interp, res);
	    Tcl_DecrRefCount(res);
	}
    } else {
	rc = TclTypePathWrite(interp, cur, path, objc - 2, objv + 2, &newA);
	if (rc == TCL_OK) {
	    /* Le stockage repasse par le chemin d'écriture typé : même type,
	     * donc le contrat est respecté. */
	    res = Tcl_ObjSetVar2(interp, baseObj, NULL, newA,
		    TCL_LEAVE_ERR_MSG);
	    Tcl_DecrRefCount(newA);
	    if (res == NULL) {
		rc = TCL_ERROR;
	    } else {
		Tcl_SetObjResult(interp, res);
	    }
	}
    }
    Tcl_Free(path);
    Tcl_DecrRefCount(baseObj);
    return rc;
}

/*
 * Lecture $x(chemin) quand x est une variable scalaire typée (spécification
 * §10). À appeler là où Tcl lèverait "variable isn't array" :
 *   - Tcl_ObjGetVar2 (chemin non compilé, TclSubstTokens),
 *   - INST_LOAD_ARRAY1/4 et INST_LOAD_ARRAY_STK (bytecode), avant
 *     TclLookupArrayElement.
 *
 * Renvoie TCL_CONTINUE si le cas ne s'applique pas (variable absente,
 * tableau, non typée) : l'appelant poursuit son chemin habituel.
 * TCL_OK : *resultPtr est la valeur, que le résultat de l'interpréteur
 * garde en vie (les appelants qui la conservent doivent l'incrémenter,
 * comme avec le résultat d'une commande). TCL_ERROR : message dans interp.
 */
int
TclTypedVarGetElement(Tcl_Interp *interp, Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr,
	Tcl_Obj **resultPtr)
{
    Var *varPtr, *arrayPtr = NULL;
    Tcl_Obj *res = NULL;
    int rc;

    varPtr = TclObjLookupVarEx(interp, part1Ptr, NULL, 0, "read", 0, 0,
	    &arrayPtr);
    if (varPtr == NULL) {
	return TCL_CONTINUE;
    }
    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    if (arrayPtr != NULL || TclIsVarArray(varPtr)
	    || !(varPtr->flags & VAR_TYPED) || varPtr->value.objPtr == NULL) {
	return TCL_CONTINUE;
    }
    rc = TclTypePathRead(interp, varPtr->value.objPtr,
	    Tcl_GetString(part2Ptr), &res);
    if (rc != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, res);	/* l'interpréteur garde une référence */
    Tcl_DecrRefCount(res);
    *resultPtr = res;
    return TCL_OK;
}

/*
 * Variantes qui travaillent sur un Var déjà résolu (exécuteur : variables
 * locales compilées, résolues sans nom).
 */
int
TclTypedVarGetElementVar(Tcl_Interp *interp, Var *varPtr, Tcl_Obj *part2Ptr,
	Tcl_Obj **resultPtr)
{
    Tcl_Obj *res = NULL;

    while (varPtr != NULL && TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    if (varPtr == NULL || TclIsVarArray(varPtr)
	    || !(varPtr->flags & VAR_TYPED) || varPtr->value.objPtr == NULL) {
	return TCL_CONTINUE;
    }
    if (TclTypePathRead(interp, varPtr->value.objPtr,
	    Tcl_GetString(part2Ptr), &res) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, res);
    Tcl_DecrRefCount(res);
    *resultPtr = res;
    return TCL_OK;
}

/*
 * Écriture P(chemin) = valeur sur une variable scalaire typée. index >= 0
 * pour une variable locale compilée (part1Ptr peut alors être NULL), -1
 * sinon. Le résultat (la valeur écrite) est gardé en vie par le résultat de
 * l'interpréteur.
 */
int
TclTypedVarSetElementVar(Tcl_Interp *interp, Var *varPtr, Tcl_Obj *part1Ptr,
	int index, Tcl_Obj *part2Ptr, Tcl_Obj *valuePtr, Tcl_Obj **resultPtr)
{
    Tcl_Obj *newA = NULL, *stored;

    while (varPtr != NULL && TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    if (varPtr == NULL || TclIsVarArray(varPtr)
	    || !(varPtr->flags & VAR_TYPED) || varPtr->value.objPtr == NULL) {
	return TCL_CONTINUE;
    }
    if (TclTypePathWrite(interp, varPtr->value.objPtr,
	    Tcl_GetString(part2Ptr), 1, &valuePtr, &newA) != TCL_OK) {
	return TCL_ERROR;
    }
    stored = TclPtrSetVarIdx(interp, varPtr, NULL, part1Ptr, NULL, newA,
	    TCL_LEAVE_ERR_MSG, index);
    Tcl_DecrRefCount(newA);
    if (stored == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, valuePtr);
    *resultPtr = valuePtr;
    return TCL_OK;
}

/* Variante par nom (chemins non compilés : Tcl_ObjSetVar2 etc.). */
int
TclTypedVarSetElement(Tcl_Interp *interp, Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr,
	Tcl_Obj *valuePtr, Tcl_Obj **resultPtr)
{
    Var *varPtr, *arrayPtr = NULL;

    varPtr = TclObjLookupVarEx(interp, part1Ptr, NULL, 0, "set", 0, 0,
	    &arrayPtr);
    if (varPtr == NULL || arrayPtr != NULL) {
	return TCL_CONTINUE;
    }
    return TclTypedVarSetElementVar(interp, varPtr, part1Ptr, -1, part2Ptr,
	    valuePtr, resultPtr);
}

