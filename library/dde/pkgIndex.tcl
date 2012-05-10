if {![package vsatisfies [package provide Tcl] 8.5]} return
if {[string compare [info sharedlibextension] .dll]} return
if {[::tcl::pkgconfig get debug]} {
    package ifneeded dde 1.3.3 [list load [file join $dir tcldde13g.dll] dde]
} else {
    package ifneeded dde 1.3.3 [list load [file join $dir tcldde13.dll] dde]
}
