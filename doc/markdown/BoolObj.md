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

Tcl\_NewBooleanObj, Tcl\_SetBooleanObj, Tcl\_GetBooleanFromObj, Tcl\_GetBoolFromObj - store/retrieve boolean value in a Tcl\_Obj

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Obj \*]{.ret} [Tcl\_NewBooleanObj]{.ccmd}[intValue]{.cargs}
[Tcl\_SetBooleanObj]{.ccmd}[objPtr, intValue]{.cargs}
[int]{.ret} [Tcl\_GetBooleanFromObj]{.ccmd}[interp, objPtr, boolPtr]{.cargs}
[int]{.ret} [Tcl\_GetBoolFromObj]{.ccmd}[interp, objPtr, flags. charPtr]{.cargs}
:::

# Arguments

.AP int intValue in Integer value to be stored as a boolean value in a Tcl\_Obj. .AP Tcl\_Obj \*objPtr in/out Points to the Tcl\_Obj in which to store, or from which to retrieve a boolean value. .AP Tcl\_Interp \*interp in/out If a boolean value cannot be retrieved, an error message is left in the interpreter's result value unless *interp* is NULL. .AP "bool | int" \*boolPtr out Points to place where **Tcl\_GetBooleanFromObj** stores the boolean value (0 or 1) obtained from *objPtr*. .AP char \*charPtr out Points to place where **Tcl\_GetBoolFromObj** stores the boolean value (0 or 1) obtained from *objPtr*. .AP int flags in 0 or TCL\_NULL\_OK. If TCL\_NULL\_OK is used, then the empty string or NULL will result in **Tcl\_GetBoolFromObj** return TCL\_OK, the \*charPtr filled with the value **'\\xFF'**; 

# Description

These procedures are used to pass boolean values to and from Tcl as Tcl\_Obj's.  When storing a boolean value into a Tcl\_Obj, any non-zero integer value in *intValue* is taken to be the boolean value **1**, and the integer value **0** is taken to be the boolean value **0**.

**Tcl\_NewBooleanObj** creates a new Tcl\_Obj, stores the boolean value *intValue* in it, and returns a pointer to the new Tcl\_Obj. The new Tcl\_Obj has reference count of zero.

**Tcl\_SetBooleanObj** accepts *objPtr*, a pointer to an existing Tcl\_Obj, and stores in the Tcl\_Obj *\*objPtr* the boolean value *intValue*.  This is a write operation on *\*objPtr*, so *objPtr* must be unshared.  Attempts to write to a shared Tcl\_Obj will panic.  A successful write of *intValue* into *\*objPtr* implies the freeing of any former value stored in *\*objPtr*.

**Tcl\_GetBooleanFromObj** attempts to retrieve a boolean value from the value stored in *\*objPtr*. If *objPtr* holds a string value recognized by **Tcl\_GetBoolean**, then the recognized boolean value is written at the address given by *boolPtr*. If *objPtr* holds any value recognized as a number by Tcl, then if that value is zero a 0 is written at the address given by *boolPtr* and if that value is non-zero a 1 is written at the address given by *boolPtr*. In all cases where a value is written at the address given by *boolPtr*, **Tcl\_GetBooleanFromObj** returns **TCL\_OK**. If the value of *objPtr* does not meet any of the conditions above, then **TCL\_ERROR** is returned and an error message is left in the interpreter's result unless *interp* is NULL. **Tcl\_GetBooleanFromObj** may also make changes to the internal fields of *\*objPtr* so that future calls to **Tcl\_GetBooleanFromObj** on the same *objPtr* can be performed more efficiently.

**Tcl\_GetBoolFromObj** functions almost the same as **Tcl\_GetBooleanFromObj**, but it has an additional parameter **flags**, which can be used to specify whether the empty string or NULL is accepted as valid.

Note that the routines **Tcl\_GetBooleanFromObj** and **Tcl\_GetBoolean** are not functional equivalents. The set of values for which **Tcl\_GetBooleanFromObj** will return **TCL\_OK** is strictly larger than the set of values for which **Tcl\_GetBoolean** will do the same. For example, the value "5" passed to **Tcl\_GetBooleanFromObj** will lead to a **TCL\_OK** return (and the boolean value 1), while the same value passed to **Tcl\_GetBoolean** will lead to a **TCL\_ERROR** return. 

# Reference count management

**Tcl\_NewBooleanObj** always returns a zero-reference object, much like **Tcl\_NewObj**.

**Tcl\_SetBooleanObj** does not modify the reference count of its *objPtr* argument, but does require that the object be unshared.

**Tcl\_GetBooleanFromObj** does not modify the reference count of its *objPtr* argument; it only reads. Note however that this function may set the interpreter result; if that is the only place that is holding a reference to the object, it will be deleted.

