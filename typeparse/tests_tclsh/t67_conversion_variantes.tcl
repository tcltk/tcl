# t67 : règles de conversion entre variantes d'une même somme (type convert S.a S.b) ;
#   lecture d'une variante inactive : la règle, sinon le cast ordinaire, sinon (même
#   forme) une erreur qui propose la règle.
proc t {l s} { set rc [catch {uplevel 1 $s} m]; puts [format "%-30s %s %s" $l $rc $m] }
type def temp {{double}celsius, {double}fahrenheit}
type def S {{int}i, {string}s}
set {temp}t {celsius}100
t "sans règle"            {set t(fahrenheit)}
t "cast sans règle"       {set u {temp.fahrenheit}$t}
type convert temp.celsius temp.fahrenheit {{x} {expr {$x*9/5.0 + 32}}}
type convert temp.fahrenheit temp.celsius {{x} {expr {($x - 32)*5/9.0}}}
t "t(fahrenheit)"         {list $t(fahrenheit) [type get $t(fahrenheit)]}
t "t(celsius)"            {set t(celsius)}
t "cast"                  {set u {temp.fahrenheit}$t; list $u [type get $u]}
t "cast retour"           {set w {temp.celsius}$u; list $w [type get $w]}
t "cast même variante"    {set w {temp.celsius}$t; list $w [type get $w]}
t "type cast"             {set u [type cast temp.fahrenheit $t]; list $u [type get $u]}
t "t inchangé"            {list $t [type get $t]}
t "écrire changer"        {set {temp}x {celsius}10; set x {fahrenheit}50; list $x [type get $x]}
t "déduction : ambigu"    {set {temp}y 5}
t "S i lu s (cast)"       {set {S}v {i}5; list $v(s) [type get $v(s)]}
t "S s lu i (cast)"       {set {S}v {s}12; list $v(i) [type get $v(i)]}
t "S s abc lu i"          {set {S}v {s}abc; set v(i)}
type convert S.s S.i {{x} {string length $x}}
t "S s abc lu i, règle"   {set {S}v {s}abc; set v(i)}
t "règle qui rend faux"   {type convert S.i S.s {{x} {return -code error "non"}}; set {S}v {i}5; set v(s)}
t "erreur : sommes ≠"     {type convert temp.celsius S.i {{x} {}}}
t "erreur : variante ?"   {type convert temp.celsius temp.kelvin {{x} {}}}
t "erreur : même"         {type convert temp.celsius temp.celsius {{x} {}}}
t "erreur : mêlé"         {type convert temp.celsius double {{x} {}}}
t "liste de temp"         {set {list.temp}L $t; set L(0.fahrenheit)}
