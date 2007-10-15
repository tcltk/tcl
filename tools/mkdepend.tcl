#==============================================================================
#
# mkdepend : generate dependency information from C/C++ files
#
# Copyright (c) 1998, Nat Pryce
#
# Permission is hereby granted, without written agreement and without
# license or royalty fees, to use, copy, modify, and distribute this
# software and its documentation for any purpose, provided that the
# above copyright notice and the following two paragraphs appear in
# all copies of this software.
#
# IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, 
# SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF 
# THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE AUTHOR HAS BEEN ADVISED 
# OF THE POSSIBILITY OF SUCH DAMAGE.
#
# THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT 
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
# PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" 
# BASIS, AND THE AUTHOR HAS NO OBLIGATION TO PROVIDE  MAINTENANCE, SUPPORT,
# UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
#==============================================================================
#
# Modified heavily by David Gravereaux <davygrvy@pobox.com> about 9/17/2006.
# Original can be found @ http://www.doc.ic.ac.uk/~np2/software/mkdepend.html
#
#==============================================================================
# RCS: @(#) $Id: mkdepend.tcl,v 1.1.2.2 2007/10/15 18:32:36 dgp Exp $
#==============================================================================

array set mode_data {}
set mode_data(vc32) {cl -nologo -E}

set cpp_args ""
set source_extensions [list .c .cpp .cxx]
set target_extension ".obj"
set target_prefix ""
set remove_prefix ""
set verbose 1

set excludes [list]
if [info exists env(INCLUDE)] {
    set rawExcludes [split [string trim $env(INCLUDE) ";"] ";"]
    foreach exclude $rawExcludes {
	lappend excludes [file normalize $exclude]
    }
}


# openOutput --
#
#	Opens the output file.
#
# Arguments:
#	file	The file to open
#
# Results:
#	None.

proc openOutput {file} {
    global output
    set output [open $file w]
    puts $output "# Automatically generated at [clock format [clock seconds]] by [info script]"
}

# closeOutput --
#
#	Closes output file.
#
# Arguments:
#	none
#
# Results:
#	None.

proc closeOutput {} {
    global output
    if {[string match stdout $output]} {
        close $output
    }
}

# readDepends --
#
#	Read off CCP pipe for #include references.  pipe channel
#	is closed when done.
#
# Arguments:
#	chan	The pipe channel we are reading in.
#
# Results:
#	Raw dependency list pairs.

