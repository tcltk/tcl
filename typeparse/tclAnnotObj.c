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
    (void)objPtr;
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

    if (interp != NULL && ((Interp *) interp)->typeRefsData == rd) {
	((Interp *) interp)->typeRefsData = NULL;
    }
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
    RefData *rd = (RefData *) ((Interp *) interp)->typeRefsData;

    if (rd == NULL) {
	rd = (RefData *) Tcl_Alloc(sizeof(RefData));
	Tcl_InitHashTable(&rd->refs, TCL_STRING_KEYS);
	Tcl_SetAssocData(interp, "tclTypeRefs", DeleteRefData, rd);
	((Interp *) interp)->typeRefsData = rd;
    }
    return rd;
}

static Tcl_Obj *
TypeRefObj(Tcl_Interp *interp, TclType *tp, int variant)
{
    RefData *rd;
    char key[64];
    int isNew, slot = variant + 1;
    Tcl_HashEntry *hPtr;
    Tcl_Obj *tPtr;
    Tcl_ObjInternalRep ir;

    if (slot >= 0 && slot < 8 && tp->refCache[slot] != NULL) {
	return tp->refCache[slot];	/* le cas courant : sans hachage */
    }
    rd = GetRefData(interp);
    snprintf(key, sizeof(key), "%p/%d", (void *) tp, variant);
    hPtr = Tcl_CreateHashEntry(&rd->refs, key, &isNew);
    if (!isNew) {
	tPtr = (Tcl_Obj *) Tcl_GetHashValue(hPtr);
	if (slot >= 0 && slot < 8) {
	    tp->refCache[slot] = tPtr;
	}
	return tPtr;
    }
    tPtr = Tcl_NewObj();
    ir.twoPtrValue.ptr1 = tp;
    ir.twoPtrValue.ptr2 = (void *) (intptr_t) (variant + 1);
    Tcl_StoreInternalRep(tPtr, &typeRefObjType, &ir);
    TclInvalidateStringRep(tPtr);	/* Tcl_NewObj() donne "" : à invalider */
    Tcl_IncrRefCount(tPtr);
    Tcl_SetHashValue(hPtr, tPtr);
    if (slot >= 0 && slot < 8) {
	tp->refCache[slot] = tPtr;
    }
    return tPtr;
}

/* --------------------------------------------------------------------
 * Type d'objet "annotated"
 * -------------------------------------------------------------------- */

static Tcl_FreeInternalRepProc	FreeAnnotated;
static void FreeAnnotatedReps(Tcl_Obj *objPtr);
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

/* --------------------------------------------------------------------
 * État {T, N} : le T résolu et les NOTES (nominaux en attente)
 * --------------------------------------------------------------------
 *
 * ptr2 d'une valeur annotée est soit T seul (typeref, ou texte brut du
 * parseur) quand il n'y a pas de note, soit un objet "annotstate" IMMUABLE
 * et partagé {T, N} : T peut être NULL (note seule, « @mm »), N est une
 * liste Tcl de noms, de la plus ancienne à la plus récente.
 */

static int RawIsLocal(Tcl_Interp *interp);
enum { ANN_TYPE, ANN_VARIANT, ANN_UNKNOWN, ANN_LOCAL, ANN_BAD };
static int LastErrorCodeIs(Tcl_Interp *interp, const char *what);
static int ClassifyAnnot(Tcl_Interp *interp, Tcl_Obj *textObj,
	const TclType *ctx, TclType **tpPtr, int *variantPtr);
static Tcl_FreeInternalRepProc	FreeAnnotState;
static Tcl_DupInternalRepProc	DupAnnotState;
static Tcl_UpdateStringProc	UpdateStringOfAnnotState;
static Tcl_SetFromAnyProc	SetAnnotStateFromAny;

static const Tcl_ObjType annotStateObjType = {
    "annotstate", FreeAnnotState, DupAnnotState, UpdateStringOfAnnotState,
    SetAnnotStateFromAny, TCL_OBJTYPE_V0
};

static void
FreeAnnotState(Tcl_Obj *objPtr)
{
    Tcl_Obj *t = (Tcl_Obj *) objPtr->internalRep.twoPtrValue.ptr1;
    Tcl_Obj *n = (Tcl_Obj *) objPtr->internalRep.twoPtrValue.ptr2;

    if (t != NULL) {
	Tcl_DecrRefCount(t);
    }
    if (n != NULL) {
	Tcl_DecrRefCount(n);
    }
}

static void
DupAnnotState(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
    Tcl_ObjInternalRep ir;

    ir.twoPtrValue.ptr1 = srcPtr->internalRep.twoPtrValue.ptr1;
    ir.twoPtrValue.ptr2 = srcPtr->internalRep.twoPtrValue.ptr2;
    if (ir.twoPtrValue.ptr1 != NULL) {
	Tcl_IncrRefCount((Tcl_Obj *) ir.twoPtrValue.ptr1);
    }
    if (ir.twoPtrValue.ptr2 != NULL) {
	Tcl_IncrRefCount((Tcl_Obj *) ir.twoPtrValue.ptr2);
    }
    Tcl_StoreInternalRep(dupPtr, &annotStateObjType, &ir);
}

/* Notes jointes par « . », de la plus ancienne à la plus récente. */
static Tcl_Obj *
NotesJoined(Tcl_Obj *notes)
{
    Tcl_Obj **elv;
    Tcl_Size n, i;
    Tcl_Obj *res = Tcl_NewObj();

    if (notes != NULL && Tcl_ListObjGetElements(NULL, notes, &n, &elv)
	    == TCL_OK) {
	for (i = 0; i < n; i++) {
	    if (i > 0) {
		Tcl_AppendToObj(res, ".", 1);
	    }
	    Tcl_AppendObjToObj(res, elv[i]);
	}
    }
    return res;
}

static void
UpdateStringOfAnnotState(Tcl_Obj *objPtr)
{
    Tcl_Obj *j = NotesJoined((Tcl_Obj *) objPtr->internalRep.twoPtrValue.ptr2);
    Tcl_Size len;
    const char *b;

    Tcl_IncrRefCount(j);
    b = Tcl_GetStringFromObj(j, &len);
    Tcl_InitStringRep(objPtr, "@", 1);
    Tcl_AppendToObj(objPtr, b, len);
    Tcl_DecrRefCount(j);
}

static int
SetAnnotStateFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    (void) objPtr;
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"annotation states cannot be created from strings", -1));
    }
    return TCL_ERROR;
}

static int
IsTypeRef(Tcl_Obj *tPtr)
{
    return tPtr != NULL && Tcl_FetchInternalRep(tPtr, &typeRefObjType) != NULL;
}

/* ptr2 -> (T, N) ; N reste NULL sans note. */
static void
SplitT(Tcl_Obj *p2, Tcl_Obj **tPtrPtr, Tcl_Obj **nPtrPtr)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(p2, &annotStateObjType);

    if (ir != NULL) {
	*tPtrPtr = (Tcl_Obj *) ir->twoPtrValue.ptr1;
	*nPtrPtr = (Tcl_Obj *) ir->twoPtrValue.ptr2;
    } else {
	*tPtrPtr = p2;
	*nPtrPtr = NULL;
    }
}

/* Construit le ptr2 pour (T, N) : T seul sans note ; NULL si rien du tout.
 * Le résultat n'a pas de référence propre (l'appelant l'attache). */
static Tcl_Obj *
StateObj(Tcl_Obj *tPtr, Tcl_Obj *nPtr)
{
    Tcl_Size n = 0;
    Tcl_Obj *st;
    Tcl_ObjInternalRep ir;

    if (nPtr != NULL) {
	Tcl_ListObjLength(NULL, nPtr, &n);
    }
    if (n == 0) {
	return tPtr;
    }
    st = Tcl_NewObj();
    if (tPtr != NULL) {
	Tcl_IncrRefCount(tPtr);
    }
    Tcl_IncrRefCount(nPtr);
    ir.twoPtrValue.ptr1 = tPtr;
    ir.twoPtrValue.ptr2 = nPtr;
    Tcl_StoreInternalRep(st, &annotStateObjType, &ir);
    TclInvalidateStringRep(st);
    return st;
}

/* Les notes d'une valeur (liste, non possédée), ou NULL. */
Tcl_Obj *
TclAnnotNotes(Tcl_Obj *objPtr)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(objPtr, &annotatedObjType);
    Tcl_Obj *t, *n;

    if (ir == NULL) {
	return NULL;
    }
    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
    return n;
}

/* Valeur A' = A avec le nom ajouté comme note la plus récente. Le type
 * résolu est gardé ; un texte brut non vide (annotation littérale) devient
 * la plus ancienne des notes. */
static Tcl_Obj *
AnnotAddNote(Tcl_Obj *src, Tcl_Obj *nameObj)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(src, &annotatedObjType);
    Tcl_Obj *content = src, *t = NULL, *n = NULL, *newN, *raw = NULL, *p2;
    Tcl_Obj *res;
    Tcl_Size len;

    if (ir != NULL) {
	content = (Tcl_Obj *) ir->twoPtrValue.ptr1;
	SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
	if (t != NULL && !IsTypeRef(t)) {
	    Tcl_GetStringFromObj(t, &len);
	    if (len > 0) {
		raw = t;
		t = NULL;
	    } else {
		/* Marque {} posée hors constructeur : le type ET la pile ont
		 * été effacés ; on annote le contenu nu. */
		t = NULL;
		n = NULL;
	    }
	}
    }
    newN = (n != NULL) ? Tcl_DuplicateObj(n) : Tcl_NewListObj(0, NULL);
    if (raw != NULL) {
	Tcl_ListObjAppendElement(NULL, newN, Tcl_DuplicateObj(raw));
    }
    if (nameObj != NULL) {
	Tcl_ListObjAppendElement(NULL, newN, Tcl_DuplicateObj(nameObj));
    }
    p2 = StateObj(t, newN);
    res = MakeAnnotated(Tcl_DuplicateObj(content), p2);
    return res;
}

/* A' = A avec T remplacé par le type vide {} (les notes restent). */
static Tcl_Obj *
AnnotClearType(Tcl_Obj *src)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(src, &annotatedObjType);
    Tcl_Obj *content = src, *t, *n = NULL;

    if (ir != NULL) {
	content = (Tcl_Obj *) ir->twoPtrValue.ptr1;
	SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
    }
    return MakeAnnotated(Tcl_DuplicateObj(content), StateObj(
	    Tcl_NewStringObj("", 0), n));
}

/* res (fraîche) + les notes d'une autre valeur. */
static Tcl_Obj *
AnnotWithNotes(Tcl_Obj *res, Tcl_Obj *notes)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(res, &annotatedObjType);
    Tcl_Obj *t, *n;

    if (ir == NULL || notes == NULL) {
	return res;
    }
    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
    if (n != NULL) {
	return res;
    }
    return MakeAnnotated(Tcl_DuplicateObj(
	    (Tcl_Obj *) ir->twoPtrValue.ptr1), StateObj(t, notes));
}

/*
 * Une annotation littérale {mm}5 dont le nom ne se résout pas (ni type, ni
 * caractère local) est une NOTE : on la range dans N. Rend A lui-même si
 * rien ne change, sinon une valeur neuve (sans référence).
 */
Tcl_Obj *
TclAnnotNormalize(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(objPtr, &annotatedObjType);
    Tcl_Obj *t, *n, *saved, *res = objPtr;
    Tcl_Size len;
    const char *text;
    int pending;

    if (ir == NULL) {
	return objPtr;
    }
    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
    if (t == NULL || IsTypeRef(t)) {
	return objPtr;
    }
    text = Tcl_GetStringFromObj(t, &len);
    if (len == 0) {
	return objPtr;
    }
    saved = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(saved);
    pending = (TclTypeParse(interp, text, len) == NULL)
	    && !RawIsLocal(interp);
    Tcl_SetObjResult(interp, saved);
    Tcl_DecrRefCount(saved);
    if (pending) {
	res = AnnotAddNote(objPtr, NULL);
    }
    return res;
}


/*
 * Un scalaire annoté (string, int, un élément atomique...) est une liste
 * d'UN élément, qui est lui-même : foreach f $e rend e, annotation
 * comprise, et l'atomicité se conserve à tous les niveaux d'imbrication.
 * getElementsProc doit rendre un tableau qui vive autant que l'objet : une
 * case (Tcl_Obj *) par objet, dans une table propre au thread, libérée
 * avec l'objet.
 */

typedef struct {
    int init;
    Tcl_HashTable cells;		/* Tcl_Obj* -> Tcl_Obj ** (1 case) */
} SelfCells;

static Tcl_ThreadDataKey selfCellsKey;
static int selfCellsEver = 0;	/* une case a-t-elle déjà été créée ? (sinon,
				 * la libération ne cherche rien) */

static SelfCells *
GetSelfCells(void)
{
    SelfCells *sc = (SelfCells *) Tcl_GetThreadData(&selfCellsKey,
	    sizeof(SelfCells));

    if (!sc->init) {
	Tcl_InitHashTable(&sc->cells, TCL_ONE_WORD_KEYS);
	sc->init = 1;
    }
    return sc;
}

static Tcl_Obj **
SelfCell(Tcl_Obj *aPtr)
{
    SelfCells *sc = GetSelfCells();
    int isNew;
    Tcl_HashEntry *hPtr = Tcl_CreateHashEntry(&sc->cells, aPtr, &isNew);
    Tcl_Obj **cell;

    if (isNew) {
	selfCellsEver = 1;
	cell = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *));
	*cell = aPtr;			/* pas de référence : aPtr survit */
	Tcl_SetHashValue(hPtr, cell);
    }
    return (Tcl_Obj **) Tcl_GetHashValue(hPtr);
}

static void
FreeAnnotated(Tcl_Obj *objPtr)
{
    SelfCells *sc;

    if (!selfCellsEver) {
	FreeAnnotatedReps(objPtr);
	return;
    }
    sc = (SelfCells *) Tcl_GetThreadData(&selfCellsKey, sizeof(SelfCells));
    if (sc->init && sc->cells.numEntries > 0) {
	Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&sc->cells, objPtr);

	if (hPtr != NULL) {
	    Tcl_Free(Tcl_GetHashValue(hPtr));
	    Tcl_DeleteHashEntry(hPtr);
	}
    }
    FreeAnnotatedReps(objPtr);
}

static void
FreeAnnotatedReps(Tcl_Obj *objPtr)
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
    Tcl_Obj *vPtr, *tPtr = (Tcl_Obj *) srcPtr->internalRep.twoPtrValue.ptr2;

    vPtr = Tcl_DuplicateObj((Tcl_Obj *) srcPtr->internalRep.twoPtrValue.ptr1);

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
    (void) objPtr;
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

    (void) interp;
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
    /* Règle B : seules les collections (list, dict) se déplient ; un
     * produit est un élément, ses champs se lisent par chemin. */
    return kind == TCLT_LIST || kind == TCLT_DICT;
}

/*
 * Éléments typés d'une liste annotée. Les éléments d'une liste de type
 * list.T sont stockés dans V sans annotation ; à la lecture par la vue
 * liste (lindex, foreach, lmap, lrange...) ils sont enveloppés, une fois
 * pour toutes, dans le type T des éléments (dans V même, à la place de
 * l'élément nu : la chaîne ne change pas). Un élément déjà annoté (balise
 * explicite {i}5 d'une somme) est laissé tel quel.
 */

static TclType *
AnnotListElemType(Tcl_Obj *aPtr)
{
    int status, variant = -1;
    TclType *tp = NULL, *lt;
    Tcl_Obj *content;

    if (TclAnnotResolve(NULL, aPtr, NULL, &status, &tp, &variant,
	    &content) != TCL_OK) {
	return NULL;
    }
    if (status == TCLA_PLAIN || status == TCLA_EMPTY) {
	return NULL;
    }
    if (status == TCLA_RAW) {
	variant = -1;
	if (TclTypeValidate(NULL, tp, content, -1, &variant) != TCL_OK) {
	    return NULL;
	}
    }
    lt = tp;
    if (tp->kind == TCLT_SUM) {
	if (variant < 0 || variant >= (int) tp->nfields
		|| tp->fields[variant].type == NULL) {
	    return NULL;
	}
	lt = tp->fields[variant].type;
    }
    if (lt->kind != TCLT_LIST || lt->elem == NULL
	    || (TclTypeIsPlainString(lt->elem) && strcmp(lt->name, "list") == 0)) {
	/* La liste nue {list} : ses éléments restent du texte Tcl ordinaire,
	 * qui peut être lui-même une liste ({a b {c d}}, lindex $L 2 1) ; pas
	 * d'enveloppe. Le type d'un tel élément se lit par $L(i). Avec
	 * list.string (ou tout autre type d'éléments), l'élément est atomique :
	 * une valeur annotée est UN SEUL élément pour les commandes de liste. */
	return NULL;
    }
    return lt->elem;
}

/* Comme TypeRefObj, sans l'interpréteur (les procédures de liste n'en ont
 * pas toujours) et sans la table : objet neuf, refcount 0. */
static Tcl_Obj *
NewTypeRefNoCache(TclType *tp, int variant)
{
    Tcl_Obj *tPtr;
    Tcl_ObjInternalRep ir;

    if (variant + 1 >= 0 && variant + 1 < 8 && tp->refCache[variant + 1]) {
	return tp->refCache[variant + 1];	/* partagé, déjà interné */
    }
    tPtr = Tcl_NewObj();

    ir.twoPtrValue.ptr1 = tp;
    ir.twoPtrValue.ptr2 = (void *) (intptr_t) (variant + 1);
    Tcl_StoreInternalRep(tPtr, &typeRefObjType, &ir);
    TclInvalidateStringRep(tPtr);
    return tPtr;
}

