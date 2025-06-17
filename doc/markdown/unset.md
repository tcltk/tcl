---
CommandName: unset
ManualSection: n
Version: 8.4
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - set(n)
 - trace(n)
 - upvar(n)
Keywords:
 - remove
 - variable
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2000 Ajuba Solutions.
---

# Name

unset - Delete variables

# Synopsis

::: {.synopsis} :::
[unset]{.cmd} [-nocomplain]{.optlit} [--]{.optlit} [name name name]{.optdot}
:::

# Description

This command removes one or more variables. Each *name* is a variable name, specified in any of the ways acceptable to the **set** command. If a *name* refers to an element of an array then that element is removed without affecting the rest of the array. If a *name* consists of an array name with no parenthesized index, then the entire array is deleted. The **unset** command returns an empty string as result. If **-nocomplain** is specified as the first argument, any possible errors are suppressed.  The option may not be abbreviated, in order to disambiguate it from possible variable names.  The option **--** indicates the end of the options, and should be used if you wish to remove a variable with the same name as any of the options. If an error occurs during variable deletion, any variables after the named one causing the error are not deleted.  An error can occur when the named variable does not exist, or the name refers to an array element but the variable is a scalar, or the name refers to a variable in a non-existent namespace.

# Example

Create an array containing a mapping from some numbers to their squares and remove the array elements for non-prime numbers:

```
array set squares {
    1 1    6 36
    2 4    7 49
    3 9    8 64
    4 16   9 81
    5 25  10 100
}

puts "The squares are:"
parray squares

unset squares(1) squares(4) squares(6)
unset squares(8) squares(9) squares(10)

puts "The prime squares are:"
parray squares
```

