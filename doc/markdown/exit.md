---
CommandName: exit
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - exec(n)
Keywords:
 - abort
 - exit
 - process
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

exit - End the application

# Synopsis

::: {.synopsis} :::
[exit]{.cmd} [returnCode]{.optarg}
:::

# Description

Terminate the process, returning \fIreturnCode\fR to the system as the exit status. If \fIreturnCode\fR is not specified then it defaults to 0.

# Example

Since non-zero exit codes are usually interpreted as error cases by the calling process, the \fBexit\fR command is an important part of signaling that something fatal has gone wrong. This code fragment is useful in scripts to act as a general problem trap:

```
proc main {} {
    # ... put the real main code in here ...
}

if {[catch {main} msg options]} {
    puts stderr "unexpected script error: $msg"
    if {[info exists env(DEBUG)]} {
        puts stderr "---- BEGIN TRACE ----"
        puts stderr [dict get $options -errorinfo]
        puts stderr "---- END TRACE ----"
    }

    # Reserve code 1 for "expected" error exits...
    exit 2
}
```