static void
AnnotWrapElems(Tcl_Obj *aPtr, Tcl_Obj *v, Tcl_Size from, Tcl_Size to)
{
    TclType *et = AnnotListElemType(aPtr);
    Tcl_Size n, i;
    Tcl_Obj *e, *w;
    int variant;

    if (et == NULL || Tcl_IsShared(v) || TclIsCompact(v)
	    || Tcl_ListObjLength(NULL, v, &n) != TCL_OK) {
	return;
    }
    if (from < 0) {
	from = 0;
    }
    if (to >= n) {
	to = n - 1;
    }
    for (i = from; i <= to; i++) {
	if (Tcl_ListObjIndex(NULL, v, i, &e) != TCL_OK || e == NULL
		|| TCL_ANNOT_P(e)) {
	    continue;
	}
	variant = -1;
	if (et->kind == TCLT_SUM
		&& TclTypeValidate(NULL, et, e, -1, &variant) != TCL_OK) {
	    continue;
	}
	w = MakeAnnotated(e, NewTypeRefNoCache(et, variant));	/* partagé */
	Tcl_ListObjReplace(NULL, v, i, 1, 1, &w);
    }
}

/*
 * Cas courant des procédures de liste : A typé (référence de type, sans
 * note) par un type liste dont les éléments sont typés. Rend le type liste
 * et V, ou NULL (cas général : AnnotListView, AnnotListElemType).
 */
static inline TclType *
FastListType(Tcl_Obj *aPtr, Tcl_Obj **vPtr)
{
    Tcl_Obj *t = (Tcl_Obj *) aPtr->internalRep.twoPtrValue.ptr2;
    TclType *lt;

    if (t->typePtr != &typeRefObjType) {
	return NULL;
    }
    lt = (TclType *) t->internalRep.twoPtrValue.ptr1;
    if (lt->kind != TCLT_LIST || lt->elem == NULL) {
	return NULL;
    }
    *vPtr = (Tcl_Obj *) aPtr->internalRep.twoPtrValue.ptr1;
    return lt;
}

/*
 * foreach, lmap : la liste à parcourir pour une liste typée A. Si TOUS les
 * éléments de V sont des valeurs typées (les éléments sont typés dans V,
 * une fois pour toutes, s'il n'est pas partagé), la boucle parcourt V comme
 * une liste ordinaire : chaque variable reçoit l'élément typé, sans passer
 * par les procédures de liste abstraite à chaque tour. Sinon NULL : la
 * boucle passe par A (AnnotListIndex), comme avant.
 */
Tcl_Obj *
TclAnnotIterList(Tcl_Obj *aPtr)
{
    Tcl_Obj *v, **ev;
    Tcl_Size n, i;
    int pass;

    if (aPtr->typePtr != &annotatedObjType
	    || FastListType(aPtr, &v) == NULL
	    || AnnotListElemType(aPtr) == NULL) {
	return NULL;
    }
    if (TclIsCompact(v)) {
	/* en cases : la liste des éléments gardée à côté des cases (des vues),
	 * privée à V, typée sur place comme une liste ordinaire */
	v = TclCompactCache(v);
	if (v == NULL) {
	    return NULL;
	}
    }
    for (pass = 0; pass < 2; pass++) {
	if (Tcl_ListObjGetElements(NULL, v, &n, &ev) != TCL_OK) {
	    return NULL;
	}
	for (i = 0; i < n && TCL_ANNOT_P(ev[i]); i++) {
	}
	if (i == n) {
	    return v;
	}
	if (pass == 1 || Tcl_IsShared(v)) {
	    return NULL;
	}
	AnnotWrapElems(aPtr, v, i, TCL_SIZE_MAX);
    }
    return NULL;
}

