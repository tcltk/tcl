if {![package vsatisfies [package provide Tcl] 8.2]} {return}
package ifneeded msgcat 1.2.3 [list source [file join $dir msgcat.tcl]]
