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

namespace eval ::tcl::unsupported::icu {
    # Map Tcl encoding names to ICU and back. Note ICU has multiple aliases
    # for the same encoding.
    variable tclToIcu
    variable icuToTcl

    proc Init {} {
        variable tclToIcu
        variable icuToTcl
        # Ignore all errors. Do not want to hold up Tcl
        # if ICU not available
        catch {
            foreach tclName [encoding names] {
                set icuNames [aliases $tclName]
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

        # Redefine ourselves to no-op. Note "args" argument as it
        # seems required for byte code compiler to optimize the
        # to a single noop
        proc Init args {}
    }

    proc icuToTcl {icuName} {
        Init
        proc icuToTcl {icuName} {
            variable icuToTcl
            return [lindex $icuToTcl($icuName) 0]
        }
        icuToTcl $icuName
    }

    proc tclToIcu {tclName} {
        Init
        proc tclToIcu {tclName} {
            variable tclToIcu
            return [lindex $tclToIcu($tclName) 0]
        }
        tclToIcu $tclName
    }

    namespace export {[a-z]*}
    namespace ensemble create
}
