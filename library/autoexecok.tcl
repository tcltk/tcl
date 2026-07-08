# autoexecok.tcl --
#
# Defines
# "auto_execok" procedure and auto-load facilities.
#
# Copyright © 1991-1993 The Regents of the University of California.
# Copyright © 1994-1996 Sun Microsystems, Inc.
# Copyright © 1998-1999 Scriptics Corporation.
# Copyright © 2004 Kevin B. Kenny.
# Copyright © 2018 Sean Woods
#
# All rights reserved.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

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

if {$tcl_platform(platform) eq "windows"} {
# Windows version.
namespace eval tcl::win {}

# Looks up the registry for the template to open a file
proc tcl::win::LookupFileAssociation {path} {
    set ext [file extension $path]
    if {$ext eq ""} {
	return ""
    }
    try {
	set ftype [tcl::registry get "HKEY_CLASSES_ROOT\\$ext" ""]
	if {[catch {tcl::registry get "HKEY_CLASSES_ROOT\\${ftype}\\shell\\open\\command" ""} command]} {
	    set command [tcl::registry get "HKEY_CLASSES_ROOT\\${ftype}\\shell\\run\\command" ""]
	}
	return $command
    } on error {} {
	return ""
    }
}

# Parses a file association template as stored in the registry and returns
# it in list form with any environment variable references of the form
# %ENVVAR% replaced by their values. Parsing is necessarily heuristic as
# (a) the format is not well defined and (b) in practice even basic rules
# such as quoting arguments with spaces are not followed. The target argument
# is the auto_execok argument that was to be resolved.
proc tcl::win::ParseFileAssociationTemplate {template target} {
    set template [string trim $template]
    # First extract the executable portion. This may or may not be quoted
    # even if it contains spaces so we use the following heuristic. If the
    # template begins with a quote, the executable ends at the following
    # quote. Otherwise, look for .exe that marks the end of the executable
    # (there may be intervening spaces!). This routine does not worry about
    # quotes within quotes.
    if {[string index $template 0] eq "\""} {
	# Quoted exe path
	set exeend [string first "\"" $template 1]
	set exe [string range $template 1 $exeend-1]
	incr exeend 2
    } else {
	# Not quoted, look for .exe marker
	set exeend [string first ".exe" [string tolower $template]]
	incr exeend 4
	set exe [string range $template 0 $exeend-1]
	incr exeend
    }
    if {$exe eq ""} {
	return ""; # Heuristic did not work.
    }

    set result [list $exe]

    # Now parse rest of the arguments. Not necessarily accurate because there
    # is no definition of what that means. Currently, simply look for matching
    # quotes. Note end of quotes does not mean end of argument unless there is
    # an immediate following whitespace.
    set arg ""
    set inQuotes 0; # 0 -> outside quotes
    foreach ch [split [string range $template $exeend end] ""] {
	if {$inQuotes} {
	    if {$ch eq "\""} {
		set inQuotes 0
		# Do NOT terminate prior arg, "foob "ar is single arg {foob ar}
	    } else {
		append arg $ch
	    }
	} else {
	    if {[string is space $ch]} {
		if {$arg ne ""} {
		    lappend result $arg
		    set arg ""
		}
	    } elseif {$ch eq "\""} {
		set inQuotes 1
		# Do NOT terminate prior arg, foo"b ar" is single arg {foob ar}
	    } else {
		append arg $ch
	    }
	}
    }
    # Last arg or unterminated quoted empty string
    if {$arg ne "" || $inQuotes} {
	lappend result $arg
    }

    set result [lmap arg $result {
	if {$arg eq "%1"} {
	    file nativename $target
	} else {
	    regsub -all -nocase -command {%([^%]+)%} $arg {
		apply {{match envvar} {
		    if {[info exists ::env($envvar)]} {
			return $::env($envvar)
		    } else {
			return $match; # original string including %
		    }
		}}
	    }
	}
    }]

    # One final check. The %N and %* placeholders can only be followed
    # by similar placeholders which stand for arguments. Any other values
    # are rejected as the auto_execok mechanism only allows for trailing
    # arguments.
    set placeHolderSeen 0
    set result [lmap arg $result {
	if {[string match {%[0-9*]*} $arg]} {
	    set placeHolderSeen 1
	    # Don't pass this up as a real argument
	    continue
	} elseif $placeHolderSeen {
	    # Saw an earlier placeholder. Cannot deal with non-trailing
	    # arguments. Indicate failure.
	    return [list ]
	} else {
	    set arg
	}
    }]

    return $result
}


#
# Note that file executable doesn't work under Windows, so we have to
# look for files with .exe, .com, or .bat extensions.  Also, the path
# may be in the Path or PATH environment variables, and path
# components are separated with semicolons, not colons as under Unix.
#
proc auto_execok arg {
    global env tcl_platform
    const name $arg

    # See TIP 753 for search algorithm

    # Not all cmd.exe built-ins are included here. In particular, those intended
    # for batch files and those considered obsolete (e.g. VERIFY) are excluded.
    set shellBuiltins [list assoc call cd cls color copy date del dir echo \
			   erase exit ftype for if md mkdir mklink move path \
			   pause prompt rd ren rename rmdir set start time \
			   title type ver vol]
    # Add an initial {} extension check first but only if
    # an extension is present in $name
    if {[info exists env(PATHEXT)]} {
	set execExtensions [split "$env(PATHEXT)" ";"]
    } else {
	set execExtensions [list .com .exe .bat .cmd]
    }
    if {[file extension $name] ne ""} {
	ledit execExtensions 0 -1 {}
    }
    if {[llength [file split $name]] != 1} {
	foreach ext $execExtensions {
	    set file ${name}[string tolower $ext]
	    if {[file exists $file] && ![file isdirectory $file]} {
		return [tcl::win::ParseFileAssociationTemplate \
			    [tcl::win::LookupFileAssociation $file] \
			    $file]
	    }
	}
	return ""
    }

    # At this point $name is a simple name, no directory components

    if {[info exists env(SystemRoot)]} {
	set windir $env(SystemRoot)
    } elseif {[info exists env(WINDIR)]} {
	set windir $env(WINDIR)
    }
    if {[info exists windir]} {
	set system32dir [file join $windir system32]
    }

    if {[string tolower $name] in $shellBuiltins} {
	# Always use cmd.exe, not env(COMSPEC) as the latter may or may not
	# interpret commands the same way.

	if {![info exists system32dir]} {
	    error "Could not locate cmd.exe."
	}
	return [list [file nativename [file join $system32dir cmd.exe]] /c $name]
    }

    lappend searchDirs [list [file dirname [info nameofexecutable]]]

    # Only include cwd based on Windows settings - see TIP 753
    if {![info exists env(NoDefaultCurrentDirectoryInExePath)]} {
	lappend searchDirs "."
    }
    if {[info exists windir]} {
	lappend searchDirs $system32dir $windir
    }
    # Do not use [info exists env(PATH)] due to Bug b492c33154. Instead just
    # catch error.
    catch {lappend searchDirs {*}[split $env(PATH) ";"]}

    foreach dir $searchDirs {
	if {[info exists checked($dir)] || ($dir eq "")} {
	    continue
	}
	set checked($dir) {}
	foreach ext $execExtensions {
	    # Skip already checked directories
	    set file [file join $dir ${name}[string tolower $ext]]
	    if {[file exists $file] && ![file isdirectory $file]} {
		return [tcl::win::ParseFileAssociationTemplate \
			    [tcl::win::LookupFileAssociation $file] \
			    $file]
	    }
	}
    }

    # Look up App Paths. If no extension or extension is not .exe, append
    # .exe a la ShellExecuteEx.
    set key "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\$name"
    set ext [file extension $name]
    if {$ext ne ".exe"} {
	# foo. -> foo.exe, foo.bar -> foo.bar.exe
	append key [expr {$ext eq "." ? "exe" : ".exe"}]
    }
    foreach hive {HKEY_CURRENT_USER HKEY_LOCAL_MACHINE} {
	if {![catch {tcl::registry get "$hive\\$key" ""} exePath]} {
	    return [list $exePath]
	}
    }
    return ""
}

} else {
# Unix version.
#
proc auto_execok name {
    global env

    if {[llength [file split $name]] != 1} {
	if {[file executable $name] && ![file isdirectory $name]} {
	    return [list $name]
	}
	return ""
    }
    foreach dir [split $env(PATH) :] {
	if {$dir eq ""} {
	    set dir .
	}
	set file [file join $dir $name]
	if {[file executable $file] && ![file isdirectory $file]} {
	    return [list $file]
	}
    }
    return ""
}

}

