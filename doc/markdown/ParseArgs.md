---
CommandName: Tcl_ParseArgsObjv
ManualSection: 3
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_GetIndexFromObj(3)
 - Tcl_Main(3)
 - Tcl_CreateObjCommand(3)
Keywords:
 - argument
 - parse
Copyright:
 - Copyright (c) 2008 Donal K. Fellows
---

# Name

Tcl_ParseArgsObjv - parse arguments according to a tabular description

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_ParseArgsObjv]{.ccmd}[interp, argTable, objcPtr, objv, remObjv]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp out Where to store error messages. .AP "const Tcl_ArgvInfo" *argTable in Pointer to array of option descriptors. .AP "Tcl_Size \&| int" *objcPtr in/out A pointer to variable holding number of arguments in *objv*. Will be modified to hold number of arguments left in the unprocessed argument list stored in *remObjv*. May be (Tcl_Size *)NULL when not used. If it points to a variable which type is not **Tcl_Size**, a compiler warning will be generated. If your extensions is compiled with -DTCL_8_API, this function will return NULL for argument lists with more than INT_MAX elements (which should trigger proper error-handling), otherwise expect it to crash. .AP "Tcl_Obj *const" *objv in The array of arguments to be parsed. .AP Tcl_Obj ***remObjv out Pointer to a variable that will hold the array of unprocessed arguments. Should be NULL if no return of unprocessed arguments is required. If *objcPtr* is updated to a non-zero value, the array returned through this must be deallocated using **Tcl_Free**.

# Description

The **Tcl_ParseArgsObjv** function provides a system for parsing argument lists of the form "**-someName** *someValue* ...". Such argument lists are commonly found both in the arguments to a program and in the arguments to an individual Tcl command. This parser assumes that the order of the arguments does not matter, other than in so far as later copies of a duplicated option overriding earlier ones.

The argument array is described by the *objcPtr* and *objv* parameters, and an array of unprocessed arguments is returned through the *objcPtr* and *remObjv* parameters; if no return of unprocessed arguments is desired, the *remObjv* parameter should be NULL. If any problems happen, including if the "generate help" option is selected, an error message is left in the interpreter result and TCL_ERROR is returned. Otherwise, the interpreter result is left unchanged and TCL_OK is returned.

The collection of arguments to be parsed is described by the *argTable* parameter. This points to a table of descriptor structures that is terminated by an entry with the *type* field set to TCL_ARGV_END. As convenience, the following prototypical entries are provided:

**TCL_ARGV_AUTO_HELP**
: Enables the argument processor to provide help when passed the argument "**-help**".

**TCL_ARGV_AUTO_REST**
: Instructs the argument processor that arguments after "**--**" are to be unprocessed.

**TCL_ARGV_TABLE_END**
: Marks the end of the table of argument descriptors.


## Argument descriptor entries

Each entry of the argument descriptor table must be a structure of type **Tcl_ArgvInfo**. The structure is defined as this:

```
typedef struct {
    int type;
    const char *keyStr;
    void *srcPtr;
    void *dstPtr;
    const char *helpStr;
    void *clientData;
} Tcl_ArgvInfo;
```

The *keyStr* field contains the name of the option; by convention, this will normally begin with a "**-**" character. The *type*, *srcPtr*, *dstPtr* and *clientData* fields describe the interpretation of the value of the argument, as described below. The *helpStr* field gives some text that is used to provide help to users when they request it.

As noted above, the *type* field is used to describe the interpretation of the argument's value. The following values are acceptable values for *type*:

**TCL_ARGV_CONSTANT**
: The argument does not take any following value argument. If this argument is present, the (integer) value of the *srcPtr* field is copied to the variable pointed to by the *dstPtr* field. The *clientData* field is ignored.

**TCL_ARGV_END**
: This value marks the end of all option descriptors in the table. All other fields are ignored.

**TCL_ARGV_FLOAT**
: This argument takes a following floating point value argument. The value (once parsed by **Tcl_GetDoubleFromObj**) will be stored as a double-precision value in the variable pointed to by the *dstPtr* field. The *srcPtr* and *clientData* fields are ignored.

**TCL_ARGV_FUNC**
: This argument optionally takes a following value argument; it is up to the handler callback function passed in *srcPtr* to decide. That function will have the following signature:

    ```
    typedef int (Tcl_ArgvFuncProc)(
            void *clientData,
            Tcl_Obj *objPtr,
            void *dstPtr);
    ```
    The result is a boolean value indicating whether to consume the following argument. The *clientData* is the value from the table entry, the *objPtr* is the value that represents the following argument or NULL if there are no following arguments at all, and the *dstPtr* argument to the **Tcl_ArgvFuncProc** is the location to write the parsed value to.

**TCL_ARGV_GENFUNC**
: This argument takes zero or more following arguments; the handler callback function passed in *srcPtr* returns how many (or a negative number to signal an error, in which case it should also set the interpreter result). The function will have the following signature:

    ```
    typedef Tcl_Size (Tcl_ArgvGenFuncProc)(
            void *clientData,
            Tcl_Interp *interp,
            Tcl_Size objc,
            Tcl_Obj *const *objv,
            void *dstPtr);
    ```
    The *clientData* is the value from the table entry, the *interp* is where to store any error messages, *objc* and *objv* describe an array of all the remaining arguments, and *dstPtr* argument to the **Tcl_ArgvGenFuncProc** is the location to write the parsed value (or values) to.

**TCL_ARGV_HELP**
: This special argument does not take any following value argument, but instead causes **Tcl_ParseArgsObjv** to generate an error message describing the arguments supported. All other fields except the *helpStr* field are ignored.

**TCL_ARGV_INT**
: This argument takes a following integer value argument. The value (once parsed by **Tcl_GetIntFromObj**) will be stored as an int in the variable pointed to by the *dstPtr* field. The *srcPtr* field is ignored.

**TCL_ARGV_REST**
: This special argument does not take any following value argument, but instead marks all following arguments to be left unprocessed. The *srcPtr*, *dstPtr* and *clientData* fields are ignored.

**TCL_ARGV_STRING**
: This argument takes a following string value argument. A pointer to the string will be stored at *dstPtr*; the string inside will have a lifetime linked to the lifetime of the string representation of the argument value that it came from, and so should be copied if it needs to be retained. The *srcPtr* and *clientData* fields are ignored.


# Reference count management

The values in the *objv* argument to **Tcl_ParseArgsObjv** will not have their reference counts modified by this function. The interpreter result may be modified on error; the values passed should not be the interpreter result with no further reference added.

