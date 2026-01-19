---
CommandName: Tcl_StringMatch
ManualSection: 3
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - match
 - pattern
 - string
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_StringMatch, Tcl\_StringCaseMatch - test whether a string matches a pattern

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_StringMatch]{.ccmd}[str=, +pattern]{.cargs}
[int]{.ret} [Tcl\_StringCaseMatch]{.ccmd}[str=, +pattern=, +flags]{.cargs}
:::

# Arguments

.AP "const char" \*str in String to test. .AP "const char" \*pattern in Pattern to match against string.  May contain special characters from the set \*?\\[]. .AP int flags in OR-ed combination of match flags, currently only **TCL\_MATCH\_NOCASE**. 0 specifies a case-sensitive search. 

# Description

This utility procedure determines whether a string matches a given pattern.  If it does, then **Tcl\_StringMatch** returns 1.  Otherwise **Tcl\_StringMatch** returns 0.  The algorithm used for matching is the same algorithm used in the [string match][string] Tcl command and is similar to the algorithm used by the C-shell for file name matching;  see the Tcl manual entry for details.

In **Tcl\_StringCaseMatch**, the algorithm is the same, but you have the option to make the matching case-insensitive. If you choose this (by passing **TCL\_MATCH\_NOCASE**), then the string and pattern are essentially matched in the lower case. 


[string]: string.md

