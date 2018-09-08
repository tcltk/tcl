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

::namespace eval ::oo {
    ::namespace path {}

    #
    # Commands that are made available to objects by default.
    #
    namespace eval Helpers {
	::namespace path {}

	# ------------------------------------------------------------------
	#
	# callback, mymethod --
	#
	#	Create a script prefix that calls a method on the current
	#	object. Same operation, two names.
	#
	# ------------------------------------------------------------------

	proc callback {method args} {
	    list [uplevel 1 {::namespace which my}] $method {*}$args
	}

	# Make the [callback] command appear as [mymethod] too.
	namespace export callback
	namespace eval tmp {namespace import ::oo::Helpers::callback}
	namespace export -clear
	rename tmp::callback mymethod
	namespace delete tmp

	# ------------------------------------------------------------------
	#
	# classvariable --
	#
	#	Link to a variable in the class of the current object.
	#
	# ------------------------------------------------------------------

	proc classvariable {name args} {
	    # Get a reference to the class's namespace
	    set ns [info object namespace [uplevel 1 {self class}]]
	    # Double up the list of variable names
	    foreach v [list $name {*}$args] {
		if {[string match *(*) $v]} {
		    set reason "can't create a scalar variable that looks like an array element"
		    return -code error -errorcode {TCL UPVAR LOCAL_ELEMENT} \
			[format {bad variable name "%s": %s} $v $reason]
		}
		if {[string match *::* $v]} {
		    set reason "can't create a local variable with a namespace separator in it"
		    return -code error -errorcode {TCL UPVAR INVERTED} \
			[format {bad variable name "%s": %s} $v $reason]
		}
		lappend vs $v $v
	    }
	    # Lastly, link the caller's local variables to the class's variables
	    tailcall namespace upvar $ns {*}$vs
	}

	# ------------------------------------------------------------------
	#
	# link --
	#
	#	Make a command that invokes a method on the current object.
	#	The name of the command and the name of the method match by
	#	default.
	#
	# ------------------------------------------------------------------

	proc link {args} {
	    set ns [uplevel 1 {::namespace current}]
	    foreach link $args {
		if {[llength $link] == 2} {
		    lassign $link src dst
		} elseif {[llength $link] == 1} {
		    lassign $link src
		    set dst $src
		} else {
		    return -code error -errorcode {TCLOO CMDLINK FORMAT} \
			"bad link description; must only have one or two elements"
		}
		if {![string match ::* $src]} {
		    set src [string cat $ns :: $src]
		}
		interp alias {} $src {} ${ns}::my $dst
		trace add command ${ns}::my delete [list \
		    ::oo::UnlinkLinkedCommand $src]
	    }
	    return
	}
    }

    # ----------------------------------------------------------------------
    #
    # UnlinkLinkedCommand --
    #
    #	Callback used to remove linked command when the underlying mechanism
    #	that supports it is deleted.
    #
    # ----------------------------------------------------------------------

    proc UnlinkLinkedCommand {cmd args} {
	if {[namespace which $cmd] ne {}} {
	    rename $cmd {}
	}
    }

    # ----------------------------------------------------------------------
    #
    # DelegateName --
    #
    #	Utility that gets the name of the class delegate for a class. It's
    #	trivial, but makes working with them much easier as delegate names are
    #	intentionally hard to create by accident.
    #
    # ----------------------------------------------------------------------

    proc DelegateName {class} {
	string cat [info object namespace $class] {:: oo ::delegate}
    }

    # ----------------------------------------------------------------------
    #
    # MixinClassDelegates --
    #
    #	Support code called *after* [oo::define] inside the constructor of a
    #	class that patches in the appropriate class delegates.
    #
    # ----------------------------------------------------------------------

    proc MixinClassDelegates {class} {
	if {![info object isa class $class]} {
	    return
	}
	set delegate [DelegateName $class]
	if {![info object isa class $delegate]} {
	    return
	}
	foreach c [info class superclass $class] {
	    set d [DelegateName $c]
	    if {![info object isa class $d]} {
		continue
	    }
	    define $delegate superclass -append $d
	}
	objdefine $class mixin -append $delegate
    }

    # ----------------------------------------------------------------------
    #
    # UpdateClassDelegatesAfterClone --
    #
    #	Support code that is like [MixinClassDelegates] except for when a
    #	class is cloned.
    #
    # ----------------------------------------------------------------------

    proc UpdateClassDelegatesAfterClone {originObject targetObject} {
	# Rebuild the class inheritance delegation class
	set originDelegate [DelegateName $originObject]
	set targetDelegate [DelegateName $targetObject]
	if {
	    [info object isa class $originDelegate]
	    && ![info object isa class $targetDelegate]
	} then {
	    copy $originDelegate $targetDelegate
	    objdefine $targetObject mixin -set \
		{*}[lmap c [info object mixin $targetObject] {
		    if {$c eq $originDelegate} {set targetDelegate} {set c}
		}]
	}
    }

    # ----------------------------------------------------------------------
    #
    # oo::define::classmethod --
    #
    #	Defines a class method. See define(n) for details.
    #
    # Note that the ::oo::define namespace is semi-public and a bit weird
    # anyway, so we don't regard the namespace path as being under control:
    # fully qualified names are used for everything.
    #
    # ----------------------------------------------------------------------

    proc define::classmethod {name {args {}} {body {}}} {
        # Create the method on the class if the caller gave arguments and body
        ::set argc [::llength [::info level 0]]
        ::if {$argc == 3} {
            ::return -code error -errorcode {TCL WRONGARGS} [::format \
		{wrong # args: should be "%s name ?args body?"} \
                [::lindex [::info level 0] 0]]
        }
        ::set cls [::uplevel 1 self]
        ::if {$argc == 4} {
            ::oo::define [::oo::DelegateName $cls] method $name $args $body
        }
        # Make the connection by forwarding
        ::tailcall forward $name myclass $name
    }

    # ----------------------------------------------------------------------
    #
    # oo::define::initialise, oo::define::initialize --
    #
    #	Do specific initialisation for a class. See define(n) for details.
    #
    # Note that the ::oo::define namespace is semi-public and a bit weird
    # anyway, so we don't regard the namespace path as being under control:
    # fully qualified names are used for everything.
    #
    # ----------------------------------------------------------------------

    proc define::initialise {body} {
        ::set clsns [::info object namespace [::uplevel 1 self]]
        ::tailcall apply [::list {} $body $clsns]
    }

    # Make the [initialise] definition appear as [initialize] too
    namespace eval define {
	::namespace export initialise
	::namespace eval tmp {::namespace import ::oo::define::initialise}
	::namespace export -clear
	::rename tmp::initialise initialize
	::namespace delete tmp
    }

    # ----------------------------------------------------------------------
    #
    # Slot --
    #
    #	The class of slot operations, which are basically lists at the low
    #	level of TclOO; this provides a more consistent interface to them.
    #
    # ----------------------------------------------------------------------

    define Slot {
	# ------------------------------------------------------------------
	#
	# Slot Get --
	#
	#	Basic slot getter. Retrieves the contents of the slot.
	#	Particular slots must provide concrete non-erroring
	#	implementation.
	#
	# ------------------------------------------------------------------

	method Get {} {
	    return -code error -errorcode {TCLOO ABSTRACT_SLOT} "unimplemented"
	}

	# ------------------------------------------------------------------
	#
	# Slot Set --
	#
	#	Basic slot setter. Sets the contents of the slot.  Particular
	#	slots must provide concrete non-erroring implementation.
	#
	# ------------------------------------------------------------------

	method Set list {
	    return -code error -errorcode {TCLOO ABSTRACT_SLOT} "unimplemented"
	}

	# ------------------------------------------------------------------
	#
	# Slot Resolve --
	#
	#	Helper that lets a slot convert a list of arguments of a
	#	particular type to their canonical forms. Defaults to doing
	#	nothing (suitable for simple strings).
	#
	# ------------------------------------------------------------------

	method Resolve list {
	    return $list
	}

	# ------------------------------------------------------------------
	#
	# Slot -set, -append, -clear, --default-operation --
	#
	#	Standard public slot operations. If a slot can't figure out
	#	what method to call directly, it uses --default-operation.
	#
	# ------------------------------------------------------------------

	method -set args {
	    set args [uplevel 1 [list [namespace which my] Resolve $args]]
	    tailcall my Set $args
	}
	method -append args {
	    set my [namespace which my]
	    set args [uplevel 1 [list $my Resolve $args]]
	    set current [uplevel 1 [list $my Get]]
	    tailcall my Set [list {*}$current {*}$args]
	}
	method -clear {} {tailcall my Set {}}
	method -prepend args {
	    set my [namespace which my]
	    set args [uplevel 1 [list $my Resolve $args]]
	    set current [uplevel 1 [list $my Get]]
	    tailcall my Set [list {*}$args {*}$current]
	}
	method -remove args {
	    set my [namespace which my]
	    set args [uplevel 1 [list $my Resolve $args]]
	    set current [uplevel 1 [list $my Get]]
	    tailcall my Set [lmap val $current {
		if {$val in $args} continue else {set val}
	    }]
	}

	# Default handling
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

	# Set up what is exported and what isn't
	export -set -append -clear -prepend -remove
	unexport unknown destroy
    }

    # Set the default operation differently for these slots
    objdefine define::superclass forward --default-operation my -set
    objdefine define::mixin forward --default-operation my -set
    objdefine objdefine::mixin forward --default-operation my -set

    # ----------------------------------------------------------------------
    #
    # oo::object <cloned> --
    #
    #	Handler for cloning objects that clones basic bits (only!) of the
    #	object's namespace. Non-procedures, traces, sub-namespaces, etc. need
    #	more complex (and class-specific) handling.
    #
    # ----------------------------------------------------------------------

    define object method <cloned> {originObject} {
	# Copy over the procedures from the original namespace
	foreach p [info procs [info object namespace $originObject]::*] {
	    set args [info args $p]
	    set idx -1
	    foreach a $args {
		if {[info default $p $a d]} {
		    lset args [incr idx] [list $a $d]
		} else {
		    lset args [incr idx] [list $a]
		}
	    }
	    set b [info body $p]
	    set p [namespace tail $p]
	    proc $p $args $b
	}
	# Copy over the variables from the original namespace
	foreach v [info vars [info object namespace $originObject]::*] {
	    upvar 0 $v vOrigin
	    namespace upvar [namespace current] [namespace tail $v] vNew
	    if {[info exists vOrigin]} {
		if {[array exists vOrigin]} {
		    array set vNew [array get vOrigin]
		} else {
		    set vNew $vOrigin
		}
	    }
	}
	# General commands, sub-namespaces and advancd variable config (traces,
	# etc) are *not* copied over. Classes that want that should do it
	# themselves.
    }

    # ----------------------------------------------------------------------
    #
    # oo::class <cloned> --
    #
    #	Handler for cloning classes, which fixes up the delegates.
    #
    # ----------------------------------------------------------------------

    define class method <cloned> {originObject} {
	next $originObject
	# Rebuild the class inheritance delegation class
	::oo::UpdateClassDelegatesAfterClone $originObject [self]
    }

    # ----------------------------------------------------------------------
    #
    # oo::singleton --
    #
    #	A metaclass that is used to make classes that only permit one instance
    #	of them to exist. See singleton(n).
    #
    # ----------------------------------------------------------------------

    class create singleton {
	superclass class
	variable object
	unexport create createWithNamespace
	method new args {
	    if {![info exists object] || ![info object isa object $object]} {
		set object [next {*}$args]
		::oo::objdefine $object {
		    method destroy {} {
			::return -code error -errorcode {TCLOO SINGLETON} \
			    "may not destroy a singleton object"
		    }
		    method <cloned> {originObject} {
			::return -code error -errorcode {TCLOO SINGLETON} \
			    "may not clone a singleton object"
		    }
		}
	    }
	    return $object
	}
    }

    # ----------------------------------------------------------------------
    #
    # oo::abstract --
    #
    #	A metaclass that is used to make classes that can't be directly
    #	instantiated. See abstract(n).
    #
    # ----------------------------------------------------------------------

    class create abstract {
	superclass class
	unexport create createWithNamespace new
    }
}

# Local Variables:
# mode: tcl
# c-basic-offset: 4
# fill-column: 78
# End:
