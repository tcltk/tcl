---
CommandName: Tcl_DumpActiveMemory
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - TCL_MEM_DEBUG
 - memory
Keywords:
 - memory
 - debug
Copyright:
 - Copyright (c) 1992-1999 Karl Lehenbauer & Mark Diekhans.
 - Copyright (c) 2000 Scriptics Corporation.
---

# Name

Tcl\_DumpActiveMemory, Tcl\_InitMemory, Tcl\_ValidateAllMemory - Validated memory allocation interface

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_DumpActiveMemory]{.ccmd}[fileName]{.cargs}
[Tcl\_InitMemory]{.ccmd}[interp]{.cargs}
[Tcl\_ValidateAllMemory]{.ccmd}[fileName, line]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Tcl interpreter in which to add commands. .AP "const char" \*fileName in For **Tcl\_DumpActiveMemory**, name of the file to which memory information will be written.  For **Tcl\_ValidateAllMemory**, name of the file from which the call is being made (normally **\_\_FILE\_\_**). .AP int line in Line number at which the call to **Tcl\_ValidateAllMemory** is made (normally **\_\_LINE\_\_**). 

# Description

These functions provide access to Tcl memory debugging information. They are only functional when Tcl has been compiled with **TCL\_MEM\_DEBUG** defined at compile-time.  When **TCL\_MEM\_DEBUG** is not defined, these functions are all no-ops.

**Tcl\_DumpActiveMemory** will output a list of all currently allocated memory to the specified file.  The information output for each allocated block of memory is:  starting and ending addresses (excluding guard zone), size, source file where **Tcl\_Alloc** was called to allocate the block and line number in that file.  It is especially useful to call **Tcl\_DumpActiveMemory** after the Tcl interpreter has been deleted.

**Tcl\_InitMemory** adds the Tcl **memory** command to the interpreter given by *interp*.  **Tcl\_InitMemory** is called by **Tcl\_Main**.

**Tcl\_ValidateAllMemory** forces a validation of the guard zones of all currently allocated blocks of memory.  Normally validation of a block occurs when its freed, unless full validation is enabled, in which case validation of all blocks occurs when **Tcl\_Alloc** and **Tcl\_Free** are called.  This function forces the validation to occur at any point. 

