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
 *	  "étiquette" copiée telle quelle dans le résultat (littérale : le
 *	              chemin ne substitue rien)
 *
 *	SUBSTITUTIONS. Le chemin n'en fait AUCUNE (même règle qu'un tableau :
 *	${a($k)} et ${R($k.x)} sont littéraux). C'est Tcl qui substitue $x,
 *	[cmd] et [(expr)] dans $R(...) et dans les mots nus, et une valeur
 *	substituée est UNE feuille, jamais relue comme un chemin : avec
 *	k = file.txt, $D($k) lit la clé « file.txt » ; collée à du texte
 *	($D(f_$n)), elle prolonge le nom. Les portées substituées arrivent
 *	avec l'index (objet « pathindex », voir TclPathIndexNew). *$x déplie
 *	une valeur liste en autant d'alternatives : $L(*$idx).
 *
 *	OPÉRATEURS, du plus faible au plus fort
 *	  ,    collecte (binaire)       a,b
 *	  *    dépliage (unaire)        *0:2,4   *(a,b),c : le groupe ou
 *	                                l'intervalle se fond dans le groupe
 *	                                qui le contient
 *	  .    imbrication (binaire)    a.b      (a.(b,c) = a.b,a.c)
 *	  :    intervalle de liste      0:end-1
 *	  ( )  groupement
 *
 *
 *	RÉSULTAT DE LA COMPILATION. L'équivalent du bytecode est un
 *	ENSEMBLE DE CHEMINS : chaque chemin est une suite de pas (nom, clé,
 *	intervalle, étiquette). "." fait le produit des chemins de ses
 *	opérandes, "," les réunit. La lecture applique chaque chemin au
 *	conteneur ; l'écriture exige des chemins de pas simples, un par
 *	valeur.
 *
 *	FORME. L'ensemble de chemins porte aussi une FORME : un arbre dont
 *	les feuilles sont les chemins et les noeuds les collectes, comme sur
 *	la page du wiki. "," fait un groupe (les parenthèses le ferment :
 *	(a,b),c donne {{a b} c}) ; "." remplace chaque feuille de gauche par
 *	la forme de droite. La lecture rend des listes imbriquées selon la
 *	forme : $R((A,B).(x,y)) donne {Ax Ay} {Bx By}.
 *
 *	ÉCRITURE ET CONSTRUCTION. Trois façons de donner les valeurs, que le
 *	NOMBRE de valeurs départage (jamais leur contenu) :
 *	  une par feuille           set R((A,B).(x,y)) 1 2 3 4
 *	  une par élément du groupe set R(A.(x,y),B.(x,y)) {1 2} {3 4}
 *	  une pour tout le chemin   set R((A,B).(x,y)) {{1 2} {3 4}}
 *	chaque valeur ayant la forme de ce que sa lecture rendrait. Les
 *	nombres ne coïncident que si les lectures coïncident.
 *	set {T}R(chemin) v... construit un produit (et ses produits
 *	imbriqués) : les champs nommés reçoivent les valeurs, les autres leur
 *	défaut (TclTypePathConstruct).
 *
 *	Limites : un intervalle a:b est un résultat multiple dont la taille
 *	n'est connue qu'à la lecture : (0:1).(x,y) donne la colonne des x puis
 *	celle des y ; une étiquette est un chemin à elle seule ; pas
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
 * L'index et ses portées substituées
 *
 *	Un index de $R(...) dont une partie vient d'une substitution est un
 *	objet « pathindex » : sa chaîne est l'index (un tableau ordinaire ne
 *	voit qu'elle), sa représentation interne la liste des portées
 *	[début, longueur) substituées. Produit par l'instruction indexConcat
 *	et par TclSubstTokens (index non compilé).
 * -------------------------------------------------------------------- */

typedef struct PathIndexRep {
    Tcl_Size n;
    Tcl_Size pos[1];		/* 2 * n : début, longueur */
} PathIndexRep;

static void FreePathIndex(Tcl_Obj *objPtr);
static void DupPathIndex(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);

static const Tcl_ObjType pathIndexType = {
    "pathindex", FreePathIndex, DupPathIndex, NULL, NULL, TCL_OBJTYPE_V0
};

struct CompiledPath;
static const Tcl_ObjType pathFieldType = {
    "pathfield", NULL, NULL, NULL, NULL, TCL_OBJTYPE_V0
};
static void CompiledPathRelease(struct CompiledPath *cp);

static void
FreePathIndex(Tcl_Obj *objPtr)
{
    Tcl_Free(objPtr->internalRep.twoPtrValue.ptr1);
    if (objPtr->internalRep.twoPtrValue.ptr2 != NULL) {
	CompiledPathRelease((struct CompiledPath *)
		objPtr->internalRep.twoPtrValue.ptr2);
    }
    objPtr->typePtr = NULL;
}

static void
DupPathIndex(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
    PathIndexRep *s = (PathIndexRep *) srcPtr->internalRep.twoPtrValue.ptr1;
    size_t sz = sizeof(PathIndexRep) + (2 * (size_t) s->n) * sizeof(Tcl_Size);
    PathIndexRep *d = (PathIndexRep *) Tcl_Alloc(sz);

    memcpy(d, s, sz);
    dupPtr->internalRep.twoPtrValue.ptr1 = d;
    dupPtr->internalRep.twoPtrValue.ptr2 = NULL;
    dupPtr->typePtr = &pathIndexType;
}

/*
 * Concatène parts[0..n-1] ; subst[i] non nul : la part i vient d'une
 * substitution et devient une portée atomique. Sans aucune part
 * substituée, c'est une simple concaténation. refcount 0.
 */
Tcl_Obj *
TclPathIndexNew(Tcl_Obj *const parts[], Tcl_Size n, const char *subst)
{
    Tcl_DString ds;
    Tcl_Size i, ns = 0, k = 0;
    PathIndexRep *rep;
    Tcl_Obj *res;

    Tcl_DStringInit(&ds);
    for (i = 0; i < n; i++) {
	ns += (subst[i] != 0);
    }
    rep = (PathIndexRep *) Tcl_Alloc(sizeof(PathIndexRep)
	    + (2 * (size_t) (ns ? ns : 1)) * sizeof(Tcl_Size));
    rep->n = ns;
    for (i = 0; i < n; i++) {
	Tcl_Size len;
	const char *str = Tcl_GetStringFromObj(parts[i], &len);

	if (subst[i]) {
	    rep->pos[2 * k] = Tcl_DStringLength(&ds);
	    rep->pos[2 * k + 1] = len;
	    k++;
	}
	Tcl_DStringAppend(&ds, str, len);
    }
    res = Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);
    if (ns == 0) {
	Tcl_Free(rep);
	return res;
    }
    res->internalRep.twoPtrValue.ptr1 = rep;
    res->internalRep.twoPtrValue.ptr2 = NULL;
    res->typePtr = &pathIndexType;
    return res;
}

/* Les portées substituées d'un index (0 s'il n'en porte pas). */
static Tcl_Size
PathIndexSpans(Tcl_Obj *objPtr, const Tcl_Size **posPtr)
{
    if (objPtr != NULL && objPtr->typePtr == &pathIndexType) {
	PathIndexRep *rep = (PathIndexRep *)
		objPtr->internalRep.twoPtrValue.ptr1;

	*posPtr = rep->pos;
	return rep->n;
    }
    *posPtr = NULL;
    return 0;
}

/* Curseur de lecture des portées, dans l'ordre du texte. */
typedef struct IndexSpans {
    const char *base;
    const Tcl_Size *pos;
    Tcl_Size n, next;
} IndexSpans;

/* Longueur de la portée qui commence en p (et l'avance), -1 sinon. */
static Tcl_Size
SpanTake(IndexSpans *sp, const char *p)
{
    if (sp != NULL && sp->next < sp->n
	    && sp->pos[2 * sp->next] == p - sp->base) {
	return sp->pos[2 * sp->next++ + 1];
    }
    return -1;
}

static int
SpanStartsAt(const IndexSpans *sp, const char *p)
{
    return sp != NULL && sp->next < sp->n
	    && sp->pos[2 * sp->next] == p - sp->base;
}

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
    OT_ALL = -9,		/* "*" seul : toutes les parties */
    OT_ATOM = -8,		/* une valeur substituée, seule */
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
    UNCLOSED = 7,		/* ' ou " sans fermeture */
    NOSUBST = 9,		/* $ ou [ : le chemin ne substitue pas */

    /* Feuilles */

    NAMEWORD = LEAF | 1,
    KEYWORD = LEAF | 2,
    TAGWORD = LEAF | 3,
    VARIABLE = LEAF | 4,
    SCRIPT = LEAF | 5,
    ARITH = LEAF | 6,
    ALLWORD = LEAF | 7,		/* "*" seul */

    /* Unaires */

    START = UNARY | 4,
    OPEN_PAREN = UNARY | 5,
    SPLAT = UNARY | 6,

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
    PREC_SPLAT,
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
    [OPEN_PAREN] = PREC_OPEN_PAREN,
    [SPLAT] = PREC_SPLAT
};

/* Octets qui ne peuvent pas faire partie d'un nom nu. */

static int
IsDelimiter(int c)
{
    return c == ',' || c == '.' || c == ':' || c == '(' || c == ')'
	    || c == '*'
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
    Tcl_Obj **literalPtr,	/* texte d'un nom, d'une 'clé', d'un "tag" */
    IndexSpans *sp,		/* portées substituées (NULL : aucune) */
    int *atomPtr)		/* le nom est UNE valeur substituée, seule */
{
    const char *end, *limit = start + numBytes;
    Tcl_Size spans = 0, L;
    int literalChars = 0;

    *literalPtr = NULL;
    *atomPtr = 0;
    if (numBytes == 0) {
	*lexemePtr = END;
	return 0;
    }
    if (!SpanStartsAt(sp, start)) {
	switch (*start) {
	case ',': *lexemePtr = COMMA; return 1;
	case '.': *lexemePtr = DOT; return 1;
	case ':': *lexemePtr = RANGE; return 1;
	case '*':
	    /* "*" seul (suivi de la fin, de . , : ) ou d'un blanc) : toutes
	     * les parties ; sinon le dépliage unaire. */
	    /* 23/11 : "*:" est le dépliage d'une tranche (*:2), pas « tout » */
	    if (start + 1 == limit || strchr(".,) \t", start[1]) != NULL) {
		*lexemePtr = ALLWORD;
		*literalPtr = Tcl_NewStringObj("*", 1);
		return 1;
	    }
	    *lexemePtr = SPLAT; return 1;
	case '(': *lexemePtr = OPEN_PAREN; return 1;
	case ')': *lexemePtr = CLOSE_PAREN; return 1;
	case '$':
	case '[': *lexemePtr = NOSUBST; return 1;
	case '\'':
	case '"': {
	    /* 'clé' ou "étiquette" : jusqu'au délimiteur pareil, les portées
	     * substituées étant du texte opaque */
	    char q = *start;

	    for (end = start + 1; end < limit; ) {
		if ((L = SpanTake(sp, end)) >= 0) {
		    end += L;
		} else if (*end == q) {
		    break;
		} else {
		    end++;
		}
	    }
	    if (end >= limit) {
		*lexemePtr = UNCLOSED;
		return 1;
	    }
	    *lexemePtr = (q == '\'') ? KEYWORD : TAGWORD;
	    *literalPtr = Tcl_NewStringObj(start + 1, end - start - 1);
	    return end + 1 - start;
	}
	}
    }
    /* Un nom : caractères de nom et portées substituées, collés. */
    for (end = start; end < limit; ) {
	if ((L = SpanTake(sp, end)) >= 0) {
	    end += L;
	    spans++;
	} else if (!IsDelimiter(UCHAR(*end))) {
	    end++;
	    literalChars = 1;
	} else {
	    break;
	}
    }
    if (end == start && spans == 0) {
	*lexemePtr = INVALID;
	return 1;
    }
    *lexemePtr = NAMEWORD;
    *atomPtr = (spans == 1 && !literalChars);
    *literalPtr = Tcl_NewStringObj(start, end - start);
    return end - start;
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
    Tcl_Obj *leafList,
    IndexSpans *sp)
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
    int insertMark = 0, atom = 0;
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

	/* les blancs d'une valeur substituée en font partie */
	scanned = SpanStartsAt(sp, start) ? 0
		: TclParseAllWhiteSpace(start, numBytes);
	start += scanned;
	numBytes -= scanned;

	literal = NULL;
	scanned = ParseIndexLexeme(start, numBytes, &lexeme, &literal, sp,
		&atom);

	/* 23/11 : une partie omise d'un intervalle (:b, a:, a::c) : un nom
	 * vide, que l'évaluation remplace (0, end, pas 1) ; rien n'est lu */
	if (IsOperator(lastParsed) && (lexeme == RANGE
		|| (nodes[lastParsed].lexeme == RANGE && (lexeme == END
		    || lexeme == CLOSE_PAREN || lexeme == COMMA)))) {
	    if (literal != NULL) {
		TclDiscard(literal);
	    }
	    lexeme = NAMEWORD;
	    literal = Tcl_NewObj();
	    atom = 0;
	    scanned = 0;
	}

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
	    case NOSUBST:
		msg = Tcl_ObjPrintf("a path does not substitute \"%c\": Tcl "
			"substitutes $x and [cmd] in $name(...) and in bare "
			"words, not inside braces or ${...}", *start);
		errCode = "NOSUBST";
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
	    case TAGWORD:
	    case ALLWORD:
		Tcl_ListObjAppendElement(NULL, leafList, literal);
		literal = NULL;
		complete = lastParsed = (lexeme == ALLWORD) ? OT_ALL
			: (lexeme == KEYWORD) ? OT_KEY
			: (lexeme == TAGWORD) ? OT_TAG
			: atom ? OT_ATOM : OT_NAME;
		break;
	    }
	    (void) value;
	    (void) arith;
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

typedef enum { IS_NAME, IS_KEY, IS_RANGE, IS_TAG, IS_ALL } StepKind;

typedef struct IndexStep {
    StepKind kind;
    Tcl_Obj *a;			/* nom, clé, début d'intervalle, étiquette */
    Tcl_Obj *b;			/* fin d'intervalle */
    Tcl_Obj *c;			/* pas d'intervalle a:b:c (NULL : aucun) */
    int atom;			/* nom = une valeur substituée, seule */
    /* 24/11 : fenêtre (s0,s1,...):b:c, une colonne par début ; le nombre
     * est celui des fenêtres complètes (NULL : intervalle ordinaire) */
    Tcl_Obj *wstarts;		/* la liste des débuts */
    Tcl_Obj *wbound;		/* la fin, pour le premier début */
    int wj;			/* la colonne de ce chemin */
} IndexStep;

/* un intervalle résolu par RangeSpec (avec pas, ou fenêtre) */
#define RANGE_SPECD(s) ((s)->c != NULL || (s)->wstarts != NULL)

typedef struct IndexPath {
    IndexStep *step;
    Tcl_Size n;
} IndexPath;

/* La forme : feuille (leaf = index du chemin) ou groupe (leaf = -1). */
typedef struct Shape {
    Tcl_Size leaf;
    Tcl_Size n;
    struct Shape *kid;
    int splat;			/* "*" : se fond dans le groupe parent */
} Shape;

typedef struct PathSet {
    IndexPath *path;
    Tcl_Size n;
    int multi;			/* résultat multiple : liste, même à 1 élément */
    Shape shape;		/* memset 0 : feuille 0, un seul chemin */
    int open;			/* groupe d'une "," pas encore refermé par ")" */
} PathSet;

static void
ShapeFree(Shape *sh)
{
    Tcl_Size i;

    for (i = 0; i < sh->n; i++) {
	ShapeFree(&sh->kid[i]);
    }
    if (sh->kid != NULL) {
	Tcl_Free(sh->kid);
    }
    sh->kid = NULL;
    sh->n = 0;
}

static Shape
ShapeCopy(const Shape *sh, Tcl_Size shift)
{
    Shape c;
    Tcl_Size i;

    c.leaf = (sh->leaf >= 0) ? sh->leaf + shift : -1;
    c.n = sh->n;
    c.kid = NULL;
    c.splat = sh->splat;
    if (sh->n > 0) {
	c.kid = (Shape *) Tcl_Alloc(sh->n * sizeof(Shape));
	for (i = 0; i < sh->n; i++) {
	    c.kid[i] = ShapeCopy(&sh->kid[i], shift);
	}
    }
    return c;
}

static void ShapeAppend(Shape *g, Shape *sh, int open);

/* l.r : chaque feuille i de l devient la forme de r, décalée de i * rn. */
static Shape
ShapeSubst(const Shape *l, const Shape *r, Tcl_Size rn)
{
    Shape c;
    Tcl_Size i;

    if (l->leaf >= 0) {
	c = ShapeCopy(r, l->leaf * rn);
	c.splat |= l->splat;
	return c;
    }
    c.leaf = -1;
    c.n = 0;
    c.splat = l->splat;
    c.kid = NULL;
    for (i = 0; i < l->n; i++) {
	Shape k = ShapeSubst(&l->kid[i], r, rn);

	ShapeAppend(&c, &k, 0);		/* un groupe marqué "*" se fond */
    }
    return c;
}

/* Ajoute sh au groupe g (sh est déplacé) ; un groupe ouvert est fondu. */
static void
ShapeAppend(Shape *g, Shape *sh, int open)
{
    Tcl_Size i;

    if ((open || sh->splat) && sh->leaf < 0) {
	g->kid = (Shape *) Tcl_Realloc(g->kid, (g->n + sh->n) * sizeof(Shape));
	for (i = 0; i < sh->n; i++) {
	    g->kid[g->n++] = sh->kid[i];
	}
	if (sh->kid != NULL) {
	    Tcl_Free(sh->kid);
	}
    } else {
	g->kid = (Shape *) Tcl_Realloc(g->kid, (g->n + 1) * sizeof(Shape));
	g->kid[g->n++] = *sh;
    }
    sh->kid = NULL;
    sh->n = 0;
}

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
	if (p->step[i].c != NULL) {
	    Tcl_DecrRefCount(p->step[i].c);
	}
	if (p->step[i].wstarts != NULL) {
	    Tcl_DecrRefCount(p->step[i].wstarts);
	    Tcl_DecrRefCount(p->step[i].wbound);
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
    ShapeFree(&s->shape);
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
    p.step[0].c = NULL;
    p.step[0].atom = 0;
    p.step[0].wstarts = NULL;
    p.step[0].wbound = NULL;
    p.step[0].wj = 0;
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
		if (p.step[k].c != NULL) {
		    Tcl_IncrRefCount(p.step[k].c);
		}
		if (p.step[k].wstarts != NULL) {
		    Tcl_IncrRefCount(p.step[k].wstarts);
		    Tcl_IncrRefCount(p.step[k].wbound);
		}
	    }
	    SetAddPath(&out, p);
	}
    }
    out.shape = ShapeSubst(&l.shape, &r.shape, r.n);
    out.open = 0;
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
    Shape g = {-1, 0, NULL, 0}, ls = l.shape, rs = ShapeCopy(&r.shape, l.n);

    ShapeAppend(&g, &ls, l.open);
    ShapeAppend(&g, &rs, r.open);
    l.shape = g;
    l.open = 1;
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

/*
 * ":" : intervalle ; les deux bornes sont des index simples. 23/11 : un
 * pas, a:b:c (à la Python, la fin restant incluse comme en Tcl), et des
 * parties omises (texte vide : :b, a:, ::c) que l'évaluation résout.
 */

