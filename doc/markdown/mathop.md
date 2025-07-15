---
CommandName: mathop
ManualSection: n
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Mathematical Operator Commands
Links:
 - expr(n)
 - mathfunc(n)
 - namespace(n)
Keywords:
 - command
 - expression
 - operator
Copyright:
 - Copyright (c) 2006-2007 Donal K. Fellows.
---

# Name

mathop - Mathematical operators as Tcl commands

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [Tcl]{.lit} [8.5-]{.lit}

[::tcl::mathop::!]{.cmd} [number]{.arg}
[::tcl::mathop::~]{.cmd} [number]{.arg}
[::tcl::mathop::+]{.cmd} [number]{.optdot}
[::tcl::mathop::-]{.cmd} [number]{.arg} [number]{.optdot}
[::tcl::mathop::*]{.cmd} [number]{.optdot}
[::tcl::mathop::/]{.cmd} [number]{.arg} [number]{.optdot}
[::tcl::mathop::%]{.cmd} [number]{.arg} [number]{.arg}
[::tcl::mathop::**]{.cmd} [number]{.optdot}
[::tcl::mathop::&]{.cmd} [number]{.optdot}
[::tcl::mathop::|]{.cmd} [number]{.optdot}
[::tcl::mathop::^]{.cmd} [number]{.optdot}
[::tcl::mathop::<<]{.cmd} [number]{.arg} [number]{.arg}
[::tcl::mathop::>>]{.cmd} [number]{.arg} [number]{.arg}
[::tcl::mathop::==]{.cmd} [arg]{.optdot}
[::tcl::mathop::!=]{.cmd} [arg]{.arg} [arg]{.arg}
[::tcl::mathop::<]{.cmd} [arg]{.optdot}
[::tcl::mathop::<=]{.cmd} [arg]{.optdot}
[::tcl::mathop::>=]{.cmd} [arg]{.optdot}
[::tcl::mathop::>]{.cmd} [arg]{.optdot}
[::tcl::mathop::eq]{.cmd} [arg]{.optdot}
[::tcl::mathop::ne]{.cmd} [arg]{.arg} [arg]{.arg}
[::tcl::mathop::lt]{.cmd} [arg]{.optdot}
[::tcl::mathop::le]{.cmd} [arg]{.optdot}
[::tcl::mathop::gt]{.cmd} [arg]{.optdot}
[::tcl::mathop::ge]{.cmd} [arg]{.optdot}
[::tcl::mathop::in]{.cmd} [arg]{.arg} [list]{.arg}
[::tcl::mathop::ni]{.cmd} [arg]{.arg} [list]{.arg}
:::

# Description

The commands in the \fB::tcl::mathop\fR namespace implement the same set of operations as supported by the \fBexpr\fR command. All are exported from the namespace, but are not imported into any other namespace by default. Note that renaming, reimplementing or deleting any of the commands in the namespace does \fInot\fR alter the way that the \fBexpr\fR command behaves, and nor does defining any new commands in the \fB::tcl::mathop\fR namespace.

The following operator commands are supported: .DS \fB~\fR	\fB!\fR	\fB+\fR	\fB-\fR	\fB*\fR \fB/\fR	\fB%\fR	\fB**\fR	\fB&\fR	\fB|\fR \fB^\fR	\fB>>\fR	\fB<<\fR	\fB==\fR	\fBeq\fR \fB!=\fR	\fBne\fR	\fB<\fR	\fB<=\fR	\fB>\fR \fB>=\fR	\fBin\fR	\fBni\fR	\fBlt\fR	\fBle\fR \fBgt\fR	\fBge\fR .DE

## Mathematical operators

The behaviors of the mathematical operator commands are as follows:

**!** \fIboolean\fR
: Returns the boolean negation of \fIboolean\fR, where \fIboolean\fR may be any numeric value or any other form of boolean value (i.e. it returns truth if the argument is falsity or zero, and falsity if the argument is truth or non-zero).

**+** ?\fInumber\fR ...?
: Returns the sum of arbitrarily many arguments. Each \fInumber\fR argument may be any numeric value. If no arguments are given, the result will be zero (the summation identity).

**-** \fInumber\fR ?\fInumber\fR ...?
: If only a single \fInumber\fR argument is given, returns the negation of that numeric value. Otherwise returns the number that results when all subsequent numeric values are subtracted from the first one. All \fInumber\fR arguments must be numeric values. At least one argument must be given.

***** ?\fInumber\fR ...?
: Returns the product of arbitrarily many arguments. Each \fInumber\fR may be any numeric value. If no arguments are given, the result will be one (the multiplicative identity).

