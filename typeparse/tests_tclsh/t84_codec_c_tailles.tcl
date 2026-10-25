# t84 : codec C pour les tailles et nombres dynamiques : préfixe de longueur
#   (string(utf-8,int(u,16,be)), list(int(u,8)).T) et expressions sur les
#   champs déjà lus ($len, $len - 4, ($a + $b) * 2, % et / au plancher) :
#   mêmes octets et mêmes valeurs que la couche Tcl ; les formes non couvertes
#   ($h(off), champ d'un produit extérieur) et les erreurs passent par elle.
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
type def pascal {{u8}k; {string(utf-8,int(u,16,be))}nom; {string(raw,int(u,8))}brut}
set {pascal}p 1 Été ab\x00c
essai pascal pascal $p
essai "pascal nat" pascal $p -align natural

type def blob {{u32le}len; {string(raw,$len)}data; {u16le}fin}
set {blob}b 5 hello 7
essai blob blob $b
type def txt {{u16le}len; {string(utf-8,$len - 4)}t; {u8}z}
set {txt}t 8 abcd 1
essai "texte -4" txt $t
type def pt {{u16le}x; {u16le}y}
type def nuage {{u8}a; {u8}b; {list(($a + $b) * 2).pt}pts}
set {nuage}n 1 0 {{1 2} {3 4}}
essai {($a+$b)*2} nuage $n
essai "nat" nuage $n -align natural
type def divmod {{u8}a; {list($a / 3).u8}q; {list($a % 3).u8}r}
set {divmod}dm 7 {1 2} {9}
essai "/ et %" divmod $dm

type def pts8 {{u8}tag; {list(int(u,8)).pt}pts; {u32le}fin}
set {pts8}q 9 {{1 2} {3 4} {5 6}} 77
essai "préfixe liste" pts8 $q
essai "préfixe nat" pts8 $q -align natural
type def {-compact}cpt list.pt
essai "préfixe vide" pts8 [set {pts8}qv 9 {} 77]

type def wrap {{u8}k; {list(int(u,8)).u8}v}
set big {}; for {set i 0} {$i < 300} {incr i} {lappend big [expr {$i % 256}]}
set {wrap}w 1 $big
puts "préfixe tronqué : [expr {[type binary encode wrap $w] eq [tclc type binary encode wrap $w]}] [binary encode hex [string range [type binary encode wrap $w] 0 2]]"

puts "écart : [catch {type binary encode blob [set {blob}x 4 hello 7]} m] $m"
puts "tronqué : [catch {type binary decode blob [binary format iua3 9 abc]} m] $m"
puts "préfixe tronqué lu : [catch {type binary decode pascal [binary format cuSu 1 50]} m] $m"
type def hdr {{u16le}off; {u16le}len}
type def outer {{hdr}h; {string(raw,$h(len))}s}
set {hdr}hh 0 3
set {outer}o(h,s) $hh xyz
essai "chemin" outer $o
type def inner {{string(raw,$n)}s}
type def outer2 {{u8}n; {inner}i}
set {inner}ii abc
set {outer2}o2(n,i) 3 $ii
puts "extérieur : [catch {essai extérieur outer2 $o2} m] [string range $m 0 70]"
