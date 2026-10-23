# t59 : un nominal de collection (type def polygone list.point) entre dans sa base structurelle
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]
type def point {{double}x; {double}y}
type def polygone list.point
type def px int
set {polygone}P {0 0} {4 0} {4 4} {0 4}
proc t {l s} { set r [catch {uplevel 1 $s} m]; puts [format "%-46s %s %s" $l $r $m] }
t "type cast list.point"           {type get [type cast list.point $P]}
t "type cast list(4).point"        {type get [type cast list(4).point $P]}
t "{list.point}\$P"                {type get {list.point}$P}
t "set {list.point}L \$P (affect.)" {set {list.point}L $P; type get $L}
t "set {list(4).point}L4 \$P"      {set {list(4).point}L4 $P; type get $L4}
type def polyC {{u32le}n; {list($n).point}pts}
t "champ list(\$n).point <- polygone" {set {polyC}c 4 $P; type get $c}
type def boite {{list.point}pts}
t "champ list.point <- polygone"   {set {boite}b $P; type get $b}
t "px entre dans int"              {set {px}x 5; set {int}i $x; type get $i}
t "polygone -> list nue (affect.)" {set {list}N $P; llength $N}
t "champ avec cast immédiat"   {set {polyC}c2 4 {list.point}$P; type get $c2}
t "champ boite cast immédiat"  {set {boite}b2 {list.point}$P; type get $b2}
type def triangle list(3).point
t "champ list(\$n) avec mauvais compte" {set {polyC}c3 3 $P}
t "nominal -> autre nominal (refus)"   {set {list.point}tmp $P; set {triangle}tr $P}
type def nuage list.point
type def boite2 {{nuage}n}
t "polygone dans champ nuage (refus)"  {set {boite2}b3 $P}
t "list(3).point <- polygone(4)"       {set {list(3).point}L3 $P}
type def poly3 list(3).point
type def b4 {{list(4).point}q}
set {poly3}P3 {0 0} {1 0} {0 1}
t "champ list(4) <- poly3 (refus)"     {set {b4}x $P3}
