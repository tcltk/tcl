# init.tcl --
#
# Default system startup file for Tcl-based applications.  Defines
# "unknown" procedure and auto-load facilities.
#
# RCS: @(#) $Id: init.tcl,v 1.22 1998/11/12 05:54:02 welch Exp $
#
# Copyright (c) 1991-1993 The Regents of the University of California.
# Copyright (c) 1994-1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

if {[info commands package] == ""} {
    error "version mismatch: library\nscripts expect Tcl version 7.5b1 or later but the loaded version is\nonly [info patchlevel]"
}
package require -exact Tcl 8.0

# Compute the auto path to use in this interpreter.
# The values on the path come from several locations:
#
# The environment variable TCLLIBPATH
#
# tcl_library, which is the directory containing this init.tcl script.
# tclInitScript.h searches around for the directory containing this
# init.tcl and defines tcl_library to that location before sourcing it.
#
# The parent directory of tcl_library. Adding the parent
# means that packages in peer directories will be found automatically.
#
# tcl_pkgPath, which is set by the platform-specific initialization routines
#	On UNIX it is compiled in
#	On Windows it comes from the registry
#	On Macintosh it is "Tool Command Language" in the Extensions folder

if {![info exists auto_path]} {
    if {[info exist env(TCLLIBPATH)]} {
	set auto_path $env(TCLLIBPATH)
    } else {
	set auto_path ""
    }
}
foreach __dir [list [info library] [file dirname [info library]]] {
    if {[lsearch -exact $auto_path $__dir] < 0} {
	lappend auto_path $__dir
    }
}
if {[info exist tcl_pkgPath]} {
    foreach __dir $tcl_pkgPath {
	if {[lsearch -exact $auto_path $__dir] < 0} {
	    lappend auto_path $__dir
	}
    }
}
unset __dir

# Windows specific initialization to handle case isses with envars

if {(![interp issafe]) && ($tcl_platform(platform) == "windows")} {
    namespace eval tcl {
	proc envTraceProc {lo n1 n2 op} {
	    set x $::env($n2)
	    set ::env($lo) $x
	    set ::env([string toupper $lo]) $x
	}
    }
    foreach p [array names env] {
	set u [string toupper $p]
	if {$u != $p} {
	    switch -- $u {
		COMSPEC -
		PATH {
		    if {![info exists env($u)]} {
			set env($u) $env($p)
		    }
		    trace variable env($p) w [list tcl::envTraceProc $p]
		    trace variable env($u) w [list tcl::envTraceProc $p]
		}
	    }
	}
    }
    if {[info exists p]} {
	unset p
    }
    if {[info exists u]} {
	unset u
    }
    if {![info exists env(COMSPEC)]} {
	if {$tcl_platform(os) == {Windows NT}} {
	    set env(COMSPEC) cmd.exe
	} else {
	    set env(COMSPEC) command.com
	}
    }
}

# Setup the unknown package handler

package unknown tclPkgUnknown

# Conditionalize for presence of exec.

if {[info commands exec] == ""} {

    # Some machines, such as the Macintosh, do not have exec. Also, on all
    # platforms, safe interpreters do not have exec.

    set auto_noexec 1
}
set errorCode ""
set errorInfo ""

# Define a log command (which can be overwitten to log errors
# differently, specially when stderr is not available)

if {[info commands tclLog] == ""} {
    proc tclLog {string} {
	catch {puts stderr $string}
    }
}

# unknown --
# This procedure is called when a Tcl command is invoked that doesn't
# exist in the interpreter.  It takes the following steps to make the
# command available:
#
#	1. See if the command has the form "namespace inscope ns cmd" and
#	   if so, concatenate its arguments onto the end and evaluate it.
#	2. See if the autoload facility can locate the command in a
#	   Tcl script file.  If so, load it and execute it.
#	3. If the command was invoked interactively at top-level:
#	    (a) see if the command exists as an executable UNIX program.
#		If so, "exec" the command.
#	    (b) see if the command requests csh-like history substitution
#		in one of the common forms !!, !<number>, or ^old^new.  If
#		so, emulate csh's history substitution.
#	    (c) see if the command is a unique abbreviation for another
#		command.  If so, invoke the command.
#
# Arguments:
# args -	A list whose elements are the words of the original
#		command, including the command name.

