/*
 * geom.c --
 *
 * FR : Le cœur de calcul de math::geometry (tcllib) en C, à la manière de
 *      l'exemple matrices :
 *        - les structures C (point, line) viennent des types Tcl par
 *          type {c!}export (geom_types.h) ;
 *        - les fonctions geom_* sont du C pur (aucun appel à Tcl) : on peut
 *          les appeler depuis un autre programme C ;
 *        - les commandes Tcl (::math::geometry::c::*) lisent leurs arguments
 *          et appellent ces fonctions. Un polygone en cases (type
 *          {-compact}, liste de double) est lu EN PLACE, sans copie ; une
 *          liste ordinaire est lue par l'API des listes.
 *      geometry_c.tcl met ces commandes à la place des procédures de
 *      geometry.tcl : mêmes noms, mêmes arguments, mêmes résultats (les tests
 *      de tcllib passent). Là où tcllib calcule en entiers (des entrées
 *      entières : division entière, somme entière), le C fait de même
 *      (struct Num), pour des résultats identiques au caractère près.
 *
 * EN : math::geometry's computing core in C, as in the matrices example:
 *      structs from type {c!}export, pure C geom_* functions, Tcl commands
 *      that read their arguments (in place for a value held in cells) and
 *      call them. Same names, arguments and results as tcllib.
 *
 * Compiler / build (Linux) :
 *   gcc -shared -fPIC -O2 -ffp-contract=off -DUSE_TCL_STUBS \
 *       -I<src>/generic -I<build> -I<src>/unix -I../.. \
 *       -o geom.so geom.c <build>/libtclstub.a
 * MinGW64 :
 *   gcc -shared -O2 -ffp-contract=off -DUSE_TCL_STUBS -I<src>/generic \
 *       -I<src>/win -I../.. -o geom.dll geom.c <build>/libtclstub.a
 *
 * (-ffp-contract=off : pas de multiplication-addition fusionnée, pour les
 * mêmes arrondis que expr.)
 */

#include <tcl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "tclAnnotatedApi.h"
#include "geom_types.h"			/* produit par type {c!}export */

#if defined(_WIN32)
#  define GEOM_EXPORT __declspec(dllexport)
#else
#  define GEOM_EXPORT __attribute__((visibility("default")))
#endif

/* ======================================================================
 * Le C pur : des points, des segments, des tableaux de points.
 * Les formules et l'ordre des opérations sont ceux de geometry.tcl.
 * ====================================================================== */

GEOM_EXPORT double
geom_distance(const point *a, const point *b)
{
    return hypot(b->x - a->x, b->y - a->y);
}

/* calculateDistanceToLineSegment : P au segment A-B */
GEOM_EXPORT double
geom_distance_to_segment(const point *P, const point *A, const point *B)
{
    double L, r, dist;

    if (A->x == B->x && A->y == B->y) {
	return hypot(P->x - A->x, P->y - A->y);
    }
    L = hypot(B->x - A->x, B->y - A->y);
    r = ((P->x - A->x) * (B->x - A->x) + (P->y - A->y) * (B->y - A->y))
	    / pow(L, 2);
    dist = fabs((A->y - P->y) * (B->x - A->x) - (A->x - P->x) * (B->y - A->y))
	    / L;
    if (r < 0) {
	return hypot(P->x - A->x, P->y - A->y);
    }
    if (r > 1) {
	return hypot(P->x - B->x, P->y - B->y);
    }
    return dist;
}

/* Le point de la droite A-B le plus proche de P, et sa position r. */
static void
ClosestOnLine(const point *P, const point *A, const point *B, point *Q,
	double *rPtr)
{
    double Lsquared, r;

    if (A->x == B->x && A->y == B->y) {
	*Q = *A;
	*rPtr = 0;
	return;
    }
    Lsquared = pow(B->x - A->x, 2) + pow(B->y - A->y, 2);
    r = ((P->x - A->x) * (B->x - A->x) + (P->y - A->y) * (B->y - A->y))
	    / Lsquared;
    Q->x = A->x + r * (B->x - A->x);
    Q->y = A->y + r * (B->y - A->y);
    *rPtr = r;
}

/* Sedgewick : A -> B -> C tourne-t-il dans le sens trigonométrique ? */
static int
Ccw(const point *A, const point *B, const point *C)
{
    double dx1 = B->x - A->x, dy1 = B->y - A->y;
    double dx2 = C->x - A->x, dy2 = C->y - A->y;

    if (dx1 * dy2 > dy1 * dx2) {
	return 1;
    }
    if (dx1 * dy2 < dy1 * dx2) {
	return -1;
    }
    if ((dx1 * dx2 < 0) || (dy1 * dy2 < 0)) {
	return -1;
    }
    if ((dx1 * dx1 + dy1 * dy1) < (dx2 * dx2 + dy2 * dy2)) {
	return 1;
    }
    return 0;
}

GEOM_EXPORT int
geom_segments_intersect(const line *s1, const line *s2)
{
    const double margin = 1.0e-7;
    point a = {s1->x1, s1->y1}, b = {s1->x2, s1->y2};
    point c = {s2->x1, s2->y1}, d = {s2->x2, s2->y2};

    if (geom_distance_to_segment(&a, &c, &d) < margin
	    || geom_distance_to_segment(&b, &c, &d) < margin
	    || geom_distance_to_segment(&c, &a, &b) < margin
	    || geom_distance_to_segment(&d, &a, &b) < margin) {
	return 1;
    }
    return (Ccw(&a, &b, &c) * Ccw(&a, &b, &d) <= 0)
	    && (Ccw(&c, &d, &a) * Ccw(&c, &d, &b) <= 0);
}

GEOM_EXPORT double
geom_area_polygon(const point *p, size_t n)
{
    double area = 0.0, b1, b2;
    size_t i;

    if (n == 0) {
	return 0.0;
    }
    b1 = p[n - 1].x;
    b2 = p[n - 1].y;
    for (i = 0; i < n; i++) {
	area = area + (b1 * p[i].y - b2 * p[i].x);
	b1 = p[i].x;
	b2 = p[i].y;
    }
    return 0.5 * fabs(area);
}

GEOM_EXPORT double
geom_length_polyline(const point *p, size_t n)
{
    double len = 0;
    size_t i;

    for (i = 1; i < n; i++) {
	len = len + hypot(p[i - 1].x - p[i].x, p[i - 1].y - p[i].y);
    }
    return len;
}

/* Le nombre d'enroulement (pointInsidePolygonAlt) */
GEOM_EXPORT int
geom_point_inside_polygon_alt(const point *t, const point *p, size_t n)
{
    int w = 0;
    size_t i, j;

    for (i = 0; i < n; i++) {
	double left;

	j = (i + 1 == n) ? 0 : i + 1;
	left = (p[j].x - p[i].x) * (t->y - p[i].y)
		- (t->x - p[i].x) * (p[j].y - p[i].y);
	if (p[i].y <= t->y) {
	    if (p[j].y > t->y && left > 0.0) {
		w++;
	    }
	} else if (p[j].y <= t->y && left < 0.0) {
	    w--;
	}
    }
    return w != 0;
}

/* min sur les segments ; HUGE_VAL si moins de deux points */
GEOM_EXPORT double
geom_distance_to_polyline(const point *P, const point *p, size_t n)
{
    double best = HUGE_VAL;
    size_t i;

    for (i = 1; i < n; i++) {
	double d = geom_distance_to_segment(P, &p[i - 1], &p[i]);

	if (d < best) {
	    best = d;
	}
    }
    return best;
}

/* Rotation (degrés, sens trigonométrique) ; reflect : symétrie par rapport
 * à la droite qui passe par l'origine avec l'angle donné. out peut être in. */
GEOM_EXPORT void
geom_rotate(point *out, const point *in, size_t n, double angle)
{
    double a = ((4 * atan(1)) / 180.0) * angle, c = cos(a), s = sin(a);
    size_t i;

    for (i = 0; i < n; i++) {
	double x = in[i].x, y = in[i].y;

	out[i].x = c * x - s * y;
	out[i].y = s * x + c * y;
    }
}

