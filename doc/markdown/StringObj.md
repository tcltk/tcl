---
CommandName: Tcl_StringObj
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_NewObj(3)
 - Tcl_IncrRefCount(3)
 - Tcl_DecrRefCount(3)
 - format(n)
 - sprintf(3)
Keywords:
 - append
 - internal representation
 - value
 - value type
 - string value
 - string type
 - string representation
 - concat
 - concatenate
 - unicode
Copyright:
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

Tcl\_NewStringObj, Tcl\_NewUnicodeObj, Tcl\_SetStringObj, Tcl\_SetUnicodeObj, Tcl\_GetStringFromObj, Tcl\_GetString, Tcl\_GetUnicodeFromObj, Tcl\_GetUnicode, Tcl\_GetUniChar, Tcl\_GetCharLength, Tcl\_GetRange, Tcl\_AppendToObj, Tcl\_AppendUnicodeToObj, Tcl\_AppendObjToObj, Tcl\_AppendStringsToObj, Tcl\_AppendLimitedToObj, Tcl\_Format, Tcl\_AppendFormatToObj, Tcl\_ObjPrintf, Tcl\_AppendPrintfToObj, Tcl\_SetObjLength, Tcl\_AttemptSetObjLength, Tcl\_ConcatObj, Tcl\_IsEmpty - manipulate Tcl values as strings

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Obj \*]{.ret} [Tcl\_NewStringObj]{.ccmd}[bytes, length]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_NewUnicodeObj]{.ccmd}[unicode, numChars]{.cargs}
[void]{.ret} [Tcl\_SetStringObj]{.ccmd}[objPtr, bytes, length]{.cargs}
[void]{.ret} [Tcl\_SetUnicodeObj]{.ccmd}[objPtr, unicode, numChars]{.cargs}
[char \*]{.ret} [Tcl\_GetStringFromObj]{.ccmd}[objPtr, lengthPtr]{.cargs}
[char \*]{.ret} [Tcl\_GetString]{.ccmd}[objPtr]{.cargs}
[Tcl\_UniChar \*]{.ret} [Tcl\_GetUnicodeFromObj]{.ccmd}[objPtr, lengthPtr]{.cargs}
[Tcl\_UniChar \*]{.ret} [Tcl\_GetUnicode]{.ccmd}[objPtr]{.cargs}
[int]{.ret} [Tcl\_GetUniChar]{.ccmd}[objPtr, index]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_GetCharLength]{.ccmd}[objPtr]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_GetRange]{.ccmd}[objPtr, first, last]{.cargs}
[void]{.ret} [Tcl\_AppendToObj]{.ccmd}[objPtr, bytes, length]{.cargs}
[void]{.ret} [Tcl\_AppendUnicodeToObj]{.ccmd}[objPtr, unicode, numChars]{.cargs}
[void]{.ret} [Tcl\_AppendObjToObj]{.ccmd}[objPtr, appendObjPtr]{.cargs}
[void]{.ret} [Tcl\_AppendStringsToObj]{.ccmd}[objPtr, string, string, ...= §(char \*)NULL]{.cargs}
[void]{.ret} [Tcl\_AppendLimitedToObj]{.ccmd}[objPtr, bytes, length, limit, ellipsis]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_Format]{.ccmd}[interp, format, objc, objv]{.cargs}
[int]{.ret} [Tcl\_AppendFormatToObj]{.ccmd}[interp, objPtr, format, objc, objv]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_ObjPrintf]{.ccmd}[format, ...]{.cargs}
[void]{.ret} [Tcl\_AppendPrintfToObj]{.ccmd}[objPtr, format, ...]{.cargs}
[void]{.ret} [Tcl\_SetObjLength]{.ccmd}[objPtr, newLength]{.cargs}
[int]{.ret} [Tcl\_AttemptSetObjLength]{.ccmd}[objPtr, newLength]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_ConcatObj]{.ccmd}[objc, objv]{.cargs}
[int]{.ret} [Tcl\_IsEmpty]{.ccmd}[fIobjPtr]{.cargs}
:::

