/*
 * tclTypedVar.c --
 *
 *	Variables typées et commande "set" annotée (spécification §8, §9).
 *
 *	Le type d'une variable se lit dans la valeur qu'elle contient (un
 *	objet annoté). Le bit VAR_TYPED ne sert qu'à désactiver les chemins
 *	directs du bytecode. Il n'y a plus de table "Var* -> type".
 *
 *	Règles de typage :
 *	  1. Adoption   : une valeur typée garde son type ; une variable sans
 *	     type adopte celui de la valeur, qui devient son contrat.
 *	  2. Déclaration: {T}nom fixe ou remplace le contrat, puis stocke.
 *	  3. Contrat collant : écriture brute -> reçoit le type de la
 *	     variable ; écriture typée du même type -> passe ; autre type ->
 *	     erreur.
 *	  4. Type vide  : {}$x retire le type d'une valeur, {}nom libère le
 *	     contrat d'une variable ; unset le libère aussi.
 */

#include "tclTypes.h"

static void
Mismatch(Tcl_Interp *interp, const TclType *expected, const TclType *got)
{
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"type mismatch: expected %s but got %s (use an explicit cast)",
		expected->name, got->name));
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "MISMATCH", (char *) NULL);
    }
}

/*
 * Fabrique, à partir de "arg", une valeur annotée de type tp (objet neuf,
 * refcount 0) ou NULL. Sert aux écritures sous contrat.
 */
static Tcl_Obj *
MakeTyped(Tcl_Interp *interp, TclType *tp, Tcl_Obj *arg)
{
    int status, variant = -1, outVariant = -1;
    TclType *at;
    Tcl_Obj *content, *copy, *res;

    if (TclAnnotResolve(interp, arg, tp, &status, &at, &variant, &content)
	    != TCL_OK) {
	return NULL;
    }
    if (status == TCLA_TYPED) {
	if (TclTypeSame(at, tp)) {
	    return Tcl_DuplicateObj(arg);
	}
	if (!TclTypeAssignable(tp, at)) {
	    Mismatch(interp, tp, at);
	    return NULL;
	}
	/* Élargissement (int(u,8) -> int) : même contenu, type de la cible. */
    }
    if (status == TCLA_RAW) {
	if (!TclTypeAssignable(tp, at)) {
	    Mismatch(interp, tp, at);
	    return NULL;
	}
	outVariant = variant;		/* balise explicite, ou -1 */
    }
    copy = Tcl_DuplicateObj(content);
    /* Valide (arité, clés) et ajoute les valeurs par défaut des clés
     * manquantes : dict(x?=0). */
    if (TclTypeComplete(interp, tp, copy, outVariant, &variant) != TCL_OK) {
	TclDiscard(copy);
	return NULL;
    }
    res = TclNewAnnotated(interp, tp, variant, copy);
    TclDiscard(copy);
    return res;
}

/* --------------------------------------------------------------------
 * Chemin d'écriture : à appeler en tête de TclPtrSetVarIdx, varPtr étant
 * résolu (liens suivis). Remplace *valuePtrPtr par la valeur à stocker.
 * Si *tmpPtrPtr est non NULL au retour, l'appelant doit faire
 * Tcl_DecrRefCount(*tmpPtrPtr) APRÈS le stockage.
 * -------------------------------------------------------------------- */

