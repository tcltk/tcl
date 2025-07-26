---
CommandName: Standard Channels
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_CreateChannel(3)
 - Tcl_RegisterChannel(3)
 - Tcl_GetChannel(3)
 - Tcl_GetStdChannel(3)
 - Tcl_SetStdChannel(3)
 - Tk_InitConsoleChannels(3)
 - tclsh(1)
 - wish(1)
 - Tcl_Main(3)
 - Tk_MainEx(3)
Keywords:
 - standard channels
Copyright:
 - Copyright (c) 2001 ActiveState Corporation
---

# Name

Tcl_StandardChannels - How the Tcl library deals with the standard channels 

# Description

This page explains the initialization and use of standard channels in the Tcl library.

The term *standard channels* comes out of the Unix world and refers to the three channels automatically opened by the OS for each new application. They are **stdin**, **stdout** and **stderr**. The first is the standard input an application can read from, the other two refer to writable channels, one for regular output and the other for error messages.

Tcl generalizes this concept in a cross-platform way and exposes standard channels to the script level.

## Application programming interfaces

The public API procedures dealing directly with standard channels are **Tcl_GetStdChannel** and **Tcl_SetStdChannel**. Additional public APIs to consider are **Tcl_RegisterChannel**, **Tcl_CreateChannel** and **Tcl_GetChannel**.

# Initialization of tcl standard channels

Standard channels are initialized by the Tcl library in three cases: when explicitly requested, when implicitly required before returning channel information, or when implicitly required during registration of a new channel.

These cases differ in how they handle unavailable platform- specific standard channels.  (A channel is not "available" if it could not be successfully opened; for example, in a Tcl application run as a Windows NT service.)

1. A single standard channel is initialized when it is explicitly specified in a call to **Tcl_SetStdChannel**.  The states of the other standard channels are unaffected.
Missing platform-specific standard channels do not matter here. This approach is not available at the script level.

2. All uninitialized standard channels are initialized to platform-specific default values:

(a)
: when open channels are listed with **Tcl_GetChannelNames** (or the **file channels** script command), or

(b)
: when information about any standard channel is requested with a call to **Tcl_GetStdChannel**, or with a call to **Tcl_GetChannel** which specifies one of the standard names (**stdin**, **stdout** and **stderr**).

In case of missing platform-specific standard channels, the Tcl standard channels are considered as initialized and then immediately closed. This means that the first three Tcl channels then opened by the application are designated as the Tcl standard channels.

3. All uninitialized standard channels are initialized to platform-specific default values when a user-requested channel is registered with **Tcl_RegisterChannel**.


In case of unavailable platform-specific standard channels the channel whose creation caused the initialization of the Tcl standard channels is made a normal channel.  The next three Tcl channels opened by the application are designated as the Tcl standard channels.  In other words, of the first four Tcl channels opened by the application the second to fourth are designated as the Tcl standard channels.

# Re-initialization of tcl standard channels

Once a Tcl standard channel is initialized through one of the methods above, closing this Tcl standard channel will cause the next call to **Tcl_CreateChannel** to make the new channel the new standard channel, too. If more than one Tcl standard channel was closed **Tcl_CreateChannel** will fill the empty slots in the order **stdin**, **stdout** and **stderr**.

**Tcl_CreateChannel** will not try to reinitialize an empty slot if that slot was not initialized before. It is this behavior which enables an application to employ method 1 of initialization, i.e. to create and designate their own Tcl standard channels.

# Shell-specific details

## Tclsh

The Tcl shell (or rather the function **Tcl_Main**, which forms the core of the shell's implementation) uses method 2 to initialize the standard channels.

## Wish

The windowing shell (or rather the function **Tk_MainEx**, which forms the core of the shell's implementation) uses method 1 to initialize the standard channels (See **Tk_InitConsoleChannels**) on non-Unix platforms.  On Unix platforms, **Tk_MainEx** implicitly uses method 2 to initialize the standard channels.

