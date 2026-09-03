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

Tcl\_CreateTimerHandler, TclCreateTimerHandlerMicroSeconds, Tcl\_DeleteTimerHandler - call a procedure at a given time

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_TimerToken]{.ret} [Tcl\_CreateTimerHandler]{.ccmd}[milliseconds, proc, clientData]{.cargs}
[Tcl\_TimerToken]{.ret} [Tcl\_CreateTimerHandlerMicroSeconds]{.ccmd}[microseconds, proc, clientData]{.cargs}
[Tcl\_DeleteTimerHandler]{.ccmd}[token]{.cargs}
:::

# Arguments

::: {.arguments} :::

[microseconds]{.carg .in type="long_long"}
: How many microseconds to wait before invoking *proc*.

[milliseconds]{.carg .in type="int"}
: How many milliseconds to wait before invoking *proc*.

[\*proc]{.carg .in type="Tcl_TimerProc"}
: Procedure to invoke after *milliseconds* have elapsed.

[\*clientData]{.carg .in type="void"}
: Arbitrary one-word value to pass to *proc*.

[token]{.carg .in type="Tcl_TimerToken"}
: Token for previously created timer handler (the return value from some previous call to **Tcl\_CreateTimerHandler**).


:::

# Description

**Tcl\_CreateTimerHandler** arranges for *proc* to be invoked at a time *milliseconds* milliseconds in the future. **Tcl\_CreateTimerHandlerMicroseconds** arranges for *proc* to be invoked at a time *microseconds* microseconds in the future. The callback to *proc* will be made by [Tcl\_DoOneEvent][DoOneEvent], so **Tcl\_CreateTimerHandler** is only useful in programs that dispatch events through [Tcl\_DoOneEvent][DoOneEvent] or through Tcl commands such as [vwait]. The call to *proc* may not be made at the exact time given by the parameter:  it will be made at the next opportunity after that time.  For example, if [Tcl\_DoOneEvent][DoOneEvent] is not called until long after the time has elapsed, or if there are other pending events to process before the call to *proc*, then the call to *proc* will be delayed.

*Proc* should have arguments and return value that match the type **Tcl\_TimerProc**:

```
typedef void Tcl_TimerProc(
        void *clientData);
```

The *clientData* parameter to *proc* is a copy of the *clientData* argument given to **Tcl\_CreateTimerHandler** when the callback was created.  Typically, *clientData* points to a data structure containing application-specific information about what to do in *proc*.

Both functions return a **tolken**. In case of **Tcl\_CreateTimerHandlerMicroSeconds**, the return value is **NULL**, if the given time interval is to much in the future and may not be represented. This is a theoretical case, as it is in around 26 thousand years. **Tcl\_CreateTimerHandler** does not check for overflow and never returns **NULL**.

**Tcl\_DeleteTimerHandler** may be called to delete a previously created timer handler.  It deletes the handler indicated by *token* so that no call to *proc* will be made;  if that handler no longer exists (e.g. because the time period has already elapsed and *proc* has been invoked then **Tcl\_DeleteTimerHandler** does nothing. The tokens returned by **Tcl\_CreateTimerHandler** never have a value of NULL, so if NULL is passed to **Tcl\_DeleteTimerHandler** then the procedure does nothing.


[DoOneEvent]: DoOneEvent.md
[vwait]: vwait.md

