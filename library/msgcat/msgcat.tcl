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
# RCS: @(#) $Id: msgcat.tcl,v 1.14 2002/06/17 05:37:39 dgp Exp $

package require Tcl 8.2
package provide msgcat 1.3

namespace eval msgcat {
    namespace export mc mcload mclocale mcmax mcmset mcpreferences mcset \
	    mcunknown

    # Records the current locale as passed to mclocale
    variable Locale ""

    # Records the list of locales to search
    variable Loclist {}

    # Records the mapping between source strings and translated strings.  The
    # array key is of the form "<locale>,<namespace>,<src>" and the value is
    # the translated string.
    array set Msgs {}

    # Map of language codes used in Windows registry to those of ISO-639
    array set WinRegToISO639 {
	0409 en_US 0809 en_UK 43c gd 83c ga 01 ar 02 bg 03 ca 04 zh 05
	cs 06 da 07 de 08 el 0a es 0b fi 0c fr 0d he 0e hu 0f is 10 it
	11 ja 12 ko 13 da 14 no 15 pl 16 pt 17 rm 18 ro 19 ru 1a hr
	1b sk 1c sq 1d sv 1e th 1f tr 20 ur 21 id 22 uk 23 be 24 sl
	25 et 26 lv 27 lt 28 tg 29 fa 2a vi 2b hy 2c az 2d eu 2e wen
	2f mk 30 bnt 31 ts 33 ven 34 xh 35 zu 36 af 37 ka 38 fo 39 hi
	3a mt 3b se 3d yi 3e ms 3f kk 40 ky 41 sw 42 tk 43 uz 44 tt
	45 bn 46 pa 47 gu 48 or 49 ta 4a te 4b kn 4c ml 4d as 4e mr
	4f sa 50 mn 51 bo 52 cy 53 km 54 lo 55 my 56 gl 57 kok 58 mni
	59 sd 5a syr 5b si 5c chr 5d iu 5e am 5f ber 60 ks 61 ne 62 fy
	63 ps 64 tl 65 div 66 bin 67 ful 68 ha 69 nic 6a yo 70 ibo
	71 kau 72 om 73 ti 74 gn 75 cpe 76 la 77 so 78 sit 79 pap
    }
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

    variable Msgs
    variable Loclist
    variable Locale

    set ns [uplevel 1 [list ::namespace current]]
    
    while {$ns != ""} {
	foreach loc $Loclist {
	    if {[info exists Msgs($loc,$ns,$src)]} {
		if {[llength $args] == 0} {
		    return $Msgs($loc,$ns,$src)
		} else {
		    return [uplevel 1 \
			    [linsert $args 0 ::format $Msgs($loc,$ns,$src)]]
		}
	    }
	}
	set ns [namespace parent $ns]
    }
    # we have not found the translation
    return [uplevel 1 \
	    [linsert $args 0 [::namespace origin mcunknown] $Locale $src]]
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
    variable Loclist
    variable Locale
    set len [llength $args]

    if {$len > 1} {
	error {wrong # args: should be "mclocale ?newLocale?"}
    }

    if {$len == 1} {
	set Locale [string tolower [lindex $args 0]]
	set Loclist {}
	set word ""
	foreach part [split $Locale _] {
	    set word [string trimleft "${word}_${part}" _]
	    set Loclist [linsert $Loclist 0 $word]
	}
    }
    return $Locale
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
    variable Loclist
    return $Loclist
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
    variable Msgs
    if {[string equal $dest ""]} {
	set dest $src
    }

    set ns [uplevel 1 [list ::namespace current]]

    set Msgs([string tolower $locale],$ns,$src) $dest
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
    variable Msgs

    set length [llength $pairs]
    if {$length % 2} {
	error {bad translation list: should be "mcmset locale {src dest ...}"}
    }
    
    set locale [string tolower $locale]
    set ns [uplevel 1 [list ::namespace current]]
    
    foreach {src dest} $pairs {
        set Msgs($locale,$ns,$src) $dest
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

# Convert the locale values stored in environment variables to a form
# suitable for passing to [mclocale]
proc msgcat::ConvertLocale {value} {
    # Assume $value is of form: $language[_$territory][.$codeset][@modifier]
    # Convert to form: $language[_$territory][_$modifier]
    #
    # Comment out expanded RE version -- bugs alleged
    #regexp -expanded {
    #	^		# Match all the way to the beginning
    #	([^_.@]*)	# Match "lanugage"; ends with _, ., or @
    #	(_([^.@]*))?	# Match (optional) "territory"; starts with _
    #	([.]([^@]*))?	# Match (optional) "codeset"; starts with .
    #	(@(.*))?	# Match (optional) "modifier"; starts with @
    #	$		# Match all the way to the end
    #} $value -> language _ territory _ codeset _ modifier
    regexp {^([^_.@]*)(_([^.@]*))?([.]([^@]*))?(@(.*))?$} $value \
	    -> language _ territory _ codeset _ modifier
    set ret $language
    if {[string length $territory]} {
	append ret _$territory
    }
    if {[string length $modifier]} {
	append ret _$modifier
    }
    return $ret
}

# Initialize the default locale
proc msgcat::Init {} {
    #
    # set default locale, try to get from environment
    #
    foreach varName {LC_ALL LC_MESSAGES LANG} {
	if {[info exists ::env($varName)] 
		&& ![string equal "" $::env($varName)]} {
            mclocale [ConvertLocale $::env($varName)]
	    return
	}
    }
    #
    # On Windows, try to set locale depending on registry settings,
    # or fall back on locale of "C".  Other platforms will return
    # when they fail to load the registry package.
    #
    set key {HKEY_CURRENT_USER\Control Panel\International}
    if {[catch {package require registry}] \
	    || [catch {registry get $key "locale"} locale]} {
        mclocale "C"
	return
    }
    #
    # Keep trying to match against smaller and smaller suffixes
    # of the registry value, since the latter hexadigits appear
    # to determine general language and earlier hexadigits determine
    # more precise information, such as territory.  For example,
    #     0409 - English - United States
    #     0809 - English - United Kingdom
    # Add more translations to the WinRegToISO639 array above.
    #
    variable WinRegToISO639
    set locale [string tolower $locale]
    while {[string length $locale]} {
        if {![catch {mclocale $WinRegToISO639($locale)}]} {
	    return
	}
	set locale [string range $locale 1 end]
    }
    #
    # No translation known.  Fall back on "C" locale
    #
    mclocale C
}
msgcat::Init
