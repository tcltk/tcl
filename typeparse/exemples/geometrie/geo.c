/*
 * geo.c --
 *
 * FR : Étape 3 : les calculs lourds de ../geometrie.tcl, écrits en C sur de
 *      vraies structures. Le type Tcl décrit la structure C :
 *
 *        Tcl (geo_c.tcl)                         C
 *        -------------------------------------   ------------------------------
 *        type def pointC {{f64le}x; {f64le}y}    typedef struct {double x, y;} Point;
 *        type def polyC {{u32le}n;               typedef struct {
 *                        {list($n).pointC}pts}       uint32_t n;   // + 4 de remplissage
 *                                                    Point    pts[];
 *                                                } Poly;          // 8 + 16 n
 *
 *      Deux voies de passage, pour chaque commande :
 *        geo::c::NOM_raw OCTETS  le polygone est encodé par `type binary encode
 *                                polyC -align natural` ; C lit un (const Poly *).
 *        geo::c::NOM_obj VALEUR  C reçoit la valeur annotée (polygone, nuage,
 *                                bezier), vérifie son type et lit ses points.
 *
 *      Commandes : aire, perimetre, englobant, dedans (un nuage de points dans
 *      un polygone), bezier (n points d'une Bézier cubique).
 *
 * EN : Stage 3: the heavy computations of ../geometrie.tcl, written in C on
 *      real structs. The Tcl type describes the C struct. Two ways in, for each
 *      command: encoded bytes read through a (const Poly *), or the annotated
 *      value itself, type-checked by C.
 *
 * HYPOTHÈSE / ASSUMPTION : hôte little-endian (x86-64, arm64).
 */
#include <tcl.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/*
 * FR : les struct viennent des types Tcl : geo_types.h est produit par
 *      `tclsh geo_types.tcl > geo_types.h` (type c), avec des _Static_assert
 *      qui arrêtent la compilation si ce compilateur range autrement.
 * EN : the structs come from the Tcl types (type c), with _Static_assert.
 */
#include "geo_types.h"
#include "../../tclCompact.h"

typedef pointC Point;		/* même disposition que point (deux double) */
typedef polyC Poly;
_Static_assert(sizeof(point) == sizeof(Point), "point et pointC");

/* ------------------------------------------------------------------ */
/* Les calculs, écrits une fois / the computations, written once      */
/* ------------------------------------------------------------------ */

static double
Aire(const Point *p, uint32_t n)
{
    double s = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++) {
	const Point *a = &p[i], *b = &p[(i + 1) % n];
	s += a->x * b->y - b->x * a->y;
    }
    return fabs(s) / 2.0;
}

static double
Perimetre(const Point *p, uint32_t n)
{
    double s = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++) {
	const Point *a = &p[i], *b = &p[(i + 1) % n];
	s += hypot(b->x - a->x, b->y - a->y);
    }
    return s;
}

static void
Englobant(const Point *p, uint32_t n, double r[4])
{
    uint32_t i;
    r[0] = r[2] = n ? p[0].x : 0.0;
    r[1] = r[3] = n ? p[0].y : 0.0;
    for (i = 1; i < n; i++) {
	if (p[i].x < r[0]) r[0] = p[i].x;
	if (p[i].y < r[1]) r[1] = p[i].y;
	if (p[i].x > r[2]) r[2] = p[i].x;
	if (p[i].y > r[3]) r[3] = p[i].y;
    }
}

/* lancer de rayon / ray casting : même règle que geo::dedans */
static int
Dedans(Point pt, const Point *q, uint32_t n)
{
    uint32_t i;
    int c = 0;
    for (i = 0; i < n; i++) {
	const Point *a = &q[i], *b = &q[(i + 1) % n];
	if ((a->y > pt.y) != (b->y > pt.y)
		&& pt.x < a->x + (pt.y - a->y) * (b->x - a->x) / (b->y - a->y)) {
	    c = !c;
	}
    }
    return c;
}

static Point
Bezier(const Point b[4], double t)
{
    double s = 1.0 - t;
    Point r;
    r.x = s*s*s*b[0].x + 3*s*s*t*b[1].x + 3*s*t*t*b[2].x + t*t*t*b[3].x;
    r.y = s*s*s*b[0].y + 3*s*s*t*b[1].y + 3*s*t*t*b[2].y + t*t*t*b[3].y;
    return r;
}

/* ------------------------------------------------------------------ */
/* Voie OCTETS : un (const Poly *) sur les octets encodés              */
/* BYTES way: a (const Poly *) over the encoded bytes                  */
/* ------------------------------------------------------------------ */

