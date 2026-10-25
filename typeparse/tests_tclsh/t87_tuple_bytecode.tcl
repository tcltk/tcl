# t87 : affectation par tuple dans expr, "(a, (b, c), d) = v", compilée en une
#   instruction (tupleAssign) au lieu de ::tcl::unsupported::content et d'une
#   suite listIndexImm / storeStk par nom : mêmes valeurs (contenu profond d'un
#   produit, élément absent = ""), mêmes erreurs, traces, liens, contrats.
type def point {{double}x; {double}y}
type def seg {{point}a; {point}b}
type def {-compact}nuage list.point
proc instr {nom} {
    set d [tcl::unsupported::disassemble proc $nom]
    list [regexp -all tupleAssign $d] [regexp -all {unsupported::content} $d]
}
proc t1 {v} {( (a, (b, c), d) = $v; [list $a $b $c $d] )}
puts "imbriqué : [t1 {1 {2 3} 4}] / [t1 {1 2}] / [t1 {}] [instr t1]"
proc t2 {} {( (x, y) = (1.5, 2.5); [list $x $y] )}
puts "littéral : [t2]"
proc t3 {s} {( ((ax, ay), (bx, by)) = $s; [list $ax $ay $bx $by [type get $s]] )}
set {seg}s(a,b) {1.0 2.0} {3.0 4.0}
puts "produit : [t3 $s]"
proc t4 {n} {( (p, q) = $n; [list $p $q [type get $p]] )}
set {nuage}N {0.5 1.5} {2.5 3.5}
puts "cases : [t4 $N] [tcl::unsupported::compact info $N]"
proc t5 {} {( r = ((a, b) = (7, 8)); [list $r $a $b] )}
puts "valeur : [t5]"
proc t6 {} {
    set ::ecr {}
    trace add variable b write {apply {{n i op} {lappend ::ecr $n}}}
    expr {(a, b) = (1, 2)}
    list $a $b $::ecr
}
puts "trace : [t6]"
proc t7 {} {upvar 0 ::G a; expr {(a, b) = (5, 6)}; list $::G $b}
puts "lien : [t7]"
proc t8 {} {array set a {x 1}; list [catch {expr {(a, b) = (1, 2)}} m] $m}
puts "tableau : [t8]"
proc t9 {} {( P is point = (1.0, 2.0); [list [catch {expr {(P, b) = ({abc}, 2)}} m] $m $P] )}
puts "contrat : [t9]"
proc t10 {v} {list [catch {expr {(a, b) = $v}} m] $m}
puts "liste invalide : [t10 "x \{y"]"
puts "global : [expr {(g1, (g2, g3)) = (1, (2, 3))}] $g1 $g2 $g3"
