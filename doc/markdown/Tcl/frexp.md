---
CommandName: frexp
title: frexp
ManualSection: n
Version: 9.1
TclPart: Tcl
TclDescription: Tcl Float Decomposition
Links:
 - expr(n)
 - mathfunc(n)
 - modf(n)
Keywords:
 - floating point
Copyright:
 - Copyright (c) 2025 Donal Fellows
---

# Name

frexp - Split a floating-point number into fraction and exponent parts

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl]{.lit} [9.1]{.lit}
[frexp]{.cmd} [value]{.arg}
:::

# Description

The **frexp** command takes a floating-point number, *value*, and returns a list of two values. The first element of the result is the fractional part of *value* (signed, of magnitude between 0.5 and 1.0), and the second element of the result is the integer exponent from *value*. When *value* is infinite, the fraction is also infinite (of the same sign).

# Example

The **frexp** command is paired with the **ldexp** function, like this:

```
set value 123.456
lassign [frexp $value] frac exp
set recovered [expr { ldexp($frac, $exp) }]
puts "$value -> $frac*2**$exp -> $recovered"
#       123.456 -> 0.9645*2**7 -> 123.456
```

