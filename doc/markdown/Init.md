---
CommandName: Tcl_Init
ManualSection: 3
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_AppInit
 - Tcl_Main
Keywords:
 - application
 - initialization
 - interpreter
Copyright:
 - Copyright (c) 1998-2000 Scriptics Corporation.
---

# Name

Tcl\_Init - find and source initialization script

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_Init]{.ccmd}[interp]{.cargs}
[const char \*]{.ret} [Tcl\_SetPreInitScript]{.ccmd}[scriptPtr]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter to initialize. .AP "const char" \*scriptPtr in Address of the initialization script. 

# Description

**Tcl\_Init** is a helper procedure that finds and [source]s the **init.tcl** script, which should exist somewhere on the Tcl library path.

**Tcl\_Init** is typically called from **Tcl\_AppInit** procedures.

**Tcl\_SetPreInitScript** registers the pre-initialization script and returns the former (now replaced) script pointer. A value of *NULL* may be passed to not register any script. The pre-initialization script is executed by **Tcl\_Init** before accessing the file system. The purpose is to typically prepare a custom file system (like an embedded zip-file) to be activated before the search.

When used in stub-enabled embedders, the stubs table must be first initialized using one of **Tcl\_InitSubsystems**, **Tcl\_SetPanicProc**, **Tcl\_FindExecutable** or **TclZipfs\_AppHook** before **Tcl\_SetPreInitScript** may be called.


[source]: source.md

