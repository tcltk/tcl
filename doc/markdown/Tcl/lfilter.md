---
CommandName: lfilter
title: lfilter
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - dict(n)
 - foreach(n)
 - lmap(n)
 - lsearch(n)
Keywords:
 - foreach
 - iteration
 - list
 - loop
 - filter
Copyright:
 - Copyright (c) 2025 Donal K. Fellows
---

# Name

lfilter - Select elements from a list based on an expression

# Synopsis

::: {.synopsis} :::
[lfilter]{.cmd} [varname]{.arg} [list]{.arg} [expression]{.arg}
[lfilter]{.cmd} [varlist1]{.arg} [list1]{.arg} [varlist2 list2]{.optdot} [body]{.arg}
:::

# Description

The **lfilter** command implements a loop where the loop variable(s) take on values from one or more lists, and the loop returns a list of values from the (first) list where the *body* script evaluates to a true value.

In the simplest case there is one loop variable, *varname*, and one list, *list*, that is a list of values to assign to *varname*. The *body* argument is a Tcl script. For each element of *list* (in order from first to last), **lfilter** assigns the contents of the element to *varname* as if the [lindex] command had been used to extract the element, then calls the Tcl interpreter to execute *body*. If execution of the script completes normally and produces a true value, that element of *list* is appended to an accumulator list. [lmap] returns the accumulator list.

In the general case there can be more than one value list (e.g., *list1* and *list2*), and each value list can be associated with a list of loop variables (e.g., *varlist1* and *varlist2*). During each iteration of the loop the variables of each *varlist* are assigned consecutive values from the corresponding *list*. Values in each *list* are used in order from first to last, and each value is used exactly once. The total number of loop iterations is large enough to use up all the values from all the value lists. If a value list does not contain enough elements for each of its loop variables in each iteration, empty values are used for the missing elements. For each iteration, the value that is appended when *body* yields a true value (i.e., anything accepted by [Tcl\_GetBooleanFromObj][BoolObj] as a boolean) is the value that was assigned to the first variable of the first list of variables; all other values are not collected.

The [break] and [continue] statements may be invoked inside *body*, with the same effect as in the [for] and [foreach] commands. In these cases the body does not complete normally and the lead value is not appended to the accumulator list.

# Examples

Select the elements of the list that are odd:

```
lfilter x {0 2 4 1 3 5 10 8 6 11 9 7} {
    expr {($x % 2) == 1}
}
# The result is "1 3 5 11 9 7"
```

Select elements of a list according to a condition in another list:

```
lfilter x [lseq 10] y [lseq 0 .. 1 by 0.1] {
    expr {$y > 0.3 && $y < 0.7}
}
# The result is "4 5 6"
```

Select the elements containing at least five characters:

```
set sentence "The quick brown fox jumps over the lazy dog."
lfilter word [split $sentence] {
    expr {[string length $word] >= 5}
}
# The result is "quick brown jumps"
```

Select the elements that are true palindromes:

```
set usernames {tom bob steve dood}
lfilter word $usernames {
    string equal $word [string reverse $word]
}
# The result is "bob dood"
```


[BoolObj]: ../TclCAPI/BoolObj.md
[break]: break.md
[continue]: continue.md
[for]: for.md
[foreach]: foreach.md
[lindex]: lindex.md
[lmap]: lmap.md

