/* 
 * tclInitScript.h --
 *
 *	This file contains Unix & Windows common init script
 *      It is not used on the Mac. (the mac init script is in tclMacInit.c)
 *	This file should only be included once in the entire set of C
 *	source files for Tcl (by the respective platform initialization
 *	C source file, tclUnixInit.c and tclWinInit.c) and thus the
 *	presence of the routine, TclSetPreInitScript, below, should be
 *	harmless.
 *
 * Copyright (c) 1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: %Z% $Id: tclInitScript.h,v 1.3 1998/07/24 13:49:59 surles Exp $ 
 */

/*
 * The following string is the startup script executed in new interpreters.
 * It looks on disk in (way too many) different directories for a script
 * "init.tcl" that is compatible with this version of Tcl.  The init.tcl
 * script does all of the real work of initialization.
 */

static char initScript[] = "if {[info proc tclInit]==\"\"} {\n\
  proc tclInit {} {\n\
    global tcl_library tcl_version tcl_patchLevel errorInfo\n\
    global env tcl_pkgPath\n\
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

/*
 * A pointer to a string that holds an initialization script that if non-NULL
 * is evaluated in Tcl_Init() prior to the the built-in initialization script
 * above.  This variable can be modified by the procedure below.
 */
 
static char *          tclPreInitScript = NULL;


/*
 *----------------------------------------------------------------------
 *
 * TclSetPreInitScript --
 *
 *	This routine is used to change the value of the internal
 *	variable, tclPreInitScript.
 *
 * Results:
 *	Returns the current value of tclPreInitScript.
 *
 * Side effects:
 *	Changes the way Tcl_Init() routine behaves.
 *
 *----------------------------------------------------------------------
 */

char *
TclSetPreInitScript (string)
    char *string;		/* Pointer to a script. */
{
    char *prevString = tclPreInitScript;
    tclPreInitScript = string;
    return(prevString);
}
