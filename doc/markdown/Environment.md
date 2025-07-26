---
CommandName: Tcl_PutEnv
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - env(n)
Keywords:
 - environment
 - variable
Copyright:
 - Copyright (c) 1997-1998 Sun Microsystems, Inc.
---

# Name

Tcl_PutEnv - procedures to manipulate the environment

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_PutEnv]{.ccmd}[assignment]{.cargs}
:::

# Arguments

.AP "const char" *assignment in Info about environment variable in the format "*NAME***=***value*". The *assignment* argument is in the system encoding.

# Description

**Tcl_PutEnv** sets an environment variable. The information is passed in a single string of the form "*NAME***=***value*". This procedure is intended to be a stand-in for the UNIX **putenv** system call. All Tcl-based applications using **putenv** should redefine it to **Tcl_PutEnv** so that they will interface properly to the Tcl runtime.

