---
CommandName: remquo
title: remquo
ManualSection: n
Version: 9.1
TclPart: Tcl
TclDescription: Tcl Float Remainder
Links:
 - expr(n)
 - mathfunc(n)
Keywords:
 - floating point
Copyright:
 - Copyright (c) 2025 Donal Fellows
---

# Name

remquo - Compute floating point remainder and quadrant determinant

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl]{.lit} [9.1]{.lit}
[remquo]{.cmd} [x]{.arg} [y]{.arg}
:::

# Description

The **remquo** command computes the floating-point remainder of *x*/*y*, much like the **remainder** function. Its result is a list of two values; the first value is the remainder, and the second value is an integer containing sign and bits sufficient to determine which octant the gradient was located within.

# Example

```
lassign [remquo 12.3 3.89] rem quo
puts "rem=$rem, quo=$quo"
#       rem=0.6300000000000003, quo=3
```

