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
TclTypesMarkBits(TypeData *d, const char *name, int code)
{
    Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&d->generics, name);

    if (hPtr != NULL) {
	((TypeGeneric *) Tcl_GetHashValue(hPtr))->bits = code;
    }
}

/*
 * Boutisme d'un argument "be" ou "le" ; TCLBIN_AUTO si le mot n'en est pas
 * un. Rend -1 pour un autre mot.
 */

static int
EndianWord(const char *s, Tcl_Size len)
{
    if (len == 2 && s[0] == 'b' && s[1] == 'e') {
	return TCLBIN_BE;
    } else if (len == 2 && s[0] == 'l' && s[1] == 'e') {
	return TCLBIN_LE;
    }
    return -1;
}

/* Entier décimal strict d'un argument ; 0 si le texte n'en est pas un. */

static int
SpanWide(const ArgSpan *sp, Tcl_WideInt *wPtr)
{
    char buf[32], *e;

    if (sp->len <= 0 || sp->len >= (Tcl_Size) sizeof(buf)) {
	return 0;
    }
    memcpy(buf, sp->start, sp->len);
    buf[sp->len] = '\0';
    *wPtr = strtoll(buf, &e, 10);
    return *e == '\0';
}

/* Décorations binaires : allouées une fois, partagées par les types qui les
 * portent, libérées avec les données du module (jamais individuellement). */

void *
TclTypesKeep(TypeData *d, void *blob)
{
    if (d->nBlobs == d->capBlobs) {
	d->capBlobs = d->capBlobs ? 2 * d->capBlobs : 16;
	d->blobs = (void **) Tcl_Realloc(d->blobs, d->capBlobs * sizeof(void *));
    }
    d->blobs[d->nBlobs++] = blob;
    return blob;
}

char *
TclTypesKeepText(TypeData *d, const char *s, Tcl_Size len)
{
    return (char *) TclTypesKeep(d, DupText(s, len));
}

/* "$nom" ou "${nom}" : un nom de champ à la position p (après le $). Rend la
 * longueur du nom, 0 si ce n'en est pas un. */

static Tcl_Size
FieldRefLen(const char *p, const char *end)
{
    const char *q = p;

    while (q < end && TclTypeIsNameChar(*q)) {
	q++;
    }
    return q - p;
}

/*
 * TclTypesParseSize --
 *
 *	L'argument qui donne la taille d'une chaîne, le nombre d'éléments d'une
 *	liste ou la position d'une valeur :
 *	    N                 un naturel : *nPtr
 *	    z                 (allowZ) terminée par un NUL : *zPtr
 *	    $n                un paramètre entier du type en cours de
 *	                      définition : sa valeur, un naturel
 *	    int(u,16,be)      un PRÉFIXE : le type entier non signé lu juste
 *	                      avant : *dynPtr
 *	    $len - 4          une EXPRESSION Tcl sur les champs déjà lus : *dynPtr
 */

