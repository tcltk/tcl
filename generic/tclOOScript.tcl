# tclOOScript.h --
#
# 	This file contains support scripts for TclOO. They are defined here so
# 	that the code can be definitely run even in safe interpreters; TclOO's
# 	core setup is safe.
#
# Copyright (c) 2012-2018 Donal K. Fellows
# Copyright (c) 2013 Andreas Kupries
# Copyright (c) 2017 Gerald Lester
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

::proc ::oo::Helpers::callback {method args} {
    list [uplevel 1 {namespace which my}] $method {*}$args
}

::proc ::oo::Helpers::mymethod {method args} {
    list [uplevel 1 {namespace which my}] $method {*}$args
}

::proc ::oo::Helpers::classvariable {name args} {
    # Get a reference to the class's namespace
    set ns [info object namespace [uplevel 1 {self class}]]
    # Double up the list of variable names
    foreach v [list $name {*}$args] {
        if {[string match *(*) $v]} {
            return -code error [string cat {bad variable name "} $v {": can't create a scalar variable that looks like an array element}]
        }
        if {[string match *::* $v]} {
            return -code error [string cat {bad variable name "} $v {": can't create a local variable with a namespace separator in it}]
        }
        lappend vs $v $v
    }
    # Lastly, link the caller's local variables to the class's variables
    tailcall namespace upvar $ns {*}$vs
}

::proc ::oo::Helpers::link {args} {
    set ns [uplevel 1 {namespace current}]
    foreach link $args {
        if {[llength $link] == 2} {
            lassign $link src dst
        } else {
            lassign $link src
            set dst $src
        }
        if {![string match ::* $src]} {
            set src [string cat $ns :: $src]
        }
        interp alias {} $src {} ${ns}::my $dst
        trace add command ${ns}::my delete [list ::oo::Helpers::Unlink $src]
    }
    return
}
::proc ::oo::Helpers::Unlink {cmd args} {
    if {[namespace which $cmd] ne {}} {
        rename $cmd {}
    }
}

::proc ::oo::DelegateName {class} {
    string cat [info object namespace $class] {:: oo ::delegate}
}

proc ::oo::MixinClassDelegates {class} {
    if {![info object isa class $class]} {
        return
    }
    set delegate [::oo::DelegateName $class]
    if {![info object isa class $delegate]} {
        return
    }
    foreach c [info class superclass $class] {
        set d [::oo::DelegateName $c]
        if {![info object isa class $d]} {
            continue
        }
        ::oo::define $delegate superclass -append $d
    }
    ::oo::objdefine $class mixin -append $delegate
}

::namespace eval ::oo::define {
    ::proc classmethod {name {args {}} {body {}}} {
        # Create the method on the class if the caller gave arguments and body
        ::set argc [::llength [::info level 0]]
        ::if {$argc == 3} {
            ::return -code error [::string cat {wrong # args: should be "} \
                    [::lindex [::info level 0] 0] { name ?args body?"}]
        }
        ::set cls [::uplevel 1 self]
        ::if {$argc == 4} {
            ::oo::define [::oo::DelegateName $cls] method $name $args $body
        }
        # Make the connection by forwarding
        ::tailcall forward $name myclass $name
    }

    ::proc initialise {body} {
        ::set clsns [::info object namespace [::uplevel 1 self]]
        ::tailcall apply [::list {} $body $clsns]
    }

    # Make the initialise command appear with US spelling too
    ::namespace export initialise
    ::namespace eval tmp {::namespace import ::oo::define::initialise}
    ::rename ::oo::define::tmp::initialise initialize
    ::namespace delete tmp
    ::namespace export -clear
}

::oo::define ::oo::Slot {
    method Get {} {return -code error unimplemented}
    method Set list {return -code error unimplemented}

    method -set args {tailcall my Set $args}
    method -append args {
        set current [uplevel 1 [list [namespace which my] Get]]
        tailcall my Set [list {*}$current {*}$args]
    }
    method -clear {} {tailcall my Set {}}
    forward --default-operation my -append

    method unknown {args} {
        set def --default-operation
        if {[llength $args] == 0} {
            tailcall my $def
        } elseif {![string match -* [lindex $args 0]]} {
            tailcall my $def {*}$args
        }
        next {*}$args
    }

    export -set -append -clear
    unexport unknown destroy
}

::oo::objdefine ::oo::define::superclass forward --default-operation my -set
::oo::objdefine ::oo::define::mixin forward --default-operation my -set
::oo::objdefine ::oo::objdefine::mixin forward --default-operation my -set

::oo::define ::oo::class method <cloned> {originObject} {
    next $originObject
    # Rebuild the class inheritance delegation class
    set originDelegate [::oo::DelegateName $originObject]
    set targetDelegate [::oo::DelegateName [self]]
    if {[info object isa class $originDelegate] && ![info object isa class $targetDelegate]} {
        ::oo::copy $originDelegate $targetDelegate
        ::oo::objdefine [self] mixin -set {*}[lmap c [info object mixin [self]] {
            if {$c eq $originDelegate} {set targetDelegate} {set c}
        }]
    }
}

::oo::class create ::oo::singleton {
    superclass ::oo::class
    variable object
    unexport create createWithNamespace
    method new args {
        if {![info exists object] || ![info object isa object $object]} {
            set object [next {*}$args]
            ::oo::objdefine $object method destroy {} {
                return -code error {may not destroy a singleton object}
            }
            ::oo::objdefine $object method <cloned> {originObject} {
                return -code error {may not clone a singleton object}
            }
        }
        return $object
    }
}

::oo::class create ::oo::abstract {
    superclass ::oo::class
    unexport create createWithNamespace new
}

# Local Variables:
# mode: tcl
# c-basic-offset: 4
# fill-column: 78
# End:
