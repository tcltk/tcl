# defs.tcl --
#
#	This file contains support code for the Tcl test suite.It is
#	It is normally sourced by the individual files in the test suite
#	before they run their tests.  This improved approach to testing
#	was designed and initially implemented by Mary Ann May-Pumphrey
#	of Sun Microsystems.
#
# Copyright (c) 1990-1994 The Regents of the University of California.
# Copyright (c) 1994-1996 Sun Microsystems, Inc.
# Copyright (c) 1998-1999 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: defs.tcl,v 1.1.2.1 1999/03/11 18:49:31 hershey Exp $

# Ensure that we have a minimal auto_path so we don't pick up extra junk.
set auto_path [list [info library]]

# create the "test" namespace for all testing variables and procedures
namespace eval test {
    foreach proc [list test cleanupTests dotests saveState restoreState \
	    normalizeMsg makeFile removeFile makeDirectory removeDirectory \
	    viewFile safeFetch bytestring set_iso8859_1_locale restore_locale \
	    setTmpDir] {
	namespace export $proc
    }

    # ::test::verbose defaults to "b"
    variable verbose "b"

    # matchingTests defaults to the empty list
    variable matchingTests {}

    # skippingTests defaults to the empty list
    variable skippingTests {}

    # Tests should not rely on the current working directory.
    # Files that are part of the test suite should be accessed relative to
    # ::test::testsDir.

    set originalDir [pwd]
    set tDir [file join $originalDir [file dirname [info script]]]
    cd $tDir
    variable testsDir [pwd]
    cd $originalDir

    # Tests should remove all files they create.  The test suite will
    # check tmpDir for files created by the tests.  ::test::filesMade
    # keeps track of such files created using the test::makeFile and
    # test::makeDirectory procedures.  ::test::filesExisted stores
    # the names of pre-existing files.

    variable filesMade {}
    variable filesExisted {}

    # initialize ::test::numTests array to keep track fo the number of
    # tests that pass, fial, and are skipped.
    array set numTests [list Total 0 Passed 0 Skipped 0 Failed 0]
    #array set originalEnv [array get env]

}

# If there is no "memory" command (because memory debugging isn't
# enabled), generate a dummy command that does nothing.

if {[info commands memory] == ""} {
    proc memory args {}
}

# ::test::setTmpDir --
#
#	Set the ::test::tmpDir to the specified value.  If the path
#	is relative, make it absolute.  If the file exists but is not
#	a dir, then return an error.  If the dir does not already
#	exist, create it.  If you cannot create it, then return an error.
#
# Arguments:
#	value	the new value of ::test::tmpDir
#
# Results:
#	::test::tmpDir is set to <value> and created if it didn't already
#	exist.  The working dir is changed to ::test::tmpDir.

proc ::test::setTmpDir {value} {

    set ::test::tmpDir $value

    if {[string compare [file pathtype $::test::tmpDir] absolute] != 0} {
	set ::test::tmpDir [file join [pwd] $::test::tmpDir]
    }
    if {[file exists $::test::tmpDir]} {
	if {![file isdir $::test::tmpDir]} {
	    puts stderr "Error:  bad argument \"$value\" to -tmpdir:"
	    puts stderr "            \"$::test::tmpDir\""
	    puts stderr "        is not a directory"
	    exit
	}
    } else {
	file mkdir $::test::tmpDir
    }

    # change the working dir to tmpDir and add the existing files in
    # tmpDir to the filesExisted list.
    cd $::test::tmpDir
    foreach file [glob -nocomplain [file join [pwd] *]] {
	lappend ::test::filesExisted $file
    }
}

# ::test::processCmdLineArgs --
#
#	Use command line args to set the tmpDir, verbose, skippingTests, and
#	matchingTests variables.
#
# Arguments:
#	none
#
# Results:
#	::test::verbose is set to <value>