proc unknown args {
    global auto_noexec auto_noload env unknown_pending tcl_interactive
    global errorCode errorInfo

    # If the command word has the form "namespace inscope ns cmd"
    # then concatenate its arguments onto the end and evaluate it.

    set cmd [lindex $args 0]
    if {[regexp "^namespace\[ \t\n\]+inscope" $cmd] && [llength $cmd] == 4} {
        set arglist [lrange $args 1 end]
	set ret [catch {uplevel $cmd $arglist} result]
        if {$ret == 0} {
            return $result
        } else {
	    return -code $ret -errorcode $errorCode $result
        }
    }

    # Save the values of errorCode and errorInfo variables, since they
    # may get modified if caught errors occur below.  The variables will
    # be restored just before re-executing the missing command.

    set savedErrorCode $errorCode
    set savedErrorInfo $errorInfo
    set name [lindex $args 0]
    if {![info exists auto_noload]} {
	#
	# Make sure we're not trying to load the same proc twice.
	#
	if {[info exists unknown_pending($name)]} {
	    return -code error "self-referential recursion in \"unknown\" for command \"$name\"";
	}
	set unknown_pending($name) pending;
	set ret [catch {auto_load $name [uplevel 1 {namespace current}]} msg]
	unset unknown_pending($name);
	if {$ret != 0} {
	    return -code $ret -errorcode $errorCode \
		"error while autoloading \"$name\": $msg"
	}
	if {![array size unknown_pending]} {
	    unset unknown_pending
	}
	if {$msg} {
	    set errorCode $savedErrorCode
	    set errorInfo $savedErrorInfo
	    set code [catch {uplevel 1 $args} msg]
	    if {$code ==  1} {
		#
		# Strip the last five lines off the error stack (they're
		# from the "uplevel" command).
		#

		set new [split $errorInfo \n]
		set new [join [lrange $new 0 [expr {[llength $new] - 6}]] \n]
		return -code error -errorcode $errorCode \
			-errorinfo $new $msg
	    } else {
		return -code $code $msg
	    }
	}
    }

    if {([info level] == 1) && ([info script] == "") \
	    && [info exists tcl_interactive] && $tcl_interactive} {
	if {![info exists auto_noexec]} {
	    set new [auto_execok $name]
	    if {$new != ""} {
		set errorCode $savedErrorCode
		set errorInfo $savedErrorInfo
		set redir ""
		if {[info commands console] == ""} {
		    set redir ">&@stdout <@stdin"
		}
		return [uplevel exec $redir $new [lrange $args 1 end]]
	    }
	}
	set errorCode $savedErrorCode
	set errorInfo $savedErrorInfo
	if {$name == "!!"} {
	    set newcmd [history event]
	} elseif {[regexp {^!(.+)$} $name dummy event]} {
	    set newcmd [history event $event]
	} elseif {[regexp {^\^([^^]*)\^([^^]*)\^?$} $name dummy old new]} {
	    set newcmd [history event -1]
	    catch {regsub -all -- $old $newcmd $new newcmd}
	}
	if {[info exists newcmd]} {
	    tclLog $newcmd
	    history change $newcmd 0
	    return [uplevel $newcmd]
	}

	set ret [catch {set cmds [info commands $name*]} msg]
	if {[string compare $name "::"] == 0} {
	    set name ""
	}
	if {$ret != 0} {
	    return -code $ret -errorcode $errorCode \
		"error in unknown while checking if \"$name\" is a unique command abbreviation: $msg"
	}
	if {[llength $cmds] == 1} {
	    return [uplevel [lreplace $args 0 0 $cmds]]
	}
	if {[llength $cmds] != 0} {
	    if {$name == ""} {
		return -code error "empty command name \"\""
	    } else {
		return -code error \
			"ambiguous command name \"$name\": [lsort $cmds]"
	    }
	}
    }
    return -code error "invalid command name \"$name\""
}

# auto_load --
# Checks a collection of library directories to see if a procedure
# is defined in one of them.  If so, it sources the appropriate
# library file to create the procedure.  Returns 1 if it successfully
# loaded the procedure, 0 otherwise.
#
# Arguments: 
# cmd -			Name of the command to find and load.
# namespace (optional)  The namespace where the command is being used - must be
#                       a canonical namespace as returned [namespace current]
#                       for instance. If not given, namespace current is used.

proc auto_load {cmd {namespace {}}} {
    global auto_index auto_oldpath auto_path

    if {[string length $namespace] == 0} {
	set namespace [uplevel {namespace current}]
    }
    set nameList [auto_qualify $cmd $namespace]
    # workaround non canonical auto_index entries that might be around
    # from older auto_mkindex versions
    lappend nameList $cmd
    foreach name $nameList {
	if {[info exists auto_index($name)]} {
	    uplevel #0 $auto_index($name)
	    return [expr {[info commands $name] != ""}]
	}
    }
    if {![info exists auto_path]} {
	return 0
    }

    if {![auto_load_index]} {
	return 0
    }

    foreach name $nameList {
	if {[info exists auto_index($name)]} {
	    uplevel #0 $auto_index($name)
	    if {[info commands $name] != ""} {
		return 1
	    }
	}
    }
    return 0
}

# auto_load_index --
# Loads the contents of tclIndex files on the auto_path directory
# list.  This is usually invoked within auto_load to load the index
# of available commands.  Returns 1 if the index is loaded, and 0 if
# the index is already loaded and up to date.
#
# Arguments: 
# None.

proc auto_load_index {} {
    global auto_index auto_oldpath auto_path errorInfo errorCode

    if {[info exists auto_oldpath]} {
	if {$auto_oldpath == $auto_path} {
	    return 0
	}
    }
    set auto_oldpath $auto_path

    # Check if we are a safe interpreter. In that case, we support only
    # newer format tclIndex files.

    set issafe [interp issafe]
    for {set i [expr {[llength $auto_path] - 1}]} {$i >= 0} {incr i -1} {
	set dir [lindex $auto_path $i]
	set f ""
	if {$issafe} {
	    catch {source [file join $dir tclIndex]}
	} elseif {[catch {set f [open [file join $dir tclIndex]]}]} {
	    continue
	} else {
	    set error [catch {
		set id [gets $f]
		if {$id == "# Tcl autoload index file, version 2.0"} {
		    eval [read $f]
		} elseif {$id == \
		    "# Tcl autoload index file: each line identifies a Tcl"} {
		    while {[gets $f line] >= 0} {
			if {([string index $line 0] == "#")
				|| ([llength $line] != 2)} {
			    continue
			}
			set name [lindex $line 0]
			set auto_index($name) \
			    "source [file join $dir [lindex $line 1]]"
		    }
		} else {
		    error \
		      "[file join $dir tclIndex] isn't a proper Tcl index file"
		}
	    } msg]
	    if {$f != ""} {
		close $f
	    }
	    if {$error} {
		error $msg $errorInfo $errorCode
	    }
	}
    }
    return 1
}

# auto_qualify --
# compute a fully qualified names list for use in the auto_index array.
# For historical reasons, commands in the global namespace do not have leading
# :: in the index key. The list has two elements when the command name is
# relative (no leading ::) and the namespace is not the global one. Otherwise
# only one name is returned (and searched in the auto_index).
#
# Arguments -
# cmd		The command name. Can be any name accepted for command
#               invocations (Like "foo::::bar").
# namespace	The namespace where the command is being used - must be
#               a canonical namespace as returned by [namespace current]
#               for instance.

