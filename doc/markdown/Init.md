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
**typedef int Tcl\_PostInitProc(Tcl\_Interp \*interp, void \*clientData)**
[int]{.ret} [Tcl\_RegisterPostInitProc]{.ccmd}[postInitProc, clientData]{.cargs}
[int]{.ret} [Tcl\_UnregisterPostInitProc]{.ccmd}[postInitProc, clientData]{.cargs}
[int]{.ret} [Tcl\_ClearPostInitProcs]{.ccmd}[void]{.cargs}
:::

# Arguments

::: {.arguments} :::

[\*interp]{.carg .in type="Tcl_Interp"}
: Interpreter to initialize.

[\*scriptPtr]{.carg .in type="const char"}
: Address of the initialization script.

[\*postInitProc]{.carg .in type="Tcl_PostInitProc"}
: Function to call post initialization of interpreter.

[\*clientData]{.carg .in type="void"}
: Arbitrary one-word value to pass to **postInitProc**.


:::

# Description

**Tcl\_Init** is a helper procedure that finds and [source]s the **init.tcl** script, which should exist somewhere on the Tcl library path.

**Tcl\_Init** is typically called from [Tcl\_AppInit][AppInit] procedures.

**Tcl\_SetPreInitScript** registers the pre-initialization script and returns the former (now replaced) script pointer. A value of *NULL* may be passed to not register any script. The pre-initialization script is executed by **Tcl\_Init** before accessing the file system. The purpose is to typically prepare a custom file system (like an embedded zip-file) to be activated before the search.

**Tcl\_RegisterPostInitProc** registers a callback that should be invoked by **Tcl\_Init** at the end of initialization of subsequently created interpreters except safe interpreters. The function returns a Tcl return code.

This callback function is passed the interpreter being initialized and the opaque *clientData* value that was passed at the time of registration. It should return a Tcl return code. Multiple callbacks are supported and will be invoked in the order of their registration except that an error return from a callback will skip the remaining registrations. The Tcl return code from the last callback invoked is returned as the return value from **Tcl\_Init**.

**Tcl\_UnregisterPostInitProc** unregisters a previously registered callback. Both *postInitProc* and *clientData* must match the the corresponding values that were passed to **Tcl\_RegisterPostInitProc**. The function returns a standard Tcl return code. It is not an error if there is no matching prior registration.

The **Tcl\_ClearPostInitProcs** function unregisters all currently registered callbacks.

Callback functions registered through **Tcl\_RegisterPostInitProc** may load static packages, add commands, or set up other facilities so they are available in all interpreters. They cannot however execute any code that may result in new interpreters and should not delete the passed interpreter.

The `Tcl\_RegisterPostInitProc` and `Tcl\_UnregisterPostInitProc` functions may be invoked from within a registered callback. However, the change in registration will not have effect for the interpreter that is being initialized.

When used in stub-enabled embedders, the stubs table must be first initialized using one of [Tcl\_InitSubsystems][InitSubSyst], [Tcl\_SetPanicProc][Panic], [Tcl\_FindExecutable][FindExec] or **TclZipfs\_AppHook** before **Tcl\_SetPreInitScript** may be called.


[AppInit]: AppInit.md
[FindExec]: FindExec.md
[InitSubSyst]: InitSubSyst.md
[Panic]: Panic.md
[source]: source.md

