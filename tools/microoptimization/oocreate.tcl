oo::class create foo {
    method bar {} {
	return abc
    }
}
apply {{{iter 10000}} {
    for {set i 0} {$i < $iter} {incr i} {
	set obj1 [foo new]
	set obj2 [foo create inst]
	$obj1 destroy
	$obj2 destroy
    }
}} {*}$argv
