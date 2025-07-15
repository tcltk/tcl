---
CommandName: upvar
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - global(n)
 - namespace(n)
 - uplevel(n)
 - variable(n)
Keywords:
 - context
 - frame
 - global
 - level
 - namespace
 - procedure
 - upvar
 - variable
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

upvar - Create link to variable in a different stack frame

# Synopsis

::: {.synopsis} :::
[upvar]{.cmd} [level]{.optarg} [otherVar]{.arg} [myVar]{.arg} [otherVar myVar]{.optdot}
:::

# Description

This command arranges for one or more local variables in the current procedure to refer to variables in an enclosing procedure call or to global variables. \fILevel\fR may have any of the forms permitted for the \fBuplevel\fR command, and may be omitted (it defaults to \fB1\fR). For each \fIotherVar\fR argument, \fBupvar\fR makes the variable by that name in the procedure frame given by \fIlevel\fR (or at global level, if \fIlevel\fR is \fB#0\fR) accessible in the current procedure by the name given in the corresponding \fImyVar\fR argument. The variable named by \fIotherVar\fR need not exist at the time of the call;  it will be created the first time \fImyVar\fR is referenced, just like an ordinary variable.  There must not exist a variable by the name \fImyVar\fR at the time \fBupvar\fR is invoked. \fIMyVar\fR is always treated as the name of a variable, not an array element.  An error is returned if the name looks like an array element, such as \fBa(b)\fR. \fIOtherVar\fR may refer to a scalar variable, an array, or an array element. \fBUpvar\fR returns an empty string.

The \fBupvar\fR command simplifies the implementation of call-by-name procedure calling and also makes it easier to build new control constructs as Tcl procedures. For example, consider the following procedure:

```
proc add2 name {
    upvar $name x
    set x [expr {$x + 2}]
}
```

If \fIadd2\fR is invoked with an argument giving the name of a variable, it adds two to the value of that variable. Although \fIadd2\fR could have been implemented using \fBuplevel\fR instead of \fBupvar\fR, \fBupvar\fR makes it simpler for \fIadd2\fR to access the variable in the caller's procedure frame.

**namespace eval** is another way (besides procedure calls) that the Tcl naming context can change. It adds a call frame to the stack to represent the namespace context. This means each \fBnamespace eval\fR command counts as another call level for \fBuplevel\fR and \fBupvar\fR commands. For example, \fBinfo level\fR \fB1\fR will return a list describing a command that is either the outermost procedure call or the outermost \fBnamespace eval\fR command. Also, \fBuplevel #0\fR evaluates a script at top-level in the outermost namespace (the global namespace).

If an upvar variable is unset (e.g. \fBx\fR in \fBadd2\fR above), the \fBunset\fR operation affects the variable it is linked to, not the upvar variable.  There is no way to unset an upvar variable except by exiting the procedure in which it is defined.  However, it is possible to retarget an upvar variable by executing another \fBupvar\fR command.

# Traces and upvar

Upvar interacts with traces in a straightforward but possibly unexpected manner.  If a variable trace is defined on \fIotherVar\fR, that trace will be triggered by actions involving \fImyVar\fR.  However, the trace procedure will be passed the name of \fImyVar\fR, rather than the name of \fIotherVar\fR.  Thus, the output of the following code will be "*localVar*" rather than "*originalVar*":

```
proc traceproc { name index op } {
    puts $name
}
proc setByUpvar { name value } {
    upvar $name localVar
    set localVar $value
}
set originalVar 1
trace add variable originalVar write traceproc
setByUpvar originalVar 2
```

If \fIotherVar\fR refers to an element of an array, then the element name is passed as the second argument to the trace procedure. This may be important information in case of traces set on an entire array.

# Example

A \fBdecr\fR command that works like \fBincr\fR except it subtracts the value from the variable instead of adding it:

```
proc decr {varName {decrement 1}} {
    upvar 1 $varName var
    incr var [expr {-$decrement}]
}
```

