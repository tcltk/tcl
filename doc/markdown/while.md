---
CommandName: while
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - break(n)
 - continue(n)
 - for(n)
 - foreach(n)
Keywords:
 - boolean
 - loop
 - test
 - while
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

while - Execute script repeatedly as long as a condition is met

# Synopsis

::: {.synopsis} :::
[while]{.cmd} [test]{.arg} [body]{.arg}
:::

# Description

The \fBwhile\fR command evaluates \fItest\fR as an expression (in the same way that \fBexpr\fR evaluates its argument). The value of the expression must a proper boolean value; if it is a true value then \fIbody\fR is executed by passing it to the Tcl interpreter. Once \fIbody\fR has been executed then \fItest\fR is evaluated again, and the process repeats until eventually \fItest\fR evaluates to a false boolean value.  \fBContinue\fR commands may be executed inside \fIbody\fR to terminate the current iteration of the loop, and \fBbreak\fR commands may be executed inside \fIbody\fR to cause immediate termination of the \fBwhile\fR command.  The \fBwhile\fR command always returns an empty string.

Note that \fItest\fR should almost always be enclosed in braces.  If not, variable substitutions will be made before the \fBwhile\fR command starts executing, which means that variable changes made by the loop body will not be considered in the expression. This is likely to result in an infinite loop.  If \fItest\fR is enclosed in braces, variable substitutions are delayed until the expression is evaluated (before each loop iteration), so changes in the variables will be visible. For an example, try the following script with and without the braces around \fB$x<10\fR:

```
set x 0
while {$x<10} {
    puts "x is $x"
    incr x
}
```

# Example

Read lines from a channel until we get to the end of the stream, and print them out with a line-number prepended:

```
set lineCount 0
while {[gets $chan line] >= 0} {
    puts "[incr lineCount]: $line"
}
```

