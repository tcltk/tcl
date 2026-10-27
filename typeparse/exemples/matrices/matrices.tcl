# matrices.tcl --
# FR : les types des matrices 3×3 en cases et les commandes de confort.
#      Les mêmes types, exportés par `type {c!}export`, donnent mat_types.h
#      (que mat.c inclut) : Tcl et C décrivent les mêmes octets.
# EN : the types of the 3×3 matrices held in cells, and convenience
#      commands. Exported by `type {c!}export`, the same types give
#      mat_types.h (included by mat.c): Tcl and C describe the same bytes.

namespace eval mat {variable dir [file dirname [file normalize [info script]]]}

# Les types : une ligne, une matrice (3 lignes), un lot de matrices.
# Rangés en cases (v16h) : une matrice est un double[3][3] de 72 octets,
# un lot un tableau de telles matrices.
proc mat::types {} {
    type def ligne3 list(3).double
    type def {-compact}matrix3 list(3).ligne3
    type def {-compact}lot list.matrix3
}

proc mat::init {{lib ""}} {
    variable dir
    mat::types
    if {$lib eq ""} {set lib [file join $dir mat[info sharedlibextension]]}
    ::load $lib Mat
}

# L'en-tête C attendu (à comparer à mat_types.h, ou à régénérer).
proc mat::header {} {
    return [type {c!}export matrix3]
}

# --- les mêmes calculs en Tcl, sur les valeurs typées ---

proc mat::tcl_trace {M} {
    expr {$M(0.0) + $M(1.1) + $M(2.2)}
}

proc mat::tcl_det {M} {
    expr {$M(0.0) * ($M(1.1) * $M(2.2) - $M(1.2) * $M(2.1))
	- $M(0.1) * ($M(1.0) * $M(2.2) - $M(1.2) * $M(2.0))
	+ $M(0.2) * ($M(1.0) * $M(2.1) - $M(1.1) * $M(2.0))}
}

proc mat::tcl_mul {A B} {
    set {matrix3}C {0 0 0} {0 0 0} {0 0 0}
    for {set i 0} {$i < 3} {incr i} {
	for {set j 0} {$j < 3} {incr j} {
	    set C($i.$j) [expr {$A($i.0) * $B(0.$j) + $A($i.1) * $B(1.$j)
		+ $A($i.2) * $B(2.$j)}]
	}
    }
    return $C
}

# Une rotation d'angle a (radians) autour de l'axe z.
proc mat::rotz {a} {
    set c [expr {cos($a)}]; set s [expr {sin($a)}]
    set {matrix3}R [list $c [expr {-$s}] 0.0] [list $s $c 0.0] {0.0 0.0 1.0}
    return $R
}

proc mat::rotz {a} {(
    (c, s)=(cos($a), sin($a));
    R is matrix3 = (($c, -$s, 0.0), ($s, $c, 0.0), (0.0, 0.0, 1.0));
    return $R
)}
    
#catch {console show}

# mat::init
# load mat.dll

# set {matrix3}M {2 2 3} {4 5 6} {7 8 9}

proc mat::tcl.c.inv {m} {
    if {[set det [mat::det $m]] != 0} {
	return [mat::mulByScal [mat::adj $m] [(1.0/$det)]]
    } else {
	error "can't inverse matrix $m : det = 0"
    }
}

proc mat::tcl.inv {matrix} {
    if {[llength $matrix] != 3 ||
        [llength [lindex $matrix 0]] != 3 || 
        [llength [lindex $matrix 1]] != 3 || 
        [llength [lindex $matrix 2]] != 3} {
        error "wrong sized matrix"
    }
    set inv {{? ? ?} {? ? ?} {? ? ?}}
 
    # Get adjoint matrix : transpose of cofactor matrix
    for {set i 0} {$i < 3} {incr i} {
        for {set j 0} {$j < 3} {incr j} {
            lset inv $i $j [::_Cofactor3 $matrix $i $j]
        }
    }
    # Now divide by the determinant
    set det [expr {double([lindex $matrix 0 0]   * [lindex $inv 0 0]
                   + [lindex $matrix 0 1] * [lindex $inv 1 0]
                   + [lindex $matrix 0 2] * [lindex $inv 2 0])}]
    if {$det == 0} {
        error "non-invertable matrix"
    }
    
    for {set i 0} {$i < 3} {incr i} {
        for {set j 0} {$j < 3} {incr j} {
            lset inv $i $j [expr {[lindex $inv $i $j] / $det}]
        }
    }
    return $inv
}
proc _Cofactor3 {matrix i j} {
    array set COLS {0 {1 2} 1 {0 2} 2 {0 1}}
    foreach {row1 row2} $COLS($j) break
    foreach {col1 col2} $COLS($i) break
    
    set a [lindex $matrix $row1 $col1]
    set b [lindex $matrix $row1 $col2]
    set c [lindex $matrix $row2 $col1]
    set d [lindex $matrix $row2 $col2]
 
    set det [expr {$a*$d - $b*$c}]
    if {($i+$j) & 1} { set det [expr {-$det}]}
    return $det
 }

# proc bench {M} {
#     puts "c only : [timerate {mat::c.inv $M}]"
#     puts "tcl + c : [timerate {mat::tcl.c.inv $M}]"
#     puts "tcl only : [timerate {mat::tcl.inv $M}]"
# }
