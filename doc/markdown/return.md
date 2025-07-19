---
CommandName: return
ManualSection: n
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - break(n)
 - catch(n)
 - continue(n)
 - dict(n)
 - error(n)
 - errorCode(n)
 - errorInfo(n)
 - proc(n)
 - source(n)
 - throw(n)
 - try(n)
Keywords:
 - break
 - catch
 - continue
 - error
 - exception
 - procedure
 - result
 - return
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

return - Return from a procedure, or set return code of a script

# Synopsis

::: {.synopsis} :::
[return]{.cmd} [result]{.optarg}
[return]{.cmd} [[-code]{.lit} [code]{.arg}]{.optarg} [result]{.optarg}
[return]{.cmd} [option value= ...? ?+result]{.optarg}
:::

# Description

In its simplest usage, the **return** command is used without options in the body of a procedure to immediately return control to the caller of the procedure.  If a *result* argument is provided, its value becomes the result of the procedure passed back to the caller. If *result* is not specified then an empty string will be returned to the caller as the result of the procedure.

The **return** command serves a similar function within script files that are evaluated by the **source** command.  When **source** evaluates the contents of a file as a script, an invocation of the **return** command will cause script evaluation to immediately cease, and the value *result* (or an empty string) will be returned as the result of the **source** command.

# Exceptional return codes

In addition to the result of a procedure, the return code of a procedure may also be set by **return** through use of the **-code** option. In the usual case where the **-code** option is not specified the procedure will return normally. However, the **-code** option may be used to generate an exceptional return from the procedure. *Code* may have any of the following values:

**ok** (or **0**)
: Normal return:  same as if the option is omitted.  The return code of the procedure is 0 (**TCL_OK**).

**error** (or **1**)
: Error return: the return code of the procedure is 1 (**TCL_ERROR**). The procedure command behaves in its calling context as if it were the command **error** *result*.  See below for additional options.

**return** (or **2**)
: The return code of the procedure is 2 (**TCL_RETURN**).  The procedure command behaves in its calling context as if it were the command **return** (with no arguments).

**break** (or **3**)
: The return code of the procedure is 3 (**TCL_BREAK**).  The procedure command behaves in its calling context as if it were the command **break**.

**continue** (or **4**)
: The return code of the procedure is 4 (**TCL_CONTINUE**).  The procedure command behaves in its calling context as if it were the command **continue**.

*value*
: *Value* must be an integer;  it will be returned as the return code for the current procedure. Applications and packages should use values in the range 5 to 1073741823 (0x3fffffff) for their own purposes. Values outside this range are reserved for use by Tcl. .LP When a procedure wants to signal that it has received invalid arguments from its caller, it may use **return -code error** with *result* set to a suitable error message.  Otherwise usage of the **return -code** option is mostly limited to procedures that implement a new control structure.


The **return -code** command acts similarly within script files that are evaluated by the **source** command.  During the evaluation of the contents of a file as a script by **source**, an invocation of the **return -code** *code* command will cause the return code of **source** to be *code*.

# Return options

