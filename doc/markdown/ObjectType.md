---
CommandName: Tcl_ObjType
ManualSection: 3
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_NewObj(3)
 - Tcl_DecrRefCount(3)
 - Tcl_IncrRefCount(3)
 - Tcl_BounceRefCount(3)
Keywords:
 - internal representation
 - value
 - value type
 - string representation
 - type conversion
Copyright:
 - Copyright (c) 1996-1997 Sun Microsystems, Inc.
---

# Name

Tcl\_RegisterObjType, Tcl\_GetObjType, Tcl\_AppendAllObjTypes, Tcl\_ConvertToType, Tcl\_FreeInternalRep, Tcl\_InitStringRep, Tcl\_HasStringRep, Tcl\_StoreInternalRep, Tcl\_FetchInternalRep  - manipulate Tcl value types

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_RegisterObjType]{.ccmd}[typePtr]{.cargs}
[const Tcl\_ObjType \*]{.ret} [Tcl\_GetObjType]{.ccmd}[typeName]{.cargs}
[int]{.ret} [Tcl\_AppendAllObjTypes]{.ccmd}[interp, objPtr]{.cargs}
[int]{.ret} [Tcl\_ConvertToType]{.ccmd}[interp, objPtr, typePtr]{.cargs}
[void]{.ret} [Tcl\_FreeInternalRep]{.ccmd}[objPtr]{.cargs}
[char \*]{.ret} [Tcl\_InitStringRep]{.ccmd}[objPtr, bytes, numBytes]{.cargs}
[int]{.ret} [Tcl\_HasStringRep]{.ccmd}[objPtr]{.cargs}
[void]{.ret} [Tcl\_StoreInternalRep]{.ccmd}[objPtr, typePtr, irPtr]{.cargs}
[Tcl\_ObjInternalRep \*]{.ret} [Tcl\_FetchInternalRep]{.ccmd}[objPtr, typePtr]{.cargs}
:::

# Arguments

.AP "const Tcl\_ObjType" \*typePtr in Points to the structure containing information about the Tcl value type. This storage must live forever, typically by being statically allocated. .AP "const char" \*typeName in The name of a Tcl value type that **Tcl\_GetObjType** should look up. .AP Tcl\_Interp \*interp in Interpreter to use for error reporting. .AP Tcl\_Obj \*objPtr in For **Tcl\_AppendAllObjTypes**, this points to the value onto which it appends the name of each value type as a list element. For **Tcl\_ConvertToType**, this points to a value that must have been the result of a previous call to **Tcl\_NewObj**. .AP "const char\*" bytes in String representation. .AP "unsigned int" numBytes in Length of the string representation in bytes. .AP "const Tcl\_ObjInternalRep\*" irPtr in Internal object representation. .AP "const Tcl\_ObjType\*" typePtr in Requested internal representation type. 

# Description

The procedures in this man page manage Tcl value types (sometimes referred to as object types or **Tcl\_ObjType**s for historical reasons). They are used to register new value types, look up types, and force conversions from one type to another.

**Tcl\_RegisterObjType** registers a new Tcl value type in the table of all value types that **Tcl\_GetObjType** can look up by name.  There are other value types supported by Tcl as well, which Tcl chooses not to register.  Extensions can likewise choose to register the value types they create or not. The argument *typePtr* points to a Tcl\_ObjType structure that describes the new type by giving its name and by supplying pointers to four procedures that implement the type. If the type table already contains a type with the same name as in *typePtr*, it is replaced with the new type. The Tcl\_ObjType structure is described in the section **THE TCL\_OBJTYPE STRUCTURE** below.

**Tcl\_GetObjType** returns a pointer to the registered Tcl\_ObjType with name *typeName*. It returns NULL if no type with that name is registered.

**Tcl\_AppendAllObjTypes** appends the name of each registered value type as a list element onto the Tcl value referenced by *objPtr*. The return value is **TCL\_OK** unless there was an error converting *objPtr* to a list value; in that case **TCL\_ERROR** is returned.

**Tcl\_ConvertToType** converts a value from one type to another if possible. It creates a new internal representation for *objPtr* appropriate for the target type *typePtr* and sets its *typePtr* member as determined by calling the *typePtr->setFromAnyProc* routine. Any internal representation for *objPtr*'s old type is freed. If an error occurs during conversion, it returns **TCL\_ERROR** and leaves an error message in the result value for *interp* unless *interp* is NULL. Otherwise, it returns **TCL\_OK**. Passing a NULL *interp* allows this procedure to be used as a test whether the conversion can be done (and in fact was done).

In many cases, the *typePtr->setFromAnyProc* routine will set *objPtr->typePtr* to the argument value *typePtr*, but that is no longer guaranteed.  The *setFromAnyProc* is free to set the internal representation for *objPtr* to make use of another related Tcl\_ObjType, if it sees fit.

