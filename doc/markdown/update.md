---
CommandName: update
ManualSection: n
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - after(n)
 - interp(n)
Keywords:
 - asynchronous I/O
 - event
 - flush
 - handler
 - idle
 - update
Copyright:
 - Copyright (c) 1990-1992 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

update - Process pending events and idle callbacks

# Synopsis

::: {.synopsis} :::
[update]{.cmd} [idletasks]{.optlit}
:::

# Description

This command is used to bring the application "up to date" by entering the event loop repeatedly until all pending events (including idle callbacks) have been processed.

If the \fBidletasks\fR keyword is specified as an argument to the command, then no new events or errors are processed;  only idle callbacks are invoked. This causes operations that are normally deferred, such as display updates and window layout calculations, to be performed immediately.

The \fBupdate idletasks\fR command is useful in scripts where changes have been made to the application's state and you want those changes to appear on the display immediately, rather than waiting for the script to complete.  Most display updates are performed as idle callbacks, so \fBupdate idletasks\fR will cause them to run. However, there are some kinds of updates that only happen in response to events, such as those triggered by window size changes; these updates will not occur in \fBupdate idletasks\fR.

The \fBupdate\fR command with no options is useful in scripts where you are performing a long-running computation but you still want the application to respond to events such as user interactions;  if you occasionally call \fBupdate\fR then user input will be processed during the next call to \fBupdate\fR.

# Example

Run computations for about a second and then finish:

```
set x 1000
set done 0
after 1000 set done 1
while {!$done} {
    # A very silly example!
    set x [expr {log($x) ** 2.8}]

    # Test to see if our time-limit has been hit.  This would
    # also give a chance for serving network sockets and, if
    # the Tk package is loaded, updating a user interface.
    update
}
```

