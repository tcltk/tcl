---
CommandName: Tcl_GetHostName
ManualSection: 3
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - hostname
Copyright:
 - Copyright (c) 1998-2000 Scriptics Corporation.
---

# Name

Tcl_GetHostName - get the name of the local host

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[const char *]{.ret} [Tcl_GetHostName]{.ccmd}[]{.cargs}
:::

# Description

**Tcl_GetHostName** is a utility procedure used by some of the Tcl commands.  It returns a pointer to a string containing the name for the current machine, or an empty string if the name cannot be determined.  The string is statically allocated, and the caller must not modify of free it.

