set {int}x 5
puts [catch {expr {$x+1}} m]:$m
puts [catch {incr x} m]:$m
puts [catch {string length $x} m]:$m
puts [catch {set y [expr {$x*2}]} m]:$m
type def pts {list.int}
set {pts}L 1 2 3
puts [catch {llength $L} m]:$m
puts [catch {lindex $L 1} m]:$m
unset -nocomplain m
puts [catch {lappend L 4} m]:$m
puts [catch {expr {[lindex $L 0] + 1}} m]:$m
proc f {} { set {int}v 3; puts "v=$v"; return [expr {$v+1}] }
puts [catch f m]:$m
