apply {{{limit1 100} {limit2 1000}} {
    for {set i 0} {$i < $limit1} {incr i} {
	apply {limit2 {
	    set a {}
	    set b {}
	    for {set i 0} {$i < $limit2} {incr i} {
		lappend a $i
		dict set b $i [expr {$i*$i}]
	    }
	    return [string length $a],[string length $b]
	}} $limit2
    }
}} {*}$argv
