# Somme directe d'une colonne d'entiers en cases (v17t) : foreach v $L
# {incr c $v}, (c = $c + $v), type map -fold : mêmes résultats que la boucle,
# dépassement de 64 bits compris (le chemin par blocs reprend).
foreach {t d} {u8 int(u,8) i8 int(s,8) u16 int(u,16) i16 int(s,16) u32 int(u,32) i32 int(s,32)} {
    type def {-compact}L$t list.$d
}
proc p1 {L c} { foreach v $L {incr c $v}; list $c $v }
proc p2 {L c} { foreach v $L {(c = $c + $v)}; set c }
proc p3 {L c} { type map -fold [list c $c] {v} $L {($c + $v)} }
foreach {t vals} {
    u8  {255 255 0 1 7}
    i8  {-128 127 -1 5}
    u16 {65535 65535 3}
    i16 {-32768 32767 -2}
    u32 {4294967295 4294967295 1}
    i32 {-2147483648 2147483647 -9}
} {
    set L [type cast L$t [concat {*}[lrepeat 10 $vals]]]
    foreach c0 {0 -5 9223372036854775000 -9223372036854775000 1.5 abc} {
        set r {}
        foreach p {p1 p2 p3} {
            if {[catch {$p $L $c0} m]} { set m "ERR $m" }
            lappend r $m
        }
        puts "$t $c0 : [join $r { | }]"
    }
}