proc ::test::processCmdLineArgs {} {
    global argv

    # The "argv" var doesn't exist in some cases, so use {}
    # The "argv" var doesn't exist in some cases.
    if {(![info exists argv]) || ([llength $argv] < 2)} {
	set flagArray {}
    } else {
	set flagArray $argv
    }

    if {[catch {array set flag $flagArray}]} {
	puts stderr "Error:  odd number of command line args specified:"
	puts stderr "        $argv"
	exit
    }
    
    # Allow for 1-char abbreviations, where applicable (e.g., -tmpdir == -t).
    foreach arg {-verbose -match -skip -constraints -tmpdir} {
	set abbrev [string range $arg 0 1]
	if {([info exists flag($abbrev)]) && \
		([lsearch $flagArray $arg] < [lsearch $flagArray $abbrev])} {
	    set flag($arg) $flag($abbrev)
	}
    }

    # Set ::test::tmpDir to the arg of the -tmpdir flag, if given.
    # ::test::tmpDir defaults to [pwd].
    # Save the names of files that already exist in ::test::tmpDir.
    if {[info exists flag(-tmpdir)]} {
	::test::setTmpDir $flag(-tmpdir)
    } else {
	set ::test::tmpDir [pwd]
    }
    foreach file [glob -nocomplain [file join $::test::tmpDir *]] {
	lappend ::test::filesExisted [file tail $file]
    }

    # Set ::test::verbose to the arg of the -verbose flag, if given
    if {[info exists flag(-verbose)]} {
	set ::test::verbose $flag(-verbose)
    }

    # Set ::test::matchingTests to the arg of the -match flag, if given
    if {[info exists flag(-match)]} {
	set ::test::matchingTests $flag(-match)
    }

    # Set ::test::skippingTests to the arg of the -skip flag, if given
    if {[info exists flag(-skip)]} {
	set ::test::skippingTests $flag(-skip)
    }

    # Use the -constraints flag, if given, so turn on the following
    # constraints:  notIfCompiled, knownBug, nonPortable
    if {[info exists flag(-constraints)]} {
	set constrList $flag(-constraints)
    } else {
	set constrList {}
    }
    foreach elt [list notIfCompiled knownBug nonPortable] {
	set ::test::testConfig($elt) [expr {[lsearch $constrList $elt] != -1}]
    }
    if {$::test::testConfig(nonPortable) == 0} {
	puts "(will skip non-portable tests)"
    }
}
test::processCmdLineArgs


# Check configuration information that will determine which tests
# to run.  To do this, create an array ::test::testConfig.  Each element
# has a 0 or 1 value, and the following elements are defined:
#	unixOnly -	1 means this is a UNIX platform, so it's OK
#			to run tests that only work under UNIX.
#	macOnly -	1 means this is a Mac platform, so it's OK
#			to run tests that only work on Macs.
#	pcOnly -	1 means this is a PC platform, so it's OK to
#			run tests that only work on PCs.
#	unixOrPc -	1 means this is a UNIX or PC platform.
#	macOrPc -	1 means this is a Mac or PC platform.
#	macOrUnix -	1 means this is a Mac or UNIX platform.
#	notIfCompiled -	1 means this that it is safe to run tests that
#                       might fail if the bytecode compiler is used. This
#                       element is set to 1 if the -allComp flag was used.
#                       Normally, this element is 0 so that tests that
#                       fail with the bytecode compiler are skipped.
#			As of 11/2/96 these are the history tests since
#			they depend on accurate source location information.
#			You can run these tests by using the -constraint
#			command line option with "knownBug" in the argument
#			list.
#       knownBug -      The test is known to fail and the bug is not yet
#                       fixed. The test will be run only if the flag
#                       -allBuggy is used (intended for Tcl dev. group
#                       internal use only).  You can run these tests by
#			using the -constraint command line option with
#			"knownBug" in the argument list.
#	nonPortable -	1 means this the tests are being running in
#			the master Tcl/Tk development environment;
#			Some tests are inherently non-portable because
#			they depend on things like word length, file system
#			configuration, window manager, etc.  These tests
#			are only run in the main Tcl development directory
#			where the configuration is well known.  You can
#			run these tests by using the -constraint command
#			line option with "nonPortable" in the argument list.
#	tempNotPc -	The inverse of pcOnly.  This flag is used to
#			temporarily disable a test.
#	tempNotMac -	The inverse of macOnly.  This flag is used to
#			temporarily disable a test.
#	nonBlockFiles - 1 means this platform supports setting files into
#			nonblocking mode.
#	asyncPipeClose- 1 means this platform supports async flush and
#			async close on a pipe.
#	unixExecs     - 1 means this machine has commands such as 'cat',
#			'echo' etc available.
#       hasIsoLocale  - 1 means the tests that need to switch to an iso
#                       locale can be run.
#

