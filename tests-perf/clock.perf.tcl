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

proc _test_out_total {} {
  upvar _ _

  puts [string repeat ** 40]
  puts [format "Total %d cases in %.2f sec.:" [llength $_(itcnt)] [expr {[llength $_(itcnt)] * $_(reptime) / 1000.0}]]
  lset _(m) 0 [format %.6f [expr [join $_(ittm) +]]]
  lset _(m) 2 [expr [join $_(itcnt) +]]
  lset _(m) 4 [expr {[lindex $_(m) 2] / ([llength $_(itcnt)] * $_(reptime) / 1000.0)}]
  puts $_(m)
  puts "Average:"
  lset _(m) 0 [format %.6f [expr {[lindex $_(m) 0] / [llength $_(itcnt)]}]]
  lset _(m) 2 [expr {[lindex $_(m) 2] / [llength $_(itcnt)]}]
  lset _(m) 4 [expr {[lindex $_(m) 2] * (1000 / $_(reptime))}]
  puts $_(m)
  puts [string repeat ** 40]
  puts ""
}

proc _test_run {reptime lst {outcmd {puts {$_(r)}}}} {
  upvar _ _
  array set _ [list ittm {} itcnt {} itrate {} reptime $reptime]

  foreach _(c) [_test_get_commands $lst] {
    puts "% [regsub -all {\n[ \t]*} $_(c) {; }]"
    if {[regexp {\s*\#} $_(c)]} continue
    set _(r) [if 1 $_(c)]
    if {$outcmd ne {}} $outcmd
    puts [set _(m) [timerate $_(c) $reptime]]
    lappend _(ittm) [lindex $_(m) 0]
    lappend _(itcnt) [lindex $_(m) 2]
    lappend _(itrate) [lindex $_(m) 4]
    puts ""
  }
  _test_out_total
}

proc test-scan {{reptime 1000}} {
  _test_run $reptime {
    # Scan : date (in gmt)
    {clock scan "25.11.2015" -format "%d.%m.%Y" -base 0 -gmt 1}
    # Scan : date (system time zone, with base)
    {clock scan "25.11.2015" -format "%d.%m.%Y" -base 0}
    # Scan : date (system time zone, without base)
    {clock scan "25.11.2015" -format "%d.%m.%Y"}
    # Scan : greedy match
    {clock scan "111" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "1111" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "11111" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "111111" -format "%d%m%y" -base 0 -gmt 1}

    # Scan : date-time (in gmt)
    {clock scan "25.11.2015 10:35:55" -format "%d.%m.%Y %H:%M:%S" -base 0 -gmt 1}
    # Scan : date-time (system time zone with base)
    {clock scan "25.11.2015 10:35:55" -format "%d.%m.%Y %H:%M:%S" -base 0}
    # Scan : date-time (system time zone without base)
    {clock scan "25.11.2015 10:35:55" -format "%d.%m.%Y %H:%M:%S"}

    # Scan : dynamic format (cacheable)
    {clock scan "25.11.2015 10:35:55" -format [string trim "%d.%m.%Y %H:%M:%S "] -base 0 -gmt 1}

    # Scan : zone only
    {clock scan "CET" -format "%z"}
    {clock scan "EST" -format "%z"}
      #{**STOP** : Wed Nov 25 01:00:00 CET 2015}

    # # Scan : long format test (allock chain)
    # {clock scan "25.11.2015" -format "%d.%m.%Y %d.%m.%Y %d.%m.%Y %d.%m.%Y %d.%m.%Y %d.%m.%Y %d.%m.%Y %d.%m.%Y" -base 0 -gmt 1}
    # # Scan : dynamic, very long format test (create obj representation, allock chain, GC, etc):
    # {clock scan "25.11.2015" -format [string repeat "[incr i] %d.%m.%Y %d.%m.%Y" 10] -base 0 -gmt 1}
    # # Scan : again:
    # {clock scan "25.11.2015" -format [string repeat "[incr i -1] %d.%m.%Y %d.%m.%Y" 10] -base 0 -gmt 1}
  } {puts [clock format $_(r) -locale en]}
}

proc test-freescan {{reptime 1000}} {
  _test_run $reptime {
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
  } {puts [clock format $_(r) -locale en]}
}

proc test-other {{reptime 1000}} {
  _test_run $reptime {
    # Bad zone
    {catch {clock scan "1 day" -timezone BAD_ZONE -locale en}}
    **STOP**
    # Scan : test rotate of GC objects (format is dynamic, so tcl-obj removed with last reference)
    {set i 0; time { clock scan "[incr i] - 25.11.2015" -format "$i - %d.%m.%Y" -base 0 -gmt 1 } 50}
    # Scan : test reusability of GC objects (format is dynamic, so tcl-obj removed with last reference)
    {set i 50; time { clock scan "[incr i -1] - 25.11.2015" -format "$i - %d.%m.%Y" -base 0 -gmt 1 } 50}
  }
}

proc test {{reptime 1000}} {
  puts ""
  test-scan $reptime
  #test-freescan $reptime
  test-other $reptime

  puts \n**OK**
}

test 250; # 250 ms
