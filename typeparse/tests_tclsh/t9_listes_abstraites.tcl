# Une valeur annotée répond aux commandes de liste par les procédures
# de Tcl_ObjType V2 : un scalaire typé est UN élément, un contenu list/dict/
# produit est vu comme sa liste.
proc check {script expected} {
    set r [uplevel 1 $script]
    if {$r ne $expected} { puts "ECHEC: $script -> '$r' (attendu '$expected')" } else { puts "ok: $script" }
}
set S {string}{hello world}
check {lindex $S 0} {hello world}
check {llength $S} 1
check {lindex $S 1} {}
check {lrange $S 0 0} {{hello world}}
check {expr {"hello world" in $S}} 1
check {expr {"hello" in $S}} 0
check {lsearch $S "hello world"} 0
check {string length $S} 11
set {list.int}M 4 5 6
check {lindex $M 1} 5
check {llength $M} 3
check {lrange $M 1 2} {5 6}
check {lsort -integer -decreasing $M} {6 5 4}
check {lreverse $M} {6 5 4}
check {expr {5 in $M}} 1
set {list}L {list}{a b {c d}}
check {lindex $L 2 1} d
set {int}i 42
check {list [llength $i] [lindex $i 0]} {1 42}
