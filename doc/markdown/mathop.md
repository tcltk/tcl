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

The commands in the **::tcl::mathop** namespace implement the same set of operations as supported by the **expr** command. All are exported from the namespace, but are not imported into any other namespace by default. Note that renaming, reimplementing or deleting any of the commands in the namespace does *not* alter the way that the **expr** command behaves, and nor does defining any new commands in the **::tcl::mathop** namespace.

The following operator commands are supported: .DS **~**	**!**	**+**	**-**	***** **/**	**%**	******	**&**	**|** **^**	**>>**	**<<**	**==**	**eq** **!=**	**ne**	**<**	**<=**	**>** **>=**	**in**	**ni**	**lt**	**le** **gt**	**ge** .DE

## Mathematical operators

The behaviors of the mathematical operator commands are as follows:

**!** *boolean*
: Returns the boolean negation of *boolean*, where *boolean* may be any numeric value or any other form of boolean value (i.e. it returns truth if the argument is falsity or zero, and falsity if the argument is truth or non-zero).

**+** ?*number* ...?
: Returns the sum of arbitrarily many arguments. Each *number* argument may be any numeric value. If no arguments are given, the result will be zero (the summation identity).

**-** *number* ?*number* ...?
: If only a single *number* argument is given, returns the negation of that numeric value. Otherwise returns the number that results when all subsequent numeric values are subtracted from the first one. All *number* arguments must be numeric values. At least one argument must be given.

***** ?*number* ...?
: Returns the product of arbitrarily many arguments. Each *number* may be any numeric value. If no arguments are given, the result will be one (the multiplicative identity).

**/** *number* ?*number* ...?
: If only a single *number* argument is given, returns the reciprocal of that numeric value (i.e. the value obtained by dividing 1.0 by that value). Otherwise returns the number that results when the first numeric argument is divided by all subsequent numeric arguments. All *number* arguments must be numeric values. At least one argument must be given.
    Note that when the leading values in the list of arguments are integers, integer division will be used for those initial steps (i.e. the intermediate results will be as if the functions *floor* and *int* are applied to them, in that order). If all values in the operation are integers, the result will be an integer.

**%** *number number*
: Returns the integral modulus (i.e., remainder) of the first argument with respect to the second. Each *number* must have an integral value. Also, the sign of the result will be the same as the sign of the second *number*, which must not be zero.
    Note that Tcl defines this operation exactly even for negative numbers, so that the following command returns a true value (omitting the namespace for clarity):

    ```
    == [* [/ x y] y] [\- x [% x y]]
    ```

****** ?*number* ...?
: Returns the result of raising each value to the power of the result of recursively operating on the result of processing the following arguments, so "**** 2 3 4**" is the same as "**** 2 [** 3 4]**". Each *number* may be any numeric value, though the second number must not be fractional if the first is negative.  The maximum exponent value that Tcl can handle if the first number is an integer > 1 is 268435455. If no arguments are given, the result will be one, and if only one argument is given, the result will be that argument. The result will have an integral value only when all arguments are integral values.


## Comparison operators

The behaviors of the comparison operator commands (most of which operate preferentially on numeric arguments) are as follows:

**==** ?*arg* ...?
: Returns whether each argument is equal to the arguments on each side of it in the sense of the **expr** == operator (*i.e.*, numeric comparison if possible, exact string comparison otherwise). If fewer than two arguments are given, this operation always returns a true value.

**eq** ?*arg* ...?
: Returns whether each argument is equal to the arguments on each side of it using exact string comparison. If fewer than two arguments are given, this operation always returns a true value.

**!=** *arg arg*
: Returns whether the two arguments are not equal to each other, in the sense of the **expr** != operator (*i.e.*, numeric comparison if possible, exact string comparison otherwise).

**ne** *arg arg*
: Returns whether the two arguments are not equal to each other using exact string comparison.

**<** ?*arg* ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be strictly more than the one preceding it. Comparisons are performed preferentially on the numeric values, and are otherwise performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value. When the arguments are numeric but should be compared as strings, the **lt** operator or the **string compare** command should be used instead.

**<=** ?*arg* ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be equal to or more than the one preceding it. Comparisons are performed preferentially on the numeric values, and are otherwise performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value. When the arguments are numeric but should be compared as strings,  the **le** operator or the **string compare** command should be used instead.

**>** ?*arg* ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be strictly less than the one preceding it. Comparisons are performed preferentially on the numeric values, and are otherwise performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value. When the arguments are numeric but should be compared as strings, the **gt** operator or the **string compare** command should be used instead.

**>=** ?*arg* ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be equal to or less than the one preceding it. Comparisons are performed preferentially on the numeric values, and are otherwise performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value. When the arguments are numeric but should be compared as strings, the **ge** operator or the **string compare** command should be used instead.

**lt** ?*arg* ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be strictly more than the one preceding it. Comparisons are performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value.

**le** ?*arg* ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be equal to or strictly more than the one preceding it. Comparisons are performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value.

**gt** ?*arg* ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be strictly less than the one preceding it. Comparisons are performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value.

**ge** ?*arg* ...?
: Returns whether the arbitrarily-many arguments are ordered, with each argument after the first having to be equal to or strictly less than the one preceding it. Comparisons are performed using UNICODE string comparison. If fewer than two arguments are present, this operation always returns a true value.


## Bit-wise operators

The behaviors of the bit-wise operator commands (all of which only operate on integral arguments) are as follows:

**~** *number*
: Returns the bit-wise negation of *number*. *Number* may be an integer of any size. Note that the result of this operation will always have the opposite sign to the input *number*.

**&** ?*number* ...?
: Returns the bit-wise AND of each of the arbitrarily many arguments. Each *number* must have an integral value. If no arguments are given, the result will be minus one.

**|** ?*number* ...?
: Returns the bit-wise OR of each of the arbitrarily many arguments. Each *number* must have an integral value. If no arguments are given, the result will be zero.

**^** ?*number* ...?
: Returns the bit-wise XOR of each of the arbitrarily many arguments. Each *number* must have an integral value. If no arguments are given, the result will be zero.

**<<** *number number*
: Returns the result of bit-wise shifting the first argument left by the number of bits specified in the second argument. Each *number* must have an integral value.

**>>** *number number*
: Returns the result of bit-wise shifting the first argument right by the number of bits specified in the second argument. Each *number* must have an integral value.


## List operators

The behaviors of the list-oriented operator commands are as follows:

**in** *arg list*
: Returns whether the value *arg* is present in the list *list* (according to exact string comparison of elements).

**ni** *arg list*
: Returns whether the value *arg* is not present in the list *list* (according to exact string comparison of elements).


# Examples

The simplest way to use the operators is often by using **namespace path** to make the commands available. This has the advantage of not affecting the set of commands defined by the current namespace.

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

