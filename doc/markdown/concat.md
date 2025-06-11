---
CommandName: concat
ManualSection: n
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - append(n)
 - eval(n)
 - join(n)
 - list(n)
Keywords:
 - concatenate
 - join
 - list
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

concat - Join lists together

# Synopsis

::: {.synopsis} :::
[concat]{.cmd} [arg arg]{.optdot}
:::

# Description

This command joins each of its arguments together with spaces after trimming leading and trailing white-space from each of them.  If all of the arguments are lists, this has the same effect as concatenating them into a single list. Arguments that are empty (after trimming) are ignored entirely. It permits any number of arguments; if no *arg*s are supplied, the result is an empty string.

# Examples

Although **concat** will concatenate lists, flattening them in the process (so giving the following interactive session):

```
% concat a b {c d e} {f {g h}}
a b c d e f {g h}
```

it will also concatenate things that are not lists, as can be seen from this session:

```
% concat " a b {c   " d "  e} f"
a b {c d e} f
```

Note also that the concatenation does not remove spaces from the middle of values, as can be seen here:

```
% concat "a   b   c" { d e f }
a   b   c d e f
```

(i.e., there are three spaces between each of the **a**, the **b** and the **c**).

For *true* list concatenation, the **list** command should be used with expansion of each input list:

```
% list {*}"a   b   c" {*}{ d e f }
a b c d e f
```

