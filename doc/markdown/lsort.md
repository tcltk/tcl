---
CommandName: lsort
ManualSection: n
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - list(n)
 - lappend(n)
 - lassign(n)
 - ledit(n)
 - lindex(n)
 - linsert(n)
 - llength(n)
 - lmap(n)
 - lpop(n)
 - lrange(n)
 - lremove(n)
 - lrepeat(n)
 - lreplace(n)
 - lreverse(n)
 - lsearch(n)
 - lseq(n)
 - lset(n)
Keywords:
 - element
 - list
 - order
 - sort
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 1999 Scriptics Corporation
 - Copyright (c) 2001 Kevin B. Kenny <kennykb@acm.org>. All rights reserved.
---

# Name

lsort - Sort the elements of a list

# Synopsis

::: {.synopsis} :::
[lsort]{.cmd} [options]{.optarg} [list]{.arg}
:::

# Description

This command sorts the elements of \fIlist\fR, returning a new list in sorted order.  The implementation of the \fBlsort\fR command uses the merge-sort algorithm which is a stable sort that has O(n log n) performance characteristics.

By default ASCII sorting is used with the result returned in increasing order.  However, any of the following options may be specified before \fIlist\fR to control the sorting process (unique abbreviations are accepted):

**-ascii**
: Use string comparison with Unicode code-point collation order (the name is for backward-compatibility reasons.)  This is the default.

**-dictionary**
: Use dictionary-style comparison.  This is the same as \fB-ascii\fR except (a) case is ignored except as a tie-breaker and (b) if two strings contain embedded numbers, the numbers compare as integers, not characters.  For example, in \fB-dictionary\fR mode, \fBbigBoy\fR sorts between \fBbigbang\fR and \fBbigboy\fR, and \fBx10y\fR sorts between \fBx9y\fR and \fBx11y\fR. Overrides the \fB-nocase\fR option.

**-integer**
: Convert list elements to integers and use integer comparison.

**-real**
: Convert list elements to floating-point values and use floating comparison.

**-command\0***command*
: Use \fIcommand\fR as a comparison command. To compare two elements, evaluate a Tcl script consisting of \fIcommand\fR with the two elements appended as additional arguments.  The script should return an integer less than, equal to, or greater than zero if the first element is to be considered less than, equal to, or greater than the second, respectively.

**-increasing**
: Sort the list in increasing order ("smallest"items first). This is the default.

**-decreasing**
: Sort the list in decreasing order ("largest"items first).

**-indices**
: Return a list of indices into \fIlist\fR in sorted order instead of the values themselves.

**-index\0***indexList*
: If this option is specified, each of the elements of \fIlist\fR must itself be a proper Tcl sublist (unless \fB-stride\fR is used). Instead of sorting based on whole sublists, \fBlsort\fR will extract the \fIindexList\fR'th element from each sublist (as if the overall element and the \fIindexList\fR were passed to \fBlindex\fR) and sort based on the given element. For example,

    ```
    lsort -integer -index 1 \
          {{First 24} {Second 18} {Third 30}}
    ```
    returns \fB{Second 18} {First 24} {Third 30}\fR,

    ```
    lsort -index end-1 \
            {{a 1 e i} {b 2 3 f g} {c 4 5 6 d h}}
    ```
    returns \fB{c 4 5 6 d h} {a 1 e i} {b 2 3 f g}\fR, and

    ```
    lsort -index {0 1} {
        {{b i g} 12345}
        {{d e m o} 34512}
        {{c o d e} 54321}
    }
    ```
    returns \fB{{d e m o} 34512} {{b i g} 12345} {{c o d e} 54321}\fR (because \fBe\fR sorts before \fBi\fR which sorts before \fBo\fR.) This option is much more efficient than using \fB-command\fR to achieve the same effect.

