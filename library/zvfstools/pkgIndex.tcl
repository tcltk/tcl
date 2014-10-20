if {![package vsatisfies [package provide zvfs] 1.0]} {return}
package ifneeded zvfstools 0.1 [list source [file join $dir zvfstools.tcl]]
