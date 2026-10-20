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
 * set {}y $x range la valeur sans rien résoudre : pendant cette écriture,
 * le chemin d'écriture ne doit pas appeler le résolveur. Compteur propre
 * au thread (un interpréteur ne quitte pas son thread).
 */
typedef struct {
    int suspended;
} SettleState;
static Tcl_ThreadDataKey settleKey;

static int
SettleSuspended(void)
{
    SettleState *st = (SettleState *) Tcl_GetThreadData(&settleKey,
	    sizeof(SettleState));

    return st->suspended > 0;
}

static void
SettleSuspend(int delta)
{
    SettleState *st = (SettleState *) Tcl_GetThreadData(&settleKey,
	    sizeof(SettleState));

    st->suspended += delta;
}

/*
 * Fabrique, à partir de "arg", une valeur annotée de type tp (objet neuf,
 * refcount 0) ou NULL. Sert aux écritures sous contrat.
 */
static Tcl_Obj *
MakeTyped(Tcl_Interp *interp, TclType *tp, Tcl_Obj *arg)
{
    int status, variant = -1, outVariant = -1, dropTop = 0;
    TclType *at;
    Tcl_Obj *content, *copy, *res, *pend, *rest;
    Tcl_Size np = 0;

    if (TclAnnotResolve(interp, arg, tp, &status, &at, &variant, &content)
	    != TCL_OK) {
	return NULL;
    }
    if (status == TCLA_TYPED) {
	if (TclTypeSame(at, tp)) {
	    return Tcl_DuplicateObj(arg);
	}
	/* Élargissement (int(u,8) -> int), ou nominal qui s'efface devant le
	 * générique (px -> int, dist -> mm) : même contenu, type du contrat. */
	if (!TclTypeEnters(tp, at)) {
	    Mismatch(interp, tp, at);
	    return NULL;
	}
    }
    if (status == TCLA_RAW) {
	if (!TclTypeEnters(tp, at)) {
	    Mismatch(interp, tp, at);
	    return NULL;
	}
	outVariant = variant;		/* balise explicite, ou -1 */
	dropTop = TclAnnotIsNoteOnly(arg);	/* {d}$v : la note est lue */
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
    /* La pile d'annotations non résolues suit la valeur (int@px). */
    pend = TclAnnotPending(arg);
    Tcl_IncrRefCount(pend);
    Tcl_ListObjLength(NULL, pend, &np);
    if (dropTop && np > 0) {
	Tcl_ListObjReplace(NULL, pend, np - 1, 1, 0, NULL);
	np--;
    }
    if (np > 0) {
	rest = TclAnnotWithPending(res, pend);
	if (rest != res) {
	    TclDiscard(res);
	    res = rest;
	}
    }
    Tcl_DecrRefCount(pend);
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
    if (varPtr->flags & VAR_CONTRACT) {
	cur = varPtr->value.objPtr;
	if (cur == NULL
		|| TclAnnotInfo(cur, &curContent, &contract, &variant, NULL)
			!= 1) {
	    varPtr->flags &= ~(VAR_TYPED|VAR_CONTRACT);	/* bit orphelin */
	    contract = NULL;
	}
    }

    /*
     * Le constructeur résout la pile de la valeur (TclAnnotSettle) : les
     * annotations qui se résolvent sont dépilées depuis la plus récente,
     * la plus récente donne le cast. set {}y $x l'a déjà fait sans rien
     * résoudre (SettleSuspended).
     */
    if (TclIsAnnotated(value) && !(*flagsPtr & TCL_APPEND_VALUE)
	    && !SettleSuspended()) {
	Tcl_Obj *settled;

	if (TclAnnotSettle(interp, value, contract, 0, &settled) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (settled != value) {
	    Tcl_IncrRefCount(settled);
	    value = settled;
	    *valuePtrPtr = settled;
	    *tmpPtrPtr = settled;
	}
    }

    if (contract == NULL) {
	/*
	 * Variable sans type : elle adopte le type de la valeur (s'il y en
	 * a un), qui devient son contrat ; la pile restante suit la valeur.
	 */
	(void) status;
	(void) at;
	(void) avar;
	(void) content;
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
	if (status == TCLA_PLAIN) {
	    add = content;		/* valeur ordinaire, ou annotation locale */
	} else if (!(contract->kind == TCLT_LIST && contract->elem != NULL
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
	    if (*tmpPtrPtr != NULL) {
		Tcl_DecrRefCount(*tmpPtrPtr);
		*tmpPtrPtr = NULL;
	    }
	    return TCL_ERROR;
	}
    }
    Tcl_IncrRefCount(res);
    if (*tmpPtrPtr != NULL) {
	Tcl_DecrRefCount(*tmpPtrPtr);	/* la valeur résolue, remplacée */
    }
    *valuePtrPtr = res;
    *tmpPtrPtr = res;
    return TCL_OK;
}

/*
 * Liaison d'un argument de procédure (tclProc.c) : c'est un constructeur.
 * La valeur reçue passe par le résolveur, puis le bit suit la valeur.
 */
int
TclTypedVarBindArg(Tcl_Interp *interp, Var *varPtr)
{
    Tcl_Obj *v = varPtr->value.objPtr, *settled;

    if (v != NULL && TclIsAnnotated(v)) {
	if (TclAnnotSettle(interp, v, NULL, 0, &settled) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (settled != v) {
	    Tcl_IncrRefCount(settled);
	    varPtr->value.objPtr = settled;
	    Tcl_DecrRefCount(v);
	}
    }
    TclTypedVarAfterStore(varPtr);
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
	varPtr->flags &= ~(VAR_TYPED|VAR_CONTRACT);
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
	    && !TclTypeEnters(tp, at)
	    && !TclTypeIsPlainString(tp) && !TclTypeIsAny(tp)) {
	Mismatch(interp, tp, at);
	return NULL;
    }
    if (status == TCLA_TYPED && tp->binLink != NULL
	    && tp->binLink->ref != NULL && at == tp->binLink->ref) {
	/* L'adresse d'un lien de frontière garde son annotation. */
	return Tcl_DuplicateObj(arg);
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
		TclDiscard(kc);		/* clé en double : non gardée */
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
	TclDiscard(k);			/* clé en double : non gardée */
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

/*
 * CheckDynCounts --
 *
 *	Un champ liste dont le nombre d'éléments est une expression sur d'autres
 *	champs (list($n).T) porte une contrainte entre champs : la valeur du
 *	produit doit la respecter dès sa construction, sans attendre l'encodage
 *	binaire. L'expression est évaluée avec les valeurs des champs
 *	qu'elle cite ($n...). Les affectations ultérieures champ par champ ne sont
 *	pas contrôlées (un état intermédiaire est légitime) : type binary vérifie
 *	à nouveau à l'écriture.
 *
 *	A list field whose element count is an expression over other fields
 *	(list($n).T) carries a constraint between fields: the product value must
 *	satisfy it as soon as it is built, without waiting for the binary
 *	encoding. Later field-by-field assignments are not checked (an intermediate
 *	state is legitimate); type binary checks again when writing.
 */

static int
CheckDynCounts(Tcl_Interp *interp, TclType *tp, Tcl_Obj **slot)
{
    Tcl_Size i, j, k, len, nn;
    Tcl_Obj *params, *lambda, *call[10], *refs[8];
    Tcl_WideInt want;
    const char *e, *q;
    char buf[128];

    for (i = 0; i < tp->nfields; i++) {
	TclType *ft = tp->fields[i].type;
	int ok = 1;

	if (ft->kind != TCLT_LIST || ft->binDyn == NULL
		|| ft->binDyn->kind != TCLDYN_EXPR || ft->binDyn->expr == NULL
		|| slot[i] == NULL) {
	    continue;
	}

	/* Les champs cités ($nom) : paramètres d'un apply, valeurs = champs. */
	nn = 0;
	params = Tcl_NewListObj(0, NULL);
	Tcl_IncrRefCount(params);
	for (e = ft->binDyn->expr; *e != '\0' && ok; e++) {
	    Tcl_Size nl, fi;

	    if (*e != '$') {
		continue;
	    }
	    for (q = e + 1; *q != '\0' && (isalnum(UCHAR(*q)) || *q == '_'); q++) {
		/* vide */
	    }
	    nl = q - e - 1;
	    if (nl <= 0 || nl >= (Tcl_Size) sizeof(buf)) {
		continue;
	    }
	    memcpy(buf, e + 1, nl);
	    buf[nl] = '\0';
	    e = q - 1;
	    fi = TclTypeFieldIndex(tp, buf);
	    if (fi < 0 || slot[fi] == NULL) {
		ok = 0;			/* pas un champ de ce produit */
		break;
	    }
	    for (k = 0; k < nn; k++) {
		Tcl_Obj *nm;

		Tcl_ListObjIndex(NULL, params, k, &nm);
		if (strcmp(Tcl_GetString(nm), buf) == 0) {
		    break;
		}
	    }
	    if (k < nn) {
		continue;
	    }
	    if (nn >= 8) {
		ok = 0;
		break;
	    }
	    Tcl_ListObjAppendElement(NULL, params, Tcl_NewStringObj(buf, nl));
	    refs[nn++] = slot[fi];
	}
	if (!ok || nn == 0) {
	    Tcl_DecrRefCount(params);
	    continue;			/* non contrôlable ici */
	}

	lambda = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(NULL, lambda, params);
	Tcl_ListObjAppendElement(NULL, lambda,
		Tcl_ObjPrintf("expr {%s}", ft->binDyn->expr));
	Tcl_DecrRefCount(params);
	call[0] = Tcl_NewStringObj("apply", -1);
	call[1] = lambda;
	for (j = 0; j < nn; j++) {
	    /* valeur nue : sans l'annotation du champ */
	    call[2 + j] = Tcl_NewStringObj(Tcl_GetString(refs[j]), -1);
	}
	len = 2 + nn;
	for (j = 0; j < len; j++) {
	    Tcl_IncrRefCount(call[j]);
	}
	k = Tcl_EvalObjv(interp, len, call, 0);
	for (j = 0; j < len; j++) {
	    Tcl_DecrRefCount(call[j]);
	}
	if (k != TCL_OK || Tcl_GetWideIntFromObj(NULL,
		Tcl_GetObjResult(interp), &want) != TCL_OK) {
	    Tcl_ResetResult(interp);	/* type binary jugera à l'écriture */
	    continue;
	}
	Tcl_ResetResult(interp);
	if (Tcl_ListObjLength(NULL, slot[i], &len) == TCL_OK
		&& (Tcl_WideInt) len != want) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "field \"%s\" of %s holds %" TCL_SIZE_MODIFIER "d "
		    "elements, but its size says %" TCL_LL_MODIFIER "d (%s)",
		    Tcl_GetString(tp->fields[i].name), tp->name, len,
		    (Tcl_WideInt) want, ft->binDyn->expr));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "FIELD", "SIZE",
		    (char *) NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

static int
BuildProductEx(Tcl_Interp *interp, TclType *tp, Tcl_Size n,
	Tcl_Obj *const a[], Tcl_Obj **contentPtr, int named)
{
    Tcl_Size nf = tp->nfields, i, idx;
    Tcl_Obj **slot, *content;
    TclType *at;
    int status, variant, rc = TCL_ERROR, forceNameValue = named;
    Tcl_Obj *tagArgs[2] = {NULL, NULL};

    slot = (Tcl_Obj **) Tcl_Alloc((nf ? nf : 1) * sizeof(Tcl_Obj *));
    memset(slot, 0, (nf ? nf : 1) * sizeof(Tcl_Obj *));

    if (named) {
	goto nameValue;
    }
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
    if (n == nf && !forceNameValue) {
	for (i = 0; i < nf; i++) {
	    slot[i] = TclTypedConform(interp, tp->fields[i].type, a[i]);
	    if (slot[i] == NULL) {
		goto done;
	    }
	}
    } else if (forceNameValue || n == 0) {
      nameValue:
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
		"wrong # args: %s needs %" TCL_SIZE_MODIFIER "d values; to "
		"name fields (the others take their default), write "
		"{field}value or set {%s}var(field,...) value...",
		tp->name, nf, tp->name));
	Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *) NULL);
	goto done;
    }
    if (CheckDynCounts(interp, tp, slot) != TCL_OK) {
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
BuildProduct(Tcl_Interp *interp, TclType *tp, Tcl_Size n, Tcl_Obj *const a[],
	Tcl_Obj **contentPtr)
{
    return BuildProductEx(interp, tp, n, a, contentPtr, 0);
}

/* Nom/valeur imposé (constructeur par chemin) : jamais positionnel. */
int
TclBuildProductNamed(Tcl_Interp *interp, TclType *tp, Tcl_Size n,
	Tcl_Obj *const a[], Tcl_Obj **contentPtr)
{
    return BuildProductEx(interp, tp, n, a, contentPtr, 1);
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
    Tcl_Obj *res, **sa = NULL, *pend = NULL;
    TclType *tp;
    Tcl_Size nargs = objc - 2, i, np = 0;
    int status, avar, variant = -1, forced = -1, hadBit, rc = TCL_ERROR;
    const char *name;
    char *ctorPath = NULL;
    Tcl_Obj **tagVals = NULL, **tagNames = NULL;
    int fieldTags = 0;
    Var *varPtr, *arrayPtr = NULL;

    if (TclAnnotResolve(interp, objv[1], NULL, &status, &tp, &avar, &nameObj)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (status != TCLA_EMPTY && tp == NULL) {
	/*
	 * Le nom d'une variable attend un TYPE : un nom inconnu n'y est pas
	 * une note, et une annotation locale ({-v}) n'y a pas de sens.
	 */
	Tcl_Obj *raw = NULL;
	Tcl_Size rl;
	const char *rt;

	TclAnnotInfo(objv[1], NULL, NULL, NULL, &raw);
	rt = (raw != NULL) ? Tcl_GetStringFromObj(raw, &rl) : "";
	if (raw == NULL) {
	    rl = 0;
	}
	if (TclTypeParse(interp, rt, rl) == NULL
		&& TclAnnotErrorCodeIs(interp, "BADCHAR")) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "local annotation {%s} cannot declare a variable: a "
		    "variable name takes a type", rt));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "LOCALANNOT",
		    (char *) NULL);
	} else if (Tcl_GetCharLength(Tcl_GetObjResult(interp)) == 0) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "unknown type \"%s\"", rt));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "UNKNOWN", (char *) NULL);
	}
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
    /* Produit : set {T}R(chemin) v... construit par les champs nommés. */
    if (baseObj == NULL && (status == TCLA_RAW || status == TCLA_TYPED)
	    && tp->kind == TCLT_PRODUCT) {
	size_t len = strlen(name);
	const char *open = strchr(name, '(');

	if (open != NULL && open != name && len > 0 && name[len - 1] == ')') {
	    ctorPath = (char *) Tcl_Alloc(len);
	    memcpy(ctorPath, open + 1, len - (open - name) - 2);
	    ctorPath[len - (open - name) - 2] = '\0';
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

    /*
     * Étiquettes de champ : set {paire}p {nom}jean {age}12. L'annotation en
     * attente la plus récente de CHAQUE argument nomme un champ du produit
     * (le contexte du constructeur passe avant les types globaux, comme le
     * nom d'une variante pour une somme) : construction nommée, ordre libre,
     * les autres champs à leur défaut. Aucun argument étiqueté : lecture
     * positionnelle. Un mélange est une erreur.
     */
    if (ctorPath == NULL && status != TCLA_EMPTY && tp->kind == TCLT_PRODUCT
	    && nargs >= 1) {
	Tcl_Size ntag = 0;

	tagVals = (Tcl_Obj **) Tcl_Alloc(nargs * sizeof(Tcl_Obj *));
	tagNames = (Tcl_Obj **) Tcl_Alloc(nargs * sizeof(Tcl_Obj *));
	for (i = 0; i < nargs; i++) {
	    Tcl_Obj *top = NULL, *v = NULL;
	    Tcl_Obj *pd = TclAnnotPending(objv[2 + i]);
	    Tcl_Size npd = 0;

	    Tcl_IncrRefCount(pd);
	    Tcl_ListObjLength(NULL, pd, &npd);
	    if (npd > 0) {
		Tcl_ListObjIndex(NULL, pd, npd - 1, &top);
	    }
	    if (top != NULL && TclTypeFieldIndex(tp, Tcl_GetString(top)) >= 0) {
		Tcl_Obj *t2 = NULL;

		v = TclAnnotPopTop(objv[2 + i], &t2);
	    }
	    tagVals[i] = v;
	    tagNames[i] = (v != NULL) ? Tcl_DuplicateObj(top) : NULL;
	    if (v != NULL) {
		Tcl_IncrRefCount(v);
		Tcl_IncrRefCount(tagNames[i]);
		ntag++;
	    }
	    Tcl_DecrRefCount(pd);
	}
	if (ntag == nargs) {
	    fieldTags = 1;
	} else if (ntag > 0) {
	    /* Une annotation inconnue à côté d'étiquettes : un champ mal
	     * nommé, plus probablement qu'un mélange voulu. */
	    for (i = 0; i < nargs; i++) {
		Tcl_Obj *pd, *top = NULL;
		Tcl_Size npd = 0;

		if (tagVals[i] != NULL) {
		    continue;
		}
		pd = TclAnnotPending(objv[2 + i]);
		Tcl_IncrRefCount(pd);
		Tcl_ListObjLength(NULL, pd, &npd);
		if (npd > 0) {
		    Tcl_ListObjIndex(NULL, pd, npd - 1, &top);
		}
		if (top != NULL && TclAnnotIsUnknown(interp, top)) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			    "unknown field \"%s\" in %s", Tcl_GetString(top),
			    tp->name));
		    Tcl_SetErrorCode(interp, "TCL", "TYPE", "FIELD",
			    (char *) NULL);
		    Tcl_DecrRefCount(pd);
		    goto done;
		}
		Tcl_DecrRefCount(pd);
	    }
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "can't mix values named by a field annotation ({field}value) "
		    "and positional values in a %s constructor: name every "
		    "field, or none", tp->name));
	    Tcl_SetErrorCode(interp, "TCL", "TYPE", "FIELD", (char *) NULL);
	    goto done;
	}
    }

    /* Valeur à stocker. */
    if (status == TCLA_EMPTY) {			/* set {}j 10 : sans type */
	if (nargs != 1) {
	    WrongArgs(interp, "should be \"set {}varName value\"");
	    goto done;
	}
	/* Le contrat est levé ; la valeur perd son type résolu et garde sa
	 * pile, sans que rien n'y soit résolu (le constructeur suivant le
	 * fera). */
	if (TclAnnotSettle(interp, objv[2], NULL, TCLA_SETTLE_KEEP, &valueA)
		!= TCL_OK) {
	    valueA = NULL;
	    goto done;
	}
	if (valueA == objv[2] && TclAnnotInfo(valueA, NULL, NULL, NULL,
		NULL) == 1) {
	    valueA = Tcl_DuplicateObj(TCL_ANNOT_CONTENT(objv[2]));
	}
    } else {
	/* Chaque valeur passe par le résolveur, dans le contexte du type
	 * déclaré quand il n'y en a qu'une. */
	sa = (Tcl_Obj **) Tcl_Alloc((nargs > 0 ? nargs : 1)
		* sizeof(Tcl_Obj *));
	for (i = 0; i < nargs; i++) {
	    Tcl_Obj *one;
	    const TclType *ctx = (nargs == 1) ? tp : NULL;

	    if (fieldTags) {		/* le contexte : le type du champ */
		ctx = tp->fields[TclTypeFieldIndex(tp,
			Tcl_GetString(tagNames[i]))].type;
	    }
	    if (TclAnnotSettle(interp, fieldTags ? tagVals[i] : objv[2 + i],
		    ctx, 0, &one) != TCL_OK) {
		nargs = i;		/* libérer seulement les précédents */
		goto done;
	    }
	    Tcl_IncrRefCount(one);
	    sa[i] = one;
	}
	if (nargs == 1 && !fieldTags) {
	    pend = TclAnnotPending(sa[0]);
	    Tcl_IncrRefCount(pend);
	    Tcl_ListObjLength(NULL, pend, &np);
	    if (np > 0 && tp->kind == TCLT_SUM
		    && TclAnnotInfo(sa[0], NULL, NULL, NULL, NULL) != 1) {
		Tcl_Obj *top = NULL;

		Tcl_ListObjIndex(NULL, pend, np - 1, &top);
		if (top != NULL && TclTypeFieldIndex(tp,
			Tcl_GetString(top)) >= 0) {
		    /* {circle}$x : la variante choisie est lue, pas gardée. */
		    Tcl_ListObjReplace(NULL, pend, np - 1, 1, 0, NULL);
		    np--;
		}
	    }
	}
	if (ctorPath != NULL) {
	    if (TclTypePathConstruct(interp, tp, ctorPath, nargs, sa, &c)
		    != TCL_OK) {
		goto done;
	    }
	} else if (fieldTags) {
	    Tcl_Obj **pairs = (Tcl_Obj **) Tcl_Alloc(2 * nargs
		    * sizeof(Tcl_Obj *));
	    int brc;

	    for (i = 0; i < nargs; i++) {
		pairs[2 * i] = tagNames[i];
		pairs[2 * i + 1] = sa[i];
	    }
	    brc = TclBuildProductNamed(interp, tp, 2 * nargs, pairs, &c);
	    Tcl_Free(pairs);
	    if (brc != TCL_OK) {
		goto done;
	    }
	} else if (BuildValue(interp, tp, forced, nargs, sa, &c, &variant)
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
	if (np > 0) {
	    Tcl_Obj *withP = TclAnnotWithPending(valueA, pend);

	    if (withP != valueA) {
		TclDiscard(valueA);
		valueA = withP;
	    }
	}
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
    hadBit = varPtr->flags & (VAR_TYPED|VAR_CONTRACT);
    varPtr->flags &= ~(VAR_TYPED|VAR_CONTRACT);
    SettleSuspend(1);			/* valueA est déjà résolue */
    res = Tcl_ObjSetVar2(interp, baseObj, NULL, valueA, TCL_LEAVE_ERR_MSG);
    SettleSuspend(-1);
    if (res == NULL) {
	varPtr->flags |= hadBit;
	goto done;
    }
    if (status != TCLA_EMPTY && (varPtr->flags & VAR_TYPED)) {
	varPtr->flags |= VAR_CONTRACT;	/* seul le nom donne un contrat */
    }
    Tcl_SetObjResult(interp, res);
    rc = TCL_OK;

  done:
    if (valueA != NULL) {
	Tcl_DecrRefCount(valueA);
    }
    if (sa != NULL) {
	for (i = 0; i < nargs; i++) {
	    Tcl_DecrRefCount(sa[i]);
	}
	Tcl_Free(sa);
    }
    if (pend != NULL) {
	Tcl_DecrRefCount(pend);
    }
    if (ctorPath != NULL) {
	Tcl_Free(ctorPath);
    }
    if (tagVals != NULL) {
	for (i = 0; i < objc - 2; i++) {
	    if (tagVals[i] != NULL) {
		Tcl_DecrRefCount(tagVals[i]);
		Tcl_DecrRefCount(tagNames[i]);
	    }
	}
	Tcl_Free(tagVals);
	Tcl_Free(tagNames);
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

/*
 * "x is T = v" dans expr : comme set {T}x, la déclaration donne le contrat.
 * typed est la valeur déjà annotée du type T.
 */
Tcl_Obj *
TclTypedVarDeclare(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_Obj *typed)
{
    Var *varPtr, *arrayPtr;
    Tcl_Obj *res;
    int hadBit;

    varPtr = TclObjLookupVarEx(interp, nameObj, NULL, TCL_LEAVE_ERR_MSG,
	    "set", 1, 0, &arrayPtr);
    if (varPtr == NULL) {
	return NULL;
    }
    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    hadBit = varPtr->flags & (VAR_TYPED|VAR_CONTRACT);
    varPtr->flags &= ~(VAR_TYPED|VAR_CONTRACT);
    res = Tcl_ObjSetVar2(interp, nameObj, NULL, typed, TCL_LEAVE_ERR_MSG);
    if (res == NULL) {
	varPtr->flags |= hadBit;
	return NULL;
    }
    if (varPtr->flags & VAR_TYPED) {
	varPtr->flags |= VAR_CONTRACT;
    }
    return res;
}

/*
 * Essai : redéclaration par un nom annoté ({T}M) dans toute commande qui
 * écrit (lappend, lset, incr, dict set...). La valeur courante est convertie
 * en T et la variable reçoit le contrat ; {}M lève le contrat.
 */
int
TclTypedVarRedeclare(Tcl_Interp *interp, Var *varPtr, Tcl_Obj *nameObj)
{
    Tcl_Obj *raw = NULL, *cur, *res, *content;
    TclType *tp;
    Tcl_Size len;
    const char *text;

    if (TclAnnotInfo(nameObj, NULL, NULL, NULL, &raw) != 2 || raw == NULL) {
	return TCL_OK;
    }
    while (TclIsVarLink(varPtr)) {
	varPtr = varPtr->value.linkPtr;
    }
    if (TclIsVarArray(varPtr)) {
	return TCL_OK;
    }
    text = Tcl_GetStringFromObj(raw, &len);
    cur = varPtr->value.objPtr;
    if (len == 0) {				/* {}M : contrat levé */
	varPtr->flags &= ~VAR_CONTRACT;
	if (cur != NULL && TclAnnotInfo(cur, &content, NULL, NULL, NULL) == 1) {
	    Tcl_IncrRefCount(content);
	    varPtr->value.objPtr = content;
	    Tcl_DecrRefCount(cur);
	}
	TclTypedVarAfterStore(varPtr);
	return TCL_OK;
    }
    tp = TclTypeParse(interp, text, len);
    if (tp == NULL) {
	return TCL_ERROR;
    }
    if (cur == NULL) {
	return TCL_OK;		/* variable neuve : pas de valeur à typer */
    }
    {
	/* Comme set M {T}$M puis contrat : un cast de la valeur courante. */
	Tcl_Obj *typed = NULL, *plainName = NULL;
	Tcl_InterpState st = Tcl_SaveInterpState(interp, TCL_OK);

	(void) res;
	TclAnnotInfo(nameObj, &plainName, NULL, NULL, NULL);
	/* Le nouveau contrat part du contenu : il remplace l'ancien type. */
	if (TclAnnotInfo(cur, &content, NULL, NULL, NULL) == 1) {
	    cur = content;
	}
	Tcl_IncrRefCount(cur);
	if (TclAnnotateValue(interp, raw, cur, &typed) != TCL_OK) {
	    Tcl_DecrRefCount(cur);
	    Tcl_DiscardInterpState(st);
	    return TCL_ERROR;
	}
	Tcl_IncrRefCount(typed);
	Tcl_DecrRefCount(cur);
	if (TclTypedVarDeclare(interp, plainName, typed) == NULL) {
	    Tcl_DecrRefCount(typed);
	    Tcl_DiscardInterpState(st);
	    return TCL_ERROR;
	}
	Tcl_DecrRefCount(typed);
	(void) Tcl_RestoreInterpState(interp, st);
    }
    return TCL_OK;
}
