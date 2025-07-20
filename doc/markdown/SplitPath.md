---
CommandName: Tcl_SplitPath
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - file
 - filename
 - join
 - path
 - split
 - type
Copyright:
 - Copyright (c) 1996 Sun Microsystems, Inc.
---

# Name

Tcl_SplitPath, Tcl_JoinPath, Tcl_GetPathType - manipulate platform-dependent file paths

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_SplitPath]{.ccmd}[path, argcPtr, argvPtr]{.cargs}
[char *]{.ret} [Tcl_JoinPath]{.ccmd}[argc, argv, resultPtr]{.cargs}
[Tcl_PathType]{.ret} [Tcl_GetPathType]{.ccmd}[path]{.cargs}
:::

# Arguments

.AP "const char" *path in File path in a form appropriate for the current platform (see the **filename** manual entry for acceptable forms for path names). .AP "Tcl_Size \&| int" *argcPtr out Filled in with number of path elements in *path*. May be (Tcl_Size *)NULL when not used. If it points to a variable which type is not **Tcl_Size**, a compiler warning will be generated. If your extensions is compiled with -DTCL_8_API, argcPtr will be filled with -1 for paths with more than INT_MAX elements (which should trigger proper error-handling), otherwise expect it to crash. .AP "const char" ***argvPtr out **argvPtr* will be filled in with the address of an array of pointers to the strings that are the extracted elements of *path*. There will be **argcPtr* valid entries in the array, followed by a NULL entry. .AP Tcl_Size argc in Number of elements in *argv*. .AP "const char *const" *argv in Array of path elements to merge together into a single path. .AP Tcl_DString *resultPtr in/out A pointer to an initialized **Tcl_DString** to which the result of **Tcl_JoinPath** will be appended. 

# Description

These procedures have been superseded by the Tcl-value-aware procedures in the **FileSystem** man page, which are more efficient.

These procedures may be used to disassemble and reassemble file paths in a platform independent manner: they provide C-level access to the same functionality as the **file split**, **file join**, and **file pathtype** commands.

**Tcl_SplitPath** breaks a path into its constituent elements, returning an array of pointers to the elements using *argcPtr* and *argvPtr*.  The area of memory pointed to by **argvPtr* is dynamically allocated; in addition to the array of pointers, it also holds copies of all the path elements.  It is the caller's responsibility to free all of this storage. For example, suppose that you have called **Tcl_SplitPath** with the following code:

```
Tcl_Size argc;
char *path;
char **argv;
\&...
Tcl_SplitPath(string, &argc, &argv);
```

Then you should eventually free the storage with a call like the following:

```
Tcl_Free(argv);
```

**Tcl_JoinPath** is the inverse of **Tcl_SplitPath**: it takes a collection of path elements given by *argc* and *argv* and generates a result string that is a properly constructed path. The result string is appended to *resultPtr*.  *ResultPtr* must refer to an initialized **Tcl_DString**.

If the result of **Tcl_SplitPath** is passed to **Tcl_JoinPath**, the result will refer to the same location, but may not be in the same form.  This is because **Tcl_SplitPath** and **Tcl_JoinPath** eliminate duplicate path separators and return a normalized form for each platform.

**Tcl_GetPathType** returns the type of the specified *path*, where **Tcl_PathType** is one of **TCL_PATH_ABSOLUTE**, **TCL_PATH_RELATIVE**, or **TCL_PATH_VOLUME_RELATIVE**.  See the **filename** manual entry for a description of the path types for each platform. 

