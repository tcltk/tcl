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

Tcl_Exit, Tcl_Finalize, Tcl_CreateExitHandler, Tcl_DeleteExitHandler, Tcl_ExitThread, Tcl_FinalizeThread, Tcl_CreateThreadExitHandler, Tcl_DeleteThreadExitHandler, Tcl_SetExitProc - end the application or thread (and invoke exit handlers)

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Exit]{.ccmd}[status]{.cargs}
[Tcl_Finalize]{.ccmd}[]{.cargs}
[Tcl_CreateExitHandler]{.ccmd}[proc, clientData]{.cargs}
[Tcl_DeleteExitHandler]{.ccmd}[proc, clientData]{.cargs}
[Tcl_ExitThread]{.ccmd}[status]{.cargs}
[Tcl_FinalizeThread]{.ccmd}[]{.cargs}
[Tcl_CreateThreadExitHandler]{.ccmd}[proc, clientData]{.cargs}
[Tcl_DeleteThreadExitHandler]{.ccmd}[proc, clientData]{.cargs}
[Tcl_ExitProc *]{.ret} [Tcl_SetExitProc]{.ccmd}[proc]{.cargs}
:::

# Arguments

.AP int status  in Provides information about why the application or thread exited. Exact meaning may be platform-specific. 0 usually means a normal exit, any nonzero value usually means that an error occurred. .AP Tcl_ExitProc *proc in Procedure to invoke before exiting application, or (for **Tcl_SetExitProc**) NULL to uninstall the current application exit procedure. .AP void *clientData in Arbitrary one-word value to pass to *proc*. 

# Description

The procedures described here provide a graceful mechanism to end the execution of a **Tcl** application. Exit handlers are invoked to cleanup the application's state before ending the execution of **Tcl** code.

Invoke **Tcl_Exit** to end a **Tcl** application and to exit from this process. This procedure is invoked by the **exit** Tcl command, and can be invoked anyplace else to terminate the application. No-one should ever invoke the **exit()** system call directly;  always invoke **Tcl_Exit** instead, so that it can invoke exit handlers. Note that if other code invokes **exit()** system call directly, or otherwise causes the application to terminate without calling **Tcl_Exit**, the exit handlers will not be run. **Tcl_Exit** internally invokes the **exit()** system call, thus it never returns control to its caller. If an application exit handler has been installed (see **Tcl_SetExitProc**), that handler is invoked with an argument consisting of the exit status (cast to void *); the application exit handler should not return control to Tcl.

**Tcl_Finalize** is similar to **Tcl_Exit** except that it does not exit from the current process. It is useful for cleaning up when a process is finished using **Tcl** but wishes to continue executing, and when **Tcl** is used in a dynamically loaded extension that is about to be unloaded. Your code should always invoke **Tcl_Finalize** when **Tcl** is being unloaded, to ensure proper cleanup. **Tcl_Finalize** can be safely called more than once.

**Tcl_ExitThread** is used to terminate the current thread and invoke per-thread exit handlers.  This finalization is done by **Tcl_FinalizeThread**, which you can call if you just want to clean up per-thread state and invoke the thread exit handlers. **Tcl_Finalize** calls **Tcl_FinalizeThread** for the current thread automatically.

**Tcl_CreateExitHandler** arranges for *proc* to be invoked by **Tcl_Finalize** and **Tcl_Exit**. **Tcl_CreateThreadExitHandler** arranges for *proc* to be invoked by **Tcl_FinalizeThread** and **Tcl_ExitThread**. This provides a hook for cleanup operations such as flushing buffers and freeing global memory. *Proc* should match the type **Tcl_ExitProc**:

```
typedef void Tcl_ExitProc(
        void *clientData);
```

The *clientData* parameter to *proc* is a copy of the *clientData* argument given to **Tcl_CreateExitHandler** or **Tcl_CreateThreadExitHandler** when the callback was created.  Typically, *clientData* points to a data structure containing application-specific information about what to do in *proc*.

**Tcl_DeleteExitHandler** and **Tcl_DeleteThreadExitHandler** may be called to delete a previously-created exit handler.  It removes the handler indicated by *proc* and *clientData* so that no call to *proc* will be made.  If no such handler exists then **Tcl_DeleteExitHandler** or **Tcl_DeleteThreadExitHandler** does nothing.

**Tcl_Finalize** and **Tcl_Exit** execute all registered exit handlers, in reverse order from the order in which they were registered. This matches the natural order in which extensions are loaded and unloaded; if extension **A** loads extension **B**, it usually unloads **B** before it itself is unloaded. If extension **A** registers its exit handlers before loading extension **B**, this ensures that any exit handlers for **B** will be executed before the exit handlers for **A**.

**Tcl_Finalize** and **Tcl_Exit** call **Tcl_FinalizeThread** and the thread exit handlers *after* the process-wide exit handlers.  This is because thread finalization shuts down the I/O channel system, so any attempt at I/O by the global exit handlers will vanish into the bitbucket.

**Tcl_SetExitProc** installs an application exit handler, returning the previously-installed application exit handler or NULL if no application handler was installed.  If an application exit handler is installed, that exit handler takes over complete responsibility for finalization of Tcl's subsystems via **Tcl_Finalize** at an appropriate time.  The argument passed to *proc* when it is invoked will be the exit status code (as passed to **Tcl_Exit**) cast to a void *value.

**Tcl_SetExitProc** can not be used in stub-enabled extensions.

