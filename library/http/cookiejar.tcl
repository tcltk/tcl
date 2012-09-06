package require Tcl 8.5
package require TclOO
package require http 2.7;#2.8.4
package require sqlite3

namespace eval ::http {
    # TODO: is this the _right_ list of domains to use?
    variable cookiejar_domainlist \
	http://mxr.mozilla.org/mozilla-central/source/netwerk/dns/effective_tld_names.dat?raw=1
    variable cookiejar_version 0.1
    variable cookiejar_loglevel info
}

::oo::class create ::http::cookiejar {
    self {
	method log {origin level msg} {
	    upvar 0 ::http::cookiejar_loglevel loglevel
	    set map {debug 0 info 1 warn 2 error 3}
	    if {[string map $map $level] >= [string map $map $loglevel]} {
		::http::Log [string toupper $level]:cookiejar($origin):${msg}
	    }
	}
	method loglevel {{level "\u0000\u0000"}} {
	    upvar 0 ::http::cookiejar_loglevel loglevel
	    if {$level in {debug info warn error}} {
		set loglevel $level
	    } elseif {$level ne "\u0000\u0000"} {
		return -code error "unknown log level \"$level\": must be debug, info, warn, or error"
	    }
	    return $loglevel
	}
    }

    variable aid deletions
    constructor {{path ""}} {
	if {$path eq ""} {
	    sqlite3 [namespace current]::db :memory:
	} else {
	    sqlite3 [namespace current]::db $path
	    db timeout 500
	}
	proc log {level msg} "::http::cookiejar log [list [self]] \$level \$msg"
	set deletions 0
	db eval {
	    CREATE TABLE IF NOT EXISTS cookies (
		id INTEGER PRIMARY KEY,
		secure INTEGER NOT NULL,
		domain TEXT NOT NULL COLLATE NOCASE,
		path TEXT NOT NULL,
		key TEXT NOT NULL,
		value TEXT NOT NULL,
		originonly INTEGER NOT NULL,
		expiry INTEGER NOT NULL);
	    CREATE UNIQUE INDEX IF NOT EXISTS cookieUnique
		ON cookies (secure, domain, path, key)
	}
	db eval {
	    CREATE TEMP TABLE sessionCookies (
		id INTEGER PRIMARY KEY,
		secure INTEGER NOT NULL,
		domain TEXT NOT NULL COLLATE NOCASE,
		path TEXT NOT NULL,
		key TEXT NOT NULL,
		originonly INTEGER NOT NULL,
		value TEXT NOT NULL);
	    CREATE UNIQUE INDEX sessionUnique
		ON sessionCookies (secure, domain, path, key)
	}

	db eval {
	    SELECT COUNT(*) AS cookieCount FROM cookies
	}
	if {[info exist cookieCount] && $cookieCount} {
	    log info "loaded cookie store from $path with $cookieCount entries"
	}

	set aid [after 60000 [namespace current]::my PurgeCookies]

	# TODO: domain list refresh policy
	db eval {
	    CREATE TABLE IF NOT EXISTS forbidden (
		domain TEXT PRIMARY KEY);
	    CREATE TABLE IF NOT EXISTS forbiddenSuper (
		domain TEXT PRIMARY KEY);
	    CREATE TABLE IF NOT EXISTS permitted (
		domain TEXT PRIMARY KEY);
	}
	if {$path ne ""} {
	    if {![db exists {
		SELECT 1 FROM sqlite_master
		WHERE type='table' AND name='forbidden'
	    }] && ![db exists {
		SELECT 1 FROM forbidden
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
			    db eval {
				INSERT INTO permitted (domain)
				VALUES ($line)
			    }
			} else {
			    if {[string match {\*.*} $line]} {
				set line [string range $line 2 end]
				db eval {
				    INSERT INTO forbiddenSuper (domain)
				    VALUES ($line)
				}
			    }
			    db eval {
				INSERT INTO forbidden (domain)
				VALUES ($line)
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

    method RenderLocation {secure domain path {key ""}} {
	if {$key eq ""} {
	    format "%s://%s%s" [expr {$secure?"https":"http"}] $domain $path
	} else {
	    format "%s://%s%s?%s" \
		[expr {$secure?"https":"http"}] $domain $path $key
	}
    }

    method GetCookiesForHostAndPath {listVar secure host path fullhost} {
	upvar 1 $listVar result
	log debug "check for cookies for [my RenderLocation $secure $host $path]"
	db eval {
	    SELECT key, value FROM cookies
	    WHERE secure <= $secure AND domain = $host AND path = $path
	    AND (NOT originonly OR domain = $fullhost)
	} cookie {
	    lappend result $cookie(key) $cookie(value)
	}
	db eval {
	    SELECT key, value FROM sessionCookies
	    WHERE secure <= $secure AND domain = $host AND path = $path
	    AND (NOT originonly OR domain = $fullhost)
	} cookie {
	    lappend result $cookie(key) $cookie(value)
	}
    }

    method SplitDomain domain {
	set pieces [split $domain "."]
	for {set i [llength $pieces]} {[incr i -1] >= 0} {} {
	    lappend result [join [lrange $pieces $i end] "."]
	}
	return $result
    }
    method SplitPath path {
	set pieces [split [string trimleft $path "/"] "/"]
	for {set j -1} {$j < [llength $pieces]} {incr j} {
	    lappend result /[join [lrange $pieces 0 $j] "/"]
	}
	return $result
    }

    method getCookies {proto host path} {
	set result {}
	set paths [my SplitPath $path]
	set domains [my SplitDomain $host]
	set secure [string equal -nocase $proto "https"]
	# Open question: how to move these manipulations into the database
	# engine (if that's where they *should* be)
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
	if {[db exists {
	    SELECT 1 FROM permitted WHERE domain = $domain
	}]} {return 0}
	if {[db exists {
	    SELECT 1 FROM forbidden WHERE domain = $domain
	}]} {
	    log warn "bad cookie: for a forbidden address"
	    return 1
	}
	if {[regexp {^[^.]+\.(.+)$} $domain -> super]} {
	    if {[db exists {
		SELECT 1 FROM forbiddenSuper WHERE domain = $super
	    }]} {
		log warn "bad cookie: for a forbidden address"
		return 1
	    }
	}
	return 0
    }

    method DeleteCookie {secure domain path key} {
	db eval {
	    DELETE FROM cookies
	    WHERE secure <= $secure AND domain = $domain AND path = $path AND key = $key
	}
	set del [db changes]
	db eval {
	    DELETE FROM sessionCookies
	    WHERE secure <= $secure AND domain = $domain AND path = $path AND key = $key
	}
	incr deletions [incr del [db changes]]
	log debug "deleted $del cookies for [my RenderLocation $secure $domain $path $key]"
    }

    method storeCookie {name val options} {
	set now [clock seconds]
	db transaction {
	    if {[my BadDomain $options]} {
		return
	    }
	    dict with options {}
	    if {!$persistent} {
		### FIXME
		db eval {
		    INSERT OR REPLACE INTO sessionCookies (
			secure, domain, path, key, value, originonly)
		    VALUES ($secure, $domain, $path, $key, $value, $hostonly);
		    DELETE FROM cookies
		    WHERE secure <= $secure AND domain = $domain AND path = $path AND key = $key
		}
		log debug "defined session cookie for [my RenderLocation $secure $domain $path $key]"
	    } elseif {$expires < $now} {
		my DeleteCookie $secure $domain $path $key
	    } else {
		### FIXME
		db eval {
		    INSERT OR REPLACE INTO cookies (
			secure, domain, path, key, value, originonly, expiry)
		    VALUES ($secure, $domain, $path, $key, $value, $hostonly, $expires);
		    DELETE FROM sessionCookies
		    WHERE secure <= $secure AND domain = $domain AND path = $path AND key = $key
		}
		log debug "defined persistent cookie for [my RenderLocation $secure $host $path $key], expires at [clock format $expires]"
	    }
	}
    }

    method PurgeCookies {} {
	set aid [after 60000 [namespace current]::my PurgeCookies]
	set now [clock seconds]
	log debug "purging cookies that expired before [clock format $now]"
	db transaction {
	    db eval {
		DELETE FROM cookies WHERE expiry < $now
	    }
	    incr deletions [db changes]
	    if {$deletions > 100} {
		set deletions 0
		log debug "vacuuming cookie database"
		db eval {
		    VACUUM
		}
	    }
	}
	### TODO: Cap the total number of cookies and session cookies,
	### purging least frequently used
    }

    forward Database db
}

package provide cookiejar $::http::cookiejar_version
