# t80 : lappend dans une liste typée sous contrat : ajout sur place quand la
#   variable est seule à tenir la valeur (temps linéaire), mêmes résultats et
#   mêmes messages qu'avant (copie partagée, élément refusé,
#   plusieurs éléments, forme compacte, somme, list.any, list(N)).
type def point {{double}x; {double}y}
type def pdef {{double}x; {double}y = 0.5}
type def {-compact}nuage list.point
type def forme {{double}cote, {string}nom}

proc remplit {N} {set {list.point}L; for {set i 0} {$i < $N} {incr i} {lappend L [list $i 1]}; list [llength $L] [lindex $L end] [type get $L] [type get [lindex $L 0]]}
puts "boucle : [remplit 1000]"
set {list.point}L {0 0}
set M $L
lappend L {1 1}
puts "copie : L=$L M=$M"
puts "refus : [catch {lappend L {a b}} m] $m ; L=$L"
puts "refus 2 : [catch {lappend L {1 2 3}} m] $m"
lappend L {2 2} {3 3}
puts "plusieurs : $L [llength $L]"
set {list.pdef}D {1 2}
puts "défaut absent : [catch {lappend D {3}} m] $m ; D=$D"
set {nuage}N {0.5 1.5}
proc nuage_plus {} {set {nuage}N {0.5 1.5}; lappend N {2.5 3.5}; lappend N {4.5 5.5}; list $N [expr {[tcl::unsupported::compact info $N] ne ""}] [catch {lappend N {x y}} m] $m $N}
puts "compacte : [nuage_plus]"
set {list.forme}S {cote}2.0
lappend S {nom}carré
puts "somme : $S [lmap e $S {type get $e}]"
set {list.any}A {int}1
lappend A {string}x
puts "list.any : [lmap e $A {type get $e}]"
set {list(2).point}C {0 0} {1 1}
puts "list(2) : [catch {lappend C {2 2}} m] $m"
proc pendant {} {
    set {list.point}L {0 0} {1 1}
    foreach p $L {lappend L {9 9}}
    list [llength $L] $L
}
puts "pendant foreach : [pendant]"
set {point}p 7 8
lappend L $p
puts "élément typé : [lindex $L end] [type get [lindex $L end]]"
puts "chaîne : [string length $L] [lindex $L 1]"
