---
CommandName: lsearch
ManualSection: n
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - foreach(n)
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
 - lseq(n)
 - lset(n)
 - lsort(n)
 - string(n)
Keywords:
 - binary search
 - linear search
 - list
 - match
 - pattern
 - regular expression
 - search
 - string
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2001 Kevin B. Kenny <kennykb@acm.org>. All rights reserved.
 - Copyright (c) 2003-2004 Donal K. Fellows.
---

# Name

lsearch - See if a list contains a particular element

# Synopsis

::: {.synopsis} :::
[lsearch]{.cmd} [options]{.optarg} [list]{.arg} [pattern]{.arg}
:::

# Description

This command searches the elements of \fIlist\fR to see if one of them matches \fIpattern\fR.  If so, the command returns the index of the first matching element (unless the options \fB-all\fR or \fB-inline\fR are specified.) If not, the command returns \fB-1\fR or (if options \fB-all\fR or \fB-inline\fR are specified) the empty string.  The \fIoption\fR arguments indicates how the elements of the list are to be matched against \fIpattern\fR and must have one of the values below:

## Matching style options

If all matching style options are omitted, the default matching style is \fB-glob\fR.  If more than one matching style is specified, the last matching style given takes precedence.

**-exact**
: *Pattern* is a literal string that is compared for exact equality against each list element.

**-glob**
: *Pattern* is a glob-style pattern which is matched against each list element using the same rules as the \fBstring match\fR command.

**-regexp**
: *Pattern* is treated as a regular expression and matched against each list element using the rules described in the \fBre_syntax\fR reference page.

**-sorted**
: The list elements are in sorted order.  If this option is specified, \fBlsearch\fR will use a more efficient searching algorithm to search \fIlist\fR.  If no other options are specified, \fIlist\fR is assumed to be sorted in increasing order, and to contain ASCII strings.  This option is mutually exclusive with \fB-glob\fR and \fB-regexp\fR, and is treated exactly like \fB-exact\fR when either \fB-all\fR or \fB-not\fR are specified.


## General modifier options

These options may be given with all matching styles.

**-all**
: Changes the result to be the list of all matching indices (or all matching values if \fB-inline\fR is specified as well.) If indices are returned, the indices will be in ascending numeric order. If values are returned, the order of the values will be the order of those values within the input \fIlist\fR.

**-inline**
: The matching value is returned instead of its index (or an empty string if no value matches.)  If \fB-all\fR is also specified, then the result of the command is the list of all values that matched.

**-not**
: This negates the sense of the match, returning the index of the first non-matching value in the list.

**-start**\0\fIindex\fR
: The list is searched starting at position \fIindex\fR. The interpretation of the \fIindex\fR value is the same as for the command \fBstring index\fR, supporting simple index arithmetic and indices relative to the end of the list.


## Contents description options

These options describe how to interpret the items in the list being searched.  They are only meaningful when used with the \fB-exact\fR and \fB-sorted\fR options.  If more than one is specified, the last one takes precedence.  The default is \fB-ascii\fR.

**-ascii**
: The list elements are to be examined as Unicode strings (the name is for backward-compatibility reasons.)

**-dictionary**
: The list elements are to be compared using dictionary-style comparisons (see \fBlsort\fR for a fuller description). Note that this only makes a meaningful difference from the \fB-ascii\fR option when the \fB-sorted\fR option is given, because values are only dictionary-equal when exactly equal.

**-integer**
: The list elements are to be compared as integers.

**-nocase**
: Causes comparisons to be handled in a case-insensitive manner.  Has no effect if combined with the \fB-dictionary\fR, \fB-integer\fR, or \fB-real\fR options.

**-real**
: The list elements are to be compared as floating-point values.


## Sorted list options

These options (only meaningful with the \fB-sorted\fR option) specify how the list is sorted.  If more than one is given, the last one takes precedence.  The default option is \fB-increasing\fR.

**-decreasing**
: The list elements are sorted in decreasing order.  This option is only meaningful when used with \fB-sorted\fR.

**-increasing**
: The list elements are sorted in increasing order.  This option is only meaningful when used with \fB-sorted\fR.

**-bisect**
: Inexact search when the list elements are in sorted order. For an increasing list the last index where the element is less than or equal to the pattern is returned. For a decreasing list the last index where the element is greater than or equal to the pattern is returned. If the pattern is before the first element or the list is empty, -1 is returned. This option implies \fB-sorted\fR and cannot be used with either \fB-all\fR or \fB-not\fR.


## Nested list options

These options are used to search lists of lists.  They may be used with any other options.

**-stride\0***strideLength*
: If this option is specified, the list is treated as consisting of groups of \fIstrideLength\fR elements and the groups are searched by either their first element or, if the \fB-index\fR option is used, by the element within each group given by the first index passed to \fB-index\fR (which is then ignored by \fB-index\fR). The resulting index always points to the first element in a group.


The list length must be an integer multiple of \fIstrideLength\fR, which in turn must be at least 1. A \fIstrideLength\fR of 1 is the default and indicates no grouping.

**-index**\0\fIindexList\fR
: This option is designed for use when searching within nested lists. The \fIindexList\fR argument gives a path of indices (much as might be used with the \fBlindex\fR or \fBlset\fR commands) within each element to allow the location of the term being matched against.

**-subindices**
: If this option is given, the index result from this command (or every index result when \fB-all\fR is also specified) will be a complete path (suitable for use with \fBlindex\fR or \fBlset\fR) within the overall list to the term found.  This option has no effect unless the \fB-index\fR is also specified, and is just a convenience short-cut.


# Examples

Basic searching:

```
lsearch {a b c d e} c
      \(-> 2
lsearch -all {a b c a b c} c
      \(-> 2 5
```

Using \fBlsearch\fR to filter lists:

```
lsearch -inline {a20 b35 c47} b*
      \(-> b35
lsearch -inline -not {a20 b35 c47} b*
      \(-> a20
lsearch -all -inline -not {a20 b35 c47} b*
      \(-> a20 c47
lsearch -all -not {a20 b35 c47} b*
      \(-> 0 2
```

This can even do a "set-like" removal operation:

```
lsearch -all -inline -not -exact {a b c a d e a f g a} a
      \(-> b c d e f g
```

Searching may start part-way through the list:

```
lsearch -start 3 {a b c a b c} c
      \(-> 5
```

It is also possible to search inside elements:

```
lsearch -index 1 -all -inline {{abc abc} {abc bcd} {abc cde}} *bc*
      \(-> {abc abc} {abc bcd}
```

The same thing for a flattened list:

```
lsearch -stride 2 -index 1 -all -inline {abc abc abc bcd abc cde} *bc*
      \(-> abc abc abc bcd
```

