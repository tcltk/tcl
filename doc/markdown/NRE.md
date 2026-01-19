---
CommandName: NRE
ManualSection: 3
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_CreateCommand(3)
 - Tcl_CreateObjCommand(3)
 - Tcl_EvalObjEx(3)
 - Tcl_GetCommandFromObj(3)
 - Tcl_ExprObj(3)
Keywords:
 - stackless
 - nonrecursive
 - execute
 - command
 - global
 - value
 - result
 - script
Copyright:
 - Copyright (c) 2008 Kevin B. Kenny.
 - Copyright (c) 2018 Nathan Coulter.
---

# Name

Tcl\_NRCreateCommand, Tcl\_NRCreateCommand2, Tcl\_NRCallObjProc, Tcl\_NRCallObjProc2, Tcl\_NREvalObj, Tcl\_NREvalObjv, Tcl\_NRCmdSwap, Tcl\_NRExprObj, Tcl\_NRAddCallback - Non-Recursive (stackless) evaluation of Tcl scripts.

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Command]{.ret} [Tcl\_NRCreateCommand]{.ccmd}[interp, cmdName, proc, nreProc, clientData, deleteProc]{.cargs}
[Tcl\_Command]{.ret} [Tcl\_NRCreateCommand2]{.ccmd}[interp, cmdName, proc2, nreProc2, clientData, deleteProc]{.cargs}
[int]{.ret} [Tcl\_NRCallObjProc]{.ccmd}[interp, nreProc, clientData, objc, objv]{.cargs}
[int]{.ret} [Tcl\_NRCallObjProc2]{.ccmd}[interp, nreProc2, clientData, objc, objv]{.cargs}
[int]{.ret} [Tcl\_NREvalObj]{.ccmd}[interp, objPtr, flags]{.cargs}
[int]{.ret} [Tcl\_NREvalObjv]{.ccmd}[interp, objc, objv, flags]{.cargs}
[int]{.ret} [Tcl\_NRCmdSwap]{.ccmd}[interp, cmd, objc, objv, flags]{.cargs}
[int]{.ret} [Tcl\_NRExprObj]{.ccmd}[interp, objPtr, resultPtr]{.cargs}
[Tcl\_NRAddCallback]{.ccmd}[interp, postProcPtr, data0, data1, data2, data3]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in The relevant Interpreter. .AP "const char" \*cmdName in Name of the command to create. .AP Tcl\_ObjCmdProc \*proc in Called in order to evaluate a command.  Is often just a small wrapper that uses **Tcl\_NRCallObjProc** to call *nreProc* using a new trampoline.  Behaves in the same way as the *proc* argument to **Tcl\_CreateObjCommand**(3) (*q.v.*). .AP Tcl\_ObjCmdProc2 \*proc2 in Called in order to evaluate a command.  Is often just a small wrapper that uses **Tcl\_NRCallObjProc2** to call *nreProc2* using a new trampoline.  Behaves in the same way as the *proc2* argument to **Tcl\_CreateObjCommand2**(3) (*q.v.*). .AP Tcl\_ObjCmdProc \*nreProc in Called instead of *proc* when a trampoline is already in use. .AP Tcl\_ObjCmdProc2 \*nreProc2 in Called instead of *proc2* when a trampoline is already in use. .AP void \*clientData in Arbitrary one-word value passed to *proc*, *nreProc*, *deleteProc* and *objProc*. .AP Tcl\_CmdDeleteProc \*deleteProc in/out Called before *cmdName* is deleted from the interpreter, allowing for command-specific cleanup. May be NULL. .AP Tcl\_Size objc in Number of items in *objv*. .AP Tcl\_Obj \*\*objv in Words in the command. .AP Tcl\_Obj \*objPtr in A script or expression to evaluate. .AP int flags in As described for *Tcl\_EvalObjv*.

.AP Tcl\_Command cmd in Token to use instead of one derived from the first word of *objv* in order to evaluate a command. .AP Tcl\_Obj \*resultPtr out Pointer to an unshared Tcl\_Obj where the result of the evaluation is stored if the return code is TCL\_OK. .AP Tcl\_NRPostProc \*postProcPtr in A function to push. .AP void \*data0 in .AP void \*data1 in .AP void \*data2 in .AP void \*data3 in *data0* through *data3* are four one-word values that will be passed to the function designated by *postProcPtr* when it is invoked.

# Description

These functions provide an interface to the function stack that an interpreter iterates through to evaluate commands.  The routine behind a command is implemented by an initial function and any additional functions that the routine pushes onto the stack as it progresses.  The interpreter itself pushes functions onto the stack to react to the end of a routine and to exercise other forms of control such as switching between in-progress stacks and the evaluation of other scripts at additional levels without adding frames to the C stack.  To execute a routine, the initial function for the routine is called and then a small bit of code called a *trampoline* iteratively takes functions off the stack and calls them, using the value of the last call as the value of the routine.

