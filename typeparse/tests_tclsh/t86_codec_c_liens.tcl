# t86 : codec C pour les liens *(ORIGINE,REPR).T : origines //, ./, ., ../ ;
#   cible lue après tous les champs du produit (elle peut citer un champ qui
#   suit), écrite après la valeur puis son adresse reportée ; listes de liens,
#   liste chaînée par opt(T), lien vers une liste à nombre dynamique, liens
#   dans at(...) : mêmes octets et mêmes valeurs que la couche Tcl. Les cas
#   non couverts (lien nul d'un champ à défaut, -linkdepth N, cycle,
#   -dedupe) passent par elle, avec ses messages.
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
proc lit {nom T data args} {
    set d  [type binary decode $T $data {*}$args]
    set dt [tclc type binary decode $T $data {*}$args]
    puts [format "%-14s C=Tcl %d  %s" $nom [expr {$d eq $dt && [type get $d] eq [type get $dt]}] $d]
}
type def rec {{u8}a; {u8}b}
type def H {{u8}magic; {*(//,u32le).rec}r}
essai "//" H [set {H}h(magic,r) 7 {1 2}]
essai "// nat" H $h -align natural
type def T {{u8}n; {*(./,u8).rec}x; {*(./,u8).rec}y}
essai "./" T [set {T}t(n,x,y) 9 {5 6} {7 8}]
lit "./ lu" T "\x09\x04\x06\x00\x00\x01\x02\x03\x04"
type def U {{u8}pad; {*(.,u8).rec}x}
essai "." U [set {U}u(pad,x) 0 {7 8}]
type def inner {{u8}k; {*(../,u8).rec}z}
type def outer {{u8}pad; {inner}i}
lit "../" outer "\x00\x00\x05\x00\x00\x0a\x0b"
essai "../ écrit" outer [type binary decode outer "\x00\x00\x05\x00\x00\x0a\x0b"]
type def L {{u8}n; {list($n).*(//,u8).rec}rs}
essai "liste" L [set {L}l(n,rs) 2 {{1 2} {3 4}}]
type def S {{*(//,u16le).list($n).rec}tab; {u8}n}
essai "taille après" S [set {S}s(tab,n) {{1 2} {3 4} {5 6}} 3]
type def noeud {{u8}val; {*(//,u8).opt(noeud)}suiv}
set c [type binary decode noeud "\x01\x02\x02\x04\x03\x00"]
lit "chaînée" noeud "\x01\x02\x02\x04\x03\x00"
essai "chaînée écrite" noeud $c
type def sig {{u8}k; {u8}a; {u8}b; {*(.,i8).rec}r}
lit "signé" sig "\x01\x05\x06\xfe"
type def A {{u8}o; {at($o).*(//,u8).rec}r}
essai "at + lien" A [set {A}a(o,r) 4 {9 9}]

type def H0 {{u8}m; {*(//,u8,null).rec}r = {0 0}}
puts "lien nul à défaut : [type binary decode H0 "\x01\x00"] [binary encode hex [type binary encode H0 [set {H0}z {m}3]]]"
set fr [type binary decode H [type binary encode H $h] -linkdepth 0]
puts "-linkdepth 0 : $fr(r) [type get $fr(r)]"
type def boucle {{u8}v; {*(//,u8).boucle}s}
puts "cycle : [catch {type binary decode boucle "\x01\x00"} m] $m"
puts "dedupe : [binary encode hex [type binary encode T $t -dedupe 1]]"
set d ""; for {set i 0} {$i < 300} {incr i} {append d [binary format su2 [list $i [expr {$i < 299 ? 2*($i+1) + 2*($i+1) : 0}]]]}
type def n16 {{u16le}val; {*(//,u16le).opt(n16)}suiv}
set l1 [type binary decode n16 $d]; set l2 [tclc type binary decode n16 $d]
puts "chaîne de 300 : [expr {$l1 eq $l2}] [expr {[type binary encode n16 $l1] eq $d}]"
