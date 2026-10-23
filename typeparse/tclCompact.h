/*
 * tclCompact.h --
 *
 * FR : La forme compacte d'une liste typée, vue d'une extension C
 *      (prototype, TIP 764 ; tcl::unsupported::compact on).
 *
 *      Une liste list.T, où T est un scalaire à taille fixe (int(s,n),
 *      int(u,n) avec n = 8, 16, 32, 64 ; double, double(32|64)) ou un produit
 *      de tels champs, peut être rangée en cases : n éléments de stride
 *      octets, disposés comme le struct C que produit `type c T` (alignement
 *      naturel). TclCompactViewOf rend cette vue, ou NULL si la valeur n'est
 *      pas sous forme compacte (forme ordinaire : la lire avec les fonctions
 *      de liste de Tcl).
 *
 *      Le pointeur data est valable tant que la valeur n'est ni modifiée ni
 *      convertie : pendant la commande qui la reçoit, sans rappeler Tcl sur
 *      elle. Vérifier elemType et stride avant de lire, puis lire en const.
 *
 *      Prototype : une fonction de stub (Tcl_AnnotatedCompact...) remplacera
 *      ce passage par les types d'objets enregistrés.
 *
 * EN : The compact form of a typed list, seen from a C extension (prototype).
 *      TclCompactViewOf returns the view, or NULL when the value is not in
 *      compact form. data is valid while the value is neither modified nor
 *      converted. Check elemType and stride before reading; read it as const.
 */
#ifndef TCL_COMPACT_H
#define TCL_COMPACT_H

#include <tcl.h>
#include <string.h>

typedef struct TclCompactView {
    unsigned char *data;	/* n cases de stride octets */
    Tcl_Size n;
    size_t stride;
    const char *elemType;	/* nom du type des éléments, ex. "point" */
} TclCompactView;

static inline const TclCompactView *
TclCompactViewOf(Tcl_Obj *obj)
{
    static const Tcl_ObjType *annotatedType = NULL, *compactType = NULL;

    if (compactType == NULL) {
	compactType = Tcl_GetObjType("compactlist");
	annotatedType = Tcl_GetObjType("annotated");
    }
    if (compactType == NULL) {
	return NULL;			/* Tcl sans forme compacte */
    }
    if (annotatedType != NULL && obj->typePtr == annotatedType) {
	obj = (Tcl_Obj *) obj->internalRep.twoPtrValue.ptr1;	/* le contenu V */
    }
    if (obj->typePtr != compactType) {
	return NULL;
    }
    return (const TclCompactView *) obj->internalRep.twoPtrValue.ptr1;
}

/* La vue, si ses éléments sont du type nommé et font size octets. */
static inline const TclCompactView *
TclCompactViewOfType(Tcl_Obj *obj, const char *elemType, size_t size)
{
    const TclCompactView *v = TclCompactViewOf(obj);

    if (v == NULL || v->stride != size || strcmp(v->elemType, elemType) != 0) {
	return NULL;
    }
    return v;
}

#endif /* TCL_COMPACT_H */
