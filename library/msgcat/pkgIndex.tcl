if {![package vsatisfies [package provide Tcl] 8.5-]} {return}
package ifneeded msgcat 1.7.0 [list source [file join $dir msgcat.tcl]]
