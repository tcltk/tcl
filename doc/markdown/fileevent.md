---
CommandName: fileevent
ManualSection: n
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - chan(n)
Copyright:
 - Copyright (c) 1994 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2008 Pat Thoyts
---

# Name

fileevent - Execute a script when a channel becomes readable or writable

# Synopsis

::: {.synopsis} :::
[fileevent]{.cmd} [channel]{.arg} [readable]{.lit} [script]{.optarg}
[fileevent]{.cmd} [channel]{.arg} [writable]{.lit} [script]{.optarg}
:::

# Description

The \fBfileevent\fR command has been superceded by the \fBchan event\fR command which supports the same syntax and options.

