---
CommandName: lrepeat
ManualSection: n
Version: 8.5
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
 - lpop(n)
 - lrange(n)
 - lremove(n)
 - lreplace(n)
 - lreverse(n)
 - lsearch(n)
 - lseq(n)
 - lset(n)
 - lsort(n)
Keywords:
 - element
 - index
 - list
Copyright:
 - Copyright (c) 2003 Simon Geard. All rights reserved.
---

# Name

lrepeat - Build a list by repeating elements

# Synopsis

::: {.synopsis} :::
[lrepeat]{.cmd} [count]{.arg} [element]{.optdot}
:::

# Description

The \fBlrepeat\fR command creates a list of size \fIcount * number of elements\fR by repeating \fIcount\fR times the sequence of elements \fIelement ...\fR.  \fIcount\fR must be a non-negative integer, \fIelement\fR can be any Tcl value.

Note that \fBlrepeat 1 element ...\fR is identical to \fBlist element ...\fR.

# Examples

```
lrepeat 3 a
      \(-> a a a
lrepeat 3 [lrepeat 3 0]
      \(-> {0 0 0} {0 0 0} {0 0 0}
lrepeat 3 a b c
      \(-> a b c a b c a b c
lrepeat 3 [lrepeat 2 a] b c
      \(-> {a a} b c {a a} b c {a a} b c
```