In addition to a result and a return code, evaluation of a command in Tcl also produces a dictionary of return options.  In general usage, all *option value* pairs given as arguments to **return** become entries in the return options dictionary, and any values at all are acceptable except as noted below.  The **catch** command may be used to capture all of this information \(em the return code, the result, and the return options dictionary \(em that arise from evaluation of a script.

As documented above, the **-code** entry in the return options dictionary receives special treatment by Tcl.  There are other return options also recognized and treated specially by Tcl.  They are:

**-errorcode** *list*
: The **-errorcode** option receives special treatment only when the value of the **-code** option is **TCL_ERROR**.  Then the *list* value is meant to be additional information about the error, presented as a Tcl list for further processing by programs. If no **-errorcode** option is provided to **return** when the **-code error** option is provided, Tcl will set the value of the **-errorcode** entry in the return options dictionary to the default value of **NONE**.  The **-errorcode** return option will also be stored in the global variable **errorCode**.

**-errorinfo** *info*
: The **-errorinfo** option receives special treatment only when the value of the **-code** option is **TCL_ERROR**.  Then *info* is the initial stack trace, meant to provide to a human reader additional information about the context in which the error occurred.  The stack trace will also be stored in the global variable **errorInfo**. If no **-errorinfo** option is provided to **return** when the **-code error** option is provided, Tcl will provide its own initial stack trace value in the entry for **-errorinfo**.  Tcl's initial stack trace will include only the call to the procedure, and stack unwinding will append information about higher stack levels, but there will be no information about the context of the error within the procedure.  Typically the *info* value is supplied from the value of **-errorinfo** in a return options dictionary captured by the **catch** command (or from the copy of that information stored in the global variable **errorInfo**).

**-errorstack** *list*
: The **-errorstack** option receives special treatment only when the value of the **-code** option is **TCL_ERROR**.  Then *list* is the initial error stack, recording actual argument values passed to each proc level. The error stack will also be reachable through **info errorstack**. If no **-errorstack** option is provided to **return** when the **-code error** option is provided, Tcl will provide its own initial error stack in the entry for **-errorstack**.  Tcl's initial error stack will include only the call to the procedure, and stack unwinding will append information about higher stack levels, but there will be no information about the context of the error within the procedure.  Typically the *list* value is supplied from the value of **-errorstack** in a return options dictionary captured by the **catch** command (or from the copy of that information from **info errorstack**).

**-level** *level*
: The **-level** and **-code** options work together to set the return code to be returned by one of the commands currently being evaluated. The *level* value must be a non-negative integer representing a number of levels on the call stack.  It defines the number of levels up the stack at which the return code of a command currently being evaluated should be *code*.  If no **-level** option is provided, the default value of *level* is 1, so that **return** sets the return code that the current procedure returns to its caller, 1 level up the call stack.  The mechanism by which these options work is described in more detail below.

**-options** *options*
: The value *options* must be a valid dictionary.  The entries of that dictionary are treated as additional *option value* pairs for the **return** command.


# Return code handling mechanisms

Return codes are used in Tcl to control program flow.  A Tcl script is a sequence of Tcl commands.  So long as each command evaluation returns a return code of **TCL_OK**, evaluation will continue to the next command in the script.  Any exceptional return code (non-**TCL_OK**) returned by a command evaluation causes the flow on to the next command to be interrupted.  Script evaluation ceases, and the exceptional return code from the command becomes the return code of the full script evaluation.  This is the mechanism by which errors during script evaluation cause an interruption and unwinding of the call stack.  It is also the mechanism by which commands like **break**, **continue**, and **return** cause script evaluation to terminate without evaluating all commands in sequence.

Some of Tcl's built-in commands evaluate scripts as part of their functioning.  These commands can make use of exceptional return codes to enable special features.  For example, the built-in Tcl commands that provide loops \(em such as **while**, **for**, and **foreach** \(em evaluate a script that is the body of the loop.  If evaluation of the loop body returns the return code of **TCL_BREAK** or **TCL_CONTINUE**, the loop command can react in such a way as to give the **break** and **continue** commands their documented interpretation in loops.

Procedure invocation also involves evaluation of a script, the body of the procedure.  Procedure invocation provides special treatment when evaluation of the procedure body returns the return code **TCL_RETURN**.  In that circumstance, the **-level** entry in the return options dictionary is decremented.  If after decrementing, the value of the **-level** entry is 0, then the value of the **-code** entry becomes the return code of the procedure. If after decrementing, the value of the **-level** entry is greater than zero, then the return code of the procedure is **TCL_RETURN**.  If the procedure invocation occurred during the evaluation of the body of another procedure, the process will repeat itself up the call stack, decrementing the value of the **-level** entry at each level, so that the *code* will be the return code of the current command *level* levels up the call stack.  The **source** command performs the same handling of the **TCL_RETURN** return code, which explains the similarity of **return** invocation during a **source** to **return** invocation within a procedure.

The return code of the **return** command itself triggers this special handling by procedure invocation.  If **return** is provided the option **-level 0**, then the return code of the **return** command itself will be the value *code* of the **-code** option (or **TCL_OK** by default).  Any other value for the **-level** option (including the default value of 1) will cause the return code of the **return** command itself to be **TCL_RETURN**, triggering a return from the enclosing procedure.

# Examples

First, a simple example of using **return** to return from a procedure, interrupting the procedure body.

```
proc printOneLine {} {
    puts "line 1"    ;# This line will be printed.
    return
    puts "line 2"    ;# This line will not be printed.
}
```

Next, an example of using **return** to set the value returned by the procedure.

```
proc returnX {} {return X}
puts [returnX]    ;# prints "X"
```

Next, a more complete example, using **return -code error** to report invalid arguments.

```
proc factorial {n} {
    if {![string is integer $n] || ($n < 0)} {
        return -code error \
                "expected non-negative integer,\
                but got \"$n\""
    }
    if {$n < 2} {
        return 1
    }
    set factor [factorial [expr {$n - 1}]]
    set product [expr {$n * $factor}]
    return $product
}
```

Next, a procedure replacement for **break**.

```
proc myBreak {} {
    return -code break
}
```

With the **-level 0** option, **return** itself can serve as a replacement for **break**, with the help of **interp alias**.

```
interp alias {} Break {} return -level 0 -code break
```

An example of using **catch** and **return -options** to re-raise a caught error:

```
proc doSomething {} {
    set resource [allocate]
    catch {
        # Long script of operations
        # that might raise an error
    } result options
    deallocate $resource
    return -options $options $result
}
```

Finally an example of advanced use of the **return** options to create a procedure replacement for **return** itself:

```
proc myReturn {args} {
    set result ""
    if {[llength $args] % 2} {
        set result [lindex $args end]
        set args [lrange $args 0 end-1]
    }
    set options [dict merge {-level 1} $args]
    dict incr options -level
    return -options $options $result
}
```

