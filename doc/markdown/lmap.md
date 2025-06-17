---
CommandName: lmap
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - break(n)
 - continue(n)
 - for(n)
 - foreach(n)
 - while(n)
 - list(n)
 - lappend(n)
 - lassign(n)
 - ledit(n)
 - lindex(n)
 - linsert(n)
 - llength(n)
 - lpop(n)
 - lrange(n)
 - lremove(n)
 - lrepeat(n)
 - lreplace(n)
 - lreverse(n)
 - lsearch(n)
 - lseq(n)
 - lset(n)
 - lsort(n)
Keywords:
 - foreach
 - iteration
 - list
 - loop
 - map
Copyright:
 - Copyright (c) 2012 Trevor Davel
---

# Name

lmap - Iterate over all elements in one or more lists and collect results

# Synopsis

::: {.synopsis} :::
[lmap]{.cmd} [varname]{.arg} [list]{.arg} [body]{.arg}
[lmap]{.cmd} [varlist1]{.arg} [list1]{.arg} [varlist2 list2]{.optdot} [body]{.arg}
:::

# Description

The **lmap** command implements a loop where the loop variable(s) take on values from one or more lists, and the loop returns a list of results collected from each iteration.

In the simplest case there is one loop variable, *varname*, and one list, *list*, that is a list of values to assign to *varname*. The *body* argument is a Tcl script. For each element of *list* (in order from first to last), **lmap** assigns the contents of the element to *varname* as if the **lindex** command had been used to extract the element, then calls the Tcl interpreter to execute *body*. If execution of the body completes normally then the result of the body is appended to an accumulator list. **lmap** returns the accumulator list.

In the general case there can be more than one value list (e.g., *list1* and *list2*), and each value list can be associated with a list of loop variables (e.g., *varlist1* and *varlist2*). During each iteration of the loop the variables of each *varlist* are assigned consecutive values from the corresponding *list*. Values in each *list* are used in order from first to last, and each value is used exactly once. The total number of loop iterations is large enough to use up all the values from all the value lists. If a value list does not contain enough elements for each of its loop variables in each iteration, empty values are used for the missing elements.

The **break** and **continue** statements may be invoked inside *body*, with the same effect as in the **for** and **foreach** commands. In these cases the body does not complete normally and the result is not appended to the accumulator list.

# Examples

Zip lists together:

```
set list1 {a b c d}
set list2 {1 2 3 4}
set zipped [lmap a $list1 b $list2 {list $a $b}]
# The value of zipped is "{a 1} {b 2} {c 3} {d 4}"
```

Filter a list to remove odd values:

```
set values {1 2 3 4 5 6 7 8}
proc isEven {n} {expr {($n % 2) == 0}}
set goodOnes [lmap x $values {expr {
    [isEven $x] ? $x : [continue]
}}]
# The value of goodOnes is "2 4 6 8"
```

Take a prefix from a list based on the contents of the list:

```
set values {8 7 6 5 4 3 2 1}
proc isGood {counter} {expr {$n > 3}}
set prefix [lmap x $values {expr {
    [isGood $x] ? $x : [break]
}}]
# The value of prefix is "8 7 6 5 4"
```

