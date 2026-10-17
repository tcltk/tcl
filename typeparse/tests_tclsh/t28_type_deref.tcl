# type deref : remplace chaque lien var par la valeur actuelle de la variable.
set A 1; set B 2; set C 3
set {list.var.int}refs A B C
puts "liens   : $refs"
set v [type deref $refs]
puts "valeurs : $v  ([type get $v])"
set B 20
puts "le lien suit B ($refs(1)), l'instantane reste fige : $v"
puts "nouvel instantane : [type deref $refs]"

# lien casse : erreur franche, et la valeur d'origine est intacte
unset C
puts [catch {type deref $refs} msg]; puts $msg
puts "refs inchange : $refs"

# dict, listes imbriquees, produit
set {dict.var.int}d x A y B
puts "dict    : [type deref $d]  ([type get [type deref $d]])"
set {list.list.var.int}mm {A B} {B A}
puts "imbrique: [type deref $mm]  ([type get [type deref $mm]])"
type def lien {{var.int}ref; {string}nom}
set {lien}p ref A nom hello
puts "produit : [type deref $p]  ([type get [type deref $p]])"

# valeur sans lien : rendue telle quelle
set {list.int}plain 1 2
puts "sans lien : [type deref $plain]  ([type get [type deref $plain]])"

# var brut : le resultat est de type string
set {list.var}raw A
puts "var brut : [type deref $raw]"

# on ne suit pas les liens que contient la valeur visee
set {list.var.int}inner A
set {list.var.list.var.int}outer inner
puts "un niveau : [type deref $outer]"
puts "deux fois : [type deref [type deref $outer]]"

# variable locale dont l'appel est termine
proc demo {} { set local 42; set {list.var.int}r local; return $r }
set r [demo]
puts [catch {type deref $r} msg]; puts $msg

# erreurs d'usage
puts [catch {type deref abc} msg]; puts $msg
puts [catch {type deref} msg]; puts $msg
puts [catch {type zz} msg]; puts $msg

# arbres, graphes et sommes : voir t29_deref_arbres_sommes.tcl
