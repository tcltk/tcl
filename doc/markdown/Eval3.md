---
CommandName: Tcl_Eval
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - execute
 - file
 - global
 - result
 - script
 - value
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
 - Copyright (c) 2000 Scriptics Corporation.
---

# Name

Tcl\_EvalObjEx, Tcl\_EvalFile, Tcl\_EvalObjv, Tcl\_Eval, Tcl\_EvalEx, Tcl\_GlobalEval, Tcl\_GlobalEvalObj, Tcl\_VarEval - execute Tcl scripts

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_EvalObjEx]{.ccmd}[interp, objPtr, flags]{.cargs}
[int]{.ret} [Tcl\_EvalFile]{.ccmd}[interp, fileName]{.cargs}
[int]{.ret} [Tcl\_EvalObjv]{.ccmd}[interp, objc, objv, flags]{.cargs}
[int]{.ret} [Tcl\_Eval]{.ccmd}[interp, script]{.cargs}
[int]{.ret} [Tcl\_EvalEx]{.ccmd}[interp, script, numBytes, flags]{.cargs}
[int]{.ret} [Tcl\_GlobalEval]{.ccmd}[interp, script]{.cargs}
[int]{.ret} [Tcl\_GlobalEvalObj]{.ccmd}[interp, objPtr]{.cargs}
[int]{.ret} [Tcl\_VarEval]{.ccmd}[interp, part, part, ...= §(char \*)NULL]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter in which to execute the script.  The interpreter's result is modified to hold the result or error message from the script. .AP Tcl\_Obj \*objPtr in A Tcl value containing the script to execute. .AP int flags in OR'ed combination of flag bits that specify additional options. **TCL\_EVAL\_GLOBAL** and **TCL\_EVAL\_DIRECT** are currently supported. .AP "const char" \*fileName in Name of a file containing a Tcl script. .AP Tcl\_Size objc in The number of values in the array pointed to by *objv*; this is also the number of words in the command. .AP Tcl\_Obj \*\*objv in Points to an array of pointers to values; each value holds the value of a single word in the command to execute. .AP int numBytes in The number of bytes in *script*, not including any null terminating character.  If -1, then all characters up to the first null byte are used. .AP "const char" \*script in Points to first byte of script to execute (null-terminated TUTF-8 byte sequence). .AP "const char" \*part in String forming part of a Tcl script. 

# Description

N.B. Refer to the **Tcl\_UniChar** documentation page for a description of the *TUTF-8* encoding and related terms referenced here.

The procedures described here are invoked to execute Tcl scripts in various forms. **Tcl\_EvalObjEx** is the core procedure and is used by many of the others. It executes the commands in the script stored in *objPtr* until either an error occurs or the end of the script is reached. If this is the first time *objPtr* has been executed, its commands are compiled into bytecode instructions which are then executed.  The bytecodes are saved in *objPtr* so that the compilation step can be skipped if the value is evaluated again in the future.

The return value from **Tcl\_EvalObjEx** (and all the other procedures described here) is a Tcl completion code with one of the values **TCL\_OK**, **TCL\_ERROR**, **TCL\_RETURN**, **TCL\_BREAK**, or **TCL\_CONTINUE**, or possibly some other integer value originating in an extension. In addition, a result value or error message is left in *interp*'s result; it can be retrieved using **Tcl\_GetObjResult**.

**Tcl\_EvalFile** reads the file given by *fileName* and evaluates its contents as a Tcl script.  It returns the same information as **Tcl\_EvalObjEx**. If the file could not be read then a Tcl error is returned to describe why the file could not be read. The eofchar for files is "\\x1A" (^Z) for all platforms. If you require a "^Z" in code for string comparison, you can use "\\x1A", which will be safely substituted by the Tcl interpreter into "^Z".

**Tcl\_EvalObjv** executes a single preparsed command instead of a script.  The *objc* and *objv* arguments contain the values of the words for the Tcl command, one word in each value in *objv*.  **Tcl\_EvalObjv** evaluates the command and returns a completion code and result just like **Tcl\_EvalObjEx**. The caller of **Tcl\_EvalObjv** has to manage the reference count of the elements of *objv*, insuring that the values are valid until **Tcl\_EvalObjv** returns.