**Tcl\_FreeInternalRep** performs the function of the existing internal macro **TclInitStringRep**, but is extended to return a pointer to the string rep, and to accept *NULL* as a value for bytes. When bytes is *NULL* and *objPtr* has no string rep, an uninitialzed buffer of *numBytes* bytes is created for filling by the caller. When *bytes* is *NULL* and *objPtr* has a string rep, the string rep will be truncated to a length of *numBytes* bytes. When *numBytes* is greater than zero, and the returned pointer is *NULL*, that indicates a failure to allocate memory for the string representation. The caller may then choose whether to raise an error or panic.

**Tcl\_InitStringRep** performs the function of the existing internal macro **TclInitStringRep**, but is extended to return a pointer to the string rep, and to accept **NULL** as a value for bytes. When *bytes* is **NULL** and *objPtr* has no string rep, an uninitialzed buffer of numBytes bytes is created for filling by the caller. When *bytes* is **NULL** and *objPtr* has a string rep, the string rep will be truncated to a length of numBytes bytes. When numBytes is greater than zero, and the returned pointer is **NULL**, that indicates a failure to allocate memory for the string representation. The caller may then choose whether to raise an error or panic.

**Tcl\_HasStringRep** returns a boolean indicating whether or not a string rep is currently stored in *objPtr*. This is used when the caller wants to act on *objPtr* differently depending on whether or not it is a pure value. Typically this only makes sense in an extension if it is already known that *objPtr* possesses an internal type that is managed by the extension.

**Tcl\_StoreInternalRep** stores in *objPtr* a copy of the internal representation pointed to by *irPtr* and sets its type to *typePtr*. When *irPtr* is *NULL*, this leaves *objPtr* without a representation for type *typePtr*.

**Tcl\_FetchInternalRep** returns a pointer to the internal representation stored in *objPtr* that matches the requested type *typePtr*. If no such internal representation is in *objPtr*, return *NULL*.

This returns a public type

```
typedef union Tcl_ObjInternalRep {...} Tcl_ObjInternalRep
```

where the contents are exactly the existing contents of the union in the *internalRep* field of the *Tcl\_Obj* struct. This definition permits us to pass internal representations and pointers to them as arguments and results in public routines.

# The tcl\_objtype structure

Extension writers can define new value types by defining four to twelve procedures and initializing a Tcl\_ObjType structure to describe the type.  Extension writers may also pass a pointer to their Tcl\_ObjType structure to **Tcl\_RegisterObjType** if they wish to permit other extensions to look up their Tcl\_ObjType by name with the **Tcl\_GetObjType** routine.  The **Tcl\_ObjType** structure is defined as follows:

```
typedef struct {
    const char *name;
    Tcl_FreeInternalRepProc *freeIntRepProc;
    Tcl_DupInternalRepProc *dupIntRepProc;
    Tcl_UpdateStringProc *updateStringProc;
    Tcl_SetFromAnyProc *setFromAnyProc;
    size_t version;
    /* List emulation functions - ObjType Version 1 & 2 */
    Tcl_ObjTypeLengthProc *lengthProc;
    /* List emulation functions - ObjType Version 2 */
    Tcl_ObjTypeIndexProc *indexProc;
    Tcl_ObjTypeSliceProc *sliceProc;
    Tcl_ObjTypeReverseProc *reverseProc;
    Tcl_ObjTypeGetElements *getElementsProc;
    Tcl_ObjTypeSetElement *setElementProc;
    Tcl_ObjTypeReplaceProc *replaceProc;
    Tcl_ObjTypeInOperatorProc *inOperProc;
} Tcl_ObjType;
```

## The name field

The *name* member describes the name of the type, e.g. **int**. When a type is registered, this is the name used by callers of **Tcl\_GetObjType** to lookup the type.  For unregistered types, the *name* field is primarily of value for debugging. The remaining four members are pointers to procedures called by the generic Tcl value code:

## The setfromanyproc field

The *setFromAnyProc* member contains the address of a function called to create a valid internal representation from a value's string representation.

```
typedef int Tcl_SetFromAnyProc(
        Tcl_Interp *interp,
        Tcl_Obj *objPtr);
```

If an internal representation cannot be created from the string, it returns **TCL\_ERROR** and puts a message describing the error in the result value for *interp* unless *interp* is NULL. If *setFromAnyProc* is successful, it stores the new internal representation, sets *objPtr*'s *typePtr* member to point to the **Tcl\_ObjType** struct corresponding to the new internal representation, and returns **TCL\_OK**. Before setting the new internal representation, the *setFromAnyProc* must free any internal representation of *objPtr*'s old type; it does this by calling the old type's *freeIntRepProc* if it is not NULL.

