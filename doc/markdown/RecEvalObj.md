---
CommandName: Tcl_RecordAndEvalObj
ManualSection: 3
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_EvalObjEx
 - Tcl_GetObjResult
Keywords:
 - command
 - event
 - execute
 - history
 - interpreter
 - value
 - record
Copyright:
 - Copyright (c) 1997 Sun Microsystems, Inc.
---

# Name

Tcl\_RecordAndEvalObj - save command on history list before evaluating

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_RecordAndEvalObj]{.ccmd}[interp, cmdPtr, flags]{.cargs}
:::

# Arguments

::: {.arguments} :::

[\*interp]{.carg .in type="Tcl_Interp"}
: Tcl interpreter in which to evaluate command.

[\*cmdPtr]{.carg .in type="Tcl_Obj"}
: Points to a Tcl value containing a command (or sequence of commands) to execute.

[flags]{.carg .in type="int"}
: An OR'ed combination of flag bits.  **TCL\_NO\_EVAL** means record the command but do not evaluate it.  **TCL\_EVAL\_GLOBAL** means evaluate the command at global level instead of the current stack level.


:::

# Description

**Tcl\_RecordAndEvalObj** is invoked to record a command as an event on the history list and then execute it using [Tcl\_EvalObjEx][Eval3]. It returns a completion code such as [TCL\_OK][catch] just like [Tcl\_EvalObjEx][Eval3], as well as a result value containing additional information (a result value or error message) that can be retrieved using [Tcl\_GetObjResult][SetResult]. If you do not want the command recorded on the history list then you should invoke [Tcl\_EvalObjEx][Eval3] instead of **Tcl\_RecordAndEvalObj**. Normally **Tcl\_RecordAndEvalObj** is only called with top-level commands typed by the user, since the purpose of history is to allow the user to re-issue recently invoked commands. If the *flags* argument contains the **TCL\_NO\_EVAL** bit then the command is recorded without being evaluated. 

# Reference count management

The reference count of the *cmdPtr* argument to **Tcl\_RecordAndEvalObj** must be at least 1. This function will modify the interpreter result; do not use an existing result as *cmdPtr* directly without incrementing its reference count.


[catch]: catch.md
[Eval3]: Eval3.md
[SetResult]: SetResult.md

