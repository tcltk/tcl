if {![package vsatisfies [package provide Tcl] 8]} {return}
if {[info exists tcl_platform(debug)]} {
    package ifneeded dde 1.2 [list load [file join $dir tcldde12d.dll] dde]
} else {
    package ifneeded dde 1.2 [list load [file join $dir tcldde12.dll] dde]
}
