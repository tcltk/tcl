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


## set testing defaults:
set ::env(TCL_TZ) :CET

## warm-up (load clock.tcl, system zones, locales, etc.):
clock scan "" -gmt 1
clock scan ""
clock scan "" -timezone :CET
clock scan "" -format "" -locale en

## ------------------------------------------

proc {**STOP**} {args} {
  return -code error -level 4 "**STOP** in [info level [expr {[info level]-2}]] [join $args { }]" 
}

proc _test_get_commands {lst} {
  regsub -all {(?:^|\n)[ \t]*(\#[^\n]*)(?=\n\s*[\{\#])} $lst "\n{\\1}"
}

proc _test_out_total {} {
  upvar _ _

  if {![llength $_(ittm)]} {
    puts ""
    return
  }

  set mintm 0x7fffffff
  set maxtm 0
  set i 0
  foreach tm $_(ittm) {
    if {$tm > $maxtm} {set maxtm $tm; set maxi $i}
    if {$tm < $mintm} {set mintm $tm; set mini $i}
    incr i
  }

  puts [string repeat ** 40]
  puts [format "Total %d cases in %.2f sec.:" [llength $_(itcnt)] [expr {[llength $_(itcnt)] * $_(reptime) / 1000.0}]]
  lset _(m) 0 [format %.6f [expr [join $_(ittm) +]]]
  lset _(m) 2 [expr [join $_(itcnt) +]]
  lset _(m) 4 [format %.3f [expr {[lindex $_(m) 2] / ([llength $_(itcnt)] * $_(reptime) / 1000.0)}]]
  puts $_(m)
  puts "Average:"
  lset _(m) 0 [format %.6f [expr {[lindex $_(m) 0] / [llength $_(itcnt)]}]]
  lset _(m) 2 [expr {[lindex $_(m) 2] / [llength $_(itcnt)]}]
  lset _(m) 4 [expr {[lindex $_(m) 2] * (1000 / $_(reptime))}]
  puts $_(m)
  puts "Min:"
  lset _(m) 0 [lindex $_(ittm) $mini]
  lset _(m) 2 [lindex $_(itcnt) $mini]
  lset _(m) 2 [lindex $_(itrate) $mini]
  puts $_(m)
  puts "Max:"
  lset _(m) 0 [lindex $_(ittm) $maxi]
  lset _(m) 2 [lindex $_(itcnt) $maxi]
  lset _(m) 2 [lindex $_(itrate) $maxi]
  puts $_(m)
  puts [string repeat ** 40]
  puts ""
}

proc _test_run {reptime lst {outcmd {puts $_(r)}}} {
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

proc test-format {{reptime 1000}} {
  _test_run $reptime {
    # Format : date only (in gmt)
    {clock format 1482525936 -format "%Y-%m-%d" -gmt 1}
    # Format : date only (system zone)
    {clock format 1482525936 -format "%Y-%m-%d"}
    # Format : date only (CEST)
    {clock format 1482525936 -format "%Y-%m-%d" -timezone :CET}
    # Format : time only (in gmt)
    {clock format 1482525936 -format "%H:%M" -gmt 1}
    # Format : time only (system zone)
    {clock format 1482525936 -format "%H:%M"}
    # Format : time only (CEST)
    {clock format 1482525936 -format "%H:%M" -timezone :CET}
    # Format : time only (in gmt)
    {clock format 1482525936 -format "%H:%M:%S" -gmt 1}
    # Format : time only (system zone)
    {clock format 1482525936 -format "%H:%M:%S"}
    # Format : time only (CEST)
    {clock format 1482525936 -format "%H:%M:%S" -timezone :CET}
    # Format : default (in gmt)
    {clock format 1482525936 -gmt 1}
    # Format : default (system zone)
    {clock format 1482525936}
    # Format : default (CEST)
    {clock format 1482525936 -timezone :CET}
    # Format : ISO date-time (in gmt, numeric zone)
    {clock format 1246379400 -format "%Y-%m-%dT%H:%M:%S %z" -gmt 1}
    # Format : ISO date-time (system zone, CEST, numeric zone)
    {clock format 1246379400 -format "%Y-%m-%dT%H:%M:%S %z"}
    # Format : ISO date-time (CEST, numeric zone)
    {clock format 1246379400 -format "%Y-%m-%dT%H:%M:%S %z" -timezone :CET}
    # Format : ISO date-time (system zone, CEST)
    {clock format 1246379400 -format "%Y-%m-%dT%H:%M:%S %Z"}
    # Format : julian day with time (in gmt):
    {clock format 1246379415 -format "%J %H:%M:%S" -gmt 1}
    # Format : julian day with time (system zone):
    {clock format 1246379415 -format "%J %H:%M:%S"}

    # Format : locale date-time (en):
    {clock format 1246379415 -format "%x %X" -locale en}
    # Format : locale date-time (de):
    {clock format 1246379415 -format "%x %X" -locale de}

    # Format : locale lookup table month:
    {clock format 1246379400 -format "%b" -locale en -gmt 1}
    # Format : locale lookup 2 tables - month and day:
    {clock format 1246379400 -format "%b %Od" -locale en -gmt 1}

    # Format : dynamic clock value (without converter caches):
    {set i 0; continue}
    {clock format [incr i] -format "%Y-%m-%dT%H:%M:%S" -locale en -gmt 1}
    {puts [clock format $i -format "%Y-%m-%dT%H:%M:%S" -locale en -gmt 1]\n; continue}
    # Format : dynamic clock value (without any converter caches, zone range overflow):
    {set i 0; continue}
    {clock format [incr i 86400] -format "%Y-%m-%dT%H:%M:%S" -locale en -gmt 1}
    {puts [clock format $i -format "%Y-%m-%dT%H:%M:%S" -locale en -gmt 1]\n; continue}

    # Format : dynamic format (cacheable)
    {clock format 1246379415 -format [string trim "%d.%m.%Y %H:%M:%S "] -gmt 1}

    # Format : all (in gmt, locale en)
    {clock format 1482525936 -format "%%a = %a | %%A = %A | %%b = %b | %%h = %h | %%B = %B | %%C = %C | %%d = %d | %%e = %e | %%g = %g | %%G = %G | %%H = %H | %%I = %I | %%j = %j | %%J = %J | %%k = %k | %%l = %l | %%m = %m | %%M = %M | %%N = %N | %%p = %p | %%P = %P | %%Q = %Q | %%s = %s | %%S = %S | %%t = %t | %%u = %u | %%U = %U | %%V = %V | %%w = %w | %%W = %W | %%y = %y | %%Y = %Y | %%z = %z | %%Z = %Z | %%n = %n | %%EE = %EE | %%EC = %EC | %%Ey = %Ey | %%n = %n | %%Od = %Od | %%Oe = %Oe | %%OH = %OH | %%Ok = %Ok | %%OI = %OI | %%Ol = %Ol | %%Om = %Om | %%OM = %OM | %%OS = %OS | %%Ou = %Ou | %%Ow = %Ow | %%Oy = %Oy" -gmt 1 -locale en}
    # Format : all (in CET, locale de)
    {clock format 1482525936 -format "%%a = %a | %%A = %A | %%b = %b | %%h = %h | %%B = %B | %%C = %C | %%d = %d | %%e = %e | %%g = %g | %%G = %G | %%H = %H | %%I = %I | %%j = %j | %%J = %J | %%k = %k | %%l = %l | %%m = %m | %%M = %M | %%N = %N | %%p = %p | %%P = %P | %%Q = %Q | %%s = %s | %%S = %S | %%t = %t | %%u = %u | %%U = %U | %%V = %V | %%w = %w | %%W = %W | %%y = %y | %%Y = %Y | %%z = %z | %%Z = %Z | %%n = %n | %%EE = %EE | %%EC = %EC | %%Ey = %Ey | %%n = %n | %%Od = %Od | %%Oe = %Oe | %%OH = %OH | %%Ok = %Ok | %%OI = %OI | %%Ol = %Ol | %%Om = %Om | %%OM = %OM | %%OS = %OS | %%Ou = %Ou | %%Ow = %Ow | %%Oy = %Oy" -timezone :CET -locale de}
  }
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
    # Scan : greedy match (space separated)
    {clock scan "1 1 1" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "111 1" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "1 111" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "1 11 1" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "1 11 11" -format "%d%m%y" -base 0 -gmt 1}
    {clock scan "11 11 11" -format "%d%m%y" -base 0 -gmt 1}

    # Scan : time (in gmt)
    {clock scan "10:35:55" -format "%H:%M:%S" -base 1000000000 -gmt 1}
    # Scan : time (system time zone, with base)
    {clock scan "10:35:55" -format "%H:%M:%S" -base 1000000000}
    # Scan : time (gmt, without base)
    {clock scan "10:35:55" -format "%H:%M:%S" -gmt 1}
    # Scan : time (system time zone, without base)
    {clock scan "10:35:55" -format "%H:%M:%S"}

    # Scan : date-time (in gmt)
    {clock scan "25.11.2015 10:35:55" -format "%d.%m.%Y %H:%M:%S" -base 0 -gmt 1}
    # Scan : date-time (system time zone with base)
    {clock scan "25.11.2015 10:35:55" -format "%d.%m.%Y %H:%M:%S" -base 0}
    # Scan : date-time (system time zone without base)
    {clock scan "25.11.2015 10:35:55" -format "%d.%m.%Y %H:%M:%S"}

    # Scan : julian day in gmt
    {clock scan 2451545 -format %J -gmt 1}
    # Scan : julian day in system TZ
    {clock scan 2451545 -format %J}
    # Scan : julian day in other TZ
    {clock scan 2451545 -format %J -timezone +0200}
    # Scan : julian day with time:
    {clock scan "2451545 10:20:30" -format "%J %H:%M:%S"}
    # Scan : julian day with time (greedy match):
    {clock scan "2451545 102030" -format "%J%H%M%S"}

    # Scan : century, lookup table month
    {clock scan {1970 Jan 2} -format {%C%y %b %d} -locale en -gmt 1}
    # Scan : century, lookup table month and day (both entries are first)
    {clock scan {1970 Jan 01} -format {%C%y %b %Od} -locale en -gmt 1}
    # Scan : century, lookup table month and day (list scan: entries with position 12 / 31)
    {clock scan {2016 Dec 31} -format {%C%y %b %Od} -locale en -gmt 1}

    # Scan : ISO date-time (CEST)
    {clock scan "2009-06-30T18:30:00+02:00" -format "%Y-%m-%dT%H:%M:%S%z"}
    {clock scan "2009-06-30T18:30:00 CEST" -format "%Y-%m-%dT%H:%M:%S %z"}
    # Scan : ISO date-time (UTC)
    {clock scan "2009-06-30T18:30:00Z" -format "%Y-%m-%dT%H:%M:%S%z"}
    {clock scan "2009-06-30T18:30:00 UTC" -format "%Y-%m-%dT%H:%M:%S %z"}

    # Scan : dynamic format (cacheable)
    {clock scan "25.11.2015 10:35:55" -format [string trim "%d.%m.%Y %H:%M:%S "] -base 0 -gmt 1}

    break

    # Scan : zone only
    {clock scan "CET" -format "%z"}
    {clock scan "EST" -format "%z"}
    {**STOP** : Wed Nov 25 01:00:00 CET 2015}

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

    # Scan : julian day (overflow)
    {catch {clock scan 5373485 -format %J}}

    **STOP**
    # Scan : test rotate of GC objects (format is dynamic, so tcl-obj removed with last reference)
    {set i 0; time { clock scan "[incr i] - 25.11.2015" -format "$i - %d.%m.%Y" -base 0 -gmt 1 } 50}
    # Scan : test reusability of GC objects (format is dynamic, so tcl-obj removed with last reference)
    {set i 50; time { clock scan "[incr i -1] - 25.11.2015" -format "$i - %d.%m.%Y" -base 0 -gmt 1 } 50}
  }
}

proc test {{reptime 1000}} {
  puts ""
  test-format $reptime
  test-scan $reptime
  test-freescan $reptime
  test-other $reptime

  puts \n**OK**
}

test 100; # ms
