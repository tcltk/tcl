# apply typé (v16z) : une lambda est une procédure sans nom. Sa liste
# d'arguments accepte la signature de proc ; l'annotation du mot lambda est
# son type de retour, comme le {T} de proc {T}nom.

proc montre {script} {
    if {[catch {uplevel 1 $script} r]} { set r "ERREUR $r" }
    puts [format "%-46s -> %s" [string trim $script] $r]
}

puts "--- la somme"
set somme [apply {double}{
   {
      {double}a;
      {double}b
   } {(
      $a + $b
   )}
} 5 10]
puts "somme = $somme"
montre {apply {double}{{{double}a; {double}b} {( $a + $b )}} 5 x}
montre {apply {double}{{{double}a; {double}b} {( $a + $b )}} 5}

puts "\n--- la lambda dans une variable, le type de retour à l'appel"
set f {{{double}a; {double}b} {( $a + $b )}}
montre {apply {double}$f 20 5}
montre {apply {int}$f 20.5 5}

puts "\n--- le type de retour est vérifié"
montre {apply {int}{{x} {expr {$x / 2}}} 9}
montre {apply {int}{{x} {expr {$x / 2.0}}} 9}

puts "\n--- lmap, lsort -command : la lambda en place"
set L {3 1 4 1 5}
montre {lmap x $L { apply {int}{{{int}x} {expr {$x * $x}}} $x }}
type def pt {{double}x; {double}y}
set P {{3 4} {1 1} {0 2}}
montre {lsort -command {apply {{{pt}p; {pt}q} {
    expr {hypot($p(x), $p(y)) < hypot($q(x), $q(y)) ? -1 : 1}
}}} $P}
montre {lsort -command {apply {{{pt}p; {pt}q} {expr {$p(x) - $q(x)}}}} {{1 2} {a b}}}

puts "\n--- options et variadiques, comme pour proc"
set echelle {{{double}-k ?= 1.0; {double}args...} {
    lmap v $args { expr {$v * $k} }
}}
montre {apply $echelle 1 2 3}
montre {apply $echelle -k 10 1 2 3}
montre {apply $echelle -k 10}

puts "\n--- type convert : une règle est une lambda"
type def celsius double
type def kelvin double
type convert celsius kelvin {{{celsius}c} {expr {$c + 273.15}}}
set {celsius}t 20
montre {set k {kelvin}$t}

puts "\n--- dans un espace de noms : les types de la lambda"
namespace eval geo { type def vec {{double}dx; {double}dy} }
montre {apply {vec}{{{double}d} {( $d, -$d )} geo} 2}
