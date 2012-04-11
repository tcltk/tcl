if {![package vsatisfies [package provide Tcl] 8.5]} {return}
if {[string compare $::tcl_platform(platform) windows]} {return}
if {[::tcl::pkgconfig get debug]} {
    package ifneeded dde 1.3.2 [list load [file join $dir tcldde13g.dll] dde]
} else {
    package ifneeded dde 1.3.2 [list load [file join $dir tcldde13.dll] dde]
}
