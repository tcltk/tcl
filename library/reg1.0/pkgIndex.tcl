if {![package vsatisfies [package provide Tcl] 8]} {return}
if {[info exists tcl_platform(debug)]} {
    package ifneeded registry 1.0 \
            [list load [file join $dir tclreg83g.dll] registry]
} else {
    package ifneeded registry 1.0 \
            [list load [file join $dir tclreg83.dll] registry]
}
