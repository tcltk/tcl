#----------------------------------------------------------------------
#
# clock.tcl --
#
#	This file implements the portions of the [clock] ensemble that
#	are coded in Tcl.  Refer to the users' manual to see the description
#	of the [clock] command and its subcommands.
#
#
#----------------------------------------------------------------------
#
# Copyright (c) 2004 by Kevin B. Kenny.  All rights reserved.
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: clock.tcl,v 1.1 2004/08/18 19:59:00 kennykb Exp $
#
#----------------------------------------------------------------------

# We must have message catalogs that support the root locale, and
# we need access to the Registry on Windows systems.  We also need
# Tcl 8.5 dictionaries.

uplevel \#0 {
    package require msgcat 1.4
    if { $::tcl_platform(platform) eq {windows} } {
	package require registry 1.1
    }
}

# Put the library directory into the namespace for the ensemble
# so that the library code can find message catalogs and time zone
# definition files.

namespace eval ::tcl::clock \
    [list variable LibDir [file dirname [info script]]]

#----------------------------------------------------------------------
#
# clock --
#
#	Manipulate times.
#
# The 'clock' command manipulates time.  Refer to the user documentation
# for the available subcommands and what they do.
#
#----------------------------------------------------------------------	

namespace eval ::tcl::clock {

    # Export the subcommands

    namespace export format
    namespace export clicks
    namespace export microseconds
    namespace export milliseconds
    namespace export scan
    namespace export seconds
    namespace export add

    # Import the message catalog commands that we use.

    namespace import ::msgcat::mc
    namespace import ::msgcat::mcload
    namespace import ::msgcat::mclocale

    # Define the Greenwich time zone

    variable TZData
    set TZData(:Etc/GMT) {
	{-9223372036854775808 0 0 GMT}
    }
    set TZData(:GMT) $TZData(:Etc/GMT)
    set TZData(:Etc/UTC) {
	{-9223372036854775808 0 0 UTC}
    }
    set TZData(:UTC) $TZData(:Etc/UTC)

    # Define the message catalog for the root locale.

    ::msgcat::mcmset {} {
	AM {am}
	BCE {B.C.E.}
	CE {C.E.}
	DATE_FORMAT {%m/%d/%Y}
	DATE_TIME_FORMAT {%a %b %e %H:%M:%S %Y}
	DAYS_OF_WEEK_ABBREV	{
	    Sun Mon Tue Wed Thu Fri Sat
	}
	DAYS_OF_WEEK_FULL	{
	    Sunday Monday Tuesday Wednesday Thursday Friday Saturday
	}
	GREGORIAN_CHANGE_DATE	2299161
	LOCALE_DATE_FORMAT {%m/%d/%Y}
	LOCALE_DATE_TIME_FORMAT {%a %b %e %H:%M:%S %Y}
	LOCALE_ERAS {}
	LOCALE_NUMERALS		{
	    00 01 02 03 04 05 06 07 08 09
	    10 11 12 13 14 15 16 17 18 19
	    20 21 22 23 24 25 26 27 28 29
	    30 31 32 33 34 35 36 37 38 39
	    40 41 42 43 44 45 46 47 48 49
	    50 51 52 53 54 55 56 57 58 59
	    60 61 62 63 64 65 66 67 68 69
	    70 71 72 73 74 75 76 77 78 79
	    80 81 82 83 84 85 86 87 88 89
	    90 91 92 93 94 95 96 97 98 99
	}
	LOCALE_TIME_FORMAT {%H:%M:%S}
	LOCALE_YEAR_FORMAT {%EC%Ey}
	MONTHS_ABBREV		{
	    Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
	}
	MONTHS_FULL		{
	    	January		February	March
	    	April		May		June
	    	July		August		September
		October		November	December
	}
	PM {pm}
	TIME_FORMAT {%H:%M:%S}
	TIME_FORMAT_12 {%I:%M:%S %P}
	TIME_FORMAT_24 {%H:%M}
	TIME_FORMAT_24_SECS {%H:%M:%S}
    }

    # Define a few Gregorian change dates for other locales.  In most cases
    # the change date follows a language, because a nation's colonies changed
    # at the same time as the nation itself.  In many cases, different
    # national boundaries existed; the dominating rule is to follow the
    # nation's capital.

    # Italy, Spain, Portugal, Poland

    ::msgcat::mcset it GREGORIAN_CHANGE_DATE 2299161
    ::msgcat::mcset es GREGORIAN_CHANGE_DATE 2299161
    ::msgcat::mcset pt GREGORIAN_CHANGE_DATE 2299161
    ::msgcat::mcset pl GREGORIAN_CHANGE_DATE 2299161

    # France, Austria

    ::msgcat::mcset fr GREGORIAN_CHANGE_DATE 2299227

    # For Belgium, we follow Southern Netherlands; Liege Diocese
    # changed several weeks later.

    ::msgcat::mcset fr_BE GREGORIAN_CHANGE_DATE 2299238
    ::msgcat::mcset nl_BE GREGORIAN_CHANGE_DATE 2299238

    # Austria

    ::msgcat::mcset de_AT GREGORIAN_CHANGE_DATE 2299527

    # Hungary

    ::msgcat::mcset hu GREGORIAN_CHANGE_DATE 2301004

    # Germany, Norway, Denmark (Catholic Germany changed earlier)

    ::msgcat::mcset de_DE GREGORIAN_CHANGE_DATE 2342032
    ::msgcat::mcset nb GREGORIAN_CHANGE_DATE 2342032    
    ::msgcat::mcset nn GREGORIAN_CHANGE_DATE 2342032
    ::msgcat::mcset no GREGORIAN_CHANGE_DATE 2342032
    ::msgcat::mcset da GREGORIAN_CHANGE_DATE 2342032

    # Holland (Brabant, Gelderland, Flanders, Friesland, etc. changed
    # at various times)

    ::msgcat::mcset nl GREGORIAN_CHANGE_DATE 2342165

    # Protestant Switzerland (Catholic cantons changed earlier)

    ::msgcat::mcset fr_CH GREGORIAN_CHANGE_DATE 2361342
    ::msgcat::mcset it_CH GREGORIAN_CHANGE_DATE 2361342
    ::msgcat::mcset de_CH GREGORIAN_CHANGE_DATE 2361342

    # English speaking countries

    ::msgcat::mcset en GREGORIAN_CHANGE_DATE 2361222

    # Sweden (had several changes onto and off of the Gregorian calendar)

    ::msgcat::mcset sv GREGORIAN_CHANGE_DATE 2361390

    # Russia

    ::msgcat::mcset ru GREGORIAN_CHANGE_DATE 2421639

    # Romania (Transylvania changed earler - perhaps de_RO should show
    # the earlier date?)

    ::msgcat::mcset ro GREGORIAN_CHANGE_DATE 2422063

    # Greece

    ::msgcat::mcset el GREGORIAN_CHANGE_DATE 2423480
    
    #------------------------------------------------------------------
    #
    #				CONSTANTS
    #
    #------------------------------------------------------------------

    # Paths at which binary time zone data for the Olson libraries
    # are known to reside on various operating systems

    variable ZoneinfoPaths {}
    proc ZoneinfoInit {} {
	variable ZoneinfoPaths
	rename ZoneinfoInit {}
	foreach path {
	    /usr/share/zoneinfo
	    /usr/share/lib/zoneinfo
	    /usr/local/etc/zoneinfo
	    C:/Progra~1/cygwin/usr/local/etc/zoneinfo
	} {
	    if { [file isdirectory $path] } {
		lappend ZoneinfoPaths $path
	    }
	}
    }
    ZoneinfoInit

    # Define the directories for time zone data and message catalogs.

    variable DataDir [file join $LibDir tzdata]
    variable MsgDir [file join $LibDir msgs]

    # Number of days in the months, in common years and leap years.

    variable DaysInRomanMonthInCommonYear \
	{ 31 28 31 30 31 30 31 31 30 31 30 31 }
    variable DaysInRomanMonthInLeapYear \
	{ 31 29 31 30 31 30 31 31 30 31 30 31 }
    variable DaysInPriorMonthsInCommonYear [list 0]
    variable DaysInPriorMonthsInLeapYear [list 0]
    set i 0
    foreach j $DaysInRomanMonthInCommonYear {
	lappend DaysInPriorMonthsInCommonYear [incr i $j]
    }
    set i 0
    foreach j $DaysInRomanMonthInLeapYear {
	lappend DaysInPriorMonthsInLeapYear [incr i $j]
    }
    unset i j

    # Julian day number of 0 January, 1 CE, in the proleptic Julian and
    # Gregorian calendars.

    variable JD0Jan1CEJul  1721423
    variable JD0Jan1CEGreg 1721425
    variable JD31Dec9999   5373484
    
    # Posix epoch, expressed as seconds from the Julian epoch

    variable PosixEpochAsJulianSeconds 210866803200

    # Another epoch (Hi, Jeff!)

    variable Roddenberry 1946

    # Integer ranges

    variable MINWIDE -9223372036854775808
    variable MAXWIDE 9223372036854775807

    # Day before Leap Day

    variable FEB_28	       58

    # Conversion factors

    variable DaysPer400Yr  146097;	# Days per 400 year Gregorian cycle
    variable DaysPerCentury 36524;	# Days per common Gregorian century
    variable DaysPer4Yr      1461;	# Days per 4 year cycle
    variable DaysPerYear      365;	# Days per common year
    variable DaysPerWeek        7;
    variable SecondsPerDay  86400;	# Seconds per day
    variable SecondsPerHour  3600;	# Seconds per hour
    variable SecondsPerMinute  60;	# Seconds per minute
    variable MinutesPerHour    60;	# Minutes per hour
    variable HoursPerDay       24;	# Hours per day

    # Translation table to map Windows TZI onto cities, so that
    # the Olson rules can apply.  In some cases the mapping is ambiguous,
    # so it's wise to specify $::env(TCL_TZ) rather than simply depending
    # on the system time zone.

    # The keys are long lists of values obtained from the time zone
    # information in the Registry.  In order, the list elements are:
    # 	Bias StandardBias DaylightBias
    #   StandardDate.wYear StandardDate.wMonth StandardDate.wDayOfWeek
    #   StandardDate.wDay StandardDate.wHour StandardDate.wMinute
    #   StandardDate.wSecond StandardDate.wMilliseconds
    #   DaylightDate.wYear DaylightDate.wMonth DaylightDate.wDayOfWeek
    #   DaylightDate.wDay DaylightDate.wHour DaylightDate.wMinute
    #   DaylightDate.wSecond DaylightDate.wMilliseconds
    # The values are the names of time zones where those rules apply.
    # There is considerable ambiguity in certain zones; an attempt has
    # been made to make a reasonable guess, but this table needs to be
    # taken with a grain of salt.