GEOM_EXPORT void
geom_reflect(point *out, const point *in, size_t n, double angle)
{
    double a = 2.0 * ((4 * atan(1)) / 180.0) * angle, c = cos(a), s = sin(a);
    size_t i;

    for (i = 0; i < n; i++) {
	double x = in[i].x, y = in[i].y;

	out[i].x = c * x + s * y;
	out[i].y = s * x - c * y;
    }
}

static void
BBox(const point *p, size_t n, point *lo, point *hi)
{
    size_t i;

    *lo = *hi = p[0];
    for (i = 1; i < n; i++) {
	if (p[i].x < lo->x) lo->x = p[i].x;
	if (p[i].x > hi->x) hi->x = p[i].x;
	if (p[i].y < lo->y) lo->y = p[i].y;
	if (p[i].y > hi->y) hi->y = p[i].y;
    }
}

/* polylinesBoundingIntersect, à l'identique (y compris sa règle : en mode
 * exact, la réponse est celle de la PREMIÈRE paire de parties dont les
 * boîtes se touchent). */
GEOM_EXPORT int
geom_polylines_bounding_intersect(const point *a, size_t na, const point *b,
	size_t nb, int granularity)
{
    int perfect = (granularity <= 1);
    size_t g, step, i, j, k, l;

    if (perfect) {
	granularity = 10;
    }
    g = (size_t) granularity;
    step = g - 1;			/* en points : 2*g-2 nombres */
    for (i = 0; i < na; i += step) {
	size_t ni = (i + g <= na) ? g : na - i;

	for (j = 0; j < nb; j += step) {
	    size_t nj = (j + g <= nb) ? g : nb - j;
	    point lo1, hi1, lo2, hi2;

	    BBox(a + i, ni, &lo1, &hi1);
	    BBox(b + j, nj, &lo2, &hi2);
	    if (!(hi1.x >= lo2.x && hi2.x >= lo1.x && hi1.y >= lo2.y
		    && hi2.y >= lo1.y)) {
		continue;
	    }
	    if (!perfect) {
		return 1;
	    }
	    for (k = 1; k < ni; k++) {
		for (l = 1; l < nj; l++) {
		    line s1 = {a[i+k-1].x, a[i+k-1].y, a[i+k].x, a[i+k].y};
		    line s2 = {b[j+l-1].x, b[j+l-1].y, b[j+l].x, b[j+l].y};

		    if (geom_segments_intersect(&s1, &s2)) {
			return 1;
		    }
		}
	    }
	    return 0;
	}
    }
    return 0;
}

/* ======================================================================
 * Les commandes Tcl
 * ====================================================================== */


/* Un tableau de points lu depuis une valeur Tcl : en place (cases d'une
 * liste {-compact} de double), ou recopié depuis une liste ordinaire ; objs
 * garde alors les nombres d'origine (rendus tels quels par bbox...). */
typedef struct Pts {
    const point *p;
    size_t n;
    point *own;
    Tcl_Obj **objs;			/* 2n nombres, ou NULL (en cases) */
    point buf[16];
} Pts;

static void
FreePts(Pts *P)
{
    if (P->own != NULL && P->own != P->buf) {
	ckfree(P->own);
    }
}

