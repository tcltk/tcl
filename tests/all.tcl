# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: all.tcl,v 1.1.2.6 1999/04/07 01:59:27 hershey Exp $

if {[lsearch ::tcltest [namespace children]] == -1} {
    source [file join [pwd] [file dirname [info script]] defs.tcl]
}
set ::tcltest::testSingleFile false

puts stdout "Tcl $tcl_patchLevel tests running in interp:  [info nameofexecutable]"
puts stdout "Tests running in working dir:  $::tcltest::workingDir"
if {[llength $::tcltest::skip] > 0} {
    puts stdout "Skipping tests that match:  $::tcltest::skip"
}
if {[llength $::tcltest::match] > 0} {
    puts stdout "Only running tests that match:  $::tcltest::match"
}

# Use command line specified glob pattern (specified by -file or -f)
# if one exists.  Otherwise use *.test.  If given, the file pattern
# should be specified relative to the dir containing this file.  If no
# files are found to match the pattern, print an error message and exit.
set fileIndex [expr {[lsearch $argv "-file"] + 1}]
set fIndex [expr {[lsearch $argv "-f"] + 1}]
if {($fileIndex < 1) || ($fIndex > $fileIndex)} {
    set fileIndex $fIndex
}
if {$fileIndex > 0} {
    set globPattern [file join $::tcltest::testsDir [lindex $argv $fileIndex]]
    puts stdout "Sourcing files that match:  $globPattern"
} else {
    set globPattern [file join $::tcltest::testsDir *.test]
}
set fileList [glob -nocomplain $globPattern]
if {[llength $fileList] < 1} {
    puts "Error: no files found matching $globPattern"
    exit
}
set timeCmd {clock format [clock seconds]}
puts stdout "Tests began at [eval $timeCmd]"

# source each of the specified tests
foreach file [lsort $fileList] {
    set tail [file tail $file]
    if {[string match l.*.test $tail]} {
	# This is an SCCS lockfile; ignore it
	continue
    }
    puts stdout $tail
    if {[catch {source $file} msg]} {
	puts stdout $msg
    }
}

# cleanup
puts stdout "\nTests ended at [eval $timeCmd]"
::tcltest::cleanupTests 1
return