# Arguments

.AP "const char" \*bytes in Points to a TUTF-8 (Tcl's internal modified UTF-8 encoding) byte sequence bytes used to set or append to a string value. This byte array may contain embedded null characters unless *numChars* is negative.  (Applications needing null bytes should represent them as the two-byte sequence *0xC0 0x80* used in TUTF-8 to represent a null byte, use **Tcl\_ExternalToUtf** to convert, or **Tcl\_NewByteArrayObj** if the string is a collection of uninterpreted bytes.) .AP Tcl\_Size length in The number of bytes to copy from *bytes* when initializing, setting, or appending to a string value. If negative, all bytes up to the first null are used. .AP "const Tcl\_UniChar" \*unicode in Points to the first byte of an array of Unicode characters used to set or append to a string value. This byte array may contain embedded null characters unless *numChars* is negative. .AP Tcl\_Size numChars in The number of Unicode characters to copy from *unicode* when initializing, setting, or appending to a string value. If negative, all characters up to the first null character are used. .AP Tcl\_Size index in The index of the Unicode character to return. .AP Tcl\_Size first in The index of the first Unicode character in the Unicode range to be returned as a new value. If negative, behave the same as if the value was 0. .AP Tcl\_Size last in The index of the last Unicode character in the Unicode range to be returned as a new value. If negative, take all characters up to the last one available. .AP Tcl\_Obj \*objPtr in/out A pointer to a value to read, or to an unshared value to modify. .AP Tcl\_Obj \*appendObjPtr in The value to append to *objPtr* in **Tcl\_AppendObjToObj**. .AP "Tcl\_Size | int" \*lengthPtr out The location where **Tcl\_GetStringFromObj** will store the length of a value's string representation. May be (Tcl\_Size \*)NULL when not used. If it points to a variable which type is not **Tcl\_Size**, a compiler warning will be generated. If your extensions is compiled with **-DTCL\_8\_API**, this function will panic for strings with more than INT\_MAX bytes/characters, otherwise expect it to crash. .AP "const char" \*string in Null-terminated string value to append to *objPtr*. .AP Tcl\_Size limit in Maximum number of bytes to be appended. .AP "const char" \*ellipsis in Suffix to append when the limit leads to string truncation. If NULL is passed then the suffix "..." is used. .AP "const char" \*format in Format control string including % conversion specifiers. .AP Tcl\_Size objc in The number of elements to format or concatenate. .AP Tcl\_Obj \*objv[] in The array of values to format or concatenate. .AP Tcl\_Size newLength in New length for the string value of *objPtr*, not including the final null character.

# Description

The procedures described in this manual entry allow Tcl values to be manipulated as string values.  They use the internal representation of the value to store additional information to make the string manipulations more efficient.  In particular, they make a series of append operations efficient by allocating extra storage space for the string so that it does not have to be copied for each append. Also, indexing and length computations are optimized because the Unicode string representation is calculated and cached as needed. When using the **Tcl\_Append\*** family of functions where the interpreter's result is the value being appended to, it is important to call Tcl\_ResetResult first to ensure you are not unintentionally appending to existing data in the result value.

**Tcl\_NewStringObj** and **Tcl\_SetStringObj** create a new value or modify an existing value to hold a copy of the string given by *bytes* and *length*.  **Tcl\_NewUnicodeObj** and **Tcl\_SetUnicodeObj** create a new value or modify an existing value to hold a copy of the Unicode string given by *unicode* and *numChars*.  **Tcl\_NewStringObj** and **Tcl\_NewUnicodeObj** return a pointer to a newly created value with reference count zero. All four procedures set the value to hold a copy of the specified string.  **Tcl\_SetStringObj** and **Tcl\_SetUnicodeObj** free any old string representation as well as any old internal representation of the value.

**Tcl\_GetStringFromObj** and **Tcl\_GetString** return a value's string representation.  This is given by the returned byte pointer and (for **Tcl\_GetStringFromObj**) length, which is stored in *lengthPtr* if it is non-NULL.  If the value's UTF string representation is invalid (its byte pointer is NULL), the string representation is regenerated from the value's internal representation.  The storage referenced by the returned byte pointer is owned by the value manager.  It is passed back as a writable pointer so that extension author creating their own **Tcl\_ObjType** will be able to modify the string representation within the **Tcl\_UpdateStringProc** of their **Tcl\_ObjType**.  Except for that limited purpose, the pointer returned by **Tcl\_GetStringFromObj** or **Tcl\_GetString** should be treated as read-only.  It is recommended that this pointer be assigned to a (const char \*) variable. Even in the limited situations where writing to this pointer is acceptable, one should take care to respect the copy-on-write semantics required by **Tcl\_Obj**'s, with appropriate calls to **Tcl\_IsShared** and **Tcl\_DuplicateObj** prior to any in-place modification of the string representation. The procedure **Tcl\_GetString** is used in the common case where the caller does not need the length of the string representation.

**Tcl\_GetUnicodeFromObj** and **Tcl\_GetUnicode** return a value's value as a Unicode string.  This is given by the returned pointer and (for **Tcl\_GetUnicodeFromObj**) length, which is stored in *lengthPtr* if it is non-NULL.  The storage referenced by the returned byte pointer is owned by the value manager and should not be modified by the caller.  The procedure **Tcl\_GetUnicode** is used in the common case where the caller does not need the length of the unicode string representation.

**Tcl\_GetUniChar** returns the *index*'th character in the value's Unicode representation. If the index is out of range it returns -1;

**Tcl\_GetRange** returns a newly created value comprised of the characters between *first* and *last* (inclusive) in the value's Unicode or byte-array representation.  If the value is not a byte-array and the values Unicode representation is invalid, the Unicode representation is regenerated from the value's string representation.  If *first* is negative, then the returned string starts at the beginning of the value. If *last* is negative, then the returned string ends at the end of the value.

**Tcl\_GetCharLength** returns the number of characters (as opposed to bytes) in the string value.

**Tcl\_AppendToObj** appends the data given by *bytes* and *length* to the string representation of the value specified by *objPtr*.  If the value has an invalid string representation, then an attempt is made to convert *bytes* to the Unicode format.  If the conversion is successful, then the converted form of *bytes* is appended to the value's Unicode representation. Otherwise, the value's Unicode representation is invalidated and converted to the UTF format, and *bytes* is appended to the value's new string representation. Eventually buffer growth is done by large allocations to optimize multiple calls.

**Tcl\_AppendUnicodeToObj** appends the Unicode string given by *unicode* and *numChars* to the value specified by *objPtr*.  If the value has an invalid Unicode representation, then *unicode* is converted to the UTF format and appended to the value's string representation.  Appends are optimized to handle repeated appends relatively efficiently (it over-allocates the string or Unicode space to avoid repeated reallocations and copies of value's string value).

