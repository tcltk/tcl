# t85 : codec C pour at(EXPR).T : la valeur est lue à la position EXPR (depuis
#   le début de la valeur lue) sans déplacer le curseur, et écrite à part puis
#   posée à cette position à la fin (zéros entre deux) : mêmes octets et mêmes
#   valeurs que la couche Tcl, y compris at(...) imbriqués, listes à nombre
#   dynamique, -offset et -align natural.
set ici [file dirname [file normalize [info script]]]
source [file join $ici .. layout filebinary.tcl]
source [file join $ici .. layout binaliases.tcl]
proc tclc {args} {set ::tcl_typebinaryc 0; try {uplevel 1 $args} finally {set ::tcl_typebinaryc 1}}
proc essai {nom T v args} {
    set o  [type binary encode $T $v {*}$args]
    set ot [tclc type binary encode $T $v {*}$args]
    set d  [type binary decode $T $o {*}$args]
    set dt [tclc type binary decode $T $o {*}$args]
    puts [format "%-14s %3d octets  C=Tcl %d  relu C=Tcl %d  relu=v %d  %s" $nom \
        [string length $o] [expr {$o eq $ot}] [expr {$d eq $dt && [type get $d] eq [type get $dt]}] \
        [expr {$d eq $v}] [binary encode hex $o]]
}
type def pt {{u16le}x; {u16le}y}
type def tete {{u32le}off; {u8}n; {at($off).list($n).pt}pts; {u8}fin}
set {tete}t 12 2 {{1 2} {3 4}} 255
essai "at liste" tete $t
essai "at nat" tete $t -align natural
type def hdr {{string(raw,2)}sig; {u16le}poff; {at($poff * 2).pt}p; {at($poff * 2 + 4).u32le}z}
set {hdr}h MZ 4 {7 8} 99
essai "at calculé" hdr $h
type def chaine {{u8}o; {u8}l; {at($o).string(utf-8,$l)}s}
set {chaine}c 3 5 été
essai "at chaîne" chaine $c
type def inner {{u8}k; {at(10).u8}far}
type def outer {{u8}a; {at(4).inner}in}
set {inner}ii 5 6
set {outer}oo(a,in) 1 $ii
essai "at imbriqué" outer $oo
type def recouv {{u8}a; {at(0).u8}b}
set {recouv}r 1 2
essai "recouvrement" recouv $r

set o [type binary encode tete $t]
set d [type binary decode tete "\x00\x00\x00$o" -offset 3]
set dt [tclc type binary decode tete "\x00\x00\x00$o" -offset 3]
puts "-offset : [expr {$d eq $dt}] $d"
type binary decode tete $o -consumed n1
tclc type binary decode tete $o -consumed n2
puts "-consumed : $n1 $n2"
puts "hors données : [catch {type binary decode tete [binary format iucu 100 1]} m] [string range $m 0 60]"
type def p2 {{u8}o; {at($h(o)).u8}x}
puts "chemin : [catch {essai chemin p2 [set {p2}z 1 2]} m] $m"
