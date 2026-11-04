# t94 : v16n. Sommes : disjointes, à déduction, strictes.
#  - type is {disjoint?}S : les paires de variantes qui peuvent accepter une même
#    valeur (analyse prudente : string, any, var acceptent tout ; un scalaire est
#    une liste d'un élément ; la liste vide est un dict vide).
#  - type def {-deduce}S : la variante d'une valeur nue est déduite (la plus stricte
#    gagne) ; type def {-strict}S : elle n'est jamais devinée, il faut la nommer.
#  - sans option, une somme qui se recouvre reçoit un avertissement (stderr).
#  - plusieurs options : {-compact,-strict}, dans la syntaxe des notes.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
type def pt {{double}x; {double}y}

puts "--- disjointes ou non"
type def F {{pt}point, {int}n, {list(3).int}tri}
type def E {{int}code, {"ok","ko"}etat}
type def X {{list(0).string}rien, {list(1).string}un, {{int}a; {int}b; {int}c}trois}
t "F"                            {type is {disjoint?}F}
t "E"                            {type is {disjoint?}E}
t "X"                            {type is {disjoint?}X}
t "énumération"                  {type def couleur {"rouge", "vert"}; type is {disjoint?}couleur}
type def {-deduce}S {{int}i, {string}s}
type def {-deduce}Sh {{double}circle, {list.double}rect}
type def {-deduce}M {{int}a, {var.int}b, {string}s}
t "S (int, string)"              {type is {disjoint?}S}
t "Sh (scalaire, liste)"         {type is {disjoint?}Sh}
t "M (var : prudent)"            {type is {disjoint?}M}
t "pas une somme"                {type is {disjoint?}pt}

puts "--- l'avertissement (une somme qui se recouvre, sans option)"
t "sans option"                  {type def W {{int}i, {double}d}; type is {strict?}W}
t "disjointe : rien"             {type def W2 {{int}i, {pt}p}; type is {strict?}W2}

puts "--- {-deduce} : la plus stricte gagne"
t "5"                            {set {S}v 5; type get $v}
t "abc"                          {set {S}v abc; type get $v}
t "strict?"                      {type is {strict?}S}

puts "--- {-strict} : nommer la variante"
type def {-strict}T {{int}i, {string}s}
t "strict?"                      {type is {strict?}T}
t "valeur nue"                   {set {T}v 5}
t "{i:}5"                        {set {T}v {i:}5; type get $v}
t "{T.s}5"                       {set {T}v {T.s}5; type get $v}
t "type {T.i}5"                  {type get [type {T.i}5]}
t "type cast T.i 5"              {type get [type cast T.i 5]}
t "type cast T 5"                {type cast T 5}
t "liste, étiquettes"            {set {list.T}L {i:}1 {s:}a; lmap e $L {type get $e}}
t "liste, nu"                    {set {list.T}L 1}
t "variable : la variante reste" {set {T}w {s:}x; set w 7; type get $w}
t "valeur déjà typée"            {set {T}a {i:}3; set {T}b $a; type get $b}
type def {-strict}U {"vide", {int}n}
t "littéral : nommé par sa valeur" {set {U}u vide; type get $u}
t "variante typée nue"           {set {U}u 4}

puts "--- options"
t "{-compact,-strict}"           {type def {-compact,-strict}C list.double}
t "{-strict,-deduce}"            {type def {-strict,-deduce}Z {{int}i, {string}s}}
t "{-strict} sur une liste"      {type def {-strict}Z list.int}
t "sous-option"                  {type def {-strict.niveau=1}Z {{int}i, {string}s}}
t "inconnue"                     {type def {-bof}Z {{int}i, {string}s}}
t "{-compact,-compact}"          {type def {-compact,-compact}C2 list.double; type is {compact?}C2}
t "alias"                        {type alias {-strict}A S}
t "question inconnue"            {type is {zz?}S}
