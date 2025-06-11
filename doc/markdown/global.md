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

This command has no effect unless executed in the context of a proc body. If the **global** command is executed in the context of a proc body, it creates local variables linked to the corresponding global variables (though these linked variables, like those created by **upvar**, are not included in the list returned by **info locals**).

If *varname* contains namespace qualifiers, the local variable's name is the unqualified name of the global variable, as determined by the **namespace tail** command.

*varname* is always treated as the name of a variable, not an array element.  An error is returned if the name looks like an array element, such as **a(b)**.

# Examples

This procedure sets the namespace variable *::a::x*

```
proc reset {} {
    global a::x
    set x 0
}
```

This procedure accumulates the strings passed to it in a global buffer, separated by newlines.  It is useful for situations when you want to build a message piece-by-piece (as if with **puts**) but send that full message in a single piece (e.g. over a connection opened with **socket** or as part of a counted HTTP response).

```
proc accum {string} {
    global accumulator
    append accumulator $string \n
}
```

