# t76 : lecture $a(champ) compilée (cache du champ dans l'objet index) et
#   nombres typés lus directement par expr. Même résultat qu'avant : type
#   du champ, champ de même nom à une autre place dans un autre produit,
#   somme, lien, list.any, opérandes typés jamais modifiés.
type def point {{double}x; {double}y}
type def rgb   {{int(u,8)}r; {int(u,8)}g; {int(u,8)}b}
type def pivot {{double}y; {double}x}
proc lx {a} {list $a(x) [type get $a(x)]}
proc sx {a} {expr {$a(x) * 2}}
set {point}p 1.5 2.5
set {pivot}q 7.0 8.0
puts "point : [lx $p] [sx $p]"
puts "pivot : [lx $q] [sx $q]   (x à une autre place, même littéral)"
puts "point : [lx $p]"
type def etiquette {{string}nom; {double}x}
set {etiquette}p2 a 3.25
puts "autre type : [lx $p2] [sx $p2]"
proc lr {c} {list [expr {$c(r) + $c(g) + $c(b)}] [type get $c(g)] [expr {$c(r) / 2}] [expr {$c(r) & 0xF}]}
set {rgb}c 200 100 50
puts "rgb : [lr $c]"
type def forme {{double}cote, {double}rayon}
type def objet {{string}nom; {forme}f}
proc lf {o} {list $o(f) [type get $o(f)] $o(nom) [type get $o(nom)]}
set {objet}o boule {rayon}2.0
puts "somme : [lf $o]"
type def boite {{list.any}contenu; {double}x}
set {list.any}C {int}1 {string}a
set {boite}bx $C 4.0
proc lb {b} {list [type get $b(contenu)] [type get [lindex $b(contenu) 0]] [expr {$b(x) + 1}]}
puts "list.any : [lb $bx]"
set cible 5.0
type def ref {{var.double}v; {double}x}
proc lv {r} {list $r(v) $r(x)}
set {ref}rv cible 9.5
puts "lien : [lv $rv]"
proc garde {} {
    set {double}d 1.5; set {point}p 2.0 3.0
    set e [expr {$d + 1}]; set f [expr {$p(x) - 0.5}]; set g [expr {$d * $p(x)}]
    list $d [type get $d] $e [type get $e] $f $p $g [expr {$p(x) ** 2}]
}
puts "intacts : [garde]"
proc boucle {L} {set s 0.0; foreach a $L {set s [expr {$s + $a(x) * $a(y)}]}; set s}
set {list.point}L {1 2} {3 4} {5 6}
puts "boucle : [boucle $L] [boucle $L]"
proc mauvais {a} {catch {set a(z)} m; set m}
puts "champ inconnu : [mauvais $p]"