    variable WinZoneInfo [dict create \
	{-43200 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}  :Pacific/Kwajalein \
	{-39600 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}	 :Pacific/Midway \
	{-36000 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}  :Pacific/Honolulu \
	{-32400 0 3600 0 10 0 5 2 0 0 0 0 4 0 1 2 0 0 0} :America/Anchorage \
	{-28800 0 3600 0 10 0 5 2 0 0 0 0 4 0 1 2 0 0 0} :America/Los_Angeles \
	{-25200 0 3600 0 10 0 5 2 0 0 0 0 4 0 1 2 0 0 0} :America/Denver \
	{-25200 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}  :America/Phoenix \
	{-21600 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}  :America/Regina \
	{-21600 0 3600 0 10 0 5 2 0 0 0 0 4 0 1 2 0 0 0} :America/Chicago \
	{-18000 0 3600 0 10 0 5 2 0 0 0 0 4 0 1 2 0 0 0} :America/New_York \
	{-18000 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}  :America/Indianapolis \
	{-14400 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}  :America/Caracas \
	{-14400 0 3600 0 3 6 2 0 0 0 0 0 10 6 2 0 0 0 0} :America/Santiago \
	{-14400 0 3600 0 10 0 5 2 0 0 0 0 4 0 1 2 0 0 0} :America/Halifax \
	{-12600 0 3600 0 10 0 5 2 0 0 0 0 4 0 1 2 0 0 0} :America/St_Johns \
	{-10800 0 3600 0 2 0 2 2 0 0 0 0 10 0 3 2 0 0 0} :America/Sao_Paulo \
	{-10800 0 3600 0 10 0 5 2 0 0 0 0 4 0 1 2 0 0 0} :America/Godthab \
	{-10800 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}  :America/Buenos_Aires \
	{-7200 0 3600 0 9 0 5 2 0 0 0 0 3 0 5 2 0 0 0}   :America/Noronha \
	{-3600 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}  :Atlantic/Azores \
	{-3600 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Atlantic/Cape_Verde \
	{0 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}       :UTC \
	{0 0 3600 0 10 0 5 2 0 0 0 0 3 0 5 1 0 0 0}      :Europe/London \
	{3600 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}    :Africa/Kinshasa \
	{3600 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}   :CET \
	{7200 0 3600 0 9 3 5 2 0 0 0 0 5 5 1 2 0 0 0}    :Africa/Cairo \
	{7200 0 3600 0 10 0 5 4 0 0 0 0 3 0 5 3 0 0 0}   :Europe/Helsinki \
	{7200 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}    :Asia/Jerusalem \
	{7200 0 3600 0 9 0 5 1 0 0 0 0 3 0 5 0 0 0 0}    :Europe/Bucharest \
	{7200 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}   :Europe/Athens \
	{10800 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Riyadh \
	{10800 0 3600 0 10 0 1 4 0 0 0 0 4 0 1 3 0 0 0}  :Asia/Baghdad \
	{10800 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}  :Europe/Moscow \
	{12600 0 3600 0 9 2 4 2 0 0 0 0 3 0 1 2 0 0 0}   :Asia/Tehran \
	{14400 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Muscat \
	{14400 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}  :Asia/Tbilisi \
	{16200 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Kabul \
	{18000 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Karachi \
	{18000 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}  :Asia/Yekaterinburg \
	{19800 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Calcutta \
	{20700 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Katmandu \
	{21600 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Dhaka \
	{21600 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}  :Asia/Novosibirsk \
	{23400 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Rangoon \
	{25200 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Bangkok \
	{25200 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}  :Asia/Krasnoyarsk \
	{28800 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Chongqing \
	{28800 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}  :Asia/Irkutsk \
	{32400 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Asia/Tokyo \
	{32400 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}  :Asia/Yakutsk \
	{34200 0 3600 0 3 0 5 3 0 0 0 0 10 0 5 2 0 0 0}  :Australia/Adelaide \
	{34200 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Australia/Darwin \
	{36000 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Australia/Brisbane \
	{36000 0 3600 0 10 0 5 3 0 0 0 0 3 0 5 2 0 0 0}  :Asia/Vladivostok \
	{36000 0 3600 0 3 0 5 3 0 0 0 0 10 0 1 2 0 0 0}  :Australia/Hobart \
	{36000 0 3600 0 3 0 5 3 0 0 0 0 10 0 5 2 0 0 0}  :Australia/Sydney \
	{39600 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Pacific/Noumea \
	{43200 0 3600 0 3 0 3 2 0 0 0 0 10 0 1 2 0 0 0}  :Pacific/Auckland \
	{43200 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Pacific/Fiji \
	{46800 0 3600 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}   :Pacific/Tongatapu]

    # Groups of fields that specify the date, priorities, and 
    # code bursts that determine Julian Day Number given those groups.
    # The code in [clock scan] will choose the highest priority
    # (lowest numbered) set of fields that determines the date.

    variable DateParseActions {

	{ seconds } 0 {}

	{ julianDay } 1 {}

	{ century yearOfCentury month dayOfMonth } 2 {
	    dict set date era CE
	    dict set date year [expr { 100 * [dict get $date century]
				       + [dict get $date yearOfCentury] }]
	    set date [GetJulianDayFromEraYearMonthDay [K $date [set date {}]]]
	}
	{ century yearOfCentury dayOfYear } 2 {
	    dict set date era CE
	    dict set date year [expr { 100 * [dict get $date century]
				       + [dict get $date yearOfCentury] }]
	    set date [GetJulianDayFromEraYearDay [K $date [set date {}]]]
	}
	{ iso8601Century iso8601YearOfCentury iso8601Week dayOfWeek } 2 {
	    dict set date era CE
	    dict set date iso8601Year \
		[expr { 100 * [dict get $date iso8601Century]
			+ [dict get $date iso8601YearOfCentury] }]
	    set date [GetJulianDayFromEraYearWeekDay [K $date [set date {}]]]
	}

	{ yearOfCentury month dayOfMonth } 3 {
	    set date [InterpretTwoDigitYear [K $date [set date {}]] $baseTime]
	    dict set date era CE
	    set date [GetJulianDayFromEraYearMonthDay [K $date [set date {}]]]
	}
	{ yearOfCentury dayOfYear } 3 {
	    set date [InterpretTwoDigitYear [K $date [set date {}]] $baseTime]
	    dict set date era CE
	    set date [GetJulianDayFromEraYearDay [K $date [set date {}]]]
	}
	{ iso8601YearOfCentury iso8601Week dayOfWeek } 3 {
	    set date [InterpretTwoDigitYear \
			  [K $date [set date {}]] $baseTime \
			  iso8601YearOfCentury iso8601Year]
	    dict set date era CE
	    set date [GetJulianDayFromEraYearWeekDay [K $date [set date {}]]]
	}

	{ month dayOfMonth } 4 {
	    set date [AssignBaseYear [K $date [set date {}]] \
			  $baseTime $timeZone]
	    set date [GetJulianDayFromEraYearMonthDay [K $date [set date {}]]]
	}
	{ dayOfYear } 4 {
	    set date [AssignBaseYear [K $date [set date {}]] \
			  $baseTime $timeZone]
	    set date [GetJulianDayFromEraYearDay [K $date [set date {}]]]
	}
	{ iso8601Week dayOfWeek } 4 {
	    set date [AssignBaseIso8601Year [K $date [set date {}]] \
			  $baseTime $timeZone]
	    set date [GetJulianDayFromEraYearWeekDay [K $date [set date {}]]]
	}

	{ dayOfMonth } 5 {
	    set date [AssignBaseMonth [K $date [set date {}]] \
			  $baseTime $timeZone]
	    set date [GetJulianDayFromEraYearMonthDay [K $date [set date {}]]]
	}

	{ dayOfWeek } 6 {
	    set date [AssignBaseWeek [K $date [set date {}]] \
			  $baseTime $timeZone]
	    set date [GetJulianDayFromEraYearWeekDay [K $date [set date {}]]]
	}

	{} 7 {
	    set date [AssignBaseJulianDay [K $date [set date {}]] \
			  $baseTime $timeZone]
	}
    }

    # Groups of fields that specify time of day, priorities,
    # and code that processes them

    variable TimeParseActions {

	seconds 1 {}

	{ hourAMPM minute second amPmIndicator } 2 {
	    dict set date secondOfDay [InterpretHMSP $date]
	}
	{ hour minute second } 2 {
	    dict set date secondOfDay [InterpretHMS $date]
	}

	{ hourAMPM minute amPmIndicator } 3 {
	    dict set date second 0
	    dict set date secondOfDay [InterpretHMSP $date]
	}
	{ hour minute } 3 {
	    dict set date second 0
	    dict set date secondOfDay [InterpretHMS $date]
	}

	{ hourAMPM amPmIndicator } 4 {
	    dict set date minute 0
	    dict set date second 0
	    dict set date secondOfDay [InterpretHMSP $date]
	}
	{ hour } 4 {
	    dict set date minute 0
	    dict set date second 0
	    dict set date secondOfDay [InterpretHMS $date]
	}

	{ } 5 {
	    dict set date secondOfDay 0
	}
    }

    # Legacy time zones, used primarily for parsing RFC822 dates.

    variable LegacyTimeZone [dict create \
	gmt	+0000 \
	ut	+0000 \
	utc	+0000 \
	bst	+0100 \
	wet	+0000 \
	wat	-0100 \
	at	-0200 \
	nft	-0330 \
	nst	-0330 \
	ndt	-0230 \
	ast	-0400 \
	adt	-0300 \
	est	-0500 \
	edt	-0400 \
	cst	-0600 \
	cdt	-0500 \
	mst	-0700 \
	mdt	-0600 \
	pst	-0800 \
	pdt	-0700 \
	yst	-0900 \
	ydt	-0800 \
	hst	-1000 \
	hdt	-0900 \
	cat	-1000 \
	ahst	-1000 \
	nt	-1100 \
	idlw	-1200 \
	cet	+0100 \
	cest	+0200 \
	met	+0100 \
	mewt	+0100 \
	mest	+0200 \
	swt	+0100 \
	sst	+0200 \
	fwt	+0100 \
	fst	+0200 \
	eet	+0200 \
	eest	+0300 \
	bt	+0300 \
	it	+0330 \
	zp4	+0400 \
	zp5	+0500 \
	ist	+0530 \
	zp6	+0600 \
	wast	+0700 \
	wadt	+0800 \
	jt	+0730 \
	cct	+0800 \
	jst	+0900 \
	cast	+0930 \
	cadt	+1030 \
	east	+1000 \
	eadt	+1030 \
	gst	+1000 \
	nzt	+1200 \
	nzst	+1200 \
	nzdt	+1300 \
	idle	+1200 \
	a	+0100 \
	b	+0200 \
	c	+0300 \
	d	+0400 \
	e	+0500 \
	f	+0600 \
	g	+0700 \
	h	+0800 \
	i	+0900 \
	k	+1000 \
	l	+1100 \
	m	+1200 \
	n	-0100 \
	o	-0200 \
	p	-0300 \
	q	-0400 \
	r	-0500 \
	s	-0600 \
	t	-0700 \
	u	-0800 \
	v	-0900 \
	w	-1000 \
	x	-1100 \
	y	-1200 \
	z	+0000 \
    ]

    # Caches

    variable LocaleNumeralCache {};	# Dictionary whose keys are locale
					# names and whose values are pairs
					# comprising regexes matching numerals
					# in the given locales and dictionaries
					# mapping the numerals to their numeric
					# values.
    variable McLoaded {};		# Dictionary whose keys are locales
					# in which [mcload] has been executed
					# and whose values are immaterial
    # variable CachedSystemTimeZone;    # If 'CachedSystemTimeZone' exists,
					# it contains the value of the
					# system time zone, as determined from
					# the environment.
    variable TZData;			# Array whose keys are time zone names
					# and whose values are lists of quads
					# comprising start time, UTC offset,
					# Daylight Saving Time indicator, and
					# time zone abbreviation.
}

#----------------------------------------------------------------------
#
# K --
#
#	The K combinator returns its first argument.  It's used for
#	reference count management.
#
# Parameters:
#	x - Argument to be unreferenced.
#	y - Unused.
#
# Results:
#	Returns the first argument.
#
# Side effects:
#	None.
#
# The K combinator is used for its effect that [K $x [set x {}]]
# reads out the value of x destructively, giving an unshared Tcl
# object and avoiding 'copy on write'
#
#----------------------------------------------------------------------

proc ::tcl::clock::K { x y } { return $x }

#----------------------------------------------------------------------
#
# clock format --
#
#	Formats a count of seconds since the Posix Epoch as a time
#	of day.
#
# The 'clock format' command formats times of day for output.
# Refer to the user documentation to see what it does.
#
#----------------------------------------------------------------------

proc ::tcl::clock::format { args } {

    variable SecondsPerDay
    variable SecondsPerHour
    variable SecondsPerMinute
    variable MinutesPerHour
    variable HoursPerDay
    variable DaysPerYear
    variable DaysPerWeek

    set format {}

    # Check the count of args

    if { [llength $args] < 1 || [llength $args] % 2 != 1 } {
	return -code error \
	    -errorcode [list CLOCK wrongNumArgs] \
	    "wrong \# args: should be\
             \"[lindex [info level 0] 0] clockval\
             ?-format string? ?-gmt boolean?\
             ?-locale LOCALE? ?-timezone ZONE?\""
    }

    # Set defaults

    set clockval [lindex $args 0]
    set format {%a %b %d %H:%M:%S %Z %Y}
    set gmt 0
    set locale C
    set timezone [GetSystemTimeZone]

    # Pick up command line options.

    foreach { flag value } [lreplace $args 0 0] {
	set saw($flag) {}
	switch -exact -- $flag {
	    -format {
		set format $value
	    }
	    -gmt {
		set gmt $value
	    }
	    -locale {
		set locale $value
	    }
	    -timezone {
		set timezone $value
	    }
	    default {
		return -code error \
		    -errorcode [list CLOCK badSwitch $flag] \
		    "bad switch \"$flag\",\
                     must be -format, -gmt, -locale or -timezone"
	    }
	}
    }

    # Check options for validity

    if { [info exists saw(-gmt)] && [info exists saw(-timezone)] } {
	return -code error \
	    -errorcode [list CLOCK gmtWithTimezone] \
	    "cannot use -gmt and -timezone in same call"
    }
    if { [catch { expr { wide($clockval) } } result] } {
	return -code error \
	    "expected integer but got \"$clockval\"" 
    }
    if { ![string is boolean $gmt] } {
	return -code error \
	    "expected boolean value but got \"$gmt\""
    } else {
	if { $gmt } {
	    set timezone :GMT
	}
    }

    EnterLocale $locale oldLocale

    # Change locale if a fresh locale has been given on the command line.

    set status [catch {

	# Map away the locale-dependent composite format groups

	set format [LocalizeFormat $format]

	# Convert the given time to local time.
	
	set date [dict create seconds $clockval]
	set date [ConvertUTCToLocal [K $date [set date {}]] $timezone]
	
	# Extract the fields of the date.
	
	set date [GetJulianDay [K $date [set date {}]]]
	set date [GetGregorianEraYearDay [K $date [set date {}]]]
	set date [GetMonthDay [K $date [set date {}]]]
	set date [GetYearWeekDay [K $date [set date {}]]]
	
	# Format the result
	
	set state {}
	set retval {}
	foreach char [split $format {}] {
	    switch -exact $state {
		{} {
		    if { [string equal % $char] } {
			set state percent
		    } else {
			append retval $char
		    }
		}
		percent {		# Character following a '%' character
		    set state {}
		    switch -exact -- $char {
			% {		# A literal character, '%'
			    append retval %
			}
			a {		# Day of week, abbreviated
			    set dow [expr { [dict get $date dayOfWeek] % 7 }]
			    append retval \
				[lindex [mc DAYS_OF_WEEK_ABBREV] $dow]
			}
			A {		# Day of week, spelt out.
			    set dow [expr { [dict get $date dayOfWeek] % 7 }]
			    append retval [lindex [mc DAYS_OF_WEEK_FULL] $dow]
			}
			b - h {		# Name of month, abbreviated.
			    set month [expr { [dict get $date month] - 1 }]
			    append retval [lindex [mc MONTHS_ABBREV] $month]
			}
			B {		# Name of month, spelt out
			    set month [expr { [dict get $date month] - 1 }]
			    append retval [lindex [mc MONTHS_FULL] $month]
			}
			C {		# Century number
			    set cent [expr { [dict get $date year] / 100 }]
			    append retval [::format %02d $cent]
			}
			d {		# Day of month, with leading zero
			    append retval [::format %02d \
					       [dict get $date dayOfMonth]]
			}
			e {		# Day of month, without leading zero
			    append retval [::format %2d \
					       [dict get $date dayOfMonth]]
			}
			E {		# Format group in a locale-dependent
					# alternative era
			    set state percentE
			    if { ![dict exists $date localeEra] } {
				set date [GetLocaleEra [K $date [set date {}]]]
			    }
			}
			g {		# Two-digit year relative to ISO8601
					# week number
			    set year \
				[expr { [dict get $date iso8601Year] % 100 }]
			    append retval [::format %02d $year]
			}
			G {		# Four-digit year relative to ISO8601
					# week number
			    append retval [::format %04d \
					       [dict get $date iso8601Year]]
			}
			H {		# Hour in the 24-hour day, leading zero
			    append retval \
				[::format %02d \
				     [expr { [dict get $date localSeconds] 
					     / $SecondsPerHour
					     % $HoursPerDay }]]
			}
			I {		# Hour AM/PM, with leading zero
			    set hour12 \
				[expr { ( ( ( [dict get $date localSeconds]
					      % $SecondsPerDay )
					    + $SecondsPerDay
					    - $SecondsPerHour )
					  / $SecondsPerHour )
					% 12 + 1 }]
			    append retval [::format %02d $hour12]
			}
			j {		# Day of year (001-366)
			    append retval [::format %03d \
					       [dict get $date dayOfYear]]
			}
			J {		# Julian Day Number
			    append retval [::format %07ld \
					       [dict get $date julianDay]]
			}
			k {		# Hour (0-23), no leading zero
			    append retval \
				[::format %2d \
				     [expr { [dict get $date localSeconds] 
					     / $SecondsPerHour
					     % $HoursPerDay }]]
			}
			l {		# Hour (12-11), no leading zero
			    set hour12 \
				[expr { ( ( ( [dict get $date localSeconds]
					      % $SecondsPerDay )
					    + $SecondsPerDay
					    - $SecondsPerHour )
					  / $SecondsPerHour )
					% 12 + 1 }]
			    append retval [::format %2d $hour12]
			}
			m {		# Month number, leading zero
			    append retval [::format %02d \
					       [dict get $date month]]
			}
			M {		# Minute of the hour, leading zero
			    append retval \
				[::format %02d \
				     [expr { [dict get $date localSeconds] 
					     / $SecondsPerMinute
					     % $MinutesPerHour }]]
			}
			n {		# A literal newline
			    append retval \n
			}
			N {		# Month number, no leading zero
			    append retval [::format %2d \
					       [dict get $date month]]
			}
			O {		# A format group in the locale's
					# alternative numerals
			    set state percentO
			}
			p {		# Localized 'AM' or 'PM' indicator
					# converted to uppercase
			    set tod [expr { [dict get $date localSeconds] 
					    % $SecondsPerDay }]
			    if { $tod >= ( $SecondsPerDay / 2 ) } {
				append retval [string toupper [mc PM]]
			    } else {
				append retval [string toupper [mc AM]]
			    }
			}
			P {		# Localized 'AM' or 'PM' indicator
			    set tod [expr { [dict get $date localSeconds] 
					    % $SecondsPerDay }]
			    if { $tod >= ( $SecondsPerDay / 2 ) } {
				append retval [mc PM]
			    } else {
				append retval [mc AM]
			    }
			}
			Q {			# Hi, Jeff!
			    append retval [FormatStarDate $date]
			}
			s {		# Seconds from the Posix Epoch
			    append retval $clockval
			}
			S {		# Second of the minute, with 
					# leading zero
			    append retval \
				[::format %02d \
				     [expr { [dict get $date localSeconds] 
					     % $SecondsPerMinute }]]
			}
			t {		# A literal tab character
			    append retval \t
			}
			u {		# Day of the week (1-Monday, 7-Sunday)
			    append retval [dict get $date dayOfWeek]
			}
			U {		# Week of the year (00-53). The
					# first Sunday of the year is the
					# first day of week 01
			    set dow [dict get $date dayOfWeek]
			    if { $dow == 7 } {
				set dow 0
			    }
			    incr dow
			    set weekNumber \
				[expr { ( [dict get $date dayOfYear]
					  - $dow
					  + $DaysPerWeek )
					/ $DaysPerWeek }]
			    append retval [::format %02d $weekNumber]
			}
			V {		# The ISO8601 week number
			    append retval [::format %02d \
					       [dict get $date iso8601Week]]
			}
			w {		# Day of the week (0-Sunday,
					# 6-Saturday)
			    append retval \
				[expr { [dict get $date dayOfWeek] % 7 }]
			}
			W {		# Week of the year (00-53). The first
					# Monday of the year is the first day
					# of week 01.
			    set weekNumber \
				[expr { ( [dict get $date dayOfYear]
					  - [dict get $date dayOfWeek]
					  + $DaysPerWeek ) 
					/ $DaysPerWeek }]
			    append retval [::format %02d $weekNumber]
			}
			y {		# The two-digit year of the century
			    append retval \
				[::format %02d \
				     [expr { [dict get $date year] % 100 }]]
			}
			Y {		# The four-digit year
			    append retval [::format %04d \
					       [dict get $date year]]
			}
			z {		# The time zone as hours and minutes
					# east (+) or west (-) of Greenwich
			    set z [dict get $date tzOffset]
			    if { $z < 0 } {
				set z [expr { - $z }]
				append retval -
			    } else {
				append retval +
			    }
			    append retval [::format %02d \
					       [expr { $z / $SecondsPerHour }]]
			    set z [expr { $z % $SecondsPerHour }]
			    append retval \
				[::format %02d \
				     [expr { $z / $SecondsPerMinute }]]
			    set z [expr { $z % $SecondsPerMinute }]
			    if { $z != 0 } {
				append retval [::format %02d $z]
			    }
			}
			Z {		# The name of the time zone
			    append retval [dict get $date tzName]
			}
			% {		# A literal percent character
			    append retval %
			}
			default {	# An unknown escape sequence
			    append retval % $char
			}
		    }
		}
		percentE {		# Character following %E
		    set state {}
		    switch -exact -- $char {
			C {		# Locale-dependent era
			    append retval [dict get $date localeEra]
			}
			y {		# Locale-dependent year of the era
			    set y [dict get $date localeYear]
			    if { $y >= 0 && $y < 100 } {
				append retval [lindex [mc LOCALE_NUMERALS] $y]
			    } else {
				append retval $y
			    }
			}
			default {	# Unknown format group
			    append retval %E $char
			}
		    }
		}
		percentO {		# Character following %O
		    set state {}
		    switch -exact -- $char {
			d - e {		# Day of the month in alternative 
					# numerals
			    append retval [lindex \
					       [mc LOCALE_NUMERALS] \
					       [dict get $date dayOfMonth]]
			}
			H - k {		# Hour of the day in alternative
					# numerals
			    set hour [expr { [dict get $date localSeconds] 
					     / $SecondsPerHour
					     % $HoursPerDay }]
			    append retval [lindex [mc LOCALE_NUMERALS] $hour]
			}
			I - l {		# Hour (12-11) AM/PM in alternative
					# numerals
			    set hour12 \
				[expr { ( ( ( [dict get $date localSeconds]
					      % $SecondsPerDay )
					    + $SecondsPerDay
					    - $SecondsPerHour )
					  / $SecondsPerHour )
					% 12 + 1 }]
			    append retval [lindex [mc LOCALE_NUMERALS] $hour12]
			}
			m {		# Month number in alternative numerals
			    append retval [lindex \
					       [mc LOCALE_NUMERALS] \
					       [dict get $date month]]
			}
			M {		# Minute of the hour in alternative
					# numerals
			    set minute [expr { [dict get $date localSeconds] 
					       / $SecondsPerMinute
					       % $MinutesPerHour }]
			    append retval [lindex [mc LOCALE_NUMERALS] $minute]
			}
			S {		# Second of the minute in alternative
					# numerals
			    set second [expr { [dict get $date localSeconds] 
					       % $SecondsPerMinute }]
			    append retval [lindex [mc LOCALE_NUMERALS] $second]
			}
			u {		# Day of the week (Monday=1,Sunday=7)
					# in alternative numerals
			    append retval [lindex \
					       [mc LOCALE_NUMERALS] \
					       [dict get $date dayOfWeek]]
			}
			w {		# Day of the week (Sunday=0,Saturday=6)
					# in alternative numerals
			    append retval \
				[lindex \
				     [mc LOCALE_NUMERALS] \
				     [expr { [dict get $date dayOfWeek] % 7 }]]
			}
			y {		# Year of the century in alternative
					# numerals
			    append retval \
				[lindex \
				     [mc LOCALE_NUMERALS] \
				     [expr { [dict get $date year] % 100 }]]
			}
			default {	# Unknown format group
			    append retval %O $char
			}
		    }
		}
	    }
	}
	
	# Clean up any improperly terminated groups
	
	switch -exact -- $state {
	    percent {
		append retval %
	    }
	    percentE {
		append retval %E
	    }
	    percentO {
		append retval %O
	    }
	}

	set retval

    } result]

    # Restore the locale

    if { [info exists oldLocale] } {
	mclocale $oldLocale
    }

    if { $status == 1 } {
	if { [lindex $::errorCode 0] eq {clock} } {
	    return -code error $result
	} else {
	    return -code error \
		-errorcode $::errorCode -errorinfo $::errorInfo \
		$result
	}
    } else {
	return $result
    }

}

#----------------------------------------------------------------------
#
# clock scan --
#
#	Inputs a count of seconds since the Posix Epoch as a time
#	of day.
#
# The 'clock format' command scans times of day on input.
# Refer to the user documentation to see what it does.
#
#----------------------------------------------------------------------

proc ::tcl::clock::scan { args } {

    set format {}

    # Check the count of args

    if { [llength $args] < 1 || [llength $args] % 2 != 1 } {
	return -code error \
	    -errorcode [list CLOCK wrongNumArgs] \
	    "wrong \# args: should be\
             \"[lindex [info level 0] 0] string\
             ?-base seconds?\
             ?-format string? ?-gmt boolean?\
             ?-locale LOCALE? ?-timezone ZONE?\""
    }

    # Set defaults

    set base [clock seconds]
    set string [lindex $args 0]
    set format {}
    set gmt 0
    set locale C
    set timezone [GetSystemTimeZone]

    # Pick up command line options.

    foreach { flag value } [lreplace $args 0 0] {
	set saw($flag) {}
	switch -exact -- $flag {
	    -base {
		set base $value
	    }
	    -format {
		set format $value
	    }
	    -gmt {
		set gmt $value
	    }
	    -locale {
		set locale $value
	    }
	    -timezone {
		set timezone $value
	    }
	    default {
		return -code error \
		    -errorcode [list CLOCK badSwitch $flag] \
		    "bad switch \"$flag\",\
                     must be -base, -format, -gmt, -locale or -timezone"
	    }
	}
    }

    # Check options for validity

    if { [info exists saw(-gmt)] && [info exists saw(-timezone)] } {
	return -code error \
	    -errorcode [list CLOCK gmtWithTimezone] \
	    "cannot use -gmt and -timezone in same call"
    }
    if { [catch { expr { wide($base) } } result] } {
	return -code error \
	    "expected integer but got \"$base\"" 
    }
    if { ![string is boolean $gmt] } {
	return -code error \
	    "expected boolean value but got \"$gmt\""
    } else {
	if { $gmt } {
	    set timezone :GMT
	}
    }

    if { ![info exists saw(-format)] } {
	if { [info exists saw(-timezone)] || [info exists saw(-locale)] } {
	    return -code error \
		-errorcode [list CLOCK flagWithLegacyFormat] \
		"legacy \[clock scan\] does not support -timezone or\
                 -locale"

	}
	if { [catch {
	    Oldscan $string -base $base -gmt $gmt
	} retval] } {
	    return -code error \
		-errorcode $::errorCode -errorinfo $::errorInfo \
		$retval
	} else {
	    return $retval
	}
    }

    # Change locale if a fresh locale has been given on the command line.

    EnterLocale $locale oldLocale

    set status [catch {

	# Map away the locale-dependent composite format groups

	set format [LocalizeFormat $format]
	set scanner [ParseClockScanFormat $format]
	$scanner $string $base $timezone

    } result]

    # Restore the locale

    if { [info exists oldLocale] } {
	mclocale $oldLocale
    }

    if { $status == 1 } {
	if { [lindex $::errorCode 0] eq {clock} } {
	    return -code error $result
	} else {
	    return -code error \
		-errorcode $::errorCode -errorinfo $::errorInfo \
		$result
	}
    } else {
	return $result
    }

}

#----------------------------------------------------------------------
#
# ParseClockScanFormat --
#
#	Parses a format string given to [clock scan -format]
#
# Parameters:
#	None.
#
# Results:
#	Constructs and returns a procedure that accepts the
#	string being scanned, the base time, and the time zone.  
#	The procedure will either return the scanned time or
#	else throw an error that should be rethrown to the caller
#	of [clock scan]
#
# Side effects:
#	The given procedure is defined in the ::tcl::clock
#	namespace.  Scan procedures are not deleted once installed.
#
# Why do we parse dates by defining a procedure to parse them?
# The reason is that by doing so, we have one convenient place to
# cache all the information: the regular expressions that match the
# patterns (which will be compiled), the code that assembles the
# date information, everything lands in one place.  In this way,
# when a given format is reused at run time, all the information
# of how to apply it is available in a single place.
#
#----------------------------------------------------------------------

proc ::tcl::clock::ParseClockScanFormat { formatString } {

    variable DateParseActions
    variable TimeParseActions

    # Condense whitespace

    regsub -all {[[:space:]]+} $formatString { } formatString

    # Check whether the format has been parsed previously, and return
    # the existing recognizer if it has.

    set procName [namespace current]::scanproc'$formatString'[mclocale]
    if { [info procs $procName] != {} } {
	return $procName
    }

    # Walk through the groups of the format string.  In this loop, we
    # accumulate:
    #	- a regular expression that matches the string,
    #   - the count of capturing brackets in the regexp
    #   - a set of code that post-processes the fields captured by the regexp,
    #   - a dictionary whose keys are the names of fields that are present
    #     in the format string.

    set re {^[[:space:]]*}
    set captureCount 0
    set postcode {}
    set fieldSet [dict create]
    set fieldCount 0
    set postSep {}
    set state {}

    foreach c [split $formatString {}] {
	switch -exact -- $state {
	    {} {
		if { $c eq "%" } {
		    set state %
		} elseif { $c eq " " } {
		    append re {[[:space:]]+}
		} else {
		    if { ! [string is alnum $c] } {
			append re \\
			}
		    append re $c
		}
	    }
	    % {
		set state {}
		switch -exact -- $c {
		    % {
			append re %
		    }
		    { } {
			append re "\[\[:space:\]\]*"
		    }
		    a - A { 		# Day of week, in words
			set l {}
			foreach \
			    i {7 1 2 3 4 5 6} \
			    abr [mc DAYS_OF_WEEK_ABBREV] \
			    full [mc DAYS_OF_WEEK_FULL] {
				dict set l $abr $i
				dict set l $full $i
				incr i
			    }
			foreach { regex lookup } [UniquePrefixRegexp $l] break
			append re ( $regex )
			dict set fieldSet dayOfWeek [incr fieldCount]
			append postcode "dict set date dayOfWeek \[" \
			    "dict get " [list $lookup] " \$field" \
			    [incr captureCount] \
			    "\]\n"
		    }
		    b - B - h {		# Name of month
			set i 0
			set l {}
			foreach \
			    abr [mc MONTHS_ABBREV] \
			    full [mc MONTHS_FULL] {
				incr i
				dict set l $abr $i
				dict set l $full $i
			    }
			foreach { regex lookup } [UniquePrefixRegexp $l] break
			append re ( $regex )
			dict set fieldSet month [incr fieldCount]
			append postcode "dict set date month \[" \
			    "dict get " [list $lookup] " \$field" \
			    [incr captureCount] \
			    "\]\n"
		    }
		    C {			# Gregorian century
			append re \\s*(\\d\\d?)
			dict set fieldSet century [incr fieldCount]
			append postcode "dict set date century \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    d - e {		# Day of month
			append re \\s*(\\d\\d?)
			dict set fieldSet dayOfMonth [incr fieldCount]
			append postcode "dict set date dayOfMonth \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    E {			# Prefix for locale-specific codes
			set state %E
		    }
		    g {			# ISO8601 2-digit year
			append re \\s*(\\d\\d)
			dict set fieldSet iso8601YearOfCentury \
			    [incr fieldCount]
			append postcode \
			    "dict set date iso8601YearOfCentury \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    G {			# ISO8601 4-digit year
			append re \\s*(\\d\\d)(\\d\\d)
			dict set fieldSet iso8601Century [incr fieldCount]
			dict set fieldSet iso8601YearOfCentury \
			    [incr fieldCount]
			append postcode \
			    "dict set date iso8601Century \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n" \
			    "dict set date iso8601YearOfCentury \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    H - k {		# Hour of day
			append re \\s*(\\d\\d?)
			dict set fieldSet hour [incr fieldCount]
			append postcode "dict set date hour \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    I - l {		# Hour, AM/PM
			append re \\s*(\\d\\d?)
			dict set fieldSet hourAMPM [incr fieldCount]
			append postcode "dict set date hourAMPM \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    j {			# Day of year
			append re \\s*(\\d\\d?\\d?)
			dict set fieldSet dayOfYear [incr fieldCount]
			append postcode "dict set date dayOfYear \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    J {			# Julian Day Number
			append re \\s*(\\d+)
			dict set fieldSet julianDay [incr fieldCount]
			append postcode "dict set date julianDay \[" \
			    "::scan \$field" [incr captureCount] " %ld" \
			    "\]\n"
		    }
		    m - N {			# Month number
			append re \\s*(\\d\\d?)
			dict set fieldSet month [incr fieldCount]
			append postcode "dict set date month \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    M {			# Minute
			append re \\s*(\\d\\d?)
			dict set fieldSet minute [incr fieldCount]
			append postcode "dict set date minute \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    n {			# Literal newline
			append re \\n
		    }
		    O {			# Prefix for locale numerics
			set state %O
		    }
		    p - P { 		# AM/PM indicator
			set l [list [mc AM] 0 [mc PM] 1]
			foreach { regex lookup } [UniquePrefixRegexp $l] break
			append re ( $regex )
			dict set fieldSet amPmIndicator [incr fieldCount]
			append postcode "dict set date amPmIndicator \[" \
			    "dict get " [list $lookup] " \[string tolower " \
			    "\$field" \
			    [incr captureCount] \
			    "\]\]\n"
		    }
		    Q {			# Hi, Jeff!
			append re {Stardate\s+([-+]?\d+)(\d\d\d)[.](\d)}
			incr captureCount
			dict set fieldSet seconds [incr fieldCount]
			append postcode {dict set date seconds } \[ \
			    {ParseStarDate $field} [incr captureCount] \
			    { $field} [incr captureCount] \
			    { $field} [incr captureCount] \
			    \] \n
		    }
		    s {			# Seconds from Posix Epoch
			# This next case is insanely difficult,
			# because it's problematic to determine
			# whether the field is actually within
			# the range of a wide integer.
			append re {\s*([-+]?\d+)}
			dict set fieldSet seconds [incr fieldCount]
			append postcode {dict set date seconds } \[ \
			    {ScanWide $field} [incr captureCount] \] \n
		    }
		    S {			# Second
			append re \\s*(\\d\\d?)
			dict set fieldSet second [incr fieldCount]
			append postcode "dict set date second \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    t {			# Literal tab character
			append re \\t
		    }
		    u - w {		# Day number within week, 0 or 7 == Sun
					# 1=Mon, 6=Sat
			append re \\s*(\\d)
			dict set fieldSet dayOfWeek [incr fieldCount]
			append postcode {::scan $field} [incr captureCount] \
			    { %d dow} \n \
			    {
				if { $dow == 0 } {
				    set dow 7
				} elseif { $dow > 7 } {
				    return -code error \
					-errorcode [list CLOCK badDayOfWeek] \
					"day of week is greater than 7"
				}
				dict set date dayOfWeek $dow
			    }
		    }
		    U {			# Week of year. The
					# first Sunday of the year is the
					# first day of week 01. No scan rule
					# uses this group.
			append re \\s*\\d\\d?
		    }
		    V {			# Week of ISO8601 year
			
			append re \\s*(\\d\\d?)
			dict set fieldSet iso8601Week [incr fieldCount]
			append postcode "dict set date iso8601Week \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    W {			# Week of the year (00-53). The first
					# Monday of the year is the first day
					# of week 01. No scan rule uses this
					# group.
			append re \\s*\\d\\d?
		    }
		    y {			# Two-digit Gregorian year
			append re \\s*(\\d\\d?)
			dict set fieldSet yearOfCentury [incr fieldCount]
			append postcode "dict set date yearOfCentury \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    Y {			# 4-digit Gregorian year
			append re \\s*(\\d\\d)(\\d\\d)
			dict set fieldSet century [incr fieldCount]
			dict set fieldSet yearOfCentury [incr fieldCount]
			append postcode \
			    "dict set date century \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n" \
			    "dict set date yearOfCentury \[" \
			    "::scan \$field" [incr captureCount] " %d" \
			    "\]\n"
		    }
		    z - Z {			# Time zone name
			append re {(?:([-+]\d\d:?\d\d(?::?\d\d)?)|([[:alnum:]]{1,4}))}
			dict set fieldSet tzName [incr fieldCount]
			append postcode \
			    {if } \{ { $field} [incr captureCount] \
			    { ne "" } \} { } \{ \n \
			    {dict set date tzName $field} \
			    $captureCount \n \
			    \} { else } \{ \n \
			    {dict set date tzName } \[ \
			    {ConvertLegacyTimeZone $field} \
			    [incr captureCount] \] \n \
			    \} \n \
		    }
		    % {			# Literal percent character
			append re %
		    }
		    default {
			append re %
			if { ! [string is alnum $c] } {
			    append re \\
			    }
			append re $c
		    }
		}
	    }
	    %E {
		switch -exact -- $c {
		    C {			# Locale-dependent era
			set d {}
			foreach triple [mc LOCALE_ERAS] {
			    foreach {t symbol year} $triple break
			    dict set d $symbol $year
			}
			foreach { regex lookup } [UniquePrefixRegexp $d] break
			append re (?: $regex )
		        
		    }
		    y {			# Locale-dependent year of the era
			foreach {regex lookup} [LocaleNumeralMatcher] break
			append re $regex
			incr fieldCount
		    }
		    default {
			append re %E
			if { ! [string is alnum $c] } {
			    append re \\
			    }
			append re $c
		    }
		}
		set state {}
	    }
	    %O {
		switch -exact -- $c {
		    d - e {
			foreach {regex lookup} [LocaleNumeralMatcher] break
			append re $regex
			dict set fieldSet dayOfMonth [incr fieldCount]
			append postcode "dict set date dayOfMonth \[" \
			    "dict get " [list $lookup] " \$field" \
			    [incr captureCount] \
			    "\]\n"
		    }
		    H - k {
			foreach {regex lookup} [LocaleNumeralMatcher] break
			append re $regex
			dict set fieldSet hour [incr fieldCount]
			append postcode "dict set date hour \[" \
			    "dict get " [list $lookup] " \$field" \
			    [incr captureCount] \
			    "\]\n"
		    }
		    I - l {
			foreach {regex lookup} [LocaleNumeralMatcher] break
			append re $regex
			dict set fieldSet hourAMPM [incr fieldCount]
			append postcode "dict set date hourAMPM \[" \
			    "dict get " [list $lookup] " \$field" \
			    [incr captureCount] \
			    "\]\n"
		    }
		    m {
			foreach {regex lookup} [LocaleNumeralMatcher] break
			append re $regex
			dict set fieldSet month [incr fieldCount]
			append postcode "dict set date month \[" \
			    "dict get " [list $lookup] " \$field" \
			    [incr captureCount] \
			    "\]\n"
		    }
		    M {
			foreach {regex lookup} [LocaleNumeralMatcher] break
			append re $regex
			dict set fieldSet minute [incr fieldCount]
			append postcode "dict set date minute \[" \
			    "dict get " [list $lookup] " \$field" \
			    [incr captureCount] \
			    "\]\n"
		    }
		    S {
			foreach {regex lookup} [LocaleNumeralMatcher] break
			append re $regex
			dict set fieldSet second [incr fieldCount]
			append postcode "dict set date second \[" \
			    "dict get " [list $lookup] " \$field" \
			    [incr captureCount] \
			    "\]\n"
		    }
		    u - w {
			foreach {regex lookup} [LocaleNumeralMatcher] break
			append re $regex
			dict set fieldSet dayOfWeek [incr fieldCount]
			append postcode "set dow \[dict get " [list $lookup] \
			    { $field} [incr captureCount] \] \n \
			    {
				if { $dow == 0 } {
				    set dow 7
				} elseif { $dow > 7 } {
				    return -code error \
					-errorcode [list CLOCK badDayOfWeek] \
					"day of week is greater than 7"
				}
				dict set date dayOfWeek $dow
			    }				
		    }
		    y {
			foreach {regex lookup} [LocaleNumeralMatcher] break
			append re $regex
			dict set fieldSet yearOfCentury [incr fieldCount]
			append postcode {dict set date yearOfCentury } \[ \
			    {dict get } [list $lookup] { $field} \
			    [incr captureCount] \] \n
		    }
		    default {
			append re %O
			if { ! [string is alnum $c] } {
			    append re \\
			    }
			append re $c
		    }
		}
		set state {}
	    }
	}
    }

    # Clean up any unfinished format groups

    append re $state \\s*\$

    # Build the procedure

    set procBody \n
    append procBody "if \{ !\[ regexp -nocase [list $re] \$string ->"
    for { set i 1 } { $i <= $captureCount } { incr i } {
	append procBody " " field $i
    }
    append procBody "\] \} \{" \n
    append procBody {
	return -code error -errorcode [list CLOCK badInputString] \
	    {input string does not match supplied format}
    }
    append procBody \}\n
    append procBody "set date \[dict create\]" \n
    append procBody {dict set date tzName $timeZone} \n
    append procBody $postcode

    # Add code that gets Julian Day Number from the fields.

    append procBody [MakeParseCodeFromFields $fieldSet $DateParseActions]

    # Get time of day

    append procBody [MakeParseCodeFromFields $fieldSet $TimeParseActions]

    # Assemble seconds, and convert local nominal time to UTC.

    if { ![dict exists $fieldSet seconds] 
         && ![dict exists $fieldSet starDate] } {
	append procBody {
	    if { [dict get $date julianDay] > $::tcl::clock::JD31Dec9999 } {
		return -code error -errorcode [list CLOCK dateTooLarge] \
		    "requested date too large to represent"
	    }
	    dict set date localSeconds \
		[expr { -$::tcl::clock::PosixEpochAsJulianSeconds
			+ ( $::tcl::clock::SecondsPerDay 
			    * wide([dict get $date julianDay]) )
			+ [dict get $date secondOfDay] }]
	}
    }

    if { ![dict exists $fieldSet seconds] 
	 && ![dict exists $fieldSet starDate] } {
	append procBody {
	    set date [::tcl::clock::ConvertLocalToUTC [K $date [set date {}]]]
	}
    }

    # Return result

    append procBody {return [dict get $date seconds]} \n

    proc $procName { string baseTime timeZone } $procBody

    # puts [list proc $procName [list string baseTime timeZone] $procBody]

    return $procName
}
	
#----------------------------------------------------------------------
#
# LocaleNumeralMatcher --
#
#	Composes a regexp that captures the numerals in the given
#	locale, and a dictionary to map them to conventional numerals.
#
# Parameters:
#	none.
#
# Results:
#	Returns a two-element list comprising the regexp and the
#	dictionary.
#
# Side effects:
#	Caches the result.
#
#----------------------------------------------------------------------

proc ::tcl::clock::LocaleNumeralMatcher {} {

    variable LocaleNumeralCache

    set l [mclocale]
    if { ![dict exists $LocaleNumeralCache $l] } {
	set d {}
	set i 0
	set sep \(
	foreach n [mc LOCALE_NUMERALS] {
	    dict set d $n $i
	    regsub -all {[^[:alnum:]]} $n \\\\& subex
	    append re $sep $subex
	    set sep |
	    incr i
	}
	append re \)
	dict set LocaleNumeralCache $l [list $re $d]
    }
    return [dict get $LocaleNumeralCache $l]
}
	


#----------------------------------------------------------------------
#
# UniquePrefixRegexp --
#
#	Composes a regexp that performs unique-prefix matching.  The
#	RE matches one of a supplied set of strings, or any unique
#	prefix thereof.
#
# Parameters:
#	data - List of alternating match-strings and values.
#	       Match-strings with distinct values are considered
#	       distinct.
#
# Results:
#	Returns a two-element list.  The first is a regexp that
#	matches any unique prefix of any of the strings.  The second
#	is a dictionary whose keys are match values from the regexp
#	and whose values are the corresponding values from 'data'.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::UniquePrefixRegexp { data } {

    # The 'successors' dictionary will contain, for each string that
    # is a prefix of any key, all characters that may follow that
    # prefix.  The 'prefixMapping' dictionary will have keys that
    # are prefixes of keys and values that correspond to the keys.

    set prefixMapping [dict create]
    set successors [dict create {} {}]

    # Walk the key-value pairs

    foreach { key value } $data {

	# Construct all prefixes of the key; 

	set prefix {}
	foreach char [split $key {}] {
	    set oldPrefix $prefix
	    dict set successors $oldPrefix $char {}
	    append prefix $char

	    # Put the prefixes in the 'prefixMapping' and 'successors'
	    # dictionaries

	    dict lappend prefixMapping $prefix $value
	    if { ![dict exists $successors $prefix] } {
		dict set successors $prefix {}
	    }
	}
    }

    # Identify those prefixes that designate unique values, and
    # those that are the full keys

    set uniquePrefixMapping {}
    dict for { key valueList } $prefixMapping {
	if { [llength $valueList] == 1 } {
	    dict set uniquePrefixMapping $key [lindex $valueList 0]
	}
    }
    foreach { key value } $data {
	dict set uniquePrefixMapping $key $value
    }

    # Construct the re.

    return [list \
		[MakeUniquePrefixRegexp $successors $uniquePrefixMapping {}] \
		$uniquePrefixMapping]
}

#----------------------------------------------------------------------
#
# MakeUniquePrefixRegexp --
#
#	Service procedure for 'UniquePrefixRegexp' that constructs
#	a regular expresison that matches the unique prefixes.
#
# Parameters:
#	successors - Dictionary whose keys are all prefixes
#		     of keys passed to 'UniquePrefixRegexp' and whose
#		     values are dictionaries whose keys are the characters
#		     that may follow those prefixes.
#	uniquePrefixMapping - Dictionary whose keys are the unique
#			      prefixes and whose values are not examined.
#	prefixString - Current prefix being processed.
#
# Results:
#	Returns a constructed regular expression that matches the set
#	of unique prefixes beginning with the 'prefixString'.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::MakeUniquePrefixRegexp { successors 
					  uniquePrefixMapping
					  prefixString } {

    # Get the characters that may follow the current prefix string

    set schars [lsort -ascii [dict keys [dict get $successors $prefixString]]]
    if { [llength $schars] == 0 } {
	return {}
    }

    # If there is more than one successor character, or if the current
    # prefix is a unique prefix, surround the generated re with non-capturing
    # parentheses.

    set re {}
    if { [dict exists $uniquePrefixMapping $prefixString]
	 || [llength $schars] > 1 } {
	append re "(?:"
    }

    # Generate a regexp that matches the successors.

    set sep ""
    foreach { c } $schars {
	set nextPrefix $prefixString$c
	regsub -all {[^[:alnum:]]} $c \\\\& rechar
	append re $sep $rechar \
	    [MakeUniquePrefixRegexp \
		 $successors $uniquePrefixMapping $nextPrefix]
	set sep |
    }

    # If the current prefix is a unique prefix, make all following text
    # optional. Otherwise, if there is more than one successor character,
    # close the non-capturing parentheses.

    if { [dict exists $uniquePrefixMapping $prefixString] } {
	append re ")?"
    }  elseif { [llength $schars] > 1 } {
	append re ")"
    }

    return $re
}

#----------------------------------------------------------------------
#
# MakeParseCodeFromFields --
#
#	Composes Tcl code to extract the Julian Day Number from a
#	dictionary containing date fields.
#
# Parameters:
#	dateFields -- Dictionary whose keys are fields of the date,
#	              and whose values are the rightmost positions
#		      at which those fields appear.
#	parseActions -- List of triples: field set, priority, and
#			code to emit.  Smaller priorities are better, and
#			the list must be in ascending order by priority
#
# Results:
#	Returns a burst of code that extracts the day number from the
#	given date.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::MakeParseCodeFromFields { dateFields parseActions } {

    set currPrio 999
    set currFieldPos [list]
    set currCodeBurst {
	error "in ::tcl::clock::MakeParseCodeFromFields: can't happen"
    }

    foreach { fieldSet prio parseAction } $parseActions {

	# If we've found an answer that's better than any that follow,
	# quit now.

	if { $prio > $currPrio } {
	    break
	}

	# Accumulate the field positions that are used in the current
	# field grouping.

	set fieldPos [list]
	set ok true
	foreach field $fieldSet {
	    if { ! [dict exists $dateFields $field] } {
		set ok 0
		break
	    }
	    lappend fieldPos [dict get $dateFields $field]
	}

	# Quit if we don't have a complete set of fields
	if { !$ok } {
	    continue
	}

	# Determine whether the current answer is better than the last.

	set fPos [lsort -integer -decreasing $fieldPos]

	if { $prio ==  $currPrio } {
	    foreach currPos $currFieldPos newPos $fPos {
		if { ![string is integer $newPos]
		     || ![string is integer $currPos]
		     || $newPos > $currPos } {
		    break
		}
		if { $newPos < $currPos } {
		    set ok 0
		    break
		}
	    }
	}
	if { !$ok } {
	    continue
	}

	# Remember the best possibility for extracting date information

	set currPrio $prio
	set currFieldPos $fPos
	set currCodeBurst $parseAction
	    
    }

    return $currCodeBurst

}

#----------------------------------------------------------------------
#
# EnterLocale --
#
#	Switch [mclocale] to a given locale if necessary
#
# Parameters:
#	locale -- Desired locale
#	oldLocaleVar -- Name of a variable in caller's scope that
#		        tracks the previous locale name.
#
# Results:
#	Returns the locale that was previously current.
#
# Side effects:
#	Does [mclocale].  If necessary, uses [mcload] to load the
#	designated locale's files, and tracks that it has done so
#	in the 'McLoaded' variable.
#
#----------------------------------------------------------------------

proc ::tcl::clock::EnterLocale { locale oldLocaleVar } {

    upvar 1 $oldLocaleVar oldLocale

    variable MsgDir
    variable McLoaded

    set oldLocale [mclocale]
    if { $locale eq {system} } {

	if { $::tcl_platform(platform) ne {windows} } {

	    # On a non-windows platform, the 'system' locale is
	    # the same as the 'current' locale

	    set locale current
	} else {

	    # On a windows platform, the 'system' locale is
	    # adapted from the 'current' locale by applying the
	    # date and time formats from the Control Panel.
	    # First, load the 'current' locale if it's not yet loaded

	    if {![dict exists $McLoaded $oldLocale] } {
		mcload $MsgDir
		dict set McLoaded $oldLocale {}
	    }

	    # Make a new locale string for the system locale, and
	    # get the Control Panel information

	    set locale ${oldLocale}_windows
	    if { ![dict exists $McLoaded $locale] } {
		LoadWindowsDateTimeFormats $locale
		dict set mcloaded $locale {}
	    }
	}
    }
    if { $locale eq {current}} {
	set locale $oldLocale
	unset oldLocale
    } elseif { $locale eq $oldLocale } {
	unset oldLocale
    } else {
	mclocale $locale
    }
    if { ![dict exists $McLoaded $locale] } {
	mcload $MsgDir
	dict set McLoaded $locale {}
    }

}    

#----------------------------------------------------------------------
#
# LoadWindowsDateTimeFormats --
#
#	Load the date/time formats from the Control Panel in Windows
#	and convert them so that they're usable by Tcl.
#
# Parameters:
#	locale - Name of the locale in whose message catalog
#	         the converted formats are to be stored.
#
# Results:
#	None.
#
# Side effects:
#	Updates the given message catalog with the locale strings.
#
# Presumes that on entry, [mclocale] is set to the current locale,
# so that default strings can be obtained if the Registry query
# fails.
#
#----------------------------------------------------------------------

proc ::tcl::clock::LoadWindowsDateTimeFormats { locale } {

    if { ![catch {
	registry get "HKEY_CURRENT_USER\\Control Panel\\International" \
	    sShortDate
    } string] } {
	set quote {}
	set datefmt {}
	foreach { unquoted quoted } [split $string '] {
	    append datefmt $quote [string map {
		dddd %A
		ddd  %a
		dd   %d
		d    %e
		MMMM %B
		MMM  %b
		MM   %m
		M    %N
		yyyy %Y
		yy   %y
                y    %y
                gg   {}
	    } $unquoted]
	    if { $quoted eq {} } {
		set quote '
	    } else {
		set quote $quoted
	    }
	}
	::msgcat::mcset $locale DATE_FORMAT $datefmt
    }

    if { ![catch {
	registry get "HKEY_CURRENT_USER\\Control Panel\\International" \
	    sLongDate
    } string] } {
	set quote {}
	set ldatefmt {}
	foreach { unquoted quoted } [split $string '] {
	    append ldatefmt $quote [string map {
		dddd %A
		ddd  %a
		dd   %d
		d    %e
		MMMM %B
		MMM  %b
		MM   %m
		M    %N
		yyyy %Y
		yy   %y
                y    %y
                gg   {}
	    } $unquoted]
	    if { $quoted eq {} } {
		set quote '
	    } else {
		set quote $quoted
	    }
	}
	::msgcat::mcset $locale LOCALE_DATE_FORMAT $ldatefmt
    }

    if { ![catch {
	registry get "HKEY_CURRENT_USER\\Control Panel\\International" \
	    sTimeFormat
    } string] } {
	set quote {}
	set timefmt {}
	foreach { unquoted quoted } [split $string '] {
	    append timefmt $quote [string map {
		HH    %H
		H     %k
		hh    %I
		h     %l
		mm    %M
		m     %M
		ss    %S
		s     %S
		tt    %p
		t     %p
	    } $unquoted]
	    if { $quoted eq {} } {
		set quote '
	    } else {
		set quote $quoted
	    }
	}
	::msgcat::mcset $locale TIME_FORMAT $timefmt
    }

    catch {
	::msgcat::mcset $locale DATE_TIME_FORMAT "$datefmt $timefmt"
    }
    catch {
	::msgcat::mcset $locale LOCALE_DATE_TIME_FORMAT "$ldatefmt $timefmt"
    }

    return

}

#----------------------------------------------------------------------
#
# LocalizeFormat --
#
#	Map away locale-dependent format groups in a clock format.
#
# Parameters:
#	format -- Format supplied to [clock scan] or [clock format]
#
# Results:
#	Returns the string with locale-dependent composite format
#	groups substituted out.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::LocalizeFormat { format } {

    # Handle locale-dependent format groups by mapping them out of
    # the input string.  Note that the order of the [string map]
    # operations is significant because earlier formats can refer
    # to later ones; for example %c can refer to %X, which in turn
    # can refer to %T.
    
    set format [string map [list %c [mc DATE_TIME_FORMAT] \
				%Ec [mc LOCALE_DATE_TIME_FORMAT]] $format]
    set format [string map [list %x [mc DATE_FORMAT] \
				%Ex [mc LOCALE_DATE_FORMAT] \
				%X [mc TIME_FORMAT] \
				%EX [mc LOCALE_TIME_FORMAT]] $format]
    set format [string map [list %r [mc TIME_FORMAT_12] \
				%R [mc TIME_FORMAT_24] \
				%T [mc TIME_FORMAT_24_SECS]] $format]
    set format [string map [list %D %m/%d/%Y \
				%EY [mc LOCALE_YEAR_FORMAT]\
				%+ {%a %b %e %H:%M:%S %Z %Y}] $format]
    return $format
}

#----------------------------------------------------------------------
#
# FormatStarDate --
#
#	Formats a date as a StarDate.
#
# Parameters:
#	date - Dictionary containing 'year', 'dayOfYear', and
#	       'localSeconds' fields.
#
# Results:
#	Returns the given date formatted as a StarDate.
#
# Side effects:
#	None.
#
# Jeff Hobbs put this in to support an atrocious pun about Tcl being
# "Enterprise ready."  Now we're stuck with it.
#
#----------------------------------------------------------------------

proc ::tcl::clock::FormatStarDate { date } {

    variable DaysPerYear
    variable SecondsPerDay
    variable Roddenberry

    # Get day of year, zero based

    set doy [expr { [dict get $date dayOfYear] - 1 }]

    # Determine whether the year is a leap year

    if { [dict get $date gregorian] } {
	set lp [IsGregorianLeapYear $date]
    } else {
	set lp [expr { [dict get $date year] % 4 == 0  }]
    }

    # Convert day of year to a fractional year

    if { $lp } {
	set fractYear [expr { 1000 * $doy / ( $DaysPerYear + 1 ) }]
    } else {
	set fractYear [expr { 1000 * $doy / $DaysPerYear }]
    }

    # Put together the StarDate

    return [::format "Stardate %02d%03d.%1d" \
		[expr { [dict get $date year] - $Roddenberry }] \
		$fractYear \
		[expr { [dict get $date localSeconds] % $SecondsPerDay
			/ ( $SecondsPerDay / 10 ) }]]
}

#----------------------------------------------------------------------
#
# ParseStarDate --
#
#	Parses a StarDate
#
# Parameters:
#	year - Year from the Roddenberry epoch
#	fractYear - Fraction of a year specifiying the day of year.
#	fractDay - Fraction of a day
#
# Results:
#	Returns a count of seconds from the Posix epoch.
#
# Side effects:
#	None.
#
# Jeff Hobbs put this in to support an atrocious pun about Tcl being
# "Enterprise ready."  Now we're stuck with it.
#
#----------------------------------------------------------------------

proc ::tcl::clock::ParseStarDate { year fractYear fractDay } {

    variable Roddenberry
    variable DaysPerYear
    variable SecondsPerDay
    variable PosixEpochAsJulianSeconds

    # Build a tentative date from year and fraction.

    set date [dict create \
		  era CE \
		  year [expr { $year + $Roddenberry }] \
		  dayOfYear [expr { $fractYear * $DaysPerYear / 1000 + 1 }]]
    set date [GetJulianDayFromGregorianEraYearDay [K $date [set date {}]]]

    # Determine whether the given year is a leap year

    if { [dict get $date gregorian] } {
	set lp [IsGregorianLeapYear $date]
    } else {
	set lp [expr { [dict get $date year] % 4 == 0  }]
    }

    # Reconvert the fractional year according to whether the given
    # year is a leap year

    if { $lp } {
	dict set date dayOfYear \
	    [expr { $fractYear * ( $DaysPerYear + 1 ) / 1000 + 1 }]
    } else {
	dict set date dayOfYear \
	    [expr { $fractYear * $DaysPerYear / 1000 + 1 }]
    }
    dict unset date julianDay
    dict unset date gregorian
    set date [GetJulianDayFromGregorianEraYearDay [K $date [set date {}]]]

    return [expr { $SecondsPerDay * [dict get $date julianDay]
		   - $PosixEpochAsJulianSeconds
		   + ( $SecondsPerDay / 10 ) * $fractDay }]

}

#----------------------------------------------------------------------
#
# ScanWide --
#
#	Scans a wide integer from an input
#
# Parameters:
#	str - String containing a decimal wide integer
#
# Results:
#	Returns the string as a pure wide integer.  Throws an error if
#	the string is misformatted or out of range.
#
#----------------------------------------------------------------------

proc ::tcl::clock::ScanWide { str } {
    set count [::scan $str {%ld %c} result junk]
    if { $count != 1 } {
	return -code error -errorcode [list CLOCK notAnInteger $str] \
	    "\"$str\" is not an integer"
    }
    if { [incr result 0] != $str } {
	return -code error -errorcode [list CLOCK integervalueTooLarge] \
	    "integer value too large to represent"
    }
    return $result
}

#----------------------------------------------------------------------
#
# InterpretTwoDigitYear --
#
#	Given a date that contains only the year of the century,
#	determines the target value of a two-digit year.
#
# Parameters:
#	date - Dictionary containing fields of the date.
#	baseTime - Base time relative to which the date is expressed.
#	twoDigitField - Name of the field that stores the two-digit year.
#			Default is 'yearOfCentury'
#	fourDigitField - Name of the field that will receive the four-digit
#	                 year.  Default is 'year'
#
# Results:
#	Returns the dictionary augmented with the four-digit year, stored in
#	the given key.
#
# Side effects:
#	None.
#
# The current rule for interpreting a two-digit year is that the year
# shall be between 1937 and 2037, thus staying within the range of a
# 32-bit signed value for time.  This rule may change to a sliding
# window in future versions, so the 'baseTime' parameter (which is
# currently ignored) is provided in the procedure signature.
#
#----------------------------------------------------------------------

proc ::tcl::clock::InterpretTwoDigitYear { date baseTime 
					   { twoDigitField yearOfCentury }
					   { fourDigitField year } } {

    set yr [dict get $date $twoDigitField]
    if { $yr <= 37 } {
	dict set date $fourDigitField [expr { $yr + 2000 }]
    } else {
	dict set date $fourDigitField [expr { $yr + 1900 }]
    }
    return $date

}

#----------------------------------------------------------------------
#
# AssignBaseYear --
#
#	Places the number of the current year into a dictionary.
#
# Parameters:
#	date - Dictionary value to update
#	baseTime - Base time from which to extract the year, expressed
#		   in seconds from the Posix epoch
#
# Results:
#	Returns the dictionary with the current year assigned.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::AssignBaseYear { date baseTime timeZone } {

    variable PosixEpochAsJulianSeconds
    variable SecondsPerDay

    # Find the Julian Day Number corresponding to the base time, and
    # find the Gregorian year corresponding to that Julian Day.

    set date2 [dict create seconds $baseTime]
    set date2 [ConvertUTCToLocal [K $date2 [set date2 {}]] $timeZone]
    set date2 [GetJulianDay [K $date2 [set date2 {}]]]
    set date2 [GetGregorianEraYearDay [K $date2 [set date2 {}]]]

    # Store the converted year

    dict set date era [dict get $date2 era]
    dict set date year [dict get $date2 year]

    return $date

}

#----------------------------------------------------------------------
#
# AssignBaseIso8601Year --
#
#	Determines the base year in the ISO8601 fiscal calendar.
#
# Parameters:
#	date - Dictionary containing the fields of the date that
#	       is to be augmented with the base year.
#	baseTime - Base time expressed in seconds from the Posix epoch.
#
# Results:
#	Returns the given date with "iso8601Year" set to the
#	base year.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::AssignBaseIso8601Year { date baseTime timeZone } {
    variable PosixEpochAsJulianSeconds
    variable SecondsPerDay

    # Find the Julian Day Number corresponding to the base time

    set date2 [dict create seconds $baseTime]
    set date2 [ConvertUTCToLocal [K $date2 [set date2 {}]] $timeZone]
    set date2 [GetJulianDay [K $date2 [set date2 {}]]]

    # Calculate the ISO8601 date and transfer the year

    set date2 [GetYearWeekDay [K $date2 [set date2 {}]]]
    dict set date era CE
    dict set date iso8601Year [dict get $date2 iso8601Year]
    return $date
}

#----------------------------------------------------------------------
#
# AssignBaseMonth --
#
#	Places the number of the current year and month into a 
#	dictionary.
#
# Parameters:
#	date - Dictionary value to update
#	baseTime - Time from which the year and month are to be
#	           obtained, expressed in seconds from the Posix epoch.
#
# Results:
#	Returns the dictionary with the base year and month assigned.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::AssignBaseMonth { date baseTime timeZone } {

    variable PosixEpochAsJulianSeconds
    variable SecondsPerDay

    # Find the Julian Day Number corresponding to the base time

    set date2 [dict create seconds $baseTime]
    set date2 [ConvertUTCToLocal [K $date2 [set date2 {}]] $timeZone]
    set date2 [GetJulianDay [K $date2 [set date2 {}]]]

    # Find the Gregorian year corresponding to that Julian Day

    set date2 [GetGregorianEraYearDay [K $date2 [set date2 {}]]]
    set date2 [GetMonthDay [K $date2 [set date2 {}]]]
    dict set date era [dict get $date2 era]
    dict set date year [dict get $date2 year]
    dict set date month [dict get $date2 month]
    return $date

}

#----------------------------------------------------------------------
#
# AssignBaseWeek --
#
#	Determines the base year and week in the ISO8601 fiscal calendar.
#
# Parameters:
#	date - Dictionary containing the fields of the date that
#	       is to be augmented with the base year and week.
#	baseTime - Base time expressed in seconds from the Posix epoch.
#
# Results:
#	Returns the given date with "iso8601Year" set to the
#	base year and "iso8601Week" to the week number.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::AssignBaseWeek { date baseTime timeZone } {
    variable PosixEpochAsJulianSeconds
    variable SecondsPerDay

    # Find the Julian Day Number corresponding to the base time

    set date2 [dict create seconds $baseTime]
    set date2 [ConvertUTCToLocal [K $date2 [set date2 {}]] $timeZone]
    set date2 [GetJulianDay [K $date2 [set date2 {}]]]

    # Calculate the ISO8601 date and transfer the year

    set date2 [GetYearWeekDay [K $date2 [set date2 {}]]]
    dict set date era CE
    dict set date iso8601Year [dict get $date2 iso8601Year]
    dict set date iso8601Week [dict get $date2 iso8601Week]
    return $date
}

#----------------------------------------------------------------------
#
# AssignBaseJulianDay --
#
#	Determines the base day for a time-of-day conversion.
#
# Parameters:
#	date - Dictionary that is to get the base day
#	baseTime - Base time expressed in seconds from the Posix epoch
#
# Results:
#	Returns the given dictionary augmented with a 'julianDay' field
#	that contains the base day.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::AssignBaseJulianDay { date baseTime timeZone } {

    variable PosixEpochAsJulianSeconds
    variable SecondsPerDay

    # Find the Julian Day Number corresponding to the base time

    set date2 [dict create seconds $baseTime]
    set date2 [ConvertUTCToLocal [K $date2 [set date2 {}]] $timeZone]
    set date2 [GetJulianDay [K $date2 [set date2 {}]]]
    dict set date julianDay [dict get $date2 julianDay]

    return $date
}

#----------------------------------------------------------------------
#
# InterpretHMSP --
#
#	Interprets a time in the form "hh:mm:ss am".
#
# Parameters:
#	date -- Dictionary containing "hourAMPM", "minute", "second"
#	        and "amPmIndicator" fields.
#
# Results:
#	Returns the number of seconds from local midnight.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::InterpretHMSP { date } {

    set hr [dict get $date hourAMPM]
    if { $hr == 12 } {
	set hr 0
    }
    if { [dict get $date amPmIndicator] } {
	incr hr 12
    }
    dict set date hour $hr
    return [InterpretHMS [K $date [set date {}]]]

}

#----------------------------------------------------------------------
#
# InterpretHMS --
#
#	Interprets a 24-hour time "hh:mm:ss"
#
# Parameters:
#	date -- Dictionary containing the "hour", "minute" and "second"
#	        fields.
#
# Results:
#	Returns the given dictionary augmented with a "secondOfDay"
#	field containing the number of seconds from local midnight.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::InterpretHMS { date } {

    variable SecondsPerMinute
    variable MinutesPerHour

    return [expr { ( [dict get $date hour] * $MinutesPerHour
		     + [dict get $date minute] ) * $SecondsPerMinute
		   + [dict get $date second] }]

}

#----------------------------------------------------------------------
#
# GetSystemTimeZone --
#
#	Determines the system time zone, which is the default for the
#	'clock' command if no other zone is supplied.
#
# Parameters:
#	None.
#
# Results:
#	Returns the system time zone.
#
# Side effects:
#	Stores the sustem time zone in the 'CachedSystemTimeZone'
#	variable, since determining it may be an expensive process.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GetSystemTimeZone {} {

    variable CachedSystemTimeZone

    if { [info exists ::env(TCL_TZ)] } {
	set timezone $::env(TCL_TZ)
    } elseif { [info exists ::env(TZ)] } {
	set timezone $::env(TZ)
    } elseif { $::tcl_platform(platform) eq {windows} } {
	if { [info exists CachedSystemTimeZone] } {
	    set timezone $CachedSystemTimeZone
	} else {
	    set timezone [GuessWindowsTimeZone]
	    set CachedSystemTimeZone $timezone
	}
    } else {
	set timezone :localtime
    }
    if { [catch {SetupTimeZone $timezone}] } {
	set timezone :localtime
    }
    return $timezone

}

#----------------------------------------------------------------------
#
# ConvertLegacyTimeZone --
#
#	Given an alphanumeric time zone identifier and the system
#	time zone, convert the alphanumeric identifier to an
#	unambiguous time zone.
#
# Parameters:
#	tzname - Name of the time zone to convert
#
# Results:
#	Returns a time zone name corresponding to tzname, but
#	in an unambiguous form, generally +hhmm.
#
# This procedure is implemented primarily to allow the parsing of
# RFC822 date/time strings.  Processing a time zone name on input
# is not recommended practice, because there is considerable room
# for ambiguity; for instance, is BST Brazilian Standard Time, or
# British Summer Time?
#
#----------------------------------------------------------------------

proc ::tcl::clock::ConvertLegacyTimeZone { tzname } {

    variable LegacyTimeZone

    set tzname [string tolower $tzname]
    if { ![dict exists $LegacyTimeZone $tzname] } {
	return -code error -errorcode [list CLOCK badTZName $tzname] \
	    "time zone \"$tzname\" not found"
    } else {
	return [dict get $LegacyTimeZone $tzname]
    }

}

#----------------------------------------------------------------------
#
# ConvertLocalToUTC --
#
#	Given a time zone and nominal local seconds, compute seconds
#	of UTC time from the Posix epoch.
#
# Parameters:
#	date - Dictionary populated with the 'localSeconds' and
#	       'tzName' fields
#
# Results:
#	Returns the given dictionary augmented with a 'seconds' field.
#
#----------------------------------------------------------------------

proc ::tcl::clock::ConvertLocalToUTC { date } {

    variable TZData

    set timezone [dict get $date tzName]
    if { $timezone eq ":localtime" } {

	# Convert using the mktime function if possible

	if { [catch { 
	    ConvertLocalToUTCViaC [dict get $date localSeconds]
	} result] } {
	    return -code error -errorcode $::errorCode $result
	}
	dict set date seconds $result
	return $date

    } else {

	# Get the time zone data

	if { [catch { SetupTimeZone $timezone } retval] } {
	    return -code error -errorcode $::errorCode $retval
	}
	
	# Initially assume that local == UTC, and locate the last time
	# conversion prior to that time.  Get the offset from that,
	# and look up again.  If that lookup finds a different offset,
	# continue looking until we find an offset that we found
	# before.  The check for "any offset previously found" rather
	# than "the same offset" avoids an endless loop if we try to
	# convert a non-existent time, for example 2:30am during the
	# US spring DST change.
	
	set localseconds [dict get $date localSeconds]
	set utcseconds(0) $localseconds
	set seconds $localseconds
	while { 1 } {
	    set i [BSearch $TZData($timezone) $seconds]
	    set offset [lindex $TZData($timezone) $i 1]
	    if { [info exists utcseconds($offset)] } {
		dict set date seconds $utcseconds($offset)
		return $date
	    } else {
		set seconds [expr { $localseconds - $offset }]
		set utcseconds($offset) $seconds
	    }
	}
	
	# In the absolute worst case, the loop above can visit each tzdata
	# row only once, so it's guaranteed to terminate.
	
	error "in ConvertLocalToUTC, can't happen"
    }

}

#----------------------------------------------------------------------
#
# ConvertLocalToUTCViaC --
#
#	Given seconds of nominal local time, compute seconds from the
#	Posix epoch.	
#
# Parameters:
#	localSeconds - Seconds of nominal local time
#
# Results:
#	Returns the seconds from the epoch.  May throw an error if
#	the time is to large/small to represent, or if 'mktime' is
#	not present in the C library.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::ConvertLocalToUTCViaC { localSeconds } {

    variable SecondsPerHour
    variable SecondsPerMinute
    variable MinutesPerHour
    variable HoursPerDay

    set date [dict create localSeconds $localSeconds]
    set date [GetJulianDay [K $date [set date {}]]]
    set date [GetGregorianEraYearDay [K $date [set date {}]]]
    set date [GetMonthDay [K $date [set date {}]]]
    set retval \
	[Mktime \
	     [dict get $date year] \
	     [dict get $date month] \
	     [dict get $date dayOfMonth] \
	     [expr { $localSeconds / $SecondsPerHour % $HoursPerDay }] \
	     [expr { $localSeconds / $SecondsPerMinute % $MinutesPerHour }] \
	     [expr { $localSeconds % $SecondsPerMinute }]]
    return $retval
}

#----------------------------------------------------------------------
#
# ConvertUTCToLocal --
#
#	Given the seconds from the Posix epoch, compute seconds of
#	nominal local time.
#
# Parameters:
#	date - Dictionary populated on entry with the 'seconds' field
#
# Results:
#	The given dictionary is returned, augmented with 'localSeconds',
#	'tzOffset', and 'tzName' fields.
#
#----------------------------------------------------------------------

proc ::tcl::clock::ConvertUTCToLocal { date timezone } {

    variable TZData

    # Get the data for time changes in the given zone

    if { [catch { SetupTimeZone $timezone } retval] } {
	return -code error -errorcode $::errorCode $retval
    }

    if { $timezone eq {:localtime} } {

	# Convert using the localtime function
	
	if { [catch {
	    ConvertUTCToLocalViaC $date
	} retval] } {
	    return -code error -errorcode $::errorCode $retval
	}
	return $retval
    }

    # Find the most recent transition in the time zone data

    set i [BSearch $TZData($timezone) [dict get $date seconds]]
    set row [lindex $TZData($timezone) $i]
    foreach { junk1 offset junk2 name } $row break

    # Add appropriate offset to convert Greenwich to local, and return
    # the local time

    dict set date localSeconds [expr { [dict get $date seconds] + $offset }]
    dict set date tzOffset $offset
    dict set date tzName $name

    return $date

}

#----------------------------------------------------------------------
#
# ConvertUTCToLocalViaC --
#
#	Convert local time using the C localtime function
#
# Parameters:
#	date - Dictionary populated on entry with the 'seconds'
#	       and 'timeZone' fields.
#
# Results:
#	The given dictionary is returned, augmented with 'localSeconds',
#	'tzOffset', and 'tzName' fields.
#
#----------------------------------------------------------------------

proc ::tcl::clock::ConvertUTCToLocalViaC { date } {

    variable PosixEpochAsJulianSeconds
    variable SecondsPerMinute
    variable SecondsPerHour
    variable MinutesPerHour
    variable HoursPerDay

    # Get y-m-d-h-m-s from the C library

    set gmtSeconds [dict get $date seconds]
    set localFields [Localtime $gmtSeconds]
    set date2 [dict create]
    foreach key { 
	year month dayOfMonth hour minute second 
    } value $localFields {
	dict set date2 $key $value
    }
    dict set date2 era CE

    # Convert to Julian Day

    set date2 [GetJulianDayFromEraYearMonthDay [K $date2 [set date2 {}]]]

    # Reconvert to seconds from the epoch in local time.

    set localSeconds [expr { ( ( ( wide([dict get $date2 julianDay]) 
				   * $HoursPerDay
				   + wide([dict get $date2 hour]) ) 
				 * $MinutesPerHour
				 + wide([dict get $date2 minute]) )
			       * $SecondsPerMinute
			       + wide([dict get $date2 second]) )
			     - $PosixEpochAsJulianSeconds }]
 
    # Determine the name and offset of the timezone

    set delta [expr { $localSeconds - $gmtSeconds }]
    if { $delta <= 0 } {
	set signum -
	set delta [expr { - $delta }]
    } else {
	set signum +
    }
    set hh [::format %02d [expr { $delta / $SecondsPerHour }]]
    set mm [::format %02d [expr { ($delta / $SecondsPerMinute )
				  % $MinutesPerHour }]]
    set ss [::format %02d [expr { $delta % $SecondsPerMinute }]]

    set zoneName $signum$hh$mm
    if { $ss ne {00} } {
	append zoneName $ss
    }

    # Fix the dictionary

    dict set date localSeconds $localSeconds
    dict set date tzOffset $delta
    dict set date tzName $zoneName
    return $date

}

#----------------------------------------------------------------------
#
# SetupTimeZone --
#
#	Given the name or specification of a time zone, sets up
#	its in-memory data.
#
# Parameters:
#	tzname - Name of a time zone
#
# Results:
#	Unless the time zone is ':localtime', sets the TZData array
#	to contain the lookup table for local<->UTC conversion.
#	Returns an error if the time zone cannot be parsed.
#
#----------------------------------------------------------------------

proc ::tcl::clock::SetupTimeZone { timezone } {

    variable TZData
    variable MinutesPerHour
    variable SecondsPerMinute
    variable MINWIDE

    if {! [info exists TZData($timezone)] } {
	if { $timezone eq {:localtime} } {

	    # Nothing to do, we'll convert using the localtime function

	} elseif { [regexp {^([-+])(\d\d):?(\d\d)(?::?(\d\d))?} $timezone \
		    -> s hh mm ss] } {

	    # Make a fixed offset

	    ::scan $hh %d hh
	    ::scan $mm %d mm
	    if { $ss eq {} } {
		set ss 0
	    } else {
		::scan $ss %d ss
	    }
	    set offset [expr { ( $hh * $MinutesPerHour
				 + $mm ) * $SecondsPerMinute
			       + $ss }]
	    if { $s eq {-} } {
		set offset [expr { - $offset }]
	    }
	    set TZData($timezone) [list [list $MINWIDE $offset -1 $timezone]]

	} elseif { [string index $timezone 0] eq {:} } {
	    
	    # Convert using a time zone file

	    if { 
		[catch {
		    LoadTimeZoneFile [string range $timezone 1 end]
		}]
		&& [catch {
		    LoadZoneinfoFile [string range $timezone 1 end]
		}]
	    } {
		return -code error \
		    -errorcode [list CLOCK badTimeZone $timezone] \
		    "time zone \"$timezone\" not found"
	    }
	    
	} elseif { ![catch {ParsePosixTimeZone $timezone} tzfields] } {
	    
	    # This looks like a POSIX time zone - try to process it

	    if { [catch {ProcessPosixTimeZone $tzfields} data] } {
		if { [lindex $::errorCode 0] eq {CLOCK} } {
		    return -code error -errorcode $::errorCode $data
		} else {
		    error $tzfields $::errorInfo $::errorCode
		}
	    } else {
		set TZData($timezone) $data
	    }

	} else {

	    # We couldn't parse this as a POSIX time zone.  Try
	    # again with a time zone file - this time without a colon

	    if { [catch {
		LoadTimeZoneFile $timezone
	    } msg] } {
		return -code error -errorcode $::errorCode $msg
	    }
	    set TZData($timezone) $TZData(:$timezone)

	}
    }

    return
}

#----------------------------------------------------------------------
#
# GuessWindowsTimeZone --
#
#	Determines the system time zone on windows.
#
# Parameters:
#	None.
#
# Results:
#	Returns a time zone specifier that corresponds to the system
#	time zone information found in the Registry.
#
# Bugs:
#	Fixed dates for DST change are unimplemented at present, because
#	no time zone information supplied with Windows actually uses
#	them!
#
# On a Windows system where neither $env(TCL_TZ) nor $env(TZ) is 
# specified, GuessWindowsTimeZone looks in the Registry for the
# system time zone information.  It then attempts to find an entry
# in WinZoneInfo for a time zone that uses the same rules.  If
# it finds one, it returns it; otherwise, it constructs a Posix-style
# time zone string and returns that.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GuessWindowsTimeZone {} {

