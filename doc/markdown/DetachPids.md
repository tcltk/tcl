---
CommandName: Tcl_DetachPids
ManualSection: 3
Version: unknown
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - background
 - child
 - detach
 - process
 - wait
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_DetachPids, Tcl\_ReapDetachedProcs, Tcl\_WaitPid - manage child processes in background

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_DetachPids]{.ccmd}[numPids, pidPtr]{.cargs}
[Tcl\_ReapDetachedProcs]{.ccmd}[]{.cargs}
[Tcl\_Pid]{.ret} [Tcl\_WaitPid]{.ccmd}[pid, statusPtr, options]{.cargs}
:::

# Arguments

.AP Tcl\_Size numPids in Number of process ids contained in the array pointed to by *pidPtr*. .AP int \*pidPtr in Address of array containing *numPids* process ids. .AP Tcl\_Pid pid in The id of the process (pipe) to wait for. .AP int \*statusPtr out The result of waiting on a process (pipe). Either 0 or ECHILD. .AP int options in The options controlling the wait. WNOHANG specifies not to wait when checking the process.

# Description

**Tcl\_DetachPids** and **Tcl\_ReapDetachedProcs** provide a mechanism for managing subprocesses that are running in background. These procedures are needed because the parent of a process must eventually invoke the **waitpid** kernel call (or one of a few other similar kernel calls) to wait for the child to exit.  Until the parent waits for the child, the child's state cannot be completely reclaimed by the system.  If a parent continually creates children and doesn't wait on them, the system's process table will eventually overflow, even if all the children have exited.

**Tcl\_DetachPids** may be called to ask Tcl to take responsibility for one or more processes whose process ids are contained in the *pidPtr* array passed as argument.  The caller presumably has started these processes running in background and does not want to have to deal with them again.

**Tcl\_ReapDetachedProcs** invokes the **waitpid** kernel call on each of the background processes so that its state can be cleaned up if it has exited.  If the process has not exited yet, **Tcl\_ReapDetachedProcs** does not wait for it to exit;  it will check again the next time it is invoked. Tcl automatically calls **Tcl\_ReapDetachedProcs** each time the [exec] command is executed, so in most cases it is not necessary for any code outside of Tcl to invoke **Tcl\_ReapDetachedProcs**. However, if you call **Tcl\_DetachPids** in situations where the [exec] command may never get executed, you may wish to call **Tcl\_ReapDetachedProcs** from time to time so that background processes can be cleaned up.

**Tcl\_WaitPid** is a thin wrapper around the facilities provided by the operating system to wait on the end of a spawned process and to check a whether spawned process is still running. It is used by **Tcl\_ReapDetachedProcs** and the channel system to portably access the operating system. 


[exec]: exec.md