static int
ApplyRange(IndexEnv *env)
{
    PathSet r = PopSet(env), l = PopSet(env), out;
    int ok = (l.n == 1 && r.n == 1 && l.path[0].n == 1 && r.path[0].n == 1
	    && l.path[0].step[0].kind <= IS_KEY
	    && r.path[0].step[0].kind <= IS_KEY);

    if (!ok && l.n == 1 && r.n == 1 && l.path[0].n == 1 && r.path[0].n == 1
	    && l.path[0].step[0].kind == IS_RANGE
	    && l.path[0].step[0].c == NULL
	    && l.path[0].step[0].wstarts == NULL
	    && r.path[0].step[0].kind <= IS_KEY) {
	/* (a:b):c : le pas */
	l.path[0].step[0].c = r.path[0].step[0].a;
	Tcl_IncrRefCount(l.path[0].step[0].c);
	SetFree(&r);
	PushSet(env, l);
	return TCL_OK;
    }
    if (!ok && l.n >= 1 && r.n == 1 && r.path[0].n == 1
	    && r.path[0].step[0].kind <= IS_KEY
	    && l.path[0].n == 1 && l.path[0].step[0].wstarts != NULL
	    && l.path[0].step[0].c == NULL) {
	/* ((s0,s1...):b):c : le pas de la fenêtre, pour chaque colonne */
	Tcl_Size j;

	for (j = 0; j < l.n; j++) {
	    l.path[j].step[0].c = r.path[0].step[0].a;
	    Tcl_IncrRefCount(l.path[j].step[0].c);
	}
	SetFree(&r);
	PushSet(env, l);
	return TCL_OK;
    }
    if (!ok && l.n > 1 && !l.open && r.n == 1 && r.path[0].n == 1
	    && r.path[0].step[0].kind <= IS_KEY) {
	/* 24/11 : (s0,s1,...):b, une fenêtre : une colonne par début */
	Tcl_Size j;
	Tcl_Obj *starts, *empty;
	PathSet w;

	for (j = 0; j < l.n; j++) {
	    if (l.path[j].n != 1 || l.path[j].step[0].kind > IS_KEY) {
		PathErr(env->interp, "a window needs single indices: "
			"(start,start,...):end:step%s%s", "", "");
		SetFree(&l);
		SetFree(&r);
		return TCL_ERROR;
	    }
	}
	starts = Tcl_NewListObj(0, NULL);
	for (j = 0; j < l.n; j++) {
	    Tcl_ListObjAppendElement(NULL, starts, l.path[j].step[0].a);
	}
	empty = Tcl_NewObj();
	Tcl_IncrRefCount(empty);
	memset(&w, 0, sizeof(w));
	for (j = 0; j < l.n; j++) {
	    PathSet one = OneStep(IS_RANGE, l.path[j].step[0].a,
		    (j == 0) ? r.path[0].step[0].a : empty);
	    IndexStep *st = &one.path[0].step[0];

	    st->wstarts = starts;
	    st->wbound = r.path[0].step[0].a;
	    Tcl_IncrRefCount(st->wstarts);
	    Tcl_IncrRefCount(st->wbound);
	    st->wj = (int) j;
	    SetAddPath(&w, one.path[0]);
	    one.n = 0;
	    SetFree(&one);
	}
	Tcl_DecrRefCount(empty);
	w.shape = l.shape;		/* le groupe : une colonne par début */
	memset(&l.shape, 0, sizeof(l.shape));
	w.multi = 1;
	w.open = 0;
	SetFree(&l);
	SetFree(&r);
	PushSet(env, w);
	return TCL_OK;
    }
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
    case SPLAT:			/* "*" : dépliage dans le groupe parent */
	if (env->nStack > 0) {
	    PathSet *top = &env->stack[env->nStack - 1];

	    if (top->n == 1 && top->path[0].n == 1
		    && top->path[0].step[0].kind == IS_NAME
		    && top->path[0].step[0].atom) {
		/* *$x : les éléments de la valeur, autant d'alternatives */
		Tcl_Obj *v = top->path[0].step[0].a, **elems;
		Tcl_Size ne, i;
		PathSet alt;

		Tcl_IncrRefCount(v);
		if (Tcl_ListObjGetElements(env->interp, v, &ne, &elems)
			!= TCL_OK) {
		    Tcl_DecrRefCount(v);
		    return TCL_ERROR;
		}
		memset(&alt, 0, sizeof(alt));
		alt.shape.leaf = -1;
		alt.shape.kid = (Shape *) Tcl_Alloc((ne ? ne : 1) * sizeof(Shape));
		for (i = 0; i < ne; i++) {
		    PathSet one = OneStep(IS_NAME, elems[i], NULL);

		    SetAddPath(&alt, one.path[0]);
		    Tcl_Free(one.path);
		    alt.shape.kid[i].leaf = i;
		    alt.shape.kid[i].n = 0;
		    alt.shape.kid[i].kid = NULL;
		    alt.shape.kid[i].splat = 0;
		}
		alt.shape.n = ne;
		alt.multi = 1;
		Tcl_DecrRefCount(v);
		SetFree(top);
		*top = alt;
	    }
	    top->shape.splat = 1;
	}
	return TCL_OK;
    case OPEN_PAREN:		/* ")" referme le groupe */
	if (env->nStack > 0) {
	    env->stack[env->nStack - 1].open = 0;
	}
	return TCL_OK;
    default:			/* START */
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
    PathSet s;

    switch (kind) {
    case OT_KEY:
	s = OneStep(IS_KEY, text, NULL);
	break;
    case OT_TAG:
	s = OneStep(IS_TAG, text, NULL);	/* littérale */
	break;
    case OT_ATOM:
	s = OneStep(IS_NAME, text, NULL);
	s.path[0].step[0].atom = 1;
	break;
    case OT_ALL:
	s = OneStep(IS_ALL, text, NULL);
	s.multi = 1;
	break;
    default:			/* OT_NAME */
	s = OneStep(IS_NAME, text, NULL);
	break;
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
	case OT_ATOM:
	case OT_ALL:
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
    Tcl_Obj *pathObj,		/* l'index et ses portées (ou NULL) */
    PathSet *setPtr)
{
    IndexSpans spans, *sp = NULL;
    OpNode *opTree = NULL;
    Tcl_Obj *leafList;
    IndexEnv env;
    int code;

    memset(&env, 0, sizeof(env));
    env.interp = interp;
    memset(setPtr, 0, sizeof(*setPtr));
    TclNewObj(leafList);
    Tcl_IncrRefCount(leafList);

    if (pathObj != NULL) {
	spans.base = text;
	spans.next = 0;
	spans.n = PathIndexSpans(pathObj, &spans.pos);
	if (spans.n > 0) {
	    sp = &spans;
	}
    }
    code = ParseIndex(interp, text, strlen(text), &opTree, leafList, sp);
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

/* --------------------------------------------------------------------
 * Le chemin compilé, gardé
 *
 *	CompileIndex ne dépend que du texte de l'index et de ses portées
 *	substituées : son résultat, un PathSet que la lecture et l'écriture ne
 *	modifient pas, est gardé
 *	  - dans l'objet index lui-même : un littéral du bytecode ("a.x" dans
 *	    $s(a.x)) prend le type "typepath" ; un index aux portées
 *	    substituées (pathindex) le garde à côté de ses portées ;
 *	  - et, pour un index sans portée, dans une table par thread, par son
 *	    texte (index fabriqué à chaque fois : $s($k.x), set s(a.x)).
 *	Un compteur de références protège le PathSet pendant qu'on s'en sert
 *	(une trace lue en chemin peut faire changer l'index de type).
 * -------------------------------------------------------------------- */

typedef struct CompiledPath {
    size_t refs;
    int chain;			/* un seul chemin de noms (FastChain) */
    PathSet set;
} CompiledPath;

static void
CompiledPathRelease(CompiledPath *cp)
{
    if (cp->refs-- <= 1) {
	SetFree(&cp->set);
	Tcl_Free(cp);
    }
}

static void
FreeTypePath(Tcl_Obj *objPtr)
{
    CompiledPathRelease((CompiledPath *) objPtr->internalRep.twoPtrValue.ptr1);
    objPtr->typePtr = NULL;
}

static void
DupTypePath(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
    CompiledPath *cp = (CompiledPath *) srcPtr->internalRep.twoPtrValue.ptr1;

    cp->refs++;
    dupPtr->internalRep.twoPtrValue.ptr1 = cp;
    dupPtr->internalRep.twoPtrValue.ptr2 = NULL;
    dupPtr->typePtr = srcPtr->typePtr;
}

static const Tcl_ObjType typePathType = {
    "typepath", FreeTypePath, DupTypePath, NULL, NULL, TCL_OBJTYPE_V0
};

#define PATH_CACHE_MAX 2048

typedef struct PathCacheTSD {
    int init;
    Tcl_HashTable table;	/* texte -> CompiledPath * */
} PathCacheTSD;
static Tcl_ThreadDataKey pathCacheKey;

static void
PathCacheClear(PathCacheTSD *tsd)
{
    Tcl_HashSearch hs;
    Tcl_HashEntry *he;

    for (he = Tcl_FirstHashEntry(&tsd->table, &hs); he != NULL;
	    he = Tcl_NextHashEntry(&hs)) {
	CompiledPathRelease((CompiledPath *) Tcl_GetHashValue(he));
    }
    Tcl_DeleteHashTable(&tsd->table);
}

static void
PathCacheExit(void *clientData)
{
    PathCacheTSD *tsd = (PathCacheTSD *) clientData;

    if (tsd->init) {
	PathCacheClear(tsd);
	tsd->init = 0;
    }
}

/*
 * Le chemin compilé de l'index (texte text, objet pathObj ou NULL), avec une
 * référence pour l'appelant, qui la rend par CompiledPathRelease. Sur une
 * erreur de syntaxe, NULL et le message dans interp (rien n'est gardé).
 */
static CompiledPath *
GetCompiledPath(Tcl_Interp *interp, const char *text, Tcl_Obj *pathObj)
{
    CompiledPath *cp;
    PathCacheTSD *tsd = NULL;
    Tcl_HashEntry *he = NULL;
    const Tcl_Size *pos;
    int isNew, spans = PathIndexSpans(pathObj, &pos) != 0;

    if (pathObj != NULL) {
	if (pathObj->typePtr == &typePathType) {
	    cp = (CompiledPath *) pathObj->internalRep.twoPtrValue.ptr1;
	    cp->refs++;
	    return cp;
	}
	if (pathObj->typePtr == &pathIndexType
		&& pathObj->internalRep.twoPtrValue.ptr2 != NULL) {
	    cp = (CompiledPath *) pathObj->internalRep.twoPtrValue.ptr2;
	    cp->refs++;
	    return cp;
	}
    }
    if (!spans) {
	tsd = (PathCacheTSD *) Tcl_GetThreadData(&pathCacheKey,
		sizeof(PathCacheTSD));
	if (!tsd->init) {
	    Tcl_InitHashTable(&tsd->table, TCL_STRING_KEYS);
	    Tcl_CreateThreadExitHandler(PathCacheExit, tsd);
	    tsd->init = 1;
	}
	he = Tcl_FindHashEntry(&tsd->table, text);
    }
    if (he != NULL) {
	cp = (CompiledPath *) Tcl_GetHashValue(he);
    } else {
	cp = (CompiledPath *) Tcl_Alloc(sizeof(CompiledPath));
	cp->refs = 0;
	if (CompileIndex(interp, text, pathObj, &cp->set) != TCL_OK) {
	    Tcl_Free(cp);
	    return NULL;
	}
	cp->chain = (cp->set.n == 1 && cp->set.shape.leaf == 0);
	if (cp->chain) {
	    Tcl_Size j;

	    for (j = 0; j < cp->set.path[0].n; j++) {
		if (cp->set.path[0].step[j].kind != IS_NAME
			|| cp->set.path[0].step[j].atom) {
		    cp->chain = 0;
		}
	    }
	}
	if (tsd != NULL) {
	    if (tsd->table.numEntries >= PATH_CACHE_MAX) {
		PathCacheClear(tsd);
		Tcl_InitHashTable(&tsd->table, TCL_STRING_KEYS);
	    }
	    he = Tcl_CreateHashEntry(&tsd->table, text, &isNew);
	    cp->refs++;
	    Tcl_SetHashValue(he, cp);
	}
    }
    /* gardé dans l'objet index : un texte seul, ou un pathindex */
    if (pathObj != NULL && !TclIsAnnotated(pathObj)) {
	if (pathObj->typePtr == &pathIndexType) {
	    cp->refs++;
	    pathObj->internalRep.twoPtrValue.ptr2 = cp;
	} else if (!spans && pathObj->bytes != NULL
		&& (pathObj->typePtr == NULL
		|| pathObj->typePtr == &pathFieldType)) {
	    Tcl_ObjInternalRep ir;

	    cp->refs++;
	    ir.twoPtrValue.ptr1 = cp;
	    ir.twoPtrValue.ptr2 = NULL;
	    Tcl_StoreInternalRep(pathObj, &typePathType, &ir);
	}
    }
    cp->refs++;				/* celle de l'appelant */
    return cp;
}

static void
PathErr(Tcl_Interp *interp, const char *fmt, const char *a, const char *b)
{
    if (interp == NULL) {
	return;
    }
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(fmt, a, b));
    Tcl_SetErrorCode(interp, "TCL", "TYPE", "PATH", (char *) NULL);
}

/* Enveloppe le contenu d'une case dans son type (somme : variante déduite
 * par revalidation, la balise d'un élément imbriqué n'est pas conservée). */
static Tcl_Obj *WrapSlotHint(Tcl_Interp *interp, TclType *tp,
	Tcl_Obj *content, int hint);

static Tcl_Obj *
WrapSlot(Tcl_Interp *interp, TclType *tp, Tcl_Obj *content)
{
    return WrapSlotHint(interp, tp, content, -1);
}

/* Idem, hint : la variante désignée par un sélecteur S($champ), ou -1. */
static Tcl_Obj *
WrapSlotHint(Tcl_Interp *interp, TclType *tp, Tcl_Obj *content, int hint)
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
	    && TclTypeValidate(interp, tp, content, hint, &variant) != TCL_OK) {
	return NULL;
    }
    /*
     * Un élément déjà enveloppé dans V (lindex l'a typé une fois pour
     * toutes) : la nouvelle annotation se pose sur son CONTENU, jamais sur
     * l'objet annoté (un A dans un A se lirait comme un seul élément :
     * $P(0.x) relisait tout le point, set P(0.x) le remplaçait).
     */
    if (TclIsAnnotated(content)) {
	/* v17a : ses notes passent sur la nouvelle valeur */
	return TclAnnotCarryNotes(content, TclNewAnnotatedShared(interp, tp,
		variant, TCL_ANNOT_CONTENT(content)));
    }
    return TclNewAnnotatedShared(interp, tp, variant, content);
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

	if (TclSumTagged(tp)) {		/* v16w : "v charge" */
	    Tcl_Obj *pl = NULL;

	    variant = TclSumDecode(tp, v, &pl);
	    if (pl != NULL) {
		v = pl;
	    }
	}
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
	    return TclNewAnnotated(interp, rt, (int) j, TclSumTagged(rt)
		    ? rt->fields[j].literal : v);
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
	if (TclSumTagged(rt)) {
	    Tcl_Obj *w = TclSumMake(rt, (int) j, c);

	    res = TclNewAnnotated(interp, rt, (int) j, w);
	    TclDiscard(w);
	} else {
	    res = TclNewAnnotated(interp, rt, (int) j, c);
	}
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
	    child = WrapSlotHint(interp, slot, elems[i], tp->kind == TCLT_PRODUCT
		    ? TclTypeTagVariantFrom(tp, i, elems, n) : -1);
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
	return WrapSlotHint(interp, tp->fields[idx].type, elem,
		TclTypeTagVariant(tp, idx, v));
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

	if (TclSumTagged(tp)) {
	    /* v16w : la valeur dit sa variante ; la charge est son second
	     * élément */
	    Tcl_Obj *pl = NULL;

	    variant = TclSumDecode(tp, v, &pl);
	    if (variant >= 0 && pl != NULL) {
		v = pl;
	    }
	}
	idx = TclTypeFieldIndex(tp, name);
	if ((idx < 0 || tp->fields[idx].type == NULL) && variant >= 0
		&& variant < tp->nfields && tp->fields[variant].type != NULL
		&& (tp->fields[variant].type->kind == TCLT_PRODUCT
		    || tp->fields[variant].type->kind == TCLT_LIST
		    || tp->fields[variant].type->kind == TCLT_DICT)
		&& !(quoted && tp->fields[variant].type->kind != TCLT_DICT)) {
	    /* v16w : un nom qui n'est pas une variante : un pas dans la
	     * charge de la variante courante ($f(w) d'un forme.rect) */
	    Tcl_Obj *pa = TclNewAnnotated(interp, tp->fields[variant].type,
		    -1, v);

	    Tcl_IncrRefCount(pa);
	    res = Step(interp, pa, name, quoted);
	    Tcl_DecrRefCount(pa);
	    return res;
	}
	if (idx < 0 || tp->fields[idx].type == NULL) {
	    PathErr(interp, "%s has no variant \"%s\"", tp->name, name);
	    return NULL;
	}
	ft = tp->fields[idx].type;
	if (idx == variant) {
	    return WrapSlot(interp, ft, v);	/* une somme imbriquée : sa
						 * variante */
	}
	/*
	 * Autre variante : le nom demandé vaut cast explicite (§10), de la
	 * valeur de la variante COURANTE vers le type demandé ({i}5 lu par
	 * $v(s) donne "5"). Sans cast possible : un message qui nomme les deux
	 * variantes.
	 */
	{
	    Tcl_Obj *c;

	    if (TclTypeVariantConvert(interp, tp, variant, v, (int) idx, &c)
		    != TCL_OK) {
		return NULL;
	    }
	    res = TclNewAnnotated(interp, ft, -1, c);
	    TclDiscard(c);
	    return res;
	}
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



/*
 * 23/11 : intervalle a:b:c (parties omises : texte vide). RangeA, RangeB :
 * les bornes d'un intervalle sans pas (0, end par défaut). RangeSpec : les
 * indices first, first+stride, ... (count) parmi n, à la Python : bornes
 * ramenées dans [0, n-1] en gardant l'alignement, la fin incluse.
 */
static const char *
RangeA(const IndexStep *s)
{
    const char *t = Tcl_GetString(s->a);

    return *t ? t : "0";
}

static const char *
RangeB(const IndexStep *s)
{
    const char *t = Tcl_GetString(s->b);

    return *t ? t : "end";
}

static int
RangeStrided(const IndexStep *s)
{
    return s->kind == IS_RANGE && RANGE_SPECD(s);
}

static int
RangeSpec(Tcl_Interp *interp, const IndexStep *s, Tcl_Size n,
	Tcl_Size *firstPtr, Tcl_Size *countPtr, Tcl_Size *stridePtr)
{
    Tcl_WideInt st = 1;
    const char *a, *b;
    Tcl_Size first, last, count;

    if (s->wstarts != NULL) {
	/* une fenêtre : la colonne wj, autant d'éléments que de fenêtres
	 * complètes (chaque début avec le même pas ; la fin donnée borne le
	 * premier) */
	IndexStep sub = *s;
	Tcl_Obj **sv, *empty;
	Tcl_Size ns, k, f, c, str, cnt = -1, fj = 0, sj = 1;
	int rc = TCL_OK;

	if (Tcl_ListObjGetElements(interp, s->wstarts, &ns, &sv) != TCL_OK) {
	    return TCL_ERROR;
	}
	empty = Tcl_NewObj();
	Tcl_IncrRefCount(empty);
	sub.wstarts = NULL;
	for (k = 0; k < ns && rc == TCL_OK; k++) {
	    sub.a = sv[k];
	    sub.b = (k == 0) ? s->wbound : empty;
	    rc = RangeSpec(interp, &sub, n, &f, &c, &str);
	    if (rc == TCL_OK) {
		if (*Tcl_GetString(sv[k]) == '\0') {
		    rc = TCL_ERROR;	/* (le lexique l'interdit déjà) */
		    PathErr(interp, "a window needs explicit starts%s%s", "", "");
		    break;
		}
		if (cnt < 0 || c < cnt) {
		    cnt = c;
		}
		if (k == s->wj) {
		    fj = f;
		}
		sj = str;
	    }
	}
	Tcl_DecrRefCount(empty);
	if (rc != TCL_OK) {
	    return TCL_ERROR;
	}
	*firstPtr = fj;
	*countPtr = (cnt < 0) ? 0 : cnt;
	*stridePtr = sj;
	return TCL_OK;
    }
    a = Tcl_GetString(s->a);
    b = Tcl_GetString(s->b);

    if (s->c != NULL && *Tcl_GetString(s->c) != '\0'
	    && (Tcl_GetWideIntFromObj(NULL, s->c, &st) != TCL_OK || st == 0)) {
	PathErr(interp, "the step of a range must be a non-zero integer, "
		"not \"%s\"%s", Tcl_GetString(s->c), "");
	return TCL_ERROR;
    }
    if (*a == '\0') {
	first = (st > 0) ? 0 : n - 1;
    } else if (ListIndex(interp, a, n, &first) != TCL_OK) {
	return TCL_ERROR;
    }
    if (*b == '\0') {
	last = (st > 0) ? n - 1 : 0;
    } else if (ListIndex(interp, b, n, &last) != TCL_OK) {
	return TCL_ERROR;
    }
    if (st > 0) {
	if (first < 0) {
	    first += ((-first + st - 1) / st) * st;
	}
	if (last >= n) {
	    last = n - 1;
	}
	count = (first > last) ? 0 : (last - first) / st + 1;
    } else {
	if (first >= n) {
	    first -= ((first - (n - 1) - st - 1) / -st) * -st;
	}
	if (last < 0) {
	    last = 0;
	}
	count = (first < last) ? 0 : (first - last) / -st + 1;
    }
    *firstPtr = first;
    *countPtr = count;
    *stridePtr = (Tcl_Size) st;
    return TCL_OK;
}

