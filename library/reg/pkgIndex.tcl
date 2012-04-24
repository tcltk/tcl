if {![package vsatisfies [package provide Tcl] 8]} return
if {[string compare [info sharedlibextension] .dll]} return
if {[info exists ::tcl_platform(debug)]} {
    package ifneeded registry 1.2.1 \
            [list load [file join $dir tclreg12g.dll] registry]
} else {
    package ifneeded registry 1.2.1 \
            [list load [file join $dir tclreg12.dll] registry]
}
