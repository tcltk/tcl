# This file contains Tcl code to implement a remote server that can be
# used during testing of Tcl socket code. This server is used by some
# of the tests in socket.test.
#
# Source this file in the remote server you are using to test Tcl against.
#
# Copyright © 1995-1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# Initialize message delimiter

# Initialize command array
unset -nocomplain command
set command(0) ""
set callerSocket ""

# Detect whether we should print out connection messages etc.
if {![info exists VERBOSE]} {
    set VERBOSE 0
}

proc __doCommands__ {l s} {
    global callerSocket VERBOSE

    if {$VERBOSE} {
	puts "--- Server executing the following for socket $s:"
	puts $l
	puts "---"
    }
    set callerSocket $s
    set code [catch {
	uplevel "#0" $l
    } msg opt]
    return [list $code [dict getdef $opt -errorinfo ""] $msg]
}

proc __readAndExecute__ {s} {
    global command VERBOSE

    set l [gets $s]
    if {$l eq "--Marker--Marker--Marker--"} {
        puts $s [__doCommands__ $command($s) $s]
	puts $s "--Marker--Marker--Marker--"
        set command($s) ""
	return
    }
    if {$l eq ""} {
	if {[eof $s]} {
	    if {$VERBOSE} {
		puts "Server closing $s, eof from client"
	    }
	    close $s
	}
	return
    }
    if {[eof $s]} {
	if {$VERBOSE} {
	    puts "Server closing $s, eof from client"
	}
	close $s
        unset command($s)
        return
    }
    append command($s) $l "\n"
}

proc __accept__ {s a p} {
    global command VERBOSE

    if {$VERBOSE} {
	puts "Server accepts new connection from $a:$p on $s"
    }
    set command($s) ""
    fconfigure $s -buffering line -translation crlf
    fileevent $s readable [list __readAndExecute__ $s]
}

set serverIsSilent [expr {"-serverIsSilent" in $argv}]
if {![info exists serverPort] && [info exists env(serverPort)]} {
    set serverPort $env(serverPort)
}
if {![info exists serverPort]} {
    for {set i 0} {$i < $argc - 1} {incr i} {
	if {"-port" eq [lindex $argv $i]} {
	    set serverPort [lindex $argv [expr {$i + 1}]]
	    break
	}
    }
}
if {![info exists serverPort]} {
    set serverPort 2048
}

if {![info exists serverAddress] && [info exists env(serverAddress)]} {
    set serverAddress $env(serverAddress)
}
if {![info exists serverAddress]} {
    for {set i 0} {$i < $argc - 1} {incr i} {
	if {"-address" eq [lindex $argv $i]} {
	    set serverAddress [lindex $argv [expr {$i + 1}]]
	    break
	}
    }
}
if {![info exists serverAddress]} {
    set serverAddress 0.0.0.0
}

if {$serverIsSilent == 0} {
    set l "Remote server listening on port $serverPort, IP $serverAddress."
    puts ""
    puts $l
    puts [regsub -all . $l -]
    puts ""
    puts "You have set the Tcl variables serverAddress to $serverAddress and"
    puts "serverPort to $serverPort. You can set these with the -address and"
    puts "-port command line options, or as environment variables in your"
    puts "shell."
    puts ""
    puts "NOTE: The tests will not work properly if serverAddress is set to"
    puts "\"localhost\" or 127.0.0.1."
    puts ""
    puts "When you invoke tcltest to run the tests, set the variables"
    puts "remoteServerPort to $serverPort and remoteServerIP to"
    puts "[info hostname]. You can set these as environment variables"
    puts "from the shell. The tests will not work properly if you set"
    puts "remoteServerIP to \"localhost\" or 127.0.0.1."
    puts ""
    puts -nonewline "Type Ctrl-C to terminate--> "
    flush stdout
}

proc getPort sock {
    lindex [fconfigure $sock -sockname] 2
}

try {
    set serverSocket \
	[socket -myaddr $serverAddress -server __accept__ $serverPort]
} on error msg {
    puts "Server on $serverAddress:$serverPort cannot start: $msg"
    exit 1
}
puts ready
vwait __server_wait_variable__
