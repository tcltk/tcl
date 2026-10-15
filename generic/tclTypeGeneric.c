/*
 * tclTypeGeneric.c --
 *
 *	Types paramétrés :
 *
 *	    type def matrix({int}i, {int}j = 3) {list(i).list(j).int}
 *	    type def stack({type}T) {list.T}
 *	    type def tree({type}T) {{T}leaf, {list.tree(T)}node}
 *
 *	    set {matrix(2,3)}m ...       ;# instance : un type nominal ordinaire
 *
 *	Un type paramétré (TypeGeneric) ne désigne aucune valeur ; c'est un
 *	modèle. Chaque instanciation (nom, arguments) produit un type nominal
 *	interné, de nom d'affichage "matrix(2,3)", dont le corps est compilé une
 *	seule fois avec les paramètres liés à leurs arguments. L'instance est
 *	enregistrée AVANT la compilation de son corps, ce qui permet la
 *	récursion (tree(T) dans le corps de tree). Si la compilation échoue,
 *	toutes les instances créées depuis le début de l'appel sont retirées.
 */

#include "tclTypesInt.h"
#include <ctype.h>
#include <stdlib.h>

#define UCHAR(c) ((unsigned char) (c))

static char *
DupText(const char *s, Tcl_Size len)
{
    char *r = (char *) Tcl_Alloc(len + 1);

    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}

static const char *
SkipWs(const char *p, const char *end)
{
    while (p < end && isspace(UCHAR(*p))) {
	p++;
    }
    return p;
}

static const char *
TrimEnd(const char *p, const char *end)
{
    while (end > p && isspace(UCHAR(end[-1]))) {
	end--;
    }
    return end;
}

static int
IsIdentifier(const char *s, Tcl_Size len)
{
    Tcl_Size i;

    if (len <= 0 || isdigit(UCHAR(s[0]))) {
	return 0;
    }
    for (i = 0; i < len; i++) {
	if (!TclTypeIsNameChar(s[i])) {
	    return 0;
	}
    }
    return 1;
}

const TypeParam *
TclTypeFindParam(
    const TypeParams *ps,
    const char *name,
    Tcl_Size len)
{
    Tcl_Size i, n;

    if (ps == NULL) {
	return NULL;
    }
    for (i = 0; i < ps->n; i++) {
	const char *pn = Tcl_GetStringFromObj(ps->p[i].name, &n);

	if (n == len && memcmp(pn, name, len) == 0) {
	    return &ps->p[i];
	}
    }
    return NULL;
}

TypeGeneric *
TclTypesFindGeneric(
    TypeData *d,
    const char *w,
    Tcl_Size len)
{
    char *name = DupText(w, len);
    Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&d->generics, name);

    Tcl_Free(name);
    return (hPtr != NULL) ? (TypeGeneric *) Tcl_GetHashValue(hPtr) : NULL;
}

/* --------------------------------------------------------------------
 * Découpage d'une liste d'arguments "a, b, f(x,y), {c,d}" aux virgules du
 * niveau supérieur. Rend des paires (début, longueur) rognées. Une liste
 * vide ou blanche donne 0 argument ; un argument vide au milieu est une
 * erreur de l'appelant (longueur 0).
 * -------------------------------------------------------------------- */

typedef struct ArgSpan {
    const char *start;
    Tcl_Size len;
} ArgSpan;

