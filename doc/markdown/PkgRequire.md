---
CommandName: Tcl_PkgRequire
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - package
 - present
 - provide
 - require
 - version
Links:
 - package(n)
 - Tcl_StaticLibrary(3)
Copyright:
 - Copyright (c) 1996 Sun Microsystems, Inc.
---

# Name

Tcl_PkgRequire, Tcl_PkgRequireEx, Tcl_PkgRequireProc, Tcl_PkgPresent, Tcl_PkgPresentEx, Tcl_PkgProvide, Tcl_PkgProvideEx - package version control

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[const char *]{.ret} [Tcl_PkgRequire]{.ccmd}[interp, name, version, exact]{.cargs}
[const char *]{.ret} [Tcl_PkgRequireEx]{.ccmd}[interp, name, version, exact, clientDataPtr]{.cargs}
[int]{.ret} [Tcl_PkgRequireProc]{.ccmd}[interp, name, objc, objv, clientDataPtr]{.cargs}
[const char *]{.ret} [Tcl_PkgPresent]{.ccmd}[interp, name, version, exact]{.cargs}
[const char *]{.ret} [Tcl_PkgPresentEx]{.ccmd}[interp, name, version, exact, clientDataPtr]{.cargs}
[int]{.ret} [Tcl_PkgProvide]{.ccmd}[interp, name, version]{.cargs}
[int]{.ret} [Tcl_PkgProvideEx]{.ccmd}[interp, name, version, clientData]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter where package is needed or available. .AP "const char" *name in Name of package. .AP "const char" *version in A version specification string as described for **package require**. .AP int exact in Non-zero means that only the particular version specified by *version* is acceptable. Zero means that newer versions than *version* are also acceptable as long as they have the same major version number as *version*. .AP "const void" *clientData in Arbitrary value to be associated with the package. .AP void *clientDataPtr out Pointer to place to store the value associated with the matching package. It is only changed if the pointer is not NULL and the function completed successfully. The storage can be any pointer type with the same size as a void pointer. .AP Tcl_Size objc in Number of requirements. .AP Tcl_Obj* objv[] in Array of requirements.

# Description

These procedures provide C-level interfaces to Tcl's package and version management facilities.

**Tcl_PkgRequire** is equivalent to the **package require** command, **Tcl_PkgPresent** is equivalent to the **package present** command, and **Tcl_PkgProvide** is equivalent to the **package provide** command.

See the documentation for the Tcl commands for details on what these procedures do.

If **Tcl_PkgPresent** or **Tcl_PkgRequire** complete successfully they return a pointer to the version string for the version of the package that is provided in the interpreter (which may be different than *version*); if an error occurs they return NULL and leave an error message in the interpreter's result.

**Tcl_PkgProvide** returns **TCL_OK** if it completes successfully; if an error occurs it returns **TCL_ERROR** and leaves an error message in the interpreter's result.

**Tcl_PkgProvideEx**, **Tcl_PkgPresentEx** and **Tcl_PkgRequireEx** allow the setting and retrieving of the client data associated with the package. In all other respects they are equivalent to the matching functions.

**Tcl_PkgRequireProc** is the form of **package require** handling multiple requirements. The other forms are present for backward compatibility and translate their invocations to this form.

# Reference count management

The requirements values given (in the *objv* argument) to **Tcl_PkgRequireProc** must have non-zero reference counts.

