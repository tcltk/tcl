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

Tcl_LimitAddHandler, Tcl_LimitCheck, Tcl_LimitExceeded, Tcl_LimitGetCommands, Tcl_LimitGetGranularity, Tcl_LimitGetTime, Tcl_LimitReady, Tcl_LimitRemoveHandler, Tcl_LimitSetCommands, Tcl_LimitSetGranularity, Tcl_LimitSetTime, Tcl_LimitTypeEnabled, Tcl_LimitTypeExceeded, Tcl_LimitTypeReset, Tcl_LimitTypeSet - manage and check resource limits on interpreters

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_LimitCheck]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl_LimitReady]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl_LimitExceeded]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl_LimitTypeExceeded]{.ccmd}[interp, type]{.cargs}
[int]{.ret} [Tcl_LimitTypeEnabled]{.ccmd}[interp, type]{.cargs}
[Tcl_LimitTypeSet]{.ccmd}[interp, type]{.cargs}
[Tcl_LimitTypeReset]{.ccmd}[interp, type]{.cargs}
[Tcl_Size]{.ret} [Tcl_LimitGetCommands]{.ccmd}[interp]{.cargs}
[Tcl_LimitSetCommands]{.ccmd}[interp, commandLimit]{.cargs}
[Tcl_LimitGetTime]{.ccmd}[interp, timeLimitPtr]{.cargs}
[Tcl_LimitSetTime]{.ccmd}[interp, timeLimitPtr]{.cargs}
[int]{.ret} [Tcl_LimitGetGranularity]{.ccmd}[interp, type]{.cargs}
[Tcl_LimitSetGranularity]{.ccmd}[interp, type, granularity]{.cargs}
[Tcl_LimitAddHandler]{.ccmd}[interp, type, handlerProc, clientData, deleteProc]{.cargs}
[Tcl_LimitRemoveHandler]{.ccmd}[interp, type, handlerProc, clientData]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter that the limit being managed applies to or that will have its limits checked. .AP int type in The type of limit that the operation refers to.  This must be either **TCL_LIMIT_COMMANDS** or **TCL_LIMIT_TIME**. .AP Tcl_Size commandLimit in The maximum number of commands (as reported by **info cmdcount**) that may be executed in the interpreter. .AP Tcl_Time *timeLimitPtr in/out A pointer to a structure that will either have the new time limit read from (**Tcl_LimitSetTime**) or the current time limit written to (**Tcl_LimitGetTime**). .AP int granularity in Divisor that indicates how often a particular limit should really be checked.  Must be at least 1. .AP Tcl_LimitHandlerProc *handlerProc in Function to call when a particular limit is exceeded.  If the *handlerProc* removes or raises the limit during its processing, the limited interpreter will be permitted to continue to process after the handler returns.  Many handlers may be attached to the same interpreter limit; their order of execution is not defined, and they must be identified by *handlerProc* and *clientData* when they are deleted. .AP void *clientData in Arbitrary pointer-sized word used to pass some context to the *handlerProc* function. .AP Tcl_LimitHandlerDeleteProc *deleteProc in Function to call whenever a handler is deleted.  May be NULL if the *clientData* requires no deletion.

# Description

Tcl's interpreter resource limit subsystem allows for close control over how much computation time a script may use, and is useful for cases where a program is divided into multiple pieces where some parts are more trusted than others (e.g. web application servers).

Every interpreter may have a limit on the wall-time for execution, and a limit on the number of commands that the interpreter may execute. Since checking of these limits is potentially expensive (especially the time limit), each limit also has a checking granularity, which is a divisor for an internal count of the number of points in the core where a check may be performed (which is immediately before executing a command and at an unspecified frequency between running commands, which can happen in empty-bodied **while** loops).

The final component of the limit engine is a callback scheme which allows for notifications of when a limit has been exceeded.  These callbacks can just provide logging, or may allocate more resources to the interpreter to permit it to continue processing longer.

When a limit is exceeded (and the callbacks have run; the order of execution of the callbacks is unspecified) execution in the limited interpreter is stopped by raising an error and setting a flag that prevents the **catch** command in that interpreter from trapping that error.  It is up to the context that started execution in that interpreter (typically the main interpreter) to handle the error.

# Limit checking api

To check the resource limits for an interpreter, call **Tcl_LimitCheck**, which returns **TCL_OK** if the limit was not exceeded (after processing callbacks) and **TCL_ERROR** if the limit was exceeded (in which case an error message is also placed in the interpreter result).  That function should only be called when **Tcl_LimitReady** returns non-zero so that granularity policy is enforced.  This API is designed to be similar in usage to **Tcl_AsyncReady** and **Tcl_AsyncInvoke**.

When writing code that may behave like **catch** in respect of errors, you should only trap an error if **Tcl_LimitExceeded** returns zero.  If it returns non-zero, the interpreter is in a limit-exceeded state and errors should be allowed to propagate to the calling context.  You can also check whether a particular type of limit has been exceeded using **Tcl_LimitTypeExceeded**.

# Limit configuration

To check whether a limit has been set (but not whether it has actually been exceeded) on an interpreter, call **Tcl_LimitTypeEnabled** with the type of limit you want to check.  To enable a particular limit call **Tcl_LimitTypeSet**, and to disable a limit call **Tcl_LimitTypeReset**.

The level of a command limit may be set using **Tcl_LimitSetCommands**, and retrieved using **Tcl_LimitGetCommands**.  Similarly for a time limit with **Tcl_LimitSetTime** and **Tcl_LimitGetTime** respectively, but with that API the time limit is copied from and to the Tcl_Time structure that the *timeLimitPtr* argument points to.

The checking granularity for a particular limit may be set using **Tcl_LimitSetGranularity** and retrieved using **Tcl_LimitGetGranularity**.  Note that granularities must always be positive.

## Limit callbacks

To add a handler callback to be invoked when a limit is exceeded, call **Tcl_LimitAddHandler**.  The *handlerProc* argument describes the function that will actually be called; it should have the following prototype:

```
typedef void Tcl_LimitHandlerProc(
        void *clientData,
        Tcl_Interp *interp);
```

The *clientData* argument to the handler will be whatever is passed to the *clientData* argument to **Tcl_LimitAddHandler**, and the *interp* is the interpreter that had its limit exceeded.

The *deleteProc* argument to **Tcl_LimitAddHandler** is a function to call to delete the *clientData* value.  It may be **TCL_STATIC** or NULL if no deletion action is necessary, or **TCL_DYNAMIC** if all that is necessary is to free the structure with **Tcl_Free**.  Otherwise, it should refer to a function with the following prototype:

```
typedef void Tcl_LimitHandlerDeleteProc(
        void *clientData);
```

A limit handler may be deleted using **Tcl_LimitRemoveHandler**; the handler removed will be the first one found (out of the handlers added with **Tcl_LimitAddHandler**) with exactly matching *type*, *handlerProc* and *clientData* arguments.  This function always invokes the *deleteProc* on the *clientData* (unless the *deleteProc* was NULL or **TCL_STATIC**).

