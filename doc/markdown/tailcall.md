---
CommandName: tailcall
ManualSection: n
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - apply(n)
 - proc(n)
 - uplevel(n)
Keywords:
 - call
 - recursion
 - tail recursion
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

tailcall - Replace the current procedure with another command

# Synopsis

::: {.synopsis} :::
[tailcall]{.cmd} [command]{.arg} [arg]{.optdot}
:::

# Description

The \fBtailcall\fR command replaces the currently executing procedure, lambda application, or method with another command. The \fIcommand\fR, which will have \fIarg ...\fR passed as arguments if they are supplied, will be looked up in the current namespace context, not in the caller's. Apart from that difference in resolution, it is equivalent to:

```
return [uplevel 1 [list command ?arg ...?]]
```

This command may not be invoked from within an \fBuplevel\fR into a procedure or inside a \fBcatch\fR inside a procedure or lambda.

# Example

Compute the factorial of a number.

```
proc factorial {n {accum 1}} {
    if {$n < 2} {
        return $accum
    }
    tailcall factorial [expr {$n - 1}] [expr {$accum * $n}]
}
```

Print the elements of a list with alternating lines having different indentations.

```
proc printList {theList} {
    if {[llength $theList]} {
        puts "> [lindex $theList 0]"
        tailcall printList2 [lrange $theList 1 end]
    }
}
proc printList2 {theList} {
    if {[llength $theList]} {
        puts "< [lindex $theList 0]"
        tailcall printList [lrange $theList 1 end]
    }
}
```

