---
CommandName: Tcl_CreateTrace
ManualSection: 3
Version: unknown
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - trace(n)
Keywords:
 - command
 - create
 - delete
 - interpreter
 - trace
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2002 Kevin B. Kenny <kennykb@acm.org>. All rights reserved.
---

# Name

Tcl_CreateTrace, Tcl_CreateObjTrace, Tcl_CreateObjTrace2, Tcl_DeleteTrace - arrange for command execution to be traced

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Trace]{.ret} [Tcl_CreateTrace]{.ccmd}[interp, level, proc, clientData]{.cargs}
[Tcl_Trace]{.ret} [Tcl_CreateObjTrace]{.ccmd}[interp, level, flags, objProc, clientData, deleteProc]{.cargs}
[Tcl_Trace]{.ret} [Tcl_CreateObjTrace2]{.ccmd}[interp, level, flags, objProc2, clientData, deleteProc]{.cargs}
[Tcl_DeleteTrace]{.ccmd}[interp, trace]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter containing command to be traced or untraced. .AP Tcl_Size level in Only commands at or below this nesting level will be traced unless 0 is specified.  1 means top-level commands only, 2 means top-level commands or those that are invoked as immediate consequences of executing top-level commands (procedure bodies, bracketed commands, etc.) and so on. A value of 0 means that commands at any level are traced. .AP int flags in Flags governing the trace execution.  See below for details. .AP Tcl_CmdObjTraceProc *objProc in Procedure to call for each command that is executed.  See below for details of the calling sequence. .AP Tcl_CmdObjTraceProc2 *objProc2 in Procedure to call for each command that is executed.  See below for details of the calling sequence. .AP Tcl_CmdTraceProc *proc in Procedure to call for each command that is executed.  See below for details on the calling sequence. .AP void *clientData in Arbitrary one-word value to pass to *objProc*, *objProc2* or *proc*. .AP Tcl_CmdObjTraceDeleteProc *deleteProc in Procedure to call when the trace is deleted.  See below for details of the calling sequence.  A NULL pointer is permissible and results in no callback when the trace is deleted. .AP Tcl_Trace trace in Token for trace to be removed (return value from previous call to **Tcl_CreateTrace**).

# Description

**Tcl_CreateObjTrace** arranges for command tracing.  After it is called, *objProc* will be invoked before the Tcl interpreter calls any command procedure when evaluating commands in *interp*. The return value from **Tcl_CreateObjTrace** is a token for the trace, which may be passed to **Tcl_DeleteTrace** to remove the trace. There may be many traces in effect simultaneously for the same interpreter.

*objProc* should have arguments and result that match the type, **Tcl_CmdObjTraceProc**:

```
typedef int Tcl_CmdObjTraceProc(
        void * clientData,
        Tcl_Interp* interp,
        int level,
        const char *command,
        Tcl_Command commandToken,
        int objc,
        Tcl_Obj *const objv[]);
```

*objProc2* should have arguments and result that match the type, **Tcl_CmdObjTraceProc2**:

```
typedef int Tcl_CmdObjTraceProc2(
        void * clientData,
        Tcl_Interp* interp,
        Tcl_Size level,
        const char *command,
        Tcl_Command commandToken,
        Tcl_Size objc,
        Tcl_Obj *const objv[]);
```

The *clientData* and *interp* parameters are copies of the corresponding arguments given to **Tcl_CreateTrace**. *clientData* typically points to an application-specific data structure that describes what to do when *objProc* is invoked.  The *level* parameter gives the nesting level of the command (1 for top-level commands passed to **Tcl_Eval** by the application, 2 for the next-level commands passed to **Tcl_Eval** as part of parsing or interpreting level-1 commands, and so on). The *command* parameter points to a string containing the text of the command, before any argument substitution.  The *commandToken* parameter is a Tcl command token that identifies the command to be invoked.  The token may be passed to **Tcl_GetCommandName**, **Tcl_GetCommandInfoFromToken**, or **Tcl_SetCommandInfoFromToken** to manipulate the definition of the command. The *objc* and *objv* parameters designate the final parameter count and parameter vector that will be passed to the command, and have had all substitutions performed.

