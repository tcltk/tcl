# geometry_cproc.tcl -- geometry.tcl, dont les commandes lourdes sont des
# cproc : même signature typée, corps en C (../cproc/cproc.tcl).
#
#	package require math; source geometry_cproc.tcl
#
# FR : troisième étape de la démarche : le prototype en Tcl (tcllib), le
#      Tcl typé (geometry.tcl), puis le C pour ce qui doit aller vite. Avec
#      geom.c, la colle Tcl <-> C était écrite à la main (la moitié du
#      fichier) ; ici, chaque procédure garde sa signature et seul son corps
#      passe en C. Les appelants, les messages d'erreur et les tests de
#      tcllib ne voient pas la différence.
#
#      Le compilateur : gcc par défaut ; CPROC_CC=tcc (ou un chemin) pour
#      tcc. La bibliothèque compilée est gardée en cache.
#
# EN : geometry.tcl, whose heavy commands are cprocs: same typed signature,
#      body in C. The Tcl <-> C glue is generated.

namespace eval ::math::geometry {
    variable cproc_dir [file dirname [file normalize [info script]]]
}
namespace eval ::math::geometry {variable coord double}	;# C structs of doubles
source [file join $::math::geometry::cproc_dir geometry.tcl]
source [file join $::math::geometry::cproc_dir .. cproc cproc.tcl]
if {[info exists ::env(CPROC_CC)]} {cproc::config -cc $::env(CPROC_CC)}

# Le C commun : les formules et l'ordre des opérations de geometry.tcl
cproc::code {
#include <math.h>

/* calculateDistanceToLineSegment : P au segment A-B */
static double
DistToSegment(point P, point A, point B)
{
    double L, r, dist;

    if (A.x == B.x && A.y == B.y) {
	return hypot(P.x - A.x, P.y - A.y);
    }
    L = hypot(B.x - A.x, B.y - A.y);
    r = ((P.x - A.x) * (B.x - A.x) + (P.y - A.y) * (B.y - A.y)) / pow(L, 2);
    dist = fabs((A.y - P.y) * (B.x - A.x) - (A.x - P.x) * (B.y - A.y)) / L;
    if (r < 0) return hypot(P.x - A.x, P.y - A.y);
    if (r > 1) return hypot(P.x - B.x, P.y - B.y);
    return dist;
}

/* Sedgewick : A -> B -> C tourne-t-il dans le sens trigonométrique ? */
static int
Ccw(point A, point B, point C)
{
    double dx1 = B.x - A.x, dy1 = B.y - A.y, dx2 = C.x - A.x, dy2 = C.y - A.y;

    if (dx1 * dy2 > dy1 * dx2) return 1;
    if (dx1 * dy2 < dy1 * dx2) return -1;
    if ((dx1 * dx2 < 0) || (dy1 * dy2 < 0)) return -1;
    if ((dx1 * dx1 + dy1 * dy1) < (dx2 * dx2 + dy2 * dy2)) return 1;
    return 0;
}

/* une polyligne (list.double) vue comme des points */
#define PTS(pl) ((const point *) (pl).at)
#define NPTS(pl) ((pl).n / 2)
}

