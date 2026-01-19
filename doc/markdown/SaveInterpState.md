---
CommandName: Tcl_SaveInterpState
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - result
 - state
 - interp
Copyright:
 - Copyright (c) 1997 Sun Microsystems, Inc.
---

# Name

Tcl\_SaveInterpState, Tcl\_RestoreInterpState, Tcl\_DiscardInterpState - save and restore an interpreter's state

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_InterpState]{.ret} [Tcl\_SaveInterpState]{.ccmd}[interp, status]{.cargs}
[int]{.ret} [Tcl\_RestoreInterpState]{.ccmd}[interp, state]{.cargs}
[Tcl\_DiscardInterpState]{.ccmd}[state]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter for which state should be saved. .AP int status in Return code value to save as part of interpreter state. .AP Tcl\_InterpState state in Saved state token to be restored or discarded.

# Description

These routines allows a C procedure to take a snapshot of the current state of an interpreter so that it can be restored after a call to **Tcl\_Eval** or some other routine that modifies the interpreter state.

**Tcl\_SaveInterpState** stores a snapshot of the interpreter state in an opaque token returned by **Tcl\_SaveInterpState**.  That token value may then be passed back to one of **Tcl\_RestoreInterpState** or **Tcl\_DiscardInterpState**, depending on whether the interp state is to be restored.  So long as one of the latter two routines is called, Tcl will take care of memory management.

**Tcl\_SaveInterpState** takes a snapshot of those portions of interpreter state that make up the full result of script evaluation. This include the interpreter result, the return code (passed in as the *status* argument, and any return options, including **-errorinfo** and **-errorcode** when an error is in progress. This snapshot is returned as an opaque token of type **Tcl\_InterpState**. The call to **Tcl\_SaveInterpState** does not itself change the state of the interpreter.

**Tcl\_RestoreInterpState** accepts a **Tcl\_InterpState** token previously returned by **Tcl\_SaveInterpState** and restores the state of the interp to the state held in that snapshot.  The return value of **Tcl\_RestoreInterpState** is the status value originally passed to **Tcl\_SaveInterpState** when the snapshot token was created.

**Tcl\_DiscardInterpState** is called to release a **Tcl\_InterpState** token previously returned by **Tcl\_SaveInterpState** when that snapshot is not to be restored to an interp.

The **Tcl\_InterpState** token returned by **Tcl\_SaveInterpState** must eventually be passed to either **Tcl\_RestoreInterpState** or **Tcl\_DiscardInterpState** to avoid a memory leak.  Once the **Tcl\_InterpState** token is passed to one of them, the token is no longer valid and should not be used anymore.

