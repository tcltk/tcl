---
CommandName: trace
ManualSection: n
Version: 8.4
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - set(n)
 - unset(n)
Keywords:
 - read
 - command
 - rename
 - variable
 - write
 - trace
 - unset
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2000 Ajuba Solutions.
---

# Name

trace - Monitor variable accesses, command usages and command executions

# Synopsis

::: {.synopsis} :::
[trace]{.cmd} [option]{.arg} [arg arg]{.optdot}
:::

# Description

This command causes Tcl commands to be executed whenever certain operations are invoked.  The legal *option*s (which may be abbreviated) are:

**trace add** *type name ops* ?*args*?
: Where *type* is **command**, **execution**, or **variable**.

**trace add command** *name ops commandPrefix*
: Arrange for *commandPrefix* to be executed (with additional arguments) whenever command *name* is modified in one of the ways given by the list *ops*. *Name* will be resolved using the usual namespace resolution rules used by commands. If the command does not exist, an error will be thrown.

    *Ops* indicates which operations are of interest, and is a list of one or more of the following items:

**rename**
: Invoke *commandPrefix* whenever the traced command is renamed.  Note that renaming to the empty string is considered deletion, and will not be traced with "**rename**".

**delete**
: Invoke *commandPrefix* when the traced command is deleted. Commands can be deleted explicitly by using the **rename** command to rename the command to an empty string. Commands are also deleted when the interpreter is deleted, but traces will not be invoked because there is no interpreter in which to execute them.

    When the trace triggers, depending on the operations being traced, a number of arguments are appended to *commandPrefix* so that the actual command is as follows:

    ```
    commandPrefix oldName newName op
    ```
    *OldName* and *newName* give the traced command's current (old) name, and the name to which it is being renamed (the empty string if this is a "delete" operation). *Op* indicates what operation is being performed on the command, and is one of **rename** or **delete** as defined above.  The trace operation cannot be used to stop a command from being deleted.  Tcl will always remove the command once the trace is complete.  Recursive renaming or deleting will not cause further traces of the same type to be evaluated, so a delete trace which itself deletes the command, or a rename trace which itself renames the command will not cause further trace evaluations to occur. Both *oldName* and *newName* are fully qualified with any namespace(s) in which they appear.

**trace add execution** *name ops commandPrefix*
: Arrange for *commandPrefix* to be executed (with additional arguments) whenever command *name* is executed, with traces occurring at the points indicated by the list *ops*.  *Name* will be resolved using the usual namespace resolution rules used by commands.  If the command does not exist, an error will be thrown.
    *Ops* indicates which operations are of interest, and is a list of one or more of the following items:

**enter**
: Invoke *commandPrefix* whenever the command *name* is executed, just before the actual execution takes place.

**leave**
: Invoke *commandPrefix* whenever the command *name* is executed, just after the actual execution takes place.

**enterstep**
: Invoke *commandPrefix* for every Tcl command which is executed from the start of the execution of the procedure *name* until that procedure finishes. *CommandPrefix* is invoked just before the actual execution of the Tcl command being reported takes place.  For example if we have "proc foo {} { puts \"hello\" }", then an *enterstep* trace would be invoked just before "*puts \"hello\"*" is executed. Setting an *enterstep* trace on a command *name* that does not refer to a procedure will not result in an error and is simply ignored.

