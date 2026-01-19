---
CommandName: Tcl_SetErrno
ManualSection: 3
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - errno
 - error code
 - global variables
Copyright:
 - Copyright (c) 1996 Sun Microsystems, Inc.
---

# Name

Tcl\_SetErrno, Tcl\_GetErrno, Tcl\_ErrnoId, Tcl\_ErrnoMsg, Tcl\_WinConvertError - manipulate errno to store and retrieve error codes

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[void]{.ret} [Tcl\_SetErrno]{.ccmd}[errorCode]{.cargs}
[int]{.ret} [Tcl\_GetErrno]{.ccmd}[]{.cargs}
[const char \*]{.ret} [Tcl\_ErrnoId]{.ccmd}[]{.cargs}
[const char \*]{.ret} [Tcl\_ErrnoMsg]{.ccmd}[errorCode]{.cargs}
[void]{.ret} [Tcl\_WinConvertError]{.ccmd}[winErrorCode]{.cargs}
:::

# Arguments

.AP int errorCode in A POSIX error code such as **ENOENT**. .AP DWORD winErrorCode in A Windows or Winsock error code such as **ERROR\_FILE\_NOT\_FOUND**. 

# Description

**Tcl\_SetErrno** and **Tcl\_GetErrno** provide portable access to the **errno** variable, which is used to record a POSIX error code after system calls and other operations such as **Tcl\_Gets**. These procedures are necessary because global variable accesses cannot be made across module boundaries on some platforms.

**Tcl\_SetErrno** sets the **errno** variable to the value of the *errorCode* argument C procedures that wish to return error information to their callers via **errno** should call **Tcl\_SetErrno** rather than setting **errno** directly.

**Tcl\_GetErrno** returns the current value of **errno**. Procedures wishing to access **errno** should call this procedure instead of accessing **errno** directly.

**Tcl\_ErrnoId** and **Tcl\_ErrnoMsg** return string representations of **errno** values.  **Tcl\_ErrnoId** returns a machine-readable textual identifier such as "EACCES" that corresponds to the current value of **errno**. **Tcl\_ErrnoMsg** returns a human-readable string such as "permission denied" that corresponds to the value of its *errorCode* argument.  The *errorCode* argument is typically the value returned by **Tcl\_GetErrno**. The strings returned by these functions are statically allocated and the caller must not free or modify them.

**Tcl\_WinConvertError** (Windows only) maps the passed Windows or Winsock error code to a POSIX error and stores it in **errno**. 