proc auto_qualify {cmd namespace} {

    # count separators and clean them up
    # (making sure that foo:::::bar will be treated as foo::bar)
    set n [regsub -all {::+} $cmd :: cmd]

    # Ignore namespace if the name starts with ::
    # Handle special case of only leading ::

    # Before each return case we give an example of which category it is
    # with the following form :
    # ( inputCmd, inputNameSpace) -> output

    if {[regexp {^::(.*)$} $cmd x tail]} {
	if {$n > 1} {
	    # ( ::foo::bar , * ) -> ::foo::bar
	    return [list $cmd]
	} else {
	    # ( ::global , * ) -> global
	    return [list $tail]
	}
    }
    
    # Potentially returning 2 elements to try  :
    # (if the current namespace is not the global one)

    if {$n == 0} {
	if {[string compare $namespace ::] == 0} {
	    # ( nocolons , :: ) -> nocolons
	    return [list $cmd]
	} else {
	    # ( nocolons , ::sub ) -> ::sub::nocolons nocolons
	    return [list ${namespace}::$cmd $cmd]
	}
    } else {
	if {[string compare $namespace ::] == 0} {
	    #  ( foo::bar , :: ) -> ::foo::bar
	    return [list ::$cmd]
	} else {
	    # ( foo::bar , ::sub ) -> ::sub::foo::bar ::foo::bar
	    return [list ${namespace}::$cmd ::$cmd]
	}
    }
}

# auto_import --
# invoked during "namespace import" to make see if the imported commands
# reside in an autoloaded library.  If so, the commands are loaded so
# that they will be available for the import links.  If not, then this
# procedure does nothing.
#
# Arguments -
# pattern	The pattern of commands being imported (like "foo::*")
#               a canonical namespace as returned by [namespace current]

proc auto_import {pattern} {
    global auto_index

    set ns [uplevel namespace current]
    set patternList [auto_qualify $pattern $ns]

    auto_load_index

    foreach pattern $patternList {
        foreach name [array names auto_index] {
            if {[string match $pattern $name] && "" == [info commands $name]} {
                uplevel #0 $auto_index($name)
            }
        }
    }
}

if {[string compare $tcl_platform(platform) windows] == 0} {

# auto_execok --
#
# Returns string that indicates name of program to execute if 
# name corresponds to a shell builtin or an executable in the
# Windows search path, or "" otherwise.  Builds an associative 
# array auto_execs that caches information about previous checks, 
# for speed.
#
# Arguments: 
# name -			Name of a command.

# Windows version.
#
# Note that info executable doesn't work under Windows, so we have to
# look for files with .exe, .com, or .bat extensions.  Also, the path
# may be in the Path or PATH environment variables, and path
# components are separated with semicolons, not colons as under Unix.
#
proc auto_execok name {
    global auto_execs env tcl_platform

    if {[info exists auto_execs($name)]} {
	return $auto_execs($name)
    }
    set auto_execs($name) ""

    if {[lsearch -exact {cls copy date del erase dir echo mkdir md rename 
	    ren rmdir rd time type ver vol} $name] != -1} {
	return [set auto_execs($name) [list $env(COMSPEC) /c $name]]
    }

    if {[llength [file split $name]] != 1} {
	foreach ext {{} .com .exe .bat} {
	    set file ${name}${ext}
	    if {[file exists $file] && ![file isdirectory $file]} {
		return [set auto_execs($name) [list $file]]
	    }
	}
	return ""
    }

    set path "[file dirname [info nameof]];.;"
    if {[info exists env(WINDIR)]} {
	set windir $env(WINDIR) 
    }
    if {[info exists windir]} {
	if {$tcl_platform(os) == "Windows NT"} {
	    append path "$windir/system32;"
	}
	append path "$windir/system;$windir;"
    }

    if {[info exists env(PATH)]} {
	append path $env(PATH)
    }

    foreach dir [split $path {;}] {
	if {$dir == ""} {
	    set dir .
	}
	foreach ext {{} .com .exe .bat} {
	    set file [file join $dir ${name}${ext}]
	    if {[file exists $file] && ![file isdirectory $file]} {
		return [set auto_execs($name) [list $file]]
	    }
	}
    }
    return ""
}

} else {

# auto_execok --
#
# Returns string that indicates name of program to execute if 
# name corresponds to an executable in the path. Builds an associative 
# array auto_execs that caches information about previous checks, 
# for speed.
#
# Arguments: 
# name -			Name of a command.

# Unix version.
#
proc auto_execok name {
    global auto_execs env

    if {[info exists auto_execs($name)]} {
	return $auto_execs($name)
    }
    set auto_execs($name) ""
    if {[llength [file split $name]] != 1} {
	if {[file executable $name] && ![file isdirectory $name]} {
	    set auto_execs($name) [list $name]
	}
	return $auto_execs($name)
    }
    foreach dir [split $env(PATH) :] {
	if {$dir == ""} {
	    set dir .
	}
	set file [file join $dir $name]
	if {[file executable $file] && ![file isdirectory $file]} {
	    set auto_execs($name) [list $file]
	    return $auto_execs($name)
	}
    }
    return ""
}

}
# auto_reset --
# Destroy all cached information for auto-loading and auto-execution,
# so that the information gets recomputed the next time it's needed.
# Also delete any procedures that are listed in the auto-load index
# except those defined in this file.
#
# Arguments: 
# None.

proc auto_reset {} {
    global auto_execs auto_index auto_oldpath
    foreach p [info procs] {
	if {[info exists auto_index($p)] && ![string match auto_* $p]
		&& ([lsearch -exact {unknown pkg_mkIndex tclPkgSetup
			tcl_findLibrary pkg_compareExtension
			tclMacPkgSearch tclPkgUnknown} $p] < 0)} {
	    rename $p {}
	}
    }
    catch {unset auto_execs}
    catch {unset auto_index}
    catch {unset auto_oldpath}
}

