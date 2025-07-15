---
CommandName: error
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - catch(n)
 - return(n)
Keywords:
 - error
 - exception
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

error - Generate an error

# Synopsis

::: {.synopsis} :::
[error]{.cmd} [message]{.arg} [info]{.optarg} [code]{.optarg}
:::

# Description

Returns a \fBTCL_ERROR\fR code, which causes command interpretation to be unwound.  \fIMessage\fR is a string that is returned to the application to indicate what went wrong.

The \fB-errorinfo\fR return option of an interpreter is used to accumulate a stack trace of what was in progress when an error occurred; as nested commands unwind, the Tcl interpreter adds information to the \fB-errorinfo\fR return option.  If the \fIinfo\fR argument is present, it is used to initialize the \fB-errorinfo\fR return options and the first increment of unwind information will not be added by the Tcl interpreter. In other words, the command containing the \fBerror\fR command will not appear in the stack trace; in its place will be \fIinfo\fR. Historically, this feature had been most useful in conjunction with the \fBcatch\fR command: if a caught error cannot be handled successfully, \fIinfo\fR can be used to return a stack trace reflecting the original point of occurrence of the error:

```
catch {...} errMsg
set savedInfo $::errorInfo
\&...
error $errMsg $savedInfo
```

When working with Tcl 8.5 or later, the following code should be used instead:

```
catch {...} errMsg options
\&...
return -options $options $errMsg
```

If the \fIcode\fR argument is present, then its value is stored in the \fB-errorcode\fR return option.  The \fB-errorcode\fR return option is intended to hold a machine-readable description of the error in cases where such information is available; see the \fBreturn\fR manual page for information on the proper format for this option's value.

# Example

Generate an error if a basic mathematical operation fails:

```
if {1+2 != 3} {
    error "something is very wrong with addition"
}
```

