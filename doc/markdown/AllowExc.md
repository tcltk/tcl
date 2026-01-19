---
CommandName: Tcl_AllowExceptions
ManualSection: 3
Version: 7.4
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - continue
 - break
 - exception
 - interpreter
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_AllowExceptions - allow all exceptions in next script evaluation

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_AllowExceptions]{.ccmd}[interp]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter in which script will be evaluated. 

# Description

If a script is evaluated at top-level (i.e. no other scripts are pending evaluation when the script is invoked), and if the script terminates with a completion code other than **TCL\_OK**, **TCL\_ERROR** or **TCL\_RETURN**, then Tcl normally converts this into a **TCL\_ERROR** return with an appropriate message.  The particular script evaluation procedures of Tcl that act in the manner are **Tcl\_EvalObjEx**, **Tcl\_EvalObjv**, **Tcl\_Eval**, **Tcl\_EvalEx**, **Tcl\_GlobalEval**, **Tcl\_GlobalEvalObj** and **Tcl\_VarEval**.

However, if **Tcl\_AllowExceptions** is invoked immediately before calling one of those a procedures, then arbitrary completion codes are permitted from the script, and they are returned without modification. This is useful in cases where the caller can deal with exceptions such as **TCL\_BREAK** or **TCL\_CONTINUE** in a meaningful way. 

