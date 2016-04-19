proc fib {n} {
	# Very bogus we have to do {$n - 1} to get performance.
	# But if we don't this takes 35 seconds.  Tcl has issues.
	expr {$n < 2 ? 1 : [fib [expr {$n -2}]] + [fib [expr {$n -1}]]}
}

set i 0
while {$i <= 30} {
	puts "n=$i => [fib $i]"
	incr i
}
