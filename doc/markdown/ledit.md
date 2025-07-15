---
CommandName: ledit
ManualSection: n
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - list(n)
 - lappend(n)
 - lassign(n)
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
 - string(n)
Keywords:
 - element
 - list
 - replace
Copyright:
 - Copyright (c) 2022 Ashok P. Nadkarni <apnmbx-public@yahoo.com>. All rights reserved.
---

# Name

ledit - Replace elements of a list stored in variable

# Synopsis

::: {.synopsis} :::
[ledit]{.cmd} [listVar]{.arg} [first]{.arg} [last]{.arg} [value value]{.optdot}
:::

# Description

The command fetches the list value in variable \fIlistVar\fR and replaces the elements in the range given by indices \fIfirst\fR to \fIlast\fR (inclusive) with the \fIvalue\fR arguments. The resulting list is then stored back in \fIlistVar\fR and returned as the result of the command.

Arguments \fIfirst\fR and \fIlast\fR are index values specifying the first and last elements of the range to replace. They are interpreted the same as index values for the command \fBstring index\fR, supporting simple index arithmetic and indices relative to the end of the list. The index \fB0\fR refers to the first element of the list, and \fBend\fR refers to the last element of the list. (Unlike with \fBlpop\fR, \fBlset\fR, and \fBlindex\fR, indices into sublists are not supported.)

If either \fIfirst\fR or \fIlast\fR is less than zero, it is considered to refer to the position before the first element of the list. This allows elements to be prepended.

If either \fIfirst\fR or \fIlast\fR indicates a position greater than the index of the last element of the list, it is treated as if it is an index one greater than the last element. This allows elements to be appended.

If \fIlast\fR is less than \fIfirst\fR, then any specified elements will be inserted into the list before the element specified by \fIfirst\fR with no elements being deleted.

The \fIvalue\fR arguments specify zero or more new elements to be added to the list in place of those that were deleted. Each \fIvalue\fR argument will become a separate element of the list.  If no \fIvalue\fR arguments are specified, the elements between \fIfirst\fR and \fIlast\fR are simply deleted.

# Examples

Prepend to a list.

```
set lst {c d e f g}
      \(-> c d e f g
ledit lst -1 -1 a b
      \(-> a b c d e f g
```

Append to the list.

```
ledit lst end+1 end+1 h i
      \(-> a b c d e f g h i
```

Delete third and fourth elements.

```
ledit lst 2 3
      \(-> a b e f g h i
```

Replace two elements with three.

```
ledit lst 2 3 x y z
      \(-> a b x y z g h i
set lst
      \(-> a b x y z g h i
```

