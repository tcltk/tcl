---
CommandName: Tcl_ByteArrayObj
ManualSection: 3
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_GetStringFromObj
 - Tcl_NewObj
 - Tcl_IncrRefCount
 - Tcl_DecrRefCount
Keywords:
 - value
 - binary data
 - byte array
 - utf
 - unicode
Copyright:
 - Copyright (c) 1997 Sun Microsystems, Inc.
---

# Name

Tcl_NewByteArrayObj, Tcl_SetByteArrayObj, Tcl_GetBytesFromObj, Tcl_GetByteArrayFromObj, Tcl_SetByteArrayLength - manipulate a Tcl value as an array of bytes

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Obj *]{.ret} [Tcl_NewByteArrayObj]{.ccmd}[bytes, numBytes]{.cargs}
[Tcl_SetByteArrayObj]{.ccmd}[objPtr, bytes, numBytes]{.cargs}
[unsigned char *]{.ret} [Tcl_GetBytesFromObj]{.ccmd}[interp, objPtr, numBytesPtr]{.cargs}
[unsigned char *]{.ret} [Tcl_GetByteArrayFromObj]{.ccmd}[objPtr, numBytesPtr]{.cargs}
[unsigned char *]{.ret} [Tcl_SetByteArrayLength]{.ccmd}[objPtr, numBytes]{.cargs}
:::

# Arguments

.AP "const unsigned char" *bytes in The array of bytes used to initialize or set a byte-array value. May be NULL even if *numBytes* is non-zero. .AP Tcl_Size numBytes in The number of bytes in the array. .AP Tcl_Obj *objPtr in/out For **Tcl_SetByteArrayObj**, this points to an unshared value to be overwritten by a byte-array value.  For **Tcl_GetBytesFromObj**, **Tcl_GetByteArrayFromObj** and **Tcl_SetByteArrayLength**, this points to the value from which to extract an array of bytes. .AP Tcl_Interp *interp in Interpreter to use for error reporting. .AP "Tcl_Size \&| int" *numBytesPtr out Points to space where the number of bytes in the array may be written. May be (Tcl_Size *)NULL when not used. If it points to a variable which type is not **Tcl_Size**, a compiler warning will be generated. If your extensions is compiled with -DTCL_8_API, this function will return NULL for byte arrays larger than INT_MAX (which should trigger proper error-handling), otherwise expect it to crash.

# Description

These routines are used to create, modify, store, transfer, and retrieve arbitrary binary data in Tcl values.  Specifically, data that can be represented as a sequence of arbitrary byte values is supported. This includes data read from binary channels, values created by the **binary** command, encrypted data, or other information representable as a finite byte sequence.

A byte is an 8-bit quantity with no inherent meaning.  When the 8 bits are interpreted as an integer value, the range of possible values is (0-255). The C type best suited to store a byte is the **unsigned char**. An **unsigned char** array of size *N* stores an arbitrary binary value of size *N* bytes.  We call this representation a byte-array. Here we document the routines that allow us to operate on Tcl values as byte-arrays.

All Tcl values must correspond to a string representation. When a byte-array value must be processed as a string, the sequence of *N* bytes is transformed into the corresponding sequence of *N* characters, where each byte value transforms to the same character codepoint value in the range (U+0000 - U+00FF).  Obtaining the string representation of a byte-array value (by calling **Tcl_GetStringFromObj**) produces this string in Tcl's usual Modified UTF-8 encoding.

**Tcl_NewByteArrayObj** and **Tcl_SetByteArrayObj** create a new value or overwrite an existing unshared value, respectively, to hold a byte-array value of *numBytes* bytes.  When a caller passes a non-NULL value of *bytes*, it must point to memory from which *numBytes* bytes can be read.  These routines allocate *numBytes* bytes of memory, copy *numBytes* bytes from *bytes* into it, and keep the result in the internal representation of the new or overwritten value. When the caller passes a NULL value of *bytes*, the data copying step is skipped, and the bytes stored in the value are undefined. A *bytes* value of NULL is useful only when the caller will arrange to write known contents into the byte-array through a pointer retrieved by a call to one of the routines explained below.  **Tcl_NewByteArrayObj** returns a pointer to the created value with a reference count of zero. **Tcl_SetByteArrayObj** overwrites and invalidates any old contents of the unshared *objPtr* as appropriate, and keeps its reference count (0 or 1) unchanged.  The value produced by these routines has no string representation.  Any memory allocation failure may cause a panic.

