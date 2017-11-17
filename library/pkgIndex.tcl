###
# Package manifest for all Tcl packages included in the /library file system
###
foreach {package version file} {
  http            2.8.12 {http http.tcl}
  http            1.0    {http1.0 http.tcl}
  msgcat          1.6.1  {msgcat msgcat.tcl}
  opt             0.4.7  {opt optparse.tcl}
  platform        1.0.14 {platform platform.tcl}
  platform::shell 1.1.4  {platform shell.tcl}
  tcltest         2.4.1  {tcltest tcltest.tcl}
} {
  package ifneeded $package $version  [list source [file join $dir {*}$file]]
}
# Opt is the odd man out
package ifneeded opt 0.4.7 [list source [file join $dir opt optparse.tcl]]
