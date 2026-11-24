/*
 * produits_ordinaires.c --
 *
 *	Exemple de bibliothèque de spécialisations (TIP 764, tclTypesOps.h).
 *	Les spécialisations intégrées lisent P(i.champ) dans les listes de
 *	produits EN CASES ; celle-ci le fait pour les listes de produits
 *	ordinaires (chaque élément une liste Tcl) dont les champs sont des
 *	nombres ou des chaînes. Elle compte ses appels (produits::appels).
 *
 *	load ./libproduits[info sharedlibextension]
 *	type ops list.pt	;# -> produits-ordinaires
 */

#include "tclTypesOps.h"

static const TclTypesOpsApi *api;
static Tcl_WideInt appels;

static int
Simple(const TclType *t)
{
    int k = api->kind(t);

    return k == TCL_TYPE_KIND_INT || k == TCL_TYPE_KIND_DOUBLE
	    || k == TCL_TYPE_KIND_STRING;
}

/* list.P, P un produit dont tous les champs sont simples */
static int
Match(Tcl_Interp *interp, const TclType *tp, void *cd)
{
    const TclType *p;
    Tcl_Size f, n;

    (void) interp;
    (void) cd;
    if (api->kind(tp) != TCL_TYPE_KIND_LIST || (p = api->elem(tp)) == NULL
	    || api->kind(p) != TCL_TYPE_KIND_PRODUCT
	    || (n = api->nfields(p)) == 0) {
	return 0;
    }
    for (f = 0; f < n; f++) {
	if (api->fieldType(p, f) == NULL || !Simple(api->fieldType(p, f))) {
	    return 0;
	}
    }
    return 1;
}

static Tcl_Obj *
ElemField(const TclType *tp, Tcl_Obj *content, Tcl_Size i, Tcl_Size f)
{
    Tcl_Obj *e = NULL, *c = NULL;
    const TclType *p = api->elem(tp);

    /* un élément liste ordinaire de nfields valeurs nues ; sinon (en
     * cases, typé, autre forme) le chemin général */
    if (Tcl_ListObjIndex(NULL, content, i, &e) != TCL_OK || e == NULL
	    || Tcl_ListObjIndex(NULL, e, f, &c) != TCL_OK || c == NULL) {
	return NULL;
    }
    {
	Tcl_Size n;

	if (Tcl_ListObjLength(NULL, e, &n) != TCL_OK
		|| n != api->nfields(p)) {
	    return NULL;
	}
    }
    appels++;
    return api->wrap(NULL, api->fieldType(p, f), c);
}

static const TclTypeOps ops = {
    TCL_TYPE_OPS_VERSION, "produits-ordinaires", NULL, ElemField, NULL
};

static const TclTypeSpec spec = {
    TCL_TYPE_OPS_VERSION, "produits-ordinaires", 10, Match, &ops, NULL
};

static int
AppelsCmd(void *cd, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    (void) cd; (void) objc; (void) objv;
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(appels));
    return TCL_OK;
}

DLLEXPORT int
Produits_Init(Tcl_Interp *interp)
{
    if (Tcl_InitStubs(interp, "9.0", 0) == NULL) {
	return TCL_ERROR;
    }
    api = (const TclTypesOpsApi *) Tcl_GetAssocData(interp, "tclTypesOps",
	    NULL);
    if (api == NULL || api->version != TCL_TYPE_OPS_VERSION) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"this Tcl has no type ops (TIP 764) of version 1", -1));
	return TCL_ERROR;
    }
    if (api->registerOps(interp, &spec) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_CreateObjCommand(interp, "produits::appels", AppelsCmd, NULL, NULL);
    return Tcl_PkgProvide(interp, "produits", "1.0");
}
