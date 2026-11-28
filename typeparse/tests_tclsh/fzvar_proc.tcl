# fuzz v18b : types de variante, chaînes, littéraux, sommes imbriquées
expr {srand([lindex $argv 0])}
type def temp {{double}c, {double}f}
type def S {{int}i, {string}s, "auto"}
type def M {{temp}t, {S}x, "none"}
type def N {{M}m, {int}k}
proc pick {l} {lindex $l [expr {int(rand()*[llength $l])}]}
set nfail 0
proc chk {what got want} {
    global nfail
    if {$got ne $want} {incr nfail; puts "ECHEC $what: '$got' ≠ '$want'"}
}
# une valeur aléatoire et sa chaîne attendue
proc gen {} {
    switch [expr {int(rand()*7)}] {
	0 {set v [pick {c f}]; return [list "{/t/$v}[expr {int(rand()*100)}]" M/t/$v M]}
	1 {return [list "{/x/i}[expr {int(rand()*100)}]" M/x/i M]}
	2 {return [list "{/x/s}a[expr {int(rand()*9)}]" M/x/s M]}
	3 {return [list none M/none M]}
	4 {return [list "{/x/auto}auto" M/x/auto M]}
	5 {set v [pick {c f}]; return [list "{/m/t/$v}[expr {int(rand()*100)}]" N/m/t/$v N]}
	6 {return [list "{/k}[expr {int(rand()*9)}]" N/k N]}
    }
}
proc main {} {
    global nfail
for {set n 0} {$n < 200} {incr n} {
    lassign [gen] lit want T
    if {[catch {eval "set {$T}v $lit"} m]} {incr nfail; puts "ECHEC construire $lit: $m"; continue}
    chk "type get $lit" [type get $v] $want
    set c [type canonic $v]
    if {[catch {type parse $c} y]} {incr nfail; puts "ECHEC parse $c: $y"; continue}
    chk "aller-retour $c" [type get $y] $want
    chk "valeur $c" $y $v
    # le type de variante complet accepte la valeur, et pas une autre
    chk "cast $want" [type get [type cast $want $v]] $want
    chk "is $want" [expr {v is $want}] 1
    # dans une liste, dans une procédure
    eval "set {list.$T}L \$v \$v"
    chk "liste" [lmap e $L {type get $e}] [list $want $want]
    chk "liste canonic" [lmap e [type parse [type canonic $L]] {type get $e}] [list $want $want]
    chk "type variant" [type variant $v] [lindex [split $want /] 1]
    proc p {x} [list type get \$x]
    #
    proc q "{$T}x" {return $x}
    chk "argument typé" [type get [q $v]] $want
    proc r {} "return $lit"
    eval "proc {$T}r2 {} {return $lit}"
    chk "retour" [type get [r2]] $want
    # variable déclarée du type complet
    if {[catch {eval "set {$want}w \$v"} m]} {incr nfail; puts "ECHEC {$want}w: $m"} else {chk "var" [type get $w] $want}
}
}
main
puts "fin, $nfail échecs"
