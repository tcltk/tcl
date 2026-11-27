# t137 (v17z) : la variante d'une somme est une métadonnée.
#  - la valeur est la charge seule ; une valeur nue n'est déduite que si une
#    seule variante l'accepte (un littéral l'emporte) ; sinon une erreur ;
#  - type variant -of T ?NAME?, type literal -of T, type variant $v ?NAME?,
#    type is {sum?}T, {enum?}T ;
#  - type canonic écrit la variante quand la chaîne ne la dit pas ({S/s}5,
#    {/s}5 dans une structure), type parse la relit.
proc t {nom script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR $r"}
    puts [format "%-34s %s" $nom $r]
}
type def S {{int}i, {string}s}
type def A {{int}i, {int}j}
type def E {"on", "off", "auto"}
type def M {{int}n, "auto"}
type def P {{S}k; {A}a}
type def temp {{double}celsius, {double}fahrenheit}
type convert temp/celsius temp/fahrenheit {{c} {expr {$c * 9 / 5.0 + 32}}}

puts "== la valeur est la charge"
t "étiquette"                      {set {S}v {/s}5; list $v [type get $v]}
t "expr sur la charge"             {set {S}v {/i}5; expr {$v * 2}}
t "chemin v(i)"                    {set {S}v {/i}5; list $v(i) [type get $v(i)]}
t "une seule variante"             {set {S}v abc; list $v [type get $v]}
t "deux variantes : erreur"        {set {S}v 5}
t "même forme : erreur"            {set {A}v 5}
t "littéral : lui-même"            {set {M}m auto; list $m [type get $m]}
t "littéral contre variante"       {type def L2 {{string}s, "auto"}; set {L2}x auto; type get $x}
t "rien de collant"                {set {S}v {/s}abc; catch {set v 8} msg; set msg}
t "cast vers la variante"          {set {S}v {/i}5; type get [type cast int $v]}
t "cast vers string"               {set {S}v {/i}5; type cast string $v}
t "égalité en valeur"              {set {S}a {/i}5; set {S}b {/s}5; expr {$a eq $b}}
t "conversion par cast"            {set {temp}c {/celsius}100.0; set f [type cast temp/fahrenheit $c]; list $f [type get $f]}

puts "== variantes et littéraux"
t "variant -of S"                  {type variant -of S}
t "variant -of E (aucune)"         {type variant -of E}
t "variant -of M"                  {type variant -of M}
t "variant -of M n"                {type variant -of M n}
t "variant -of M auto"             {type variant -of M auto}
t "literal -of E"                  {type literal -of E}
t "literal -of M"                  {type literal -of M}
t "literal -of S (aucun)"          {type literal -of S}
t "variant -of int"                {type variant -of int}
t "variant -of de type get"        {set {S}v {/i}5; type variant -of [type get $v]}
t "is {sum?}"                      {list [type is {sum?}S] [type is {sum?}E] [type is {sum?}int]}
t "is {enum?}"                     {list [type is {enum?}E] [type is {enum?}M] [type is {enum?}S]}
t "is {sum?} d'une valeur"         {set {S}v {/i}5; type is {sum?}[type get $v]}
t "variant \$v"                    {set {S}v {/s}5; type variant $v}
t "variant d'un littéral : vide"   {set {M}m auto; list <[type variant $m]>}
t "réétiqueter"                    {set {A}v {/i}5; set w [type variant $v j]; list $w [type get $w] [type get $v]}
t "réétiqueter : charge refusée"   {set {S}v {/s}abc; type variant $v i}
t "réétiqueter vers un littéral"   {set {M}m {/n}5; type variant $m auto}
t "réétiqueter n'est pas convertir" {set {temp}c {/celsius}100.0; set f [type variant $c fahrenheit]; list $f [type get $f]}

puts "== la forme chaîne"
proc ar {v} {
    set c [type canonic $v]
    set y [type parse $c]
    list $c [expr {[type canonic $y] eq $c && [type get $y] eq [type get $v] && $y eq $v ? "≡" : "DIFF"}]
}
t "variante écrite : i"            {ar [type {S/i}5]}
t "déduite : une seule variante"   {ar [type {S/s}abc]}
t "variante écrite : s"            {ar [type {S/s}5]}
t "même forme"                     {ar [type {A/j}5]}
t "dans une liste"                 {set {list.S}L {/s}5 {/i}6 x; ar $L}
t "dans un produit"                {set {S}k {/s}5; set {A}a {/j}7; set {P}p $k $a; ar $p}
t "relu : les variantes"           {set {list.A}L {/j}5 {/i}6; lmap e [type parse [type canonic $L]] {type get $e}}
t "par la chaîne nue : erreur"     {set {list.A}L {/j}5; set {list.A}M {*}[string range " $L" 1 end]}
t "variante inconnue"              {type parse {{S/zz}5}}
t "étiquette inconnue : un texte"  {type parse {{list.S}{{{/zz}5}}}}
t "charge refusée"                 {type parse {{list.S}{{{/i}abc}}}}
