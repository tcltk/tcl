# msgcat.tcl --
#
#	This file defines various procedures which implement a
#	message catalog facility for Tcl programs.  It should be
#	loaded with the command "package require msgcat".
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
# Copyright (c) 1998 by Mark Harrison.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: msgcat.tcl,v 1.11.8.1 2002/06/10 05:33:14 wolfsuit Exp $

package require Tcl 8.2
package provide msgcat 1.2.3

namespace eval msgcat {
    namespace export mc mcload mclocale mcmax mcmset mcpreferences mcset \
	    mcunknown

    # Records the current locale as passed to mclocale
    variable locale ""

    # Records the list of locales to search
    variable loclist {}

    # Records the mapping between source strings and translated strings.  The
    # array key is of the form "<locale>,<namespace>,<src>" and the value is
    # the translated string.
    array set msgs {}
}

# msgcat::mc --
#
#	Find the translation for the given string based on the current
#	locale setting. Check the local namespace first, then look in each
#	parent namespace until the source is found.  If additional args are
#	specified, use the format command to work them into the traslated
#	string.
#
# Arguments:
#	src	The string to translate.
#	args	Args to pass to the format command
#
# Results:
#	Returns the translatd string.  Propagates errors thrown by the 
#	format command.

proc msgcat::mc {src args} {
    # Check for the src in each namespace starting from the local and
    # ending in the global.

    variable msgs
    variable loclist
    variable locale

    set ns [uplevel 1 [list ::namespace current]]
    
    while {$ns != ""} {
	foreach loc $loclist {
	    if {[info exists msgs($loc,$ns,$src)]} {
		if {[llength $args] == 0} {
		    return $msgs($loc,$ns,$src)
		} else {
		    return [uplevel 1 \
			    [linsert $args 0 ::format $msgs($loc,$ns,$src)]]
		}
	    }
	}
	set ns [namespace parent $ns]
    }
    # we have not found the translation
    return [uplevel 1 \
	    [linsert $args 0 [::namespace origin mcunknown] $locale $src]]
}

# msgcat::mclocale --
#
#	Query or set the current locale.
#
# Arguments:
#	newLocale	(Optional) The new locale string. Locale strings
#			should be composed of one or more sublocale parts
#			separated by underscores (e.g. en_US).
#
# Results:
#	Returns the current locale.

