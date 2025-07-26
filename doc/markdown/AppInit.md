---
CommandName: Tcl_AppInit
ManualSection: 3
Version: 7.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_Main(3)
Keywords:
 - application
 - argument
 - command
 - initialization
 - interpreter
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_AppInit - perform application-specific initialization

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_AppInit]{.ccmd}[interp]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter for the application. 

# Description

**Tcl_AppInit** is a "hook" procedure that is invoked by the main programs for Tcl applications such as **tclsh** and **wish**. Its purpose is to allow new Tcl applications to be created without modifying the main programs provided as part of Tcl and Tk. To create a new application you write a new version of **Tcl_AppInit** to replace the default version provided by Tcl, then link your new **Tcl_AppInit** with the Tcl library.

**Tcl_AppInit** is invoked by **Tcl_Main** and **Tk_Main** after their own initialization and before entering the main loop to process commands. Here are some examples of things that **Tcl_AppInit** might do:

1. Call initialization procedures for various packages used by the application. Each initialization procedure adds new commands to *interp* for its package and performs other package-specific initialization.

2. Process command-line arguments, which can be accessed from the Tcl variables **argv** and **argv0** in *interp*.

3. Invoke a startup script to initialize the application.

4. Use the routines **Tcl_SetStartupScript** and **Tcl_GetStartupScript** to set or query the file and encoding that the active **Tcl_Main** or **Tk_Main** routine will use as a startup script. .LP **Tcl_AppInit** returns **TCL_OK** or **TCL_ERROR**. If it returns **TCL_ERROR** then it must leave an error message in for the interpreter's result;  otherwise the result is ignored.


In addition to **Tcl_AppInit**, your application should also contain a procedure **main** that calls **Tcl_Main** as follows:

```
Tcl_Main(argc, argv, Tcl_AppInit);
```

The third argument to **Tcl_Main** gives the address of the application-specific initialization procedure to invoke. This means that you do not have to use the name **Tcl_AppInit** for the procedure, but in practice the name is nearly always **Tcl_AppInit** (in versions before Tcl 7.4 the name **Tcl_AppInit** was implicit;  there was no way to specify the procedure explicitly). The best way to get started is to make a copy of the file **tclAppInit.c** from the Tcl library or source directory. It already contains a **main** procedure and a template for **Tcl_AppInit** that you can modify for your application. 

