puts exe:\t[info nameofexecutable]
puts ver:\t[info patchlevel]
catch {
    puts build:\t[tcl::build-info]
}
puts lib:\t[info library]
puts plat:\t[lsort -dictionary -stride 2 [array get tcl_platform]]
