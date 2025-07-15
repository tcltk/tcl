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

Tcl_AllowExceptions - allow all exceptions in next script evaluation

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_AllowExceptions]{.cmd} [interp]{.arg}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter in which script will be evaluated. 

# Description

If a script is evaluated at top-level (i.e. no other scripts are pending evaluation when the script is invoked), and if the script terminates with a completion code other than **TCL_OK**, **TCL_ERROR** or **TCL_RETURN**, then Tcl normally converts this into a **TCL_ERROR** return with an appropriate message.  The particular script evaluation procedures of Tcl that act in the manner are **Tcl_EvalObjEx**, **Tcl_EvalObjv**, **Tcl_Eval**, **Tcl_EvalEx**, **Tcl_GlobalEval**, **Tcl_GlobalEvalObj** and **Tcl_VarEval**.

However, if **Tcl_AllowExceptions** is invoked immediately before calling one of those a procedures, then arbitrary completion codes are permitted from the script, and they are returned without modification. This is useful in cases where the caller can deal with exceptions such as **TCL_BREAK** or **TCL_CONTINUE** in a meaningful way. 

