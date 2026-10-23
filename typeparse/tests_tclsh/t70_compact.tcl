# t70 : représentation compacte (prototype). Chaque cas tourne deux fois,
#   forme ordinaire puis forme compacte : les résultats doivent être les mêmes.
#   « forme » dit si la valeur finale est compacte.
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
type def point {{double}x; {double}y}
type def ptC {{double(64,le)}x; {double(64,le)}y}
type def pt3 {{int(s,32)}id; {point}p; {double(32)}w}
proc deux {l s} {
    set r {}
    foreach mode {off on} {
        tcl::unsupported::compact $mode
        uplevel 1 {unset -nocomplain P Q A e}
        set rc [catch {uplevel 1 $s} m]
        lappend r "$rc $m"
    }
    tcl::unsupported::compact off
    set f [uplevel 1 {if {[info exists P]} {tcl::unsupported::compact info $P}}]
    set forme [expr {$f eq "" ? "ordinaire" : "compacte"}]
    if {[lindex $r 0] eq [lindex $r 1]} {
        puts [format "%-30s identique  %-9s %s" $l $forme [lindex $r 1]]
    } else {
        puts [format "%-30s DIFFÉRENT : %s / %s" $l [lindex $r 0] [lindex $r 1]]
    }
}
tcl::unsupported::compact on
puts "disposition point : [tcl::unsupported::compact layout point]"
puts "disposition pt3   : [tcl::unsupported::compact layout pt3]"
catch {tcl::unsupported::compact layout {{string}s}} m; puts "string : $m"
catch {tcl::unsupported::compact layout int} m; puts "int : $m"
tcl::unsupported::compact off

deux "construire"      {set {list.point}P {0.5 1.5} {2.5 3.5} {-1.0 4.0}; set P}
deux "type, llength"   {set {list.point}P {0.5 1.5} {2.5 3.5}; list [type get $P] [llength $P]}
deux "lindex, type"    {set {list.point}P {0.5 1.5} {2.5 3.5}; set e [lindex $P 1]; list $e [type get $e] $e(y)}
deux "chemin P(1.y)"   {set {list.point}P {0.5 1.5} {2.5 3.5}; list $P(1.y) [type get $P(1.y)] $P(end.x)}
deux "foreach"         {set {list.point}P {0.5 1.5} {2.5 3.5}; set o {}; foreach e $P {lappend o $e(x) [type get $e]}; set o}
deux "écrire P(1.y)"   {set {list.point}P {0.5 1.5} {2.5 3.5}; set P(1.y) 9.5; set P}
deux "écrire 9 (non canonique)" {set {list.point}P {0.5 1.5} {2.5 3.5}; set P(1.y) 9; set P}
deux "lappend"         {set {list.point}P {0.5 1.5}; lappend P {7.5 8.5}; set P}
deux "lappend invalide" {set {list.point}P {0.5 1.5}; lappend P {a b}}
deux "lset"            {set {list.point}P {0.5 1.5} {2.5 3.5}; lset P 0 {1.25 2.25}; set P}
deux "lrange, lsort"   {set {list.point}P {2.5 3.5} {0.5 1.5}; list [lrange $P 1 end] [lsort $P]}
deux "non canonique 1 2" {set {list.point}P {1 2} {3 4}; set P}
deux "espaces gardés"  {set P [type cast list.point "{0.5 1.5}   {2.5 3.5}"]; set P}
deux "produit imbriqué" {set {list.pt3}P {7 {0.5 0.25} 0.5} {-3 {1.0 2.0} 0.25}; list $P $P(1.p.y) [type get $P(1.p)]}
deux "int(s,32) trop grand" {set {list.pt3}P {7 {0.5 0.25} 0.5}; set P(0.id) 99999999999}
deux "double(32) inexact" {set {list.pt3}P {7 {0.5 0.25} 0.1}; set P}
deux "u8"              {set {list.int(u,8)}P 1 2 3 255; list $P [catch {lappend P 256} m] $m $P}
deux "vecteur de double" {set {list.double}P 0.1 0.2 0.30000000000000004; list $P [expr {$P(0) + $P(2)}]}
deux "lire après lindex"    {set {list.point}P {0.5 1.5} {2.5 3.5}; set e [lindex $P 0]; list $P(0.x) $P(0.y)}
deux "vue gardée après écriture" {set {list.point}P {0.5 1.5} {2.5 3.5}; set e [lindex $P 0]; set P(0.x) 7.5; list $e $P}
deux "copie puis écriture" {set {list.point}P {0.5 1.5}; set Q $P; set Q(0.y) 5.5; list $P $Q}
deux "élément vers autre liste" {set {list.point}A {0.5 1.5} {2.5 3.5}; set {list.point}P [lindex $A 1] [lindex $A 0]; set P}
deux "string length"   {set {list.point}P {0.5 1.5}; list [string length $P] [type get $P]}
deux "contenu"         {set {list.point}P {0.5 1.5}; tcl::unsupported::content $P}
deux "binary decode"   {set P [type binary decode list(2).ptC [binary format q4 {1.5 2.5 3.5 4.5}]]; set P}
