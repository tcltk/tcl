---
CommandName: Tcl_ExprLong
ManualSection: 3
Version: 7.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_ExprLongObj
 - Tcl_ExprDoubleObj
 - Tcl_ExprBooleanObj
 - Tcl_ExprObj
Keywords:
 - boolean
 - double
 - evaluate
 - expression
 - integer
 - value
 - string
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

Tcl_ExprLong, Tcl_ExprDouble, Tcl_ExprBoolean, Tcl_ExprString - evaluate an expression

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_ExprLong]{.ccmd}[interp, expr, longPtr]{.cargs}
[int]{.ret} [Tcl_ExprDouble]{.ccmd}[interp, expr, doublePtr]{.cargs}
[int]{.ret} [Tcl_ExprBoolean]{.ccmd}[interp, expr, booleanPtr]{.cargs}
[int]{.ret} [Tcl_ExprString]{.ccmd}[interp, expr]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter in whose context to evaluate *expr*. .AP "const char" *expr in Expression to be evaluated. .AP long *longPtr out Pointer to location in which to store the integer value of the expression. .AP int *doublePtr out Pointer to location in which to store the floating-point value of the expression. .AP int *booleanPtr out Pointer to location in which to store the 0/1 boolean value of the expression. 

# Description

These four procedures all evaluate the expression given by the *expr* argument and return the result in one of four different forms. The expression can have any of the forms accepted by the **expr** command. Note that these procedures have been largely replaced by the value-based procedures **Tcl_ExprLongObj**, **Tcl_ExprDoubleObj**, **Tcl_ExprBooleanObj**, and **Tcl_ExprObj**. Those value-based procedures evaluate an expression held in a Tcl value instead of a string. The value argument can retain an internal representation that is more efficient to execute.

The *interp* argument refers to an interpreter used to evaluate the expression (e.g. for variables and nested Tcl commands) and to return error information.

For all of these procedures the return value is a standard Tcl result: **TCL_OK** means the expression was successfully evaluated, and **TCL_ERROR** means that an error occurred while evaluating the expression. If **TCL_ERROR** is returned then the interpreter's result will hold a message describing the error. If an error occurs while executing a Tcl command embedded in the expression then that error will be returned.

If the expression is successfully evaluated, then its value is returned in one of four forms, depending on which procedure is invoked. **Tcl_ExprLong** stores an integer value at **longPtr*. If the expression's actual value is a floating-point number, then it is truncated to an integer. If the expression's actual value is a non-numeric string then an error is returned.

**Tcl_ExprDouble** stores a floating-point value at **doublePtr*. If the expression's actual value is an integer, it is converted to floating-point. If the expression's actual value is a non-numeric string then an error is returned.

**Tcl_ExprBoolean** stores a 0/1 integer value at **booleanPtr*. If the expression's actual value is an integer or floating-point number, then they store 0 at **booleanPtr* if the value was zero and 1 otherwise. If the expression's actual value is a non-numeric string then it must be one of the values accepted by **Tcl_GetBoolean** such as "yes" or "no", or else an error occurs.

**Tcl_ExprString** returns the value of the expression as a string stored in the interpreter's result. 

