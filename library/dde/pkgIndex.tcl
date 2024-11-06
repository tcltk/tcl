if {$::tcl_platform(platform) ne "windows"} return
# The dde package version tracks the Tcl version
package ifneeded dde [info patchlevel] [load {} Dde]