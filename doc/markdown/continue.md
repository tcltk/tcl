---
CommandName: continue
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - break(n)
 - for(n)
 - foreach(n)
 - return(n)
 - while(n)
Keywords:
 - continue
 - iteration
 - loop
Copyright:
 - Copyright (c) 1993-1994 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

continue - Skip to the next iteration of a loop

# Synopsis

::: {.synopsis} :::
[continue]{.cmd}
:::

# Description

This command is typically invoked inside the body of a looping command such as **for** or **foreach** or **while**. It returns a 4 (**TCL_CONTINUE**) result code, which causes a continue exception to occur. The exception causes the current script to be aborted out to the innermost containing loop command, which then continues with the next iteration of the loop. Continue exceptions are also handled in a few other situations, such as the **catch** command and the outermost scripts of procedure bodies.

# Example

Print a line for each of the integers from 0 to 10 *except* 5:

```
for {set x 0} {$x<10} {incr x} {
    if {$x == 5} {
        continue
    }
    puts "x is $x"
}
```

