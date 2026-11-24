# t120 : spécialisations de types (table d'opérations, registre, match) ;
# P($i.champ) lu sans former l'index ; affectation sous contrat sur place.
type def pt {{double}x; {double}y}
type def {-compact}pts list.pt
foreach T {list.double list.int list.string list.pt pts int pt dict} {
    puts [format "%-12s %s" $T [list [type ops $T]]]
}
proc champs {} {
    set {pts}P [type cast pts {{1.5 2.5} {3.5 4.5} {5.5 6.5}}]
    set {list.pt}Q {1 2} {3 4}
    set r {}
    for {set i 0} {$i < 3} {incr i} { lappend r $P($i.x) $P($i.y) }
    lappend r [type canonic $P(2.y)] $Q(1.x) [type canonic $Q(0.y)]
    puts "champs : $r"
    foreach c {{set P(3.x)} {set P(end.x)} {set P(0.z)} {set i -1; set P($i.x)} {set i 01; set P($i.x)}} {
	catch $c e; puts "  [list $c] -> $e"
    }
}
champs
proc liste {} {
    set {list.double}L 1 2.5 0x10
    set {list.int}M 0x10 7
    puts "lindex : [lindex $L 0] [lindex $L 2] [type canonic [lindex $M 0]] [lindex $L 9]"
}
liste
proc contrat {} {
    set {double}x 1.5
    set y $x
    set x [expr {2.5 * 2}]
    puts "contrat : x=$x y=$y [type canonic $x]"
    set {int}n 0
    for {set i 0} {$i < 5} {incr i} { set n [expr {$n + $i}] }
    puts "contrat : n=$n [type canonic $n]"
    catch {set n [expr {1.5}]} e; puts "contrat : $e"
    set l {}
    for {set i 0} {$i < 3} {incr i} { set x [expr {$i * 0.5}]; lappend l $x }
    puts "contrat : l=$l"
}
contrat
