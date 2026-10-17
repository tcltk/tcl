proc new {re im} { list [expr {double($re)}] [expr {double($im)}] }
puts "new is cmd = [expr {new is cmd}]"
puts "new(3,4)   = [expr {new(3,4)}]"
type def Complex {{double}re = 0.0; {double}im = 0.0}
expr {z is Complex = new(5,6)}
puts "z=$z typed=[expr {z is Complex}]"
puts "inconnue is cmd = [expr {inconnue is cmd}]"
proc add {a b} {expr {$a + $b}}
puts [expr {add is cmd && add(2,3) == 5}]
namespace eval geo { proc dist {x y} {expr {hypot($x,$y)}}
  puts "dans geo: [expr {dist is cmd}] [expr {dist(3,4)}]" }
puts "fonctions: [lsort [info commands ::tcl::mathfunc::n*]] [info commands ::tcl::mathfunc::dist]"
proc g {} { expr {(new is cmd; (a,b) = new(7,8); $a+$b)} }
puts [g]
puts [expr {(1,2)==(1,2)}]
