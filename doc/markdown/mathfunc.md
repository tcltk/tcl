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

The \fBexpr\fR command handles mathematical functions of the form \fBsin($x)\fR or \fBatan2($y,$x)\fR by converting them to calls of the form \fB[tcl::mathfunc::sin [expr {$x}]]\fR or \fB[tcl::mathfunc::atan2 [expr {$y}] [expr {$x}]]\fR. A number of math functions are available by default within the namespace \fB::tcl::mathfunc\fR; these functions are also available for code apart from \fBexpr\fR, by invoking the given commands directly.

Tcl supports the following mathematical functions in expressions, all of which work solely with floating-point numbers unless otherwise noted: .DS \fBabs\fR	\fBacos\fR	\fBasin\fR	\fBatan\fR \fBatan2\fR	\fBbool\fR	\fBceil\fR	\fBcos\fR \fBcosh\fR	\fBdouble\fR	\fBentier\fR	\fBexp\fR \fBfloor\fR	\fBfmod\fR	\fBhypot\fR	\fBint\fR \fBisfinite\fR	\fBisinf\fR	\fBisnan\fR	\fBisnormal\fR \fBisqrt\fR	\fBissubnormal\fR	\fBisunordered\fR	\fBlog\fR \fBlog10\fR	\fBmax\fR	\fBmin\fR	\fBpow\fR \fBrand\fR	\fBround\fR	\fBsin\fR	\fBsinh\fR \fBsqrt\fR	\fBsrand\fR	\fBtan\fR	\fBtanh\fR \fBwide\fR .DE

In addition to these predefined functions, applications may define additional functions by using \fBproc\fR (or any other method, such as \fBinterp alias\fR or \fBTcl_CreateObjCommand\fR) to define new commands in the \fBtcl::mathfunc\fR namespace.

## Detailed definitions

**abs** \fIarg\fR
: Returns the absolute value of \fIarg\fR.  \fIArg\fR may be either integer or floating-point, and the result is returned in the same form.

**acos** \fIarg\fR
: Returns the arc cosine of \fIarg\fR, in the range [\fI0\fR,\fIpi\fR] radians. \fIArg\fR should be in the range [\fI-1\fR,\fI1\fR].

**asin** \fIarg\fR
: Returns the arc sine of \fIarg\fR, in the range [\fI-pi/2\fR,\fIpi/2\fR] radians.  \fIArg\fR should be in the range [\fI-1\fR,\fI1\fR].

**atan** \fIarg\fR
: Returns the arc tangent of \fIarg\fR, in the range [\fI-pi/2\fR,\fIpi/2\fR] radians.

**atan2** \fIy x\fR
: Returns the arc tangent of \fIy\fR/\fIx\fR, in the range [\fI-pi\fR,\fIpi\fR] radians.  \fIx\fR and \fIy\fR cannot both be 0.  If \fIx\fR is greater than \fI0\fR, this is equivalent to "**atan** [\fBexpr\fR {\fIy\fR\fB/\fR\fIx\fR}]".

**bool** \fIarg\fR
: Accepts any numeric value, or any string acceptable to \fBstring is boolean\fR, and returns the corresponding boolean value \fB0\fR or \fB1\fR.  Non-zero numbers are true. Other numbers are false.  Non-numeric strings produce boolean value in agreement with \fBstring is true\fR and \fBstring is false\fR.

**ceil** \fIarg\fR
: Returns the smallest integral floating-point value (i.e. with a zero fractional part) not less than \fIarg\fR.  The argument may be any numeric value.

**cos** \fIarg\fR
: Returns the cosine of \fIarg\fR, measured in radians.

**cosh** \fIarg\fR
: Returns the hyperbolic cosine of \fIarg\fR.  If the result would cause an overflow, an error is returned.

**double** \fIarg\fR
: The argument may be any numeric value, If \fIarg\fR is a floating-point value, returns \fIarg\fR, otherwise converts \fIarg\fR to floating-point and returns the converted value.  May return \fBInf\fR or \fB-Inf\fR when the argument is a numeric value that exceeds the floating-point range.

**entier** \fIarg\fR
: The argument may be any numeric value.  The integer part of \fIarg\fR is determined and returned.  The integer range returned by this function is unlimited, unlike \fBint\fR and \fBwide\fR which truncate their range to fit in particular storage widths.

**exp** \fIarg\fR
: Returns the exponential of \fIarg\fR, defined as \fIe\fR**\fIarg\fR. If the result would cause an overflow, an error is returned.

**floor** \fIarg\fR
: Returns the largest integral floating-point value (i.e. with a zero fractional part) not greater than \fIarg\fR.  The argument may be any numeric value.

**fmod** \fIx y\fR
: Returns the floating-point remainder of the division of \fIx\fR by \fIy\fR.  If \fIy\fR is 0, an error is returned.

**hypot** \fIx y\fR
: Computes the length of the hypotenuse of a right-angled triangle, approximately "**sqrt** [\fBexpr\fR {\fIx\fR\fB*\fR\fIx\fR\fB+\fR\fIy\fR\fB*\fR\fIy\fR}]" except for being more numerically stable when the two arguments have substantially different magnitudes.

