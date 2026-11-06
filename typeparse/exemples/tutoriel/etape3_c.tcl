# Étape 3 : les procédures qui comptent passent en C.
#
# Les types et les signatures sont ceux de l'étape 2 ; seuls les corps
# changent de langue (cproc). {-compact} range un polygone en cases, comme
# un tableau C de points : le C le lit en place.

source [file join [file dirname [file normalize [info script]]] .. cproc cproc.tcl]
if {[info exists ::env(CPROC_CC)]} {cproc::config -cc $::env(CPROC_CC)}

namespace eval poly {
    type def point    {{double}x; {double}y}
    type def {-compact}polygone list.point
    type def boite    {{double}xmin; {double}ymin; {double}xmax; {double}ymax}
}

# trop simple pour le C : elle reste en Tcl
proc {point}poly::point {{double}x; {double}y} {( $x, $y )}

cproc::code {#include <math.h>}

# pg arrive comme { const point *at; Tcl_Size n; }
cproc {double}poly::aire {{polygone}pg} {
    double a = 0.0;
    point q = pg.at[pg.n - 1];

    for (Tcl_Size i = 0; i < pg.n; i++) {
	a = a + q.x * pg.at[i].y - q.y * pg.at[i].x;
	q = pg.at[i];
    }
    return fabs(a) / 2;
}

cproc {double}poly::perimetre {{flag}-ouvert; {polygone}pg} {
    double l = 0.0;
    point q = ouvert ? pg.at[0] : pg.at[pg.n - 1];

    for (Tcl_Size i = ouvert; i < pg.n; i++) {
	l = l + hypot(pg.at[i].x - q.x, pg.at[i].y - q.y);
	q = pg.at[i];
    }
    return l;
}

cproc {int}poly::contient {{polygone}pg; {point}P} {
    int dedans = 0;
    point q = pg.at[pg.n - 1];

    for (Tcl_Size i = 0; i < pg.n; i++) {
	point p = pg.at[i];

	if ((p.y > P.y) != (q.y > P.y)
		&& P.x < (q.x - p.x) * (P.y - p.y) / (q.y - p.y) + p.x) {
	    dedans = !dedans;
	}
	q = p;
    }
    return dedans;
}

# un résultat liste : construit par cproc_list_point
cproc {polygone}poly::deplace {{polygone}pg; {double}dx; {double}dy ?= 0.0} {
    point *r = (point *) Tcl_Alloc(sizeof(point) * (pg.n > 0 ? pg.n : 1));
    Tcl_Obj *res;

    for (Tcl_Size i = 0; i < pg.n; i++) {
	r[i] = (point){pg.at[i].x + dx, pg.at[i].y + dy};
    }
    res = cproc_list_point(r, pg.n);
    Tcl_Free(r);
    return res;
}

cproc {boite}poly::boite {{polygone}pg} {
    boite b = {pg.at[0].x, pg.at[0].y, pg.at[0].x, pg.at[0].y};

    for (Tcl_Size i = 0; i < pg.n; i++) {
	b.xmin = fmin(b.xmin, pg.at[i].x);  b.xmax = fmax(b.xmax, pg.at[i].x);
	b.ymin = fmin(b.ymin, pg.at[i].y);  b.ymax = fmax(b.ymax, pg.at[i].y);
    }
    return b;
}
