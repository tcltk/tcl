# t94 : v16w. Sommes et unions.
#  - une somme (",") écrit sa variante dans sa valeur : "variante charge", ou
#    le littéral seul. Rien n'est deviné : une valeur nue est refusée. Toute
#    variante typée a un nom (c'est sa balise).
#  - une union ("|") est la charge seule ; ses alternatives doivent être
#    disjointes (vérifié à la définition), l'alternative se calcule.
#  - type is {disjoint?}S : les paires qui peuvent accepter une même valeur ;
#    type is {union?}S ; type variant $v.
#  - les options {-strict} et {-deduce} n'existent plus.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
proc fresh {x} {string range [string cat _ $x] 1 end}
type def pt {{double}x; {double}y}
type def vec {{double}x; {double}y}

puts "--- disjointes ou non"
type def F {{pt}point, {int}n, {list(3).int}tri}
type def E {{int}code, {"ok","ko"}etat}
type def X {{list(0).string}rien, {list(1).string}un, {{int}a; {int}b; {int}c}trois}
t "F"                            {type is {disjoint?}F}
t "E"                            {type is {disjoint?}E}
t "X"                            {type is {disjoint?}X}
t "énumération"                  {type def couleur {"rouge", "vert"}; type is {disjoint?}couleur}
type def S {{int}i, {string}s}
type def Sh {{double}circle, {list.double}rect}
type def M {{int}a, {var.int}b, {string}s}
t "S (int, string)"              {type is {disjoint?}S}
t "Sh (scalaire, liste)"         {type is {disjoint?}Sh}
t "M (var : prudent)"            {type is {disjoint?}M}
t "pas une somme"                {type is {disjoint?}pt}

puts "--- définitions"
t "somme recouvrante : permise"  {type def W {{int}i, {double}d}; type is {union?}W}
t "variante sans nom"            {type def W1 {{int}, {string}}}
t "union disjointe"              {type def U {{int}n | {list(2).int}paire}; type is {union?}U}
t "union anonyme"                {type def U1 {{int} | {list(2).int}}; type is {union?}U1}
t "union recouvrante : refusée"  {type def U2 {{int}i | {string}s}}
# v17b : int et double sont disjoints sur leurs formes canoniques (16 / 2.0)
t "union int | double"           {type def U3 {{int}i | {double}d}; lmap v {10 2.0 0x10 2e1} {set {U3}u $v; list $u [type get $u]}}
t "union int | string"           {type def U4 {{int}i | {string}s}}
t "point | vecteur : refusée"    {type def U4 {{pt}p | {vec}v}}
t "point , vecteur : somme"      {type def PV {{pt}p, {vec}v}; type is {union?}PV}
t "mélange , et |"               {type def U5 {{int}a, {int}b | {string}c}}
t "énumération en union"         {type def onoff {"on" | "off"}; set {onoff}o on; list $o [type get $o]}
t "variante produit à un champ"  {type def F1 {{{double}r}cercle, {{double}w; {double}h}rect}; type def F1.cercle}
t "{-strict} n'existe plus"      {type def {-strict}Z {{int}i, {string}s}}
t "{-deduce} n'existe plus"      {type def {-deduce}Z {{int}i, {string}s}}

puts "--- somme : la valeur écrit sa variante"
t "nue : refusée"                {set {S}v 5}
t "{i:}5"                        {set {S}v {i:}5; list $v [type get $v]}
t "{S.s}5"                       {set {S}v {S.s}5; list $v [type get $v]}
t "en clair"                     {set {S}v s 5; list $v [type get $v]}
t "par la chaîne"                {set {S}v [fresh {s 5}]; type get $v}
t "variante inconnue"            {set {S}v z 5}
t "charge invalide"              {set {S}v i abc}
t "type {S.i}5"                  {type get [type {S.i}5]}
t "type cast S.i 5"              {type get [type cast S.i 5]}
t "type cast S 5"                {type cast S 5}
t "type cast S {i 5}"            {type get [type cast S {i 5}]}
t "point, vecteur"               {set {PV}m {v:}{1 2}; list $m [type get $m] $m(v.x)}
type def Lit {"vide", {int}n}
t "littéral seul"                {set {Lit}u vide; list $u [type get $u]}
t "littéral : n 4"               {set {Lit}u n 4; list $u [type get $u]}
t "variante typée nue"           {set {Lit}u 4}

