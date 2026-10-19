# type deref sur les sommes, les arbres (types recursifs) et les graphes.
set A 1; set B 2; set C 3

# --- sommes : la variante est conservee, meme si l'ordre des variantes change
type def mix {{int}a, {var.int}b, {string}s}
set {mix}s1 {b}A
set d1 [type deref $s1]
puts "somme : $s1 -> $d1  ([type get $d1])  b=$d1(b)"
set {mix}x1 {a}5
puts "variante a : [type deref $x1]  ([type get [type deref $x1]])"
set {mix}x2 {s}hello
puts "variante s : [type deref $x2]  ([type get [type deref $x2]])"
set {list.mix}L {b}A {a}7 {b}B
set dl [type deref $L]
puts "liste de sommes : $dl  [lmap e $dl {type get $e}]"

# --- somme recursive : feuilles = liens, noeuds = listes
type def bt {{var.int}leaf, {list.bt}node}
set {bt}l1 {leaf}A
set {bt}l2 {leaf}B
set {list.bt}kids $l1 $l2
set {bt}x {node}$kids
set dx [type deref $x]
puts "bt : $x -> $dx  ([type get $dx])  premiere feuille : [type get $dx(node.0)] = $dx(node.0.leaf)"

# --- arbre : type recursif parametre, dont la valeur est un lien
type def {tree({type}T)} {{$T}value; {list.tree($T)}children}
set {tree(var)}T A {{B {}} {C {}}}
set dT [type deref $T]
puts "arbre : $dT  ([type get $dT])"
puts "  racine=$dT(value) enfants=$dT(children.0.value),$dT(children.1.value)"
set A 100
puts "  apres A=100 : instantane=$dT(value) lien=[expr {$T(value)}]"
set dT2 [type deref $T]
puts "  nouvel instantane=$dT2(value) ; meme type : [expr {[type get $dT] eq [type get $dT2]}]"
set A 1
unset C
puts [catch {type deref $T} msg]; puts "  $msg"
set C 3

# --- graphe oriente avec cycle : les voisins sont des liens vers des noeuds
type def node {{string}name; {list.var.node}next}
set {node}na(name,next) A {nb nc}
set {node}nb(name,next) B {nc}
set {node}nc(name,next) C {na}
set g [type deref $na]
puts "graphe : $g  ([type get $g])"
puts "  voisins de A : $g(next.0.name) $g(next.1.name) ; voisins de B vus depuis A : $g(next.0.next)"
set g2 [type deref $g(next.0)]
puts "  un niveau de plus : $g2 ; voisin de B : $g2(next.0.name)"
unset nb
puts [catch {type deref $na} msg]; puts "  $msg"
