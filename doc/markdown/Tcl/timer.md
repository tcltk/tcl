---
CommandName: timer
title: timer
ManualSection: n
Version: 9.1
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - concat(n)
 - interp(n)
 - timer(n)
 - update(n)
 - vwait(n)
Keywords:
 - cancel
 - delay
 - idle callback
 - sleep
 - time
Copyright:
 - Copyright (c) 2025 The TCL Association
---

# Name

timer - timer events for monotonic and wall clock time

# Synopsis

::: {.synopsis} :::
[timer]{.cmd} [in]{.sub} [delay]{.arg} [unit]{.arg} [script]{.arg}
[timer]{.cmd} [at]{.sub} [timepoint]{.arg} [unit]{.arg} [script]{.arg}
[timer]{.cmd} [idle]{.sub} [script]{.arg}
[timer]{.cmd} [sleep]{.sub} [for]{.lit} [delay]{.arg} [unit]{.optarg}
[timer]{.cmd} [sleep]{.sub} [until]{.lit} [timepoint]{.arg} [unit]{.optarg}
[timer]{.cmd} [cancel]{.sub} [id]{.arg}
[timer]{.cmd} [info]{.sub} [id]{.optarg}
:::

# Description

This command is used to delay execution of the program or to execute a command in the background sometime in the future. Time may be specified by a monotonic time delay or by wall clock time point. It has the following methods:

[timer]{.cmd} [in]{.sub} [delay]{.arg} [unit]{.arg} [script]{.arg}
: In this form the command returns immediately, but it arranges for a Tcl command to be executed after the given monotonic time *delay* elapsed as an event handler. The *delay* value is given in the specified *unit*. See [Time units] below for available units. The command will be executed exactly once, after the given time delay. The delayed command is given by the argument *script*. The command will be executed at global level (outside the context of any Tcl procedure). If an error occurs while executing the delayed command then the background error will be reported by the command registered with [interp bgerror][interp]. The command returns an identifier that can be used to cancel the delayed command using **timer cancel** or [after cancel][after]. A *delay* value of 0 (or negative) queues the event immediately with priority over other event types (if not installed with an event proc, which will wait for next round of events).

    The command **after ms script** may also be used to register monotonic clock timer events. They are handled by the same queue as this form of the **timer** command.

[timer]{.cmd} [at]{.sub} [timepoint]{.arg} [unit]{.arg} [script]{.arg}
: Same as **timer in**, but specifying a wall clock time point (epoc). A typical use-case is to pass result of a [clock scan][clock] as time point. See example below.

    Wall clock events have second priority after monotonic clock events. If both fire the same instant, the monotonic clock command is always handled first.

[timer]{.cmd} [idle]{.sub} [script]{.arg}
: Arranges for the *script* to be evaluated later as an idle callback. The script will be run exactly once, the next time the event loop is entered and there are no events to process. The command returns an identifier that can be used to cancel the delayed command using **timer cancel** or [after cancel][after]. If an error occurs while executing the script then the background error will be reported by the command registered with [interp bgerror][interp].

[timer]{.cmd} [sleep]{.sub} [for]{.lit} [delay]{.arg} [unit]{.optarg}
: *delay* must be a wide integer giving a time in the given unit (default: **milliseconds**). A negative number is treated as 0. The command sleeps for the given monotonic delay and then returns. While the command is sleeping the application does not respond to events.

[timer]{.cmd} [sleep]{.sub} [until]{.lit} [timepoint]{.arg} [unit]{.optarg}
: *timepoint* must be a wide integer giving a wall clock time point in the given unit (default: **seconds**). A negative number is treated as 0. The command sleeps (at least) until the given wall clock time point and then returns. While the command is sleeping the application does not respond to events.

[after]{.cmd} [cancel]{.sub} [id]{.arg}
: Cancels the execution of a delayed command that was previously scheduled. *id* indicates which command should be canceled;  it must have been the return value from a previous **timer** or [after] command. If the command given by *id* has already been executed then the **timer cancel** command has no effect.

[timer]{.cmd} [info]{.sub} [id]{.optarg}
: This command returns information about existing event handlers. If no *id* argument is supplied, the command returns a list of the identifiers for all existing event handlers created by the **timer** and [after] command for this interpreter. If *id* is supplied, it specifies an existing handler; *id* must have been the return value from some previous call to **timer** or [after] and it must not have triggered yet or been canceled. In this case the command returns a list with two to three elements. The first element of the list is the script associated with *id*, and the second element is either **idle**, **monotonic** or **wallclock** to indicate what kind of event handler it is. A third list element is present for the kinds ** monotonic** or **wallclock**. This element is the scheduled execution time point in microseconds of the given clock type.


The **timer in**, **timer at** and **timer idle** forms of the command assume that the application is event driven:  the delayed commands will not be executed unless the application enters the event loop. In applications that are not normally event-driven, such as [tclsh], the event loop can be entered with the [vwait] and [update] commands.

Note that the output differs from [after info][after].

# Time units

Some forms of the command take a time unit. Available units are: **us** (for microseconds), **microseconds**, **ms** (for milliseconds), **milliseconds**, **s** (for seconds), **seconds** or any unique abbreviation.

# Time maximum value

The arguments *delay* and *timepoint* are bound to a resulting 63 bit clock value (in unit microseconds). Any higher value (which is after year 294441) will result in a **time too far away** error.

# Examples

This arranges for the command *wake\_up* to be run in eight hours wall clock time (providing the event loop is active at that time):

```
timer at [clock add [clock seconds] 8 hours] seconds wake_up
```


[after]: after.md
[clock]: clock.md
[interp]: interp.md
[tclsh]: tclsh.md
[update]: update.md
[vwait]: vwait.md