static int
PolyFromBytes(Tcl_Interp *interp, Tcl_Obj *obj, const Point **ptsPtr,
	uint32_t *nPtr)
{
    Tcl_Size len;
    const unsigned char *p = Tcl_GetBytesFromObj(interp, obj, &len);
    const Poly *poly;

    if (p == NULL) {
	return TCL_ERROR;
    }
    /* FR : le contrat binaire est vérifié, jamais supposé. */
    if ((size_t) len < sizeof(Poly) || ((uintptr_t) p % _Alignof(Poly)) != 0) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"geo: not a polyC (too short or misaligned)", -1));
	return TCL_ERROR;
    }
    poly = (const Poly *) p;
    if ((size_t) len != sizeof(Poly) + (size_t) poly->n * sizeof(Point)) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"geo: %" TCL_SIZE_MODIFIER "d bytes for n=%u (expected %zu): "
		"was the polyC encoded with -align natural?", len,
		(unsigned) poly->n, sizeof(Poly) + (size_t) poly->n * sizeof(Point)));
	return TCL_ERROR;
    }
    *ptsPtr = poly->pts;
    *nPtr = poly->n;
    return TCL_OK;
}

/* ------------------------------------------------------------------ */
/* Voie OBJET : la valeur annotée elle-même                            */
/* OBJECT way: the annotated value itself                              */
/* ------------------------------------------------------------------ */

/*
 * FR : AUJOURD'HUI le type et le contenu se demandent à Tcl (`type get`,
 *      ::tcl::unsupported::content). Avec les fonctions de stub prévues
 *      (Tcl_AnnotatedGetType, Tcl_AnnotatedGetParts), ce seraient deux appels C.
 * EN : TODAY type and content are asked of Tcl. With the planned stub
 *      functions, these would be two plain C calls.
 */
static Tcl_Obj *
AskTcl(Tcl_Interp *interp, const char *c1, const char *c2, Tcl_Obj *obj)
{
    Tcl_Obj *cmd[3], *res = NULL;
    int n = 0, i;

    cmd[n++] = Tcl_NewStringObj(c1, -1);
    if (c2 != NULL) {
	cmd[n++] = Tcl_NewStringObj(c2, -1);
    }
    for (i = 0; i < n; i++) {
	Tcl_IncrRefCount(cmd[i]);
    }
    cmd[n++] = obj;
    if (Tcl_EvalObjv(interp, n, cmd, 0) == TCL_OK) {
	res = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(res);
    }
    for (i = 0; i < n - 1; i++) {
	Tcl_DecrRefCount(cmd[i]);
    }
    return res;			/* une référence pour l'appelant, ou NULL */
}

/*
 * FR : rend VALEUR typée par TYPE (l'équivalent de `type cast TYPE VALEUR`).
 *      Avec une fonction de stub (Tcl_AnnotatedCreate), ce serait un appel C.
 * EN : returns VALUE typed as TYPE. A stub function would make it a C call.
 */
static int
SetTypedResult(Tcl_Interp *interp, const char *type, Tcl_Obj *value)
{
    Tcl_Obj *cmd[4];
    int i, code;

    cmd[0] = Tcl_NewStringObj("type", -1);
    cmd[1] = Tcl_NewStringObj("cast", -1);
    cmd[2] = Tcl_NewStringObj(type, -1);
    cmd[3] = value;
    for (i = 0; i < 4; i++) {
	Tcl_IncrRefCount(cmd[i]);
    }
    code = Tcl_EvalObjv(interp, 4, cmd, 0);	/* résultat dans interp */
    for (i = 0; i < 4; i++) {
	Tcl_DecrRefCount(cmd[i]);
    }
    return code;
}

/*
 * Lit les points d'une valeur annotée du type `want` dans un tableau alloué
 * (*ptsPtr, à libérer par Tcl_Free).
 */