catch {unset ::test::testConfig}

# The following trace procedure makes it so that we can safely refer to
# non-existent members of the ::test::testConfig array without causing an
# error.  Instead, reading a non-existent member will return 0.  This is
# necessary because tests are allowed to use constraint "X" without ensuring
# that ::test::testConfig("X") is defined.

trace variable ::test::testConfig r ::test::safeFetch

proc ::test::safeFetch {n1 n2 op} {
    if {($n2 != {}) && ([info exists ::test::testConfig($n2)] == 0)} {
	set ::test::testConfig($n2) 0
    }
}

set ::test::testConfig(unixOnly) 	[expr {$tcl_platform(platform) == "unix"}]
set ::test::testConfig(macOnly) 	[expr {$tcl_platform(platform) == "macintosh"}]
set ::test::testConfig(pcOnly)		[expr {$tcl_platform(platform) == "windows"}]

set ::test::testConfig(unix)		$::test::testConfig(unixOnly)
set ::test::testConfig(mac)		$::test::testConfig(macOnly)
set ::test::testConfig(pc)		$::test::testConfig(pcOnly)

set ::test::testConfig(unixOrPc)	[expr {$::test::testConfig(unix) || $::test::testConfig(pc)}]
set ::test::testConfig(macOrPc)		[expr {$::test::testConfig(mac) || $::test::testConfig(pc)}]
set ::test::testConfig(macOrUnix)	[expr {$::test::testConfig(mac) || $::test::testConfig(unix)}]

set ::test::testConfig(nt)		[expr {$tcl_platform(os) == "Windows NT"}]
set ::test::testConfig(95)		[expr {$tcl_platform(os) == "Windows 95"}]
set ::test::testConfig(win32s)		[expr {$tcl_platform(os) == "Win32s"}]

# The following config switches are used to mark tests that should work,
# but have been temporarily disabled on certain platforms because they don't
# and we haven't gotten around to fixing the underlying problem.

set ::test::testConfig(tempNotPc) 	[expr {!$::test::testConfig(pc)}]
set ::test::testConfig(tempNotMac) 	[expr {!$::test::testConfig(mac)}]
set ::test::testConfig(tempNotUnix)	[expr {!$::test::testConfig(unix)}]

# The following config switches are used to mark tests that crash on
# certain platforms, so that they can be reactivated again when the
# underlying problem is fixed.

set ::test::testConfig(pcCrash) 	[expr {!$::test::testConfig(pc)}]
set ::test::testConfig(macCrash) 	[expr {!$::test::testConfig(mac)}]
set ::test::testConfig(unixCrash)	[expr {!$::test::testConfig(unix)}]

if {[catch {set f [open defs r]}]} {
    set ::test::testConfig(nonBlockFiles) 1
} else {
    if {[catch {fconfigure $f -blocking off}] == 0} {
	set ::test::testConfig(nonBlockFiles) 1
    } else {
	set ::test::testConfig(nonBlockFiles) 0
    }
    close $f
}

# If tests are being run as root, issue a warning message and set a
# variable to prevent some tests from running at all.

set user {}
if {$tcl_platform(platform) == "unix"} {
    catch {set user [exec whoami]}
    if {$user == ""} {
        catch {regexp {^[^(]*\(([^)]*)\)} [exec id] dummy user}
    }
    if {$user == ""} {set user root}
    if {$user == "root"} {
        puts stdout "Warning: you're executing as root.  I'll have to"
        puts stdout "skip some of the tests, since they'll fail as root."
	set ::test::testConfig(root) 1
    }
}