As an example, the *setFromAnyProc* for the built-in Tcl list type gets an up-to-date string representation for *objPtr* by calling **Tcl\_GetStringFromObj**. It parses the string to verify it is in a valid list format and to obtain each element value in the list, and, if this succeeds, stores the list elements in *objPtr*'s internal representation and sets *objPtr*'s *typePtr* member to point to the list type's Tcl\_ObjType structure.

Do not release *objPtr*'s old internal representation unless you replace it with a new one or reset the *typePtr* member to NULL.

The *setFromAnyProc* member may be set to NULL, if the routines making use of the internal representation have no need to derive that internal representation from an arbitrary string value.  However, in this case, passing a pointer to the type to **Tcl\_ConvertToType** will lead to a panic, so to avoid this possibility, the type should *not* be registered.

## The updatestringproc field

The *updateStringProc* member contains the address of a function called to create a valid string representation from a value's internal representation.

```
typedef void Tcl_UpdateStringProc(
        Tcl_Obj *objPtr);
```

*objPtr*'s *bytes* member is always NULL when it is called. It must always set *bytes* non-NULL before returning. We require the string representation's byte array to have a null after the last byte, at offset *length*, and to have no null bytes before that; this allows string representations to be treated as conventional null character-terminated C strings. These restrictions are easily met by using Tcl's internal UTF encoding for the string representation, same as one would do for other Tcl routines accepting string values as arguments. Storage for the byte array must be allocated in the heap by **Tcl\_Alloc**. Note that *updateStringProc*s must allocate enough storage for the string's bytes and the terminating null byte.

The *updateStringProc* for Tcl's built-in double type, for example, calls Tcl\_PrintDouble to write to a buffer of size TCL\_DOUBLE\_SPACE, then allocates and copies the string representation to just enough space to hold it.  A pointer to the allocated space is stored in the *bytes* member.

The *updateStringProc* member may be set to NULL, if the routines making use of the internal representation are written so that the string representation is never invalidated.  Failure to meet this obligation will lead to panics or crashes when **Tcl\_GetStringFromObj** or other similar routines ask for the string representation.

## The dupintrepproc field

The *dupIntRepProc* member contains the address of a function called to copy an internal representation from one value to another.

```
typedef void Tcl_DupInternalRepProc(
        Tcl_Obj *srcPtr,
        Tcl_Obj *dupPtr);
```

*dupPtr*'s internal representation is made a copy of *srcPtr*'s internal representation. Before the call, *srcPtr*'s internal representation is valid and *dupPtr*'s is not. *srcPtr*'s value type determines what copying its internal representation means.

For example, the *dupIntRepProc* for the Tcl integer type simply copies an integer. The built-in list type's *dupIntRepProc* uses a far more sophisticated scheme to continue sharing storage as much as it reasonably can.

## The freeintrepproc field

The *freeIntRepProc* member contains the address of a function that is called when a value is freed.

```
typedef void Tcl_FreeInternalRepProc(
        Tcl_Obj *objPtr);
```

The *freeIntRepProc* function can deallocate the storage for the value's internal representation and do other type-specific processing necessary when a value is freed.

For example, the list type's *freeIntRepProc* respects the storage sharing scheme established by the *dupIntRepProc* so that it only frees storage when the last value sharing it is being freed.

The *freeIntRepProc* member can be set to NULL to indicate that the internal representation does not require freeing. The *freeIntRepProc* implementation must not access the *bytes* member of the value, since Tcl makes its own internal uses of that field during value deletion.  The defined tasks for the *freeIntRepProc* have no need to consult the *bytes* member.

Note that if a subsidiary value has its reference count reduced to zero during the running of a *freeIntRepProc*, that value may be not freed immediately, in order to limit stack usage. However, the value will be freed before the outermost current **Tcl\_DecrRefCount** returns.

## The version field

The *version* member provides for future extensibility of the structure and should be set to **TCL\_OBJTYPE\_V0** for compatibility of ObjType definitions prior to version 9.0. Specifics about versions will be described further in the sections below.

# Abstract list types

Additional fields in the Tcl\_ObjType descriptor allow for control over how custom data values can be manipulated using Tcl's List commands without converting the value to a List type. This requires the custom type to provide functions that will perform the given operation on the custom data representation.  Not all functions are required. In the absence of a particular function (set to NULL), the fallback is to allow the internal List operation to perform the operation, which may possibly cause the value type to be converted to a traditional list.

## Scalar value types

For a custom value type that is scalar or atomic in nature, i.e., not a divisible collection, version **TCL\_OBJTYPE\_V1** is recommended. In this case, List commands will treat the scalar value as if it where a list of length 1, and not convert the value to a List type.

## Version 2: abstract lists

