# La console Tk de type debug : tclsh console.tcl (ou wish console.tcl)
# Arrêt sur {@step}, pas à pas (F10), pile et variables (avec type et notes),
# commandes Tcl dans le cadre choisi. Une condition : {@neg} ne s'arrête que
# sur une valeur négative.
package require Tk
wm withdraw .
type def pt {
    {double}x;
    {double}y
}

proc {double}norme {
    {pt}p
} {
    set x2 [expr {$p(x) ** 2}]
    return [expr {sqrt($x2 + [lindex $p 1] ** 2)}]
}

proc chemin {L} {
    set s 0.0
    foreach p $L { set s [expr {$s + [norme $p]}] }
    return $s
}
proc racine {x} {( $x > 0 ? sqrt($x) : (0, sqrt(-$x)))}

type debug on
type debug console
type debug when neg {$value < 0}

set P {@step}{3.0 4.0}
puts "chemin : [chemin [list $P {@trace}{1.0 0.0} {3.0 5.0}]]"
foreach v {4 -1 9} { puts "racine $v : [racine {@neg}$v]" }
type debug off
exit

