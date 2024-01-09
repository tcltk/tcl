# regexpTestLib.tcl --
#
# This file contains tcl procedures used by spencer2testregexp.tcl and
# spencer2regexp.tcl, which are programs written to convert Henry
# Spencer's test suite to tcl test files.
#
# Copyright © 1996 Sun Microsystems, Inc.

proc readInputFile {} {
    global inFileName lineArray

    set fileId [open $inFileName r]
    try {
	set i 0
	while {[gets $fileId line] >= 0} {
	    if {[string index $line end] eq "\\"} {
		incr lineArray(c$i)
		append lineArray($i) [string range $line 0 end-1]
		continue
	    }
	    incr lineArray(c$i)
	    append lineArray($i) $line
	    incr i
	}
	return $i
    } finally {
	close $fileId
    }
}

#
# strings with embedded @'s are truncated
# unpreceded @'s are replaced by {}
#
proc removeAts {ls} {
    lmap item $ls {
	regsub @.* $item ""
    }
}

proc convertErrCode {code} {
    set errMsg "couldn't compile regular expression pattern:"
    switch $code {
	"INVARG" {
	    return "$errMsg invalid argument to regex routine"
	}
	"BADRPT" {
	    return "$errMsg ?+* follows nothing"
	}
	"BADBR" {
	    return "$errMsg invalid repetition count(s)"
	}
	"BADOPT" {
	    return "$errMsg invalid embedded option"
	}
	"EPAREN" {
	    return "$errMsg unmatched ()"
	}
	"EBRACE" {
	    return "$errMsg unmatched {}"
	}
	"EBRACK" {
	    return "$errMsg unmatched \[\]"
	}
	"ERANGE" {
	    return "$errMsg invalid character range"
	}
	"ECTYPE" {
	    return "$errMsg invalid character class"
	}
	"ECOLLATE" {
	    return "$errMsg invalid collating element"
	}
	"EESCAPE" {
	    return "$errMsg invalid escape sequence"
	}
	"BADPAT" {
	    return "$errMsg invalid regular expression"
	}
	"ESUBREG" {
	    return "$errMsg invalid backreference number"
	}
	"IMPOSS" {
	    return "$errMsg can never match"
	}
	default {
	    return "$errMsg $code"
	}
    }
}

proc writeOutputFile {numLines fcn} {
    global outFileName
    global lineArray

    # open output file and write file header info to it.

    set fileId [open $outFileName w]

    puts $fileId "# Commands covered:  $fcn"
    puts $fileId "#"
    puts $fileId "# This Tcl-generated file contains tests for the $fcn tcl command."
    puts $fileId "# Sourcing this file into Tcl runs the tests and generates output for"
    puts $fileId "# errors.  No output means no errors were found.  Setting VERBOSE to"
    puts $fileId "# -1 will run tests that are known to fail."
    puts $fileId "#"
    puts $fileId "# Copyright © 1998 Sun Microsystems, Inc."
    puts $fileId "#"
    puts $fileId "# See the file \"license.terms\" for information on usage and redistribution"
    puts $fileId "# of this file, and for a DISCLAIMER OF ALL WARRANTIES."
    puts $fileId "#"
    puts $fileId "\# SCCS: \%Z\% \%M\% \%I\% \%E\% \%U\%"
    puts $fileId "\nproc print \{arg\} \{puts \$arg\}\n"
    puts $fileId "if \{\[string compare test \[info procs test\]\] == 1\} \{"
    puts $fileId "    source defs ; set VERBOSE -1\n\}\n"
    puts $fileId "if \{\$VERBOSE != -1\} \{"
    puts $fileId "    proc print \{arg\} \{\}\n\}\n"
    puts $fileId "#"
    puts $fileId "# The remainder of this file is Tcl tests that have been"
    puts $fileId "# converted from Henry Spencer's regexp test suite."
    puts $fileId "#\n"

    set lineNum 0
    set srcLineNum 1
    while {$lineNum < $numLines} {
	set currentLine $lineArray($lineNum)

	# copy comment string to output file and continue

	if {[string match "#*" $currentLine]} {
	    puts $fileId $currentLine
	    incr srcLineNum $lineArray(c$lineNum)
	    incr lineNum
	    continue
	}

	set len [llength $currentLine]

	# copy empty string to output file and continue

	if {$len == 0} {
	    puts $fileId "\n"
	    incr srcLineNum $lineArray(c$lineNum)
	    incr lineNum
	    continue
	}
	if {$len < 3} {
	    puts "warning: test is too short --\n\t$currentLine"
	    incr srcLineNum $lineArray(c$lineNum)
	    incr lineNum
	    continue
	}

	puts $fileId [convertTestLine $currentLine $len $lineNum $srcLineNum]

	incr srcLineNum $lineArray(c$lineNum)
	incr lineNum
    }

    close $fileId
}

proc convertTestLine {currentLine len lineNum srcLineNum} {
    lassign [regsub -all {(?b)\\} $currentLine {\\\\}] re flags str

    # based on flags, decide whether to skip the test

    if {[findSkipFlag $flags]} {
	regsub -all {\[|\]|\(|\)|\{|\}|\#} $currentLine {\&} line
	set msg "\# skipping char mapping test from line $srcLineNum\n"
	append msg "print \{... skip test from line $srcLineNum:  $line\}"
	return $msg
    }

    # perform mapping if '=' flag exists

    set noBraces 0
    if {[regexp {=|>} $flags]} {
	set currentLine [string map {
	    _ {\\ }
	    A {\\x07}
	    B {\\b}
	    E {\\x1B}
	    F {\\f}
	    N {\\n}
	    T {\\t}
	    V {\\v}
	} $currentLine]

	# if any \r substitutions are made, do not wrap re, flags,
	# str, and result in braces
	set noBraces [regsub -all {R} $currentLine {\\\x0D} currentLine]

	if {[regexp {=} $flags]} {
	    set re [lindex $currentLine 0]
	}
	set str [lindex $currentLine 2]
    }
    set flags [removeFlags $flags]

    # find the test result

    set numVars [expr {$len - 3}]
    set vars {}
    set vals {}
    set result 0
    set v 0

    if {[regsub {\*} "$flags" "" newFlags]} {
	# an error is expected

	if {$str eq "EMPTY"} {
	    # empty regexp is not an error
	    # skip this test

	    return "\# skipping the empty-re test from line $srcLineNum\n"
	}
	set flags $newFlags
	set result "\{1 \{[convertErrCode $str]\}\}"
    } elseif {$numVars > 0} {
	# at least 1 match is made

	if {[regexp {s} $flags]} {
	    set result "\{0 1\}"
	} else {
	    while {$v < $numVars} {
		append vars " var($v)"
		append vals " \$var($v)"
		incr v
	    }
	    set tmp [removeAts [lrange $currentLine 3 $len]]
	    set result "\{0 \{1 $tmp\}\}"
	    if {$noBraces} {
		set result "\[subst $result\]"
	    }
	}
    } else {
	# no match is made

	set result "\{0 0\}"
    }

    # set up the test and write it to the output file

    set cmd [prepareCmd $flags $re $str $vars $noBraces]
    if {$cmd == -1} {
	return "\# skipping test with metasyntax from line $srcLineNum\n"
    }

    set test "test regexp-1.$srcLineNum \{converted from line $srcLineNum\} \{\n"
    append test "\tunset -nocomplain var\n"
    append test "\tlist \[catch \{\n"
    append test "\t\tset match \[$cmd\]\n"
    append test "\t\tlist \$match $vals\n"
    append test "\t\} msg\] \$msg\n"
    append test "\} $result\n"
    return $test
}
