---
CommandName: Tcl_Cancel
ManualSection: 3
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - interp(n)
 - Tcl_Eval(3)
Keywords:
 - cancel
 - unwind
Copyright:
 - Copyright (c) 2006-2008 Joe Mistachkin.
---

# Name

Tcl\_CancelEval, Tcl\_Canceled - cancel Tcl scripts

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_CancelEval]{.ccmd}[interp, resultObjPtr, clientData, flags]{.cargs}
[int]{.ret} [Tcl\_Canceled]{.ccmd}[interp, flags]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter in which to cancel the script. .AP Tcl\_Obj \*resultObjPtr in Error message to use in the cancellation, or NULL to use a default message. If not NULL, this object will have its reference count decremented before **Tcl\_CancelEval** returns. .AP int flags in OR'ed combination of flag bits that specify additional options. For **Tcl\_CancelEval**, only **TCL\_CANCEL\_UNWIND** is currently supported.  For **Tcl\_Canceled**, only **TCL\_LEAVE\_ERR\_MSG** and **TCL\_CANCEL\_UNWIND** are currently supported. .AP void \*clientData in Currently reserved for future use. It should be set to NULL.

# Description

**Tcl\_CancelEval** cancels or unwinds the script in progress soon after the next invocation of asynchronous handlers, causing **TCL\_ERROR** to be the return code for that script.  This function is thread-safe and may be called from any thread in the process.

**Tcl\_Canceled** checks if the script in progress has been canceled and returns **TCL\_ERROR** if it has.  Otherwise, **TCL\_OK** is returned. Extensions can use this function to check to see if they should abort a long running command.  This function is thread sensitive and may only be called from the thread the interpreter was created in.

## Flag bits

Any OR'ed combination of the following values may be used for the *flags* argument to procedures such as **Tcl\_CancelEval**:

**TCL\_CANCEL\_UNWIND**
: This flag is used by **Tcl\_CancelEval** and **Tcl\_Canceled**. For **Tcl\_CancelEval**, if this flag is set, the script in progress is canceled and the evaluation stack for the interpreter is unwound. For **Tcl\_Canceled**, if this flag is set, the script in progress is considered to be canceled only if the evaluation stack for the interpreter is being unwound.

**TCL\_LEAVE\_ERR\_MSG**
: This flag is only used by **Tcl\_Canceled**; it is ignored by other procedures.  If an error is returned and this bit is set in *flags*, then an error message will be left in the interpreter's result, where it can be retrieved with **Tcl\_GetObjResult** or **Tcl\_GetStringResult**.  If this flag bit is not set then no error message is left and the interpreter's result will not be modified.


# Reference count management

**Tcl\_CancelEval** always decrements the reference count of its *resultObjPtr* argument (if that is non-NULL). It is expected to be usually called with an object with zero reference count. If the object is shared with some other location (including the Tcl evaluation stack) it should have its reference count incremented before calling this function.

