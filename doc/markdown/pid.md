---
CommandName: pid
ManualSection: n
Version: 7.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - exec(n)
 - open(n)
Keywords:
 - file
 - pipeline
 - process identifier
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

pid - Retrieve process identifiers

# Synopsis

::: {.synopsis} :::
[pid]{.cmd} [fileId]{.optarg}
:::

# Description

If the *fileId* argument is given then it should normally refer to a process pipeline created with the **open** command. In this case the **pid** command will return a list whose elements are the process identifiers of all the processes in the pipeline, in order. The list will be empty if *fileId* refers to an open file that is not a process pipeline. If no *fileId* argument is given then **pid** returns the process identifier of the current process. All process identifiers are returned as decimal strings.

# Example

Print process information about the processes in a pipeline using the SysV **ps** program before reading the output of that pipeline:

```
set pipeline [open "| zcat somefile.gz | grep foobar | sort -u"]
# Print process information
exec ps -fp [pid $pipeline] >@stdout
# Print a separator and then the output of the pipeline
puts [string repeat - 70]
puts [read $pipeline]
close $pipeline
```

