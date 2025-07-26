---
CommandName: Tcl_ExprLongObj
ManualSection: 3
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_ExprLong
 - Tcl_ExprDouble
 - Tcl_ExprBoolean
 - Tcl_ExprString
 - Tcl_GetObjResult
Keywords:
 - boolean
 - double
 - evaluate
 - expression
 - integer
 - value
 - string
Copyright:
 - Copyright (c) 1996-1997 Sun Microsystems, Inc.
---

# Name

Tcl_ExprLongObj, Tcl_ExprDoubleObj, Tcl_ExprBooleanObj, Tcl_ExprObj - evaluate an expression

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_ExprLongObj]{.ccmd}[interp, objPtr, longPtr]{.cargs}
[int]{.ret} [Tcl_ExprDoubleObj]{.ccmd}[interp, objPtr, doublePtr]{.cargs}
[int]{.ret} [Tcl_ExprBooleanObj]{.ccmd}[interp, objPtr, booleanPtr]{.cargs}
[int]{.ret} [Tcl_ExprObj]{.ccmd}[interp, objPtr, resultPtrPtr]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter in whose context to evaluate *objPtr*. .AP Tcl_Obj *objPtr in Pointer to a value containing the expression to evaluate. .AP long *longPtr out Pointer to location in which to store the integer value of the expression. .AP int *doublePtr out Pointer to location in which to store the floating-point value of the expression. .AP int *booleanPtr out Pointer to location in which to store the 0/1 boolean value of the expression. .AP Tcl_Obj **resultPtrPtr out Pointer to location in which to store a pointer to the value that is the result of the expression. 

# Description

These four procedures all evaluate an expression, returning the result in one of four different forms. The expression is given by the *objPtr* argument, and it can have any of the forms accepted by the **expr** command.

The *interp* argument refers to an interpreter used to evaluate the expression (e.g. for variables and nested Tcl commands) and to return error information.

For all of these procedures the return value is a standard Tcl result: **TCL_OK** means the expression was successfully evaluated, and **TCL_ERROR** means that an error occurred while evaluating the expression. If **TCL_ERROR** is returned, then a message describing the error can be retrieved using **Tcl_GetObjResult**. If an error occurs while executing a Tcl command embedded in the expression then that error will be returned.

If the expression is successfully evaluated, then its value is returned in one of four forms, depending on which procedure is invoked. **Tcl_ExprLongObj** stores an integer value at **longPtr*. If the expression's actual value is a floating-point number, then it is truncated to an integer. If the expression's actual value is a non-numeric string then an error is returned.

**Tcl_ExprDoubleObj** stores a floating-point value at **doublePtr*. If the expression's actual value is an integer, it is converted to floating-point. If the expression's actual value is a non-numeric string then an error is returned.

**Tcl_ExprBooleanObj** stores a 0/1 integer value at **booleanPtr*. If the expression's actual value is an integer or floating-point number, then they store 0 at **booleanPtr* if the value was zero and 1 otherwise. If the expression's actual value is a non-numeric string then it must be one of the values accepted by **Tcl_GetBoolean** such as "yes" or "no", or else an error occurs.

If **Tcl_ExprObj** successfully evaluates the expression, it stores a pointer to the Tcl value containing the expression's value at **resultPtrPtr*. In this case, the caller is responsible for calling **Tcl_DecrRefCount** to decrement the value's reference count when it is finished with the value.

# Reference count management

**Tcl_ExprLongObj**, **Tcl_ExprDoubleObj**, **Tcl_ExprBooleanObj**, and **Tcl_ExprObj** all increment and decrement the reference count of their *objPtr* arguments; you must not pass them any value with a reference count of zero. They also manipulate the interpreter result; you must not count on the interpreter result to hold the reference count of any value over these calls. 

