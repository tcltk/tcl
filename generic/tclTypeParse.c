/*
 * tclTypeParse.c --
 *
 *	Analyseur et "compilateur" des expressions de types, bâtis sur le modèle
 *	de generic/tclCompExpr.c (ParseExpr, ParseLexeme, TclCompileExpr,
 *	CompileExprTree). Correspondance :
 *
 *	  tclCompExpr.c            ici
 *	  ParseExpr                ParseType
 *	  ParseLexeme              ParseTypeLexeme
 *	  TclCompileExpr           TclCompileType
 *	  CompileExprTree          TclCompileTypeTree
 *	  CompileEnv               TypeEnv
 *	  table instruction[]      table builder[]
 *
 *	GRAMMAIRE. Un type s'écrit avec des opérateurs :
 *
 *	  .   imbrication (binaire, associatif à droite, le plus fort)
 *	        list.dict   liste de dictionnaires
 *	        list.list.int
 *	  =   valeur par défaut d'un champ (binaire)
 *	        {num}real = 0
 *	  ;   produit (binaire)          {double}x; {double}y
 *	  ,   somme (binaire)            {int}i, {double}d
 *	  {   }nom   groupement, avec un nom optionnel COLLÉ à l'accolade
 *	        fermante. {double}x : champ x de type double. {list.int} :
 *	        simple groupement.
 *	  (   )      arité d'une collection, COLLÉE à list ou dict :
 *	        list          liste extensible
 *	        list(2)       exactement 2 éléments
 *	        dict          toutes les clés sont valides
 *	        dict(x, y)    seules x et y, toutes deux obligatoires
 *	        dict(x, z?)   z peut manquer
 *	        dict(x, z?=0) z peut manquer ; on lui donne alors la valeur 0
 *	        dict()        seule la clé vide
 *	      Les clés sont toujours du texte ; seules les valeurs sont typées
 *	      (dict(x, y).int). list() sans taille est une erreur ; la taille est
 *	      un nombre exact, pas un intervalle.
 *
 *	TROIS SORTES D'OPÉRANDES, chacune avec sa liste (comme litList et
 *	funcList dans ParseExpr) :
 *
 *	  type    mot nu, à gauche d'une "}" ou en position d'opérande
 *	          -> typeList, code OT_TYPE
 *	  nom     mot collé à droite d'une "}"
 *	          -> nameList, code OT_NAME (OT_EMPTY s'il n'y en a pas)
 *	  valeur  "entre guillemets", {entre accolades} ou mot nu après "="
 *	          -> valueList, code OT_VALUE
 *
 *	CLASSIFICATION DES OPÉRATEURS (comme LEAF / UNARY / BINARY) :
 *
 *	  LEAF    TYPEWORD, VALUEWORD, QUOTED, BRACED
 *	  UNARY   OPEN_BRACE "{"  (comme OPEN_PAREN), START
 *	  BINARY  NEST ".", ASSIGN "=", PRODUCT ";", SUM ",",
 *	          CLOSE_BRACE "}" (comme CLOSE_PAREN), END
 *
 *	Écarts volontaires avec tclCompExpr.c :
 *	  - "}" crée un noeud (le groupe nommé), alors que ")" n'en crée pas :
 *	    le nom est son opérande droit.
 *	  - ";" et "," ont la MÊME précédence et ne peuvent pas se mélanger
 *	    sans accolades (règle de la spécification, §4) ; l'analyseur le
 *	    vérifie quand il relie un opérande gauche.
 *	  - le champ OpNode.constant n'existe pas : il n'y a pas d'évaluation
 *	    de sous-arbres constants à la compilation d'un type.
 *	  - les tables sont écrites avec des initialiseurs désignés (C99).
 *
 *	Une valeur entre guillemets ou accolades passe par
 *	Tcl_ParseQuotedString / Tcl_ParseBraces, comme les mots de ParseExpr,
 *	puis TclWordKnownAtCompileTime : toute substitution est refusée, car un
 *	type est déclaratif.
 *
 *	Cet ensemble n'est pas compilé contre le vrai Tcl 9.0 ; les noms
 *	TclParseAllWhiteSpace, TclParseInit, TclGrowParseTokenArray,
 *	TclWordKnownAtCompileTime sont ceux de tclCompExpr.c (9.x).
 */

#include "tclTypesInt.h"
#include "tclCompile.h"

#ifndef UCHAR
#define UCHAR(c)	((unsigned char) (c))
#endif

/* --------------------------------------------------------------------
 * L'arbre : OpNode, OperandTypes, Marks
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
 * Un opérande qui n'est pas un sous-arbre est un code négatif. L'arbre ne
 * dit pas QUELLE feuille : les parcours visitent les feuilles dans l'ordre
 * du texte, et chaque sorte de feuille a sa liste.
 */

enum OperandTypes {
    OT_VARIABLE = -7,		/* opérande : $nom, dans valueList (texte source) */
    OT_SCRIPT = -6,		/* opérande : [script], dans valueList (le texte
				 * entre crochets) */
    OT_TYPEARGS = -5,		/* opérande : type avec arité, mot(args) ;
				 * dans typeList ET argList */
    OT_VALUE = -4,		/* opérande : valeur, dans valueList */
    OT_NAME = -3,		/* opérande : nom, dans nameList */
    OT_TYPE = -2,		/* opérande : type, dans typeList */
    OT_EMPTY = -1		/* "opérande" absent : groupe sans nom */
};

#define IsValueCode(l)	((l) == OT_VALUE || (l) == OT_SCRIPT \
			    || (l) == OT_VARIABLE)
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
    NODE_TYPE = 0xC0,		/* les deux bits de poids fort */
    BINARY = 0x40,		/* opérateur binaire : un OpNode, 2 opérandes */
    UNARY = 0x80,		/* opérateur unaire : un OpNode, 1 opérande */
    LEAF = 0xC0			/* feuille : pas d'OpNode */
};

enum LexemeCodes {
    /* Lexèmes non catégorisés */

    BAREWORD = 3,		/* Ambigu : TYPEWORD (le contexte tranche). */
    INVALID = 5,		/* Erreur : ponctuation non supportée. */
    COMMENT = 6,		/* "#" jusqu'à la fin de la ligne. */
    UNCLOSED = 7,		/* Erreur : "(" sans ")". */

    /* Lexèmes feuilles */

    TYPEWORD = LEAF | 1,	/* un nom de type : int, pixel, list */
    VALUEWORD = LEAF | 2,	/* mot nu après "=" : 0, -1.5, red */
    QUOTED = LEAF | 3,		/* "valeur" (littéral d'énumération ou défaut) */
    BRACED = LEAF | 4,		/* {valeur} après "=" seulement */
    SCRIPT = LEAF | 5,		/* [script] après "=" : défaut calculé */
    VARIABLE = LEAF | 6,	/* $nom après "=" : paramètre ou variable */

    /* Lexèmes unaires */

    START = UNARY | 4,		/* Racine de l'arbre, fabriquée par ParseType. */
    OPEN_BRACE = UNARY | 5,	/* "{" : l'analogue de OPEN_PAREN. */

    /* Lexèmes binaires */

    SUM = BINARY | 3,		/* "," */
    PRODUCT = BINARY | 4,	/* ";" */
    NEST = BINARY | 5,		/* "." : associatif à droite, comme EXPON. */
    ASSIGN = BINARY | 6,	/* "=" */
    CLOSE_BRACE = BINARY | 7,	/* "}" : comme CLOSE_PAREN, sauf qu'il crée
				 * un noeud (le groupe nommé). */
    END = BINARY | 8		/* Fin du texte : s'apparie avec START. */
};

/*
 * Précédences, de la plus faible à la plus forte. "," lie plus fort que
 * ";" : {int}a; {double}d, {int}i est un produit de deux champs, dont le
 * second (sans nom) est la somme {double}d, {int}i.
 */

enum Precedence {
    PREC_END = 1,		/* END */
    PREC_START,			/* START */
    PREC_CLOSE_BRACE,		/* "}" */
    PREC_OPEN_BRACE,		/* "{" */
    PREC_PRODUCT,		/* ";" */
    PREC_SUM,			/* "," */
    PREC_ASSIGN,		/* "=" */
    PREC_NEST			/* "." */
};

/* Précédence d'un lexème : consultation directe. */

static const unsigned char prec[] = {
    [SUM] = PREC_SUM,
    [PRODUCT] = PREC_PRODUCT,
    [NEST] = PREC_NEST,
    [ASSIGN] = PREC_ASSIGN,
    [CLOSE_BRACE] = PREC_CLOSE_BRACE,
    [END] = PREC_END,
    [START] = PREC_START,
    [OPEN_BRACE] = PREC_OPEN_BRACE
};

/*
 * L'analogue de la table instruction[] : au lieu d'un opcode, l'opération
 * de construction exécutée en remontant dans l'arbre (post-ordre).
 */

enum TypeOps {
    TB_NONE = 0,		/* START, OPEN_BRACE : rien à faire */
    TB_GROUP,			/* "}" : groupement / champ nommé */
    TB_SUM,			/* "," */
    TB_PRODUCT,			/* ";" */
    TB_NEST,			/* "." */
    TB_ASSIGN			/* "=" */
};

static const unsigned char builder[] = {
    [SUM] = TB_SUM,
    [PRODUCT] = TB_PRODUCT,
    [NEST] = TB_NEST,
    [ASSIGN] = TB_ASSIGN,
    [CLOSE_BRACE] = TB_GROUP,
    [END] = TB_NONE,
    [START] = TB_NONE,
    [OPEN_BRACE] = TB_NONE
};

/* Table octet -> lexème, pour ParseTypeLexeme (0 : à analyser plus loin). */

static const unsigned char Lexeme[] = {
    ['"'] = QUOTED,
    ['{'] = OPEN_BRACE,
    ['}'] = CLOSE_BRACE,
    [','] = SUM,
    [';'] = PRODUCT,
    ['.'] = NEST,
    ['='] = ASSIGN,
    ['!'] = INVALID, ['$'] = INVALID, ['%'] = INVALID, ['&'] = INVALID,
    ['\''] = INVALID, ['('] = INVALID, [')'] = INVALID, ['*'] = INVALID,
    ['+'] = INVALID, ['-'] = INVALID, ['/'] = INVALID, [':'] = INVALID,
    ['<'] = INVALID, ['>'] = INVALID, ['?'] = INVALID, ['@'] = INVALID,
    ['['] = INVALID, ['\\'] = INVALID, [']'] = INVALID, ['^'] = INVALID,
    ['`'] = INVALID, ['|'] = INVALID, ['~'] = INVALID, [127] = INVALID
};