    variable WinZoneInfo
    variable SecondsPerHour
    variable SecondsPerMinute
    variable MinutesPerHour

    # Dredge time zone information out of the registry

    set rpath HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\TimeZoneInformation
    set data [list \
		  [expr { -$SecondsPerMinute * [registry get $rpath Bias] }] \
		  [expr { -$SecondsPerMinute \
			      * [registry get $rpath StandardBias] }] \
		  [expr { -$SecondsPerMinute \
			      * [registry get $rpath DaylightBias] }]]
    set stdtzi [registry get $rpath StandardStart]
    foreach ind {0 2 14 4 6 8 10 12} {
	binary scan $stdtzi @${ind}s val
	lappend data $val
    }
    set daytzi [registry get $rpath DaylightStart]
    foreach ind {0 2 14 4 6 8 10 12} {
	binary scan $daytzi @${ind}s val
	lappend data $val
    }

    # Make up a Posix time zone specifier if we can't find one

    if { ! [dict exists $WinZoneInfo $data] } {
	foreach {
	    bias stdBias dstBias
	    stdYear stdMonth stdDayOfWeek stdDayOfMonth
	    stdHour stdMinute stdSecond stdMillisec
	    dstYear dstMonth dstDayOfWeek dstDayOfMonth
	    dstHour dstMinute dstSecond dstMillisec
	} $data break
	set stdDelta [expr { $bias + $stdBias }]
	set dstDelta [expr { $bias + $dstBias }]
	if { $stdDelta <= 0 } {
	    set stdSignum +
	    set stdDelta [expr { - $stdDelta }]
	} else {
	    set stdSignum -
	}
	set hh [::format %02d [expr { $stdDelta / $SecondsPerHour }]]
	set mm [::format %02d [expr { ($stdDelta / $SecondsPerMinute )
				      % $MinutesPerHour }]]
	set ss [::format %02d [expr { $stdDelta % $SecondsPerMinute }]]
	append tzname < $stdSignum $hh $mm > $stdSignum $hh : $mm : $ss
	if { $stdMonth >= 0 } {
	    if { $dstDelta <= 0 } {
		set dstSignum +
		set dstDelta [expr { - $dstDelta }]
	    } else {
		set dstSignum -
	    }
	    set hh [::format %02d [expr { $dstDelta / $SecondsPerHour }]]
	    set mm [::format %02d [expr { ($dstDelta / $SecondsPerMinute )
					   % $MinutesPerHour }]]
	    set ss [::format %02d [expr { $dstDelta % $SecondsPerMinute }]]
	    append tzname < $dstSignum $hh $mm > $dstSignum $hh : $mm : $ss
	    if { $dstYear == 0 } {
		append tzname ,M $dstMonth . $dstDayOfMonth . $dstDayOfWeek
	    } else {
		# I have not been able to find any locale on which
		# Windows converts time zone on a fixed day of the year,
		# hence don't know how to interpret the fields.
		# If someone can inform me, I'd be glad to code it up.
		# For right now, we bail out in such a case.
		return :localtime
	    }
	    append tzname / [::format %02d $dstHour] \
		: [::format %02d $dstMinute] \
		: [::format %02d $dstSecond]
	    if { $stdYear == 0 } {
		append tzname ,M $stdMonth . $stdDayOfMonth . $stdDayOfWeek
	    } else {
		# I have not been able to find any locale on which
		# Windows converts time zone on a fixed day of the year,
		# hence don't know how to interpret the fields.
		# If someone can inform me, I'd be glad to code it up.
		# For right now, we bail out in such a case.
		return :localtime
	    }
	    append tzname / [::format %02d $stdHour] \
		: [::format %02d $stdMinute] \
		: [::format %02d $stdSecond]
	}
	dict set WinZoneInfo $data $tzname
    } 