**/** \fInumber\fR ?\fInumber\fR ...?
: If only a single \fInumber\fR argument is given, returns the reciprocal of that numeric value (i.e. the value obtained by dividing 1.0 by that value). Otherwise returns the number that results when the first numeric argument is divided by all subsequent numeric arguments. All \fInumber\fR arguments must be numeric values. At least one argument must be given.
    Note that when the leading values in the list of arguments are integers, integer division will be used for those initial steps (i.e. the intermediate results will be as if the functions \fIfloor\fR and \fIint\fR are applied to them, in that order). If all values in the operation are integers, the result will be an integer.

**%** \fInumber number\fR
: Returns the integral modulus (i.e., remainder) of the first argument with respect to the second. Each \fInumber\fR must have an integral value. Also, the sign of the result will be the same as the sign of the second \fInumber\fR, which must not be zero.
    Note that Tcl defines this operation exactly even for negative numbers, so that the following command returns a true value (omitting the namespace for clarity):

    ```
    == [* [/ x y] y] [\- x [% x y]]
    ```

****** ?\fInumber\fR ...?
: Returns the result of raising each value to the power of the result of recursively operating on the result of processing the following arguments, so "**** 2 3 4**" is the same as "**** 2 [** 3 4]**". Each \fInumber\fR may be any numeric value, though the second number must not be fractional if the first is negative.  The maximum exponent value that Tcl can handle if the first number is an integer > 1 is 268435455. If no arguments are given, the result will be one, and if only one argument is given, the result will be that argument. The result will have an integral value only when all arguments are integral values.


## Comparison operators

The behaviors of the comparison operator commands (most of which operate preferentially on numeric arguments) are as follows:

**==** ?\fIarg\fR ...?
: Returns whether each argument is equal to the arguments on each side of it in the sense of the \fBexpr\fR == operator (\fIi.e.\fR, numeric comparison if possible, exact string comparison otherwise). If fewer than two arguments are given, this operation always returns a true value.

**eq** ?\fIarg\fR ...?
: Returns whether each argument is equal to the arguments on each side of it using exact string comparison. If fewer than two arguments are given, this operation always returns a true value.

**!=** \fIarg arg\fR
: Returns whether the two arguments are not equal to each other, in the sense of the \fBexpr\fR != operator (\fIi.e.\fR, numeric comparison if possible, exact string comparison otherwise).

**ne** \fIarg arg\fR
: Returns whether the two arguments are not equal to each other using exact string comparison.

**<** ?\fIarg\fR ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be strictly more than the one preceding it. Comparisons are performed preferentially on the numeric values, and are otherwise performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value. When the arguments are numeric but should be compared as strings, the \fBlt\fR operator or the \fBstring compare\fR command should be used instead.

**<=** ?\fIarg\fR ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be equal to or more than the one preceding it. Comparisons are performed preferentially on the numeric values, and are otherwise performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value. When the arguments are numeric but should be compared as strings,  the \fBle\fR operator or the \fBstring compare\fR command should be used instead.

**>** ?\fIarg\fR ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be strictly less than the one preceding it. Comparisons are performed preferentially on the numeric values, and are otherwise performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value. When the arguments are numeric but should be compared as strings, the \fBgt\fR operator or the \fBstring compare\fR command should be used instead.

**>=** ?\fIarg\fR ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be equal to or less than the one preceding it. Comparisons are performed preferentially on the numeric values, and are otherwise performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value. When the arguments are numeric but should be compared as strings, the \fBge\fR operator or the \fBstring compare\fR command should be used instead.

**lt** ?\fIarg\fR ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be strictly more than the one preceding it. Comparisons are performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value.

**le** ?\fIarg\fR ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be equal to or strictly more than the one preceding it. Comparisons are performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value.

**gt** ?\fIarg\fR ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be strictly less than the one preceding it. Comparisons are performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value.

**ge** ?\fIarg\fR ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be equal to or strictly less than the one preceding it. Comparisons are performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value.


## Bit-wise operators

The behaviors of the bit-wise operator commands (all of which only operate on integral arguments) are as follows:

**~** \fInumber\fR
: Returns the bit-wise negation of \fInumber\fR. \fINumber\fR may be an integer of any size. Note that the result of this operation will always have the opposite sign to the input \fInumber\fR.

**&** ?\fInumber\fR ...?
: Returns the bit-wise AND of each of the arbitrarily many arguments. Each \fInumber\fR must have an integral value. If no arguments are given, the result will be minus one.

**|** ?\fInumber\fR ...?
: Returns the bit-wise OR of each of the arbitrarily many arguments. Each \fInumber\fR must have an integral value. If no arguments are given, the result will be zero.

**^** ?\fInumber\fR ...?
: Returns the bit-wise XOR of each of the arbitrarily many arguments. Each \fInumber\fR must have an integral value. If no arguments are given, the result will be zero.

**<<** \fInumber number\fR
: Returns the result of bit-wise shifting the first argument left by the number of bits specified in the second argument. Each \fInumber\fR must have an integral value.

**>>** \fInumber number\fR
: Returns the result of bit-wise shifting the first argument right by the number of bits specified in the second argument. Each \fInumber\fR must have an integral value.


## List operators

The behaviors of the list-oriented operator commands are as follows:

**in** \fIarg list\fR
: Returns whether the value \fIarg\fR is present in the list \fIlist\fR (according to exact string comparison of elements).

**ni** \fIarg list\fR
: Returns whether the value \fIarg\fR is not present in the list \fIlist\fR (according to exact string comparison of elements).


# Examples

The simplest way to use the operators is often by using \fBnamespace path\fR to make the commands available. This has the advantage of not affecting the set of commands defined by the current namespace.

```
namespace path {::tcl::mathop ::tcl::mathfunc}

# Compute the sum of some numbers
set sum [+ 1 2 3]

# Compute the average of a list
set list {1 2 3 4 5 6}
set mean [/ [+ {*}$list] [double [llength $list]]]

# Test for list membership
set gotIt [in 3 $list]

# Test to see if a value is within some defined range
set inRange [<= 1 $x 5]

# Test to see if a list is numerically sorted
set sorted [<= {*}$list]

# Test to see if a list is lexically sorted
set alphaList {a b c d e f}
set sorted [le {*}$alphaList]
```