proc msgcat::mclocale {args} {
    variable loclist
    variable locale
    set len [llength $args]

    if {$len > 1} {
	error {wrong # args: should be "mclocale ?newLocale?"}
    }

    if {$len == 1} {
	set locale [string tolower [lindex $args 0]]
	set loclist {}
	set word ""
	foreach part [split $locale _] {
	    set word [string trimleft "${word}_${part}" _]
	    set loclist [linsert $loclist 0 $word]
	}
    }
    return $locale
}

# msgcat::mcpreferences --
#
#	Fetch the list of locales used to look up strings, ordered from
#	most preferred to least preferred.
#
# Arguments:
#	None.
#
# Results:
#	Returns an ordered list of the locales preferred by the user.

proc msgcat::mcpreferences {} {
    variable loclist
    return $loclist
}

# msgcat::mcload --
#
#	Attempt to load message catalogs for each locale in the
#	preference list from the specified directory.
#
# Arguments:
#	langdir		The directory to search.
#
# Results:
#	Returns the number of message catalogs that were loaded.

proc msgcat::mcload {langdir} {
    set x 0
    foreach p [mcpreferences] {
	set langfile [file join $langdir $p.msg]
	if {[file exists $langfile]} {
	    incr x
	    set fid [open $langfile "r"]
	    fconfigure $fid -encoding utf-8
            uplevel 1 [read $fid]
	    close $fid
	}
    }
    return $x
}

# msgcat::mcset --
#
#	Set the translation for a given string in a specified locale.
#
# Arguments:
#	locale		The locale to use.
#	src		The source string.
#	dest		(Optional) The translated string.  If omitted,
#			the source string is used.
#
# Results:
#	Returns the new locale.

proc msgcat::mcset {locale src {dest ""}} {
    variable msgs
    if {[string equal $dest ""]} {
	set dest $src
    }

    set ns [uplevel 1 [list ::namespace current]]

    set msgs([string tolower $locale],$ns,$src) $dest
    return $dest
}

# msgcat::mcmset --
#
#	Set the translation for multiple strings in a specified locale.
#
# Arguments:
#	locale		The locale to use.
#	pairs		One or more src/dest pairs (must be even length)
#
# Results:
#	Returns the number of pairs processed

proc msgcat::mcmset {locale pairs } {
    variable msgs

    set length [llength $pairs]
    if {$length % 2} {
	error {bad translation list: should be "mcmset locale {src dest ...}"}
    }
    
    set locale [string tolower $locale]
    set ns [uplevel 1 [list ::namespace current]]
    
    foreach {src dest} $pairs {
        set msgs($locale,$ns,$src) $dest
    }
    
    return $length
}

# msgcat::mcunknown --
#
#	This routine is called by msgcat::mc if a translation cannot
#	be found for a string.  This routine is intended to be replaced
#	by an application specific routine for error reporting
#	purposes.  The default behavior is to return the source string.  
#	If additional args are specified, the format command will be used
#	to work them into the traslated string.
#
# Arguments:
#	locale		The current locale.
#	src		The string to be translated.
#	args		Args to pass to the format command
#
# Results:
#	Returns the translated value.

proc msgcat::mcunknown {locale src args} {
    if {[llength $args]} {
	return [uplevel 1 [linsert $args 0 ::format $src]]
    } else {
	return $src
    }
}

# msgcat::mcmax --
#
#	Calculates the maximun length of the translated strings of the given 
#	list.
#
# Arguments:
#	args	strings to translate.
#
# Results:
#	Returns the length of the longest translated string.

proc msgcat::mcmax {args} {
    set max 0
    foreach string $args {
	set translated [uplevel 1 [list [namespace origin mc] $string]]
        set len [string length $translated]
        if {$len>$max} {
            set max $len
        }
    }
    return $max
}

# Initialize the default locale

namespace eval msgcat {
    # set default locale, try to get from environment
    if {[info exists ::env(LANG)]} {
        mclocale $::env(LANG)
    } else {
        if { $tcl_platform(platform) == "windows" } {
            # try to set locale depending on registry settings
            #
            set key {HKEY_CURRENT_USER\Control Panel\International}
            if {[catch {package require registry}] || \
		    [catch {registry get $key "locale"} locale]} {
                mclocale "C"
            } else {
		
                #
                # Clean up registry value for translating LCID value
                # by using only the last 2 digits, since first
                # 2 digits appear to be the country...  For example
                #     0409 - English - United States
                #     0809 - English - United Kingdom
                #
                set locale [string trimleft $locale "0"]
                set locale [string range $locale end-1 end]
                set locale [string tolower $locale]
                switch -- $locale {
		    01      { mclocale "ar" }
		    02      { mclocale "bg" }
		    03      { mclocale "ca" }
		    04      { mclocale "zh" }
		    05      { mclocale "cs" }
		    06      { mclocale "da" }
		    07      { mclocale "de" }
		    08      { mclocale "el" }
		    09      { mclocale "en" }
		    0a      { mclocale "es" }
		    0b      { mclocale "fi" }
		    0c      { mclocale "fr" }
		    0d      { mclocale "he" }
		    0e      { mclocale "hu" }
		    0f      { mclocale "is" }
		    10      { mclocale "it" }
		    11      { mclocale "ja" }
		    12      { mclocale "ko" }
		    13      { mclocale "da" }
		    14      { mclocale "no" }
		    15      { mclocale "pl" }
		    16      { mclocale "pt" }
		    
		    default  { mclocale "C" }
		}
            }
        } else {
            mclocale "C"
        }
    }
}