static int
GetPts(Tcl_Interp *interp, Tcl_Obj *obj, Pts *P)
{
    const void *cells = NULL;
    Tcl_Size n, i;
    Tcl_Obj *t = Tcl_NewStringObj("double", -1), **ev;
    point *q;

    P->own = NULL;
    P->objs = NULL;
    Tcl_IncrRefCount(t);
    (void) Tcl_AnnotatedGetCells(interp, obj, t, sizeof(double), &cells, &n);
    Tcl_DecrRefCount(t);
    if (cells != NULL && n % 2 == 0) {
	P->p = (const point *) cells;	/* en place : n doubles = n/2 points */
	P->n = (size_t) n / 2;
	return TCL_OK;
    }
    if (Tcl_ListObjGetElements(interp, obj, &n, &ev) != TCL_OK) {
	return TCL_ERROR;
    }
    if (n % 2 != 0) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected a list of x y "
		"coordinates, got %" TCL_SIZE_MODIFIER "d numbers", n));
	return TCL_ERROR;
    }
    q = (n / 2 <= 16) ? P->buf : (point *) ckalloc(sizeof(point) * (n / 2));
    for (i = 0; i < n; i += 2) {
	if (Tcl_GetDoubleFromObj(interp, ev[i], &q[i / 2].x) != TCL_OK
		|| Tcl_GetDoubleFromObj(interp, ev[i + 1], &q[i / 2].y)
		!= TCL_OK) {
	    if (q != P->buf) {
		ckfree(q);
	    }
	    return TCL_ERROR;
	}
    }
    P->own = q;
    P->p = q;
    P->n = (size_t) n / 2;
    P->objs = ev;
    return TCL_OK;
}

/* Un point {x y} (les deux premiers nombres, comme lindex). */
static int
GetPoint(Tcl_Interp *interp, Tcl_Obj *obj, point *p)
{
    Tcl_Obj **ev;
    Tcl_Size n;

    if (Tcl_ListObjGetElements(interp, obj, &n, &ev) != TCL_OK) {
	return TCL_ERROR;
    }
    if (n < 2) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected a point {x y} but got "
		"\"%s\"", Tcl_GetString(obj)));
	return TCL_ERROR;
    }
    if (Tcl_GetDoubleFromObj(interp, ev[0], &p->x) != TCL_OK
	    || Tcl_GetDoubleFromObj(interp, ev[1], &p->y) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
GetLine(Tcl_Interp *interp, Tcl_Obj *obj, line *l)
{
    Tcl_Obj **ev;
    Tcl_Size n;

    if (Tcl_ListObjGetElements(interp, obj, &n, &ev) != TCL_OK) {
	return TCL_ERROR;
    }
    if (n < 4) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected a line {x1 y1 x2 y2} "
		"but got \"%s\"", Tcl_GetString(obj)));
	return TCL_ERROR;
    }
    if (Tcl_GetDoubleFromObj(interp, ev[0], &l->x1) != TCL_OK
	    || Tcl_GetDoubleFromObj(interp, ev[1], &l->y1) != TCL_OK
	    || Tcl_GetDoubleFromObj(interp, ev[2], &l->x2) != TCL_OK
	    || Tcl_GetDoubleFromObj(interp, ev[3], &l->y2) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/* Un nombre de Tcl, entier ou flottant, et l'arithmétique de expr : un
 * entier op un entier reste entier (division entière arrondie vers -inf). */
typedef struct Num {
    int isInt;
    Tcl_WideInt i;
    double d;
} Num;

static int
GetNum(Tcl_Interp *interp, Tcl_Obj *o, Num *n)
{
    void *cd;
    int type;

    if (Tcl_GetNumberFromObj(NULL, o, &cd, &type) == TCL_OK
	    && type == TCL_NUMBER_INT) {
	n->isInt = 1;
	n->i = *(Tcl_WideInt *) cd;
	n->d = (double) n->i;
	return TCL_OK;
    }
    n->isInt = 0;
    return Tcl_GetDoubleFromObj(interp, o, &n->d);
}

static Num NumD(double d) { Num r = {0, 0, d}; return r; }
static Num NumI(Tcl_WideInt i) { Num r = {1, i, (double) i}; return r; }
static Num Add(Num a, Num b) { return (a.isInt && b.isInt) ? NumI(a.i + b.i) : NumD(a.d + b.d); }
static Num Sub(Num a, Num b) { return (a.isInt && b.isInt) ? NumI(a.i - b.i) : NumD(a.d - b.d); }
static Num Mul(Num a, Num b) { return (a.isInt && b.isInt) ? NumI(a.i * b.i) : NumD(a.d * b.d); }
static Num
Div(Num a, Num b)
{
    if (a.isInt && b.isInt) {
	Tcl_WideInt q = a.i / b.i;

	if ((a.i % b.i != 0) && ((a.i < 0) != (b.i < 0))) {
	    q--;			/* vers -inf, comme expr */
	}
	return NumI(q);
    }
    return NumD(a.d / b.d);
}
static int Lt(Num a, Num b) { return (a.isInt && b.isInt) ? a.i < b.i : a.d < b.d; }
static int IsZero(Num a) { return a.isInt ? a.i == 0 : a.d == 0; }
static Tcl_Obj *NumObj(Num a) { return a.isInt ? Tcl_NewWideIntObj(a.i) : Tcl_NewDoubleObj(a.d); }

/* Les nombres d'une liste plate (ou des cases) en Num. */
static int
GetNums(Tcl_Interp *interp, Tcl_Obj *obj, Num **numsPtr, Tcl_Size *nPtr)
{
    Pts P;
    Tcl_Size i;
    Num *v;

    if (GetPts(interp, obj, &P) != TCL_OK) {
	return TCL_ERROR;
    }
    v = (Num *) ckalloc(sizeof(Num) * (2 * P.n + 1));
    for (i = 0; i < (Tcl_Size) (2 * P.n); i++) {
	if (P.objs != NULL) {
	    if (GetNum(interp, P.objs[i], &v[i]) != TCL_OK) {
		ckfree(v);
		FreePts(&P);
		return TCL_ERROR;
	    }
	} else {
	    v[i] = NumD(i % 2 ? P.p[i / 2].y : P.p[i / 2].x);
	}
    }
    FreePts(&P);
    *numsPtr = v;
    *nPtr = 2 * (Tcl_Size) P.n;
    return TCL_OK;
}

#define ARGS(n, usage) \
    if (objc != (n) + 1) { Tcl_WrongNumArgs(interp, 1, objv, usage); return TCL_ERROR; }

static int
DistanceCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    point a, b;
    (void) cd;

    ARGS(2, "pa pb");
    if (GetPoint(interp, objv[1], &a) != TCL_OK
	    || GetPoint(interp, objv[2], &b) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(geom_distance(&a, &b)));
    return TCL_OK;
}

