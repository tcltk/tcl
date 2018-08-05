# makeHeader.tcl --
#
#	This script generates embeddable C source (in a .h file) from a .tcl
#	script.
#
# Copyright (c) 2018 Donal K. Fellows
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require Tcl 8.6

namespace eval makeHeader {

    ####################################################################
    #
    # mapSpecial --
    #	Transform a single line so that it is able to be put in a C string.
    #
    proc mapSpecial {str} {
	# All Tcl metacharacters and key C backslash sequences
	set MAP {
	    \" \\\\\" \\ \\\\\\\\ $ \\$ [ \\[ ] \\] ' \\\\' ? \\\\?
	    \a \\\\a \b \\\\b \f \\\\f \n \\\\n \r \\\\r \t \\\\t \v \\\\v
	}
	set XFORM {[format \\\\\\\\u%04x {*}[scan & %c]]}

	subst [regsub -all {[^\u0020-\u007e]} [string map $MAP $str] $XFORM]
    }

    ####################################################################
    #
    # processScript --
    #	Transform a whole sequence of lines with [mapSpecial].
    #
    proc processScript {scriptLines} {
	lmap line $scriptLines {
	    format {"%s"} [mapSpecial $line\n]
	}
    }

    ####################################################################
    #
    # updateTemplate --
    #	Rewrite a template to contain the content from the input script.
    #
    proc updateTemplate {dataVar scriptLines} {
	set BEGIN "*!BEGIN!: Do not edit below this line.*"
	set END "*!END!: Do not edit above this line.*"

	upvar 1 $dataVar data

	set from [lsearch -glob $data $BEGIN]
	set to [lsearch -glob $data $END]
	if {$from == -1 || $to == -1 || $from >= $to} {
	    throw BAD "not a template"
	}

	set data [lreplace $data $from+1 $to-1 {*}[processScript $scriptLines]]
    }

    ####################################################################
    #
    # stripSurround --
    #	Removes the header and footer comments from a (line-split list of
    #	lines of) Tcl script code.
    #
    proc stripSurround {lines} {
	set RE {^\s*$|^#}
	set state 0
	set lines [lmap line [lreverse $lines] {
	    if {!$state && [regexp $RE $line]} continue {
		set state 1
		set line
	    }
	}]
	return [lmap line [lreverse $lines] {
	    if {$state && [regexp $RE $line]} continue {
		set state 0
		set line
	    }
	}]
    }

    ####################################################################
    #
    # updateTemplateFile --
    #	Rewrites a template file with the lines of the given script.
    #
    proc updateTemplateFile {headerFile scriptLines} {
	set f [open $headerFile "r+"]
	try {
	    set content [split [chan read -nonewline $f] "\n"]
	    updateTemplate content [stripSurround $scriptLines]
	    chan seek $f 0
	    chan puts $f [join $content \n]
	    chan truncate $f
	} trap BAD msg {
	    # Add the filename to the message
	    throw BAD "${headerFile}: $msg"
	} finally {
	    chan close $f
	}
    }

    ####################################################################
    #
    # readScript --
    #	Read a script from a file and return its lines.
    #
    proc readScript {script} {
	set f [open $script]
	try {
	    chan configure $f -encoding utf-8
	    return [split [string trim [chan read $f]] "\n"]
	} finally {
	    chan close $f
	}
    }

    ####################################################################
    #
    # run --
    #	The main program of this script.
    #
    proc run {args} {
	try {
	    if {[llength $args] != 2} {
		throw ARGS "inputTclScript templateFile"
	    }
	    lassign $args inputTclScript templateFile

	    puts "Inserting $inputTclScript into $templateFile"
	    set scriptLines [readScript $inputTclScript]
	    updateTemplateFile $templateFile $scriptLines
	    exit 0
	} trap ARGS msg {
	    puts stderr "wrong # args: should be \"[file tail $::argv0] $msg\""
	    exit 2
	} trap BAD msg {
	    puts stderr $msg
	    exit 1
	} trap POSIX msg {
	    puts stderr $msg
	    exit 1
	} on error {- opts} {
	    puts stderr [dict get $opts -errorinfo]
	    exit 3
	}
    }
}

########################################################################
#
# Launch the main program
#
if {[info script] eq $::argv0} {
    makeHeader::run {*}$::argv
}

# Local-Variables:
# mode: tcl
# fill-column: 78
# End:
