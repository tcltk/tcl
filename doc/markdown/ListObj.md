---
CommandName: Tcl_ListObj
ManualSection: 3
Version: 9.1
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

Tcl\_ListObjAppendList, Tcl\_ListObjAppendElement, Tcl\_NewListObj, Tcl\_SetListObj, Tcl\_ListObjGetElements, Tcl\_ListObjLength, Tcl\_ListObjIndex, Tcl\_ListObjReplace, Tcl\_ListObjRange, Tcl\_ListObjRepeat, Tcl\_ListObjReverse - manipulate Tcl values as lists

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_ListObjAppendList]{.ccmd}[interp, listPtr, elemListPtr]{.cargs}
[int]{.ret} [Tcl\_ListObjAppendElement]{.ccmd}[interp, listPtr, objPtr]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_NewListObj]{.ccmd}[objc, objv]{.cargs}
[Tcl\_SetListObj]{.ccmd}[objPtr, objc, objv]{.cargs}
[int]{.ret} [Tcl\_ListObjGetElements]{.ccmd}[interp, listPtr, objcPtr, objvPtr]{.cargs}
[int]{.ret} [Tcl\_ListObjLength]{.ccmd}[interp, listPtr, lengthPtr]{.cargs}
[int]{.ret} [Tcl\_ListObjIndex]{.ccmd}[interp, listPtr, index, objPtrPtr]{.cargs}
[int]{.ret} [Tcl\_ListObjReplace]{.ccmd}[interp, listPtr, first, count, objc, objv]{.cargs}
[int]{.ret} [Tcl\_ListObjRange]{.ccmd}[interp, listPtr, first, last, objPtrPtr]{.cargs}
[int]{.ret} [Tcl\_ListObjRepeat]{.ccmd}[interp, count, objc, objv, objPtrPtr]{.cargs}
[int]{.ret} [Tcl\_ListObjReverse]{.ccmd}[interp, listPtr, objPtrPtr]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in If an error occurs while converting a value to be a list value, an error message is left in the interpreter's result value unless *interp* is NULL. .AP Tcl\_Obj \*listPtr in/out Points to the input list value. If *listPtr* does not already point to a list value, an attempt will be made to convert it to one. Some functions may store a reference to it internally so care must be taken when managing its reference count. See the [Reference count management] section below. .AP Tcl\_Obj \*elemListPtr in/out For **Tcl\_ListObjAppendList**, this points to a list value containing elements to be appended onto *listPtr*. Each element of \**elemListPtr* will become a new element of *listPtr*. If \**elemListPtr* is not NULL and does not already point to a list value, an attempt will be made to convert it to one. .AP Tcl\_Obj \*objPtr in For **Tcl\_ListObjAppendElement**, points to the Tcl value that will be appended to *listPtr*. For **Tcl\_SetListObj**, this points to the Tcl value that will be converted to a list value containing the *objc* elements of the array referenced by *objv*. .AP "Tcl\_Size \\&| int" \*objcPtr in Points to location where **Tcl\_ListObjGetElements** stores the number of element values in *listPtr*. May be (Tcl\_Size \*)NULL when not used. If it points to a variable which type is not **Tcl\_Size**, a compiler warning will be generated. If your extensions is compiled with **-DTCL\_8\_API**, this function will return NULL for lists with more than INT\_MAX elements (which should trigger proper error-handling), otherwise expect it to crash. .AP Tcl\_Obj \*\*\*objvPtr out A location where **Tcl\_ListObjGetElements** stores a pointer to an array of pointers to the element values of *listPtr*. .AP Tcl\_Size objc in The number of Tcl values in the *objv* array. .AP "Tcl\_Obj \*const" objv[] in An array of pointers to Tcl values. .AP "Tcl\_Size \\&| int" \*lengthPtr out Points to location where **Tcl\_ListObjLength** stores the length of the list. May be (Tcl\_Size \*)NULL when not used. If it points to a variable which type is not **Tcl\_Size**, a compiler warning will be generated. If your extensions is compiled with **-DTCL\_8\_API**, this function will return TCL\_ERROR for lists with more than INT\_MAX elements (which should trigger proper error-handling), otherwise expect it to crash. .AP Tcl\_Size index in Index of the list element that **Tcl\_ListObjIndex** is to return. The first element has index 0. .AP Tcl\_Obj \*\*objPtrPtr out Points to location to store an output list or element value. No assumptions should be made about the reference count of the returned value. See the [Reference count management] section below. .AP Tcl\_Size first in Index of the first element in a range of elements in a list. .AP Tcl\_Size last in Index of the last element in a range of elements in a list. .AP Tcl\_Size count in The number of elements to be operated on or a repetition count.

# Description

