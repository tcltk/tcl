/*
 * tclAnnotObj.c --
 *
 *	L'objet annoté A = (V, T), spécification §6 et §7.
 *
 *	  A.internalRep.twoPtrValue.ptr1 = V : objet contenant la valeur
 *	  A.internalRep.twoPtrValue.ptr2 = T : objet contenant le type
 *
 *	T est de deux sortes :
 *	  - T RÉSOLU : objet privé, de type "typeref", interné par
 *	    (descripteur, variante). Il épingle le descripteur et la balise
 *	    d'une somme. Sa chaîne est le nom du type.
 *	  - T BRUT : simple objet chaîne, copie privée du texte du type
 *	    (annotation fabriquée par le parseur, résolue à la consommation).
 *
 *	La chaîne de A est celle de V : l'annotation est invisible.
 *	setFromAny échoue toujours : seul le code d'ici fabrique des A.
 *
 *	Écart avec la spécification : T ne cache pas une résolution « par
 *	nom avec époque » mais épingle directement le descripteur, ce qui
 *	donne le même comportement observable (le type survit à une
 *	redéfinition) sans époque.
 */

#include "tclTypes.h"

/* --------------------------------------------------------------------
 * Utilitaires
 * -------------------------------------------------------------------- */

void
TclDiscard(Tcl_Obj *objPtr)
{
    if (objPtr != NULL) {
	Tcl_IncrRefCount(objPtr);
	Tcl_DecrRefCount(objPtr);
    }
}

void
TclVecAdd(TclObjVec *vec, Tcl_Obj *objPtr)
{
    if (vec->n == vec->cap) {
	vec->cap = vec->cap ? 2 * vec->cap : 8;
	vec->v = (Tcl_Obj **) Tcl_Realloc(vec->v, vec->cap * sizeof(Tcl_Obj *));
    }
    Tcl_IncrRefCount(objPtr);
    vec->v[vec->n++] = objPtr;
}

void
TclVecFree(TclObjVec *vec)
{
    Tcl_Size i;

    for (i = 0; i < vec->n; i++) {
	Tcl_DecrRefCount(vec->v[i]);
    }
    if (vec->v != NULL) {
	Tcl_Free(vec->v);
    }
    vec->v = NULL;
    vec->n = vec->cap = 0;
}

/* --------------------------------------------------------------------
 * Type d'objet "typeref" : le T résolu
 * -------------------------------------------------------------------- */

static Tcl_DupInternalRepProc	DupTypeRef;
static Tcl_UpdateStringProc	UpdateStringOfTypeRef;
static Tcl_SetFromAnyProc	SetTypeRefFromAny;

static const Tcl_ObjType typeRefObjType = {
    "typeref", NULL, DupTypeRef, UpdateStringOfTypeRef, SetTypeRefFromAny,
    TCL_OBJTYPE_V0
};

static void
DupTypeRef(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
    Tcl_ObjInternalRep ir;

    ir.twoPtrValue.ptr1 = srcPtr->internalRep.twoPtrValue.ptr1;
    ir.twoPtrValue.ptr2 = srcPtr->internalRep.twoPtrValue.ptr2;
    Tcl_StoreInternalRep(dupPtr, &typeRefObjType, &ir);
}

static void
UpdateStringOfTypeRef(Tcl_Obj *objPtr)
{
    const TclType *tp = (const TclType *) objPtr->internalRep.twoPtrValue.ptr1;

    Tcl_InitStringRep(objPtr, tp->name, (Tcl_Size) strlen(tp->name));
}

static int
SetTypeRefFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"type references cannot be created from strings",
		TCL_AUTO_LENGTH));
    }
    return TCL_ERROR;
}

/* Table des T internés, une par interpréteur (données associées). */

typedef struct RefData {
    Tcl_HashTable refs;		/* "%p/%d" -> Tcl_Obj* (compté) */
} RefData;

static void
DeleteRefData(void *clientData, Tcl_Interp *interp)
{
    RefData *rd = (RefData *) clientData;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;

    for (hPtr = Tcl_FirstHashEntry(&rd->refs, &search); hPtr != NULL;
	    hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_DecrRefCount((Tcl_Obj *) Tcl_GetHashValue(hPtr));
    }
    Tcl_DeleteHashTable(&rd->refs);
    Tcl_Free(rd);
}

