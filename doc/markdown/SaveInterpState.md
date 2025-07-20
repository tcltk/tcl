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

Tcl_SaveInterpState, Tcl_RestoreInterpState, Tcl_DiscardInterpState - save and restore an interpreter's state

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_InterpState]{.ret} [Tcl_SaveInterpState]{.ccmd}[interp, status]{.cargs}
[int]{.ret} [Tcl_RestoreInterpState]{.ccmd}[interp, state]{.cargs}
[Tcl_DiscardInterpState]{.ccmd}[state]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter for which state should be saved. .AP int status in Return code value to save as part of interpreter state. .AP Tcl_InterpState state in Saved state token to be restored or discarded.

# Description

These routines allows a C procedure to take a snapshot of the current state of an interpreter so that it can be restored after a call to **Tcl_Eval** or some other routine that modifies the interpreter state.

**Tcl_SaveInterpState** stores a snapshot of the interpreter state in an opaque token returned by **Tcl_SaveInterpState**.  That token value may then be passed back to one of **Tcl_RestoreInterpState** or **Tcl_DiscardInterpState**, depending on whether the interp state is to be restored.  So long as one of the latter two routines is called, Tcl will take care of memory management.

**Tcl_SaveInterpState** takes a snapshot of those portions of interpreter state that make up the full result of script evaluation. This include the interpreter result, the return code (passed in as the *status* argument, and any return options, including **-errorinfo** and **-errorcode** when an error is in progress. This snapshot is returned as an opaque token of type **Tcl_InterpState**. The call to **Tcl_SaveInterpState** does not itself change the state of the interpreter.

**Tcl_RestoreInterpState** accepts a **Tcl_InterpState** token previously returned by **Tcl_SaveInterpState** and restores the state of the interp to the state held in that snapshot.  The return value of **Tcl_RestoreInterpState** is the status value originally passed to **Tcl_SaveInterpState** when the snapshot token was created.

**Tcl_DiscardInterpState** is called to release a **Tcl_InterpState** token previously returned by **Tcl_SaveInterpState** when that snapshot is not to be restored to an interp.

The **Tcl_InterpState** token returned by **Tcl_SaveInterpState** must eventually be passed to either **Tcl_RestoreInterpState** or **Tcl_DiscardInterpState** to avoid a memory leak.  Once the **Tcl_InterpState** token is passed to one of them, the token is no longer valid and should not be used anymore.