static ArgSpan *
SplitArgs(const char *text, Tcl_Size len, Tcl_Size *nPtr)
{
    const char *p = text, *end = text + len, *from;
    ArgSpan *spans = NULL;
    Tcl_Size n = 0, cap = 0;
    int braces = 0, parens = 0;

    *nPtr = 0;
    if (SkipWs(text, end) == end) {
	return NULL;
    }
    from = p;
    while (1) {
	int atEnd = (p >= end);

	if (!atEnd) {
	    char c = *p;

	    if (c == '\\') {
		p += 2;
		continue;
	    } else if (c == '"') {
		for (p++; p < end && *p != '"'; p++) {
		    if (*p == '\\') {
			p++;
		    }
		}
		if (p < end) {
		    p++;
		}
		continue;
	    } else if (c == '{') {
		braces++;
	    } else if (c == '}') {
		braces--;
	    } else if (c == '(') {
		parens++;
	    } else if (c == ')') {
		parens--;
	    }
	}
	if (atEnd || (*p == ',' && braces <= 0 && parens <= 0)) {
	    const char *a = SkipWs(from, p), *b = TrimEnd(a, p);

	    if (n == cap) {
		cap = cap ? 2 * cap : 4;
		spans = (ArgSpan *) Tcl_Realloc(spans, cap * sizeof(ArgSpan));
	    }
	    spans[n].start = a;
	    spans[n].len = b - a;
	    n++;
	    if (atEnd) {
		break;
	    }
	    from = p + 1;
	}
	p++;
	if (p > end) {
	    p = end;
	}
    }
    *nPtr = n;
    return spans;
}

/* --------------------------------------------------------------------
 * Définition : type def matrix({int}i, {int}j = 3) {corps}
 * -------------------------------------------------------------------- */

static void
FreeParam(TypeParam *pp)
{
    if (pp->name != NULL) {
	Tcl_DecrRefCount(pp->name);
    }
    if (pp->defText != NULL) {
	Tcl_Free(pp->defText);
    }
}

static void
FreeGeneric(TypeGeneric *g)
{
    Tcl_Size i;

    for (i = 0; i < g->nparams; i++) {
	FreeParam(&g->params[i]);
    }
    Tcl_Free(g->params);
    Tcl_Free(g->body);
    Tcl_Free(g->name);
    Tcl_Free(g);
}

void
TclTypesMarkBits(TypeData *d, const char *name)
{
    Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&d->generics, name);

    if (hPtr != NULL) {
	((TypeGeneric *) Tcl_GetHashValue(hPtr))->bits = 1;
    }
}

void
TclTypesFreeGenerics(TypeData *d)
{
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    Tcl_Size i;

    for (hPtr = Tcl_FirstHashEntry(&d->generics, &search); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&search)) {
	FreeGeneric((TypeGeneric *) Tcl_GetHashValue(hPtr));
    }
    for (i = 0; i < d->nInstLog; i++) {
	Tcl_Free(d->instLog[i]);
    }
    Tcl_Free(d->instLog);
    d->instLog = NULL;
    d->nInstLog = d->capInstLog = 0;
}

/* Le mot "name" apparaît-il dans "body" comme mot entier ? */

static int
WordOccurs(const char *body, Tcl_Size bodyLen, const char *name, Tcl_Size len)
{
    const char *p = body, *end = body + bodyLen;

    while (p + len <= end) {
	if (memcmp(p, name, len) == 0
		&& (p == body || !TclTypeIsNameChar(p[-1]))
		&& (p + len == end || !TclTypeIsNameChar(p[len]))) {
	    return 1;
	}
	p++;
    }
    return 0;
}

/*
 * Un argument de genre {cmd} : le nom d'une commande pleinement qualifié,
 * ::ns::cmd. Pas de blanc ni de caractère de substitution : l'argument est
 * évalué tel quel comme script d'un seul mot.
 */

int
TclTypeIsCmdName(const char *s)
{
    if (s[0] != ':' || s[1] != ':' || s[2] == '\0') {
	return 0;
    }
    for (; *s != '\0'; s++) {
	if (isspace(UCHAR(*s)) || strchr("$[]{}\"\\;", *s)) {
	    return 0;
	}
    }
    return 1;
}

/*
 * Une déclaration de paramètre : "{int}j" ou "{int}j = 3" ou "{type}T".
 * Remplit *pp (name, isType, type, defText) ; rend TCL_ERROR avec message.
 */

