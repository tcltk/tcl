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

In its simplest usage, the \fBreturn\fR command is used without options in the body of a procedure to immediately return control to the caller of the procedure.  If a \fIresult\fR argument is provided, its value becomes the result of the procedure passed back to the caller. If \fIresult\fR is not specified then an empty string will be returned to the caller as the result of the procedure.

The \fBreturn\fR command serves a similar function within script files that are evaluated by the \fBsource\fR command.  When \fBsource\fR evaluates the contents of a file as a script, an invocation of the \fBreturn\fR command will cause script evaluation to immediately cease, and the value \fIresult\fR (or an empty string) will be returned as the result of the \fBsource\fR command.

# Exceptional return codes

In addition to the result of a procedure, the return code of a procedure may also be set by \fBreturn\fR through use of the \fB-code\fR option. In the usual case where the \fB-code\fR option is not specified the procedure will return normally. However, the \fB-code\fR option may be used to generate an exceptional return from the procedure. \fICode\fR may have any of the following values:

**ok** (or \fB0\fR)
: Normal return:  same as if the option is omitted.  The return code of the procedure is 0 (\fBTCL_OK\fR).

**error** (or \fB1\fR)
: Error return: the return code of the procedure is 1 (\fBTCL_ERROR\fR). The procedure command behaves in its calling context as if it were the command \fBerror\fR \fIresult\fR.  See below for additional options.

**return** (or \fB2\fR)
: The return code of the procedure is 2 (\fBTCL_RETURN\fR).  The procedure command behaves in its calling context as if it were the command \fBreturn\fR (with no arguments).

**break** (or \fB3\fR)
: The return code of the procedure is 3 (\fBTCL_BREAK\fR).  The procedure command behaves in its calling context as if it were the command \fBbreak\fR.

**continue** (or \fB4\fR)
: The return code of the procedure is 4 (\fBTCL_CONTINUE\fR).  The procedure command behaves in its calling context as if it were the command \fBcontinue\fR.

*value*
: *Value* must be an integer;  it will be returned as the return code for the current procedure. Applications and packages should use values in the range 5 to 1073741823 (0x3fffffff) for their own purposes. Values outside this range are reserved for use by Tcl. .LP When a procedure wants to signal that it has received invalid arguments from its caller, it may use \fBreturn -code error\fR with \fIresult\fR set to a suitable error message.  Otherwise usage of the \fBreturn -code\fR option is mostly limited to procedures that implement a new control structure.


The \fBreturn -code\fR command acts similarly within script files that are evaluated by the \fBsource\fR command.  During the evaluation of the contents of a file as a script by \fBsource\fR, an invocation of the \fBreturn -code\fR \fIcode\fR command will cause the return code of \fBsource\fR to be \fIcode\fR.

# Return options

