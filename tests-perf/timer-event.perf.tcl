#!/usr/bin/tclsh

# ------------------------------------------------------------------------
#
# timer-event.perf.tcl --
# 
#  This file provides performance tests for comparison of tcl-speed
#  of timer events (event-driven tcl-handling).
#
# ------------------------------------------------------------------------
# 
# Copyright (c) 2014 Serg G. Brester (aka sebres)
# 
# See the file "license.terms" for information on usage and redistribution
# of this file.
# 


if {![namespace exists ::tclTestPerf]} {
  source [file join [file dirname [info script]] test-performance.tcl]
}


namespace eval ::tclTestPerf-Timer-Event {

namespace path {::tclTestPerf}

proc test-queue {howmuch} {

  # because of extremely short measurement times by tests below, wait a little bit (warming-up),
  # to minimize influence of the time-gradation (just for better dispersion resp. result-comparison)
  timerate {after 0} 156

  puts "*** $howmuch events ***"
  _test_run 0 [string map [list \$howmuch $howmuch \\# \#] {
    # update / after idle:
    setup {puts [time {after idle {set foo bar}} $howmuch]}
    {update; \# $howmuch idle-events}
    # update idletasks / after idle:
    setup {puts [time {after idle {set foo bar}} $howmuch]}
    {update idletasks; \# $howmuch idle-events}

    # update / after 0:
    setup {puts [time {after 0 {set foo bar}} $howmuch]}
    {update; \# $howmuch timer-events}
    # update / after 1:
    setup {puts [time {after 1 {set foo bar}} $howmuch]; after 1}
    {update; \# $howmuch timer-events}

    # cancel forwards "after idle" / $howmuch idle-events in queue:
    setup {set i 0; time {set ev([incr i]) [after idle {set foo bar}]} $howmuch}
    {set i 0; time {after cancel $ev([incr i])} $howmuch}
    cleanup {update}
    # cancel backwards "after idle" / $howmuch idle-events in queue:
    setup {set i 0; time {set ev([incr i]) [after idle {set foo bar}]} $howmuch}
    {incr i; time {after cancel $ev([incr i -1])} $howmuch}
    cleanup {update}

    # cancel forwards "after 0" / $howmuch timer-events in queue:
    setup {set i 0; time {set ev([incr i]) [after 0 {set foo bar}]} $howmuch}
    {set i 0; time {after cancel $ev([incr i])} $howmuch}
    cleanup {update}
    # cancel backwards "after 0" / $howmuch timer-events in queue:
    setup {set i 0; time {set ev([incr i]) [after 0 {set foo bar}]} $howmuch}
    {incr i; time {after cancel $ev([incr i -1])} $howmuch}
    cleanup {update}
    # end $howmuch events.
  }]
}

proc test-exec {{reptime 1000}} {
  _test_run $reptime {
    # after idle + after cancel
    {after cancel [after idle {set foo bar}]}
    # after 0 + after cancel
    {after cancel [after 0 {set foo bar}]}
    # after idle + update idletasks
    {after idle {set foo bar}; update idletasks}
    # after idle + update
    {after idle {set foo bar}; update}
    # immediate: after 0 + update
    {after 0 {set foo bar}; update}
    # delayed: after 1 + update
    {after 1 {set foo bar}; update}
    # empty update:
    {update}
    # empty update idle tasks:
    {update idletasks}
  }
}

proc test-exec-new {{reptime 1000}} {
  _test_run $reptime {
    # conditional update pure idle only (without window):
    {update -idle}
    # conditional update without idle events:
    {update -noidle}
    # conditional update timers only:
    {update -timer}
    # conditional update AIO only:
    {update -async}

    # conditional vwait with zero timeout: pure idle only (without window):
    {vwait -idle 0 x}
    # conditional vwait with zero timeout: without idle events:
    {vwait -noidle 0 x}
    # conditional vwait with zero timeout: timers only:
    {vwait -timer 0 x}
    # conditional vwait with zero timeout: AIO only:
    {vwait -async 0 x}
  }
}

proc test-long {{reptime 1000}} {
  _test_run $reptime {
    # in-between important event by amount of idle events:
    {time {after idle {after 30}} 10; after 1 {set important 1}; vwait important;}
    cleanup {foreach i [after info] {after cancel $i}}
    # in-between important event (of new generation) by amount of idle events:
    {time {after idle {after 30}} 10; after 1 {after 0 {set important 1}}; vwait important;} 
    cleanup {foreach i [after info] {after cancel $i}}
  }
}

proc test {{reptime 1000}} {
  test-exec $reptime
  if {![catch {update -noidle}]} {
    test-exec-new $reptime
  }
  test-long $reptime

  puts ""
  foreach howmuch { 10000 20000 40000 60000 } {
    test-queue $howmuch
  }

  puts \n**OK**
}

}; # end of ::tclTestPerf-Timer-Event

# ------------------------------------------------------------------------

# if calling direct:
if {[info exists ::argv0] && [file tail $::argv0] eq [file tail [info script]]} {
  array set in {-time 500}
  array set in $argv
  ::tclTestPerf-Timer-Event::test $in(-time)
}
