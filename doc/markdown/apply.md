---
CommandName: apply
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - proc(n)
 - uplevel(n)
Keywords:
 - anonymous function
 - argument
 - lambda
 - procedure
Copyright:
 - Copyright (c) 2006 Miguel Sofer
 - Copyright (c) 2006 Donal K. Fellows
---

# Name

apply - Apply an anonymous function

# Synopsis

::: {.synopsis} :::
[apply]{.cmd} [func]{.arg} [arg1 arg2]{.optdot}
:::

# Description

The command \fBapply\fR applies the function \fIfunc\fR to the arguments \fIarg1 arg2 ...\fR and returns the result.

The function \fIfunc\fR is a two element list \fI{args body}\fR or a three element list \fI{args body namespace}\fR (as if the \fBlist\fR command had been used). The first element \fIargs\fR specifies the formal arguments to \fIfunc\fR. The specification of the formal arguments \fIargs\fR is shared with the \fBproc\fR command, and is described in detail in the corresponding manual page.

The contents of \fIbody\fR are executed by the Tcl interpreter after the local variables corresponding to the formal arguments are given the values of the actual parameters \fIarg1 arg2 ...\fR. When \fIbody\fR is being executed, variable names normally refer to local variables, which are created automatically when referenced and deleted when \fBapply\fR returns.  One local variable is automatically created for each of the function's arguments. Global variables can only be accessed by invoking the \fBglobal\fR command or the \fBupvar\fR command. Namespace variables can only be accessed by invoking the \fBvariable\fR command or the \fBupvar\fR command.

The invocation of \fBapply\fR adds a call frame to Tcl's evaluation stack (the stack of frames accessed via \fBuplevel\fR). The execution of \fIbody\fR proceeds in this call frame, in the namespace given by \fInamespace\fR or in the global namespace if none was specified. If given, \fInamespace\fR is interpreted relative to the global namespace even if its name does not start with "::".

The semantics of \fBapply\fR can also be described by approximately this:

```
proc apply {fun args} {
    set len [llength $fun]
    if {($len < 2) || ($len > 3)} {
        error "can't interpret \"$fun\" as anonymous function"
    }
    lassign $fun argList body ns
    set name ::$ns::[getGloballyUniqueName]
    set body0 {
        rename [lindex [info level 0] 0] {}
    }
    proc $name $argList ${body0}$body
    set code [catch {uplevel 1 $name $args} res opt]
    return -options $opt $res
}
```

# Examples

This shows how to make a simple general command that applies a transformation to each element of a list.

```
proc map {lambda list} {
    set result {}
    foreach item $list {
        lappend result [apply $lambda $item]
    }
    return $result
}
map {x {return [string length $x]:$x}} {a bb ccc dddd}
      \(-> 1:a 2:bb 3:ccc 4:dddd
map {x {expr {$x**2 + 3*$x - 2}}} {-4 -3 -2 -1 0 1 2 3 4}
      \(-> 2 -2 -4 -4 -2 2 8 16 26
```

The \fBapply\fR command is also useful for defining callbacks for use in the \fBtrace\fR command:

```
set vbl "123abc"
trace add variable vbl write {apply {{v1 v2 op} {
    upvar 1 $v1 v
    puts "updated variable to \"$v\""
}}}
set vbl 123
set vbl abc
```