int
TclTypesParseSize(
    Tcl_Interp *interp,
    TypeData *d,
    const char *what,
    const char *text,
    Tcl_Size len,
    int allowZ,
    const TypeParams *params,
    Tcl_WideInt *nPtr,
    int *zPtr,
    TclBinDyn **dynPtr)
{
    const char *p = SkipWs(text, text + len), *end = TrimEnd(p, text + len);
    char buf[40];
    Tcl_Size n = end - p, i;
    Tcl_WideInt w;
    TclBinDyn *dyn;
    int hasDollar = 0;

    *nPtr = -1;
    *zPtr = 0;
    *dynPtr = NULL;
    if (n == 0) {
	ERR(interp, "PARAM", "%s: empty size", what);
	return TCL_ERROR;
    }
    if (n < (Tcl_Size) sizeof(buf)) {
	char *e;

	memcpy(buf, p, n);
	buf[n] = '\0';
	w = strtoll(buf, &e, 10);
	if (*e == '\0' && isdigit(UCHAR(buf[0]))) {
	    *nPtr = w;
	    return TCL_OK;
	}
    }
    if (allowZ && n == 1 && *p == 'z') {
	*zPtr = 1;
	return TCL_OK;
    }
    for (i = 0; i < n; i++) {
	if (p[i] == '$') {
	    hasDollar = 1;
	}
    }
    if (hasDollar) {
	/* $param seul : la valeur du paramètre entier du type en cours */

	if (p[0] == '$' && FieldRefLen(p + 1, end) == n - 1 && params != NULL) {
	    const TypeParam *prm = TclTypeFindParam(params, p + 1, n - 1);

	    if (prm != NULL) {
		const char *v;

		if (prm->isType || prm->type->kind != TCLT_INT) {
		    ERR(interp, "PARAM", "%s: parameter \"%s\" is not an int",
			    what, Tcl_GetString(prm->name));
		    return TCL_ERROR;
		}
		v = Tcl_GetString(prm->value);
		w = strtoll(v, NULL, 10);
		if (w < 0) {
		    ERR(interp, "PARAM", "%s: a size cannot be negative", what);
		    return TCL_ERROR;
		}
		*nPtr = w;
		return TCL_OK;
	    }
	}
	for (i = 0; i < n; i++) {
	    if (p[i] == '$' && (i + 1 >= n || (FieldRefLen(p + i + 1, end) == 0
		    && p[i + 1] != '{'))) {
		ERR(interp, "PARAM", "%s: \"$\" must be followed by a field "
			"name in \"%.*s\"", what, (int) n, p);
		return TCL_ERROR;
	    }
	}
	dyn = (TclBinDyn *) TclTypesKeep(d, Tcl_Alloc(sizeof(TclBinDyn)));
	memset(dyn, 0, sizeof(*dyn));
	dyn->kind = TCLDYN_EXPR;
	dyn->expr = TclTypesKeepText(d, p, n);
	*dynPtr = dyn;
	return TCL_OK;
    }
    /* sinon : un type entier non signé (préfixe) */
    {
	Tcl_Obj *saved = (interp != NULL) ? Tcl_GetObjResult(interp) : NULL;
	TclType *pre;

	if (saved != NULL) {
	    Tcl_IncrRefCount(saved);
	}
	pre = TclTypeParse(interp, p, n);
	if (pre == NULL) {
	    if (interp != NULL) {
		const char *m = Tcl_GetString(Tcl_GetObjResult(interp));

		ERR(interp, "PARAM", "%s: bad size \"%.*s\": expected a number, "
			"%sa prefix type such as int(u,16,be) or an expression "
			"with $field (%s)", what, (int) n, p, allowZ ? "z, " : "",
			m);
		Tcl_DecrRefCount(saved);
	    }
	    return TCL_ERROR;
	}
	if (saved != NULL) {
	    Tcl_SetObjResult(interp, saved);
	    Tcl_DecrRefCount(saved);
	}
	if (pre->binKind != TCLBIN_UINT) {
	    ERR(interp, "PARAM", "%s: a prefix must be an unsigned integer "
		    "type, not %s", what, pre->name);
	    return TCL_ERROR;
	}
	dyn = (TclBinDyn *) TclTypesKeep(d, Tcl_Alloc(sizeof(TclBinDyn)));
	memset(dyn, 0, sizeof(*dyn));
	dyn->kind = TCLDYN_PREFIX;
	dyn->pre = pre;
	*dynPtr = dyn;
    }
    return TCL_OK;
}

/* Encodages des chaînes binaires : mot-clé, nom d'encodage Tcl (NULL pour
 * des octets bruts), nombre d'octets du terminateur NUL de string(enc,z). */

static const struct {
    const char *word, *tclName;
    int nul;
} encodings[] = {
    {"raw", NULL, 1},
    {"ascii", "ascii", 1},
    {"latin1", "iso8859-1", 1},
    {"utf-8", "utf-8", 1},
    {"utf-16le", "utf-16le", 2},
    {"utf-16be", "utf-16be", 2},
    {"ucs-2le", "ucs-2le", 2},
    {"ucs-2be", "ucs-2be", 2}
};
#define NENCODINGS ((int) (sizeof(encodings) / sizeof(encodings[0])))

