---
CommandName: join
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - list(n)
 - lappend(n)
 - split(n)
Keywords:
 - element
 - join
 - list
 - separator
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

join - Create a string by joining together list elements

# Synopsis

::: {.synopsis} :::
[join]{.cmd} [list]{.arg} [joinString]{.optarg}
:::

# Description

The *list* argument must be a valid Tcl list. This command returns the string formed by joining all of the elements of *list* together with *joinString* separating each adjacent pair of elements. The *joinString* argument defaults to a space character.

# Examples

Making a comma-separated list:

```
set data {1 2 3 4 5}
join $data ", "
     \(-> 1, 2, 3, 4, 5
```

Using **join** to flatten a list by a single level:

```
set data {1 {2 3} 4 {5 {6 7} 8}}
join $data
     \(-> 1 2 3 4 5 {6 7} 8
```