    return [dict get $WinZoneInfo $data]

}

#----------------------------------------------------------------------
#
# LoadTimeZoneFile --
#
#	Load the data file that specifies the conversion between a
#	given time zone and Greenwich.
#
# Parameters:
#	fileName -- Name of the file to load
#
# Results:
#	None.
#
# Side effects:
#	TZData(:fileName) contains the time zone data
#
#----------------------------------------------------------------------

proc ::tcl::clock::LoadTimeZoneFile { fileName } {
    variable DataDir
    variable TZData

    # Since an unsafe interp uses the [clock] command in the master,
    # this code is security sensitive.  Make sure that the path name
    # cannot escape the given directory.

    if { ![regexp {^[[:alpha:]_]+(?:/[[:alpha:]_]+)*$} $fileName] } {
	return -code error \
	    -errorcode [list CLOCK badTimeZone $:fileName] \
	    "time zone \":$fileName\" not valid"
    }
    if { [catch {
	source -encoding utf-8 [file join $DataDir $fileName]
    }] } {
	return -code error \
	    -errorcode [list CLOCK badTimeZone :$fileName] \
	    "time zone \":$fileName\" not found"
    }
}

#----------------------------------------------------------------------
#
# LoadZoneinfoFile --
#
#	Loads a binary time zone information file in Olson format.
#
# Parameters:
#	fileName - Path name of the file to load.
#
# Results:
#	Returns an empty result normally; returns an error if no
#	Olson file was found or the file was malformed in some way.
#
# Side effects:
#	TZData(:fileName) contains the time zone data
#
#----------------------------------------------------------------------

