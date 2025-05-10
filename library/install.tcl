###
# Installer actions built into tclsh and invoked
# if the first command line argument is "install"
###
if {[llength $argv] < 2} {
  exit 0
}
namespace eval ::practcl {}
###
# Installer tools
###
proc ::practcl::_isdirectory name {
  return [file isdirectory $name]
}
###
# Return true if the pkgindex file contains
# any statement other than "package ifneeded"
# and/or if any package ifneeded loads a DLL
###
proc ::practcl::_pkgindex_directory {path} {
  set buffer {}
  set pkgidxfile [file join $path pkgIndex.tcl]
  if {![file exists $pkgidxfile]} {
    # No pkgIndex file, read the source
    foreach file [glob -nocomplain $path/*.tm] {
      set file [file normalize $file]
      set fname [file rootname [file tail $file]]
      ###
      # We used to be able to ... Assume the package is correct in the filename
      # No hunt for a "package provides"
      ###
      set package [lindex [split $fname -] 0]
      set version [lindex [split $fname -] 1]
      ###
      # Read the file, and override assumptions as needed
      ###
      set fin [open $file r]
      fconfigure $fin -encoding utf-8 -eofchar \x1A
      set dat [read $fin]
      close $fin
      # Look for a teapot style Package statement
      foreach line [split $dat \n] {
	set line [string trim $line]
	if { [string range $line 0 9] != "# Package " } continue
	set package [lindex $line 2]
	set version [lindex $line 3]
	break
      }
      # Look for a package provide statement
      foreach line [split $dat \n] {
	set line [string trim $line]
	if { [string range $line 0 14] != "package provide" } continue
	set package [lindex $line 2]
	set version [lindex $line 3]
	break
      }
      append buffer "package ifneeded $package $version \[list source \[file join \$dir [file tail $file]\]\]" \n
    }
    foreach file [glob -nocomplain $path/*.tcl] {
      if { [file tail $file] == "version_info.tcl" } continue
      set fin [open $file r]
      fconfigure $fin -encoding utf-8 -eofchar \x1A
      set dat [read $fin]
      close $fin
      if {![regexp "package provide" $dat]} continue
      set fname [file rootname [file tail $file]]
      # Look for a package provide statement
      foreach line [split $dat \n] {
	set line [string trim $line]
	if { [string range $line 0 14] != "package provide" } continue
	set package [lindex $line 2]
	set version [lindex $line 3]
	if {[string index $package 0] in "\$ \[ @"} continue
	if {[string index $version 0] in "\$ \[ @"} continue
	append buffer "package ifneeded $package $version \[list source \[file join \$dir [file tail $file]\]\]" \n
	break
      }
    }
    return $buffer
  }
  set fin [open $pkgidxfile r]
  fconfigure $fin -encoding utf-8 -eofchar \x1A
  set dat [read $fin]
  close $fin
  set trace 0
  #if {[file tail $path] eq "tool"} {
  #  set trace 1
  #}
  set thisline {}
  foreach line [split $dat \n] {
    append thisline $line \n
    if {![info complete $thisline]} continue
    set line [string trim $line]
    if {[string length $line]==0} {
      set thisline {} ; continue
    }
    if {[string index $line 0] eq "#"} {
      set thisline {} ; continue
    }
    if {[regexp "if.*catch.*package.*Tcl.*return" $thisline]} {
      if {$trace} {puts "[file dirname $pkgidxfile] Ignoring $thisline"}
      set thisline {} ; continue
    }
    if {[regexp "if.*package.*vsatisfies.*package.*provide.*return" $thisline]} {
      if {$trace} { puts "[file dirname $pkgidxfile] Ignoring $thisline" }
      set thisline {} ; continue
    }
    if {![regexp "package.*ifneeded" $thisline]} {
      # This package index contains arbitrary code
      # source instead of trying to add it to the main
      # package index
      if {$trace} { puts "[file dirname $pkgidxfile] Arbitrary code $thisline" }
      return {source [file join $dir pkgIndex.tcl]}
    }
    append buffer $thisline \n
    set thisline {}
  }
  if {$trace} {puts [list [file dirname $pkgidxfile] $buffer]}
  return $buffer
}


proc ::practcl::_pkgindex_path_subdir {path} {
  set result {}
  foreach subpath [glob -nocomplain [file join $path *]] {
    if {[file isdirectory $subpath]} {
      lappend result $subpath {*}[_pkgindex_path_subdir $subpath]
    }
  }
  return $result
}
###
# Index all paths given as though they will end up in the same
# virtual file system
###
proc ::practcl::pkgindex_path args {
  set stack {}
  set buffer {
lappend ::PATHSTACK $dir
  }
  foreach base $args {
    set base [file normalize $base]
    set paths {}
    foreach dir [glob -nocomplain [file join $base *]] {
      if {[file tail $dir] eq "teapot"} continue
      lappend paths $dir {*}[::practcl::_pkgindex_path_subdir $dir]
    }
    set i    [string length  $base]
    # Build a list of all of the paths
    if {[llength $paths]} {
      foreach path $paths {
	if {$path eq $base} continue
	set path_indexed($path) 0
      }
    } else {
      puts [list WARNING: NO PATHS FOUND IN $base]
    }
    set path_indexed($base) 1
    set path_indexed([file join $base boot tcl]) 1
    foreach teapath [glob -nocomplain [file join $base teapot *]] {
      set pkg [file tail $teapath]
      append buffer [list set pkg $pkg]
      append buffer {
set pkginstall [file join $::g(HOME) teapot $pkg]
if {![file exists $pkginstall]} {
  installDir [file join $dir teapot $pkg] $pkginstall
}
}
    }
    foreach path $paths {
      if {$path_indexed($path)} continue
      set thisdir [file_relative $base $path]
      set idxbuf [::practcl::_pkgindex_directory $path]
      if {[string length $idxbuf]} {
	incr path_indexed($path)
	append buffer "set dir \[set PKGDIR \[file join \[lindex \$::PATHSTACK end\] $thisdir\]\]" \n
	append buffer [string map {$dir $PKGDIR} [string trimright $idxbuf]] \n
      }
    }
  }
  append buffer {
set dir [lindex $::PATHSTACK end]
set ::PATHSTACK [lrange $::PATHSTACK 0 end-1]
}
  return $buffer
}

###
# topic: 64319f4600fb63c82b2258d908f9d066
# description: Script to build the VFS file system
###
proc ::practcl::installDir {d1 d2} {

  puts [format {%*sCreating %s} [expr {4 * [info level]}] {} [file tail $d2]]
  file delete -force -- $d2
  file mkdir $d2

  foreach ftail [glob -directory $d1 -nocomplain -tails *] {
    set f [file join $d1 $ftail]
    if {[file isdirectory $f] && [string compare CVS $ftail]} {
      installDir $f [file join $d2 $ftail]
    } elseif {[file isfile $f]} {
	    file copy -force $f [file join $d2 $ftail]
	    if {$::tcl_platform(platform) eq {unix}} {
	file attributes [file join $d2 $ftail] -permissions 0o644
	    } else {
	file attributes [file join $d2 $ftail] -readonly 1
	    }
    }
  }

  if {$::tcl_platform(platform) eq {unix}} {
    file attributes $d2 -permissions 0o755
  } else {
    file attributes $d2 -readonly 1
  }
}

proc ::practcl::copyDir {d1 d2 {toplevel 1}} {
  #if {$toplevel} {
  #  puts [list ::practcl::copyDir $d1 -> $d2]
  #}
  #file delete -force -- $d2
  file mkdir $d2

  foreach ftail [glob -directory $d1 -nocomplain -tails *] {
    set f [file join $d1 $ftail]
    if {[file isdirectory $f] && [string compare CVS $ftail]} {
      copyDir $f [file join $d2 $ftail] 0
    } elseif {[file isfile $f]} {
      file copy -force $f [file join $d2 $ftail]
    }
  }
}

switch [lindex $argv 1] {
  mkzip {
    zipfs mkzip {*}[lrange $argv 2 end]
  }
  mkzip {
    zipfs mkimg {*}[lrange $argv 2 end]
  }
  default {
    ::practcl::[lindex $argv 1] {*}[lrange $argv 2 end]
  }
}
exit 0
