# t111 (v17d) : un seul ordre de résolution pour set, type cast et les
#   arguments d'une procédure. Une valeur typée : le même type,
#   l'élargissement, la variante d'une somme ; une règle (type convert) ou
#   une conversion intégrée reste explicite (type cast, {T}$x), arguments
#   compris. Une valeur nue : le lecteur natif, la validation d'un nominal
#   (règle depuis son type sous-jacent), puis une règle any T ou string T.
type def Celsius double
type def Fahrenheit double
type convert Celsius Fahrenheit {{c} {expr {$c * 9.0 / 5 + 32}}}
type def S {{int}i, {double}d}
type def S2 {{Celsius}c, {string}s}
type convert Celsius S2 {{c} {list s "via règle"}}
type def Jour string
type convert string Jour {{s} {
    if {![regexp {^\d{4}-\d\d-\d\d$} $s]} {error "« $s » n'est pas une date"}
    return $s
}}
type def Pos double
type convert double Pos {{x} {if {$x < 0} {error "$x < 0"}; return $x}}
type def Pt {{double}x; {double}y}
type convert any Pt {{s} {
    if {[regexp {^\((\S+),(\S+)\)$} $s -> a b]} {return [list $a $b]}
    error "pas un point"
}}

proc r {script} {
    if {[catch {uplevel 1 $script} v]} {return "ERREUR [lindex [split $v \n] 0]"}
    list $v [type get $v]
}
proc via_set  {T x} {r {eval "set {$T}v \$x"}}
proc via_cast {T x} {r {type cast $T $x}}
proc via_arg  {T x} {
    proc ::f_$T "{$T}a" {return $a}
    r {::f_$T $x}
}
set cas {
  "même type"                  S          {[type cast S {i 5}]}
  "variante (int→S)"           S          {[type cast int 5]}
  "élargissement int→double"   double     {[type cast int 2]}
  "nominal→base"               double     {[type cast Celsius 3]}
  "variante avant règle"       S2         {[type cast Celsius 10]}
  "règle : explicite"          Fahrenheit {[type cast Celsius 10]}
  "troncature : explicite"     int        {[type cast double 2.5]}
  "base→nominal : explicite"   Celsius    {[type cast double 3]}
  "string→int : explicite"     int        {[type cast string 2]}
  "vers string : lu"           string     {[type cast int 2]}
  "sans chemin"                Fahrenheit {[type cast Pt {1 2}]}
  "nu : lecteur natif"         Pt         {[list 1 2]}
  "nu : règle any"             Pt         {(1,2)}
  "nu : ni l'un ni l'autre"    Pt         {abc}
  "nu : nominal validé"        Jour       {2026-11-14}
  "nu : nominal refusé"        Jour       {10/11}
  "nu : nominal numérique"     Pos        {-3}
  "nu : 2.5 vers int"          int        {2.5}
  "noté (note inconnue)"       Pt         {[set _ {zz}[string cat (1,2)]]}
}
foreach {nom T expr} $cas {
    set x [subst $expr]
    puts "$nom\n    set:  [via_set $T $x]\n    cast: [via_cast $T $x]\n    arg:  [via_arg $T $x]"
}

puts "== le cast s'écrit à l'appel"
proc VeutEntier {{int}i} {list $i [type get $i]}
proc VeutF {{Fahrenheit}f} {list $f [type get $f]}
set {double}d 2.5
set {Celsius}c 10
foreach s {
    {VeutEntier 2.5} {VeutEntier $d} {VeutEntier {int}$d}
    {VeutEntier [type cast int $d]} {VeutF $c} {VeutF {Fahrenheit}$c}
    {set {Fahrenheit}f $c} {set f {Fahrenheit}$c}
} {
    puts [format "%-32s %s" $s [expr {[catch $s v] ? "ERREUR $v" : $v}]]
}
puts "== set : un seul argument pour un produit"
puts [r {set {Pt}p {1 2}}]
puts [r {set {Pt}p (3,4)}]
puts [r {set {Pt}p 1}]
puts "== nominal de collection : base -> nominal s'écrit"
type def point111 {{double}x; {double}y}
type def polygone111 list.point111
type def carnet111 dict.int
proc aire111 {{polygone111}pg} {llength $pg}
set {list.point111}L {0 0} {4 0}
set {dict.int}D a 1
foreach s {
    {set {polygone111}P $L} {aire111 $L} {aire111 {polygone111}$L}
    {set {polygone111}P {polygone111}$L} {set {carnet111}C $D}
    {set {list.point111}L2 [type cast polygone111 $L]}
} {
    puts [format "%-36s %s" $s [expr {[catch $s v] ? "ERREUR $v" : $v}]]
}
