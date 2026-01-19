---
CommandName: Tcl_DoubleObj
ManualSection: 3
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_NewObj
 - Tcl_DecrRefCount
 - Tcl_IncrRefCount
 - Tcl_GetObjResult
Keywords:
 - double
 - double value
 - double type
 - internal representation
 - value
 - value type
 - string representation
Copyright:
 - Copyright (c) 1996-1997 Sun Microsystems, Inc.
---

# Name

Tcl\_NewDoubleObj, Tcl\_SetDoubleObj, Tcl\_GetDoubleFromObj - manipulate Tcl values as floating-point values

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Obj \*]{.ret} [Tcl\_NewDoubleObj]{.ccmd}[doubleValue]{.cargs}
[Tcl\_SetDoubleObj]{.ccmd}[objPtr, doubleValue]{.cargs}
[int]{.ret} [Tcl\_GetDoubleFromObj]{.ccmd}[interp, objPtr, doublePtr]{.cargs}
:::

# Arguments

.AP double doubleValue in A double-precision floating-point value used to initialize or set a Tcl value. .AP Tcl\_Obj \*objPtr in/out For **Tcl\_SetDoubleObj**, this points to the value in which to store a double value. For **Tcl\_GetDoubleFromObj**, this refers to the value from which to retrieve a double value. .AP Tcl\_Interp \*interp in/out When non-NULL, an error message is left here when double value retrieval fails. .AP double \*doublePtr out Points to place to store the double value obtained from *objPtr*. 

# Description

These procedures are used to create, modify, and read Tcl values that hold double-precision floating-point values.

**Tcl\_NewDoubleObj** creates and returns a new Tcl value initialized to the double value *doubleValue*.  The returned Tcl value is unshared.

**Tcl\_SetDoubleObj** sets the value of an existing Tcl value pointed to by *objPtr* to the double value *doubleValue*.  The *objPtr* argument must point to an unshared Tcl value.  Any attempt to set the value of a shared Tcl value violates Tcl's copy-on-write policy.  Any existing string representation or internal representation in the unshared Tcl value will be freed as a consequence of setting the new value.

**Tcl\_GetDoubleFromObj** attempts to retrieve a double value from the Tcl value *objPtr*.  If the attempt succeeds, then **TCL\_OK** is returned, and the double value is written to the storage pointed to by *doublePtr*.  If the attempt fails, then **TCL\_ERROR** is returned, and if *interp* is non-NULL, an error message is left in *interp*. The **Tcl\_ObjType** of *objPtr* may be changed to make subsequent calls to **Tcl\_GetDoubleFromObj** more efficient.

# Reference count management

**Tcl\_NewDoubleObj** always returns a zero-reference object, much like **Tcl\_NewObj**.

**Tcl\_SetDoubleObj** does not modify the reference count of its *objPtr* argument, but does require that the object be unshared.

**Tcl\_GetDoubleFromObj** does not modify the reference count of its *objPtr* argument; it only reads. Note however that this function may set the interpreter result; if that is the only place that is holding a reference to the object, it will be deleted.

