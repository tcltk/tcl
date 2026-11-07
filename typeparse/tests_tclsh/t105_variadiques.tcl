# t105 : v16y. Champs variadiques {T}nom... n'importe où dans une signature.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
type def pt {{double}x; {double}y}

puts "--- au milieu : les suivants depuis la fin"
proc m {{string}a; {int}args...; {string}z} { list $a $args $z }
t "un"                      {m A 1 Z}
t "trois"                   {m A 1 2 3 Z}
t "zéro : au moins un"      {m A Z}
t "type vérifié"            {m A 1 x Z}
t "usage"                   {m}
t "info args"               {info args m}

puts "--- avec défaut : zéro ou plus"
proc d {{int}args... ?= {}; {string}z} { list $args $z }
t "zéro"                    {d Z}
t "deux"                    {d 1 2 Z}
proc d2 {{int}args... ?= {7 8}} { set args }
t "défaut liste"            {d2}
t "donné"                   {d2 1}
t "défaut validé"           {proc d3 {{int}args... ?= {a}} {}}

puts "--- défauts avant : de gauche à droite"
proc r {{string}e; {string}c; {string}m ?= M; {string}args... ?= {}} { list $e $c $m $args }
t "e c"                     {r E C}
t "e c m"                   {r E C m1}
t "e c m s s"               {r E C m1 s1 s2}
proc r2 {{string}a ?= A; {int}args...} { list $a $args }
t "le variadique d'abord"   {r2 5}
t "puis le défaut"          {r2 x 5}

puts "--- un produit s'étale"
proc f {{{list}vars; {list}vals}args...; {string}corps} { list [lmap p $args {set p(vars)}] $corps }
t "deux paires"             {f {a b} {1 2} c {3} {B}}
t "impaire"                 {f a {1} c {B}}
t "usage"                   {f}
proc l {{pt}args...} { lmap p $args {set p(y)} }
t "points"                  {l 1 2 3 4}
t "point incomplet"         {l 1 2 3}
proc dc {{{string}k; {string}v}args... ?= {}} { llength $args }
t "aucune paire"            {dc}

puts "--- avec options et liens"
proc o {{flag}-n; {var.dict}d; {string}args...; {string}v} { dict set d {*}$args $v; set n }
t "dict set"                {set D {}; o D a b 1; o -n D a c 2; set D}

puts "--- refus à la définition"
t "deux variadiques"        {proc x1 {{int}a...; {int}b...} {}}
t "défaut après"            {proc x2 {{int}a...; {int}b ?= 1} {}}
t "lien variadique"         {proc x3 {{var.int}a...} {}}

puts "--- args en dernier : inchangé"
proc a {{int}n; {list.int}args} { list $n $args }
t "zéro"                    {a 1}
t "deux"                    {a 1 2 3}
proc ac {n args} { list $n $args }
t "classique"               {ac 1 2 3}