**Tcl\_Eval** is similar to **Tcl\_EvalObjEx** except that the script to be executed is supplied as a string instead of a value and no compilation occurs.  The string should be a proper TUTF-8 byte sequence as converted by **Tcl\_ExternalToUtfDString** or **Tcl\_ExternalToUtf** when it is known to possibly contain upper ASCII characters whose possible combinations might be a UTF-8 special code.  The string is parsed and executed directly (using **Tcl\_EvalObjv**) instead of compiling it and executing the bytecodes.  In situations where it is known that the script will never be executed again, **Tcl\_Eval** may be faster than **Tcl\_EvalObjEx**.  **Tcl\_Eval** returns a completion code and result just like **Tcl\_EvalObjEx**.

**Tcl\_EvalEx** is an extended version of **Tcl\_Eval** that takes additional arguments *numBytes* and *flags*.

**Tcl\_GlobalEval** and **Tcl\_GlobalEvalObj** are older procedures that are now deprecated.  They are similar to **Tcl\_EvalEx** and **Tcl\_EvalObjEx** except that the script is evaluated in the global namespace and its variable context consists of global variables only (it ignores any Tcl procedures that are active).  These functions are equivalent to using the **TCL\_EVAL\_GLOBAL** flag (see below).

**Tcl\_VarEval** takes any number of string arguments of any length, concatenates them into a single string, then calls **Tcl\_Eval** to execute that string as a Tcl command. It returns the result of the command and also modifies the interpreter result in the same way as **Tcl\_Eval**. The last argument to **Tcl\_VarEval** must be (char \*)NULL to indicate the end of arguments. 

# Flag bits

Any OR'ed combination of the following values may be used for the *flags* argument to procedures such as **Tcl\_EvalObjEx**:

**TCL\_EVAL\_DIRECT**
: This flag is only used by **Tcl\_EvalObjEx**; it is ignored by other procedures.  If this flag bit is set, the script is not compiled to bytecodes; instead it is executed directly as is done by **Tcl\_EvalEx**.  The **TCL\_EVAL\_DIRECT** flag is useful in situations where the contents of a value are going to change immediately, so the bytecodes will not be reused in a future execution.  In this case, it is faster to execute the script directly.

**TCL\_EVAL\_GLOBAL**
: If this flag is set, the script is evaluated in the global namespace instead of the current namespace and its variable context consists of global variables only (it ignores any Tcl procedures that are active). 


# Miscellaneous details

During the processing of a Tcl command it is legal to make nested calls to evaluate other commands (this is how procedures and some control structures are implemented). If a code other than **TCL\_OK** is returned from a nested **Tcl\_EvalObjEx** invocation, then the caller should normally return immediately, passing that same return code back to its caller, and so on until the top-level application is reached. A few commands, like [for], will check for certain return codes, like **TCL\_BREAK** and **TCL\_CONTINUE**, and process them specially without returning.

**Tcl\_EvalObjEx** keeps track of how many nested **Tcl\_EvalObjEx** invocations are in progress for *interp*. If a code of **TCL\_RETURN**, **TCL\_BREAK**, or **TCL\_CONTINUE** is about to be returned from the topmost **Tcl\_EvalObjEx** invocation for *interp*, it converts the return code to **TCL\_ERROR** and sets *interp*'s result to an error message indicating that the [return], [break], or [continue] command was invoked in an inappropriate place. This means that top-level applications should never see a return code from **Tcl\_EvalObjEx** other than **TCL\_OK** or **TCL\_ERROR**.

# Reference count management

**Tcl\_EvalObjEx** and **Tcl\_GlobalEvalObj** both increment and decrement the reference count of their *objPtr* argument; you must not pass them any value with a reference count of zero. They also manipulate the interpreter result; you must not count on the interpreter result to hold the reference count of any value over these calls.

**Tcl\_EvalObjv** may increment and decrement the reference count of any value passed via its *objv* argument; you must not pass any value with a reference count of zero. This function also manipulates the interpreter result; you must not count on the interpreter result to hold the reference count of any value over this call. 


[break]: break.md
[continue]: continue.md
[for]: for.md
[return]: return.md

