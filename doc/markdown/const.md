---
CommandName: const
ManualSection: n
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - proc(n)
 - namespace(n)
 - set(n)
 - unset(n)
Keywords:
 - namespace
 - procedure
 - variable
 - constant
Copyright:
 - Copyright (c) 2023 Donal K. Fellows
---

# Name

const - create and initialize a constant

# Synopsis

::: {.synopsis} :::
[const]{.cmd} [varName]{.arg} [value]{.arg}
:::

# Description

This command is normally used within a procedure body (or method body, or lambda term) to create a constant within that procedure, or within a **namespace eval** body to create a constant within that namespace. The constant is an unmodifiable variable, called *varName*, that is initialized with *value*. The result of **const** is always the empty string on success.

If a variable *varName* does not exist, it is created with its value set to *value* and marked as a constant; this means that no other command (e.g., **set**, **append**, **incr**, **unset**) may modify or remove the variable; variables are checked for whether they are constants before any traces are called. If a variable *varName* already exists, it is an error unless that variable is marked as a constant (in which case **const** is a no-op).

The *varName* may not be a qualified name or reference an element of an array by any means. If the variable exists and is an array, that is an error.

Constants are normally only removed by their containing procedure exiting or their namespace being deleted.

# Examples

Create a constant in a procedure:

```
proc foo {a b} {
    const BAR 12345
    return [expr {$a + $b + $BAR}]
}
```

Create a constant in a namespace to factor out a regular expression:

```
namespace eval someNS {
    const FOO_MATCHER {(?i)}\mfoo\M}

    proc findFoos str {
        variable FOO_MATCHER
        regexp -all $FOO_MATCHER $str
    }

    proc findFooIndices str {
        variable FOO_MATCHER
        regexp -all -indices $FOO_MATCHER $str
    }
}
```

Making a constant in a loop doesn't error:

```
proc foo {n} {
    set result {}
    for {set i 0} {$i < $n} {incr i} {
        const X 123
	lappend result [expr {$X + $i**2}]
    }
}
```

