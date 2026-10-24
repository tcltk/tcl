# t75 : foreach / lmap sur une liste typée : la variable de boucle reçoit
#   l'élément typé, dans une procédure (compilé) comme au niveau global, avec
#   plusieurs variables ou plusieurs listes, une variable sous contrat, une
#   liste modifiée pendant la boucle, une somme, la liste nue {list}.
type def point {{double}x; {double}y}
type def polygone list.point
type def forme {{point}p, {double}r}

set {polygone}P {0 0} {1 0} {1 1}
proc types {L} {set r {}; foreach q $L {lappend r [type get $q]}; set r}
puts "proc foreach      : [types $P]"
set r {}; foreach q $P {lappend r [type get $q]}
puts "global foreach    : $r"
eval {set r {}; foreach q $P {lappend r [type get $q]}}
puts "eval foreach      : $r"
puts "lmap \$q(x)        : [lmap q $P {set q(x)}]"
puts "apply lmap        : [apply {L {lmap q $L {expr {$q(x) + $q(y)}}}} $P]"
puts "après la boucle   : $q [type get $q]"
foreach {a b} $P {puts "deux variables    : $a ([type get $a]) / $b ([type get $b])"}
foreach a $P k {u v w} {puts -nonewline "$k=[type get $a] "}; puts ""
proc contrat {L} {
    set {point}q 9 9
    foreach q $L {}
    list $q [type get $q] [catch {set q abc} m] $m
}
puts "sous contrat      : [contrat $P]"
proc modifie {L} {
    set {polygone}M {*}$L; set n 0
    foreach q $M {lappend M {5 5}; incr n}
    list $n [llength $M] [type get [lindex $M end]]
}
puts "modifiée          : [modifie $P]"
set Q $P
foreach q $Q {}
puts "copie partagée    : [type get $q]"
set {list.forme}S {p}{0 0} {r}2.5
puts "somme             : [lmap s $S {type get $s}]"
set {list}N {a b} {c d}
puts "liste nue         : [lmap e $N {list $e [type get $e]}]"
set {list.list.int}LL {1 2} {3 4}
puts "liste de listes   : [lmap e $LL {list $e [type get $e]}]"
proc vide {L} {set n 0; foreach q $L {incr n}; set n}
puts "liste vide        : [vide [set {polygone}V {*}{}]]"
puts "chaîne inchangée  : $P"
