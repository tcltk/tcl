---
CommandName: variable
ManualSection: n
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - global(n)
 - namespace(n)
 - upvar(n)
Keywords:
 - global
 - namespace
 - procedure
 - variable
Copyright:
 - Copyright (c) 1993-1997 Bell Labs Innovations for Lucent Technologies
 - Copyright (c) 1997 Sun Microsystems, Inc.
---

# Name

variable - create and initialize a namespace variable

# Synopsis

::: {.synopsis} :::
[variable]{.cmd} [name]{.arg}
[variable]{.cmd} [name value...]{.optarg}
:::

# Description

This command is normally used within a \fBnamespace eval\fR command to create one or more variables within a namespace. Each variable \fIname\fR is initialized with \fIvalue\fR. The \fIvalue\fR for the last variable is optional.

If a variable \fIname\fR does not exist, it is created. In this case, if \fIvalue\fR is specified, it is assigned to the newly created variable. If no \fIvalue\fR is specified, the new variable is left undefined. If the variable already exists, it is set to \fIvalue\fR if \fIvalue\fR is specified or left unchanged if no \fIvalue\fR is given. Normally, \fIname\fR is unqualified (does not include the names of any containing namespaces), and the variable is created in the current namespace. If \fIname\fR includes any namespace qualifiers, the variable is created in the specified namespace.  If the variable is not defined, it will be visible to the \fBnamespace which\fR command, but not to the \fBinfo exists\fR command.

If the \fBvariable\fR command is executed inside a Tcl procedure, it creates local variables linked to the corresponding namespace variables (and therefore these variables are listed by \fBinfo vars\fR.) In this way the \fBvariable\fR command resembles the \fBglobal\fR command, although the \fBglobal\fR command resolves variable names with respect to the global namespace instead of the current namespace of the procedure. If any \fIvalue\fRs are given, they are used to modify the values of the associated namespace variables. If a namespace variable does not exist, it is created and optionally initialized.

A \fIname\fR argument cannot reference an element within an array. Instead, \fIname\fR should reference the entire array, and the initialization \fIvalue\fR should be left off. After the variable has been declared, elements within the array can be set using ordinary \fBset\fR or \fBarray\fR commands.

# Examples

Create a variable in a namespace:

```
namespace eval foo {
    variable bar 12345
}
```

Create an array in a namespace:

```
namespace eval someNS {
    variable someAry
    array set someAry {
        someName  someValue
        otherName otherValue
    }
}
```

Access variables in namespaces from a procedure:

```
namespace eval foo {
    proc spong {} {
        # Variable in this namespace
        variable bar
        puts "bar is $bar"

        # Variable in another namespace
        variable ::someNS::someAry
        parray someAry
    }
}
```

