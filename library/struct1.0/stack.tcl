# stack.tcl --
#
#	Stack implementation for Tcl.
#
# Copyright (c) 1998-2000 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: stack.tcl,v 1.1 2000/02/05 03:20:20 ericm Exp $

namespace eval ::struct {}

namespace eval ::struct::stack {
    # The stacks array holds all of the stacks you've made
    variable stacks
    
    # counter is used to give a unique name for unnamed stacks
    variable counter 0

    # commands is the list of subcommands recognized by the stack
    variable commands [list \
	    "clear"	\
	    "destroy"	\
	    "peek"	\
	    "pop"	\
	    "push"	\
	    "rotate"	\
	    "size"	\
	    ]

    # Only export one command, the one used to instantiate a new stack
    namespace export stack
}

# ::struct::stack::stack --
#
#	Create a new stack with a given name; if no name is given, use
#	stackX, where X is a number.
#
# Arguments:
#	name	name of the stack; if null, generate one.
#
# Results:
#	name	name of the stack created

proc ::struct::stack::stack {{name ""}} {
    variable stacks
    variable counter
    
    if { [llength [info level 0]] == 1 } {
	incr counter
	set name "stack${counter}"
    }

    if { ![string equal [info commands ::$name] ""] } {
	error "command \"$name\" already exists, unable to create stack"
    }
    set stacks($name) [list ]

    # Create the command to manipulate the stack
    interp alias {} ::$name {} ::struct::stack::StackProc $name

    return $name
}

##########################
# Private functions follow

# ::struct::stack::StackProc --
#
#	Command that processes all stack object commands.
#
# Arguments:
#	name	name of the stack object to manipulate.
#	args	command name and args for the command
#
# Results:
#	Varies based on command to perform

proc ::struct::stack::StackProc {name {cmd ""} args} {
    # Make sure this stack exists
    if { ![info exists ::struct::stack::stacks($name)] } {
	error "unknown stack \"$name\""
    }

    # Do minimal args checks here
    if { [llength [info level 0]] == 2 } {
	error "wrong # args: should be \"$name option ?arg arg ...?\""
    }
    
    # Split the args into command and args components
    if { [llength [info commands ::struct::stack::_$cmd]] == 0 } {
	variable commands
	set optlist [join $commands ", "]
	set optlist [linsert $optlist "end-1" "or"]
	error "bad option \"$cmd\": must be $optlist"
    }
    return [eval [list ::struct::stack::_$cmd $name] $args]
}

# ::struct::stack::_clear --
#
#	Clear a stack.
#
# Arguments:
#	name	name of the stack object.
#
# Results:
#	None.

proc ::struct::stack::_clear {name} {
    set ::struct::stack::stacks($name) [list ]
    return
}

# ::struct::stack::_destroy --
#
#	Destroy a stack object by removing it's storage space and 
#	eliminating it's proc.
#
# Arguments:
#	name	name of the stack object.
#
# Results:
#	None.

proc ::struct::stack::_destroy {name} {
    unset ::struct::stack::stacks($name)
    interp alias {} ::$name {}
    return
}

# ::struct::stack::_peek --
#
#	Retrive the value of an item on the stack without popping it.
#
# Arguments:
#	name	name of the stack object.
#	count	number of items to pop; defaults to 1
#
# Results:
#	items	top count items from the stack; if there are not enough items
#		to fufill the request, throws an error.

proc ::struct::stack::_peek {name {count 1}} {
    variable stacks
    if { $count < 1 } {
	error "invalid item count $count"
    }

    if { $count > [llength $stacks($name)] } {
	error "insufficient items on stack to fill request"
    }

    if { $count == 1 } {
	# Handle this as a special case, so single item pops aren't listified
	set item [lindex $stacks($name) end]
	return $item
    }

    # Otherwise, return a list of items
    set result [list ]
    for {set i 0} {$i < $count} {incr i} {
	lappend result [lindex $stacks($name) "end-${i}"]
    }
    return $result
}

# ::struct::stack::_pop --
#
#	Pop an item off a stack.
#
# Arguments:
#	name	name of the stack object.
#	count	number of items to pop; defaults to 1
#
# Results:
#	item	top count items from the stack; if the stack is empty, 
#		returns a list of count nulls.

proc ::struct::stack::_pop {name {count 1}} {
    variable stacks
    if { $count > [llength $stacks($name)] } {
	error "insufficient items on stack to fill request"
    } elseif { $count < 1 } {
	error "invalid item count $count"
    }

    if { $count == 1 } {
	# Handle this as a special case, so single item pops aren't listified
	set item [lindex $stacks($name) end]
	set stacks($name) [lreplace $stacks($name) end end]
	return $item
    }

    # Otherwise, return a list of items
    set result [list ]
    for {set i 0} {$i < $count} {incr i} {
	lappend result [lindex $stacks($name) "end-${i}"]
    }

    # Remove these items from the stack
    incr i -1
    set stacks($name) [lreplace $stacks($name) "end-${i}" end]

    return $result
}

# ::struct::stack::_push --
#
#	Push an item onto a stack.
#
# Arguments:
#	name	name of the stack object
#	args	items to push.
#
# Results:
#	None.

proc ::struct::stack::_push {name args} {
    if { [llength $args] == 0 } {
	error "wrong # args: should be \"$name push item ?item ...?\""
    }
    foreach item $args {
	lappend ::struct::stack::stacks($name) $item
    }
    return
}

# ::struct::stack::_rotate --
#
#	Rotate the top count number of items by step number of steps.
#
# Arguments:
#	name	name of the stack object.
#	count	number of items to rotate.
#	steps	number of steps to rotate.
#
# Results:
#	None.

proc ::struct::stack::_rotate {name count steps} {
    variable stacks
    set len [llength $stacks($name)]
    if { $count > $len } {
	error "insufficient items on stack to fill request"
    }

    # Rotation algorithm:
    # do
    #   Find the insertion point in the stack
    #   Move the end item to the insertion point
    # repeat $steps times

    set start [expr {$len - $count}]
    set steps [expr {$steps % $count}]
    for {set i 0} {$i < $steps} {incr i} {
	set item [lindex $stacks($name) end]
	set stacks($name) [lreplace $stacks($name) end end]
	set stacks($name) [linsert $stacks($name) $start $item]
    }
    return
}

# ::struct::stack::_size --
#
#	Return the number of objects on a stack.
#
# Arguments:
#	name	name of the stack object.
#
# Results:
#	count	number of items on the stack.

proc ::struct::stack::_size {name} {
    return [llength $::struct::stack::stacks($name)]
}
