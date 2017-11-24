if {([info commands ::tcl::pkgconfig] eq "")
	|| ([info sharedlibextension] ne ".dll")} return
package ifneeded registry 1.3.2 \
	[list load [file join $dir tclreg1.dll] registry]
