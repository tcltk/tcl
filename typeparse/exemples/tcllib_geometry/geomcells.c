/*
 * geomcells.c -- translate et rotate de cases en cases, pour
 * comparaison.tcl (variantes 5 et 9) : une polyligne en cases
 * ({-compact} list.double, ::math::geometry::polylinec) lue en place par
 * Tcl_AnnotatedGetCells, le résultat rendu en cases par
 * Tcl_AnnotatedCreateCells. Les commandes translate et rotate de geom.c,
 * elles, rendent des listes d'objets (comme tcllib). Même arithmétique que
 * geom.c et que expr.
 *
 * EN : translate and rotate from cells to cells, for comparaison.tcl.
 *
 *   gcc -shared -fPIC -O2 -ffp-contract=off -DUSE_TCL_STUBS \
 *       -I<src>/generic -I<build> -I<src>/unix -I../.. \
 *       -o geomcells.so geomcells.c <build>/libtclstub.a
 *
 *   load geomcells.so Geomcells
 *   ::math::geometry::translateCells {dx dy} polylinec
 *   ::math::geometry::rotateCells angle polylinec   (degrés)
 */
#include <tcl.h>
#include <math.h>
#include <string.h>
#include "tclAnnotatedApi.h"

typedef struct { double x, y; } pt;

static int
GetCells(Tcl_Interp *interp, Tcl_Obj *o, const pt **p, Tcl_Size *n)
{
    Tcl_Obj *t = Tcl_NewStringObj("double", -1);
    const void *c = NULL;
    Tcl_Size k;

    Tcl_IncrRefCount(t);
    Tcl_AnnotatedGetCells(interp, o, t, sizeof(double), &c, &k);
    Tcl_DecrRefCount(t);
    if (c == NULL || k % 2) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("expected a polyline in cells", -1));
	return TCL_ERROR;
    }
    *p = (const pt *) c;
    *n = k / 2;
    return TCL_OK;
}

static int
Out(Tcl_Interp *interp, pt *q, Tcl_Size n)
{
    Tcl_Obj *t = Tcl_NewStringObj((const char *) Tcl_GetAssocData(interp, "geomcells", NULL), -1), *r;
    int rc;

    Tcl_IncrRefCount(t);
    rc = Tcl_AnnotatedCreateCells(interp, t, q, 2 * n, sizeof(double), &r);
    Tcl_DecrRefCount(t);
    if (rc == TCL_OK) {
	Tcl_SetObjResult(interp, r);
    }
    return rc;
}

static int
TranslateCells(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const pt *p; Tcl_Size n, i, nv; Tcl_Obj **ev; double tx, ty; pt *q; int rc;
    (void) cd;
    if (objc != 3) { Tcl_WrongNumArgs(interp, 1, objv, "vector polyline"); return TCL_ERROR; }
    if (Tcl_ListObjGetElements(interp, objv[1], &nv, &ev) != TCL_OK || nv != 2
	    || Tcl_GetDoubleFromObj(interp, ev[0], &tx) != TCL_OK
	    || Tcl_GetDoubleFromObj(interp, ev[1], &ty) != TCL_OK
	    || GetCells(interp, objv[2], &p, &n) != TCL_OK) {
	return TCL_ERROR;
    }
    q = (pt *) Tcl_Alloc(sizeof(pt) * (n + 1));
    for (i = 0; i < n; i++) { q[i].x = p[i].x + tx; q[i].y = p[i].y + ty; }
    rc = Out(interp, q, n);
    Tcl_Free(q);
    return rc;
}

static int
RotateCells(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const pt *p; Tcl_Size n, i; double angle, a, c, s; pt *q; int rc;
    (void) cd;
    if (objc != 3) { Tcl_WrongNumArgs(interp, 1, objv, "angle polyline"); return TCL_ERROR; }
    if (Tcl_GetDoubleFromObj(interp, objv[1], &angle) != TCL_OK
	    || GetCells(interp, objv[2], &p, &n) != TCL_OK) {
	return TCL_ERROR;
    }
    a = ((4 * atan(1)) / 180.0) * angle; c = cos(a); s = sin(a);
    q = (pt *) Tcl_Alloc(sizeof(pt) * (n + 1));
    for (i = 0; i < n; i++) {
	double x = p[i].x, y = p[i].y;
	q[i].x = c * x - s * y; q[i].y = s * x + c * y;
    }
    rc = Out(interp, q, n);
    Tcl_Free(q);
    return rc;
}

int
Geomcells_Init(Tcl_Interp *interp)
{
    static char typeName[] = "::math::geometry::polylinec";
    if (Tcl_InitStubs(interp, "9.0", 0) == NULL) return TCL_ERROR;
    Tcl_SetAssocData(interp, "geomcells", NULL, typeName);
    Tcl_CreateObjCommand2(interp, "::math::geometry::translateCells", TranslateCells, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::math::geometry::rotateCells", RotateCells, NULL, NULL);
    return TCL_OK;
}