int
TclTypedVarPrepareWrite(Tcl_Interp *interp, Var *varPtr,
	Tcl_Obj **valuePtrPtr, int *flagsPtr, Tcl_Obj **tmpPtrPtr)
{
    Tcl_Interp *errInterp = (*flagsPtr & TCL_LEAVE_ERR_MSG) ? interp : NULL;
    Tcl_Obj *value = *valuePtrPtr, *cur, *curContent = NULL, *res;
    Tcl_Obj *content;
    TclType *contract = NULL, *at;
    int variant = -1, status, avar;

    *tmpPtrPtr = NULL;
    if (varPtr->flags & VAR_TYPED) {
	cur = varPtr->value.objPtr;
	if (cur == NULL
		|| TclAnnotInfo(cur, &curContent, &contract, &variant, NULL)
			!= 1) {
	    varPtr->flags &= ~VAR_TYPED;	/* bit orphelin : on se répare */
	    contract = NULL;
	}
    }

    if (contract == NULL) {
	/* Variable sans type : adoption d'une valeur typée. */
	if (!TclIsAnnotated(value)) {
	    return TCL_OK;
	}
	if (TclAnnotResolve(interp, value, NULL, &status, &at, &avar, &content)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	if (status == TCLA_TYPED) {
	    return TCL_OK;			/* stockée telle quelle */
	}
	if (status == TCLA_EMPTY) {
	    res = Tcl_DuplicateObj(content);	/* {}$x : on retire le type */
	} else {
	    res = MakeTyped(interp, at, value);
	    if (res == NULL) {
		return TCL_ERROR;
	    }
	}
	Tcl_IncrRefCount(res);
	*valuePtrPtr = res;
	*tmpPtrPtr = res;
	return TCL_OK;
    }

    /* Variable typée : le contrat est le type de la valeur courante. */
    if (*flagsPtr & TCL_APPEND_VALUE) {
	/* append / lappend : valider le RÉSULTAT, pas le delta. */
	Tcl_Obj *add = value, *sum;
	int rc;

	if (TclAnnotResolve(interp, value, contract, &status, &at, &avar,
		&content) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (status != TCLA_PLAIN
		&& !(contract->kind == TCLT_LIST && contract->elem != NULL
			&& (*flagsPtr & TCL_LIST_ELEMENT)
			&& (TclTypeIsAny(contract->elem)
			    || (contract->elem->kind == TCLT_SUM
				&& status == TCLA_TYPED
				&& at == contract->elem)))) {
	    /* lappend à list.any ou à une liste de sommes : l'annotation
	     * (donc la variante) de l'élément est gardée. */
	    add = content;
	}
	sum = Tcl_DuplicateObj(curContent);
	if (*flagsPtr & TCL_LIST_ELEMENT) {
	    rc = Tcl_ListObjAppendElement(errInterp, sum, add);
	} else {
	    Tcl_AppendObjToObj(sum, add);
	    rc = TCL_OK;
	}
	if (rc != TCL_OK) {
	    TclDiscard(sum);
	    return TCL_ERROR;
	}
	res = MakeTyped(interp, contract, sum);
	TclDiscard(sum);
	if (res == NULL) {
	    return TCL_ERROR;
	}
	*flagsPtr &= ~(TCL_APPEND_VALUE | TCL_LIST_ELEMENT);
    } else {
	res = MakeTyped(interp, contract, value);
	if (res == NULL) {
	    return TCL_ERROR;
	}
    }
    Tcl_IncrRefCount(res);
    *valuePtrPtr = res;
    *tmpPtrPtr = res;
    return TCL_OK;
}

/* Après le stockage : le bit suit la valeur effectivement stockée. */
void
TclTypedVarAfterStore(Var *varPtr)
{
    Tcl_Obj *v = varPtr->value.objPtr;

    if (v != NULL && TclAnnotInfo(v, NULL, NULL, NULL, NULL) == 1) {
	varPtr->flags |= VAR_TYPED;
    } else {
	varPtr->flags &= ~VAR_TYPED;
    }
}

/* --------------------------------------------------------------------
 * Variable de boucle : on retire le type avant l'affectation, puisque la
 * variable va recevoir une nouvelle valeur. varPtr doit être résolu
 * (liens suivis). La valeur courante est remplacée par son contenu brut.
 * -------------------------------------------------------------------- */

void
TclTypedVarForget(Var *varPtr)
{
    Tcl_Obj *cur, *content = NULL;

    if (varPtr == NULL || !(varPtr->flags & VAR_TYPED)
	    || TclIsVarArray(varPtr)) {
	return;
    }
    cur = varPtr->value.objPtr;
    if (cur != NULL && TclAnnotInfo(cur, &content, NULL, NULL, NULL) == 1
	    && content != NULL) {
	Tcl_IncrRefCount(content);
	varPtr->value.objPtr = content;
	Tcl_DecrRefCount(cur);
    }
    varPtr->flags &= ~VAR_TYPED;
}

void
TclTypedVarForgetByName(Tcl_Interp *interp, Tcl_Obj *nameObj)
{
    Var *varPtr, *arrayPtr;

    varPtr = TclObjLookupVarEx(interp, nameObj, NULL, 0, "set", 0, 0,
	    &arrayPtr);
    if (varPtr == NULL || arrayPtr != NULL) {
	return;
    }
    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    TclTypedVarForget(varPtr);
}

/* --------------------------------------------------------------------
 * Construction de la valeur d'un "set" annoté
 * -------------------------------------------------------------------- */

/* Contenu validé (objet neuf) de arg pour le type tp, ou NULL. */
Tcl_Obj *
TclTypedConform(Tcl_Interp *interp, TclType *tp, Tcl_Obj *arg)
{
    int status, variant;
    TclType *at;
    Tcl_Obj *content, *copy;

    if (TclAnnotResolve(interp, arg, tp, &status, &at, &variant, &content)
	    != TCL_OK) {
	return NULL;
    }
    if (status == TCLA_RAW
	    && TclTypeValidate(interp, at, content, variant, NULL) != TCL_OK) {
	return NULL;
    }
    if ((status == TCLA_TYPED || status == TCLA_RAW)
	    && !TclTypeAssignable(tp, at)
	    && !TclTypeIsPlainString(tp) && !TclTypeIsAny(tp)) {
	Mismatch(interp, tp, at);
	return NULL;
    }
    copy = Tcl_DuplicateObj(content);
    if (TclTypeIsAny(tp)) {
	return copy;
    }
    /* Balise explicite d'une somme ({tEXt}...) : elle fixe la variante. */
    if (TclTypeComplete(interp, tp, copy,
	    ((status == TCLA_TYPED || status == TCLA_RAW) && at == tp
		    && tp->kind == TCLT_SUM) ? variant : -1, NULL) != TCL_OK) {
	TclDiscard(copy);
	return NULL;
    }
    return copy;
}

static void
WrongArgs(Tcl_Interp *interp, const char *what)
{
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("wrong # args: %s", what));
    Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
}

/*
 * Règle de dépliage (§9) : un argument sans annotation est un élément ;
 * un argument annoté d'une collection du même genre que la cible, et dont
 * le type n'est pas déjà celui d'un élément attendu, est déplié.
 */
static int
BuildList(Tcl_Interp *interp, TclType *tp, Tcl_Size n, Tcl_Obj *const a[],
	Tcl_Obj **contentPtr)
{
    TclObjVec vec = {NULL, 0, 0};
    Tcl_Size i, j, m;
    Tcl_Obj **e, *content, *c, *e0;
    TclType *at;
    int status, variant, rc = TCL_ERROR;

    for (i = 0; i < n; i++) {
	if (TclAnnotResolve(interp, a[i], tp->elem, &status, &at, &variant,
		&content) != TCL_OK) {
	    goto done;
	}
	if (TclTypeIsAny(tp->elem)) {
	    /* list.any : l'élément garde son annotation, sans dépliage. */
	    if (status == TCLA_TYPED || status == TCLA_RAW) {
		if (status == TCLA_RAW
			&& TclTypeValidate(interp, at, content, variant, NULL)
				!= TCL_OK) {
		    goto done;
		}
		c = Tcl_DuplicateObj(content);
		e0 = TclNewAnnotated(interp, at, variant, c);
		TclDiscard(c);
		if (e0 == NULL) {
		    goto done;
		}
		TclVecAdd(&vec, e0);
	    } else {
		TclVecAdd(&vec, Tcl_DuplicateObj(
			status == TCLA_EMPTY ? content : a[i]));
	    }
	    continue;
	}
	if ((status == TCLA_TYPED || status == TCLA_RAW)
		&& at->kind == TCLT_LIST && at != tp->elem) {
	    if (status == TCLA_RAW
		    && TclTypeValidate(interp, at, content, -1, NULL)
			    != TCL_OK) {
		goto done;
	    }
	    if (status == TCLA_TYPED && at->elem != NULL
		    && (tp->elem->kind == TCLT_INT || tp->elem->kind == TCLT_DOUBLE)
		    && at->elem->kind == tp->elem->kind
		    && !TclTypeAssignable(tp->elem, at->elem)) {
		/* Pas de rétrécissement caché : list<int> n'entre pas dans
		 * list.int(u,8), comme int n'entre pas dans int(u,8). */
		Mismatch(interp, tp, at);
		goto done;
	    }
	    if (Tcl_ListObjGetElements(interp, content, &m, &e) != TCL_OK) {
		goto done;
	    }
	    for (j = 0; j < m; j++) {
		c = Tcl_DuplicateObj(e[j]);
		if (TclTypeValidate(interp, tp->elem, c, -1, NULL) != TCL_OK) {
		    TclDiscard(c);
		    goto done;
		}
		TclVecAdd(&vec, c);
	    }
	} else {
	    c = TclTypedConform(interp, tp->elem, a[i]);
	    if (c == NULL) {
		goto done;
	    }
	    if (status == TCLA_RAW && at == tp->elem && variant >= 0
		    && tp->elem->kind == TCLT_SUM) {
		/* {i}5 dans une liste de sommes : la balise est conservée. */
		Tcl_Obj *w = TclNewAnnotated(interp, tp->elem, variant, c);

		TclDiscard(c);
		c = w;
	    }
	    TclVecAdd(&vec, c);
	}
    }
    *contentPtr = Tcl_NewListObj(vec.n, vec.v);
    rc = TCL_OK;

  done:
    TclVecFree(&vec);
    return rc;
}

static int
BuildDict(Tcl_Interp *interp, TclType *tp, Tcl_Size n, Tcl_Obj *const a[],
	Tcl_Obj **contentPtr)
{
    Tcl_Obj *dict = Tcl_NewDictObj(), *content, *k, *v;
    Tcl_Size i = 0;
    TclType *at;
    int status, variant;

    while (i < n) {
	if (TclAnnotResolve(interp, a[i], NULL, &status, &at, &variant,
		&content) != TCL_OK) {
	    goto fail;
	}
	if ((status == TCLA_TYPED || status == TCLA_RAW)
		&& at->kind == TCLT_DICT) {
	    Tcl_DictSearch search;
	    Tcl_Obj *dk, *dv;
	    int done;

	    if (status == TCLA_RAW
		    && TclTypeValidate(interp, at, content, -1, NULL)
			    != TCL_OK) {
		goto fail;
	    }
	    if (Tcl_DictObjFirst(interp, content, &search, &dk, &dv, &done)
		    != TCL_OK) {
		goto fail;
	    }
	    for (; !done; Tcl_DictObjNext(&search, &dk, &dv, &done)) {
		Tcl_Obj *kc = Tcl_DuplicateObj(dk), *vc = Tcl_DuplicateObj(dv);

		if (TclTypeValidate(interp, tp->key, kc, -1, NULL) != TCL_OK
			|| TclTypeValidate(interp, tp->elem, vc, -1, NULL)
				!= TCL_OK) {
		    TclDiscard(kc);
		    TclDiscard(vc);
		    Tcl_DictObjDone(&search);
		    goto fail;
		}
		Tcl_DictObjPut(interp, dict, kc, vc);
	    }
	    Tcl_DictObjDone(&search);
	    i++;
	    continue;
	}
	if (i + 1 >= n) {
	    WrongArgs(interp, "a dict needs an even number of elements");
	    goto fail;
	}
	k = TclTypedConform(interp, tp->key, a[i]);
	if (k == NULL) {
	    goto fail;
	}
	v = TclTypedConform(interp, tp->elem, a[i + 1]);
	if (v == NULL) {
	    TclDiscard(k);
	    goto fail;
	}
	Tcl_DictObjPut(interp, dict, k, v);
	i += 2;
    }
    *contentPtr = dict;
    return TCL_OK;

  fail:
    TclDiscard(dict);
    return TCL_ERROR;
}

/*
 * Deux lectures possibles pour n == nf arguments (n pair) : positionnelle
 * (une valeur par champ) ou nom/valeur (n/2 couples, champs omis par
 * défaut). On essaie les deux SANS rien modifier ni laisser de message :
 * si une seule est valide on la choisit ; si les deux le sont, c'est une
 * ambiguïté réelle ; si aucune, on garde la lecture positionnelle (son
 * message d'erreur est celui d'avant).
 */
static int
ProductTryPositional(Tcl_Interp *interp, TclType *tp, Tcl_Size n,
	Tcl_Obj *const a[])
{
    Tcl_Size i;
    Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);
    int ok = 1;

    for (i = 0; i < n && ok; i++) {
	Tcl_Obj *c = TclTypedConform(interp, tp->fields[i].type, a[i]);

	if (c == NULL) {
	    ok = 0;
	} else {
	    TclDiscard(c);
	}
    }
    (void) Tcl_RestoreInterpState(interp, st);
    return ok;
}