/* Le même, en positions de bits depuis le poids faible (lo, pas). */
static int
BitsStride(Tcl_Interp *interp, const TclType *tp, const IndexStep *s,
	Tcl_Size *loPtr, Tcl_Size *countPtr, Tcl_Size *stepPtr)
{
    Tcl_Size first, stride;

    if (RangeSpec(interp, s, tp->bitWidth, &first, countPtr, &stride)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    *loPtr = tp->bitMsb0 ? tp->bitWidth - 1 - first : first;
    *stepPtr = tp->bitMsb0 ? -stride : stride;
    return TCL_OK;
}

static int
StepRange(Tcl_Interp *interp, Tcl_Obj *a, const IndexStep *st, Items *out)
{
    Tcl_Obj *v, *elem;
    TclType *tp;
    int variant;
    Tcl_Size n, i, lo, hi;
    const char *from = RangeA(st), *to = RangeB(st);

    if (RANGE_SPECD(st) && TclAnnotInfo(a, &v, &tp, &variant, NULL) == 1
	    && tp->kind == TCLT_INT && tp->bitWidth > 0) {
	/* b(a:b:c) : les bits choisis, en un bitarray de count bits */
	Tcl_Obj *bits, *w;
	Tcl_Size blo, count, bstep;

	if (BitsStride(interp, tp, st, &blo, &count, &bstep) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (count == 0) {
	    PathErr(interp, "bit range \"%s\" selects no bit%s",
		    Tcl_GetString(st->a), "");
	    return TCL_ERROR;
	}
	if (TclBitsGather(interp, v, blo, bstep, count, tp->bitMsb0, &bits)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	w = TclNewAnnotated(interp, BitsSliceType(interp, tp, count), -1, bits);
	TclDiscard(bits);
	if (w == NULL) {
	    return TCL_ERROR;
	}
	TclVecAdd(&out->vec, w);
	out->bitSlice = 1;
	return TCL_OK;
    }

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
    if (RANGE_SPECD(st)) {
	Tcl_Size count, stride, k;

	if (Tcl_ListObjLength(interp, v, &n) != TCL_OK
		|| RangeSpec(interp, st, n, &lo, &count, &stride) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (k = 0, i = lo; k < count; k++, i += stride) {
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
    case IS_ALL: {
	/* "*" : chaque partie, dans l'ordre (champs d'un produit, éléments
	 * d'une liste, valeurs d'un dict) ; les pas suivants s'appliquent à
	 * chacune. */
	Tcl_Obj *v, **elv, *key, *val;
	TclType *tp;
	Tcl_Size k, ne;
	Tcl_DictSearch ds;
	int done, variant;
	char ibuf[32];

	if (TclAnnotInfo(a, &v, &tp, &variant, NULL) != 1) {
	    PathErr(interp, "\"*\" needs a typed container%s%s", "", "");
	    return TCL_ERROR;
	}
	switch (tp->kind) {
	case TCLT_PRODUCT:
	    for (k = 0; k < tp->nfields; k++) {
		r = Step(interp, a, Tcl_GetString(tp->fields[k].name), 0);
		if (r == NULL) return TCL_ERROR;
		rc = ReadSteps(interp, r, p, i + 1, out);
		TclDiscard(r);
		if (rc != TCL_OK) return rc;
	    }
	    return TCL_OK;
	case TCLT_LIST:
	    if (Tcl_ListObjGetElements(interp, v, &ne, &elv) != TCL_OK) {
		return TCL_ERROR;
	    }
	    for (k = 0; k < ne; k++) {
		snprintf(ibuf, sizeof(ibuf), "%" TCL_SIZE_MODIFIER "d", k);
		r = Step(interp, a, ibuf, 0);
		if (r == NULL) return TCL_ERROR;
		rc = ReadSteps(interp, r, p, i + 1, out);
		TclDiscard(r);
		if (rc != TCL_OK) return rc;
	    }
	    return TCL_OK;
	case TCLT_DICT:
	    if (Tcl_DictObjFirst(interp, v, &ds, &key, &val, &done) != TCL_OK) {
		return TCL_ERROR;
	    }
	    for (; !done; Tcl_DictObjNext(&ds, &key, &val, &done)) {
		r = Step(interp, a, Tcl_GetString(key), 1);
		if (r == NULL) { Tcl_DictObjDone(&ds); return TCL_ERROR; }
		rc = ReadSteps(interp, r, p, i + 1, out);
		TclDiscard(r);
		if (rc != TCL_OK) { Tcl_DictObjDone(&ds); return rc; }
	    }
	    return TCL_OK;
	default:
	    PathErr(interp, "\"*\" needs a product, a list or a dict, not %s%s",
		    tp->name, "");
	    return TCL_ERROR;
	}
    }
    case IS_RANGE: {
	Items part;
	Tcl_Size k;

	ItemsInit(&part);
	if (StepRange(interp, a, step, &part) != TCL_OK) {
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
	if (step->kind == IS_NAME) {
	    /* Forme compacte : L(i.x.y) se lit dans la case, sans fabriquer
	     * l'élément entier. */
	    Tcl_Obj *v;
	    TclType *tp, *pt;
	    Tcl_Size n, idx, j, nn = 0;
	    const char *names[16];

	    if (TclAnnotInfo(a, &v, &tp, NULL, NULL) == 1
		    && tp->kind == TCLT_LIST && TclIsCompact(v)
		    && Tcl_ListObjLength(NULL, v, &n) == TCL_OK
		    && ListIndex(NULL, Tcl_GetString(step->a), n, &idx) == TCL_OK
		    && idx >= 0 && idx < n) {
		for (j = i + 1; j < p->n && nn < 16
			&& p->step[j].kind == IS_NAME; j++) {
		    names[nn++] = Tcl_GetString(p->step[j].a);
		}
		r = (j == p->n) ? TclCompactReadPath(v, idx, nn, names, &pt)
			: NULL;
		if (r != NULL) {
		    Tcl_Obj *w = TclNewAnnotated(interp, pt, -1, r);

		    TclDiscard(r);
		    TclVecAdd(&out->vec, w);
		    return TCL_OK;
		}
	    }
	}
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
 * Un chemin de noms (a.b.c, L(3.x)) au travers de produits et de listes
 * dont les parts sont « simples » : on descend dans les contenus sans
 * envelopper les étapes intermédiaires, et seul le résultat reçoit son
 * type. NULL (sans message) dès que le chemin sort de ce cas : le chemin
 * général (ReadSteps) fait alors la lecture, ou donne l'erreur.
 */
static int SimpleFieldType(const TclType *ft);
static Tcl_Obj *FieldAt(Tcl_Interp *interp, Tcl_Obj *v, TclType *ft,
	Tcl_Size idx);

static Tcl_Obj *
FastChain(Tcl_Interp *interp, Tcl_Obj *a, const IndexPath *p)
{
    Tcl_Obj *v, *e = NULL;
    TclType *tp;
    Tcl_Size i, idx = -1, n;
    int variant;

    if (p->n < 1 || !TclAnnotIsSettled(a)
	    || TclAnnotInfo(a, &v, &tp, &variant, NULL) != 1) {
	return NULL;
    }
    for (i = 0; i < p->n; i++) {
	const IndexStep *st = &p->step[i];

	if (st->kind != IS_NAME || st->atom || TclIsCompact(v)) {
	    return NULL;
	}
	if (i > 0 && !SimpleFieldType(tp)) {
	    return NULL;
	}
	switch (tp->kind) {
	case TCLT_PRODUCT: {
	    const char *name = Tcl_GetString(st->a);

	    idx = TclTypeFieldIndex(tp, name);
	    if (idx < 0 && IsIntegerName(name)) {
		idx = atol(name);
	    }
	    if (idx < 0 || idx >= tp->nfields) {
		return NULL;
	    }
	    if (Tcl_ListObjIndex(NULL, v, idx, &e) != TCL_OK || e == NULL) {
		return NULL;
	    }
	    tp = tp->fields[idx].type;
	    break;
	}
	case TCLT_LIST:
	    if (Tcl_ListObjLength(NULL, v, &n) != TCL_OK
		    || TclGetIntForIndexM(NULL, st->a, n - 1, &idx) != TCL_OK
		    || idx < 0 || idx >= n
		    || Tcl_ListObjIndex(NULL, v, idx, &e) != TCL_OK
		    || e == NULL) {
		return NULL;
	    }
	    tp = tp->elem;
	    break;
	default:
	    return NULL;
	}
	if (i == p->n - 1) {
	    /* le dernier pas : typé une fois pour toutes dans le contenu */
	    if (!SimpleFieldType(tp) || tp->kind == TCLT_VAR) {
		return NULL;
	    }
	    return FieldAt(interp, v, tp, idx);
	}
	if (TclIsAnnotated(e)) {
	    if (!SimpleFieldType(tp)) {
		return NULL;	/* garde sa propre annotation : chemin général */
	    }
	    e = TCL_ANNOT_CONTENT(e);
	}
	v = e;
    }
    if (tp->kind == TCLT_VAR) {
	return NULL;
    }
    if (SimpleFieldType(tp)) {
	return TclNewAnnotatedShared(interp, tp, -1, v);
    }
    return NULL;
}

/*
 * Lecture : *resultPtr reçoit une valeur typée, avec une référence de
 * plus que l'appelant doit rendre (Tcl_DecrRefCount). Un résultat unique
 * garde le type du champ ; une collecte donne une liste {list}T, T étant
 * le type commun des éléments (§10).
 */
static int
PathHasRange(const IndexPath *p)
{
    Tcl_Size i;

    for (i = 0; i < p->n; i++) {
	if (p->step[i].kind == IS_RANGE || p->step[i].kind == IS_ALL) {
	    return 1;
	}
    }
    return 0;
}

/* Liste typée {list}T de valeurs typées, T leur type commun. */
static Tcl_Obj *
TypedListOf(Tcl_Interp *interp, Tcl_Obj **v, Tcl_Size n)
{
    TclObjVec contents = {NULL, 0, 0};
    TclType *common = NULL, *tp;
    Tcl_Obj *content, *list, *a;
    Tcl_Size i;

    TclType *first = NULL;
    int mixed = 0;

    for (i = 0; i < n; i++) {
	tp = NULL;
	content = v[i];
	TclAnnotInfo(v[i], &content, &tp, NULL, NULL);
	if (tp != NULL) {
	    if (first == NULL) {
		first = tp;
	    } else if (!TclTypeSame(first, tp)) {
		mixed = 1;
	    }
	    common = (common == NULL) ? tp : TclTypeCommon(interp, common, tp);
	}
	TclVecAdd(&contents, content);
    }
    if (mixed) {
	/* Types différents (les champs d'un produit, $P(*)) : list.any,
	 * chaque élément garde son propre type. */
	TclVecFree(&contents);
	for (i = 0; i < n; i++) {
	    TclVecAdd(&contents, v[i]);
	}
	common = TclTypeParse(interp, "any", 3);
    }
    if (common == NULL) {
	common = TclTypeParse(interp, "string", 6);
    }
    list = Tcl_NewListObj(contents.n, contents.v);
    TclVecFree(&contents);
    a = TclNewAnnotated(interp, TclTypeListOf(interp, common), -1, list);
    TclDiscard(list);
    return a;
}

/* Résultat selon la forme ; *resPtr sans référence ajoutée (refcount 0
 * possible) : l'appelant l'incrémente. */
static Tcl_Obj *
ShapeRead(Tcl_Interp *interp, const Shape *sh, Items *per,
	const PathSet *set)
{
    if (sh->leaf >= 0) {
	Items *it = &per[sh->leaf];

	if (it->vec.n == 1 && (!PathHasRange(&set->path[sh->leaf])
		|| it->bitSlice)) {
	    return it->vec.v[0];
	}
	return TypedListOf(interp, it->vec.v, it->vec.n);
    } else {
	TclObjVec kids = {NULL, 0, 0};
	Tcl_Obj *res;
	Tcl_Size i;

	for (i = 0; i < sh->n; i++) {
	    Tcl_Obj *k;

	    if (sh->kid[i].leaf >= 0 && sh->kid[i].splat) {
		/* *a:b : les éléments de l'intervalle, un par un */
		Items *it = &per[sh->kid[i].leaf];
		Tcl_Size m;

		for (m = 0; m < it->vec.n; m++) {
		    TclVecAdd(&kids, it->vec.v[m]);
		}
		continue;
	    }
	    k = ShapeRead(interp, &sh->kid[i], per, set);
	    if (k == NULL) {
		TclVecFree(&kids);
		return NULL;
	    }
	    TclVecAdd(&kids, k);
	}
	res = TypedListOf(interp, kids.v, kids.n);
	TclVecFree(&kids);
	return res;
    }
}

/*
 * Lecture : *resultPtr reçoit une valeur typée, avec une référence de
 * plus que l'appelant doit rendre (Tcl_DecrRefCount). Un résultat unique
 * garde le type du champ ; une collecte donne une liste {list}T, T étant
 * le type commun des éléments (§10), imbriquée selon la forme du chemin.
 */
static int
PathRead(Tcl_Interp *interp, Tcl_Obj *containerPtr, const char *path,
	Tcl_Obj *pathObj, Tcl_Obj **resultPtr)
{
    CompiledPath *cp;
    const PathSet *set;
    Items *per;
    Tcl_Size k, m;
    Tcl_Obj *res;
    int rc = TCL_ERROR;

    {
	/* Le cas courant, $a(x), $L(3) : un seul nom, un seul pas, sans
	 * compiler l'index (ni portées substituées, ni opérateur). */
	const char *c;
	const Tcl_Size *pos;

	for (c = path; *c != '\0'; c++) {
	    if (!(isalnum(UCHAR(*c)) || *c == '_')) {
		break;
	    }
	}
	if (c != path && *c == '\0' && PathIndexSpans(pathObj, &pos) == 0) {
	    Tcl_Obj *content, *target;
	    TclType *vt;

	    res = Step(interp, containerPtr, path, 0);
	    if (res == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_IncrRefCount(res);
	    /* Une case var en fin de chemin se lit à travers son lien. */
	    if (TclAnnotInfo(res, &content, &vt, NULL, NULL) == 1
		    && vt->kind == TCLT_VAR) {
		target = DerefSlot(interp, vt, content);
		Tcl_DecrRefCount(res);
		if (target == NULL) {
		    return TCL_ERROR;
		}
		Tcl_IncrRefCount(target);
		res = target;
	    }
	    *resultPtr = res;
	    return TCL_OK;
	}
    }
    cp = GetCompiledPath(interp, path, pathObj);
    if (cp == NULL) {
	return TCL_ERROR;
    }
    set = &cp->set;
    if (cp->chain) {
	/* un seul chemin de noms : ni Items ni forme */
	res = FastChain(interp, containerPtr, &set->path[0]);
	if (res != NULL) {
	    Tcl_IncrRefCount(res);
	    *resultPtr = res;
	    CompiledPathRelease(cp);
	    return TCL_OK;
	}
    }
    per = (Items *) Tcl_Alloc((set->n ? set->n : 1) * sizeof(Items));
    for (k = 0; k < set->n; k++) {
	ItemsInit(&per[k]);
    }
    for (k = 0; k < set->n; k++) {
	Tcl_Obj *quick = FastChain(interp, containerPtr, &set->path[k]);

	if (quick != NULL) {
	    TclVecAdd(&per[k].vec, quick);
	    continue;
	}
	if (ReadSteps(interp, containerPtr, &set->path[k], 0, &per[k])
		!= TCL_OK) {
	    goto done;
	}
	/* Une case var en fin de chemin se lit à travers son lien. */
	for (m = 0; m < per[k].vec.n; m++) {
	    Tcl_Obj *content, *target;
	    TclType *vt;

	    if (TclAnnotInfo(per[k].vec.v[m], &content, &vt, NULL, NULL) == 1
		    && vt->kind == TCLT_VAR) {
		target = DerefSlot(interp, vt, content);
		if (target == NULL) {
		    goto done;
		}
		Tcl_IncrRefCount(target);
		Tcl_DecrRefCount(per[k].vec.v[m]);
		per[k].vec.v[m] = target;
	    }
	}
    }
    res = ShapeRead(interp, &set->shape, per, set);
    if (res == NULL) {
	goto done;
    }
    Tcl_IncrRefCount(res);
    *resultPtr = res;
    rc = TCL_OK;

  done:
    for (k = 0; k < set->n; k++) {
	TclVecFree(&per[k].vec);
    }
    Tcl_Free(per);
    CompiledPathRelease(cp);
    return rc;
}

/* --------------------------------------------------------------------
 * Répartition des valeurs selon la forme (écriture, construction)
 * -------------------------------------------------------------------- */

static int
ShapeDistribute(Tcl_Interp *interp, const Shape *sh, Tcl_Obj *v,
	Tcl_Obj **flat)
{
    Tcl_Obj **elv;
    Tcl_Size n, i;

    if (sh->leaf >= 0) {
	flat[sh->leaf] = v;
	return TCL_OK;
    }
    if (Tcl_ListObjGetElements(interp, v, &n, &elv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (n != sh->n) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("value \"%s\" has %"
		TCL_SIZE_MODIFIER "d elements, but this part of the path "
		"selects %" TCL_SIZE_MODIFIER "d", Tcl_GetString(v), n, sh->n));
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "PATH", (char *) NULL);
	return TCL_ERROR;
    }
    for (i = 0; i < n; i++) {
	if (ShapeDistribute(interp, &sh->kid[i], elv[i], flat) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 * Une valeur par feuille, par élément du groupe de tête, ou pour tout le
 * chemin. *flatPtr (set->n cases, à libérer par Tcl_Free) : valeurs sans
 * référence ajoutée, vivantes tant que vals[] l'est.
 */
static int
FlattenValues(Tcl_Interp *interp, const PathSet *set, Tcl_Size nvals,
	Tcl_Obj *const vals[], Tcl_Obj ***flatPtr)
{
    Tcl_Obj **flat = (Tcl_Obj **) Tcl_Alloc((set->n ? set->n : 1)
	    * sizeof(Tcl_Obj *));
    Tcl_Size i;
    const Shape *sh = &set->shape;

    if (nvals == set->n) {
	for (i = 0; i < nvals; i++) {
	    flat[i] = vals[i];
	}
    } else if (sh->leaf < 0 && nvals == sh->n) {
	for (i = 0; i < nvals; i++) {
	    if (ShapeDistribute(interp, &sh->kid[i], vals[i], flat) != TCL_OK) {
		Tcl_Free(flat);
		return TCL_ERROR;
	    }
	}
    } else if (sh->leaf < 0 && nvals == 1) {
	if (ShapeDistribute(interp, sh, vals[0], flat) != TCL_OK) {
	    Tcl_Free(flat);
	    return TCL_ERROR;
	}
    } else {
	if (sh->leaf < 0) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("wrong # args: the path "
		    "selects %" TCL_SIZE_MODIFIER "d targets in %"
		    TCL_SIZE_MODIFIER "d groups: give %" TCL_SIZE_MODIFIER
		    "d values, %" TCL_SIZE_MODIFIER "d or 1, not %"
		    TCL_SIZE_MODIFIER "d", set->n, sh->n, set->n, sh->n, nvals));
	} else {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("wrong # args: %"
		    TCL_SIZE_MODIFIER "d targets but %" TCL_SIZE_MODIFIER
		    "d values", set->n, nvals));
	}
	Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
	Tcl_Free(flat);
	return TCL_ERROR;
    }
    *flatPtr = flat;
    return TCL_OK;
}

/* --------------------------------------------------------------------
 * Écriture : reconstruit les objets le long du chemin (valeurs
 * immuables), le reste est partagé.
 * -------------------------------------------------------------------- */

/*
 * 23/11 : b(a:b:c) = valeur. Un bitarray (valeur typée) donne un motif de
 * count bits ; 0 ou 1 vaut pour tous les bits choisis ; sinon une liste de
 * count bits.
 */
static Tcl_Obj *
BitsStrideSet(Tcl_Interp *interp, TclType *tp, Tcl_Obj *v,
	const IndexStep *step, Tcl_Obj *val)
{
    Tcl_Size lo, count, bstep, wd = tp->bitWidth / 60 + 2, n, k;
    Tcl_Obj *nv = NULL, *res, *vc, **elems;
    TclType *vt;
    int b;

    if (BitsStride(interp, tp, step, &lo, &count, &bstep) != TCL_OK) {
	return NULL;
    }
    if (TclAnnotInfo(val, &vc, &vt, NULL, NULL) == 1
	    && vt->kind == TCLT_INT) {
	if (vt->bitWidth > 0 && vt->bitWidth != count) {
	    char num[64];

	    snprintf(num, sizeof(num), "%lld bits, not %lld",
		    (long long) count, (long long) vt->bitWidth);
	    PathErr(interp, "the bit range selects %s%s", num, "");
	    return NULL;
	}
	if (TclBitsPutStride(interp, v, lo, bstep, count,
		vt->bitWidth > 0 ? vt->bitMsb0 : tp->bitMsb0, vc, 0,
		wd, &nv) != TCL_OK) {
	    return NULL;
	}
    } else if (Tcl_ListObjGetElements(NULL, val, &n, &elems) == TCL_OK
	    && n == 1 && Tcl_GetBooleanFromObj(NULL, elems[0], &b) == TCL_OK
	    && (strcmp(Tcl_GetString(elems[0]), "0") == 0
		|| strcmp(Tcl_GetString(elems[0]), "1") == 0)) {
	if (TclBitsPutStride(interp, v, lo, bstep, count, 0, NULL, b, wd,
		&nv) != TCL_OK) {
	    return NULL;
	}
    } else {
	if (Tcl_ListObjGetElements(NULL, val, &n, &elems) != TCL_OK
		|| n != count) {
	    char num[64];

	    snprintf(num, sizeof(num), "%lld", (long long) count);
	    PathErr(interp, "a strided bit range takes 0, 1, a bitarray or "
		    "a list of %s bits, not \"%s\"", num, Tcl_GetString(val));
	    return NULL;
	}
	for (k = 0; k < n; k++) {
	    const char *t = Tcl_GetString(elems[k]);

	    if ((t[0] != '0' && t[0] != '1') || t[1] != '\0') {
		PathErr(interp, "expected a bit 0 or 1 but got \"%s\"%s", t,
			"");
		return NULL;
	    }
	}
	if (TclBitsPutStride(interp, v, lo, bstep, 0, 0, NULL, 0, wd, &nv)
		!= TCL_OK) {
	    return NULL;
	}
	for (k = 0; k < n; k++) {
	    TclBitsSetInPlace(nv, lo + k * bstep,
		    Tcl_GetString(elems[k])[0] == '1', wd);
	}
    }
    res = TclNewAnnotated(interp, tp, -1, nv);
    TclDiscard(nv);
    return res;
}

/*
 * 23/11 : L(a:b:c) = valeurs, une par élément choisi, ou une seule pour
 * tous (si elle est du type des éléments, ou s'il n'y a qu'un élément).
 * La longueur ne change pas.
 */
static Tcl_Obj *
ListRangeSet(Tcl_Interp *interp, TclType *tp, Tcl_Obj *v,
	const IndexStep *step, Tcl_Obj *val)
{
    Tcl_Size n, first, count, stride, nv, k, i;
    Tcl_Obj **vals, *nl, *res, *one = NULL;
    int trusted = 0;

    if (Tcl_ListObjLength(interp, v, &n) != TCL_OK) {
	return NULL;
    }
    if (RANGE_SPECD(step)) {
	if (RangeSpec(interp, step, n, &first, &count, &stride) != TCL_OK) {
	    return NULL;
	}
    } else {
	Tcl_Size hi;

	if (ListIndex(interp, RangeA(step), n, &first) != TCL_OK
		|| ListIndex(interp, RangeB(step), n, &hi) != TCL_OK) {
	    return NULL;
	}
	if (first < 0) {
	    first = 0;
	}
	if (hi >= n) {
	    hi = n - 1;
	}
	count = (first > hi) ? 0 : hi - first + 1;
	stride = 1;
    }
    if (count == 1) {
	one = val;
    } else if (TclAnnotInfo(val, NULL, NULL, NULL, NULL) != 1
	    && tp->elem->kind != TCLT_LIST && tp->elem->kind != TCLT_PRODUCT
	    && tp->elem->kind != TCLT_DICT
	    && Tcl_ListObjLength(NULL, val, &nv) == TCL_OK && nv == 1) {
	one = val;			/* un scalaire : pour tous */
    } else {
	TclType *vt = NULL;

	if (TclAnnotInfo(val, NULL, &vt, NULL, NULL) == 1 && vt == tp->elem) {
	    one = val;			/* typé comme un élément : pour tous */
	}
    }
    if (one == NULL) {
	Tcl_Obj *lv = val;
	TclType *vlt = NULL;

	if (TclAnnotInfo(val, &lv, &vlt, NULL, NULL) != 1) {
	    lv = val;
	    vlt = NULL;
	}
	/* 23/11 : une liste déjà du type des éléments (list.T, T scalaire
	 * simple) : ses éléments sont valides, pas de vérification une à une */
	trusted = (vlt != NULL && vlt->kind == TCLT_LIST
		&& vlt->elem == tp->elem && TclTypePlainScalar(tp->elem));
	if (trusted && (TclIsCompact(v) || TclIsCompactView(v))) {
	    /* 24/11 : des cases dans des cases, de même disposition : copiées
	     * octet à octet (une liste neuve), sans objet par élément */
	    Tcl_Size tn, sn;
	    const unsigned char *td, *sd;
	    unsigned char *dd;
	    size_t ts, ss, ds;
	    int tk, tz, sk, sz, dk, dz;
	    Tcl_Obj *cl;

	    if (TclCompactScalarInfo(v, &tn, &td, &ts, &tk, &tz)
		    && TclCompactScalarInfo(lv, &sn, &sd, &ss, &sk, &sz)
		    && tk == sk && tz == sz && sn == count
		    && (cl = TclCompactNewAlways(tp, tn, &dd)) != NULL) {
		Tcl_Size dn;
		const unsigned char *d0;

		if (TclCompactScalarInfo(cl, &dn, &d0, &ds, &dk, &dz)
			&& dk == tk && dz == tz) {
		    dd = (unsigned char *) d0;
		    for (i = 0; i < tn; i++) {
			memcpy(dd + (size_t) i * ds, td + (size_t) i * ts,
				(size_t) tz);
		    }
		    for (k = 0, i = first; k < count; k++, i += stride) {
			memcpy(dd + (size_t) i * ds, sd + (size_t) k * ss,
				(size_t) tz);
		    }
		    res = TclNewAnnotated(interp, tp, -1, cl);
		    TclDiscard(cl);
		    return res;
		}
		TclDiscard(cl);
	    }
	}
	if (Tcl_ListObjGetElements(interp, lv, &nv, &vals) != TCL_OK) {
	    return NULL;
	}
	if (nv != count) {
	    char num[64];

	    snprintf(num, sizeof(num), "%lld values (or one for all), not "
		    "%lld", (long long) count, (long long) nv);
	    PathErr(interp, "the range selects %s%s", num, "");
	    return NULL;
	}
    }
    if (one != NULL) {
	one = TclTypedConform(interp, tp->elem, one);	/* une fois */
	if (one == NULL) {
	    return NULL;
	}
	Tcl_IncrRefCount(one);
    }
    nl = Tcl_DuplicateObj(v);
    Tcl_IncrRefCount(nl);
    for (k = 0, i = first; k < count; k++, i += stride) {
	Tcl_Obj *c = one ? one : (trusted && !TclIsAnnotated(vals[k]))
		? vals[k] : TclTypedConform(interp, tp->elem, vals[k]);

	if (c == NULL) {
	    Tcl_DecrRefCount(nl);
	    return NULL;
	}
	if (TclIsCompact(nl) ? !TclCompactWriteScalar(nl, i, 0, NULL, c)
		&& Tcl_ListObjReplace(interp, nl, i, 1, 1, &c) != TCL_OK
		: Tcl_ListObjReplace(interp, nl, i, 1, 1, &c) != TCL_OK) {
	    if (c == one) {
		Tcl_DecrRefCount(one);
	    } else if (c != vals[k]) {
		TclDiscard(c);
	    }
	    Tcl_DecrRefCount(nl);
	    return NULL;
	}
	if (c != one && c != vals[k]) {
	    TclDiscard(c);
	}
    }
    if (one != NULL) {
	Tcl_DecrRefCount(one);
    }
    TclInvalidateStringRep(nl);
    res = TclNewAnnotated(interp, tp, -1, nl);
    Tcl_DecrRefCount(nl);
    return res;
}

static Tcl_Obj *
SetIn(Tcl_Interp *interp, Tcl_Obj *a, const IndexPath *chain, Tcl_Size i,
	Tcl_Obj *val, PendingWrites *pw)
{
    const IndexStep *step = &chain->step[i];
    int last = (i == chain->n - 1);

    if (step->kind == IS_ALL) {
	PathErr(interp, "\"*\" reads all the parts; it cannot be written to%s%s",
		"", "");
	return NULL;
    }
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
    if (TclSumTagged(tp)) {
	/* v16w : la valeur dit sa variante ; on travaille sur la charge, la
	 * valeur est refaite à la fin ("v charge") */
	Tcl_Obj *pl = NULL;

	variant = TclSumDecode(tp, v, &pl);
	if (variant >= 0 && pl != NULL) {
	    v = pl;
	}
    }
    if (tp->kind == TCLT_SUM && step->kind != IS_RANGE
	    && (TclTypeFieldIndex(tp, name) < 0
		|| tp->fields[TclTypeFieldIndex(tp, name)].type == NULL)
	    && variant >= 0 && variant < tp->nfields
	    && tp->fields[variant].type != NULL) {
	/* v16w : un nom qui n'est pas une variante : le même pas dans la
	 * charge de la variante courante (set f(w) 5 d'un forme.rect) */
	Tcl_Obj *pa = TclNewAnnotated(interp, tp->fields[variant].type, -1, v);
	Tcl_Obj *nc, *ncc, *w;

	Tcl_IncrRefCount(pa);
	nc = SetIn(interp, pa, chain, i, val, pw);
	Tcl_DecrRefCount(pa);
	if (nc == NULL) {
	    return NULL;
	}
	TclAnnotInfo(nc, &ncc, NULL, NULL, NULL);
	w = TclSumTagged(tp) ? TclSumMake(tp, variant, ncc)
		: Tcl_DuplicateObj(ncc);
	res = TclNewAnnotated(interp, tp, variant, w);
	TclDiscard(w);
	TclDiscard(nc);
	return res;
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
	if (RangeStrided(step)) {
	    return BitsStrideSet(interp, tp, v, step, val);
	}
	if (step->kind == IS_RANGE) {
	    if (BitsSlice(interp, tp, RangeA(step), RangeB(step), &lo, &len)
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
    if (step->kind == IS_RANGE && last && tp->kind == TCLT_LIST) {
	return ListRangeSet(interp, tp, v, step, val);
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
	if (TclIsCompact(v)) {
	    /* forme compacte : pas de tableau d'éléments (il la défairait) */
	    elems = NULL;
	    if (Tcl_ListObjLength(interp, v, &n) != TCL_OK
		    || ListIndex(interp, name, n, &idx) != TCL_OK) {
		return NULL;
	    }
	} else if (Tcl_ListObjGetElements(interp, v, &n, &elems) != TCL_OK
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
	if (idx == n && tp->lenRange && tp->lenMax >= 0
		&& n + 1 > tp->lenMax) {	/* v17g : list(2:5) */
	    char num[32];

	    snprintf(num, sizeof(num), "%lld", (long long) tp->lenMax);
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
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"the current variant of %s is \"%s\", not \"%s\": to change "
			"the variant, write the whole value ({%s}...)",
			tp->name, (variant >= 0 && variant < (int) tp->nfields
			    && tp->fields[variant].name != NULL)
			    ? Tcl_GetString(tp->fields[variant].name) : "?",
			name, name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "VARIANT",
			(char *) NULL);
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
	if (tp->kind == TCLT_PRODUCT && idx < n
		&& TclTypeTagCheck(interp, tp, idx, val, elems, n) != TCL_OK) {
	    return NULL;
	}
	{
	    int hint = tp->kind == TCLT_PRODUCT ? TclTypeTagVariant(tp, idx, v)
		    : -1;

	    if (hint < 0 && slotType->kind == TCLT_SUM && elems != NULL
		    && (tp->kind == TCLT_PRODUCT || tp->kind == TCLT_LIST)
		    && idx >= 0 && idx < n) {
		/* La variante de la partie remplacée départage des variantes
		 * également strictes, comme pour une variable (set v 8). */
		int cur = TclTypeSumCurrent(slotType, elems[idx]);

		if (cur >= 0) {
		    hint = -2 - cur;
		}
	    }
	    childContent = TclTypedConformHint(interp, slotType, val, hint);
	}
	if (childContent == NULL) {
	    return NULL;
	}
    } else {
	Tcl_Obj *old = NULL, *nc;

	if (tp->kind == TCLT_SUM) {
	    if (idx != variant) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"the current variant of %s is \"%s\", not \"%s\": to change "
			"the variant, write the whole value ({%s}...)",
			tp->name, (variant >= 0 && variant < (int) tp->nfields
			    && tp->fields[variant].name != NULL)
			    ? Tcl_GetString(tp->fields[variant].name) : "?",
			name, name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "VARIANT",
			(char *) NULL);
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
	} else if (elems == NULL) {
	    old = TclCompactElement(v, idx);	/* neuf : l'enfant le garde */
	} else {
	    old = elems[idx];
	}
	child = WrapSlotHint(interp, slotType, old, tp->kind == TCLT_PRODUCT
		? TclTypeTagVariant(tp, idx, v) : -1);
	if (child == NULL) {
	    if (elems == NULL) {
		TclDiscard(old);
	    }
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
	if (TclSumTagged(tp)) {		/* v16w : "v charge" */
	    Tcl_Obj *w = TclSumMake(tp, (int) idx, childContent);

	    res = TclNewAnnotated(interp, tp, (int) idx, w);
	    TclDiscard(w);
	} else {
	    res = TclNewAnnotated(interp, tp, (int) idx, childContent);
	}
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
	TclDiscard(key);	/* clé existante : Tcl_DictObjPut ne la garde pas */
	res = TclNewAnnotated(interp, tp, variant, newV);
	TclDiscard(newV);
	break;
    }
    default:				/* produit, liste */
	newV = Tcl_DuplicateObj(v);
	{
	    /* Une liste ordinaire garde childContent ; une forme compacte
	     * l'encode sans le garder : la référence prise ici le libère
	     * dans ce cas, et seulement dans ce cas. */
	    int rrc;

	    Tcl_IncrRefCount(childContent);
	    rrc = Tcl_ListObjReplace(interp, newV, idx, (idx < n) ? 1 : 0, 1,
		    &childContent);
	    Tcl_DecrRefCount(childContent);
	    childContent = NULL;	/* à newV, ou déjà libéré */
	    if (rrc != TCL_OK) {
		TclDiscard(newV);
		return NULL;
	    }
	}
	if (tp->kind == TCLT_PRODUCT) {
	    /* Un champ S($champ) dépend de son sélecteur : changer le
	     * sélecteur revalide le champ selon la nouvelle variante. */
	    Tcl_Size k, nn;
	    Tcl_Obj **ne;

	    /* childContent appartient désormais à newV : on ne libère que
	     * newV en cas d'erreur. */
	    if (Tcl_ListObjGetElements(interp, newV, &nn, &ne) != TCL_OK) {
		TclDiscard(newV);
		return NULL;
	    }
	    for (k = 0; k < tp->nfields && k < nn; k++) {
		TclType *fty = tp->fields[k].type;

		if (k == idx || fty == NULL || fty->kind != TCLT_SUM
			|| fty->binTag == NULL || fty->binTag[0] == '='
			|| TclTypeFieldIndex(tp, fty->binTag) != idx) {
		    continue;
		}
		if (TclTypeValidate(interp, fty, ne[k],
			TclTypeTagVariantFrom(tp, k, ne, nn), NULL) != TCL_OK) {
		    TclDiscard(newV);
		    return NULL;
		}
	    }
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
static int
PathWrite(Tcl_Interp *interp, Tcl_Obj *containerPtr, const char *path,
	Tcl_Obj *pathObj, Tcl_Size nvals, Tcl_Obj *const vals[],
	Tcl_Obj **newPtr)
{
    CompiledPath *cp;
    const PathSet *set;
    Tcl_Size i, j;
    Tcl_Obj *cur, *next;
    PendingWrites pw;
    Tcl_Obj **flat = NULL;
    int rc = TCL_ERROR;

    cp = GetCompiledPath(interp, path, pathObj);
    if (cp == NULL) {
	return TCL_ERROR;
    }
    set = &cp->set;
    memset(&pw, 0, sizeof(pw));
    if (set->n == 1 && nvals > 1 && set->path[0].n > 0
	    && set->path[0].step[set->path[0].n - 1].kind == IS_RANGE) {
	/* 23/11 : set L(a:b:c) v1 v2 ... : les valeurs de la tranche */
	Tcl_Obj *packed = Tcl_NewListObj(nvals, vals);

	Tcl_IncrRefCount(packed);
	rc = PathWrite(interp, containerPtr, path, pathObj, 1, &packed,
		newPtr);
	Tcl_DecrRefCount(packed);
	return rc;
    }
    if (FlattenValues(interp, set, nvals, vals, &flat) != TCL_OK) {
	goto done;
    }
    for (i = 0; i < set->n; i++) {
	for (j = 0; j < set->path[i].n; j++) {
	    /* une tranche a:b est admise en dernier pas : elle vise un
	     * bitarray (SetIn refuse les autres conteneurs) */
	    if (set->path[i].step[j].kind == IS_RANGE
		    && j == set->path[i].n - 1) {
		continue;
	    }
	    if (set->path[i].step[j].kind > IS_KEY) {
		PathErr(interp, "only simple names can be assigned, not "
			"ranges or tags%s%s", "", "");
		goto done;
	    }
	}
    }
    cur = containerPtr;
    Tcl_IncrRefCount(cur);
    for (i = 0; i < set->n; i++) {
	next = SetIn(interp, cur, &set->path[i], 0, flat[i], &pw);
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
    if (flat != NULL) {
	Tcl_Free(flat);
    }
    TclVecFree(&pw.names);
    TclVecFree(&pw.values);
    CompiledPathRelease(cp);
    return rc;
}

/* --------------------------------------------------------------------
 * Construction : set {T}R(chemin) v...
 *
 *	Les chemins, groupés par leur premier pas, nomment des champs de T ;
 *	un groupe qui descend plus loin construit le produit du champ. Chaque
 *	niveau passe par le constructeur en nom/valeur (défauts, contrôles),
 *	sans essai de lecture positionnelle.
 * -------------------------------------------------------------------- */

/*
 * set {segment}AB(0,1) $A $B, set {list.pt}L(0.x,0.y,1.x,1.y) ..., set
 * {dict.int}D(a,b) 1 2 : construction d'une liste par ses indices (tous, de 0
 * à N-1, dans un ordre quelconque : une liste n'a pas de défaut) ou d'un
 * dict par ses clés. Chaque élément est validé comme élément de tp.
 */
static int ConstructLevel(Tcl_Interp *interp, TclType *tp,
	const PathSet *set, Tcl_Obj **flat, const Tcl_Size *idx, Tcl_Size cnt,
	Tcl_Size d, Tcl_Obj **contentPtr, int *variantPtr);

static int
ConstructCollection(Tcl_Interp *interp, TclType *tp, const PathSet *set,
	Tcl_Obj **flat, const Tcl_Size *idx, Tcl_Size cnt, Tcl_Size d,
	Tcl_Obj **contentPtr)
{
    char *used;
    Tcl_Size i, j, *sub, nslots = 0, k;
    Tcl_Obj **slots = NULL, *res = NULL;
    int rc = TCL_ERROR, isList = (tp->kind == TCLT_LIST);
    TclType *et = tp->elem;

    used = (char *) Tcl_Alloc(cnt ? cnt : 1);
    memset(used, 0, cnt ? cnt : 1);
    sub = (Tcl_Size *) Tcl_Alloc((cnt ? cnt : 1) * sizeof(Tcl_Size));
    if (isList) {
	/* Nombre d'éléments : le plus grand indice + 1. */
	for (i = 0; i < cnt; i++) {
	    const char *nm = Tcl_GetString(set->path[idx[i]].step[d].a);
	    Tcl_WideInt w;

	    if (!IsIntegerName(nm) || Tcl_GetWideIntFromObj(NULL,
		    set->path[idx[i]].step[d].a, &w) != TCL_OK || w < 0) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("a list is built by "
			"its indices: \"%s\" is not an index of %s", nm,
			tp->name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "PATH", (char *) NULL);
		goto done;
	    }
	    if (w + 1 > nslots) {
		nslots = (Tcl_Size) w + 1;
	    }
	}
	slots = (Tcl_Obj **) Tcl_Alloc((nslots ? nslots : 1)
		* sizeof(Tcl_Obj *));
	memset(slots, 0, (nslots ? nslots : 1) * sizeof(Tcl_Obj *));
    } else {
	res = Tcl_NewDictObj();
	Tcl_IncrRefCount(res);
    }
    for (i = 0; i < cnt; i++) {
	const IndexPath *p = &set->path[idx[i]];
	const char *name;
	Tcl_Size ns = 0, whole = -1;
	Tcl_Obj *value, *c;

	if (used[i]) {
	    continue;
	}
	name = Tcl_GetString(p->step[d].a);
	for (j = i; j < cnt; j++) {
	    const IndexPath *q = &set->path[idx[j]];

	    if (!used[j] && strcmp(Tcl_GetString(q->step[d].a), name) == 0) {
		used[j] = 1;
		sub[ns++] = idx[j];
		if (q->n == d + 1) {
		    whole = idx[j];
		}
	    }
	}
	if (whole >= 0) {
	    if (ns > 1) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("element \"%s\" of %s is "
			"assigned more than once", name, tp->name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "PATH", (char *) NULL);
		goto done;
	    }
	    value = flat[whole];
	    Tcl_IncrRefCount(value);
	} else {
	    int sv = -1;

	    c = NULL;
	    if (et == NULL || ConstructLevel(interp, et, set, flat, sub, ns,
		    d + 1, &c, &sv) != TCL_OK) {
		if (et == NULL) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf("elements of %s have "
			    "no type to construct", tp->name));
		}
		goto done;
	    }
	    value = TclNewAnnotated(interp, et, sv, c);
	    TclDiscard(c);
	    Tcl_IncrRefCount(value);
	}
	/* L'élément, validé comme élément de tp. */
	c = (et != NULL) ? TclTypedConform(interp, et, value)
		: Tcl_DuplicateObj(TCL_ANNOT_CONTENT(value));
	Tcl_DecrRefCount(value);
	if (c == NULL) {
	    goto done;
	}
	if (isList) {
	    Tcl_WideInt w;

	    Tcl_GetWideIntFromObj(NULL, p->step[d].a, &w);
	    Tcl_IncrRefCount(c);
	    slots[w] = c;
	} else if (Tcl_DictObjPut(interp, res, p->step[d].a, c) != TCL_OK) {
	    TclDiscard(c);
	    goto done;
	}
    }
    if (isList) {
	for (k = 0; k < nslots; k++) {
	    if (slots[k] == NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("element %"
			TCL_SIZE_MODIFIER "d of %s is not given: a list built by "
			"its indices needs all of them, from 0 to %"
			TCL_SIZE_MODIFIER "d", k, tp->name, nslots - 1));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "PATH", (char *) NULL);
		goto done;
	    }
	}
	res = Tcl_NewListObj(nslots, slots);
	Tcl_IncrRefCount(res);
    }
    if (TclTypeComplete(interp, tp, res, -1, NULL) != TCL_OK) {
	goto done;
    }
    *contentPtr = res;
    res->refCount--;			/* refcount 0, privé */
    res = NULL;
    rc = TCL_OK;

  done:
    if (slots != NULL) {
	for (k = 0; k < nslots; k++) {
	    if (slots[k] != NULL) {
		Tcl_DecrRefCount(slots[k]);
	    }
	}
	Tcl_Free(slots);
    }
    if (res != NULL) {
	Tcl_DecrRefCount(res);
    }
    Tcl_Free(used);
    Tcl_Free(sub);
    return rc;
}

static int
ConstructLevel(Tcl_Interp *interp, TclType *tp, const PathSet *set,
	Tcl_Obj **flat, const Tcl_Size *idx, Tcl_Size cnt, Tcl_Size d,
	Tcl_Obj **contentPtr, int *variantPtr)
{
    TclObjVec args = {NULL, 0, 0};
    char *used;
    Tcl_Size i, j, *sub;
    int rc = TCL_ERROR;

    if (variantPtr != NULL) {
	*variantPtr = -1;
    }
    if (tp->kind == TCLT_SUM && cnt > 0) {
	/*
	 * v16o : set {segment}s(lien.(cible,contenu)) ... : le premier pas
	 * choisit la variante (une seule pour tous les chemins), la suite
	 * construit la valeur de son type.
	 */
	const char *vn = Tcl_GetString(set->path[idx[0]].step[d].a);
	Tcl_Size fi = TclTypeFieldIndex(tp, vn), whole = -1;
	TclType *vt;
	Tcl_Obj *c = NULL;

	for (i = 0; i < cnt; i++) {
	    const IndexPath *q = &set->path[idx[i]];

	    if (strcmp(Tcl_GetString(q->step[d].a), vn) != 0) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("a value of %s has one "
			"variant: \"%s\" and \"%s\" can't be built together",
			tp->name, vn, Tcl_GetString(q->step[d].a)));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "PATH", (char *) NULL);
		return TCL_ERROR;
	    }
	    if (q->n == d + 1) {
		whole = idx[i];
	    }
	}
	if (fi < 0 || (vt = tp->fields[fi].type) == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s has no variant \"%s\"",
		    tp->name, vn));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "VARIANT", (char *) NULL);
	    return TCL_ERROR;
	}
	if (whole >= 0) {
	    if (cnt > 1) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("variant \"%s\" of %s is "
			"assigned more than once", vn, tp->name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "PATH", (char *) NULL);
		return TCL_ERROR;
	    }
	    c = TclTypedConform(interp, vt, flat[whole]);
	    if (c == NULL) {
		return TCL_ERROR;
	    }
	} else if (ConstructLevel(interp, vt, set, flat, idx, cnt, d + 1, &c,
		NULL) != TCL_OK) {
	    if (!vt->nominal) {
		Tcl_Obj *nm = Tcl_ObjPrintf("%s.%s", tp->name, vn);

		Tcl_IncrRefCount(nm);
		TclTypeResultRename(interp, vt->name, Tcl_GetString(nm));
		Tcl_DecrRefCount(nm);
	    }
	    return TCL_ERROR;
	}
	if (TclSumTagged(tp)) {		/* v16w : "v charge" */
	    Tcl_Obj *w = TclSumMake(tp, (int) fi, c);

	    TclDiscard(c);
	    c = w;
	}
	*contentPtr = c;
	if (variantPtr != NULL) {
	    *variantPtr = (int) fi;
	}
	return TCL_OK;
    }
    if (tp->kind == TCLT_LIST || tp->kind == TCLT_DICT) {
	return ConstructCollection(interp, tp, set, flat, idx, cnt, d,
		contentPtr);
    }
    if (tp->kind != TCLT_PRODUCT) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("a constructor path names "
		"fields of products, indices of lists or keys of dicts: %s is "
		"none of these", tp->name));
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "PATH", (char *) NULL);
	return TCL_ERROR;
    }
    used = (char *) Tcl_Alloc(cnt ? cnt : 1);
    memset(used, 0, cnt ? cnt : 1);
    sub = (Tcl_Size *) Tcl_Alloc((cnt ? cnt : 1) * sizeof(Tcl_Size));
    for (i = 0; i < cnt; i++) {
	const IndexPath *p = &set->path[idx[i]];
	const char *name;
	Tcl_Size fi, ns = 0, whole = -1;
	Tcl_Obj *value;

	if (used[i]) {
	    continue;
	}
	name = Tcl_GetString(p->step[d].a);
	fi = TclTypeFieldIndex(tp, name);
	if (fi < 0) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("unknown field \"%s\" in %s",
		    name, tp->name));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "FIELD", (char *) NULL);
	    goto done;
	}
	for (j = i; j < cnt; j++) {
	    const IndexPath *q = &set->path[idx[j]];

	    if (!used[j] && strcmp(Tcl_GetString(q->step[d].a), name) == 0) {
		used[j] = 1;
		sub[ns++] = idx[j];
		if (q->n == d + 1) {
		    whole = idx[j];
		}
	    }
	}
	if (whole >= 0) {
	    if (ns > 1) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("field \"%s\" of %s is "
			"assigned more than once", name, tp->name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "FIELD", (char *) NULL);
		goto done;
	    }
	    value = flat[whole];
	    Tcl_IncrRefCount(value);
	} else {
	    TclType *ft = tp->fields[fi].type;
	    Tcl_Obj *c = NULL;
	    int sv = -1;

	    if (ft == NULL || ConstructLevel(interp, ft, set, flat, sub, ns,
		    d + 1, &c, &sv) != TCL_OK) {
		if (ft == NULL) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf("field \"%s\" of %s "
			    "has no type to construct", name, tp->name));
		}
		goto done;
	    }
	    value = TclNewAnnotated(interp, ft, sv, c);
	    TclDiscard(c);
	    if (value == NULL) {
		goto done;
	    }
	    Tcl_IncrRefCount(value);
	}
	TclVecAdd(&args, p->step[d].a);
	TclVecAdd(&args, value);
	Tcl_DecrRefCount(value);
    }
    rc = TclBuildProductNamed(interp, tp, args.n, args.v, contentPtr);

  done:
    TclVecFree(&args);
    Tcl_Free(used);
    Tcl_Free(sub);
    return rc;
}

