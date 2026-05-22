if {[info sharedlibextension] != ".dll"} return
if {[package vsatisfies [package provide Tcl] 9.0-]} {
    # On static builds, dde command is a static package
    if {![::tcl::build-info static]} {
	package ifneeded dde 1.4.6 \
	    [list load [file join $dir tcl9dde14.dll] Dde]
    }
} elseif {[::tcl::pkgconfig get debug]} {
    package ifneeded dde 1.4.6 \
	    [list load [file join $dir tcldde14g.dll] Dde]
} else {
    package ifneeded dde 1.4.6 \
	    [list load [file join $dir tcldde14.dll] Dde]
}
