---
CommandName: lindex
ManualSection: n
Version: 8.4
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - list(n)
 - lappend(n)
 - lassign(n)
 - ledit(n)
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
 - index
 - list
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2001 Kevin B. Kenny <kennykb@acm.org>. All rights reserved.
---

# Name

lindex - Retrieve an element from a list

# Synopsis

::: {.synopsis} :::
[lindex]{.cmd} [list]{.arg} [index]{.optdot}
:::

# Description

The **lindex** command accepts a parameter, *list*, which it treats as a Tcl list. It also accepts zero or more *indices* into the list.  The indices may be presented either consecutively on the command line, or grouped in a Tcl list and presented as a single argument.

If no indices are presented, the command takes the form:

```
lindex list
```

or

```
lindex list {}
```

In this case, the return value of **lindex** is simply the value of the *list* parameter.

When presented with a single index, the **lindex** command treats *list* as a Tcl list and returns the *index*'th element from it (0 refers to the first element of the list). In extracting the element, **lindex** observes the same rules concerning braces and quotes and backslashes as the Tcl command interpreter; however, variable substitution and command substitution do not occur. If *index* is negative or greater than or equal to the number of elements in *value*, then an empty string is returned. The interpretation of each simple *index* value is the same as for the command **string index**, supporting simple index arithmetic and indices relative to the end of the list.

If additional *index* arguments are supplied, then each argument is used in turn to select an element from the previous indexing operation, allowing the script to select elements from sublists.  The command,

```
lindex $a 1 2 3
```

or

```
lindex $a {1 2 3}
```

is synonymous with

```
lindex [lindex [lindex $a 1] 2] 3
```

# Examples

Lists can be indexed into from either end:

```
lindex {a b c} 0
      \(-> a
lindex {a b c} 2
      \(-> c
lindex {a b c} end
      \(-> c
lindex {a b c} end-1
      \(-> b
```

Lists or sequences of indices allow selection into lists of lists:

```
lindex {a b c}
      \(-> a b c
lindex {a b c} {}
      \(-> a b c
lindex {{a b c} {d e f} {g h i}} 2 1
      \(-> h
lindex {{a b c} {d e f} {g h i}} {2 1}
      \(-> h
lindex {{{a b} {c d}} {{e f} {g h}}} 1 1 0
      \(-> g
lindex {{{a b} {c d}} {{e f} {g h}}} {1 1 0}
      \(-> g
```

List indices may also perform limited computation, adding or subtracting fixed amounts from other indices:

```
set idx 1
lindex {a b c d e f} $idx+2
      \(-> d
set idx 3
lindex {a b c d e f} $idx+2
      \(-> f
```

