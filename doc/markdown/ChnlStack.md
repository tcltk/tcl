---
CommandName: Tcl_StackChannel
ManualSection: 3
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Notifier(3)
 - Tcl_CreateChannel(3)
 - Tcl_OpenFileChannel(3)
 - vwait(n)
Keywords:
 - channel
 - compression
Copyright:
 - Copyright (c) 1999-2000 Ajuba Solutions.
---

# Name

Tcl_StackChannel, Tcl_UnstackChannel, Tcl_GetStackedChannel, Tcl_GetTopChannel - manipulate stacked I/O channels

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Channel]{.ret} [Tcl_StackChannel]{.ccmd}[interp, typePtr, clientData, mask, channel]{.cargs}
[int]{.ret} [Tcl_UnstackChannel]{.ccmd}[interp, channel]{.cargs}
[Tcl_Channel]{.ret} [Tcl_GetStackedChannel]{.ccmd}[channel]{.cargs}
[Tcl_Channel]{.ret} [Tcl_GetTopChannel]{.ccmd}[channel]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter for error reporting. .AP "const Tcl_ChannelType" *typePtr in The new channel I/O procedures to use for *channel*. .AP void *clientData in Arbitrary one-word value to pass to channel I/O procedures. .AP int mask in Conditions under which *channel* will be used: OR-ed combination of **TCL_READABLE**, **TCL_WRITABLE** and **TCL_EXCEPTION**. This can be a subset of the operations currently allowed on *channel*. .AP Tcl_Channel channel in An existing Tcl channel such as returned by **Tcl_CreateChannel**. 

# Description

These functions are for use by extensions that add processing layers to Tcl I/O channels.  Examples include compression and encryption modules.  These functions transparently stack and unstack a new channel on top of an existing one.  Any number of channels can be stacked together.

The **Tcl_ChannelType** version currently supported is **TCL_CHANNEL_VERSION_5**.  See **Tcl_CreateChannel** for details.

**Tcl_StackChannel** stacks a new *channel* on an existing channel with the same name that was registered for *channel* by **Tcl_RegisterChannel**.

**Tcl_StackChannel** works by creating a new channel structure and placing itself on top of the channel stack.  EOL translation, encoding and buffering options are shared between all channels in the stack.  The hidden channel does no buffering, newline translations, or character set encoding. Instead, the buffering, newline translations, and encoding functions all remain at the top of the channel stack.  A pointer to the new top channel structure is returned.  If an error occurs when stacking the channel, NULL is returned instead.

The *mask* parameter specifies the operations that are allowed on the new channel.  These can be a subset of the operations allowed on the original channel.  For example, a read-write channel may become read-only after the **Tcl_StackChannel** call.

Closing a channel closes the channels stacked below it.  The close of stacked channels is executed in a way that allows buffered data to be properly flushed.

**Tcl_UnstackChannel** reverses the process.  The old channel is associated with the channel name, and the processing module added by **Tcl_StackChannel** is destroyed.  If there is no old channel, then **Tcl_UnstackChannel** is equivalent to **Tcl_Close**.  If an error occurs unstacking the channel, **TCL_ERROR** is returned, otherwise **TCL_OK** is returned.

**Tcl_GetTopChannel** returns the top channel in the stack of channels the supplied channel is part of.

**Tcl_GetStackedChannel** returns the channel in the stack of channels which is just below the supplied channel. 

