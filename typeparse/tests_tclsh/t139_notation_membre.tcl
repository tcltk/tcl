# t139 -- v18a : la notation des membres. Une étiquette s'écrit {/nom}
# (champ d'un produit, variante d'une somme) ; un membre d'un type s'écrit
# X/m (P/f : le type du champ f ; S/v : la variante v de S). Les anciennes
# écritures {nom:} et S.v sont refusées avec un message qui donne la
# nouvelle.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} {set r "ERREUR: $r"}
    puts [format "%-36s %s" $label $r]
}
type def point {{double}x; {double}y}
type def temp {{double}celsius, {double}fahrenheit}
type def V {{int}int, {string}string}

puts "== étiquettes"
t "champ {/x}"                       {set {point}p {/y}2 {/x}1; set p}
t "variante {/celsius}"              {set {temp}t {/celsius}50; list $t [type get $t]}
t "variante inconnue"                {set {temp}t {/kelvin}5}
t "ancienne étiquette refusée"       {set {temp}t {celsius:}50}
t "ancienne, champ"                  {set {point}p {x:}1 {y:}2}
t "ancienne, en retour"              {proc {temp}r1 {} {return {celsius:}1}; r1}
t "retour étiqueté"                  {proc {temp}r2 {} {return {/fahrenheit}1}; type get [r2]}
t "argument nommé"                   {proc fa {{int}a; {int}b} {expr {$a - $b}}; fa {/b}1 {/a}10}
t "élément de liste"                 {set {list.temp}L {/celsius}1 {/fahrenheit}2; type canonic $L}
t "relu"                             {set c [type canonic $L]; set M [type parse $c]; list [type variant [lindex $M 0]] [type variant [lindex $M 1]]}

puts "== membres X/m"
t "type def point/x"                 {type def point/x}
t "type def temp/celsius"            {type def temp/celsius}
t "{temp/celsius}u"                  {set {temp/celsius}u 4; type get $u}
t "{point/x}d : le type du champ"    {set {point/x}d 4; list $d [type get $d]}
t "type cast temp/fahrenheit"        {type get [type cast temp/fahrenheit 4]}
t "type include temp temp/celsius"   {type include temp temp/celsius}
t "type variant -of temp/celsius"    {type variant -of temp/celsius}
t "champ inconnu"                    {type def point/z}
t "pas un produit"                   {type def int/z}
t "ancien S.v refusé"                {type cast temp.celsius 4}
t "ancien P.f refusé"                {type def point.x}
t "étiquette qualifiée {point/x}"    {set {point}q {point/x}1 {point/y}2; set q}

puts "== expr is"
set {temp}tc {/celsius}4
t "tc is temp"                       {expr {tc is temp}}
t "tc is temp/celsius"               {expr {tc is temp/celsius}}
t "tc is temp/fahrenheit"            {expr {tc is temp/fahrenheit}}
t "is temp/kelvin"                   {expr {tc is temp/kelvin}}
set {list(2).int}w 1 2
t "w is list(2).int"                 {expr {w is list(2).int}}
t "dans une procédure"               {proc pr {} {set {temp}x {/fahrenheit}1; expr {x is temp/fahrenheit && !(x is temp/celsius)}}; pr}

puts "== nom de variante = nom de type"
t "{/int}5"                          {set {V}v {/int}5; list $v [type get $v]}
t "{/string}5"                       {set {V}v {/string}5; list $v [type get $v] [type canonic $v]}
t "num/int"                          {set {num}n 5; type get $n}
