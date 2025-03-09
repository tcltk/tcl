if {![package vsatisfies [package provide Tcl] 9.0-]} return
if {[info sharedlibextension] != ".dll"} return
package ifneeded dde 1.5a0 \
	[list load [file join $dir tcl9dde15.dll] Dde]

