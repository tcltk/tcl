# spencer2regexp.tcl --
#
# This file generates a test suite for the regexp command based on
# Henry Spencer's test suite.  This script must be run in tcl 8.1 or
# higher and takes an output filename as its parameter.
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
# SCCS: @(#) spencer2regexp.tcl 1.4 98/01/22 14:48:09
# 

source ../tools/regexpTestLib.tcl

#
# remove flags that have no meaning
#
proc removeFlags {flags} {
    regsub -all {a|s|&|-|/|\+|,|\$|;|:|>|=|\.|\[|[A-Z]|[0-9]} $flags "" newFlags 
    return $newFlags
}

#
# NOTBOL flag is already tested and can't be tested from the command line, skip ^
# L is locale dependant, skip L
# $ flag has no (?$) meaning
# b&a and q&e are bad flag combinations
#
proc findSkipFlag {flags} {
    if {[string first "^" $flags] != -1} {
	return 1
    }
    if {([string first "q" $flags] != -1) 
	&& ([string first "e" $flags] != -1)} {
	return 1
    }
    if {([string first "a" $flags] != -1) 
	&& ([string first "b" $flags] != -1)} {
	return 1
    }
    if {[regexp {\$|\+} $flags] == 1} {
	return 1
    }
    return 0
}

proc prepareCmd {flags re str vars noBraces} {
    # adjust the re to include to compile-time flag, where applicable
    
    if {[llength $flags] != 0} {
	
	# if re already has meta-syntax, skip this test
	
	if {[regexp {^(\(\?)|^(\*\*\*)} $re] == 1} {
	    return -1
	}
	set re "\(?$flags\)$re"
    }
    if {$noBraces > 0} {
	set cmd "regexp -- $re $str $vars"
    } else {
	set cmd "regexp -- [list $re] [list $str] $vars"
    }
    return $cmd
}

proc convertTestLineJunk {currentLine len lineNum srcLineNum} {

    regsub -all {(?b)\\} $currentLine {\\\\} currentLine
    set re [lindex $currentLine 0]
    set flags [lindex $currentLine 1]
    set str [lindex $currentLine 2]

    # based on flags, decide whether to skip the test

    if {[findSkipFlag $flags]} {
	regsub -all {\[|\]|\(|\)|\{|\}|\#} $currentLine {\&} line
	set msg "\# skipping char mapping test from line $srcLineNum\n"
	append msg "print \{... skip test from line $srcLineNum:  $line\}"
	return $msg
    }

    # perform mapping if '=' flag exists

    if {[regexp {=|>} $flags] == 1} {
	regsub -all {_} $currentLine {\\ } currentLine
	regsub -all {A} $currentLine {\\007} currentLine
	regsub -all {B} $currentLine {\\b} currentLine
	regsub -all {E} $currentLine {\\033} currentLine
	regsub -all {F} $currentLine {\\f} currentLine
	regsub -all {N} $currentLine {\\n} currentLine
	regsub -all {R} $currentLine {\\r} currentLine
	regsub -all {T} $currentLine {\\t} currentLine
	regsub -all {V} $currentLine {\\v} currentLine
	if {[regexp {=} $flags] == 1} {
	    set re [lindex $currentLine 0]
	}
	set str [lindex $currentLine 2]
    }
    set flags [removeFlags $flags]

    # find the test result

    set numVars [expr $len - 3]
    set vars {}
    set vals {}
    set result 0
    set v 0
    
    if {[regsub {\*} "$flags" "" newFlags] == 1} {
	# an error is expected
	
	if {[string compare $str "EMPTY"] == 0} {
	    # empty regexp is not an error
	    # skip this test
	    
	    return "\# skipping the empty-re test from line $srcLineNum\n"
	}
	set flags $newFlags
	set result "1 \{[convertErrCode $str]\}"
    } elseif {$numVars > 0} {
	# at least 1 match is made
	
	if {[regexp {s} $flags] == 1} {
	    set result {0 1}
	} else {
	    while {$v < $numVars} {
		append vars " var($v)"
		append vals " \$var($v)"
		incr v
	    }
	    set result "0 \{1 [removeAts [lrange $currentLine 3 $len]]\}"
	}
    } else {
	# no match is made
	
	set result "0 0"
    }

    # adjust the re to include to compile-time flag, where applicable
    
    if {[llength $flags] != 0} {

	# if re already has meta-syntax, skip this test

	if {[regexp {^(\(\?)|^(\*\*\*)} $re] == 1} {
	    return "\# skipping test with metasyntax from line $srcLineNum\n"	    
	}
	set re "\(?$flags\)$re"
    }

    # set up the test and write it to the output file
    
    set cmd "regexp -- [list $re] [list $str] $vars"
    set test "test regexp-1.$srcLineNum \{converted from line $srcLineNum\} \{\n"
    append test "\tcatch {unset var}\n"
    append test "\tlist \[catch \{ \n"
    append test "\t\tset match \[$cmd\] \n"
    append test "\t\tlist \$match $vals \n"
    append test "\t\} msg\] \$msg \n"
    append test "\} \{$result\} \n"
    return $test
} 

# main

if {$argc != 2} {
    puts "name of input and outfile reuqired"
    return
}

set inFileName [lindex $argv 0]
set outFileName [lindex $argv 1]

writeOutputFile [readInputFile] regexp
