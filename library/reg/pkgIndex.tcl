if {([info commands ::tcl::pkgconfig] eq "")
	|| ([info sharedlibextension] ne ".dll")} return
if {[package vsatisfies [package provide Tcl] 9.0]} {
    package ifneeded registry 1.3.0 [list load [file join $dir reg.dll] registry]
} elseif {[::tcl::pkgconfig get debug]} {
    package ifneeded registry 1.3.0 [list load [file join $dir tclreg13g.dll] registry]    	
} else {
    package ifneeded registry 1.3.0 [list load [file join $dir tclreg13.dll] registry]
}
