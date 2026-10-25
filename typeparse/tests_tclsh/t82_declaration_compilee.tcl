# t82 : "X is T = v" compilé dans une procédure (instruction declareLocal) :
#   mêmes valeurs, mêmes contrats et mêmes messages que la commande
#   ::tcl::unsupported::declare (hors procédure, nom qualifié, tableau) ;
#   la liste neuve (a, b) devient le contenu, une valeur partagée est copiée.
type def point {{double}x; {double}y}
type def pdef {{double}x; {double}y = 0.5}
type def {-compact}nuage list.point
type def forme {{double}cote, {string}nom}

proc compile {nom} {expr {[string first declareLocal [tcl::unsupported::disassemble proc $nom]] >= 0}}
proc p1 {} {( A is point = (1.0, 2.0); [list $A [type get $A] [catch {set A abc} m] $m] )}
puts "point : [p1] compilé [compile p1]"
proc p2 {} {( A is pdef = [list 1.0]; [list $A [type get $A]] )}
puts "défaut : [catch p2 m] $m"
proc p3 {x} {( A is double = $x; [list $A [type get $A] [catch {set A z} m] $m] )}
puts "double : [p3 1.5] [p3 3]"
proc p4 {} {( A is point = (1.0, {abc}) )}
puts "refus : [catch p4 m] $m"
puts "  $::errorInfo"
proc p5 {} {( L = (1.0, 2.0); A is point = $L; [list $L [type get $L] $A [type get $A]] )}
puts "partagée : [p5]"
proc p6 {p} {( A is point = $p; [list $A [type get $A]] )}
set {point}q 3.0 4.0
puts "déjà typée : [p6 $q]"
proc p7 {} {( N is nuage = ((1.0, 2.0), (3.0, 4.0)); [list $N [expr {[tcl::unsupported::compact info $N] ne ""}]] )}
puts "compacte : [p7]"
proc p8 {s} {( S is forme = $s; [list $S [type get $S]] )}
puts "somme : [p8 {cote}2.0] [p8 {nom}carré]"
proc p9 {} {
    upvar 0 B A
    expr {A is point = (5.0, 6.0)}
    list $B [type get $B] [catch {set B x} m] $m
}
puts "lien : [p9]"
proc p10 {} {
    set A 0
    trace add variable A write {apply {{n i op} {lappend ::traces $n}}}
    expr {A is point = (7.0, 8.0)}
    list $A [type get $A] $::traces
}
set traces {}
puts "trace : [p10]"
proc p11 {} {expr {{::G} is point = (1.0, 1.0)}; list $::G [type get $::G]}
puts "qualifié : [p11] compilé [compile p11]"
puts "global : [expr {H is point = (2.0, 2.0)}] [type get $H]"
proc p12 {n} {for {set i 0} {$i < $n} {incr i} {expr {A is point = ($i, 1.0)}}; list $A [type get $A]}
puts "boucle : [p12 3]"
proc p13 {} {( A is inconnu = 1 )}
puts "type inconnu : [catch p13 m] $m"
