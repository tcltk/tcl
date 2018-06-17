/*
 * tclOOScript.h --
 *
 *	This file contains support scripts for TclOO. They are defined here so
 *	that the code can be definitely run even in safe interpreters; TclOO's
 *	core setup is safe.
 *
 * Copyright (c) 2012-2018 by Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef TCL_OO_SCRIPT_H
#define TCL_OO_SCRIPT_H

/*
 * The scripted part of the definitions of TclOO.
 */

static const char *tclOOSetupScript =
"::oo::define ::oo::Slot {\n"
"    method Get {} {return -code error unimplemented}\n"
"    method Set list {return -code error unimplemented}\n"
"    method -set args {tailcall my Set $args}\n"
"    method -append args {\n"
"        set current [uplevel 1 [list [namespace which my] Get]]\n"
"        tailcall my Set [list {*}$current {*}$args]\n"
"    }\n"
"    method -clear {} {tailcall my Set {}}\n"
"    forward --default-operation my -append\n"
"    method unknown {args} {\n"
"        set def --default-operation\n"
"        if {[llength $args] == 0} {\n"
"            tailcall my $def\n"
"        } elseif {![string match -* [lindex $args 0]]} {\n"
"            tailcall my $def {*}$args\n"
"        }\n"
"        next {*}$args\n"
"    }\n"
"    export -set -append -clear\n"
"    unexport unknown destroy\n"
"}\n"
"\n"
"::oo::objdefine ::oo::define::superclass forward --default-operation my -set\n"
"::oo::objdefine ::oo::define::mixin forward --default-operation my -set\n"
"::oo::objdefine ::oo::objdefine::mixin forward --default-operation my -set\n";

/*
 * The body of the <cloned> method of oo::object.
 */

static const char *clonedBody =
"foreach p [info procs [info object namespace $originObject]::*] {\n"
"    set args [info args $p]\n"
"    set idx -1\n"
"    foreach a $args {\n"
"        lset args [incr idx]"
"            [if {[info default $p $a d]} {list $a $d} {list $a}]\n"
"    }\n"
"    set b [info body $p]\n"
"    set p [namespace tail $p]\n"
"    proc $p $args $b\n"
"}\n"
"foreach v [info vars [info object namespace $originObject]::*] {\n"
"    upvar 0 $v vOrigin\n"
"    namespace upvar [namespace current] [namespace tail $v] vNew\n"
"    if {[info exists vOrigin]} {\n"
"        if {[array exists vOrigin]} {\n"
"            array set vNew [array get vOrigin]\n"
"        } else {\n"
"            set vNew $vOrigin\n"
"        }\n"
"    }\n"
"}\n";

#endif /* TCL_OO_SCRIPT_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
