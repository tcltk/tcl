---
CommandName: Tcl_SetChannelError
ManualSection: 3
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_Close(3)
 - Tcl_OpenFileChannel(3)
 - Tcl_SetErrno(3)
Keywords:
 - channel driver
 - error messages
 - channel type
Copyright:
 - Copyright (c) 2005 Andreas Kupries <andreas_kupries@users.sourceforge.net>
---

# Name

Tcl\_SetChannelError, Tcl\_SetChannelErrorInterp, Tcl\_GetChannelError, Tcl\_GetChannelErrorInterp - functions to create/intercept Tcl errors by channel drivers.

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_SetChannelError]{.ccmd}[chan, msg]{.cargs}
[Tcl\_SetChannelErrorInterp]{.ccmd}[interp, msg]{.cargs}
[Tcl\_GetChannelError]{.ccmd}[chan, msgPtr]{.cargs}
[Tcl\_GetChannelErrorInterp]{.ccmd}[interp, msgPtr]{.cargs}
:::

# Arguments

.AP Tcl\_Channel chan in Refers to the Tcl channel whose bypass area is accessed. .AP Tcl\_Interp\* interp in Refers to the Tcl interpreter whose bypass area is accessed. .AP Tcl\_Obj\* msg in Error message put into a bypass area. A list of return options and values, followed by a string message. Both message and the option/value information are optional. This *must* be a well-formed list. .AP Tcl\_Obj\*\* msgPtr out Reference to a place where the message stored in the accessed bypass area can be stored in.

# Description

The standard definition of a Tcl channel driver does not permit the direct return of arbitrary error messages, except for the setting and retrieval of channel options. All other functions are restricted to POSIX error codes.

The functions described here overcome this limitation. Channel drivers are allowed to use **Tcl\_SetChannelError** and **Tcl\_SetChannelErrorInterp** to place arbitrary error messages in *bypass areas* defined for channels and interpreters. And the generic I/O layer uses **Tcl\_GetChannelError** and **Tcl\_GetChannelErrorInterp** to look for messages in the bypass areas and arrange for their return as errors. The POSIX error codes set by a driver are used now if and only if no messages are present.

**Tcl\_SetChannelError** stores error information in the bypass area of the specified channel. The number of references to the **msg** value goes up by one. Previously stored information will be discarded, by releasing the reference held by the channel. The channel reference must not be NULL.

**Tcl\_SetChannelErrorInterp** stores error information in the bypass area of the specified interpreter. The number of references to the **msg** value goes up by one. Previously stored information will be discarded, by releasing the reference held by the interpreter. The interpreter reference must not be NULL.

**Tcl\_GetChannelError** places either the error message held in the bypass area of the specified channel into *msgPtr*, or NULL; and resets the bypass, that is, after an invocation all following invocations will return NULL, until an intervening invocation of **Tcl\_SetChannelError** with a non-NULL message. The *msgPtr* must not be NULL. The reference count of the message is not touched.  The reference previously held by the channel is now held by the caller of the function and it is its responsibility to release that reference when it is done with the value.

**Tcl\_GetChannelErrorInterp** places either the error message held in the bypass area of the specified interpreter into *msgPtr*, or NULL; and resets the bypass, that is, after an invocation all following invocations will return NULL, until an intervening invocation of **Tcl\_SetChannelErrorInterp** with a non-NULL message. The *msgPtr* must not be NULL. The reference count of the message is not touched.  The reference previously held by the interpreter is now held by the caller of the function and it is its responsibility to release that reference when it is done with the value.

Which functions of a channel driver are allowed to use which bypass function is listed below, as is which functions of the public channel API may leave a messages in the bypass areas.

**Tcl\_DriverInputProc**
: May use **Tcl\_SetChannelError**, and only this function.

**Tcl\_DriverOutputProc**
: May use **Tcl\_SetChannelError**, and only this function.

**Tcl\_DriverWideSeekProc**
: May use **Tcl\_SetChannelError**, and only this function.

**Tcl\_DriverSetOptionProc**
: Has already the ability to pass arbitrary error messages. Must *not* use any of the new functions.

**Tcl\_DriverGetOptionProc**
: Has already the ability to pass arbitrary error messages. Must *not* use any of the new functions.

**Tcl\_DriverWatchProc**
: Must *not* use any of the new functions. Is internally called and has no ability to return any type of error whatsoever.

**Tcl\_DriverBlockModeProc**
: May use **Tcl\_SetChannelError**, and only this function.

**Tcl\_DriverGetHandleProc**
: Must *not* use any of the new functions. It is only a low-level function, and not used by Tcl commands.

**Tcl\_DriverHandlerProc**
: Must *not* use any of the new functions. Is internally called and has no ability to return any type of error whatsoever.


Given the information above the following public functions of the Tcl C API are affected by these changes; when these functions are called, the channel may now contain a stored arbitrary error message requiring processing by the caller. .DS **Tcl\_Flush**	**Tcl\_GetsObj**	**Tcl\_Gets** **Tcl\_ReadChars**	**Tcl\_ReadRaw**	**Tcl\_Read** **Tcl\_Seek**	**Tcl\_StackChannel**	**Tcl\_Tell** **Tcl\_WriteChars**	**Tcl\_WriteObj**	**Tcl\_WriteRaw** **Tcl\_Write** .DE

All other API functions are unchanged. In particular, the functions below leave all their error information in the interpreter result. .DS **Tcl\_Close**	**Tcl\_UnstackChannel**	**Tcl\_UnregisterChannel** .DE

# Reference count management

The *msg* argument to **Tcl\_SetChannelError** and **Tcl\_SetChannelErrorInterp**, if not NULL, may have any reference count; these functions will copy.

**Tcl\_GetChannelError** and **Tcl\_GetChannelErrorInterp** write a value reference into their *msgPtr*, but do not manipulate its reference count. The reference count will be at least 1 (unless the reference is NULL).