**Tcl_GetBytesFromObj** performs the opposite function of **Tcl_SetByteArrayObj**, providing access to read a byte-array from a Tcl value that was previously written into it.  When *objPtr* is a value previously produced by **Tcl_NewByteArrayObj** or **Tcl_SetByteArrayObj**, then **Tcl_GetBytesFromObj** returns a pointer to the byte-array kept in the value's internal representation. If the caller provides a non-NULL value for *numBytesPtr*, it must point to memory where **Tcl_GetBytesFromObj** can write the number of bytes in the value's internal byte-array.  With both pieces of information, the caller is able to retrieve any information about the contents of that byte-array that it seeks.  When *objPtr* does not already contain an internal byte-array, **Tcl_GetBytesFromObj** will try to create one from the value's string representation.  Any string value that does not include any character codepoints outside the range (U+0000 - U+00FF) will successfully translate to a unique byte-array value.  With the created byte-array, the routine returns as before.  For any string representation which does contain a forbidden character codepoint, the conversion fails, and **Tcl_GetBytesFromObj** returns NULL to signal that failure.  On failure, nothing will be written to *numBytesPtr*, and if the *interp* argument is non-NULL, then error messages and codes are left in it recording the error.

**Tcl_GetByteArrayFromObj** performs exactly the same function as **Tcl_GetBytesFromObj** does when called with the *interp* argument passed the value NULL.  This is incompatible with the way **Tcl_GetByteArrayFromObj** functioned in Tcl 8. **Tcl_GetBytesFromObj** is the more capable interface and should usually be favored for use over **Tcl_GetByteArrayFromObj**.

On success, both **Tcl_GetBytesFromObj** and **Tcl_GetByteArrayFromObj** return a pointer into the internal representation of a **Tcl_Obj**. That pointer must not be freed by the caller, and should not be retained for use beyond the known time the internal representation of the value has not been disturbed.  The pointer may be used to overwrite the byte contents of the internal representation, so long as the value is unshared and any string representation is invalidated.

On success, both **Tcl_GetBytesFromObj** and **Tcl_GetByteArrayFromObj** write the number of bytes in the byte-array value of *objPtr* to the space pointed to by *numBytesPtr*.  This space may be of type **Tcl_Size** or of type **int**.  It is recommended that callers provide a **Tcl_Size** space for this purpose.  If the caller provides only an **int** space and the number of bytes in the byte-array value of *objPtr* is greater than **INT_MAX**, the routine will fail due to being unable to correctly report the byte-array size to the caller. The ability to provide an **int** space is best considered a migration aid for codebases constrained to continue operating with Tcl releases older than 9.0.

**Tcl_SetByteArrayLength** enables a caller to change the size of a byte-array in the internal representation of an unshared *objPtr* to become *numBytes* bytes.  This is most often useful after the bytes of the internal byte-array have been directly overwritten and it has been discovered that the required size differs from the first estimate used in the allocation. **Tcl_SetByteArrayLength** returns a pointer to the resized byte-array.  Because resizing the byte-array changes the internal representation, **Tcl_SetByteArrayLength** also invalidates any string representation in *objPtr*.  If resizing grows the byte-array, the new byte values are undefined.  If *objPtr* does not already possess an internal byte-array, one is produced in the same way that **Tcl_GetBytesFromObj** does, also returning NULL when any characters of the value in *objPtr* (up to *numBytes* of them) are not valid bytes.

# Reference count management

**Tcl_NewByteArrayObj** always returns a zero-reference object, much like **Tcl_NewObj**.

**Tcl_SetByteArrayObj** and **Tcl_SetByteArrayLength** do not modify the reference count of their *objPtr* arguments, but do require that the object be unshared.

**Tcl_GetBytesFromObj** and **Tcl_GetByteArrayFromObj** do not modify the reference count of *objPtr*; they only read. 

