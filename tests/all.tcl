# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: all.tcl,v 1.1.2.3 1999/03/12 19:51:30 hershey Exp $

if {[lsearch ::test [namespace children]] == -1} {
    source [file join [pwd] [file dirname [info script]] defs.tcl]
}
set ::test::testSingleFile false

puts stdout "Tcl 8.1 tests running in interp:  [info nameofexecutable]"
puts stdout "Tests running in working dir:  $::test::tmpDir"
if {[llength $::test::skippingTests] > 0} {
    puts stdout "Skipping tests that match:  $::test::skippingTests"
}
if {[llength $::test::matchingTests] > 0} {
    puts stdout "Only running tests that match:  $::test::matchingTests"
}

# Use command line specified glob pattern (specified by -file or -f)
# if one exists.  Otherwise use *.test (or *.tes on win32s).  If given,
# the file pattern should be specified relative to the dir containing
# this file.  If no files are found to match the pattern, print an
# error message and exit.
set fileIndex [expr {[lsearch $argv "-file"] + 1}]
set fIndex [expr {[lsearch $argv "-f"] + 1}]
if {($fileIndex < 1) || ($fIndex > $fileIndex)} {
    set fileIndex $fIndex
}
if {$fileIndex > 0} {
    set globPattern [file join $::test::testsDir [lindex $argv $fileIndex]]
    puts stdout "Sourcing files that match:  $globPattern"
} elseif {$tcl_platform(os) == "Win32s"} {
    set globPattern [file join $::test::testsDir *.tes]
} else {
    set globPattern [file join $::test::testsDir *.test]
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
::test::cleanupTests 1
return
