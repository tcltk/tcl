---
CommandName: incr
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - expr(n)
 - set(n)
Keywords:
 - add
 - increment
 - variable
 - value
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

incr - Increment the value of a variable

# Synopsis

::: {.synopsis} :::
[incr]{.cmd} [varName]{.arg} [increment]{.optarg}
:::

# Description

Increments the value stored in the variable whose name is \fIvarName\fR. The value of the variable must be an integer. If \fIincrement\fR is supplied then its value (which must be an integer) is added to the value of variable \fIvarName\fR;  otherwise 1 is added to \fIvarName\fR. The new value is stored as a decimal string in variable \fIvarName\fR and also returned as result.

Starting with the Tcl 8.5 release, the variable \fIvarName\fR passed to \fBincr\fR may be unset, and in that case, it will be set to the value \fIincrement\fR or to the default increment value of \fB1\fR.

::: {.info -version="TIP508"}
If \fIvarName\fR indicate an element that does not exist of an array that has a default value set, the sum of the default value and the \fIincrement\fR (or 1) will be stored in the array element.
:::

# Examples

Add one to the contents of the variable \fIx\fR:

```
incr x
```

Add 42 to the contents of the variable \fIx\fR:

```
incr x 42
```

Add the contents of the variable \fIy\fR to the contents of the variable \fIx\fR:

```
incr x $y
```

Add nothing at all to the variable \fIx\fR (often useful for checking whether an argument to a procedure is actually integral and generating an error if it is not):

```
incr x 0
```

