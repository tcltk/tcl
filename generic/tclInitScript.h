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
 * SCCS: @(#) tclInitScript.h 1.2 98/01/09 17:20:19
 */

/*
 * The following string is the startup script executed in new
 * interpreters.  It looks on disk in several different directories
 * for a script "init.tcl" that is compatible with this version
 * of Tcl.  The init.tcl script does all of the real work of
 * initialization.
 */

static char initScript[] = "if {[info proc tclInit]==\"\"} {\n\
  proc tclInit {} {\n\
    global tcl_libPath tcl_library errorInfo\n\
    rename tclInit {}\n\
    set errors {}\n\
    foreach i $tcl_libPath {\n\
	set tcl_library $i\n\
	set tclfile [file join $i init.tcl]\n\
	if {[file exists $tclfile]} {\n\
	    if {[catch {uplevel #0 [list source $tclfile]} msg] != 1} {\n\
		return\n\
	    } else {\n\
		append errors \"$tclfile: $msg\n$errorInfo\n\"\n\
	    }\n\
	}\n\
    }\n\
    set msg \"Can't find a usable init.tcl in the following directories: \n\"\n\
    append msg \"    $tcl_libPath\n\n\"\n\
    append msg \"$errors\n\n\"\n\
    append msg \"This probably means that Tcl wasn't installed properly.\n\"\n\
    error $msg\n\
  }\n\
}\n\
tclInit";

