---
CommandName: Tcl_CreateInterp
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_Preserve(3)
 - Tcl_Release(3)
Keywords:
 - command
 - create
 - delete
 - interpreter
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_CreateInterp, Tcl_DeleteInterp, Tcl_InterpActive, Tcl_InterpDeleted - create and delete Tcl command interpreters

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Interp *]{.ret} [Tcl_CreateInterp]{.ccmd}[]{.cargs}
[Tcl_DeleteInterp]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl_InterpDeleted]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl_InterpActive]{.ccmd}[interp]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Token for interpreter to be destroyed or queried.

# Description

**Tcl_CreateInterp** creates a new interpreter structure and returns a token for it. The token is required in calls to most other Tcl procedures, such as **Tcl_CreateCommand**, **Tcl_Eval**, and **Tcl_DeleteInterp**.  The token returned by **Tcl_CreateInterp** may only be passed to Tcl routines called from the same thread as the original **Tcl_CreateInterp** call.  It is not safe for multiple threads to pass the same token to Tcl's routines. The new interpreter is initialized with the built-in Tcl commands and with standard variables like **tcl_platform** and **env**. To bind in additional commands, call **Tcl_CreateCommand**, and to create additional variables, call **Tcl_SetVar**.

**Tcl_DeleteInterp** marks an interpreter as deleted; the interpreter will eventually be deleted when all calls to **Tcl_Preserve** for it have been matched by calls to **Tcl_Release**. At that time, all of the resources associated with it, including variables, procedures, and application-specific command bindings, will be deleted. After **Tcl_DeleteInterp** returns any attempt to use **Tcl_Eval** on the interpreter will fail and return **TCL_ERROR**. After the call to **Tcl_DeleteInterp** it is safe to examine the interpreter's result, query or set the values of variables, define, undefine or retrieve procedures, and examine the runtime evaluation stack. See below, in the section **INTERPRETERS AND MEMORY MANAGEMENT** for details.

**Tcl_InterpDeleted** returns nonzero if **Tcl_DeleteInterp** was called with *interp* as its argument; this indicates that the interpreter will eventually be deleted, when the last call to **Tcl_Preserve** for it is matched by a call to **Tcl_Release**. If nonzero is returned, further calls to **Tcl_Eval** in this interpreter will return **TCL_ERROR**.

**Tcl_InterpDeleted** is useful in deletion callbacks to distinguish between when only the memory the callback is responsible for is being deleted and when the whole interpreter is being deleted. In the former case the callback may recreate the data being deleted, but this would lead to an infinite loop if the interpreter were being deleted.

**Tcl_InterpActive** is useful for determining whether there is any execution of scripts ongoing in an interpreter, which is a useful piece of information when Tcl is embedded in a garbage-collected environment and it becomes necessary to determine whether the interpreter is a candidate for deletion. The function returns a true value if the interpreter has at least one active execution running inside it, and a false value otherwise.

# Interpreters and memory management

**Tcl_DeleteInterp** can be called at any time on an interpreter that may be used by nested evaluations and C code in various extensions. Tcl implements a simple mechanism that allows callers to use interpreters without worrying about the interpreter being deleted in a nested call, and without requiring special code to protect the interpreter, in most cases. This mechanism ensures that nested uses of an interpreter can safely continue using it even after **Tcl_DeleteInterp** is called.

The mechanism relies on matching up calls to **Tcl_Preserve** with calls to **Tcl_Release**. If **Tcl_DeleteInterp** has been called, only when the last call to **Tcl_Preserve** is matched by a call to **Tcl_Release**, will the interpreter be freed. See the manual entry for **Tcl_Preserve** for a description of these functions.

The rules for when the user of an interpreter must call **Tcl_Preserve** and **Tcl_Release** are simple:

**Interpreters Passed As Arguments**
: Functions that are passed an interpreter as an argument can safely use the interpreter without any special protection. Thus, when you write an extension consisting of new Tcl commands, no special code is needed to protect interpreters received as arguments. This covers the majority of all uses.

**Interpreter Creation And Deletion**
: When a new interpreter is created and used in a call to **Tcl_Eval**, **Tcl_VarEval**, **Tcl_GlobalEval**, **Tcl_SetVar**, or **Tcl_GetVar**, a pair of calls to **Tcl_Preserve** and **Tcl_Release** should be wrapped around all uses of the interpreter. Remember that it is unsafe to use the interpreter once **Tcl_Release** has been called. To ensure that the interpreter is properly deleted when it is no longer needed, call **Tcl_InterpDeleted** to test if some other code already called **Tcl_DeleteInterp**; if not, call **Tcl_DeleteInterp** before calling **Tcl_Release** in your own code.

**Retrieving An Interpreter From A Data Structure**
: When an interpreter is retrieved from a data structure (e.g. the client data of a callback) for use in one of the evaluation functions (**Tcl_Eval**, **Tcl_VarEval**, **Tcl_GlobalEval**, **Tcl_EvalObjv**, etc.) or variable access functions (**Tcl_SetVar**, **Tcl_GetVar**, **Tcl_SetVar2Ex**, etc.), a pair of calls to **Tcl_Preserve** and **Tcl_Release** should be wrapped around all uses of the interpreter; it is unsafe to reuse the interpreter once **Tcl_Release** has been called. If an interpreter is stored inside a callback data structure, an appropriate deletion cleanup mechanism should be set up by the code that creates the data structure so that the interpreter is removed from the data structure (e.g. by setting the field to NULL) when the interpreter is deleted. Otherwise, you may be using an interpreter that has been freed and whose memory may already have been reused.


All uses of interpreters in Tcl and Tk have already been protected. Extension writers should ensure that their code also properly protects any additional interpreters used, as described above.

Note that the protection mechanisms do not work well with conventional garbage collection systems. When in such a managed environment, **Tcl_InterpActive** should be used to determine when an interpreter is a candidate for deletion due to inactivity.