**Tcl\_NRCallObjProc** calls *nreProc* using a new trampoline.

**Tcl\_NRCreateCommand**, an alternative to **Tcl\_CreateObjCommand**, resolves *cmdName*, which may contain namespace qualifiers, relative to the current namespace, creates a command by that name, and returns a token for the command which may be used in subsequent calls to **Tcl\_GetCommandName**. Except for a few cases noted below any existing command by the same name is first deleted.  If *interp* is in the process of being deleted **Tcl\_NRCreateCommand** does not create any command, does not delete any command, and returns NULL.

**Tcl\_NRCreateCommand2**, is an alternative to **Tcl\_NRCreateCommand** in the same way as **Tcl\_CreateObjCommand2**.

**Tcl\_NREvalObj** pushes a function that is like **Tcl\_EvalObjEx** but consumes no space on the C stack.

**Tcl\_NREvalObjv** pushes a function that is like **Tcl\_EvalObjv** but consumes no space on the C stack.

**Tcl\_NRCmdSwap** is like **Tcl\_NREvalObjv**, but uses *cmd*, a token previously returned by **Tcl\_CreateObjCommand** or **Tcl\_GetCommandFromObj**, instead of resolving the first word of *objv*. .  The name of this command must be the same as *objv[0]*.

**Tcl\_NRExprObj** pushes a function that evaluates *objPtr* as an expression in the same manner as **Tcl\_ExprObj** but without consuming space on the C stack.

All of the functions return **TCL\_OK** if the evaluation of the script, command, or expression has been scheduled successfully.  Otherwise (for example if the command name cannot be resolved), they return **TCL\_ERROR** and store a message as the interpreter's result.

**Tcl\_NRAddCallback** pushes *postProcPtr*.  The signature for **Tcl\_NRPostProc** is:

```
typedef int
Tcl_NRPostProc(
        void * data[],
        Tcl_Interp *interp,
        int result);
```

*data* is a pointer to an array containing *data0* through *data3*. *result* is the value returned by the previous function implementing part the routine.

# Example

The following command uses **Tcl\_EvalObjEx**, which consumes space on the C stack, to evaluate a script:

```
int
TheCmdOldObjProc(
    void *clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int result;
    Tcl_Obj *objPtr;

    ... preparation ...

    result = Tcl_EvalObjEx(interp, objPtr, 0);

    ... postprocessing ...

    return result;
}
Tcl_CreateObjCommand(interp, "theCommand",
        TheCmdOldObjProc, clientData, TheCmdDeleteProc);
```

To avoid consuming space on the C stack, *TheCmdOldObjProc* is renamed to *TheCmdNRObjProc* and the postprocessing step is split into a separate function, *TheCmdPostProc*, which is pushed onto the function stack. *Tcl\_EvalObjEx* is replaced with *Tcl\_NREvalObj*, which uses a trampoline instead of consuming space on the C stack.  A new version of *TheCmdOldObjProc* is just a a wrapper that uses **Tcl\_NRCallObjProc** to call *TheCmdNRObjProc*:

```
int
TheCmdOldObjProc(
    void *clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    return Tcl_NRCallObjProc(interp, TheCmdNRObjProc,
            clientData, objc, objv);
}
```

```
int
TheCmdNRObjProc
    void *clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *objPtr;

    ... preparation ...

    Tcl_NRAddCallback(interp, TheCmdPostProc,
            data0, data1, data2, data3);
    /* data0 .. data3 are up to four one-word items to
     * pass to the postprocessing procedure */

    return Tcl_NREvalObj(interp, objPtr, 0);
}
```

```
int
TheCmdNRPostProc(
    void *data[],
    Tcl_Interp *interp,
    int result)
{
    /* data[0] .. data[3] are the four words of data
     * passed to Tcl_NRAddCallback */

    ... postprocessing ...

    return result;
}
```

Any function comprising a routine can push other functions, making it possible implement looping and sequencing constructs using the function stack.

# Reference count management

The first *objc* values in the *objv* array passed to the functions **Tcl\_NRCallObjProc**, **Tcl\_NREvalObjv**, and **Tcl\_NRCmdSwap** should have a reference count of at least 1; they may have additional references taken during the execution.

The *objPtr* argument to **Tcl\_NREvalObj** and **Tcl\_NRExprObj** should have a reference count of at least 1, and may have additional references taken to it during execution.

The *resultObj* argument to **Tcl\_NRExprObj** should be an unshared object.

Use **Tcl\_NRAddCallback** to schedule any required final decrementing of the reference counts of arguments to any of the other functions on this page, as with any other post-processing step in the non-recursive execution engine.

The

# Copyright

Copyright \\(co 2008 Kevin B. Kenny. Copyright \\(co 2018 Nathan Coulter. 

