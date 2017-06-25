package require Tcl 8.7-
package provided http 3

namespace eval ::http {
    if {[info command ::http::Log] eq {}} {proc ::http::Log {args} {}}

    variable ContextConfiguration {
	-accept accept
	-proxyfilter proxyfilter
	-proxyhost proxyhost
	-proxyport proxyport
	-urlencoding urlencoding
	-useragent useragent
    }
    variable ContextCfgType {
	-accept {string {^[^\s/]+/+[^\s/+]$} "MIME type"}
	-proxyfilter callback
	-proxyhost string
	-proxyport integer
	-urlencoding encoding
	-useragent string
    }

    variable ConnectionConfiguration {
	-binary isBinary
	-blocksize blocksize
	-channel channel
	-command cmdCallback
	-handler handlerCallback
	-headers headers
	-keepalive keepalive
	-method method
	-myaddr myaddr
	-progress progressCallback
	-protocol protocol
	-query queryData
	-queryblocksize queryBlocksize
	-querychannel queryChannel
	-queryprogress queryCallback
	-strict strict
	-timeout timeout
	-type mimetype
	-validate validate
    }
    variable ConnectionCfgType {
	-binary boolean
	-blocksize integer
	-channel channel
	-command callback
	-handler callback
	-headers dict
	-keepalive boolean
	-method {string ^[A-Z0-9]+$ "uppercase string"}
	-myaddr string
	-progress callback
	-protocol string
	-query string
	-queryblocksize integer
	-querychannel channel
	-queryprogress callback
	-strict boolean
	-timeout integer
	-type {string {^[^\s/]+/+[^\s/+]$} "MIME type"}
	-validate boolean
    }