# Test for SCO Unix - cannot run async flushing tests because a potential
# problem with select is apparently interfering. (Mark Diekhans).

if {$tcl_platform(platform) == "unix"} {
    if {[catch {exec uname -X | fgrep {Release = 3.2v}}] == 0} {
	set ::test::testConfig(asyncPipeClose) 0
    } else {
	set ::test::testConfig(asyncPipeClose) 1
    }
} else {
    set ::test::testConfig(asyncPipeClose) 1
}

# Test to see if we have a broken version of sprintf with respect to the
# "e" format of floating-point numbers.

set ::test::testConfig(eformat) 1
if {[string compare "[format %g 5e-5]" "5e-05"] != 0} {
    set ::test::testConfig(eformat) 0
    puts stdout "(will skip tests that depend on the \"e\" format of floating-point numbers)"
}

# Test to see if execed commands such as cat, echo, rm and so forth are
# present on this machine.

set ::test::testConfig(unixExecs) 1
if {$tcl_platform(platform) == "macintosh"} {
    set ::test::testConfig(unixExecs) 0
}
if {($::test::testConfig(unixExecs) == 1) && ($tcl_platform(platform) == "windows")} {
    if {[catch {exec cat defs}] == 1} {
	set ::test::testConfig(unixExecs) 0
    }
    if {($::test::testConfig(unixExecs) == 1) && ([catch {exec echo hello}] == 1)} {
	set ::test::testConfig(unixExecs) 0
    }
    if {($::test::testConfig(unixExecs) == 1) && \
		([catch {exec sh -c echo hello}] == 1)} {
	set ::test::testConfig(unixExecs) 0
    }
    if {($::test::testConfig(unixExecs) == 1) && ([catch {exec wc defs}] == 1)} {
	set ::test::testConfig(unixExecs) 0
    }
    if {$::test::testConfig(unixExecs) == 1} {
	exec echo hello > removeMe
        if {[catch {exec rm removeMe}] == 1} {
	    set ::test::testConfig(unixExecs) 0
	}
    }
    if {($::test::testConfig(unixExecs) == 1) && ([catch {exec sleep 1}] == 1)} {
	set ::test::testConfig(unixExecs) 0
    }
    if {($::test::testConfig(unixExecs) == 1) && \
		([catch {exec fgrep unixExecs defs}] == 1)} {
	set ::test::testConfig(unixExecs) 0
    }
    if {($::test::testConfig(unixExecs) == 1) && ([catch {exec ps}] == 1)} {
	set ::test::testConfig(unixExecs) 0
    }
    if {($::test::testConfig(unixExecs) == 1) && \
		([catch {exec echo abc > removeMe}] == 0) && \
		([catch {exec chmod 644 removeMe}] == 1) && \
		([catch {exec rm removeMe}] == 0)} {
	set ::test::testConfig(unixExecs) 0
    } else {
	catch {exec rm -f removeMe}
    }
    if {($::test::testConfig(unixExecs) == 1) && \
		([catch {exec mkdir removeMe}] == 1)} {
	set ::test::testConfig(unixExecs) 0
    } else {
	catch {exec rm -r removeMe}
    }
    if {$::test::testConfig(unixExecs) == 0} {
	puts "(will skip tests that depend on Unix-style executables)"
    }
}

# ::test::cleanupTests --
#
# Print the number tests (total, passed, failed, and skipped) since the
# last time this procedure was invoked.
#
# Remove files and dirs created using the makeFile and makeDirectory
# commands since the last time this proc was invoked.
#
# Print the names of the files created without the makeFile command
# since the last time this proc was invoked.
#

