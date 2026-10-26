# v16h : listes de taille fixe en cases (matrices, tableaux à N dimensions).
set ici [file dirname [file normalize [info script]]]
source [file join $ici .. layout filebinary.tcl]
# Chaque opération est faite sur une valeur en cases ({-compact}) et sur sa
# jumelle ordinaire : les résultats doivent être les mêmes.
type def {-compact}m3c list(3).list(3).double
type def m3o list(3).list(3).double
type def {-compact}c3c list(2).list(3).list(2).int(s,32)
type def c3o list(2).list(3).list(2).int(s,32)
type def {-compact}lc list.list(4).int(u,8)
type def lo list.list(4).int(u,8)
type def ligne {{double}a; {list(2).double}v}
type def {-compact}plc list.ligne
type def plo list.ligne

proc essai {nom script} {
    if {[catch {uplevel 1 $script} r]} {set r "erreur : $r"}
    puts "$nom : $r"
}
proc deux {nom script} {
    # le script lit $T (type) ; c puis o
    foreach s {c o} {
	if {[catch {uplevel 1 [string map [list %S $s] $script]} r]} {set r "erreur : $r"}
	lappend res $r
    }
    if {[lindex $res 0] eq [lindex $res 1]} {
	puts "$nom : [lindex $res 0]"
    } else {
	puts "$nom : DIFFÉRENT\n  c: [lindex $res 0]\n  o: [lindex $res 1]"
    }
}

essai "disposition" {tcl::unsupported::compact layout list(2).list(3).int(s,16)}
essai "disposition produit" {dict get [tcl::unsupported::compact layout ligne] fields}

set {m3c}Mc {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}
set {m3o}Mo {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}
essai "en cases" {type is {compact?}$Mc}
deux "valeur" {list $M%S [string range [type get $M%S] 0 1]}
deux "ligne" {set r [lindex $M%S 1]; list $r [type get $r]}
deux "case" {set r [lindex $M%S 1 2]; list $r [type get $r]}
deux "chemin" {set r $M%S(2.0); list $r [type get $r]}
deux "chemin ligne" {set r $M%S(1); list $r [type get $r]}
deux "chemin end" {set M%S(end.end)}
deux "chemin hors bornes" {set M%S(1.3)}
deux "expr" {expr {$M%S(0.0) * $M%S(1.1) + $M%S(2.2)}}
deux "lindex end" {lindex $M%S end end-1}
deux "llength" {list [llength $M%S] [llength [lindex $M%S 0]]}
deux "foreach" {set acc {}; foreach r $M%S {foreach x $r {lappend acc $x}}; set acc}
deux "lmap" {lmap r $M%S {expr {[lindex $r 0] + $r(1) + [lindex $r 2]}}}
deux "écriture chemin" {set N $M%S; set N(1.2) 60.5; list $N [string range [type get $N] 0 1] $M%S(1.2)}
deux "lset" {set N $M%S; lset N 0 0 11.0; lset N 2 {1.0 1.0 1.0}; list $N [string range [type get $N] 0 1]}
deux "lset mauvaise ligne" {set N $M%S; lset N 2 {1 1}}
deux "lset mauvais nombre" {set N $M%S; lset N 0 1 x}
deux "ligne entière" {set N $M%S; set N(0) {7.0 7.0 7.0}; set N}
deux "mauvaise arité" {set {m3%S}Z {1 2 3} {4 5} {7 8 9}}
deux "mauvais nombre" {set {m3%S}Z {1 2 3} {4 5 x} {7 8 9}}
deux "tuple" {expr {(a, b, c) = $M%S}; list $a $b $c}
deux "partage" {set N $M%S; set N(0.0) 99.0; list $M%S(0.0) $N(0.0)}
deux "chaîne" {string length $M%S}

set {c3c}Cc {{1 2} {3 4} {5 6}} {{7 8} {9 10} {11 12}}
set {c3o}Co {{1 2} {3 4} {5 6}} {{7 8} {9 10} {11 12}}
essai "3D en cases" {type is {compact?}$Cc}
deux "3D chemin" {set C%S(1.2.0)}
deux "3D lindex" {lindex $C%S 1 2 1}
deux "3D plan" {set r [lindex $C%S 0]; list $r [type get $r]}
deux "3D écriture" {set N $C%S; set N(0.1.1) 40; set N(1.0) {-1 -2}; set N}
deux "3D débordement" {set N $C%S; set N(0.0.0) 3000000000}

