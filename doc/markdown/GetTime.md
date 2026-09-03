---
CommandName: Tcl_GetTime
ManualSection: 3
Version: 8.4
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - clock(n)
 - timer(n)
Keywords:
 - date
 - time
Copyright:
 - Copyright (c) 2001 Kevin B. Kenny <kennykb@acm.org>.
---

# Name

Tcl\_GetMonotonicTime, Tcl\_GetTime, Tcl\_SetTimeProc, Tcl\_QueryTimeProc - get date and time

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[long long]{.ret} [Tcl\_GetMonotonicTime]{.ccmd}[]{.cargs}
[Tcl\_GetTime]{.ccmd}[timePtr]{.cargs}
[Tcl\_SetTimeProc]{.ccmd}[getProc, scaleProc, clientData]{.cargs}
[Tcl\_QueryTimeProc]{.ccmd}[getProcPtr, scaleProcPtr, clientDataPtr]{.cargs}
:::

# Arguments

::: {.arguments} :::

[\*timePtr]{.carg .out type="Tcl_Time"}
: Points to memory in which to store the date and time information.

[getProc]{.carg .in type="Tcl_GetTimeProc"}
: Pointer to handler function replacing **Tcl\_GetTime**'s access to the OS.

[scaleProc]{.carg .in type="Tcl_ScaleTimeProc"}
: Pointer to handler function for the conversion of time delays in the virtual domain to real-time.

[\*clientData]{.carg .in type="void"}
: Value passed through to the two handler functions.

[\*getProcPtr]{.carg .out type="Tcl_GetTimeProc"}
: Pointer to place the currently registered get handler function into.

[\*scaleProcPtr]{.carg .out type="Tcl_ScaleTimeProc"}
: Pointer to place the currently registered scale handler function into.

[\*\*clientDataPtr]{.carg .out type="void"}
: Pointer to place the currently registered pass-through value into.


:::

# Description

The **Tcl\_GetMonotonicTime** function returns the current monotonic time. The unit is micro-seconds. Only time differences are defined. The timer value 0 may be the epoc but may also be the start-up of the system.

The value is contiguous but may jump due to timer virtualization or sleep mode.

The precision is system dependent and not guaranteed to be micro-second precise.

The **Tcl\_GetTime** function retrieves the current time as a *Tcl\_Time* structure in memory the caller provides.  This structure has the following definition:

```
typedef struct {
    long long sec;
    long usec;
} Tcl_Time;
```

On return, the *sec* member of the structure is filled in with the number of seconds that have elapsed since the *epoch:* the epoch is the point in time of 00:00 UTC, 1 January 1970.  This number does *not* count leap seconds; an interval of one day advances it by 86400 seconds regardless of whether a leap second has been inserted.

The *usec* member of the structure is filled in with the number of microseconds that have elapsed since the start of the second designated by *sec*.

This command uses the system wall clock and is not contiguous by definition. It may have large jumps in short time, specially if the system is awaked from sleep mode.

## Virtualized time

The **Tcl\_SetTimeProc** and **Tcl\_QueryTimeProc** are not supported since TCL 9.1. They call a [Tcl\_Panic][Panic].


[Panic]: Panic.md

