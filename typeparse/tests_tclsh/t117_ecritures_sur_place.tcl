# t117 (23/11, v17l) : écritures sur place
#   - b(i) = 0|1 d'un bitarray tenu par la seule variable : le bit est écrit
#     dans les chiffres du grand entier (avant : un grand entier neuf par bit)
#   - lset L i v d'une liste typée d'entiers ou de flottants (en cases ou
#     non) : l'élément vérifié seul, écrit sur place (avant : copie et
#     revalidation de toute la liste à chaque lset)
#   - popcount sans copie ; un grand entier annoté se compare par son contenu
# Le partage reste sûr : une copie de la valeur n'est jamais touchée.
type def octs list.int(u,8)
type def {-compact}octc list.int(u,8)
proc bits {} {
    set {bitarray(200)}b 0; set b(150) 1; set c $b; set b(3) 1
    puts "partage : c=[expr {$c == 2**150}] b=[expr {$b == 2**150 + 8}] popcount=[expr {popcount($b)}]"
    set b(150) 0
    puts "effacé : $b popcount=[expr {popcount($b)}] b(3)=$b(3) b(150)=$b(150)"
    set {bitarray(100,msb0)}m 0; set m(0) 1; puts "msb0 : [expr {$m == 2**99}]"
    set {bitarray(300)}t 0
    trace add variable t write {apply {{a b o} {puts "  (trace)"}}}
    set t(250) 1
    puts "trace : [expr {$t == 2**250}]"
    if {[catch {set t(300) 1} e]} {puts "hors plage : $e"}
    if {[catch {set t(5) 2} e]} {puts "deux : $e"}
    puts "intact : [expr {$t == 2**250}]"
}
proc listes {} {
    set {octs}s 1 2 3; set t $s; lset s 0 9; puts "partage lset : s=$s t=$t"
    if {[catch {lset s 1 300} e]} {puts "refus : $e | s=$s"}
    set {octc}u 1 2 3; set w $u; lset u 2 7; puts "en cases : u=$u w=$w"
    if {[catch {lset u 1 x} e]} {puts "refus en cases : $e | u=$u"}
}
proc crible {N} {
    set {bitarray(10000)}c 0
    for {set i 2} {$i*$i < $N} {incr i} {
	if {!$c($i)} {for {set j [expr {$i*$i}]} {$j < $N} {incr j $i} {set c($j) 1}}
    }
    expr {$N - 2 - popcount($c)}
}
proc crible_octets {N} {
    set {octc}s {*}[lrepeat $N 1]
    for {set i 2} {$i*$i < $N} {incr i} {
	if {[lindex $s $i]} {for {set j [expr {$i*$i}]} {$j < $N} {incr j $i} {lset s $j 0}}
    }
    set n 0; for {set i 2} {$i < $N} {incr i} {incr n [lindex $s $i]}; return $n
}
bits
listes
puts "crible (bits) : [crible 10000] premiers sous 10000"
puts "crible (octets en cases) : [crible_octets 10000] premiers sous 10000"
set {bitarray(130)}g 0; set g(129) 1; set h $g; set g(0) 1
puts "global : [expr {$h == 2**129}] [expr {$g == 2**129 + 1}]"
