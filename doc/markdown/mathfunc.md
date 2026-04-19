---
CommandName: mathfunc
ManualSection: n
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Mathematical Functions
Links:
 - expr(n)
 - fpclassify(n)
 - mathop(n)
 - namespace(n)
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-2000 Sun Microsystems, Inc.
 - Copyright (c) 2005 Kevin B. Kenny <kennykb@acm.org>. All rights reserved
---

# Name

mathfunc - Mathematical functions for Tcl expressions

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [Tcl]{.lit} [8.5-]{.lit}

[::tcl::mathfunc::abs]{.cmd} [arg]{.arg}
[::tcl::mathfunc::acos]{.cmd} [arg]{.arg}
[::tcl::mathfunc::asin]{.cmd} [arg]{.arg}
[::tcl::mathfunc::atan]{.cmd} [arg]{.arg}
[::tcl::mathfunc::atan2]{.cmd} [y]{.arg} [x]{.arg}
[::tcl::mathfunc::bool]{.cmd} [arg]{.arg}
[::tcl::mathfunc::ceil]{.cmd} [arg]{.arg}
[::tcl::mathfunc::cos]{.cmd} [arg]{.arg}
[::tcl::mathfunc::cosh]{.cmd} [arg]{.arg}
[::tcl::mathfunc::double]{.cmd} [arg]{.arg}
[::tcl::mathfunc::entier]{.cmd} [arg]{.arg}
[::tcl::mathfunc::exp]{.cmd} [arg]{.arg}
[::tcl::mathfunc::floor]{.cmd} [arg]{.arg}
[::tcl::mathfunc::fmod]{.cmd} [x]{.arg} [y]{.arg}
[::tcl::mathfunc::hypot]{.cmd} [x]{.arg} [y]{.arg}
[::tcl::mathfunc::int]{.cmd} [arg]{.arg}
[::tcl::mathfunc::isfinite]{.cmd} [arg]{.arg}
[::tcl::mathfunc::isinf]{.cmd} [arg]{.arg}
[::tcl::mathfunc::isnan]{.cmd} [arg]{.arg}
[::tcl::mathfunc::isnormal]{.cmd} [arg]{.arg}
[::tcl::mathfunc::isqrt]{.cmd} [arg]{.arg}
[::tcl::mathfunc::issubnormal]{.cmd} [arg]{.arg}
[::tcl::mathfunc::isunordered]{.cmd} [x]{.arg} [y]{.arg}
[::tcl::mathfunc::log]{.cmd} [arg]{.arg}
[::tcl::mathfunc::log10]{.cmd} [arg]{.arg}
[::tcl::mathfunc::max]{.cmd} [arg]{.arg} [arg]{.optdot}
[::tcl::mathfunc::min]{.cmd} [arg]{.arg} [arg]{.optdot}
[::tcl::mathfunc::pow]{.cmd} [x]{.arg} [y]{.arg}
[::tcl::mathfunc::rand]{.cmd}
[::tcl::mathfunc::round]{.cmd} [arg]{.arg}
[::tcl::mathfunc::sin]{.cmd} [arg]{.arg}
[::tcl::mathfunc::sinh]{.cmd} [arg]{.arg}
[::tcl::mathfunc::sqrt]{.cmd} [arg]{.arg}
[::tcl::mathfunc::srand]{.cmd} [arg]{.arg}
[::tcl::mathfunc::tan]{.cmd} [arg]{.arg}
[::tcl::mathfunc::tanh]{.cmd} [arg]{.arg}
[::tcl::mathfunc::wide]{.cmd} [arg]{.arg}
:::

# Description

The [expr] command handles mathematical functions of the form **sin($x)** or **atan2($y,$x)** by converting them to calls of the form `[tcl::mathfunc::sin [expr {$x}]]` or `[tcl::mathfunc::atan2 [expr {$y}] [expr {$x}]]`. A number of math functions are available by default within the namespace **::tcl::mathfunc**; these functions are also available for code apart from [expr], by invoking the given commands directly.

Tcl supports the following mathematical functions in expressions, all of which work solely with floating-point numbers unless otherwise noted:

