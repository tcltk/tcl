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

The **Tcl\_InitSubsystems** procedure initializes the Tcl library. This procedure is typically invoked as the very first thing in the application's main program.

The result of **Tcl\_InitSubsystems** is the full Tcl version with build information (e.g., **9.0.0+abcdef...abcdef.gcc-1002**).

**Tcl\_InitSubsystems** is very similar in use to **Tcl\_FindExecutable**. It can be used when Tcl is used as utility library, no other encodings than utf-8, iso8859-1 or utf-16 are used, and no interest exists in the value of [info nameofexecutable][info]. The system encoding will not be extracted from the environment, but falls back to utf-8.


[info]: info.md