In addition to a result and a return code, evaluation of a command in Tcl also produces a dictionary of return options.  In general usage, all \fIoption value\fR pairs given as arguments to \fBreturn\fR become entries in the return options dictionary, and any values at all are acceptable except as noted below.  The \fBcatch\fR command may be used to capture all of this information \(em the return code, the result, and the return options dictionary \(em that arise from evaluation of a script.

As documented above, the \fB-code\fR entry in the return options dictionary receives special treatment by Tcl.  There are other return options also recognized and treated specially by Tcl.  They are:

**-errorcode** \fIlist\fR
: The \fB-errorcode\fR option receives special treatment only when the value of the \fB-code\fR option is \fBTCL_ERROR\fR.  Then the \fIlist\fR value is meant to be additional information about the error, presented as a Tcl list for further processing by programs. If no \fB-errorcode\fR option is provided to \fBreturn\fR when the \fB-code error\fR option is provided, Tcl will set the value of the \fB-errorcode\fR entry in the return options dictionary to the default value of \fBNONE\fR.  The \fB-errorcode\fR return option will also be stored in the global variable \fBerrorCode\fR.

**-errorinfo** \fIinfo\fR
: The \fB-errorinfo\fR option receives special treatment only when the value of the \fB-code\fR option is \fBTCL_ERROR\fR.  Then \fIinfo\fR is the initial stack trace, meant to provide to a human reader additional information about the context in which the error occurred.  The stack trace will also be stored in the global variable \fBerrorInfo\fR. If no \fB-errorinfo\fR option is provided to \fBreturn\fR when the \fB-code error\fR option is provided, Tcl will provide its own initial stack trace value in the entry for \fB-errorinfo\fR.  Tcl's initial stack trace will include only the call to the procedure, and stack unwinding will append information about higher stack levels, but there will be no information about the context of the error within the procedure.  Typically the \fIinfo\fR value is supplied from the value of \fB-errorinfo\fR in a return options dictionary captured by the \fBcatch\fR command (or from the copy of that information stored in the global variable \fBerrorInfo\fR).

**-errorstack** \fIlist\fR
: The \fB-errorstack\fR option receives special treatment only when the value of the \fB-code\fR option is \fBTCL_ERROR\fR.  Then \fIlist\fR is the initial error stack, recording actual argument values passed to each proc level. The error stack will also be reachable through \fBinfo errorstack\fR. If no \fB-errorstack\fR option is provided to \fBreturn\fR when the \fB-code error\fR option is provided, Tcl will provide its own initial error stack in the entry for \fB-errorstack\fR.  Tcl's initial error stack will include only the call to the procedure, and stack unwinding will append information about higher stack levels, but there will be no information about the context of the error within the procedure.  Typically the \fIlist\fR value is supplied from the value of \fB-errorstack\fR in a return options dictionary captured by the \fBcatch\fR command (or from the copy of that information from \fBinfo errorstack\fR).

**-level** \fIlevel\fR
: The \fB-level\fR and \fB-code\fR options work together to set the return code to be returned by one of the commands currently being evaluated. The \fIlevel\fR value must be a non-negative integer representing a number of levels on the call stack.  It defines the number of levels up the stack at which the return code of a command currently being evaluated should be \fIcode\fR.  If no \fB-level\fR option is provided, the default value of \fIlevel\fR is 1, so that \fBreturn\fR sets the return code that the current procedure returns to its caller, 1 level up the call stack.  The mechanism by which these options work is described in more detail below.

**-options** \fIoptions\fR
: The value \fIoptions\fR must be a valid dictionary.  The entries of that dictionary are treated as additional \fIoption value\fR pairs for the \fBreturn\fR command.


# Return code handling mechanisms

Return codes are used in Tcl to control program flow.  A Tcl script is a sequence of Tcl commands.  So long as each command evaluation returns a return code of \fBTCL_OK\fR, evaluation will continue to the next command in the script.  Any exceptional return code (non-\fBTCL_OK\fR) returned by a command evaluation causes the flow on to the next command to be interrupted.  Script evaluation ceases, and the exceptional return code from the command becomes the return code of the full script evaluation.  This is the mechanism by which errors during script evaluation cause an interruption and unwinding of the call stack.  It is also the mechanism by which commands like \fBbreak\fR, \fBcontinue\fR, and \fBreturn\fR cause script evaluation to terminate without evaluating all commands in sequence.

Some of Tcl's built-in commands evaluate scripts as part of their functioning.  These commands can make use of exceptional return codes to enable special features.  For example, the built-in Tcl commands that provide loops \(em such as \fBwhile\fR, \fBfor\fR, and \fBforeach\fR \(em evaluate a script that is the body of the loop.  If evaluation of the loop body returns the return code of \fBTCL_BREAK\fR or \fBTCL_CONTINUE\fR, the loop command can react in such a way as to give the \fBbreak\fR and \fBcontinue\fR commands their documented interpretation in loops.

Procedure invocation also involves evaluation of a script, the body of the procedure.  Procedure invocation provides special treatment when evaluation of the procedure body returns the return code \fBTCL_RETURN\fR.  In that circumstance, the \fB-level\fR entry in the return options dictionary is decremented.  If after decrementing, the value of the \fB-level\fR entry is 0, then the value of the \fB-code\fR entry becomes the return code of the procedure. If after decrementing, the value of the \fB-level\fR entry is greater than zero, then the return code of the procedure is \fBTCL_RETURN\fR.  If the procedure invocation occurred during the evaluation of the body of another procedure, the process will repeat itself up the call stack, decrementing the value of the \fB-level\fR entry at each level, so that the \fIcode\fR will be the return code of the current command \fIlevel\fR levels up the call stack.  The \fBsource\fR command performs the same handling of the \fBTCL_RETURN\fR return code, which explains the similarity of \fBreturn\fR invocation during a \fBsource\fR to \fBreturn\fR invocation within a procedure.

The return code of the \fBreturn\fR command itself triggers this special handling by procedure invocation.  If \fBreturn\fR is provided the option \fB-level 0\fR, then the return code of the \fBreturn\fR command itself will be the value \fIcode\fR of the \fB-code\fR option (or \fBTCL_OK\fR by default).  Any other value for the \fB-level\fR option (including the default value of 1) will cause the return code of the \fBreturn\fR command itself to be \fBTCL_RETURN\fR, triggering a return from the enclosing procedure.

# Examples

First, a simple example of using \fBreturn\fR to return from a procedure, interrupting the procedure body.

```
proc printOneLine {} {
    puts "line 1"    ;# This line will be printed.
    return
    puts "line 2"    ;# This line will not be printed.
}
```

Next, an example of using \fBreturn\fR to set the value returned by the procedure.

```
proc returnX {} {return X}
puts [returnX]    ;# prints "X"
```

Next, a more complete example, using \fBreturn -code error\fR to report invalid arguments.

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

Next, a procedure replacement for \fBbreak\fR.

```
proc myBreak {} {
    return -code break
}
```

With the \fB-level 0\fR option, \fBreturn\fR itself can serve as a replacement for \fBbreak\fR, with the help of \fBinterp alias\fR.

```
interp alias {} Break {} return -level 0 -code break
```

An example of using \fBcatch\fR and \fBreturn -options\fR to re-raise a caught error:

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

Finally an example of advanced use of the \fBreturn\fR options to create a procedure replacement for \fBreturn\fR itself:

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

