# spencer2testregexp.tcl --
#
# This file generates a test suite for the testregexp command based on
# Henry Spencer's test suite.  This script must be run in tcl 8.1 or
# higher and takes an output filename as its parameter.
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
# SCCS: @(#) spencer2testregexp.tcl 1.4 98/01/22 14:48:14
# 

source ../tools/regexpTestLib.tcl

#
# remove flags that have no meaning
#
proc removeFlags {flags} {
    regsub -all {\[|[A-Z]|[0-9]} $flags "" newFlags 
    return $newFlags
}

#
# code for mapping char is not yet written, skip = and >
# NOTBOL flag is already tested and can't be tested from the command line, skip ^
# L is locale dependant, skip L
#
proc findSkipFlag {flags} {
    if {[regexp {\+} $flags] == 1} {
	return 1
    }
    return 0
}

proc prepareCmd {flags re str vars noBraces} {
    if {$noBraces > 0} {
	set cmd "testregexp $flags $re $str $vars"
    } else {
	set cmd "testregexp [list $flags] [list $re] [list $str] $vars"
    }
    return $cmd
}

# main

if {$argc != 2} {
    puts "name of input and outfile reuqired"
    return
}

set inFileName [lindex $argv 0]
set outFileName [lindex $argv 1]

writeOutputFile [readInputFile] testregexp
