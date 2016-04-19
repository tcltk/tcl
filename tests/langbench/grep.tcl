proc grep {file} {
	set f [open $file rb]
	set buf ""
	while {[gets $f buf] >= 0} {
		if {[regexp -- {[^A-Za-z]fopen\(.*\)} $buf]} { puts $buf }
	}
	close $f
}
fconfigure stdout -translation binary 
foreach file $argv {
	grep $file
}
