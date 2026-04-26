if {$::tcl_platform(platform) != "windows"} return
package ifneeded registry [info patchlevel] [string cat \
	"interp alias {} ::registry {} ::tcl::registry;" \
    "package provide registry [info patchlevel]" \
]
# Version 1.4 is the last version prior to moving the registry module
# into the core tcl library
package ifneeded registry 1.4 [string cat \
	"interp alias {} ::registry {} ::tcl::registry;" \
    "package provide registry 1.4" \
]