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

::tcl::unsupported::loadIcu

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

        # Redefine ourselves to no-op.
        proc Init {} {}
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

    # Prints a warning
    proc warn {message} {
        puts stderr "Warning: $message"
    }

    # Return 1 / 0 depending on whether the data can
    # be decoded with a given encoding
    proc checkEncoding {data enc} {
        encoding convertfrom -profile strict -failindex x $enc $data
        return [expr {$x < 0}]
    }

    # Detect the encoding for a file. If sampleLength is the
    # empty string, entire file is read.
    # NOTE: sampleLength other than "" has the problem that
    # the encoding may be perfectly valid but the data at
    # end is a truncated encoding sequence.
    # TODO - may be do line at a time to get around this problem
    proc detectFileEncoding {path {expectedEncoding utf-8} {sampleLength {}}} {
        Init
        set fd [open $path rb]
        try {
            set data [read $fd {*}$sampleLength]
        } finally {
            close $fd
        }
        if {[checkEncoding $data $expectedEncoding]} {
            return $expectedEncoding
        }

        # ICU sometimes returns ISO8859-1 for UTF-8 since all bytes are always
        # valid in 8859-1. So always check UTF-8 first
        if {$expectedEncoding ne "utf-8"} {
            if {[checkEncoding $data "utf-8"]} {
                return utf-8
            }
        }

        # Get possible encodings in order of confidence
        set encodingCandidates [detect $data -all]
        # Pick the first that has a Tcl equivalent skipping utf-8
        # as already checked above.
        foreach icuName $encodingCandidates {
            set tclName [icuToTcl $icuName]
            if {$tclName ne "" &&
                $tclName ne $expectedEncoding &&
                $tclName ne "utf-8" &&
                [checkEncoding $data $tclName]} {
                return $tclName
            }
        }

        return ""
    }

    # Checks if path begins with a tilde and expands it after
    # logging a warning
    proc tildeexpand {path {cmd {}}} {
        if {[string index $path 0] eq "~" && ![file exists $path]} {
            if {$cmd ne ""} {
                append cmd " command "
            }
            warn [string cat "Tcl 9 ${cmd}does not do tilde expansion on paths." \
                      " Change code to explicitly call \"file tildeexpand\"." \
                      " Expanding \"$path\"."]
            set path [::file tildeexpand $path]
        }
        return $path
    }

    # Checks if file command argument needs tilde expansion,
    # expanding it if so after a warning.
    proc file {cmd args} {
        switch $cmd {
            atime - attributes - dirname - executable - exists - extension -
            isdirectory - isfile - lstat - mtime - nativename - normalize -
            owned - pathtype - readable - readlink - rootname - size - split -
            separator - stat - system - tail - type - writable {
                # First argument if present is the name
                if {[llength $args]} {
                    # Replace first arg with expanded form
                    ledit args 0 0 [tildeexpand [lindex $args 0] "file $cmd"]
                }
            }
            copy -
            delete -
            link -
            mkdir -
            rename {
                # Some arguments may be options, all others paths. Only check
                # if beginning with tilde. Option will not begin with tilde
                set args [lmap arg $args {
                    tildeexpand $arg "file $cmd"
                }]
            }
            join {
                # Cannot tilde expand as semantics will not be same.
                # Just warn, throw away result
                foreach arg $args {
                    tildeexpand $arg "file $cmd"
                }
            }
            home -
            tempdir -
            tempfile -
            tildeexpand {
                # Naught to do
            }
        }
        tailcall ::file $cmd {*}$args
    }

    # Opens a channel with an appropriate encoding if it cannot be read with
    # configured encoding. Also prints warning if path begins with a ~ and
    # tilde expands it on caller's behalf, again emitting a warning.
    proc open {path args} {
        if {[catch {
            set path [tildeexpand $path]

            # Avoid /dev/random etc.
            if {[file isfile $path] && [file size $path] > 0} {
                # Files are opened in system encoding by default. Ensure file
                # readable with that encoding.
                set encoding [detectFileEncoding $path [encoding system]]
                if {$encoding eq ""} {
                    unset encoding
                }
            }
        } message]} {
           warn $message
        }

        # Actual open should not be in a catch!
        set fd [::open $path {*}$args]

        catch {
            if {[info exists encoding]} {
                if {[fconfigure $fd -encoding] ne "binary"} {
                    if {$encoding ne [encoding system]} {
                        warn [string cat "File \"$path\" is not in the system encoding." \
                                  " Configuring channel for encoding $encoding." \
                                  " This warning may be ignored if the code subsequently sets the encoding."]
                    fconfigure $fd -encoding $encoding
                    }
                }
            }
        }
        return $fd
    }

    # Sources a file, attempting to guess an encoding if one is not
    # specified. Logs a message if encoding was not the default UTF-8
    proc source {args} {
        if {[llength $args] == 1} {
            # No options specified. Try to determine encoding.
            # In case of errors, just invoke ::source as is
            if {[catch {
                set path [lindex $args end]
                set tclName [detectFileEncoding $path]
                if {$tclName ne "" && $tclName ne "utf-8"} {
                    warn "Encoding of $path is not UTF-8. Sourcing with encoding $tclName."
                    set args [linsert $args 0 -encoding $tclName]
                }
            } message]} {
                warn "Error detecting encoding for $path: $message"
            }
        }
        tailcall ::source {*}$args
    }

    namespace export {[a-z]*}
    namespace ensemble create
}
