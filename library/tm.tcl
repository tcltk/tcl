# -*- tcl -*-
#
# Searching for Tcl Modules. Defines a procedure, declares it as the
# primary command for finding packages, however also uses the former
# 'package unknown' command as a fallback.
#
# Locates all possible packages in a directory via a less restricted
# glob. The targeted directory is derived from the name of the
# requested package. I.e. the TM scan will look only at directories
# which can contain the requested package. It will register all
# packages it found in the directory so that future requests have a
# higher chance of being fulfilled by the ifneeded database without
# having to come to us again.
#
# We do not remember where we have been and simply rescan targeted
# directories when invoked again. The reasoning is this:
#
# - The only way we get back to the same directory is if someone is
#   trying to [package require] something that wasn't there on the
#   first scan.
#
#   Either
#   1) It is there now:  If we rescan, you get it; if not you don't.
#
#      This covers the possibility that the application asked for a
#      package late, and the package was actually added to the
#      installation after the application was started. It shoukld
#      still be able to find it.
#
#   2) It still is not there: Either way, you don't get it, but the
#      rescan takes time. This is however an error case and we dont't
#      care that much about it
#
#   3) It was there the first time; but for some reason a "package
#      forget" has been run, and "package" doesn't know about it
#      anymore.
#
#      This can be an indication that the application wishes to reload
#      some functionality. And should work as well.
#
# Note that this also strikes a balance between doing a glob targeting
# a single package, and thus most likely requiring multiple globs of
# the same directory when the application is asking for many packages,
# and trying to glob for _everything_ in all subdirectories when
# looking for a package, which comes with a heavy startup cost.
#
# We scan for regular packages only if no satisfying module was found.

namespace eval ::tcl::tm {
    # Default paths. None yet.

    variable paths {}

    # The regex pattern a file name has to match to make it a Tcl Module.

    set pkgpattern {^([[:alpha:]][:[:alnum:]]*)-([[:digit:]].*)[.]tm$}

    # Export the public API

    namespace export path
}

# ::tcl::tm::path --
#
#	Public API to the module path. See specification.
#
# Arguments
#	cmd -	The subcommand to execute
#	args -	The paths to add/remove. Must not appear querying the
#		path with 'list'.
#
# Results
#	No result for subcommands 'add' and 'remove'. A list of paths
#	for 'list'.
#
# Sideeffects
#	The subcommands 'add' and 'remove' manipulate the list of
#	paths to search for Tcl Modules. The subcommand 'list' has no
#	sideeffects.

proc ::tcl::tm::path {cmd args} {
    variable paths
    switch -exact -- $cmd {
	add {
	    # The path is added at the head to the list of module
	    # paths.
	    #
	    # The command enforces the restriction that no path may be
	    # an ancestor directory of any other path on the list. If
	    # the new path violates this restriction an error wil be
	    # raised.
	    #
	    # If the path is already present as is no error will be
	    # raised and no action will be taken.

	    if {![llength $args]} {
		return -code error "wrong#args, expected: [lindex [info level 0] 0] add path path..."
	    }

	    set newpaths {}
	    foreach p $args {
		set pos [lsearch -exact $paths $p]
		if {$pos >= 0} {
		    # Ignore a path already on the list.
		    continue
		}

		# Search for paths which are subdirectories of the new
		# one. If there are any then new path violates the
		# restriction about ancestors.

		set pos [lsearch -glob $paths ${p}/*]
		if {$pos >= 0} {
		    return -code error "$p is ancestor of existing module path [lindex $paths $pos]."
		}

		# Now look for paths which are ancestors of the new
		# one. This reverse question req us to loop over the
		# existing paths :(

		foreach ep $paths {
		    if {[string match ${ep}/* $p]} {
			return -code error "$p is subdirectory of existing module path $ep."
		    }
		}

		lappend newpaths $p
	    }

	    # The validation of the input is complete and successful,
	    # and everything in newpaths is actually new. We can now
	    # extend the list of paths.

	    foreach p $newpaths {
		set paths [linsert $paths 0 $p]
	    }
	}
	remove {
	    # Removes the path from the list of module paths. The
	    # command is silently ignored if the path is not on the
	    # list.

	    if {![llength $args]} {
		return -code error "wrong#args, expected: [lindex [info level 0] 0] remove path path ..."
	    }

	    foreach p $args {
		set pos [lsearch -exact $paths $p]
		if {$pos >= 0} {
		    set paths [lreplace $paths $pos $pos]
		}
	    }
	}
	list {
	    if {[llength $args]} {
		return -code error "wrong#args, expected: [lindex [info level 0] 0] list"
	    }
	    return $paths
	}
	default {
	    return -code error "Expect one of add, remove, or list, got \"$cmd\""
	}
    }
}

# ::tcl::tm::unknown --
#
#	Unknown handler for Tcl Modules, i.e. packages in module form.
#
# Arguments
#	original	- Original [package unknown] procedure.
#	name		- Name of desired package.
#	version		- Version of desired package. Can be the
#			  empty string.
#	exact		- Either -exact or ommitted.
#
#	Name, version, and exact are used to determine
#	satisfaction. The original is called iff no satisfaction was
#	achieved. The name is also used to compute the directory to
#	target in the search.
#
# Results
#	None.
#
# Sideeffects
#	May populate the package ifneeded database with additional
#	provide scripts.

proc ::tcl::tm::unknown {original name version {exact {}}} {
    # Import the list of paths to search for packages in module form.
    # Import the pattern used to check package names in detail.  

    variable paths
    variable pkgpattern

    # Without paths to search we can do nothing. (Except falling back
    # to the regular search).

    if {[llength $paths]} {
	set pkgpath [string map {:: /} $name]
	set pkgroot [file dirname $pkgpath]
	if {$pkgroot eq "."} {set pkgroot ""}

	# We don't remember a copy of the paths while looping. Tcl
	# Modules are unable to change the list while we are searching
	# for them. This also simplifies the loop, as we cannot get
	# additional directories while iterating over the list. A
	# simple foreach is sufficient.

	set satisfied 0
	foreach path $paths {
	    if {![file exists $path]} continue
	    set currentsearchpath [file join $path $pkgroot]
	    if {![file exists $currentsearchpath]} continue
	    set strip             [llength [file split $path]]

	    # We can't use glob in safe interps, so enclose the following
	    # in a catch statement, where we get the module files out
	    # of the subdirectories. In other words, Tcl Modules are
	    # not-functional in such an interpreter. This is the same
	    # as for the command "tclPkgUnknown", i.e. the search for
	    # regular packages.

	    catch {
		# We always look for _all_ possible modules in the current
		# path, to get the max result out of the glob.

		foreach file [glob -nocomplain -directory $currentsearchpath *.tm] {
		    set pkgfilename [join [lrange [file split $file] $strip end] ::]

		    if {![regexp -- $pkgpattern $pkgfilename --> pkgname pkgversion]} {
			# Ignore everything not matching our pattern
			# for package names.
			continue
		    }
		    if {[catch {package vcompare $pkgversion 0}]} {
			# Ignore everything where the version part is
			# not acceptable to "package vcompare".
			continue
		    }

		    # We have found a candidate, generate a "provide
		    # script" for it, and remember it.

		    package ifneeded $pkgname $pkgversion [list source $file]

		    # We abort in this unknown handler only if we got
		    # a satisfying candidate for the requested
		    # package. Otherwise we still have to fallback to
		    # the regular package search to complete the
		    # processing.

		    if {
			($pkgname eq $name) &&
			((($exact eq "-exact") && (0==[package vcompare $pkgversion $version])) ||
			(($version ne "") && [package vsatisfies $pkgversion $version]) ||
			($version eq ""))
		    } {
			set satisfied 1
			# We do not abort the loop, and keep adding
			# provide scripts for every candidate in the
			# directory, just remember to not fall back to
			# the regular search anymore.
		    }
		}
	    }
	}

	if {$satisfied} {
	    return
	}
    }

    # Fallback to previous command, if existing.
    if {[llength $original]} {
	uplevel 1 $original [list $name $version $exact]
    }
}

# ::tcl::tm::Defaults --
#
#	Determines the default search paths.
#
# Arguments
#	None
#
# Results
#	None.
#
# Sideeffects
#	May add paths to the list of defaults.

proc ::tcl::tm::Defaults {} {
    foreach {major minor} [split [info tclversion] .] break

    roots [list \
	    [file dirname [info library]] \
	    [file join [file dirname [file normalize [info nameofexecutable]]] lib] \
	    ]

    if {$::tcl_platform(platform) eq "windows"} {
	set sep \;
    } else {
	set sep :
    }
    for {set n $minor} {$n >= 0} {incr n -1} {
	set ev TCL${major}.{$n}_TM_PATH
	if {[info exists ::env($ev)]} {
	    foreach p [split $::env($ev) $sep] {
		path add $p
	    }
	}
    }
    return
}

# ::tcl::tm::roots --
#
#	Public API to the module path. See specification.
#
# Arguments
#	paths -	List of 'root' paths to derive search paths from.
#
# Results
#	No result.
#
# Sideeffects
#	Calls 'path add' to paths to the list of module search paths.

proc ::tcl::tm::roots {paths} {
    foreach {major minor} [split [info tclversion] .] break
    foreach pa $paths {
	set p [file join $pa tcl$major]
	for {set n $minor} {$n >= 0} {incr n -1} {
	    path add [file normalize [file join $p ${major}.${n}]]
	}
	path add [file normalize [file join $pa site-tcl]]
    }
    return
}

# Initialization. Set up the default paths, then insert the new
# handler into the chain.

::tcl::tm::Defaults
package unknown [list ::tcl::tm::unknown [package unknown]]
