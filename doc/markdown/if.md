---
CommandName: if
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - expr(n)
 - for(n)
 - foreach(n)
Keywords:
 - boolean
 - conditional
 - else
 - false
 - if
 - true
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

if - Execute scripts conditionally

# Synopsis

::: {.synopsis} :::
[if]{.cmd} [expr1]{.arg} [then]{.optlit} [body1]{.arg} [elseifClause]{.optdot} [elseClause]{.optarg}
:::

# Description

The \fIif\fR command evaluates \fIexpr1\fR as an expression (in the same way that \fBexpr\fR evaluates its argument).  The result value of the expression must be a boolean (a numeric value, where 0 is false and anything is true, or a string value such as \fBtrue\fR or \fByes\fR for true and \fBfalse\fR or \fBno\fR for false); if it is true then \fIbody1\fR is executed by passing it to the Tcl interpreter. Otherwise, if present, the \fIelseifClause\fR is evaluated. This clause is defined as "\fBelseif\fR \fIexprN\fR ?\fBthen\fR? \fIbodyN\fR" and there may be any number of such clauses following each other with diferent expressions and bodies. \fIexprN\fR is then evaluated as an expression and if it evaluates to true then \fIbodyN\fR is executed, and so on. If none of the expressions evaluates to true then \fIelseClause\fR is executed, if present. The \fIelseClause\fR is defined as "?\fBelse\fR? \fIbodyM\fR". Note, that the \fBthen\fR and \fBelse\fR arguments are optional "syntactic sugar" to make the command easier to read.

The return value from the command is the result of the single body script that was executed, or an empty string if none of the expressions was non-zero and there was no \fIbodyM\fR.

# Examples

A simple conditional:

```
if {$vbl == 1} { puts "vbl is one" }
```

With an \fBelse\fR-clause:

```
if {$vbl == 1} {
    puts "vbl is one"
} else {
    puts "vbl is not one"
}
```

With an \fBelseif\fR-clause too:

```
if {$vbl == 1} {
    puts "vbl is one"
} elseif {$vbl == 2} {
    puts "vbl is two"
} else {
    puts "vbl is not one or two"
}
```

Remember, expressions can be multi-line, but in that case it can be a good idea to use the optional \fBthen\fR keyword for clarity:

```
if {
    $vbl == 1
    || $vbl == 2
    || $vbl == 3
} then {
    puts "vbl is one, two or three"
}
```

