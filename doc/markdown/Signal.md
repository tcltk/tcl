---
CommandName: Tcl_SignalId
ManualSection: 3
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - signals
 - signal numbers
Copyright:
 - Copyright (c) 2001 ActiveState Tool Corp.
---

# Name

Tcl\_SignalId, Tcl\_SignalMsg - Convert signal codes

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[const char \*]{.ret} [Tcl\_SignalId]{.ccmd}[sig]{.cargs}
[const char \*]{.ret} [Tcl\_SignalMsg]{.ccmd}[sig]{.cargs}
:::

# Arguments

.AP int sig in A POSIX signal number such as **SIGPIPE**. 

# Description

**Tcl\_SignalId** and **Tcl\_SignalMsg** return a string representation of the provided signal number (*sig*). **Tcl\_SignalId** returns a machine-readable textual identifier such as "SIGPIPE". **Tcl\_SignalMsg** returns a human-readable string such as "bus error". The strings returned by these functions are statically allocated and the caller must not free or modify them. 

