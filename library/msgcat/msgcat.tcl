# msgcat.tcl --
#
# This file defines various procedures which implement a
# message catalog facility for Tcl programs.  It should be
# loaded with the command "package require msgcat".
#
# Copyright (c) 1998 by Mark Harrison.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# $Id: msgcat.tcl,v 1.1.2.1 1998/12/04 18:52:40 stanton Exp $
#

#---------------------------------------------------------------
# message catalog procedures
#---------------------------------------------------------------

namespace eval msgcat {
    set _locale ""
    set _loclist {}

    proc mc {src} {
        set ns [uplevel {namespace current}]
        foreach loc $::msgcat::_loclist {
            if {[info exists ::msgcat::_msg($loc,$ns,$src)]} {
                return $::msgcat::_msg($loc,$ns,$src)
            }
        }
        # we have not found the translation
        return [mcunknown $::msgcat::_locale $src]
    }

    proc mclocale {args} {
        set len [llength $args]
        if {$len == 0} {
            return $::msgcat::_locale
        } elseif {$len == 1} {
            set ::msgcat::_locale $args
            set ::msgcat::_loclist {}
            set word ""
            foreach part [split $args _] {
                set word [string trimleft "${word}_${part}" _]
                set ::msgcat::_loclist \
                    [linsert $::msgcat::_loclist 0 $word]
            }
        } else {
            # incorrect argument count
            error {called "mclocale" with too many arguments}
        }
    }

    proc mcpreferences {} {
        return $::msgcat::_loclist
    }

    proc mcload {langdir} {
        foreach p [::msgcat::mcpreferences] {
            set langfile $langdir/$p.msg
            if {[file exists $langfile]} {
                uplevel [list source $langfile]
            }
        }
    }

    proc mcset {locale src {dest ""}} {
        if {[string compare "" $dest] == 0} {
            set dest $src
        }

        set ns [uplevel {namespace current}]

        set ::msgcat::_msg($locale,$ns,$src) $dest
    }

    proc mcunknown {locale src} {
        return $src
    }

    # set default locale try to get from environment
    if {[info exists ::env(LANG)]} {
        mclocale $::env(LANG)
    } else {
        mclocale "C"
    }

    namespace export mc mcset mclocale mclocales mcunknown
}

package provide msgcat 1.0

#eof: msgcat.tcl
