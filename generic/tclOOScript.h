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
"    foreach v [list $name {*}$args] {\n"
"        if {[string match *(*) $v]} {\n"
"            return -code error [string cat {bad variable name \"} $v {\": can\'t create a scalar variable that looks like an array element}]\n"
"        }\n"
"        if {[string match *::* $v]} {\n"
"            return -code error [string cat {bad variable name \"} $v {\": can\'t create a local variable with a namespace separator in it}]\n"
"        }\n"
"        lappend vs $v $v\n"
"    }\n"
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
"        if {![string match ::* $src]} {\n"
"            set src [string cat $ns :: $src]\n"
"        }\n"
"        interp alias {} $src {} ${ns}::my $dst\n"
"        trace add command ${ns}::my delete [list ::oo::Helpers::Unlink $src]\n"
"    }\n"
"    return\n"
"}\n"
"::proc ::oo::Helpers::Unlink {cmd args} {\n"
"    if {[namespace which $cmd] ne {}} {\n"
"        rename $cmd {}\n"
"    }\n"
"}\n"

"::proc ::oo::DelegateName {class} {\n"
"    string cat [info object namespace $class] {:: oo ::delegate}\n"
"}\n"

"proc ::oo::MixinClassDelegates {class} {\n"
"    if {![info object isa class $class]} {\n"
"        return\n"
"    }\n"
"    set delegate [::oo::DelegateName $class]\n"
"    if {![info object isa class $delegate]} {\n"
"        return\n"
"    }\n"
"    foreach c [info class superclass $class] {"
"        set d [::oo::DelegateName $c]\n"
"        if {![info object isa class $d]} {\n"
"            continue\n"
"        }\n"
"        ::oo::define $delegate superclass -append $d\n"
"    }\n"
"    ::oo::objdefine $class mixin -append $delegate\n"
"}\n"

"::namespace eval ::oo::define {"
"    ::proc classmethod {name {args {}} {body {}}} {\n"
"        # Create the method on the class if the caller gave arguments and body\n"
"        ::set argc [::llength [::info level 0]]\n"
"        ::if {$argc == 3} {\n"
"            ::return -code error [::string cat {wrong # args: should be \"}"
"                    [::lindex [::info level 0] 0] { name ?args body?\"}]\n"
"        }\n"
"        ::set cls [::uplevel 1 self]\n"
"        ::if {$argc == 4} {\n"
"            ::oo::define [::oo::DelegateName $cls] method $name $args $body\n"
"        }\n"
"        # Make the connection by forwarding\n"
"        ::tailcall forward $name myclass $name\n"
"    }\n"

"    ::proc initialise {body} {\n"
"        ::set clsns [::info object namespace [::uplevel 1 self]]\n"
"        ::tailcall apply [::list {} $body $clsns]\n"
"    }\n"

"    # Make the initialise command appear with US spelling too\n"
"    ::namespace export initialise\n"
"    ::namespace eval tmp {::namespace import ::oo::define::initialise}\n"
"    ::rename ::oo::define::tmp::initialise initialize\n"
"    ::namespace delete tmp\n"
"    ::namespace export -clear\n"
"}\n"

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

"::oo::define ::oo::class method <cloned> {originObject} {\n"
"    next $originObject\n"
"    # Rebuild the class inheritance delegation class\n"
"    set originDelegate [::oo::DelegateName $originObject]\n"
"    set targetDelegate [::oo::DelegateName [self]]\n"
"    if {[info object isa class $originDelegate] && ![info object isa class $targetDelegate]} {\n"
"        ::oo::copy $originDelegate $targetDelegate\n"
"        ::oo::objdefine [self] mixin -set {*}[lmap c [info object mixin [self]] {\n"
"            if {$c eq $originDelegate} {set targetDelegate} {set c}\n"
"        }]\n"
"    }\n"
"}\n"

"::oo::class create ::oo::singleton {\n"
"    superclass ::oo::class\n"
"    variable object\n"
"    unexport create createWithNamespace\n"
"    method new args {\n"
"        if {![info exists object] || ![info object isa object $object]} {\n"
"            set object [next {*}$args]\n"
"            ::oo::objdefine $object method destroy {} {\n"
"                return -code error {may not destroy a singleton object}\n"
"            }\n"
"            ::oo::objdefine $object method <cloned> {originObject} {\n"
"                return -code error {may not clone a singleton object}\n"
"            }\n"
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
"# Copy over the procedures from the original namespace\n"
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
"# Copy over the variables from the original namespace\n"
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
"}\n"
;

#endif /* TCL_OO_SCRIPT_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
