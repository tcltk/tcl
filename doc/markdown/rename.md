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

Rename the command that used to be called *oldName* so that it is now called *newName*. If *newName* is an empty string then *oldName* is deleted. *oldName* and *newName* may include namespace qualifiers (names of containing namespaces). If a command is renamed into a different namespace, future invocations of it will execute in the new namespace. The **rename** command returns an empty string as result.

# Example

The **rename** command can be used to wrap the standard Tcl commands with your own monitoring machinery.  For example, you might wish to count how often the **source** command is called:

```
rename ::source ::theRealSource
set sourceCount 0
proc ::source args {
    global sourceCount
    puts "called source for the [incr sourceCount]'th time"
    uplevel 1 ::theRealSource $args
}
```

