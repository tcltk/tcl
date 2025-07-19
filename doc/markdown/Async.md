---
CommandName: Tcl_AsyncCreate
ManualSection: 3
Version: 7.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - asynchronous event
 - handler
 - signal
 - Tcl_SaveInterpState
 - thread
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_AsyncCreate, Tcl_AsyncMark, Tcl_AsyncMarkFromSignal, Tcl_AsyncInvoke, Tcl_AsyncDelete, Tcl_AsyncReady - handle asynchronous events

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_AsyncHandler]{.ret} [Tcl_AsyncCreate]{.ccmd}[proc, clientData]{.cargs}
[void]{.ret} [Tcl_AsyncMark]{.ccmd}[async]{.cargs}
[int]{.ret} [Tcl_AsyncMarkFromSignal]{.ccmd}[async=, +sigNumber]{.cargs}
[int]{.ret} [Tcl_AsyncInvoke]{.ccmd}[interp, code]{.cargs}
[void]{.ret} [Tcl_AsyncDelete]{.ccmd}[async]{.cargs}
[int]{.ret} [Tcl_AsyncReady]{.ccmd}[]{.cargs}
:::

# Arguments

.AP Tcl_AsyncProc *proc in Procedure to invoke to handle an asynchronous event. .AP void *clientData in One-word value to pass to *proc*. .AP Tcl_AsyncHandler async in Token for asynchronous event handler. .AP int sigNumber in POSIX signal number, when used in a signal context. .AP Tcl_Interp *interp in Tcl interpreter in which command was being evaluated when handler was invoked, or NULL if handler was invoked when there was no interpreter active. .AP int code in Completion code from command that just completed in *interp*, or 0 if *interp* is NULL. 

# Description

These procedures provide a safe mechanism for dealing with asynchronous events such as signals. If an event such as a signal occurs while a Tcl script is being evaluated then it is not safe to take any substantive action to process the event. For example, it is not safe to evaluate a Tcl script since the interpreter may already be in the middle of evaluating a script; it may not even be safe to allocate memory, since a memory allocation could have been in progress when the event occurred. The only safe approach is to set a flag indicating that the event occurred, then handle the event later when the world has returned to a clean state, such as after the current Tcl command completes.

**Tcl_AsyncCreate**, **Tcl_AsyncDelete**, and **Tcl_AsyncReady** are thread sensitive.  They access and/or set a thread-specific data structure in the event of a core built with *--enable-threads*. The token created by **Tcl_AsyncCreate** contains the needed thread information it was called from so that calling **Tcl_AsyncMarkFromSignal** or **Tcl_AsyncMark** with this token will only yield the origin thread into the asynchronous handler.

**Tcl_AsyncCreate** creates an asynchronous handler and returns a token for it. The asynchronous handler must be created before any occurrences of the asynchronous event that it is intended to handle (it is not safe to create a handler at the time of an event). When an asynchronous event occurs the code that detects the event (such as a POSIX signal handler) should call **Tcl_AsyncMarkFromSignal** with the token for the handler and the POSIX signal number. The return value of this function is true, when the handler will be marked, false otherwise. For non-signal contexts, **Tcl_AsyncMark** serves the same purpose. **Tcl_AsyncMarkFromSignal** and **Tcl_AsyncMark** will mark the handler as ready to execute, but will not invoke the handler immediately. Tcl will call the *proc* associated with the handler later, when the world is in a safe state, and *proc* can then carry out the actions associated with the asynchronous event. *Proc* should have arguments and result that match the type **Tcl_AsyncProc**:

```
typedef int Tcl_AsyncProc(
        void *clientData,
        Tcl_Interp *interp,
        int code);
```

The *clientData* will be the same as the *clientData* argument passed to **Tcl_AsyncCreate** when the handler was created. If *proc* is invoked just after a command has completed execution in an interpreter, then *interp* will identify the interpreter in which the command was evaluated and *code* will be the completion code returned by that command. The command's result will be present in the interpreter's result. When *proc* returns, whatever it leaves in the interpreter's result will be returned as the result of the command and the integer value returned by *proc* will be used as the new completion code for the command.

It is also possible for *proc* to be invoked when no interpreter is active. This can happen, for example, if an asynchronous event occurs while the application is waiting for interactive input or an X event. In this case *interp* will be NULL and *code* will be 0, and the return value from *proc* will be ignored.

The procedure **Tcl_AsyncInvoke** is called to invoke all of the handlers that are ready. The procedure **Tcl_AsyncReady** will return non-zero whenever any asynchronous handlers are ready;  it can be checked to avoid calls to **Tcl_AsyncInvoke** when there are no ready handlers. Tcl calls **Tcl_AsyncReady** after each command is evaluated and calls **Tcl_AsyncInvoke** if needed. Applications may also call **Tcl_AsyncInvoke** at interesting times for that application. For example, Tcl's event handler calls **Tcl_AsyncReady** after each event and calls **Tcl_AsyncInvoke** if needed. The *interp* and *code* arguments to **Tcl_AsyncInvoke** have the same meaning as for *proc*:  they identify the active interpreter, if any, and the completion code from the command that just completed.

**Tcl_AsyncDelete** removes an asynchronous handler so that its *proc* will never be invoked again. A handler can be deleted even when ready, and it will still not be invoked.

If multiple handlers become active at the same time, the handlers are invoked in the order they were created (oldest handler first). The *code* and the interpreter's result for later handlers reflect the values returned by earlier handlers, so that the most recently created handler has last say about the interpreter's result and completion code. If new handlers become ready while handlers are executing, **Tcl_AsyncInvoke** will invoke them all;  at each point it invokes the highest-priority (oldest) ready handler, repeating this over and over until there are no longer any ready handlers.

# Warning

It is almost always a bad idea for an asynchronous event handler to modify the interpreter's result or return a code different from its *code* argument. This sort of behavior can disrupt the execution of scripts in subtle ways and result in bugs that are extremely difficult to track down. If an asynchronous event handler needs to evaluate Tcl scripts then it should first save the interpreter's state by calling **Tcl_SaveInterpState**, passing in the *code* argument. When the asynchronous handler is finished it should restore the interpreter's state by calling **Tcl_RestoreInterpState**, and then returning the *code* argument. 