static int
ProductTryNameValue(Tcl_Interp *interp, TclType *tp, Tcl_Size n,
	Tcl_Obj *const a[])
{
    Tcl_Size nf = tp->nfields, i, idx;
    Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);
    int ok = 1;
    char seen[256];

    if (nf > (Tcl_Size) sizeof(seen)) {
	(void) Tcl_RestoreInterpState(interp, st);
	return 0;
    }
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < n / 2 && ok; i++) {
	Tcl_Obj *c;

	idx = TclTypeFieldIndex(tp, Tcl_GetString(a[2 * i]));
	if (idx < 0 || seen[idx]) {
	    ok = 0;
	    break;
	}
	seen[idx] = 1;
	c = TclTypedConform(interp, tp->fields[idx].type, a[2 * i + 1]);
	if (c == NULL) {
	    ok = 0;
	} else {
	    TclDiscard(c);
	}
    }
    for (i = 0; i < nf && ok; i++) {
	if (!seen[i] && tp->fields[i].defcmd == NULL
		&& tp->fields[i].defval == NULL) {
	    ok = 0;			/* champ omis sans valeur par défaut */
	}
    }
    (void) Tcl_RestoreInterpState(interp, st);
    return ok;
}

/*
 * {T.champ}valeur : un seul champ d'un produit, les autres prenant leur
 * défaut -- même écriture que {Val.i}5 pour la variante d'une somme.
 * Rend 1 (idx et *valuePtr remplis), 0 (ce n'est pas cette écriture) ou -1
 * (T.champ désigne un champ inexistant de T : message dans l'interp).
 */
