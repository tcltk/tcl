if {![package vsatisfies [package provide Tcl] 9-]} {return}
package ifneeded cookiejar 0.3.0 [list source [file join $dir cookiejar.tcl]]
package ifneeded tcl::idna 1.0.2 [list source [file join $dir idna.tcl]]
