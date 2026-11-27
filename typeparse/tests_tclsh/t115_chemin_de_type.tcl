# t115 (v17i) : T(chemin), le type d'une partie, comme $v(chemin) en est la
#   valeur : list <=> tableau (indice, end, *), dict et produit <=> struct
#   (clé, champ) ; T(a.b) descend ; S/v dans un chemin : le type de la
#   variante. S(v) seul garde son sens (la variante choisie).
proc t {s} {puts [format "%-62s %s" $s [expr {[catch {uplevel 1 $s} r] ? "ERREUR $r" : $r}]]}
type def Emplacement_propriétés {{string}code; {int}stock ?= 0; {int}max ?= 10}
type def Emplacement_ref string
type def Emplacement {{Emplacement_ref}référence; {Emplacement_propriétés}propriétés}
t {type get [type cast Emplacement(propriétés) {E1 3 10}]}
t {type get [type cast Emplacement(référence) E1]}
t {type get [type cast Emplacement(propriétés.stock) 3]}
t {set {Emplacement(propriétés)}p E1 3 10; list $p [type get $p]}
t {proc f {{Emplacement(référence)}e} {type get $e}; f E1}
t {type def Liste list.Emplacement; type get [type cast Liste(0) {E1 {E1 3 10}}]}
t {type get [type cast Liste(*.propriétés.max) 4]}
t {type def C {dict.int}; type get [type cast C(qqch) 4]}
t {type def K {dict(a, b).int}; list [type get [type cast K(a) 1]] [catch {type cast K(c) 1} m] $m}
t {type def S {{int}i, {Emplacement}e}; type def P {{S}s}; type get [type cast P(s.e.référence) X]}
t {type get [type cast S(i) 5]}
t {type cast Emplacement(zz) 1}
t {type cast Emplacement(propriétés.stock.x) 1}
t {type cast Liste(a) 1}
t {type cast Emplacement() 1}
t {type canonic [type cast Emplacement(propriétés) {E1 3 10}]}
t {type def pt {{double}x; {double}y}; type get [type cast pt(x) 1]}
t {set {list(2).Emplacement(propriétés)}L {E1 1 2} {E2 3 4}; type get $L}
