---
CommandName: Tcl_GetVersion
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - version
 - patchlevel
 - major
 - minor
 - alpha
 - beta
 - release
Copyright:
 - Copyright (c) 1999 Scriptics Corporation
---

# Name

Tcl_GetVersion - get the version of the library at runtime

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_GetVersion]{.ccmd}[major, minor, patchLevel, type]{.cargs}
:::

# Arguments

.AP int *major out Major version number of the Tcl library. .AP int *minor out Minor version number of the Tcl library. .AP int *patchLevel out The patch level of the Tcl library (or alpha or beta number). .AP int *type out The type of release, also indicates the type of patch level. Can be one of **TCL_ALPHA_RELEASE**, **TCL_BETA_RELEASE**, or **TCL_FINAL_RELEASE**. 

# Description

**Tcl_GetVersion** should be used to query the version number of the Tcl library at runtime.  This is useful when using a dynamically loaded Tcl library or when writing a stubs-aware extension.  For instance, if you write an extension that is linked against the Tcl stubs library, it could be loaded into a program linked to an older version of Tcl than you expected. Use **Tcl_GetVersion** to verify that fact, and possibly to change the behavior of your extension.

**Tcl_GetVersion** accepts NULL for any of the arguments. For instance if you do not care about the *patchLevel* of the library, pass a NULL for the *patchLevel* argument. 