proc ::tcl::clock::LoadZoneinfoFile { fileName } {

    variable MINWIDE
    variable TZData
    variable ZoneinfoPaths

    # Since an unsafe interp uses the [clock] command in the master,
    # this code is security sensitive.  Make sure that the path name
    # cannot escape the given directory.

    if { ![regexp {^[[:alpha:]_]+(?:/[[:alpha:]_]+)*$} $fileName] } {
	return -code error \
	    -errorcode [list CLOCK badTimeZone $:fileName] \
	    "time zone \":$fileName\" not valid"
    }
    foreach d $ZoneinfoPaths {
	set fname [file join $d $fileName]
	if { [file readable $fname] && [file isfile $fname] } {
	    break
	}
	unset fname
    }
    if { ![info exists fname] } {
	return -code error "$fileName not found"
    }

    if { [file size $fname] > 262144 } {
	return -code error "$fileName too big"
    }

    # Suck in all the data from the file

    set f [open $fname r]
    fconfigure $f -translation binary
    set d [read $f]
    close $f

    # The file begins with a magic number, sixteen reserved bytes,
    # and then six 4-byte integers giving counts of fileds in the file.

    binary scan $d a4x16IIIIII magic nIsGMT mIsStd nLeap nTime nType nChar
    set seek 44
    if { $magic != {TZif} } {
	return -code error "$fileName not a time zone information file"
    }
    if { $nType > 255 } {
	return -code error "$fileName contains too many time types"
    }
    if { $nLeap != 0 } {
	return -code error "$fileName contains leap seconds"
    }

    # Next come ${nTime} transition times, followed by ${nTime} time type
    # codes.  The type codes are unsigned 1-byte quantities.  We insert an
    # arbitrary start time in front of the transitions.

    binary scan $d @${seek}I${nTime}c${nTime} times tempCodes
    incr seek [expr { 5 * $nTime }]
    set times [linsert $times 0 $MINWIDE]
    foreach c $tempCodes {
	lappend codes [expr { $c & 0xff }]
    }
    set codes [linsert $codes 0 0]

    # Next come ${nType} time type descriptions, each of which has an
    # offset (seconds east of GMT), a DST indicator, and an index into
    # the abbreviation text.

    for { set i 0 } { $i < $nType } { incr i } {
	binary scan $d @${seek}Icc gmtOff isDst abbrInd
	lappend types [list $gmtOff $isDst $abbrInd]
	incr seek 6
    }

    # Next come $nChar characters of time zone name abbreviations,
    # which are null-terminated.
    # We build them up into a dictionary indexed by character index,
    # because that's what's in the indices above.

    binary scan $d @${seek}a${nChar} abbrs
    incr seek ${nChar}
    set abbrList [split $abbrs \0]
    set i 0
    set abbrevs {}
    foreach a $abbrList {
	dict set abbrevs $i $a
	incr i [expr { [string length $a] + 1 }]
    }

    # The rest of the data in the file are not used at present.
    # Package up a list of tuples, each of which contains transition time,
    # seconds east of Greenwich, DST flag and time zone abbreviation.

    set r {}
    set lastTime $MINWIDE
    foreach t $times c $codes {
	if { $t < $lastTime } {
	    return -code error "$fileName has times out of order"
	}
	set lastTime $t
	foreach { gmtoff isDst abbrInd } [lindex $types $c] break
	set abbrev [dict get $abbrevs $abbrInd]
	lappend r [list $t $gmtoff $isDst $abbrev]
    }

    set TZData(:$fileName) $r

}

