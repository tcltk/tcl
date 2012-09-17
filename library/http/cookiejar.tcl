# cookiejar.tcl --
#
#	Implementation of an HTTP cookie storage engine using SQLite. The
#	implementation is done as a TclOO class, and includes a punycode
#	encoder and decoder (though only the encoder is currently used).
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

# Dependencies
package require Tcl 8.6
package require http 2.8.4
package require sqlite3

#
# Configuration for the cookiejar package, plus basic support procedures.
#

namespace eval ::http {
    # Keep this in sync with pkgIndex.tcl and with the install directories in
    # Makefiles
    variable cookiejar_version 0.1

    # TODO: is this the _right_ list of domains to use?
    variable cookiejar_domainlist \
	http://mxr.mozilla.org/mozilla-central/source/netwerk/dns/effective_tld_names.dat?raw=1
    variable cookiejar_domainfile \
	[file join [file dirname [info script]] effective_tld_names.txt]
    # The list is directed to from http://publicsuffix.org/list/
    variable cookiejar_loglevel info
    variable cookiejar_vacuumtrigger 200
    variable cookiejar_offline false
    variable cookiejar_purgeinterval 60000

    # This is the class that we are creating
    ::oo::class create cookiejar

    # Some support procedures, none particularly useful in general
    namespace eval cookiejar_support {
	# Set up a logger if the http package isn't actually loaded yet.
	if {![llength [info commands ::http::Log]]} {
	    proc ::http::Log args {
		# Do nothing by default...
	    }
	}

	namespace export *
	proc locn {secure domain path {key ""}} {
	    if {$key eq ""} {
		format "%s://%s%s" [expr {$secure?"https":"http"}] \
		    [IDNAencode $domain] $path
	    } else {
		format "%s://%s%s?%s" \
		    [expr {$secure?"https":"http"}] [IDNAencode $domain] \
		    $path $key
	    }
	}
	proc splitDomain domain {
	    set pieces [split $domain "."]
	    for {set i [llength $pieces]} {[incr i -1] >= 0} {} {
		lappend result [join [lrange $pieces $i end] "."]
	    }
	    return $result
	}
	proc splitPath path {
	    set pieces [split [string trimleft $path "/"] "/"]
	    for {set j -1} {$j < [llength $pieces]} {incr j} {
		lappend result /[join [lrange $pieces 0 $j] "/"]
	    }
	    return $result
	}
	proc isoNow {} {
	    set ms [clock milliseconds]
	    set ts [expr {$ms / 1000}]
	    set ms [format %03d [expr {$ms % 1000}]]
	    clock format $ts -format "%Y%m%dT%H%M%S.${ms}Z" -gmt 1
	}
	proc log {level msg} {
	    namespace upvar ::http cookiejar_loglevel loglevel
	    set who [uplevel 1 self]
	    set map {debug 0 info 1 warn 2 error 3}
	    if {[string map $map $level] >= [string map $map $loglevel]} {
		::http::Log "[isoNow] [string toupper $level] cookiejar($who) - ${msg}"
	    }
	}
	proc IDNAencode str {
	    set parts {}
	    # Split term from RFC 3490, Sec 3.1
	    foreach part [split $str "\u002E\u3002\uFF0E\uFF61"] {
		if {![string is ascii $part]} {
		    set part xn--[puny::encode $part]
		}
		lappend parts $part
	    }
	    return [join $parts .]
	}
	proc IDNAdecode str {
	    set parts {}
	    # Split term from RFC 3490, Sec 3.1
	    foreach part [split $str "\u002E\u3002\uFF0E\uFF61"] {
		if {[string match "xn--*" $part]} {
		    set part [puny::decode [string range $part 4 end]]
		}
		lappend parts $part
	    }
	    return [join $parts .]
	}
    }
}

# Now we have enough information to provide the package.
package provide cookiejar $::http::cookiejar_version

