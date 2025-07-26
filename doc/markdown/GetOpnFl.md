---
CommandName: Tcl_GetOpenFile
ManualSection: 3
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - channel
 - file handle
 - permissions
 - pipeline
 - read
 - write
Copyright:
 - Copyright (c) 1996-1997 Sun Microsystems, Inc.
---

# Name

Tcl_GetOpenFile - Return a FILE* for a channel registered in the given interpreter (Unix only)

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_GetOpenFile]{.ccmd}[interp, chanID, write, checkUsage, filePtr]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Tcl interpreter from which file handle is to be obtained. .AP "const char" *chanID in String identifying channel, such as **stdin** or **file4**. .AP int write in Non-zero means the file will be used for writing, zero means it will be used for reading. .AP int checkUsage in If non-zero, then an error will be generated if the file was not opened for the access indicated by *write*. .AP void **filePtr out Points to word in which to store pointer to FILE structure for the file given by *chanID*. 

# Description

**Tcl_GetOpenFile** takes as argument a file identifier of the form returned by the **open** command and returns at **filePtr* a pointer to the FILE structure for the file. The *write* argument indicates whether the FILE pointer will be used for reading or writing. In some cases, such as a channel that connects to a pipeline of subprocesses, different FILE pointers will be returned for reading and writing. **Tcl_GetOpenFile** normally returns **TCL_OK**. If an error occurs in **Tcl_GetOpenFile** (e.g. *chanID* did not make any sense or *checkUsage* was set and the file was not opened for the access specified by *write*) then **TCL_ERROR** is returned and the interpreter's result will contain an error message. In the current implementation *checkUsage* is ignored and consistency checks are always performed.

Note that this interface is only supported on the Unix platform. 

