if {![package vsatisfies [package provide Tcl] 8.5-]} return
if {[info sharedlibextension] != ".dll"} return
if {[package vsatisfies [package provide Tcl] 9.0-]} {
    package ifneeded registry 1.3.7 \
	    [list load [file join $dir tcl9registry13.dll] Registry]
} else {
    package ifneeded registry 1.3.7 \
	    [list load [file join $dir tclregistry13.dll] Registry]
}