**leavestep**
: Invoke *commandPrefix* for every Tcl command which is executed from the start of the execution of the procedure *name* until that procedure finishes. *CommandPrefix* is invoked just after the actual execution of the Tcl command being reported takes place. Setting a *leavestep* trace on a command *name* that does not refer to a procedure will not result in an error and is simply ignored.

    When the trace triggers, depending on the operations being traced, a number of arguments are appended to *commandPrefix* so that the actual command is as follows:
    For **enter** and **enterstep** operations:

    ```
    commandPrefix command-string op
    ```
    *Command-string* gives the complete current command being executed (the traced command for a **enter** operation, an arbitrary command for a **enterstep** operation), including all arguments in their fully expanded form. *Op* indicates what operation is being performed on the command execution, and is one of **enter** or **enterstep** as defined above.  The trace operation can be used to stop the command from executing, by deleting the command in question.  Of course when the command is subsequently executed, an "invalid command" error will occur.
    For **leave** and **leavestep** operations:

    ```
    commandPrefix command-string code result op
    ```
    *Command-string* gives the complete current command being executed (the traced command for a **enter** operation, an arbitrary command for a **enterstep** operation), including all arguments in their fully expanded form. *Code* gives the result code of that execution, and *result* the result string. *Op* indicates what operation is being performed on the command execution, and is one of **leave** or **leavestep** as defined above.
    Note that the creation of many **enterstep** or **leavestep** traces can lead to unintuitive results, since the invoked commands from one trace can themselves lead to further command invocations for other traces.
    *CommandPrefix* executes in the same context as the code that invoked the traced operation: thus the *commandPrefix*, if invoked from a procedure, will have access to the same local variables as code in the procedure. This context may be different than the context in which the trace was created. If *commandPrefix* invokes a procedure (which it normally does) then the procedure will have to use **upvar** or **uplevel** commands if it wishes to access the local variables of the code which invoked the trace operation.
    While *commandPrefix* is executing during an execution trace, traces on *name* are temporarily disabled. This allows the *commandPrefix* to execute *name* in its body without invoking any other traces again. If an error occurs while executing the *commandPrefix*, then the command *name* as a whole will return that same error.
    When multiple traces are set on *name*, then for *enter* and *enterstep* operations, the traced commands are invoked in the reverse order of how the traces were originally created; and for *leave* and *leavestep* operations, the traced commands are invoked in the original order of creation.
    The behavior of execution traces is currently undefined for a command *name* imported into another namespace.

**trace add variable** *name ops commandPrefix*
: Arrange for *commandPrefix* to be executed whenever variable *name* is accessed in one of the ways given by the list *ops*.  *Name* may refer to a normal variable, an element of an array, or to an array as a whole (i.e. *name* may be just the name of an array, with no parenthesized index).  If *name* refers to a whole array, then *commandPrefix* is invoked whenever any element of the array is manipulated.  If the variable does not exist, it will be created but will not be given a value, so it will be visible to **namespace which** queries, but not to **info exists** queries.
    *Ops* indicates which operations are of interest, and is a list of one or more of the following items:

**array**
: Invoke *commandPrefix* whenever the variable is accessed or modified via the **array** command, provided that *name* is not a scalar variable at the time that the **array** command is invoked.  If *name* is a scalar variable, the access via the **array** command will not trigger the trace.

**read**
: Invoke *commandPrefix* whenever the variable is read.

**write**
: Invoke *commandPrefix* whenever the variable is written.

**unset**
: Invoke *commandPrefix* whenever the variable is unset.  Variables can be unset explicitly with the **unset** command, or implicitly when procedures return (all of their local variables are unset).  Variables are also unset when interpreters are deleted, but traces will not be invoked because there is no interpreter in which to execute them.

    When the trace triggers, three arguments are appended to *commandPrefix* so that the actual command is as follows:

    ```
    commandPrefix name1 name2 op
    ```
    *Name1* gives the name for the variable being accessed. This is not necessarily the same as the name used in the **trace add variable** command:  the **upvar** command allows a procedure to reference a variable under a different name. If the trace was originally set on an array or array element, *name2* provides which index into the array was affected. This information is present even when *name1* refers to a scalar, which may happen if the **upvar** command was used to create a reference to a single array element. If an entire array is being deleted and the trace was registered on the overall array, rather than a single element, then *name1* gives the array name and *name2* is an empty string. *Op* indicates what operation is being performed on the variable, and is one of **read**, **write**, or **unset** as defined above.
    *CommandPrefix* executes in the same context as the code that invoked the traced operation:  if the variable was accessed as part of a Tcl procedure, then *commandPrefix* will have access to the same local variables as code in the procedure.  This context may be different than the context in which the trace was created. If *commandPrefix* invokes a procedure (which it normally does) then the procedure will have to use **upvar** or **uplevel** if it wishes to access the traced variable.  Note also that *name1* may not necessarily be the same as the name used to set the trace on the variable; differences can occur if the access is made through a variable defined with the **upvar** command.
    For read and write traces, *commandPrefix* can modify the variable to affect the result of the traced operation.  If *commandPrefix* modifies the value of a variable during a read or write trace, then the new value will be returned as the result of the traced operation.  The return value from  *commandPrefix* is ignored except that if it returns an error of any sort then the traced operation also returns an error with the same error message returned by the trace command (this mechanism can be used to implement read-only variables, for example). For write traces, *commandPrefix* is invoked after the variable's value has been changed; it can write a new value into the variable to override the original value specified in the write operation.  To implement read-only variables, *commandPrefix* will have to restore the old value of the variable.
    While *commandPrefix* is executing during a read or write trace, traces on the variable are temporarily disabled.  This means that reads and writes invoked by *commandPrefix* will occur directly, without invoking *commandPrefix* (or any other traces) again.  However, if *commandPrefix* unsets the variable then unset traces will be invoked.
    When an unset trace is invoked, the variable has already been deleted: it will appear to be undefined with no traces.  If an unset occurs because of a procedure return, then the trace will be invoked in the variable context of the procedure being returned to:  the stack frame of the returning procedure will no longer exist.  Traces are not disabled during unset traces, so if an unset trace command creates a new trace and accesses the variable, the trace will be invoked.  Any errors in unset traces are ignored.
    If there are multiple traces on a variable they are invoked in order of creation, most-recent first.  If one trace returns an error, then no further traces are invoked for the variable.  If an array element has a trace set, and there is also a trace set on the array as a whole, the trace on the overall array is invoked before the one on the element.
    Once created, the trace remains in effect either until the trace is removed with the **trace remove variable** command described below, until the variable is unset, or until the interpreter is deleted. Unsetting an element of array will remove any traces on that element, but will not remove traces on the overall array.
    This command returns an empty string.

