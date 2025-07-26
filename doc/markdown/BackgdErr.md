---
CommandName: Tcl_BackgroundError
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - background
 - bgerror
 - error
 - interp
Copyright:
 - Copyright (c) 1992-1994 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_BackgroundException, Tcl_BackgroundError - report Tcl exception that occurred in background processing

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_BackgroundException]{.ccmd}[interp, code]{.cargs}
[Tcl_BackgroundError]{.ccmd}[interp]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter in which the exception occurred. .AP int code in The exceptional return code to be reported. 

# Description

This procedure is typically invoked when a Tcl exception (any return code other than TCL_OK) occurs during "background processing" such as executing an event handler. When such an exception occurs, the condition is reported to Tcl or to a widget or some other C code, and there is not usually any obvious way for that code to report the exception to the user. In these cases the code calls **Tcl_BackgroundException** with an *interp* argument identifying the interpreter in which the exception occurred, and a *code* argument holding the return code value of the exception.  The state of the interpreter, including any error message in the interpreter result, and the values of any entries in the return options dictionary, is captured and saved.  **Tcl_BackgroundException** then arranges for the event loop to invoke at some later time the command registered in that interpreter to handle background errors by the **interp bgerror** command, passing the captured values as arguments. The registered handler command is meant to report the exception in an application-specific fashion.  The handler command receives two arguments, the result of the interp, and the return options of the interp at the time the error occurred. If the application registers no handler command, the default handler command will attempt to call **bgerror** to report the error.  If an error condition arises while invoking the handler command, then **Tcl_BackgroundException** reports the error itself by printing a message on the standard error file.

**Tcl_BackgroundException** does not invoke the handler command immediately because this could potentially interfere with scripts that are in process at the time the error occurred. Instead, it invokes the handler command later as an idle callback.

It is possible for many background exceptions to accumulate before the handler command is invoked.  When this happens, each of the exceptions is processed in order.  However, if the handler command returns a break exception, then all remaining error reports for the interpreter are skipped.

The **Tcl_BackgroundError** routine is an older and simpler interface useful when the exception code reported is **TCL_ERROR**.  It is equivalent to:

```
Tcl_BackgroundException(interp, TCL_ERROR);
```

