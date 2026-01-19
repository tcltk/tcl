---
CommandName: Tcl_Access
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - stat
 - access
Links:
 - Tcl_FSAccess(3)
 - Tcl_FSStat(3)
Copyright:
 - Copyright (c) 1998-1999 Scriptics Corporation
---

# Name

Tcl\_Access, Tcl\_Stat - check file permissions and other attributes

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_Access]{.ccmd}[path=, +mode]{.cargs}
[int]{.ret} [Tcl\_Stat]{.ccmd}[path=, +statPtr]{.cargs}
:::

# Arguments

.AP "const char" \*path in Native name of the file to check the attributes of. .AP int mode in Mask consisting of one or more of **R\_OK**, **W\_OK**, **X\_OK** and **F\_OK**. **R\_OK**, **W\_OK** and **X\_OK** request checking whether the file exists and has read, write and execute permissions, respectively. **F\_OK** just requests a check for the existence of the file. .AP "struct stat" \*statPtr out The structure that contains the result.

# Description

The object-based APIs **Tcl\_FSAccess** and **Tcl\_FSStat** should be used in preference to **Tcl\_Access** and **Tcl\_Stat**, wherever possible. Those functions also support Tcl's virtual filesystem layer, which these do not.

## Obsolete functions

There are two reasons for calling **Tcl\_Access** and **Tcl\_Stat** rather than calling system level functions **access** and **stat** directly. First, the Windows implementation of both functions fixes some bugs in the system level calls. Second, both **Tcl\_Access** and **Tcl\_Stat** (as well as **Tcl\_OpenFileChannelProc**) hook into a linked list of functions. This allows the possibility to reroute file access to alternative media or access methods.

**Tcl\_Access** checks whether the process would be allowed to read, write or test for existence of the file (or other file system object) whose name is *path*. If *path* is a symbolic link on Unix, then permissions of the file referred by this symbolic link are tested.

On success (all requested permissions granted), zero is returned. On error (at least one bit in mode asked for a permission that is denied, or some other error occurred), -1 is returned.

**Tcl\_Stat** fills the stat structure *statPtr* with information about the specified file. You do not need any access rights to the file to get this information but you need search rights to all directories named in the path leading to the file. The stat structure includes info regarding device, inode (always 0 on Windows), privilege mode, nlink (always 1 on Windows), user id (always 0 on Windows), group id (always 0 on Windows), rdev (same as device on Windows), size, last access time, last modification time, and creation time.

If *path* exists, **Tcl\_Stat** returns 0 and the stat structure is filled with data. Otherwise, -1 is returned, and no stat info is given.

