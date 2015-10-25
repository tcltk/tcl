apply {{} {
    for {set i 0} {$i < 5000} {incr i} {
	clock scan [clock format $i -format %T] -format %T
    }
}}
