if {![package vsatisfies [package provide Tcl] 8]} return
if {[info sharedlibextension] != ".dll"} return
if {[info exists ::tcl_platform(debug)]} {
  if {[info exists [file join $dir tclreg12g.dll]]} {
    package ifneeded registry 1.2.2 \
            [list load [file join $dir tclreg12g.dll] registry]
  } else {
    package ifneeded registry 1.2.2 \
            [list load tclreg12g registry]
  }
} else {
  if {[info exists [file join $dir tclreg12.dll]]} {
    package ifneeded registry 1.2.2 \
            [list load [file join $dir tclreg12.dll] registry]
  } else {
    package ifneeded registry 1.2.2 \
            [list load tclreg12 registry]
  }
}
