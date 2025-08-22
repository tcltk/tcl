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
"::namespace eval ::oo {\n"
"\tproc Helpers::link {args} {\n"
"\t\tset ns [uplevel 1 {::namespace current}]\n"
"\t\tforeach link $args {\n"
"\t\t\tif {[llength $link] == 2} {\n"
"\t\t\t\tlassign $link src dst\n"
"\t\t\t} elseif {[llength $link] == 1} {\n"
"\t\t\t\tlassign $link src\n"
"\t\t\t\tset dst $src\n"
"\t\t\t} else {\n"
"\t\t\t\treturn -code error -errorcode {TCL OO CMDLINK_FORMAT} \\\n"
"\t\t\t\t\t\"bad link description; must only have one or two elements\"\n"
"\t\t\t}\n"
"\t\t\tif {![string match ::* $src]} {\n"
"\t\t\t\tset src [string cat $ns :: $src]\n"
"\t\t\t}\n"
"\t\t\tinterp alias {} $src {} ${ns}::my $dst\n"
"\t\t\ttrace add command ${ns}::my delete [list \\\n"
"\t\t\t\t::oo::UnlinkLinkedCommand $src]\n"
"\t\t}\n"
"\t}\n"
"\tproc UnlinkLinkedCommand {cmd args} {\n"
"\t\tif {[namespace which $cmd] ne {}} {\n"
"\t\t\trename $cmd {}\n"
"\t\t}\n"
"\t}\n"
"\tproc UpdateClassDelegatesAfterClone {originObject targetObject} {\n"
"\t\tset originDelegate [DelegateName $originObject]\n"
"\t\tset targetDelegate [DelegateName $targetObject]\n"
"\t\tif {\n"
"\t\t\t[info object isa class $originDelegate]\n"
"\t\t\t&& ![info object isa class $targetDelegate]\n"
"\t\t} then {\n"
"\t\t\tcopy $originDelegate $targetDelegate\n"
"\t\t\tobjdefine $targetObject ::oo::objdefine::mixin -set \\\n"
"\t\t\t\t{*}[lmap c [info object mixin $targetObject] {\n"
"\t\t\t\t\tif {$c eq $originDelegate} {set targetDelegate} {set c}\n"
"\t\t\t\t}]\n"
"\t\t}\n"
"\t}\n"
"\tproc define::classmethod {name args} {\n"
"\t\t::set argc [::llength [::info level 0]]\n"
"\t\t::if {$argc == 3} {\n"
"\t\t\t::return -code error -errorcode {TCL WRONGARGS} [::format \\\n"
"\t\t\t\t{wrong # args: should be \"%s name \?args body\?\"} \\\n"
"\t\t\t\t[::lindex [::info level 0] 0]]\n"
"\t\t}\n"
"\t\t::set cls [::uplevel 1 self]\n"
"\t\t::if {$argc == 4} {\n"
"\t\t\t::oo::define [::oo::DelegateName $cls] method $name {*}$args\n"
"\t\t}\n"
"\t\t::tailcall forward $name myclass $name\n"
"\t}\n"
"\tdefine Slot forward --default-operation my -append\n"
"\tdefine Slot unexport destroy\n"
"\tobjdefine define::superclass forward --default-operation my -set\n"
"\tobjdefine define::mixin forward --default-operation my -set\n"
"\tobjdefine objdefine::mixin forward --default-operation my -set\n"
"\tdefine object method <cloned> -unexport {originObject} {\n"
"\t\tforeach p [info procs [info object namespace $originObject]::*] {\n"
"\t\t\tset args [info args $p]\n"
"\t\t\tset idx -1\n"
"\t\t\tforeach a $args {\n"
"\t\t\t\tif {[info default $p $a d]} {\n"
"\t\t\t\t\tlset args [incr idx] [list $a $d]\n"
"\t\t\t\t} else {\n"
"\t\t\t\t\tlset args [incr idx] [list $a]\n"
"\t\t\t\t}\n"
"\t\t\t}\n"
"\t\t\tset b [info body $p]\n"
"\t\t\tset p [namespace tail $p]\n"
"\t\t\tproc $p $args $b\n"
"\t\t}\n"
"\t\tforeach v [info vars [info object namespace $originObject]::*] {\n"
"\t\t\tupvar 0 $v vOrigin\n"
"\t\t\tnamespace upvar [namespace current] [namespace tail $v] vNew\n"
"\t\t\tif {[info exists vOrigin]} {\n"
"\t\t\t\tif {[array exists vOrigin]} {\n"
"\t\t\t\t\tarray set vNew [array get vOrigin]\n"
"\t\t\t\t} else {\n"
"\t\t\t\t\tset vNew $vOrigin\n"
"\t\t\t\t}\n"
"\t\t\t}\n"
"\t\t}\n"
"\t}\n"
"\tdefine class method <cloned> -unexport {originObject} {\n"
"\t\tnext $originObject\n"
"\t\t::oo::UpdateClassDelegatesAfterClone $originObject [self]\n"
"\t}\n"
"\tclass create singleton\n"
"\tdefine singleton superclass -set class\n"
"\tdefine singleton variable -set object\n"
"\tdefine singleton unexport create createWithNamespace\n"
"\tdefine singleton method new args {\n"
"\t\tif {![info exists object] || ![info object isa object $object]} {\n"
"\t\t\tset object [next {*}$args]\n"
"\t\t\t::oo::objdefine $object {\n"
"\t\t\t\tmethod destroy {} {\n"
"\t\t\t\t\t::return -code error -errorcode {TCL OO SINGLETON} \\\n"
"\t\t\t\t\t\t\"may not destroy a singleton object\"\n"
"\t\t\t\t}\n"
"\t\t\t\tmethod <cloned> -unexport {originObject} {\n"
"\t\t\t\t\t::return -code error -errorcode {TCL OO SINGLETON} \\\n"
"\t\t\t\t\t\t\"may not clone a singleton object\"\n"
"\t\t\t\t}\n"
"\t\t\t}\n"
"\t\t}\n"
"\t\treturn $object\n"
"\t}\n"
"\tclass create abstract\n"
"\tdefine abstract superclass -set class\n"
"\tdefine abstract unexport create createWithNamespace new\n"
"\tnamespace eval configuresupport::configurableclass {\n"
"\t\t::proc properties args {::tailcall property {*}$args}\n"
"\t\t::namespace path ::oo::define\n"
"\t\t::namespace export property\n"
"\t}\n"
"\tnamespace eval configuresupport::configurableobject {\n"
"\t\t::proc properties args {::tailcall property {*}$args}\n"
"\t\t::namespace path ::oo::objdefine\n"
"\t\t::namespace export property\n"
"\t}\n"
"\tdefine configuresupport::configurable {\n"
"\t\tdefinitionnamespace -instance configuresupport::configurableobject\n"
"\t\tdefinitionnamespace -class configuresupport::configurableclass\n"
"\t}\n"
"\tclass create configurable\n"
"\tdefine configurable superclass -set class\n"
"\tdefine configurable constructor {{definitionScript \"\"}} {\n"
"\t\tnext {mixin ::oo::configuresupport::configurable}\n"
"\t\tnext $definitionScript\n"
"\t}\n"
"\tdefine configurable definitionnamespace -class configuresupport::configurableclass\n"
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
