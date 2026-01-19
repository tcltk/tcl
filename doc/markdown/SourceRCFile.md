---
CommandName: Tcl_SourceRCFile
ManualSection: 3
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - application-specific initialization
 - main program
 - rc file
Copyright:
 - Copyright (c) 1998-2000 Scriptics Corporation.
---

# Name

Tcl\_SourceRCFile - source the Tcl rc file

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_SourceRCFile]{.ccmd}[interp]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Tcl interpreter to source rc file into. 

# Description

**Tcl\_SourceRCFile** is used to source the Tcl rc file at startup. It is typically invoked by Tcl\_Main or Tk\_Main.  The name of the file sourced is obtained from the global variable **tcl\_rcFileName** in the interpreter given by *interp*.  If this variable is not defined, or if the file it indicates cannot be found, no action is taken. 

