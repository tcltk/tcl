---
CommandName: lpop
ManualSection: n
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - list(n)
 - lappend(n)
 - lassign(n)
 - ledit(n)
 - lindex(n)
 - linsert(n)
 - llength(n)
 - lmap(n)
 - lrange(n)
 - lremove(n)
 - lrepeat(n)
 - lreplace(n)
 - lreverse(n)
 - lsearch(n)
 - lseq(n)
 - lset(n)
 - lsort(n)
 - string(n)
Keywords:
 - element
 - index
 - list
 - remove
 - pop
 - stack
 - queue
Copyright:
 - Copyright (c) 2018 Peter Spjuth. All rights reserved.
---

# Name

lpop - Get and remove an element in a list

# Synopsis

::: {.synopsis} :::
[lpop]{.cmd} [varName]{.arg} [?index]{.arg} [...?]{.arg}
:::

# Description

The **lpop** command accepts a parameter, *varName*, which it interprets as the name of a variable containing a Tcl list. It also accepts one or more *indices* into the list. If no indices are presented, it defaults to "**end**".

When presented with a single index, the **lpop** command addresses the *index*'th element in it, removes if from the list and returns the element.

If *index* is negative or greater or equal than the number of elements in the list in the variable called *varName*, an error occurs.

The interpretation of each simple *index* value is the same as for the command **string index**, supporting simple index arithmetic and indices relative to the end of the list.

If additional *index* arguments are supplied, then each argument is used in turn to address an element within a sublist designated by the previous indexing operation, allowing the script to remove elements in sublists, similar to **lindex** and **lset**. The command,

```
lpop a 1 2
```

gets and removes element 2 of sublist 1.

# Examples

In each of these examples, the initial value of *x* is:

```
set x [list [list a b c] [list d e f] [list g h i]]
      \(-> {a b c} {d e f} {g h i}
```

The indicated value becomes the new value of *x* (except in the last case, which is an error which leaves the value of *x* unchanged.)

```
lpop x 0
      \(-> {d e f} {g h i}
lpop x 2
      \(-> {a b c} {d e f}
lpop x end
      \(-> {a b c} {d e f}
lpop x end-1
      \(-> {a b c} {g h i}
lpop x 2 1
      \(-> {a b c} {d e f} {g i}
lpop x 2 3 j
      \(-> list index out of range
```

In the following examples, the initial value of *x* is:

```
set x [list [list [list a b] [list c d]] \
            [list [list e f] [list g h]]]
      \(-> {{a b} {c d}} {{e f} {g h}}
```

The indicated value becomes the new value of *x*.

```
lpop x 1 1 0
      \(-> {{a b} {c d}} {{e f} h}
```

