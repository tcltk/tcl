---
CommandName: Tcl_GetIndexFromObj
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - prefix(n)
 - Tcl_WrongNumArgs(3)
Keywords:
 - index
 - option
 - value
 - table lookup
Copyright:
 - Copyright (c) 1997 Sun Microsystems, Inc.
---

# Name

Tcl_GetIndexFromObj, Tcl_GetIndexFromObjStruct - lookup string in table of keywords

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_GetIndexFromObj]{.lit} [interp, objPtr, tablePtr, msg, flags, indexPtr]{.arg}
[int]{.ret} [Tcl_GetIndexFromObjStruct]{.lit} [interp, objPtr, structTablePtr, offset, msg, flags, indexPtr]{.arg}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter to use for error reporting; if NULL, then no message is provided on errors. .AP Tcl_Obj *objPtr in/out The string value of this value is used to search through *tablePtr*. If the **TCL_INDEX_TEMP_TABLE** flag is not specified, the internal representation is modified to hold the index of the matching table entry. .AP "const char *const" *tablePtr in An array of null-terminated strings.  The end of the array is marked by a NULL string pointer. Note that, unless the **TCL_INDEX_TEMP_TABLE** flag is specified, references to the *tablePtr* may be retained in the internal representation of *objPtr*, so this should represent the address of a statically-allocated array. .AP "const void" *structTablePtr in An array of arbitrary type, typically some **struct** type. The first member of the structure must be a null-terminated string. The size of the structure is given by *offset*. Note that, unless the **TCL_INDEX_TEMP_TABLE** flag is specified, references to the *structTablePtr* may be retained in the internal representation of *objPtr*, so this should represent the address of a statically-allocated array of structures. .AP int offset in The offset to add to structTablePtr to get to the next entry. The end of the array is marked by a NULL string pointer. .AP "const char" *msg in Null-terminated string describing what is being looked up, such as **option**.  This string is included in error messages. .AP int flags in OR-ed combination of bits providing additional information for operation.  The only bits that are currently defined are **TCL_EXACT** , **TCL_INDEX_TEMP_TABLE**, and **TCL_NULL_OK**. .AP enum|char|short|int|long *indexPtr out If not (int *)NULL, the index of the string in *tablePtr* that matches the value of *objPtr* is returned here. The variable can be any integer type, signed or unsigned, char, short, long or long long. It can also be an enum.

# Description

These procedures provide an efficient way for looking up keywords, switch names, option names, and similar things where the literal value of a Tcl value must be chosen from a predefined set. **Tcl_GetIndexFromObj** compares *objPtr* against each of the strings in *tablePtr* to find a match.  A match occurs if *objPtr*'s string value is identical to one of the strings in *tablePtr*, or if it is a non-empty unique abbreviation for exactly one of the strings in *tablePtr* and the **TCL_EXACT** flag was not specified; in either case **TCL_OK** is returned. If *indexPtr* is not NULL the index of the matching entry is stored at **indexPtr*.

If there is no matching entry, **TCL_ERROR** is returned and an error message is left in *interp*'s result if *interp* is not NULL.  *Msg* is included in the error message to indicate what was being looked up.  For example, if *msg* is **option** the error message will have a form like "**bad option \N'34'firt\N'34': must be first, second, or third**".

If the **TCL_INDEX_TEMP_TABLE** was not specified, when **Tcl_GetIndexFromObj** completes successfully it modifies the internal representation of *objPtr* to hold the address of the table and the index of the matching entry.  If **Tcl_GetIndexFromObj** is invoked again with the same *objPtr* and *tablePtr* arguments (e.g. during a reinvocation of a Tcl command), it returns the matching index immediately without having to redo the lookup operation.  Note that **Tcl_GetIndexFromObj** assumes that the entries in *tablePtr* are static: they must not change between invocations.  This caching mechanism can be disallowed by specifying the **TCL_INDEX_TEMP_TABLE** flag. If the **TCL_NULL_OK** flag was specified, objPtr is allowed to be NULL or the empty string. The resulting index is -1. Otherwise, if the value of *objPtr* is the empty string, **Tcl_GetIndexFromObj** will treat it as a non-matching value and return **TCL_ERROR**.

**Tcl_GetIndexFromObjStruct** works just like **Tcl_GetIndexFromObj**, except that instead of treating *tablePtr* as an array of string pointers, it treats it as a pointer to the first string in a series of strings that have *offset* bytes between them (i.e. that there is a pointer to the first array of characters at *tablePtr*, a pointer to the second array of characters at *tablePtr*+*offset* bytes, etc.) This is particularly useful when processing things like **Tk_ConfigurationSpec**, whose string keys are in the same place in each of several array elements.

# Reference count management

**Tcl_GetIndexFromObj** and **Tcl_GetIndexFromObjStruct** do not modify the reference count of their *objPtr* arguments; they only read. Note however that these functions may set the interpreter result; if that is the only place that is holding a reference to the object, it will be deleted.

