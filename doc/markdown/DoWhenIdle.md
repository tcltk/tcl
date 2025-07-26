---
CommandName: Tcl_DoWhenIdle
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - after(n)
 - Tcl_CreateFileHandler(3)
 - Tcl_CreateTimerHandler(3)
Keywords:
 - callback
 - defer
 - idle callback
Copyright:
 - Copyright (c) 1990 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_DoWhenIdle, Tcl_CancelIdleCall - invoke a procedure when there are no pending events

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_DoWhenIdle]{.ccmd}[proc, clientData]{.cargs}
[Tcl_CancelIdleCall]{.ccmd}[proc, clientData]{.cargs}
:::

# Arguments

.AP Tcl_IdleProc *proc in Procedure to invoke. .AP void *clientData in Arbitrary one-word value to pass to *proc*.

# Description

**Tcl_DoWhenIdle** arranges for *proc* to be invoked when the application becomes idle.  The application is considered to be idle when **Tcl_DoOneEvent** has been called, could not find any events to handle, and is about to go to sleep waiting for an event to occur.  At this point all pending **Tcl_DoWhenIdle** handlers are invoked.  For each call to **Tcl_DoWhenIdle** there will be a single call to *proc*;  after *proc* is invoked the handler is automatically removed. **Tcl_DoWhenIdle** is only usable in programs that use **Tcl_DoOneEvent** to dispatch events.

*Proc* should have arguments and result that match the type **Tcl_IdleProc**:

```
typedef void Tcl_IdleProc(
        void *clientData);
```

The *clientData* parameter to *proc* is a copy of the *clientData* argument given to **Tcl_DoWhenIdle**.  Typically, *clientData* points to a data structure containing application-specific information about what *proc* should do.

**Tcl_CancelIdleCall** may be used to cancel one or more previous calls to **Tcl_DoWhenIdle**:  if there is a **Tcl_DoWhenIdle** handler registered for *proc* and *clientData*, then it is removed without invoking it.  If there is more than one handler on the idle list that refers to *proc* and *clientData*, all of the handlers are removed.  If no existing handlers match *proc* and *clientData* then nothing happens.

**Tcl_DoWhenIdle** is most useful in situations where (a) a piece of work will have to be done but (b) it is possible that something will happen in the near future that will change what has to be done or require something different to be done.  **Tcl_DoWhenIdle** allows the actual work to be deferred until all pending events have been processed.  At this point the exact work to be done will presumably be known and it can be done exactly once.

For example, **Tcl_DoWhenIdle** might be used by an editor to defer display updates until all pending commands have been processed.  Without this feature, redundant redisplays might occur in some situations, such as the processing of a command file.

# Bugs

At present it is not safe for an idle callback to reschedule itself continuously.  This will interact badly with certain features of Tk that attempt to wait for all idle callbacks to complete.  If you would like for an idle callback to reschedule itself continuously, it is better to use a timer handler with a zero timeout period.

