# t106 : v16z. apply typé : une lambda est une procédure sans nom.
# apply {T}{{args typés} corps ?ns?} : la liste d'arguments est une signature,
# l'annotation du mot lambda est le type de retour.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
type def pt {{double}x; {double}y}

puts "--- l'exemple"
t "somme" {set somme [apply {double}{
   {
      {double}a;
      {double}b
   } {(
      $a + $b
   )}
} 5 10]}

puts "--- arguments typés"
t "contrôle"                {apply {{{int}a; {int}b} {expr {$a * $b}}} 6 x}
t "usage"                   {apply {{{int}a; {int}b} {expr {$a * $b}}} 6}
t "défaut"                  {apply {{{int}n; {int}m ?= 10} {list $n $m}} 1}
t "option"                  {apply {{{int}n; {int}-pas ?= 2} {expr {$n * $pas}}} -pas 5 3}
t "option inconnue"         {apply {{{int}n; {int}-pas ?= 2} {expr {$n * $pas}}} -pos 5 3}
t "drapeau"                 {apply {{{flag}-v; {string}s} {list $v $s}} -v x}
t "variadique"              {apply {{{string}s; {int}args...} {list $s $args}} a 1 2 3}
t "variadique contrôlé"     {apply {{{string}s; {int}args...} {list $s $args}} a 1 x}
t "produit"                 {apply {{{pt}p} {expr {$p(x) + $p(y)}}} {1 2}}
set n 0
t "lien"                    {apply {{{var.int}v} {incr v 5}} n; set n}
t "classique inchangé"      {apply {{a {b 2}} {list $a $b}} 1}

puts "--- type de retour"
t "rendu"                   {apply {double}{{x} {return $x}} 2.5}
t "refusé"                  {apply {int}{{x} {return $x}} 2.5}
t "même lambda, int"        {apply {int}{{x} {return $x}} 2}
t "produit rendu"           {apply {pt}{{{double}x} {list $x $x}} 3}
t "produit refusé"          {apply {pt}{{{double}x} {list $x a}} 3}
t "déplié"                  {apply {{*}list.int}{{{int}args...} {return $args}} 1 2 3}
t "type inconnu"            {apply {nimp}{{x} {set x}} 1}
t "une note n'est pas un type" {apply {@trace}{{x} {expr {$x + 1}}} 1}

puts "--- espace de noms"
namespace eval geo { type def vec {{double}dx; {double}dy} }
t "args dans geo"           {apply {{{vec}v} {set v(dx)} geo} {1 2}}
t "retour dans geo"         {apply {vec}{{{double}d} {list $d $d} ::geo} 4}
t "hors de geo"             {apply {{{vec}v} {set v(dx)}} {1 2}}

puts "--- dans une boucle, récursion"
proc carres {L} { lmap x $L { apply {int}{{{int}x} {expr {$x * $x}}} $x } }
t "lmap"                    {carres {1 2 3 4}}
t "lmap refusé"             {carres {1 2.5}}
proc fact {n} { apply {int}{{{int}n} {expr {$n < 2 ? 1 : $n * [fact [expr {$n - 1}]]}}} $n }
t "récursion"               {fact 10}
t "beaucoup de lambdas"     {set ok 1; for {set i 0} {$i < 600} {incr i} { if {[apply {int}[list {} "return $i"]] != $i} { set ok 0 } }; set ok}

puts "--- type convert : une règle est une lambda"
type def celsius double
type def kelvin double
t "règle typée"             {type convert celsius kelvin {{{celsius}c} {expr {$c + 273.15}}}; set {celsius}c 20; set k {kelvin}$c}
type def polaire {{double}r; {double}a}
t "produit vers produit"    {type convert pt polaire {{{pt}p} {list [expr {hypot($p(x), $p(y))}] [expr {atan2($p(y), $p(x))}]}}; set {pt}p 3 4; set q {polaire}$p; set q(r)}

puts "--- erreurs"
t "signature invalide"      {apply {{{int}a {int}b} {list}} 1 2}
catch {apply {double}{{{double}a} {error boum}} 1}
puts [string map [list [info script] FICHIER] $::errorInfo]
