---
CommandName: Tcl_SetRecursionLimit
ManualSection: 3
Version: 7.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - nesting depth
 - recursion
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_SetRecursionLimit - set maximum allowable nesting depth in interpreter

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Size]{.ret} [Tcl_SetRecursionLimit]{.ccmd}[interp, depth]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter whose recursion limit is to be set. Must be greater than zero. .AP Tcl_Size depth in New limit for nested calls to **Tcl_Eval** for *interp*. 

# Description

At any given time Tcl enforces a limit on the number of recursive calls that may be active for **Tcl_Eval** and related procedures such as **Tcl_EvalEx**. Any call to **Tcl_Eval** that exceeds this depth is aborted with an error. By default the recursion limit is 1000.

**Tcl_SetRecursionLimit** may be used to change the maximum allowable nesting depth for an interpreter. The *depth* argument specifies a new limit for *interp*, and **Tcl_SetRecursionLimit** returns the old limit. To read out the old limit without modifying it, invoke **Tcl_SetRecursionLimit** with *depth* equal to 0.

The **Tcl_SetRecursionLimit** only sets the size of the Tcl call stack:  it cannot by itself prevent stack overflows on the C stack being used by the application.  If your machine has a limit on the size of the C stack, you may get stack overflows before reaching the limit set by **Tcl_SetRecursionLimit**. If this happens, see if there is a mechanism in your system for increasing the maximum size of the C stack. 