/* --------------------------------------------------------------------
 * Caractères
 * -------------------------------------------------------------------- */

/* Un nom de type ou de champ : lettres, chiffres, "_" et UTF-8. */

int
TclTypeIsNameChar(int c)
{
    c = UCHAR(c);
    return isalnum(c) || c == '_' || (c & 0x80);
}

/* Un mot nu de valeur (après "=") : tout sauf blanc et ponctuation. */

static int
IsValueChar(int c)
{
    c = UCHAR(c);
    return !isspace(c) && c != ';' && c != ',' && c != '{' && c != '}'
	    && c != '"' && c != '=';
}

/*
 * Trouve la ")" qui ferme une "(" déjà lue. On saute les chaînes "..." et
 * les accolades {...}, pour qu'une valeur par défaut puisse contenir ")" ou
 * ",". Rend NULL si elle manque.
 */

const char *
TclTypeFindArgsEnd(const char *p, const char *limit)
{
    int depth = 0, pdepth = 0;

    while (p < limit) {
	char c = *p;

	if (c == '\\') {
	    p += 2;
	    continue;
	}
	if (c == '"') {
	    for (p++; p < limit && *p != '"'; p++) {
		if (*p == '\\') {
		    p++;
		}
	    }
	    if (p >= limit) {
		return NULL;
	    }
	    p++;
	    continue;
	}
	if (c == '{') {
	    depth++;
	} else if (c == '}') {
	    depth--;
	} else if (c == '(' && depth <= 0) {
	    pdepth++;
	} else if (c == ')' && depth <= 0) {
	    if (pdepth == 0) {
		return p;
	    }
	    pdepth--;
	}
	p++;
    }
    return NULL;
}

/* --------------------------------------------------------------------
 * ParseTypeLexeme --
 *
 *	Analyse UN lexème au début de la chaîne, sans lire plus de numBytes
 *	octets. expectValue vaut 1 juste après un "=" : un mot nu y est alors
 *	une VALEUR (0, -1.5, red), et "{" ouvre une valeur entre accolades,
 *	pas un groupe.
 *
 *	Pour "}" le nom collé est lu avec le lexème et rendu dans *literalPtr
 *	(NULL s'il n'y en a pas).
 *
 *	Pour un mot nu suivi de "(" COLLÉ, les arguments (le texte entre les
 *	parenthèses) sont rendus dans *argsPtr ; sinon *argsPtr vaut NULL.
 *
 * Résultat : le nombre d'octets lus. Pour QUOTED et BRACED, ParseType
 *	recalcule la longueur avec Tcl_ParseQuotedString / Tcl_ParseBraces.
 * -------------------------------------------------------------------- */

/* $nom, ${nom}, $::ns::nom, $a(i) : un jeton variable, comme dans une
 * expression ; "$" suivi d'un chiffre ou de rien reste un texte. */

static int
IsVariableStart(const char *s, Tcl_Size n)
{
    if (n < 2 || s[0] != '$') {
	return 0;
    }
    return (TclTypeIsNameChar(s[1]) && !isdigit(UCHAR(s[1])))
	    || s[1] == '{' || (n > 2 && s[1] == ':' && s[2] == ':');
}

/* $nom : référence à un paramètre (de type, en position de type). */

static int
IsParamRef(const char *start, Tcl_Size numBytes)
{
    return numBytes > 1 && *start == '$' && TclTypeIsNameChar(start[1])
	    && !isdigit(UCHAR(start[1]));
}

static Tcl_Size
ParseTypeLexeme(
    const char *start,		/* Début du lexème. */
    Tcl_Size numBytes,		/* Nombre d'octets disponibles. */
    int expectValue,		/* 1 si le lexème précédent est "=". */
    unsigned char *lexemePtr,	/* Reçoit le code du lexème. */
    Tcl_Obj **literalPtr,	/* Reçoit le texte (type, valeur ou nom). */
    Tcl_Obj **argsPtr)		/* Reçoit les arguments de mot(args). */
{
    const char *end;
    unsigned char byte;

    if (literalPtr != NULL) {
	*literalPtr = NULL;
    }
    if (argsPtr != NULL) {
	*argsPtr = NULL;
    }
    if (numBytes == 0) {
	*lexemePtr = END;
	return 0;
    }
    byte = UCHAR(*start);

    if (expectValue) {
	if (byte == '[') {
	    *lexemePtr = SCRIPT;
	    return 1;
	}
	if (IsVariableStart(start, numBytes)) {
	    *lexemePtr = VARIABLE;
	    return 1;
	}
	if (byte == '"') {
	    *lexemePtr = QUOTED;
	    return 1;
	}
	if (byte == '{') {
	    *lexemePtr = BRACED;
	    return 1;
	}
	end = start;
	while ((end < start + numBytes) && IsValueChar(*end)) {
	    end++;
	}
	if (end > start) {
	    *lexemePtr = VALUEWORD;
	    if (literalPtr != NULL) {
		*literalPtr = Tcl_NewStringObj(start, end - start);
	    }
	    return end - start;
	}
	/* Pas de valeur : ";" "," "}" ou fin. ParseType signalera
	 * l'opérande manquant. */
    }

    if (byte == '?' && numBytes > 1 && start[1] == '=') {
	/* "?=" : synonyme de "=" (même écriture que pour dict(z?=0)) */

	*lexemePtr = ASSIGN;
	return 2;
    }

    if (!IsParamRef(start, numBytes) && (byte < sizeof(Lexeme))
	    && (Lexeme[byte] != 0)) {
	*lexemePtr = Lexeme[byte];
	if (*lexemePtr == CLOSE_BRACE) {
	    /* Le nom, s'il y en a un, est COLLÉ à l'accolade fermante. */

	    end = start + 1;
	    while ((end < start + numBytes) && TclTypeIsNameChar(*end)) {
		end++;
	    }
	    if ((end > start + 1) && (literalPtr != NULL)) {
		*literalPtr = Tcl_NewStringObj(start + 1, end - start - 1);
	    }
	    return end - start;
	}
	return 1;
    }

    if (byte == '#') {
	Tcl_Size size;

	for (size = 0; (size < numBytes) && (start[size] != '\n'); size++) {
	    /* vide */
	}
	*lexemePtr = COMMENT;
	return size;
    }

    if (TclTypeIsNameChar(byte) || IsParamRef(start, numBytes)) {
	end = start + (byte == '$');
	while ((end < start + numBytes) && TclTypeIsNameChar(*end)) {
	    end++;
	}
	*lexemePtr = BAREWORD;
	if (literalPtr != NULL) {
	    *literalPtr = Tcl_NewStringObj(start, end - start);
	}
	if ((end < start + numBytes) && (*end == '(')) {
	    /* mot(arguments) : l'arité d'une collection */

	    const char *close = TclTypeFindArgsEnd(end + 1, start + numBytes);

	    if (close == NULL) {
		*lexemePtr = UNCLOSED;
		return end - start + 1;
	    }
	    if (argsPtr != NULL) {
		*argsPtr = Tcl_NewStringObj(end + 1, close - end - 1);
	    }
	    return close + 1 - start;
	}
	return end - start;
    }

    *lexemePtr = INVALID;
    return 1;
}

/* --------------------------------------------------------------------
 * ParseQuotedOrBraced --
 *
 *	Une valeur "..." ou {...} : on s'appuie sur Tcl_ParseQuotedString et
 *	Tcl_ParseBraces, comme ParseExpr pour ses mots, puis on exige que le
 *	mot soit connu à la compilation (aucune substitution).
 * -------------------------------------------------------------------- */

static int
ParseQuotedOrBraced(
    Tcl_Interp *interp,
    const char *start,
    Tcl_Size numBytes,
    int lexeme,			/* QUOTED ou BRACED */
    Tcl_Obj **valuePtr,		/* Reçoit la valeur (refcount 0). */
    Tcl_Size *scannedPtr,	/* Reçoit le nombre d'octets lus. */
    Tcl_Obj **msgPtr,
    const char **errCodePtr)
{
    Tcl_Parse parse;
    Tcl_Token *tokenPtr;
    const char *end = start;
    Tcl_Obj *literal;
    int code;

    TclParseInit(interp, start, numBytes, &parse);
    TclGrowParseTokenArray(&parse, 2);
    tokenPtr = parse.tokenPtr;
    tokenPtr->type = TCL_TOKEN_WORD;
    tokenPtr->start = start;
    parse.numTokens++;

    if (lexeme == QUOTED) {
	code = Tcl_ParseQuotedString(NULL, start, numBytes, &parse, 1, &end);
    } else {
	code = Tcl_ParseBraces(NULL, start, numBytes, &parse, 1, &end);
    }
    if (code != TCL_OK) {
	*msgPtr = Tcl_ObjPrintf((lexeme == QUOTED)
		? "missing close-quote" : "missing close-brace in value");
	*errCodePtr = "UNBALANCED";
	*scannedPtr = 1;
	Tcl_FreeParse(&parse);
	return TCL_ERROR;
    }

    tokenPtr = parse.tokenPtr;
    tokenPtr->size = end - start;
    tokenPtr->numComponents = parse.numTokens - 1;
    TclNewObj(literal);
    if (!TclWordKnownAtCompileTime(tokenPtr, literal)) {
	TclDiscard(literal);
	*msgPtr = Tcl_ObjPrintf("substitutions are not allowed in a type "
		"definition");
	*errCodePtr = "SUBST";
	*scannedPtr = end - start;
	Tcl_FreeParse(&parse);
	return TCL_ERROR;
    }
    Tcl_FreeParse(&parse);
    *valuePtr = literal;
    *scannedPtr = end - start;
    return TCL_OK;
}

/* --------------------------------------------------------------------
 * ParseScriptWord, ParseVariableWord --
 *
 *	Les jetons « script » et « variable » d'un défaut, lus par l'analyseur
 *	de Tcl comme le fait ParseExpr pour [commande] et $variable :
 *	Tcl_ParseCommand (en mode imbriqué : un "]" non apparié termine) et
 *	Tcl_ParseVarName. Le texte est conservé tel quel : il est évalué à
 *	chaque construction d'une valeur, jamais à la définition du type.
 * -------------------------------------------------------------------- */