static int
ParseParamDecl(
    Tcl_Interp *interp,
    TypeData *d,
    const char *text,
    Tcl_Size len,
    const char *baseName,
    TypeGeneric *g,
    Tcl_Size index,
    TypeParam *pp)
{
    const char *p = text, *end = text + len, *q, *tstart;
    Tcl_Size nameLen, i;
    int depth;
    TypeEnv env;
    char *name;

    memset(pp, 0, sizeof(*pp));
    p = SkipWs(p, end);
    if (p >= end || *p != '{') {
	ERR(interp, "PARAM", "parameter \"%.*s\" of %s needs a kind: write "
		"{int}%.*s or {type}%.*s", (int) (end - p), p, baseName,
		(int) (end - p), p, (int) (end - p), p);
	return TCL_ERROR;
    }
    tstart = ++p;
    for (depth = 1; p < end; p++) {
	if (*p == '\\') {
	    p++;
	} else if (*p == '{') {
	    depth++;
	} else if (*p == '}' && --depth == 0) {
	    break;
	}
    }
    if (p >= end) {
	ERR(interp, "PARAM", "missing close-brace in the parameter list of %s",
		baseName);
	return TCL_ERROR;
    }
    q = p;				/* le "}" */
    p++;
    for (nameLen = 0; p + nameLen < end && TclTypeIsNameChar(p[nameLen]);
	    nameLen++) {
	/* vide */
    }
    if (!IsIdentifier(p, nameLen)) {
	ERR(interp, "PARAM", "a parameter of %s needs a name right after "
		"\"}\", as in {int}n", baseName);
	return TCL_ERROR;
    }
    name = DupText(p, nameLen);
    p += nameLen;

    if (strcmp(name, "type") == 0 || strcmp(name, baseName) == 0
	    || Tcl_FindHashEntry(&d->names, name) != NULL
	    || Tcl_FindHashEntry(&d->generics, name) != NULL) {
	ERR(interp, "PARAM", "parameter name \"%s\" is already a type name",
		name);
	Tcl_Free(name);
	return TCL_ERROR;
    }
    for (i = 0; i < index; i++) {
	if (strcmp(Tcl_GetString(g->params[i].name), name) == 0) {
	    ERR(interp, "PARAM", "duplicate parameter \"%s\" in %s", name,
		    baseName);
	    Tcl_Free(name);
	    return TCL_ERROR;
	}
    }
    pp->name = Tcl_NewStringObj(name, nameLen);
    Tcl_IncrRefCount(pp->name);
    Tcl_Free(name);

    {
	const char *te = TrimEnd(SkipWs(tstart, q), q);

	tstart = SkipWs(tstart, q);
	if (te - tstart == 4 && memcmp(tstart, "type", 4) == 0) {
	    pp->isType = 1;
	} else if (te - tstart == 3 && memcmp(tstart, "cmd", 3) == 0) {
	    pp->isCmd = 1;
	} else {
	    memset(&env, 0, sizeof(env));
	    env.d = d;
	    if (TclCompileType(interp, tstart, te - tstart, &env) != TCL_OK) {
		return TCL_ERROR;
	    }
	    pp->type = env.result;
	}
    }

    p = SkipWs(p, end);
    if (p < end) {
	const char *b;

	if (*p != '=') {
	    ERR(interp, "PARAM", "unexpected text \"%.*s\" after parameter "
		    "\"%s\"", (int) (end - p), p, Tcl_GetString(pp->name));
	    return TCL_ERROR;
	}
	p = SkipWs(p + 1, end);
	b = TrimEnd(p, end);
	if (b == p) {
	    ERR(interp, "PARAM", "parameter \"%s\" has \"=\" but no default "
		    "value", Tcl_GetString(pp->name));
	    return TCL_ERROR;
	}
	pp->defText = DupText(p, b - p);
	if (pp->isCmd && memchr(p, '$', b - p) == NULL) {
	    char *txt = DupText(p, b - p);
	    int ok = TclTypeIsCmdName(txt);

	    Tcl_Free(txt);
	    if (!ok) {
		ERR(interp, "PARAM", "the default \"%.*s\" of parameter \"%s\" "
			"is not a fully qualified command name (::ns::cmd)",
			(int) (b - p), p, Tcl_GetString(pp->name));
		return TCL_ERROR;
	    }
	} else if (!pp->isType && !pp->isCmd && memchr(p, '$', b - p) == NULL) {
	    /* Défaut littéral : on le valide tout de suite. */

	    const char *qp = p;
	    Tcl_Obj *val = NULL;
	    int bad;

	    if (TclTypeParseWord(interp, NULL, &qp, b, 1, &val) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_IncrRefCount(val);
	    bad = (SkipWs(qp, b) != b)
		    || TclTypeValidate(NULL, pp->type, val, -1, NULL) != TCL_OK;
	    if (bad) {
		ERR(interp, "PARAM", "the default \"%.*s\" of parameter \"%s\" "
			"is not a valid %s", (int) (b - p), p,
			Tcl_GetString(pp->name), pp->type->name);
	    }
	    Tcl_DecrRefCount(val);
	    if (bad) {
		return TCL_ERROR;
	    }
	}
    } else if (index > 0 && g->params[index - 1].defText != NULL) {
	ERR(interp, "PARAM", "parameter \"%s\" has no default but follows a "
		"parameter that has one", Tcl_GetString(pp->name));
	return TCL_ERROR;
    }
    return TCL_OK;
}

int
TclTypesDefineGeneric(
    Tcl_Interp *interp,
    TypeData *d,
    const char *spec,
    Tcl_Size specLen,
    const char *body,
    Tcl_Size bodyLen)
{
    const char *open = (const char *) memchr(spec, '(', specLen);
    const char *end = spec + specLen, *close, *p;
    char *baseName;
    TypeGeneric *g;
    ArgSpan *spans;
    Tcl_Size n, i;
    int isNew;

    baseName = DupText(spec, open - spec);
    if (TclTypesCheckNewName(interp, d, baseName) != TCL_OK) {
	Tcl_Free(baseName);
	return TCL_ERROR;
    }
    close = TclTypeFindArgsEnd(open + 1, end);
    if (close == NULL || SkipWs(close + 1, end) != end) {
	ERR(interp, "PARAM", "bad parameter list in \"%s\": expected "
		"name(params) with nothing after \")\"", spec);
	Tcl_Free(baseName);
	return TCL_ERROR;
    }
    spans = SplitArgs(open + 1, close - open - 1, &n);
    if (n == 0) {
	ERR(interp, "PARAM", "type \"%s\" has an empty parameter list: "
		"define it without parentheses", baseName);
	Tcl_Free(baseName);
	return TCL_ERROR;
    }
    g = (TypeGeneric *) Tcl_Alloc(sizeof(TypeGeneric));
    memset(g, 0, sizeof(*g));
    g->name = baseName;
    g->params = (TypeParam *) Tcl_Alloc(n * sizeof(TypeParam));
    memset(g->params, 0, n * sizeof(TypeParam));
    g->body = DupText(body, bodyLen);
    g->bodyLen = bodyLen;

    for (i = 0; i < n; i++) {
	if (ParseParamDecl(interp, d, spans[i].start, spans[i].len, baseName,
		g, i, &g->params[i]) != TCL_OK) {
	    FreeParam(&g->params[i]);
	    goto fail;
	}
	g->nparams = i + 1;
	p = Tcl_GetString(g->params[i].name);
	if (!WordOccurs(body, bodyLen, p, (Tcl_Size) strlen(p))) {
	    ERR(interp, "PARAM", "parameter \"%s\" is never used in the "
		    "definition of %s", p, baseName);
	    goto fail;
	}
    }
    Tcl_Free(spans);
    Tcl_SetHashValue(Tcl_CreateHashEntry(&d->generics, baseName, &isNew), g);
    return TCL_OK;

  fail:
    Tcl_Free(spans);
    FreeGeneric(g);
    return TCL_ERROR;
}

/* --------------------------------------------------------------------
 * Instanciation
 * -------------------------------------------------------------------- */

/* Valeur de paramètre à afficher dans un nom d'instance. */

static void
AppendDisplay(Tcl_DString *ds, const char *s)
{
    const char *q;
    int plain = (*s != '\0');

    for (q = s; *q != '\0'; q++) {
	if (!isalnum(UCHAR(*q)) && !strchr("_.+-", *q) && !(*q & 0x80)) {
	    plain = 0;
	    break;
	}
    }
    if (plain) {
	Tcl_DStringAppend(ds, s, TCL_AUTO_LENGTH);
	return;
    }
    Tcl_DStringAppend(ds, "{", 1);
    for (q = s; *q != '\0'; q++) {
	if (*q == '{' || *q == '}' || *q == '\\') {
	    Tcl_DStringAppend(ds, "\\", 1);
	}
	Tcl_DStringAppend(ds, q, 1);
    }
    Tcl_DStringAppend(ds, "}", 1);
}

/*
 * Évalue l'argument "text" du paramètre "decl" dans l'environnement evalEnv
 * (celui de l'appelant pour un argument fourni, celui des paramètres déjà
 * liés pour un défaut). Rend la valeur canonique (refcount 0) et, pour un
 * paramètre de type, le type.
 */

static int
EvalArg(
    Tcl_Interp *interp,
    TypeData *d,
    const TypeGeneric *g,
    const TypeParam *decl,
    const char *text,
    Tcl_Size len,
    const TypeEnv *evalEnv,
    Tcl_Obj **valuePtr,
    TclType **typePtr)
{
    const char *pname = Tcl_GetString(decl->name);

    *typePtr = NULL;
    if (len == 0) {
	ERR(interp, "PARAM", "empty argument for parameter \"%s\" of %s",
		pname, g->name);
	return TCL_ERROR;
    }
    if (decl->isType) {
	TypeEnv sub;

	memset(&sub, 0, sizeof(sub));
	sub.d = d;
	sub.params = evalEnv->params;
	sub.selfName = evalEnv->selfName;
	sub.self = evalEnv->self;
	if (TclCompileType(interp, text, len, &sub) != TCL_OK) {
	    return TCL_ERROR;
	}
	*typePtr = sub.result;
	*valuePtr = Tcl_NewStringObj(sub.result->name, TCL_AUTO_LENGTH);
	return TCL_OK;
    } else if (decl->isCmd) {
	const char *p = text, *end = text + len;
	Tcl_Obj *obj = NULL;

	/* Une commande : ::ns::cmd, ou $nom pour un paramètre {cmd} de l'appelant */

	if (TclTypeParseWord(interp, evalEnv, &p, end, 1, &obj) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (SkipWs(p, end) != end || !TclTypeIsCmdName(Tcl_GetString(obj))) {
	    ERR(interp, "PARAM", "bad value \"%.*s\" for parameter \"%s\" of %s: "
		    "expected a fully qualified command name (::ns::cmd)",
		    (int) len, text, pname, g->name);
	    TclDiscard(obj);
	    return TCL_ERROR;
	}
	*valuePtr = obj;
	return TCL_OK;
    } else {
	const char *p = text, *end = text + len;
	Tcl_Obj *obj = NULL, *shown;

	/* Un paramètre de l'appelant se désigne par $nom, comme en Tcl. */

	if (TclTypeParseWord(interp, evalEnv, &p, end, 1, &obj) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (SkipWs(p, end) != end) {
	    TclDiscard(obj);
	    ERR(interp, "PARAM", "unexpected text \"%.*s\" in the argument of "
		    "parameter \"%s\"", (int) (end - p), p, pname);
	    return TCL_ERROR;
	}
	Tcl_IncrRefCount(obj);
	if (TclTypeValidate(NULL, decl->type, obj, -1, NULL) != TCL_OK) {
	    ERR(interp, "PARAM", "bad value \"%s\" for parameter \"%s\" of %s: "
		    "expected %s%s", Tcl_GetString(obj), pname, g->name,
		    decl->type->name,
		    TclTypeFindParam(evalEnv->params, text, len) != NULL
		    ? " (to pass a parameter, write $name)" : "");
	    Tcl_DecrRefCount(obj);
	    return TCL_ERROR;
	}
	if (decl->type->kind == TCLT_INT) {
	    char *e;
	    long long v = strtoll(Tcl_GetString(obj), &e, 10);

	    if (*e == '\0' && Tcl_GetString(obj)[0] != '\0') {
		shown = Tcl_ObjPrintf("%lld", v);
	    } else {
		shown = Tcl_NewStringObj(Tcl_GetString(obj), TCL_AUTO_LENGTH);
	    }
	} else {
	    shown = Tcl_NewStringObj(Tcl_GetString(obj), TCL_AUTO_LENGTH);
	}
	Tcl_DecrRefCount(obj);
	*valuePtr = shown;
	return TCL_OK;
    }
}

static void
LogInstance(TypeData *d, const char *key)
{
    if (d->nInstLog == d->capInstLog) {
	d->capInstLog = d->capInstLog ? 2 * d->capInstLog : 8;
	d->instLog = (char **) Tcl_Realloc(d->instLog,
		d->capInstLog * sizeof(char *));
    }
    d->instLog[d->nInstLog++] = DupText(key, (Tcl_Size) strlen(key));
}

/* "sous" apparaît-il dans "s" comme mot, c'est-à-dire sans caractère de
 * nom devant ? */

static int
MentionsName(const char *s, const char *sub, Tcl_Size len)
{
    const char *p = s;

    while ((p = strstr(p, sub)) != NULL) {
	if (p == s || !TclTypeIsNameChar(p[-1])) {
	    return 1;
	}
	p++;
	(void) len;
    }
    return 0;
}

/*
 * Retire du cache toutes les instances enregistrées depuis le rang "from",
 * ainsi que les types anonymes internés qui les mentionnent : sans cela une
 * nouvelle instance de même nom retomberait sur un list<matrix(3,3)> bâti sur
 * l'ancienne, abandonnée.
 */

static void
Rollback(TypeData *d, Tcl_Size from)
{
    Tcl_Size i, nk = 0, capk = 0;
    char **doomed = NULL;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;

    for (i = d->nInstLog - 1; i >= from; i--) {
	hPtr = Tcl_FindHashEntry(&d->instances, d->instLog[i]);
	if (hPtr != NULL) {
	    TclType *dead = (TclType *) Tcl_GetHashValue(hPtr);
	    Tcl_Size nlen = (Tcl_Size) strlen(dead->name);

	    for (hPtr = Tcl_FirstHashEntry(&d->canon, &search); hPtr != NULL;
		    hPtr = Tcl_NextHashEntry(&search)) {
		const char *k = (const char *) Tcl_GetHashKey(&d->canon, hPtr);

		if (MentionsName(k, dead->name, nlen)) {
		    if (nk == capk) {
			capk = capk ? 2 * capk : 8;
			doomed = (char **) Tcl_Realloc(doomed,
				capk * sizeof(char *));
		    }
		    doomed[nk++] = DupText(k, (Tcl_Size) strlen(k));
		}
	    }
	    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&d->instances,
		    d->instLog[i]));
	}
	Tcl_Free(d->instLog[i]);
    }
    d->nInstLog = from;
    for (i = 0; i < nk; i++) {
	hPtr = Tcl_FindHashEntry(&d->canon, doomed[i]);
	if (hPtr != NULL) {
	    Tcl_DeleteHashEntry(hPtr);
	}
	Tcl_Free(doomed[i]);
    }
    Tcl_Free(doomed);
}

static void
ForgetLog(TypeData *d)
{
    Tcl_Size i;

    for (i = 0; i < d->nInstLog; i++) {
	Tcl_Free(d->instLog[i]);
    }
    d->nInstLog = 0;
}

static void
FreeBound(TypeParams *bound, Tcl_Size n)
{
    Tcl_Size i;

    for (i = 0; i < n; i++) {
	if (bound->p[i].value != NULL) {
	    Tcl_DecrRefCount(bound->p[i].value);
	}
    }
    Tcl_Free(bound->p);
}

TclType *
TclTypesInstantiate(
    Tcl_Interp *interp,
    TypeData *d,
    TypeGeneric *g,
    const char *argText,
    Tcl_Size argLen,
    const TypeEnv *parent)
{
    ArgSpan *spans = NULL;
    Tcl_Size nargs = 0, i, logStart = d->nInstLog;
    TypeParams bound;
    TypeEnv none, defEnv;
    const TypeEnv *callerEnv = parent;
    Tcl_DString key, display;
    Tcl_HashEntry *hPtr;
    TclType *inst = NULL, *res = NULL;
    const TclType *stack[64];
    int isNew, depthUp = 0;
    TypeEnv env;
    const char *msg;

    memset(&none, 0, sizeof(none));
    none.d = d;
    if (callerEnv == NULL) {
	callerEnv = &none;
    }
    if (argText != NULL) {
	spans = SplitArgs(argText, argLen, &nargs);
    }
    if (nargs > g->nparams) {
	ERR(interp, "PARAM", "too many arguments for %s: expected %s%d, got "
		"%d", g->name, (g->params[g->nparams - 1].defText != NULL
		|| g->params[0].defText != NULL) ? "at most " : "",
		(int) g->nparams, (int) nargs);
	Tcl_Free(spans);
	return NULL;
    }
    if (nargs == 0 && g->params[0].defText == NULL) {
	ERR(interp, "PARAM", "type \"%s\" takes parameters: write %s(%s...)",
		g->name, g->name, Tcl_GetString(g->params[0].name));
	Tcl_Free(spans);
	return NULL;
    }

    bound.p = (TypeParam *) Tcl_Alloc(g->nparams * sizeof(TypeParam));
    memset(bound.p, 0, g->nparams * sizeof(TypeParam));
    bound.n = 0;
    Tcl_DStringInit(&key);
    Tcl_DStringInit(&display);
    Tcl_DStringAppend(&key, g->name, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&display, g->name, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&display, "(", 1);

    for (i = 0; i < g->nparams; i++) {
	const TypeParam *decl = &g->params[i];
	const char *text;
	Tcl_Size len;
	const TypeEnv *ev;

	bound.p[i] = *decl;
	bound.p[i].value = NULL;
	bound.p[i].typeValue = NULL;
	if (i < nargs) {
	    text = spans[i].start;
	    len = spans[i].len;
	    ev = callerEnv;
	} else if (decl->defText != NULL) {
	    text = decl->defText;
	    len = (Tcl_Size) strlen(text);
	    memset(&defEnv, 0, sizeof(defEnv));
	    defEnv.d = d;
	    defEnv.params = &bound;
	    ev = &defEnv;
	} else {
	    ERR(interp, "PARAM", "missing argument for parameter \"%s\" of %s",
		    Tcl_GetString(decl->name), g->name);
	    goto fail;
	}
	if (EvalArg(interp, d, g, decl, text, len, ev, &bound.p[i].value,
		&bound.p[i].typeValue) != TCL_OK) {
	    bound.p[i].value = NULL;
	    goto fail;
	}
	Tcl_IncrRefCount(bound.p[i].value);
	bound.n = i + 1;
	Tcl_DStringAppend(&key, "\1", 1);
	Tcl_DStringAppend(&key, Tcl_GetString(bound.p[i].value),
		TCL_AUTO_LENGTH);
	if (i > 0) {
	    Tcl_DStringAppend(&display, ",", 1);
	}
	if (decl->isType || decl->isCmd) {
	    /* le nom canonique d'un type, d'une commande, est non ambigu */
	    Tcl_DStringAppend(&display, Tcl_GetString(bound.p[i].value),
		    TCL_AUTO_LENGTH);
	} else {
	    AppendDisplay(&display, Tcl_GetString(bound.p[i].value));
	}
    }
    Tcl_DStringAppend(&display, ")", 1);
    Tcl_Free(spans);
    spans = NULL;

    hPtr = Tcl_CreateHashEntry(&d->instances, Tcl_DStringValue(&key), &isNew);
    if (!isNew) {
	inst = (TclType *) Tcl_GetHashValue(hPtr);
	goto done;
    }

    /* Nouvelle instance : enregistrée avant son corps (récursion). */

    inst = TclTypesNewType(d, TCLT_STRING, Tcl_DStringValue(&display));
    inst->nominal = 1;
    Tcl_SetHashValue(hPtr, inst);
    LogInstance(d, Tcl_DStringValue(&key));
    d->instDepth++;
    depthUp = 1;

    memset(&env, 0, sizeof(env));
    env.d = d;
    env.params = &bound;
    env.selfName = inst->name;
    env.self = inst;
    env.into = inst;
    if (TclCompileType(interp, g->body, g->bodyLen, &env) != TCL_OK) {
	goto fail;
    }
    res = env.result;
    if (res == inst && inst->kind == TCLT_STRING && inst->nfields == 0) {
	ERR(interp, "SYNTAX", "type \"%s\" cannot be defined as itself",
		inst->name);
	goto fail;
    }
    if (res != inst) {
	TclTypesCopyInto(inst, res);
    }
    if (g->bits) {
	/* bitarray(n, ordre) : largeur et sens des indices */

	Tcl_WideInt w = 0;
	const char *ord = Tcl_GetString(bound.p[1].value);

	if (Tcl_GetWideIntFromObj(NULL, bound.p[0].value, &w) != TCL_OK
		|| w < 1 || w > 1000000) {
	    ERR(interp, "PARAM", "bitarray needs a width between 1 and "
		    "1000000, not \"%s\"", Tcl_GetString(bound.p[0].value));
	    goto fail;
	}
	if (strcmp(ord, "lsb0") != 0 && strcmp(ord, "msb0") != 0) {
	    ERR(interp, "PARAM", "bitarray order must be lsb0 (index 0 is the "
		    "rightmost bit) or msb0 (index 0 is the leftmost bit), "
		    "not \"%s\"", ord);
	    goto fail;
	}
	inst->kind = TCLT_INT;
	inst->bitWidth = (Tcl_Size) w;
	inst->bitMsb0 = (strcmp(ord, "msb0") == 0);
    }
    if (!TclTypesInhabited(inst, stack, 0)) {
	ERR(interp, "RECURSIVE", "type \"%s\" has no finite value: every "
		"recursive reference must go through a sum variant or a "
		"collection", inst->name);
	goto fail;
    }

  done:
    if (depthUp && --d->instDepth == 0) {
	ForgetLog(d);			/* succès de l'appel le plus externe */
    }
    FreeBound(&bound, g->nparams);
    Tcl_DStringFree(&key);
    Tcl_DStringFree(&display);
    return inst;

  fail:
    if (interp != NULL && depthUp) {
	char *copy;

	msg = Tcl_GetString(Tcl_GetObjResult(interp));
	copy = DupText(msg, (Tcl_Size) strlen(msg));
	ERR(interp, "INSTANCE", "in the instance %s: %s",
		Tcl_DStringValue(&display), copy);
	Tcl_Free(copy);
    }
    Tcl_Free(spans);
    if (depthUp) {
	d->instDepth--;
	Rollback(d, logStart);
    }
    FreeBound(&bound, g->nparams);
    Tcl_DStringFree(&key);
    Tcl_DStringFree(&display);
    return NULL;
}