::: {.info DISPLAY="yes"}
**abs**	**acos**	**asin**	**atan** **atan2**	**bool**	**ceil**	**cos** **cosh**	**double**	**entier**	**exp** **floor**	**fmod**	**hypot**	**int** **isfinite**	**isinf**	**isnan**	**isnormal** **isqrt**	**issubnormal**	**isunordered**	**log** **log10**	**max**	**min**	**pow** **rand**	**round**	**sin**	**sinh** **sqrt**	**srand**	**tan**	**tanh** **wide**
:::

In addition to these predefined functions, applications may define additional functions by using [proc] (or any other method, such as [interp alias][interp] or [Tcl\_CreateObjCommand][CrtObjCmd]) to define new commands in the **tcl::mathfunc** namespace.

## Detailed definitions

[abs]{.cmd} [arg]{.arg}
: Returns the absolute value of *arg*.  *Arg* may be either integer or floating-point, and the result is returned in the same form.

[acos]{.cmd} [arg]{.arg}
: Returns the arc cosine of *arg*, in the range \[*0*,*pi*\] radians. *Arg* should be in the range \[*-1*,*1*\].

[asin]{.cmd} [arg]{.arg}
: Returns the arc sine of *arg*, in the range \[*-pi/2*,*pi/2*\] radians.  *Arg* should be in the range \[*-1*,*1*\].

[atan]{.cmd} [arg]{.arg}
: Returns the arc tangent of *arg*, in the range \[*-pi/2*,*pi/2*\] radians.

