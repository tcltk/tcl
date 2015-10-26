oo::class create foo {
    method bar {} {
	return abc
    }
}
foo create inst
apply {{{iter 100000}} {
    for {set i 0} {$i < $iter} {incr i} {
	inst bar
    }
}} {*}$argv