static int
ProductFieldTag(Tcl_Interp *interp, TclType *tp, Tcl_Obj *arg,
	Tcl_Obj **valuePtr, Tcl_Size *idxPtr)
{
    Tcl_Obj *content, *raw = NULL;
    const char *text, *dot, *p;
    Tcl_Size len, idx;
    TclType *named;

    if (TclAnnotInfo(arg, &content, NULL, NULL, &raw) != 2 || raw == NULL) {
	return 0;
    }
    text = Tcl_GetStringFromObj(raw, &len);
    dot = NULL;
    for (p = text; p < text + len; p++) {
	if (*p == '.') {
	    dot = p;			/* dernier point : le nom du champ */
	}
    }
    if (dot == NULL || dot == text || dot + 1 >= text + len) {
	return 0;
    }
    if ((Tcl_Size) strlen(tp->name) == dot - text
	    && memcmp(text, tp->name, dot - text) == 0) {
	named = tp;
    } else {
	Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);

	named = TclTypeParse(interp, text, dot - text);
	(void) Tcl_RestoreInterpState(interp, st);
    }
    if (named != tp) {
	return 0;
    }
    idx = TclTypeFieldIndex(tp, dot + 1);
    if (idx < 0) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"unknown field \"%s\" in %s", dot + 1, tp->name));
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "FIELD", (char *) NULL);
	return -1;
    }
    *idxPtr = idx;
    *valuePtr = content;
    return 1;
}

