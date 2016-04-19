proc main {} {
	global	argv

	foreach file $argv {
		set f [open $file rb]

		# Takes 2.7 seconds/12.3
		while {[gets $f buf] >= 0} {
			lappend l $buf
		}
		close $f
	}

	# takes 7.9 seconds/12.3
	foreach buf [lsort $l] {
		puts $buf
	}
}
fconfigure stdout -buffering full -translation binary
main
