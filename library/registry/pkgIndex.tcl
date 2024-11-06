if {$::tcl_platform(platform) ne "windows"} return
# The registry package version tracks the Tcl version
package ifneeded registry [info patchlevel] [load {} Registry]