static int
ParseScriptWord(
    const char *start,		/* sur le "[" */
    Tcl_Size numBytes,
    Tcl_Obj **valuePtr,		/* le texte entre les crochets (refcount 0) */
    Tcl_Size *scannedPtr,	/* octets lus, crochets compris */
    Tcl_Obj **msgPtr,
    const char **errCodePtr)
{
    const char *p = start + 1, *end = start + numBytes;

    while (p < end) {
	Tcl_Parse parse;
	int code, closed;

	code = Tcl_ParseCommand(NULL, p, end - p, 1, &parse);
	if (code != TCL_OK) {
	    *msgPtr = Tcl_ObjPrintf(parse.incomplete
		    ? "missing close-bracket in the default script"
		    : "syntax error in the default script");
	    *errCodePtr = parse.incomplete ? "UNBALANCED" : "SCRIPT";
	    *scannedPtr = 1;
	    Tcl_FreeParse(&parse);
	    return TCL_ERROR;
	}
	closed = (parse.term < end) && (*parse.term == ']');
	if (closed) {
	    *valuePtr = Tcl_NewStringObj(start + 1, parse.term - start - 1);
	    *scannedPtr = parse.term + 1 - start;
	    Tcl_FreeParse(&parse);
	    return TCL_OK;
	}
	p = parse.commandStart + parse.commandSize;
	Tcl_FreeParse(&parse);
    }
    *msgPtr = Tcl_ObjPrintf("missing close-bracket in the default script");
    *errCodePtr = "UNBALANCED";
    *scannedPtr = 1;
    return TCL_ERROR;
}

static int
ParseVariableWord(
    const char *start,		/* sur le "$" */
    Tcl_Size numBytes,
    Tcl_Obj **valuePtr,		/* le texte source "$...", refcount 0 */
    Tcl_Size *scannedPtr,
    Tcl_Obj **msgPtr,
    const char **errCodePtr)
{
    Tcl_Parse parse;

    if (Tcl_ParseVarName(NULL, start, numBytes, &parse, 0) != TCL_OK
	    || parse.numTokens < 2
	    || parse.tokenPtr[0].type != TCL_TOKEN_VARIABLE) {
	Tcl_FreeParse(&parse);
	*msgPtr = Tcl_ObjPrintf("invalid variable reference");
	*errCodePtr = "VARIABLE";
	*scannedPtr = 1;
	return TCL_ERROR;
    }
    *scannedPtr = parse.tokenPtr[0].size;
    *valuePtr = Tcl_NewStringObj(start, parse.tokenPtr[0].size);
    Tcl_FreeParse(&parse);
    return TCL_OK;
}

static int VariableDefault(Tcl_Interp *interp, const TypeEnv *envPtr,
	Tcl_Obj *ref, Tcl_Obj **defvalPtr, Tcl_Obj **defcmdPtr);

/* --------------------------------------------------------------------
 * AppendTypeContext --
 *
 *	Ajoute au message la citation du texte autour de l'erreur, avec la
 *	marque _@_ quand l'erreur est une position (comme ParseExpr).
 * -------------------------------------------------------------------- */

static void
AppendTypeContext(
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
    Tcl_AppendPrintfToObj(msg, "\nin type \"%s%.*s%.*s%s%.*s%s\"",
	    (from > text) ? "..." : "", (int) (start - from), from,
	    (int) scanned, start, insertMark ? mark : "",
	    (int) (to - after), after, (to < textEnd) ? "..." : "");
}

/*
 * Tolérance du ";" : un séparateur de produit au début, à la fin d'un
 * groupe ou du texte, ou répété (";;"), est ignoré, comme les instructions
 * vides d'un script Tcl. TrailingSemi regarde ce qui suit un ";" : après
 * les blancs, les ";" et les commentaires, vient-il la fin du texte ou un
 * "}" ?
 */

static int
TrailingSemi(const char *p, Tcl_Size n)
{
    while (1) {
	unsigned char lex;
	Tcl_Size used = TclParseAllWhiteSpace(p, n);

	p += used;
	n -= used;
	if (n > 0 && *p == ';') {
	    p++;
	    n--;
	    continue;
	}
	used = ParseTypeLexeme(p, n, 0, &lex, NULL, NULL);
	if (lex == COMMENT) {
	    p += used;
	    n -= used;
	    continue;
	}
	return (lex == END) || (lex == CLOSE_BRACE);
    }
}

/* --------------------------------------------------------------------
 * ParseType --
 *
 *	Analyse le texte d'un type et construit l'arbre d'opérateurs, la liste
 *	des types, la liste des noms et la liste des valeurs. L'appelant passe
 *	trois listes vides.
 *
 * Résultat :
 *	TCL_OK avec *opTreePtr rempli (à libérer par Tcl_Free), ou TCL_ERROR
 *	et un message dans interp si interp != NULL.
 * -------------------------------------------------------------------- */