static int
AreaPolygonCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Pts P;
    (void) cd;

    ARGS(1, "polygon");
    if (GetPts(interp, objv[1], &P) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(geom_area_polygon(P.p, P.n)));
    FreePts(&P);
    return TCL_OK;
}

static int
LengthOfPolylineCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Pts P;
    (void) cd;

    ARGS(1, "polyline");
    if (GetPts(interp, objv[1], &P) != TCL_OK) {
	return TCL_ERROR;
    }
    /* tcllib : 0 (entier) s'il n'y a aucun segment */
    Tcl_SetObjResult(interp, P.n < 2 ? Tcl_NewWideIntObj(0)
	    : Tcl_NewDoubleObj(geom_length_polyline(P.p, P.n)));
    FreePts(&P);
    return TCL_OK;
}

/* bbox : les nombres d'origine (comme tcllib), pas des doubles recalculés */
static int
BboxCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Pts P;
    size_t i, im[4] = {0, 0, 0, 0};	/* indices de minX, minY, maxX, maxY */
    Tcl_Obj *r[4];
    (void) cd;

    ARGS(1, "polyline");
    if (GetPts(interp, objv[1], &P) != TCL_OK) {
	return TCL_ERROR;
    }
    if (P.n == 0) {
	for (i = 0; i < 4; i++) {
	    r[i] = Tcl_NewObj();
	}
    } else {
	for (i = 1; i < P.n; i++) {
	    if (P.p[i].x < P.p[im[0]].x) im[0] = i;
	    if (P.p[i].y < P.p[im[1]].y) im[1] = i;
	    if (P.p[i].x > P.p[im[2]].x) im[2] = i;
	    if (P.p[i].y > P.p[im[3]].y) im[3] = i;
	}
	for (i = 0; i < 4; i++) {
	    size_t k = 2 * im[i] + (i % 2);

	    r[i] = P.objs ? P.objs[k] : Tcl_NewDoubleObj(i % 2 ? P.p[im[i]].y
		    : P.p[im[i]].x);
	}
    }
    Tcl_SetObjResult(interp, Tcl_NewListObj(4, r));
    FreePts(&P);
    return TCL_OK;
}

