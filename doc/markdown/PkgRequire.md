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

Tcl\_PkgRequire, Tcl\_PkgRequireEx, Tcl\_PkgRequireProc, Tcl\_PkgPresent, Tcl\_PkgPresentEx, Tcl\_PkgProvide, Tcl\_PkgProvideEx - package version control

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[const char \*]{.ret} [Tcl\_PkgRequire]{.ccmd}[interp, name, version, exact]{.cargs}
[const char \*]{.ret} [Tcl\_PkgRequireEx]{.ccmd}[interp, name, version, exact, clientDataPtr]{.cargs}
[int]{.ret} [Tcl\_PkgRequireProc]{.ccmd}[interp, name, objc, objv, clientDataPtr]{.cargs}
[const char \*]{.ret} [Tcl\_PkgPresent]{.ccmd}[interp, name, version, exact]{.cargs}
[const char \*]{.ret} [Tcl\_PkgPresentEx]{.ccmd}[interp, name, version, exact, clientDataPtr]{.cargs}
[int]{.ret} [Tcl\_PkgProvide]{.ccmd}[interp, name, version]{.cargs}
[int]{.ret} [Tcl\_PkgProvideEx]{.ccmd}[interp, name, version, clientData]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter where package is needed or available. .AP "const char" \*name in Name of package. .AP "const char" \*version in A version specification string as described for [package require][package]. .AP int exact in Non-zero means that only the particular version specified by *version* is acceptable. Zero means that newer versions than *version* are also acceptable as long as they have the same major version number as *version*. .AP "const void" \*clientData in Arbitrary value to be associated with the package. .AP void \*clientDataPtr out Pointer to place to store the value associated with the matching package. It is only changed if the pointer is not NULL and the function completed successfully. The storage can be any pointer type with the same size as a void pointer. .AP Tcl\_Size objc in Number of requirements. .AP Tcl\_Obj\* objv[] in Array of requirements.

# Description

These procedures provide C-level interfaces to Tcl's package and version management facilities.

**Tcl\_PkgRequire** is equivalent to the [package require][package] command, **Tcl\_PkgPresent** is equivalent to the [package present][package] command, and **Tcl\_PkgProvide** is equivalent to the [package provide][package] command.

See the documentation for the Tcl commands for details on what these procedures do.

If **Tcl\_PkgPresent** or **Tcl\_PkgRequire** complete successfully they return a pointer to the version string for the version of the package that is provided in the interpreter (which may be different than *version*); if an error occurs they return NULL and leave an error message in the interpreter's result.

**Tcl\_PkgProvide** returns **TCL\_OK** if it completes successfully; if an error occurs it returns **TCL\_ERROR** and leaves an error message in the interpreter's result.

**Tcl\_PkgProvideEx**, **Tcl\_PkgPresentEx** and **Tcl\_PkgRequireEx** allow the setting and retrieving of the client data associated with the package. In all other respects they are equivalent to the matching functions.

**Tcl\_PkgRequireProc** is the form of [package require][package] handling multiple requirements. The other forms are present for backward compatibility and translate their invocations to this form.

# Reference count management

The requirements values given (in the *objv* argument) to **Tcl\_PkgRequireProc** must have non-zero reference counts.


[package]: package.md

