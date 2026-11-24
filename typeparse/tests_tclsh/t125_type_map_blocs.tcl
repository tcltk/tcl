# type map par blocs (v17n) : le résultat doit être celui d'expr, élément par
# élément, quels que soient les éléments qui repassent un par un.
type def {-compact}vd list.double
proc ref {X Y e} {
    lmap x $X y $Y { if {[catch {expr $e} r]} { set r ERR } ; set r }
}
proc cmp {nom X Y e} {
    set a [type map {x} $X {y} $Y $e]
    set b [ref $X $Y $e]
    set ok [expr {[llength $a] == [llength $b]}]
    foreach u $a v $b { if {$u ne $v} { set ok 0; break } }
    puts "$nom : [expr {$ok ? "identique" : "ÉCART"}] ([llength $a] éléments)"
}
# 1000 éléments : 4 blocs ; des cas à reprendre aux bords des blocs
set X {}; set Y {}
for {set i 0} {$i < 1000} {incr i} { lappend X [expr {$i * 0.25 + 1}]; lappend Y [expr {1.0 - $i * 0.001}] }
foreach i {0 255 256 511 512 999} { lset X $i [expr {$i % 2 ? 3 : 0.0}] }
lset Y 300 1000
cmp "entiers et zéros aux bords" $X $Y {($x / $y)}
cmp "racines" $X $Y {(sqrt($y) * $x)}
cmp "log" $X $Y {(log($y) + $x)}
cmp "débordement" $X $Y {($x * 1e306 * $x)}
cmp "fonctions" $X $Y {(atan2($x, $y) - hypot($x, $y) / ($y - 0.5))}
# l'erreur au milieu d'un bloc : l'indice de l'élément
set Z $X; lset Z 300 abc
catch {type map {x} $Z {y} $Y {($x + $y)}} m o
puts "erreur : $m / [regexp -inline {type map element \d+} [dict get $o -errorinfo]]"
# en cases : l'infini passe tel quel, ou repasse par expr s'il est calculé
set C [type cast vd {1.5 Inf 2.5 -Inf}]
catch {type map {x} $C {($x)}} r; puts "infini seul : $r"
catch {type map {x} $C {($x + 1)}} r; puts "infini calculé : $r"
catch {type map {x} $C {(1.0 / $x)}} r; puts "inverse : $r"
# les variables gardent le dernier élément
type map {x} $X {y} $Y {($x + $y)}
puts "après : x=$x y=$y"
# trois colonnes en cases, et la même variable deux fois (la dernière l'emporte)
set A [type cast vd [lrange $Y 0 599]]
puts "cases : [lrange [type map {x} $A {y} $A {($x * $y + 1)}] 597 599]"
puts "doublon : [lrange [type map {x} [lrange $X 0 599] {x} $A {($x * 2)}] 0 2]"
puts "somme : [type map -sum {x} $A {y} $A {($x * $y)}]"
