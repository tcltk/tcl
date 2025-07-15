---
CommandName: llength
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - list(n)
 - lappend(n)
 - lassign(n)
 - ledit(n)
 - lindex(n)
 - linsert(n)
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
 - length
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2001 Kevin B. Kenny <kennykb@acm.org>. All rights reserved.
---

# Name

llength - Count the number of elements in a list

# Synopsis

::: {.synopsis} :::
[llength]{.cmd} [list]{.arg}
:::

# Description

Treats \fIlist\fR as a list and returns a decimal string giving the number of elements in it.

# Examples

The result is the number of elements:

```
% llength {a b c d e}
5
% llength {a b c}
3
% llength {}
0
```

Elements are not guaranteed to be exactly words in a dictionary sense of course, especially when quoting is used:

```
% llength {a b {c d} e}
4
% llength {a b { } c d e}
6
```

An empty list is not necessarily an empty string:

```
% set var { }; puts "[string length $var],[llength $var]"
1,0
```

