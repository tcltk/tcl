package require Tcl 8.7-
package provided http 3

namespace eval ::http {
    if {[info command ::http::Log] eq {}} {proc ::http::Log {args} {}}

    variable ContextConfig {
	-accept {string {^[^\s/]+/+[^\s/+]$} "MIME type"}
	-charset string
	-connectionclass class
	-keepalive boolean
	-proxyfilter callback
	-proxyhost string
	-proxyport integer
	-urlencoding encoding
	-useragent string
    }

    variable ConnectionConfig {
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
    variable ConnectionDefaults {
	-binary false
	-blocksize 8192
	-queryblocksize 8192
	-validate false
	-headers {}
	-timeout 0
	-type application/x-www-form-urlencoded
	-queryprogress {}
	-protocol 1.1
    }

    oo::class create Context {
	variable config
	variable strict socketmap urltypes encodings charset keepalive
	variable connectionclass counter

	constructor {} {
	    array set config {
		-accept */*
		-charset iso8859-1
		-keepalive 0
		-proxyhost {}
		-proxyport {}
		-urlencoding utf-8
	    }
	    set config(-proxyfilter) [namespace code {my ProxyRequired}]
	    set connectionclass ::http::Connection

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
	    set config(-useragent) "Mozilla/5.0 ($platform)\
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
	    set counter 0
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
	    variable ::http::ContextConfig

	    set options [dict keys $ContextConfig]
	    if {[llength $args] == 0} {
		set result {}
		dict for {option typeinfo} $ContextConfig {
		    lappend result $option $config($option)
		}
		return $result
	    }
	    if {[llength $args] == 1} {
		set opt [::tcl::prefix match $options [lindex $args 0]]
		return $config($opt)
	    }
	    foreach {option value} $args {
		set opt [::tcl::prefix match $options $option]
		set typeinfo [lassign [dict get $ContextConfig $opt] type]
		::http::Validate($type) $opt $value {*}$typeinfo
		set config($opt) $value
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
	    variable ::http::ConnectionConfig
	    variable ::http::ConnectionDefaults
	    if {[llength $args] & 1} {
		return -code error "missing configuration option"
	    }
	    set names [dict keys $ConnectionConfig]
	    set options [dict map {opt value} $args {
		set opt [::tcl::prefix match $names $opt]
		set typeinfo [lassign [dict get $ConnectionConfig $opt] type]
		::http::Validate($type) $opt $value {*}$typeinfo
		set value
	    }]
	    $connectionclass create [incr counter] \
		[self] $url $ConnectionDefaults $options
	}

	method ProxyRequred {host} {
	    if {[info exists proxyhost] && [string length $proxyhost]} {
		if {![info exists proxyport] || ![string length $proxyport]} {
		    set proxyport 8080
		}
		return [list $proxyhost $proxyport]
	    }
	}
    }

    oo::class create Connection {
	variable cfg http
	variable binary state meta coding currentsize totalsize querylength
	variable queryoffset type body status httpline connection charset
	constructor {context url defaults options} {
	    interp alias {} [namespace current]::Context {} $context
	    my eval upvar 0 [info object namespace $context]::config http
	    foreach {opt value} $defaults {
		set cfg($opt) $value
	    }
	    set cfg(-keepalive) $http(-keepalive)
	    foreach {opt value} $options {
		set cfg($opt) $value
	    }

	    my reset

	    set binary 0
	    set state connecting
	    set meta {}
	    set coding {}
	    set currentsize 0
	    set totalsize 0
	    set querylength 0
	    set queryoffset 0
	    set type text/html
	    set body {}
	    set status ""
	    set httpline ""
	    set connection close
	    set charset $http(-charset)
	}

	destructor {
	}

	method reset {{why ""}} {
	}

	method wait {} {
	    if {![info exists status] || $status eq ""} {
		# We must wait on the true variable name, not the local
		# unqualified version.
		vwait [my varname status]
	    }

	    return [my status]
	}

	method data {} {
	    return $body
	}

	method error {} {
	    if {[info exists error]} {
		return $error
	    }
	    return ""
	}

	method status {} {
	    if {![info exists status]} {
		return "error"
	    }
	    return $status
	}

	method code {} {
	    return $httpline
	}

	method ncode {} {
	    set thecode [my code]
	    if {[regexp {[0-9]{3}} $thecode numeric_code]} {
		return $numeric_code
	    } else {
		return $thecode
	    }
	}

	method size {} {
	    return $currentsize
	}

	method meta {} {
	    return $meta
	}

	# Return the list of content-encoding transformations we need to do in
	# order.
	method ContentEncoding {} {
	    set r {}
	    if {[info exists coding]} {
		foreach c [split $coding ,] {
		    switch -exact -- $c {
			deflate { lappend r inflate }
			gzip - x-gzip { lappend r gunzip }
			compress - x-compress { lappend r decompress }
			identity {}
			default {
			    return -code error \
				"unsupported content-encoding \"$c\""
			}
		    }
		}
	    }
	    return $r
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

    proc Validate(class) {option value} {
	if {![info object isa class $value]} {
	    return -code error \
		"bad value for $opt ($value), must be class"
	}
    }

    # http::CharsetToEncoding --
    #
    #	Tries to map a given IANA charset to a tcl encoding.  If no encoding
    #	can be found, returns binary.
    #

    proc CharsetToEncoding {charset} {
	variable encodings

	set charset [string tolower $charset]
	if {[regexp {iso-?8859-([0-9]+)} $charset -> num]} {
	    set encoding "iso8859-$num"
	} elseif {[regexp {iso-?2022-(jp|kr)} $charset -> ext]} {
	    set encoding "iso2022-$ext"
	} elseif {[regexp {shift[-_]?js} $charset]} {
	    set encoding "shiftjis"
	} elseif {[regexp {(?:windows|cp)-?([0-9]+)} $charset -> num]} {
	    set encoding "cp$num"
	} elseif {$charset eq "us-ascii"} {
	    set encoding "ascii"
	} elseif {[regexp {(?:iso-?)?lat(?:in)?-?([0-9]+)} $charset -> num]} {
	    switch -- $num {
		5 {set encoding "iso8859-9"}
		1 - 2 - 3 {
		    set encoding "iso8859-$num"
		}
	    }
	} else {
	    # other charset, like euc-xx, utf-8,... may directly map to
	    # encoding
	    set encoding $charset
	}
	set idx [lsearch -exact $encodings $encoding]
	if {$idx >= 0} {
	    return $encoding
	} else {
	    return "binary"
	}
    }
}

# Local variables:
# mode: tcl
# indent-tabs-mode: t
# End:
