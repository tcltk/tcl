/*
 * tclOOScript.h --
 *
 *	This file contains support scripts for TclOO. They are defined here so
 *	that the code can be definitely run even in safe interpreters; TclOO's
 *	core setup is safe.
 *
 * Copyright (c) 2012-2018 Donal K. Fellows
 * Copyright (c) 2013 Andreas Kupries
 * Copyright (c) 2017 Gerald Lester
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef TCL_OO_SCRIPT_H
#define TCL_OO_SCRIPT_H

/*
 * The scripted part of the definitions of TclOO.
 *
 * Compiled from tools/tclOOScript.tcl by tools/makeHeader.tcl, which
 * contains the commented version of everything; *this* file is automatically
 * generated.
 */

static const char *tclOOSetupScript =
/* !BEGIN!: Do not edit below this line. */
"::oo::define ::oo::object method <cloned> -unexport {originObject} {\n"
"\tforeach p [info procs [info object namespace $originObject]::*] {\n"
"\t\tset args [info args $p]\n"
"\t\tset idx -1\n"
"\t\tforeach a $args {\n"
"\t\t\tif {[info default $p $a d]} {\n"
"\t\t\t\tlset args [incr idx] [list $a $d]\n"
"\t\t\t} else {\n"
"\t\t\t\tlset args [incr idx] [list $a]\n"
"\t\t\t}\n"
"\t\t}\n"
"\t\tset b [info body $p]\n"
"\t\tset p [namespace tail $p]\n"
"\t\tproc $p $args $b\n"
"\t}\n"
"\tforeach v [info vars [info object namespace $originObject]::*] {\n"
"\t\tupvar 0 $v vOrigin\n"
"\t\tnamespace upvar [namespace current] [namespace tail $v] vNew\n"
"\t\tif {[info exists vOrigin]} {\n"
"\t\t\tif {[array exists vOrigin]} {\n"
"\t\t\t\tarray set vNew [array get vOrigin]\n"
"\t\t\t} else {\n"
"\t\t\t\tset vNew $vOrigin\n"
"\t\t\t}\n"
"\t\t}\n"
"\t}\n"
"}\n"
/* !END!: Do not edit above this line. */
;

#endif /* TCL_OO_SCRIPT_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