**Tcl\_AppendObjToObj** is similar to **Tcl\_AppendToObj**, but it appends the string or Unicode value (whichever exists and is best suited to be appended to *objPtr*) of *appendObjPtr* to *objPtr*.

**Tcl\_AppendStringsToObj** is similar to **Tcl\_AppendToObj** except that it can be passed more than one value to append and each value must be a null-terminated string (i.e. none of the values may contain internal null characters).  Any number of *string* arguments may be provided, but the last argument must be (char \*)NULL to indicate the end of the list.

**Tcl\_AppendLimitedToObj** is similar to **Tcl\_AppendToObj** except that it imposes a limit on how many bytes are appended. This can be handy when the string to be appended might be very large, but the value being constructed should not be allowed to grow without bound. A common usage is when constructing an error message, where the end result should be kept short enough to be read. Bytes from *bytes* are appended to *objPtr*, but no more than *limit* bytes total are to be appended. If the limit prevents all *length* bytes that are available from being appended, then the appending is done so that the last bytes appended are from the string *ellipsis*. This allows for an indication of the truncation to be left in the string. When *length* is negative, all bytes up to the first zero byte are appended, subject to the limit. When *ellipsis* is NULL, the default string **...** is used. When *ellipsis* is non-NULL, it must point to a zero-byte-terminated string in Tcl's internal UTF encoding. The number of bytes appended can be less than the lesser of *length* and *limit* when appending fewer bytes is necessary to append only whole multi-byte characters.

