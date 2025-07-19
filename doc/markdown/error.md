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

Returns a **TCL_ERROR** code, which causes command interpretation to be unwound.  *Message* is a string that is returned to the application to indicate what went wrong.

The **-errorinfo** return option of an interpreter is used to accumulate a stack trace of what was in progress when an error occurred; as nested commands unwind, the Tcl interpreter adds information to the **-errorinfo** return option.  If the *info* argument is present, it is used to initialize the **-errorinfo** return options and the first increment of unwind information will not be added by the Tcl interpreter. In other words, the command containing the **error** command will not appear in the stack trace; in its place will be *info*. Historically, this feature had been most useful in conjunction with the **catch** command: if a caught error cannot be handled successfully, *info* can be used to return a stack trace reflecting the original point of occurrence of the error:

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

If the *code* argument is present, then its value is stored in the **-errorcode** return option.  The **-errorcode** return option is intended to hold a machine-readable description of the error in cases where such information is available; see the **return** manual page for information on the proper format for this option's value.

# Example

Generate an error if a basic mathematical operation fails:

```
if {1+2 != 3} {
    error "something is very wrong with addition"
}
```

