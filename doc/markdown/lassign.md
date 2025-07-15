---
CommandName: lassign
ManualSection: n
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - list(n)
 - lappend(n)
 - ledit(n)
 - lindex(n)
 - linsert(n)
 - llength(n)
 - lmap(n)
 - lpop(n)
 - lrange(n)
 - lremove(n)
 - lrepeat(n)
 - lreplace(n)
 - lreverse(n)
 - lsearch(n)
 - lseq(n)
 - lset(n)
 - lsort(n)
Keywords:
 - assign
 - element
 - list
 - multiple
 - set
 - variable
Copyright:
 - Copyright (c) 1992-1999 Karl Lehenbauer & Mark Diekhans
 - Copyright (c) 2004 Donal K. Fellows
---

# Name

lassign - Assign list elements to variables

# Synopsis

::: {.synopsis} :::
[lassign]{.cmd} [list]{.arg} [varName]{.optdot}
:::

# Description

This command treats the value \fIlist\fR as a list and assigns successive elements from that list to the variables given by the \fIvarName\fR arguments in order.  If there are more variable names than list elements, the remaining variables are set to the empty string.  If there are more list elements than variables, a list of unassigned elements is returned.

# Examples

An illustration of how multiple assignment works, and what happens when there are either too few or too many elements.

```
lassign {a b c} x y z       ;# Empty return
puts $x                     ;# Prints "a"
puts $y                     ;# Prints "b"
puts $z                     ;# Prints "c"

lassign {d e} x y z         ;# Empty return
puts $x                     ;# Prints "d"
puts $y                     ;# Prints "e"
puts $z                     ;# Prints ""

lassign {f g h i} x y       ;# Returns "h i"
puts $x                     ;# Prints "f"
puts $y                     ;# Prints "g"
```

The \fBlassign\fR command has other uses.  It can be used to create the analogue of the "shift" command in many shell languages like this:

```
set ::argv [lassign $::argv argumentToReadOff]
```

