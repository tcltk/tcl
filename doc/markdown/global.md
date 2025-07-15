---
CommandName: global
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - namespace(n)
 - upvar(n)
 - variable(n)
Keywords:
 - global
 - namespace
 - procedure
 - variable
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

global - Access global variables

# Synopsis

::: {.synopsis} :::
[global]{.cmd} [varname]{.optdot}
:::

# Description

This command has no effect unless executed in the context of a proc body. If the \fBglobal\fR command is executed in the context of a proc body, it creates local variables linked to the corresponding global variables (though these linked variables, like those created by \fBupvar\fR, are not included in the list returned by \fBinfo locals\fR).

If \fIvarname\fR contains namespace qualifiers, the local variable's name is the unqualified name of the global variable, as determined by the \fBnamespace tail\fR command.

*varname* is always treated as the name of a variable, not an array element.  An error is returned if the name looks like an array element, such as \fBa(b)\fR.

# Examples

This procedure sets the namespace variable \fI::a::x\fR

```
proc reset {} {
    global a::x
    set x 0
}
```

This procedure accumulates the strings passed to it in a global buffer, separated by newlines.  It is useful for situations when you want to build a message piece-by-piece (as if with \fBputs\fR) but send that full message in a single piece (e.g. over a connection opened with \fBsocket\fR or as part of a counted HTTP response).

```
proc accum {string} {
    global accumulator
    append accumulator $string \n
}
```