static int
BuildProduct(Tcl_Interp *interp, TclType *tp, Tcl_Size n, Tcl_Obj *const a[],
	Tcl_Obj **contentPtr)
{
    Tcl_Size nf = tp->nfields, i, idx;
    Tcl_Obj **slot, *content;
    TclType *at;
    int status, variant, rc = TCL_ERROR, forceNameValue = 0;
    Tcl_Obj *tagArgs[2] = {NULL, NULL};

    slot = (Tcl_Obj **) Tcl_Alloc((nf ? nf : 1) * sizeof(Tcl_Obj *));
    memset(slot, 0, (nf ? nf : 1) * sizeof(Tcl_Obj *));

    if (n == 1 && TclAnnotResolve(interp, a[0], NULL, &status, &at, &variant,
	    &content) == TCL_OK && status == TCLA_TYPED && at == tp) {
	*contentPtr = Tcl_DuplicateObj(content);	/* copie d'un produit */
	rc = TCL_OK;
	goto done;
    }
    if (n == 1 && tp->kind == TCLT_PRODUCT) {
	Tcl_Size fidx;
	Tcl_Obj *fval;
	int ft = ProductFieldTag(interp, tp, a[0], &fval, &fidx);

	if (ft < 0) {
	    goto done;
	}
	if (ft > 0) {
	    /* {T.champ}valeur : lu comme "champ valeur", le reste par défaut. */
	    tagArgs[0] = Tcl_NewStringObj(Tcl_GetString(tp->fields[fidx].name),
		    TCL_AUTO_LENGTH);
	    tagArgs[1] = fval;
	    Tcl_IncrRefCount(tagArgs[0]);
	    a = tagArgs;
	    n = 2;
	    forceNameValue = 1;
	}
    }
    if (n == nf && n >= 2 && n % 2 == 0 && !forceNameValue) {
	int pos = ProductTryPositional(interp, tp, n, a);
	int nv = ProductTryNameValue(interp, tp, n, a);

	if (pos && nv) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "ambiguous arguments for %s: they read both as %"
		    TCL_SIZE_MODIFIER "d positional values and as name/value "
		    "pairs; name every field (name value ...) or change the "
		    "first value", tp->name, nf));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "AMBIGUOUS", (char *) NULL);
	    goto done;
	}
	forceNameValue = (!pos && nv);
    }
    if (n == nf && !forceNameValue) {
	for (i = 0; i < nf; i++) {
	    slot[i] = TclTypedConform(interp, tp->fields[i].type, a[i]);
	    if (slot[i] == NULL) {
		goto done;
	    }
	}
    } else if (nf > 0 && n % 2 == 0 && n <= 2 * nf) {
	/*
	 * Forme nom/valeur, éventuellement PARTIELLE : les champs omis
	 * prennent leur valeur par défaut ("=" dans la définition du type).
	 * Aucun argument : tous les champs sont par défaut.
	 */

	for (i = 0; i < n / 2; i++) {
	    idx = TclTypeFieldIndex(tp, Tcl_GetString(a[2 * i]));
	    if (idx < 0 || slot[idx] != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"%s field \"%s\" in %s", idx < 0 ? "unknown"
			: "duplicate", Tcl_GetString(a[2 * i]), tp->name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "FIELD", (char *) NULL);
		goto done;
	    }
	    slot[idx] = TclTypedConform(interp, tp->fields[idx].type, a[2 * i + 1]);
	    if (slot[idx] == NULL) {
		goto done;
	    }
	}
	for (i = 0; i < nf; i++) {
	    if (slot[i] != NULL) {
		continue;
	    }
	    if (tp->fields[i].defcmd != NULL) {
		/* défaut calculé : [script] tourne à chaque construction */
		char what[200];
		Tcl_Obj *res = NULL;

		snprintf(what, sizeof(what), "field \"%s\" of %s",
			Tcl_GetString(tp->fields[i].name), tp->name);
		if (TclTypeRunDefault(interp, what, tp->fields[i].defcmd, &res)
			!= TCL_OK) {
		    goto done;
		}
		Tcl_IncrRefCount(res);
		slot[i] = TclTypedConform(interp, tp->fields[i].type, res);
		Tcl_DecrRefCount(res);
		if (slot[i] == NULL) {
		    goto done;
		}
		continue;
	    }
	    if (tp->fields[i].defval == NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"field \"%s\" of %s has no value and no default",
			Tcl_GetString(tp->fields[i].name), tp->name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "FIELD", (char *) NULL);
		goto done;
	    }
	    slot[i] = TclTypedConform(interp, tp->fields[i].type,
		    tp->fields[i].defval);
	    if (slot[i] == NULL) {
		goto done;
	    }
	}
    } else {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"wrong # args: %s needs %" TCL_SIZE_MODIFIER "d values, or up "
		"to %" TCL_SIZE_MODIFIER "d name/value arguments (omitted "
		"fields take their default)", tp->name, nf, 2 * nf));
	Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
	goto done;
    }
    *contentPtr = Tcl_NewListObj(nf, slot);
    memset(slot, 0, nf * sizeof(Tcl_Obj *));	/* la liste en est propriétaire */
    rc = TCL_OK;

  done:
    for (i = 0; i < nf; i++) {
	if (slot[i] != NULL) {
	    TclDiscard(slot[i]);
	}
    }
    Tcl_Free(slot);
    if (tagArgs[0] != NULL) {
	Tcl_DecrRefCount(tagArgs[0]);
    }
    return rc;
}

