---
CommandName: Tcl_Panic
ManualSection: 3
Version: 8.4
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - abort(3)
 - printf(3)
 - exec(n)
 - format(n)
Keywords:
 - abort
 - fatal
 - error
Copyright:
---

# Name

Tcl_Panic, Tcl_SetPanicProc, Tcl_ConsolePanic - report fatal error and abort

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Panic]{.ccmd}[format=, +arg=, +arg=, +...]{.cargs}
[const char *]{.ret} [Tcl_SetPanicProc]{.ccmd}[panicProc]{.cargs}
[Tcl_ConsolePanic]{.ccmd}[format=, +arg=, +arg=, +...]{.cargs}
:::

# Arguments

.AP "const char*" format in A printf-style format string. .AP "" arg in Arguments matching the format string. .AP Tcl_PanicProc *panicProc in Procedure to report fatal error message and abort.

# Description

When the Tcl library detects that its internal data structures are in an inconsistent state, or that its C procedures have been called in a manner inconsistent with their documentation, it calls **Tcl_Panic** to display a message describing the error and abort the process.  The *format* argument is a format string describing how to format the remaining arguments *arg* into an error message, according to the same formatting rules used by the **printf** family of functions.  The same formatting rules are also used by the built-in Tcl command **format**.

In a freshly loaded Tcl library, **Tcl_Panic** prints the formatted error message to the standard error file of the process, and then calls **abort** to terminate the process.  **Tcl_Panic** does not return. On Windows, when a debugger is running, the formatted error message is sent to the debugger instead. If the windows executable does not have a stderr channel (e.g. **wish.exe**), then a system dialog box is used to display the panic message.

If your application doesn't use **Tcl_Main** or **Tk_Main** and you want to implicitly use the stderr channel of your application's C runtime (instead of the stderr channel of the C runtime used by Tcl), you can call **Tcl_SetPanicProc** with **Tcl_ConsolePanic** as its argument. On platforms which only have one C runtime (almost all platforms except Windows) **Tcl_ConsolePanic** is equivalent to NULL.

**Tcl_SetPanicProc** may be used to modify the behavior of **Tcl_Panic**.  The *panicProc* argument should match the type **Tcl_PanicProc**:

```
typedef void Tcl_PanicProc(
        const char *format,
        arg, arg,...);
```

After **Tcl_SetPanicProc** returns, any future calls to **Tcl_Panic** will call *panicProc*, passing along the *format* and *arg* arguments. *panicProc* should avoid making calls into the Tcl library, or into other libraries that may call the Tcl library, since the original call to **Tcl_Panic** indicates the Tcl library is not in a state of reliable operation.

The result of **Tcl_SetPanicProc** is the full Tcl version with build information (e.g., **9.0.0+abcdef...abcdef.gcc-1002**).

The typical use of **Tcl_SetPanicProc** arranges for the error message to be displayed or reported in a manner more suitable for the application or the platform.

**Tcl_SetPanicProc** can not be used in stub-enabled extensions.

Although the primary callers of **Tcl_Panic** are the procedures of the Tcl library, **Tcl_Panic** is a public function and may be called by any extension or application that wishes to abort the process and have a panic message displayed the same way that panic messages from Tcl will be displayed.

