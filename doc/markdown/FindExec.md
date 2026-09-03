---
CommandName: Tcl_FindExecutable
ManualSection: 3
Version: 9.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - binary
 - executable file
Copyright:
 - Copyright (c) 1995-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_FindExecutable, Tcl\_GetNameOfExecutable - identify or return the name of the binary file containing the application

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[const char \*]{.ret} [Tcl\_FindExecutable]{.ccmd}[argv0]{.cargs}
[const char \*]{.ret} [Tcl\_GetNameOfExecutable]{.ccmd}[]{.cargs}
:::

# Arguments

::: {.arguments} :::

[\*argv0]{.carg .in type="char"}
: The first command-line argument to the program, which gives the application's name.


:::

# Description

**Tcl\_FindExecutable** is one of two functions, **TclZipfs\_AppHook** being the other, that must be called by an application to initialize Tcl prior to any other calls into Tcl. Applications that wish to use ZipFS-based builds should call **TclZipfs\_AppHook** in preference to this function.

On UNIX platforms, the function should be passed *argv[0]* as its argument. It is important not to change the working directory before this invocation. **Tcl\_FindExecutable** uses *argv0* together with the **PATH** environment variable to locate the application's executable, if possible. If it fails to find the binary, subsequent calls to [info nameofexecutable][info] will return an empty string.

On Windows platforms, the *argv[0]* argument is used only to indicate whether the executable has a standard error channel (any non-null value) or not (the value null). If [Tcl\_SetPanicProc][Panic] is never called and no debugger is running, this setting determines whether a panic message is sent to standard error or displayed in a system dialog.

As part of its initialization sequence, **Tcl\_FindExecutable** computes the full path name of the executable file from which the application was invoked. This result is saved for Tcl's internal use and is returned by the [info nameofexecutable][info] command.

The result of **Tcl\_FindExecutable** is the full Tcl version string, including build information (for example, **9.0.0+abcdef...abcdef.gcc-1002**).

**Tcl\_GetNameOfExecutable** simply returns a pointer to the internal full path name of the executable file as computed by **Tcl\_FindExecutable**.  This procedure call is the C API equivalent to the [info nameofexecutable][info] command.  NULL is returned if the internal full path name has not been computed or unknown.

**Tcl\_FindExecutable** can not be used in stub-enabled extensions.


[info]: info.md
[Panic]: Panic.md

