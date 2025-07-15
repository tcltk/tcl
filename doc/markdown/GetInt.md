---
CommandName: Tcl_GetInt
ManualSection: 3
Version: unknown
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - boolean
 - conversion
 - double
 - floating-point
 - integer
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_GetInt, Tcl_GetDouble, Tcl_GetBoolean - convert from string to integer, double, or boolean

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_GetInt]{.lit} [interp, src, intPtr]{.arg}
[int]{.ret} [Tcl_GetDouble]{.lit} [interp, src, doublePtr]{.arg}
[int]{.ret} [Tcl_GetBoolean]{.lit} [interp, src, intPtr]{.arg}
[int]{.ret} [Tcl_GetBool]{.lit} [interp, src, flags, charPtr]{.arg}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter to use for error reporting. .AP "const char" *src in Textual value to be converted. .AP int *intPtr out Points to place to store integer value converted from *src*. .AP double *doublePtr out Points to place to store double-precision floating-point value converted from *src*. .AP char *charPtr out Points to place to store boolean value (0 or 1) value converted from *src*. .AP int flags in 0 or TCL_NULL_OK. If TCL_NULL_OK is used, then the empty string or NULL will result in **Tcl_GetBool** return TCL_OK, the *charPtr filled with the value **'\xFF'**; 

# Description

These procedures convert from strings to integers or double-precision floating-point values or booleans (represented as 0- or 1-valued integers).  Each of the procedures takes a *src* argument, converts it to an internal form of a particular type, and stores the converted value at the location indicated by the procedure's third argument.  If all goes well, each of the procedures returns **TCL_OK**.  If *src* does not have the proper syntax for the desired type then **TCL_ERROR** is returned, an error message is left in the interpreter's result, and nothing is stored at **intPtr* or **doublePtr*.

**Tcl_GetInt** expects *src* to consist of a collection of integer digits, optionally signed and optionally preceded and followed by white space.  If the first two characters of *src* after the optional white space and sign are "**0x**" then *src* is expected to be in hexadecimal form;  otherwise, if the first such characters are "**0d**" then *src* is expected to be in decimal form; otherwise, if the first such characters are "**0o**" then *src* is expected to be in octal form;  otherwise, if the first such characters are "**0b**" then *src* is expected to be in binary form;  otherwise, *src* is expected to be in decimal form.

**Tcl_GetDouble** expects *src* to consist of a floating-point number, which is:  white space;  a sign; a sequence of digits;  a decimal point "**.**"; a sequence of digits;  the letter "**e**"; a signed decimal exponent;  and more white space. Any of the fields may be omitted, except that the digits either before or after the decimal point must be present and if the "**e**" is present then it must be followed by the exponent number. If there are no fields apart from the sign and initial sequence of digits (i.e., no decimal point or exponent indicator), that initial sequence of digits should take one of the forms that **Tcl_GetInt** supports, described above. The use of "**,**" as a decimal point is not supported nor should any other sort of inter-digit separator be present.

**Tcl_GetBoolean** expects *src* to specify a boolean value.  If *src* is any of **0**, **false**, **no**, or **off**, then **Tcl_GetBoolean** stores a zero value at **intPtr*. If *src* is any of **1**, **true**, **yes**, or **on**, then 1 is stored at **intPtr*. Any of these values may be abbreviated, and upper-case spellings are also acceptable.

**Tcl_GetBool** functions almost the same as **Tcl_GetBoolean**, but it has an additional parameter **flags**, which can be used to specify whether the empty string or NULL is accepted as valid. 

