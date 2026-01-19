---
CommandName: Tcl_SetVar
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_GetObjResult
 - Tcl_GetStringResult
 - Tcl_TraceVar
Keywords:
 - array
 - get variable
 - interpreter
 - scalar
 - set
 - unset
 - value
 - variable
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

Tcl\_SetVar2Ex, Tcl\_SetVar, Tcl\_SetVar2, Tcl\_ObjSetVar2, Tcl\_GetVar2Ex, Tcl\_GetVar, Tcl\_GetVar2, Tcl\_ObjGetVar2, Tcl\_UnsetVar, Tcl\_UnsetVar2 - manipulate Tcl variables

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Obj \*]{.ret} [Tcl\_SetVar2Ex]{.ccmd}[interp, name1, name2, newValuePtr, flags]{.cargs}
[const char \*]{.ret} [Tcl\_SetVar]{.ccmd}[interp, varName, newValue, flags]{.cargs}
[const char \*]{.ret} [Tcl\_SetVar2]{.ccmd}[interp, name1, name2, newValue, flags]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_ObjSetVar2]{.ccmd}[interp, part1Ptr, part2Ptr, newValuePtr, flags]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_GetVar2Ex]{.ccmd}[interp, name1, name2, flags]{.cargs}
[const char \*]{.ret} [Tcl\_GetVar]{.ccmd}[interp, varName, flags]{.cargs}
[const char \*]{.ret} [Tcl\_GetVar2]{.ccmd}[interp, name1, name2, flags]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_ObjGetVar2]{.ccmd}[interp, part1Ptr, part2Ptr, flags]{.cargs}
[int]{.ret} [Tcl\_UnsetVar]{.ccmd}[interp, varName, flags]{.cargs}
[int]{.ret} [Tcl\_UnsetVar2]{.ccmd}[interp, name1, name2, flags]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter containing variable. .AP "const char" \*name1 in Contains the name of an array variable (if *name2* is non-NULL) or (if *name2* is NULL) either the name of a scalar variable or a complete name including both variable name and index. May include **::** namespace qualifiers to specify a variable in a particular namespace. .AP "const char" \*name2 in If non-NULL, gives name of element within array; in this case *name1* must refer to an array variable. .AP Tcl\_Obj \*newValuePtr in Points to a Tcl value containing the new value for the variable. .AP int flags in OR-ed combination of bits providing additional information. See below for valid values. .AP "const char" \*varName in Name of variable. May include **::** namespace qualifiers to specify a variable in a particular namespace. May refer to a scalar variable or an element of an array. .AP "const char" \*newValue in New value for variable, specified as a null-terminated string. A copy of this value is stored in the variable. .AP Tcl\_Obj \*part1Ptr in Points to a Tcl value containing the variable's name. The name may include a series of **::** namespace qualifiers to specify a variable in a particular namespace. May refer to a scalar variable or an element of an array variable. .AP Tcl\_Obj \*part2Ptr in If non-NULL, points to a value containing the name of an element within an array and *part1Ptr* must refer to an array variable. 

# Description

These procedures are used to create, modify, read, and delete Tcl variables from C code.

**Tcl\_SetVar2Ex**, **Tcl\_SetVar**, **Tcl\_SetVar2**, and **Tcl\_ObjSetVar2** will create a new variable or modify an existing one. These procedures set the given variable to the value given by *newValuePtr* or *newValue* and return a pointer to the variable's new value, which is stored in Tcl's variable structure. **Tcl\_SetVar2Ex** and **Tcl\_ObjSetVar2** take the new value as a Tcl\_Obj and return a pointer to a Tcl\_Obj.  **Tcl\_SetVar** and **Tcl\_SetVar2** take the new value as a string and return a string; they are usually less efficient than **Tcl\_ObjSetVar2**.  Note that the return value may be different than the *newValuePtr* or *newValue* argument, due to modifications made by write traces. If an error occurs in setting the variable (e.g. an array variable is referenced without giving an index into the array) NULL is returned and an error message is left in *interp*'s result if the **TCL\_LEAVE\_ERR\_MSG** *flag* bit is set.

**Tcl\_GetVar2Ex**, **Tcl\_GetVar**, **Tcl\_GetVar2**, and **Tcl\_ObjGetVar2** return the current value of a variable. The arguments to these procedures are treated in the same way as the arguments to the procedures described above. Under normal circumstances, the return value is a pointer to the variable's value.  For **Tcl\_GetVar2Ex** and **Tcl\_ObjGetVar2** the value is returned as a pointer to a Tcl\_Obj.  For **Tcl\_GetVar** and **Tcl\_GetVar2** the value is returned as a string; this is usually less efficient, so **Tcl\_GetVar2Ex** or **Tcl\_ObjGetVar2** are preferred. If an error occurs while reading the variable (e.g. the variable does not exist or an array element is specified for a scalar variable), then NULL is returned and an error message is left in *interp*'s result if the **TCL\_LEAVE\_ERR\_MSG** *flag* bit is set.

**Tcl\_UnsetVar** and **Tcl\_UnsetVar2** may be used to remove a variable, so that future attempts to read the variable will return an error. The arguments to these procedures are treated in the same way as the arguments to the procedures above. If the variable is successfully removed then **TCL\_OK** is returned. If the variable cannot be removed because it does not exist then **TCL\_ERROR** is returned and an error message is left in *interp*'s result if the **TCL\_LEAVE\_ERR\_MSG** *flag* bit is set. If an array element is specified, the given element is removed but the array remains. If an array name is specified without an index, then the entire array is removed.

