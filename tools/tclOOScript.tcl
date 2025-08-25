# tclOOScript.h --
#
#	This file contains support scripts for TclOO. They are defined here so
#	that the code can be definitely run even in safe interpreters; TclOO's
#	core setup is safe.
#
# Copyright © 2012-2019 Donal K. Fellows
# Copyright © 2013 Andreas Kupries
# Copyright © 2017 Gerald Lester
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

::namespace eval ::oo {
    # ----------------------------------------------------------------------
    #
    # oo::object <cloned> --
    #
    #	Handler for cloning objects that clones basic bits (only!) of the
    #	object's namespace. Non-procedures, traces, sub-namespaces, etc. need
    #	more complex (and class-specific) handling.
    #
    # ----------------------------------------------------------------------

    define object method <cloned> -unexport {originObject} {
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

    define class method <cloned> -unexport {originObject} {
	set targetObject [self]
	next $originObject
	# Rebuild the class inheritance delegation class
	set originDelegate [::oo::DelegateName $originObject]
	set targetDelegate [::oo::DelegateName $targetObject]
	if {
	    [info object isa class $originDelegate]
	    && ![info object isa class $targetDelegate]
	} then {
	    ::oo::copy $originDelegate $targetDelegate
	    ::oo::objdefine $targetObject mixin -set \
		{*}[lmap c [info object mixin $targetObject] {
		    if {$c eq $originDelegate} {set targetDelegate} {set c}
		}]
	}
    }

    # ----------------------------------------------------------------------
    #
    # oo::configuresupport --
    #
    #	Namespace that holds all the implementation details of TIP #558.
    #	Also includes the commands:
    #
    #	 * readableproperties
    #	 * writableproperties
    #	 * objreadableproperties
    #	 * objwritableproperties
    #
    #	These are all slot implementations that provide access to the C layer
    #	of property support (i.e., very fast cached lookup of property names).
    #
    #	 * StdClassProperties
    #	 * StdObjectPropertes
    #
    #	These cause very fast basic implementation methods for a property
    #	following the standard model of property implementation naming.
    #	Property schemes that use other models (such as to be more Tk-like)
    #	should not use these (or the oo::cconfigurable metaclass).
    #
    # ----------------------------------------------------------------------

    # ------------------------------------------------------------------
    #
    # oo::configuresupport::configurableclass,
    # oo::configuresupport::configurableobject --
    #
    #	Namespaces used as implementation vectors for oo::define and
    #	oo::objdefine when the class/instance is configurable.
    #	Note that these also contain commands implemented in C,
    #	especially the [property] definition command.
    #
    # ------------------------------------------------------------------

    namespace eval configuresupport::configurableclass {
	::namespace path ::oo::define
	::namespace export property
    }

    namespace eval configuresupport::configurableobject {
	::namespace path ::oo::objdefine
	::namespace export property
    }

    # ------------------------------------------------------------------
    #
    # oo::configuresupport::configurable --
    #
    #	The class that contains the implementation of the actual
    #	'configure' method (mixed into actually configurable classes).
    #	The 'configure' method is in tclOOBasic.c.
    #
    # ------------------------------------------------------------------

    define configuresupport::configurable {
	definitionnamespace -instance configuresupport::configurableobject
	definitionnamespace -class configuresupport::configurableclass
    }

    # ----------------------------------------------------------------------
    #
    # oo::configurable --
    #
    #	A metaclass that is used to make classes that can be configured in
    #	their creation phase (and later too). All the metaclass itself does is
    #	arrange for the class created to have a 'configure' method and for
    #	oo::define and oo::objdefine (on the class and its instances) to have
    #	a property definition for setting things up for 'configure'.
    #
    # ----------------------------------------------------------------------

    define configurable {
	constructor {{definitionScript ""}} {
	    ::oo::define [self] {mixin -append ::oo::configuresupport::configurable}
	    next $definitionScript
	}
	definitionnamespace -class configuresupport::configurableclass
    }
}

# Local Variables:
# mode: tcl
# c-basic-offset: 4
# fill-column: 78
# End:
