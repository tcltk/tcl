---
CommandName: Tcl_CreateCloseHandler
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - close(n)
 - Tcl_Close(3)
 - Tcl_UnregisterChannel(3)
Keywords:
 - callback
 - channel closing
Copyright:
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_CreateCloseHandler, Tcl\_DeleteCloseHandler - arrange for callbacks when channels are closed

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_CreateCloseHandler]{.ccmd}[channel, proc, clientData]{.cargs}
[Tcl\_DeleteCloseHandler]{.ccmd}[channel, proc, clientData]{.cargs}
:::

# Arguments

.AP Tcl\_Channel channel in The channel for which to create or delete a close callback. .AP Tcl\_CloseProc \*proc in The procedure to call as the callback. .AP void \*clientData in Arbitrary one-word value to pass to *proc*.

# Description

**Tcl\_CreateCloseHandler** arranges for *proc* to be called when *channel* is closed with **Tcl\_Close** or **Tcl\_UnregisterChannel**, or using the Tcl [close] command. *Proc* should match the following prototype:

```
typedef void Tcl_CloseProc(
        void *clientData);
```

The *clientData* is the same as the value provided in the call to **Tcl\_CreateCloseHandler**.

**Tcl\_DeleteCloseHandler** removes a close callback for *channel*. The *proc* and *clientData* identify which close callback to remove; **Tcl\_DeleteCloseHandler** does nothing if its *proc* and *clientData* arguments do not match the *proc* and *clientData* for a  close handler for *channel*.


[close]: close.md

