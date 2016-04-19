proc cat {file} {
	set f [open $file rb]
	while {[gets $f buf] >= 0} { puts $buf }
	close $f
}
fconfigure stdout -buffering full -translation binary 
foreach file $argv {
	cat $file
}