static RefData *
GetRefData(Tcl_Interp *interp)
{
    RefData *rd = (RefData *) Tcl_GetAssocData(interp, "tclTypeRefs", NULL);

    if (rd == NULL) {
	rd = (RefData *) Tcl_Alloc(sizeof(RefData));
	Tcl_InitHashTable(&rd->refs, TCL_STRING_KEYS);
	Tcl_SetAssocData(interp, "tclTypeRefs", DeleteRefData, rd);
    }
    return rd;
}

static Tcl_Obj *
TypeRefObj(Tcl_Interp *interp, TclType *tp, int variant)
{
    RefData *rd = GetRefData(interp);
    char key[64];
    int isNew;
    Tcl_HashEntry *hPtr;
    Tcl_Obj *tPtr;
    Tcl_ObjInternalRep ir;

    snprintf(key, sizeof(key), "%p/%d", (void *) tp, variant);
    hPtr = Tcl_CreateHashEntry(&rd->refs, key, &isNew);
    if (!isNew) {
	return (Tcl_Obj *) Tcl_GetHashValue(hPtr);
    }
    tPtr = Tcl_NewObj();
    ir.twoPtrValue.ptr1 = tp;
    ir.twoPtrValue.ptr2 = (void *) (intptr_t) (variant + 1);
    Tcl_StoreInternalRep(tPtr, &typeRefObjType, &ir);
    TclInvalidateStringRep(tPtr);	/* Tcl_NewObj() donne "" : à invalider */
    Tcl_IncrRefCount(tPtr);
    Tcl_SetHashValue(hPtr, tPtr);
    return tPtr;
}

/* --------------------------------------------------------------------
 * Type d'objet "annotated"
 * -------------------------------------------------------------------- */

static Tcl_FreeInternalRepProc	FreeAnnotated;
static Tcl_DupInternalRepProc	DupAnnotated;
static Tcl_UpdateStringProc	UpdateStringOfAnnotated;
static Tcl_SetFromAnyProc	SetAnnotatedFromAny;

#ifdef TCL_OBJTYPE_V2
/*
 * Vue "liste" d'une valeur annotée (Tcl 9, listes abstraites).
 *
 * Le type "annotated" fournit les procédures de liste de Tcl_ObjType V2 :
 * toutes les commandes de liste (lindex, llength, lrange, foreach, in...)
 * passent par elles au lieu de réinterpréter la chaîne.
 *
 *   - contenu de genre list, dict ou produit : la vue est la liste de V ;
 *   - contenu scalaire (string, int, double, énumération, variante inconnue) :
 *     la valeur est UN SEUL élément, "hello world" n'est pas deux mots.
 *
 * lset (setElementProc) et lappend, lreplace, linsert (replaceProc) sont
 * fournies aussi : sans elles le noyau transforme l'objet en liste ordinaire
 * (SetListFromAny) et l'annotation, donc le contrat, est perdue en silence.
 * Ici la liste modifiée est REVALIDÉE contre le type, et l'objet garde son
 * annotation ; en cas d'échec il reste inchangé.
 */
static Tcl_ObjTypeLengthProc	AnnotListLength;
static Tcl_ObjTypeIndexProc	AnnotListIndex;
static Tcl_ObjTypeSliceProc	AnnotListSlice;
static Tcl_ObjTypeReverseProc	AnnotListReverse;
static Tcl_ObjTypeGetElements	AnnotListGetElements;
static Tcl_ObjTypeSetElement	AnnotListSetElement;
static Tcl_ObjTypeReplaceProc	AnnotListReplace;
static Tcl_ObjTypeInOperatorProc AnnotListIn;
#define ANNOT_LIST_PROCS TCL_OBJTYPE_V2(AnnotListLength, AnnotListIndex, \
	AnnotListSlice, AnnotListReverse, AnnotListGetElements, \
	AnnotListSetElement, AnnotListReplace, AnnotListIn)
#else
#define ANNOT_LIST_PROCS TCL_OBJTYPE_V0
#endif

static TclTypeKind EffectiveKind(const TclType *tp, int variant,
	Tcl_Obj *content);
