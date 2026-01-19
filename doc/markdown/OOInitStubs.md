---
CommandName: Tcl_OOInitStubs
ManualSection: 3
Version: 1.0
TclPart: TclOO
TclDescription: TclOO Library Functions
Keywords:
 - stubs
Links:
 - Tcl_InitStubs(3)
Copyright:
 - Copyright (c) 2012 Donal K. Fellows
---

# Name

Tcl\_OOInitStubs - initialize library access to TclOO functionality

# Synopsis

::: {.synopsis} :::
**#include <tclOO.h>**
[const char \*]{.ret} [Tcl\_OOInitStubs]{.ccmd}[interp]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in The Tcl interpreter that the TclOO API is integrated with and whose C interface is going to be used.

# Description

When an extension library is going to use the C interface exposed by TclOO, it should use **Tcl\_OOInitStubs** to initialize its access to that interface from within its *\****\_Init** (or *\****\_SafeInit**) function, passing in the *interp* that was passed into that routine as context. If the result of calling **Tcl\_OOInitStubs** is NULL, the initialization failed and an error message will have been left in the interpreter's result. Otherwise, the initialization succeeded and the TclOO API may thereafter be used; the version of the TclOO API is returned.

When using this function, either the C #define symbol **USE\_TCLOO\_STUBS** should be defined and your library code linked against the Tcl stub library, or that #define symbol should *not* be defined and your library code linked against the Tcl main library directly.

# Backward compatibility note

If you are linking against the Tcl 8.5 forward compatibility package for TclOO, *only* the stub-enabled configuration is supported and you should also link against the TclOO independent stub library; that library is an integrated part of the main Tcl stub library in Tcl 8.6.

