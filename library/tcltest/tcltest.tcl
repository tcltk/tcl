# tcltest.tcl --
#
#	This file contains support code for the Tcl test suite.  It
#       defines the tcltest namespace and finds and defines the output
#       directory, constraints available, output and error channels,
#	etc. used by Tcl tests.  See the tcltest man page for more
#	details.
#
#       This design was based on the Tcl testing approach designed and
#       initially implemented by Mary Ann May-Pumphrey of Sun
#	Microsystems.
#
# Copyright (c) 1994-1997 Sun Microsystems, Inc.
# Copyright (c) 1998-1999 by Scriptics Corporation.
# Copyright (c) 2000 by Ajuba Solutions
# All rights reserved.
#
# RCS: @(#) $Id: tcltest.tcl,v 1.33.8.1 2002/06/10 05:33:14 wolfsuit Exp $

# create the "tcltest" namespace for all testing variables and
# procedures

package require Tcl 8.3

namespace eval tcltest {

    # Export the public tcltest procs
    namespace export bytestring cleanupTests customMatch debug errorChannel \
	    errorFile interpreter limitConstraints loadFile loadScript \
	    loadTestedCommands makeDirectory makeFile match \
	    matchDirectories matchFiles normalizeMsg normalizePath \
	    outputChannel outputFile preserveCore removeDirectory \
	    removeFile runAllTests singleProcess skip skipDirectories \
	    skipFiles temporaryDirectory test testConstraint \
	    testsDirectory verbose viewFile workingDirectory
    # Export the tcltest 1 compatibility procs
    namespace export getMatchingFiles mainThread restoreState saveState \
	    threadReap

    proc Default {varName value} {
	variable $varName
	if {![info exists $varName]} {
	    variable $varName $value
	}
    }
    proc ArrayDefault {varName value} {
	variable $varName
	if {[array exists $varName]} {
	    return
	}
	if {[info exists $varName]} {
	    # Pre-initialized value is a scalar: destroy it!
	    unset $varName
	}
	array set $varName $value
    }

    # verbose defaults to {body}
    Default verbose body

    # Match and skip patterns default to the empty list, except for
    # matchFiles, which defaults to all .test files in the
    # testsDirectory and matchDirectories, which defaults to all
    # directories.
    Default match {}
    Default skip {}
    Default matchFiles {*.test}
    Default skipFiles {}
    Default matchDirectories {*}
    Default skipDirectories {}

    # By default, don't save core files
    Default preserveCore 0

    # output goes to stdout by default
    Default outputChannel stdout
    Default outputFile stdout

    # errors go to stderr by default
    Default errorChannel stderr
    Default errorFile stderr

    # debug output doesn't get printed by default; debug level 1 spits
    # up only the tests that were skipped because they didn't match or
    # were specifically skipped.  A debug level of 2 would spit up the
    # tcltest variables and flags provided; a debug level of 3 causes
    # some additional output regarding operations of the test harness.
    # The tcltest package currently implements only up to debug level 3.
    Default debug 0

    # Save any arguments that we might want to pass through to other
    # programs.  This is used by the -args flag.
    Default parameters {}

    # Count the number of files tested (0 if runAllTests wasn't called).
    # runAllTests will set testSingleFile to false, so stats will
    # not be printed until runAllTests calls the cleanupTests proc.
    # The currentFailure var stores the boolean value of whether the
    # current test file has had any failures.  The failFiles list
    # stores the names of test files that had failures.
    Default numTestFiles 0
    Default testSingleFile true
    Default currentFailure false
    Default failFiles {}

    # Tests should remove all files they create.  The test suite will
    # check the current working dir for files created by the tests.
    # filesMade keeps track of such files created using the makeFile and
    # makeDirectory procedures.  filesExisted stores the names of
    # pre-existing files.
    Default filesMade {}
    Default filesExisted {}

    # numTests will store test files as indices and the list of files
    # (that should not have been) left behind by the test files.
    ArrayDefault createdNewFiles {}

    # initialize numTests array to keep track fo the number of tests
    # that pass, fail, and are skipped.
    ArrayDefault numTests [list Total 0 Passed 0 Skipped 0 Failed 0]

    # initialize skippedBecause array to keep track of constraints that
    # kept tests from running; a constraint name of "userSpecifiedSkip"
    # means that the test appeared on the list of tests that matched the
    # -skip value given to the flag; "userSpecifiedNonMatch" means that
    # the test didn't match the argument given to the -match flag; both
    # of these constraints are counted only if tcltest::debug is set to
    # true.
    ArrayDefault skippedBecause {}

    # initialize the testConstraints array to keep track of valid
    # predefined constraints (see the explanation for the
    # InitConstraints proc for more details).
    ArrayDefault testConstraints {}
    Default ConstraintsSpecifiedByCommandLineArgument {}

