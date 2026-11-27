# t92 : v16l. Deux règles.
# 1. (v16l) {T}$x était un cast immédiat. v16m (piste B) : plus de cast
#    immédiat ; l'annotation n'agit que sur la commande qui reçoit le mot, puis
#    expire (elle devient une note). concat ne résout rien : les sorties de la
#    première partie le montrent ; le cast explicite est « type {T}mot » (t93).
# 2. Les étiquettes de champ et de variante s'écrivent {/nom} (annotation
#    locale) ; {nom} est toujours un type ou une note. Le contexte du
#    constructeur ne passe plus avant les types globaux.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
proc show {v} { list $v [type get $v] [note get $v] }

puts "--- plus de cast immédiat (v16m)"
set s 3.0
t "catch {concat {int}\$s} r"    {list [catch {concat {int}$s} r] $r}
t "s = 3"                        {set s 3; show [concat {int}$s]}
set d [set {double}w 3.0]
t "double typé -> int"           {show [concat {int}$d]}
t "{int}\${s}"                   {set s 4; show [concat {int}${s}]}
t "nom inconnu : note"           {set s 3.0; show [concat {zz}$s]}
t "{}\$d retire le type"          {show [concat {}$d]}
t "commande : pas de cast ici"   {set s abc; list [string length [concat {int}[set s]]] [note get [concat {int}[set s]]]}
proc lit {} { set s abc; return [concat {int}$s] }
t "dans une procédure"           {list [catch lit e] $e}

puts "--- étiquettes de champ"
type def paire {{string}nom; {int}age ?= 0}
set n ana; set a 30
t "littérales"                   {set {paire}p {/nom}jean {/age}12}
t "substituées"                  {set {paire}p {/nom}$n {/age}$a}
t "ordre libre, défaut"          {set {paire}p {/nom}zoe}
proc f {n a} {set {paire}q {/nom}$n {/age}$a; list $q(nom) $q(age)}
t "compilé"                      {f bob 40}
t "à travers une commande"       {set {paire}p [concat {/nom}jo] [concat {/age}3]}
t "ancienne écriture"            {set {paire}p {nom}jean {/age}12}
t "étiquette inconnue"           {set {paire}p {/nom}jean {/taille}3}
type def age int
t "type global age : aucun conflit" {set {paire}p {/nom}eve {/age}20; type get $p(age)}
t "{age} est le type age"        {show [concat {age}12]}

puts "--- étiquettes de variante"
type def S {{int}i, {string}s}
t "{/i}5"                        {set {S}v {/i}5; type get $v}
set x 7
t "{/i}\$x"                      {set {S}w {/i}$x; type get $w}
t "ancienne écriture {i}5"       {set {S}v {i}5}
t "{S/s}\$n, un type"            {show [concat {S/s}$n]}
t "contrat : set v {S/s}abc"     {set {S}v {/i}5; set v {S/s}abc; type get $v}
t "type cast S {/s}abc"          {type get [type cast S {/s}abc]}
t "liste {list.S}"               {set {list.S}L {/i}1 {/s}a; lmap e $L {type get $e}}
t "variante inconnue"            {set {S}v {/zz}5}

puts "--- une étiquette ne se range pas"
t "set y {/age}21"               {set y {/age}21}
t {set y [concat {/age}21]}      {set y [concat {/age}21]}

puts "--- variante dont le nom est aussi un type global"
type def {elf({int}n)} {list($n).int}
type def exe {{int}elf, {string}pe}
t "{/elf}5"                      {set {exe}e {/elf}5; type get $e}

puts "--- un nom annoté avec une substitution reste un nom"
type def rect {{{int}x; {int}y}A ; {{int}x; {int}y}B }
t "set {rect}Q(A, \$k)"          {set k B; set {rect}Q(A, $k) {1 1} {2 2}}