puts "--- lire"
t "v(i)"                         {set {S}v i 5; list $v(i) [type get $v(i)]}
t "expr {\$v(i) * 2}"            {set {S}v i 5; expr {$v(i) * 2}}
t "expr {\$v * 2} : refusé"      {set {S}v i 5; expr {$v * 2}}
t "lassign"                      {set {S}v i 5; lassign $v n x; list $n $x}
t "type variant"                 {set {S}v s abc; type variant $v}
t "type variant littéral"        {set {Lit}u vide; type variant $u}
t "type variant, pas une somme"  {set {pt}p 1 2; type variant $p}
t "variante inactive"            {set {S}v i 5; set v(s)}
type def forme {{double}cercle, {{double}w; {double}h}rect}
t "f(rect.w)"                    {set {forme}f rect {2.0 3.0}; set f(rect.w)}
t "f(w) : la charge courante"    {set {forme.rect}f 2.0 3.0; set f(w)}
t "f(zz)"                        {set {forme.rect}f 2.0 3.0; set f(zz)}

puts "--- la variante est dans la valeur"
t "égalité"                      {set {S}a i 5; set {S}b s 5; expr {$a eq $b}}
t "clés de dict"                 {set {S}a i 5; set {S}b s 5; dict size [dict create $a 1 $b 2]}
t "liste par la chaîne"          {set {list.S}L {i:}1 {s:}a; set {list.S}M {*}[fresh $L]; lmap e $M {type get $e}}
t "lappend {s:}x"                {set {list.S}L {i:}1; lappend L {s:}x; lmap e $L {type get $e}}
t "lappend en clair"             {set {list.S}L {i:}1; lappend L {s x}; lmap e $L {type get $e}}
t "lset {s:}x"                   {set {list.S}L {i:}1; lset L 0 {s:}x; set L}
t "produit"                      {type def P {{S}k; {int}n}; set {P}p {s 5} 3; list $p [type get $p(k)]}
t "retour de procédure"          {proc {P}h {} {set {S}k {s:}5; return [list $k 3]}; set r [h]; type get $r(k)}
t "somme imbriquée"              {type def E2 {{S}k, {string}t}; set {E2}e {k:}{s 5}; list $e [type get $e(k)]}

puts "--- modifier"
t "set v 8 : refusé"             {set {S}v i 5; set v 8}
t "set v(i) 8"                   {set {S}v i 5; set v(i) 8; set v}
t "set v(s) x : change"          {set {S}v i 5; set v(s) x; set v}
t "incr v(i)"                    {set {S}v i 5; incr v(i); set v}

puts "--- union : la valeur est la charge"
t "n"                            {set {U}u 5; list $u [type get $u] [type variant $u]}
t "paire"                        {set {U}u 3 4; list $u [type get $u] [type variant $u]}
t "u(paire)"                     {set {U}u 3 4; set u(paire)}
t "expr direct"                  {set {U}u 5; expr {$u * 2}}
t "étiquette qui confirme"       {set {U}u {paire:}{3 4}; type get $u}
t "étiquette fausse"             {set {U}u {n:}{3 4}}
t "rien ne passe"                {set {U}u abc}

puts "--- casts"
t "énumération -> string"        {set {couleur}c rouge; type cast string $c}
t "somme -> string : son texte"  {set {S}v i 5; type cast string $v}
t "somme -> int : refusé"        {set {S}v i 5; type cast int $v}
t "union -> int"                 {set {U}u 5; type get [type cast int $u]}
t "union -> list.int"            {set {U}u 3 4; type get [type cast list.int $u]}
t "int -> S : deux variantes ?"  {set {int}n 5; type cast W $n}
t "int -> F"                     {set {int}n 5; set r [type cast F $n]; list $r [type get $r]}
proc {string}fs {{string}s} {return $s}
proc {int}fi {{int}n} {return $n}
t "proc {string} <- énumération" {set {couleur}c vert; fs $c}
t "proc {int} <- somme"          {set {S}v i 5; fi $v}
t "proc {int} <- v(i)"           {set {S}v i 5; fi $v(i)}

puts "--- options et questions"
t "{-compact}"                   {type def {-compact}C list.double}
t "sur une liste"                {type def Z list.int}
t "inconnue"                     {type def {-bof}Z {{int}i, {string}s}}
t "{-compact,-compact}"          {type def {-compact,-compact}C2 list.double; type is {compact?}C2}
t "alias"                        {type alias A S}
t "question inconnue"            {type is {zz?}S}