The *objProc* callback is expected to return a standard Tcl status return code.  If this code is **TCL_OK** (the normal case), then the Tcl interpreter will invoke the command.  Any other return code is treated as if the command returned that status, and the command is *not* invoked.

The *objProc* callback must not modify *objv* in any way.

Tracing will only occur for commands at nesting level less than or equal to the *level* parameter (i.e. the *level* parameter to *objProc* will always be less than or equal to the *level* parameter to **Tcl_CreateTrace**).

Tracing has a significant effect on runtime performance because it causes the bytecode compiler to refrain from generating in-line code for Tcl commands such as **if** and **while** in order that they may be traced.  If traces for the built-in commands are not required, the *flags* parameter may be set to the constant value **TCL_ALLOW_INLINE_COMPILATION**.  In this case, traces on built-in commands may or may not result in trace callbacks, depending on the state of the interpreter, but run-time performance will be improved significantly.  (This functionality is desirable, for example, when using **Tcl_CreateObjTrace** to implement an execution time profiler.)

Calls to *objProc* will be made by the Tcl parser immediately before it calls the command procedure for the command (*cmdProc*).  This occurs after argument parsing and substitution, so tracing for substituted commands occurs before tracing of the commands containing the substitutions.  If there is a syntax error in a command, or if there is no command procedure associated with a command name, then no tracing will occur for that command.  If a string passed to Tcl_Eval contains multiple commands (bracketed, or on different lines) then multiple calls to *objProc* will occur, one for each command.

**Tcl_DeleteTrace** removes a trace, so that no future calls will be made to the procedure associated with the trace.  After **Tcl_DeleteTrace** returns, the caller should never again use the *trace* token.

When **Tcl_DeleteTrace** is called, the interpreter invokes the *deleteProc* that was passed as a parameter to **Tcl_CreateObjTrace**.  The *deleteProc* must match the type, **Tcl_CmdObjTraceDeleteProc**:

```
typedef void Tcl_CmdObjTraceDeleteProc(
        void * clientData);
```

The *clientData* parameter will be the same as the *clientData* parameter that was originally passed to **Tcl_CreateObjTrace**.

**Tcl_CreateTrace** is an alternative interface for command tracing, *not recommended for new applications*.  It is provided for backward compatibility with code that was developed for older versions of the Tcl interpreter.  It is similar to **Tcl_CreateObjTrace**, except that its *proc* parameter should have arguments and result that match the type **Tcl_CmdTraceProc**:

```
typedef void Tcl_CmdTraceProc(
        void *clientData,
        Tcl_Interp *interp,
        int level,
        char *command,
        Tcl_CmdProc *cmdProc,
        void *cmdClientData,
        int argc,
        const char *argv[]);
```

The parameters to the *proc* callback are similar to those of the *objProc* callback above. The *commandToken* is replaced with *cmdProc*, a pointer to the (string-based) command procedure that will be invoked; and *cmdClientData*, the client data that will be passed to the procedure.  The *objc* parameter is replaced with an *argv* parameter, that gives the arguments to the command as character strings. *Proc* must not modify the *command* or *argv* strings.

If a trace created with **Tcl_CreateTrace** is in effect, inline compilation of Tcl commands such as **if** and **while** is always disabled.  There is no notification when a trace created with **Tcl_CreateTrace** is deleted. There is no way to be notified when the trace created by **Tcl_CreateTrace** is deleted.  There is no way for the *proc* associated with a call to **Tcl_CreateTrace** to abort execution of *command*.

# Reference count management

When the *proc* passed to **Tcl_CreateObjTrace** is called, the values in its *objv* argument will have a reference count of at least 1, with that guaranteed reference being from the Tcl evaluation stack. You should not call **Tcl_DecrRefCount** on any of those values unless you call **Tcl_IncrRefCount** on them first.

