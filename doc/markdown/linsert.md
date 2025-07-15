---
CommandName: linsert
ManualSection: n
Version: 8.2
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - list(n)
 - lappend(n)
 - lassign(n)
 - ledit(n)
 - lindex(n)
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
 - string(n)
Keywords:
 - element
 - insert
 - list
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2001 Kevin B. Kenny <kennykb@acm.org>. All rights reserved.
---

# Name

linsert - Insert elements into a list

# Synopsis

::: {.synopsis} :::
[linsert]{.cmd} [list]{.arg} [index]{.arg} [element element]{.optdot}
:::

# Description

This command produces a new list from \fIlist\fR by inserting all of the \fIelement\fR arguments just before the \fIindex\fR'th element of \fIlist\fR.  Each \fIelement\fR argument will become a separate element of the new list.  If \fIindex\fR is less than or equal to zero, then the new elements are inserted at the beginning of the list, and if \fIindex\fR is greater or equal to the length of \fIlist\fR, it is as if it was \fBend\fR. As with \fBstring index\fR, the \fIindex\fR value supports both simple index arithmetic and end-relative indexing.

Subject to the restrictions that indices must refer to locations inside the list and that the \fIelement\fRs will always be inserted in order, insertions are done so that when \fIindex\fR is start-relative, the first \fIelement\fR will be at that index in the resulting list, and when \fIindex\fR is end-relative, the last \fIelement\fR will be at that index in the resulting list.

# Example

Putting some values into a list, first indexing from the start and then indexing from the end, and then chaining them together:

```
set oldList {the fox jumps over the dog}
set midList [linsert $oldList 1 quick]
set newList [linsert $midList end-1 lazy]
# The old lists still exist though...
set newerList [linsert [linsert $oldList end-1 quick] 1 lazy]
```

