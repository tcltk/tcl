apply {{{limit 5000}} {
    for {set i 0} {$i < $limit} {incr i} {
	clock scan [clock format $i -format %T] -format %T
    }
}} {*}$argv
