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

proc test-scan {{rep 100000}} {
  foreach {comment c} {
    "# FreeScan : relative date"
    {clock scan "5 years 18 months 385 days" -base 0 -gmt 1}
    "# FreeScan : relative date with relative weekday"
    {clock scan "5 years 18 months 385 days Fri" -base 0 -gmt 1}
    "# FreeScan : relative date with ordinal month"
    {clock scan "5 years 18 months 385 days next 1 January" -base 0 -gmt 1}
    "# FreeScan : relative date with ordinal month and relative weekday"
    {clock scan "5 years 18 months 385 days next January Fri" -base 0 -gmt 1}
    "# FreeScan : ordinal month"
    {clock scan "next January" -base 0 -gmt 1}
    "# FreeScan : relative week"
    {clock scan "next Fri" -base 0 -gmt 1}
    "# FreeScan : relative weekday and week offset "
    {clock scan "next January + 2 week" -base 0 -gmt 1}
    "# FreeScan : time only with base"
    {clock scan "19:18:30" -base 148863600 -gmt 1}
    "# FreeScan : time only without base"
    {clock scan "19:18:30" -gmt 1}
    "# FreeScan : date, system time zone"
    {clock scan "05/08/2016 20:18:30"}
    "# FreeScan : date, supplied time zone"
    {clock scan "05/08/2016 20:18:30" -timezone :CET}
    "# FreeScan : date, supplied gmt (equivalent -timezone :GMT)"
    {clock scan "05/08/2016 20:18:30" -gmt 1}
    "# FreeScan : time only, numeric zone in string, base time gmt (exchange zones between gmt / -0500)"
    {clock scan "20:18:30 -0500" -base 148863600 -gmt 1}
    "# FreeScan : time only, zone in string (exchange zones between system / gmt)"
    {clock scan "19:18:30 GMT" -base 148863600}
  } {
    if {[string first "**STOP**" $comment] != -1} { return -code error "**STOP**" }
    puts "\n% $comment\n% $c"
    puts [clock format [{*}$c] -locale en]
    puts [time $c $rep]
  }
}

proc test-other {{rep 100000}} {
  foreach {comment c} {
    "# Bad zone"
    {catch {clock scan "1 day" -timezone BAD_ZONE}}
  } {
    if {[string first "**STOP**" $comment] != -1} { return -code error "**STOP**" }
    puts "\n% $comment\n% $c"
    puts [if 1 $c]
    puts [time $c $rep]
  }
}

set factor 10; # 50
if 1 {;#
  test-scan [expr 10000 * $factor]
  test-other [expr 5000 * $factor]

  puts \n**OK**
};#