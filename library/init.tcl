# init.tcl --
#
# Default system startup file for Tcl-based applications.  Defines
# "unknown" procedure and auto-load facilities.
#
# SCCS: @(#) init.tcl 1.104 98/01/09 17:52:21
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
package require -exact Tcl 8.1

# Compute the auto path to use in this interpreter.
# (auto_path could be already set, in safe interps for instance
#  and some variables are usually exist might not be there, proceed
#  with caution)

if {![info exists auto_path]} {
    if {[catch {set auto_path $env(TCLLIBPATH)}]} {
	set auto_path ""
    }
    
    if {[lsearch -exact $auto_path $tcl_library] < 0} {
	lappend auto_path $tcl_library
    }

    set __dir [file dirname $tcl_library]

    if {[lsearch -exact $auto_path $__dir] < 0} {
	lappend auto_path $__dir
    }

    # Add directories from the tcl_pkgPath
    # (we might want to check the potential candidates in tcl_libPath too,
    #  and check that those dirs refer to compatible tcl versions
    #  (ie if they end with tcl7.6 we should prbably not add them))

    if {[info exist tcl_pkgPath]} {
	foreach __dir $tcl_pkgPath {
	    if {[lsearch -exact $auto_path $__dir] < 0} {
		lappend auto_path $__dir
	    }
	}
    }

    unset __dir
}

# Windows specific end of initialization

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

# The procs defined in this file that have a leading space
# are 'hidden' from auto_mkindex because they are not
# auto-loadable.


# unknown --
# This procedure is called when a Tcl command is invoked that doesn't
# exist in the interpreter.  It takes the following steps to make the
# command available:
#
#	1. See if the autoload facility can locate the command in a
#	   Tcl script file.  If so, load it and execute it.
#	2. If the command was invoked interactively at top-level:
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
    global auto_index auto_oldpath auto_path env errorInfo errorCode

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