/* *contentPtr : contenu du produit tp (refcount 0, privé). */
int
TclTypePathConstruct(Tcl_Interp *interp, TclType *tp, const char *path,
	Tcl_Size nvals, Tcl_Obj *const vals[], Tcl_Obj **contentPtr,
	int *variantPtr)
{
    CompiledPath *cp;
    const PathSet *set;
    Tcl_Obj **flat = NULL;
    Tcl_Size i, j, *idx = NULL;
    int rc = TCL_ERROR;

    cp = GetCompiledPath(interp, path, NULL);
    if (cp == NULL) {
	return TCL_ERROR;
    }
    set = &cp->set;
    for (i = 0; i < set->n; i++) {
	for (j = 0; j < set->path[i].n; j++) {
	    if (set->path[i].step[j].kind > IS_KEY) {
		PathErr(interp, "a constructor path names fields: no ranges or "
			"tags%s%s", "", "");
		goto done;
	    }
	}
    }
    if (FlattenValues(interp, set, nvals, vals, &flat) != TCL_OK) {
	goto done;
    }
    idx = (Tcl_Size *) Tcl_Alloc((set->n ? set->n : 1) * sizeof(Tcl_Size));
    for (i = 0; i < set->n; i++) {
	idx[i] = i;
    }
    rc = ConstructLevel(interp, tp, set, flat, idx, set->n, 0, contentPtr,
	    variantPtr);

  done:
    if (flat != NULL) {
	Tcl_Free(flat);
    }
    if (idx != NULL) {
	Tcl_Free(idx);
    }
    CompiledPathRelease(cp);
    return rc;
}



