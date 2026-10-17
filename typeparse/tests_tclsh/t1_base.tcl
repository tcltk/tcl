set {int}x 5
puts "x=$x"
type def point {{double}x; {double}y}
set {point}P 1 2
puts "P=$P"
puts "P(x)=$P(x)"
catch {set x abc} m; puts "set x abc -> $m"
set x 7
puts "x=$x"
catch {set P(y) 7} m; puts "write -> $m"
puts "P=$P"
