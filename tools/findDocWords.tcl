lassign $argv dir dictionary

set f [open $dictionary]
while {[gets $f line] > 0} {
    dict set realWord [string tolower $line] yes
}
close $f
puts "loaded [dict size $realWord] words from dictionary"

set files [glob -directory $dir {*.[13n]}]
set found {}

proc identifyWords {fragment filename} {
    global realWord found
    foreach frag [split [string map {\\fB "" \\fR "" \\fI "" \\fP "" \\0 _} $fragment] _] {
	if {[string is entier $frag]} continue
	set frag [string trim $frag "\\0123456789"]
	if {$frag eq ""} continue
	foreach word [regexp -all -inline {^[a-z]+|[A-Z][a-z]*} $frag] {
	    set word [string tolower $word]
	    if {![dict exists $realWord $word]} {
		dict lappend found $word $filename
	    }
	}
    }
}

foreach fn $files {
    set f [open $fn]
    foreach word [regexp -all -inline {[\\\w]+} [read $f]] {
	identifyWords $word $fn
    }
    close $f
}
set len [tcl::mathfunc::max {*}[lmap word [dict keys $found] {string length $word}]]
foreach word [lsort [dict keys $found]] {
    puts [format "%-${len}s: %s" $word [lindex [dict get $found $word] 0]]
}
