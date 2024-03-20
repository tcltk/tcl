#!/usr/bin/tclsh

# ------------------------------------------------------------------------
#
# list.perf.tcl --
#
#  This file provides performance tests for comparison of tcl-speed
#  of list facilities.
#
# ------------------------------------------------------------------------
#
# Copyright (c) 2024 Serg G. Brester (aka sebres)
#
# See the file "license.terms" for information on usage and redistribution
# of this file.
#


if {![namespace exists ::tclTestPerf]} {
  source [file join [file dirname [info script]] test-performance.tcl]
}


namespace eval ::tclTestPerf-List {

namespace path {::tclTestPerf}

# regression tests for [bug-da16d15574] (fix for [db4f2843cd]):
proc test-lsearch-regress {{reptime 1000}} {
  _test_run -no-result $reptime {
    # list with 5000 strings with ca. 50 chars elements:
    setup   { set str [join [lrepeat 13 "XXX"] /]; set l [lrepeat 5000 $str]; llength $l }
    # lsearch with no option, found immediatelly :
    { lsearch $l $str }
    # lsearch with -glob, found immediatelly :
    { lsearch -glob $l $str }
    # lsearch with -exact, found immediatelly :
    { lsearch -exact $l $str }
    # lsearch with -dictionary, found immediatelly :
    { lsearch -dictionary $l $str }

    # lsearch with -nocase only, found immediatelly :
    { lsearch -nocase $l $str }
    # lsearch with -nocase -glob, found immediatelly :
    { lsearch -nocase -glob $l $str }
    # lsearch with -nocase -exact, found immediatelly :
    { lsearch -nocase -exact $l $str }
    # lsearch with -nocase -dictionary, found immediatelly :
    { lsearch -nocase -dictionary $l $str }
  }
}

proc test-lsearch-nf-regress {{reptime 1000}} {
  _test_run -no-result $reptime {
    # list with 5000 strings with ca. 50 chars elements:
    setup   { set str [join [lrepeat 13 "XXX"] /]; set sNF $str/NF; set l [lrepeat 5000 $str]; llength $l }
    # lsearch with no option, not found:
    { lsearch $l $sNF }
    # lsearch with -glob, not found:
    { lsearch -glob $l $sNF }
    # lsearch with -exact, not found:
    { lsearch -exact $l $sNF }
    # lsearch with -dictionary, not found:
    { lsearch -dictionary $l $sNF }
  }
}

proc test-lsearch-nc-nf-regress {{reptime 1000}} {
  _test_run -no-result $reptime {
    # list with 5000 strings with ca. 50 chars elements:
    setup   { set str [join [lrepeat 13 "XXX"] /]; set sNF $str/NF; set l [lrepeat 5000 $str]; llength $l }
    # lsearch with -nocase only, not found:
    { lsearch -nocase $l $sNF }
    # lsearch with -nocase -glob, not found:
    { lsearch -nocase -glob $l $sNF }
    # lsearch with -nocase -exact, not found:
    { lsearch -nocase -exact $l $sNF }
    # lsearch with -nocase -dictionary, not found:
    { lsearch -nocase -dictionary $l $sNF }
  }
}

proc test {{reptime 1000}} {
  test-lsearch-regress $reptime
  test-lsearch-nf-regress $reptime
  test-lsearch-nc-nf-regress $reptime

  puts \n**OK**
}

}; # end of ::tclTestPerf-List

# ------------------------------------------------------------------------

# if calling direct:
if {[info exists ::argv0] && [file tail $::argv0] eq [file tail [info script]]} {
  array set in {-time 500}
  array set in $argv
  ::tclTestPerf-List::test $in(-time)
}
