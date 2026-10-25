# t81 : codec C pour les chaînes (taille fixe, cstring), les sommes de
#   littéraux et les sommes balisées S($champ) : mêmes octets et mêmes
#   valeurs (texte, types des champs, variante) que la couche Tcl.
set ici [file dirname [file normalize [info script]]]
source [file join $ici .. layout filebinary.tcl]
source [file join $ici .. layout binaliases.tcl]
proc tclc {args} {set ::tcl_typebinaryc 0; try {uplevel 1 $args} finally {set ::tcl_typebinaryc 1}}
proc champs {v} {
    if {[catch {set c [::tcl::unsupported::content $v]}]} {return [type get $v]}
    set r [type get $v]
    foreach f [type get -fields [type get $v]] {catch {lappend r $f=[type get $v($f)]}}
    set r
}
proc essai {nom T v args} {
    set o  [type binary encode $T $v {*}$args]
    set ot [tclc type binary encode $T $v {*}$args]
    set d  [type binary decode $T $o {*}$args]
    set dt [tclc type binary decode $T $o {*}$args]
    puts [format "%-12s %3d octets  C=Tcl %d  relu C=Tcl %d  relu=v %d  %s" $nom \
        [string length $o] [expr {$o eq $ot}] [expr {$d eq $dt && [type get $d] eq [type get $dt]}] \
        [expr {$d eq $v}] [binary encode hex $o]]
}
type def mz {"MZ"}
type def entete {{mz}sig; {u16le}n; {string(raw,4)}tag; {string(utf-8,8)}nom; {string(utf-8,z)}texte; {string(utf-16le,z)}large}
set {entete}e MZ 7 ab\x01d Été cstring héhé
essai entete entete $e
essai naturel entete $e -align natural
type def pp {{u16le}a; {u16le}b}
type def corps {{pp}1, {string(raw,3)}2, {u32le}7}
type def bloc {{u8}genre; {corps($genre)}corps; {u8}fin = 255}
set {pp}p 5 6
set {bloc}b1(genre,corps) 1 {1}$p
set {bloc}b2(genre,corps) 2 {2}xyz
set {bloc}b7(genre,corps) 7 {7}99
foreach b [list $b1 $b2 $b7] {essai "bloc [lindex $b 0]" bloc $b}
type def corps2 {{u8}AA, {u16le}BB}
type def bloc2 {{string(raw,2)}k; {corps2($k)}v}
set {bloc2}c(k,v) BB {BB}513
essai "balise txt" bloc2 $c
set d [type binary decode bloc [type binary encode bloc $b1]]
puts "variante relue : [type get $d(corps)] $d(corps.1.b)"
puts "balise inconnue : [catch {type binary decode bloc [binary format cu 9]} m] [string range $m 0 60]"
puts "littéral faux : [catch {type binary decode entete [binary format a2 ZZ]} m] [string range $m 0 50]"
puts "tronqué : [catch {type binary decode entete [binary format a5 MZ\x07\x00a]} m] [string range $m 0 40]"
