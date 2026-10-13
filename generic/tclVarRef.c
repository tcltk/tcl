/*
 * tclVarRef.c --
 *
 *	Résolution des liens « var » (type var) vers les variables Tcl : voir
 *	le contrat dans tclTypes.h. Un lien est le NOM CANONIQUE d'une
 *	variable :
 *
 *	  ::ns::x, ::a(k)	variable d'espace de noms (ou globale), tableau
 *	  frame:UID:x		variable locale d'un appel de procédure
 *
 *	Un appel de procédure reçoit un numéro (UID) la première fois qu'un
 *	lien vise l'une de ses variables locales. Tcl_PopCallFrame appelle
 *	TclVarRefFrameGone : l'appel disparaît du registre, et le lien devient
 *	périmé (erreur franche, jamais une autre variable : un UID n'est
 *	jamais réutilisé). Le registre est vide, donc inerte, tant qu'aucun
 *	lien ne vise une variable locale.
 */

#include "tclInt.h"
#include "tclTypes.h"

#define REG_KEY "tclTypesFrameRegistry"

typedef struct FrameReg {
    Tcl_HashTable byFrame;	/* CallFrame * -> UID */
    Tcl_HashTable byUid;	/* UID -> CallFrame * */
    Tcl_WideInt next;		/* prochain UID, jamais réutilisé */
} FrameReg;

static void
RegDelete(void *clientData, TCL_UNUSED(Tcl_Interp *))
{
    FrameReg *reg = (FrameReg *) clientData;

    Tcl_DeleteHashTable(&reg->byFrame);
    Tcl_DeleteHashTable(&reg->byUid);
    Tcl_Free(reg);
}

static FrameReg *
GetReg(Tcl_Interp *interp, int create)
{
    FrameReg *reg = (FrameReg *) Tcl_GetAssocData(interp, REG_KEY, NULL);

    if (reg == NULL && create) {
	reg = (FrameReg *) Tcl_Alloc(sizeof(FrameReg));
	Tcl_InitHashTable(&reg->byFrame, TCL_ONE_WORD_KEYS);
	Tcl_InitHashTable(&reg->byUid, TCL_ONE_WORD_KEYS);
	reg->next = 1;
	Tcl_SetAssocData(interp, REG_KEY, RegDelete, reg);
    }
    return reg;
}

/* Appelé par Tcl_PopCallFrame pour chaque appel qui se termine. */

void
TclVarRefFrameGone(Tcl_Interp *interp, CallFrame *framePtr)
{
    FrameReg *reg = GetReg(interp, 0);
    Tcl_HashEntry *hPtr;

    if (reg == NULL || reg->byFrame.numEntries == 0) {
	return;
    }
    hPtr = Tcl_FindHashEntry(&reg->byFrame, framePtr);
    if (hPtr != NULL) {
	Tcl_WideInt uid = (Tcl_WideInt) (ptrdiff_t) Tcl_GetHashValue(hPtr);
	Tcl_HashEntry *uPtr = Tcl_FindHashEntry(&reg->byUid,
		(void *) (ptrdiff_t) uid);

	if (uPtr != NULL) {
	    Tcl_DeleteHashEntry(uPtr);
	}
	Tcl_DeleteHashEntry(hPtr);
    }
}

static Tcl_WideInt
FrameUid(Tcl_Interp *interp, CallFrame *framePtr)
{
    FrameReg *reg = GetReg(interp, 1);
    Tcl_HashEntry *hPtr;
    int isNew;
    Tcl_WideInt uid;

    hPtr = Tcl_CreateHashEntry(&reg->byFrame, framePtr, &isNew);
    if (!isNew) {
	return (Tcl_WideInt) (ptrdiff_t) Tcl_GetHashValue(hPtr);
    }
    uid = reg->next++;
    Tcl_SetHashValue(hPtr, (void *) (ptrdiff_t) uid);
    hPtr = Tcl_CreateHashEntry(&reg->byUid, (void *) (ptrdiff_t) uid, &isNew);
    Tcl_SetHashValue(hPtr, framePtr);
    return uid;
}

/* --------------------------------------------------------------------
 * Un nom canonique décomposé
 * -------------------------------------------------------------------- */

typedef struct Ref {
    Tcl_Obj *part1;		/* nom (qualifié, ou local) */
    Tcl_Obj *part2;		/* index de tableau, ou NULL */
    CallFrame *frame;		/* appel visé (variable locale), ou NULL */
} Ref;

static void
RefFree(Ref *r)
{
    if (r->part1 != NULL) {
	Tcl_DecrRefCount(r->part1);
    }
    if (r->part2 != NULL) {
	Tcl_DecrRefCount(r->part2);
    }
}

