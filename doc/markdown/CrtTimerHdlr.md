---
CommandName: Tcl_CreateTimerHandler
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - after(n)
 - Tcl_CreateFileHandler(3)
 - Tcl_DoWhenIdle(3)
Keywords:
 - callback
 - clock
 - handler
 - timer
Copyright:
 - Copyright (c) 1990 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_CreateTimerHandler, Tcl\_DeleteTimerHandler - call a procedure at a given time

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_TimerToken]{.ret} [Tcl\_CreateTimerHandler]{.ccmd}[milliseconds, proc, clientData]{.cargs}
[Tcl\_DeleteTimerHandler]{.ccmd}[token]{.cargs}
:::

# Arguments

.AP int milliseconds  in How many milliseconds to wait before invoking *proc*. .AP Tcl\_TimerProc \*proc in Procedure to invoke after *milliseconds* have elapsed. .AP void \*clientData in Arbitrary one-word value to pass to *proc*. .AP Tcl\_TimerToken token in Token for previously created timer handler (the return value from some previous call to **Tcl\_CreateTimerHandler**).

# Description

**Tcl\_CreateTimerHandler** arranges for *proc* to be invoked at a time *milliseconds* milliseconds in the future. The callback to *proc* will be made by **Tcl\_DoOneEvent**, so **Tcl\_CreateTimerHandler** is only useful in programs that dispatch events through **Tcl\_DoOneEvent** or through Tcl commands such as [vwait]. The call to *proc* may not be made at the exact time given by *milliseconds*:  it will be made at the next opportunity after that time.  For example, if **Tcl\_DoOneEvent** is not called until long after the time has elapsed, or if there are other pending events to process before the call to *proc*, then the call to *proc* will be delayed.

*Proc* should have arguments and return value that match the type **Tcl\_TimerProc**:

```
typedef void Tcl_TimerProc(
        void *clientData);
```

The *clientData* parameter to *proc* is a copy of the *clientData* argument given to **Tcl\_CreateTimerHandler** when the callback was created.  Typically, *clientData* points to a data structure containing application-specific information about what to do in *proc*.

**Tcl\_DeleteTimerHandler** may be called to delete a previously created timer handler.  It deletes the handler indicated by *token* so that no call to *proc* will be made;  if that handler no longer exists (e.g. because the time period has already elapsed and *proc* has been invoked then **Tcl\_DeleteTimerHandler** does nothing. The tokens returned by **Tcl\_CreateTimerHandler** never have a value of NULL, so if NULL is passed to **Tcl\_DeleteTimerHandler** then the procedure does nothing.


[vwait]: vwait.md

