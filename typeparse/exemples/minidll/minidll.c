/*
 * minidll.c --
 *
 * FR : Une mini-extension Tcl (Tcl 9, API à stubs) : quelques commandes, plus
 *      une commande qui expose la structure Tcl_Obj en mémoire, pour la
 *      comparer à ce que décrit exemples/tcl_obj.tcl.
 * EN : A tiny Tcl 9 extension (stubs API): a few commands, plus one that
 *      exposes the in-memory Tcl_Obj, to compare with exemples/tcl_obj.tcl.
 *
 *   mini::add A B          somme de deux entiers / sum of two integers
 *   mini::reverse STR      retourne la chaîne / reverses a string
 *   mini::objinfo_raw V    liste : objAddr refCount bytesPtr length typePtr
 *                          typeName (voir minidll.tcl, qui l'annote en `objinfo`)
 *   mini::peek ADDR LEN    LEN octets à l'adresse ADDR du processus
 *                          (LECTURE de la mémoire du processus : à manier avec
 *                          précaution) / LEN bytes at process address ADDR
 */
#include <tcl.h>
#include <stdint.h>

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

static int
AddCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_WideInt a, b;
    UNUSED(cd);
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "a b");
	return TCL_ERROR;
    }
    if (Tcl_GetWideIntFromObj(interp, objv[1], &a) != TCL_OK
	    || Tcl_GetWideIntFromObj(interp, objv[2], &b) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(a + b));
    return TCL_OK;
}

static int
ReverseCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Size i, n;
    Tcl_Obj *r;
    UNUSED(cd);
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "string");
	return TCL_ERROR;
    }
    n = Tcl_GetCharLength(objv[1]);
    r = Tcl_NewObj();
    for (i = n - 1; i >= 0; i--) {
	Tcl_AppendObjToObj(r, Tcl_GetRange(objv[1], i, i));
    }
    Tcl_SetObjResult(interp, r);
    return TCL_OK;
}

static int
ObjInfoCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *o, *d;
    UNUSED(cd);
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "value");
	return TCL_ERROR;
    }
    o = objv[1];
    d = Tcl_NewListObj(0, NULL);
    /* FR : valeurs dans l'ordre des champs du type Tcl `objinfo`
     * EN : values in the field order of the Tcl type `objinfo` */
    Tcl_ListObjAppendElement(NULL, d, Tcl_NewWideIntObj((Tcl_WideInt)(uintptr_t) o));
    Tcl_ListObjAppendElement(NULL, d, Tcl_NewWideIntObj((Tcl_WideInt) o->refCount));
    Tcl_ListObjAppendElement(NULL, d, Tcl_NewWideIntObj((Tcl_WideInt)(uintptr_t) o->bytes));
    Tcl_ListObjAppendElement(NULL, d, Tcl_NewWideIntObj((Tcl_WideInt) o->length));
    Tcl_ListObjAppendElement(NULL, d, Tcl_NewWideIntObj((Tcl_WideInt)(uintptr_t) o->typePtr));
    Tcl_ListObjAppendElement(NULL, d, Tcl_NewStringObj(o->typePtr ? o->typePtr->name : "", -1));
    Tcl_SetObjResult(interp, d);
    return TCL_OK;
}

/*
 * FR : ANNOTATION DEPUIS C (non implémentée) -- Cette extension ne peut pas
 *      renvoyer directement une valeur annotée : le noyau n'expose aucune
 *      fonction publique pour cela (seul TclAnnotatedObjTypePtr existe, en-tête
 *      interne). Elle renvoie donc la liste des champs, et minidll.tcl annote.
 *      Avec une commande d'annotation dans la table des stubs, on écrirait :
 *
 *          // proposition de signature (à ajouter à tcl.decls)
 *          Tcl_Obj *Tcl_NewAnnotatedObj(Tcl_Interp *interp,
 *                  Tcl_Obj *valueObj,        // la valeur nue (ici : la liste)
 *                  Tcl_Obj *typeNameObj);    // "objinfo", "list.u8", ...
 *          // valide valueObj contre le type nommé, renvoie une valeur
 *          // annotée (refCount 0) ou NULL avec un message d'erreur.
 *
 *          // dans ObjInfoCmd, à la place de Tcl_SetObjResult(interp, d) :
 *          Tcl_Obj *a = Tcl_NewAnnotatedObj(interp, d,
 *                  Tcl_NewStringObj("objinfo", -1));
 *          if (a == NULL) return TCL_ERROR;
 *          Tcl_SetObjResult(interp, a);
 *
 *      Et pour lire le type d'une valeur : Tcl_GetAnnotation(valueObj) ->
 *      Tcl_Obj* (nom du type) ou NULL. Le type `objinfo` devrait exister
 *      (type def, côté Tcl) avant l'appel.
 *
 * EN : ANNOTATING FROM C (not implemented) -- This extension cannot return an
 *      annotated value directly: the core exposes no public function for it
 *      (only TclAnnotatedObjTypePtr, in an internal header). It returns the
 *      field list, and minidll.tcl annotates. With an annotation command in
 *      the stub table (signature above), ObjInfoCmd would end with
 *      Tcl_NewAnnotatedObj(interp, list, "objinfo") instead of returning the
 *      bare list. The type must already exist (type def, Tcl side).
 */

static int
PeekCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_WideInt addr, len;
    UNUSED(cd);
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "address length");
	return TCL_ERROR;
    }
    if (Tcl_GetWideIntFromObj(interp, objv[1], &addr) != TCL_OK
	    || Tcl_GetWideIntFromObj(interp, objv[2], &len) != TCL_OK) {
	return TCL_ERROR;
    }
    if (addr <= 0 || len < 0 || len > 1 << 20) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"bad address or length (max 1 MiB)", -1));
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(
	    (const unsigned char *)(uintptr_t) addr, (Tcl_Size) len));
    return TCL_OK;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int
Minidll_Init(Tcl_Interp *interp)
{
    if (Tcl_InitStubs(interp, "9.0", 0) == NULL) {
	return TCL_ERROR;
    }
    Tcl_CreateObjCommand2(interp, "::mini::add", AddCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::mini::reverse", ReverseCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::mini::objinfo_raw", ObjInfoCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::mini::peek", PeekCmd, NULL, NULL);
    return Tcl_PkgProvide(interp, "minidll", "1.0");
}