**Tcl\_Format** is the C-level interface to the engine of the [format] command.  The actual command procedure for [format] is little more than

```
Tcl_Format(interp, Tcl_GetString(objv[1]), objc-2, objv+2);
```

The *objc* Tcl\_Obj values in *objv* are formatted into a string according to the conversion specification in *format* argument, following the documentation for the [format] command.  The resulting formatted string is converted to a new Tcl\_Obj with refcount of zero and returned. If some error happens during production of the formatted string, NULL is returned, and an error message is recorded in *interp*, if *interp* is non-NULL.

**Tcl\_AppendFormatToObj** is an appending alternative form of **Tcl\_Format** with functionality equivalent to:

```
Tcl_Obj *newPtr = Tcl_Format(interp, format, objc, objv);
if (newPtr == NULL) return TCL_ERROR;
Tcl_AppendObjToObj(objPtr, newPtr);
Tcl_DecrRefCount(newPtr);
return TCL_OK;
```

but with greater convenience and efficiency when the appending functionality is needed.

**Tcl\_ObjPrintf** serves as a replacement for the common sequence

```
char buf[SOME_SUITABLE_LENGTH];
sprintf(buf, format, ...);
Tcl_NewStringObj(buf, -1);
```

but with greater convenience and no need to determine **SOME\_SUITABLE\_LENGTH**. The formatting is done with the same core formatting engine used by **Tcl\_Format**.  This means the set of supported conversion specifiers is that of the [format] command but the behavior is as similar as possible to **sprintf**. The "hh" and (Microsoft-specific) "w" format specifiers are not supported. The "L" format specifier means that an "mp\_int \*" argument is expected (or a "long double" in combination with **[aAeEgGaA]**). When a conversion specifier passed to **Tcl\_ObjPrintf** includes a precision, the value is taken as a number of bytes, as **sprintf** does, and not as a number of characters, as [format] does.  This is done on the assumption that C code is more likely to know how many bytes it is passing around than the number of encoded characters those bytes happen to represent.  The variable number of arguments passed in should be of the types that would be suitable for passing to **sprintf**.  Note in this example usage, *x* is of type **int**.

```
int x = 5;
Tcl_Obj *objPtr = Tcl_ObjPrintf("Value is %d", x);
```

If the value of *format* contains internal inconsistencies or invalid specifier formats, the formatted string result produced by **Tcl\_ObjPrintf** will be an error message describing the error. It is impossible however to provide runtime protection against mismatches between the format and any subsequent arguments. Compile-time protection may be provided by some compilers.

**Tcl\_AppendPrintfToObj** is an appending alternative form of **Tcl\_ObjPrintf** with functionality equivalent to

```
Tcl_Obj *newPtr = Tcl_ObjPrintf(format, ...);
Tcl_AppendObjToObj(objPtr, newPtr);
Tcl_DecrRefCount(newPtr);
```

but with greater convenience and efficiency when the appending functionality is needed.

When printing integer types defined by Tcl, such as **Tcl\_Size** or **Tcl\_WideInt**, a format size specifier is needed as the integer width of those types is dependent on the Tcl version, platform and compiler. To accomodate these differences, Tcl defines C preprocessor symbols **TCL\_LL\_MODIFER** and **TCL\_SIZE\_MODIFER** for use when formatting values of type **Tcl\_WideInt** and **Tcl\_Size** respectively. Their usage is illustrated by