/* pnpoly, avec l'arithmétique de expr (division entière si tout est
 * entier) */
static int
PointInsidePolygonCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Num *v, t[2];
    Tcl_Size n, i, j, nv;
    Tcl_Obj **ev;
    int c = 0;
    (void) cd;

    ARGS(2, "point polygon");
    if (Tcl_ListObjGetElements(interp, objv[1], &n, &ev) != TCL_OK) {
	return TCL_ERROR;
    }
    if (n < 2 || GetNum(interp, ev[0], &t[0]) != TCL_OK
	    || GetNum(interp, ev[1], &t[1]) != TCL_OK) {
	if (n < 2) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("expected a point {x y}", -1));
	}
	return TCL_ERROR;
    }
    if (GetNums(interp, objv[2], &v, &n) != TCL_OK) {
	return TCL_ERROR;
    }
    nv = n / 2;
    for (i = 0, j = nv - 1; i < nv; j = i++) {
	Num xi = v[2*i], yi = v[2*i+1], xj = v[2*j], yj = v[2*j+1];

	if ((Lt(t[1], yi)) != (Lt(t[1], yj))
		&& Lt(t[0], Add(Div(Mul(Sub(xj, xi), Sub(t[1], yi)),
			Sub(yj, yi)), xi))) {
	    c = !c;
	}
    }
    ckfree(v);
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(c));
    return TCL_OK;
}

static int
PointInsidePolygonAltCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    point t;
    Pts P;
    int r;
    (void) cd;

    ARGS(2, "point polygon");
    if (GetPoint(interp, objv[1], &t) != TCL_OK
	    || GetPts(interp, objv[2], &P) != TCL_OK) {
	return TCL_ERROR;
    }
    r = geom_point_inside_polygon_alt(&t, P.p, P.n);
    FreePts(&P);
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(r));
    return TCL_OK;
}

static int
DistanceToPolylineCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    point t;
    Pts P;
    double d;
    (void) cd;

    ARGS(2, "P polyline");
    if (GetPoint(interp, objv[1], &t) != TCL_OK
	    || GetPts(interp, objv[2], &P) != TCL_OK) {
	return TCL_ERROR;
    }
    d = geom_distance_to_polyline(&t, P.p, P.n);
    Tcl_SetObjResult(interp, P.n < 2 ? Tcl_NewStringObj("Inf", 3)
	    : Tcl_NewDoubleObj(d));
    FreePts(&P);
    return TCL_OK;
}

/* findClosestPointOnPolyline : une extrémité rend ses nombres d'origine */
static int
ClosestPointOnPolylineCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    point t;
    Pts P;
    size_t i;
    double best = HUGE_VAL;
    long bestK = -1;			/* -1 : aucun ; >= 0 : sommet ; -2 : projeté */
    point bestQ = {0, 0};
    (void) cd;

    ARGS(2, "P polyline");
    if (GetPoint(interp, objv[1], &t) != TCL_OK
	    || GetPts(interp, objv[2], &P) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 1; i < P.n; i++) {
	point Q;
	double r, d;
	long k;

	ClosestOnLine(&t, &P.p[i - 1], &P.p[i], &Q, &r);
	if (r < 0) {
	    k = (long) i - 1;
	    Q = P.p[i - 1];
	} else if (r > 1) {
	    k = (long) i;
	    Q = P.p[i];
	} else {
	    /* A == B : {Ax Ay} d'origine ; sinon le projeté */
	    k = (P.p[i-1].x == P.p[i].x && P.p[i-1].y == P.p[i].y)
		    ? (long) i - 1 : -2;
	}
	d = hypot(Q.x - t.x, Q.y - t.y);
	if (d < best) {
	    best = d;
	    bestK = k;
	    bestQ = Q;
	}
    }
    if (bestK == -1) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("none", 4));
    } else {
	Tcl_Obj *r[2];

	if (bestK >= 0 && P.objs != NULL) {
	    r[0] = P.objs[2 * bestK];
	    r[1] = P.objs[2 * bestK + 1];
	} else {
	    r[0] = Tcl_NewDoubleObj(bestQ.x);
	    r[1] = Tcl_NewDoubleObj(bestQ.y);
	}
	Tcl_SetObjResult(interp, Tcl_NewListObj(2, r));
    }
    FreePts(&P);
    return TCL_OK;
}