# tcl_findLibrary
#	This is a utility for extensions that searches for a library directory
#	using a canonical searching algorithm. A side effect is to source
#	the initialization script and set a global library variable.
# Arguments:
# 	basename	Prefix of the directory name, (e.g., "tk")
#	version		Version number of the package, (e.g., "8.0")
#	patch		Patchlevel of the package, (e.g., "8.0.3")
#	initScript	Initialization script to source (e.g., tk.tcl)
#	enVarName	environment variable to honor (e.g., TK_LIBRARY)
#	varName		Global variable to set when done (e.g., tk_library)

proc tcl_findLibrary {basename version patch initScript enVarName varName} {
    upvar #0 $varName the_library
    global env errorInfo

    set dirs {}
    set errors {}

    # The C application may have hardwired a path, which we honor
    
    if {[info exist the_library]} {
	lappend dirs $the_library
    } else {

	# Do the canonical search

	# 1. From an environment variable, if it exists

        if {[info exists env($enVarName)]} {
            lappend dirs $env($enVarName)
        }

	# 2. Relative to the Tcl library

        lappend dirs [file join [file dirname [info library]] $basename$version]

	# 3. Various locations relative to the executable
	# ../lib/foo1.0		(From bin directory in install hierarchy)
	# ../../lib/foo1.0	(From bin/arch directory in install hierarchy)
	# ../library		(From unix directory in build hierarchy)
	# ../../library		(From unix/arch directory in build hierarchy)
	# ../../foo1.0b1/library (From unix directory in parallel build hierarchy)
	# ../../../foo1.0b1/library (From unix/arch directory in parallel build hierarchy)

        set parentDir [file dirname [file dirname [info nameofexecutable]]]
        set grandParentDir [file dirname $parentDir]
        lappend dirs [file join $parentDir lib $basename$version]
        lappend dirs [file join $grandParentDir lib $basename$version]
        lappend dirs [file join $parentDir library]
        lappend dirs [file join $grandParentDir library]
        if {[string match {*[ab]*} $patch]} {
            set ver $patch
        } else {
            set ver $version
        }
        lappend dirs [file join $grandParentDir $basename$ver library]
        lappend dirs [file join [file dirname $grandParentDir] $basename$ver library]
    }
    foreach i $dirs {
        set the_library $i
        set file [file join $i $initScript]

	# source everything when in a safe interpreter because
	# we have a source command, but no file exists command

        if {[interp issafe] || [file exists $file]} {
            if {![catch {uplevel #0 [list source $file]} msg]} {
                return
            } else {
                append errors "$file: $msg\n$errorInfo\n"
            }
        }
    }
    set msg "Can't find a usable $initScript in the following directories: \n"
    append msg "    $dirs\n\n"
    append msg "$errors\n\n"
    append msg "This probably means that $basename wasn't installed properly.\n"
    error $msg
}


# OPTIONAL SUPPORT PROCEDURES
# In Tcl 8.1 all the code below here has been moved to other files to
# reduce the size of init.tcl

# ----------------------------------------------------------------------
# auto_mkindex
# ----------------------------------------------------------------------
# The following procedures are used to generate the tclIndex file
# from Tcl source files.  They use a special safe interpreter to
# parse Tcl source files, writing out index entries as "proc"
# commands are encountered.  This implementation won't work in a
# safe interpreter, since a safe interpreter can't create the
# special parser and mess with its commands.  If this is a safe
# interpreter, we simply clip these procs out.

if {! [interp issafe]} {

    # auto_mkindex --
    # Regenerate a tclIndex file from Tcl source files.  Takes as argument
    # the name of the directory in which the tclIndex file is to be placed,
    # followed by any number of glob patterns to use in that directory to
    # locate all of the relevant files.
    #
    # Arguments: 
    # dir -		Name of the directory in which to create an index.
    # args -	Any number of additional arguments giving the
    #		names of files within dir.  If no additional
    #		are given auto_mkindex will look for *.tcl.

    proc auto_mkindex {dir args} {
	global errorCode errorInfo

	set oldDir [pwd]
	cd $dir
	set dir [pwd]

	append index "# Tcl autoload index file, version 2.0\n"
	append index "# This file is generated by the \"auto_mkindex\" command\n"
	append index "# and sourced to set up indexing information for one or\n"
	append index "# more commands.  Typically each line is a command that\n"
	append index "# sets an element in the auto_index array, where the\n"
	append index "# element name is the name of a command and the value is\n"
	append index "# a script that loads the command.\n\n"
	if {$args == ""} {
	    set args *.tcl
	}
	auto_mkindex_parser::init
	foreach file [eval glob $args] {
	    if {[catch {auto_mkindex_parser::mkindex $file} msg] == 0} {
		append index $msg
	    } else {
		set code $errorCode
		set info $errorInfo
		cd $oldDir
		error $msg $info $code
	    }
	}
	auto_mkindex_parser::cleanup

	set fid [open "tclIndex" w]
	puts $fid $index nonewline
	close $fid
	cd $oldDir
    }

    # Original version of auto_mkindex that just searches the source
    # code for "proc" at the beginning of the line.

    proc auto_mkindex_old {dir args} {
	global errorCode errorInfo
	set oldDir [pwd]
	cd $dir
	set dir [pwd]
	append index "# Tcl autoload index file, version 2.0\n"
	append index "# This file is generated by the \"auto_mkindex\" command\n"
	append index "# and sourced to set up indexing information for one or\n"
	append index "# more commands.  Typically each line is a command that\n"
	append index "# sets an element in the auto_index array, where the\n"
	append index "# element name is the name of a command and the value is\n"
	append index "# a script that loads the command.\n\n"
	if {$args == ""} {
	    set args *.tcl
	}
	foreach file [eval glob $args] {
	    set f ""
	    set error [catch {
		set f [open $file]
		while {[gets $f line] >= 0} {
		    if {[regexp {^proc[ 	]+([^ 	]*)} $line match procName]} {
			set procName [lindex [auto_qualify $procName "::"] 0]
			append index "set [list auto_index($procName)]"
			append index " \[list source \[file join \$dir [list $file]\]\]\n"
		    }
		}
		close $f
	    } msg]
	    if {$error} {
		set code $errorCode
		set info $errorInfo
		catch {close $f}
		cd $oldDir
		error $msg $info $code
	    }
	}
	set f ""
	set error [catch {
	    set f [open tclIndex w]
	    puts $f $index nonewline
	    close $f
	    cd $oldDir
	} msg]
	if {$error} {
	    set code $errorCode
	    set info $errorInfo
	    catch {close $f}
	    cd $oldDir
	    error $msg $info $code
	}
    }

    # Create a safe interpreter that can be used to parse Tcl source files
    # generate a tclIndex file for autoloading.  This interp contains
    # commands for things that need index entries.  Each time a command
    # is executed, it writes an entry out to the index file.

    namespace eval auto_mkindex_parser {
	variable parser ""          ;# parser used to build index
	variable index ""           ;# maintains index as it is built
	variable scriptFile ""      ;# name of file being processed
	variable contextStack ""    ;# stack of namespace scopes
	variable imports ""         ;# keeps track of all imported cmds
	variable initCommands ""    ;# list of commands that create aliases
	proc init {} {
	    variable parser
	    variable initCommands
	    if {![interp issafe]} {
		set parser [interp create -safe]
		$parser hide info
		$parser hide rename
		$parser hide proc
		$parser hide namespace
		$parser hide eval
		$parser hide puts
		$parser invokehidden namespace delete ::
		$parser invokehidden proc unknown {args} {}

		#
		# We'll need access to the "namespace" command within the
		# interp.  Put it back, but move it out of the way.
		#
		$parser expose namespace
		$parser invokehidden rename namespace _%@namespace
		$parser expose eval
		$parser invokehidden rename eval _%@eval

		# Install all the registered psuedo-command implementations

		foreach cmd $initCommands {
		    eval $cmd
		}
	    }
	}
	proc cleanup {} {
	    variable parser
	    interp delete $parser
	    unset parser
	}
    }

    # auto_mkindex_parser::mkindex --
    # Used by the "auto_mkindex" command to create a "tclIndex" file for
    # the given Tcl source file.  Executes the commands in the file, and
    # handles things like the "proc" command by adding an entry for the
    # index file.  Returns a string that represents the index file.
    #
    # Arguments: 
    # file -		Name of Tcl source file to be indexed.

    proc auto_mkindex_parser::mkindex {file} {
	variable parser
	variable index
	variable scriptFile
	variable contextStack
	variable imports

	set scriptFile $file

	set fid [open $file]
	set contents [read $fid]
	close $fid

	# There is one problem with sourcing files into the safe
	# interpreter:  references like "$x" will fail since code is not
	# really being executed and variables do not really exist.
	# Be careful to escape all naked "$" before evaluating.

	regsub -all {([^\$])\$([^\$])} $contents {\1\\$\2} contents

	set index ""
	set contextStack ""
	set imports ""

	$parser eval $contents

	foreach name $imports {
	    catch {$parser eval [list _%@namespace forget $name]}
	}
	return $index
    }

    # auto_mkindex_parser::hook command
    # Registers a Tcl command to evaluate when initializing the
    # slave interpreter used by the mkindex parser.
    # The command is evaluated in the master interpreter, and can
    # use the variable auto_mkindex_parser::parser to get to the slave

    proc auto_mkindex_parser::hook {cmd} {
	variable initCommands

	lappend initCommands $cmd
    }

    # auto_mkindex_parser::slavehook command
    # Registers a Tcl command to evaluate when initializing the
    # slave interpreter used by the mkindex parser.
    # The command is evaluated in the slave interpreter.

    proc auto_mkindex_parser::slavehook {cmd} {
	variable initCommands

	lappend initCommands "\$parser eval [list $cmd]"
    }

    # auto_mkindex_parser::command --
    # Registers a new command with the "auto_mkindex_parser" interpreter
    # that parses Tcl files.  These commands are fake versions of things
    # like the "proc" command.  When you execute them, they simply write
    # out an entry to a "tclIndex" file for auto-loading.
    #
    # This procedure allows extensions to register their own commands
    # with the auto_mkindex facility.  For example, a package like
    # [incr Tcl] might register a "class" command so that class definitions
    # could be added to a "tclIndex" file for auto-loading.
    #
    # Arguments:
    # name -		Name of command recognized in Tcl files.
    # arglist -		Argument list for command.
    # body -		Implementation of command to handle indexing.

    proc auto_mkindex_parser::command {name arglist body} {
	hook [list auto_mkindex_parser::commandInit $name $arglist $body]
    }

    # auto_mkindex_parser::commandInit --
    # This does the actual work set up by auto_mkindex_parser::command
    # This is called when the interpreter used by the parser is created.

    proc auto_mkindex_parser::commandInit {name arglist body} {
	variable parser

	set ns [namespace qualifiers $name]
	set tail [namespace tail $name]
	if {$ns == ""} {
	    set fakeName "[namespace current]::_%@fake_$tail"
	} else {
	    set fakeName "_%@fake_$name"
	    regsub -all {::} $fakeName "_" fakeName
	    set fakeName "[namespace current]::$fakeName"
	}
	proc $fakeName $arglist $body

	#
	# YUK!  Tcl won't let us alias fully qualified command names,
	# so we can't handle names like "::itcl::class".  Instead,
	# we have to build procs with the fully qualified names, and
	# have the procs point to the aliases.
	#
	if {[regexp {::} $name]} {
	    set exportCmd [list _%@namespace export [namespace tail $name]]
	    $parser eval [list _%@namespace eval $ns $exportCmd]
	    set alias [namespace tail $fakeName]
	    $parser invokehidden proc $name {args} "_%@eval $alias \$args"
	    $parser alias $alias $fakeName
	} else {
	    $parser alias $name $fakeName
	}
	return
    }

    # auto_mkindex_parser::fullname --
    # Used by commands like "proc" within the auto_mkindex parser.
    # Returns the qualified namespace name for the "name" argument.
    # If the "name" does not start with "::", elements are added from
    # the current namespace stack to produce a qualified name.  Then,
    # the name is examined to see whether or not it should really be
    # qualified.  If the name has more than the leading "::", it is
    # returned as a fully qualified name.  Otherwise, it is returned
    # as a simple name.  That way, the Tcl autoloader will recognize
    # it properly.
    #
    # Arguments:
    # name -		Name that is being added to index.

    proc auto_mkindex_parser::fullname {name} {
	variable contextStack

	if {![string match ::* $name]} {
	    foreach ns $contextStack {
		set name "${ns}::$name"
		if {[string match ::* $name]} {
		    break
		}
	    }
	}

	if {[namespace qualifiers $name] == ""} {
	    return [namespace tail $name]
	} elseif {![string match ::* $name]} {
	    return "::$name"
	}
	return $name
    }

    # Register all of the procedures for the auto_mkindex parser that
    # will build the "tclIndex" file.

    # AUTO MKINDEX:  proc name arglist body
    # Adds an entry to the auto index list for the given procedure name.

    auto_mkindex_parser::command proc {name args} {
	variable index
	variable scriptFile
	append index "set [list auto_index([fullname $name])]"
	append index " \[list source \[file join \$dir [list $scriptFile]\]\]\n"
    }

    # AUTO MKINDEX:  namespace eval name command ?arg arg...?
    # Adds the namespace name onto the context stack and evaluates the
    # associated body of commands.
    #
    # AUTO MKINDEX:  namespace import ?-force? pattern ?pattern...?
    # Performs the "import" action in the parser interpreter.  This is
    # important for any commands contained in a namespace that affect
    # the index.  For example, a script may say "itcl::class ...",
    # or it may import "itcl::*" and then say "class ...".  This
    # procedure does the import operation, but keeps track of imported
    # patterns so we can remove the imports later.

    auto_mkindex_parser::command namespace {op args} {
	switch -- $op {
	    eval {
		variable parser
		variable contextStack

		set name [lindex $args 0]
		set args [lrange $args 1 end]

		set contextStack [linsert $contextStack 0 $name]
		if {[llength $args] == 1} {
		    $parser eval [lindex $args 0]
		} else {
		    eval $parser eval $args
		}
		set contextStack [lrange $contextStack 1 end]
	    }
	    import {
		variable parser
		variable imports
		foreach pattern $args {
		    if {$pattern != "-force"} {
			lappend imports $pattern
		    }
		}
		catch {$parser eval "_%@namespace import $args"}
	    }
	}
    }

# Close of the if ![interp issafe] block
}

# pkg_compareExtension --
#
#  Used internally by pkg_mkIndex to compare the extension of a file to
#  a given extension. On Windows, it uses a case-insensitive comparison.
#
# Arguments:
#  fileName	name of a file whose extension is compared
#  ext		(optional) The extension to compare against; you must
#		provide the starting dot.
#		Defaults to [info sharedlibextension]
#
# Results:
#  Returns 1 if the extension matches, 0 otherwise

proc pkg_compareExtension { fileName {ext {}} } {
    global tcl_platform
    if {[string length $ext] == 0} {
	set ext [info sharedlibextension]
    }
    if {[string compare $tcl_platform(platform) "windows"] == 0} {
	return [expr {[string compare \
		[string tolower [file extension $fileName]] \
		[string tolower $ext]] == 0}]
    } else {
	return [expr {[string compare [file extension $fileName] $ext] == 0}]
    }
}

# pkg_mkIndex --
# This procedure creates a package index in a given directory.  The
# package index consists of a "pkgIndex.tcl" file whose contents are
# a Tcl script that sets up package information with "package require"
# commands.  The commands describe all of the packages defined by the
# files given as arguments.
#
# Arguments:
# -direct		(optional) If this flag is present, the generated
#			code in pkgMkIndex.tcl will cause the package to be
#			loaded when "package require" is executed, rather
#			than lazily when the first reference to an exported
#			procedure in the package is made.
# -verbose		(optional) Verbose output; the name of each file that
#			was successfully rocessed is printed out. Additionally,
#			if processing of a file failed a message is printed.
# -load pat		(optional) Preload any packages whose names match
#			the pattern.  Used to handle DLLs that depend on
#			other packages during their Init procedure.
# dir -			Name of the directory in which to create the index.
# args -		Any number of additional arguments, each giving
#			a glob pattern that matches the names of one or
#			more shared libraries or Tcl script files in
#			dir.

proc pkg_mkIndex {args} {
    global errorCode errorInfo
    set usage {"pkg_mkIndex ?-direct? ?-verbose? ?-load pattern? ?--? dir ?pattern ...?"};

    set argCount [llength $args]
    if {$argCount < 1} {
	return -code error "wrong # args: should be\n$usage"
    }

    set more ""
    set direct 0
    set doVerbose 0
    set loadPat ""
    for {set idx 0} {$idx < $argCount} {incr idx} {
	set flag [lindex $args $idx]
	switch -glob -- $flag {
	    -- {
		# done with the flags
		incr idx
		break
	    }
	    -verbose {
		set doVerbose 1
	    }
	    -direct {
		set direct 1
		append more " -direct"
	    }
	    -load {
		incr idx
		set loadPat [lindex $args $idx]
		append more " -load $loadPat"
	    }
	    -* {
		return -code error "unknown flag $flag: should be\n$usage"
	    }
	    default {
		# done with the flags
		break
	    }
	}
    }

    set dir [lindex $args $idx]
    set patternList [lrange $args [expr {$idx + 1}] end]
    if {[llength $patternList] == 0} {
	set patternList [list "*.tcl" "*[info sharedlibextension]"]
    }

    append index "# Tcl package index file, version 1.1\n"
    append index "# This file is generated by the \"pkg_mkIndex$more\" command\n"
    append index "# and sourced either when an application starts up or\n"
    append index "# by a \"package unknown\" script.  It invokes the\n"
    append index "# \"package ifneeded\" command to set up package-related\n"
    append index "# information so that packages will be loaded automatically\n"
    append index "# in response to \"package require\" commands.  When this\n"
    append index "# script is sourced, the variable \$dir must contain the\n"
    append index "# full path name of this file's directory.\n"
    set oldDir [pwd]
    cd $dir

    if {[catch {eval glob $patternList} fileList]} {
	global errorCode errorInfo
	cd $oldDir
	return -code error -errorcode $errorCode -errorinfo $errorInfo $fileList
    }
    foreach file $fileList {
	# For each file, figure out what commands and packages it provides.
	# To do this, create a child interpreter, load the file into the
	# interpreter, and get a list of the new commands and packages
	# that are defined.

	if {[string compare $file "pkgIndex.tcl"] == 0} {
	    continue
	}

	# Changed back to the original directory before initializing the
	# slave in case TCL_LIBRARY is a relative path (e.g. in the test
	# suite). 

	cd $oldDir
	set c [interp create]

	# Load into the child any packages currently loaded in the parent
	# interpreter that match the -load pattern.

	foreach pkg [info loaded] {
	    if {! [string match $loadPat [lindex $pkg 1]]} {
		continue
	    }
	    if {[lindex $pkg 1] == "Tk"} {
		$c eval {set argv {-geometry +0+0}}
	    }
	    if {[catch {
		load [lindex $pkg 0] [lindex $pkg 1] $c
	    } err]} {
		if {$doVerbose} {
		    tclLog "warning: load [lindex $pkg 0] [lindex $pkg 1]\nfailed with: $err"
		}
	    } else {
		if {$doVerbose} {
		    tclLog "loaded [lindex $pkg 0] [lindex $pkg 1]"
		}
	    }
	}
	cd $dir

	$c eval {
	    # Stub out the package command so packages can
	    # require other packages.

	    rename package __package_orig
	    proc package {what args} {
		switch -- $what {
		    require { return ; # ignore transitive requires }
		    default { eval __package_orig {$what} $args }
		}
	    }
	    proc tclPkgUnknown args {}
	    package unknown tclPkgUnknown

	    # Stub out the unknown command so package can call
	    # into each other during their initialilzation.

	    proc unknown {args} {}

	    # Stub out the auto_import mechanism

	    proc auto_import {args} {}

	    # reserve the ::tcl namespace for support procs
	    # and temporary variables.  This might make it awkward
	    # to generate a pkgIndex.tcl file for the ::tcl namespace.

	    namespace eval ::tcl {
		variable file		;# Current file being processed
		variable direct		;# -direct flag value
		variable x		;# Loop variable
		variable debug		;# For debugging
		variable type		;# "load" or "source", for -direct
		variable namespaces	;# Existing namespaces (e.g., ::tcl)
		variable packages	;# Existing packages (e.g., Tcl)
		variable origCmds	;# Existing commands
		variable newCmds	;# Newly created commands
		variable newPkgs {}	;# Newly created packages
	    }
	}

	$c eval [list set ::tcl::file $file]
	$c eval [list set ::tcl::direct $direct]
	if {[catch {
	    $c eval {
		set ::tcl::debug "loading or sourcing"

		# we need to track command defined by each package even in
		# the -direct case, because they are needed internally by
		# the "partial pkgIndex.tcl" step above.

		proc ::tcl::GetAllNamespaces {{root ::}} {
		    set list $root
		    foreach ns [namespace children $root] {
			eval lappend list [::tcl::GetAllNamespaces $ns]
		    }
		    return $list
		}

		# initialize the list of existing namespaces, packages, commands

		foreach ::tcl::x [::tcl::GetAllNamespaces] {
		    set ::tcl::namespaces($::tcl::x) 1
		}
		foreach ::tcl::x [package names] {
		    set ::tcl::packages($::tcl::x) 1
		}
		set ::tcl::origCmds [info commands]

		# Try to load the file if it has the shared library
		# extension, otherwise source it.  It's important not to
		# try to load files that aren't shared libraries, because
		# on some systems (like SunOS) the loader will abort the
		# whole application when it gets an error.

		if {[pkg_compareExtension $::tcl::file [info sharedlibextension]]} {
		    # The "file join ." command below is necessary.
		    # Without it, if the file name has no \'s and we're
		    # on UNIX, the load command will invoke the
		    # LD_LIBRARY_PATH search mechanism, which could cause
		    # the wrong file to be used.

		    set ::tcl::debug loading
		    load [file join . $::tcl::file]
		    set ::tcl::type load
		} else {
		    set ::tcl::debug sourcing
		    source $::tcl::file
		    set ::tcl::type source
		}

		# See what new namespaces appeared, and import commands
		# from them.  Only exported commands go into the index.

		foreach ::tcl::x [::tcl::GetAllNamespaces] {
		    if {! [info exists ::tcl::namespaces($::tcl::x)]} {
			namespace import ${::tcl::x}::*
		    }
		}

		# Figure out what commands appeared

		foreach ::tcl::x [info commands] {
		    set ::tcl::newCmds($::tcl::x) 1
		}
		foreach ::tcl::x $::tcl::origCmds {
		    catch {unset ::tcl::newCmds($::tcl::x)}
		}
		foreach ::tcl::x [array names ::tcl::newCmds] {
		    # reverse engineer which namespace a command comes from
		    
		    set ::tcl::abs [namespace origin $::tcl::x]

		    # special case so that global names have no leading
		    # ::, this is required by the unknown command

		    set ::tcl::abs [auto_qualify $::tcl::abs ::]

		    if {[string compare $::tcl::x $::tcl::abs] != 0} {
			# Name changed during qualification

			set ::tcl::newCmds($::tcl::abs) 1
			unset ::tcl::newCmds($::tcl::x)
		    }
		}

		# Look through the packages that appeared, and if there is
		# a version provided, then record it

		foreach ::tcl::x [package names] {
		    if {([string compare [package provide $::tcl::x] ""] != 0) \
			    && ![info exists ::tcl::packages($::tcl::x)]} {
			lappend ::tcl::newPkgs \
			    [list $::tcl::x [package provide $::tcl::x]]
		    }
		}
	    }
	} msg] == 1} {
	    set what [$c eval set ::tcl::debug]
	    if {$doVerbose} {
		tclLog "warning: error while $what $file: $msg"
	    }
	} else {
	    set type [$c eval set ::tcl::type]
	    set cmds [lsort [$c eval array names ::tcl::newCmds]]
	    set pkgs [$c eval set ::tcl::newPkgs]
	    if {[llength $pkgs] > 1} {
		tclLog "warning: \"$file\" provides more than one package ($pkgs)"
	    }
	    foreach pkg $pkgs {
		# cmds is empty/not used in the direct case
		lappend files($pkg) [list $file $type $cmds]
	    }

	    if {$doVerbose} {
		tclLog "processed $file"
	    }
	}
	interp delete $c
    }

    foreach pkg [lsort [array names files]] {
	append index "\npackage ifneeded $pkg "
	if {$direct} {
	    set cmdList {}
	    foreach elem $files($pkg) {
		set file [lindex $elem 0]
		set type [lindex $elem 1]
		lappend cmdList "\[list $type \[file join \$dir\
			[list $file]\]\]"
	    }
	    append index [join $cmdList "\\n"]
	} else {
	    append index "\[list tclPkgSetup \$dir [lrange $pkg 0 0]\
		    [lrange $pkg 1 1] [list $files($pkg)]\]"
	}
    }
    set f [open pkgIndex.tcl w]
    puts $f $index
    close $f
    cd $oldDir
}

# tclPkgSetup --
# This is a utility procedure use by pkgIndex.tcl files.  It is invoked
# as part of a "package ifneeded" script.  It calls "package provide"
# to indicate that a package is available, then sets entries in the
# auto_index array so that the package's files will be auto-loaded when
# the commands are used.
#
# Arguments:
# dir -			Directory containing all the files for this package.
# pkg -			Name of the package (no version number).
# version -		Version number for the package, such as 2.1.3.
# files -		List of files that constitute the package.  Each
#			element is a sub-list with three elements.  The first
#			is the name of a file relative to $dir, the second is
#			"load" or "source", indicating whether the file is a
#			loadable binary or a script to source, and the third
#			is a list of commands defined by this file.

proc tclPkgSetup {dir pkg version files} {
    global auto_index

    package provide $pkg $version
    foreach fileInfo $files {
	set f [lindex $fileInfo 0]
	set type [lindex $fileInfo 1]
	foreach cmd [lindex $fileInfo 2] {
	    if {$type == "load"} {
		set auto_index($cmd) [list load [file join $dir $f] $pkg]
	    } else {
		set auto_index($cmd) [list source [file join $dir $f]]
	    } 
	}
    }
}

# tclMacPkgSearch --
# The procedure is used on the Macintosh to search a given directory for files
# with a TEXT resource named "pkgIndex".  If it exists it is sourced in to the
# interpreter to setup the package database.

proc tclMacPkgSearch {dir} {
    foreach x [glob -nocomplain [file join $dir *.shlb]] {
	if {[file isfile $x]} {
	    set res [resource open $x]
	    foreach y [resource list TEXT $res] {
		if {$y == "pkgIndex"} {source -rsrc pkgIndex}
	    }
	    catch {resource close $res}
	}
    }
}

# tclPkgUnknown --
# This procedure provides the default for the "package unknown" function.
# It is invoked when a package that's needed can't be found.  It scans
# the auto_path directories and their immediate children looking for
# pkgIndex.tcl files and sources any such files that are found to setup
# the package database.  (On the Macintosh we also search for pkgIndex
# TEXT resources in all files.)
#
# Arguments:
# name -		Name of desired package.  Not used.
# version -		Version of desired package.  Not used.
# exact -		Either "-exact" or omitted.  Not used.

proc tclPkgUnknown {name version {exact {}}} {
    global auto_path tcl_platform env

    if {![info exists auto_path]} {
	return
    }
    for {set i [expr {[llength $auto_path] - 1}]} {$i >= 0} {incr i -1} {
	# we can't use glob in safe interps, so enclose the following
	# in a catch statement
	catch {
	    foreach file [glob -nocomplain [file join [lindex $auto_path $i] \
		    * pkgIndex.tcl]] {
		set dir [file dirname $file]
		if {[catch {source $file} msg]} {
		    tclLog "error reading package index file $file: $msg"
		}
	    }
        }
	set dir [lindex $auto_path $i]
	set file [file join $dir pkgIndex.tcl]
	# safe interps usually don't have "file readable", nor stderr channel
	if {[interp issafe] || [file readable $file]} {
	    if {[catch {source $file} msg] && ![interp issafe]}  {
		tclLog "error reading package index file $file: $msg"
	    }
	}
	# On the Macintosh we also look in the resource fork 
	# of shared libraries
	# We can't use tclMacPkgSearch in safe interps because it uses glob
	if {(![interp issafe]) && ($tcl_platform(platform) == "macintosh")} {
	    set dir [lindex $auto_path $i]
	    tclMacPkgSearch $dir
	    foreach x [glob -nocomplain [file join $dir *]] {
		if {[file isdirectory $x]} {
		    set dir $x
		    tclMacPkgSearch $dir
		}
	    }
	}
    }
}
