# list.any : liste hétérogène, chaque élément garde sa propre annotation.
set nfail 0
proc check {what got want} {
    global nfail
    if {$got eq $want} {puts "ok: $what"} else {incr nfail; puts "ECHEC: $what -> '$got' (attendu '$want')"}
}
set {list.any}L {int}1 {double}1.0 {string}{a b} {list}{e f g} plain
check "valeur" $L {1 1.0 {a b} {e f g} plain}
check "type de la liste" [type get $L] list<any>
check "foreach" [lmap e $L {type get $e}] {int double string list any}
check "le filtre de l'énoncé" [lmap e $L {if {[type get $e] eq "int"} {set e} else continue}] 1
check "lindex" [type get [lindex $L 1]] double
check "L(0)" [type get $L(0)] int
check "L(3)" [type get $L(3)] list
check "lindex imbriqué" [lindex $L 3 1] f
# atomicité : l'élément {a b} n'est pas redécoupé
check "atomique" [lmap e $L {if {[type get $e] eq "string"} {llength $e}}] {{} {} 1 {} {}}
set n 0
foreach e $L {foreach f $e {if {[type get $e] eq "string"} {incr n}}}
check "foreach interne sur un string" $n 1
# lappend garde l'annotation
lappend L {int}7
check "lappend" [lmap e $L {type get $e}] {int double string list any int}
# validation à la construction
check "refus" [catch {set {list.any}B {int}abc}] 1
# dans une procédure
proc t {} {
    set {list.any}M {int}1 {double}2.5 {string}{x y}
    lmap e $M {type get $e}
}
check "procédure" [t] {int double string}
# les listes nues restent classiques
set {list}P {int}1 {double}2.0
check "{list} nu inchangé" [lmap e $P {type get $e}] {{} {}}
# types successifs dans une même variable de boucle : pas de type mismatch
set {list.any}H {int}1 {double}1.5 {string}{a b} {list}{e f} {int}2 plain
set r {}
foreach e $H {lappend r [type get $e]}
check "types successifs (global)" $r {int double string list int any}
proc succ {H} {set r {}; foreach e $H {lappend r [type get $e]}; set r}
check "types successifs (proc)" [succ $H] {int double string list int any}
proc succ2 {H} {set {int}e 3; lmap e $H {type get $e}}
check "variable int préalable (proc)" [succ2 $H] {int double string list int any}
proc succ3 {H} {set r {}; foreach {a b} $H {lappend r [type get $a],[type get $b]}; set r}
check "deux variables" [succ3 $H] {int,double string,list int,any}
puts [expr {$nfail ? "$nfail ECHEC(S)" : "tout est ok"}]
