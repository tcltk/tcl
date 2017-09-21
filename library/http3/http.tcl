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
	-strict boolean
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
	variable socketmap urltypes encodings charset keepalive
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

	method parseURL {url} {
	    # Validate URL, determine the server host and port, and check
	    # proxy case Recognize user:pass@host URLs also, although we do
	    # not do anything with that info yet.

	    # URLs have basically four parts.
	    #
	    # First, before the colon, is the protocol scheme (e.g. http).
	    #
	    # Second, for HTTP-like protocols, is the authority. The authority
	    #	is preceded by // and lasts up to (but not including) the
	    #	following / or ? and it identifies up to four parts, of which
	    #	only one, the host, is required (if an authority is present at
	    #	all). All other parts of the authority (user name, password,
	    #	port number) are optional.
	    #
	    # Third is the resource name, which is split into two parts at a ?
	    #	The first part (from the single "/" up to "?") is the path,
	    #	and the second part (from that "?" up to "#") is the
	    #	query. *HOWEVER*, we do not need to separate them; we send the
	    #	whole lot to the server.  Both, path and query are allowed to
	    #	be missing, including their delimiting character.
	    #
	    # Fourth is the fragment identifier, which is everything after the
	    #	firsts "#" in the URL. The fragment identifier MUST NOT be
	    #	sent to the server and indeed, we don't bother to validate it
	    #	(it could be an error to pass it in here, but it's cheap to
	    #	strip).
	    #
	    # An example of a URL that has all the parts:
	    #
	    #   http://joe:xyzzy@www.bogus.net:8000/foo/bar.tml?q=foo#changes
	    #
	    # The "http" is the protocol, the user is "joe", the password is
	    # "xyzzy", the host is "www.bogus.net", the port is "8000", the
	    # path is "/foo/bar.tml", the query is "q=foo", and the fragment
	    # is "changes".
	    #
	    # Note that the RE actually combines the user and password parts,
	    # as recommended in RFC 3986. Indeed, that RFC states that putting
	    # passwords in URLs is a Really Bad Idea, something with which I
	    # would agree utterly.
	    #
	    # From a validation perspective, we need to ensure that the parts
	    # of the URL that are going to the server are correctly encoded.
	    # This is only done if $config(-strict) is true.

	    set URLmatcher {(?x)	# this is _expanded_ syntax
		^
		(?: (\w+) : ) ?		# <protocol scheme>
		(?: //
		    (?:
			(
			    [^@/\#?]+	# <userinfo part of authority>
			) @
		    )?
		    (			# <host part of authority>
			[^/:\#?]+ |	# host name or IPv4 address
			\[ [^/\#?]+ \]	# IPv6 address in square brackets
		    )
		    (?: : (\d+) )?	# <port part of authority>
		)?
		( [/\?] [^\#]*)?	# <path> (including query)
		(?: \# (.*) )?		# <fragment>
		$
	    }

	    # Phase one: parse
	    if {![regexp -- $URLmatcher $url -> \
		    proto user host port srvurl fragment]} {
		return -code error "unsupported URL: $url"
	    }
	    # Phase two: validate
	    set host [string trim $host {[]}]; # strip square brackets from IPv6 address
	    if {$host eq ""} {
		# Caller has to provide a host name; we do not have a "default
		# host" that would enable us to handle relative URLs.
		return -code error "Missing host part: $url"
		# Note that we don't check the hostname for validity here; if
		# it's invalid, we'll simply fail to resolve it later on.
	    }
	    if {$port ne "" && $port > 65535} {
		return -code error "invalid port number: $port"
	    }
	    # The user identification and resource identification parts of the
	    # URL can have encoded characters in them; take care!
	    if {$user ne ""} {
		# Check for validity according to RFC 3986, Appendix A
		set validityRE {(?xi)
		    ^
		    (?: [-\w.~!$&'()*+,;=:] | %[0-9a-f][0-9a-f] )+
		    $
		}
		if {$config(-strict) && ![regexp -- $validityRE $user]} {
		    # Provide a better error message in this error case
		    if {[regexp {(?i)%(?![0-9a-f][0-9a-f]).?.?} $user bad]} {
			return -code error \
			    "illegal encoding character usage \"$bad\" in URL user"
		    }
		    return -code error "illegal characters in URL user"
		}
	    }
	    if {$srvurl ne ""} {
		# RFC 3986 allows empty paths (not even a /), but servers
		# return 400 if the path in the HTTP request doesn't start
		# with / , so add it here if needed.
		if {[string index $srvurl 0] ne "/"} {
		    set srvurl /$srvurl
		}
		# Check for validity according to RFC 3986, Appendix A
		set validityRE {(?xi)
		    ^
		    # Path part (already must start with / character)
		    (?:	      [-\w.~!$&'()*+,;=:@/]  | %[0-9a-f][0-9a-f] )*
		    # Query part (optional, permits ? characters)
		    (?: \? (?: [-\w.~!$&'()*+,;=:@/?] | %[0-9a-f][0-9a-f] )* )?
		    $
		}
		if {$config(-strict) && ![regexp -- $validityRE $srvurl]} {
		    # Provide a better error message in this error case
		    if {[regexp {(?i)%(?![0-9a-f][0-9a-f])..} $srvurl bad]} {
			return -code error \
			    "illegal encoding character usage \"$bad\" in URL path"
		    }
		    return -code error "illegal characters in URL path"
		}
	    }

	    return [list $proto $user $host $port $srvurl \
		    [string trimleft $fragment "#"]]
	}
    }

    oo::class create Connection {
	variable cfg urlTypes http
	variable binary state meta coding currentsize totalsize querylength
	variable queryoffset type body status httpline connection charset
	variable theURL after socketinfo sock acceptTypes

	constructor {context url defaults options} {
	    interp alias {} [namespace current]::Context {} $context
	    set ns [info object namespace $context]
	    my eval namespace upvar $ns \
		config http urlTypes urlTypes socketmap socketmap
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

	    if {[info exists cfg(-querychannel)]&&[info exists cfg(-query)]} {
		return -code error \
		    "can't use -query and -querychannel options together"
	    }

	    lassign [Context parseURL $url] proto user host port srvurl
	    if {$srvurl eq ""} {
		set srvurl "/"
	    }
	    if {$proto eq ""} {
		set proto "http"
	    }
	    set lower [string tolower $proto]
	    if {![info exists urlTypes($lower)]} {
		return -code error "unsupported URL type \"$proto\""
	    }
	    lassign $urlTypes($lower) defport defcmd
	    if {$port eq ""} {
		set port $defport
	    }

	    # Check for the proxy's opinion
	    catch {
		if {[llength $http(-proxyfilter)]} {
		    lassign [{*}$http(-proxyfilter) $host] phost pport
		}
	    }

	    # OK, now reassemble into a full URL
	    set url ${proto}://
	    if {$user ne ""} {
		append url $user
		append url @
	    }
	    append url $host
	    if {$port != $defport} {
		append url : $port
	    }
	    append url $srvurl
	    # Don't append the fragment!
	    set theURL $url

	    # If a timeout is specified we set up the after event and arrange
	    # for an asynchronous socket connection.

	    set sockopts [list -async]
	    if {$cfg(-timeout) > 0} {
		set after [after $cfg(-timeout) [namespace code {
		    my reset timeout
		}]]
	    }

	    # If we are using the proxy, we must pass in the full URL that
	    # includes the server name.

	    if {[info exists phost] && ($phost ne "")} {
		set srvurl $url
		set targetAddr [list $phost $pport]
	    } else {
		set targetAddr [list $host $port]
	    }
	    # Proxy connections aren't shared among different hosts.
	    set socketinfo $host:$port

	    # Save the accept types at this point to prevent a race condition.
	    # [Bug c11a51c482]
	    set acceptTypes $http(-accept)

	    # See if we are supposed to use a previously opened channel.
	    if {$cfg(-keepalive)} {
		if {[info exists socketmap($socketinfo)]} {
		    if {[catch {fconfigure $socketmap($socketinfo)}]} {
			Log "WARNING: socket for $socketinfo was closed"
			unset socketmap($socketinfo)
		    } else {
			set sock $socketmap($socketinfo)
			Log "reusing socket $sock for $socketinfo"
			catch {fileevent $sock writable {}}
			catch {fileevent $sock readable {}}
		    }
		}
		# don't automatically close this connection socket
		set connection {}
	    }
	    if {![info exists sock]} {
		# Pass -myaddr directly to the socket command
		if {[info exists cfg(-myaddr)]} {
		    lappend sockopts -myaddr $cfg(-myaddr)
		}
		try {
		    set sock [{*}$defcmd {*}$sockopts {*}$targetAddr]
		} on error {msg} {
		    # Something went wrong while trying to establish the
		    # connection.  Clean up after events and such, but DON'T
		    # call the command callback (if available) because we're
		    # going to throw an exception from here instead.
		    my Finish "" 1
		    return -code error $msg
		}
	    }
	    Log "Using $sock for $socketinfo" \
		[expr {$cfg(-keepalive) ? "keepalive" : ""}]
	    if {$cfg(-keepalive)} {
		set socketmap($socketinfo) $sock
	    }

	    if {![info exists phost]} {
		set phost ""
	    }
	    fileevent $sock writable [namespace code [list \
		my Connect $proto $phost $srvurl]]

	    # Wait for the connection to complete.
	    if {![info exists cfg(-command)]} {
		# geturl does EVERYTHING asynchronously, so if the user calls
		# it synchronously, we just do a wait here.
		my wait

		if {![info exists status]} {
		    # If we timed out then Finish has been called and the
		    # users command callback may have cleaned up the token. If
		    # so we end up here with nothing left to do.
		    return
		} elseif {$status eq "error"} {
		    # Something went wrong while trying to establish the
		    # connection.  Clean up after events and such, but DON'T
		    # call the command callback (if available) because we're
		    # going to throw an exception from here instead.
		    return -code error [lindex $error 0]
		}
	    }
	}

	method Connect {proto phost srvurl} {
	    
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
	if {![string is list $value]} {
	    return -code error \
		"bad value for $option ($value), must be command prefix"
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
