if {![package vsatisfies [package provide Tcl] 8]} return
if {[string compare [info sharedlibextension] .dll]} return
if {[info exists ::tcl_platform(debug)]} {
    package ifneeded dde 1.3.2 [list load [file join $dir tcldde13g.dll] dde]
} else {
    package ifneeded dde 1.3.2 [list load [file join $dir tcldde13.dll] dde]
}