/* « nom(index) » -> nom, index. Le nom est le début jusqu'à la première
 * parenthèse ouvrante, l'index finit à la dernière parenthèse. */

static void
SplitIndex(const char *s, Tcl_Size len, Tcl_Size *baseLen, const char **idx,
	Tcl_Size *idxLen)
{
    const char *lp;

    *baseLen = len;
    *idx = NULL;
    *idxLen = 0;
    if (len > 2 && s[len - 1] == ')' && (lp = memchr(s, '(', len)) != NULL
	    && lp > s) {
	*baseLen = lp - s;
	*idx = lp + 1;
	*idxLen = len - (lp - s) - 2;
    }
}

static int
RefParse(Tcl_Interp *interp, const char *canon, Ref *r)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Size len = (Tcl_Size) strlen(canon), baseLen, idxLen;
    const char *base = canon, *idx;
    FrameReg *reg;
    (void) iPtr;

    memset(r, 0, sizeof(*r));
    if (strncmp(canon, "frame:", 6) == 0) {
	const char *colon;
	Tcl_WideInt uid;
	Tcl_HashEntry *hPtr;
	char *end;

	uid = (Tcl_WideInt) strtoll(canon + 6, &end, 10);
	colon = end;
	if (*colon != ':') {
	    goto bad;
	}
	base = colon + 1;
	len = (Tcl_Size) strlen(base);
	reg = GetReg(interp, 0);
	hPtr = (reg == NULL) ? NULL : Tcl_FindHashEntry(&reg->byUid,
		(void *) (ptrdiff_t) uid);
	if (hPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "can't access \"%s\": the procedure call that owned this "
		    "variable has returned", canon));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "STALE", (char *) NULL);
	    return TCL_ERROR;
	}
	r->frame = (CallFrame *) Tcl_GetHashValue(hPtr);
    }
    SplitIndex(base, len, &baseLen, &idx, &idxLen);
    r->part1 = Tcl_NewStringObj(base, baseLen);
    Tcl_IncrRefCount(r->part1);
    if (idx != NULL) {
	r->part2 = Tcl_NewStringObj(idx, idxLen);
	Tcl_IncrRefCount(r->part2);
    }
    return TCL_OK;

  bad:
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad variable link \"%s\"", canon));
    Tcl_SetErrorCode(interp, "TCL", "TYPE", "LINK", (char *) NULL);
    return TCL_ERROR;
}

/* Exécute le corps avec varFramePtr = l'appel visé, comme uplevel. */

#define WITH_FRAME(interp, r, body) \
    do { \
	Interp *iPtr_ = (Interp *) (interp); \
	CallFrame *saved_ = iPtr_->varFramePtr; \
	if ((r)->frame != NULL) { \
	    iPtr_->varFramePtr = (r)->frame; \
	} \
	body; \
	iPtr_->varFramePtr = saved_; \
    } while (0)

/* --------------------------------------------------------------------
 * Le contrat de tclTypes.h
 * -------------------------------------------------------------------- */

static void
FullName(Tcl_Interp *interp, Var *varPtr, Tcl_Obj *out)
{
    Tcl_GetVariableFullName(interp, (Tcl_Var) varPtr, out);
    /* l'espace de noms global rend un nom sans « :: » */
    if (Tcl_GetString(out)[0] != ':') {
	Tcl_Obj *tmp = Tcl_NewStringObj("::", 2);

	Tcl_AppendObjToObj(tmp, out);
	Tcl_SetStringObj(out, Tcl_GetString(tmp), TCL_AUTO_LENGTH);
	Tcl_DecrRefCount(tmp);
    }
}