static int
BuildSum(Tcl_Interp *interp, TclType *tp, int forced, Tcl_Size n,
	Tcl_Obj *const a[], Tcl_Obj **contentPtr, int *variantPtr)
{
    int status, variant;
    TclType *at;
    Tcl_Obj *content, *c;
    Tcl_Size i;

    if (n != 1) {
	WrongArgs(interp, "a sum value takes exactly one argument");
	return TCL_ERROR;
    }
    if (forced >= 0) {			/* set {U}x(d) 5 */
	c = TclTypedConform(interp, tp->fields[forced].type, a[0]);
	if (c == NULL) {
	    return TCL_ERROR;
	}
	*contentPtr = c;
	*variantPtr = forced;
	return TCL_OK;
    }
    if (TclAnnotResolve(interp, a[0], tp, &status, &at, &variant, &content)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    c = Tcl_DuplicateObj(content);
    if (status == TCLA_RAW && variant >= 0) {	/* set {U}x {d}5 */
	if (TclTypeValidate(interp, tp, c, variant, variantPtr) != TCL_OK) {
	    TclDiscard(c);
	    return TCL_ERROR;
	}
	*contentPtr = c;
	return TCL_OK;
    }
    if (status == TCLA_TYPED && (at == tp || (at->kind == TCLT_SUM
	    && TclTypeAssignable(tp, at)))) {	/* copie d'une somme */
	*variantPtr = variant;
	*contentPtr = c;
	return TCL_OK;
    }
    if (status == TCLA_TYPED || status == TCLA_RAW) {
	if (status == TCLA_RAW
		&& TclTypeValidate(interp, at, c, -1, NULL) != TCL_OK) {
	    TclDiscard(c);
	    return TCL_ERROR;
	}
	for (i = 0; i < tp->nfields; i++) {
	    if (tp->fields[i].type == at) {	/* valeur du type d'une variante */
		*variantPtr = (int) i;
		*contentPtr = c;
		return TCL_OK;
	    }
	}
	TclDiscard(c);
	Mismatch(interp, tp, at);
	return TCL_ERROR;
    }
    if (TclTypeValidate(interp, tp, c, -1, variantPtr) != TCL_OK) {
	TclDiscard(c);
	return TCL_ERROR;
    }
    *contentPtr = c;
    return TCL_OK;
}

static int
BuildValue(Tcl_Interp *interp, TclType *tp, int forced, Tcl_Size n,
	Tcl_Obj *const a[], Tcl_Obj **contentPtr, int *variantPtr)
{
    *variantPtr = -1;
    switch (tp->kind) {
    case TCLT_INT:
    case TCLT_DOUBLE:
    case TCLT_STRING:
    case TCLT_VAR:
	if (n != 1) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "wrong # args: should be \"set {%s}varName value\"",
		    tp->name));
	    Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
	    return TCL_ERROR;
	}
	*contentPtr = TclTypedConform(interp, tp, a[0]);
	return (*contentPtr != NULL) ? TCL_OK : TCL_ERROR;
    case TCLT_LIST:
	return BuildList(interp, tp, n, a, contentPtr);
    case TCLT_DICT:
	return BuildDict(interp, tp, n, a, contentPtr);
    case TCLT_PRODUCT:
	return BuildProduct(interp, tp, n, a, contentPtr);
    case TCLT_SUM:
	return BuildSum(interp, tp, forced, n, a, contentPtr, variantPtr);
    }
    return TCL_ERROR;
}

