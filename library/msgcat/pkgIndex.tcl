if {![package vsatisfies [package provide Tcl] 8.7-]} {return}
package ifneeded msgcat 1.7.1 [list source -encoding utf-8 [file join $dir msgcat.tcl]]
