---
CommandName: rename
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - namespace(n)
 - proc(n)
Keywords:
 - command
 - delete
 - namespace
 - rename
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

rename - Rename or delete a command

# Synopsis

::: {.synopsis} :::
[rename]{.cmd} [oldName]{.arg} [newName]{.arg}
:::

# Description

Rename the command that used to be called \fIoldName\fR so that it is now called \fInewName\fR. If \fInewName\fR is an empty string then \fIoldName\fR is deleted. \fIoldName\fR and \fInewName\fR may include namespace qualifiers (names of containing namespaces). If a command is renamed into a different namespace, future invocations of it will execute in the new namespace. The \fBrename\fR command returns an empty string as result.

# Example

The \fBrename\fR command can be used to wrap the standard Tcl commands with your own monitoring machinery.  For example, you might wish to count how often the \fBsource\fR command is called:

```
rename ::source ::theRealSource
set sourceCount 0
proc ::source args {
    global sourceCount
    puts "called source for the [incr sourceCount]'th time"
    uplevel 1 ::theRealSource $args
}
```

