---
CommandName: Tcl_Sleep
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - sleep
 - time
 - wait
Copyright:
 - Copyright (c) 1990 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_Sleep - delay execution for a given number of milliseconds

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Sleep]{.ccmd}[ms]{.cargs}
[Tcl\_SleepMicroSeconds]{.ccmd}[us]{.cargs}
:::

# Arguments

::: {.arguments} :::

[ms]{.carg .in type="int"}
: Number of milliseconds to sleep.

[long]{.carg .us type="long"}
: Number of micro-seconds to sleep.


:::

# Description

Those procedures delays the calling process by the given time and return after the time has elapsed. The two variants of the command only differ in thue time unit. **Tcl\_Sleep** uses milli-seconds, while, **Tcl\_SleepMicroSeconds** uses micro-seconds. They are typically used for things like flashing a button, where the delay is short and the application need not do anything while it waits.  For longer delays where the application needs to respond to other events during the delay, the procedure [Tcl\_CreateTimerHandler][CrtTimerHdlr] should be used instead of those commands.


[CrtTimerHdlr]: CrtTimerHdlr.md

