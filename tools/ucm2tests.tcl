# ucm2tests.tcl
#
# Parses given ucm files (from ICU) to generate test data
# for encodings. The generated scripts are written to stdout.
#
#  tclsh ucmtotests.tcl PATH_TO_ICU_UCM_DIRECTORY
#

namespace eval ucm {
    # No means to change these currently but ...
    variable outputChan stdout
    variable errorChan stderr
    variable verbose 0

    # Map Tcl encoding name to ICU UCM file name
    variable encNameMap
    array set encNameMap {
        cp1250    glibc-CP1250-2.1.2
        cp1251    glibc-CP1251-2.1.2
        cp1252    glibc-CP1252-2.1.2
        cp1253    glibc-CP1253-2.1.2
        cp1254    glibc-CP1254-2.1.2
        cp1255    glibc-CP1255-2.1.2
        cp1256    glibc-CP1256-2.1.2
        cp1257    glibc-CP1257-2.1.2
        cp1258    glibc-CP1258-2.1.2
        iso8859-1 glibc-ISO_8859_1-2.1.2
        iso8859-2 glibc-ISO_8859_2-2.1.2
        iso8859-3 glibc-ISO_8859_3-2.1.2
        iso8859-4 glibc-ISO_8859_4-2.1.2
        iso8859-5 glibc-ISO_8859_5-2.1.2
        iso8859-6 glibc-ISO_8859_6-2.1.2
        iso8859-7 glibc-ISO_8859_7-2.3.3
        iso8859-8 glibc-ISO_8859_8-2.3.3
        iso8859-9 glibc-ISO_8859_9-2.1.2
        iso8859-10 glibc-ISO_8859_10-2.1.2
        iso8859-11 glibc-ISO_8859_11-2.1.2
        iso8859-13 glibc-ISO_8859_13-2.1.2
        iso8859-14 glibc-ISO_8859_14-2.1.2
        iso8859-15 glibc-ISO_8859_15-2.1.2
        iso8859-16 glibc-ISO_8859_16-2.3.3
    }

    # Dictionary Character map for Tcl encoding
    variable charMap
}

proc ucm::abort {msg} {
    variable errorChan
    puts $errorChan $msg
    exit 1
}
proc ucm::warn {msg} {
    variable errorChan
    puts $errorChan $msg
}
proc ucm::log {msg} {
    variable verbose
    if {$verbose} {
        variable errorChan
        puts $errorChan $msg
    }
}
proc ucm::print {s} {
    variable outputChan
    puts $outputChan $s
}

proc ucm::parse_SBCS {fd} {
    set result {}
    while {[gets $fd line] >= 0} {
        if {[string match #* $line]} {
            continue
        }
        if {[string equal "END CHARMAP" [string trim $line]]} {
            break
        }
        if {![regexp {^\s*<U([[:xdigit:]]{4})>\s*((\\x[[:xdigit:]]{2})+)\s*(\|(0|1|2|3|4))} $line -> unichar bytes - - precision]} {
            error "Unexpected line parsing SBCS: $line"
        }
        set bytes [string map {\\x {}} $bytes]; # \xNN -> NN
        if {$precision eq "" || $precision eq "0"} {
            lappend result $unichar $bytes
        } else {
            # It is a fallback mapping - ignore
        }
    }
    return $result
}

proc ucm::generate_tests {} {
    variable encNameMap
    variable charMap

    array set tclNames {}
    foreach encName [encoding names] {
        set tclNames($encName) ""
    }
    foreach encName [lsort [array names encNameMap]] {
        if {![info exists charMap($encName)]} {
            warn "No character map read for $encName"
            continue
        }
        unset tclNames($encName)
        print "\n# $encName (generated from $encNameMap($encName))"
        print "lappend encValidStrings {*}{"
        foreach {unich hex} $charMap($encName) {
            print "    $encName \\u$unich $hex {} {}"
        }
        print "}; # $encName"
    }
    if {[array size tclNames]} {
        warn "Missing encoding: [lsort [array names tclNames]]"
    }
}

proc ucm::parse_file {encName ucmPath} {
    variable charMap
    set fd [open $ucmPath]
    try {
        # Parse the metadata
        unset -nocomplain state
        while {[gets $fd line] >= 0} {
            if {[regexp {<(code_set_name|mb_cur_max|mb_cur_min|uconv_class|subchar)>\s+(\S+)} $line -> key val]} {
                set state($key) $val
            } elseif {[regexp {^\s*CHARMAP\s*$} $line]} {
                set state(charmap) ""
                break
            } else {
                # Skip all else
            }
        }
        if {![info exists state(charmap)]} {
            abort "Error: $path has No CHARMAP line."
        }
        foreach key {code_set_name uconv_class} {
            if {[info exists state($key)]} {
                set state($key) [string trim $state($key) {"}]
            }
        }
        if {[info exists charMap($encName)]} {
            abort "Duplicate file for $encName ($path)"
        }
        if {![info exists state(uconv_class)]} {
            abort "Error: $path has no uconv_class definition."
        }
        switch -exact -- $state(uconv_class) {
            SBCS {
                if {[catch {
                    set charMap($encName) [parse_SBCS $fd]
                } result]} {
                    abort "Could not process $path. $result"
                }
            }
            default {
                log "Skipping $path -- not SBCS encoding."
                return
            }
        }
    } finally {
        close $fd
    }
}

proc ucm::expand_paths {patterns} {
    set expanded {}
    foreach pat $patterns {
        # The file join is for \ -> /
        lappend expanded {*}[glob -nocomplain [file join $pat]]
    }
    return $expanded
}

proc ucm::run {} {
    variable encNameMap
    if {[llength $::argv] != 1} {
        abort "Usage: [info nameofexecutable] $::argv0 PATHTOUCMFILES"
    }
    foreach {encName fname} [array get encNameMap] {
        ucm::parse_file $encName [file join [lindex $::argv 0] ${fname}.ucm]
    }
    generate_tests
}

ucm::run
