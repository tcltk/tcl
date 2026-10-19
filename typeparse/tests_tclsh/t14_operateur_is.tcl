type def complex {{double}re = 0.0; {double}im = 0.0}
set {complex}z {re}3 {im}4
set {int}n 5
set s hello
set plain 42
puts "z is complex      : [expr {z is complex}]"
puts "n is int          : [expr {n is int}]"
puts "n is double       : [expr {n is double}]"
puts "n is string       : [expr {n is string}]"
puts "z is int          : [expr {z is int}]"
puts "plain is int      : [expr {plain is int}]"
puts "s is int          : [expr {s is int}]"
puts "nope is int       : [expr {nope is int}]"
set name n; puts "\$name is int     : [expr {$name is int}]"
puts [expr {z is complex && n is int}]
puts [expr {!(s is int) ? "pas int" : "int"}]
if {z is complex} {puts "if ok"}
catch {expr {z is bogus}} m; puts "type inconnu: $m"
set T complex; puts "type en variable: [expr {z is $T}]"
