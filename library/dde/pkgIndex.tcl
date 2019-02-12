if {![package vsatisfies [package provide Tcl] 8]} return
if {[info sharedlibextension] != ".dll"} return
if {[info exists ::tcl_platform(debug)]} {
    package ifneeded dde 1.4.1 [list load [file join $dir tcldde14g.dll] dde]
} else {
    package ifneeded dde 1.4.1 [list load [file join $dir tcldde14.dll] dde]
}
