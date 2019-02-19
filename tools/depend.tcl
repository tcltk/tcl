#!/usr/local/bin/tclsh
# -----------------------------------------------------------------------------
#
# depend.tcl --
#
# This script generates a set of dependencies in format of make include-file.
#
# Usage:
#
#   tclsh tools/depend.tcl ?-source path? ?-subdirs list? ?-target unix|win? ??-out? target-dir/depend.mk?
#
# Examples:
#
#   tclsh tools/depend.tcl
#   tclsh tools/depend.tcl unix/depend.mk
#   tclsh $TCL/tools/depend.tcl -source .. -subdirs 'generic' ./depend.mk
#
# Copyright (c) 2007- Sergey G. Brester aka sebres.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# -----------------------------------------------------------------------------

namespace eval ::genDepend {

proc out {args} {
  variable in
  if {$in(outf) ne ""} {
    ::puts {*}[lrange $args 0 end-1] $in(outf) [lindex $args end]
  } else {
    puts {*}$args
  }
}

proc grep_dep {hlst fn {size {}}} {
  variable in
  set d {}
  foreach fn $fn {
    foreach fn [glob -nocomplain [file join $in(-source) $fn]] {
      #puts -nonewline "    $fn ... "
      set f [open $fn r]
      set body [read $f {*}$size]
      close $f
      foreach h $hlst {
        if {[regexp "(?:^|\\n)\\s*\[#\]\\s*include\\s+\"[string map {. \\.} $h]\"" $body]} {
          dict lappend d $fn $h
        }
      }
      #puts [if {[dict exists $d $fn]} {dict get $d $fn}]
    }
  }
  return $d
}

proc get_depend_lst {_d hlst m} {
  upvar $_d d
  variable in
  set fnlst {}
  foreach p $in(-subdirs) {
    lappend fnlst $p/$m
  }
  set d [grep_dep $hlst $fnlst]
}

proc out_depend_lst {dd} {
  set last ""
  foreach {fn lst} [lsort -dictionary -stride 2 -index 0 $dd] {
    set dir [file dirname $fn]
    if {$dir ne $last} {
      set last $dir
      out "### -- [file tail $dir] --"
    }
    set fn [file tail $fn]
    regsub {.c$} $fn ".\$(OBJEXT)" fn
    out "$fn : $lst"
  }
}

# -----------------------------------------------------------------------------

proc generate {args} {

  if {![llength $args]} {
    # build for tcl and both platforms:
    set src [file dirname [file dirname [info script]]]
    generate -source $src $src/unix/depend.mk
    puts ""
    generate -source $src $src/win/depend.mk
    puts "\nDONE."
    return
  }

  variable in
  array set in {-source . -subdirs {} -target {} -out {} outf {}}
  if {[llength $args] & 1} {
    set in(-out) [lindex $args end]
    set args [lrange $args 0 end-1]
  }
  array set in $args

  if {$in(-out) ne ""} {
    set in(-out) [file normalize $in(-out)]
  }
  if {$in(-target) eq ""} {
    set in(-target) [file tail [file dirname $in(-out)]]
  }
  if {$in(-subdirs) eq ""} {
    set in(-subdirs) [list generic $in(-target)]
  }

  # ----------------------------

  if {$in(-out) ne ""} {
    puts "Build into $in(-out) for target \"$in(-target)\""
    puts "Scanning \"[join $in(-subdirs) "\", \""]\" (in \"[file normalize $in(-source)]\") for dependencies ..."
    set in(outf) [open $in(-out) w]
    fconfigure $in(outf) -translation lf 
  }
  try {

    set hlst {}
    foreach p $in(-subdirs) {
      lappend hlst {*}[glob -nocomplain -tail -directory [file join $in(-source) $p] *.h]
    }
    set hlst [lsort -dictionary $hlst]
    #puts "  headers found: $hlst"

    out "#"
    out "# Dependencies:"
    out "#"
    out "# This file was automatically generated using helper \"[file tail [info script]]\" for target \"$in(-target)\"."
    out "#"

    get_depend_lst dh $hlst *.h

    out "\n## header includes:\n"
    out_depend_lst $dh

    get_depend_lst dc $hlst *.c

    out "\n## includes:\n"
    out_depend_lst $dc

    out "\n# / end of dependencies."

  } finally {
    if {$in(outf) ne {}} {
      close $in(outf)
    }
  }
  if {$in(outf) ne {}} {
    puts "[llength $hlst] headers and [dict size $dh] + [dict size $dc] includes found."
  }
}

}; #end of namespace genDepend

# -----------------------------------------------------------------------------

if {[info exists argv0] && [file normalize [info script]] eq [file normalize $argv0]} {

  ## allow inlining in web-env's:
  if {[info level] > 0 || [namespace current] ne "::"} {
    set argv0 [info script]
    if {![info exists argv]} { set argv {} }
    namespace eval ::genDepend [list interp alias {} ::genDepend::puts {} [namespace current]::puts]
  }

  # executed via command line or in web-env:
  ::genDepend::generate {*}$argv
}
