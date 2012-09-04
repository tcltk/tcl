package require Tcl 8.6
package require sqlite3
package provide cookiejar 0.1

namespace eval ::http {
    # TODO is this the _right_ list of domains to use?
    variable CookiejarDomainList \
	http://mxr.mozilla.org/mozilla-central/source/netwerk/dns/effective_tld_names.dat?raw=1
}

::oo::class create ::http::cookiejar {
    variable aid
    constructor {{path ""}} {
	if {$path eq ""} {
	    sqlite3 [namespace current]::db :memory:
	} else {
	    sqlite3 [namespace current]::db $path
	}
	## FIXME
	## Model from Safari:
	# * Creation instant
	# * Domain
	# * Expiration instant
	# * Name
	# * Path
	# * Value
	## Model from Firefox:
	# CREATE TABLE moz_cookies (
	#     id INTEGER PRIMARY KEY,
	#     name TEXT,
	#     value TEXT,
	#     host TEXT,
	#     path TEXT,
	#     expiry INTEGER,
	#     lastAccessed INTEGER,
	#     isSecure INTEGER,
	#     isHttpOnly INTEGER,
	#     baseDomain TEXT,
	#     creationTime INTEGER)
	# CREATE INDEX moz_basedomain ON moz_cookies (baseDomain)
	# CREATE UNIQUE INDEX moz_uniqueid ON moz_cookies (name, host, path)
	db eval {
	    CREATE TABLE IF NOT EXISTS cookies (
		id INTEGER PRIMARY KEY,
		origin TEXT NOT NULL COLLATE NOCASE,
		path TEXT NOT NULL,
		domain TEXT COLLATE NOCASE,
		key TEXT NOT NULL,
		value TEXT NOT NULL,
		expiry INTEGER NOT NULL);
	    CREATE UNIQUE INDEX IF NOT EXISTS cookieUnique
		ON cookies (origin, path, key)
	}
	db eval {
	    CREATE TEMP TABLE IF NOT EXISTS sessionCookies (
		origin TEXT NOT NULL COLLATE NOCASE,
		path TEXT NOT NULL,
		domain TEXT COLLATE NOCASE,
		key TEXT NOT NULL,
		value TEXT NOT NULL);
	    CREATE UNIQUE INDEX IF NOT EXISTS sessionUnique
		ON sessionCookies (origin, path, key)
	}

	set aid [after 60000 [namespace current]::my PurgeCookies]

	if {$path ne ""} {
	    db transaction {
		db eval {
		    SELECT count(*) AS present FROM sqlite_master
		    WHERE type='table' AND name='forbidden'
		}
		if {!$present} {
		    my InitDomainList
		}
	    }
	}
    }

    method InitDomainList {} {
	variable ::http::CookiejarDomainList
	db eval {
	    CREATE TABLE IF NOT EXISTS forbidden (
		domain TEXT PRIMARY KEY);
	    CREATE TABLE IF NOT EXISTS forbiddenSuper (
		domain TEXT PRIMARY KEY);
	    CREATE TABLE IF NOT EXISTS permitted (
		domain TEXT PRIMARY KEY);
	}
	http::Log "Loading domain list from $CookiejarDomainList"
	set tok [http::geturl $CookiejarDomainList]
	try {
	    if {[http::ncode $tok] == 200} {
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
	    } else {
		http::Log "Warning: failed to fetch list of forbidden cookie domains"
	    }
	} finally {
	    http::cleanup $tok
	}
    }

    destructor {
	after cancel $aid
	db close
    }

    method GetCookiesForHostAndPath {result host path} {
	upvar 1 $result result
	db eval {
	    SELECT key, value FROM cookies
	    WHERE domain = $host AND path = $path
	} cookie {
	    dict set result $cookie(key) $cookie(value)
	}
	db eval {
	    SELECT key, value FROM sessionCookies
	    WHERE domain = $host AND path = $path
	} cookie {
	    dict set result $cookie(key) $cookie(value)
	}
    }
    method getCookies {proto host port path} {
	upvar 1 state state
	set result {}
	db transaction {
	    # Open question: how to move these manipulations into the
	    # database engine (if that's where they *should* be)
	    # Suggestion from kbk
	    #LENGTH(theColumn) <= LENGTH(:queryStr) AND SUBSTR(theColumn, LENGTH(:queryStr) LENGTH(theColumn)+1) = :queryStr
	    set pathbits [split [string trimleft $path "/"] "/"]
	    set hostbits [split $host "."]
	    if {[regexp {[^0-9.]} $host]} {
		for {set i [llength $hostbits]} {[incr i -1] >= 0} {} {
		    set domain [join [lrange $hostbits $i end] "."]
		    for {set j -1} {$j < [llength $pathbits]} {incr j} {
			set p /[join [lrange $pathbits 0 $j] "/"]
			my GetCookiesForHostAndPath result $domain $p
		    }
		}
	    } else {
		for {set j -1} {$j < [llength $pathbits]} {incr j} {
		    set p /[join [lrange $pathbits 0 $j] "/"]
		    my GetCookiesForHostAndPath result $host $p
		}
	    }
	}
	return $result
    }

    method BadDomain options {
	if {![dict exists $options domain]} {
	    return 0
	}
	set domain [dict get $options domain]
	db eval {
	    SELECT domain FROM permitted WHERE domain == $domain
	} x {return 0}
	db eval {
	    SELECT domain FROM forbidden WHERE domain == $domain
	} x {return 1}
	if {[regexp {^[^.]+\.(.+)$} $domain -> super]} {
	    db eval {
		SELECT domain FROM forbiddenSuper WHERE domain == $super
	    } x {return 1}
	}
	return 0
    }

    method storeCookie {name val options} {
	upvar 1 state state
	set now [clock seconds]
	db transaction {
	    if {[my BadDomain $options]} {
		http::Log "Warning: evil cookie detected"
		return
	    }
	    dict with options {}
	    if {!$persistent} {
		### FIXME
		db eval {
		    INSERT OR REPLACE sessionCookies (
			origin, domain, key, value)
		    VALUES ($origin, $domain, $key, $value)
		}
	    } elseif {$expires < $now} {
		db eval {
		    DELETE FROM cookies
		    WHERE domain = $domain AND key = $name AND path = $path
		}
		db eval {
		    DELETE FROM sessionCookies
		    WHERE domain = $domain AND key = $name AND path = $path
		}
	    } else {
		### FIXME
		db eval {
		    INSERT OR REPLACE cookies (
			origin, domain, key, value, expiry)
		    VALUES (:origin, :domain, :key, :value, :expiry)
		}
	    }
	}
    }

    method PurgeCookies {} {
	set aid [after 60000 [namespace current]::my PurgeCookies]
	set now [clock seconds]
	db eval {DELETE FROM cookies WHERE expiry < :now}
	### TODO: Cap the total number of cookies and session cookies,
	### purging least frequently used
    }

    forward Database db
}
