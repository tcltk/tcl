---
CommandName: Tcl_CreateFileHandler
ManualSection: 3
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - fileevent(n)
 - Tcl_CreateTimerHandler(3)
 - Tcl_DoWhenIdle(3)
Keywords:
 - callback
 - file
 - handler
Copyright:
 - Copyright (c) 1990-1994 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

Tcl\_CreateFileHandler, Tcl\_DeleteFileHandler - associate procedure callbacks with files or devices (Unix only)

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_CreateFileHandler]{.ccmd}[fd, mask, proc, clientData]{.cargs}
[Tcl\_DeleteFileHandler]{.ccmd}[fd]{.cargs}
:::

# Arguments

.AP int fd in Unix file descriptor for an open file or device. .AP int mask in Conditions under which *proc* should be called: OR-ed combination of **TCL\_READABLE**, **TCL\_WRITABLE**, and **TCL\_EXCEPTION**.  May be set to 0 to temporarily disable a handler. .AP Tcl\_FileProc \*proc in Procedure to invoke whenever the file or device indicated by *file* meets the conditions specified by *mask*. .AP void \*clientData in Arbitrary one-word value to pass to *proc*.

# Description

**Tcl\_CreateFileHandler** arranges for *proc* to be invoked in the future whenever I/O becomes possible on a file or an exceptional condition exists for the file.  The file is indicated by *fd*, and the conditions of interest are indicated by *mask*.  For example, if *mask* is **TCL\_READABLE**, *proc* will be called when the file is readable. The callback to *proc* is made by **Tcl\_DoOneEvent**, so **Tcl\_CreateFileHandler** is only useful in programs that dispatch events through **Tcl\_DoOneEvent** or through Tcl commands such as [vwait].

*Proc* should have arguments and result that match the type **Tcl\_FileProc**:

```
typedef void Tcl_FileProc(
        void *clientData,
        int mask);
```

The *clientData* parameter to *proc* is a copy of the *clientData* argument given to **Tcl\_CreateFileHandler** when the callback was created.  Typically, *clientData* points to a data structure containing application-specific information about the file.  *Mask* is an integer mask indicating which of the requested conditions actually exists for the file;  it will contain a subset of the bits in the *mask* argument to **Tcl\_CreateFileHandler**.

There may exist only one handler for a given file at a given time. If **Tcl\_CreateFileHandler** is called when a handler already exists for *fd*, then the new callback replaces the information that was previously recorded.

**Tcl\_DeleteFileHandler** may be called to delete the file handler for *fd*;  if no handler exists for the file given by *fd* then the procedure has no effect.

The purpose of file handlers is to enable an application to respond to events while waiting for files to become ready for I/O.  For this to work correctly, the application may need to use non-blocking I/O operations on the files for which handlers are declared.  Otherwise the application may block if it reads or writes too much data; while waiting for the I/O to complete the application will not be able to service other events. Use **Tcl\_SetChannelOption** with **-blocking** to set the channel into blocking or nonblocking mode as required.

Note that these interfaces are only supported by the Unix implementation of the Tcl notifier.


[vwait]: vwait.md

