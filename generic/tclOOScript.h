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
"::proc ::oo::Helpers::callback {method args} {\n"
"    list [uplevel 1 {namespace which my}] $method {*}$args\n"
"}\n"

"::proc ::oo::Helpers::mymethod {method args} {\n"
"    list [uplevel 1 {namespace which my}] $method {*}$args\n"
"}\n"

"::proc ::oo::Helpers::classvariable {name args} {\n"
"    # Get a reference to the class's namespace\n"
"    set ns [info object namespace [uplevel 1 {self class}]]\n"
"    # Double up the list of variable names\n"
"    set vs [list $name $name]\n"
"    foreach v $args {lappend vs $v $v}\n"
"    # Lastly, link the caller's local variables to the class's variables\n"
"    tailcall namespace upvar $ns {*}$vs\n"
"}\n"

"::proc ::oo::Helpers::link {args} {\n"
"    set ns [uplevel 1 {namespace current}]\n"
"    foreach link $args {\n"
"        if {[llength $link] == 2} {\n"
"            lassign $link src dst\n"
"        } else {\n"
"            lassign $link src\n"
"            set dst $src\n"
"        }\n"
"        interp alias {} ${ns}::$src {} ${ns}::my $dst\n"
"    }\n"
"    return\n"
"}\n"

/*
"proc ::oo::define::classmethod {name {args {}} {body {}}} {\n"
"    # Create the method on the class if the caller gave arguments and body\n"
"    set argc [llength [info level 0]]\n"
"    if {$argc == 3} {\n"
"        return -code error [string cat {wrong # args: should be \"}"
"                [lindex [info level 0] 0] { name ?args body?\"}]\n"
"    }\n"
"    # Get the name of the current class or class delegate\n"
"    set cls [uplevel 1 self]\n"
"    set d $cls.Delegate\n"
"    if {[info object isa object $d] && [info object isa class $d]} {\n"
"        set cls $d\n"
"    }\n"
"    if {$argc == 4} {\n"
"        ::oo::define $cls method $name $args $body\n"
"    }\n"
"    # Make the connection by forwarding\n"
"    tailcall forward $name [info object namespace $cls]::my $name\n"
"}\n"

"# Build this *almost* like a class method, but with extra care to avoid\n"
"# nuking the existing method.\n"
"::oo::class create ::oo::class.Delegate {\n"
"    method create {name args} {\n"
"        if {![string match ::* $name]} {\n"
"            set ns [uplevel 1 {namespace current}]\n"
"            if {$ns eq {::}} {set ns {}}\n"
"            set name ${ns}::${name}\n"
"        }\n"
"        if {[string match *.Delegate $name]} {\n"
"            return [next $name {*}$args]\n"
"        }\n"
"        set delegate [oo::class create $name.Delegate]\n"
"        set cls [next $name {*}$args]\n"
"        set superdelegates [list $delegate]\n"
"        foreach c [info class superclass $cls] {\n"
"            set d $c.Delegate\n"
"            if {[info object isa object $d] && [info object isa class $d]} {\n"
"                lappend superdelegates $d\n"
"            }\n"
"        }\n"
"        oo::objdefine $cls mixin {*}$superdelegates\n"
"        return $cls\n"
"    }\n"
"}\n"
*/

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
"::oo::objdefine ::oo::objdefine::mixin forward --default-operation my -set\n"

/*
"::oo::define ::oo::class self mixin ::oo::class.Delegate\n"
*/

"::oo::class create ::oo::singleton {\n"
"    superclass ::oo::class\n"
"    variable object\n"
"    unexport create createWithNamespace\n"
"    method new args {\n"
"        if {![info exists object]} {\n"
"            set object [next {*}$args]\n"
"        }\n"
"        return $object\n"
"    }\n"
"}\n"

"::oo::class create ::oo::abstract {\n"
"    superclass ::oo::class\n"
"    unexport create createWithNamespace new\n"
"}\n"
;

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
