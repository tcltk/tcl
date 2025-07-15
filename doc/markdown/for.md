---
CommandName: for
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - break(n)
 - continue(n)
 - foreach(n)
 - while(n)
Keywords:
 - boolean
 - for
 - iteration
 - loop
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

for - 'For' loop

# Synopsis

::: {.synopsis} :::
[for]{.cmd} [start]{.arg} [test]{.arg} [next]{.arg} [body]{.arg}
:::

# Description

**For** is a looping command, similar in structure to the C \fBfor\fR statement.  The \fIstart\fR, \fInext\fR, and \fIbody\fR arguments must be Tcl command strings, and \fItest\fR is an expression string. The \fBfor\fR command first invokes the Tcl interpreter to execute \fIstart\fR.  Then it repeatedly evaluates \fItest\fR as an expression; if the result is non-zero it invokes the Tcl interpreter on \fIbody\fR, then invokes the Tcl interpreter on \fInext\fR, then repeats the loop.  The command terminates when \fItest\fR evaluates to 0.  If a \fBcontinue\fR command is invoked within \fIbody\fR then any remaining commands in the current execution of \fIbody\fR are skipped; processing continues by invoking the Tcl interpreter on \fInext\fR, then evaluating \fItest\fR, and so on.  If a \fBbreak\fR command is invoked within \fIbody\fR or \fInext\fR, then the \fBfor\fR command will return immediately. The operation of \fBbreak\fR and \fBcontinue\fR are similar to the corresponding statements in C. \fBFor\fR returns an empty string.

Note that \fItest\fR should almost always be enclosed in braces.  If not, variable substitutions will be made before the \fBfor\fR command starts executing, which means that variable changes made by the loop body will not be considered in the expression. This is likely to result in an infinite loop.  If \fItest\fR is enclosed in braces, variable substitutions are delayed until the expression is evaluated (before each loop iteration), so changes in the variables will be visible. See below for an example:

# Examples

Print a line for each of the integers from 0 to 9:

```
for {set x 0} {$x<10} {incr x} {
    puts "x is $x"
}
```

Either loop infinitely or not at all because the expression being evaluated is actually the constant, or even generate an error!  The actual behaviour will depend on whether the variable \fIx\fR exists before the \fBfor\fR command is run and whether its value is a value that is less than or greater than/equal to ten, and this is because the expression will be substituted before the \fBfor\fR command is executed.

```
for {set x 0} $x<10 {incr x} {
    puts "x is $x"
}
```

Print out the powers of two from 1 to 1024:

```
for {set x 1} {$x<=1024} {set x [expr {$x * 2}]} {
    puts "x is $x"
}
```