**trace remove** *type name opList commandPrefix*
: Where *type* is either **command**, **execution** or **variable**.

**trace remove command** *name opList commandPrefix*
: If there is a trace set on command *name* with the operations and command given by *opList* and *commandPrefix*, then the trace is removed, so that *commandPrefix* will never again be invoked.  Returns an empty string.   If *name* does not exist, the command will throw an error.

**trace remove execution** *name opList commandPrefix*
: If there is a trace set on command *name* with the operations and command given by *opList* and *commandPrefix*, then the trace is removed, so that *commandPrefix* will never again be invoked.  Returns an empty string.   If *name* does not exist, the command will throw an error.

**trace remove variable** *name opList commandPrefix*
: If there is a trace set on variable *name* with the operations and command given by *opList* and *commandPrefix*, then the trace is removed, so that *commandPrefix* will never again be invoked.  Returns an empty string.


**trace info** *type name*
: Where *type* is either **command**, **execution** or **variable**.

**trace info command** *name*
: Returns a list containing one element for each trace currently set on command *name*. Each element of the list is itself a list containing two elements, which are the *opList* and *commandPrefix* associated with the trace.  If *name* does not have any traces set, then the result of the command will be an empty string.  If *name* does not exist, the command will throw an error.

**trace info execution** *name*
: Returns a list containing one element for each trace currently set on command *name*. Each element of the list is itself a list containing two elements, which are the *opList* and *commandPrefix* associated with the trace.  If *name* does not have any traces set, then the result of the command will be an empty string.  If *name* does not exist, the command will throw an error.

**trace info variable** *name*
: Returns a list containing one element for each trace currently set on variable *name*.  Each element of the list is itself a list containing two elements, which are the *opList* and *commandPrefix* associated with the trace.  If *name* does not exist or does not have any traces set, then the result of the command will be an empty string.



# Examples

Print a message whenever either of the global variables **foo** and **bar** are updated, even if they have a different local name at the time (which can be done with the **upvar** command):

```
proc tracer {varname args} {
    upvar #0 $varname var
    puts "$varname was updated to be \"$var\""
}
trace add variable foo write "tracer foo"
trace add variable bar write "tracer bar"
```

Ensure that the global variable **foobar** always contains the product of the global variables **foo** and **bar**:

```
proc doMult args {
    global foo bar foobar
    set foobar [expr {$foo * $bar}]
}
trace add variable foo write doMult
trace add variable bar write doMult
```

Print a trace of what commands are executed during the processing of a Tcl procedure:

```
proc x {} { y }
proc y {} { z }
proc z {} { puts hello }
proc report args {puts [info level 0]}
trace add execution x enterstep report
x
  \(-> report y enterstep
    report z enterstep
    report {puts hello} enterstep
    hello
```

