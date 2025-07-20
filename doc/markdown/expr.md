---
CommandName: expr
ManualSection: n
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - array(n)
 - for(n)
 - if(n)
 - mathfunc(n)
 - mathop(n)
 - namespace(n)
 - proc(n)
 - string(n)
 - Tcl(n)
 - while(n)
Keywords:
 - arithmetic
 - boolean
 - compare
 - expression
 - fuzzy comparison
 - integer value
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-2000 Sun Microsystems, Inc.
 - Copyright (c) 2005 Kevin B. Kenny <kennykb@acm.org>. All rights reserved
---

# Name

expr - Evaluate an expression

# Synopsis

::: {.synopsis} :::
[expr]{.cmd} [arg]{.arg} [arg arg]{.optdot}
:::

# Description

Concatenates *arg*s, separated by a space, into an expression, and evaluates that expression, returning its value. The operators permitted in an expression include a subset of the operators permitted in C expressions.  For those operators common to both Tcl and C, Tcl applies the same meaning and precedence as the corresponding C operators. The value of an expression is often a numeric result, either an integer or a floating-point value, but may also be a non-numeric value. For example, the expression

```
expr 8.2 + 6
```

evaluates to 14.2. Expressions differ from C expressions in the way that operands are specified.  Expressions also support non-numeric operands, string comparisons, and some additional operators not found in C.

When the result of expression is an integer, it is in decimal form, and when the result is a floating-point number, it is in the form produced by the **%g** format specifier of **format**.

::: {.info version="TIP582"}
At any point in the expression except within double quotes or braces, **#** is the beginning of a comment, which lasts to the end of the line or the end of the expression, whichever comes first.
:::

## Operands

An expression consists of a combination of operands, operators, parentheses and commas, possibly with whitespace between any of these elements, which is ignored.  Each operand is interpreted as a numeric value if at all possible.

Each operand has one of the following forms:

A **numeric value**
: ...see next...


Either integer or floating-point.  The first two characters of an integer may also be **0d** for decimal, **0b** for binary, **0o** for octal or **0x** for hexadecimal.

A floating-point number may be take any of several common decimal formats, and may use the decimal point **.**, **e** or **E** for scientific notation, and the sign characters **+** and **-**.  The following are all valid floating-point numbers:  2.1, 3., 6e4, 7.91e+16. The strings **Inf** and **NaN**, in any combination of case, are also recognized as floating point values.  An operand that doesn't have a numeric interpretation must be quoted with either braces or with double quotes.

Digits in any numeric value may be separated with one or more underscore characters, "**_**".  A separator may only appear between digits, not appear at the start of a numeric value, between the leading 0 and radix specifier, or at the end of a numeric value.  Here are some examples:

```
expr 100_000_000		100000000
expr 0xffff_ffff		4294967295
format 0x%x 0b1111_1110_1101_1011		0xfedb
expr 3_141_592_653_589e-1_2		3.141592653589
```

A **boolean value**
: Using any form understood by **string is** **boolean**.

A **variable**
: Using standard **$** notation. The value of the variable is the value of the operand.

A string enclosed in **double-quotes**
: Backslash, variable, and command substitution are performed according to the rules for **Tcl**.

A string enclosed in **braces**.
: The operand is treated as a braced value according to the rule for braces in **Tcl**.

A Tcl command enclosed in **brackets**
: Command substitution is performed as according to the command substitution rule for **Tcl**.

A function call.
: This is mathematical function such as **sin($x)**, whose arguments have any of the above forms for operands.  See **MATH FUNCTIONS** below for a discussion of how mathematical functions are handled.


Because **expr** parses and performs substitutions on values that have already been parsed and substituted by **Tcl**, it is usually best to enclose expressions in braces to avoid the first round of substitutions by **Tcl**.

Below are some examples of simple expressions where the value of **a** is 3 and the value of **b** is 6.  The command on the left side of each line produces the value on the right side.

```
expr {3.1 + $a}	6.1
expr {2 + "$a.$b"}	5.6
expr {4*[llength {6 2}]}	8
expr {{word one} < "word $a"}	0
```

## Operators

For operators having both a numeric mode and a string mode, the numeric mode is chosen when all operands have a numeric interpretation.  The integer interpretation of an operand is preferred over the floating-point interpretation.  To ensure string operations on arbitrary values it is generally a good idea to use **eq**, **ne**, or the **string** command instead of more versatile operators such as **==**.

Unless otherwise specified, operators accept non-numeric operands.  The value of a boolean operation is 1 if true, 0 otherwise.  See also **string is** **boolean**.  The valid operators, most of which are also available as commands in the **tcl::mathop** namespace (see **mathop**(n)), are listed below, grouped in decreasing order of precedence:

**-\0\0+\0\0~\0\0!**
: Unary minus, unary plus, bit-wise NOT, logical NOT.  These operators may only be applied to numeric operands, and bit-wise NOT may only be applied to integers.

******
: Exponentiation.  Valid for numeric operands.  The maximum exponent value that Tcl can handle if the first number is an integer > 1 is 268435455.

***\0\0/\0\0%**
: Multiply and divide, which are valid for numeric operands, and remainder, which is valid for integers.  The remainder, an absolute value smaller than the absolute value of the divisor, has the same sign as the divisor.
    When applied to integers, division and remainder can be considered to partition the number line into a sequence of adjacent non-overlapping pieces, where each piece is the size of the divisor; the quotient identifies which piece the dividend lies within, and the remainder identifies where within that piece the dividend lies. A consequence of this is that the result of "-57 **/** 10" is always -6, and the result of "-57 **%** 10" is always 3.

**+\0\0-**
: Add and subtract.  Valid for numeric operands.

**<<\0\0>>**
: Left and right shift.  Valid for integers. A right shift always propagates the sign bit.

**<\0\0>\0\0<=\0\0>=**
: Boolean numeric-preferring comparisons: less than, greater than, less than or equal, and greater than or equal. If either argument is not numeric, the comparison is done using UNICODE string comparison, as with the string comparison operators below, which have the same precedence.

**lt\0\0gt\0\0le\0\0ge**
: Boolean string comparisons: less than, greater than, less than or equal, and greater than or equal. These always compare values using their UNICODE strings (also see **string compare**), unlike with the numeric-preferring comparisons above, which have the same precedence.

**==\0\0!=**
: Boolean equal and not equal.

**eq\0\0ne**
: Boolean string equal and string not equal.

**in\0\0ni**
: List containment and negated list containment.  The first argument is interpreted as a string, the second as a list.  **in** tests for membership in the list, and **ni** is the inverse.

**&**
: Bit-wise AND.  Valid for integer operands.

**^**
: Bit-wise exclusive OR.  Valid for integer operands.

**|**
: Bit-wise OR.  Valid for integer operands.

**&&**
: Logical AND.  If both operands are true, the result is 1, or 0 otherwise. This operator evaluates lazily; it only evaluates its second operand if it must in order to determine its result. This operator evaluates lazily; it only evaluates its second operand if it must in order to determine its result.

**||**
: Logical OR.  If both operands are false, the result is 0, or 1 otherwise. This operator evaluates lazily; it only evaluates its second operand if it must in order to determine its result.

*x* **?** *y* **:** *z*
: If-then-else, as in C.  If *x* is false , the result is the value of *y*.  Otherwise the result is the value of *z*. This operator evaluates lazily; it evaluates only one of *y* or *z*.


The exponentiation operator promotes types in the same way that the multiply and divide operators do, and the result is is the same as the result of **pow**. Exponentiation groups right-to-left within a precedence level. Other binary operators group left-to-right.  For example, the value of

```
expr {4*2 < 7}
```

is 0, while the value of

```
expr {2**3**2}
```

is 512.

As in C, **&&**, **||**, and **?:** feature "lazy evaluation", which means that operands are not evaluated if they are not needed to determine the outcome.  For example, in

```
expr {$v?[a]:[b]}
```

only one of **[a]** or **[b]** is evaluated, depending on the value of **$v**.  This is not true of the normal Tcl parser, so it is normally recommended to enclose the arguments to **expr** in braces. Without braces, as in **expr** $v ? [a] : [b] both **[a]** and **[b]** are evaluated before **expr** is even called.

For more details on the results produced by each operator, see the documentation for C.

## Math functions

A mathematical function such as **sin($x)** is replaced with a call to an ordinary Tcl command in the **tcl::mathfunc** namespace.  The evaluation of an expression such as

```
expr {sin($x+$y)}
```

is the same in every way as the evaluation of

```
expr {[tcl::mathfunc::sin [expr {$x+$y}]]}
```

which in turn is the same as the evaluation of

```
tcl::mathfunc::sin [expr {$x+$y}]
```

**tcl::mathfunc::sin** is resolved as described in **NAMESPACE RESOLUTION** in the **namespace**(n) documentation.  Given the default value of **namespace path**, **[namespace current]::tcl::mathfunc::sin** or **::tcl::mathfunc::sin** are the typical resolutions.

As in C, a mathematical function may accept multiple arguments separated by commas. Thus,

```
expr {hypot($x,$y)}
```

becomes