/* rotate, reflect : (angle polyline) -> une liste plate de doubles */
static int
TransformCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    double angle;
    Pts P;
    point *out;
    Tcl_Obj *res, **ev;
    size_t i;

    ARGS(2, "angle polyline");
    if (Tcl_GetDoubleFromObj(interp, objv[1], &angle) != TCL_OK
	    || GetPts(interp, objv[2], &P) != TCL_OK) {
	return TCL_ERROR;
    }
    out = (point *) ckalloc(sizeof(point) * (P.n + 1));
    (cd == NULL ? geom_rotate : geom_reflect)(out, P.p, P.n, angle);
    ev = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * (2 * P.n + 1));
    for (i = 0; i < P.n; i++) {
	ev[2 * i] = Tcl_NewDoubleObj(out[i].x);
	ev[2 * i + 1] = Tcl_NewDoubleObj(out[i].y);
    }
    res = Tcl_NewListObj((Tcl_Size) (2 * P.n), ev);
    ckfree(ev);
    ckfree(out);
    FreePts(&P);
    Tcl_SetObjResult(interp, res);
    return TCL_OK;
}

/* translate : x + xt avec l'arithmétique de expr (somme entière d'entiers) */
static int
TranslateCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Num t[2], *v;
    Tcl_Obj **ev, *res;
    Tcl_Size n, i;
    (void) cd;

    ARGS(2, "vector polyline");
    if (Tcl_ListObjGetElements(interp, objv[1], &n, &ev) != TCL_OK) {
	return TCL_ERROR;
    }
    if (n < 2 || GetNum(interp, ev[0], &t[0]) != TCL_OK
	    || GetNum(interp, ev[1], &t[1]) != TCL_OK) {
	if (n < 2) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("expected a vector {x y}", -1));
	}
	return TCL_ERROR;
    }
    if (GetNums(interp, objv[2], &v, &n) != TCL_OK) {
	return TCL_ERROR;
    }
    ev = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * (n + 1));
    for (i = 0; i < n; i++) {
	ev[i] = NumObj(Add(v[i], t[i % 2]));
    }
    res = Tcl_NewListObj(n, ev);
    ckfree(ev);
    ckfree(v);
    Tcl_SetObjResult(interp, res);
    return TCL_OK;
}

/* findLineIntersection, avec l'arithmétique de expr */
static int
LineIntersectionCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Num a[4], b[4], d, na;
    Tcl_Obj **ev, *r[2];
    Tcl_Size n;
    int k, w;
    (void) cd;

    ARGS(2, "line1 line2");
    for (w = 0; w < 2; w++) {
	Num *t = w ? b : a;

	if (Tcl_ListObjGetElements(interp, objv[w + 1], &n, &ev) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (n < 4) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("expected a line {x1 y1 x2 y2}", -1));
	    return TCL_ERROR;
	}
	for (k = 0; k < 4; k++) {
	    if (GetNum(interp, ev[k], &t[k]) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    /* a : l1x1 l1y1 l1x2 l1y2 ; b : l2x1 l2y1 l2x2 l2y2 */
    d = Sub(Mul(Sub(b[3], b[1]), Sub(a[2], a[0])),
	    Mul(Sub(b[2], b[0]), Sub(a[3], a[1])));
    na = Sub(Mul(Sub(b[2], b[0]), Sub(a[1], b[1])),
	    Mul(Sub(b[3], b[1]), Sub(a[0], b[0])));
    if (IsZero(d)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(IsZero(na) ? "coincident"
		: "none", -1));
	return TCL_OK;
    }
    r[0] = NumObj(Add(a[0], Div(Mul(na, Sub(a[2], a[0])), d)));
    r[1] = NumObj(Add(a[1], Div(Mul(na, Sub(a[3], a[1])), d)));
    Tcl_SetObjResult(interp, Tcl_NewListObj(2, r));
    return TCL_OK;
}

