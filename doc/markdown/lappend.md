---
CommandName: lappend
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - list(n)
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
 - append
 - element
 - list
 - variable
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2001 Kevin B. Kenny <kennykb@acm.org>. All rights reserved.
---

# Name

lappend - Append list elements onto a variable

# Synopsis

::: {.synopsis} :::
[lappend]{.cmd} [varName]{.arg} [value value value]{.optdot}
:::

# Description

This command treats the variable given by *varName* as a list and appends each of the *value* arguments to that list as a separate element, with spaces between elements. If *varName* does not exist, it is created as a list with elements given by the *value* arguments.

::: {.info -version="TIP508"}
If *varName* indicate an element that does not exist of an array that has a default value set, list that is comprised of the default value with all the *value* arguments appended as elements will be stored in the array element.
:::

**Lappend** is similar to **append** except that the *value*s are appended as list elements rather than raw text. This command provides a relatively efficient way to build up large lists.  For example, "**lappend a $b**" is much more efficient than "**set a [concat $a [list $b]]**" when **$a** is long.

# Example

Using **lappend** to build up a list of numbers.

```
% set var 1
1
% lappend var 2
1 2
% lappend var 3 4 5
1 2 3 4 5
```

