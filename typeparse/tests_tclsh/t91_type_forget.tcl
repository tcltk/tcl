# t91 : type forget T ?T ...? : l'interpréteur oublie la définition d'un type.
# Le nom redevient libre ; les valeurs et les variables déjà typées gardent
# l'ancienne définition (un type distinct du nouveau, de même nom).
proc t {label script} { set c [catch {uplevel 1 $script} r]; puts "$label: $c $r" }

puts "-- redéfinir après oubli"
type def binop {"+", "**"}
t "redéfinir sans oublier" {type def binop {"+", "-"}}
set {binop}v +
set w [set {binop}x **]
t "oublier"            {type forget binop}
t "redéfinir"          {type def binop {"+", "-", "*"}}
t "nouveau binop"      {set {binop}y *}
t "v : ancienne valeur" {list $v [type get $v]}
t "v : ancien contrat" {set v *}
t "w : ancienne valeur" {list $w [type get $w]}

puts "-- refus"
t "inconnu"            {type forget nexistepas}
t "intégré"            {type forget int}
type def exprop {"+", "-", {string}name}
type def exprtree {{list(0).string}none, {list(1).string}leaf, {{exprtree}left; {exprop}op; {exprtree}right}node}
t "utilisé par un autre" {type forget exprop}
t "les deux ensemble"  {type forget exprop exprtree}
t "plus défini"        {set {exprtree}e {}}

puts "-- types anonymes : list.T voit le nouveau T"
type def p {{int}x; {int}y}
t "list.p ancien"      {set {list.p}L {1 2}; type get $L}
type forget p
type def p {{double}x; {double}y; {double}z}
t "list.p nouveau"     {set {list.p}M {1 2 3}; type get $M}
t "list.p : 2 champs"  {set {list.p}M2 {1 2}}

puts "-- conversions déclarées"
type def mm {double}; type def cm {double}
type convert cm mm {x {expr {$x*10}}}
set {cm}c 2
t "cast"               {type cast mm $c}
type forget cm
type def cm {double}
set {cm}c2 2
t "règle oubliée"      {type cast mm $c2}

puts "-- familles"
type def {q({string}D, {double}f)} {double}
type def mm2 {q(L, 0.001)}
set {mm2}d 1.5
t "oublier q"          {type forget q}
t "q inconnu"          {set {q(M, 1.0)}k 3}
t "q redéfini"         {type def {q({int}n)} {list($n).int}}
t "nouvelle q"         {set {q(2)}k3 4 5; type get $k3}
t "mm2 garde"          {list $d [type get $d]}
