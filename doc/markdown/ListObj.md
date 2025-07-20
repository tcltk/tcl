---
CommandName: Tcl_ListObj
ManualSection: 3
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_NewObj(3)
 - Tcl_DecrRefCount(3)
 - Tcl_IncrRefCount(3)
 - Tcl_GetObjResult(3)
Keywords:
 - append
 - index
 - insert
 - internal representation
 - length
 - list
 - list value
 - list type
 - value
 - value type
 - replace
 - string representation
Copyright:
 - Copyright (c) 1996-1997 Sun Microsystems, Inc.
---

# Name

Tcl_ListObjAppendList, Tcl_ListObjAppendElement, Tcl_NewListObj, Tcl_SetListObj, Tcl_ListObjGetElements, Tcl_ListObjLength, Tcl_ListObjIndex, Tcl_ListObjReplace - manipulate Tcl values as lists

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_ListObjAppendList]{.ccmd}[interp, listPtr, elemListPtr]{.cargs}
[int]{.ret} [Tcl_ListObjAppendElement]{.ccmd}[interp, listPtr, objPtr]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_NewListObj]{.ccmd}[objc, objv]{.cargs}
[Tcl_SetListObj]{.ccmd}[objPtr, objc, objv]{.cargs}
[int]{.ret} [Tcl_ListObjGetElements]{.ccmd}[interp, listPtr, objcPtr, objvPtr]{.cargs}
[int]{.ret} [Tcl_ListObjLength]{.ccmd}[interp, listPtr, lengthPtr]{.cargs}
[int]{.ret} [Tcl_ListObjIndex]{.ccmd}[interp, listPtr, index, objPtrPtr]{.cargs}
[int]{.ret} [Tcl_ListObjReplace]{.ccmd}[interp, listPtr, first, count, objc, objv]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in If an error occurs while converting a value to be a list value, an error message is left in the interpreter's result value unless *interp* is NULL. .AP Tcl_Obj *listPtr in/out Points to the list value to be manipulated. If *listPtr* does not already point to a list value, an attempt will be made to convert it to one. .AP Tcl_Obj *elemListPtr in/out For **Tcl_ListObjAppendList**, this points to a list value containing elements to be appended onto *listPtr*. Each element of **elemListPtr* will become a new element of *listPtr*. If **elemListPtr* is not NULL and does not already point to a list value, an attempt will be made to convert it to one. .AP Tcl_Obj *objPtr in For **Tcl_ListObjAppendElement**, points to the Tcl value that will be appended to *listPtr*. For **Tcl_SetListObj**, this points to the Tcl value that will be converted to a list value containing the *objc* elements of the array referenced by *objv*. .AP "Tcl_Size \&| int" *objcPtr in Points to location where **Tcl_ListObjGetElements** stores the number of element values in *listPtr*. May be (Tcl_Size *)NULL when not used. If it points to a variable which type is not **Tcl_Size**, a compiler warning will be generated. If your extensions is compiled with -DTCL_8_API, this function will return NULL for lists with more than INT_MAX elements (which should trigger proper error-handling), otherwise expect it to crash. .AP Tcl_Obj ***objvPtr out A location where **Tcl_ListObjGetElements** stores a pointer to an array of pointers to the element values of *listPtr*. .AP Tcl_Size objc in The number of Tcl values that **Tcl_NewListObj** will insert into a new list value, and **Tcl_ListObjReplace** will insert into *listPtr*. For **Tcl_SetListObj**, the number of Tcl values to insert into *objPtr*. .AP "Tcl_Obj *const" objv[] in An array of pointers to values. **Tcl_NewListObj** will insert these values into a new list value and **Tcl_ListObjReplace** will insert them into an existing *listPtr*. Each value will become a separate list element. .AP "Tcl_Size \&| int" *lengthPtr out Points to location where **Tcl_ListObjLength** stores the length of the list. May be (Tcl_Size *)NULL when not used. If it points to a variable which type is not **Tcl_Size**, a compiler warning will be generated. If your extensions is compiled with -DTCL_8_API, this function will return TCL_ERROR for lists with more than INT_MAX elements (which should trigger proper error-handling), otherwise expect it to crash. .AP Tcl_Size index in Index of the list element that **Tcl_ListObjIndex** is to return. The first element has index 0. .AP Tcl_Obj **objPtrPtr out Points to place where **Tcl_ListObjIndex** is to store a pointer to the resulting list element value. .AP Tcl_Size first in Index of the starting list element that **Tcl_ListObjReplace** is to replace. The list's first element has index 0. .AP Tcl_Size count in The number of elements that **Tcl_ListObjReplace** is to replace. 

# Description

Tcl list values have an internal representation that supports the efficient indexing and appending. The procedures described in this man page are used to create, modify, index, and append to Tcl list values from C code.

**Tcl_ListObjAppendList** and **Tcl_ListObjAppendElement** both add one or more values to the end of the list value referenced by *listPtr*. **Tcl_ListObjAppendList** appends each element of the list value referenced by *elemListPtr* while **Tcl_ListObjAppendElement** appends the single value referenced by *objPtr*. Both procedures will convert the value referenced by *listPtr* to a list value if necessary. If an error occurs during conversion, both procedures return **TCL_ERROR** and leave an error message in the interpreter's result value if *interp* is not NULL. Similarly, if *elemListPtr* does not already refer to a list value, **Tcl_ListObjAppendList** will attempt to convert it to one and if an error occurs during conversion, will return **TCL_ERROR** and leave an error message in the interpreter's result value if interp is not NULL. Both procedures invalidate any old string representation of *listPtr* and, if it was converted to a list value, free any old internal representation. Similarly, **Tcl_ListObjAppendList** frees any old internal representation of *elemListPtr* if it converts it to a list value. After appending each element in *elemListPtr*, **Tcl_ListObjAppendList** increments the element's reference count since *listPtr* now also refers to it. For the same reason, **Tcl_ListObjAppendElement** increments *objPtr*'s reference count. If no error occurs, the two procedures return **TCL_OK** after appending the values.

**Tcl_NewListObj** and **Tcl_SetListObj** create a new value or modify an existing value to hold the *objc* elements of the array referenced by *objv* where each element is a pointer to a Tcl value. If *objc* is less than or equal to zero, they return an empty value. If *objv* is NULL, the resulting list contains 0 elements, with reserved space in an internal representation for *objc* more elements (to avoid its reallocation later). The new value's string representation is left invalid. The two procedures increment the reference counts of the elements in *objc* since the list value now refers to them. The new list value returned by **Tcl_NewListObj** has reference count zero.

**Tcl_ListObjGetElements** returns a count and a pointer to an array of the elements in a list value.  It returns the count by storing it in the address *objcPtr*.  Similarly, it returns the array pointer by storing it in the address *objvPtr*. The memory pointed to is managed by Tcl and should not be freed or written to by the caller. If the list is empty, 0 is stored at *objcPtr* and NULL at *objvPtr*. If *objcPtr* points to a variable of type **int** and the list contains more than 2**31 elements, the function returns **TCL_ERROR**. If *listPtr* is not already a list value, **Tcl_ListObjGetElements** will attempt to convert it to one; if the conversion fails, it returns **TCL_ERROR** and leaves an error message in the interpreter's result value if *interp* is not NULL. Otherwise it returns **TCL_OK** after storing the count and array pointer.

**Tcl_ListObjLength** returns the number of elements in the list value referenced by *listPtr*. If the value is not already a list value, **Tcl_ListObjLength** will attempt to convert it to one; if the conversion fails, it returns **TCL_ERROR** and leaves an error message in the interpreter's result value if *interp* is not NULL. Otherwise it returns **TCL_OK** after storing the list's length.

