# t112 (v17e) : les méta-types bare, typed, noted, typednoted ; any, leur
#   union ; type meta. Une exigence vérifie l'état d'une valeur, et la
#   garde telle quelle (type et notes) : elle ne convertit rien.
proc tn {v} {list [type get $v] [note get $v]}
proc m {v} {set r [type meta $v]; list $r [note get $r]}
proc t {nom script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR $r"}
    puts [format "%-30s %s" $nom $r]
}
set i 1; incr i
set {int}ty 3
set no {mm}3
set {int}tno {mm}3
set s abc

puts "== type meta"
foreach v {i ty no tno s} {t "type meta \$$v" [list m [set $v]]}
t "représentation : list"        {m [list a b]}
t "représentation : dict"        {m [dict create a 1]}
t "le résultat est une somme"    {type meta $ty}

puts "== exigences d'un argument"
proc fb  {{bare}x}       {tn $x}
proc ft  {{typed}x}      {tn $x}
proc fn  {{noted}x}      {tn $x}
proc ftn {{typednoted}x} {tn $x}
proc fany {{any}x}       {tn $x}
type def annotee {noted | typednoted}
proc fa  {{annotee}x}    {tn $x}
foreach f {fb ft fn ftn fany fa} {
    foreach v {i ty no tno} {t "$f \$$v" [list $f [set $v]]}
}
t "le cast de l'appel d'abord"   {fb {int}$i}

puts "== any : l'identité"
t "type cast any (typée, notée)" {tn [type cast any $tno]}
t "type cast any (nue)"          {tn [type cast any 5]}
t "{any}\$x"                     {tn {any}$tno}
t "set {any}v"                   {set {any}v $tno; tn $v}
t "list.any"                     {set {list.any}P 5 {this}6 $tno; lmap x $P {tn $x}}
t "dict.any"                     {set {dict.any}D a 5 b $tno; dict map {k x} $D {tn $x}}

puts "== set et conteneurs"
t "set {typed}x"                 {set {typed}x $ty; tn $x}
t "set {typed}x 5"               {set {typed}x 5}
t "pas de contrat"               {set {typed}x $ty; set x 7; tn $x}
t "list.typed"                   {set {list.typed}L {int}5 [type cast double 2.5]; lmap e $L {tn $e}}
t "list.typed refuse une nue"    {set {list.typed}L 5}
t "list.bare"                    {set {list.bare}L a b; lmap e $L {tn $e}}
t "list.bare refuse une typée"   {set {list.bare}L a $ty}
t "dict.typed"                   {set {dict.typed}D a $ty b [type cast double 1]; dict map {k e} $D {tn $e}}
t "type cast typed"              {tn [type cast typed $ty]}
t "type cast typed 5"            {type cast typed 5}

puts "== unions"
t "noted | typednoted"           {type def annotee2 {noted | typednoted}; set _ ok}
t "bare | any : refusée"         {type def u1 {{bare}a | {any}b}}
t "int | bare : refusée"         {type def u2 {{int}i | {bare}b}}
t "noms réservés"                {type def typed int}

puts "== forme canonique d'une valeur nue"
foreach v [list 5 "a b" {}] {
    t "canonic [list $v]" [list apply {v {set c [type canonic $v]; list $c [type meta [type parse $c]] [expr {[type parse $c] eq $v}]}} $v]
}
t "type parse {typed}5"          {type parse {{typed}5}}
