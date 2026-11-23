# t118 : tranches à pas a:b:c, à la Python (la fin reste incluse, comme en
# Tcl) ; parties omises :b a: ::c ; pas négatif en lecture.
proc fmt {v w} { format %0${w}b $v }
proc t {} {
    # bitarray : lecture et écriture
    set {bitarray(16)}b 0
    set b(0:end:2) 1;        puts "b(0:end:2) 1   -> [fmt $b 16]"
    set b(::4) 0;            puts "b(::4) 0       -> [fmt $b 16]"
    set b(1:7:2) {1 0 1 0};  puts "b(1:7:2) liste -> [fmt $b 16]"
    puts "lu b(0:6:2)    -> [fmt $b(0:6:2) 4]"
    set b(:3) 5;             puts "b(:3) 5        -> [fmt $b 16]"
    set b(12:) 9;            puts "b(12:) 9       -> [fmt $b 16]"
    # msb0 : l'indice 0 est le bit de gauche
    set {bitarray(8,msb0)}m 0
    set m(0::3) 1;           puts "m(0::3) 1      -> [fmt $m 8]"
    # motif typé : un bitarray de la bonne largeur
    set {bitarray(8)}p 0
    set p(0:4:2) $m(0::3);   puts "p(0:4:2) motif -> [fmt $p 8]"
    foreach {c} {{set p(0:end:2) $m(0::3)} {set b(0:end:0) 1}
	    {set b(0:end:2:1) 1} {set b(::2) 2} {set b(::2) {1 1}}} {
	catch $c e; puts "erreur          -> $e"
    }
    # grand bitarray : écriture sur place, la copie n'est pas touchée
    set {bitarray(200)}g 0
    set h $g
    set g(3::50) 1
    puts "g(3::50) 1     -> popcount [expr {popcount($g)}], copie $h"
    puts "lu g(3::50)    -> $g(3::50)"
    # listes
    set {list.int}L 0 1 2 3 4 5 6 7
    puts "L(0:end:2)     -> $L(0:end:2)"
    puts "L(::-1)        -> $L(::-1)"
    puts "L(end:0:-3)    -> $L(end:0:-3)"
    puts "L(:2) L(5:)    -> $L(:2) | $L(5:)"
    set M $L
    set L(1::2) 0;           puts "L(1::2) 0      -> $L (copie $M)"
    set L(::4) 7 8;          puts "L(::4) 7 8     -> $L"
    set L(0:1) {5 6};        puts "L(0:1) {5 6}   -> $L"
    foreach {c} {{set L(::4) 1 2 3} {set L(::2) x}} {
	catch $c e; puts "erreur          -> $e"
    }
    # liste compacte : écriture sur place
    type def {-compact}octets list.int(u,8)
    set {octets}o {*}[lrepeat 10 1]
    set o(4::2) 0;           puts "o(4::2) 0      -> $o"
    catch {set o(::3) 300} e; puts "erreur          -> $e / $o"
}
t
# le crible d'Ératosthène : une tranche par nombre premier
proc crible {N} {
    set {bitarray(1000)}b 0
    for {set i 2} {$i*$i < $N} {incr i} {
	if {!$b($i)} {set b([expr {$i*$i}]:[expr {$N-1}]:$i) 1}
    }
    expr {$N - 2 - popcount($b)}
}
puts "premiers < 1000 -> [crible 1000]"
