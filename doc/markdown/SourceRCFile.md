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

Tcl_SourceRCFile - source the Tcl rc file

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_SourceRCFile]{.ccmd}[interp]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Tcl interpreter to source rc file into. 

# Description

**Tcl_SourceRCFile** is used to source the Tcl rc file at startup. It is typically invoked by Tcl_Main or Tk_Main.  The name of the file sourced is obtained from the global variable **tcl_rcFileName** in the interpreter given by *interp*.  If this variable is not defined, or if the file it indicates cannot be found, no action is taken. 