```
tcl::mathfunc::hypot $x $y
```

See the **mathfunc**(n) documentation for the math functions that are available by default.

## Types, overflow, and precision

Internal floating-point computations are performed using the *double* C type. When converting a string to floating-point value, exponent overflow is detected and results in the *double* value of **Inf** or **-Inf** as appropriate.  Floating-point overflow and underflow are detected to the degree supported by the hardware, which is generally fairly reliable.

Conversion among internal representations for integer, floating-point, and string operands is done automatically as needed.  For arithmetic computations, integers are used until some floating-point number is introduced, after which floating-point values are used.  For example,

```
expr {5 / 4}
```

returns 1, while

```
expr {5 / 4.0}
expr {5 / ( [string length "abcd"] + 0.0 )}
```

both return 1.25. A floating-point result can be distinguished from an integer result by the presence of either "**.**" or "**e**"

. For example,

```
expr {20.0/5.0}
```

returns **4.0**, not **4**.

# Performance considerations

Where an expression contains syntax that Tcl would otherwise perform substitutions on, enclosing an expression in braces or otherwise quoting it so that it's a static value allows the Tcl compiler to generate bytecode for the expression, resulting in better speed and smaller storage requirements. This also avoids issues that can arise if Tcl is allowed to perform substitution on the value before **expr** is called.

In the following example, the value of the expression is 11 because the Tcl parser first substitutes **$b** and **expr** then substitutes **$a** as part of evaluating the expression "$a + 2*4". Enclosing the expression in braces would result in a syntax error as **$b** does not evaluate to a numeric value.

```
set a 3
set b {$a + 2}
expr $b*4
```

When an expression is generated at runtime, like the one above is, the bytecode compiler must ensure that new code is generated each time the expression is evaluated.  This is the most costly kind of expression from a performance perspective.  In such cases, consider directly using the commands described in the **mathfunc**(n) or **mathop**(n) documentation instead of **expr**.

Most expressions are not formed at runtime, but are literal strings or contain substitutions that don't introduce other substitutions.  To allow the bytecode compiler to work with an expression as a string literal at compilation time, ensure that it contains no substitutions or that it is enclosed in braces or otherwise quoted to prevent Tcl from performing substitutions, allowing **expr** to perform them instead.

If it is necessary to include a non-constant expression string within the wider context of an otherwise-constant expression, the most efficient technique is to put the varying part inside a recursive **expr**, as this at least allows for the compilation of the outer part, though it does mean that the varying part must itself be evaluated as a separate expression. Thus, in this example the result is 20 and the outer expression benefits from fully cached bytecode compilation.

```
set a 3
set b {$a + 2}
expr {[expr $b] * 4}
```

In general, you should enclose your expression in braces wherever possible, and where not possible, the argument to **expr** should be an expression defined elsewhere as simply as possible. It is usually more efficient and safer to use other techniques (e.g., the commands in the **tcl::mathop** namespace) than it is to do complex expression generation.

# Examples

A numeric comparison whose result is 1:

```
expr {"0x03" > "2"}
```

A string comparison whose result is 1:

```
expr {"0y" > "0x12"}
```

::: {.info version="TIP461"}
A forced string comparison whose result is 0:
:::

```
expr {"0x03" gt "2"}
```

Define a procedure that computes an "interesting" mathematical function:

```
proc tcl::mathfunc::calc {x y} {
    expr { ($x**2 - $y**2) / exp($x**2 + $y**2) }
}
```

Convert polar coordinates into cartesian coordinates:

```
# convert from ($radius,$angle)
set x [expr { $radius * cos($angle) }]
set y [expr { $radius * sin($angle) }]
```

Convert cartesian coordinates into polar coordinates:

```
# convert from ($x,$y)
set radius [expr { hypot($y, $x) }]
set angle  [expr { atan2($y, $x) }]
```

Print a message describing the relationship of two string values to each other:

```
puts "a and b are [expr {$a eq $b ? {equal} : {different}}]"
```

Set a variable indicating whether an environment variable is defined and has value of true:

```
set isTrue [expr {
    # Does the environment variable exist, and...
    [info exists ::env(SOME_ENV_VAR)] &&
    # ...does it contain a proper true value?
    [string is true -strict $::env(SOME_ENV_VAR)]
}]
```

Generate a random integer in the range 0..99 inclusive:

```
set randNum [expr { int(100 * rand()) }]
```

# Copyright

Copyright \(co 1993 The Regents of the University of California. Copyright \(co 1994-2000 Sun Microsystems Incorporated. Copyright \(co 2005 Kevin B. Kenny <kennykb@acm.org>. All rights reserved. 