proc ::test::cleanupTests {} {
    # print stats
    puts -nonewline stdout "[file tail [info script]]:"
    foreach index [list "Total" "Passed" "Skipped" "Failed"] {
	puts -nonewline stdout "\t$index\t$::test::numTests($index)"
    }
    puts stdout ""

    # remove files and directories created by the tests
    foreach file $::test::filesMade {
	if {[file exists $file]} {
	    catch {file delete -force $file}
	}
    }

    # report the names of files in ::test::tmpDir that were not pre-existing.
    set currentFiles {}
    foreach file [glob -nocomplain [file join $::test::tmpDir *]] {
	lappend currentFiles [file tail $file]
    }
    set filesNew {}
    foreach file $currentFiles {
	if {[lsearch $::test::filesExisted $file] == -1} {
	    lappend filesNew $file
	}
    }
    if {[llength $filesNew] > 0} {
	puts stdout "\t\tFiles created:\t$filesNew"
    }

    # reset filesMade, filesExisted, and numTests
    foreach index [list "Total" "Passed" "Skipped" "Failed"] {
	set ::test::numTests($index) 0
    }
    set ::test::filesMade {}
    set ::test::filesExisted $currentFiles
}


# test --
#
# This procedure runs a test and prints an error message if the test fails.
# If ::test::verbose has been set, it also prints a message even if the
# test succeeds.  The test will be skipped if it doesn't match the
# ::test::matchingTests variable, if it matches an element in
# ::test::skippingTests, or if one of the elements of "constraints" turns
# out not to be true.
#
# Arguments:
# name -		Name of test, in the form foo-1.2.
# description -		Short textual description of the test, to
#			help humans understand what it does.
# constraints -		A list of one or more keywords, each of
#			which must be the name of an element in
#			the array "::test::testConfig".  If any of these
#			elements is zero, the test is skipped.
#			This argument may be omitted.
# script -		Script to run to carry out the test.  It must
#			return a result that can be checked for
#			correctness.
# expectedAnswer -	Expected result from script.

