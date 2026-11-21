# t116 (v17j) : les méthodes TclOO prennent les signatures typées des
#   procédures (v16p) : {int}x, un produit ({string}a; {int}b ?= 5), un type
#   de retour ({double}m). Avant, la liste d'arguments était lue comme une
#   liste ordinaire (« list element in braces followed by "x" »). L'objet
#   garde ses variables typées d'un appel à l'autre ; l'introspection rend
#   les noms et les défauts, comme info args pour une procédure.
proc t {s} {puts [format "%-62s %s" $s [expr {[catch {uplevel 1 $s} r] ? "ERREUR $r" : $r}]]}
type def Fiche {{string}nom; {int}stock ?= 0}
type def Point {{double}x; {double}y}
oo::class create P {
    variable fiche
    constructor {{string}nom; {int}stock} { set {Fiche}fiche [list $nom $stock] }
    method ajouter {{int}n} { set fiche(stock) [expr {$fiche(stock) + $n}] }
    method get {c} { set fiche($c) }
    method genre {{int}n} { type get $n }
    method opt {{int}n; {int}m ?= 5} { list $n $m }
    method {double}moitié {{int}n} { expr {$n / 2.0} }
    method {int}rend {} { return "pas un entier" }
    method pt {{Point}p} { list $p(x) $p(y) [type get $p] }
}
set o [P new vis 3]
t {$o ajouter 2}
t {$o ajouter deux}
t {$o genre 7}
t {list [$o opt 1] [$o opt 1 2]}
t {$o moitié 3}
t {$o rend}
t {P new vis trois}
t {$o pt {1 2}}
t {$o get stock}
t {oo::objdefine $o method seul {{int}k} {expr {$k * 10}}; $o seul 4}
t {$o seul x}
t {oo::class create Q {method m {a b} {list $a $b}}; [Q new] m 1 2}
t {oo::class create P2 {superclass P; method ajouter {{int}n} {next [expr {$n*2}]}}; [P2 new a 1] ajouter 5}
t {info class definition P opt}
t {lindex [info class constructor P] 0}
t {lindex [info object definition $o seul] 0}
t {oo::configurable create R {property y}; set r [R new]; $r configure -y 4; $r configure -y}
# v17k : TclCreateTypedProc, un seul point d'entrée pour proc, apply et TclOO
# - la copie d'un objet (oo::copy) garde la signature de ses méthodes propres
t {set c [oo::copy $o]; list [$c seul 4] [catch {$c seul x}]}
t {oo::copy P P3; [P3 new b 2] moitié 5}
# - les types d'une méthode se lisent dans l'espace de sa classe
namespace eval geom {
    type def point {{double}x; {double}y}
    oo::class create C { method {double}nx {{point}p} { return $p(x) } }
}
t {[geom::C new] nx {1 2}}
t {oo::define geom::C method ny {{point}p} { return $p(y) }; [geom::C new] ny {1 2}}
# - un objet anonyme : l'espace de sa classe
t {set a [geom::C new]; oo::objdefine $a method nz {{point}p} { expr {$p(x)+$p(y)} }; $a nz {1 2}}
# - une liste classique mal formée : la même erreur que proc
t {list [catch {proc f {a "b"c} {}} e1] [catch {oo::class create E {method m {a "b"c} {}}} e2] [expr {$e1 eq $e2}]}
