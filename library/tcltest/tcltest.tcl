# tcltest.tcl --
#
#	This file contains support code for the Tcl test suite.  It 
#       defines the tcltest namespace and finds and defines the output
#       directory, constraints available, output and error channels, etc. used
#       by Tcl tests.  See the tcltest man page for more details.
#       
#       This design was based on the Tcl testing approach designed and
#       initially implemented by Mary Ann May-Pumphrey of Sun Microsystems. 
#
# Copyright (c) 1994-1997 Sun Microsystems, Inc.
# Copyright (c) 1998-1999 by Scriptics Corporation.
# Copyright (c) 2000 by Ajuba Solutions
# All rights reserved.
# 
# RCS: @(#) $Id: tcltest.tcl,v 1.42 2002/03/25 22:05:03 dgp Exp $

# create the "tcltest" namespace for all testing variables and procedures

package require Tcl 8.3

namespace eval tcltest { 

    # Export the public tcltest procs
    namespace export bytestring cleanupTests debug errorChannel errorFile \
	    interpreter limitConstraints loadFile loadScript \
	    loadTestedCommands mainThread makeDirectory makeFile match \
	    matchDirectories matchFiles normalizeMsg normalizePath \
	    outputChannel outputFile preserveCore removeDirectory removeFile \
	    restoreState runAllTests saveState singleProcess skip \
	    skipDirectories skipFiles temporaryDirectory test testConstraint \
	    testsDirectory threadReap verbose viewFile workingDirectory

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

    # tcltest::verbose defaults to {body}
    Default verbose body

    # Match and skip patterns default to the empty list, except for
    # matchFiles, which defaults to all .test files in the testsDirectory and
    # matchDirectories, which defaults to all directories.
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
    # up only the tests that were skipped because they didn't match or were 
    # specifically skipped.  A debug level of 2 would spit up the tcltest
    # variables and flags provided; a debug level of 3 causes some additional
    # output regarding operations of the test harness.  The tcltest package
    # currently implements only up to debug level 3.
    Default debug 0

    # Save any arguments that we might want to pass through to other programs. 
    # This is used by the -args flag.
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
    # tcltest::filesMade keeps track of such files created using the
    # tcltest::makeFile and tcltest::makeDirectory procedures.
    # tcltest::filesExisted stores the names of pre-existing files.
    Default filesMade {}
    Default filesExisted {}

    # tcltest::numTests will store test files as indices and the list
    # of files (that should not have been) left behind by the test files.
    ArrayDefault createdNewFiles {}

    # initialize tcltest::numTests array to keep track fo the number of
    # tests that pass, fail, and are skipped.
    ArrayDefault numTests [list Total 0 Passed 0 Skipped 0 Failed 0] 

    # initialize tcltest::skippedBecause array to keep track of
    # constraints that kept tests from running; a constraint name of
    # "userSpecifiedSkip" means that the test appeared on the list of tests
    # that matched the -skip value given to the flag; "userSpecifiedNonMatch"
    # means that the test didn't match the argument given to the -match flag;
    # both of these constraints are counted only if tcltest::debug is set to
    # true. 
    ArrayDefault skippedBecause {}

    # initialize the tcltest::testConstraints array to keep track of valid
    # predefined constraints (see the explanation for the
    # tcltest::initConstraints proc for more details).
    ArrayDefault testConstraints {}
    Default constraintsSpecified {}

    # Don't run only the constrained tests by default
    Default limitConstraints false

    # A test application has to know how to load the tested commands into
    # the interpreter.
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

    # Set tcltest::workingDirectory to [pwd]. The default output directory
    # for Tcl tests is the working directory.
    Default workingDirectory [pwd]
    Default temporaryDirectory $workingDirectory

    # Tests should not rely on the current working directory.
    # Files that are part of the test suite should be accessed relative to 
    # tcltest::testsDirectory.

    if {![info exists [namespace current]::testsDirectory]} {
	variable oldpwd [pwd]
	catch {cd [file join [file dirname [info script]] .. .. tests]}
	variable testsDirectory [pwd]
	cd $oldpwd
	unset oldpwd
    }

    # Default is to run each test file in a separate process
    Default singleProcess 0

    # the variables and procs that existed when tcltest::saveState was
    # called are stored in a variable of the same name
    Default saveState {}

    # Internationalization support -- used in tcltest::set_iso8859_1_locale
    # and tcltest::restore_locale. Those commands are used in cmdIL.test.

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

			# Works on SunOS 4 and Solaris, and maybe others...
			# define it to something else on your system
			#if you want to test those.

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
	Default coreModificationTime \
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
	uplevel $script
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
	set msg "$errMsg \"$dir\" is not a directory"
	error $msg
    } elseif {([string first w $rw] >= 0) && ![file writable $dir]} {
	set msg "$errMsg \"$dir\" is not writeable"
	error $msg
    } elseif {([string first r $rw] >= 0) && ![file readable $dir]} {
	set msg "$errMsg \"$dir\" is not readable"
	error $msg
    }
    return
}

# tcltest::normalizePath --
#
#     This procedure resolves any symlinks in the path thus creating a
#     path without internal redirection. It assumes that the incoming
#     path is absolute.
#
# Arguments
#     pathVar contains the name of the variable containing the path to modify.
#
# Results
#     The path is modified in place.
#
# Side Effects:
#     None.
#

proc tcltest::normalizePath {pathVar} {
    upvar $pathVar path

    set oldpwd [pwd]
    catch {cd $path}
    set path [pwd]
    cd $oldpwd
    return $path
}


# tcltest::MakeAbsolutePath --
#
#     This procedure checks whether the incoming path is absolute or not.
#     Makes it absolute if it was not.
#
# Arguments
#     pathVar contains the name of the variable containing the path to modify.
#     prefix  is optional, contains the path to use to make the other an
#             absolute one. The current working directory is used if it was
#             not specified.
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
	if {$prefix == {}} {
	    set prefix [pwd]
	}

	set path [file join $prefix $path] 
    }
    return $path
}

#####################################################################

# tcltest::<variableName>
#     
#     Accessor functions for tcltest variables that can be modified externally.
#     These are vars that could otherwise be modified using command line
#     arguments to tcltest.

# tcltest::verbose --
#
#	Set or return the verbosity level (tcltest::verbose) for tests.  This
#       determines what gets printed to the screen and when, with regard to the
#       running of the tests.  The proc does not check for invalid values.  It
#       assumes that a string that doesn't match its predefined keywords
#       is a string containing letter-specified verbosity levels.
#
# Arguments:
#	A string containing any combination of 'pbste' or a list of keywords
#       (listed in parens)
#          p = print output whenever a test passes (pass)
#          b = print the body of the test when it fails (body)
#          s = print when a test is skipped (skip)
#          t = print when a test starts (start)
#          e = print errorInfo and errorCode when a test encounters an error
#              (error) 
#
# Results:
#	content of tcltest::verbose - this is always the character combination
#       (pbste) instead of the list form.
#
# Side effects:
#	None.

proc tcltest::verbose { {level ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::verbose
    } 
    if {[llength $level] > 1} {
  	set tcltest::verbose $level
    } else {
	if {[regexp {pass|body|skip|start|error} $level]} {
	    set tcltest::verbose $level
	} else { 
	    set levelList [split $level {}]
	    set tcltest::verbose [string map {p pass b body s skip t start e
		error} $levelList]
	}
    }
    return $tcltest::verbose
}

# tcltest::isVerbose --
#
#	Returns true if argument is one of the verbosity levels currently being
#       used; returns false otherwise.
#
# Arguments:
#	level
#
# Results:
#	boolean 1 (true) or 0 (false), depending on whether or not the level
#       provided is one of the ones stored in tcltest::verbose.
#
# Side effects:
#	None.

proc tcltest::isVerbose {level} {
    if {[lsearch -exact [tcltest::verbose] $level] == -1} {
	return 0
    }
    return 1
}



# tcltest::match --
#
#	Set or return the match patterns (tcltest::match) that determine which
#       tests are run. 
#
# Arguments:
#	List containing match patterns (glob format)
#
# Results:
#	content of tcltest::match
#
# Side effects:
#	none

proc tcltest::match { {matchList ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::match
    } 
    set tcltest::match $matchList
}

# tcltest::skip --
#
#	Set or return the skip patterns (tcltest::skip) that determine which
#       tests are skipped. 
#
# Arguments:
#	List containing skip patterns (glob format)
#
# Results:
#	content of tcltest::skip
#
# Side effects:
#	None.

proc tcltest::skip { {skipList ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::skip
    }
    set tcltest::skip $skipList
}

# tcltest::matchFiles --
#
#	set or return the match patterns for file sourcing
#
# Arguments:
#	list containing match file list (glob format)
#
# Results:
#	content of tcltest::matchFiles
#
# Side effects:
#	None.

proc tcltest::matchFiles { {matchFileList ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::matchFiles
    }
    set tcltest::matchFiles $matchFileList
}

# tcltest::skipFiles --
#
#	set or return the skip patterns for file sourcing
#
# Arguments:
#	list containing the skip file list (glob format)
#
# Results:
#	content of tcltest::skipFiles
#
# Side effects:
#	None.

proc tcltest::skipFiles { {skipFileList ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::skipFiles
    }
    set tcltest::skipFiles $skipFileList
}


# tcltest::matchDirectories --
#
#	set or return the list of directories for matching (glob pattern list)
#
# Arguments:
#	list of glob patterns matching subdirectories of
#       tcltest::testsDirectory 
#
# Results:
#	content of tcltest::matchDirectories
#
# Side effects:
#	None.

proc tcltest::matchDirectories { {dirlist ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::matchDirectories
    }
    set tcltest::matchDirectories $dirlist
}

# tcltest::skipDirectories --
#
#	set or return the list of directories to skip (glob pattern list)
#
# Arguments:
#	list of glob patterns matching directories to skip; these directories
#       are subdirectories of tcltest::testsDirectory
#
# Results:
#	content of tcltest::skipDirectories
#
# Side effects:
#	None.

