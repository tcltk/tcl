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

Tcl_EvalObjEx, Tcl_EvalFile, Tcl_EvalObjv, Tcl_Eval, Tcl_EvalEx, Tcl_GlobalEval, Tcl_GlobalEvalObj, Tcl_VarEval - execute Tcl scripts

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_EvalObjEx]{.lit} [interp, objPtr, flags]{.arg}
[int]{.ret} [Tcl_EvalFile]{.lit} [interp, fileName]{.arg}
[int]{.ret} [Tcl_EvalObjv]{.lit} [interp, objc, objv, flags]{.arg}
[int]{.ret} [Tcl_Eval]{.lit} [interp, script]{.arg}
[int]{.ret} [Tcl_EvalEx]{.lit} [interp, script, numBytes, flags]{.arg}
[int]{.ret} [Tcl_GlobalEval]{.lit} [interp, script]{.arg}
[int]{.ret} [Tcl_GlobalEvalObj]{.lit} [interp, objPtr]{.arg}
[int]{.ret} [Tcl_VarEval]{.lit} [interp, part, part, ...= §(char *)NULL]{.arg}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter in which to execute the script.  The interpreter's result is modified to hold the result or error message from the script. .AP Tcl_Obj *objPtr in A Tcl value containing the script to execute. .AP int flags in OR'ed combination of flag bits that specify additional options. **TCL_EVAL_GLOBAL** and **TCL_EVAL_DIRECT** are currently supported. .AP "const char" *fileName in Name of a file containing a Tcl script. .AP Tcl_Size objc in The number of values in the array pointed to by *objv*; this is also the number of words in the command. .AP Tcl_Obj **objv in Points to an array of pointers to values; each value holds the value of a single word in the command to execute. .AP int numBytes in The number of bytes in *script*, not including any null terminating character.  If -1, then all characters up to the first null byte are used. .AP "const char" *script in Points to first byte of script to execute (null-terminated and UTF-8). .AP "const char" *part in String forming part of a Tcl script. 

# Description

The procedures described here are invoked to execute Tcl scripts in various forms. **Tcl_EvalObjEx** is the core procedure and is used by many of the others. It executes the commands in the script stored in *objPtr* until either an error occurs or the end of the script is reached. If this is the first time *objPtr* has been executed, its commands are compiled into bytecode instructions which are then executed.  The bytecodes are saved in *objPtr* so that the compilation step can be skipped if the value is evaluated again in the future.

The return value from **Tcl_EvalObjEx** (and all the other procedures described here) is a Tcl completion code with one of the values **TCL_OK**, **TCL_ERROR**, **TCL_RETURN**, **TCL_BREAK**, or **TCL_CONTINUE**, or possibly some other integer value originating in an extension. In addition, a result value or error message is left in *interp*'s result; it can be retrieved using **Tcl_GetObjResult**.

**Tcl_EvalFile** reads the file given by *fileName* and evaluates its contents as a Tcl script.  It returns the same information as **Tcl_EvalObjEx**. If the file could not be read then a Tcl error is returned to describe why the file could not be read. The eofchar for files is "\x1A" (^Z) for all platforms. If you require a "^Z" in code for string comparison, you can use "\x1A", which will be safely substituted by the Tcl interpreter into "^Z".

**Tcl_EvalObjv** executes a single preparsed command instead of a script.  The *objc* and *objv* arguments contain the values of the words for the Tcl command, one word in each value in *objv*.  **Tcl_EvalObjv** evaluates the command and returns a completion code and result just like **Tcl_EvalObjEx**. The caller of **Tcl_EvalObjv** has to manage the reference count of the elements of *objv*, insuring that the values are valid until **Tcl_EvalObjv** returns.

**Tcl_Eval** is similar to **Tcl_EvalObjEx** except that the script to be executed is supplied as a string instead of a value and no compilation occurs.  The string should be a proper UTF-8 string as converted by **Tcl_ExternalToUtfDString** or **Tcl_ExternalToUtf** when it is known to possibly contain upper ASCII characters whose possible combinations might be a UTF-8 special code.  The string is parsed and executed directly (using **Tcl_EvalObjv**) instead of compiling it and executing the bytecodes.  In situations where it is known that the script will never be executed again, **Tcl_Eval** may be faster than **Tcl_EvalObjEx**.  **Tcl_Eval** returns a completion code and result just like **Tcl_EvalObjEx**.

**Tcl_EvalEx** is an extended version of **Tcl_Eval** that takes additional arguments *numBytes* and *flags*.

**Tcl_GlobalEval** and **Tcl_GlobalEvalObj** are older procedures that are now deprecated.  They are similar to **Tcl_EvalEx** and **Tcl_EvalObjEx** except that the script is evaluated in the global namespace and its variable context consists of global variables only (it ignores any Tcl procedures that are active).  These functions are equivalent to using the **TCL_EVAL_GLOBAL** flag (see below).

**Tcl_VarEval** takes any number of string arguments of any length, concatenates them into a single string, then calls **Tcl_Eval** to execute that string as a Tcl command. It returns the result of the command and also modifies the interpreter result in the same way as **Tcl_Eval**. The last argument to **Tcl_VarEval** must be (char *)NULL to indicate the end of arguments. 

# Flag bits

Any OR'ed combination of the following values may be used for the *flags* argument to procedures such as **Tcl_EvalObjEx**:

**TCL_EVAL_DIRECT**
: This flag is only used by **Tcl_EvalObjEx**; it is ignored by other procedures.  If this flag bit is set, the script is not compiled to bytecodes; instead it is executed directly as is done by **Tcl_EvalEx**.  The **TCL_EVAL_DIRECT** flag is useful in situations where the contents of a value are going to change immediately, so the bytecodes will not be reused in a future execution.  In this case, it is faster to execute the script directly.

**TCL_EVAL_GLOBAL**
: If this flag is set, the script is evaluated in the global namespace instead of the current namespace and its variable context consists of global variables only (it ignores any Tcl procedures that are active). 


# Miscellaneous details

During the processing of a Tcl command it is legal to make nested calls to evaluate other commands (this is how procedures and some control structures are implemented). If a code other than **TCL_OK** is returned from a nested **Tcl_EvalObjEx** invocation, then the caller should normally return immediately, passing that same return code back to its caller, and so on until the top-level application is reached. A few commands, like **for**, will check for certain return codes, like **TCL_BREAK** and **TCL_CONTINUE**, and process them specially without returning.

**Tcl_EvalObjEx** keeps track of how many nested **Tcl_EvalObjEx** invocations are in progress for *interp*. If a code of **TCL_RETURN**, **TCL_BREAK**, or **TCL_CONTINUE** is about to be returned from the topmost **Tcl_EvalObjEx** invocation for *interp*, it converts the return code to **TCL_ERROR** and sets *interp*'s result to an error message indicating that the **return**, **break**, or **continue** command was invoked in an inappropriate place. This means that top-level applications should never see a return code from **Tcl_EvalObjEx** other than **TCL_OK** or **TCL_ERROR**.

# Reference count management

**Tcl_EvalObjEx** and **Tcl_GlobalEvalObj** both increment and decrement the reference count of their *objPtr* argument; you must not pass them any value with a reference count of zero. They also manipulate the interpreter result; you must not count on the interpreter result to hold the reference count of any value over these calls.

**Tcl_EvalObjv** may increment and decrement the reference count of any value passed via its *objv* argument; you must not pass any value with a reference count of zero. This function also manipulates the interpreter result; you must not count on the interpreter result to hold the reference count of any value over this call. 

