/*
 * tclAnnotatedApi.h --
 *
 * FR : Les fonctions C prévues pour les valeurs annotées (TIP 764, planning :
 *      « API C publique — fonctions de stub à envisager »), écrites AUJOURD'HUI
 *      avec l'API publique de Tcl 9 : une extension s'écrit déjà avec leurs
 *      noms et leurs conventions. Quand elles seront de vraies fonctions de
 *      stub (tcl.decls), ce fichier se retire : rien d'autre ne change.
 *
 *      Conventions (celles du planning) : un type se passe en Tcl_Obj * qui
 *      porte son texte ("polygone", "list.point") ; TCL_OK ou TCL_ERROR,
 *      message dans interp ; un objet rendu par **resultPtr a un compte de
 *      références à 0, comme Tcl_NewObj ; un tableau de parties est emprunté.
 *
 *      Ce que la cale (shim) ne peut pas faire comme les vraies fonctions :
 *        - elle passe par l'interpréteur (type get, type cast,
 *          ::tcl::unsupported::content) : quelques µs par appel ;
 *        - Tcl_AnnotatedGetParts recopie les parties (le contenu « profond »)
 *          au lieu de les lire en place ; le tableau rendu reste valable
 *          jusqu'au 8e appel suivant dans le même thread, pas « autant que la
 *          valeur » ;
 *        - Tcl_AnnotatedGetCells lit la forme compacte par les types
 *          d'objets enregistrés (tclCompact.h) et compare le NOM du type des
 *          éléments ;
 *        - Tcl_AnnotatedCreateCells passe les octets par un objet tableau
 *          d'octets (une copie de plus que la vraie fonction).
 *
 * EN : The planned C functions for annotated values, written TODAY on top of
 *      Tcl 9's public API, so an extension already uses their names and
 *      conventions. When they become real stub functions, drop this file.
 *
 * Pourquoi des fonctions inline et pas des macros (#define Tcl_X(...)) : un
 * appel de fonction vérifie le type des arguments, évalue chacun une seule
 * fois et peut garder des variables locales ; une macro ne sert qu'à
 * renommer. Les vraies fonctions de stub, elles, SERONT des macros
 * (#define Tcl_AnnotatedGetType (tclStubsPtr->tcl_AnnotatedGetType) dans
 * tclDecls.h) : la garde ci-dessous évite le conflit de noms.
 */
#ifndef TCL_ANNOTATED_API_H
#define TCL_ANNOTATED_API_H

#include <tcl.h>
#include <string.h>

#if !defined(TCL_ANNOTATED_STUBS) && !defined(Tcl_AnnotatedGetType)
#include "tclCompact.h"

#define TCL_ANNOT_PLAIN	1	/* parties sans annotation */
#define TCL_ANNOT_DEEP	2	/* à toute profondeur */

/* (cale) commande Tcl à un ou deux mots fixes, puis un argument ; rend le
 * résultat avec une référence, ou NULL. */
static inline Tcl_Obj *
TclAnnotShimEval(Tcl_Interp *interp, const char *w1, const char *w2,
	Tcl_Obj *a, Tcl_Obj *b)
{
    Tcl_Obj *cmd[4], *res = NULL;
    int n = 0, i, fixed;

    cmd[n++] = Tcl_NewStringObj(w1, -1);
    if (w2 != NULL) {
	cmd[n++] = Tcl_NewStringObj(w2, -1);
    }
    fixed = n;
    cmd[n++] = a;
    if (b != NULL) {
	cmd[n++] = b;
    }
    for (i = 0; i < n; i++) {
	Tcl_IncrRefCount(cmd[i]);
    }
    if (Tcl_EvalObjv(interp, n, cmd, 0) == TCL_OK) {
	res = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(res);
	Tcl_ResetResult(interp);
    }
    for (i = 0; i < n; i++) {
	Tcl_DecrRefCount(cmd[i]);
    }
    (void) fixed;
    return res;
}

/* Type d'une valeur : *typePtr = son nom (refcount 0), NULL sans type. */
static inline int
Tcl_AnnotatedGetType(Tcl_Interp *interp, Tcl_Obj *valuePtr, Tcl_Obj **typePtr)
{
    Tcl_Obj *r = TclAnnotShimEval(interp, "type", "get", valuePtr, NULL);
    Tcl_Size len;

    if (r == NULL) {
	return TCL_ERROR;
    }
    Tcl_GetStringFromObj(r, &len);
    *typePtr = (len > 0) ? Tcl_DuplicateObj(r) : NULL;
    Tcl_DecrRefCount(r);
    return TCL_OK;
}

/* Construire une valeur typée en validant le contenu ({T}contenu). */
static inline int
Tcl_AnnotatedCreate(Tcl_Interp *interp, Tcl_Obj *typePtr, Tcl_Obj *contentPtr,
	Tcl_Obj **resultPtr)
{
    Tcl_Obj *r = TclAnnotShimEval(interp, "type", "cast", typePtr, contentPtr);

    if (r == NULL) {
	return TCL_ERROR;
    }
    *resultPtr = Tcl_DuplicateObj(r);	/* refcount 0 (partage le contenu) */
    Tcl_DecrRefCount(r);
    return TCL_OK;
}

