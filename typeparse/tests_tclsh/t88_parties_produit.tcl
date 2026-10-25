# t88 : $P(*) d'un produit typé (toutes ses parties) par le raccourci de
#   lecture : mêmes valeurs et mêmes types que le chemin général (types
#   différents : list.any des champs typés ; tous identiques : list.T),
#   produit imbriqué, forme compacte, lassign ; les autres formes ($P(*.x),
#   sommes, any) restent au chemin général.
type def Sample {{int}id; {double}value; {int}flags}
type def point {{double}x; {double}y}
type def seg {{point}a; {point}b}
type def {-compact}nuage list.point
type def forme {{double}cote, {string}nom}
type def boite {{forme}f; {int}n}
proc montre {v} {list $v [type get $v] [lmap e $v {type get $e}]}
proc p1 {s} {montre $s(*)}
set {Sample}s 1 2.5 0
puts "mixte : [p1 $s]"
set {point}p 1.0 2.0
puts "identiques : [p1 $p]"
set {seg}g(a,b) {1.0 2.0} {3.0 4.0}
puts "imbriqué : [p1 $g] [montre $g(*.x)]"
set {nuage}N {0.5 1.5} {2.5 3.5}
proc p2 {N} {set q [lindex $N 1]; montre $q(*)}
puts "case : [p2 $N]"
proc p3 {s} {lassign $s(*) a b c; list $a $b $c [type get $b]}
puts "lassign : [p3 $s]"
set {boite}bx(f,n) {cote}2.0 3
puts "somme : [p1 $bx]"
proc p4 {s} {set s(value) 9.5; montre $s(*)}
puts "après écriture : [p4 $s] $s"
