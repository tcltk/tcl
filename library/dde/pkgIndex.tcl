if {[info exists tcl_platform(debug)]} {
    package ifneeded dde 1.1 [list load [file join $dir tcldde84d.dll] dde]
} else {
    package ifneeded dde 1.1 [list load [file join $dir tcldde84.dll] dde]
}
