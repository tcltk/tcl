if {![package vsatisfies [package provide Tcl] 9.0-]} return
if {[info sharedlibextension] != ".dll"} return
# On static builds, dde command is a static package
if {![::tcl::build-info static]} {
    package ifneeded dde 1.5b0 \
	[list load [file join $dir tcl9dde15.dll] Dde]
}
