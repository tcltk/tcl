# genWinImage.tcl --
#
#	This script generates the Windows installer.
#
# Copyright (c) 1999 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: genWinImage.tcl,v 1.1.2.1 1999/03/26 00:23:25 suresh Exp $


# This file is insensitive to the directory from which it is invoked.

namespace eval genWinImage {
    # toolsDir --
    #
    # This variable points to the platform specific tools directory.

    variable toolsDir

    # tclBuildDir --
    #
    # This variable points to the directory containing the Tcl built tree.

    variable tclBuildDir

    # tkBuildDir --
    #
    # This variable points to the directory containing the Tk built tree.

    variable tkBuildDir
}

# genWinImage::init --
#
#	This is the main entry point.
#
# Arguments:
#	None.
#
# Results:
#	None.

proc genWinImage::init {} {
    global tcl_platform argv argv0
    variable tclBuildDir
    variable tkBuildDir
    variable toolsDir
 
    puts "\n--- genWiImage.tcl started: \
	    [clock format [clock seconds] -format "%Y%m%d-%H:%M"] --\n"

    if {$tcl_platform(platform) != "windows"} {
	puts stderr "ERROR: Cannot build TCL.EXE on Unix systems"
	exit 1
    }

    if {[llength $argv] != 3} {
	puts stderr "usage: $argv0 <tclBuildDir> <tkBuildDir> <toolsDir>"
	exit 0
    }

    set tclBuildDir [lindex $argv 0]
    set tkBuildDir [lindex $argv 1]
    set toolsDir [lindex $argv 2]

    generateInstallers
 
    puts "\n--- genWiImage.tcl finished: \
	    [clock format [clock seconds] -format "%Y%m%d-%H:%M"] --\n\n"
}

# genWinImage::generateInstallers --
#
#	Perform substitutions on the pro.wse.in file and then
#	invoke the WSE script twice; once for CD and once for web.
#
# Arguments:
#	None.
#
# Results:
#	Leaves proweb.exe and procd.exe sitting in the curent directory.

proc genWinImage::generateInstallers {} {
    variable toolsDir
    variable tclBuildDir
    variable tkBuildDir

    # Now read the "pro/srcs/install/pro.wse.in" file, have Tcl make
    # appropriate substitutions, write out the resulting file in a
    # current-working-directory.  Use this new file to perform installation
    # image creation.  Note that we have to use this technique to set
    # the value of _WISE_ because wise32 won't use a /d switch for this
    # variable.

    set __TCLBASEDIR__ [file native $tclBuildDir]
    set __TKBASEDIR__ [file native $tkBuildDir]
    set __WISE__ [file native [file join $toolsDir wise]]
    set f [open tcl.wse.in r]
    set s [read $f]
    close $f
    set s [subst -nocommands -nobackslashes $s]
    set f [open tcl.wse w]
    puts $f $s
    close $f

    set wise32ProgFilePath [file native [file join $__WISE__ wise32.exe]]

    # Run the Wise installer to create the Windows install images.

    if {[catch {exec [file native $wise32ProgFilePath] \
	    /c tcl.wse} errMsg]} {
	puts stderr "ERROR: $errMsg"
    }

    return
}

genWinImage::init
