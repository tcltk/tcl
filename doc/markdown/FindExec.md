---
CommandName: Tcl_FindExecutable
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - binary
 - executable file
Copyright:
 - Copyright (c) 1995-1996 Sun Microsystems, Inc.
---

# Name

Tcl_FindExecutable, Tcl_GetNameOfExecutable - identify or return the name of the binary file containing the application

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[const char *]{.ret} [Tcl_FindExecutable]{.ccmd}[argv0]{.cargs}
[const char *]{.ret} [Tcl_GetNameOfExecutable]{.ccmd}[]{.cargs}
:::

# Arguments

.AP char *argv0 in The first command-line argument to the program, which gives the application's name. 

# Description

The **Tcl_FindExecutable** procedure computes the full path name of the executable file from which the application was invoked and saves it for Tcl's internal use. The executable's path name is needed for several purposes in Tcl.  For example, it is needed on some platforms in the implementation of the **load** command. It is also returned by the **info nameofexecutable** command.

The result of **Tcl_FindExecutable** is the full Tcl version with build information (e.g., **9.0.0+abcdef...abcdef.gcc-1002**).

On UNIX platforms this procedure is typically invoked as the very first thing in the application's main program;  it must be passed *argv[0]* as its argument.  It is important not to change the working directory before the invocation. **Tcl_FindExecutable** uses *argv0* along with the **PATH** environment variable to find the application's executable, if possible.  If it fails to find the binary, then future calls to **info nameofexecutable** will return an empty string.

On Windows platforms this procedure is typically invoked as the very first thing in the application's main program as well; Its *argv[0]* argument is only used to indicate whether the executable has a stderr channel (any non-null value) or not (the value null). If **Tcl_SetPanicProc** is never called and no debugger is running, this determines whether the panic message is sent to stderr or to a standard system dialog.

**Tcl_GetNameOfExecutable** simply returns a pointer to the internal full path name of the executable file as computed by **Tcl_FindExecutable**.  This procedure call is the C API equivalent to the **info nameofexecutable** command.  NULL is returned if the internal full path name has not been computed or unknown.

**Tcl_FindExecutable** can not be used in stub-enabled extensions.

