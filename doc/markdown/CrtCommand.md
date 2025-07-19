---
CommandName: Tcl_CreateCommand
ManualSection: 3
Version: unknown
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_CreateObjCommand
 - Tcl_DeleteCommand
 - Tcl_GetCommandInfo
 - Tcl_SetCommandInfo
 - Tcl_GetCommandName
 - Tcl_SetObjResult
Keywords:
 - bind
 - command
 - create
 - delete
 - interpreter
 - namespace
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

Tcl_CreateCommand - implement new commands in C

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Command]{.ret} [Tcl_CreateCommand]{.ccmd}[interp, cmdName, proc, clientData, deleteProc]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter in which to create new command. .AP "const char" *cmdName in Name of command. .AP Tcl_CmdProc *proc in Implementation of new command:  *proc* will be called whenever *cmdName* is invoked as a command. .AP void *clientData in Arbitrary one-word value to pass to *proc* and *deleteProc*. .AP Tcl_CmdDeleteProc *deleteProc in Procedure to call before *cmdName* is deleted from the interpreter; allows for command-specific cleanup.  If NULL, then no procedure is called before the command is deleted.

# Description

**Tcl_CreateCommand** defines a new command in *interp* and associates it with procedure *proc* such that whenever *cmdName* is invoked as a Tcl command (via a call to **Tcl_Eval**) the Tcl interpreter will call *proc* to process the command. It differs from **Tcl_CreateObjCommand** in that a new string-based command is defined; that is, a command procedure is defined that takes an array of argument strings instead of values. The value-based command procedures registered by **Tcl_CreateObjCommand** can execute significantly faster than the string-based command procedures defined by **Tcl_CreateCommand**. This is because they take Tcl values as arguments and those values can retain an internal representation that can be manipulated more efficiently. Also, Tcl's interpreter now uses values internally. In order to invoke a string-based command procedure registered by **Tcl_CreateCommand**, it must generate and fetch a string representation from each argument value before the call. New commands should be defined using **Tcl_CreateObjCommand**. We support **Tcl_CreateCommand** for backwards compatibility.

The procedures **Tcl_DeleteCommand**, **Tcl_GetCommandInfo**, and **Tcl_SetCommandInfo** are used in conjunction with **Tcl_CreateCommand**.

**Tcl_CreateCommand** will delete an existing command *cmdName*, if one is already associated with the interpreter. It returns a token that may be used to refer to the command in subsequent calls to **Tcl_GetCommandName**. If *cmdName* contains any **::** namespace qualifiers, then the command is added to the specified namespace; otherwise the command is added to the global namespace. If **Tcl_CreateCommand** is called for an interpreter that is in the process of being deleted, then it does not create a new command and it returns NULL. *Proc* should have arguments and result that match the type **Tcl_CmdProc**:

```
typedef int Tcl_CmdProc(
        void *clientData,
        Tcl_Interp *interp,
        int argc,
        const char *argv[]);
```

When *proc* is invoked the *clientData* and *interp* parameters will be copies of the *clientData* and *interp* arguments given to **Tcl_CreateCommand**. Typically, *clientData* points to an application-specific data structure that describes what to do when the command procedure is invoked.  *Argc* and *argv* describe the arguments to the command, *argc* giving the number of arguments (including the command name) and *argv* giving the values of the arguments as strings.  The *argv* array will contain *argc*+1 values; the first *argc* values point to the argument strings, and the last value is NULL.

Note that the argument strings should not be modified as they may point to constant strings or may be shared with other parts of the interpreter. Note also that the argument strings are encoded in normalized UTF-8 since version 8.1 of Tcl.

*Proc* must return an integer code that is expected to be one of **TCL_OK**, **TCL_ERROR**, **TCL_RETURN**, **TCL_BREAK**, or **TCL_CONTINUE**. See the **return** man page for details on what these codes mean and the use of extended values for an extension's private use. Most normal commands will only return **TCL_OK** or **TCL_ERROR**.

In addition, *proc* must set the interpreter result; in the case of a **TCL_OK** return code this gives the result of the command, and in the case of **TCL_ERROR** it gives an error message. The **Tcl_SetResult** procedure provides an easy interface for setting the return value;  for complete details on how the interpreter result field is managed, see the **Tcl_Interp** man page. Before invoking a command procedure, **Tcl_Eval** sets the interpreter result to point to an empty string, so simple commands can return an empty result by doing nothing at all.

The contents of the *argv* array belong to Tcl and are not guaranteed to persist once *proc* returns:  *proc* should not modify them, nor should it set the interpreter result to point anywhere within the *argv* values. Call **Tcl_SetResult** with status **TCL_VOLATILE** if you want to return something from the *argv* array.

*DeleteProc* will be invoked when (if) *cmdName* is deleted. This can occur through a call to **Tcl_DeleteCommand** or **Tcl_DeleteInterp**, or by replacing *cmdName* in another call to **Tcl_CreateCommand**. *DeleteProc* is invoked before the command is deleted, and gives the application an opportunity to release any structures associated with the command.  *DeleteProc* should have arguments and result that match the type **Tcl_CmdDeleteProc**:

```
typedef void Tcl_CmdDeleteProc(
        void *clientData);
```

The *clientData* argument will be the same as the *clientData* argument passed to **Tcl_CreateCommand**.

