---
CommandName: Tcl_Main
ManualSection: 3
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - tclsh(1)
 - Tcl_GetStdChannel(3)
 - Tcl_StandardChannels(3)
 - Tcl_AppInit(3)
 - exit(n)
 - encoding(n)
Keywords:
 - application-specific initialization
 - command-line arguments
 - main program
Copyright:
 - Copyright (c) 1994 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2000 Ajuba Solutions.
---

# Name

Tcl\_Main, Tcl\_MainEx, Tcl\_MainExW, Tcl\_SetStartupScript, Tcl\_GetStartupScript, Tcl\_SetMainLoop - main program, startup script, and event loop definition for Tcl-based applications

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Main]{.ccmd}[argc, argv, appInitProc]{.cargs}
[Tcl\_MainEx]{.ccmd}[argc, charargv, appInitProc, interp]{.cargs}
[Tcl\_MainExW]{.ccmd}[argc, wideargv, appInitProc, interp]{.cargs}
[Tcl\_SetStartupScript]{.ccmd}[path, encoding]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_GetStartupScript]{.ccmd}[encodingPtr]{.cargs}
[Tcl\_SetMainLoop]{.ccmd}[mainLoopProc]{.cargs}
:::

# Arguments

.AP Tcl\_Size argc in Number of elements in *argv*. .AP char \*argv[] in Array of strings containing command-line arguments. On Windows, when using **-DUNICODE**, the parameter type changes to wchar\_t \*. .AP char \*charargv[] in As argv, but does not change type to wchar\_t. .AP char \*wideargv[] in As argv, but type is always wchar\_t. .AP Tcl\_AppInitProc \*appInitProc in Address of an application-specific initialization procedure. The value for this argument is usually **Tcl\_AppInit**. .AP Tcl\_Obj \*path in Name of file to use as startup script, or NULL. .AP "const char" \*encoding in Encoding of file to use as startup script, or NULL. .AP "const char" \*\*encodingPtr out If non-NULL, location to write a copy of the (const char \*) pointing to the encoding name. .AP Tcl\_MainLoopProc \*mainLoopProc in Address of an application-specific event loop procedure. .AP Tcl\_Interp \*interp in Already created Tcl Interpreter.

# Description

**Tcl\_Main** can serve as the main program for Tcl-based shell applications.  A "shell application" is a program like tclsh or wish that supports both interactive interpretation of Tcl and evaluation of a script contained in a file given as a command line argument.  **Tcl\_Main** is offered as a convenience to developers of shell applications, so they do not have to reproduce all of the code for proper initialization of the Tcl library and interactive shell operation.  Other styles of embedding Tcl in an application are not supported by **Tcl\_Main**.  Those must be achieved by calling lower level functions in the Tcl library directly.

The **Tcl\_Main** function has been offered by the Tcl library since release Tcl 7.4.  In older releases of Tcl, the Tcl library itself defined a function **main**, but that lacks flexibility of embedding style and having a function **main** in a library (particularly a shared library) causes problems on many systems. Having **main** in the Tcl library would also make it hard to use Tcl in C++ programs, since C++ programs must have special C++ **main** functions.

Normally each shell application contains a small **main** function that does nothing but invoke **Tcl\_Main**. **Tcl\_Main** then does all the work of creating and running a **tclsh**-like application.

**Tcl\_Main** is not provided by the public interface of Tcl's stub library.  Programs that call **Tcl\_Main** must be linked against the standard Tcl library.  If the standard Tcl library is a dll (so, not a static .lib/.a) , then the program must be linked against the stub library as well. Extensions (stub-enabled or not) are not intended to call **Tcl\_Main**.

**Tcl\_Main** is not thread-safe.  It should only be called by a single main thread of a multi-threaded application.  This restriction is not a problem with normal use described above.

**Tcl\_Main** and therefore all applications based upon it, like **tclsh**, use **Tcl\_GetStdChannel** to initialize the standard channels to their default values. See **Tcl\_StandardChannels** for more information.

**Tcl\_Main** supports two modes of operation, depending on whether the filename and encoding of a startup script has been established.  The routines **Tcl\_SetStartupScript** and **Tcl\_GetStartupScript** are the tools for controlling this configuration of **Tcl\_Main**.

**Tcl\_SetStartupScript** registers the value *path* as the name of the file for **Tcl\_Main** to evaluate as its startup script.  The value *encoding* is Tcl's name for the encoding used to store the text in that file.  A value of **NULL** for *encoding* is a signal to use the system encoding.  A value of **NULL** for *path* erases any existing registration so that **Tcl\_Main** will not evaluate any startup script.

