if {![package vsatisfies [package provide Tcl] 9.0-]} return
if {[info sharedlibextension] != ".dll"} return
package ifneeded registry 1.4a0 \
	[list load [file join $dir tcl9registry14.dll] Registry]