#----------------------------------------------------------------------
#
# ParsePosixTimeZone --
#
#	Parses the TZ environment variable in Posix form
#
# Parameters:
#	tz	Time zone specifier to be interpreted
#
# Results:
#	Returns a dictionary whose values contain the various pieces of
#	the time zone specification.
#
# Side effects:
#	None.
#
# Errors:
#	Throws an error if the syntax of the time zone is incorrect.
#
# The following keys are present in the dictionary:
#	stdName - Name of the time zone when Daylight Saving Time
#		  is not in effect.
#	stdSignum - Sign (+, -, or empty) of the offset from Greenwich 
#		    to the given (non-DST) time zone.  + and the empty
#		    string denote zones west of Greenwich, - denotes east
#		    of Greenwich; this is contrary to the ISO convention
#		    but follows Posix.
#	stdHours - Hours part of the offset from Greenwich to the given
#		   (non-DST) time zone.
#	stdMinutes - Minutes part of the offset from Greenwich to the
#		     given (non-DST) time zone. Empty denotes zero.
#	stdSeconds - Seconds part of the offset from Greenwich to the
#		     given (non-DST) time zone. Empty denotes zero.
#	dstName - Name of the time zone when DST is in effect, or the
#		  empty string if the time zone does not observe Daylight
#		  Saving Time.
#	dstSignum, dstHours, dstMinutes, dstSeconds -
#		Fields corresponding to stdSignum, stdHours, stdMinutes,
#		stdSeconds for the Daylight Saving Time version of the
#		time zone.  If dstHours is empty, it is presumed to be 1.
#	startDayOfYear - The ordinal number of the day of the year on which
#			 Daylight Saving Time begins.  If this field is
#			 empty, then DST begins on a given month-week-day,
#			 as below.
#	startJ - The letter J, or an empty string.  If a J is present in
#		 this field, then startDayOfYear does not count February 29
#		 even in leap years.
#	startMonth - The number of the month in which Daylight Saving Time
#		     begins, supplied if startDayOfYear is empty.  If both
#		     startDayOfYear and startMonth are empty, then US rules
#		     are presumed.
#	startWeekOfMonth - The number of the week in the month in which
#			   Daylight Saving Time begins, in the range 1-5.
#			   5 denotes the last week of the month even in a
#			   4-week month.
#	startDayOfWeek - The number of the day of the week (Sunday=0,
#			 Saturday=6) on which Daylight Saving Time begins.
#	startHours - The hours part of the time of day at which Daylight
#		     Saving Time begins. An empty string is presumed to be 2.
#	startMinutes - The minutes part of the time of day at which DST begins.
#		       An empty string is presumed zero.
#	startSeconds - The seconds part of the time of day at which DST begins.
#		       An empty string is presumed zero.
#	endDayOfYear, endJ, endMonth, endWeekOfMonth, endDayOfWeek,
#	endHours, endMinutes, endSeconds -
#		Specify the end of DST in the same way that the start* fields
#		specify the beginning of DST.
#		
# This procedure serves only to break the time specifier into fields.
# No attempt is made to canonicalize the fields or supply default values.
#
#----------------------------------------------------------------------

proc ::tcl::clock::ParsePosixTimeZone { tz } {

    if {[regexp -expanded -nocase -- {
	^
	# 1 - Standard time zone name
	([[:alpha:]]+ | <[-+[:alnum:]]+>)
	# 2 - Standard time zone offset, signum
	([-+]?)
	# 3 - Standard time zone offset, hours
	([[:digit:]]{1,2})
	(?:
	    # 4 - Standard time zone offset, minutes
	    : ([[:digit:]]{1,2}) 
	    (?: 
	        # 5 - Standard time zone offset, seconds
		: ([[:digit:]]{1,2} )
	    )?
	)?
	(?:
	    # 6 - DST time zone name
	    ([[:alpha:]]+ | <[-+[:alnum:]]+>)
	    (?:
	        (?:
		    # 7 - DST time zone offset, signum
		    ([-+]?)
		    # 8 - DST time zone offset, hours
		    ([[:digit:]]{1,2})
		    (?:
			# 9 - DST time zone offset, minutes
			: ([[:digit:]]{1,2}) 
			(?: 
		            # 10 - DST time zone offset, seconds
			    : ([[:digit:]]{1,2})
			)?
		    )?
		)?
	        (?:
		    ,
		    (?:
			# 11 - Optional J in n and Jn form 12 - Day of year
		        ( J ? )	( [[:digit:]]+ )
                        | M
			# 13 - Month number 14 - Week of month 15 - Day of week
			( [[:digit:]] + ) 
			[.] ( [[:digit:]] + ) 
			[.] ( [[:digit:]] + )
		    )
		    (?:
			# 16 - Start time of DST - hours
			/ ( [[:digit:]]{1,2} )
		        (?:
			    # 17 - Start time of DST - minutes
			    : ( [[:digit:]]{1,2} )
			    (?:
				# 18 - Start time of DST - seconds
				: ( [[:digit:]]{1,2} )
			    )?
			)?
		    )?
		    ,
		    (?:
			# 19 - Optional J in n and Jn form 20 - Day of year
		        ( J ? )	( [[:digit:]]+ )
                        | M
			# 21 - Month number 22 - Week of month 23 - Day of week
			( [[:digit:]] + ) 
			[.] ( [[:digit:]] + ) 
			[.] ( [[:digit:]] + )
		    )
		    (?:
			# 24 - End time of DST - hours
			/ ( [[:digit:]]{1,2} )
		        (?:
			    # 25 - End time of DST - minutes
			    : ( [[:digit:]]{1,2} )
			    (?:
				# 26 - End time of DST - seconds
				: ( [[:digit:]]{1,2} )
			    )?
			)?
		    )?
                )?
	    )?
        )?
	$
    } $tz -> x(stdName) x(stdSignum) x(stdHours) x(stdMinutes) x(stdSeconds) \
	     x(dstName) x(dstSignum) x(dstHours) x(dstMinutes) x(dstSeconds) \
	     x(startJ) x(startDayOfYear) \
	     x(startMonth) x(startWeekOfMonth) x(startDayOfWeek) \
	     x(startHours) x(startMinutes) x(startSeconds) \
	     x(endJ) x(endDayOfYear) \
	     x(endMonth) x(endWeekOfMonth) x(endDayOfWeek) \
	     x(endHours) x(endMinutes) x(endSeconds)] } {

	# it's a good timezone

	return [array get x]

    } else {

	return -code error\
	    -errorcode [list CLOCK badTimeZone $tz] \
	    "unable to parse time zone specification \"$tz\""

    }

}

