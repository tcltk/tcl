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

Tcl_DoOneEvent - wait for events and invoke event handlers

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_DoOneEvent]{.ccmd}[flags]{.cargs}
:::

# Arguments

.AP int flags in This parameter is normally zero.  It may be an OR-ed combination of any of the following flag bits: **TCL_WINDOW_EVENTS**, **TCL_FILE_EVENTS**, **TCL_TIMER_EVENTS**, **TCL_IDLE_EVENTS**, **TCL_ALL_EVENTS**, or **TCL_DONT_WAIT**. 

# Description

This procedure is the entry point to Tcl's event loop; it is responsible for waiting for events and dispatching event handlers created with procedures such as **Tk_CreateEventHandler**, **Tcl_CreateFileHandler**, **Tcl_CreateTimerHandler**, and **Tcl_DoWhenIdle**. **Tcl_DoOneEvent** checks to see if events are already present on the Tcl event queue; if so, it calls the handler(s) for the first (oldest) event, removes it from the queue, and returns. If there are no events ready to be handled, then **Tcl_DoOneEvent** checks for new events from all possible sources. If any are found, it puts all of them on Tcl's event queue, calls handlers for the first event on the queue, and returns. If no events are found, **Tcl_DoOneEvent** checks for **Tcl_DoWhenIdle** callbacks; if any are found, it invokes all of them and returns. Finally, if no events or idle callbacks have been found, then **Tcl_DoOneEvent** sleeps until an event occurs; then it adds any new events to the Tcl event queue, calls handlers for the first event, and returns. The normal return value is 1 to signify that some event was processed (see below for other alternatives).

If the *flags* argument to **Tcl_DoOneEvent** is non-zero, it restricts the kinds of events that will be processed by **Tcl_DoOneEvent**. *Flags* may be an OR-ed combination of any of the following bits:

**TCL_WINDOW_EVENTS**
: Process window system events.

**TCL_FILE_EVENTS**
: Process file events.

**TCL_TIMER_EVENTS**
: Process timer events.

**TCL_IDLE_EVENTS**
: Process idle callbacks.

**TCL_ALL_EVENTS**
: Process all kinds of events:  equivalent to OR-ing together all of the above flags or specifying none of them.

**TCL_DONT_WAIT**
: Do not sleep:  process only events that are ready at the time of the call. .LP If any of the flags **TCL_WINDOW_EVENTS**, **TCL_FILE_EVENTS**, **TCL_TIMER_EVENTS**, or **TCL_IDLE_EVENTS** is set, then the only events that will be considered are those for which flags are set. Setting none of these flags is equivalent to the value **TCL_ALL_EVENTS**, which causes all event types to be processed. If an application has defined additional event sources with **Tcl_CreateEventSource**, then additional *flag* values may also be valid, depending on those event sources.


The **TCL_DONT_WAIT** flag causes **Tcl_DoOneEvent** not to put the process to sleep:  it will check for events but if none are found then it returns immediately with a return value of 0 to indicate that no work was done. **Tcl_DoOneEvent** will also return 0 without doing anything if the only alternative is to block forever (this can happen, for example, if *flags* is **TCL_IDLE_EVENTS** and there are no **Tcl_DoWhenIdle** callbacks pending, or if no event handlers or timer handlers exist).

**Tcl_DoOneEvent** may be invoked recursively.  For example, it is possible to invoke **Tcl_DoOneEvent** recursively from a handler called by **Tcl_DoOneEvent**.  This sort of operation is useful in some modal situations, such as when a notification dialog has been popped up and an application wishes to wait for the user to click a button in the dialog before doing anything else. 