static Tcl_Size
AnnotListLength(Tcl_Obj *aPtr)
{
    Tcl_Obj *v;
    Tcl_Size n;

    if (FastListType(aPtr, &v) != NULL) {
	return (Tcl_ListObjLength(NULL, v, &n) == TCL_OK) ? n : 1;
    }

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
    TclType *lt = FastListType(aPtr, &v);

    if (lt != NULL && TclIsCompact(v)) {
	/* liste typée en cases : la vue sur la case, typée (refcount 0),
	 * sans repasser par la résolution du type */
	Tcl_Obj *c = TclCompactElement(v, index);

	*elemPtr = (c != NULL) ? MakeAnnotated(c, NewTypeRefNoCache(lt->elem,
		-1)) : NULL;
	return TCL_OK;
    }
    if (lt != NULL && TclIsCompactElem(v)) {
	/* une ligne d'une liste en cases (list(n).T) : la partie lue dans la
	 * case est neuve (refcount 0), typée ici */
	Tcl_Obj *c = NULL;

	Tcl_ListObjIndex(NULL, v, index, &c);
	*elemPtr = (c != NULL) ? MakeAnnotated(c, NewTypeRefNoCache(lt->elem,
		-1)) : NULL;
	return TCL_OK;
    }
    if (lt != NULL && Tcl_ListObjIndex(NULL, v, index, elemPtr) == TCL_OK
	    && *elemPtr != NULL && TCL_ANNOT_P(*elemPtr)) {
	return TCL_OK;			/* élément déjà typé dans V */
    }
    if (AnnotListView(interp, aPtr, &v)) {
	if (TclIsCompact(v)) {
	    /* une case : un élément neuf, typé (refcount 0, comme lseq) */
	    TclType *et = AnnotListElemType(aPtr);
	    Tcl_Obj *c = TclCompactElement(v, index);

	    *elemPtr = (c != NULL && et != NULL)
		    ? MakeAnnotated(c, NewTypeRefNoCache(et, -1)) : c;
	    return TCL_OK;
	}
	AnnotWrapElems(aPtr, v, index, index);
	if (Tcl_ListObjIndex(interp, v, index, elemPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (*elemPtr != NULL && !TCL_ANNOT_P(*elemPtr)) {
	    /*
	     * V partagé (une partie lue par chemin, une valeur copiée) :
	     * l'élément n'a pas pu être typé dans V. Il est typé ici, objet
	     * neuf (refcount 0, comme pour lseq) : l'élément d'une list.T est
	     * toujours un T, que V soit partagé ou non.
	     */
	    TclType *et = AnnotListElemType(aPtr);
	    int variant = -1;

	    if (et != NULL && !(et->kind == TCLT_SUM && TclTypeValidate(NULL,
		    et, *elemPtr, -1, &variant) != TCL_OK)) {
		*elemPtr = MakeAnnotated(*elemPtr, NewTypeRefNoCache(et,
			variant));
	    }
	}
	return TCL_OK;
    }
    *elemPtr = (index == 0) ? aPtr : NULL;	/* le scalaire est son élément */
    return TCL_OK;
}

static int
AnnotListGetElements(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Size *objcPtr,
	Tcl_Obj ***objvPtr)
{
    Tcl_Obj *v;

    if (AnnotListView(interp, aPtr, &v)) {
	if (TclIsCompact(v)) {
	    Tcl_Obj *c = TclCompactCache(v);	/* privé à V : enveloppable */

	    AnnotWrapElems(aPtr, c, 0, TCL_SIZE_MAX);
	    return Tcl_ListObjGetElements(interp, c, objcPtr, objvPtr);
	}
	AnnotWrapElems(aPtr, v, 0, TCL_SIZE_MAX);
	return Tcl_ListObjGetElements(interp, v, objcPtr, objvPtr);
    }
    /* Un seul élément : A lui-même (annotation comprise). */
    *objcPtr = 1;
    *objvPtr = SelfCell(aPtr);
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
    Tcl_Obj *content, *nPtr, *oldV, *oldT, *newV, *newT, *keepNotes = NULL;

    /*
     * Règle « une modification est un calcul » : le résultat est une valeur
     * ordinaire. Une variable sous contrat la retype au rangement
     * (TclTypedVarPrepareWrite) ; une variable sans contrat garde le
     * résultat brut, comme pour expr.
     */
    TclSetDuplicateObj(aPtr, wPtr);
    if (wPtr != NULL) {
	return TCL_OK;
    }

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
	{
	    Tcl_Obj *ot, *on;

	    SplitT((Tcl_Obj *) aPtr->internalRep.twoPtrValue.ptr2, &ot, &on);
	    if (on != NULL) {
		keepNotes = on;
	    }
	}
    } else {
	nPtr = MakeAnnotated(Tcl_DuplicateObj(wPtr),
		(Tcl_Obj *) aPtr->internalRep.twoPtrValue.ptr2);
    }
    newV = (Tcl_Obj *) nPtr->internalRep.twoPtrValue.ptr1;
    newT = (Tcl_Obj *) nPtr->internalRep.twoPtrValue.ptr2;
    if (keepNotes != NULL) {
	newT = StateObj(newT, keepNotes);	/* les notes suivent la valeur */
    }
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

/* Convention des setElementProc (TclLsetList et [lset] ajoutent la
 * référence ; INST_LSET_FLAT et lpop aussi, avec chemins-modification ou
 * lset-reference.patch) : une copie SANS référence propre ; NULL en cas
 * d'erreur. */
static Tcl_Obj *
AnnotListSetElement(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Size indexCount,
	Tcl_Obj *const indexArray[], Tcl_Obj *valueObj)
{
    Tcl_Obj *w = AnnotListCopy(interp, aPtr), *r, *target, *tagged = NULL;
    int rc;
    TclType *ltp = NULL;

    Tcl_IncrRefCount(w);
    if (indexCount > 1 && TclIsCompact(w)) {
	/*
	 * lset L i j... d'une liste en cases : TclLsetFlat garde l'élément i
	 * (pris dans la liste des éléments fabriquée à la demande) puis
	 * remplace l'élément de la liste, ce qui la convertit et libère cette
	 * liste avec l'élément. On part donc d'une liste ordinaire ; le
	 * résultat est remis en cases au rangement (TclAnnotSetContent).
	 */
	Tcl_Obj **ev, *pl;
	Tcl_Size ne;

	if (Tcl_ListObjGetElements(interp, w, &ne, &ev) != TCL_OK) {
	    Tcl_DecrRefCount(w);
	    return NULL;
	}
	pl = Tcl_NewListObj(ne, ev);
	Tcl_IncrRefCount(pl);
	Tcl_DecrRefCount(w);
	w = pl;
    }
    if (indexCount == 1 && TclAnnotInfo(aPtr, NULL, &ltp, NULL, NULL) == 1
	    && ltp->kind == TCLT_LIST && ltp->elem != NULL
	    && ltp->elem->kind == TCLT_SUM && !TclIsAnnotated(valueObj)) {
	/*
	 * Liste de sommes : la variante de l'élément remplacé départage des
	 * variantes également strictes (comme set L(0) 4, ou set v 8).
	 */
	Tcl_Obj **ev;
	Tcl_Size ne, ix;
	int cur, chosen = -1;

	if (Tcl_ListObjGetElements(NULL, w, &ne, &ev) == TCL_OK
		&& Tcl_GetIntForIndex(NULL, indexArray[0], ne - 1, &ix) == TCL_OK
		&& ix >= 0 && ix < ne
		&& (cur = TclTypeSumCurrent(ltp->elem, ev[ix])) >= 0
		&& TclTypeValidate(NULL, ltp->elem, valueObj, -2 - cur, &chosen)
			== TCL_OK
		&& TclTypeSumNeedsTag(ltp->elem, valueObj, chosen)) {
	    Tcl_Obj *c = Tcl_DuplicateObj(valueObj);

	    tagged = TclNewAnnotated(interp, ltp->elem, chosen, c);
	    TclDiscard(c);
	    Tcl_IncrRefCount(tagged);
	    valueObj = tagged;
	}
    }
    r = TclLsetFlat(interp, w, indexCount, indexArray, valueObj);
    if (tagged != NULL) {
	Tcl_DecrRefCount(tagged);
    }
    if (r == NULL) {
	Tcl_DecrRefCount(w);
	return NULL;
    }
    /* Jamais en place : la valeur d'une variable sous contrat doit rester
     * intacte si le contrat refuse le résultat au rangement. */
    target = Tcl_DuplicateObj(aPtr);
    rc = TclAnnotSetContent(interp, target, r);
    Tcl_DecrRefCount(r);			/* la référence de TclLsetFlat */
    Tcl_DecrRefCount(w);
    if (rc != TCL_OK) {
	if (target != aPtr) {
	    Tcl_BounceRefCount(target);
	}
	return NULL;
    }
    return target;			/* refcount 0 : l'appelant le compte */
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
 * Un produit dont un champ (ou le champ d'un champ) est une liste qui peut
 * être en cases : list($n).pointC dans polyC. Calculé une fois par type.
 */
static int
ProductHasCompactable(TclType *tp, int depth)
{
    Tcl_Size i;
    int r = -1;

    if (tp->compactFields != 0) {
	return tp->compactFields > 0;
    }
    if (depth > 16) {
	return 0;
    }
    tp->compactFields = -1;		/* type récursif : pas de boucle */
    for (i = 0; i < tp->nfields && r < 0; i++) {
	TclType *ft = tp->fields[i].type;

	if (ft == NULL) {
	    continue;
	}
	if ((ft->kind == TCLT_LIST && ft->elem != NULL
		&& TclCompactLayoutOf(ft->elem) != NULL)
		|| (ft->kind == TCLT_PRODUCT
		&& ProductHasCompactable(ft, depth + 1))) {
	    r = 1;
	}
    }
    tp->compactFields = r;
    return r > 0;
}

/*
 * Contenu (objet neuf) d'un produit dont les champs listes sont mis en
 * cases, ou NULL si rien ne change. Le texte du produit est gardé quand
 * celui de chaque champ l'est (pas de normalisation).
 */
static Tcl_Obj *
CompactFields(TclType *tp, Tcl_Obj *content, int depth)
{
    Tcl_Obj **ev, **nev = NULL, *out;
    Tcl_Size n, i;
    int keep = (content->bytes != NULL);

    if (tp->kind != TCLT_PRODUCT || depth > 16
	    || !ProductHasCompactable(tp, 0)
	    || TclIsAnnotated(content)
	    || Tcl_ListObjGetElements(NULL, content, &n, &ev) != TCL_OK
	    || n > tp->nfields) {
	return NULL;
    }
    for (i = 0; i < n; i++) {
	TclType *ft = tp->fields[i].type;
	Tcl_Obj *nv = NULL;

	if (ft == NULL || TclIsAnnotated(ev[i])) {
	    continue;
	}
	if (ft->kind == TCLT_LIST) {
	    nv = TclCompactFrom(ft, ev[i]);
	} else if (ft->kind == TCLT_PRODUCT) {
	    nv = CompactFields(ft, ev[i], depth + 1);
	}
	if (nv == NULL) {
	    continue;
	}
	if (nev == NULL) {
	    nev = (Tcl_Obj **) Tcl_Alloc((size_t) n * sizeof(Tcl_Obj *));
	    memcpy(nev, ev, (size_t) n * sizeof(Tcl_Obj *));
	}
	if (nv->bytes == NULL) {
	    keep = 0;			/* champ normalisé */
	}
	nev[i] = nv;
    }
    if (nev == NULL) {
	return NULL;
    }
    out = Tcl_NewListObj(n, nev);
    Tcl_Free(nev);
    if (keep) {
	TclInvalidateStringRep(out);
	Tcl_InitStringRep(out, content->bytes, content->length);	/* EIAS */
    }
    return out;
}

/* Contenu en cases pour un nouveau A de type tp, ou NULL. */
static Tcl_Obj *
CompactContent(TclType *tp, Tcl_Obj *content)
{
    if (tp->kind == TCLT_LIST) {
	return TclCompactFrom(tp, content);
    }
    if (tp->kind == TCLT_PRODUCT && tp->compactFields >= 0) {
	return CompactFields(tp, content, 0);
    }
    return NULL;
}

/*
 * Valeur typée : V = copie privée du contenu (conserve sa représentation
 * interne), T résolu. variant = -1 sauf pour une somme.
 */
Tcl_Obj *
TclNewAnnotated(Tcl_Interp *interp, TclType *tp, int variant,
	Tcl_Obj *content)
{
    Tcl_Obj *cv = CompactContent(tp, content);

    return MakeAnnotated(cv != NULL ? cv : Tcl_DuplicateObj(content),
	    TypeRefObj(interp, tp, variant));
}

/*
 * Idem, en PARTAGEANT le contenu (lecture d'une partie : le champ d'un
 * produit, l'élément d'une liste). Une valeur est immuable ; qui la modifie
 * travaille sur une copie (TclIncrObj, TclAnnotSetContent) : le partage
 * évite une copie par lecture.
 */
Tcl_Obj *
TclNewAnnotatedShared(Tcl_Interp *interp, TclType *tp, int variant,
	Tcl_Obj *content)
{
    return MakeAnnotated(content, TypeRefObj(interp, tp, variant));
}

/*
 * Pour les codecs en C (type binary decode) : la référence de type, à
 * garder pour une série de valeurs, et une valeur typée qui PREND le contenu
 * (pas de copie ; le contenu doit être neuf, sans autre propriétaire).
 */
Tcl_Obj *
TclTypeRefFor(Tcl_Interp *interp, TclType *tp, int variant)
{
    return TypeRefObj(interp, tp, variant);
}

Tcl_Obj *
TclNewAnnotatedOwn(Tcl_Obj *content, Tcl_Obj *typeRef)
{
    TclType *tp = IsTypeRef(typeRef)
	    ? (TclType *) typeRef->internalRep.twoPtrValue.ptr1 : NULL;
    Tcl_Obj *cv = (tp != NULL) ? CompactContent(tp, content) : NULL;

    if (cv != NULL) {
	TclDiscard(content);		/* le contenu neuf est remplacé */
	return MakeAnnotated(cv, typeRef);
    }
    return MakeAnnotated(content, typeRef);
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

/* Valeur typée par une référence de type, sans note : TclAnnotSettle n'a
 * rien à faire. */
int
TclAnnotIsSettled(Tcl_Obj *objPtr)
{
    return objPtr->typePtr == &annotatedObjType && ((Tcl_Obj *)
	    objPtr->internalRep.twoPtrValue.ptr2)->typePtr == &typeRefObjType;
}

/*
 * Opérande d'expr : une valeur typée int ou double (genre, nominal, famille,
 * bitarray), sans note, dont le contenu est déjà un nombre. Rend le contenu,
 * que l'exécuteur lit directement (comme TclAnnotUnwrap le ferait), ou NULL.
 */
Tcl_Obj *
TclAnnotNumericContent(Tcl_Obj *objPtr)
{
    Tcl_Obj *t, *c;
    TclType *tp;

    if (objPtr->typePtr != &annotatedObjType) {
	return NULL;
    }
    t = (Tcl_Obj *) objPtr->internalRep.twoPtrValue.ptr2;
    if (t->typePtr != &typeRefObjType) {
	return NULL;
    }
    tp = (TclType *) t->internalRep.twoPtrValue.ptr1;
    if (tp->kind != TCLT_INT && tp->kind != TCLT_DOUBLE) {
	return NULL;
    }
    c = (Tcl_Obj *) objPtr->internalRep.twoPtrValue.ptr1;
    if (c->typePtr != &tclIntType && c->typePtr != &tclDoubleType) {
	return NULL;
    }
    return c;
}

/* 0 : pas annoté ; 1 : typé (tpPtr, variantPtr remplis) ; 2 : brut. */
int
TclAnnotInfo(Tcl_Obj *objPtr, Tcl_Obj **contentPtr, TclType **tpPtr,
	int *variantPtr, Tcl_Obj **rawTypePtr)
{
    Tcl_ObjInternalRep *ir, *tir;
    Tcl_Obj *vPtr, *tPtr, *nPtr;

    if (objPtr->typePtr != &annotatedObjType) {
	return 0;
    }
    ir = &objPtr->internalRep;
    tPtr = (Tcl_Obj *) ir->twoPtrValue.ptr2;
    if (tPtr->typePtr == &typeRefObjType) {
	/* Cas courant : valeur typée, sans note (appel par élément). */
	if (contentPtr != NULL) {
	    *contentPtr = (Tcl_Obj *) ir->twoPtrValue.ptr1;
	}
	if (tpPtr != NULL) {
	    *tpPtr = (TclType *) tPtr->internalRep.twoPtrValue.ptr1;
	}
	if (variantPtr != NULL) {
	    *variantPtr = (int) (intptr_t) tPtr->internalRep.twoPtrValue.ptr2
		    - 1;
	}
	return 1;
    }
    vPtr = (Tcl_Obj *) ir->twoPtrValue.ptr1;
    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &tPtr, &nPtr);
    if (contentPtr != NULL) {
	*contentPtr = vPtr;
    }
    if (tPtr == NULL) {
	/* Note seule : la plus récente est lue comme un nom à la consommation. */
	Tcl_Size nn = 0;

	Tcl_ListObjLength(NULL, nPtr, &nn);
	Tcl_ListObjIndex(NULL, nPtr, nn - 1, &tPtr);
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
 * Vrai si le dernier échec de TclTypeParse vient d'un caractère invalide
 * (code d'erreur TCL PARSE TYPE BADCHAR) : le texte ne peut pas être un
 * type, c'est une annotation locale. Un nom inconnu, une parenthèse non
 * fermée, etc. restent des erreurs (faute de frappe sur un type ou une
 * variante).
 */
static int
RawIsLocal(Tcl_Interp *interp)
{
    Tcl_Obj *opts, *key, *code = NULL, *last = NULL;
    Tcl_Size n;
    int isLocal = 0;

    if (interp == NULL) {
	return 0;
    }
    opts = Tcl_GetReturnOptions(interp, TCL_ERROR);
    Tcl_IncrRefCount(opts);
    key = Tcl_NewStringObj("-errorcode", -1);
    Tcl_IncrRefCount(key);
    if (Tcl_DictObjGet(NULL, opts, key, &code)
	    == TCL_OK && code != NULL
	    && Tcl_ListObjLength(NULL, code, &n) == TCL_OK && n > 0
	    && Tcl_ListObjIndex(NULL, code, n - 1, &last) == TCL_OK
	    && last != NULL) {
	isLocal = (strcmp(Tcl_GetString(last), "BADCHAR") == 0);
    }
    Tcl_DecrRefCount(key);
    Tcl_DecrRefCount(opts);
    return isLocal;
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
    int variant = -1, kind, badVariant = 0;
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
    if (kind == 1 && ctx != NULL && ctx->kind == TCLT_SUM
	    && TclAnnotNotes(objPtr) != NULL) {
	/* {d}$v : une note qui nomme une variante de la somme cible. */
	Tcl_Obj *nl = TclAnnotNotes(objPtr), *last = NULL;
	Tcl_Size nn = 0;

	Tcl_ListObjLength(NULL, nl, &nn);
	if (nn > 0 && Tcl_ListObjIndex(NULL, nl, nn - 1, &last) == TCL_OK) {
	    idx = TclTypeFieldIndex(ctx, Tcl_GetString(last));
	    if (idx >= 0 && ctx->fields[idx].type != NULL) {
		*statusPtr = TCLA_RAW;
		*tpPtr = (TclType *) ctx;
		*variantPtr = (int) idx;
		return TCL_OK;
	    }
	}
    }
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
    if (interp == NULL) {
	/*
	 * Sans interpréteur, pas de table des types (Tcl_GetNumberFromObj
	 * (NULL, ...)) : une annotation en attente se lit comme son contenu,
	 * sans validation.
	 */
	*statusPtr = TCLA_PLAIN;
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
    /* Val.i : le type somme Val, variante i (comme {Val}{i}). */
    {
	const char *dot = (const char *) memchr(text, '.', len);

	if (interp != NULL && dot != NULL && dot > text && dot + 1 < text + len) {
	    Tcl_Obj *saved = Tcl_GetObjResult(interp);
	    TclType *sum;

	    Tcl_IncrRefCount(saved);
	    sum = TclTypeParse(interp, text, dot - text);
	    Tcl_SetObjResult(interp, saved);	/* l'échec éventuel n'est pas une erreur */
	    Tcl_DecrRefCount(saved);

	    if (sum != NULL && sum->kind == TCLT_SUM) {
		char *vname = (char *) Tcl_Alloc(len - (dot - text));

		memcpy(vname, dot + 1, len - (dot - text) - 1);
		vname[len - (dot - text) - 1] = '\0';
		idx = TclTypeFieldIndex(sum, vname);
		Tcl_Free(vname);
		if (idx >= 0 && sum->fields[idx].type != NULL) {
		    *statusPtr = TCLA_RAW;
		    *tpPtr = sum;
		    *variantPtr = (int) idx;
		    return TCL_OK;
		}
		badVariant = 1;		/* Val.zz : pas d'indulgence */
	    }
	}
    }
    {
	/*
	 * L'appelant peut tenir la valeur à ranger dans le résultat de
	 * l'interpréteur (catch, set x [cmd]) : l'échec de l'analyse y écrit
	 * son message, et on ne le laisse pas écraser ce résultat tant qu'on
	 * ne sait pas si c'est une vraie erreur.
	 */

	Tcl_Obj *saved = NULL;

	if (interp != NULL) {
	    saved = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(saved);
	}
	tp = TclTypeParse(interp, text, len);
	if (tp == NULL && saved != NULL && (RawIsLocal(interp)
		|| (!badVariant && LastErrorCodeIs(interp, "UNKNOWN")))) {
	    /*
	     * Un nom pas encore défini est une NOTE en attente : elle ne
	     * gêne aucun lecteur, la valeur se lit comme son contenu. (Le nom
	     * d'une variable, lui, doit être un type : TclSetAnnotatedCmd le
	     * vérifie à part.)
	     */
	    if (!RawIsLocal(interp)) {
		Tcl_SetObjResult(interp, saved);
		Tcl_DecrRefCount(saved);
		*statusPtr = TCLA_PLAIN;
		return TCL_OK;
	    }
	    /*
	     * Annotation LOCALE ({-v}, {what?}) : un caractère que la grammaire
	     * des types refuse, donc jamais un type ni un nom de variante. Elle
	     * s'adresse à la commande appelée, pas à la variable qui rangera la
	     * valeur : on la laisse de côté et on garde le contenu.
	     */

	    Tcl_SetObjResult(interp, saved);
	    Tcl_DecrRefCount(saved);
	    *statusPtr = TCLA_PLAIN;
	    return TCL_OK;
	}
	if (saved != NULL) {
	    Tcl_DecrRefCount(saved);
	}
    }
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
    /* EIAS : une chaîne (string, littéral d'énumération, nominal sur
     * string) se lit comme un nombre si son texte en est un ; la
     * conversion qui suit échoue avec le message habituel sinon. */
    case TCLWANT_NUMBER:
    case TCLWANT_DOUBLE:	/* élargissement sans shimmering */
	return kind == TCLT_INT || kind == TCLT_DOUBLE || kind == TCLT_STRING;
    case TCLWANT_INT:		return kind == TCLT_INT || kind == TCLT_STRING;
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

    {
	/* Le cas courant : un scalaire typé sans note (un double lu par
	 * chemin, un int) ; son genre suffit, sans résolution. */
	Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(objPtr,
		&annotatedObjType);
	Tcl_Obj *t, *n;

	if (ir != NULL) {
	    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
	    if (n == NULL && t != NULL && IsTypeRef(t)) {
		TclType *ft = (TclType *) Tcl_FetchInternalRep(t,
			&typeRefObjType)->twoPtrValue.ptr1;

		if ((ft->kind == TCLT_INT || ft->kind == TCLT_DOUBLE
			|| ft->kind == TCLT_STRING) && KindAllows(ft->kind, want)) {
		    *contentPtr = (Tcl_Obj *) ir->twoPtrValue.ptr1;
		    return TCL_OK;
		}
	    }
	}
    }
    if (TclAnnotResolve(interp, objPtr, NULL, &status, &tp, &variant,
	    &content) != TCL_OK) {
	return TCL_ERROR;
    }
    if (want != TCLWANT_LIST && TclIsCompact(content)) {
	/* une conversion (chaîne, dict...) se fait sur une copie : V reste
	 * en cases */
	content = TclCompactConv(content);
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
    /*
     * Un produit d'UN SEUL champ se lit comme ce champ pour les conversions
     * scalaires (nombre, entier, flottant, booléen) : {{int}i} qui vaut 5
     * est le nombre 5. Le contenu rendu est le champ lui-même, pour ne pas
     * convertir la liste V. (La vue liste, elle, garde la règle B.)
     */
    if (want != TCLWANT_LIST && want != TCLWANT_DICT) {
	int depth;

	for (depth = 0; depth < 16; depth++) {
	    const TclType *et = tp;
	    Tcl_Obj *field;

	    if (et->kind == TCLT_SUM && variant >= 0
		    && variant < (int) et->nfields
		    && et->fields[variant].type != NULL) {
		et = et->fields[variant].type;
	    }
	    if (et->kind != TCLT_PRODUCT || et->nfields != 1
		    || et->fields[0].type == NULL
		    || Tcl_ListObjIndex(NULL, content, 0, &field) != TCL_OK
		    || field == NULL) {
		break;
	    }
	    tp = et->fields[0].type;
	    content = TCL_ANNOT_CONTENT(field);
	    variant = -1;
	    if (tp->kind == TCLT_SUM
		    && TclTypeValidate(NULL, tp, content, -1, &variant)
			    != TCL_OK) {
		break;
	    }
	    *contentPtr = content;
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
static int AnnotateCore(Tcl_Interp *interp, Tcl_Obj *typeText,
	Tcl_Obj *value, Tcl_Obj **resultPtr);

int
TclAnnotateValue(Tcl_Interp *interp, Tcl_Obj *typeText, Tcl_Obj *value,
	Tcl_Obj **resultPtr)
{
    Tcl_Obj *notes = TclAnnotNotes(value);

    if (AnnotateCore(interp, typeText, value, resultPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (notes != NULL && *resultPtr != value
	    && TclAnnotNotes(*resultPtr) == NULL) {
	/* Le cast ne touche pas aux notes. */
	Tcl_Obj *r2 = AnnotWithNotes(*resultPtr, notes);

	if (r2 != *resultPtr) {
	    TclDiscard(*resultPtr);
	    *resultPtr = r2;
	}
    }
    return TCL_OK;
}

static int
AnnotateCore(Tcl_Interp *interp, Tcl_Obj *typeText, Tcl_Obj *value,
	Tcl_Obj **resultPtr)
{
    Tcl_Size len;
    int kind;
    Tcl_Obj *vcontent = NULL;

    (void) Tcl_GetStringFromObj(typeText, &len);
    kind = TclAnnotInfo(value, &vcontent, NULL, NULL, NULL);
    if (kind == 0) {
	vcontent = value;
    }
    if (len == 0) {
	/*
	 * {}$x : marque « type vide ». Un constructeur qui la reçoit retire
	 * le type résolu et garde la pile ; tout autre lecteur voit le
	 * contenu nu (type et pile effacés).
	 */
	*resultPtr = AnnotClearType(value);
	return TCL_OK;
    }
    if (ClassifyAnnot(interp, typeText, NULL, NULL, NULL) == ANN_LOCAL) {
	/* {-v} : locale, éphémère, lue par la commande appelée seule. */
	*resultPtr = TclNewRawAnnotation(typeText, vcontent);
	return TCL_OK;
    }
    /*
     * Hors constructeur, une annotation ne valide ni ne caste rien, même
     * si elle nomme un type connu : elle est empilée. Le premier
     * constructeur que la valeur traverse la résoudra (TclAnnotSettle).
     */
    *resultPtr = AnnotAddNote(value, typeText);
    return TCL_OK;
}

/* --------------------------------------------------------------------
 * Le résolveur des constructeurs
 * -------------------------------------------------------------------- */

/* Dernier élément du code d'erreur de l'interpréteur (objet à libérer), ou
 * NULL. */
static int
LastErrorCodeIs(Tcl_Interp *interp, const char *what)
{
    Tcl_Obj *opts, *key, *code = NULL, *last = NULL;
    Tcl_Size n;
    int is = 0;

    if (interp == NULL) {
	return 0;
    }
    opts = Tcl_GetReturnOptions(interp, TCL_ERROR);
    Tcl_IncrRefCount(opts);
    key = Tcl_NewStringObj("-errorcode", -1);
    Tcl_IncrRefCount(key);
    if (Tcl_DictObjGet(NULL, opts, key, &code) == TCL_OK && code != NULL
	    && Tcl_ListObjLength(NULL, code, &n) == TCL_OK && n > 0
	    && Tcl_ListObjIndex(NULL, code, n - 1, &last) == TCL_OK
	    && last != NULL) {
	is = (strcmp(Tcl_GetString(last), what) == 0);
    }
    Tcl_DecrRefCount(key);
    Tcl_DecrRefCount(opts);
    return is;
}

/*
 * Ce que désigne le texte d'une annotation, sans toucher au résultat de
 * l'interpréteur (sauf ANN_BAD, qui y laisse le message d'erreur) :
 *   ANN_TYPE    : un type connu (*tpPtr), éventuellement une variante
 *                 désignée par Val.i (*variantPtr) ;
 *   ANN_VARIANT : un nom de variante de la somme ctx (*variantPtr) ;
 *   ANN_UNKNOWN : un nom pas (encore) défini, list.zz... : une note ;
 *   ANN_LOCAL   : un caractère que les noms refusent ({-v}, {what?}) ;
 *   ANN_BAD     : un texte de type mal formé (int( ...).
 */

/*
 * Le classement d'un texte d'annotation (type, local, inconnu), gardé dans
 * l'objet texte lui-même : « point » dans « A is point = $a », littéral du
 * bytecode, n'est analysé qu'une fois. Valable pour la même table de types
 * (interp, uid) à la même génération (tout type def la change).
 */
typedef struct AnnotClass {
    Tcl_Interp *interp;
    size_t uid, gen;
    int res, variant;
    TclType *tp;
} AnnotClass;

static Tcl_FreeInternalRepProc FreeAnnotClass;
static Tcl_DupInternalRepProc DupAnnotClass;

static const Tcl_ObjType annotClassType = {
    "annotclass", FreeAnnotClass, DupAnnotClass, NULL, NULL, TCL_OBJTYPE_V0
};

static void
FreeAnnotClass(Tcl_Obj *objPtr)
{
    Tcl_Free(objPtr->internalRep.twoPtrValue.ptr1);
    objPtr->typePtr = NULL;
}

static void
DupAnnotClass(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
    AnnotClass *c = (AnnotClass *) Tcl_Alloc(sizeof(AnnotClass));
    Tcl_ObjInternalRep ir;

    *c = *(AnnotClass *) srcPtr->internalRep.twoPtrValue.ptr1;
    ir.twoPtrValue.ptr1 = c;
    ir.twoPtrValue.ptr2 = NULL;
    Tcl_StoreInternalRep(dupPtr, &annotClassType, &ir);
}

static int ClassifyAnnotRaw(Tcl_Interp *interp, Tcl_Obj *textObj,
	TclType **tpPtr, int *variantPtr);

static int
ClassifyAnnot(Tcl_Interp *interp, Tcl_Obj *textObj, const TclType *ctx,
	TclType **tpPtr, int *variantPtr)
{
    Tcl_Size len, idx;
    const char *text = Tcl_GetStringFromObj(textObj, &len);
    TclType *tp = NULL;

    if (tpPtr != NULL) {
	*tpPtr = NULL;
    }
    if (variantPtr != NULL) {
	*variantPtr = -1;
    }
    if (ctx != NULL && ctx->kind == TCLT_SUM) {
	idx = TclTypeFieldIndex(ctx, text);
	if (idx >= 0 && ctx->fields[idx].type != NULL) {
	    if (variantPtr != NULL) {
		*variantPtr = (int) idx;
	    }
	    return ANN_VARIANT;
	}
    }
    if (interp == NULL) {
	return ANN_UNKNOWN;
    }
    {
	size_t uid, gen;
	AnnotClass *c;
	int r, v = -1;

	if (!TclTypesGeneration(interp, &uid, &gen)) {
	    return ClassifyAnnotRaw(interp, textObj, tpPtr, variantPtr);
	}
	c = (textObj->typePtr == &annotClassType)
		? (AnnotClass *) textObj->internalRep.twoPtrValue.ptr1 : NULL;
	if (c != NULL && c->interp == interp && c->uid == uid
		&& c->gen == gen) {
	    if (tpPtr != NULL) {
		*tpPtr = c->tp;
	    }
	    if (variantPtr != NULL) {
		*variantPtr = c->variant;
	    }
	    return c->res;
	}
	(void) text;
	(void) len;
	r = ClassifyAnnotRaw(interp, textObj, &tp, &v);
	/* gardé si le texte a sa chaîne (il la garde : pas de mise à jour
	 * de chaîne pour ce type) et si le classement est sûr */
	if (r != ANN_BAD && textObj->bytes != NULL
		&& (textObj->typePtr == NULL
		    || textObj->typePtr == &annotClassType
		    || textObj->typePtr->updateStringProc != NULL)) {
	    if (c == NULL) {
		Tcl_ObjInternalRep ir;

		c = (AnnotClass *) Tcl_Alloc(sizeof(AnnotClass));
		ir.twoPtrValue.ptr1 = c;
		ir.twoPtrValue.ptr2 = NULL;
		Tcl_StoreInternalRep(textObj, &annotClassType, &ir);
	    }
	    c->interp = interp;
	    c->uid = uid;
	    c->gen = gen;
	    c->res = r;
	    c->variant = v;
	    c->tp = tp;
	}
	if (tpPtr != NULL) {
	    *tpPtr = tp;
	}
	if (variantPtr != NULL) {
	    *variantPtr = v;
	}
	return r;
    }
}

/* Le type que nomme un texte d'annotation (classement en cache), ou NULL
 * (note, local, variante, invalide). */
TclType *
TclAnnotTypeOf(Tcl_Interp *interp, Tcl_Obj *textObj)
{
    TclType *tp = NULL;
    int v = -1;

    if (ClassifyAnnot(interp, textObj, NULL, &tp, &v) != ANN_TYPE || v >= 0) {
	return NULL;
    }
    return tp;
}

/* Le classement sans cache (ni contexte) : type, variante S.v, local,
 * inconnu ou invalide. */
static int
ClassifyAnnotRaw(Tcl_Interp *interp, Tcl_Obj *textObj, TclType **tpPtr,
	int *variantPtr)
{
    Tcl_Size len, idx;
    const char *text = Tcl_GetStringFromObj(textObj, &len);
    Tcl_InterpState saved;
    TclType *tp;
    int res;

    if (tpPtr != NULL) {
	*tpPtr = NULL;
    }
    if (variantPtr != NULL) {
	*variantPtr = -1;
    }
    /* Tout l'état (résultat, errorCode, errorInfo) : une analyse qui échoue
     * ne doit rien laisser derrière elle. */
    saved = Tcl_SaveInterpState(interp, TCL_OK);
    /* Val.i : la somme Val, variante i. */
    {
	const char *dot = (const char *) memchr(text, '.', len);

	if (dot != NULL && dot > text && dot + 1 < text + len) {
	    TclType *sum = TclTypeParse(interp, text, dot - text);

	    if (sum != NULL && sum->kind == TCLT_SUM) {
		char *vname = (char *) Tcl_Alloc(len - (dot - text));

		memcpy(vname, dot + 1, len - (dot - text) - 1);
		vname[len - (dot - text) - 1] = '\0';
		idx = TclTypeFieldIndex(sum, vname);
		Tcl_Free(vname);
		if (idx >= 0 && sum->fields[idx].type != NULL) {
		    Tcl_RestoreInterpState(interp, saved);
		    if (tpPtr != NULL) {
			*tpPtr = sum;
		    }
		    if (variantPtr != NULL) {
			*variantPtr = (int) idx;
		    }
		    return ANN_TYPE;
		}
		/* Val.zz, Val connu : une faute sur la variante, pas une
		 * note. */
		Tcl_RestoreInterpState(interp, saved);
		return ANN_BAD;
	    }
	}
    }
    tp = TclTypeParse(interp, text, len);
    if (tp != NULL) {
	res = ANN_TYPE;
	if (tpPtr != NULL) {
	    *tpPtr = tp;
	}
    } else if (LastErrorCodeIs(interp, "BADCHAR")) {
	res = ANN_LOCAL;
    } else if (LastErrorCodeIs(interp, "UNKNOWN")
	    || LastErrorCodeIs(interp, "RESERVED")) {
	res = ANN_UNKNOWN;
    } else {
	res = ANN_BAD;		/* l'appelant refait l'analyse pour le message */
    }
    Tcl_RestoreInterpState(interp, saved);
    return res;
}

/* Le dernier élément du code d'erreur est-il WHAT ? (BADCHAR, UNKNOWN) */
int
TclAnnotErrorCodeIs(Tcl_Interp *interp, const char *what)
{
    return LastErrorCodeIs(interp, what);
}

/* Valeur A' = valeur avec typeText empilé (la plus récente). */
Tcl_Obj *
TclAnnotPush(Tcl_Obj *value, Tcl_Obj *typeText)
{
    return AnnotAddNote(value, typeText);
}

int
TclAnnotIsCleared(Tcl_Obj *objPtr)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(objPtr, &annotatedObjType);
    Tcl_Obj *t, *n;
    Tcl_Size len;

    if (ir == NULL) {
	return 0;
    }
    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
    if (t == NULL || IsTypeRef(t)) {
	return 0;
    }
    Tcl_GetStringFromObj(t, &len);
    return len == 0;
}

/*
 * La pile complète d'une valeur, constructeur compris : un texte brut non
 * vide (annotation littérale {int}5) en est le fond, puis les notes.
 * withCleared : garder la pile sous une marque {} (lecture d'un
 * constructeur) ; sinon une valeur marquée n'a pas de pile.
 */
static Tcl_Obj *
PendingOf(Tcl_Obj *objPtr, int withCleared)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(objPtr, &annotatedObjType);
    Tcl_Obj *t, *n, *res, **elv;
    Tcl_Size len, nn, i;

    res = Tcl_NewListObj(0, NULL);
    if (ir == NULL) {
	return res;
    }
    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
    if (t != NULL && !IsTypeRef(t)) {
	Tcl_GetStringFromObj(t, &len);
	if (len == 0 && !withCleared) {
	    return res;
	}
	if (len > 0) {
	    Tcl_ListObjAppendElement(NULL, res, Tcl_DuplicateObj(t));
	}
    }
    if (n != NULL && Tcl_ListObjGetElements(NULL, n, &nn, &elv) == TCL_OK) {
	for (i = 0; i < nn; i++) {
	    Tcl_ListObjAppendElement(NULL, res, Tcl_DuplicateObj(elv[i]));
	}
    }
    return res;
}

Tcl_Obj *
TclAnnotPending(Tcl_Obj *objPtr)
{
    return PendingOf(objPtr, 0);
}

/*
 * La valeur sans son annotation EN ATTENTE la plus récente (la dernière note,
 * ou le texte brut d'une annotation littérale s'il n'y a pas de note) ;
 * *topPtr reçoit ce texte (non possédé, vivant tant que src l'est). NULL si
 * la valeur n'a pas d'annotation en attente. Sert aux étiquettes de champ
 * d'un constructeur de produit : set {paire}p {nom}jean {age}12.
 */
/* Annotation qui ne nomme rien de connu (ni type, ni local) : une note. */
int
TclAnnotIsUnknown(Tcl_Interp *interp, Tcl_Obj *textObj)
{
    return ClassifyAnnot(interp, textObj, NULL, NULL, NULL) == ANN_UNKNOWN;
}

Tcl_Obj *
TclAnnotPopTop(Tcl_Obj *src, Tcl_Obj **topPtr)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(src, &annotatedObjType);
    Tcl_Obj *content, *t, *n, *newN, *p2;
    Tcl_Size nn = 0, len;

    if (ir == NULL) {
	return NULL;
    }
    content = (Tcl_Obj *) ir->twoPtrValue.ptr1;
    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
    if (n != NULL) {
	Tcl_ListObjLength(NULL, n, &nn);
    }
    if (nn > 0) {
	Tcl_ListObjIndex(NULL, n, nn - 1, topPtr);
	newN = Tcl_DuplicateObj(n);
	Tcl_ListObjReplace(NULL, newN, nn - 1, 1, 0, NULL);
	p2 = StateObj(t, newN);
	if (p2 == NULL) {
	    TclDiscard(newN);
	    return Tcl_DuplicateObj(content);
	}
	if (p2 == t) {
	    TclDiscard(newN);
	}
	return MakeAnnotated(Tcl_DuplicateObj(content), p2);
    }
    if (t != NULL && !IsTypeRef(t)) {
	Tcl_GetStringFromObj(t, &len);
	if (len > 0) {
	    *topPtr = t;
	    return Tcl_DuplicateObj(content);
	}
    }
    return NULL;
}

/* res (valeur typée fraîche, sans pile) + la pile donnée. */
Tcl_Obj *
TclAnnotWithPending(Tcl_Obj *res, Tcl_Obj *pending)
{
    return AnnotWithNotes(res, pending);
}

#define SETTLE_MAX 64

int
TclAnnotSettle(Tcl_Interp *interp, Tcl_Obj *objPtr, const TclType *ctx,
	int flags, Tcl_Obj **resultPtr)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(objPtr, &annotatedObjType);
    Tcl_Obj *content, *t, *n, *pend, **elv, *remaining, *val = NULL, *res;
    TclType *cur = NULL, *rt[SETTLE_MAX], *tp;
    int curVar = -1, rv[SETTLE_MAX], nr = 0, changed = 0, stopped = 0;
    int cls, v, variant = -1;
    char keep[SETTLE_MAX * 4];
    Tcl_Size np, i, j;

    *resultPtr = objPtr;
    if (ir == NULL) {
	return TCL_OK;
    }
    content = (Tcl_Obj *) ir->twoPtrValue.ptr1;
    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
    if (n == NULL && t != NULL && IsTypeRef(t) && !(flags & TCLA_SETTLE_KEEP)) {
	return TCL_OK;		/* type résolu, aucune note : rien à faire */
    }
    if (t != NULL && IsTypeRef(t)) {
	Tcl_ObjInternalRep *tir = Tcl_FetchInternalRep(t, &typeRefObjType);

	cur = (TclType *) tir->twoPtrValue.ptr1;
	curVar = (int) (intptr_t) tir->twoPtrValue.ptr2 - 1;
    } else if (t != NULL) {
	changed = 1;			/* texte brut ou marque {} : à replier */
    }
    if (flags & TCLA_SETTLE_KEEP) {
	if (cur != NULL) {
	    changed = 1;		/* set {}y $x : le type résolu part */
	}
	cur = NULL;
    }
    pend = PendingOf(objPtr, 1);
    Tcl_IncrRefCount(pend);
    Tcl_ListObjGetElements(NULL, pend, &np, &elv);
    if (np > (Tcl_Size) sizeof(keep)) {
	np = (Tcl_Size) sizeof(keep);
    }

    /* Dépilement depuis le sommet. */
    for (i = np - 1; i >= 0; i--) {
	keep[i] = 1;
	if (stopped) {
	    continue;
	}
	cls = ClassifyAnnot(interp, elv[i], ctx, &tp, &v);
	if (cls == ANN_LOCAL) {
	    keep[i] = 0;		/* éphémère : jamais rangée */
	    changed = 1;
	    continue;
	}
	if (flags & TCLA_SETTLE_KEEP) {
	    continue;			/* rien n'est résolu */
	}
	if (cls == ANN_TYPE && nr < SETTLE_MAX) {
	    rt[nr] = tp;
	    rv[nr] = v;
	    nr++;
	    keep[i] = 0;
	    changed = 1;
	    continue;
	}
	if (cls == ANN_BAD) {
	    Tcl_Size bl;
	    const char *bt = Tcl_GetStringFromObj(elv[i], &bl);

	    (void) TclTypeParse(interp, bt, bl);	/* le message d'erreur */
	    Tcl_DecrRefCount(pend);
	    return TCL_ERROR;
	}
	if (nr > 0 && rv[nr - 1] < 0 && rt[nr - 1]->kind == TCLT_SUM
		&& cur == NULL) {
	    /* type cast S {v}x : la balise sous le type d'une somme nomme
	     * sa variante (comme set {S}y {v}x). */
	    Tcl_Size fi = TclTypeFieldIndex(rt[nr - 1],
		    Tcl_GetString(elv[i]));

	    if (fi >= 0) {
		rv[nr - 1] = (int) fi;
		keep[i] = 0;
		changed = 1;
	    }
	}
	stopped = 1;			/* nom inconnu, ou variante de ctx */
    }

    if (!changed) {
	Tcl_DecrRefCount(pend);
	return TCL_OK;			/* rien à faire : la valeur telle quelle */
    }

    /* Cohérence : la plus récente doit entrer dans chacune des autres. */
    for (j = 1; j < nr; j++) {
	if (!TclTypeEnters(rt[j], rt[0])) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"contradictory annotations %s and %s",
			rt[0]->name, rt[j]->name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "CONTRADICT",
			(char *) NULL);
	    }
	    Tcl_DecrRefCount(pend);
	    return TCL_ERROR;
	}
    }

    /* Le cast. */
    if (nr > 0 && cur != NULL && cur == rt[0] && cur->kind == TCLT_SUM
	    && rv[0] >= 0 && curVar >= 0 && rv[0] != curVar) {
	/* {S.b}$v, v en S.a : d'une variante à l'autre (règle, ou cast). */
	Tcl_Obj *c;

	if (TclTypeVariantConvert(interp, cur, curVar, content, rv[0], &c)
		!= TCL_OK) {
	    Tcl_DecrRefCount(pend);
	    return TCL_ERROR;
	}
	val = TclNewAnnotated(interp, cur, rv[0], c);
	TclDiscard(c);
	Tcl_IncrRefCount(val);
    } else if (nr > 0 && cur != NULL) {
	Tcl_Obj *base = MakeAnnotated(Tcl_DuplicateObj(content),
		TypeRefObj(interp, cur, curVar));

	Tcl_IncrRefCount(base);
	if (TclTypeCast(interp, base, rt[0], &val) != TCL_OK) {
	    Tcl_DecrRefCount(base);
	    Tcl_DecrRefCount(pend);
	    return TCL_ERROR;
	}
	if (val == base) {
	    val = MakeAnnotated(Tcl_DuplicateObj(content),
		    TypeRefObj(interp, cur, curVar));
	}
	Tcl_IncrRefCount(val);
	Tcl_DecrRefCount(base);
    } else if (nr > 0) {
	Tcl_Obj *copy = Tcl_DuplicateObj(content);

	Tcl_IncrRefCount(copy);
	if (TclTypeComplete(interp, rt[0], copy, rv[0], &variant) != TCL_OK) {
	    Tcl_DecrRefCount(copy);
	    Tcl_DecrRefCount(pend);
	    return TCL_ERROR;
	}
	val = TclNewAnnotated(interp, rt[0], variant, copy);
	Tcl_IncrRefCount(val);
	Tcl_DecrRefCount(copy);
    } else if (cur != NULL) {
	val = MakeAnnotated(Tcl_DuplicateObj(content),
		TypeRefObj(interp, cur, curVar));
	Tcl_IncrRefCount(val);
    }

    /* La pile restante, dans l'ordre. */
    remaining = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(remaining);
    for (i = 0; i < np; i++) {
	if (keep[i]) {
	    Tcl_ListObjAppendElement(NULL, remaining, elv[i]);
	}
    }
    Tcl_ListObjLength(NULL, remaining, &np);
    if (val != NULL) {
	Tcl_ObjInternalRep *vir = Tcl_FetchInternalRep(val, &annotatedObjType);

	res = (np == 0) ? val : MakeAnnotated(
		(Tcl_Obj *) vir->twoPtrValue.ptr1,
		StateObj((Tcl_Obj *) vir->twoPtrValue.ptr2, remaining));
	if (res != val) {
	    Tcl_IncrRefCount(res);
	    Tcl_DecrRefCount(val);
	}
	/* res tient une référence ; on la rend « sans référence propre » en
	 * la confiant au résultat (l'appelant l'incrémente). */
	res->refCount--;
    } else if (np > 0) {
	res = MakeAnnotated(Tcl_DuplicateObj(content), StateObj(NULL,
		remaining));
    } else {
	res = Tcl_DuplicateObj(content);	/* plus rien : valeur ordinaire */
    }
    Tcl_DecrRefCount(remaining);
    Tcl_DecrRefCount(pend);
    *resultPtr = res;
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

/* Le mot (TCL_TOKEN_WORD) est-il de la forme {texte}mot où texte est une annotation
 * LOCALE (un caractère que la grammaire des types refuse) ? Renvoie son texte (objet
 * à libérer par l'appelant) ou NULL. Sert à TclCompileSetCmd. */
Tcl_Obj *
TclAnnotLocalWordText(Tcl_Interp *interp, Tcl_Token *wordTokenPtr)
{
    Tcl_Obj *saved, *text;
    int local;

    if (wordTokenPtr->type != TCL_TOKEN_WORD || wordTokenPtr->numComponents < 1
	    || wordTokenPtr[1].type != TCL_TOKEN_ANNOT) {
	return NULL;
    }
    saved = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(saved);
    text = Tcl_NewStringObj(wordTokenPtr[1].start, wordTokenPtr[1].size);
    Tcl_IncrRefCount(text);
    local = (TclTypeParse(interp, wordTokenPtr[1].start,
	    wordTokenPtr[1].size) == NULL) && RawIsLocal(interp);
    Tcl_SetObjResult(interp, saved);
    Tcl_DecrRefCount(saved);
    if (!local) {
	Tcl_DecrRefCount(text);
	return NULL;
    }
    return text;		/* refCount 1 : à libérer */
}

/* ::tcl::annotlocal text : émis par TclCompileSetCmd pour « set x {-v}mot » :
 * une annotation locale ne s'adresse qu'à l'appelé, pas à une variable. */
static int
AnnotLocalObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    (void) clientData;
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "text");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "local annotation {%s} cannot be stored by \"set\": it only "
	    "applies between a caller and the command it calls",
	    Tcl_GetString(objv[1])));
    Tcl_SetErrorCode(interp, "TCL", "TYPE", "LOCALANNOT", (char *)NULL);
    return TCL_ERROR;
}

/*
 * {list}$p, {dict}$p, {list.T}$p sur une valeur TYPÉE : un cast vers une
 * collection, fait tout de suite, au moment de l'annotation (la vue liste
 * n'a pas d'interpréteur pour résoudre un nom de type). Le produit rend ses
 * champs, la liste nominale ses éléments ; la chaîne ne change pas.
 * Rend TCL_OK (*resultPtr rempli), TCL_ERROR (cast refusé), ou TCL_CONTINUE
 * (pas concerné : annoter normalement).
 */
int
TclAnnotCollectionCast(Tcl_Interp *interp, Tcl_Obj *typeText, Tcl_Obj *value,
	Tcl_Obj **resultPtr)
{
    TclType *from = NULL, *to;
    Tcl_InterpState state;
    Tcl_Size tlen;
    const char *ttext = Tcl_GetStringFromObj(typeText, &tlen);

    if (TclAnnotInfo(value, NULL, &from, NULL, NULL) != 1 || from == NULL) {
	return TCL_CONTINUE;
    }
    /* Un nom inconnu reste une note : l'échec de l'analyse ne doit rien
     * laisser, ni résultat ni errorInfo (TclEvalEx s'en servirait). */
    state = Tcl_SaveInterpState(interp, TCL_OK);
    to = TclTypeParse(interp, ttext, tlen);
    Tcl_RestoreInterpState(interp, state);
    if (to == NULL || to == from
	    || (to->kind != TCLT_LIST && to->kind != TCLT_DICT)) {
	return TCL_CONTINUE;
    }
    return TclTypeCast(interp, value, to, resultPtr);
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
    switch (TclAnnotCollectionCast(interp, objv[1], objv[2], &res)) {
    case TCL_ERROR:
	return TCL_ERROR;
    case TCL_OK:
	Tcl_SetObjResult(interp, res);
	return TCL_OK;
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


/*
 * Vrai si l'annotation a été ÉCRITE dans le source ({T}M, {}M) : un texte de
 * type brut, pas encore résolu. Une valeur substituée ($nm) porte au plus un
 * type résolu ou des notes : pour un NOM de variable, elle ne déclare rien.
 */
int
TclAnnotIsSourceRaw(Tcl_Obj *objPtr)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(objPtr, &annotatedObjType);
    Tcl_Obj *t, *n;

    if (ir == NULL) {
	return 0;
    }
    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
    return t != NULL && !IsTypeRef(t);
}

/* Vrai si la valeur n'a que des notes (aucun type résolu ni brut). */
int
TclAnnotIsNoteOnly(Tcl_Obj *objPtr)
{
    Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(objPtr, &annotatedObjType);
    Tcl_Obj *t, *n;

    if (ir == NULL) {
	return 0;
    }
    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
    return t == NULL && n != NULL;
}

/*
 * Affichage symétrique autour de « @ » (withNotes) : le type résolu de l'extérieur vers
 * l'intérieur, puis les notes de la plus ancienne (intérieure) à la plus
 * récente (extérieure) : list.int@mm.hello. Sans note : list.int. Sans type
 * résolu : @mm. Valeur non annotée : chaîne vide. Rend un objet neuf.
 */
Tcl_Obj *
TclAnnotDisplay(Tcl_Interp *interp, Tcl_Obj *objPtr, int withNotes)
{
    Tcl_Obj *norm, *res = Tcl_NewObj(), *raw = NULL, *content, *notes, *j;
    TclType *tp = NULL;
    int variant = -1, kind;
    Tcl_ObjInternalRep *ir;
    Tcl_Obj *t, *n;

    norm = TclAnnotNormalize(interp, objPtr);
    Tcl_IncrRefCount(norm);
    ir = Tcl_FetchInternalRep(norm, &annotatedObjType);
    if (ir != NULL) {
	SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &n);
	kind = TclAnnotInfo(norm, &content, &tp, &variant, &raw);
	if (kind == 1) {
	    Tcl_AppendToObj(res, tp->name, TCL_AUTO_LENGTH);
	    if (tp->kind == TCLT_SUM && variant >= 0
		    && variant < (int) tp->nfields
		    && tp->fields[variant].name != NULL) {
		Tcl_AppendToObj(res, ".", 1);
		Tcl_AppendObjToObj(res, tp->fields[variant].name);
	    }
	}
	/* Une annotation en attente (même un type connu, pas encore résolu
	 * par un constructeur) appartient à la pile, pas au type. */
	(void) t;
	(void) n;
	(void) raw;
	notes = PendingOf(norm, 0);
	Tcl_IncrRefCount(notes);
	if (withNotes) {
	    Tcl_Size nn = 0;

	    Tcl_ListObjLength(NULL, notes, &nn);
	    if (nn > 0) {
		j = NotesJoined(notes);
		Tcl_AppendToObj(res, "@", 1);
		Tcl_AppendObjToObj(res, j);
		TclDiscard(j);
	    }
	}
	Tcl_DecrRefCount(notes);
    }
    Tcl_DecrRefCount(norm);
    return res;
}

/*
 * note get VALUE | note has VALUE NAME | note delete VALUE ?NAME?
 *
 * Les valeurs sont immuables : delete RENVOIE la valeur sans la note (ou
 * sans aucune note), contenu et type résolu inchangés.
 */
static int
NoteObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    const char *sub;
    Tcl_Obj *norm, *notes, **elv;
    Tcl_Size n, i;
    int rc = TCL_OK;

    (void) clientData;
    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "get|has|delete value ?name?");
	return TCL_ERROR;
    }
    sub = Tcl_GetString(objv[1]);
    norm = objv[2];
    Tcl_IncrRefCount(norm);
    notes = PendingOf(norm, 0);		/* la pile, vue d'un lecteur */
    Tcl_IncrRefCount(notes);
    if (strcmp(sub, "get") == 0 && objc == 3) {
	Tcl_Obj *j = NotesJoined(notes);

	Tcl_SetObjResult(interp, j);
    } else if (strcmp(sub, "has") == 0 && objc == 4) {
	int found = 0;

	if (notes != NULL && Tcl_ListObjGetElements(NULL, notes, &n, &elv)
		== TCL_OK) {
	    for (i = 0; i < n; i++) {
		if (strcmp(Tcl_GetString(elv[i]), Tcl_GetString(objv[3]))
			== 0) {
		    found = 1;
		    break;
		}
	    }
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(found));
    } else if (strcmp(sub, "delete") == 0 && (objc == 3 || objc == 4)) {
	Tcl_ObjInternalRep *ir = Tcl_FetchInternalRep(norm, &annotatedObjType);
	Tcl_Obj *t, *nl, *newN, *content, *res;

	Tcl_Size pn = 0;

	Tcl_ListObjLength(NULL, notes, &pn);
	if (ir == NULL || pn == 0) {
	    Tcl_SetObjResult(interp, objv[2]);	/* rien à retirer */
	} else {
	    SplitT((Tcl_Obj *) ir->twoPtrValue.ptr2, &t, &nl);
	    if (t != NULL && !IsTypeRef(t)) {
		t = NULL;			/* texte brut : il est dans la pile */
	    }
	    content = (Tcl_Obj *) ir->twoPtrValue.ptr1;
	    newN = Tcl_NewListObj(0, NULL);
	    if (objc == 4) {
		Tcl_ListObjGetElements(NULL, notes, &n, &elv);
		for (i = 0; i < n; i++) {
		    if (strcmp(Tcl_GetString(elv[i]),
			    Tcl_GetString(objv[3])) != 0) {
			Tcl_ListObjAppendElement(NULL, newN,
				Tcl_DuplicateObj(elv[i]));
		    }
		}
	    }
	    if (t == NULL) {
		Tcl_Size k = 0;

		Tcl_ListObjLength(NULL, newN, &k);
		if (k == 0) {			/* plus rien : valeur ordinaire */
		    res = Tcl_DuplicateObj(content);
		} else {
		    res = MakeAnnotated(Tcl_DuplicateObj(content),
			    StateObj(NULL, newN));
		}
	    } else {
		res = MakeAnnotated(Tcl_DuplicateObj(content),
			StateObj(t, newN));
	    }
	    TclDiscard(newN);
	    Tcl_SetObjResult(interp, res);
	}
    } else {
	Tcl_WrongNumArgs(interp, 1, objv, "get|has|delete value ?name?");
	rc = TCL_ERROR;
    }
    Tcl_DecrRefCount(notes);
    Tcl_DecrRefCount(norm);
    return rc;
}

void
TclAnnotInit(Tcl_Interp *interp)
{
    Tcl_CreateObjCommand2(interp, "note", NoteObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::annotate", AnnotateObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::annotraw", AnnotRawObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand2(interp, "::tcl::annotlocal", AnnotLocalObjCmd, NULL,
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

/* ====================================================================
 * Représentation compacte (prototype ; tcl::unsupported::compact on)
 * ====================================================================
 *
 * Le contenu V d'une liste list.T, quand T est un scalaire à taille fixe
 * (int(s,n), int(u,n) avec n = 8, 16, 32, 64 ; double, double(32|64)) ou un
 * produit de tels champs, devient un tableau de cases : une case par
 * élément, disposée comme le struct C équivalent (alignement naturel,
 * mesuré à la compilation : TclTypeAbiAlign). V reste un Tcl_Obj, de type
 * "compactlist", liste abstraite (longueur, index, remplacement) : le reste
 * de Tcl le voit comme une liste ; ce qui demande tous les éléments à la
 * fois (Tcl_ListObjGetElements) le convertit en liste ordinaire.
 *
 * EIAS : la forme compacte n'est prise que si chaque nombre est écrit sous
 * sa forme canonique (celle que Tcl regénère) et chaque élément aussi
 * (espaces) ; la chaîne de la liste entière, si elle existe, est gardée
 * telle quelle jusqu'à la première modification. Ainsi la chaîne d'une
 * valeur compacte est toujours celle qu'aurait la liste ordinaire.
 *
 * Pour le C : le type "compactlist" est enregistré (Tcl_GetObjType) ;
 * V->internalRep.twoPtrValue.ptr1 pointe sur un TclCompactBuf dont les
 * quatre premiers champs sont publics (voir typeparse/tclCompact.h). Le
 * pointeur data est valable tant que l'objet n'est pas modifié ni converti.
 */

enum { CN_PROD = 0, CN_INT, CN_UINT, CN_FLT };

typedef struct CNode {
    int kind;
    int size;			/* scalaire : octets (1, 2, 4, 8) */
    size_t off;			/* décalage dans la case de l'élément */
    int nchild;			/* produit : nombre de champs */
    int nscal;			/* scalaires du sous-arbre */
    int nsub;			/* noeuds du sous-arbre, lui compris */
    const char *name;		/* champ (pour la disposition), ou NULL */
    int isList;			/* produit : 1 si c'est une liste de taille
				 * fixe list(n).T (n enfants identiques,
				 * disposés comme un tableau C T[n]) */
    int index;			/* élément d'une telle liste : son rang, sinon
				 * -1 */
} CNode;

/* Au-delà, une liste de taille fixe (développée noeud par noeud) n'est pas
 * mise en cases. */
#define COMPACT_MAX_NODES 4096

struct TclCompactLayout {
    size_t refCount;
    size_t size, align;
    int nnodes;
    CNode node[1];		/* préordre */
};

typedef struct TclCompactBuf {
    /* public (tclCompact.h) */
    unsigned char *data;
    Tcl_Size n;
    size_t stride;
    const char *elemType;	/* nom du type des éléments */
    /* privé */
    size_t refCount;
    Tcl_Size cap;
    TclType *et;
    TclCompactLayout *lay;
    int norm;			/* type {-compact} : les nombres sont normalisés
				 * (écrits sous leur forme canonique) au lieu
				 * de refuser la forme compacte */
} TclCompactBuf;

static int compactOn = 0;

static Tcl_FreeInternalRepProc	FreeCompact;
static Tcl_DupInternalRepProc	DupCompact;
static Tcl_UpdateStringProc	UpdateStringOfCompact;
static Tcl_ObjTypeLengthProc	CompactLength;
static Tcl_ObjTypeIndexProc	CompactIndex;
static Tcl_ObjTypeReplaceProc	CompactReplace;
static Tcl_ObjTypeGetElements	CompactGetElements;

static const Tcl_ObjType compactListType = {
    "compactlist", FreeCompact, DupCompact, UpdateStringOfCompact, NULL,
    TCL_OBJTYPE_V2(CompactLength, CompactIndex, NULL, NULL,
	    CompactGetElements, NULL, CompactReplace, NULL)
};

/* ptr2 : ce qu'on fabrique à la demande et garde tant que V ne change
 * pas, pour que V reste compact : la liste ordinaire des éléments (à la
 * première demande de TOUS les éléments : lsort, lrange...) et une copie
 * en chaîne, que les conversions du noyau (string length...) convertissent
 * à la place de V. */
typedef struct CompactAux {
    Tcl_Obj *list;
    Tcl_Obj *conv;
} CompactAux;

#define COMPACT_AUX(o) ((CompactAux *) (o)->internalRep.twoPtrValue.ptr2)
#define COMPACT_CACHE(o) (COMPACT_AUX(o) ? COMPACT_AUX(o)->list : NULL)

static CompactAux *
CompactAuxOf(Tcl_Obj *objPtr)
{
    CompactAux *a = COMPACT_AUX(objPtr);

    if (a == NULL) {
	a = (CompactAux *) Tcl_Alloc(sizeof(CompactAux));
	a->list = a->conv = NULL;
	objPtr->internalRep.twoPtrValue.ptr2 = a;
    }
    return a;
}

static void
CompactDropCache(Tcl_Obj *objPtr)
{
    CompactAux *a = COMPACT_AUX(objPtr);

    if (a != NULL) {
	objPtr->internalRep.twoPtrValue.ptr2 = NULL;
	if (a->list != NULL) {
	    Tcl_DecrRefCount(a->list);
	}
	if (a->conv != NULL) {
	    Tcl_DecrRefCount(a->conv);
	}
	Tcl_Free(a);
    }
}

#define COMPACT_BUF(o) ((TclCompactBuf *) (o)->internalRep.twoPtrValue.ptr1)

/*
 * Un élément produit d'une liste compacte, lu sans être recopié : une vue
 * (tampon partagé, début de la case, noeud). La liste qui change copie son
 * tampon s'il est partagé : la vue garde la valeur qu'elle a lue.
 */
typedef struct CompactElem {
    TclCompactBuf *b;
    size_t cell;		/* début de la case de l'élément */
    int node;			/* le produit vu (noeud de la disposition) */
    Tcl_Obj *cache;		/* liste ordinaire, à la demande */
} CompactElem;

static Tcl_FreeInternalRepProc	FreeCompactElem;
static Tcl_DupInternalRepProc	DupCompactElem;
static Tcl_UpdateStringProc	UpdateStringOfCompactElem;
static Tcl_ObjTypeLengthProc	CompactElemLength;
static Tcl_ObjTypeIndexProc	CompactElemIndex;
static Tcl_ObjTypeGetElements	CompactElemGetElements;
static Tcl_ObjTypeReplaceProc	CompactElemReplace;

static const Tcl_ObjType compactElemType = {
    "compactelem", FreeCompactElem, DupCompactElem, UpdateStringOfCompactElem,
    NULL, TCL_OBJTYPE_V2(CompactElemLength, CompactElemIndex, NULL, NULL,
	    CompactElemGetElements, NULL, CompactElemReplace, NULL)
};

#define COMPACT_ELEM(o) ((CompactElem *) (o)->internalRep.twoPtrValue.ptr1)
static Tcl_Obj *NewCompactElemObj(TclCompactBuf *b, size_t cell, int node);

/* --- disposition --- */

static int
CompactScalar(const TclType *t, int *kindPtr, int *sizePtr)
{
    union { uint16_t u; unsigned char c[2]; } bo;
    int native;

    bo.u = 1;
    native = bo.c[0] ? TCLBIN_LE : TCLBIN_BE;
    if (t->binLink != NULL || t->binTag != NULL || t->binAt != NULL
	    || t->binDyn != NULL || t->bitWidth != 0) {
	return 0;
    }
    if (t->binEnd != TCLBIN_AUTO && t->binEnd != native) {
	return 0;
    }
    if (t->kind == TCLT_DOUBLE) {
	if (t->binKind == TCLBIN_NONE) {
	    *kindPtr = CN_FLT;
	    *sizePtr = 8;
	    return 1;
	}
	if (t->binKind == TCLBIN_FLOAT && (t->binSize == 4 || t->binSize == 8)) {
	    *kindPtr = CN_FLT;
	    *sizePtr = (int) t->binSize;
	    return 1;
	}
	return 0;
    }
    if (t->kind == TCLT_INT && (t->binKind == TCLBIN_SINT
	    || t->binKind == TCLBIN_UINT) && (t->binSize == 1
	    || t->binSize == 2 || t->binSize == 4 || t->binSize == 8)) {
	*kindPtr = (t->binKind == TCLBIN_SINT) ? CN_INT : CN_UINT;
	*sizePtr = (int) t->binSize;
	return 1;
    }
    return 0;
}

/* Compte les noeuds (ou -1 : pas de forme compacte). */
static int
CompactCount(const TclType *t, int depth)
{
    int k, s, n = 1, i, c;

    if (depth > 32) {
	return -1;
    }
    if (CompactScalar(t, &k, &s)) {
	return 1;
    }
    if (t->kind == TCLT_LIST) {
	/* list(n).T, n fixe, T en cases : un tableau C T[n] */
	if (t->arity < 1 || t->elem == NULL || t->binTag != NULL
		|| t->binAt != NULL || t->binDyn != NULL || t->binLink != NULL
		|| (c = CompactCount(t->elem, depth + 1)) < 0
		|| (Tcl_WideInt) c * t->arity >= COMPACT_MAX_NODES) {
	    return -1;
	}
	return 1 + (int) (c * t->arity);
    }
    if (t->kind != TCLT_PRODUCT || t->nfields < 1 || t->binTag != NULL
	    || t->binAt != NULL || t->binDyn != NULL) {
	return -1;
    }
    for (i = 0; i < t->nfields; i++) {
	if (t->fields[i].type == NULL
		|| (c = CompactCount(t->fields[i].type, depth + 1)) < 0) {
	    return -1;
	}
	n += c;
	if (n >= COMPACT_MAX_NODES) {
	    return -1;
	}
    }
    return n;
}

static size_t
AlignUp(size_t x, size_t a)
{
    return (x + a - 1) / a * a;
}

/* Remplit en préordre ; *off et *align avancent comme dans un struct C. */
static void
CompactFill(const TclType *t, const char *name, CNode *nodes, int *idx,
	size_t *off, size_t *align)
{
    int me = (*idx)++;
    CNode *nd = &nodes[me];
    int k, s, i;

    nd->name = name;
    nd->nsub = 1;
    nd->isList = 0;
    nd->index = -1;
    if (CompactScalar(t, &k, &s)) {
	size_t a = (size_t) TclTypeAbiAlign(k == CN_FLT, s * 8);

	*off = AlignUp(*off, a);
	nd->kind = k;
	nd->size = s;
	nd->off = *off;
	nd->nchild = 0;
	nd->nscal = 1;
	*off += (size_t) s;
	if (a > *align) {
	    *align = a;
	}
	return;
    }
    {
	/* un produit imbriqué est un struct imbriqué : il s'aligne et se
	 * complète comme tel */
	size_t inner = 0, ialign = 1, base;
	int first = *idx;

	nd->kind = CN_PROD;
	nd->nscal = 0;
	if (t->kind == TCLT_LIST) {
	    nd->isList = 1;
	    nd->nchild = (int) t->arity;
	    for (i = 0; i < t->arity; i++) {
		int child = *idx;

		CompactFill(t->elem, NULL, nodes, idx, &inner, &ialign);
		nodes[child].index = i;
	    }
	} else {
	    nd->nchild = (int) t->nfields;
	    for (i = 0; i < t->nfields; i++) {
		CompactFill(t->fields[i].type,
			Tcl_GetString(t->fields[i].name), nodes, idx, &inner,
			&ialign);
	    }
	}
	inner = AlignUp(inner, ialign);
	base = AlignUp(*off, ialign);
	for (i = first; i < *idx; i++) {
	    nodes[i].off += base;
	    if (nodes[i].kind != CN_PROD) {
		nd->nscal++;
	    }
	}
	nd->off = base;
	nd->nsub = *idx - me;
	*off = base + inner;
	if (ialign > *align) {
	    *align = ialign;
	}
    }
}

TclCompactLayout *
TclCompactLayoutOf(TclType *et)
{
    TclCompactLayout *lay;
    int n, idx = 0;
    size_t off = 0, align = 1;

    if (et->compactState != 0) {
	return et->compact;
    }
    n = CompactCount(et, 0);
    if (n < 0) {
	et->compactState = -1;
	return NULL;
    }
    lay = (TclCompactLayout *) Tcl_Alloc(sizeof(TclCompactLayout)
	    + (size_t) (n - 1) * sizeof(CNode));
    memset(lay, 0, sizeof(TclCompactLayout) + (size_t) (n - 1) * sizeof(CNode));
    CompactFill(et, NULL, lay->node, &idx, &off, &align);
    lay->nnodes = n;
    lay->align = align;
    lay->size = AlignUp(off, align);
    lay->refCount = 1;			/* celle du type */
    et->compact = lay;
    et->compactState = 1;
    return lay;
}

void
TclCompactLayoutRelease(TclCompactLayout *lay)
{
    if (lay != NULL && --lay->refCount == 0) {
	Tcl_Free(lay);
    }
}

/* --- cases --- */

static int
EncodeScalar(const CNode *nd, Tcl_Obj *o, unsigned char *p, int norm)
{
    char buf[TCL_DOUBLE_SPACE + 32];
    int pure;

    o = TCL_ANNOT_CONTENT(o);
    if (nd->kind == CN_FLT) {
	double d;

	pure = norm || (TclHasInternalRep(o, &tclDoubleType) && o->bytes == NULL);
	if (Tcl_GetDoubleFromObj(NULL, o, &d) != TCL_OK) {
	    return 0;
	}
	if (nd->size == 4) {
	    float f = (float) d;

	    if ((double) f != d && d == d && !norm) {
		return 0;		/* pas exactement un float */
	    }
	    memcpy(p, &f, 4);
	} else {
	    memcpy(p, &d, 8);
	}
	if (!pure) {
	    Tcl_PrintDouble(NULL, d, buf);
	    if (strcmp(Tcl_GetString(o), buf) != 0) {
		return 0;		/* forme non canonique : 1, 0x10, 1e3 */
	    }
	}
	return 1;
    }
    pure = norm || (TclHasInternalRep(o, &tclIntType) && o->bytes == NULL);
    if (nd->kind == CN_UINT) {
	Tcl_WideUInt u;

	if (Tcl_GetWideUIntFromObj(NULL, o, &u) != TCL_OK
		|| (nd->size < 8 && u >> (nd->size * 8) != 0)) {
	    return 0;
	}
	switch (nd->size) {
	case 1: { uint8_t v = (uint8_t) u; memcpy(p, &v, 1); break; }
	case 2: { uint16_t v = (uint16_t) u; memcpy(p, &v, 2); break; }
	case 4: { uint32_t v = (uint32_t) u; memcpy(p, &v, 4); break; }
	default: { uint64_t v = (uint64_t) u; memcpy(p, &v, 8); break; }
	}
	if (!pure) {
	    snprintf(buf, sizeof(buf), "%llu", (unsigned long long) u);
	    if (strcmp(Tcl_GetString(o), buf) != 0) {
		return 0;
	    }
	}
	return 1;
    } else {
	Tcl_WideInt w;

	if (Tcl_GetWideIntFromObj(NULL, o, &w) != TCL_OK) {
	    return 0;
	}
	if (nd->size < 8) {
	    Tcl_WideInt lim = (Tcl_WideInt) 1 << (nd->size * 8 - 1);

	    if (w < -lim || w >= lim) {
		return 0;
	    }
	}
	switch (nd->size) {
	case 1: { int8_t v = (int8_t) w; memcpy(p, &v, 1); break; }
	case 2: { int16_t v = (int16_t) w; memcpy(p, &v, 2); break; }
	case 4: { int32_t v = (int32_t) w; memcpy(p, &v, 4); break; }
	default: { int64_t v = (int64_t) w; memcpy(p, &v, 8); break; }
	}
	if (!pure) {
	    snprintf(buf, sizeof(buf), "%lld", (long long) w);
	    if (strcmp(Tcl_GetString(o), buf) != 0) {
		return 0;
	    }
	}
	return 1;
    }
}

static void
FormatScalar(const CNode *nd, const unsigned char *p, Tcl_DString *ds)
{
    char buf[TCL_DOUBLE_SPACE + 32];

    switch (nd->kind) {
    case CN_FLT:
	if (nd->size == 4) {
	    float f;

	    memcpy(&f, p, 4);
	    Tcl_PrintDouble(NULL, (double) f, buf);
	} else {
	    double d;

	    memcpy(&d, p, 8);
	    Tcl_PrintDouble(NULL, d, buf);
	}
	break;
    case CN_INT: {
	long long w = 0;

	switch (nd->size) {
	case 1: { int8_t v; memcpy(&v, p, 1); w = v; break; }
	case 2: { int16_t v; memcpy(&v, p, 2); w = v; break; }
	case 4: { int32_t v; memcpy(&v, p, 4); w = v; break; }
	default: { int64_t v; memcpy(&v, p, 8); w = v; break; }
	}
	snprintf(buf, sizeof(buf), "%lld", w);
	break;
    }
    default: {
	unsigned long long u = 0;

	switch (nd->size) {
	case 1: { uint8_t v; memcpy(&v, p, 1); u = v; break; }
	case 2: { uint16_t v; memcpy(&v, p, 2); u = v; break; }
	case 4: { uint32_t v; memcpy(&v, p, 4); u = v; break; }
	default: { uint64_t v; memcpy(&v, p, 8); u = v; break; }
	}
	snprintf(buf, sizeof(buf), "%llu", u);
	break;
    }
    }
    Tcl_DStringAppend(ds, buf, TCL_AUTO_LENGTH);
}

static Tcl_Obj *
DecodeScalar(const CNode *nd, const unsigned char *p)
{
    switch (nd->kind) {
    case CN_FLT:
	if (nd->size == 4) {
	    float f;

	    memcpy(&f, p, 4);
	    return Tcl_NewDoubleObj((double) f);
	} else {
	    double d;

	    memcpy(&d, p, 8);
	    return Tcl_NewDoubleObj(d);
	}
    case CN_INT:
	switch (nd->size) {
	case 1: { int8_t v; memcpy(&v, p, 1); return Tcl_NewWideIntObj(v); }
	case 2: { int16_t v; memcpy(&v, p, 2); return Tcl_NewWideIntObj(v); }
	case 4: { int32_t v; memcpy(&v, p, 4); return Tcl_NewWideIntObj(v); }
	default: { int64_t v; memcpy(&v, p, 8); return Tcl_NewWideIntObj(v); }
	}
    default:
	switch (nd->size) {
	case 1: { uint8_t v; memcpy(&v, p, 1); return Tcl_NewWideIntObj(v); }
	case 2: { uint16_t v; memcpy(&v, p, 2); return Tcl_NewWideIntObj(v); }
	case 4: { uint32_t v; memcpy(&v, p, 4); return Tcl_NewWideIntObj(v); }
	default: { uint64_t v; memcpy(&v, p, 8); return Tcl_NewWideUIntObj(v); }
	}
    }
}

/* Texte d'un noeud (sans quotation extérieure). Rend l'indice suivant. */
static int
FormatNode(const CNode *nodes, int i, const unsigned char *cell,
	Tcl_DString *ds)
{
    const CNode *nd = &nodes[i];
    int k, j = i + 1;

    if (nd->kind != CN_PROD) {
	FormatScalar(nd, cell + nd->off, ds);
	return j;
    }
    for (k = 0; k < nd->nchild; k++) {
	Tcl_DString sub;
	int flags;
	Tcl_Size len;
	char *q;

	if (k > 0) {
	    Tcl_DStringAppend(ds, " ", 1);
	}
	Tcl_DStringInit(&sub);
	j = FormatNode(nodes, j, cell, &sub);
	len = Tcl_ScanCountedElement(Tcl_DStringValue(&sub),
		Tcl_DStringLength(&sub), &flags);
	q = (char *) Tcl_Alloc(len + 1);
	len = Tcl_ConvertCountedElement(Tcl_DStringValue(&sub),
		Tcl_DStringLength(&sub), q,
		flags | (k > 0 ? TCL_DONT_QUOTE_HASH : 0));
	Tcl_DStringAppend(ds, q, len);
	Tcl_Free(q);
	Tcl_DStringFree(&sub);
    }
    return j;
}

/* Encode un élément (contenu, éventuellement annoté) ; -1 si impossible. */
static int
EncodeNode(const CNode *nodes, int i, Tcl_Obj *o, unsigned char *cell,
	int norm)
{
    const CNode *nd = &nodes[i];
    Tcl_Obj **ev;
    Tcl_Size n, k;
    int j = i + 1;

    if (nd->kind != CN_PROD) {
	return EncodeScalar(nd, o, cell + nd->off, norm) ? j : -1;
    }
    o = TCL_ANNOT_CONTENT(o);
    if (o->typePtr == &compactElemType && o->bytes == NULL) {
	/* une vue sur une case de même disposition : copie des octets */
	CompactElem *ce = COMPACT_ELEM(o);
	const CNode *src = &ce->b->lay->node[ce->node];

	if (src->nsub == nd->nsub && memcmp(src, nd, (size_t) nd->nsub
		* sizeof(CNode)) == 0) {
	    /* mêmes noeuds, décalages compris : la case entière ou la partie */
	    size_t lo = nd->off, hi = lo;
	    int q;

	    for (q = i; q < i + nd->nsub; q++) {
		if (nodes[q].kind != CN_PROD
			&& nodes[q].off + (size_t) nodes[q].size > hi) {
		    hi = nodes[q].off + (size_t) nodes[q].size;
		}
	    }
	    memcpy(cell + lo, ce->b->data + ce->cell + lo, hi - lo);
	    return i + nd->nsub;
	}
    }
    if (Tcl_ListObjGetElements(NULL, o, &n, &ev) != TCL_OK
	    || n != nd->nchild) {
	return -1;
    }
    for (k = 0; k < n; k++) {
	j = EncodeNode(nodes, j, ev[k], cell, norm);
	if (j < 0) {
	    return -1;
	}
    }
    if (o->bytes != NULL && !norm) {
	/* le texte de l'élément doit être celui qu'on regénérera */
	Tcl_DString ds;
	int same;

	Tcl_DStringInit(&ds);
	FormatNode(nodes, i, cell, &ds);
	same = (Tcl_DStringLength(&ds) == o->length
		&& memcmp(Tcl_DStringValue(&ds), o->bytes, o->length) == 0);
	Tcl_DStringFree(&ds);
	if (!same) {
	    return -1;
	}
    }
    return j;
}

/* Un noeud d'une case du tampon b : vue pour un produit, nombre sinon. */
static Tcl_Obj *
CompactPart(TclCompactBuf *b, size_t cell, int node)
{
    const CNode *nd = &b->lay->node[node];

    if (nd->kind == CN_PROD) {
	return NewCompactElemObj(b, cell, node);
    }
    return DecodeScalar(nd, b->data + cell + nd->off);
}

static Tcl_Obj *
DecodeNode(const CNode *nodes, int *ip, const unsigned char *cell)
{
    const CNode *nd = &nodes[(*ip)++];
    Tcl_Obj *l;
    int k;

    if (nd->kind != CN_PROD) {
	return DecodeScalar(nd, cell + nd->off);
    }
    l = Tcl_NewListObj(0, NULL);
    for (k = 0; k < nd->nchild; k++) {
	Tcl_ListObjAppendElement(NULL, l, DecodeNode(nodes, ip, cell));
    }
    return l;
}

/* --- le tampon --- */

static TclCompactBuf *
NewCompactBuf(TclType *et, TclCompactLayout *lay, Tcl_Size cap)
{
    TclCompactBuf *b = (TclCompactBuf *) Tcl_Alloc(sizeof(TclCompactBuf));
    size_t bytes = (size_t) (cap > 0 ? cap : 1) * lay->size;

    b->data = (unsigned char *) Tcl_Alloc(bytes);
    memset(b->data, 0, bytes);		/* remplissage d'alignement : 0 */
    b->n = 0;
    b->cap = cap > 0 ? cap : 1;
    b->stride = lay->size;
    b->elemType = et->name;
    b->refCount = 1;
    b->et = et;
    b->lay = lay;
    b->norm = 0;
    lay->refCount++;
    return b;
}

static void
ReleaseCompactBuf(TclCompactBuf *b)
{
    if (--b->refCount == 0) {
	TclCompactLayoutRelease(b->lay);
	Tcl_Free(b->data);
	Tcl_Free(b);
    }
}

static void
FreeCompact(Tcl_Obj *objPtr)
{
    CompactDropCache(objPtr);
    ReleaseCompactBuf(COMPACT_BUF(objPtr));
    objPtr->typePtr = NULL;
}

static void
DupCompact(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
    TclCompactBuf *b = COMPACT_BUF(srcPtr);
    Tcl_ObjInternalRep ir;

    b->refCount++;			/* partagé ; copie à l'écriture */
    ir.twoPtrValue.ptr1 = b;
    ir.twoPtrValue.ptr2 = NULL;
    Tcl_StoreInternalRep(dupPtr, &compactListType, &ir);
}

static void
UpdateStringOfCompact(Tcl_Obj *objPtr)
{
    TclCompactBuf *b = COMPACT_BUF(objPtr);
    Tcl_DString ds, el;
    Tcl_Size i, len;
    int flags;
    char *q;

    Tcl_DStringInit(&ds);
    for (i = 0; i < b->n; i++) {
	if (i > 0) {
	    Tcl_DStringAppend(&ds, " ", 1);
	}
	Tcl_DStringInit(&el);
	FormatNode(b->lay->node, 0, b->data + (size_t) i * b->stride, &el);
	len = Tcl_ScanCountedElement(Tcl_DStringValue(&el),
		Tcl_DStringLength(&el), &flags);
	q = (char *) Tcl_Alloc(len + 1);
	len = Tcl_ConvertCountedElement(Tcl_DStringValue(&el),
		Tcl_DStringLength(&el), q,
		flags | (i > 0 ? TCL_DONT_QUOTE_HASH : 0));
	Tcl_DStringAppend(&ds, q, len);
	Tcl_Free(q);
	Tcl_DStringFree(&el);
    }
    Tcl_InitStringRep(objPtr, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);
}

static Tcl_Size
CompactLength(Tcl_Obj *objPtr)
{
    return COMPACT_BUF(objPtr)->n;
}

static int
CompactIndex(Tcl_Interp *interp, Tcl_Obj *objPtr, Tcl_Size index,
	Tcl_Obj **elemPtr)
{
    TclCompactBuf *b = COMPACT_BUF(objPtr);
    int i = 0;

    (void) interp;
    if (index < 0 || index >= b->n) {
	*elemPtr = NULL;
	return TCL_OK;
    }
    (void) i;
    *elemPtr = CompactPart(b, (size_t) index * b->stride, 0);
    return TCL_OK;
}

/* Liste ordinaire des éléments (refcount 0). */
static Tcl_Obj *
CompactMaterialize(TclCompactBuf *b)
{
    Tcl_Obj *l = Tcl_NewListObj(b->n, NULL);
    Tcl_Size k;

    for (k = 0; k < b->n; k++) {
	int i = 0;

	Tcl_ListObjAppendElement(NULL, l, DecodeNode(b->lay->node, &i,
		b->data + (size_t) k * b->stride));
    }
    return l;
}

static int
CompactGetElements(Tcl_Interp *interp, Tcl_Obj *objPtr, Tcl_Size *objcPtr,
	Tcl_Obj ***objvPtr)
{
    CompactAux *a = CompactAuxOf(objPtr);

    if (a->list == NULL) {
	a->list = CompactMaterialize(COMPACT_BUF(objPtr));
	Tcl_IncrRefCount(a->list);
    }
    return Tcl_ListObjGetElements(interp, a->list, objcPtr, objvPtr);
}

/* La copie en chaîne de V, que les conversions du noyau peuvent
 * transformer sans défaire la forme compacte. */
Tcl_Obj *
TclCompactConv(Tcl_Obj *v)
{
    CompactAux *a;

    if (v->typePtr != &compactListType) {
	return v;
    }
    a = CompactAuxOf(v);
    if (a->conv == NULL) {
	Tcl_Size len;
	const char *s = Tcl_GetStringFromObj(v, &len);

	a->conv = Tcl_NewStringObj(s, len);
	Tcl_IncrRefCount(a->conv);
    }
    return a->conv;
}

/* Le cache des éléments d'un V compact (pour l'enveloppe des éléments
 * d'une valeur annotée), ou NULL. */
Tcl_Obj *
TclCompactCache(Tcl_Obj *v)
{
    Tcl_Size n;
    Tcl_Obj **ev;

    if (v->typePtr != &compactListType) {
	return NULL;
    }
    CompactGetElements(NULL, v, &n, &ev);
    return COMPACT_CACHE(v);
}

static int
CompactReplace(Tcl_Interp *interp, Tcl_Obj *objPtr, Tcl_Size first,
	Tcl_Size numToDelete, Tcl_Size numToInsert, Tcl_Obj *const insertObjs[])
{
    TclCompactBuf *b = COMPACT_BUF(objPtr), *nb;
    size_t st = b->stride;
    unsigned char *tmp = NULL;
    Tcl_Size k, n = b->n, newn;
    int ok = 1;

    if (first < 0) {
	first = 0;
    }
    if (first > n) {
	first = n;
    }
    if (numToDelete < 0) {
	numToDelete = 0;
    }
    if (first + numToDelete > n) {
	numToDelete = n - first;
    }
    if (numToInsert > 0) {
	tmp = (unsigned char *) Tcl_Alloc((size_t) numToInsert * st);
	memset(tmp, 0, (size_t) numToInsert * st);
	for (k = 0; k < numToInsert && ok; k++) {
	    /* une case ne contient que des éléments valides pour le type */
	    ok = TclTypeValidate(NULL, b->et, TCL_ANNOT_CONTENT(insertObjs[k]),
		    -1, NULL) == TCL_OK
		    && EncodeNode(b->lay->node, 0, insertObjs[k],
			    tmp + (size_t) k * st, b->norm) >= 0;
	}
    }
    if (!ok) {
	/* repli : liste ordinaire */
	Tcl_Obj *l = CompactMaterialize(b), **ev;
	Tcl_Size ln;
	int rc;

	Tcl_Free(tmp);
	Tcl_IncrRefCount(l);
	rc = Tcl_ListObjReplace(interp, l, first, numToDelete, numToInsert,
		insertObjs);
	if (rc == TCL_OK) {
	    Tcl_ListObjGetElements(NULL, l, &ln, &ev);
	    Tcl_SetListObj(objPtr, ln, ev);
	}
	Tcl_DecrRefCount(l);
	return rc;
    }
    newn = n - numToDelete + numToInsert;
    if (b->refCount > 1 || newn > b->cap) {
	Tcl_Size cap = (newn > b->cap) ? (newn < 2 * b->cap ? 2 * b->cap : newn)
		: b->cap;
	Tcl_ObjInternalRep ir;

	nb = NewCompactBuf(b->et, b->lay, cap);
	nb->norm = b->norm;
	memcpy(nb->data, b->data, (size_t) first * st);
	if (numToInsert > 0) {
	    memcpy(nb->data + (size_t) first * st, tmp, (size_t) numToInsert * st);
	}
	memcpy(nb->data + (size_t) (first + numToInsert) * st,
		b->data + (size_t) (first + numToDelete) * st,
		(size_t) (n - first - numToDelete) * st);
	nb->n = newn;
	b->refCount++;			/* garder b pendant le remplacement */
	ir.twoPtrValue.ptr1 = nb;
	ir.twoPtrValue.ptr2 = NULL;
	Tcl_StoreInternalRep(objPtr, &compactListType, &ir);	/* libère b */
	ReleaseCompactBuf(b);
    } else {
	memmove(b->data + (size_t) (first + numToInsert) * st,
		b->data + (size_t) (first + numToDelete) * st,
		(size_t) (n - first - numToDelete) * st);
	if (numToInsert > 0) {
	    memcpy(b->data + (size_t) first * st, tmp, (size_t) numToInsert * st);
	}
	b->n = newn;
    }
    Tcl_Free(tmp);
    if (objPtr->typePtr == &compactListType) {
	CompactDropCache(objPtr);
    }
    TclInvalidateStringRep(objPtr);
    return TCL_OK;
}


/* --- vues d'éléments --- */

static Tcl_Obj *
NewCompactElemObj(TclCompactBuf *b, size_t cell, int node)
{
    CompactElem *ce = (CompactElem *) Tcl_Alloc(sizeof(CompactElem));
    Tcl_Obj *o = Tcl_NewObj();
    Tcl_ObjInternalRep ir;

    ce->b = b;
    b->refCount++;
    ce->cell = cell;
    ce->node = node;
    ce->cache = NULL;
    ir.twoPtrValue.ptr1 = ce;
    ir.twoPtrValue.ptr2 = NULL;
    Tcl_StoreInternalRep(o, &compactElemType, &ir);
    TclInvalidateStringRep(o);
    return o;
}

static void
FreeCompactElem(Tcl_Obj *objPtr)
{
    CompactElem *ce = COMPACT_ELEM(objPtr);

    if (ce->cache != NULL) {
	Tcl_DecrRefCount(ce->cache);
    }
    ReleaseCompactBuf(ce->b);
    Tcl_Free(ce);
    objPtr->typePtr = NULL;
}

static void
DupCompactElem(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr)
{
    CompactElem *src = COMPACT_ELEM(srcPtr);
    CompactElem *ce = (CompactElem *) Tcl_Alloc(sizeof(CompactElem));
    Tcl_ObjInternalRep ir;

    *ce = *src;
    ce->cache = NULL;
    ce->b->refCount++;
    ir.twoPtrValue.ptr1 = ce;
    ir.twoPtrValue.ptr2 = NULL;
    Tcl_StoreInternalRep(dupPtr, &compactElemType, &ir);
}

static void
UpdateStringOfCompactElem(Tcl_Obj *objPtr)
{
    CompactElem *ce = COMPACT_ELEM(objPtr);
    Tcl_DString ds;

    Tcl_DStringInit(&ds);
    FormatNode(ce->b->lay->node, ce->node, ce->b->data + ce->cell, &ds);
    Tcl_InitStringRep(objPtr, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);
}

static Tcl_Size
CompactElemLength(Tcl_Obj *objPtr)
{
    CompactElem *ce = COMPACT_ELEM(objPtr);

    return ce->b->lay->node[ce->node].nchild;
}

static int
CompactElemChild(CompactElem *ce, Tcl_Size k)
{
    const CNode *nodes = ce->b->lay->node;
    int j = ce->node + 1;
    Tcl_Size c;

    for (c = 0; c < k; c++) {
	j += nodes[j].nsub;
    }
    return j;
}

static int
CompactElemIndex(Tcl_Interp *interp, Tcl_Obj *objPtr, Tcl_Size index,
	Tcl_Obj **elemPtr)
{
    CompactElem *ce = COMPACT_ELEM(objPtr);

    (void) interp;
    if (index < 0 || index >= ce->b->lay->node[ce->node].nchild) {
	*elemPtr = NULL;
	return TCL_OK;
    }
    *elemPtr = CompactPart(ce->b, ce->cell, CompactElemChild(ce, index));
    return TCL_OK;
}

static int
CompactElemGetElements(Tcl_Interp *interp, Tcl_Obj *objPtr,
	Tcl_Size *objcPtr, Tcl_Obj ***objvPtr)
{
    CompactElem *ce = COMPACT_ELEM(objPtr);

    if (ce->cache == NULL) {
	Tcl_Size k, n = ce->b->lay->node[ce->node].nchild;

	ce->cache = Tcl_NewListObj(n, NULL);
	Tcl_IncrRefCount(ce->cache);
	for (k = 0; k < n; k++) {
	    Tcl_ListObjAppendElement(NULL, ce->cache,
		    CompactPart(ce->b, ce->cell, CompactElemChild(ce, k)));
	}
    }
    return Tcl_ListObjGetElements(interp, ce->cache, objcPtr, objvPtr);
}

/* Modifier une vue : elle devient une liste ordinaire. */
static int
CompactElemReplace(Tcl_Interp *interp, Tcl_Obj *objPtr, Tcl_Size first,
	Tcl_Size numToDelete, Tcl_Size numToInsert, Tcl_Obj *const insertObjs[])
{
    Tcl_Obj **ev, *l;
    Tcl_Size n;
    int rc;

    if (CompactElemGetElements(interp, objPtr, &n, &ev) != TCL_OK) {
	return TCL_ERROR;
    }
    l = Tcl_NewListObj(n, ev);
    Tcl_IncrRefCount(l);
    rc = Tcl_ListObjReplace(interp, l, first, numToDelete, numToInsert,
	    insertObjs);
    if (rc == TCL_OK) {
	Tcl_ListObjGetElements(NULL, l, &n, &ev);
	Tcl_SetListObj(objPtr, n, ev);
    }
    Tcl_DecrRefCount(l);
    return rc;
}

/* --- interface --- */

int
TclIsCompact(Tcl_Obj *v)
{
    return v != NULL && v->typePtr == &compactListType;
}

int
TclIsCompactElem(Tcl_Obj *v)
{
    return v != NULL && v->typePtr == &compactElemType;
}

int
TclCompactHolds(Tcl_Obj *v, const TclType *et, Tcl_Size *nPtr)
{
    if (v == NULL || v->typePtr != &compactListType
	    || COMPACT_BUF(v)->et != et) {
	return 0;
    }
    if (nPtr != NULL) {
	*nPtr = COMPACT_BUF(v)->n;
    }
    return 1;
}

/*
 * Le champ idx d'une vue sur une case (produit en cases), s'il est un
 * nombre : lu dans la case, objet neuf (refcount 0) ; NULL sinon (pas une
 * vue, champ hors bornes, champ produit).
 */
Tcl_Obj *
TclCompactFieldNumber(Tcl_Obj *o, Tcl_Size idx)
{
    CompactElem *ce;
    const CNode *nd;

    if (o == NULL || o->typePtr != &compactElemType) {
	return NULL;
    }
    ce = COMPACT_ELEM(o);
    if (idx < 0 || idx >= ce->b->lay->node[ce->node].nchild) {
	return NULL;
    }
    nd = &ce->b->lay->node[CompactElemChild(ce, idx)];
    if (nd->kind == CN_PROD) {
	return NULL;
    }
    return DecodeScalar(nd, ce->b->data + ce->cell + nd->off);
}

Tcl_Obj *
TclCompactElement(Tcl_Obj *v, Tcl_Size i)
{
    Tcl_Obj *e = NULL;

    CompactIndex(NULL, v, i, &e);
    return e;
}


/*
 * Lecture directe d'une partie par chemin : l'élément idx, puis les champs
 * names[0..nnames-1] (des produits). Rend le contenu neuf (refcount 0) et
 * son type, ou NULL si le chemin ne se suit pas ici (le chemin ordinaire
 * dira l'erreur). *usedPtr : nombre de noms consommés (tous).
 */
/* Le noeud (et son type) désigné par names dans une case du tampon b ;
 * -1 si le chemin ne se suit pas ici. */
static int
CompactWalkFrom(TclCompactBuf *b, int i, TclType *t, Tcl_Size nnames,
	const char *const names[], TclType **tpPtr)
{
    Tcl_Size k;

    for (k = 0; k < nnames; k++) {
	const CNode *nd = &b->lay->node[i];
	Tcl_Size f, c;
	int j;

	if (nd->kind != CN_PROD) {
	    return -1;
	}
	if (nd->isList && t->kind == TCLT_LIST) {
	    /* un rang écrit en chiffres ; le reste (end, i+1...) au chemin
	     * ordinaire */
	    const char *q = names[k];

	    if (*q == '\0' || (q[0] == '0' && q[1] != '\0')) {
		return -1;
	    }
	    for (f = 0; *q; q++) {
		if (*q < '0' || *q > '9' || f > nd->nchild) {
		    return -1;
		}
		f = f * 10 + (*q - '0');
	    }
	} else if (!nd->isList && t->kind == TCLT_PRODUCT) {
	    f = TclTypeFieldIndex(t, names[k]);
	} else {
	    return -1;
	}
	if (f < 0 || f >= nd->nchild) {
	    return -1;
	}
	for (j = i + 1, c = 0; c < f; c++) {
	    j += b->lay->node[j].nsub;
	}
	i = j;
	t = (t->kind == TCLT_LIST) ? t->elem : t->fields[f].type;
    }
    *tpPtr = t;
    return i;
}

static int
CompactWalk(TclCompactBuf *b, Tcl_Size nnames, const char *const names[],
	TclType **tpPtr)
{
    return CompactWalkFrom(b, 0, b->et, nnames, names, tpPtr);
}

/*
 * Idem depuis une vue sur une case (une ligne d'une matrice, une matrice
 * d'un lot), vue typée vt : la partie names lue dans la case (contenu neuf,
 * refcount 0, et son type), ou NULL.
 */
Tcl_Obj *
TclCompactViewReadPath(Tcl_Obj *view, TclType *vt, Tcl_Size nnames,
	const char *const names[], TclType **tpPtr)
{
    CompactElem *ce;
    int i;

    if (view == NULL || view->typePtr != &compactElemType || nnames < 1) {
	return NULL;
    }
    ce = COMPACT_ELEM(view);
    i = CompactWalkFrom(ce->b, ce->node, vt, nnames, names, tpPtr);
    if (i < 0) {
	return NULL;
    }
    return DecodeNode(ce->b->lay->node, &i, ce->b->data + ce->cell);
}

Tcl_Obj *
TclCompactReadPath(Tcl_Obj *v, Tcl_Size idx, Tcl_Size nnames,
	const char *const names[], TclType **tpPtr)
{
    TclCompactBuf *b;
    int i;

    if (v->typePtr != &compactListType) {
	return NULL;
    }
    b = COMPACT_BUF(v);
    if (idx < 0 || idx >= b->n
	    || (i = CompactWalk(b, nnames, names, tpPtr)) < 0) {
	return NULL;
    }
    return DecodeNode(b->lay->node, &i, b->data + (size_t) idx * b->stride);
}

/*
 * L(idx.names...) = value écrit sur place dans la case, quand la partie
 * est un scalaire et que rien d'autre ne voit ces cases : V non partagé,
 * son tampon non plus (ni copie, ni vue vivante). value doit être un nombre
 * nu que le type de la partie accepte, et (sauf type {-compact}) sous sa
 * forme canonique. 1 : écrit (chaîne de V invalidée) ; 0 : rien n'a changé,
 * le chemin ordinaire s'en charge.
 */
int
TclCompactWriteScalar(Tcl_Obj *v, Tcl_Size idx, Tcl_Size nnames,
	const char *const names[], Tcl_Obj *value)
{
    TclCompactBuf *b;
    TclType *t;
    const CNode *nd;
    unsigned char tmp[8];
    int i;

    if (v->typePtr != &compactListType || Tcl_IsShared(v)
	    || TclIsAnnotated(value)) {
	return 0;
    }
    b = COMPACT_BUF(v);
    if (idx < 0 || idx >= b->n
	    || (i = CompactWalk(b, nnames, names, &t)) < 0) {
	return 0;
    }
    nd = &b->lay->node[i];
    if (nd->kind == CN_PROD
	    || TclTypeValidate(NULL, t, value, -1, NULL) != TCL_OK) {
	return 0;
    }
    if (b->refCount > 1) {
	CompactDropCache(v);		/* ses vues tiennent le tampon */
	if (b->refCount > 1) {
	    return 0;
	}
    }
    if (!EncodeScalar(nd, value, tmp, b->norm)) {
	return 0;
    }
    memcpy(b->data + (size_t) idx * b->stride + nd->off, tmp,
	    (size_t) nd->size);
    CompactDropCache(v);
    TclInvalidateStringRep(v);
    return 1;
}

/* Contenu compact (objet neuf) d'une liste list.T, ou NULL. */
Tcl_Obj *
TclCompactFrom(TclType *ltp, Tcl_Obj *content)
{
    TclCompactLayout *lay;
    TclCompactBuf *b;
    Tcl_Obj **ev, *v;
    Tcl_Size n, k;
    Tcl_ObjInternalRep ir;

    int wanted;

    if (ltp == NULL || ltp->kind != TCLT_LIST || ltp->elem == NULL) {
	return NULL;
    }
    /* type def {-compact}L (la liste) ou {-compact}P (son élément) */
    wanted = ltp->compactWanted || ltp->elem->compactWanted;
    if (!(compactOn || wanted) || content->typePtr == &compactListType
	    || (lay = TclCompactLayoutOf(ltp->elem)) == NULL
	    || Tcl_ListObjGetElements(NULL, content, &n, &ev) != TCL_OK) {
	return NULL;
    }
    b = NewCompactBuf(ltp->elem, lay, n);
    b->norm = wanted;
    for (k = 0; k < n; k++) {
	if (EncodeNode(lay->node, 0, ev[k], b->data + (size_t) k * lay->size,
		b->norm) < 0) {
	    ReleaseCompactBuf(b);
	    return NULL;
	}
    }
    b->n = n;
    v = Tcl_NewObj();
    ir.twoPtrValue.ptr1 = b;
    ir.twoPtrValue.ptr2 = NULL;
    Tcl_StoreInternalRep(v, &compactListType, &ir);
    TclInvalidateStringRep(v);		/* Tcl_NewObj a mis "" */
    if (content->bytes != NULL && !b->norm) {
	Tcl_InitStringRep(v, content->bytes, content->length);	/* EIAS */
    }
    return v;
}

/*
 * Pour les codecs binaires en C (type binary encode / decode) : la
 * disposition à plat des cases d'un type élément (ses scalaires en préordre :
 * décalage, taille, flottant ou non) et l'accès aux cases.
 */
int
TclCompactFlat(TclType *et, size_t *stridePtr, int max, size_t off[],
	int size[], int isFloat[])
{
    TclCompactLayout *lay = TclCompactLayoutOf(et);
    int i, k = 0;

    if (lay == NULL) {
	return -1;
    }
    for (i = 0; i < lay->nnodes; i++) {
	const CNode *nd = &lay->node[i];

	if (nd->kind == CN_PROD) {
	    continue;
	}
	if (k >= max) {
	    return -1;
	}
	off[k] = nd->off;
	size[k] = nd->size;
	isFloat[k] = (nd->kind == CN_FLT);
	k++;
    }
    *stridePtr = lay->size;
    return k;
}

/* Les cases de V si V est une liste compacte d'éléments et, sinon NULL. */
const unsigned char *
TclCompactData(Tcl_Obj *v, const TclType *et, Tcl_Size *nPtr)
{
    if (v == NULL || v->typePtr != &compactListType
	    || COMPACT_BUF(v)->et != et) {
	return NULL;
    }
    *nPtr = COMPACT_BUF(v)->n;
    return COMPACT_BUF(v)->data;
}

/*
 * Une liste compacte neuve (refcount 0) de n cases à zéro, à remplir par
 * *dataPtr, pour une liste du type ltp qui serait de toute façon en cases
 * (type ou élément {-compact}, ou forme compacte partout). NULL sinon.
 */
Tcl_Obj *
TclCompactNew(TclType *ltp, Tcl_Size n, unsigned char **dataPtr)
{
    TclCompactLayout *lay;
    TclCompactBuf *b;
    Tcl_Obj *v;
    Tcl_ObjInternalRep ir;
    int wanted;

    if (ltp == NULL || ltp->kind != TCLT_LIST || ltp->elem == NULL || n < 0) {
	return NULL;
    }
    wanted = ltp->compactWanted || ltp->elem->compactWanted;
    if (!(compactOn || wanted)
	    || (lay = TclCompactLayoutOf(ltp->elem)) == NULL) {
	return NULL;
    }
    b = NewCompactBuf(ltp->elem, lay, n);
    b->norm = wanted;
    b->n = n;
    v = Tcl_NewObj();
    ir.twoPtrValue.ptr1 = b;
    ir.twoPtrValue.ptr2 = NULL;
    Tcl_StoreInternalRep(v, &compactListType, &ir);
    TclInvalidateStringRep(v);		/* chaîne recalculée depuis les cases */
    *dataPtr = b->data;
    return v;
}

/* Deux dispositions de cases identiques (noms des champs exceptés). */
static int
SameCompactLayout(const TclCompactLayout *a, const TclCompactLayout *b)
{
    int i;

    if (a == b) {
	return 1;
    }
    if (a->nnodes != b->nnodes || a->size != b->size) {
	return 0;
    }
    for (i = 0; i < a->nnodes; i++) {
	if (a->node[i].kind != b->node[i].kind
		|| a->node[i].off != b->node[i].off
		|| (a->node[i].kind == CN_PROD
		    ? (a->node[i].nchild != b->node[i].nchild
			|| a->node[i].isList != b->node[i].isList)
		    : a->node[i].size != b->node[i].size)) {
	    return 0;
	}
    }
    return 1;
}

/*
 * Cast d'une liste vers le type liste target dont les éléments ont une
 * disposition en cases, quand le résultat serait en cases : contenu neuf
 * (refcount 0), ou NULL (le cast ordinaire s'en charge). Une source déjà en
 * cases de même disposition (list.point vers list.pointC) est copiée d'un
 * bloc ; sinon les éléments sont écrits directement dans les cases
 * (TclCompactFrom), sans objet intermédiaire.
 */
Tcl_Obj *
TclCompactCast(TclType *target, Tcl_Obj *content)
{
    TclCompactLayout *tl;
    TclCompactBuf *sb, *b;
    Tcl_Obj *v;
    Tcl_ObjInternalRep ir;
    int wanted;

    if (target == NULL || target->kind != TCLT_LIST || target->elem == NULL) {
	return NULL;
    }
    wanted = target->compactWanted || target->elem->compactWanted;
    if (!(compactOn || wanted)
	    || (tl = TclCompactLayoutOf(target->elem)) == NULL) {
	return NULL;
    }
    if (content->typePtr != &compactListType) {
	return TclCompactFrom(target, content);
    }
    sb = COMPACT_BUF(content);
    if (!SameCompactLayout(sb->lay, tl)
	    || (target->arity >= 0 && sb->n != target->arity)) {
	return NULL;
    }
    b = NewCompactBuf(target->elem, tl, sb->n);
    memcpy(b->data, sb->data, (size_t) sb->n * tl->size);
    b->n = sb->n;
    b->norm = wanted;
    v = Tcl_NewObj();
    ir.twoPtrValue.ptr1 = b;
    ir.twoPtrValue.ptr2 = NULL;
    Tcl_StoreInternalRep(v, &compactListType, &ir);
    TclInvalidateStringRep(v);		/* chaîne recalculée depuis les cases */
    return v;
}

/*
 * tcl::unsupported::compact          -> on | off
 * tcl::unsupported::compact on|off
 * tcl::unsupported::compact info V   -> {} ou {n N stride S elem T}
 * tcl::unsupported::compact layout T -> {size S align A fields {{chemin
 *                                       genre octets décalage} ...}}
 */
static void
LayoutFields(const CNode *nodes, int *ip, Tcl_DString *path, Tcl_Obj *out)
{
    const CNode *nd = &nodes[(*ip)++];
    Tcl_Size mark = Tcl_DStringLength(path);
    int k;

    if (nd->name != NULL || nd->index >= 0) {
	char num[TCL_INTEGER_SPACE];

	if (mark > 0) {
	    Tcl_DStringAppend(path, ".", 1);
	}
	if (nd->name != NULL) {
	    Tcl_DStringAppend(path, nd->name, TCL_AUTO_LENGTH);
	} else {
	    snprintf(num, sizeof(num), "%d", nd->index);
	    Tcl_DStringAppend(path, num, TCL_AUTO_LENGTH);
	}
    }
    if (nd->kind == CN_PROD) {
	for (k = 0; k < nd->nchild; k++) {
	    LayoutFields(nodes, ip, path, out);
	}
    } else {
	Tcl_Obj *f[4];
	static const char *const kinds[] = {"", "int", "uint", "float"};

	f[0] = Tcl_NewStringObj(Tcl_DStringValue(path), Tcl_DStringLength(path));
	f[1] = Tcl_NewStringObj(kinds[nd->kind], TCL_AUTO_LENGTH);
	f[2] = Tcl_NewIntObj(nd->size);
	f[3] = Tcl_NewWideIntObj((Tcl_WideInt) nd->off);
	Tcl_ListObjAppendElement(NULL, out, Tcl_NewListObj(4, f));
    }
    Tcl_DStringSetLength(path, mark);
}

static int
CompactObjCmd(void *clientData, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    const char *sub;

    (void) clientData;
    if (objc == 1) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(compactOn ? "on" : "off", -1));
	return TCL_OK;
    }
    sub = Tcl_GetString(objv[1]);
    if (objc == 2 && (strcmp(sub, "on") == 0 || strcmp(sub, "off") == 0)) {
	compactOn = (sub[1] == 'n');
	Tcl_SetObjResult(interp, objv[1]);
	return TCL_OK;
    }
    if (objc == 3 && strcmp(sub, "info") == 0) {
	Tcl_Obj *v = objv[2], *r;
	TclCompactBuf *b;

	if (TclIsAnnotated(v)) {
	    v = (Tcl_Obj *) v->internalRep.twoPtrValue.ptr1;
	}
	if (v->typePtr != &compactListType) {
	    Tcl_ResetResult(interp);
	    return TCL_OK;
	}
	b = COMPACT_BUF(v);
	r = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("n", 1));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewWideIntObj(b->n));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("stride", 6));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewWideIntObj((Tcl_WideInt) b->stride));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("elem", 4));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(b->elemType, -1));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("shared", 6));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewWideIntObj((Tcl_WideInt) b->refCount));
	Tcl_SetObjResult(interp, r);
	return TCL_OK;
    }
    if (objc == 3 && strcmp(sub, "layout") == 0) {
	Tcl_Size tl;
	const char *tt = Tcl_GetStringFromObj(objv[2], &tl);
	TclType *tp = TclTypeParse(interp, tt, tl);
	TclCompactLayout *lay;
	Tcl_Obj *r, *fields;
	Tcl_DString path;
	int i = 0;

	if (tp == NULL) {
	    return TCL_ERROR;
	}
	if ((lay = TclCompactLayoutOf(tp)) == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s has no compact layout: "
		    "only fixed-size scalars (int(s,n), int(u,n) with n = 8, 16, "
		    "32 or 64, double, double(32|64)), products of them and "
		    "fixed-size lists list(n) of them (at most 4096 parts per "
		    "element)",
		    tp->name));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "COMPACT", (char *) NULL);
	    return TCL_ERROR;
	}
	fields = Tcl_NewListObj(0, NULL);
	Tcl_DStringInit(&path);
	LayoutFields(lay->node, &i, &path, fields);
	Tcl_DStringFree(&path);
	r = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("size", 4));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewWideIntObj((Tcl_WideInt) lay->size));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("align", 5));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewWideIntObj((Tcl_WideInt) lay->align));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj("fields", 6));
	Tcl_ListObjAppendElement(NULL, r, fields);
	Tcl_SetObjResult(interp, r);
	return TCL_OK;
    }
    if ((objc == 4 || objc == 5) && strcmp(sub, "cells") == 0) {
	/*
	 * cells TYPE BYTES ?STRIDE? : une valeur du type liste TYPE dont les
	 * cases sont les octets donnés (un tableau de struct écrit par C,
	 * Tcl_AnnotatedCreateCells). Toujours en cases ; seuls les scalaires
	 * sont copiés (le remplissage reste à zéro).
	 */
	Tcl_Size tl, len, n, k;
	const char *tt = Tcl_GetStringFromObj(objv[2], &tl);
	TclType *tp = TclTypeParse(interp, tt, tl);
	TclCompactLayout *lay;
	TclCompactBuf *b;
	const unsigned char *bytes;
	Tcl_Obj *v;
	Tcl_ObjInternalRep ir;
	Tcl_WideInt stride;
	int i;

	if (tp == NULL) {
	    return TCL_ERROR;
	}
	if (tp->kind != TCLT_LIST || tp->elem == NULL
		|| (lay = TclCompactLayoutOf(tp->elem)) == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s is not a list whose "
		    "elements have a compact layout", tp->name));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "COMPACT", (char *) NULL);
	    return TCL_ERROR;
	}
	if (objc == 5) {
	    if (Tcl_GetWideIntFromObj(interp, objv[4], &stride) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if ((size_t) stride != lay->size) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("stride %" TCL_LL_MODIFIER
			"d does not match the cells of %s (%" TCL_Z_MODIFIER
			"u bytes)", (long long) stride, tp->elem->name,
			lay->size));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "COMPACT", (char *) NULL);
		return TCL_ERROR;
	    }
	}
	bytes = Tcl_GetBytesFromObj(interp, objv[3], &len);
	if (bytes == NULL) {
	    return TCL_ERROR;
	}
	if ((size_t) len % lay->size != 0) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("%" TCL_SIZE_MODIFIER
		    "d bytes is not a whole number of %s cells (%"
		    TCL_Z_MODIFIER "u bytes)", len, tp->elem->name, lay->size));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "COMPACT", (char *) NULL);
	    return TCL_ERROR;
	}
	n = (Tcl_Size) ((size_t) len / lay->size);
	if (tp->arity >= 0 && n != tp->arity) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s needs exactly %"
		    TCL_SIZE_MODIFIER "d elements but got %" TCL_SIZE_MODIFIER
		    "d", tp->name, tp->arity, n));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "LIST", (char *) NULL);
	    return TCL_ERROR;
	}
	b = NewCompactBuf(tp->elem, lay, n);
	for (k = 0; k < n; k++) {
	    size_t base = (size_t) k * lay->size;

	    for (i = 0; i < lay->nnodes; i++) {
		const CNode *nd = &lay->node[i];

		if (nd->kind != CN_PROD) {
		    memcpy(b->data + base + nd->off, bytes + base + nd->off,
			    (size_t) nd->size);
		}
	    }
	}
	b->n = n;
	b->norm = tp->compactWanted || tp->elem->compactWanted;
	v = Tcl_NewObj();
	ir.twoPtrValue.ptr1 = b;
	ir.twoPtrValue.ptr2 = NULL;
	Tcl_StoreInternalRep(v, &compactListType, &ir);
	TclInvalidateStringRep(v);
	Tcl_SetObjResult(interp, TclNewAnnotatedOwn(v,
		TypeRefObj(interp, tp, -1)));
	return TCL_OK;
    }
    Tcl_WrongNumArgs(interp, 1, objv,
	    "?on|off? | info value | layout type | cells type bytes ?stride?");
    return TCL_ERROR;
}

void
TclCompactInit(Tcl_Interp *interp)
{
    const char *env = getenv("TCL_TYPES_COMPACT");

    if (env != NULL && env[0] == '1') {
	compactOn = 1;			/* pour passer les tests en forme compacte */
    }
    Tcl_RegisterObjType(&compactListType);
    Tcl_RegisterObjType(&compactElemType);
    Tcl_RegisterObjType(&annotatedObjType);	/* pour tclCompact.h */
    Tcl_CreateObjCommand2(interp, "::tcl::unsupported::compact", CompactObjCmd,
	    NULL, NULL);
}
