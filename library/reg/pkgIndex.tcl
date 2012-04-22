if {![package vsatisfies [package provide Tcl] 8.5]} {return}
if {[string compare $::tcl_platform(platform) windows]} {return}
if {[::tcl::pkgconfig get debug]} {
    package ifneeded registry 1.3 \
            [list load [file join $dir tclreg13g.dll] registry]
} else {
    package ifneeded registry 1.3 \
            [list load [file join $dir tclreg13.dll] registry]
}
