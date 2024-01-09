# findBadExternals.tcl --
#
#	This script scans the Tcl load library for exported symbols
#	that do not begin with 'Tcl' or 'tcl'.  It reports them on the
#	standard output.  It is used to make sure that the library does
#	not inadvertently export externals that may be in conflict with
#	other code.
#
# Usage:
#
#	tclsh findBadExternals.tcl /path/to/tclXX.so-or-.dll
#
# Copyright © 2005 George Peter Staplin and Kevin Kenny
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#----------------------------------------------------------------------

proc main {argv} {
    if {[llength $argv] != 1} {
	puts stderr "syntax is: [info script] libtcl"
	return 1
    }

    try {
	switch -exact -- $::tcl_platform(platform) {
	    unix - macosx {
		exec nm --extern-only --defined-only [lindex $argv 0]
	    }
	    windows {
		exec dumpbin /exports [lindex $argv 0]
	    }
	}
    } trap NONE {} {
	# Nothing to do
    } on error msg {
	puts stderr $msg
	return 1
    } on ok result {
	foreach line [split $result \n] {
	    if {! [string match {* [Tt]cl*} $line]} {
		puts $line
	    }
	}
    }

    return 0
}
exit [main $::argv]
