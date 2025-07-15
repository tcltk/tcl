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

The \fBlpop\fR command accepts a parameter, \fIvarName\fR, which it interprets as the name of a variable containing a Tcl list. It also accepts one or more \fIindices\fR into the list. If no indices are presented, it defaults to "\fBend\fR".

When presented with a single index, the \fBlpop\fR command addresses the \fIindex\fR'th element in it, removes if from the list and returns the element.

If \fIindex\fR is negative or greater or equal than the number of elements in the list in the variable called \fIvarName\fR, an error occurs.

The interpretation of each simple \fIindex\fR value is the same as for the command \fBstring index\fR, supporting simple index arithmetic and indices relative to the end of the list.

If additional \fIindex\fR arguments are supplied, then each argument is used in turn to address an element within a sublist designated by the previous indexing operation, allowing the script to remove elements in sublists, similar to \fBlindex\fR and \fBlset\fR. The command,

```
lpop a 1 2
```

gets and removes element 2 of sublist 1.

# Examples

In each of these examples, the initial value of \fIx\fR is:

```
set x [list [list a b c] [list d e f] [list g h i]]
      \(-> {a b c} {d e f} {g h i}
```

The indicated value becomes the new value of \fIx\fR (except in the last case, which is an error which leaves the value of \fIx\fR unchanged.)

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

In the following examples, the initial value of \fIx\fR is:

```
set x [list [list [list a b] [list c d]] \
            [list [list e f] [list g h]]]
      \(-> {{a b} {c d}} {{e f} {g h}}
```

The indicated value becomes the new value of \fIx\fR.

```
lpop x 1 1 0
      \(-> {{a b} {c d}} {{e f} h}
```

