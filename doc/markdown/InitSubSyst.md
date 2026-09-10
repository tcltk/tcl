---
CommandName: Tcl_InitSubsystems
ManualSection: 3
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - binary
 - executable file
Copyright:
 - Copyright (c) 2018 Tcl Core Team
---

# Name

Tcl\_InitSubsystems - initialize the Tcl library.

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[const char \*]{.ret} [Tcl\_InitSubsystems]{.ccmd}[]{.cargs}
:::

# Description

The **Tcl\_InitSubsystems** procedure initializes various Tcl subsystems. It should be called by any additional Tcl threads created after the main thread in a Tcl application that wish to use Tcl facilities without creating an interpreter. Threads that create interpreters using [Tcl\_CreateInterp][CrtInterp] do not need to call this function, provided that [Tcl\_CreateInterp][CrtInterp] is the first call into Tcl from that thread.

Note that the main thread must have first called either [TclZipfs\_AppHook][zipfs] or [Tcl\_FindExecutable][FindExec] to perform application-wide Tcl initialization before additional threads using Tcl are created. **Tcl\_InitSubsystems** does not suffice for this purpose and should only be used to initialize Tcl in secondary threads. It need not be called in the main thread invoking one of the two aforementioned functions.

The result of **Tcl\_InitSubsystems** is the full Tcl version string, including build information (for example, **9.0.0+abcdef...abcdef.gcc-1002**).


[CrtInterp]: CrtInterp.md
[FindExec]: FindExec.md
[zipfs]: zipfs.md

