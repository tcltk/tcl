# type get valeur : le type d'une valeur / d'une variable typée
set {int}n 5
puts "n      : [type get $n]"
set {list.int}L 1 2 3
puts "L      : [type get $L]"
set {double}d 2.5
puts "d      : [type get $d]"
type def Complex {{double}re = 0.0; {double}im = 0.0}
expr {z is Complex = (3,4)}
puts "z      : [type get $z]"
set s hello
puts "s      : <[type get $s]> (variable non typée)"
puts "n+1    : <[type get [expr {$n + 1}]]>"
set M {list}{}
puts "M      : [type get $M]"
puts "copie  : [type get [set c $L]]"
proc ty {v} { type get $v }
puts "proc   : [ty $L]"
catch {type get} m;       puts "args   : $m"
catch {type get a b} m;   puts "args2  : $m"
catch {type nimporte} m;  puts "sous-cmd: $m"
