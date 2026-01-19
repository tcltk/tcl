---
CommandName: Tcl_TranslateFileName
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - filename(n)
Keywords:
 - file name
 - home directory
 - translate
 - user
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1998 Sun Microsystems, Inc.
---

# Name

Tcl\_TranslateFileName - convert file name to native form

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[char \*]{.ret} [Tcl\_TranslateFileName]{.ccmd}[interp=, +name=, +bufferPtr]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter in which to report an error, if any. .AP "const char" \*name in File name .AP Tcl\_DString \*bufferPtr in/out If needed, this dynamic string is used to store the new file name. At the time of the call it should be uninitialized or free.  The caller must eventually call **Tcl\_DStringFree** to free up anything stored here.

# Description

This utility procedure translates a file name to a platform-specific form which, after being converted to the appropriate encoding, is suitable for passing to the local operating system.  In particular, it converts network names into native form.

However, with the advent of the newer **Tcl\_FSGetNormalizedPath** and **Tcl\_FSGetNativePath**, there is no longer any need to use this procedure.  In particular, **Tcl\_FSGetNativePath** performs all the necessary translation and encoding conversion, is virtual-filesystem aware, and caches the native result for faster repeated calls. Finally **Tcl\_FSGetNativePath** does not require you to free anything afterwards.

If **Tcl\_TranslateFileName** has to translate the name then it uses the dynamic string at *\*bufferPtr* to hold the new string it generates. After **Tcl\_TranslateFileName** returns a non-NULL result, the caller must eventually invoke **Tcl\_DStringFree** to free any information placed in *\*bufferPtr*.  The caller need not know whether or not **Tcl\_TranslateFileName** actually used the string;  **Tcl\_TranslateFileName** initializes *\*bufferPtr* even if it does not use it, so the call to **Tcl\_DStringFree** will be safe in either case.

If an error occurs (e.g. because there was no user by the given name) then NULL is returned and an error message will be left in the interpreter's result. When an error occurs, **Tcl\_TranslateFileName** frees the dynamic string itself so that the caller need not call **Tcl\_DStringFree**.

The caller is responsible for making sure that the interpreter's result has its default empty value when **Tcl\_TranslateFileName** is invoked.