static int
ParseType(
    Tcl_Interp *interp,		/* Pour les messages d'erreur. */
    const char *start,		/* Début du texte. */
    Tcl_Size numBytes,		/* Longueur du texte. */
    OpNode **opTreePtr,		/* Reçoit l'arbre alloué. */
    Tcl_Obj *typeList,		/* Reçoit les opérandes de type. */
    Tcl_Obj *nameList,		/* Reçoit les noms collés après "}". */
    Tcl_Obj *valueList,		/* Reçoit les opérandes de valeur. */
    Tcl_Obj *argList)		/* Reçoit les arguments de mot(args). */
{
    OpNode *nodes = NULL;
    unsigned int nodesAvailable = 64;
    unsigned int nodesUsed = 0;
    Tcl_Size scanned = 0;
    int lastParsed;		/* Dernier lexème : index d'opérateur, ou un
				 * code OperandTypes. */
    int incomplete;		/* Sommet de la pile des arbres incomplets. */
    int complete = OT_EMPTY;	/* Arbre complet courant. */
    const char *const text = start;
    const char *const textEnd = start + numBytes;
    Tcl_Obj *literal = NULL;
    Tcl_Obj *args = NULL;
    Tcl_Obj *msg = NULL;
    const char *errCode = NULL;
    const char *const mark = "_@_";
    int insertMark = 0;
    const Tcl_Size limit = 25;

    nodes = (OpNode *) Tcl_AttemptAlloc(nodesAvailable * sizeof(OpNode));
    if (nodes == NULL) {
	msg = Tcl_ObjPrintf("not enough memory to parse type");
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
	int expectValue;

	/* Chaque passage ajoute au plus un OpNode. */

	if (nodesUsed >= nodesAvailable) {
	    unsigned int size = nodesUsed * 2;
	    OpNode *newPtr = (OpNode *) Tcl_AttemptRealloc(nodes,
		    size * sizeof(OpNode));

	    if (newPtr == NULL) {
		msg = Tcl_ObjPrintf("not enough memory to parse type");
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

	/* Après "=", un mot nu est une valeur. */

	expectValue = IsOperator(lastParsed)
		&& (nodes[lastParsed].lexeme == ASSIGN);
	literal = NULL;
	args = NULL;
	scanned = ParseTypeLexeme(start, numBytes, expectValue, &lexeme,
		&literal, &args);

	/* Catégorise les lexèmes ambigus d'après le contexte. */

	if ((NODE_TYPE & lexeme) == 0) {
	    switch (lexeme) {
	    case COMMENT:
		start += scanned;
		numBytes -= scanned;
		continue;
	    case INVALID:
		msg = Tcl_ObjPrintf("invalid character \"%.*s\"",
			(int) scanned, start);
		errCode = "BADCHAR";
		goto error;
	    case UNCLOSED:
		msg = Tcl_ObjPrintf("missing close-parenthesis");
		errCode = "UNBALANCED";
		goto error;
	    case BAREWORD:
		lexeme = TYPEWORD;
		break;
	    }
	}

	if (lexeme == PRODUCT) {
	    int skip = 0;

	    if (IsOperator(lastParsed)
		    && ((nodes[lastParsed].lexeme == START)
		    || (nodes[lastParsed].lexeme == OPEN_BRACE)
		    || (nodes[lastParsed].lexeme == PRODUCT))) {
		skip = 1;		/* ";" initial ou répété */
	    } else if (TrailingSemi(start + scanned, numBytes - scanned)) {
		skip = 1;		/* ";" final */
	    }
	    if (skip) {
		start += scanned;
		numBytes -= scanned;
		continue;
	    }
	}

	switch (NODE_TYPE & lexeme) {
	case LEAF: {
	    /*
	     * Une feuille range un texte dans la liste de son espèce. Aucun
	     * OpNode n'est rempli.
	     */

	    Tcl_Obj *value = NULL;

	    /* Une feuille juste après autre chose qu'un opérateur : erreur
	     * (c'est aussi le cas d'un nom séparé de "}" par un blanc). */

	    if (NotOperator(lastParsed)) {
		msg = Tcl_ObjPrintf("missing operator at %s", mark);
		errCode = "MISSING";
		scanned = 0;
		insertMark = 1;
		goto error;
	    }

	    switch (lexeme) {
	    case TYPEWORD:
		Tcl_ListObjAppendElement(NULL, typeList, literal);
		literal = NULL;
		if (args != NULL) {
		    Tcl_ListObjAppendElement(NULL, argList, args);
		    args = NULL;
		    complete = lastParsed = OT_TYPEARGS;
		} else {
		    complete = lastParsed = OT_TYPE;
		}
		break;
	    case VALUEWORD:
		Tcl_ListObjAppendElement(NULL, valueList, literal);
		literal = NULL;
		complete = lastParsed = OT_VALUE;
		break;
	    case QUOTED:
	    case BRACED:
		if (ParseQuotedOrBraced(interp, start, numBytes, lexeme,
			&value, &scanned, &msg, &errCode) != TCL_OK) {
		    goto error;
		}
		Tcl_ListObjAppendElement(NULL, valueList, value);
		complete = lastParsed = OT_VALUE;
		break;
	    case SCRIPT:
		if (ParseScriptWord(start, numBytes, &value, &scanned,
			&msg, &errCode) != TCL_OK) {
		    goto error;
		}
		Tcl_ListObjAppendElement(NULL, valueList, value);
		complete = lastParsed = OT_SCRIPT;
		break;
	    case VARIABLE:
		if (ParseVariableWord(start, numBytes, &value, &scanned,
			&msg, &errCode) != TCL_OK) {
		    goto error;
		}
		Tcl_ListObjAppendElement(NULL, valueList, value);
		complete = lastParsed = OT_VARIABLE;
		break;
	    }
	    break;
	}

	case UNARY:

	    /* "{" doit suivre un opérateur (ou START). */

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

	    /* Nouvel arbre incomplet : on l'empile. */

	    nodePtr->prev = incomplete;
	    incomplete = lastParsed = nodesUsed;
	    nodesUsed++;
	    break;

	case BINARY: {
	    OpNode *incompletePtr;
	    unsigned char precedence = prec[lexeme];

	    /* Un binaire juste après un opérateur : il manque un opérande. */

	    if (IsOperator(lastParsed)) {
		if ((lexeme == CLOSE_BRACE)
			&& (nodePtr[-1].lexeme == OPEN_BRACE)) {
		    msg = Tcl_ObjPrintf("empty group at %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    errCode = "EMPTY";
		    goto error;
		}
		if (nodePtr[-1].precedence > precedence) {
		    if (nodePtr[-1].lexeme == OPEN_BRACE) {
			msg = Tcl_ObjPrintf("unbalanced open brace");
			errCode = "UNBALANCED";
		    } else if (nodePtr[-1].lexeme == START) {
			msg = Tcl_ObjPrintf("empty type expression");
			errCode = "EMPTY";
		    }
		} else if (lexeme == CLOSE_BRACE) {
		    msg = Tcl_ObjPrintf("unbalanced close brace");
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
	     * Ici l'arbre se referme. On a une pile d'arbres incomplets en
	     * attente d'un opérande droit, puis un arbre complet, puis le
	     * binaire qu'on vient de lire. Selon les précédences, l'arbre
	     * complet devient l'opérande droit du dernier incomplet (et on
	     * recommence) ou l'opérande gauche du nouvel opérateur.
	     */

	    while (1) {
		incompletePtr = nodes + incomplete;

		if (incompletePtr->precedence < precedence) {
		    break;
		}

		if (incompletePtr->precedence == precedence) {
		    /* "." est associatif à droite, comme EXPON. */

		    if (lexeme == NEST) {
			break;
		    }
		}

		/* Les accolades doivent s'apparier. */

		if ((incompletePtr->lexeme == OPEN_BRACE)
			&& (lexeme != CLOSE_BRACE)) {
		    msg = Tcl_ObjPrintf("unbalanced open brace");
		    errCode = "UNBALANCED";
		    goto error;
		}

		/* L'opérande droit de "=" est une valeur. */

		if ((incompletePtr->lexeme == ASSIGN)
			&& !IsValueCode(complete)) {
		    msg = Tcl_ObjPrintf("the right operand of \"=\" must be "
			    "a value, before %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    errCode = "ASSIGN";
		    goto error;
		}

		/* Une valeur n'est pas un opérande de ".". */

		if ((incompletePtr->lexeme == NEST) && IsValueCode(complete)) {
		    msg = Tcl_ObjPrintf("a value cannot be an operand of "
			    "\".\", before %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    errCode = "NEST";
		    goto error;
		}

		/* Rattache l'arbre complet comme opérande droit. */

		incompletePtr->right = complete;
		if (IsOperator(complete)) {
		    nodes[complete].parent = incomplete;
		}

		if (incompletePtr->lexeme == START) {
		    /* L'arbre START est complet : on a fini. */

		    *opTreePtr = nodes;
		    return TCL_OK;
		}

		/* Cet incomplet devient le complet ; on le dépile. */

		complete = incomplete;
		incomplete = incompletePtr->prev;

		/* "}" ne referme qu'une "{". */

		if (incompletePtr->lexeme == OPEN_BRACE) {
		    break;
		}
	    }

	    /* Les accolades doivent s'apparier. */

	    if (lexeme == CLOSE_BRACE) {
		if (incompletePtr->lexeme != OPEN_BRACE) {
		    msg = Tcl_ObjPrintf("unbalanced close brace");
		    errCode = "UNBALANCED";
		    goto error;
		}

		/*
		 * Contrairement à ")", "}" crée un noeud : le groupe
		 * "{...}nom". Son opérande gauche est le noeud "{" (dont
		 * l'opérande est l'intérieur), son opérande droit le nom
		 * collé (OT_NAME) ou rien (OT_EMPTY). Le groupe est un arbre
		 * COMPLET : il n'est pas empilé.
		 */

		nodePtr->lexeme = lexeme;
		nodePtr->precedence = precedence;
		nodePtr->mark = MARK_LEFT;
		nodePtr->left = complete;
		nodes[complete].parent = nodesUsed;
		if (literal != NULL) {
		    Tcl_ListObjAppendElement(NULL, nameList, literal);
		    literal = NULL;
		    nodePtr->right = OT_NAME;
		} else {
		    nodePtr->right = OT_EMPTY;
		}
		complete = nodesUsed;
		lastParsed = nodePtr->right;	/* un opérande : un binaire
						 * doit suivre */
		nodesUsed++;
		break;
	    }

	    /* Contrôles sur l'opérande gauche. */

	    if (lexeme == ASSIGN) {
		if (NotOperator(complete)
			|| (nodes[complete].lexeme != CLOSE_BRACE)
			|| (nodes[complete].right != OT_NAME)) {
		    msg = Tcl_ObjPrintf("the left operand of \"=\" must be a "
			    "named field {type}name, before %s", mark);
		    scanned = 0;
		    insertMark = 1;
		    errCode = "ASSIGN";
		    goto error;
		}
	    }
	    if ((lexeme == NEST) && IsValueCode(complete)) {
		msg = Tcl_ObjPrintf("a value cannot be an operand of \".\", "
			"before %s", mark);
		scanned = 0;
		insertMark = 1;
		errCode = "NEST";
		goto error;
	    }
	    /* Relie l'arbre complet comme opérande gauche du nouveau noeud. */

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

	/* Avance après le lexème. */

	start += scanned;
	numBytes -= scanned;
    }

    /* On n'arrive ici qu'en cas d'erreur. */

  error:
    if (literal != NULL) {
	TclDiscard(literal);
    }
    if (args != NULL) {
	TclDiscard(args);
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
	    msg = Tcl_ObjPrintf("syntax error in type");
	}
	AppendTypeContext(msg, text, textEnd, start, scanned, insertMark,
		mark, limit);
	Tcl_SetObjResult(interp, msg);
	if (errCode != NULL) {
	    Tcl_SetErrorCode(interp, "TCL", "PARSE", "TYPE", errCode,
		    (char *) NULL);
	}
    }
    return TCL_ERROR;
}

/* --------------------------------------------------------------------
 * La pile d'opérandes de TclCompileTypeTree
 *
 *	L'analogue de la pile d'évaluation du bytecode : les feuilles y
 *	poussent leur valeur, chaque opérateur dépile ses opérandes et
 *	repousse son résultat.
 * -------------------------------------------------------------------- */

enum { VK_LITERAL = 0, VK_SCRIPT, VK_VARIABLE };

typedef enum {
    SK_TYPE,			/* un type résolu */
    SK_VALUE,			/* une valeur (littéral ou défaut) */
    SK_NAME,			/* un nom collé à "}" ; obj NULL s'il manque */
    SK_FIELD,			/* un champ {type}nom, avec défaut éventuel */
    SK_COMP,			/* un produit ou une somme en construction */
    SK_CTOR			/* list(...) ou dict(...) en attente de son
				 * type d'éléments (le "." qui suit) */
} ItemKind;

typedef struct TypeItem {
    ItemKind kind;
    TclType *type;		/* SK_TYPE, SK_FIELD */
    Tcl_Obj *obj;		/* SK_VALUE : valeur ; SK_NAME, SK_FIELD : nom */
    Tcl_Obj *defval;		/* SK_FIELD : valeur par défaut, ou NULL */
    Tcl_Obj *defcmd;		/* SK_FIELD : script du défaut, ou NULL */
    int vkind;			/* SK_VALUE : VK_LITERAL, VK_SCRIPT, VK_VARIABLE */
    TclTypeKind comp;		/* SK_COMP : TCLT_PRODUCT ou TCLT_SUM */
    TclTypeField *fields;	/* SK_COMP : éléments accumulés */
    Tcl_Size n, cap;
    Tcl_Size arity;		/* SK_CTOR liste : nombre d'éléments */
    TclDictKey *dkeys;		/* SK_CTOR dict : clés admises */
    Tcl_Size nd;
    TclBinDyn *dyn;		/* SK_CTOR list : nombre lu dans le fichier */
    const char *atExpr;		/* SK_CTOR at : position (partagée) */
} TypeItem;

#define ITEMS(env)	((TypeItem *) (env)->stack)

static TypeItem *
PushItem(TypeEnv *envPtr, ItemKind kind)
{
    TypeItem *it;

    if (envPtr->nStack == envPtr->capStack) {
	envPtr->capStack = envPtr->capStack ? 2 * envPtr->capStack : 16;
	envPtr->stack = Tcl_Realloc(envPtr->stack,
		envPtr->capStack * sizeof(TypeItem));
    }
    it = ITEMS(envPtr) + envPtr->nStack++;
    memset(it, 0, sizeof(*it));
    it->kind = kind;
    return it;
}

/* Dépile : le résultat appartient à l'appelant, qui doit le libérer. */

static TypeItem
PopItem(TypeEnv *envPtr)
{
    return ITEMS(envPtr)[--envPtr->nStack];
}

static void
ReleaseItem(TypeItem *it)
{
    if (it->obj != NULL) {
	Tcl_DecrRefCount(it->obj);
    }
    if (it->defval != NULL) {
	Tcl_DecrRefCount(it->defval);
    }
    if (it->defcmd != NULL) {
	Tcl_DecrRefCount(it->defcmd);
    }
    if (it->fields != NULL) {
	TclTypesFreeFields(it->fields, it->n);
    }
    if (it->dkeys != NULL) {
	TclTypesFreeKeys(it->dkeys, it->nd);
    }
    memset(it, 0, sizeof(*it));
}

static void
ClearStack(TypeEnv *envPtr)
{
    while (envPtr->nStack > 0) {
	TypeItem it = PopItem(envPtr);

	ReleaseItem(&it);
    }
    if (envPtr->stack != NULL) {
	Tcl_Free(envPtr->stack);
	envPtr->stack = NULL;
    }
    envPtr->capStack = 0;
}

static void
SetRef(Tcl_Obj **slot, Tcl_Obj *obj)
{
    *slot = obj;
    if (obj != NULL) {
	Tcl_IncrRefCount(obj);
    }
}

/* Ajoute un élément à un produit ou à une somme en construction. */

static TclTypeField *
NewElement(TypeItem *comp)
{
    if (comp->n == comp->cap) {
	comp->cap = comp->cap ? 2 * comp->cap : 4;
	comp->fields = (TclTypeField *) Tcl_Realloc(comp->fields,
		comp->cap * sizeof(TclTypeField));
    }
    memset(comp->fields + comp->n, 0, sizeof(TclTypeField));
    return comp->fields + comp->n++;
}

static int
AddElement(Tcl_Interp *interp, TypeItem *comp, const TypeItem *item)
{
    TclTypeField *f;

    if (comp->comp == TCLT_PRODUCT) {
	if (item->kind != SK_FIELD) {
	    ERR(interp, "SYNTAX", "a product field must be written "
		    "{type}name");
	    return TCL_ERROR;
	}
	f = NewElement(comp);
	SetRef(&f->name, item->obj);
	f->type = item->type;
	SetRef(&f->defval, item->defval);
	SetRef(&f->defcmd, item->defcmd);
	return TCL_OK;
    }

    /* Somme */

    switch (item->kind) {
    case SK_VALUE:			/* littéral d'énumération */
	f = NewElement(comp);
	SetRef(&f->literal, item->obj);
	return TCL_OK;
    case SK_TYPE:			/* variante sans nom */
	f = NewElement(comp);
	f->type = item->type;
	return TCL_OK;
    case SK_FIELD:			/* variante nommée */
	if (item->defval != NULL || item->defcmd != NULL) {
	    ERR(interp, "SYNTAX", "a sum variant cannot have a default "
		    "value");
	    return TCL_ERROR;
	}
	f = NewElement(comp);
	SetRef(&f->name, item->obj);
	f->type = item->type;
	return TCL_OK;
    default:
	ERR(interp, "SYNTAX", "invalid sum variant");
	return TCL_ERROR;
    }
}

/* --------------------------------------------------------------------
 * Arguments d'une collection : list(2), dict(x, y?, z?=0)
 * -------------------------------------------------------------------- */

static const char *
SkipSpace(const char *p, const char *end)
{
    while (p < end && isspace(UCHAR(*p))) {
	p++;
    }
    return p;
}

/* Un entier naturel. */

static int
ParseNatural(const char *text, Tcl_Size len, Tcl_Size *valuePtr)
{
    char buf[32];
    char *endp;
    long v;

    if (len <= 0 || len >= (Tcl_Size) sizeof(buf)) {
	return TCL_ERROR;
    }
    memcpy(buf, text, len);
    buf[len] = '\0';
    if (!isdigit(UCHAR(buf[0]))) {
	return TCL_ERROR;
    }
    v = strtol(buf, &endp, 10);
    if (*endp != '\0' || v < 0) {
	return TCL_ERROR;
    }
    *valuePtr = (Tcl_Size) v;
    return TCL_OK;
}

/*
 * list(N) : exactement N éléments. list() est une erreur. N est un naturel,
 * ou un paramètre entier (list($i)) dans le corps d'un type
 * paramétré.
 */

static int
ParseListArity(Tcl_Interp *interp, const TypeEnv *envPtr, const char *text,
	Tcl_Size len, Tcl_Size *arityPtr, TclBinDyn **dynPtr)
{
    const char *p = SkipSpace(text, text + len), *end = text + len;
    const char *name = NULL;
    Tcl_Size nameLen = 0;

    while (end > p && isspace(UCHAR(end[-1]))) {
	end--;
    }
    if (p == end) {
	ERR(interp, "ARITY", "list needs a size: write list(2)");
	return TCL_ERROR;
    }
    if (*p == '$') {
	name = p + 1;
	nameLen = end - name;
    }
    if (name != NULL
	    && TclTypeFindParam(envPtr->params, name, nameLen) == NULL) {
	/* $champ, $champ - 1 ... : le nombre est lu dans le fichier */

	Tcl_WideInt n;
	int z;

	return TclTypesParseSize(interp, envPtr->d, "list", p, end - p, 0, NULL,
		&n, &z, dynPtr);
    }
    if (name != NULL) {
	const TypeParam *prm = TclTypeFindParam(envPtr->params, name, nameLen);
	Tcl_Size i;

	for (i = 0; i < nameLen && TclTypeIsNameChar(name[i]); i++) {
	    /* vide */
	}
	if (nameLen == 0 || i != nameLen || prm == NULL) {
	    ERR(interp, "ARITY", "bad list size \"%.*s\": expected a number "
		    "of elements or an int parameter such as $n",
		    (int) (end - p), p);
	    return TCL_ERROR;
	}
	if (prm->isType || prm->type->kind != TCLT_INT) {
	    ERR(interp, "ARITY", "parameter \"%s\" cannot give a list size: "
		    "it is not an int", Tcl_GetString(prm->name));
	    return TCL_ERROR;
	}
	text = Tcl_GetString(prm->value);
	if (ParseNatural(text, (Tcl_Size) strlen(text), arityPtr) != TCL_OK) {
	    ERR(interp, "ARITY", "bad list size %s (from parameter \"%s\"): "
		    "a size cannot be negative", text,
		    Tcl_GetString(prm->name));
	    return TCL_ERROR;
	}
	return TCL_OK;
    }
    if (ParseNatural(p, end - p, arityPtr) != TCL_OK) {
	if (TclTypeFindParam(envPtr->params, p, end - p) != NULL) {
	    ERR(interp, "ARITY", "bad list size \"%.*s\": to use the "
		    "parameter, write list($%.*s)", (int) (end - p), p,
		    (int) (end - p), p);
	    return TCL_ERROR;
	}
	/* un préfixe : list(int(u,16,be)).T */
	{
	    Tcl_WideInt n;
	    int z;

	    return TclTypesParseSize(interp, envPtr->d, "list", p, end - p, 0,
		    NULL, &n, &z, dynPtr);
	}
    }
    return TCL_OK;
}

/* var(typed) : la variable visée doit être typée. */

static int
ParseVarArgs(Tcl_Interp *interp, const char *text, Tcl_Size len,
	Tcl_Size *typedPtr)
{
    const char *p = SkipSpace(text, text + len), *end = text + len;

    while (end > p && isspace(UCHAR(end[-1]))) {
	end--;
    }
    if (end - p == 5 && memcmp(p, "typed", 5) == 0) {
	*typedPtr = 1;
	return TCL_OK;
    }
    ERR(interp, "ARITY", "bad argument \"%.*s\" for var: the only option is "
	    "var(typed)", (int) (end - p), p);
    return TCL_ERROR;
}

static void
FreeKeyArray(TclDictKey *keys, Tcl_Size n, TclDictKey *partial)
{
    if (partial != NULL) {
	if (partial->name != NULL) {
	    Tcl_DecrRefCount(partial->name);
	}
	if (partial->defcmd != NULL) {
	    Tcl_DecrRefCount(partial->defcmd);
	}
	if (partial->defval != NULL) {
	    Tcl_DecrRefCount(partial->defval);
	}
    }
    if (keys != NULL) {
	TclTypesFreeKeys(keys, n);
    }
}

/*
 * Une clé ou une valeur : "entre guillemets", {entre accolades}, $param ou
 * un mot nu. Pour la clé, un mot nu est un nom (lettres, chiffres, _). Pour
 * la valeur, c'est tout jusqu'à la virgule. "$param" (seulement dans le
 * corps d'un type paramétré) vaut la valeur de l'argument.
 */

int
TclTypeParseWord(Tcl_Interp *interp, const TypeEnv *envPtr, const char **pp,
	const char *end, int isValue, Tcl_Obj **objPtr)
{
    const char *p = *pp, *q;
    Tcl_Size scanned;
    Tcl_Obj *msg = NULL;
    const char *code = NULL;

    if (p < end && (*p == '"' || *p == '{')) {
	if (ParseQuotedOrBraced(interp, p, end - p, (*p == '"') ? QUOTED
		: BRACED, objPtr, &scanned, &msg, &code) != TCL_OK) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, msg);
	    } else {
		TclDiscard(msg);
	    }
	    return TCL_ERROR;
	}
	*pp = p + scanned;
	return TCL_OK;
    }
    if (p < end && *p == '$' && envPtr != NULL && envPtr->params != NULL
	    && envPtr->params->n > 0) {
	const TypeParam *prm;

	for (q = p + 1; q < end && TclTypeIsNameChar(*q); q++) {
	    /* vide */
	}
	prm = TclTypeFindParam(envPtr->params, p + 1, q - p - 1);
	if (prm == NULL) {
	    ERR(interp, "PARAM", "unknown parameter \"%.*s\" (write {%.*s} "
		    "for a literal text)", (int) (q - p), p, (int) (q - p), p);
	    return TCL_ERROR;
	}
	if (prm->isType) {
	    ERR(interp, "PARAM", "parameter \"%s\" is a type: it cannot be "
		    "used as a value", Tcl_GetString(prm->name));
	    return TCL_ERROR;
	}
	if (isValue) {
	    const char *r = SkipSpace(q, end);

	    if (r < end && *r != ',') {
		ERR(interp, "PARAM", "unexpected text after \"%.*s\"",
			(int) (q - p), p);
		return TCL_ERROR;
	    }
	}
	*pp = q;
	*objPtr = Tcl_NewStringObj(Tcl_GetString(prm->value),
		TCL_AUTO_LENGTH);
	return TCL_OK;
    }
    q = p;
    if (isValue) {
	while (q < end && *q != ',') {
	    q++;
	}
	*pp = q;
	while (q > p && isspace(UCHAR(q[-1]))) {
	    q--;
	}
    } else {
	while (q < end && TclTypeIsNameChar(*q)) {
	    q++;
	}
	*pp = q;
    }
    *objPtr = Tcl_NewStringObj(p, q - p);
    return TCL_OK;
}

/*
 * dict(x, y, z?, w?=0, "a b") : une liste d'éléments séparés par des
 * virgules. Chaque élément est une clé, puis éventuellement "?" (elle peut
 * manquer), puis éventuellement "=" et une valeur par défaut (qui exige
 * "?"). Un élément vide est la clé vide : dict() n'admet que "".
 */

static int
ParseDictKeys(Tcl_Interp *interp, const TypeEnv *envPtr, const char *text,
	Tcl_Size len, TclDictKey **keysPtr, Tcl_Size *nPtr)
{
    const char *p = text, *end = text + len;
    TclDictKey *keys = NULL, k;
    Tcl_Size n = 0, cap = 0, i;

    while (1) {
	memset(&k, 0, sizeof(k));
	p = SkipSpace(p, end);
	if (TclTypeParseWord(interp, envPtr, &p, end, 0, &k.name) != TCL_OK) {
	    goto fail;
	}
	Tcl_IncrRefCount(k.name);
	p = SkipSpace(p, end);
	if (p < end && *p == '?') {
	    k.optional = 1;
	    p = SkipSpace(p + 1, end);
	}
	if (p < end && *p == '=') {
	    if (!k.optional) {
		ERR(interp, "ARITY", "key \"%s\" has a default value but is "
			"not optional: write %s?=value", Tcl_GetString(k.name),
			Tcl_GetString(k.name));
		goto fail;
	    }
	    p = SkipSpace(p + 1, end);
	    if (p < end && (*p == '[' || IsVariableStart(p, end - p))) {
		/* défaut calculé : [script] ou $nom (cf. VariableDefault) */

		Tcl_Obj *word = NULL, *msg = NULL, *dv = NULL, *dc = NULL;
		const char *code = NULL;
		Tcl_Size used = 0;
		int ok;

		if (*p == '[') {
		    ok = ParseScriptWord(p, end - p, &dc, &used, &msg, &code);
		} else {
		    ok = ParseVariableWord(p, end - p, &word, &used, &msg, &code);
		    if (ok == TCL_OK) {
			Tcl_IncrRefCount(word);
			ok = VariableDefault(interp, envPtr, word, &dv, &dc);
			Tcl_DecrRefCount(word);
		    }
		}
		if (ok != TCL_OK) {
		    if (msg != NULL) {
			if (interp != NULL) {
			    Tcl_SetObjResult(interp, msg);
			} else {
			    TclDiscard(msg);
			}
		    }
		    goto fail;
		}
		p += used;
		if (dv != NULL) {
		    k.defval = dv;
		    Tcl_IncrRefCount(k.defval);
		} else {
		    k.defcmd = dc;
		    Tcl_IncrRefCount(k.defcmd);
		}
	    } else {
		if (TclTypeParseWord(interp, envPtr, &p, end, 1, &k.defval)
			!= TCL_OK) {
		    goto fail;
		}
		Tcl_IncrRefCount(k.defval);
	    }
	    p = SkipSpace(p, end);
	}
	if (p < end && *p != ',') {
	    ERR(interp, "ARITY", "unexpected text \"%.*s\" in the key list of "
		    "dict(...)", (int) (end - p), p);
	    goto fail;
	}
	for (i = 0; i < n; i++) {
	    if (strcmp(Tcl_GetString(keys[i].name),
		    Tcl_GetString(k.name)) == 0) {
		ERR(interp, "ARITY", "duplicate key \"%s\" in dict(...)",
			Tcl_GetString(k.name));
		goto fail;
	    }
	}
	if (n == cap) {
	    cap = cap ? 2 * cap : 4;
	    keys = (TclDictKey *) Tcl_Realloc(keys, cap * sizeof(TclDictKey));
	}
	keys[n++] = k;
	memset(&k, 0, sizeof(k));
	if (p >= end) {
	    break;
	}
	p++;				/* la virgule */
    }
    *keysPtr = keys;
    *nPtr = n;
    return TCL_OK;

  fail:
    FreeKeyArray(keys, n, &k);
    return TCL_ERROR;
}

/*
 * Construit le type d'une collection à partir de son constructeur
 * (list(...) ou dict(...)) et du type de ses éléments. Les valeurs par
 * défaut sont validées ICI, contre le type des valeurs du dictionnaire : un
 * défaut invalide est une erreur de définition. Prend les clés du
 * constructeur.
 */

static int
MakeCtorType(Tcl_Interp *interp, TypeEnv *envPtr, TypeItem *ctor,
	TclType *elem, TclType **tpPtr)
{
    TypeData *d = envPtr->d;
    Tcl_Size i;

    if (ctor->type == d->tAt) {
	*tpPtr = TclTypesMakeAt(d, elem, ctor->atExpr);
	return TCL_OK;
    }
    if (ctor->type == d->tList) {
	if (ctor->dyn != NULL) {
	    *tpPtr = TclTypesMakeListDyn(d, elem, ctor->dyn);
	} else {
	    *tpPtr = TclTypesMakeListN(d, elem, ctor->arity);
	}
	return TCL_OK;
    }
    if (ctor->type == d->tVar) {
	*tpPtr = TclTypesMakeVar(d, elem, ctor->arity == 1);
	return TCL_OK;
    }
    for (i = 0; i < ctor->nd; i++) {
	TclDictKey *k = &ctor->dkeys[i];

	/* Le type en cours de définition n'est pas encore complet. */

	if (k->defval != NULL && elem != envPtr->self
		&& TclTypeValidate(NULL, elem, k->defval, -1, NULL)
		!= TCL_OK) {
	    ERR(interp, "ARITY", "the default value \"%s\" of key \"%s\" is "
		    "not a valid %s", Tcl_GetString(k->defval),
		    Tcl_GetString(k->name), elem->name);
	    return TCL_ERROR;
	}
    }
    *tpPtr = TclTypesMakeDict(d, elem, 1, ctor->dkeys, ctor->nd);
    ctor->dkeys = NULL;			/* possession transmise */
    ctor->nd = 0;
    return TCL_OK;
}

/* list(2) ou dict(x, y) SANS "." : les éléments sont des chaînes. */

static int
ResolveCtor(Tcl_Interp *interp, TypeEnv *envPtr, TypeItem *it)
{
    TclType *tp;

    if (it->kind != SK_CTOR) {
	return TCL_OK;
    }
    if (it->type == envPtr->d->tAt) {
	ERR(interp, "NEST", "at(...) needs the type of the value: write "
		"at(%s).T", it->atExpr);
	return TCL_ERROR;
    }
    if (MakeCtorType(interp, envPtr, it, envPtr->d->tString, &tp) != TCL_OK) {
	return TCL_ERROR;
    }
    ReleaseItem(it);
    it->kind = SK_TYPE;
    it->type = tp;
    return TCL_OK;
}

/*
 * Transforme un élément en type. Un composite est figé (interné, ou versé
 * dans into pour la racine d'un "type def") ; une valeur seule est une
 * énumération à un littéral ; un champ seul est un produit à un champ.
 */

static int
FinishItem(Tcl_Interp *interp, TypeEnv *envPtr, TypeItem *it, TclType *into,
	TclType **tpPtr)
{
    TypeItem comp;

    if (ResolveCtor(interp, envPtr, it) != TCL_OK) {
	return TCL_ERROR;
    }
    switch (it->kind) {
    case SK_TYPE:
	*tpPtr = it->type;
	return TCL_OK;
    case SK_COMP:
	*tpPtr = TclTypesFinish(interp, envPtr->d, envPtr->selfName,
		it->comp, it->fields, it->n, into);
	it->fields = NULL;		/* possession transmise */
	it->n = it->cap = 0;
	return (*tpPtr != NULL) ? TCL_OK : TCL_ERROR;
    case SK_VALUE:
	memset(&comp, 0, sizeof(comp));
	comp.kind = SK_COMP;
	comp.comp = TCLT_SUM;
	break;
    case SK_FIELD:
	memset(&comp, 0, sizeof(comp));
	comp.kind = SK_COMP;
	comp.comp = TCLT_PRODUCT;
	break;
    default:
	ERR(interp, "SYNTAX", "invalid type expression");
	return TCL_ERROR;
    }
    if (AddElement(interp, &comp, it) != TCL_OK) {
	ReleaseItem(&comp);
	return TCL_ERROR;
    }
    *tpPtr = TclTypesFinish(interp, envPtr->d, envPtr->selfName, comp.comp,
	    comp.fields, comp.n, into);
    return (*tpPtr != NULL) ? TCL_OK : TCL_ERROR;
}

/* --------------------------------------------------------------------
 * Les opérations de construction (table builder[])
 * -------------------------------------------------------------------- */

/* "}" : groupement simple {X}, ou champ nommé {X}nom. */

static int
BuildGroup(Tcl_Interp *interp, TypeEnv *envPtr)
{
    TypeItem name = PopItem(envPtr);
    TypeItem *x = ITEMS(envPtr) + envPtr->nStack - 1;
    TclType *tp;
    int rc = TCL_ERROR;

    if (ResolveCtor(interp, envPtr, x) != TCL_OK) {
	goto done;
    }
    if (name.obj == NULL) {
	/* Groupement : l'élément traverse. Un composite est figé, pour
	 * qu'un parent ne le fusionne pas avec ses propres éléments. */

	if (x->kind == SK_COMP) {
	    if (FinishItem(interp, envPtr, x, NULL, &tp) != TCL_OK) {
		goto done;
	    }
	    ReleaseItem(x);
	    x->kind = SK_TYPE;
	    x->type = tp;
	}
	rc = TCL_OK;
	goto done;
    }

    if ((x->kind != SK_TYPE) && (x->kind != SK_COMP)) {
	ERR(interp, "SYNTAX", "\"%s\" can only name a type, not a field or a "
		"value", Tcl_GetString(name.obj));
	goto done;
    }
    if (FinishItem(interp, envPtr, x, NULL, &tp) != TCL_OK) {
	goto done;
    }
    ReleaseItem(x);
    x->kind = SK_FIELD;
    x->type = tp;
    SetRef(&x->obj, name.obj);
    rc = TCL_OK;

  done:
    ReleaseItem(&name);
    return rc;
}

/*
 * Une somme en construction, élément d'un produit : on la fige et elle
 * devient un champ sans nom, de ce type somme.
 */

static int
SumToField(Tcl_Interp *interp, TypeEnv *envPtr, TypeItem *it)
{
    TclType *tp;

    if (it->kind == SK_TYPE && it->type->kind == TCLT_SUM) {
	/* {int}a; {{double}d, {int}i} : le groupe est le même champ. */

	it->kind = SK_FIELD;
	return TCL_OK;
    }
    if (it->kind != SK_COMP || it->comp != TCLT_SUM) {
	return TCL_OK;
    }
    if (FinishItem(interp, envPtr, it, NULL, &tp) != TCL_OK) {
	return TCL_ERROR;
    }
    ReleaseItem(it);
    it->kind = SK_FIELD;
    it->type = tp;
    return TCL_OK;
}

/* "," et ";" : accumulent dans un composite ; le chaînage est à gauche. */

static int
BuildSeparated(Tcl_Interp *interp, TypeEnv *envPtr, TclTypeKind kind)
{
    TypeItem r = PopItem(envPtr);
    TypeItem l = PopItem(envPtr);
    TypeItem comp;
    int rc = TCL_ERROR;

    memset(&comp, 0, sizeof(comp));
    if (ResolveCtor(interp, envPtr, &l) != TCL_OK
	    || ResolveCtor(interp, envPtr, &r) != TCL_OK) {
	goto done;
    }
    /* Dans un produit, une somme (a, b) est un champ sans nom. */

    if (kind == TCLT_PRODUCT) {
	if (SumToField(interp, envPtr, &l) != TCL_OK
		|| SumToField(interp, envPtr, &r) != TCL_OK) {
	    goto done;
	}
    }
    if (l.kind == SK_COMP) {
	if (l.comp != kind) {
	    ERR(interp, "SYNTAX", "cannot use a product as an element of a "
		    "sum without braces");
	    goto done;
	}
	comp = l;			/* on poursuit l'accumulation */
	memset(&l, 0, sizeof(l));
    } else {
	memset(&comp, 0, sizeof(comp));
	comp.kind = SK_COMP;
	comp.comp = kind;
	if (AddElement(interp, &comp, &l) != TCL_OK) {
	    goto done;
	}
    }
    if (AddElement(interp, &comp, &r) != TCL_OK) {
	goto done;
    }
    *PushItem(envPtr, SK_COMP) = comp;
    memset(&comp, 0, sizeof(comp));
    rc = TCL_OK;

  done:
    ReleaseItem(&comp);
    ReleaseItem(&l);
    ReleaseItem(&r);
    return rc;
}

/*
 * "." : list.T, dict.T, list(2).T, dict(x, y).T. Le constructeur est le
 * type list ou dict, nu ou avec ses arguments (SK_CTOR).
 */

static int
BuildNest(Tcl_Interp *interp, TypeEnv *envPtr)
{
    TypeItem r = PopItem(envPtr);
    TypeItem l = PopItem(envPtr);
    TypeData *d = envPtr->d;
    TclType *tp;
    int rc = TCL_ERROR;

    if (ResolveCtor(interp, envPtr, &r) != TCL_OK) {
	goto done;
    }
    if (r.kind != SK_TYPE) {
	ERR(interp, "NEST", "the right operand of \".\" must be a type");
	goto done;
    }
    if (l.kind == SK_CTOR) {
	if (MakeCtorType(interp, envPtr, &l, r.type, &tp) != TCL_OK) {
	    goto done;
	}
    } else if ((l.kind == SK_TYPE) && (l.type == d->tList)) {
	tp = TclTypesMakeListExplicit(d, r.type);
    } else if ((l.kind == SK_TYPE) && (l.type == d->tDict)) {
	tp = TclTypesMakeDict(d, r.type, 0, NULL, 0);
    } else if ((l.kind == SK_TYPE) && (l.type == d->tVar)) {
	tp = TclTypesMakeVar(d, r.type, 0);
    } else {
	ERR(interp, "NEST", "only list, dict and var can be nested "
		"with \".\"");
	goto done;
    }
    PushItem(envPtr, SK_TYPE)->type = tp;
    rc = TCL_OK;

  done:
    ReleaseItem(&l);
    ReleaseItem(&r);
    return rc;
}

/*
 * VariableDefault --
 *
 *	Le jeton $nom d'un défaut. Dans le corps d'un type paramétré, $nom
 *	est d'abord un PARAMÈTRE, résolu à l'instanciation :
 *	  - {int}n, {string}s ... : sa valeur devient le défaut (littéral) ;
 *	  - {cmd}c : sa commande devient le script du défaut, évalué à chaque
 *	    construction ;
 *	  - {type}T : erreur.
 *	Sinon (nom qualifié, ${nom}, a(i), ou type non paramétré) c'est une
 *	variable lue au niveau global à chaque construction, comme [set nom].
 *	Rend un défaut littéral dans *defvalPtr OU un script dans *defcmdPtr
 *	(objets neufs, refcount 0).
 */

static int
VariableDefault(Tcl_Interp *interp, const TypeEnv *envPtr, Tcl_Obj *ref,
	Tcl_Obj **defvalPtr, Tcl_Obj **defcmdPtr)
{
    const char *text = Tcl_GetString(ref);
    Tcl_Parse parse;
    const TypeParam *prm = NULL;
    int simple = 0, generic;
    char *name = NULL;
    Tcl_Obj *words[4];

    *defvalPtr = *defcmdPtr = NULL;
    if (Tcl_ParseVarName(NULL, text, strlen(text), &parse, 0) != TCL_OK
	    || parse.numTokens < 2) {
	Tcl_FreeParse(&parse);
	ERR(interp, "VARIABLE", "invalid variable reference \"%s\"", text);
	return TCL_ERROR;
    }
    if (parse.tokenPtr[0].numComponents == 1 && text[1] != '{') {
	simple = 1;
	name = (char *) Tcl_Alloc(parse.tokenPtr[1].size + 1);
	memcpy(name, parse.tokenPtr[1].start, parse.tokenPtr[1].size);
	name[parse.tokenPtr[1].size] = '\0';
    }
    Tcl_FreeParse(&parse);
    generic = (envPtr->params != NULL && envPtr->params->n > 0);

    if (simple && generic) {
	prm = TclTypeFindParam(envPtr->params, name, (Tcl_Size) strlen(name));
	if (prm == NULL && strstr(name, "::") == NULL) {
	    ERR(interp, "PARAM", "unknown parameter \"$%s\" (to read the global "
		    "variable, write $::%s)", name, name);
	    Tcl_Free(name);
	    return TCL_ERROR;
	}
    }
    if (name != NULL) {
	Tcl_Free(name);
    }
    if (prm != NULL) {
	if (prm->isType) {
	    ERR(interp, "PARAM", "parameter \"%s\" is a type: it cannot be a "
		    "default value", Tcl_GetString(prm->name));
	    return TCL_ERROR;
	}
	if (prm->isCmd) {
	    *defcmdPtr = Tcl_NewStringObj(Tcl_GetString(prm->value),
		    TCL_AUTO_LENGTH);
	} else {
	    *defvalPtr = Tcl_NewStringObj(Tcl_GetString(prm->value),
		    TCL_AUTO_LENGTH);
	}
	return TCL_OK;
    }

    /* Variable : subst -nocommands -nobackslashes {$nom}, qui lit la variable
     * avec toute la syntaxe de Tcl (tableaux, espaces de noms). */

    words[0] = Tcl_NewStringObj("::subst", TCL_AUTO_LENGTH);
    words[1] = Tcl_NewStringObj("-nocommands", TCL_AUTO_LENGTH);
    words[2] = Tcl_NewStringObj("-nobackslashes", TCL_AUTO_LENGTH);
    words[3] = Tcl_NewStringObj(text, TCL_AUTO_LENGTH);
    *defcmdPtr = Tcl_NewListObj(4, words);
    return TCL_OK;
}

/* "=" : valeur par défaut d'un champ, validée contre le type du champ. */

static int
BuildAssign(Tcl_Interp *interp, TypeEnv *envPtr)
{
    TypeItem r = PopItem(envPtr);
    TypeItem *l = ITEMS(envPtr) + envPtr->nStack - 1;
    int rc = TCL_ERROR;

    if ((l->kind != SK_FIELD) || (r.kind != SK_VALUE)) {
	ERR(interp, "ASSIGN", "\"=\" needs a named field and a value");
	goto done;
    }
    if (r.vkind == VK_SCRIPT) {
	/* [script] : calculé à chaque construction, non validé ici */

	SetRef(&l->defcmd, r.obj);
	rc = TCL_OK;
	goto done;
    }
    if (r.vkind == VK_VARIABLE) {
	Tcl_Obj *dv, *dc;

	if (VariableDefault(interp, envPtr, r.obj, &dv, &dc) != TCL_OK) {
	    goto done;
	}
	if (dc != NULL) {
	    SetRef(&l->defcmd, dc);
	    rc = TCL_OK;
	    goto done;
	}
	Tcl_IncrRefCount(dv);
	if (TclTypeValidate(interp, l->type, dv, -1, NULL) != TCL_OK) {
	    Tcl_DecrRefCount(dv);
	    goto done;
	}
	SetRef(&l->defval, dv);
	Tcl_DecrRefCount(dv);
	rc = TCL_OK;
	goto done;
    }
    if (TclTypeValidate(interp, l->type, r.obj, -1, NULL) != TCL_OK) {
	goto done;
    }
    SetRef(&l->defval, r.obj);
    rc = TCL_OK;

  done:
    ReleaseItem(&r);
    return rc;
}

static int
ApplyOperator(Tcl_Interp *interp, TypeEnv *envPtr, unsigned char lexeme)
{
    switch (builder[lexeme]) {
    case TB_GROUP:
	return BuildGroup(interp, envPtr);
    case TB_SUM:
	return BuildSeparated(interp, envPtr, TCLT_SUM);
    case TB_PRODUCT:
	return BuildSeparated(interp, envPtr, TCLT_PRODUCT);
    case TB_NEST:
	return BuildNest(interp, envPtr);
    case TB_ASSIGN:
	return BuildAssign(interp, envPtr);
    default:
	return TCL_OK;		/* START, OPEN_BRACE : comme OPEN_PAREN */
    }
}

/* --------------------------------------------------------------------
 * TclCompileTypeTree --
 *
 *	Parcourt l'arbre SANS RÉCURSION, avec les marques, comme
 *	CompileExprTree. Les feuilles poussent un élément sur la pile ; en
 *	remontant (MARK_PARENT) chaque opérateur applique sa construction.
 *	*typeObjvPtr, *nameObjvPtr et *valueObjvPtr avancent dans les trois
 *	listes, dans l'ordre du texte.
 * -------------------------------------------------------------------- */

static int
TclCompileTypeTree(
    Tcl_Interp *interp,
    OpNode *nodes,
    Tcl_Size index,
    Tcl_Obj *const **typeObjvPtr,
    Tcl_Obj *const **nameObjvPtr,
    Tcl_Obj *const **valueObjvPtr,
    Tcl_Obj *const **argObjvPtr,
    TypeEnv *envPtr)
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
	    /* Retour au parent : on construit. */

	    if (ApplyOperator(interp, envPtr, nodePtr->lexeme) != TCL_OK) {
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
	case OT_EMPTY:
	    PushItem(envPtr, SK_NAME);		/* groupe sans nom */
	    break;
	case OT_TYPE: {
	    Tcl_Obj *obj = **typeObjvPtr;
	    Tcl_Size len;
	    const char *w = Tcl_GetStringFromObj(obj, &len);
	    int dollar = (len > 0 && w[0] == '$');
	    const TypeParam *prm = TclTypeFindParam(envPtr->params, w + dollar,
		    len - dollar);
	    TypeGeneric *g;
	    TclType *tp;

	    (*typeObjvPtr)++;
	    if (dollar) {
		if (prm == NULL) {
		    ERR(interp, "PARAM", "unknown parameter \"%s\"", w);
		    return TCL_ERROR;
		}
		if (!prm->isType) {
		    ERR(interp, "PARAM", "parameter \"%s\" is a value, not a "
			    "type: declare it {type}%s to use it as a type",
			    w + 1, w + 1);
		    return TCL_ERROR;
		}
		tp = prm->typeValue;
	    } else if (prm != NULL) {
		ERR(interp, "PARAM", "\"%s\" is a parameter: write $%s", w, w);
		return TCL_ERROR;
	    } else if ((g = TclTypesFindGeneric(envPtr->d, w, len)) != NULL) {
		tp = TclTypesInstantiate(interp, envPtr->d, g, NULL, 0, envPtr);
	    } else {
		tp = TclTypesResolve(interp, envPtr->d, envPtr->selfName,
			envPtr->self, w, len);
	    }
	    if (tp == NULL) {
		return TCL_ERROR;
	    }
	    PushItem(envPtr, SK_TYPE)->type = tp;
	    break;
	}
	case OT_TYPEARGS: {
	    /* mot(arguments) : list et dict ont une arité ; un type
	     * paramétré reçoit ses arguments. */

	    Tcl_Obj *obj = **typeObjvPtr;
	    Tcl_Obj *aobj = **argObjvPtr;
	    Tcl_Size len, alen;
	    const char *w = Tcl_GetStringFromObj(obj, &len);
	    const char *a = Tcl_GetStringFromObj(aobj, &alen);
	    TypeGeneric *g;
	    TclType *tp;
	    TypeItem *it;

	    (*typeObjvPtr)++;
	    (*argObjvPtr)++;
	    if (w[0] == '$' || TclTypeFindParam(envPtr->params, w, len)
		    != NULL) {
		ERR(interp, "PARAM", "\"%s\" is a parameter: it takes no "
			"arguments", w);
		return TCL_ERROR;
	    }
	    if ((len == 3 && memcmp(w, "int", 3) == 0)
		    || (len == 6 && (memcmp(w, "double", 6) == 0
		    || memcmp(w, "string", 6) == 0))) {
		/* int(u,16,be), double(32,le), string(utf-8,8) : le type de
		 * base avec sa représentation binaire (tclTypeGeneric.c) */

		tp = TclTypesBinaryType(interp, envPtr->d, w, len, a, alen,
			envPtr->params);
		if (tp == NULL) {
		    return TCL_ERROR;
		}
		PushItem(envPtr, SK_TYPE)->type = tp;
		break;
	    }
	    if ((g = TclTypesFindGeneric(envPtr->d, w, len)) != NULL) {
		tp = TclTypesInstantiate(interp, envPtr->d, g, a, alen, envPtr);
		if (tp == NULL) {
		    return TCL_ERROR;
		}
		PushItem(envPtr, SK_TYPE)->type = tp;
		break;
	    }
	    if (len == 2 && w[0] == 'a' && w[1] == 't') {
		/* at(EXPR).T : la valeur est à la position EXPR du fichier */

		Tcl_WideInt n;
		int z;
		TclBinDyn *dyn = NULL;

		if (TclTypesParseSize(interp, envPtr->d, "at", a, alen, 0, NULL,
			&n, &z, &dyn) != TCL_OK) {
		    return TCL_ERROR;
		}
		it = PushItem(envPtr, SK_CTOR);
		it->type = envPtr->d->tAt;
		if (dyn != NULL && dyn->kind == TCLDYN_EXPR) {
		    it->atExpr = dyn->expr;
		} else if (dyn == NULL) {
		    it->atExpr = TclTypesKeepText(envPtr->d, a, alen);
		} else {
		    ERR(interp, "ARITY", "at: a position is a number or an "
			    "expression with $field, not a type");
		    return TCL_ERROR;
		}
		break;
	    }
	    tp = TclTypesResolve(interp, envPtr->d, envPtr->selfName,
		    envPtr->self, w, len);
	    if (tp == NULL) {
		return TCL_ERROR;
	    }
	    if (tp->kind == TCLT_SUM) {
		/* S($champ) : la variante est choisie par ce champ */

		const char *q = a, *qe = a + alen;
		char *field;

		while (q < qe && isspace(UCHAR(*q))) {
		    q++;
		}
		while (qe > q && isspace(UCHAR(qe[-1]))) {
		    qe--;
		}
		if (q >= qe || *q != '$' || qe - q < 2) {
		    ERR(interp, "ARITY", "\"%s\" is a sum: write %s($field) to "
			    "choose its variant by the value of a field", w, w);
		    return TCL_ERROR;
		}
		field = TclTypesKeepText(envPtr->d, q + 1, qe - q - 1);
		tp = TclTypesMakeTagged(interp, envPtr->d, tp, field);
		if (tp == NULL) {
		    return TCL_ERROR;
		}
		PushItem(envPtr, SK_TYPE)->type = tp;
		break;
	    }
	    if ((tp != envPtr->d->tList) && (tp != envPtr->d->tDict)
		    && (tp != envPtr->d->tVar)) {
		ERR(interp, "ARITY", "\"%s\": only list, dict, var and "
			"parameterized types take arguments", w);
		return TCL_ERROR;
	    }
	    it = PushItem(envPtr, SK_CTOR);
	    it->type = tp;
	    if (tp == envPtr->d->tVar) {
		if (ParseVarArgs(interp, a, alen, &it->arity) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else if (tp == envPtr->d->tList) {
		if (ParseListArity(interp, envPtr, a, alen, &it->arity,
			&it->dyn) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else if (ParseDictKeys(interp, envPtr, a, alen, &it->dkeys,
		    &it->nd) != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;
	}
	case OT_NAME: {
	    TypeItem *it = PushItem(envPtr, SK_NAME);

	    SetRef(&it->obj, **nameObjvPtr);
	    (*nameObjvPtr)++;
	    break;
	}
	case OT_VALUE:
	case OT_SCRIPT:
	case OT_VARIABLE: {
	    TypeItem *it = PushItem(envPtr, SK_VALUE);

	    it->vkind = (next == OT_SCRIPT) ? VK_SCRIPT
		    : (next == OT_VARIABLE) ? VK_VARIABLE : VK_LITERAL;
	    SetRef(&it->obj, **valueObjvPtr);
	    (*valueObjvPtr)++;
	    break;
	}
	default:
	    nodePtr = nodes + next;		/* sous-arbre : on descend */
	}
    }
}

/* --------------------------------------------------------------------
 * TclCompileType --
 *
 *	Analyse un type puis le "compile" : le résultat est un descripteur
 *	interné, rendu dans envPtr->result. Si envPtr->into est non NULL, un
 *	produit ou une somme racine y est versé (type nominal de "type def").
 *
 * Résultat : TCL_OK ou TCL_ERROR (message dans interp si non NULL).
 * -------------------------------------------------------------------- */

int
TclCompileType(
    Tcl_Interp *interp,
    const char *script,
    Tcl_Size numBytes,
    TypeEnv *envPtr)
{
    OpNode *opTree = NULL;
    Tcl_Obj *typeList, *nameList, *valueList, *argList;
    int code;

    envPtr->result = NULL;
    envPtr->stack = NULL;
    envPtr->nStack = envPtr->capStack = 0;

    TclNewObj(typeList);
    TclNewObj(nameList);
    TclNewObj(valueList);
    TclNewObj(argList);
    Tcl_IncrRefCount(typeList);
    Tcl_IncrRefCount(nameList);
    Tcl_IncrRefCount(valueList);
    Tcl_IncrRefCount(argList);

    code = ParseType(interp, script, numBytes, &opTree, typeList, nameList,
	    valueList, argList);
    if (code == TCL_OK) {
	Tcl_Size objc;
	Tcl_Obj **typeObjv, **nameObjv, **valueObjv, **argObjv;
	Tcl_Obj *const *tv, *const *nv, *const *vv, *const *av;

	Tcl_ListObjGetElements(NULL, typeList, &objc, &typeObjv);
	Tcl_ListObjGetElements(NULL, nameList, &objc, &nameObjv);
	Tcl_ListObjGetElements(NULL, valueList, &objc, &valueObjv);
	Tcl_ListObjGetElements(NULL, argList, &objc, &argObjv);
	tv = typeObjv;
	nv = nameObjv;
	vv = valueObjv;
	av = argObjv;

	code = TclCompileTypeTree(interp, opTree, 0, &tv, &nv, &vv, &av,
		envPtr);
	if (code == TCL_OK) {
	    /* La racine : il reste exactement un élément. */

	    TypeItem it = PopItem(envPtr);

	    code = FinishItem(interp, envPtr, &it, envPtr->into,
		    &envPtr->result);
	    ReleaseItem(&it);
	}
	Tcl_Free(opTree);
    }
    ClearStack(envPtr);
    Tcl_DecrRefCount(argList);
    Tcl_DecrRefCount(valueList);
    Tcl_DecrRefCount(nameList);
    Tcl_DecrRefCount(typeList);
    return code;
}