**int** \fIarg\fR
: The argument may be any numeric value.  The integer part of \fIarg\fR is determined, and then the low order bits of that integer value up to the machine word size are returned as an integer value.  For reference, the number of bytes in the machine word are stored in the \fBwordSize\fR element of the \fBtcl_platform\fR array.

**isfinite** \fIarg\fR
: Returns 1 if the floating-point number \fIarg\fR is finite. That is, if it is zero, subnormal, or normal. Returns 0 if the number is infinite or NaN. Throws an error if \fIarg\fR cannot be promoted to a floating-point value.

**isinf** \fIarg\fR
: Returns 1 if the floating-point number \fIarg\fR is infinite. Returns 0 if the number is finite or NaN. Throws an error if \fIarg\fR cannot be promoted to a floating-point value.

**isnan** \fIarg\fR
: Returns 1 if the floating-point number \fIarg\fR is Not-a-Number. Returns 0 if the number is finite or infinite. Throws an error if \fIarg\fR cannot be promoted to a floating-point value.

**isnormal** \fIarg\fR
: Returns 1 if the floating-point number \fIarg\fR is normal. Returns 0 if the number is zero, subnormal, infinite or NaN. Throws an error if \fIarg\fR cannot be promoted to a floating-point value.

**isqrt** \fIarg\fR
: Computes the integer part of the square root of \fIarg\fR.  \fIArg\fR must be a positive value, either an integer or a floating point number. Unlike \fBsqrt\fR, which is limited to the precision of a floating point number, \fIisqrt\fR will return a result of arbitrary precision.

**issubnormal** \fIarg\fR
: Returns 1 if the floating-point number \fIarg\fR is subnormal, i.e., the result of gradual underflow. Returns 0 if the number is zero, normal, infinite or NaN. Throws an error if \fIarg\fR cannot be promoted to a floating-point value.

**isunordered** \fIx y\fR
: Returns 1 if \fIx\fR and \fIy\fR cannot be compared for ordering, that is, if either one is NaN. Returns 0 if both values can be ordered, that is, if they are both chosen from among the set of zero, subnormal, normal and infinite values. Throws an error if either \fIx\fR or \fIy\fR cannot be promoted to a floating-point value.

**log** \fIarg\fR
: Returns the natural logarithm of \fIarg\fR.  \fIArg\fR must be a positive value.

**log10** \fIarg\fR
: Returns the base 10 logarithm of \fIarg\fR.  \fIArg\fR must be a positive value.

**max** \fIarg\fR \fI...\fR
: Accepts one or more numeric arguments.  Returns the one argument with the greatest value.

**min** \fIarg\fR \fI...\fR
: Accepts one or more numeric arguments.  Returns the one argument with the least value.

**pow** \fIx y\fR
: Computes the value of \fIx\fR raised to the power \fIy\fR.  If \fIx\fR is negative, \fIy\fR must be an integer value.

**rand**
: Returns a pseudo-random floating-point value in the range (\fI0\fR,\fI1\fR). The generator algorithm is a simple linear congruential generator that is not cryptographically secure.  Each result from \fBrand\fR completely determines all future results from subsequent calls to \fBrand\fR, so \fBrand\fR should not be used to generate a sequence of secrets, such as one-time passwords.  The seed of the generator is initialized from the internal clock of the machine or may be set with the \fBsrand\fR function.

**round** \fIarg\fR
: If \fIarg\fR is an integer value, returns \fIarg\fR, otherwise converts \fIarg\fR to integer by rounding and returns the converted value.

**sin** \fIarg\fR
: Returns the sine of \fIarg\fR, measured in radians.

**sinh** \fIarg\fR
: Returns the hyperbolic sine of \fIarg\fR.  If the result would cause an overflow, an error is returned.

**sqrt** \fIarg\fR
: The argument may be any non-negative numeric value.  Returns a floating-point value that is the square root of \fIarg\fR.  May return \fBInf\fR when the argument is a numeric value that exceeds the square of the maximum value of the floating-point range.

**srand** \fIarg\fR
: The \fIarg\fR, which must be an integer, is used to reset the seed for the random number generator of \fBrand\fR.  Returns the first random number (see \fBrand\fR) from that seed.  Each interpreter has its own seed.

**tan** \fIarg\fR
: Returns the tangent of \fIarg\fR, measured in radians.

**tanh** \fIarg\fR
: Returns the hyperbolic tangent of \fIarg\fR.

**wide** \fIarg\fR
: The argument may be any numeric value.  The integer part of \fIarg\fR is determined, and then the low order 64 bits of that integer value are returned as an integer value.


# Copyright

Copyright \(co 1993 The Regents of the University of California. Copyright \(co 1994-2000 Sun Microsystems Incorporated. Copyright \(co 2005-2006 Kevin B. Kenny <kennykb@acm.org>. 