# The implementation of the cookiejar package
::oo::define ::http::cookiejar {
    self method loglevel {{level "\u0000\u0000"}} {
	namespace upvar ::http cookiejar_loglevel loglevel
	if {$level in {debug info warn error}} {
	    set loglevel $level
	} elseif {$level ne "\u0000\u0000"} {
	    return -code error "unknown log level \"$level\": must be debug, info, warn, or error"
	}
	return $loglevel
    }

    variable aid deletions
    constructor {{path ""}} {
	namespace import ::http::cookiejar_support::*
	namespace upvar ::http cookiejar_purgeinterval purgeinterval

	if {$path eq ""} {
	    sqlite3 [namespace current]::db :memory:
	    set storeorigin "constructed cookie store in memory"
	} else {
	    sqlite3 [namespace current]::db $path
	    db timeout 500
	    set storeorigin "loaded cookie store from $path"
	}

	set deletions 0
	db eval {
	    --;# Store the persistent cookies in this table.
	    --;# Deletion policy: once they expire, or if explicitly killed.
	    CREATE TABLE IF NOT EXISTS persistentCookies (
		id INTEGER PRIMARY KEY,
		secure INTEGER NOT NULL,
		domain TEXT NOT NULL COLLATE NOCASE,
		path TEXT NOT NULL,
		key TEXT NOT NULL,
		value TEXT NOT NULL,
		originonly INTEGER NOT NULL,
		expiry INTEGER NOT NULL,
		creation INTEGER NOT NULL);
	    CREATE UNIQUE INDEX IF NOT EXISTS persistentUnique
		ON persistentCookies (domain, path, key);
	    CREATE INDEX IF NOT EXISTS persistentLookup
		ON persistentCookies (domain, path);

	    --;# Store the session cookies in this table.
	    --;# Deletion policy: at cookiejar instance deletion, if
	    --;# explicitly killed, or if the number of session cookies is too
	    --;# large and the cookie has not been used recently.
	    CREATE TEMP TABLE sessionCookies (
		id INTEGER PRIMARY KEY,
		secure INTEGER NOT NULL,
		domain TEXT NOT NULL COLLATE NOCASE,
		path TEXT NOT NULL,
		key TEXT NOT NULL,
		originonly INTEGER NOT NULL,
		value TEXT NOT NULL,
		lastuse INTEGER NOT NULL,
		creation INTEGER NOT NULL);
	    CREATE UNIQUE INDEX sessionUnique
		ON sessionCookies (domain, path, key);
	    CREATE INDEX sessionLookup ON sessionCookies (domain, path);

	    --;# View to allow for simple looking up of a cookie.
	    CREATE TEMP VIEW cookies AS
		SELECT id, domain, path, key, value, originonly, secure,
		    1 AS persistent
		    FROM persistentCookies
		UNION
		SELECT id, domain, path, key, value, originonly, secure,
		    0 AS persistent
		    FROM sessionCookies;

	    --;# Encoded domain permission policy; if forbidden is 1, no
	    --;# cookie may be ever set for the domain, and if forbidden is 0,
	    --;# cookies *may* be created for the domain (overriding the
	    --;# forbiddenSuper table).
	    --;# Deletion policy: normally not modified.
	    CREATE TABLE IF NOT EXISTS domains (
		domain TEXT PRIMARY KEY NOT NULL,
		forbidden INTEGER NOT NULL);

	    --;# Domains that may not have a cookie defined for direct child
	    --;# domains of them.
	    --;# Deletion policy: normally not modified.
	    CREATE TABLE IF NOT EXISTS forbiddenSuper (
		domain TEXT PRIMARY KEY);
	}

	set cookieCount "no"
	db eval {
	    SELECT COUNT(*) AS cookieCount FROM persistentCookies
	}
	log info "$storeorigin with $cookieCount entries"

	set aid [after $purgeinterval [namespace current]::my PurgeCookies]

	# TODO: domain list refresh policy
	if {$path ne "" && ![db exists {
	    SELECT 1 FROM domains
	}]} then {
	    my InitDomainList
	}
    }
 
    method InitDomainList {} {
	namespace upvar ::http \
	    cookiejar_domainlist url \
	    cookiejar_domainfile filename \
	    cookiejar_offline	 offline
	if {!$offline} {
	    log debug "loading domain list from $url"
	    set tok [::http::geturl $url]
	    try {
		if {[::http::ncode $tok] == 200} {
		    my InstallDomainData [::http::data $tok]
		    return
		} else {
		    log error "failed to fetch list of forbidden cookie domains from ${url}: [::http::error $tok]"
		    log warn "attempting to fall back to built in version"
		}
	    } finally {
		::http::cleanup $tok
	    }
	}
	log debug "loading domain list from $filename"
	try {
	    set f [open $filename]
	    try {
		if {[string match *.gz $filename]} {
		    zlib push gunzip $f
		}
		fconfigure $f -encoding utf-8
		my InstallDomainData [read $f]
	    } finally {
		close $f
	    }
	} on error msg {
	    log error "failed to read list of forbidden cookie domains from ${filename}: $msg"
	    return -code error $msg
	}
    }

    method InstallDomainData {data} {
	set n [db total_changes]
	db transaction {
	    foreach line [split $data "\n"] {
		if {[string trim $line] eq ""} {
		    continue
		} elseif {[string match //* $line]} {
		    continue
		} elseif {[string match !* $line]} {
		    set line [string range $line 1 end]
		    set idna [IDNAencode $line]
		    set utf [IDNAdecode $line]
		    db eval {
			INSERT OR REPLACE INTO domains (domain, forbidden)
			VALUES ($utf, 0);
		    }
		    if {$idna ne $utf} {
			db eval {
			    INSERT OR REPLACE INTO domains (domain, forbidden)
			    VALUES ($idna, 0);
			}
		    }
		} else {
		    if {[string match {\*.*} $line]} {
			set line [string range $line 2 end]
			set idna [IDNAencode $line]
			set utf [IDNAdecode $line]
			db eval {
			    INSERT OR REPLACE INTO forbiddenSuper (domain)
			    VALUES ($utf);
			}
			if {$idna ne $utf} {
			    db eval {
				INSERT OR REPLACE INTO forbiddenSuper (domain)
				VALUES ($idna);
			    }
			}
		    } else {
			set idna [IDNAencode $line]
			set utf [IDNAdecode $line]
		    }
		    db eval {
			INSERT OR REPLACE INTO domains (domain, forbidden)
			VALUES ($utf, 1);
		    }
		    if {$idna ne $utf} {
			db eval {
			    INSERT OR REPLACE INTO domains (domain, forbidden)
			    VALUES ($idna, 1);
			}
		    }
		}
		if {$utf ne [IDNAdecode $idna]} {
		    log warn "mismatch in IDNA handling for $idna ($line, $utf, [IDNAdecode $idna])"
		}
	    }
	}
	set n [expr {[db total_changes] - $n}]
	log debug "processed $n inserts generated from domain list"
    }

    # This forces the rebuild of the domain data, loading it from 
    method forceLoadDomainData {} {
	db transaction {
	    db eval {
		DELETE FROM domains;
		DELETE FROM forbiddenSuper;
	    }
	    my InitDomainList
	}
    }

    destructor {
	after cancel $aid
	db close
    }

    method GetCookiesForHostAndPath {listVar secure host path fullhost} {
	upvar 1 $listVar result
	log debug "check for cookies for [locn $secure $host $path]"
	db eval {
	    SELECT key, value FROM persistentCookies
	    WHERE domain = $host AND path = $path AND secure <= $secure
	    AND (NOT originonly OR domain = $fullhost)
	} {
	    lappend result $key $value
	}
	set now [clock seconds]
	db eval {
	    SELECT id, key, value FROM sessionCookies
	    WHERE domain = $host AND path = $path AND secure <= $secure
	    AND (NOT originonly OR domain = $fullhost)
	} {
	    lappend result $key $value
	    db eval {
		UPDATE sessionCookies SET lastuse = $now WHERE id = $id
	    }
	}
    }

    method getCookies {proto host path} {
	set result {}
	set paths [splitPath $path]
	set domains [splitDomain [IDNAencode $host]]
	set secure [string equal -nocase $proto "https"]
	# Open question: how to move these manipulations into the database
	# engine (if that's where they *should* be).
	# Suggestion from kbk:
	#LENGTH(theColumn) <= LENGTH($queryStr) AND
	#SUBSTR(theColumn, LENGTH($queryStr) LENGTH(theColumn)+1) = $queryStr
	if {[regexp {[^0-9.]} $host]} {
	    # Ugh, it's a numeric domain! Restrict it...
	    db transaction {
		foreach domain $domains {
		    foreach p $paths {
			my GetCookiesForHostAndPath result $secure $domain $p $host
		    }
		}
	    }
	} else {
	    db transaction {
		foreach p $paths {
		    my GetCookiesForHostAndPath result $secure $host $p $host
		}
	    }
	}
	return $result
    }

    method BadDomain options {
	if {![dict exists $options domain]} {
	    return 0
	}
	dict with options {}
	if {$domain ne $origin} {
	    log debug "cookie domain varies from origin ($domain, $origin)"
	}
	if {![regexp {[^0-9.]} $domain]} {
	    if {$domain eq $origin} {
		# May set for itself
		return 0
	    }
	    log warn "bad cookie: for a numeric address"
	    return 1
	}
	db eval {
	    SELECT forbidden FROM domains WHERE domain = $domain
	} {
	    if {$forbidden} {
		log warn "bad cookie: for a forbidden address"
	    }
	    return $forbidden
	}
	if {[regexp {^[^.]+\.(.+)$} $domain -> super] && [db exists {
	    SELECT 1 FROM forbiddenSuper WHERE domain = $super
	}]} then {
	    log warn "bad cookie: for a forbidden address"
	    return 1
	}
	return 0
    }

    method storeCookie {name val options} {
	set now [clock seconds]
	db transaction {
	    if {[my BadDomain $options]} {
		return
	    }
	    set now [clock seconds]
	    dict with options {}
	    if {!$persistent} {
		db eval {
		    INSERT OR REPLACE INTO sessionCookies (
			secure, domain, path, key, value, originonly, creation, lastuse)
		    VALUES ($secure, $domain, $path, $key, $value, $hostonly, $now, $now);
		    DELETE FROM persistentCookies
		    WHERE domain = $domain AND path = $path AND key = $key AND secure <= $secure
		}
		incr deletions [db changes]
		log debug "defined session cookie for [locn $secure $domain $path $key]"
	    } elseif {$expires < $now} {
		db eval {
		    DELETE FROM persistentCookies
		    WHERE domain = $domain AND path = $path AND key = $key
			AND secure <= $secure;
		}
		set del [db changes]
		db eval {
		    DELETE FROM sessionCookies
		    WHERE domain = $domain AND path = $path AND key = $key
			AND secure <= $secure;
		}
		incr deletions [incr del [db changes]]
		log debug "deleted $del cookies for [locn $secure $domain $path $key]"
	    } else {
		db eval {
		    INSERT OR REPLACE INTO persistentCookies (
			secure, domain, path, key, value, originonly, expiry, creation)
		    VALUES ($secure, $domain, $path, $key, $value, $hostonly, $expires, $now);
		    DELETE FROM sessionCookies
		    WHERE domain = $domain AND path = $path AND key = $key AND secure <= $secure
		}
		incr deletions [db changes]
		log debug "defined persistent cookie for [locn $secure $domain $path $key], expires at [clock format $expires]"
	    }
	}
    }

    method PurgeCookies {} {
	namespace upvar ::http \
	    cookiejar_vacuumtrigger trigger \
	    cookiejar_purgeinterval interval
	set aid [after $interval [namespace current]::my PurgeCookies]
	set now [clock seconds]
	log debug "purging cookies that expired before [clock format $now]"
	db transaction {
	    db eval {
		DELETE FROM persistentCookies WHERE expiry < $now
	    }
	    incr deletions [db changes]
	}
	### TODO: Cap the total number of cookies and session cookies,
	### purging least frequently used

	# Once we've deleted a fair bit, vacuum the database. Must be done
	# outside a transaction.
	if {$deletions > $trigger} {
	    set deletions 0
	    log debug "vacuuming cookie database"
	    catch {
		db eval {
		    VACUUM
		}
	    }
	}
    }

    forward Database db

    method lookup {{host ""} {key ""}} {
	set host [IDNAencode $host]
	db transaction {
	    if {$host eq ""} {
		set result {}
		db eval {
		    SELECT DISTINCT domain FROM cookies
		    ORDER BY domain
		} {
		    lappend result [IDNAdecode $domain]
		}
		return $result
	    } elseif {$key eq ""} {
		set result {}
		db eval {
		    SELECT DISTINCT key FROM cookies
		    WHERE domain = $host
		    ORDER BY key
		} {
		    lappend result $key
		}
		return $result
	    } else {
		db eval {
		    SELECT value FROM cookies
		    WHERE domain = $host AND key = $key
		    LIMIT 1
		} {
		    return $value
		}
		return -code error "no such key for that host"
	    }
	}
    }
}

# The implementation of the punycode encoder. This is based on the code on
# http://tools.ietf.org/html/rfc3492 (encoder) and http://wiki.tcl.tk/10501
# (decoder) but with extensive modifications.

namespace eval ::http::cookiejar_support::puny {
    namespace export encode decode

    variable digits [split "abcdefghijklmnopqrstuvwxyz0123456789" ""]
    # Bootstring parameters for Punycode
    variable base 36
    variable tmin 1
    variable tmax 26
    variable skew 38
    variable damp 700
    variable initial_bias 72
    variable initial_n 0x80

    variable maxcodepoint 0xFFFF  ;# 0x10FFFF would be correct, except Tcl
				   # can't handle non-BMP characters right now
				   # anyway.

    proc adapt {delta first numchars} {
	variable base
	variable tmin
	variable tmax
	variable damp
	variable skew

	set delta [expr {$delta / ($first ? $damp : 2)}]
	incr delta [expr {$delta / $numchars}]
	set k 0
	while {$delta > ($base - $tmin) * $tmax / 2} {
	    set delta [expr {$delta / ($base-$tmin)}]
	    incr k $base
	}
	return [expr {$k + ($base-$tmin+1) * $delta / ($delta+$skew)}]
    }

    # Main encode function
    proc encode {input {case ""}} {
	variable digits
	variable tmin
	variable tmax
	variable base
	variable initial_n
	variable initial_bias

	set in {}
	foreach char [set input [split $input ""]] {
	    scan $char "%c" ch
	    lappend in $ch
	}
	set output {}

	# Initialize the state:
	set n $initial_n
	set delta 0
	set bias $initial_bias

	# Handle the basic code points:
	foreach ch $input {
	    if {$ch < "\u0080"} {
		if {$case eq ""} {
		    append output $ch
		} elseif {$case} {
		    append output [string toupper $ch]
		} else {
		    append output [string tolower $ch]
		}
	    }
	}

	set b [string length $output]

	# h is the number of code points that have been handled, b is the
	# number of basic code points.

	if {$b > 0} {
	    append output "-"
	}

	# Main encoding loop:

	for {set h $b} {$h < [llength $in]} {incr delta; incr n} {
	    # All non-basic code points < n have been handled already.  Find
	    # the next larger one:

	    set m inf
	    foreach ch $in {
		if {$ch >= $n && $ch < $m} {
		    set m $ch
		}
	    }

	    # Increase delta enough to advance the decoder's <n,i> state to
	    # <m,0>, but guard against overflow:

	    if {$m-$n > (0xffffffff-$delta)/($h+1)} {
		throw {PUNYCODE OVERFLOW} "overflow in delta computation"
	    }
	    incr delta [expr {($m-$n) * ($h+1)}]
	    set n $m

	    foreach ch $in {
		if {$ch < $n && ([incr delta] & 0xffffffff) == 0} {
		    throw {PUNYCODE OVERFLOW} "overflow in delta computation"
		}

		if {$ch != $n} {
		    continue
		}

		# Represent delta as a generalized variable-length integer:

		for {set q $delta; set k $base} true {incr k $base} {
		    set t [expr {min(max($k-$bias, $tmin), $tmax)}]
		    if {$q < $t} {
			break
		    }
		    append output \
			[lindex $digits [expr {$t + ($q-$t)%($base-$t)}]]
		    set q [expr {($q-$t) / ($base-$t)}]
		}

		append output [lindex $digits $q]
		set bias [adapt $delta [expr {$h==$b}] [expr {$h+1}]]
		set delta 0
		incr h
	    }
	}

	return $output
    }

    # Main decode function
    proc decode {text {errors "lax"}} {
	namespace upvar ::http::cookiejar_support::puny \
	    tmin tmin tmax tmax base base initial_bias initial_bias \
	    initial_n initial_n maxcodepoint maxcodepoint

	set n $initial_n
	set pos -1
	set bias $initial_bias
	set buffer [set chars {}]
	set pos [string last "-" $text]
	if {$pos >= 0} {
	    set buffer [split [string range $text 0 [expr {$pos-1}]] ""]
	    set text [string range $text [expr {$pos+1}] end]
	}
	set points [split $text ""]
	set first true

	for {set extpos 0} {$extpos < [llength $points]} {} {
	    # Extract the delta, which is the encoding of the character and
	    # where to insert it.

	    set delta 0
	    set w 1
	    for {set j 1} true {incr j} {
		scan [set c [lindex $points $extpos]] "%c" char
		if {[string match {[A-Z]} $c]} {
		    set digit [expr {$char - 0x41}];	  # A=0,Z=25
		} elseif {[string match {[a-z]} $c]} {
		    set digit [expr {$char - 0x61}];	  # a=0,z=25
		} elseif {[string match {[0-9]} $c]} {
		    set digit [expr {$char - 0x30 + 26}]; # 0=26,9=35
		} else {
		    if {$errors eq "strict"} {
			throw {PUNYCODE INVALID} \
			    "invalid extended code point '$c'"
		    }
		    # There was an error in decoding. We can't continue
		    # because synchronization is lost.
		    return [join $buffer ""]
		}

		incr extpos
		set t [expr {min(max($base*$j - $bias, $tmin), $tmax)}]
		incr delta [expr {$digit * $w}]
		if {$digit < $t} {
		    break
		}
		set w [expr {$w * ($base - $t)}]

		if {$extpos >= [llength $points]} {
		    if {$errors eq "strict"} {
			throw {PUNYCODE PARTIAL} "incomplete punycode string"
		    }
		    # There was an error in decoding. We can't continue
		    # because synchronization is lost.
		    return [join $buffer ""]
		}
	    }

	    # Now we've got the delta, we can generate the character and
	    # insert it.

	    incr n [expr {[incr pos [expr {$delta+1}]]/([llength $buffer]+1)}]
	    if {$n > $maxcodepoint} {
		if {$errors eq "strict"} {
		    if {$n < 0x10ffff} {
			throw {PUNYCODE NON_BMP} \
			    [format "unsupported character U+%06x" $n]
		    }
		    throw {PUNYCODE NON_UNICODE} "bad codepoint $n"
		}
		set n 63 ;# "?"
		set extpos inf; # We're blowing up anyway...
	    }
	    set pos [expr {$pos % ([llength $buffer] + 1)}]
	    set buffer [linsert $buffer $pos [format "%c" $n]]
	    set bias [adapt $delta $first [llength $buffer]]
	    set first false
	}
	return [join $buffer ""]
    }
}