proc tcltest::skipDirectories { {dirlist ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::skipDirectories
    }
    set tcltest::skipDirectories $dirlist
}

# tcltest::preserveCore --
#
#	set or return the core preservation level.  This proc does not do any
#       error checking for invalid values.
#
# Arguments:
#	core level:
#         '0' = don't do anything with core files (default)
#         '1' = notify the user if core files are created
#         '2' = save any core files produced during testing to
#               tcltest::temporaryDirectory 
#
# Results:
#	content of tcltest::preserveCore
#
# Side effects:
#	None.

proc tcltest::preserveCore { {coreLevel ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::preserveCore
    }
    set tcltest::preserveCore $coreLevel
}

# tcltest::outputChannel --
#
#	set or return the output file descriptor based on the supplied file
#       name (where tcltest puts all of its output) 
#
# Arguments:
#	output file descriptor
#
# Results:
#	file descriptor corresponding to supplied file name (or currently set
#       file descriptor, if no new filename was supplied) - this is the content
#       of tcltest::outputChannel 
#
# Side effects:
#	None.

proc tcltest::outputChannel { {filename ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::outputChannel
    }
    if {($filename == "stderr") || ($filename == "stdout")} {
	set tcltest::outputChannel $filename
    } else {
	set tcltest::outputChannel [open $filename w]
    }
    return $tcltest::outputChannel
}

# tcltest::outputFile --
#
#	set or return the output file name (where tcltest puts all of its
#       output); calls tcltest::outputChannel to set the corresponding file
#       descriptor 
#
# Arguments:
#	output file name
#
# Results:
#       file name corresponding to supplied file name (or currently set
#       file name, if no new filename was supplied) - this is the content
#       of tcltest::outputFile
#
# Side effects:
#	if the file name supplied is relative, it will be made absolute with
#       respect to the predefined temporaryDirectory

proc tcltest::outputFile { {filename ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::outputFile
    }
    if {($filename != "stderr") && ($filename != "stdout")} {
	MakeAbsolutePath filename $tcltest::temporaryDirectory
    }
    tcltest::outputChannel $filename
    set tcltest::outputFile $filename
}

# tcltest::errorChannel --
#
#	set or return the error file descriptor based on the supplied file name
#       (where tcltest sends all its errors)  
#
# Arguments:
#	error file name
#
# Results:
#	file descriptor corresponding to the supplied file name (or currently
#       set file descriptor, if no new filename was supplied) - this is the
#       content of tcltest::errorChannel
#
# Side effects:
#	opens the descriptor in w mode unless the filename is set to stderr or
#       stdout 

proc tcltest::errorChannel { {filename ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::errorChannel
    }
    if {($filename == "stderr") || ($filename == "stdout")} {
	set tcltest::errorChannel $filename
    } else {
	set tcltest::errorChannel [open $filename w]
    }
    return $tcltest::errorChannel
}

# tcltest::errorFile --
#
#	set or return the error file name; calls tcltest::errorChannel to set
#       the corresponding file descriptor
#
# Arguments:
#	error file name
#
# Results:
#	content of tcltest::errorFile
#
# Side effects:
#	if the file name supplied is relative, it will be made absolute with
#       respect to the predefined temporaryDirectory

proc tcltest::errorFile { {filename ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::errorFile
    }
    if {($filename != "stderr") && ($filename != "stdout")} {
	MakeAbsolutePath filename $tcltest::temporaryDirectory
    }
    set tcltest::errorFile $filename
    errorChannel $tcltest::errorFile
    return $tcltest::errorFile
}

# tcltest::debug --
#
#	set or return the debug level for tcltest; this proc does not check for
#       invalid values
#
# Arguments:
#	debug level:
#       '0' = no debug output (default)
#       '1' = skipped tests
#       '2' = tcltest variables and supplied flags
#       '3' = harness operations
#
# Results:
#	content of tcltest::debug
#
# Side effects:
#	None.

proc tcltest::debug { {debugLevel ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::debug
    }
    set tcltest::debug $debugLevel
}

# tcltest::testConstraint --
#
#	sets a test constraint to a value; to do multiple constraints, call
#       this proc multiple times.  also returns the value of the named
#       constraint if no value was supplied.
#
# Arguments:
#	constraint - name of the constraint
#       value - new value for constraint (should be boolean) - if not supplied,
#               this is a query
#
# Results:
#	content of tcltest::testConstraints($constraint)
#
# Side effects:
#	appends the constraint name to tcltest::constraintsSpecified

proc tcltest::testConstraint {constraint {value ""}} {
    DebugPuts 3 "entering testConstraint $constraint $value"
    if {[llength [info level 0]] == 2} {
	return $tcltest::testConstraints($constraint)
    } 
    lappend tcltest::constraintsSpecified $constraint
    set tcltest::testConstraints($constraint) $value
}

# tcltest::constraintsSpecified --
#
#	returns a list of all the constraint names specified using
#       testConstraint 
#
# Arguments:
#	None.
#
# Results:
#	list of the constraint names in tcltest::constraintsSpecified
#
# Side effects:
#	None.

proc tcltest::constraintsSpecified {} {
    return $tcltest::constraintsSpecified
}

# tcltest::constraintList --
#
#	returns a list of all the constraint names
#
# Arguments:
#	None.
#
# Results:
#	list of the constraint names in tcltest::testConstraints
#
# Side effects:
#	None.

proc tcltest::constraintList {} {
    return [array names tcltest::testConstraints]
}

# tcltest::limitConstraints --
#
#	sets the limited constraints to tcltest::limitConstraints
#
# Arguments:
#	list of constraint names
#
# Results:
#	content of tcltest::limitConstraints
#
# Side effects:
#	None.

proc tcltest::limitConstraints { {constraintList ""} } {
    DebugPuts 3 "entering limitConstraints $constraintList"
    if {[llength [info level 0]] == 1} {
	return $tcltest::limitConstraints
    } 
    set tcltest::limitConstraints $constraintList
    foreach elt [tcltest::constraintList] {
	if {[lsearch -exact [tcltest::constraintsSpecified] $elt] == -1} {
	    tcltest::testConstraint $elt 0
	}
    }
    return $tcltest::limitConstraints
}

# tcltest::loadScript --
#
#	sets the load script
#
# Arguments:
#	script to be set
#
# Results:
#	contents of tcltest::loadScript
#
# Side effects:
#	None.

proc tcltest::loadScript { {script ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::loadScript
    }
    set tcltest::loadScript $script
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
    if {[llength [info level 0]] == 1} {
	return $tcltest::loadFile
    }
    MakeAbsolutePath scriptFile $tcltest::temporaryDirectory
    set tmp [open $scriptFile r]
    tcltest::loadScript [read $tmp]
    close $tmp
    set tcltest::loadFile $scriptFile
}

# tcltest::workingDirectory --
#
#	set workingDirectory to the given path. 
#       If the path is relative, make it absolute.
#       change directory to the stated working directory, if resetting the
#       value 
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
    if {[llength [info level 0]] == 1} {
	return $tcltest::workingDirectory
    }
    set tcltest::workingDirectory $dir
    MakeAbsolutePath tcltest::workingDirectory
    cd $tcltest::workingDirectory
    return $tcltest::workingDirectory
}

# tcltest::temporaryDirectory --
#
#     Set tcltest::temporaryDirectory to the given path.
#     If the path is relative, make it absolute.  If the file exists but
#     is not a dir, then return an error.
#
#     If tcltest::temporaryDirectory does not already exist, create it.
#     If you cannot create it, then return an error (the file mkdir isn't
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
    if {[llength [info level 0]] == 1} {
	return $tcltest::temporaryDirectory
    }
    set tcltest::temporaryDirectory $dir

    MakeAbsolutePath tcltest::temporaryDirectory
    set tmpDirError "bad argument for temporary directory: "

    if {[file exists $tcltest::temporaryDirectory]} {
	tcltest::CheckDirectory rw $tcltest::temporaryDirectory $tmpDirError
    } else {
	file mkdir $tcltest::temporaryDirectory
    }

    normalizePath tcltest::temporaryDirectory
}

# tcltest::testsDirectory --
#
#     Set tcltest::testsDirectory to the given path.
#     If the path is relative, make it absolute.  If the file exists but
#     is not a dir, then return an error.
#
#     If tcltest::testsDirectory does not already exist, return an error.
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
    if {[llength [info level 0]] == 1} {
	return $tcltest::testsDirectory
    }

    set tcltest::testsDirectory $dir
    
    MakeAbsolutePath tcltest::testsDirectory
    set testDirError "bad argument for tests directory: "

    if {[file exists $tcltest::testsDirectory]} {
	tcltest::CheckDirectory r $tcltest::testsDirectory $testDirError
    } else {
	set msg "$testDirError \"$tcltest::testsDirectory\" does not exist"
	error $msg
    }
    
    normalizePath tcltest::testsDirectory
}

# tcltest::singleProcess --
#
#	sets tcltest::singleProcess to the value provided.
#
# Arguments:
#	value for singleProcess:
#       1 = source each test file into the current process
#       0 = run each test file in its own process 
#
# Results:
#	content of tcltest::singleProcess
#
# Side effects:
#	None.

proc tcltest::singleProcess { {value ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::singleProcess
    }
    set tcltest::singleProcess $value
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
    if {$interp == "{}"} {
	set tcltest {}
    } else {
	set tcltest $interp
    }
}

# tcltest::mainThread --
#
#	sets or returns the thread id stored in tcltest::mainThread
#
# Arguments:
#	thread id
#
# Results:
#	content of tcltest::mainThread
#
# Side effects:
#	None.

proc tcltest::mainThread { {threadid ""} } {
    if {[llength [info level 0]] == 1} {
	return $tcltest::mainThread
    }
    set tcltest::mainThread $threadid
}

