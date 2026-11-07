# variadiques.tcl -- cproc et les arguments variadiques {T}args... (v16y)
# La procédure typée de même signature lie les mots (au milieu, groupés par
# produit, au moins un ou zéro avec ?=) ; le C reçoit une cproc_span_T.
#   tclsh variadiques.tcl ?COMPILATEUR?
source [file join [file dirname [file normalize [info script]]] cproc.tcl]
if {$argc > 0} {cproc::config -cc [lindex $argv 0]}

type def pt {{double}x; {double}y}

# des points à plat : x1 y1 x2 y2 ?x y ...?
cproc {double}longueur {{pt}args...} {
    double l = 0;
    for (Tcl_Size i = 1; i < args.n; i++) {
        l += hypot(args.at[i].x - args.at[i-1].x, args.at[i].y - args.at[i-1].y);
    }
    return l;
}

# au milieu : k args... fin
cproc {double}affine {{double}k; {double}args...; {double}fin} {
    double s = 0;
    for (Tcl_Size i = 0; i < args.n; i++) s += args.at[i];
    return k * s + fin;
}

# zéro ou plus
cproc {int}compte {{int}args... ?= {}} {
    return args.n;
}

proc montre {script} {
    if {[catch {uplevel 1 $script} r]} { set r "ERREUR $r" }
    puts [format "%-28s -> %s" $script $r]
}
montre {longueur 0 0 3 4 3 0}
montre {longueur 0 0 3}
montre {longueur 0 0 3 x}
montre {affine 2 1 2 3 100}
montre {affine 2 100}
montre {compte}
montre {compte 1 2 3}
montre {compte 1 a}
