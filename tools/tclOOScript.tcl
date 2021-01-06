# tclOOScript.h --
#
# 	This file contains support scripts for TclOO. They are defined here so
# 	that the code can be definitely run even in safe interpreters; TclOO's
# 	core setup is safe.
#
# Copyright (c) 2012-2019 Donal K. Fellows
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
	namespace path {}

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
	    define $delegate ::oo::define::superclass -appendifnew $d
	}
	objdefine $class ::oo::objdefine::mixin -appendifnew $delegate
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
	    objdefine $targetObject ::oo::objdefine::mixin -set \
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

	method Get -unexport {} {
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

	method Set -unexport list {
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

	method Resolve -unexport list {
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

	method -set -export args {
	    set my [namespace which my]
	    set args [lmap a $args {uplevel 1 [list $my Resolve $a]}]
	    tailcall my Set $args
	}
	method -append -export args {
	    set my [namespace which my]
	    set args [lmap a $args {uplevel 1 [list $my Resolve $a]}]
	    set current [uplevel 1 [list $my Get]]
	    tailcall my Set [list {*}$current {*}$args]
	}
	method -appendifnew -export args {
	    set my [namespace which my]
	    set current [uplevel 1 [list $my Get]]
	    foreach a $args {
		set a [uplevel 1 [list $my Resolve $a]]
		if {$a ni $current} {
		    lappend current $a
		}
	    }
	    tailcall my Set $current
	}
	method -clear -export {} {tailcall my Set {}}
	method -prepend -export args {
	    set my [namespace which my]
	    set args [lmap a $args {uplevel 1 [list $my Resolve $a]}]
	    set current [uplevel 1 [list $my Get]]
	    tailcall my Set [list {*}$args {*}$current]
	}
	method -remove -export args {
	    set my [namespace which my]
	    set args [lmap a $args {uplevel 1 [list $my Resolve $a]}]
	    set current [uplevel 1 [list $my Get]]
	    tailcall my Set [lmap val $current {
		if {$val in $args} continue else {set val}
	    }]
	}

	# Default handling
	forward --default-operation my -append
	method unknown -unexport {args} {
	    set def --default-operation
	    if {[llength $args] == 0} {
		tailcall my $def
	    } elseif {![string match -* [lindex $args 0]]} {
		tailcall my $def {*}$args
	    }
	    next {*}$args
	}

	# Hide destroy
	unexport destroy
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
		    method <cloned> -unexport {originObject} {
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
    #	Those are all slot implementations that provide access to the C layer
    #	of property support (i.e., very fast cached lookup of property names).
    #
    # ----------------------------------------------------------------------

    ::namespace eval configuresupport {
	namespace path ::tcl

	# ------------------------------------------------------------------
	#
	# oo::configuresupport --
	#
	#	A metaclass that is used to make classes that can be configured.
	#
	# ------------------------------------------------------------------

	proc PropertyImpl {readslot writeslot args} {
	    for {set i 0} {$i < [llength $args]} {incr i} {
		# Parse the property name
		set prop [lindex $args $i]
		if {[string match "-*" $prop]} {
		    return -code error -level 2 \
			-errorcode {TCLOO PROPERTY_FORMAT} \
			"bad property name \"$prop\": must not begin with -"
		}
		if {$prop ne [list $prop]} {
		    return -code error -level 2 \
			-errorcode {TCLOO PROPERTY_FORMAT} \
			"bad property name \"$prop\": must be a simple word"
		}
		if {[string first "::" $prop] != -1} {
		    return -code error -level 2 \
			-errorcode {TCLOO PROPERTY_FORMAT} \
			"bad property name \"$prop\": must not contain namespace separators"
		}
		if {[string match {*[()]*} $prop]} {
		    return -code error -level 2 \
			-errorcode {TCLOO PROPERTY_FORMAT} \
			"bad property name \"$prop\": must not contain parentheses"
		}
		set realprop [string cat "-" $prop]
		set getter [format {::set [my varname %s]} $prop]
		set setter [format {::set [my varname %s] $value} $prop]
		set kind readwrite

		# Parse the extra options
		while {[set next [lindex $args [expr {$i + 1}]]
			string match "-*" $next]} {
		    set arg [lindex $args [incr i 2]]
		    switch [prefix match -error [list -level 2 -errorcode \
			    [list TCL LOOKUP INDEX option $next]] {-get -kind -set} $next] {
			-get {
			    if {$i >= [llength $args]} {
				return -code error -level 2 \
				    -errorcode {TCL WRONGARGS} \
				    "missing body to go with -get option"
			    }
			    set getter $arg
			}
			-set {
			    if {$i >= [llength $args]} {
				return -code error -level 2 \
				    -errorcode {TCL WRONGARGS} \
				    "missing body to go with -set option"
			    }
			    set setter $arg
			}
			-kind {
			    if {$i >= [llength $args]} {
				return -code error -level 2\
				    -errorcode {TCL WRONGARGS} \
				    "missing kind value to go with -kind option"
			    }
			    set kind [prefix match -message "kind" -error [list \
				    -level 2 \
				    -errorcode [list TCL LOOKUP INDEX kind $arg]] {
				readable readwrite writable
			    } $arg]
			}
		    }
		}

		# Install the option
		set reader <ReadProp$realprop>
		set writer <WriteProp$realprop>
		switch $kind {
		    readable {
			uplevel 2 [list $readslot -append $realprop]
			uplevel 2 [list $writeslot -remove $realprop]
			uplevel 2 [list method $reader -unexport {} $getter]
		    }
		    writable {
			uplevel 2 [list $readslot -remove $realprop]
			uplevel 2 [list $writeslot -append $realprop]
			uplevel 2 [list method $writer -unexport {value} $setter]
		    }
		    readwrite {
			uplevel 2 [list $readslot -append $realprop]
			uplevel 2 [list $writeslot -append $realprop]
			uplevel 2 [list method $reader -unexport {} $getter]
			uplevel 2 [list method $writer -unexport {value} $setter]
		    }
		}
	    }
	}

	# ------------------------------------------------------------------
	#
	# oo::configuresupport::configurableclass,
	# oo::configuresupport::configurableobject --
	#
	#	Namespaces used as implementation vectors for oo::define and
	#	oo::objdefine when the class/instance is configurable.
	#
	# ------------------------------------------------------------------

	namespace eval configurableclass {
	    ::proc property args {
		::oo::configuresupport::PropertyImpl \
		    ::oo::configuresupport::readableproperties \
		    ::oo::configuresupport::writableproperties {*}$args
	    }
	    # Plural alias just in case; deliberately NOT documented!
	    ::proc properties args {::tailcall property {*}$args}
	    ::namespace path ::oo::define
	    ::namespace export property
	}

	namespace eval configurableobject {
	    ::proc property args {
		::oo::configuresupport::PropertyImpl \
		    ::oo::configuresupport::objreadableproperties \
		    ::oo::configuresupport::objwritableproperties {*}$args
	    }
	    # Plural alias just in case; deliberately NOT documented!
	    ::proc properties args {::tailcall property {*}$args}
	    ::namespace path ::oo::objdefine
	    ::namespace export property
	}

	# ------------------------------------------------------------------
	#
	# oo::configuresupport::ReadAll --
	#
	#	The implementation of [$o configure] with no extra arguments.
	#
	# ------------------------------------------------------------------

	proc ReadAll {object my} {
	    set result {}
	    foreach prop [info object properties $object -all -readable] {
		try {
		    dict set result $prop [$my <ReadProp$prop>]
		} on error {msg opt} {
		    dict set opt -level 2
		    return -options $opt $msg
		} on return {msg opt} {
		    dict incr opt -level 2
		    return -options $opt $msg
		} on break {} {
		    return -code error -level 2 -errorcode {TCLOO SHENANIGANS} \
			"property getter for $prop did a break"
		} on continue {} {
		    return -code error -level 2 -errorcode {TCLOO SHENANIGANS} \
			"property getter for $prop did a continue"
		}
	    }
	    return $result
	}

	# ------------------------------------------------------------------
	#
	# oo::configuresupport::ReadOne --
	#
	#	The implementation of [$o configure -prop] with that single
	#	extra argument.
	#
	# ------------------------------------------------------------------

	proc ReadOne {object my propertyName} {
	    set props [info object properties $object -all -readable]
	    try {
		set prop [prefix match -message "property" $props $propertyName]
	    } on error {msg} {
		catch {
		    set wps [info object properties $object -all -writable]
		    set wprop [prefix match $wps $propertyName]
		    set msg "property \"$wprop\" is write only"
		}
		return -code error -level 2 -errorcode [list \
			TCL LOOKUP INDEX property $propertyName] $msg
	    }
	    try {
		set value [$my <ReadProp$prop>]
	    } on error {msg opt} {
		dict set opt -level 2
		return -options $opt $msg
	    } on return {msg opt} {
		dict incr opt -level 2
		return -options $opt $msg
	    } on break {} {
		return -code error -level 2 -errorcode {TCLOO SHENANIGANS} \
		    "property getter for $prop did a break"
	    } on continue {} {
		return -code error -level 2 -errorcode {TCLOO SHENANIGANS} \
		    "property getter for $prop did a continue"
	    }
	    return $value
	}

	# ------------------------------------------------------------------
	#
	# oo::configuresupport::WriteMany --
	#
	#	The implementation of [$o configure -prop val ?-prop val...?].
	#
	# ------------------------------------------------------------------

	proc WriteMany {object my setterMap} {
	    set props [info object properties $object -all -writable]
	    foreach {prop value} $setterMap {
		try {
		    set prop [prefix match -message "property" $props $prop]
		} on error {msg} {
		    catch {
			set rps [info object properties $object -all -readable]
			set rprop [prefix match $rps $prop]
			set msg "property \"$rprop\" is read only"
		    }
		    return -code error -level 2 -errorcode [list \
			    TCL LOOKUP INDEX property $prop] $msg
		}
		try {
		    $my <WriteProp$prop> $value
		} on error {msg opt} {
		    dict set opt -level 2
		    return -options $opt $msg
		} on return {msg opt} {
		    dict incr opt -level 2
		    return -options $opt $msg
		} on break {} {
		    return -code error -level 2 -errorcode {TCLOO SHENANIGANS} \
			"property setter for $prop did a break"
		} on continue {} {
		    return -code error -level 2 -errorcode {TCLOO SHENANIGANS} \
			"property setter for $prop did a continue"
		}
	    }
	    return
	}

	# ------------------------------------------------------------------
	#
	# oo::configuresupport::configurable --
	#
	#	The class that contains the implementation of the actual
	#	'configure' method (mixed into actually configurable classes).
	#	Great care needs to be taken in these methods as they are
	#	potentially used in classes where the current namespace is set
	#	up very strangely.
	#
	# ------------------------------------------------------------------

	::oo::class create configurable {
	    private variable my
	    #
	    # configure --
	    #	Method for providing client access to the property mechanism.
	    #	Has a user-facing API similar to that of [chan configure].
	    #
	    method configure -export args {
		::if {![::info exists my]} {
		    ::set my [::namespace which my]
		}
		::if {[::llength $args] == 0} {
		    # Read all properties
		    ::oo::configuresupport::ReadAll [self] $my
		} elseif {[::llength $args] == 1} {
		    # Read a single property
		    ::oo::configuresupport::ReadOne [self] $my \
			[::lindex $args 0]
		} elseif {[::llength $args] % 2 == 0} {
		    # Set properties, one or several
		    ::oo::configuresupport::WriteMany [self] $my $args
		} else {
		    # Invalid call
		    ::return -code error -errorcode {TCL WRONGARGS} \
			[::format {wrong # args: should be "%s"} \
			    "[self] configure ?-option value ...?"]
		}
	    }

	    definitionnamespace -instance configurableobject
	    definitionnamespace -class configurableclass
	}
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

    class create configurable {
	superclass class

	constructor {{definitionScript ""}} {
	    next {mixin ::oo::configuresupport::configurable}
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
