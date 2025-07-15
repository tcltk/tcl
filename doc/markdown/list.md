---
CommandName: list
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
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
 - lreverse(n)
 - lsearch(n)
 - lseq(n)
 - lset(n)
 - lsort(n)
Keywords:
 - element
 - list
 - quoting
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2001 Kevin B. Kenny <kennykb@acm.org>. All rights reserved.
---

# Name

list - Create a list

# Synopsis

::: {.synopsis} :::
[list]{.cmd} [arg arg]{.optdot}
:::

# Description

This command returns a list comprised of all the \fIarg\fRs, or an empty string if no \fIarg\fRs are specified. Braces and backslashes get added as necessary, so that the \fBlindex\fR command may be used on the result to re-extract the original arguments, and also so that \fBeval\fR may be used to execute the resulting list, with \fIarg1\fR comprising the command's name and the other \fIarg\fRs comprising its arguments.  \fBList\fR produces slightly different results than \fBconcat\fR:  \fBconcat\fR removes one level of grouping before forming the list, while \fBlist\fR works directly from the original arguments.

# Example

The command

```
list a b "c d e  " "  f {g h}"
```

will return

```
a b {c d e  } {  f {g h}}
```

while \fBconcat\fR with the same arguments will return

```
a b c d e f {g h}
```