namespace eval ::math::geometry {

cproc {double}distance {{point}pa; {point}pb} {
    return hypot(pb.x - pa.x, pb.y - pa.y);
}

cproc {double}areaPolygon {{polygon}pg} {
    const point *p = PTS(pg);
    Tcl_Size n = NPTS(pg), i;
    double area = 0.0, b1, b2;

    if (n == 0) return 0.0;
    b1 = p[n - 1].x;
    b2 = p[n - 1].y;
    for (i = 0; i < n; i++) {
	area = area + (b1 * p[i].y - b2 * p[i].x);
	b1 = p[i].x;
	b2 = p[i].y;
    }
    return 0.5 * fabs(area);
}

cproc {double}lengthOfPolyline {{polyline}pl} {
    const point *p = PTS(pl);
    Tcl_Size i;
    double len = 0;

    for (i = 1; i < NPTS(pl); i++) {
	len = len + hypot(p[i - 1].x - p[i].x, p[i - 1].y - p[i].y);
    }
    return len;
}

# le nombre d'enroulement
cproc {int}pointInsidePolygonAlt {{point}P; {polygon}pg} {
    const point *p = PTS(pg);
    Tcl_Size n = NPTS(pg), i, j;
    int w = 0;

    for (i = 0; i < n; i++) {
	double left;

	j = (i + 1 == n) ? 0 : i + 1;
	left = (p[j].x - p[i].x) * (P.y - p[i].y) - (P.x - p[i].x) * (p[j].y - p[i].y);
	if (p[i].y <= P.y) {
	    if (p[j].y > P.y && left > 0.0) w++;
	} else if (p[j].y <= P.y && left < 0.0) {
	    w--;
	}
    }
    return w != 0;
}

cproc {double}calculateDistanceToPolyline {{point}P; {polyline}pl} {
    const point *p = PTS(pl);
    Tcl_Size i;
    double best = HUGE_VAL;		/* moins de deux points : Inf */

    for (i = 1; i < NPTS(pl); i++) {
	double d = DistToSegment(P, p[i - 1], p[i]);

	if (d < best) best = d;
    }
    return best;
}

cproc {int}lineSegmentsIntersect {{segment}s1; {segment}s2} {
    const double margin = 1.0e-7;
    point a = {s1.x1, s1.y1}, b = {s1.x2, s1.y2};
    point c = {s2.x1, s2.y1}, d = {s2.x2, s2.y2};

    if (DistToSegment(a, c, d) < margin || DistToSegment(b, c, d) < margin
	    || DistToSegment(c, a, b) < margin || DistToSegment(d, a, b) < margin) {
	return 1;
    }
    return Ccw(a, b, c) * Ccw(a, b, d) <= 0 && Ccw(c, d, a) * Ccw(c, d, b) <= 0;
}

# une somme : le C rend la valeur (le point, ou un mot), la procédure la
# range dans la somme intersection
cproc {intersection}findLineIntersection {{line}l1; {line}l2} {
    double d  = (l2.y2 - l2.y1) * (l1.x2 - l1.x1) - (l2.x2 - l2.x1) * (l1.y2 - l1.y1);
    double na = (l2.x2 - l2.x1) * (l1.y1 - l2.y1) - (l2.y2 - l2.y1) * (l1.x1 - l2.x1);
    point q;

    if (d == 0) {
	return Tcl_NewStringObj(na == 0 ? "coincident" : "none", -1);
    }
    q.x = l1.x1 + na * (l1.x2 - l1.x1) / d;
    q.y = l1.y1 + na * (l1.y2 - l1.y1) / d;
    return cproc_list_double(&q.x, 2);
}

cproc rotate {{double}angle; {polyline}pl} {
    double a = ((4 * atan(1)) / 180.0) * angle, c = cos(a), s = sin(a);
    Tcl_Obj *r = Tcl_NewListObj(0, NULL);
    Tcl_Size i;

    for (i = 0; i + 1 < pl.n; i += 2) {
	double x = pl.at[i], y = pl.at[i + 1];

	Tcl_ListObjAppendElement(NULL, r, Tcl_NewDoubleObj(c * x - s * y));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewDoubleObj(s * x + c * y));
    }
    return r;
}

cproc translate {{vector}t; {polyline}pl} {
    Tcl_Obj *r = Tcl_NewListObj(0, NULL);
    Tcl_Size i;

    for (i = 0; i + 1 < pl.n; i += 2) {
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewDoubleObj(pl.at[i] + t.x));
	Tcl_ListObjAppendElement(NULL, r, Tcl_NewDoubleObj(pl.at[i + 1] + t.y));
    }
    return r;
}

}

cproc::build