Tcl list values have an internal representation that supports the efficient indexing and appending. The procedures described in this man page are used to create, modify, access, and transform Tcl list values from C code.

**Tcl\_ListObjAppendList** and **Tcl\_ListObjAppendElement** both add one or more values to the end of the list value referenced by *listPtr*. **Tcl\_ListObjAppendList** appends each element of the list value referenced by *elemListPtr* while **Tcl\_ListObjAppendElement** appends the single value referenced by *objPtr*. Both procedures will convert the value referenced by *listPtr* to a list value if necessary. If an error occurs during conversion, both procedures return **TCL\_ERROR** and leave an error message in the interpreter's result value if *interp* is not NULL. Similarly, if *elemListPtr* does not already refer to a list value, **Tcl\_ListObjAppendList** will attempt to convert it to one and if an error occurs during conversion, will return **TCL\_ERROR** and leave an error message in the interpreter's result value if interp is not NULL. Both procedures invalidate any old string representation of *listPtr* and, if it was converted to a list value, free any old internal representation. Similarly, **Tcl\_ListObjAppendList** frees any old internal representation of *elemListPtr* if it converts it to a list value. After appending each element in *elemListPtr*, **Tcl\_ListObjAppendList** increments the element's reference count since *listPtr* now also refers to it. For the same reason, **Tcl\_ListObjAppendElement** increments *objPtr*'s reference count. If no error occurs, the two procedures return **TCL\_OK** after appending the values.

**Tcl\_NewListObj** and **Tcl\_SetListObj** create a new value or modify an existing value to hold the *objc* elements of the array referenced by *objv* where each element is a pointer to a Tcl value. If *objc* is less than or equal to zero, they return an empty value. If *objv* is NULL, the resulting list contains 0 elements, with reserved space in an internal representation for *objc* more elements (to avoid its reallocation later). The new value's string representation is left invalid. The two procedures increment the reference counts of the elements in *objc* since the list value now refers to them. The new list value returned by **Tcl\_NewListObj** has reference count zero.

**Tcl\_ListObjGetElements** returns a count and a pointer to an array of the elements in a list value.  It returns the count by storing it in the address *objcPtr*.  Similarly, it returns the array pointer by storing it in the address *objvPtr*. The memory pointed to is managed by Tcl and should not be freed or written to by the caller. If the list is empty, 0 is stored at *objcPtr* and NULL at *objvPtr*. If *objcPtr* points to a variable of type **int** and the list contains more than 2\*\*31 elements, the function returns **TCL\_ERROR**. If *listPtr* is not already a list value, **Tcl\_ListObjGetElements** will attempt to convert it to one; if the conversion fails, it returns **TCL\_ERROR** and leaves an error message in the interpreter's result value if *interp* is not NULL. Otherwise it returns **TCL\_OK** after storing the count and array pointer.

**Tcl\_ListObjLength** returns the number of elements in the list value referenced by *listPtr*. If the value is not already a list value, **Tcl\_ListObjLength** will attempt to convert it to one; if the conversion fails, it returns **TCL\_ERROR** and leaves an error message in the interpreter's result value if *interp* is not NULL. Otherwise it returns **TCL\_OK** after storing the list's length.

The procedure **Tcl\_ListObjIndex** returns a pointer to the value at element *index* in the list referenced by *listPtr*. It returns this value by storing a pointer to it in the address *objPtrPtr*. If *listPtr* does not already refer to a list value, **Tcl\_ListObjIndex** will attempt to convert it to one; if the conversion fails, it returns **TCL\_ERROR** and leaves an error message in the interpreter's result value if *interp* is not NULL. If the index is out of range, that is, *index* is negative or greater than or equal to the number of elements in the list, **Tcl\_ListObjIndex** stores a NULL in *objPtrPtr* and returns **TCL\_OK**. Otherwise it returns **TCL\_OK** after storing the element's value pointer. The reference count for the list element is not incremented; the caller must do that if it needs to retain a pointer to the element, or "bounce" the reference count when the element is no longer needed. This is because a returned element may have a reference count of 0. Abstract Lists create a new element Obj on demand, and do not retain any element Obj values. Therefore, the caller is responsible for freeing the element when it is no longer needed. Note that this is a change from Tcl 8 where all list elements always have a reference count of at least 1. (See **ABSTRACT LIST TYPES**, **STORAGE MANAGEMENT OF VALUES**, Tcl\_BounceRefCount(3), and lseq(n) for more information.)

