package require Tcl 8.6
package require sqlite3
package provide cookiejar 0.1

::oo::class create cookiejar {
    variable aid
    constructor {{path ""}} {
	if {$path eq ""} {
	    sqlite3 [namespace current]::db :memory:
	} else {
	    sqlite3 [namespace current]::db $path
	}
	## FIXME
	db eval {
	    CREATE TABLE IF NOT EXISTS cookies (
		TEXT origin NOT NULL,
		TEXT domain,
		TEXT key NOT NULL,
		TEXT value NOT NULL,
		INTEGER expiry NOT NULL)
	    PRIMARY KEY (origin, key)
	}
	db eval {
	    CREATE TEMP TABLE IF NOT EXISTS sessionCookies (
		TEXT origin NOT NULL,
		TEXT domain,
		TEXT key NOT NULL,
		TEXT value NOT NULL)
	    PRIMARY KEY (origin, key)
	}

	set aid [after 60000 [namespace current]::my PurgeCookies]
    }

    destructor {
	after cancel $aid
	db close
    }

    method getCookies {proto host port path} {
	upvar 1 state state
	set result {}
	## TODO: How to handle prefix matches?
	db eval {
	    SELECT key, value FROM cookies WHERE domain = :host
	} cookie {
	    dict set result $key $value
	}
	db eval {
	    SELECT key, value FROM sessionCookies WHERE domain = :host
	} cookie {
	    dict set result $key $value
	}
	db eval {
	    SELECT key, value FROM cookies WHERE origin = :host
	} cookie {
	    dict set result $key $value
	}
	db eval {
	    SELECT key, value FROM sessionCookies WHERE origin = :host
	} cookie {
	    dict set result $key $value
	}
	return $result
    }

    method storeCookie {name val options} {
	upvar 1 state state
	set now [clock seconds]
	dict with options {}
	if {!$persistent} {
	    ### FIXME
	    db eval {
		INSERT OR REPLACE sessionCookies (
		    origin, domain, key, value
		) VALUES (:origin, :domain, :key, :value)
	    }
	} elseif {$expires < $now} {
	    db eval {
		DELETE FROM cookies
		WHERE domain = :domain AND key = :name
	    }
	    db eval {
		DELETE FROM sessionCookies
		WHERE domain = :domain AND key = :name
	    }
	} else {
	    ### FIXME
	    db eval {
		INSERT OR REPLACE cookies (
		    origin, domain, key, value, expiry
		) VALUES (:origin, :domain, :key, :value, :expiry)
	    }
	}
    }

    method PurgeCookies {} {
	set aid [after 60000 [namespace current]::my PurgeCookies]
	set now [clock seconds]
	db eval {DELETE FROM cookies WHERE expiry < :now}
    }

    forward Database db
}
