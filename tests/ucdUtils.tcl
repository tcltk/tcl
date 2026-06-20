# Utilities to read Unicode Character Data files
# Copyright © 2025-2026 Ashok P. Nadkarni
#
# See the file license.terms for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

if {[namespace exists tcltest::ucd]} {
    return
}
package require tcltest

namespace eval tcltests::ucd {
    # UCD file paths
    variable normalizationDataFile \
	[file join [file dirname [info script]] unicodeTestVectors NormalizationTest.txt]
    variable caseFoldDataFile \
	[file join [file dirname [info script]] unicodeTestVectors DerivedNormalizationProps.txt]
    variable derivedCorePropertiesFile \
	[file join [file dirname [info script]] unicodeTestVectors DerivedCoreProperties.txt]
    variable graphemeBreaksDataFile \
	[file join [file dirname [info script]] unicodeTestVectors GraphemeBreakTest.txt]

    # Highest assigned Unicode code point
    variable maxCodepoint 0x10ffff

    tcltest::testConstraint ucdnormalization [file exists $normalizationDataFile]
    tcltest::testConstraint ucdproperties [file exists $derivedCorePropertiesFile]
    tcltest::testConstraint ucdgraphemes [file exists $graphemeBreaksDataFile]

    # Don't enable casefolding tests - not implemented or TIP'ed in core
    # tcltest::testConstraint ucdcasefolding [file exists $caseFoldDataFile]

    proc hexListToChars {s} {
	# 0044 030c -> \u0044\u030c
	subst -novariables -nocommands \\U[join $s \\U]
    }