int
TclTypePathRead(Tcl_Interp *interp, Tcl_Obj *containerPtr, const char *path,
	Tcl_Obj **resultPtr)
{
    return PathRead(interp, containerPtr, path, NULL, resultPtr);
}

int
TclTypePathWrite(Tcl_Interp *interp, Tcl_Obj *containerPtr, const char *path,
	Tcl_Size nvals, Tcl_Obj *const vals[], Tcl_Obj **newPtr)
{
    return PathWrite(interp, containerPtr, path, NULL, nvals, vals, newPtr);
}

/* Variantes qui gardent les portées substituées de l'index (pathindex). */
static int
PathReadObj(Tcl_Interp *interp, Tcl_Obj *containerPtr, Tcl_Obj *pathObj,
	Tcl_Obj **resultPtr)
{
    return PathRead(interp, containerPtr, Tcl_GetString(pathObj), pathObj,
	    resultPtr);
}

static int
PathWriteObj(Tcl_Interp *interp, Tcl_Obj *containerPtr, Tcl_Obj *pathObj,
	Tcl_Size nvals, Tcl_Obj *const vals[], Tcl_Obj **newPtr)
{
    return PathWrite(interp, containerPtr, Tcl_GetString(pathObj), pathObj,
	    nvals, vals, newPtr);
}

/*
 * Appelée en tête de Tcl_SetObjCmd quand objv[1] ressemble à nom(chemin) :
 *	set P(x)          lecture ;  set P(x) v      écriture
 *	set L(1,2) a b    écriture groupée
 * Renvoie TCL_CONTINUE si la variable n'existe pas, n'est pas scalaire ou
 * n'est pas typée : le comportement habituel de Tcl est alors conservé
 * (set a(1,2) x reste l'idiome du tableau à deux dimensions).
 */
static int ColumnsPathWrite(Tcl_Obj *a, const char *name, Tcl_Size nvals,
	Tcl_Obj *const vals[]);

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
	if (TclTypeOpsMode() == TCL_TYPE_OPS_ON
		&& ColumnsPathWrite(cur, path, objc - 2, objv + 2)) {
	    /* 24/11 : colonnes écrites sur place */
	    newA = cur;
	    Tcl_IncrRefCount(newA);
	    rc = TCL_OK;
	} else {
	    rc = TclTypePathWrite(interp, cur, path, objc - 2, objv + 2,
		    &newA);
	}
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
    /* 24/11 : les mêmes lectures directes que le chemin compilé (tranches,
     * colonnes, champs) */
    rc = TclTypedVarGetElementVar(interp, varPtr, part2Ptr, &res);
    if (rc != TCL_OK) {
	return rc;
    }
    Tcl_SetObjResult(interp, res);	/* l'interpréteur garde une référence */
    Tcl_DecrRefCount(res);
    *resultPtr = res;
    return TCL_OK;
}

/*
 * ${R(chemin)} : le nom entier porte l'index (part2 absent). Le chemin est
 * LITTÉRAL, comme la clé de ${a(clé)} pour un tableau. TCL_CONTINUE si le
 * nom n'a pas cette forme ou si la variable n'est pas un scalaire typé
 * (le message d'erreur de Tcl est alors conservé).
 */
int
TclTypedVarGetElementByName(Tcl_Interp *interp, Tcl_Obj *nameObj,
	Tcl_Obj **resultPtr)
{
    Tcl_Size len;
    const char *nm = Tcl_GetStringFromObj(nameObj, &len);
    const char *open = (len > 2 && nm[len - 1] == ')') ? strchr(nm, '(')
	    : NULL;
    Tcl_Obj *p1, *p2;
    int rc;

    if (open == NULL || open == nm) {
	return TCL_CONTINUE;
    }
    p1 = Tcl_NewStringObj(nm, open - nm);
    p2 = Tcl_NewStringObj(open + 1, (nm + len - 1) - (open + 1));
    Tcl_IncrRefCount(p1);
    Tcl_IncrRefCount(p2);
    rc = TclTypedVarGetElement(interp, p1, p2, resultPtr);
    Tcl_DecrRefCount(p1);
    Tcl_DecrRefCount(p2);
    return rc;
}

/*
 * Lecture $a(champ) d'un produit : cache à l'endroit de la lecture. L'objet
 * index d'un chemin compilé ("x" dans $q(x)) est un littéral du bytecode ; il
 * garde le type produit vu la dernière fois et l'indice du champ. Les
 * descripteurs de types vivent autant que l'interpréteur et ne changent
 * jamais : même type, même indice. Le nom du champ est revérifié (strcmp
 * court) par prudence.
 */

/* Champ « simple » : lu sans règle particulière (somme et sélecteur S($f),
 * lien var, list.any, lien de frontière passent par le chemin général). */
static int
SimpleFieldType(const TclType *ft)
{
    return ft != NULL && ft->kind != TCLT_SUM && ft->kind != TCLT_VAR
	    && ft->binLink == NULL && !TclTypeIsAny(ft);
}

/*
 * L'index du champ simple nommé par part2Ptr dans le produit tp (cache dans
 * part2Ptr), ou -1 (un vrai chemin, un champ inconnu ou non simple).
 */
static Tcl_Size
SimpleFieldIndex(TclType *tp, Tcl_Obj *part2Ptr)
{
    Tcl_Size idx, len;
    const char *name, *c;
    const Tcl_Size *pos;

    name = Tcl_GetStringFromObj(part2Ptr, &len);
    if (part2Ptr->typePtr == &pathFieldType
	    && part2Ptr->internalRep.twoPtrValue.ptr1 == tp) {
	idx = (Tcl_Size) (intptr_t) part2Ptr->internalRep.twoPtrValue.ptr2;
    } else {
	Tcl_ObjInternalRep ir;

	if (len == 0 || PathIndexSpans(part2Ptr, &pos) != 0) {
	    return -1;
	}
	for (c = name; *c != '\0'; c++) {
	    if (!(isalnum(UCHAR(*c)) || *c == '_')) {
		return -1;		/* un vrai chemin : a.b, (x,y), ... */
	    }
	}
	idx = TclTypeFieldIndex(tp, name);
	if (idx < 0 || idx >= tp->nfields
		|| !SimpleFieldType(tp->fields[idx].type)) {
	    return -1;
	}
	ir.twoPtrValue.ptr1 = tp;
	ir.twoPtrValue.ptr2 = (void *) (intptr_t) idx;
	Tcl_StoreInternalRep(part2Ptr, &pathFieldType, &ir);
    }
    if (idx < 0 || idx >= tp->nfields || tp->fields[idx].name == NULL
	    || strcmp(Tcl_GetString(tp->fields[idx].name), name) != 0) {
	return -1;
    }
    return idx;
}

static Tcl_Obj *FieldAt(Tcl_Interp *interp, Tcl_Obj *v, TclType *ft,
	Tcl_Size idx);

static Tcl_Obj *
FastFieldRead(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *part2Ptr)
{
    Tcl_Obj *v;
    TclType *tp, *ft;
    int variant;
    Tcl_Size idx;

    if (!TclAnnotIsSettled(a)
	    || TclAnnotInfo(a, &v, &tp, &variant, NULL) != 1
	    || tp->kind != TCLT_PRODUCT) {
	return NULL;
    }
    idx = SimpleFieldIndex(tp, part2Ptr);
    if (idx < 0) {
	return NULL;
    }
    ft = tp->fields[idx].type;
    return FieldAt(interp, v, ft, idx);
}

/* Le champ idx (de type ft) du contenu v d'un produit, typé ; NULL si le
 * contenu n'a pas ce champ. */
static Tcl_Obj *
FieldAt(Tcl_Interp *interp, Tcl_Obj *v, TclType *ft, Tcl_Size idx)
{
    Tcl_Obj *e;

    if (Tcl_ListObjIndex(NULL, v, idx, &e) != TCL_OK || e == NULL) {
	return NULL;
    }
    if (TclIsAnnotated(e)) {
	TclType *et = NULL;

	if (TclAnnotIsSettled(e)
		&& TclAnnotInfo(e, NULL, &et, NULL, NULL) == 1 && et == ft) {
	    return e;			/* champ déjà typé dans le contenu */
	}
	e = TCL_ANNOT_CONTENT(e);
    }
    {
	Tcl_Obj *w = TclNewAnnotatedShared(interp, ft, -1, e);

	/*
	 * Le champ est typé une fois pour toutes dans le contenu du produit,
	 * comme les éléments d'une liste typée (la chaîne ne change pas) : la
	 * lecture suivante le rend sans rien allouer. Seulement dans un contenu
	 * privé (non partagé), liste ordinaire.
	 */
	if (!Tcl_IsShared(v) && TclHasInternalRep(v, &tclListType)) {
	    Tcl_ListObjReplace(NULL, v, idx, 1, 1, &w);
	}
	return w;
    }
}

/*
 * Instruction loadFieldNum ($a(x) opérande d'un opérateur arithmétique ou
 * d'une comparaison, dans expr compilé) : le NOMBRE du champ x d'un produit
 * typé, sans l'envelopper dans son type (l'opérateur n'en lirait que le
 * contenu) ; dans une vue sur une case, lu directement dans la case. Rend
 * l'objet (sans référence propre) ou NULL : l'instruction fait alors ce que
 * fait loadArray (même résultat, mêmes erreurs, traces).
 */
/*
 * Index a.b.c fait de noms simples (lettres, chiffres, _), sans portée
 * substituée qui contienne un point (elle serait un seul nom) : les noms,
 * terminés par '\0' dans buf. Rend leur nombre, 0 sinon.
 */
static int
DottedNames(Tcl_Obj *part2Ptr, int max, const char *names[], char *buf,
	size_t bufLen)
{
    Tcl_Size len, ns, k;
    const Tcl_Size *pos;
    const char *t = Tcl_GetStringFromObj(part2Ptr, &len);
    int n = 0;
    char *q;

    if (len == 0 || (size_t) len >= bufLen) {
	return 0;
    }
    ns = PathIndexSpans(part2Ptr, &pos);
    for (k = 0; k < ns; k++) {
	if (pos[2 * k + 1] == 0
		|| memchr(t + pos[2 * k], '.', (size_t) pos[2 * k + 1])) {
	    return 0;
	}
    }
    memcpy(buf, t, (size_t) len + 1);
    names[n++] = buf;
    for (q = buf; *q != '\0'; q++) {
	if (*q == '.') {
	    if (q == names[n - 1] || n == max) {
		return 0;
	    }
	    *q = '\0';
	    names[n++] = q + 1;
	} else if (!(isalnum(UCHAR(*q)) || *q == '_')) {
	    return 0;
	}
    }
    if (*names[n - 1] == '\0') {
	return 0;
    }
    return n;
}

/* L'élément idx d'une liste de n éléments, désigné par un nom simple. */
static int
SimpleListIndex(const char *name, Tcl_Size n, Tcl_Size *idxPtr)
{
    const char *q = name;
    Tcl_Size i = 0;

    if (isdigit(UCHAR(*q)) && (q[0] != '0' || q[1] == '\0')) {
	for (; isdigit(UCHAR(*q)); q++) {
	    if (i > n) {
		return 0;
	    }
	    i = i * 10 + (*q - '0');
	}
	if (*q != '\0') {
	    return 0;
	}
    } else if (ListIndex(NULL, name, n, &i) != TCL_OK) {
	return 0;
    }
    if (i < 0 || i >= n) {
	return 0;
    }
    *idxPtr = i;
    return 1;
}

/*
 * L(i.j...) d'une liste typée ordinaire dont chaque niveau est une liste
 * (list.list(3).double...) et la partie un nombre : le contenu de la partie
 * (partagé, refcount quelconque : l'appelant l'enveloppe ou le pousse), ou
 * NULL dès qu'un niveau sort de ce cas simple (élément annoté, produit,
 * somme...) : le chemin général s'en charge.
 */
static Tcl_Obj *
PlainPathPart(Tcl_Obj *v, TclType *tp, int k, const char *const names[],
	TclType **tpPtr)
{
    int d;

    for (d = 0; d < k; d++) {
	Tcl_Size n, idx;
	Tcl_Obj **ev;

	/* les éléments appartiennent à la liste (pas d'objet neuf) */
	if (tp->kind != TCLT_LIST || tp->elem == NULL || TclIsCompact(v)
		|| TclIsCompactElem(v)
		|| Tcl_ListObjGetElements(NULL, v, &n, &ev) != TCL_OK
		|| !SimpleListIndex(names[d], n, &idx) || TclIsAnnotated(ev[idx])) {
	    return NULL;
	}
	v = ev[idx];
	tp = tp->elem;
    }
    if (tp->kind != TCLT_INT && tp->kind != TCLT_DOUBLE) {
	return NULL;
    }
    *tpPtr = tp;
    return v;
}

/*
 * L(i.j...) d'une liste typée en cases : la partie lue dans la case (contenu
 * neuf, refcount 0, et son type), sans compiler l'index ni fabriquer
 * l'élément. NULL : le chemin général (qui fait la même lecture, ou dit
 * l'erreur).
 */
static Tcl_Obj *
CompactPathPart(Tcl_Obj *a, Tcl_Obj *part2Ptr, TclType **tpPtr)
{
    Tcl_Obj *v;
    TclType *tp;
    Tcl_Size n, idx;
    const char *names[16];
    char buf[256];
    int k;

    if (!TclAnnotIsSettled(a) || TclAnnotInfo(a, &v, &tp, NULL, NULL) != 1
	    || tp->kind != TCLT_LIST
	    || (k = DottedNames(part2Ptr, 16, names, buf, sizeof(buf))) < 2) {
	return NULL;
    }
    if (TclIsCompactElem(v)) {
	/* une vue sur une case : une matrice d'un lot (foreach, lindex) */
	return TclCompactViewReadPath(v, tp, k, names, tpPtr);
    }
    if (!TclIsCompact(v)) {
	return PlainPathPart(v, tp, k, names, tpPtr);
    }
    if (Tcl_ListObjLength(NULL, v, &n) != TCL_OK
	    || !SimpleListIndex(names[0], n, &idx)) {
	return NULL;
    }
    return TclCompactReadPath(v, idx, k - 1, names + 1, tpPtr);
}

/*
 * 23/11 : b(i) = 0|1 sur place, pour un bitarray que seule la variable tient :
 * un bit écrit dans les chiffres du grand entier (TclBitsSetInPlace), au
 * lieu d'un grand entier neuf à chaque bit (O(n) par écriture). 1 si écrit.
 */
static int
BitsPathWrite(Tcl_Obj *a, Tcl_Obj *part2Ptr, Tcl_Obj *value)
{
    Tcl_Obj *v, *nv;
    TclType *tp;
    const char *name, *p;
    Tcl_WideInt i;
    Tcl_Size lo, widthDigits;
    int bit;

    if (Tcl_IsShared(a) || !TclAnnotIsSettled(a) || TclIsAnnotated(value)
	    || TclAnnotInfo(a, &v, &tp, NULL, NULL) != 1
	    || tp->kind != TCLT_INT || tp->bitWidth <= 64) {
	return 0;
    }
    name = Tcl_GetString(part2Ptr);
    for (p = name; *p; p++) {
	if (*p < '0' || *p > '9') {
	    return 0;
	}
    }
    if (p == name || p - name > 15) {
	return 0;
    }
    i = strtoll(name, NULL, 10);
    /* seulement 0 ou 1 tels quels : le reste (2, true, 0x1) suit le
     * chemin ordinaire, qui le vérifie ou le refuse */
    p = Tcl_GetString(value);
    if (i < 0 || i >= tp->bitWidth || (p[0] != '0' && p[0] != '1')
	    || p[1] != '\0') {
	return 0;
    }
    bit = (p[0] == '1');
    lo = tp->bitMsb0 ? tp->bitWidth - 1 - (Tcl_Size) i : (Tcl_Size) i;
    widthDigits = tp->bitWidth / 60 + 2;
    if (!TclBitsSetInPlace(v, lo, bit, widthDigits)) {
	/* le premier bit : un contenu de travail, à soi */
	nv = TclBitsNewWorkObj(a, widthDigits);
	if (nv == NULL) {
	    return 0;
	}
	Tcl_IncrRefCount(nv);
	if (!TclAnnotSwapContent(a, nv)) {
	    Tcl_DecrRefCount(nv);
	    return 0;
	}
	Tcl_DecrRefCount(nv);
	if (!TclBitsSetInPlace(nv, lo, bit, widthDigits)) {
	    return 0;
	}
    }
    Tcl_InvalidateStringRep(a);
    return 1;
}

/*
 * 23/11 : b(a:b:c) = 0|1 et L(a:b:c) = scalaire, sur place, pour une
 * valeur que seule la variable tient (le crible : b(i*i::i) 1). Le chemin
 * est fait de bornes simples (chiffres, end, end-n, vides). 1 si écrit.
 */
static int
SimpleBound(const char *p, const char *e)
{
    if (p == e) {
	return 1;
    }
    if (e - p >= 3 && strncmp(p, "end", 3) == 0) {
	p += 3;
	if (p == e) {
	    return 1;
	}
	if (*p != '-' && *p != '+') {
	    return 0;
	}
	p++;
	if (p == e) {
	    return 0;
	}
    } else if (*p == '-') {
	p++;
	if (p == e) {
	    return 0;
	}
    }
    for (; p < e; p++) {
	if (*p < '0' || *p > '9') {
	    return 0;
	}
    }
    return 1;
}

/*
 * 23/11 : L(a:b:c) d'une liste de scalaires simples (nus dans le contenu,
 * ou en cases) : la tranche faite d'un coup, list.T comme le chemin
 * général, sans envelopper chaque élément pour le défaire ensuite. Une
 * référence pour l'appelant ; NULL : le chemin général.
 */
/*
 * Le texte d'une tranche a:b, a:b:c, ou d'une fenêtre (s0,s1,...):b:c
 * (24/11), bornes simples seulement (entiers, end, end-k) : *st prêt pour
 * RangeSpec (st->wj à régler par colonne), parts[] et *startsPtr à libérer
 * par RangeTextFree. 1 si le texte a cette forme.
 */
typedef struct RangeText {
    IndexStep st;
    Tcl_Obj *parts[3];
    int np;
    Tcl_Obj *starts;		/* NULL : une tranche */
    Tcl_Size ncols;		/* 1 pour une tranche */
} RangeText;

static void
RangeTextFree(RangeText *rt)
{
    int k;

    for (k = 0; k < rt->np; k++) {
	TclDiscard(rt->parts[k]);
    }
    rt->np = 0;
    if (rt->starts != NULL) {
	Tcl_DecrRefCount(rt->starts);
	rt->starts = NULL;
    }
}

static int
RangeTextParse(const char *name, Tcl_Size len, RangeText *rt)
{
    const char *p = name, *q = NULL, *end = name + len;

    rt->np = 0;
    rt->starts = NULL;
    rt->ncols = 1;
    if (len > 200 || memchr(name, ':', len) == NULL) {
	return 0;
    }
    if (*p == '(') {
	/* la fenêtre : (s0,s1,...) puis :b ou :b:c */
	const char *close = (const char *) memchr(p, ')', end - p);

	if (close == NULL || close + 1 >= end || close[1] != ':') {
	    return 0;
	}
	rt->starts = Tcl_NewListObj(0, NULL);
	Tcl_IncrRefCount(rt->starts);
	rt->ncols = 0;
	for (p = name + 1; ; p = q + 1) {
	    q = (const char *) memchr(p, ',', close - p);
	    if (q == NULL) {
		q = close;
	    }
	    if (p == q || !SimpleBound(p, q) || ++rt->ncols > 64) {
		RangeTextFree(rt);
		return 0;
	    }
	    Tcl_ListObjAppendElement(NULL, rt->starts,
		    Tcl_NewStringObj(p, q - p));
	    if (q == close) {
		break;
	    }
	}
	if (rt->ncols < 2) {
	    RangeTextFree(rt);
	    return 0;
	}
	rt->parts[rt->np++] = Tcl_NewObj();	/* (débuts dans starts) */
	p = close + 2;
    }
    for (; rt->np < 3; p = q + 1) {
	q = (const char *) memchr(p, ':', end - p);
	if (q == NULL) {
	    q = end;
	}
	if (!SimpleBound(p, q)) {
	    RangeTextFree(rt);
	    return 0;
	}
	rt->parts[rt->np++] = Tcl_NewStringObj(p, q - p);
	if (q == end) {
	    break;
	}
    }
    if (rt->np < 2 || q != end) {
	RangeTextFree(rt);
	return 0;
    }
    rt->st.kind = IS_RANGE;
    rt->st.a = rt->parts[0];
    rt->st.b = rt->parts[1];
    rt->st.c = (rt->np == 3) ? rt->parts[2] : NULL;
    rt->st.atom = 0;
    rt->st.wstarts = rt->starts;
    rt->st.wbound = rt->parts[1];
    rt->st.wj = 0;
    return 1;
}

/*
 * 24/11 : l'index porte-t-il une valeur substituée qui contient de la
 * syntaxe de chemin (: , ( ) . *) ? Le chemin général la lit comme un nom
 * entier ($L($s), s = "1:3" : l'élément "1:3", pas une tranche) ; les voies
 * rapides qui relisent le texte doivent alors lui laisser la main.
 */
static int
SpansHideSyntax(Tcl_Obj *part2Ptr)
{
    const Tcl_Size *pos;
    Tcl_Size n, k, i, len;
    const char *t;

    if (part2Ptr->typePtr == &typePathType) {
	const CompiledPath *cp = (const CompiledPath *)
		part2Ptr->internalRep.twoPtrValue.ptr1;
	Tcl_Size a, b;

	for (a = 0; a < cp->set.n; a++) {
	    for (b = 0; b < cp->set.path[a].n; b++) {
		const IndexStep *st = &cp->set.path[a].step[b];

		if (st->atom && st->a != NULL
			&& strpbrk(Tcl_GetString(st->a), ":,().*") != NULL) {
		    return 1;
		}
	    }
	}
	return 0;
    }
    n = PathIndexSpans(part2Ptr, &pos);
    if (n == 0) {
	return 0;
    }
    t = Tcl_GetStringFromObj(part2Ptr, &len);
    for (k = 0; k < n; k++) {
	for (i = pos[2 * k]; i < pos[2 * k] + pos[2 * k + 1] && i < len; i++) {
	    if (strchr(":,().*", t[i]) != NULL) {
		return 1;
	    }
	}
    }
    return 0;
}

/* une colonne lue d'une liste de scalaires V (type lt) : une vue (cases ou
 * objets) quand c'est possible, sinon une copie ; une référence pour
 * l'appelant, NULL si la forme ne s'y prête pas */
static Tcl_Obj *
RangeColumn(Tcl_Interp *interp, Tcl_Obj *v, TclType *rt, Tcl_Size first,
	Tcl_Size count, Tcl_Size stride)
{
    Tcl_Obj *res = NULL;
    Tcl_Size n, k;
    int noView = (getenv("TCL_TYPE_NOVIEW") != NULL);

    if (stride > 0 && !noView
	    && (TclIsCompact(v) || TclIsCompactView(v))) {
	/* 24/11 : une vue à pas, sans copie */
	Tcl_Obj *vw = TclCompactViewNew(v, first, stride, count);

	if (vw != NULL) {
	    res = TclNewAnnotatedOwn(vw, TclTypeRefFor(interp, rt, -1));
	    Tcl_IncrRefCount(res);
	    return res;
	}
    }
    if (TclIsCompact(v) || TclIsCompactView(v)) {
	Tcl_Size cn;
	const unsigned char *src;
	unsigned char *dst;
	size_t cstride, dstride;
	int ckind, csize;
	Tcl_Obj *col;

	if (!TclCompactScalarInfo(v, &cn, &src, &cstride, &ckind, &csize)
		|| (col = TclCompactNewAlways(rt, count, &dst)) == NULL) {
	    return NULL;
	}
	{
	    Tcl_Size dn;
	    const unsigned char *d0;
	    int dk, ds;

	    if (!TclCompactScalarInfo(col, &dn, &d0, &dstride, &dk, &ds)
		    || dk != ckind || ds != csize) {
		Tcl_DecrRefCount(col);
		return NULL;
	    }
	    dst = (unsigned char *) d0;
	}
	for (k = 0; k < count; k++) {
	    memcpy(dst + (size_t) k * dstride,
		    src + (size_t) (first + k * stride) * cstride,
		    (size_t) csize);
	}
	res = TclNewAnnotatedOwn(col, TclTypeRefFor(interp, rt, -1));
    } else {
	Tcl_Obj **ev, **sel, *l;

	if (stride > 0 && !noView
		&& (l = TclObjViewNew(v, first, stride, count)) != NULL) {
	    /* 24/11 : une vue sur les objets, sans les compter un à un */
	    res = TclNewAnnotatedOwn(l, TclTypeRefFor(interp, rt, -1));
	    Tcl_IncrRefCount(res);
	    return res;
	}
	if (TclListObjGetElements(NULL, v, &n, &ev) != TCL_OK) {
	    return NULL;
	}
	sel = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *)
		* (size_t) (count ? count : 1));
	for (k = 0; k < count; k++) {
	    /* (nus dans V, v16s ; un élément typé y resterait valide) */
	    sel[k] = ev[first + k * stride];
	}
	l = Tcl_NewListObj(count, sel);
	Tcl_Free(sel);
	res = TclNewAnnotatedOwn(l, TclTypeRefFor(interp, rt, -1));
    }
    Tcl_IncrRefCount(res);
    return res;
}

/*
 * L(a:b:c) et la fenêtre L((s0,s1,...):b:c) (24/11) sur une liste typée de
 * scalaires : les colonnes en vues quand c'est possible. Les bornes sont
 * des index simples (entiers, end, end-k) ; sinon NULL (le chemin général).
 */
static Tcl_Obj *
ScalarRangeRead(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *part2Ptr)
{
    Tcl_Obj *v, *res = NULL, *cols[64];
    TclType *lt, *rt;
    RangeText rx;
    Tcl_Size len, first, count, stride, n, k;
    const char *name;

    if (TclAnnotInfo(a, &v, &lt, NULL, NULL) != 1 || lt == NULL
	    || lt->kind != TCLT_LIST || !TclTypeBareElem(lt->elem)) {
	return NULL;
    }
    name = Tcl_GetStringFromObj(part2Ptr, &len);
    if (!RangeTextParse(name, len, &rx)) {
	return NULL;
    }
    if (Tcl_ListObjLength(NULL, v, &n) != TCL_OK
	    || (rt = TclTypeListOf(interp, lt->elem)) == NULL) {
	goto done;
    }
    if (rx.starts == NULL) {
	if (RangeSpec(NULL, &rx.st, n, &first, &count, &stride) != TCL_OK
		|| count == 0) {
	    goto done;
	}
	res = RangeColumn(interp, v, rt, first, count, stride);
	goto done;
    }
    for (k = 0; k < rx.ncols; k++) {
	rx.st.wj = (int) k;
	if (RangeSpec(NULL, &rx.st, n, &first, &count, &stride) != TCL_OK
		|| count < 2
		|| (cols[k] = RangeColumn(interp, v, rt, first, count, stride))
		== NULL) {
	    /* (une fenêtre de 0 ou 1 : le chemin général, qui la rend comme
	     * un intervalle ordinaire) */
	    while (k-- > 0) {
		Tcl_DecrRefCount(cols[k]);
	    }
	    goto done;
	}
    }
    {
	TclType *outT = TclTypeListOf(interp, rt);

	if (outT != NULL) {
	    Tcl_Obj *l = Tcl_NewListObj(rx.ncols, cols);

	    Tcl_IncrRefCount(l);
	    res = TclNewAnnotated(interp, outT, -1, l);
	    Tcl_DecrRefCount(l);
	    if (res != NULL) {
		Tcl_IncrRefCount(res);
	    }
	}
	for (k = 0; k < rx.ncols; k++) {
	    Tcl_DecrRefCount(cols[k]);
	}
    }
  done:
    RangeTextFree(&rx);
    return res;
}

/*
 * v17w : la même chose pour une liste d'objets : une liste neuve, faite des
 * éléments gardés et des valeurs des colonnes (des objets, ou des nombres
 * tirés de cases), rangée dans la variable sous le même type. Les colonnes
 * sont déjà dans le type des éléments (l'appelant les y a mises).
 */
/* une case réécrite (des colonnes qui se recouvrent) : l'objet neuf
 * qu'elle tenait, jamais rangé, est libéré */
#define OBJCOL_DROP(I) \
    do {								\
	if (nev[(I)] != ev[(I)] && nev[(I)]->refCount == 0) {		\
	    Tcl_IncrRefCount(nev[(I)]);					\
	    Tcl_DecrRefCount(nev[(I)]);					\
	}								\
    } while (0)

static int
ObjColumnsWrite(Tcl_Interp *interp, Var *varPtr, Tcl_Obj *a, Tcl_Obj *v,
	TclType *tp, const char *path, Tcl_Size K, Tcl_Obj *const cols[])
{
    RangeText rx;
    Tcl_Size n, j, k, i, first[64], count[64], stride[64], cn;
    Tcl_Obj **ev, **nev = NULL, *nl, *na, *old;
    int ok = 0;

    if (TclIsCompactView(v) || TclListObjGetElements(NULL, v, &n, &ev)
	    != TCL_OK) {
	return 0;
    }
    if (!RangeTextParse(path, (Tcl_Size) strlen(path), &rx)) {
	return 0;
    }
    if (rx.ncols != K) {
	goto done;
    }
    for (j = 0; j < K; j++) {
	TclType *ct = NULL;

	rx.st.wj = (int) j;
	if (RangeSpec(NULL, &rx.st, n, &first[j], &count[j], &stride[j])
		!= TCL_OK || TclAnnotInfo(cols[j], NULL, &ct, NULL, NULL) != 1
		|| ct->kind != TCLT_LIST || ct->elem != tp->elem
		|| Tcl_ListObjLength(NULL, cols[j], &cn) != TCL_OK
		|| cn != count[j]) {
	    goto done;
	}
    }
    nev = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) (n > 0 ? n : 1));
    memcpy(nev, ev, sizeof(Tcl_Obj *) * (size_t) n);
    for (j = 0; j < K; j++) {
	Tcl_Obj *c = TCL_ANNOT_CONTENT(cols[j]), **cev;
	const unsigned char *cd;
	size_t cs;
	int ck, cz;
	Tcl_Size cnn;

	if (TclCompactScalarInfo(c, &cnn, &cd, &cs, &ck, &cz)) {
	    /* des cases : un objet par nombre (la liste d'objets en veut) */
	    for (k = 0, i = first[j]; k < count[j]; k++, i += stride[j]) {
		const unsigned char *cp = cd + (size_t) k * cs;

		OBJCOL_DROP(i);

		if (ck == 2) {
		    double d;

		    if (cz == 4) {
			float f;

			memcpy(&f, cp, 4);
			d = f;
		    } else {
			memcpy(&d, cp, 8);
		    }
		    nev[i] = Tcl_NewDoubleObj(d);
		} else if (cz == 8) {
		    Tcl_WideInt w;

		    memcpy(&w, cp, 8);
		    if (ck == 1 && w < 0) {
			goto undo;		/* (un grand non signé) */
		    }
		    nev[i] = Tcl_NewWideIntObj(w);
		} else {
		    goto undo;
		}
	    }
	} else if (TclListObjGetElements(NULL, c, &cnn, &cev) == TCL_OK) {
	    for (k = 0, i = first[j]; k < count[j]; k++, i += stride[j]) {
		OBJCOL_DROP(i);
		nev[i] = TCL_ANNOT_CONTENT(cev[k]);
	    }
	} else {
	    goto undo;
	}
    }
    if (0) {
      undo:
	/* les objets neufs jamais rangés : libérés par une liste jetable */
	for (i = 0; i < n; i++) {
	    if (nev[i] != ev[i] && nev[i]->refCount == 0) {
		Tcl_IncrRefCount(nev[i]);
		Tcl_DecrRefCount(nev[i]);
	    }
	}
	goto done;
    }
    nl = Tcl_NewListObj(n, nev);
    na = TclNewAnnotatedOwn(nl, TclTypeRefFor(interp, tp, -1));
    if (na == NULL) {
	goto done;
    }
    Tcl_IncrRefCount(na);
    old = varPtr->value.objPtr;
    varPtr->value.objPtr = na;
    Tcl_DecrRefCount(old);
    (void) a;
    ok = 1;
  done:
    if (nev != NULL) {
	Tcl_Free(nev);
    }
    RangeTextFree(&rx);
    return ok;
}

/*
 * v17w (type map -in) : écrit K colonnes en cases (même disposition que
 * la cible) dans la variable, par un chemin de tranches ou de fenêtres ;
 * la liste de la variable est copiée d'abord si elle est partagée (une
 * seule copie, puis les écritures sur place). 1 : fait ; 0 : rien n'est
 * écrit, le cas ne s'y prête pas (l'appelant passe par set). L'appelant a
 * vérifié que le type des éléments n'a rien d'autre à vérifier.
 */
