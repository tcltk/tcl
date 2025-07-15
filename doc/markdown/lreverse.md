---
CommandName: lreverse
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
 - lrepeat(n)
 - lreplace(n)
 - lsearch(n)
 - lseq(n)
 - lset(n)
 - lsort(n)
Keywords:
 - element
 - list
 - reverse
Copyright:
 - Copyright (c) 2006 Donal K. Fellows. All rights reserved.
---

# Name

lreverse - Reverse the order of a list

# Synopsis

::: {.synopsis} :::
[lreverse]{.cmd} [list]{.arg}
:::

# Description

The \fBlreverse\fR command returns a list that has the same elements as its input list, \fIlist\fR, except with the elements in the reverse order.

# Examples

```
lreverse {a a b c}
      \(-> c b a a
lreverse {a b {c d} e f}
      \(-> f e {c d} b a
```