/*
 * Construire une valeur du type liste TYPE (polyligne, list.point...) depuis
 * un tableau C de n éléments de stride octets, sans passer par le texte : la
 * valeur est rangée en cases (forme compacte), le tableau est copié. Le
 * type des éléments doit avoir une disposition en cases égale à la struct C
 * (stride = sa taille ; voir type {c!}export et tcl::unsupported::compact
 * layout) ; sinon TCL_ERROR et un message.
 * (cale : un appel à tcl::unsupported::compact cells, et une copie des
 * octets en plus.)
 */
static inline int
Tcl_AnnotatedCreateCells(Tcl_Interp *interp, Tcl_Obj *typePtr,
	const void *data, Tcl_Size n, size_t stride, Tcl_Obj **resultPtr)
{
    Tcl_Obj *cmd[5], *r;
    int i, rc;

    cmd[0] = Tcl_NewStringObj("::tcl::unsupported::compact", -1);
    cmd[1] = Tcl_NewStringObj("cells", -1);
    cmd[2] = typePtr;
    cmd[3] = Tcl_NewByteArrayObj((const unsigned char *) data,
	    (Tcl_Size) ((size_t) n * stride));
    cmd[4] = Tcl_NewWideIntObj((Tcl_WideInt) stride);
    for (i = 0; i < 5; i++) {
	Tcl_IncrRefCount(cmd[i]);
    }
    rc = Tcl_EvalObjv(interp, 5, cmd, 0);
    if (rc == TCL_OK) {
	r = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(r);
	Tcl_ResetResult(interp);
	*resultPtr = Tcl_DuplicateObj(r);	/* refcount 0 (cases partagées) */
	Tcl_DecrRefCount(r);
    }
    for (i = 0; i < 5; i++) {
	Tcl_DecrRefCount(cmd[i]);
    }
    return rc;
}

/* (cale) les contenus gardés pour Tcl_AnnotatedGetParts, par thread */
typedef struct TclAnnotShimHolder {
    Tcl_Obj *keep[8];
    int next, registered;
} TclAnnotShimHolder;

static inline void
TclAnnotShimRelease(void *cd)
{
    TclAnnotShimHolder *h = (TclAnnotShimHolder *) cd;
    int i;

    for (i = 0; i < 8; i++) {
	if (h->keep[i] != NULL) {
	    Tcl_DecrRefCount(h->keep[i]);
	    h->keep[i] = NULL;
	}
    }
}

/* Les parties d'un produit ou les éléments d'une collection ($P(*)). */
static inline int
Tcl_AnnotatedGetParts(Tcl_Interp *interp, Tcl_Obj *valuePtr, int flags,
	Tcl_Size *objcPtr, Tcl_Obj ***objvPtr)
{
    static Tcl_ThreadDataKey key;
    TclAnnotShimHolder *h = (TclAnnotShimHolder *)
	    Tcl_GetThreadData(&key, sizeof(TclAnnotShimHolder));
    Tcl_Obj *c;

    (void) flags;			/* la cale rend toujours le contenu nu */
    if (!h->registered) {
	Tcl_CreateThreadExitHandler(TclAnnotShimRelease, h);
	h->registered = 1;
    }
    c = TclAnnotShimEval(interp, "::tcl::unsupported::content", NULL,
	    valuePtr, NULL);
    if (c == NULL) {
	return TCL_ERROR;
    }
    if (h->keep[h->next] != NULL) {
	Tcl_DecrRefCount(h->keep[h->next]);
    }
    h->keep[h->next] = c;		/* garde le tableau en vie */
    h->next = (h->next + 1) % 8;
    return Tcl_ListObjGetElements(interp, c, objcPtr, objvPtr);
}

/*
 * (proposée) Les cases d'une liste compacte dont les éléments sont du type
 * elemType et font elemSize octets (le struct de `type {c!}export`) :
 * *dataPtr = les n cases, en lecture seule, valables tant que la valeur
 * n'est pas modifiée ; *dataPtr = NULL si la valeur n'est pas en cases
 * (la lire alors par Tcl_AnnotatedGetParts).
 */
static inline int
Tcl_AnnotatedGetCells(Tcl_Interp *interp, Tcl_Obj *valuePtr,
	Tcl_Obj *elemTypePtr, size_t elemSize, const void **dataPtr,
	Tcl_Size *nPtr)
{
    const TclCompactView *v = TclCompactViewOfType(valuePtr,
	    Tcl_GetString(elemTypePtr), elemSize);

    (void) interp;
    *dataPtr = (v != NULL) ? v->data : NULL;
    *nPtr = (v != NULL) ? v->n : 0;
    return TCL_OK;
}

#endif /* pas encore de vraies fonctions de stub */
#endif /* TCL_ANNOTATED_API_H */
