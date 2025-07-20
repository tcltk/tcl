---
CommandName: Tcl_StaticLibrary
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - initialization procedure
 - package
 - static linking
Links:
 - load(n)
 - package(n)
 - Tcl_PkgRequire(3)
Copyright:
 - Copyright (c) 1995-1996 Sun Microsystems, Inc.
---

# Name

Tcl_StaticLibrary, Tcl_StaticPackage - make a statically linked library available via the 'load' command

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_StaticLibrary]{.ccmd}[interp, prefix, initProc, safeInitProc]{.cargs}
[Tcl_StaticPackage]{.ccmd}[interp, prefix, initProc, safeInitProc]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in If not NULL, points to an interpreter into which the library has already been incorporated (i.e., the caller has already invoked the appropriate initialization procedure).  NULL means the library has not yet been incorporated into any interpreter. .AP "const char" *prefix in Prefix for library initialization function. Normally in titlecase (first letter upper-case, all others lower-case), but this is no longer required. .AP Tcl_LibraryInitProc *initProc in Procedure to invoke to incorporate this library into a trusted interpreter. .AP Tcl_LibraryInitProc *safeInitProc in Procedure to call to incorporate this library into a safe interpreter (one that will execute untrusted scripts).  NULL means the library cannot be used in safe interpreters.

# Description

This procedure may be invoked to announce that a library has been linked statically with a Tcl application and, optionally, that it has already been incorporated into an interpreter. Once **Tcl_StaticLibrary** has been invoked for a library, it may be incorporated into interpreters using the **load** command. **Tcl_StaticLibrary** is normally invoked only by the **Tcl_AppInit** procedure for the application, not by libraries for themselves (**Tcl_StaticLibrary** should only be invoked for statically linked libraries, and code in the library itself should not need to know whether the library is dynamically loaded or statically linked).

When the **load** command is used later to incorporate the library into an interpreter, one of *initProc* and *safeInitProc* will be invoked, depending on whether the target interpreter is safe or not. *initProc* and *safeInitProc* must both match the following prototype:

```
typedef int Tcl_LibraryInitProc(
        Tcl_Interp *interp);
```

The *interp* argument identifies the interpreter in which the library is to be incorporated.  The initialization procedure must return **TCL_OK** or **TCL_ERROR** to indicate whether or not it completed successfully; in the event of an error it should set the interpreter's result to point to an error message.  The result or error from the initialization procedure will be returned as the result of the **load** command that caused the initialization procedure to be invoked.

**Tcl_StaticLibrary** was named **Tcl_StaticPackage** in Tcl 8.6 and earlier, but the old name is deprecated now.

**Tcl_StaticLibrary** can not be used in stub-enabled extensions.

