# bitarray(n, ordre) : lsb0 (défaut) ou msb0
set {bitarray(8)}b 0xB2
puts [type get $b]
puts "lsb0 : [lmap i {0 1 2 3 4 5 6 7} {set b($i)}]"
set {bitarray(8,msb0)}c 0xB2
puts "msb0 : [lmap i {0 1 2 3 4 5 6 7} {set c($i)}]"
# le défaut est lsb0 : même type
puts [expr {[type get $b] eq [type get [set {bitarray(8,lsb0)}b2 1]]}]
# tranches
puts "b(1:4)=$b(1:4) [type get $b(1:4)] ; c(0:3)=$c(0:3) [type get $c(0:3)]"
puts "end : $b(end) $c(end) $b(end-1:end)"
# écriture msb0
set c(0) 0
puts "c apres c(0)=0 : $c"
set c(4:7) 15
puts "c apres c(4:7)=15 : $c"
expr {"c(0:1)" = 3}
puts "c apres c(0:1)=3 : $c"
# la valeur d'un champ de produit
type def reg {{bitarray(8)}flags = 0; {bitarray(16,msb0)}addr = 0}
set {reg}r {flags}5 {addr}0x8001
puts "r=$r  flags(2)=[set r(flags.2)]  addr(0)=[set r(addr.0)] addr(15)=[set r(addr.15)]"
set r(flags.7) 1
puts "r=$r"
# liste de bitarrays
set {list.bitarray(4)}L 1 2 15
puts "L=$L  L(2)=$L(2) type [type get $L(2)] bit L(2.3)=[set L(2.3)]"
# erreurs
foreach s {
    {set {bitarray(8)}z 256}
    {set {bitarray(8)}z -1}
    {set {bitarray(8)}z abc}
    {set b(8)}
    {set b(3:1)}
    {set b(0:1) 4}
    {set b(0) 2}
    {set {bitarray(8,msb0)}z $b}
    {set {bitarray(0)}z 1}
    {set {bitarray(8,xx)}z 1}
} { puts "[catch $s m] $m" }
# popcount, bitlen
puts [expr {popcount(255)}],[expr {popcount($b)}],[expr {bitlen($b)}],[expr {bitlen(255)}]
puts [catch {expr {popcount(-1)}} m]\ $m
# largeur > 64
set {bitarray(130)}w 0
set w(129) 1
set w(0) 1
puts "w=$w pop=[expr {popcount($w)}] w(129)=$w(129) w(0:1)=$w(0:1)"
# procedure
proc f {} { set {bitarray(4)}q 6; set q(0) 1; return $q }
puts [f]
# opérations bit à bit via expr : le résultat doit rester conforme
set {bitarray(8)}m 0xF0
set m [expr {$m | 0x0F}]
puts "m=$m [type get $m]"
puts [catch {set m [expr {$m << 1}]} e]\ $e
