---
CommandName: Tcl_GetCwd
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - pwd
Copyright:
 - Copyright (c) 1998-1999 Scriptics Corporation
---

# Name

Tcl\_GetCwd, Tcl\_Chdir - manipulate the current working directory

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[char \*]{.ret} [Tcl\_GetCwd]{.ccmd}[interp=, +bufferPtr]{.cargs}
[int]{.ret} [Tcl\_Chdir]{.ccmd}[dirName]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter in which to report an error, if any. .AP Tcl\_DString \*bufferPtr in/out This dynamic string is used to store the current working directory. At the time of the call it should be uninitialized or free.  The caller must eventually call **Tcl\_DStringFree** to free up anything stored here. .AP "const char" \*dirName in File path in UTF-8 format. 

# Description

These procedures may be used to manipulate the current working directory for the application.  They provide C-level access to the same functionality as the Tcl [pwd] command.

**Tcl\_GetCwd** returns a pointer to a string specifying the current directory, or NULL if the current directory could not be determined. If NULL is returned, an error message is left in the *interp*'s result. Storage for the result string is allocated in bufferPtr; the caller must call **Tcl\_DStringFree()** when the result is no longer needed. The format of the path is UTF-8.

**Tcl\_Chdir** changes the applications current working directory to the value specified in *dirName*.  The format of the passed in string must be UTF-8.  The function returns -1 on error or 0 on success. 


[pwd]: pwd.md