static Tcl_Obj *MakeAnnotated(Tcl_Obj *vPtr, Tcl_Obj *tPtr);

static const Tcl_ObjType annotatedObjType = {
    "annotated", FreeAnnotated, DupAnnotated, UpdateStringOfAnnotated,
    SetAnnotatedFromAny, ANNOT_LIST_PROCS
};

const Tcl_ObjType *const TclAnnotatedObjTypePtr = &annotatedObjType;

static void
FreeAnnotated(Tcl_Obj *objPtr)
{
    Tcl_DecrRefCount((Tcl_Obj *) objPtr->internalRep.twoPtrValue.ptr1);
    Tcl_DecrRefCount((Tcl_Obj *) objPtr->internalRep.twoPtrValue.ptr2);
}

/*
 * V est dupliqué : Tcl_IsShared(V) ignore le partage de A, donc deux A
 * ne doivent jamais partager le même V (voir spécification §6). T est
 * partagé, car privé et immuable.
 */
static void
DupAnnotated(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
    Tcl_ObjInternalRep ir;
    Tcl_Obj *vPtr = Tcl_DuplicateObj(
	    (Tcl_Obj *) srcPtr->internalRep.twoPtrValue.ptr1);
    Tcl_Obj *tPtr = (Tcl_Obj *) srcPtr->internalRep.twoPtrValue.ptr2;

    Tcl_IncrRefCount(vPtr);
    Tcl_IncrRefCount(tPtr);
    ir.twoPtrValue.ptr1 = vPtr;
    ir.twoPtrValue.ptr2 = tPtr;
    Tcl_StoreInternalRep(dupPtr, &annotatedObjType, &ir);
}

/* L'annotation est une MÉTA-DONNÉE : chaîne de A = chaîne de V. */
static void
UpdateStringOfAnnotated(Tcl_Obj *objPtr)
{
    Tcl_Obj *vPtr = (Tcl_Obj *) objPtr->internalRep.twoPtrValue.ptr1;
    Tcl_Size len;
    const char *bytes = Tcl_GetStringFromObj(vPtr, &len);

    Tcl_InitStringRep(objPtr, bytes, len);
}

/* Une chaîne ne devient jamais une annotation par shimmering. */
static int
SetAnnotatedFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"annotated values can only be created by the parser or by "
		"typed assignments", TCL_AUTO_LENGTH));
	Tcl_SetErrorCode(interp, "TCL", "VALUE", "ANNOTATED", (char *) NULL);
    }
    return TCL_ERROR;
}

#ifdef TCL_OBJTYPE_V2
/*
 * Renvoie le contenu V et 1 si sa vue est une liste, 0 s'il est un élément
 * unique. En cas de contenu brut invalide, le contenu est vu comme scalaire.
 */
static int
AnnotListView(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Obj **contentPtr)
{
    int status, variant = -1;
    TclType *tp = NULL;
    Tcl_Obj *content;
    TclTypeKind kind;

    if (TclAnnotResolve(NULL, aPtr, NULL, &status, &tp, &variant,
	    &content) != TCL_OK) {
	*contentPtr = (Tcl_Obj *) aPtr->internalRep.twoPtrValue.ptr1;
	return 0;
    }
    *contentPtr = content;
    if (status == TCLA_PLAIN || status == TCLA_EMPTY) {
	return 1;			/* sans type : liste ordinaire de Tcl */
    }
    if (status == TCLA_RAW) {
	variant = -1;
	if (TclTypeValidate(NULL, tp, content, -1, &variant) != TCL_OK) {
	    return 0;
	}
    }
    kind = EffectiveKind(tp, variant, content);
    return kind == TCLT_LIST || kind == TCLT_DICT || kind == TCLT_PRODUCT;
}

static Tcl_Size
AnnotListLength(Tcl_Obj *aPtr)
{
    Tcl_Obj *v;
    Tcl_Size n;

    if (!AnnotListView(NULL, aPtr, &v)) {
	return 1;
    }
    return (Tcl_ListObjLength(NULL, v, &n) == TCL_OK) ? n : 1;
}

static int
AnnotListIndex(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Size index,
	Tcl_Obj **elemPtr)
{
    Tcl_Obj *v;

    if (AnnotListView(interp, aPtr, &v)) {
	return Tcl_ListObjIndex(interp, v, index, elemPtr);
    }
    *elemPtr = (index == 0) ? v : NULL;
    return TCL_OK;
}

