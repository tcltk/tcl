---
CommandName: lset
ManualSection: n
Version: 8.4
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
 - lreverse(n)
 - lsearch(n)
 - lseq(n)
 - lsort(n)
 - string(n)
Keywords:
 - element
 - index
 - list
 - replace
 - set
Copyright:
 - Copyright (c) 2001 Kevin B. Kenny <kennykb@acm.org>. All rights reserved.
---

# Name

lset - Change an element in a list

# Synopsis

::: {.synopsis} :::
[lset]{.cmd} [varName]{.arg} [?index]{.arg} [...?]{.arg} [newValue]{.arg}
:::

# Description

The \fBlset\fR command accepts a parameter, \fIvarName\fR, which it interprets as the name of a variable containing a Tcl list. It also accepts zero or more \fIindices\fR into the list.  The indices may be presented either consecutively on the command line, or grouped in a Tcl list and presented as a single argument. Finally, it accepts a new value for an element of \fIvarName\fR.

If no indices are presented, the command takes the form:

```
lset varName newValue
```

or

```
lset varName {} newValue
```

In this case, \fInewValue\fR replaces the old value of the variable \fIvarName\fR.

When presented with a single index, the \fBlset\fR command treats the content of the \fIvarName\fR variable as a Tcl list. It addresses the \fIindex\fR'th element in it (0 refers to the first element of the list). When interpreting the list, \fBlset\fR observes the same rules concerning braces and quotes and backslashes as the Tcl command interpreter; however, variable substitution and command substitution do not occur. The command constructs a new list in which the designated element is replaced with \fInewValue\fR.  This new list is stored in the variable \fIvarName\fR, and is also the return value from the \fBlset\fR command.

If \fIindex\fR is negative or greater than the number of elements in \fI$varName\fR, then an error occurs.

If \fIindex\fR is equal to the number of elements in \fI$varName\fR, then the given element is appended to the list.

The interpretation of each simple \fIindex\fR value is the same as for the command \fBstring index\fR, supporting simple index arithmetic and indices relative to the end of the list.

If additional \fIindex\fR arguments are supplied, then each argument is used in turn to address an element within a sublist designated by the previous indexing operation, allowing the script to alter elements in sublists (or append elements to sublists).  The command,

```
lset a 1 2 newValue
```

or

```
lset a {1 2} newValue
```

replaces element 2 of sublist 1 with \fInewValue\fR.

The integer appearing in each \fIindex\fR argument must be greater than or equal to zero.  The integer appearing in each \fIindex\fR argument must be less than or equal to the length of the corresponding list.  In other words, the \fBlset\fR command can change the size of a list only by appending an element (setting the one after the current end).  If an index is outside the permitted range, an error is reported.

# Examples

In each of these examples, the initial value of \fIx\fR is:

```
set x [list [list a b c] [list d e f] [list g h i]]
      \(-> {a b c} {d e f} {g h i}
```

The indicated return value also becomes the new value of \fIx\fR (except in the last case, which is an error which leaves the value of \fIx\fR unchanged.)

```
lset x {j k l}
      \(-> j k l
lset x {} {j k l}
      \(-> j k l
lset x 0 j
      \(-> j {d e f} {g h i}
lset x 2 j
      \(-> {a b c} {d e f} j
lset x end j
      \(-> {a b c} {d e f} j
lset x end-1 j
      \(-> {a b c} j {g h i}
lset x 2 1 j
      \(-> {a b c} {d e f} {g j i}
lset x {2 1} j
      \(-> {a b c} {d e f} {g j i}
lset x {2 3} j
      \(-> {a b c} {d e f} {g h i j}
lset x {2 4} j
      \(-> list index out of range
```

In the following examples, the initial value of \fIx\fR is:

```
set x [list [list [list a b] [list c d]] \
            [list [list e f] [list g h]]]
      \(-> {{a b} {c d}} {{e f} {g h}}
```

The indicated return value also becomes the new value of \fIx\fR.

```
lset x 1 1 0 j
      \(-> {{a b} {c d}} {{e f} {j h}}
lset x {1 1 0} j
      \(-> {{a b} {c d}} {{e f} {j h}}
```