Version 2, **TCL\_OBJTYPE\_V2**, allows full List support when the functions described below are provided.  This allows for script level use of the List commands without causing the type of the Tcl\_Obj value to be converted to a list.  Unless specified otherwise, all functions specific to Version 2 should return **TCL\_OK** on success and **TCL\_ERROR** on failure.  In the case that a **Tcl\_Obj\*** is also returned, the reference count of the returned **Tcl\_Obj** should not be incremented so, for example, if a new **Tcl\_Obj** value is returned it should have a reference count of zero.  The functions should not assume that any **Tcl\_Obj** passed in is unshared. 

## The lengthproc field

The **LengthProc** function correlates with the **Tcl\_ListObjLength** C API. The function returns the number of elements in the list. It is used in every List operation and is required for all Abstract List implementations.

```
typedef Tcl_Size
(Tcl_ObjTypeLengthProc) (Tcl_Obj *listPtr);
```

## The indexproc field

The **IndexProc** function correlates with with the **Tcl\_ListObjIndex** C API. The function should store a pointer to the element at the specified **index** in **\*elemObj**. Indices that are out of bounds should not be treated as errors; rather, the function should store a null pointer and return TCL\_OK.

```
typedef int (Tcl_ObjTypeIndexProc) (
    Tcl_Interp *interp,
    Tcl_Obj *listPtr,
    Tcl_Size index,
    Tcl_Obj** elemObj);
```

## The sliceproc field

The **SliceProc** correlates with the [lrange] command, returning a new List or Abstract List for the portion of the original list specified.

```
typedef int (Tcl_ObjTypeSliceProc) (
    Tcl_Interp *interp,
    Tcl_Obj *listPtr,
    Tcl_Size fromIdx,
    Tcl_Size toIdx,
    Tcl_Obj **newObjPtr);
```

## The reverseproc field

The **ReverseProc** correlates with the [lreverse] command, returning a List or Abstract List that has the same elements as the input Abstract List, but in reverse order.

```
typedef int (Tcl_ObjTypeReverseProc) (
    Tcl_Interp *interp,
    Tcl_Obj *listPtr,
    Tcl_Obj **newObjPtr);
```

## The getelements field

The **GetElements** function returns a count and a pointer to an array of Tcl\_Obj values for the entire Abstract List. This correlates to the **Tcl\_ListObjGetElements** C API call.

```
typedef int (Tcl_ObjTypeGetElements) (
    Tcl_Interp *interp,
    Tcl_Obj *listPtr,
    Tcl_Size *objcptr,
    Tcl_Obj ***objvptr);
```

## The setelement field

The **SetElement** function replaces the element within the specified list at the give index. This function correlates to the [lset] command.

```
typedef Tcl_Obj *(Tcl_ObjTypeSetElement) (
    Tcl_Interp *interp,
    Tcl_Obj *listPtr,
    Tcl_Size indexCount,
    Tcl_Obj *const indexArray[],
    Tcl_Obj *valueObj);
```

## Replaceproc field

The **ReplaceProc** returns a new list after modifying the list replacing the elements to be deleted, and adding the elements to be inserted. This function correlates to the **Tcl\_ListObjReplace** C API.

```
typedef int (Tcl_ObjTypeReplaceProc) (
    Tcl_Interp *interp,
    Tcl_Obj *listObj,
    Tcl_Size first,
    Tcl_Size numToDelete,
    Tcl_Size numToInsert,
    Tcl_Obj *const insertObjs[]);
```

## The inoperproc field

The **InOperProc** function determines whether the value is present in the given list, according to equivalent string comparison of elements. The **boolResult** is set to 1 (true) if the value is present, and 0 (false) if it is not present. This function implements the "in" and "ni" math operators for an abstract list.

```
typedef int (Tcl_ObjTypeInOperatorProc) (
    Tcl_Interp *interp,
    Tcl_Obj *valueObj,
    Tcl_Obj *listObj,
    int *boolResult);
```

# Reference count management

The *objPtr* argument to **Tcl\_AppendAllObjTypes** should be an unshared value; this function will not modify the reference count of that value, but will modify its contents. If *objPtr* is not (interpretable as) a list, this function will set the interpreter result and produce an error; using an unshared empty value is strongly recommended.

The *objPtr* argument to **Tcl\_ConvertToType** can have any non-zero reference count; this function will not modify the reference count, but may write to the interpreter result on error so values that originate from there should have an additional reference made before calling this.

None of the callback functions in the **Tcl\_ObjType** structure should modify the reference count of their arguments, but if the values contain subsidiary values (e.g., the elements of a list or the keys of a dictionary) then those subsidiary values may have their reference counts modified.


[lrange]: lrange.md
[lreverse]: lreverse.md
[lset]: lset.md

