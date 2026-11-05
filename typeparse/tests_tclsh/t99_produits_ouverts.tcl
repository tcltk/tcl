# t99 : v16t, scalaires atomiques et produits ouverts.
#   Une chaîne typée (et tout scalaire) est UN élément de liste : elle n'est
#   jamais relue comme une liste, à aucun niveau d'imbrication. Un produit
#   est un conteneur : sa vue liste est celle de ses champs, chacun dans son
#   type (un champ string reste atomique).
type def point {{double}x; {double}y}
set {point}P 1 2
set Pn {1 2}

puts "--- produit : la vue liste de ses champs"
puts "llength      : [llength $P] [llength $Pn]"
puts "lindex       : [lindex $P 0] [type get [lindex $P 0]]"
lassign $P x y
puts "lassign      : $x $y"
foreach {a b} $P {puts "foreach x y  : $a $b"}
puts "{*}          : [llength [list {*}$P]]"
puts "concat       : [concat $P $P] / [concat $Pn $Pn]"
puts "join         : [join $P ,]"
puts "lmap         : [lmap v $P {expr {$v * 2}}]"
puts "même structure après la chaîne : [llength [list 0 $P]] [llength [lindex [list 0 $P] 1]] [llength [lindex [string cat [list 0 $P]] 1]]"

puts "--- écritures par la vue liste (sous contrat)"
lset P 0 5
puts "lset         : $P [type get $P]"
puts "lappend      : [catch {lappend P 3} m] $m"
puts "P            : $P"

puts "--- chaînes atomiques"
set {list.string}L {hello world} coucou
foreach s $L {puts "list.string  : <$s> [llength $s] [type get $s]"}
set {list.list.string}M {{hello world} a} {b c}
foreach r $M {foreach s $r {puts "imbriquée    : <$s> [llength $s]"}}
set {string}st "a b c"
puts "string       : [llength $st]"

puts "--- champ chaîne d'un produit"
set {{string}nom; {int}age}q "hello world" 3
puts "llength      : [llength $q] [llength [lindex $q 0]] [type get [lindex $q 0]]"
foreach f $q {puts "champ        : <$f> [llength $f]"}

puts "--- sommes, listes de produits"
type def S {{point}at, "none"}
set {S}s1 {at:}$P
set {S}s2 none
puts "somme        : [llength $s1] [llength $s2] [type get [lindex $s1 0]]"
set {list.point}LP {1 2} {3 4}
puts "list.point   : [llength $LP] [type get [lindex $LP 0]] [llength [lindex $LP 0]]"
puts "concat {*}   : [concat {*}$LP]"
set {{list.int}l; {string}n}r {1 2 3} "a b"
puts "produit mixte: [llength $r] [llength [lindex $r 0]] [llength [lindex $r 1]]"

puts "--- retour typé vers un appelant non typé"
proc {point}p {{double}x; {double}y} {( $x, $y )}
lassign [p 3 4] u v
puts "lassign      : $u $v"
