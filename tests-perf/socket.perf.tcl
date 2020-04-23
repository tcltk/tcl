#!/usr/bin/tclsh

# ------------------------------------------------------------------------
#
# socket.perf.tcl --
#
#  This file provides performance tests for comparison of tcl-speed
#  of socket or channel handling.
#
# ------------------------------------------------------------------------
#
# Copyright (c) 2014 Serg G. Brester (aka sebres)
#
# See the file "license.terms" for information on usage and redistribution
# of this file.
#


## common test performance framework:
if {![namespace exists ::tclTestPerf]} {
  source [file join [file dirname [info script]] test-performance.tcl]
}

namespace eval ::tclTestPerf-Socket {

namespace path {::tclTestPerf}

## init:

proc create-server {} {
  upvar ::in in
  package require Thread
  # random port if needed:
  if {$in(-port) eq ""} {
     set in(-port) [lindex [fconfigure [set srv [socket -server {} 0]] -sockname] 2]
     close $srv
  }
  # helper worker for server :
  set in(-srv-worker) [thread::create]
  thread::send -async $in(-srv-worker) [list array set in [array get in]]
  set ip [thread::send $in(-srv-worker) {
    proc _accept {ch addr port} {
      chan configure $ch -blocking 0
      chan even $ch readable [list _read $ch]
      chan even $ch writable [list _write $ch]
    }
    proc _read {ch} {
      if {[catch { string length [read $ch] } dl] || !$dl || [eof $ch]} {
        close $ch
      }
    }
    set wrbuf [string repeat "X" 4096]
    proc _write {ch} {
      if {[catch { puts -nonewline $ch $::wrbuf }] || [eof $ch]} {
        close $ch
      }
    }
    set srv [socket -server _accept $in(-port)]
    # return IP of server to tester:
    lindex [fconfigure $srv -sockname] 0
  }]
  if {$in(-server) eq ""} {
    set in(-server) $ip
  }
  puts " ** working on '$in(-server):$in(-port)' ..."
}

proc stop-server {} {
  upvar ::in in
  thread::send -async $in(-srv-worker) { close $srv; update; thread::release }
  catch { thread::release -wait $in(-srv-worker) }
}

## ------------------------------------------

proc test-socket-connect {{reptime 1000}} {
  _test_run $reptime {
    setup {set i 0}
    # socket : simple connect:
    {set s([incr i]) [socket $::in(-server) $::in(-port)]}
    cleanup {while {$i > 0} {close $s($i); incr i -1}; unset s}
    # socket : simple connect / close:
    {close [socket $::in(-server) $::in(-port)]}
  }
}

proc test-socket-throughput {{reptime 1000}} {
  _test_run -no-result $reptime {
    setup {set s [socket $::in(-server) $::in(-port)]; string length [set wrbuf [string repeat "Z" 4096]]}

    # read 4K:
    {read $s 4096}
    # write 4K:
    {puts -nonewline $s $wrbuf}
    
    cleanup {close $s}
  }
}

proc test {{reptime 1000}} {
  puts ""
  test-socket-connect $reptime
  test-socket-throughput $reptime

  puts \n**OK**
}

}; # end of ::tclTestPerf-Socket

# ------------------------------------------------------------------------

# if calling direct:
if {[info exists ::argv0] && [file tail $::argv0] eq [file tail [info script]]} {
  array set in {-time "1000 500" -server "" -port ""}
  array set in $argv 
  if {$in(-server) eq "" || $in(-port) eq ""} { ::tclTestPerf-Socket::create-server }
  ::tclTestPerf-Socket::test $in(-time)
  if {[info exists in(-srv-worker)]} { ::tclTestPerf-Socket::stop-server }
}