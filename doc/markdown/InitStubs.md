---
CommandName: Tcl_InitStubs
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tk_InitStubs
 - package
Keywords:
 - stubs
Copyright:
 - Copyright (c) 1998-1999 Scriptics Corporation
---

# Name

Tcl_InitStubs - initialize the Tcl stubs mechanism

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[const char *]{.ret} [Tcl_InitStubs]{.ccmd}[interp, version, exact]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Tcl interpreter handle. .AP "const char" *version in A version string, indicating which minimal version of Tcl is accepted. Normally just **"9.0"**. Or **"8.6-"** if both 8.6 and 9.0 are accepted. .AP int exact in 1 means that only the particular version specified by *version* is accepted. 0 means that versions newer than *version* are also accepted. If the*version* ends with **-**, higher major versions are accepted as well, otherwise the major version must be the same as in *version*. Other bits have no effect.

# Introduction

The Tcl stubs mechanism defines a way to dynamically bind extensions to a particular Tcl implementation at run time. This provides two significant benefits to Tcl users:

1. Extensions that use the stubs mechanism can be loaded into multiple versions of Tcl without being recompiled or relinked, as long as the major Tcl version is the same.

2. Extensions that use the stubs mechanism can be dynamically loaded into statically-linked Tcl applications.


The stubs mechanism accomplishes this by exporting function tables that define an interface to the Tcl API.  The extension then accesses the Tcl API through offsets into the function table, so there are no direct references to any of the Tcl library's symbols.  This redirection is transparent to the extension, so an extension writer can continue to use all public Tcl functions as documented.

The stubs mechanism requires no changes to applications incorporating Tcl interpreters.  Only developers creating C-based Tcl extensions need to take steps to use the stubs mechanism with their extensions.

Enabling the stubs mechanism for an extension requires the following steps:

1. Call **Tcl_InitStubs** in the extension before calling any other Tcl functions.

2. Define the **USE_TCL_STUBS** symbol.  Typically, you would include the **-DUSE_TCL_STUBS** flag when compiling the extension.

3. Link the extension with the Tcl stubs library instead of the standard Tcl library.  For example, to use the Tcl 9.0 ABI on Unix platforms, the library name is *libtclstub.a*; on Windows platforms, the library name is *tclstub.lib*.


If the extension also requires the Tk API, it must also call **Tk_InitStubs** to initialize the Tk stubs interface and link with the Tk stubs libraries.  See the **Tk_InitStubs** page for more information.

# Description

**Tcl_InitStubs** attempts to initialize the stub table pointers and ensure that the correct version of Tcl is loaded.  In addition to an interpreter handle, it accepts as arguments a version number and a Boolean flag indicating whether the extension requires an exact version match or not.  If *exact* is 0, then versions newer than *version* are also accepted. If the*version* ends with **-**, higher major versions are accepted as well, otherwise the major version must be the same as in *version*. 1 means that only the specified *version* is accepted. *version* can be any construct as described for **package require** (**PACKAGE** manual page in the section **REQUIREMENT**). Multiple requirement strings like with **package require** are not supported.  **Tcl_InitStubs** returns a string containing the actual version of Tcl satisfying the request, or NULL if the Tcl version is not accepted, does not support stubs, or any other error condition occurred.