/*
 * TclTypesBinaryType --
 *
 *	int(u|s, n ?, be|le?)    entier de n bits (multiple de 8, de 8 à 1024)
 *	double(32|64 ?, be|le?)  flottant IEEE
 *	string(enc, N)           N octets, enc : raw ascii latin1 utf-8
 *	                         utf-16le utf-16be ucs-2le ucs-2be
 *	string(enc, z)           terminé par un NUL (2 octets pour utf-16, ucs-2)
 *
 *	L'instance est un type nominal interné par son nom d'affichage ; elle
 *	a les mêmes valeurs qu'int, double ou string, dans la plage de sa
 *	représentation (voir TclTypeAssignable pour l'élargissement).
 */

TclType *
TclTypesBinaryType(
    Tcl_Interp *interp,
    TypeData *d,
    const char *base,
    Tcl_Size baseLen,
    const char *argText,
    Tcl_Size argLen,
    const TypeParams *params)
{
    ArgSpan *sp;
    Tcl_Size n = 0, i;
    Tcl_DString key;
    Tcl_HashEntry *hPtr;
    TclType *inst = NULL, *under;
    int isNew, kind, endian = TCLBIN_AUTO, signd = 0, enc = 0, cstr = 0;
    TclBinDyn *dyn = NULL;
    const char *dynText = NULL;
    Tcl_WideInt w = 0;
    int isInt = (baseLen == 3), isDouble = (baseLen == 6 && base[0] == 'd');

    sp = SplitArgs(argText, argLen, &n);
    for (i = 0; i < n; i++) {
	if (sp[i].len == 0) {
	    ERR(interp, "PARAM", "empty argument in %.*s(...)", (int) baseLen,
		    base);
	    goto fail;
	}
    }
    under = isInt ? d->tInt : isDouble ? d->tDouble : d->tString;
    kind = isInt ? TCLT_INT : isDouble ? TCLT_DOUBLE : TCLT_STRING;

    if (isInt) {
	/* int(u|s, n ?, be|le?) */

	if (n < 2 || n > 3) {
	    ERR(interp, "PARAM", "int takes (u|s, bits ?, be|le?), for "
		    "example int(u,16,be)");
	    goto fail;
	}
	if (sp[0].len == 1 && (sp[0].start[0] == 'u'
		|| sp[0].start[0] == 's')) {
	    signd = (sp[0].start[0] == 's');
	} else {
	    ERR(interp, "PARAM", "int: the first argument must be u "
		    "(unsigned) or s (signed), not \"%.*s\"", (int) sp[0].len,
		    sp[0].start);
	    goto fail;
	}
	if (!SpanWide(&sp[1], &w) || w < 8 || w > 1024 || w % 8) {
	    ERR(interp, "PARAM", "int: the width in bits must be a multiple "
		    "of 8 between 8 and 1024, not \"%.*s\"", (int) sp[1].len,
		    sp[1].start);
	    goto fail;
	}
	if (n == 3 && (endian = EndianWord(sp[2].start, sp[2].len)) < 0) {
	    ERR(interp, "PARAM", "int: the byte order must be be or le, not "
		    "\"%.*s\"", (int) sp[2].len, sp[2].start);
	    goto fail;
	}
    } else if (isDouble) {
	/* double(32|64 ?, be|le?) */

	if (n < 1 || n > 2) {
	    ERR(interp, "PARAM", "double takes (32|64 ?, be|le?), for "
		    "example double(64,le)");
	    goto fail;
	}
	if (!SpanWide(&sp[0], &w) || (w != 32 && w != 64)) {
	    ERR(interp, "PARAM", "double: the width in bits must be 32 or "
		    "64, not \"%.*s\"", (int) sp[0].len, sp[0].start);
	    goto fail;
	}
	if (n == 2 && (endian = EndianWord(sp[1].start, sp[1].len)) < 0) {
	    ERR(interp, "PARAM", "double: the byte order must be be or le, "
		    "not \"%.*s\"", (int) sp[1].len, sp[1].start);
	    goto fail;
	}
    } else {
	/* string(enc, N|z|préfixe|$expr) */

	if (n != 2) {
	    ERR(interp, "PARAM", "string takes (encoding, size|z), for "
		    "example string(utf-8,8) or string(utf-8,z)");
	    goto fail;
	}
	for (enc = 0; enc < NENCODINGS; enc++) {
	    if ((Tcl_Size) strlen(encodings[enc].word) == sp[0].len
		    && memcmp(encodings[enc].word, sp[0].start, sp[0].len)
		    == 0) {
		break;
	    }
	}
	if (enc == NENCODINGS) {
	    ERR(interp, "PARAM", "string: unknown encoding \"%.*s\" (raw, "
		    "ascii, latin1, utf-8, utf-16le, utf-16be, ucs-2le, "
		    "ucs-2be)", (int) sp[0].len, sp[0].start);
	    goto fail;
	}
	if (TclTypesParseSize(interp, d, "string", sp[1].start, sp[1].len, 1,
		params, &w, &cstr, &dyn) != TCL_OK) {
	    goto fail;
	}
	if (cstr && enc == 0) {
	    ERR(interp, "PARAM", "string(raw,z): raw bytes cannot be "
		    "NUL-terminated, use a text encoding");
	    goto fail;
	}
	if (dyn == NULL && !cstr && (w < 1 || w > 1000000)) {
	    ERR(interp, "PARAM", "string: the size in bytes must be "
		    "between 1 and 1000000 or z, not \"%.*s\"",
		    (int) sp[1].len, sp[1].start);
	    goto fail;
	}
	if (dyn != NULL) {
	    dynText = (dyn->kind == TCLDYN_PREFIX) ? dyn->pre->name : dyn->expr;
	}
    }

    /* nom canonique et intern */

    Tcl_DStringInit(&key);
    Tcl_DStringAppend(&key, base, baseLen);
    Tcl_DStringAppend(&key, "(", 1);
    if (isInt) {
	Tcl_DStringAppend(&key, signd ? "s," : "u,", 2);
    } else if (!isDouble) {
	Tcl_DStringAppend(&key, encodings[enc].word, TCL_AUTO_LENGTH);
	Tcl_DStringAppend(&key, ",", 1);
    }
    if (cstr) {
	Tcl_DStringAppend(&key, "z", 1);
    } else if (dyn != NULL) {
	Tcl_DStringAppend(&key, dynText, TCL_AUTO_LENGTH);
    } else {
	char num[24];

	snprintf(num, sizeof(num), "%lld", (long long) w);
	Tcl_DStringAppend(&key, num, TCL_AUTO_LENGTH);
    }
    if (endian != TCLBIN_AUTO) {
	Tcl_DStringAppend(&key, endian == TCLBIN_BE ? ",be" : ",le", 3);
    }
    Tcl_DStringAppend(&key, ")", 1);
    hPtr = Tcl_CreateHashEntry(&d->instances, Tcl_DStringValue(&key), &isNew);
    if (!isNew) {
	inst = (TclType *) Tcl_GetHashValue(hPtr);
    } else {
	inst = TclTypesNewType(d, (TclTypeKind) kind, Tcl_DStringValue(&key));
	inst->nominal = 1;
	TclTypesCopyInto(inst, under);
	if (isInt) {
	    inst->binKind = signd ? TCLBIN_SINT : TCLBIN_UINT;
	    inst->binSize = (Tcl_Size) (w / 8);
	    inst->refine = 1;
	} else if (isDouble) {
	    inst->binKind = TCLBIN_FLOAT;
	    inst->binSize = (Tcl_Size) (w / 8);
	    inst->refine = 1;
	} else if (cstr) {
	    inst->binKind = TCLBIN_CSTRING;
	    inst->binEnc = encodings[enc].tclName;
	} else if (enc == 0) {
	    inst->binKind = TCLBIN_BYTES;
	    inst->binSize = (dyn != NULL) ? 0 : (Tcl_Size) w;
	} else {
	    inst->binKind = TCLBIN_STR;
	    inst->binSize = (dyn != NULL) ? 0 : (Tcl_Size) w;
	    inst->binEnc = encodings[enc].tclName;
	}
	inst->binDyn = dyn;
	inst->binEnd = endian;
	Tcl_SetHashValue(hPtr, inst);
    }
    Tcl_DStringFree(&key);
    Tcl_Free(sp);
    return inst;

  fail:
    Tcl_Free(sp);
    return NULL;
}

