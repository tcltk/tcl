# t51 : codec C de type binary encode / decode (dispositions fixes).
# Chaque cas est fait deux fois : couche Tcl seule (::tcl_typebinaryc 0) et
# voie C (1). Les octets, la valeur décodée, son type, les types de ses
# parties, -consumed et les messages d'erreur doivent être identiques.
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]
type def Pt   {{i8}x; {i16be}y}
type def Box  {{u8}tag; {Pt}a; {list(2).Pt}b; {f32le}r}
type def Grid {{u8}k; {list(2).list(3).u16le}g; {u64le}big; {i64le}neg}
type def Hdr  {{u16le}n; {u8}pad; {list($n).f64le}v}
set {Pt}p1 x -5 y -300; set {Pt}p2 x 127 y 32767; set {Pt}p3 x -128 y 1
set {Box}bx tag 9 a $p1 b [list $p2 $p3] r 1.5
set {Grid}gr k 1 g {{1 2 3} {65535 0 7}} big 18446744073709551615 neg -9223372036854775808
set {Hdr}hd n 3 pad 0 v {1.0 -2.5 1e300}

# forme d'une valeur : texte, type, et types des parties (récursif)
proc forme {v} {
    set t [type get $v]
    if {[catch {llength $v} n] || $n < 2} {return "$t"}
    set parts {}
    foreach e $v {lappend parts [forme $e]}
    return "$t\[[join $parts ,]\]"
}
proc deux {label script} {
    foreach m {0 1} {
        set ::tcl_typebinaryc $m
        unset -nocomplain ::used
        set r($m) [catch {uplevel #0 $script} v]
        if {$r($m)} {
            set s($m) "ERREUR: [lindex [split $v \n] 0]"
        } elseif {[type get $v] eq ""} {
            binary scan $v H* hex
            set s($m) "octets [string length $v]: [string range $hex 0 47]"
        } else {
            set s($m) "$v | [forme $v]"
        }
        if {[info exists ::used]} {append s($m) " | consumed $::used"}
    }
    puts [format "%-26s %s %s" $label [expr {$s(0) eq $s(1) ? "==" : "DIFF"}] $s(1)]
    if {$s(0) ne $s(1)} {puts "    tcl: $s(0)"}
}
foreach al {packed natural 4} {
    foreach en {little big} {
        deux "encode Box $al $en" [list type binary encode Box $bx -align $al -endian $en]
        set ::tcl_typebinaryc 0; set b [type binary encode Box $bx -align $al -endian $en]
        deux "decode Box $al $en" [list type binary decode Box $b -align $al -endian $en -consumed ::used]
        deux "decode Box -offset 3" [list type binary decode Box "\x01\x02\x03$b" -offset 3 -align $al -endian $en -consumed ::used]
        deux "decode Box tronqué" [list type binary decode Box [string range $b 0 end-1] -align $al -endian $en]
    }
}
foreach al {packed natural} {
    deux "encode Grid $al" [list type binary encode Grid $gr -align $al]
    set ::tcl_typebinaryc 0; set b [type binary encode Grid $gr -align $al]
    deux "decode Grid $al" [list type binary decode Grid $b -align $al -consumed ::used]
}
set ::tcl_typebinaryc 0; set b [type binary encode Hdr $hd -align natural]
deux "encode Hdr"              [list type binary encode Hdr $hd -align natural]
deux "decode Hdr"              [list type binary decode Hdr $b -align natural]
deux "decode Hdr -strictpad"   [list type binary decode Hdr $b -align natural -strictpad 1]
set sale [string replace $b 3 3 \x07]
deux "remplissage non nul"     [list type binary decode Hdr $sale -align natural]
deux "idem, -strictpad 1"      [list type binary decode Hdr $sale -align natural -strictpad 1]
deux "n trop grand"            [list type binary decode Hdr "\xff\x00[string range $b 2 end]" -align natural]
deux "option inconnue"         [list type binary decode Hdr $b -bogus 1]
deux "boutisme faux"           [list type binary decode Hdr $b -endian middle]
deux "liste (couche Tcl)"      [list type binary decode {list(2).u8} "\x01\x02"]
deux "liste de Pt (couche Tcl)" [list type binary decode {list(2).Pt} "\x01\x00\x02\x03\x00\x04"]
set {Hdr}faux n 2 pad 0 v {1.0 2.0}
deux "encode, n incohérent"    {set ::faux2 [lreplace $::faux 0 0 5]; type binary encode Hdr $::faux2}
