# Le pas à pas par une note : tclsh pas_a_pas.tcl, puis à l invite c, s, p p, r... (h : aide)
type def pt {{double}x; {double}y}
proc {double}norme {{pt}p} {
    set x2 [expr {$p(x) ** 2}]
    return [expr {sqrt($x2 + [lindex $p 1] ** 2)}]
}
proc chemin {L} {
    set s 0.0
    foreach p $L { set s [expr {$s + [norme $p]}] }
    return $s
}
type debug on
set P {@step}{3.0 4.0}
chemin [list $P {1.0 0.0}]
