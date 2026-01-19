---
CommandName: Tcl_PrintDouble
ManualSection: 3
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - conversion
 - double-precision
 - floating-point
 - string
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

Tcl\_PrintDouble - Convert floating value to string

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_PrintDouble]{.ccmd}[interp, value, dst]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in This argument is ignored. .AP double value in Floating-point value to be converted. .AP char \*dst out Where to store the string representing *value*.  Must have at least **TCL\_DOUBLE\_SPACE** characters of storage.

# Description

**Tcl\_PrintDouble** generates a string that represents the value of *value* and stores it in memory at the location given by *dst*.  It uses **%g** format to generate the string, with one special twist: the string is guaranteed to contain either a "." or an "e" so that it does not look like an integer.  Where **%g** would generate an integer with no decimal point, **Tcl\_PrintDouble** adds ".0".

The result will have the fewest digits needed to represent the number in such a way that **Tcl\_NewDoubleObj** will generate the same number when presented with the given string. IEEE semantics of rounding to even apply to the conversion.

