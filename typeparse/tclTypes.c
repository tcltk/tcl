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
    return (TypeData *) Tcl_GetAssocData(interp, "tclTypes", NULL);
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

static void
DeleteData(void *clientData, Tcl_Interp *interp)
{
    TypeData *d = (TypeData *) clientData;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    Tcl_Size i;

    (void) interp;
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
	if (d->all[i]->compact != NULL) {
	    TclCompactLayoutRelease(d->all[i]->compact);
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
    Tcl_DeleteHashTable(&d->converts);
    Tcl_DeleteHashTable(&d->derefs);
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

/* "any" : type prédéfini des éléments d'une liste hétérogène (list.any).
 * Chaque élément y conserve sa propre annotation. */
int
TclTypeIsAny(const TclType *tp)
{
    return tp->kind == TCLT_STRING && tp->reserved
	    && strcmp(tp->name, "any") == 0;
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
    return TclTypesMakeList(GetData(interp), elem);
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
    if (b->arity < 0) {
	return 1;
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
    if (a->kind == TCLT_SUM) {
	for (i = 0; i < a->nfields; i++) {
	    if (!VariantIncludesType(&a->fields[i], b, depth)) {
		return 0;
	    }
	}
	return 1;
    }
    if (b->kind == TCLT_SUM) {
	for (i = 0; i < b->nfields; i++) {
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
		    && ArityWithin(a->arity < 0 ? 0 : a->arity, a->arity, b);
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

    if (Tcl_GetBignumFromObj(interp, TCL_ANNOT_CONTENT(value), &big)
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

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "value");
	return TCL_ERROR;
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
	    || (tp->binTag != NULL && tp->binTag[0] == '=')) {
	return 0;
    }
    if (ValidateG(NULL, tp, TCL_ANNOT_CONTENT(content), -1, &got, NULL)
	    != TCL_OK) {
	return 1;
    }
    return got != v;
}

static int
ValidateG(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *value,
	int variantHint, int *variantOut, const ValGuard *up)
{
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
		&& ownTp == tp && ownVariant >= 0) {
	    variantHint = ownVariant;
	}
    }
    value = TCL_ANNOT_CONTENT(value);	/* une valeur annotée : son contenu */
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
	    if (tp->arity >= 0 && n != tp->arity) {
		ERR(interp, "LIST", "%s needs exactly %" TCL_SIZE_MODIFIER
			"d elements but got %" TCL_SIZE_MODIFIER "d", tp->name,
			tp->arity, n);
		return TCL_ERROR;
	    }
	    return TCL_OK;
	}
	if (Tcl_ListObjGetElements(interp, value, &n, &elems) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (tp->arity >= 0 && n != tp->arity) {
	    ERR(interp, "LIST", "%s needs exactly %" TCL_SIZE_MODIFIER
		    "d elements but got %" TCL_SIZE_MODIFIER "d", tp->name,
		    tp->arity, n);
	    return TCL_ERROR;
	}
	if (tp->elem->kind != TCLT_STRING || tp->elem->nominal) {
	    for (i = 0; i < n; i++) {
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

	if (variantHint <= -2) {
	    /* -2 - v : la variante courante, qui départage des variantes
	     * également strictes (écriture dans une variable typée) */
	    prefer = -2 - variantHint;
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
		ERR(interp, "SUM", "value \"%s\" is not valid for that variant "
			"of %s", Tcl_GetString(value), tp->name);
		return TCL_ERROR;
	    }
	    if (variantOut != NULL) {
		*variantOut = variantHint;
	    }
	    return TCL_OK;
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

			Tcl_AppendPrintfToObj(msg, " of %s equally: write {%s}%s",
				tp->name, v0, Tcl_GetString(q));
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

    if (TclTypeValidate(interp, tp, content, variantHint, &variant)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (variantOut != NULL) {
	*variantOut = variant;
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

TclType *
TclTypesResolve(Tcl_Interp *interp, TypeData *d, const char *selfName,
	TclType *self, const char *w, Tcl_Size len)
{
    char *name = DupStr(w, len);
    Tcl_HashEntry *hPtr;
    TclType *tp = NULL;

    if (selfName != NULL && strcmp(name, selfName) == 0) {
	Tcl_Free(name);
	return self;
    }
    hPtr = Tcl_FindHashEntry(&d->names, name);
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
    Tcl_DString ds;
    Tcl_HashEntry *hPtr;
    Tcl_Size i;
    int isNew;
    TclType *tp;

    if (DuplicateName(fields, n)) {
	ERR(interp, "SYNTAX", "duplicate %s name in %s",
		kind == TCLT_SUM ? "variant" : "field",
		kind == TCLT_SUM ? "sum" : "product");
	goto fail;
    }
    if (kind == TCLT_SUM) {
	for (i = 0; i < n; i++) {
	    if (fields[i].name != NULL) {
		const char *nm = Tcl_GetString(fields[i].name);

		/* Un nom de variante ne doit pas être aussi un nom de type. */

		if (Tcl_FindHashEntry(&d->names, nm) != NULL
			|| (selfName != NULL && strcmp(nm, selfName) == 0)) {
		    ERR(interp, "SYNTAX", "variant name \"%s\" is also a type "
			    "name", nm);
		    goto fail;
		}
	    }
	}
	SortVariants(fields, n);
    }

    if (into != NULL) {
	into->kind = kind;
	into->fields = fields;
	into->nfields = n;
	return into;
    }

    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, kind == TCLT_PRODUCT ? "prod<" : "sum<",
	    TCL_AUTO_LENGTH);
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
	Tcl_SetHashValue(hPtr, tp);
    }
    Tcl_DStringFree(&ds);
    return tp;

  fail:
    TclTypesFreeFields(fields, n);
    return NULL;
}

TclType *
TclTypeParse(Tcl_Interp *interp, const char *text, Tcl_Size len)
{
    TypeEnv env;

    memset(&env, 0, sizeof(env));
    env.d = GetData(interp);
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
 * Cast de "src" (valeur typée, ou chaîne ordinaire) vers "target".
 * Ordre : même type ; conversion déclarée ; nominal <-> sous-jacent ;
 * conversions intégrées entre int, double et string ; variante d'une
 * somme ; sinon erreur. Ne modifie jamais src.
 */
int
TclTypeCast(Tcl_Interp *interp, Tcl_Obj *src, TclType *target,
	Tcl_Obj **resultPtr)
{
    TypeData *d = GetData(interp);
    Tcl_Obj *content, *res = NULL;
    TclType *from = NULL;
    int variant = -1, outVariant = -1, kind;
    Tcl_HashEntry *hPtr;
    char key[256];
    Tcl_Size i;

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
    if (from != NULL) {
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
    /* Variante d'une somme dont le type est exactement celui de la source. */
    if (target->kind == TCLT_SUM && from != NULL) {
	for (i = 0; i < target->nfields; i++) {
	    if (target->fields[i].type == from) {
		res = Tcl_DuplicateObj(content);
		outVariant = (int) i;
		goto validate;
	    }
	}
    }
    /*
     * Produit -> liste ou dict (cast de base de tout produit) : la liste de
     * ses champs, ou le dict nom -> champ. {list} et {dict} rendent les
     * champs nus (texte ordinaire) ; {list.any} et {dict.any} les rendent
     * annotés de leur type ; {list.T}, {list(N).T}, {dict.T} les valident
     * comme T. Pour une somme, on part de la variante active.
     */
    if (from != NULL && (target->kind == TCLT_LIST
	    || target->kind == TCLT_DICT)) {
	const TclType *pt = from;
	Tcl_Obj *pc = content, **fv;
	Tcl_Size nf;

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
    if (from != NULL && from->kind == target->kind
	    && (target->kind == TCLT_LIST || target->kind == TCLT_DICT)
	    && !(from->nominal && target->nominal)) {
	Tcl_Obj **ev;
	Tcl_Size ne;
	int annotated = (target->elem != NULL && TclTypeIsAny(target->elem));

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
		res = Tcl_NewDoubleObj(dbl);	/* perd la précision > 2^53 */
	    } else if (target->kind == TCLT_STRING) {
		res = Tcl_NewStringObj(Tcl_GetString(content), TCL_AUTO_LENGTH);
	    } else {
		res = Tcl_DuplicateObj(content);	/* INT<-INT, STRING<-... */
	    }
	    goto validate;
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
    fn = ff->name != NULL ? Tcl_GetString(ff->name) : "?";
    tn = tf->name != NULL ? Tcl_GetString(tf->name) : "?";
    if (from == to) {
	*resPtr = Tcl_DuplicateObj(content);
	return TCL_OK;
    }
    snprintf(key, sizeof(key), "%s.%s\1%s.%s", sum->name, fn, sum->name, tn);
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
		    "rule: type convert %s.%s %s.%s {{x} ...}", sum->name, fn,
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
    if (TclTypeValidate(interp, sum, res, to, NULL) != TCL_OK) {
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

    if (*p >= '0' && *p <= '9') {
	Tcl_DStringAppend(ds, "_", 1);
    }
    while (*p) {
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
    CIdent(tp->name, &cn);
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
		: TclTypesMakeList(d, e);
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
    into->binLink = res->binLink;
    into->binTag = res->binTag;
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
CheckNewName(Tcl_Interp *interp, TypeData *d, const char *name)
{
    const char *q;

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
    if (Tcl_FindHashEntry(&d->generics, name) != NULL) {
	ERR(interp, "NAME", "type \"%s\" is already defined", name);
	return TCL_ERROR;
    }
    if (Tcl_FindHashEntry(&d->names, name) != NULL) {
	ERR(interp, "NAME", "type \"%s\" is %s", name,
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
	 * Deux variantes d'une même somme : type convert S.a S.b {{x} ...}.
	 * La règle sert à la lecture d'une variante inactive ($v(b)) et au
	 * cast explicite {S.b}$v ; elle n'intervient jamais dans la déduction.
	 */
	TclType *vs[2] = {NULL, NULL};
	Tcl_Size vi[2] = {-1, -1};
	int nv = 0;

	for (i = 0; i < 2; i++) {
	    const char *nm = Tcl_GetString(objs[i]);
	    const char *dot = strrchr(nm, '.');

	    if (dot != NULL && Tcl_FindHashEntry(&d->names, nm) == NULL) {
		Tcl_DString sd;
		Tcl_HashEntry *h;

		Tcl_DStringInit(&sd);
		Tcl_DStringAppend(&sd, nm, dot - nm);
		h = Tcl_FindHashEntry(&d->names, Tcl_DStringValue(&sd));
		Tcl_DStringFree(&sd);
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
			"variants of the same sum, as in S.a S.b, not \"%s\" and "
			"\"%s\"", Tcl_GetString(fromObj), Tcl_GetString(toObj));
		return TCL_ERROR;
	    }
	    if (vi[0] == vi[1]) {
		ERR(interp, "SYNTAX", "type convert: \"%s\" and \"%s\" are the "
			"same variant", Tcl_GetString(fromObj),
			Tcl_GetString(toObj));
		return TCL_ERROR;
	    }
	    snprintf(key, sizeof(key), "%s.%s\1%s.%s", vs[0]->name,
		    Tcl_GetString(vs[0]->fields[vi[0]].name), vs[1]->name,
		    Tcl_GetString(vs[1]->fields[vi[1]].name));
	    goto store;
	}
    }
    for (i = 0; i < 2; i++) {
	const char *nm = Tcl_GetString(objs[i]);
	Tcl_HashEntry *h = Tcl_FindHashEntry(&d->names, nm);

	if (h != NULL && Tcl_GetHashValue(h) != NULL) {
	    snprintf(side[i], sizeof(side[i]), "%s",
		    ((TclType *) Tcl_GetHashValue(h))->name);
	} else if (Tcl_FindHashEntry(&d->generics, nm) != NULL) {
	    snprintf(side[i], sizeof(side[i]), "@%s", nm);
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
	    const char *nm = Tcl_GetString(target->fields[i].name);

	    if (strcmp(nm, "some") == 0) {
		some = target->fields[i].type;
	    } else if (strcmp(nm, "none") == 0) {
		hasNone = 1;
	    }
	}
	if (some == NULL || !hasNone) {
	    Tcl_DecrRefCount(reprR);
	    ERR(interp, "BIN", "%s: a link to a sum needs the variants "
		    "{T}some and {unit}none", tp->name);
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
 * Codec C de « type binary encode » pour les dispositions FIXES
 * --------------------------------------------------------------------
 *
 * Entiers u/s de 1, 2, 4 ou 8 octets, flottants de 4 ou 8 octets, produits
 * de ces types, listes de taille fixe (list(N).T) ou donnée par un champ
 * déjà écrit du même produit (list($n).T). Même disposition que la couche
 * Tcl (layout/filebinary.tcl) : remplissage avant chaque champ jusqu'à son
 * alignement, et à la fin d'un produit jusqu'au sien ; alignement d'un
 * scalaire = la plus grande puissance de 2 <= sa taille, plafonnée par
 * -align. Tout le reste (chaînes, sommes, liens, at(...), préfixes de
 * taille, bitarray, -dedupe, valeur non typée, incohérence n / éléments)
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

/* Contexte d'un encodage : options et mémo des alignements par type. */
#define BIN_MEMO 16
typedef struct {
    size_t cap;
    int bigDefault, hostBig, nmemo;
    BinBuf *out;
    const TclType *memoT[BIN_MEMO];
    size_t memoA[BIN_MEMO];
} BinCtx;

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

/* Alignement d'un type couvert ; 0 si le type n'est pas couvert. */
static size_t
BinFastAlign(const TclType *tp, size_t cap, int depth)
{
    Tcl_Size i;
    size_t m = 1, a;

    if (depth > 32) {
	return 0;
    }
    if (BinFastScalar(tp)) {
	size_t p = 1;

	while (p * 2 <= (size_t) tp->binSize) {
	    p *= 2;
	}
	return (cap <= 1) ? 1 : (p < cap ? p : cap);
    }
    if (tp->binAt != NULL || tp->binLink != NULL || tp->binTag != NULL) {
	return 0;
    }
    if (tp->kind == TCLT_LIST && tp->elem != NULL) {
	if (tp->binDyn != NULL && (tp->binDyn->kind != TCLDYN_EXPR
		|| tp->binDyn->expr == NULL || tp->binDyn->expr[0] != '$')) {
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

static int
BinFastEnc(BinCtx *c, const TclType *tp, Tcl_Obj *v, const TclType *encl,
	Tcl_Obj **vals, Tcl_Size nvals, int depth)
{
    BinBuf *out = c->out;
    int bigDefault = c->bigDefault, hostBig = c->hostBig;
    Tcl_Obj **elv;
    Tcl_Size n, i;

    v = BinContent(v);
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
    if (Tcl_ListObjGetElements(NULL, v, &n, &elv) != TCL_OK) {
	return TCL_CONTINUE;
    }
    if (tp->kind == TCLT_LIST) {
	if (tp->binDyn != NULL) {
	    /* list($n).T : n est un champ déjà écrit du produit englobant */
	    const char *name = tp->binDyn->expr + 1;
	    Tcl_Size f;
	    Tcl_WideInt want;

	    if (encl == NULL) {
		return TCL_CONTINUE;
	    }
	    for (f = 0; f < nvals; f++) {
		if (encl->fields[f].name != NULL
			&& strcmp(Tcl_GetString(encl->fields[f].name), name) == 0) {
		    break;
		}
	    }
	    if (f == nvals || Tcl_GetWideIntFromObj(NULL, BinContent(vals[f]),
		    &want) != TCL_OK || want != (Tcl_WideInt) n) {
		return TCL_CONTINUE;	/* le message d'erreur : couche Tcl */
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
    for (i = 0; i < n; i++) {
	const TclType *ft = tp->fields[i].type;
	int rc;

	BinBufPad(out, BinAlignOf(c, ft, depth + 1));
	rc = BinFastEnc(c, ft, elv[i], tp, elv, i, depth + 1);
	if (rc != TCL_OK) {
	    return rc;
	}
    }
    BinBufPad(out, BinAlignOf(c, tp, depth));
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
    probe.s = 1;
    hostBig = (probe.c[0] == 0);
    ctx.cap = cap;
    ctx.bigDefault = bigDefault;
    ctx.hostBig = hostBig;
    ctx.nmemo = 0;
    ctx.out = &out;
    rc = BinFastEnc(&ctx, tp, objv[1], NULL, NULL, 0, 0);
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
	    const char *name = tp->binDyn->expr + 1;
	    Tcl_Size f;

	    if (encl == NULL) {
		return TCL_CONTINUE;
	    }
	    for (f = 0; f < nvals; f++) {
		if (encl->fields[f].name != NULL
			&& strcmp(Tcl_GetString(encl->fields[f].name), name) == 0) {
		    break;
		}
	    }
	    if (f == nvals || Tcl_GetWideIntFromObj(NULL, vals[f], &want)
		    != TCL_OK) {
		return TCL_CONTINUE;
	    }
	} else {
	    want = tp->arity;
	}
	if (want < 0 || (size_t) want > d->len - d->pos) {
	    return TCL_CONTINUE;	/* taille absurde : couche Tcl */
	}
	n = (Tcl_Size) want;
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
    for (i = 0; i < n; i++) {
	const TclType *ft = tp->fields[i].type;

	rc = BinDecPad(d, BinAlignOf(&d->b, ft, depth + 1));
	if (rc == TCL_OK) {
	    rc = BinFastDec(d, ft, 1, elv, tp, i, depth + 1, &elv[i]);
	}
	if (rc != TCL_OK) {
	    Tcl_Size j;

	    for (j = 0; j < i; j++) {
		Tcl_DecrRefCount(elv[j]);	/* référence prise ci-dessous */
	    }
	    Tcl_Free(elv);
	    return rc;
	}
	Tcl_IncrRefCount(elv[i]);
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
    int rc;

    if ((flag != NULL && strcmp(flag, "0") == 0) || objc < 2 || objc % 2) {
	return TCL_CONTINUE;
    }
    memset(&d, 0, sizeof(d));
    d.b.cap = 1;
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
	     * son message par la couche Tcl */
	    if (strcmp(a, "Inf") != 0 && (Tcl_GetWideIntFromObj(NULL,
		    objv[i + 1], &w) != TCL_OK || w < 0)) {
		return TCL_CONTINUE;
	    }
	} else if (strcmp(o, "-consumed") == 0) {
	    consumed = (a[0] != '\0') ? objv[i + 1] : NULL;
	} else {
	    return TCL_CONTINUE;
	}
    }
    if (TclIsAnnotated(objv[0])) {
	return TCL_CONTINUE;
    }
    bytes = Tcl_GetBytesFromObj(NULL, objv[1], &len);
    if (bytes == NULL || (Tcl_WideInt) len < off) {
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
    probe.s = 1;
    d.b.hostBig = (probe.c[0] == 0);
    d.interp = interp;
    d.p = bytes;
    d.len = (size_t) len;
    d.pos = d.base = (size_t) off;
    rc = BinFastDec(&d, tp, 0, NULL, NULL, 0, 0, &res);
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
	Tcl_ListObjAppendElement(NULL, *words, Tcl_NewStringObj(t, -1));
	TclVecAdd(keep, rest);
	cur = rest;
	n++;
    }
    *subject = cur;
    return n;
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
    } else {
	ERR(interp, "IS", "unknown question \"%s\": must be compact?",
		Tcl_GetString(q));
    }
  done:
    Tcl_DecrRefCount(words);
    TclVecFree(&keep);
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
    sub = Tcl_GetString(objv[1]);
    if (strcmp(sub, "def") == 0 || strcmp(sub, "define") == 0
	    || strcmp(sub, "alias") == 0) {
	int isAlias = (strcmp(sub, "alias") == 0), compact = 0, rc;
	Tcl_Obj *words, *nameObj, **wv;
	TclObjVec keep = {NULL, 0, 0};
	Tcl_Size nw, k;

	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "name definition");
	    return TCL_ERROR;
	}
	/* type def {-compact}T ... : options sur le nom du type */
	LocalWords(objv[2], &words, &nameObj, &keep);
	Tcl_IncrRefCount(words);
	Tcl_ListObjGetElements(NULL, words, &nw, &wv);
	for (k = 0; k < nw; k++) {
	    const char *w = Tcl_GetString(wv[k]);

	    if (strcmp(w, "-compact") == 0 && !isAlias) {
		compact = 1;
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
	rc = TypeDefine(interp, d, nameObj, objv[3], isAlias);
	if (rc == TCL_OK && compact) {
	    const char *nm = Tcl_GetString(nameObj);
	    Tcl_HashEntry *h = Tcl_FindHashEntry(&d->names, nm);
	    TclType *tp = (h != NULL) ? (TclType *) Tcl_GetHashValue(h) : NULL;

	    if (tp == NULL || tp->kind != TCLT_LIST || tp->elem == NULL
		    || TclCompactLayoutOf(tp->elem) == NULL) {
		ERR(interp, "COMPACT", "type def {-compact}%s: only a list of "
			"fixed-size scalars (int(s,n), int(u,n) with n = 8, 16, "
			"32 or 64, double, double(32|64)) or of products of them "
			"can be compact, not %s", nm, tp != NULL ? tp->name : "?");
		if (h != NULL) {
		    Tcl_DeleteHashEntry(h);	/* la définition est annulée */
		}
		rc = TCL_ERROR;
	    } else {
		tp->compactWanted = 1;
	    }
	}
	TclVecFree(&keep);
	return rc;
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
		/* S.v : une variante d'une somme (comme {S.v}$x). */
		const char *dot = strrchr(tt, '.');
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
    if (strcmp(sub, "binary") == 0) {
	return TypeBinaryCmd(interp, objc, objv);
    }
    ERR(interp, "SUBCMD", "bad subcommand \"%s\": must be def, alias, "
	    "cast, convert, deref, export, get, is or binary", sub);
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
    if (TclAnnotateValue(interp, objv[2], objv[3], &typed) != TCL_OK) {
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
    res = Tcl_NewListObj(0, NULL);
    for (i = 0; i < n; i++) {
	Tcl_ListObjAppendElement(NULL, res, DeepContent(elv[i]));
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

void
TclTypesInit(Tcl_Interp *interp)
{
    TypeData *d = (TypeData *) Tcl_Alloc(sizeof(TypeData));
    int isNew;

    memset(d, 0, sizeof(*d));
    Tcl_InitHashTable(&d->names, TCL_STRING_KEYS);
    Tcl_InitHashTable(&d->generics, TCL_STRING_KEYS);
    Tcl_InitHashTable(&d->instances, TCL_STRING_KEYS);
    Tcl_InitHashTable(&d->canon, TCL_STRING_KEYS);
    Tcl_InitHashTable(&d->converts, TCL_STRING_KEYS);
    Tcl_InitHashTable(&d->derefs, TCL_ONE_WORD_KEYS);
    d->tInt = Builtin(d, TCLT_INT, "int");
    d->tDouble = Builtin(d, TCLT_DOUBLE, "double");
    d->tString = Builtin(d, TCLT_STRING, "string");
    Builtin(d, TCLT_STRING, "any");		/* éléments de list.any */
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
    Tcl_CreateObjCommand2(interp, "type", TypeObjCmd, d, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::mathop::is", IsOpObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::unsupported::content", ContentObjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::unsupported::declare", DeclareObjCmd,
	    NULL, NULL);
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
