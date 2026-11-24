/*
 * tclTypesOps.h --
 *
 *	TIP 764 : spécialisations de types fournies par une bibliothèque
 *	externe (version simplifiée). Une extension chargée par « load »
 *	prend la table par Tcl_GetAssocData(interp, "tclTypesOps", NULL) et
 *	déclare ses spécialisations : une fonction match qui reconnaît les
 *	types visés, une priorité, une table d'opérations. Une opération
 *	absente, ou qui rend NULL / 0, laisse le chemin général : la
 *	spécialisation ne fait qu'aller plus vite, jamais autre chose (le mode
 *	TCL_TYPE_OPS=check le vérifie).
 */

#ifndef _TCLTYPESOPS
#define _TCLTYPESOPS

#include "tcl.h"

#define TCL_TYPE_OPS_VERSION 1

/* genres de type (TclTypesOpsApi.kind) */
enum {
    TCL_TYPE_KIND_INT, TCL_TYPE_KIND_DOUBLE, TCL_TYPE_KIND_STRING,
    TCL_TYPE_KIND_LIST, TCL_TYPE_KIND_DICT, TCL_TYPE_KIND_PRODUCT,
    TCL_TYPE_KIND_SUM, TCL_TYPE_KIND_VAR
};

#ifndef TCL_TYPES_INTERNAL
typedef struct TclType TclType;		/* opaque */

typedef struct TclTypeOps {
    int version;
    const char *name;
    /* l'élément I (vue liste) du contenu : une valeur typée neuve */
    Tcl_Obj *(*index)(const TclType *tp, Tcl_Obj *content, Tcl_Size i);
    /* le champ F de l'élément I d'une liste de produits */
    Tcl_Obj *(*elemField)(const TclType *tp, Tcl_Obj *content, Tcl_Size i,
	    Tcl_Size f);
    /* l'élément I en double : 1 si lu */
    int (*indexDouble)(const TclType *tp, Tcl_Obj *content, Tcl_Size i,
	    double *outPtr);
} TclTypeOps;

typedef struct TclTypeSpec {
    int version;
    const char *name;
    int priority;		/* la plus haute l'emporte ; intégrées : 0 */
    int (*match)(Tcl_Interp *interp, const TclType *tp, void *clientData);
    const TclTypeOps *ops;
    void *clientData;
} TclTypeSpec;
#endif

typedef struct TclTypesOpsApi {
    int version;
    int (*registerOps)(Tcl_Interp *interp, const TclTypeSpec *spec);
    int (*kind)(const TclType *tp);
    const char *(*name)(const TclType *tp);
    const TclType *(*elem)(const TclType *tp);
    Tcl_Size (*nfields)(const TclType *tp);
    const char *(*fieldName)(const TclType *tp, Tcl_Size f);
    const TclType *(*fieldType)(const TclType *tp, Tcl_Size f);
    /* une valeur typée de type TP autour de CONTENT (déjà conforme à TP) ;
     * CONTENT déjà typé : lui-même s'il est de type TP, sinon NULL */
    Tcl_Obj *(*wrap)(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *content);
} TclTypesOpsApi;

#endif /* _TCLTYPESOPS */
