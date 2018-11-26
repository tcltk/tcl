#!/bin/sh
#\
exec tclsh "$0" ${1+"$@"}

#----------------------------------------------------------------------
#
# installVfs.tcl --
#
#        This file wraps the /library file system around a binary
#
#----------------------------------------------------------------------
#
# Copyright (c) 2018 by Sean Woods.  All rights reserved.
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#----------------------------------------------------------------------

proc mapDir {resultvar prefix filepath} {
    upvar 1 $resultvar result
    if {![info exists result]} {
      set result {}
    }
    set queue [list $prefix $filepath]
    while {[llength $queue]} {
      set queue [lassign $queue qprefix qpath]
      foreach ftail [glob -directory $qpath -nocomplain -tails *] {
          set f [file join $qpath $ftail]
          if {[file isdirectory $f]} {
            if {$ftail eq "CVS"} continue
            lappend queue [file join $qprefix $ftail] $f
          } elseif {[file isfile $f]} {
              if {$ftail eq "pkgIndex.tcl"} continue
              if {$ftail eq "manifest.txt"} {
                lappend result $f [file join $qprefix pkgIndex.tcl]
              } else {
                lappend result $f [file join $qprefix $ftail]
              }
          }
       }
    }
}
if {[llength $argv]<4} {
  error "Usage: [file tail [info script]] IMG_OUTPUT IMG_INPUT PREFIX FILE_SYSTEM ?PREFIX FILE_SYSTEM?..."
}

set paths [lassign $argv IMG_OUTPUT IMG_INPUT]
foreach {prefix fpath} $paths {
  mapDir files $prefix [file normalize $fpath]
}
if {$IMG_INPUT eq {}} {
  zipfs lmkzip $IMG_OUTPUT $files
} else {
  zipfs lmkimg $IMG_OUTPUT $files {} $IMG_INPUT
}
