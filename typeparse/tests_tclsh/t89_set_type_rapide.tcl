# v16g : set {T}x a b... d'un produit de scalaires (voie rapide) et
# lecture numérique des champs pour les opérateurs de bits dans expr.
type def point {{double}x; {double}y}
type def pi {{int}a; {string}b}
type def seg {{point}p; {point}q}

proc essai {nom script} {
    if {[catch {uplevel 1 $script} r]} {puts "$nom : erreur : $r"} else {puts "$nom : $r"}
}

proc p1 {} {
    set {point}A 1.0 2.0
    list $A [type get $A] [info locals]
}
essai "produit de doubles" {p1}

proc p2 {} {
    set {pi}Q 3 zz
    list $Q [type get $Q] $Q(a) $Q(b)
}
essai "int et chaîne" {p2}

proc p3 {} { set {point}A 1.0 abc }
essai "valeur refusée (voie générale)" {p3}

proc p4 {} { set {pi}A 3.5 zz }
essai "entier refusé" {p4}

proc p5 {} { set {point}A 1.0 }
essai "arité" {p5}

proc p6 {} { set {point}A {x}1.0 {y}2.0; set A }
essai "champs nommés" {p6}

proc p7 {} { set p [set {point}P 1 2]; set {seg}S $P $P; list $S [type get $S] }
essai "produit de produits" {p7}

proc p8 {} { set {pi}A 1 x; set A(a) 5; set A(a) zz }
essai "contrat après la voie rapide" {p8}

proc p9 {} { upvar 1 G g; set {point}g 4 5; return $g }
set G {}
essai "variable liée (upvar)" {list [p9] $G [type get $G]}

proc p10 {} { set {point}A(x) 7; set A }
essai "chemin de constructeur" {p10}

proc p11 {} { set {int}A(k) 5 }
essai "élément de tableau refusé" {p11}

proc p12 {} { set v 1.5; set w 2.5; set {point}A $v $w; list $A [type get $A] }
essai "valeurs substituées" {p12}

# Opérateurs de bits : le champ est lu comme nombre (loadFieldNum).
proc b1 {} {
    set {pi}q 13 a
    list [expr {$q(a) & 3}] [expr {$q(a) | 16}] [expr {$q(a) ^ 1}] \
	[expr {$q(a) << 2}] [expr {$q(a) >> 1}] [expr {~$q(a)}]
}
essai "bits sur un champ" {b1}
proc b2 {} { set {point}p 1.5 2; expr {$p(x) & 1} }
essai "bits sur un double" {b2}
proc b3 {} { array set t {k 6}; expr {$t(k) & 3} }
essai "tableau ordinaire" {b3}
essai "bytecode : lectures numériques" {regexp -all {loadFieldNum} [tcl::unsupported::disassemble proc b1]}
