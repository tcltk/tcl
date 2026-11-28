/*
 * tclTypes.c --
 *
 *	Table de types (une par interpréteur, globale), interning, validation,
 *	inclusion, cast et commande "type" (spécification §4, §5, §7). Les
 *	descripteurs vivent autant que l'interpréteur. L'ANALYSE de la grammaire
 *	des types (opérateurs . = ; , et accolades) est dans tclTypeParse.c.
 */

#include "tclTypesInt.h"
#include "tclTomMath.h"
#include <ctype.h>
#include <stddef.h>
#include <math.h>

static TypeData *
GetData(Tcl_Interp *interp)
{
    /* gardé dans l'interpréteur (Interp.typesData), posé par TclTypesInit */
    return (TypeData *) ((Interp *) interp)->typesData;
}

/* (uid, gen) de la table de types de l'interpréteur : 0 si aucune. */
int
TclTypesGeneration(Tcl_Interp *interp, size_t *uidPtr, size_t *genPtr)
{
    TypeData *d = GetData(interp);

    if (d == NULL) {
	return 0;
    }
    TclTypesSync(d);
    *uidPtr = d->uid;
    *genPtr = d->gen;
    if (d->nNs > 0) {
	/* v16u : la résolution d'un nom dépend de l'espace courant et de son
	 * chemin (namespace path) : ils entrent dans la génération. */
	Namespace *ns = (Namespace *) TclGetCurrentNamespace(interp);

	*genPtr = (d->gen * (size_t) 0x9E3779B97F4A7C15ULL) ^ ns->nsId;
	*genPtr = (*genPtr * (size_t) 0x9E3779B97F4A7C15ULL)
		^ (size_t) ns->resolverEpoch;
    }
    return 1;
}

/* v16y : les noms de types définis (pour les suggestions de type debug). */
Tcl_Obj *
TclTypesNames(Tcl_Interp *interp)
{
    TypeData *d = GetData(interp);
    Tcl_Obj *res = Tcl_NewListObj(0, NULL);
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;

    if (d == NULL) {
	return res;
    }
    for (hPtr = Tcl_FirstHashEntry(&d->names, &search); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_ListObjAppendElement(NULL, res, Tcl_NewStringObj(
		(const char *) Tcl_GetHashKey(&d->names, hPtr), -1));
    }
    return res;
}

/* --------------------------------------------------------------------
 * v16u : espaces de noms des types
 *
 * Un type défini dans ::geom est rangé sous ::geom::point ; un type global
 * sous son nom simple. Un nom est résolu comme une commande : l'espace
 * courant, son chemin (namespace path), puis l'espace global. Quand un
 * espace est supprimé, ses types perdent leur nom (les valeurs gardent
 * leur descripteur).
 * -------------------------------------------------------------------- */

/* Un nom de type : des mots, séparés par "::" ; "::" en tête possible. */
Tcl_Size
TclTypeNameSpan(const char *p, const char *end)
{
    const char *q = p;

    for (;;) {
	while (q < end && TclTypeIsNameChar(*q)) {
	    q++;
	}
	if (q + 2 < end && q[0] == ':' && q[1] == ':'
		&& TclTypeIsNameChar(q[2])) {
	    q += 2;
	    continue;
	}
	return q - p;
    }
}

/* L'espace où se résolvent les noms : celui d'une famille pendant
 * l'analyse de son corps, sinon l'espace courant. */
static Namespace *
CtxNs(Tcl_Interp *interp, TypeData *d)
{
    if (d != NULL && d->ctxNs != NULL) {
	return d->ctxNs;
    }
    return interp ? (Namespace *) TclGetCurrentNamespace(interp) : NULL;
}

static int
NsIsGlobal(Tcl_Interp *interp, Namespace *ns)
{
    return ns == NULL || interp == NULL
	    || ns == ((Interp *) interp)->globalNsPtr;
}

static int
NsDead(const TypeNs *r)
{
    return r->nsPtr->nsId != r->nsId
	    || (r->nsPtr->flags & (NS_DYING | NS_DEAD));
}

static int
HasPrefix(const char *k, const char *prefix, size_t plen)
{
    return strncmp(k, prefix, plen) == 0;
}

/* Les noms d'un espace supprimé (et de ses enfants) disparaissent. */
static void
PurgeNs(TypeData *d, const char *prefix, size_t plen)
{
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr, *next;

    for (hPtr = Tcl_FirstHashEntry(&d->names, &search); hPtr != NULL;
	    hPtr = next) {
	next = Tcl_NextHashEntry(&search);
	if (HasPrefix((const char *) Tcl_GetHashKey(&d->names, hPtr), prefix,
		plen)) {
	    Tcl_DeleteHashEntry(hPtr);
	}
    }
    for (hPtr = Tcl_FirstHashEntry(&d->generics, &search); hPtr != NULL;
	    hPtr = next) {
	next = Tcl_NextHashEntry(&search);
	if (HasPrefix((const char *) Tcl_GetHashKey(&d->generics, hPtr),
		prefix, plen)) {
	    /* la famille sert encore à ses instances : gardée sous une clé
	     * qui n'est pas un nom (comme type forget) */
	    void *g = Tcl_GetHashValue(hPtr);
	    char key[64];
	    int isNew;

	    Tcl_DeleteHashEntry(hPtr);
	    snprintf(key, sizeof(key), "\1forgotten\1%p", g);
	    Tcl_SetHashValue(Tcl_CreateHashEntry(&d->generics, key, &isNew),
		    g);
	}
    }
    for (hPtr = Tcl_FirstHashEntry(&d->converts, &search); hPtr != NULL;
	    hPtr = next) {
	const char *k = (const char *) Tcl_GetHashKey(&d->converts, hPtr);
	const char *sep = strchr(k, '\1');

	next = Tcl_NextHashEntry(&search);
	if (HasPrefix(k + (*k == '@'), prefix, plen) || (sep != NULL
		&& HasPrefix(sep + 1 + (sep[1] == '@'), prefix, plen))) {
	    Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(hPtr));
	    Tcl_DeleteHashEntry(hPtr);
	}
    }
}

void
TclTypesSync(TypeData *d)
{
    Tcl_Size i = 0;

    while (i < d->nNs) {
	TypeNs *r = &d->nss[i];

	if (!NsDead(r)) {
	    i++;
	    continue;
	}
	PurgeNs(d, r->prefix, r->plen);
	TclNsDecrRefCount(r->nsPtr);
	Tcl_Free(r->prefix);
	d->nss[i] = d->nss[--d->nNs];
	d->gen++;
    }
}

static void
RegisterNs(TypeData *d, Namespace *ns)
{
    Tcl_Size i;
    TypeNs *r;

    for (i = 0; i < d->nNs; i++) {
	if (d->nss[i].nsPtr == ns && !NsDead(&d->nss[i])) {
	    return;
	}
    }
    if (d->nNs == d->capNs) {
	d->capNs = d->capNs ? 2 * d->capNs : 8;
	d->nss = (TypeNs *) Tcl_Realloc(d->nss, d->capNs * sizeof(TypeNs));
    }
    r = &d->nss[d->nNs++];
    r->nsPtr = ns;
    ns->refCount++;
    r->nsId = ns->nsId;
    r->plen = strlen(ns->fullName) + 2;
    r->prefix = (char *) Tcl_Alloc(r->plen + 1);
    snprintf(r->prefix, r->plen + 1, "%s::", ns->fullName);
    d->gen++;
}

static const char *const reservedNames[] = {
    "int", "double", "string", "list", "dict", "array", "var", "any", "flag",
    "type", "at", "bare", "typed", "noted", "typednoted", NULL
};

/*
 * TclTypesQualify --
 *	Le nom sous lequel « type def NAME » range un type : NAME qualifié par
 *	l'espace courant (ou par l'espace qu'il nomme), le nom simple pour un
 *	type global. Les arguments d'une famille, NAME(...), suivent tels quels.
 */
int
TclTypesQualify(Tcl_Interp *interp, TypeData *d, const char *name,
	Tcl_DString *dsPtr)
{
    const char *paren = strchr(name, '(');
    size_t blen = paren ? (size_t) (paren - name) : strlen(name);
    const char *last = NULL, *p, *tail;
    Namespace *ns;
    int i;

    for (p = name; p + 1 < name + blen; p++) {
	if (p[0] == ':' && p[1] == ':') {
	    last = p;
	}
    }
    if (last == NULL) {
	ns = interp ? (Namespace *) TclGetCurrentNamespace(interp) : NULL;
	tail = name;
    } else {
	tail = last + 2;
	while (last > name && last[-1] == ':') {
	    last--;
	}
	if (last == name) {
	    ns = interp ? ((Interp *) interp)->globalNsPtr : NULL;
	} else {
	    Tcl_DString nd;

	    Tcl_DStringInit(&nd);
	    Tcl_DStringAppend(&nd, name, last - name);
	    ns = (Namespace *) Tcl_FindNamespace(interp,
		    Tcl_DStringValue(&nd), NULL, 0);
	    if (ns == NULL) {
		ERR(interp, "NAME", "type \"%s\": unknown namespace \"%s\"",
			name, Tcl_DStringValue(&nd));
		Tcl_DStringFree(&nd);
		return TCL_ERROR;
	    }
	    Tcl_DStringFree(&nd);
	}
    }
    if (tail >= name + blen) {
	ERR(interp, "NAME", "bad type name \"%s\"", name);
	return TCL_ERROR;
    }
    Tcl_DStringInit(dsPtr);
    if (!NsIsGlobal(interp, ns)) {
	for (i = 0; reservedNames[i] != NULL; i++) {
	    if (strlen(reservedNames[i]) == (size_t) (name + blen - tail)
		    && strncmp(tail, reservedNames[i], name + blen - tail) == 0) {
		ERR(interp, "NAME", "type \"%s\" is reserved: a built-in type "
			"cannot be redefined in a namespace", reservedNames[i]);
		return TCL_ERROR;
	    }
	}
	TclTypesSync(d);
	RegisterNs(d, ns);
	Tcl_DStringAppend(dsPtr, ns->fullName, TCL_AUTO_LENGTH);
	Tcl_DStringAppend(dsPtr, "::", 2);
    }
    Tcl_DStringAppend(dsPtr, tail, TCL_AUTO_LENGTH);
    return TCL_OK;
}

/*
 * TclTypesFindName --
 *	Résout NAME (len octets) dans TABLE (les noms ou les familles) :
 *	::a::b absolu ; a::b dans l'espace courant puis global ; b dans
 *	l'espace courant, son chemin, puis global. SELFNAME : le type en cours
 *	de définition (déjà qualifié), reconnu avant la table (*isSelfPtr).
 */
static int
TryKey(TypeData *d, Tcl_HashTable *table, Tcl_DString *ds,
	const char *selfName, Tcl_HashEntry **hPtrPtr, int *isSelfPtr)
{
    const char *k = Tcl_DStringValue(ds);

    if (selfName != NULL && strcmp(k, selfName) == 0) {
	if (isSelfPtr != NULL) {
	    *isSelfPtr = 1;
	}
	return 1;
    }
    *hPtrPtr = Tcl_FindHashEntry(table, k);
    (void) d;
    return *hPtrPtr != NULL;
}

Tcl_HashEntry *
TclTypesFindName(Tcl_Interp *interp, TypeData *d, Tcl_HashTable *table,
	const char *name, Tcl_Size len, const char *selfName, int *isSelfPtr)
{
    Tcl_DString ds;
    Tcl_HashEntry *hPtr = NULL;
    Namespace *ns;
    const char *qual;
    Tcl_Size i;

    if (isSelfPtr != NULL) {
	*isSelfPtr = 0;
    }
    if (len < 0) {
	len = (Tcl_Size) strlen(name);
    }
    TclTypesSync(d);
    Tcl_DStringInit(&ds);
    qual = NULL;
    for (i = 0; i + 1 < len; i++) {
	if (name[i] == ':' && name[i + 1] == ':') {
	    qual = name + i;
	    break;
	}
    }
    if (qual == name) {
	/* ::a::b, ou ::b (global) */
	const char *rest = name + 2;
	Tcl_Size rlen = len - 2, j;
	int more = 0;

	for (j = 0; j + 1 < rlen; j++) {
	    if (rest[j] == ':' && rest[j + 1] == ':') {
		more = 1;
	    }
	}
	if (more) {
	    Tcl_DStringAppend(&ds, name, len);
	} else {
	    Tcl_DStringAppend(&ds, rest, rlen);
	}
	TryKey(d, table, &ds, selfName, &hPtr, isSelfPtr);
	Tcl_DStringFree(&ds);
	return hPtr;
    }
    ns = CtxNs(interp, d);
    if (d->nNs > 0 && !NsIsGlobal(interp, ns)) {
	Tcl_DStringAppend(&ds, ns->fullName, TCL_AUTO_LENGTH);
	Tcl_DStringAppend(&ds, "::", 2);
	Tcl_DStringAppend(&ds, name, len);
	if (TryKey(d, table, &ds, selfName, &hPtr, isSelfPtr)) {
	    goto done;
	}
    }
    if (qual == NULL && d->nNs > 0 && ns != NULL
	    && ns->commandPathLength > 0) {
	for (i = 0; i < ns->commandPathLength; i++) {
	    Namespace *pns = ns->commandPathArray[i].nsPtr;

	    if (pns == NULL) {
		continue;
	    }
	    Tcl_DStringSetLength(&ds, 0);
	    if (!NsIsGlobal(interp, pns)) {
		Tcl_DStringAppend(&ds, pns->fullName, TCL_AUTO_LENGTH);
		Tcl_DStringAppend(&ds, "::", 2);
	    }
	    Tcl_DStringAppend(&ds, name, len);
	    if (TryKey(d, table, &ds, selfName, &hPtr, isSelfPtr)) {
		goto done;
	    }
	}
    }
    Tcl_DStringSetLength(&ds, 0);
    if (qual != NULL) {
	Tcl_DStringAppend(&ds, "::", 2);	/* a::b : ::a::b */
    }
    Tcl_DStringAppend(&ds, name, len);
    TryKey(d, table, &ds, selfName, &hPtr, isSelfPtr);
  done:
    Tcl_DStringFree(&ds);
    return hPtr;
}

void
TclTypesBump(TypeData *d)
{
    d->gen++;
}

static char *
DupStr(const char *s, Tcl_Size len)
{
    char *r = (char *) Tcl_Alloc(len + 1);

    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}

static TclType *
NewType(TypeData *d, TclTypeKind kind, const char *name)
{
    TclType *tp = (TclType *) Tcl_Alloc(sizeof(TclType));

    memset(tp, 0, sizeof(TclType));
    tp->kind = kind;
    tp->name = DupStr(name, (Tcl_Size) strlen(name));
    tp->owner = d;
    if (d->nAll == d->capAll) {
	d->capAll = d->capAll ? 2 * d->capAll : 32;
	d->all = (TclType **) Tcl_Realloc(d->all,
		d->capAll * sizeof(TclType *));
    }
    d->all[d->nAll++] = tp;
    return tp;
}

static void CopyInto(TclType *into, const TclType *res);


static TclType *
Builtin(TypeData *d, TclTypeKind kind, const char *name)
{
    int isNew;
    TclType *tp = NewType(d, kind, name);

    tp->reserved = 1;
    Tcl_SetHashValue(Tcl_CreateHashEntry(&d->names, name, &isNew), tp);
    return tp;
}

void
TclTypesFreeFields(TclTypeField *fields, Tcl_Size n)
{
    Tcl_Size i;

    for (i = 0; i < n; i++) {
	if (fields[i].name != NULL) {
	    Tcl_DecrRefCount(fields[i].name);
	}
	if (fields[i].literal != NULL) {
	    Tcl_DecrRefCount(fields[i].literal);
	}
	if (fields[i].defval != NULL) {
	    Tcl_DecrRefCount(fields[i].defval);
	}
	if (fields[i].defcmd != NULL) {
	    Tcl_DecrRefCount(fields[i].defcmd);
	}
    }
    Tcl_Free(fields);
}

void
TclTypesFreeKeys(TclDictKey *keys, Tcl_Size n)
{
    Tcl_Size i;

    for (i = 0; i < n; i++) {
	if (keys[i].name != NULL) {
	    Tcl_DecrRefCount(keys[i].name);
	}
	if (keys[i].defval != NULL) {
	    Tcl_DecrRefCount(keys[i].defval);
	}
	if (keys[i].defcmd != NULL) {
	    Tcl_DecrRefCount(keys[i].defcmd);
	}
    }
    Tcl_Free(keys);
}

static void PredProgFree(TclType *tp);
static void FreeTypeData(TypeData *d);

void
TclTypeHold(const TclType *tp)
{
    if (tp != NULL && tp->owner != NULL) {
	tp->owner->refs++;
    }
}

void
TclTypeRelease(const TclType *tp)
{
    TypeData *d;

    if (tp == NULL || (d = tp->owner) == NULL) {
	return;
    }
    if (--d->refs == 0 && d->dead) {
	FreeTypeData(d);
    }
}

/*
 * L'interpréteur est supprimé. Les valeurs typées qui lui survivent (passées
 * à un autre interpréteur, gardées par le C) gardent la table en vie : on
 * n'en libère ici que ce qui peut contenir des valeurs (défauts, arguments
 * des familles, règles de conversion), qui sinon la garderaient en vie pour
 * toujours, et on attend la dernière référence.
 */
/* v16z : vide le cache des lambdas annotées (apply {T}{...}). */
static void
LambdasFlush(TypeData *d)
{
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;

    for (hPtr = Tcl_FirstHashEntry(&d->lambdas, &search); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(hPtr));
	Tcl_DeleteHashEntry(hPtr);
    }
}

static void
DeleteData(void *clientData, Tcl_Interp *interp)
{
    TypeData *d = (TypeData *) clientData;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    Tcl_Size i, j;

    if (interp != NULL && ((Interp *) interp)->typesData == d) {
	((Interp *) interp)->typesData = NULL;
    }
    d->dead = 1;
    d->interp = NULL;
    d->refs++;				/* pendant le nettoyage */
    for (hPtr = Tcl_FirstHashEntry(&d->converts, &search); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(hPtr));
    }
    Tcl_DeleteHashTable(&d->converts);
    Tcl_InitHashTable(&d->converts, TCL_STRING_KEYS);
    LambdasFlush(d);
    for (i = 0; i < d->nAll; i++) {
	TclType *tp = d->all[i];

	for (j = 0; tp->fields != NULL && j < tp->nfields; j++) {
	    if (tp->fields[j].defval != NULL) {
		Tcl_Obj *o = tp->fields[j].defval;

		tp->fields[j].defval = NULL;
		Tcl_DecrRefCount(o);
	    }
	    if (tp->fields[j].defcmd != NULL) {
		Tcl_Obj *o = tp->fields[j].defcmd;

		tp->fields[j].defcmd = NULL;
		Tcl_DecrRefCount(o);
	    }
	}
	for (j = 0; tp->keys != NULL && j < tp->nkeys; j++) {
	    if (tp->keys[j].defval != NULL) {
		Tcl_Obj *o = tp->keys[j].defval;

		tp->keys[j].defval = NULL;
		Tcl_DecrRefCount(o);
	    }
	    if (tp->keys[j].defcmd != NULL) {
		Tcl_Obj *o = tp->keys[j].defcmd;

		tp->keys[j].defcmd = NULL;
		Tcl_DecrRefCount(o);
	    }
	}
	if (tp->familyArgs != NULL) {
	    Tcl_Obj *o = tp->familyArgs;

	    tp->familyArgs = NULL;
	    Tcl_DecrRefCount(o);
	}
	tp->predInterp = NULL;		/* v17u : plus d'évaluation */
    }
    for (i = 0; i < d->nNs; i++) {	/* v16u : les espaces de noms */
	TclNsDecrRefCount(d->nss[i].nsPtr);
	Tcl_Free(d->nss[i].prefix);
    }
    d->nNs = 0;
    if (d->nss != NULL) {
	Tcl_Free(d->nss);
	d->nss = NULL;
    }
    d->ctxNs = NULL;
    d->refs -= 2;			/* le nettoyage, l'interpréteur */
    if (d->refs == 0) {
	FreeTypeData(d);
    }
}

static void
FreeTypeData(TypeData *d)
{
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    Tcl_Size i;

    for (hPtr = Tcl_FirstHashEntry(&d->converts, &search); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(hPtr));
    }
    for (i = 0; i < d->nAll; i++) {
	if (d->all[i]->fields != NULL) {
	    TclTypesFreeFields(d->all[i]->fields, d->all[i]->nfields);
	}
	if (d->all[i]->keys != NULL) {
	    TclTypesFreeKeys(d->all[i]->keys, d->all[i]->nkeys);
	}
	if (d->all[i]->familyArgs != NULL) {
	    Tcl_DecrRefCount(d->all[i]->familyArgs);
	}
	if (d->all[i]->pred != NULL) {
	    Tcl_DecrRefCount(d->all[i]->pred);
	}
	PredProgFree(d->all[i]);
	if (d->all[i]->compact != NULL) {
	    TclCompactLayoutRelease(d->all[i]->compact);
	}
	if (d->all[i]->variants != NULL) {
	    Tcl_Free(d->all[i]->variants);
	}
	Tcl_Free(d->all[i]->name);
	Tcl_Free(d->all[i]);
    }
    Tcl_Free(d->all);
    for (i = 0; i < d->nBlobs; i++) {
	Tcl_Free(d->blobs[i]);
    }
    Tcl_Free(d->blobs);
    TclTypesFreeGenerics(d);
    Tcl_DeleteHashTable(&d->generics);
    Tcl_DeleteHashTable(&d->instances);
    Tcl_DeleteHashTable(&d->names);
    Tcl_DeleteHashTable(&d->canon);
    Tcl_DeleteHashTable(&d->parsed);
    Tcl_DeleteHashTable(&d->converts);
    if (d->specs != NULL) {
	Tcl_Free(d->specs);
	d->specs = NULL;
	d->nSpecs = d->capSpecs = 0;
    }
    Tcl_DeleteHashTable(&d->derefs);
    LambdasFlush(d);
    Tcl_DeleteHashTable(&d->lambdas);
    Tcl_Free(d);
}

/*
 * TclTypeRunDefault --
 *
 *	Le défaut d'un champ peut être un script : {int}t = [clock seconds].
 *	Il est évalué au niveau global (les variables locales de l'appelant ne
 *	sont pas visibles) à CHAQUE construction d'une valeur, et son résultat
 *	est ensuite stocké : la valeur ne le rappelle plus.
 */

int
TclTypeRunDefault(Tcl_Interp *interp, const char *what, Tcl_Obj *script,
	Tcl_Obj **resultPtr)
{
    int rc;

    if (interp == NULL) {
	return TCL_ERROR;
    }
    Tcl_IncrRefCount(script);
    rc = Tcl_EvalObjEx(interp, script, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(script);
    if (rc != TCL_OK) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("cannot compute the default "
		"of %s: %s", what, Tcl_GetString(Tcl_GetObjResult(interp))));
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "DEFAULT", (char *) NULL);
	return TCL_ERROR;
    }
    *resultPtr = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
    Tcl_ResetResult(interp);
    return TCL_OK;
}

Tcl_Size
TclTypeFieldIndex(const TclType *tp, const char *name)
{
    Tcl_Size i;

    for (i = 0; i < tp->nfields; i++) {
	if (tp->fields[i].name != NULL
		&& strcmp(Tcl_GetString(tp->fields[i].name), name) == 0) {
	    return i;
	}
    }
    return -1;
}

/*
 * v18b : les types de variante. S/v est une somme qui a les variantes de S
 * mais n'en permet qu'une, v ; une valeur de S/v est une valeur de S (son
 * annotation nomme S et v, TypeRefObj). S/v/w restreint aussi la charge :
 * la variante w de la somme qu'est la charge de v.
 */

/* L'indice du membre NAME de tp (variante nommée ou littéral), ou -1. */
Tcl_Size
TclTypeMemberIndex(const TclType *tp, const char *name, Tcl_Size len)
{
    Tcl_Size i;

    if (len < 0) {
	len = (Tcl_Size) strlen(name);
    }
    for (i = 0; i < tp->nfields; i++) {
	Tcl_Size nl;
	const char *nm;

	if (tp->fields[i].name != NULL) {
	    nm = Tcl_GetStringFromObj(tp->fields[i].name, &nl);
	} else if (tp->kind == TCLT_SUM && tp->fields[i].literal != NULL) {
	    nm = Tcl_GetStringFromObj(tp->fields[i].literal, &nl);
	} else {
	    continue;
	}
	if (nl == len && memcmp(nm, name, len) == 0) {
	    return i;
	}
    }
    return -1;
}

/* v18b : un nom de membre écrit après « / » : tel quel si c'est un nom,
 * sinon entre guillemets (un littéral : binop/"+", pi/"3.14159") */
void
TclTypeAppendMemberName(Tcl_DString *ds, const char *name)
{
    const char *p;
    int plain = (name[0] != '\0');

    for (p = name; *p != '\0'; p++) {
	if (!TclTypeIsNameChar(*p)) {
	    plain = 0;
	    break;
	}
    }
    if (plain) {
	Tcl_DStringAppend(ds, name, -1);
	return;
    }
    Tcl_DStringAppend(ds, "\"", 1);
    for (p = name; *p != '\0'; p++) {
	if (*p == '"' || *p == '\\') {
	    Tcl_DStringAppend(ds, "\\", 1);
	}
	Tcl_DStringAppend(ds, p, 1);
    }
    Tcl_DStringAppend(ds, "\"", 1);
}

/* Le nom d'un membre (variante nommée, ou littéral) */
static const char *
MemberName(const TclType *tp, Tcl_Size i)
{
    return tp->fields[i].name != NULL ? Tcl_GetString(tp->fields[i].name)
	    : tp->fields[i].literal != NULL
	    ? Tcl_GetString(tp->fields[i].literal) : "?";
}

/*
 * Le type de variante (sum, idx), charge restreinte à payload (NULL : la
 * charge déclarée). sum est une somme racine (pas un type de variante).
 */
TclType *
TclTypeVariantOf(TclType *sum, Tcl_Size idx, TclType *payload)
{
    TypeData *d = sum->owner;
    TclType *vt;
    Tcl_DString name;
    Tcl_Size i;

    if (sum->variantOf != NULL) {
	if (idx != sum->variantIdx) {
	    return NULL;
	}
	if (payload == NULL) {
	    return sum;
	}
	sum = sum->variantOf;
    }
    if (payload == sum->fields[idx].type) {
	payload = NULL;
    }
    if (payload == NULL && sum->variants != NULL
	    && sum->variants[idx] != NULL) {
	return sum->variants[idx];
    }
    Tcl_DStringInit(&name);
    Tcl_DStringAppend(&name, sum->name, -1);
    Tcl_DStringAppend(&name, "/", 1);
    TclTypeAppendMemberName(&name, MemberName(sum, idx));
    if (payload != NULL) {
	/* la charge restreinte : son chemin de membres (temp/fahrenheit sous
	 * mesure/some : /fahrenheit) */
	const TclType *p = payload;

	while (p != NULL && p->variantOf != NULL) {
	    Tcl_DStringAppend(&name, "/", 1);
	    TclTypeAppendMemberName(&name, MemberName(p->variantOf,
		    p->variantIdx));
	    p = p->fields[p->variantIdx].type;
	    if (p != NULL && p->variantOf == NULL) {
		break;
	    }
	}
	{
	    /* déjà interné ? (par son nom) */
	    Tcl_HashEntry *h = Tcl_FindHashEntry(&d->canon,
		    Tcl_DStringValue(&name));

	    if (h != NULL) {
		vt = (TclType *) Tcl_GetHashValue(h);
		if (vt->variantOf == sum && vt->variantIdx == idx
			&& vt->fields[idx].type == payload) {
		    Tcl_DStringFree(&name);
		    return vt;
		}
	    }
	}
    }
    vt = NewType(d, TCLT_SUM, Tcl_DStringValue(&name));
    vt->sumMode = sum->sumMode;
    vt->variantOf = sum;
    vt->variantIdx = idx;
    vt->nfields = sum->nfields;
    vt->fields = (TclTypeField *) Tcl_Alloc(sum->nfields
	    * sizeof(TclTypeField));
    for (i = 0; i < sum->nfields; i++) {
	vt->fields[i] = sum->fields[i];
	if (vt->fields[i].name != NULL) {
	    Tcl_IncrRefCount(vt->fields[i].name);
	}
	if (vt->fields[i].literal != NULL) {
	    Tcl_IncrRefCount(vt->fields[i].literal);
	}
	if (vt->fields[i].defval != NULL) {
	    Tcl_IncrRefCount(vt->fields[i].defval);
	}
	if (vt->fields[i].defcmd != NULL) {
	    Tcl_IncrRefCount(vt->fields[i].defcmd);
	}
    }
    if (payload != NULL) {
	int isNew;

	vt->fields[idx].type = payload;
	Tcl_SetHashValue(Tcl_CreateHashEntry(&d->canon,
		Tcl_DStringValue(&name), &isNew), vt);
    } else {
	if (sum->variants == NULL) {
	    sum->variants = (TclType **) Tcl_Alloc(sum->nfields
		    * sizeof(TclType *));
	    memset(sum->variants, 0, sum->nfields * sizeof(TclType *));
	}
	sum->variants[idx] = vt;
    }
    Tcl_DStringFree(&name);
    return vt;
}

/* Le type sous un nominal scalaire (une somme, un produit nommés le sont
 * eux-mêmes). */
static TclType *
MemberBase(TclType *tp)
{
    while (tp != NULL && tp->kind != TCLT_SUM && tp->kind != TCLT_PRODUCT
	    && tp->under != NULL) {
	tp = tp->under;
    }
    return tp;
}

/*
 * v18b : T/m1/m2... : les membres d'un type. Une somme : sa variante (un
 * type de variante) ; un type de variante : la variante de sa charge (la
 * charge est restreinte) ; un produit : le type du champ.
 */
TclType *
TclTypeMember(Tcl_Interp *interp, TypeData *d, TclType *tp, const char *p,
	Tcl_Size len)
{
    const char *end = p + len;

    (void) d;
    while (p < end) {
	const char *m, *q;
	Tcl_Size i;
	TclType *b;

	if (*p != '/') {
	    ERR(interp, "BADMEMBER", "bad member \"%.*s\"", (int) (end - p), p);
	    return NULL;
	}
	m = p + 1;
	if (m < end && *m == '"') {
	    /* /"a b" : un littéral entre guillemets */
	    Tcl_DString un;
	    const char *r;

	    Tcl_DStringInit(&un);
	    for (r = m + 1; r < end && *r != '"'; r++) {
		if (*r == '\\' && r + 1 < end) {
		    r++;
		}
		Tcl_DStringAppend(&un, r, 1);
	    }
	    if (r >= end) {
		Tcl_DStringFree(&un);
		ERR(interp, "BADMEMBER", "missing \" in member %.*s",
			(int) (end - p), p);
		return NULL;
	    }
	    q = r + 1;
	    b = MemberBase(tp);
	    if (b == NULL || b->kind != TCLT_SUM || b->variantOf != NULL
		    || (i = TclTypeMemberIndex(b, Tcl_DStringValue(&un),
			Tcl_DStringLength(&un))) < 0) {
		ERR(interp, "UNKNOWN", "%s has no variant %.*s", tp->name,
			(int) (q - m), m);
		Tcl_DStringFree(&un);
		return NULL;
	    }
	    Tcl_DStringFree(&un);
	    tp = TclTypeVariantOf(b, i, NULL);
	    p = q;
	    continue;
	}
	for (q = m; q < end && *q != '/'; q++) {
	}
	b = MemberBase(tp);
	if (b != NULL && b->variantOf != NULL) {
	    /* S/v/w : w est une variante de la charge de v */
	    TclType *pl = b->fields[b->variantIdx].type, *pb, *sub;

	    pb = MemberBase(pl);
	    if (pb == NULL || pb->kind != TCLT_SUM) {
		ERR(interp, "BADMEMBER", "%s: the value of variant %s is %s, "
			"not a sum: \"/%.*s\" names nothing in it", b->name,
			MemberName(b->variantOf, b->variantIdx),
			pl != NULL ? pl->name : "a literal", (int) (q - m), m);
		return NULL;
	    }
	    sub = TclTypeMember(interp, d, pl, p, q - p);
	    if (sub == NULL) {
		return NULL;
	    }
	    tp = TclTypeVariantOf(b->variantOf, b->variantIdx, sub);
	} else if (b != NULL && b->kind == TCLT_SUM) {
	    i = TclTypeMemberIndex(b, m, q - m);
	    if (i < 0) {
		ERR(interp, "UNKNOWN", "%s has no variant \"%.*s\"", b->name,
			(int) (q - m), m);
		return NULL;
	    }
	    tp = TclTypeVariantOf(b, i, NULL);
	} else if (b != NULL && b->kind == TCLT_PRODUCT) {
	    i = TclTypeMemberIndex(b, m, q - m);
	    if (i < 0 || b->fields[i].type == NULL) {
		ERR(interp, "UNKNOWN", "%s has no field \"%.*s\"", b->name,
			(int) (q - m), m);
		return NULL;
	    }
	    tp = b->fields[i].type;
	} else {
	    ERR(interp, "BADMEMBER", "%s is neither a product nor a sum: "
		    "\"/%.*s\" names nothing in it", tp->name, (int) (q - m),
		    m);
	    return NULL;
	}
	if (tp == NULL) {
	    ERR(interp, "BADMEMBER", "bad member \"%.*s\"", (int) (q - m), m);
	    return NULL;
	}
	p = q;
    }
    return tp;
}

/*
 * "any" et les méta-types (v17e) : bare, typed, noted, typednoted, et
 * leurs unions (type def annotee {noted | typednoted}). Une valeur y entre
 * telle quelle, son type et ses notes gardés, si son méta-type y est
 * (TclTypeMetaCheck) : une exigence, pas une conversion. any est l'union
 * des quatre, l'absence d'exigence.
 */
int
TclTypeMetaMask(const TclType *tp)
{
    Tcl_Size i;
    int m = 0;

    if (tp == NULL) {
	return 0;
    }
    if (tp->metaMask != 0) {
	return tp->metaMask;
    }
    if (tp->kind == TCLT_SUM && tp->sumMode == TCLT_SUM_UNION
	    && tp->nfields > 0) {
	for (i = 0; i < tp->nfields; i++) {
	    const TclType *ft = tp->fields[i].type;

	    if (ft == NULL || ft->metaMask == 0) {
		return 0;
	    }
	    m |= ft->metaMask;
	}
    }
    return m;
}

int
TclTypeIsAny(const TclType *tp)
{
    return TclTypeMetaMask(tp) != 0;
}

const char *
TclMetaName(TclMeta m)
{
    switch (m) {
    case TCL_META_BARE: return "bare";
    case TCL_META_TYPED: return "typed";
    case TCL_META_NOTED: return "noted";
    case TCL_META_TYPED_NOTED: return "typednoted";
    }
    return "?";
}

/*
 * La valeur v entre-t-elle dans tp ? TCL_OK si tp n'est pas un méta-type,
 * ou si l'état de v y est. Sinon le message (« expected a typed value but
 * got a bare value »).
 */
int
TclTypeMetaCheck(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *objPtr)
{
    int mask = TclTypeMetaMask(tp);
    TclMeta m;

    if (mask == 0 || mask == TCL_META_ANY) {
	return TCL_OK;
    }
    m = TclAnnotMeta(objPtr);
    if (mask & m) {
	return TCL_OK;
    }
    if (interp != NULL) {
	TclType *vt = NULL;
	Tcl_Obj *msg;

	(void) TclAnnotInfo(objPtr, NULL, &vt, NULL, NULL);
	msg = (tp->metaMask != 0)
		? Tcl_ObjPrintf("expected a %s value but got a %s value",
			tp->name, TclMetaName(m))
		: Tcl_ObjPrintf("expected %s but got a %s value", tp->name,
			TclMetaName(m));
	if (vt != NULL) {
	    Tcl_AppendPrintfToObj(msg, " (%s)", vt->name);
	} else {
	    Tcl_AppendPrintfToObj(msg, " \"%s\"", Tcl_GetString(objPtr));
	}
	Tcl_SetObjResult(interp, msg);
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "META", (char *) NULL);
    }
    return TCL_ERROR;
}

int
TclTypeIsPlainString(const TclType *tp)
{
    return tp->kind == TCLT_STRING && !tp->nominal && !TclTypeIsAny(tp);
}

/* --------------------------------------------------------------------
 * Constructeurs de types anonymes internés
 *
 * Une collection est identifiée par sa structure : l'arité d'une liste et
 * les clés d'un dictionnaire font partie du nom canonique, car list(2).int
 * et list.int sont deux types distincts.
 * -------------------------------------------------------------------- */

static void
AppendSize(Tcl_DString *ds, Tcl_Size n)
{
    char buf[40];

    snprintf(buf, sizeof(buf), "%lld", (long long) n);
    Tcl_DStringAppend(ds, buf, TCL_AUTO_LENGTH);
}

static TclType *
Intern(TypeData *d, Tcl_DString *ds, TclTypeKind kind, TclType *elem,
	int *isNewPtr)
{
    int isNew;
    Tcl_HashEntry *hPtr = Tcl_CreateHashEntry(&d->canon,
	    Tcl_DStringValue(ds), &isNew);
    TclType *tp;

    if (isNew) {
	tp = NewType(d, kind, Tcl_DStringValue(ds));
	tp->elem = elem;
	tp->key = d->tString;		/* les clés sont toujours du texte */
	Tcl_SetHashValue(hPtr, tp);
    } else {
	tp = (TclType *) Tcl_GetHashValue(hPtr);
    }
    *isNewPtr = isNew;
    return tp;
}

TclType *
TclTypesMakeList(TypeData *d, TclType *elem)
{
    return TclTypesMakeListN(d, elem, -1);
}

/*
 * list.T écrit explicitement. Pour T = string c'est "list<string>", type
 * distinct de la liste nue "list" : ses éléments sont atomiques (enveloppés
 * à la lecture, un élément n'est pas redécoupé en mots), alors que ceux de
 * "list" restent du texte Tcl ordinaire. Les deux se contiennent mutuellement.
 */
TclType *
TclTypesMakeListExplicit(TypeData *d, TclType *elem)
{
    Tcl_DString ds;
    TclType *tp;
    int isNew;

    if (elem != d->tString) {
	return TclTypesMakeListN(d, elem, -1);
    }
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "list<string>", TCL_AUTO_LENGTH);
    tp = Intern(d, &ds, TCLT_LIST, elem, &isNew);
    if (isNew) {
	tp->arity = -1;
    }
    Tcl_DStringFree(&ds);
    return tp;
}

/* Deux types « identiques » pour les contrôles d'affectation : le même
 * descripteur, ou list et list<string>. */
int
TclTypeSame(const TclType *a, const TclType *b)
{
    return a == b || (a->kind == TCLT_LIST && b->kind == TCLT_LIST
	    && a->arity < 0 && b->arity < 0 && !a->nominal && !b->nominal
	    && !a->lenRange && !b->lenRange
	    && a->elem == b->elem && a->elem->kind == TCLT_STRING
	    && !a->elem->nominal);
}

/*
 * Ensemble de valeurs d'un type numérique : classe (1 non signé, 2 signé,
 * 3 flottant) et largeur en bits (0 : non bornée, int et double). Rend 0 si
 * le type n'est pas numérique à représentation connue.
 */

static int
ValueSet(const TclType *tp, int builtinInt, int builtinDouble, int *bitsPtr)
{
    if (tp->binKind == TCLBIN_UINT || tp->binKind == TCLBIN_SINT
	    || tp->binKind == TCLBIN_FLOAT) {
	*bitsPtr = (int) (tp->binSize * 8);
	return tp->binKind == TCLBIN_UINT ? 1 : tp->binKind == TCLBIN_SINT
		? 2 : 3;
    }
    if (tp->kind == TCLT_INT && tp->bitWidth > 0) {
	*bitsPtr = (int) tp->bitWidth;
	return 1;
    }
    if (builtinInt) {
	*bitsPtr = 0;
	return 2;
    }
    if (builtinDouble) {
	*bitsPtr = 0;
	return 3;
    }
    return 0;
}

/*
 * TclTypeAssignable --
 *
 *	Même type, ou ÉLARGISSEMENT automatique. Seuls les raffinements
 *	(int(s,n), int(u,n), double(n), bitarray(n)) élargissent : ce
 *	sont les mêmes valeurs qu'un int ou un double, vues dans une plage. Un
 *	type défini par `type def` reste nominal : il élargit vers int, double
 *	ou un raffinement, mais rien d'autre n'entre dans lui.
 */

int
TclTypeAssignable(const TclType *expected, const TclType *got)
{
    int ek, gk, eb, gb;

    if (TclTypeSame(expected, got)) {
	return 1;
    }
    if (expected->pred != NULL) {
	return 0;	/* v17u : un prédicat se vérifie sur la valeur */
    }
    if (expected->binLink != NULL && expected->binLink->ref == got) {
	return 1;			/* l'adresse d'un lien de frontière */
    }
    if (expected->binBase != NULL || got->binBase != NULL) {
	/* at(..).T, S($champ), list($n).T : les mêmes valeurs que leur base */
	return TclTypeAssignable(expected->binBase ? expected->binBase : expected,
		got->binBase ? got->binBase : got);
    }
    ek = ValueSet(expected, expected->reserved
	    && strcmp(expected->name, "int") == 0, expected->reserved
	    && strcmp(expected->name, "double") == 0, &eb);
    gk = ValueSet(got, got->reserved && strcmp(got->name, "int") == 0,
	    got->reserved && strcmp(got->name, "double") == 0, &gb);
    if (ek == 0 || gk == 0) {
	return 0;
    }
    if (!expected->reserved && !expected->refine) {
	return 0;			/* nominal : rien d'autre n'y entre */
    }
    if (expected->bitWidth > 0 && got->bitWidth > 0
	    && expected->bitMsb0 != got->bitMsb0) {
	return 0;			/* l'indice 0 changerait de sens */
    }
    if (eb == 0) {
	/* int accepte tout entier ; double tout nombre */
	return ek == 2 ? (gk == 1 || gk == 2) : 1;
    }
    if (gb == 0) {
	return 0;			/* rétrécissement */
    }
    switch (ek) {
    case 1:
	return gk == 1 && gb <= eb;
    case 2:
	return (gk == 2 && gb <= eb) || (gk == 1 && gb < eb);
    default:
	return gk == 3 && gb <= eb;
    }
}

/*
 * Type GÉNÉRIQUE d'un nominal scalaire : on suit la chaîne des bases
 * (dist -> mm -> double ; port -> int(u,16)). Un type structurel, un
 * raffinement, ou un nominal composé (produit, liste...), est son propre
 * générique.
 */
const TclType *
TclTypeGenericOf(const TclType *tp)
{
    int depth;

    /* Un raffinement (int(u,8), bitarray(8)) est une contrainte de plage,
     * pas un nominal d'auteur : la chaîne s'y arrête. */
    for (depth = 0; tp != NULL && tp->nominal && !tp->refine
	    && tp->under != NULL && depth < 64; depth++) {
	tp = tp->under;
    }
    return tp;
}

/*
 * Un type GOT entre-t-il dans le contrat EXPECTED ? Oui s'il y est
 * assignable (même type, élargissement d'un raffinement), ou si l'un des
 * types de sa chaîne de bases l'est : un nominal s'efface devant sa base
 * (dist -> mm -> double, px -> int, port -> int(u,16) -> int). Deux
 * nominaux sans lien de chaîne ne sont jamais équivalents, même s'ils ont
 * le même générique (mm et kg) : il faut une conversion déclarée.
 */
int
TclTypeEnters(const TclType *expected, const TclType *got)
{
    const TclType *b;
    int depth;

    if (got != NULL && expected != NULL && got->formOf != NULL
	    && got->formOf == expected) {
	return 1;		/* 24/11 : une forme de T est un T */
    }
    for (b = got, depth = 0; b != NULL && depth < 64; b = b->under,
	    depth++) {
	if (TclTypeAssignable(expected, b)) {
	    return 1;
	}
	if (!b->nominal || b->refine) {
	    break;		/* fin de la chaîne : un type structurel */
	}
    }
    /*
     * Un nominal de collection (type def polygone list.point) n'a pas de
     * chaîne "under" : sa base est sa propre structure. Il entre dans une
     * collection STRUCTURELLE de même genre dont les éléments l'acceptent
     * (list.point, list(4).point, list($n).point, list) ; le nombre
     * d'éléments est vérifié ensuite, sur le contenu. Deux nominaux restent
     * distincts.
     */
    if (got != NULL && got->nominal && !got->refine && got->under == NULL
	    && !expected->nominal && expected->kind == got->kind
	    && (got->kind == TCLT_LIST || got->kind == TCLT_DICT)) {
	if (expected->kind == TCLT_LIST && expected->arity >= 0
		&& got->arity >= 0 && expected->arity != got->arity) {
	    return 0;
	}
	if (expected->elem == NULL || TclTypeIsAny(expected->elem)
		|| TclTypeIsPlainString(expected->elem)) {
	    return 1;
	}
	if (got->elem != NULL && TclTypeEnters(expected->elem, got->elem)) {
	    return 1;
	}
    }
    return 0;
}

/* list(N).elem : exactement N éléments. arity < 0 : liste ordinaire,
 * extensible. */
TclType *
TclTypesMakeListN(TypeData *d, TclType *elem, Tcl_Size arity)
{
    Tcl_DString ds;
    TclType *tp;
    int isNew;

    if (arity < 0 && elem == d->tString) {
	return d->tList;
    }
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "list", TCL_AUTO_LENGTH);
    if (arity >= 0) {
	Tcl_DStringAppend(&ds, "(", 1);
	AppendSize(&ds, arity);
	Tcl_DStringAppend(&ds, ")", 1);
    }
    Tcl_DStringAppend(&ds, "<", 1);
    Tcl_DStringAppend(&ds, elem->name, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, ">", 1);
    tp = Intern(d, &ds, TCLT_LIST, elem, &isNew);
    if (isNew) {
	tp->arity = arity;
    }
    Tcl_DStringFree(&ds);
    return tp;
}

/*
 * v17g : list(LO:HI).elem, une liste extensible de LO à HI éléments (HI
 * < 0 : Inf). list(N:N) est list(N), list(0:Inf) est list.elem.
 */
TclType *
TclTypesMakeListRange(TypeData *d, TclType *elem, Tcl_Size lo, Tcl_Size hi)
{
    Tcl_DString ds;
    TclType *tp;
    int isNew;

    if (hi >= 0 && lo == hi) {
	return TclTypesMakeListN(d, elem, lo);
    }
    if (lo == 0 && hi < 0) {
	return TclTypesMakeListN(d, elem, -1);
    }
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "list(", 5);
    AppendSize(&ds, lo);
    Tcl_DStringAppend(&ds, ":", 1);
    if (hi < 0) {
	Tcl_DStringAppend(&ds, "Inf", 3);
    } else {
	AppendSize(&ds, hi);
    }
    Tcl_DStringAppend(&ds, ")<", 2);
    Tcl_DStringAppend(&ds, elem->name, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, ">", 1);
    tp = Intern(d, &ds, TCLT_LIST, elem, &isNew);
    if (isNew) {
	tp->arity = -1;
	tp->lenRange = 1;
	tp->lenMin = lo;
	tp->lenMax = hi;
    }
    Tcl_DStringFree(&ds);
    return tp;
}

/*
 * v17g : le nombre d'éléments n convient-il à la liste tp ? list(N) :
 * exactement N ; list(LO:HI) : entre les deux. Sinon le message.
 */
int
TclTypeListLenCheck(Tcl_Interp *interp, const TclType *tp, Tcl_Size n)
{
    if (tp == NULL || tp->kind != TCLT_LIST) {
	return TCL_OK;
    }
    if (tp->arity >= 0 && n != tp->arity) {
	ERR(interp, "LIST", "%s needs exactly %" TCL_SIZE_MODIFIER
		"d elements but got %" TCL_SIZE_MODIFIER "d", tp->name,
		tp->arity, n);
	return TCL_ERROR;
    }
    if (tp->lenRange && (n < tp->lenMin
	    || (tp->lenMax >= 0 && n > tp->lenMax))) {
	if (tp->lenMax < 0) {
	    ERR(interp, "LIST", "%s needs at least %" TCL_SIZE_MODIFIER
		    "d element%s but got %" TCL_SIZE_MODIFIER "d", tp->name,
		    tp->lenMin, tp->lenMin == 1 ? "" : "s", n);
	} else {
	    ERR(interp, "LIST", "%s needs %" TCL_SIZE_MODIFIER "d to %"
		    TCL_SIZE_MODIFIER "d elements but got %" TCL_SIZE_MODIFIER
		    "d", tp->name, tp->lenMin, tp->lenMax, n);
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}

/* list(PRÉFIXE).T, list($expr).T : une liste ordinaire dont le nombre
 * d'éléments est lu dans le fichier. Mêmes valeurs que list.T (binBase). */
TclType *
TclTypesMakeListDyn(TypeData *d, TclType *elem, TclBinDyn *dyn)
{
    Tcl_DString ds;
    TclType *tp, *base = TclTypesMakeListN(d, elem, -1);
    int isNew;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "list(", TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, dyn->kind == TCLDYN_PREFIX ? dyn->pre->name
	    : dyn->expr, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, ")<", 2);
    Tcl_DStringAppend(&ds, elem->name, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, ">", 1);
    tp = Intern(d, &ds, TCLT_LIST, elem, &isNew);
    if (isNew) {
	tp->arity = -1;
	tp->binDyn = dyn;
	tp->binBase = base;
    }
    Tcl_DStringFree(&ds);
    return tp;
}

/* at(EXPR).T : la valeur de type T est située à la position EXPR du fichier
 * (depuis le début de la valeur lue), et non à la suite du champ précédent.
 * Mêmes valeurs que T (binBase). */
TclType *
TclTypesMakeAt(TypeData *d, TclType *elem, const char *expr)
{
    Tcl_DString ds;
    Tcl_HashEntry *hPtr;
    TclType *tp;
    int isNew;

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "at(", 3);
    Tcl_DStringAppend(&ds, expr, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, ").", 2);
    Tcl_DStringAppend(&ds, elem->name, TCL_AUTO_LENGTH);
    hPtr = Tcl_CreateHashEntry(&d->canon, Tcl_DStringValue(&ds), &isNew);
    if (isNew) {
	tp = NewType(d, elem->kind, Tcl_DStringValue(&ds));
	tp->nominal = elem->nominal;
	CopyInto(tp, elem);
	tp->refine = elem->refine;
	tp->binAt = TclTypesKeepText(d, expr, (Tcl_Size) strlen(expr));
	tp->binAtBody = elem;
	tp->binBase = (elem->binBase != NULL) ? elem->binBase : elem;
	Tcl_SetHashValue(hPtr, tp);
    } else {
	tp = (TclType *) Tcl_GetHashValue(hPtr);
    }
    Tcl_DStringFree(&ds);
    return tp;
}

/* *(ORIGINE,REPR ?,null=V?).T : la valeur de type T est désignée par l'entier
 * REPR, compté depuis ORIGINE. Mêmes valeurs que T (binBase). Si T est une
 * somme {T}some, {unit}none, un lien nul vaut none. */
TclType *
TclTypesMakeLink(Tcl_Interp *interp, TypeData *d, TclType *elem,
	TclBinLink *link)
{
    Tcl_DString ds;
    Tcl_HashEntry *hPtr;
    TclType *tp;
    int isNew;

    (void) interp;
    link->target = elem;
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "*(", 2);
    Tcl_DStringAppend(&ds, link->origin, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, ",", 1);
    Tcl_DStringAppend(&ds, link->repr->name, TCL_AUTO_LENGTH);
    if (link->nullVal != NULL) {
	Tcl_DStringAppend(&ds, ",null=", 6);
	Tcl_DStringAppend(&ds, link->nullVal, TCL_AUTO_LENGTH);
    }
    if (link->opaque) {
	Tcl_DStringAppend(&ds, ",opaque", 7);
    }
    Tcl_DStringAppend(&ds, ").", 2);
    Tcl_DStringAppend(&ds, elem->name, TCL_AUTO_LENGTH);
    hPtr = Tcl_CreateHashEntry(&d->canon, Tcl_DStringValue(&ds), &isNew);
    if (isNew) {
	tp = NewType(d, elem->kind, Tcl_DStringValue(&ds));
	tp->nominal = elem->nominal;
	CopyInto(tp, elem);
	tp->refine = elem->refine;
	tp->binLink = link;
	tp->binBase = (elem->binBase != NULL) ? elem->binBase : elem;
	Tcl_SetHashValue(hPtr, tp);
	{
	    /* FR : l'adresse d'un lien de frontière (type binary -linkdepth) :
	     *      la position absolue de la cible, annotée par ce type.
	     * EN : the address of a frontier link (type binary -linkdepth):
	     *      the target's absolute position, annotated with this type. */

	    Tcl_DString rs;
	    Tcl_HashEntry *rh;
	    int rNew;
	    TclType *rt;

	    Tcl_DStringInit(&rs);
	    Tcl_DStringAppend(&rs, "&", 1);
	    Tcl_DStringAppend(&rs, Tcl_DStringValue(&ds), TCL_AUTO_LENGTH);
	    rh = Tcl_CreateHashEntry(&d->canon, Tcl_DStringValue(&rs), &rNew);
	    if (rNew) {
		rt = NewType(d, TCLT_INT, Tcl_DStringValue(&rs));
		rt->nominal = 1;
		rt->under = d->tInt;
		rt->refOf = tp;
		Tcl_SetHashValue(rh, rt);
	    } else {
		rt = (TclType *) Tcl_GetHashValue(rh);
	    }
	    Tcl_DStringFree(&rs);
	    link->ref = rt;
	}
    } else {
	tp = (TclType *) Tcl_GetHashValue(hPtr);
    }
    Tcl_DStringFree(&ds);
    return tp;
}

/* S($champ) : la somme S dont la variante est choisie par la valeur du champ
 * (déjà lu) du même produit ou d'un produit englobant. La variante est celle
 * dont le nom égale la valeur du champ ({int}1, {string}2 : étiquettes 1, 2). */
TclType *
TclTypesMakeTagged(Tcl_Interp *interp, TypeData *d, TclType *sum,
	const char *field, int isConst)
{
    Tcl_DString ds;
    Tcl_HashEntry *hPtr;
    TclType *tp;
    Tcl_Size i;
    int isNew;

    if (sum->kind != TCLT_SUM || sum->nfields == 0) {
	ERR(interp, "ARITY", "\"%s\": only a sum type can be chosen by a "
		"field value, as in %s($tag)", sum->name, sum->name);
	return NULL;
    }
    for (i = 0; i < sum->nfields; i++) {
	if (sum->fields[i].type == NULL) {
	    ERR(interp, "ARITY", "\"%s\" has a literal alternative: only a "
		    "sum whose variants all carry a value can be chosen by "
		    "a field", sum->name);
	    return NULL;
	}
    }
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, sum->name, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, isConst ? "(" : "($", isConst ? 1 : 2);
    Tcl_DStringAppend(&ds, field, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, ")", 1);
    hPtr = Tcl_CreateHashEntry(&d->canon, Tcl_DStringValue(&ds), &isNew);
    if (isNew) {
	tp = NewType(d, TCLT_SUM, Tcl_DStringValue(&ds));
	tp->nominal = sum->nominal;
	CopyInto(tp, sum);
	if (isConst) {
	    /* binTag "=nom" : sélecteur constant (paramètre de type) */

	    Tcl_DString tag;

	    Tcl_DStringInit(&tag);
	    Tcl_DStringAppend(&tag, "=", 1);
	    Tcl_DStringAppend(&tag, field, TCL_AUTO_LENGTH);
	    tp->binTag = TclTypesKeepText(d, Tcl_DStringValue(&tag),
		    (Tcl_Size) Tcl_DStringLength(&tag));
	    Tcl_DStringFree(&tag);
	} else {
	    tp->binTag = TclTypesKeepText(d, field, (Tcl_Size) strlen(field));
	}
	tp->binBase = (sum->binBase != NULL) ? sum->binBase : sum;
	Tcl_SetHashValue(hPtr, tp);
    } else {
	tp = (TclType *) Tcl_GetHashValue(hPtr);
    }
    Tcl_DStringFree(&ds);
    return tp;
}

/* dict.elem (keyed == 0) ou dict(clés).elem. Prend possession de keys. */
TclType *
TclTypesMakeDict(TypeData *d, TclType *elem, int keyed, TclDictKey *keys,
	Tcl_Size nkeys)
{
    Tcl_DString ds;
    TclType *tp;
    Tcl_Size i;
    int isNew;

    if (!keyed && elem == d->tString) {
	return d->tDict;
    }
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "dict", TCL_AUTO_LENGTH);
    if (keyed) {
	Tcl_DStringAppend(&ds, "(", 1);
	for (i = 0; i < nkeys; i++) {
	    if (i > 0) {
		Tcl_DStringAppend(&ds, ",", 1);
	    }
	    Tcl_DStringAppend(&ds, "\"", 1);
	    Tcl_DStringAppend(&ds, Tcl_GetString(keys[i].name),
		    TCL_AUTO_LENGTH);
	    Tcl_DStringAppend(&ds, "\"", 1);
	    if (keys[i].optional) {
		Tcl_DStringAppend(&ds, "?", 1);
	    }
	    if (keys[i].defval != NULL) {
		Tcl_DStringAppend(&ds, "=\"", 2);
		Tcl_DStringAppend(&ds, Tcl_GetString(keys[i].defval),
			TCL_AUTO_LENGTH);
		Tcl_DStringAppend(&ds, "\"", 1);
	    }
	    if (keys[i].defcmd != NULL) {
		Tcl_DStringAppend(&ds, "=[", 2);
		Tcl_DStringAppend(&ds, Tcl_GetString(keys[i].defcmd),
			TCL_AUTO_LENGTH);
		Tcl_DStringAppend(&ds, "]", 1);
	    }
	}
	Tcl_DStringAppend(&ds, ")", 1);
    }
    Tcl_DStringAppend(&ds, "<", 1);
    Tcl_DStringAppend(&ds, elem->name, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, ">", 1);
    tp = Intern(d, &ds, TCLT_DICT, elem, &isNew);
    if (isNew && keyed) {
	tp->keyed = 1;
	tp->keys = keys;
	tp->nkeys = nkeys;
    } else if (keys != NULL) {
	TclTypesFreeKeys(keys, nkeys);	/* type déjà interné : on garde le sien */
    }
    Tcl_DStringFree(&ds);
    return tp;
}

/* var.T, var(typed).T : le nom d'une variable. */
TclType *
TclTypesMakeVar(TypeData *d, TclType *elem, int typed)
{
    Tcl_DString ds;
    TclType *tp;
    int isNew;

    if (!typed && elem == d->tString) {
	return d->tVar;
    }
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, typed ? "var(typed)<" : "var<", TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, elem->name, TCL_AUTO_LENGTH);
    Tcl_DStringAppend(&ds, ">", 1);
    tp = Intern(d, &ds, TCLT_VAR, elem, &isNew);
    if (isNew) {
	tp->varTyped = typed;
    }
    Tcl_DStringFree(&ds);
    return tp;
}

/* La clé est-elle admise par ce dictionnaire ? (Sans contrainte : oui.) */
int
TclTypeDictKeyAllowed(const TclType *tp, const char *key)
{
    Tcl_Size i;

    if (!tp->keyed) {
	return 1;
    }
    for (i = 0; i < tp->nkeys; i++) {
	if (strcmp(Tcl_GetString(tp->keys[i].name), key) == 0) {
	    return 1;
	}
    }
    return 0;
}

TclType *
TclTypeListOf(Tcl_Interp *interp, TclType *elem)
{
    /* list.T est interné une fois pour toutes (la table des types le garde
     * aussi longtemps que T) : le pointeur se garde dans T */
    if (elem->listOf == NULL) {
	elem->listOf = TclTypesMakeList(GetData(interp), elem);
    }
    return elem->listOf;
}

/* --------------------------------------------------------------------
 * Inclusion : a est inclus dans b si toute valeur valide comme a l'est
 * aussi comme b (structurel, les noms nominaux sont ignorés : il s'agit
 * de la validité des CHAÎNES). Conservateur : 0 en cas de doute.
 * -------------------------------------------------------------------- */

static int IncludesD(const TclType *a, const TclType *b, int depth);

static int
LiteralFits(Tcl_Obj *literal, const TclType *tp)
{
    return TclTypeValidate(NULL, tp, literal, -1, NULL) == TCL_OK;
}

/* Les tailles lo..hi (hi < 0 : illimité) sont-elles toutes égales à l'arité de b ?
 * (Un type qui n'a qu'une taille possible est de la forme lo == hi.) */
static int
ArityWithin(Tcl_Size lo, Tcl_Size hi, const TclType *b)
{
    if (b->arity < 0 && !b->lenRange) {
	return 1;
    }
    if (b->lenRange) {		/* v17g : list(LO:HI) */
	return lo >= b->lenMin && (b->lenMax < 0
		|| (hi >= 0 && hi <= b->lenMax));
    }
    return lo == b->arity && hi == b->arity;
}

/* Clés : tout dictionnaire admis par a l'est-il par b ? */
static int
KeysWithin(const TclType *a, const TclType *b)
{
    Tcl_Size i, j;

    if (!b->keyed) {
	return 1;
    }
    if (!a->keyed) {
	return 0;
    }
    for (i = 0; i < a->nkeys; i++) {
	if (!TclTypeDictKeyAllowed(b, Tcl_GetString(a->keys[i].name))) {
	    return 0;
	}
    }
    for (i = 0; i < b->nkeys; i++) {
	if (b->keys[i].optional) {
	    continue;
	}
	/* une clé obligatoire pour b doit l'être aussi pour a */
	for (j = 0; j < a->nkeys; j++) {
	    if (!a->keys[j].optional && strcmp(Tcl_GetString(a->keys[j].name),
		    Tcl_GetString(b->keys[i].name)) == 0) {
		break;
	    }
	}
	if (j == a->nkeys) {
	    return 0;
	}
    }
    return 1;
}

static int VariantIncludes(const TclTypeField *fa, const TclTypeField *fb);

static int
VariantIncludesType(const TclTypeField *fa, const TclType *b, int depth)
{
    if (fa->type == NULL) {
	return LiteralFits(fa->literal, b);
    }
    return IncludesD(fa->type, b, depth + 1);
}

static int
IncludesD(const TclType *a, const TclType *b, int depth)
{
    Tcl_Size i;

    if (a == b) {
	return 1;
    }
    if (depth > 16) {
	return 0;
    }
    if (b->kind == TCLT_STRING) {
	return 1;
    }
    if (a->variantOf != NULL && a->variantOf == b) {
	return 1;			/* v18b : S/v dans S */
    }
    if (a->kind == TCLT_SUM) {
	for (i = 0; i < a->nfields; i++) {
	    if (a->variantOf != NULL && i != a->variantIdx) {
		continue;		/* v18b : la seule variante de S/v */
	    }
	    if (a->variantOf != NULL && b->variantOf == a->variantOf) {
		if (b->variantIdx != i) {
		    return 0;
		}
		if (!VariantIncludes(&a->fields[i], &b->fields[i])
			&& a->fields[i].type != b->fields[i].type) {
		    return 0;
		}
		continue;
	    }
	    if (!VariantIncludesType(&a->fields[i], b, depth)) {
		return 0;
	    }
	}
	return 1;
    }
    if (b->kind == TCLT_SUM) {
	for (i = 0; i < b->nfields; i++) {
	    if (b->variantOf != NULL && i != b->variantIdx) {
		continue;
	    }
	    if (b->fields[i].type != NULL
		    && IncludesD(a, b->fields[i].type, depth + 1)) {
		return 1;
	    }
	}
	return 0;
    }
    switch (b->kind) {
    case TCLT_INT:
	return a->kind == TCLT_INT;
    case TCLT_DOUBLE:
	return a->kind == TCLT_INT || a->kind == TCLT_DOUBLE;
    case TCLT_LIST:
	switch (a->kind) {
	case TCLT_LIST:
	    return IncludesD(a->elem, b->elem, depth + 1)
		    && ArityWithin(a->lenRange ? a->lenMin : a->arity < 0 ? 0
			    : a->arity, a->lenRange ? a->lenMax : a->arity, b);
	case TCLT_INT:
	case TCLT_DOUBLE:
	    /* un scalaire est une liste d'un élément */
	    return IncludesD(a, b->elem, depth + 1) && ArityWithin(1, 1, b);
	case TCLT_DICT: {
	    /* un dictionnaire est une liste de 2n éléments */
	    Tcl_Size req = 0;

	    for (i = 0; i < a->nkeys; i++) {
		req += a->keys[i].optional ? 0 : 1;
	    }
	    return IncludesD(a->key, b->elem, depth + 1)
		    && IncludesD(a->elem, b->elem, depth + 1)
		    && ArityWithin(2 * req, a->keyed ? 2 * a->nkeys : -1, b);
	}
	case TCLT_PRODUCT:
	    for (i = 0; i < a->nfields; i++) {
		if (!IncludesD(a->fields[i].type, b->elem, depth + 1)) {
		    return 0;
		}
	    }
	    return ArityWithin(a->nfields, a->nfields, b);
	default:
	    return 0;
	}
    case TCLT_VAR:
	/* invariant : écrire dans la variable suppose le même T */
	return a->kind == TCLT_VAR && a->elem == b->elem
		&& (a->varTyped || !b->varTyped);
    case TCLT_DICT:
	return a->kind == TCLT_DICT
		&& IncludesD(a->elem, b->elem, depth + 1) && KeysWithin(a, b);
    case TCLT_PRODUCT:
	if (a->kind != TCLT_PRODUCT || a->nfields != b->nfields) {
	    return 0;
	}
	for (i = 0; i < a->nfields; i++) {
	    if (strcmp(Tcl_GetString(a->fields[i].name),
		    Tcl_GetString(b->fields[i].name)) != 0
		    || !IncludesD(a->fields[i].type, b->fields[i].type,
			    depth + 1)) {
		return 0;
	    }
	}
	return 1;
    default:
	return 0;
    }
}

int
TclTypeIncludes(const TclType *a, const TclType *b)
{
    return IncludesD(a, b, 0);
}

static int
VariantIncludes(const TclTypeField *fa, const TclTypeField *fb)
{
    if (fa->type == NULL && fb->type == NULL) {
	return strcmp(Tcl_GetString(fa->literal),
		Tcl_GetString(fb->literal)) == 0;
    }
    if (fa->type == NULL) {
	return LiteralFits(fa->literal, fb->type);
    }
    if (fb->type == NULL) {
	return 0;
    }
    return IncludesD(fa->type, fb->type, 0);
}

/* Type commun de deux types (collecte hétérogène, §10) : le plus petit
 * qui contient les deux par inclusion, à défaut string. */
TclType *
TclTypeCommon(Tcl_Interp *interp, TclType *a, TclType *b)
{
    if (a == b) {
	return a;
    }
    if (TclTypeIncludes(a, b)) {
	return b;
    }
    if (TclTypeIncludes(b, a)) {
	return a;
    }
    return GetData(interp)->tString;
}

/* --------------------------------------------------------------------
 * Validation
 * -------------------------------------------------------------------- */

/*
 * Types int(u,n), int(s,n) : l'entier tient-il sur bits bits ?
 */

static int
BinIntFits(
    mp_int *big,
    int bits,
    int isSigned)
{
    mp_int lim;
    int ok = 0;

    if (!isSigned) {
	return !mp_isneg(big) && mp_count_bits(big) <= bits;
    }
    if (!mp_isneg(big)) {
	return mp_count_bits(big) <= bits - 1;
    }
    /* négatif : |v| <= 2^(bits-1) */
    if (mp_init_u64(&lim, 1) != MP_OKAY) {
	return 0;
    }
    if (mp_mul_2d(&lim, bits - 1, &lim) == MP_OKAY) {
	ok = (mp_cmp_mag(big, &lim) != MP_GT);
    }
    mp_clear(&lim);
    return ok;
}

/*
 * Types string(enc,N), string(enc,z), string(raw,N) : contrôle d'une chaîne.
 * Le texte doit s'encoder sans perte dans l'encodage du type (ni caractère
 * inexistant, ni NUL) et tenir dans N octets pour une taille fixe.
 */

static int
BinStringValid(
    Tcl_Interp *interp,
    const TclType *tp,
    Tcl_Obj *value)
{
    Tcl_Size len, i, nchars;
    const char *s = Tcl_GetStringFromObj(value, &len);

    if (strstr(s, "\xC0\x80") != NULL && tp->binKind != TCLBIN_BYTES) {
	ERR(interp, "BIN", "value for %s cannot contain a NUL character",
		tp->name);
	return TCL_ERROR;
    }
    if (tp->binKind == TCLBIN_BYTES) {
	nchars = Tcl_GetCharLength(value);
	if (tp->binDyn == NULL && nchars != tp->binSize) {
	    ERR(interp, "BIN", "%s needs exactly %" TCL_SIZE_MODIFIER "d "
		    "bytes but got %" TCL_SIZE_MODIFIER "d", tp->name,
		    tp->binSize, nchars);
	    return TCL_ERROR;
	}
	for (i = 0; i < len; ) {
	    int ch;

	    i += Tcl_UtfToUniChar(s + i, &ch);
	    if (ch > 255) {
		ERR(interp, "BIN", "%s holds bytes: character U+%04X is "
			"above 255", tp->name, ch);
		return TCL_ERROR;
	    }
	}
    } else if (tp->binEnc != NULL) {
	Tcl_Encoding enc = Tcl_GetEncoding(NULL, tp->binEnc);
	Tcl_DString ds;
	Tcl_Size loc = 0, size;
	int rc;

	if (enc == NULL) {
	    ERR(interp, "BIN", "encoding \"%s\" is not available",
		    tp->binEnc);
	    return TCL_ERROR;
	}
	Tcl_DStringInit(&ds);
	rc = Tcl_UtfToExternalDStringEx(NULL, enc, s, len,
		TCL_ENCODING_PROFILE_STRICT, &ds, &loc);
	size = Tcl_DStringLength(&ds);
	Tcl_DStringFree(&ds);
	Tcl_FreeEncoding(enc);
	if (rc != TCL_OK) {
	    ERR(interp, "BIN", "text for %s cannot be encoded in %s (byte "
		    "%" TCL_SIZE_MODIFIER "d of its UTF-8 form)", tp->name,
		    tp->binEnc, loc);
	    return TCL_ERROR;
	}
	if (tp->binKind == TCLBIN_STR && tp->binDyn == NULL
		&& size > tp->binSize) {
	    ERR(interp, "BIN", "text for %s is %" TCL_SIZE_MODIFIER "d bytes "
		    "long in %s, the limit is %" TCL_SIZE_MODIFIER "d",
		    tp->name, size, tp->binEnc, tp->binSize);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static int
IsIntegerValue(Tcl_Obj *v)
{
    void *clientData;
    int type;

    if (Tcl_GetNumberFromObj(NULL, v, &clientData, &type) != TCL_OK) {
	return 0;
    }
    return type == TCL_NUMBER_INT || type == TCL_NUMBER_BIG;
}

/* --------------------------------------------------------------------
 * bitarray(n, ordre) : lecture et écriture de bits dans un entier
 * (positions comptées depuis le poids faible, 0 = bit de droite).
 * -------------------------------------------------------------------- */

/* Les opérations mp_* échouent seulement faute de mémoire. */
#define MPOK(call) \
    do { \
	if ((call) != MP_OKAY) { \
	    Tcl_Panic("out of memory in bitarray operation"); \
	} \
    } while (0)

/* (value >> lo) mod 2^len, entier non négatif. */
int
TclBitsGet(
    Tcl_Interp *interp,
    Tcl_Obj *value,
    Tcl_Size lo,
    Tcl_Size len,
    Tcl_Obj **resultPtr)
{
    mp_int big, q, r;
    void *cd;
    int nt;
    Tcl_Obj *content = TCL_ANNOT_CONTENT(value);

    /*
     * 23/11 : sans copie, quand on lit au plus 64 bits d'un entier >= 0 :
     * la représentation est lue sur place (Tcl_GetNumberFromObj ne copie
     * pas les chiffres). Avant : une copie du grand entier par lecture,
     * O(n) pour un bit.
     */
    if (len <= 64 && lo >= 0
	    && Tcl_GetNumberFromObj(NULL, content, &cd, &nt) == TCL_OK) {
	if (nt == TCL_NUMBER_INT) {
	    Tcl_WideInt w = *(Tcl_WideInt *) cd;

	    if (w >= 0) {
		Tcl_WideUInt u = (lo >= 64) ? 0 : ((Tcl_WideUInt) w >> lo);

		if (len < 64) {
		    u &= (((Tcl_WideUInt) 1 << len) - 1);
		}
		*resultPtr = (u > (Tcl_WideUInt) WIDE_MAX)
			? NULL : Tcl_NewWideIntObj((Tcl_WideInt) u);
		if (*resultPtr != NULL) {
		    return TCL_OK;
		}
	    }
	} else if (nt == TCL_NUMBER_BIG && !mp_isneg((mp_int *) cd)) {
	    const mp_int *bp = (const mp_int *) cd;
	    Tcl_WideUInt u = 0;
	    Tcl_Size k;

	    for (k = 0; k < len; k++) {
		Tcl_Size pos = lo + k, d = pos / MP_DIGIT_BIT;

		if (d < bp->used
			&& ((bp->dp[d] >> (pos % MP_DIGIT_BIT)) & 1)) {
		    u |= (Tcl_WideUInt) 1 << k;
		}
	    }
	    if (u <= (Tcl_WideUInt) WIDE_MAX) {
		*resultPtr = Tcl_NewWideIntObj((Tcl_WideInt) u);
		return TCL_OK;
	    }
	}
    }
    if (Tcl_GetBignumFromObj(interp, content, &big)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    MPOK(mp_init(&q));
    MPOK(mp_init(&r));
    MPOK(mp_div_2d(&big, (int) lo, &q, NULL));
    MPOK(mp_mod_2d(&q, (int) len, &r));
    mp_clear(&big);
    mp_clear(&q);
    *resultPtr = Tcl_NewBignumObj(&r);	/* r est repris */
    return TCL_OK;
}

/* value avec les len bits à partir de lo remplacés par bits (0 <= bits <
 * 2^len, sinon erreur). */
int
TclBitsPut(
    Tcl_Interp *interp,
    Tcl_Obj *value,
    Tcl_Size lo,
    Tcl_Size len,
    Tcl_Obj *bits,
    Tcl_Obj **resultPtr)
{
    mp_int big, nb, cur, tmp, res;
    int fits;

    if (Tcl_GetBignumFromObj(interp, TCL_ANNOT_CONTENT(value), &big)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetBignumFromObj(interp, TCL_ANNOT_CONTENT(bits), &nb)
	    != TCL_OK) {
	mp_clear(&big);
	return TCL_ERROR;
    }
    fits = !mp_isneg(&nb) && (Tcl_Size) mp_count_bits(&nb) <= len;
    if (!fits) {
	ERR(interp, "BITS", "value \"%s\" does not fit in %" TCL_SIZE_MODIFIER
		"d bit%s", Tcl_GetString(bits), len, (len == 1) ? "" : "s");
	mp_clear(&big);
	mp_clear(&nb);
	return TCL_ERROR;
    }
    MPOK(mp_init(&cur));
    MPOK(mp_init(&tmp));
    MPOK(mp_init(&res));
    MPOK(mp_div_2d(&big, (int) lo, &tmp, NULL));
    MPOK(mp_mod_2d(&tmp, (int) len, &cur));		/* anciens bits */
    MPOK(mp_mul_2d(&cur, (int) lo, &tmp));		/* ... à leur place */
    MPOK(mp_sub(&big, &tmp, &res));			/* effacés */
    MPOK(mp_mul_2d(&nb, (int) lo, &tmp));		/* nouveaux bits à leur place */
    MPOK(mp_add(&res, &tmp, &cur));
    mp_clear(&big);
    mp_clear(&nb);
    mp_clear(&tmp);
    mp_clear(&res);
    *resultPtr = Tcl_NewBignumObj(&cur);	/* cur est repris */
    return TCL_OK;
}

/*
 * 23/11 : écrire UN bit sur place, dans un grand entier que personne
 * d'autre ne tient (la variable d'un bitarray). WIDTHDIGITS : la taille
 * visée (les chiffres de la largeur du bitarray), pour ne pas réallouer à
 * chaque bit plus haut. Rend 0 si ce n'est pas possible (l'appelant prend
 * alors le chemin ordinaire, qui copie).
 */
int
TclBitsSetInPlace(
    Tcl_Obj *v,
    Tcl_Size lo,
    int bit,
    Tcl_Size widthDigits)
{
    mp_int big, *bp;
    int payload;
    Tcl_Size d, k;

    if (v->typePtr != &tclBignumType || Tcl_IsShared(v) || lo < 0) {
	return 0;
    }
    payload = (int) PTR2INT(v->internalRep.twoPtrValue.ptr2);
    if (payload == -1) {
	bp = (mp_int *) v->internalRep.twoPtrValue.ptr1;
    } else {
	TclUnpackBignum(v, big);
	bp = &big;
    }
    if (mp_isneg(bp)) {
	return 0;
    }
    d = lo / MP_DIGIT_BIT;
    if (bit) {
	if (d >= bp->alloc) {
	    Tcl_Size want = d + 1;

	    if (want < 2 * (Tcl_Size) bp->alloc) {
		want = 2 * (Tcl_Size) bp->alloc;
	    }
	    if (widthDigits > d && want > widthDigits) {
		want = widthDigits;
	    }
	    if (want > INT_MAX || mp_grow(bp, (int) want) != MP_OKAY) {
		return 0;
	    }
	}
	for (k = bp->used; k <= d; k++) {
	    bp->dp[k] = 0;
	}
	if (d >= bp->used) {
	    bp->used = (int) d + 1;
	}
	bp->dp[d] |= (mp_digit) 1 << (lo % MP_DIGIT_BIT);
	bp->sign = MP_ZPOS;
    } else if (d < bp->used) {
	bp->dp[d] &= ~((mp_digit) 1 << (lo % MP_DIGIT_BIT));
	while (bp->used > 0 && bp->dp[bp->used - 1] == 0) {
	    bp->used--;
	}
	if (bp->used == 0) {
	    bp->sign = MP_ZPOS;
	}
    }
    if (payload != -1) {
	/* la forme rangée dans l'objet (chiffres, taille) a pu changer */
	TclSetBignumInternalRep(v, bp);
    }
    TclInvalidateStringRep(v);
    return 1;
}

/*
 * 23/11 : les bits lo, lo+step, ... (count bits ; step peut être négatif,
 * pour msb0) mis à BIT, sur place, comme TclBitsSetInPlace. 0 si ce n'est
 * pas possible (rien n'a alors été écrit).
 */
int
TclBitsFillStride(
    Tcl_Obj *v,
    Tcl_Size lo,
    Tcl_Size step,
    Tcl_Size count,
    int bit,
    Tcl_Size widthDigits)
{
    mp_int big, *bp;
    int payload;
    Tcl_Size k, pos, top, bot, d;

    if (v->typePtr != &tclBignumType || Tcl_IsShared(v) || count <= 0) {
	return count == 0 && v->typePtr == &tclBignumType;
    }
    top = (step >= 0) ? lo + (count - 1) * step : lo;
    bot = (step >= 0) ? lo : lo + (count - 1) * step;
    if (bot < 0) {
	return 0;
    }
    payload = (int) PTR2INT(v->internalRep.twoPtrValue.ptr2);
    if (payload == -1) {
	bp = (mp_int *) v->internalRep.twoPtrValue.ptr1;
    } else {
	TclUnpackBignum(v, big);
	bp = &big;
    }
    if (mp_isneg(bp)) {
	return 0;
    }
    if (bit) {
	d = top / MP_DIGIT_BIT;
	if (d >= bp->alloc) {
	    Tcl_Size want = d + 1;

	    if (widthDigits > want) {
		want = widthDigits;
	    }
	    if (want > INT_MAX || mp_grow(bp, (int) want) != MP_OKAY) {
		return 0;
	    }
	}
	for (k = bp->used; k <= d; k++) {
	    bp->dp[k] = 0;
	}
	if (d >= bp->used) {
	    bp->used = (int) d + 1;
	}
	for (k = 0, pos = lo; k < count; k++, pos += step) {
	    bp->dp[pos / MP_DIGIT_BIT] |= (mp_digit) 1 << (pos % MP_DIGIT_BIT);
	}
	bp->sign = MP_ZPOS;
    } else {
	for (k = 0, pos = lo; k < count; k++, pos += step) {
	    if (pos / MP_DIGIT_BIT < bp->used) {
		bp->dp[pos / MP_DIGIT_BIT]
			&= ~((mp_digit) 1 << (pos % MP_DIGIT_BIT));
	    }
	}
	while (bp->used > 0 && bp->dp[bp->used - 1] == 0) {
	    bp->used--;
	}
	if (bp->used == 0) {
	    bp->sign = MP_ZPOS;
	}
    }
    if (payload != -1) {
	TclSetBignumInternalRep(v, bp);
    }
    TclInvalidateStringRep(v);
    return 1;
}

/*
 * 23/11 : les bits lo, lo+step, ... (count bits) recueillis en un entier :
 * le k-ième va en position k, ou count-1-k si REVERSE (msb0).
 */
int
TclBitsGather(
    Tcl_Interp *interp,
    Tcl_Obj *value,
    Tcl_Size lo,
    Tcl_Size step,
    Tcl_Size count,
    int reverse,
    Tcl_Obj **resultPtr)
{
    mp_int big, res;
    Tcl_Size k, pos, q;

    if (Tcl_GetBignumFromObj(interp, TCL_ANNOT_CONTENT(value), &big)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (mp_isneg(&big) || count > (Tcl_Size) INT_MAX - MP_DIGIT_BIT
	    || mp_init_size(&res, (int) (count / MP_DIGIT_BIT + 1))
	    != MP_OKAY) {
	mp_clear(&big);
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "bits of a negative or too large value", -1));
	}
	return TCL_ERROR;
    }
    res.used = (int) (count / MP_DIGIT_BIT + 1);
    for (k = 0; k < res.used; k++) {
	res.dp[k] = 0;
    }
    for (k = 0, pos = lo; k < count; k++, pos += step) {
	if (pos >= 0 && pos / MP_DIGIT_BIT < big.used
		&& (big.dp[pos / MP_DIGIT_BIT] >> (pos % MP_DIGIT_BIT)) & 1) {
	    q = reverse ? count - 1 - k : k;
	    res.dp[q / MP_DIGIT_BIT] |= (mp_digit) 1 << (q % MP_DIGIT_BIT);
	}
    }
    mp_clamp(&res);
    mp_clear(&big);
    *resultPtr = Tcl_NewBignumObj(&res);
    return TCL_OK;
}

/*
 * 23/11 : écrire les bits lo, lo+step, ... (count) de VALUE : tous à FILL
 * (PATTERN NULL), ou le k-ième au bit k de PATTERN (count-1-k si REVERSE).
 * *resultPtr : un entier neuf.
 */
int
TclBitsPutStride(
    Tcl_Interp *interp,
    Tcl_Obj *value,
    Tcl_Size lo,
    Tcl_Size step,
    Tcl_Size count,
    int reverse,
    Tcl_Obj *pattern,
    int fill,
    Tcl_Size widthDigits,
    Tcl_Obj **resultPtr)
{
    Tcl_Obj *nv = TclBitsNewWorkObj(value, widthDigits);
    mp_int pat;
    Tcl_Size k, q;

    if (nv == NULL) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "bits of a negative or too large value", -1));
	}
	return TCL_ERROR;
    }
    if (pattern == NULL) {
	TclBitsFillStride(nv, lo, step, count, fill, widthDigits);
	*resultPtr = nv;
	return TCL_OK;
    }
    if (Tcl_GetBignumFromObj(interp, TCL_ANNOT_CONTENT(pattern), &pat)
	    != TCL_OK) {
	Tcl_DecrRefCount(nv);
	return TCL_ERROR;
    }
    if (mp_isneg(&pat)) {
	mp_clear(&pat);
	Tcl_DecrRefCount(nv);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"bits of a negative value", -1));
	return TCL_ERROR;
    }
    for (k = 0; k < count; k++) {
	int bit;

	q = reverse ? count - 1 - k : k;
	bit = q / MP_DIGIT_BIT < pat.used
		&& ((pat.dp[q / MP_DIGIT_BIT] >> (q % MP_DIGIT_BIT)) & 1);
	TclBitsSetInPlace(nv, lo + k * step, bit, widthDigits);
    }
    mp_clear(&pat);
    *resultPtr = nv;
    return TCL_OK;
}

/*
 * 23/11 : un contenu neuf pour l'écriture sur place : un grand entier (même
 * petit : il ne sera pas ramené à un entier machine), à la taille visée.
 * NULL si la valeur n'est pas un entier >= 0.
 */
Tcl_Obj *
TclBitsNewWorkObj(
    Tcl_Obj *value,
    Tcl_Size widthDigits)
{
    mp_int big;
    Tcl_Obj *o;

    if (Tcl_GetBignumFromObj(NULL, TCL_ANNOT_CONTENT(value), &big)
	    != TCL_OK) {
	return NULL;
    }
    if (mp_isneg(&big) || widthDigits > 0x7FFF
	    || (widthDigits > big.alloc
		&& mp_grow(&big, (int) widthDigits) != MP_OKAY)) {
	mp_clear(&big);
	return NULL;
    }
    o = Tcl_NewObj();
    TclInvalidateStringRep(o);
    TclSetBignumInternalRep(o, &big);
    return o;
}

/* popcount(x) : nombre de bits à 1 d'un entier >= 0. */
static int
PopcountCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const objv[])
{
    mp_int big, q;
    Tcl_WideInt count = 0;
    void *cd;
    int nt;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "value");
	return TCL_ERROR;
    }
    /* 23/11 : sans copie ni divisions (avant : O(n²) pour n bits) */
    if (Tcl_GetNumberFromObj(NULL, TCL_ANNOT_CONTENT(objv[1]), &cd, &nt)
	    == TCL_OK && (nt == TCL_NUMBER_INT || nt == TCL_NUMBER_BIG)) {
	if (nt == TCL_NUMBER_INT && *(Tcl_WideInt *) cd >= 0) {
	    Tcl_WideUInt w = (Tcl_WideUInt) *(Tcl_WideInt *) cd;

	    while (w != 0) {
		w &= w - 1;
		count++;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(count));
	    return TCL_OK;
	}
	if (nt == TCL_NUMBER_BIG && !mp_isneg((mp_int *) cd)) {
	    const mp_int *bp = (const mp_int *) cd;
	    int k;

	    for (k = 0; k < bp->used; k++) {
		mp_digit w = bp->dp[k];

		while (w != 0) {
		    w &= w - 1;
		    count++;
		}
	    }
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(count));
	    return TCL_OK;
	}
    }
    if (Tcl_GetBignumFromObj(interp, TCL_ANNOT_CONTENT(objv[1]), &big)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (mp_isneg(&big)) {
	mp_clear(&big);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"popcount needs a non-negative integer", TCL_AUTO_LENGTH));
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "BITS", (char *) NULL);
	return TCL_ERROR;
    }
    MPOK(mp_init(&q));
    while (!mp_iszero(&big)) {
	mp_int r;
	Tcl_WideUInt w;

	MPOK(mp_init(&r));
	MPOK(mp_mod_2d(&big, 64, &r));
	w = mp_get_mag_u64(&r);
	mp_clear(&r);
	while (w != 0) {
	    w &= w - 1;
	    count++;
	}
	MPOK(mp_div_2d(&big, 64, &q, NULL));
	mp_exch(&big, &q);
    }
    mp_clear(&big);
    mp_clear(&q);
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(count));
    return TCL_OK;
}

/* bitlen(x) : largeur d'un bitarray, sinon nombre de bits de |x|. */
static int
BitlenCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const objv[])
{
    mp_int big;
    TclType *tp = NULL;
    Tcl_WideInt n;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "value");
	return TCL_ERROR;
    }
    if (TclAnnotInfo(objv[1], NULL, &tp, NULL, NULL) == 1
	    && tp->bitWidth > 0) {
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(tp->bitWidth));
	return TCL_OK;
    }
    if (Tcl_GetBignumFromObj(interp, TCL_ANNOT_CONTENT(objv[1]), &big)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    n = mp_count_bits(&big);
    mp_clear(&big);
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(n));
    return TCL_OK;
}

/*
 * Garde contre la régression infinie : avec un type récursif comme
 * {int}leaf, {list.tree}node, la valeur "x" est une liste d'UN élément, qui
 * est "x" lui-même, et la validation redescendrait sans fin. Si le même
 * (type, texte) est déjà en cours de validation plus haut, la valeur n'a pas
 * de décomposition finie : on la refuse.
 */

typedef struct ValGuard {
    const TclType *tp;
    Tcl_Obj *value;
    const struct ValGuard *up;
} ValGuard;

static int ValidateG(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *value,
	int variantHint, int *variantOut, const ValGuard *up);
static int PredCheck(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *value);

static int
VariantAccepts(const TclTypeField *f, Tcl_Obj *v, const ValGuard *up)
{
    if (f->type == NULL) {
	return strcmp(Tcl_GetString(f->literal), Tcl_GetString(v)) == 0;
    }
    return ValidateG(NULL, f->type, v, -1, NULL, up) == TCL_OK;
}

/*
 * Valide la valeur "value" (contenu, jamais une valeur annotée) contre tp.
 * Somme : variantHint >= 0 impose la variante ; sinon la première variante
 * qui accepte est retenue, et c'est une erreur si une variante suivante
 * qui accepte aussi n'inclut pas la première (balise nécessaire).
 */
static int CanonDisjoint(const TclType *a, const TclType *b);	/* v17b */

int
TclTypeValidate(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *value,
	int variantHint, int *variantOut)
{
    return ValidateG(interp, tp, value, variantHint, variantOut, NULL);
}

/* Variante d'une partie rangée (annotée, ou déduite de son contenu), ou -1. */
int
TclTypeSumCurrent(const TclType *tp, Tcl_Obj *part)
{
    TclType *ownTp = NULL;
    int own = -1, got = -1;

    if (tp == NULL || tp->kind != TCLT_SUM || part == NULL) {
	return -1;
    }
    if (TclAnnotInfo(part, NULL, &ownTp, &own, NULL) == 1 && ownTp == tp) {
	return own;
    }
    if (ValidateG(NULL, tp, TCL_ANNOT_CONTENT(part), -1, &got, NULL)
	    != TCL_OK) {
	return -1;
    }
    return got;
}

/*
 * Somme : le contenu seul (sans balise) redonne-t-il la variante v ? Non
 * quand plusieurs variantes également strictes l'acceptent, ou quand une
 * balise a choisi une variante moins stricte que celle qu'on déduirait.
 * Une partie de liste ou de produit garde alors son annotation.
 */
int
TclTypeSumNeedsTag(const TclType *tp, Tcl_Obj *content, int v)
{
    int got = -1;

    if (tp == NULL || tp->kind != TCLT_SUM || v < 0
	    || (tp->binTag != NULL && tp->binTag[0] == '=') || TclSumTagged(tp)) {
	return 0;			/* v16w : la valeur dit sa variante */
    }
    if (ValidateG(NULL, tp, TCL_ANNOT_CONTENT(content), -1, &got, NULL)
	    != TCL_OK) {
	return 1;
    }
    return got != v;
}

/*
 * v16w : la valeur V d'une somme balisée (",") écrit sa variante. Une
 * variante littérale : V est le littéral. Une variante typée : V est la
 * liste {nom charge}. TclSumDecode rend la variante (et la charge, empruntée
 * à V, NULL pour un littéral), ou -1 si V n'a pas cette forme.
 */
int
TclSumDecode(const TclType *tp, Tcl_Obj *v, Tcl_Obj **payloadPtr)
{
    Tcl_Size i, n, len;
    Tcl_Obj **ev;
    const char *str, *nm;

    if (payloadPtr != NULL) {
	*payloadPtr = NULL;
    }
    v = TCL_ANNOT_CONTENT(v);
    str = Tcl_GetStringFromObj(v, &len);
    for (i = 0; i < tp->nfields; i++) {
	if (tp->fields[i].type == NULL && tp->fields[i].literal != NULL
		&& strcmp(Tcl_GetString(tp->fields[i].literal), str) == 0) {
	    return (int) i;
	}
    }
    if (len == 0 || TclListObjGetElements(NULL, v, &n, &ev) != TCL_OK
	    || n != 2) {
	return -1;
    }
    nm = Tcl_GetString(ev[0]);
    for (i = 0; i < tp->nfields; i++) {
	if (tp->fields[i].type != NULL && tp->fields[i].name != NULL
		&& strcmp(Tcl_GetString(tp->fields[i].name), nm) == 0) {
	    if (payloadPtr != NULL) {
		*payloadPtr = ev[1];
	    }
	    return (int) i;
	}
    }
    return -1;
}

/* La valeur V (neuve, refcount 0) de la variante v d'une somme balisée, de
 * charge payload (ignorée pour un littéral). */
Tcl_Obj *
TclSumMake(const TclType *tp, int v, Tcl_Obj *payload)
{
    Tcl_Obj *pair[2];

    if (tp->fields[v].type == NULL) {
	return Tcl_DuplicateObj(tp->fields[v].literal);
    }
    pair[0] = tp->fields[v].name;
    pair[1] = (payload != NULL) ? payload : Tcl_NewObj();
    return Tcl_NewListObj(2, pair);
}

/* Le message d'une valeur qui n'est pas de la forme d'une somme balisée. */
static void
SumFormError(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *value)
{
    Tcl_Size i;
    Tcl_Obj *n0 = NULL;

    if (interp == NULL) {
	return;
    }
    /* l'exemple : la première variante qui accepterait cette valeur */
    for (i = 0; i < tp->nfields && n0 == NULL; i++) {
	if (tp->fields[i].type != NULL && tp->fields[i].name != NULL
		&& TclTypeValidate(NULL, tp->fields[i].type, value, -1, NULL)
		== TCL_OK) {
	    n0 = tp->fields[i].name;
	}
    }
    for (i = 0; i < tp->nfields && n0 == NULL; i++) {
	if (tp->fields[i].type != NULL) {
	    n0 = tp->fields[i].name;
	}
    }
    if (n0 != NULL) {
	Tcl_Obj *pair[2], *ex, *one, *vc;

	/* une copie : value peut n'avoir aucune référence, et les listes la
	 * libéreraient en partant */
	vc = Tcl_DuplicateObj(value);
	pair[0] = n0;
	pair[1] = vc;
	ex = Tcl_NewListObj(2, pair);
	one = Tcl_NewListObj(1, &vc);
	Tcl_IncrRefCount(ex);
	Tcl_IncrRefCount(one);
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("value \"%s\" names no variant "
		"of the sum %s: a value of a sum is the name of its variant, "
		"then its value, as {%s}, or {/%s}%s",
		Tcl_GetString(value), tp->name, Tcl_GetString(ex),
		Tcl_GetString(n0), Tcl_GetString(one)));
	Tcl_DecrRefCount(ex);
	Tcl_DecrRefCount(one);
    } else {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("value \"%s\" matches no "
		"variant of %s", Tcl_GetString(value), tp->name));
    }
    Tcl_SetErrorCode(interp, "TCL", "TYPE", "SUM", (char *) NULL);
}

static int
ValidateG(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *value,
	int variantHint, int *variantOut, const ValGuard *up)
{
    Tcl_Obj *sumPayload = NULL;
    Tcl_Size i, n;
    Tcl_Obj **elems;
    double dbl;
    ValGuard me;
    const ValGuard *g;

    if (tp->binLink != NULL && tp->binLink->ref != NULL) {
	/* Un lien de frontière : l'adresse de la cible, pas la cible. */

	TclType *refTp = NULL;

	if (TclAnnotInfo(value, NULL, &refTp, NULL, NULL) == 1
		&& refTp == tp->binLink->ref) {
	    if (variantOut != NULL) {
		*variantOut = -1;
	    }
	    return TCL_OK;
	}
    }
    if (variantHint < 0 && tp->kind == TCLT_SUM) {
	/* Un élément déjà balisé ({tEXt}...) garde sa variante. */
	TclType *ownTp = NULL;
	int ownVariant = -1;

	if (TclAnnotInfo(value, NULL, &ownTp, &ownVariant, NULL) == 1
		&& ownTp != NULL && ownVariant >= 0
		&& TclTypeSumRoot(ownTp) == TclTypeSumRoot(tp)) {
	    variantHint = ownVariant;	/* v18b : S/v compte comme S */
	}
    }
    if (tp->kind == TCLT_SUM && value->typePtr != NULL) {
	/* v18b : la charge d'une somme peut être une valeur d'une autre somme
	 * (mesure/some/celsius) : son annotation lui appartient, elle est
	 * gardée pour la validation de la variante */
	TclType *ownTp = NULL;

	if (TclAnnotInfo(value, NULL, &ownTp, NULL, NULL) == 1
		&& ownTp != NULL && ownTp->kind == TCLT_SUM
		&& TclTypeSumRoot(ownTp) != TclTypeSumRoot(tp)) {
	    sumPayload = value;
	}
    }
    value = TCL_ANNOT_CONTENT(value);	/* une valeur annotée : son contenu */
    if (sumPayload != NULL) {
	value = sumPayload;
    }
    if (tp->pred != NULL && tp->predBase != NULL) {
	/* v17u : le type restreint, puis le prédicat */
	if (ValidateG(interp, tp->predBase, value, variantHint, variantOut,
		up) != TCL_OK) {
	    return TCL_ERROR;
	}
	return PredCheck(interp, tp, value);
    }
    me.tp = tp;
    me.value = value;
    me.up = up;
    if (tp->kind != TCLT_INT && tp->kind != TCLT_DOUBLE
	    && tp->kind != TCLT_STRING) {
	for (g = up; g != NULL; g = g->up) {
	    if (g->tp == tp && (g->value == value
		    || strcmp(Tcl_GetString(g->value),
			    Tcl_GetString(value)) == 0)) {
		if (variantOut != NULL) {
		    *variantOut = -1;
		}
		ERR(interp, "RECURSIVE", "value \"%s\" has no finite "
			"decomposition as %s", Tcl_GetString(value), tp->name);
		return TCL_ERROR;
	    }
	}
    }

    if (variantOut != NULL) {
	*variantOut = -1;
    }
    switch (tp->kind) {
    case TCLT_STRING:
	if (tp->binKind != TCLBIN_NONE) {
	    return BinStringValid(interp, tp, value);
	}
	return TCL_OK;
    case TCLT_INT:
	if (!IsIntegerValue(value)) {
	    ERR(interp, "INT", "expected integer but got \"%s\"",
		    Tcl_GetString(value));
	    return TCL_ERROR;
	}
	if (tp->bitWidth > 0) {
	    mp_int big;
	    int fits;

	    if (Tcl_GetBignumFromObj(NULL, value, &big) != TCL_OK) {
		return TCL_ERROR;
	    }
	    fits = !mp_isneg(&big)
		    && (Tcl_Size) mp_count_bits(&big) <= tp->bitWidth;
	    mp_clear(&big);
	    if (!fits) {
		ERR(interp, "BITS", "value \"%s\" does not fit in %s (%"
			TCL_SIZE_MODIFIER "d bits)", Tcl_GetString(value),
			tp->name, tp->bitWidth);
		return TCL_ERROR;
	    }
	}
	if (tp->binKind == TCLBIN_UINT || tp->binKind == TCLBIN_SINT) {
	    mp_int big;
	    int fits;

	    if (Tcl_GetBignumFromObj(NULL, value, &big) != TCL_OK) {
		return TCL_ERROR;
	    }
	    fits = BinIntFits(&big, (int) (tp->binSize * 8),
		    tp->binKind == TCLBIN_SINT);
	    mp_clear(&big);
	    if (!fits) {
		ERR(interp, "BIN", "value \"%s\" does not fit in %s",
			Tcl_GetString(value), tp->name);
		return TCL_ERROR;
	    }
	}
	return TCL_OK;
    case TCLT_DOUBLE:
	if (Tcl_GetDoubleFromObj(NULL, value, &dbl) != TCL_OK) {
	    ERR(interp, "DOUBLE", "expected floating-point number but got "
		    "\"%s\"", Tcl_GetString(value));
	    return TCL_ERROR;
	}
	return TCL_OK;
    case TCLT_LIST:
	if (TclCompactHolds(value, tp->elem, &n)) {
	    /* forme compacte : ses cases sont valides par construction */
	    return TclTypeListLenCheck(interp, tp, n);
	}
	if (Tcl_ListObjGetElements(interp, value, &n, &elems) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (TclTypeListLenCheck(interp, tp, n) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (tp->elem->kind != TCLT_STRING || tp->elem->nominal) {
	    for (i = 0; i < n; i++) {
		/* 24/11 : le même objet que le précédent (lrepeat) : déjà
		 * vérifié, la vérification ne dépend que de la valeur */
		if (i > 0 && elems[i] == elems[i - 1]) {
		    continue;
		}
		if (ValidateG(interp, tp->elem, elems[i], -1, NULL, &me)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	}
	return TCL_OK;
    case TCLT_DICT: {
	Tcl_DictSearch search;
	Tcl_Obj *k, *v;
	int done, rc = TCL_OK;

	if (Tcl_DictObjFirst(interp, value, &search, &k, &v, &done)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
	    if (!TclTypeDictKeyAllowed(tp, Tcl_GetString(k))) {
		ERR(interp, "DICT", "key \"%s\" is not allowed in %s",
			Tcl_GetString(k), tp->name);
		rc = TCL_ERROR;
		break;
	    }
	    if (ValidateG(interp, tp->elem, v, -1, NULL, &me) != TCL_OK) {
		rc = TCL_ERROR;
		break;
	    }
	}
	Tcl_DictObjDone(&search);
	if (rc == TCL_OK && tp->keyed) {
	    for (i = 0; i < tp->nkeys; i++) {
		Tcl_Obj *got = NULL;

		if (tp->keys[i].optional) {
		    continue;
		}
		if (Tcl_DictObjGet(NULL, value, tp->keys[i].name, &got)
			!= TCL_OK || got == NULL) {
		    ERR(interp, "DICT", "key \"%s\" is missing in %s",
			    Tcl_GetString(tp->keys[i].name), tp->name);
		    return TCL_ERROR;
		}
	    }
	}
	return rc;
    }
    case TCLT_VAR:
	/* Syntaxe seulement : la variable peut ne pas exister encore. */
	if (Tcl_GetString(value)[0] == '\0') {
	    ERR(interp, "VAR", "expected a variable name but got an empty "
		    "string");
	    return TCL_ERROR;
	}
	return TCL_OK;
    case TCLT_PRODUCT:
	if (Tcl_ListObjGetElements(interp, value, &n, &elems) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (n != tp->nfields) {
	    ERR(interp, "PRODUCT", "%s needs %" TCL_SIZE_MODIFIER "d values "
		    "but got %" TCL_SIZE_MODIFIER "d", tp->name,
		    tp->nfields, n);
	    return TCL_ERROR;
	}
	for (i = 0; i < n; i++) {
	    /* S($champ) : la variante est celle que nomme le champ frère. */
	    if (ValidateG(interp, tp->fields[i].type, elems[i],
		    TclTypeTagVariantFrom(tp, i, elems, n), NULL, &me)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	return TCL_OK;
    case TCLT_SUM: {
	int first = -1, prefer = -1;

	if (tp->variantOf != NULL) {
	    /* v18b : S/v, la seule variante permise */
	    if (variantHint >= 0 && variantHint != (int) tp->variantIdx) {
		ERR(interp, "VARIANT", "value \"%s\" is %s/%s, not %s",
			Tcl_GetString(value), tp->variantOf->name,
			variantHint < tp->nfields
			? MemberName(tp, variantHint) : "?", tp->name);
		return TCL_ERROR;
	    }
	    variantHint = (int) tp->variantIdx;
	}

	if (TclSumTagged(tp)) {
	    /* v16w : la valeur écrit sa variante ; rien n'est deviné */
	    Tcl_Obj *pl = NULL;
	    int vi = TclSumDecode(tp, value, &pl);

	    if (vi < 0) {
		SumFormError(interp, tp, value);
		return TCL_ERROR;
	    }
	    if (variantHint >= 0 && vi != variantHint) {
		ERR(interp, "SUM", "value \"%s\" is variant %s of %s, not %s",
			Tcl_GetString(value), tp->fields[vi].name != NULL
			? Tcl_GetString(tp->fields[vi].name)
			: Tcl_GetString(tp->fields[vi].literal), tp->name,
			variantHint < tp->nfields
			&& tp->fields[variantHint].name != NULL
			? Tcl_GetString(tp->fields[variantHint].name) : "?");
		return TCL_ERROR;
	    }
	    if (pl != NULL) {
		Tcl_IncrRefCount(pl);
		if (ValidateG(interp, tp->fields[vi].type, pl, -1, NULL, &me)
			!= TCL_OK) {
		    Tcl_DecrRefCount(pl);
		    if (interp != NULL) {
			Tcl_Obj *m = Tcl_GetObjResult(interp);

			Tcl_IncrRefCount(m);
			Tcl_SetObjResult(interp, Tcl_ObjPrintf("variant %s of "
				"%s: %s", Tcl_GetString(tp->fields[vi].name),
				tp->name, Tcl_GetString(m)));
			Tcl_DecrRefCount(m);
		    }
		    return TCL_ERROR;
		}
		Tcl_DecrRefCount(pl);
	    }
	    if (variantOut != NULL) {
		*variantOut = vi;
	    }
	    return TCL_OK;
	}
	if (variantHint <= -2) {
	    /* -2 - v : la variante courante, qui départage des variantes
	     * également strictes (écriture dans une variable typée) ; v17z :
	     * plus pour une somme, où la variante n'est jamais collante (une
	     * égalité est une erreur, l'étiquette la lève) */
	    prefer = TclSumStrict(tp) ? -1 : -2 - variantHint;
	    variantHint = -1;
	}

	if (variantHint < 0 && tp->binTag != NULL && tp->binTag[0] == '=') {
	    /* S(nom) : la variante est CHOISIE, il n'y a rien à deviner */

	    for (i = 0; i < tp->nfields; i++) {
		if (strcmp(Tcl_GetString(tp->fields[i].name),
			tp->binTag + 1) == 0) {
		    variantHint = (int) i;
		    break;
		}
	    }
	}
	if (variantHint >= 0) {
	    if (variantHint >= tp->nfields
		    || !VariantAccepts(&tp->fields[variantHint], value, &me)) {
		if (variantHint < tp->nfields && tp->binTag == NULL
			&& tp->fields[variantHint].type != NULL
			&& TclTypeValidate(interp, tp->fields[variantHint].type,
			value, -1, NULL) != TCL_OK) {
		    return TCL_ERROR;	/* v17z : le message du type */
		}
		if (tp->variantOf != NULL) {
		    ERR(interp, "SUM", "value \"%s\" is not a %s",
			    Tcl_GetString(value), tp->name);
		} else {
		    ERR(interp, "SUM", "value \"%s\" is not valid for that "
			    "variant of %s", Tcl_GetString(value), tp->name);
		}
		return TCL_ERROR;
	    }
	    if (variantOut != NULL) {
		*variantOut = variantHint;
	    }
	    return TCL_OK;
	}
	if (tp->sumMode == TCLT_SUM_UNION && tp->nfields == 2
		&& tp->fields[0].type != NULL && tp->fields[1].type != NULL
		&& CanonDisjoint(tp->fields[0].type, tp->fields[1].type)) {
	    /*
	     * v17b : une union entier | double (le nombre de Tcl) : une seule
	     * lecture du nombre dit l'alternative (entier, grand entier :
	     * l'entier ; double : le double), au lieu d'essayer les deux.
	     * C'est le choix de la plus stricte, sans la recherche.
	     */
	    int ii = tp->fields[0].type->kind == TCLT_INT ? 0 : 1;
	    void *cd;
	    int nt, pick = -1;

	    if (Tcl_GetNumberFromObj(NULL, value, &cd, &nt) == TCL_OK) {
		pick = (nt == TCL_NUMBER_DOUBLE) ? 1 - ii
			: (nt == TCL_NUMBER_INT || nt == TCL_NUMBER_BIG) ? ii : -1;
	    }
	    if (pick >= 0) {
		const TclType *pt = tp->fields[pick].type;

		/* un int ou un double ordinaires : la lecture du nombre suffit ;
		 * un raffinement (int(u,8), double(32)...) se vérifie */
		if (!(pt->refine == 0 && pt->binKind == TCLBIN_NONE
			&& pt->bitWidth == 0 && pt->under == NULL)
			&& !VariantAccepts(&tp->fields[pick], value, &me)) {
		    pick = -1;
		}
	    }
	    if (pick >= 0) {
		if (variantOut != NULL) {
		    *variantOut = pick;
		}
		return TCL_OK;
	    }
	}
	{
	    /*
	     * Déduction par la plus stricte : parmi les variantes qui
	     * acceptent la valeur, on garde celles qu'aucune autre ne bat
	     * (strictement plus spécifique). Une seule : c'est elle. Plusieurs
	     * (même forme, ou formes incomparables) : erreur qui propose
	     * l'étiquette.
	     */
	    int ok[64], *acc = ok;
	    Tcl_Size nacc = 0, a, b, nmin = 0;

	    if (tp->nfields > 64) {
		acc = (int *) Tcl_Alloc(tp->nfields * sizeof(int));
	    }
	    for (i = 0; i < tp->nfields; i++) {
		if (VariantAccepts(&tp->fields[i], value, &me)) {
		    acc[nacc++] = (int) i;
		}
	    }
	    if (TclSumStrict(tp)) {
		/*
		 * v17z : une somme ne départage pas par la plus stricte (la
		 * leçon de la v16n : une somme dont une variante accepte tout,
		 * texte, liste, accepterait n'importe quoi en silence). Un
		 * littéral, une seule valeur, l'emporte ; sinon une seule
		 * variante doit accepter la valeur, l'étiquette choisit.
		 */
		for (a = 0; a < nacc; a++) {
		    if (tp->fields[acc[a]].type == NULL) {
			acc[0] = acc[a];
			nacc = 1;
			break;
		    }
		}
		nmin = nacc;
		goto chosen;
	    }
	    for (a = 0; a < nacc; a++) {
		int beaten = 0;

		for (b = 0; b < nacc && !beaten; b++) {
		    beaten = (b != a
			    && VariantIncludes(&tp->fields[acc[b]],
				    &tp->fields[acc[a]])
			    && !VariantIncludes(&tp->fields[acc[a]],
				    &tp->fields[acc[b]]));
		}
		if (!beaten) {
		    acc[nmin++] = acc[a];	/* a >= nmin : sans risque */
		}
	    }
	    for (a = 0; nmin > 1 && a < nmin; a++) {
		if (acc[a] == prefer) {
		    acc[0] = prefer;
		    nmin = 1;
		}
	    }
	  chosen:
	    if (nmin == 1) {
		first = acc[0];
	    } else if (nmin > 1) {
		if (interp != NULL) {
		    /* une copie du texte : value peut n'avoir aucune référence,
		     * et la liste q la libérerait en partant */
		    Tcl_Obj *vs = Tcl_NewStringObj(Tcl_GetString(value), -1);
		    Tcl_Obj *msg = Tcl_NewObj(), *q = Tcl_NewListObj(1, &vs);
		    Tcl_Obj *n0 = tp->fields[acc[0]].name;

		    Tcl_IncrRefCount(q);
		    Tcl_AppendPrintfToObj(msg, "value \"%s\" fits variants ",
			    Tcl_GetString(value));
		    for (a = 0; a < nmin; a++) {
			Tcl_Obj *nm = tp->fields[acc[a]].name;

			Tcl_AppendToObj(msg, a == 0 ? "" :
				(a == nmin - 1 ? " and " : ", "), -1);
			if (nm != NULL) {
			    Tcl_AppendObjToObj(msg, nm);
			} else {
			    /* variante anonyme : son rang et sa forme */
			    Tcl_AppendPrintfToObj(msg, "#%d (%s)", acc[a] + 1,
				    tp->fields[acc[a]].type != NULL
				    ? tp->fields[acc[a]].type->name
				    : Tcl_GetString(tp->fields[acc[a]].literal));
			}
		    }
		    if (n0 != NULL) {
			const char *v0 = Tcl_GetString(n0);

			Tcl_AppendPrintfToObj(msg, " of %s%s: write {/%s}%s",
				tp->name, TclSumStrict(tp) ? "" : " equally", v0,
				Tcl_GetString(q));
		    } else {
			Tcl_AppendPrintfToObj(msg, " of %s equally: name the "
				"variants to choose one", tp->name);
		    }
		    Tcl_DecrRefCount(q);
		    Tcl_SetObjResult(interp, msg);
		    Tcl_SetErrorCode(interp, "TCL", "TYPE", "AMBIGUOUS",
			    tp->name, n0 != NULL ? Tcl_GetString(n0) : "",
			    (char *) NULL);
		}
		if (acc != ok) {
		    Tcl_Free(acc);
		}
		return TCL_ERROR;
	    }
	    if (acc != ok) {
		Tcl_Free(acc);
	    }
	}
	if (first < 0) {
	    ERR(interp, "SUM", "value \"%s\" matches no variant of %s",
		    Tcl_GetString(value), tp->name);
	    return TCL_ERROR;
	}
	if (variantOut != NULL) {
	    *variantOut = first;
	}
	return TCL_OK;
    }
    }
    return TCL_ERROR;
}

/*
 * Champ idx d'un produit dont le type est une somme S($champ) : la variante
 * est celle dont le NOM est la valeur du champ frère `champ` (un champ déjà
 * lu ou déjà donné). vals : les valeurs des champs (NULL : pas encore
 * connue). Rend l'indice de la variante, ou -1 (pas de sélecteur, champ
 * frère inconnu, ou valeur qui ne nomme aucune variante : la validation
 * dira alors ce qui ne va pas).
 */
int
TclTypeTagVariantFrom(const TclType *prod, Tcl_Size idx, Tcl_Obj *const *vals,
	Tcl_Size nvals)
{
    const TclType *ft;
    Tcl_Size j, v;
    const char *tv;
    Tcl_WideInt wt, wv;
    int tIsInt;

    if (prod == NULL || prod->kind != TCLT_PRODUCT || idx < 0
	    || idx >= prod->nfields) {
	return -1;
    }
    ft = prod->fields[idx].type;
    if (ft == NULL || ft->kind != TCLT_SUM || ft->binTag == NULL
	    || ft->binTag[0] == '=') {
	return -1;
    }
    j = TclTypeFieldIndex(prod, ft->binTag);
    if (j < 0 || j >= nvals || vals[j] == NULL) {
	return -1;
    }
    tv = Tcl_GetString(TCL_ANNOT_CONTENT(vals[j]));
    tIsInt = (Tcl_GetWideIntFromObj(NULL, TCL_ANNOT_CONTENT(vals[j]), &wt)
	    == TCL_OK);
    for (v = 0; v < ft->nfields; v++) {
	Tcl_Obj *name = ft->fields[v].name;

	if (name == NULL) {
	    continue;
	}
	if (strcmp(Tcl_GetString(name), tv) == 0) {
	    return (int) v;
	}
	if (tIsInt && Tcl_GetWideIntFromObj(NULL, name, &wv) == TCL_OK
		&& wv == wt) {
	    return (int) v;		/* 0x04 et 4 nomment la même variante */
	}
    }
    return -1;
}

int
TclTypeTagVariant(const TclType *prod, Tcl_Size idx, Tcl_Obj *content)
{
    Tcl_Obj **elems;
    Tcl_Size n;

    if (prod == NULL || prod->kind != TCLT_PRODUCT || content == NULL
	    || Tcl_ListObjGetElements(NULL, content, &n, &elems) != TCL_OK) {
	return -1;
    }
    return TclTypeTagVariantFrom(prod, idx, elems, n);
}

/* Termination : un type récursif doit avoir une valeur finie (§4). */
/* --------------------------------------------------------------------
 * Valeurs par défaut des clés manquantes : dict(x?=0, y?=0).int
 *
 * Elles sont validées à la DÉFINITION du type (tclTypeParse.c), donc ici
 * il n'y a rien à revalider. Le remplissage traverse les collections, les
 * produits et la variante courante d'une somme, mais seulement quand le type
 * contient un défaut quelque part (NeedsFill).
 * -------------------------------------------------------------------- */

static int
NeedsFillS(const TclType *tp, const TclType **stack, int depth)
{
    Tcl_Size i;
    int k;

    if (depth >= 64) {
	return 0;
    }
    for (k = 0; k < depth; k++) {
	if (stack[k] == tp) {
	    return 0;			/* type récursif : déjà en cours d'examen */
	}
    }
    stack[depth] = tp;
    switch (tp->kind) {
    case TCLT_VAR:
	return 1;			/* les noms sont à canoniser */
    case TCLT_DICT:
	if (tp->keyed) {
	    return 1;		/* au moins pour remettre les clés dans l'ordre */
	}
	return NeedsFillS(tp->elem, stack, depth + 1);
    case TCLT_LIST:
	return NeedsFillS(tp->elem, stack, depth + 1);
    case TCLT_PRODUCT:
	for (i = 0; i < tp->nfields; i++) {
	    if (NeedsFillS(tp->fields[i].type, stack, depth + 1)) {
		return 1;
	    }
	}
	return 0;
    case TCLT_SUM:
	for (i = 0; i < tp->nfields; i++) {
	    if (tp->fields[i].type != NULL
		    && NeedsFillS(tp->fields[i].type, stack, depth + 1)) {
		return 1;
	    }
	}
	return 0;
    default:
	return 0;
    }
}

static int
NeedsFill(const TclType *tp, int depth)
{
    const TclType *stack[64];

    (void) depth;
    return NeedsFillS(tp, stack, 0);
}

/*
 * Lie la valeur d'un champ var à sa variable : nom canonique, contrôles de
 * type. obj est modifié en place.
 */

static int
BindVar(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *obj)
{
    Tcl_Obj *canon;
    TclType *vt;
    const char *c;

    if (TclVarRefCanon(interp, obj, &canon) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_IncrRefCount(canon);
    c = Tcl_GetString(canon);
    vt = TclVarRefType(interp, c);
    if (tp->varTyped && vt == NULL) {
	ERR(interp, "VAR", "variable \"%s\" must be a typed variable "
		"(declared with set {type}name ...)", c);
	Tcl_DecrRefCount(canon);
	return TCL_ERROR;
    }
    if (vt != NULL && !TclTypeIncludes(vt, tp->elem)) {
	ERR(interp, "VAR", "variable \"%s\" has type %s, which is not "
		"included in %s", c, vt->name, tp->elem->name);
	Tcl_DecrRefCount(canon);
	return TCL_ERROR;
    }
    Tcl_SetStringObj(obj, c, TCL_AUTO_LENGTH);
    Tcl_DecrRefCount(canon);
    return TCL_OK;
}

/* obj est un objet PRIVÉ, déjà valide pour tp ; il est modifié en place.
 * variant (>= 0) désigne la variante courante quand tp est une somme. */
static int
FillDefaults(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *obj,
	int variant, int depth)
{
    Tcl_Size i, n;
    Tcl_Obj *copy, *e;

    if (depth > 2000 || !NeedsFill(tp, 0)) {
	return TCL_OK;
    }
    if (tp->kind == TCLT_SUM && TclIsAnnotated(obj)
	    && TclAnnotIsSettled(obj)) {
	/* v17z : un élément d'une somme déjà typé (il garde sa variante) a
	 * été complété en entrant ; le refaire ici le réécrirait en texte
	 * (BindVar), et sa variante serait perdue */
	TclType *ot = NULL;

	if (TclAnnotInfo(obj, NULL, &ot, NULL, NULL) == 1 && ot == tp) {
	    return TCL_OK;
	}
    }
    switch (tp->kind) {
    case TCLT_VAR:
	return BindVar(interp, tp, obj);
    case TCLT_DICT: {
	TclObjVec keys = {NULL, 0, 0}, vals = {NULL, 0, 0};
	int rc = TCL_ERROR;

	/* 1. les valeurs présentes, si leur type contient des défauts */
	if (NeedsFill(tp->elem, 0)) {
	    Tcl_DictSearch search;
	    Tcl_Obj *k, *v;
	    int done;

	    if (Tcl_DictObjFirst(interp, obj, &search, &k, &v, &done)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	    for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
		copy = Tcl_DuplicateObj(v);
		if (FillDefaults(interp, tp->elem, copy, -1, depth + 1)
			!= TCL_OK) {
		    TclDiscard(copy);
		    Tcl_DictObjDone(&search);
		    goto dictDone;
		}
		TclVecAdd(&keys, k);
		TclVecAdd(&vals, copy);
	    }
	    Tcl_DictObjDone(&search);
	    for (i = 0; i < keys.n; i++) {
		if (Tcl_DictObjPut(interp, obj, keys.v[i], vals.v[i])
			!= TCL_OK) {
		    goto dictDone;
		}
	    }
	}
	/* 2. les clés manquantes qui ont un défaut, dans l'ordre écrit */
	for (i = 0; i < tp->nkeys; i++) {
	    Tcl_Obj *got = NULL;

	    if (tp->keys[i].defval == NULL && tp->keys[i].defcmd == NULL) {
		continue;
	    }
	    if (Tcl_DictObjGet(NULL, obj, tp->keys[i].name, &got) != TCL_OK) {
		goto dictDone;
	    }
	    if (got == NULL && tp->keys[i].defcmd != NULL) {
		/* défaut calculé : le script tourne à chaque construction */
		char what[200];

		snprintf(what, sizeof(what), "key \"%s\" of %s",
			Tcl_GetString(tp->keys[i].name), tp->name);
		if (TclTypeRunDefault(interp, what, tp->keys[i].defcmd, &copy)
			!= TCL_OK) {
		    goto dictDone;
		}
		Tcl_IncrRefCount(copy);
		if (TclTypeValidate(interp, tp->elem, copy, -1, NULL) != TCL_OK
			|| FillDefaults(interp, tp->elem, copy, -1, depth + 1)
			!= TCL_OK
			|| Tcl_DictObjPut(interp, obj, tp->keys[i].name, copy)
			!= TCL_OK) {
		    Tcl_DecrRefCount(copy);
		    goto dictDone;
		}
		Tcl_DecrRefCount(copy);
	    } else if (got == NULL) {
		copy = Tcl_DuplicateObj(tp->keys[i].defval);
		if (FillDefaults(interp, tp->elem, copy, -1, depth + 1)
			!= TCL_OK
			|| Tcl_DictObjPut(interp, obj, tp->keys[i].name, copy)
			!= TCL_OK) {
		    TclDiscard(copy);
		    goto dictDone;
		}
	    }
	}
	/* 3. ordre canonique : celui de la DÉFINITION du type. Toutes les
	 * clés présentes sont admises (validé), donc aucune n'est perdue. */
	if (tp->keyed) {
	    TclObjVec ordered = {NULL, 0, 0};
	    Tcl_Size cur;
	    int ok = 0;

	    for (i = 0; i < tp->nkeys; i++) {
		Tcl_Obj *got = NULL;

		if (Tcl_DictObjGet(NULL, obj, tp->keys[i].name, &got) != TCL_OK) {
		    TclVecFree(&ordered);
		    goto dictDone;
		}
		if (got != NULL) {
		    TclVecAdd(&ordered, tp->keys[i].name);
		    TclVecAdd(&ordered, got);
		}
	    }
	    ok = (Tcl_ListObjLength(interp, obj, &cur) == TCL_OK)
		    && (Tcl_ListObjReplace(interp, obj, 0, cur, ordered.n,
			    ordered.v) == TCL_OK);
	    TclVecFree(&ordered);
	    if (!ok) {
		goto dictDone;
	    }
	}
	rc = TCL_OK;
      dictDone:
	TclVecFree(&keys);
	TclVecFree(&vals);
	return rc;
    }
    case TCLT_LIST:
	if (Tcl_ListObjLength(interp, obj, &n) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (i = 0; i < n; i++) {
	    if (Tcl_ListObjIndex(interp, obj, i, &e) != TCL_OK) {
		return TCL_ERROR;
	    }
	    copy = Tcl_DuplicateObj(e);
	    if (FillDefaults(interp, tp->elem, copy, -1, depth + 1) != TCL_OK
		    || Tcl_ListObjReplace(interp, obj, i, 1, 1, &copy)
		    != TCL_OK) {
		TclDiscard(copy);
		return TCL_ERROR;
	    }
	}
	return TCL_OK;
    case TCLT_PRODUCT:
	for (i = 0; i < tp->nfields; i++) {
	    if (!NeedsFill(tp->fields[i].type, 0)) {
		continue;
	    }
	    if (Tcl_ListObjIndex(interp, obj, i, &e) != TCL_OK) {
		return TCL_ERROR;
	    }
	    copy = Tcl_DuplicateObj(e);
	    if (FillDefaults(interp, tp->fields[i].type, copy, -1, depth + 1)
		    != TCL_OK
		    || Tcl_ListObjReplace(interp, obj, i, 1, 1, &copy)
		    != TCL_OK) {
		TclDiscard(copy);
		return TCL_ERROR;
	    }
	}
	return TCL_OK;
    case TCLT_SUM:
	if (variant < 0) {
	    TclTypeValidate(NULL, tp, obj, -1, &variant);
	}
	if (variant >= 0 && variant < tp->nfields
		&& tp->fields[variant].type != NULL && TclSumTagged(tp)) {
	    /* v16w : la charge, deuxième élément de "v charge" (une valeur
	     * déjà typée est complète) */
	    Tcl_Obj *pl = NULL, *plc;

	    if (TclIsAnnotated(obj)
		    || Tcl_ListObjIndex(NULL, obj, 1, &pl) != TCL_OK
		    || pl == NULL) {
		return TCL_OK;
	    }
	    plc = Tcl_DuplicateObj(pl);
	    Tcl_IncrRefCount(plc);
	    if (FillDefaults(interp, tp->fields[variant].type, plc, -1,
		    depth + 1) != TCL_OK) {
		Tcl_DecrRefCount(plc);
		return TCL_ERROR;
	    }
	    if (strcmp(Tcl_GetString(plc), Tcl_GetString(pl)) != 0) {
		Tcl_ListObjReplace(NULL, obj, 1, 1, 1, &plc);
	    }
	    Tcl_DecrRefCount(plc);
	    return TCL_OK;
	}
	if (variant >= 0 && variant < tp->nfields
		&& tp->fields[variant].type != NULL) {
	    return FillDefaults(interp, tp->fields[variant].type, obj, -1,
		    depth + 1);
	}
	return TCL_OK;
    default:
	return TCL_OK;
    }
}

int
TclTypeComplete(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *content,
	int variantHint, int *variantOut)
{
    int variant = -1;

    if (variantHint >= 0 && variantHint < tp->nfields
	    && tp->kind == TCLT_SUM && !TclSumTagged(tp)) {
	/*
	 * v17z : une variante désignée (étiquette {/v}, type S.v) : la valeur
	 * est sa charge, complétée et normalisée dans le type de la variante ;
	 * la variante reste en métadonnée (variantOut).
	 */
	const TclTypeField *f = &tp->fields[variantHint];

	if (f->type == NULL) {
	    if (strcmp(Tcl_GetString(content), Tcl_GetString(f->literal))
		    != 0 && Tcl_GetCharLength(content) > 0) {
		ERR(interp, "SUM", "variant %s of %s takes no value, got \"%s\"",
			Tcl_GetString(f->literal), tp->name,
			Tcl_GetString(content));
		return TCL_ERROR;
	    }
	    Tcl_SetStringObj(content, Tcl_GetString(f->literal), -1);
	} else {
	    /* (v18b : une valeur de cette somme, annotée : sa charge) */
	    TclType *ct = NULL;
	    Tcl_Obj *pl = content;

	    if (TclAnnotInfo(content, &pl, &ct, NULL, NULL) != 1
		    || ct == NULL || TclTypeSumRoot(ct) != TclTypeSumRoot(tp)) {
		pl = content;
	    }
	    if (TclTypeComplete(interp, f->type, pl, -1, NULL) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	if (variantOut != NULL) {
	    *variantOut = variantHint;
	}
	return TCL_OK;
    }
    if (variantHint >= 0 && variantHint < tp->nfields && TclSumTagged(tp)) {
	/*
	 * v16w : une variante désignée (étiquette {/v}, type S.v) avec sa
	 * charge : la charge est complétée dans son type, puis content (privé)
	 * devient la valeur de la somme, "v charge".
	 */
	const TclTypeField *f = &tp->fields[variantHint];

	if (f->type == NULL) {
	    if (strcmp(Tcl_GetString(content), Tcl_GetString(f->literal))
		    != 0 && Tcl_GetCharLength(content) > 0) {
		ERR(interp, "SUM", "variant %s of %s takes no value, got \"%s\"",
			Tcl_GetString(f->literal), tp->name,
			Tcl_GetString(content));
		return TCL_ERROR;
	    }
	    Tcl_SetStringObj(content, Tcl_GetString(f->literal), -1);
	} else {
	    Tcl_Obj *pl = Tcl_DuplicateObj(content), *pair[2];

	    Tcl_IncrRefCount(pl);
	    if (TclTypeComplete(interp, f->type, pl, -1, NULL) != TCL_OK) {
		Tcl_DecrRefCount(pl);
		return TCL_ERROR;
	    }
	    pair[0] = f->name;
	    pair[1] = pl;
	    Tcl_SetListObj(content, 2, pair);
	    Tcl_DecrRefCount(pl);
	}
	if (variantOut != NULL) {
	    *variantOut = variantHint;
	}
	return TCL_OK;
    }
    if (variantHint <= -2 && TclSumTagged(tp)) {
	variantHint = -1;		/* la valeur dit sa variante */
    }
    if (TclTypeValidate(interp, tp, content, variantHint, &variant)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (variantOut != NULL) {
	*variantOut = variant;
    }
    if (tp->kind == TCLT_SUM && !TclSumTagged(tp) && variant >= 0
	    && variant < tp->nfields && tp->fields[variant].type != NULL) {
	/* v17z : variante déduite : la charge normalisée dans son type
	 * (v18b : une valeur de cette somme, annotée : sa charge) */
	TclType *ct = NULL;
	Tcl_Obj *pl = content;

	if (TclAnnotInfo(content, &pl, &ct, NULL, NULL) != 1 || ct == NULL
		|| TclTypeSumRoot(ct) != TclTypeSumRoot(tp)) {
	    pl = content;
	}
	return TclTypeComplete(interp, tp->fields[variant].type, pl, -1,
		NULL);
    }
    return FillDefaults(interp, tp, content, variant, 0);
}

static int
Inhabited(const TclType *tp, const TclType **stack, int depth)
{
    Tcl_Size i;

    if (depth >= 64) {
	return 1;
    }
    for (i = 0; i < depth; i++) {
	if (stack[i] == tp) {
	    return 0;
	}
    }
    switch (tp->kind) {
    case TCLT_PRODUCT:
	stack[depth] = tp;
	for (i = 0; i < tp->nfields; i++) {
	    if (!Inhabited(tp->fields[i].type, stack, depth + 1)) {
		return 0;
	    }
	}
	return 1;
    case TCLT_SUM:
	stack[depth] = tp;
	for (i = 0; i < tp->nfields; i++) {
	    if (tp->fields[i].type == NULL
		    || Inhabited(tp->fields[i].type, stack, depth + 1)) {
		return 1;
	    }
	}
	return 0;
    default:
	return 1;		/* scalaires, collections (vides possibles) */
    }
}

/* --------------------------------------------------------------------
 * Résolution des noms, interning et tri des variantes (§4)
 *
 * L'analyse du texte d'un type est dans tclTypeParse.c : ce fichier ne
 * garde que ce qui touche à la table.
 * -------------------------------------------------------------------- */

/* Analyse TEXT avec les noms résolus dans l'espace de OWNER (un nom
 * qualifié : ::geom::matrix) : le corps d'une famille. */
int
TclTypesCompileIn(Tcl_Interp *interp, TypeData *d, const char *owner,
	const char *text, Tcl_Size len, TypeEnv *envPtr)
{
    Namespace *save = d->ctxNs, *ns = NULL;
    const char *last = NULL, *p;
    int rc;

    for (p = owner; p[0] != '\0' && p[0] != '('; p++) {
	if (p[0] == ':' && p[1] == ':') {
	    last = p;
	}
    }
    if (last != NULL && last > owner && interp != NULL) {
	Tcl_DString nd;

	Tcl_DStringInit(&nd);
	Tcl_DStringAppend(&nd, owner, last - owner);
	ns = (Namespace *) Tcl_FindNamespace(interp, Tcl_DStringValue(&nd),
		NULL, TCL_GLOBAL_ONLY);
	Tcl_DStringFree(&nd);
    } else if (interp != NULL) {
	ns = ((Interp *) interp)->globalNsPtr;
    }
    d->ctxNs = ns;
    rc = TclCompileType(interp, text, len, envPtr);
    d->ctxNs = save;
    return rc;
}

TclType *
TclTypesResolve(Tcl_Interp *interp, TypeData *d, const char *selfName,
	TclType *self, const char *w, Tcl_Size len)
{
    char *name = DupStr(w, len);
    Tcl_HashEntry *hPtr;
    TclType *tp = NULL;
    int isSelf = 0;

    hPtr = TclTypesFindName(interp, d, &d->names, name, len, selfName,
	    &isSelf);
    if (isSelf) {
	Tcl_Free(name);
	return self;
    }
    if (hPtr == NULL) {
	ERR(interp, "UNKNOWN", "unknown type \"%s\"", name);
    } else {
	tp = (TclType *) Tcl_GetHashValue(hPtr);
	if (tp == NULL) {
	    ERR(interp, "RESERVED", "type \"%s\" is reserved and has no "
		    "meaning in this version", name);
	}
    }
    Tcl_Free(name);
    return tp;
}

static int
DuplicateName(const TclTypeField *fields, Tcl_Size n)
{
    Tcl_Size i, j;

    for (i = 0; i < n; i++) {
	for (j = i + 1; j < n; j++) {
	    if (fields[i].name != NULL && fields[j].name != NULL
		    && strcmp(Tcl_GetString(fields[i].name),
			    Tcl_GetString(fields[j].name)) == 0) {
		return 1;
	    }
	}
    }
    return 0;
}

/*
 * Ordre canonique des variantes : une variante plus spécifique passe avant
 * toute variante plus générale (§4), QUEL QUE SOIT l'ordre d'écriture.
 *
 * L'inclusion est un ordre PARTIEL : un tri par échanges adjacents ne
 * convient pas (une variante incomparable entre deux variantes comparables
 * bloquerait l'échange). On fait un tri topologique stable : on place à
 * chaque tour la première variante restante dont aucune autre variante
 * restante n'est strictement plus spécifique.
 */

static void
SortVariants(TclTypeField *f, Tcl_Size n)
{
    TclTypeField *out = (TclTypeField *) Tcl_Alloc(n * sizeof(*out));
    int *used = (int *) Tcl_Alloc(n * sizeof(int));
    Tcl_Size k, i, j;

    memset(used, 0, n * sizeof(int));
    for (k = 0; k < n; k++) {
	for (i = 0; i < n; i++) {
	    int blocked = 0;

	    if (used[i]) {
		continue;
	    }
	    for (j = 0; j < n && !blocked; j++) {
		blocked = (j != i && !used[j]
			&& VariantIncludes(&f[j], &f[i])
			&& !VariantIncludes(&f[i], &f[j]));
	    }
	    if (!blocked) {
		break;
	    }
	}
	if (i == n) {		/* garde-fou : cycle, normalement impossible */
	    for (i = 0; used[i]; i++) {
		/* premier restant */
	    }
	}
	out[k] = f[i];
	used[i] = 1;
    }
    memcpy(f, out, n * sizeof(*out));
    Tcl_Free(out);
    Tcl_Free(used);
}

/*
 * Fige un produit ou une somme. Prend possession de fields, y compris en
 * cas d'échec. Si into n'est pas NULL (racine d'un "type def"), le type
 * nominal est rempli ; sinon le type anonyme est interné par son nom
 * canonique (qui inclut les valeurs par défaut).
 */

TclType *
TclTypesFinish(Tcl_Interp *interp, TypeData *d, const char *selfName,
	TclTypeKind kind, TclTypeField *fields, Tcl_Size n, TclType *into)
{
    return TclTypesFinishSum(interp, d, selfName, kind, fields, n, into, 0);
}

/*
 * v16w : une union ("|") doit être disjointe : sa valeur est la charge
 * seule, et l'alternative se calcule à partir d'elle. NULL si elle l'est,
 * sinon un message (refcount 0).
 */
static Tcl_Obj *SumOverlapsEx(const TclType *tp, int canonical);

static Tcl_Obj *
UnionOverlap(const TclType *tp)
{
    Tcl_Obj *ov = SumOverlapsEx(tp, 1), *p0, *va, *vb, *msg = NULL;
    Tcl_Size nov = 0;

    Tcl_IncrRefCount(ov);
    Tcl_ListObjLength(NULL, ov, &nov);
    if (nov > 0) {
	Tcl_ListObjIndex(NULL, ov, 0, &p0);
	Tcl_ListObjIndex(NULL, p0, 0, &va);
	Tcl_ListObjIndex(NULL, p0, 1, &vb);
	msg = Tcl_ObjPrintf("union: alternatives %s and %s can take the same "
		"value, so the value could not tell which one it is; a union "
		"must be disjoint. Write a sum with \",\" to tag the "
		"alternatives", Tcl_GetString(va), Tcl_GetString(vb));
    }
    Tcl_DecrRefCount(ov);
    return msg;
}

TclType *
TclTypesFinishSum(Tcl_Interp *interp, TypeData *d, const char *selfName,
	TclTypeKind kind, TclTypeField *fields, Tcl_Size n, TclType *into,
	int isUnion)
{
    Tcl_DString ds;
    Tcl_HashEntry *hPtr;
    Tcl_Size i;
    int isNew;
    TclType *tp;

    (void) selfName;			/* v18a : plus de conflit nom/type */
    if (DuplicateName(fields, n)) {
	ERR(interp, "SYNTAX", "duplicate %s name in %s",
		kind == TCLT_SUM ? "variant" : "field",
		kind == TCLT_SUM ? "sum" : "product");
	goto fail;
    }
    if (kind == TCLT_SUM) {
	/* v18a : un nom de variante peut aussi être un nom de type : la
	 * variante s'écrit {/v} ou S/v, jamais {v} ni S.v. */
	if (!isUnion) {
	    /* v16w : la valeur d'une somme écrit le nom de sa variante :
	     * une variante typée a un nom */
	    for (i = 0; i < n; i++) {
		if (fields[i].type != NULL && fields[i].name == NULL) {
		    const char *tn = fields[i].type->name;
		    int anon = (strchr(tn, '<') != NULL);

		    ERR(interp, "SYNTAX", "variant %d of the sum has no name: "
			    "a label or a path chooses a variant by its name; "
			    "name it (%s%s%sname), or write a union with \"|\" "
			    "if the value itself tells the alternatives apart",
			    (int) i + 1, anon ? "{...}" : "{", anon ? "" : tn,
			    anon ? "" : "}");
		    goto fail;
		}
	    }
	}
	SortVariants(fields, n);
    } else {
	isUnion = 0;
    }

    if (into != NULL) {
	into->kind = kind;
	into->fields = fields;
	into->nfields = n;
	if (kind == TCLT_SUM) {
	    Tcl_Obj *msg;

	    into->sumMode = isUnion ? TCLT_SUM_UNION : TCLT_SUM_TAGGED;
	    if (isUnion && (msg = UnionOverlap(into)) != NULL) {
		if (interp != NULL) {
		    Tcl_SetObjResult(interp, msg);
		    Tcl_SetErrorCode(interp, "TCL", "TYPE", "UNION",
			    (char *) NULL);
		} else {
		    Tcl_IncrRefCount(msg);
		Tcl_DecrRefCount(msg);
		}
		into->fields = NULL;
		into->nfields = 0;
		goto fail;
	    }
	}
	return into;
    }

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, kind == TCLT_PRODUCT ? "prod<"
	    : isUnion ? "union<" : "sum<", TCL_AUTO_LENGTH);
    for (i = 0; i < n; i++) {
	if (fields[i].name != NULL) {
	    Tcl_DStringAppend(&ds, Tcl_GetString(fields[i].name),
		    TCL_AUTO_LENGTH);
	}
	Tcl_DStringAppend(&ds, ":", 1);
	if (fields[i].type != NULL) {
	    Tcl_DStringAppend(&ds, fields[i].type->name, TCL_AUTO_LENGTH);
	} else {
	    Tcl_DStringAppend(&ds, "\"", 1);
	    Tcl_DStringAppend(&ds, Tcl_GetString(fields[i].literal),
		    TCL_AUTO_LENGTH);
	    Tcl_DStringAppend(&ds, "\"", 1);
	}
	if (fields[i].defval != NULL) {
	    Tcl_DStringAppend(&ds, "=\"", 2);
	    Tcl_DStringAppend(&ds, Tcl_GetString(fields[i].defval),
		    TCL_AUTO_LENGTH);
	    Tcl_DStringAppend(&ds, "\"", 1);
	}
	if (fields[i].defcmd != NULL) {
	    Tcl_DStringAppend(&ds, "=[", 2);
	    Tcl_DStringAppend(&ds, Tcl_GetString(fields[i].defcmd),
		    TCL_AUTO_LENGTH);
	    Tcl_DStringAppend(&ds, "]", 1);
	}
	Tcl_DStringAppend(&ds, ";", 1);
    }
    Tcl_DStringAppend(&ds, ">", 1);
    hPtr = Tcl_CreateHashEntry(&d->canon, Tcl_DStringValue(&ds), &isNew);
    if (!isNew) {
	TclTypesFreeFields(fields, n);
	tp = (TclType *) Tcl_GetHashValue(hPtr);
    } else {
	tp = NewType(d, kind, Tcl_DStringValue(&ds));
	tp->fields = fields;
	tp->nfields = n;
	tp->sumMode = isUnion ? TCLT_SUM_UNION : TCLT_SUM_TAGGED;
	Tcl_SetHashValue(hPtr, tp);
    }
    Tcl_DStringFree(&ds);
    if (isUnion) {
	Tcl_Obj *msg = UnionOverlap(tp);

	if (msg != NULL) {
	    /* le type reste interné (inaccessible), l'erreur revient à
	     * chaque analyse */
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, msg);
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "UNION", (char *) NULL);
	    } else {
		Tcl_IncrRefCount(msg);
		Tcl_DecrRefCount(msg);
	    }
	    return NULL;
	}
    }
    return tp;

  fail:
    TclTypesFreeFields(fields, n);
    return NULL;
}

static TclType *TypeParseMain(Tcl_Interp *interp, const char *text,
	Tcl_Size len);

/*
 * v18a : un membre s'écrit X/m. L'ancienne écriture S.v (variante) ou P.f
 * (champ) échoue comme un type inconnu : le message donne la nouvelle.
 */
TclType *
TclTypeParse(Tcl_Interp *interp, const char *text, Tcl_Size len)
{
    TclType *tp, *left;
    const char *dot;
    Tcl_Size k;
    Tcl_InterpState st;

    if (len < 0) {
	len = strlen(text);
    }
    tp = TypeParseMain(interp, text, len);
    if (tp != NULL || interp == NULL || len > 200
	    || (dot = (const char *) memchr(text, '.', len)) == NULL) {
	return tp;
    }
    for (dot = text + len - 1; dot > text && *dot != '.'; dot--) {
    }
    for (k = dot + 1 - text; k < len && (isalnum(UCHAR(text[k]))
	    || text[k] == '_' || (text[k] & 0x80)); k++) {
    }
    if (dot == text || k < len || dot + 1 == text + len) {
	return NULL;
    }
    st = Tcl_SaveInterpState(interp, TCL_ERROR);
    left = TypeParseMain(interp, text, dot - text);
    if (left != NULL && (left->kind == TCLT_SUM
	    || left->kind == TCLT_PRODUCT)) {
	char buf[128];
	Tcl_Size ml = len - (dot + 1 - text);

	if (ml < (Tcl_Size) sizeof(buf)) {
	    memcpy(buf, dot + 1, ml);
	    buf[ml] = '\0';
	    if (TclTypeFieldIndex(left, buf) >= 0) {
		Tcl_DiscardInterpState(st);
		ERR(interp, "OLDMEMBER", "a %s of a type is now written with "
			"\"/\": %.*s/%s", left->kind == TCLT_SUM ? "variant"
			: "field", (int) (dot - text), text, buf);
		return NULL;
	    }
	}
    }
    (void) Tcl_RestoreInterpState(interp, st);
    return NULL;
}

static TclType *
TypeParseMain(Tcl_Interp *interp, const char *text, Tcl_Size len)
{
    TypeEnv env;

    memset(&env, 0, sizeof(env));
    env.d = GetData(interp);
    if (len < 0) {
	len = strlen(text);
    }
    if (TclLabelIsOld(text, len)) {
	/* v18a : l'étiquette {nom:} s'écrit désormais {/nom} */
	Tcl_Size k;

	for (k = 0; k < len - 1 && (isalnum(UCHAR(text[k])) || text[k] == '_'
		|| (text[k] & 0x80)); k++) {
	}
	if (k == len - 1) {
	    ERR(interp, "OLDLABEL", "label {%.*s} is now written {/%.*s}",
		    (int) len, text, (int) len - 1, text);
	    return NULL;
	}
    }
    if (len > 1 && text[0] == '@') {
	/* v16y : une note d'instrument ({@trace}) n'est jamais un type, mais
	 * c'est un nom (une note), pas un caractère interdit (local). */
	Tcl_Size k;

	for (k = 1; k < len && (isalnum(UCHAR(text[k])) || text[k] == '_'
		|| (text[k] & 0x80)); k++) {
	}
	if (k == len) {
	    ERR(interp, "UNKNOWN", "\"%.*s\" is an instrument note, not a "
		    "type", (int) len, text);
	    return NULL;
	}
    }
    if ((len > 2 && text[0] == '*' && text[1] == '(')
	    || (len > 3 && text[0] == '&' && text[1] == '*' && text[2] == '(')) {
	/* Le nom d'un lien déjà construit (il peut contenir un nom canonique
	 * anonyme, list($n)<sec>, qui ne s'écrit pas) : on le retrouve tel quel. */

	Tcl_DString ds;
	Tcl_HashEntry *hPtr;

	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, text, len);
	hPtr = Tcl_FindHashEntry(&env.d->canon, Tcl_DStringValue(&ds));
	Tcl_DStringFree(&ds);
	if (hPtr != NULL) {
	    return (TclType *) Tcl_GetHashValue(hPtr);
	}
    }
    /*
     * Mémo des textes déjà analysés ({point}p reconstruit à chaque appel
     * son texte d'annotation : TclTypeParse le recompilait à chaque fois).
     * Valable tant que les noms ne changent pas (gen) ; borné.
     */
    if (env.d != NULL && len > 0 && len < 200) {
	char key[248];
	Tcl_HashEntry *hPtr;
	int isNew, pre = 0;

	TclTypesSync(env.d);
	if (env.d->parsedGen != env.d->gen
		|| env.d->parsed.numEntries > 4096) {
	    Tcl_DeleteHashTable(&env.d->parsed);
	    Tcl_InitHashTable(&env.d->parsed, TCL_STRING_KEYS);
	    env.d->parsedGen = env.d->gen;
	}
	if (env.d->nNs > 0 && interp != NULL) {
	    /* v16u : le même texte se résout selon l'espace courant */
	    Namespace *ns = CtxNs(interp, env.d);

	    pre = snprintf(key, 48, "\2%" TCL_Z_MODIFIER "x.%" TCL_Z_MODIFIER "x\2", (size_t) ns->nsId,
		    (size_t) ns->resolverEpoch);
	}
	memcpy(key + pre, text, (size_t) len);
	key[pre + len] = '\0';
	if (memchr(text, '\0', (size_t) len) == NULL) {
	    hPtr = Tcl_FindHashEntry(&env.d->parsed, key);
	    if (hPtr != NULL) {
		return (TclType *) Tcl_GetHashValue(hPtr);
	    }
	    if (TclCompileType(interp, text, len, &env) != TCL_OK) {
		return NULL;
	    }
	    if (env.result != NULL && env.d->parsedGen == env.d->gen) {
		hPtr = Tcl_CreateHashEntry(&env.d->parsed, key, &isNew);
		Tcl_SetHashValue(hPtr, env.result);
	    }
	    return env.result;
	}
    }
    if (TclCompileType(interp, text, len, &env) != TCL_OK) {
	return NULL;
    }
    return env.result;
}

/* --------------------------------------------------------------------
 * Cast et conversions (§5, §7)
 * -------------------------------------------------------------------- */

static Tcl_Obj *
TruncateToInt(Tcl_Interp *interp, Tcl_Obj *v)
{
    void *clientData;
    int type;
    double dbl;
    mp_int big;

    if (Tcl_GetNumberFromObj(interp, v, &clientData, &type) != TCL_OK) {
	return NULL;
    }
    switch (type) {
    case TCL_NUMBER_INT:
    case TCL_NUMBER_BIG:
	return Tcl_DuplicateObj(v);
    case TCL_NUMBER_DOUBLE:
	dbl = *(const double *) clientData;
	if (isnan(dbl) || isinf(dbl)) {
	    break;
	}
	dbl = trunc(dbl);		/* vers zéro : -1.9 donne -1 */
	if (fabs(dbl) < 9.0e18) {
	    return Tcl_NewWideIntObj((Tcl_WideInt) dbl);
	}
	if (Tcl_InitBignumFromDouble(interp, dbl, &big) != TCL_OK) {
	    return NULL;
	}
	return Tcl_NewBignumObj(&big);
    default:
	break;
    }
    ERR(interp, "CAST", "can't convert \"%s\" to an integer",
	    Tcl_GetString(v));
    return NULL;
}

static Tcl_Obj *
ApplyConvert(Tcl_Interp *interp, Tcl_Obj *lambda, Tcl_Obj *content)
{
    Tcl_Obj *argv[3];
    Tcl_Obj *res;
    int i, rc;

    argv[0] = Tcl_NewStringObj("apply", 5);
    argv[1] = lambda;
    argv[2] = content;
    for (i = 0; i < 3; i++) {
	Tcl_IncrRefCount(argv[i]);
    }
    rc = Tcl_EvalObjv(interp, 3, argv, 0);
    for (i = 0; i < 3; i++) {
	Tcl_DecrRefCount(argv[i]);
    }
    if (rc != TCL_OK) {
	return NULL;
    }
    res = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
    return res;
}

/* --------------------------------------------------------------------
 * Règles de famille : type convert grandeur grandeur {{x de vers} ...}
 * -------------------------------------------------------------------- */

/* Le premier type de la chaîne de bases qui appartient à une famille. */
static TclType *
FamilyOf(TclType *tp)
{
    int depth;

    for (depth = 0; tp != NULL && depth < 64; tp = tp->under, depth++) {
	if (tp->family != NULL) {
	    return tp;
	}
    }
    return NULL;
}

/* Même famille et mêmes paramètres : la même instance (couple et energie
 * sur grandeur(M.L2.T-2)) ; ce n'est pas un changement d'unité. */
static int
SameInstance(TclType *a, TclType *b)
{
    return a->family == b->family && strcmp(Tcl_GetString(a->familyArgs),
	    Tcl_GetString(b->familyArgs)) == 0;
}

/*
 * Les paramètres de l'instance fi, comme valeur du produit anonyme de sa
 * famille : {string}dim, {double}f = 1.0 donne {{string}dim; {double}f}.
 * Un paramètre {type}T ou {cmd}c est un champ string (son nom canonique) ;
 * les autres gardent leur type s'il est int, double ou string.
 */
static Tcl_Obj *
FamilyParams(Tcl_Interp *interp, TclType *fi)
{
    TypeGeneric *g = fi->family;
    Tcl_Obj *copy, *res;
    int variant = -1;
    Tcl_Size i;

    if (g->paramsType == NULL) {
	Tcl_DString ds;

	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, "{", 1);
	for (i = 0; i < g->nparams; i++) {
	    const TypeParam *pp = &g->params[i];
	    const char *tn = "string";

	    if (!pp->isType && !pp->isCmd && pp->type != NULL
		    && pp->type->reserved && (strcmp(pp->type->name, "int") == 0
		    || strcmp(pp->type->name, "double") == 0)) {
		tn = pp->type->name;
	    }
	    Tcl_DStringAppend(&ds, i > 0 ? "; {" : "{", TCL_AUTO_LENGTH);
	    Tcl_DStringAppend(&ds, tn, TCL_AUTO_LENGTH);
	    Tcl_DStringAppend(&ds, "}", 1);
	    Tcl_DStringAppend(&ds, Tcl_GetString(pp->name), TCL_AUTO_LENGTH);
	}
	Tcl_DStringAppend(&ds, "}", 1);
	g->paramsType = TclTypeParse(interp, Tcl_DStringValue(&ds),
		Tcl_DStringLength(&ds));
	Tcl_DStringFree(&ds);
	if (g->paramsType == NULL) {
	    return NULL;
	}
    }
    copy = Tcl_DuplicateObj(fi->familyArgs);
    Tcl_IncrRefCount(copy);
    if (TclTypeComplete(interp, g->paramsType, copy, -1, &variant)
	    != TCL_OK) {
	Tcl_DecrRefCount(copy);
	return NULL;
    }
    res = TclNewAnnotated(interp, g->paramsType, variant, copy);
    Tcl_DecrRefCount(copy);
    return res;
}

/*
 * Appelle la règle : apply lambda contenu ?de? ?vers?. Rend TCL_OK
 * (*resPtr, objet neuf), TCL_CONTINUE (« règle non applicable ») ou
 * TCL_ERROR.
 */
static int
ApplyFamily(Tcl_Interp *interp, Tcl_Obj *lambda, Tcl_Obj *content,
	TclType *fi, TclType *ti, Tcl_Obj **resPtr)
{
    Tcl_Obj *argv[5];
    int argc = 0, i, rc;

    argv[argc++] = Tcl_NewStringObj("apply", 5);
    argv[argc++] = lambda;
    argv[argc++] = content;
    if (fi != NULL) {
	if ((argv[argc] = FamilyParams(interp, fi)) == NULL) {
	    TclDiscard(argv[0]);
	    return TCL_ERROR;
	}
	argc++;
    }
    if (ti != NULL) {
	if ((argv[argc] = FamilyParams(interp, ti)) == NULL) {
	    for (i = 3; i < argc; i++) {
		TclDiscard(argv[i]);
	    }
	    TclDiscard(argv[0]);
	    return TCL_ERROR;
	}
	argc++;
    }
    for (i = 0; i < argc; i++) {
	Tcl_IncrRefCount(argv[i]);
    }
    rc = Tcl_EvalObjv(interp, argc, argv, 0);
    for (i = 0; i < argc; i++) {
	Tcl_DecrRefCount(argv[i]);
    }
    if (rc == TCL_OK) {
	*resPtr = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
	return TCL_OK;
    }
    if (rc == TCL_CONTINUE) {
	Tcl_ResetResult(interp);
	return TCL_CONTINUE;
    }
    return TCL_ERROR;
}


/*
 * v17d : le lecteur d'une valeur NUE (ou seulement notée) pour tp, le même
 * pour set, type cast et les arguments (étape 4 de PISTES-any-string § 6) :
 *
 *   - tp nominal, avec une règle depuis son type sous-jacent (type convert
 *     string Jour) : c'est sa validation ; lecture native comme le
 *     sous-jacent, puis la règle, puis la lecture native de tp ;
 *   - sinon la lecture native de tp ; si elle échoue et qu'une règle depuis
 *     la valeur nue existe (type convert any T, ou string T : toute valeur
 *     se lit en chaîne), la règle, puis la lecture native.
 *
 * content est privé (modifiable). Une règle qui relit une valeur nue du
 * même type n'est pas rappelée (pas de récursion).
 */
#define BARE_RULE_DEPTH 16
static const TclType *bareRuleStack[BARE_RULE_DEPTH];
static int bareRuleTop = 0;		/* (un interpréteur, un thread) */

static Tcl_Obj *
FindRule(TypeData *d, const char *from, const char *to)
{
    char key[512];
    Tcl_HashEntry *hPtr;

    if (d->converts.numEntries == 0) {
	return NULL;			/* aucune règle : pas de clé à former */
    }
    snprintf(key, sizeof(key), "%s\1%s", from, to);
    hPtr = Tcl_FindHashEntry(&d->converts, key);
    return hPtr != NULL ? (Tcl_Obj *) Tcl_GetHashValue(hPtr) : NULL;
}

static int
ApplyBareRule(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *rule,
	Tcl_Obj *content)
{
    Tcl_Obj *res;
    Tcl_Size len;
    const char *s;

    if (bareRuleTop >= BARE_RULE_DEPTH) {
	ERR(interp, "CAST", "type convert rules nested too deeply for %s",
		tp->name);
	return TCL_ERROR;
    }
    {
	/* la règle reçoit une copie : content reste à nous seuls */
	Tcl_Obj *arg = Tcl_DuplicateObj(content);

	Tcl_IncrRefCount(arg);
	bareRuleStack[bareRuleTop++] = tp;
	res = ApplyConvert(interp, rule, arg);
	bareRuleTop--;
	Tcl_DecrRefCount(arg);
    }
    if (res == NULL) {
	return TCL_ERROR;
    }
    if (TclIsAnnotated(res)) {
	Tcl_Obj *c = TCL_ANNOT_CONTENT(res);

	s = Tcl_GetStringFromObj(c, &len);
    } else {
	s = Tcl_GetStringFromObj(res, &len);
    }
    Tcl_SetStringObj(content, s, len);
    TclDiscard(res);
    return TCL_OK;
}

/*
 * 23/11 : le registre des spécialisations. Une déclaration fait monter la
 * génération : chaque type refait son choix à son prochain usage (les types
 * déjà définis profitent d'une bibliothèque chargée après eux).
 */
int
TclTypesRegisterOps(Tcl_Interp *interp, const TclTypeSpec *spec)
{
    TypeData *d = GetData(interp);

    if (d == NULL || spec == NULL || spec->match == NULL
	    || spec->ops == NULL || spec->version != TCL_TYPE_OPS_VERSION
	    || spec->ops->version != TCL_TYPE_OPS_VERSION) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("type ops \"%s\": "
		    "version %d expected", (spec && spec->name) ? spec->name
		    : "?", TCL_TYPE_OPS_VERSION));
	}
	return TCL_ERROR;
    }
    if (d->nSpecs == d->capSpecs) {
	d->capSpecs = d->capSpecs ? 2 * d->capSpecs : 8;
	d->specs = (TclTypeSpec *) Tcl_Realloc(d->specs,
		d->capSpecs * sizeof(TclTypeSpec));
    }
    d->specs[d->nSpecs++] = *spec;
    d->opsGen++;
    return TCL_OK;
}

/*
 * Mode de contrôle (variable d'environnement TCL_TYPE_OPS) : "off", aucune
 * spécialisation ; "check", la spécialisation ET le chemin général, comparés
 * (texte et type) ; un écart est écrit sur stderr et le résultat général
 * l'emporte. Lu une fois.
 */
int
TclTypeOpsMode(void)
{
    static int mode = -1;

    if (mode < 0) {
	const char *e = getenv("TCL_TYPE_OPS");

	mode = (e == NULL) ? TCL_TYPE_OPS_ON : strcmp(e, "off") == 0
		? TCL_TYPE_OPS_OFF : strcmp(e, "check") == 0
		? TCL_TYPE_OPS_CHECK : TCL_TYPE_OPS_ON;
    }
    return mode;
}

/* Compare la valeur d'une spécialisation (OPS) à celle du chemin général
 * (GEN) ; 1 si égales. */
int
TclTypeOpsSame(const char *where, Tcl_Obj *ops, Tcl_Obj *gen)
{
    TclType *t1 = NULL, *t2 = NULL;
    int same;

    if (ops == NULL || gen == NULL) {
	same = (ops == gen);
    } else {
	TclAnnotInfo(ops, NULL, &t1, NULL, NULL);
	TclAnnotInfo(gen, NULL, &t2, NULL, NULL);
	same = (t1 == t2) && strcmp(Tcl_GetString(ops), Tcl_GetString(gen)) == 0;
    }
    if (!same) {
	fprintf(stderr, "type ops %s : \"%s\" (%s) au lieu de \"%s\" (%s)\n",
		where, ops ? Tcl_GetString(ops) : "NULL", t1 ? t1->name : "-",
		gen ? Tcl_GetString(gen) : "NULL", t2 ? t2->name : "-");
    }
    return same;
}

const TclTypeOps *
TclTypeOpsOf(const TclType *tp)
{
    TypeData *d;
    TclType *t = (TclType *) tp;
    Tcl_Size k;
    int best = 0, found = 0;

    if (tp == NULL || (d = tp->owner) == NULL || d->dead
	    || TclTypeOpsMode() == TCL_TYPE_OPS_OFF) {
	return NULL;
    }
    if (t->opsGen == d->opsGen) {
	return t->ops;
    }
    t->ops = NULL;
    for (k = 0; k < d->nSpecs; k++) {
	const TclTypeSpec *sp = &d->specs[k];

	if ((!found || sp->priority > best)
		&& sp->match(d->interp, tp, sp->clientData)) {
	    t->ops = sp->ops;
	    best = sp->priority;
	    found = 1;
	}
    }
    t->opsGen = d->opsGen;
    return t->ops;
}

/*
 * 23/11 : l'entrée des bibliothèques externes (version simplifiée, TIP 764).
 * Une extension la trouve par Tcl_GetAssocData(interp, "tclTypesOps", NULL)
 * (tclTypesOps.h) : la déclaration des spécialisations, la lecture d'un
 * type (opaque) et la fabrication d'une valeur typée.
 */
static int ExtKind(const TclType *tp) { return (int) tp->kind; }
static const char *ExtName(const TclType *tp) { return tp->name; }
static const TclType *ExtElem(const TclType *tp) { return tp->elem; }
static Tcl_Size ExtNFields(const TclType *tp)
{
    return tp->kind == TCLT_PRODUCT ? tp->nfields : 0;
}
static const char *ExtFieldName(const TclType *tp, Tcl_Size f)
{
    return (f >= 0 && f < ExtNFields(tp) && tp->fields[f].name)
	    ? Tcl_GetString(tp->fields[f].name) : NULL;
}
static const TclType *ExtFieldType(const TclType *tp, Tcl_Size f)
{
    return (f >= 0 && f < ExtNFields(tp)) ? tp->fields[f].type : NULL;
}
static Tcl_Obj *ExtWrap(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *c)
{
    (void) interp;
    return TclAnnotWrapType(c, tp);
}

static const TclTypesOpsApi typesOpsApi = {
    TCL_TYPE_OPS_VERSION,
    TclTypesRegisterOps,
    ExtKind, ExtName, ExtElem, ExtNFields, ExtFieldName, ExtFieldType,
    ExtWrap
};

const char *
TclTypeOpsName(const TclType *tp)
{
    const TclTypeOps *ops = TclTypeOpsOf(tp);

    return (ops != NULL && ops->name != NULL) ? ops->name : NULL;
}

/* 23/11 : une règle type convert s'applique-t-elle à une valeur nue de
 * type tp (any T, string T, ou la validation d'un nominal) ? */
int
TclTypeHasBareRule(Tcl_Interp *interp, const TclType *tp)
{
    TypeData *d;

    if (interp == NULL || tp == NULL) {
	return 0;
    }
    d = GetData(interp);
    if (d->converts.numEntries == 0) {
	return 0;
    }
    return (tp->nominal && tp->under != NULL
	    && FindRule(d, tp->under->name, tp->name) != NULL)
	    || FindRule(d, "any", tp->name) != NULL
	    || FindRule(d, "string", tp->name) != NULL;
}

int
TclTypeCompleteBare(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *content,
	int variantHint, int *variantOut)
{
    TypeData *d;
    Tcl_Obj *rule;
    int i, rc;
    Tcl_InterpState st;

    if (interp == NULL || tp == NULL || TclTypeIsAny(tp)) {
	return TclTypeComplete(interp, tp, content, variantHint, variantOut);
    }
    for (i = 0; i < bareRuleTop; i++) {
	if (bareRuleStack[i] == tp) {
	    return TclTypeComplete(interp, tp, content, variantHint,
		    variantOut);	/* déjà dans sa règle */
	}
    }
    d = GetData(interp);
    if (tp->nominal && tp->under != NULL
	    && (rule = FindRule(d, tp->under->name, tp->name)) != NULL) {
	/* la validation du nominal */
	if (TclTypeComplete(interp, tp->under, content, -1, NULL) != TCL_OK
		|| ApplyBareRule(interp, tp, rule, content) != TCL_OK) {
	    return TCL_ERROR;
	}
	return TclTypeComplete(interp, tp, content, variantHint, variantOut);
    }
    rule = FindRule(d, "any", tp->name);
    if (rule == NULL) {
	rule = FindRule(d, "string", tp->name);
    }
    if (rule == NULL) {
	return TclTypeComplete(interp, tp, content, variantHint, variantOut);
    }
    st = Tcl_SaveInterpState(interp, TCL_OK);
    rc = TclTypeComplete(interp, tp, content, variantHint, variantOut);
    if (rc == TCL_OK) {
	Tcl_DiscardInterpState(st);
	return TCL_OK;
    }
    Tcl_RestoreInterpState(interp, st);
    if (ApplyBareRule(interp, tp, rule, content) != TCL_OK) {
	return TCL_ERROR;		/* le message de la règle */
    }
    return TclTypeComplete(interp, tp, content, variantHint, variantOut);
}

/*
 * Cast de "src" (valeur typée, ou chaîne ordinaire) vers "target".
 * Ordre : même type ; conversion déclarée ; nominal <-> sous-jacent ;
 * conversions intégrées entre int, double et string ; variante d'une
 * somme ; sinon erreur. Ne modifie jamais src.
 */
static int IntDoubleExact(Tcl_Obj *content, double dbl);
static int TypeCastEx(Tcl_Interp *interp, Tcl_Obj *src, TclType *target,
	int implicit, Tcl_Obj **resultPtr);

int
TclTypeCast(Tcl_Interp *interp, Tcl_Obj *src, TclType *target,
	Tcl_Obj **resultPtr)
{
    return TypeCastEx(interp, src, target, 0, resultPtr);
}

/*
 * v17d : la résolution implicite d'une valeur TYPÉE (set, contrat,
 * argument d'une procédure) : le même type, l'élargissement
 * (TclTypeEnters : int -> double, Celsius -> double), la lecture d'une
 * union, la variante d'une somme qui a exactement son type. Les règles
 * (type convert) et les conversions intégrées (troncature, nominal <-
 * base, lecture d'un produit en liste) restent explicites : type cast,
 * {T}$x (ANNOTATIONS § 5.6). TCL_CONTINUE, sans message, quand rien ne
 * s'applique.
 */
int
TclTypeCastImplicit(Tcl_Interp *interp, Tcl_Obj *src, TclType *target,
	Tcl_Obj **resultPtr)
{
    return TypeCastEx(interp, src, target, 1, resultPtr);
}

static int
TypeCastEx(Tcl_Interp *interp, Tcl_Obj *src, TclType *target, int implicit,
	Tcl_Obj **resultPtr)
{
    TypeData *d = GetData(interp);
    Tcl_Obj *content, *res = NULL;
    TclType *from = NULL;
    int variant = -1, outVariant = -1, kind;
    Tcl_HashEntry *hPtr;
    char key[256];
    Tcl_Size i;

    if (TclTypeIsAny(target)) {
	/* v17e : any, un méta-type : la valeur telle quelle, si son état y
	 * est (l'identité) */
	if (TclTypeMetaCheck(interp, target, src) != TCL_OK) {
	    return TCL_ERROR;
	}
	*resultPtr = src;
	return TCL_OK;
    }
    kind = TclAnnotInfo(src, &content, &from, &variant, NULL);
    if (kind == 2) {
	ERR(interp, "CAST", "cannot cast an unresolved annotation");
	return TCL_ERROR;
    }
    if (kind == 0) {
	content = src;
    }
    if (from == target) {
	*resultPtr = src;
	return TCL_OK;
    }
    if (from != NULL && from->variantOf != NULL
	    && from->variantOf == target) {
	/* v18b : S/v vers S : la même valeur, de variante v */
	*resultPtr = TclNewAnnotated(interp, target, (int) from->variantIdx,
		content);
	return TCL_OK;
    }
    if (target->variantOf != NULL && from != NULL && variant >= 0
	    && (from == target->variantOf
		|| from->variantOf == target->variantOf)) {
	/*
	 * v18b : une valeur de S vers S/v. Sa variante est v : elle y est
	 * (la charge restreinte se vérifie). Une autre : un cast écrit passe
	 * par la règle (type convert S/a S/v) ; implicite, c'est une erreur.
	 */
	TclType *root = target->variantOf;
	int vi = (int) target->variantIdx;
	Tcl_Obj *c = NULL;

	if (variant == vi) {
	    if (target->fields[vi].type != root->fields[vi].type
		    && TclTypeValidate(interp, target, content, vi, NULL)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	    *resultPtr = TclNewAnnotated(interp, target, vi, content);
	    return TCL_OK;
	}
	if (implicit) {
	    ERR(interp, "VARIANT", "value \"%s\" is %s/%s, not %s",
		    Tcl_GetString(content), root->name,
		    MemberName(root, variant), target->name);
	    return TCL_ERROR;
	}
	if (TclTypeVariantConvert(interp, root, variant, content, vi, &c)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	*resultPtr = TclNewAnnotated(interp, target, vi, c);
	TclDiscard(c);
	return TCL_OK;
    }
    if (from != NULL && from->kind == TCLT_SUM && !TclSumTagged(from)
	    && target->kind != TCLT_SUM && variant >= 0
	    && variant < from->nfields && from->fields[variant].type != NULL
	    && (target->kind != TCLT_STRING
		|| from->fields[variant].type->kind == TCLT_STRING)) {
	/*
	 * v17z : une somme se lit comme le type de sa variante (la
	 * projection) : le cast part de la charge, dans ce type.
	 */
	TclType *vt = from->fields[variant].type;
	Tcl_Obj *tmp;
	int rc;

	if (vt == target) {
	    *resultPtr = TclNewAnnotated(interp, vt, -1, content);
	    return (*resultPtr != NULL) ? TCL_OK : TCL_ERROR;
	}
	tmp = TclNewAnnotated(interp, vt, -1, content);
	if (tmp == NULL) {
	    return TCL_ERROR;
	}
	Tcl_IncrRefCount(tmp);
	rc = TypeCastEx(interp, tmp, target, implicit, resultPtr);
	if (rc == TCL_OK && *resultPtr == tmp) {
	    tmp->refCount--;		/* (rendu : la référence passe) */
	} else {
	    Tcl_DecrRefCount(tmp);
	}
	return rc;
    }
    if (from != NULL && TclTypeFamily(from) == TclTypeFamily(target)) {
	/*
	 * 24/11 : deux formes d'un même type (type def T DEF {-compact}F).
	 * Implicite : une forme entre dans sa base telle quelle (la
	 * disposition ne change rien pour Tcl) ; vers une forme, il faut un
	 * cast écrit (un contrat de disposition, pour le C). Écrit : la
	 * disposition change (cases <-> objets), les valeurs sont revérifiées.
	 */
	if (implicit) {
	    if (target == TclTypeFamily(target)) {
		*resultPtr = src;
		return TCL_OK;
	    }
	    return TCL_CONTINUE;
	}
	if (TclIsCompact(content) && !target->compactWanted) {
	    Tcl_Obj **ev;
	    Tcl_Size n;

	    if (TclListObjGetElements(interp, content, &n, &ev) != TCL_OK) {
		return TCL_ERROR;
	    }
	    res = Tcl_NewListObj(n, ev);	/* en objets */
	} else {
	    res = Tcl_DuplicateObj(content);
	}
	goto validate;
    }
    if (implicit && from != NULL && TclTypeEnters(target, from)) {
	implicit = 0;			/* élargissement : sans perte */
    }
    /*
     * Variante d'une somme dont le type est exactement celui de la source :
     * une valeur qui convient déjà n'est jamais convertie (v17d : avant
     * les règles).
     */
    if (target->kind == TCLT_SUM && from != NULL) {
	int found = -1, again = -1;

	for (i = 0; i < target->nfields; i++) {
	    if (target->fields[i].type == from) {
		if (found < 0) {
		    found = (int) i;
		} else if (again < 0) {
		    again = (int) i;
		}
	    }
	}
	if (found >= 0 && again >= 0 && TclSumStrict(target)) {
	    ERR(interp, "AMBIGUOUS", "a value of type %s fits variants %s and "
		    "%s of %s: write {/%s}value", from->name,
		    Tcl_GetString(target->fields[found].name),
		    Tcl_GetString(target->fields[again].name), target->name,
		    Tcl_GetString(target->fields[found].name));
	    return TCL_ERROR;
	}
	if (found >= 0) {
	    res = Tcl_DuplicateObj(content);
	    outVariant = found;
	    goto validate;
	}
    }
    if (implicit) {
	return TCL_CONTINUE;		/* v17d : les règles restent explicites */
    }
    /* Conversion déclarée par "type convert". */
    if (from != NULL) {
	snprintf(key, sizeof(key), "%s\1%s", from->name, target->name);
	hPtr = Tcl_FindHashEntry(&d->converts, key);
	if (hPtr != NULL) {
	    res = ApplyConvert(interp, (Tcl_Obj *) Tcl_GetHashValue(hPtr),
		    content);
	    goto validate;
	}
    }
    /* Nominal <-> une de ses bases (toute la chaîne) : même représentation. */
    if (from != NULL && !implicit) {
	const TclType *b;
	int depth;

	for (b = from, depth = 0; b != NULL && depth < 64; b = b->under,
		depth++) {
	    if (b == target) {
		break;
	    }
	}
	if (b == NULL) {
	    for (b = target, depth = 0; b != NULL && depth < 64;
		    b = b->under, depth++) {
		if (b == from) {
		    break;
		}
	    }
	}
	if (b != NULL) {
	    res = Tcl_DuplicateObj(content);
	    goto validate;
	}
    }
    /*
     * v16w : toute valeur se lit comme une chaîne ordinaire : son texte
     * (un littéral d'énumération, "v 5" d'une somme, un point...).
     */
    if (from != NULL && !implicit && TclTypeIsPlainString(target)) {
	res = Tcl_NewStringObj(Tcl_GetString(content), TCL_AUTO_LENGTH);
	goto validate;
    }
    /*
     * v16w : une union se lit comme le type de son alternative : sa valeur
     * est la charge. (Une somme balisée, jamais : sa variante se lit par
     * son chemin, $t(celsius), pour ne pas perdre ce qu'elle dit.)
     */
    if (from != NULL && from->kind == TCLT_SUM && target->kind != TCLT_SUM
	    && from->sumMode == TCLT_SUM_UNION && from->binTag == NULL) {
	int av = variant;

	if (av < 0) {
	    (void) TclTypeValidate(NULL, from, content, -1, &av);
	}
	if (av >= 0 && av < from->nfields && from->fields[av].type != NULL) {
	    Tcl_Obj *cur = TclNewAnnotated(interp, from->fields[av].type, -1,
		    content);
	    int crc;

	    Tcl_IncrRefCount(cur);
	    crc = TypeCastEx(interp, cur, target, implicit, resultPtr);
	    if (crc == TCL_OK && *resultPtr == cur) {
		*resultPtr = TclNewAnnotated(interp, from->fields[av].type,
			-1, content);
	    }
	    Tcl_DecrRefCount(cur);
	    return crc;
	}
    }
    /*
     * Produit -> liste ou dict (cast de base de tout produit) : la liste de
     * ses champs, ou le dict nom -> champ. {list} et {dict} rendent les
     * champs nus (texte ordinaire) ; {list.any} et {dict.any} les rendent
     * annotés de leur type ; {list.T}, {list(N).T}, {dict.T} les valident
     * comme T. Pour une somme, on part de la variante active.
     */
    if (from != NULL && !implicit && (target->kind == TCLT_LIST
	    || target->kind == TCLT_DICT)) {
	const TclType *pt = from;
	Tcl_Obj *pc = content, **fv;
	Tcl_Size nf;

	if (TclSumTagged(pt)) {
	    /* v16w : "v charge" se lit comme la liste de ses deux mots */
	    res = Tcl_DuplicateObj(content);
	    goto validate;
	}
	if (pt->kind == TCLT_SUM && variant >= 0 && variant < (int) pt->nfields
		&& pt->fields[variant].type != NULL) {
	    pt = pt->fields[variant].type;
	}
	if (pt->kind == TCLT_PRODUCT
		&& Tcl_ListObjGetElements(NULL, pc, &nf, &fv) == TCL_OK
		&& nf == pt->nfields) {
	    int annotated = (target->elem != NULL && TclTypeIsAny(target->elem));

	    res = (target->kind == TCLT_LIST) ? Tcl_NewListObj(nf, NULL)
		    : Tcl_NewDictObj();
	    for (i = 0; i < nf; i++) {
		TclType *ft = pt->fields[i].type;
		Tcl_Obj *fc = TCL_ANNOT_CONTENT(fv[i]), *e;

		if (annotated && ft != NULL) {
		    int fvar = -1;

		    if (ft->kind == TCLT_SUM) {
			TclType *own = NULL;

			if (TclAnnotInfo(fv[i], NULL, &own, &fvar, NULL) != 1
				|| own != ft) {
			    fvar = -1;
			    (void) TclTypeValidate(NULL, ft, fc, -1, &fvar);
			}
		    }
		    e = TclNewAnnotated(interp, ft, fvar, fc);
		} else {
		    e = Tcl_DuplicateObj(fc);
		}
		if (target->kind == TCLT_LIST) {
		    Tcl_ListObjAppendElement(NULL, res, e);
		} else {
		    Tcl_Obj *k = pt->fields[i].name ? pt->fields[i].name
			    : Tcl_ObjPrintf("%" TCL_SIZE_MODIFIER "d", i);

		    Tcl_DictObjPut(NULL, res, k, e);
		}
	    }
	    goto validate;
	}
    }
    /*
     * Liste -> liste, dict -> dict, quand l'un des deux côtés au moins est
     * structurel (polygone -> list.point, list(4).point, list) : même
     * contenu, revalidé pour la cible. Deux nominaux sans lien restent
     * refusés plus bas. {list} rend les éléments nus ; {list.any} les rend
     * annotés du type d'élément de la source.
     */
    if (from != NULL && !implicit && from->kind == target->kind
	    && (target->kind == TCLT_LIST || target->kind == TCLT_DICT)
	    && !(from->nominal && target->nominal)) {
	Tcl_Obj **ev;
	Tcl_Size ne;
	int annotated = (target->elem != NULL && TclTypeIsAny(target->elem));

	if (target->kind == TCLT_LIST && !annotated
		&& (res = TclCompactCast(target, content)) != NULL) {
	    goto validate;		/* écrit directement en cases */
	}
	if (target->kind == TCLT_LIST
		&& Tcl_ListObjGetElements(NULL, content, &ne, &ev) == TCL_OK) {
	    res = Tcl_NewListObj(ne, NULL);
	    for (i = 0; i < ne; i++) {
		Tcl_Obj *e;

		if (annotated) {
		    e = TclIsAnnotated(ev[i]) || from->elem == NULL
			    || TclTypeIsAny(from->elem)
			    ? Tcl_DuplicateObj(ev[i])
			    : TclNewAnnotated(interp, from->elem, -1, ev[i]);
		} else {
		    e = Tcl_DuplicateObj(TCL_ANNOT_CONTENT(ev[i]));
		}
		Tcl_ListObjAppendElement(NULL, res, e);
	    }
	    goto validate;
	}
	if (target->kind == TCLT_DICT) {
	    res = Tcl_DuplicateObj(content);
	    goto validate;
	}
    }
    /*
     * Règle de FAMILLE (type convert grandeur grandeur ...), entre deux
     * instances différentes : un changement d'unité. Deux nominaux sur la
     * même instance (couple, energie) ne relèvent pas de la règle.
     */
    if (from != NULL) {
	TclType *fi = FamilyOf(from), *ti = FamilyOf(target);

	if ((fi != NULL || ti != NULL)
		&& !(fi != NULL && ti != NULL && SameInstance(fi, ti))) {
	    char fk[3][160], tk[3][160];
	    int nf = 0, nt = 0, a, b, frc;
	    Tcl_DString fkey;

	    if (fi != NULL) {
		snprintf(fk[nf++], sizeof(fk[0]), "@%s", fi->family->name);
	    }
	    snprintf(fk[nf++], sizeof(fk[0]), "%s", from->name);
	    snprintf(fk[nf++], sizeof(fk[0]), "%s",
		    TclTypeGenericOf(from)->name);
	    if (ti != NULL) {
		snprintf(tk[nt++], sizeof(tk[0]), "@%s", ti->family->name);
	    }
	    snprintf(tk[nt++], sizeof(tk[0]), "%s", target->name);
	    snprintf(tk[nt++], sizeof(tk[0]), "%s",
		    TclTypeGenericOf(target)->name);
	    for (a = 0; a < nf; a++) {
		for (b = 0; b < nt; b++) {
		    if (fk[a][0] != '@' && tk[b][0] != '@') {
			continue;	/* pas une règle de famille */
		    }
		    Tcl_DStringInit(&fkey);
		    Tcl_DStringAppend(&fkey, fk[a], TCL_AUTO_LENGTH);
		    Tcl_DStringAppend(&fkey, "\1", 1);
		    Tcl_DStringAppend(&fkey, tk[b], TCL_AUTO_LENGTH);
		    hPtr = Tcl_FindHashEntry(&d->converts,
			    Tcl_DStringValue(&fkey));
		    Tcl_DStringFree(&fkey);
		    if (hPtr == NULL) {
			continue;
		    }
		    frc = ApplyFamily(interp, (Tcl_Obj *) Tcl_GetHashValue(hPtr),
			    content, fk[a][0] == '@' ? fi : NULL,
			    tk[b][0] == '@' ? ti : NULL, &res);
		    if (frc == TCL_OK) {
			goto validate;
		    }
		    if (frc == TCL_ERROR) {
			return TCL_ERROR;
		    }
		}
	    }
	}
    }

    /*
     * Pas de règle spécifique : on se ramène aux types GÉNÉRIQUES. D'abord
     * une conversion déclarée entre eux, puis les conversions intégrées
     * entre scalaires.
     */
    if (from != NULL) {
	const TclType *gf = TclTypeGenericOf(from);
	const TclType *gt = TclTypeGenericOf(target);

	if (gf != from || gt != target) {
	    snprintf(key, sizeof(key), "%s\1%s", gf->name, gt->name);
	    hPtr = Tcl_FindHashEntry(&d->converts, key);
	    if (hPtr != NULL) {
		res = ApplyConvert(interp,
			(Tcl_Obj *) Tcl_GetHashValue(hPtr), content);
		goto validate;
	    }
	}
    }
    if (implicit) {
	return TCL_CONTINUE;		/* rien d'implicite : à l'appelant */
    }
    /*
     * Deux nominaux d'auteur sans lien (mm et kg, mm et px) : seule une
     * conversion déclarée les relie. Les conversions intégrées ne valent
     * que si l'un des deux côtés est structurel (mm -> int, double -> mm).
     */
    if (from != NULL && from->nominal && !from->refine && target->nominal
	    && !target->refine) {
	ERR(interp, "CAST", "unknown way to cast the value from %s to %s",
		from->name, target->name);
	return TCL_ERROR;
    }
    /* Conversions intégrées entre scalaires. */
    {
	TclTypeKind fk = (from != NULL) ? from->kind : TCLT_STRING;

	if ((fk == TCLT_INT || fk == TCLT_DOUBLE || fk == TCLT_STRING)
		&& (target->kind == TCLT_INT || target->kind == TCLT_DOUBLE
		    || target->kind == TCLT_STRING)) {
	    if (target->kind == TCLT_INT && fk == TCLT_DOUBLE) {
		res = TruncateToInt(interp, content);
		if (res == NULL) {
		    return TCL_ERROR;
		}
	    } else if (target->kind == TCLT_DOUBLE && fk == TCLT_INT) {
		double dbl;

		if (Tcl_GetDoubleFromObj(interp, content, &dbl) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (!(target->reserved && strcmp(target->name, "inexact") == 0)
			&& !IntDoubleExact(content, dbl)) {
		    /* v17z : exact, ou une erreur ; l'arrondi se demande */
		    ERR(interp, "INEXACT", "the integer %s has no exact double "
			    "(beyond 2^53): to round it, cast it to inexact "
			    "(type cast inexact $n)", Tcl_GetString(content));
		    return TCL_ERROR;
		}
		res = Tcl_NewDoubleObj(dbl);
	    } else if (target->kind == TCLT_STRING) {
		res = Tcl_NewStringObj(Tcl_GetString(content), TCL_AUTO_LENGTH);
	    } else {
		res = Tcl_DuplicateObj(content);	/* INT<-INT, STRING<-... */
	    }
	    goto validate;
	}
    }
    if (from != NULL && TclSumTagged(from) && target->kind != TCLT_SUM) {
	int sv = TclSumDecode(from, content, NULL);

	if (sv >= 0 && from->fields[sv].type != NULL) {
	    /* v16w : la variante se lit par son nom, pas en silence */
	    ERR(interp, "CAST", "a value of the sum %s is not of type %s: it is "
		    "\"%s\" then its value; read that value by its path, as "
		    "$v(%s)", from->name, target->name,
		    Tcl_GetString(from->fields[sv].name),
		    Tcl_GetString(from->fields[sv].name));
	    return TCL_ERROR;
	}
    }
    ERR(interp, "CAST", "unknown way to cast the value from %s to %s",
	    from ? from->name : "string", target->name);
    return TCL_ERROR;

  validate:
    if (res == NULL) {
	return TCL_ERROR;
    }
    if (TclTypeComplete(interp, target, res, outVariant, &variant)
	    != TCL_OK) {
	TclDiscard(res);
	return TCL_ERROR;
    }
    *resultPtr = TclNewAnnotated(interp, target, variant, res);
    TclDiscard(res);
    return TCL_OK;
}

/*
 * D'une variante à une autre de la même somme : le contenu de la variante
 * "from" lu comme variante "to". Dans l'ordre : une règle "type convert
 * S.from S.to" ; refus si les deux variantes ont la même forme (un cast
 * identité dirait qu'un celsius est un fahrenheit) ; sinon le cast
 * ordinaire du type de "from" vers celui de "to". *resPtr : contenu neuf,
 * valide pour "to" (sans annotation).
 */
/* v17z : une valeur de type from (un int) qui entre dans tp (un double, pas
 * inexact) : exacte, ou une erreur. */
int
TclTypeIntToDoubleCheck(Tcl_Interp *interp, const TclType *tp,
	const TclType *from, Tcl_Obj *content)
{
    double dbl;

    if (tp == NULL || from == NULL || tp->kind != TCLT_DOUBLE
	    || from->kind != TCLT_INT
	    || (tp->reserved && strcmp(tp->name, "inexact") == 0)) {
	return TCL_OK;
    }
    content = TCL_ANNOT_CONTENT(content);
    if (Tcl_GetDoubleFromObj(NULL, content, &dbl) != TCL_OK
	    || IntDoubleExact(content, dbl)) {
	return TCL_OK;
    }
    ERR(interp, "INEXACT", "the integer %s has no exact double (beyond "
	    "2^53): to round it, cast it to inexact (type cast inexact $n)",
	    Tcl_GetString(content));
    return TCL_ERROR;
}

/* v17z : le double dbl vaut-il exactement l'entier content ? */
static int
IntDoubleExact(Tcl_Obj *content, double dbl)
{
    void *cd;
    int nt;
    char buf[400];
    Tcl_Obj *big = NULL;
    int same;

    if (fabs(dbl) < 9007199254740992.0) {
	return 1;			/* sous 2^53 : tout entier y est exact */
    }
    if (Tcl_GetNumberFromObj(NULL, content, &cd, &nt) != TCL_OK) {
	return 1;			/* (le message viendra d'ailleurs) */
    }
    if (isinf(dbl) || isnan(dbl)) {
	return 0;
    }
    snprintf(buf, sizeof(buf), "%.0f", dbl);
    if (nt == TCL_NUMBER_INT) {
	char wb[32];

	snprintf(wb, sizeof(wb), "%" TCL_LL_MODIFIER "d",
		(long long) *(Tcl_WideInt *) cd);
	return strcmp(wb, buf) == 0;
    }
    if (nt != TCL_NUMBER_BIG) {
	return 1;
    }
    {
	mp_int bi;

	if (Tcl_GetBignumFromObj(NULL, content, &bi) != TCL_OK) {
	    return 1;
	}
	big = Tcl_NewBignumObj(&bi);
	Tcl_IncrRefCount(big);
	same = (strcmp(Tcl_GetString(big), buf) == 0);
	Tcl_DecrRefCount(big);
	return same;
    }
}

/*
 * v17z : type include U T : toute valeur de T entre dans U sans cast, sans
 * vérification qui puisse échouer, et reste égale à elle-même (au sens de
 * ==). Voir doc/CONCEPTION-sommes-num.md, section 4.
 */
static int TypeIncludes(const TclType *u, const TclType *t, int depth);
static int MayOverlap(const TclType *a, const TclType *b, int depth);

static int
PlainString(const TclType *u)
{
    return u->kind == TCLT_STRING && !u->refine && !u->nominal
	    && u->binKind == TCLBIN_NONE && u->pred == NULL;
}

static int
TypeIncludes(const TclType *u, const TclType *t, int depth)
{
    Tcl_Size i, j;

    if (u == NULL || t == NULL || depth > 64) {
	return 0;
    }
    if (u == t || TclTypeIsAny(u) || PlainString(u)) {
	return 1;
    }
    if (t->formOf != NULL && t->formOf == u) {
	return 1;			/* une forme de U est un U */
    }
    if (t->kind == TCLT_SUM && u->kind == TCLT_SUM) {
	/* chaque variante de T, sous son nom, dans U, charge incluse ;
	 * chaque littéral de T parmi ceux de U */
	if (u->variantOf != NULL && t->variantOf == NULL) {
	    return 0;			/* v18b : S n'entre pas dans S/v */
	}
	for (i = 0; i < t->nfields; i++) {
	    const TclTypeField *f = &t->fields[i];
	    int found = 0;

	    if (t->variantOf != NULL && i != t->variantIdx) {
		continue;		/* v18b : S/v n'a que v */
	    }
	    for (j = 0; j < u->nfields && !found; j++) {
		const TclTypeField *g = &u->fields[j];

		if (u->variantOf != NULL && j != u->variantIdx) {
		    continue;
		}

		if (f->type == NULL) {
		    found = (g->type == NULL && g->literal != NULL
			    && f->literal != NULL && strcmp(Tcl_GetString(
			    g->literal), Tcl_GetString(f->literal)) == 0);
		} else {
		    found = (g->type != NULL && f->name != NULL
			    && g->name != NULL && strcmp(Tcl_GetString(g->name),
			    Tcl_GetString(f->name)) == 0
			    && TypeIncludes(g->type, f->type, depth + 1));
		}
	    }
	    if (!found) {
		return 0;
	    }
	}
	return 1;
    }
    if (t->kind == TCLT_SUM) {
	/* une somme se lit comme sa variante : chacune doit entrer */
	for (i = 0; i < t->nfields; i++) {
	    if (t->fields[i].type != NULL
		    ? !TypeIncludes(u, t->fields[i].type, depth + 1)
		    : TclTypeValidate(NULL, u, t->fields[i].literal, -1, NULL)
		    != TCL_OK) {
		return 0;
	    }
	}
	return 1;
    }
    if (u->kind == TCLT_SUM) {
	/* une union : une alternative l'inclut ; une somme : une seule
	 * variante peut prendre ses valeurs (sinon la déduction échoue) */
	int in = 0, other = 0;

	for (i = 0; i < u->nfields; i++) {
	    const TclType *vt = u->fields[i].type;

	    if (vt == NULL) {
		continue;
	    }
	    if (TypeIncludes(vt, t, depth + 1)) {
		in++;
	    } else if (MayOverlap(vt, t, 0)) {
		other++;
	    }
	}
	if (u->sumMode == TCLT_SUM_UNION) {
	    return in > 0;
	}
	return in == 1 && other == 0;
    }
    /* un prédicat, un nominal, un raffinement : leur base les inclut */
    if (u->pred != NULL) {
	return 0;			/* (U == T : vu plus haut) */
    }
    if (t->pred != NULL && t->predBase != NULL) {
	return TypeIncludes(u, t->predBase, depth + 1);
    }
    if (t->nominal && t->under != NULL && !t->refine) {
	return TypeIncludes(u, t->under, depth + 1);
    }
    if (u->nominal && !u->refine) {
	return 0;			/* deux nominaux sans lien */
    }
    switch (u->kind) {
    case TCLT_INT:
	return t->kind == TCLT_INT && TclTypeAssignable(u, t);
    case TCLT_DOUBLE:
	if (t->kind == TCLT_INT) {
	    /* exact jusqu'à 53 bits */
	    return (t->binKind == TCLBIN_SINT || t->binKind == TCLBIN_UINT)
		    && t->binSize > 0 && t->binSize * 8 <= 53;
	}
	return t->kind == TCLT_DOUBLE && TclTypeAssignable(u, t);
    case TCLT_LIST:
	if (t->kind != TCLT_LIST) {
	    return 0;
	}
	if (u->arity >= 0 && u->arity != t->arity) {
	    return 0;
	}
	return u->elem == NULL || TypeIncludes(u->elem, t->elem, depth + 1);
    case TCLT_DICT:
	return t->kind == TCLT_DICT
		&& TypeIncludes(u->key, t->key, depth + 1)
		&& TypeIncludes(u->elem, t->elem, depth + 1);
    case TCLT_PRODUCT:
	if (t->kind != TCLT_PRODUCT || t->nfields != u->nfields) {
	    return 0;
	}
	for (i = 0; i < u->nfields; i++) {
	    if ((u->fields[i].name == NULL) != (t->fields[i].name == NULL)
		    || (u->fields[i].name != NULL && strcmp(Tcl_GetString(
		    u->fields[i].name), Tcl_GetString(t->fields[i].name)) != 0)
		    || !TypeIncludes(u->fields[i].type, t->fields[i].type,
		    depth + 1)) {
		return 0;
	    }
	}
	return 1;
    case TCLT_VAR:
	return t->kind == TCLT_VAR && u->elem == t->elem
		&& u->varTyped == t->varTyped;
    default:
	return 0;
    }
}

/* v17z : un type, ou T.v (la variante v d'une somme T, comme le rend
 * type get) : la somme T. NULL et un message sinon. */
static TclType *
ParseTypeOrVariant(Tcl_Interp *interp, const char *tt, Tcl_Size tl)
{
    Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);
    TclType *tp = (tl > 0) ? TclTypeParse(interp, tt, tl) : NULL;
    const char *dot = NULL, *q;

    if (tp != NULL) {
	Tcl_DiscardInterpState(st);
	return tp;
    }
    for (q = tt; q < tt + tl; q++) {
	if (*q == '/') {
	    dot = q;
	}
    }
    if (dot != NULL && dot > tt) {
	Tcl_InterpState st2 = Tcl_SaveInterpState(interp, TCL_OK);
	TclType *sp = TclTypeParse(interp, tt, dot - tt);

	Tcl_RestoreInterpState(interp, st2);
	if (sp != NULL && sp->kind == TCLT_SUM) {
	    Tcl_Obj *nm = Tcl_NewStringObj(dot + 1, tt + tl - dot - 1);
	    Tcl_Size k;

	    Tcl_IncrRefCount(nm);
	    k = TclTypeFieldIndex(sp, Tcl_GetString(nm));
	    Tcl_DecrRefCount(nm);
	    if (k >= 0) {
		Tcl_DiscardInterpState(st);
		return sp;
	    }
	}
    }
    Tcl_RestoreInterpState(interp, st);
    return (tl > 0) ? TclTypeParse(interp, tt, tl) : NULL;	/* le message */
}

/* v17z : le texte est-il un type ? (sans message) */
static int
TypeParsesQuietly(Tcl_Interp *interp, const char *tt, Tcl_Size tl)
{
    Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);
    int ok = (TclTypeParse(interp, tt, tl) != NULL);

    Tcl_RestoreInterpState(interp, st);
    return ok;
}

/* v17z : l'indice du littéral NAME d'une somme, ou -1 */
static Tcl_Size
SumLiteralIndex(const TclType *tp, Tcl_Obj *name)
{
    Tcl_Size i;

    for (i = 0; i < tp->nfields; i++) {
	if (tp->fields[i].type == NULL && tp->fields[i].literal != NULL
		&& strcmp(Tcl_GetString(tp->fields[i].literal),
		Tcl_GetString(name)) == 0) {
	    return i;
	}
    }
    return -1;
}

/* v17z : une règle type convert S.from S.to existe-t-elle ? */
static int
TclTypeHasVariantRule(Tcl_Interp *interp, TclType *sum, int from, int to)
{
    TypeData *d = GetData(interp);
    char key[512];

    if (sum->fields[from].name == NULL || sum->fields[to].name == NULL) {
	return 0;
    }
    snprintf(key, sizeof(key), "%s/%s\1%s/%s", sum->name,
	    Tcl_GetString(sum->fields[from].name), sum->name,
	    Tcl_GetString(sum->fields[to].name));
    return Tcl_FindHashEntry(&d->converts, key) != NULL;
}

int
TclTypeVariantConvert(Tcl_Interp *interp, TclType *sum, int from,
	Tcl_Obj *content, int to, Tcl_Obj **resPtr)
{
    TypeData *d = GetData(interp);
    TclTypeField *ff, *tf;
    Tcl_HashEntry *hPtr;
    Tcl_Obj *res = NULL;
    const char *fn, *tn;
    char key[512];

    if (sum == NULL || sum->kind != TCLT_SUM || from < 0 || to < 0
	    || from >= (int) sum->nfields || to >= (int) sum->nfields) {
	ERR(interp, "VARIANT", "no such variant");
	return TCL_ERROR;
    }
    ff = &sum->fields[from];
    tf = &sum->fields[to];
    fn = MemberName(sum, from);		/* v18b : un littéral aussi */
    tn = MemberName(sum, to);
    if (from == to) {
	*resPtr = Tcl_DuplicateObj(content);
	return TCL_OK;
    }
    snprintf(key, sizeof(key), "%s/%s\1%s/%s", sum->name, fn, sum->name, tn);
    hPtr = Tcl_FindHashEntry(&d->converts, key);
    if (hPtr != NULL) {
	res = ApplyConvert(interp, (Tcl_Obj *) Tcl_GetHashValue(hPtr),
		content);
	if (res == NULL) {
	    return TCL_ERROR;
	}
    } else if (VariantIncludes(ff, tf) && VariantIncludes(tf, ff)) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "the current variant of %s is \"%s\", not \"%s\"; both "
		    "have the same form, so reading one as the other needs a "
		    "rule: type convert %s/%s %s/%s {{x} ...}", sum->name, fn,
		    tn, sum->name, fn, sum->name, tn));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "VARIANT", (char *) NULL);
	}
	return TCL_ERROR;
    } else if (ff->type != NULL && tf->type != NULL) {
	Tcl_Obj *cur = TclNewAnnotated(interp, ff->type, -1, content), *out;

	Tcl_IncrRefCount(cur);
	if (TclTypeCast(interp, cur, tf->type, &out) != TCL_OK) {
	    Tcl_DecrRefCount(cur);
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"the current variant of %s is \"%s\", not \"%s\", and "
			"its value \"%s\" can't be read as %s", sum->name, fn,
			tn, Tcl_GetString(content), tf->type->name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "VARIANT",
			(char *) NULL);
	    }
	    return TCL_ERROR;
	}
	Tcl_IncrRefCount(out);
	res = Tcl_DuplicateObj(TCL_ANNOT_CONTENT(out));
	Tcl_DecrRefCount(out);
	Tcl_DecrRefCount(cur);
    } else {
	ERR(interp, "VARIANT", "the current variant of %s is \"%s\", not "
		"\"%s\", and its value \"%s\" can't be read as that variant",
		sum->name, fn, tn, Tcl_GetString(content));
	return TCL_ERROR;
    }
    if ((TclSumTagged(sum) ? (tf->type != NULL && TclTypeValidate(interp,
	    tf->type, res, -1, NULL) != TCL_OK)
	    : TclTypeValidate(interp, sum, res, to, NULL) != TCL_OK)) {
	/* v16w : une somme balisée : res est la charge de la variante */
	TclDiscard(res);
	return TCL_ERROR;
    }
    *resPtr = res;
    return TCL_OK;
}


/* ====================================================================
 * type {c!}export T ?T ...? : en-tête C produit depuis les types
 * ====================================================================
 *
 * Un typedef par type nommé, dans l'ordre des dépendances ; produits en
 * struct, sommes de littéraux en enum, sommes en struct {int tag; union},
 * S($champ) en union seule, list(N).T en tableau, list($n).T final en
 * membre flexible ; string, list.T, dict et liens en Tcl_Obj *. Chaque
 * struct est suivi de _Static_assert sur sizeof et offsetof, calculés ici
 * avec les alignements de TclTypeAbiAlign : si le compilateur de
 * l'extension range autrement, la compilation échoue.
 */

typedef struct CGen {
    Tcl_Interp *interp;
    Tcl_DString defs;		/* les typedef, dans l'ordre */
    Tcl_HashTable done;		/* TclType * -> CGenNamed * */
    int needTcl;
    int depth;
    TclType *self;		/* typedef en cours d'un scalaire ou tableau
				 * nommé : sa propre déclaration, pas son nom */
} CGen;

typedef struct CGenNamed {
    char *cname;
    size_t size, align;
    int flex;			/* finit par un membre flexible */
    int busy;			/* en cours (récursion) */
} CGenNamed;

static size_t
CAlignUp(size_t x, size_t a)
{
    return (x + a - 1) / a * a;
}

/* Nom C : ASCII, accents usuels translittérés, le reste en "_". */
static void
CIdent(const char *s, Tcl_DString *ds)
{
    static const char *const kw[] = {
	"auto", "break", "case", "char", "const", "continue", "default", "do",
	"double", "else", "enum", "extern", "float", "for", "goto", "if",
	"inline", "int", "long", "register", "restrict", "return", "short",
	"signed", "sizeof", "static", "struct", "switch", "typedef", "union",
	"unsigned", "void", "volatile", "while", "bool", "true", "false", NULL
    };
    Tcl_Size start = Tcl_DStringLength(ds);
    const unsigned char *p = (const unsigned char *) s;
    int i;

    while (p[0] == ':' && p[1] == ':') {
	p += 2;			/* v16u : ::geom::point -> geom_point */
    }
    if (*p >= '0' && *p <= '9') {
	Tcl_DStringAppend(ds, "_", 1);
    }
    while (*p) {
	if (p[0] == ':' && p[1] == ':') {
	    Tcl_DStringAppend(ds, "_", 1);
	    p += 2;
	    continue;
	}
	if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
		|| (*p >= '0' && *p <= '9') || *p == '_') {
	    Tcl_DStringAppend(ds, (const char *) p, 1);
	    p++;
	} else if (*p == 0xC3 && p[1] != 0) {
	    static const char map[] =	/* U+00C0 .. U+00FF */
		"AAAAAAACEEEEIIII_NOOOOO_OUUUUY__aaaaaaaceeeeiiii_nooooo_ouuuuy_y";
	    unsigned c = p[1];
	    char ch = (c >= 0x80 && c <= 0xBF) ? map[c - 0x80 + 0] : '_';

	    if (c >= 0x80 && c <= 0xBF) {
		ch = map[(c - 0x80) + 0];
	    }
	    Tcl_DStringAppend(ds, &ch, 1);
	    p += 2;
	} else {
	    Tcl_DStringAppend(ds, "_", 1);
	    p++;
	    while ((*p & 0xC0) == 0x80) {
		p++;			/* reste d'un caractère UTF-8 */
	    }
	}
    }
    for (i = 0; kw[i] != NULL; i++) {
	if (strcmp(Tcl_DStringValue(ds) + start, kw[i]) == 0) {
	    Tcl_DStringAppend(ds, "_", 1);
	    break;
	}
    }
}

/* Un type à nommer (typedef) : défini par type def, sans paramètres. */
static int
CIsNamed(const TclType *tp)
{
    return tp->nominal && !tp->reserved && strchr(tp->name, '(') == NULL
	    && strchr(tp->name, '<') == NULL && tp->binTag == NULL;
}

static int CDecl(CGen *g, TclType *tp, const char *name, int last,
	Tcl_DString *out, size_t *sizePtr, size_t *alignPtr, int *flexPtr,
	const char *where);
static CGenNamed *CEmitNamed(CGen *g, TclType *tp);

static int
CErr(CGen *g, const char *where, const char *msg, const char *tname)
{
    Tcl_SetObjResult(g->interp, Tcl_ObjPrintf("type {c!}export: %s%s%s%s", where,
	    *where ? ": " : "", msg, tname ? tname : ""));
    Tcl_SetErrorCode(g->interp, "TCL", "TYPE", "C", (char *) NULL);
    return TCL_ERROR;
}

/* Scalaire : texte du type C, taille, alignement ; 0 si ce n'en est pas un,
 * -1 après une erreur. */
static int
CScalar(CGen *g, const TclType *tp, Tcl_DString *base, size_t *sz,
	size_t *al, const char *where)
{
    union { uint16_t u; unsigned char c[2]; } bo;
    int native;

    bo.u = 1;
    native = bo.c[0] ? TCLBIN_LE : TCLBIN_BE;
    if (tp->kind != TCLT_INT && tp->kind != TCLT_DOUBLE) {
	return 0;
    }
    if (tp->binEnd != TCLBIN_AUTO && tp->binEnd != native) {
	CErr(g, where, "the byte order differs from this machine's: ",
		tp->name);
	return -1;
    }
    if (tp->bitWidth != 0) {
	CErr(g, where, "a bitarray has no C struct form: ", tp->name);
	return -1;
    }
    if (tp->kind == TCLT_DOUBLE) {
	int b = (tp->binKind == TCLBIN_FLOAT) ? (int) tp->binSize * 8 : 64;

	Tcl_DStringAppend(base, b == 32 ? "float" : "double", -1);
	*sz = (size_t) b / 8;
	*al = (size_t) TclTypeAbiAlign(1, b);
	return 1;
    }
    if ((tp->binKind == TCLBIN_SINT || tp->binKind == TCLBIN_UINT)
	    && (tp->binSize == 1 || tp->binSize == 2 || tp->binSize == 4
		|| tp->binSize == 8)) {
	char buf[16];

	snprintf(buf, sizeof(buf), "%sint%d_t",
		tp->binKind == TCLBIN_UINT ? "u" : "", (int) tp->binSize * 8);
	Tcl_DStringAppend(base, buf, -1);
	*sz = (size_t) tp->binSize;
	*al = (size_t) TclTypeAbiAlign(0, (int) tp->binSize * 8);
	return 1;
    }
    CErr(g, where, "int has no fixed size in C: use int(s,n) or int(u,n) "
	    "with n = 8, 16, 32 or 64, not ", tp->name);
    return -1;
}

/* Corps d'un struct pour un produit ; rend taille, alignement, flex. */
static int
CStructBody(CGen *g, TclType *tp, Tcl_DString *out, size_t *sz, size_t *al,
	int *flex, Tcl_DString *asserts, const char *cname, const char *prefix)
{
    size_t off = 0, align = 1;
    Tcl_Size i;
    int fl = 0;

    Tcl_DStringAppend(out, "struct {\n", -1);
    for (i = 0; i < tp->nfields; i++) {
	Tcl_DString fn, decl, where;
	size_t fs, fa;
	int ff = 0;

	Tcl_DStringInit(&fn);
	Tcl_DStringInit(&decl);
	Tcl_DStringInit(&where);
	CIdent(Tcl_GetString(tp->fields[i].name), &fn);
	Tcl_DStringAppend(&where, tp->name, -1);
	Tcl_DStringAppend(&where, ".", 1);
	Tcl_DStringAppend(&where, Tcl_GetString(tp->fields[i].name), -1);
	if (CDecl(g, tp->fields[i].type, Tcl_DStringValue(&fn),
		i == tp->nfields - 1, &decl, &fs, &fa, &ff,
		Tcl_DStringValue(&where)) != TCL_OK) {
	    Tcl_DStringFree(&fn);
	    Tcl_DStringFree(&decl);
	    Tcl_DStringFree(&where);
	    return TCL_ERROR;
	}
	off = CAlignUp(off, fa);
	if (fa > align) {
	    align = fa;
	}
	Tcl_DStringAppend(out, "    ", 4);
	Tcl_DStringAppend(out, Tcl_DStringValue(&decl), -1);
	Tcl_DStringAppend(out, ";\n", 2);
	if (asserts != NULL) {
	    char buf[64];

	    snprintf(buf, sizeof(buf), "%llu", (unsigned long long) off);
	    Tcl_DStringAppend(asserts, "_Static_assert(offsetof(", -1);
	    Tcl_DStringAppend(asserts, cname, -1);
	    Tcl_DStringAppend(asserts, ", ", 2);
	    Tcl_DStringAppend(asserts, prefix, -1);
	    Tcl_DStringAppend(asserts, Tcl_DStringValue(&fn), -1);
	    Tcl_DStringAppend(asserts, ") == ", -1);
	    Tcl_DStringAppend(asserts, buf, -1);
	    Tcl_DStringAppend(asserts, ", \"", 3);
	    Tcl_DStringAppend(asserts, Tcl_DStringValue(&where), -1);
	    Tcl_DStringAppend(asserts, ": offset\");\n", -1);
	}
	off += fs;
	fl = ff;
	Tcl_DStringFree(&fn);
	Tcl_DStringFree(&decl);
	Tcl_DStringFree(&where);
    }
    Tcl_DStringAppend(out, "}", 1);
    *sz = CAlignUp(off, align);
    *al = align;
    *flex = fl;
    return TCL_OK;
}

/* Une somme : enum (littéraux seuls), union (S($champ)), ou struct
 * {int tag; union}. Les noms de variantes en énumérateurs NOM_variante. */
static int
CSumBody(CGen *g, TclType *tp, const char *cname, Tcl_DString *out,
	size_t *sz, size_t *al, const char *where)
{
    Tcl_Size i;
    int lits = 1, unionOnly = (tp->binTag != NULL);
    size_t us = 0, ua = 1;
    Tcl_DString un;

    for (i = 0; i < tp->nfields; i++) {
	if (tp->fields[i].type != NULL) {
	    lits = 0;
	}
    }
    if (lits) {
	Tcl_DStringAppend(out, "enum {", -1);
	for (i = 0; i < tp->nfields; i++) {
	    Tcl_DStringAppend(out, i ? ", " : " ", -1);
	    CIdent(cname, out);
	    Tcl_DStringAppend(out, "_", 1);
	    CIdent(Tcl_GetString(tp->fields[i].literal), out);
	}
	Tcl_DStringAppend(out, " }", -1);
	*sz = sizeof(int);
	*al = (size_t) TclTypeAbiAlign(0, 32);
	return TCL_OK;
    }
    Tcl_DStringInit(&un);
    Tcl_DStringAppend(&un, "union {\n", -1);
    for (i = 0; i < tp->nfields; i++) {
	Tcl_DString fn, decl;
	size_t fs, fa;
	int ff = 0;
	char vbuf[32];

	if (tp->fields[i].type == NULL) {
	    continue;			/* variante littérale : la balise seule */
	}
	Tcl_DStringInit(&fn);
	Tcl_DStringInit(&decl);
	if (tp->fields[i].name != NULL) {
	    CIdent(Tcl_GetString(tp->fields[i].name), &fn);
	} else {
	    snprintf(vbuf, sizeof(vbuf), "v%d", (int) i + 1);
	    Tcl_DStringAppend(&fn, vbuf, -1);
	}
	if (CDecl(g, tp->fields[i].type, Tcl_DStringValue(&fn), 0, &decl,
		&fs, &fa, &ff, where) != TCL_OK) {
	    Tcl_DStringFree(&fn);
	    Tcl_DStringFree(&decl);
	    Tcl_DStringFree(&un);
	    return TCL_ERROR;
	}
	Tcl_DStringAppend(&un, "        ", -1);
	Tcl_DStringAppend(&un, Tcl_DStringValue(&decl), -1);
	Tcl_DStringAppend(&un, ";\n", 2);
	if (fs > us) {
	    us = fs;
	}
	if (fa > ua) {
	    ua = fa;
	}
	Tcl_DStringFree(&fn);
	Tcl_DStringFree(&decl);
    }
    Tcl_DStringAppend(&un, "    }", -1);
    us = CAlignUp(us, ua);
    if (unionOnly) {
	/* S($champ) : la variante est donnée par le champ frère */
	Tcl_DStringAppend(out, Tcl_DStringValue(&un), -1);
	*sz = us;
	*al = ua;
    } else {
	size_t ta = (size_t) TclTypeAbiAlign(0, 32);
	size_t uoff = CAlignUp(sizeof(int), ua);

	Tcl_DStringAppend(out, "struct {\n    int tag;    /* ", -1);
	for (i = 0; i < tp->nfields; i++) {
	    Tcl_DStringAppend(out, i ? ", " : "", -1);
	    CIdent(cname, out);
	    Tcl_DStringAppend(out, "_", 1);
	    if (tp->fields[i].name != NULL) {
		CIdent(Tcl_GetString(tp->fields[i].name), out);
	    } else if (tp->fields[i].literal != NULL) {
		CIdent(Tcl_GetString(tp->fields[i].literal), out);
	    } else {
		char vbuf[32];

		snprintf(vbuf, sizeof(vbuf), "v%d", (int) i + 1);
		Tcl_DStringAppend(out, vbuf, -1);
	    }
	}
	Tcl_DStringAppend(out, " */\n    ", -1);
	Tcl_DStringAppend(out, Tcl_DStringValue(&un), -1);
	Tcl_DStringAppend(out, " u;\n}", -1);
	*al = ua > ta ? ua : ta;
	*sz = CAlignUp(uoff + us, *al);
    }
    Tcl_DStringFree(&un);
    return TCL_OK;
}

/* Déclaration "TYPE nom[...]" d'un champ (ou d'une variante). */
static int
CDecl(CGen *g, TclType *tp, const char *name, int last, Tcl_DString *out,
	size_t *sz, size_t *al, int *flex, const char *where)
{
    int r, isSelf = (tp == g->self);

    g->self = NULL;
    *flex = 0;
    if (++g->depth > 64) {
	g->depth--;
	return CErr(g, where, "type nested too deeply", NULL);
    }
    if (tp->kind == TCLT_LIST && tp->binDyn == NULL && tp->arity < 0
	    && CIsNamed(tp) && !isSelf) {
	goto pointer;			/* list.T nommé : Tcl_Obj * */
    }
    if (!isSelf && CIsNamed(tp) && tp->kind != TCLT_STRING && tp->kind != TCLT_VAR
	    && tp->kind != TCLT_DICT) {
	CGenNamed *nm = CEmitNamed(g, tp);

	if (nm == NULL) {
	    g->depth--;
	    return TCL_ERROR;
	}
	if (nm->busy) {
	    g->depth--;
	    return CErr(g, where, "a recursive type needs a pointer, which "
		    "the C export does not make yet: ", tp->name);
	}
	Tcl_DStringAppend(out, nm->cname, -1);
	Tcl_DStringAppend(out, " ", 1);
	Tcl_DStringAppend(out, name, -1);
	*sz = nm->size;
	*al = nm->align;
	*flex = nm->flex;
	g->depth--;
	return TCL_OK;
    }
    {
	Tcl_DString base;

	Tcl_DStringInit(&base);
	r = CScalar(g, tp, &base, sz, al, where);
	if (r != 0) {
	    if (r > 0) {
		Tcl_DStringAppend(out, Tcl_DStringValue(&base), -1);
		Tcl_DStringAppend(out, " ", 1);
		Tcl_DStringAppend(out, name, -1);
	    }
	    Tcl_DStringFree(&base);
	    g->depth--;
	    return r > 0 ? TCL_OK : TCL_ERROR;
	}
	Tcl_DStringFree(&base);
    }
    switch (tp->kind) {
    case TCLT_STRING:
	if ((tp->binKind == TCLBIN_STR || tp->binKind == TCLBIN_BYTES)
		&& tp->binDyn == NULL && tp->binSize > 0) {
	    char buf[48];

	    snprintf(buf, sizeof(buf), "[%lld]", (long long) tp->binSize);
	    Tcl_DStringAppend(out, tp->binKind == TCLBIN_BYTES ? "uint8_t "
		    : "char ", -1);
	    Tcl_DStringAppend(out, name, -1);
	    Tcl_DStringAppend(out, buf, -1);
	    *sz = (size_t) tp->binSize;
	    *al = 1;
	    g->depth--;
	    return TCL_OK;
	}
	goto pointer;
    case TCLT_DICT:
    case TCLT_VAR:
	goto pointer;
    case TCLT_LIST: {
	Tcl_DString inner, suffix;
	size_t es, ea;
	int ef;

	if (tp->arity < 0 && tp->binDyn == NULL) {
	    goto pointer;
	}
	if (tp->binDyn != NULL && !last) {
	    g->depth--;
	    return CErr(g, where, "a list whose length depends on another "
		    "field is a flexible array member, so it must be the last "
		    "field", NULL);
	}
	Tcl_DStringInit(&suffix);
	if (tp->binDyn != NULL) {
	    Tcl_DStringAppend(&suffix, "[]", 2);
	} else {
	    char buf[32];

	    snprintf(buf, sizeof(buf), "[%lld]", (long long) tp->arity);
	    Tcl_DStringAppend(&suffix, buf, -1);
	}
	Tcl_DStringInit(&inner);
	Tcl_DStringAppend(&inner, name, -1);
	Tcl_DStringAppend(&inner, Tcl_DStringValue(&suffix), -1);
	if (CDecl(g, tp->elem, Tcl_DStringValue(&inner), 0, out, &es, &ea,
		&ef, where) != TCL_OK) {
	    Tcl_DStringFree(&inner);
	    Tcl_DStringFree(&suffix);
	    g->depth--;
	    return TCL_ERROR;
	}
	Tcl_DStringFree(&inner);
	Tcl_DStringFree(&suffix);
	if (ef) {
	    g->depth--;
	    return CErr(g, where, "an array of structs ending in a flexible "
		    "member is not valid C", NULL);
	}
	*sz = (tp->binDyn != NULL) ? 0 : es * (size_t) tp->arity;
	*al = ea;
	*flex = (tp->binDyn != NULL);
	g->depth--;
	return TCL_OK;
    }
    case TCLT_PRODUCT: {
	int ff;

	if (CStructBody(g, tp, out, sz, al, &ff, NULL, NULL, NULL) != TCL_OK) {
	    g->depth--;
	    return TCL_ERROR;
	}
	Tcl_DStringAppend(out, " ", 1);
	Tcl_DStringAppend(out, name, -1);
	*flex = ff;
	g->depth--;
	return TCL_OK;
    }
    case TCLT_SUM: {
	Tcl_DString cn;

	Tcl_DStringInit(&cn);
	CIdent(name, &cn);
	if (CSumBody(g, tp, Tcl_DStringValue(&cn), out, sz, al, where)
		!= TCL_OK) {
	    Tcl_DStringFree(&cn);
	    g->depth--;
	    return TCL_ERROR;
	}
	Tcl_DStringFree(&cn);
	Tcl_DStringAppend(out, " ", 1);
	Tcl_DStringAppend(out, name, -1);
	g->depth--;
	return TCL_OK;
    }
    default:
	break;
    }
  pointer:
    g->needTcl = 1;
    Tcl_DStringAppend(out, "Tcl_Obj *", -1);
    Tcl_DStringAppend(out, name, -1);
    Tcl_DStringAppend(out, "    /* ", -1);
    Tcl_DStringAppend(out, tp->name, -1);
    Tcl_DStringAppend(out, " */", -1);
    *sz = sizeof(void *);
    *al = (size_t) TclTypeAbiAlign(0, 0);
    g->depth--;
    return TCL_OK;
}

/* Valeurs par défaut en initialiseur C : seulement si chaque champ en a
 * une qui s'écrit en C (un nombre), produits imbriqués compris. */
static int
CInit(TclType *tp, Tcl_DString *out)
{
    Tcl_Size i;

    if (tp->kind != TCLT_PRODUCT) {
	return 0;
    }
    Tcl_DStringAppend(out, "{", 1);
    for (i = 0; i < tp->nfields; i++) {
	TclType *ft = tp->fields[i].type;
	double dv;

	Tcl_DStringAppend(out, i ? ", " : " ", -1);
	if (ft->kind == TCLT_PRODUCT && tp->fields[i].defval == NULL) {
	    if (!CInit(ft, out)) {
		return 0;
	    }
	    continue;
	}
	if (tp->fields[i].defval == NULL
		|| (ft->kind != TCLT_INT && ft->kind != TCLT_DOUBLE)
		|| Tcl_GetDoubleFromObj(NULL, tp->fields[i].defval, &dv)
			!= TCL_OK) {
	    return 0;
	}
	Tcl_DStringAppend(out, Tcl_GetString(tp->fields[i].defval), -1);
    }
    Tcl_DStringAppend(out, " }", 2);
    return 1;
}

static CGenNamed *
CEmitNamed(CGen *g, TclType *tp)
{
    Tcl_HashEntry *h;
    CGenNamed *nm;
    Tcl_DString cn, body, asserts;
    int isNew, flex = 0;
    size_t sz = 0, al = 1;

    h = Tcl_CreateHashEntry(&g->done, (char *) tp, &isNew);
    if (!isNew) {
	return (CGenNamed *) Tcl_GetHashValue(h);
    }
    nm = (CGenNamed *) Tcl_Alloc(sizeof(CGenNamed));
    memset(nm, 0, sizeof(*nm));
    Tcl_SetHashValue(h, nm);
    Tcl_DStringInit(&cn);
    {
	/*
	 * v16u : le nom C d'un type d'un espace de noms est son dernier mot
	 * (::math::geometry::point -> point), comme namespace tail ; si un
	 * autre type déjà exporté a le même, le nom qualifié
	 * (math_geometry_point).
	 */
	const char *tail = tp->name, *q;
	Tcl_HashSearch hs;
	Tcl_HashEntry *e;

	for (q = tp->name; q[0] != '\0'; q++) {
	    if (q[0] == ':' && q[1] == ':') {
		tail = q + 2;
	    }
	}
	CIdent(tail, &cn);
	for (e = Tcl_FirstHashEntry(&g->done, &hs); tail != tp->name
		&& e != NULL; e = Tcl_NextHashEntry(&hs)) {
	    CGenNamed *o = (CGenNamed *) Tcl_GetHashValue(e);

	    if (o != nm && o->cname != NULL
		    && strcmp(o->cname, Tcl_DStringValue(&cn)) == 0) {
		Tcl_DStringSetLength(&cn, 0);
		CIdent(tp->name, &cn);
		break;
	    }
	}
    }
    nm->cname = (char *) Tcl_Alloc(Tcl_DStringLength(&cn) + 1);
    strcpy(nm->cname, Tcl_DStringValue(&cn));
    nm->busy = 1;
    Tcl_DStringInit(&body);
    Tcl_DStringInit(&asserts);
    if (tp->kind == TCLT_PRODUCT) {
	if (CStructBody(g, tp, &body, &sz, &al, &flex, &asserts, nm->cname, "")
		!= TCL_OK) {
	    goto fail;
	}
    } else if (tp->kind == TCLT_SUM) {
	if (CSumBody(g, tp, nm->cname, &body, &sz, &al, tp->name) != TCL_OK) {
	    goto fail;
	}
    } else {
	/* scalaire, tableau : typedef de la déclaration */
	g->self = (tp->under != NULL && tp->kind != TCLT_LIST) ? tp->under : tp;
	if (CDecl(g, tp->under != NULL && tp->kind != TCLT_LIST ? tp->under : tp,
		nm->cname, 1, &body, &sz, &al, &flex, tp->name) != TCL_OK) {
	    goto fail;
	}
	if (flex) {
	    CErr(g, tp->name, "a flexible array needs an enclosing struct",
		    NULL);
	    goto fail;
	}
    }
    Tcl_DStringAppend(&g->defs, "/* ", 3);
    Tcl_DStringAppend(&g->defs, tp->name, -1);
    Tcl_DStringAppend(&g->defs, " */\ntypedef ", -1);
    Tcl_DStringAppend(&g->defs, Tcl_DStringValue(&body), -1);
    if (tp->kind == TCLT_PRODUCT || tp->kind == TCLT_SUM) {
	Tcl_DStringAppend(&g->defs, " ", 1);
	Tcl_DStringAppend(&g->defs, nm->cname, -1);
    }
    Tcl_DStringAppend(&g->defs, ";\n", 2);
    {
	char buf[200];

	if (!flex) {
	    snprintf(buf, sizeof(buf), "_Static_assert(sizeof(%s) == %llu, "
		    "\"%s: size\");\n", nm->cname, (unsigned long long) sz,
		    nm->cname);
	} else {
	    snprintf(buf, sizeof(buf), "_Static_assert(sizeof(%s) >= %llu, "
		    "\"%s: size\");\n", nm->cname, (unsigned long long) sz,
		    nm->cname);
	}
	Tcl_DStringAppend(&g->defs, buf, -1);
    }
    Tcl_DStringAppend(&g->defs, Tcl_DStringValue(&asserts), -1);
    if (tp->kind == TCLT_PRODUCT) {
	Tcl_DString init;

	Tcl_DStringInit(&init);
	if (CInit(tp, &init)) {
	    Tcl_DStringAppend(&g->defs, "#define ", -1);
	    {
		const char *c;

		for (c = nm->cname; *c; c++) {
		    char u = (char) ((*c >= 'a' && *c <= 'z') ? *c - 32 : *c);

		    Tcl_DStringAppend(&g->defs, &u, 1);
		}
	    }
	    Tcl_DStringAppend(&g->defs, "_INIT ", -1);
	    Tcl_DStringAppend(&g->defs, Tcl_DStringValue(&init), -1);
	    Tcl_DStringAppend(&g->defs, "\n", 1);
	}
	Tcl_DStringFree(&init);
    }
    Tcl_DStringAppend(&g->defs, "\n", 1);
    nm->size = sz;
    nm->align = al;
    nm->flex = flex;
    nm->busy = 0;
    Tcl_DStringFree(&cn);
    Tcl_DStringFree(&body);
    Tcl_DStringFree(&asserts);
    return nm;

  fail:
    Tcl_DStringFree(&cn);
    Tcl_DStringFree(&body);
    Tcl_DStringFree(&asserts);
    return NULL;
}

static int
TypeCCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    CGen g;
    Tcl_Size i;
    Tcl_HashSearch hs;
    Tcl_HashEntry *h;
    int rc = TCL_OK;
    Tcl_DString head;
    char buf[200];
    union { uint16_t u; unsigned char c[2]; } bo;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "type ?type ...?");
	return TCL_ERROR;
    }
    memset(&g, 0, sizeof(g));
    g.interp = interp;
    Tcl_DStringInit(&g.defs);
    Tcl_InitHashTable(&g.done, TCL_ONE_WORD_KEYS);
    for (i = 2; i < objc && rc == TCL_OK; i++) {
	Tcl_Size tl;
	const char *tt = Tcl_GetStringFromObj(objv[i], &tl);
	TclType *tp = TclTypeParse(interp, tt, tl);

	if (tp == NULL) {
	    rc = TCL_ERROR;
	} else if (!CIsNamed(tp)) {
	    rc = CErr(&g, "", "give names defined by type def, not ", tp->name);
	} else if (CEmitNamed(&g, tp) == NULL) {
	    rc = TCL_ERROR;
	}
    }
    if (rc == TCL_OK) {
	bo.u = 1;
	Tcl_DStringInit(&head);
	snprintf(buf, sizeof(buf), "/* Produit par type {c!}export : %s-endian, "
		"pointeurs de %d octets. Ne pas modifier : regénérer. */\n",
		bo.c[0] ? "little" : "big", (int) sizeof(void *));
	Tcl_DStringAppend(&head, buf, -1);
	Tcl_DStringAppend(&head, "#include <stdint.h>\n#include <stddef.h>\n",
		-1);
	/* les contrôles de taille sont des _Static_assert (C11) : un
	 * compilateur C99 sans en-tête qui le fournisse (tcc 0.9.27 sous
	 * Windows) reçoit l'équivalent, un tableau de taille négative */
	Tcl_DStringAppend(&head, "#if !defined(__cplusplus) && "
		"!defined(_Static_assert) \\\n"
		"\t&& (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L)\n"
		"#define TCLT_SA_CAT_(a, b) a##b\n"
		"#define TCLT_SA_CAT(a, b) TCLT_SA_CAT_(a, b)\n"
		"#define _Static_assert(c, m) typedef char "
		"TCLT_SA_CAT(tclt_static_assert_, __LINE__)[(c) ? 1 : -1]\n"
		"#endif\n", -1);
	if (g.needTcl) {
	    Tcl_DStringAppend(&head, "#include <tcl.h>\n", -1);
	}
	Tcl_DStringAppend(&head, "\n", 1);
	Tcl_DStringAppend(&head, Tcl_DStringValue(&g.defs), -1);
	Tcl_DStringResult(interp, &head);
    }
    for (h = Tcl_FirstHashEntry(&g.done, &hs); h; h = Tcl_NextHashEntry(&hs)) {
	CGenNamed *nm = (CGenNamed *) Tcl_GetHashValue(h);

	Tcl_Free(nm->cname);
	Tcl_Free(nm);
    }
    Tcl_DeleteHashTable(&g.done);
    Tcl_DStringFree(&g.defs);
    return rc;
}

/* --------------------------------------------------------------------
 * type deref : le type d'une valeur dont les liens ont été suivis
 * -------------------------------------------------------------------- */

static int
HasLinkG(const TclType *tp, const TclType **stack, int depth)
{
    Tcl_Size i;

    if (tp == NULL) {
	return 0;
    }
    if (tp->kind == TCLT_VAR) {
	return 1;
    }
    for (i = 0; i < depth; i++) {
	if (stack[i] == tp) {
	    return 0;			/* type récursif : déjà en cours */
	}
    }
    if (depth >= 63) {
	return 0;
    }
    stack[depth] = tp;
    switch (tp->kind) {
    case TCLT_LIST:
    case TCLT_DICT:
	return HasLinkG(tp->elem, stack, depth + 1);
    case TCLT_PRODUCT:
    case TCLT_SUM:
	for (i = 0; i < tp->nfields; i++) {
	    if (HasLinkG(tp->fields[i].type, stack, depth + 1)) {
		return 1;
	    }
	}
	return 0;
    default:
	return 0;
    }
}

int
TclTypeHasLink(const TclType *tp)
{
    const TclType *stack[64];

    return HasLinkG(tp, stack, 0);
}

static TclType *
DerefTypeG(Tcl_Interp *interp, TypeData *d, TclType *tp, const TclType **stack,
	int depth)
{
    Tcl_Size i;
    TclType *e;

    if (!TclTypeHasLink(tp)) {
	return tp;
    }
    if (tp->kind == TCLT_VAR) {
	return tp->elem;		/* var.T donne T (var seul : string) */
    }
    /* Produits et sommes récursifs : le descripteur en cours est retrouvé
     * dans d->derefs. Une collection récursive sans produit ni somme n'a
     * pas de sens ici. */
    if (tp->kind == TCLT_LIST || tp->kind == TCLT_DICT) {
	for (i = 0; i < depth; i++) {
	    if (stack[i] == tp) {
		ERR(interp, "DEREF", "type deref: the recursive type %s "
			"contains links", tp->name);
		return NULL;
	    }
	}
    }
    stack[depth] = tp;
    switch (tp->kind) {
    case TCLT_LIST:
	e = DerefTypeG(interp, d, tp->elem, stack, depth + 1);
	if (e == NULL) {
	    return NULL;
	}
	return (tp->arity >= 0) ? TclTypesMakeListN(d, e, tp->arity)
		: tp->lenRange ? TclTypesMakeListRange(d, e, tp->lenMin,
		    tp->lenMax) : TclTypesMakeList(d, e);
    case TCLT_DICT: {
	TclDictKey *keys = NULL;

	e = DerefTypeG(interp, d, tp->elem, stack, depth + 1);
	if (e == NULL) {
	    return NULL;
	}
	if (tp->keyed) {
	    keys = (TclDictKey *) Tcl_Alloc(tp->nkeys * sizeof(TclDictKey));
	    for (i = 0; i < tp->nkeys; i++) {
		keys[i] = tp->keys[i];
		Tcl_IncrRefCount(keys[i].name);
		if (keys[i].defval != NULL) {
		    Tcl_IncrRefCount(keys[i].defval);
		}
		if (keys[i].defcmd != NULL) {
		    Tcl_IncrRefCount(keys[i].defcmd);
		}
	    }
	}
	return TclTypesMakeDict(d, e, tp->keyed, keys, tp->nkeys);
    }
    case TCLT_PRODUCT:
    case TCLT_SUM: {
	/*
	 * Le type est enregistré AVANT son contenu : un type récursif
	 * (tree(var) contient list.tree(var)) retombe alors sur ce même
	 * descripteur au lieu de boucler.
	 */

	Tcl_HashEntry *hPtr;
	TclType *nt;
	TclTypeField *f;
	char name[300];
	int isNew;

	hPtr = Tcl_CreateHashEntry(&d->derefs, (const char *) tp, &isNew);
	if (!isNew) {
	    return (TclType *) Tcl_GetHashValue(hPtr);
	}
	snprintf(name, sizeof(name), "deref(%.280s)", tp->name);
	nt = NewType(d, tp->kind, name);
	Tcl_SetHashValue(hPtr, nt);
	f = (TclTypeField *) Tcl_Alloc(tp->nfields * sizeof(TclTypeField));
	memset(f, 0, tp->nfields * sizeof(TclTypeField));
	for (i = 0; i < tp->nfields; i++) {
	    f[i] = tp->fields[i];
	    if (f[i].type != NULL) {
		f[i].type = DerefTypeG(interp, d, f[i].type, stack, depth + 1);
		if (f[i].type == NULL) {
		    Tcl_DeleteHashEntry(hPtr);
		    Tcl_Free(f);
		    return NULL;
		}
	    }
	}
	for (i = 0; i < tp->nfields; i++) {
	    if (f[i].name != NULL) {
		Tcl_IncrRefCount(f[i].name);
	    }
	    if (f[i].literal != NULL) {
		Tcl_IncrRefCount(f[i].literal);
	    }
	    if (f[i].defval != NULL) {
		Tcl_IncrRefCount(f[i].defval);
	    }
	    if (f[i].defcmd != NULL) {
		Tcl_IncrRefCount(f[i].defcmd);
	    }
	}
	if (tp->kind == TCLT_SUM) {
	    SortVariants(f, tp->nfields);
	}
	nt->fields = f;
	nt->nfields = tp->nfields;
	return nt;
    }
    default:
	return tp;
    }
}

TclType *
TclTypeDerefType(Tcl_Interp *interp, TclType *tp)
{
    const TclType *stack[64];

    return DerefTypeG(interp, GetData(interp), tp, stack, 0);
}

static int
TypeDerefCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *res = NULL;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "value");
	return TCL_ERROR;
    }
    if (TclValueDeref(interp, objv[2], &res) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, res);
    Tcl_DecrRefCount(res);
    return TCL_OK;
}

/* --------------------------------------------------------------------
 * La commande "type" : def, alias, convert
 * -------------------------------------------------------------------- */

static void
CopyInto(TclType *into, const TclType *res)
{
    Tcl_Size i;

    into->kind = res->kind;
    into->key = res->key;
    into->elem = res->elem;
    into->arity = res->arity;
    into->keyed = res->keyed;
    into->varTyped = res->varTyped;
    into->bitWidth = res->bitWidth;
    into->bitMsb0 = res->bitMsb0;
    into->binKind = res->binKind;
    into->binSize = res->binSize;
    into->binEnd = res->binEnd;
    into->binEnc = res->binEnc;
    into->binDyn = res->binDyn;
    into->binAt = res->binAt;
    into->binAtBody = res->binAtBody;
    into->binLink = res->binLink;
    into->binTag = res->binTag;
    into->sumMode = res->sumMode;
    into->variantOf = res->variantOf;	/* v18b : type def chaud temp/celsius */
    into->variantIdx = res->variantIdx;
    if (res->pred != NULL && into->pred == NULL) {
	/* v17u : type def pair int(($%2==0)) : le prédicat suit */
	into->pred = res->pred;
	Tcl_IncrRefCount(into->pred);
	into->predBase = res->predBase;
	into->predInterp = res->predInterp;
    }
    if (into->family == NULL && res->family != NULL) {
	/* type def mm {grandeur(L, 0.001)} : mm hérite de la famille et des
	 * paramètres de l'instance (règles de famille de type convert). */
	into->family = res->family;
	into->familyArgs = res->familyArgs;
	Tcl_IncrRefCount(into->familyArgs);
    }
    if (res->nkeys > 0) {
	into->keys = (TclDictKey *) Tcl_Alloc(res->nkeys * sizeof(TclDictKey));
	into->nkeys = res->nkeys;
	for (i = 0; i < res->nkeys; i++) {
	    into->keys[i] = res->keys[i];
	    Tcl_IncrRefCount(into->keys[i].name);
	    if (into->keys[i].defval != NULL) {
		Tcl_IncrRefCount(into->keys[i].defval);
	    }
	    if (into->keys[i].defcmd != NULL) {
		Tcl_IncrRefCount(into->keys[i].defcmd);
	    }
	}
    }
    if (res->kind == TCLT_INT || res->kind == TCLT_DOUBLE
	    || res->kind == TCLT_STRING) {
	into->under = (TclType *) res;
    }
    if (res->nfields > 0) {
	into->fields = (TclTypeField *) Tcl_Alloc(
		res->nfields * sizeof(TclTypeField));
	into->nfields = res->nfields;
	for (i = 0; i < res->nfields; i++) {
	    into->fields[i] = res->fields[i];
	    if (into->fields[i].name != NULL) {
		Tcl_IncrRefCount(into->fields[i].name);
	    }
	    if (into->fields[i].literal != NULL) {
		Tcl_IncrRefCount(into->fields[i].literal);
	    }
	    if (into->fields[i].defval != NULL) {
		Tcl_IncrRefCount(into->fields[i].defval);
	    }
	    if (into->fields[i].defcmd != NULL) {
		Tcl_IncrRefCount(into->fields[i].defcmd);
	    }
	}
    }
}

/* L'argument de définition : annotation du parseur ({double}x) ou mot
 * ordinaire. Dans le premier cas on reconstruit le texte {type}suffixe,
 * que l'analyseur de types lit comme un groupe nommé : {double}x est donc
 * un produit à un champ. Pour une liste de point, on écrit list.point. */
static const char *
DefinitionText(Tcl_Interp *interp, Tcl_Obj *obj, Tcl_DString *ds)
{
    Tcl_Obj *content, *raw;
    int kind = TclAnnotInfo(obj, &content, NULL, NULL, &raw);

    (void) interp;
    Tcl_DStringInit(ds);
    if (kind == 2) {
	Tcl_DStringAppend(ds, "{", 1);
	Tcl_DStringAppend(ds, Tcl_GetString(raw), TCL_AUTO_LENGTH);
	Tcl_DStringAppend(ds, "}", 1);
	Tcl_DStringAppend(ds, Tcl_GetString(content), TCL_AUTO_LENGTH);
    } else {
	Tcl_DStringAppend(ds, Tcl_GetString(obj), TCL_AUTO_LENGTH);
    }
    return Tcl_DStringValue(ds);
}

static int
CheckNewName(Tcl_Interp *interp, TypeData *d, const char *full)
{
    const char *q, *name = full;

    /* v16u : NAME est déjà qualifié (::geom::point) ; seul le dernier mot
     * est un nom de type (l'espace de noms est celui de Tcl). */
    for (q = full; q[0] != '\0'; q++) {
	if (q[0] == ':' && q[1] == ':') {
	    name = q + 2;
	}
    }
    if (name[0] == '\0') {
	ERR(interp, "NAME", "empty type name");
	return TCL_ERROR;
    }
    /* Un nom de type est un mot nu : lettres, chiffres, "_" (le même
     * jeu de caractères que l'analyseur), sans chiffre initial. Les noms
     * canoniques anonymes (list<int>, prod<...>) ne peuvent donc jamais
     * entrer en collision avec un nom défini par l'utilisateur. */
    if (isdigit((unsigned char) name[0])) {
	ERR(interp, "NAME", "bad type name \"%s\"", name);
	return TCL_ERROR;
    }
    for (q = name; *q != '\0'; q++) {
	if (!TclTypeIsNameChar(*q)) {
	    ERR(interp, "NAME", "bad type name \"%s\"", name);
	    return TCL_ERROR;
	}
    }
    if (strcmp(name, "type") == 0) {
	ERR(interp, "NAME", "type \"type\" is reserved: it names the kind of a "
		"parameter, as in {type}T");
	return TCL_ERROR;
    }
    if (Tcl_FindHashEntry(&d->generics, full) != NULL) {
	ERR(interp, "NAME", "type \"%s\" is already defined", full);
	return TCL_ERROR;
    }
    if (Tcl_FindHashEntry(&d->names, full) != NULL) {
	ERR(interp, "NAME", "type \"%s\" is %s", full,
		strcmp(name, "int") == 0 || strcmp(name, "double") == 0
		|| strcmp(name, "string") == 0 || strcmp(name, "list") == 0
		|| strcmp(name, "dict") == 0 || strcmp(name, "array") == 0
		|| strcmp(name, "var") == 0
		? "reserved" : "already defined");
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
TypeDefine(Tcl_Interp *interp, TypeData *d, Tcl_Obj *nameObj, Tcl_Obj *defObj,
	int alias)
{
    const char *name = Tcl_GetString(nameObj);
    Tcl_DString ds;
    TypeEnv env;
    TclType *into = NULL, *res = NULL;
    const TclType *stack[64];
    int isNew;
    const char *text;

    if (strchr(name, '(') != NULL) {
	/* type def matrix({int}i, {int}j) {...} : un type paramétré. */

	int rc;

	text = DefinitionText(interp, defObj, &ds);
	rc = TclTypesDefineGeneric(interp, d, name, (Tcl_Size) strlen(name),
		text, Tcl_DStringLength(&ds), alias);
	Tcl_DStringFree(&ds);
	return rc;
    }
    if (CheckNewName(interp, d, name) != TCL_OK) {
	return TCL_ERROR;
    }
    text = DefinitionText(interp, defObj, &ds);
    memset(&env, 0, sizeof(env));
    env.d = d;
    if (!alias) {
	into = NewType(d, TCLT_STRING, name);
	into->nominal = 1;
	env.selfName = into->name;		/* auto-référence permise */
	env.self = into;
	env.into = into;
    }
    if (TclCompileType(interp, text, Tcl_DStringLength(&ds), &env)
	    != TCL_OK) {
	Tcl_DStringFree(&ds);
	return TCL_ERROR;
    }
    res = env.result;
    Tcl_DStringFree(&ds);
    if (!alias) {
	if (res == into && into->kind == TCLT_STRING && into->nfields == 0) {
	    ERR(interp, "SYNTAX", "type \"%s\" cannot be defined as itself",
		    name);
	    return TCL_ERROR;
	}
	if (res != into) {
	    CopyInto(into, res);
	}
	if (!Inhabited(into, stack, 0)) {
	    ERR(interp, "RECURSIVE", "type \"%s\" has no finite value: every "
		    "recursive reference must go through a sum variant or a "
		    "collection", name);
	    return TCL_ERROR;
	}
	res = into;
    }
    Tcl_SetHashValue(Tcl_CreateHashEntry(&d->names, name, &isNew), res);
    d->gen++;
    return TCL_OK;
}

static int
TypeConvertCmd(Tcl_Interp *interp, TypeData *d, Tcl_Obj *fromObj,
	Tcl_Obj *toObj, Tcl_Obj *lambda)
{
    char key[2 * 128 + 2], side[2][128];
    int isNew, i;
    Tcl_HashEntry *hPtr;
    Tcl_Obj *objs[2];

    /*
     * Chaque côté est un type défini, ou une FAMILLE (type def paramétré,
     * « grandeur » pour grandeur(L, 0.001)) : la règle vaut alors pour
     * toutes ses instances, et sa lambda reçoit en plus les paramètres
     * de l'instance (un produit) : {x de vers}, {x de} ou {x vers}.
     */
    objs[0] = fromObj;
    objs[1] = toObj;
    {
	/*
	 * Deux variantes d'une même somme : type convert S/a S/b {{x} ...}.
	 * La règle sert à la lecture d'une variante inactive ($v(b)) et au
	 * cast explicite {S.b}$v ; elle n'intervient jamais dans la déduction.
	 */
	TclType *vs[2] = {NULL, NULL};
	Tcl_Size vi[2] = {-1, -1};
	int nv = 0;

	for (i = 0; i < 2; i++) {
	    const char *nm = Tcl_GetString(objs[i]);
	    const char *dot = strrchr(nm, '/');

	    if (dot != NULL && TclTypesFindName(interp, d, &d->names, nm, -1,
		    NULL, NULL) == NULL) {
		Tcl_HashEntry *h;

		h = TclTypesFindName(interp, d, &d->names, nm, dot - nm, NULL,
			NULL);
		if (h != NULL && Tcl_GetHashValue(h) != NULL
			&& ((TclType *) Tcl_GetHashValue(h))->kind == TCLT_SUM) {
		    vs[i] = (TclType *) Tcl_GetHashValue(h);
		    vi[i] = TclTypeFieldIndex(vs[i], dot + 1);
		    if (vi[i] < 0) {
			ERR(interp, "UNKNOWN", "type convert: %s has no variant "
				"\"%s\"", vs[i]->name, dot + 1);
			return TCL_ERROR;
		    }
		    nv++;
		}
	    }
	}
	if (nv > 0) {
	    if (nv != 2 || vs[0] != vs[1]) {
		ERR(interp, "SYNTAX", "type convert between variants needs two "
			"variants of the same sum, as in S/a S/b, not \"%s\" and "
			"\"%s\"", Tcl_GetString(fromObj), Tcl_GetString(toObj));
		return TCL_ERROR;
	    }
	    if (vi[0] == vi[1]) {
		ERR(interp, "SYNTAX", "type convert: \"%s\" and \"%s\" are the "
			"same variant", Tcl_GetString(fromObj),
			Tcl_GetString(toObj));
		return TCL_ERROR;
	    }
	    snprintf(key, sizeof(key), "%s/%s\1%s/%s", vs[0]->name,
		    Tcl_GetString(vs[0]->fields[vi[0]].name), vs[1]->name,
		    Tcl_GetString(vs[1]->fields[vi[1]].name));
	    goto store;
	}
    }
    for (i = 0; i < 2; i++) {
	const char *nm = Tcl_GetString(objs[i]);
	Tcl_HashEntry *h = TclTypesFindName(interp, d, &d->names, nm, -1,
		NULL, NULL);

	if (h != NULL && Tcl_GetHashValue(h) != NULL) {
	    snprintf(side[i], sizeof(side[i]), "%s",
		    ((TclType *) Tcl_GetHashValue(h))->name);
	} else if ((h = TclTypesFindName(interp, d, &d->generics, nm, -1,
		NULL, NULL)) != NULL) {
	    snprintf(side[i], sizeof(side[i]), "@%s",
		    (const char *) Tcl_GetHashKey(&d->generics, h));
	} else {
	    ERR(interp, "UNKNOWN", "type convert needs two defined types or "
		    "type families, not \"%s\"", nm);
	    return TCL_ERROR;
	}
    }
    snprintf(key, sizeof(key), "%s\1%s", side[0], side[1]);
  store:
    hPtr = Tcl_CreateHashEntry(&d->converts, key, &isNew);
    if (!isNew) {
	Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(hPtr));
    }
    lambda = Tcl_DuplicateObj(lambda);
    Tcl_IncrRefCount(lambda);
    Tcl_SetHashValue(hPtr, lambda);
    return TCL_OK;
}

/*
 * "type get valeur" : le type d'une valeur (donc d'une variable : type get
 * $L). Le résultat est le nom du type ({int}n -> int, {list<int>}L ->
 * list<int>, {Complex}z -> Complex). Pour une somme, la variante suit :
 * Val.i. Une valeur sans annotation n'a pas de
 * type : le résultat est la chaîne vide ; une annotation brute {texte}
 * rend son texte. Pour tester le type d'une variable par son nom, utiliser
 * l'opérateur "is" de expr.
 */

static int
TypeGetCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "value");
	return TCL_ERROR;
    }
    /* Le type résolu seul ; les notes se lisent avec « note get ». */
    Tcl_SetObjResult(interp, TclAnnotDisplay(interp, objv[2], 0));
    return TCL_OK;
}

/*
 * BinRepr --
 *
 *	Représentation binaire d'un type, sous forme de liste Tcl :
 *	    uint SIZE END | sint SIZE END | float SIZE END    (octets)
 *	    bits SIZE END    bitarray(n), n multiple de 8
 *	    bytes N | str N ENC | cstring ENC       (ENC : nom d'encodage Tcl)
 *	    lit N            somme de littéraux de même longueur
 *	    array N R        list(N).T
 *	    product NAME {{FIELD R HASDEFAULT} ...}
 *	    tagged FIELD {{VARIANT R} ...}           S($champ)
 *	    at EXPR R        at(EXPR).T
 *	où N (taille, nombre) est un entier, {prefix R} (lu juste avant) ou
 *	{expr TEXTE} (calculé sur des champs déjà lus). `vis` liste les champs
 *	déjà lus et visibles (le produit courant, puis les englobants) : une
 *	expression ne peut citer qu'eux.
 *	Rend 0 et laisse *objPtr à NULL si le type n'en a pas.
 */

static int BinRepr(Tcl_Interp *interp, TclType *tp, int depth, Tcl_Obj *vis,
	Tcl_Obj **objPtr);
static int BinReprBody(Tcl_Interp *interp, TclType *tp, int depth,
	Tcl_Obj *vis, Tcl_Obj **objPtr);

/*
 * LinkRepr --
 *
 *	link ORIGINE REPR NULL CIBLE OPT   pour *(...).T. REPR est la
 *	représentation de l'entier ; NULL le texte de la valeur nulle, ou
 *	"none" ; CIBLE la représentation de T. Si T est une somme
 *	{U}some, {unit}none, CIBLE est celle de U et OPT le nom de la somme
 *	(sinon OPT est vide).
 */

static int LinkReprBody(Tcl_Interp *interp, TclType *tp, int depth,
	Tcl_Obj *vis, Tcl_Obj **objPtr);

/* Un lien qui se retrouve lui-même dans sa propre cible (liste chaînée,
 * arbre) : la seconde occurrence est la représentation {linkref NOM}, que
 * type binary développe à la demande. */

static int
LinkRepr(
    Tcl_Interp *interp,
    TclType *tp,
    int depth,
    Tcl_Obj *vis,
    Tcl_Obj **objPtr)
{
    TypeData *d = GetData(interp);
    int i, rc;
    Tcl_Obj *r;

    for (i = 0; i < d->nLinkStack; i++) {
	if (d->linkStack[i] == tp) {
	    r = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("linkref", -1));
	    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(tp->name, -1));
	    *objPtr = r;
	    return TCL_OK;
	}
    }
    if (d->nLinkStack >= 32) {
	ERR(interp, "BIN", "type %s is too deeply nested for a binary layout",
		tp->name);
	return TCL_ERROR;
    }
    d->linkStack[d->nLinkStack++] = tp;
    rc = LinkReprBody(interp, tp, depth, vis, objPtr);
    d->nLinkStack--;
    return rc;
}

static int
LinkReprBody(
    Tcl_Interp *interp,
    TclType *tp,
    int depth,
    Tcl_Obj *vis,
    Tcl_Obj **objPtr)
{
    TclBinLink *lk = tp->binLink;
    TclType *target = lk->target;
    Tcl_Obj *reprR = NULL, *targR = NULL, *r;
    const char *optName = "";
    const char *nullText = lk->nullVal;
    Tcl_Size i;

    if (BinRepr(interp, lk->repr, depth + 1, vis, &reprR) != TCL_OK
	    || reprR == NULL) {
	ERR(interp, "BIN", "%s: the address type %s has no binary "
		"representation", tp->name, lk->repr->name);
	return TCL_ERROR;
    }
    if (target->kind == TCLT_SUM && target->binTag == NULL
	    && target->binLink == NULL && target->nfields == 2) {
	TclType *some = NULL;
	int hasNone = 0;

	for (i = 0; i < target->nfields; i++) {
	    const char *nm = Tcl_GetString(target->fields[i].name != NULL
		    ? target->fields[i].name : target->fields[i].literal);

	    if (strcmp(nm, "some") == 0) {
		some = target->fields[i].type;
	    } else if (strcmp(nm, "none") == 0) {
		hasNone = 1;
	    }
	}
	if (some == NULL || !hasNone) {
	    Tcl_DecrRefCount(reprR);
	    ERR(interp, "BIN", "%s: a link to a sum needs the variants "
		    "{T}some and \"none\"", tp->name);
	    return TCL_ERROR;
	}
	optName = tp->name;
	if (nullText == NULL) {
	    nullText = "0";
	}
	if (BinRepr(interp, some, depth + 1, vis, &targR) != TCL_OK) {
	    Tcl_DecrRefCount(reprR);
	    return TCL_ERROR;
	}
    } else {
	/* la cible, avec sa propre décoration (un lien, at...) */

	Tcl_Obj *vis2 = vis;

	if (target->binLink != NULL || target->binAt != NULL
		|| target->binTag != NULL) {
	    if (BinRepr(interp, target, depth + 1, vis2, &targR) != TCL_OK) {
		Tcl_DecrRefCount(reprR);
		return TCL_ERROR;
	    }
	} else if (BinReprBody(interp, target, depth + 1, vis2, &targR)
		!= TCL_OK) {
	    Tcl_DecrRefCount(reprR);
	    return TCL_ERROR;
	}
    }
    if (targR == NULL) {
	Tcl_DecrRefCount(reprR);
	ERR(interp, "BIN", "%s: the target %s has no binary representation",
		tp->name, target->name);
	return TCL_ERROR;
    }
    r = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("link", -1));
    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(lk->origin, -1));
    Tcl_ListObjAppendElement(NULL, r, reprR);
    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(
	    nullText != NULL ? nullText : "none", -1));
    Tcl_ListObjAppendElement(NULL, r, targR);
    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(optName, -1));
    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(
	    lk->ref != NULL ? lk->ref->name : "", -1));
    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(target->name, -1));
    *objPtr = r;
    return TCL_OK;
}

/* Les noms $x et ${x} cités par EXPR sont-ils tous dans vis ? */

static int
CheckRefs(
    Tcl_Interp *interp,
    const char *what,
    const char *expr,
    Tcl_Obj *vis)
{
    const char *p;

    for (p = expr; *p != '\0'; p++) {
	const char *q;
	Tcl_Size len, n, i, found = 0;
	Tcl_Obj **v;

	if (*p != '$') {
	    continue;
	}
	q = p + 1;
	if (*q == '{') {
	    const char *close = strchr(q, '}');

	    q++;
	    len = (close != NULL) ? close - q : 0;
	} else {
	    for (len = 0; TclTypeIsNameChar(q[len]); len++) {
		/* vide */
	    }
	}
	if (len == 0) {
	    continue;
	}
	Tcl_ListObjGetElements(NULL, vis, &n, &v);
	for (i = 0; i < n; i++) {
	    Tcl_Size l;
	    const char *nm = Tcl_GetStringFromObj(v[i], &l);

	    if (l == len && memcmp(nm, q, len) == 0) {
		found = 1;
		break;
	    }
	}
	if (!found) {
	    ERR(interp, "BIN", "%s uses $%.*s, which is not a field read "
		    "before it (in this product or an enclosing one)", what,
		    (int) len, q);
	    return TCL_ERROR;
	}
	p = q + len - 1;
    }
    return TCL_OK;
}

/* Taille ou nombre : entier, {prefix R} ou {expr TEXTE}. */

static int
SizeSpec(
    Tcl_Interp *interp,
    TclType *tp,
    Tcl_Size fixed,
    int depth,
    Tcl_Obj *vis,
    Tcl_Obj **specPtr)
{
    Tcl_Obj *spec, *sub;

    if (tp->binDyn == NULL) {
	*specPtr = Tcl_NewWideIntObj(fixed);
	return TCL_OK;
    }
    spec = Tcl_NewListObj(0, NULL);
    if (tp->binDyn->kind == TCLDYN_PREFIX) {
	if (BinRepr(interp, tp->binDyn->pre, depth + 1, vis, &sub) != TCL_OK
		|| sub == NULL) {
	    Tcl_DecrRefCount(spec);
	    return TCL_ERROR;
	}
	Tcl_ListObjAppendElement(NULL, spec, Tcl_NewStringObj("prefix", -1));
	Tcl_ListObjAppendElement(NULL, spec, sub);
    } else {
	if (CheckRefs(interp, tp->name, tp->binDyn->expr, vis) != TCL_OK) {
	    Tcl_DecrRefCount(spec);
	    return TCL_ERROR;
	}
	Tcl_ListObjAppendElement(NULL, spec, Tcl_NewStringObj("expr", -1));
	Tcl_ListObjAppendElement(NULL, spec,
		Tcl_NewStringObj(tp->binDyn->expr, -1));
    }
    *specPtr = spec;
    return TCL_OK;
}

static int
BinReprBody(
    Tcl_Interp *interp,
    TclType *tp,
    int depth,
    Tcl_Obj *vis,
    Tcl_Obj **objPtr)
{
    static const char *const endName[] = {"auto", "le", "be"};
    Tcl_Obj *r = NULL, *sub, *spec;
    Tcl_Size i, len;

    *objPtr = NULL;
    switch (tp->binKind) {
    case TCLBIN_UINT: case TCLBIN_SINT: case TCLBIN_FLOAT:
	r = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(
		tp->binKind == TCLBIN_UINT ? "uint"
		: tp->binKind == TCLBIN_SINT ? "sint" : "float", -1));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewWideIntObj(tp->binSize));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(
		endName[tp->binEnd], -1));
	*objPtr = r;
	return TCL_OK;
    case TCLBIN_BYTES: case TCLBIN_STR:
	if (SizeSpec(interp, tp, tp->binSize, depth, vis, &spec) != TCL_OK) {
	    return TCL_ERROR;
	}
	r = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(
		tp->binKind == TCLBIN_BYTES ? "bytes" : "str", -1));
	Tcl_ListObjAppendElement(NULL, r, spec);
	if (tp->binKind == TCLBIN_STR) {
	    Tcl_ListObjAppendElement(NULL, r,
		    Tcl_NewStringObj(tp->binEnc, -1));
	}
	*objPtr = r;
	return TCL_OK;
    case TCLBIN_CSTRING:
	r = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("cstring", -1));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(tp->binEnc, -1));
	*objPtr = r;
	return TCL_OK;
    default:
	break;
    }
    if (tp->kind == TCLT_INT && tp->bitWidth > 0) {
	if (tp->bitWidth % 8 == 0) {
	    r = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("bits", -1));
	    Tcl_ListObjAppendElement(NULL, r,
		    Tcl_NewWideIntObj(tp->bitWidth / 8));
	    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(
		    endName[tp->binEnd], -1));
	    *objPtr = r;
	}
	return TCL_OK;
    }
    switch (tp->kind) {
    case TCLT_SUM:
	if (tp->binTag != NULL) {
	    Tcl_Obj *vs = Tcl_NewListObj(0, NULL);

	    Tcl_DString ref;
	    int rc;

	    Tcl_DStringInit(&ref);
	    if (tp->binTag[0] != '=') {
		Tcl_DStringAppend(&ref, "$", 1);
		Tcl_DStringAppend(&ref, tp->binTag, TCL_AUTO_LENGTH);
	    }
	    rc = CheckRefs(interp, tp->name, Tcl_DStringValue(&ref), vis);
	    Tcl_DStringFree(&ref);
	    if (rc != TCL_OK) {
		Tcl_DecrRefCount(vs);
		return TCL_ERROR;
	    }
	    for (i = 0; i < tp->nfields; i++) {
		Tcl_Obj *item = Tcl_NewListObj(0, NULL);

		if (BinRepr(interp, tp->fields[i].type, depth + 1, vis, &sub)
			!= TCL_OK) {
		    Tcl_DecrRefCount(item);
		    Tcl_DecrRefCount(vs);
		    return TCL_ERROR;
		}
		if (sub == NULL) {
		    ERR(interp, "BIN", "variant %s of %s has no binary "
			    "representation", Tcl_GetString(tp->fields[i].name),
			    tp->name);
		    Tcl_DecrRefCount(item);
		    Tcl_DecrRefCount(vs);
		    return TCL_ERROR;
		}
		Tcl_ListObjAppendElement(NULL, item, tp->fields[i].name);
		Tcl_ListObjAppendElement(NULL, item, sub);
		Tcl_ListObjAppendElement(NULL, vs, item);
	    }
	    r = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("tagged", -1));
	    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(tp->binTag, -1));
	    Tcl_ListObjAppendElement(NULL, r, vs);
	    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(tp->name, -1));
	    *objPtr = r;
	    return TCL_OK;
	}
	/* Somme de littéraux de même longueur (une constante, "MZ") */
	len = -1;
	for (i = 0; i < tp->nfields; i++) {
	    Tcl_Size l;

	    if (tp->fields[i].type != NULL || tp->fields[i].literal == NULL) {
		return TCL_OK;
	    }
	    l = Tcl_GetCharLength(tp->fields[i].literal);
	    if (len >= 0 && l != len) {
		return TCL_OK;
	    }
	    len = l;
	}
	if (len >= 0) {
	    /* len == 0 : le littéral vide {""} occupe 0 octet (« unit ») */

	    r = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("lit", -1));
	    Tcl_ListObjAppendElement(NULL, r, Tcl_NewWideIntObj(len));
	    *objPtr = r;
	}
	return TCL_OK;
    case TCLT_LIST:
	if (tp->arity >= 0 || tp->binDyn != NULL) {
	    if (SizeSpec(interp, tp, tp->arity, depth, vis, &spec)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (BinRepr(interp, tp->elem, depth + 1, vis, &sub) != TCL_OK) {
		Tcl_DecrRefCount(spec);
		return TCL_ERROR;
	    }
	    if (sub != NULL) {
		r = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("array", -1));
		Tcl_ListObjAppendElement(NULL, r, spec);
		Tcl_ListObjAppendElement(NULL, r, sub);
		*objPtr = r;
	    } else {
		Tcl_DecrRefCount(spec);
	    }
	}
	return TCL_OK;
    case TCLT_PRODUCT: {
	Tcl_Obj *flds = Tcl_NewListObj(0, NULL);
	Tcl_Obj *vis2 = Tcl_DuplicateObj(vis);

	for (i = 0; i < tp->nfields; i++) {
	    Tcl_Obj *fr, *item;
	    const TclTypeField *f = &tp->fields[i];

	    fr = NULL;
	    if (f->type != NULL && f->type->binLink != NULL) {
		/* La cible d'un lien est lue après tous les champs du produit :
		 * elle peut citer ceux qui suivent. */

		Tcl_Obj *visAll = Tcl_DuplicateObj(vis2);
		Tcl_Size j;
		int rc;

		for (j = i + 1; j < tp->nfields; j++) {
		    if (tp->fields[j].name != NULL) {
			Tcl_ListObjAppendElement(NULL, visAll,
				tp->fields[j].name);
		    }
		}
		rc = BinRepr(interp, f->type, depth + 1, visAll, &fr);
		Tcl_DecrRefCount(visAll);
		if (rc != TCL_OK) {
		    Tcl_DecrRefCount(flds);
		    Tcl_DecrRefCount(vis2);
		    return TCL_ERROR;
		}
	    } else if (f->type != NULL
		    && BinRepr(interp, f->type, depth + 1, vis2, &fr)
		    != TCL_OK) {
		Tcl_DecrRefCount(flds);
		Tcl_DecrRefCount(vis2);
		return TCL_ERROR;
	    }
	    item = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(NULL, item, f->name != NULL ? f->name
		    : Tcl_NewObj());
	    Tcl_ListObjAppendElement(NULL, item,
		    fr != NULL ? fr : Tcl_NewStringObj("none", -1));
	    Tcl_ListObjAppendElement(NULL, item, Tcl_NewIntObj(
		    f->defval != NULL || f->defcmd != NULL));
	    Tcl_ListObjAppendElement(NULL, flds, item);
	    if (f->name != NULL) {
		Tcl_ListObjAppendElement(NULL, vis2, f->name);
	    }
	}
	Tcl_DecrRefCount(vis2);
	r = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("product", -1));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(tp->name, -1));
	Tcl_ListObjAppendElement(NULL, r, flds);
	*objPtr = r;
	return TCL_OK;
    }
    default:
	return TCL_OK;
    }
}

static int
BinRepr(
    Tcl_Interp *interp,
    TclType *tp,
    int depth,
    Tcl_Obj *vis,
    Tcl_Obj **objPtr)
{
    Tcl_Obj *inner, *r;

    *objPtr = NULL;
    if (depth > 32) {
	ERR(interp, "BIN", "type %s is too deeply nested (or recursive) for "
		"a binary layout", tp->name);
	return TCL_ERROR;
    }
    if (tp->binLink != NULL) {
	return LinkRepr(interp, tp, depth, vis, objPtr);
    }
    if (BinReprBody(interp, tp, depth, vis, &inner) != TCL_OK) {
	return TCL_ERROR;
    }
    if (tp->binAt != NULL && inner != NULL) {
	if (CheckRefs(interp, tp->name, tp->binAt, vis) != TCL_OK) {
	    Tcl_DecrRefCount(inner);
	    return TCL_ERROR;
	}
	r = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("at", -1));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(tp->binAt, -1));
	Tcl_ListObjAppendElement(NULL, r, inner);
	inner = r;
    }
    *objPtr = inner;
    return TCL_OK;
}

/*
 * type binary : la représentation binaire d'un type, et la porte d'entrée de
 * la couche Tcl (layout/filebinary.tcl) qui lit et écrit des octets.
 *
 *   type binary T                     la représentation de T (forme nue)
 *   type binary {what?}T              idem
 *   type binary {sizeof?}T ?opts?     la taille en octets de T
 *   type binary {is?}T DATA ?opts?    DATA est-elle valide pour T ? (0 ou 1)
 *   type binary decode|encode|deref|space ...
 *
 * Les interrogations sont des annotations LOCALES (voir CONCEPTION-
 * annotations.md) : un texte terminé par "?" n'est jamais un nom de type, donc
 * jamais un cast. Elles s'adressent à cette commande seule, qui lit
 * l'annotation brute de son argument. Une annotation inconnue qui se termine
 * par "?" est une erreur (faute de frappe probable) ; toute autre est ignorée.
 * Les verbes d'action passent à la couche Tcl (::fb::filebinary), qui doit
 * être chargée.
 */

static const char *const binVerbs[] = {
    "decode", "encode", "deref", "space", NULL
};

static int
BinCallFb(Tcl_Interp *interp, const char *cmd, Tcl_Obj *typeObj,
	Tcl_Size nrest, Tcl_Obj *const *rest, const char *verb)
{
    Tcl_Obj **argv;
    Tcl_Size n = 0, i;
    int rc;

    if (Tcl_FindCommand(interp, cmd, NULL, 0) == NULL) {
	ERR(interp, "BIN", "type binary: the binary layer is not loaded "
		"(source layout/filebinary.tcl)");
	return TCL_ERROR;
    }
    argv = (Tcl_Obj **) Tcl_Alloc((size_t) (nrest + 3) * sizeof(Tcl_Obj *));
    argv[n++] = Tcl_NewStringObj(cmd, -1);
    if (verb != NULL) {
	argv[n++] = Tcl_NewStringObj(verb, -1);
    }
    if (typeObj != NULL) {
	argv[n++] = typeObj;
    }
    for (i = 0; i < nrest; i++) {
	argv[n++] = rest[i];
    }
    for (i = 0; i < n; i++) {
	Tcl_IncrRefCount(argv[i]);
    }
    rc = Tcl_EvalObjv(interp, n, argv, 0);
    for (i = 0; i < n; i++) {
	Tcl_DecrRefCount(argv[i]);
    }
    Tcl_Free(argv);
    return rc;
}

/* --------------------------------------------------------------------
 * Codec C de « type binary encode / decode »
 * --------------------------------------------------------------------
 *
 * Entiers u/s de 1, 2, 4 ou 8 octets, flottants de 4 ou 8 octets, produits,
 * listes de taille fixe (list(N).T), chaînes fixes et à zéro final, sommes
 * de littéraux et balisées (v16a) ; tailles et nombres dynamiques : préfixe
 * (string(utf-8,int(u,16,be)), list(int(u,8)).T) ou expression entière sur
 * les champs déjà lus ($len, $len - 4, ($a + $b) * 2) ; at(EXPR).T ; liens
 * *(ORIGINE,REPR).T (origines //, ., ./, ../, cibles quelconques, opt(T))
 * (v16d). Même disposition que la couche Tcl (layout/filebinary.tcl) :
 * remplissage avant chaque champ jusqu'à son alignement, et à la fin d'un
 * produit jusqu'au sien ; alignement d'un scalaire = la plus grande
 * puissance de 2 <= sa taille, plafonnée par -align. Le reste (bitarray,
 * référentiels nommés, -linkdepth fini, -dedupe, lien nul d'un champ à
 * défaut, $h(off) dans une expression, valeur non typée, toute erreur)
 * rend TCL_CONTINUE : la couche Tcl s'en charge, avec ses messages.
 * La variable globale tcl_typebinaryc = 0 désactive le codec (comparaisons).
 */

typedef struct {
    unsigned char *p;
    size_t len, cap;
} BinBuf;

static void
BinBufPut(BinBuf *b, const void *src, size_t n)
{
    if (b->len + n > b->cap) {
	size_t nc = b->cap ? b->cap : 256;

	while (nc < b->len + n) {
	    nc *= 2;
	}
	b->p = (unsigned char *) Tcl_Realloc(b->p, nc);
	b->cap = nc;
    }
    if (src != NULL) {
	memcpy(b->p + b->len, src, n);
    } else {
	memset(b->p + b->len, 0, n);
    }
    b->len += n;
}

static void
BinBufPad(BinBuf *b, size_t al)
{
    if (al > 1 && b->len % al != 0) {
	BinBufPut(b, NULL, al - b->len % al);
    }
}

/* Le contenu d'une valeur, sous toutes ses couches d'annotation. */
static Tcl_Obj *
BinContent(Tcl_Obj *v)
{
    int depth;

    for (depth = 0; TCL_ANNOT_P(v) && depth < 16; depth++) {
	v = TCL_ANNOT_CONTENT(v);
    }
    return v;
}

static int
BinFastScalar(const TclType *tp)
{
    if (tp->bitWidth > 0 || tp->binDyn != NULL || tp->binAt != NULL
	    || tp->binLink != NULL || tp->binTag != NULL) {
	return 0;
    }
    if ((tp->binKind == TCLBIN_UINT || tp->binKind == TCLBIN_SINT)
	    && (tp->binSize == 1 || tp->binSize == 2 || tp->binSize == 4
	    || tp->binSize == 8)) {
	return 1;
    }
    return tp->binKind == TCLBIN_FLOAT && (tp->binSize == 4
	    || tp->binSize == 8);
}

/*
 * Chaînes couvertes par le codec C : octets ou texte de taille fixe
 * (string(raw,8), string(utf-8,16)), texte terminé par un zéro (cstring),
 * et taille dynamique couverte (BinDynCovered : préfixe, expression).
 */
static int BinDynCovered(const TclBinDyn *dyn);

static int BinExprCovered(const char *text);

static int
BinFastString(const TclType *tp)
{
    if (tp->binAt != NULL || tp->binLink != NULL || tp->binTag != NULL
	    || tp->bitWidth > 0) {
	return 0;
    }
    if ((tp->binKind == TCLBIN_BYTES || tp->binKind == TCLBIN_STR)
	    && (tp->binDyn == NULL ? tp->binSize >= 0
		: BinDynCovered(tp->binDyn))
	    && (tp->binKind == TCLBIN_BYTES || tp->binEnc != NULL)) {
	return 1;
    }
    return tp->binKind == TCLBIN_CSTRING && tp->binEnc != NULL;
}

/* Somme de littéraux de même longueur ("MZ", "PE\0\0") : longueur, ou -1. */
static Tcl_Size
BinFastLit(const TclType *tp)
{
    Tcl_Size i, len = -1, l;

    if (tp->kind != TCLT_SUM || tp->binTag != NULL || tp->nfields < 1
	    || tp->binAt != NULL || tp->binLink != NULL) {
	return -1;
    }
    for (i = 0; i < tp->nfields; i++) {
	if (tp->fields[i].type != NULL || tp->fields[i].literal == NULL) {
	    return -1;
	}
	l = Tcl_GetCharLength(tp->fields[i].literal);
	if (len >= 0 && l != len) {
	    return -1;
	}
	len = l;
    }
    return len;
}

/* Largeur du zéro final d'un cstring : 2 pour utf-16 et ucs-2. */
static int
BinNulWidth(const char *enc)
{
    return (enc != NULL && (strncmp(enc, "utf-16", 6) == 0
	    || strncmp(enc, "ucs-2", 5) == 0)) ? 2 : 1;
}

/* Variante d'une somme balisée nommée par le texte tv (comme tagEq). */
static int
BinTagMatch(const TclType *sum, const char *tv)
{
    Tcl_Obj *t = Tcl_NewStringObj(tv, -1);
    Tcl_WideInt wt, wv;
    int tIsInt, v, r = -1;

    Tcl_IncrRefCount(t);
    tIsInt = (Tcl_GetWideIntFromObj(NULL, t, &wt) == TCL_OK);
    for (v = 0; v < (int) sum->nfields && r < 0; v++) {
	Tcl_Obj *name = sum->fields[v].name;

	if (name == NULL) {
	    continue;
	}
	if (strcmp(Tcl_GetString(name), tv) == 0
		|| (tIsInt && Tcl_GetWideIntFromObj(NULL, name, &wv) == TCL_OK
		&& wv == wt)) {
	    r = v;
	}
    }
    Tcl_DecrRefCount(t);
    return r;
}

/* La variante choisie par la balise : champ déjà lu ou écrit du produit
 * englobant (vals[0..nvals-1]), ou constante =texte ; -1 sinon. */
static int
BinTagVariant(const TclType *sum, const TclType *encl, Tcl_Obj **vals,
	Tcl_Size nvals)
{
    if (sum->binTag[0] == '=') {
	return BinTagMatch(sum, sum->binTag + 1);
    }
    if (encl == NULL) {
	return -1;
    }
    return TclTypeTagVariantFrom(encl, nvals, vals, nvals);
}

/* Contexte d'un encodage : options et mémo des alignements par type. */
#define BIN_MEMO 16
typedef struct {
    size_t off;
    BinBuf buf;
} BinLate;

typedef struct {
    size_t cap;
    int bigDefault, hostBig, nmemo;
    BinBuf *out;
    const TclType *memoT[BIN_MEMO];
    size_t memoA[BIN_MEMO];
    BinLate *late;		/* at(...) : posées à la fin, dans l'ordre */
    int nlate, maxlate;
    size_t wbase;		/* position de out dans la sortie (at(...)) */
    size_t *starts;		/* débuts des structures englobantes */
    int nstarts, maxstarts;
    struct BinPend *pend;	/* liens : cibles à écrire après la valeur */
    int npend, maxpend;
} BinCtx;

/* Une cible de lien en attente (encodage) : écrite après la valeur, puis
 * son adresse reportée à patch (comme flushLinks de la couche Tcl). */
typedef struct BinPend {
    size_t patch, origin;
    const TclType *repr, *target;
    Tcl_Obj *payload;		/* référence tenue */
    const TclType *encl;	/* produit dont les champs sont la portée */
    Tcl_Obj *scope;		/* ses champs (liste), référence tenue, ou NULL */
    int fin;
} BinPend;

static void
BinStartPush(size_t **sp, int *n, int *max, size_t pos)
{
    if (*n == *max) {
	*max = *max ? 2 * *max : 8;
	*sp = (size_t *) Tcl_Realloc(*sp, *max * sizeof(size_t));
    }
    (*sp)[(*n)++] = pos;
}

static void
BinPendFree(BinCtx *c, int from)
{
    int k;

    for (k = from; k < c->npend; k++) {
	Tcl_DecrRefCount(c->pend[k].payload);
	if (c->pend[k].scope != NULL) {
	    Tcl_DecrRefCount(c->pend[k].scope);
	}
    }
    c->npend = from;
}

/*
 * Liens *(ORIGINE,REPR).T dans le codec C : origines //, ., ./, ../, ../../
 * (pas les référentiels nommés), adresse entière de 1, 2, 4 ou 8 octets,
 * cible quelconque couverte, opt(T) ({T}some, {unit}none). Pas de
 * -linkdepth fini, ni de -dedupe, ni de lien de frontière, ni de lien nul
 * hors opt (champ à défaut) : la couche Tcl.
 */
static int
BinOriginPos(const char *origin, const size_t *starts, int nstarts,
	size_t here, size_t top, size_t *posPtr)
{
    size_t len = strlen(origin), k;
    int i;

    if (strcmp(origin, "//") == 0) {
	*posPtr = top;
	return 1;
    }
    if (strcmp(origin, ".") == 0) {
	*posPtr = here;
	return 1;
    }
    if (strcmp(origin, "./") == 0) {
	*posPtr = nstarts ? starts[nstarts - 1] : top;
	return 1;
    }
    if (len == 0 || len % 3 != 0) {
	return 0;
    }
    for (k = 0; k < len; k += 3) {
	if (strncmp(origin + k, "../", 3) != 0) {
	    return 0;			/* un référentiel nommé : couche Tcl */
	}
    }
    i = nstarts - 1 - (int) (len / 3);
    if (i < 0) {
	return 0;			/* le message : couche Tcl */
    }
    *posPtr = starts[i];
    return 1;
}

static int
BinOriginCovered(const char *origin)
{
    size_t len = strlen(origin), k;

    if (strcmp(origin, "//") == 0 || strcmp(origin, ".") == 0
	    || strcmp(origin, "./") == 0) {
	return 1;
    }
    if (len == 0 || len % 3 != 0) {
	return 0;
    }
    for (k = 0; k < len; k += 3) {
	if (strncmp(origin + k, "../", 3) != 0) {
	    return 0;
	}
    }
    return 1;
}

/* Lien vers opt(T) : le type T de {T}some (et les indices des variantes),
 * ou NULL (comme LinkReprBody). */
static const TclType *
BinLinkSome(const TclType *tp, int *someIdx, int *noneIdx)
{
    const TclType *t = tp->binLink->target, *some = NULL;
    Tcl_Size i;
    int hasNone = 0;

    if (t->kind != TCLT_SUM || t->binTag != NULL || t->binLink != NULL
	    || t->nfields != 2) {
	return NULL;
    }
    for (i = 0; i < t->nfields; i++) {
	const char *nm = t->fields[i].name ? Tcl_GetString(t->fields[i].name)
		: t->fields[i].literal ? Tcl_GetString(t->fields[i].literal)
		: "";			/* v16w : "none" est un littéral */

	if (strcmp(nm, "some") == 0) {
	    some = t->fields[i].type;
	    if (someIdx) {
		*someIdx = (int) i;
	    }
	} else if (strcmp(nm, "none") == 0) {
	    hasNone = 1;
	    if (noneIdx) {
		*noneIdx = (int) i;
	    }
	}
    }
    return hasNone ? some : NULL;
}

/* La valeur nulle d'un lien (texte), ou NULL s'il n'en a pas. */
static const char *
BinLinkNull(const TclType *tp)
{
    if (tp->binLink->nullVal != NULL) {
	return tp->binLink->nullVal;
    }
    return BinLinkSome(tp, NULL, NULL) != NULL ? "0" : NULL;
}

static size_t BinFastAlign(const TclType *tp, size_t cap, int depth);

static size_t
BinAlignOf(BinCtx *c, const TclType *tp, int depth)
{
    int k;
    size_t a;

    for (k = 0; k < c->nmemo; k++) {
	if (c->memoT[k] == tp) {
	    return c->memoA[k];
	}
    }
    a = BinFastAlign(tp, c->cap, depth);
    if (c->nmemo < BIN_MEMO) {
	c->memoT[c->nmemo] = tp;
	c->memoA[c->nmemo++] = a;
    }
    return a;
}

/* Les cibles des liens atteignables depuis tp sont-elles couvertes ?
 * (BinFastAlign ne suit pas les liens : une liste chaînée est récursive) */
static int
BinLinksCovered(const TclType *tp, size_t cap, const TclType **seen,
	int *nseen, int depth)
{
    Tcl_Size i;
    int k;

    if (tp == NULL || depth > 64) {
	return 0;
    }
    for (k = 0; k < *nseen; k++) {
	if (seen[k] == tp) {
	    return 1;
	}
    }
    if (*nseen >= 64) {
	return 0;
    }
    seen[(*nseen)++] = tp;
    if (tp->binLink != NULL) {
	const TclType *t = BinLinkSome(tp, NULL, NULL);

	if (t == NULL) {
	    t = tp->binLink->target;
	}
	return BinFastAlign(t, cap, 0) != 0
		&& BinLinksCovered(t, cap, seen, nseen, depth + 1);
    }
    if (tp->binAt != NULL) {
	return BinLinksCovered(tp->binAtBody, cap, seen, nseen, depth + 1);
    }
    if (tp->kind == TCLT_LIST && tp->elem != NULL) {
	return BinLinksCovered(tp->elem, cap, seen, nseen, depth + 1);
    }
    if (tp->kind == TCLT_PRODUCT || (tp->kind == TCLT_SUM
	    && tp->binTag != NULL)) {
	for (i = 0; i < tp->nfields; i++) {
	    if (tp->fields[i].type != NULL && !BinLinksCovered(
		    tp->fields[i].type, cap, seen, nseen, depth + 1)) {
		return 0;
	    }
	}
    }
    return 1;
}

/* Alignement d'un type couvert ; 0 si le type n'est pas couvert. */
static size_t
BinFastAlign(const TclType *tp, size_t cap, int depth)
{
    Tcl_Size i;
    size_t m = 1, a;

    if (depth > 32) {
	return 0;
    }
    if (tp->binLink != NULL) {
	/* *(ORIGINE,REPR).T : l'adresse ici (la cible : BinLinksCovered) */
	if (tp->binAt != NULL || !BinOriginCovered(tp->binLink->origin)
		|| !BinFastScalar(tp->binLink->repr)) {
	    return 0;
	}
	return BinFastAlign(tp->binLink->repr, cap, depth + 1);
    }
    if (tp->binAt != NULL) {
	/* at(EXPR).T : ailleurs ; alignement 1 ici, comme la couche Tcl */
	return (tp->binAtBody != NULL && tp->binLink == NULL
		&& BinExprCovered(tp->binAt)
		&& BinFastAlign(tp->binAtBody, cap, depth + 1) != 0) ? 1 : 0;
    }
    if (BinFastScalar(tp)) {
	size_t p = 1;

	while (p * 2 <= (size_t) tp->binSize) {
	    p *= 2;
	}
	return (cap <= 1) ? 1 : (p < cap ? p : cap);
    }
    if (BinFastString(tp) || BinFastLit(tp) >= 0) {
	return 1;			/* octets : pas d'alignement */
    }
    if (tp->kind == TCLT_SUM && tp->binTag != NULL && tp->binAt == NULL
	    && tp->binLink == NULL && tp->nfields > 0) {
	for (i = 0; i < tp->nfields; i++) {
	    if (tp->fields[i].type == NULL
		    || BinFastAlign(tp->fields[i].type, cap, depth + 1) == 0) {
		return 0;
	    }
	}
	return 1;			/* comme la couche Tcl (alignOf tagged) */
    }
    if (tp->binAt != NULL || tp->binLink != NULL || tp->binTag != NULL) {
	return 0;
    }
    if (tp->kind == TCLT_LIST && tp->elem != NULL) {
	if (tp->binDyn != NULL && !BinDynCovered(tp->binDyn)) {
	    return 0;
	}
	if (tp->binDyn == NULL && tp->arity < 0) {
	    return 0;			/* liste sans taille : pas binaire */
	}
	return BinFastAlign(tp->elem, cap, depth + 1);
    }
    if (tp->kind == TCLT_PRODUCT && tp->nfields > 0) {
	for (i = 0; i < tp->nfields; i++) {
	    if (tp->fields[i].type == NULL) {
		return 0;
	    }
	    a = BinFastAlign(tp->fields[i].type, cap, depth + 1);
	    if (a == 0) {
		return 0;
	    }
	    if (a > m) {
		m = a;
	    }
	}
	return (cap <= 1) ? 1 : m;
    }
    return 0;
}

/*
 * Cases d'une liste compacte et octets du codec : quand un élément a, sous
 * les options de l'appel (-align, -endian), exactement la disposition de sa
 * case (mêmes scalaires aux mêmes décalages, même taille, boutisme de la
 * machine), une liste en cases s'encode et se décode par copie de ses
 * scalaires, sans objet par élément. Le remplissage reste à zéro.
 */
#define BIN_FLAT_MAX 64

static size_t
BinAlignUp(size_t x, size_t a)
{
    return (a > 1) ? (x + a - 1) / a * a : x;
}

/* Disposition binaire d'un élément, à plat ; nombre de scalaires ou -1. */
static int
BinFlat(BinCtx *c, const TclType *tp, size_t *pos, int depth, int k,
	size_t off[], int size[], int isFloat[])
{
    Tcl_Size i;

    if (depth > 32 || tp->binLink != NULL || tp->binAt != NULL) {
	return -1;			/* un lien, at(...) : pas une case */
    }
    if (BinFastScalar(tp)) {
	int big = tp->binEnd == TCLBIN_BE
		|| (tp->binEnd == TCLBIN_AUTO && c->bigDefault);

	if (big != c->hostBig || k >= BIN_FLAT_MAX) {
	    return -1;
	}
	off[k] = *pos;
	size[k] = (int) tp->binSize;
	isFloat[k] = (tp->binKind == TCLBIN_FLOAT);
	*pos += (size_t) tp->binSize;
	return k + 1;
    }
    if (tp->kind != TCLT_PRODUCT || tp->nfields < 1) {
	return -1;
    }
    for (i = 0; i < tp->nfields; i++) {
	const TclType *ft = tp->fields[i].type;

	if (ft == NULL) {
	    return -1;
	}
	*pos = BinAlignUp(*pos, BinAlignOf(c, ft, depth + 1));
	k = BinFlat(c, ft, pos, depth + 1, k, off, size, isFloat);
	if (k < 0) {
	    return -1;
	}
    }
    *pos = BinAlignUp(*pos, BinAlignOf(c, tp, depth));
    return k;
}

/* Nombre de scalaires (off, size remplis) si les cases de et ont la
 * disposition binaire ; -1 sinon. */
static int
BinCellsMatch(BinCtx *c, const TclType *et, size_t *stridePtr, size_t off[],
	int size[])
{
    size_t coff[BIN_FLAT_MAX], stride, pos = 0;
    int csize[BIN_FLAT_MAX], cfl[BIN_FLAT_MAX], fl[BIN_FLAT_MAX], nc, nb, i;

    nc = TclCompactFlat((TclType *) et, &stride, BIN_FLAT_MAX, coff, csize,
	    cfl);
    if (nc <= 0) {
	return -1;
    }
    nb = BinFlat(c, et, &pos, 1, 0, off, size, fl);
    if (nb != nc || pos != stride) {
	return -1;
    }
    for (i = 0; i < nc; i++) {
	if (off[i] != coff[i] || size[i] != csize[i] || fl[i] != cfl[i]) {
	    return -1;
	}
    }
    *stridePtr = stride;
    return nc;
}

/* Copie les scalaires de n cases (le remplissage de dst reste tel quel). */
static void
BinCopyCells(unsigned char *dst, const unsigned char *src, Tcl_Size n,
	size_t stride, int ns, const size_t off[], const int size[])
{
    Tcl_Size i;
    int k;
    size_t sum = 0;

    for (k = 0; k < ns; k++) {
	sum += (size_t) size[k];
    }
    if (sum == stride) {
	memcpy(dst, src, (size_t) n * stride);	/* sans remplissage : un bloc */
	return;
    }
    for (i = 0; i < n; i++) {
	size_t base = (size_t) i * stride;

	for (k = 0; k < ns; k++) {
	    memcpy(dst + base + off[k], src + base + off[k], (size_t) size[k]);
	}
    }
}

/*
 * Tailles et nombres dynamiques (TclBinDyn) dans le codec C.
 *
 * Expression : le sous-ensemble entier d'expr que la couche Tcl évalue le
 * plus souvent, sur les champs déjà lus ou écrits du produit englobant :
 * $champ, entiers, + - * / % (division et reste au plancher, comme expr),
 * moins unaire, parenthèses. Tout le reste ($h(off), champ d'un produit
 * extérieur, nombre non entier, division par zéro, taille négative) rend
 * TCL_CONTINUE : la couche Tcl, avec ses messages.
 */
typedef struct {
    const char *p;
    const TclType *encl;
    Tcl_Obj **vals;
    Tcl_Size nvals;
    int bad;
} BinExprCtx;

static Tcl_WideInt BinExprSum(BinExprCtx *e);

static void
BinExprSkip(BinExprCtx *e)
{
    while (*e->p == ' ' || *e->p == '\t') {
	e->p++;
    }
}

static Tcl_WideInt
BinExprAtom(BinExprCtx *e)
{
    Tcl_WideInt v = 0;

    BinExprSkip(e);
    if (*e->p == '-') {
	e->p++;
	return -BinExprAtom(e);
    }
    if (*e->p == '+') {
	e->p++;
	return BinExprAtom(e);
    }
    if (*e->p == '(') {
	e->p++;
	v = BinExprSum(e);
	BinExprSkip(e);
	if (*e->p != ')') {
	    e->bad = 1;
	    return 0;
	}
	e->p++;
	return v;
    }
    if (*e->p >= '0' && *e->p <= '9') {
	if (e->p[0] == '0' && e->p[1] != '\0' && strchr("xXbBoO", e->p[1])) {
	    e->bad = 1;			/* 0x10... : la couche Tcl */
	    return 0;
	}
	while (*e->p >= '0' && *e->p <= '9') {
	    if (v > (WIDE_MAX - 9) / 10) {
		e->bad = 1;
		return 0;
	    }
	    v = v * 10 + (*e->p++ - '0');
	}
	return v;
    }
    if (*e->p == '$') {
	const char *q = ++e->p;
	Tcl_Size f, len;

	while (isalnum(UCHAR(*e->p)) || *e->p == '_') {
	    e->p++;
	}
	len = e->p - q;
	if (len == 0 || *e->p == '(' || *e->p == ':' || e->encl == NULL) {
	    e->bad = 1;			/* $h(off), ${x}, ::x : la couche Tcl */
	    return 0;
	}
	for (f = 0; f < e->nvals; f++) {
	    const char *fn = (e->encl->fields[f].name != NULL)
		    ? Tcl_GetString(e->encl->fields[f].name) : NULL;

	    if (fn != NULL && (Tcl_Size) strlen(fn) == len
		    && strncmp(fn, q, (size_t) len) == 0) {
		break;
	    }
	}
	if (f == e->nvals || Tcl_GetWideIntFromObj(NULL,
		BinContent(e->vals[f]), &v) != TCL_OK) {
	    e->bad = 1;
	    return 0;
	}
	return v;
    }
    e->bad = 1;
    return 0;
}

static Tcl_WideInt
BinExprProd(BinExprCtx *e)
{
    Tcl_WideInt v = BinExprAtom(e), w, q;

    for (;;) {
	char op;

	BinExprSkip(e);
	op = *e->p;
	if (e->bad || (op != '*' && op != '/' && op != '%') || e->p[1] == '*') {
	    return v;			/* ** : la couche Tcl */
	}
	e->p++;
	w = BinExprAtom(e);
	if (e->bad) {
	    return 0;
	}
	if (op == '*') {
	    v *= w;
	} else if (w == 0) {
	    e->bad = 1;
	    return 0;
	} else {
	    /* division et reste au plancher, comme expr */
	    q = v / w;
	    if ((v % w != 0) && ((v < 0) != (w < 0))) {
		q--;
	    }
	    v = (op == '/') ? q : v - q * w;
	}
    }
}

static Tcl_WideInt
BinExprSum(BinExprCtx *e)
{
    Tcl_WideInt v = BinExprProd(e), w;

    for (;;) {
	char op;

	BinExprSkip(e);
	op = *e->p;
	if (e->bad || (op != '+' && op != '-')) {
	    return v;
	}
	e->p++;
	w = BinExprProd(e);
	v = (op == '+') ? v + w : v - w;
    }
}

/* L'expression est-elle de la forme couverte ? (syntaxe seulement) */
static int
BinExprCovered(const char *text)
{
    const char *p;

    for (p = text; *p != '\0'; p++) {
	if (strchr("0123456789+-*/%() \t$_", *p) == NULL && !isalnum(UCHAR(*p))) {
	    return 0;
	}
	if (*p == '$' && (p[1] == '{' || !(isalpha(UCHAR(p[1])) || p[1] == '_'))) {
	    return 0;
	}
	if (*p == '*' && p[1] == '*') {
	    return 0;
	}
    }
    return 1;
}

/* Une taille dynamique couverte : préfixe entier 1, 2, 4 ou 8 octets, ou
 * expression de la forme couverte. */
static int
BinDynCovered(const TclBinDyn *dyn)
{
    if (dyn->kind == TCLDYN_PREFIX) {
	return dyn->pre != NULL && BinFastScalar(dyn->pre)
		&& dyn->pre->binKind == TCLBIN_UINT;
    }
    return dyn->kind == TCLDYN_EXPR && dyn->expr != NULL
	    && BinExprCovered(dyn->expr);
}

/* La valeur d'une expression de taille ou de position : TCL_OK, ou
 * TCL_CONTINUE. */
static int
BinExprValue(const char *text, const TclType *encl, Tcl_Obj **vals,
	Tcl_Size nvals, Tcl_WideInt *wantPtr)
{
    BinExprCtx e;
    Tcl_WideInt v;

    e.p = text;
    e.encl = encl;
    e.vals = vals;
    e.nvals = nvals;
    e.bad = 0;
    v = BinExprSum(&e);
    BinExprSkip(&e);
    if (e.bad || *e.p != '\0' || v < 0) {
	return TCL_CONTINUE;
    }
    *wantPtr = v;
    return TCL_OK;
}

static int
BinDynExpr(const TclBinDyn *dyn, const TclType *encl, Tcl_Obj **vals,
	Tcl_Size nvals, Tcl_WideInt *wantPtr)
{
    return BinExprValue(dyn->expr, encl, vals, nvals, wantPtr);
}

static int BinFastEnc(BinCtx *c, const TclType *tp, Tcl_Obj *v,
	const TclType *encl, Tcl_Obj **vals, Tcl_Size nvals, int depth);
static int BinFlushLinks(BinCtx *c);

/* Écriture : n éléments ou octets. Préfixe : écrit (tronqué comme binary
 * format) ; expression : doit valoir n. */
static int
BinDynWrite(BinCtx *c, const TclBinDyn *dyn, Tcl_Size n, const TclType *encl,
	Tcl_Obj **vals, Tcl_Size nvals)
{
    Tcl_WideInt want;
    Tcl_Obj *o;
    int rc;

    if (dyn->kind == TCLDYN_PREFIX) {
	o = Tcl_NewWideIntObj((Tcl_WideInt) n);
	Tcl_IncrRefCount(o);
	rc = BinFastEnc(c, dyn->pre, o, NULL, NULL, 0, 1);
	Tcl_DecrRefCount(o);
	return rc;
    }
    if (BinDynExpr(dyn, encl, vals, nvals, &want) != TCL_OK
	    || want != (Tcl_WideInt) n) {
	return TCL_CONTINUE;		/* le message : couche Tcl */
    }
    return TCL_OK;
}

static int
BinFastEnc(BinCtx *c, const TclType *tp, Tcl_Obj *v, const TclType *encl,
	Tcl_Obj **vals, Tcl_Size nvals, int depth)
{
    BinBuf *out = c->out;
    int bigDefault = c->bigDefault, hostBig = c->hostBig;
    Tcl_Obj **elv, *orig = v;
    Tcl_Size n, i;

    v = BinContent(v);
    if (tp->binLink != NULL) {
	/* *(ORIGINE,REPR).T : l'adresse 0 ici, la cible après la valeur
	 * (BinFlushLinks), son adresse reportée ensuite */
	const TclBinLink *lk = tp->binLink;
	const TclType *target = lk->target, *some;
	Tcl_Obj *payload = orig, *zero;
	TclType *ot = NULL;
	size_t origin;
	int someIdx = -1, noneIdx = -1, var = -1, rc;

	if (lk->ref != NULL && TclAnnotInfo(orig, NULL, &ot, NULL, NULL) == 1
		&& ot == lk->ref) {
	    return TCL_CONTINUE;	/* lien de frontière : couche Tcl */
	}
	some = BinLinkSome(tp, &someIdx, &noneIdx);
	if (some != NULL) {
	    Tcl_Obj *content = NULL;

	    if (TclAnnotInfo(orig, &content, &ot, &var, NULL) != 1) {
		/* champ d'un produit : contenu nu, la variante se déduit
		 * (comme la lecture $x(champ) de la couche Tcl) */
		Tcl_Size ln = 1;

		ot = lk->target;
		content = v;
		var = -1;
		if (Tcl_ListObjLength(NULL, content, &ln) != TCL_OK) {
		    return TCL_CONTINUE;
		}
		if (ln > 0) {
		    var = someIdx;	/* none est "" : un contenu non vide est
					 * some (sa forme : à l'encodage) ; pas de
					 * validation de toute la suite */
		} else if (TclTypeValidate(NULL, ot, content, -1, &var)
			!= TCL_OK) {
		    return TCL_CONTINUE;
		}
	    }
	    if (ot != NULL && TclSumTagged(ot)) {
		/* v16w : "some charge" ou "none" */
		Tcl_Obj *pl = NULL;

		var = TclSumDecode(ot, content, &pl);
		content = pl;
	    }
	    if (ot == NULL || ot->kind != TCLT_SUM || var < 0
		    || var >= (int) ot->nfields) {
		return TCL_CONTINUE;
	    }
	    if (strcmp(Tcl_GetString(ot->fields[var].name != NULL
		    ? ot->fields[var].name : ot->fields[var].literal), "none")
		    == 0) {
		Tcl_WideInt w;
		Tcl_Obj *nt = Tcl_NewStringObj(BinLinkNull(tp), -1);

		Tcl_IncrRefCount(nt);
		rc = Tcl_GetWideIntFromObj(NULL, nt, &w);
		Tcl_DecrRefCount(nt);
		if (rc != TCL_OK) {
		    return TCL_CONTINUE;
		}
		zero = Tcl_NewWideIntObj(w);
		Tcl_IncrRefCount(zero);
		rc = BinFastEnc(c, lk->repr, zero, NULL, NULL, 0, depth + 1);
		Tcl_DecrRefCount(zero);
		return rc;
	    }
	    if (ot->fields[var].name == NULL || content == NULL
		    || strcmp(Tcl_GetString(ot->fields[var].name), "some")
		    != 0) {
		return TCL_CONTINUE;
	    }
	    payload = content;
	    target = some;
	}
	if (!BinOriginPos(lk->origin, c->starts, c->nstarts,
		c->wbase + out->len, 0, &origin)) {
	    return TCL_CONTINUE;
	}
	if (c->npend == c->maxpend) {
	    c->maxpend = c->maxpend ? 2 * c->maxpend : 8;
	    c->pend = (BinPend *) Tcl_Realloc(c->pend,
		    c->maxpend * sizeof(BinPend));
	}
	c->pend[c->npend].patch = out->len;
	c->pend[c->npend].origin = origin;
	c->pend[c->npend].repr = lk->repr;
	c->pend[c->npend].target = target;
	c->pend[c->npend].payload = payload;
	Tcl_IncrRefCount(payload);
	c->pend[c->npend].encl = NULL;
	c->pend[c->npend].scope = NULL;
	c->pend[c->npend].fin = 0;
	c->npend++;
	zero = Tcl_NewWideIntObj(0);
	Tcl_IncrRefCount(zero);
	rc = BinFastEnc(c, lk->repr, zero, NULL, NULL, 0, depth + 1);
	Tcl_DecrRefCount(zero);
	return rc;
    }
    if (tp->binAt != NULL) {
	/* at(EXPR).T : encodé à part, posé à la position EXPR à la fin (le
	 * remplissage y est compté depuis le début de la partie, comme dans
	 * la couche Tcl) */
	Tcl_WideInt off;
	BinBuf sub = {NULL, 0, 0};
	int rc;

	if (tp->binAtBody == NULL || BinExprValue(tp->binAt, encl, vals, nvals,
		&off) != TCL_OK) {
	    return TCL_CONTINUE;
	}
	{
	    /* comme la couche Tcl : wbase = la position, pas de structure
	     * englobante, des liens à part (écrits dans la partie) */
	    size_t sw = c->wbase, *ss = c->starts;
	    int sn = c->nstarts, sm = c->maxstarts, pn = c->npend;
	    int pm = c->maxpend;
	    BinPend *pp = c->pend;

	    c->wbase = (size_t) off;
	    c->starts = NULL;
	    c->nstarts = c->maxstarts = 0;
	    c->pend = NULL;
	    c->npend = c->maxpend = 0;
	    c->out = &sub;
	    rc = BinFastEnc(c, tp->binAtBody, v, encl, vals, nvals, depth + 1);
	    if (rc == TCL_OK) {
		rc = BinFlushLinks(c);
	    }
	    BinPendFree(c, 0);
	    if (c->pend != NULL) {
		Tcl_Free(c->pend);
	    }
	    if (c->starts != NULL) {
		Tcl_Free(c->starts);
	    }
	    c->out = out;
	    c->wbase = sw;
	    c->starts = ss;
	    c->nstarts = sn;
	    c->maxstarts = sm;
	    c->pend = pp;
	    c->npend = pn;
	    c->maxpend = pm;
	}
	if (rc != TCL_OK) {
	    if (sub.p != NULL) {
		Tcl_Free(sub.p);
	    }
	    return rc;
	}
	if (c->nlate == c->maxlate) {
	    c->maxlate = c->maxlate ? 2 * c->maxlate : 8;
	    c->late = (BinLate *) Tcl_Realloc(c->late,
		    c->maxlate * sizeof(BinLate));
	}
	c->late[c->nlate].off = (size_t) off;
	c->late[c->nlate++].buf = sub;
	return TCL_OK;
    }
    if (tp->kind == TCLT_SUM && tp->binTag != NULL) {
	/* somme balisée : la balise (déjà écrite) choisit la variante, qui
	 * doit être celle de la valeur */
	int var = BinTagVariant(tp, encl, vals, nvals), own = -1;
	TclType *ot = NULL;

	if (var < 0 || tp->fields[var].type == NULL) {
	    return TCL_CONTINUE;		/* le message : couche Tcl */
	}
	if (TclAnnotInfo(orig, NULL, &ot, &own, NULL) == 1
		&& (ot != tp || own != var)) {
	    return TCL_CONTINUE;
	}
	return BinFastEnc(c, tp->fields[var].type, v, NULL, NULL, 0,
		depth + 1);
    }
    if (BinFastLit(tp) >= 0) {
	unsigned char *b = Tcl_GetBytesFromObj(NULL, v, &n);

	if (b == NULL) {
	    return TCL_CONTINUE;
	}
	BinBufPut(out, b, (size_t) n);
	return TCL_OK;
    }
    if (BinFastString(tp)) {
	if (tp->binKind == TCLBIN_BYTES) {
	    unsigned char *b = Tcl_GetBytesFromObj(NULL, v, &n);
	    int rc;

	    if (b == NULL) {
		return TCL_CONTINUE;
	    }
	    if (tp->binDyn != NULL) {
		/* string(raw,$len), string(raw,int(u,16)) : la taille d'abord */
		rc = BinDynWrite(c, tp->binDyn, n, encl, vals, nvals);
		if (rc != TCL_OK) {
		    return rc;
		}
		b = Tcl_GetBytesFromObj(NULL, v, &n);	/* v n'a pas changé */
	    }
	    BinBufPut(out, b, (size_t) n);	/* comme la couche Tcl */
	    return TCL_OK;
	} else {
	    Tcl_Encoding enc = Tcl_GetEncoding(NULL, tp->binEnc);
	    Tcl_DString ds;
	    Tcl_Size len;
	    const char *str = Tcl_GetStringFromObj(v, &len);
	    int rc;

	    if (enc == NULL) {
		return TCL_CONTINUE;
	    }
	    Tcl_DStringInit(&ds);
	    rc = Tcl_UtfToExternalDStringEx(NULL, enc, str, len,
		    TCL_ENCODING_PROFILE_STRICT, &ds, NULL);
	    Tcl_FreeEncoding(enc);
	    n = Tcl_DStringLength(&ds);
	    if (rc != TCL_OK || (tp->binKind == TCLBIN_STR
		    && tp->binDyn == NULL && n > tp->binSize)) {
		Tcl_DStringFree(&ds);
		return TCL_CONTINUE;
	    }
	    if (tp->binDyn != NULL) {
		/* string(utf-8,$len), string(utf-8,int(u,16,be)) : la taille
		 * en octets, puis le texte, sans remplissage */
		rc = BinDynWrite(c, tp->binDyn, n, encl, vals, nvals);
		if (rc == TCL_OK) {
		    BinBufPut(out, Tcl_DStringValue(&ds), (size_t) n);
		}
		Tcl_DStringFree(&ds);
		return rc;
	    }
	    BinBufPut(out, Tcl_DStringValue(&ds), (size_t) n);
	    Tcl_DStringFree(&ds);
	    BinBufPut(out, NULL, tp->binKind == TCLBIN_STR
		    ? (size_t) (tp->binSize - n)
		    : (size_t) BinNulWidth(tp->binEnc));
	    return TCL_OK;
	}
    }
    if (BinFastScalar(tp)) {
	unsigned char buf[8], tmp;
	int big = tp->binEnd == TCLBIN_BE
		|| (tp->binEnd == TCLBIN_AUTO && bigDefault);
	size_t sz = (size_t) tp->binSize, k;

	if (tp->binKind == TCLBIN_FLOAT) {
	    double d;

	    if (Tcl_GetDoubleFromObj(NULL, v, &d) != TCL_OK) {
		return TCL_CONTINUE;
	    }
	    if (sz == 4) {
		float f = (float) d;

		memcpy(buf, &f, 4);
	    } else {
		memcpy(buf, &d, 8);
	    }
	    if (big != hostBig) {
		for (k = 0; k < sz / 2; k++) {
		    tmp = buf[k];
		    buf[k] = buf[sz - 1 - k];
		    buf[sz - 1 - k] = tmp;
		}
	    }
	} else {
	    Tcl_WideInt w;
	    unsigned long long u;

	    if (Tcl_GetWideIntFromObj(NULL, v, &w) != TCL_OK) {
		return TCL_CONTINUE;	/* u64 > 2^63, texte... : la couche Tcl */
	    }
	    u = (unsigned long long) w;
	    for (k = 0; k < sz; k++) {	/* octet de poids faible d'abord */
		buf[big ? sz - 1 - k : k] = (unsigned char) (u >> (8 * k));
	    }
	}
	BinBufPut(out, buf, sz);
	return TCL_OK;
    }
    if (tp->kind == TCLT_LIST && tp->elem != NULL) {
	/* une liste en cases à la disposition binaire : copie des scalaires */
	const unsigned char *cells;
	size_t off[BIN_FLAT_MAX], stride;
	int size[BIN_FLAT_MAX], ns;
	Tcl_WideInt want;

	(void) want;
	if ((cells = TclCompactData(v, tp->elem, &n)) != NULL
		&& (ns = BinCellsMatch(c, tp->elem, &stride, off, size)) > 0
		&& (tp->binDyn == NULL || tp->binDyn->kind != TCLDYN_PREFIX
		    || (out->len + (size_t) tp->binDyn->pre->binSize)
			% BinAlignOf(c, tp->elem, depth + 1) == 0)) {
	    /* (après un préfixe, les cases doivent rester alignées) */
	    if (tp->binDyn != NULL) {
		int rc = BinDynWrite(c, tp->binDyn, n, encl, vals, nvals);

		if (rc != TCL_OK) {
		    return rc;
		}
	    } else if (n != tp->arity) {
		return TCL_CONTINUE;
	    }
	    {
		size_t at = out->len;

		BinBufPut(out, NULL, (size_t) n * stride);
		BinCopyCells(out->p + at, cells, n, stride, ns, off, size);
	    }
	    return TCL_OK;
	}
    }
    if (Tcl_ListObjGetElements(NULL, v, &n, &elv) != TCL_OK) {
	return TCL_CONTINUE;
    }
    if (tp->kind == TCLT_LIST) {
	if (tp->binDyn != NULL) {
	    /* list($n).T, list(int(u,8)).T : nombre vérifié, ou préfixe écrit */
	    int rc = BinDynWrite(c, tp->binDyn, n, encl, vals, nvals);

	    if (rc != TCL_OK) {
		return rc;
	    }
	} else if (n != tp->arity) {
	    return TCL_CONTINUE;
	}
	for (i = 0; i < n; i++) {
	    int rc = BinFastEnc(c, tp->elem, elv[i], NULL, NULL, 0, depth + 1);

	    if (rc != TCL_OK) {
		return rc;
	    }
	}
	return TCL_OK;
    }
    /* produit : les champs, dans l'ordre du type */
    if (n != tp->nfields) {
	return TCL_CONTINUE;
    }
    {
	int n0 = c->npend, ns0 = c->nstarts, k;

	for (i = 0; i < n; i++) {
	    const TclType *ft = tp->fields[i].type;
	    int rc;

	    if (ft->binLink != NULL && tp->fields[i].defval != NULL) {
		c->nstarts = ns0;
		return TCL_CONTINUE;	/* lien nul possible : couche Tcl */
	    }
	    BinBufPad(out, BinAlignOf(c, ft, depth + 1));
	    if (i == 0) {
		/* le début de la structure : son premier champ, aligné */
		BinStartPush(&c->starts, &c->nstarts, &c->maxstarts,
			c->wbase + out->len);
	    }
	    rc = BinFastEnc(c, ft, elv[i], tp, elv, i, depth + 1);
	    if (rc != TCL_OK) {
		c->nstarts = ns0;
		return rc;
	    }
	}
	c->nstarts = ns0;
	/* les cibles des liens voient TOUS les champs du produit */
	for (k = n0; k < c->npend; k++) {
	    if (!c->pend[k].fin) {
		c->pend[k].encl = tp;
		c->pend[k].scope = v;
		Tcl_IncrRefCount(v);
		c->pend[k].fin = 1;
	    }
	}
    }
    BinBufPad(out, BinAlignOf(c, tp, depth));
    return TCL_OK;
}

/* Les cibles des liens en attente, à la suite de la sortie (alignées), puis
 * leurs adresses reportées ; une cible peut en ajouter d'autres. */
static int
BinFlushLinks(BinCtx *c)
{
    int k, rc;

    for (k = 0; k < c->npend; k++) {
	BinPend it = c->pend[k];	/* copie : pend peut être agrandi */
	BinBuf *out = c->out, tmp = {NULL, 0, 0};
	Tcl_Obj **sv = NULL, *relObj;
	Tcl_Size sn = 0;
	size_t tpos, *ss = c->starts;
	int sns = c->nstarts, sms = c->maxstarts;
	Tcl_WideInt rel;

	BinBufPad(out, BinAlignOf(c, it.target, 1));
	tpos = c->wbase + out->len;
	if (it.scope != NULL && Tcl_ListObjGetElements(NULL, it.scope, &sn,
		&sv) != TCL_OK) {
	    return TCL_CONTINUE;
	}
	c->starts = NULL;		/* starts {} pour la cible */
	c->nstarts = c->maxstarts = 0;
	rc = BinFastEnc(c, it.target, it.payload, it.encl, sv,
		(it.encl != NULL) ? sn : 0, 1);
	if (c->starts != NULL) {
	    Tcl_Free(c->starts);
	}
	c->starts = ss;
	c->nstarts = sns;
	c->maxstarts = sms;
	if (rc != TCL_OK) {
	    return rc;
	}
	rel = (Tcl_WideInt) tpos - (Tcl_WideInt) it.origin;
	if (rel < 0 && it.repr->binKind == TCLBIN_UINT) {
	    return TCL_CONTINUE;	/* le message : couche Tcl */
	}
	relObj = Tcl_NewWideIntObj(rel);
	Tcl_IncrRefCount(relObj);
	c->out = &tmp;
	rc = BinFastEnc(c, it.repr, relObj, NULL, NULL, 0, 1);
	c->out = out;
	Tcl_DecrRefCount(relObj);
	if (rc == TCL_OK) {
	    memcpy(out->p + it.patch, tmp.p, tmp.len);
	}
	if (tmp.p != NULL) {
	    Tcl_Free(tmp.p);
	}
	if (rc != TCL_OK) {
	    return rc;
	}
    }
    return TCL_OK;
}

/* type binary encode TYPE VALUE ?-endian E? ?-align A? : TCL_OK (résultat
 * posé), TCL_ERROR, ou TCL_CONTINUE (la couche Tcl prend le relais). */
static int
BinEncodeFast(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const char *flag = Tcl_GetVar2(interp, "tcl_typebinaryc", NULL,
	    TCL_GLOBAL_ONLY);
    TclType *tp, *vt = NULL;
    size_t cap = 1;
    int bigDefault = 0, hostBig, rc;
    union { unsigned short s; unsigned char c[2]; } probe;
    Tcl_Size i, len;
    const char *text;
    BinBuf out = {NULL, 0, 0};
    BinCtx ctx;

    if ((flag != NULL && strcmp(flag, "0") == 0) || objc < 2 || objc % 2) {
	return TCL_CONTINUE;
    }
    for (i = 2; i < objc; i += 2) {
	const char *o = Tcl_GetString(objv[i]), *a = Tcl_GetString(objv[i + 1]);

	if (strcmp(o, "-endian") == 0 && strcmp(a, "little") == 0) {
	    bigDefault = 0;
	} else if (strcmp(o, "-endian") == 0 && strcmp(a, "big") == 0) {
	    bigDefault = 1;
	} else if (strcmp(o, "-align") == 0 && strcmp(a, "packed") == 0) {
	    cap = 1;
	} else if (strcmp(o, "-align") == 0 && strcmp(a, "natural") == 0) {
	    cap = 8;
	} else if (strcmp(o, "-align") == 0) {
	    Tcl_WideInt w;

	    if (Tcl_GetWideIntFromObj(NULL, objv[i + 1], &w) != TCL_OK
		    || w < 1 || (w & (w - 1)) != 0) {
		return TCL_CONTINUE;
	    }
	    cap = (size_t) w;
	} else {
	    return TCL_CONTINUE;	/* -dedupe, option inconnue : couche Tcl */
	}
    }
    if (TclIsAnnotated(objv[0])) {
	return TCL_CONTINUE;
    }
    text = Tcl_GetStringFromObj(objv[0], &len);
    {
	Tcl_Obj *saved = Tcl_GetObjResult(interp);

	Tcl_IncrRefCount(saved);
	tp = TclTypeParse(interp, text, len);
	Tcl_SetObjResult(interp, saved);
	Tcl_DecrRefCount(saved);
    }
    /* la valeur doit être typée du type demandé, comme pour la couche Tcl
     * (qui lit ses champs par $x(champ)) */
    if (tp == NULL || TclAnnotInfo(objv[1], NULL, &vt, NULL, NULL) != 1
	    || !TclTypeSame(vt, tp) || BinFastAlign(tp, cap, 0) == 0) {
	return TCL_CONTINUE;
    }
    {
	const TclType *seen[64];
	int nseen = 0;

	if (!BinLinksCovered(tp, cap, seen, &nseen, 0)) {
	    return TCL_CONTINUE;
	}
    }
    probe.s = 1;
    hostBig = (probe.c[0] == 0);
    ctx.cap = cap;
    ctx.bigDefault = bigDefault;
    ctx.hostBig = hostBig;
    ctx.nmemo = 0;
    ctx.out = &out;
    ctx.late = NULL;
    ctx.nlate = ctx.maxlate = 0;
    ctx.wbase = 0;
    ctx.starts = NULL;
    ctx.nstarts = ctx.maxstarts = 0;
    ctx.pend = NULL;
    ctx.npend = ctx.maxpend = 0;
    rc = BinFastEnc(&ctx, tp, objv[1], NULL, NULL, 0, 0);
    if (rc == TCL_OK) {
	rc = BinFlushLinks(&ctx);
    }
    BinPendFree(&ctx, 0);
    if (ctx.pend != NULL) {
	Tcl_Free(ctx.pend);
    }
    if (ctx.starts != NULL) {
	Tcl_Free(ctx.starts);
    }
    for (i = 0; i < ctx.nlate; i++) {
	/* at(...) : comme la couche Tcl, dans l'ordre, des zéros jusqu'à la
	 * position si la sortie est plus courte, puis les octets par-dessus */
	BinLate *l = &ctx.late[i];

	if (rc == TCL_OK) {
	    if (l->off > out.len) {
		BinBufPut(&out, NULL, l->off - out.len);
	    }
	    if (l->off + l->buf.len > out.len) {
		BinBufPut(&out, NULL, l->off + l->buf.len - out.len);
	    }
	    if (l->buf.len > 0) {
		memcpy(out.p + l->off, l->buf.p, l->buf.len);
	    }
	}
	if (l->buf.p != NULL) {
	    Tcl_Free(l->buf.p);
	}
    }
    if (ctx.late != NULL) {
	Tcl_Free(ctx.late);
    }
    if (rc == TCL_OK) {
	Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(out.p, (Tcl_Size) out.len));
    }
    if (out.p != NULL) {
	Tcl_Free(out.p);
    }
    return rc;
}


/* --------------------------------------------------------------------
 * Décodage en C, même domaine que l'encodage : dispositions fixes.
 *
 * La valeur rendue a la forme de celle de la couche Tcl (vérifiée par
 * comparaison des représentations internes) :
 *   - scalaires : entiers et doubles nus ;
 *   - liste : liste nue de ses éléments ;
 *   - produit : liste de ses champs, ANNOTÉE de son type, sauf quand il est
 *     lui-même la valeur d'un champ (le constructeur retire l'annotation
 *     d'un champ, set {T}V champ $v ...).
 * Le décodage n'a d'effet qu'en cas de succès (-consumed compris).
 * -------------------------------------------------------------------- */

typedef struct {
    BinCtx b;
    Tcl_Interp *interp;
    const unsigned char *p;
    size_t len, pos, base;
    int strict;
    const TclType *refT[BIN_MEMO];
    Tcl_Obj *refO[BIN_MEMO];
    int nref;
    int linkdepth;		/* -1 : Inf (seul cas couvert) */
    size_t *starts;		/* débuts des structures englobantes */
    int nstarts, maxstarts;
    size_t chain[512];		/* cibles en cours (cycles ; liste chaînée :
				 * au-delà, la couche Tcl) */
    int nchain;
} BinDec;

static Tcl_Obj *
BinDecRef(BinDec *d, const TclType *tp)
{
    int k;

    for (k = 0; k < d->nref; k++) {
	if (d->refT[k] == tp) {
	    return d->refO[k];
	}
    }
    if (d->nref < BIN_MEMO) {
	d->refT[d->nref] = tp;
	d->refO[d->nref] = TclTypeRefFor(d->interp, (TclType *) tp, -1);
	return d->refO[d->nref++];
    }
    return TclTypeRefFor(d->interp, (TclType *) tp, -1);
}

static int
BinDecPad(BinDec *d, size_t al)
{
    size_t rel = d->pos - d->base, n, k;

    if (al <= 1 || rel % al == 0) {
	return TCL_OK;
    }
    n = al - rel % al;
    if (d->pos + n > d->len) {
	return TCL_CONTINUE;
    }
    if (d->strict) {
	for (k = 0; k < n; k++) {
	    if (d->p[d->pos + k] != 0) {
		return TCL_CONTINUE;	/* le message : couche Tcl */
	    }
	}
    }
    d->pos += n;
    return TCL_OK;
}

static int BinFastDec(BinDec *d, const TclType *tp, int asField,
	Tcl_Obj **vals, const TclType *encl, Tcl_Size nvals, int depth,
	Tcl_Obj **resPtr);

/* Lecture : la taille ou le nombre (préfixe lu, ou expression sur les
 * champs déjà lus). TCL_OK ou TCL_CONTINUE. */
static int
BinDynRead(BinDec *d, const TclBinDyn *dyn, const TclType *encl,
	Tcl_Obj **vals, Tcl_Size nvals, Tcl_WideInt *wantPtr)
{
    if (dyn->kind == TCLDYN_PREFIX) {
	Tcl_Obj *o = NULL;
	int rc = BinFastDec(d, dyn->pre, 1, NULL, NULL, 0, 1, &o);

	if (rc != TCL_OK) {
	    return rc;
	}
	Tcl_IncrRefCount(o);
	rc = (Tcl_GetWideIntFromObj(NULL, o, wantPtr) == TCL_OK
		&& *wantPtr >= 0) ? TCL_OK : TCL_CONTINUE;	/* u64 > 2^63 */
	Tcl_DecrRefCount(o);
	return rc;
    }
    return BinDynExpr(dyn, encl, vals, nvals, wantPtr);
}

/* Lit l'adresse d'un lien ; *nullPtr si c'est la valeur nulle, sinon
 * *targetPtr = position absolue de la cible. */
static int
BinLinkRead(BinDec *d, const TclType *tp, int *nullPtr, size_t *targetPtr,
	int depth)
{
    const TclBinLink *lk = tp->binLink;
    const char *nt = BinLinkNull(tp);
    size_t here = d->pos, origin;
    Tcl_Obj *a = NULL;
    Tcl_WideInt off, nv;
    int rc;

    if (d->linkdepth >= 0) {
	return TCL_CONTINUE;		/* -linkdepth N : couche Tcl */
    }
    rc = BinFastDec(d, lk->repr, 1, NULL, NULL, 0, depth + 1, &a);
    if (rc != TCL_OK) {
	return rc;
    }
    Tcl_IncrRefCount(a);
    rc = Tcl_GetWideIntFromObj(NULL, a, &off);
    Tcl_DecrRefCount(a);
    if (rc != TCL_OK) {
	return TCL_CONTINUE;
    }
    *nullPtr = 0;
    if (nt != NULL) {
	Tcl_Obj *n = Tcl_NewStringObj(nt, -1);

	Tcl_IncrRefCount(n);
	rc = Tcl_GetWideIntFromObj(NULL, n, &nv);
	Tcl_DecrRefCount(n);
	if (rc != TCL_OK) {
	    return TCL_CONTINUE;
	}
	if (nv == off) {
	    *nullPtr = 1;
	    return TCL_OK;
	}
    }
    if (!BinOriginPos(lk->origin, d->starts, d->nstarts, here, d->base,
	    &origin)) {
	return TCL_CONTINUE;
    }
    if ((off < 0 && (size_t) (-off) > origin - d->base)
	    || (off > 0 && (size_t) off > d->len - origin)) {
	return TCL_CONTINUE;		/* avant les données, ou au-delà */
    }
    *targetPtr = (size_t) ((Tcl_WideInt) origin + off);
    return TCL_OK;
}

/* La valeur d'un lien : nul (opt : none) ou cible lue ; le curseur revient
 * où il était. */
static int
BinLinkValue(BinDec *d, const TclType *tp, int isNull, size_t target,
	int asField, Tcl_Obj **vals, const TclType *encl, Tcl_Size nvals,
	int depth, Tcl_Obj **resPtr)
{
    int someIdx = -1, noneIdx = -1, rc, k;
    const TclType *some = BinLinkSome(tp, &someIdx, &noneIdx), *t;
    size_t save = d->pos, *ss = d->starts;
    int sns = d->nstarts, sms = d->maxstarts;
    Tcl_Obj *v = NULL;

    if (isNull) {
	if (some == NULL) {
	    return TCL_CONTINUE;	/* champ à défaut : couche Tcl */
	}
	/* comme field : le constructeur de la couche Tcl garde le contenu nu
	 * d'une somme opt ; ailleurs, la valeur annotée de mkSum */
	{
	    /* v16w : la valeur nulle est "none" (une somme balisée) */
	    /* v17z : la valeur nue, le littéral "none" */
	    Tcl_Obj *nv = TclSumTagged(tp) ? TclSumMake(tp, noneIdx, NULL)
		    : (tp->fields[noneIdx].literal != NULL)
		    ? Tcl_DuplicateObj(tp->fields[noneIdx].literal)
		    : Tcl_NewObj();

	    *resPtr = asField ? nv : TclNewAnnotatedOwn(nv,
		    TclTypeRefFor(d->interp, (TclType *) tp, noneIdx));
	}
	return TCL_OK;
    }
    for (k = 0; k < d->nchain; k++) {
	if (d->chain[k] == target) {
	    return TCL_CONTINUE;	/* cycle : le message, couche Tcl */
	}
    }
    if (d->nchain >= 512) {
	return TCL_CONTINUE;
    }
    d->chain[d->nchain++] = target;
    t = (some != NULL) ? some : tp->binLink->target;
    d->pos = target;
    d->starts = NULL;			/* starts {} pour la cible */
    d->nstarts = d->maxstarts = 0;
    /* la profondeur des types repart de 1 : une liste chaînée n'est bornée
     * que par la chaîne */
    (void) depth;
    rc = BinFastDec(d, t, (some != NULL) ? 1 : asField, vals, encl, nvals,
	    1, &v);
    if (d->starts != NULL) {
	Tcl_Free(d->starts);
    }
    d->starts = ss;
    d->nstarts = sns;
    d->maxstarts = sms;
    d->pos = save;
    d->nchain--;
    if (rc != TCL_OK) {
	return rc;
    }
    if (some != NULL && TclSumTagged(tp)) {
	v = TclSumMake(tp, someIdx, v);	/* v16w : "some charge" */
    }
    *resPtr = (some != NULL && !asField) ? TclNewAnnotatedOwn(v,
	    TclTypeRefFor(d->interp, (TclType *) tp, someIdx)) : v;
    return TCL_OK;
}

/* asField : la valeur est celle d'un champ (produit rendu sans annotation) */
static int
BinFastDec(BinDec *d, const TclType *tp, int asField, Tcl_Obj **vals,
	const TclType *encl, Tcl_Size nvals, int depth, Tcl_Obj **resPtr)
{
    Tcl_Size i, n;
    Tcl_Obj **elv, *lst;
    int rc;

    if (depth > 32) {
	return TCL_CONTINUE;
    }
    if (tp->binLink != NULL) {
	/* un lien hors d'un produit (élément de liste, cible) : suivi tout
	 * de suite */
	size_t target = 0;
	int isNull = 0;

	rc = BinLinkRead(d, tp, &isNull, &target, depth);
	if (rc != TCL_OK) {
	    return rc;
	}
	return BinLinkValue(d, tp, isNull, target, asField, vals, encl, nvals,
		depth, resPtr);
    }
    if (tp->binAt != NULL) {
	/* at(EXPR).T : lu à la position EXPR (depuis le début de la valeur
	 * lue) ; le curseur séquentiel ne bouge pas */
	Tcl_WideInt off;
	size_t save = d->pos;

	if (tp->binAtBody == NULL || BinExprValue(tp->binAt, encl, vals, nvals,
		&off) != TCL_OK || (Tcl_WideUInt) off > d->len - d->base) {
	    return TCL_CONTINUE;
	}
	d->pos = d->base + (size_t) off;
	rc = BinFastDec(d, tp->binAtBody, asField, vals, encl, nvals,
		depth + 1, resPtr);
	d->pos = save;
	return rc;
    }
    if (tp->kind == TCLT_SUM && tp->binTag != NULL) {
	/* somme balisée : la balise (champ déjà lu) choisit la variante ; la
	 * valeur garde son annotation (la variante), même comme champ */
	int var = BinTagVariant(tp, encl, vals, nvals);
	Tcl_Obj *payload;

	if (var < 0 || tp->fields[var].type == NULL) {
	    return TCL_CONTINUE;		/* le message : couche Tcl */
	}
	rc = BinFastDec(d, tp->fields[var].type, 1, NULL, NULL, 0, depth + 1,
		&payload);
	if (rc != TCL_OK) {
	    return rc;
	}
	*resPtr = TclNewAnnotatedOwn(payload,
		TclTypeRefFor(d->interp, (TclType *) tp, var));
	return TCL_OK;
    }
    {
	Tcl_Size ll = BinFastLit(tp);

	if (ll >= 0) {
	    /* un des littéraux, octet pour octet ; sinon la couche Tcl (et
	     * son message) */
	    Tcl_Obj *got;

	    if (d->pos + (size_t) ll > d->len) {
		return TCL_CONTINUE;
	    }
	    got = Tcl_NewByteArrayObj(d->p + d->pos, ll);
	    Tcl_IncrRefCount(got);
	    for (i = 0; i < tp->nfields; i++) {
		if (strcmp(Tcl_GetString(got),
			Tcl_GetString(tp->fields[i].literal)) == 0) {
		    break;
		}
	    }
	    if (i == tp->nfields) {
		Tcl_DecrRefCount(got);
		return TCL_CONTINUE;
	    }
	    d->pos += (size_t) ll;
	    *resPtr = Tcl_NewStringObj(Tcl_GetString(got), -1);
	    Tcl_DecrRefCount(got);
	    return TCL_OK;
	}
    }
    if (BinFastString(tp)) {
	const unsigned char *q = d->p + d->pos;
	size_t take, adv;

	if (tp->binKind == TCLBIN_CSTRING) {
	    size_t w = (size_t) BinNulWidth(tp->binEnc), k;

	    for (k = 0; d->pos + k + w <= d->len; k += w) {
		if (q[k] == 0 && (w == 1 || q[k + 1] == 0)) {
		    break;
		}
	    }
	    if (d->pos + k + w > d->len) {
		return TCL_CONTINUE;		/* pas de zéro final */
	    }
	    take = k;
	    adv = k + w;
	} else if (tp->binDyn != NULL) {
	    /* string(raw,$len), string(utf-8,int(u,16,be)) */
	    Tcl_WideInt want;

	    rc = BinDynRead(d, tp->binDyn, encl, vals, nvals, &want);
	    if (rc != TCL_OK) {
		return rc;
	    }
	    q = d->p + d->pos;		/* après le préfixe */
	    if ((Tcl_WideUInt) want > (Tcl_WideUInt) (d->len - d->pos)) {
		return TCL_CONTINUE;	/* données tronquées : couche Tcl */
	    }
	    take = adv = (size_t) want;
	} else {
	    take = adv = (size_t) tp->binSize;
	    if (d->pos + take > d->len) {
		return TCL_CONTINUE;
	    }
	}
	if (tp->binKind == TCLBIN_BYTES) {
	    *resPtr = Tcl_NewByteArrayObj(q, (Tcl_Size) take);
	} else {
	    Tcl_Encoding enc = Tcl_GetEncoding(NULL, tp->binEnc);
	    Tcl_DString ds;
	    Tcl_Size len;
	    const char *str;
	    int erc;

	    if (enc == NULL) {
		return TCL_CONTINUE;
	    }
	    Tcl_DStringInit(&ds);
	    erc = Tcl_ExternalToUtfDStringEx(NULL, enc, (const char *) q,
		    (Tcl_Size) take, TCL_ENCODING_PROFILE_STRICT, &ds, NULL);
	    Tcl_FreeEncoding(enc);
	    if (erc != TCL_OK) {
		Tcl_DStringFree(&ds);
		return TCL_CONTINUE;
	    }
	    str = Tcl_DStringValue(&ds);
	    len = Tcl_DStringLength(&ds);
	    if (tp->binKind == TCLBIN_STR && tp->binDyn == NULL) {
		/* taille fixe : les zéros de fin sont du remplissage
		 * (string trimright \x00 ; U+0000 s'écrit C0 80) */
		while (len >= 2 && (unsigned char) str[len - 2] == 0xC0
			&& (unsigned char) str[len - 1] == 0x80) {
		    len -= 2;
		}
		while (len >= 1 && str[len - 1] == '\0') {
		    len--;
		}
	    }
	    *resPtr = Tcl_NewStringObj(str, len);
	    Tcl_DStringFree(&ds);
	}
	d->pos += adv;
	return TCL_OK;
    }
    if (BinFastScalar(tp)) {
	size_t sz = (size_t) tp->binSize, k;
	const unsigned char *q = d->p + d->pos;
	int big = tp->binEnd == TCLBIN_BE
		|| (tp->binEnd == TCLBIN_AUTO && d->b.bigDefault);
	unsigned char buf[8];

	if (d->pos + sz > d->len) {
	    return TCL_CONTINUE;
	}
	d->pos += sz;
	if (tp->binKind == TCLBIN_FLOAT) {
	    for (k = 0; k < sz; k++) {
		buf[k] = q[(big != d->b.hostBig) ? sz - 1 - k : k];
	    }
	    if (sz == 4) {
		float f;

		memcpy(&f, buf, 4);
		*resPtr = Tcl_NewDoubleObj((double) f);
	    } else {
		double x;

		memcpy(&x, buf, 8);
		*resPtr = Tcl_NewDoubleObj(x);
	    }
	} else {
	    unsigned long long u = 0;

	    for (k = 0; k < sz; k++) {
		u |= (unsigned long long) q[big ? sz - 1 - k : k] << (8 * k);
	    }
	    if (tp->binKind == TCLBIN_SINT) {
		if (sz < 8 && (u >> (8 * sz - 1)) & 1) {
		    u |= ~0ULL << (8 * sz);	/* extension du signe */
		}
		*resPtr = Tcl_NewWideIntObj((Tcl_WideInt) u);
	    } else if (sz == 8) {
		*resPtr = Tcl_NewWideUIntObj((Tcl_WideUInt) u);
	    } else {
		*resPtr = Tcl_NewWideIntObj((Tcl_WideInt) u);
	    }
	}
	return TCL_OK;
    }
    if (tp->kind == TCLT_LIST) {
	Tcl_WideInt want;

	if (tp->binDyn != NULL) {
	    rc = BinDynRead(d, tp->binDyn, encl, vals, nvals, &want);
	    if (rc != TCL_OK) {
		return rc;
	    }
	} else {
	    want = tp->arity;
	}
	if (want < 0 || (size_t) want > d->len - d->pos) {
	    return TCL_CONTINUE;	/* taille absurde : couche Tcl */
	}
	n = (Tcl_Size) want;
	if (!d->strict && tp->elem != NULL) {
	    /* liste qui serait en cases, à la disposition binaire : les
	     * scalaires sont copiés dans les cases, sans objet par élément */
	    size_t off[BIN_FLAT_MAX], stride;
	    int size[BIN_FLAT_MAX], ns;
	    unsigned char *cells;

	    /* déjà aligné (champ précédé de son remplissage) ; après un
	     * préfixe qui le désaligne : élément par élément, comme la
	     * couche Tcl */
	    if ((d->pos - d->base) % BinAlignOf(&d->b, tp->elem, depth + 1) == 0
		    && (ns = BinCellsMatch(&d->b, tp->elem, &stride, off,
			size)) > 0
		    && (size_t) n * stride <= d->len - d->pos
		    && (lst = TclCompactNew((TclType *) tp, n, &cells))
			!= NULL) {
		BinCopyCells(cells, d->p + d->pos, n, stride, ns, off, size);
		d->pos += (size_t) n * stride;
		*resPtr = lst;
		return TCL_OK;
	    }
	}
	lst = Tcl_NewListObj(n, NULL);
	for (i = 0; i < n; i++) {
	    Tcl_Obj *e;

	    rc = BinFastDec(d, tp->elem, 0, NULL, NULL, 0, depth + 1, &e);
	    if (rc != TCL_OK) {
		TclDiscard(lst);
		return rc;
	    }
	    Tcl_ListObjAppendElement(NULL, lst, e);
	}
	*resPtr = lst;
	return TCL_OK;
    }
    /* produit */
    n = tp->nfields;
    elv = (Tcl_Obj **) Tcl_Alloc((n ? n : 1) * sizeof(Tcl_Obj *));
    {
	/* liens : l'adresse à sa place, la cible APRÈS tous les champs (ses
	 * tailles peuvent citer des champs qui suivent), comme la couche Tcl */
	size_t *tgt = (size_t *) Tcl_Alloc((n ? n : 1) * sizeof(size_t));
	int ns0 = d->nstarts, nlinks = 0;

	for (i = 0; i < n; i++) {
	    const TclType *ft = tp->fields[i].type;

	    rc = BinDecPad(d, BinAlignOf(&d->b, ft, depth + 1));
	    if (rc == TCL_OK && i == 0) {
		BinStartPush(&d->starts, &d->nstarts, &d->maxstarts, d->pos);
	    }
	    if (rc == TCL_OK && ft->binLink != NULL) {
		int isNull = 0;

		tgt[i] = 0;
		rc = BinLinkRead(d, ft, &isNull, &tgt[i], depth + 1);
		if (rc == TCL_OK && isNull) {
		    rc = BinLinkValue(d, ft, 1, 0, 1, NULL, NULL, 0, depth + 1,
			    &elv[i]);
		    tgt[i] = (size_t) -1;	/* déjà lu (nul) */
		} else if (rc == TCL_OK) {
		    elv[i] = Tcl_NewObj();	/* "" en attendant, comme fv$i */
		    nlinks++;
		}
	    } else if (rc == TCL_OK) {
		tgt[i] = (size_t) -1;
		rc = BinFastDec(d, ft, 1, elv, tp, i, depth + 1, &elv[i]);
	    }
	    if (rc != TCL_OK) {
		Tcl_Size j;

		for (j = 0; j < i; j++) {
		    Tcl_DecrRefCount(elv[j]);	/* référence prise ci-dessous */
		}
		Tcl_Free(elv);
		Tcl_Free(tgt);
		d->nstarts = ns0;
		return rc;
	    }
	    Tcl_IncrRefCount(elv[i]);
	}
	d->nstarts = ns0;
	for (i = 0; nlinks > 0 && i < n; i++) {
	    Tcl_Obj *lv = NULL;

	    if (tgt[i] == (size_t) -1) {
		continue;
	    }
	    rc = BinLinkValue(d, tp->fields[i].type, 0, tgt[i], 1, elv, tp, n,
		    depth + 1, &lv);
	    if (rc != TCL_OK) {
		for (i = 0; i < n; i++) {
		    Tcl_DecrRefCount(elv[i]);
		}
		Tcl_Free(elv);
		Tcl_Free(tgt);
		return rc;
	    }
	    Tcl_IncrRefCount(lv);
	    Tcl_DecrRefCount(elv[i]);
	    elv[i] = lv;
	}
	Tcl_Free(tgt);
    }
    rc = BinDecPad(d, BinAlignOf(&d->b, tp, depth));
    if (rc != TCL_OK) {
	for (i = 0; i < n; i++) {
	    Tcl_DecrRefCount(elv[i]);
	}
	Tcl_Free(elv);
	return rc;
    }
    lst = Tcl_NewListObj(n, elv);
    for (i = 0; i < n; i++) {
	Tcl_DecrRefCount(elv[i]);
    }
    Tcl_Free(elv);
    *resPtr = asField ? lst : TclNewAnnotatedOwn(lst, BinDecRef(d, tp));
    return TCL_OK;
}

/* type binary decode TYPE DATA ?-offset N? ?-endian E? ?-align A?
 * ?-strictpad B? ?-linkdepth D? ?-consumed VAR? */
static int
BinDecodeFast(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const char *flag = Tcl_GetVar2(interp, "tcl_typebinaryc", NULL,
	    TCL_GLOBAL_ONLY);
    TclType *tp;
    BinDec d;
    union { unsigned short s; unsigned char c[2]; } probe;
    Tcl_Size i, len;
    Tcl_WideInt off = 0;
    Tcl_Obj *consumed = NULL, *res = NULL;
    const char *text;
    unsigned char *bytes;
    int rc, linkdepth = -1;

    if ((flag != NULL && strcmp(flag, "0") == 0) || objc < 2 || objc % 2) {
	return TCL_CONTINUE;
    }
    memset(&d, 0, sizeof(d));
    d.b.cap = 1;
    linkdepth = -1;
    for (i = 2; i < objc; i += 2) {
	const char *o = Tcl_GetString(objv[i]), *a = Tcl_GetString(objv[i + 1]);
	Tcl_WideInt w;

	if (strcmp(o, "-endian") == 0 && strcmp(a, "little") == 0) {
	    d.b.bigDefault = 0;
	} else if (strcmp(o, "-endian") == 0 && strcmp(a, "big") == 0) {
	    d.b.bigDefault = 1;
	} else if (strcmp(o, "-align") == 0 && strcmp(a, "packed") == 0) {
	    d.b.cap = 1;
	} else if (strcmp(o, "-align") == 0 && strcmp(a, "natural") == 0) {
	    d.b.cap = 8;
	} else if (strcmp(o, "-align") == 0) {
	    if (Tcl_GetWideIntFromObj(NULL, objv[i + 1], &w) != TCL_OK
		    || w < 1 || (w & (w - 1)) != 0) {
		return TCL_CONTINUE;
	    }
	    d.b.cap = (size_t) w;
	} else if (strcmp(o, "-offset") == 0) {
	    if (Tcl_GetWideIntFromObj(NULL, objv[i + 1], &off) != TCL_OK
		    || off < 0) {
		return TCL_CONTINUE;
	    }
	} else if (strcmp(o, "-strictpad") == 0) {
	    if (Tcl_GetBooleanFromObj(NULL, objv[i + 1], &d.strict) != TCL_OK) {
		d.strict = 0;		/* string is true -strict : faux */
	    }
	} else if (strcmp(o, "-linkdepth") == 0) {
	    /* sans lien, la profondeur ne joue pas ; une valeur fausse donne
	     * son message par la couche Tcl ; une profondeur finie, avec un
	     * lien : la couche Tcl (BinLinkRead) */
	    if (strcmp(a, "Inf") != 0 && (Tcl_GetWideIntFromObj(NULL,
		    objv[i + 1], &w) != TCL_OK || w < 0)) {
		return TCL_CONTINUE;
	    }
	    linkdepth = (strcmp(a, "Inf") == 0) ? -1 : (int) (w > 1000000 ? 1000000 : w);
	} else if (strcmp(o, "-consumed") == 0) {
	    consumed = (a[0] != '\0') ? objv[i + 1] : NULL;
	} else {
	    return TCL_CONTINUE;
	}
    }
    if (TclIsAnnotated(objv[0])) {
	return TCL_CONTINUE;
    }
    text = Tcl_GetStringFromObj(objv[0], &i);
    {
	Tcl_Obj *saved = Tcl_GetObjResult(interp);

	Tcl_IncrRefCount(saved);
	tp = TclTypeParse(interp, text, i);
	Tcl_SetObjResult(interp, saved);
	Tcl_DecrRefCount(saved);
    }
    /* un résultat produit seulement : les autres passent par set {T}V */
    if (tp == NULL || tp->kind != TCLT_PRODUCT
	    || BinFastAlign(tp, d.b.cap, 0) == 0) {
	return TCL_CONTINUE;
    }
    {
	const TclType *seen[64];
	int nseen = 0;

	if (!BinLinksCovered(tp, d.b.cap, seen, &nseen, 0)) {
	    return TCL_CONTINUE;
	}
    }
    d.linkdepth = linkdepth;
    /*
     * Les octets seulement maintenant : convertir les données d'un type que
     * le codec C ne couvre pas les ferait changer de représentation à
     * chaque appel (tableau d'octets ici, chaîne dans la couche Tcl : tout
     * le fichier converti dans les deux sens).
     */
    bytes = Tcl_GetBytesFromObj(NULL, objv[1], &len);
    if (bytes == NULL || (Tcl_WideInt) len < off) {
	return TCL_CONTINUE;
    }
    probe.s = 1;
    d.b.hostBig = (probe.c[0] == 0);
    d.interp = interp;
    d.p = bytes;
    d.len = (size_t) len;
    d.pos = d.base = (size_t) off;
    rc = BinFastDec(&d, tp, 0, NULL, NULL, 0, 0, &res);
    if (d.starts != NULL) {
	Tcl_Free(d.starts);
    }
    if (rc != TCL_OK) {
	return rc;
    }
    if (consumed != NULL && Tcl_ObjSetVar2(interp, consumed, NULL,
	    Tcl_NewWideIntObj((Tcl_WideInt) d.pos), TCL_LEAVE_ERR_MSG) == NULL) {
	TclDiscard(res);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, res);
    return TCL_OK;
}

static int
TypeBinaryCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    TclType *tp;
    Tcl_Obj *r, *vis, *content = NULL, *raw = NULL;
    Tcl_Size len;
    const char *text;
    int kind;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "type");
	return TCL_ERROR;
    }

    /* Un verbe d'action (decode, encode, deref, space), sauf si c'est le nom
     * d'un type et qu'il n'y a pas d'autre argument. */
    if (!TclIsAnnotated(objv[2])) {
	int i;

	text = Tcl_GetStringFromObj(objv[2], &len);
	for (i = 0; binVerbs[i] != NULL; i++) {
	    if (strcmp(text, binVerbs[i]) == 0) {
		int isType = 0;

		if (objc == 3) {
		    Tcl_Obj *saved = Tcl_GetObjResult(interp);

		    Tcl_IncrRefCount(saved);
		    isType = (TclTypeParse(interp, text, len) != NULL);
		    Tcl_SetObjResult(interp, saved);
		    Tcl_DecrRefCount(saved);
		}
		if (!isType) {
		    if (strcmp(binVerbs[i], "encode") == 0) {
			int frc = BinEncodeFast(interp, objc - 3, objv + 3);

			if (frc != TCL_CONTINUE) {
			    return frc;
			}
		    }
		    if (strcmp(binVerbs[i], "decode") == 0) {
			int frc = BinDecodeFast(interp, objc - 3, objv + 3);

			if (frc != TCL_CONTINUE) {
			    return frc;
			}
		    }
		    return BinCallFb(interp, "::fb::filebinary", NULL,
			    objc - 3, objv + 3, binVerbs[i]);
		}
	    }
	}
    }

    /* Une interrogation : annotation locale sur l'argument de type. */
    kind = TclAnnotInfo(objv[2], &content, NULL, NULL, &raw);
    if (kind == 2 && raw != NULL) {
	const char *q = Tcl_GetString(raw);
	size_t ql = strlen(q);

	if (strcmp(q, "sizeof?") == 0) {
	    return BinCallFb(interp, "::fb::sizeof", content, objc - 3,
		    objv + 3, NULL);
	}
	if (strcmp(q, "is?") == 0) {
	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "{is?}type data ?options?");
		return TCL_ERROR;
	    }
	    return BinCallFb(interp, "::fb::isvalid", content, objc - 3,
		    objv + 3, NULL);
	}
	if (strcmp(q, "what?") != 0 && ql > 0 && q[ql - 1] == '?') {
	    ERR(interp, "BIN", "unknown question \"%s\": must be is?, sizeof? "
		    "or what?", q);
	    return TCL_ERROR;
	}
    }
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "type");
	return TCL_ERROR;
    }
    text = Tcl_GetStringFromObj(objv[2], &len);
    tp = TclTypeParse(interp, text, len);
    if (tp == NULL) {
	return TCL_ERROR;
    }
    vis = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(vis);
    if (BinRepr(interp, tp, 0, vis, &r) != TCL_OK) {
	Tcl_DecrRefCount(vis);
	return TCL_ERROR;
    }
    Tcl_DecrRefCount(vis);
    if (r == NULL) {
	ERR(interp, "BIN", "type \"%s\" has no binary representation (use "
		"int(u|s,n), double(n), string(enc,N|z), bitarray(n), a "
		"literal sum, list(N).T or a product of these)", tp->name);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, r);
    return TCL_OK;
}


/*
 * Annotations locales d'un mot (options -nom, questions nom?, cibles nom!),
 * de la plus récente à la plus ancienne : *words reçoit une liste neuve
 * (refcount 0), *subject le mot sans elles (non possédé : objv, ou un objet
 * gardé par la liste *keep). Rend le nombre d'annotations.
 */
/*
 * v16m : le texte d'une annotation locale peut porter plusieurs options,
 * dans la syntaxe des notes : « , » sépare, « . » descend, « = » donne une
 * valeur, (…) regroupe, "…" cite (substitutions backslash). Chaque option est
 * rendue à plat, son chemin écrit en entier :
 *	{-compact,-strict}	-> -compact  -strict
 *	{-a.(b=1,c="x,y")}	-> -a.b=1  -a.c="x,y"
 * Un texte qui ne suit pas cette syntaxe reste une seule option.
 */
static int
OptToken(const char *s, Tcl_Size *ip, Tcl_Size n)
{
    Tcl_Size i = *ip;

    if (i < n && s[i] == '"') {
	for (i++; i < n && s[i] != '"'; i++) {
	    if (s[i] == '\\' && i + 1 < n) {
		i++;
	    }
	}
	if (i >= n) {
	    return 0;
	}
	*ip = i + 1;
	return 1;
    }
    while (i < n && strchr(",.()=\"", s[i]) == NULL) {
	i++;
    }
    if (i == *ip) {
	return 0;
    }
    *ip = i;
    return 1;
}

static int OptSeq(const char *s, Tcl_Size *ip, Tcl_Size n, Tcl_DString *pre,
	Tcl_Obj *out);

static int
OptItem(const char *s, Tcl_Size *ip, Tcl_Size n, Tcl_DString *pre,
	Tcl_Obj *out)
{
    Tcl_DString path;
    Tcl_Size st = *ip;
    int ok = 1;

    Tcl_DStringInit(&path);
    Tcl_DStringAppend(&path, Tcl_DStringValue(pre), Tcl_DStringLength(pre));
    if (!OptToken(s, ip, n)) {
	Tcl_DStringFree(&path);
	return 0;
    }
    Tcl_DStringAppend(&path, s + st, *ip - st);
    while (*ip < n && s[*ip] == '.') {
	(*ip)++;
	if (*ip < n && s[*ip] == '(') {
	    (*ip)++;
	    Tcl_DStringAppend(&path, ".", 1);
	    ok = OptSeq(s, ip, n, &path, out);
	    if (ok && (*ip >= n || s[*ip] != ')')) {
		ok = 0;
	    }
	    (*ip)++;
	    Tcl_DStringFree(&path);
	    return ok;
	}
	st = *ip;
	if (!OptToken(s, ip, n)) {
	    Tcl_DStringFree(&path);
	    return 0;
	}
	Tcl_DStringAppend(&path, ".", 1);
	Tcl_DStringAppend(&path, s + st, *ip - st);
    }
    if (*ip < n && s[*ip] == '=') {
	(*ip)++;
	st = *ip;
	if (!OptToken(s, ip, n)) {
	    Tcl_DStringFree(&path);
	    return 0;
	}
	Tcl_DStringAppend(&path, "=", 1);
	Tcl_DStringAppend(&path, s + st, *ip - st);
    }
    Tcl_ListObjAppendElement(NULL, out, Tcl_NewStringObj(
	    Tcl_DStringValue(&path), Tcl_DStringLength(&path)));
    Tcl_DStringFree(&path);
    return 1;
}

static int
OptSeq(const char *s, Tcl_Size *ip, Tcl_Size n, Tcl_DString *pre,
	Tcl_Obj *out)
{
    while (1) {
	if (!OptItem(s, ip, n, pre, out)) {
	    return 0;
	}
	if (*ip >= n || s[*ip] != ',') {
	    return 1;
	}
	(*ip)++;
    }
}

static void
SplitOptions(const char *text, Tcl_Obj *out)
{
    Tcl_Obj *items = Tcl_NewListObj(0, NULL);
    Tcl_DString pre;
    Tcl_Size i = 0, n = (Tcl_Size) strlen(text);

    Tcl_IncrRefCount(items);
    Tcl_DStringInit(&pre);
    if (OptSeq(text, &i, n, &pre, items) && i == n) {
	Tcl_Obj **ev;
	Tcl_Size ne, k;

	Tcl_ListObjGetElements(NULL, items, &ne, &ev);
	for (k = 0; k < ne; k++) {
	    Tcl_ListObjAppendElement(NULL, out, ev[k]);
	}
    } else {
	Tcl_ListObjAppendElement(NULL, out, Tcl_NewStringObj(text, -1));
    }
    Tcl_DStringFree(&pre);
    Tcl_DecrRefCount(items);
}

static Tcl_Size
LocalWords(Tcl_Obj *word, Tcl_Obj **words, Tcl_Obj **subject, TclObjVec *keep)
{
    Tcl_Obj *cur = word, *top = NULL, *rest;
    Tcl_Size n = 0;

    *words = Tcl_NewListObj(0, NULL);
    while ((rest = TclAnnotPopTop(cur, &top)) != NULL) {
	const char *t = Tcl_GetString(top);

	if (strpbrk(t, "-?!") == NULL) {
	    TclDiscard(rest);
	    break;			/* une note ou un type : pas à nous */
	}
	SplitOptions(t, *words);		/* {-a,-b} (v16m) */
	TclVecAdd(keep, rest);
	cur = rest;
	n++;
    }
    *subject = cur;
    Tcl_ListObjLength(NULL, *words, &n);
    return n;
}

/*
 * v16m : deux types peuvent-ils accepter une même valeur ? Réponse prudente :
 * 1 dans le doute (string, any, var, profondeur dépassée). Avec EIAS, un
 * scalaire est une liste d'un élément, la liste vide est un dict vide, et une
 * liste extensible accepte la liste vide.
 */
static const TclType *
OverlapBase(const TclType *t)
{
    while (t != NULL && t->under != NULL
	    && (t->kind == TCLT_INT || t->kind == TCLT_DOUBLE
		|| t->kind == TCLT_STRING)) {
	t = t->under;
    }
    return t;
}

static int
SumLiteralFits(const TclType *t, Tcl_Obj *lit)
{
    return TclTypeValidate(NULL, t, lit, -1, NULL) == TCL_OK;
}

static int
MayOverlap(const TclType *a, const TclType *b, int depth)
{
    Tcl_Size i;

    a = OverlapBase(a);
    b = OverlapBase(b);
    if (depth > 8 || a == NULL || b == NULL || a == b
	    || TclTypeIsAny(a) || TclTypeIsAny(b)) {
	return 1;
    }
    if (a->kind != TCLT_SUM && b->kind == TCLT_SUM) {
	const TclType *x = a;

	a = b;
	b = x;
    }
    if (a->kind == TCLT_SUM) {
	for (i = 0; i < a->nfields; i++) {
	    const TclTypeField *f = &a->fields[i];

	    if (f->type == NULL) {
		if (f->literal != NULL && SumLiteralFits(b, f->literal)) {
		    return 1;
		}
	    } else if (MayOverlap(f->type, b, depth + 1)) {
		return 1;
	    }
	}
	return 0;
    }
    if (a->kind == TCLT_STRING || b->kind == TCLT_STRING
	    || a->kind == TCLT_VAR || b->kind == TCLT_VAR) {
	return 1;
    }
    /* Mettre le scalaire, puis le dict, en premier. */
    if ((b->kind == TCLT_INT || b->kind == TCLT_DOUBLE)
	    && a->kind != TCLT_INT && a->kind != TCLT_DOUBLE) {
	const TclType *x = a;

	a = b;
	b = x;
    }
    if (a->kind == TCLT_INT || a->kind == TCLT_DOUBLE) {
	switch (b->kind) {
	case TCLT_INT:
	case TCLT_DOUBLE:
	    return 1;
	case TCLT_LIST:
	    return (b->arity < 0 || b->arity == 1)
		    && MayOverlap(a, b->elem, depth + 1);
	case TCLT_PRODUCT:
	    return b->nfields == 1
		    && MayOverlap(a, b->fields[0].type, depth + 1);
	default:
	    return 0;			/* un dict a un nombre pair de mots */
	}
    }
    if (b->kind == TCLT_DICT && a->kind != TCLT_DICT) {
	const TclType *x = a;

	a = b;
	b = x;
    }
    if (a->kind == TCLT_DICT) {
	switch (b->kind) {
	case TCLT_DICT:
	    return 1;			/* le dict vide, au moins */
	case TCLT_LIST:
	    return b->arity < 0 || b->arity % 2 == 0;
	case TCLT_PRODUCT:
	    return b->nfields % 2 == 0;
	default:
	    return 1;
	}
    }
    if (a->kind == TCLT_PRODUCT && b->kind == TCLT_LIST) {
	const TclType *x = a;

	a = b;
	b = x;
    }
    if (a->kind == TCLT_LIST && b->kind == TCLT_LIST) {
	if (a->arity < 0 || b->arity < 0) {
	    if (a->arity <= 0 || b->arity <= 0) {
		return 1;		/* la liste vide */
	    }
	} else if (a->arity != b->arity) {
	    return 0;
	} else if (a->arity == 0) {
	    return 1;
	}
	return MayOverlap(a->elem, b->elem, depth + 1);
    }
    if (a->kind == TCLT_LIST && b->kind == TCLT_PRODUCT) {
	if (a->arity >= 0 && a->arity != b->nfields) {
	    return 0;
	}
	if (b->nfields == 0) {
	    return 1;
	}
	for (i = 0; i < b->nfields; i++) {
	    if (!MayOverlap(a->elem, b->fields[i].type, depth + 1)) {
		return 0;
	    }
	}
	return 1;
    }
    if (a->kind == TCLT_PRODUCT && b->kind == TCLT_PRODUCT) {
	if (a->nfields != b->nfields) {
	    return 0;
	}
	for (i = 0; i < a->nfields; i++) {
	    if (!MayOverlap(a->fields[i].type, b->fields[i].type, depth + 1)) {
		return 0;
	    }
	}
	return 1;
    }
    return 1;
}

/*
 * v17b : deux types dont les FORMES CANONIQUES ne se rencontrent jamais, même
 * si l'un accepte des écritures de l'autre : un entier canonique n'a que des
 * chiffres (16), un double canonique a toujours « . », « e » ou est Inf (2.0,
 * 1e+23) — Tcl_PrintDouble l'assure, et la normalisation à la construction
 * (v17a) fait que toute valeur rangée est canonique. Une union {int}i |
 * {double}d est donc disjointe sur ses valeurs ; une écriture non canonique
 * (0x10, que les deux acceptent) va à la plus stricte (int), puis prend sa
 * forme canonique (16).
 */
static int
CanonDisjoint(const TclType *a, const TclType *b)
{
    if (a == NULL || b == NULL) {
	return 0;
    }
    if (a->metaMask != 0 && b->metaMask != 0) {
	return (a->metaMask & b->metaMask) == 0;	/* v17e : méta-types */
    }
    if ((a->kind == TCLT_INT && a->bitWidth == 0 && b->kind == TCLT_DOUBLE)
	    || (b->kind == TCLT_INT && b->bitWidth == 0
		    && a->kind == TCLT_DOUBLE)) {
	return 1;
    }
    return 0;
}

static Tcl_Obj *SumOverlapsEx(const TclType *tp, int canonical);

/* Les paires de variantes de la somme tp qui peuvent accepter une même
 * valeur : {{a b} {a c} ...}, vide si la somme est disjointe. Pour une union
 * (v17b), jugé sur les formes canoniques. */
Tcl_Obj *
TclTypeSumOverlaps(const TclType *tp)
{
    return SumOverlapsEx(tp, tp->sumMode == TCLT_SUM_UNION);
}

static Tcl_Obj *
SumOverlapsEx(const TclType *tp, int canonical)
{
    Tcl_Obj *res = Tcl_NewListObj(0, NULL);
    Tcl_Size i, j;

    for (i = 0; i < tp->nfields; i++) {
	for (j = i + 1; j < tp->nfields; j++) {
	    const TclTypeField *a = &tp->fields[i], *b = &tp->fields[j];
	    int ov;

	    if (a->type == NULL && b->type == NULL) {
		ov = a->literal != NULL && b->literal != NULL
			&& strcmp(Tcl_GetString(a->literal),
				Tcl_GetString(b->literal)) == 0;
	    } else if (a->type == NULL) {
		ov = a->literal != NULL && SumLiteralFits(b->type, a->literal);
	    } else if (b->type == NULL) {
		ov = b->literal != NULL && SumLiteralFits(a->type, b->literal);
	    } else if (canonical && CanonDisjoint(a->type, b->type)) {
		ov = 0;
	    } else {
		ov = MayOverlap(a->type, b->type, 0);
	    }
	    if (ov) {
		Tcl_Obj *pair[2];

		/* une variante sans nom : son indice */
		pair[0] = a->name ? a->name : a->literal ? a->literal
			: Tcl_NewWideIntObj(i);
		pair[1] = b->name ? b->name : b->literal ? b->literal
			: Tcl_NewWideIntObj(j);
		Tcl_ListObjAppendElement(NULL, res, Tcl_NewListObj(2, pair));
	    }
	}
    }
    return res;
}

/*
 * v16o : la définition d'un type, dans la syntaxe de type def (ce que
 * « type def NOM » rend). Un type nommé s'écrit par son nom, sauf au premier
 * niveau ; un produit ou une somme anonymes s'écrivent en entier.
 */
static void DefText(const TclType *tp, int top, Tcl_DString *ds);
static Tcl_Obj *TypeTextObj(const TclType *tp, int top);
static int TypeCanonicCmd(Tcl_Interp *interp, Tcl_Size objc,
			    Tcl_Obj *const objv[]);	/* v17c */
static int TypeMetaCmd(Tcl_Interp *interp, Tcl_Size objc,
			    Tcl_Obj *const objv[]);	/* v17e */
static int TypeParseCmd(Tcl_Interp *interp, Tcl_Size objc,
			    Tcl_Obj *const objv[]);	/* v17c */
static int TypeCallCmd(Tcl_Interp *interp, Tcl_Size objc,
			    Tcl_Obj *const objv[]);
static Tcl_Obj *SigRecord(Tcl_Interp *interp, Proc *procPtr, int call,
			    Tcl_Obj *pairs);
static int TypeProcCmd(Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[]);		/* v16p */
static int CastWordUnfold(Tcl_Interp *interp, Tcl_Obj *word);	/* v16r */

static void
DefWord(const char *s, Tcl_DString *ds)
{
    const char *p;
    int plain = (*s != '\0');

    for (p = s; *p; p++) {
	if (!(isalnum(UCHAR(*p)) || *p == '_' || *p == '-' || *p == '.'
		|| (*p & 0x80))) {
	    plain = 0;
	    break;
	}
    }
    if (plain) {
	Tcl_DStringAppend(ds, s, -1);
	return;
    }
    Tcl_DStringAppend(ds, "\"", 1);
    for (p = s; *p; p++) {
	if (*p == '"' || *p == '\\' || *p == '$' || *p == '[') {
	    Tcl_DStringAppend(ds, "\\", 1);
	}
	Tcl_DStringAppend(ds, p, 1);
    }
    Tcl_DStringAppend(ds, "\"", 1);
}

static void
DefDefault(const TclTypeField *f, Tcl_DString *ds)
{
    if (f->defval != NULL) {
	Tcl_DStringAppend(ds, " ?= ", 4);
	DefWord(Tcl_GetString(f->defval), ds);
    } else if (f->defcmd != NULL) {
	Tcl_DStringAppend(ds, " ?= [", 5);
	Tcl_DStringAppend(ds, Tcl_GetString(f->defcmd), -1);
	Tcl_DStringAppend(ds, "]", 1);
    }
}

static void
DefField(const TclTypeField *f, Tcl_DString *ds)
{
    if (f->type == NULL) {
	Tcl_DStringAppend(ds, "\"", 1);
	if (f->literal != NULL) {
	    Tcl_DStringAppend(ds, Tcl_GetString(f->literal), -1);
	}
	Tcl_DStringAppend(ds, "\"", 1);
	return;
    }
    Tcl_DStringAppend(ds, "{", 1);
    DefText(f->type, 2, ds);		/* les accolades du champ suffisent */
    Tcl_DStringAppend(ds, "}", 1);
    if (f->name != NULL) {
	Tcl_DStringAppend(ds, Tcl_GetString(f->name), -1);
    }
    DefDefault(f, ds);
}

static void
DefText(const TclType *tp, int top, Tcl_DString *ds)
{
    Tcl_Size i;

    if (tp == NULL) {
	Tcl_DStringAppend(ds, "string", 6);
	return;
    }
    if (tp->metaMask != 0) {		/* any, bare, typed... (v17e) */
	Tcl_DStringAppend(ds, tp->name, TCL_AUTO_LENGTH);
	return;
    }
    if (tp->variantOf != NULL && !(top == 1 && tp->nominal)) {
	/* v18b : un type de variante s'écrit S/v (type def S/v : sa charge,
	 * voir TypeDefCmd) */
	Tcl_DStringAppend(ds, tp->name, -1);
	return;
    }
    if (tp->variantOf != NULL) {
	/* v18e : type def chaud temp/celsius : sa définition, temp/celsius
	 * (et la chaîne d'une charge restreinte) */
	const TclType *pl = tp->fields[tp->variantIdx].type;

	Tcl_DStringAppend(ds, tp->variantOf->name, -1);
	Tcl_DStringAppend(ds, "/", 1);
	TclTypeAppendMemberName(ds, MemberName(tp, tp->variantIdx));
	if (pl != NULL && pl->variantOf != NULL) {
	    Tcl_DStringAppend(ds, pl->name + strlen(pl->variantOf->name), -1);
	}
	return;
    }
    if (top != 1 && (tp->nominal || tp->reserved)) {
	Tcl_DStringAppend(ds, tp->name, -1);
	return;
    }
    switch (tp->kind) {
    case TCLT_INT:
    case TCLT_DOUBLE:
    case TCLT_STRING:
	if (tp->nominal && tp->under != NULL) {
	    DefText(tp->under, 0, ds);	/* type def mm double */
	} else {
	    Tcl_DStringAppend(ds, tp->name, -1);
	}
	return;
    case TCLT_VAR:
	Tcl_DStringAppend(ds, tp->varTyped ? "var(typed)" : "var", -1);
	if (tp->elem != NULL) {
	    Tcl_DStringAppend(ds, ".", 1);
	    DefText(tp->elem, 0, ds);
	}
	return;
    case TCLT_LIST:
	if (tp->binDyn != NULL) {
	    Tcl_DStringAppend(ds, tp->name, -1);
	    return;
	}
	Tcl_DStringAppend(ds, "list", 4);
	if (tp->arity >= 0) {
	    char buf[32];

	    snprintf(buf, sizeof(buf), "(%" TCL_SIZE_MODIFIER "d)",
		    tp->arity);
	    Tcl_DStringAppend(ds, buf, -1);
	} else if (tp->lenRange) {	/* v17g */
	    char buf[64];

	    if (tp->lenMax < 0) {
		snprintf(buf, sizeof(buf), "(%" TCL_SIZE_MODIFIER "d:Inf)",
			tp->lenMin);
	    } else {
		snprintf(buf, sizeof(buf), "(%" TCL_SIZE_MODIFIER "d:%"
			TCL_SIZE_MODIFIER "d)", tp->lenMin, tp->lenMax);
	    }
	    Tcl_DStringAppend(ds, buf, -1);
	}
	if (tp->elem != NULL) {
	    Tcl_DStringAppend(ds, ".", 1);
	    DefText(tp->elem, 0, ds);
	}
	return;
    case TCLT_DICT:
	Tcl_DStringAppend(ds, "dict", 4);
	if (tp->keyed) {
	    Tcl_DStringAppend(ds, "(", 1);
	    for (i = 0; i < tp->nkeys; i++) {
		if (i > 0) {
		    Tcl_DStringAppend(ds, ", ", 2);
		}
		Tcl_DStringAppend(ds, Tcl_GetString(tp->keys[i].name), -1);
		if (tp->keys[i].optional) {
		    Tcl_DStringAppend(ds, "?", 1);
		}
		if (tp->keys[i].defval != NULL) {
		    Tcl_DStringAppend(ds, "=", 1);
		    DefWord(Tcl_GetString(tp->keys[i].defval), ds);
		}
	    }
	    Tcl_DStringAppend(ds, ")", 1);
	}
	if (tp->elem != NULL) {
	    Tcl_DStringAppend(ds, ".", 1);
	    DefText(tp->elem, 0, ds);
	}
	return;
    case TCLT_PRODUCT:
    case TCLT_SUM:
	if (!top) {
	    /* anonyme hors d'un champ : entre accolades */
	    Tcl_DStringAppend(ds, "{", 1);
	}
	for (i = 0; i < tp->nfields; i++) {
	    if (i > 0) {
		Tcl_DStringAppend(ds, tp->kind == TCLT_SUM ? ", " : "; ", 2);
	    }
	    DefField(&tp->fields[i], ds);
	}
	if (!top) {
	    Tcl_DStringAppend(ds, "}", 1);
	}
	return;
    }
    Tcl_DStringAppend(ds, tp->name, -1);
}

/* type def NOM : la définition (NOM peut être S/v, une variante). */
static int
TypeDefShow(Tcl_Interp *interp, Tcl_Obj *nameObj)
{
    Tcl_Size len;
    const char *text = Tcl_GetStringFromObj(nameObj, &len);
    TclType *tp = NULL;
    Tcl_DString ds;

    tp = TclTypeParse(interp, text, len);
    if (tp == NULL) {
	return TCL_ERROR;
    }
    Tcl_DStringInit(&ds);
    if (tp->variantOf != NULL && !tp->nominal) {
	/* S/v : sa charge (P/f est déjà le type du champ) */
	if (tp->fields[tp->variantIdx].type == NULL) {
	    DefField(&tp->fields[tp->variantIdx], &ds);
	} else {
	    DefText(tp->fields[tp->variantIdx].type, 1, &ds);
	}
	Tcl_DStringResult(interp, &ds);
	return TCL_OK;
    }
    DefText(tp, 1, &ds);
    Tcl_DStringResult(interp, &ds);
    return TCL_OK;
}

/* v16o : dans le message d'erreur de l'interpréteur, le nom canonique d'un
 * type anonyme (prod<cible:string;…>) remplacé par un nom lisible (S.v). */
void
TclTypeResultRename(Tcl_Interp *interp, const char *from, const char *to)
{
    Tcl_Obj *old, *r;
    const char *src, *p;
    Tcl_Size fl;

    if (interp == NULL || from == NULL || from[0] == '\0') {
	return;
    }
    old = Tcl_GetObjResult(interp);
    src = Tcl_GetString(old);
    if (strstr(src, from) == NULL) {
	return;
    }
    Tcl_IncrRefCount(old);
    fl = (Tcl_Size) strlen(from);
    r = Tcl_NewObj();
    while ((p = strstr(src, from)) != NULL) {
	Tcl_AppendToObj(r, src, p - src);
	Tcl_AppendToObj(r, to, -1);
	src = p + fl;
    }
    Tcl_AppendToObj(r, src, -1);
    Tcl_SetObjResult(interp, r);
    Tcl_DecrRefCount(old);
}

/* type is {question?}SUJET : une question sur un type ou une valeur. */
static int
TypeIsCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *words, *subject, *q;
    TclObjVec keep = {NULL, 0, 0};
    Tcl_Size n;
    int rc = TCL_ERROR;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "{question?}type|value");
	return TCL_ERROR;
    }
    n = LocalWords(objv[2], &words, &subject, &keep);
    Tcl_IncrRefCount(words);
    if (n != 1) {
	ERR(interp, "IS", "type is takes one question: type is {compact?}T "
		"or type is {compact?}$value");
	goto done;
    }
    Tcl_ListObjIndex(NULL, words, 0, &q);
    if (strcmp(Tcl_GetString(q), "compact?") == 0) {
	Tcl_Obj *content = NULL;
	TclType *tp = NULL;

	/*
	 * Une annotation locale sur une valeur ne garde que son contenu
	 * ({compact?}$P : le contenu V de P). Un contenu en cases : la valeur
	 * est compacte. Sinon, un nom de type : le type le demande-t-il
	 * (type def {-compact}T) ? Sinon une valeur ordinaire : 0.
	 */
	if (TclAnnotInfo(subject, &content, &tp, NULL, NULL) == 1) {
	    subject = content;
	}
	if (TclIsCompact(subject)) {
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
	} else {
	    Tcl_Size tl;
	    const char *tt = Tcl_GetStringFromObj(subject, &tl);
	    Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);

	    tp = (tl > 0) ? TclTypeParse(interp, tt, tl) : NULL;
	    Tcl_RestoreInterpState(interp, st);
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(tp != NULL
		    && tp->compactWanted));
	}
	rc = TCL_OK;
    } else if (strcmp(Tcl_GetString(q), "sum?") == 0
	    || strcmp(Tcl_GetString(q), "enum?") == 0) {
	/* v17z : type is {sum?}X, {enum?}X : X un type, ou une valeur
	 * annotée (son type) ; une énumération n'a que des littéraux */
	TclType *tp = NULL;
	Tcl_Size i;
	int yes, st = 0, vv = -1;
	Tcl_Obj *cc = NULL;

	if (TclIsAnnotated(subject) && (TclAnnotResolve(NULL, subject, NULL,
		&st, &tp, &vv, &cc) != TCL_OK || st != TCLA_TYPED)) {
	    tp = NULL;
	}
	if (tp == NULL) {
	    Tcl_Size tl;
	    const char *tt = Tcl_GetStringFromObj(subject, &tl);

	    tp = ParseTypeOrVariant(interp, tt, tl);
	    if (tp == NULL) {
		goto done;
	    }
	}
	while (tp->kind != TCLT_SUM && tp->under != NULL) {
	    tp = tp->under;
	}
	yes = (tp->kind == TCLT_SUM);
	if (yes && Tcl_GetString(q)[0] == 'e') {
	    for (i = 0; i < tp->nfields && yes; i++) {
		yes = (tp->fields[i].type == NULL);
	    }
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(yes));
	rc = TCL_OK;
    } else if (strcmp(Tcl_GetString(q), "union?") == 0) {
	/* v16w : type is {union?}S : 1 si S est une union ("|"). */
	Tcl_Size tl;
	const char *tt = Tcl_GetStringFromObj(subject, &tl);
	TclType *tp = (tl > 0) ? TclTypeParse(interp, tt, tl) : NULL;

	if (tp == NULL) {
	    goto done;
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(tp->kind == TCLT_SUM
		&& tp->sumMode == TCLT_SUM_UNION));
	rc = TCL_OK;
    } else if (strcmp(Tcl_GetString(q), "disjoint?") == 0) {
	/* v16m : type is {disjoint?}S : les paires de variantes de la somme
	 * S qui peuvent accepter une même valeur (vide : disjointe). */
	Tcl_Size tl;
	const char *tt = Tcl_GetStringFromObj(subject, &tl);
	TclType *tp = (tl > 0) ? TclTypeParse(interp, tt, tl) : NULL;

	if (tp == NULL) {
	    goto done;
	}
	if (tp->kind != TCLT_SUM) {
	    ERR(interp, "IS", "type is {disjoint?}: %s is not a sum", tp->name);
	    goto done;
	}
	Tcl_SetObjResult(interp, TclTypeSumOverlaps(tp));
	rc = TCL_OK;
    } else {
	ERR(interp, "IS", "unknown question \"%s\": must be compact?, "
		"disjoint?, enum?, sum? or union?", Tcl_GetString(q));
    }
  done:
    Tcl_DecrRefCount(words);
    TclVecFree(&keep);
    return rc;
}


/*
 * type forget T ?T ...? : l'interpréteur oublie la définition de ces types.
 * Le nom est libre ensuite (un nouveau type def est permis). Les types ne
 * sont jamais libérés (d->all) : les valeurs et les variables déjà typées
 * gardent l'ancienne définition, un type distinct du nouveau de même nom.
 * Refus : un type intégré, un nom inconnu, un type dont se sert un autre
 * type nommé qui n'est pas oublié dans le même appel.
 */

/* Le nom k mentionne-t-il le type "name" (borné à gauche et à droite) ? */
static int
ForgetMentions(const char *k, const char *name)
{
    Tcl_Size len = (Tcl_Size) strlen(name);
    const char *p = k;

    while ((p = strstr(p, name)) != NULL) {
	if ((p == k || !TclTypeIsNameChar((unsigned char) p[-1]))
		&& !TclTypeIsNameChar((unsigned char) p[len])) {
	    return 1;
	}
	p++;
    }
    return 0;
}

/* tp atteint-il un des types de "doomed" (sans passer par un nom) ? */
static int
ForgetReaches(TclType *tp, TclType **doomed, Tcl_Size nd)
{
    Tcl_HashTable seen;
    TclType **stack = NULL;
    Tcl_Size top = 0, cap = 0, i;
    int found = 0, isNew;

#define FORGET_PUSH(t) do {						\
	TclType *t_ = (t);						\
	if (t_ != NULL) {						\
	    Tcl_CreateHashEntry(&seen, (char *) t_, &isNew);		\
	    if (isNew) {						\
		if (top == cap) {					\
		    cap = cap ? 2 * cap : 16;				\
		    stack = (TclType **) Tcl_Realloc(stack,		\
			    cap * sizeof(TclType *));			\
		}							\
		stack[top++] = t_;					\
	    }								\
	}								\
    } while (0)

    Tcl_InitHashTable(&seen, TCL_ONE_WORD_KEYS);
    FORGET_PUSH(tp);
    while (top > 0 && !found) {
	TclType *t = stack[--top];

	for (i = 0; i < nd; i++) {
	    if (t == doomed[i] && t != tp) {
		found = 1;
	    }
	}
	FORGET_PUSH(t->under);
	FORGET_PUSH(t->key);
	FORGET_PUSH(t->elem);
	FORGET_PUSH(t->binAtBody);
	FORGET_PUSH(t->binBase);
	if (t->binLink != NULL) {
	    FORGET_PUSH(t->binLink->repr);
	    FORGET_PUSH(t->binLink->target);
	}
	if (t->binDyn != NULL) {
	    FORGET_PUSH(t->binDyn->pre);
	}
	for (i = 0; i < t->nfields; i++) {
	    FORGET_PUSH(t->fields[i].type);
	}
    }
#undef FORGET_PUSH
    Tcl_Free(stack);
    Tcl_DeleteHashTable(&seen);
    return found;
}

/* Supprime de la table "tab" (clés chaîne) les entrées dont la clé mentionne
 * un des noms : un segment entre \1 (instances, conversions), ou un nom
 * n'importe où dans la clé (canon). ownsObj : la valeur est un Tcl_Obj compté. */
enum { FORGET_ANY, FORGET_SEGMENTS };

static void
ForgetKeys(Tcl_HashTable *tab, Tcl_Obj *const names[], Tcl_Size n, int how,
	int ownsObj)
{
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    char **doomed = NULL;
    Tcl_Size nk = 0, capk = 0, i, j;

    for (hPtr = Tcl_FirstHashEntry(tab, &search); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&search)) {
	const char *k = (const char *) Tcl_GetHashKey(tab, hPtr);
	int hit = 0;

	for (j = 0; j < n && !hit; j++) {
	    const char *nm = Tcl_GetString(names[j]);

	    if (how == FORGET_ANY) {
		hit = ForgetMentions(k, nm);
	    } else {
		/* segments séparés par \1 : l'un d'eux est le nom */
		const char *p = k;

		while (!hit) {
		    const char *e = strchr(p, '\1');
		    Tcl_Size l = e ? (Tcl_Size) (e - p) : (Tcl_Size) strlen(p);

		    hit = (l == (Tcl_Size) strlen(nm)
			    && strncmp(p, nm, (size_t) l) == 0);
		    if (e == NULL) {
			break;
		    }
		    p = e + 1;
		}
	    }
	}
	if (hit) {
	    if (nk == capk) {
		capk = capk ? 2 * capk : 8;
		doomed = (char **) Tcl_Realloc(doomed, capk * sizeof(char *));
	    }
	    doomed[nk++] = DupStr(k, (Tcl_Size) strlen(k));
	}
    }
    for (i = 0; i < nk; i++) {
	hPtr = Tcl_FindHashEntry(tab, doomed[i]);
	if (hPtr != NULL) {
	    if (ownsObj) {
		/* converts : la lambda est comptée */
		Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(hPtr));
	    }
	    Tcl_DeleteHashEntry(hPtr);
	}
	Tcl_Free(doomed[i]);
    }
    Tcl_Free(doomed);
}

static int
TypeForgetCmd(Tcl_Interp *interp, TypeData *d, Tcl_Size n,
	Tcl_Obj *const names[])
{
    TclType **doomed;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    Tcl_Size i, j;

    doomed = (TclType **) Tcl_Alloc((n > 0 ? n : 1) * sizeof(TclType *));
    for (i = 0; i < n; i++) {
	const char *nm = Tcl_GetString(names[i]);

	doomed[i] = NULL;
	hPtr = Tcl_FindHashEntry(&d->names, nm);
	if (hPtr != NULL) {
	    TclType *tp = (TclType *) Tcl_GetHashValue(hPtr);

	    if (tp == NULL || tp->reserved || TclTypeIsAny(tp)
		    || tp == d->tInt || tp == d->tDouble || tp == d->tString
		    || tp == d->tList || tp == d->tDict || tp == d->tVar) {
		ERR(interp, "NAME", "type \"%s\" is built in: it cannot be "
			"forgotten", nm);
		Tcl_Free(doomed);
		return TCL_ERROR;
	    }
	    doomed[i] = tp;
	} else if (Tcl_FindHashEntry(&d->generics, nm) == NULL) {
	    ERR(interp, "UNKNOWN", "type \"%s\" is not defined", nm);
	    Tcl_Free(doomed);
	    return TCL_ERROR;
	}
    }

    /*
     * Un autre type nommé, qui reste, se sert-il d'un type oublié ? Un alias
     * (même TclType sous un autre nom) n'est pas un usage : il garde le type.
     */
    for (hPtr = Tcl_FirstHashEntry(&d->names, &search); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&search)) {
	const char *k = (const char *) Tcl_GetHashKey(&d->names, hPtr);
	TclType *tp = (TclType *) Tcl_GetHashValue(hPtr);
	int listed = 0;

	if (tp == NULL || tp->reserved) {
	    continue;
	}
	for (j = 0; j < n; j++) {
	    if (strcmp(k, Tcl_GetString(names[j])) == 0 || tp == doomed[j]) {
		listed = 1;
	    }
	}
	if (!listed && ForgetReaches(tp, doomed, n)) {
	    for (j = 0; j < n; j++) {
		if (doomed[j] != NULL && ForgetReaches(tp, doomed + j, 1)) {
		    break;
		}
	    }
	    ERR(interp, "INUSE", "type \"%s\" is used by type \"%s\": forget "
		    "both (type forget %s %s)",
		    j < n ? Tcl_GetString(names[j]) : "?", k, k,
		    j < n ? Tcl_GetString(names[j]) : "?");
	    Tcl_Free(doomed);
	    return TCL_ERROR;
	}
    }

    for (i = 0; i < n; i++) {
	const char *nm = Tcl_GetString(names[i]);

	hPtr = Tcl_FindHashEntry(&d->names, nm);
	if (hPtr != NULL) {
	    Tcl_DeleteHashEntry(hPtr);
	}
	hPtr = Tcl_FindHashEntry(&d->generics, nm);
	if (hPtr != NULL) {
	    /*
	     * La famille reste utilisée par ses instances (->family) : elle
	     * est rangée sous une clé qui ne peut pas être un nom (\1), et
	     * libérée avec les autres par TclTypesFreeGenerics.
	     */
	    void *gen = Tcl_GetHashValue(hPtr);
	    char key[64];
	    int isNew;

	    snprintf(key, sizeof(key), "\1forgotten\1%p", gen);
	    Tcl_DeleteHashEntry(hPtr);
	    Tcl_SetHashValue(Tcl_CreateHashEntry(&d->generics, key, &isNew),
		    gen);
	}
    }
    ForgetKeys(&d->instances, names, n, FORGET_SEGMENTS, 0);
    ForgetKeys(&d->converts, names, n, FORGET_SEGMENTS, 1);
    ForgetKeys(&d->canon, names, n, FORGET_ANY, 0);
    Tcl_Free(doomed);
    TclTypesBump(d);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 * 23/11 : type map VARS LISTE ?VARS LISTE ...? {(EXPR)}
 *
 * Applique une expression à chaque élément (les listes avancent ensemble ;
 * VARS découpe l'élément comme lassign). L'expression, si elle n'est faite
 * que de nombres, de variables, des opérateurs arithmétiques et de
 * comparaison, de ?: et des fonctions mathématiques usuelles, est compilée
 * une fois en un petit programme sur des nombres et évaluée en C, sans
 * objet ni passage par l'interpréteur. Tout élément que ce programme ne sait
 * pas traiter exactement comme expr (débordement, division par zéro,
 * résultat infini ou NaN, valeur qui n'est pas un nombre simple) est
 * évalué par expr ; une expression hors de ce sous-ensemble l'est pour
 * tous les éléments (la boucle reste en C). Les variables gardent, après la
 * commande, les valeurs du dernier élément (comme lmap). Résultat : une
 * list.double ou une list.int si tous les résultats en sont, sinon une
 * liste ordinaire.
 * ------------------------------------------------------------------------
 */

typedef struct MNum {
    int isD;			/* 1 : d ; 0 : w ; < 0 : à lire ailleurs */
    union {
	Tcl_WideInt w;
	double d;
    };
} MNum;

/* type map : petits entiers partagés dans un résultat en objets */
#define MAP_SMALL_LO (-256)
#define MAP_SMALL_HI 1023

enum {
    MN_CONST, MN_VAR, MN_NEG, MN_POS, MN_NOT, MN_ADD, MN_SUB, MN_MUL, MN_DIV,
    MN_MOD, MN_POW, MN_LT, MN_LE, MN_GT, MN_GE, MN_EQ, MN_NE, MN_AND, MN_OR,
    MN_COND, MN_FUNC
};

enum {
    MF_ABS, MF_SQRT, MF_SIN, MF_COS, MF_TAN, MF_ASIN, MF_ACOS, MF_ATAN,
    MF_SINH, MF_COSH, MF_TANH, MF_EXP, MF_LOG, MF_LOG10, MF_FLOOR, MF_CEIL,
    MF_ROUND, MF_INT, MF_DOUBLE, MF_POWF, MF_ATAN2, MF_FMOD, MF_HYPOT,
    MF_MIN, MF_MAX
};

static const struct {
    const char *name;
    int fn, minArgs, maxArgs;
} mapFuncs[] = {
    {"abs", MF_ABS, 1, 1}, {"sqrt", MF_SQRT, 1, 1}, {"sin", MF_SIN, 1, 1},
    {"cos", MF_COS, 1, 1}, {"tan", MF_TAN, 1, 1}, {"asin", MF_ASIN, 1, 1},
    {"acos", MF_ACOS, 1, 1}, {"atan", MF_ATAN, 1, 1},
    {"sinh", MF_SINH, 1, 1}, {"cosh", MF_COSH, 1, 1},
    {"tanh", MF_TANH, 1, 1}, {"exp", MF_EXP, 1, 1}, {"log", MF_LOG, 1, 1},
    {"log10", MF_LOG10, 1, 1}, {"floor", MF_FLOOR, 1, 1},
    {"ceil", MF_CEIL, 1, 1}, {"round", MF_ROUND, 1, 1},
    {"int", MF_INT, 1, 1}, {"double", MF_DOUBLE, 1, 1},
    {"pow", MF_POWF, 2, 2}, {"atan2", MF_ATAN2, 2, 2},
    {"fmod", MF_FMOD, 2, 2}, {"hypot", MF_HYPOT, 2, 2},
    {"min", MF_MIN, 1, 8}, {"max", MF_MAX, 1, 8},
    {NULL, 0, 0, 0}
};

typedef struct MNode {
    int kind, a, b, c;		/* opérandes (rangs de noeuds) */
    int slot;			/* MN_VAR : rang de la variable */
    int fn, nargs, args[8];	/* MN_FUNC */
    MNum k;			/* MN_CONST */
} MNode;

typedef struct MProg {
    MNode *n;
    int nn, cap;
    Tcl_Obj **names;		/* les variables lues, dans l'ordre */
    int nnames, capNames;
    const char *p;		/* lecture */
    int bad;			/* hors du sous-ensemble */
    const char *badAt;		/* là où la lecture s'est arrêtée */
} MProg;

/* hors du sous-ensemble compilé : la première position est gardée (pour
 * type map -report) */
#define MBAD(g) \
    do {								\
	if (!(g)->bad) {						\
	    (g)->badAt = (g)->p;					\
	}								\
	(g)->bad = 1;							\
    } while (0)

static int
MNew(MProg *g, int kind)
{
    if (g->nn == g->cap) {
	g->cap = g->cap ? 2 * g->cap : 32;
	g->n = (MNode *) Tcl_Realloc(g->n, g->cap * sizeof(MNode));
    }
    memset(&g->n[g->nn], 0, sizeof(MNode));
    g->n[g->nn].kind = kind;
    return g->nn++;
}

static void
MSkip(MProg *g)
{
    while (*g->p == ' ' || *g->p == '\t' || *g->p == '\n' || *g->p == '\r') {
	g->p++;
    }
}

static int MExpr(MProg *g);

static int
MSlot(MProg *g, const char *s, Tcl_Size len)
{
    int i;

    for (i = 0; i < g->nnames; i++) {
	Tcl_Size l;
	const char *t = Tcl_GetStringFromObj(g->names[i], &l);

	if (l == len && memcmp(t, s, len) == 0) {
	    return i;
	}
    }
    if (g->nnames == g->capNames) {
	g->capNames = g->capNames ? 2 * g->capNames : 8;
	g->names = (Tcl_Obj **) Tcl_Realloc(g->names,
		g->capNames * sizeof(Tcl_Obj *));
    }
    g->names[g->nnames] = Tcl_NewStringObj(s, len);
    Tcl_IncrRefCount(g->names[g->nnames]);
    return g->nnames++;
}

static int
MIdent(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
	    || (c >= '0' && c <= '9') || c == '_';
}

static int
MPrimary(MProg *g)
{
    int r;

    MSkip(g);
    if (g->bad) {
	return -1;
    }
    if (*g->p == '(') {
	g->p++;
	r = MExpr(g);
	MSkip(g);
	if (*g->p != ')') {
	    MBAD(g);
	    return -1;
	}
	g->p++;
	return r;
    }
    if (*g->p == '$') {
	const char *s = ++g->p;

	while (MIdent(*g->p)) {
	    g->p++;
	}
	if (g->p == s || *g->p == '(' || *g->p == ':') {
	    g->p = s - 1;		/* (le $) */
	    MBAD(g);			/* ${x}, $a(i), $::x : expr */
	    return -1;
	}
	r = MNew(g, MN_VAR);
	g->n[r].slot = MSlot(g, s, g->p - s);
	return r;
    }
    if ((*g->p >= '0' && *g->p <= '9') || *g->p == '.') {
	const char *s = g->p;
	Tcl_Obj *t;
	void *ptr;
	int type;

	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X' || s[1] == 'b'
		|| s[1] == 'B' || s[1] == 'o' || s[1] == 'O')) {
	    g->p += 2;
	    while (MIdent(*g->p)) {
		g->p++;
	    }
	} else {
	    while (MIdent(*g->p) || *g->p == '.'
		    || ((*g->p == '+' || *g->p == '-')
		    && (g->p[-1] == 'e' || g->p[-1] == 'E'))) {
		g->p++;
	    }
	}
	t = Tcl_NewStringObj(s, g->p - s);
	r = -1;
	if (Tcl_GetNumberFromObj(NULL, t, &ptr, &type) == TCL_OK
		&& (type == TCL_NUMBER_INT || type == TCL_NUMBER_DOUBLE)) {
	    r = MNew(g, MN_CONST);
	    if (type == TCL_NUMBER_INT) {
		g->n[r].k.w = *(Tcl_WideInt *) ptr;
	    } else {
		g->n[r].k.isD = 1;
		g->n[r].k.d = *(double *) ptr;
	    }
	} else {
	    MBAD(g);
	}
	Tcl_DecrRefCount(t);
	return r;
    }
    if ((*g->p >= 'a' && *g->p <= 'z') || (*g->p >= 'A' && *g->p <= 'Z')) {
	const char *s = g->p;
	int k;

	while (MIdent(*g->p)) {
	    g->p++;
	}
	MSkip(g);
	if (*g->p != '(') {
	    g->p = s;
	    MBAD(g);			/* eq, ne, in, true... : expr */
	    return -1;
	}
	for (k = 0; mapFuncs[k].name; k++) {
	    if ((size_t) (g->p - s) >= strlen(mapFuncs[k].name)
		    && strncmp(s, mapFuncs[k].name, strlen(mapFuncs[k].name)) == 0
		    && !MIdent(s[strlen(mapFuncs[k].name)])) {
		break;
	    }
	}
	if (mapFuncs[k].name == NULL) {
	    g->p = s;			/* (le nom de la fonction) */
	    MBAD(g);
	    return -1;
	}
	r = MNew(g, MN_FUNC);
	g->n[r].fn = mapFuncs[k].fn;
	g->p++;				/* ( */
	MSkip(g);
	if (*g->p != ')') {
	    for (;;) {
		int a = MExpr(g);

		if (g->bad || g->n[r].nargs == 8) {
		    MBAD(g);
		    return -1;
		}
		g->n[r].args[g->n[r].nargs++] = a;
		MSkip(g);
		if (*g->p == ',') {
		    g->p++;
		    continue;
		}
		break;
	    }
	}
	MSkip(g);
	if (*g->p != ')' || g->n[r].nargs < mapFuncs[k].minArgs
		|| g->n[r].nargs > mapFuncs[k].maxArgs) {
	    MBAD(g);
	    return -1;
	}
	g->p++;
	return r;
    }
    MBAD(g);
    return -1;
}

/* unaires (plus forts que **, comme dans expr : -2**2 vaut 4) */
static int
MUnary(MProg *g)
{
    int r, kind;

    MSkip(g);
    if (*g->p == '-' || *g->p == '+' || (*g->p == '!' && g->p[1] != '=')) {
	kind = (*g->p == '-') ? MN_NEG : (*g->p == '+') ? MN_POS : MN_NOT;
	g->p++;
	r = MNew(g, kind);
	g->n[r].a = MUnary(g);
	return r;
    }
    return MPrimary(g);
}

static int
MPower(MProg *g)
{
    int a = MUnary(g), r;

    MSkip(g);
    if (g->p[0] == '*' && g->p[1] == '*') {
	g->p += 2;
	r = MNew(g, MN_POW);
	g->n[r].a = a;
	g->n[r].b = MPower(g);		/* à droite */
	return r;
    }
    return a;
}

static int
MMul(MProg *g)
{
    int a = MPower(g), r, kind;

    for (;;) {
	MSkip(g);
	if (g->p[0] == '*' && g->p[1] != '*') {
	    kind = MN_MUL;
	} else if (g->p[0] == '/') {
	    kind = MN_DIV;
	} else if (g->p[0] == '%') {
	    kind = MN_MOD;
	} else {
	    return a;
	}
	g->p++;
	r = MNew(g, kind);
	g->n[r].a = a;
	g->n[r].b = MPower(g);
	a = r;
    }
}

static int
MAdd(MProg *g)
{
    int a = MMul(g), r, kind;

    for (;;) {
	MSkip(g);
	if (g->p[0] == '+') {
	    kind = MN_ADD;
	} else if (g->p[0] == '-') {
	    kind = MN_SUB;
	} else {
	    return a;
	}
	g->p++;
	r = MNew(g, kind);
	g->n[r].a = a;
	g->n[r].b = MMul(g);
	a = r;
    }
}

static int
MRel(MProg *g)
{
    int a = MAdd(g), r, kind;

    for (;;) {
	MSkip(g);
	if (g->p[0] == '<' && g->p[1] == '<') {
	    MBAD(g);
	    return -1;
	} else if (g->p[0] == '>' && g->p[1] == '>') {
	    MBAD(g);
	    return -1;
	} else if (g->p[0] == '<' && g->p[1] == '=') {
	    kind = MN_LE; g->p += 2;
	} else if (g->p[0] == '>' && g->p[1] == '=') {
	    kind = MN_GE; g->p += 2;
	} else if (g->p[0] == '<') {
	    kind = MN_LT; g->p++;
	} else if (g->p[0] == '>') {
	    kind = MN_GT; g->p++;
	} else {
	    return a;
	}
	r = MNew(g, kind);
	g->n[r].a = a;
	g->n[r].b = MAdd(g);
	a = r;
    }
}

static int
MEqual(MProg *g)
{
    int a = MRel(g), r, kind;

    for (;;) {
	MSkip(g);
	if (g->p[0] == '=' && g->p[1] == '=') {
	    kind = MN_EQ;
	} else if (g->p[0] == '!' && g->p[1] == '=') {
	    kind = MN_NE;
	} else {
	    if (g->p[0] == '&' && g->p[1] != '&') {
		MBAD(g);		/* & | ^ : expr */
	    }
	    if ((g->p[0] == '|' && g->p[1] != '|') || g->p[0] == '^'
		    || g->p[0] == '~' || g->p[0] == '=') {
		MBAD(g);
	    }
	    return a;
	}
	g->p += 2;
	r = MNew(g, kind);
	g->n[r].a = a;
	g->n[r].b = MRel(g);
	a = r;
    }
}

static int
MAnd(MProg *g)
{
    int a = MEqual(g), r;

    for (;;) {
	MSkip(g);
	if (g->p[0] != '&' || g->p[1] != '&') {
	    return a;
	}
	g->p += 2;
	r = MNew(g, MN_AND);
	g->n[r].a = a;
	g->n[r].b = MEqual(g);
	a = r;
    }
}

static int
MOr(MProg *g)
{
    int a = MAnd(g), r;

    for (;;) {
	MSkip(g);
	if (g->p[0] != '|' || g->p[1] != '|') {
	    return a;
	}
	g->p += 2;
	r = MNew(g, MN_OR);
	g->n[r].a = a;
	g->n[r].b = MAnd(g);
	a = r;
    }
}

static int
MExpr(MProg *g)
{
    int c = MOr(g), r;

    MSkip(g);
    if (*g->p != '?') {
	return c;
    }
    g->p++;
    r = MNew(g, MN_COND);
    g->n[r].a = c;
    g->n[r].b = MExpr(g);
    MSkip(g);
    if (*g->p != ':') {
	MBAD(g);
	return -1;
    }
    g->p++;
    g->n[r].c = MExpr(g);
    return r;
}

/* Évaluation. 0 : l'élément passe par expr (débordement, division par zéro,
 * résultat non fini...). */

#define MD(x) ((x).isD ? (x).d : (double) (x).w)
#define M_EXACT ((Tcl_WideInt) 1 << 53)

static int
MTruth(MNum x)
{
    return x.isD ? (x.d != 0.0) : (x.w != 0);
}

static int
MFinite(double d)
{
    return !isnan(d) && !isinf(d);
}

/* a < b (-1), = (0), > (1) ; 2 : comparaison qui ne serait pas exacte */
static int
MCmp(MNum a, MNum b)
{
    if (!a.isD && !b.isD) {
	return (a.w < b.w) ? -1 : (a.w > b.w);
    }
    if ((!a.isD && (a.w > M_EXACT || a.w < -M_EXACT))
	    || (!b.isD && (b.w > M_EXACT || b.w < -M_EXACT))) {
	return 2;
    }
    {
	double x = MD(a), y = MD(b);

	if (isnan(x) || isnan(y)) {
	    return 2;
	}
	return (x < y) ? -1 : (x > y);
    }
}

static int MEval(const MProg *g, int i, const MNum *vars, MNum *out);

static int
MEvalFunc(const MProg *g, const MNode *nd, const MNum *vars, MNum *out)
{
    int c;
	MNum v[8];
	int k;
	double d, r;

	for (k = 0; k < nd->nargs; k++) {
	    if (!MEval(g, nd->args[k], vars, &v[k])) {
		return 0;
	    }
	}
	d = MD(v[0]);
	out->isD = 1;
	switch (nd->fn) {
	case MF_ABS:
	    if (!v[0].isD) {
		if (v[0].w == WIDE_MIN) {
		    return 0;
		}
		out->isD = 0;
		out->w = v[0].w < 0 ? -v[0].w : v[0].w;
		return 1;
	    }
	    r = fabs(d);
	    break;
	case MF_SQRT:
	    if (d < 0 || (!v[0].isD && v[0].w > M_EXACT)) {
		return 0;
	    }
	    r = sqrt(d);
	    break;
	case MF_SIN: r = sin(d); break;
	case MF_COS: r = cos(d); break;
	case MF_TAN: r = tan(d); break;
	case MF_ASIN: r = asin(d); break;
	case MF_ACOS: r = acos(d); break;
	case MF_ATAN: r = atan(d); break;
	case MF_SINH: r = sinh(d); break;
	case MF_COSH: r = cosh(d); break;
	case MF_TANH: r = tanh(d); break;
	case MF_EXP: r = exp(d); break;
	case MF_LOG:
	    if (d <= 0) {
		return 0;
	    }
	    r = log(d);
	    break;
	case MF_LOG10:
	    if (d <= 0) {
		return 0;
	    }
	    r = log10(d);
	    break;
	case MF_FLOOR:
	    if (!v[0].isD && (v[0].w > M_EXACT || v[0].w < -M_EXACT)) {
		return 0;
	    }
	    r = floor(d);
	    break;
	case MF_CEIL:
	    if (!v[0].isD && (v[0].w > M_EXACT || v[0].w < -M_EXACT)) {
		return 0;
	    }
	    r = ceil(d);
	    break;
	case MF_DOUBLE:
	    if (!v[0].isD && (v[0].w > M_EXACT || v[0].w < -M_EXACT)) {
		return 0;
	    }
	    r = d;
	    break;
	case MF_ROUND:
	case MF_INT:
	    if (!v[0].isD) {
		*out = v[0];
		return 1;
	    }
	    if (!MFinite(d) || d >= 9.2e18 || d <= -9.2e18) {
		return 0;
	    }
	    if (nd->fn == MF_INT) {
		out->w = (Tcl_WideInt) d;	/* troncature, comme int() */
	    } else {
		/* round() : le plus proche, les demis loin de zéro */
		double f = floor(d), frac = d - f;

		if (d < 0) {
		    double af = floor(-d), afrac = -d - af;

		    out->w = -(Tcl_WideInt) ((afrac >= 0.5) ? af + 1 : af);
		} else {
		    out->w = (Tcl_WideInt) ((frac >= 0.5) ? f + 1 : f);
		}
	    }
	    out->isD = 0;
	    return 1;
	case MF_POWF:
	case MF_ATAN2:
	case MF_FMOD:
	case MF_HYPOT: {
	    double e = MD(v[1]);

	    if ((!v[0].isD && (v[0].w > M_EXACT || v[0].w < -M_EXACT))
		    || (!v[1].isD && (v[1].w > M_EXACT
			|| v[1].w < -M_EXACT))) {
		return 0;
	    }
	    if (nd->fn == MF_FMOD && e == 0.0) {
		return 0;
	    }
	    r = (nd->fn == MF_POWF) ? pow(d, e) : (nd->fn == MF_ATAN2)
		    ? atan2(d, e) : (nd->fn == MF_FMOD) ? fmod(d, e)
		    : hypot(d, e);
	    break;
	}
	case MF_MIN:
	case MF_MAX:
	    *out = v[0];
	    for (k = 1; k < nd->nargs; k++) {
		c = MCmp(v[k], *out);
		if (c == 2) {
		    return 0;
		}
		if (nd->fn == MF_MIN ? c < 0 : c > 0) {
		    *out = v[k];
		}
	    }
	    return 1;
	default:
	    return 0;
	}
	if (!MFinite(r) || (!v[0].isD && nd->fn != MF_ABS
		&& (v[0].w > M_EXACT || v[0].w < -M_EXACT))) {
	    return 0;
	}
	out->d = r;
	return 1;
    }

static int
MEval(const MProg *g, int i, const MNum *vars, MNum *out)
{
    const MNode *nd = &g->n[i];
    MNum a, b;
    int c;

    switch (nd->kind) {
    case MN_CONST:
	*out = nd->k;
	return 1;
    case MN_VAR:
	*out = vars[nd->slot];
	return 1;
    case MN_NEG:
	if (!MEval(g, nd->a, vars, &a)) {
	    return 0;
	}
	if (a.isD) {
	    out->isD = 1;
	    out->d = -a.d;
	    return 1;
	}
	if (a.w == WIDE_MIN) {
	    return 0;
	}
	out->isD = 0;
	out->w = -a.w;
	return 1;
    case MN_POS:
	return MEval(g, nd->a, vars, out);
    case MN_NOT:
	if (!MEval(g, nd->a, vars, &a)) {
	    return 0;
	}
	out->isD = 0;
	out->w = !MTruth(a);
	return 1;
    case MN_AND:
    case MN_OR:
	if (!MEval(g, nd->a, vars, &a)) {
	    return 0;
	}
	out->isD = 0;
	if (nd->kind == MN_AND ? !MTruth(a) : MTruth(a)) {
	    out->w = (nd->kind == MN_OR);
	    return 1;
	}
	if (!MEval(g, nd->b, vars, &b)) {
	    return 0;
	}
	out->w = MTruth(b);
	return 1;
    case MN_COND:
	if (!MEval(g, nd->a, vars, &a)) {
	    return 0;
	}
	return MEval(g, MTruth(a) ? nd->b : nd->c, vars, out);
    case MN_FUNC:
	return MEvalFunc(g, nd, vars, out);
    default:
	break;
    }

    /* binaires */
    if (!MEval(g, nd->a, vars, &a) || !MEval(g, nd->b, vars, &b)) {
	return 0;
    }
    switch (nd->kind) {
    case MN_LT: case MN_LE: case MN_GT: case MN_GE: case MN_EQ: case MN_NE:
	c = MCmp(a, b);
	if (c == 2) {
	    return 0;
	}
	out->isD = 0;
	out->w = (nd->kind == MN_LT) ? c < 0 : (nd->kind == MN_LE) ? c <= 0
		: (nd->kind == MN_GT) ? c > 0 : (nd->kind == MN_GE) ? c >= 0
		: (nd->kind == MN_EQ) ? c == 0 : c != 0;
	return 1;
    default:
	break;
    }
    if (!a.isD && !b.isD) {
	Tcl_WideInt r;

	out->isD = 0;
	switch (nd->kind) {
	case MN_ADD:
	    if (__builtin_add_overflow(a.w, b.w, &r)) {
		return 0;
	    }
	    break;
	case MN_SUB:
	    if (__builtin_sub_overflow(a.w, b.w, &r)) {
		return 0;
	    }
	    break;
	case MN_MUL:
	    if (__builtin_mul_overflow(a.w, b.w, &r)) {
		return 0;
	    }
	    break;
	case MN_DIV:
	case MN_MOD: {
	    Tcl_WideInt q, m;

	    if (b.w == 0 || (a.w == WIDE_MIN && b.w == -1)) {
		return 0;
	    }
	    q = a.w / b.w;
	    m = a.w % b.w;
	    if (m != 0 && ((m < 0) != (b.w < 0))) {
		q--;			/* vers -inf, comme expr */
		m += b.w;
	    }
	    r = (nd->kind == MN_DIV) ? q : m;
	    break;
	}
	default:
	    return 0;			/* ** entier : expr */
	}
	out->w = r;
	return 1;
    }
    if ((!a.isD && (a.w > M_EXACT || a.w < -M_EXACT))
	    || (!b.isD && (b.w > M_EXACT || b.w < -M_EXACT))) {
	return 0;
    }
    {
	double x = MD(a), y = MD(b), r;

	switch (nd->kind) {
	case MN_ADD: r = x + y; break;
	case MN_SUB: r = x - y; break;
	case MN_MUL: r = x * y; break;
	case MN_DIV:
	    if (y == 0.0) {
		return 0;
	    }
	    r = x / y;
	    break;
	case MN_POW:
	    r = pow(x, y);
	    break;
	default:
	    return 0;			/* % flottant : expr (erreur) */
	}
	if (!MFinite(r)) {
	    return 0;
	}
	out->isD = 1;
	out->d = r;
	return 1;
    }
}


/*
 * Programme sur des flottants seuls (le cas courant : des colonnes de
 * doubles) : la même expression en notation postfixe, évaluée sur une pile
 * de doubles, sans récursion. Admis quand chaque opération a un opérande
 * flottant (les variables, des doubles à l'exécution ; les constantes
 * entières seulement à côté d'un flottant), et que l'expression n'a ni
 * comparaison, ni ?:, ni fonction à résultat entier.
 */
enum {
    MD_VAR, MD_CONST, MD_ADD, MD_SUB, MD_MUL, MD_DIV, MD_POW, MD_NEG,
    MD_F1, MD_F2
};

typedef struct MDOp {
    int op, arg;
    double k;
    Tcl_WideInt w;		/* constante du programme entier */
} MDOp;

typedef struct MDProg {
    MDOp *ops;
    int nops, cap, depth, maxDepth;
    int ok;
} MDProg;

/* 1 : flottant ; 0 : entier (constante) ; -1 : hors du programme */
static int
MDType(const MProg *g, int i)
{
    const MNode *nd = &g->n[i];
    int a, b;

    switch (nd->kind) {
    case MN_VAR:
	return 1;
    case MN_CONST:
	return nd->k.isD ? 1 : (nd->k.w <= M_EXACT && nd->k.w >= -M_EXACT)
		? 0 : -1;
    case MN_NEG:
    case MN_POS:
	return MDType(g, nd->a) == 1 ? 1 : -1;
    case MN_ADD: case MN_SUB: case MN_MUL: case MN_DIV: case MN_POW:
	a = MDType(g, nd->a);
	b = MDType(g, nd->b);
	if (a < 0 || b < 0 || (a == 0 && b == 0)) {
	    return -1;
	}
	return 1;
    case MN_FUNC:
	switch (nd->fn) {
	case MF_SQRT: case MF_SIN: case MF_COS: case MF_TAN: case MF_ASIN:
	case MF_ACOS: case MF_ATAN: case MF_SINH: case MF_COSH:
	case MF_TANH: case MF_EXP: case MF_LOG: case MF_LOG10:
	case MF_FLOOR: case MF_CEIL: case MF_DOUBLE: case MF_ABS:
	    return MDType(g, nd->args[0]) == 1 ? 1 : -1;
	case MF_POWF: case MF_ATAN2: case MF_FMOD: case MF_HYPOT:
	    return (MDType(g, nd->args[0]) == 1 && MDType(g, nd->args[1]) == 1)
		    ? 1 : -1;
	default:
	    return -1;
	}
    default:
	return -1;
    }
}

static void
MDEmit(MDProg *d, int op, int arg, double k, int delta)
{
    if (d->nops == d->cap) {
	d->cap = d->cap ? 2 * d->cap : 16;
	d->ops = (MDOp *) Tcl_Realloc(d->ops, d->cap * sizeof(MDOp));
    }
    d->ops[d->nops].op = op;
    d->ops[d->nops].arg = arg;
    d->ops[d->nops].k = k;
    d->ops[d->nops].w = 0;
    d->nops++;
    d->depth += delta;
    if (d->depth > d->maxDepth) {
	d->maxDepth = d->depth;
    }
}

static void
MDCompile(const MProg *g, int i, MDProg *d)
{
    const MNode *nd = &g->n[i];

    switch (nd->kind) {
    case MN_VAR:
	MDEmit(d, MD_VAR, nd->slot, 0, 1);
	return;
    case MN_CONST:
	MDEmit(d, MD_CONST, 0, nd->k.isD ? nd->k.d : (double) nd->k.w, 1);
	return;
    case MN_POS:
	MDCompile(g, nd->a, d);
	return;
    case MN_NEG:
	MDCompile(g, nd->a, d);
	MDEmit(d, MD_NEG, 0, 0, 0);
	return;
    case MN_FUNC:
	MDCompile(g, nd->args[0], d);
	if (nd->nargs == 2) {
	    MDCompile(g, nd->args[1], d);
	    MDEmit(d, MD_F2, nd->fn, 0, -1);
	} else {
	    MDEmit(d, MD_F1, nd->fn, 0, 0);
	}
	return;
    default:
	MDCompile(g, nd->a, d);
	MDCompile(g, nd->b, d);
	MDEmit(d, nd->kind == MN_ADD ? MD_ADD : nd->kind == MN_SUB ? MD_SUB
		: nd->kind == MN_MUL ? MD_MUL : nd->kind == MN_DIV ? MD_DIV
		: MD_POW, 0, 0, -1);
	return;
    }
}

/*
 * 24/11 : le programme entier. Sur des entiers 64 bits, + - * et la
 * négation donnent exactement le résultat d'expr tant qu'ils ne débordent
 * pas ; un dépassement marque l'élément (expr en fait un grand entier).
 * Les constantes doivent être entières (un double rendrait le calcul
 * flottant).
 */
static int
MIType(const MProg *g, int i)
{
    const MNode *nd = &g->n[i];

    switch (nd->kind) {
    case MN_VAR:
	return 1;
    case MN_CONST:
	return !nd->k.isD;
    case MN_NEG: case MN_POS:
	return MIType(g, nd->a);
    case MN_ADD: case MN_SUB: case MN_MUL:
	return MIType(g, nd->a) && MIType(g, nd->b);
    default:
	return 0;
    }
}

static void
MICompile(const MProg *g, int i, MDProg *d)
{
    const MNode *nd = &g->n[i];

    switch (nd->kind) {
    case MN_VAR:
	MDEmit(d, MD_VAR, nd->slot, 0, 1);
	return;
    case MN_CONST:
	MDEmit(d, MD_CONST, 0, 0, 1);
	d->ops[d->nops - 1].w = nd->k.w;
	return;
    case MN_POS:
	MICompile(g, nd->a, d);
	return;
    case MN_NEG:
	MICompile(g, nd->a, d);
	MDEmit(d, MD_NEG, 0, 0, 0);
	return;
    default:
	MICompile(g, nd->a, d);
	MICompile(g, nd->b, d);
	MDEmit(d, nd->kind == MN_ADD ? MD_ADD : nd->kind == MN_SUB ? MD_SUB
		: MD_MUL, 0, 0, -1);
	return;
    }
}

/* une opération entière ; 0 si elle déborde */
static inline int
MIOp(int op, Tcl_WideInt x, Tcl_WideInt y, Tcl_WideInt *r)
{
#if defined(__GNUC__)
    switch (op) {
    case MD_ADD: return !__builtin_add_overflow(x, y, r);
    case MD_SUB: return !__builtin_sub_overflow(x, y, r);
    default: return !__builtin_mul_overflow(x, y, r);
    }
#else
    switch (op) {
    case MD_ADD:
	if ((y > 0 && x > WIDE_MAX - y) || (y < 0 && x < WIDE_MIN - y)) {
	    return 0;
	}
	*r = x + y;
	return 1;
    case MD_SUB:
	if ((y < 0 && x > WIDE_MAX + y) || (y > 0 && x < WIDE_MIN + y)) {
	    return 0;
	}
	*r = x - y;
	return 1;
    default:
	if (x != 0 && y != 0) {
	    Tcl_WideInt q = x * y;	/* (contrôlé par la division) */

	    if ((x == -1 && y == WIDE_MIN) || (y == -1 && x == WIDE_MIN)
		    || q / y != x) {
		return 0;
	    }
	    *r = q;
	    return 1;
	}
	*r = 0;
	return 1;
    }
#endif
}

/* 1 et *out ; 0 : l'élément passe par le chemin général (comme MEval) */
static int
MDRun(const MDProg *d, const MNum *vars, double *out)
{
    double st[32];
    int sp = 0, k;

    for (k = 0; k < d->nops; k++) {
	const MDOp *o = &d->ops[k];
	double x, y, r;

	switch (o->op) {
	case MD_VAR:
	    st[sp++] = vars[o->arg].d;
	    continue;
	case MD_CONST:
	    st[sp++] = o->k;
	    continue;
	case MD_NEG:
	    st[sp - 1] = -st[sp - 1];
	    continue;
	case MD_F1:
	    x = st[sp - 1];
	    switch (o->arg) {
	    case MF_ABS: r = fabs(x); break;
	    case MF_SQRT: if (x < 0) return 0; r = sqrt(x); break;
	    case MF_SIN: r = sin(x); break;
	    case MF_COS: r = cos(x); break;
	    case MF_TAN: r = tan(x); break;
	    case MF_ASIN: r = asin(x); break;
	    case MF_ACOS: r = acos(x); break;
	    case MF_ATAN: r = atan(x); break;
	    case MF_SINH: r = sinh(x); break;
	    case MF_COSH: r = cosh(x); break;
	    case MF_TANH: r = tanh(x); break;
	    case MF_EXP: r = exp(x); break;
	    case MF_LOG: if (x <= 0) return 0; r = log(x); break;
	    case MF_LOG10: if (x <= 0) return 0; r = log10(x); break;
	    case MF_FLOOR: r = floor(x); break;
	    case MF_CEIL: r = ceil(x); break;
	    default: r = x; break;		/* double() */
	    }
	    break;
	case MD_F2:
	    y = st[--sp];
	    x = st[sp - 1];
	    switch (o->arg) {
	    case MF_POWF: r = pow(x, y); break;
	    case MF_ATAN2: r = atan2(x, y); break;
	    case MF_FMOD: if (y == 0.0) return 0; r = fmod(x, y); break;
	    default: r = hypot(x, y); break;
	    }
	    break;
	default:
	    y = st[--sp];
	    x = st[sp - 1];
	    switch (o->op) {
	    case MD_ADD: r = x + y; break;
	    case MD_SUB: r = x - y; break;
	    case MD_MUL: r = x * y; break;
	    case MD_DIV: if (y == 0.0) return 0; r = x / y; break;
	    default: r = pow(x, y); break;
	    }
	    break;
	}
	if (isnan(r) || isinf(r)) {
	    return 0;
	}
	st[sp - 1] = r;
    }
    *out = st[0];
    return 1;
}

/*
 * Le même programme sur un bloc d'éléments (le « résumé » le temps d'une
 * opération) : chaque instruction s'applique au bloc entier, colonne par
 * colonne. L'interprétation se paie une fois par bloc, et les boucles simples
 * se vectorisent. Résultat identique à MDRun, élément par élément : les mêmes
 * opérations, dans le même ordre, et un élément est marqué (bad) exactement
 * quand MDRun le refuserait, c'est-à-dire quand une opération donne un
 * résultat non fini (les refus de domaine de MDRun, racine d'un négatif,
 * log de 0, division par 0, donnent tous un résultat non fini).
 *
 * Le contrôle est différé : +, -, * et la négation gardent un opérande non
 * fini non fini (inf*0 donne nan), donc on ne vérifie qu'avant une opération
 * qui pourrait le rendre fini (/, **, fonctions) et à la fin.
 */
#define MD_BLK 256

/* les boucles du bloc vectorisées (exactes : opérations IEEE élément par
 * élément, sans réassociation ni contraction) */
#if defined(__GNUC__) && !defined(__clang__)
#   define MD_VECTORIZE __attribute__((optimize("tree-vectorize", "fp-contract=off")))
#else
#   define MD_VECTORIZE
#endif

/* La somme de n cases d'entiers (8, 16, 32 bits), sur 64 bits. */
static Tcl_WideInt MD_VECTORIZE
MapCellSum(const unsigned char *p, size_t stride, int isU, int size,
	Tcl_Size n)
{
    Tcl_WideInt s = 0;
    Tcl_Size i;

    if (size == 1 && stride == 1) {
	if (isU) {
	    const uint8_t *q = p;
	    uint64_t u = 0;

	    for (i = 0; i < n; i++) {
		u += q[i];
	    }
	    return (Tcl_WideInt) u;
	} else {
	    const int8_t *q = (const int8_t *) p;

	    for (i = 0; i < n; i++) {
		s += q[i];
	    }
	    return s;
	}
    }
    for (i = 0; i < n; i++, p += stride) {
	switch (size) {
	case 1:
	    s += isU ? (Tcl_WideInt) *(const uint8_t *) p
		    : (Tcl_WideInt) *(const int8_t *) p;
	    break;
	case 2: {
	    uint16_t u;

	    memcpy(&u, p, 2);
	    s += isU ? (Tcl_WideInt) u : (Tcl_WideInt) (int16_t) u;
	    break;
	}
	default: {
	    uint32_t u;

	    memcpy(&u, p, 4);
	    s += isU ? (Tcl_WideInt) u : (Tcl_WideInt) (int32_t) u;
	    break;
	}
	}
    }
    return s;
}

typedef struct MDEnt {
    const double *p;		/* NULL : la constante k */
    double k;
    int pend;			/* sortie d'opération pas encore vérifiée */
} MDEnt;

static MD_VECTORIZE void
MDCheck(MDEnt *x, int nb, unsigned char *bad)
{
    int e;

    if (!x->pend) {
	return;
    }
    x->pend = 0;
    if (x->p == NULL) {
	if (!(fabs(x->k) <= DBL_MAX)) {
	    memset(bad, 1, nb);
	}
	return;
    }
    {
	const double *p = x->p;
	int any = 0;

	for (e = 0; e < nb; e++) {
	    any |= !(fabs(p[e]) <= DBL_MAX);
	}
	if (any) {
	    for (e = 0; e < nb; e++) {
		if (!(fabs(p[e]) <= DBL_MAX)) {
		    bad[e] = 1;
		}
	    }
	}
    }
}

static double
MDF1(int fn, double x)
{
    switch (fn) {
    case MF_ABS: return fabs(x);
    case MF_SQRT: return sqrt(x);
    case MF_SIN: return sin(x);
    case MF_COS: return cos(x);
    case MF_TAN: return tan(x);
    case MF_ASIN: return asin(x);
    case MF_ACOS: return acos(x);
    case MF_ATAN: return atan(x);
    case MF_SINH: return sinh(x);
    case MF_COSH: return cosh(x);
    case MF_TANH: return tanh(x);
    case MF_EXP: return exp(x);
    case MF_LOG: return log(x);
    case MF_LOG10: return log10(x);
    case MF_FLOOR: return floor(x);
    case MF_CEIL: return ceil(x);
    default: return x;
    }
}

static double
MDF2(int fn, double x, double y)
{
    switch (fn) {
    case MF_POWF: return pow(x, y);
    case MF_ATAN2: return atan2(x, y);
    case MF_FMOD: return fmod(x, y);
    default: return hypot(x, y);
    }
}

/* cols[s] : la colonne du bloc pour une variable de boucle, NULL pour une
 * variable lue une fois (vals[s].d) ; scratch : maxDepth * MD_BLK doubles ;
 * bad : déjà marqué pour les éléments à écarter (entrées) ; out : les
 * résultats */
static MD_VECTORIZE void
MDRunBlock(const MDProg *d, double *const *cols, const MNum *vals, int nb,
	double *scratch, unsigned char *bad, double *out)
{
    MDEnt st[32];
    int sp = 0, k, e;

    for (k = 0; k < d->nops; k++) {
	const MDOp *o = &d->ops[k];
	MDEnt *x, *y;
	double *dst;

	switch (o->op) {
	case MD_VAR:
	    st[sp].p = cols[o->arg];
	    st[sp].k = cols[o->arg] ? 0.0 : vals[o->arg].d;
	    st[sp++].pend = 0;
	    continue;
	case MD_CONST:
	    st[sp].p = NULL;
	    st[sp].k = o->k;
	    st[sp++].pend = 0;
	    continue;
	case MD_NEG:
	    x = &st[sp - 1];
	    if (x->p == NULL) {
		x->k = -x->k;
	    } else {
		const double *p = x->p;

		dst = scratch + (size_t) (sp - 1) * MD_BLK;
		for (e = 0; e < nb; e++) {
		    dst[e] = -p[e];
		}
		x->p = dst;
	    }
	    continue;
	case MD_F1:
	    x = &st[sp - 1];
	    MDCheck(x, nb, bad);
	    if (x->p == NULL) {
		x->k = MDF1(o->arg, x->k);
	    } else {
		const double *p = x->p;

		dst = scratch + (size_t) (sp - 1) * MD_BLK;
		switch (o->arg) {
		case MF_ABS:
		    for (e = 0; e < nb; e++) dst[e] = fabs(p[e]);
		    break;
		case MF_SQRT:
		    for (e = 0; e < nb; e++) dst[e] = sqrt(p[e]);
		    break;
		default:
		    for (e = 0; e < nb; e++) dst[e] = MDF1(o->arg, p[e]);
		    break;
		}
		x->p = dst;
	    }
	    x->pend = 1;
	    continue;
	default:
	    break;
	}

	/* binaires */
	y = &st[--sp];
	x = &st[sp - 1];
	if (o->op == MD_F2 || o->op == MD_DIV || o->op == MD_POW) {
	    MDCheck(x, nb, bad);
	    MDCheck(y, nb, bad);
	}
	dst = scratch + (size_t) (sp - 1) * MD_BLK;
	if (x->p == NULL && y->p == NULL) {
	    double a = x->k, b = y->k, r;

	    switch (o->op) {
	    case MD_ADD: r = a + b; break;
	    case MD_SUB: r = a - b; break;
	    case MD_MUL: r = a * b; break;
	    case MD_DIV: r = a / b; break;
	    case MD_POW: r = pow(a, b); break;
	    default: r = MDF2(o->arg, a, b); break;
	    }
	    x->k = r;
	} else {
	    const double *a = x->p, *b = y->p;
	    double ka = x->k, kb = y->k;

#define MD_LOOP(EXPR) \
	    if (a && b) { for (e = 0; e < nb; e++) { double xa = a[e], yb = b[e]; dst[e] = (EXPR); } } \
	    else if (a) { for (e = 0; e < nb; e++) { double xa = a[e], yb = kb; dst[e] = (EXPR); } } \
	    else { for (e = 0; e < nb; e++) { double xa = ka, yb = b[e]; dst[e] = (EXPR); } }
	    switch (o->op) {
	    case MD_ADD: MD_LOOP(xa + yb); break;
	    case MD_SUB: MD_LOOP(xa - yb); break;
	    case MD_MUL: MD_LOOP(xa * yb); break;
	    case MD_DIV: MD_LOOP(xa / yb); break;
	    case MD_POW: MD_LOOP(pow(xa, yb)); break;
	    default: {
		int fn = o->arg;

		MD_LOOP(MDF2(fn, xa, yb));
		break;
	    }
	    }
#undef MD_LOOP
	    x->p = dst;
	}
	x->pend = 1;
    }
    MDCheck(&st[0], nb, bad);
    if (st[0].p == NULL) {
	for (e = 0; e < nb; e++) {
	    out[e] = st[0].k;
	}
    } else if (st[0].p != out) {
	memcpy(out, st[0].p, nb * sizeof(double));
    }
}

/* le programme entier sur un bloc ; bad : déjà marqué (entrées), marqué
 * en plus pour un dépassement */
typedef struct MIEnt {
    const Tcl_WideInt *p;		/* NULL : la constante k */
    Tcl_WideInt k;
} MIEnt;

static void
MIRunBlock(const MDProg *d, Tcl_WideInt *const *cols, const MNum *vals,
	int nb, Tcl_WideInt *scratch, unsigned char *bad, Tcl_WideInt *out)
{
    MIEnt st[32];
    int sp = 0, k, e;

    for (k = 0; k < d->nops; k++) {
	const MDOp *o = &d->ops[k];
	MIEnt *x, *y;
	Tcl_WideInt *dst;

	switch (o->op) {
	case MD_VAR:
	    st[sp].p = cols[o->arg];
	    st[sp++].k = cols[o->arg] ? 0 : vals[o->arg].w;
	    continue;
	case MD_CONST:
	    st[sp].p = NULL;
	    st[sp++].k = o->w;
	    continue;
	case MD_NEG:
	    x = &st[sp - 1];
	    if (x->p == NULL) {
		if (x->k == WIDE_MIN) {
		    memset(bad, 1, nb);
		} else {
		    x->k = -x->k;
		}
	    } else {
		const Tcl_WideInt *p = x->p;

		dst = scratch + (size_t) (sp - 1) * MD_BLK;
		for (e = 0; e < nb; e++) {
		    if (p[e] == WIDE_MIN) {
			bad[e] = 1;
			dst[e] = 0;
		    } else {
			dst[e] = -p[e];
		    }
		}
		x->p = dst;
	    }
	    continue;
	default:
	    break;
	}
	y = &st[--sp];
	x = &st[sp - 1];
	dst = scratch + (size_t) (sp - 1) * MD_BLK;
	if (x->p == NULL && y->p == NULL) {
	    Tcl_WideInt r;

	    if (!MIOp(o->op, x->k, y->k, &r)) {
		memset(bad, 1, nb);
		r = 0;
	    }
	    x->k = r;
	    continue;
	}
	for (e = 0; e < nb; e++) {
	    Tcl_WideInt a = x->p ? x->p[e] : x->k, b = y->p ? y->p[e] : y->k, r;

	    if (!MIOp(o->op, a, b, &r)) {
		bad[e] = 1;
		r = 0;
	    }
	    dst[e] = r;
	}
	x->p = dst;
    }
    if (st[0].p == NULL) {
	for (e = 0; e < nb; e++) {
	    out[e] = st[0].k;
	}
    } else if (st[0].p != out) {
	memcpy(out, st[0].p, nb * sizeof(Tcl_WideInt));
    }
}

static void
MProgFree(MProg *g)
{
    int i;

    for (i = 0; i < g->nnames; i++) {
	Tcl_DecrRefCount(g->names[i]);
    }
    if (g->names) {
	Tcl_Free(g->names);
    }
    if (g->n) {
	Tcl_Free(g->n);
    }
}

/* un nombre simple (entier 64 bits ou flottant), contenu d'une valeur typée
 * compris */
static int
MNumOf(Tcl_Obj *o, MNum *out)
{
    void *ptr;
    int type;

    if (o == NULL) {
	return 0;
    }
    o = TCL_ANNOT_CONTENT(o);
    if (TclHasInternalRep(o, &tclDoubleType)) {
	out->isD = 1;
	out->d = o->internalRep.doubleValue;
	return !isnan(out->d);
    }
    if (TclHasInternalRep(o, &tclIntType)) {
	out->isD = 0;
	out->w = o->internalRep.wideValue;
	return 1;
    }
    if (Tcl_GetNumberFromObj(NULL, o, &ptr, &type) != TCL_OK) {
	return 0;
    }
    if (type == TCL_NUMBER_INT) {
	out->isD = 0;
	out->w = *(Tcl_WideInt *) ptr;
	return 1;
    }
    if (type == TCL_NUMBER_DOUBLE) {
	out->isD = 1;
	out->d = *(double *) ptr;
	return 1;
    }
    return 0;
}

/*
 * v17u : les prédicats de type, T((expression)). « $ » seul (suivi d'un
 * caractère qui ne commence pas un nom) désigne la valeur.
 *
 * PredRewrite rend le texte où chaque $ seul est remplacé par repl, hors
 * des chaînes "..." et des {...} (des littéraux pour expr).
 */
static Tcl_Obj *
PredRewrite(Tcl_Obj *pred, const char *repl, Tcl_Size rlen)
{
    Tcl_Size len, i;
    const char *p = Tcl_GetStringFromObj(pred, &len);
    Tcl_Obj *r = Tcl_NewObj();
    int dq = 0, br = 0;
    Tcl_Size from = 0;

    for (i = 0; i < len; i++) {
	char c = p[i];

	if (c == '\\') {
	    i++;
	    continue;
	}
	if (c == '"' && br == 0) {
	    dq = !dq;
	} else if (!dq && c == '{') {
	    br++;
	} else if (!dq && c == '}' && br > 0) {
	    br--;
	} else if (c == '$' && !dq && br == 0) {
	    char nc = (i + 1 < len) ? p[i + 1] : ' ';

	    if (!(isalnum(UCHAR(nc)) || nc == '_' || nc == ':' || nc == '{'
		    || nc == '(')) {
		Tcl_AppendToObj(r, p + from, i - from);
		Tcl_AppendToObj(r, repl, rlen);
		from = i + 1;
	    }
	}
    }
    Tcl_AppendToObj(r, p + from, len - from);
    return r;
}

typedef struct PredNum {
    MProg g;
    int root;
    Tcl_Obj **gnames;		/* __gK : la variable qualifiée lue (::max) */
    int ng;
} PredNum;

/* Les $::nom (hors chaînes et accolades) remplacés par $__gK : le
 * sous-ensemble numérique ne lit que des noms simples. */
static Tcl_Obj *
PredGlobals(Tcl_Obj *text, Tcl_Obj ***namesPtr, int *nPtr)
{
    Tcl_Size len, i, from = 0;
    const char *p = Tcl_GetStringFromObj(text, &len);
    Tcl_Obj *r = Tcl_NewObj();
    int dq = 0, br = 0;

    *namesPtr = NULL;
    *nPtr = 0;
    for (i = 0; i < len; i++) {
	char c = p[i];

	if (c == '\\') {
	    i++;
	    continue;
	}
	if (c == '"' && br == 0) {
	    dq = !dq;
	} else if (!dq && c == '{') {
	    br++;
	} else if (!dq && c == '}' && br > 0) {
	    br--;
	} else if (c == '$' && !dq && br == 0 && i + 2 < len
		&& p[i + 1] == ':' && p[i + 2] == ':') {
	    Tcl_Size j = i + 1;
	    char buf[32];

	    while (j < len && (isalnum(UCHAR(p[j])) || p[j] == '_'
		    || p[j] == ':')) {
		j++;
	    }
	    *namesPtr = (Tcl_Obj **) Tcl_Realloc(*namesPtr,
		    (*nPtr + 1) * sizeof(Tcl_Obj *));
	    (*namesPtr)[*nPtr] = Tcl_NewStringObj(p + i + 1, j - i - 1);
	    Tcl_IncrRefCount((*namesPtr)[*nPtr]);
	    snprintf(buf, sizeof(buf), "$__g%d", *nPtr);
	    (*nPtr)++;
	    Tcl_AppendToObj(r, p + from, i - from);
	    Tcl_AppendToObj(r, buf, -1);
	    from = j;
	    i = j - 1;
	}
    }
    Tcl_AppendToObj(r, p + from, len - from);
    return r;
}

static void
PredProgFree(TclType *tp)
{
    if (tp->predProg != NULL && tp->predProg != (void *) 1) {
	PredNum *pn = (PredNum *) tp->predProg;
	int k;

	MProgFree(&pn->g);
	for (k = 0; k < pn->ng; k++) {
	    Tcl_DecrRefCount(pn->gnames[k]);
	}
	if (pn->gnames != NULL) {
	    Tcl_Free(pn->gnames);
	}
	Tcl_Free(pn);
    }
    tp->predProg = NULL;
}

/* La forme numérique (le sous-ensemble de type map), ou NULL. */
static PredNum *
PredNumOf(const TclType *tp)
{
    TclType *mt = (TclType *) tp;

    if (mt->predProg == NULL) {
	Tcl_Obj *t0 = PredRewrite(tp->pred, "$__v", 4), *t;
	PredNum *pn = (PredNum *) Tcl_Alloc(sizeof(PredNum));
	int k, bad = 0;

	memset(pn, 0, sizeof(*pn));
	Tcl_IncrRefCount(t0);
	t = PredGlobals(t0, &pn->gnames, &pn->ng);
	Tcl_DecrRefCount(t0);
	Tcl_IncrRefCount(t);
	pn->g.p = Tcl_GetString(t);
	pn->root = MExpr(&pn->g);
	MSkip(&pn->g);
	for (k = 0; k < pn->g.nnames; k++) {
	    const char *nm = Tcl_GetString(pn->g.names[k]);

	    if (strcmp(nm, "__v") != 0 && strncmp(nm, "__g", 3) != 0) {
		bad = 1;
	    }
	}
	if (bad || pn->g.bad || pn->root < 0 || *pn->g.p != '\0'
		|| pn->g.nnames > 8) {
	    MProgFree(&pn->g);
	    for (k = 0; k < pn->ng; k++) {
		Tcl_DecrRefCount(pn->gnames[k]);
	    }
	    if (pn->gnames != NULL) {
		Tcl_Free(pn->gnames);
	    }
	    Tcl_Free(pn);
	    mt->predProg = (void *) 1;
	} else {
	    pn->g.p = NULL;
	    mt->predProg = pn;
	}
	Tcl_DecrRefCount(t);
    }
    return (mt->predProg == (void *) 1) ? NULL : (PredNum *) mt->predProg;
}

/* Les valeurs des variables de la forme numérique : la valeur, et les
 * variables qualifiées lues à l'instant ; 0 si l'une n'est pas un nombre. */
static int
PredVars(Tcl_Interp *pi, const PredNum *pn, Tcl_Obj *value, MNum *vars)
{
    int k;

    for (k = 0; k < pn->g.nnames; k++) {
	const char *nm = Tcl_GetString(pn->g.names[k]);
	Tcl_Obj *v;

	if (nm[2] == 'v') {
	    v = value;
	} else {
	    int gi = atoi(nm + 3);

	    if (gi < 0 || gi >= pn->ng) {
		return 0;
	    }
	    v = Tcl_ObjGetVar2(pi, pn->gnames[gi], NULL, 0);
	}
	if (v == NULL || !MNumOf(v, &vars[k])) {
	    return 0;
	}
    }
    return 1;
}

/* La valeur, en littéral d'expr : "..." où chaque caractère spécial est
 * précédé d'une barre oblique inverse. */
static void
PredQuote(Tcl_DString *ds, Tcl_Obj *value)
{
    Tcl_Size len, i;
    const char *v = Tcl_GetStringFromObj(value, &len);

    Tcl_DStringAppend(ds, "\"", 1);
    for (i = 0; i < len; i++) {
	switch (v[i]) {
	case '\\': case '"': case '$': case '[': case ']': case '{': case '}':
	    Tcl_DStringAppend(ds, "\\", 1);
	    Tcl_DStringAppend(ds, v + i, 1);
	    break;
	case '\n':
	    Tcl_DStringAppend(ds, "\\n", 2);
	    break;
	default:
	    Tcl_DStringAppend(ds, v + i, 1);
	}
    }
    Tcl_DStringAppend(ds, "\"", 1);
}

/*
 * Vérifie le prédicat de tp sur value (son contenu). Évalué dans
 * l'interpréteur du type, son résultat et son état d'erreur gardés ; une
 * erreur de l'expression vaut refus (le message la cite).
 */
static Tcl_ThreadDataKey predTrustKey;

/* v17u : une valeur qui a déjà le visa (déjà de ce type) n'est pas
 * revérifiée pendant qu'on la recopie : TclTypePredTrust(+1) ... (-1). */
void
TclTypePredTrust(int delta)
{
    int *tr = (int *) Tcl_GetThreadData(&predTrustKey, sizeof(int));

    *tr += delta;
}

static int
PredCheck(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *value)
{
    Tcl_Interp *pi = tp->predInterp;
    PredNum *pn;
    MNum vars[8], out;
    int ok = 0, rc;
    Tcl_Obj *why = NULL;

    if (pi == NULL || (tp->owner != NULL && tp->owner->dead)) {
	return TCL_OK;			/* l'interpréteur n'est plus */
    }
    if (*(int *) Tcl_GetThreadData(&predTrustKey, sizeof(int)) > 0) {
	return TCL_OK;			/* le visa : déjà de ce type */
    }
    value = TCL_ANNOT_CONTENT(value);
    if ((pn = PredNumOf(tp)) != NULL && PredVars(pi, pn, value, vars)
	    && MEval(&pn->g, pn->root, vars, &out)
	    && !(out.isD && isnan(out.d))) {
	ok = out.isD ? (out.d != 0.0) : (out.w != 0);
    } else {
	/* la valeur en littéral, à la place de $ */
	Tcl_DString lit;
	Tcl_Obj *t;
	Tcl_InterpState st;
	int b = 0;

	Tcl_DStringInit(&lit);
	PredQuote(&lit, value);
	t = PredRewrite(tp->pred, Tcl_DStringValue(&lit),
		Tcl_DStringLength(&lit));
	Tcl_DStringFree(&lit);
	Tcl_IncrRefCount(t);
	st = Tcl_SaveInterpState(pi, TCL_OK);
	rc = Tcl_ExprBooleanObj(pi, t, &b);
	if (rc != TCL_OK) {
	    why = Tcl_DuplicateObj(Tcl_GetObjResult(pi));
	    Tcl_IncrRefCount(why);
	}
	Tcl_RestoreInterpState(pi, st);
	Tcl_DecrRefCount(t);
	ok = (rc == TCL_OK && b);
    }
    if (ok) {
	return TCL_OK;
    }
    if (interp != NULL) {
	if (why != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("value \"%s\" is not of type %s: "
		    "its predicate %s fails: %s", Tcl_GetString(value),
		    tp->name, Tcl_GetString(tp->pred), Tcl_GetString(why)));
	} else {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("value \"%s\" is not of type %s: "
		    "%s is false", Tcl_GetString(value), tp->name,
		    Tcl_GetString(tp->pred)));
	}
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "PRED", (char *) NULL);
    }
    if (why != NULL) {
	Tcl_DecrRefCount(why);
    }
    return TCL_ERROR;
}

static Tcl_Obj *
MListContent(Tcl_Obj *o)
{
    Tcl_Obj *c = NULL;

    if (TclAnnotInfo(o, &c, NULL, NULL, NULL) == 1 && c != NULL) {
	return c;
    }
    return o;
}

/* 24/11 : -fold {s init} : le corps donne la nouvelle valeur de s */
typedef struct MapFold {
    Tcl_Obj *name;
    Tcl_Obj *init;
} MapFold;

static int TypeMapCore(Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[], const MapFold *fold);
static int TypeMapList(Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[], const MapFold *fold);

/*
 * {x y z} $P(*.(a,b,c)) : à une liste de K variables répond une liste de K
 * colonnes de même longueur (ce que rend un chemin à plusieurs champs) ;
 * elle devient K couples x COLONNE.
 */
/*
 * type map -sum ... : la somme des résultats, comme ::tcl::mathop::+ sur la
 * liste (de gauche à droite) ; en C tant que les nombres le permettent,
 * sinon par ::tcl::mathop::+ lui-même.
 */
/*
 * 24/11 : type def T DEF {-compact}F ... : T, puis chaque forme F de T, de
 * même définition, qui ne diffère que par sa disposition (options de
 * disposition seules). F->formOf = T.
 */
static int TypeObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[]);

static int
TypeDefForms(Tcl_Interp *interp, TypeData *d, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    Tcl_Obj *av[4], *words, *nameObj, **wv;
    TclObjVec keep = {NULL, 0, 0};
    TclType *base, *form;
    Tcl_Size k, nw, j;
    Tcl_HashEntry *h;
    Tcl_DString qd;
    int rc;

    if (strchr(Tcl_GetString(objv[2]), '(') != NULL) {
	ERR(interp, "OPTION", "type def: forms are not yet possible for a "
		"type family (%s)", Tcl_GetString(objv[2]));
	return TCL_ERROR;
    }
    /* chaque forme : des options de disposition, et un nom */
    for (k = 4; k < objc; k++) {
	int layout = 0;

	LocalWords(objv[k], &words, &nameObj, &keep);
	Tcl_IncrRefCount(words);
	Tcl_ListObjGetElements(NULL, words, &nw, &wv);
	for (j = 0; j < nw; j++) {
	    if (strcmp(Tcl_GetString(wv[j]), "-compact") == 0) {
		layout = 1;
	    } else {
		ERR(interp, "OPTION", "type def: a form takes only layout "
			"options ({-compact}), not {%s}", Tcl_GetString(wv[j]));
		Tcl_DecrRefCount(words);
		TclVecFree(&keep);
		return TCL_ERROR;
	    }
	}
	Tcl_DecrRefCount(words);
	if (!layout) {
	    ERR(interp, "OPTION", "type def: the form %s must say its layout, "
		    "as {-compact}%s", Tcl_GetString(nameObj),
		    Tcl_GetString(nameObj));
	    TclVecFree(&keep);
	    return TCL_ERROR;
	}
    }
    TclVecFree(&keep);
    av[0] = objv[0];
    av[1] = objv[1];
    av[2] = objv[2];
    av[3] = objv[3];
    rc = TypeObjCmd(d, interp, 4, av);
    if (rc != TCL_OK) {
	return rc;
    }
    {
	/* le nom de base, sans ses options */
	Tcl_Obj *bn;

	LocalWords(objv[2], &words, &bn, &keep);
	Tcl_IncrRefCount(words);
	rc = TclTypesQualify(interp, d, Tcl_GetString(bn), &qd);
	Tcl_DecrRefCount(words);
	TclVecFree(&keep);
	if (rc != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    h = Tcl_FindHashEntry(&d->names, Tcl_DStringValue(&qd));
    Tcl_DStringFree(&qd);
    base = (h != NULL) ? (TclType *) Tcl_GetHashValue(h) : NULL;
    if (base == NULL) {
	ERR(interp, "OPTION", "type def: %s was not defined",
		Tcl_GetString(objv[2]));
	return TCL_ERROR;
    }
    for (k = 4; k < objc; k++) {
	Tcl_Obj *fn;

	av[2] = objv[k];
	rc = TypeObjCmd(d, interp, 4, av);
	if (rc != TCL_OK) {
	    /* tout ou rien : la base et les formes déjà faites sont retirées */
	    Tcl_Size kk;

	    for (kk = 2; kk < k; kk = (kk == 2) ? 4 : kk + 1) {
		Tcl_Obj *nn;

		LocalWords(objv[kk], &words, &nn, &keep);
		Tcl_IncrRefCount(words);
		if (TclTypesQualify(interp, d, Tcl_GetString(nn), &qd)
			== TCL_OK) {
		    h = Tcl_FindHashEntry(&d->names, Tcl_DStringValue(&qd));
		    if (h != NULL) {
			Tcl_DeleteHashEntry(h);
			d->gen++;
		    }
		    Tcl_DStringFree(&qd);
		}
		Tcl_DecrRefCount(words);
		TclVecFree(&keep);
	    }
	    return rc;
	}
	LocalWords(objv[k], &words, &fn, &keep);
	Tcl_IncrRefCount(words);
	rc = TclTypesQualify(interp, d, Tcl_GetString(fn), &qd);
	Tcl_DecrRefCount(words);
	TclVecFree(&keep);
	if (rc != TCL_OK) {
	    return TCL_ERROR;
	}
	h = Tcl_FindHashEntry(&d->names, Tcl_DStringValue(&qd));
	Tcl_DStringFree(&qd);
	form = (h != NULL) ? (TclType *) Tcl_GetHashValue(h) : NULL;
	if (form != NULL) {
	    form->formOf = base;
	}
    }
    Tcl_ResetResult(interp);
    return TCL_OK;
}

static int
TypeMapSum(Tcl_Interp *interp, Tcl_Obj *list)
{
    Tcl_Obj *c = MListContent(list), **ev;
    Tcl_Size n, i, cn;
    const unsigned char *data;
    size_t stride;
    int ckind, csize;
    MNum acc, x;

    if (TclCompactScalarInfo(c, &cn, &data, &stride, &ckind, &csize)
	    && ckind == 2 && csize == 8) {
	double d = 0.0;

	for (i = 0; i < cn; i++) {
	    double e;

	    memcpy(&e, data + (size_t) i * stride, 8);
	    d = (i == 0) ? e : d + e;
	}
	if (cn > 0 && !isnan(d) && !isinf(d)) {
	    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(d));
	    return TCL_OK;
	}
    } else if (TclCompactScalarInfo(c, &cn, &data, &stride, &ckind, &csize)
	    && ckind == 0 && csize == 8 && cn > 0) {
	/* 24/11 : des entiers en cases, sans un objet par élément ; un
	 * dépassement : le cas général (un grand entier) */
	Tcl_WideInt w = 0, e;
	int ok = 1;

	for (i = 0; i < cn && ok; i++) {
	    memcpy(&e, data + (size_t) i * stride, 8);
	    if (i == 0) {
		w = e;
	    } else {
		ok = MIOp(MD_ADD, w, e, &w);
	    }
	}
	if (ok) {
	    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(w));
	    return TCL_OK;
	}
    } else if (Tcl_ListObjGetElements(NULL, c, &n, &ev) == TCL_OK) {
	int ok = 1;

	acc.isD = 0;
	acc.w = 0;
	for (i = 0; i < n && ok; i++) {
	    if (!MNumOf(ev[i], &x)) {
		ok = 0;
	    } else if (i == 0) {
		acc = x;
	    } else if (!acc.isD && !x.isD) {
		ok = !__builtin_add_overflow(acc.w, x.w, &acc.w);
	    } else if ((!acc.isD && (acc.w > M_EXACT || acc.w < -M_EXACT))
		    || (!x.isD && (x.w > M_EXACT || x.w < -M_EXACT))) {
		ok = 0;
	    } else {
		acc.d = MD(acc) + MD(x);
		acc.isD = 1;
		ok = !isnan(acc.d) && !isinf(acc.d);
	    }
	}
	if (ok) {
	    Tcl_SetObjResult(interp, acc.isD ? Tcl_NewDoubleObj(acc.d)
		    : Tcl_NewWideIntObj(acc.w));
	    return TCL_OK;
	}
    }
    {
	/* le cas général : ::tcl::mathop::+ {*}$list */
	Tcl_Obj *cmd = Tcl_NewStringObj("::tcl::mathop::+", -1);
	int rc;

	Tcl_IncrRefCount(cmd);
	if (Tcl_ListObjGetElements(interp, c, &n, &ev) != TCL_OK) {
	    Tcl_DecrRefCount(cmd);
	    return TCL_ERROR;
	}
	{
	    Tcl_Obj **av = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *)
		    * (size_t) (n + 1));

	    av[0] = cmd;
	    memcpy(av + 1, ev, sizeof(Tcl_Obj *) * (size_t) n);
	    for (i = 1; i <= n; i++) {
		Tcl_IncrRefCount(av[i]);
	    }
	    rc = Tcl_EvalObjv(interp, n + 1, av, 0);
	    for (i = 1; i <= n; i++) {
		Tcl_DecrRefCount(av[i]);
	    }
	    Tcl_Free(av);
	}
	Tcl_DecrRefCount(cmd);
	return rc;
    }
}

/* type map -min / -max : comme ::tcl::mathfunc::min / max sur la liste
 * (le premier en cas d'égalité) ; en C pour des doubles en cases ou des
 * nombres simples, sinon par la fonction elle-même. */
static int
TypeMapMinMax(Tcl_Interp *interp, Tcl_Obj *list, int isMax)
{
    Tcl_Obj *c = MListContent(list), **ev;
    Tcl_Size n, i, cn;
    const unsigned char *data;
    size_t stride;
    int ckind, csize;
    MNum best, x;

    if (TclCompactScalarInfo(c, &cn, &data, &stride, &ckind, &csize)
	    && ckind == 2 && csize == 8 && cn > 0) {
	double d, e;

	memcpy(&d, data, 8);
	for (i = 1; i < cn; i++) {
	    memcpy(&e, data + (size_t) i * stride, 8);
	    if (isMax ? e > d : e < d) {
		d = e;
	    }
	}
	if (!isnan(d)) {
	    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(d));
	    return TCL_OK;
	}
    } else if (TclCompactScalarInfo(c, &cn, &data, &stride, &ckind, &csize)
	    && ckind == 0 && csize == 8 && cn > 0) {
	/* des entiers de 64 bits en cases */
	Tcl_WideInt w, e;

	memcpy(&w, data, 8);
	for (i = 1; i < cn; i++) {
	    memcpy(&e, data + (size_t) i * stride, 8);
	    if (isMax ? e > w : e < w) {
		w = e;
	    }
	}
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(w));
	return TCL_OK;
    } else if (Tcl_ListObjGetElements(NULL, c, &n, &ev) == TCL_OK && n > 0) {
	int ok = MNumOf(ev[0], &best);

	for (i = 1; i < n && ok; i++) {
	    int cmp;

	    ok = MNumOf(ev[i], &x);
	    if (ok) {
		cmp = MCmp(x, best);
		if (cmp == 2) {
		    ok = 0;
		} else if (isMax ? cmp > 0 : cmp < 0) {
		    best = x;
		}
	    }
	}
	if (ok) {
	    Tcl_SetObjResult(interp, best.isD ? Tcl_NewDoubleObj(best.d)
		    : Tcl_NewWideIntObj(best.w));
	    return TCL_OK;
	}
    }
    {
	Tcl_Obj *cmd = Tcl_NewStringObj(isMax ? "::tcl::mathfunc::max"
		: "::tcl::mathfunc::min", -1), **av;
	int rc;

	if (Tcl_ListObjGetElements(interp, c, &n, &ev) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_IncrRefCount(cmd);
	av = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) (n + 1));
	av[0] = cmd;
	memcpy(av + 1, ev, sizeof(Tcl_Obj *) * (size_t) n);
	for (i = 1; i <= n; i++) {
	    Tcl_IncrRefCount(av[i]);
	}
	rc = Tcl_EvalObjv(interp, n + 1, av, 0);
	for (i = 1; i <= n; i++) {
	    Tcl_DecrRefCount(av[i]);
	}
	Tcl_Free(av);
	Tcl_DecrRefCount(cmd);
	return rc;
    }
}

/*
 * 24/11 : type map typé.
 *
 *   type map ?-sum|-min|-max? {{double}x; {double}y} $cols ... {T}{(e1, e2)}
 *
 * Une signature d'entrée en produit : ses champs sont les variables, et
 * chaque colonne est vérifiée (ou convertie) une fois à l'entrée, comme un
 * argument de procédure. Un corps annoté donne le type de ce qu'il rend à
 * chaque élément : un scalaire (une colonne), un produit écrit en ligne
 * (une colonne par champ, le tuple du corps les remplit dans l'ordre), ou
 * un produit nommé (une liste de ces produits). Un corps vide annoté
 * projette par les noms. Sans annotation nulle part : type map comme avant.
 */

/* une signature typée ? (une annotation ou un ;) */
static int
MapSpecTyped(Tcl_Obj *spec)
{
    Tcl_Size len;
    const char *s = Tcl_GetStringFromObj(spec, &len);

    const char *e;
    int depth = 0;

    while (len > 0 && isspace(UCHAR(*s))) {
	s++;
	len--;
    }
    if (len == 0) {
	return 0;
    }
    if (memchr(s, ';', len) != NULL) {
	return 1;
    }
    if (*s != '{') {
	return 0;
    }
    /* {T}nom : l'accolade fermante collée à un nom ; {x} {y}, une liste de
     * noms, reste une liste de noms */
    for (e = s; e < s + len; e++) {
	if (*e == '{') {
	    depth++;
	} else if (*e == '}' && --depth == 0) {
	    break;
	}
    }
    return e + 1 < s + len && !isspace(UCHAR(e[1]));
}

/* le corps annoté : son type (résolu) et son texte ; 0 si pas annoté */
static int
MapBodyType(Tcl_Interp *interp, Tcl_Obj *body, TclType **tpPtr,
	Tcl_Obj **contentPtr, int *inlinePtr, int *errPtr)
{
    Tcl_Obj *content = NULL, *raw = NULL;
    TclType *tp = NULL;
    int st = TclAnnotInfo(body, &content, &tp, NULL, &raw);
    const char *name;

    *errPtr = 0;
    if (st == 0) {
	return 0;
    }
    if (st == 2) {
	if (raw == NULL) {
	    return 0;
	}
	name = Tcl_GetString(raw);
	tp = TclTypeParse(interp, name, (Tcl_Size) strlen(name));
	if (tp == NULL) {
	    *errPtr = 1;
	    return 1;
	}
    } else {
	name = tp->name;
    }
    while (isspace(UCHAR(*name))) {
	name++;
    }
    *tpPtr = tp;
    *contentPtr = content;
    *inlinePtr = (tp->kind == TCLT_PRODUCT
	    && (*name == '{' || strchr(name, ';') != NULL));
    return 1;
}

/* (e1, e2, ...) au premier niveau : les composantes, chacune entre
 * parenthèses ; NULL si le corps n'a pas cette forme */
static Tcl_Obj *
MapTupleSplit(const char *s, Tcl_Size len)
{
    const char *e = s + len, *p, *start;
    int depth = 0, quote = 0;
    Tcl_Obj *parts;

    while (s < e && isspace(UCHAR(*s))) {
	s++;
    }
    while (e > s && isspace(UCHAR(e[-1]))) {
	e--;
    }
    if (e - s < 2 || *s != '(' || e[-1] != ')') {
	return NULL;
    }
    /* la parenthèse ouvrante doit fermer à la fin */
    for (p = s; p < e; p++) {
	if (quote) {
	    if (*p == '\\' && p + 1 < e) {
		p++;
	    } else if (*p == '"') {
		quote = 0;
	    }
	    continue;
	}
	if (*p == '\\' && p + 1 < e) {
	    p++;
	} else if (*p == '"') {
	    quote = 1;
	} else if (*p == '(' || *p == '[' || *p == '{') {
	    depth++;
	} else if (*p == ')' || *p == ']' || *p == '}') {
	    if (--depth == 0 && p != e - 1) {
		return NULL;
	    }
	}
    }
    if (depth != 0 || quote) {
	return NULL;
    }
    parts = Tcl_NewListObj(0, NULL);
    depth = 0;
    start = s + 1;
    for (p = s + 1; p < e - 1; p++) {
	if (quote) {
	    if (*p == '\\' && p + 1 < e) {
		p++;
	    } else if (*p == '"') {
		quote = 0;
	    }
	    continue;
	}
	if (*p == '\\' && p + 1 < e) {
	    p++;
	} else if (*p == '"') {
	    quote = 1;
	} else if (*p == '(' || *p == '[' || *p == '{') {
	    depth++;
	} else if (*p == ')' || *p == ']' || *p == '}') {
	    depth--;
	} else if (*p == ',' && depth == 0) {
	    Tcl_ListObjAppendElement(NULL, parts,
		    Tcl_ObjPrintf("(%.*s)", (int) (p - start), start));
	    start = p + 1;
	}
    }
    Tcl_ListObjAppendElement(NULL, parts,
	    Tcl_ObjPrintf("(%.*s)", (int) (e - 1 - start), start));
    return parts;
}

/* une colonne rangée dans list.T : telle quelle si elle y est déjà */
static Tcl_Obj *
MapColumnAs(Tcl_Interp *interp, TclType *et, Tcl_Obj *col, const char *what)
{
    TclType *lt = TclTypeListOf(interp, et), *at = NULL;
    Tcl_Obj *c, *res;

    if (lt == NULL) {
	return NULL;
    }
    if (TclAnnotInfo(col, NULL, &at, NULL, NULL) == 1 && at != NULL
	    && (at == lt || TclTypeEnters(lt, at))) {
	return col;
    }
    c = TclTypedConform(interp, lt, col);
    if (c == NULL) {
	Tcl_Obj *m = Tcl_ObjPrintf("type map: %s: %s", what,
		Tcl_GetString(Tcl_GetObjResult(interp)));

	Tcl_SetObjResult(interp, m);
	return NULL;
    }
    Tcl_IncrRefCount(c);
    res = TclNewAnnotated(interp, lt, -1, c);
    Tcl_DecrRefCount(c);
    return res;
}

/*
 * v17y : le relevé de type map (où est la falaise). Chaque appel au niveau
 * de l'utilisateur (type map, une boucle vectorisée) peut tenir un relevé :
 * les voies rapides y notent ce qu'elles ont fait, ou pourquoi elles se
 * sont retirées. type map -report varName ... le rend pour un appel ;
 * ::tcl::unsupported::mapreport on|off|clear|get l'agrège par lieu
 * d'appel. Sans relevé demandé : un pointeur NULL, rien d'autre.
 */
typedef struct MapRep {
    int depth;			/* l'appel de l'utilisateur qui le tient */
    int comp;			/* la composante en cours (-1 : le corps) */
    int ncomp;
    Tcl_Size elements;
    Tcl_Obj *inputs;		/* nom genre ... */
    Tcl_Obj *body;		/* une description par composante */
    const char *pass;
    Tcl_Obj *why;		/* pourquoi la voie rapide n'a pas servi */
    Tcl_WideInt blocks, elementwise, expr;
    Tcl_Obj *write;
} MapRep;

typedef struct MapRepTsd {
    MapRep *rep;
    int depth;
} MapRepTsd;
static Tcl_ThreadDataKey mapRepKey;

/* le relevé de l'appel en cours, s'il en tient un (pas celui d'un appel
 * imbriqué, par expr par exemple) */
static MapRep *
CurRep(void)
{
    MapRepTsd *t = (MapRepTsd *) Tcl_GetThreadData(&mapRepKey,
	    sizeof(MapRepTsd));

    return (t->rep != NULL && t->rep->depth == t->depth) ? t->rep : NULL;
}

static void
RepSetObj(Tcl_Obj **slot, Tcl_Obj *o)
{
    if (o != NULL) {
	Tcl_IncrRefCount(o);
    }
    if (*slot != NULL) {
	Tcl_DecrRefCount(*slot);
    }
    *slot = o;
}

/* pourquoi (la première raison seulement : la plus proche de la cause) */
static void
RepWhy(MapRep *r, Tcl_Obj *why)
{
    if (r != NULL && r->why == NULL) {
	RepSetObj(&r->why, why);
    } else {
	Tcl_IncrRefCount(why);
	Tcl_DecrRefCount(why);
    }
}

/* la description de la composante en cours (ou du corps entier) */
static void
RepBody(MapRep *r, Tcl_Obj *desc)
{
    Tcl_Size n, c = (r->comp < 0) ? 0 : r->comp;

    if (r->body == NULL) {
	RepSetObj(&r->body, Tcl_NewListObj(0, NULL));
    }
    if (Tcl_IsShared(r->body)) {
	RepSetObj(&r->body, Tcl_DuplicateObj(r->body));
    }
    Tcl_ListObjLength(NULL, r->body, &n);
    while (n <= c) {
	Tcl_ListObjAppendElement(NULL, r->body, Tcl_NewObj());
	n++;
    }
    Tcl_ListObjReplace(NULL, r->body, c, 1, 1, &desc);
}

/* « not compiled near "…" » : là où la lecture s'est arrêtée */
static Tcl_Obj *
RepNotCompiled(const MProg *g, const char *text)
{
    const char *at = g->badAt;

    if (at == NULL) {
	return Tcl_NewStringObj("expr: typed operations are off", -1);
    }
    if (at < text || at > text + strlen(text)) {
	at = text;
    }
    while (*at == ' ' || *at == '\t' || *at == '\n') {
	at++;
    }
    if (*at == '\0') {
	return Tcl_NewStringObj("expr: not compiled (unbalanced)", -1);
    }
    return Tcl_ObjPrintf("expr: not compiled near \"%.24s%s\"", at,
	    strlen(at) > 24 ? "..." : "");
}

static void
MapRepFree(MapRep *r)
{
    RepSetObj(&r->inputs, NULL);
    RepSetObj(&r->body, NULL);
    RepSetObj(&r->why, NULL);
    RepSetObj(&r->write, NULL);
}

/* le relevé en dictionnaire */
static Tcl_Obj *
MapRepDict(const MapRep *r)
{
    Tcl_Obj *d = Tcl_NewDictObj();

#define RD_PUT(k, v) Tcl_DictObjPut(NULL, d, Tcl_NewStringObj(k, -1), (v))
    RD_PUT("elements", Tcl_NewWideIntObj(r->elements));
    RD_PUT("inputs", r->inputs ? r->inputs : Tcl_NewObj());
    RD_PUT("body", r->body ? r->body : Tcl_NewObj());
    RD_PUT("pass", Tcl_NewStringObj(r->pass ? r->pass : "none", -1));
    RD_PUT("blocks", Tcl_NewWideIntObj(r->blocks));
    RD_PUT("elementwise", Tcl_NewWideIntObj(r->elementwise));
    RD_PUT("expr", Tcl_NewWideIntObj(r->expr));
    if (r->write != NULL) {
	RD_PUT("write", r->write);
    }
    RD_PUT("why", r->why ? r->why : Tcl_NewObj());
#undef RD_PUT
    return d;
}

/* le genre d'une colonne d'entrée */
static Tcl_Obj *
RepInputKind(const unsigned char *cdata, int ckind, int csize, size_t cstride,
	Tcl_Size estride, int isView)
{
    if (cdata != NULL) {
	return Tcl_ObjPrintf("cells %s(%d) stride %d", ckind == 2 ? "double"
		: ckind == 1 ? "int u" : "int s", csize * 8, (int) cstride);
    }
    if (isView) {
	return Tcl_ObjPrintf("objects view stride %d", (int) estride);
    }
    return Tcl_NewStringObj("objects", -1);
}

/*
 * Le relevé global, par interpréteur : un dictionnaire par lieu d'appel
 * (procédure et ligne, ou fichier et ligne, ou le texte de la commande).
 */
typedef struct MapLog {
    int on;
    Tcl_Obj *d;			/* lieu -> statistiques */
} MapLog;

/* des relevés globaux en marche (dans un interpréteur au moins) : sans
 * eux, l'entrée ne cherche même pas la donnée de l'interpréteur */
static int mapLogAny = 0;

static void
MapLogDelete(void *cd, TCL_UNUSED(Tcl_Interp *))
{
    MapLog *l = (MapLog *) cd;

    if (l->on) {
	mapLogAny--;
    }
    if (l->d != NULL) {
	Tcl_DecrRefCount(l->d);
    }
    Tcl_Free(l);
}

static MapLog *
GetMapLog(Tcl_Interp *interp)
{
    MapLog *l = (MapLog *) Tcl_GetAssocData(interp, "tclMapReport", NULL);

    if (l == NULL) {
	l = (MapLog *) Tcl_Alloc(sizeof(MapLog));
	l->on = 0;
	l->d = Tcl_NewDictObj();
	Tcl_IncrRefCount(l->d);
	Tcl_SetAssocData(interp, "tclMapReport", MapLogDelete, l);
    }
    return l;
}

/* la valeur d'une clé (NULL si absente) */
static Tcl_Obj *
MapDictGet(Tcl_Obj *d, const char *key)
{
    Tcl_Obj *k = Tcl_NewStringObj(key, -1), *v = NULL;

    Tcl_IncrRefCount(k);
    if (Tcl_DictObjGet(NULL, d, k, &v) != TCL_OK) {
	v = NULL;
    }
    Tcl_DecrRefCount(k);
    return v;
}

/* le lieu de l'appel en cours (le cadre de la commande appelante) */
static Tcl_Obj *
MapLogWhere(Tcl_Interp *interp)
{
    Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);
    Tcl_Obj *where = NULL, *fr;

    if (Tcl_EvalEx(interp, "::info frame [expr {[::info frame] - 1}]", -1,
	    TCL_EVAL_GLOBAL) == TCL_OK) {
	Tcl_Obj *proc = NULL, *file = NULL, *line = NULL, *cmd = NULL;

	fr = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(fr);
	proc = MapDictGet(fr, "proc");
	file = MapDictGet(fr, "file");
	line = MapDictGet(fr, "line");
	cmd = MapDictGet(fr, "cmd");
	if (file != NULL && line != NULL) {
	    where = Tcl_ObjPrintf("%s:%s", Tcl_GetString(file),
		    Tcl_GetString(line));
	    if (proc != NULL) {
		Tcl_AppendPrintfToObj(where, " (%s)", Tcl_GetString(proc));
	    }
	} else if (proc != NULL && line != NULL) {
	    where = Tcl_ObjPrintf("%s line %s", Tcl_GetString(proc),
		    Tcl_GetString(line));
	} else if (cmd != NULL) {
	    const char *c = Tcl_GetString(cmd);

	    where = Tcl_ObjPrintf("%.60s%s", c, strlen(c) > 60 ? "..." : "");
	}
	Tcl_DecrRefCount(fr);
    }
    Tcl_RestoreInterpState(interp, st);
    return where ? where : Tcl_NewStringObj("?", -1);
}

static void
MapLogIncr(Tcl_Obj *d, const char *key, Tcl_WideInt by)
{
    Tcl_Obj *k = Tcl_NewStringObj(key, -1), *v;
    Tcl_WideInt w = 0;

    Tcl_IncrRefCount(k);
    if (Tcl_DictObjGet(NULL, d, k, &v) == TCL_OK && v != NULL) {
	Tcl_GetWideIntFromObj(NULL, v, &w);
    }
    Tcl_DictObjPut(NULL, d, k, Tcl_NewWideIntObj(w + by));
    Tcl_DecrRefCount(k);
}

/* un appel de plus au lieu de l'appel : what (type map, foreach, lmap) */
static void
MapLogAdd(Tcl_Interp *interp, MapLog *l, const char *what, const MapRep *r,
	const char *declined)
{
    Tcl_Obj *where = MapLogWhere(interp), *e, *k, *v;

    Tcl_IncrRefCount(where);
    if (Tcl_IsShared(l->d)) {
	Tcl_DecrRefCount(l->d);
	l->d = Tcl_DuplicateObj(l->d);
	Tcl_IncrRefCount(l->d);
    }
    if (Tcl_DictObjGet(NULL, l->d, where, &e) != TCL_OK || e == NULL) {
	e = Tcl_NewDictObj();
	Tcl_DictObjPut(NULL, e, Tcl_NewStringObj("command", -1),
		Tcl_NewStringObj(what, -1));
    } else {
	e = Tcl_DuplicateObj(e);
    }
    MapLogIncr(e, "calls", 1);
    if (declined != NULL) {
	k = Tcl_ObjPrintf("declined: %s", declined);
	Tcl_IncrRefCount(k);
	MapLogIncr(e, Tcl_GetString(k), 1);
	Tcl_DecrRefCount(k);
    } else if (r != NULL) {
	Tcl_Obj *pk = Tcl_ObjPrintf("pass %s", r->pass ? r->pass : "none");

	Tcl_IncrRefCount(pk);
	MapLogIncr(e, Tcl_GetString(pk), 1);
	Tcl_DecrRefCount(pk);
	MapLogIncr(e, "elements", r->elements);
	MapLogIncr(e, "blocks", r->blocks);
	MapLogIncr(e, "elementwise", r->elementwise);
	MapLogIncr(e, "expr", r->expr);
	if (r->write != NULL) {
	    Tcl_Obj *wk = Tcl_ObjPrintf("write %s", Tcl_GetString(r->write));

	    Tcl_IncrRefCount(wk);
	    MapLogIncr(e, Tcl_GetString(wk), 1);
	    Tcl_DecrRefCount(wk);
	}
	k = Tcl_NewStringObj("body", -1);
	if (r->body != NULL && (Tcl_DictObjGet(NULL, e, k, &v) != TCL_OK
		|| v == NULL)) {
	    Tcl_DictObjPut(NULL, e, k, r->body);
	} else {
	    TclDiscard(k);
	}
	if (r->why != NULL) {
	    /* la première raison vue à ce lieu */
	    k = Tcl_NewStringObj("why", -1);
	    if (Tcl_DictObjGet(NULL, e, k, &v) != TCL_OK || v == NULL) {
		Tcl_DictObjPut(NULL, e, k, r->why);
	    } else {
		TclDiscard(k);
	    }
	}
    }
    Tcl_DictObjPut(NULL, l->d, where, e);
    Tcl_DecrRefCount(where);
}

/* l'entrée d'un appel de l'utilisateur ; want : un relevé demandé */
static MapLog *
MapRepEnter(Tcl_Interp *interp, MapRep *r, MapRep **savePtr, int want)
{
    MapRepTsd *t = (MapRepTsd *) Tcl_GetThreadData(&mapRepKey,
	    sizeof(MapRepTsd));
    MapLog *l = mapLogAny ? (MapLog *) Tcl_GetAssocData(interp,
	    "tclMapReport", NULL) : NULL;

    t->depth++;
    *savePtr = t->rep;
    if (want || (l != NULL && l->on)) {
	memset(r, 0, sizeof(MapRep));
	r->depth = t->depth;
	r->comp = -1;
	t->rep = r;
    }
    return (l != NULL && l->on) ? l : NULL;
}

static void
MapRepLeave(MapRep *save)
{
    MapRepTsd *t = (MapRepTsd *) Tcl_GetThreadData(&mapRepKey,
	    sizeof(MapRepTsd));

    t->depth--;
    t->rep = save;
}

static int TypeMapCmd(Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[]);

/*
 * type map ?-report varName? ... : l'appel de l'utilisateur. Le relevé est
 * rangé dans varName même si la commande échoue (il dit alors où).
 */
static int
TypeMapUser(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    MapRep rep, *save;
    MapLog *l;
    Tcl_Obj *repVar = NULL, **av = (Tcl_Obj **) objv;
    Tcl_Size ac = objc;
    int rc;

    if (objc > 2 && strcmp(Tcl_GetString(objv[2]), "-report") == 0) {
	if (objc < 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "-report varName ?-in target|"
		    "-sum|-min|-max|-fold {name init}? varList list ?varList "
		    "list ...? {(expression)}");
	    return TCL_ERROR;
	}
	repVar = objv[3];
	av = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) objc);
	av[0] = objv[0];
	av[1] = objv[1];
	memcpy(av + 2, objv + 4, sizeof(Tcl_Obj *) * (size_t) (objc - 4));
	ac = objc - 2;
    }
    l = MapRepEnter(interp, &rep, &save, repVar != NULL);
    if (repVar == NULL && l == NULL) {
	rc = TypeMapCmd(interp, ac, av);
	MapRepLeave(save);
	return rc;
    }
    rc = TypeMapCmd(interp, ac, av);
    MapRepLeave(save);
    if (rc != TCL_OK && rep.why == NULL) {
	RepSetObj(&rep.why, Tcl_ObjPrintf("error: %s",
		Tcl_GetString(Tcl_GetObjResult(interp))));
    }
    if (l != NULL) {
	MapLogAdd(interp, l, "type map", &rep, NULL);
    }
    if (repVar != NULL) {
	Tcl_Obj *d = MapRepDict(&rep);

	Tcl_IncrRefCount(d);
	if (rc == TCL_OK) {
	    Tcl_Obj *res = Tcl_GetObjResult(interp);

	    Tcl_IncrRefCount(res);
	    if (Tcl_ObjSetVar2(interp, repVar, NULL, d, TCL_LEAVE_ERR_MSG)
		    == NULL) {
		rc = TCL_ERROR;
	    } else {
		Tcl_SetObjResult(interp, res);
	    }
	    Tcl_DecrRefCount(res);
	} else {
	    Tcl_InterpState st = Tcl_SaveInterpState(interp, rc);

	    Tcl_ObjSetVar2(interp, repVar, NULL, d, 0);
	    rc = Tcl_RestoreInterpState(interp, st);
	}
	Tcl_DecrRefCount(d);
	Tcl_Free(av);
    }
    MapRepFree(&rep);
    return rc;
}

/* ::tcl::unsupported::mapreport on|off|clear|get */
static int
MapReportObjCmd(TCL_UNUSED(void *), Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    static const char *const opts[] = {"on", "off", "clear", "get", NULL};
    int idx;
    MapLog *l;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "on|off|clear|get");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], opts, "option", 0, &idx)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    l = GetMapLog(interp);
    switch (idx) {
    case 0:
	mapLogAny += !l->on;
	l->on = 1;
	break;
    case 1:
	mapLogAny -= l->on;
	l->on = 0;
	break;
    case 2:
	Tcl_DecrRefCount(l->d);
	l->d = Tcl_NewDictObj();
	Tcl_IncrRefCount(l->d);
	break;
    default:
	Tcl_SetObjResult(interp, l->d);
	break;
    }
    return TCL_OK;
}

static int MapTupleFast(Tcl_Interp *interp, Tcl_Size ac,
	Tcl_Obj *const av[], Tcl_Obj *const parts[], Tcl_Size K,
	Tcl_Obj **outCols, Tcl_Obj *sinkName, const char *sinkPath);
static int TypeMapPlain(Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[]);

static int
TypeMapTyped(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[],
	int optc)
{
    Tcl_Obj **av = NULL, *names = NULL, *keep = NULL, *body = objv[objc - 1];
    Tcl_Obj *content = NULL, *parts = NULL, *res = NULL, *cols = NULL;
    TclType *outT = NULL;
    Tcl_Size k, j, nf, K = 1, ac;
    int inl = 0, err = 0, annotated, rc = TCL_ERROR;

    annotated = MapBodyType(interp, body, &outT, &content, &inl, &err);
    if (err) {
	return TCL_ERROR;
    }
    keep = Tcl_NewListObj(0, NULL);	/* les objets faits ici */
    Tcl_IncrRefCount(keep);
    names = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(names);
    av = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) (objc + 1));
    av[0] = objv[0];
    av[1] = objv[1];
    ac = 2;
    for (k = 0; k < optc; k++) {
	av[ac++] = objv[2 + k];
    }
    /* les entrées */
    for (k = 2 + optc; k < objc - 1; k += 2) {
	Tcl_Obj *spec = objv[k], *data = objv[k + 1];

	if (!MapSpecTyped(spec)) {
	    Tcl_Obj **vv;
	    Tcl_Size nv;

	    if (Tcl_ListObjGetElements(interp, spec, &nv, &vv) != TCL_OK) {
		goto done;
	    }
	    for (j = 0; j < nv; j++) {
		Tcl_ListObjAppendElement(NULL, names, vv[j]);
	    }
	    av[ac++] = spec;
	    av[ac++] = data;
	} else {
	    const char *st = Tcl_GetString(spec);
	    TclType *pt = TclTypeParse(interp, st, (Tcl_Size) strlen(st));
	    Tcl_Obj *vl, *dl, **cv = NULL;
	    Tcl_Size nc = 1;

	    if (pt == NULL) {
		goto done;
	    }
	    if (pt->kind != TCLT_PRODUCT || pt->nfields < 1) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map: \"%s\" "
			"is not a list of typed variables ({T}a; {T}b ...)",
			st));
		goto done;
	    }
	    nf = pt->nfields;
	    if (nf > 1) {
		if (Tcl_ListObjGetElements(interp, MListContent(data), &nc,
			&cv) != TCL_OK) {
		    goto done;
		}
		if (nc != nf) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map: {%s} "
			    "needs a list of %" TCL_SIZE_MODIFIER "d columns, "
			    "not %" TCL_SIZE_MODIFIER "d", st, nf, nc));
		    goto done;
		}
	    }
	    vl = Tcl_NewListObj(0, NULL);
	    dl = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(NULL, keep, vl);
	    Tcl_ListObjAppendElement(NULL, keep, dl);
	    for (j = 0; j < nf; j++) {
		Tcl_Obj *fname = pt->fields[j].name, *c;
		char what[200];

		if (fname == NULL) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map: "
			    "every variable of {%s} needs a name", st));
		    goto done;
		}
		snprintf(what, sizeof(what), "column \"%s\"",
			Tcl_GetString(fname));
		c = MapColumnAs(interp, pt->fields[j].type,
			(nf > 1) ? cv[j] : data, what);
		if (c == NULL) {
		    goto done;
		}
		Tcl_ListObjAppendElement(NULL, vl, fname);
		Tcl_ListObjAppendElement(NULL, dl, c);
		Tcl_ListObjAppendElement(NULL, names, fname);
	    }
	    av[ac++] = vl;
	    if (nf == 1) {
		Tcl_Obj *one;

		Tcl_ListObjIndex(NULL, dl, 0, &one);
		av[ac++] = one;
	    } else {
		av[ac++] = dl;
	    }
	}
    }
    /* le corps */
    if (!annotated) {
	av[ac++] = body;
	rc = TypeMapPlain(interp, ac, av);
	goto done;
    }
    if (outT->kind == TCLT_PRODUCT) {
	K = outT->nfields;
    }
    if (Tcl_GetCharLength(content) == 0) {
	/* la projection : les champs de la sortie, par leurs noms */
	Tcl_Obj *b = Tcl_NewStringObj("(", 1);

	Tcl_ListObjAppendElement(NULL, keep, b);
	if (outT->kind != TCLT_PRODUCT) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("type map: an empty "
		    "body projects onto a product: {{T}a; {T}b}{}", -1));
	    goto done;
	}
	for (j = 0; j < K; j++) {
	    Tcl_Obj *fn = outT->fields[j].name, **nv;
	    Tcl_Size nn, q;
	    int found = 0;

	    Tcl_ListObjGetElements(NULL, names, &nn, &nv);
	    for (q = 0; fn != NULL && q < nn; q++) {
		found |= (strcmp(Tcl_GetString(nv[q]), Tcl_GetString(fn)) == 0);
	    }
	    if (!found) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map: projection: "
			"no input variable \"%s\"",
			fn ? Tcl_GetString(fn) : "?"));
		goto done;
	    }
	    Tcl_AppendPrintfToObj(b, "%s$%s", j ? ", " : "", Tcl_GetString(fn));
	}
	Tcl_AppendToObj(b, ")", 1);
	content = b;
    }
    if (K == 1 && !(outT->kind == TCLT_PRODUCT)) {
	/* un scalaire : une colonne de T (ou, avec -sum..., un T) */
	Tcl_Obj *r;

	av[ac++] = content;
	if (TypeMapPlain(interp, ac, av) != TCL_OK) {
	    goto done;
	}
	r = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(r);
	Tcl_ResetResult(interp);
	if (optc) {
	    Tcl_Obj *c = TclTypedConform(interp, outT, r);

	    if (c == NULL) {
		Tcl_DecrRefCount(r);
		goto done;
	    }
	    Tcl_IncrRefCount(c);
	    res = TclNewAnnotated(interp, outT, -1, c);
	    Tcl_DecrRefCount(c);
	} else {
	    res = MapColumnAs(interp, outT, r, "result");
	}
	if (res == NULL) {
	    Tcl_DecrRefCount(r);
	    goto done;
	}
	Tcl_IncrRefCount(res);		/* (peut être r lui-même) */
	Tcl_DecrRefCount(r);
	Tcl_SetObjResult(interp, res);
	Tcl_DecrRefCount(res);
	res = NULL;
	rc = TCL_OK;
	goto done;
    }
    /* un produit : K colonnes */
    if (optc) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map: %s needs a "
		"scalar result, not a product", Tcl_GetString(objv[2])));
	goto done;
    }
    cols = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(cols);
    parts = MapTupleSplit(Tcl_GetString(content), Tcl_GetCharLength(content)
	    ? (Tcl_Size) strlen(Tcl_GetString(content)) : 0);
    if (parts != NULL) {
	Tcl_Obj **pv;
	Tcl_Size np;

	Tcl_IncrRefCount(parts);
	Tcl_ListObjGetElements(NULL, parts, &np, &pv);
	Tcl_Obj *fo[64];

	if (np == K && K <= 64
		&& MapTupleFast(interp, ac, av, pv, K, fo, NULL, NULL)) {
	    /* v17x : les K composantes en une passe */
	    for (j = 0; j < K; j++) {
		Tcl_ListObjAppendElement(NULL, cols, fo[j]);
		Tcl_DecrRefCount(fo[j]);
	    }
	} else if (np == K) {
	    /* une composante à la fois, chacune compilée (et par blocs) */
	    MapRep *rp = CurRep();

	    if (rp != NULL) {
		rp->pass = "per-component";
	    }
	    for (j = 0; j < K; j++) {
		av[ac] = pv[j];
		if (rp != NULL) {
		    rp->comp = (int) j;
		}
		if (TypeMapPlain(interp, ac + 1, av) != TCL_OK) {
		    break;
		}
		Tcl_ListObjAppendElement(NULL, cols, Tcl_GetObjResult(interp));
		Tcl_ResetResult(interp);
	    }
	    if (j < K) {
		/* l'erreur telle que l'élément par élément la donne */
		Tcl_ResetResult(interp);
		Tcl_SetListObj(cols, 0, NULL);
	    }
	}
    }
    {
	Tcl_Size nc;

	Tcl_ListObjLength(NULL, cols, &nc);
	if (nc != K) {
	    /* le corps entier, élément par élément, puis les colonnes */
	    Tcl_Obj *r, **rv;
	    Tcl_Size nr, i;
	    MapRep *rp = CurRep();

	    if (rp != NULL) {
		rp->comp = -1;
		rp->pass = "tuple by expr";
		RepSetObj(&rp->body, NULL);
		rp->blocks = rp->elementwise = rp->expr = 0;
		RepWhy(rp, Tcl_NewStringObj("tuple: the components do not "
			"split or compile, by expr", -1));
	    }
	    av[ac] = content;
	    if (TypeMapPlain(interp, ac + 1, av) != TCL_OK) {
		goto done;
	    }
	    r = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(r);
	    Tcl_ResetResult(interp);
	    if (Tcl_ListObjGetElements(interp, r, &nr, &rv) != TCL_OK) {
		Tcl_DecrRefCount(r);
		goto done;
	    }
	    for (j = 0; j < K; j++) {
		Tcl_ListObjAppendElement(NULL, cols, Tcl_NewListObj(0, NULL));
	    }
	    for (i = 0; i < nr; i++) {
		Tcl_Obj **tv, *cj;
		Tcl_Size nt;

		if (Tcl_ListObjGetElements(interp, rv[i], &nt, &tv) != TCL_OK
			|| nt != K) {
		    if (nt != K) {
			Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map: the "
				"body gives %" TCL_SIZE_MODIFIER "d values for "
				"element %" TCL_SIZE_MODIFIER "d, %s needs %"
				TCL_SIZE_MODIFIER "d", nt, i, outT->name, K));
		    }
		    Tcl_DecrRefCount(r);
		    goto done;
		}
		for (j = 0; j < K; j++) {
		    Tcl_ListObjIndex(NULL, cols, j, &cj);
		    if (Tcl_IsShared(cj)) {
			cj = Tcl_DuplicateObj(cj);
			Tcl_ListObjReplace(NULL, cols, j, 1, 1, &cj);
		    }
		    Tcl_ListObjAppendElement(NULL, cj, tv[j]);
		}
	    }
	    Tcl_DecrRefCount(r);
	}
    }
    /* chaque colonne dans le type de son champ */
    {
	Tcl_Obj **cv, *out = Tcl_NewListObj(0, NULL);

	Tcl_IncrRefCount(out);
	Tcl_ListObjGetElements(NULL, cols, &nf, &cv);
	for (j = 0; j < K; j++) {
	    char what[200];
	    Tcl_Obj *c;

	    snprintf(what, sizeof(what), "result \"%s\"",
		    outT->fields[j].name ? Tcl_GetString(outT->fields[j].name)
		    : "?");
	    c = MapColumnAs(interp, outT->fields[j].type, cv[j], what);
	    if (c == NULL) {
		Tcl_DecrRefCount(out);
		goto done;
	    }
	    Tcl_ListObjAppendElement(NULL, out, c);
	}
	if (inl) {
	    res = out;		/* une colonne par champ */
	} else {
	    /* un produit nommé : la liste de ses valeurs */
	    Tcl_Obj **ov, *rows = Tcl_NewListObj(0, NULL), *c;
	    Tcl_Size n = 0, i;
	    TclType *lt = TclTypeListOf(interp, outT);

	    Tcl_IncrRefCount(rows);
	    Tcl_ListObjGetElements(NULL, out, &nf, &ov);
	    Tcl_ListObjLength(NULL, ov[0], &n);
	    for (i = 0; i < n; i++) {
		Tcl_Obj *row = Tcl_NewListObj(0, NULL), *e;

		for (j = 0; j < K; j++) {
		    Tcl_ListObjIndex(NULL, ov[j], i, &e);
		    Tcl_ListObjAppendElement(NULL, row, e);
		}
		Tcl_ListObjAppendElement(NULL, rows, row);
	    }
	    Tcl_DecrRefCount(out);
	    c = (lt != NULL) ? TclTypedConform(interp, lt, rows) : NULL;
	    Tcl_DecrRefCount(rows);
	    if (c == NULL) {
		goto done;
	    }
	    Tcl_IncrRefCount(c);
	    res = TclNewAnnotated(interp, lt, -1, c);
	    Tcl_DecrRefCount(c);
	    if (res == NULL) {
		goto done;
	    }
	    Tcl_IncrRefCount(res);
	    out = res;
	}
	Tcl_SetObjResult(interp, out);
	Tcl_DecrRefCount(out);
	res = NULL;
	rc = TCL_OK;
    }
  done:
    if (parts != NULL) {
	Tcl_DecrRefCount(parts);
    }
    if (cols != NULL) {
	Tcl_DecrRefCount(cols);
    }
    Tcl_DecrRefCount(names);
    Tcl_DecrRefCount(keep);
    Tcl_Free(av);
    return rc;
}

/*
 * type map -in VAR(CHEMIN) varList ?varList liste ...? corps (v17w)
 *
 * Calcule comme type map, la première varList liée à ce que désigne le
 * chemin dans la variable, et écrit le résultat au même endroit : une
 * valeur par colonne du chemin (un tuple pour une fenêtre). Le type de la
 * sortie est celui de la cible ; une annotation du corps est vérifiée une
 * fois (elle doit y entrer). Tout ou rien : les colonnes sont calculées,
 * puis écrites d'un coup par le chemin. Rend la nouvelle valeur.
 */
static int TypeMapCmd(Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[]);

static TclType *
MapInElemType(Tcl_Obj *col)
{
    TclType *tp = NULL;

    if (TclAnnotInfo(col, NULL, &tp, NULL, NULL) != 1 || tp == NULL) {
	return NULL;
    }
    while (tp->kind != TCLT_LIST && tp->under != NULL) {
	tp = tp->under;
    }
    return (tp->kind == TCLT_LIST) ? tp->elem : NULL;
}

/* Des éléments qui ne vérifient rien de plus qu'un nombre : int ou double
 * (ou un nominal sur eux, sans règle de validation), sans prédicat. */
static int
MapInPlainCells(Tcl_Interp *interp, TclType *const *tt, Tcl_Size K)
{
    Tcl_Size j;

    for (j = 0; j < K; j++) {
	TclType *t = tt[j];

	if (t == NULL || (t->kind != TCLT_INT && t->kind != TCLT_DOUBLE)
		|| t->pred != NULL || t->binLink != NULL || t->bitWidth > 0
		|| (t->nominal && t->under != NULL
		    && FindRule(GetData(interp), t->under->name, t->name))) {
	    return 0;
	}
    }
    return 1;
}

static int
TypeMapIn(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *target, *nameObj = NULL, *pathObj = NULL, *cur = NULL;
    Tcl_Obj *body, *content = NULL, *cols = NULL, **av = NULL, **cv;
    Tcl_Obj *parts = NULL, **wv = NULL;
    TclType *outT = NULL, *tt[64];
    Tcl_Size K, j, ac, tlen, nc;
    const char *t, *par;
    int annotated, inl = 0, err = 0, rc = TCL_ERROR;

    if (objc < 6 || (objc % 2) != 0) {
	Tcl_WrongNumArgs(interp, 2, objv, "-in varName(path) varList "
		"?varList list ...? body");
	return TCL_ERROR;
    }
    target = objv[3];
    body = objv[objc - 1];
    t = Tcl_GetStringFromObj(target, &tlen);
    par = (const char *) memchr(t, '(', (size_t) tlen);
    if (par != NULL && tlen > 0 && t[tlen - 1] == ')' && par > t) {
	nameObj = Tcl_NewStringObj(t, par - t);
	pathObj = Tcl_NewStringObj(par + 1, tlen - (par - t) - 2);
    } else {
	nameObj = Tcl_NewStringObj(t, tlen);
    }
    Tcl_IncrRefCount(nameObj);
    if (pathObj != NULL) {
	Tcl_IncrRefCount(pathObj);
    }
    cur = Tcl_ObjGetVar2(interp, nameObj, pathObj, TCL_LEAVE_ERR_MSG);
    if (cur == NULL) {
	goto done;
    }
    Tcl_IncrRefCount(cur);

    /* le nombre de colonnes : les variables de la première varList */
    if (MapSpecTyped(objv[4])) {
	const char *st = Tcl_GetString(objv[4]);
	TclType *pt = TclTypeParse(interp, st, (Tcl_Size) strlen(st));

	if (pt == NULL) {
	    goto done;
	}
	K = (pt->kind == TCLT_PRODUCT) ? pt->nfields : 1;
    } else if (Tcl_ListObjLength(interp, objv[4], &K) != TCL_OK) {
	goto done;
    }
    if (K < 1 || K > 64) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("type map -in: the first "
		"varList names the columns of the target (1 to 64)", -1));
	goto done;
    }
    /* le type des éléments de chaque colonne de la cible (NULL : libre) */
    if (K == 1) {
	tt[0] = MapInElemType(cur);
    } else {
	if (Tcl_ListObjGetElements(interp, MListContent(cur), &nc, &cv)
		!= TCL_OK) {
	    goto done;
	}
	if (nc != K) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map -in: %s has %"
		    TCL_SIZE_MODIFIER "d columns, and the first varList "
		    "names %" TCL_SIZE_MODIFIER "d", t, nc, K));
	    goto done;
	}
	for (j = 0; j < K; j++) {
	    tt[j] = MapInElemType(cv[j]);
	}
    }

    /* une annotation du corps : vérifiée une fois contre la cible */
    annotated = MapBodyType(interp, body, &outT, &content, &inl, &err);
    if (err) {
	goto done;
    }
    if (annotated) {
	Tcl_Size Kd = (outT->kind == TCLT_PRODUCT) ? outT->nfields : 1;

	if (Kd != K) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map -in: the body "
		    "gives %s, %s needs %" TCL_SIZE_MODIFIER "d value%s",
		    outT->name, t, K, K > 1 ? "s" : ""));
	    goto done;
	}
	for (j = 0; j < K; j++) {
	    TclType *dj = (outT->kind == TCLT_PRODUCT)
		    ? outT->fields[j].type : outT;

	    if (tt[j] != NULL && dj != NULL && !TclTypeEnters(tt[j], dj)) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map -in: the "
			"body gives %s, which does not enter %s (column %"
			TCL_SIZE_MODIFIER "d of %s)", dj->name, tt[j]->name,
			j, t));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "MISMATCH",
			(char *) NULL);
		goto done;
	    }
	}
    }

    /* les colonnes, calculées comme type map */
    av = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) objc);
    av[0] = objv[0];
    av[1] = objv[1];
    av[2] = objv[4];
    av[3] = cur;
    ac = 4;
    for (j = 5; j < objc - 1; j++) {
	av[ac++] = objv[j];
    }
    cols = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(cols);
    {
	MapRep *rp = CurRep();

	if (rp != NULL) {
	    const char *w = (pathObj == NULL) ? "the whole variable (no path)"
		    : (TclTypeOpsMode() != TCL_TYPE_OPS_ON)
		    ? "typed operations are not on"
		    : !MapInPlainCells(interp, tt, K)
		    ? "the target elements are not plain numbers" : NULL;

	    for (j = 0; w == NULL && j < K; j++) {
		if (tt[j]->refine || tt[j]->kind != TCLT_DOUBLE) {
		    w = "the target elements are not double";
		}
	    }
	    if (w != NULL) {
		RepWhy(rp, Tcl_ObjPrintf("direct write: %s", w));
	    }
	}
    }
    if (pathObj != NULL && TclTypeOpsMode() == TCL_TYPE_OPS_ON
	    && MapInPlainCells(interp, tt, K)) {
	/* v17x : des doubles en cases ; calculés et écrits bloc par bloc,
	 * à leur pas, dans des cases neuves installées à la fin */
	int direct = 1;
	Tcl_Obj *txt = annotated ? content : body, *pl = NULL, **pv;
	Tcl_Size np;

	for (j = 0; j < K; j++) {
	    direct &= !tt[j]->refine && tt[j]->kind == TCLT_DOUBLE;
	}
	if (direct && annotated) {
	    /* une annotation de doubles seulement (sinon sa conversion) */
	    for (j = 0; j < K; j++) {
		TclType *dj = (outT->kind == TCLT_PRODUCT)
			? outT->fields[j].type : outT;

		direct &= (dj != NULL && dj->kind == TCLT_DOUBLE
			&& !dj->refine && MapInPlainCells(interp, &dj, 1));
	    }
	    if (!direct) {
		RepWhy(CurRep(), Tcl_NewStringObj("direct write: the body "
			"annotation is not all double", -1));
	    }
	}
	if (direct && txt != NULL) {
	    if (K == 1) {
		pl = Tcl_NewListObj(1, &txt);
	    } else {
		Tcl_Size bl;
		const char *bt = Tcl_GetStringFromObj(txt, &bl);

		pl = MapTupleSplit(bt, bl);
	    }
	}
	if (pl != NULL) {
	    Tcl_IncrRefCount(pl);
	    Tcl_ListObjGetElements(NULL, pl, &np, &pv);
	    if (np == K && MapTupleFast(interp, ac, av, pv, K, NULL, nameObj,
		    Tcl_GetString(pathObj))) {
		Tcl_Obj *nv = Tcl_ObjGetVar2(interp, nameObj, NULL,
			TCL_LEAVE_ERR_MSG);

		Tcl_DecrRefCount(pl);
		if (nv != NULL) {
		    Tcl_SetObjResult(interp, nv);
		    rc = TCL_OK;
		}
		goto done;
	    }
	    Tcl_DecrRefCount(pl);
	}
    }
    if (K == 1 || annotated) {
	av[ac] = body;
	if (TypeMapCmd(interp, ac + 1, av) != TCL_OK) {
	    goto done;
	}
	if (K == 1) {
	    Tcl_ListObjAppendElement(NULL, cols, Tcl_GetObjResult(interp));
	} else {
	    Tcl_SetListObj(cols, 0, NULL);
	    if (Tcl_ListObjGetElements(interp, Tcl_GetObjResult(interp), &nc,
		    &wv) != TCL_OK) {
		goto done;
	    }
	    Tcl_ListObjReplace(NULL, cols, 0, 0, nc, wv);
	}
	Tcl_ResetResult(interp);
    } else {
	/* un tuple sans annotation : une composante à la fois */
	Tcl_Obj **pv, *fo[64];
	Tcl_Size np, bl;
	const char *bt = Tcl_GetStringFromObj(body, &bl);

	parts = MapTupleSplit(bt, bl);
	if (parts == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map -in: %s has %"
		    TCL_SIZE_MODIFIER "d columns: the body must give as many "
		    "values, ($a, $b ...)", t, K));
	    goto done;
	}
	Tcl_IncrRefCount(parts);
	Tcl_ListObjGetElements(NULL, parts, &np, &pv);
	if (np != K) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map -in: the body "
		    "gives %" TCL_SIZE_MODIFIER "d value%s, %s needs %"
		    TCL_SIZE_MODIFIER "d", np, np > 1 ? "s" : "", t, K));
	    goto done;
	}
	if (MapTupleFast(interp, ac, av, pv, K, fo, NULL, NULL)) {
	    /* v17x : les K composantes en une passe */
	    for (j = 0; j < K; j++) {
		Tcl_ListObjAppendElement(NULL, cols, fo[j]);
		Tcl_DecrRefCount(fo[j]);
	    }
	} else for (j = 0; j < K; j++) {
	    MapRep *rp = CurRep();

	    if (rp != NULL) {
		rp->pass = "per-component";
		rp->comp = (int) j;
	    }
	    av[ac] = pv[j];
	    if (TypeMapCmd(interp, ac + 1, av) != TCL_OK) {
		goto done;
	    }
	    Tcl_ListObjAppendElement(NULL, cols, Tcl_GetObjResult(interp));
	    Tcl_ResetResult(interp);
	}
    }

    /* l'écriture, d'un coup, par le chemin (ou la variable entière) */
    {
	Tcl_Obj **sv = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *)
		* (size_t) (K + 2)), *setCmd = Tcl_NewStringObj("::set", -1);

	Tcl_Obj *held = Tcl_NewListObj(0, NULL);

	Tcl_IncrRefCount(setCmd);
	Tcl_IncrRefCount(held);
	Tcl_ListObjGetElements(NULL, cols, &nc, &cv);
	sv[0] = setCmd;
	sv[1] = target;
	for (j = 0; j < nc; j++) {
	    /* le contenu nu : c'est la cible qui le range dans son type
	     * (validation, cases, prédicats), comme une affectation ; des
	     * nombres sans autre règle : la colonne déjà dans le type des
	     * éléments (le chemin d'écriture n'a rien à revérifier) */
	    if (pathObj != NULL && j < K && MapInPlainCells(interp, &tt[j], 1)
		    && !tt[j]->refine) {
		Tcl_Obj *cj = MapColumnAs(interp, tt[j], cv[j], "result");

		if (cj == NULL) {
		    Tcl_DecrRefCount(setCmd);
		    Tcl_DecrRefCount(held);
		    Tcl_Free(sv);
		    goto done;
		}
		Tcl_ListObjAppendElement(NULL, held, cj);
		sv[2 + j] = cj;
	    } else {
		sv[2 + j] = TCL_ANNOT_CONTENT(cv[j]);
	    }
	}
	cv = sv + 2;
	if (pathObj != NULL && MapInPlainCells(interp, tt, K)
		&& TclTypeOpsMode() == TCL_TYPE_OPS_ON
		&& TclTypePathWriteCellColumns(interp, nameObj,
			Tcl_GetString(pathObj), nc, cv)) {
	    MapRep *rp = CurRep();

	    rc = TCL_OK;		/* écrit dans les cases */
	    if (rp != NULL) {
		Tcl_Obj *now = Tcl_ObjGetVar2(interp, nameObj, NULL, 0);

		RepSetObj(&rp->write, Tcl_NewStringObj((now != NULL
			&& TclIsCompact(MListContent(now)))
			? "columns into cells" : "columns into objects", -1));
	    }
	} else if (pathObj == NULL && nc == 1) {
	    rc = (Tcl_ObjSetVar2(interp, nameObj, NULL, cv[0],
		    TCL_LEAVE_ERR_MSG) != NULL) ? TCL_OK : TCL_ERROR;
	    if (CurRep() != NULL) {
		RepSetObj(&CurRep()->write, Tcl_NewStringObj("set (the "
			"whole variable)", -1));
	    }
	} else {
	    rc = Tcl_EvalObjv(interp, (int) (nc + 2), sv, 0);
	    if (CurRep() != NULL) {
		RepSetObj(&CurRep()->write, Tcl_NewStringObj("set (by the "
			"path, each value checked)", -1));
	    }
	}
	Tcl_DecrRefCount(setCmd);
	Tcl_Free(sv);
	Tcl_DecrRefCount(held);
	if (rc == TCL_OK) {
	    Tcl_Obj *nv = Tcl_ObjGetVar2(interp, nameObj, NULL,
		    TCL_LEAVE_ERR_MSG);

	    if (nv == NULL) {
		rc = TCL_ERROR;
	    } else {
		Tcl_SetObjResult(interp, nv);
	    }
	}
    }

  done:
    if (parts != NULL) {
	Tcl_DecrRefCount(parts);
    }
    if (cols != NULL) {
	Tcl_DecrRefCount(cols);
    }
    if (av != NULL) {
	Tcl_Free(av);
    }
    if (cur != NULL) {
	Tcl_DecrRefCount(cur);
    }
    Tcl_DecrRefCount(nameObj);
    if (pathObj != NULL) {
	Tcl_DecrRefCount(pathObj);
    }
    return rc;
}

static int
TypeMapCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const char *opt = (objc > 2) ? Tcl_GetString(objv[2]) : "";
    int optc = (strcmp(opt, "-sum") == 0 || strcmp(opt, "-min") == 0
	    || strcmp(opt, "-max") == 0) ? 1 : (strcmp(opt, "-fold") == 0
	    && objc > 3) ? 2 : 0;
    Tcl_Size k;

    if (strcmp(opt, "-in") == 0) {
	return TypeMapIn(interp, objc, objv);	/* v17w */
    }
    if (objc >= 4 + optc) {
	int typed = (TclAnnotInfo(objv[objc - 1], NULL, NULL, NULL, NULL)
		!= 0);

	for (k = 2 + optc; !typed && k < objc - 1; k += 2) {
	    typed = MapSpecTyped(objv[k]);
	}
	if (typed) {
	    return TypeMapTyped(interp, objc, objv, optc);
	}
    }
    return TypeMapPlain(interp, objc, objv);
}

static int
TypeMapPlain(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const char *opt = (objc > 2) ? Tcl_GetString(objv[2]) : "";

    if (strcmp(opt, "-sum") == 0 || strcmp(opt, "-min") == 0
	    || strcmp(opt, "-max") == 0) {
	Tcl_Obj **av = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *)
		* (size_t) objc), *r;
	int rc;
	MapFold cells;

	if (objc == 6) {
	    /* -min {v} L {($v)} sur des cases : la liste elle-même, sans la
	     * recopier (le cas des colonnes d'une boîte englobante) */
	    Tcl_Obj **vv, *c;
	    Tcl_Size nv, bl;
	    const char *b = Tcl_GetStringFromObj(objv[5], &bl), *vn;
	    const unsigned char *data;
	    size_t stride;
	    Tcl_Size cn;
	    int ckind, csize;

	    while (bl > 0 && isspace(UCHAR(*b))) {
		b++;
		bl--;
	    }
	    while (bl > 0 && isspace(UCHAR(b[bl - 1]))) {
		bl--;
	    }
	    if (Tcl_ListObjGetElements(NULL, objv[3], &nv, &vv) == TCL_OK
		    && nv == 1 && bl >= 3 && b[0] == '(' && b[1] == '$'
		    && b[bl - 1] == ')'
		    && (Tcl_Size) strlen(vn = Tcl_GetString(vv[0])) == bl - 3
		    && memcmp(vn, b + 2, bl - 3) == 0
		    && (c = MListContent(objv[4])) != NULL
		    && TclCompactScalarInfo(c, &cn, &data, &stride, &ckind,
			    &csize) && cn > 0) {
		Tcl_Free(av);
		if (CurRep() != NULL) {
		    MapRep *rp = CurRep();

		    rp->pass = "reduce (cells read in place)";
		    rp->elements = cn;
		    rp->blocks += cn;
		    RepBody(rp, Tcl_NewStringObj("identity", -1));
		}
		return (opt[1] == 's') ? TypeMapSum(interp, objv[4])
			: TypeMapMinMax(interp, objv[4], opt[2] == 'a');
	    }
	}
	av[0] = objv[0];
	av[1] = objv[1];
	memcpy(av + 2, objv + 3, sizeof(Tcl_Obj *) * (size_t) (objc - 3));
	cells.name = NULL;		/* (pas un cumul : des cases) */
	cells.init = NULL;
	rc = TypeMapList(interp, objc - 1, av, &cells);
	Tcl_Free(av);
	if (rc != TCL_OK) {
	    return rc;
	}
	r = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(r);
	Tcl_ResetResult(interp);
	rc = (opt[1] == 's') ? TypeMapSum(interp, r)
		: TypeMapMinMax(interp, r, opt[2] == 'a');
	Tcl_DecrRefCount(r);
	return rc;
    }
    if (strcmp(opt, "-fold") == 0) {
	/* -fold {s init} : un cumul ; le résultat est la dernière valeur */
	Tcl_Obj **fv, **av;
	Tcl_Size nf;
	MapFold f;
	int rc;

	if (objc < 4 || Tcl_ListObjGetElements(interp, objv[3], &nf, &fv)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	if (nf != 2) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("type map: -fold needs "
		    "{name initialValue}", -1));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "MAP", (char *) NULL);
	    return TCL_ERROR;
	}
	f.name = fv[0];
	f.init = fv[1];
	Tcl_IncrRefCount(f.name);
	Tcl_IncrRefCount(f.init);
	av = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) objc);
	av[0] = objv[0];
	av[1] = objv[1];
	memcpy(av + 2, objv + 4, sizeof(Tcl_Obj *) * (size_t) (objc - 4));
	rc = TypeMapList(interp, objc - 2, av, &f);
	Tcl_Free(av);
	Tcl_DecrRefCount(f.name);
	Tcl_DecrRefCount(f.init);
	return rc;
    }
    if (objc >= 5 && (objc % 2) == 1) {
	/* v17x : une colonne de flottants en une passe (le résultat de la
	 * boucle par blocs quand aucun élément n'y est marqué) */
	Tcl_Obj *one;

	if (MapTupleFast(interp, objc - 1, objv, &objv[objc - 1], 1, &one,
		NULL, NULL)) {
	    Tcl_SetObjResult(interp, one);
	    Tcl_DecrRefCount(one);
	    return TCL_OK;
	}
    }
    return TypeMapList(interp, objc, objv, NULL);
}

static int
TypeMapList(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[],
	const MapFold *fold)
{
    Tcl_Obj **ev;
    Tcl_Size k, j, nv, nc, m = 2, cap;
    Tcl_Obj **vars, **cols;
    int rc, multi = 0;

    if (objc < 5 || (objc - 3) % 2 != 0) {
	return TypeMapCore(interp, objc, objv, fold);	/* (le message) */
    }
    for (k = 2; k < objc - 1; k += 2) {
	if (Tcl_ListObjLength(NULL, objv[k], &nv) == TCL_OK && nv > 1) {
	    multi = 1;
	}
    }
    if (!multi) {
	return TypeMapCore(interp, objc, objv, fold);
    }
    cap = 2 * objc + 64;
    ev = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) cap);
    ev[0] = objv[0];
    ev[1] = objv[1];
    rc = TCL_OK;
    for (k = 2; k < objc - 1 && rc == TCL_OK; k += 2) {
	Tcl_Obj *vl = Tcl_DuplicateObj(objv[k]), *cl;

	Tcl_IncrRefCount(vl);
	cl = Tcl_DuplicateObj(MListContent(objv[k + 1]));
	Tcl_IncrRefCount(cl);
	if (Tcl_ListObjGetElements(interp, vl, &nv, &vars) != TCL_OK) {
	    rc = TCL_ERROR;
	} else if (nv <= 1) {
	    ev[m++] = objv[k];
	    ev[m++] = objv[k + 1];
	    Tcl_IncrRefCount(objv[k]);
	    Tcl_IncrRefCount(objv[k + 1]);
	} else if (Tcl_ListObjGetElements(interp, cl, &nc, &cols) != TCL_OK) {
	    rc = TCL_ERROR;
	} else if (nc != nv) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map: {%s} needs a "
		    "list of %" TCL_SIZE_MODIFIER "d columns, not %"
		    TCL_SIZE_MODIFIER "d", Tcl_GetString(objv[k]), nv, nc));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "MAP", (char *) NULL);
	    rc = TCL_ERROR;
	} else {
	    if (m + 2 * nv + objc + 1 > cap) {
		cap = m + 2 * nv + objc + 64;
		ev = (Tcl_Obj **) Tcl_Realloc(ev, sizeof(Tcl_Obj *)
			* (size_t) cap);
	    }
	    for (j = 0; j < nv; j++) {
		ev[m] = Tcl_NewListObj(1, &vars[j]);
		Tcl_IncrRefCount(ev[m++]);
		ev[m] = cols[j];
		Tcl_IncrRefCount(ev[m++]);
	    }
	}
	Tcl_DecrRefCount(vl);
	Tcl_DecrRefCount(cl);
	if (rc != TCL_OK) {
	    break;
	}
    }
    if (rc == TCL_OK) {
	ev[m] = objv[objc - 1];
	rc = TypeMapCore(interp, m + 1, ev, fold);
    }
    for (j = 2; j < m; j++) {
	Tcl_DecrRefCount(ev[j]);
    }
    Tcl_Free(ev);
    return rc;
}

/* un objet pour chaque valeur de l'élément I lue en cases (pour expr, ou
 * pour les variables après la commande) */
#define MAP_MATERIALIZE(I) \
    do {								\
	Tcl_Size k_, v_;						\
	for (k_ = 0, v_ = 0; k_ < npairs; k_++) {			\
	    if (pairs[k_].cdata != NULL && nkeep < npairs) {		\
		lastVals[v_] = TclCompactElement(pairs[k_].list, (I));	\
		if (lastVals[v_] != NULL) {				\
		    Tcl_IncrRefCount(lastVals[v_]);			\
		    keep[nkeep++] = lastVals[v_];			\
		}							\
	    }								\
	    v_ += pairs[k_].nvars;					\
	}								\
    } while (0)

struct MapPair {
    Tcl_Size nvars;
    Tcl_Obj **vars;
    Tcl_Size len;
    Tcl_Obj **elems;
    Tcl_Obj *list;
    Tcl_Obj *varList;
    const unsigned char *cdata;	/* en cases : lues directement */
    size_t cstride;
    int ckind, csize;
    Tcl_Size estride;		/* pas dans elems (vue d'objets) */
};

/*
 * Le rassemblement d'un bloc : pour chaque variable de boucle lue par
 * l'expression, ses nb valeurs à partir de l'élément i0 dans la colonne de
 * son rang (la dernière variable du même nom l'emporte, comme dans la
 * boucle). Un élément est marqué quand l'une de ses valeurs n'est pas un
 * flottant simple, ou quand un élément d'une liste à plusieurs variables
 * n'est pas une liste : il repasse par le chemin élément par élément, qui
 * décide (MEval, expr, ou l'erreur). Les cases de flottants sont copiées
 * telles quelles (MDRun les prend telles quelles aussi, nan compris).
 */
static void
TypeMapGather(const struct MapPair *pairs, Tcl_Size npairs,
	const int *slotOf, double *const *cols, Tcl_WideInt *const *icols,
	Tcl_Size i0, int nb, unsigned char *bad, unsigned char *ibad)
{
    Tcl_Size k, v = 0;
    int e;
    MNum t;

    /* bad : l'élément n'est pas tout en doubles ; ibad : pas tout en
     * entiers (sans programme entier, icols NULL : toujours marqué) */
    memset(bad, 0, nb);
    memset(ibad, icols == NULL, nb);
    for (k = 0; k < npairs; k++) {
	const struct MapPair *pr = &pairs[k];

	if (pr->cdata != NULL) {
	    int s = slotOf[v++];

	    if (s < 0) {
		continue;
	    }
	    {
		const unsigned char *c = pr->cdata + (size_t) i0 * pr->cstride;
		double *col = cols[s];

		if (pr->ckind == 2) {
		    memset(ibad, 1, nb);
		    if (pr->csize == 8) {
			if (pr->cstride == 8) {
			    memcpy(col, c, (size_t) nb * 8);
			} else if (pr->cstride % 8 == 0
				&& ((uintptr_t) c % 8) == 0) {
			    /* à pas (une fenêtre) : lecture typée */
			    const double *cd = (const double *) c;
			    size_t s8 = pr->cstride / 8;

			    for (e = 0; e < nb; e++) {
				col[e] = cd[(size_t) e * s8];
			    }
			} else {
			    for (e = 0; e < nb; e++) {
				memcpy(&col[e], c + (size_t) e * pr->cstride, 8);
			    }
			}
		    } else {
			for (e = 0; e < nb; e++) {
			    float f;

			    memcpy(&f, c + (size_t) e * pr->cstride, 4);
			    col[e] = f;
			}
		    }
		} else {
		    /* des entiers en cases */
		    Tcl_WideInt *ic = icols ? icols[s] : NULL;

		    memset(bad, 1, nb);
		    if (ic == NULL) {
			continue;
		    }
		    for (e = 0; e < nb; e++) {
			const unsigned char *cp = c + (size_t) e * pr->cstride;

			switch (pr->csize) {
			case 1:
			    ic[e] = pr->ckind ? (Tcl_WideInt) *(uint8_t *) cp
				    : (Tcl_WideInt) *(int8_t *) cp;
			    break;
			case 2: {
			    uint16_t u; int16_t s16;

			    memcpy(&u, cp, 2); memcpy(&s16, cp, 2);
			    ic[e] = pr->ckind ? (Tcl_WideInt) u : s16;
			    break;
			}
			case 4: {
			    uint32_t u; int32_t s32;

			    memcpy(&u, cp, 4); memcpy(&s32, cp, 4);
			    ic[e] = pr->ckind ? (Tcl_WideInt) u : s32;
			    break;
			}
			default: {
			    uint64_t u;

			    memcpy(&u, cp, 8);
			    if (pr->ckind && u > (uint64_t) WIDE_MAX) {
				ibad[e] = 1;	/* trop grand */
			    }
			    ic[e] = (Tcl_WideInt) u;
			}
			}
		    }
		}
	    }
	    continue;
	}
	if (pr->nvars == 1) {
	    int s = slotOf[v++];

	    if (s >= 0) {
		Tcl_Obj *const *el = pr->elems + i0 * pr->estride;
		Tcl_Size es = pr->estride;
		double *col = cols[s];
		Tcl_WideInt *ic = icols ? icols[s] : NULL;

		for (e = 0; e < nb; e++) {
		    Tcl_Obj *o;

#if defined(__GNUC__)
		    if (e + 8 < nb) {	/* l'objet de dans 8 : déjà demandé */
			__builtin_prefetch(el[(e + 8) * es]);
		    }
#endif
		    o = TCL_ANNOT_CONTENT(el[e * es]);
		    if (TclHasInternalRep(o, &tclDoubleType)
			    && !isnan(o->internalRep.doubleValue)) {
			col[e] = o->internalRep.doubleValue;
			ibad[e] = 1;
		    } else if (MNumOf(o, &t)) {
			if (t.isD == 1) {
			    col[e] = t.d;
			    ibad[e] = 1;
			} else {
			    bad[e] = 1;
			    if (ic != NULL) {
				ic[e] = t.w;
			    }
			}
		    } else {
			bad[e] = ibad[e] = 1;
		    }
		}
	    }
	    continue;
	}
	for (e = 0; e < nb; e++) {
	    Tcl_Obj **parts;
	    Tcl_Size np, j;

	    if (Tcl_ListObjGetElements(NULL, MListContent(
		    pr->elems[(i0 + e) * pr->estride]), &np, &parts) != TCL_OK) {
		bad[e] = ibad[e] = 1;
		continue;
	    }
	    for (j = 0; j < pr->nvars; j++) {
		int s = slotOf[v + j];

		if (s < 0) {
		    continue;
		}
		if (j < np && MNumOf(parts[j], &t)) {
		    if (t.isD == 1) {
			cols[s][e] = t.d;
			ibad[e] = 1;
		    } else {
			bad[e] = 1;
			if (icols != NULL) {
			    icols[s][e] = t.w;
			}
		    }
		} else {
		    bad[e] = ibad[e] = 1;
		}
	    }
	}
	v += pr->nvars;
    }
}

/* les variables qu'un sous-arbre lit */
static void
MMarkSlots(const MProg *g, int i, unsigned char *mark)
{
    const MNode *nd = &g->n[i];
    int k;

    switch (nd->kind) {
    case MN_CONST:
	return;
    case MN_VAR:
	mark[nd->slot] = 1;
	return;
    case MN_NEG: case MN_POS: case MN_NOT:
	MMarkSlots(g, nd->a, mark);
	return;
    case MN_COND:
	MMarkSlots(g, nd->a, mark);
	MMarkSlots(g, nd->b, mark);
	MMarkSlots(g, nd->c, mark);
	return;
    case MN_FUNC:
	for (k = 0; k < nd->nargs; k++) {
	    MMarkSlots(g, nd->args[k], mark);
	}
	return;
    default:
	MMarkSlots(g, nd->a, mark);
	MMarkSlots(g, nd->b, mark);
	return;
    }
}

/* l'opération du sommet d'un cumul découpé, comme MDRun la ferait ; 0 si
 * MDRun refuserait (résultat non fini) */
static int
MDCombine(int op, int fn, double x, double y, double *out)
{
    double r;

    switch (op) {
    case MD_ADD: r = x + y; break;
    case MD_SUB: r = x - y; break;
    case MD_MUL: r = x * y; break;
    case MD_DIV: if (y == 0.0) return 0; r = x / y; break;
    case MD_POW: r = pow(x, y); break;
    default:
	switch (fn) {
	case MF_POWF: r = pow(x, y); break;
	case MF_ATAN2: r = atan2(x, y); break;
	case MF_FMOD: if (y == 0.0) return 0; r = fmod(x, y); break;
	default: r = hypot(x, y); break;
	}
	break;
    }
    if (isnan(r) || isinf(r)) {
	return 0;
    }
    *out = r;
    return 1;
}

static int
TypeMapCore(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[],
	const MapFold *fold)
{
    Tcl_Size npairs, k, j, n = -1, i, total = 0;
    Tcl_Obj *body, **res = NULL, *resList;
    MNum *num = NULL;		/* les résultats nombres (res[i] NULL) */
    const char *bt;
    Tcl_Size blen;
    MProg g;
    int root = -1, fast, allD = 1, allW = 1, rc = TCL_OK;
    int cellsOut = 0;		/* -sum, -min, -max : entiers en cases */
    struct MapPair *pairs;
    int *slotOf = NULL;		/* rang de chaque variable de boucle dans
				 * le programme, ou -1 */
    MDProg dp;
    MNum *vals = NULL, *cnum = NULL;
    Tcl_Obj **lastVals = NULL, **keep = NULL;
    Tcl_Size nkeep = 0;
    int blk = 0;			/* par blocs */
    double **bcols = NULL, *bmem = NULL;
    unsigned char bbad[MD_BLK], ibad[MD_BLK], gbad[MD_BLK];
    Tcl_Size bStart = 0, bEnd = 0;
    int bAllGood = 0;
    /* le programme entier (mi, et iE pour un cumul) : quels programmes
     * servent par blocs */
    MDProg mi, iE;
    int dblOK = 0, intOK = 0, dblSplit = 0, intSplit = 0;
    Tcl_WideInt **icols = NULL, *imem = NULL;
    int iDepth = 0, dDepth = 0;
    /* -fold : s, sa valeur (nombre fNum, ou objet fObj), et le découpage
     * (fA : ce qui dépend de s, fE : le reste, calculé par blocs) */
    int fSlot = -1, fIsNum = 0, fStale = 0, split = 0, fOp = 0, fFn = 0;
    int eSumSlot = -1;		/* s + $v : la variable v (somme directe) */
    Tcl_Size iStart = 0;
    int fSLeft = 0, fAIsS = 0, fAMul = 0;	/* A = s, ou A = c * s */
    double fC = 0.0;
    MNum fNum;
    Tcl_Obj *fObj = NULL;
    MDProg dA, dE;
    MapRep *rep = CurRep();		/* v17y : le relevé demandé */
    Tcl_WideInt rBlk = 0, rElt = 0, rExpr = 0;
    Tcl_Obj *rWhy = NULL;
    Tcl_Size rN = 0;

    memset(&dA, 0, sizeof(dA));
    memset(&dE, 0, sizeof(dE));
    memset(&fNum, 0, sizeof(fNum));
    if (fold != NULL && fold->name == NULL) {
	/* pas un cumul : un résultat interne (-sum, -min, -max), les
	 * entiers en cases de 64 bits, sans objet par élément */
	cellsOut = 1;
	fold = NULL;
    }

    if (objc < 5 || (objc - 3) % 2 != 0) {
	Tcl_WrongNumArgs(interp, 2, objv, "?-sum|-min|-max|-fold {name init}? "
		"varList list ?varList list ...? {(expression)}");
	return TCL_ERROR;
    }
    body = objv[objc - 1];
    bt = Tcl_GetStringFromObj(body, &blen);
    {
	const char *s = bt, *e = bt + blen;

	while (s < e && isspace(UCHAR(*s))) {
	    s++;
	}
	while (e > s && isspace(UCHAR(e[-1]))) {
	    e--;
	}
	if (s == e || *s != '(' || e[-1] != ')') {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("type map: the body "
		    "must be an expression in parentheses, {(...)}", -1));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "MAP", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    npairs = (objc - 3) / 2;
    pairs = (struct MapPair *) Tcl_Alloc(npairs * sizeof(struct MapPair));
    for (k = 0; k < npairs; k++) {
	Tcl_Obj *lo = MListContent(objv[3 + 2 * k]), *vbase;
	Tcl_Obj *const *vel;
	Tcl_Size vn, vst;
	int isView = 0;

	pairs[k].estride = 1;
	if (TclTypeOpsMode() != TCL_TYPE_OPS_OFF
		&& TclObjViewInfo(lo, &vbase, &vel, &vn, &vst)) {
	    /* 24/11 : une vue d'objets, lue dans sa base (tenue ici) */
	    lo = vbase;
	    isView = 1;
	} else if (TclHasInternalRep(lo, &tclListType)) {
	    lo = TclListObjCopy(NULL, lo);	/* comme lmap : à l'abri */
	}
	pairs[k].cdata = NULL;
	pairs[k].elems = NULL;
	pairs[k].list = lo;
	Tcl_IncrRefCount(lo);
	/* la liste des noms à l'abri d'un changement de représentation (le
	 * littéral "x" lu comme nom de variable), comme lmap */
	pairs[k].varList = TclListObjCopy(NULL, objv[2 + 2 * k]);
	if (pairs[k].varList == NULL) {
	    pairs[k].varList = Tcl_NewObj();
	}
	Tcl_IncrRefCount(pairs[k].varList);
	if (isView) {
	    pairs[k].elems = (Tcl_Obj **) vel;
	    pairs[k].len = vn;
	    pairs[k].estride = vst;
	}
	if (Tcl_ListObjGetElements(interp, pairs[k].varList, &pairs[k].nvars,
		&pairs[k].vars) != TCL_OK
		|| (!isView && !(pairs[k].nvars == 1
		    && TclTypeOpsMode() != TCL_TYPE_OPS_OFF
		    && TclCompactScalarInfo(lo, &pairs[k].len, &pairs[k].cdata,
		    &pairs[k].cstride, &pairs[k].ckind, &pairs[k].csize))
		&& Tcl_ListObjGetElements(interp, lo, &pairs[k].len,
		&pairs[k].elems) != TCL_OK)) {
	    npairs = k + 1;
	    rc = TCL_ERROR;
	    goto done;
	}
	if (pairs[k].nvars == 0) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "type map: empty variable list", -1));
	    npairs = k + 1;
	    rc = TCL_ERROR;
	    goto done;
	}
	if (n >= 0 && pairs[k].len != n) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "type map: the lists have different lengths", -1));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "MAP", (char *) NULL);
	    npairs = k + 1;
	    rc = TCL_ERROR;
	    goto done;
	}
	n = pairs[k].len;
	total += pairs[k].nvars;
    }

    /* le programme (une fois) */
    memset(&g, 0, sizeof(g));
    g.p = bt;
    if (TclTypeOpsMode() != TCL_TYPE_OPS_OFF) {
	root = MExpr(&g);
	MSkip(&g);
	if (*g.p != '\0') {
	    MBAD(&g);
	}
    } else {
	g.bad = 1;
	g.badAt = NULL;			/* (les opérations typées éteintes) */
    }
    fast = !g.bad;
    memset(&dp, 0, sizeof(dp));
    memset(&mi, 0, sizeof(mi));
    memset(&iE, 0, sizeof(iE));
    if (fast && MDType(&g, root) == 1) {
	MDCompile(&g, root, &dp);
	dp.ok = (dp.maxDepth <= 32);
    }
    if (fast && MIType(&g, root)) {
	MICompile(&g, root, &mi);
	mi.ok = (mi.maxDepth <= 32);
    }

    /* les variables lues qui ne sont pas des variables de boucle : lues une
     * fois (l'expression est pure) */
    slotOf = (int *) Tcl_Alloc((total ? total : 1) * sizeof(int));
    vals = (MNum *) Tcl_Alloc(((g.nnames ? g.nnames : 1)) * sizeof(MNum));
    {
	Tcl_Size v = 0;

	for (k = 0; k < npairs; k++) {
	    for (j = 0; j < pairs[k].nvars; j++, v++) {
		Tcl_Size l;
		const char *nm = Tcl_GetStringFromObj(pairs[k].vars[j], &l);
		int s;

		slotOf[v] = -1;
		for (s = 0; fast && s < g.nnames; s++) {
		    Tcl_Size l2;
		    const char *t = Tcl_GetStringFromObj(g.names[s], &l2);

		    if (l == l2 && memcmp(nm, t, l) == 0) {
			slotOf[v] = s;	/* (la dernière l'emporte) */
		    }
		}
	    }
	}
	if (fold != NULL) {
	    const char *fn = Tcl_GetString(fold->name);

	    for (k = 0; k < npairs; k++) {
		for (j = 0; j < pairs[k].nvars; j++) {
		    if (strcmp(Tcl_GetString(pairs[k].vars[j]), fn) == 0) {
			Tcl_SetObjResult(interp, Tcl_ObjPrintf("type map: "
				"\"%s\" is both the -fold variable and a loop "
				"variable", fn));
			Tcl_SetErrorCode(interp, "TCL", "TYPE", "MAP",
				(char *) NULL);
			rc = TCL_ERROR;
			n = 0;		/* (rien à parcourir) */
		    }
		}
	    }
	    for (i = 0; fast && i < g.nnames; i++) {
		if (strcmp(Tcl_GetString(g.names[i]), fn) == 0) {
		    fSlot = (int) i;
		}
	    }
	    fObj = fold->init;
	    Tcl_IncrRefCount(fObj);
	    fIsNum = !TclIsAnnotated(fObj) && MNumOf(fObj, &fNum);
	}
	for (i = 0; fast && i < g.nnames; i++) {
	    int loop = 0;

	    for (v = 0; v < total; v++) {
		loop |= (slotOf[v] == i);
	    }
	    if (!loop && i != fSlot) {
		Tcl_Obj *o = Tcl_ObjGetVar2(interp, g.names[i], NULL, 0);

		if (o == NULL || !MNumOf(o, &vals[i])) {
		    fast = 0;		/* absente ou pas un nombre : expr */
		    if (rep != NULL && rWhy == NULL) {
			rWhy = Tcl_ObjPrintf("expr: variable \"%s\" %s",
				Tcl_GetString(g.names[i]), (o == NULL)
				? "does not exist" : "is not a number");
			Tcl_IncrRefCount(rWhy);
		    }
		}
	    }
	}
    }

    res = NULL;			/* fait au premier résultat objet */
    num = (MNum *) Tcl_Alloc(((n > 0) ? n : 1) * sizeof(MNum));
    lastVals = (Tcl_Obj **) Tcl_Alloc((total ? total : 1) * sizeof(Tcl_Obj *));
    cnum = (MNum *) Tcl_Alloc((total ? total : 1) * sizeof(MNum));
    keep = (Tcl_Obj **) Tcl_Alloc((npairs ? npairs : 1) * sizeof(Tcl_Obj *));

    /* par blocs quand tout peut être en flottants : les colonnes du bloc
     * rassemblées dans de petits tableaux (lectures indépendantes), le
     * programme appliqué au bloc entier, et les éléments marqués repris
     * un par un par le chemin ci-dessous */
    if (fast && (dp.ok || mi.ok) && n > 1
	    && getenv("TCL_TYPE_NOBLOCK") == NULL) {
	Tcl_Size v = 0;

	/* les valeurs lues une fois : des doubles pour le programme
	 * flottant, des entiers pour le programme entier ; chaque élément
	 * est ensuite classé au rassemblement */
	dblOK = dp.ok;
	intOK = mi.ok;
	for (i = 0; i < g.nnames; i++) {
	    int loop = 0;

	    for (v = 0; v < total; v++) {
		loop |= (slotOf[v] == i);
	    }
	    if (!loop && i != fSlot) {
		dblOK &= (vals[i].isD == 1);
		intOK &= (vals[i].isD == 0);
	    }
	}
	blk = dblOK || intOK;
    }
    if (blk && fold != NULL) {
	/* un cumul : par blocs seulement s'il se découpe. Le sommet est une
	 * opération binaire ; un côté (A) ne lit que s et des valeurs fixes,
	 * l'autre (E) ne lit pas s : E par blocs, puis s = A(s) op E, élément
	 * par élément, dans l'ordre (le résultat est celui de la boucle) */
	int r = root, L, R;
	const MNode *nd;

	blk = 0;
	while (r >= 0 && g.n[r].kind == MN_POS) {
	    r = g.n[r].a;
	}
	nd = (r >= 0) ? &g.n[r] : NULL;
	if (fSlot >= 0 && nd != NULL && (nd->kind == MN_ADD
		|| nd->kind == MN_SUB || nd->kind == MN_MUL
		|| nd->kind == MN_DIV || nd->kind == MN_POW
		|| (nd->kind == MN_FUNC && nd->nargs == 2
		&& (nd->fn == MF_POWF || nd->fn == MF_ATAN2
		|| nd->fn == MF_FMOD || nd->fn == MF_HYPOT)))) {
	    unsigned char *mL = (unsigned char *) Tcl_Alloc(g.nnames + 1);
	    unsigned char *mR = (unsigned char *) Tcl_Alloc(g.nnames + 1);
	    int aSide = -1, okA = 1;

	    L = (nd->kind == MN_FUNC) ? nd->args[0] : nd->a;
	    R = (nd->kind == MN_FUNC) ? nd->args[1] : nd->b;
	    memset(mL, 0, g.nnames + 1);
	    memset(mR, 0, g.nnames + 1);
	    MMarkSlots(&g, L, mL);
	    MMarkSlots(&g, R, mR);
	    if (mL[fSlot] && !mR[fSlot]) {
		aSide = 0;
	    } else if (mR[fSlot] && !mL[fSlot]) {
		aSide = 1;
	    }
	    for (j = 0; aSide >= 0 && j < total; j++) {
		if (slotOf[j] >= 0 && (aSide ? mR : mL)[slotOf[j]]) {
		    okA = 0;		/* A lit une variable de boucle */
		}
	    }
	    if (aSide >= 0 && okA && intOK
		    && g.n[aSide ? R : L].kind == MN_VAR
		    && g.n[aSide ? R : L].slot == fSlot
		    && (nd->kind == MN_ADD || nd->kind == MN_SUB
		    || nd->kind == MN_MUL) && MIType(&g, aSide ? L : R)) {
		/* en entiers : s op E, E par blocs */
		MICompile(&g, aSide ? L : R, &iE);
		if (iE.maxDepth <= 32) {
		    intSplit = 1;
		    if (nd->kind == MN_ADD
			    && g.n[aSide ? L : R].kind == MN_VAR
			    && g.n[aSide ? L : R].slot != fSlot) {
			eSumSlot = g.n[aSide ? L : R].slot;	/* s + $v */
		    }
		}
	    }
	    if (aSide >= 0 && okA && (dblOK || intSplit)) {
		int dTyped = dblOK && MDType(&g, aSide ? R : L) == 1
			&& MDType(&g, aSide ? L : R) == 1;

		if (dTyped) {
		    MDCompile(&g, aSide ? R : L, &dA);
		    MDCompile(&g, aSide ? L : R, &dE);
		    dblSplit = (dA.maxDepth <= 32 && dE.maxDepth <= 32);
		}
		if (dblSplit || intSplit) {
		    split = blk = 1;
		    fSLeft = (aSide == 0);
		    fOp = (nd->kind == MN_ADD) ? MD_ADD
			    : (nd->kind == MN_SUB) ? MD_SUB
			    : (nd->kind == MN_MUL) ? MD_MUL
			    : (nd->kind == MN_DIV) ? MD_DIV
			    : (nd->kind == MN_POW) ? MD_POW : MD_F2;
		    fFn = nd->fn;
		    fAIsS = (g.n[aSide ? R : L].kind == MN_VAR
			    && g.n[aSide ? R : L].slot == fSlot);
		    /* A = c * s ou s * c, c constante ou valeur fixe : la
		     * multiplication faite telle quelle, dans le même ordre */
		    if (dA.nops == 3 && dA.ops[2].op == MD_MUL) {
			const MDOp *o0 = &dA.ops[0], *o1 = &dA.ops[1];

			if (o1->op == MD_VAR && o1->arg == fSlot
				&& (o0->op == MD_CONST || (o0->op == MD_VAR
				&& o0->arg != fSlot))) {
			    fAMul = 1;		/* c * s */
			    fC = (o0->op == MD_CONST) ? o0->k : vals[o0->arg].d;
			} else if (o0->op == MD_VAR && o0->arg == fSlot
				&& (o1->op == MD_CONST || (o1->op == MD_VAR
				&& o1->arg != fSlot))) {
			    fAMul = 2;		/* s * c */
			    fC = (o1->op == MD_CONST) ? o1->k : vals[o1->arg].d;
			}
		    }
		}
	    }
	    Tcl_Free(mL);
	    Tcl_Free(mR);
	}
    }
    if (blk) {
	dDepth = (dp.maxDepth > dE.maxDepth) ? dp.maxDepth : dE.maxDepth;
	iDepth = (mi.maxDepth > iE.maxDepth) ? mi.maxDepth : iE.maxDepth;
	icols = (Tcl_WideInt **) Tcl_Alloc(g.nnames * sizeof(Tcl_WideInt *));
	imem = (Tcl_WideInt *) Tcl_Alloc(((size_t) g.nnames + iDepth + 1)
		* MD_BLK * sizeof(Tcl_WideInt));
	memset(imem, 0, ((size_t) g.nnames + iDepth + 1) * MD_BLK
		* sizeof(Tcl_WideInt));
	for (i = 0; i < g.nnames; i++) {
	    icols[i] = NULL;
	}
	for (j = 0; j < total; j++) {
	    if (slotOf[j] >= 0) {
		icols[slotOf[j]] = imem + (size_t) slotOf[j] * MD_BLK;
	    }
	}
	bcols = (double **) Tcl_Alloc(g.nnames * sizeof(double *));
	bmem = (double *) Tcl_Alloc(((size_t) g.nnames + dDepth + 1)
		* MD_BLK * sizeof(double));
	memset(bmem, 0, ((size_t) g.nnames + dDepth + 1) * MD_BLK
		* sizeof(double));
	for (i = 0; i < g.nnames; i++) {
	    bcols[i] = NULL;
	}
	for (j = 0; j < total; j++) {
	    if (slotOf[j] >= 0) {
		bcols[slotOf[j]] = bmem + (size_t) slotOf[j] * MD_BLK;
	    }
	}
    }
    /*
     * s + $v sur une colonne d'entiers en cases (8, 16, 32 bits) : la somme
     * des n-1 premiers éléments d'un trait (le dernier passe par le chemin
     * général, qui pose les variables). Même résultat que l'ordre élément
     * par élément tant qu'aucune somme partielle ne dépasse 64 bits : non
     * signés, les sommes partielles vont de s à la somme finale ; signés,
     * |s| < 2^62 et moins de 2^30 éléments. Sinon le chemin par blocs.
     */
    if (split && intSplit && eSumSlot >= 0 && fIsNum && fNum.isD == 0
	    && n > 1) {
	Tcl_Size col = 0;

	for (k = 0; k < npairs; k++) {
	    const struct MapPair *pr = &pairs[k];
	    Tcl_Size jj;

	    for (jj = 0; jj < pr->nvars; jj++, col++) {
		if (slotOf[col] == eSumSlot && pr->nvars == 1
			&& pr->cdata != NULL && pr->ckind < 2
			&& pr->csize <= 4 && n - 1 < ((Tcl_Size) 1 << 30)) {
		    Tcl_WideInt sum = MapCellSum(pr->cdata, pr->cstride,
			    pr->ckind, pr->csize, n - 1), r;
		    Tcl_WideInt sw = fNum.w;
		    int safe = pr->ckind == 1 || (sw < ((Tcl_WideInt) 1 << 62)
			    && sw > -((Tcl_WideInt) 1 << 62));

		    if (safe && !__builtin_add_overflow(sw, sum, &r)) {
			fNum.w = r;
			fStale = 1;
			iStart = n - 1;
			rBlk += n - 1;	/* (la somme directe) */
		    }
		}
	    }
	}
    }
    for (i = iStart; i < n; i++) {
	MNum out;
	int ok = fast;
	Tcl_Size v = 0;

	while (nkeep > 0) {		/* les objets du tour précédent */
	    Tcl_DecrRefCount(keep[--nkeep]);
	}

	if (blk) {
	    if (i >= bEnd) {
		bStart = i;
		bEnd = (n - i > MD_BLK) ? i + MD_BLK : n;
		int dRun = split ? dblSplit : dblOK;
		int iRun = split ? intSplit : intOK;
		const double *bout = bmem
			+ ((size_t) g.nnames + dDepth) * MD_BLK;
		const Tcl_WideInt *iout = imem
			+ ((size_t) g.nnames + iDepth) * MD_BLK;
		int e, nb = (int) (bEnd - bStart), anyD = 0, anyI = 0;
		MNum *dst = num + bStart;

		TypeMapGather(pairs, npairs, slotOf, bcols,
			iRun ? icols : NULL, bStart, nb, bbad, ibad);
		if (dRun) {
		    MDRunBlock(split ? &dE : &dp, bcols, vals, nb,
			    bmem + (size_t) g.nnames * MD_BLK, bbad,
			    (double *) bout);
		} else {
		    memset(bbad, 1, nb);
		}
		if (iRun && memchr(ibad, 0, nb) != NULL) {
		    /* (rien à faire si aucun élément n'est tout en entiers :
		     * des cases de flottants, par exemple) */
		    MIRunBlock(split ? &iE : &mi, icols, vals, nb,
			    imem + (size_t) g.nnames * MD_BLK, ibad,
			    (Tcl_WideInt *) iout);
		}
		/* tous rangés ; les marqués seront refaits un par un */
		for (e = 0; e < nb; e++) {
		    gbad[e] = bbad[e] && ibad[e];
		    rBlk += (!split && !gbad[e]);
		    if (!bbad[e]) {
			anyD = 1;
			if (!split) {
			    dst[e].isD = 1;
			    dst[e].d = bout[e];
			}
		    } else if (!ibad[e]) {
			anyI = 1;
			if (!split) {
			    dst[e].isD = 0;
			    dst[e].w = iout[e];
			}
		    }
		}
		bAllGood = (memchr(gbad, 1, nb) == NULL);
		if (anyD) {
		    allW = 0;		/* au moins un flottant */
		}
		if (anyI) {
		    allD = 0;		/* au moins un entier */
		}
	    }
	    if (split) {
		/* le cumul : s = A(s) op E[i], dans l'ordre */
		const double *bout = bmem
			+ ((size_t) g.nnames + dDepth) * MD_BLK;
		Tcl_Size i2 = i, lim = (bEnd < n - 1) ? bEnd : n - 1;

		/* en entiers : s op e, sur 64 bits, arrêt au dépassement */
		if (intSplit && fIsNum && fNum.isD == 0) {
		    const Tcl_WideInt *ie = imem
			    + ((size_t) g.nnames + iDepth) * MD_BLK - bStart;
		    Tcl_WideInt sw = fNum.w, r;

		    for (; i2 < lim && !ibad[i2 - bStart]; i2++) {
			if (!(fSLeft ? MIOp(fOp, sw, ie[i2], &r)
				: MIOp(fOp, ie[i2], sw, &r))) {
			    break;
			}
			sw = r;
		    }
		    if (i2 > i) {
			fNum.w = sw;
			fStale = 1;
		    }
		}
		/* les formes courantes en boucle serrée : mêmes opérations, même
		 * ordre, même refus d'un résultat non fini */
		if (dblSplit && i2 == i && fIsNum && fNum.isD == 1 && fSLeft
			&& (fOp == MD_ADD || fOp == MD_SUB || fOp == MD_MUL)
			&& (fAIsS || fAMul)) {
		    const double *eb = bout - bStart;
		    double sd = fNum.d, a, r;

		    for (; i2 < lim && !bbad[i2 - bStart]; i2++) {
			a = fAIsS ? sd : (fAMul == 1) ? fC * sd : sd * fC;
			r = (fOp == MD_ADD) ? a + eb[i2] : (fOp == MD_SUB)
				? a - eb[i2] : a * eb[i2];
			if (isnan(r) || isinf(r)
				|| (!fAIsS && (isnan(a) || isinf(a)))) {
			    break;
			}
			sd = r;
		    }
		    if (i2 > i) {
			fNum.d = sd;
			fStale = 1;
		    }
		}
		for (; dblSplit && i2 < bEnd && i2 != n - 1; i2++) {
		    double a, r;

		    if (bbad[i2 - bStart] || !fIsNum || fNum.isD != 1) {
			break;
		    }
		    if (fAIsS) {
			a = fNum.d;
		    } else {
			vals[fSlot] = fNum;
			if (!MDRun(&dA, vals, &a)) {
			    break;
			}
		    }
		    if (!(fSLeft ? MDCombine(fOp, fFn, a, bout[i2 - bStart], &r)
			    : MDCombine(fOp, fFn, bout[i2 - bStart], a, &r))) {
			break;
		    }
		    fNum.d = r;
		    fStale = 1;
		}
		if (i2 > i) {
		    rBlk += i2 - i;
		    i = i2 - 1;		/* faits ; le suivant reprend ici */
		    continue;
		}
		/* l'élément i par le chemin général */
	    } else if (bAllGood && bEnd != n) {
		i = bEnd - 1;		/* le bloc entier est fait */
		continue;
	    }
	    if (!split && !gbad[i - bStart] && i != n - 1) {
		Tcl_Size i2 = i + 1;	/* les suivants bons : sautés */

		while (i2 < bEnd && !gbad[i2 - bStart] && i2 != n - 1) {
		    i2++;
		}
		i = i2 - 1;
		continue;
	    }
	}

	/* les valeurs de l'élément i */
	for (k = 0; k < npairs; k++) {
	    Tcl_Obj *e;

	    if (pairs[k].cdata != NULL) {
		/* en cases : le nombre lu dans la case ; un objet seulement
		 * si l'élément passe par expr, ou pour le dernier */
		const unsigned char *c = pairs[k].cdata
			+ (size_t) i * pairs[k].cstride;
		MNum t;

		t.isD = (pairs[k].ckind == 2);
		if (t.isD) {
		    if (pairs[k].csize == 4) {
			float f;

			memcpy(&f, c, 4);
			t.d = f;
		    } else {
			memcpy(&t.d, c, 8);
		    }
		} else {
		    switch (pairs[k].csize) {
		    case 1:
			t.w = pairs[k].ckind ? (Tcl_WideInt) *(uint8_t *) c
				: (Tcl_WideInt) *(int8_t *) c;
			break;
		    case 2: {
			uint16_t u; int16_t s16;

			memcpy(&u, c, 2); memcpy(&s16, c, 2);
			t.w = pairs[k].ckind ? (Tcl_WideInt) u : s16;
			break;
		    }
		    case 4: {
			uint32_t u; int32_t s32;

			memcpy(&u, c, 4); memcpy(&s32, c, 4);
			t.w = pairs[k].ckind ? (Tcl_WideInt) u : s32;
			break;
		    }
		    default: {
			uint64_t u;

			memcpy(&u, c, 8);
			if (pairs[k].ckind && u > (uint64_t) WIDE_MAX) {
			    t.isD = -1;		/* trop grand : expr */
			}
			t.w = (Tcl_WideInt) u;
		    }
		    }
		}
		cnum[v] = t;
		lastVals[v++] = NULL;	/* fait à la demande */
		continue;
	    }
	    e = pairs[k].elems[i * pairs[k].estride];
	    if (pairs[k].nvars == 1) {
		cnum[v].isD = -2;	/* pas en cases */
		lastVals[v++] = e;
	    } else {
		Tcl_Obj **parts;
		Tcl_Size np;

		if (Tcl_ListObjGetElements(interp, MListContent(e), &np,
			&parts) != TCL_OK) {
		    rc = TCL_ERROR;
		    n = i;
		    goto loopdone;
		}
		for (j = 0; j < pairs[k].nvars; j++) {
		    cnum[v].isD = -2;
		    lastVals[v++] = (j < np) ? parts[j] : NULL;
		}
	    }
	}
	if (ok && fSlot >= 0) {
	    if (fIsNum) {
		vals[fSlot] = fNum;
	    } else {
		ok = 0;			/* s n'est pas un nombre : expr */
	    }
	}
	if (ok) {
	    for (v = 0; v < total; v++) {
		if (slotOf[v] < 0) {
		    continue;
		}
		if (cnum[v].isD >= 0) {
		    vals[slotOf[v]] = cnum[v];
		} else if (cnum[v].isD == -1
			|| !MNumOf(lastVals[v], &vals[slotOf[v]])) {
		    ok = 0;
		    break;
		}
	    }
	}
	if (i == n - 1) {
	    MAP_MATERIALIZE(i);		/* les variables après la commande */
	}
	if (ok && dp.ok) {
	    /* tout en flottants ? le programme postfixe */
	    int s2, allDv = 1;

	    for (s2 = 0; s2 < g.nnames; s2++) {
		allDv &= (vals[s2].isD == 1);
	    }
	    if (allDv) {
		out.isD = 1;
		if (MDRun(&dp, vals, &out.d)) {
		    rElt += !(blk && !split && i == n - 1 && !gbad[i - bStart]);
		    num[i] = out;
		    allW = 0;
		    if (fold != NULL) {
			fNum = out;
			fIsNum = fStale = 1;
		    }
		    continue;
		}
		ok = 0;			/* expr pour cet élément */
	    }
	}
	if (ok && MEval(&g, root, vals, &out)) {
	    rElt += !(blk && !split && i == n - 1 && !gbad[i - bStart]);
	    num[i] = out;		/* un nombre, gardé tel quel */
	    allD &= out.isD;
	    allW &= !out.isD;
	    if (fold != NULL) {
		fNum = out;
		fIsNum = fStale = 1;
	    }
	} else {
	    /* par expr, les variables posées dans le cadre de l'appelant */
	    Tcl_Obj *r;

	    if (i != n - 1) {
		MAP_MATERIALIZE(i);
	    }

	    for (k = 0, v = 0; k < npairs; k++) {
		for (j = 0; j < pairs[k].nvars; j++, v++) {
		    if (Tcl_ObjSetVar2(interp, pairs[k].vars[j], NULL,
			    lastVals[v] ? lastVals[v] : Tcl_NewObj(),
			    TCL_LEAVE_ERR_MSG) == NULL) {
			rc = TCL_ERROR;
			n = i;
			goto loopdone;
		    }
		}
	    }
	    if (fold != NULL) {
		/* s, à jour, pour expr */
		if (fStale) {
		    Tcl_DecrRefCount(fObj);
		    fObj = fNum.isD ? Tcl_NewDoubleObj(fNum.d)
			    : Tcl_NewWideIntObj(fNum.w);
		    Tcl_IncrRefCount(fObj);
		    fStale = 0;
		}
		if (Tcl_ObjSetVar2(interp, fold->name, NULL, fObj,
			TCL_LEAVE_ERR_MSG) == NULL) {
		    rc = TCL_ERROR;
		    n = i;
		    goto loopdone;
		}
	    }
	    rExpr++;
	    if (rep != NULL && rWhy == NULL && fast) {
		/* la première raison qui envoie un élément à expr */
		Tcl_Size v2 = 0, k2, j2;
		const char *nm = NULL;
		Tcl_Obj *val = NULL;

		for (k2 = 0; k2 < npairs && nm == NULL; k2++) {
		    for (j2 = 0; j2 < pairs[k2].nvars; j2++, v2++) {
			MNum t2;

			if (slotOf[v2] >= 0 && cnum[v2].isD < 0
				&& (lastVals[v2] == NULL
				|| !MNumOf(lastVals[v2], &t2))) {
			    nm = Tcl_GetString(pairs[k2].vars[j2]);
			    val = lastVals[v2];
			    break;
			}
		    }
		}
		if (nm != NULL) {
		    rWhy = Tcl_ObjPrintf("element %" TCL_SIZE_MODIFIER "d: "
			    "%s = \"%.30s\" is not a number, by expr", i, nm,
			    val ? Tcl_GetString(val) : "");
		} else if (fSlot >= 0 && !fIsNum) {
		    rWhy = Tcl_ObjPrintf("element %" TCL_SIZE_MODIFIER "d: "
			    "the accumulator is not a number, by expr", i);
		} else {
		    rWhy = Tcl_ObjPrintf("element %" TCL_SIZE_MODIFIER "d: "
			    "not computable as a number (non-finite, "
			    "overflow...), by expr", i);
		}
		Tcl_IncrRefCount(rWhy);
	    }
	    if (Tcl_ExprObj(interp, body, &r) != TCL_OK) {
		Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
			"\n    (type map element %" TCL_SIZE_MODIFIER "d)", i));
		rc = TCL_ERROR;
		n = i;
		goto loopdone;
	    }
	    if (fold != NULL) {
		/* la nouvelle valeur de s (l'objet d'expr, tel quel) */
		MNum t;

		Tcl_DecrRefCount(fObj);
		fObj = r;		/* (la référence d'expr) */
		fStale = 0;
		fIsNum = !TclIsAnnotated(r) && MNumOf(r, &t);
		if (fIsNum) {
		    fNum = t;
		}
		continue;
	    }
	    {
		MNum t;
		int isNum = !TclIsAnnotated(r) && MNumOf(r, &t)
			&& TclHasInternalRep(r, t.isD ? &tclDoubleType
			: &tclIntType);

		if (isNum) {
		    num[i] = t;		/* un nombre pur : en cases aussi */
		    Tcl_DecrRefCount(r);
		} else {
		    if (res == NULL) {
			res = (Tcl_Obj **) Tcl_AttemptAlloc(
				((n > 0) ? n : 1) * sizeof(Tcl_Obj *));
			if (res == NULL) {
			    Tcl_DecrRefCount(r);
			    Tcl_SetObjResult(interp, Tcl_NewStringObj(
				    "type map: out of memory", -1));
			    rc = TCL_ERROR;
			    n = i;
			    goto loopdone;
			}
			memset(res, 0, ((n > 0) ? n : 1) * sizeof(Tcl_Obj *));
		    }
		    res[i] = r;		/* rendu avec une référence */
		    num[i].isD = -1;
		}
		allD &= isNum && t.isD;
		allW &= isNum && !t.isD;
	    }
	}
    }
  loopdone:
    rN = n;
    /* comme lmap : les variables gardent le dernier élément */
    if (rc == TCL_OK && n > 0) {
	Tcl_Size v = 0;

	for (k = 0; k < npairs; k++) {
	    for (j = 0; j < pairs[k].nvars; j++, v++) {
		if (Tcl_ObjSetVar2(interp, pairs[k].vars[j], NULL,
			lastVals[v] ? lastVals[v] : Tcl_NewObj(),
			TCL_LEAVE_ERR_MSG) == NULL) {
		    rc = TCL_ERROR;
		    break;
		}
	    }
	    if (rc != TCL_OK) {
		break;
	    }
	}
    }
    if (fold != NULL) {
	if (rc == TCL_OK) {
	    if (fStale) {
		Tcl_DecrRefCount(fObj);
		fObj = fNum.isD ? Tcl_NewDoubleObj(fNum.d)
			: Tcl_NewWideIntObj(fNum.w);
		Tcl_IncrRefCount(fObj);
	    }
	    if (Tcl_ObjSetVar2(interp, fold->name, NULL, fObj,
		    TCL_LEAVE_ERR_MSG) == NULL) {
		rc = TCL_ERROR;
	    } else {
		Tcl_SetObjResult(interp, fObj);
	    }
	}
	goto results;
    }
    if (rc == TCL_OK && n > 0 && (allD || (allW && cellsOut))) {
	/* que des nombres d'un genre : une liste en cases, sans objets */
	const char *tn = allD ? "list.double" : "list.int(s,64)";
	TclType *lt = TclTypeParse(interp, tn, (Tcl_Size) strlen(tn));
	unsigned char *data;
	Tcl_Obj *cv = (lt != NULL) ? TclCompactNewAlways(lt, n, &data) : NULL;

	Tcl_ResetResult(interp);
	if (cv != NULL) {
	    if (allD) {
		for (i = 0; i < n; i++) {
		    memcpy(data + 8 * (size_t) i, &num[i].d, 8);
		}
	    } else {
		for (i = 0; i < n; i++) {
		    memcpy(data + 8 * (size_t) i, &num[i].w, 8);
		}
	    }
	    Tcl_SetObjResult(interp, TclNewAnnotatedOwn(cv,
		    TclTypeRefFor(interp, lt, -1)));
	    goto results;
	}
    }
    if (rc == TCL_OK) {
	if (res == NULL) {
	    res = (Tcl_Obj **) Tcl_Alloc(((n > 0) ? n : 1)
		    * sizeof(Tcl_Obj *));
	    memset(res, 0, ((n > 0) ? n : 1) * sizeof(Tcl_Obj *));
	}
	{
	    /*
	     * Petits entiers : un objet partagé par valeur (les octets, les
	     * compteurs) ; ailleurs, la valeur précédente si elle est égale.
	     * Les objets sont purs et sans texte : les partager ne change rien.
	     */
	    Tcl_Obj *small[MAP_SMALL_HI - MAP_SMALL_LO + 1];
	    Tcl_Obj *prev = NULL;
	    MNum pv;

	    memset(small, 0, sizeof(small));
	    pv.isD = -1;
	    pv.w = 0;
	    for (i = 0; i < n; i++) {
		if (res[i] != NULL) {
		    continue;
		}
		if (!num[i].isD && num[i].w >= MAP_SMALL_LO
			&& num[i].w <= MAP_SMALL_HI) {
		    Tcl_Obj **c = &small[num[i].w - MAP_SMALL_LO];

		    if (*c == NULL) {
			*c = Tcl_NewWideIntObj(num[i].w);
		    }
		    res[i] = *c;
		} else if (prev != NULL && pv.isD == num[i].isD
			&& (num[i].isD ? memcmp(&pv.d, &num[i].d, 8) == 0
			    : pv.w == num[i].w)) {
		    res[i] = prev;
		} else {
		    res[i] = num[i].isD ? Tcl_NewDoubleObj(num[i].d)
			    : Tcl_NewWideIntObj(num[i].w);
		    prev = res[i];
		    pv = num[i];
		}
		Tcl_IncrRefCount(res[i]);
	    }
	}
	resList = Tcl_NewListObj(n, res);
	if (n > 0 && (allD || allW)) {
	    const char *tn = allD ? "list.double" : "list.int";
	    TclType *lt = TclTypeParse(interp, tn, (Tcl_Size) strlen(tn));
	    Tcl_Obj *a = NULL;

	    if (lt != NULL) {
		Tcl_IncrRefCount(resList);
		a = TclNewAnnotated(interp, lt, -1, resList);
		Tcl_DecrRefCount(resList);
	    }

	    if (a != NULL) {
		resList = a;
	    }
	    Tcl_ResetResult(interp);
	}
	Tcl_SetObjResult(interp, resList);
    }
  results:
    if (rep != NULL) {
	/* v17y : ce que cette passe a fait */
	Tcl_Obj *desc;

	rep->elements = rN;
	if (rep->comp <= 0) {
	    Tcl_Obj *in = Tcl_NewListObj(0, NULL);
	    Tcl_Obj *vb;
	    Tcl_Obj *const *ve;
	    Tcl_Size vn, vs;

	    for (k = 0; k < npairs; k++) {
		for (j = 0; j < pairs[k].nvars; j++) {
		    Tcl_ListObjAppendElement(NULL, in, pairs[k].vars[j]);
		    Tcl_ListObjAppendElement(NULL, in, (pairs[k].nvars > 1)
			    ? Tcl_NewStringObj("objects (lists)", -1)
			    : RepInputKind(pairs[k].cdata, pairs[k].ckind,
			    pairs[k].csize, pairs[k].cstride,
			    pairs[k].estride, pairs[k].estride != 1
			    || TclObjViewInfo(MListContent(objv[3 + 2 * k]),
			    &vb, &ve, &vn, &vs)));
		}
	    }
	    RepSetObj(&rep->inputs, in);
	}
	if (!fast) {
	    desc = g.bad ? RepNotCompiled(&g, bt) : (rWhy != NULL
		    && strncmp(Tcl_GetString(rWhy), "expr:", 5) == 0)
		    ? rWhy : Tcl_NewStringObj("expr", -1);
	} else {
	    desc = Tcl_ObjPrintf("compiled%s%s", dp.ok ? " double" : "",
		    mi.ok ? " int" : (dp.ok ? "" : " (scalar)"));
	}
	RepBody(rep, desc);
	if (rep->pass == NULL) {
	    rep->pass = (fold != NULL) ? "fold" : cellsOut ? "reduce"
		    : "column";
	}
	if (fast && !blk && n > 1 && getenv("TCL_TYPE_NOBLOCK") == NULL) {
	    RepWhy(rep, Tcl_NewStringObj((fold != NULL)
		    ? "blocks: the fold does not split (s op E, E without s)"
		    : "blocks: the fixed values are neither all double nor "
		    "all int", -1));
	}
	if (rWhy != NULL && fast) {
	    RepWhy(rep, rWhy);
	}
	rep->blocks += rBlk;
	rep->elementwise += rElt;
	rep->expr += rExpr;
    }
    if (rWhy != NULL) {
	Tcl_DecrRefCount(rWhy);
    }
    if (res != NULL) {
	for (i = 0; i < n; i++) {
	    if (res[i] != NULL) {
		Tcl_DecrRefCount(res[i]);
	    }
	}
	Tcl_Free(res);
    }
    Tcl_Free(num);
    Tcl_Free(lastVals);
    Tcl_Free(cnum);
    for (k = 0; k < nkeep; k++) {
	Tcl_DecrRefCount(keep[k]);
    }
    Tcl_Free(keep);
    Tcl_Free(slotOf);
    Tcl_Free(vals);
    if (bcols != NULL) {
	Tcl_Free(bcols);
	Tcl_Free(bmem);
    }
    if (fObj != NULL) {
	Tcl_DecrRefCount(fObj);
    }
    if (dA.ops) {
	Tcl_Free(dA.ops);
    }
    if (dE.ops) {
	Tcl_Free(dE.ops);
    }
    if (mi.ops) {
	Tcl_Free(mi.ops);
    }
    if (iE.ops) {
	Tcl_Free(iE.ops);
    }
    if (icols != NULL) {
	Tcl_Free(icols);
	Tcl_Free(imem);
    }
    MProgFree(&g);
    if (dp.ops) {
	Tcl_Free(dp.ops);
    }
  done:
    for (k = 0; k < npairs; k++) {
	Tcl_DecrRefCount(pairs[k].list);
	Tcl_DecrRefCount(pairs[k].varList);
    }
    Tcl_Free(pairs);
    return rc;
}

/* comme lmap : les variables de boucle gardent le dernier élément */
static int
MapSetLastVars(Tcl_Interp *interp, const struct MapPair *pairs,
	Tcl_Size npairs, Tcl_Size n)
{
    Tcl_Size k;

    for (k = 0; k < npairs; k++) {
	Tcl_Obj *e = (pairs[k].cdata != NULL)
		? TclCompactElement(pairs[k].list, n - 1)
		: pairs[k].elems[(n - 1) * pairs[k].estride];

	if (e == NULL) {
	    e = Tcl_NewObj();
	}
	Tcl_IncrRefCount(e);
	if (Tcl_ObjSetVar2(interp, pairs[k].varList, NULL, e, 0) == NULL) {
	    Tcl_DecrRefCount(e);
	    return TCL_ERROR;
	}
	Tcl_DecrRefCount(e);
    }
    return TCL_OK;
}

/*
 * v17x : un tuple en une passe. type map {x y} L {(A, B ...)} : les K
 * composantes lisent les mêmes colonnes ; chaque bloc est rassemblé une
 * fois et les K programmes flottants tournent dessus, chacun écrivant dans
 * sa colonne de sortie (list.double en cases). Seulement quand le
 * résultat est celui des K passes à l'identique : chaque composante dans
 * le programme flottant, les valeurs fixes des flottants, et aucun élément
 * marqué (un entier, un texte, un résultat non fini) ; sinon 0, sans rien
 * avoir changé, et le découpage habituel décide (ses messages, expr...).
 * av : objv[0], objv[1], puis les paires varList liste (sans le corps).
 * Rend 1 et K colonnes (avec une référence chacune) dans outCols.
 */
static int
MapTupleFast(Tcl_Interp *interp, Tcl_Size ac, Tcl_Obj *const av[],
	Tcl_Obj *const parts[], Tcl_Size K, Tcl_Obj **outCols,
	Tcl_Obj *sinkName, const char *sinkPath)
{
    void *sink = NULL;		/* -in : les cases de la cible */
    unsigned char *sdst[64];
    size_t sstep[64];
    double *tmp = NULL;
    struct MapPair *pairs = NULL;
    MProg *g = NULL;
    MDProg *dp = NULL;
    MNum **vals = NULL;
    double ***ccols = NULL, **gcols = NULL, *gmem = NULL, *scratch = NULL;
    unsigned char **outData = NULL;
    int *slotOf = NULL, maxDepth = 0, ok = 0;
    Tcl_Size npairs = 0, k, j, s, n = -1, i, built = 0;
    unsigned char bbad[MD_BLK], ibad[MD_BLK], cbad[MD_BLK];
    TclType *lt;
    MapRep *rep = CurRep();
    Tcl_Obj *why = NULL;		/* v17y : pourquoi pas en une passe */
    const char *swhy = NULL;
    int copied = 0;

#define MTF_FAIL(o) \
    do {								\
	if (rep != NULL && why == NULL) {				\
	    why = (o);							\
	    Tcl_IncrRefCount(why);					\
	}								\
	goto done;							\
    } while (0)

    if (K < 1 || K > 64 || ac < 4 || (ac % 2) != 0
	    || TclTypeOpsMode() == TCL_TYPE_OPS_OFF
	    || getenv("TCL_TYPE_NOBLOCK") != NULL
	    || getenv("TCL_TYPE_NOTUPLE") != NULL) {
	return 0;
    }
    /* les colonnes : une variable chacune (une varList de plusieurs noms
     * prend une liste d'autant de colonnes, comme TypeMapList) */
    for (k = 2; k < ac; k += 2) {
	Tcl_Size nv;

	if (MapSpecTyped(av[k])
		|| Tcl_ListObjLength(NULL, av[k], &nv) != TCL_OK || nv < 1) {
	    return 0;
	}
	npairs += nv;
    }
    pairs = (struct MapPair *) Tcl_Alloc(npairs * sizeof(struct MapPair));
    memset(pairs, 0, npairs * sizeof(struct MapPair));
    npairs = 0;
    for (k = 2; k < ac; k += 2) {
	Tcl_Obj **vv, **cv, *one = av[k + 1];
	Tcl_Size nv, nc;

	Tcl_ListObjGetElements(NULL, av[k], &nv, &vv);
	if (nv > 1 && (Tcl_ListObjGetElements(NULL, MListContent(av[k + 1]),
		&nc, &cv) != TCL_OK || nc != nv)) {
	    MTF_FAIL(Tcl_ObjPrintf("{%s} needs as many columns",
		    Tcl_GetString(av[k])));
	}
	for (j = 0; j < nv; j++) {
	    struct MapPair *pr = &pairs[npairs];
	    Tcl_Obj *lo = MListContent((nv > 1) ? cv[j] : one), *vbase;
	    Tcl_Obj *const *vel;
	    Tcl_Size vn, vst;

	    pr->nvars = 1;
	    pr->estride = 1;
	    if (TclObjViewInfo(lo, &vbase, &vel, &vn, &vst)) {
		pr->list = vbase;
		pr->elems = (Tcl_Obj **) vel;
		pr->len = vn;
		pr->estride = vst;
	    } else if (TclCompactScalarInfo(lo, &pr->len, &pr->cdata,
		    &pr->cstride, &pr->ckind, &pr->csize)) {
		pr->list = lo;
	    } else if (TclHasInternalRep(lo, &tclListType)) {
		pr->list = lo;
		Tcl_ListObjGetElements(NULL, lo, &pr->len, &pr->elems);
	    } else {
		MTF_FAIL(Tcl_ObjPrintf("input %s is not a list yet",
			Tcl_GetString(vv[j])));
	    }
	    if (pr->cdata != NULL && pr->ckind != 2) {
		MTF_FAIL(Tcl_ObjPrintf("input %s is in integer cells",
			Tcl_GetString(vv[j])));
	    }
	    Tcl_IncrRefCount(pr->list);
	    pr->varList = vv[j];
	    Tcl_IncrRefCount(pr->varList);
	    pr->vars = &pr->varList;
	    npairs++;
	    if (n >= 0 && pr->len != n) {
		MTF_FAIL(Tcl_NewStringObj("the lists have different lengths",
			-1));
	    }
	    n = pr->len;
	}
    }
    if (n < 2) {
	MTF_FAIL(Tcl_NewStringObj("fewer than 2 elements", -1));
    }
    /* les K programmes */
    g = (MProg *) Tcl_Alloc(K * sizeof(MProg));
    dp = (MDProg *) Tcl_Alloc(K * sizeof(MDProg));
    vals = (MNum **) Tcl_Alloc(K * sizeof(MNum *));
    ccols = (double ***) Tcl_Alloc(K * sizeof(double **));
    memset(g, 0, K * sizeof(MProg));
    memset(dp, 0, K * sizeof(MDProg));
    memset(vals, 0, K * sizeof(MNum *));
    memset(ccols, 0, K * sizeof(double **));
    slotOf = (int *) Tcl_Alloc(npairs * sizeof(int));
    for (k = 0; k < npairs; k++) {
	slotOf[k] = -1;
    }
    for (j = 0; j < K; j++) {
	int root;

	g[j].p = Tcl_GetString(parts[j]);
	root = MExpr(&g[j]);
	MSkip(&g[j]);
	if (g[j].bad || *g[j].p != '\0' || MDType(&g[j], root) != 1) {
	    if (*g[j].p != '\0' && !g[j].bad) {
		MBAD(&g[j]);
	    }
	    if (rep != NULL && why == NULL && g[j].bad) {
		Tcl_Obj *nc = RepNotCompiled(&g[j], Tcl_GetString(parts[j]));

		Tcl_IncrRefCount(nc);
		why = Tcl_ObjPrintf("component %" TCL_SIZE_MODIFIER "d: %s", j,
			Tcl_GetString(nc));
		Tcl_IncrRefCount(why);
		Tcl_DecrRefCount(nc);
	    }
	    MTF_FAIL(Tcl_ObjPrintf("component %" TCL_SIZE_MODIFIER "d is not "
		    "a double expression", j));
	}
	MDCompile(&g[j], root, &dp[j]);
	if (dp[j].maxDepth > 32) {
	    MTF_FAIL(Tcl_ObjPrintf("component %" TCL_SIZE_MODIFIER "d is too "
		    "deep", j));
	}
	if (dp[j].maxDepth > maxDepth) {
	    maxDepth = dp[j].maxDepth;
	}
	vals[j] = (MNum *) Tcl_Alloc((g[j].nnames + 1) * sizeof(MNum));
	ccols[j] = (double **) Tcl_Alloc((g[j].nnames + 1) * sizeof(double *));
	for (s = 0; s < g[j].nnames; s++) {
	    Tcl_Size l;
	    const char *nm = Tcl_GetStringFromObj(g[j].names[s], &l);
	    Tcl_Size last = -1;

	    for (k = 0; k < npairs; k++) {
		Tcl_Size l2;
		const char *t = Tcl_GetStringFromObj(pairs[k].varList, &l2);

		if (l == l2 && memcmp(nm, t, l) == 0) {
		    last = k;		/* (la dernière l'emporte) */
		}
	    }
	    ccols[j][s] = NULL;
	    if (last >= 0) {
		slotOf[last] = (int) last;
		ccols[j][s] = (double *) (intptr_t) (last + 1);	/* (après) */
	    } else {
		/* une valeur fixe : un flottant, lu une fois */
		Tcl_Obj *o = Tcl_ObjGetVar2(interp, g[j].names[s], NULL, 0);

		if (o == NULL || !MNumOf(o, &vals[j][s])
			|| vals[j][s].isD != 1) {
		    MTF_FAIL(Tcl_ObjPrintf("variable %s is not a double",
			    Tcl_GetString(g[j].names[s])));
		}
	    }
	}
    }
    /* les colonnes rassemblées (une par variable lue) et les sorties */
    gcols = (double **) Tcl_Alloc(npairs * sizeof(double *));
    gmem = (double *) Tcl_Alloc((size_t) npairs * MD_BLK * sizeof(double));
    scratch = (double *) Tcl_Alloc(((size_t) maxDepth + 1) * MD_BLK
	    * sizeof(double));
    for (k = 0; k < npairs; k++) {
	gcols[k] = (slotOf[k] >= 0) ? gmem + (size_t) k * MD_BLK : NULL;
    }
    for (j = 0; j < K; j++) {
	for (s = 0; s < g[j].nnames; s++) {
	    if (ccols[j][s] != NULL) {
		ccols[j][s] = gcols[(intptr_t) ccols[j][s] - 1];
	    }
	}
    }
    if (sinkName != NULL) {
	/* -in : les variables d'abord (le dernier élément, connu), puis les
	 * cases neuves de la cible, écrites bloc par bloc */
	if (MapSetLastVars(interp, pairs, npairs, n) != TCL_OK) {
	    MTF_FAIL(Tcl_NewStringObj("a loop variable cannot be set", -1));
	}
	sink = TclTypePathCellsBegin(interp, sinkName, sinkPath, K, n, sdst,
		sstep, &swhy, &copied);
	if (sink == NULL) {
	    MTF_FAIL(Tcl_ObjPrintf("target: %s", swhy ? swhy : "?"));
	}
	tmp = (double *) Tcl_Alloc(MD_BLK * sizeof(double));
	goto pass;
    }
    lt = TclTypeParse(interp, "list.double", 11);
    if (lt == NULL) {
	Tcl_ResetResult(interp);
	goto done;
    }
    outData = (unsigned char **) Tcl_Alloc(K * sizeof(unsigned char *));
    for (j = 0; j < K; j++) {
	Tcl_Obj *cv = TclCompactNewAlways(lt, n, &outData[j]);

	if (cv == NULL || ((uintptr_t) outData[j] % sizeof(double)) != 0) {
	    if (cv != NULL) {
		Tcl_IncrRefCount(cv);
		Tcl_DecrRefCount(cv);
	    }
	    goto done;
	}
	outCols[j] = TclNewAnnotatedOwn(cv, TclTypeRefFor(interp, lt, -1));
	Tcl_IncrRefCount(outCols[j]);
	built++;
    }
    /* la passe : un rassemblement par bloc, K programmes dessus */
  pass:
    for (i = 0; i < n; i += MD_BLK) {
	int nb = (n - i > MD_BLK) ? MD_BLK : (int) (n - i);

	TypeMapGather(pairs, npairs, slotOf, gcols, NULL, i, nb, bbad, ibad);
	if (memchr(bbad, 1, nb) != NULL) {
	    MTF_FAIL(Tcl_ObjPrintf("element %" TCL_SIZE_MODIFIER "d is not a "
		    "double in every input", i + (Tcl_Size) ((unsigned char *)
		    memchr(bbad, 1, nb) - bbad)));
	}
	for (j = 0; j < K; j++) {
	    memset(cbad, 0, nb);
	    MDRunBlock(&dp[j], ccols[j], vals[j], nb, scratch, cbad,
		    sink ? tmp : (double *) outData[j] + i);
	    if (memchr(cbad, 1, nb) != NULL) {
		MTF_FAIL(Tcl_ObjPrintf("component %" TCL_SIZE_MODIFIER "d, "
			"element %" TCL_SIZE_MODIFIER "d: not a finite double",
			j, i + (Tcl_Size) ((unsigned char *)
			memchr(cbad, 1, nb) - cbad)));
	    }
	    if (sink != NULL) {
		/* à son pas dans la cible */
		unsigned char *d = sdst[j] + (size_t) i * sstep[j];
		size_t st = sstep[j];
		int e;

		if (st == 8) {
		    memcpy(d, tmp, (size_t) nb * 8);
		} else if (((uintptr_t) d % 8) == 0) {
		    double *dd = (double *) d;
		    size_t s8 = st / 8;

		    for (e = 0; e < nb; e++) {
			dd[(size_t) e * s8] = tmp[e];
		    }
		} else {
		    for (e = 0; e < nb; e++) {
			memcpy(d + (size_t) e * st, &tmp[e], 8);
		    }
		}
	    }
	}
    }
    if (sink != NULL) {
	TclTypePathCellsEnd(sink, 1);	/* installée */
	sink = NULL;
	ok = 1;
	if (rep != NULL) {
	    RepSetObj(&rep->write, Tcl_NewStringObj(copied
		    ? "direct (copied first)" : "direct", -1));
	}
	goto done;
    }
    /* comme lmap : les variables gardent le dernier élément */
    if (MapSetLastVars(interp, pairs, npairs, n) != TCL_OK) {
	MTF_FAIL(Tcl_NewStringObj("a loop variable cannot be set", -1));
    }
    ok = 1;
  done:
#undef MTF_FAIL
    if (rep != NULL) {
	if (ok) {
	    Tcl_Obj *in = Tcl_NewListObj(0, NULL);
	    int c0 = rep->comp;

	    for (k = 0; k < npairs; k++) {
		Tcl_ListObjAppendElement(NULL, in, pairs[k].varList);
		Tcl_ListObjAppendElement(NULL, in, RepInputKind(pairs[k].cdata,
			pairs[k].ckind, pairs[k].csize, pairs[k].cstride,
			pairs[k].estride, pairs[k].elems != NULL
			&& pairs[k].estride != 1));
	    }
	    if (c0 <= 0) {
		RepSetObj(&rep->inputs, in);
	    } else {
		Tcl_IncrRefCount(in);
		Tcl_DecrRefCount(in);
	    }
	    rep->elements = n;
	    for (j = 0; j < K; j++) {
		rep->comp = (K > 1) ? (int) j : c0;
		RepBody(rep, Tcl_NewStringObj("compiled double", -1));
	    }
	    rep->comp = c0;
	    rep->blocks += (Tcl_WideInt) n * K;
	    if (rep->pass == NULL || K > 1 || sinkName != NULL) {
		rep->pass = (K > 1) ? "single" : (rep->pass ? rep->pass
			: "column");
	    }
	} else if (why != NULL && (K > 1 || sinkName != NULL)) {
	    RepWhy(rep, Tcl_ObjPrintf("%s: %s", (sinkName != NULL)
		    ? "direct write" : "single pass", Tcl_GetString(why)));
	}
    }
    if (why != NULL) {
	Tcl_DecrRefCount(why);
    }
    if (sink != NULL) {
	TclTypePathCellsEnd(sink, 0);	/* abandonnée : rien n'a changé */
    }
    if (tmp != NULL) {
	Tcl_Free(tmp);
    }
    if (!ok) {
	for (j = 0; j < built; j++) {
	    Tcl_DecrRefCount(outCols[j]);
	    outCols[j] = NULL;
	}
    }
    if (outData != NULL) {
	Tcl_Free(outData);
    }
    if (gcols != NULL) {
	Tcl_Free(gcols);
	Tcl_Free(gmem);
	Tcl_Free(scratch);
    }
    if (g != NULL) {
	for (j = 0; j < K; j++) {
	    MProgFree(&g[j]);
	    if (dp[j].ops) {
		Tcl_Free(dp[j].ops);
	    }
	    if (vals[j]) {
		Tcl_Free(vals[j]);
	    }
	    if (ccols[j]) {
		Tcl_Free(ccols[j]);
	    }
	}
	Tcl_Free(g);
	Tcl_Free(dp);
	Tcl_Free(vals);
	Tcl_Free(ccols);
    }
    if (slotOf != NULL) {
	Tcl_Free(slotOf);
    }
    for (k = 0; k < npairs; k++) {
	Tcl_DecrRefCount(pairs[k].list);
	Tcl_DecrRefCount(pairs[k].varList);
    }
    if (pairs != NULL) {
	Tcl_Free(pairs);
    }
    return ok;
}

/*
 * 24/11 (v17r) : les boucles vectorisables. foreach et lmap demandent au
 * registre, à chaque appel, si une entrée sait faire cet appel-là ; une
 * entrée rend TCL_CONTINUE si elle ne sait pas (la boucle ordinaire suit),
 * sinon son résultat. Les entrées sont essayées par priorité décroissante.
 * L'entrée intégrée s'appuie sur type map : elle n'accepte que ce qu'elle
 * fait à l'identique de la boucle.
 */
typedef struct VecEntry {
    const char *name;
    TclVecEachloopProc *proc;
    int priority;
} VecEntry;

static VecEntry vecReg[16];
static int nVec = 0;
TCL_DECLARE_MUTEX(vecMutex)

int
TclVecRegister(const char *name, TclVecEachloopProc *proc, int priority)
{
    int k, j;

    Tcl_MutexLock(&vecMutex);
    for (k = 0; k < nVec; k++) {
	if (strcmp(vecReg[k].name, name) == 0) {
	    vecReg[k].proc = proc;	/* (remplace) */
	    vecReg[k].priority = priority;
	    Tcl_MutexUnlock(&vecMutex);
	    return TCL_OK;
	}
    }
    if (nVec == 16) {
	Tcl_MutexUnlock(&vecMutex);
	return TCL_ERROR;
    }
    for (k = 0; k < nVec && vecReg[k].priority >= priority; k++) {
    }
    for (j = nVec; j > k; j--) {
	vecReg[j] = vecReg[j - 1];
    }
    vecReg[k].name = name;
    vecReg[k].proc = proc;
    vecReg[k].priority = priority;
    nVec++;
    Tcl_MutexUnlock(&vecMutex);
    return TCL_OK;
}

static TclVecEachloopProc VecTypeMap;

int
TclVecEachloop(Tcl_Interp *interp, int isLmap, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    int k, rc;
    static int init = 0;

    if (!init) {
	TclVecRegister("typemap", VecTypeMap, 0);
	init = 1;
    }
    for (k = 0; k < nVec; k++) {
	rc = vecReg[k].proc(interp, isLmap, objc, objv);
	if (rc != TCL_CONTINUE) {
	    return rc;
	}
    }
    return TCL_CONTINUE;
}

/* le noeud rend-il toujours une valeur neuve (un calcul), jamais un de
 * ses opérandes tel quel ($x, ?:, min, max, abs, int, double, +x) ? */
static int
MFresh(const MProg *g, int i)
{
    const MNode *nd = &g->n[i];

    switch (nd->kind) {
    case MN_POS:
	return MFresh(g, nd->a);	/* (des parenthèses, ou +x) */
    case MN_VAR: case MN_CONST: case MN_COND:
	return 0;
    case MN_FUNC:
	return !(nd->fn == MF_ABS || nd->fn == MF_DOUBLE || nd->fn == MF_INT
		|| nd->fn == MF_MIN || nd->fn == MF_MAX || nd->fn == MF_ROUND);
    default:
	return 1;
    }
}

/* le texte est-il une expression pure (le sous-ensemble compilé) ? Et rend-
 * elle toujours une valeur neuve (*freshPtr) ? */
static int
MapExprPure(const char *text, int *freshPtr)
{
    MProg g;
    int ok, root;

    memset(&g, 0, sizeof(g));
    g.p = text;
    root = MExpr(&g);
    MSkip(&g);
    ok = !g.bad && *g.p == '\0';
    if (freshPtr != NULL) {
	*freshPtr = ok && root >= 0 && MFresh(&g, root);
    }
    MProgFree(&g);
    return ok;
}

/* une commande du nom donné, telle qu'elle est résolue ici, est-elle la
 * commande intégrée ? */
static int
VecBuiltin(Tcl_Interp *interp, const char *name, Tcl_ObjCmdProc *proc)
{
    Tcl_CmdInfo info;

    return Tcl_GetCommandInfo(interp, name, &info) && info.objProc == proc;
}

/* le mot d'un jeton : son texte s'il est simple (un seul jeton texte, en
 * accolades ou nu) ; NULL sinon */
static Tcl_Obj *
VecSimpleWord(Tcl_Token *t, int *braced)
{
    if (t->type != TCL_TOKEN_SIMPLE_WORD || t->numComponents != 1) {
	return NULL;
    }
    *braced = (t->start[0] == '{');
    return Tcl_NewStringObj(t[1].start, t[1].size);
}

static int VecBodyForm(Tcl_Interp *interp, int isLmap, Tcl_Obj *body,
	Tcl_Obj **exprPtr, Tcl_Obj **foldPtr, int *assignPtr);

/* un nom de variable simple : lettres, chiffres, _ */
static int
VecPlainName(const char *p, Tcl_Size n)
{
    Tcl_Size i;

    if (n <= 0) {
	return 0;
    }
    for (i = 0; i < n; i++) {
	if (!(isalnum(UCHAR(p[i])) || p[i] == '_')) {
	    return 0;
	}
    }
    return 1;
}

/*
 * incr s ?E? (foreach) : E est 1, un entier écrit, $v, ou [expr {E}].
 * L'expression rendue : ($s + E) ; l'appelant vérifie que tout reste entier
 * (sinon la boucle ordinaire, qui donne l'erreur d'incr).
 */
static int
VecIncrForm(Tcl_Interp *interp, Tcl_Parse *pp, Tcl_Obj **exprPtr,
	Tcl_Obj **foldPtr)
{
    Tcl_Token *t0 = pp->tokenPtr, *t1, *t2;
    Tcl_Obj *w0, *w1 = NULL, *add = NULL;
    int braced, ok = 0;

    w0 = VecSimpleWord(t0, &braced);
    if (w0 == NULL || strcmp(Tcl_GetString(w0), "incr") != 0
	    || !VecBuiltin(interp, "incr", Tcl_IncrObjCmd)) {
	goto done;
    }
    t1 = t0 + 1 + t0->numComponents;
    w1 = VecSimpleWord(t1, &braced);
    if (w1 == NULL || braced || !VecPlainName(Tcl_GetString(w1),
	    (Tcl_Size) strlen(Tcl_GetString(w1)))) {
	goto done;
    }
    if (pp->numWords == 2) {
	add = Tcl_NewStringObj("1", 1);
    } else {
	Tcl_Obj *lit;
	Tcl_WideInt w;

	t2 = t1 + 1 + t1->numComponents;
	if ((lit = VecSimpleWord(t2, &braced)) != NULL) {
	    /* un entier écrit (décimal, sans espace) */
	    const char *q = Tcl_GetString(lit);
	    const char *r = (*q == '-' || *q == '+') ? q + 1 : q;

	    if (!braced && *r != '\0' && strspn(r, "0123456789")
		    == strlen(r) && (*r != '0' || r[1] == '\0')
		    && Tcl_GetWideIntFromObj(NULL, lit, &w) == TCL_OK) {
		add = Tcl_ObjPrintf("%" TCL_LL_MODIFIER "d", (long long) w);
	    }
	    TclDiscard(lit);
	} else if (t2->type == TCL_TOKEN_WORD && t2->numComponents == 2
		&& t2[1].type == TCL_TOKEN_VARIABLE
		&& t2[1].numComponents == 1 && t2[2].type == TCL_TOKEN_TEXT
		&& VecPlainName(t2[2].start, t2[2].size)) {
	    /* $v */
	    add = Tcl_NewStringObj(t2[1].start, t2[1].size);
	} else if (t2->type == TCL_TOKEN_WORD && t2->numComponents == 1
		&& t2[1].type == TCL_TOKEN_COMMAND) {
	    /* [expr {E}] */
	    Tcl_Obj *inner = Tcl_NewStringObj(t2[1].start + 1,
		    t2[1].size - 2), *ie = NULL, *dummy = NULL;
	    int da;

	    Tcl_IncrRefCount(inner);
	    if (VecBodyForm(interp, 1, inner, &ie, &dummy, &da) && ie != NULL
		    && Tcl_GetString(inner)[0] != '(') {
		add = ie;
	    } else if (ie != NULL) {
		TclDiscard(ie);
	    }
	    Tcl_DecrRefCount(inner);
	}
    }
    if (add != NULL) {
	*exprPtr = Tcl_ObjPrintf("($%s + %s)", Tcl_GetString(w1),
		Tcl_GetString(add));
	*foldPtr = w1;
	w1 = NULL;
	TclDiscard(add);
	ok = 1;
    }
  done:
    if (w1 != NULL) {
	TclDiscard(w1);
    }
    if (w0 != NULL) {
	TclDiscard(w0);
    }
    return ok;
}

/*
 * Le corps : (E), ou expr {E} (lmap) ; (s = E), ou set s [expr {E}]
 * (foreach). *exprPtr reçoit « (E) », *foldPtr le nom de s (foreach). 0 si
 * le corps n'a pas une de ces formes.
 */
static int
VecBodyForm(Tcl_Interp *interp, int isLmap, Tcl_Obj *body, Tcl_Obj **exprPtr,
	Tcl_Obj **foldPtr, int *assignPtr)
{
    Tcl_Size len;
    const char *b = Tcl_GetStringFromObj(body, &len), *e = b + len, *p;
    Tcl_Parse parse;
    int depth = 0, braced, ok = 0;

    *exprPtr = *foldPtr = NULL;
    *assignPtr = 0;
    while (b < e && isspace(UCHAR(*b))) {
	b++;
    }
    while (e > b && isspace(UCHAR(e[-1]))) {
	e--;
    }
    if (e - b >= 2 && *b == '(' && e[-1] == ')') {
	/* (E) ou (s = E) : la parenthèse ouvrante ferme à la fin, pas de ; */
	const char *in = b + 1, *ie = e - 1, *eq = NULL;

	for (p = b; p < e; p++) {
	    if (*p == '(' || *p == '[' || *p == '{') {
		depth++;
	    } else if (*p == ')' || *p == ']' || *p == '}') {
		if (--depth == 0 && p != e - 1) {
		    return 0;
		}
	    } else if (depth == 1 && (*p == ';' || *p == '"' || *p == '\\')) {
		return 0;
	    } else if (depth == 1 && *p == '=' && eq == NULL
		    && p[1] != '=' && p > in && p[-1] != '=' && p[-1] != '!'
		    && p[-1] != '<' && p[-1] != '>') {
		eq = p;
	    }
	}
	if (eq == NULL) {
	    if (!isLmap) {
		return 0;
	    }
	    *exprPtr = Tcl_NewStringObj(b, e - b);
	    return 1;
	}
	if (isLmap) {
	    return 0;
	}
	{
	    const char *n = in, *ne = eq;

	    while (n < ne && isspace(UCHAR(*n))) {
		n++;
	    }
	    while (ne > n && isspace(UCHAR(ne[-1]))) {
		ne--;
	    }
	    for (p = n; p < ne; p++) {
		if (!(isalnum(UCHAR(*p)) || *p == '_')) {
		    return 0;
		}
	    }
	    if (n == ne) {
		return 0;
	    }
	    *foldPtr = Tcl_NewStringObj(n, ne - n);
	    *exprPtr = Tcl_ObjPrintf("(%.*s)", (int) (ie - eq - 1), eq + 1);
	    *assignPtr = 1;		/* (s = E) : E tel qu'il est rendu */
	    return 1;
	}
    }
    /* expr {E} ; set s [expr {E}] */
    if (Tcl_ParseCommand(NULL, b, e - b, 0, &parse) != TCL_OK) {
	return 0;
    }
    if (parse.term < e - 1 || parse.commentSize > 0) {
	goto parsedone;		/* plus d'une commande */
    }
    if (isLmap && parse.numWords == 2) {
	Tcl_Obj *w0 = VecSimpleWord(parse.tokenPtr, &braced), *w1;

	if (w0 != NULL && strcmp(Tcl_GetString(w0), "expr") == 0
		&& VecBuiltin(interp, "expr", Tcl_ExprObjCmd)) {
	    Tcl_Token *t1 = parse.tokenPtr + 1 + parse.tokenPtr->numComponents;

	    w1 = VecSimpleWord(t1, &braced);
	    if (w1 != NULL && braced) {
		*exprPtr = Tcl_ObjPrintf("(%s)", Tcl_GetString(w1));
		ok = 1;
	    }
	    if (w1 != NULL) {
		TclDiscard(w1);
	    }
	}
	if (w0 != NULL) {
	    TclDiscard(w0);
	}
    } else if (!isLmap && (parse.numWords == 2 || parse.numWords == 3)
	    && VecIncrForm(interp, &parse, exprPtr, foldPtr)) {
	/* incr s ?E? : un cumul d'entiers, ($s + E) */
	*assignPtr = 2;
	ok = 1;
    } else if (!isLmap && parse.numWords == 3) {
	Tcl_Token *t0 = parse.tokenPtr, *t1, *t2;
	Tcl_Obj *w0 = VecSimpleWord(t0, &braced), *w1 = NULL;

	t1 = t0 + 1 + t0->numComponents;
	t2 = t1 + 1 + t1->numComponents;
	if (w0 != NULL && strcmp(Tcl_GetString(w0), "set") == 0
		&& VecBuiltin(interp, "set", Tcl_SetObjCmd)
		&& (w1 = VecSimpleWord(t1, &braced)) != NULL && !braced
		&& t2->type == TCL_TOKEN_WORD && t2->numComponents == 1
		&& t2[1].type == TCL_TOKEN_COMMAND) {
	    /* [expr {E}] : le script entre crochets, une commande */
	    Tcl_Obj *inner = Tcl_NewStringObj(t2[1].start + 1,
		    t2[1].size - 2);
	    Tcl_Obj *ie = NULL, *dummy = NULL;
	    int da;

	    Tcl_IncrRefCount(inner);
	    if (VecBodyForm(interp, 1, inner, &ie, &dummy, &da) && ie != NULL
		    && Tcl_GetString(inner)[0] != '(') {
		*exprPtr = ie;
		*foldPtr = w1;
		w1 = NULL;
		ok = 1;
	    } else if (ie != NULL) {
		TclDiscard(ie);
	    }
	    Tcl_DecrRefCount(inner);
	}
	if (w1 != NULL) {
	    TclDiscard(w1);
	}
	if (w0 != NULL) {
	    TclDiscard(w0);
	}
    }
  parsedone:
    Tcl_FreeParse(&parse);
    return ok;
}

/* une variable que la boucle peut poser sans témoin : pas d'élément de
 * tableau, pas un tableau, pas de trace, pas de type ni de contrat */
static int
VecPlainVar(Tcl_Interp *interp, Tcl_Obj *name)
{
    Var *varPtr, *arrayPtr = NULL;
    const char *n = Tcl_GetString(name);

    if (strchr(n, '(') != NULL || *n == '\0') {
	return 0;
    }
    varPtr = TclObjLookupVarEx(interp, name, NULL, 0, NULL, 0, 0, &arrayPtr);
    if (varPtr == NULL) {
	return 1;			/* (pas encore là : créée sans trace) */
    }
    if (TclIsVarArray(varPtr) || TclIsVarTraced(varPtr)
	    || (arrayPtr != NULL && TclIsVarTraced(arrayPtr))
	    || (varPtr->flags & (VAR_TYPED | VAR_CONTRACT))) {
	return 0;
    }
    return 1;
}

#define VEC_MIN 16		/* en dessous : la boucle ordinaire */

/* un entier (grand ou non), au sens d'incr */
static int
VecIsInt(Tcl_Obj *o)
{
    void *p;
    int t;

    if (Tcl_GetNumberFromObj(NULL, o, &p, &t) != TCL_OK) {
	return 0;
    }
    return t == TCL_NUMBER_INT || t == TCL_NUMBER_BIG;
}

static int VecTypeMapRun(Tcl_Interp *interp, int isLmap, Tcl_Size objc,
	Tcl_Obj *const objv[], const char **whyPtr);

/*
 * Les listes de variables, copiées : le mot peut être l'objet-nom d'une
 * variable locale compilée (un littéral partagé), dont la recherche de
 * variable efface la représentation interne (tclVar.c, [80304238ac]) ;
 * ses éléments de liste disparaîtraient sous nos pieds.
 */
static int
VecTypeMap(Tcl_Interp *interp, int isLmap, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    Tcl_Obj *small[16], **ov = (objc <= 16) ? small
	    : (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) objc);
    Tcl_Size k;
    int rc;

    MapRep rep, *save;
    MapLog *l;
    const char *why = NULL;

    for (k = 0; k < objc; k++) {
	ov[k] = (k % 2 == 1 && k < objc - 1) ? Tcl_DuplicateObj(objv[k])
		: objv[k];
	Tcl_IncrRefCount(ov[k]);
    }
    l = MapRepEnter(interp, &rep, &save, 0);
    rc = VecTypeMapRun(interp, isLmap, objc, ov, &why);
    MapRepLeave(save);
    if (l != NULL) {
	/* v17y : le relevé global (une boucle candidate seulement) */
	if (rc == TCL_OK || rc == TCL_ERROR) {
	    MapLogAdd(interp, l, isLmap ? "lmap" : "foreach", &rep, NULL);
	} else if (why != NULL) {
	    Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);

	    MapLogAdd(interp, l, isLmap ? "lmap" : "foreach", NULL, why);
	    Tcl_RestoreInterpState(interp, st);
	}
	MapRepFree(&rep);
    }
    for (k = 0; k < objc; k++) {
	Tcl_DecrRefCount(ov[k]);
    }
    if (ov != small) {
	Tcl_Free(ov);
    }
    return rc;
}

static int
VecTypeMapRun(Tcl_Interp *interp, int isLmap, Tcl_Size objc,
	Tcl_Obj *const objv[], const char **whyPtr)
{
    Tcl_Obj *ex = NULL, *fv = NULL, **av = NULL, *init = NULL;
    Tcl_Size k, n = -1, len, ac = 0;
    int rc = TCL_CONTINUE, assign = 0, fresh = 0;

    if (TclTypeOpsMode() == TCL_TYPE_OPS_OFF
	    || getenv("TCL_TYPE_NOVECLOOP") != NULL || objc < 4
	    || (objc % 2) != 0) {
	return TCL_CONTINUE;
    }
    if (!VecBodyForm(interp, isLmap, objv[objc - 1], &ex, &fv, &assign)) {
	return TCL_CONTINUE;
    }
    Tcl_IncrRefCount(ex);
    if (fv != NULL) {
	Tcl_IncrRefCount(fv);
    }
    if (!MapExprPure(Tcl_GetString(ex), &fresh)) {
	*whyPtr = "the expression is not in the compiled subset";
	goto decline;
    }
    if (assign == 1 && !fresh) {
	/* (s = E) garde un opérande tel quel ($u, ?:...), sans le
	 * normaliser comme expr : la boucle ordinaire */
	*whyPtr = "(s = E) where E may be an operand as is";
	goto decline;
    }
    /* une variable par liste, des listes de même longueur, assez longues */
    for (k = 1; k < objc - 1; k += 2) {
	Tcl_Obj **vv;
	Tcl_Size nv;

	if (Tcl_ListObjGetElements(NULL, objv[k], &nv, &vv) != TCL_OK
		|| nv != 1 || !VecPlainVar(interp, vv[0])
		|| (fv != NULL && strcmp(Tcl_GetString(vv[0]),
		    Tcl_GetString(fv)) == 0)
		|| Tcl_ListObjLength(NULL, objv[k + 1], &len) != TCL_OK
		|| (n >= 0 && len != n)) {
	    *whyPtr = "one simple variable per list, lists of one length";
	    goto decline;
	}
	n = len;
    }
    if (n < VEC_MIN) {
	*whyPtr = "short lists";
	goto decline;
    }
    if (fv != NULL) {
	/* le cumul : s doit exister (sa valeur de départ), sans témoin */
	if (!VecPlainVar(interp, fv)
		|| (init = Tcl_ObjGetVar2(interp, fv, NULL, 0)) == NULL) {
	    *whyPtr = "the accumulator does not exist (or is traced)";
	    goto decline;
	}
	Tcl_IncrRefCount(init);
	if (assign == 2 && !VecIsInt(init)) {
	    *whyPtr = "incr: the accumulator is not an integer";
	    goto decline;		/* incr : s entier au départ */
	}
    }
    /* type map ?-fold {s init}? {v} L ... (E) */
    av = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) (objc + 4));
    av[ac++] = objv[0];
    av[ac++] = objv[0];
    for (k = 1; k < objc - 1; k++) {
	av[ac++] = objv[k];
    }
    av[ac++] = ex;
    if (fv != NULL) {
	MapFold f;

	f.name = fv;
	f.init = init;
	rc = TypeMapList(interp, ac, av, &f);
    } else {
	rc = TypeMapList(interp, ac, av, NULL);
    }
    if (rc == TCL_OK && assign == 2) {
	/* incr : un cumul resté entier, sinon la boucle ordinaire (l'erreur
	 * d'incr sur l'élément qui n'est pas entier) */
	Tcl_Obj *fin = Tcl_ObjGetVar2(interp, fv, NULL, 0);

	if (fin == NULL || !VecIsInt(fin)) {
	    rc = TCL_ERROR;
	}
    }
    if (rc != TCL_OK) {
	/* l'erreur telle que la boucle la donne : la boucle ordinaire,
	 * depuis le début (le corps est pur ; s remis à son départ) */
	if (fv != NULL) {
	    Tcl_ObjSetVar2(interp, fv, NULL, init, 0);
	}
	Tcl_ResetResult(interp);
	rc = TCL_CONTINUE;
	*whyPtr = "an element errs: the ordinary loop, from the start";
    } else if (isLmap) {
	/* la liste des résultats, sans annotation (comme lmap) */
	Tcl_Obj *r = Tcl_GetObjResult(interp), *c = NULL;

	if (TclAnnotInfo(r, &c, NULL, NULL, NULL) != 0 && c != NULL) {
	    Tcl_SetObjResult(interp, c);
	}
    } else {
	Tcl_ResetResult(interp);	/* foreach ne rend rien */
    }
    Tcl_Free(av);
  decline:
    if (init != NULL) {
	Tcl_DecrRefCount(init);
    }
    if (fv != NULL) {
	Tcl_DecrRefCount(fv);
    }
    Tcl_DecrRefCount(ex);
    return rc;
}

/*
 * Le niveau compilation (v17s). TclVecCompilable : le corps a-t-il une
 * forme que le registre peut prendre ? (À la compilation : seules ces
 * boucles paient l'essai.) Le reste se décide à l'exécution, par
 * ::tcl::unsupported::vecloop isLmap v1 L1 ?v2 L2 ...? body, qui rend
 * {1 résultat} si la boucle a été faite, ou {0 L1 L2 ...} : les listes,
 * déjà évaluées, pour la boucle ordinaire.
 */
int
TclVecCompilable(Tcl_Interp *interp, int isLmap, const char *body,
	Tcl_Size len)
{
    Tcl_Obj *b = Tcl_NewStringObj(body, len), *ex = NULL, *fv = NULL;
    int assign = 0, fresh = 0, ok;

    Tcl_IncrRefCount(b);
    ok = VecBodyForm(interp, isLmap, b, &ex, &fv, &assign);
    if (ok) {
	Tcl_IncrRefCount(ex);
	ok = MapExprPure(Tcl_GetString(ex), &fresh)
		&& !(assign == 1 && !fresh);
	Tcl_DecrRefCount(ex);
    }
    if (fv != NULL) {
	TclDiscard(fv);
    }
    Tcl_DecrRefCount(b);
    return ok;
}

static int
VecLoopObjCmd(TCL_UNUSED(void *), Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    int isLmap, rc;
    Tcl_Size k, nl = 0;
    Tcl_Obj **av, *r;

    if (objc < 5 || (objc % 2) != 1
	    || Tcl_GetBooleanFromObj(interp, objv[1], &isLmap) != TCL_OK) {
	Tcl_WrongNumArgs(interp, 1, objv, "isLmap varList list "
		"?varList list ...? body");
	return TCL_ERROR;
    }
    /* d'abord le moins cher : assez d'éléments ? */
    for (k = 3; k < objc - 1; k += 2) {
	Tcl_Size len;

	if (Tcl_ListObjLength(NULL, objv[k], &len) == TCL_OK && len > nl) {
	    nl = len;
	}
    }
    rc = TCL_CONTINUE;
    if (nl >= VEC_MIN) {
	av = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) (objc - 1));
	av[0] = Tcl_NewStringObj(isLmap ? "lmap" : "foreach", -1);
	Tcl_IncrRefCount(av[0]);
	memcpy(av + 1, objv + 2, sizeof(Tcl_Obj *) * (size_t) (objc - 2));
	rc = TclVecEachloop(interp, isLmap, objc - 1, av);
	if (rc != TCL_OK && rc != TCL_ERROR) {
	    rc = TCL_CONTINUE;	/* (jamais break ni continue ici) */
	}
	Tcl_DecrRefCount(av[0]);
	Tcl_Free(av);
    }
    if (rc == TCL_CONTINUE) {
	Tcl_Obj *z = Tcl_NewWideIntObj(0);

	r = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(NULL, r, z);
	for (k = 3; k < objc - 1; k += 2) {
	    Tcl_ListObjAppendElement(NULL, r, objv[k]);
	}
	Tcl_ResetResult(interp);
	Tcl_SetObjResult(interp, r);
	return TCL_OK;
    }
    if (rc != TCL_OK) {
	return rc;
    }
    {
	Tcl_Obj *pair[2];

	pair[0] = Tcl_NewWideIntObj(1);
	pair[1] = Tcl_GetObjResult(interp);
	r = Tcl_NewListObj(2, pair);
	Tcl_SetObjResult(interp, r);
    }
    return TCL_OK;
}

/*
 * type repeat T N ?valeur? (v17t) : une liste list.T de N fois la valeur ;
 * type repeat -into L N ?valeur? : une valeur de type L (son élément ; une
 * longueur fixée par L est vérifiée). Par défaut, le zéro de l'élément (0,
 * ou 0.0 pour un flottant ; demandé sinon). La valeur est vérifiée une
 * fois ; toujours en cases quand l'élément en a une forme : codée une fois
 * et recopiée par blocs (TclCompactRepeat). Sinon la même valeur partagée.
 */
static int
TypeRepeatCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    TclType *tp, *et;
    Tcl_Size n, len, k;
    const char *tn;
    Tcl_Obj *v, *c = NULL, *content = NULL, *res = NULL, *z = NULL;
    int rc = TCL_ERROR;

    int into = (objc >= 3 && strcmp(Tcl_GetString(objv[2]), "-into") == 0);

    if (into) {
	objv++;
	objc--;			/* -into L : L à la place de T */
    }
    if (objc != 4 && objc != 5) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("wrong # args: should be "
		"\"type repeat type count ?value?\" or \"type repeat -into "
		"listType count ?value?\"", -1));
	Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
	return TCL_ERROR;
    }
    tn = Tcl_GetStringFromObj(objv[2], &len);
    if ((tp = TclTypeParse(interp, tn, len)) == NULL) {
	return TCL_ERROR;
    }
    if (!into) {
	et = tp;
	if ((tp = TclTypeListOf(interp, et)) == NULL) {
	    return TCL_ERROR;
	}
    } else if (tp->kind != TCLT_LIST || (et = tp->elem) == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"type repeat: \"%s\" is not a list type", tn));
	return TCL_ERROR;
    }
    if (Tcl_GetSizeIntFromObj(interp, objv[3], &n) != TCL_OK) {
	return TCL_ERROR;
    }
    if (n < 0) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"type repeat: bad count \"%s\"", Tcl_GetString(objv[3])));
	return TCL_ERROR;
    }
    if ((tp->arity >= 0 && n != tp->arity) || (tp->lenRange
	    && (n < tp->lenMin || (tp->lenMax >= 0 && n > tp->lenMax)))) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"type repeat: %" TCL_SIZE_MODIFIER "d elements do not fit "
		"in %s", n, tn));
	return TCL_ERROR;
    }
    if (objc == 5) {
	v = objv[4];
    } else {
	/* le zéro d'un élément numérique : refuse un mot, accepte 0 */
	Tcl_Obj *w = Tcl_NewStringObj("x", 1), *h = Tcl_NewStringObj("0.5", 3);

	Tcl_IncrRefCount(w);
	Tcl_IncrRefCount(h);
	z = Tcl_NewStringObj("0", 1);
	Tcl_IncrRefCount(z);
	if (TclTypeValidate(NULL, et, w, -1, NULL) == TCL_OK
		|| TclTypeValidate(NULL, et, z, -1, NULL) != TCL_OK) {
	    Tcl_DecrRefCount(w);
	    Tcl_DecrRefCount(h);
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "type repeat: a value is needed for elements of type %s",
		    et->name));
	    goto done;
	}
	if (TclTypeValidate(NULL, et, h, -1, NULL) == TCL_OK) {
	    Tcl_DecrRefCount(z);
	    z = Tcl_NewStringObj("0.0", 3);
	    Tcl_IncrRefCount(z);
	}
	Tcl_DecrRefCount(w);
	Tcl_DecrRefCount(h);
	v = z;
    }
    if ((c = TclTypedConform(interp, et, v)) == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("type repeat: value: %s",
		Tcl_GetString(Tcl_GetObjResult(interp))));
	goto done;
    }
    Tcl_IncrRefCount(c);
    content = TclCompactRepeat(tp, c, n);
    if (content != NULL) {
	res = TclNewAnnotatedOwn(content, TclTypeRefFor(interp, tp, -1));
    } else {
	/* des objets : la même valeur partagée, la liste mise au type L
	 * (éléments identiques : vérifiés une fois, ValidateG) */
	Tcl_Obj **ev = NULL, *lst, *cc, *bare = NULL;
	TclType *at = NULL;

	if (TclIsAnnotated(c) && TclAnnotNotes(c) == NULL
		&& TclAnnotInfo(c, &bare, &at, NULL, NULL) == 1 && at == et
		&& bare != NULL && !TclIsAnnotated(bare)) {
	    /* l'élément de son propre type, sans notes : son contenu nu,
	     * comme dans une liste typée */
	    Tcl_IncrRefCount(bare);
	    Tcl_DecrRefCount(c);
	    c = bare;
	}
	if (n > 0) {
	    ev = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (size_t) n);
	    for (k = 0; k < n; k++) {
		ev[k] = c;
	    }
	}
	lst = Tcl_NewListObj(n, ev);
	if (ev != NULL) {
	    Tcl_Free(ev);
	}
	Tcl_IncrRefCount(lst);
	if (!TclIsAnnotated(c)) {
	    /* un élément nu, déjà vérifié, et la longueur aussi : la liste
	     * neuve prise telle quelle */
	    res = TclNewAnnotatedOwn(lst, TclTypeRefFor(interp, tp, -1));
	    Tcl_DecrRefCount(lst);
	    goto set;
	}
	cc = TclTypedConform(interp, tp, lst);
	if (cc == NULL) {
	    Tcl_DecrRefCount(lst);
	    goto done;
	}
	if (cc == lst && lst->refCount == 1 && TclIsAnnotated(cc) == 0) {
	    /* la liste neuve, à nous seuls : prise telle quelle */
	    res = TclNewAnnotatedOwn(lst, TclTypeRefFor(interp, tp, -1));
	    Tcl_DecrRefCount(lst);
	} else {
	    Tcl_IncrRefCount(cc);
	    res = TclNewAnnotated(interp, tp, -1, cc);
	    Tcl_DecrRefCount(cc);
	    Tcl_DecrRefCount(lst);
	}
    }
  set:
    Tcl_SetObjResult(interp, res);
    rc = TCL_OK;
  done:
    if (c != NULL) {
	Tcl_DecrRefCount(c);
    }
    if (z != NULL) {
	Tcl_DecrRefCount(z);
    }
    return rc;
}

static int
TypeObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    TypeData *d = (TypeData *) clientData;
    const char *sub;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?arg ...?");
	return TCL_ERROR;
    }
    if (objc == 2) {
	/* type {T}mot (v16m) : le cast explicite ; la commande reçoit le mot
	 * et résout son annotation. */
	Tcl_Obj *res = NULL;
	int urc = CastWordUnfold(interp, objv[1]);	/* v16r : {*}T, {**}T */

	if (urc != TCL_CONTINUE) {
	    return urc;
	}
	switch (TclAnnotCastWord(interp, objv[1], &res)) {
	case TCL_OK:
	    Tcl_SetObjResult(interp, res);
	    return TCL_OK;
	case TCL_ERROR:
	    return TCL_ERROR;
	}
    }
    sub = Tcl_GetString(objv[1]);
    if (strcmp(sub, "def") == 0 || strcmp(sub, "define") == 0
	    || strcmp(sub, "alias") == 0) {
	int isAlias = (strcmp(sub, "alias") == 0), compact = 0, rc;
	Tcl_Obj *words, *nameObj, **wv;
	TclObjVec keep = {NULL, 0, 0};
	Tcl_Size nw, k;

	if (objc == 3 && !isAlias) {
	    return TypeDefShow(interp, objv[2]);	/* v16o */
	}
	if (objc > 4 && !isAlias) {
	    return TypeDefForms(interp, d, objc, objv);	/* 24/11 */
	}
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, isAlias ? "name definition"
		    : "name ?definition?");
	    return TCL_ERROR;
	}
	/* type def {-compact}T ... : options sur le nom du type */
	LocalWords(objv[2], &words, &nameObj, &keep);
	Tcl_IncrRefCount(words);
	Tcl_ListObjGetElements(NULL, words, &nw, &wv);
	for (k = 0; k < nw; k++) {
	    const char *w = Tcl_GetString(wv[k]);

	    const char *dot = strchr(w, '.'), *eq = strchr(w, '=');
	    size_t base = dot ? (size_t) (dot - w)
		    : eq ? (size_t) (eq - w) : strlen(w);

	    if (!isAlias && (dot != NULL || eq != NULL)
		    && base == 8 && strncmp(w, "-compact", 8) == 0) {
		ERR(interp, "OPTION", "type def: option {%.*s} takes no "
			"sub-option or value: {%s}", (int) base, w, w);
		Tcl_DecrRefCount(words);
		TclVecFree(&keep);
		return TCL_ERROR;
	    }
	    if (strcmp(w, "-compact") == 0 && !isAlias) {
		compact = 1;
	    } else if ((strcmp(w, "-strict") == 0
		    || strcmp(w, "-deduce") == 0) && !isAlias) {
		/* v16w : il n'y a plus de régimes de somme */
		ERR(interp, "OPTION", "type def: {%s} is gone: a sum (\",\") "
			"always writes its variant in its value (v 5), and a "
			"union (\"|\") must be disjoint, so nothing is ever "
			"guessed", w);
		Tcl_DecrRefCount(words);
		TclVecFree(&keep);
		return TCL_ERROR;
	    } else {
		ERR(interp, "OPTION", "type %s: unknown option {%s}%s", sub, w,
			isAlias ? " (an alias is not a new type: it takes no "
			"option)" : ": must be {-compact}");
		Tcl_DecrRefCount(words);
		TclVecFree(&keep);
		return TCL_ERROR;
	    }
	}
	Tcl_DecrRefCount(words);
	if (compact && strchr(Tcl_GetString(nameObj), '(') != NULL) {
	    ERR(interp, "OPTION", "type def {-compact}: not yet for a type "
		    "family (%s)", Tcl_GetString(nameObj));
	    TclVecFree(&keep);
	    return TCL_ERROR;
	}
	{
	    /* v16u : le nom sous lequel le type est rangé (::geom::point) */
	    Tcl_DString qd;

	    if (TclTypesQualify(interp, d, Tcl_GetString(nameObj), &qd)
		    != TCL_OK) {
		TclVecFree(&keep);
		return TCL_ERROR;
	    }
	    nameObj = Tcl_NewStringObj(Tcl_DStringValue(&qd),
		    Tcl_DStringLength(&qd));
	    Tcl_DStringFree(&qd);
	    TclVecAdd(&keep, nameObj);
	}
	rc = TypeDefine(interp, d, nameObj, objv[3], isAlias);
	if (rc == TCL_OK && compact) {
	    const char *nm = Tcl_GetString(nameObj);
	    Tcl_HashEntry *h = Tcl_FindHashEntry(&d->names, nm);
	    TclType *tp = (h != NULL) ? (TclType *) Tcl_GetHashValue(h) : NULL;

	    /*
	     * Une liste dont les éléments ont une disposition en cases, ou un
	     * produit qui en a une : toute liste de ce produit sera en cases
	     * (comme une struct C), y compris dans le champ d'un autre produit.
	     */
	    if (tp == NULL || !((tp->kind == TCLT_LIST && tp->elem != NULL
		    && TclCompactLayoutOf(tp->elem) != NULL)
		    || (tp->kind == TCLT_PRODUCT
		    && TclCompactLayoutOf(tp) != NULL))) {
		ERR(interp, "COMPACT", "type def {-compact}%s: only a list of "
			"fixed-size scalars (int(s,n), int(u,n) with n = 8, 16, "
			"32 or 64, double, double(32|64)), of products of them or of "
			"fixed-size lists list(n) of them (at most 4096 parts per "
			"element), or such a product, "
			"can be compact, not %s", nm,
			tp != NULL ? tp->name : "?");
		if (h != NULL) {
		    Tcl_DeleteHashEntry(h);	/* la définition est annulée */
		    d->gen++;
		}
		rc = TCL_ERROR;
	    } else {
		tp->compactWanted = 1;
	    }
	}
	TclVecFree(&keep);
	return rc;
    }
    if (strcmp(sub, "forget") == 0) {
	{
	    /* v16u : chaque nom résolu en son nom rangé (::geom::point) */
	    Tcl_Size i, n = objc - 2;
	    Tcl_Obj **keys = (Tcl_Obj **) Tcl_Alloc(
		    (n > 0 ? n : 1) * sizeof(Tcl_Obj *));
	    int rc;

	    for (i = 0; i < n; i++) {
		const char *nm = Tcl_GetString(objv[i + 2]);
		Tcl_HashEntry *h = TclTypesFindName(interp, d, &d->names, nm,
			-1, NULL, NULL);
		Tcl_HashTable *t = &d->names;

		if (h == NULL) {
		    h = TclTypesFindName(interp, d, &d->generics, nm, -1, NULL,
			    NULL);
		    t = &d->generics;
		}
		keys[i] = (h != NULL) ? Tcl_NewStringObj(
			(const char *) Tcl_GetHashKey(t, h), TCL_AUTO_LENGTH)
			: objv[i + 2];
		Tcl_IncrRefCount(keys[i]);
	    }
	    rc = TypeForgetCmd(interp, d, n, keys);
	    for (i = 0; i < n; i++) {
		Tcl_DecrRefCount(keys[i]);
	    }
	    Tcl_Free(keys);
	    return rc;
	}
    }
    if (strcmp(sub, "convert") == 0) {
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 2, objv, "from to lambda");
	    return TCL_ERROR;
	}
	return TypeConvertCmd(interp, d, objv[2], objv[3], objv[4]);
    }
    if (strcmp(sub, "get") == 0) {
	return TypeGetCmd(interp, objc, objv);
    }
    if (strcmp(sub, "export") == 0) {
	/* type {c!}export T ... : la cible est une annotation locale sur le
	 * verbe */
	Tcl_Obj *words, *verb, *tg;
	TclObjVec keep = {NULL, 0, 0};
	Tcl_Size nw;
	int rc = TCL_ERROR;

	nw = LocalWords(objv[1], &words, &verb, &keep);
	Tcl_IncrRefCount(words);
	if (nw != 1) {
	    ERR(interp, "EXPORT", "type export needs one target: "
		    "type {c!}export T ?T ...?");
	} else {
	    const char *t;

	    Tcl_ListObjIndex(NULL, words, 0, &tg);
	    t = Tcl_GetString(tg);
	    if (strcmp(t, "c!") == 0) {
		rc = TypeCCmd(interp, objc, objv);
	    } else if (strcmp(t, "llvm!") == 0) {
		ERR(interp, "EXPORT", "type export: the target llvm! is not "
			"available yet: must be c!");
	    } else {
		ERR(interp, "EXPORT", "type export: unknown target {%s}: must be "
			"c!", t);
	    }
	}
	Tcl_DecrRefCount(words);
	TclVecFree(&keep);
	return rc;
    }
    if (strcmp(sub, "is") == 0) {
	return TypeIsCmd(interp, objc, objv);
    }
    if (strcmp(sub, "cast") == 0) {
	/*
	 * type cast T valeur : le résolveur d'un constructeur, sans
	 * variable. T est empilé sur la valeur, puis la pile est résolue
	 * comme dans "set y {T}$valeur".
	 */
	Tcl_Obj *pushed, *res = NULL;
	int rc;

	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "type value");
	    return TCL_ERROR;
	}
	{
	    Tcl_Size tl;
	    const char *tt = Tcl_GetStringFromObj(objv[2], &tl);

	    if (TclTypeParse(interp, tt, tl) == NULL) {
		/* S.v : une variante d'une somme (comme {S/v}$x). */
		const char *dot = strrchr(tt, '/');
		TclType *sum = (dot != NULL && dot > tt)
			? TclTypeParse(interp, tt, dot - tt) : NULL;

		if (sum == NULL || sum->kind != TCLT_SUM
			|| TclTypeFieldIndex(sum, dot + 1) < 0) {
		    (void) TclTypeParse(interp, tt, tl);	/* le message */
		    return TCL_ERROR;
		}
		Tcl_ResetResult(interp);
	    }
	}
	pushed = TclAnnotPush(objv[3], objv[2]);
	Tcl_IncrRefCount(pushed);
	rc = TclAnnotSettle(interp, pushed, NULL, 0, &res);
	if (rc == TCL_OK) {
	    Tcl_SetObjResult(interp, res);
	}
	Tcl_DecrRefCount(pushed);
	return rc;
    }
    if (strcmp(sub, "deref") == 0) {
	return TypeDerefCmd(interp, objc, objv);
    }
    if (strcmp(sub, "meta") == 0) {
	return TypeMetaCmd(interp, objc, objv);	/* v17e */
    }
    if (strcmp(sub, "canonic") == 0) {
	return TypeCanonicCmd(interp, objc, objv);	/* v17c */
    }
    if (strcmp(sub, "parse") == 0) {
	return TypeParseCmd(interp, objc, objv);	/* v17c */
    }
    if (strcmp(sub, "proc") == 0) {
	return TypeProcCmd(interp, objc, objv);	/* v16p */
    }
    if (strcmp(sub, "call") == 0) {
	return TypeCallCmd(interp, objc, objv);	/* v16z */
    }
    if (strcmp(sub, "binary") == 0) {
	return TypeBinaryCmd(interp, objc, objv);
    }
    if (strcmp(sub, "debug") == 0) {
	return TclTypeDebugCmd(interp, objc, objv);	/* v16y */
    }
    if (strcmp(sub, "map") == 0) {
	return TypeMapUser(interp, objc, objv);	/* 23/11, v17y */
    }
    if (strcmp(sub, "repeat") == 0) {
	return TypeRepeatCmd(interp, objc, objv);	/* v17t */
    }
    if (strcmp(sub, "ops") == 0) {
	/* 23/11 : type ops TYPE : la spécialisation choisie pour TYPE (son
	 * nom), ou rien si TYPE n'a que le chemin général */
	TclType *tp;
	const char *w, *nm;
	Tcl_Size len;

	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "type");
	    return TCL_ERROR;
	}
	w = Tcl_GetStringFromObj(objv[2], &len);
	tp = TclTypeParse(interp, w, len);
	if (tp == NULL) {
	    return TCL_ERROR;
	}
	nm = TclTypeOpsName(tp);
	Tcl_SetObjResult(interp, Tcl_NewStringObj(nm ? nm : "", -1));
	return TCL_OK;
    }
    if (strcmp(sub, "eq") == 0) {
	/* v17z : type eq A B : le même type résolu (variante comprise), ni
	 * la valeur ni les notes */
	Tcl_Obj *ga, *gb, *av[3];
	int same;

	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "value value");
	    return TCL_ERROR;
	}
	av[0] = objv[0];
	av[1] = objv[1];
	av[2] = objv[2];
	if (TypeGetCmd(interp, 3, av) != TCL_OK) {
	    return TCL_ERROR;
	}
	ga = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(ga);
	av[2] = objv[3];
	if (TypeGetCmd(interp, 3, av) != TCL_OK) {
	    Tcl_DecrRefCount(ga);
	    return TCL_ERROR;
	}
	gb = Tcl_GetObjResult(interp);
	same = (strcmp(Tcl_GetString(ga), Tcl_GetString(gb)) == 0);
	Tcl_DecrRefCount(ga);
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(same));
	return TCL_OK;
    }
    if (strcmp(sub, "include") == 0) {
	/* v17z : type include U T : toute valeur de T entre dans U sans
	 * cast ni échec possible, égale à elle-même ; T peut être S.v */
	TclType *u, *t;
	Tcl_Size ul, tl;
	const char *ut, *tt;
	int tv = -1;

	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "type type");
	    return TCL_ERROR;
	}
	ut = Tcl_GetStringFromObj(objv[2], &ul);
	tt = Tcl_GetStringFromObj(objv[3], &tl);
	u = ParseTypeOrVariant(interp, ut, ul);
	if (u == NULL) {
	    return TCL_ERROR;
	}
	if (!TypeParsesQuietly(interp, tt, tl)) {
	    t = ParseTypeOrVariant(interp, tt, tl);
	    if (t == NULL) {
		return TCL_ERROR;
	    }
	    if (t->kind == TCLT_SUM) {
		/* S/v : une valeur de la variante v de S */
		const char *dot = strrchr(tt, '/');
		Tcl_Obj *nm = Tcl_NewStringObj(dot + 1, -1);

		Tcl_IncrRefCount(nm);
		tv = (int) TclTypeFieldIndex(t, Tcl_GetString(nm));
		Tcl_DecrRefCount(nm);
	    }
	} else {
	    t = TclTypeParse(interp, tt, tl);
	}
	if (tv >= 0) {
	    /* S/v dans U : U inclut S, ou U inclut la charge de v (une somme
	     * se lit comme sa variante) quand U n'est pas une somme */
	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(TypeIncludes(u, t, 0)
		    || (u->kind != TCLT_SUM && t->fields[tv].type != NULL
		    && TypeIncludes(u, t->fields[tv].type, 0))));
	    return TCL_OK;
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(TypeIncludes(u, t, 0)));
	return TCL_OK;
    }
    if (strcmp(sub, "literal") == 0 || (strcmp(sub, "variant") == 0
	    && objc >= 4 && strcmp(Tcl_GetString(objv[2]), "-of") == 0)) {
	/*
	 * v17z : type variant -of T ?NAME? : les variantes nommées de T (ou
	 * le type de la charge de NAME) ; type literal -of T : ses littéraux.
	 */
	int lit = (sub[0] == 'l');
	TclType *tp;
	Tcl_Obj *res;
	Tcl_Size i, tl;
	const char *tt;

	if (lit ? (objc != 4 || strcmp(Tcl_GetString(objv[2]), "-of") != 0)
		: (objc != 4 && objc != 5)) {
	    Tcl_WrongNumArgs(interp, 2, objv, lit ? "-of type"
		    : "-of type ?variant?");
	    return TCL_ERROR;
	}
	tt = Tcl_GetStringFromObj(objv[3], &tl);
	tp = (tl > 0) ? ParseTypeOrVariant(interp, tt, tl) : NULL;
	if (tp == NULL) {
	    if (tl == 0) {
		ERR(interp, "VARIANT", "type %s -of: empty type name", sub);
	    }
	    return TCL_ERROR;
	}
	while (tp->kind != TCLT_SUM && tp->under != NULL) {
	    tp = tp->under;		/* un nominal sur une somme */
	}
	if (tp->kind != TCLT_SUM) {
	    ERR(interp, "VARIANT", "type %s -of: %s is not a sum", sub,
		    tp->name);
	    return TCL_ERROR;
	}
	if (objc == 5) {
	    Tcl_Size k = TclTypeFieldIndex(tp, Tcl_GetString(objv[4]));

	    if (k < 0 || tp->fields[k].type == NULL) {
		ERR(interp, "VARIANT", "type variant -of %s: no variant \"%s\""
			"%s", tp->name, Tcl_GetString(objv[4]),
			(k >= 0 || SumLiteralIndex(tp, objv[4]) >= 0)
			? " (a literal of it: a literal has no payload)" : "");
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    tp->fields[k].type->name, TCL_AUTO_LENGTH));
	    return TCL_OK;
	}
	res = Tcl_NewListObj(0, NULL);
	for (i = 0; i < tp->nfields; i++) {
	    if (lit && tp->fields[i].type == NULL
		    && tp->fields[i].literal != NULL) {
		Tcl_ListObjAppendElement(NULL, res, tp->fields[i].literal);
	    } else if (!lit && tp->fields[i].type != NULL
		    && tp->fields[i].name != NULL) {
		Tcl_ListObjAppendElement(NULL, res, tp->fields[i].name);
	    }
	}
	Tcl_SetObjResult(interp, res);
	return TCL_OK;
    }
    if (strcmp(sub, "variant") == 0) {
	/* v16w : type variant VALEUR : le nom de la variante d'une valeur
	 * d'une somme ou d'une union ; v18e : un littéral, son nom ; avec
	 * NAME, la même charge sous la variante NAME (une nouvelle valeur) */
	TclType *tp = NULL;
	Tcl_Obj *content = NULL;
	int v = -1;

	if (objc != 3 && objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "value ?variant?");
	    return TCL_ERROR;
	}
	if (TclAnnotInfo(objv[2], &content, &tp, &v, NULL) != 1
		|| tp == NULL) {
	    ERR(interp, "VARIANT", "type variant: the value \"%s\" has no "
		    "type", Tcl_GetString(objv[2]));
	    return TCL_ERROR;
	}
	if (tp->kind != TCLT_SUM) {
	    ERR(interp, "VARIANT", "type variant: the value is a %s, not a "
		    "sum", tp->name);
	    return TCL_ERROR;
	}
	if (TclSumTagged(tp)) {
	    v = TclSumDecode(tp, content, NULL);
	} else if (v < 0) {
	    TclTypeValidate(NULL, tp, content, -1, &v);
	}
	if (objc == 4) {
	    /* réétiqueter : la même charge, lue sous la variante NAME */
	    Tcl_Size k = TclTypeFieldIndex(tp, Tcl_GetString(objv[3]));
	    Tcl_Obj *copy, *nv;

	    if (k < 0 && (k = SumLiteralIndex(tp, objv[3])) >= 0) {
		/* v18e : un littéral, variante à valeur unique : la valeur
		 * doit être ce littéral */
		if (strcmp(Tcl_GetString(content),
			Tcl_GetString(tp->fields[k].literal)) != 0) {
		    ERR(interp, "VARIANT", "type variant: the variant %s of %s "
			    "is the value \"%s\", not \"%s\"",
			    Tcl_GetString(objv[3]), tp->name,
			    Tcl_GetString(tp->fields[k].literal),
			    Tcl_GetString(content));
		    return TCL_ERROR;
		}
		nv = TclNewAnnotated(interp, TclTypeSumRoot(tp), (int) k,
			tp->fields[k].literal);
		Tcl_SetObjResult(interp, TclAnnotCarryNotes(objv[2], nv));
		return TCL_OK;
	    }
	    if (k < 0 || tp->fields[k].type == NULL) {
		ERR(interp, "VARIANT", "type variant: %s has no variant \"%s\""
			"%s", tp->name, Tcl_GetString(objv[3]),
			(k >= 0 || SumLiteralIndex(tp, objv[3]) >= 0)
			? " (a literal of it is its own value: write it)" : "");
		return TCL_ERROR;
	    }
	    copy = Tcl_DuplicateObj(content);
	    Tcl_IncrRefCount(copy);
	    if (TclTypeComplete(interp, tp, copy, (int) k, NULL) != TCL_OK) {
		Tcl_DecrRefCount(copy);
		if (v >= 0 && v < tp->nfields && v != k
			&& TclTypeHasVariantRule(interp, tp, v, (int) k)) {
		    Tcl_AppendObjToObj(Tcl_GetObjResult(interp), Tcl_ObjPrintf(
			    " (to convert it, use type cast %s/%s)", tp->name,
			    Tcl_GetString(objv[3])));
		}
		return TCL_ERROR;
	    }
	    nv = TclNewAnnotated(interp, tp, (int) k, copy);
	    Tcl_DecrRefCount(copy);
	    if (nv == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, TclAnnotCarryNotes(objv[2], nv));
	    return TCL_OK;
	}
	if (v < 0 || v >= tp->nfields) {
	    ERR(interp, "VARIANT", "type variant: no variant of %s",
		    tp->name);
	    return TCL_ERROR;
	}
	/* v18e : un littéral est une variante à valeur unique : son nom est
	 * sa valeur (M/auto) */
	Tcl_SetObjResult(interp, tp->fields[v].name != NULL
		? tp->fields[v].name : tp->fields[v].literal != NULL
		? tp->fields[v].literal : Tcl_NewWideIntObj(v));
	return TCL_OK;
    }
    ERR(interp, "SUBCMD", "bad subcommand \"%s\": must be def, alias, "
	    "call, canonic, cast, convert, debug, deref, eq, export, forget, get, include, is, literal, map, meta, ops, parse, proc, repeat, variant "
	    "or binary", sub);
    return TCL_ERROR;
}

/* Exports pour tclTypeGeneric.c. */

TclType *
TclTypesNewType(TypeData *d, TclTypeKind kind, const char *name)
{
    return NewType(d, kind, name);
}

int
TclTypesInhabited(const TclType *tp, const TclType **stack, int depth)
{
    return Inhabited(tp, stack, depth);
}

void
TclTypesCopyInto(TclType *into, const TclType *res)
{
    CopyInto(into, res);
}

int
TclTypesCheckNewName(Tcl_Interp *interp, TypeData *d, const char *name)
{
    return CheckNewName(interp, d, name);
}

/*
 * L'opérateur "is" de expr :   expr {z is Complex}
 *
 * ::tcl::mathop::is nom type. Le NOM est celui d'une variable (visible
 * depuis l'appelant). Vrai si
 *  - la variable est typée et son type déclaré est inclus dans TYPE (une
 *    variable {int}n est un int, un double, ... un number si défini) ;
 *  - la variable n'est pas typée, existe, et sa valeur est valide pour TYPE.
 * Une variable inexistante donne faux ; un type inconnu est une erreur.
 */

/*
 * "nom is cmd" : NOM est une commande Tcl (cherchée depuis l'espace de noms
 * courant puis le global). Si elle existe, elle devient appelable comme
 * fonction de expr : un alias ::tcl::mathfunc::<queue du nom> la désigne
 * (sauf si une fonction de ce nom existe déjà). Résultat : 1 si la commande
 * existe, 0 sinon.
 */

static int
IsCmdOp(Tcl_Interp *interp, Tcl_Obj *nameObj)
{
    Tcl_Command cmd = Tcl_FindCommand(interp, Tcl_GetString(nameObj), NULL, 0);
    int found = (cmd != NULL);

    if (found) {
	Tcl_Obj *full = Tcl_NewObj(), *alias;
	const char *fullName, *tail, *p;

	Tcl_GetCommandFullName(interp, cmd, full);
	Tcl_IncrRefCount(full);
	fullName = Tcl_GetString(full);
	tail = fullName;
	for (p = fullName; *p != '\0'; p++) {
	    if (p[0] == ':' && p[1] == ':') {
		tail = p + 2;
	    }
	}
	alias = Tcl_ObjPrintf("::tcl::mathfunc::%s", tail);
	Tcl_IncrRefCount(alias);
	if (Tcl_FindCommand(interp, Tcl_GetString(alias), NULL,
		TCL_GLOBAL_ONLY) == NULL) {
	    if (Tcl_CreateAlias(interp, Tcl_GetString(alias), interp,
		    fullName, 0, NULL) != TCL_OK) {
		Tcl_DecrRefCount(alias);
		Tcl_DecrRefCount(full);
		return TCL_ERROR;
	    }
	}
	Tcl_DecrRefCount(alias);
	Tcl_DecrRefCount(full);
    }
    Tcl_ResetResult(interp);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(found));
    return TCL_OK;
}

/*
 * v18d : ((a,b) = valeur) is T : le test porte sur la VALEUR affectée (une
 * affectation par tuple désigne plusieurs variables). Une valeur typée :
 * son type entre dans T (S/v : sa variante, sa charge) ; une valeur nue :
 * elle est valide pour T.
 */
static int
IsValueObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    TclType *want, *vt = NULL;
    Tcl_Obj *content = NULL;
    int vv = -1, result = 0, dv = -1;
    Tcl_Size tl;
    const char *tt;
    (void) clientData;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "value type");
	return TCL_ERROR;
    }
    tt = Tcl_GetStringFromObj(objv[2], &tl);
    want = TclTypeParse(interp, tt, tl);
    if (want == NULL) {
	return TCL_ERROR;
    }
    if (TclAnnotInfo(objv[1], &content, &vt, &vv, NULL) == 1 && vt != NULL) {
	if (want->variantOf != NULL) {
	    TclType *root = want->variantOf;
	    int vi = (int) want->variantIdx;

	    result = (TclTypeSumRoot(vt) == root && vv == vi && (vt == want
		    || want->fields[vi].type == root->fields[vi].type
		    || TclTypeValidate(NULL, want, content, vi, NULL)
		    == TCL_OK));
	} else {
	    result = (vt == want || TypeIncludes(want, vt, 0));
	}
    } else {
	content = TCL_ANNOT_CONTENT(objv[1]);
	result = (TclTypeValidate(NULL, want, content, -1, &dv) == TCL_OK);
    }
    Tcl_ResetResult(interp);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(result));
    return TCL_OK;
}

static int
IsOpObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    TclType *want, *have;
    Tcl_Obj *canon = NULL, *value = NULL, *content;
    const char *typeText;
    Tcl_Size typeLen;
    int result = 0;
    (void) clientData;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "varName type");
	return TCL_ERROR;
    }
    typeText = Tcl_GetStringFromObj(objv[2], &typeLen);
    if (typeLen == 3 && memcmp(typeText, "cmd", 3) == 0) {
	return IsCmdOp(interp, objv[1]);
    }
    want = TclTypeParse(interp, typeText, typeLen);
    if (want == NULL) {
	return TCL_ERROR;
    }
    {
	/* v18e : à gauche, un nom de variable. Une variable qui n'existe
	 * pas : faux ; mais un texte qui est un type ([type get $r] is T,
	 * result/ok) est une confusion : une erreur qui le dit. */
	Tcl_Obj *cn = NULL, *val = NULL;
	int exists = 0;
	Tcl_Size nl;
	const char *nt = Tcl_GetStringFromObj(objv[1], &nl);

	if (TclVarRefCanon(interp, objv[1], &cn) == TCL_OK) {
	    Tcl_IncrRefCount(cn);
	    exists = TclVarRefType(interp, Tcl_GetString(cn)) != NULL
		    || TclVarRefGet(interp, Tcl_GetString(cn), &val) == TCL_OK;
	    Tcl_DecrRefCount(cn);
	}
	Tcl_ResetResult(interp);
	if (!exists && nl > 0) {
	    Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);
	    TclType *lt = TclTypeParse(interp, nt, nl);

	    Tcl_RestoreInterpState(interp, st);
	    if (lt != NULL) {
		ERR(interp, "ISNAME", "\"%s\" is a type, not a variable: the "
			"left of \"is\" names a variable (x is T), or assigns "
			"one ((r = value) is T)", nt);
		return TCL_ERROR;
	    }
	}
    }
    if (want->variantOf != NULL) {
	/* v18a, v18b : x is S/v, la variable vaut une valeur de S (ou de
	 * S/v), de variante v (charge restreinte comprise) */
	TclType *root = want->variantOf;
	int vi = (int) want->variantIdx, dv = -1;

	if (TclVarRefCanon(interp, objv[1], &canon) == TCL_OK) {
	    Tcl_IncrRefCount(canon);
	    if (TclVarRefGet(interp, Tcl_GetString(canon), &value)
		    == TCL_OK) {
		TclType *vt = NULL;
		int vv = -1;

		Tcl_IncrRefCount(value);
		if (TclAnnotInfo(value, &content, &vt, &vv, NULL) == 1
			&& vt != NULL) {
		    TclType *vr = vt->variantOf != NULL ? vt->variantOf : vt;

		    result = (vr == root && vv == vi && (vt == want
			    || want->fields[vi].type == root->fields[vi].type
			    || TclTypeValidate(NULL, want, content, vi, NULL)
			    == TCL_OK));
		} else {
		    content = TCL_ANNOT_CONTENT(value);
		    result = (TclTypeValidate(NULL, root, content, -1, &dv)
			    == TCL_OK && dv == vi && TclTypeValidate(NULL, want,
			    content, vi, NULL) == TCL_OK);
		}
		Tcl_DecrRefCount(value);
	    }
	    Tcl_DecrRefCount(canon);
	}
	Tcl_ResetResult(interp);
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(result));
	return TCL_OK;
    }
    if (TclVarRefCanon(interp, objv[1], &canon) != TCL_OK) {
	Tcl_ResetResult(interp);
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
	return TCL_OK;
    }
    Tcl_IncrRefCount(canon);
    have = TclVarRefType(interp, Tcl_GetString(canon));
    if (have != NULL) {
	result = TclTypeIncludes(want, have);
    } else if (TclVarRefGet(interp, Tcl_GetString(canon), &value) == TCL_OK) {
	Tcl_IncrRefCount(value);
	content = TCL_ANNOT_CONTENT(value);
	result = (TclTypeValidate(interp, want, content, -1, NULL) == TCL_OK);
	Tcl_DecrRefCount(value);
    }
    Tcl_DecrRefCount(canon);
    Tcl_ResetResult(interp);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(result));
    return TCL_OK;
}

/*
 * "z is T = valeur" dans expr : ::tcl::unsupported::declare nom type valeur
 * déclare la variable NOM de type TYPE (comme set {T}nom valeur) et lui
 * affecte VALEUR (validée). Le résultat est la valeur typée.
 */

/* La valeur typée d'une déclaration X is T = v : *typedPtr, sans
 * référence propre (la valeur elle-même quand elle est déjà de type T).
 * mayOwn : valueObj non partagée peut devenir le contenu (valeur de la pile
 * d'exécution, que seule la pile tient : la liste neuve de (a, b)). */
static int DeclareTypedTp(Tcl_Interp *interp, TclType *tp, Tcl_Obj *typeObj,
	Tcl_Obj *valueObj, int mayOwn, Tcl_Obj **typedPtr);

/* Un champ scalaire simple : int, double, string, sans représentation
 * binaire qui borne la valeur. */
static int
QuickScalarOk(const TclType *ft, Tcl_Obj *v)
{
    double d;

    if (ft == NULL || ft->binLink != NULL || ft->pred != NULL) {
	return 0;
    }
    switch (ft->kind) {
    case TCLT_DOUBLE:
	return Tcl_GetDoubleFromObj(NULL, v, &d) == TCL_OK;
    case TCLT_INT:
	return ft->bitWidth == 0 && ft->binKind != TCLBIN_UINT
		&& ft->binKind != TCLBIN_SINT && IsIntegerValue(v);
    case TCLT_STRING:
	return ft->binKind == TCLBIN_NONE && !TclIsAnnotated(v);
    default:
	return 0;
    }
}

/* v16s : un produit de scalaires simples donné par une liste nue d'autant
 * de valeurs que de champs : la valeur typée, ou NULL (cas général : le
 * message d'erreur, les défauts, les cases viennent du chemin ordinaire). */
/* Les valeurs d'une liste nue, champ par champ, pour un produit de
 * scalaires simples : *plainPtr reçoit la liste des contenus (nue, refcount
 * 0 ; la liste elle-même si aucun élément n'est annoté), ou NULL. Un
 * élément déjà typé du type du champ, sans note, est accepté. */
static Tcl_Obj *
ScalarProductPlain(TclType *tp, Tcl_Obj *valueObj)
{
    Tcl_Size n, i;
    Tcl_Obj **ev, *buf[16], **cv = buf, *res;
    int annotated = 0;

    if (tp->kind != TCLT_PRODUCT || tp->compactWanted || tp->binTag != NULL
	    || Tcl_ListObjGetElements(NULL, valueObj, &n, &ev) != TCL_OK
	    || n != tp->nfields || n == 0 || n > 16) {
	return NULL;
    }
    for (i = 0; i < n; i++) {
	const TclType *ft = tp->fields[i].type;
	Tcl_Obj *e = ev[i];

	if (TclIsAnnotated(e)) {
	    TclType *et = NULL;
	    Tcl_Obj *c = NULL;

	    if (TclAnnotInfo(e, &c, &et, NULL, NULL) != 1 || et != ft
		    || c == NULL || !TclAnnotIsSettled(e)
		    || ft->kind == TCLT_SUM) {
		return NULL;		/* v17z : une somme garde sa variante */
	    }
	    e = c;
	    annotated = 1;
	} else if (!QuickScalarOk(ft, e)) {
	    return NULL;
	}
	cv[i] = e;
    }
    res = annotated ? Tcl_NewListObj(n, cv) : valueObj;
    return res;
}

static Tcl_Obj *
ScalarProductQuick(Tcl_Interp *interp, TclType *tp, Tcl_Obj *valueObj)
{
    Tcl_Obj *plain = ScalarProductPlain(tp, valueObj), *copy;

    if (plain == NULL) {
	return NULL;
    }
    copy = (plain == valueObj) ? Tcl_DuplicateObj(valueObj) : plain;
    return TclNewAnnotatedOwn(copy, TclTypeRefFor(interp, tp, -1));
}

static int
DeclareTyped(Tcl_Interp *interp, Tcl_Obj *typeObj, Tcl_Obj *valueObj,
	int mayOwn, Tcl_Obj **typedPtr)
{
    return DeclareTypedTp(interp, TclAnnotTypeOf(interp, typeObj), typeObj,
	    valueObj, mayOwn, typedPtr);
}

/* v16s : la même chose, le type déjà résolu (tp, peut être NULL). */
static int
DeclareTypedTp(Tcl_Interp *interp, TclType *tp, Tcl_Obj *typeObj,
	Tcl_Obj *valueObj, int mayOwn, Tcl_Obj **typedPtr)
{
    /* X is T = $x avec $x déjà de type T, sans note : la valeur telle
     * quelle (le cas courant des procédures typées : P is point = $p) */
    TclType *vt = NULL;
    Tcl_Size np = 1;

    if (tp != NULL && TclTypeIsAny(tp)) {
	/* v17e : any, un méta-type : l'argument tel quel, son état vérifié
	 * (après l'annotation de l'appel) */
	Tcl_Obj *sv = valueObj;

	if (TclIsAnnotated(valueObj)
		&& TclAnnotSettle(interp, valueObj, NULL, 0, &sv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (TclTypeMetaCheck(interp, tp, sv) != TCL_OK) {
	    if (sv != valueObj) {
		TclDiscard(sv);
	    }
	    return TCL_ERROR;
	}
	*typedPtr = sv;
	return TCL_OK;
    }
    if (tp != NULL && TclAnnotInfo(valueObj, NULL, &vt, NULL, NULL) != 1) {
	vt = NULL;			/* v17d : seulement un type résolu */
    }
    if (tp != NULL && vt == tp) {
	if (TclAnnotIsSettled(valueObj)) {
	    np = 0;
	} else {
	    Tcl_Obj *pend = TclAnnotPending(valueObj);

	    Tcl_IncrRefCount(pend);
	    Tcl_ListObjLength(NULL, pend, &np);
	    Tcl_DecrRefCount(pend);
	}
    }
    if (np == 0) {
	*typedPtr = valueObj;
    } else if (tp != NULL && (tp->kind == TCLT_INT || tp->kind == TCLT_DOUBLE)
	    && !TclIsAnnotated(valueObj)
	    && !(tp->nominal && tp->under != NULL	/* v17d : sa validation */
		&& FindRule(GetData(interp), tp->under->name, tp->name))) {
	/* X is double = $x : un nombre n'a rien à compléter ; le contenu est
	 * partagé (une valeur est immuable), comme pour la lecture d'un champ.
	 * v16s : double et int simples vérifiés directement (ValidateG fait
	 * la même chose après ses cas généraux) ; le message vient de
	 * TclTypeValidate. */
	double dv;
	int quick = (tp->binLink == NULL && tp->pred == NULL)
		&& (tp->kind == TCLT_DOUBLE
		? Tcl_GetDoubleFromObj(NULL, valueObj, &dv) == TCL_OK
		: (tp->bitWidth == 0 && tp->binKind != TCLBIN_UINT
		    && tp->binKind != TCLBIN_SINT
		    && IsIntegerValue(valueObj)));

	if (!quick && TclTypeValidate(interp, tp, valueObj, -1, NULL)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	*typedPtr = TclNewAnnotatedShared(interp, tp, -1, valueObj);
    } else if (tp != NULL && !TclIsAnnotated(valueObj)
	    && (*typedPtr = ScalarProductQuick(interp, tp, valueObj)) != NULL) {
	/* v16s : un produit de scalaires donné en liste ({1 2} pour un
	 * point) : ses champs vérifiés un à un, sans le validateur général */
    } else if (tp != NULL && !TclIsAnnotated(valueObj)) {
	/* X is T = (a, b) : une valeur sans annotation, construite
	 * directement (ce que ferait le résolveur du constructeur) */
	Tcl_Obj *copy;
	int variant = -1;
	const TclType *kt = tp;
	Tcl_Size dummy;

	while (kt->under != NULL && kt->kind != TCLT_LIST
		&& kt->kind != TCLT_PRODUCT && kt->kind != TCLT_DICT) {
	    kt = kt->under;
	}
	if (kt->kind == TCLT_LIST || kt->kind == TCLT_PRODUCT) {
	    /* v16q : la source devient une liste une fois pour toutes (comme
	     * pour un argument classique) : la copie garde la liste, au lieu
	     * de relire la chaîne à chaque appel */
	    (void) Tcl_ListObjLength(NULL, valueObj, &dummy);
	} else if (kt->kind == TCLT_DICT) {
	    (void) Tcl_DictObjSize(NULL, valueObj, &dummy);
	}
	copy = (mayOwn && !Tcl_IsShared(valueObj))
		? valueObj : Tcl_DuplicateObj(valueObj);

	Tcl_IncrRefCount(copy);
	if (TclTypeCompleteBare(interp, tp, copy, -1, &variant) != TCL_OK) {
	    Tcl_DecrRefCount(copy);
	    return TCL_ERROR;
	}
	/* la copie (ou la valeur de la pile) devient le contenu : pas de
	 * seconde copie */
	*typedPtr = TclNewAnnotatedOwn(copy, TclTypeRefFor(interp, tp, variant));
	Tcl_DecrRefCount(copy);
    } else if (tp != NULL && vt != NULL && vt != tp
	    && !TclTypeIsPlainString(tp) && !TclTypeIsAny(tp)) {
	/*
	 * v17d : une valeur typée d'un autre type, sans annotation en
	 * attente : un argument n'est pas un cast (ANNOTATIONS § 5.6).
	 * L'élargissement et la variante passent ; une règle ou une
	 * conversion intégrée s'écrit à l'appel : f {int}$x.
	 */
	Tcl_Obj *r = NULL, *sv = valueObj;
	TclType *st = vt;
	int rc;

	if (!TclAnnotIsSettled(valueObj)) {
	    /* f {int}$x : l'annotation de l'appel (un cast écrit) d'abord */
	    if (TclAnnotSettle(interp, valueObj, NULL, 0, &sv) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (TclAnnotInfo(sv, NULL, &st, NULL, NULL) != 1) {
		st = NULL;
	    }
	    if (st == NULL || st == tp) {
		/* plus rien de typé à refuser : le chemin ordinaire */
		Tcl_IncrRefCount(sv);
		rc = TclAnnotateValue(interp, typeObj, sv, typedPtr);
		Tcl_DecrRefCount(sv);
		return rc;
	    }
	}
	Tcl_IncrRefCount(sv);
	rc = TclTypeCastImplicit(interp, sv, tp, &r);
	if (rc == TCL_OK && r == sv) {
	    r = Tcl_DuplicateObj(sv);
	}
	Tcl_DecrRefCount(sv);
	vt = st;

	if (rc == TCL_CONTINUE && vt != NULL && vt->kind == TCLT_SUM) {
	    /* le message du cast, plus parlant (« lire sa valeur par son
	     * chemin ») ; un cast qui réussirait reste refusé */
	    Tcl_Obj *tmp = NULL;

	    if (TclTypeCast(interp, valueObj, tp, &tmp) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (tmp != NULL && tmp != valueObj) {
		TclDiscard(tmp);
	    }
	}
	if (rc == TCL_CONTINUE) {
	    ERR(interp, "MISMATCH", "type mismatch: expected %s but got %s "
		    "(use an explicit cast)", tp->name, vt->name);
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "MISMATCH", (char *) NULL);
	    return TCL_ERROR;
	}
	if (rc != TCL_OK) {
	    return TCL_ERROR;
	}
	*typedPtr = r;
    } else if (TclAnnotateValue(interp, typeObj, valueObj, typedPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
DeclareObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    Tcl_Obj *typed = NULL, *res;
    (void) clientData;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "varName type value");
	return TCL_ERROR;
    }
    if (DeclareTyped(interp, objv[2], objv[3], 0, &typed) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_IncrRefCount(typed);
    res = TclTypedVarDeclare(interp, objv[1], typed);
    if (res == NULL) {
	Tcl_DecrRefCount(typed);
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, res);	/* la valeur rangée, résolue */
    Tcl_DecrRefCount(typed);
    return TCL_OK;
}

/*
 * Instruction declareLocal (X is T = v compilé dans une procédure, X
 * variable locale) : comme ::tcl::unsupported::declare, sans appel de
 * commande ni recherche de la variable par son nom. Rend la valeur rangée
 * AVEC une référence pour l'appelant (une trace d'écriture peut défaire la
 * variable), NULL en cas d'erreur (message dans l'interpréteur).
 */
Tcl_Obj *
TclTypedDeclareLocal(Tcl_Interp *interp, Var *varPtr, Tcl_Obj *nameObj,
	Tcl_Obj *typeObj, Tcl_Obj *valueObj)
{
    Tcl_Obj *typed = NULL, *res;

    if (DeclareTyped(interp, typeObj, valueObj, 1, &typed) != TCL_OK) {
	return NULL;
    }
    Tcl_IncrRefCount(typed);
    res = TclTypedVarDeclareVar(interp, varPtr, NULL, nameObj, typed);
    if (res != NULL) {
	Tcl_IncrRefCount(res);
    }
    Tcl_DecrRefCount(typed);
    return res;
}

/* Contenu PROFOND : les éléments annotés des collections et des produits
 * (valeurs décodées, list.T) deviennent bruts eux aussi ; une chaîne n'est
 * jamais redécoupée. Objet neuf ou partagé, sans référence propre. */
static Tcl_Obj *
DeepContent(Tcl_Obj *obj)
{
    Tcl_Obj *content = NULL, **elv, *res;
    TclType *tp = NULL;
    Tcl_Size n, i;
    int composite = 0;

    if (TclAnnotInfo(obj, &content, &tp, NULL, NULL) == 1 && content != NULL) {
	composite = (tp->kind == TCLT_LIST || tp->kind == TCLT_DICT
		|| tp->kind == TCLT_PRODUCT || tp->kind == TCLT_SUM);
	obj = content;
	if (TclIsCompact(obj)) {
	    /* une copie (le tampon est partagé) : ce que le script fera du
	     * contenu ne défait pas la forme compacte de la valeur */
	    return Tcl_DuplicateObj(obj);
	}
    } else {
	composite = TclHasInternalRep(obj, &tclListType);
    }
    if (!composite || !TclHasInternalRep(obj, &tclListType)
	    || Tcl_ListObjGetElements(NULL, obj, &n, &elv) != TCL_OK) {
	return obj;
    }
    for (i = 0; i < n; i++) {
	if (TclIsAnnotated(elv[i]) || TclHasInternalRep(elv[i], &tclListType)) {
	    break;
	}
    }
    if (i == n) {
	return obj;			/* rien d'annoté dedans */
    }
    {
	/* la liste n'est refaite que si un élément change vraiment (une
	 * liste imbriquée sans annotation reste telle quelle) */
	Tcl_Obj *first = NULL;
	Tcl_Size j;

	for (; i < n; i++) {
	    first = DeepContent(elv[i]);
	    if (first != elv[i]) {
		break;
	    }
	}
	if (i == n) {
	    return obj;
	}
	res = Tcl_NewListObj(n, NULL);
	for (j = 0; j < i; j++) {
	    Tcl_ListObjAppendElement(NULL, res, elv[j]);
	}
	Tcl_ListObjAppendElement(NULL, res, first);
	for (j = i + 1; j < n; j++) {
	    Tcl_ListObjAppendElement(NULL, res, DeepContent(elv[j]));
	}
    }
    return res;
}

/*
 * Caractéristiques de la machine, telles que les voit le compilateur qui a
 * construit Tcl : l'alignement de chaque type fixe DANS UNE STRUCTURE
 * (offsetof sur {char c; T x;}), que tcl_platform ne donne pas (Linux i386 :
 * double et int64_t alignés sur 4). Sert au calcul de disposition des cases
 * (représentation compacte) et à "type c".
 */
/* Structures nommées : un type défini dans offsetof n'est pas portable. */
struct AbiI8 { char c; int8_t x; };
struct AbiI16 { char c; int16_t x; };
struct AbiI32 { char c; int32_t x; };
struct AbiI64 { char c; int64_t x; };
struct AbiF32 { char c; float x; };
struct AbiF64 { char c; double x; };
struct AbiPtr { char c; void *x; };
#define ABI_ALIGN(S) offsetof(struct S, x)

int
TclTypeAbiAlign(int isFloat, int bits)
{
    if (isFloat) {
	return bits == 32 ? (int) ABI_ALIGN(AbiF32) : (int) ABI_ALIGN(AbiF64);
    }
    switch (bits) {
    case 8:  return (int) ABI_ALIGN(AbiI8);
    case 16: return (int) ABI_ALIGN(AbiI16);
    case 32: return (int) ABI_ALIGN(AbiI32);
    case 64: return (int) ABI_ALIGN(AbiI64);
    case 0:  return (int) ABI_ALIGN(AbiPtr);	/* un pointeur */
    }
    return 1;
}

static int
AbiObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    static const struct { const char *n; int f, b, size; } t[] = {
	{"i8", 0, 8, 1}, {"i16", 0, 16, 2}, {"i32", 0, 32, 4},
	{"i64", 0, 64, 8}, {"f32", 1, 32, 4}, {"f64", 1, 64, 8},
	{"ptr", 0, 0, (int) sizeof(void *)}
    };
    Tcl_Obj *res = Tcl_NewDictObj(), *al = Tcl_NewDictObj();
    Tcl_Obj *sz = Tcl_NewDictObj();
    union { uint16_t u; unsigned char c[2]; } bo;
    size_t i;

    (void) clientData;
    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }
    bo.u = 1;
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
	Tcl_DictObjPut(NULL, al, Tcl_NewStringObj(t[i].n, -1),
		Tcl_NewIntObj(TclTypeAbiAlign(t[i].f, t[i].b)));
	Tcl_DictObjPut(NULL, sz, Tcl_NewStringObj(t[i].n, -1),
		Tcl_NewIntObj(t[i].size));
    }
    Tcl_DictObjPut(NULL, res, Tcl_NewStringObj("byteOrder", -1),
	    Tcl_NewStringObj(bo.c[0] ? "little" : "big", -1));
    Tcl_DictObjPut(NULL, res, Tcl_NewStringObj("pointer", -1),
	    Tcl_NewIntObj((int) sizeof(void *)));
    Tcl_DictObjPut(NULL, res, Tcl_NewStringObj("size", -1), sz);
    Tcl_DictObjPut(NULL, res, Tcl_NewStringObj("align", -1), al);
    Tcl_SetObjResult(interp, res);
    return TCL_OK;
}

/* (a,b) = $v dans expr : le contenu d'une valeur annotée (ses champs pour un
 * produit), ou la valeur elle-même. */
static int
ContentObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    Tcl_Obj *content = NULL;
    (void) clientData;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "value");
	return TCL_ERROR;
    }
    (void) content;
    Tcl_SetObjResult(interp, DeepContent(objv[1]));
    return TCL_OK;
}

/*
 * Instruction tupleAssign ("(a, (b, c), d) = v" dans expr) : le contenu
 * profond de v (comme ::tcl::unsupported::content), puis, pour chaque
 * chemin de PATHS ({0} {1 0} {1 1} {2}, établi à la compilation), le nom
 * au chemin dans NAMES reçoit l'élément au même chemin du contenu (un
 * élément absent donne "", comme lindex), dans l'ordre, comme le faisaient
 * listIndexImm et storeStk. Rend le contenu avec une référence pour
 * l'appelant, ou NULL (message dans l'interpréteur).
 */
static int
TupleIndex(Tcl_Interp *interp, Tcl_Obj *list, Tcl_Obj *const *pv,
	Tcl_Size np, Tcl_Obj **res)
{
    Tcl_Obj *cur = list;
    Tcl_Size k;
    Tcl_WideInt idx;

    for (k = 0; k < np; k++) {
	Tcl_Obj *e = NULL;

	if (Tcl_GetWideIntFromObj(interp, pv[k], &idx) != TCL_OK
		|| Tcl_ListObjIndex(interp, cur, (Tcl_Size) idx, &e) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (e == NULL) {
	    *res = Tcl_NewObj();	/* absent : "" (les niveaux suivants
					 * aussi, comme listIndexImm) */
	    return TCL_OK;
	}
	cur = e;
    }
    *res = cur;
    return TCL_OK;
}

Tcl_Obj *
TclTupleAssign(Tcl_Interp *interp, Tcl_Obj *names, Tcl_Obj *value,
	Tcl_Obj *paths)
{
    Tcl_Obj *content = DeepContent(value), **pv;
    Tcl_Size np, i;

    Tcl_IncrRefCount(content);
    if (Tcl_ListObjGetElements(interp, paths, &np, &pv) != TCL_OK) {
	goto error;
    }
    for (i = 0; i < np; i++) {
	/* chaque entrée : {LOCAL i0 i1 ...} ; LOCAL >= 0 : variable locale
	 * compilée (comme storeScalar), sinon le nom au même chemin dans
	 * NAMES (comme storeStk) */
	Tcl_Obj *name = NULL, *val = NULL, **ev;
	Tcl_Size ne;
	Tcl_WideInt lv;
	int ok;

	if (Tcl_ListObjGetElements(interp, pv[i], &ne, &ev) != TCL_OK
		|| ne < 2 || Tcl_GetWideIntFromObj(interp, ev[0], &lv) != TCL_OK) {
	    goto error;
	}
	Tcl_IncrRefCount(pv[i]);		/* ev reste valable */
	if (TupleIndex(interp, content, ev + 1, ne - 1, &val) != TCL_OK) {
	    Tcl_DecrRefCount(pv[i]);
	    goto error;
	}
	Tcl_IncrRefCount(val);
	if (lv >= 0) {
	    Interp *iPtr = (Interp *) interp;
	    Var *varPtr = &iPtr->varFramePtr->compiledLocals[lv];

	    while (TclIsVarLink(varPtr)) {
		varPtr = varPtr->value.linkPtr;
	    }
	    ok = (TclPtrSetVarIdx(interp, varPtr, NULL, NULL, NULL, val,
		    TCL_LEAVE_ERR_MSG, (int) lv) != NULL);
	} else if (TupleIndex(interp, names, ev + 1, ne - 1, &name) != TCL_OK) {
	    ok = 0;
	} else {
	    Tcl_IncrRefCount(name);
	    ok = (Tcl_ObjSetVar2(interp, name, NULL, val, TCL_LEAVE_ERR_MSG)
		    != NULL);
	    Tcl_DecrRefCount(name);
	}
	Tcl_DecrRefCount(val);
	Tcl_DecrRefCount(pv[i]);
	if (!ok) {
	    goto error;
	}
    }
    return content;
  error:
    Tcl_DecrRefCount(content);
    return NULL;
}

/* tcl::unsupported::typesversion : la version du prototype compilée
 * (TCL_TYPES_VERSION, tclTypes.h ; aussi dans typeparse/VERSION). */
static int
TypesVersionObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    (void) clientData;
    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(TCL_TYPES_VERSION, -1));
    return TCL_OK;
}

void
TclTypesInit(Tcl_Interp *interp)
{
    TypeData *d = (TypeData *) Tcl_Alloc(sizeof(TypeData));
    int isNew;

    memset(d, 0, sizeof(*d));
    {
	static size_t uids = 0;

	d->uid = ++uids;
	d->gen = 1;
    }
    d->refs = 1;
    Tcl_InitHashTable(&d->names, TCL_STRING_KEYS);
    Tcl_InitHashTable(&d->generics, TCL_STRING_KEYS);
    Tcl_InitHashTable(&d->instances, TCL_STRING_KEYS);
    Tcl_InitHashTable(&d->canon, TCL_STRING_KEYS);
    Tcl_InitHashTable(&d->parsed, TCL_STRING_KEYS);
    d->parsedGen = 0;
    Tcl_InitHashTable(&d->converts, TCL_STRING_KEYS);
    Tcl_InitHashTable(&d->derefs, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&d->lambdas, TCL_STRING_KEYS);
    d->tInt = Builtin(d, TCLT_INT, "int");
    d->tDouble = Builtin(d, TCLT_DOUBLE, "double");
    d->tString = Builtin(d, TCLT_STRING, "string");
    {
	/* v17z : inexact, un double arrondi depuis un entier (le cast
	 * l'accepte) ; un nominal de double, qui y entre */
	TclType *ix = Builtin(d, TCLT_DOUBLE, "inexact");

	ix->nominal = 1;
	ix->under = d->tDouble;
    }
    Builtin(d, TCLT_STRING, "any")->metaMask = TCL_META_ANY;
    Builtin(d, TCLT_STRING, "bare")->metaMask = TCL_META_BARE;	/* v17e */
    Builtin(d, TCLT_STRING, "typed")->metaMask = TCL_META_TYPED;
    Builtin(d, TCLT_STRING, "noted")->metaMask = TCL_META_NOTED;
    Builtin(d, TCLT_STRING, "typednoted")->metaMask = TCL_META_TYPED_NOTED;
    Builtin(d, TCLT_INT, "flag");		/* v16q : option sans valeur, 0/1 */
    d->tList = Builtin(d, TCLT_LIST, "list");
    d->tList->arity = -1;			/* liste extensible */
    d->tList->elem = d->tString;
    d->tDict = Builtin(d, TCLT_DICT, "dict");
    d->tDict->key = d->tDict->elem = d->tString;
    d->tVar = Builtin(d, TCLT_VAR, "var");
    d->tVar->elem = d->tString;
    /* at(EXPR).T : constructeur sans nom de type (voir tclTypeParse.c) */
    d->tAt = NewType(d, TCLT_LIST, "at");
    /* *(...).T : idem */
    d->tLink = NewType(d, TCLT_LIST, "*");
    /* "array" est réservé sans sémantique : entrée à valeur NULL. */
    Tcl_SetHashValue(Tcl_CreateHashEntry(&d->names, "array", &isNew), NULL);
    Tcl_SetAssocData(interp, "tclTypes", DeleteData, d);
    ((Interp *) interp)->typesData = d;
    d->interp = interp;
    d->opsGen = 1;
    Tcl_SetAssocData(interp, "tclTypesOps", NULL, (void *) &typesOpsApi);
    {
	int k;

	for (k = 0; k < tclBuiltinTypeSpecCount; k++) {
	    TclTypesRegisterOps(interp, &tclBuiltinTypeSpecs[k]);
	}
    }
    Tcl_CreateObjCommand2(interp, "type", TypeObjCmd, d, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::mathop::is", IsOpObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::unsupported::isvalue",
	    IsValueObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::unsupported::content", ContentObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::unsupported::declare", DeclareObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::unsupported::typesversion",
	    TypesVersionObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::unsupported::mapreport",
	    MapReportObjCmd, NULL, NULL);
    {
	/*
	 * v17z : num, le nombre, intégré : une union de ses représentations
	 * (num/int, num/double), lue comme expr lit un nombre.
	 */
	Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);
	Tcl_HashEntry *hPtr;

	(void) Tcl_EvalEx(interp, "::type def ::num {{int}int | {double}double}",
		-1, TCL_EVAL_GLOBAL);
	Tcl_RestoreInterpState(interp, st);
	hPtr = Tcl_FindHashEntry(&d->names, "::num");
	if (hPtr == NULL) {
	    hPtr = Tcl_FindHashEntry(&d->names, "num");
	}
	if (hPtr != NULL && Tcl_GetHashValue(hPtr) != NULL) {
	    ((TclType *) Tcl_GetHashValue(hPtr))->reserved = 1;
	}
    }
    Tcl_CreateObjCommand2(interp, "::tcl::unsupported::vecloop",
	    VecLoopObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::unsupported::abi", AbiObjCmd,
	    NULL, NULL);
    TclCompactInit(interp);
    Tcl_CreateObjCommand2(interp, "::tcl::mathfunc::popcount", PopcountCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::mathfunc::bitlen", BitlenCmd,
	    NULL, NULL);
    /* bitarray(n, ordre, boutisme) : type paramétré prédéfini. Le corps n'est
     * que "int" (le commentaire nomme les paramètres) ; la largeur, l'ordre
     * et le boutisme sont posés par l'instanciation (tclTypeGeneric.c).
     * Les autres types binaires, int(u,16,be), double(32,le) et
     * string(utf-8,8), sont construits par TclTypesBinaryType. */
    {
	static const char spec[] = "bitarray({int}n, {string}ord = lsb0, "
		"{string}e = auto)";

	if (TclTypesDefineGeneric(NULL, d, spec, (Tcl_Size) strlen(spec),
		"int # n ord e", 13, 0) == TCL_OK) {
	    TclTypesMarkBits(d, "bitarray", 1);
	}
    }
    TclAnnotInit(interp);
}

/* --------------------------------------------------------------------
 * v16p : procédures typées.
 *
 *   proc {pt}decale {{pt}p; {double}dx ?= 0; {double}dy ?= 0} {...}
 *
 * La liste d'arguments est un type produit : chaque champ devient une
 * variable locale qui porte son contrat ; un défaut (?= v, ?= [cmd]) rend le
 * champ facultatif ; à l'appel, des arguments positionnels puis des
 * étiquettes ({dy:}3) ; un dernier champ {list.T}args reçoit le reste ; un
 * champ {var.T}n lie n à la variable de l'appelant (upvar automatique), dont
 * la valeur courante est vérifiée. L'annotation du nom donne le type de
 * retour : le résultat est casté au retour normal ; une étiquette ({/v})
 * choisit la variante d'une somme ; {T}$x sur un return n'est qu'une note
 * locale, qui expire avec la procédure.
 *
 * v16q : options et drapeaux. Un champ dont le nom commence par « - » est
 * une option, donnée au début de l'appel (jusqu'au premier mot qui n'est pas
 * une option, ou « -- ») ; la variable locale perd le « - » ; un préfixe
 * unique suffit (-dec pour -decreasing).
 *   {flag}-decreasing           drapeau : la locale vaut 1 ou 0
 *   {int}-index ?= -1           option à valeur : -index 2
 *   {"ascii", "dictionary", {string}command}-mode ?= ascii
 *                               options exclusives (une somme) : -ascii,
 *                               -dictionary, -command cmd (ou -mode ascii) ;
 *                               la locale mode reçoit le nom de la variante,
 *                               la locale command sa valeur ; une variante
 *                               produit {{string}cmd; {int}-depth ?= 1}command
 *                               prend ses champs sans « - » à la suite de
 *                               l'option, et ses champs « -x » sont des
 *                               options qui dépendent d'elle
 *   {triopts}-o                 un groupe : les champs du produit triopts
 *                               sont les options (-decreasing, -index...),
 *                               la locale o est une valeur de triopts
 * -------------------------------------------------------------------- */

enum { OPT_NONE = 0, OPT_SCALAR, OPT_EXCL, OPT_GROUP };

typedef struct TypedArg {
    Tcl_Obj *name;		/* nom du champ, tel qu'écrit (-index) */
    Tcl_Obj *local;		/* la variable locale (index) */
    Tcl_Obj *typeText;		/* texte du type (DeclareTyped) */
    Tcl_Obj *defval;		/* ?= v */
    Tcl_Obj *defcmd;		/* ?= [cmd] : évalué dans la procédure */
    int isVar;			/* {var.T} : upvar */
    Tcl_Obj *varElem;		/* T de var.T, ou NULL */
    TclType *tp;		/* le type du champ (messages, défaut) */
    int allDefault;		/* produit dont tous les champs ont un défaut :
				 * l'argument absent prend ces défauts */
    int optKind;		/* OPT_* (v16q) */
    int isFlag;			/* OPT_SCALAR : {flag}-x */
    TclType *sub;		/* OPT_EXCL : la somme ; OPT_GROUP : le
				 * produit */
    Tcl_Obj *cache[2];		/* OPT_SCALAR : la valeur typée du défaut (ou
				 * 0) et de 1 (drapeau donné), partagées */
    size_t cacheGen;		/* génération des types de ce cache */
    TclType *ctp;		/* v16s : le type du champ, résolu (typeText) */
    size_t ctpGen;		/* ... pour cette génération des types */
    Tcl_Size lidx;		/* v16s : l'indice de la locale compilée (à
				 * vérifier par son nom), ou -1 */
    Tcl_Obj *lname;		/* ... et l'objet nom vu à cet indice (une
				 * référence : l'égalité des pointeurs suffit) */
    Tcl_Size group;		/* v16y : {T}nom... : mots par répétition (les
				 * champs d'un produit, sinon 1) ; 0 : pas
				 * variadique */
    int vmin;			/* ... au moins une répétition (sans ?=) */
} TypedArg;

/* Un mot d'option reconnu à l'appel. */
typedef struct OptEntry {
    Tcl_Obj *opt;		/* "-ascii" */
    Tcl_Size arg;		/* le champ de la signature */
    Tcl_Size variant;		/* EXCL : la variante ; GROUP : la variante du
				 * champ somme, sinon -1 */
    Tcl_Size gfield;		/* GROUP : le champ du produit, sinon -1 */
    Tcl_Size dep;		/* EXCL : champ « -x » de la variante produit
				 * (option dépendante), sinon -1 */
    int nvals;			/* mots pris après l'option */
    int byName;			/* -mode ascii : la valeur nomme la variante */
    int isFlag;
} OptEntry;

typedef struct TclTypedSig {
    size_t refCount;
    Tcl_Size n;			/* -1 : liste d'arguments classique */
    TypedArg *a;
    int rest;			/* le dernier champ est {list...}args */
    Tcl_Size npos;		/* champs positionnels (sans le reste) */
    Tcl_Size *pos;		/* leurs indices */
    Tcl_Size nopts;		/* mots d'option */
    OptEntry *opts;
    Tcl_Obj *optList;		/* -a, -b or -c (messages) */
    Tcl_Obj *argsText;		/* la signature, canonique */
    Tcl_Obj *retText;		/* le type de retour, ou NULL */
    int unfold;			/* v16r : {*}T (1, une liste), {**}T (2, un
				 * dict) : le résultat est rendu déplié */
    TclType *retTp;		/* v16s : retText résolu ... */
    size_t retGen;		/* ... pour cette génération des types */
    Tcl_Obj *usage;		/* p ?dx? ?dy? */
    Tcl_Obj **locals;		/* v16s : les noms des locales des champs */
    Tcl_Size vpos;		/* v16y : le champ variadique {T}nom...,
				 * indice dans pos[], ou -1 */
} TclTypedSig;

static Tcl_Obj *
TypeTextObj(const TclType *tp, int top)
{
    Tcl_DString ds;
    Tcl_Obj *o;

    Tcl_DStringInit(&ds);
    DefText(tp, top, &ds);
    o = Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);
    Tcl_IncrRefCount(o);
    return o;
}


/*
 * v17c : type canonic ?-notes? VALEUR, type parse TEXTE.
 *
 * La forme canonique d'une valeur typée : {T}contenu, ou {T@n1,n2}contenu
 * avec ses notes (un ensemble : triées, sans doublon, séparées par des
 * virgules ; dans une note, tout caractère ASCII hors lettres, chiffres, _
 * et . s'écrit %XX). T est le texte du type (son nom s'il est nommé, sans
 * la variante : une somme étiquetée la porte dans son contenu, une union la
 * lit dans le sien). Le contenu est le texte canonique, reconstruit selon
 * le type (listes, dicts, produits, sommes) et mis en forme d'un élément de
 * liste. Une valeur seulement notée s'écrit {@notes}contenu. Avec -notes :
 * T@notes, sans le contenu. type parse relit {…}contenu.
 */


/*
 * v17e : type meta VALEUR. La variante du méta-type (une énumération
 * "bare", "typed", "noted", "typednoted" : type variant la lit), notée de
 * ce qu'on sait du type : le type résolu d'une valeur typée ; pour une
 * valeur nue, sa représentation (le nom de son Tcl_ObjType, tel qu'il est :
 * int, list, dict, bytearray, cmdName...), aucune pour une chaîne pure.
 * La représentation est celle du moment, dans ce Tcl.
 */
static int
TypeMetaCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    static const char metaDef[] =
	    "\"bare\", \"typed\", \"noted\", \"typednoted\"";
    TclType *mt, *vt = NULL;
    TclMeta m;
    Tcl_Obj *v, *res, *note = NULL;
    int variant;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "value");
	return TCL_ERROR;
    }
    v = objv[2];
    m = TclAnnotMeta(v);
    mt = TclTypeParse(interp, metaDef, (Tcl_Size) strlen(metaDef));
    if (mt == NULL) {
	return TCL_ERROR;
    }
    variant = (m == TCL_META_BARE) ? 0 : (m == TCL_META_TYPED) ? 1
	    : (m == TCL_META_NOTED) ? 2 : 3;
    {
	Tcl_Obj *mn = Tcl_NewStringObj(TclMetaName(m), TCL_AUTO_LENGTH);

	Tcl_IncrRefCount(mn);		/* (TclNewAnnotated la copie) */
	res = TclNewAnnotated(interp, mt, variant, mn);
	Tcl_DecrRefCount(mn);
    }
    if (res == NULL) {
	return TCL_ERROR;
    }
    if (TclAnnotInfo(v, NULL, &vt, NULL, NULL) == 1 && vt != NULL) {
	note = TypeTextObj(vt, 0);	/* compte 1 */
    } else if (m == TCL_META_BARE && v->typePtr != NULL) {
	note = Tcl_NewStringObj(v->typePtr->name, TCL_AUTO_LENGTH);
	Tcl_IncrRefCount(note);
    }
    if (note != NULL) {
	Tcl_Obj *w = TclAnnotAddNote(res, note);

	Tcl_DecrRefCount(note);
	TclDiscard(res);
	res = w;
    }
    Tcl_SetObjResult(interp, res);
    return TCL_OK;
}

/* Le texte canonique du contenu v selon tp (compte 0, ou v lui-même). */
static Tcl_Obj *
CanonContent(const TclType *tp, int variant, Tcl_Obj *v, int depth)
{
    Tcl_Size n, i;
    Tcl_Obj **ev, *res, *e;
    int ev0Variant = -1, ownVariant = 0;

    if (TclIsAnnotated(v)) {
	TclType *et = NULL;

	if (TclAnnotInfo(v, NULL, &et, &ev0Variant, NULL) != 1 || et == NULL
		|| tp == NULL || TclTypeSumRoot(et) != TclTypeSumRoot(tp)) {
	    ev0Variant = -1;		/* (v18e : S/v compte comme S) */
	}
	v = TCL_ANNOT_CONTENT(v);
    }
    if (tp == NULL || depth > 64 || TclTypeIsAny(tp)) {
	return v;
    }
    switch (tp->kind) {
    case TCLT_LIST:
    case TCLT_DICT:
	if (tp->binDyn != NULL || TclIsCompact(v)
		|| Tcl_ListObjGetElements(NULL, v, &n, &ev) != TCL_OK) {
	    return v;			/* les cases s'écrivent déjà canoniques */
	}
	res = Tcl_NewListObj(n, NULL);
	for (i = 0; i < n; i++) {
	    if (tp->kind == TCLT_DICT && (i % 2) == 0) {
		e = ev[i];
		if (TclIsAnnotated(e)) {
		    e = TCL_ANNOT_CONTENT(e);
		}
	    } else {
		e = CanonContent(tp->elem, -1, ev[i], depth + 1);
	    }
	    Tcl_ListObjAppendElement(NULL, res, e);
	}
	return res;
    case TCLT_PRODUCT:
	if (TclIsCompact(v)
		|| Tcl_ListObjGetElements(NULL, v, &n, &ev) != TCL_OK
		|| n != tp->nfields) {
	    return v;
	}
	res = Tcl_NewListObj(n, NULL);
	for (i = 0; i < n; i++) {
	    e = CanonContent(tp->fields[i].type, -1, ev[i], depth + 1);
	    Tcl_ListObjAppendElement(NULL, res, e);
	}
	return res;
    case TCLT_SUM:
	if (tp->binTag != NULL) {
	    return v;
	}
	if (TclSumTagged(tp)) {
	    Tcl_Obj *payload = NULL, *pair[2];

	    variant = TclSumDecode(tp, v, &payload);
	    if (variant < 0 || variant >= (int) tp->nfields
		    || tp->fields[variant].type == NULL || payload == NULL
		    || Tcl_ListObjGetElements(NULL, v, &n, &ev) != TCL_OK
		    || n != 2) {
		return v;		/* un littéral : lui-même */
	    }
	    e = CanonContent(tp->fields[variant].type, -1, payload, depth + 1);
	    pair[0] = ev[0];
	    pair[1] = e;
	    return Tcl_NewListObj(2, pair);
	}
	ownVariant = (ev0Variant >= 0);
	if (variant < 0 && ev0Variant >= 0) {
	    variant = ev0Variant;	/* l'élément garde sa variante */
	}
	if (variant < 0) {
	    TclTypeValidate(NULL, tp, v, -1, &variant);
	}
	if (variant >= 0 && variant < (int) tp->nfields
		&& tp->fields[variant].type == NULL && ownVariant && depth > 1
		&& tp->fields[variant].literal != NULL) {
	    /* v18e : un littéral dans une charge, sa variante gardée :
	     * {/none}none (la chaîne d'étiquettes du niveau au-dessus le
	     * reprend : {/m/none}none) ; un littéral qui n'est pas un nom
	     * reste sa valeur */
	    const char *lt = Tcl_GetString(tp->fields[variant].literal), *q;

	    for (q = lt; *q != '\0' && TclTypeIsNameChar(*q); q++) {
	    }
	    if (*q == '\0' && q > lt) {
		Tcl_Obj *one = Tcl_NewListObj(1, &v), *lab;

		Tcl_IncrRefCount(one);
		lab = Tcl_ObjPrintf("{/%s}%s", lt, Tcl_GetString(one));
		Tcl_DecrRefCount(one);
		return lab;
	    }
	}
	if (variant < 0 || variant >= (int) tp->nfields
		|| tp->fields[variant].type == NULL) {
	    return v;
	}
	e = CanonContent(tp->fields[variant].type, -1, v, depth + 1);
	if (depth > 0 && tp->fields[variant].name != NULL) {
	    /* v17z : dans une structure, l'étiquette quand la chaîne ne dit
	     * pas la variante : {s:}5 */
	    int dv = -1;

	    TclType *pt = NULL;
	    int nested = (TclAnnotInfo(v, NULL, &pt, NULL, NULL) == 1
		    && pt != NULL && pt->kind == TCLT_SUM)
		    || (ownVariant && depth > 1);	/* dans une charge */

	    Tcl_IncrRefCount(e);
	    /* v18e : une charge qui est elle-même une valeur d'une somme
	     * (sa variante gardée) s'écrit toujours avec sa chaîne
	     * d'étiquettes ; une étiquette dans e aussi */
	    if (nested || (Tcl_GetString(e)[0] == '{'
		    && Tcl_GetString(e)[1] == '/')
		    || TclTypeValidate(NULL, tp, e, -1, &dv) != TCL_OK
		    || dv != variant) {
		Tcl_Obj *one = Tcl_NewListObj(1, &e), *lab;
		const char *et = Tcl_GetString(e), *cl;

		Tcl_IncrRefCount(one);
		if (et[0] == '{' && et[1] == '/' && (cl = strchr(et, '}'))
			!= NULL && memchr(et + 1, '{', cl - et - 1) == NULL) {
		    /* v18e : la charge, elle-même étiquetée ({/celsius}1.0) :
		     * une chaîne d'étiquettes, {/some/celsius}1.0 */
		    lab = Tcl_ObjPrintf("{/%s%.*s",
			    Tcl_GetString(tp->fields[variant].name),
			    (int) (cl - et - 1), et + 1);
		    Tcl_AppendToObj(lab, cl, -1);
		} else
		lab = Tcl_ObjPrintf("{/%s}%s",
			Tcl_GetString(tp->fields[variant].name),
			Tcl_GetString(one));
		Tcl_DecrRefCount(one);
		Tcl_DecrRefCount(e);
		return lab;
	    }
	    e->refCount--;		/* (rendu sans référence) */
	}
	return e;
    case TCLT_INT:
    case TCLT_DOUBLE:
	e = TclTypeNormalizeContent((TclType *) tp, v);
	return (e != NULL) ? e : v;	/* un élément seulement noté */
    default:
	return v;
    }
}

static void
CanonNote(const char *s, Tcl_DString *ds)
{
    const unsigned char *p;
    char buf[4];

    for (p = (const unsigned char *) s; *p; p++) {
	if (isalnum(*p) || *p == '_' || *p == '.' || *p >= 0x80) {
	    Tcl_DStringAppend(ds, (const char *) p, 1);
	} else {
	    snprintf(buf, sizeof(buf), "%%%02X", *p);
	    Tcl_DStringAppend(ds, buf, 3);
	}
    }
}

static int
CanonNoteCmp(const void *a, const void *b)
{
    return strcmp(Tcl_GetString(*(Tcl_Obj *const *) a),
	    Tcl_GetString(*(Tcl_Obj *const *) b));
}

static int
TypeCanonicCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    int onlyNotes = 0, variant = -1, kind;
    Tcl_Obj *val, *content = NULL, *notes, **nv, **sorted, *tt, *c, *res;
    TclType *tp = NULL;
    Tcl_Size nn = 0, i;
    Tcl_DString ds;

    if (objc == 4 && strcmp(Tcl_GetString(objv[2]), "-notes") == 0) {
	onlyNotes = 1;
    } else if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "?-notes? value");
	return TCL_ERROR;
    }
    val = objv[objc - 1];
    if (TclIsAnnotated(val)) {
	/* le mot reçu est réglé d'abord : type canonic {int@a}$x (v17f) */
	Tcl_Obj *sv = NULL;

	if (TclAnnotSettle(interp, val, NULL, 0, &sv) != TCL_OK) {
	    return TCL_ERROR;
	}
	val = sv;
    }
    Tcl_IncrRefCount(val);
    kind = TclAnnotInfo(val, &content, &tp, &variant, NULL);
    notes = TclAnnotNoteList(interp, val);
    Tcl_IncrRefCount(notes);
    Tcl_ListObjGetElements(NULL, notes, &nn, &nv);
    if ((kind != 1 || tp == NULL) && nn == 0) {
	/* v17e : une valeur nue, {bare}texte (type parse la relit nue) */
	Tcl_Obj *one = Tcl_NewStringObj(Tcl_GetString(
		TclIsAnnotated(val) ? TCL_ANNOT_CONTENT(val) : val),
		TCL_AUTO_LENGTH), *l;

	Tcl_DecrRefCount(notes);
	if (onlyNotes) {
	    Tcl_DecrRefCount(val);
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("bare", 4));
	    return TCL_OK;
	}
	l = Tcl_NewListObj(1, &one);
	Tcl_IncrRefCount(l);
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("{bare}%s", Tcl_GetString(l)));
	Tcl_DecrRefCount(l);
	Tcl_DecrRefCount(val);
	return TCL_OK;
    }
    if (kind != 1 || tp == NULL) {
	tp = NULL;
	content = TclIsAnnotated(val) ? TCL_ANNOT_CONTENT(val) : val;
    }
    Tcl_DStringInit(&ds);
    if (!onlyNotes) {
	Tcl_DStringAppend(&ds, "{", 1);
    }
    if (tp != NULL) {
	Tcl_Obj *chain = Tcl_NewObj();

	tt = TypeTextObj(tp, 0);
	Tcl_DStringAppend(&ds, Tcl_GetString(tt), TCL_AUTO_LENGTH);
	Tcl_DecrRefCount(tt);
	Tcl_IncrRefCount(chain);
	if (tp->kind == TCLT_SUM && variant >= 0 && variant < tp->nfields) {
	    /* v18b : la charge garde sa variante (mesure/some/celsius) : la
	     * chaîne ne la dit pas, le texte du type l'écrit */
	    TclAppendPayloadChain(chain, tp, variant, content);
	}
	if (Tcl_GetCharLength(chain) > 0) {
	    if (tp->variantOf == NULL) {
		Tcl_DStringAppend(&ds, "/", 1);
		TclTypeAppendMemberName(&ds, MemberName(tp, variant));
	    }
	    Tcl_DStringAppend(&ds, Tcl_GetString(chain), TCL_AUTO_LENGTH);
	} else if (tp->kind == TCLT_SUM && !TclSumTagged(tp)
		&& tp->binTag == NULL
		&& tp->variantOf == NULL
		&& variant >= 0 && variant < tp->nfields
		&& tp->fields[variant].name != NULL
		&& tp->fields[variant].type != NULL) {
	    /* v17z : la variante, quand la chaîne ne la dit pas : {S/s}5 */
	    int dv = -1;
	    Tcl_Obj *cc = CanonContent(tp, variant, content, 0);

	    Tcl_IncrRefCount(cc);
	    if (TclTypeValidate(NULL, tp, cc, -1, &dv) != TCL_OK
		    || dv != variant) {
		Tcl_DStringAppend(&ds, "/", 1);
		Tcl_DStringAppend(&ds, Tcl_GetString(
			tp->fields[variant].name), TCL_AUTO_LENGTH);
	    }
	    Tcl_DecrRefCount(cc);
	}
	Tcl_DecrRefCount(chain);
    }
    if (nn > 0) {
	sorted = (Tcl_Obj **) Tcl_Alloc(nn * sizeof(Tcl_Obj *));
	memcpy(sorted, nv, nn * sizeof(Tcl_Obj *));
	qsort(sorted, nn, sizeof(Tcl_Obj *), CanonNoteCmp);
	Tcl_DStringAppend(&ds, "@", 1);
	for (i = 0; i < nn; i++) {
	    if (i > 0 && strcmp(Tcl_GetString(sorted[i]),
		    Tcl_GetString(sorted[i - 1])) == 0) {
		continue;		/* un ensemble */
	    }
	    if (i > 0) {
		Tcl_DStringAppend(&ds, ",", 1);
	    }
	    CanonNote(Tcl_GetString(sorted[i]), &ds);
	}
	Tcl_Free(sorted);
    }
    Tcl_DecrRefCount(notes);
    if (!onlyNotes) {
	Tcl_DStringAppend(&ds, "}", 1);
	c = CanonContent(tp, variant, content, 0);
	Tcl_IncrRefCount(c);
	{
	    Tcl_Obj *one = Tcl_NewStringObj(Tcl_GetString(c), TCL_AUTO_LENGTH);
	    Tcl_Obj *l = Tcl_NewListObj(1, &one);

	    Tcl_IncrRefCount(l);
	    Tcl_DStringAppend(&ds, Tcl_GetString(l), TCL_AUTO_LENGTH);
	    Tcl_DecrRefCount(l);
	}
	Tcl_DecrRefCount(c);
    }
    res = Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);
    Tcl_DecrRefCount(val);
    Tcl_SetObjResult(interp, res);
    return TCL_OK;
}

static int
HexVal(int c)
{
    return (c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'F')
	    ? c - 'A' + 10 : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
}

/*
 * v17z : type parse relit les étiquettes que type canonic écrit dans une
 * structure ({s:}5, élément d'une list.S) : l'élément devient une valeur
 * annotée de sa variante. Rend un objet neuf, ou NULL si rien ne change.
 */
static Tcl_Obj *
ParseLabels(Tcl_Interp *interp, TclType *tp, Tcl_Obj *v, int depth,
	int *errPtr)
{
    Tcl_Size n, i;
    Tcl_Obj **ev, *res = NULL, *e;

    if (tp == NULL || depth > 64 || *errPtr) {
	return NULL;
    }
    switch (tp->kind) {
    case TCLT_LIST:
    case TCLT_DICT:
    case TCLT_PRODUCT:
	if (tp->binDyn != NULL
		|| Tcl_ListObjGetElements(NULL, v, &n, &ev) != TCL_OK
		|| (tp->kind == TCLT_PRODUCT && n != tp->nfields)) {
	    return NULL;
	}
	for (i = 0; i < n; i++) {
	    TclType *et = (tp->kind == TCLT_PRODUCT) ? tp->fields[i].type
		    : (tp->kind == TCLT_DICT && (i % 2) == 0) ? NULL : tp->elem;

	    e = ParseLabels(interp, et, ev[i], depth + 1, errPtr);
	    if (*errPtr) {
		if (res != NULL) {
		    TclDiscard(res);
		}
		return NULL;
	    }
	    if (e != NULL && res == NULL) {
		res = Tcl_NewListObj(n, ev);
	    }
	    if (e != NULL) {
		Tcl_ListObjReplace(NULL, res, i, 1, 1, &e);
	    }
	}
	return res;
    case TCLT_SUM: {
	const char *t = Tcl_GetString(v), *c;
	Tcl_Size idx;
	Tcl_Obj *pl, **pv, *inner;
	Tcl_Size np;

	if (tp->binTag != NULL || t[0] != '{' || t[1] != '/'
		|| (c = strchr(t, '}')) == NULL
		|| (strchr(t + 1, '{') != NULL && strchr(t + 1, '{') < c)) {
	    return NULL;
	}
	{
	    /* {/v}charge (v18a) ; {/v/w}charge (v18e) : la suite de la chaîne
	     * étiquette la charge, relue dans le type de la variante */
	    const char *sl = (const char *) memchr(t + 2, '/', c - t - 2);
	    Tcl_Obj *nm = Tcl_NewStringObj(t + 2, (sl != NULL ? sl : c) - t
		    - 2);

	    Tcl_IncrRefCount(nm);
	    idx = TclTypeFieldIndex(tp, Tcl_GetString(nm));
	    if (idx < 0) {
		idx = SumLiteralIndex(tp, nm);
	    }
	    Tcl_DecrRefCount(nm);
	    if (sl != NULL && idx >= 0 && tp->fields[idx].type != NULL) {
		Tcl_Obj *one, *rest;

		pl = Tcl_NewStringObj(c + 1, TCL_AUTO_LENGTH);
		Tcl_IncrRefCount(pl);
		if (Tcl_ListObjGetElements(NULL, pl, &np, &pv) != TCL_OK
			|| np != 1) {
		    Tcl_DecrRefCount(pl);
		    return NULL;
		}
		one = Tcl_NewListObj(1, pv);
		Tcl_IncrRefCount(one);
		rest = Tcl_ObjPrintf("{%.*s}%s", (int) (c - sl), sl,
			Tcl_GetString(one));
		Tcl_DecrRefCount(one);
		Tcl_DecrRefCount(pl);
		Tcl_IncrRefCount(rest);
		e = ParseLabels(interp, tp->fields[idx].type, rest, depth + 1,
			errPtr);
		Tcl_DecrRefCount(rest);
		if (e == NULL || *errPtr) {
		    if (e != NULL) {
			Tcl_BounceRefCount(e);
		    }
		    if (!*errPtr) {
			ERR(interp, "PARSE", "type parse: bad label chain in "
				"\"%s\"", t);
			*errPtr = 1;
		    }
		    return NULL;
		}
		inner = e;
		Tcl_IncrRefCount(inner);
		goto haveInner;
	    }
	}
	if (idx < 0) {
	    return NULL;
	}
	pl = Tcl_NewStringObj(c + 1, TCL_AUTO_LENGTH);
	Tcl_IncrRefCount(pl);
	if (Tcl_ListObjGetElements(NULL, pl, &np, &pv) != TCL_OK
		|| np != 1) {
	    Tcl_DecrRefCount(pl);
	    return NULL;
	}
	inner = Tcl_DuplicateObj(pv[0]);
	Tcl_DecrRefCount(pl);
	Tcl_IncrRefCount(inner);
	if (tp->fields[idx].type != NULL) {
	    e = ParseLabels(interp, tp->fields[idx].type, inner, depth + 1,
		    errPtr);
	    if (e != NULL) {
		Tcl_DecrRefCount(inner);
		inner = e;
		Tcl_IncrRefCount(inner);
	    }
	}
      haveInner:
	if (*errPtr || TclTypeComplete(interp, tp, inner, (int) idx, NULL)
		!= TCL_OK) {
	    *errPtr = 1;
	    Tcl_DecrRefCount(inner);
	    return NULL;
	}
	res = TclNewAnnotated(interp, tp, (int) idx, inner);
	Tcl_DecrRefCount(inner);
	return res;
    }
    default:
	return NULL;
    }
}

static int
TypeParseCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Size len, i, close = -1, at = -1, nc;
    const char *text;
    int depth = 0, rc;
    Tcl_Obj *rest, **cv, *val, *pushed, *res = NULL, *hold = NULL;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "text");
	return TCL_ERROR;
    }
    text = Tcl_GetStringFromObj(objv[2], &len);
    if (len < 2 || text[0] != '{') {
	goto bad;
    }
    for (i = 0; i < len; i++) {
	if (text[i] == '\\' && i + 1 < len) {
	    i++;
	} else if (text[i] == '{') {
	    depth++;
	} else if (text[i] == '}' && --depth == 0) {
	    close = i;
	    break;
	} else if (text[i] == '@' && depth == 1) {
	    at = i;			/* le dernier @ du premier niveau */
	}
    }
    if (close < 0) {
	goto bad;
    }
    rest = Tcl_NewStringObj(text + close + 1, len - close - 1);
    Tcl_IncrRefCount(rest);
    if (Tcl_ListObjGetElements(NULL, rest, &nc, &cv) != TCL_OK || nc != 1) {
	Tcl_DecrRefCount(rest);
	goto bad;
    }
    val = Tcl_DuplicateObj(cv[0]);	/* le contenu, sans le type */
    Tcl_DecrRefCount(rest);
    pushed = val;
    if ((at < 0 ? close : at) > 1) {
	/* le type d'abord : il doit exister, et le contenu y entrer */
	Tcl_Size tl = (at < 0 ? close : at) - 1;
	Tcl_Obj *typed = NULL, *lv;
	TclType *ptp = NULL;
	int perr = 0, pvar = -1;

	{
	    /* v17z : {S/v}charge, la variante d'une somme (v18b : un type de
	     * variante) */
	    Tcl_InterpState ist = Tcl_SaveInterpState(interp, TCL_OK);
	    TclType *st = TclTypeParse(interp, text + 1, tl);

	    Tcl_RestoreInterpState(interp, ist);
	    if (st != NULL && st->variantOf != NULL) {
		ptp = st;
		pvar = (int) st->variantIdx;
	    }
	}
	if (ptp == NULL && (ptp = TclTypeParse(interp, text + 1, tl)) == NULL) {
	    Tcl_BounceRefCount(val);
	    return TCL_ERROR;
	}
	if (pvar >= 0 && ptp->fields[pvar].type != NULL) {
	    lv = ParseLabels(interp, ptp->fields[pvar].type, val, 1, &perr);
	    if (perr) {
		Tcl_BounceRefCount(val);
		return TCL_ERROR;
	    }
	    if (lv != NULL) {
		Tcl_BounceRefCount(val);
		val = lv;
	    }
	    Tcl_IncrRefCount(val);
	    if (TclTypeComplete(interp, ptp, val, pvar, NULL) != TCL_OK) {
		Tcl_DecrRefCount(val);
		return TCL_ERROR;
	    }
	    typed = TclNewAnnotated(interp, ptp, pvar, val);
	    Tcl_DecrRefCount(val);
	    Tcl_IncrRefCount(typed);
	    pushed = typed;
	    if (at < 0) {
		Tcl_SetObjResult(interp, typed);
		Tcl_DecrRefCount(typed);
		return TCL_OK;
	    }
	    hold = typed;
	    goto notes;
	}
	lv = ParseLabels(interp, ptp, val, 0, &perr);	/* v17z */
	if (perr) {
	    Tcl_BounceRefCount(val);
	    return TCL_ERROR;
	}
	if (lv != NULL) {
	    Tcl_BounceRefCount(val);
	    val = lv;
	}
	Tcl_IncrRefCount(val);
	{
	    Tcl_Obj *tx = Tcl_NewStringObj(text + 1, tl);

	    Tcl_IncrRefCount(tx);
	    pushed = TclAnnotPush(val, tx);
	    Tcl_IncrRefCount(pushed);
	    Tcl_DecrRefCount(tx);
	}
	rc = TclAnnotSettle(interp, pushed, NULL, 0, &typed);
	if (rc == TCL_OK) {
	    Tcl_IncrRefCount(typed);
	}
	Tcl_DecrRefCount(pushed);
	Tcl_DecrRefCount(val);
	if (rc != TCL_OK) {
	    return rc;
	}
	pushed = typed;
	if (at < 0) {
	    Tcl_SetObjResult(interp, typed);
	    Tcl_DecrRefCount(typed);
	    return TCL_OK;
	}
	hold = typed;			/* les notes s'empilent dessus */
    }
  notes:
    if (at >= 0) {
	/* les notes, décodées, empilées dans l'ordre */
	Tcl_DString nd;
	const char *p = text + at + 1, *end = text + close;

	Tcl_DStringInit(&nd);
	Tcl_IncrRefCount(pushed);	/* (une référence à nous, v17z) */
	while (p <= end) {
	    if (p == end || *p == ',') {
		Tcl_Obj *nt, *np;

		if (Tcl_DStringLength(&nd) == 0) {
		    Tcl_DStringFree(&nd);
		    Tcl_DecrRefCount(pushed);
		    if (hold != NULL) {
			Tcl_DecrRefCount(hold);
		    }
		    goto bad;
		}
		nt = Tcl_NewStringObj(Tcl_DStringValue(&nd),
			Tcl_DStringLength(&nd));
		Tcl_IncrRefCount(nt);
		np = TclAnnotPush(pushed, nt);
		Tcl_IncrRefCount(np);
		Tcl_DecrRefCount(nt);
		Tcl_DecrRefCount(pushed);
		pushed = np;
		Tcl_DStringSetLength(&nd, 0);
	    } else if (*p == '%') {
		int h1 = (p + 2 < end) ? HexVal(p[1]) : -1;
		int h2 = (h1 >= 0) ? HexVal(p[2]) : -1;
		char ch;

		if (h2 < 0) {
		    Tcl_DStringFree(&nd);
		    Tcl_DecrRefCount(pushed);
		    if (hold != NULL) {
			Tcl_DecrRefCount(hold);
		    }
		    goto bad;
		}
		ch = (char) (h1 * 16 + h2);
		Tcl_DStringAppend(&nd, &ch, 1);
		p += 2;
	    } else {
		Tcl_DStringAppend(&nd, p, 1);
	    }
	    p++;
	}
	Tcl_DStringFree(&nd);
    } else {
	Tcl_IncrRefCount(pushed);
    }
    rc = TclAnnotSettle(interp, pushed, NULL, 0, &res);
    if (rc == TCL_OK) {
	Tcl_SetObjResult(interp, res);
    }
    Tcl_DecrRefCount(pushed);
    if (hold != NULL) {
	Tcl_DecrRefCount(hold);
    }
    return rc;

  bad:
    ERR(interp, "PARSE", "type parse: \"%s\" is not {TYPE}content, "
	    "{TYPE@notes}content or {@notes}content", text);
    return TCL_ERROR;
}

void
TclTypedSigRetain(void *sigv)
{
    if (sigv != NULL) {
	((TclTypedSig *) sigv)->refCount++;
    }
}

static void
DecrIf(Tcl_Obj *o)
{
    if (o != NULL) {
	Tcl_DecrRefCount(o);
    }
}

void
TclTypedSigRelease(void *sigv)
{
    TclTypedSig *sig = (TclTypedSig *) sigv;
    Tcl_Size i;

    if (sig == NULL || --sig->refCount > 0) {
	return;
    }
    for (i = 0; i < sig->n; i++) {
	TypedArg *a = &sig->a[i];

	DecrIf(a->name);
	DecrIf(a->local);
	DecrIf(a->typeText);
	DecrIf(a->defval);
	DecrIf(a->defcmd);
	DecrIf(a->varElem);
	DecrIf(a->cache[0]);
	DecrIf(a->cache[1]);
	DecrIf(a->lname);
    }
    for (i = 0; i < sig->nopts; i++) {
	DecrIf(sig->opts[i].opt);
    }
    if (sig->opts != NULL) {
	Tcl_Free(sig->opts);
    }
    if (sig->locals != NULL) {
	Tcl_Free(sig->locals);
    }
    if (sig->pos != NULL) {
	Tcl_Free(sig->pos);
    }
    if (sig->a != NULL) {
	Tcl_Free(sig->a);
    }
    DecrIf(sig->optList);
    DecrIf(sig->argsText);
    DecrIf(sig->retText);
    DecrIf(sig->usage);
    Tcl_Free(sig);
}

/* Le type de retour : un type, ou une somme (les étiquettes {/v}). */
static int
RetTypeCheck(Tcl_Interp *interp, Tcl_Obj *text)
{
    Tcl_Size l;
    const char *t = Tcl_GetStringFromObj(text, &l);
    TclType *tp = TclTypeParse(interp, t, l);

    if (tp == NULL) {
	const char *dot = strrchr(t, '/');
	TclType *sum = (dot != NULL && dot > t)
		? TclTypeParse(interp, t, dot - t) : NULL;

	if (sum != NULL && sum->kind == TCLT_SUM
		&& TclTypeFieldIndex(sum, dot + 1) >= 0) {
	    Tcl_ResetResult(interp);
	    return TCL_OK;		/* {S/v}f : toujours la variante v */
	}
	(void) TclTypeParse(interp, t, l);	/* le message */
	return TCL_ERROR;
    }
    return TCL_OK;
}

/* Une liste invalide qui a un {T}nom (une accolade fermante collée à un
 * nom) : une signature typée ; sinon l'erreur classique de la liste. */
static int
LooksTyped(const char *t, Tcl_Size len)
{
    Tcl_Size i;

    for (i = 0; i + 1 < len; i++) {
	if (t[i] == '}' && !TclIsSpaceProc(t[i + 1]) && t[i + 1] != '}'
		&& t[i + 1] != '{' && t[i + 1] != ';') {
	    return 1;
	}
    }
    return 0;
}

/* v16q : le type de kind sous un nominal, ou NULL. */
static const TclType *
UnderKind(const TclType *tp, TclTypeKind kind)
{
    while (tp != NULL && tp->kind != kind && tp->under != NULL) {
	tp = tp->under;
    }
    return (tp != NULL && tp->kind == kind) ? tp : NULL;
}

static int
IsFlagType(const TclType *tp)
{
    return tp != NULL && tp->reserved && strcmp(tp->name, "flag") == 0;
}

/* Le nom d'une variante : son nom, ou le texte d'une variante littérale. */
static Tcl_Obj *
VariantName(const TclTypeField *f)
{
    return f->name != NULL ? f->name : f->literal;
}

/* Le nom sans son « - » éventuel. */
static Tcl_Obj *
OptLocalName(Tcl_Obj *name)
{
    Tcl_Size l;
    const char *t = Tcl_GetStringFromObj(name, &l);

    if (l > 1 && t[0] == '-') {
	return Tcl_NewStringObj(t + 1, l - 1);
    }
    if (l > 3 && memcmp(t + l - 3, "...", 3) == 0) {
	return Tcl_NewStringObj(t, l - 3);	/* v16y : args... */
    }
    return name;
}

static int
AddOpt(Tcl_Interp *interp, TclTypedSig *sig, Tcl_Size *capPtr,
	Tcl_Obj *name, Tcl_Size arg, Tcl_Size variant, Tcl_Size gfield,
	Tcl_Size dep, int nvals, int byName, int isFlag)
{
    Tcl_Size l, i;
    const char *t = Tcl_GetStringFromObj(name, &l);
    OptEntry *e;
    Tcl_Obj *opt;

    opt = (l > 0 && t[0] == '-') ? name
	    : Tcl_ObjPrintf("-%s", t);	/* les variantes : sans « - » */
    Tcl_IncrRefCount(opt);
    for (i = 0; i < sig->nopts; i++) {
	if (strcmp(Tcl_GetString(sig->opts[i].opt), Tcl_GetString(opt)) == 0) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("option \"%s\" is defined "
		    "twice", Tcl_GetString(opt)));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "PROCARGS", (char *) NULL);
	    Tcl_DecrRefCount(opt);
	    return TCL_ERROR;
	}
    }
    if (sig->nopts == *capPtr) {
	*capPtr = (*capPtr == 0) ? 8 : 2 * *capPtr;
	sig->opts = (OptEntry *) Tcl_Realloc(sig->opts,
		sizeof(OptEntry) * *capPtr);
    }
    e = &sig->opts[sig->nopts++];
    e->opt = opt;
    e->arg = arg;
    e->variant = variant;
    e->gfield = gfield;
    e->dep = dep;
    e->nvals = nvals;
    e->byName = byName;
    e->isFlag = isFlag;
    return TCL_OK;
}

/* Les options d'une somme (options exclusives) : une par variante ; une
 * variante produit prend ses champs sans « - » à la suite, ses champs
 * « -x » sont des options dépendantes. gfield >= 0 : dans un groupe. */
static int
AddSumOpts(Tcl_Interp *interp, TclTypedSig *sig, Tcl_Size *capPtr,
	const TclType *sum, Tcl_Size arg, Tcl_Size gfield)
{
    Tcl_Size v, d;

    for (v = 0; v < sum->nfields; v++) {
	const TclTypeField *f = &sum->fields[v];
	const TclType *vp;
	Tcl_Obj *vn = VariantName(f);

	if (vn == NULL) {
	    continue;
	}
	if (f->type == NULL || IsFlagType(f->type)) {
	    if (AddOpt(interp, sig, capPtr, vn, arg, v, gfield, -1, 0, 0, 1)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	} else if (gfield < 0 && (vp = UnderKind(f->type, TCLT_PRODUCT))
		!= NULL) {
	    int nv = 0;

	    for (d = 0; d < vp->nfields; d++) {
		if (vp->fields[d].name != NULL
			&& Tcl_GetString(vp->fields[d].name)[0] != '-') {
		    nv++;
		}
	    }
	    if (AddOpt(interp, sig, capPtr, vn, arg, v, -1, -1, nv, 0, 0)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	    for (d = 0; d < vp->nfields; d++) {
		const TclTypeField *df = &vp->fields[d];

		if (df->name != NULL && Tcl_GetString(df->name)[0] == '-') {
		    int fl = IsFlagType(df->type);

		    if (AddOpt(interp, sig, capPtr, df->name, arg, v, -1, d,
			    fl ? 0 : 1, 0, fl) != TCL_OK) {
			return TCL_ERROR;
		    }
		}
	    }
	} else if (AddOpt(interp, sig, capPtr, vn, arg, v, gfield, -1, 1, 0, 0)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static Tcl_Size
SumVariantByName(const TclType *sum, const char *name)
{
    Tcl_Size v;

    for (v = 0; v < sum->nfields; v++) {
	Tcl_Obj *vn = VariantName(&sum->fields[v]);

	if (vn != NULL && strcmp(Tcl_GetString(vn), name) == 0) {
	    return v;
	}
    }
    return -1;
}

static void
AppendSumUsage(Tcl_Obj *u, const TclType *sum)
{
    Tcl_Size v, d;

    Tcl_AppendToObj(u, "?", 1);
    for (v = 0; v < sum->nfields; v++) {
	const TclTypeField *f = &sum->fields[v];
	const TclType *vp;
	Tcl_Obj *vn = VariantName(f);

	if (vn == NULL) {
	    continue;
	}
	Tcl_AppendStringsToObj(u, v > 0 ? "|-" : "-", Tcl_GetString(vn),
		(char *) NULL);
	if (f->type == NULL || IsFlagType(f->type)) {
	    continue;
	}
	if ((vp = UnderKind(f->type, TCLT_PRODUCT)) != NULL) {
	    for (d = 0; d < vp->nfields; d++) {
		const char *dn = vp->fields[d].name
			? Tcl_GetString(vp->fields[d].name) : "?";

		if (dn[0] != '-') {
		    Tcl_AppendStringsToObj(u, " ", dn, (char *) NULL);
		}
	    }
	    for (d = 0; d < vp->nfields; d++) {
		const char *dn = vp->fields[d].name
			? Tcl_GetString(vp->fields[d].name) : "?";

		if (dn[0] == '-') {
		    Tcl_AppendStringsToObj(u, " ?", dn,
			    IsFlagType(vp->fields[d].type) ? "?" : " value?",
			    (char *) NULL);
		}
	    }
	} else {
	    Tcl_AppendToObj(u, " value", -1);
	}
    }
    Tcl_AppendToObj(u, "?", 1);
}

static int
CompareObjStr(const void *a, const void *b)
{
    return strcmp(Tcl_GetString(*(Tcl_Obj *const *) a),
	    Tcl_GetString(*(Tcl_Obj *const *) b));
}

/*
 * v16r : {*}T, {**}T en tête d'une annotation de livraison (type de retour
 * d'une procédure, type {…}mot) : la valeur est vérifiée comme un T, puis
 * rendue DÉPLIÉE, en données Tcl ordinaires : {*} une liste (les valeurs,
 * comme $P(*)), {**} un dict (les noms et les valeurs), en profondeur. Rend
 * 0, 1 ou 2 ; *textPtr perd le préfixe (nouvel objet, référence gardée).
 */
static int
UnfoldPrefix(Tcl_Obj **textPtr)
{
    Tcl_Size l;
    const char *t = Tcl_GetStringFromObj(*textPtr, &l);
    int mode = 0, skip = 0;

    if (l >= 4 && strncmp(t, "{**}", 4) == 0) {
	mode = 2;
	skip = 4;
    } else if (l >= 3 && strncmp(t, "{*}", 3) == 0) {
	mode = 1;
	skip = 3;
    }
    if (mode != 0) {
	Tcl_Obj *rest = Tcl_NewStringObj(t + skip, l - skip);

	Tcl_IncrRefCount(rest);
	Tcl_DecrRefCount(*textPtr);
	*textPtr = rest;
    }
    return mode;
}

/* {**} : la valeur en dicts (produits : nom -> valeur ; somme : nom de la
 * variante -> valeur ; listes et dicts : leurs éléments), en profondeur.
 * Objet neuf ou partagé, sans référence propre. */
static Tcl_Obj *
DictUnfold(Tcl_Obj *obj, const TclType *tp)
{
    Tcl_Obj *content = NULL, **elv, *res;
    TclType *own = NULL;
    int variant = -1;
    Tcl_Size n, i;

    if (TclAnnotInfo(obj, &content, &own, &variant, NULL) == 1
	    && content != NULL && own != NULL) {
	tp = own;
	obj = content;
    } else {
	variant = -1;
	obj = TCL_ANNOT_CONTENT(obj);
    }
    while (tp != NULL && tp->under != NULL && tp->kind != TCLT_PRODUCT
	    && tp->kind != TCLT_LIST && tp->kind != TCLT_DICT
	    && tp->kind != TCLT_SUM) {
	tp = tp->under;
    }
    if (tp == NULL || (tp->kind != TCLT_PRODUCT && tp->kind != TCLT_LIST
	    && tp->kind != TCLT_DICT && tp->kind != TCLT_SUM)) {
	return DeepContent(obj);
    }
    if (TclIsCompact(obj)) {
	/* en cases : la liste des éléments gardée à côté des cases */
	Tcl_Obj *c = TclCompactCache(obj);

	if (c == NULL) {
	    return DeepContent(obj);
	}
	obj = c;
    }
    switch (tp->kind) {
    case TCLT_PRODUCT:
	res = Tcl_NewDictObj();
	if (Tcl_ListObjGetElements(NULL, obj, &n, &elv) == TCL_OK) {
	    for (i = 0; i < n && i < tp->nfields; i++) {
		const TclTypeField *f = &tp->fields[i];

		Tcl_DictObjPut(NULL, res, f->name != NULL ? f->name
			: Tcl_NewWideIntObj(i), DictUnfold(elv[i], f->type));
	    }
	}
	break;
    case TCLT_LIST:
	res = Tcl_NewListObj(0, NULL);
	if (Tcl_ListObjGetElements(NULL, obj, &n, &elv) == TCL_OK) {
	    for (i = 0; i < n; i++) {
		Tcl_ListObjAppendElement(NULL, res, DictUnfold(elv[i],
			tp->elem));
	    }
	}
	break;
    case TCLT_DICT: {
	Tcl_DictSearch search;
	Tcl_Obj *k, *v;
	int done;

	res = Tcl_NewDictObj();
	if (Tcl_DictObjFirst(NULL, obj, &search, &k, &v, &done) == TCL_OK) {
	    for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
		Tcl_DictObjPut(NULL, res, DeepContent(k),
			DictUnfold(v, tp->elem));
	    }
	    Tcl_DictObjDone(&search);
	}
	break;
    }
    default: {				/* TCLT_SUM */
	const TclTypeField *f;

	if (TclSumTagged(tp)) {		/* v16w : "v charge" */
	    Tcl_Obj *pl = NULL;

	    variant = TclSumDecode(tp, obj, &pl);
	    if (pl != NULL) {
		obj = pl;
	    }
	}
	if (variant < 0) {
	    variant = TclTypeSumCurrent(tp, obj);
	}
	if (variant < 0 || variant >= tp->nfields) {
	    res = DeepContent(obj);
	    break;
	}
	f = &tp->fields[variant];
	if (f->type == NULL) {
	    res = obj;			/* une variante littérale : elle-même */
	} else if (f->name != NULL) {
	    res = Tcl_NewDictObj();
	    Tcl_DictObjPut(NULL, res, f->name, DictUnfold(obj, f->type));
	} else {
	    res = DictUnfold(obj, f->type);
	}
	break;
    }
    }
    return res;
}

static Tcl_Obj *
Unfold(Tcl_Obj *value, int mode)
{
    return (mode == 2) ? DictUnfold(value, NULL) : DeepContent(value);
}

/* type {{*}T}mot, type {{**}T}mot : le cast explicite, puis le dépliage,
 * rangé dans le résultat de l'interpréteur. TCL_CONTINUE : pas de préfixe
 * (le cas ordinaire). */
static int
CastWordUnfold(Tcl_Interp *interp, Tcl_Obj *word)
{
    Tcl_Obj *top = NULL, *popped, *text, *pushed, *res = NULL;
    int mode, rc;

    if (!TclIsAnnotated(word)) {
	return TCL_CONTINUE;
    }
    popped = TclAnnotPopTop(word, &top);
    if (popped == NULL || top == NULL) {
	if (popped != NULL) {
	    TclDiscard(popped);
	}
	return TCL_CONTINUE;
    }
    text = Tcl_DuplicateObj(top);
    Tcl_IncrRefCount(text);
    mode = UnfoldPrefix(&text);
    if (mode == 0) {
	Tcl_DecrRefCount(text);
	TclDiscard(popped);
	return TCL_CONTINUE;
    }
    Tcl_IncrRefCount(popped);
    pushed = TclAnnotPush(popped, text);
    Tcl_IncrRefCount(pushed);
    rc = TclAnnotCastWord(interp, pushed, &res);
    if (rc == TCL_OK) {
	Tcl_IncrRefCount(res);
	Tcl_SetObjResult(interp, Unfold(res, mode));
	Tcl_DecrRefCount(res);
    } else if (rc == TCL_CONTINUE) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("type {%s}: nothing to cast",
		Tcl_GetString(top)));
	rc = TCL_ERROR;
    }
    Tcl_DecrRefCount(pushed);
    Tcl_DecrRefCount(popped);
    Tcl_DecrRefCount(text);
    return rc;
}

/*
 * À la définition (proc NOM ARGS CORPS) : *sigPtr reçoit la signature, ou
 * NULL pour une procédure classique (ni liste typée, ni type de retour).
 * *typedArgsPtr : la liste d'arguments est un produit (la procédure est
 * alors créée avec la liste « args », la signature lie les champs).
 */
static int TypedProcSig(Tcl_Interp *interp, Tcl_Obj *nameObj,
	Tcl_Obj *argsObj, void **sigPtr, int *typedArgsPtr);

/*
 * v16u : les types d'une signature se lisent dans l'espace de noms de la
 * procédure (proc {point}::geom::mk {...} : ::geom::point), comme son corps.
 */
int
TclTypedProcSig(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_Obj *argsObj,
	void **sigPtr, int *typedArgsPtr)
{
    return TclTypedProcSigNs(interp, NULL, nameObj, argsObj, sigPtr,
	    typedArgsPtr);
}

/* v17k : l'espace des types donné par l'appelant (une méthode TclOO : celui
 * de sa classe), ou, si NULL, celui du nom (proc, apply). */
int
TclTypedProcSigNs(Tcl_Interp *interp, Tcl_Namespace *typeNsPtr,
	Tcl_Obj *nameObj, Tcl_Obj *argsObj, void **sigPtr, int *typedArgsPtr)
{
    TypeData *d = GetData(interp);
    Namespace *nsPtr = (Namespace *) typeNsPtr, *altNsPtr, *cxtNsPtr, *save;
    const char *simple;
    int rc;

    if (d == NULL) {
	return TypedProcSig(interp, nameObj, argsObj, sigPtr, typedArgsPtr);
    }
    if (nsPtr == NULL) {
	TclGetNamespaceForQualName(interp, TclGetString(nameObj), NULL, 0,
		&nsPtr, &altNsPtr, &cxtNsPtr, &simple);
    }
    save = d->ctxNs;
    if (nsPtr != NULL) {
	d->ctxNs = nsPtr;
    }
    rc = TypedProcSig(interp, nameObj, argsObj, sigPtr, typedArgsPtr);
    d->ctxNs = save;
    return rc;
}

/* Le type de retour porté par l'annotation d'un nom (proc {T}nom) ou d'une
 * lambda (apply {T}{args corps}) : *retPtr (compté) ou NULL. */
static int
RetFromName(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_Obj **retPtr,
	int *unfoldPtr)
{
    Tcl_Obj *ret, *top = NULL, *popped;
    TclType *tp = NULL;

    *retPtr = NULL;
    *unfoldPtr = 0;
    if (!TclIsAnnotated(nameObj)) {
	return TCL_OK;
    }
    popped = TclAnnotPopTop(nameObj, &top);
    if (popped != NULL) {
	ret = Tcl_DuplicateObj(top);
	TclDiscard(popped);
    } else if (TclAnnotInfo(nameObj, NULL, &tp, NULL, NULL) == 1
	    && tp != NULL) {
	ret = Tcl_NewStringObj(tp->name, -1);	/* déjà résolue */
    } else {
	return TCL_OK;
    }
    Tcl_IncrRefCount(ret);
    *unfoldPtr = UnfoldPrefix(&ret);
    if (Tcl_GetCharLength(ret) == 0 || Tcl_GetString(ret)[0] == '@') {
	Tcl_DecrRefCount(ret);		/* proc {}f, une note @ : rien */
	*unfoldPtr = 0;
	return TCL_OK;
    }
    if (RetTypeCheck(interp, ret) != TCL_OK) {
	Tcl_Obj *msg = Tcl_ObjPrintf("bad return type {%s}: ",
		Tcl_GetString(ret));

	Tcl_AppendObjToObj(msg, Tcl_GetObjResult(interp));
	Tcl_SetObjResult(interp, msg);
	Tcl_DecrRefCount(ret);
	return TCL_ERROR;
    }
    *retPtr = ret;
    return TCL_OK;
}

/*
 * v16z : apply {T}{args corps ?ns?} -- une lambda est une procédure sans nom,
 * son type de retour est l'annotation du mot lambda.
 *
 * TclTypedLambdaFor : l'objet lambda à convertir. Le littéral {T}{...} est
 * refait à chaque exécution (::tcl::annotraw copie le contenu) : sans cache,
 * la Proc serait reconstruite à chaque appel. La table d'interpréteur garde
 * une lambda par texte "{T}contenu" ; au-delà de LAMBDAS_MAX entrées, elle
 * est vidée. Une valeur qui porte aussi des notes n'est pas mise en cache.
 * Rend un objet non possédé.
 *
 * TclTypedLambdaRet : pose le type de retour sur la signature d'une Proc
 * neuve (créée au besoin).
 */
#define LAMBDAS_MAX 512

Tcl_Obj *
TclTypedLambdaFor(Tcl_Interp *interp, Tcl_Obj *lambdaObj)
{
    TypeData *d = GetData(interp);
    Tcl_Obj *content = TCL_ANNOT_CONTENT(lambdaObj), *raw;
    Tcl_HashEntry *hPtr;
    Tcl_DString key;
    Tcl_Size len;
    const char *t;
    int isNew;

    raw = TclAnnotRawText(lambdaObj);
    if (d == NULL || raw == NULL) {
	return content;
    }
    Tcl_DStringInit(&key);
    Tcl_DStringAppend(&key, "{", 1);
    t = Tcl_GetStringFromObj(raw, &len);
    Tcl_DStringAppend(&key, t, len);
    Tcl_DStringAppend(&key, "}", 1);
    t = Tcl_GetStringFromObj(content, &len);
    Tcl_DStringAppend(&key, t, len);
    hPtr = Tcl_FindHashEntry(&d->lambdas, Tcl_DStringValue(&key));
    if (hPtr == NULL) {
	if (d->lambdas.numEntries >= LAMBDAS_MAX) {
	    LambdasFlush(d);
	}
	hPtr = Tcl_CreateHashEntry(&d->lambdas, Tcl_DStringValue(&key),
		&isNew);
	Tcl_IncrRefCount(content);
	Tcl_SetHashValue(hPtr, content);
    }
    Tcl_DStringFree(&key);
    return (Tcl_Obj *) Tcl_GetHashValue(hPtr);
}

int
TclTypedLambdaRet(Tcl_Interp *interp, Tcl_Obj *lambdaObj,
	Tcl_Namespace *nsPtr, void **sigPtr)
{
    TclTypedSig *sig = (TclTypedSig *) *sigPtr;
    TypeData *d = GetData(interp);
    Namespace *save = NULL;
    Tcl_Obj *ret = NULL;
    int unfold = 0, rc;

    if (d != NULL) {
	save = d->ctxNs;	/* les types de l'espace de noms de la lambda */
	d->ctxNs = (Namespace *) nsPtr;
    }
    rc = RetFromName(interp, lambdaObj, &ret, &unfold);
    if (d != NULL) {
	d->ctxNs = save;
    }
    if (rc != TCL_OK) {
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "PROCARGS", (char *) NULL);
	return TCL_ERROR;
    }
    if (ret == NULL) {
	return TCL_OK;
    }
    if (sig == NULL) {
	sig = (TclTypedSig *) Tcl_Alloc(sizeof(TclTypedSig));
	memset(sig, 0, sizeof(TclTypedSig));
	sig->refCount = 1;
	sig->n = -1;
	sig->vpos = -1;
	*sigPtr = sig;
    }
    DecrIf(sig->retText);
    sig->retText = ret;
    sig->unfold = unfold;
    sig->retTp = NULL;
    return TCL_OK;
}

static int
TypedProcSig(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_Obj *argsObj,
	void **sigPtr, int *typedArgsPtr)
{
    Tcl_Obj *ret = NULL, *uopt = NULL, *upos = NULL;
    Tcl_Size len, n, i, cap = 0;
    int unfold = 0;
    const char *text = Tcl_GetStringFromObj(argsObj, &len);
    TclType *prod = NULL;
    TclTypedSig *sig;

    *sigPtr = NULL;
    *typedArgsPtr = 0;
    if (RetFromName(interp, nameObj, &ret, &unfold) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_ListObjLength(NULL, argsObj, &n) == TCL_OK) {
	/* une liste valide reste classique ({a b}, {a {b 2}}) ; avec des
	 * « ; », elle est typée si elle se lit comme un produit */
	if (memchr(text, ';', len) != NULL) {
	    Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);

	    prod = TclTypeParse(interp, text, len);
	    if (prod == NULL || prod->kind != TCLT_PRODUCT) {
		prod = NULL;
	    }
	    Tcl_RestoreInterpState(interp, st);
	}
    } else if (LooksTyped(text, len)) {
	prod = TclTypeParse(interp, text, len);
	if (prod == NULL || prod->kind != TCLT_PRODUCT) {
	    if (prod != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("a typed argument list "
			"is a product: {{T}a; {T}b ?= v}, not %s", prod->name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "PROCARGS",
			(char *) NULL);
	    }
	    if (ret != NULL) {
		Tcl_DecrRefCount(ret);
	    }
	    return TCL_ERROR;
	}
    }
    if (prod == NULL && ret == NULL) {
	return TCL_OK;			/* procédure classique */
    }
    sig = (TclTypedSig *) Tcl_Alloc(sizeof(TclTypedSig));
    memset(sig, 0, sizeof(TclTypedSig));
    sig->refCount = 1;
    sig->n = -1;
    sig->vpos = -1;
    sig->retText = ret;
    sig->unfold = (ret != NULL) ? unfold : 0;
    if (prod != NULL) {
	uopt = Tcl_NewObj();
	upos = Tcl_NewObj();
	Tcl_IncrRefCount(uopt);
	Tcl_IncrRefCount(upos);
	sig->n = prod->nfields;
	sig->a = (TypedArg *) Tcl_Alloc(sizeof(TypedArg) * (prod->nfields + 1));
	memset(sig->a, 0, sizeof(TypedArg) * (prod->nfields + 1));
	sig->pos = (Tcl_Size *) Tcl_Alloc(sizeof(Tcl_Size)
		* (prod->nfields + 1));
	for (i = 0; i < prod->nfields; i++) {
	    TclTypeField *f = &prod->fields[i];
	    TypedArg *a = &sig->a[i];
	    const char *nm;
	    const TclType *st;
	    Tcl_Obj *u;

	    if (f->name == NULL || f->type == NULL) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj("every field of a "
			"typed argument list needs a type and a name", -1));
		goto sigError;
	    }
	    nm = Tcl_GetString(f->name);
	    a->name = f->name;
	    Tcl_IncrRefCount(a->name);
	    a->local = OptLocalName(f->name);
	    Tcl_IncrRefCount(a->local);
	    a->typeText = TypeTextObj(f->type, 2);
	    if (f->defval) {
		a->defval = f->defval;
		Tcl_IncrRefCount(a->defval);
	    }
	    if (f->defcmd) {
		a->defcmd = f->defcmd;
		Tcl_IncrRefCount(a->defcmd);
	    }
	    a->tp = f->type;
	    a->lidx = -1;
	    if (f->type->kind == TCLT_VAR) {
		/* {var.T}n, {var.T}-o : un lien vers la variable de l'appelant
		 * (v16y : aussi pour une option) */
		a->isVar = 1;
		if (f->type->elem != NULL
			&& !TclTypeIsPlainString(f->type->elem)) {
		    a->varElem = TypeTextObj(f->type->elem, 2);
		}
	    }
	    if (nm[0] == '-') {
		/* v16q : une option */
		if (Tcl_GetCharLength(uopt) > 0) {
		    Tcl_AppendToObj(uopt, " ", 1);
		}
		if (IsFlagType(f->type)) {
		    a->optKind = OPT_SCALAR;
		    a->isFlag = 1;
		    if (AddOpt(interp, sig, &cap, f->name, i, -1, -1, -1, 0, 0,
			    1) != TCL_OK) {
			goto sigError;
		    }
		    Tcl_AppendStringsToObj(uopt, "?", nm, "?", (char *) NULL);
		} else if ((st = UnderKind(f->type, TCLT_SUM)) != NULL) {
		    a->optKind = OPT_EXCL;
		    a->sub = (TclType *) st;
		    if (AddOpt(interp, sig, &cap, f->name, i, -1, -1, -1, 1, 1,
			    0) != TCL_OK
			    || AddSumOpts(interp, sig, &cap, st, i, -1)
			    != TCL_OK) {
			goto sigError;
		    }
		    if (a->defcmd != NULL || (a->defval != NULL
			    && SumVariantByName(st, Tcl_GetString(a->defval))
			    < 0)) {
			Tcl_SetObjResult(interp, Tcl_ObjPrintf("the default of "
				"option \"%s\" must name one of its variants",
				nm));
			goto sigError;
		    }
		    AppendSumUsage(uopt, st);
		} else if ((st = UnderKind(f->type, TCLT_PRODUCT)) != NULL) {
		    Tcl_Size g;

		    a->optKind = OPT_GROUP;
		    a->sub = (TclType *) st;
		    for (g = 0; g < st->nfields; g++) {
			const TclTypeField *gf = &st->fields[g];
			const TclType *gs;

			if (gf->name == NULL || gf->type == NULL) {
			    continue;
			}
			if (g > 0) {
			    Tcl_AppendToObj(uopt, " ", 1);
			}
			if (IsFlagType(gf->type)) {
			    if (AddOpt(interp, sig, &cap, gf->name, i, -1, g, -1,
				    0, 0, 1) != TCL_OK) {
				goto sigError;
			    }
			    Tcl_AppendStringsToObj(uopt, "?-",
				    Tcl_GetString(gf->name), "?", (char *) NULL);
			} else if ((gs = UnderKind(gf->type, TCLT_SUM)) != NULL) {
			    if (AddOpt(interp, sig, &cap, gf->name, i, -1, g, -1,
				    1, 1, 0) != TCL_OK
				    || AddSumOpts(interp, sig, &cap, gs, i, g)
				    != TCL_OK) {
				goto sigError;
			    }
			    AppendSumUsage(uopt, gs);
			} else {
			    if (AddOpt(interp, sig, &cap, gf->name, i, -1, g, -1,
				    1, 0, 0) != TCL_OK) {
				goto sigError;
			    }
			    Tcl_AppendStringsToObj(uopt, "?-",
				    Tcl_GetString(gf->name), " value?",
				    (char *) NULL);
			}
		    }
		} else {
		    a->optKind = OPT_SCALAR;
		    if (AddOpt(interp, sig, &cap, f->name, i, -1, -1, -1, 1, 0,
			    0) != TCL_OK) {
			goto sigError;
		    }
		    Tcl_AppendStringsToObj(uopt, "?", nm,
			    a->isVar ? " varName?" : " value?", (char *) NULL);
		}
		continue;
	    }
	    if (!a->defval && !a->defcmd) {
		const TclType *pt = UnderKind(f->type, TCLT_PRODUCT);
		Tcl_Size j;

		if (pt != NULL && pt->nfields > 0) {
		    a->allDefault = 1;
		    for (j = 0; j < pt->nfields; j++) {
			if (!pt->fields[j].defval && !pt->fields[j].defcmd) {
			    a->allDefault = 0;
			}
		    }
		}
	    }
	    u = upos;
	    if (Tcl_GetCharLength(u) > 0) {
		Tcl_AppendToObj(u, " ", 1);
	    }
	    if (strlen(nm) > 3 && strcmp(nm + strlen(nm) - 3, "...") == 0) {
		/* v16y : {T}nom... : variadique, n'importe où ; sans défaut,
		 * au moins une fois ; un produit s'étale sur ses champs */
		const TclType *pt = UnderKind(f->type, TCLT_PRODUCT);
		Tcl_Obj *words, *et;
		const char *ett;
		Tcl_Size k;

		if (sig->vpos >= 0) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj("a typed "
			    "argument list has at most one variadic field", -1));
		    goto sigError;
		}
		if (a->isVar) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf("variadic field "
			    "\"%s\" cannot be a var.T", nm));
		    goto sigError;
		}
		a->group = (pt != NULL && pt->nfields > 0) ? pt->nfields : 1;
		a->vmin = !(a->defval || a->defcmd);
		a->allDefault = 0;
		/* la locale est une liste de T */
		et = a->typeText;
		ett = Tcl_GetString(et);
		a->typeText = (strpbrk(ett, " ;,|") != NULL)
			? Tcl_ObjPrintf("list.{%s}", ett)
			: Tcl_ObjPrintf("list.%s", ett);
		Tcl_IncrRefCount(a->typeText);
		Tcl_DecrRefCount(et);
		words = Tcl_NewObj();
		for (k = 0; k < a->group; k++) {
		    Tcl_AppendStringsToObj(words, k ? " " : "",
			    Tcl_GetString(pt != NULL && pt->nfields > 0
				? pt->fields[k].name : a->local), (char *) NULL);
		}
		Tcl_IncrRefCount(words);
		if (a->vmin) {
		    Tcl_AppendPrintfToObj(u, "%s ?%s ...?", Tcl_GetString(words),
			    Tcl_GetString(words));
		} else {
		    Tcl_AppendPrintfToObj(u, "?%s ...?", Tcl_GetString(words));
		}
		Tcl_DecrRefCount(words);
		sig->vpos = sig->npos;
		sig->pos[sig->npos++] = i;
		continue;
	    }
	    if (sig->vpos >= 0 && (a->defval || a->defcmd || a->allDefault)) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("argument \"%s\" after "
			"a variadic field cannot have a default", nm));
		goto sigError;
	    }
	    if (i == prod->nfields - 1 && strcmp(nm, "args") == 0
		    && sig->vpos < 0
		    && f->type->kind == TCLT_LIST && f->type->arity < 0
		    && !a->defval && !a->defcmd) {
		sig->rest = 1;
		Tcl_AppendToObj(u, "?arg ...?", -1);
		continue;
	    }
	    sig->pos[sig->npos++] = i;
	    if (a->defval || a->defcmd || a->allDefault) {
		Tcl_AppendStringsToObj(u, "?", nm, "?", (char *) NULL);
	    } else {
		Tcl_AppendToObj(u, nm, -1);
	    }
	}
	if (Tcl_GetCharLength(uopt) > 0) {
	    Tcl_AppendToObj(uopt, " ?--?", -1);	/* la fin des options */
	}
	if (Tcl_GetCharLength(uopt) > 0 && Tcl_GetCharLength(upos) > 0) {
	    Tcl_AppendToObj(uopt, " ", 1);
	}
	Tcl_AppendObjToObj(uopt, upos);
	sig->usage = uopt;
	Tcl_DecrRefCount(upos);
	if (sig->nopts > 0) {
	    Tcl_Obj **names = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *)
		    * sig->nopts);

	    for (i = 0; i < sig->nopts; i++) {
		names[i] = sig->opts[i].opt;
	    }
	    qsort(names, sig->nopts, sizeof(Tcl_Obj *), CompareObjStr);
	    sig->optList = Tcl_NewObj();
	    Tcl_IncrRefCount(sig->optList);
	    for (i = 0; i < sig->nopts; i++) {
		Tcl_AppendStringsToObj(sig->optList, i == 0 ? "" : ", ",
			Tcl_GetString(names[i]), (char *) NULL);
	    }
	    /* -- termine les options : rappelé comme dans regexp */
	    Tcl_AppendToObj(sig->optList, " or --", -1);
	    Tcl_Free(names);
	}
	sig->argsText = TypeTextObj(prod, 1);
	*typedArgsPtr = 1;
    }
    *sigPtr = sig;
    return TCL_OK;

  sigError:
    Tcl_SetErrorCode(interp, "TCL", "TYPE", "PROCARGS", (char *) NULL);
    DecrIf(uopt);
    DecrIf(upos);
    TclTypedSigRelease(sig);
    return TCL_ERROR;
}

static int
TypedArgsWrong(Tcl_Interp *interp, TclTypedSig *sig, CallFrame *framePtr,
	Tcl_Size skip)
{
    Tcl_Obj *msg = Tcl_NewStringObj("wrong # args: should be \"", -1);
    Tcl_Size i;

    for (i = 0; i < skip; i++) {
	Tcl_AppendObjToObj(msg, framePtr->objv[i]);
	Tcl_AppendToObj(msg, " ", 1);
    }
    Tcl_AppendObjToObj(msg, sig->usage);
    Tcl_AppendToObj(msg, "\"", 1);
    Tcl_SetObjResult(interp, msg);
    Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
    return TCL_ERROR;
}

static int
TypedArgError(Tcl_Interp *interp, TypedArg *a)
{
    Tcl_Obj *msg = Tcl_ObjPrintf("%s \"%s\": ", a->optKind ? "option"
	    : "argument", Tcl_GetString(a->name));

    Tcl_AppendObjToObj(msg, Tcl_GetObjResult(interp));
    Tcl_SetObjResult(interp, msg);
    if (a->tp != NULL && !a->tp->nominal && a->tp->variantOf == NULL
	    && (a->tp->kind == TCLT_PRODUCT || a->tp->kind == TCLT_SUM)) {
	/* un produit écrit dans la signature : son texte, pas prod<…> (v18b :
	 * pas un type de variante, qui a un nom lisible) */
	Tcl_Obj *txt = Tcl_ObjPrintf("{%s}", Tcl_GetString(a->typeText));

	Tcl_IncrRefCount(txt);
	TclTypeResultRename(interp, a->tp->name, Tcl_GetString(txt));
	Tcl_DecrRefCount(txt);
    }
    return TCL_ERROR;
}

/* Le champ (un champ positionnel ou le reste) attend-il une somme (dont une
 * étiquette {/v} nomme la variante) ? -1 : aucun champ. */
static int
LabelForSum(Tcl_Interp *interp, TclTypedSig *sig, Tcl_Size field)
{
    TclType *tp;

    if (field < 0) {
	return 0;
    }
    tp = TclAnnotTypeOf(interp, sig->a[field].typeText);
    if (tp != NULL && field == sig->n - 1 && sig->rest) {
	tp = tp->elem;
    }
    return UnderKind(tp, TCLT_SUM) != NULL;
}

/* Déclare la locale nameObj, de type typeText, valeur v. */
static int
DeclareLocalTyped(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_Obj *typeText,
	Tcl_Obj *v)
{
    Tcl_Obj *typed = NULL;

    if (DeclareTyped(interp, typeText, v, 0, &typed) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_IncrRefCount(typed);
    if (TclTypedVarDeclare(interp, nameObj, typed) == NULL) {
	Tcl_DecrRefCount(typed);
	return TCL_ERROR;
    }
    Tcl_DecrRefCount(typed);
    return TCL_OK;
}

/* v16s : la locale compilée d'un champ (AddTypedLocals, tclProc.c), par
 * son indice, vérifié par son nom ; NULL si le champ n'en a pas. */
static Var *
FieldVar(Tcl_Interp *interp, TypedArg *a)
{
    CallFrame *framePtr = ((Interp *) interp)->varFramePtr;
    Tcl_Size i, n = framePtr->numCompiledLocals, ll;
    const char *ln = Tcl_GetStringFromObj(a->local, &ll);

    if (framePtr->localCachePtr == NULL) {
	return NULL;
    }
    if (a->lidx >= 0 && a->lidx < n
	    && localName(framePtr, a->lidx) == a->lname) {
	return &framePtr->compiledLocals[a->lidx];
    }
    for (i = 0; i < n; i++) {
	Tcl_Obj *nm = localName(framePtr, i);
	Tcl_Size l;

	if (nm != NULL && strcmp(Tcl_GetStringFromObj(nm, &l), ln) == 0
		&& l == ll) {
	    a->lidx = i;
	    Tcl_IncrRefCount(nm);	/* tenu : son adresse reste la sienne */
	    DecrIf(a->lname);
	    a->lname = nm;
	    return &framePtr->compiledLocals[i];
	}
    }
    return NULL;
}

/* v16s : déclare la locale du champ a (type résolu une fois par génération
 * des types, locale compilée trouvée par son indice). */
static int
DeclareField(Tcl_Interp *interp, TypedArg *a, Tcl_Obj *v)
{
    Tcl_Obj *typed = NULL, *res;
    size_t uid, gen = 0;		/* (initialisé : -Wmaybe-uninitialized sous MinGW) */
    Var *varPtr;

    (void) TclTypesGeneration(interp, &uid, &gen);
    if (a->ctp == NULL || a->ctpGen != gen) {
	a->ctp = TclAnnotTypeOf(interp, a->typeText);
	a->ctpGen = gen;
    }
    if (DeclareTypedTp(interp, a->ctp, a->typeText, v, 0, &typed)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_IncrRefCount(typed);
    varPtr = FieldVar(interp, a);
    res = (varPtr != NULL)
	    ? TclTypedVarDeclareVar(interp, varPtr, NULL, a->local, typed)
	    : TclTypedVarDeclare(interp, a->local, typed);
    Tcl_DecrRefCount(typed);
    return (res != NULL) ? TCL_OK : TCL_ERROR;
}

/* La valeur absente d'une option ou d'un champ : son défaut (ou 0 pour un
 * drapeau), NULL si aucun (la locale reste indéfinie). Référence propre. */
static int
OptDefault(Tcl_Interp *interp, const TclTypeField *f, int isFlag,
	Tcl_Obj **vPtr)
{
    *vPtr = NULL;
    if (f->defval != NULL) {
	*vPtr = f->defval;
    } else if (f->defcmd != NULL) {
	if (Tcl_EvalObjEx(interp, f->defcmd, 0) != TCL_OK) {
	    return TCL_ERROR;
	}
	*vPtr = Tcl_GetObjResult(interp);
    } else if (isFlag) {
	*vPtr = Tcl_NewWideIntObj(0);
    }
    if (*vPtr != NULL) {
	Tcl_IncrRefCount(*vPtr);
    }
    return TCL_OK;
}

/* Cherche un mot d'option : le nom exact, ou un préfixe unique. -1 : aucun
 * (*ambigPtr : plusieurs). */
static Tcl_Size
FindOpt(TclTypedSig *sig, const char *w, Tcl_Size wl, int *ambigPtr)
{
    Tcl_Size i, found = -1, nf = 0;

    *ambigPtr = 0;
    for (i = 0; i < sig->nopts; i++) {
	Tcl_Size ol;
	const char *o = Tcl_GetStringFromObj(sig->opts[i].opt, &ol);

	if (ol == wl && memcmp(o, w, wl) == 0) {
	    return i;
	}
	if (ol > wl && memcmp(o, w, wl) == 0) {
	    found = i;
	    nf++;
	}
    }
    if (nf > 1) {
	*ambigPtr = 1;
	return -1;
    }
    return found;
}

static int
OptErr(Tcl_Interp *interp, const char *code, Tcl_Obj *msg)
{
    Tcl_SetObjResult(interp, msg);
    Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", code, (char *) NULL);
    return TCL_ERROR;
}

/*
 * Les options d'un champ : ov[e] est la valeur donnée à l'option e (NULL :
 * absente ; un objet « 1 » pour un drapeau ; le mot, ou la liste des mots
 * pour une variante produit).
 */
/*
 * {var.T}n (argument ou option) : n est liée à la variable de l'appelant
 * nommée par v ; sa valeur courante, si elle existe, doit être un T.
 */
static int
BindVarArg(Tcl_Interp *interp, TypedArg *a, Tcl_Obj *v)
{
    Tcl_Obj *cur, *typed = NULL;

    if (Tcl_UpVar2(interp, "1", Tcl_GetString(TCL_ANNOT_CONTENT(v)),
	    NULL, Tcl_GetString(a->local != NULL ? a->local : a->name), 0)
	    != TCL_OK) {
	return TypedArgError(interp, a);
    }
    if (a->varElem != NULL) {
	cur = Tcl_ObjGetVar2(interp, a->local != NULL ? a->local : a->name,
		NULL, 0);
	if (cur != NULL) {
	    Tcl_IncrRefCount(cur);
	    if (DeclareTyped(interp, a->varElem, cur, 0, &typed) != TCL_OK) {
		Tcl_DecrRefCount(cur);
		return TypedArgError(interp, a);
	    }
	    TclDiscard(typed);
	    Tcl_DecrRefCount(cur);
	}
    }
    return TCL_OK;
}

/*
 * v16y : une option absente dont le défaut est calculé ([cmd], $var) est
 * liée après les arguments positionnels : son défaut peut les lire.
 */
static int
OptDeferred(TclTypedSig *sig, Tcl_Size ai, Tcl_Obj **ov)
{
    TypedArg *a = &sig->a[ai];
    Tcl_Size e;

    if (a->optKind != OPT_SCALAR || a->isFlag || a->defcmd == NULL) {
	return 0;
    }
    for (e = 0; e < sig->nopts; e++) {
	if (sig->opts[e].arg == ai && ov[e] != NULL) {
	    return 0;
	}
    }
    return 1;
}

static int
BindOption(Tcl_Interp *interp, TclTypedSig *sig, Tcl_Size ai, Tcl_Obj **ov)
{
    TypedArg *a = &sig->a[ai];
    Tcl_Size e, chosen = -1, ce = -1;

    if (a->optKind == OPT_SCALAR) {
	Tcl_Obj *v = NULL;
	TclTypeField fake;
	int slot = 0;
	size_t uid, gen = 0;		/* (initialisé : -Wmaybe-uninitialized sous MinGW) */

	for (e = 0; e < sig->nopts; e++) {
	    if (sig->opts[e].arg == ai && ov[e] != NULL) {
		v = ov[e];
		Tcl_IncrRefCount(v);
	    }
	}
	if (a->isVar) {
	    /* v16y : {var.T}-o NOM : o est la variable NOM de l'appelant */
	    int rc;

	    if (v == NULL) {
		memset(&fake, 0, sizeof(fake));
		fake.defval = a->defval;
		fake.defcmd = a->defcmd;
		if (OptDefault(interp, &fake, 0, &v) != TCL_OK) {
		    return TypedArgError(interp, a);
		}
		if (v == NULL) {
		    return TCL_OK;	/* absente, sans défaut : indéfinie */
		}
	    }
	    rc = BindVarArg(interp, a, v);
	    Tcl_DecrRefCount(v);
	    return rc;
	}
	if ((v == NULL && a->defcmd == NULL) || a->isFlag) {
	    /* le défaut, ou un drapeau : une valeur typée, faite une fois */
	    if (v != NULL) {
		slot = 1;
		Tcl_DecrRefCount(v);
		v = NULL;
	    }
	    (void) TclTypesGeneration(interp, &uid, &gen);
	    if (a->cacheGen != gen) {
		DecrIf(a->cache[0]);
		DecrIf(a->cache[1]);
		a->cache[0] = a->cache[1] = NULL;
		a->cacheGen = gen;
	    }
	    if (a->cache[slot] == NULL) {
		Tcl_Obj *raw = NULL, *typed = NULL;

		if (slot == 1) {
		    raw = Tcl_NewWideIntObj(1);
		    Tcl_IncrRefCount(raw);
		} else {
		    memset(&fake, 0, sizeof(fake));
		    fake.defval = a->defval;
		    if (OptDefault(interp, &fake, a->isFlag, &raw) != TCL_OK) {
			return TypedArgError(interp, a);
		    }
		    if (raw == NULL) {
			return TCL_OK;	/* absente, sans défaut : indéfinie */
		    }
		}
		if (DeclareTyped(interp, a->typeText, raw, 0, &typed)
			!= TCL_OK) {
		    Tcl_DecrRefCount(raw);
		    return TypedArgError(interp, a);
		}
		Tcl_IncrRefCount(typed);
		Tcl_DecrRefCount(raw);
		a->cache[slot] = typed;
	    }
	    {
		Var *fv = FieldVar(interp, a);

		if ((fv != NULL ? TclTypedVarDeclareVar(interp, fv, NULL,
			a->local, a->cache[slot])
			: TclTypedVarDeclare(interp, a->local, a->cache[slot]))
			== NULL) {
		    return TypedArgError(interp, a);
		}
	    }
	    return TCL_OK;
	}
	if (v == NULL) {
	    memset(&fake, 0, sizeof(fake));
	    fake.defval = a->defval;
	    fake.defcmd = a->defcmd;
	    if (OptDefault(interp, &fake, a->isFlag, &v) != TCL_OK) {
		return TypedArgError(interp, a);
	    }
	    if (v == NULL) {
		return TCL_OK;		/* absente, sans défaut : indéfinie */
	    }
	}
	if (DeclareField(interp, a, v) != TCL_OK) {
	    Tcl_DecrRefCount(v);
	    return TypedArgError(interp, a);
	}
	Tcl_DecrRefCount(v);
	return TCL_OK;
    }

    if (a->optKind == OPT_EXCL) {
	const TclType *sum = a->sub;
	const TclTypeField *vf;
	const TclType *vp;
	Tcl_Obj *vn;

	for (e = 0; e < sig->nopts; e++) {
	    OptEntry *oe = &sig->opts[e];
	    Tcl_Size v;

	    if (oe->arg != ai || oe->dep >= 0 || ov[e] == NULL) {
		continue;
	    }
	    if (oe->byName) {
		/* -mode ascii : le nom d'une variante sans valeur */
		v = SumVariantByName(sum, Tcl_GetString(ov[e]));
		if (v < 0 || (sum->fields[v].type != NULL
			&& !IsFlagType(sum->fields[v].type))) {
		    Tcl_Obj *msg = Tcl_ObjPrintf("bad value \"%s\" for option "
			    "\"%s\": must be ", Tcl_GetString(ov[e]),
			    Tcl_GetString(a->name));
		    Tcl_Size k, m = 0, tot = 0;

		    for (k = 0; k < sum->nfields; k++) {
			if (sum->fields[k].type == NULL
				|| IsFlagType(sum->fields[k].type)) {
			    tot++;
			}
		    }
		    for (k = 0; k < sum->nfields; k++) {
			if (sum->fields[k].type == NULL
				|| IsFlagType(sum->fields[k].type)) {
			    m++;
			    Tcl_AppendStringsToObj(msg, m == 1 ? "" : m == tot
				    ? " or " : ", ",
				    Tcl_GetString(VariantName(&sum->fields[k])),
				    (char *) NULL);
			}
		    }
		    return OptErr(interp, "VALUE", msg);
		}
	    } else {
		v = oe->variant;
	    }
	    if (chosen >= 0 && chosen != v) {
		return OptErr(interp, "EXCLUSIVE", Tcl_ObjPrintf(
			"options -%s and -%s are exclusive",
			Tcl_GetString(VariantName(&sum->fields[chosen])),
			Tcl_GetString(VariantName(&sum->fields[v]))));
	    }
	    chosen = v;
	    ce = oe->byName ? -1 : e;
	}
	for (e = 0; e < sig->nopts; e++) {
	    OptEntry *oe = &sig->opts[e];

	    if (oe->arg == ai && oe->dep >= 0 && ov[e] != NULL
		    && oe->variant != chosen) {
		return OptErr(interp, "DEPENDENT", Tcl_ObjPrintf(
			"option \"%s\" requires -%s", Tcl_GetString(oe->opt),
			Tcl_GetString(VariantName(&sum->fields[oe->variant]))));
	    }
	}
	if (chosen < 0) {
	    if (a->defval == NULL) {
		return TCL_OK;		/* aucune : indéfinie */
	    }
	    chosen = SumVariantByName(sum, Tcl_GetString(a->defval));
	}
	vf = &sum->fields[chosen];
	vn = VariantName(vf);
	if (Tcl_ObjSetVar2(interp, a->local, NULL, vn, TCL_LEAVE_ERR_MSG)
		== NULL) {
	    return TypedArgError(interp, a);
	}
	if (vf->type == NULL || IsFlagType(vf->type)) {
	    return TCL_OK;
	}
	if ((vp = UnderKind(vf->type, TCLT_PRODUCT)) != NULL) {
	    /* -command cmd -depth 2 : les champs de la variante, en locales */
	    Tcl_Obj **words = NULL;
	    Tcl_Size nw = 0, k, d;

	    if (ce >= 0) {
		if (sig->opts[ce].nvals == 1) {
		    words = &ov[ce];
		    nw = 1;
		} else {
		    Tcl_ListObjGetElements(NULL, ov[ce], &nw, &words);
		}
	    }
	    for (d = 0, k = 0; d < vp->nfields; d++) {
		const TclTypeField *df = &vp->fields[d];
		Tcl_Obj *v = NULL, *tt, *ln;
		int fl = IsFlagType(df->type), rc;

		if (df->name == NULL) {
		    continue;
		}
		if (Tcl_GetString(df->name)[0] != '-') {
		    if (k < nw) {
			v = words[k];
			Tcl_IncrRefCount(v);
		    }
		    k++;
		} else {
		    for (e = 0; e < sig->nopts; e++) {
			if (sig->opts[e].arg == ai && sig->opts[e].dep == d
				&& sig->opts[e].variant == chosen
				&& ov[e] != NULL) {
			    v = ov[e];
			    Tcl_IncrRefCount(v);
			}
		    }
		}
		if (v == NULL) {
		    if (OptDefault(interp, df, fl, &v) != TCL_OK) {
			return TypedArgError(interp, a);
		    }
		    if (v == NULL) {
			if (Tcl_GetString(df->name)[0] != '-') {
			    return OptErr(interp, "VALUE", Tcl_ObjPrintf(
				    "option \"-%s\" needs a value for \"%s\"",
				    Tcl_GetString(vn),
				    Tcl_GetString(df->name)));
			}
			continue;
		    }
		}
		tt = TypeTextObj(df->type, 2);
		ln = OptLocalName(df->name);
		Tcl_IncrRefCount(ln);
		rc = DeclareLocalTyped(interp, ln, tt, v);
		Tcl_DecrRefCount(ln);
		Tcl_DecrRefCount(tt);
		Tcl_DecrRefCount(v);
		if (rc != TCL_OK) {
		    return TypedArgError(interp, a);
		}
	    }
	    return TCL_OK;
	}
	if (ce >= 0) {
	    /* -command cmd : la locale command */
	    Tcl_Obj *tt = TypeTextObj(vf->type, 2);
	    int rc = DeclareLocalTyped(interp, vn, tt, ov[ce]);

	    Tcl_DecrRefCount(tt);
	    if (rc != TCL_OK) {
		return TypedArgError(interp, a);
	    }
	}
	return TCL_OK;
    }

    /* OPT_GROUP : la valeur du produit, à partir des options données */
    {
	const TclType *gp = a->sub;
	Tcl_Obj **pairs, *content = NULL, *typed;
	Tcl_Size np = 0, g, k;
	int rc = TCL_ERROR;

	pairs = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * 2
		* (gp->nfields + 1));
	for (g = 0; g < gp->nfields; g++) {
	    const TclTypeField *gf = &gp->fields[g];
	    const TclType *gs = UnderKind(gf->type, TCLT_SUM);
	    Tcl_Obj *v = NULL;
	    Tcl_Size gchosen = -1;

	    if (gf->name == NULL) {
		continue;
	    }
	    for (e = 0; e < sig->nopts; e++) {
		OptEntry *oe = &sig->opts[e];
		Tcl_Size vv = -1;

		if (oe->arg != ai || oe->gfield != g || ov[e] == NULL) {
		    continue;
		}
		if (gs != NULL) {
		    vv = oe->byName ? SumVariantByName(gs,
			    Tcl_GetString(ov[e])) : oe->variant;
		    if (gchosen >= 0 && vv >= 0 && vv != gchosen) {
			OptErr(interp, "EXCLUSIVE", Tcl_ObjPrintf(
				"options -%s and -%s are exclusive",
				Tcl_GetString(VariantName(&gs->fields[gchosen])),
				Tcl_GetString(VariantName(&gs->fields[vv]))));
			if (v != NULL) {
			    Tcl_DecrRefCount(v);
			}
			goto groupDone;
		    }
		    if (vv >= 0) {
			gchosen = vv;
		    }
		}
		if (v != NULL) {
		    Tcl_DecrRefCount(v);
		}
		if (gs != NULL && !oe->byName) {
		    const TclTypeField *vf = &gs->fields[oe->variant];

		    if (vf->type == NULL) {
			v = vf->literal;	/* -ascii : le littéral */
		    } else {
			/* -command cmd : la valeur, sa variante nommée */
			Tcl_Obj *lab = Tcl_ObjPrintf("/%s",
				Tcl_GetString(VariantName(vf)));

			Tcl_IncrRefCount(lab);
			v = TclAnnotPush(ov[e], lab);
			Tcl_DecrRefCount(lab);
		    }
		} else {
		    v = ov[e];
		}
		Tcl_IncrRefCount(v);
	    }
	    if (v == NULL && IsFlagType(gf->type) && gf->defval == NULL
		    && gf->defcmd == NULL) {
		v = Tcl_NewWideIntObj(0);
		Tcl_IncrRefCount(v);
	    }
	    if (v != NULL) {
		pairs[np] = gf->name;
		Tcl_IncrRefCount(pairs[np]);
		pairs[np + 1] = v;
		np += 2;
	    }
	}
	if (TclBuildProductNamed(interp, (TclType *) gp, np, pairs, &content)
		!= TCL_OK) {
	    TypedArgError(interp, a);
	    goto groupDone;
	}
	Tcl_IncrRefCount(content);
	typed = TclNewAnnotated(interp, a->tp, -1, content);
	Tcl_DecrRefCount(content);
	Tcl_IncrRefCount(typed);
	if (TclTypedVarDeclare(interp, a->local, typed) == NULL) {
	    Tcl_DecrRefCount(typed);
	    TypedArgError(interp, a);
	    goto groupDone;
	}
	Tcl_DecrRefCount(typed);
	rc = TCL_OK;

      groupDone:
	for (k = 0; k < np; k++) {
	    Tcl_DecrRefCount(pairs[k]);
	}
	Tcl_Free(pairs);
	return rc;
    }
}

/*
 * À l'appel (InitArgsAndLocals, le cadre de la procédure poussé, ses locales
 * prêtes) : la liste d'arguments a été rangée dans « args » (la seule
 * locale formelle) ; les options, puis les champs, sont liés.
 */
int
TclTypedProcBind(Tcl_Interp *interp, void *sigv, Tcl_Size skip)
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *framePtr = iPtr->varFramePtr;
    TclTypedSig *sig = (TclTypedSig *) sigv;
    Tcl_Size objc = framePtr->objc - skip, i = 0, k = 0, j;
    Tcl_Size nf = sig->npos;
    Tcl_Obj *const *objv = framePtr->objv + skip;
    Tcl_Obj **vals, **ov = NULL, *restList = NULL;
    Tcl_Obj *valsBuf[8], *ovBuf[16];
    int seenLabel = 0, rc = TCL_ERROR;
    Tcl_Size restField = sig->rest ? sig->n - 1 : -1;

    if (sig->n < 0) {
	return TCL_OK;			/* seulement un type de retour */
    }
    /* la locale « args » qui a reçu les mots : défaite */
    if (framePtr->numCompiledLocals > 0) {
	Var *av = framePtr->compiledLocals;

	if (av->value.objPtr != NULL && !TclIsVarLink(av)) {
	    Tcl_DecrRefCount(av->value.objPtr);
	}
	av->value.objPtr = NULL;
	av->flags = 0;
    }
    vals = (nf + 1 <= 8) ? valsBuf
	    : (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * (nf + 1));
    memset(vals, 0, sizeof(Tcl_Obj *) * (nf + 1));
    if (sig->rest) {
	restList = Tcl_NewListObj(0, NULL);
	Tcl_IncrRefCount(restList);
    }

    /* v16q : les options, au début */
    if (sig->nopts > 0) {
	ov = (sig->nopts <= 16) ? ovBuf
		: (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * sig->nopts);
	memset(ov, 0, sizeof(Tcl_Obj *) * sig->nopts);
	while (i < objc) {
	    Tcl_Size wl, e;
	    const char *w = Tcl_GetStringFromObj(objv[i], &wl);
	    int ambig;
	    OptEntry *oe;
	    Tcl_Obj *v;

	    if (wl < 2 || w[0] != '-') {
		break;
	    }
	    if (wl == 2 && w[1] == '-') {
		i++;			/* -- : fin des options */
		break;
	    }
	    e = FindOpt(sig, w, wl, &ambig);
	    if (e < 0) {
		double d;

		if (!ambig && Tcl_GetDoubleFromObj(NULL, objv[i], &d)
			== TCL_OK) {
		    break;		/* -5 : un argument */
		}
		OptErr(interp, ambig ? "AMBIGUOUS" : "OPTION", Tcl_ObjPrintf(
			"%s option \"%s\": must be %s", ambig ? "ambiguous"
			: "bad", w, Tcl_GetString(sig->optList)));
		goto done;
	    }
	    oe = &sig->opts[e];
	    if (objc - i - 1 < oe->nvals) {
		OptErr(interp, "VALUE", oe->nvals == 1
			? Tcl_ObjPrintf("option \"%s\" needs a value",
			    Tcl_GetString(oe->opt))
			: Tcl_ObjPrintf("option \"%s\" needs %d values",
			    Tcl_GetString(oe->opt), oe->nvals));
		goto done;
	    }
	    if (oe->nvals == 0) {
		v = Tcl_NewWideIntObj(1);
	    } else if (oe->nvals == 1) {
		v = objv[i + 1];
	    } else {
		v = Tcl_NewListObj(oe->nvals, objv + i + 1);
	    }
	    Tcl_IncrRefCount(v);
	    if (ov[e] != NULL) {
		Tcl_DecrRefCount(ov[e]);	/* la dernière l'emporte */
	    }
	    ov[e] = v;
	    i += 1 + oe->nvals;
	}
    }

    if (sig->vpos >= 0) {
	/*
	 * v16y : un champ variadique {T}nom... . Les champs d'avant se
	 * remplissent de gauche à droite (un défaut ne prend un mot que s'il
	 * en reste assez), ceux d'après depuis la fin (TIP 288) ; le milieu
	 * va au champ variadique, par groupes de « group » mots.
	 */
	Tcl_Size n = objc - i, vp = sig->vpos, na = nf - vp - 1;
	Tcl_Size avail = n - na, reqLeft = 0, used = 0, vw, g;
	TypedArg *va = &sig->a[sig->pos[vp]];
	Tcl_Obj *lst;

	for (j = 0; j < vp; j++) {
	    TypedArg *b = &sig->a[sig->pos[j]];

	    if (!(b->defval || b->defcmd || b->allDefault)) {
		reqLeft++;
	    }
	}
	if (avail < reqLeft + va->vmin * va->group) {
	    TypedArgsWrong(interp, sig, framePtr, skip);
	    goto done;
	}
	for (j = 0; j < vp; j++) {
	    TypedArg *b = &sig->a[sig->pos[j]];

	    if (!(b->defval || b->defcmd || b->allDefault)) {
		reqLeft--;
	    } else if (avail - used - reqLeft - va->vmin * va->group < 1) {
		continue;			/* son défaut */
	    }
	    vals[j] = objv[i + used++];
	    Tcl_IncrRefCount(vals[j]);
	}
	vw = avail - used;
	if (vw % va->group != 0) {
	    TypedArgsWrong(interp, sig, framePtr, skip);
	    goto done;
	}
	for (j = 0; j < na; j++) {
	    vals[vp + 1 + j] = objv[i + avail + j];
	    Tcl_IncrRefCount(vals[vp + 1 + j]);
	}
	if (vw > 0 || va->vmin) {
	    lst = Tcl_NewListObj(0, NULL);
	    for (g = 0; g < vw; g += va->group) {
		Tcl_ListObjAppendElement(NULL, lst, va->group == 1
			? objv[i + used + g]
			: Tcl_NewListObj(va->group, objv + i + used + g));
	    }
	    vals[vp] = lst;
	    Tcl_IncrRefCount(lst);
	}				/* sinon : son défaut */
	i = objc;
    }

    for (; i < objc; i++) {
	Tcl_Obj *arg = objv[i], *lab = NULL, *val = arg;
	Tcl_Size fi = -1;

	if (TclIsAnnotated(arg)) {
	    lab = TclAnnotLabelOf(arg);
	}
	if (lab != NULL) {
	    Tcl_Size ll;
	    const char *lt = Tcl_GetStringFromObj(lab, &ll);

	    for (j = 0; j < nf; j++) {
		Tcl_Size nl;
		const char *nt = Tcl_GetStringFromObj(
			sig->a[sig->pos[j]].name, &nl);

		if (nl == ll - 1 && memcmp(nt, lt + 1, nl) == 0) {
		    fi = j;
		    break;
		}
	    }
	    if (fi >= 0) {
		Tcl_Obj *top = NULL, *popped = TclAnnotPopTop(arg, &top);

		if (popped != NULL && top != NULL
			&& strcmp(Tcl_GetString(top), lt) == 0) {
		    val = popped;
		} else if (popped != NULL) {
		    TclDiscard(popped);
		}
	    }
	    if (fi < 0 && !seenLabel && LabelForSum(interp, sig,
		    (k < nf) ? sig->pos[k] : restField) == 0) {
		/* ni un champ, ni la variante d'une somme */
		Tcl_Obj *msg = Tcl_ObjPrintf("unknown argument label {%s}: "
			"must be ", lt);

		for (j = 0; j < nf; j++) {
		    Tcl_AppendStringsToObj(msg, j == 0 ? "" : j == nf - 1
			    ? " or " : ", ",
			    Tcl_GetString(sig->a[sig->pos[j]].name),
			    ":", (char *) NULL);
		}
		Tcl_SetObjResult(interp, msg);
		Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
		Tcl_DecrRefCount(lab);
		goto done;
	    }
	    Tcl_DecrRefCount(lab);
	}
	if (fi >= 0) {
	    if (vals[fi] != NULL) {
		if (val != arg) {
		    TclDiscard(val);
		}
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"argument \"%s\" given twice",
			Tcl_GetString(sig->a[sig->pos[fi]].name)));
		Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
		goto done;
	    }
	    vals[fi] = val;
	    Tcl_IncrRefCount(val);
	    seenLabel = 1;
	} else if (seenLabel) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "positional argument \"%s\" after labelled arguments",
		    Tcl_GetString(arg)));
	    Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
	    goto done;
	} else if (k < nf) {
	    vals[k++] = arg;
	    Tcl_IncrRefCount(arg);
	} else if (restList != NULL) {
	    Tcl_ListObjAppendElement(NULL, restList, arg);
	} else {
	    TypedArgsWrong(interp, sig, framePtr, skip);
	    goto done;
	}
    }

    /*
     * Les options d'abord : un défaut [cmd] d'un argument peut les lire.
     * v16y : une option absente à défaut calculé vient après les arguments
     * (son défaut peut les lire, -fin ?= [llength $l]).
     */
    for (j = 0; j < sig->n; j++) {
	if (sig->a[j].optKind != OPT_NONE && !OptDeferred(sig, j, ov)
		&& BindOption(interp, sig, j, ov) != TCL_OK) {
	    goto done;
	}
    }
    for (j = 0; j < nf; j++) {
	TypedArg *a = &sig->a[sig->pos[j]];
	Tcl_Obj *v = vals[j];

	if (v == NULL) {
	    if (a->defval != NULL) {
		v = a->defval;
	    } else if (a->defcmd != NULL) {
		if (Tcl_EvalObjEx(interp, a->defcmd, 0) != TCL_OK) {
		    TypedArgError(interp, a);
		    goto done;
		}
		v = Tcl_GetObjResult(interp);
	    } else if (a->allDefault) {
		/* {{T}x ?= 0; {T}y ?= 0}dp absent : la valeur des défauts */
		Tcl_Obj *content = NULL;

		if (TclBuildProductNamed(interp,
			(TclType *) UnderKind(a->tp, TCLT_PRODUCT), 0, NULL,
			&content) != TCL_OK) {
		    TypedArgError(interp, a);
		    goto done;
		}
		Tcl_IncrRefCount(content);
		v = TclNewAnnotated(interp, a->tp, -1, content);
		Tcl_DecrRefCount(content);
	    } else if (!seenLabel) {
		TypedArgsWrong(interp, sig, framePtr, skip);
		goto done;
	    } else {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"missing argument \"%s\"", Tcl_GetString(a->name)));
		Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
		goto done;
	    }
	    vals[j] = v;
	    Tcl_IncrRefCount(v);
	}
	if (a->isVar) {
	    /* {var.T}n : n est la variable de l'appelant */
	    if (BindVarArg(interp, a, v) != TCL_OK) {
		goto done;
	    }
	    continue;
	}
	if (DeclareField(interp, a, v) != TCL_OK) {
	    TypedArgError(interp, a);
	    goto done;
	}
    }
    if (restList != NULL) {
	TypedArg *a = &sig->a[restField];

	if (DeclareField(interp, a, restList) != TCL_OK) {
	    TypedArgError(interp, a);
	    goto done;
	}
    }
    for (j = 0; j < sig->n; j++) {
	if (sig->a[j].optKind != OPT_NONE && OptDeferred(sig, j, ov)
		&& BindOption(interp, sig, j, ov) != TCL_OK) {
	    goto done;
	}
    }
    Tcl_ResetResult(interp);
    rc = TCL_OK;

  done:
    for (j = 0; j < nf; j++) {
	if (vals[j] != NULL) {
	    Tcl_DecrRefCount(vals[j]);
	}
    }
    if (ov != NULL) {
	for (j = 0; j < sig->nopts; j++) {
	    if (ov[j] != NULL) {
		Tcl_DecrRefCount(ov[j]);
	    }
	}
	if (ov != ovBuf) {
	    Tcl_Free(ov);
	}
    }
    if (vals != valsBuf) {
	Tcl_Free(vals);
    }
    if (restList != NULL) {
	Tcl_DecrRefCount(restList);
    }
    return rc;
}

/* Le résultat casté, rendu tel quel, ou déplié ({*}T, {**}T). */
static void
SetRet(Tcl_Interp *interp, TclTypedSig *sig, Tcl_Obj *out)
{
    if (sig->unfold == 0) {
	Tcl_SetObjResult(interp, out);
	return;
    }
    Tcl_IncrRefCount(out);
    Tcl_SetObjResult(interp, Unfold(out, sig->unfold));
    Tcl_DecrRefCount(out);
}

/* Un « return » réussi devient une erreur : ses options sont oubliées. */
static void
ResetReturn(Tcl_Interp *interp, int result)
{
    if (result == TCL_RETURN) {
	Tcl_Obj *opts = Tcl_NewObj();

	Tcl_IncrRefCount(opts);
	(void) Tcl_SetReturnOptions(interp, opts);
	Tcl_DecrRefCount(opts);
    }
}

/*
 * À la sortie du corps (TEBC, avant l'expiration des annotations du corps) :
 * le résultat d'un retour normal est casté au type de retour. Les notes du
 * résultat expirent d'abord ({T}$x reste local) ; une étiquette {/v} en
 * attente choisit la variante d'une somme.
 */
int
TclTypedProcReturn(Tcl_Interp *interp, void *sigv, int result)
{
    Interp *iPtr = (Interp *) interp;
    TclTypedSig *sig = (TclTypedSig *) sigv;
    Tcl_Obj *res, *lab = NULL, *base, *pushed, *out = NULL;
    int rc;

    if (sig == NULL || sig->retText == NULL
	    || iPtr->varFramePtr->tailcallPtr != NULL) {
	return result;			/* tailcall : le résultat viendra après */
    }
    if (!(result == TCL_OK || (result == TCL_RETURN
	    && iPtr->returnLevel == 1 && iPtr->returnCode == TCL_OK))) {
	return result;
    }
    res = Tcl_GetObjResult(interp);
    if (!TclIsAnnotated(res)) {
	/* v16s : un produit de scalaires rendu en liste nue ( $x, $y ) :
	 * ses champs vérifiés un à un ; {*}T le rend tel quel */
	size_t uid, gen = 0;		/* (initialisé : -Wmaybe-uninitialized sous MinGW) */
	TclType *rt;
	Tcl_Obj *q;

	(void) TclTypesGeneration(interp, &uid, &gen);
	if (sig->retTp == NULL || sig->retGen != gen) {
	    sig->retTp = TclAnnotTypeOf(interp, sig->retText);
	    sig->retGen = gen;
	}
	rt = sig->retTp;
	if (rt != NULL && rt->kind == TCLT_PRODUCT) {
	    if (sig->unfold == 1) {
		Tcl_Obj *plain = ScalarProductPlain(rt, res);

		if (plain != NULL) {
		    if (plain != res) {
			Tcl_SetObjResult(interp, plain);
		    }
		    return result;	/* la forme dépliée */
		}
	    } else if ((q = ScalarProductQuick(interp, rt, res)) != NULL) {
		SetRet(interp, sig, q);
		return result;
	    }
	}
    }
    if (TclIsAnnotated(res) && TclAnnotIsSettled(res)) {
	/* déjà du type de retour (return $p, p typée) : rien à faire */
	TclType *rt = TclAnnotTypeOf(interp, sig->retText), *vt = NULL;

	if (rt != NULL && TclAnnotInfo(res, NULL, &vt, NULL, NULL) == 1
		&& vt == rt) {
	    SetRet(interp, sig, res);
	    return result;
	}
    }
    Tcl_IncrRefCount(res);
    if (TclIsAnnotated(res)) {
	if (TclAnnotCheckOldLabel(interp, res) != TCL_OK) {
	    Tcl_Obj *msg = Tcl_GetObjResult(interp);

	    Tcl_IncrRefCount(msg);
	    ResetReturn(interp, result);
	    Tcl_SetObjResult(interp, msg);
	    Tcl_DecrRefCount(msg);
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "OLDLABEL", (char *) NULL);
	    Tcl_DecrRefCount(res);
	    return TCL_ERROR;
	}
	lab = TclAnnotLabelOf(res);
	TclAnnotExpire(interp, res);
    }
    if (lab == NULL) {
	/* le cas courant : une valeur sans étiquette (cast direct) */
	if (TclAnnotCastText(interp, sig->retText, res, &out) == TCL_OK) {
	    SetRet(interp, sig, out);
	    Tcl_DecrRefCount(res);
	    return result;
	}
	pushed = NULL;
	base = NULL;
	goto failed;
    }
    {
	/* {/v} : v doit être une variante du type de retour, une somme */
	TclType *rt = TclAnnotTypeOf(interp, sig->retText);
	Tcl_Size ll;
	const char *lt = Tcl_GetStringFromObj(lab, &ll);
	char *vn = (char *) Tcl_Alloc(ll);

	memcpy(vn, lt + 1, ll - 1);	/* {/v} : le nom suit la barre */
	vn[ll - 1] = '\0';
	if (strchr(vn, '/') != NULL) {
	    *strchr(vn, '/') = '\0';	/* v18b : {/v/w}, la variante v */
	}
	while (rt != NULL && rt->kind != TCLT_SUM && rt->under != NULL) {
	    rt = rt->under;
	}
	if (rt == NULL || rt->kind != TCLT_SUM) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("label {%s} on a return "
		    "value: the return type %s is not a sum", lt,
		    Tcl_GetString(sig->retText)));
	} else if (TclTypeFieldIndex(rt, vn) < 0) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("label {%s} on a return "
		    "value: %s has no variant \"%s\"", lt,
		    Tcl_GetString(sig->retText), vn));
	} else {
	    rt = NULL;			/* bonne étiquette */
	}
	Tcl_Free(vn);
	if (rt != NULL) {
	    Tcl_Obj *msg = Tcl_GetObjResult(interp);

	    Tcl_IncrRefCount(msg);
	    ResetReturn(interp, result);
	    Tcl_SetObjResult(interp, msg);
	    Tcl_DecrRefCount(msg);
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "LABEL", (char *) NULL);
	    pushed = NULL;
	    base = NULL;
	    result = TCL_ERROR;
	    goto cleanup;
	}
    }
    base = TclAnnotPush(res, lab);
    Tcl_IncrRefCount(base);
    pushed = TclAnnotPush(base, sig->retText);
    Tcl_IncrRefCount(pushed);
    rc = TclAnnotSettle(interp, pushed, NULL, 0, &out);
    if (rc == TCL_OK) {
	SetRet(interp, sig, out);
    } else {
	Tcl_Obj *msg;

      failed:
	msg = Tcl_ObjPrintf("return value {%s}: ",
		Tcl_GetString(sig->retText));

	Tcl_AppendObjToObj(msg, Tcl_GetObjResult(interp));
	Tcl_IncrRefCount(msg);
	{
	    Tcl_Obj *code = Tcl_GetReturnOptions(interp, TCL_ERROR), *ec = NULL,
		    *k = Tcl_NewStringObj("-errorcode", -1);

	    Tcl_IncrRefCount(code);
	    Tcl_IncrRefCount(k);
	    Tcl_DictObjGet(NULL, code, k, &ec);
	    if (ec != NULL) {
		Tcl_IncrRefCount(ec);
	    }
	    ResetReturn(interp, result);
	    Tcl_SetObjResult(interp, msg);
	    if (ec != NULL) {
		Tcl_SetObjErrorCode(interp, ec);
		Tcl_DecrRefCount(ec);
	    }
	    Tcl_DecrRefCount(k);
	    Tcl_DecrRefCount(code);
	}
	Tcl_DecrRefCount(msg);
	result = TCL_ERROR;
    }
  cleanup:
    if (pushed != NULL) {
	Tcl_DecrRefCount(pushed);
    }
    if (base != NULL) {
	Tcl_DecrRefCount(base);
    }
    if (lab != NULL) {
	Tcl_DecrRefCount(lab);
    }
    Tcl_DecrRefCount(res);
    return result;
}

/* type proc NOM : la signature (dict : args, return). */
static int
TypeProcCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Proc *procPtr;
    TclTypedSig *sig;
    Tcl_Obj *d;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "procName");
	return TCL_ERROR;
    }
    procPtr = TclFindProc((Interp *) interp, Tcl_GetString(objv[2]));
    if (procPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("\"%s\" isn't a procedure",
		Tcl_GetString(objv[2])));
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "PROCEDURE",
		Tcl_GetString(objv[2]), (char *) NULL);
	return TCL_ERROR;
    }
    sig = (TclTypedSig *) procPtr->typedSig;
    d = Tcl_NewDictObj();
    Tcl_DictObjPut(NULL, d, Tcl_NewStringObj("args", 4),
	    (sig != NULL && sig->argsText != NULL) ? sig->argsText
	    : Tcl_NewObj());
    Tcl_DictObjPut(NULL, d, Tcl_NewStringObj("return", 6),
	    (sig != NULL && sig->retText != NULL) ? Tcl_ObjPrintf("%s%s",
	    sig->unfold == 2 ? "{**}" : sig->unfold == 1 ? "{*}" : "",
	    Tcl_GetString(sig->retText)) : Tcl_NewObj());
    /* v16z : le produit des valeurs d'un appel (type call) */
    Tcl_DictObjPut(NULL, d, Tcl_NewStringObj("record", 6),
	    SigRecord(interp, procPtr, 0, NULL));
    Tcl_SetObjResult(interp, d);
    return TCL_OK;
}

/*
 * v16z : type call ?TYPE? -- l'appel courant, comme une valeur (l'appel
 * réifié). Une signature est un produit : les arguments liés d'un appel en
 * sont une valeur.
 *
 *  - Sans TYPE : le produit « enregistrement » de la signature. Chaque champ
 *    y est sous le nom de sa locale (-mode : mode), avec son type ; un champ
 *    variadique {T}nom... y est un list.T, un lien {var.T}n la valeur de la
 *    variable (T). Un champ sans valeur à cet appel (option absente sans
 *    défaut, lien vers une variable qui n'existe pas) n'y est pas. Pour une
 *    procédure classique : {any}a pour chaque argument, {list}args.
 *  - Avec TYPE (un produit) : chaque champ de TYPE prend la variable locale
 *    de même nom ; un champ sans variable prend le défaut de TYPE.
 *
 * SigRecord rend le texte du produit (les champs dans l'ordre de la
 * signature) et, si frame != NULL, les paires nom/valeur de l'appel.
 */

/* La valeur d'une option exclusive : la variante choisie (un littéral), ou
 * « variante charge ». NULL : aucune. */
static Tcl_Obj *
ExclValue(Tcl_Interp *interp, TypedArg *a)
{
    const TclType *sum = a->sub, *vp;
    Tcl_Obj *vn = Tcl_ObjGetVar2(interp, a->local, NULL, 0), *l, *pay;
    Tcl_Size v, d, np = 0;

    if (vn == NULL || sum == NULL) {
	return NULL;
    }
    v = SumVariantByName(sum, Tcl_GetString(vn));
    if (v < 0) {
	return NULL;
    }
    if (sum->fields[v].type == NULL || IsFlagType(sum->fields[v].type)) {
	return vn;
    }
    if ((vp = UnderKind(sum->fields[v].type, TCLT_PRODUCT)) != NULL) {
	/* une variante produit : ses champs sont des locales */
	Tcl_Obj **pairs = (Tcl_Obj **) Tcl_Alloc(
		(2 * vp->nfields + 1) * sizeof(Tcl_Obj *));
	Tcl_Obj *content = NULL;

	for (d = 0; d < vp->nfields; d++) {
	    Tcl_Obj *fv, *ln;

	    if (vp->fields[d].name == NULL) {
		continue;
	    }
	    ln = OptLocalName(vp->fields[d].name);
	    Tcl_IncrRefCount(ln);
	    fv = Tcl_ObjGetVar2(interp, ln, NULL, 0);
	    Tcl_DecrRefCount(ln);
	    if (fv != NULL) {
		pairs[np++] = vp->fields[d].name;
		pairs[np++] = fv;
	    }
	}
	if (TclBuildProductNamed(interp, (TclType *) vp, np, pairs, &content)
		!= TCL_OK) {
	    Tcl_Free(pairs);
	    Tcl_ResetResult(interp);
	    return NULL;
	}
	Tcl_Free(pairs);
	pay = content;
    } else {
	pay = Tcl_ObjGetVar2(interp, VariantName(&sum->fields[v]), NULL, 0);
	if (pay == NULL) {
	    return NULL;
	}
    }
    l = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(NULL, l, vn);
    Tcl_ListObjAppendElement(NULL, l, pay);
    return l;
}

static Tcl_Obj *
SigRecord(Tcl_Interp *interp, Proc *procPtr, int call, Tcl_Obj *pairs)
{
    TclTypedSig *sig = (TclTypedSig *) procPtr->typedSig;
    Tcl_Obj *rec = Tcl_NewObj();
    Tcl_Size i;
    int first = 1;

#define REC_FIELD(T, N) do {						\
	Tcl_AppendPrintfToObj(rec, "%s{%s}%s", first ? "" : "; ", (T),	\
		Tcl_GetString(N));					\
	first = 0;							\
    } while (0)

    if (sig == NULL || sig->n < 0) {
	CompiledLocal *lp = procPtr->firstLocalPtr;
	Tcl_Size k;

	for (k = 0; lp != NULL && k < procPtr->numArgs;
		k++, lp = lp->nextPtr) {
	    Tcl_Obj *nm = Tcl_NewStringObj(lp->name, lp->nameLength), *v;
	    int rest = (k == procPtr->numArgs - 1 && lp->flags & VAR_IS_ARGS);

	    Tcl_IncrRefCount(nm);
	    v = call ? Tcl_ObjGetVar2(interp, nm, NULL, 0) : NULL;
	    if (!call || v != NULL) {
		REC_FIELD(rest ? "list" : "any", nm);
		if (v != NULL) {
		    Tcl_ListObjAppendElement(NULL, pairs, nm);
		    Tcl_ListObjAppendElement(NULL, pairs, v);
		}
	    }
	    Tcl_DecrRefCount(nm);
	}
	return rec;
    }
    for (i = 0; i < sig->n; i++) {
	TypedArg *a = &sig->a[i];
	Tcl_Obj *v = NULL;
	char *tt;

	if (a->isVar) {
	    tt = a->varElem != NULL ? Tcl_GetString(a->varElem) : (char *) "any";
	} else {
	    tt = Tcl_GetString(a->typeText);
	}
	if (call) {
	    v = (a->optKind == OPT_EXCL) ? ExclValue(interp, a)
		    : Tcl_ObjGetVar2(interp, a->local, NULL, 0);
	    if (v == NULL) {
		continue;		/* sans valeur à cet appel */
	    }
	}
	REC_FIELD(tt, a->local);	/* {T}args... : typeText est list.T */
	if (v != NULL) {
	    Tcl_ListObjAppendElement(NULL, pairs, a->local);
	    Tcl_ListObjAppendElement(NULL, pairs, v);
	}
    }
#undef REC_FIELD
    return rec;
}

static int
TypeCallCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *framePtr = iPtr->varFramePtr;
    Tcl_Obj *pairs, *rec = NULL, *content = NULL, *res, **pv;
    TclType *tp;
    Tcl_Size np, i;
    int rc = TCL_ERROR;

    if (objc > 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "?type?");
	return TCL_ERROR;
    }
    if (framePtr == NULL || !(framePtr->isProcCallFrame & FRAME_IS_PROC)
	    || framePtr->procPtr == NULL) {
	ERR(interp, "CALL", "type call: not called from a procedure");
	return TCL_ERROR;
    }
    pairs = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(pairs);
    if (objc == 3) {
	/* les champs de TYPE, pris dans les locales de même nom */
	Tcl_Size tl;
	const char *tt = Tcl_GetStringFromObj(objv[2], &tl);

	tp = TclTypeParse(interp, tt, tl);
	if (tp == NULL) {
	    goto done;
	}
	if (UnderKind(tp, TCLT_PRODUCT) == NULL) {
	    ERR(interp, "CALL", "type call: %s is not a product", tp->name);
	    goto done;
	}
	{
	    const TclType *pp = UnderKind(tp, TCLT_PRODUCT);

	    for (i = 0; i < pp->nfields; i++) {
		Tcl_Obj *v;

		if (pp->fields[i].name == NULL) {
		    continue;
		}
		v = Tcl_ObjGetVar2(interp, pp->fields[i].name, NULL, 0);
		if (v != NULL) {
		    Tcl_ListObjAppendElement(NULL, pairs, pp->fields[i].name);
		    Tcl_ListObjAppendElement(NULL, pairs, v);
		}
	    }
	}
    } else {
	rec = SigRecord(interp, framePtr->procPtr, 1, pairs);
	Tcl_IncrRefCount(rec);
	if (Tcl_GetCharLength(rec) == 0) {
	    /* aucun argument : un produit vide n'existe pas */
	    Tcl_SetObjResult(interp, Tcl_NewObj());
	    rc = TCL_OK;
	    goto done;
	}
	{
	    Tcl_Size rl;
	    const char *rt = Tcl_GetStringFromObj(rec, &rl);

	    tp = TclTypeParse(interp, rt, rl);
	}
	if (tp == NULL) {
	    goto done;
	}
    }
    Tcl_ListObjGetElements(NULL, pairs, &np, &pv);
    if (TclBuildProductNamed(interp, tp, np, pv, &content) != TCL_OK) {
	goto done;
    }
    Tcl_IncrRefCount(content);
    res = TclNewAnnotated(interp, tp, -1, content);
    Tcl_DecrRefCount(content);
    Tcl_SetObjResult(interp, res);
    rc = TCL_OK;

  done:
    Tcl_DecrRefCount(pairs);
    if (rec != NULL) {
	Tcl_DecrRefCount(rec);
    }
    return rc;
}

/* info args / info default d'une procédure typée : les champs. NULL, -1 :
 * liste d'arguments classique. */
Tcl_Obj *
TclTypedSigArgNames(void *sigv)
{
    TclTypedSig *sig = (TclTypedSig *) sigv;
    Tcl_Obj *l;
    Tcl_Size i;

    if (sig == NULL || sig->n < 0) {
	return NULL;
    }
    l = Tcl_NewListObj(0, NULL);
    for (i = 0; i < sig->n; i++) {
	Tcl_ListObjAppendElement(NULL, l, sig->a[i].name);
    }
    return l;
}

/* 1 : le champ a un défaut (*defPtr, ou le texte [cmd]) ; 0 : non ; -1 : pas
 * un champ ; -2 : liste classique. */
int
TclTypedSigDefault(void *sigv, const char *name, Tcl_Obj **defPtr)
{
    TclTypedSig *sig = (TclTypedSig *) sigv;
    Tcl_Size i;

    if (sig == NULL || sig->n < 0) {
	return -2;
    }
    for (i = 0; i < sig->n; i++) {
	if (strcmp(Tcl_GetString(sig->a[i].name), name) == 0) {
	    if (sig->a[i].defval != NULL) {
		*defPtr = sig->a[i].defval;
		return 1;
	    }
	    if (sig->a[i].defcmd != NULL) {
		*defPtr = Tcl_ObjPrintf("[%s]",
			Tcl_GetString(sig->a[i].defcmd));
		return 1;
	    }
	    if (sig->a[i].isFlag) {
		*defPtr = Tcl_NewWideIntObj(0);	/* un drapeau absent : 0 */
		return 1;
	    }
	    return 0;
	}
    }
    return -1;
}

/* v16s : les noms des variables locales des champs (pour qu'elles soient
 * des locales compilées : tclProc.c, AddTypedLocals). */
Tcl_Size
TclTypedSigLocals(void *sigv, Tcl_Obj ***namesPtr)
{
    TclTypedSig *sig = (TclTypedSig *) sigv;
    Tcl_Size i;

    if (sig == NULL || sig->n <= 0) {
	*namesPtr = NULL;
	return 0;
    }
    if (sig->locals == NULL) {
	sig->locals = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * sig->n);
	for (i = 0; i < sig->n; i++) {
	    sig->locals[i] = sig->a[i].local;
	}
    }
    *namesPtr = sig->locals;
    return sig->n;
}

/* v16s : la signature lie elle-même les mots (liste typée) : InitArgsAndLocals
 * n'a pas à faire la liste « args ». */
int
TclTypedSigTakesWords(void *sigv)
{
    return sigv != NULL && ((TclTypedSig *) sigv)->n >= 0;
}
