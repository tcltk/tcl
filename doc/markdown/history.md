---
CommandName: history
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Keywords:
 - event
 - history
 - record
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

history - Manipulate the history list

# Synopsis

::: {.synopsis} :::
[history]{.cmd} [option]{.optarg} [arg arg]{.optdot}
:::

# Description

The **history** command performs one of several operations related to recently-executed commands recorded in a history list.  Each of these recorded commands is referred to as an "event". When specifying an event to the **history** command, the following forms may be used:

1. A number:  if positive, it refers to the event with that number (all events are numbered starting at 1).  If the number is negative, it selects an event relative to the current event (**-1** refers to the previous event, **-2** to the one before that, and so on).  Event **0** refers to the current event.

2. A string:  selects the most recent event that matches the string. An event is considered to match the string either if the string is the same as the first characters of the event, or if the string matches the event in the sense of the **string match** command.


The **history** command can take any of the following forms:

**history**
: Same as **history info**, described below.

**history add** *command* ?**exec**?
: Adds the *command* argument to the history list as a new event.  If **exec** is specified (or abbreviated) then the command is also executed and its result is returned.  If **exec** is not specified then an empty string is returned as result.

**history change** *newValue* ?*event*?
: Replaces the value recorded for an event with *newValue*.  *Event* specifies the event to replace, and defaults to the *current* event (not event **-1**).  This command is intended for use in commands that implement new forms of history substitution and wish to replace the current event (which invokes the substitution) with the command created through substitution.  The return value is an empty string.

**history clear**
: Erase the history list.  The current keep limit is retained. The history event numbers are reset.

**history event** ?*event*?
: Returns the value of the event given by *event*.  *Event* defaults to **-1**.

**history info** ?*count*?
: Returns a formatted string (intended for humans to read) giving the event number and contents for each of the events in the history list except the current event.  If *count* is specified then only the most recent *count* events are returned.

**history keep** ?*count*?
: This command may be used to change the size of the history list to *count* events.  Initially, 20 events are retained in the history list.  If *count* is not specified, the current keep limit is returned.

**history nextid**
: Returns the number of the next event to be recorded in the history list.  It is useful for things like printing the event number in command-line prompts.

**history redo** ?*event*?
: Re-executes the command indicated by *event* and returns its result. *Event* defaults to **-1**.  This command results in history revision:  see below for details.


# History revision

Pre-8.0 Tcl had a complex history revision mechanism. The current mechanism is more limited, and the old history operations **substitute** and **words** have been removed. (As a consolation, the **clear** operation was added.)

The history option **redo** results in much simpler "history revision". When this option is invoked then the most recent event is modified to eliminate the **history** command and replace it with the result of the **history** command. If you want to redo an event without modifying history, then use the **event** operation to retrieve some event, and the **add** operation to add it to history and execute it.

