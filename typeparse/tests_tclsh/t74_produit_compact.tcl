# t74 : type def {-compact} sur un produit (toute liste de ce produit est en
#   cases, comme une struct C) ; une liste champ d'un produit est en cases
#   (list($n).pointC dans polyC), y compris dans un produit imbriqué, après
#   une écriture par chemin, un lappend et un type binary decode.
set ici [file dirname [file normalize [info script]]]
source [file join $ici .. layout filebinary.tcl]
source [file join $ici .. layout binaliases.tcl]
proc c {v} {expr {[tcl::unsupported::compact info $v] ne "" ? "cases" : "liste"}}

type def {-compact}pointC {{f64le}x; {f64le}y}
type def polyC {{u32le}n; {list($n).pointC}pts}
type def boite {{u32le}id; {polyC}poly}
puts "type is {compact?}pointC : [type is {compact?}pointC]"
puts "type is {compact?}polyC  : [type is {compact?}polyC]"

set {polyC}p 2 {{1.0 2.0} {3 4.0}}
puts "p = $p   pts : [c $p(pts)]   (3 normalisé en 3.0)"
puts "p(pts.1.x) = $p(pts.1.x)   [type get $p(pts.1)]"
set p(pts.0.y) 7.5
puts "set p(pts.0.y) 7.5 : $p   pts : [c $p(pts)]"
set o [type binary encode polyC $p -align natural]
binary scan $o iu1x4d4 n d
puts "octets : [string length $o]   n = $n   $d"
set q [type binary decode polyC $o -align natural]
puts "decode : $q   pts : [c $q(pts)]"
set ::tcl_typebinaryc 0
puts "codec Tcl, mêmes octets : [expr {[type binary encode polyC $p -align natural] eq $o}]"
set ::tcl_typebinaryc 1
lappend p(pts) {5 6}
set p(n) 3
puts "lappend p(pts) : $p   pts : [c $p(pts)]"
set {boite}b 7 $p
puts "boite : b(poly.pts) : [c $b(poly.pts)]   b(poly.pts.2.y) = $b(poly.pts.2.y)"
set {list.pointC}L {1 2} {3 4}
puts "list.pointC : $L   [c $L]"
puts "refus : [catch {set {polyC}r 1 {{a 2}}} m] $m"
type def pointD {{double}x; {double}y}
type def polyD {{u32le}n; {list($n).pointD}pts}
set {polyD}d 1 {{1.0 2.0}}
# FR : sans {-compact}, en cases seulement si la forme compacte est activée partout
set attendu [expr {[tcl::unsupported::compact] eq "on" ? "cases" : "liste"}]
puts "sans -compact : $d   pts en cases si compact on : [expr {[c $d(pts)] eq $attendu}]"
