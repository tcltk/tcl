# init.tcl --
#
# Default system startup file for Tcl-based applications.  Defines
# "unknown" procedure and auto-load facilities.
#
# SCCS: @(#) init.tcl 1.8 98/07/20 16:24:45
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
# (auto_path could be already set, in safe interps for instance)

if {![info exists auto_path]} {
    if {[catch {set auto_path $env(TCLLIBPATH)}]} {
	set auto_path ""
    }
}
if {[lsearch -exact $auto_path [info library]] < 0} {
    lappend auto_path [info library]
}
catch {
    foreach __dir $tcl_pkgPath {
	if {[lsearch -exact $auto_path $__dir] < 0} {
	    lappend auto_path $__dir
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
			tclMacPkgSearch tclPkgUnknown} $p] < 0)} {
	    rename $p {}
	}
    }
    catch {unset auto_execs}
    catch {unset auto_index}
    catch {unset auto_oldpath}
}

# auto_mkindex --
# Regenerate a tclIndex file from Tcl source files.  Takes as argument
# the name of the directory in which the tclIndex file is to be placed,
# followed by any number of glob patterns to use in that directory to
# locate all of the relevant files. It does not parse or source the file
# so the generated index will not contain the appropriate namespace qualifiers
# if you don't explicitly specify it.
#
# Arguments: 
# dir -			Name of the directory in which to create an index.
# args -		Any number of additional arguments giving the
#			names of files within dir.  If no additional
#			are given auto_mkindex will look for *.tcl.

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
# -nopkgrequire		(optional) If this flag is present, "package require"
#			commands are ignored. This flag is useful in some
#			situations, for example when there is a circularity
#			in package requires (package a requires package b,
#			which in turns requires package a).
# -verbose		(optional) Verbose output; the name of each file that
#			was successfully rocessed is printed out. Additionally,
#			if processing of a file failed a message is printed
#			out; a file failure may not indicate that the indexing
#			has failed, since pkg_mkIndex stores the list of failed
#			files and tries again. The second time the processing
#			may succeed, for example if a required package has been
#			indexed by a previous pass.
# dir -			Name of the directory in which to create the index.
# args -		Any number of additional arguments, each giving
#			a glob pattern that matches the names of one or
#			more shared libraries or Tcl script files in
#			dir.

proc pkg_mkIndex {args} {
    global errorCode errorInfo
    set usage {"pkg_mkIndex ?-nopkgrequire? ?-direct? ?-verbose? dir ?pattern ...?"};

    set argCount [llength $args]
    if {$argCount < 1} {
	return -code error "wrong # args: should be\n$usage"
    }

    set more ""
    set direct 0
    set noPkgRequire 0
    set doVerbose 0
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

	    -nopkgrequire {
		set noPkgRequire 1
		append more " -nopkgrequire"
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
    set patternList [lrange $args [expr $idx + 1] end]
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

    # In order to support building of index files from scratch, we make
    # repeated passes on the files to index, until either all have been
    # indexed, or we can no longer make any headway.

    foreach file [eval glob $patternList] {
	set toProcess($file) 1
    }

    while {[array size toProcess] > 0} {
	set processed 0

	foreach file [array names toProcess] {
	    # For each file, figure out what commands and packages it provides.
	    # To do this, create a child interpreter, load the file into the
	    # interpreter, and get a list of the new commands and packages
	    # that are defined. The interpeter uses a special version of
	    # tclPkgSetup to force loading of required packages at require
	    # time rather than lazily, so that we can keep track of commands
	    # and packages that are defined indirectly rather than from the
	    # file itself.

	    set c [interp create]

	    # Load into the child all packages currently loaded in the parent
	    # interpreter, in case the extension depends on some of them.

	    foreach pkg [info loaded] {
		if {[lindex $pkg 1] == "Tk"} {
		    $c eval {set argv {-geometry +0+0}}
		}
		load [lindex $pkg 0] [lindex $pkg 1] $c
	    }

	    # We also call package ifneeded for all packages that have been
	    # identified so far. This way, each pass will have loaded the
	    # equivalent of the pkgIndex.tcl file that we are constructing,
	    # and packages whose processing failed in previous passes may
	    # be processed successfully now

	    foreach pkg [array names files] {
		$c eval "package ifneeded $pkg\
			\[list tclPkgSetup $dir \
			[lrange $pkg 0 0] [lrange $pkg 1 1]\
			[list $files($pkg)]\]"
	    }
	    if {$noPkgRequire == 1} {
		$c eval {
		    rename package __package_orig
		    proc package {what args} {
			switch -- $what {
			    require { return ; # ignore transitive requires }
			    default { eval __package_orig {$what} $args }
			}
		    }
		    proc __dummy args {}
		    package unknown __dummy
		}
	    } else {
		$c eval {
		    rename package __package_orig
		    proc package {what args} {
			switch -- $what {
			    require {
				eval __package_orig require $args

				# a package that was required needs to be
				# placed in the list of packages to ignore.
				# tclPkgSetup is unable to do it, so do it
				# here.

				set ::__ignorePkgs([lindex $args 0]) 1
			    }

			    provide {
				# if package provide is called at level 1 and
				# with two arguments, then this package is
				# being provided by one of the files we are
				# indexing, and therefore we need to add it
				# to the list of packages to write out.
				# We need to do this check because otherwise
				# packages that are spread over multiple
				# files are indexed only by their first file
				# loaded.
				# Note that packages that this cannot catch
				# packages that are implemented by a
				# combination of TCL files and DLLs

				if {([info level] == 1) \
					&& ([llength $args] == 2)} {
				    lappend ::__providedPkgs [lindex $args 0]
				}

				eval __package_orig provide $args
			    }

			    default { eval __package_orig {$what} $args }
			}
		    }
		}
	    }

	    $c eval [list set __file $file]
	    $c eval [list set __direct $direct]
	    if {[catch {
		$c eval {
		    set __doingWhat "loading or sourcing"

		    # override the tclPkgSetup procedure (which is called by
		    # package ifneeded statements from pkgIndex.tcl) to force
		    # loads of packages, and also keep track of
		    # packages/namespaces/commands that the load generated

		    proc tclPkgSetup {dir pkg version files} {
			# remember the current set of packages and commands,
			# so that we can add any that were defined by the
			# package files to the list of packages and commands
			# to ignore

			foreach __p [package names] {
			    set __localIgnorePkgs($__p) 1
			}
			foreach __ns [__pkgGetAllNamespaces] {
			    set __localIgnoreNs($__ns) 1

			    # if the namespace is already in the __ignoreNs
			    # array, its commands have already been imported

			    if {[info exists ::__ignoreNs($__ns)] == 0} {
				namespace import ${__ns}::*
			    }
			}
			foreach __cmd [info commands] {
			    set __localIgnoreCmds($__cmd) 1
			}
			
			# load the files that make up the package

			package provide $pkg $version
			foreach __fileInfo $files {
			    set __f [lindex $__fileInfo 0]
			    set __type [lindex $__fileInfo 1]
			    if {$__type == "load"} {
				load [file join $dir $__f] $pkg
			    } else {
				source [file join $dir $__f]
			    }
			}

			# packages and commands that were defined by these
			# files are to be ignored.

			foreach __p [package names] {
			    if {[info exists __localIgnorePkgs($__p)] == 0} {
				set ::__ignorePkgs($__p) 1
			    }
			}
			foreach __ns [__pkgGetAllNamespaces] {
			    if {([info exists __localIgnoreNs($__ns)] == 0) \
				&& ([info exists ::__ignoreNs($__ns)] == 0)} {
				namespace import ${__ns}::*
				set ::__ignoreNs($__ns) 1
			    }
			}
			foreach __cmd [info commands] {
			    if {[info exists __localIgnoreCmds($__cmd)] == 0} {
				lappend ::__ignoreCmds $__cmd
			    }
			}
		    }

		    # we need to track command defined by each package even in
		    # the -direct case, because they are needed internally by
		    # the "partial pkgIndex.tcl" step above.

		    proc __pkgGetAllNamespaces {{root {}}} {
			set __list $root
			foreach __ns [namespace children $root] {
			    eval lappend __list [__pkgGetAllNamespaces $__ns]
			}
			return $__list
		    }

		    # initialize the list of packages to ignore; these are
		    # packages that are present before the script/dll is loaded

		    set ::__ignorePkgs(Tcl) 1
		    set ::__ignorePkgs(Tk) 1
		    foreach __pkg [package names] {
			set ::__ignorePkgs($__pkg) 1
		    }

		    # before marking the original commands, import all the
		    # namespaces that may have been loaded from the parent;
		    # these namespaces and their commands are to be ignored

		    foreach __ns [__pkgGetAllNamespaces] {
			set ::__ignoreNs($__ns) 1
			namespace import ${__ns}::*
		    }

		    set ::__ignoreCmds [info commands]

		    set dir ""		;# in case file is pkgIndex.tcl

		    # Try to load the file if it has the shared library
		    # extension, otherwise source it.  It's important not to
		    # try to load files that aren't shared libraries, because
		    # on some systems (like SunOS) the loader will abort the
		    # whole application when it gets an error.

		    set __pkgs {}
		    set __providedPkgs {}
		    if {[string compare [file extension $__file] \
			    [info sharedlibextension]] == 0} {

			# The "file join ." command below is necessary.
			# Without it, if the file name has no \'s and we're
			# on UNIX, the load command will invoke the
			# LD_LIBRARY_PATH search mechanism, which could cause
			# the wrong file to be used.

			set __doingWhat loading
			load [file join . $__file]
			set __type load
		    } else {
			set __doingWhat sourcing
			source $__file
			set __type source
		    }

		    # Using __ variable names to avoid potential namespaces
		    # clash, even here in post processing because the
		    # loaded package could have set up traces,...

		    foreach __ns [__pkgGetAllNamespaces] {
			if {[info exists ::__ignoreNs($__ns)] == 0} {
			    namespace import ${__ns}::*
			}
		    }
		    foreach __i [info commands] {
			set __cmds($__i) 1
		    }
		    foreach __i $::__ignoreCmds {
			catch {unset __cmds($__i)}
		    }
		    foreach __i [array names __cmds] {
			# reverse engineer which namespace a command comes from
			
			set __absolute [namespace origin $__i]

			# special case so that global names have no leading
			# ::, this is required by the unknown command

			set __absolute [auto_qualify $__absolute ::]

			if {[string compare $__i $__absolute] != 0} {
			    set __cmds($__absolute) 1
			    unset __cmds($__i)
			}
		    }

		    foreach __i $::__providedPkgs {
			lappend __pkgs [list $__i [package provide $__i]]
			set __ignorePkgs($__i) 1
		    }
		    foreach __i [package names] {
			if {([string compare [package provide $__i] ""] != 0) \
				&& ([info exists ::__ignorePkgs($__i)] == 0)} {
			    lappend __pkgs [list $__i [package provide $__i]]
			}
		    }
		}
	    } msg] == 1} {
		set what [$c eval set __doingWhat]
		if {$doVerbose} {
		    tclLog "warning: error while $what $file: $msg\nthis file will be retried in the next pass"
		}
	    } else {
		set type [$c eval set __type]
		set cmds [lsort [$c eval array names __cmds]]
		set pkgs [$c eval set __pkgs]
		if {[llength $pkgs] > 1} {
		    tclLog "warning: \"$file\" provides more than one package ($pkgs)"
		}
		foreach pkg $pkgs {
		    # cmds is empty/not used in the direct case
		    lappend files($pkg) [list $file $type $cmds]
		}

		incr processed
		unset toProcess($file)

		if {$doVerbose} {
		    tclLog "processed $file"
		}
	    }
	    interp delete $c
	}

	if {$processed == 0} {
	    tclLog "this iteration could not process any files: giving up here"
	    break
	}
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