#####################################################################

# tcltest::AddToSkippedBecause --
#
#	Increments the variable used to track how many tests were skipped
#       because of a particular constraint.
#
# Arguments:
#	constraint     The name of the constraint to be modified
#
# Results:
#	Modifies tcltest::skippedBecause; sets the variable to 1 if didn't
#       previously exist - otherwise, it just increments it.
#
# Side effects:
#	None.

proc tcltest::AddToSkippedBecause { constraint {value 1}} {
    # add the constraint to the list of constraints that kept tests
    # from running

    if {[info exists tcltest::skippedBecause($constraint)]} {
	incr tcltest::skippedBecause($constraint) $value
    } else {
	set tcltest::skippedBecause($constraint) $value
    }
    return
}

# tcltest::PrintError --
#
#	Prints errors to tcltest::errorChannel and then flushes that
#       channel, making sure that all messages are < 80 characters per line.
#
# Arguments:
#	errorMsg     String containing the error to be printed
#
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

    if {$endingIndex < 80} {
	puts [errorChannel] $errorMsg
    } else {
	# Print up to 80 characters on the first line, including the
	# InitialMessage. 
	set beginningIndex [string last " " [string range $errorMsg 0 \
		[expr {80 - $InitialMsgLen}]]]
	puts [errorChannel] [string range $errorMsg 0 $beginningIndex]

	while {$beginningIndex != "end"} {
	    puts -nonewline [errorChannel] \
		    [string repeat " " $InitialMsgLen]  
	    if {[expr {$endingIndex - $beginningIndex}] < 72} {
		puts [errorChannel] [string trim \
			[string range $errorMsg $beginningIndex end]]
		set beginningIndex end
	    } else {
		set newEndingIndex [expr [string last " " [string range \
			$errorMsg $beginningIndex \
			[expr {$beginningIndex + 72}]]] + $beginningIndex]
		if {($newEndingIndex <= 0) \
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

if {[llength [info commands tcltest::initConstraintsHook]] == 0} {
    proc tcltest::initConstraintsHook {} {}
}

# tcltest::safeFetch --
#
#	 The following trace procedure makes it so that we can safely refer to
#        non-existent members of the tcltest::testConstraints array without 
#        causing an error.  Instead, reading a non-existent member will return
#        0. This is necessary because tests are allowed to use constraint "X"
#        without ensuring that tcltest::testConstraints("X") is defined.
#
# Arguments:
#	n1 - name of the array (tcltest::testConstraints)
#       n2 - array key value (constraint name)
#       op - operation performed on tcltest::testConstraints (generally r)
#
# Results:
#	none
#
# Side effects:
#	sets tcltest::testConstraints($n2) to 0 if it's referenced but never
#       before used

proc tcltest::safeFetch {n1 n2 op} {
    DebugPuts 3 "entering safeFetch $n1 $n2 $op"
    if {($n2 != {}) && ([info exists tcltest::testConstraints($n2)] == 0)} {
	tcltest::testConstraint $n2 0
    }
}

# tcltest::initConstraints --
#
# Check constraint information that will determine which tests
# to run.  To do this, create an array tcltest::testConstraints.  Each
# element has a 0 or 1 value.  If the element is "true" then tests
# with that constraint will be run, otherwise tests with that constraint
# will be skipped.  See the tcltest man page for the list of built-in
# constraints defined in this procedure.
#
# Arguments:
#	none
#
# Results:
#	The tcltest::testConstraints array is reset to have an index for
#	each built-in test constraint.
# 
# Side Effects:
#       None.
#

proc tcltest::initConstraints {} {
    global tcl_platform tcl_interactive tk_version

    # Safely refer to non-existent members of the tcltest::testConstraints
    # array without causing an error.  
    trace variable tcltest::testConstraints r tcltest::safeFetch

    tcltest::initConstraintsHook

    tcltest::testConstraint singleTestInterp [singleProcess]

    # All the 'pc' constraints are here for backward compatibility and are not
    # documented.  They have been replaced with equivalent 'win' constraints.

    tcltest::testConstraint unixOnly \
	    [string equal $tcl_platform(platform) "unix"]
    tcltest::testConstraint macOnly \
	    [string equal $tcl_platform(platform) "macintosh"]
    tcltest::testConstraint pcOnly \
	    [string equal $tcl_platform(platform) "windows"]
    tcltest::testConstraint winOnly \
	    [string equal $tcl_platform(platform) "windows"]

    tcltest::testConstraint unix [tcltest::testConstraint unixOnly]
    tcltest::testConstraint mac [tcltest::testConstraint macOnly]
    tcltest::testConstraint pc [tcltest::testConstraint pcOnly]
    tcltest::testConstraint win [tcltest::testConstraint winOnly]

    tcltest::testConstraint unixOrPc \
	    [expr {[tcltest::testConstraint unix] \
		|| [tcltest::testConstraint pc]}]
    tcltest::testConstraint macOrPc \
	    [expr {[tcltest::testConstraint mac] \
		|| [tcltest::testConstraint pc]}]
    tcltest::testConstraint unixOrWin \
	    [expr {[tcltest::testConstraint unix] \
		|| [tcltest::testConstraint win]}]
    tcltest::testConstraint macOrWin \
	    [expr {[tcltest::testConstraint mac] \
		|| [tcltest::testConstraint win]}]
    tcltest::testConstraint macOrUnix \
	    [expr {[tcltest::testConstraint mac] \
		|| [tcltest::testConstraint unix]}]

    tcltest::testConstraint nt [string equal $tcl_platform(os) "Windows NT"]
    tcltest::testConstraint 95 [string equal $tcl_platform(os) "Windows 95"]
    tcltest::testConstraint 98 [string equal $tcl_platform(os) "Windows 98"]

    # The following Constraints switches are used to mark tests that should
    # work, but have been temporarily disabled on certain platforms because
    # they don't and we haven't gotten around to fixing the underlying
    # problem. 

    tcltest::testConstraint tempNotPc \
	    [expr {![tcltest::testConstraint pc]}]
    tcltest::testConstraint tempNotWin \
	    [expr {![tcltest::testConstraint win]}]
    tcltest::testConstraint tempNotMac \
	    [expr {![tcltest::testConstraint mac]}]
    tcltest::testConstraint tempNotUnix \
	    [expr {![tcltest::testConstraint unix]}]

    # The following Constraints switches are used to mark tests that crash on
    # certain platforms, so that they can be reactivated again when the
    # underlying problem is fixed.

    tcltest::testConstraint pcCrash \
	    [expr {![tcltest::testConstraint pc]}]
    tcltest::testConstraint winCrash \
	    [expr {![tcltest::testConstraint win]}]
    tcltest::testConstraint macCrash \
	    [expr {![tcltest::testConstraint mac]}]
    tcltest::testConstraint unixCrash \
	    [expr {![tcltest::testConstraint unix]}]

    # Skip empty tests

    tcltest::testConstraint emptyTest 0

    # By default, tests that expose known bugs are skipped.

    tcltest::testConstraint knownBug 0

    # By default, non-portable tests are skipped.

    tcltest::testConstraint nonPortable 0

    # Some tests require user interaction.

    tcltest::testConstraint userInteraction 0

    # Some tests must be skipped if the interpreter is not in interactive mode
    
    if {[info exists tcl_interactive]} {
	tcltest::testConstraint interactive $::tcl_interactive
    } else {
	tcltest::testConstraint interactive 0
    }

    # Some tests can only be run if the installation came from a CD image
    # instead of a web image
    # Some tests must be skipped if you are running as root on Unix.
    # Other tests can only be run if you are running as root on Unix.

    tcltest::testConstraint root 0
    tcltest::testConstraint notRoot 1
    set user {}
    if {[string equal $tcl_platform(platform) "unix"]} {
	catch {set user [exec whoami]}
	if {[string equal $user ""]} {
	    catch {regexp {^[^(]*\(([^)]*)\)} [exec id] dummy user}
	}
	if {([string equal $user "root"]) || ([string equal $user ""])} {
	    tcltest::testConstraint root 1
	    tcltest::testConstraint notRoot 0
	}
    }

    # Set nonBlockFiles constraint: 1 means this platform supports
    # setting files into nonblocking mode.

    if {[catch {set f [open defs r]}]} {
	tcltest::testConstraint nonBlockFiles 1
    } else {
	if {[catch {fconfigure $f -blocking off}] == 0} {
	    tcltest::testConstraint nonBlockFiles 1
	} else {
	    tcltest::testConstraint nonBlockFiles 0
	}
	close $f
    }

    # Set asyncPipeClose constraint: 1 means this platform supports
    # async flush and async close on a pipe.
    #
    # Test for SCO Unix - cannot run async flushing tests because a
    # potential problem with select is apparently interfering.
    # (Mark Diekhans).

    tcltest::testConstraint asyncPipeClose 1
    if {[string equal $tcl_platform(platform) "unix"] \
	    && ([catch {exec uname -X | fgrep {Release = 3.2v}}] == 0)} {
	    tcltest::testConstraint asyncPipeClose 0
    }

    # Test to see if we have a broken version of sprintf with respect
    # to the "e" format of floating-point numbers.

    tcltest::testConstraint eformat 1
    if {![string equal "[format %g 5e-5]" "5e-05"]} {
	tcltest::testConstraint eformat 0
    }

    # Test to see if execed commands such as cat, echo, rm and so forth are
    # present on this machine.

    tcltest::testConstraint unixExecs 1
    if {[string equal $tcl_platform(platform) "macintosh"]} {
	tcltest::testConstraint unixExecs 0
    }
    if {([tcltest::testConstraint unixExecs] == 1) && \
	    ([string equal $tcl_platform(platform) "windows"])} {
	set file "_tcl_test_remove_me.txt"
	if {[catch {
	    set fid [open $file w]
	    puts $fid "hello"
	    close $fid
	}]} {
	    tcltest::testConstraint unixExecs 0
	} elseif {
	    [catch {exec cat $file}] ||
	    [catch {exec echo hello}] ||
	    [catch {exec sh -c echo hello}] ||
	    [catch {exec wc $file}] ||
	    [catch {exec sleep 1}] ||
	    [catch {exec echo abc > $file}] ||
	    [catch {exec chmod 644 $file}] ||
	    [catch {exec rm $file}] ||
	    [string equal {} [auto_execok mkdir]] ||
	    [string equal {} [auto_execok fgrep]] ||
	    [string equal {} [auto_execok grep]] ||
	    [string equal {} [auto_execok ps]]
	    } {
	    tcltest::testConstraint unixExecs 0
	}
	file delete -force $file
    }

    # Locate tcltest executable
    if {![info exists tk_version]} {
	interpreter [info nameofexecutable]
    }

    tcltest::testConstraint stdio 0
    catch {
	catch {file delete -force tmp}
	set f [open tmp w]
	puts $f {
	    exit
	}
	close $f

	set f [open "|[list [interpreter] tmp]" r]
	close $f
	
	tcltest::testConstraint stdio 1
    }
    catch {file delete -force tmp}

    # Deliberately call socket with the wrong number of arguments.  The error
    # message you get will indicate whether sockets are available on this
    # system. 

    catch {socket} msg
    tcltest::testConstraint socket \
	    [expr {$msg != "sockets are not available on this system"}]
    
    # Check for internationalization

    if {[info commands testlocale] == ""} {
	# No testlocale command, no tests...
	tcltest::testConstraint hasIsoLocale 0
    } else {
	tcltest::testConstraint hasIsoLocale \
		[string length [tcltest::set_iso8859_1_locale]]
	tcltest::restore_locale
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
#	Prints out the usage information for package tcltest.  This can be
#       customized with the redefinition of tcltest::PrintUsageInfoHook.
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
    puts [format "Usage: [file tail [info nameofexecutable]] \
	    script ?-help? ?flag value? ... \n\
	    Available flags (and valid input values) are: \n\
	    -help          \t Display this usage information. \n\
	    -verbose level \t Takes any combination of the values \n\
	    \t                 'p', 's', 'b', 't' and 'e'.  Test suite will \n\
	    \t                 display all passed tests if 'p' is \n\
	    \t                 specified, all skipped tests if 's' \n\
	    \t                 is specified, the bodies of \n\
	    \t                 failed tests if 'b' is specified, \n\
	    \t                 and when tests start if 't' is specified. \n\
	    \t                 ErrorInfo is displayed if 'e' is specified. \n\
	    \t                 The default value is 'b'. \n\
	    -constraints list\t Do not skip the listed constraints\n\
	    -limitconstraints bool\t Only run tests with the constraints\n\
	    \t                 listed in -constraints.\n\
	    -match pattern \t Run all tests within the specified \n\
	    \t                 files that match the glob pattern \n\
	    \t                 given. \n\
	    -skip pattern  \t Skip all tests within the set of \n\
	    \t                 specified tests (via -match) and \n\
	    \t                 files that match the glob pattern \n\
	    \t                 given. \n\
	    -file pattern  \t Run tests in all test files that \n\
	    \t                 match the glob pattern given. \n\
	    -notfile pattern\t Skip all test files that match the \n\
	    \t                 glob pattern given. \n\
	    -relateddir pattern\t Run tests in directories that match \n\
	    \t                 the glob pattern given. \n\
	    -asidefromdir pattern\t Skip tests in directories that match \n\
	    \t                 the glob pattern given.\n\
	    -preservecore level \t If 2, save any core files produced \n\
	    \t                 during testing in the directory \n\
	    \t                 specified by -tmpdir. If 1, notify the\n\
	    \t                 user if core files are created. The default \n\
	    \t                 is $tcltest::preserveCore. \n\
	    -tmpdir directory\t Save temporary files in the specified\n\
	    \t                 directory.  The default value is \n\
	    \t                 $tcltest::temporaryDirectory. \n\
	    -testdir directories\t Search tests in the specified\n\
	    \t                 directories.  The default value is \n\
	    \t                 $tcltest::testsDirectory. \n\
	    -outfile file    \t Send output from test runs to the \n\
	    \t                 specified file.  The default is \n\
	    \t                 stdout. \n\
	    -errfile file    \t Send errors from test runs to the \n\
	    \t                 specified file.  The default is \n\
	    \t                 stderr. \n\
	    -loadfile file   \t Read the script to load the tested \n\
	    \t                 commands from the specified file. \n\
	    -load script     \t Specifies the script to load the tested \n\
	    \t                 commands. \n\
	    -debug level     \t Internal debug flag."]
    tcltest::PrintUsageInfoHook
    return
}

# tcltest::processCmdLineArgsFlagsHook --
#
#	This hook is used to add to the list of command line arguments that are
#       processed by tcltest::ProcessFlags.   It is called at the beginning of
#       ProcessFlags.
#

if {[llength [info commands tcltest::processCmdLineArgsAddFlagsHook]] == 0} {
    proc tcltest::processCmdLineArgsAddFlagsHook {} {}
}

# tcltest::processCmdLineArgsHook --
#
#	This hook is used to actually process the flags added by
#       tcltest::processCmdLineArgsAddFlagsHook.  It is called at the end of
#       ProcessFlags. 
#
# Arguments:
#	flags      The flags that have been pulled out of argv
#

if {[llength [info commands tcltest::processCmdLineArgsHook]] == 0} {
    proc tcltest::processCmdLineArgsHook {flag} {}
}

# tcltest::ProcessFlags --
#
#	process command line arguments supplied in the flagArray - this is
#       called by processCmdLineArgs
#       modifies tcltest variables according to the content of the flagArray.
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
	tcltest::PrintUsageInfo
	exit 1
    }
    
    catch {array set flag $flagArray}

    # -help is not listed since it has already been processed
    lappend defaultFlags -verbose -match -skip -constraints \
	    -outfile -errfile -debug -tmpdir -file -notfile \
	    -preservecore -limitconstraints -testdir \
	    -load -loadfile -asidefromdir \
	    -relateddir -singleproc
    set defaultFlags [concat $defaultFlags \
	    [tcltest::processCmdLineArgsAddFlagsHook ]]

    # Set tcltest::verbose to the arg of the -verbose flag, if given
    if {[info exists flag(-verbose)]} {
	tcltest::verbose $flag(-verbose)
    }

    # Set tcltest::match to the arg of the -match flag, if given.  
    if {[info exists flag(-match)]} {
	tcltest::match $flag(-match)
    } 

    # Set tcltest::skip to the arg of the -skip flag, if given
    if {[info exists flag(-skip)]} {
	tcltest::skip $flag(-skip)
    }

    # Handle the -file and -notfile flags
    if {[info exists flag(-file)]} {
	tcltest::matchFiles $flag(-file)
    }
    if {[info exists flag(-notfile)]} {
	tcltest::skipFiles $flag(-notfile)
    }

    # Handle -relateddir and -asidefromdir flags
    if {[info exists flag(-relateddir)]} {
	tcltest::matchDirectories $flag(-relateddir)
    }
    if {[info exists flag(-asidefromdir)]} {
	tcltest::skipDirectories $flag(-asidefromdir)
    }

    # Use the -constraints flag, if given, to turn on constraints that are
    # turned off by default: userInteractive knownBug nonPortable.  This
    # code fragment must be run after constraints are initialized.

    if {[info exists flag(-constraints)]} {
	foreach elt $flag(-constraints) {
	    tcltest::testConstraint $elt 1
	}
    }

    # Use the -limitconstraints flag, if given, to tell the harness to limit
    # tests run to those that were specified using the -constraints flag.  If
    # the -constraints flag was not specified, print out an error and exit.
    if {[info exists flag(-limitconstraints)]} {
	if {![info exists flag(-constraints)]} {
	    set msg "-limitconstraints flag can only be used with -constraints"
	    error $msg
	}
	tcltest::limitConstraints $flag(-limitconstraints)
    }

    # Set the tcltest::temporaryDirectory to the arg of -tmpdir, if
    # given.

    if {[info exists flag(-tmpdir)]} {
	tcltest::temporaryDirectory $flag(-tmpdir)
    }

    # Set the tcltest::testsDirectory to the arg of -testdir, if
    # given.
    
    if {[info exists flag(-testdir)]} {
	tcltest::testsDirectory $flag(-testdir)
    }

    # If an alternate error or output files are specified, change the
    # default channels.

    if {[info exists flag(-outfile)]} {
	tcltest::outputFile $flag(-outfile)
    } 

    if {[info exists flag(-errfile)]} {
	tcltest::errorFile $flag(-errfile)
    }

    # If a load script was specified, either directly or through
    # a file, remember it for later usage.
    
    if {[info exists flag(-load)] &&  \
	    ([lsearch -exact $flagArray -load] > \
		[lsearch -exact $flagArray -loadfile])} {
	tcltest::loadScript $flag(-load)
    }
    
    if {[info exists flag(-loadfile)] && \
	    ([lsearch -exact $flagArray -loadfile] > \
		[lsearch -exact $flagArray -load]) } {
	tcltest::loadFile $flag(-loadfile)
    }

    # If the user specifies debug testing, print out extra information during
    # the run.
    if {[info exists flag(-debug)]} {
	tcltest::debug $flag(-debug)
    }

    # Handle -preservecore
    if {[info exists flag(-preservecore)]} {
	tcltest::preserveCore $flag(-preservecore)
    }

    # Handle -singleproc flag
    if {[info exists flag(-singleproc)]} {
	tcltest::singleProcess $flag(-singleproc)
    }

    # Call the hook
    tcltest::processCmdLineArgsHook [array get flag]
    return
}

# tcltest::processCmdLineArgs --
#
#	Use command line args to set tcltest namespace variables.
#
#       This procedure must be run after constraints are initialized, because
#       some constraints can be overridden.
# 
#       Set variables based on the contents of the environment variable
#       TCLTEST_OPTIONS first, then override with command-line options, if
#       specified. 
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

proc tcltest::processCmdLineArgs {} {
    global argv

    # If the TCLTEST_OPTIONS environment variable exists, parse it first, then
    # the argv list.  The command line argument parsing will be a two-pass
    # affair from now on, so that TCLTEST_OPTIONS contain the default options.
    # These can be overridden by the command line flags.

    if {[info exists ::env(TCLTEST_OPTIONS)]} {
	tcltest::ProcessFlags $::env(TCLTEST_OPTIONS)
    } 

    # The "argv" var doesn't exist in some cases, so use {}.
    if {(![info exists argv]) || ([llength $argv] < 1)} {
	set flagArray {}
    } else {
	set flagArray $argv
    }
    
    tcltest::ProcessFlags $flagArray

    # Spit out everything you know if we're at a debug level 2 or greater
    DebugPuts 2 "Flags passed into tcltest:"
    if {[info exists ::env(TCLTEST_OPTIONS)]} {
	DebugPuts 2 "    ::env(TCLTEST_OPTIONS): $::env(TCLTEST_OPTIONS)"
    }
    if {[info exists argv]} {
	DebugPuts 2 "    argv: $argv"
    }
    DebugPuts    2 "tcltest::debug              = [tcltest::debug]"
    DebugPuts    2 "tcltest::testsDirectory     = [tcltest::testsDirectory]"
    DebugPuts    2 "tcltest::workingDirectory   = [tcltest::workingDirectory]"
    DebugPuts    2 "tcltest::temporaryDirectory = [tcltest::temporaryDirectory]"
    DebugPuts    2 "tcltest::outputChannel      = [outputChannel]"
    DebugPuts    2 "tcltest::errorChannel       = [errorChannel]"
    DebugPuts    2 "Original environment (tcltest::originalEnv):"
    DebugPArray  2 tcltest::originalEnv
    DebugPuts    2 "Constraints:"
    DebugPArray  2 tcltest::testConstraints
    return
}

#####################################################################

# Code to run the tests goes here.

# tcltest::testPuts --
#
#	Used to redefine puts in test environment.
#       Stores whatever goes out on stdout in tcltest::outData and stderr in 
#       tcltest::errData before sending it on to the regular puts.
#
# Arguments:
#	same as standard puts
#
# Results:
#	none
#
# Side effects:
#       Intercepts puts; data that would otherwise go to stdout, stderr, or
#       file channels specified in tcltest::outputChannel and errorChannel does
#       not get sent to the normal puts function.

proc tcltest::testPuts {args} {
    set len [llength $args]
    if {$len == 1} {
	# Only the string to be printed is specified
	append tcltest::outData "[lindex $args 0]\n"
	return
#	return [tcltest::normalPuts [lindex $args 0]]
    } elseif {$len == 2} {
	# Either -nonewline or channelId has been specified
	if {[regexp {^-nonewline} [lindex $args 0]]} {
	    append tcltest::outData "[lindex $args end]"
	    return
#	    return [tcltest::normalPuts -nonewline [lindex $args end]]
	} else {
	    set channel [lindex $args 0]
	}
    } elseif {$len == 3} {
	if {[lindex $args 0] == "-nonewline"} {
	    # Both -nonewline and channelId are specified, unless it's an
	    # error.  -nonewline is supposed to be argv[0].
	    set channel [lindex $args 1]
	}
    }

    if {[info exists channel]} {
	if {($channel == [outputChannel]) || ($channel == "stdout")} {
	    append tcltest::outData "[lindex $args end]\n"
	} elseif {($channel == [errorChannel]) || ($channel == "stderr")} {
	    append tcltest::errData "[lindex $args end]\n"
	}
	return
	# return [tcltest::normalPuts [lindex $args 0] [lindex $args end]]
    }

    # If we haven't returned by now, we don't know how to handle the input.
    # Let puts handle it.
    return [eval tcltest::normalPuts $args]
}

# tcltest::testEval --
#
#	Evaluate the script in the test environment.  If ignoreOutput is 
#       false, store data sent to stderr and stdout in tcltest::outData and 
#       tcltest::errData.  Otherwise, ignore this output altogether.
#
# Arguments:
#	script             Script to evaluate
#       ?ignoreOutput?     Indicates whether or not to ignore output sent to
#                          stdout & stderr
#
# Results:
#	result from running the script
#
# Side effects:
#	Empties the contents of tcltest::outData and tcltest::errData before
#       running a test if ignoreOutput is set to 0. 

proc tcltest::testEval {script {ignoreOutput 1}} {
    DebugPuts 3 "testEval called" 
    if {!$ignoreOutput} {
	set tcltest::outData {}
	set tcltest::errData {}
	uplevel rename ::puts tcltest::normalPuts
	uplevel rename tcltest::testPuts ::puts
    }
    set result [uplevel $script]
    if {!$ignoreOutput} {
	uplevel rename ::puts tcltest::testPuts
	uplevel rename tcltest::normalPuts ::puts
    }
    return $result
}

# compareStrings --
#
#	compares the expected answer to the actual answer, depending on the
#       mode provided.  Mode determines whether a regexp, exact, or glob
#       comparison is done.
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

proc tcltest::compareStrings {actual expected mode} {
    switch -- $mode {
	exact {
	    set retval [string equal $actual $expected]
	}
	glob {
	    set retval [string match $expected $actual]
	}
	regexp {
	    set retval [regexp -- $expected $actual]
	}
    }
    return $retval
}


#
# tcltest::substArguments list
#
# This helper function takes in a list of words, then perform a
# substitution on the list as though each word in the list is a
# separate argument to the Tcl function.  For example, if this
# function is invoked as:
#
#      substArguments {$a {$a}}
#
# Then it is as though the function is invoked as:
#
#      substArguments $a {$a}
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

proc tcltest::substArguments {argList} {

    # We need to split the argList up into tokens but cannot use
    # list operations as they throw away some significant
    # quoting, and [split] ignores braces as it should.
    # Therefore what we do is gradually build up a string out of
    # whitespace seperated strings. We cannot use [split] to
    # split the argList into whitespace seperated strings as it
    # throws away the whitespace which maybe important so we
    # have to do it all by hand.

    set result {}
    set token ""

    while {[string length $argList]} {
        # Look for the next word containing a quote: " { }
        if {[regexp -indices {[^ \t\n]*[\"\{\}]+[^ \t\n]*} \
                $argList all]} {
            # Get the text leading up to this word, but not
            # including this word, from the argList.
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
# This procedure runs a test and prints an error message if the test fails.
# If tcltest::verbose has been set, it also prints a message even if the
# test succeeds.  The test will be skipped if it doesn't match the
# tcltest::match variable, if it matches an element in
# tcltest::skip, or if one of the elements of "constraints" turns
# out not to be true.
# 
# If testLevel is 1, then this is a top level test, and we record pass/fail
# information; otherwise, this information is not logged and is not added to
# running totals.
# 
# Attributes:
#   Only description is a required attribute.  All others are optional.
#   Default values are indicated.
# 
#   constraints -		A list of one or more keywords, each of
#			        which must be the name of an element in
#			        the array "tcltest::testConstraints".  If any
#                               of these elements is zero, the test is
#                               skipped. This attribute is optional; default is {}
#   body -		        Script to run to carry out the test.  It must
#			        return a result that can be checked for
#			        correctness.  This attribute is optional;
#                               default is {}
#   result -	                Expected result from script.  This attribute is
#                               optional; default is {}.
#   output -                    Expected output sent to stdout.  This attribute
#                               is optional; default is {}.  
#   errorOutput -               Expected output sent to stderr.  This attribute
#                               is optional; default is {}.  
#   returnCodes -              Expected return codes.  This attribute is
#                               optional; default is {0 2}.
#   setup -                     Code to run before $script (above).  This
#                               attribute is optional; default is {}.
#   cleanup -                   Code to run after $script (above).  This
#                               attribute is optional; default is {}.
#   match -                     specifies type of matching to do on result,
#                               output, errorOutput; this must be one of: exact,
#                               glob, regexp.  default is exact.
#
# Arguments:
#   name -		Name of test, in the form foo-1.2.
#   description -	Short textual description of the test, to
#  		  	help humans understand what it does.
# 
# Results:
#	0 if the command ran successfully; 1 otherwise.
#
# Side effects:
#       None.
# 

proc tcltest::test {name description args} {
    DebugPuts 3 "Test $name $args"

    incr tcltest::testLevel

    # Pre-define everything to null except output and errorOutput.  We
    # determine whether or not to trap output based on whether or not these
    # variables (output & errorOutput) are defined.
    foreach item {constraints setup cleanup body result returnCodes match} {
	set $item {}
    }

    # Set the default match mode
    set match exact

    # Set the default match values for return codes (0 is the standard expected
    # return value if everything went well; 2 represents 'return' being used in
    # the test script).
    set returnCodes [list 0 2]

    # The old test format can't have a 3rd argument (constraints or script)
    # that starts with '-'.
    if {[llength $args] == 0} {
	puts [errorChannel] "test $name: {wrong # args: should be \"test name desc ?options?\"}"
	incr tcltest::testLevel -1
	return 1
    } elseif {([string index [lindex $args 0] 0] == "-") || ([llength $args] == 1)} {
	
	if {[llength $args] == 1} {
	    set list [substArguments [lindex $args 0]]
	    foreach {element value} $list { 
		set testAttributes($element) $value
	    }
	    foreach item {constraints match setup body cleanup \
		    result returnCodes output errorOutput} {
		if {[info exists testAttributes([subst -$item])]} {
		    set testAttributes([subst -$item]) \
			    [uplevel concat $testAttributes([subst -$item])]
		}
	    }
	} else {
	    array set testAttributes $args
	}

	set validFlags {-setup -cleanup -body -result -returnCodes -match \
		-output -errorOutput -constraints}

	foreach flag [array names testAttributes] {
	    if {[lsearch -exact $validFlags $flag] == -1} {
		puts [errorChannel] "test $name: bad flag $flag supplied to tcltest::test"
		incr tcltest::testLevel -1
		return 1
	    }
	}

	# store whatever the user gave us
	foreach item [array names testAttributes] {
	    set [string trimleft $item "-"] $testAttributes($item)
	}

	# Check the values supplied for -match
	if {[lsearch {regexp glob exact} $match] == -1} {
	    puts [errorChannel] "test $name: {bad value for -match: must be one of exact, glob, regexp}"
	    incr tcltest::testLevel -1
	    return 1
	}

	# Replace symbolic valies supplied for -returnCodes
	regsub -nocase normal $returnCodes 0 returnCodes
	regsub -nocase error $returnCodes 1 returnCodes
	regsub -nocase return $returnCodes 2 returnCodes
	regsub -nocase break $returnCodes 3 returnCodes
	regsub -nocase continue $returnCodes 4 returnCodes 
    } else {
	# This is parsing for the old test command format; it is here for
	# backward compatibility.
	set result [lindex $args end]
	if {[llength $args] == 2} {
	    set body [lindex $args 0]
	} elseif {[llength $args] == 3} {
	    set constraints [lindex $args 0]
	    set body [lindex $args 1]
	} else {
	    puts [errorChannel] "test $name: {wrong # args: should be \"test name desc ?constraints? script expectedResult\"}"
	    incr tcltest::testLevel -1
	    return 1
	}
    } 

    set setupFailure 0
    set cleanupFailure 0

    # Run the setup script
    if {[catch {uplevel $setup} setupMsg]} {
	set setupFailure 1
    }

    # run the test script
    set command [list tcltest::runTest $name $description $body \
	    $result $constraints]  
    if {!$setupFailure} {
	if {[info exists output] || [info exists errorOutput]} {
	    set testResult [uplevel tcltest::testEval [list $command] 0]
	} else {
	    set testResult [uplevel tcltest::testEval [list $command] 1]
	}
    } else {
	set testResult setupFailure
    }

    # Run the cleanup code
    if {[catch {uplevel $cleanup} cleanupMsg]} {
	set cleanupFailure 1
    }

    # If testResult is an empty list, then the test was skipped
    if {$testResult != {}} {
	set coreFailure 0
	set coreMsg ""
	# check for a core file first - if one was created by the test, then
	# the test failed
	if {$tcltest::preserveCore} {
	    set currentTclPlatform [array get tcl_platform]
	    if {[file exists [file join [tcltest::workingDirectory] core]]} {
		# There's only a test failure if there is a core file and (1)
		# there previously wasn't one or (2) the new one is different
		# from the old one. 
		if {[info exists coreModTime]} {
		    if {$coreModTime != [file mtime \
			    [file join [tcltest::workingDirectory] core]]} {
			set coreFailure 1
		    } 
		} else {
		    set coreFailure 1
		}
		
		if {($tcltest::preserveCore > 1) && ($coreFailure)} {
		    append coreMsg "\nMoving file to: [file join $tcltest::temporaryDirectory core-$name]"
		    catch {file rename -force \
			    [file join [tcltest::workingDirectory] core] \
			    [file join $tcltest::temporaryDirectory \
				core-$name]} msg
		    if {[string length $msg] > 0} {
			append coreMsg "\nError: Problem renaming core file: $msg"
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
	set errorFailure 0
	if {[info exists output]} { 
	    set outputFailure [expr ![compareStrings $tcltest::outData \
		    $output $match]]
	} 
	if {[info exists errorOutput]} {
	    set errorFailure [expr ![compareStrings $tcltest::errData \
		    $errorOutput $match]]
	}

	set testFailed 1
	set codeFailure 0
	set scriptFailure 0

	# check if the return code matched the expected return code
	if {[lsearch -exact $returnCodes $code] == -1} {
	    set codeFailure 1
	} 

	# check if the answer matched the expected answer
	if {[compareStrings $actualAnswer $result $match] == 0} {
	    set scriptFailure 1
	}

	# if we didn't experience any failures, then we passed
	if {!($setupFailure || $cleanupFailure || $coreFailure || \
		$outputFailure || $errorFailure || $codeFailure || \
		$scriptFailure)} {
	    if {$tcltest::testLevel == 1} {
		incr tcltest::numTests(Passed)
		if {[tcltest::isVerbose pass]} {
		    puts [outputChannel] "++++ $name PASSED"
		}
	    }
	    set testFailed 0
	}

	if {$testFailed} {
	    if {$tcltest::testLevel == 1} {
		incr tcltest::numTests(Failed)
	    }
	    set tcltest::currentFailure true
	    if {![tcltest::isVerbose body]} {
		set body ""
	    }	
	    puts [outputChannel] "\n==== $name [string trim $description] FAILED"
	    if {$body != ""} {
		puts [outputChannel] "==== Contents of test case:"
		puts [outputChannel] $body
	    }
	    if {$setupFailure} {
		puts [outputChannel] "---- Test setup failed:\n$setupMsg"
	    } 
	    if {$scriptFailure} {
		puts [outputChannel] "---- Result was:\n$actualAnswer"
		puts [outputChannel] "---- Result should have been ($match matching):\n$result"
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
		puts [outputChannel] "---- Return code should have been one of: $returnCodes"
		if {[tcltest::isVerbose error]} {
		    if {[info exists ::errorInfo]} {
			puts [outputChannel] "---- errorInfo: $::errorInfo"
			puts [outputChannel] "---- errorCode: $::errorCode"
		    }
		}
	    }
	    if {$outputFailure} {
		puts [outputChannel] "---- Output was:\n$tcltest::outData"
		puts [outputChannel] "---- Output should have been ($match matching):\n$output"
	    }
	    if {$errorFailure} {
		puts [outputChannel] "---- Error output was:\n$tcltest::errData"
		puts [outputChannel] "---- Error output should have been ($match matching):\n$errorOutput"
	    }
	    if {$cleanupFailure} {
		puts [outputChannel] "---- Test cleanup failed:\n$cleanupMsg"
	    }
	    if {$coreFailure} {
		puts [outputChannel] "---- Core file produced while running test! $coreMsg"
	    }
	    puts [outputChannel] "==== $name FAILED\n"

	}
    }
    
    incr tcltest::testLevel -1
    return 0
}


# runTest --
#
# This is the defnition of the version 1.0 test routine for tcltest.  It is
# provided here for backward compatibility.  It is also used as the 'backbone'
# of the test procedure, as in, this is where all the work really gets done.
# 
# This procedure runs a test and prints an error message if the test fails.
# If tcltest::verbose has been set, it also prints a message even if the
# test succeeds.  The test will be skipped if it doesn't match the
# tcltest::match variable, if it matches an element in
# tcltest::skip, or if one of the elements of "constraints" turns
# out not to be true.
#
# Arguments:
# name -		Name of test, in the form foo-1.2.
# description -		Short textual description of the test, to
#			help humans understand what it does.
# constraints -		A list of one or more keywords, each of
#			which must be the name of an element in
#			the array "tcltest::testConstraints".  If any of these
#			elements is zero, the test is skipped.
#			This argument may be omitted.
# script -		Script to run to carry out the test.  It must
#			return a result that can be checked for
#			correctness.
# expectedAnswer -	Expected result from script.
# 
# Behavior depends on the value of testLevel; if testLevel is 1 (top level),
# then events are logged and we track the number of tests run/skipped and why.
# Otherwise, we don't track this information.
# 
# Results:
#    empty list if test is skipped; otherwise returns list containing
#    actual returned value from the test and the return code.
# 
# Side Effects:
#    none.
#

proc tcltest::runTest {name description script expectedAnswer constraints} {

    if {$tcltest::testLevel == 1} {
	incr tcltest::numTests(Total)
    }
    
    # skip the test if it's name matches an element of skip
    foreach pattern $tcltest::skip {
	if {[string match $pattern $name]} {
	    if {$tcltest::testLevel == 1} {
		incr tcltest::numTests(Skipped)
		DebugDo 1 {tcltest::AddToSkippedBecause userSpecifiedSkip}
	    }
	    return
	}
    }

    # skip the test if it's name doesn't match any element of match
    if {[llength $tcltest::match] > 0} {
	set ok 0
	foreach pattern $tcltest::match {
	    if {[string match $pattern $name]} {
		set ok 1
		break
	    }
        }
	if {!$ok} {
	    if {$tcltest::testLevel == 1} {
		incr tcltest::numTests(Skipped)
		DebugDo 1 {tcltest::AddToSkippedBecause userSpecifiedNonMatch}
	    }
	    return
	}
    }

    DebugPuts 3 "Running $name ($description) {$script} {$expectedAnswer} $constraints"

    if {$constraints == {}} {
	# If we're limited to the listed constraints and there aren't any
	# listed, then we shouldn't run the test.
	if {$tcltest::limitConstraints} {
	    tcltest::AddToSkippedBecause userSpecifiedLimitConstraint
	    if {$tcltest::testLevel == 1} {
		incr tcltest::numTests(Skipped)
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
	    # $tcltest::testConstraints(a) || $tcltest::testConstraints(b).
	    regsub -all {[.\w]+} $constraints \
		    {$tcltest::testConstraints(&)} c
	    catch {set doTest [eval expr $c]}
	} elseif {![catch {llength $constraints}]} {
	    # just simple constraints such as {unixOnly fonts}.
	    set doTest 1
	    foreach constraint $constraints {
		if {(![info exists tcltest::testConstraints($constraint)]) \
			|| (!$tcltest::testConstraints($constraint))} {
		    set doTest 0

		    # store the constraint that kept the test from running
		    set constraints $constraint
		    break
		}
	    }
	}
	
	if {$doTest == 0} {
	    if {[tcltest::isVerbose skip]} {
		puts [outputChannel] "++++ $name SKIPPED: $constraints"
	    }
	    
	    if {$tcltest::testLevel == 1} {
		incr tcltest::numTests(Skipped)
		tcltest::AddToSkippedBecause $constraints
	    }
	    return	
	}
    }

    # Save information about the core file.  You need to restore the original
    # tcl_platform environment because some of the tests mess with
    # tcl_platform.

    if {$tcltest::preserveCore} {
	set currentTclPlatform [array get tcl_platform]
	array set tcl_platform $tcltest::originalTclPlatform
	if {[file exists [file join [tcltest::workingDirectory] core]]} {
	    set coreModTime [file mtime [file join \
		    [tcltest::workingDirectory] core]]
	}
	array set tcl_platform $currentTclPlatform
    }

    # If there is no "memory" command (because memory debugging isn't
    # enabled), then don't attempt to use the command.
    
    if {[info commands memory] != {}} {
	memory tag $name
    }

    if {[tcltest::isVerbose start]} {
	puts [outputChannel] "---- $name start"
	flush [outputChannel]
    }
    
    set code [catch {uplevel $script} actualAnswer]
	    
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
#      calledFromAllFile - if 0, behave as if we are running a single test file
#      within an entire suite of tests.  if we aren't running a single test
#      file, then don't report status.  check for new files created during the
#      test run and report on them.  if 1, report collated status from all the
#      test file runs.
# 
# Results: 
#      None.
# 
# Side Effects: 
#      None
# 

proc tcltest::cleanupTests {{calledFromAllFile 0}} {

    set testFileName [file tail [info script]]

    # Call the cleanup hook
    tcltest::cleanupTestsHook 

    # Remove files and directories created by the :tcltest::makeFile and
    # tcltest::makeDirectory procedures.
    # Record the names of files in tcltest::workingDirectory that were not
    # pre-existing, and associate them with the test file that created them.

    if {!$calledFromAllFile} {
	foreach file $tcltest::filesMade {
	    if {[file exists $file]} {
		catch {file delete -force $file}
	    }
	}
	set currentFiles {}
	foreach file [glob -nocomplain \
		[file join $tcltest::temporaryDirectory *]] {
	    lappend currentFiles [file tail $file]
	}
	set newFiles {}
	foreach file $currentFiles {
	    if {[lsearch -exact $tcltest::filesExisted $file] == -1} {
		lappend newFiles $file
	    }
	}
	set tcltest::filesExisted $currentFiles
	if {[llength $newFiles] > 0} {
	    set tcltest::createdNewFiles($testFileName) $newFiles
	}
    }

    if {$calledFromAllFile || $tcltest::testSingleFile} {

	# print stats

	puts -nonewline [outputChannel] "$testFileName:"
	foreach index [list "Total" "Passed" "Skipped" "Failed"] {
	    puts -nonewline [outputChannel] \
		    "\t$index\t$tcltest::numTests($index)"
	}
	puts [outputChannel] ""

	# print number test files sourced
	# print names of files that ran tests which failed

	if {$calledFromAllFile} {
	    puts [outputChannel] \
		    "Sourced $tcltest::numTestFiles Test Files."
	    set tcltest::numTestFiles 0
	    if {[llength $tcltest::failFiles] > 0} {
		puts [outputChannel] \
			"Files with failing tests: $tcltest::failFiles"
		set tcltest::failFiles {}
	    }
	}

	# if any tests were skipped, print the constraints that kept them
	# from running.

	set constraintList [array names tcltest::skippedBecause]
	if {[llength $constraintList] > 0} {
	    puts [outputChannel] \
		    "Number of tests skipped for each constraint:"
	    foreach constraint [lsort $constraintList] {
		puts [outputChannel] \
			"\t$tcltest::skippedBecause($constraint)\t$constraint"
		unset tcltest::skippedBecause($constraint)
	    }
	}

	# report the names of test files in tcltest::createdNewFiles, and
	# reset the array to be empty.

	set testFilesThatTurded [lsort [array names tcltest::createdNewFiles]]
	if {[llength $testFilesThatTurded] > 0} {
	    puts [outputChannel] "Warning: files left behind:"
	    foreach testFile $testFilesThatTurded {
		puts [outputChannel] \
			"\t$testFile:\t$tcltest::createdNewFiles($testFile)"
		unset tcltest::createdNewFiles($testFile)
	    }
	}

	# reset filesMade, filesExisted, and numTests

	set tcltest::filesMade {}
	foreach index [list "Total" "Passed" "Skipped" "Failed"] {
	    set tcltest::numTests($index) 0
	}

	# exit only if running Tk in non-interactive mode

	global tk_version tcl_interactive
	if {[info exists tk_version] && ![info exists tcl_interactive]} {
	    exit
	}
    } else {

	# if we're deferring stat-reporting until all files are sourced,
	# then add current file to failFile list if any tests in this file
	# failed

	incr tcltest::numTestFiles
	if {($tcltest::currentFailure) && \
		([lsearch -exact $tcltest::failFiles $testFileName] == -1)} {
	    lappend tcltest::failFiles $testFileName
	}
	set tcltest::currentFailure false

	# restore the environment to the state it was in before this package
	# was loaded

	set newEnv {}
	set changedEnv {}
	set removedEnv {}
	foreach index [array names ::env] {
	    if {![info exists tcltest::originalEnv($index)]} {
		lappend newEnv $index
		unset ::env($index)
	    } else {
		if {$::env($index) != $tcltest::originalEnv($index)} {
		    lappend changedEnv $index
		    set ::env($index) $tcltest::originalEnv($index)
		}
	    }
	}
	foreach index [array names tcltest::originalEnv] {
	    if {![info exists ::env($index)]} {
		lappend removedEnv $index
		set ::env($index) $tcltest::originalEnv($index)
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
	foreach index [array names tcltest::originalTclPlatform] {
	    if {$::tcl_platform($index) != \
		    $tcltest::originalTclPlatform($index)} { 
		lappend changedTclPlatform $index
		set ::tcl_platform($index) \
			$tcltest::originalTclPlatform($index) 
	    }
	}
	if {[llength $changedTclPlatform] > 0} {
	    puts [outputChannel] \
		    "tcl_platform array elements changed:\t$changedTclPlatform"
	} 

	if {[file exists [file join [tcltest::workingDirectory] core]]} {
	    if {$tcltest::preserveCore > 1} {
		puts "rename core file (> 1)" 
		puts [outputChannel] "produced core file! \
			Moving file to: \
			[file join $tcltest::temporaryDirectory core-$name]"
		catch {file rename -force \
			[file join [tcltest::workingDirectory] core] \
			[file join $tcltest::temporaryDirectory \
			    core-$name]} msg
		if {[string length $msg] > 0} {
		    tcltest::PrintError "Problem renaming file: $msg"
		}
	    } else {
		# Print a message if there is a core file and (1) there
		# previously wasn't one or (2) the new one is different from
		# the old one. 

		if {[info exists tcltest::coreModificationTime]} {
		    if {$tcltest::coreModificationTime != [file mtime \
			    [file join [tcltest::workingDirectory] core]]} {
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

# tcltest::getMatchingFiles
#
#       Looks at the patterns given to match and skip files
#       and uses them to put together a list of the tests that will be run.
#
# Arguments:
#       directory to search
#
# Results:
#       The constructed list is returned to the user.  This will primarily
#       be used in 'all.tcl' files.  It is used in runAllTests.
# 
# Side Effects:
#       None

proc tcltest::getMatchingFiles { {searchDirectory ""} } {
    if {[llength [info level 0]] == 1} {
	set searchDirectory [tcltest::testsDirectory]
    }
    set matchingFiles {}

    # Find the matching files in the list of directories and then remove the
    # ones that match the skip pattern. Passing a list to foreach is required
    # so that a patch like D:\Foo\Bar does not get munged into D:FooBar.
    foreach directory [list $searchDirectory] {
	set matchFileList {}
	foreach match $tcltest::matchFiles {
	    set matchFileList [concat $matchFileList \
		    [glob -directory $directory -nocomplain -- $match]]
	}
	if {[string compare {} $tcltest::skipFiles]} {
	    set skipFileList {}
	    foreach skip $tcltest::skipFiles {
		set skipFileList [concat $skipFileList \
			[glob -directory $directory -nocomplain -- $skip]]
	    }
	    foreach file $matchFileList {
		# Only include files that don't match the skip pattern and
		# aren't SCCS lock files.
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
	tcltest::PrintError "No test files remain after applying \
		your match and skip patterns!"
    }
    return $matchingFiles
}

# tcltest::getMatchingDirectories --
#
#	Looks at the patterns given to match and skip directories and uses them
#       to put together a list of the test directories that we should attempt
#       to run.  (Only subdirectories containing an "all.tcl" file are put into
#       the list.)
#
# Arguments:
#	root directory from which to search
#
# Results:
#	The constructed list is returned to the user.  This is used in the
#       primary all.tcl file.  Lower-level all.tcl files should use the
#       tcltest::testAllFiles proc instead.
# 
# Side Effects: 
#       None.

proc tcltest::getMatchingDirectories {rootdir} {
    set matchingDirs {}
    set matchDirList {}
    # Find the matching directories in tcltest::testsDirectory and then
    # remove the ones that match the skip pattern
    foreach match $tcltest::matchDirectories {
	foreach file [glob -directory $rootdir -nocomplain -- $match] {
	    if {([file isdirectory $file]) && ($file != $rootdir)} {
		set matchDirList [concat $matchDirList \
			[tcltest::getMatchingDirectories $file]]
		if {[file exists [file join $file all.tcl]]} {
		    set matchDirList [concat $matchDirList $file]
		}
	    }
	}
    }
    if {$tcltest::skipDirectories != {}} {
	set skipDirs {} 
	foreach skip $tcltest::skipDirectories {
	    set skipDirs [concat $skipDirs \
		[glob -nocomplain -directory $tcltest::testsDirectory $skip]]
	}
	foreach dir $matchDirList {
	    # Only include directories that don't match the skip pattern
	    if {[lsearch -exact $skipDirs $dir] == -1} {
		lappend matchingDirs $dir
	    }
	}
    } else {
	set matchingDirs [concat $matchingDirs $matchDirList]
    }
    if {$matchingDirs == {}} {
	DebugPuts 1 "No test directories remain after applying match and skip patterns!"
    }
    return $matchingDirs
}

# tcltest::runAllTests --
#
#	prints output and sources test files according to the match and skip
#       patterns provided.  after sourcing test files, it goes on to source
#       all.tcl files in matching test subdirectories.
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

    if {[llength [info level 0]] == 1} {
	set shell [tcltest::interpreter]
    }

    set tcltest::testSingleFile false

    puts [outputChannel] "Tests running in interp:  $shell"
    puts [outputChannel] "Tests located in:  $tcltest::testsDirectory"
    puts [outputChannel] "Tests running in:  [tcltest::workingDirectory]"
    puts [outputChannel] "Temporary files stored in $tcltest::temporaryDirectory"
    
    if {[package vcompare [package provide Tcl] 8.4] >= 0} {
	# If we aren't running in the native filesystem, then we must
	# run the tests in a single process (via 'source'), because
	# trying to run then via a pipe will fail since the files don't
	# really exist.
	if {[lindex [file system [tcltest::testsDirectory]] 0] != "native"} {
	    tcltest::singleProcess 1
	}
    }

    if {[tcltest::singleProcess]} {
	puts [outputChannel] "Test files sourced into current interpreter"
    } else {
	puts [outputChannel] "Test files run in separate interpreters"
    }
    if {[llength $tcltest::skip] > 0} {
	puts [outputChannel] "Skipping tests that match:  $tcltest::skip"
    }
    if {[llength $tcltest::match] > 0} {
	puts [outputChannel] "Only running tests that match:  $tcltest::match"
    }

    if {[llength $tcltest::skipFiles] > 0} {
	puts [outputChannel] "Skipping test files that match:  $tcltest::skipFiles"
    }
    if {[llength $tcltest::matchFiles] > 0} {
	puts [outputChannel] "Only running test files that match:  $tcltest::matchFiles"
    }

    set timeCmd {clock format [clock seconds]}
    puts [outputChannel] "Tests began at [eval $timeCmd]"

    # Run each of the specified tests
    foreach file [lsort [tcltest::getMatchingFiles]] {
	set tail [file tail $file]
	puts [outputChannel] $tail

	if {$tcltest::singleProcess} {
	    incr tcltest::numTestFiles
	    uplevel [list source $file]
	} else {
	    set cmd [concat [list | $shell $file] [split $argv]]
	    if {[catch {
		incr tcltest::numTestFiles
		set pipeFd [open $cmd "r"] 
		while {[gets $pipeFd line] >= 0} {
		    if {[regexp {^([^:]+):\tTotal\t([0-9]+)\tPassed\t([0-9]+)\tSkipped\t([0-9]+)\tFailed\t([0-9]+)} $line null testFile Total Passed Skipped Failed]} {
			foreach index [list "Total" "Passed" "Skipped" "Failed"] {
			    incr tcltest::numTests($index) [set $index]
			}
			if {$Failed > 0} {
			    lappend tcltest::failFiles $testFile
			}
		    } elseif {[regexp {^Number of tests skipped for each constraint:|^\t(\d+)\t(.+)$} $line match skipped constraint]} {
			if {$match != "Number of tests skipped for each constraint:"} {
			    tcltest::AddToSkippedBecause $constraint $skipped
			}
		    } else {
			puts [outputChannel] $line
		    }
		}
		close $pipeFd
	    } msg]} {
		# Print results to tcltest::outputChannel.
		puts [outputChannel] "Test file error: $msg"
		# append the name of the test to a list to be reported later
		lappend testFileFailures $file 
	    }
	}
    }

    # cleanup
    puts [outputChannel] "\nTests ended at [eval $timeCmd]"
    tcltest::cleanupTests 1
    if {[info exists testFileFailures]} {
	puts [outputChannel] "\nTest files exiting with errors:  \n"
	foreach file $testFileFailures {
	    puts "  [file tail $file]\n"
	}
    }

    # Checking for subdirectories in which to run tests
    foreach directory [tcltest::getMatchingDirectories $tcltest::testsDirectory] {
	set dir [file tail $directory]
	puts [outputChannel] "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
	puts [outputChannel] "$dir test began at [eval $timeCmd]\n"
	
	uplevel "source [file join $directory all.tcl]"
	
	set endTime [eval $timeCmd]
	puts [outputChannel] "\n$dir test ended at $endTime"
	puts [outputChannel] "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    }
    return
}

#####################################################################

# Test utility procs - not used in tcltest, but may be useful for testing.

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
    if {$tcltest::loadScript == {}} {
	return
    }
    
    return [uplevel $tcltest::loadScript]
}

# tcltest::saveState --
#
#	Save information regarding what procs and variables exist.
#
# Arguments:
#	none
#
# Results:
#	Modifies the variable tcltest::saveState
#
# Side effects:
#	None.

proc tcltest::saveState {} {
    uplevel {set tcltest::saveState [list [info procs] [info vars]]}
    DebugPuts  2 "tcltest::saveState: $tcltest::saveState"
    return
}

# tcltest::restoreState --
#
#	Remove procs and variables that didn't exist before the call to
#       tcltest::saveState.
#
# Arguments:
#	none
#
# Results:
#	Removes procs and variables from your environment if they don't exist
#       in the tcltest::saveState variable.
#
# Side effects:
#	None.

proc tcltest::restoreState {} {
    foreach p [info procs] {
	if {([lsearch [lindex $tcltest::saveState 0] $p] < 0) && \
		(![string match "*tcltest::$p" [namespace origin $p]])} {
	    
	    DebugPuts 2 "tcltest::restoreState: Removing proc $p"
	    rename $p {}
	}
    }
    foreach p [uplevel {info vars}] {
	if {[lsearch [lindex $tcltest::saveState 1] $p] < 0} {
	    DebugPuts 2 "tcltest::restoreState: Removing variable $p"
	    uplevel "catch {unset $p}"
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
# cleanupTests was called, add it to the $filesMade list, so it will
# be removed by the next call to cleanupTests.
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

    if {[llength [info level 0]] == 3} {
	set directory [tcltest::temporaryDirectory]
    }
    
    set fullName [file join $directory $name]

    DebugPuts 3 "tcltest::makeFile: putting $contents into $fullName"

    set fd [open $fullName w]

    fconfigure $fd -translation lf

    if {[string equal [string index $contents end] "\n"]} {
	puts -nonewline $fd $contents
    } else {
	puts $fd $contents
    }
    close $fd

    if {[lsearch -exact $tcltest::filesMade $fullName] == -1} {
	lappend tcltest::filesMade $fullName
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
	set directory [tcltest::temporaryDirectory]
    }
    set fullName [file join $directory $name]
    DebugPuts 3 "tcltest::removeFile: removing $fullName"
    return [file delete $fullName]
}

# tcltest::makeDirectory --
#
# Create a new dir with the name <name>.
#
# If this dir hasn't been created via makeDirectory since the last time
# cleanupTests was called, add it to the $directoriesMade list, so it will
# be removed by the next call to cleanupTests.
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
    if {[llength [info level 0]] == 2} {
	set directory [tcltest::temporaryDirectory]
    }
    set fullName [file join $directory $name]
    DebugPuts 3 "tcltest::makeDirectory: creating $fullName"
    file mkdir $fullName
    if {[lsearch -exact $tcltest::filesMade $fullName] == -1} {
	lappend tcltest::filesMade $fullName
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
	set directory [tcltest::temporaryDirectory]
    }
    set fullName [file join $directory $name]
    DebugPuts 3 "tcltest::removeDirectory: deleting $fullName"
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
	set directory [tcltest::temporaryDirectory]
    }
    set fullName [file join $directory $name]
    if {([string equal $tcl_platform(platform) "macintosh"]) || \
	    ([tcltest::testConstraint unixExecs] == 0)} {
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
# 1. Create denormalized or improperly formed strings to pass to C procedures 
#    that are supposed to accept strings with embedded NULL bytes.
# 2. Confirm that a string result has a certain pattern of bytes, for instance
#    to confirm that "\xe0\0" in a Tcl script is stored internally in 
#    UTF-8 as the sequence of bytes "\xc3\xa0\xc0\x80".
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

# tcltest::openfiles --
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

proc tcltest::openfiles {} {
    if {[catch {testchannel open} result]} {
	return {}
    }
    return $result
}

# tcltest::leakfiles --
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

proc tcltest::leakfiles {old} {
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

# tcltest::set_iso8859_1_locale --
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

proc tcltest::set_iso8859_1_locale {} {
    variable previousLocale
    if {[info commands testlocale] != ""} {
	set previousLocale [testlocale ctype]
	testlocale ctype $tcltest::isoLocale
    }
    return
}

# tcltest::restore_locale --
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

proc tcltest::restore_locale {} {
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
		if {$tid != $tcltest::mainThread} {
		    catch {testthread send -async $tid {testthread exit}}
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
		if {$tid != $tcltest::mainThread} {
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
    initConstraints
    processCmdLineArgs

    # Save the names of files that already exist in
    # the output directory.
    variable file {}
    foreach file [glob -nocomplain -directory $temporaryDirectory *] {
	lappend filesExisted [file tail $file]
    }
    unset file
}

package provide tcltest 2.0.2

