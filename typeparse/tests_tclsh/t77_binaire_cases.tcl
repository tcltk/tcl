# t77 : type binary encode / decode d'une liste en cases : copie des
#   scalaires quand la disposition des cases est celle des octets (mêmes
#   octets que le codec Tcl et que sans cases), retour au chemin ordinaire
#   sinon (autre boutisme, -strictpad, compte faux).
set ici [file dirname [file normalize [info script]]]
source [file join $ici .. layout filebinary.tcl]
source [file join $ici .. layout binaliases.tcl]
proc c {v} {expr {[tcl::unsupported::compact info $v] ne "" ? "cases" : "liste"}}
proc tcl_codec {args} {set ::tcl_typebinaryc 0; try {uplevel 1 $args} finally {set ::tcl_typebinaryc 1}}

type def {-compact}pointC {{f64le}x; {f64le}y}
type def polyC {{u32le}n; {list($n).pointC}pts}
type def pointD {{f64le}x; {f64le}y}
type def polyD {{u32le}n; {list($n).pointD}pts}
type def {-compact}pix {{u8}r; {u16le}g; {u8}b; {i32le}k}
type def img {{u16le}n; {list($n).pix}px}
type def pbe {{f64be}x; {f64be}y}   ;# autre boutisme : jamais en cases
type def polyB {{u32be}n; {list($n).pbe}pts}

set pts [lmap k [lseq 50] {list [expr {$k * 0.25}] [expr {-$k / 3.0}]}]
set {polyC}p 50 $pts
set {polyD}d 50 $pts
foreach al {natural packed 4} {
    set o [type binary encode polyC $p -align $al]
    set q [type binary decode polyC $o -align $al]
    puts [format "%-8s %6d octets  codec Tcl : %d  sans cases : %d  relu : %d %s" $al \
        [string length $o] [expr {$o eq [tcl_codec type binary encode polyC $p -align $al]}] \
        [expr {$o eq [type binary encode polyD $d -align $al]}] [expr {$q eq $p}] [c $q(pts)]]
}
set {img}im 3 {{1 2 3 -4} {255 65535 0 2147483647} {0 0 0 -2147483648}}
foreach al {natural packed} {
    set o [type binary encode img $im -align $al]
    set q [type binary decode img $o -align $al]
    puts "img $al : [string length $o] octets, codec Tcl [expr {$o eq [tcl_codec type binary encode img $im -align $al]}], relu [expr {$q eq $im}] [c $q(px)]"
}
set {polyB}b 2 {{1.5 2.5} {3.5 4.5}}
set o [type binary encode polyB $b -align natural]
puts "gros-boutiste : [string length $o] octets, codec Tcl [expr {$o eq [tcl_codec type binary encode polyB $b -align natural]}], relu [expr {[type binary decode polyB $o -align natural] eq $b}]"
set o [type binary encode polyC $p -align natural]
puts "strictpad : [expr {[type binary decode polyC $o -align natural -strictpad 1] eq $p}]"
set p(n) 49
puts "compte faux : [catch {type binary encode polyC $p -align natural} m] [string range $m 0 40]"
set {polyC}v 0 {}
set o [type binary encode polyC $v -align natural]
puts "vide : [string length $o] [type binary decode polyC $o -align natural]"