    oo::class create Context {
	variable accept proxyhost proxyport proxyfilter urlencoding strict
	variable useragent socketmap urltypes encodings charset keepalive 

	constructor {} {
	    set accept */*
	    set proxyhost {}
	    set proxyport {}
	    set proxyfilter [namespace code {my ProxyRequired}]
	    set urlencoding utf-8

	    # We need a useragent string of this style or various servers will
	    # refuse to send us compressed content even when we ask for it.
	    # This follows the de-facto layout of user-agent strings in
	    # current browsers.  Safe interpreters do not have
	    # ::tcl_platform(os) or ::tcl_platform(osVersion).
	    if {[interp issafe]} {
		set platform "Windows; U; Windows NT 10.0"
	    } else {
		global tcl_platform
		set platform "[string totitle $tcl_platform(platform)]; U;\
			$tcl_platform(os) $tcl_platform(osVersion)"
	    }
	    set useragent "Mozilla/5.0 ($platform)\
		    http/[package provide http] Tcl/[package provide Tcl]"

	    # Create a map for HTTP/1.1 open sockets
	    if {[info exists socketmap]} {
		# Close but don't remove open sockets on re-init
		foreach {url sock} [array get socketmap] {
		    catch {close $sock}
		}
	    }
	    array set socketmap {}

	    set urltypes(http) [list 80 ::socket]

	    set encodings [string tolower [encoding names]]
	    set charset "iso8859-1"
	    set keepalive 0
	    set strict 1
	}

	method register {proto port command} {
	    set lower [string tolower $proto]
	    try {
		return $urltypes($lower)
	    } on error {} {
		return {}
	    } finally {
		set urltypes($lower) [list $port $command]
	    }
	}

	method unregister {proto} {
	    set lower [string tolower $proto]
	    if {![info exists urlTypes($lower)]} {
		return -code error "unsupported url type \"$proto\""
	    }
	    try {
		return $urlTypes($lower)
	    } finally {
		unset -nocomplain urlTypes($lower)
	    }
	}

	method configure {args} {
	    variable ::http::ContextConfiguration
	    variable ::http::ContextCfgType

	    set options [dict keys $ContextConfiguration]
	    set usage [join $options ", "]
	    if {[llength $args] == 0} {
		set result {}
		dict for {option var} $ContextConfiguration {
		    upvar 0 [my varname $var] v
		    lappend result $option $v
		}
		return $result
	    }
	    if {[llength $args] == 1} {
		set opt [::tcl::prefix match $options [lindex $args 0]]
		upvar 0 [my varname [dict get $ContextConfiguration $opt]] v
		return $v
	    }
	    foreach {option value} $args {
		set opt [::tcl::prefix match $options $option]
		upvar 0 [my varname [dict get $ContextConfiguration $opt]] v
		set typeinfo [lassign [dict get $ContextCfgType $opt] type]
		::http::Validate($type) $opt $value {*}$typeinfo
		set v $value
	    }
	}

	method formatQuery args {
	    set result ""
	    set sep ""
	    foreach i $args {
		append result $sep [my mapReply $i]
		if {$sep eq "="} {
		    set sep &
		} else {
		    set sep =
		}
	    }
	    return $result
	}

	method mapReply {string} {
	    # The spec says: "non-alphanumeric characters are replaced by
	    # '%HH'". [Bug 1020491] [regsub -command] is *designed* for this.

	    if {$urlencoding ne ""} {
		set string [encoding convertto $urlencoding $string]
	    }

	    set RE "(\r?\n)|(\[^._~a-zA-Z0-9\])"
	    return [regsub -all -command -- $RE $string {apply {{- nl ch} {
		# RFC3986 Section 2.3 say percent encode all except:
		# "... percent-encoded octets in the ranges of ALPHA (%41-%5A
		# and %61-%7A), DIGIT (%30-%39), hyphen (%2D), period (%2E),
		# underscore (%5F), or tilde (%7E) should not be created by
		# URI producers ..."
		#
		# Note that newline is a special case
		if {$nl ne ""} {return %0D%0A}
		scan $ch %c c
		return [format %%%.2X $c]
	    }}}]
	}

	method geturl {url args} {
	    variable ::http::ConnectionCfgType
	    if {[llength $args] & 1} {
		return -code error "missing configuration option"
	    }
	    set names [dict keys $ConnectionCfgType]
	    set options [dict map {opt value} $args {
		set opt [::tcl::prefix match $names $opt]
		set typeinfo [lassign [dict get $ConnectionCfgType $opt] type]
		::http::Validate($type) $opt $value {*}$typeinfo
		set value
	    }]
	    ::http::Connection new [self] $url $options
	}
    }

    oo::class create Connection {
	constructor {config url options} {
	    variable ::http::ConnectionConfiguration
	}

	destructor {
	}

	method reset {{why ""}} {
	}

	method wait {} {
	}

	method data {} {
	}

	method error {} {
	}

	method status {} {
	}

	method code {} {
	}

	method ncode {} {
	}

	method meta {} {
	}
    }

    # ----------------------------------------------------------------------
    # General type validators

    proc Validate(boolean) {option value} {
	if {![string is boolean -strict $value]} {
	    return -code error \
		"bad value for $option ($value), must be boolean"
	}
    }

    proc Validate(integer) {option value} {
	if {![string is integer -strict $value] || $value < 0} {
	    return -code error \
		"bad value for $option ($value), must be non-negative integer"
	}
    }

    proc Validate(channel) {option value} {
	if {$value ni [channel names]} {
	    return -code error \
		"bad value for $option ($value), must be open channel"
	}
    }

    proc Validate(encoding) {option value} {
	if {$value ni [encoding names]} {
	    return -code error \
		"bad value for $option ($value), must be encoding"
	}
    }

    proc Validate(string) {option value {regexp ""} {typedesc ""}} {
	if {$regexp ne "" && ![regexp -- $regexp $value]} {
	    if {$typedesc eq ""} {
		set typedesc "match for $regexp"
	    }
	    return -code error \
		"bad value for $option ($value), must be $typedesc"
	}
    }

    proc Validate(callback) {option value} {
	if {![string is list $value] || [llength $value] == 0} {
	    return -code error \
		"bad value for $option ($value), must be non-empty callback"
	}
    }

    proc Validate(dict) {option value} {
	if {![string is list $value] || [llength $value] & 1} {
	    return -code error \
		"bad value for $opt ($value), must be dict"
	}
    }
}