/* --------------------------------------------------------------------
 * set {T}nom args...  (appelée par Tcl_SetObjCmd quand objv[1] est
 * une annotation)
 * -------------------------------------------------------------------- */

int
TclSetAnnotatedCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *nameObj, *baseObj = NULL, *c = NULL, *valueA = NULL;
    Tcl_Obj *res;
    TclType *tp;
    Tcl_Size nargs = objc - 2;
    int status, avar, variant = -1, forced = -1, hadBit, rc = TCL_ERROR;
    const char *name;
    Var *varPtr, *arrayPtr = NULL;

    if (TclAnnotResolve(interp, objv[1], NULL, &status, &tp, &avar, &nameObj)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    name = Tcl_GetString(nameObj);

    /* Somme : set {U}x(d) 5 choisit la variante d comme un champ. */
    if (status == TCLA_RAW && tp->kind == TCLT_SUM) {
	size_t len = strlen(name);
	const char *open = strchr(name, '(');

	if (open != NULL && len > 0 && name[len - 1] == ')') {
	    char *tag = (char *) Tcl_Alloc(len);

	    memcpy(tag, open + 1, len - (open - name) - 2);
	    tag[len - (open - name) - 2] = '\0';
	    forced = (int) TclTypeFieldIndex(tp, tag);
	    Tcl_Free(tag);
	    if (forced < 0 || tp->fields[forced].type == NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"%s has no variant named in \"%s\"", tp->name, name));
		Tcl_SetErrorCode(interp, "TCL", "TYPE", "VARIANT", (char *) NULL);
		return TCL_ERROR;
	    }
	    baseObj = Tcl_NewStringObj(name, open - name);
	}
    }
    if (forced < 0 && status == TCLA_RAW && tp->kind == TCLT_SUM && avar >= 0) {
	forced = avar;			/* set {Val.s}x 7 */
    }
    if (baseObj == NULL) {
	baseObj = Tcl_NewStringObj(name, TCL_AUTO_LENGTH);
    }
    Tcl_IncrRefCount(baseObj);

    /* Valeur à stocker. */
    if (status == TCLA_EMPTY) {			/* set {}j 10 : sans type */
	int st2, av2;
	TclType *t2;
	Tcl_Obj *ct2;

	if (nargs != 1) {
	    WrongArgs(interp, "should be \"set {}varName value\"");
	    goto done;
	}
	if (TclAnnotResolve(interp, objv[2], NULL, &st2, &t2, &av2, &ct2)
		!= TCL_OK) {
	    goto done;
	}
	valueA = Tcl_DuplicateObj(ct2);		/* valeur ordinaire */
    } else {
	if (BuildValue(interp, tp, forced, nargs, objv + 2, &c, &variant)
		!= TCL_OK) {
	    goto done;
	}
	/*
	 * BuildList et BuildDict ne valident que les éléments : l'arité d'une
	 * liste et les clés d'un dictionnaire se vérifient sur le conteneur
	 * entier, et les clés manquantes reçoivent leur valeur par défaut.
	 * c est un objet privé (refcount 0), modifiable en place.
	 */
	if ((tp->kind == TCLT_LIST || tp->kind == TCLT_DICT)
		&& TclTypeComplete(interp, tp, c, -1, NULL) != TCL_OK) {
	    TclDiscard(c);
	    goto done;
	}
	valueA = TclNewAnnotated(interp, tp, variant, c);
	TclDiscard(c);
    }
    Tcl_IncrRefCount(valueA);

    /* La variable : scalaire seulement (v1), liens suivis. */
    varPtr = TclObjLookupVarEx(interp, baseObj, NULL, TCL_LEAVE_ERR_MSG,
	    "set", 1, 0, &arrayPtr);
    if (varPtr == NULL) {
	goto done;
    }
    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    if (arrayPtr != NULL || TclIsVarArray(varPtr)
	    || TclIsVarArrayElement(varPtr)) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"can't declare \"%s\" as typed: typed variables must be "
		"scalars", name));
	Tcl_SetErrorCode(interp, "TCL", "TYPE", "NOTSCALAR", (char *) NULL);
	goto done;
    }

    /*
     * Déclaration : le nouveau contrat REMPLACE l'ancien. On lève le bit
     * pour que l'écriture passe par la branche "adoption" de
     * TclTypedVarPrepareWrite, puis on le restaure en cas d'échec.
     * Après un échec, varPtr peut avoir été détruit par une trace ; à
     * durcir (relire la variable) si les traces sont à supporter.
     */
    hadBit = varPtr->flags & VAR_TYPED;
    varPtr->flags &= ~VAR_TYPED;
    res = Tcl_ObjSetVar2(interp, baseObj, NULL, valueA, TCL_LEAVE_ERR_MSG);
    if (res == NULL) {
	if (hadBit) {
	    varPtr->flags |= VAR_TYPED;
	}
	goto done;
    }
    Tcl_SetObjResult(interp, res);
    rc = TCL_OK;

  done:
    if (valueA != NULL) {
	Tcl_DecrRefCount(valueA);
    }
    Tcl_DecrRefCount(baseObj);
    return rc;
}

