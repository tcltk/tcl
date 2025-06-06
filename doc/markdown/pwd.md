---
CommandName: pwd
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - file(n)
 - cd(n)
 - glob(n)
 - filename(n)
Keywords:
 - working directory
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

pwd - Return the absolute path of the current working directory

# Synopsis

::: {.synopsis} :::
[pwd]{.cmd}
:::

# Description

Returns the absolute path name of the current working directory.

# Example

Sometimes it is useful to change to a known directory when running some external command using **exec**, but it is important to keep the application usually running in the directory that it was started in (unless the user specifies otherwise) since that minimizes user confusion. The way to do this is to save the current directory while the external command is being run:

```
set tarFile [file normalize somefile.tar]
set savedDir [pwd]
cd /tmp
exec tar -xf $tarFile
cd $savedDir
```

