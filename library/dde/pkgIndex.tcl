if {([info commands ::tcl::pkgconfig] eq "")
	|| ([info sharedlibextension] ne ".dll")} return
if {[package vsatisfies [package provide Tcl] 9.0]} {
    package ifneeded dde 1.4.0 [list load [file join $dir dde.dll] dde]
} elseif {[::tcl::pkgconfig get debug]} {
    package ifneeded dde 1.4.0 [list load [file join $dir tcldde14g.dll] dde]    	
} else {
    package ifneeded dde 1.4.0 [list load [file join $dir tcldde14.dll] dde]
}
