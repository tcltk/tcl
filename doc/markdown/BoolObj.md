---
CommandName: Tcl_BooleanObj
ManualSection: 3
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_NewObj
 - Tcl_IsShared
 - Tcl_GetBoolean
Keywords:
 - boolean
 - value
Copyright:
 - Copyright (c) 1996-1997 Sun Microsystems, Inc.
---

# Name

Tcl_NewBooleanObj, Tcl_SetBooleanObj, Tcl_GetBooleanFromObj, Tcl_GetBoolFromObj - store/retrieve boolean value in a Tcl_Obj

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Obj *]{.ret} [Tcl_NewBooleanObj]{.ccmd}[intValue]{.cargs}
[Tcl_SetBooleanObj]{.ccmd}[objPtr, intValue]{.cargs}
[int]{.ret} [Tcl_GetBooleanFromObj]{.ccmd}[interp, objPtr, boolPtr]{.cargs}
[int]{.ret} [Tcl_GetBoolFromObj]{.ccmd}[interp, objPtr, flags. charPtr]{.cargs}
:::

# Arguments

.AP int intValue in Integer value to be stored as a boolean value in a Tcl_Obj. .AP Tcl_Obj *objPtr in/out Points to the Tcl_Obj in which to store, or from which to retrieve a boolean value. .AP Tcl_Interp *interp in/out If a boolean value cannot be retrieved, an error message is left in the interpreter's result value unless *interp* is NULL. .AP "bool \&| int" *boolPtr out Points to place where **Tcl_GetBooleanFromObj** stores the boolean value (0 or 1) obtained from *objPtr*. .AP char *charPtr out Points to place where **Tcl_GetBoolFromObj** stores the boolean value (0 or 1) obtained from *objPtr*. .AP int flags in 0 or TCL_NULL_OK. If TCL_NULL_OK is used, then the empty string or NULL will result in **Tcl_GetBoolFromObj** return TCL_OK, the *charPtr filled with the value **'\xFF'**; 

# Description

These procedures are used to pass boolean values to and from Tcl as Tcl_Obj's.  When storing a boolean value into a Tcl_Obj, any non-zero integer value in *intValue* is taken to be the boolean value **1**, and the integer value **0** is taken to be the boolean value **0**.

**Tcl_NewBooleanObj** creates a new Tcl_Obj, stores the boolean value *intValue* in it, and returns a pointer to the new Tcl_Obj. The new Tcl_Obj has reference count of zero.

**Tcl_SetBooleanObj** accepts *objPtr*, a pointer to an existing Tcl_Obj, and stores in the Tcl_Obj **objPtr* the boolean value *intValue*.  This is a write operation on **objPtr*, so *objPtr* must be unshared.  Attempts to write to a shared Tcl_Obj will panic.  A successful write of *intValue* into **objPtr* implies the freeing of any former value stored in **objPtr*.

**Tcl_GetBooleanFromObj** attempts to retrieve a boolean value from the value stored in **objPtr*. If *objPtr* holds a string value recognized by **Tcl_GetBoolean**, then the recognized boolean value is written at the address given by *boolPtr*. If *objPtr* holds any value recognized as a number by Tcl, then if that value is zero a 0 is written at the address given by *boolPtr* and if that value is non-zero a 1 is written at the address given by *boolPtr*. In all cases where a value is written at the address given by *boolPtr*, **Tcl_GetBooleanFromObj** returns **TCL_OK**. If the value of *objPtr* does not meet any of the conditions above, then **TCL_ERROR** is returned and an error message is left in the interpreter's result unless *interp* is NULL. **Tcl_GetBooleanFromObj** may also make changes to the internal fields of **objPtr* so that future calls to **Tcl_GetBooleanFromObj** on the same *objPtr* can be performed more efficiently.

**Tcl_GetBoolFromObj** functions almost the same as **Tcl_GetBooleanFromObj**, but it has an additional parameter **flags**, which can be used to specify whether the empty string or NULL is accepted as valid.

Note that the routines **Tcl_GetBooleanFromObj** and **Tcl_GetBoolean** are not functional equivalents. The set of values for which **Tcl_GetBooleanFromObj** will return **TCL_OK** is strictly larger than the set of values for which **Tcl_GetBoolean** will do the same. For example, the value "5" passed to **Tcl_GetBooleanFromObj** will lead to a **TCL_OK** return (and the boolean value 1), while the same value passed to **Tcl_GetBoolean** will lead to a **TCL_ERROR** return. 

# Reference count management

**Tcl_NewBooleanObj** always returns a zero-reference object, much like **Tcl_NewObj**.

**Tcl_SetBooleanObj** does not modify the reference count of its *objPtr* argument, but does require that the object be unshared.

**Tcl_GetBooleanFromObj** does not modify the reference count of its *objPtr* argument; it only reads. Note however that this function may set the interpreter result; if that is the only place that is holding a reference to the object, it will be deleted.

