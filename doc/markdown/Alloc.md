---
CommandName: Tcl_Alloc
ManualSection: 3
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - alloc
 - allocation
 - free
 - malloc
 - memory
 - realloc
 - TCL_MEM_DEBUG
Copyright:
 - Copyright (c) 1995-1996 Sun Microsystems, Inc.
---

# Name

Tcl_Alloc, Tcl_Free, Tcl_Realloc, Tcl_AttemptAlloc, Tcl_AttemptRealloc, Tcl_GetMemoryInfo - allocate or free heap memory

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[void *]{.ret} [Tcl_Alloc]{.ccmd}[size]{.cargs}
[Tcl_Free]{.ccmd}[ptr]{.cargs}
[void *]{.ret} [Tcl_Realloc]{.ccmd}[ptr, size]{.cargs}
[void *]{.ret} [Tcl_AttemptAlloc]{.ccmd}[size]{.cargs}
[void *]{.ret} [Tcl_AttemptRealloc]{.ccmd}[ptr, size]{.cargs}
[Tcl_GetMemoryInfo]{.ccmd}[dsPtr]{.cargs}
:::

# Arguments

.AP "size_t" size in Size in bytes of the memory block to allocate. .AP void *ptr in Pointer to memory block to free or realloc. .AP Tcl_DString *dsPtr in Initialized DString pointer. 

# Description

These procedures provide a platform and compiler independent interface for memory allocation.  Programs that need to transfer ownership of memory blocks between Tcl and other modules should use these routines rather than the native **malloc()** and **free()** routines provided by the C run-time library.

**Tcl_Alloc** returns a pointer to a block of at least *size* bytes suitably aligned for any use.

**Tcl_Free** makes the space referred to by *ptr* available for further allocation.

**Tcl_Realloc** changes the size of the block pointed to by *ptr* to *size* bytes and returns a pointer to the new block. The contents will be unchanged up to the lesser of the new and old sizes.  The returned location may be different from *ptr*.  If *ptr* is NULL, this is equivalent to calling **Tcl_Alloc** with just the *size* argument.

**Tcl_AttemptAlloc** and **Tcl_AttemptRealloc** are identical in function to **Tcl_Alloc** and **Tcl_Realloc**, except that **Tcl_AttemptAlloc** and **Tcl_AttemptRealloc** will not cause the Tcl interpreter to **panic** if the memory allocation fails.  If the allocation fails, these functions will return NULL.  Note that on some platforms, but not all, attempting to allocate a zero-sized block of memory will also cause these functions to return NULL.

When a module or Tcl itself is compiled with **TCL_MEM_DEBUG** defined, the procedures **Tcl_Alloc**, **Tcl_Free**, **Tcl_Realloc**, **Tcl_AttemptAlloc**, and **Tcl_AttempRealloc** are implemented as macros, redefined to be special debugging versions of these procedures.

**Tcl_GetMemoryInfo** appends a list-of-lists of memory stats to the provided DString. This function cannot be used in stub-enabled extensions, and it is only available if Tcl is compiled with the threaded memory allocator When used in stub-enabled embedders, the stubs table must be first initialized using one of **Tcl_InitSubsystems**, **Tcl_SetPanicProc**, **Tcl_FindExecutable** or **TclZipfs_AppHook**. 