static int
AnnotListGetElements(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Size *objcPtr,
	Tcl_Obj ***objvPtr)
{
    Tcl_Obj *v;

    if (AnnotListView(interp, aPtr, &v)) {
	return Tcl_ListObjGetElements(interp, v, objcPtr, objvPtr);
    }
    /* Un seul élément : ptr1 de la représentation est justement un Tcl_Obj*
     * qui vit autant que A. */
    *objcPtr = 1;
    *objvPtr = (Tcl_Obj **) &aPtr->internalRep.twoPtrValue.ptr1;
    return TCL_OK;
}

static int
AnnotListSlice(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Size from, Tcl_Size to,
	Tcl_Obj **newPtr)
{
    Tcl_Obj **elv;
    Tcl_Size n;

    if (AnnotListGetElements(interp, aPtr, &n, &elv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (from < 0) {
	from = 0;
    }
    if (to >= n) {
	to = n - 1;
    }
    *newPtr = (from > to) ? Tcl_NewObj()
	    : Tcl_NewListObj(to - from + 1, elv + from);
    return TCL_OK;
}

static int
AnnotListReverse(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Obj **newPtr)
{
    Tcl_Obj **elv, *l;
    Tcl_Size n, i;

    if (AnnotListGetElements(interp, aPtr, &n, &elv) != TCL_OK) {
	return TCL_ERROR;
    }
    l = Tcl_NewObj();
    for (i = n; i > 0; i--) {
	Tcl_ListObjAppendElement(NULL, l, elv[i - 1]);
    }
    *newPtr = l;
    return TCL_OK;
}

/*
 * Met à jour A avec le nouveau contenu W (objet neuf, refcount 0 ou plus) :
 * revalidation contre le type, nouvelle variante pour une somme, V et T
 * remplacés dans la représentation interne, chaîne invalidée. A inchangé
 * en cas d'erreur.
 */
int
TclAnnotSetContent(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Obj *wPtr)
{
    int status, variant = -1;
    TclType *tp = NULL;
    Tcl_Obj *content, *nPtr, *oldV, *oldT, *newV, *newT;

    if (TclAnnotResolve(interp, aPtr, NULL, &status, &tp, &variant,
	    &content) != TCL_OK) {
	return TCL_ERROR;
    }
    if (status == TCLA_TYPED || status == TCLA_RAW) {
	variant = -1;
	if (TclTypeValidate(interp, tp, wPtr, -1, &variant) != TCL_OK) {
	    return TCL_ERROR;
	}
	nPtr = TclNewAnnotated(interp, tp, variant, wPtr);
    } else {
	nPtr = MakeAnnotated(Tcl_DuplicateObj(wPtr),
		(Tcl_Obj *) aPtr->internalRep.twoPtrValue.ptr2);
    }
    newV = (Tcl_Obj *) nPtr->internalRep.twoPtrValue.ptr1;
    newT = (Tcl_Obj *) nPtr->internalRep.twoPtrValue.ptr2;
    oldV = (Tcl_Obj *) aPtr->internalRep.twoPtrValue.ptr1;
    oldT = (Tcl_Obj *) aPtr->internalRep.twoPtrValue.ptr2;
    Tcl_IncrRefCount(newV);
    Tcl_IncrRefCount(newT);
    aPtr->internalRep.twoPtrValue.ptr1 = newV;
    aPtr->internalRep.twoPtrValue.ptr2 = newT;
    Tcl_DecrRefCount(oldV);
    Tcl_DecrRefCount(oldT);
    TclInvalidateStringRep(aPtr);
    Tcl_BounceRefCount(nPtr);
    return TCL_OK;
}

/* Copie privée de la vue liste de A (un scalaire devient {V}). */
static Tcl_Obj *
AnnotListCopy(Tcl_Interp *interp, Tcl_Obj *aPtr)
{
    Tcl_Obj *v;

    if (AnnotListView(interp, aPtr, &v)) {
	return Tcl_DuplicateObj(v);
    }
    return Tcl_NewListObj(1, &v);
}

static int
AnnotListReplace(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Size first,
	Tcl_Size numToDelete, Tcl_Size numToInsert, Tcl_Obj *const insertObjs[])
{
    Tcl_Obj *w = AnnotListCopy(interp, aPtr);
    int rc;

    Tcl_IncrRefCount(w);
    rc = Tcl_ListObjReplace(interp, w, first, numToDelete, numToInsert,
	    insertObjs);
    if (rc == TCL_OK) {
	rc = TclAnnotSetContent(interp, aPtr, w);
    }
    Tcl_DecrRefCount(w);
    return rc;
}

/* Convention de TclLsetFlat (INST_LSET_FLAT et [lset] l'attendent) : A
 * lui-même s'il n'est pas partagé, sinon une copie, avec UNE référence
 * comptée pour l'appelant ; NULL en cas d'erreur. */
static Tcl_Obj *
AnnotListSetElement(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Size indexCount,
	Tcl_Obj *const indexArray[], Tcl_Obj *valueObj)
{
    Tcl_Obj *w = AnnotListCopy(interp, aPtr), *r, *target;
    int rc;

    Tcl_IncrRefCount(w);
    r = TclLsetFlat(interp, w, indexCount, indexArray, valueObj);
    if (r == NULL) {
	Tcl_DecrRefCount(w);
	return NULL;
    }
    target = Tcl_IsShared(aPtr) ? Tcl_DuplicateObj(aPtr) : aPtr;
    rc = TclAnnotSetContent(interp, target, r);
    Tcl_DecrRefCount(r);			/* la référence de TclLsetFlat */
    Tcl_DecrRefCount(w);
    if (rc != TCL_OK) {
	if (target != aPtr) {
	    Tcl_BounceRefCount(target);
	}
	return NULL;
    }
    Tcl_IncrRefCount(target);		/* comme TclLsetFlat : une référence */
    return target;
}

static int
AnnotListIn(Tcl_Interp *interp, Tcl_Obj *valuePtr, Tcl_Obj *aPtr, int *resPtr)
{
    Tcl_Obj **elv;
    Tcl_Size n, i, l1;
    const char *s1 = Tcl_GetStringFromObj(valuePtr, &l1);

    if (AnnotListGetElements(interp, aPtr, &n, &elv) != TCL_OK) {
	return TCL_ERROR;
    }
    *resPtr = 0;
    for (i = 0; i < n && !*resPtr; i++) {
	Tcl_Size l2;
	const char *s2 = Tcl_GetStringFromObj(elv[i], &l2);

	*resPtr = (l1 == l2 && memcmp(s1, s2, l1) == 0);
    }
    return TCL_OK;
}
#endif /* TCL_OBJTYPE_V2 */

static Tcl_Obj *
MakeAnnotated(Tcl_Obj *vPtr, Tcl_Obj *tPtr)
{
    Tcl_Obj *aPtr = Tcl_NewObj();
    Tcl_ObjInternalRep ir;

    Tcl_IncrRefCount(vPtr);
    Tcl_IncrRefCount(tPtr);
    ir.twoPtrValue.ptr1 = vPtr;
    ir.twoPtrValue.ptr2 = tPtr;
    Tcl_StoreInternalRep(aPtr, &annotatedObjType, &ir);
    TclInvalidateStringRep(aPtr);
    return aPtr;
}

/*
 * Valeur typée : V = copie privée du contenu (conserve sa représentation
 * interne), T résolu. variant = -1 sauf pour une somme.
 */
Tcl_Obj *
TclNewAnnotated(Tcl_Interp *interp, TclType *tp, int variant,
	Tcl_Obj *content)
{
    return MakeAnnotated(Tcl_DuplicateObj(content),
	    TypeRefObj(interp, tp, variant));
}

/* Annotation du parseur : T brut = copie privée du texte du type. */
Tcl_Obj *
TclNewRawAnnotation(Tcl_Obj *typeText, Tcl_Obj *content)
{
    Tcl_Size len;
    const char *text = Tcl_GetStringFromObj(typeText, &len);

    return MakeAnnotated(Tcl_DuplicateObj(content),
	    Tcl_NewStringObj(text, len));
}

int
TclIsAnnotated(Tcl_Obj *objPtr)
{
    return Tcl_FetchInternalRep(objPtr, &annotatedObjType) != NULL;
}

/* 0 : pas annoté ; 1 : typé (tpPtr, variantPtr remplis) ; 2 : brut. */
int
TclAnnotInfo(Tcl_Obj *objPtr, Tcl_Obj **contentPtr, TclType **tpPtr,
	int *variantPtr, Tcl_Obj **rawTypePtr)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(objPtr, &annotatedObjType);
    Tcl_ObjInternalRep *tir;
    Tcl_Obj *vPtr, *tPtr;

    if (ir == NULL) {
	return 0;
    }
    vPtr = (Tcl_Obj *) ir->twoPtrValue.ptr1;
    tPtr = (Tcl_Obj *) ir->twoPtrValue.ptr2;
    if (contentPtr != NULL) {
	*contentPtr = vPtr;
    }
    tir = Tcl_FetchInternalRep(tPtr, &typeRefObjType);
    if (tir != NULL) {
	if (tpPtr != NULL) {
	    *tpPtr = (TclType *) tir->twoPtrValue.ptr1;
	}
	if (variantPtr != NULL) {
	    *variantPtr = (int) (intptr_t) tir->twoPtrValue.ptr2 - 1;
	}
	return 1;
    }
    if (rawTypePtr != NULL) {
	*rawTypePtr = tPtr;
    }
    return 2;
}

/*
 * Résout un argument. Quand la cible est une somme (ctx), une annotation
 * brute {nom} est d'abord lue comme un NOM DE VARIANTE (spéc. §12 : {d}5),
 * puis comme un type. Le contenu d'une annotation brute n'est pas validé.
 */
int
TclAnnotResolve(Tcl_Interp *interp, Tcl_Obj *objPtr, const TclType *ctx,
	int *statusPtr, TclType **tpPtr, int *variantPtr, Tcl_Obj **contentPtr)
{
    Tcl_Obj *vPtr, *rawPtr = NULL;
    TclType *tp = NULL;
    int variant = -1, kind;
    const char *text;
    Tcl_Size len, idx;

    *tpPtr = NULL;
    *variantPtr = -1;
    kind = TclAnnotInfo(objPtr, &vPtr, &tp, &variant, &rawPtr);
    if (kind == 0) {
	*statusPtr = TCLA_PLAIN;
	*contentPtr = objPtr;
	return TCL_OK;
    }
    *contentPtr = vPtr;
    if (kind == 1) {
	*statusPtr = TCLA_TYPED;
	*tpPtr = tp;
	*variantPtr = variant;
	return TCL_OK;
    }
    text = Tcl_GetStringFromObj(rawPtr, &len);
    if (len == 0) {
	*statusPtr = TCLA_EMPTY;		/* le type vide {} */
	return TCL_OK;
    }
    if (ctx != NULL && ctx->kind == TCLT_SUM) {
	idx = TclTypeFieldIndex(ctx, text);
	if (idx >= 0 && ctx->fields[idx].type != NULL) {
	    *statusPtr = TCLA_RAW;
	    *tpPtr = (TclType *) ctx;
	    *variantPtr = (int) idx;
	    return TCL_OK;
	}
    }
    tp = TclTypeParse(interp, text, len);
    if (tp == NULL) {
	return TCL_ERROR;
    }
    *statusPtr = TCLA_RAW;
    *tpPtr = tp;
    return TCL_OK;
}

/* --------------------------------------------------------------------
 * Déballage strict, appelé par les fonctions de conversion du noyau
 * -------------------------------------------------------------------- */

static int
KindAllows(TclTypeKind kind, int want)
{
    switch (want) {
    case TCLWANT_STRING:	return 1;
    case TCLWANT_NUMBER:	return kind == TCLT_INT || kind == TCLT_DOUBLE;
    case TCLWANT_INT:		return kind == TCLT_INT;
    case TCLWANT_DOUBLE:	/* élargissement sans shimmering */
	return kind == TCLT_INT || kind == TCLT_DOUBLE;
    case TCLWANT_BOOL:		return kind == TCLT_INT || kind == TCLT_DOUBLE
				|| kind == TCLT_STRING;
    case TCLWANT_LIST:		return kind == TCLT_LIST;
    case TCLWANT_DICT:		return kind == TCLT_DICT;
    default:			return 0;
    }
}

static const char *const wantNames[] = {
    "number", "integer", "floating-point number", "boolean", "list", "dict",
    "string"
};

/*
 * Genre EFFECTIF d'un type pour les conversions du noyau.
 *
 * Une somme n'est pas un genre de valeur : c'est la variante COURANTE qui
 * décide de ce qu'on peut faire du contenu. Un "num" défini par
 * "double, int" contenant 5 se comporte comme un int, et contenant 5.5
 * comme un double. On descend donc dans la variante, et, si elle est
 * elle-même une somme, on cherche la sous-variante en validant le contenu.
 *
 *   - variante typée      : le genre de son type ;
 *   - variante littérale  : une énumération est une chaîne ;
 *   - variante inconnue   : on rend TCLT_SUM, qui n'autorise rien.
 *
 * Le contenu est déjà valide : la recherche de sous-variante ne peut pas
 * échouer, et se fait sans message (interp == NULL).
 */

static TclTypeKind
EffectiveKind(const TclType *tp, int variant, Tcl_Obj *content)
{
    int depth;

    for (depth = 0; tp->kind == TCLT_SUM && depth < 16; depth++) {
	const TclTypeField *f;

	if (variant < 0 || variant >= tp->nfields) {
	    return TCLT_SUM;
	}
	f = &tp->fields[variant];
	if (f->type == NULL) {
	    return TCLT_STRING;			/* littéral d'énumération */
	}
	tp = f->type;
	variant = -1;
	if (tp->kind == TCLT_SUM && TclTypeValidate(NULL, tp, content, -1,
		&variant) != TCL_OK) {
	    return TCLT_SUM;
	}
    }
    return tp->kind;
}

/*
 * À appeler en tête de chaque fonction de conversion du noyau (nombres,
 * booléens, listes, dicts, chaînes Unicode, octets) :
 *
 *	if (TclAnnotUnwrap(interp, objPtr, TCLWANT_INT, &objPtr) != TCL_OK) {
 *	    return TCL_ERROR;
 *	}
 *
 * Contenu du type demandé : on le renvoie. Autre type : erreur.
 * Pour une somme, le type demandé est comparé à celui de la variante
 * courante (voir EffectiveKind).
 * Vue chaîne : toujours permise, sans toucher à la représentation.
 * Un objet non annoté est renvoyé tel quel.
 */
int
TclAnnotUnwrap(Tcl_Interp *interp, Tcl_Obj *objPtr, int want,
	Tcl_Obj **contentPtr)
{
    int status, variant;
    TclType *tp;
    Tcl_Obj *content;

    if (TclAnnotResolve(interp, objPtr, NULL, &status, &tp, &variant,
	    &content) != TCL_OK) {
	return TCL_ERROR;
    }
    *contentPtr = content;
    if (status == TCLA_PLAIN || status == TCLA_EMPTY
	    || want == TCLWANT_STRING) {
	return TCL_OK;
    }
    if (status == TCLA_RAW) {
	/* On valide, et pour une somme la validation désigne la variante. */

	variant = -1;
	if (TclTypeValidate(interp, tp, content, -1, &variant) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (!KindAllows(EffectiveKind(tp, variant, content), want)) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "can't use %s value as %s", tp->name, wantNames[want]));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "STRICT", (char *) NULL);
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}


