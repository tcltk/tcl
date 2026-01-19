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

Tcl\_CreateInterp, Tcl\_DeleteInterp, Tcl\_InterpActive, Tcl\_InterpDeleted - create and delete Tcl command interpreters

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Interp \*]{.ret} [Tcl\_CreateInterp]{.ccmd}[]{.cargs}
[Tcl\_DeleteInterp]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl\_InterpDeleted]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl\_InterpActive]{.ccmd}[interp]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Token for interpreter to be destroyed or queried.

# Description

**Tcl\_CreateInterp** creates a new interpreter structure and returns a token for it. The token is required in calls to most other Tcl procedures, such as **Tcl\_CreateCommand**, **Tcl\_Eval**, and **Tcl\_DeleteInterp**.  The token returned by **Tcl\_CreateInterp** may only be passed to Tcl routines called from the same thread as the original **Tcl\_CreateInterp** call.  It is not safe for multiple threads to pass the same token to Tcl's routines. The new interpreter is initialized with the built-in Tcl commands and with standard variables like **tcl\_platform** and **env**. To bind in additional commands, call **Tcl\_CreateCommand**, and to create additional variables, call **Tcl\_SetVar**.

**Tcl\_DeleteInterp** marks an interpreter as deleted; the interpreter will eventually be deleted when all calls to **Tcl\_Preserve** for it have been matched by calls to **Tcl\_Release**. At that time, all of the resources associated with it, including variables, procedures, and application-specific command bindings, will be deleted. After **Tcl\_DeleteInterp** returns any attempt to use **Tcl\_Eval** on the interpreter will fail and return **TCL\_ERROR**. After the call to **Tcl\_DeleteInterp** it is safe to examine the interpreter's result, query or set the values of variables, define, undefine or retrieve procedures, and examine the runtime evaluation stack. See below, in the section **INTERPRETERS AND MEMORY MANAGEMENT** for details.

**Tcl\_InterpDeleted** returns nonzero if **Tcl\_DeleteInterp** was called with *interp* as its argument; this indicates that the interpreter will eventually be deleted, when the last call to **Tcl\_Preserve** for it is matched by a call to **Tcl\_Release**. If nonzero is returned, further calls to **Tcl\_Eval** in this interpreter will return **TCL\_ERROR**.

**Tcl\_InterpDeleted** is useful in deletion callbacks to distinguish between when only the memory the callback is responsible for is being deleted and when the whole interpreter is being deleted. In the former case the callback may recreate the data being deleted, but this would lead to an infinite loop if the interpreter were being deleted.

**Tcl\_InterpActive** is useful for determining whether there is any execution of scripts ongoing in an interpreter, which is a useful piece of information when Tcl is embedded in a garbage-collected environment and it becomes necessary to determine whether the interpreter is a candidate for deletion. The function returns a true value if the interpreter has at least one active execution running inside it, and a false value otherwise.

# Interpreters and memory management

**Tcl\_DeleteInterp** can be called at any time on an interpreter that may be used by nested evaluations and C code in various extensions. Tcl implements a simple mechanism that allows callers to use interpreters without worrying about the interpreter being deleted in a nested call, and without requiring special code to protect the interpreter, in most cases. This mechanism ensures that nested uses of an interpreter can safely continue using it even after **Tcl\_DeleteInterp** is called.

The mechanism relies on matching up calls to **Tcl\_Preserve** with calls to **Tcl\_Release**. If **Tcl\_DeleteInterp** has been called, only when the last call to **Tcl\_Preserve** is matched by a call to **Tcl\_Release**, will the interpreter be freed. See the manual entry for **Tcl\_Preserve** for a description of these functions.

The rules for when the user of an interpreter must call **Tcl\_Preserve** and **Tcl\_Release** are simple:

**Interpreters Passed As Arguments**
: Functions that are passed an interpreter as an argument can safely use the interpreter without any special protection. Thus, when you write an extension consisting of new Tcl commands, no special code is needed to protect interpreters received as arguments. This covers the majority of all uses.

**Interpreter Creation And Deletion**
: When a new interpreter is created and used in a call to **Tcl\_Eval**, **Tcl\_VarEval**, **Tcl\_GlobalEval**, **Tcl\_SetVar**, or **Tcl\_GetVar**, a pair of calls to **Tcl\_Preserve** and **Tcl\_Release** should be wrapped around all uses of the interpreter. Remember that it is unsafe to use the interpreter once **Tcl\_Release** has been called. To ensure that the interpreter is properly deleted when it is no longer needed, call **Tcl\_InterpDeleted** to test if some other code already called **Tcl\_DeleteInterp**; if not, call **Tcl\_DeleteInterp** before calling **Tcl\_Release** in your own code.

**Retrieving An Interpreter From A Data Structure**
: When an interpreter is retrieved from a data structure (e.g. the client data of a callback) for use in one of the evaluation functions (**Tcl\_Eval**, **Tcl\_VarEval**, **Tcl\_GlobalEval**, **Tcl\_EvalObjv**, etc.) or variable access functions (**Tcl\_SetVar**, **Tcl\_GetVar**, **Tcl\_SetVar2Ex**, etc.), a pair of calls to **Tcl\_Preserve** and **Tcl\_Release** should be wrapped around all uses of the interpreter; it is unsafe to reuse the interpreter once **Tcl\_Release** has been called. If an interpreter is stored inside a callback data structure, an appropriate deletion cleanup mechanism should be set up by the code that creates the data structure so that the interpreter is removed from the data structure (e.g. by setting the field to NULL) when the interpreter is deleted. Otherwise, you may be using an interpreter that has been freed and whose memory may already have been reused.


All uses of interpreters in Tcl and Tk have already been protected. Extension writers should ensure that their code also properly protects any additional interpreters used, as described above.

Note that the protection mechanisms do not work well with conventional garbage collection systems. When in such a managed environment, **Tcl\_InterpActive** should be used to determine when an interpreter is a candidate for deletion due to inactivity.