[atan2]{.cmd} [y]{.arg} [x]{.arg}
: Returns the arc tangent of *y*/*x*, in the range \[*-pi*,*pi*\] radians.  *x* and *y* cannot both be 0.  If *x* is greater than *0*, this is equivalent to `atan [expr {y/x}]`.

[bool]{.cmd} [arg]{.arg}
: Accepts any numeric value, or any string acceptable to [string is boolean][string], and returns the corresponding boolean value **0** or **1**.  Non-zero numbers are true. Other numbers are false.  Non-numeric strings produce boolean value in agreement with [string is true][string] and [string is false][string].

[ceil]{.cmd} [arg]{.arg}
: Returns the smallest integral floating-point value (i.e. with a zero fractional part) not less than *arg*.  The argument may be any numeric value.

[cos]{.cmd} [arg]{.arg}
: Returns the cosine of *arg*, measured in radians.

[cosh]{.cmd} [arg]{.arg}
: Returns the hyperbolic cosine of *arg*.  If the result would cause an overflow, an error is returned.

[double]{.cmd} [arg]{.arg}
: The argument may be any numeric value, If *arg* is a floating-point value, returns *arg*, otherwise converts *arg* to floating-point and returns the converted value.  May return **Inf** or **-Inf** when the argument is a numeric value that exceeds the floating-point range.

[entier]{.cmd} [arg]{.arg}
: The argument may be any numeric value.  The integer part of *arg* is determined and returned.  The integer range returned by this function is unlimited, unlike **int** and **wide** which truncate their range to fit in particular storage widths.

[exp]{.cmd} [arg]{.arg}
: Returns the exponential of *arg*, defined as *e*\*\**arg*. If the result would cause an overflow, an error is returned.

[floor]{.cmd} [arg]{.arg}
: Returns the largest integral floating-point value (i.e. with a zero fractional part) not greater than *arg*.  The argument may be any numeric value.

[fmod]{.cmd} [x]{.arg} [y]{.arg}
: Returns the floating-point remainder of the division of *x* by *y*.  If *y* is 0, an error is returned.

[hypot]{.cmd} [x]{.arg} [y]{.arg}
: Computes the length of the hypotenuse of a right-angled triangle, approximately `sqrt [expr {x*x+y*y}]` except for being more numerically stable when the two arguments have substantially different magnitudes.

[int]{.cmd} [arg]{.arg}
: The argument may be any numeric value.  The integer part of *arg* is determined, and then the low order bits of that integer value up to the machine word size are returned as an integer value.  For reference, the number of bytes in the machine word are stored in the **wordSize** element of the [tcl\_platform][tclvars] array.

[isfinite]{.cmd} [arg]{.arg}
: Returns 1 if the floating-point number *arg* is finite. That is, if it is zero, subnormal, or normal. Returns 0 if the number is infinite or NaN. Throws an error if *arg* cannot be promoted to a floating-point value.

[isinf]{.cmd} [arg]{.arg}
: Returns 1 if the floating-point number *arg* is infinite. Returns 0 if the number is finite or NaN. Throws an error if *arg* cannot be promoted to a floating-point value.

[isnan]{.cmd} [arg]{.arg}
: Returns 1 if the floating-point number *arg* is Not-a-Number. Returns 0 if the number is finite or infinite. Throws an error if *arg* cannot be promoted to a floating-point value.

[isnormal]{.cmd} [arg]{.arg}
: Returns 1 if the floating-point number *arg* is normal. Returns 0 if the number is zero, subnormal, infinite or NaN. Throws an error if *arg* cannot be promoted to a floating-point value.

[isqrt]{.cmd} [arg]{.arg}
: Computes the integer part of the square root of *arg*.  *Arg* must be a positive value, either an integer or a floating point number. Unlike **sqrt**, which is limited to the precision of a floating point number, *isqrt* will return a result of arbitrary precision.

[issubnormal]{.cmd} [arg]{.arg}
: Returns 1 if the floating-point number *arg* is subnormal, i.e., the result of gradual underflow. Returns 0 if the number is zero, normal, infinite or NaN. Throws an error if *arg* cannot be promoted to a floating-point value.

[isunordered]{.cmd} [x]{.arg} [y]{.arg}
: Returns 1 if *x* and *y* cannot be compared for ordering, that is, if either one is NaN. Returns 0 if both values can be ordered, that is, if they are both chosen from among the set of zero, subnormal, normal and infinite values. Throws an error if either *x* or *y* cannot be promoted to a floating-point value.

[log]{.cmd} [arg]{.arg}
: Returns the natural logarithm of *arg*.  *Arg* must be a positive value.

[log10]{.cmd} [arg]{.arg}
: Returns the base 10 logarithm of *arg*.  *Arg* must be a positive value.

[max]{.cmd} [arg]{.arg} [arg]{.optdot}
: Accepts one or more numeric arguments.  Returns the one argument with the greatest value.

[min]{.cmd} [arg]{.arg} [arg]{.optdot}
: Accepts one or more numeric arguments.  Returns the one argument with the least value.

[pow]{.cmd} [x]{.arg} [y]{.arg}
: Computes the value of *x* raised to the power *y*.  If *x* is negative, *y* must be an integer value.

[rand]{.cmd}
: Returns a pseudo-random floating-point value in the range (*0*,*1*). The generator algorithm is a simple linear congruential generator that is not cryptographically secure.  Each result from **rand** completely determines all future results from subsequent calls to **rand**, so **rand** should not be used to generate a sequence of secrets, such as one-time passwords.  The seed of the generator is initialized from the internal clock of the machine or may be set with the **srand** function.

[round]{.cmd} [arg]{.arg}
: If *arg* is an integer value, returns *arg*, otherwise converts *arg* to integer by rounding and returns the converted value.

[sin]{.cmd} [arg]{.arg}
: Returns the sine of *arg*, measured in radians.

[sinh]{.cmd} [arg]{.arg}
: Returns the hyperbolic sine of *arg*.  If the result would cause an overflow, an error is returned.

[sqrt]{.cmd} [arg]{.arg}
: The argument may be any non-negative numeric value.  Returns a floating-point value that is the square root of *arg*.  May return **Inf** when the argument is a numeric value that exceeds the square of the maximum value of the floating-point range.

[srand]{.cmd} [arg]{.arg}
: The *arg*, which must be an integer, is used to reset the seed for the random number generator of **rand**.  Returns the first random number (see **rand**) from that seed.  Each interpreter has its own seed.

[tan]{.cmd} [arg]{.arg}
: Returns the tangent of *arg*, measured in radians.

[tanh]{.cmd} [arg]{.arg}
: Returns the hyperbolic tangent of *arg*.

[wide]{.cmd} [arg]{.arg}
: The argument may be any numeric value.  The integer part of *arg* is determined, and then the low order 64 bits of that integer value are returned as an integer value.



[CrtObjCmd]: CrtObjCmd.md
[expr]: expr.md
[interp]: interp.md
[proc]: proc.md
[string]: string.md
[tclvars]: tclvars.md