#----------------------------------------------------------------------
#
# ProcessPosixTimeZone --
#
#	Handle a Posix time zone after it's been broken out into
#	fields.
#
# Parameters:
#	z - Dictionary returned from 'ParsePosixTimeZone'
#
# Results:
#	Returns time zone information for the 'TZData' array.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::ProcessPosixTimeZone { z } {

    variable MINWIDE
    variable SecondsPerMinute
    variable MinutesPerHour
    variable TZData

    # Determine the standard time zone name and seconds east of Greenwich

    set stdName [dict get $z stdName]
    if { [string index $stdName 0] eq {<} } {
	set stdName [string range $stdName 1 end-1]
    }
    if { [dict get $z stdSignum] eq {-} } {
	set stdSignum +1
    } else {
	set stdSignum -1
    }
    set stdHours [lindex [::scan [dict get $z stdHours] %d] 0] 
    if { [dict get $z stdMinutes] ne {} } {
	set stdMinutes [lindex [::scan [dict get $z stdMinutes] %d] 0] 
    } else {
	set stdMinutes 0
    }
    if { [dict get $z stdSeconds] ne {} } {
	set stdSeconds [lindex [::scan [dict get $z stdSeconds] %d] 0] 
    } else {
	set stdSeconds 0
    }
    set stdOffset [expr { ( ( $stdHours * $MinutesPerHour + $stdMinutes )
			    * $SecondsPerMinute + $stdSeconds )
			  * $stdSignum }]
    set data [list [list $MINWIDE $stdOffset 0 $stdName]]

    # If there's no daylight zone, we're done

    set dstName [dict get $z dstName]
    if { $dstName eq {} } {
	return $data
    }
    if { [string index $dstName 0] eq {<} } {
	set dstName [string range $dstName 1 end-1]
    }

    # Determine the daylight name

    if { [dict get $z dstSignum] eq {-} } {
	set dstSignum +1
    } else {
	set dstSignum -1
    }
    if { [dict get $z dstHours] eq {} } {
	set dstOffset [expr { 3600 + $stdOffset }]
    } else {
	set dstHours [lindex [::scan [dict get $z dstHours] %d] 0] 
	if { [dict get $z dstMinutes] ne {} } {
	    set dstMinutes [lindex [::scan [dict get $z dstMinutes] %d] 0] 
	} else {
	    set dstMinutes 0
	}
	if { [dict get $z dstSeconds] ne {} } {
	    set dstSeconds [lindex [::scan [dict get $z dstSeconds] %d] 0] 
	} else {
	    set dstSeconds 0
	}
	set dstOffset [expr { ( ( $dstHours * $MinutesPerHour + $dstMinutes )
				* $SecondsPerMinute + $dstSeconds )
			      * $dstSignum }]
    }

    # Fill in defaults for US DST rules

    if { [dict get $z startDayOfYear] eq {} 
	 && [dict get $z startMonth] eq {} } {
	dict set z startMonth 4
	dict set z startWeekOfMonth 1
	dict set z startDayOfWeek 0
	dict set z startHours 2
	dict set z startMinutes 0
	dict set z startSeconds 0
    }
    if { [dict get $z endDayOfYear] eq {} 
	 && [dict get $z endMonth] eq {} } {
	dict set z endMonth 10
	dict set z endWeekOfMonth 5
	dict set z endDayOfWeek 0
	dict set z endHours 2
	dict set z endMinutes 0
	dict set z endSeconds 0
    }

    # Put DST in effect in all years from 1916 to 2099.

    for { set y 1916 } { $y < 2099 } { incr y } {
	set startTime [DeterminePosixDSTTime $z start $y]
	incr startTime [expr { - wide($stdOffset) }]
	set endTime [DeterminePosixDSTTime $z end $y]
	incr endTime [expr { - wide($dstOffset) }]
	if { $startTime < $endTime } {
	    lappend data \
		[list $startTime $dstOffset 1 $dstName] \
		[list $endTime $stdOffset 0 $stdName]
	} else {
	    lappend data \
		[list $endTime $stdOffset 0 $stdName] \
		[list $startTime $dstOffset 1 $dstName]
	}
    }

    return $data
    
}    

#----------------------------------------------------------------------
#
# DeterminePosixDSTTime --
#
#	Determines the time that Daylight Saving Time starts or ends
#	from a Posix time zone specification.
#
# Parameters:
#	z - Time zone data returned from ParsePosixTimeZone.
#	    Missing fields are expected to be filled in with
#	    default values.
#	bound - The word 'start' or 'end'
#	y - The year for which the transition time is to be determined.
#
# Results:
#	Returns the transition time as a count of seconds from
#	the epoch.  The time is relative to the wall clock, not UTC.
#
#----------------------------------------------------------------------

proc ::tcl::clock::DeterminePosixDSTTime { z bound y } {

    variable FEB_28
    variable PosixEpochAsJulianSeconds
    variable SecondsPerDay
    variable SecondsPerMinute
    variable MinutesPerHour

    # Determine the start or end day of DST

    set date [dict create era CE year $y]
    set doy [dict get $z ${bound}DayOfYear]
    if { $doy ne {} } {

	# Time was specified as a day of the year

	if { [dict get $z ${bound}J] ne {}
	     && [IsGregorianLeapYear $y] 
	     && ( $doy > $FEB_28 ) } {
	    incr doy
	}
	dict set date dayOfYear $doy
	set date [GetJulianDayFromEraYearDay [K $date [set date {}]]]
    } else {

	# Time was specified as a day of the week within a month

	dict set date month [dict get $z ${bound}Month]
	dict set date dayOfWeekInMonth [dict get $z ${bound}WeekOfMonth]
	set dow [dict get $z ${bound}DayOfWeek]
	if { $dow >= 5 } {
	    set dow -1
	}
	dict set date dayOfWeek $dow
	set date [GetJulianDayFromEraYearMonthWeekDay [K $date [set date {}]]]

    }

    set jd [dict get $date julianDay]
    set seconds [expr { wide($jd) * wide($SecondsPerDay)
			- wide($PosixEpochAsJulianSeconds) }]

    set h [dict get $z ${bound}Hours]
    if { $h eq {} } {
	set h 2
    } else {
	set h [lindex [::scan $h %d] 0]
    }
    set m [dict get $z ${bound}Minutes]
    if { $m eq {} } {
	set m 0
    } else {
	set m [lindex [::scan $m %d] 0]
    }
    set s [dict get $z ${bound}Seconds]
    if { $s eq {} } {
	set s 0
    } else {
	set s [lindex [::scan $s %d] 0]
    }
    set tod [expr { ( $h * $MinutesPerHour + $m ) * $SecondsPerMinute + $s }]
    return [expr { $seconds + $tod }]

}

#----------------------------------------------------------------------
#
# GetLocaleEra --
#
#	Given local time expressed in seconds from the Posix epoch,
#	determine localized era and year within the era.
#
# Parameters:
#	date - Dictionary that must contain the keys, 'localSeconds',
#	       whose value is expressed as the appropriate local time;
#	       and 'year', whose value is the Gregorian year.
#
# Results:
#	Returns the dictionary, augmented with the keys, 'localeEra'
#	and 'localeYear'.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GetLocaleEra { date } {

    set etable [mc LOCALE_ERAS]
    set index [BSearch $etable [dict get $date localSeconds]]
    if { $index < 0 } {
	dict set date localeEra \
	    [::format %02d [expr { [dict get $date year] / 100 }]]
	dict set date localeYear \
	    [expr { [dict get $date year] % 100 }]
    } else {
	dict set date localeEra [lindex $etable $index 1]
	dict set date localeYear [expr { [dict get $date year] 
					 - [lindex $etable $index 2] }]
    }
    return $date

}
#----------------------------------------------------------------------
#
# GetJulianDay --
#
#	Given the seconds from the Posix epoch, derives the Julian
#	day number.
#
# Parameters:
#	date - Dictionary containing the date fields.  On input, 
#	       populated with a 'localSeconds' field that gives the
#	       nominal seconds from the epoch (in the local time zone,
#	       rather than UTC).
#
# Results:
#	Returns the given dictionary, augmented by a 'julianDay'
#	field that gives the Julian Day Number at noon of the current
#	date.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GetJulianDay { date } {

    variable PosixEpochAsJulianSeconds
    variable SecondsPerDay

    set secs [dict get $date localSeconds]

    return [dict set date julianDay \
		[expr { ( $secs + $PosixEpochAsJulianSeconds )
			/ $SecondsPerDay }]]

}

#----------------------------------------------------------------------
#
# GetGregorianEraYearDay --
#
#	Given the time from the Posix epoch and the current time zone,
#	develops the era, year, and day of year in the Gregorian calendar.
#
# Parameters:
#	date - Dictionary containing the date fields. On input, populated
#	       with the 'julianDay' key whose value is the Julian Day Number.
#
# Results:
#	Returns the given dictionary with the 'gregorian', 'era', 
#	'year', and 'dayOfYear' populated.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GetGregorianEraYearDay { date } {

    variable JD0Jan1CEGreg
    variable JD0Jan1CEJul
    variable DaysPer400Yr
    variable DaysPerCentury
    variable DaysPer4Yr
    variable DaysPerYear

    set jday [dict get $date julianDay]

    set changeover [mc GREGORIAN_CHANGE_DATE]

    if { $jday >= $changeover } {

	# Gregorian date

	dict set date gregorian 1

	# Calculate number of days since 1 January, 1 CE

	set day [expr { $jday - $JD0Jan1CEGreg - 1 }]

	# Calculate number of 400 year cycles

	set year 1
	set n [expr { $day / $DaysPer400Yr }]
	incr year [expr { 400 * $n }]
	set day [expr { $day % $DaysPer400Yr }]

	# Calculate number of centuries in the current cycle

	set n [expr { $day / $DaysPerCentury }]
	set day [expr { $day % $DaysPerCentury }]
	if { $n > 3 } {
	    set n 3			;# 31 December 2000, for instance
	    incr day $DaysPerCentury    ;# is last day of 400 year cycle
        }
	incr year [expr { 100 * $n }]

    } else {

	# Julian date

	dict set date gregorian 0

	# Calculate days since 0 January, 1 CE Julian

	set day [expr { $jday - $JD0Jan1CEJul - 1 }]
	set year 1
	
    }

    # Calculate number of 4-year cycles in current century (or in
    # the Common Era, if the calendar is Julian)

    set n [expr { $day / $DaysPer4Yr }]
    set day [expr { $day % $DaysPer4Yr }]
    incr year [expr { 4 * $n }]

    # Calculate number of years in current 4-year cycle

    set n [expr { $day / $DaysPerYear }]
    set day [expr { $day % $DaysPerYear }]
    if { $n > 3 } {
	set n 3				;# 31 December in a leap year
	incr day $DaysPerYear
    }
    incr year $n

    # Calculate the era

    if { $year <= 0 } {
	dict set date year [expr { 1 - $year }]
	dict set date era BCE
    } else {
	dict set date year $year
	dict set date era CE
    }

    # Return day of the year

    dict set date dayOfYear [expr { $day + 1 }]

    return $date

}

#----------------------------------------------------------------------
#
# GetMonthDay --
#
#	Given the ordinal number of the day within the year, determines
#	month and day of month in the Gregorian calendar.
#
# Parameters:
#	date - Dictionary containing the date fields. On input, populated
#	       with the 'era', 'gregorian', 'year' and 'dayOfYear' fields.
#
# Results:
#	Returns the given dictionary with the 'month' and 'dayOfMonth' 
#	fields populated.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GetMonthDay { date } {

    variable DaysInRomanMonthInCommonYear
    variable DaysInRomanMonthInLeapYear

    set day [dict get $date dayOfYear]
    if { [IsGregorianLeapYear $date] } {
	set hath $DaysInRomanMonthInLeapYear
    } else {
	set hath $DaysInRomanMonthInCommonYear
    }
    set month 1
    foreach n $hath {
	if { $day <= $n } {
	    break
	}
	incr month
	incr day [expr { -$n }]
    }
    dict set date month $month
    dict set date dayOfMonth $day

    return $date

}

#----------------------------------------------------------------------
#
# GetYearWeekDay
#
#	Given a julian day number, fiscal year, fiscal week,
#	and day of week in the ISO8601 calendar.
#
# Parameters:
#	
#	date - Dictionary where the 'julianDay' field is populated.
#	daysInFirstWeek - (Optional) Parameter giving the minimum number
#			  of days in the first week of a year.  Default is 4.
#
# Results:
#	Returns the given dictionary with values filled in for the
#	three given keys.
#
# Side effects:
#	None.
#
# Bugs:
#	Since ISO8601 week numbering is defined only for the Gregorian
#	calendar, dates on the Julian calendar or before the Common
#	Era may yield unexpected results. In particular, the year of
#	the Julian-to-Gregorian change may be up to three weeks short.
#	The era is not managed separately, so if the Common Era begins
#	(or the period Before the Common Era ends) with a partial week,
#	the few days at the beginning or end of the era may show up
#	as incorrectly belonging to the year zero.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GetYearWeekDay { date 
				    { keys { iso8601Year iso8601Week dayOfWeek } } } {

    set daysInFirstWeek 4
    set firstDayOfWeek 1

    # Determine the calendar year of $j - $daysInFirstWeek + 1.
    # Compute an upper bound of the fiscal year as being one year
    # past the day on which the current week begins. Find the start
    # of that year.
    
    set j [dict get $date julianDay]
    set jd [expr { $j - $daysInFirstWeek + 1 }]
    set date1 [GetGregorianEraYearDay [dict create julianDay $jd]]
    switch -exact -- [dict get $date1 era] {
	BCE {
	    dict set date1 fiscalYear [expr { [dict get $date1 year] - 1}]
	}
	CE {
	    dict set date1 fiscalYear [expr { [dict get $date1 year] + 1}]
	}
    }
    dict unset date1 year
    dict unset date1 dayOfYear
    dict set date1 weekOfFiscalYear 1
    dict set date1 dayOfWeek $firstDayOfWeek

    set date1 [GetJulianDayFromEraYearWeekDay \
		   [K $date1 [set date1 {}]] \
		   $daysInFirstWeek \
		   $firstDayOfWeek \
		   { fiscalYear weekOfFiscalYear dayOfWeek }]
    set startOfFiscalYear [dict get $date1 julianDay]

    # If we guessed high, move one year earlier.
    
    if { $j < $startOfFiscalYear } {
	switch -exact -- [dict get $date1 era] {
	    BCE {
		dict incr date1 fiscalYear
	    }
	    CE {
		dict incr date1 fiscalYear -1
	    }
	}
	set date1 [GetJulianDayFromEraYearWeekDay \
		       [K $date1 [set date1 {}]] \
		       $daysInFirstWeek \
		       $firstDayOfWeek \
		       {fiscalYear weekOfFiscalYear dayOfWeek }]
	set startOfFiscalYear [dict get $date1 julianDay]
    }
    
    # Get the week number and the day within the week
    
    set fiscalYear [dict get $date1 fiscalYear]
    set dayOfFiscalYear [expr { $j - $startOfFiscalYear }]
    set weekOfFiscalYear [expr { ( $dayOfFiscalYear / 7 ) + 1 }]
    set dayOfWeek [expr { ( $dayOfFiscalYear + 1 ) % 7 }]
    if { $dayOfWeek < $firstDayOfWeek } {
	incr dayOfWeek 7
    }
    
    # Store the fiscal year, week, and day in the given slots in the
    # given dictionary.

    foreach key $keys \
	value [list $fiscalYear $weekOfFiscalYear $dayOfWeek] {
	    dict set date $key $value
	}

    return $date
}

