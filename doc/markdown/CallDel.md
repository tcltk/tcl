---
CommandName: Tcl_CallWhenDeleted
ManualSection: 3
Version: 7.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_CreateExitHandler(3)
 - Tcl_CreateThreadExitHandler(3)
Keywords:
 - callback
 - cleanup
 - delete
 - interpreter
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_CallWhenDeleted, Tcl_DontCallWhenDeleted - Arrange for callback when interpreter is deleted

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_CallWhenDeleted]{.ccmd}[interp=, +proc=, +clientData]{.cargs}
[Tcl_DontCallWhenDeleted]{.ccmd}[interp=, +proc=, +clientData]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter with which to associated callback. .AP Tcl_InterpDeleteProc *proc in Procedure to call when *interp* is deleted. .AP void *clientData in Arbitrary one-word value to pass to *proc*.

# Description

**Tcl_CallWhenDeleted** arranges for *proc* to be called by **Tcl_DeleteInterp** if/when *interp* is deleted at some future time.  *Proc* will be invoked just before the interpreter is deleted, but the interpreter will still be valid at the time of the call. *Proc* should have arguments and result that match the type **Tcl_InterpDeleteProc**:

```
typedef void Tcl_InterpDeleteProc(
        void *clientData,
        Tcl_Interp *interp);
```

The *clientData* and *interp* parameters are copies of the *clientData* and *interp* arguments given to **Tcl_CallWhenDeleted**. Typically, *clientData* points to an application-specific data structure that *proc* uses to perform cleanup when an interpreter is about to go away. *Proc* does not return a value.

**Tcl_DontCallWhenDeleted** cancels a previous call to **Tcl_CallWhenDeleted** with the same arguments, so that *proc* will not be called after all when *interp* is deleted. If there is no deletion callback that matches *interp*, *proc*, and *clientData* then the call to **Tcl_DontCallWhenDeleted** has no effect.

Note that if the callback is being used to delete a resource that *must* be released on exit, **Tcl_CreateExitHandler** should be used to ensure that a callback is received even if the application terminates without deleting the interpreter.

