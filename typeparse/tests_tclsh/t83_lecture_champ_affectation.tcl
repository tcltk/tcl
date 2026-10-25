# t83 : dans expr compilé, $a(x) opérande d'un opérateur arithmétique ou
#   d'une comparaison lit le nombre du champ sans l'envelopper (instruction
#   loadFieldNum ; dans une vue sur une case, lu dans la case) ; sinon c'est
#   loadArray : mêmes valeurs, mêmes erreurs, traces. "nom = v" vers une
#   variable locale : storeScalar (traces, contrat, lien).
type def point {{double}x; {double}y}
type def pi {{int}n; {point}p; {string}s}
type def {-compact}nuage list.point
type def forme {{double}cote, {string}nom}

proc instr {nom} {
    set d [tcl::unsupported::disassemble proc $nom]
    list [regexp -all loadFieldNum $d] [regexp -all {storeStk} $d]
}
proc f1 {a b} {expr {$a(x)*$b(y) - $b(x)*$a(y)}}
set {point}p 1.5 2.5
set {point}q 3.0 -1.0
puts "produit : [f1 $p $q] [instr f1]"
proc f2 {N} {( s = 0.0; foreach e $N {( s = $s + $e(x) - $e(y) )}; $s )}
set {nuage}N {0.5 1.5} {2.5 3.5} {4.5 5.5}
puts "cases : [f2 $N] [tcl::unsupported::compact info $N] [instr f2]"
proc f3 {v} {list [expr {$v(n) + 1}] [expr {$v(n) < 3}] [expr {-$v(n)}] [expr {$v(n) == 2}] [expr {$v(n) ** 2}] [expr {$v(n) % 2}]}
set {pi}w 2 {1.0 2.0} txt
puts "entier : [f3 $w] [instr f3]"
proc f4 {v} {list [catch {expr {$v(p) + 1}} m] $m [catch {expr {$v(s) + 1}} m] $m [catch {expr {$v(z) + 1}} m] $m}
puts "repli : [f4 $w]"
proc f5 {} {array set a {x 4 y 5}; expr {$a(x) * $a(y)}}
puts "tableau : [f5]"
proc f6 {} {set l {1 2}; list [catch {expr {$l(x) + 1}} m] $m [catch {expr {$nul(x) + 1}} m] $m}
puts "erreurs : [f6]"
proc f7 {a} {
    set ::lus {}
    trace add variable a read {apply {{n i op} {lappend ::lus $n/$i/$op}}}
    list [expr {$a(x) + 1}] $::lus
}
puts "trace lecture : [f7 $p]"
proc f8 {} {upvar 0 ::p a; expr {$a(x) * 2}}
puts "lien : [f8]"
proc f9 {s} {expr {$s(cote) * 2}}
puts "somme : [catch {f9 {cote}3.0} m] $m"
proc f10 {a} {expr {$a(x) eq "1.5"}}
puts "eq : [f10 $p] [instr f10]"

proc g1 {x} {( A = $x * 2; B = C = 3; [list $A $B $C] )}
puts "affectations : [g1 2] [instr g1]"
proc g2 {} {
    set ::ecrits {}
    trace add variable A write {apply {{n i op} {lappend ::ecrits $n}}}
    expr {A = 5}
    list $A $::ecrits
}
puts "trace écriture : [g2]"
proc g3 {} {( A is point = (1.0, 2.0); A = (3.0, 4.0); [list $A [type get $A] [catch {expr {A = 7}} m] $m] )}
puts "contrat : [g3]"
proc g4 {} {upvar 0 ::G A; expr {A = 9}; set ::G}
puts "lien : [g4]"
proc g5 {} {( A = (1.0, 2.0); [list $A [type get $A]] )}
puts "liste : [g5]"
puts "global : [expr {Z = 4}] $Z"
