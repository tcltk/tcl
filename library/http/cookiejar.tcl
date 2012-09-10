# Cookie Jar package.

# Dependencies
package require Tcl 8.5;# FIXME: JUST DURING DEVELOPMENT
package require TclOO;# FIXME: JUST DURING DEVELOPMENT
package require http 2.7;# FIXME: JUST DURING DEVELOPMENT
#package require Tcl 8.6
#package require http 2.8.4
package require sqlite3

# Configuration for the cookiejar package 
namespace eval ::http {
    # TODO: is this the _right_ list of domains to use?
    variable cookiejar_domainlist \
	http://mxr.mozilla.org/mozilla-central/source/netwerk/dns/effective_tld_names.dat?raw=1
    # The list is directed to from http://publicsuffix.org/list/
    variable cookiejar_version 0.1
    variable cookiejar_loglevel info
    variable cookiejar_vacuumtrigger 200

    # This is the class that we are creating
    ::oo::class create cookiejar

    # Some support procedures, none particularly useful in general
    namespace eval cookiejar_support {
	namespace export *
	proc locn {secure domain path {key ""}} {
	    if {$key eq ""} {
		format "%s://%s%s" [expr {$secure?"https":"http"}] $domain $path
	    } else {
		format "%s://%s%s?%s" \
		    [expr {$secure?"https":"http"}] $domain $path $key
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
	    upvar 0 ::http::cookiejar_loglevel loglevel
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
    }
}

# Now we have enough information to provide the package.
package provide cookiejar $::http::cookiejar_version

# The implementation of the cookiejar package
::oo::define ::http::cookiejar {
    self method loglevel {{level "\u0000\u0000"}} {
	upvar 0 ::http::cookiejar_loglevel loglevel
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

	if {$path eq ""} {
	    sqlite3 [namespace current]::db :memory:
	} else {
	    sqlite3 [namespace current]::db $path
	    db timeout 500
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
	}

	db eval {
	    SELECT COUNT(*) AS cookieCount FROM persistentCookies
	}
	if {[info exist cookieCount] && $cookieCount} {
	    log info "loaded cookie store from $path with $cookieCount entries"
	}

	set aid [after 60000 [namespace current]::my PurgeCookies]

	# TODO: domain list refresh policy
	db eval {
	    --;# Encoded domain permission policy; if forbidden is 1, no
	    --;# cookie may be ever set for the domain, and if forbidden is 0,
	    --;# cookies *may* be created for the domain (overriding the
	    --;# forbiddenSuper table).
	    CREATE TABLE IF NOT EXISTS domains (
		domain TEXT PRIMARY KEY NOT NULL,
		forbidden INTEGER NOT NULL)

	    --;# Domains that may not have a cookie defined for direct child
	    --;# domains of them.
	    CREATE TABLE IF NOT EXISTS forbiddenSuper (
		domain TEXT PRIMARY KEY);
	}
	if {$path ne ""} {
	    if {![db exists {
		SELECT 1 FROM sqlite_master
		WHERE type='table' AND name='domains'
	    }] && ![db exists {
		SELECT 1 FROM domains
	    }]} then {
		my InitDomainList
	    }
	}
    }
 
    method InitDomainList {} {
	# TODO: Handle IDNs (but Tcl overall gets that wrong at the moment...)
	variable ::http::cookiejar_domainlist
	log debug "loading domain list from $cookiejar_domainlist"
	set tok [http::geturl $cookiejar_domainlist]
	try {
	    if {[http::ncode $tok] == 200} {
		db transaction {
		    foreach line [split [http::data $tok] \n] {
			if {[string trim $line] eq ""} continue
			if {[string match //* $line]} continue
			if {[string match !* $line]} {
			    set line [string range $line 1 end]
			    set idna [IDNAencode $line]
			    db eval {
				INSERT INTO domains (domain, forbidden)
				VALUES ($line, 0);
				INSERT OR REPLACE INTO domains (domain, forbidden)
				VALUES ($idna, 0);
			    }
			} else {
			    if {[string match {\*.*} $line]} {
				set line [string range $line 2 end]
				db eval {
				    INSERT INTO forbiddenSuper (domain)
				    VALUES ($line);
				    INSERT OR REPLACE INTO forbiddenSuper (domain)
				    VALUES ($idna);
				}
			    }
			    set idna [IDNAencode $line]
			    db eval {
				INSERT INTO domains (domain, forbidden)
				VALUES ($line, 1);
				INSERT OR REPLACE INTO domains (domain, forbidden)
				VALUES ($idna, 1);
			    }
			}
		    }
		}
	    } else {
		log error "failed to fetch list of forbidden cookie domains from $cookiejar_domainlist"
	    }
	} finally {
	    http::cleanup $tok
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
	set domains [splitDomain $host]
	set secure [string equal -nocase $proto "https"]
	# Open question: how to move these manipulations into the database
	# engine (if that's where they *should* be).
	# Suggestion from kbk:
	#LENGTH(theColumn) <= LENGTH($queryStr) AND SUBSTR(theColumn, LENGTH($queryStr) LENGTH(theColumn)+1) = $queryStr
	if {[regexp {[^0-9.]} $host]} {
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
	upvar 0 ::http::cookiejar_vacuumtrigger vacuumtrigger
	set aid [after 60000 [namespace current]::my PurgeCookies]
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
	if {$deletions > $vacuumtrigger} {
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
}

# The implementation of the puncode encoder. This is based on the code on
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

	set in [split $input ""]
	set output {}

	# Initialize the state:
	set n $initial_n
	set delta 0
	set bias $initial_bias

	# Handle the basic code points:
	foreach ch $in {
	    if {$ch < "\u0080"} {
		if {$case ne ""} {
		    if {$case} {
			append output [string toupper $ch]
		    } else {
			append output [string tolower $ch]
		    }
		} else {
		    append output $ch
		}
	    }
	}

	set h [set b [string length $output]]

	# h is the number of code points that have been handled, b is the
	# number of basic code points.

	if {$b} {
	    append output "-"
	}

	# Main encoding loop:

	while {$h < [llength $in]} {
	    # All non-basic code points < n have been handled already.  Find
	    # the next larger one:

	    for {set m inf; set j 0} {$j < [llength $in]} {incr j} {
		scan [lindex $in $j] "%c" ch
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

	    for {set j 0} {$j < [llength $in]} {incr j} {
		scan [lindex $in $j] "%c" ch
		if {$ch < $n && ([incr delta] & 0xffffffff) == 0} {
		    throw {PUNYCODE OVERFLOW} "overflow in delta computation"
		}

		if {$ch == $n} {
		    # Represent delta as a generalized variable-length
		    # integer:

		    for {set q $delta; set k $base} true {incr k $base} {
			set t [expr {min(max($k-$bias,$tmin),$tmax)}]
			if {$q < $t} break
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

	    incr delta
	    incr n
	}

	return $output
    }


    # Decoding
    proc toNums {text} {
	set retval {}
	foreach c [split $text ""] {
	    scan $c "%c" ch
	    lappend retval $ch
	}
	return $retval
    }

    proc toChars {nums} {
	set chars {}
	foreach char $nums {
	    append chars [format "%c" $char]
	}
	return $chars
    }

    # 3.3 Generalized variable-length integers
    proc decodeGeneralizedInteger {extended extpos bias errors} {
	variable tmin
	variable tmax
	variable base

	set result 0
	set w 1
	set j 0
	while true {
	    set c [lindex $extended $extpos]
	    incr extpos
	    if {[string length $c] == 0} {
		if {$errors eq "strict"} {
		    error "incomplete punicode string"
		}
		return [list $extpos -1]
	    }
	    scan $c "%c" char
	    if {[string match {[A-Z]} $c]} {
		set digit [expr {$char - 0x41}];	# A=0,Z=25
	    } elseif {[string match {[a-z]} $c]} {
		set digit [expr {$char - 0x61}];	# a=0,z=25
	    } elseif {[string match {[0-9]} $c]} {
		set digit [expr {$char - 0x30 + 26}];	# 0=26,9=35
	    } elseif {$errors eq "strict"} {
		set pos [lindex $extended $extpos]
		error "Invalid extended code point '$pos'"
	    } else {
		return [list $extpos -1]
	    }
	    set t [expr {min(max($base*($j + 1) - $bias, $tmin), $tmax)}]
	    set result [expr {$result + $digit * $w}]
	    if {$digit < $t} {
		return [list $extpos $result]
	    }
	    set w [expr {$w * ($base - $t)}]
	    incr j
	}
    }

    # 3.2 Insertion unsort coding
    proc insertionSort {buffer extended errors} {
	variable initial_bias
	variable initial_n

	set char $initial_n
	set pos -1
	set bias $initial_bias
	set extpos 0
	while {$extpos < [llength $extended]} {
	    lassign [decodeGeneralizedInteger $extended $extpos $bias $errors]\
		newpos delta
	    if {$delta < 0} {
		# There was an error in decoding. We can't continue because
		# synchronization is lost.
		return $buffer
	    }
	    incr pos [expr {$delta + 1}]
	    set char [expr {$char + $pos / ([llength $buffer] + 1)}]
	    if {$char > 1114111} {
		if {$errors eq "strict"} {
		    error [format "Invalid character U+%x" $char]
		}
		set char 63 ;# "?"
	    }
	    set pos [expr {$pos % ([llength $buffer] + 1)}]
	    set buffer [linsert $buffer $pos $char]
	    set bias [adapt $delta [expr {$extpos == 0}] [llength $buffer]]
	    set extpos $newpos
	}
	return $buffer
    }

    proc decode {text {errors "lax"}} {
	set baseline {}
	set pos [string last "-" $text]
	if {$pos == -1} {
	    set extended $text
	} else {
	    set baseline [toNums [string range $text 0 [expr {$pos-1}]]]
	    set extended [string range $text [expr {$pos+1}] end]
	}
	return [toChars [insertionSort $baseline [split $extended ""] $errors]]
    }
}
