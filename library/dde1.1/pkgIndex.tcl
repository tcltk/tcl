if {![package vsatisfies [package provide Tcl] 8]} {return}
if {[info exists tcl_platform(debug)]} {
    package ifneeded dde 1.1 [list load [file join $dir tcldde83g.dll] dde]
} else {
    package ifneeded dde 1.1 [list load [file join $dir tcldde83.dll] dde]
}
