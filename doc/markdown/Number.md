---
CommandName: Tcl_GetNumber
ManualSection: 3
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_GetDouble
 - Tcl_GetDoubleFromObj
 - Tcl_GetWideIntFromObj
Keywords:
 - double
 - double value
 - double type
 - integer
 - integer value
 - integer type
 - internal representation
 - value
 - value type
 - string representation
Copyright:
---

# Name

Tcl_GetNumber, Tcl_GetNumberFromObj - get numeric value from Tcl value

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
**#include <tclTomMath.h>**
[int]{.ret} [Tcl_GetNumber]{.ccmd}[interp, bytes, numBytes, clientDataPtr, typePtr]{.cargs}
[int]{.ret} [Tcl_GetNumberFromObj]{.ccmd}[interp, objPtr, clientDataPtr, typePtr]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp out When non-NULL, error information is recorded here when the value is not in any of the numeric formats recognized by Tcl. .AP "const char" *bytes in Points to first byte of the string value to be examined. .AP Tcl_Size numBytes in The number of bytes, starting at *bytes*, that should be examined. If **numBytes** is negative, then all bytes should be examined until the first **NUL** byte terminates examination. .AP "void *" *clientDataPtr out Points to space where a pointer value may be written through which a numeric value is available to read. .AP int *typePtr out Points to space where a value may be written reporting what type of numeric storage is available to read. .AP Tcl_Obj *objPtr in A Tcl value to be examined.

# Description

These procedures enable callers to retrieve a numeric value from a Tcl value in a numeric format recognized by Tcl.

Tcl recognizes many values as numbers.  Several examples include: **"0"**, **" +1"**, **"-2 "**, **" 3 "**, **"0xdad1"**, **"0d09"**, **"1_000_000"**, **"4.0"**, **"1e-7"**, **"NaN"**, or **"Inf"**. When built-in Tcl commands act on these values as numbers, they are converted to a numeric representation for efficient handling in C code.  Tcl makes use of three C types to store these representations: **double**, **Tcl_WideInt**, and **mp_int**.  The **double** type is provided by the C language standard.  The **Tcl_WideInt** type is declared in the Tcl header file, **tcl.h**, and is equivalent to the C standard type **long long** on most platforms. The **mp_int** type is declared in the header file **tclTomMath.h**, and implemented by the LibTomMath multiple-precision integer library, included with Tcl.

The routines **Tcl_GetNumber** and **Tcl_GetNumberFromObj** perform the same function.  They differ only in how the arguments present the Tcl value to be examined.  **Tcl_GetNumber** accepts a counted string value in the arguments *bytes* and *numBytes* (or a **NUL**-terminated string value when *numBytes* is negative).  **Tcl_GetNumberFromObj** accepts the Tcl value in *objPtr*.

Both routines examine the Tcl value and determine whether Tcl recognizes it as a number.  If not, both routines return **TCL_ERROR** and (when *interp* is not NULL) record an error message and error code in *interp*.

If Tcl does recognize the examined value as a number, both routines return **TCL_OK**, and use the pointer arguments *clientDataPtr* and *typePtr* (which may not be NULL) to report information the caller can use to retrieve the numeric representation.  Both routines write to **clientDataPtr* a pointer to the internal storage location where Tcl holds the converted numeric value.

When the converted numeric value is stored as a **double**, a call to math library routine **isnan** determines whether that value is not a number (NaN).  If so, both **Tcl_GetNumber** and **Tcl_GetNumberFromObj** write the value **TCL_NUMBER_NAN** to **typePtr*. If not, both routines write the value **TCL_NUMBER_DOUBLE** to **typePtr*.  These routines report different type values in these cases because **Tcl_GetDoubleFromObj** raises an error on NaN values.  For both reported type values, the storage pointer may be cast to type **const double *** and the **double** numeric value may be read through it.

When the converted numeric value is stored as a **Tcl_WideInt**, both **Tcl_GetNumber** and **Tcl_GetNumberFromObj** write the value **TCL_NUMBER_INT** to **typePtr*. The storage pointer may be cast to type **const Tcl_WideInt *** and the **Tcl_WideInt** numeric value may be read through it.

When the converted numeric value is stored as an **mp_int**, both **Tcl_GetNumber** and **Tcl_GetNumberFromObj** write the value **TCL_NUMBER_BIG** to **typePtr*. The storage pointer may be cast to type **const mp_int *** and the **mp_int** numeric value may be read through it.

Future releases of Tcl might expand or revise the recognition of values as numbers.  If additional storage representations are adopted, these routines will add new values to be written to **typePtr* to identify them.  Callers should consider how they should react to unknown values written to **typePtr*.

When callers of these routines read numeric values through the reported storage pointer, they are accessing memory that belongs to the Tcl library.  The Tcl library has the power to overwrite or free this memory.  The storage pointer reported by a call to **Tcl_GetNumber** or **Tcl_GetNumberFromObj** should not be used after the same thread has possibly returned control to the Tcl library.  If longer term access to the numeric value is needed, it should be copied into memory controlled by the caller.  Callers must not attempt to write through or free the storage pointer.