int
TclTypePathWriteCellColumns(Tcl_Interp *interp, Tcl_Obj *nameObj,
	const char *path, Tcl_Size K, Tcl_Obj *const cols[])
{
    Var *varPtr, *arrayPtr;
    Tcl_Obj *a, *v, *nv = NULL, *na = NULL, *ref;
    TclType *tp;
    RangeText rx;
    Tcl_Size n, j, k, i, first[64], count[64], stride[64], dn, cn;
    const unsigned char *dd0, *sd[64];
    size_t ds, ss[64];
    int dk, dz, sk, sz, ok = 0;

    if (K < 1 || K > 64) {
	return 0;
    }
    varPtr = TclObjLookupVarEx(interp, nameObj, NULL, 0, "set", 0, 0,
	    &arrayPtr);
    if (varPtr == NULL || arrayPtr != NULL) {
	return 0;
    }
    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    if (TclIsVarArray(varPtr) || !(varPtr->flags & VAR_TYPED)
	    || (a = varPtr->value.objPtr) == NULL
	    || (varPtr->flags & (VAR_TRACED_READ|VAR_TRACED_WRITE))
	    || TclAnnotInfo(a, &v, &tp, NULL, NULL) != 1
	    || tp->kind != TCLT_LIST) {
	return 0;
    }
    if (!TclCompactScalarInfo(v, &dn, &dd0, &ds, &dk, &dz)) {
	return ObjColumnsWrite(interp, varPtr, a, v, tp, path, K, cols);
    }
    if (!RangeTextParse(path, (Tcl_Size) strlen(path), &rx)) {
	return 0;
    }
    n = dn;
    if (rx.ncols != K) {
	goto done;
    }
    for (j = 0; j < K; j++) {
	rx.st.wj = (int) j;
	if (RangeSpec(NULL, &rx.st, n, &first[j], &count[j], &stride[j])
		!= TCL_OK
		|| !TclCompactScalarInfo(TCL_ANNOT_CONTENT(cols[j]), &cn,
			&sd[j], &ss[j], &sk, &sz)
		|| sk != dk || sz != dz || cn != count[j]) {
	    goto done;
	}
    }
    /* la liste à la seule variable : sinon une copie privée */
    if (Tcl_IsShared(a) || Tcl_IsShared(v) || !TclCompactScalarWritable(v)) {
	/* les colonnes couvrent-elles toute la liste ? (chaque case une
	 * fois : pas K, débuts 0..K-1) ; alors rien à recopier */
	int full = 1;
	unsigned char seen[64];

	memset(seen, 0, sizeof(seen));
	for (j = 0; j < K && full; j++) {
	    if (stride[j] != K || first[j] < 0 || first[j] >= K
		    || seen[first[j]] || count[j] != (n - first[j] + K - 1) / K) {
		full = 0;
	    } else {
		seen[first[j]] = 1;
	    }
	}
	if (full && n % K != 0) {
	    full = 0;
	}
	nv = TclCompactCloneEx(v, !full);
	if (nv == NULL) {
	    goto done;
	}
	ref = TclTypeRefFor(interp, tp, -1);
	na = TclNewAnnotatedOwn(nv, ref);
	if (na == NULL) {
	    goto done;
	}
	Tcl_IncrRefCount(na);
	v = nv;
	(void) TclCompactScalarInfo(v, &dn, &dd0, &ds, &dk, &dz);
    }
    for (j = 0; j < K; j++) {
	unsigned char *dd = (unsigned char *) dd0;

	if (ss[j] == (size_t) dz && stride[j] == 1) {
	    memcpy(dd + (size_t) first[j] * ds, sd[j],
		    (size_t) count[j] * (size_t) dz);
	    continue;
	}
	if (dz == 8 && ds % 8 == 0 && ss[j] % 8 == 0
		&& ((uintptr_t) dd0 % 8) == 0 && ((uintptr_t) sd[j] % 8) == 0) {
	    uint64_t *d64 = (uint64_t *) dd0;
	    const uint64_t *s64 = (const uint64_t *) sd[j];
	    size_t dstep = (ds / 8) * (size_t) stride[j], sstep = ss[j] / 8;
	    uint64_t *dp = d64 + (size_t) first[j] * (ds / 8);

	    for (k = 0; k < count[j]; k++) {
		dp[(size_t) k * dstep] = s64[(size_t) k * sstep];
	    }
	    continue;
	}
	for (k = 0, i = first[j]; k < count[j]; k++, i += stride[j]) {
	    memcpy(dd + (size_t) i * ds, sd[j] + (size_t) k * ss[j],
		    (size_t) dz);
	}
    }
    if (na != NULL) {
	/* la copie devient la valeur de la variable (même type) */
	Tcl_Obj *old = varPtr->value.objPtr;

	varPtr->value.objPtr = na;	/* (la référence de na passe) */
	Tcl_DecrRefCount(old);
	na = NULL;
    } else {
	TclInvalidateStringRep(a);
    }
    ok = 1;
  done:
    if (na != NULL) {
	Tcl_DecrRefCount(na);
    }
    RangeTextFree(&rx);
    return ok;
}

/*
 * v17x : écrire type map -in bloc par bloc. TclTypePathCellsBegin prépare
 * une liste neuve en cases de doubles (8 octets) pour VAR, de même type :
 * non remplie quand les colonnes du chemin couvrent chaque case une fois,
 * sinon une copie (seulement si la liste est partagée : une liste à la
 * seule variable s'écrit sur place, par l'autre chemin). Rend, pour chaque
 * colonne, l'adresse de sa première case et le pas en octets ; rien n'est
 * installé avant TclTypePathCellsEnd(…, 1) ; End(…, 0) abandonne. Entre les
 * deux, aucun script ne doit s'exécuter. NULL : le cas ne s'y prête pas.
 */
typedef struct CellSink {
    Var *varPtr;
    Tcl_Obj *na;		/* la nouvelle valeur (une référence) */
} CellSink;

void *
TclTypePathCellsBegin(Tcl_Interp *interp, Tcl_Obj *nameObj,
	const char *path, Tcl_Size K, Tcl_Size count0, unsigned char **dst,
	size_t *dstep)
{
    Var *varPtr, *arrayPtr;
    Tcl_Obj *a, *v, *nv, *na;
    TclType *tp;
    RangeText rx;
    Tcl_Size n, j, first[64], count[64], stride[64], dn;
    const unsigned char *dd0;
    size_t ds;
    int dk, dz, full = 1;
    unsigned char seen[64];
    CellSink *cs;

    if (K < 1 || K > 64) {
	return NULL;
    }
    varPtr = TclObjLookupVarEx(interp, nameObj, NULL, 0, "set", 0, 0,
	    &arrayPtr);
    if (varPtr == NULL || arrayPtr != NULL) {
	Tcl_ResetResult(interp);
	return NULL;
    }
    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    if (TclIsVarArray(varPtr) || !(varPtr->flags & VAR_TYPED)
	    || (a = varPtr->value.objPtr) == NULL
	    || (varPtr->flags & (VAR_TRACED_READ|VAR_TRACED_WRITE))
	    || TclAnnotInfo(a, &v, &tp, NULL, NULL) != 1
	    || tp->kind != TCLT_LIST
	    || !TclCompactScalarInfo(v, &dn, &dd0, &ds, &dk, &dz)
	    || dk != 2 || dz != 8 || ds % 8 != 0) {
	return NULL;
    }
    if (!RangeTextParse(path, (Tcl_Size) strlen(path), &rx)) {
	return NULL;
    }
    n = dn;
    if (rx.ncols != K) {
	RangeTextFree(&rx);
	return NULL;
    }
    memset(seen, 0, sizeof(seen));
    for (j = 0; j < K; j++) {
	rx.st.wj = (int) j;
	if (RangeSpec(NULL, &rx.st, n, &first[j], &count[j], &stride[j])
		!= TCL_OK || count[j] != count0) {
	    RangeTextFree(&rx);
	    return NULL;
	}
	if (stride[j] != K || first[j] < 0 || first[j] >= K
		|| seen[first[j]] || count[j] != (n - first[j] + K - 1) / K) {
	    full = 0;
	} else {
	    seen[first[j]] = 1;
	}
    }
    RangeTextFree(&rx);
    if (full && n % K != 0) {
	full = 0;
    }
    if (!full) {
	/* des colonnes disjointes seulement (même pas, débuts distincts dans
	 * un pas) : sinon l'ordre des écritures compterait (la dernière
	 * colonne l'emporte, colonne par colonne) */
	Tcl_Size lo = first[0], hi = first[0], q;

	for (j = 0; j < K; j++) {
	    if (stride[j] != stride[0] || stride[j] < 1) {
		return NULL;
	    }
	    for (q = 0; q < j; q++) {
		if (first[q] == first[j]) {
		    return NULL;
		}
	    }
	    lo = (first[j] < lo) ? first[j] : lo;
	    hi = (first[j] > hi) ? first[j] : hi;
	}
	if (hi - lo >= stride[0]) {
	    return NULL;
	}
    }
    if (!full && !(Tcl_IsShared(a) || Tcl_IsShared(v)
	    || !TclCompactScalarWritable(v))) {
	return NULL;		/* sur place : le chemin des colonnes */
    }
    nv = TclCompactCloneEx(v, !full);
    if (nv == NULL) {
	return NULL;
    }
    na = TclNewAnnotatedOwn(nv, TclTypeRefFor(interp, tp, -1));
    if (na == NULL) {
	return NULL;
    }
    Tcl_IncrRefCount(na);
    (void) TclCompactScalarInfo(nv, &dn, &dd0, &ds, &dk, &dz);
    for (j = 0; j < K; j++) {
	dst[j] = (unsigned char *) dd0 + (size_t) first[j] * ds;
	dstep[j] = ds * (size_t) stride[j];
    }
    cs = (CellSink *) Tcl_Alloc(sizeof(CellSink));
    cs->varPtr = varPtr;
    cs->na = na;
    return cs;
}

void
TclTypePathCellsEnd(void *h, int commit)
{
    CellSink *cs = (CellSink *) h;

    if (commit) {
	Tcl_Obj *old = cs->varPtr->value.objPtr;

	cs->varPtr->value.objPtr = cs->na;	/* (la référence passe) */
	Tcl_DecrRefCount(old);
    } else {
	Tcl_DecrRefCount(cs->na);
    }
    Tcl_Free(cs);
}

/*
 * 24/11 : L(a:b:c) = colonne, L((s0,s1,...):b:c) = colonne colonne ...,
 * sur place : L une liste typée de nombres (entiers ou flottants, ni
 * nominale ni binaire) tenue par la seule variable, en objets ou en cases ;
 * chaque colonne une liste typée du même type d'élément (ses éléments n'ont
 * pas à être revérifiés), de la longueur de la tranche. Tout ou rien : 0
 * sans rien écrire si un cas ne s'y prête pas (le chemin général décide,
 * avec ses messages).
 */
static int
ColumnsPathWrite(Tcl_Obj *a, const char *name, Tcl_Size nvals,
	Tcl_Obj *const vals[])
{
    Tcl_Obj *v, *srcList[64];
    TclType *tp;
    RangeText rx;
    Tcl_Size n, k, i, j, first[64], count[64], stride[64];
    Tcl_Obj *const *sElems[64];
    Tcl_Size sStride[64];
    const unsigned char *sData[64];
    size_t sCStride[64];
    int sKind[64], sSize[64], ok = 0, dKind = 0, dSize = 0;
    int compact, numU;

    if (nvals < 1 || nvals > 64 || Tcl_IsShared(a) || !TclAnnotIsSettled(a)
	    || TclAnnotInfo(a, &v, &tp, NULL, NULL) != 1
	    || tp->kind != TCLT_LIST || tp->elem == NULL
	    || Tcl_IsShared(v)) {
	return 0;
    }
    /* les éléments : des nombres simples, ou l'union {int}i | {double}d
     * (num : la charge seule, nue ; sa variante est celle du nombre) */
    numU = TclTypeIsNumUnion(tp->elem);
    if (!numU && ((tp->elem->kind != TCLT_INT && tp->elem->kind != TCLT_DOUBLE)
	    || tp->elem->nominal || tp->elem->binLink != NULL)) {
	return 0;
    }
    compact = TclIsCompact(v);
    if ((!compact && !TclHasInternalRep(v, &tclListType))
	    || (compact && numU)) {
	return 0;
    }
    if (!RangeTextParse(name, (Tcl_Size) strlen(name), &rx)) {
	return 0;
    }
    if (rx.ncols != nvals || Tcl_ListObjLength(NULL, v, &n) != TCL_OK) {
	goto done;
    }
    if (compact) {
	Tcl_Size dn;
	const unsigned char *dd;
	size_t ds;

	if (!TclCompactScalarInfo(v, &dn, &dd, &ds, &dKind, &dSize)) {
	    goto done;
	}
    }
    /* les colonnes : toutes vérifiées avant d'écrire */
    for (j = 0; j < nvals; j++) {
	Tcl_Obj *c;
	TclType *vt;
	Tcl_Size cn, bn;
	Tcl_Obj *base;

	rx.st.wj = (int) j;
	if (RangeSpec(NULL, &rx.st, n, &first[j], &count[j], &stride[j])
		!= TCL_OK
		|| TclAnnotInfo(vals[j], &c, &vt, NULL, NULL) != 1
		|| vt->kind != TCLT_LIST
		|| (vt->elem != tp->elem && !(numU
		    && (vt->elem == tp->elem->fields[0].type
			|| vt->elem == tp->elem->fields[1].type)))) {
	    goto done;
	}
	srcList[j] = c;
	sElems[j] = NULL;
	sData[j] = NULL;
	if (TclCompactScalarInfo(c, &cn, &sData[j], &sCStride[j], &sKind[j],
		&sSize[j])) {
	    if ((compact && (sKind[j] != dKind || sSize[j] != dSize))
		    || (!compact && sKind[j] == 1 && sSize[j] == 8)) {
		goto done;		/* (un grand non signé : général) */
	    }
	} else if (compact) {
	    goto done;			/* objets dans des cases : général */
	} else if (TclObjViewInfo(c, &base, &sElems[j], &cn, &sStride[j])) {
	    /* (base tenue par la vue, tenue par vals[j]) */
	} else if (TclHasInternalRep(c, &tclListType)) {
	    Tcl_Obj **ev;

	    if (TclListObjGetElements(NULL, c, &cn, &ev) != TCL_OK) {
		goto done;
	    }
	    sElems[j] = ev;
	    sStride[j] = 1;
	} else {
	    goto done;
	}
	if (cn != count[j] || count[j] < 2) {
	    goto done;			/* (0 ou 1 : le général, sa règle) */
	}
	(void) bn;
    }
    /* l'écriture */
    if (compact) {
	Tcl_Size dn;
	const unsigned char *d0;
	size_t ds;
	int dk, dz;

	/* le tampon à la seule liste (une vue le tient : général) */
	if (!TclCompactScalarWritable(v)
		|| !TclCompactScalarInfo(v, &dn, &d0, &ds, &dk, &dz)) {
	    goto done;
	}
	for (j = 0; j < nvals; j++) {
	    unsigned char *dd = (unsigned char *) d0;

	    for (k = 0, i = first[j]; k < count[j]; k++, i += stride[j]) {
		memcpy(dd + (size_t) i * ds, sData[j] + (size_t) k * sCStride[j],
			(size_t) dz);
	    }
	}
	ok = 1;
    }
    for (j = 0; j < nvals && !compact; j++) {
	if (sData[j] != NULL) {
	    for (k = 0, i = first[j]; k < count[j]; k++, i += stride[j]) {
		const unsigned char *cp = sData[j] + (size_t) k * sCStride[j];
		Tcl_Obj *o;

		if (sKind[j] == 2) {
		    double d;

		    if (sSize[j] == 4) {
			float f;

			memcpy(&f, cp, 4);
			d = f;
		    } else {
			memcpy(&d, cp, 8);
		    }
		    o = Tcl_NewDoubleObj(d);
		} else {
		    Tcl_WideInt w = 0;

		    switch (sSize[j]) {
		    case 1: w = sKind[j] ? (Tcl_WideInt) *(uint8_t *) cp
				: (Tcl_WideInt) *(int8_t *) cp; break;
		    case 2: { uint16_t u; int16_t s16; memcpy(&u, cp, 2);
				memcpy(&s16, cp, 2);
				w = sKind[j] ? (Tcl_WideInt) u : s16; break; }
		    case 4: { uint32_t u; int32_t s32; memcpy(&u, cp, 4);
				memcpy(&s32, cp, 4);
				w = sKind[j] ? (Tcl_WideInt) u : s32; break; }
		    default: memcpy(&w, cp, 8); break;
		    }
		    o = Tcl_NewWideIntObj(w);
		}
		TclListObjSetElement(NULL, v, i, o);
	    }
	} else {
	    for (k = 0, i = first[j]; k < count[j]; k++, i += stride[j]) {
		TclListObjSetElement(NULL, v, i, sElems[j][k * sStride[j]]);
	    }
	}
	ok = 1;
    }
    TclInvalidateStringRep(v);
    Tcl_InvalidateStringRep(a);
  done:
    (void) srcList;
    RangeTextFree(&rx);
    return ok;
}

static int
RangePathWrite(Tcl_Obj *a, Tcl_Obj *part2Ptr, Tcl_Obj *value)
{
    Tcl_Obj *v, *parts[3] = {NULL, NULL, NULL}, *c = NULL;
    TclType *tp;
    IndexStep st;
    Tcl_Size len, first, count, stride, n, k, i;
    const char *name, *p, *q, *end;
    int np = 0, ok = 0;

    if (Tcl_IsShared(a) || !TclAnnotIsSettled(a) || TclIsAnnotated(value)
	    || TclAnnotInfo(a, &v, &tp, NULL, NULL) != 1) {
	return 0;
    }
    if (!((tp->kind == TCLT_INT && tp->bitWidth > 64)
	    || (tp->kind == TCLT_LIST && TclIsCompact(v))
	    || (tp->kind == TCLT_LIST && !Tcl_IsShared(v)
		&& TclHasInternalRep(v, &tclListType)
		&& tp->elem != NULL && (tp->elem->kind == TCLT_INT
		    || tp->elem->kind == TCLT_DOUBLE)
		&& !tp->elem->nominal && tp->elem->binLink == NULL))) {
	return 0;
    }
    name = Tcl_GetStringFromObj(part2Ptr, &len);
    end = name + len;
    if (memchr(name, ':', len) == NULL || len > 64) {
	return 0;
    }
    for (p = name; np < 3; p = q + 1) {
	q = (const char *) memchr(p, ':', end - p);
	if (q == NULL) {
	    q = end;
	}
	if (!SimpleBound(p, q)) {
	    return 0;
	}
	parts[np++] = Tcl_NewStringObj(p, q - p);
	if (q == end) {
	    break;
	}
    }
    if (np < 2 || q != end) {
	goto done;
    }
    st.kind = IS_RANGE;
    st.a = parts[0];
    st.b = parts[1];
    st.c = (np == 3) ? parts[2] : NULL;
    st.atom = 0;
    st.wstarts = NULL;
    st.wbound = NULL;
    st.wj = 0;
    if (tp->kind == TCLT_INT) {
	Tcl_Size lo, bstep, wd = tp->bitWidth / 60 + 2;
	const char *t = Tcl_GetString(value);

	if ((t[0] != '0' && t[0] != '1') || t[1] != '\0'
		|| BitsStride(NULL, tp, &st, &lo, &count, &bstep) != TCL_OK) {
	    goto done;
	}
	if (!TclBitsFillStride(v, lo, bstep, count, t[0] == '1', wd)) {
	    Tcl_Obj *nv = TclBitsNewWorkObj(a, wd);

	    if (nv == NULL) {
		goto done;
	    }
	    Tcl_IncrRefCount(nv);
	    if (!TclAnnotSwapContent(a, nv)
		    || !TclBitsFillStride(nv, lo, bstep, count, t[0] == '1',
			wd)) {
		Tcl_DecrRefCount(nv);
		goto done;		/* rien d'écrit : le chemin ordinaire */
	    }
	    Tcl_DecrRefCount(nv);
	}
	ok = 1;
    } else {
	if (Tcl_ListObjLength(NULL, v, &n) != TCL_OK
		|| RangeSpec(NULL, &st, n, &first, &count, &stride) != TCL_OK
		|| (c = TclTypedConform(NULL, tp->elem, value)) == NULL) {
	    goto done;
	}
	Tcl_IncrRefCount(c);
	if (!TclIsCompact(v)) {
	    /* liste ordinaire de nombres, à la seule variable : la même
	     * valeur (vérifiée une fois) partagée par les cases choisies */
	    for (k = 0, i = first; k < count; k++, i += stride) {
		if (TclListObjSetElement(NULL, v, i, c) != TCL_OK) {
		    goto done;		/* (n'arrive pas : i est dans v) */
		}
	    }
	    TclInvalidateStringRep(v);
	    ok = 1;
	    goto written;
	}
	if (count > 0 && !TclCompactWriteScalar(v, first, 0, NULL, c)) {
	    goto done;
	}
	if (count > 1 && !TclCompactFillFrom(v, first, first + stride,
		count - 1, stride)) {
	    for (k = 1, i = first + stride; k < count; k++, i += stride) {
		TclCompactWriteScalar(v, i, 0, NULL, c);
	    }
	}
	TclInvalidateStringRep(v);
	ok = 1;
    }
  written:
    if (ok) {
	Tcl_InvalidateStringRep(a);
    }
  done:
    for (k = 0; k < np; k++) {
	TclDiscard(parts[k]);
    }
    if (c != NULL) {
	Tcl_DecrRefCount(c);
    }
    return ok;
}

/* L(i.j...) = value sur place (TclCompactWriteScalar) ; 1 si écrit. */
static int
CompactPathWrite(Tcl_Obj *a, Tcl_Obj *part2Ptr, Tcl_Obj *value)
{
    Tcl_Obj *v;
    TclType *tp;
    Tcl_Size n, idx;
    const char *names[16];
    char buf[256];
    int k;

    if (Tcl_IsShared(a) || !TclAnnotIsSettled(a)
	    || TclAnnotInfo(a, &v, &tp, NULL, NULL) != 1
	    || tp->kind != TCLT_LIST || !TclIsCompact(v)
	    || (k = DottedNames(part2Ptr, 16, names, buf, sizeof(buf))) < 2
	    || Tcl_ListObjLength(NULL, v, &n) != TCL_OK
	    || !SimpleListIndex(names[0], n, &idx)
	    || !TclCompactWriteScalar(v, idx, k - 1, names + 1, value)) {
	return 0;
    }
    Tcl_InvalidateStringRep(a);
    return 1;
}

/*
 * v17b : le champ nommé x d'un produit, de type {int}i | {double}d (le
 * nombre de Tcl : son texte canonique dit la variante, l'opérateur ne lit
 * que le contenu). Pas de cache : SimpleFieldIndex ne doit pas l'accepter.
 */
static Tcl_Size
NumUnionFieldIndex(TclType *tp, Tcl_Obj *part2Ptr)
{
    Tcl_Size idx, len;
    const char *name = Tcl_GetStringFromObj(part2Ptr, &len), *c;

    if (len == 0) {
	return -1;
    }
    for (c = name; *c != '\0'; c++) {
	if (!(isalnum(UCHAR(*c)) || *c == '_')) {
	    return -1;
	}
    }
    idx = TclTypeFieldIndex(tp, name);
    if (idx < 0 || idx >= tp->nfields
	    || !TclTypeIsNumUnion(tp->fields[idx].type)) {
	return -1;
    }
    return idx;
}

