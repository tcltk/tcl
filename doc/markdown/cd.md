---
CommandName: cd
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - filename(n)
 - glob(n)
 - pwd(n)
Keywords:
 - working directory
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

cd - Change working directory

# Synopsis

::: {.synopsis} :::
[cd]{.cmd} [dirName]{.optarg}
:::

# Description

Change the current working directory to \fIdirName\fR, or to the home directory (as specified in the HOME environment variable) if \fIdirName\fR is not given. Returns an empty string.

Note that the current working directory is a per-process resource; the \fBcd\fR command changes the working directory for all interpreters and all threads.

# Examples

Change to the home directory of the user \fBfred\fR:

```
cd [file home fred]
```

Change to the directory \fBlib\fR that is a sibling directory of the current one:

```
cd ../lib
```

