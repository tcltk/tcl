#----------------------------------------------------------------------
#
# icu.tcl --
#
#	This file implements the portions of the [tcl::unsupported::icu]
#       ensemble that are coded in Tcl.
#
#----------------------------------------------------------------------
#
# Copyright © 2024 Ashok P. Nadkarni
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
#----------------------------------------------------------------------

loadIcu

namespace eval ::tcl::unsupported::icu {
    # Map Tcl encoding names to ICU and back. Note ICU has multiple aliases
    # for the same encoding.
    variable tclToIcu
    variable icuToTcl

    proc Init {} {
        variable tclToIcu
        variable icuToTcl

        ::tcl::unsupported::loadIcu

        # There are some special cases where names do not line up
        # at all. Map Tcl -> ICU
        array set specialCases {
            ebcdic ebcdic-cp-us
            macCentEuro maccentraleurope
            utf16 UTF16_PlatformEndian
            utf-16be UnicodeBig
            utf-16le UnicodeLittle
            utf32 UTF32_PlatformEndian
        }
        # Ignore all errors. Do not want to hold up Tcl
        # if ICU not available
        catch {
            foreach tclName [encoding names] {
                set icuNames [aliases $tclName]
                if {[llength $icuNames] == 0} {
                    # E.g. macGreek -> x-MacGreek
                    set icuNames [aliases x-$tclName]
                    if {[llength $icuNames] == 0} {
                        # Still no joy, check for special cases
                        if {[info exists specialCases($tclName)]} {
                            set icuNames [aliases $specialCases($tclName)]
                        }
                    }
                }
                # If the Tcl name is also an ICU name use it else use
                # the first name which is the canonical ICU name
                set pos [lsearch -exact -nocase $icuNames $tclName]
                if {$pos >= 0} {
                    lappend tclToIcu($tclName) [lindex $icuNames $pos] {*}[lreplace $icuNames $pos $pos]
                } else {
                    set tclToIcu($tclName) $icuNames
                }
                foreach icuName $icuNames {
                    lappend icuToTcl($icuName) $tclName
                }
            }
        }
        array default set tclToIcu ""
        array default set icuToTcl ""

        # Redefine ourselves to no-op. Note "args" argument as it
        # seems required for byte code compiler to optimize the
        # to a single noop
        proc Init args {}
    }
    # Primarily used during development
    proc MappedIcuNames {{pat *}} {
        Init
        variable icuToTcl
        return [array names icuToTcl $pat]
    }
    # Primarily used during development
    proc UnmappedIcuNames {{pat *}} {
        Init
        variable icuToTcl
        set unmappedNames {}
        foreach icuName [converters] {
            if {[llength [icuToTcl $icuName]] == 0} {
                lappend unmappedNames $icuName
            }
            foreach alias [aliases $icuName] {
                if {[llength [icuToTcl $alias]] == 0} {
                    lappend unmappedNames $alias
                }
            }
        }
        # Aliases can be duplicates. Remove
        return [lsort -unique [lsearch -inline -all $unmappedNames $pat]]
    }
    # Primarily used during development
    proc UnmappedTclNames {{pat *}} {
        Init
        variable tclToIcu
        set unmappedNames {}
        foreach tclName [encoding names] {
            # Note entry will always exist. Check if empty
            if {[llength [tclToIcu $tclName]] == 0} {
                lappend unmappedNames $tclName
            }
        }
        return [lsearch -inline -all $unmappedNames $pat]
    }

    # Returns the Tcl equivalent of an ICU encoding name or
    # the empty string in case not found.
    proc icuToTcl {icuName} {
        Init
        proc icuToTcl {icuName} {
            variable icuToTcl
            return [lindex $icuToTcl($icuName) 0]
        }
        icuToTcl $icuName
    }

    # Returns the ICU equivalent of an Tcl encoding name or
    # the empty string in case not found.
    proc tclToIcu {tclName} {
        Init
        proc tclToIcu {tclName} {
            variable tclToIcu
            return [lindex $tclToIcu($tclName) 0]
        }
        tclToIcu $tclName
    }

    # Prints a log message
    proc log {message} {
        puts stderr $message
    }

    # Return 1 / 0 depending on whether the data can
    # be decoded with a given encoding
    proc checkEncoding {data enc} {
        encoding convertfrom -failindex x $enc $data
        return [expr {$x < 0}]
    }

    # Detect the encoding for a file. If sampleLength is the
    # empty string, entire file is read.
    # NOTE: sampleLength other than "" has the problem that
    # the encoding may be perfectly valid but the data at
    # end is a truncated encoding sequence.
    # TODO - may be do line at a time to get around this problem
    proc detectFileEncoding {path {sampleLength {}}} {
        set fd [open $path rb]
        try {
            set data [read $fd {*}$sampleLength]
        } finally {
            close $fd
        }
        # ICU sometimes returns ISO8859-1 for UTF-8 since
        # all bytes are always valid is 8859-1. So always check
        # UTF-8 first
        if {[checkEncoding $data utf-8]} {
            return utf-8
        }

        # Get possible encodings in order of confidence
        set encodingCandidates [detect $data -all]
        # Pick the first that has a Tcl equivalent skipping utf-8
        # as already checked above.
        foreach icuName $encodingCandidates {
            set tclName [icuToTcl $icuName]
            if {$tclName ne "" && $tclName ne "utf-8" && [checkEncoding $data $tclName]} {
                return $tclName
            }
        }
        return ""
    }

    # Sources a file, attempting to guess an encoding if one is not
    # specified. Logs a message if encoding was not the default UTF-8
    proc source {args} {
        Init
        if {[llength $args] == 1} {
            # No options specified. Try to determine encoding.
            # In case of errors, just invoke ::source as is
            if {[catch {
                set path [lindex $args end]
                set tclName [detectFileEncoding $path]
                if {$tclName ne "" && $tclName ne "utf-8"} {
                    log "Warning: Encoding of $path is not UTF-8. Sourcing with encoding $tclName."
                    set args [linsert $args 0 -encoding $tclName]
                }
            } message]} {
                log "Warning: Error detecting encoding for $path: $message"
            }
        }
        tailcall ::source {*}$args
    }

    namespace export {[a-z]*}
    namespace ensemble create
}