Tcl_Obj *
TclTypedFieldNumber(Var *varPtr, Tcl_Obj *part2Ptr)
{
    Tcl_Obj *a, *v, *e;
    TclType *tp, *ft;
    Tcl_Size idx;

    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    if (TclIsVarArray(varPtr) || !(varPtr->flags & VAR_TYPED)
	    || !TclIsVarDirectReadable(varPtr)) {
	return NULL;
    }
    a = varPtr->value.objPtr;
    if (!TclAnnotIsSettled(a)
	    || TclAnnotInfo(a, &v, &tp, NULL, NULL) != 1) {
	return NULL;
    }
    if (tp->kind == TCLT_LIST) {
	/* M(i.j) d'une matrice en cases : le nombre lu dans la case */
	e = CompactPathPart(a, part2Ptr, &ft);
	if (e != NULL && ft->kind != TCLT_INT && ft->kind != TCLT_DOUBLE) {
	    TclDiscard(e);
	    e = NULL;
	}
	return e;
    }
    if (tp->kind != TCLT_PRODUCT) {
	return NULL;
    }
    idx = SimpleFieldIndex(tp, part2Ptr);
    if (idx < 0) {
	idx = NumUnionFieldIndex(tp, part2Ptr);
	if (idx < 0) {
	    return NULL;
	}
    }
    ft = tp->fields[idx].type;
    if (ft->kind != TCLT_INT && ft->kind != TCLT_DOUBLE
	    && !TclTypeIsNumUnion(ft)) {	/* v17b : le nombre de Tcl */
	return NULL;
    }
    e = TclCompactFieldNumber(v, idx);
    if (e != NULL) {
	return e;
    }
    if (Tcl_ListObjIndex(NULL, v, idx, &e) != TCL_OK || e == NULL) {
	return NULL;
    }
    if (TclIsAnnotated(e)) {
	e = TCL_ANNOT_CONTENT(e);
    }
    return e;
}

/*
 * $P(*) d'un produit typé dont tous les champs sont simples : chaque champ
 * typé (comme $P(champ)), puis, comme le chemin général (MergeItems) : des
 * types tous identiques -> list.T des contenus ; sinon list.any des champs
 * typés. NULL : le chemin général.
 */
static Tcl_Obj *
FastAllParts(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *part2Ptr)
{
    Tcl_Obj *v, **parts, *list, *res;
    TclType *tp, *common = NULL;
    Tcl_Size len, i, n;
    int mixed = 0;
    const char *name = Tcl_GetStringFromObj(part2Ptr, &len);

    if (len != 1 || name[0] != '*' || !TclAnnotIsSettled(a)
	    || TclAnnotInfo(a, &v, &tp, NULL, NULL) != 1
	    || tp->kind != TCLT_PRODUCT || tp->nfields < 1) {
	return NULL;
    }
    n = tp->nfields;
    for (i = 0; i < n; i++) {
	if (!SimpleFieldType(tp->fields[i].type)
		|| TclTypeIsAny(tp->fields[i].type)) {
	    return NULL;
	}
	if (common == NULL) {
	    common = tp->fields[i].type;
	} else if (!TclTypeSame(common, tp->fields[i].type)) {
	    mixed = 1;
	}
    }
    parts = (Tcl_Obj **) Tcl_Alloc(n * sizeof(Tcl_Obj *));
    for (i = 0; i < n; i++) {
	Tcl_Obj *f = FieldAt(interp, v, tp->fields[i].type, i);

	if (f == NULL) {
	    while (i-- > 0) {
		Tcl_DecrRefCount(parts[i]);
	    }
	    Tcl_Free(parts);
	    return NULL;
	}
	if (!mixed && TclIsAnnotated(f)) {
	    Tcl_Obj *c = TCL_ANNOT_CONTENT(f);

	    Tcl_IncrRefCount(c);
	    TclDiscard(f);
	    f = c;
	} else {
	    Tcl_IncrRefCount(f);
	}
	parts[i] = f;
    }
    if (mixed) {
	common = TclTypeParse(interp, "any", 3);
    }
    list = Tcl_NewListObj(n, parts);
    for (i = 0; i < n; i++) {
	Tcl_DecrRefCount(parts[i]);
    }
    Tcl_Free(parts);
    if (common == NULL) {
	TclDiscard(list);
	return NULL;
    }
    /* la liste neuve devient le contenu (pas de copie) */
    res = TclNewAnnotatedOwn(list, TclTypeRefFor(interp,
	    TclTypeListOf(interp, common), -1));
    return res;
}

/*
 * Variantes qui travaillent sur un Var déjà résolu (exécuteur : variables
 * locales compilées, résolues sans nom).
 */
/*
 * 23/11 : P(*.champ) et P(*.(f,g,...)) sur une liste en cases de produits :
 * chaque colonne copiée d'un coup en une liste en cases (avant : la liste
 * des éléments refaite en objets, puis le champ de chacun). Mêmes types
 * que le chemin général : list.T pour un champ ; plusieurs, list.list.T si
 * leurs types sont les mêmes, sinon list.any de colonnes typées.
 */
static int
MIdentChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
	    || (c >= '0' && c <= '9') || c == '_';
}

static Tcl_Obj *
CompactColumnsRead(Tcl_Interp *interp, Tcl_Obj *a, Tcl_Obj *part2Ptr)
{
    Tcl_Obj *v, *cols[64], *res = NULL;
    TclType *lt, *pt, *ft0 = NULL, *outT;
    const char *s, *p, *q;
    Tcl_Size len, f, nf = 0, k;
    Tcl_Size fidx[64];
    int multi, same = 1;

    if (TclAnnotInfo(a, &v, &lt, NULL, NULL) != 1 || lt == NULL
	    || lt->kind != TCLT_LIST || (pt = lt->elem) == NULL
	    || pt->kind != TCLT_PRODUCT || !TclIsCompact(v)) {
	return NULL;
    }
    s = Tcl_GetStringFromObj(part2Ptr, &len);
    if (len < 3 || s[0] != '*' || s[1] != '.') {
	return NULL;
    }
    p = s + 2;
    multi = (*p == '(');
    if (multi) {
	if (s[len - 1] != ')') {
	    return NULL;
	}
	p++;
    }
    for (;;) {
	for (q = p; MIdentChar(*q); q++) {
	}
	if (q == p || nf == 64) {
	    return NULL;
	}
	for (f = 0; f < pt->nfields; f++) {
	    Tcl_Size nl;
	    const char *nm;

	    if (pt->fields[f].name == NULL) {
		continue;
	    }
	    nm = Tcl_GetStringFromObj(pt->fields[f].name, &nl);
	    if (nl == q - p && memcmp(nm, p, nl) == 0) {
		break;
	    }
	}
	if (f == pt->nfields || pt->fields[f].type == NULL
		|| pt->fields[f].type->kind == TCLT_SUM) {
	    return NULL;
	}
	fidx[nf++] = f;
	if (!multi) {
	    if (*q != '\0') {
		return NULL;
	    }
	    break;
	}
	if (*q == ',') {
	    p = q + 1;
	    continue;
	}
	if (*q == ')' && q[1] == '\0') {
	    break;
	}
	return NULL;
    }
    for (k = 0; k < nf; k++) {
	TclType *ft = pt->fields[fidx[k]].type;
	TclType *clt = TclTypeListOf(interp, ft);
	Tcl_Obj *c = (clt != NULL) ? TclCompactFieldColumn(v, fidx[k], clt)
		: NULL;

	if (c == NULL) {
	    while (k-- > 0) {
		Tcl_DecrRefCount(cols[k]);
	    }
	    return NULL;
	}
	cols[k] = TclNewAnnotated(interp, clt, -1, c);
	TclDiscard(c);
	Tcl_IncrRefCount(cols[k]);
	if (k == 0) {
	    ft0 = ft;
	} else if (ft != ft0) {
	    same = 0;
	}
    }
    if (!multi) {
	res = cols[0];			/* une référence pour l'appelant */
	return res;
    }
    outT = same ? TclTypeListOf(interp, TclTypeListOf(interp, ft0))
	    : TclTypeParse(interp, "list.any", 8);
    if (outT != NULL) {
	Tcl_Obj *l = Tcl_NewListObj(nf, cols);

	Tcl_IncrRefCount(l);
	res = TclNewAnnotated(interp, outT, -1, l);
	Tcl_DecrRefCount(l);
	Tcl_IncrRefCount(res);
    }
    for (k = 0; k < nf; k++) {
	Tcl_DecrRefCount(cols[k]);
    }
    return res;
}

/*
 * 23/11 : P(i.champ), i en chiffres, champ un nom simple, P une liste de
 * produits dont le type a une opération elemField : le champ lu sans
 * analyser le chemin (son texte change à chaque tour d'une boucle).
 */
static Tcl_Obj *
OpsFieldNamed(TclType *lt, const TclTypeOps *ops, Tcl_Obj *v, Tcl_Size i,
	const char *nm)
{
    Tcl_Size f;

    for (f = 0; f < lt->elem->nfields; f++) {
	if (lt->elem->fields[f].name != NULL
		&& strcmp(Tcl_GetString(lt->elem->fields[f].name), nm) == 0) {
	    return ops->elemField(lt, v, i, f);
	}
    }
    return NULL;
}

static Tcl_Obj *
OpsElemFieldRead(Tcl_Obj *a, Tcl_Obj *part2Ptr)
{
    Tcl_Obj *v;
    TclType *lt;
    const TclTypeOps *ops;
    const char *s, *p, *nm;
    Tcl_Size len, i = 0;

    if (TclAnnotInfo(a, &v, &lt, NULL, NULL) != 1 || lt == NULL
	    || lt->kind != TCLT_LIST || lt->elem == NULL
	    || lt->elem->kind != TCLT_PRODUCT
	    || (ops = TclTypeOpsOf(lt)) == NULL || ops->elemField == NULL) {
	return NULL;
    }
    s = Tcl_GetStringFromObj(part2Ptr, &len);
    for (p = s; *p >= '0' && *p <= '9'; p++) {
	if (i > (TCL_SIZE_MAX - 9) / 10) {
	    return NULL;
	}
	i = 10 * i + (*p - '0');
    }
    if (p == s || (p - s > 1 && s[0] == '0') || *p != '.') {
	return NULL;
    }
    nm = ++p;
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')) {
	return NULL;
    }
    for (; *p; p++) {
	if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
		|| (*p >= '0' && *p <= '9') || *p == '_')) {
	    return NULL;
	}
    }
    return OpsFieldNamed(lt, ops, v, i, nm);
}

/*
 * 23/11 : la même lecture, pour le bytecode de $P($i.champ) (indexConcat
 * suivi de loadArray) : IDX la valeur de $i, LIT le littéral ".champ".
 * NULL : le chemin ordinaire.
 */
Tcl_Obj *
TclTypedElemFieldFast(Tcl_Obj *a, Tcl_Obj *idx, Tcl_Obj *lit)
{
    Tcl_Obj *v;
    TclType *lt;
    const TclTypeOps *ops;
    const char *p, *nm;
    Tcl_WideInt w;

    if (TclAnnotInfo(a, &v, &lt, NULL, NULL) != 1 || lt == NULL
	    || lt->kind != TCLT_LIST || lt->elem == NULL
	    || lt->elem->kind != TCLT_PRODUCT
	    || (ops = TclTypeOpsOf(lt)) == NULL || ops->elemField == NULL) {
	return NULL;
    }
    if (TclHasInternalRep(idx, &tclIntType)) {
	w = idx->internalRep.wideValue;
    } else {
	const char *s = Tcl_GetString(idx);

	for (w = 0, p = s; *p >= '0' && *p <= '9'; p++) {
	    if (w > (WIDE_MAX - 9) / 10) {
		return NULL;
	    }
	    w = 10 * w + (*p - '0');
	}
	if (p == s || *p != '\0' || (p - s > 1 && s[0] == '0')) {
	    return NULL;
	}
    }
    if (w < 0 || w > TCL_SIZE_MAX) {
	return NULL;
    }
    p = Tcl_GetString(lit);
    if (*p++ != '.') {
	return NULL;
    }
    nm = p;
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')) {
	return NULL;
    }
    for (; *p; p++) {
	if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
		|| (*p >= '0' && *p <= '9') || *p == '_')) {
	    return NULL;
	}
    }
    return OpsFieldNamed(lt, ops, v, (Tcl_Size) w, nm);
}

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
    if (SpansHideSyntax(part2Ptr)) {
	/* une valeur substituée qui ressemble à un chemin : un nom entier,
	 * que seul le chemin général sait lire ainsi */
	if (PathReadObj(interp, varPtr->value.objPtr, part2Ptr, &res)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	*resultPtr = res;
	return TCL_OK;
    }
    if (part2Ptr->typePtr != &typePathType
	    && (res = OpsElemFieldRead(varPtr->value.objPtr, part2Ptr))
	    != NULL) {
	/* 23/11 : P(i.champ) par la spécialisation du type */
	if (TclTypeOpsMode() == TCL_TYPE_OPS_CHECK) {
	    Tcl_Obj *g = NULL;

	    Tcl_IncrRefCount(res);
	    if (PathReadObj(interp, varPtr->value.objPtr, part2Ptr, &g)
		    != TCL_OK) {
		Tcl_DecrRefCount(res);
		return TCL_ERROR;
	    }
	    TclTypeOpsSame("elemField", res, g);
	    Tcl_DecrRefCount(res);
	    *resultPtr = g;
	    return TCL_OK;
	}
    } else if (part2Ptr->typePtr == &typePathType
	    && ((CompiledPath *) part2Ptr->internalRep.twoPtrValue.ptr1)->chain) {
	/* a.b.c déjà compilé : la descente directe */
	res = FastChain(interp, varPtr->value.objPtr, &((CompiledPath *)
		part2Ptr->internalRep.twoPtrValue.ptr1)->set.path[0]);
    } else {
	res = FastFieldRead(interp, varPtr->value.objPtr, part2Ptr);
	if (res == NULL) {
	    res = FastAllParts(interp, varPtr->value.objPtr, part2Ptr);
	}
    }
    if (res == NULL) {
	TclType *pt;
	Tcl_Obj *r = CompactPathPart(varPtr->value.objPtr, part2Ptr, &pt);

	if (r != NULL) {
	    res = TclNewAnnotated(interp, pt, -1, r);
	    TclDiscard(r);
	}
    }
    if (res != NULL) {
	Tcl_IncrRefCount(res);
	*resultPtr = res;
	return TCL_OK;
    }
    if (TclTypeOpsMode() != TCL_TYPE_OPS_OFF
	    && ((res = CompactColumnsRead(interp, varPtr->value.objPtr,
	    part2Ptr)) != NULL || (res = ScalarRangeRead(interp,
	    varPtr->value.objPtr, part2Ptr)) != NULL)) {
	/* (une référence) */
	if (TclTypeOpsMode() == TCL_TYPE_OPS_CHECK) {
	    Tcl_Obj *g = NULL;

	    if (PathReadObj(interp, varPtr->value.objPtr, part2Ptr, &g)
		    != TCL_OK) {
		Tcl_DecrRefCount(res);
		return TCL_ERROR;
	    }
	    TclTypeOpsSame("columns", res, g);
	    Tcl_DecrRefCount(res);
	    res = g;
	}
	*resultPtr = res;
	return TCL_OK;
    }
    if (PathReadObj(interp, varPtr->value.objPtr, part2Ptr, &res)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    /* une référence pour l'appelant (TEBC la pousse sur la pile sans en
     * prendre une autre) : pas de passage par le résultat de l'interp */
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
    if (SpansHideSyntax(part2Ptr)) {
	if (PathWriteObj(interp, varPtr->value.objPtr, part2Ptr, 1,
		&valuePtr, &newA) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else if (CompactPathWrite(varPtr->value.objPtr, part2Ptr, valuePtr)
	    || BitsPathWrite(varPtr->value.objPtr, part2Ptr, valuePtr)
	    || RangePathWrite(varPtr->value.objPtr, part2Ptr, valuePtr)
	    || (TclTypeOpsMode() == TCL_TYPE_OPS_ON
		&& ColumnsPathWrite(varPtr->value.objPtr,
		    Tcl_GetString(part2Ptr), 1, &valuePtr))) {
	/* écrit dans la case : la variable garde sa valeur (traces) */
	newA = varPtr->value.objPtr;
	Tcl_IncrRefCount(newA);
    } else if (PathWriteObj(interp, varPtr->value.objPtr, part2Ptr, 1,
	    &valuePtr, &newA) != TCL_OK) {
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

/*
 * incr v(age), append v(nom) X, lappend v(notes) 3 : la partie est lue par
 * chemin, modifiée, puis écrite par chemin (le contrat et le type de la
 * partie sont vérifiés comme pour set v(age) ...). TCL_CONTINUE : pas une
 * variable scalaire typée, l'appelant garde son erreur.
 */
int
TclTypedVarModifyElementVar(Tcl_Interp *interp, Var *varPtr, Tcl_Obj *part1Ptr,
	int index, Tcl_Obj *part2Ptr, int op, Tcl_Obj *argPtr,
	Tcl_Obj **resultPtr)
{
    Tcl_Obj *cur = NULL, *c, *w, **ev;
    Tcl_Size ne, i;
    int rc;

    while (varPtr != NULL && TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    if (varPtr == NULL || TclIsVarArray(varPtr)
	    || !(varPtr->flags & VAR_TYPED) || varPtr->value.objPtr == NULL) {
	return TCL_CONTINUE;
    }
    if (PathReadObj(interp, varPtr->value.objPtr, part2Ptr, &cur) != TCL_OK) {
	return TCL_ERROR;
    }
    c = TCL_ANNOT_CONTENT(cur);
    switch (op) {
    case TCLTV_APPEND:
	w = Tcl_NewStringObj(Tcl_GetString(c), TCL_AUTO_LENGTH);
	if (argPtr != NULL) {
	    Tcl_AppendObjToObj(w, argPtr);
	}
	break;
    case TCLTV_LAPPEND:
    case TCLTV_LAPPEND_LIST:
	w = Tcl_DuplicateObj(c);
	if (op == TCLTV_LAPPEND) {
	    ev = &argPtr;
	    ne = (argPtr != NULL);
	} else if (Tcl_ListObjGetElements(interp, argPtr, &ne, &ev)
		!= TCL_OK) {
	    goto fail;
	}
	for (i = 0; i < ne; i++) {
	    if (Tcl_ListObjAppendElement(interp, w, ev[i]) != TCL_OK) {
		goto fail;
	    }
	}
	break;
    case TCLTV_INCR: {
	Tcl_Obj *one = NULL;

	w = Tcl_DuplicateObj(c);
	if (argPtr == NULL) {
	    TclNewIntObj(one, 1);
	    Tcl_IncrRefCount(one);
	}
	rc = TclIncrObj(interp, w, argPtr != NULL ? argPtr : one);
	if (one != NULL) {
	    Tcl_DecrRefCount(one);
	}
	if (rc != TCL_OK) {
	    goto fail;
	}
	break;
    }
    default:
	Tcl_DecrRefCount(cur);
	return TCL_CONTINUE;
    }
    Tcl_IncrRefCount(w);
    rc = TclTypedVarSetElementVar(interp, varPtr, part1Ptr, index, part2Ptr,
	    w, resultPtr);			/* le résultat de l'interp garde w */
    Tcl_DecrRefCount(w);
    Tcl_DecrRefCount(cur);
    return rc == TCL_CONTINUE ? TCL_ERROR : rc;

  fail:
    TclDiscard(w);
    Tcl_DecrRefCount(cur);
    return TCL_ERROR;
}

/* Par nom : part2Ptr NULL, part1Ptr écrit "v(chemin)" en un seul mot. */
int
TclTypedVarModifyElement(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
	Tcl_Obj *part2Ptr, int op, Tcl_Obj *argPtr, Tcl_Obj **resultPtr)
{
    Var *varPtr, *arrayPtr = NULL;
    Tcl_Obj *p1 = part1Ptr, *p2 = part2Ptr;
    int rc;

    if (part2Ptr == NULL) {
	Tcl_Size len;
	const char *nm = Tcl_GetStringFromObj(part1Ptr, &len);
	const char *open = (len > 2 && nm[len - 1] == ')') ? strchr(nm, '(')
		: NULL;

	if (open == NULL || open == nm) {
	    return TCL_CONTINUE;
	}
	p1 = Tcl_NewStringObj(nm, open - nm);
	p2 = Tcl_NewStringObj(open + 1, (nm + len - 1) - (open + 1));
	Tcl_IncrRefCount(p1);
	Tcl_IncrRefCount(p2);
    }
    varPtr = TclObjLookupVarEx(interp, p1, NULL, 0, "set", 0, 0, &arrayPtr);
    if (varPtr == NULL || arrayPtr != NULL) {
	rc = TCL_CONTINUE;
    } else {
	rc = TclTypedVarModifyElementVar(interp, varPtr, p1, -1, p2, op,
		argPtr, resultPtr);
    }
    if (part2Ptr == NULL) {
	Tcl_DecrRefCount(p1);
	Tcl_DecrRefCount(p2);
    }
    return rc;
}

/* Drapeaux de Tcl_ObjSetVar2 -> opération (0 : set). */
int
TclTypedVarFlagsOp(int flags)
{
    if (flags & TCL_LIST_ELEMENT) {
	return TCLTV_LAPPEND;
    }
    return (flags & TCL_APPEND_VALUE) ? TCLTV_APPEND : 0;
}
