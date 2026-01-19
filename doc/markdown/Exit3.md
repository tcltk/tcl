---
CommandName: Tcl_Exit
ManualSection: 3
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - exit(n)
Keywords:
 - abort
 - callback
 - cleanup
 - dynamic loading
 - end application
 - exit
 - unloading
 - thread
Copyright:
 - Copyright (c) 1995-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_Exit, Tcl\_Finalize, Tcl\_CreateExitHandler, Tcl\_DeleteExitHandler, Tcl\_ExitThread, Tcl\_FinalizeThread, Tcl\_CreateThreadExitHandler, Tcl\_DeleteThreadExitHandler, Tcl\_SetExitProc - end the application or thread (and invoke exit handlers)

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Exit]{.ccmd}[status]{.cargs}
[Tcl\_Finalize]{.ccmd}[]{.cargs}
[Tcl\_CreateExitHandler]{.ccmd}[proc, clientData]{.cargs}
[Tcl\_DeleteExitHandler]{.ccmd}[proc, clientData]{.cargs}
[Tcl\_ExitThread]{.ccmd}[status]{.cargs}
[Tcl\_FinalizeThread]{.ccmd}[]{.cargs}
[Tcl\_CreateThreadExitHandler]{.ccmd}[proc, clientData]{.cargs}
[Tcl\_DeleteThreadExitHandler]{.ccmd}[proc, clientData]{.cargs}
[Tcl\_ExitProc \*]{.ret} [Tcl\_SetExitProc]{.ccmd}[proc]{.cargs}
:::

# Arguments

.AP int status  in Provides information about why the application or thread exited. Exact meaning may be platform-specific. 0 usually means a normal exit, any nonzero value usually means that an error occurred. .AP Tcl\_ExitProc \*proc in Procedure to invoke before exiting application, or (for **Tcl\_SetExitProc**) NULL to uninstall the current application exit procedure. .AP void \*clientData in Arbitrary one-word value to pass to *proc*. 

# Description

The procedures described here provide a graceful mechanism to end the execution of a **Tcl** application. Exit handlers are invoked to cleanup the application's state before ending the execution of **Tcl** code.

Invoke **Tcl\_Exit** to end a **Tcl** application and to exit from this process. This procedure is invoked by the [exit] Tcl command, and can be invoked anyplace else to terminate the application. No-one should ever invoke the **exit()** system call directly;  always invoke **Tcl\_Exit** instead, so that it can invoke exit handlers. Note that if other code invokes **exit()** system call directly, or otherwise causes the application to terminate without calling **Tcl\_Exit**, the exit handlers will not be run. **Tcl\_Exit** internally invokes the **exit()** system call, thus it never returns control to its caller. If an application exit handler has been installed (see **Tcl\_SetExitProc**), that handler is invoked with an argument consisting of the exit status (cast to void \*); the application exit handler should not return control to Tcl.

**Tcl\_Finalize** is similar to **Tcl\_Exit** except that it does not exit from the current process. It is useful for cleaning up when a process is finished using **Tcl** but wishes to continue executing, and when **Tcl** is used in a dynamically loaded extension that is about to be unloaded. Your code should always invoke **Tcl\_Finalize** when **Tcl** is being unloaded, to ensure proper cleanup. **Tcl\_Finalize** can be safely called more than once.

**Tcl\_ExitThread** is used to terminate the current thread and invoke per-thread exit handlers.  This finalization is done by **Tcl\_FinalizeThread**, which you can call if you just want to clean up per-thread state and invoke the thread exit handlers. **Tcl\_Finalize** calls **Tcl\_FinalizeThread** for the current thread automatically.

**Tcl\_CreateExitHandler** arranges for *proc* to be invoked by **Tcl\_Finalize** and **Tcl\_Exit**. **Tcl\_CreateThreadExitHandler** arranges for *proc* to be invoked by **Tcl\_FinalizeThread** and **Tcl\_ExitThread**. This provides a hook for cleanup operations such as flushing buffers and freeing global memory. *Proc* should match the type **Tcl\_ExitProc**:

```
typedef void Tcl_ExitProc(
        void *clientData);
```

The *clientData* parameter to *proc* is a copy of the *clientData* argument given to **Tcl\_CreateExitHandler** or **Tcl\_CreateThreadExitHandler** when the callback was created.  Typically, *clientData* points to a data structure containing application-specific information about what to do in *proc*.

**Tcl\_DeleteExitHandler** and **Tcl\_DeleteThreadExitHandler** may be called to delete a previously-created exit handler.  It removes the handler indicated by *proc* and *clientData* so that no call to *proc* will be made.  If no such handler exists then **Tcl\_DeleteExitHandler** or **Tcl\_DeleteThreadExitHandler** does nothing.

**Tcl\_Finalize** and **Tcl\_Exit** execute all registered exit handlers, in reverse order from the order in which they were registered. This matches the natural order in which extensions are loaded and unloaded; if extension **A** loads extension **B**, it usually unloads **B** before it itself is unloaded. If extension **A** registers its exit handlers before loading extension **B**, this ensures that any exit handlers for **B** will be executed before the exit handlers for **A**.

**Tcl\_Finalize** and **Tcl\_Exit** call **Tcl\_FinalizeThread** and the thread exit handlers *after* the process-wide exit handlers.  This is because thread finalization shuts down the I/O channel system, so any attempt at I/O by the global exit handlers will vanish into the bitbucket.

**Tcl\_SetExitProc** installs an application exit handler, returning the previously-installed application exit handler or NULL if no application handler was installed.  If an application exit handler is installed, that exit handler takes over complete responsibility for finalization of Tcl's subsystems via **Tcl\_Finalize** at an appropriate time.  The argument passed to *proc* when it is invoked will be the exit status code (as passed to **Tcl\_Exit**) cast to a void \*value.

**Tcl\_SetExitProc** can not be used in stub-enabled extensions.


[exit]: exit.md

