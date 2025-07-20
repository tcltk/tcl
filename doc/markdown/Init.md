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

Tcl_Init - find and source initialization script

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_Init]{.ccmd}[interp]{.cargs}
[const char *]{.ret} [Tcl_SetPreInitScript]{.ccmd}[scriptPtr]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter to initialize. .AP "const char" *scriptPtr in Address of the initialization script. 

# Description

**Tcl_Init** is a helper procedure that finds and **source**s the **init.tcl** script, which should exist somewhere on the Tcl library path.

**Tcl_Init** is typically called from **Tcl_AppInit** procedures.

**Tcl_SetPreInitScript** registers the pre-initialization script and returns the former (now replaced) script pointer. A value of *NULL* may be passed to not register any script. The pre-initialization script is executed by **Tcl_Init** before accessing the file system. The purpose is to typically prepare a custom file system (like an embedded zip-file) to be activated before the search.

When used in stub-enabled embedders, the stubs table must be first initialized using one of **Tcl_InitSubsystems**, **Tcl_SetPanicProc**, **Tcl_FindExecutable** or **TclZipfs_AppHook** before **Tcl_SetPreInitScript** may be called.

