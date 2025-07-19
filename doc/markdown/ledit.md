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

The command fetches the list value in variable *listVar* and replaces the elements in the range given by indices *first* to *last* (inclusive) with the *value* arguments. The resulting list is then stored back in *listVar* and returned as the result of the command.

Arguments *first* and *last* are index values specifying the first and last elements of the range to replace. They are interpreted the same as index values for the command **string index**, supporting simple index arithmetic and indices relative to the end of the list. The index **0** refers to the first element of the list, and **end** refers to the last element of the list. (Unlike with **lpop**, **lset**, and **lindex**, indices into sublists are not supported.)

If either *first* or *last* is less than zero, it is considered to refer to the position before the first element of the list. This allows elements to be prepended.

If either *first* or *last* indicates a position greater than the index of the last element of the list, it is treated as if it is an index one greater than the last element. This allows elements to be appended.

If *last* is less than *first*, then any specified elements will be inserted into the list before the element specified by *first* with no elements being deleted.

The *value* arguments specify zero or more new elements to be added to the list in place of those that were deleted. Each *value* argument will become a separate element of the list.  If no *value* arguments are specified, the elements between *first* and *last* are simply deleted.

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