The name of a variable may be specified to these procedures in four ways:

1. If **Tcl\_SetVar**, **Tcl\_GetVar**, or **Tcl\_UnsetVar** is invoked, the variable name is given as a single string, *varName*. If *varName* contains an open parenthesis and ends with a close parenthesis, then the value between the parentheses is treated as an index (which can have any string value) and the characters before the first open parenthesis are treated as the name of an array variable. If *varName* does not have parentheses as described above, then the entire string is treated as the name of a scalar variable.

2. If the *name1* and *name2* arguments are provided and *name2* is non-NULL, then an array element is specified and the array name and index have already been separated by the caller: *name1* contains the name and *name2* contains the index.  An error is generated if *name1*  contains an open parenthesis and ends with a close parenthesis (array element) and *name2* is non-NULL.

3. If *name2* is NULL, *name1* is treated just like *varName* in case [1] above (it can be either a scalar or an array element variable name).


The *flags* argument may be used to specify any of several options to the procedures. It consists of an OR-ed combination of the following bits.

**TCL\_GLOBAL\_ONLY**
: Under normal circumstances the procedures look up variables as follows. If a procedure call is active in *interp*, the variable is looked up at the current level of procedure call. Otherwise, the variable is looked up first in the current namespace, then in the global namespace. However, if this bit is set in *flags* then the variable is looked up only in the global namespace even if there is a procedure call active. If both **TCL\_GLOBAL\_ONLY** and **TCL\_NAMESPACE\_ONLY** are given, **TCL\_GLOBAL\_ONLY** is ignored.

**TCL\_NAMESPACE\_ONLY**
: If this bit is set in *flags* then the variable is looked up only in the current namespace; if a procedure is active its variables are ignored, and the global namespace is also ignored unless it is the current namespace.

**TCL\_LEAVE\_ERR\_MSG**
: If an error is returned and this bit is set in *flags*, then an error message will be left in the interpreter's result, where it can be retrieved with **Tcl\_GetObjResult** or **Tcl\_GetStringResult**. If this flag bit is not set then no error message is left and the interpreter's result will not be modified.

**TCL\_APPEND\_VALUE**
: If this bit is set then *newValuePtr* or *newValue* is appended to the current value instead of replacing it. If the variable is currently undefined, then the bit is ignored. This bit is only used by the **Tcl\_Set\*** procedures.

**TCL\_LIST\_ELEMENT**
: If this bit is set, then *newValue* is converted to a valid Tcl list element before setting (or appending to) the variable. A separator space is appended before the new list element unless the list element is going to be the first element in a list or sublist (i.e. the variable's current value is empty, or contains the single character "{", or ends in " }"). When appending, the original value of the variable must also be a valid list, so that the operation is the appending of a new list element onto a list.


**Tcl\_GetVar** and **Tcl\_GetVar2** return the current value of a variable. The arguments to these procedures are treated in the same way as the arguments to **Tcl\_SetVar** and **Tcl\_SetVar2**. Under normal circumstances, the return value is a pointer to the variable's value (which is stored in Tcl's variable structure and will not change before the next call to **Tcl\_SetVar** or **Tcl\_SetVar2**). **Tcl\_GetVar** and **Tcl\_GetVar2** use the flag bits **TCL\_GLOBAL\_ONLY** and **TCL\_LEAVE\_ERR\_MSG**, both of which have the same meaning as for **Tcl\_SetVar**. If an error occurs in reading the variable (e.g. the variable does not exist or an array element is specified for a scalar variable), then NULL is returned.

**Tcl\_UnsetVar** and **Tcl\_UnsetVar2** may be used to remove a variable, so that future calls to **Tcl\_GetVar** or **Tcl\_GetVar2** for the variable will return an error. The arguments to these procedures are treated in the same way as the arguments to **Tcl\_GetVar** and **Tcl\_GetVar2**. If the variable is successfully removed then **TCL\_OK** is returned. If the variable cannot be removed because it does not exist then **TCL\_ERROR** is returned. If an array element is specified, the given element is removed but the array remains. If an array name is specified without an index, then the entire array is removed. 

# Reference count management

The result of **Tcl\_SetVar2Ex**, **Tcl\_ObjSetVar2**, **Tcl\_GetVar2Ex**, and **Tcl\_ObjGetVar2** is (if non-NULL) a value with a reference of at least 1, where that reference is held by the variable that the function has just operated upon.

The *newValuePtr* argument to **Tcl\_SetVar2Ex** and **Tcl\_ObjSetVar2** may be an arbitrary reference count value.  Its reference count is incremented on success.  On failure, if its reference count is zero, it is decremented and freed so the caller need do nothing with it.

The *part1Ptr* argument to **Tcl\_ObjSetVar2** and **Tcl\_ObjGetVar2** can have any reference count.  These functions never modify it.

The *part2Ptr* argument to **Tcl\_ObjSetVar2** and **Tcl\_ObjGetVar2**, if non-NULL, should not have a zero reference count as these functions may retain a reference to it, particularly when it is used to create an array element that did not previously exist, and decrementing the reference count later would leave them pointing to a freed Tcl\_Obj. 

