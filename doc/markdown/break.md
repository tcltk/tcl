---
CommandName: break
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - catch(n)
 - continue(n)
 - for(n)
 - foreach(n)
 - return(n)
 - while(n)
Keywords:
 - abort
 - break
 - loop
Copyright:
 - Copyright (c) 1993-1994 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

break - Abort looping command

# Synopsis

::: {.synopsis} :::
[break]{.cmd}
:::

# Description

This command is typically invoked inside the body of a looping command such as **for** or **foreach** or **while**. It returns a 3 (**TCL_BREAK**) result code, which causes a break exception to occur. The exception causes the current script to be aborted out to the innermost containing loop command, which then aborts its execution and returns normally. Break exceptions are also handled in a few other situations, such as the **catch** command, Tk event bindings, and the outermost scripts of procedure bodies.

# Example

Print a line for each of the integers from 0 to 5:

```
for {set x 0} {$x<10} {incr x} {
    if {$x > 5} {
        break
    }
    puts "x is $x"
}
```