**-stride\0***strideLength*
: If this option is specified, the list is treated as consisting of groups of \fIstrideLength\fR elements and the groups are sorted by either their first element or, if the \fB-index\fR option is used, by the element within each group given by the first index passed to \fB-index\fR (which is then ignored by \fB-index\fR). Elements always remain in the same position within their group.
    The list length must be an integer multiple of \fIstrideLength\fR, which in turn must be at least 2.
    For example,

    ```
    lsort -stride 2 {carrot 10 apple 50 banana 25}
    ```
    returns "apple 50 banana 25 carrot 10", and

    ```
    lsort -stride 2 -index 1 -integer {carrot 10 apple 50 banana 25}
    ```
    returns "carrot 10 banana 25 apple 50".

**-nocase**
: Causes comparisons to be handled in a case-insensitive manner.  Has no effect if combined with the \fB-dictionary\fR, \fB-integer\fR, or \fB-real\fR options.

**-unique**
: If this option is specified, then only the last set of duplicate elements found in the list will be retained.  Note that duplicates are determined relative to the comparison used in the sort.  Thus if \fB-index 0\fR is used, \fB{1 a}\fR and \fB{1 b}\fR would be considered duplicates and only the second element, \fB{1 b}\fR, would be retained.


# Notes

The options to \fBlsort\fR only control what sort of comparison is used, and do not necessarily constrain what the values themselves actually are.  This distinction is only noticeable when the list to be sorted has fewer than two elements.

The \fBlsort\fR command is reentrant, meaning it is safe to use as part of the implementation of a command used in the \fB-command\fR option.

# Examples

Sorting a list using ASCII sorting:

```
% lsort {a10 B2 b1 a1 a2}
B2 a1 a10 a2 b1
```

Sorting a list using Dictionary sorting:

```
% lsort -dictionary {a10 B2 b1 a1 a2}
a1 a2 a10 b1 B2
```

Sorting lists of integers:

```
% lsort -integer {5 3 1 2 11 4}
1 2 3 4 5 11
% lsort -integer {1 2 0x5 7 0 4 -1}
-1 0 1 2 4 0x5 7
```

Sorting lists of floating-point numbers:

```
% lsort -real {5 3 1 2 11 4}
1 2 3 4 5 11
% lsort -real {.5 0.07e1 0.4 6e-1}
0.4 .5 6e-1 0.07e1
```

Sorting using indices:

```
% # Note the space character before the c
% lsort {{a 5} { c 3} {b 4} {e 1} {d 2}}
{ c 3} {a 5} {b 4} {d 2} {e 1}
% lsort -index 0 {{a 5} { c 3} {b 4} {e 1} {d 2}}
{a 5} {b 4} { c 3} {d 2} {e 1}
% lsort -index 1 {{a 5} { c 3} {b 4} {e 1} {d 2}}
{e 1} {d 2} { c 3} {b 4} {a 5}
```

Sorting a dictionary:

```
% set d [dict create c d a b h i f g c e]
c e a b h i f g
% lsort -stride 2 $d
a b c e f g h i
```

Sorting using striding and multiple indices:

```
% # Note the first index value is relative to the group
% lsort -stride 3 -index {0 1} \
     {{Bob Smith} 25 Audi {Jane Doe} 40 Ford}
{{Jane Doe} 40 Ford {Bob Smith} 25 Audi}
```

Stripping duplicate values using sorting:

```
% lsort -unique {a b c a b c a b c}
a b c
```

More complex sorting using a comparison function:

```
% proc compare {a b} {
    set a0 [lindex $a 0]
    set b0 [lindex $b 0]
    if {$a0 < $b0} {
        return -1
    } elseif {$a0 > $b0} {
        return 1
    }
    return [string compare [lindex $a 1] [lindex $b 1]]
}
% lsort -command compare \
        {{3 apple} {0x2 carrot} {1 dingo} {2 banana}}
{1 dingo} {2 banana} {0x2 carrot} {3 apple}
```

