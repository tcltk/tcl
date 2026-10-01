if {![package vsatisfies [package provide Tcl] 8.5-]} {return}
package ifneeded msgcat 1.6.2 [list source -encoding utf-8 [file join $dir msgcat.tcl]]
