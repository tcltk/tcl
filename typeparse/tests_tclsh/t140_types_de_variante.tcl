# t140 -- v18b : la variante, un type. S/v est la somme S restreinte à sa
# variante v (un sous-type de S) ; S/v/w restreint aussi la charge ; un
# littéral est une variante à valeur unique (opt(int)/none). Une variable
# {S/v}x n'accepte que v ; list.S/v = list.(S/v) ; une étiquette garde le
# type de la valeur ({/some}$t) et s'enchaîne ({/some/celsius}5).
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} {set r "ERREUR: $r"}
    puts [format "%-38s %s" $label $r]
}
type def temp {{double}celsius, {double}fahrenheit}
type def mesure {{temp}some, "none"}
type def binop {"+", "**"}
proc {temp/fahrenheit}chaud {} {return 5}

puts "== le type S/v"
t "type cast temp/celsius"               {type get [type cast temp/celsius 4]}
t "variable {temp/celsius}u"             {set {temp/celsius}u 4; set u 5; list $u [type get $u]}
t "u refuse fahrenheit"                  {set u [chaud]}
t "un temp reçoit un temp/fahrenheit"    {set {temp}t [chaud]; type get $t}
t "t reste un temp (ambigu)"             {set t 6}
t "conversion écrite, sans règle"        {type cast temp/celsius [chaud]}
type convert temp/fahrenheit temp/celsius {{f} {expr {($f - 32) * 5 / 9.0}}}
t "conversion écrite, par la règle"      {set c [type cast temp/celsius [type cast temp/fahrenheit 212]]; list $c [type get $c]}
t "argument {temp/celsius}x"             {proc f {{temp/celsius}x} {type get $x}; f 4}
t "argument : autre variante refusée"    {f [chaud]}
t "retour {temp/fahrenheit}"             {type get [chaud]}
t "type def temp/celsius (la charge)"    {type def temp/celsius}
t "dans un produit"                      {type def P {{temp/celsius}c; {int}n}; type def P}

puts "== listes : list.temp/celsius = list.(temp/celsius)"
t "construction"                         {set {list.temp/celsius}L 1 2 3; list [type get $L] [lmap e $L {type get $e}]}
t "lappend 4"                            {lappend L 4; type get [lindex $L end]}
t "lappend fahrenheit refusé"            {lappend L [chaud]}
t "liste de temp : variantes gardées"    {set {list.temp}M {/celsius}1; lappend M [chaud]; lmap e $M {type get $e}}
t "list.temp vers list.temp/celsius"     {set {list.temp/celsius}N $M}

puts "== inclusion, égalité, is"
t "temp ⊇ temp/celsius"                  {type include temp temp/celsius}
t "temp/celsius ⊉ temp"                  {type include temp/celsius temp}
t "temp/celsius ⊉ temp/fahrenheit"       {type include temp/celsius temp/fahrenheit}
t "mesure ⊇ mesure/some/celsius"         {type include mesure mesure/some/celsius}
t "mesure/some ⊇ mesure/some/celsius"    {type include mesure/some mesure/some/celsius}
t "type eq"                              {type eq [chaud] [type cast temp/fahrenheit 1]}
set {temp}tc {/celsius}4
t "tc is temp/celsius"                   {expr {tc is temp/celsius}}
t "tc is temp/fahrenheit"                {expr {tc is temp/fahrenheit}}

puts "== littéraux : des variantes à valeur unique"
t "mesure/none"                          {set {mesure}m none; type get $m}
t "binop/\"+\""                          {set {binop}b +; type get $b}
t "{binop/\"+\"}x"                       {set {binop/"+"}x +; type get $x}
t "littéral refusé"                      {set x **}

puts "== sommes imbriquées : la variante de la charge est gardée"
t "{/some}\$t"                           {set {temp}t {/celsius}3; set {mesure}m {/some}$t; list [type get $m] [type get $m(some)]}
t "{/some}\[chaud\]"                     {set {mesure}m {/some}[chaud]; list [type get $m] [type get $m(some)]}
t "chaîne {/some/fahrenheit}7"           {set {mesure}w {/some/fahrenheit}7; list [type get $w] [type get $w(some)]}
t "chaîne inconnue"                      {set {mesure}w {/some/kelvin}7}
t "{mesure/some/celsius}c"               {set {mesure/some/celsius}c 4; type get $c}
t "restreint : fahrenheit refusé"        {set {mesure/some/celsius}c {/some}[chaud]}
t "liste de mesures"                     {set {list.mesure}L {/some/celsius}1 none {/some}[chaud]; lmap e $L {type get $e}}
t "canonic"                              {type canonic $L}
t "parse"                                {lmap e [type parse [type canonic $L]] {type get $e}}
t "canonic d'une mesure"                 {type canonic $w}
t "retour {/some/celsius}3"              {proc {mesure}r {} {return {/some/celsius}3}; type get [r]}
t "is mesure/some/fahrenheit"            {expr {w is mesure/some/fahrenheit}}
t "is mesure/some/celsius"               {expr {w is mesure/some/celsius}}
t "argument mesure"                      {proc h {{mesure}x} {type get $x(some)}; h $w}
