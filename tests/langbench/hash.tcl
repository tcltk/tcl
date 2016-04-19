proc main {} {
	global	argv

	set d [dict create]
	foreach file $argv {
		set f [open $file rb]
		while {[gets $f buf] >= 0} {
			dict set d $buf 1
		}
		close $f
	}
}
main
set f [open "/proc/[pid]/status"]
while {[gets $f buf] >= 0} {
	if {[regexp {^Vm[RD]} $buf]} { puts $buf }
}