proc ::test::test {name description script expectedAnswer args} {
    incr ::test::numTests(Total)

    # skip the test if it's name matches an element of skippingTests
    foreach pattern $::test::skippingTests {
	if {[string match $pattern $name]} {
	    incr ::test::numTests(Skipped)
	    return
	}
    }
    # skip the test if it's name doesn't match any element of matchingTests
    if {[llength $::test::matchingTests] > 0} {
	set ok 0
	foreach pattern $::test::matchingTests {
	    if {[string match $pattern $name]} {
		set ok 1
		break
	    }
        }
	if {!$ok} {
	    incr ::test::numTests(Skipped)
	    return
	}
    }
    set i [llength $args]
    if {$i == 0} {
	set constraints {}
    } elseif {$i == 1} {
	# "constraints" argument exists;  shuffle arguments down, then
	# make sure that the constraints are satisfied.

	set constraints $script
	set script $expectedAnswer
	set expectedAnswer [lindex $args 0]
	set doTest 0
	if {[string match {*[$\[]*} $constraints] != 0} {
	    # full expression, e.g. {$foo > [info tclversion]}

	    catch {set doTest [uplevel #0 expr $constraints]}
	} elseif {[regexp {[^.a-zA-Z0-9 ]+} $constraints] != 0} {
	    # something like {a || b} should be turned into 
	    # $::test::testConfig(a) || $::test::testConfig(b).

 	    regsub -all {[.a-zA-Z0-9]+} $constraints {$::test::testConfig(&)} c
	    catch {set doTest [eval expr $c]}
	} else {
	    # just simple constraints such as {unixOnly fonts}.

	    set doTest 1
	    foreach constraint $constraints {
		if {![info exists ::test::testConfig($constraint)]
			|| !$::test::testConfig($constraint)} {
		    set doTest 0
		    break
		}
	    }
	}
	if {$doTest == 0} {
	    incr ::test::numTests(Skipped)
	    if {[string first s $::test::verbose] != -1} {
		puts stdout "++++ $name SKIPPED: $constraints"
	    }
	    return	
	}
    } else {
	error "wrong # args: must be \"test name description ?constraints? script expectedAnswer\""
    }
    memory tag $name
    set code [catch {uplevel $script} actualAnswer]
    if {$code != 0 || [string compare $actualAnswer $expectedAnswer] != 0} {
	incr ::test::numTests(Failed)
	if {[string first b $::test::verbose] == -1} {
	    set script ""
	}
	puts stdout "\n==== $name $description FAILED"
	if {$script != ""} {
	    puts stdout "==== Contents of test case:"
	    puts stdout $script
	}
	if {$code != 0} {
	    if {$code == 1} {
		puts stdout "==== Test generated error:"
		puts stdout $actualAnswer
	    } elseif {$code == 2} {
		puts stdout "==== Test generated return exception;  result was:"
		puts stdout $actualAnswer
	    } elseif {$code == 3} {
		puts stdout "==== Test generated break exception"
	    } elseif {$code == 4} {
		puts stdout "==== Test generated continue exception"
	    } else {
		puts stdout "==== Test generated exception $code;  message was:"
		puts stdout $actualAnswer
	    }
	} else {
	    puts stdout "---- Result was:\n$actualAnswer"
	}
	puts stdout "---- Result should have been:\n$expectedAnswer"
	puts stdout "==== $name FAILED\n" 
    } else { 
	incr ::test::numTests(Passed)
	if {[string first p $::test::verbose] != -1} {
	    puts stdout "++++ $name PASSED"
	}
    }
}

proc ::test::dotests {file args} {
    set savedTests $::test::matchingTests
    set ::test::matchingTests $args
    source $file
    set ::test::matchingTests $savedTests
}

proc ::test::openfiles {} {
    if {[catch {testchannel open} result]} {
	return {}
    }
    return $result
}

proc ::test::leakfiles {old} {
    if {[catch {testchannel open} new]} {
        return {}
    }
    set leak {}
    foreach p $new {
    	if {[lsearch $old $p] < 0} {
	    lappend leak $p
	}
    }
    return $leak
}

set ::test::saveState {}

proc ::test::saveState {} {
    uplevel #0 {set ::test::saveState [list [info procs] [info vars]]}
}

proc ::test::restoreState {} {
    foreach p [info procs] {
	if {[lsearch [lindex $::test::saveState 0] $p] < 0} {
	    rename $p {}
	}
    }
    foreach p [uplevel #0 {info vars}] {
	if {[lsearch [lindex $::test::saveState 1] $p] < 0} {
	    uplevel #0 "unset $p"
	}
    }
}

proc ::test::normalizeMsg {msg} {
    regsub "\n$" [string tolower $msg] "" msg
    regsub -all "\n\n" $msg "\n" msg
    regsub -all "\n\}" $msg "\}" msg
    return $msg
}

# makeFile --
#
# Create a new file with the name <name>, and write <contents> to it.
#
# If this file hasn't been created via makeFile since the last time
# cleanupTests was called, add it to the $filesMade list, so it will
# be removed by the next call to cleanupTests.
#
proc ::test::makeFile {contents name} {
    set fd [open $name w]
    fconfigure $fd -translation lf
    if {[string index $contents [expr {[string length $contents] - 1}]] == "\n"} {
	puts -nonewline $fd $contents
    } else {
	puts $fd $contents
    }
    close $fd

    set fullName [file join [pwd] $name]
    if {[lsearch $::test::filesMade $fullName] == -1} {
	lappend ::test::filesMade $fullName
    }
}

proc ::test::removeFile {name} {
    file delete $name
}

# makeDirectory --
#
# Create a new dir with the name <name>.
#
# If this dir hasn't been created via makeDirectory since the last time
# cleanupTests was called, add it to the $directoriesMade list, so it will
# be removed by the next call to cleanupTests.
#
proc ::test::makeDirectory {name} {
    file mkdir $name

    set fullName [file join [pwd] $name]
    if {[lsearch $::test::filesMade $fullName] == -1} {
	lappend ::test::filesMade $fullName
    }
}

proc ::test::removeDirectory {name} {
    file delete -force $name
}

proc ::test::viewFile {name} {
    global tcl_platform
    if {($tcl_platform(platform) == "macintosh") || \
		($::test::testConfig(unixExecs) == 0)} {
	set f [open $name]
	set data [read -nonewline $f]
	close $f
	return $data
    } else {
	exec cat $name
    }
}

#
# Construct a string that consists of the requested sequence of bytes,
# as opposed to a string of properly formed UTF-8 characters.  
# This allows the tester to 
# 1. Create denormalized or improperly formed strings to pass to C procedures 
#    that are supposed to accept strings with embedded NULL bytes.
# 2. Confirm that a string result has a certain pattern of bytes, for instance
#    to confirm that "\xe0\0" in a Tcl script is stored internally in 
#    UTF-8 as the sequence of bytes "\xc3\xa0\xc0\x80".
#
# Generally, it's a bad idea to examine the bytes in a Tcl string or to
# construct improperly formed strings in this manner, because it involves
# exposing that Tcl uses UTF-8 internally.

proc ::test::bytestring {string} {
    encoding convertfrom identity $string
}

# Locate tcltest executable

set tcltest [info nameofexecutable]

if {$tcltest == "{}"} {
    set tcltest {}
    puts stdout "Unable to find tcltest executable, multiple process tests will fail."
}

set ::test::testConfig(stdio) 0
if {$tcl_platform(os) != "Win32s"} {
    # Don't even try running another copy of tcltest under win32s, or you 
    # get an error dialog about multiple instances.

    catch {
	file delete -force tmp
	set f [open tmp w]
	puts $f {
	    exit
	}
	close $f
	set f [open "|[list $tcltest tmp]" r]
	close $f
	set ::test::testConfig(stdio) 1
    }
    catch {file delete -force tmp}
}

if {($tcl_platform(platform) == "windows") && ($::test::testConfig(stdio) == 0)} {
    puts stdout "(will skip tests that redirect stdio of exec'd 32-bit applications)"
}

catch {socket} msg
set ::test::testConfig(socket) [expr {$msg != "sockets are not available on this system"}]

if {$::test::testConfig(socket) == 0} {
    puts stdout "(will skip tests that use sockets)"
}

#
# Internationalization / ISO support procs     -- dl
#
if {[info commands testlocale]==""} {
    # No testlocale command, no tests...
    # (it could be that we are a sub interp and we could just load
    # the Tcltest package but that would interfere with tests
    # that tests packages/loading in slaves...)
    set ::test::testConfig(hasIsoLocale) 0
} else {
    proc ::test::set_iso8859_1_locale {} {
	set ::test::previousLocale [testlocale ctype]
	testlocale ctype $::test::isoLocale
    }

    proc ::test::restore_locale {} {
	testlocale ctype $::test::previousLocale
    }

    if {![info exists ::test::isoLocale]} {
	set ::test::isoLocale fr
        switch $tcl_platform(platform) {
	    "unix" {
		# Try some 'known' values for some platforms:
		switch -exact -- $tcl_platform(os) {
		    "FreeBSD" {
			set ::test::isoLocale fr_FR.ISO_8859-1
		    }
		    HP-UX {
			set ::test::isoLocale fr_FR.iso88591
		    }
		    Linux -
		    IRIX {
			set ::test::isoLocale fr
		    }
		    default {
			# Works on SunOS 4 and Solaris, and maybe others...
			# define it to something else on your system
			#if you want to test those.
			set ::test::isoLocale iso_8859_1
		    }
		}
	    }
	    "windows" {
		set ::test::isoLocale French
	    }
	}
    }

    set ::test::testConfig(hasIsoLocale) \
	    [string length [::test::set_iso8859_1_locale]]
    ::test::restore_locale

    if {$::test::testConfig(hasIsoLocale) == 0} {
	puts "(will skip tests that need to set an iso8859-1 locale)"
    }

} 

# Need to catch the import because it fails if defs.tcl is sourced
# more than once.
catch {namespace import ::test::*}
