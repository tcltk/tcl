---
CommandName: Tcl_SubstObj
ManualSection: 3
Version: 8.4
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - subst(n)
Keywords:
 - backslash substitution
 - command substitution
 - variable substitution
Copyright:
 - Copyright (c) 2001 Donal K. Fellows
---

# Name

Tcl_SubstObj - perform substitutions on Tcl values

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Obj *]{.ret} [Tcl_SubstObj]{.ccmd}[interp, objPtr, flags]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter in which to execute Tcl scripts and lookup variables.  If an error occurs, the interpreter's result is modified to hold an error message. .AP Tcl_Obj *objPtr in A Tcl value containing the string to perform substitutions on. .AP int flags in OR'ed combination of flag bits that specify which substitutions to perform.  The flags **TCL_SUBST_COMMANDS**, **TCL_SUBST_VARIABLES** and **TCL_SUBST_BACKSLASHES** are currently supported, and **TCL_SUBST_ALL** is provided as a convenience for the common case where all substitutions are desired.

# Description

The **Tcl_SubstObj** function is used to perform substitutions on strings in the fashion of the **subst** command.  It gets the value of the string contained in *objPtr* and scans it, copying characters and performing the chosen substitutions as it goes to an output value which is returned as the result of the function.  In the event of an error occurring during the execution of a command or variable substitution, the function returns NULL and an error message is left in *interp*'s result.

Three kinds of substitutions are supported.  When the **TCL_SUBST_BACKSLASHES** bit is set in *flags*, sequences that look like backslash substitutions for Tcl commands are replaced by their corresponding character.

When the **TCL_SUBST_VARIABLES** bit is set in *flags*, sequences that look like variable substitutions for Tcl commands are replaced by the contents of the named variable.

When the **TCL_SUBST_COMMANDS** bit is set in *flags*, sequences that look like command substitutions for Tcl commands are replaced by the result of evaluating that script.  Where an uncaught "continue exception" occurs during the evaluation of a command substitution, an empty string is substituted for the command.  Where an uncaught "break exception" occurs during the evaluation of a command substitution, the result of the whole substitution on *objPtr* will be truncated at the point immediately before the start of the command substitution, and no characters will be added to the result or substitutions performed after that point.

# Reference count management

The *objPtr* argument to **Tcl_SubstObj** must not have a reference count of zero. This function modifies the interpreter result, both on success and on failure; the result of this function on success is exactly the current interpreter result. Successful results should have their reference count incremented if they are to be retained.

