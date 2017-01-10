#!/usr/bin/tclsh
# ------------------------------------------------------------------------
#
# test-performance.tcl --
# 
#  This file provides common performance tests for comparison of tcl-speed
#  degradation by switching between branches.
#  (currently for clock ensemble only)
#
# ------------------------------------------------------------------------
# 
# Copyright (c) 2014 Serg G. Brester (aka sebres)
# 
# See the file "license.terms" for information on usage and redistribution
# of this file.
# 

set ::env(TCL_TZ) :CET

proc {**STOP**} {args} {
  return -code error -level 2 "**STOP** in [info level [expr {[info level]-1}]] [join $args { }]" 
}

proc _test_get_commands {lst} {
  regsub -all {(?:^|\n)[ \t]*(\#[^\n]*)(?=\n\s*[\{\#])} $lst "\n{\\1}"
}

proc test-scan {{reptime 1000}} {
  foreach _(c) [_test_get_commands {
    # Scan : date
    {clock scan "25.11.2015" -format "%d.%m.%Y" -base 0 -gmt 1}
    #return
    #{**STOP** : Wed Nov 25 01:00:00 CET 2015}
    # FreeScan : relative date
    {clock scan "5 years 18 months 385 days" -base 0 -gmt 1}
    # FreeScan : relative date with relative weekday
    {clock scan "5 years 18 months 385 days Fri" -base 0 -gmt 1}
    # FreeScan : relative date with ordinal month
    {clock scan "5 years 18 months 385 days next 1 January" -base 0 -gmt 1}
    # FreeScan : relative date with ordinal month and relative weekday
    {clock scan "5 years 18 months 385 days next January Fri" -base 0 -gmt 1}
    # FreeScan : ordinal month
    {clock scan "next January" -base 0 -gmt 1}
    # FreeScan : relative week
    {clock scan "next Fri" -base 0 -gmt 1}
    # FreeScan : relative weekday and week offset 
    {clock scan "next January + 2 week" -base 0 -gmt 1}
    # FreeScan : time only with base
    {clock scan "19:18:30" -base 148863600 -gmt 1}
    # FreeScan : time only without base, gmt
    {clock scan "19:18:30" -gmt 1}
    # FreeScan : time only without base, system
    {clock scan "19:18:30"}
    # FreeScan : date, system time zone
    {clock scan "05/08/2016 20:18:30"}
    # FreeScan : date, supplied time zone
    {clock scan "05/08/2016 20:18:30" -timezone :CET}
    # FreeScan : date, supplied gmt (equivalent -timezone :GMT)
    {clock scan "05/08/2016 20:18:30" -gmt 1}
    # FreeScan : date, supplied time zone gmt
    {clock scan "05/08/2016 20:18:30" -timezone :GMT}
    # FreeScan : time only, numeric zone in string, base time gmt (exchange zones between gmt / -0500)
    {clock scan "20:18:30 -0500" -base 148863600 -gmt 1}
    # FreeScan : time only, zone in string (exchange zones between system / gmt)
    {clock scan "19:18:30 GMT" -base 148863600}
    # FreeScan : fast switch of zones in cycle - GMT, MST, CET (system) and EST
    {clock scan "19:18:30 MST" -base 148863600 -gmt 1
     clock scan "19:18:30 EST" -base 148863600
    }
  }] {
    puts "% [regsub -all {\n[ \t]*} $_(c) {; }]"
    if {[regexp {\s*\#} $_(c)]} continue
    puts [clock format [if 1 $_(c)] -locale en]
    puts [timerate $_(c) $reptime]
    puts ""
  }
}

proc test-other {{reptime 1000}} {
  foreach _(c) [_test_get_commands {
    # Bad zone
    {catch {clock scan "1 day" -timezone BAD_ZONE -locale en}}
    # Scan : test rotate of GC objects (format is dynamic, so tcl-obj removed with last reference)
    {set i 0; time { clock scan "[incr i] - 25.11.2015" -format "$i - %d.%m.%Y" -base 0 -gmt 1 } 50}
    # Scan : test reusability of GC objects (format is dynamic, so tcl-obj removed with last reference)
    {set i 50; time { clock scan "[incr i -1] - 25.11.2015" -format "$i - %d.%m.%Y" -base 0 -gmt 1 } 50}
  }] {
    puts "% [regsub -all {\n[ \t]*} $_(c) {; }]"
    if {[regexp {\s*\#} $_(c)]} continue
    puts [if 1 $_(c)]
    puts [timerate $_(c) $reptime]
    puts ""
  }
}

proc test {{reptime 1000}} {
  puts ""
  test-scan $reptime
  test-other $reptime

  puts \n**OK**
}

test 250; # 250 ms
