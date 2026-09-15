# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.tcl" when running tcltest
# in this directory.
#
# Copyright © 1998-1999 Scriptics Corporation.
# Copyright © 2000 Ajuba Solutions
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

package prefer latest
package require tcltest 2.5
namespace import ::tcltest::*

configure -testdir [file normalize [file dirname [info script]]] {*}$argv

if {[singleProcess]} {
    interp debug {} -frame 1
}

configure -load {
    catch {
        set s [socket -server {} 0]
        foreach {host addr port} [chan configure $s -sockname] {
            if {[string match "*::*" $addr]} {
                set ::tcltest_ipv6 $addr
            } else {
                set ::tcltest_ipv4 $addr
            }
        }
        chan close $s
    }

    foreach {var key def} {::tcltest_ipv4 TCLTEST_IPV4 "127.0.0.1"
                           ::tcltest_ipv6 TCLTEST_IPV6 "::1"} {
        if {![info exists $var]} {
            if {[info exists ::env($key)]} {
                set $var $::env($key)
            } else {
                set $var $def
            }
        }
    }
}

set ErrorOnFailures [info exists env(ERROR_ON_FAILURES)]
unset -nocomplain env(ERROR_ON_FAILURES)
if {[runAllTests] && $ErrorOnFailures} {exit 1}
# if calling direct only (avoid rewrite exit if inlined or interactive):
if { [info exists ::argv0] && [file tail $::argv0] eq [file tail [info script]]
  && !([info exists ::tcl_interactive] && $::tcl_interactive)
} {
    proc exit args {}
}
