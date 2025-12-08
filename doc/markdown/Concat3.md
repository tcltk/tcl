---
CommandName: Tcl_Concat
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_ConcatObj
Keywords:
 - concatenate
 - strings
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_Concat - concatenate a collection of strings

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[const char *]{.ret} [Tcl_Concat]{.ccmd}[argc, argv]{.cargs}
:::

# Arguments

.AP Tcl_Size argc in Number of strings. .AP "const char *const" argv[] in Array of strings to concatenate.  Must have *argc* entries. 

# Description

**Tcl_Concat** is a utility procedure used by several of the Tcl commands.  Given a collection of strings, it concatenates them together into a single string, with the original strings separated by spaces.  This procedure behaves differently than **Tcl_Merge**, in that the arguments are simply concatenated: no effort is made to ensure proper list structure. However, in most common usage the arguments will all be proper lists themselves;  if this is true, then the result will also have proper list structure.

**Tcl_Concat** eliminates leading and trailing white space as it copies strings from **argv** to the result.  If an element of **argv** consists of nothing but white space, then that string is ignored entirely.  This white-space removal was added to make the output of the **concat** command cleaner-looking.

The result string is dynamically allocated using **Tcl_Alloc**;  the caller must eventually release the space by calling **Tcl_Free**.

