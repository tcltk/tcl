# queue.tcl --
#
#	Queue implementation for Tcl.
#
# Copyright (c) 1998-2000 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: queue.tcl,v 1.1 2000/02/05 03:20:20 ericm Exp $

namespace eval ::struct {}

namespace eval ::struct::queue {
    # The queues array holds all of the queues you've made
    variable queues
    
    # counter is used to give a unique name for unnamed queues
    variable counter 0

    # commands is the list of subcommands recognized by the queue
    variable commands [list \
	    "clear"	\
	    "destroy"	\
	    "get"	\
	    "peek"	\
	    "put"	\
	    "size"	\
	    ]

    # Only export one command, the one used to instantiate a new queue
    namespace export queue
}

# ::struct::queue::queue --
#
#	Create a new queue with a given name; if no name is given, use
#	queueX, where X is a number.
#
# Arguments:
#	name	name of the queue; if null, generate one.
#
# Results:
#	name	name of the queue created

proc ::struct::queue::queue {{name ""}} {
    variable queues
    variable counter
    
    if { [llength [info level 0]] == 1 } {
	incr counter
	set name "queue${counter}"
    }

    if { ![string equal [info commands ::$name] ""] } {
	error "command \"$name\" already exists, unable to create queue"
    }

    # Initialize the queue as empty
    set queues($name) [list ]

    # Create the command to manipulate the queue
    interp alias {} ::$name {} ::struct::queue::QueueProc $name

    return $name
}

##########################
# Private functions follow

# ::struct::queue::QueueProc --
#
#	Command that processes all queue object commands.
#
# Arguments:
#	name	name of the queue object to manipulate.
#	args	command name and args for the command
#
# Results:
#	Varies based on command to perform

proc ::struct::queue::QueueProc {name {cmd ""} args} {
    # Make sure this queue exists
    if { ![info exists ::struct::queue::queues($name)] } {
	error "unknown queue \"$name\""
    }

    # Do minimal args checks here
    if { [llength [info level 0]] == 2 } {
	error "wrong # args: should be \"$name option ?arg arg ...?\""
    }
    
    # Split the args into command and args components
    if { [string equal [info commands ::struct::queue::_$cmd] ""] } {
	variable commands
	set optlist [join $commands ", "]
	set optlist [linsert $optlist "end-1" "or"]
	error "bad option \"$cmd\": must be $optlist"
    }
    return [uplevel 1 [list ::struct::queue::_$cmd $name] $args]
}

# ::struct::queue::_clear --
#
#	Clear a queue.
#
# Arguments:
#	name	name of the queue object.
#
# Results:
#	None.

proc ::struct::queue::_clear {name} {
    variable queues
    set queues($name) [list ]
    return
}

# ::struct::queue::_destroy --
#
#	Destroy a queue object by removing it's storage space and 
#	eliminating it's proc.
#
# Arguments:
#	name	name of the queue object.
#
# Results:
#	None.

proc ::struct::queue::_destroy {name} {
    variable queues
    unset queues($name)
    interp alias {} ::$name {}
    return
}

# ::struct::queue::_get --
#
#	Get an item from a queue.
#
# Arguments:
#	name	name of the queue object.
#	count	number of items to get; defaults to 1
#
# Results:
#	item	first count items from the queue; if there are not enough 
#		items in the queue, throws an error.

proc ::struct::queue::_get {name {count 1}} {
    variable queues
    if { $count < 1 } {
	error "invalid item count $count"
    }

    if { $count > [llength $queues($name)] } {
	error "insufficient items in queue to fill request"
    }

    if { $count == 1 } {
	# Handle this as a special case, so single item gets aren't listified
	set item [lindex $queues($name) 0]
	set queues($name) [lreplace $queues($name) 0 0]
	return $item
    }

    # Otherwise, return a list of items
    set index [expr {$count - 1}]
    set result [lrange $queues($name) 0 $index]
    set queues($name) [lreplace $queues($name) 0 $index]

    return $result
}

# ::struct::queue::_peek --
#
#	Retrive the value of an item on the queue without removing it.
#
# Arguments:
#	name	name of the queue object.
#	count	number of items to peek; defaults to 1
#
# Results:
#	items	top count items from the queue; if there are not enough items
#		to fufill the request, throws an error.

proc ::struct::queue::_peek {name {count 1}} {
    variable queues
    if { $count < 1 } {
	error "invalid item count $count"
    }

    if { $count > [llength $queues($name)] } {
	error "insufficient items in queue to fill request"
    }

    if { $count == 1 } {
	# Handle this as a special case, so single item pops aren't listified
	return [lindex $queues($name) 0]
    }

    # Otherwise, return a list of items
    set index [expr {$count - 1}]
    return [lrange $queues($name) 0 $index]
}

# ::struct::queue::_put --
#
#	Put an item into a queue.
#
# Arguments:
#	name	name of the queue object
#	args	items to put.
#
# Results:
#	None.

proc ::struct::queue::_put {name args} {
    variable queues
    if { [llength $args] == 0 } {
	error "wrong # args: should be \"$name put item ?item ...?\""
    }
    foreach item $args {
	lappend queues($name) $item
    }
    return
}

# ::struct::queue::_size --
#
#	Return the number of objects on a queue.
#
# Arguments:
#	name	name of the queue object.
#
# Results:
#	count	number of items on the queue.

proc ::struct::queue::_size {name} {
    variable queues
    return [llength $queues($name)]
}
