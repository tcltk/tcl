---
CommandName: divmod
title: divmod
ManualSection: n
Version: 9.1
TclPart: Tcl
TclDescription: Tcl Integer Division
Links:
 - expr(n)
 - mathfunc(n)
 - mathop(n)
Keywords:
 - division
 - integer value
Copyright:
 - Copyright (c) 2025 Donal Fellows
---

# Name

divmod - Divide an integer by another and report quotient and remainder

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl]{.lit} [9.1]{.lit}
[divmod]{.cmd} [x]{.arg} [y]{.arg}
:::

# Description

The **divmod** command takes two integer numbers, *x* and *y*, and returns a list of two values. The first element of the result is the whole quotient of *x*/*y*, and the second is the remainder such that the sum of the remainder and the product of the quotient and *y* is equal to *x*.

The quotient and remainder are calculated using the same rules as for Tcl's **/** and **%** operators.

# Example

```
lassign [divmod 123 50] q r
puts "quotient=$q remainder=$r"
#       quotient=2 remainder=23
```

