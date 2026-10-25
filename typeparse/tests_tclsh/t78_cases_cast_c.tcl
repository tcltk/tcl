# t78 : cast vers une liste en cases (copie d'un bloc depuis des cases de
#   même disposition, ou écriture directe depuis une liste ordinaire), et
#   tcl::unsupported::compact cells (Tcl_AnnotatedCreateCells : une valeur
#   construite depuis un tableau C).
set ici [file dirname [file normalize [info script]]]
source [file join $ici .. layout filebinary.tcl]
source [file join $ici .. layout binaliases.tcl]
proc c {v} {expr {[tcl::unsupported::compact info $v] ne "" ? "cases" : "liste"}}

type def point {{double}x; {double}y}
type def {-compact}polygone list.point
type def {-compact}pointC {{f64le}x; {f64le}y}
type def polyC {{u32le}n; {list($n).pointC}pts}
type def {-compact}ps {{int(s,32)}a}
type def {-compact}pu {{int(u,32)}a}

set {polygone}P {0.5 1} {2 3.25}
set L [type cast list.pointC $P]
puts "cases -> cases  : $L [type get $L] [c $L] [c $P]"
set {list.point}Q {0.5 1} {2 3.25}
set L [type cast list.pointC $Q]
puts "liste -> cases  : $L [type get $L] [c $L] [c $Q]"
set {polyC}p 2 $L
puts "dans polyC      : $p [c $p(pts)]"
puts "list(2)         : [type cast list(2).pointC $P] [catch {type cast list(3).pointC $P} m] $m"
set {list.ps}S {-1} {7}
puts "signe différent : [catch {type cast list.pu $S} m] $m"
set {list.ps}S2 {1} {7}
set U [type cast list.pu $S2]
puts "même valeurs    : $U [c $U]"
puts "texte invalide  : [catch {type cast list.pointC {{a b}}} m] $m"

# un tableau C de deux point : {1.5 2.5} {-3.0 4.0}
set octets [binary format d4 {1.5 2.5 -3.0 4.0}]
set V [tcl::unsupported::compact cells polygone $octets 16]
puts "cells           : $V [type get $V] [c $V] [expr {$V(1.x) * 2}]"
set W [tcl::unsupported::compact cells list.point $octets]
puts "sans stride     : $W [type get $W]"
puts "erreurs :"
foreach cmd {
    {tcl::unsupported::compact cells polygone $octets 24}
    {tcl::unsupported::compact cells polygone [string range $octets 0 9]}
    {tcl::unsupported::compact cells list(3).point $octets}
    {tcl::unsupported::compact cells list.string $octets}
    {tcl::unsupported::compact cells point $octets}
} {puts "  [catch $cmd m] $m"}
lappend V {9 9}
puts "lappend         : $V [c $V]"