#----------------------------------------------------------------------
#
# GetJulianDayFromEraYearWeekDay --
#
#	Finds the Julian Day Number corresponding to the given era,
#	year, week and day.
#
# Parameters:
#	date -- A dictionary populated with fields whose keys are given
#		by the 'keys' parameter below, plus the 'era' field.
#	daysInFirstWeek -- (Optional) The minimum number of days in
#			   the first week of the year.  Default is 4.
#	firstDayOfWeek -- (Optional) The ordinal number of the first
#			  day of the week. Default is 1 (Monday);
#			  0 (Sunday) is an alternative.
#	keys -- (Optional) Keys in the dictionary for looking up the
#		fiscal year, fiscal week, and day of week. The
#		default is { iso8601Year iso8601Week dayOfWeek }.
#
# Results:
#	Returns the dictionary augmented with a 'julianDay' field
#	that gives the Julian Day Number corresponding to the given
#	date.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GetJulianDayFromEraYearWeekDay {
    date 
    { daysInFirstWeek 4 } 
    { firstDayOfWeek 1 }
    { keys { iso8601Year iso8601Week dayOfWeek } }
} {

    foreach var { fiscalYear fiscalWeek dayOfWeek } key $keys {
	set $var [dict get $date $key]
    }

    # Find a day of the first week of the year.

    set date2 [dict create \
		   era [dict get $date era] \
		   year $fiscalYear \
		   month 1 \
		   dayOfMonth $daysInFirstWeek]
    set date2 [GetJulianDayFromEraYearMonthDay [K $date2 [set date2 {}]]]

    # Find the Julian Day Number of the start of that week.

    set jd [WeekdayOnOrBefore $firstDayOfWeek [dict get $date2 julianDay]]

    # Add the required number of weeks and days

    dict set date julianDay \
	[expr { $jd
		+ ( 7 * ( $fiscalWeek - 1 ) )
	        + $dayOfWeek - $firstDayOfWeek }]

    return $date

}

#----------------------------------------------------------------------
#
# GetJulianDayFromEraYearMonthDay --
#
#	Given a year, month and day on the Gregorian calendar, determines
#	the Julian Day Number beginning at noon on that date.
#
# Parameters:
#	date -- A dictionary in which the 'era', 'year', 'month', and
#		'dayOfMonth' slots are populated. The calendar in use
#		is determined by the date itself relative to
#		[mc GREGORIAN_CHANGE_DATE] in the current locale.
#
# Results:
#	Returns the given dictionary augmented with a 'julianDay' key
#	whose value is the desired Julian Day Number, and a 'gregorian'
#	key that specifies whether the calendar is Gregorian (1) or
#	Julian (0).
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GetJulianDayFromEraYearMonthDay { date } {

    variable JD0Jan1CEJul
    variable JD0Jan1CEGreg
    variable DaysInPriorMonthsInCommonYear
    variable DaysInPriorMonthsInLeapYear

    # Get absolute year number from the civil year

    switch -exact [dict get $date era] {
	BCE {
	    set year [expr { 1 - [dict get $date year] }]
	}
	CE {
	    set year [dict get $date year]
	}
    }
    set ym1 [expr { $year - 1 }]

    # Try the Gregorian calendar first.

    dict set date gregorian 1
    set jd [expr { $JD0Jan1CEGreg
		   + [dict get $date dayOfMonth]
		   + ( [IsGregorianLeapYear $date] ?
		       [lindex $DaysInPriorMonthsInLeapYear \
			    [expr { [dict get $date month] - 1}]]
		       : [lindex $DaysInPriorMonthsInCommonYear \
			      [expr { [dict get $date month] - 1}]] )
		   + ( 365 * $ym1 )
		   + ( $ym1 / 4 )
		   - ( $ym1 / 100 )
		   + ( $ym1 / 400 ) }]
    
    # If the date is before the Gregorian change, use the Julian calendar.

    if { $jd < [mc GREGORIAN_CHANGE_DATE] } {

	dict set date gregorian 0
	set jd [expr { $JD0Jan1CEJul
		       + [dict get $date dayOfMonth]
		       + ( ( $year % 4 == 0 ) ?
		       [lindex $DaysInPriorMonthsInLeapYear \
			    [expr { [dict get $date month] - 1}]]
		       : [lindex $DaysInPriorMonthsInCommonYear \
			      [expr { [dict get $date month] - 1}]] )
		       + ( 365 * $ym1 )
		       + ( $ym1 / 4 ) }]
    }

    dict set date julianDay $jd
    return $date

}

#----------------------------------------------------------------------
#
# GetJulianDayFromEraYearDay --
#
#	Given a year, month and day on the Gregorian calendar, determines
#	the Julian Day Number beginning at noon on that date.
#
# Parameters:
#	date -- A dictionary in which the 'era', 'year', and
#		'dayOfYear' slots are populated. The calendar in use
#		is determined by the date itself relative to
#		[mc GREGORIAN_CHANGE_DATE] in the current locale.
#
# Results:
#	Returns the given dictionary augmented with a 'julianDay' key
#	whose value is the desired Julian Day Number, and a 'gregorian'
#	key that specifies whether the calendar is Gregorian (1) or
#	Julian (0).
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GetJulianDayFromEraYearDay { date } {

    variable JD0Jan1CEJul
    variable JD0Jan1CEGreg
    variable DaysInPriorMonthsInCommonYear
    variable DaysInPriorMonthsInLeapYear

    # Get absolute year number from the civil year

    switch -exact [dict get $date era] {
	BCE {
	    set year [expr { 1 - [dict get $date year] }]
	}
	CE {
	    set year [dict get $date year]
	}
    }
    set ym1 [expr { $year - 1 }]

    # Try the Gregorian calendar first.

    dict set date gregorian 1
    set jd [expr { $JD0Jan1CEGreg
		   + [dict get $date dayOfYear]
		   + ( 365 * $ym1 )
		   + ( $ym1 / 4 )
		   - ( $ym1 / 100 )
		   + ( $ym1 / 400 ) }]
    
    # If the date is before the Gregorian change, use the Julian calendar.

    if { $jd < [mc GREGORIAN_CHANGE_DATE] } {
	dict set date gregorian 0
	set jd [expr { $JD0Jan1CEJul
		       + [dict get $date dayOfYear]
		       + ( 365 * $ym1 )
		       + ( $ym1 / 4 ) }]
    }

    dict set date julianDay $jd
    return $date
}

#----------------------------------------------------------------------
#
# GetJulianDayFromEraYearMonthWeekDay --
#
#	Determines the Julian Day number corresponding to the nth
#	given day-of-the-week in a given month.
#
# Parameters:
#	date - Dictionary containing the keys, 'era', 'year', 'month'
#	       'weekOfMonth', 'dayOfWeek', and 'dayOfWeekInMonth'.
#
# Results:
#	Returns the given dictionary, augmented with a 'julianDay' key.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::GetJulianDayFromEraYearMonthWeekDay { date } {

    variable DaysPerWeek

    # Come up with a reference day; either the zeroeth day of the
    # given month (dayOfWeekInMonth >= 0) or the seventh day of the
    # following month (dayOfWeekInMonth < 0)

    set date2 $date
    set week [dict get $date dayOfWeekInMonth]
    if { $week >= 0 } {
	dict set date2 dayOfMonth 0
    } else {
	dict incr date2 month
	dict set date2 dayOfMonth 7
    }
    set date2 [GetJulianDayFromEraYearMonthDay [K $date2 [set date2 {}]]]
    set wd0 [WeekdayOnOrBefore [dict get $date dayOfWeek] \
		 [dict get $date2 julianDay]]
    dict set date julianDay [expr { $wd0 + $DaysPerWeek * $week }]
    return $date

}

#----------------------------------------------------------------------
#
# IsGregorianLeapYear --
#
#	Determines whether a given date represents a leap year in the
#	Gregorian calendar.
#
# Parameters:
#	date -- The date to test.  The fields, 'era', 'year' and 'gregorian'
#	        must be set.
#
# Results:
#	Returns 1 if the year is a leap year, 0 otherwise.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::IsGregorianLeapYear { date } {

    switch -exact -- [dict get $date era] {
	BCE { 
	    set year [expr { 1 - [dict get $date year]}]
	}
	CE {
	    set year [dict get $date year]
	}
    }
    if { $year % 4 != 0 } {
	return 0
    } elseif { ![dict get $date gregorian] } {
	return 1
    } elseif { $year % 400 == 0 } {
	return 1
    } elseif { $year % 100 == 0 } {
	return 0
    } else {
	return 1
    }

}

#----------------------------------------------------------------------
#
# WeekdayOnOrBefore --
#
#	Determine the nearest day of week (given by the 'weekday'
#	parameter, Sunday==0) on or before a given Julian Day.
#
# Parameters:
#	weekday -- Day of the week
#	j -- Julian Day number
#
# Results:
#	Returns the Julian Day Number of the desired date.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::WeekdayOnOrBefore { weekday j } {

    set k [expr { ( $weekday + 6 ) %  7 }]
    return [expr { $j - ( $j - $k ) % 7 }]

}

#----------------------------------------------------------------------
#
# BSearch --
#
#	Service procedure that does binary search in several places
#	inside the 'clock' command.
#
# Parameters:
#	list - List of lists, sorted in ascending order by the
#	       first elements
#	key - Value to search for
#
# Results:
#	Returns the index of the greatest element in $list that is less
#	than or equal to $key.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::BSearch { list key } {

    if { $key < [lindex $list 0 0] } {
	return -1
    }

    set l 0
    set u [expr { [llength $list] - 1 }]

    while { $l < $u } {

	# At this point, we know that
	#   $k >= [lindex $list $l 0]
	#   Either $u == [llength $list] or else $k < [lindex $list $u+1 0]
	# We find the midpoint of the interval {l,u} rounded UP, compare
	# against it, and set l or u to maintain the invariant.  Note
	# that the interval shrinks at each step, guaranteeing convergence.

	set m [expr { ( $l + $u + 1 ) / 2 }]
	if { $key >= [lindex $list $m 0] } {
	    set l $m
	} else {
	    set u [expr { $m - 1 }]
	}
    }

    return $l
}

#----------------------------------------------------------------------
#
# clock add --
#
#	Adds an offset to a given time.
#
# Syntax:
#	clock add clockval ?count unit?... ?-option value?
#
# Parameters:
#	clockval -- Starting time value
#	count -- Amount of a unit of time to add
#	unit -- Unit of time to add, must be one of:
#			years year months month weeks week
#			days day hours hour minutes minute
#			seconds second
#
# Options:
#	-gmt BOOLEAN
#		(Deprecated) Flag synonymous with '-timezone :GMT'
#	-timezone ZONE
#		Name of the time zone in which calculations are to be done.
#	-locale NAME
#		Name of the locale in which calculations are to be done.
#		Used to determine the Gregorian change date.
#
# Results:
#	Returns the given time adjusted by the given offset(s) in
#	order.
#
# Notes:
#	It is possible that adding a number of months or years will adjust
#	the day of the month as well.  For instance, the time at
#	one month after 31 January is either 28 or 29 February, because
#	February has fewer than 31 days.
#
#----------------------------------------------------------------------

proc ::tcl::clock::add { clockval args } {

    if { [llength $args] % 2 != 0 } {
	return -code error \
	    -errorcode [list CLOCK wrongNumArgs] \
	    "wrong \# args: should be\
             \"[lindex [info level 0] 0] clockval\
             ?number units?...\
             ?-gmt boolean? ?-locale LOCALE? ?-timezone ZONE?\""
    }
    if { [catch { expr wide($clockval) } result] } {
	return -code error $result
    }

    set offsets {}
    set gmt 0
    set locale C
    set timezone [GetSystemTimeZone]

    foreach { a b } $args {

	if { [string is integer -strict $a] } {

	    lappend offsets $a $b

	} else {

	    switch -exact -- $a {

		-gmt {
		    set gmt $b
		}
		-locale {
		    set locale $b
		}
		-timezone {
		    set timezone $b
		}
		default {
		    return -code error \
			-errorcode [list CLOCK badSwitch $flag] \
			"bad switch \"$flag\",\
                         must be -gmt, -locale or -timezone"
		}
	    }
	}
    }

    # Check options for validity

    if { [info exists saw(-gmt)] && [info exists saw(-timezone)] } {
	return -code error \
	    -errorcode [list CLOCK gmtWithTimezone] \
	    "cannot use -gmt and -timezone in same call"
    }
    if { [catch { expr { wide($clockval) } } result] } {
	return -code error \
	    "expected integer but got \"$clockval\"" 
    }
    if { ![string is boolean $gmt] } {
	return -code error \
	    "expected boolean value but got \"$gmt\""
    } else {
	if { $gmt } {
	    set timezone :GMT
	}
    }

    EnterLocale $locale oldLocale

    set status [catch {

	foreach { quantity unit } $offsets {

	    switch -exact -- $unit {

		years - year {
		    set clockval \
			[AddMonths [expr { 12 * $quantity }] \
			     $clockval $timezone]
		}
		months - month {
		    set clockval [AddMonths $quantity $clockval $timezone]
		}

		weeks - week {
		    set clockval [AddDays [expr { 7 * $quantity }] \
				      $clockval $timezone]
		}
		days - day {
		    set clockval [AddDays $quantity $clockval $timezone]
		}

		hours - hour {
		    set clockval [expr { 3600 * $quantity + $clockval }]
		}
		minutes - minute {
		    set clockval [expr { 60 * $quantity + $clockval }]
		}
		seconds - second {
		    set clockval [expr { $quantity + $clockval }]
		}

		default {
		    error "unknown unit \"$unit\", must be \
                        years, months, weeks, days, hours, minutes or seconds" \
			  "unknown unit \"$unit\", must be \
                        years, months, weeks, days, hours, minutes or seconds" \
			[list CLOCK badUnit $unit]
		}
	    }
	}
    } result]

    # Restore the locale

    if { [info exists oldLocale] } {
	mclocale $oldLocale
    }

    if { $status == 1 } {
	if { [lindex $::errorCode 0] eq {CLOCK} } {
	    return -code error -errorcode $::errorCode $result
	} else {
	    error $result $::errorInfo $::errorCode
	}
    } else {
	return $clockval
    }

}

#----------------------------------------------------------------------
#
# AddMonths --
#
#	Add a given number of months to a given clock value in a given
#	time zone.
#
# Parameters:
#	months - Number of months to add (may be negative)
#	clockval - Seconds since the epoch before the operation
#	timezone - Time zone in which the operation is to be performed
#
# Results:
#	Returns the new clock value as a number of seconds since
#	the epoch.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::AddMonths { months clockval timezone } {

    variable DaysInRomanMonthInCommonYear
    variable DaysInRomanMonthInLeapYear
    variable PosixEpochAsJulianSeconds
    variable SecondsPerDay

    # Convert the time to year, month, day, and fraction of day.

    set date [GetMonthDay \
		  [GetGregorianEraYearDay \
		       [GetJulianDay \
			    [ConvertUTCToLocal \
				 [dict create seconds $clockval] \
				 $timezone]]]]
    dict set date secondOfDay [expr { [dict get $date localSeconds]
				      % $SecondsPerDay }]
    dict set date tzName $timezone

    # Add the requisite number of months

    set m [dict get $date month]
    incr m $months
    incr m -1
    set delta [expr { $m / 12 }]
    set mm [expr { $m % 12 }]
    dict set date month [expr { $mm + 1 }]
    dict incr date year $delta

    # If the date doesn't exist in the current month, repair it

    if { [IsGregorianLeapYear $date] } {
	set hath [lindex $DaysInRomanMonthInLeapYear $mm]
    } else {
	set hath [lindex $DaysInRomanMonthInCommonYear $mm]
    }
    if { [dict get $date dayOfMonth] > $hath } {
	dict set date dayOfMonth $hath
    }

    # Reconvert to a number of seconds

    set date [GetJulianDayFromEraYearMonthDay \
		  [K $date [set date {}]]]
    dict set date localSeconds \
	[expr { -$PosixEpochAsJulianSeconds
		+ ( $SecondsPerDay * wide([dict get $date julianDay]) )
		+ [dict get $date secondOfDay] }]
    set date [ConvertLocalToUTC [K $date [set date {}]]]

    return [dict get $date seconds]

}

#----------------------------------------------------------------------
#
# AddDays --
#
#	Add a given number of days to a given clock value in a given
#	time zone.
#
# Parameters:
#	days - Number of days to add (may be negative)
#	clockval - Seconds since the epoch before the operation
#	timezone - Time zone in which the operation is to be performed
#
# Results:
#	Returns the new clock value as a number of seconds since
#	the epoch.
#
# Side effects:
#	None.
#
#----------------------------------------------------------------------

proc ::tcl::clock::AddDays { days clockval timezone } {

    variable PosixEpochAsJulianSeconds
    variable SecondsPerDay

    # Convert the time to Julian Day

    set date [GetJulianDay \
		  [ConvertUTCToLocal \
		       [dict create seconds $clockval] \
		       $timezone]]
    dict set date secondOfDay [expr { [dict get $date localSeconds]
				      % $SecondsPerDay }]
    dict set date tzName $timezone

    # Add the requisite number of days

    dict incr date julianDay $days

    # Reconvert to a number of seconds

    dict set date localSeconds \
	[expr { -$PosixEpochAsJulianSeconds
		+ ( $SecondsPerDay * wide([dict get $date julianDay]) )
		+ [dict get $date secondOfDay] }]
    set date [ConvertLocalToUTC [K $date [set date {}]]]

    return [dict get $date seconds]

}

#----------------------------------------------------------------------
#
# ClearCaches --
#
#	Clears all caches to reclaim the memory used in [clock]
#
# Parameters:
#	None.
#
# Results:
#	None.
#
# Side effects:
#	Caches are cleared.
#
#----------------------------------------------------------------------

proc ::tcl::clock::ClearCaches {} {

    variable LocaleNumeralCache
    variable McLoaded
    variable CachedSystemTimeZone
    variable TZData

    foreach p [info procs [namespace current]::scanproc'*] {
	rename $p {}
    }

    set LocaleNumeralCache {}
    set McLoaded {}
    catch {unset CachedSystemTimeZone}
    array unset TZData

}
