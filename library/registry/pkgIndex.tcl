if {![package vsatisfies [package provide Tcl] 8.5-]} return
if {[info sharedlibextension] != ".dll"} return
package ifneeded registry 1.3.6 \
	[list load [file join $dir tclregistry13.dll] Registry]
