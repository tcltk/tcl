if {([info commands ::tcl::pkgconfig] eq "")
  || ([info sharedlibextension] ne ".dll")} return
if {[::tcl::pkgconfig get debug]} {
  if {[info exists [file join $dir tclreg13g.dll]]} {
    package ifneeded registry 1.3.2 \
            [list load [file join $dir tclreg13g.dll] registry]
  } else {
    package ifneeded registry 1.3.2 \
            [list load tclreg13g registry]
  }
} else {
  if {[info exists [file join $dir tclreg13.dll]]} {
    package ifneeded registry 1.3.2 \
            [list load [file join $dir tclreg13.dll] registry]
  } else {
    package ifneeded registry 1.3.2 \
            [list load tclreg13 registry]
  }
}
