# -*- tcl -*-
# ### ### ### ######### ######### #########
## Copyright (c) 2008-2009 ActiveState Software Inc.
##                         Andreas Kupries
## Copyright (C) 2009 Pat Thoyts <patthoyts@users.sourceforge.net>
## Copyright (C) 2014 Sean Woods <yoda@etoyoc.com>
##
## BSD License
##
# Package providing commands for:
# * the generation of a zip archive,
# * building a zip archive from a file system
# * building a file system from a zip archive

package require Tcl 8.6
# Cop
#
#        Create ZIP archives in Tcl.
#
# Create a zipkit using mkzip filename.zkit -zipkit -directory xyz.vfs
# or a zipfile using mkzip filename.zip -directory dirname -exclude "*~"
#

namespace eval ::zvfs {}

proc ::zvfs::setbinary chan {
  fconfigure $chan \
      -encoding    binary \
      -translation binary \
      -eofchar     {}

}

# zip::timet_to_dos
#
#        Convert a unix timestamp into a DOS timestamp for ZIP times.
#
#   DOS timestamps are 32 bits split into bit regions as follows:
#                  24                16                 8                 0
#   +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
#   |Y|Y|Y|Y|Y|Y|Y|m| |m|m|m|d|d|d|d|d| |h|h|h|h|h|m|m|m| |m|m|m|s|s|s|s|s|
#   +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
#
proc ::zvfs::timet_to_dos {time_t} {
    set s [clock format $time_t -format {%Y %m %e %k %M %S}]
    scan $s {%d %d %d %d %d %d} year month day hour min sec
    expr {(($year-1980) << 25) | ($month << 21) | ($day << 16) 
          | ($hour << 11) | ($min << 5) | ($sec >> 1)}
}

# zip::pop --
#
#        Pop an element from a list
#
proc ::zvfs::pop {varname {nth 0}} {
    upvar $varname args
    set r [lindex $args $nth]
    set args [lreplace $args $nth $nth]
    return $r
}

# zip::walk --
#
#        Walk a directory tree rooted at 'path'. The excludes list can be
#        a set of glob expressions to match against files and to avoid.
#        The match arg is internal.
#        eg: walk library {CVS/* *~ .#*} to exclude CVS and emacs cruft.
#
proc ::zvfs::walk {base {excludes ""} {match *} {path {}}} {
    set result {}
    set imatch [file join $path $match]
    set files [glob -nocomplain -tails -types f -directory $base $imatch]
    foreach file $files {
        set excluded 0
        foreach glob $excludes {
            if {[string match $glob $file]} {
                set excluded 1
                break
            }
        }
        if {!$excluded} {lappend result $file}
    }
    foreach dir [glob -nocomplain -tails -types d -directory $base $imatch] {
        lappend result $dir
        set subdir [walk $base $excludes $match $dir]
        if {[llength $subdir]>0} {
            set result [concat $result [list $dir] $subdir]
        }
    }
    return $result
}

# zvfs::mkzip --
#
#        Create a zip archive in 'filename'. If a file already exists it will be
#        overwritten by a new file. If '-directory' is used, the new zip archive
#        will be rooted in the provided directory.
#        -runtime can be used to specify a prefix file. For instance, 
#        zip myzip -runtime unzipsfx.exe -directory subdir
#        will create a self-extracting zip archive from the subdir/ folder.
#        The -comment parameter specifies an optional comment for the archive.
#
#        eg: zip my.zip -directory Subdir -runtime unzipsfx.exe *.txt
# 
proc ::zvfs::mkzip {filename args} {
  array set opts {
      -zipkit 0 -runtime "" -comment "" -directory ""
      -exclude {CVS/* */CVS/* *~ ".#*" "*/.#*"}
  }
  
  while {[string match -* [set option [lindex $args 0]]]} {
      switch -exact -- $option {
          -zipkit  { set opts(-zipkit) 1 }
          -comment { set opts(-comment) [encoding convertto utf-8 [pop args 1]] }
          -runtime { set opts(-runtime) [pop args 1] }
          -directory {set opts(-directory) [file normalize [pop args 1]] }
          -exclude {set opts(-exclude) [pop args 1] }
          -- { pop args ; break }
          default {
              break
          }
      }
      pop args
  }

  set zf [::open $filename wb]
  setbinary $zf
  if {$opts(-runtime) ne ""} {
      set rt [::open $opts(-runtime) rb]
      setbinary $rt
      fcopy $rt $zf
      close $rt
  } elseif {$opts(-zipkit)} {
      set zkd {#!/usr/bin/env tclsh
# This is a zip-based Tcl Module
if {![package vsatisfies [package provide zvfs] 1.0]} {
  package require vfs::zip
  vfs::zip::Mount [info script] [info script]
} else {
  zvfs::mount [info script] [info script]
}
# Load any CLIP file present
if {[file exists [file join [info script] pkgIndex.tcl]] } {
  set dir [info script]
  source [file join [info script] pkgIndex.tcl]
}
# Run any main.tcl present
if {[file exists [file join [info script] main.tcl]] } {
  source [file join [info script] main.tcl]
}
      }
      ::append zkd \x1A
      puts -nonewline $zf $zkd
  }
  close $zf

  set count 0
  set cd ""

  if {$opts(-directory) ne ""} {
    set paths [walk $opts(-directory) $opts(-exclude)]
    set zippathlist {}
    foreach path $paths {
      if {[file isdirectory [file join $opts(-directory) $path]]} continue
      lappend zippathlist [file join $opts(-directory) $path] $path
    }
    ::zvfs::addlist $filename {*}$zippathlist
  } else {
    ::zvfs::add $filename {*}[glob -nocomplain {*}$args]
  }
}

###
# Decode routines
###
proc ::zvfs::copy_file {zipbase destbase file} {
    set l [string length $zipbase]
    set relname [string trimleft [string range $file $l end] /]
    if {[file isdirectory $file]} {
      foreach sfile [glob -nocomplain $file/*] { 
         file mkdir [file join $destbase $relname]
         copy_file $zipbase $destbase $sfile
      }
      return
    }
    file copy -force $file [file join $destbase $relname]
}

# ### ### ### ######### ######### #########
## Convenience command, decode and copy to dir
## This routine relies on zvfs::mount, so we only load
## it when the zvfs package is present
##
proc ::zvfs::unzip {in out} {
    package require zvfs 1.0
    set root /ziptmp#[incr ::zvfs::count]
    zvfs::mount $in $root
    set out [file normalize $out]
    foreach file [glob $root/*] {
      copy_file $root $out $file
    }
    zvfs::unmount $in
    return
}
package provide zvfstools 0.1
