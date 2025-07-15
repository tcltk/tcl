---
CommandName: append
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - concat(n)
 - lappend(n)
Keywords:
 - append
 - variable
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

append - Append to variable

# Synopsis

::: {.synopsis} :::
[append]{.cmd} [varName]{.arg} [value value value]{.optdot}
:::

# Description

Append all of the \fIvalue\fR arguments to the current value of variable \fIvarName\fR.  If \fIvarName\fR does not exist, it is given a value equal to the concatenation of all the \fIvalue\fR arguments.

::: {.info -version="TIP508"}
If \fIvarName\fR indicate an element that does not exist of an array that has a default value set, the concatenation of the default value and all the \fIvalue\fR arguments will be stored in the array element.
:::

The result of this command is the new value stored in variable \fIvarName\fR. This command provides an efficient way to build up long variables incrementally. For example, "**append a $b**" is much more efficient than "**set a $a$b**" if \fB$a\fR is long.

# Example

Building a string of comma-separated numbers piecemeal using a loop.

```
set var 0
for {set i 1} {$i<=10} {incr i} {
    append var "," $i
}
puts $var
# Prints 0,1,2,3,4,5,6,7,8,9,10
```

