---
CommandName: Tcl_RecordAndEval
ManualSection: 3
Version: 7.4
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_RecordAndEvalObj
Keywords:
 - command
 - event
 - execute
 - history
 - interpreter
 - record
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

Tcl_RecordAndEval - save command on history list before evaluating

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_RecordAndEval]{.ccmd}[interp, cmd, flags]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Tcl interpreter in which to evaluate command. .AP "const char" *cmd in Command (or sequence of commands) to execute. .AP int flags in An OR'ed combination of flag bits.  **TCL_NO_EVAL** means record the command but do not evaluate it.  **TCL_EVAL_GLOBAL** means evaluate the command at global level instead of the current stack level. 

# Description

**Tcl_RecordAndEval** is invoked to record a command as an event on the history list and then execute it using **Tcl_Eval** (or **Tcl_GlobalEval** if the **TCL_EVAL_GLOBAL** bit is set in *flags*). It returns a completion code such as **TCL_OK** just like **Tcl_Eval** and it leaves information in the interpreter's result. If you do not want the command recorded on the history list then you should invoke **Tcl_Eval** instead of **Tcl_RecordAndEval**. Normally **Tcl_RecordAndEval** is only called with top-level commands typed by the user, since the purpose of history is to allow the user to re-issue recently-invoked commands. If the *flags* argument contains the **TCL_NO_EVAL** bit then the command is recorded without being evaluated.

Note that **Tcl_RecordAndEval** has been largely replaced by the value-based procedure **Tcl_RecordAndEvalObj**. That value-based procedure records and optionally executes a command held in a Tcl value instead of a string. 

