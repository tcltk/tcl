---
CommandName: lseq
ManualSection: n
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - foreach(n)
 - list(n)
 - lappend(n)
 - lassign(n)
 - lindex(n)
 - linsert(n)
 - llength(n)
 - lmap(n)
 - lpop(n)
 - lrange(n)
 - lremove(n)
 - lreplace(n)
 - lreverse(n)
 - lsearch(n)
 - lset(n)
 - lsort(n)
Keywords:
 - element
 - index
 - list
Copyright:
 - Copyright (c) 2022 Eric Taylor. All rights reserved.
---

# Name

lseq - Build a numeric sequence returned as a list

# Synopsis

::: {.synopsis} :::
[lseq]{.cmd} [start]{.arg} [end]{.arg}
[lseq]{.cmd} [start]{.arg} [to]{.optlit} [end]{.arg} [stepSpec]{.optarg}
[lseq]{.cmd} [start]{.arg} [..]{.optlit} [end]{.arg} [stepSpec]{.optarg}
[lseq]{.cmd} [start]{.arg} [count]{.lit} [count]{.arg} [stepSpec]{.optarg}

[lseq]{.cmd} [count]{.arg} [[by]{.lit} [stepValue]{.arg}]{.optarg}
:::

# Description

The **lseq** command creates a sequence of numeric values using the arguments *start*, *end*, *count*, and optionally *stepSpec*/*stepValue*. 

If a *start* value is specified as the first argument, the *end* value of the sequence can be specified as the next argument, optionally preceeded by the literal word "**to**" or "**..**". Alternatively, the *count* (number of elements in the sequence) can be defined instead when preceeded by the word **count**. In both cases, the interval between the subsequent numbers of the sequence can be specified using the optional *stepSpec* argument. It is defined as "?**by**? *stepValue*", i.e. the optional word **by** followed by the step value.

A short form use of the command with a single argument *count* will create a sequence from 0 to *count*-1. It can optioanlly be followed by a step value preceeded by the word **by**.

The **lseq** command can produce both increasing and decreasing sequences. When both *start* and *end* are provided without a *stepValue*, then if *start* <= *end*, the sequence will be increasing and if *start* > *end* it will be decreasing. If the *stepValue* is included, it's sign should agree with the direction of the sequence (descending \(-> negative and ascending \(-> positive), otherwise an empty list is returned.  For example:

```
% lseq 1 to 5    ;# increasing
\(-> 1 2 3 4 5

% lseq 5 to 1    ;# decreasing
\(-> 5 4 3 2 1

% lseq 6 to 1 by 2   ;# decreasing, step wrong sign, empty list

% lseq 1 to 5 by 0   ;# all step sizes of 0 produce an empty list
```

The numeric arguments in *start*, *end*, *stepValue* and *count* may also be valid expressions. The expression will be evaluated and the numeric result will be used.  An expression that does not evaluate to a number will produce an invalid argument error.

*Start* defines the initial value and *end* defines the limit, not necessarily the last value. **lseq** produces a list with *count* elements, and if *count* is not supplied, it is computed as:

```
count = int( (end - start + stepValue) / stepValueq )
```

# Examples

```
lseq 3
\(-> 0 1 2

lseq 3 0
\(-> 3 2 1 0

lseq 10 .. 1 by -2
\(-> 10 8 6 4 2

set l [lseq 0 -5]
\(-> 0 -1 -2 -3 -4 -5

foreach i [lseq [llength $l]] {
    puts l($i)=[lindex $l $i]
}
\(-> l(0)=0
\(-> l(1)=-1
\(-> l(2)=-2
\(-> l(3)=-3
\(-> l(4)=-4
\(-> l(5)=-5

foreach i [lseq {[llength $l]-1} 0] {
    puts l($i)=[lindex $l $i]
}
\(-> l(5)=-5
\(-> l(4)=-4
\(-> l(3)=-3
\(-> l(2)=-2
\(-> l(1)=-1
\(-> l(0)=0

set i 17
         \(-> 17
if {$i in [lseq 0 50]} { # equivalent to: (0 <= $i && $i <= 50)
    puts "Ok"
} else {
    puts "outside :("
}
\(-> Ok

set sqrs [lmap i [lseq 1 10] { expr {$i*$i} }]
\(-> 1 4 9 16 25 36 49 64 81 100
```

