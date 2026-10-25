# t79 : champ d'un produit typé sur place à la première lecture (contenu
#   privé seulement : une liste partagée ne change pas), boucle sur une
#   liste en cases, déclaration X is T = v directe (contrat posé, trace,
#   lien upvar, tableau : chemin ordinaire).
type def point {{double}x; {double}y}
type def {-compact}nuage list.point

set brut {{1.5 2.5} {3.5 4.5}}
set {list.point}P {*}$brut
proc somme {L} {set s 0; foreach q $L {set s [expr {$s + $q(x) + $q(y)}]}; set s}
puts "somme : [somme $P] [somme $P]"
puts "brut intact : [type get [lindex $brut 0]] [type get [lindex $brut 0 0]] / $brut"
set {point}p 1.0 2.0
proc lit {a} {list $a(x) [type get $a(x)] $a(x) $a(y)}
puts "lectures : [lit $p] [lit $p] $p [type get $p]"
set q $p; set q(x) 7.0
puts "copie modifiée : q=$q p=$p"

set {nuage}N {0.5 1.5} {2.5 3.5} {4.5 5.5}
puts "cases : [tcl::unsupported::compact info $N]"
puts "boucle en cases : [somme $N] [lmap e $N {type get $e}] [lmap e $N {set e(y)}]"
proc compte {L} {set n 0; foreach {a b} $L {incr n}; set n}
puts "deux variables : [compte $N]"

proc decl {} {(
    A is point = (1.0, 2.0);
    [list $A [type get $A] [catch {set A abc} m] $m]
)}
puts "déclaration : [decl]"
proc trace_decl {} {
    set ::vu {}
    trace add variable B write {apply {{n i o} {lappend ::vu $n}}}
    expr {B is point = (3.0, 4.0)}
    list $B $::vu
}
puts "avec trace : [trace_decl]"
proc lien {nom} {upvar 1 $nom v; expr {v is point = (5.0, 6.0)}; set v}
set W {}
puts "par upvar : [lien W] $W [type get $W]"
