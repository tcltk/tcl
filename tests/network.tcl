# This file contains Tcl code with facilities to test network stack of tcl. 
# This is used by some of the tests in io.test, chanio.test etc.
#
# Copyright © 1995-1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# DiscoverLocalAddresses --
#
#	Find the localhost addresses for the IPv4 and IPv6 families.
#
#	We use the following heuristics to try to determine the best address
#
#	1. contents of the TCLTEST_IPV4 and TCLTEST_IPV6 environment variables
#	2. addresses on a server socket opened with -myaddr localhost
#	3. addresses on a server socket opened without a -myaddr parameter
#          (can resolve to 0.0.0.0 and ::, which may be affected by firewalls
#	    more aggressively than 127.0.0.1 and ::1)
# 	4. 127.0.0.1 and ::1
#
# Arguments:
#	none.
#
# Results:
#	none.
#
# Side Effects:
#	Rewrites localIPv4Address and localIPv6Address on the first call.
#

proc DiscoverLocalAddresses {} {
    # phase 1:
    if {[info exists ::env(TCLTEST_IPV4)]} {
	set ipv4 $::env(TCLTEST_IPV4)
    }
    if {[info exists ::env(TCLTEST_IPV6)]} {
	set ipv6 $::env(TCLTEST_IPV6)
    }
    if {![info exists ipv4] || ![info exists ipv6]} {
    	# phase 2 & 3:
	foreach opts {
	    {-myaddr localhost}
	    {}
	} {
	    catch {
		set s [socket {*}$opts -server {} 0]
		foreach {addr host port} [chan configure $s -sockname] {
		    if {[string match "*:*" $addr]} {
			if {![info exists ipv6]} {
			    set ipv6 $addr
			}
		    } else {
			if {![info exists ipv4]} {
			    set ipv4 $addr
			}
		    }
		}
		chan close $s
	    }
	    if {[info exists ipv4] && [info exists ipv6]} break
	}
	# fallback to phase 4:
	if {![info exists ipv4]} {
	    set ipv4 {127.0.0.1}
	}
	if {![info exists ipv6]} {
	    set ipv6 {::1}
	}
    }
    # set constants to procs:
    proc localIPv4Address {} [list return $ipv4]
    proc localIPv6Address {} [list return $ipv6]
}

# localIPv4Address --
#
#	Get the local address for the IPv4 family
#
# Arguments:
#	none.
#
# Results:
#	The address
#
# Side Effects:
#	May increase compiler epoch after first call.
#
proc localIPv4Address {} {
    DiscoverLocalAddresses
    localIPv4Address
}

# localIPv6Address --
#
#	Get the local address for the IPv6 family
#
# Arguments:
#	none.
#
# Results:
#	The address
#
# Side Effects:
#	May increase compiler epoch after first call.
#
proc localIPv6Address {} {
    DiscoverLocalAddresses
    localIPv6Address
}