proc readDepends {chan} {
    global source_extensions target_extension verbose

    array set depends {}
    set line ""

    while {[gets $chan line] != -1} {
        if {[regexp {^#line [0-9]+ \"(.*)\"$} $line tmp fname] != 0} {
            if {[lsearch $source_extensions [file extension $fname]] != -1} {
                set target2 "[file rootname $fname]$target_extension"

                if {![info exists target] ||
                    [string compare $target $target2] != 0} \
                {
                    set target $target2
                    set depends($target|[file normalize $fname]) ""

                    if $verbose {
                        puts stderr "processing [file tail $fname]"
                    }
                }
            } else {
                set depends($target|[file normalize $fname]) ""
            }
        }
    }
    catch {close $chan}

    set result {}
    foreach n [array names depends] {
        set pair [split $n "|"]
        lappend result [list [lindex $pair 0] [lindex $pair 1]]
    }

    return $result
}

# genStubs::interface --
#
#	This function is used in the declarations file to set the name
#	of the interface currently being defined.
#
# Arguments:
#	name	The name of the interface.
#
# Results:
#	None.
proc writeDepends {out depends} {
    foreach pair $depends {
        puts $out "[lindex $pair 0] : \\\n\t[join [lindex $pair 1] " \\\n\t"]"
    }
}

# genStubs::interface --
#
#	This function is used in the declarations file to set the name
#	of the interface currently being defined.
#
# Arguments:
#	name	The name of the interface.
#
# Results:
#	None.
proc stringStartsWith {str prefix} {
    set front [string range $str 0 [expr {[string length $prefix] - 1}]]
    return [expr {[string compare [string tolower $prefix] \
                                  [string tolower $front]] == 0}]
}

# genStubs::interface --
#
#	This function is used in the declarations file to set the name
#	of the interface currently being defined.
#
# Arguments:
#	name	The name of the interface.
#
# Results:
#	None.
proc filterExcludes {depends excludes} {
    set filtered {}

    foreach pair $depends {
        set excluded 0
        set file [lindex $pair 1]

        foreach dir $excludes {
            if [stringStartsWith $file $dir] {
                set excluded 1
                break;
            }
        }

        if {!$excluded} {
            lappend filtered $pair
        }
    }

    return $filtered
}

# genStubs::interface --
#
#	This function is used in the declarations file to set the name
#	of the interface currently being defined.
#
# Arguments:
#	name	The name of the interface.
#
# Results:
#	None.
proc replacePrefix {file} {
    global srcPathList srcPathReplaceList

    foreach was $srcPathList is $srcPathReplaceList {
	regsub $was $file $is file
    }
    return $file
}

# rebaseFiles --
#
#	Replaces normalized paths with original macro names.
#
# Arguments:
#	depends	Dependency pair list.
#
# Results:
#	None.

proc rebaseFiles {depends} {
    set rebased {}
    foreach pair $depends {
        lappend rebased [list \
                [replacePrefix [lindex $pair 0]] \
		[replacePrefix [lindex $pair 1]]]

    }
    return $rebased
}

# compressDeps --
#
#	Compresses same named tragets into one pair with
#	multiple deps.
#
# Arguments:
#	depends	Dependency pair list.
#
# Results:
#	The processed list.

proc compressDeps {depends} {
    array set compressed [list]

    foreach pair $depends {
	lappend compressed([lindex $pair 0]) [lindex $pair 1]
    }

    foreach n [array names compressed] {
        lappend result [list $n $compressed($n)]
    }

    return $result
}

# addSearchPath --
#
#	Adds a new set of path and replacement string to the global list.
#
# Arguments:
#	newPathInfo	comma seperated path and replacement string
#
# Results:
#	None.

proc addSearchPath {newPathInfo} {
    global srcPathList srcPathReplaceList

    set infoList [split $newPathInfo ,]
    lappend srcPathList [file normalize [lindex $infoList 0]]
    lappend srcPathReplaceList [lindex $infoList 1]
}


# displayUsage --
#
#	Displays usage to stderr
#
# Arguments:
#	none.
#
# Results:
#	None.

proc displayUsage {} {
    puts stderr "mkdepend.tcl \[options\] genericDir,macroName compatDir,macroName platformDir,macroName"
}

# readInputListFile --
#
#	Open and read the object file list.
#
# Arguments:
#	objectListFile - name of the file to open.
#
# Results:
#	None.

proc readInputListFile {objectListFile} {
    global srcFileList srcPathList
    set f [open $objectListFile r]

    # this probably isn't bullet-proof.
    set fl [split [read $f]]
    close $f

    foreach fname $fl {
	# compiled .res resource files should be ignored.
	if {[file extension $fname] ne ".obj"} {continue}

	# just filename without path or extension.
	set baseName [file rootname [file tail $fname]]

	foreach path $srcPathList {
	    if {[file exist [file join $path ${baseName}.c]]} {
		lappend srcFileList [file join $path ${baseName}.c]
	    } elseif {[file exist [file join $path ${baseName}.cpp]]} {
		lappend srcFileList [file join $path ${baseName}.cpp]		
	    } else {
		# ignore it
	    }
	}
    }
}

# main --
#
#	The main procedure of this script.
#
# Arguments:
#	none.
#
# Results:
#	None.

proc main {} {
    global argc argv mode mode_data srcFileList srcPathList excludes
    global remove_prefix target_prefix output env

    set srcPathList [list]
    set srcFileList [list]

    if {$argc == 1} {displayUsage}

    # Parse mkdepend input
    for {set i 0} {$i < [llength $argv]} {incr i} {
	switch -glob -- [set arg [lindex $argv $i]] {
	    -vc32 {
		set mode vc32
	    }
	    -bc32 {
		set mode bc32
	    }
	    -wc32 {
		set mode wc32
	    }
	    -lc32 {
		set mode lc32
	    }
	    -mgw32 {
		set mode mgw32
	    }
	    -passthru:* {
		puts stderr [set passthru [string range $arg 10 end]]
	    }
	    -out:* {
		openOutput [string range $arg 5 end]
	    }
	    @* {
		readInputListFile [string range $arg 1 end]
	    }
	    -? -
	    -help -
	    --help {
		displayUsage
		exit 1
	    }
	    default {
		if {![info exist mode]} {
		    puts stderr "mode not set"
		    displayUsage
		}
		addSearchPath $arg
	    }
	}
    }

    # Execute the CPP command and parse output

    foreach srcFile $srcFileList {
	set command "$mode_data($mode) $passthru \"$srcFile\""
	set input [open |$command r]

	set depends [readDepends $input]
	set depends [filterExcludes $depends $excludes]
	set depends [rebaseFiles $depends]
	set depends [compressDeps $depends]
	set depends [lsort -index 0 $depends]
	writeDepends $output $depends
    }

    closeOutput
}

# kick it up.
main
