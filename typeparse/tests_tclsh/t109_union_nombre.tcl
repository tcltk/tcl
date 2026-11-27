# t109 (v17b) : l'union {int}i | {double}d, disjointe sur les formes
#   canoniques ; lecture, champs, boucles, expr.
proc t {nom script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR $r"}
    puts [format "%-34s %s" $nom $r]
}

puts "== définition"
t "num intégré"                 {type get [type cast num 1]}
t "disjoint? num"               {type is {disjoint?}num}
t "int | string refusé"         {type def ns109 {{int}i | {string}s}}
t "double | string refusé"      {type def ds109 {{double}d | {string}s}}

puts "== lecture : la variante du texte canonique"
foreach v {10 -3 0x10 2.0 2e1 1e300 .5 inf} {
    t "num $v"                  [list apply {v {set {num}n $v; list $n [type get $n]}} $v]
}
t "num abc refusé"              {set {num}n abc}
t "num {} refusé"               {set {num}n {}}
t "union : pas d'étiquette"     {set {num}n {d 1.5}}

puts "== expr : sémantique de Tcl"
t "int / 2"                     {set {num}a 5; expr {$a / 2}}
t "double / 2"                  {set {num}a 5.0; expr {$a / 2}}
t "0x10 normalisé"              {set {num}a 0x10; expr {$a + 0}}

puts "== champs d'un produit"
type def pt109 {{num}x; {num}y}
proc moitie {{pt109}P} {( ($P(x) + $P(y)) / 2 )}
t "champs int"                  {moitie {3 4}}
t "champs mixtes"               {moitie {3 4.0}}
t "champ lu nu"                 {set {pt109}p 1 2.5; list $p(x) $p(y)}
t "type du champ"               {set {pt109}p 1 2.5; list [type get $p(x)] [type get $p(y)]}

puts "== listes et boucles"
t "list.num normalisée"         {set {list.num}L {*}{1 2.0 0x10 3e0}; set L}
t "lindex typé"                 {set {list.num}L {*}{1 2.0}; list [type get [lindex $L 0]] [type get [lindex $L 1]]}
t "boucle : éléments nus"       {set {list.num}L {*}{1 2.0}; lmap e $L {type get $e}}
t "éléments nus en boucle"      {set {list.num}L {*}{1 2.0}; lmap e $L {expr {$e / 2}}}
t "somme"                       {set {list.num}L {*}{1 2.5 3}; set s 0; foreach e $L {set s [expr {$s + $e}]}; set s}
t "lappend"                     {set {list.num}L 1; lappend L 2.0; lappend L 0x10; set L}
t "lappend refusé"              {set {list.num}L 1; lappend L x}
