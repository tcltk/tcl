# t110 (v17c) : type canonic ?-notes? VALEUR, type parse TEXTE.
#   {T}contenu, {T@n1,n2}contenu, {@notes}contenu ; deux lois :
#   type parse [type canonic $v] ≡ $v ; type canonic [type parse $t] = $t
#   quand $t est canonique.
proc t {nom script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR $r"}
    puts [format "%-30s %s" $nom $r]
}
# la forme canonique, et l'aller-retour vérifié
proc ar {v} {
    set c [type canonic $v]
    set y [type parse $c]
    list $c [expr {[type canonic $y] eq $c && [type get $y] eq [type get $v]
	&& [note get $y] eq [note get $v] && $y eq $v ? "≡" : "DIFF"}]
}

type def num110 {{int}i | {double}d}
type def S110 {{int}i, {double}d}
type def pt110 {{num110}x; {num110}y}
type def opt110 {"none", {int}some}
type def Jour110 string
namespace eval ::g110 {type def P {{double}x; {double}y}}

puts "== scalaires"
t "int 0x10"                  {ar [type cast int 0x10]}
t "double 2e1"                {ar [type cast double 2e1]}
t "grand entier"              {ar [type cast int 123456789012345678901234567890]}
t "string avec espaces"       {ar [type cast string "a b"]}
t "string vide"               {ar [type cast string ""]}
t "nominal"                   {ar [type cast Jour110 lundi]}
t "alias : le type visé"      {type alias coord110 double; type canonic [type cast coord110 3]}

puts "== conteneurs : le texte reconstruit"
t "list.int espacée"          {type canonic [type cast list.int "1   2"]}
t "list de listes"            {ar [type cast list.list.int {{1 2} 3}]}
t "dict.int"                  {ar [type cast dict.int {a 1 b 0x2}]}
t "produit nommé"             {ar [type cast pt110 {1 2.5}]}
t "produit anonyme"           {ar [type cast {{int}a; {double}b} {1 2}]}
t "type d'un espace"          {ar [type cast ::g110::P {1 2}]}
t "list d'un type d'espace"   {ar [type cast list.::g110::P {{1 2}}]}
t "cases (-compact)"          {type canonic [type cast {-compact}list.double {1 2}]}

puts "== sommes et unions"
t "somme : la variante"       {ar [type cast S110/d 2]}
t "littéral"                  {ar [type cast opt110 none]}
t "union : lue du contenu"    {ar [type cast num110 0x1F]}
t "list.num110"               {ar [type cast list.num110 {1 2.0 0x10}]}

puts "== notes"
t "une note"                  {ar {vitesse}[type cast double 5]}
t "un ensemble trié"          {type canonic {zeta}[set x {alpha}[type cast double 5]]}
t "seulement notée"           {ar {vitesse}5}
t "-notes"                    {type canonic -notes {vitesse}[type cast double 5]}
t "-notes sans note"          {type canonic -notes [type cast pt110 {1 2}]}
t "note encodée"              {set c [type canonic [type parse {{@a%2Cb}x}]]; list $c [note get [type parse $c]]}
t "élément noté normalisé"    {type canonic [type cast list.double [list {vitesse}5 6]]}

puts "== refus"
t "valeur nue"                {type canonic 5}
t "pas d'accolade"            {type parse 5}
t "sans contenu"              {type parse {{int}}}
t "deux mots"                 {type parse {{int}x y}}
t "type inconnu"              {type parse {{intt}5}}
t "contenu refusé"            {type parse {{int}abc}}
t "note vide"                 {type parse {{int@a,,b}5}}
t "encodage incomplet"        {type parse {{int@%4}5}}
t "arguments"                 {type canonic}
