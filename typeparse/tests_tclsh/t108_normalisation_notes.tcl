# t108 (v17a) : normalisation des nombres à la construction, et notes
#   gardées par les constructeurs de listes et de dicts.
proc t {nom script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR $r"}
    puts [format "%-34s %s" $nom $r]
}
proc tn {v} {list [type get $v] [note get $v]}

puts "== normalisation"
t "int 0x10"                {set {int}n 0x10; set n}
t "int \" 3 \", +3, 0o10"   {lmap x {{ 3 } +3 0o10} {type cast int $x}}
t "double 2e1, 1"           {list [type cast double 2e1] [type cast double 1]}
t "string inchangée"        {set {string}s 0.10; set s}
t "argument typé"           {proc f {{int}x} {return $x}; f 0x10}
t "list.double"             {set {list.double}L {*}{1 2e1}; set L}
t "dict.int"                {set {dict.int}D a 0x10; set D}
t "produit"                 {type def pt108 {{string}nom; {int}age}; set {pt108}p Jean 0x0C; set p}
t "somme"                   {type def S108 {{int}i, {double}d}; set {S108}s {d 1}; set s}
t "nominal"                 {type def v108 double; set {v108}v 2e1; set v}
t "lappend sous contrat"    {set {list.double}M 1; lappend M 2e1; set M}
t "double : division réelle" {set {double}x 5; expr {$x / 2}}
t "égalité de texte"        {set {int}a 0x10; set {int}b 16; expr {$a eq $b}}

puts "== notes gardées"
t "list (commande)"         {set L [list {int}5 {this}6]; lmap x $L {tn $x}}
t "{list}"                  {set {list}P 5 {this}6; lmap x $P {tn $x}}
t "list.double, lindex"     {set {list.double}D {vitesse}12.5 3; list [tn [lindex $D 0]] [tn [lindex $D 1]]}
t "dict.int, chemin"        {set {dict.int}M a {poids}3 b 4; list [tn $M(a)] [tn $M(b)]}
t "{dict}, dict get"        {set {dict}N a {poids}3; tn [dict get $N a]}
t "scalaire typé et noté"   {set a {mm}3; set {int}b $a; tn $b}
t "étiquette : pas une note" {type def S2 {{int}i, {string}s}; set {list.S2}L {i:}1 {s:}a; lmap e $L {type get $e}}