/* Octets du terminateur NUL d'un string(enc,z) : 2 pour utf-16, ucs-2. */

int
TclTypesNulWidth(const char *tclName)
{
    int i;

    for (i = 0; i < NENCODINGS; i++) {
	if (encodings[i].tclName != NULL && tclName != NULL
		&& strcmp(encodings[i].tclName, tclName) == 0) {
	    return encodings[i].nul;
	}
    }
    return 1;
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
    Tcl_Size bodyLen,
    int alias)
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
    g->alias = alias;

    for (i = 0; i < n; i++) {
	if (ParseParamDecl(interp, d, spans[i].start, spans[i].len, baseName,
		g, i, &g->params[i]) != TCL_OK) {
	    FreeParam(&g->params[i]);
	    goto fail;
	}
	g->nparams = i + 1;
	p = Tcl_GetString(g->params[i].name);
	/*
	 * Un paramètre absent du corps est FANTÔME : il ne change pas la
	 * représentation mais distingue les instances, qui sont des
	 * nominaux différents (grandeur(L,0.001) et grandeur(L,1.0) sont
	 * deux types). Il n'a de sens que pour type def : l'instance d'un
	 * alias est le corps lui-même, le paramètre y serait perdu.
	 */
	if (alias && !WordOccurs(body, bodyLen, p, (Tcl_Size) strlen(p))) {
	    ERR(interp, "PARAM", "parameter \"%s\" is never used in the "
		    "definition of %s", p, baseName);
	    goto fail;
	}
    }
    Tcl_Free(spans);
    Tcl_SetHashValue(Tcl_CreateHashEntry(&d->generics, baseName, &isNew), g);
    TclTypesBump(d);
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
	if (g->bits == 1 && i == 2
		&& strcmp(Tcl_GetString(bound.p[i].value), "auto") == 0) {
	    continue;			/* bitarray(16,msb0), pas (16,msb0,auto) */
	}
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
    if (g->alias && res != inst) {
	/* alias paramétré : l'instance EST le type du corps (structurel) */

	Tcl_SetHashValue(hPtr, res);
	inst = res;
	goto done;
    }
    if (res != inst) {
	TclTypesCopyInto(inst, res);
    }
    if (g->bits == 1) {
	/* bitarray(n, ordre) : largeur et sens des indices */

	Tcl_WideInt w = 0;
	const char *ord = Tcl_GetString(bound.p[1].value);
	const char *endw = Tcl_GetString(bound.p[2].value);

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
	inst->refine = 1;
	if (strcmp(endw, "be") == 0) {
	    inst->binEnd = TCLBIN_BE;
	} else if (strcmp(endw, "le") == 0) {
	    inst->binEnd = TCLBIN_LE;
	} else if (strcmp(endw, "auto") != 0) {
	    ERR(interp, "PARAM", "bitarray byte order must be be or le, not "
		    "\"%s\"", endw);
	    goto fail;
	}
    }
    if (!g->bits) {
	/* Famille et paramètres liés : les règles « type convert famille
	 * famille » les passent à leur lambda. Une instance appartient à SA
	 * famille, même si son corps est l'instance d'une autre. */
	Tcl_Obj *args = Tcl_NewListObj(0, NULL);
	Tcl_Size k;

	if (inst->familyArgs != NULL) {
	    Tcl_DecrRefCount(inst->familyArgs);
	}

	for (k = 0; k < g->nparams; k++) {
	    Tcl_ListObjAppendElement(NULL, args, bound.p[k].value != NULL
		    ? Tcl_DuplicateObj(bound.p[k].value) : Tcl_NewObj());
	}
	inst->family = g;
	inst->familyArgs = args;
	Tcl_IncrRefCount(args);
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