    # Kept only for compatibility
    Default constraintsSpecified {}
    trace variable constraintsSpecified r {set ::tcltest::constraintsSpecified \
		[array names ::tcltest::testConstraints] ;# }

    # Don't run only the "-constraint" specified tests by default
    Default limitConstraints false

    # A test application has to know how to load the tested commands
    # into the interpreter.
    Default loadScript {}

    # and the filename of the script file, if it exists
    Default loadFile {}

    # tests that use threads need to know which is the main thread
    Default mainThread 1
    variable mainThread
    if {[info commands thread::id] != {}} {
	set mainThread [thread::id]
    } elseif {[info commands testthread] != {}} {
	set mainThread [testthread id]
    }

    # save the original environment so that it can be restored later
    ArrayDefault originalEnv [array get ::env]

    # Set workingDirectory to [pwd]. The default output directory for
    # Tcl tests is the working directory.
    Default workingDirectory [pwd]
    Default temporaryDirectory $workingDirectory

    # tcltest::normalizePath --
    #
    #     This procedure resolves any symlinks in the path thus creating
    #     a path without internal redirection. It assumes that the
    #     incoming path is absolute.
    #
    # Arguments
    #     pathVar contains the name of the variable containing the path
    #     to modify.
    #
    # Results
    #     The path is modified in place.
    #
    # Side Effects:
    #     None.
    #
    proc normalizePath {pathVar} {
	upvar $pathVar path
	set oldpwd [pwd]
	catch {cd $path}
	set path [pwd]
	cd $oldpwd
	return $path
    }

    # Tests should not rely on the current working directory.
    # Files that are part of the test suite should be accessed relative
    # to tcltest::testsDirectory.
    Default testsDirectory [file join \
	    [file dirname [info script]] .. .. tests]
    variable testsDirectory
    normalizePath testsDirectory

    # Default is to run each test file in a separate process
    Default singleProcess 0

    # the variables and procs that existed when saveState was called are
    # stored in a variable of the same name
    Default saveState {}

    # Internationalization support -- used in [SetIso8859_1_Locale] and
    # [RestoreLocale]. Those commands are used in cmdIL.test.

    if {![info exists [namespace current]::isoLocale]} {
	variable isoLocale fr
	switch -- $tcl_platform(platform) {
	    "unix" {

		# Try some 'known' values for some platforms:

		switch -exact -- $tcl_platform(os) {
		    "FreeBSD" {
			set isoLocale fr_FR.ISO_8859-1
		    }
		    HP-UX {
			set isoLocale fr_FR.iso88591
		    }
		    Linux -
		    IRIX {
			set isoLocale fr
		    }
		    default {

			# Works on SunOS 4 and Solaris, and maybe
			# others...  Define it to something else on your
			# system if you want to test those.

			set isoLocale iso_8859_1
		    }
		}
	    }
	    "windows" {
		set isoLocale French
	    }
	}
    }

    # Set the location of the execuatble
    Default tcltest [info nameofexecutable]

    # save the platform information so it can be restored later
    Default originalTclPlatform [array get tcl_platform]

    # If a core file exists, save its modification time.
    variable workingDirectory
    if {[file exists [file join $workingDirectory core]]} {
	Default coreModTime \
		[file mtime [file join $workingDirectory core]]
    }

    # stdout and stderr buffers for use when we want to store them
    Default outData {}
    Default errData {}

    # keep track of test level for nested test commands
    variable testLevel 0
}

#####################################################################

# tcltest::Debug* --
#
#     Internal helper procedures to write out debug information
#     dependent on the chosen level. A test shell may overide
#     them, f.e. to redirect the output into a different
#     channel, or even into a GUI.

# tcltest::DebugPuts --
#
#     Prints the specified string if the current debug level is
#     higher than the provided level argument.
#
# Arguments:
#     level   The lowest debug level triggering the output
#     string  The string to print out.
#
# Results:
#     Prints the string. Nothing else is allowed.
#
# Side Effects:
#     None.
#

proc tcltest::DebugPuts {level string} {
    variable debug
    if {$debug >= $level} {
	puts $string
    }
    return
}

# tcltest::DebugPArray --
#
#     Prints the contents of the specified array if the current
#       debug level is higher than the provided level argument
#
# Arguments:
#     level           The lowest debug level triggering the output
#     arrayvar        The name of the array to print out.
#
# Results:
#     Prints the contents of the array. Nothing else is allowed.
#
# Side Effects:
#     None.
#

proc tcltest::DebugPArray {level arrayvar} {
    variable debug

    if {$debug >= $level} {
	catch {upvar  $arrayvar $arrayvar}
	parray $arrayvar
    }
    return
}

# Define our own [parray] in ::tcltest that will inherit use of the [puts]
# defined in ::tcltest.  NOTE: Ought to construct with [info args] and
# [info default], but can't be bothered now.  If [parray] changes, then
# this will need changing too.
auto_load ::parray
proc tcltest::parray {a {pattern *}} [info body ::parray]

# tcltest::DebugDo --
#
#     Executes the script if the current debug level is greater than
#       the provided level argument
#
# Arguments:
#     level   The lowest debug level triggering the execution.
#     script  The tcl script executed upon a debug level high enough.
#
# Results:
#     Arbitrary side effects, dependent on the executed script.
#
# Side Effects:
#     None.
#

proc tcltest::DebugDo {level script} {
    variable debug

    if {$debug >= $level} {
	uplevel 1 $script
    }
    return
}

#####################################################################

# tcltest::CheckDirectory --
#
#     This procedure checks whether the specified path is a readable
#     and/or writable directory. If one of the conditions is not
#     satisfied an error is printed and the application aborted. The
#     procedure assumes that the caller already checked the existence
#     of the path.
#
# Arguments
#     rw      Information what attributes to check. Allowed values:
#             r, w, rw, wr. If 'r' is part of the value the directory
#             must be readable. 'w' associates to 'writable'.
#     dir     The directory to check.
#     errMsg  The string to prepend to the actual error message before
#             printing it.
#
# Results
#     none
#
# Side Effects:
#     None.
#

proc tcltest::CheckDirectory {rw dir errMsg} {
    # Allowed values for 'rw': r, w, rw, wr

    if {![file isdir $dir]} {
	return -code error "$errMsg \"$dir\" is not a directory"
    } elseif {([string first w $rw] >= 0) && ![file writable $dir]} {
	return -code error "$errMsg \"$dir\" is not writeable"
    } elseif {([string first r $rw] >= 0) && ![file readable $dir]} {
	return -code error "$errMsg \"$dir\" is not readable"
    }
    return
}

# tcltest::MakeAbsolutePath --
#
#     This procedure checks whether the incoming path is absolute or
#     not.  Makes it absolute if it was not.
#
# Arguments
#     pathVar contains the name of the variable containing the path to
#             modify.
#     prefix  is optional, contains the path to use to make the other an
#             absolute one. The current working directory is used if it
#             was not specified.
#
# Results
#     The path is modified in place.
#
# Side Effects:
#     None.
#

proc tcltest::MakeAbsolutePath {pathVar {prefix {}}} {
    upvar $pathVar path

    if {![string equal [file pathtype $path] "absolute"]} {
	if {[string equal {} $prefix]} {
	    set prefix [pwd]
	}

	set path [file join $prefix $path]
    }
    return $path
}

#####################################################################

# tcltest::<variableName>
#
#     Accessor functions for tcltest variables that can be modified
#     externally.  These are vars that could otherwise be modified
#     using command line arguments to tcltest.

#     Many of them are all the same boilerplate:

namespace eval tcltest {
    variable var
    foreach var {
	    match skip matchFiles skipFiles matchDirectories
	    skipDirectories preserveCore debug loadScript singleProcess
	    mainThread ConstraintsSpecifiedByCommandLineArgument
    } {
	proc $var { {new ""} } [subst -nocommands {
	    variable $var
	    if {[llength [info level 0]] == 1} {
		return [set $var]
	    }
	    set $var \$new
	}]
    }
    unset var
}

#     The rest have something special to deal with:

# tcltest::verbose --
#
#	Set or return the verbosity level (tcltest::verbose) for tests.
#       This determines what gets printed to the screen and when, with
#       regard to the running of the tests.  The proc does not check for
#       invalid values.  It assumes that a string that doesn't match its
#       predefined keywords is a string containing letter-specified
#       verbosity levels.
#
# Arguments:
#	A string containing any combination of 'pbste' or a list of
#       keywords (listed in parens)
#          p = print output whenever a test passes (pass)
#          b = print the body of the test when it fails (body)
#          s = print when a test is skipped (skip)
#          t = print when a test starts (start)
#          e = print errorInfo and errorCode when a test encounters an
#              error (error)
#
# Results:
#	content of tcltest::verbose
#
# Side effects:
#	None.

proc tcltest::verbose { {level ""} } {
    variable verbose
    if {[llength [info level 0]] == 1} {
	return $verbose
    }
    if {[llength $level] > 1} {
  	set verbose $level
    } else {
	if {[regexp {pass|body|skip|start|error} $level]} {
	    set verbose $level
	} else {
	    set levelList [split $level {}]
	    set verbose [string map \
		    {p pass b body s skip t start e error} $levelList]
	}
    }
    return $verbose
}

# tcltest::IsVerbose --
#
#	Returns true if argument is one of the verbosity levels
#       currently being used; returns false otherwise.
#
# Arguments:
#	level
#
# Results:
#	boolean 1 (true) or 0 (false), depending on whether or not the
#       level provided is one of the ones stored in tcltest::verbose.
#
# Side effects:
#	None.

proc tcltest::IsVerbose {level} {
    if {[lsearch -exact [verbose] $level] == -1} {
	return 0
    }
    return 1
}

# tcltest::outputChannel --
#
#	set or return the output file descriptor based on the supplied
#       file name (where tcltest puts all of its output)
#
# Arguments:
#	output file descriptor
#
# Results:
#	file descriptor corresponding to supplied file name (or
#       currently set file descriptor, if no new filename was supplied)
#       - this is the content of tcltest::outputChannel
#
# Side effects:
#	None.

proc tcltest::outputChannel { {filename ""} } {
    variable outputChannel
    if {[llength [info level 0]] == 1} {
	return $outputChannel
    }
    switch -exact -- $filename {
	stderr -
	stdout {
	    set outputChannel $filename
	}
	default {
	    set outputChannel [open $filename w]
	}
    }
    return $outputChannel
}

# tcltest::outputFile --
#
#	set or return the output file name (where tcltest puts all of
#       its output); calls [outputChannel] to set the corresponding
#       file descriptor
#
# Arguments:
#	output file name
#
# Results:
#       file name corresponding to supplied file name (or currently set
#       file name, if no new filename was supplied) - this is the
#       content of tcltest::outputFile
#
# Side effects:
#	if the file name supplied is relative, it will be made absolute
#       with respect to the predefined temporaryDirectory

proc tcltest::outputFile { {filename ""} } {
    variable outputFile
    if {[llength [info level 0]] == 1} {
	return $outputFile
    }
    switch -exact -- $filename {
	stderr -
	stdout {
	    # do nothing
	}
	default {
	    MakeAbsolutePath filename [temporaryDirectory]
	}
    }
    outputChannel $filename
    set outputFile $filename
}

# tcltest::errorChannel --
#
#	set or return the error file descriptor based on the supplied
#       file name (where tcltest sends all its errors)
#
# Arguments:
#	error file name
#
# Results:
#	file descriptor corresponding to the supplied file name (or
#       currently set file descriptor, if no new filename was supplied)
#       - this is the content of tcltest::errorChannel
#
# Side effects:
#	opens the descriptor in w mode unless the filename is set to
#       stderr or stdout

proc tcltest::errorChannel { {filename ""} } {
    variable errorChannel
    if {[llength [info level 0]] == 1} {
	return $errorChannel
    }
    switch -exact -- $filename {
	stderr -
	stdout {
	    set errorChannel $filename
	}
	default {
	    set errorChannel [open $filename w]
	}
    }
    return $errorChannel
}

# tcltest::errorFile --
#
#	set or return the error file name; calls [errorChannel] to set
#       the corresponding file descriptor
#
# Arguments:
#	error file name
#
# Results:
#	content of tcltest::errorFile
#
# Side effects:
#	if the file name supplied is relative, it will be made absolute
#       with respect to the predefined temporaryDirectory

proc tcltest::errorFile { {filename ""} } {
    variable errorFile
    if {[llength [info level 0]] == 1} {
	return $errorFile
    }
    switch -exact -- $filename {
	stderr -
	stdout {
	    # do nothing
	}
	default {
	    MakeAbsolutePath filename [temporaryDirectory]
	}
    }
    set errorFile $filename
    errorChannel $errorFile
    return $errorFile
}

# tcltest::testConstraint --
#
#	sets a test constraint to a value; to do multiple constraints,
#       call this proc multiple times.  also returns the value of the
#       named constraint if no value was supplied.
#
# Arguments:
#	constraint - name of the constraint
#       value - new value for constraint (should be boolean) - if not
#               supplied, this is a query
#
# Results:
#	content of tcltest::testConstraints($constraint)
#
# Side effects:
#	none

proc tcltest::testConstraint {constraint {value ""}} {
    variable testConstraints
    DebugPuts 3 "entering testConstraint $constraint $value"
    if {[llength [info level 0]] == 2} {
	return $testConstraints($constraint)
    }
    # Check for boolean values
    if {[catch {expr {$value && $value}} msg]} {
	return -code error $msg
    }
    set testConstraints($constraint) $value
}

# tcltest::limitConstraints --
#
#	sets/gets flag indicating whether tests run are limited only
#	to those matching constraints specified by the -constraints
#	command line option.
#
# Arguments:
#	new boolean value for the flag
#
# Results:
#	content of tcltest::limitConstraints
#
# Side effects:
#	None.

proc tcltest::limitConstraints { {value ""} } {
    variable testConstraints
    variable limitConstraints
    DebugPuts 3 "entering limitConstraints $value"
    if {[llength [info level 0]] == 1} {
	return $limitConstraints
    }
    # Check for boolean values
    if {[catch {expr {$value && $value}} msg]} {
	return -code error $msg
    }
    set limitConstraints $value
    if {!$limitConstraints} {return $limitConstraints}
    foreach elt [array names testConstraints] {
	if {[lsearch -exact [ConstraintsSpecifiedByCommandLineArgument] $elt] 
		== -1} {
	    testConstraint $elt 0
	}
    }
    return $limitConstraints
}

# tcltest::loadFile --
#
#	set the load file (containing the load script);
#       put the content of the load file into loadScript
#
# Arguments:
#	script's file name
#
# Results:
#	content of tcltest::loadFile
#
# Side effects:
#	None.

proc tcltest::loadFile { {scriptFile ""} } {
    variable loadFile
    if {[llength [info level 0]] == 1} {
	return $loadFile
    }
    MakeAbsolutePath scriptFile [temporaryDirectory]
    set tmp [open $scriptFile r]
    loadScript [read $tmp]
    close $tmp
    set loadFile $scriptFile
}

# tcltest::workingDirectory --
#
#	set workingDirectory to the given path.  If the path is
#       relative, make it absolute.  Change directory to the stated
#       working directory, if resetting the value
#
# Arguments:
#	directory name
#
# Results:
#	content of tcltest::workingDirectory
#
# Side effects:
#	None.

proc tcltest::workingDirectory { {dir ""} } {
    variable workingDirectory
    if {[llength [info level 0]] == 1} {
	return $workingDirectory
    }
    set workingDirectory $dir
    MakeAbsolutePath workingDirectory
    cd $workingDirectory
    return $workingDirectory
}

# tcltest::temporaryDirectory --
#
#     Set temporaryDirectory to the given path.  If the path is
#     relative, make it absolute.  If the file exists but is not a dir,
#     then return an error.
#
#     If temporaryDirectory does not already exist, create it.  If you
#     cannot create it, then return an error (the file mkdir isn't
#     caught and will propagate).
#
# Arguments:
#	directory name
#
# Results:
#	content of tcltest::temporaryDirectory
#
# Side effects:
#	None.

proc tcltest::temporaryDirectory { {dir ""} } {
    variable temporaryDirectory
    if {[llength [info level 0]] == 1} {
	return $temporaryDirectory
    }
    set temporaryDirectory $dir
    MakeAbsolutePath temporaryDirectory

    if {[file exists $temporaryDirectory]} {
	CheckDirectory rw $temporaryDirectory \
		{bad argument for temporary directory: }
    } else {
	file mkdir $temporaryDirectory
    }

    normalizePath temporaryDirectory
}

# tcltest::testsDirectory --
#
#     Set testsDirectory to the given path.  If the path is relative,
#     make it absolute.  If the file exists but is not a dir, then
#     return an error.
#
#     If testsDirectory does not already exist, return an error.
#
# Arguments:
#	directory name
#
# Results:
#	content of tcltest::testsDirectory
#
# Side effects:
#	None.

proc tcltest::testsDirectory { {dir ""} } {
    variable testsDirectory
    if {[llength [info level 0]] == 1} {
	return $testsDirectory
    }

    set testsDirectory $dir
    MakeAbsolutePath testsDirectory
    set testDirError "bad argument for tests directory: "
    if {[file exists $testsDirectory]} {
	CheckDirectory r $testsDirectory $testDirError
    } else {
	return -code error \
		"$testDirError \"$testsDirectory\" does not exist"
    }

    normalizePath testsDirectory
}

# tcltest::interpreter --
#
#	the interpreter name stored in tcltest::tcltest
#
# Arguments:
#	executable name
#
# Results:
#	content of tcltest::tcltest
#
# Side effects:
#	None.

proc tcltest::interpreter { {interp ""} } {
    variable tcltest
    if {[llength [info level 0]] == 1} {
	return $tcltest
    }
    if {[string equal {} $interp]} {
	set tcltest {}
    } else {
	set tcltest $interp
    }
}

#####################################################################

# tcltest::AddToSkippedBecause --
#
#	Increments the variable used to track how many tests were
#       skipped because of a particular constraint.
#
# Arguments:
#	constraint     The name of the constraint to be modified
#
# Results:
#	Modifies tcltest::skippedBecause; sets the variable to 1 if
#       didn't previously exist - otherwise, it just increments it.
#
# Side effects:
#	None.

proc tcltest::AddToSkippedBecause { constraint {value 1}} {
    # add the constraint to the list of constraints that kept tests
    # from running
    variable skippedBecause

    if {[info exists skippedBecause($constraint)]} {
	incr skippedBecause($constraint) $value
    } else {
	set skippedBecause($constraint) $value
    }
    return
}

# tcltest::PrintError --
#
#	Prints errors to tcltest::errorChannel and then flushes that
#       channel, making sure that all messages are < 80 characters per
#       line.
#
# Arguments:
#	errorMsg     String containing the error to be printed
#
# Results:
#	None.
#
# Side effects:
#	None.

proc tcltest::PrintError {errorMsg} {
    set InitialMessage "Error:  "
    set InitialMsgLen  [string length $InitialMessage]
    puts -nonewline [errorChannel] $InitialMessage

    # Keep track of where the end of the string is.
    set endingIndex [string length $errorMsg]

    if {$endingIndex < (80 - $InitialMsgLen)} {
	puts [errorChannel] $errorMsg
    } else {
	# Print up to 80 characters on the first line, including the
	# InitialMessage.
	set beginningIndex [string last " " [string range $errorMsg 0 \
		[expr {80 - $InitialMsgLen}]]]
	puts [errorChannel] [string range $errorMsg 0 $beginningIndex]

	while {![string equal end $beginningIndex]} {
	    puts -nonewline [errorChannel] \
		    [string repeat " " $InitialMsgLen]
	    if {($endingIndex - $beginningIndex)
		    < (80 - $InitialMsgLen)} {
		puts [errorChannel] [string trim \
			[string range $errorMsg $beginningIndex end]]
		break
	    } else {
		set newEndingIndex [expr {[string last " " \
			[string range $errorMsg $beginningIndex \
				[expr {$beginningIndex
					+ (80 - $InitialMsgLen)}]
		]] + $beginningIndex}]
		if {($newEndingIndex <= 0)
			|| ($newEndingIndex <= $beginningIndex)} {
		    set newEndingIndex end
		}
		puts [errorChannel] [string trim \
			[string range $errorMsg \
			    $beginningIndex $newEndingIndex]]
		set beginningIndex $newEndingIndex
	    }
	}
    }
    flush [errorChannel]
    return
}

# tcltest::SafeFetch --
#
#	 The following trace procedure makes it so that we can safely
#        refer to non-existent members of the testConstraints array
#        without causing an error.  Instead, reading a non-existent
#        member will return 0. This is necessary because tests are
#        allowed to use constraint "X" without ensuring that
#        testConstraints("X") is defined.
#
# Arguments:
#	n1 - name of the array (testConstraints)
#       n2 - array key value (constraint name)
#       op - operation performed on testConstraints (generally r)
#
# Results:
#	none
#
# Side effects:
#	sets testConstraints($n2) to 0 if it's referenced but never
#       before used

proc tcltest::SafeFetch {n1 n2 op} {
    variable testConstraints
    DebugPuts 3 "entering SafeFetch $n1 $n2 $op"
    if {[string equal {} $n2]} {return}
    if {![info exists testConstraints($n2)]} {
	if {[catch {testConstraint $n2 [eval [ConstraintInitializer $n2]]}]} {
	    testConstraint $n2 0
	}
    }
}

# tcltest::ConstraintInitializer --
#
#	Get or set a script that when evaluated in the tcltest namespace
#	will return a boolean value with which to initialize the
#	associated constraint.
#
# Arguments:
#	constraint - name of the constraint initialized by the script
#	script - the initializer script
#
# Results
#	boolean value of the constraint - enabled or disabled
#
# Side effects:
#	Constraint is initialized for future reference by [test]
proc tcltest::ConstraintInitializer {constraint {script ""}} {
    variable ConstraintInitializer
    DebugPuts 3 "entering ConstraintInitializer $constraint $script"
    if {[llength [info level 0]] == 2} {
	return $ConstraintInitializer($constraint)
    }
    # Check for boolean values
    if {![info complete $script]} {
	return -code error "ConstraintInitializer must be complete script"
    }
    set ConstraintInitializer($constraint) $script
}

# tcltest::InitConstraints --
#
# Call all registered constraint initializers to force initialization
# of all known constraints.
# See the tcltest man page for the list of built-in constraints defined
# in this procedure.
#
# Arguments:
#	none
#
# Results:
#	The testConstraints array is reset to have an index for each
#	built-in test constraint.
#
# Side Effects:
#       None.
#

proc tcltest::InitConstraints {} {
    variable ConstraintInitializer
    initConstraintsHook
    foreach constraint [array names ConstraintInitializer] {
	testConstraint $constraint
    }
}

proc tcltest::DefineConstraintInitializers {} {
    ConstraintInitializer singleTestInterp {singleProcess}

    # All the 'pc' constraints are here for backward compatibility and
    # are not documented.  They have been replaced with equivalent 'win'
    # constraints.

    ConstraintInitializer unixOnly \
	    {string equal $::tcl_platform(platform) unix}
    ConstraintInitializer macOnly \
	    {string equal $::tcl_platform(platform) macintosh}
    ConstraintInitializer pcOnly \
	    {string equal $::tcl_platform(platform) windows}
    ConstraintInitializer winOnly \
	    {string equal $::tcl_platform(platform) windows}

    ConstraintInitializer unix {testConstraint unixOnly}
    ConstraintInitializer mac {testConstraint macOnly}
    ConstraintInitializer pc {testConstraint pcOnly}
    ConstraintInitializer win {testConstraint winOnly}

    ConstraintInitializer unixOrPc \
	    {expr {[testConstraint unix] || [testConstraint pc]}}
    ConstraintInitializer macOrPc \
	    {expr {[testConstraint mac] || [testConstraint pc]}}
    ConstraintInitializer unixOrWin \
	    {expr {[testConstraint unix] || [testConstraint win]}}
    ConstraintInitializer macOrWin \
	    {expr {[testConstraint mac] || [testConstraint win]}}
    ConstraintInitializer macOrUnix \
	    {expr {[testConstraint mac] || [testConstraint unix]}}

    ConstraintInitializer nt {string equal $tcl_platform(os) "Windows NT"}
    ConstraintInitializer 95 {string equal $tcl_platform(os) "Windows 95"}
    ConstraintInitializer 98 {string equal $tcl_platform(os) "Windows 98"}

    # The following Constraints switches are used to mark tests that
    # should work, but have been temporarily disabled on certain
    # platforms because they don't and we haven't gotten around to
    # fixing the underlying problem.

    ConstraintInitializer tempNotPc {expr {![testConstraint pc]}}
    ConstraintInitializer tempNotWin {expr {![testConstraint win]}}
    ConstraintInitializer tempNotMac {expr {![testConstraint mac]}}
    ConstraintInitializer tempNotUnix {expr {![testConstraint unix]}}

    # The following Constraints switches are used to mark tests that
    # crash on certain platforms, so that they can be reactivated again
    # when the underlying problem is fixed.

    ConstraintInitializer pcCrash {expr {![testConstraint pc]}}
    ConstraintInitializer winCrash {expr {![testConstraint win]}}
    ConstraintInitializer macCrash {expr {![testConstraint mac]}}
    ConstraintInitializer unixCrash {expr {![testConstraint unix]}}

    # Skip empty tests

    ConstraintInitializer emptyTest {format 0}

    # By default, tests that expose known bugs are skipped.

    ConstraintInitializer knownBug {format 0}

    # By default, non-portable tests are skipped.

    ConstraintInitializer nonPortable {format 0}

    # Some tests require user interaction.

    ConstraintInitializer userInteraction {format 0}

    # Some tests must be skipped if the interpreter is not in
    # interactive mode

    ConstraintInitializer interactive \
	    {expr {[info exists ::tcl_interactive] && $::tcl_interactive}}

    # Some tests can only be run if the installation came from a CD
    # image instead of a web image.  Some tests must be skipped if you
    # are running as root on Unix.  Other tests can only be run if you
    # are running as root on Unix.

    ConstraintInitializer root {expr \
	    {[string equal unix $::tcl_platform(platform)]
	    && ([string equal root $::tcl_platform(user)]
		|| [string equal "" $::tcl_platform(user)])}}
    ConstraintInitializer notRoot {expr {![testConstraint root]}}

    # Set nonBlockFiles constraint: 1 means this platform supports
    # setting files into nonblocking mode.

    ConstraintInitializer nonBlockFiles {
	    set code [expr {[catch {set f [open defs r]}] 
		    || [catch {fconfigure $f -blocking off}]}]
	    catch {close $f}
	    set code
    }

    # Set asyncPipeClose constraint: 1 means this platform supports
    # async flush and async close on a pipe.
    #
    # Test for SCO Unix - cannot run async flushing tests because a
    # potential problem with select is apparently interfering.
    # (Mark Diekhans).

    ConstraintInitializer asyncPipeClose {expr {
	    !([string equal unix $::tcl_platform(platform)] 
	    && ([catch {exec uname -X | fgrep {Release = 3.2v}}] == 0))}}

    # Test to see if we have a broken version of sprintf with respect
    # to the "e" format of floating-point numbers.

    ConstraintInitializer eformat {string equal [format %g 5e-5] 5e-05}

    # Test to see if execed commands such as cat, echo, rm and so forth
    # are present on this machine.

    ConstraintInitializer unixExecs {
	set code 1
        if {[string equal macintosh $::tcl_platform(platform)]} {
	    set code 0
        }
        if {[string equal windows $::tcl_platform(platform)]} {
	    if {[catch {
	        set file _tcl_test_remove_me.txt
	        makeFile {hello} $file
	    }]} {
	        set code 0
	    } elseif {
	        [catch {exec cat $file}] ||
	        [catch {exec echo hello}] ||
	        [catch {exec sh -c echo hello}] ||
	        [catch {exec wc $file}] ||
	        [catch {exec sleep 1}] ||
	        [catch {exec echo abc > $file}] ||
	        [catch {exec chmod 644 $file}] ||
	        [catch {exec rm $file}] ||
	        [llength [auto_execok mkdir]] == 0 ||
	        [llength [auto_execok fgrep]] == 0 ||
	        [llength [auto_execok grep]] == 0 ||
	        [llength [auto_execok ps]] == 0
	    } {
	        set code 0
	    }
	    removeFile $file
        }
	set code
    }

    ConstraintInitializer stdio {
	set code 0
	if {![catch {set f [open "|[list [interpreter]]" w]}]} {
	    if {![catch {puts $f exit}]} {
		if {![catch {close $f}]} {
		    set code 1
		}
	    }
	}
	set code
    }

    # Deliberately call socket with the wrong number of arguments.  The
    # error message you get will indicate whether sockets are available
    # on this system.

    ConstraintInitializer socket {
	catch {socket} msg
	string compare $msg "sockets are not available on this system"
    }

    # Check for internationalization
    ConstraintInitializer hasIsoLocale {
	if {[llength [info commands testlocale]] == 0} {
	    set code 0
	} else {
	    set code [string length [SetIso8859_1_Locale]]
	    RestoreLocale
	}
	set code
    }

}
#####################################################################

# Handle command line arguments (from argv) and default arg settings
# (in TCLTEST_OPTIONS).

# tcltest::PrintUsageInfoHook
#
#       Hook used for customization of display of usage information.
#

if {[llength [info commands tcltest::PrintUsageInfoHook]] == 0} {
    proc tcltest::PrintUsageInfoHook {} {}
}

# tcltest::PrintUsageInfo
#
#	Prints out the usage information for package tcltest.  This can
#	be customized with the redefinition of [PrintUsageInfoHook].
#
# Arguments:
#	none
#
# Results:
#       none
#
# Side Effects:
#       none
proc tcltest::PrintUsageInfo {} {
    puts "Usage: [file tail [info nameofexecutable]]\
	script ?-help? ?flag value? ... \n\
	Available flags (and valid input values) are:\n\
	-help          		 Display this usage information.\n\
	-verbose level 		 Takes any combination of the values\n\
	\t                 'p', 's', 'b', 't' and 'e'. \
			   Test suite will\n\
	\t                 display all passed tests if 'p' is\n\
	\t                 specified, all skipped tests if 's'\n\
	\t                 is specified, the bodies of\n\
	\t                 failed tests if 'b' is specified,\n\
	\t                 and when tests start if 't' is specified.\n\
	\t                 ErrorInfo is displayed\
			   if 'e' is specified.\n\
	\t                 The default value is 'b'.\n\
	-constraints list	 Do not skip the listed constraints\n\
	-limitconstraints bool	 Only run tests with the constraints\n\
	\t                 listed in -constraints.\n\
	-match pattern 		 Run all tests within the specified\n\
	\t                 files that match the glob pattern\n\
	\t                 given.\n\
	-skip pattern  		 Skip all tests within the set of\n\
	\t                 specified tests (via -match) and\n\
	\t                 files that match the glob pattern\n\
	\t                 given.\n\
	-file pattern  		 Run tests in all test files that\n\
	\t                 match the glob pattern given.\n\
	-notfile pattern	 Skip all test files that match the\n\
	\t                 glob pattern given.\n\
	-relateddir pattern	 Run tests in directories that match\n\
	\t                 the glob pattern given.\n\
	-asidefromdir pattern	 Skip tests in directories that match\n\
	\t                 the glob pattern given.\n\
	-preservecore level 	 If 2, save any core files produced\n\
	\t                 during testing in the directory\n\
	\t                 specified by -tmpdir. If 1, notify the\n\
	\t                 user if core files are created. \
			   The default\n\
	\t                 is [preserveCore].\n\
	-tmpdir directory	 Save temporary files\
			   in the specified\n\
	\t                 directory.  The default value is\n\
	\t                 [temporaryDirectory]\n\
	-testdir directories	 Search tests in the specified\n\
	\t                 directories.  The default value is\n\
	\t                 [testsDirectory].\n\
	-outfile file    	 Send output from test runs to the\n\
	\t                 specified file.  The default is\n\
	\t                 stdout.\n\
	-errfile file    	 Send errors from test runs to the\n\
	\t                 specified file.  The default is\n\
	\t                 stderr.\n\
	-loadfile file   	 Read the script to load the tested\n\
	\t                 commands from the specified file.\n\
	-load script     	 Specifies the script\
			   to load the tested\n\
	\t                 commands.\n\
	-debug level     	 Internal debug flag."
    PrintUsageInfoHook
    return
}

# tcltest::processCmdLineArgsFlagsHook --
#
#	This hook is used to add to the list of command line arguments
#	that are processed by tcltest::ProcessFlags.   It is called at
#	the beginning of ProcessFlags.
#

if {[llength [info commands \
	tcltest::processCmdLineArgsAddFlagsHook]] == 0} {
    proc tcltest::processCmdLineArgsAddFlagsHook {} {}
}

# tcltest::processCmdLineArgsHook --
#
#	This hook is used to actually process the flags added by
#       tcltest::processCmdLineArgsAddFlagsHook.  It is called at the
#	end of ProcessFlags.
#
# Arguments:
#	flags      The flags that have been pulled out of argv
#

if {[llength [info commands tcltest::processCmdLineArgsHook]] == 0} {
    proc tcltest::processCmdLineArgsHook {flag} {}
}

# tcltest::ProcessFlags --
#
#	process command line arguments supplied in the flagArray - this
#	is called by processCmdLineArgs.  Modifies tcltest variables
#	according to the content of the flagArray.
#
# Arguments:
#	flagArray - array containing name/value pairs of flags
#
# Results:
#	sets tcltest variables according to their values as defined by
#       flagArray
#
# Side effects:
#	None.

proc tcltest::ProcessFlags {flagArray} {
    # Process -help first
    if {[lsearch -exact $flagArray {-help}] != -1} {
	PrintUsageInfo
	exit 1
    }

    catch {array set flag $flagArray}

    # -help is not listed since it has already been processed
    lappend defaultFlags -verbose -match -skip -constraints \
	    -outfile -errfile -debug -tmpdir -file -notfile \
	    -preservecore -limitconstraints -testdir \
	    -load -loadfile -asidefromdir \
	    -relateddir -singleproc
    set defaultFlags \
	    [concat $defaultFlags [processCmdLineArgsAddFlagsHook]]

    # Set verbose to the arg of the -verbose flag, if given
    if {[info exists flag(-verbose)]} {
	verbose $flag(-verbose)
    }

    # Set match to the arg of the -match flag, if given.
    if {[info exists flag(-match)]} {
	match $flag(-match)
    }

    # Set skip to the arg of the -skip flag, if given
    if {[info exists flag(-skip)]} {
	skip $flag(-skip)
    }

    # Handle the -file and -notfile flags
    if {[info exists flag(-file)]} {
	matchFiles $flag(-file)
    }
    if {[info exists flag(-notfile)]} {
	skipFiles $flag(-notfile)
    }

    # Handle -relateddir and -asidefromdir flags
    if {[info exists flag(-relateddir)]} {
	matchDirectories $flag(-relateddir)
    }
    if {[info exists flag(-asidefromdir)]} {
	skipDirectories $flag(-asidefromdir)
    }

    # Use the -constraints flag, if given, to turn on constraints that
    # are turned off by default: userInteractive knownBug nonPortable.

    if {[info exists flag(-constraints)]} {
	foreach elt $flag(-constraints) {
	    testConstraint $elt 1
	}
	ConstraintsSpecifiedByCommandLineArgument $flag(-constraints)
    }

    # Use the -limitconstraints flag, if given, to tell the harness to
    # limit tests run to those that were specified using the
    # -constraints flag.  If the -constraints flag was not specified,
    # print out an error and exit.
    if {[info exists flag(-limitconstraints)]} {
	if {![info exists flag(-constraints)]} {
	    error "-limitconstraints flag can only\
		    be used with -constraints"
	}
	limitConstraints $flag(-limitconstraints)
    }

    # Set the temporaryDirectory to the arg of -tmpdir, if given.

    if {[info exists flag(-tmpdir)]} {
	temporaryDirectory $flag(-tmpdir)
    }

    # Set the testsDirectory to the arg of -testdir, if given.

    if {[info exists flag(-testdir)]} {
	testsDirectory $flag(-testdir)
    }

    # If an alternate error or output files are specified, change the
    # default channels.

    if {[info exists flag(-outfile)]} {
	outputFile $flag(-outfile)
    }

    if {[info exists flag(-errfile)]} {
	errorFile $flag(-errfile)
    }

    # If a load script was specified, either directly or through
    # a file, remember it for later usage.

    if {[info exists flag(-load)] &&  \
	    ([lsearch -exact $flagArray -load] > \
		[lsearch -exact $flagArray -loadfile])} {
	loadScript $flag(-load)
    }

    if {[info exists flag(-loadfile)] && \
	    ([lsearch -exact $flagArray -loadfile] > \
		[lsearch -exact $flagArray -load]) } {
	loadFile $flag(-loadfile)
    }

    # If the user specifies debug testing, print out extra information
    # during the run.
    if {[info exists flag(-debug)]} {
	debug $flag(-debug)
    }

    # Handle -preservecore
    if {[info exists flag(-preservecore)]} {
	preserveCore $flag(-preservecore)
    }

    # Handle -singleproc flag
    if {[info exists flag(-singleproc)]} {
	singleProcess $flag(-singleproc)
    }

    # Call the hook
    processCmdLineArgsHook [array get flag]
    return
}

# tcltest::ProcessCmdLineArgs --
#
#	Use command line args to set tcltest namespace variables.
#
#       This procedure must be run after constraints are initialized,
#	because some constraints can be overridden.
#
#       Set variables based on the contents of the environment variable
#       TCLTEST_OPTIONS first, then override with command-line options,
#	if specified.
#
# Arguments:
#	none
#
# Results:
#	Sets the above-named variables in the tcltest namespace.
#
# Side Effects:
#       None.
#

proc tcltest::ProcessCmdLineArgs {} {
    global argv
    variable originalEnv
    variable testConstraints

    # If the TCLTEST_OPTIONS environment variable exists, parse it
    # first, then the argv list.  The command line argument parsing will
    # be a two-pass affair from now on, so that TCLTEST_OPTIONS contain
    # the default options.  These can be overridden by the command line
    # flags.

    if {[info exists ::env(TCLTEST_OPTIONS)]} {
	ProcessFlags $::env(TCLTEST_OPTIONS)
    }

    # The "argv" var doesn't exist in some cases, so use {}.
    if {(![info exists argv]) || ([llength $argv] < 1)} {
	set flagArray {}
    } else {
	set flagArray $argv
    }

    ProcessFlags $flagArray

    # Spit out everything you know if we're at a debug level 2 or
    # greater
    DebugPuts 2 "Flags passed into tcltest:"
    if {[info exists ::env(TCLTEST_OPTIONS)]} {
	DebugPuts 2 \
		"    ::env(TCLTEST_OPTIONS): $::env(TCLTEST_OPTIONS)"
    }
    if {[info exists argv]} {
	DebugPuts 2 "    argv: $argv"
    }
    DebugPuts    2 "tcltest::debug              = [debug]"
    DebugPuts    2 "tcltest::testsDirectory     = [testsDirectory]"
    DebugPuts    2 "tcltest::workingDirectory   = [workingDirectory]"
    DebugPuts    2 "tcltest::temporaryDirectory = [temporaryDirectory]"
    DebugPuts    2 "tcltest::outputChannel      = [outputChannel]"
    DebugPuts    2 "tcltest::errorChannel       = [errorChannel]"
    DebugPuts    2 "Original environment (tcltest::originalEnv):"
    DebugPArray  2 originalEnv
    DebugPuts    2 "Constraints:"
    DebugPArray  2 testConstraints
    return
}

#####################################################################

# Code to run the tests goes here.

# tcltest::TestPuts --
#
#	Used to redefine puts in test environment.  Stores whatever goes
#	out on stdout in tcltest::outData and stderr in errData before
#	sending it on to the regular puts.
#
# Arguments:
#	same as standard puts
#
# Results:
#	none
#
# Side effects:
#       Intercepts puts; data that would otherwise go to stdout, stderr,
#	or file channels specified in outputChannel and errorChannel
#	does not get sent to the normal puts function.
namespace eval tcltest::Replace {
    namespace export puts
}
proc tcltest::Replace::puts {args} {
    variable [namespace parent]::outData
    variable [namespace parent]::errData
    switch [llength $args] {
	1 {
	    # Only the string to be printed is specified
	    append outData [lindex $args 0]\n
	    return
	    # return [Puts [lindex $args 0]]
	}
	2 {
	    # Either -nonewline or channelId has been specified
	    if {[string equal -nonewline [lindex $args 0]]} {
		append outData [lindex $args end]
		return
		# return [Puts -nonewline [lindex $args end]]
	    } else {
		set channel [lindex $args 0]
	    }
	}
	3 {
	    if {[string equal -nonewline [lindex $args 0]]} {
		# Both -nonewline and channelId are specified, unless
		# it's an error.  -nonewline is supposed to be argv[0].
		set channel [lindex $args 1]
	    }
	}
    }

    if {[info exists channel]} {
	if {[string equal $channel [[namespace parent]::outputChannel]]
		|| [string equal $channel stdout]} {
	    append outData [lindex $args end]\n
	} elseif {[string equal $channel [[namespace parent]::errorChannel]]
		|| [string equal $channel stderr]} {
	    append errData [lindex $args end]\n
	}
	return
	# return [Puts [lindex $args 0] [lindex $args end]]
    }

    # If we haven't returned by now, we don't know how to handle the
    # input.  Let puts handle it.
    return [eval Puts $args]
}

# tcltest::Eval --
#
#	Evaluate the script in the test environment.  If ignoreOutput is
#       false, store data sent to stderr and stdout in outData and
#       errData.  Otherwise, ignore this output altogether.
#
# Arguments:
#	script             Script to evaluate
#       ?ignoreOutput?     Indicates whether or not to ignore output
#			   sent to stdout & stderr
#
# Results:
#	result from running the script
#
# Side effects:
#	Empties the contents of outData and errData before running a
#	test if ignoreOutput is set to 0.

proc tcltest::Eval {script {ignoreOutput 1}} {
    variable outData
    variable errData
    DebugPuts 3 "[lindex [info level 0] 0] called"
    if {!$ignoreOutput} {
	set outData {}
	set errData {}
	set callerHasPuts [llength [uplevel 1 {
		::info commands [::namespace current]::puts
	}]]
	if {$callerHasPuts} {
	    uplevel 1 [list ::rename puts [namespace current]::Replace::Puts]
	} else {
	    interp alias {} [namespace current]::Replace::Puts {} ::puts
	}
	uplevel 1 [list ::namespace import [namespace origin Replace::puts]]
	namespace import Replace::puts
    }
    set result [uplevel 1 $script]
    if {!$ignoreOutput} {
	namespace forget puts
	uplevel 1 ::namespace forget puts
	if {$callerHasPuts} {
	    uplevel 1 [list ::rename [namespace current]::Replace::Puts puts]
	} else {
	    interp alias {} [namespace current]::Replace::Puts {}
	}
    }
    return $result
}

# tcltest::CompareStrings --
#
#	compares the expected answer to the actual answer, depending on
#	the mode provided.  Mode determines whether a regexp, exact,
#	glob or custom comparison is done.
#
# Arguments:
#	actual - string containing the actual result
#       expected - pattern to be matched against
#       mode - type of comparison to be done
#
# Results:
#	result of the match
#
# Side effects:
#	None.

proc tcltest::CompareStrings {actual expected mode} {
    variable CustomMatch
    if {![info exists CustomMatch($mode)]} {
        return -code error "No matching command registered for `-match $mode'"
    }
    set match [namespace eval :: $CustomMatch($mode) [list $expected $actual]]
    if {[catch {expr {$match && $match}} result]} {
	return -code error "Invalid result from `-match $mode' command: $result"
    }
    return $match
}

# tcltest::customMatch --
#
#	registers a command to be called when a particular type of
#	matching is required.
#
# Arguments:
#	nickname - Keyword for the type of matching
#	cmd - Incomplete command that implements that type of matching
#		when completed with expected string and actual string
#		and then evaluated.
#
# Results:
#	None.
#
# Side effects:
#	Sets the variable tcltest::CustomMatch

proc tcltest::customMatch {mode script} {
    variable CustomMatch
    if {![info complete $script]} {
	return -code error \
		"invalid customMatch script; can't evaluate after completion"
    }
    set CustomMatch($mode) $script
}

# tcltest::SubstArguments list
#
# This helper function takes in a list of words, then perform a
# substitution on the list as though each word in the list is a separate
# argument to the Tcl function.  For example, if this function is
# invoked as:
#
#      SubstArguments {$a {$a}}
#
# Then it is as though the function is invoked as:
#
#      SubstArguments $a {$a}
#
# This code is adapted from Paul Duffin's function "SplitIntoWords".
# The original function can be found  on:
#
#      http://purl.org/thecliff/tcl/wiki/858.html
#
# Results:
#     a list containing the result of the substitution
#
# Exceptions:
#     An error may occur if the list containing unbalanced quote or
#     unknown variable.
#
# Side Effects:
#     None.
#

proc tcltest::SubstArguments {argList} {

    # We need to split the argList up into tokens but cannot use list
    # operations as they throw away some significant quoting, and
    # [split] ignores braces as it should.  Therefore what we do is
    # gradually build up a string out of whitespace seperated strings.
    # We cannot use [split] to split the argList into whitespace
    # separated strings as it throws away the whitespace which maybe
    # important so we have to do it all by hand.

    set result {}
    set token ""

    while {[string length $argList]} {
        # Look for the next word containing a quote: " { }
        if {[regexp -indices {[^ \t\n]*[\"\{\}]+[^ \t\n]*} \
		$argList all]} {
            # Get the text leading up to this word, but not including
	    # this word, from the argList.
            set text [string range $argList 0 \
		    [expr {[lindex $all 0] - 1}]]
            # Get the word with the quote
            set word [string range $argList \
                    [lindex $all 0] [lindex $all 1]]

            # Remove all text up to and including the word from the
            # argList.
            set argList [string range $argList \
                    [expr {[lindex $all 1] + 1}] end]
        } else {
            # Take everything up to the end of the argList.
            set text $argList
            set word {}
            set argList {}
        }

        if {$token != {}} {
            # If we saw a word with quote before, then there is a
            # multi-word token starting with that word.  In this case,
            # add the text and the current word to this token.
            append token $text $word
        } else {
            # Add the text to the result.  There is no need to parse
            # the text because it couldn't be a part of any multi-word
            # token.  Then start a new multi-word token with the word
            # because we need to pass this token to the Tcl parser to
            # check for balancing quotes
            append result $text
            set token $word
        }

        if { [catch {llength $token} length] == 0 && $length == 1} {
            # The token is a valid list so add it to the result.
            # lappend result [string trim $token]
            append result \{$token\}
            set token {}
        }
    }

    # If the last token has not been added to the list then there
    # is a problem.
    if { [string length $token] } {
        error "incomplete token \"$token\""
    }

    return $result
}


# tcltest::test --
#
# This procedure runs a test and prints an error message if the test
# fails.  If verbose has been set, it also prints a message even if the
# test succeeds.  The test will be skipped if it doesn't match the
# match variable, if it matches an element in skip, or if one of the
# elements of "constraints" turns out not to be true.
#
# If testLevel is 1, then this is a top level test, and we record
# pass/fail information; otherwise, this information is not logged and
# is not added to running totals.
#
# Attributes:
#   Only description is a required attribute.  All others are optional.
#   Default values are indicated.
#
#   constraints -	A list of one or more keywords, each of which
#			must be the name of an element in the array
#			"testConstraints".  If any of these elements is
#			zero, the test is skipped. This attribute is
#			optional; default is {}
#   body -	        Script to run to carry out the test.  It must
#		        return a result that can be checked for
#		        correctness.  This attribute is optional;
#                       default is {}
#   result -	        Expected result from script.  This attribute is
#                       optional; default is {}.
#   output -            Expected output sent to stdout.  This attribute
#                       is optional; default is {}.
#   errorOutput -       Expected output sent to stderr.  This attribute
#                       is optional; default is {}.
#   returnCodes -       Expected return codes.  This attribute is
#                       optional; default is {0 2}.
#   setup -             Code to run before $script (above).  This
#                       attribute is optional; default is {}.
#   cleanup -           Code to run after $script (above).  This
#                       attribute is optional; default is {}.
#   match -             specifies type of matching to do on result,
#                       output, errorOutput; this must be a string
#			previously registered by a call to [customMatch].
#			The strings exact, glob, and regexp are pre-registered
#			by the tcltest package.  Default value is exact.
#
# Arguments:
#   name -		Name of test, in the form foo-1.2.
#   description -	Short textual description of the test, to
#  		  	help humans understand what it does.
#
# Results:
#	None.
#
# Side effects:
#       Just about anything is possible depending on the test.
#

proc tcltest::test {name description args} {
    global tcl_platform
    variable testLevel
    DebugPuts 3 "test $name $args"

    incr testLevel

    # Pre-define everything to null except output and errorOutput.  We
    # determine whether or not to trap output based on whether or not
    # these variables (output & errorOutput) are defined.
    foreach item {constraints setup cleanup body result returnCodes
	    match} {
	set $item {}
    }

    # Set the default match mode
    set match exact

    # Set the default match values for return codes (0 is the standard
    # expected return value if everything went well; 2 represents
    # 'return' being used in the test script).
    set returnCodes [list 0 2]

    # The old test format can't have a 3rd argument (constraints or
    # script) that starts with '-'.
    if {[string match -* [lindex $args 0]]
	    || ([llength $args] <= 1)} {
	if {[llength $args] == 1} {
	    set list [SubstArguments [lindex $args 0]]
	    foreach {element value} $list {
		set testAttributes($element) $value
	    }
	    foreach item {constraints match setup body cleanup \
		    result returnCodes output errorOutput} {
		if {[info exists testAttributes([subst -$item])]} {
		    set testAttributes([subst -$item]) [uplevel 1 \
			    ::concat $testAttributes([subst -$item])]
		}
	    }
	} else {
	    array set testAttributes $args
	}

	set validFlags {-setup -cleanup -body -result -returnCodes \
		-match -output -errorOutput -constraints}

	foreach flag [array names testAttributes] {
	    if {[lsearch -exact $validFlags $flag] == -1} {
		incr tcltest::testLevel -1
		set sorted [lsort $validFlags]
		set options [join [lrange $sorted 0 end-1] ", "]
		append options ", or [lindex $sorted end]"
		return -code error "bad option \"$flag\": must be $options"
	    }
	}

	# store whatever the user gave us
	foreach item [array names testAttributes] {
	    set [string trimleft $item "-"] $testAttributes($item)
	}

	# Check the values supplied for -match
	variable CustomMatch
	if {[lsearch [array names CustomMatch] $match] == -1} {
	    incr tcltest::testLevel -1
	    set sorted [lsort [array names CustomMatch]]
	    set values [join [lrange $sorted 0 end-1] ", "]
	    append values ", or [lindex $sorted end]"
	    return -code error "bad -match value \"$match\":\
		    must be $values"
	}

	# Replace symbolic valies supplied for -returnCodes
	regsub -nocase normal $returnCodes 0 returnCodes
	regsub -nocase error $returnCodes 1 returnCodes
	regsub -nocase return $returnCodes 2 returnCodes
	regsub -nocase break $returnCodes 3 returnCodes
	regsub -nocase continue $returnCodes 4 returnCodes
    } else {
	# This is parsing for the old test command format; it is here
	# for backward compatibility.
	set result [lindex $args end]
	if {[llength $args] == 2} {
	    set body [lindex $args 0]
	} elseif {[llength $args] == 3} {
	    set constraints [lindex $args 0]
	    set body [lindex $args 1]
	} else {
	    incr tcltest::testLevel -1
	    return -code error "wrong # args:\
		    should be \"test name desc ?options?\""
	}
    }

    set setupFailure 0
    set cleanupFailure 0

    # Run the setup script
    if {[catch {uplevel 1 $setup} setupMsg]} {
	set setupFailure 1
    }

    # run the test script
    set command [list [namespace origin RunTest] $name $description \
	    $body $result $constraints]
    if {!$setupFailure} {
	if {[info exists output] || [info exists errorOutput]} {
	    set testResult [uplevel 1 \
		    [list [namespace origin Eval] $command 0]]
	} else {
	    set testResult [uplevel 1 \
		    [list [namespace origin Eval] $command 1]]
	}
    } else {
	set testResult setupFailure
    }

    # Run the cleanup code
    if {[catch {uplevel 1 $cleanup} cleanupMsg]} {
	set cleanupFailure 1
    }

    # If testResult is an empty list, then the test was skipped
    if {$testResult != {}} {
	set coreFailure 0
	set coreMsg ""
	# check for a core file first - if one was created by the test,
	# then the test failed
	if {[preserveCore]} {
	    set currentTclPlatform [array get tcl_platform]
	    if {[file exists [file join [workingDirectory] core]]} {
		# There's only a test failure if there is a core file
		# and (1) there previously wasn't one or (2) the new
		# one is different from the old one.
		variable coreModTime
		if {[info exists coreModTime]} {
		    if {$coreModTime != [file mtime \
			    [file join [workingDirectory] core]]} {
			set coreFailure 1
		    }
		} else {
		    set coreFailure 1
		}
		
		if {([preserveCore] > 1) && ($coreFailure)} {
		    append coreMsg "\nMoving file to:\
			    [file join [temporaryDirectory] core-$name]"
		    catch {file rename -force \
			    [file join [workingDirectory] core] \
			    [file join [temporaryDirectory] core-$name]
		    } msg
		    if {[string length $msg] > 0} {
			append coreMsg "\nError:\
				Problem renaming core file: $msg"
		    }
		}
	    }
	    array set tcl_platform $currentTclPlatform
	}

	set actualAnswer [lindex $testResult 0]
	set code [lindex $testResult end]

	# If expected output/error strings exist, we have to compare
	# them.  If the comparison fails, then so did the test.
	set outputFailure 0
	variable outData
	if {[info exists output]} {
	    if {[set outputCompare [catch {
		CompareStrings $outData $output $match
	    } outputMatch]] == 0} {
		set outputFailure [expr {!$outputMatch}]
	    } else {
		set outputFailure 1
	    }
	}
	set errorFailure 0
	variable errData
	if {[info exists errorOutput]} {
	    if {[set errorCompare [catch {
		CompareStrings $errData $errorOutput $match
	    } errorMatch]] == 0} {
		set errorFailure [expr {!$errorMatch}]
	    } else {
		set errorFailure 1
	    }
	}

	# check if the return code matched the expected return code
	set codeFailure 0
	if {[lsearch -exact $returnCodes $code] == -1} {
	    set codeFailure 1
	}

	# check if the answer matched the expected answer
	if {[set scriptCompare [catch {
	    CompareStrings $actualAnswer $result $match
	} scriptMatch]] == 0} {
	    set scriptFailure [expr {!$scriptMatch}]
	} else {
	    set scriptFailure 1
	}

	# if we didn't experience any failures, then we passed
	set testFailed 1
	variable numTests
	if {!($setupFailure || $cleanupFailure || $coreFailure
		|| $outputFailure || $errorFailure || $codeFailure
		|| $scriptFailure)} {
	    if {$testLevel == 1} {
		incr numTests(Passed)
		if {[IsVerbose pass]} {
		    puts [outputChannel] "++++ $name PASSED"
		}
	    }
	    set testFailed 0
	}

	if {$testFailed} {
	    if {$testLevel == 1} {
		incr numTests(Failed)
	    }
	    variable currentFailure true
	    if {![IsVerbose body]} {
		set body ""
	    }	
	    puts [outputChannel] "\n==== $name\
		    [string trim $description] FAILED"
	    if {[string length $body]} {
		puts [outputChannel] "==== Contents of test case:"
		puts [outputChannel] $body
	    }
	    if {$setupFailure} {
		puts [outputChannel] "---- Test setup\
			failed:\n$setupMsg"
	    }
	    if {$scriptFailure} {
	      if {$scriptCompare} {
		puts [outputChannel] "---- Error testing result: $scriptMatch"
	      } else {
		puts [outputChannel] "---- Result\
			was:\n$actualAnswer"
		puts [outputChannel] "---- Result should have been\
			($match matching):\n$result"
	      }
	    }
	    if {$codeFailure} {
		switch -- $code {
		    0 { set msg "Test completed normally" }
		    1 { set msg "Test generated error" }
		    2 { set msg "Test generated return exception" }
		    3 { set msg "Test generated break exception" }
		    4 { set msg "Test generated continue exception" }
		    default { set msg "Test generated exception" }
		}
		puts [outputChannel] "---- $msg; Return code was: $code"
		puts [outputChannel] "---- Return code should have been\
			one of: $returnCodes"
		if {[IsVerbose error]} {
		    if {[info exists ::errorInfo]} {
			puts [outputChannel] "---- errorInfo:\
				$::errorInfo"
			puts [outputChannel] "---- errorCode:\
				$::errorCode"
		    }
		}
	    }
	    if {$outputFailure} {
	      if {$outputCompare} {
		puts [outputChannel] "---- Error testing output: $outputMatch"
	      } else {
		puts [outputChannel] "---- Output was:\n$outData"
		puts [outputChannel] "---- Output should have been\
			($match matching):\n$output"
	      }
	    }
	    if {$errorFailure} {
	      if {$errorCompare} {
		puts [outputChannel] "---- Error testing errorOutput:\
			$errorMatch"
	      } else {
		puts [outputChannel] "---- Error output was:\n$errData"
		puts [outputChannel] "---- Error output should have\
			been ($match matching):\n$errorOutput"
	      }
	    }
	    if {$cleanupFailure} {
		puts [outputChannel] "---- Test cleanup\
			failed:\n$cleanupMsg"
	    }
	    if {$coreFailure} {
		puts [outputChannel] "---- Core file produced while\
			running test!  $coreMsg"
	    }
	    puts [outputChannel] "==== $name FAILED\n"

	}
    }

    incr testLevel -1
    return
}


# RunTest --
#
# This is the defnition of the version 1.0 test routine for tcltest.  It
# is provided here for backward compatibility.  It is also used as the
# 'backbone' of the test procedure, as in, this is where all the work
# really gets done.  This procedure runs a test and prints an error
# message if the test fails.  If verbose has been set, it also prints a
# message even if the test succeeds.  The test will be skipped if it
# doesn't match the match variable, if it matches an element in skip, or
# if one of the elements of "constraints" turns out not to be true.
#
# Arguments:
# name -		Name of test, in the form foo-1.2.
# description -		Short textual description of the test, to help
#			humans understand what it does.
# constraints -		A list of one or more keywords, each of which
#			must be the name of an element in the array
#			"testConstraints".  If any of these elements is
#			zero, the test is skipped.  This argument may be
#			omitted.
# script -		Script to run to carry out the test.  It must
#			return a result that can be checked for
#			correctness.
# expectedAnswer -	Expected result from script.
#
# Behavior depends on the value of testLevel; if testLevel is 1 (top
# level), then events are logged and we track the number of tests
# run/skipped and why.  Otherwise, we don't track this information.
#
# Results:
#    empty list if test is skipped; otherwise returns list containing
#    actual returned value from the test and the return code.
#
# Side Effects:
#    none.
#

proc tcltest::RunTest {
	name description script expectedAnswer constraints
} {
    variable testLevel
    variable numTests
    variable skip
    variable match
    variable testConstraints
    variable originalTclPlatform
    variable coreModTime

    if {$testLevel == 1} {
	incr numTests(Total)
    }

    # skip the test if it's name matches an element of skip
    foreach pattern $skip {
	if {[string match $pattern $name]} {
	    if {$testLevel == 1} {
		incr numTests(Skipped)
		DebugDo 1 {AddToSkippedBecause userSpecifiedSkip}
	    }
	    return
	}
    }

    # skip the test if it's name doesn't match any element of match
    if {[llength $match] > 0} {
	set ok 0
	foreach pattern $match {
	    if {[string match $pattern $name]} {
		set ok 1
		break
	    }
        }
	if {!$ok} {
	    if {$testLevel == 1} {
		incr numTests(Skipped)
		DebugDo 1 {AddToSkippedBecause userSpecifiedNonMatch}
	    }
	    return
	}
    }

    DebugPuts 3 "Running $name ($description) {$script}\
	    {$expectedAnswer} $constraints"

    if {[string equal {} $constraints]} {
	# If we're limited to the listed constraints and there aren't
	# any listed, then we shouldn't run the test.
	if {[limitConstraints]} {
	    AddToSkippedBecause userSpecifiedLimitConstraint
	    if {$testLevel == 1} {
		incr numTests(Skipped)
	    }
	    return
	}
    } else {
	# "constraints" argument exists;
	# make sure that the constraints are satisfied.

	set doTest 0
	if {[string match {*[$\[]*} $constraints] != 0} {
	    # full expression, e.g. {$foo > [info tclversion]}
	    catch {set doTest [uplevel #0 expr $constraints]}
	} elseif {[regexp {[^.a-zA-Z0-9 \n\r\t]+} $constraints] != 0} {
	    # something like {a || b} should be turned into
	    # $testConstraints(a) || $testConstraints(b).
	    regsub -all {[.\w]+} $constraints {$testConstraints(&)} c
	    catch {set doTest [eval expr $c]}
	} elseif {![catch {llength $constraints}]} {
	    # just simple constraints such as {unixOnly fonts}.
	    set doTest 1
	    foreach constraint $constraints {
		if {(![info exists testConstraints($constraint)]) \
			|| (!$testConstraints($constraint))} {
		    set doTest 0

		    # store the constraint that kept the test from
		    # running
		    set constraints $constraint
		    break
		}
	    }
	}
	
	if {$doTest == 0} {
	    if {[IsVerbose skip]} {
		puts [outputChannel] "++++ $name SKIPPED: $constraints"
	    }

	    if {$testLevel == 1} {
		incr numTests(Skipped)
		AddToSkippedBecause $constraints
	    }
	    return	
	}
    }

    # Save information about the core file.  You need to restore the
    # original tcl_platform environment because some of the tests mess
    # with tcl_platform.

    if {[preserveCore]} {
	set currentTclPlatform [array get tcl_platform]
	array set tcl_platform $originalTclPlatform
	if {[file exists [file join [workingDirectory] core]]} {
	    set coreModTime \
		    [file mtime [file join [workingDirectory] core]]
	}
	array set tcl_platform $currentTclPlatform
    }

    # If there is no "memory" command (because memory debugging isn't
    # enabled), then don't attempt to use the command.

    if {[llength [info commands memory]] == 1} {
	memory tag $name
    }

    if {[IsVerbose start]} {
	puts [outputChannel] "---- $name start"
	flush [outputChannel]
    }

    set code [catch {uplevel 1 $script} actualAnswer]

    return [list $actualAnswer $code]
}

#####################################################################

# tcltest::cleanupTestsHook --
#
#	This hook allows a harness that builds upon tcltest to specify
#       additional things that should be done at cleanup.
#

if {[llength [info commands tcltest::cleanupTestsHook]] == 0} {
    proc tcltest::cleanupTestsHook {} {}
}

# tcltest::cleanupTests --
#
# Remove files and dirs created using the makeFile and makeDirectory
# commands since the last time this proc was invoked.
#
# Print the names of the files created without the makeFile command
# since the tests were invoked.
#
# Print the number tests (total, passed, failed, and skipped) since the
# tests were invoked.
#
# Restore original environment (as reported by special variable env).
#
# Arguments:
#      calledFromAllFile - if 0, behave as if we are running a single
#      test file within an entire suite of tests.  if we aren't running
#      a single test file, then don't report status.  check for new
#      files created during the test run and report on them.  if 1,
#      report collated status from all the test file runs.
#
# Results:
#      None.
#
# Side Effects:
#      None
#

proc tcltest::cleanupTests {{calledFromAllFile 0}} {
    variable filesMade
    variable filesExisted
    variable createdNewFiles
    variable testSingleFile
    variable numTests
    variable numTestFiles
    variable failFiles
    variable skippedBecause
    variable currentFailure
    variable originalEnv
    variable originalTclPlatform
    variable coreModTime

    set testFileName [file tail [info script]]

    # Call the cleanup hook
    cleanupTestsHook

    # Remove files and directories created by the makeFile and
    # makeDirectory procedures.  Record the names of files in
    # workingDirectory that were not pre-existing, and associate them
    # with the test file that created them.

    if {!$calledFromAllFile} {
	foreach file $filesMade {
	    if {[file exists $file]} {
		catch {file delete -force $file}
	    }
	}
	set currentFiles {}
	foreach file [glob -nocomplain \
		-directory [temporaryDirectory] *] {
	    lappend currentFiles [file tail $file]
	}
	set newFiles {}
	foreach file $currentFiles {
	    if {[lsearch -exact $filesExisted $file] == -1} {
		lappend newFiles $file
	    }
	}
	set filesExisted $currentFiles
	if {[llength $newFiles] > 0} {
	    set createdNewFiles($testFileName) $newFiles
	}
    }

    if {$calledFromAllFile || $testSingleFile} {

	# print stats

	puts -nonewline [outputChannel] "$testFileName:"
	foreach index [list "Total" "Passed" "Skipped" "Failed"] {
	    puts -nonewline [outputChannel] \
		    "\t$index\t$numTests($index)"
	}
	puts [outputChannel] ""

	# print number test files sourced
	# print names of files that ran tests which failed

	if {$calledFromAllFile} {
	    puts [outputChannel] \
		    "Sourced $numTestFiles Test Files."
	    set numTestFiles 0
	    if {[llength $failFiles] > 0} {
		puts [outputChannel] \
			"Files with failing tests: $failFiles"
		set failFiles {}
	    }
	}

	# if any tests were skipped, print the constraints that kept
	# them from running.

	set constraintList [array names skippedBecause]
	if {[llength $constraintList] > 0} {
	    puts [outputChannel] \
		    "Number of tests skipped for each constraint:"
	    foreach constraint [lsort $constraintList] {
		puts [outputChannel] \
			"\t$skippedBecause($constraint)\t$constraint"
		unset skippedBecause($constraint)
	    }
	}

	# report the names of test files in createdNewFiles, and reset
	# the array to be empty.

	set testFilesThatTurded [lsort [array names createdNewFiles]]
	if {[llength $testFilesThatTurded] > 0} {
	    puts [outputChannel] "Warning: files left behind:"
	    foreach testFile $testFilesThatTurded {
		puts [outputChannel] \
			"\t$testFile:\t$createdNewFiles($testFile)"
		unset createdNewFiles($testFile)
	    }
	}

	# reset filesMade, filesExisted, and numTests

	set filesMade {}
	foreach index [list "Total" "Passed" "Skipped" "Failed"] {
	    set numTests($index) 0
	}

	# exit only if running Tk in non-interactive mode

	global tk_version tcl_interactive
	if {![catch {package present Tk}]
		&& ![info exists tcl_interactive]} {
	    exit
	}
    } else {

	# if we're deferring stat-reporting until all files are sourced,
	# then add current file to failFile list if any tests in this
	# file failed

	incr numTestFiles
	if {$currentFailure \
		&& ([lsearch -exact $failFiles $testFileName] == -1)} {
	    lappend failFiles $testFileName
	}
	set currentFailure false

	# restore the environment to the state it was in before this package
	# was loaded

	set newEnv {}
	set changedEnv {}
	set removedEnv {}
	foreach index [array names ::env] {
	    if {![info exists originalEnv($index)]} {
		lappend newEnv $index
		unset ::env($index)
	    } else {
		if {$::env($index) != $originalEnv($index)} {
		    lappend changedEnv $index
		    set ::env($index) $originalEnv($index)
		}
	    }
	}
	foreach index [array names originalEnv] {
	    if {![info exists ::env($index)]} {
		lappend removedEnv $index
		set ::env($index) $originalEnv($index)
	    }
	}
	if {[llength $newEnv] > 0} {
	    puts [outputChannel] \
		    "env array elements created:\t$newEnv"
	}
	if {[llength $changedEnv] > 0} {
	    puts [outputChannel] \
		    "env array elements changed:\t$changedEnv"
	}
	if {[llength $removedEnv] > 0} {
	    puts [outputChannel] \
		    "env array elements removed:\t$removedEnv"
	}

	set changedTclPlatform {}
	foreach index [array names originalTclPlatform] {
	    if {$::tcl_platform($index) \
		    != $originalTclPlatform($index)} {
		lappend changedTclPlatform $index
		set ::tcl_platform($index) $originalTclPlatform($index)
	    }
	}
	if {[llength $changedTclPlatform] > 0} {
	    puts [outputChannel] "tcl_platform array elements\
		    changed:\t$changedTclPlatform"
	}

	if {[file exists [file join [workingDirectory] core]]} {
	    if {[preserveCore] > 1} {
		puts "rename core file (> 1)"
		puts [outputChannel] "produced core file! \
			Moving file to: \
			[file join [temporaryDirectory] core-$name]"
		catch {file rename -force \
			[file join [workingDirectory] core] \
			[file join [temporaryDirectory] core-$name]
		} msg
		if {[string length $msg] > 0} {
		    PrintError "Problem renaming file: $msg"
		}
	    } else {
		# Print a message if there is a core file and (1) there
		# previously wasn't one or (2) the new one is different
		# from the old one.

		if {[info exists coreModTime]} {
		    if {$coreModTime != [file mtime \
			    [file join [workingDirectory] core]]} {
			puts [outputChannel] "A core file was created!"
		    }
		} else {
		    puts [outputChannel] "A core file was created!"
		}
	    }
	}
    }
    flush [outputChannel]
    flush [errorChannel]
    return
}

#####################################################################

# Procs that determine which tests/test files to run

# tcltest::GetMatchingFiles
#
#       Looks at the patterns given to match and skip files and uses
#	them to put together a list of the tests that will be run.
#
# Arguments:
#       directory to search
#
# Results:
#       The constructed list is returned to the user.  This will
#	primarily be used in 'all.tcl' files.  It is used in
#	runAllTests.
#
# Side Effects:
#       None

# a lower case version is needed for compatibility with tcltest 1.0
proc tcltest::getMatchingFiles args {eval GetMatchingFiles $args}

proc tcltest::GetMatchingFiles { {searchDirectory ""} } {
    if {[llength [info level 0]] == 1} {
	set searchDirectory [testsDirectory]
    }
    set matchingFiles {}

    # Find the matching files in the list of directories and then remove
    # the ones that match the skip pattern. Passing a list to foreach is
    # required so that a patch like D:\Foo\Bar does not get munged into
    # D:FooBar.
    foreach directory [list $searchDirectory] {
	set matchFileList {}
	foreach match [matchFiles] {
	    set matchFileList [concat $matchFileList \
		    [glob -directory $directory -nocomplain -- $match]]
	}
	if {[string compare {} $tcltest::skipFiles]} {
	    set skipFileList {}
	    foreach skip [skipFiles] {
		set skipFileList [concat $skipFileList \
			[glob -directory $directory \
			-nocomplain -- $skip]]
	    }
	    foreach file $matchFileList {
		# Only include files that don't match the skip pattern
		# and aren't SCCS lock files.
		if {([lsearch -exact $skipFileList $file] == -1) && \
			(![string match l.*.test [file tail $file]])} {
		    lappend matchingFiles $file
		}
	    }
	} else {
	    set matchingFiles [concat $matchingFiles $matchFileList]
	}
    }
    if {[string equal $matchingFiles {}]} {
	PrintError "No test files remain after applying your match and\
		skip patterns!"
    }
    return $matchingFiles
}

# tcltest::GetMatchingDirectories --
#
#	Looks at the patterns given to match and skip directories and
#	uses them to put together a list of the test directories that we
#	should attempt to run.  (Only subdirectories containing an
#	"all.tcl" file are put into the list.)
#
# Arguments:
#	root directory from which to search
#
# Results:
#	The constructed list is returned to the user.  This is used in
#	the primary all.tcl file.
#
# Side Effects:
#       None.

proc tcltest::GetMatchingDirectories {rootdir} {
    set matchingDirs {}
    set matchDirList {}
    # Find the matching directories in testsDirectory and then remove
    # the ones that match the skip pattern
    foreach match [matchDirectories] {
	foreach file [glob -directory $rootdir -nocomplain -- $match] {
	    if {[file isdirectory $file]
		    && [string compare $file $rootdir]} {
		set matchDirList [concat $matchDirList \
			[GetMatchingDirectories $file]]
		if {[file exists [file join $file all.tcl]]} {
		    lappend matchDirList $file
		}
	    }
	}
    }
    if {[llength [skipDirectories]]} {
	set skipDirs {}
	foreach skip [skipDirectories] {
	    set skipDirs [concat $skipDirs \
		[glob -nocomplain -directory [testsDirectory] $skip]]
	}
	foreach dir $matchDirList {
	    # Only include directories that don't match the skip pattern
	    if {[lsearch -exact $skipDirs $dir] == -1} {
		lappend matchingDirs $dir
	    }
	}
    } else {
	set matchingDirs $matchDirList
    }
    if {[llength $matchingDirs] == 0} {
	DebugPuts 1 "No test directories remain after applying match\
		and skip patterns!"
    }
    return $matchingDirs
}

# tcltest::runAllTests --
#
#	prints output and sources test files according to the match and
#	skip patterns provided.  after sourcing test files, it goes on
#	to source all.tcl files in matching test subdirectories.
#
# Arguments:
#	shell being tested
#
# Results:
#	None.
#
# Side effects:
#	None.

proc tcltest::runAllTests { {shell ""} } {
    global argv
    variable testSingleFile
    variable numTestFiles
    variable numTests
    variable failFiles

    if {[llength [info level 0]] == 1} {
	set shell [interpreter]
    }

    set testSingleFile false

    puts [outputChannel] "Tests running in interp:  $shell"
    puts [outputChannel] "Tests located in:  [testsDirectory]"
    puts [outputChannel] "Tests running in:  [workingDirectory]"
    puts [outputChannel] "Temporary files stored in\
	    [temporaryDirectory]"

    # [file system] first available in Tcl 8.4
    if {![catch {file system [testsDirectory]} result]
	    && ![string equal native [lindex $result 0]]} {
	# If we aren't running in the native filesystem, then we must
	# run the tests in a single process (via 'source'), because
	# trying to run then via a pipe will fail since the files don't
	# really exist.
	singleProcess 1
    }

    if {[singleProcess]} {
	puts [outputChannel] \
		"Test files sourced into current interpreter"
    } else {
	puts [outputChannel] \
		"Test files run in separate interpreters"
    }
    if {[llength [skip]] > 0} {
	puts [outputChannel] "Skipping tests that match:  [skip]"
    }
    if {[llength [match]] > 0} {
	puts [outputChannel] "Only running tests that match:  [match]"
    }

    if {[llength [skipFiles]] > 0} {
	puts [outputChannel] \
		"Skipping test files that match:  [skipFiles]"
    }
    if {[llength [matchFiles]] > 0} {
	puts [outputChannel] \
		"Only running test files that match:  [matchFiles]"
    }

    set timeCmd {clock format [clock seconds]}
    puts [outputChannel] "Tests began at [eval $timeCmd]"

    # Run each of the specified tests
    foreach file [lsort [GetMatchingFiles]] {
	set tail [file tail $file]
	puts [outputChannel] $tail

	if {[singleProcess]} {
	    incr numTestFiles
	    uplevel 1 [list ::source $file]
	} else {
	    set cmd [linsert $argv 0 | $shell $file]
	    if {[catch {
		incr numTestFiles
		set pipeFd [open $cmd "r"]
		while {[gets $pipeFd line] >= 0} {
		    if {[regexp [join {
			    {^([^:]+):\t}
			    {Total\t([0-9]+)\t}
			    {Passed\t([0-9]+)\t}
			    {Skipped\t([0-9]+)\t}
			    {Failed\t([0-9]+)}
			    } ""] $line null testFile \
			    Total Passed Skipped Failed]} {
			foreach index {Total Passed Skipped Failed} {
			    incr numTests($index) [set $index]
			}
			if {$Failed > 0} {
			    lappend failFiles $testFile
			}
		    } elseif {[regexp [join {
			    {^Number of tests skipped }
			    {for each constraint:}
			    {|^\t(\d+)\t(.+)$}
			    } ""] $line match skipped constraint]} {
			if {[string match \t* $match]} {
			    AddToSkippedBecause $constraint $skipped
			}
		    } else {
			puts [outputChannel] $line
		    }
		}
		close $pipeFd
	    } msg]} {
		puts [outputChannel] "Test file error: $msg"
		# append the name of the test to a list to be reported
		# later
		lappend testFileFailures $file
	    }
	}
    }

    # cleanup
    puts [outputChannel] "\nTests ended at [eval $timeCmd]"
    cleanupTests 1
    if {[info exists testFileFailures]} {
	puts [outputChannel] "\nTest files exiting with errors:  \n"
	foreach file $testFileFailures {
	    puts "  [file tail $file]\n"
	}
    }

    # Checking for subdirectories in which to run tests
    foreach directory [GetMatchingDirectories [testsDirectory]] {
	set dir [file tail $directory]
	puts [outputChannel] [string repeat ~ 44]
	puts [outputChannel] "$dir test began at [eval $timeCmd]\n"
	
	uplevel 1 [list ::source [file join $directory all.tcl]]
	
	set endTime [eval $timeCmd]
	puts [outputChannel] "\n$dir test ended at $endTime"
	puts [outputChannel] ""
	puts [outputChannel] [string repeat ~ 44]
    }
    return
}

#####################################################################

# Test utility procs - not used in tcltest, but may be useful for
# testing.

# tcltest::loadTestedCommands --
#
#     Uses the specified script to load the commands to test. Allowed to
#     be empty, as the tested commands could have been compiled into the
#     interpreter.
#
# Arguments
#     none
#
# Results
#     none
#
# Side Effects:
#     none.

proc tcltest::loadTestedCommands {} {
    variable l
    if {[string equal {} [loadScript]]} {
	return
    }

    return [uplevel 1 [loadScript]]
}

# tcltest::saveState --
#
#	Save information regarding what procs and variables exist.
#
# Arguments:
#	none
#
# Results:
#	Modifies the variable saveState
#
# Side effects:
#	None.

proc tcltest::saveState {} {
    variable saveState
    uplevel 1 [list ::set [namespace which -variable saveState]] \
	    {[::list [::info procs] [::info vars]]}
    DebugPuts  2 "[lindex [info level 0] 0]: $saveState"
    return
}

# tcltest::restoreState --
#
#	Remove procs and variables that didn't exist before the call to
#       [saveState].
#
# Arguments:
#	none
#
# Results:
#	Removes procs and variables from your environment if they don't
#	exist in the saveState variable.
#
# Side effects:
#	None.

proc tcltest::restoreState {} {
    variable saveState
    foreach p [uplevel 1 {::info procs}] {
	if {([lsearch [lindex $saveState 0] $p] < 0)
		&& ![string equal [namespace current]::$p \
		[uplevel 1 [list ::namespace origin $p]]]} {

	    DebugPuts 2 "[lindex [info level 0] 0]: Removing proc $p"
	    uplevel 1 [list ::catch [list ::rename $p {}]]
	}
    }
    foreach p [uplevel 1 {::info vars}] {
	if {[lsearch [lindex $saveState 1] $p] < 0} {
	    DebugPuts 2 "[lindex [info level 0] 0]:\
		    Removing variable $p"
	    uplevel 1 [list ::catch [list ::unset $p]]
	}
    }
    return
}

# tcltest::normalizeMsg --
#
#	Removes "extra" newlines from a string.
#
# Arguments:
#	msg        String to be modified
#
# Results:
#	string with extra newlines removed
#
# Side effects:
#	None.

proc tcltest::normalizeMsg {msg} {
    regsub "\n$" [string tolower $msg] "" msg
    regsub -all "\n\n" $msg "\n" msg
    regsub -all "\n\}" $msg "\}" msg
    return $msg
}

# tcltest::makeFile --
#
# Create a new file with the name <name>, and write <contents> to it.
#
# If this file hasn't been created via makeFile since the last time
# cleanupTests was called, add it to the $filesMade list, so it will be
# removed by the next call to cleanupTests.
#
# Arguments:
#	contents        content of the new file
#       name            name of the new file
#       directory       directory name for new file
#
# Results:
#	absolute path to the file created
#
# Side effects:
#	None.

proc tcltest::makeFile {contents name {directory ""}} {
    global tcl_platform
    variable filesMade

    if {[llength [info level 0]] == 3} {
	set directory [temporaryDirectory]
    }

    set fullName [file join $directory $name]

    DebugPuts 3 "[lindex [info level 0] 0]:\
	     putting $contents into $fullName"

    set fd [open $fullName w]

    fconfigure $fd -translation lf

    if {[string equal [string index $contents end] "\n"]} {
	puts -nonewline $fd $contents
    } else {
	puts $fd $contents
    }
    close $fd

    if {[lsearch -exact $filesMade $fullName] == -1} {
	lappend filesMade $fullName
    }
    return $fullName
}

# tcltest::removeFile --
#
#	Removes the named file from the filesystem
#
# Arguments:
#	name          file to be removed
#       directory     directory from which to remove file
#
# Results:
#	return value from [file delete]
#
# Side effects:
#	None.

proc tcltest::removeFile {name {directory ""}} {
    if {[llength [info level 0]] == 2} {
	set directory [temporaryDirectory]
    }
    set fullName [file join $directory $name]
    DebugPuts 3 "[lindex [info level 0] 0]: removing $fullName"
    return [file delete $fullName]
}

# tcltest::makeDirectory --
#
# Create a new dir with the name <name>.
#
# If this dir hasn't been created via makeDirectory since the last time
# cleanupTests was called, add it to the $directoriesMade list, so it
# will be removed by the next call to cleanupTests.
#
# Arguments:
#       name            name of the new directory
#       directory       directory in which to create new dir
#
# Results:
#	absolute path to the directory created
#
# Side effects:
#	None.

proc tcltest::makeDirectory {name {directory ""}} {
    variable filesMade
    if {[llength [info level 0]] == 2} {
	set directory [temporaryDirectory]
    }
    set fullName [file join $directory $name]
    DebugPuts 3 "[lindex [info level 0] 0]: creating $fullName"
    file mkdir $fullName
    if {[lsearch -exact $filesMade $fullName] == -1} {
	lappend filesMade $fullName
    }
    return $fullName
}

# tcltest::removeDirectory --
#
#	Removes a named directory from the file system.
#
# Arguments:
#	name          Name of the directory to remove
#       directory     Directory from which to remove
#
# Results:
#	return value from [file delete]
#
# Side effects:
#	None

proc tcltest::removeDirectory {name {directory ""}} {
    if {[llength [info level 0]] == 2} {
	set directory [temporaryDirectory]
    }
    set fullName [file join $directory $name]
    DebugPuts 3 "[lindex [info level 0] 0]: deleting $fullName"
    return [file delete -force $fullName]
}

# tcltest::viewFile --
#
#	reads the content of a file and returns it
#
# Arguments:
#	name of the file to read
#       directory in which file is located
#
# Results:
#	content of the named file
#
# Side effects:
#	None.

proc tcltest::viewFile {name {directory ""}} {
    global tcl_platform
    if {[llength [info level 0]] == 2} {
	set directory [temporaryDirectory]
    }
    set fullName [file join $directory $name]
    if {[string equal $tcl_platform(platform) macintosh]
	    || ![testConstraint unixExecs]} {
	set f [open $fullName]
	set data [read -nonewline $f]
	close $f
	return $data
    } else {
	return [exec cat $fullName]
    }
    return
}

# tcltest::bytestring --
#
# Construct a string that consists of the requested sequence of bytes,
# as opposed to a string of properly formed UTF-8 characters.
# This allows the tester to
# 1. Create denormalized or improperly formed strings to pass to C
#    procedures that are supposed to accept strings with embedded NULL
#    bytes.
# 2. Confirm that a string result has a certain pattern of bytes, for
#    instance to confirm that "\xe0\0" in a Tcl script is stored
#    internally in UTF-8 as the sequence of bytes "\xc3\xa0\xc0\x80".
#
# Generally, it's a bad idea to examine the bytes in a Tcl string or to
# construct improperly formed strings in this manner, because it involves
# exposing that Tcl uses UTF-8 internally.
#
# Arguments:
#	string being converted
#
# Results:
#	result fom encoding
#
# Side effects:
#	None

proc tcltest::bytestring {string} {
    return [encoding convertfrom identity $string]
}

# tcltest::OpenFiles --
#
#	used in io tests, uses testchannel
#
# Arguments:
#	None.
#
# Results:
#	???
#
# Side effects:
#	None.

proc tcltest::OpenFiles {} {
    if {[catch {testchannel open} result]} {
	return {}
    }
    return $result
}

# tcltest::LeakFiles --
#
#	used in io tests, uses testchannel
#
# Arguments:
#	None.
#
# Results:
#	???
#
# Side effects:
#	None.

proc tcltest::LeakFiles {old} {
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

#
# Internationalization / ISO support procs     -- dl
#

# tcltest::SetIso8859_1_Locale --
#
#	used in cmdIL.test, uses testlocale
#
# Arguments:
#	None.
#
# Results:
#	None.
#
# Side effects:
#	None.

proc tcltest::SetIso8859_1_Locale {} {
    variable previousLocale
    variable isoLocale
    if {[info commands testlocale] != ""} {
	set previousLocale [testlocale ctype]
	testlocale ctype $isoLocale
    }
    return
}

# tcltest::RestoreLocale --
#
#	used in cmdIL.test, uses testlocale
#
# Arguments:
#	None.
#
# Results:
#	None.
#
# Side effects:
#	None.

proc tcltest::RestoreLocale {} {
    variable previousLocale
    if {[info commands testlocale] != ""} {
	testlocale ctype $previousLocale
    }
    return
}

# tcltest::threadReap --
#
#	Kill all threads except for the main thread.
#	Do nothing if testthread is not defined.
#
# Arguments:
#	none.
#
# Results:
#	Returns the number of existing threads.
#
# Side Effects:
#       none.
#

proc tcltest::threadReap {} {
    if {[info commands testthread] != {}} {

	# testthread built into tcltest

	testthread errorproc ThreadNullError
	while {[llength [testthread names]] > 1} {
	    foreach tid [testthread names] {
		if {$tid != [mainThread]} {
		    catch {
			testthread send -async $tid {testthread exit}
		    }
		}
	    }
	    ## Enter a bit a sleep to give the threads enough breathing
	    ## room to kill themselves off, otherwise the end up with a
	    ## massive queue of repeated events
	    after 1
	}
	testthread errorproc ThreadError
	return [llength [testthread names]]
    } elseif {[info commands thread::id] != {}} {
	
	# Thread extension

	thread::errorproc ThreadNullError
	while {[llength [thread::names]] > 1} {
	    foreach tid [thread::names] {
		if {$tid != [mainThread]} {
		    catch {thread::send -async $tid {thread::exit}}
		}
	    }
	    ## Enter a bit a sleep to give the threads enough breathing
	    ## room to kill themselves off, otherwise the end up with a
	    ## massive queue of repeated events
	    after 1
	}
	thread::errorproc ThreadError
	return [llength [thread::names]]
    } else {
	return 1
    }
    return 0
}

# Initialize the constraints and set up command line arguments
namespace eval tcltest {
    # Define initializers for all the built-in contraint definitions
    DefineConstraintInitializers

    # Set up the constraints in the testConstraints array to be lazily
    # initialized by a registered initializer, or by "false" if no
    # initializer is registered.
    trace variable testConstraints r [namespace code SafeFetch]

    # Only initialize constraints at package load time if an
    # [initConstraintsHook] has been pre-defined.  This is only
    # for compatibility support.  The modern way to add a custom
    # test constraint is to just call the [testConstraint] command
    # straight away, without all this "hook" nonsense.
    if {[string equal [namespace current] \
	    [namespace qualifiers [namespace which initConstraintsHook]]]} {
	InitConstraints
    } else {
	proc initConstraintsHook {} {}
    }
    ProcessCmdLineArgs

    # Save the names of files that already exist in
    # the output directory.
    variable file {}
    foreach file [glob -nocomplain -directory [temporaryDirectory] *] {
	lappend filesExisted [file tail $file]
    }

    # Define the standard match commands
    customMatch exact	[list string equal]
    customMatch glob	[list string match]
    customMatch regexp	[list regexp --]
    unset file
}

package provide tcltest 2.1

