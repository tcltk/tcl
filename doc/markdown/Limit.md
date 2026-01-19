---
CommandName: Tcl_LimitCheck
ManualSection: 3
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - interpreter
 - resource
 - limit
 - commands
 - time
 - callback
Copyright:
 - Copyright (c) 2004 Donal K. Fellows
---

# Name

Tcl\_LimitAddHandler, Tcl\_LimitCheck, Tcl\_LimitExceeded, Tcl\_LimitGetCommands, Tcl\_LimitGetGranularity, Tcl\_LimitGetTime, Tcl\_LimitReady, Tcl\_LimitRemoveHandler, Tcl\_LimitSetCommands, Tcl\_LimitSetGranularity, Tcl\_LimitSetTime, Tcl\_LimitTypeEnabled, Tcl\_LimitTypeExceeded, Tcl\_LimitTypeReset, Tcl\_LimitTypeSet - manage and check resource limits on interpreters

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_LimitCheck]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl\_LimitReady]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl\_LimitExceeded]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl\_LimitTypeExceeded]{.ccmd}[interp, type]{.cargs}
[int]{.ret} [Tcl\_LimitTypeEnabled]{.ccmd}[interp, type]{.cargs}
[Tcl\_LimitTypeSet]{.ccmd}[interp, type]{.cargs}
[Tcl\_LimitTypeReset]{.ccmd}[interp, type]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_LimitGetCommands]{.ccmd}[interp]{.cargs}
[Tcl\_LimitSetCommands]{.ccmd}[interp, commandLimit]{.cargs}
[Tcl\_LimitGetTime]{.ccmd}[interp, timeLimitPtr]{.cargs}
[Tcl\_LimitSetTime]{.ccmd}[interp, timeLimitPtr]{.cargs}
[int]{.ret} [Tcl\_LimitGetGranularity]{.ccmd}[interp, type]{.cargs}
[Tcl\_LimitSetGranularity]{.ccmd}[interp, type, granularity]{.cargs}
[Tcl\_LimitAddHandler]{.ccmd}[interp, type, handlerProc, clientData, deleteProc]{.cargs}
[Tcl\_LimitRemoveHandler]{.ccmd}[interp, type, handlerProc, clientData]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter that the limit being managed applies to or that will have its limits checked. .AP int type in The type of limit that the operation refers to.  This must be either **TCL\_LIMIT\_COMMANDS** or **TCL\_LIMIT\_TIME**. .AP Tcl\_Size commandLimit in The maximum number of commands (as reported by [info cmdcount][info]) that may be executed in the interpreter. .AP Tcl\_Time \*timeLimitPtr in/out A pointer to a structure that will either have the new time limit read from (**Tcl\_LimitSetTime**) or the current time limit written to (**Tcl\_LimitGetTime**). .AP int granularity in Divisor that indicates how often a particular limit should really be checked.  Must be at least 1. .AP Tcl\_LimitHandlerProc \*handlerProc in Function to call when a particular limit is exceeded.  If the *handlerProc* removes or raises the limit during its processing, the limited interpreter will be permitted to continue to process after the handler returns.  Many handlers may be attached to the same interpreter limit; their order of execution is not defined, and they must be identified by *handlerProc* and *clientData* when they are deleted. .AP void \*clientData in Arbitrary pointer-sized word used to pass some context to the *handlerProc* function. .AP Tcl\_LimitHandlerDeleteProc \*deleteProc in Function to call whenever a handler is deleted.  May be NULL if the *clientData* requires no deletion.

# Description

Tcl's interpreter resource limit subsystem allows for close control over how much computation time a script may use, and is useful for cases where a program is divided into multiple pieces where some parts are more trusted than others (e.g. web application servers).

Every interpreter may have a limit on the wall-time for execution, and a limit on the number of commands that the interpreter may execute. Since checking of these limits is potentially expensive (especially the time limit), each limit also has a checking granularity, which is a divisor for an internal count of the number of points in the core where a check may be performed (which is immediately before executing a command and at an unspecified frequency between running commands, which can happen in empty-bodied [while] loops).

The final component of the limit engine is a callback scheme which allows for notifications of when a limit has been exceeded.  These callbacks can just provide logging, or may allocate more resources to the interpreter to permit it to continue processing longer.

When a limit is exceeded (and the callbacks have run; the order of execution of the callbacks is unspecified) execution in the limited interpreter is stopped by raising an error and setting a flag that prevents the [catch] command in that interpreter from trapping that error.  It is up to the context that started execution in that interpreter (typically the main interpreter) to handle the error.

# Limit checking api

To check the resource limits for an interpreter, call **Tcl\_LimitCheck**, which returns **TCL\_OK** if the limit was not exceeded (after processing callbacks) and **TCL\_ERROR** if the limit was exceeded (in which case an error message is also placed in the interpreter result).  That function should only be called when **Tcl\_LimitReady** returns non-zero so that granularity policy is enforced.  This API is designed to be similar in usage to **Tcl\_AsyncReady** and **Tcl\_AsyncInvoke**.

When writing code that may behave like [catch] in respect of errors, you should only trap an error if **Tcl\_LimitExceeded** returns zero.  If it returns non-zero, the interpreter is in a limit-exceeded state and errors should be allowed to propagate to the calling context.  You can also check whether a particular type of limit has been exceeded using **Tcl\_LimitTypeExceeded**.

# Limit configuration

To check whether a limit has been set (but not whether it has actually been exceeded) on an interpreter, call **Tcl\_LimitTypeEnabled** with the type of limit you want to check.  To enable a particular limit call **Tcl\_LimitTypeSet**, and to disable a limit call **Tcl\_LimitTypeReset**.

The level of a command limit may be set using **Tcl\_LimitSetCommands**, and retrieved using **Tcl\_LimitGetCommands**.  Similarly for a time limit with **Tcl\_LimitSetTime** and **Tcl\_LimitGetTime** respectively, but with that API the time limit is copied from and to the Tcl\_Time structure that the *timeLimitPtr* argument points to.

The checking granularity for a particular limit may be set using **Tcl\_LimitSetGranularity** and retrieved using **Tcl\_LimitGetGranularity**.  Note that granularities must always be positive.

## Limit callbacks

To add a handler callback to be invoked when a limit is exceeded, call **Tcl\_LimitAddHandler**.  The *handlerProc* argument describes the function that will actually be called; it should have the following prototype:

```
typedef void Tcl_LimitHandlerProc(
        void *clientData,
        Tcl_Interp *interp);
```

The *clientData* argument to the handler will be whatever is passed to the *clientData* argument to **Tcl\_LimitAddHandler**, and the *interp* is the interpreter that had its limit exceeded.

The *deleteProc* argument to **Tcl\_LimitAddHandler** is a function to call to delete the *clientData* value.  It may be **TCL\_STATIC** or NULL if no deletion action is necessary, or **TCL\_DYNAMIC** if all that is necessary is to free the structure with **Tcl\_Free**.  Otherwise, it should refer to a function with the following prototype:

```
typedef void Tcl_LimitHandlerDeleteProc(
        void *clientData);
```

A limit handler may be deleted using **Tcl\_LimitRemoveHandler**; the handler removed will be the first one found (out of the handlers added with **Tcl\_LimitAddHandler**) with exactly matching *type*, *handlerProc* and *clientData* arguments.  This function always invokes the *deleteProc* on the *clientData* (unless the *deleteProc* was NULL or **TCL\_STATIC**).


[catch]: catch.md
[info]: info.md
[while]: while.md

