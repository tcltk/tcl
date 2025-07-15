---
CommandName: lreplace
ManualSection: n
Version: 7.4
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
 - lreverse(n)
 - lsearch(n)
 - lseq(n)
 - lset(n)
 - lsort(n)
 - string(n)
Keywords:
 - element
 - list
 - replace
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2001 Kevin B. Kenny <kennykb@acm.org>. All rights reserved.
---

# Name

lreplace - Replace elements in a list with new elements

# Synopsis

::: {.synopsis} :::
[lreplace]{.cmd} [list]{.arg} [first]{.arg} [last]{.arg} [element element]{.optdot}
:::

# Description

**lreplace** returns a new list formed by replacing zero or more elements of \fIlist\fR with the \fIelement\fR arguments. \fIfirst\fR and \fIlast\fR are index values specifying the first and last elements of the range to replace. The index values \fIfirst\fR and \fIlast\fR are interpreted the same as index values for the command \fBstring index\fR, supporting simple index arithmetic and indices relative to the end of the list. 0 refers to the first element of the list, and \fBend\fR refers to the last element of the list.

If either \fIfirst\fR or \fIlast\fR is less than zero, it is considered to refer to before the first element of the list. This allows \fBlreplace\fR to prepend elements to \fIlist\fR. If either \fIfirst\fR or \fIlast\fR indicates a position greater than the index of the last element of the list, it is treated as if it is an index one greater than the last element. This allows \fBlreplace\fR to append elements to \fIlist\fR.

If \fIlast\fR is less than \fIfirst\fR, then any specified elements will be inserted into the list before the element specified by \fIfirst\fR with no elements being deleted.

The \fIelement\fR arguments specify zero or more new elements to be added to the list in place of those that were deleted. Each \fIelement\fR argument will become a separate element of the list.  If no \fIelement\fR arguments are specified, then the elements between \fIfirst\fR and \fIlast\fR are simply deleted.

# Examples

Replacing an element of a list with another:

```
% lreplace {a b c d e} 1 1 foo
a foo c d e
```

Replacing two elements of a list with three:

```
% lreplace {a b c d e} 1 2 three more elements
a three more elements d e
```

Deleting the last element from a list in a variable:

```
% set var {a b c d e}
a b c d e
% set var [lreplace $var end end]
a b c d
```

A procedure to delete a given element from a list:

```
proc lremove {listVariable value} {
    upvar 1 $listVariable var
    set idx [lsearch -exact $var $value]
    set var [lreplace $var $idx $idx]
}
```

Appending elements to the list; note that \fBend+2\fR will initially be treated as if it is \fB6\fR here, but both that and \fB12345\fR are greater than the index of the final item so they behave identically:

```
% set var {a b c d e}
a b c d e
% set var [lreplace $var 12345 end+2 f g h i]
a b c d e f g h i
```