set {lc}Lc {1 2 3 4} {5 6 7 8}
set {lo}Lo {1 2 3 4} {5 6 7 8}
essai "liste extensible en cases" {type is {compact?}$Lc}
deux "lappend" {set N $L%S; lappend N {9 10 11 12}; list $N [llength $N]}
deux "lappend mauvais" {set N $L%S; lappend N {9 10 11 300}}
deux "u8" {set L%S(1.3)}

set {plc}Pc {1.5 {2.0 3.0}} {4.5 {5.0 6.0}}
set {plo}Po {1.5 {2.0 3.0}} {4.5 {5.0 6.0}}
essai "produit à liste en cases" {type is {compact?}$Pc}
deux "produit chemin" {list $P%S(1.v.1) $P%S(0.a) [type get $P%S(1.v)]}
deux "produit écriture" {set N $P%S; set N(0.v.0) 20.0; set N}

essai "cast" {set x [type cast m3c {{1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}}]; list $x [type get $x] [type is {compact?}$x]}
essai "binaire" {binary encode hex [type binary encode c3c $Cc]}
essai "binaire retour" {set b [type binary encode c3c $Cc]; set y [type binary decode c3c $b]; list $y [type is {compact?}$y]}
essai "cells" {set y [tcl::unsupported::compact cells list.list(2).int(s,16) [binary format s4 {1 2 3 4}]]; list $y [type get $y]}
type def {-compact}m2b list(2).list(2).double(64)
essai "binaire double(64)" {set {m2b}B {1.0 2.5} {-3.0 4.25}; set y [type binary decode m2b [type binary encode m2b $B]]; list $y [type is {compact?}$y] [string length [type binary encode m2b $B]]}
proc trace_m {} {
    set {m3c}T {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}
    set s 0.0
    for {set i 0} {$i < 3} {incr i} {set s [expr {$s + $T($i.$i)}]}
    set s
}
essai "trace (procédure)" {trace_m}
essai "erreur de type de ligne" {set {m3c}T {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}; set T(1) {1.0 2.0}}
essai "contrat sur une case" {set {m3c}T {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}; set T(1.1) abc}
essai "grande matrice" {type def {-compact}m100 list(100).list(100).double; dict get [tcl::unsupported::compact layout list(100).double] size}
essai "trop grande" {type def {-compact}m5000 list(2).list(5000).double}
# Écriture dans la case (v16h) : rien d'autre ne doit voir le changement.
proc ecrit_vue {} {
    set {m3c}T {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}
    set r [lindex $T 0]
    set T(0.0) 5.0
    list $r $T(0) [type is {compact?}$T]
}
essai "vue gardée" {ecrit_vue}
proc ecrit_copie {} {
    set {m3c}T {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}
    set U $T
    set T(2.2) -1.5
    list $U(2.2) $T(2.2)
}
essai "copie gardée" {ecrit_copie}
proc ecrit_foreach {} {
    set {m3c}T {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}
    set l {}
    foreach r $T {lappend l $r}
    set T(1.1) 0.5
    list $l $T
}
essai "éléments parcourus gardés" {ecrit_foreach}
proc ecrit_chaine {} {
    set {m3c}T {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}
    set avant "$T"
    set T(0.1) 2.5
    list $avant "$T" [string length $T]
}
essai "chaîne refaite" {ecrit_chaine}
proc tr {args} {lappend ::vu [set ::trT(0.2)]}
proc ecrit_trace {} {
    global trT vu
    set vu {}
    set {m3c}trT {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}
    trace add variable trT write tr
    set trT(0.2) 33.0
    trace remove variable trT write tr
    set vu
}
essai "trace d'écriture" {ecrit_trace}
proc ecrit_indices {} {
    set {m3c}T {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}
    for {set i 0} {$i < 3} {incr i} {set T($i.$i) [expr {$T($i.$i) * 10}]}
    set k 1.2
    lappend r $T [catch {set T($k)} m] $m
    set T(end.0) 0.0
    lappend r $T(2)
}
essai "indices calculés" {ecrit_indices}
proc ecrit_refus {} {
    set {m3c}T {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}
    lappend r [catch {set T(0.0) abc} m] $m [catch {set T(0.3) 1.0} m] $m
    set T(0.0) 7
    lappend r $T(0)
}
essai "refus et normalisation" {ecrit_refus}
proc ecrit_3d {} {
    set {c3c}X {{1 2} {3 4} {5 6}} {{7 8} {9 10} {11 12}}
    set X(1.2.1) -7
    lappend r [catch {set X(0.0.0) 3000000000} m] $m $X
}
essai "3D sur place" {ecrit_3d}
