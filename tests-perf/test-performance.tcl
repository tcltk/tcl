# ------------------------------------------------------------------------
#
# test-performance.tcl --
# 
#  This file provides common performance tests for comparison of tcl-speed
#  degradation or regression by switching between branches.
#
#  To execute test case evaluate direct corresponding file "tests-perf\*.perf.tcl".
#
# ------------------------------------------------------------------------
# 
# Copyright (c) 2014 Serg G. Brester (aka sebres)
# 
# See the file "license.terms" for information on usage and redistribution
# of this file.
# 

namespace eval ::tclTestPerf {
# warm-up interpeter compiler env, calibrate timerate measurement functionality:

# if no timerate here - import from unsupported:
if {[namespace which -command timerate] eq {}} {
  namespace inscope ::tcl::unsupported {namespace export timerate}
  namespace import ::tcl::unsupported::timerate
}

# if not yet calibrated:
if {[lindex [timerate {} 10] 6] >= (10-1)} {
  puts -nonewline "Calibration ... "; flush stdout
  puts "done: [lrange \
    [timerate -calibrate {}] \
  0 1]"
}

proc {**STOP**} {args} {
  return -code error -level 4 "**STOP** in [info level [expr {[info level]-2}]] [join $args { }]" 
}

proc _test_get_commands {lst} {
  regsub -all {(?:^|\n)[ \t]*(\#[^\n]*|\msetup\M[^\n]*|\mcleanup\M[^\n]*)(?=\n\s*(?:[\{\#]|setup|cleanup|$))} $lst "\n{\\1}"
}

proc _test_out_total {} {
  upvar _ _

  set tcnt [llength $_(itm)]
  if {!$tcnt} {
    puts ""
    return
  }

  set mintm 0x7fffffff
  set maxtm 0
  set nett 0
  set wtm 0
  set wcnt 0
  set i 0
  foreach tm $_(itm) {
    if {[llength $tm] > 6} {
      set nett [expr {$nett + [lindex $tm 6]}]
    }
    set wtm [expr {$wtm + [lindex $tm 0]}]
    set wcnt [expr {$wcnt + [lindex $tm 2]}]
    set tm [lindex $tm 0]
    if {$tm > $maxtm} {set maxtm $tm; set maxi $i}
    if {$tm < $mintm} {set mintm $tm; set mini $i}
    incr i
  }

  puts [string repeat ** 40]
  set s [format "%d cases in %.2f sec." $tcnt [expr {([clock milliseconds] - $_(starttime)) / 1000.0}]]
  if {$nett > 0} {
    append s [format " (%.2f nett-sec.)" [expr {$nett / 1000.0}]]
  }
  puts "Total $s:"
  lset _(m) 0 [format %.6f $wtm]
  lset _(m) 2 $wcnt
  lset _(m) 4 [format %.3f [expr {$wcnt / (($nett ? $nett : ($tcnt * $_(reptime))) / 1000.0)}]]
  if {[llength $_(m)] > 6} {
    lset _(m) 6 [format %.3f $nett]
  }
  puts $_(m)
  puts "Average:"
  lset _(m) 0 [format %.6f [expr {[lindex $_(m) 0] / $tcnt}]]
  lset _(m) 2 [expr {[lindex $_(m) 2] / $tcnt}]
  if {[llength $_(m)] > 6} {
    lset _(m) 6 [format %.3f [expr {[lindex $_(m) 6] / $tcnt}]]
    lset _(m) 4 [format %.0f [expr {[lindex $_(m) 2] / [lindex $_(m) 6] * 1000}]]
  }
  puts $_(m)
  puts "Min:"
  puts [lindex $_(itm) $mini]
  puts "Max:"
  puts [lindex $_(itm) $maxi]
  puts [string repeat ** 40]
  puts ""
}

proc _test_run {reptime lst {outcmd {puts $_(r)}}} {
  upvar _ _
  array set _ [list itm {} reptime $reptime starttime [clock milliseconds]]

  foreach _(c) [_test_get_commands $lst] {
    puts "% [regsub -all {\n[ \t]*} $_(c) {; }]"
    if {[regexp {^\s*\#} $_(c)]} continue
    if {[regexp {^\s*(?:setup|cleanup)\s+} $_(c)]} {
      puts [if 1 [lindex $_(c) 1]]
      continue
    }
    if {$reptime > 1} {; #if not once:
      set _(r) [if 1 $_(c)]
      if {$outcmd ne {}} $outcmd
    }
    puts [set _(m) [timerate $_(c) $reptime]]
    lappend _(itm) $_(m)
    puts ""
  }
  _test_out_total
}

}; # end of namespace ::tclTestPerf
