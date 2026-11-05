# t96 : v16p. Procédures typées.
#  - la liste d'arguments est un produit : {{pt}p; {double}dx ?= 0; ...} ;
#    chaque champ est une variable locale avec son contrat ;
#  - à l'appel : positionnels, puis étiquettes {dx:}3 ; {list.T}args en
#    dernier reçoit le reste ; {var.T}n lie n à la variable de l'appelant ;
#  - proc {T}nom : le résultat d'un retour normal est casté en T ; {v:}
#    choisit la variante d'une somme ; return {T}$x n'est qu'une note locale.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
type def pt {{double}x; {double}y}
proc {pt}decale {{pt}p; {double}dx ?= 0; {double}dy ?= 0} {
    set {pt}q [expr {$p(x) + $dx}] [expr {$p(y) + $dy}]
    return $q
}
set {pt}a 1 2

puts "--- arguments : positionnels, défauts, étiquettes"
t "positionnel"                  {set r [decale $a 1 1]; list $r [type get $r]}
t "défauts"                      {decale $a}
t "étiquette"                    {decale $a {dy:}5}
t "mélange"                      {decale $a 2 {dy:}5}
t "étiquettes seules"            {decale {dy:}5 {p:}$a}
t "deux fois"                    {decale $a 2 {dx:}5}
t "positionnel après étiquette"  {decale {dx:}2 $a}
t "trop"                         {decale $a 1 2 3}
t "manque"                       {decale}
t "manque, avec étiquette"       {decale {dx:}1}
t "étiquette inconnue"           {decale $a {zz:}1}
t "mauvais type"                 {decale $a abc}
t "contrat dans le corps"        {proc pc {{int}a} {set a x}; pc 1}
t "défaut calculé"               {proc dc {{int}a ?= [expr {6 * 7}]} {set a}; dc}
t "défaut calculé, champ avant"  {proc dc2 {{int}a; {int}b ?= [expr {$a * 2}]} {list $a $b}; dc2 4}
type def {-strict}S {{int}i, {string}s}
t "variante d'un argument"       {proc ps {{S}v} {type get $v}; ps {s:}abc}
t "argument somme strict, nu"    {ps abc}
t "appel compilé"                {proc w {} { list [decale {dx:}1 {p:}[type {pt}{3 4}]] }; w}

puts "--- le reste : {list.T}args"
proc somme {{int}a; {list.int}args} { expr {$a + [tcl::mathop::+ {*}$args]} }
t "args"                         {somme 1 2 3 4}
t "args vide"                    {somme 1}
t "args mauvais"                 {somme 1 2 x}
t "args : contrat"               {proc s2 {{int}a; {list.int}args} {lappend args z}; s2 1 2}
t "usage"                        {somme}

puts "--- {var.T} : upvar automatique"
proc incrv {{var.int}n; {int}d ?= 1} { incr n $d }
set k 5
t "upvar"                        {incrv k; incrv k 10; set k}
t "étiquette"                    {incrv {d:}2 {n:}k; set k}
set ks abc
t "valeur courante vérifiée"     {incrv ks}
t "variable neuve"               {incrv knew; set knew}

puts "--- type de retour"
proc {int}f {a} {return $a}
t "liste classique"              {type get [f 5]}
t "mauvais retour"               {f abc}
proc {int}h3 {} { expr {3} }
t "fin sans return"              {type get [h3]}
proc {int}h {} { return {double}2.5 }
t "return {T}\$x : note locale"  {h}
proc {int}h2 {} { return -code error oops }
t "erreur : inchangée"           {h2}
proc {int}h4 {} { return -level 2 abc }
proc h5 {} { h4; return fin }
t "return -level 2"              {h5}
proc {int}rr {} { return -code ok abc }
t "options d'erreur"             {list [catch {rr} m o] [dict get $o -errorcode] [dict get $o -code]}
proc {int}tc {} { tailcall expr {6 * 7} }
t "tailcall"                     {tc}
proc {int}fact {{int}n} { if {$n <= 1} {return 1}; expr {$n * [fact [expr {$n - 1}]]} }
t "récursion"                    {list [fact 10] [type get [fact 3]]}
t "eval dans le corps"           {proc {int}ev {} { eval {return x} }; ev}
t "coroutine"                    {proc {int}co {} { yield 1; return 2 }; list [coroutine C co] [C]}

puts "--- étiquettes : la variante d'une somme rendue"
type def {-strict}R {{pt}point, {int}n}
proc {R}g {{string}w} {
    if {$w eq "p"} { set {pt}z 1 2; return {point:}$z }
    if {$w eq "n"} { return {n:}4 }
    if {$w eq "z"} { return {zz:}4 }
    return 7
}
t "{point:}"                     {set r [g p]; list $r [type get $r]}
t "{n:}"                         {set r [g n]; list $r [type get $r]}
t "somme stricte, valeur nue"    {g x}
t "variante inconnue"            {g z}
proc {int}lab {} { return {zz:}5 }
t "étiquette, pas une somme"     {lab}
proc {S.s}vs {} { return 5 }
t "{S.s}nom : toujours s"        {type get [vs]}

puts "--- introspection, compatibilité"
t "type proc"                    {type proc decale}
t "type proc, classique"         {proc c {a {b 2} args} {list $a $b $args}; type proc c}
t "info args"                    {info args decale}
t "info default"                 {list [info default decale dx v] $v [info default decale p v] $v [info default dc a v] $v}
t "info default : pas un champ"  {info default decale zz v}
t "classique intact"             {list [c 1] [info args c]}
t "classique avec ;"             {proc c2 {a;} {set {a;}}; c2 9}
t "type inconnu"                 {proc e1 {{zz}a} {}}
t "retour inconnu"               {proc {zz}e2 {} {}}
t "pas un produit"               {proc e3 {{int}a, {string}b} {}}
t "rename"                       {rename decale dec2; dec2 $a 1}
t "namespace"                    {namespace eval ns { proc {pt}mk {{double}a; {double}b} { list $a $b } }; type get [ns::mk 1 2]}

puts "--- types écrits dans la signature (produits anonymes)"
proc {pt}decale2 {
    {{double}x; {double}y}p;
    {{double}x ?= 0; {double}y ?= 0}dp
} {(
    $p(x) + $dp(x), $p(y) + $dp(y)
)}
set Point {100 200}
set Vecteur {50 30}
t "listes nues"                  {set r [decale2 $Point $Vecteur]; list $r [type get $r]}
t "dp absent : ses défauts"      {decale2 $Point}
t "dp incomplet"                 {decale2 $Point {50}}
t "étiquette de champ"           {decale2 $Point {dp:}{50 1}}
t "mauvais champ"                {decale2 {a b} {1 2}}
t "trop"                         {decale2 $Point {1 2} 3}
t "type nommé, tous défauts"     {type def V {{double}x ?= 0; {double}y ?= 0}; proc hv {{V}v} {type get $v}; list [hv] [hv {1 2}]}
