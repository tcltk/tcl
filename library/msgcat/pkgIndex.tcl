if {![package vsatisfies [package provide Tcl] 9.0-]} {return}
package ifneeded msgcat 1.7.2 [list source -encoding utf-8 [file join $dir msgcat.tcl]]