The procedure **Tcl_ListObjIndex** returns a pointer to the value at element *index* in the list referenced by *listPtr*. It returns this value by storing a pointer to it in the address *objPtrPtr*. If *listPtr* does not already refer to a list value, **Tcl_ListObjIndex** will attempt to convert it to one; if the conversion fails, it returns **TCL_ERROR** and leaves an error message in the interpreter's result value if *interp* is not NULL. If the index is out of range, that is, *index* is negative or greater than or equal to the number of elements in the list, **Tcl_ListObjIndex** stores a NULL in *objPtrPtr* and returns **TCL_OK**. Otherwise it returns **TCL_OK** after storing the element's value pointer. The reference count for the list element is not incremented; the caller must do that if it needs to retain a pointer to the element, or "bounce" the reference count when the element is no longer needed. This is because a returned element may have a reference count of 0. Abstract Lists create a new element Obj on demand, and do not retain any element Obj values. Therefore, the caller is responsible for freeing the element when it is no longer needed. Note that this is a change from Tcl 8 where all list elements always have a reference count of at least 1. (See **ABSTRACT LIST TYPES**, **STORAGE MANAGEMENT OF VALUES**, Tcl_BounceRefCount(3), and lseq(n) for more information.) 

**Tcl_ListObjReplace** replaces zero or more elements of the list referenced by *listPtr* with the *objc* values in the array referenced by *objv*. If *listPtr* does not point to a list value, **Tcl_ListObjReplace** will attempt to convert it to one; if the conversion fails, it returns **TCL_ERROR** and leaves an error message in the interpreter's result value if *interp* is not NULL. Otherwise, it returns **TCL_OK** after replacing the values. If *objv* is NULL, no new elements are added. If the argument *first* is zero or negative, it refers to the first element. If *first* is greater than or equal to the number of elements in the list, then no elements are deleted; the new elements are appended to the list. *count* gives the number of elements to replace. If *count* is zero or negative then no elements are deleted; the new elements are simply inserted before the one designated by *first*. **Tcl_ListObjReplace** invalidates *listPtr*'s old string representation. The reference counts of any elements inserted from *objv* are incremented since the resulting list now refers to them. Similarly, the reference counts for any replaced values are decremented.

Because **Tcl_ListObjReplace** combines both element insertion and deletion, it can be used to implement a number of list operations. For example, the following code inserts the *objc* values referenced by the array of value pointers *objv* just before the element *index* of the list referenced by *listPtr*:

```
result = Tcl_ListObjReplace(interp, listPtr, index, 0,
        objc, objv);
```

Similarly, the following code appends the *objc* values referenced by the array *objv* to the end of the list *listPtr*:

```
result = Tcl_ListObjLength(interp, listPtr, &length);
if (result == TCL_OK) {
    result = Tcl_ListObjReplace(interp, listPtr, length, 0,
            objc, objv);
}
```

The *count* list elements starting at *first* can be deleted by simply calling **Tcl_ListObjReplace** with a NULL *objvPtr*:

```
result = Tcl_ListObjReplace(interp, listPtr, first, count,
        0, NULL);
```

# Reference count management

**Tcl_NewListObj** always returns a zero-reference object, much like **Tcl_NewObj**. If a non-NULL *objv* argument is given, the reference counts of the first *objc* values in that array are incremented.

**Tcl_SetListObj** does not modify the reference count of its *objPtr* argument, but does require that the object be unshared. The reference counts of the first *objc* values in the *objv* array are incremented.

**Tcl_ListObjGetElements**, **Tcl_ListObjIndex**, and **Tcl_ListObjLength** do not modify the reference count of their *listPtr* arguments; they only read. Note however that these three functions may set the interpreter result; if that is the only place that is holding a reference to the object, it will be deleted.

**Tcl_ListObjAppendList**, **Tcl_ListObjAppendElement**, and **Tcl_ListObjReplace** require an unshared *listPtr* argument. **Tcl_ListObjAppendList** only reads its *elemListPtr* argument. **Tcl_ListObjAppendElement** increments the reference count of its *objPtr* on success. **Tcl_ListObjReplace** increments the reference count of the first *objc* values in the *objv* array on success.  Note however that all these three functions may set the interpreter result on failure; if that is the only place that is holding a reference to the object, it will be deleted.