    proc readNormalizationData {} {
	variable normalizationData {}
	variable normalizationDataFile
	variable singleFormChars {}

	set fd [open $normalizationDataFile]
	fconfigure $fd -encoding utf-8
	set lineno 0; # Start numbering as line number 1
	set normalizationData {}
	set inPart1 0
	set prevPart1Char -1
	# See comments at top of normalizationDataFile for format
	while {[gets $fd line] >= 0} {
	    incr lineno
	    set line [string trim $line]
	    if {[string index $line 0] in {{} #}} {
		continue
	    }
	    if {[regexp -nocase {^@part(\d+)} $line -> partNum]} {
		set inPart1 [expr {$partNum == 1}]
		continue
	    }
	    # Form list of {lineno chars nfcform nfdform nfkfcform nfkdform}
	    set fields [lrange [split $line \;] 0 4]
	    lappend normalizationData \
		[list $lineno {*}[lmap v $fields {hexListToChars $v}]]
	    # Characters that are not listed in part1 need to be tested that
	    # they map to themselves.
	    if {$inPart1} {
		set thisChar [scan [lindex $fields 0] %x]
		# We assume the file is in order!
		if {$thisChar <= $prevPart1Char} {
		    puts stderr "Warning: Part 1 lines in $normalizationDataFile do not seem sorted by codepoint!"
		}
		while {[incr prevPart1Char] != $thisChar} {
		    if {$prevPart1Char >= 0xD800 && $prevPart1Char <= 0xDFFF} {
			continue; # Surrogates
		    }
		    lappend singleFormChars [hexListToChars [format %x $prevPart1Char]]
		}
	    }
	}
	while {[incr prevPart1Char] <= 0x10ffff} {
	    lappend singleFormChars [hexListToChars [format %x $prevPart1Char]]
	}

	proc readNormalizationData {} {}; # Only read once
    }

    proc getNormalizationData {} {
	variable normalizationData
	readNormalizationData
	return $normalizationData
    }

    proc getSingleFormChars {} {
	variable singleFormChars
	readNormalizationData
	return $singleFormChars
    }

    proc readCaseFoldData {} {
	variable caseFoldData {}
	variable caseFoldDataFile
	variable caseFoldIdentities

	set fd [open $caseFoldDataFile]
	fconfigure $fd -encoding utf-8
	set lineno 0; # Start numbering as line number 1
	set caseFoldData {}
	set prevCodePoint -1
	# See comments at top of normalizationDataFile for format
	while {[gets $fd line] >= 0} {
	    incr lineno
	    set line [string trim $line]
	    if {[string index $line 0] in {{} #}} {
		continue
	    }
	    # Line is of the form
	    #   XXXX[..YYYY]  ; NFKC_CF; XXXX XXXX # descriptive text
	    lassign [lmap field [split [lindex [split $line #] 0] \;] {
		string trim $field
	    }] rangeOfChars typeOfLine casefoldedchars
	    if {$typeOfLine ne "NFKC_CF"} {
		continue
	    }
	    # Some entries may be ranges of chars xxxx..yyyy
	    set rangeOfChars [regexp -inline -all {[[:xdigit:]]+} $rangeOfChars]; # xxxx..yyyy
	    foreach codePoint [lseq 0x[lindex $rangeOfChars 0] .. 0x[lindex $rangeOfChars end]] {
		set chars [format %c $codePoint]
		# Some chars are ignored in case folds
		if {$casefoldedchars eq ""} {
		    # Mapping column empty. Reserved chars map to themselves.
		    # Otherwise, character is ignored by folding algorithm
		    if {[string match *reserved* $line]} {
			set expected $chars
		    } else {
			set expected {}
		    }
		} else {
		    set expected [hexListToChars $casefoldedchars]
		}
		lappend caseFoldData \
		    [list $lineno $chars $expected]

		# Characters that are not listed need to be tested that
		# they map to themselves.
		# We assume the file is in order!
		if {$codePoint <= $prevCodePoint} {
		    puts stderr "Warning: Part 1 lines in $caseFoldDataFile do not seem sorted by codepoint!"
		    puts stderr "Line $lineno ($codePoint <= $prevCodePoint)"
		    exit
		}
		while {[incr prevCodePoint] < $codePoint} {
		    if {$prevCodePoint >= 0xD800 && $prevCodePoint <= 0xDFFF} {
			continue; # Surrogates
		    }
		    lappend caseFoldIdentities [hexListToChars [format %x $prevCodePoint]]
		}
	    }
	}
	while {[incr prevCodePoint] <= 0x10ffff} {
	    lappend caseFoldIdentities [hexListToChars [format %x $prevCodePoint]]
	}

	proc readCaseFoldData {} {}; # Only read once
    }

    proc getCaseFoldData {} {
	variable caseFoldData
	readCaseFoldData
	return $caseFoldData
    }

    proc getCaseFoldIdentities {} {
	variable caseFoldIdentities
	readCaseFoldData
	return $caseFoldIdentities
    }

    proc readDerivedCoreProperties {} {
	variable derivedCorePropertiesFile
	variable derivedCoreProperties; # Dict indexed by property name

	set fd [open $derivedCorePropertiesFile]
	fconfigure $fd -encoding utf-8
	# See comments at top of $derivedCorePropertiesFile for format
	while {[gets $fd line] >= 0} {
	    set line [string trim $line]
	    if {[string index $line 0] in {{} #}} {
		continue
	    }
	    # Line is of the form
	    #   XXXX[..YYYY]  ; PROPERTYNAME # descriptive text
	    lassign [lmap field [split [lindex [split $line #] 0] \;] {
		string trim $field
	    }] rangeOfChars propertyName
	    if {$propertyName ni {Lowercase Uppercase}} {
		continue
	    }
	    # Some entries may be ranges of chars xxxx..yyyy
	    set rangeOfChars [regexp -inline -all {[[:xdigit:]]+} $rangeOfChars]; # xxxx..yyyy
	    foreach codePoint [lseq 0x[lindex $rangeOfChars 0] .. 0x[lindex $rangeOfChars end]] {
		set char [format %c $codePoint]
		dict set derivedCoreProperties $propertyName $char {}
	    }
	}

	proc readDerivedCoreProperties {} {}; # Only read once
    }

    proc getLowercaseChars {} {
	variable derivedCoreProperties
	readDerivedCoreProperties
	return [dict get $derivedCoreProperties Lowercase]
    }

    proc getUppercaseChars {} {
	variable derivedCoreProperties
	readDerivedCoreProperties
	return [dict get $derivedCoreProperties Uppercase]
    }

    proc readGraphemeBreaks {} {
        variable graphemeBreaksDataFile
        variable graphemeBreaksData

        set fd [open $graphemeBreaksDataFile]
        fconfigure $fd -encoding utf-8
        set lineno 0
        # See comments atop $graphemeBreaksDataFile for format
        while {[gets $fd line] >= 0} {
            incr lineno
            set line [string trim $line]
	    if {[string index $line 0] in {{} #}} {
		continue
	    }
            # Line is of the form
            # (SEP XXXX)+ SEP (# comment)?
            # where SEP is
            #	÷ (U+00F7) wherever there is a break opportunity, and
            #	× (U+00D7) wherever there is not.
            set break \u00f7
            set nobreak \u00d7
            lassign [split $line #] definition comment
            set definition [string trim $definition]
            if {[string index $definition 0] ne $break || [string index $definition end] ne $break} {
                puts stderr "Unexpected format of line $lineno in $graphemeBreaksDataFile $definition"
                continue
            }
            set record [list $lineno $comment]
            set grapheme {}
            set graphemes [list ]
            foreach {hex breakChar} [regexp -all -inline {\S+} [string range $definition 1 end]] {
                append grapheme [format %c 0x$hex]
                if {$breakChar eq $break} {
                    lappend graphemes $grapheme
                    set grapheme ""
                } elseif {$breakChar ne $nobreak} {
                    puts stderr "Unexpected separator character $breakChar"
                    unset record
                    break
                }
            }
            if {[info exists record]} {
                lappend record $graphemes
                lappend graphemeBreaksData $record
            }
        }
    }

    # Returns the test vectors for graphemes as a list of triples comprising
    # the line number in GraphemeBreakTest.txt, the comment and the test case
    # data. This last is a list of strings each of which is a single grapheme.
    proc getGraphemeData {} {
        readGraphemeBreaks
        proc getGraphemeData {} {
            variable graphemeBreaksData
            return $graphemeBreaksData
        }
        tailcall getGraphemeData
    }
}
