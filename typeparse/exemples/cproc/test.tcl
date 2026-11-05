# test.tcl -- cproc : procédures typées au corps en C
#
#   tclsh test.tcl ?COMPILATEUR?        (gcc par défaut ; tcc, ou un chemin)
#
# La bibliothèque compilée est gardée en cache (CPROC_CACHE, sinon
# ~/.cache/cproc ou %LOCALAPPDATA%\cproc).
source [file join [file dirname [file normalize [info script]]] cproc.tcl]
if {$argc > 0} {cproc::config -cc [lindex $argv 0]}
type def point {{double}x; {double}y}
cproc {point}decale {{point}p; {double}dx ?= 0} {
    return (point){p.x + dx, p.y};
}
proc {point}decale_tcl {{point}p; {double}dx ?= 0} {( $p(x) + $dx, $p(y) )}
cproc {double}aire {{list.double}poly} {
    double a = 0;
    Tcl_Size n = poly.n / 2, i;
    for (i = 0; i < n; i++) {
        Tcl_Size j = (i + 1) % n;
        a += poly.at[2*i] * poly.at[2*j+1] - poly.at[2*j] * poly.at[2*i+1];
    }
    return a / 2;
}
cproc {int}compte {{list.point}pts; {double}r; {flag}-strict} {
    Tcl_Size i; Tcl_WideInt k = 0;
    for (i = 0; i < pts.n; i++) {
        double d = pts.at[i].x * pts.at[i].x + pts.at[i].y * pts.at[i].y;
        if (strict ? d < r*r : d <= r*r) k++;
    }
    return k;
}
cproc {string}salut {{string}nom} {
    static char buf[100];
    if (nom[0] == 0) { cproc_error("salut: empty name"); return NULL; }
    snprintf(buf, sizeof buf, "bonjour %s", nom);
    return buf;
}
cproc {list.point}miroir {{list.point}pts} {
    point tmp[64]; Tcl_Size i;
    if (pts.n > 64) { cproc_error("miroir: too many points (%d)", (int) pts.n); return NULL; }
    for (i = 0; i < pts.n; i++) tmp[i] = (point){pts.at[i].y, pts.at[i].x};
    return cproc_list_point(tmp, pts.n);
}
puts "decale       : [decale {1 2} 3] [type get [decale {1 2} 3]] [decale {1 2}]"
puts "même que Tcl : [decale {1.5 2} 0.1] | [decale_tcl {1.5 2} 0.1]"
puts "aire         : [aire {0 0 10 0 10 10 0 10}]"
type def {-compact}poly_c list.double
set {poly_c}PC 0 0 10 0 10 10 0 10
puts "aire (cases) : [aire $PC]"
puts "compte       : [compte {{0 0} {1 0} {3 4}} 1] [compte -strict {{0 0} {1 0} {3 4}} 1] [compte {{0 0} {1 0} {3 4}} 5]"
puts "salut        : [salut monde] [catch {salut {}} m] $m"
puts "miroir       : [miroir {{1 2} {3 4}}] [type get [miroir {{1 2} {3 4}}]]"
puts "erreur Tcl   : [catch {decale {1 x}} m] $m"
puts "args         : [catch {decale} m] $m"
puts "étiquette    : [decale {dx:}5 {p:}{0 0}]"
namespace eval g {
    type def v {{double}a; {double}b}
    cproc {v}::g::somme {{v}u; {v}w} { return (v){u.a + w.a, u.b + w.b}; }
}
puts "namespace    : [g::somme {1 2} {3 4}] [type get [g::somme {1 2} {3 4}]]"