```
Tcl_WideInt wide;
Tcl_Size len;
Tcl_Obj *wideObj = Tcl_ObjPrintf("wide = %" TCL_LL_MODIFIER "d", wide);
Tcl_Obj *lenObj = Tcl_ObjPrintf("len = %" TCL_SIZE_MODIFIER "d", len);
```

The **Tcl\_SetObjLength** procedure changes the length of the string value of its *objPtr* argument.  If the *newLength* argument is greater than the space allocated for the value's string, then the string space is reallocated and the old value is copied to the new space; the bytes between the old length of the string and the new length may have arbitrary values. If the *newLength* argument is less than the current length of the value's string, with *objPtr->length* is reduced without reallocating the string space; the original allocated size for the string is recorded in the value, so that the string length can be enlarged in a subsequent call to **Tcl\_SetObjLength** without reallocating storage.  In all cases **Tcl\_SetObjLength** leaves a null character at *objPtr->bytes[newLength]*.

**Tcl\_AttemptSetObjLength** is identical in function to **Tcl\_SetObjLength** except that if sufficient memory to satisfy the request cannot be allocated, it does not cause the Tcl interpreter to **panic**.  Thus, if *newLength* is greater than the space allocated for the value's string, and there is not enough memory available to satisfy the request, **Tcl\_AttemptSetObjLength** will take no action and return 0 to indicate failure.  If there is enough memory to satisfy the request, **Tcl\_AttemptSetObjLength** behaves just like **Tcl\_SetObjLength** and returns 1 to indicate success.

The **Tcl\_ConcatObj** function returns a new string value whose value is the space-separated concatenation of the string representations of all of the values in the *objv* array. **Tcl\_ConcatObj** eliminates leading and trailing white space as it copies the string representations of the *objv* array to the result. If an element of the *objv* array consists of nothing but white space, then that value is ignored entirely. This white-space removal was added to make the output of the [concat] command cleaner-looking. **Tcl\_ConcatObj** returns a pointer to a newly-created value whose ref count is zero.

The **Tcl\_IsEmpty** function returns 1 if *objPtr* is the empty string, 0 otherwise. It doesn't generate the string representation (unless there is no other way to do it), so it can safely be called on lists with billions of elements, or any other data structure for which it is impossible or expensive to construct the string representation.

# Reference count management

**Tcl\_NewStringObj**, **Tcl\_NewUnicodeObj**, **Tcl\_Format**, **Tcl\_ObjPrintf**, and **Tcl\_ConcatObj** always return a zero-reference object, much like **Tcl\_NewObj**.

**Tcl\_GetStringFromObj**, **Tcl\_GetString**, **Tcl\_GetUnicodeFromObj**, **Tcl\_GetUnicode**, **Tcl\_GetUniChar**, **Tcl\_GetCharLength**, and **Tcl\_GetRange** all only work with an existing value; they do not manipulate its reference count in any way.

**Tcl\_SetStringObj**, **Tcl\_SetUnicodeObj**, **Tcl\_AppendToObj**, **Tcl\_AppendUnicodeToObj**, **Tcl\_AppendObjToObj**, **Tcl\_AppendStringsToObj**, **Tcl\_AppendStringsToObjVA**, **Tcl\_AppendLimitedToObj**, **Tcl\_AppendFormatToObj**, **Tcl\_AppendPrintfToObj**, **Tcl\_SetObjLength**, and **Tcl\_AttemptSetObjLength** and require their *objPtr* to be an unshared value (i.e, a reference count no more than 1) as they will modify it.

Additional arguments to the above functions (the *appendObjPtr* argument to **Tcl\_AppendObjToObj**, values in the *objv* argument to **Tcl\_Format**, **Tcl\_AppendFormatToObj**, and **Tcl\_ConcatObj**) can have any reference count, but reference counts of zero are not recommended.

**Tcl\_Format** and **Tcl\_AppendFormatToObj** may modify the interpreter result, which involves changing the reference count of a value. 


[concat]: concat.md
[format]: format.md

