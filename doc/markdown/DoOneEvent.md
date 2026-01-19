---
CommandName: Tcl_DoOneEvent
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - callback
 - event
 - handler
 - idle
 - timer
Copyright:
 - Copyright (c) 1990-1992 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_DoOneEvent - wait for events and invoke event handlers

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_DoOneEvent]{.ccmd}[flags]{.cargs}
:::

# Arguments

.AP int flags in This parameter is normally zero.  It may be an OR-ed combination of any of the following flag bits: **TCL\_WINDOW\_EVENTS**, **TCL\_FILE\_EVENTS**, **TCL\_TIMER\_EVENTS**, **TCL\_IDLE\_EVENTS**, **TCL\_ALL\_EVENTS**, or **TCL\_DONT\_WAIT**. 

# Description

This procedure is the entry point to Tcl's event loop; it is responsible for waiting for events and dispatching event handlers created with procedures such as **Tk\_CreateEventHandler**, **Tcl\_CreateFileHandler**, **Tcl\_CreateTimerHandler**, and **Tcl\_DoWhenIdle**. **Tcl\_DoOneEvent** checks to see if events are already present on the Tcl event queue; if so, it calls the handler(s) for the first (oldest) event, removes it from the queue, and returns. If there are no events ready to be handled, then **Tcl\_DoOneEvent** checks for new events from all possible sources. If any are found, it puts all of them on Tcl's event queue, calls handlers for the first event on the queue, and returns. If no events are found, **Tcl\_DoOneEvent** checks for **Tcl\_DoWhenIdle** callbacks; if any are found, it invokes all of them and returns. Finally, if no events or idle callbacks have been found, then **Tcl\_DoOneEvent** sleeps until an event occurs; then it adds any new events to the Tcl event queue, calls handlers for the first event, and returns. The normal return value is 1 to signify that some event was processed (see below for other alternatives).

If the *flags* argument to **Tcl\_DoOneEvent** is non-zero, it restricts the kinds of events that will be processed by **Tcl\_DoOneEvent**. *Flags* may be an OR-ed combination of any of the following bits:

**TCL\_WINDOW\_EVENTS**
: Process window system events.

**TCL\_FILE\_EVENTS**
: Process file events.

**TCL\_TIMER\_EVENTS**
: Process timer events.

**TCL\_IDLE\_EVENTS**
: Process idle callbacks.

**TCL\_ALL\_EVENTS**
: Process all kinds of events:  equivalent to OR-ing together all of the above flags or specifying none of them.

**TCL\_DONT\_WAIT**
: Do not sleep:  process only events that are ready at the time of the call.


If any of the flags **TCL\_WINDOW\_EVENTS**, **TCL\_FILE\_EVENTS**, **TCL\_TIMER\_EVENTS**, or **TCL\_IDLE\_EVENTS** is set, then the only events that will be considered are those for which flags are set. Setting none of these flags is equivalent to the value **TCL\_ALL\_EVENTS**, which causes all event types to be processed. If an application has defined additional event sources with **Tcl\_CreateEventSource**, then additional *flag* values may also be valid, depending on those event sources.

The **TCL\_DONT\_WAIT** flag causes **Tcl\_DoOneEvent** not to put the process to sleep:  it will check for events but if none are found then it returns immediately with a return value of 0 to indicate that no work was done. **Tcl\_DoOneEvent** will also return 0 without doing anything if the only alternative is to block forever (this can happen, for example, if *flags* is **TCL\_IDLE\_EVENTS** and there are no **Tcl\_DoWhenIdle** callbacks pending, or if no event handlers or timer handlers exist).

**Tcl\_DoOneEvent** may be invoked recursively.  For example, it is possible to invoke **Tcl\_DoOneEvent** recursively from a handler called by **Tcl\_DoOneEvent**.  This sort of operation is useful in some modal situations, such as when a notification dialog has been popped up and an application wishes to wait for the user to click a button in the dialog before doing anything else. 

