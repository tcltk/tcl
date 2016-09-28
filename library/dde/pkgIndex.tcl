if {([info commands ::tcl::pkgconfig] eq "")
	|| ([info sharedlibextension] ne ".dll")} return
package ifneeded dde 1.4.0 [list load [file join $dir tcldde1.dll] dde]
