###
# Package manifest for all Tcl packages included in the /library file system
###
apply {{dir} {
  set ::test [info script]
  set isafe [interp issafe]
  foreach {safe package version file} {
    0 http            2.9.0 {http http.tcl}
    1 msgcat          1.7.0  {msgcat msgcat.tcl}
    1 opt             0.4.7  {opt optparse.tcl}
    0 platform        1.0.14 {platform platform.tcl}
    0 platform::shell 1.1.4  {platform shell.tcl}
    1 tcltest         2.5.0  {tcltest tcltest.tcl}
  } {
    if {$isafe && !$safe} continue
    package ifneeded $package $version  [list source [file join $dir {*}$file]]
  }
}} $dir
