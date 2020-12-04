if {![package vsatisfies [package provide Tcl] 8.5-]} return
if {[info sharedlibextension] != ".dll"} return
package ifneeded registry 1.3.5 \
        [list load [file join $dir tcl9registry13.dll]]
