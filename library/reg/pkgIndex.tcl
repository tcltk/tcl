if {([info commands ::tcl::pkgconfig] eq "")
	|| ([info sharedlibextension] ne ".dll")} return
package ifneeded registry 1.3.1 \
	[list load [file join $dir tclreg13.dll] registry]
