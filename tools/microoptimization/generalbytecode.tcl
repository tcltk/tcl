apply {{} {
    for {set i 0} {$i < 100} {incr i} {
	apply {{} {
	    set a {}
	    set b {}
	    for {set i 0} {$i < 1000} {incr i} {
		lappend a $i
		dict set b $i [expr {$i*$i}]
	    }
	    return [string length $a],[string length $b]
	}}
    }
}}

