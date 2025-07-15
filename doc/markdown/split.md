---
CommandName: split
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - join(n)
 - list(n)
 - string(n)
Keywords:
 - list
 - split
 - string
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

split - Split a string into a proper Tcl list

# Synopsis

::: {.synopsis} :::
[split]{.cmd} [string]{.arg} [splitChars]{.optarg}
:::

# Description

Returns a list created by splitting \fIstring\fR at each character that is in the \fIsplitChars\fR argument. Each element of the result list will consist of the characters from \fIstring\fR that lie between instances of the characters in \fIsplitChars\fR. Empty list elements will be generated if \fIstring\fR contains adjacent characters in \fIsplitChars\fR, or if the first or last character of \fIstring\fR is in \fIsplitChars\fR. If \fIsplitChars\fR is an empty string then each character of \fIstring\fR becomes a separate element of the result list. \fISplitChars\fR defaults to the standard white-space characters.

# Examples

Divide up a USENET group name into its hierarchical components:

```
split "comp.lang.tcl" .
      \(-> comp lang tcl
```

See how the \fBsplit\fR command splits on \fIevery\fR character in \fIsplitChars\fR, which can result in information loss if you are not careful:

```
split "alpha beta gamma" "temp"
      \(-> al {ha b} {} {a ga} {} a
```

Extract the list words from a string that is not a well-formed list:

```
split "Example with {unbalanced brace character"
      \(-> Example with \{unbalanced brace character
```

Split a string into its constituent characters

```
split "Hello world" {}
      \(-> H e l l o { } w o r l d
```

## Parsing record-oriented files

Parse a Unix /etc/passwd file, which consists of one entry per line, with each line consisting of a colon-separated list of fields:

```
## Read the file
set fid [open /etc/passwd]
set content [read $fid]
close $fid

## Split into records on newlines
set records [split $content "\n"]

## Iterate over the records
foreach rec $records {

    ## Split into fields on colons
    set fields [split $rec ":"]

    ## Assign fields to variables and print some out...
    lassign $fields \
            userName password uid grp longName homeDir shell
    puts "$longName uses [file tail $shell] for a login shell"
}
```

