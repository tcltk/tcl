---
CommandName: fpclassify
ManualSection: n
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Float Classifier
Links:
 - expr(n)
 - mathfunc(n)
Keywords:
 - floating point
Copyright:
 - Copyright (c) 2018 Kevin B. Kenny <kennykb@acm.org>. All rights reserved
 - Copyright (c) 2019 Donal Fellows
---

# Name

fpclassify - Floating point number classification of Tcl values

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl]{.lit} [9.0]{.lit}
[fpclassify]{.cmd} [value]{.arg}
:::

# Description

The \fBfpclassify\fR command takes a floating point number, \fIvalue\fR, and returns one of the following strings that describe it:

**zero**
: *value* is a floating point zero.

**subnormal**
: *value* is the result of a gradual underflow.

**normal**
: *value* is an ordinary floating-point number (not zero, subnormal, infinite, nor NaN).

**infinite**
: *value* is a floating-point infinity.

**nan**
: *value* is Not-a-Number.


The \fBfpclassify\fR command throws an error if value is not a floating-point value and cannot be converted to one.

# Example

This shows how to check whether the result of a computation is numerically safe or not. (Note however that it does not guard against numerical errors; just against representational problems.)

```
set value [command-that-computes-a-value]
switch [fpclassify $value] {
    normal - zero {
        puts "Result is $value"
    }
    infinite {
        puts "Result is infinite"
    }
    subnormal {
        puts "Result is $value - WARNING! precision lost"
    }
    nan {
        puts "Computation completely failed"
    }
}
```

# Standards

This command depends on the \fBfpclassify\fR() C macro conforming to "ISO C99" (i.e., to ISO/IEC 9899:1999).

# Copyright

Copyright \(co 2018 Kevin B. Kenny <kennykb@acm.org>. All rights reserved 

