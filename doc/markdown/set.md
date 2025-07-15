---
CommandName: set
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - expr(n)
 - global(n)
 - namespace(n)
 - proc(n)
 - trace(n)
 - unset(n)
 - upvar(n)
 - variable(n)
Keywords:
 - read
 - write
 - variable
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

set - Read and write variables

# Synopsis

::: {.synopsis} :::
[set]{.cmd} [varName]{.arg} [value]{.optarg}
:::

# Description

Returns the value of variable \fIvarName\fR. If \fIvalue\fR is specified, then set the value of \fIvarName\fR to \fIvalue\fR, creating a new variable if one does not already exist, and return its value. If \fIvarName\fR contains an open parenthesis and ends with a close parenthesis, then it refers to an array element:  the characters before the first open parenthesis are the name of the array, and the characters between the parentheses are the index within the array. Otherwise \fIvarName\fR refers to a scalar variable.

If \fIvarName\fR includes namespace qualifiers (in the array name if it refers to an array element), or if \fIvarName\fR is unqualified (does not include the names of any containing namespaces) but no procedure is active, \fIvarName\fR refers to a namespace variable resolved according to the rules described under \fBNAME RESOLUTION\fR in the \fBnamespace\fR manual page.

If a procedure is active and \fIvarName\fR is unqualified, then \fIvarName\fR refers to a parameter or local variable of the procedure, unless \fIvarName\fR was declared to resolve differently through one of the \fBglobal\fR, \fBvariable\fR or \fBupvar\fR commands.

# Examples

Store a random number in the variable \fIr\fR:

```
set r [expr {rand()}]
```

Store a short message in an array element:

```
set anAry(msg) "Hello, World!"
```

Store a short message in an array element specified by a variable:

```
set elemName "msg"
set anAry($elemName) "Hello, World!"
```

Copy a value into the variable \fIout\fR from a variable whose name is stored in the \fIvbl\fR (note that it is often easier to use arrays in practice instead of doing double-dereferencing):

```
set in0 "small random"
set in1 "large random"
set vbl in[expr {rand() >= 0.5}]
set out [set $vbl]
```

