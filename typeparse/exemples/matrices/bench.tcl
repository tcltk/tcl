# usage : tclsh bench.tcl [chemin/vers/mat.so|.dll]
# FR : µs par opération, matrices 3×3 : Tcl sur des listes brutes, Tcl sur des
#      matrices typées (en cases, ou ordinaires), C sur les cases.
# EN : µs per operation on 3×3 matrices: Tcl on raw lists, Tcl on typed
#      matrices (in cells, or ordinary), C on the cells.
set here [file dirname [file normalize [info script]]]
source [file join $here matrices.tcl]
mat::init {*}$argv
type def matrix3o list(3).list(3).double

# Les mêmes calculs en Tcl classique, sur des listes de listes.
proc brut_det {M} {
    lassign $M r0 r1 r2
    lassign $r0 a b c; lassign $r1 d e f; lassign $r2 g h i
    expr {$a*($e*$i-$f*$h) - $b*($d*$i-$f*$g) + $c*($d*$h-$e*$g)}
}
proc brut_mul {A B} {
    set C {{0.0 0.0 0.0} {0.0 0.0 0.0} {0.0 0.0 0.0}}
    foreach i [lseq  3]  {
	foreach j [lseq 3] {
	    lset C $i $j [expr {[lindex $A $i 0] * [lindex $B 0 $j]
		+ [lindex $A $i 1] * [lindex $B 1 $j]
		+ [lindex $A $i 2] * [lindex $B 2 $j]}]
	}
    }
    return $C
}
# mat::tcl_mul déclare son résultat matrix3 ; la variante ordinaire :
proc o_mul {A B} {
    set {matrix3o}C {0.0 0.0 0.0} {0.0 0.0 0.0} {0.0 0.0 0.0}
    foreach i [lseq  3]  {
	foreach j [lseq 3] {
	    set C($i.$j) [expr {$A($i.0) * $B(0.$j) + $A($i.1) * $B(1.$j)
		+ $A($i.2) * $B(2.$j)}]
	}
    }
    return $C
}

proc mesure {label n script} {
    uplevel 1 $script
    set t [lindex [uplevel 1 [list time $script $n]] 0]
    puts [format "| %-44s | %10.2f |" $label $t]
}

set R {{2.0 0.0 1.0} {1.0 3.0 2.0} {1.0 1.0 2.0}}
set {matrix3}A {*}$R
set {matrix3o}O {*}$R
puts "| opération (µs) | |\n|---|---:|"
mesure "déterminant, Tcl brut (lassign)"            20000 {brut_det $R}
mesure "déterminant, Tcl typé en cases (\$M(i.j))"  20000 {mat::tcl_det $A}
mesure "déterminant, Tcl typé ordinaire"            20000 {mat::tcl_det $O}
mesure "déterminant, C sur les cases"               20000 {mat::det $A}
mesure "déterminant, C sur une liste brute"         20000 {mat::det $R}
mesure "produit, Tcl brut (lindex/lset)"             5000 {brut_mul $R $R}
mesure "produit, Tcl typé en cases"                  5000 {mat::tcl_mul $A $A}
mesure "produit, Tcl typé ordinaire"                 5000 {o_mul $O $O}
mesure "produit, C (résultat en cases)"              5000 {mat::mul $A $A}

set N 10000
set {lot}L
set I [list]
set brut {}
for {set k 0} {$k < $N} {incr k} {
    set M [mat::rotz [expr {$k * 0.001}]]
    lappend L $M
    lappend I $M
    lappend brut [list {*}$M]
}
mesure "$N déterminants, Tcl brut"                   3 {lmap M $brut {brut_det $M}}
mesure "$N déterminants, lot Tcl typé en cases"          3 {lmap M $L {mat::tcl_det $M}}
mesure "$N déterminants, list Tcl typé en cases"          3 {lmap M $I {mat::tcl_det $M}}
mesure "$N déterminants, C sur le lot en cases"     20 {mat::lot_det $L}
mesure "$N déterminants, C sur une liste brute"     20 {mat::lot_det $brut}
