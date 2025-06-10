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

Change the current working directory to *dirName*, or to the home directory (as specified in the HOME environment variable) if *dirName* is not given. Returns an empty string.

Note that the current working directory is a per-process resource; the **cd** command changes the working directory for all interpreters and all threads.

# Examples

Change to the home directory of the user **fred**:

```
cd [file home fred]
```

Change to the directory **lib** that is a sibling directory of the current one:

```
cd ../lib
```

