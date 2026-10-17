type def Complex {{double}re = 0.0; {double}im = 0.0}
# 1. déclarer et affecter
puts [expr {z is Complex = (3,4)}]
puts "z=$z  re=$z(re)  typed: [expr {z is Complex}]"
catch {expr {z = (7,abc)}} m; puts "refus valeur: $m"
catch {expr {y is Complex = (1,2,3)}} m; puts "refus 3 champs: $m"
puts "y existe? [info exists y]"
expr {n is int = 42}; puts "n=$n typed int: [expr {n is int}]"
catch {expr {n = 2.5}} m; puts "n reste int: $m"
# 2. affecter sans déclarer
expr {z = (5,6)}; puts "z=$z"
# 3. champs
expr {z(re) = 10}; puts "z=$z"
catch {expr {z(im) = "xyz"}} m; puts "champ refusé: $m"
expr {a(1) = 7}; puts "a(1)=$a(1)"
puts [expr {$z(re) + $z(im)}]
set {int}k 3
puts [expr {k is int}]
proc f {} {expr {w is Complex = (1,2)}; expr {w(im) = $w(im) * 10}; return $w}
puts [f]
