if {![package vsatisfies [package provide Tcl] 8.6-]} {return}
package ifneeded platform        1.1.0 [list source -encoding utf-8 [file join $dir platform.tcl]]
package ifneeded platform::shell 1.1.4 [list source -encoding utf-8 [file join $dir shell.tcl]]