static int
PolyFromObj(Tcl_Interp *interp, Tcl_Obj *obj, const char *want,
	Point **ptsPtr, uint32_t *nPtr)
{
    Tcl_Obj *type, *content, **elv, **xy;
    Tcl_Size n, i, nxy;
    Point *pts;

    type = AskTcl(interp, "type", "get", obj);
    if (type == NULL) {
	return TCL_ERROR;
    }
    if (strcmp(Tcl_GetString(type), want) != 0) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"geo: expected a %s but got \"%s\"", want,
		Tcl_GetString(type)[0] ? Tcl_GetString(type) : "untyped value"));
	Tcl_DecrRefCount(type);
	return TCL_ERROR;
    }
    Tcl_DecrRefCount(type);
    content = AskTcl(interp, "::tcl::unsupported::content", NULL, obj);
    if (content == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_ListObjGetElements(interp, content, &n, &elv) != TCL_OK) {
	Tcl_DecrRefCount(content);
	return TCL_ERROR;
    }
    pts = (Point *) Tcl_Alloc((size_t)(n ? n : 1) * sizeof(Point));
    for (i = 0; i < n; i++) {
	if (Tcl_ListObjGetElements(interp, elv[i], &nxy, &xy) != TCL_OK
		|| nxy != 2
		|| Tcl_GetDoubleFromObj(interp, xy[0], &pts[i].x) != TCL_OK
		|| Tcl_GetDoubleFromObj(interp, xy[1], &pts[i].y) != TCL_OK) {
	    if (nxy != 2) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"geo: a point has 2 fields", -1));
	    }
	    Tcl_Free(pts);
	    Tcl_DecrRefCount(content);
	    return TCL_ERROR;
	}
    }
    Tcl_DecrRefCount(content);
    *ptsPtr = pts;
    *nPtr = (uint32_t) n;
    return TCL_OK;
}

/* ------------------------------------------------------------------ */
/* Les commandes / the commands                                        */
/*   clientData = NULL : voie octets ; non NULL : voie objet           */
/* ------------------------------------------------------------------ */

#define VOIE_OBJET(cd) ((cd) != NULL)

/* Obtient les points de objv[k], selon la voie. *toFree à libérer. */
static int
GetPoints(Tcl_Interp *interp, void *cd, Tcl_Obj *obj, const char *want,
	const Point **ptsPtr, uint32_t *nPtr, Point **toFree)
{
    *toFree = NULL;
    if (VOIE_OBJET(cd)) {
	/*
	 * FR : forme compacte (tcl::unsupported::compact on) : les points
	 *      SONT un tableau de `point`, lu en place, sans copie. Le type
	 *      est vérifié quand même (polygone, nuage, bezier).
	 * EN : compact form: the points ARE an array of `point`, read in place.
	 */
	const TclCompactView *cv = TclCompactViewOfType(obj, "point",
		sizeof(point));

	if (cv != NULL) {
	    Tcl_Obj *type = AskTcl(interp, "type", "get", obj);

	    if (type == NULL) {
		return TCL_ERROR;
	    }
	    if (strcmp(Tcl_GetString(type), want) != 0) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"geo: expected a %s but got \"%s\"", want,
			Tcl_GetString(type)));
		Tcl_DecrRefCount(type);
		return TCL_ERROR;
	    }
	    Tcl_DecrRefCount(type);
	    *ptsPtr = (const Point *) cv->data;
	    *nPtr = (uint32_t) cv->n;
	    return TCL_OK;
	}
	if (PolyFromObj(interp, obj, want, toFree, nPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	*ptsPtr = *toFree;
	return TCL_OK;
    }
    return PolyFromBytes(interp, obj, ptsPtr, nPtr);
}

static int
AireCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const Point *p;
    Point *f;
    uint32_t n;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "polygone");
	return TCL_ERROR;
    }
    if (GetPoints(interp, cd, objv[1], "polygone", &p, &n, &f) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(Aire(p, n)));
    if (f) Tcl_Free(f);
    return TCL_OK;
}

static int
PerimetreCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const Point *p;
    Point *f;
    uint32_t n;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "polygone");
	return TCL_ERROR;
    }
    if (GetPoints(interp, cd, objv[1], "polygone", &p, &n, &f) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(Perimetre(p, n)));
    if (f) Tcl_Free(f);
    return TCL_OK;
}

/* rend un rectangle annoté {{xmin ymin} {xmax ymax}} */
static int
EnglobantCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const Point *p;
    Point *f;
    uint32_t n;
    double r[4];
    Tcl_Obj *v[4];
    int i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "polygone");
	return TCL_ERROR;
    }
    if (GetPoints(interp, cd, objv[1], "polygone", &p, &n, &f) != TCL_OK) {
	return TCL_ERROR;
    }
    Englobant(p, n, r);
    if (f) Tcl_Free(f);
    for (i = 0; i < 4; i++) {
	v[i] = Tcl_NewDoubleObj(r[i]);
    }
    {
	Tcl_Obj *mm[2];
	mm[0] = Tcl_NewListObj(2, v);
	mm[1] = Tcl_NewListObj(2, v + 2);
	return SetTypedResult(interp, "rectangle", Tcl_NewListObj(2, mm));
    }
}

