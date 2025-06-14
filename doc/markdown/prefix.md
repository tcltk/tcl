---
CommandName: prefix
ManualSection: n
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - lsearch(n)
 - namespace(n)
 - string(n)
 - Tcl_GetIndexFromObj(3)
Keywords:
 - prefix
 - table lookup
Copyright:
 - Copyright (c) 2008 Peter Spjuth <pspjuth@users.sourceforge.net>
---

# Name

tcl::prefix - facilities for prefix matching

# Synopsis

::: {.synopsis} :::
[::tcl::prefix]{.cmd} [all]{.sub} [table]{.arg} [string]{.arg}
[::tcl::prefix]{.cmd} [longest]{.sub} [table]{.arg} [string]{.arg}
[::tcl::prefix]{.cmd} [match]{.sub} [option]{.optdot} [table]{.arg} [string]{.arg}
:::

# Description

This document describes commands looking up a prefix in a list of strings. The following commands are supported:

**::tcl::prefix all** *table string*
: Returns a list of all elements in *table* that begin with the prefix *string*.

**::tcl::prefix longest** *table string*
: Returns the longest common prefix of all elements in *table* that begin with the prefix *string*.

**::tcl::prefix match** ?*option ...*? *table string*
: If *string* equals one element in *table* or is a prefix to exactly one element, the matched element is returned. If not, the result depends on the **-error** option. (It is recommended that the *table* be sorted before use with this subcommand, so that the list of matches presented in the error message also becomes sorted, though this is not strictly necessary for the operation of this subcommand itself.) The following options are supported:

**-exact**
: Accept only exact matches.

**-message\0***string*
: Use *string* in the error message at a mismatch. Default is "option".

**-error\0***options*
: The *options* are used when no match is found. If *options* is empty, no error is generated and an empty string is returned. Otherwise the *options* are used as **return** options when generating the error message. The default corresponds to setting "-level 0". Example: If "**-error** {-errorcode MyError -level 1}" is used, an error would be generated as:


    ```
    return -errorcode MyError -level 1 -code error \
           "ambiguous option ..."
    ```


# Examples

Basic use:

```
namespace import ::tcl::prefix
prefix match {apa bepa cepa} apa
     \(-> apa
prefix match {apa bepa cepa} a
     \(-> apa
prefix match -exact {apa bepa cepa} a
     \(-> bad option "a": must be apa, bepa, or cepa
prefix match -message "switch" {apa ada bepa cepa} a
     \(-> ambiguous switch "a": must be apa, ada, bepa, or cepa
prefix longest {fblocked fconfigure fcopy file fileevent flush} fc
     \(-> fco
prefix all {fblocked fconfigure fcopy file fileevent flush} fc
     \(-> fconfigure fcopy
```

Simplifying option matching:

```
array set opts {-apa 1 -bepa "" -cepa 0}
foreach {arg val} $args {
    set opts([prefix match {-apa -bepa -cepa} $arg]) $val
}
```

Creating a **switch** that supports prefixes:

```
switch [prefix match {apa bepa cepa} $arg] {
    apa  { }
    bepa { }
    cepa { }
}
```

