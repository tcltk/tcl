#! /usr/bin/env tclsh

# Don't overwrite tcltests facilities already present
if {[package provide tcltests] ne {}} return

package require tcltest 2.5
namespace import ::tcltest::*
testConstraint exec [llength [info commands exec]]
testConstraint deprecated [expr {![tcl::build-info no-deprecate]}]
testConstraint debug [tcl::build-info debug]
testConstraint purify [tcl::build-info purify]
testConstraint debugpurify [
    expr {
	![tcl::build-info memdebug]
	&& [testConstraint debug]
	&& [testConstraint purify]
    }]
testConstraint fcopy         [llength [info commands fcopy]]
testConstraint fileevent     [llength [info commands fileevent]]
testConstraint thread        [expr {![catch {package require Thread 2.7-}]}]
testConstraint notValgrind   [expr {![testConstraint valgrind]}]


namespace eval ::tcltests {


    proc init {} {
	if {[namespace which ::tcl::file::tempdir] eq {}} {
	    interp alias {} [namespace current]::tempdir {} [
		namespace current]::tempdir_alternate
	} else {
	    interp alias {} [namespace current]::tempdir {} ::tcl::file::tempdir
	}
    }


    # Stolen from dict.test
    proc scriptmemcheck script {
	set end [lindex [split [memory info] \n] 3 3]
	for {set i 0} {$i < 5} {incr i} {
	    uplevel 1 $script
	    set tmp $end
	    set end [lindex [split [memory info] \n] 3 3]
	}
	expr {$end - $tmp}
    }


    proc tempdir_alternate {} {
	close [file tempfile tempfile]
	set tmpdir [file dirname $tempfile]
	set execname [info nameofexecutable]
	regsub -all {[^[:alpha:][:digit:]]} $execname _ execname
	for {set i 0} {$i < 10000} {incr i} {
	    set time [clock milliseconds]
	    set name $tmpdir/${execname}_${time}_$i
	    if {![file exists $name]} {
		file mkdir $name
		return $name
	    }
	}
	error [list {could not create temporary directory}]
    }

    init

    package provide tcltests 0.1
}

