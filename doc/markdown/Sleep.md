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

Tcl_Sleep - delay execution for a given number of milliseconds

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Sleep]{.ccmd}[ms]{.cargs}
:::

# Arguments

.AP int ms in Number of milliseconds to sleep.

# Description

This procedure delays the calling process by the number of milliseconds given by the *ms* parameter and returns after that time has elapsed.  It is typically used for things like flashing a button, where the delay is short and the application need not do anything while it waits.  For longer delays where the application needs to respond to other events during the delay, the procedure **Tcl_CreateTimerHandler** should be used instead of **Tcl_Sleep**.