/* dedans NUAGE POLYGONE : rend une liste de 0 et de 1 */
static int
DedansCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const Point *pts, *q;
    Point *f1, *f2;
    uint32_t np, nq, i;
    Tcl_Obj *res, *un, *zero;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "nuage polygone");
	return TCL_ERROR;
    }
    if (GetPoints(interp, cd, objv[1], "nuage", &pts, &np, &f1) != TCL_OK) {
	return TCL_ERROR;
    }
    if (GetPoints(interp, cd, objv[2], "polygone", &q, &nq, &f2) != TCL_OK) {
	if (f1) Tcl_Free(f1);
	return TCL_ERROR;
    }
    res = Tcl_NewListObj(np, NULL);
    un = Tcl_NewWideIntObj(1);
    zero = Tcl_NewWideIntObj(0);
    for (i = 0; i < np; i++) {
	Tcl_ListObjAppendElement(NULL, res, Dedans(pts[i], q, nq) ? un : zero);
    }
    Tcl_SetObjResult(interp, res);
    if (f1) Tcl_Free(f1);
    if (f2) Tcl_Free(f2);
    return TCL_OK;
}

/* bezier BEZIER N : rend une polyligne annotée de N points */
static int
BezierCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const Point *b;
    Point *f;
    uint32_t nb;
    Tcl_WideInt n, i;
    Tcl_Obj *res;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "bezier n");
	return TCL_ERROR;
    }
    if (Tcl_GetWideIntFromObj(interp, objv[2], &n) != TCL_OK) {
	return TCL_ERROR;
    }
    if (n < 2 || n > 10000000) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("geo: n must be in 2..10000000", -1));
	return TCL_ERROR;
    }
    if (GetPoints(interp, cd, objv[1], "bezier", &b, &nb, &f) != TCL_OK) {
	return TCL_ERROR;
    }
    if (nb != 4) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("geo: a bezier has 4 points", -1));
	if (f) Tcl_Free(f);
	return TCL_ERROR;
    }
    res = Tcl_NewListObj(n, NULL);
    for (i = 0; i < n; i++) {
	Point r = Bezier(b, (double) i / (double)(n - 1));
	Tcl_Obj *xy[2];
	xy[0] = Tcl_NewDoubleObj(r.x);
	xy[1] = Tcl_NewDoubleObj(r.y);
	Tcl_ListObjAppendElement(NULL, res, Tcl_NewListObj(2, xy));
    }
    if (f) Tcl_Free(f);
    return SetTypedResult(interp, "polyligne", res);
}

/* geo::c::layout : tailles et décalages vus par le compilateur */
static int
LayoutCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *r = Tcl_NewListObj(0, NULL);
    (void) cd; (void) objc; (void) objv;
#define ADD(name, val) \
    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(name, -1)); \
    Tcl_ListObjAppendElement(NULL, r, Tcl_NewWideIntObj((Tcl_WideInt)(val)))
    ADD("sizeof(Point)", sizeof(Point));
    ADD("Poly.n", offsetof(Poly, n));
    ADD("Poly.pts", offsetof(Poly, pts));
    Tcl_SetObjResult(interp, r);
    return TCL_OK;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int
Geo_Init(Tcl_Interp *interp)
{
    static int objet = 1;
    static const struct {
	const char *nom;
	Tcl_ObjCmdProc2 *proc;
    } cmds[] = {
	{"aire", AireCmd}, {"perimetre", PerimetreCmd},
	{"englobant", EnglobantCmd}, {"dedans", DedansCmd},
	{"bezier", BezierCmd}, {NULL, NULL}
    };
    int i;

    if (Tcl_InitStubs(interp, "9.0", 0) == NULL) {
	return TCL_ERROR;
    }
    for (i = 0; cmds[i].nom != NULL; i++) {
	char buf[64];
	snprintf(buf, sizeof(buf), "::geo::c::%s_raw", cmds[i].nom);
	Tcl_CreateObjCommand2(interp, buf, cmds[i].proc, NULL, NULL);
	snprintf(buf, sizeof(buf), "::geo::c::%s_obj", cmds[i].nom);
	Tcl_CreateObjCommand2(interp, buf, cmds[i].proc, &objet, NULL);
    }
    Tcl_CreateObjCommand2(interp, "::geo::c::layout", LayoutCmd, NULL, NULL);
    return Tcl_PkgProvide(interp, "geo", "1.0");
}