**Tcl\_GetStartupScript** queries the registered file name and encoding set by the most recent **Tcl\_SetStartupScript** call in the same thread.  The stored file name is returned, and the stored encoding name is written to space pointed to by *encodingPtr*, when that is not NULL.

The file name and encoding values managed by the routines **Tcl\_SetStartupScript** and **Tcl\_GetStartupScript** are stored per-thread.  Although the storage and retrieval functions of these routines work in any thread, only those calls in the same main thread as **Tcl\_Main** can have any influence on it.

The caller of **Tcl\_Main** may call **Tcl\_SetStartupScript** first to establish its desired startup script.  If **Tcl\_Main** finds that no such startup script has been established, it consults the first few arguments in *argv*.  If they match ?**-encoding** *name*? *fileName*, where *fileName* does not begin with the character *-*, then *fileName* is taken to be the name of a file containing a *startup script*, and *name* is taken to be the name of the encoding of the contents of that file.  **Tcl\_Main** then calls **Tcl\_SetStartupScript** with these values.

**Tcl\_Main** then defines in its main interpreter the Tcl variables *argc*, *argv*, *argv0*, and *tcl\_interactive*, as described in the documentation for **tclsh**.

When it has finished its own initialization, but before it processes commands, **Tcl\_Main** calls the procedure given by the *appInitProc* argument.  This procedure provides a "hook" for the application to perform its own initialization of the interpreter created by **Tcl\_Main**, such as defining application-specific commands.  The application initialization routine might also call **Tcl\_SetStartupScript** to (re-)set the file and encoding to be used as a startup script.  The procedure must have an interface that matches the type **Tcl\_AppInitProc**:

```
typedef int Tcl_AppInitProc(
        Tcl_Interp *interp);
```

*AppInitProc* is almost always a pointer to **Tcl\_AppInit**; for more details on this procedure, see the documentation for **Tcl\_AppInit**.

When the *appInitProc* is finished, **Tcl\_Main** calls **Tcl\_GetStartupScript** to determine what startup script has been requested, if any.  If a startup script has been provided, **Tcl\_Main** attempts to evaluate it.  Otherwise, interactive mode begins with examination of the variable *tcl\_rcFileName* in the main interpreter.  If that variable exists and holds the name of a readable file, the contents of that file are evaluated in the main interpreter.  Then interactive operations begin, with prompts and command evaluation results written to the standard output channel, and commands read from the standard input channel and then evaluated.  The prompts written to the standard output channel may be customized by defining the Tcl variables *tcl\_prompt1* and *tcl\_prompt2* as described in the documentation for **tclsh**. The prompts and command evaluation results are written to the standard output channel only if the Tcl variable *tcl\_interactive* in the main interpreter holds a non-zero integer value.

**Tcl\_SetMainLoop** allows setting an event loop procedure to be run. This allows, for example, Tk to be dynamically loaded and set its event loop.  The event loop will run following the startup script.  If you are in interactive mode, setting the main loop procedure will cause the prompt to become fileevent based and then the loop procedure is called. When the loop procedure returns in interactive mode, interactive operation will continue. The main loop procedure must have an interface that matches the type **Tcl\_MainLoopProc**:

```
typedef void Tcl_MainLoopProc(void);
```

**Tcl\_Main** does not return.  Normally a program based on **Tcl\_Main** will terminate when the [exit] command is evaluated.  In interactive mode, if an EOF or channel error is encountered on the standard input channel, then **Tcl\_Main** itself will evaluate the [exit] command after the main loop procedure (if any) returns.  In non-interactive mode, after **Tcl\_Main** evaluates the startup script, and the main loop procedure (if any) returns, **Tcl\_Main** will also evaluate the [exit] command.

**Tcl\_Main** can not be used in stub-enabled extensions.

The difference between Tcl\_MainEx and Tcl\_MainExW is that the arguments are passed as characters or wide characters. When used in stub-enabled embedders, the stubs table must be first initialized using one of **Tcl\_InitSubsystems**, **Tcl\_SetPanicProc**, **Tcl\_FindExecutable** or **TclZipfs\_AppHook**.

# Reference count management

**Tcl\_SetStartupScript** takes a value (or NULL) for its *path* argument, and will increment the reference count of it.

**Tcl\_GetStartupScript** returns a value with reference count at least 1, or NULL. It's *encodingPtr* is also used (if non-NULL) to return a value with a reference count at least 1, or NULL. In both cases, the owner of the values is the current thread.


[exit]: exit.md