**Tcl\_ListObjReplace** replaces zero or more elements of the list referenced by *listPtr* with the *objc* values in the array referenced by *objv*. If *listPtr* does not point to a list value, **Tcl\_ListObjReplace** will attempt to convert it to one; if the conversion fails, it returns **TCL\_ERROR** and leaves an error message in the interpreter's result value if *interp* is not NULL. Otherwise, it returns **TCL\_OK** after replacing the values. If *objv* is NULL, no new elements are added. If the argument *first* is zero or negative, it refers to the first element. If *first* is greater than or equal to the number of elements in the list, then no elements are deleted; the new elements are appended to the list. *count* gives the number of elements to replace. If *count* is zero or negative then no elements are deleted; the new elements are simply inserted before the one designated by *first*. **Tcl\_ListObjReplace** invalidates *listPtr*'s old string representation. The reference counts of any elements inserted from *objv* are incremented since the resulting list now refers to them. Similarly, the reference counts for any replaced values are decremented.

Because **Tcl\_ListObjReplace** combines both element insertion and deletion, it can be used to implement a number of list operations. For example, the following code inserts the *objc* values referenced by the array of value pointers *objv* just before the element *index* of the list referenced by *listPtr*:

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

The *count* list elements starting at *first* can be deleted by simply calling **Tcl\_ListObjReplace** with a NULL *objvPtr*:

```
result = Tcl_ListObjReplace(interp, listPtr, first, count,
        0, NULL);
```

**Tcl\_ListObjRange** stores in *objPtrPtr* a pointer to a list value containing all elements in the passed input list *listPtr* at indices between *first* and *last*, both inclusive. An empty list is returned in the case of *first* being greater than *last*.

**Tcl\_ListObjRepeat** stores in *objPtrPtr* a pointer to a list value whose elements are the *objc* elements passed in *objv*, repeated *count* number of times. An error is raised if *count* is negative.

**Tcl\_ListObjReverse** stores in *objPtrPtr* a pointer to a list value containing all elements of the input list *listPtr* in reverse order.

For all three functions, **Tcl\_ListObjRange**, **Tcl\_ListObjRepeat** and **Tcl\_ListObjReverse**, the value passed in *listPtr* need not be unshared. The pointer stored at *objPtrPtr* is guaranteed to be different from *listPtr* but no assumptions should be made about its reference count. In case of errors, the location addressed by *objPtrPtr* may have been modified and should be treated by the caller as undefined.

# Reference count management

Care must be taken by callers when managing the reference count of values passed in as well as values returned from the above functions.

Unless specified otherwise below, the above functions may internally keep a reference to the value passed in *listPtr* and increment its reference count. Therefore, if *listPtr* is newly allocated and has a zero reference count, it should not be freed after the call with **Tcl\_DecrRefCount**. Rather, use **Tcl\_BounceRefCount** instead. Furthermore, if *listPtr* was retrieved from the interpreter result (e.g. via **Tcl\_GetObjResult**) and that is the only reference to the value, it may be deleted if a function sets the interpreter result on error. The more general and safe method for passing values is to increment their reference counts with **Tcl\_IncrRefCount** prior to the call and release them with **Tcl\_DecrRefCount** when the call returns. However, this may preclude certain optimizations based on modifying unshared values in-place.

**Tcl\_NewListObj** always returns a zero-reference object, much like **Tcl\_NewObj**. If a non-NULL *objv* argument is given, the reference counts of the first *objc* values in that array are incremented.

**Tcl\_SetListObj** does not modify the reference count of its *objPtr* argument, but does require that the object be unshared. The reference counts of the first *objc* values in the *objv* array are incremented.

**Tcl\_ListObjGetElements**, **Tcl\_ListObjIndex**, and **Tcl\_ListObjLength** do not modify the reference count of the *listPtr* argument except on error when the interpreter result holds a reference to it as described earlier.

**Tcl\_ListObjAppendList**, **Tcl\_ListObjAppendElement**, and **Tcl\_ListObjReplace** require an unshared *listPtr* argument. **Tcl\_ListObjAppendList** only reads its *elemListPtr* argument. **Tcl\_ListObjAppendElement** increments the reference count of its *objPtr* on success. **Tcl\_ListObjReplace** increments the reference count of the first *objc* values in the *objv* array on success. Note that the same caveat stated earlier applies if the interpreter result holds a reference to *listPtr*.

The pointer to a result value returned in *objPtrPtr* by the functions **Tcl\_ListObjRange**, **Tcl\_ListObjRepeat** and **Tcl\_ListObjReverse** is guaranteed to be different from the **listPtr** passed in. It is therefore safe to manage its reference count independently from that of **listPtr**. No assumptions should be made about its reference count and the standard reference counting idiom should be followed. The caller can pass it to a function such as **Tcl\_SetObjResult**, **Tcl\_ListObjAppendElement** etc. which take ownership of its reference count management or dispose of the value itself with either a **Tcl\_IncrRefCount**/**Tcl\_DecrRefCount** pair or **Tcl\_BounceRefCount**.