/*
 * ====================================================================
 * POINTS D'ACCROCHE DANS LE NOYAU (à confirmer par lecture de la source)
 * ====================================================================
 *
 *  1. tclInt.h : VAR_TYPED (bit libre) ; l'ajouter aux masques
 *     TclIsVarDirectWritable, TclIsVarDirectModifyable et
 *     TclIsVarDirectUnsettable.
 *  2. tclVar.c, TclPtrSetVarIdx : en tête,
 *	    rc = TclTypedVarPrepareWrite(interp, varPtr, &newValuePtr,
 *		    &flags, &tmp);
 *	    if (rc != TCL_OK) return NULL;
 *     après le stockage : TclTypedVarAfterStore(varPtr);
 *     puis  if (tmp) Tcl_DecrRefCount(tmp);
 *  3. tclVar.c, UnsetVarStruct : varPtr->flags &= ~VAR_TYPED.
 *  4. tclCmdMZ.c, Tcl_SetObjCmd, en tête :
 *	    if (objc >= 2) {
 *		if (TclIsAnnotated(objv[1])) {
 *		    return TclSetAnnotatedCmd(interp, objc, objv);
 *		}
 *		rc = TclSetPathCmd(interp, objc, objv);
 *		if (rc != TCL_CONTINUE) return rc;
 *	    }
 *  5. tclParse.c / tclCompile.c : mot annoté {expr}suffixe -> couple
 *     (type, suffixe) ; littéral : TclNewRawAnnotation + ajout au tableau
 *     de littéraux SANS hachage (TclAddLiteralObj ?) ; substitution :
 *     instruction qui enveloppe la valeur à l'exécution. TclCompileSetCmd
 *     refuse de compiler tout "set" dont un mot est annoté.
 *  6. Fonctions de conversion (nombres, booléens, listes, dicts, chaînes
 *     Unicode, octets) : TclAnnotUnwrap en tête.
 *  7. Lecture $x(chemin) là où Tcl lève "variable isn't array" : si la
 *     valeur est annotée, TclTypePathRead.
 *  8. Tcl_CreateInterp / Tcl_Init : TclTypesInit(interp).
 *
 * À VÉRIFIER dans la source de Tcl 9.0 : les masques TclIsVarDirect*,
 * un bit libre pour VAR_TYPED, TclObjLookupVarEx (signature, suivi des
 * liens), TclIsVarLink / TclIsVarArray / TclIsVarArrayElement,
 * TclInvalidateStringRep, TCL_OBJTYPE_V0, Tcl_StoreInternalRep,
 * Tcl_InitStringRep, Tcl_CreateObjCommand2, la stabilité de
 * compiledLocals pendant la vie d'un frame, et la disponibilité de
 * TclAddLiteralObj.
 */