/* --------------------------------------------------------------------
 * Annotation à l'exécution (mot {T}$x, {T}[cmd], {T}"...$x...")
 * --------------------------------------------------------------------
 *
 * Appelée par INST_ANNOTATE et par TclSubstTokens, quand le suffixe d'un
 * mot annoté contient une substitution : la valeur n'est connue qu'ici.
 *
 *   {}$x        type vide : le type est retiré (annotation brute vide).
 *   {T}$typed   T se résout : cast explicite de la valeur typée (§7).
 *   {T}$plain   T se résout : la valeur est validée et typée tout de
 *               suite.
 *   {nom}...    T ne se résout pas (nom de variante d'une somme, ou type
 *               pas encore défini) : annotation BRUTE, résolue à la
 *               consommation, dans le contexte de la cible.
 *
 * Une annotation littérale ({int}5) ne passe pas par ici : elle est
 * fabriquée à la compilation, toujours brute (les types peuvent ne pas
 * exister encore).
 */
int
TclAnnotateValue(Tcl_Interp *interp, Tcl_Obj *typeText, Tcl_Obj *value,
	Tcl_Obj **resultPtr)
{
    Tcl_Size len;
    const char *text = Tcl_GetStringFromObj(typeText, &len);
    TclType *tp, *vtp = NULL;
    int kind, vvar = -1, variant = -1;
    Tcl_Obj *vcontent = NULL, *copy;

    kind = TclAnnotInfo(value, &vcontent, &vtp, &vvar, NULL);
    if (kind == 0) {
	vcontent = value;
    }
    if (len == 0) {				/* {}$x : retire le type */
	*resultPtr = TclNewRawAnnotation(typeText, vcontent);
	return TCL_OK;
    }
    tp = TclTypeParse(interp, text, len);
    if (tp == NULL) {				/* nom de variante, type futur */
	Tcl_ResetResult(interp);
	*resultPtr = TclNewRawAnnotation(typeText, vcontent);
	return TCL_OK;
    }
    if (kind == 1) {				/* valeur typée : cast */
	if (TclTypeCast(interp, value, tp, resultPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (*resultPtr == value) {
	    /* Même type : INST_ANNOTATE dépile la valeur avant de pousser le
	     * résultat, qui ne doit donc pas être cet objet. */
	    *resultPtr = Tcl_DuplicateObj(value);
	}
	return TCL_OK;
    }
    copy = Tcl_DuplicateObj(vcontent);
    if (TclTypeValidate(interp, tp, copy, -1, &variant) != TCL_OK) {
	TclDiscard(copy);
	return TCL_ERROR;
    }
    *resultPtr = TclNewAnnotated(interp, tp, variant, copy);
    TclDiscard(copy);
    return TCL_OK;
}

/* --------------------------------------------------------------------
 * Intégration au noyau : mots annotés {type}mot
 * -------------------------------------------------------------------- */

/* Un des mots de la commande analysée est-il annoté ({type}mot) ? Sert à
 * TclCompileSetCmd, qui laisse alors le travail à l'exécution. */
int
TclHasAnnotatedWord(Tcl_Parse *parsePtr)
{
    Tcl_Token *tokenPtr = parsePtr->tokenPtr;
    Tcl_Size i;

    for (i = 0; i < parsePtr->numWords; i++) {
	if (tokenPtr->type == TCL_TOKEN_WORD && tokenPtr->numComponents > 0
		&& tokenPtr[1].type == TCL_TOKEN_ANNOT) {
	    return 1;
	}
	tokenPtr += tokenPtr->numComponents + 1;
    }
    return 0;
}

/* ::tcl::annotate type value : l'équivalent exécutable de {type}valeur,
 * appelé par le code compilé et par TclSubstTokens. */
static int
AnnotateObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    Tcl_Obj *res = NULL;

    (void) clientData;
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "type value");
	return TCL_ERROR;
    }
    if (TclAnnotateValue(interp, objv[1], objv[2], &res) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, res);
    return TCL_OK;
}

/* ::tcl::annotraw type value : annotation BRUTE d'un suffixe littéral
 * ({int}5, {int}x) : jamais validée ici, le type peut ne pas exister
 * encore ; elle est résolue à la consommation. */
static int
AnnotRawObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    (void) clientData;
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "type value");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, TclNewRawAnnotation(objv[1], objv[2]));
    return TCL_OK;
}

void
TclAnnotInit(Tcl_Interp *interp)
{
    Tcl_CreateObjCommand2(interp, "::tcl::annotate", AnnotateObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::annotraw", AnnotRawObjCmd, NULL,
	    NULL);
}

/* Le suffixe d'un mot annoté est-il littéral (que du texte) ? */
int
TclAnnotSuffixIsLiteral(Tcl_Token *tokenPtr, Tcl_Size count)
{
    Tcl_Size i;

    for (i = 0; i < count; i++) {
	if (tokenPtr[i].type != TCL_TOKEN_TEXT
		&& tokenPtr[i].type != TCL_TOKEN_BS) {
	    return 0;
	}
    }
    return 1;
}
