proc a {val} { return [b $val] }
proc b {val} { return [c $val] }
proc c {val} { return [d $val] }
proc d {val} { return [e $val] }
proc e {val} { return [f $val] }
proc f {val} { return [g $val 2] }
proc g {v1 v2} { return [h $v1 $v2 3] }
proc h {v1 v2 v3} { return [i $v1 $v2 $v3 4] }
proc i {v1 v2 v3 v4} { return [j $v1 $v2 $v3 $v4 5] }
proc j {v1 v2 v3 v4 v5} { return [expr $v1 + $v2 + $v3 + $v4 + $v5] }
proc main {} {
	set n 100000
	while {$n > 0} { set x [a $n]; incr n -1 }
	puts $x
}
main
