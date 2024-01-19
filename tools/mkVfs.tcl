set FORBIDDEN_DIRS {CVS}

proc cat fname {
    set fname [open $fname r]
    try {
	return [read $fname]
    } finally {
	close $fname
    }
}

proc pkgIndexDir {root fout d1} {
    global FORBIDDEN_DIRS
    puts [format {%*sIndexing %s} [expr {4 * [info level]}] {} \
	    [file tail $d1]]
    set idx [string length $root]
    foreach ftail [glob -directory $d1 -nocomplain -tails *] {
        set f [file join $d1 $ftail]
        if {[file isdirectory $f] && $ftail ni $FORBIDDEN_DIRS} {
            pkgIndexDir $root $fout $f
        } elseif {[file tail $f] eq "pkgIndex.tcl"} {
      	    puts $fout "set dir \${VFSROOT}[string range $d1 $idx end]"
	    puts $fout [cat $f]
	}
    }
}

###
# Script to build the VFS file system
###
proc copyDir {d1 d2} {
    puts [format {%*sCreating %s} [expr {4 * [info level]}] {} \
	    [file tail $d2]]

    file delete -force -- $d2
    file mkdir $d2

    foreach ftail [glob -directory $d1 -nocomplain -tails *] {
        set f [file join $d1 $ftail]
        if {[file isdirectory $f] && "CVS" ne $ftail} {
            copyDir $f [file join $d2 $ftail]
        } elseif {[file isfile $f]} {
      	    file copy -force $f [file join $d2 $ftail]
	    if {$::tcl_platform(platform) eq {unix}} {
            	file attributes [file join $d2 $ftail] -permissions 0o644
      	    } else {
            	file attributes [file join $d2 $ftail] -readonly 1
	    }
	}
    }

    if {$::tcl_platform(platform) eq {unix}} {
      	file attributes $d2 -permissions 0o755
    } else {
	file attributes $d2 -readonly 1
    }
}

# Locate and copy a standard extension DLL into the VFS
proc copyStdExtDLL {base} {
    set dlls [glob -nocomplain ${TCLSRC_ROOT}/win/tcl$base*.dll]
    puts "[string toupper $base] DLL $dlls"
    if {[llength $dlls]} {
      	file copy [lindex $dlls 0] ${TCL_SCRIPT_DIR}/$base
    }
}

if {[llength $argv] < 3} {
    puts "Usage: VFS_ROOT TCLSRC_ROOT PLATFORM"
    exit 1
}
lassign $argv TCL_SCRIPT_DIR TCLSRC_ROOT PLATFORM TKDLL TKVER

puts "Building [file tail $TCL_SCRIPT_DIR] for $PLATFORM"
copyDir ${TCLSRC_ROOT}/library ${TCL_SCRIPT_DIR}

if {$PLATFORM eq "windows"} {
    copyStdExtDLL dde
    copyStdExtDLL reg
} else {
    # Remove the dde and reg package paths
    file delete -force ${TCL_SCRIPT_DIR}/dde
    file delete -force ${TCL_SCRIPT_DIR}/reg
}

# For the following packages, cat their pkgIndex files to tclIndex
file attributes ${TCL_SCRIPT_DIR}/tclIndex -readonly 0
set fout [open ${TCL_SCRIPT_DIR}/tclIndex a]
try {
    puts $fout {#
# MANIFEST OF INCLUDED PACKAGES
#
set VFSROOT $dir
}
    if {$TKDLL ne "" && [file exists $TKDLL] && $TKVER ne ""} {
	file copy $TKDLL ${TCL_SCRIPT_DIR}
	puts $fout [list package ifneeded Tk $TKVER "load \$dir $TKDLL"]
    }
    pkgIndexDir ${TCL_SCRIPT_DIR} $fout ${TCL_SCRIPT_DIR}
} finally {
    close $fout
}
