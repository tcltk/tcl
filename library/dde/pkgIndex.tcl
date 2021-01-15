if {[info sharedlibextension] != ".dll"} return
if {[package vsatisfies [package provide Tcl] 9.0]} {
    package ifneeded dde 1.4.4 \
	    [list load [file join $dir tcl9dde14.dll] Dde]
} else {
    package ifneeded dde 1.4.4 \
	    [list load [file join $dir tcldde14.dll] Dde]
}
