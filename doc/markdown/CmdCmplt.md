---
CommandName: Tcl_CommandComplete
ManualSection: 3
Version: unknown
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - complete command
 - partial command
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_CommandComplete - Check for unmatched braces in a Tcl command

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_CommandComplete]{.ccmd}[cmd]{.cargs}
:::

# Arguments

.AP "const char" \*cmd in Command string to test for completeness. 

# Description

**Tcl\_CommandComplete** takes a Tcl command string as argument and determines whether it contains one or more complete commands (i.e. there are no unclosed quotes, braces, brackets, or variable references). If the command string is complete then it returns 1; otherwise it returns 0. 

