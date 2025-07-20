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

Tcl_CreateTimerHandler, Tcl_DeleteTimerHandler - call a procedure at a given time

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_TimerToken]{.ret} [Tcl_CreateTimerHandler]{.ccmd}[milliseconds, proc, clientData]{.cargs}
[Tcl_DeleteTimerHandler]{.ccmd}[token]{.cargs}
:::

# Arguments

.AP int milliseconds  in How many milliseconds to wait before invoking *proc*. .AP Tcl_TimerProc *proc in Procedure to invoke after *milliseconds* have elapsed. .AP void *clientData in Arbitrary one-word value to pass to *proc*. .AP Tcl_TimerToken token in Token for previously created timer handler (the return value from some previous call to **Tcl_CreateTimerHandler**).

# Description

**Tcl_CreateTimerHandler** arranges for *proc* to be invoked at a time *milliseconds* milliseconds in the future. The callback to *proc* will be made by **Tcl_DoOneEvent**, so **Tcl_CreateTimerHandler** is only useful in programs that dispatch events through **Tcl_DoOneEvent** or through Tcl commands such as **vwait**. The call to *proc* may not be made at the exact time given by *milliseconds*:  it will be made at the next opportunity after that time.  For example, if **Tcl_DoOneEvent** is not called until long after the time has elapsed, or if there are other pending events to process before the call to *proc*, then the call to *proc* will be delayed.

*Proc* should have arguments and return value that match the type **Tcl_TimerProc**:

```
typedef void Tcl_TimerProc(
        void *clientData);
```

The *clientData* parameter to *proc* is a copy of the *clientData* argument given to **Tcl_CreateTimerHandler** when the callback was created.  Typically, *clientData* points to a data structure containing application-specific information about what to do in *proc*.

**Tcl_DeleteTimerHandler** may be called to delete a previously created timer handler.  It deletes the handler indicated by *token* so that no call to *proc* will be made;  if that handler no longer exists (e.g. because the time period has already elapsed and *proc* has been invoked then **Tcl_DeleteTimerHandler** does nothing. The tokens returned by **Tcl_CreateTimerHandler** never have a value of NULL, so if NULL is passed to **Tcl_DeleteTimerHandler** then the procedure does nothing.

