/* 
 * tclInitScript.h --
 *
 *	This file contains Unix & Windows common init script
 *      It is not used on the Mac. (the mac init script is in tclMacInit.c)
 *
 * Copyright (c) 1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: %Z% $Id: tclInitScript.h,v 1.1 1998/06/27 22:11:40 welch Exp $ 
 */

/*
 * The following string is the startup script executed in new
 * interpreters.  It looks on disk in (way too many) different directories
 * for a script "init.tcl" that is compatible with this version
 * of Tcl.  The init.tcl script does all of the real work of
 * initialization.
 */

static char initScript[] = "if {[info proc tclInit]==\"\"} {\n\
  proc tclInit {} {\n\
    global tcl_library tcl_version tcl_patchLevel errorInfo\n\
    global tcl_pkgPath\n\
    rename tclInit {}\n\
    set errors {}\n\
    set dirs {}\n\
    if {[info exists env(TCL_LIBRARY)]} {\n\
	lappend dirs $env(TCL_LIBRARY)\n\
    }\n\
    lappend dirs $tcl_library\n\
    set parentDir [file dirname [file dirname [info nameofexecutable]]]\n\
    lappend dirs [file join $parentDir lib/tcl$tcl_version]\n\
    if {[string match {*[ab]*} $tcl_patchLevel]} {\n\
	set lib tcl$tcl_patchLevel\n\
    } else {\n\
	set lib tcl$tcl_version\n\
    }\n\
    lappend dirs [file join [file dirname [file dirname [pwd]]] $lib/library]\n\
    lappend dirs [file join [file dirname [pwd]] library]\n\
    lappend dirs [file join [file dirname $parentDir] $lib/library]\n\
    lappend dirs [file join $parentDir library]\n\
    foreach i $dirs {\n\
	set tcl_library $i\n\
	set tclfile [file join $i init.tcl]\n\
	if {[file exists $tclfile]} {\n\
	    if {![catch {uplevel #0 [list source $tclfile]} msg]} {\n\
                lappend tcl_pkgPath [file dirname $i]\n\
	        return\n\
	    } else {\n\
		append errors \"$tclfile: $msg\n$errorInfo\n\"\n\
	    }\n\
	}\n\
    }\n\
    set msg \"Can't find a usable init.tcl in the following directories: \n\"\n\
    append msg \"    $dirs\n\n\"\n\
    append msg \"$errors\n\n\"\n\
    append msg \"This probably means that Tcl wasn't installed properly.\n\"\n\
    error $msg\n\
  }\n\
}\n\
tclInit";