int
TclVarRefCanon(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_Obj **canonPtr)
{
    Interp *iPtr = (Interp *) interp;
    const char *name = Tcl_GetString(nameObj);
    Tcl_Size len = (Tcl_Size) strlen(name), baseLen, idxLen;
    const char *idx;
    Tcl_Obj *baseObj, *out;
    Var *varPtr, *arrayPtr = NULL;

    if (strncmp(name, "frame:", 6) == 0) {		/* déjà canonique */
	*canonPtr = Tcl_NewStringObj(name, len);
	return TCL_OK;
    }
    SplitIndex(name, len, &baseLen, &idx, &idxLen);
    if (baseLen == 0) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"a link needs a variable name", TCL_AUTO_LENGTH));
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "LINK", (char *) NULL);
	return TCL_ERROR;
    }
    baseObj = Tcl_NewStringObj(name, baseLen);
    Tcl_IncrRefCount(baseObj);
    out = Tcl_NewObj();

    varPtr = TclObjLookupVarEx(interp, baseObj, NULL, 0, NULL, 0, 0,
	    &arrayPtr);
    while (varPtr != NULL && TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;		/* upvar, global, variable */
    }
    if (varPtr != NULL && TclGetVarNsPtr(varPtr) != NULL) {
	FullName(interp, varPtr, out);		/* une variable d'espace de noms */
    } else if (strstr(name, "::") != NULL) {
	/* qualifié mais absente : relatif à l'espace de noms courant */
	if (name[0] == ':') {
	    Tcl_AppendToObj(out, name, baseLen);
	} else {
	    Namespace *nsPtr = iPtr->varFramePtr->nsPtr;

	    Tcl_AppendToObj(out, nsPtr->fullName, TCL_AUTO_LENGTH);
	    if (nsPtr != iPtr->globalNsPtr) {
		Tcl_AppendToObj(out, "::", 2);
	    }
	    Tcl_AppendToObj(out, name, baseLen);
	    if (Tcl_GetString(out)[0] != ':') {
		Tcl_Obj *tmp = Tcl_NewStringObj("::", 2);

		Tcl_AppendObjToObj(tmp, out);
		Tcl_SetStringObj(out, Tcl_GetString(tmp), TCL_AUTO_LENGTH);
		Tcl_DecrRefCount(tmp);
	    }
	}
    } else if (iPtr->varFramePtr->isProcCallFrame) {
	/* variable locale d'un appel : on le numérote */
	Tcl_WideInt uid = FrameUid(interp, iPtr->varFramePtr);

	Tcl_AppendPrintfToObj(out, "frame:%" TCL_LL_MODIFIER "d:", uid);
	Tcl_AppendToObj(out, name, baseLen);
    } else {
	/* absente, hors procédure : variable de l'espace de noms courant */
	Namespace *nsPtr = iPtr->varFramePtr->nsPtr;

	Tcl_AppendToObj(out, nsPtr->fullName, TCL_AUTO_LENGTH);
	if (nsPtr != iPtr->globalNsPtr) {
	    Tcl_AppendToObj(out, "::", 2);
	}
	if (Tcl_GetString(out)[0] != ':') {
	    Tcl_Obj *tmp = Tcl_NewStringObj("::", 2);

	    Tcl_AppendObjToObj(tmp, out);
	    Tcl_SetStringObj(out, Tcl_GetString(tmp), TCL_AUTO_LENGTH);
	    Tcl_DecrRefCount(tmp);
	}
	Tcl_AppendToObj(out, name, baseLen);
    }
    if (idx != NULL) {
	Tcl_AppendToObj(out, "(", 1);
	Tcl_AppendToObj(out, idx, idxLen);
	Tcl_AppendToObj(out, ")", 1);
    }
    Tcl_DecrRefCount(baseObj);
    *canonPtr = out;
    return TCL_OK;
}

int
TclVarRefGet(Tcl_Interp *interp, const char *canon, Tcl_Obj **valuePtr)
{
    Ref r;
    Tcl_Obj *v = NULL;

    if (RefParse(interp, canon, &r) != TCL_OK) {
	return TCL_ERROR;
    }
    WITH_FRAME(interp, &r,
	    v = Tcl_ObjGetVar2(interp, r.part1, r.part2, TCL_LEAVE_ERR_MSG));
    RefFree(&r);
    if (v == NULL) {
	return TCL_ERROR;
    }
    *valuePtr = v;
    return TCL_OK;
}

int
TclVarRefSet(Tcl_Interp *interp, const char *canon, Tcl_Obj *value)
{
    Ref r;
    Tcl_Obj *res = NULL;

    if (RefParse(interp, canon, &r) != TCL_OK) {
	return TCL_ERROR;
    }
    WITH_FRAME(interp, &r,
	    res = Tcl_ObjSetVar2(interp, r.part1, r.part2, value,
		    TCL_LEAVE_ERR_MSG));
    RefFree(&r);
    return (res == NULL) ? TCL_ERROR : TCL_OK;
}

TclType *
TclVarRefType(Tcl_Interp *interp, const char *canon)
{
    Ref r;
    Var *varPtr = NULL, *arrayPtr = NULL;
    TclType *tp = NULL;

    if (RefParse(interp, canon, &r) != TCL_OK) {
	Tcl_ResetResult(interp);
	return NULL;
    }
    WITH_FRAME(interp, &r,
	    varPtr = TclObjLookupVarEx(interp, r.part1, r.part2, 0, NULL, 0, 0,
		    &arrayPtr));
    while (varPtr != NULL && TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    if (varPtr != NULL && (varPtr->flags & VAR_TYPED)
	    && varPtr->value.objPtr != NULL) {
	TclAnnotInfo(varPtr->value.objPtr, NULL, &tp, NULL, NULL);
    }
    RefFree(&r);
    return tp;
}
