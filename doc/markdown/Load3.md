---
CommandName: Load
ManualSection: 3
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_FSLoadFile(3)
 - Tcl_FSUnloadFile(3)
 - load(n)
 - unload(n)
Keywords:
 - binary code
 - loading
 - shared library
Copyright:
 - Copyright (c) 2009-2010 Kevin B. Kenny
 - Copyright (c) 2010 Donal K. Fellows
---

# Name

Tcl_LoadFile, Tcl_FindSymbol - platform-independent dynamic library loading

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_LoadFile]{.ccmd}[interp, pathPtr, symbols, flags, procPtrs, loadHandlePtr]{.cargs}
[void *]{.ret} [Tcl_FindSymbol]{.ccmd}[interp, loadHandle, symbol]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter to use for reporting error messages. .AP Tcl_Obj *pathPtr in The name of the file to load. If it is a single name, the library search path of the current environment will be used to resolve it. .AP "const char *const" symbols[] in Array of names of symbols to be resolved during the load of the library, or NULL if no symbols are to be resolved. If an array is given, the last entry in the array must be NULL. .AP int flags in The value should normally be 0, but *TCL_LOAD_GLOBAL* or *TCL_LOAD_LAZY* or a combination of those two is allowed as well. .AP void *procPtrs out Points to an array that will hold the addresses of the functions described in the *symbols* argument. Should be NULL if no symbols are to be resolved. .AP Tcl_LoadHandle *loadHandlePtr out Points to a variable that will hold the handle to the abstract token describing the library that has been loaded. .AP Tcl_LoadHandle loadHandle in Abstract token describing the library to look up a symbol in. .AP "const char" *symbol in The name of the symbol to look up.

# Description

**Tcl_LoadFile** loads a file from the filesystem (including potentially any virtual filesystem that has been installed) and provides a handle to it that may be used in further operations. The *symbols* array, if non-NULL, supplies a set of names of symbols (typically functions) that must be resolved from the library and which will be stored in the array indicated by *procPtrs*. If any of the symbols is not resolved, the loading of the file will fail with an error message left in the interpreter (if that is non-NULL). The result of **Tcl_LoadFile** is a standard Tcl error code. The library may be unloaded with **Tcl_FSUnloadFile**.

**Tcl_FindSymbol** locates a symbol in a loaded library and returns it. If the symbol cannot be found, it returns NULL and sets an error message in the given *interp* (if that is non-NULL). Note that it is unsafe to use this operation on a handle that has been passed to **Tcl_FSUnloadFile**.

# Reference count management

The reference count of the *pathPtr* argument to **Tcl_LoadFile** may be incremented. As such, it should not be given a zero reference count value.

