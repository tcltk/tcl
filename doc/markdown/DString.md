---
CommandName: Tcl_DString
ManualSection: 3
Version: 7.4
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - append
 - dynamic string
 - free
 - result
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_DStringInit, Tcl_DStringAppend, Tcl_DStringAppendElement, Tcl_DStringStartSublist, Tcl_DStringEndSublist, Tcl_DStringLength, Tcl_DStringValue, Tcl_DStringSetLength, Tcl_DStringFree, Tcl_DStringResult, Tcl_DStringGetResult, Tcl_DStringToObj - manipulate dynamic strings

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_DStringInit]{.ccmd}[dsPtr]{.cargs}
[char *]{.ret} [Tcl_DStringAppend]{.ccmd}[dsPtr, bytes, length]{.cargs}
[char *]{.ret} [Tcl_DStringAppendElement]{.ccmd}[dsPtr, element]{.cargs}
[Tcl_DStringStartSublist]{.ccmd}[dsPtr]{.cargs}
[Tcl_DStringEndSublist]{.ccmd}[dsPtr]{.cargs}
[Tcl_Size]{.ret} [Tcl_DStringLength]{.ccmd}[dsPtr]{.cargs}
[char *]{.ret} [Tcl_DStringValue]{.ccmd}[dsPtr]{.cargs}
[Tcl_DStringSetLength]{.ccmd}[dsPtr, newLength]{.cargs}
[Tcl_DStringFree]{.ccmd}[dsPtr]{.cargs}
[Tcl_DStringResult]{.ccmd}[interp, dsPtr]{.cargs}
[Tcl_DStringGetResult]{.ccmd}[interp, dsPtr]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_DStringToObj]{.ccmd}[dsPtr]{.cargs}
:::

# Arguments

.AP Tcl_DString *dsPtr in/out Pointer to structure that is used to manage a dynamic string. .AP "const char" *bytes in Pointer to characters to append to dynamic string. .AP "const char" *element in Pointer to characters to append as list element to dynamic string. .AP Tcl_Size length in Number of bytes from *bytes* to add to dynamic string.  If negative, add all characters up to null terminating character. .AP Tcl_Size newLength in New length for dynamic string, not including null terminating character. .AP Tcl_Interp *interp in/out Interpreter whose result is to be set from or moved to the dynamic string. 

# Description

Dynamic strings provide a mechanism for building up arbitrarily long strings by gradually appending information.  If the dynamic string is short then there will be no memory allocation overhead;  as the string gets larger, additional space will be allocated as needed.

**Tcl_DStringInit** initializes a dynamic string to zero length. The Tcl_DString structure must have been allocated by the caller. No assumptions are made about the current state of the structure; anything already in it is discarded. If the structure has been used previously, **Tcl_DStringFree** should be called first to free up any memory allocated for the old string.

**Tcl_DStringAppend** adds new information to a dynamic string, allocating more memory for the string if needed. If *length* is less than zero then everything in *bytes* is appended to the dynamic string;  otherwise *length* specifies the number of bytes to append. **Tcl_DStringAppend** returns a pointer to the characters of the new string.  The string can also be retrieved from the *string* field of the Tcl_DString structure.

**Tcl_DStringAppendElement** is similar to **Tcl_DStringAppend** except that it does not take a *length* argument (it appends all of *element*) and it converts the string to a proper list element before appending. **Tcl_DStringAppendElement** adds a separator space before the new list element unless the new list element is the first in a list or sub-list (i.e. either the current string is empty, or it contains the single character "{", or the last two characters of the current string are " {"). **Tcl_DStringAppendElement** returns a pointer to the characters of the new string.

**Tcl_DStringStartSublist** and **Tcl_DStringEndSublist** can be used to create nested lists. To append a list element that is itself a sublist, first call **Tcl_DStringStartSublist**, then call **Tcl_DStringAppendElement** for each of the elements in the sublist, then call **Tcl_DStringEndSublist** to end the sublist. **Tcl_DStringStartSublist** appends a space character if needed, followed by an open brace;  **Tcl_DStringEndSublist** appends a close brace. Lists can be nested to any depth.

**Tcl_DStringLength** is a macro that returns the current length of a dynamic string (not including the terminating null character). **Tcl_DStringValue** is a  macro that returns a pointer to the current contents of a dynamic string.

**Tcl_DStringSetLength** changes the length of a dynamic string. If *newLength* is less than the string's current length, then the string is truncated. If *newLength* is greater than the string's current length, then the string will become longer and new space will be allocated for the string if needed. However, **Tcl_DStringSetLength** will not initialize the new space except to provide a terminating null character;  it is up to the caller to fill in the new space. **Tcl_DStringSetLength** does not free up the string's storage space even if the string is truncated to zero length, so **Tcl_DStringFree** will still need to be called.

**Tcl_DStringFree** should be called when you are finished using the string.  It frees up any memory that was allocated for the string and reinitializes the string's value to an empty string.

**Tcl_DStringResult** sets the result of *interp* to the value of the dynamic string given by *dsPtr*.  It does this by moving a pointer from *dsPtr* to the interpreter's result. This saves the cost of allocating new memory and copying the string. **Tcl_DStringResult** also reinitializes the dynamic string to an empty string. Since the dynamic string is reinitialized, there is no need to further call **Tcl_DStringFree** on it and it can be reused without calling **Tcl_DStringInit**. The caller must ensure that the dynamic string stored in *dsPtr* is encoded in Tcl's internal UTF-8 format.

**Tcl_DStringGetResult** does the opposite of **Tcl_DStringResult**. It sets the value of *dsPtr* to the result of *interp* and it clears *interp*'s result. If possible it does this by moving a pointer rather than by copying the string.

**Tcl_DStringToObj** returns a **Tcl_Obj** containing the value of the dynamic string given by *dsPtr*. It does this by moving a pointer from *dsPtr* to a newly allocated **Tcl_Obj** and reinitializing to dynamic string to an empty string. This saves the cost of allocating new memory and copying the string. Since the dynamic string is reinitialized, there is no need to further call **Tcl_DStringFree** on it and it can be reused without calling **Tcl_DStringInit**. The returned **Tcl_Obj** has a reference count of 0. The caller must ensure that the dynamic string stored in *dsPtr* is encoded in Tcl's internal UTF-8 format. 

