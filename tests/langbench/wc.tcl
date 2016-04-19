proc wordsplit {str} {
	set list {}
	set word {}
	foreach char [split $str {}] {
		if {[string is space $char]} {
			if {[string length $word] > 0} {
				lappend list $word
			}
			set word {}
		} else {
			append word $char
		}
	}
	if {[string length $word] > 0} {
		lappend list $word
	}
	return $list
}

proc doit {file} {
	set f [open $file r]
	fconfigure $f -translation binary
	set buf ""
	set n 0
	while {[gets $f buf] >= 0} {
		set words [wordsplit $buf]
		incr n [llength $words]
	}
	close $f
	return $n
}
set total 0
foreach file $argv {
	incr total [doit $file]
}
puts $total
