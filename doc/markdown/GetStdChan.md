---
CommandName: Tcl_GetStdChannel
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_Close(3)
 - Tcl_CreateChannel(3)
 - Tcl_Main(3)
 - tclsh(1)
Keywords:
 - standard channel
 - standard input
 - standard output
 - standard error
Copyright:
 - Copyright (c) 1996 Sun Microsystems, Inc.
---

# Name

Tcl\_GetStdChannel, Tcl\_SetStdChannel - procedures for retrieving and replacing the standard channels

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Channel]{.ret} [Tcl\_GetStdChannel]{.ccmd}[type]{.cargs}
[Tcl\_SetStdChannel]{.ccmd}[channel, type]{.cargs}
:::

# Arguments

.AP int type in The identifier for the standard channel to retrieve or modify.  Must be one of **TCL\_STDIN**, **TCL\_STDOUT**, or **TCL\_STDERR**. .AP Tcl\_Channel channel in The channel to use as the new value for the specified standard channel. 

# Description

Tcl defines three special channels that are used by various I/O related commands if no other channels are specified.  The standard input channel has a channel name of **stdin** and is used by [read] and [gets]. The standard output channel is named **stdout** and is used by [puts].  The standard error channel is named **stderr** and is used for reporting errors.  In addition, the standard channels are inherited by any child processes created using [exec] or [open] in the absence of any other redirections.

The standard channels are actually aliases for other normal channels.  The current channel associated with a standard channel can be retrieved by calling **Tcl\_GetStdChannel** with one of **TCL\_STDIN**, **TCL\_STDOUT**, or **TCL\_STDERR** as the *type*.  The return value will be a valid channel, or NULL.

A new channel can be set for the standard channel specified by *type* by calling **Tcl\_SetStdChannel** with a new channel or NULL in the *channel* argument.  If the specified channel is closed by a later call to **Tcl\_Close**, then the corresponding standard channel will automatically be set to NULL.

If a non-NULL value for *channel* is passed to **Tcl\_SetStdChannel**, then that same value should be passed to **Tcl\_RegisterChannel**, like so:

```
Tcl_RegisterChannel(NULL, channel);
```

This is a workaround for a misfeature in **Tcl\_SetStdChannel** that it fails to do some reference counting housekeeping.  This misfeature cannot be corrected without contradicting the assumptions of some existing code that calls **Tcl\_SetStdChannel**.

If **Tcl\_GetStdChannel** is called before **Tcl\_SetStdChannel**, Tcl will construct a new channel to wrap the appropriate platform-specific standard file handle.  If **Tcl\_SetStdChannel** is called before **Tcl\_GetStdChannel**, then the default channel will not be created.

If one of the standard channels is set to NULL, either by calling **Tcl\_SetStdChannel** with a NULL *channel* argument, or by calling **Tcl\_Close** on the channel, then the next call to **Tcl\_CreateChannel** will automatically set the standard channel with the newly created channel.  If more than one standard channel is NULL, then the standard channels will be assigned starting with standard input, followed by standard output, with standard error being last.

See **Tcl\_StandardChannels** for a general treatise about standard channels and the behavior of the Tcl library with regard to them. 


[exec]: exec.md
[gets]: gets.md
[open]: open.md
[puts]: puts.md
[read]: read.md

