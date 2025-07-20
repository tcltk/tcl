---
CommandName: Tcl_TraceCommand
ManualSection: 3
Version: 7.4
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - clientData
 - trace
 - command
Copyright:
 - Copyright (c) 2002 Donal K. Fellows
---

# Name

Tcl_CommandTraceInfo, Tcl_TraceCommand, Tcl_UntraceCommand - monitor renames and deletes of a command

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[void *]{.ret} [Tcl_CommandTraceInfo]{.ccmd}[interp, cmdName, flags, proc, prevClientData]{.cargs}
[int]{.ret} [Tcl_TraceCommand]{.ccmd}[interp, cmdName, flags, proc, clientData]{.cargs}
[Tcl_UntraceCommand]{.ccmd}[interp, cmdName, flags, proc, clientData]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter containing the command. .AP "const char" *cmdName in Name of command. .AP int flags in OR'ed collection of the values **TCL_TRACE_RENAME** and **TCL_TRACE_DELETE**. .AP Tcl_CommandTraceProc *proc in Procedure to call when specified operations occur to *cmdName*. .AP void *clientData in Arbitrary argument to pass to *proc*. .AP void *prevClientData in If non-NULL, gives last value returned by **Tcl_CommandTraceInfo**, so this call will return information about next trace.  If NULL, this call will return information about first trace.

# Description

**Tcl_TraceCommand** allows a C procedure to monitor operations performed on a Tcl command, so that the C procedure is invoked whenever the command is renamed or deleted.  If the trace is created successfully then **Tcl_TraceCommand** returns **TCL_OK**. If an error occurred (e.g. *cmdName* specifies a non-existent command) then **TCL_ERROR** is returned and an error message is left in the interpreter's result.

The *flags* argument to **Tcl_TraceCommand** indicates when the trace procedure is to be invoked.  It consists of an OR'ed combination of any of the following values:

**TCL_TRACE_RENAME**
: Invoke *proc* whenever the command is renamed.

**TCL_TRACE_DELETE**
: Invoke *proc* when the command is deleted.


Whenever one of the specified operations occurs to the command, *proc* will be invoked.  It should have arguments and result that match the type **Tcl_CommandTraceProc**:

```
typedef void Tcl_CommandTraceProc(
        void *clientData,
        Tcl_Interp *interp,
        const char *oldName,
        const char *newName,
        int flags);
```

The *clientData* and *interp* parameters will have the same values as those passed to **Tcl_TraceCommand** when the trace was created.  *clientData* typically points to an application-specific data structure that describes what to do when *proc* is invoked. *OldName* gives the name of the command being renamed, and *newName* gives the name that the command is being renamed to (or NULL when the command is being deleted.) *Flags* is an OR'ed combination of bits potentially providing several pieces of information.  One of the bits **TCL_TRACE_RENAME** and **TCL_TRACE_DELETE** will be set in *flags* to indicate which operation is being performed on the command.  The bit **TCL_TRACE_DESTROYED** will be set in *flags* if the trace is about to be destroyed; this information may be useful to *proc* so that it can clean up its own internal data structures (see the section **TCL_TRACE_DESTROYED** below for more details).  Because the deletion of commands can take place as part of the deletion of the interp that contains them, *proc* must be careful about checking what the passed in *interp* value can be called upon to do. The routine **Tcl_InterpDeleted** is an important tool for this. When **Tcl_InterpDeleted** returns 1, *proc* will not be able to invoke any scripts in *interp*.  The function of *proc* in that circumstance is limited to the cleanup of its own data structures.

**Tcl_UntraceCommand** may be used to remove a trace.  If the command specified by *interp*, *cmdName*, and *flags* has a trace set with *flags*, *proc*, and *clientData*, then the corresponding trace is removed.  If no such trace exists, then the call to **Tcl_UntraceCommand** has no effect.  The same bits are valid for *flags* as for calls to **Tcl_TraceCommand**.

**Tcl_CommandTraceInfo** may be used to retrieve information about traces set on a given command. The return value from **Tcl_CommandTraceInfo** is the *clientData* associated with a particular trace. The trace must be on the command specified by the *interp*, *cmdName*, and *flags* arguments (note that currently the flags are ignored; *flags* should be set to 0 for future compatibility) and its trace procedure must the same as the *proc* argument. If the *prevClientData* argument is NULL then the return value corresponds to the first (most recently created) matching trace, or NULL if there are no matching traces. If the *prevClientData* argument is not NULL, then it should be the return value from a previous call to **Tcl_CommandTraceInfo**. In this case, the new return value will correspond to the next matching trace after the one whose *clientData* matches *prevClientData*, or NULL if no trace matches *prevClientData* or if there are no more matching traces after it. This mechanism makes it possible to step through all of the traces for a given command that have the same *proc*.

# Calling commands during traces

During rename traces, the command being renamed is visible with both names simultaneously, and the command still exists during delete traces, unless the interp that contains it is being deleted. However, there is no mechanism for signaling that an error occurred in a trace procedure, so great care should be taken that errors do not get silently lost.

# Multiple traces

It is possible for multiple traces to exist on the same command. When this happens, all of the trace procedures will be invoked on each access, in order from most-recently-created to least-recently-created. Attempts to delete the command during a delete trace will fail silently, since the command is already scheduled for deletion anyway. If the command being renamed is renamed by one of its rename traces, that renaming takes precedence over the one that triggered the trace and the collection of traces will not be reexecuted; if several traces rename the command, the last renaming takes precedence.

# Tcl_trace_destroyed flag

In a delete callback to *proc*, the **TCL_TRACE_DESTROYED** bit is set in *flags*.

