---
CommandName: modf
title: modf
ManualSection: n
Version: 9.1
TclPart: Tcl
TclDescription: Tcl Float Decomposition
Links:
 - expr(n)
 - frexp(n)
 - mathfunc(n)
Keywords:
 - floating point
Copyright:
 - Copyright (c) 2025 Donal Fellows
---

# Name

modf - Split a floating-point number into integer and fraction parts

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl]{.lit} [9.1]{.lit}
[modf]{.cmd} [value]{.arg}
:::

# Description

The **modf** command takes a floating-point number, *value*, and returns a list of two values. The first element of the result is the whole integer part (as a floating-point number) of *value*, and the second element of the result is the fractional part of *value*. The signs of the two elements will match the sign of *value*.

# Example

It should be noted that the fractional part may have some digits that appear to be undetermined by the input. This is due to the nature of approximation inherent in floating-point arithmetic; the digits will apparently disappear if the two parts are added back together.

```
set value 123.456
lassign [modf $value] whole part
set sum [expr {$whole + $part}]
puts "$value -> $whole $part -> $sum"
#       123.456 -> 123.0 0.45600000000000307 -> 123.456
```