static int
SegmentsIntersectCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    line s1, s2;
    (void) cd;

    ARGS(2, "linesegment1 linesegment2");
    if (GetLine(interp, objv[1], &s1) != TCL_OK
	    || GetLine(interp, objv[2], &s2) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(geom_segments_intersect(&s1, &s2)));
    return TCL_OK;
}

static int
PolylinesIntersectCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Pts A, B;
    int g = 0, r;

    if (cd == NULL) {
	ARGS(2, "polyline1 polyline2");
    } else {
	ARGS(3, "polyline1 polyline2 granularity");
	if (Tcl_GetIntFromObj(interp, objv[3], &g) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (GetPts(interp, objv[1], &A) != TCL_OK) {
	return TCL_ERROR;
    }
    if (GetPts(interp, objv[2], &B) != TCL_OK) {
	FreePts(&A);
	return TCL_ERROR;
    }
    r = geom_polylines_bounding_intersect(A.p, A.n, B.p, B.n, g);
    FreePts(&A);
    FreePts(&B);
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(r));
    return TCL_OK;
}

/* sizeof vus par le compilateur (à comparer à type binary {sizeof?}) */
static int
LayoutCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    (void) cd; (void) objc; (void) objv;
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("point %d line %d",
	    (int) sizeof(point), (int) sizeof(line)));
    return TCL_OK;
}

GEOM_EXPORT int
Geom_Init(Tcl_Interp *interp)
{
    static const struct {
	const char *name;
	Tcl_ObjCmdProc2 *proc;
	void *cd;
    } cmds[] = {
	{"distance",                    DistanceCmd, NULL},
	{"areaPolygon",                 AreaPolygonCmd, NULL},
	{"lengthOfPolyline",            LengthOfPolylineCmd, NULL},
	{"bbox",                        BboxCmd, NULL},
	{"pointInsidePolygon",          PointInsidePolygonCmd, NULL},
	{"pointInsidePolygonAlt",       PointInsidePolygonAltCmd, NULL},
	{"calculateDistanceToPolyline", DistanceToPolylineCmd, NULL},
	{"findClosestPointOnPolyline",  ClosestPointOnPolylineCmd, NULL},
	{"rotate",                      TransformCmd, NULL},
	{"reflect",                     TransformCmd, (void *) 1},
	{"translate",                   TranslateCmd, NULL},
	{"findLineIntersection",        LineIntersectionCmd, NULL},
	{"lineSegmentsIntersect",       SegmentsIntersectCmd, NULL},
	{"polylinesIntersect",          PolylinesIntersectCmd, NULL},
	{"polylinesBoundingIntersect",  PolylinesIntersectCmd, (void *) 1},
	{"layout",                      LayoutCmd, NULL},
	{NULL, NULL, NULL}
    };
    int i;

    if (Tcl_InitStubs(interp, "9.0", 0) == NULL) {
	return TCL_ERROR;
    }
    for (i = 0; cmds[i].name != NULL; i++) {
	Tcl_Obj *n = Tcl_ObjPrintf("::math::geometry::c::%s", cmds[i].name);

	Tcl_IncrRefCount(n);
	Tcl_CreateObjCommand2(interp, Tcl_GetString(n), cmds[i].proc,
		cmds[i].cd, NULL);
	Tcl_DecrRefCount(n);
    }
    return Tcl_PkgProvide(interp, "geom", "1.0");
}
