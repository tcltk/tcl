---
CommandName: regexp
ManualSection: n
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - re_syntax(n)
 - regsub(n)
 - string(n)
Keywords:
 - match
 - parsing
 - pattern
 - regular expression
 - splitting
 - string
Copyright:
 - Copyright (c) 1998 Sun Microsystems, Inc.
---

# Name

regexp - Match a regular expression against a string

# Synopsis

::: {.synopsis} :::
[regexp]{.cmd} [switches]{.optarg} [exp]{.arg} [string]{.arg} [matchVar]{.optarg} [subMatchVar subMatchVar]{.optdot}
:::

# Description

Determines whether the regular expression \fIexp\fR matches part or all of \fIstring\fR and returns 1 if it does, 0 if it does not, unless \fB-inline\fR is specified (see below). (Regular expression matching is described in the \fBre_syntax\fR reference page.)

If additional arguments are specified after \fIstring\fR then they are treated as the names of variables in which to return information about which part(s) of \fIstring\fR matched \fIexp\fR. \fIMatchVar\fR will be set to the range of \fIstring\fR that matched all of \fIexp\fR.  The first \fIsubMatchVar\fR will contain the characters in \fIstring\fR that matched the leftmost parenthesized subexpression within \fIexp\fR, the next \fIsubMatchVar\fR will contain the characters that matched the next parenthesized subexpression to the right in \fIexp\fR, and so on.

If the initial arguments to \fBregexp\fR start with \fB-\fR then they are treated as switches.  The following switches are currently supported:

**-about**
: Instead of attempting to match the regular expression, returns a list containing information about the regular expression.  The first element of the list is a subexpression count.  The second element is a list of property names that describe various attributes of the regular expression. This switch is primarily intended for debugging purposes.

**-expanded**
: Enables use of the expanded regular expression syntax where whitespace and comments are ignored.  This is the same as specifying the \fB(?x)\fR embedded option (see the \fBre_syntax\fR manual page).

**-indices**
: Changes what is stored in the \fImatchVar\fR and \fIsubMatchVar\fRs. Instead of storing the matching characters from \fIstring\fR, each variable will contain a list of two decimal strings giving the indices in \fIstring\fR of the first and last characters in the matching range of characters.

**-line**
: Enables newline-sensitive matching.  By default, newline is a completely ordinary character with no special meaning.  With this flag, "[^" bracket expressions and "." never match newline, "^" matches an empty string after any newline in addition to its normal function, and "$" matches an empty string before any newline in addition to its normal function.  This flag is equivalent to specifying both \fB-linestop\fR and \fB-lineanchor\fR, or the \fB(?n)\fR embedded option (see the \fBre_syntax\fR manual page).

**-linestop**
: Changes the behavior of "[^" bracket expressions and "." so that they stop at newlines.  This is the same as specifying the \fB(?p)\fR embedded option (see the \fBre_syntax\fR manual page).

**-lineanchor**
: Changes the behavior of "^" and "$" (the "anchors") so they match the beginning and end of a line respectively.  This is the same as specifying the \fB(?w)\fR embedded option (see the \fBre_syntax\fR manual page).

**-nocase**
: Causes upper-case characters in \fIstring\fR to be treated as lower case during the matching process.

**-all**
: Causes the regular expression to be matched as many times as possible in the string, returning the total number of matches found.  If this is specified with match variables, they will contain information for the last match only.

**-inline**
: Causes the command to return, as a list, the data that would otherwise be placed in match variables.  When using \fB-inline\fR, match variables may not be specified.  If used with \fB-all\fR, the list will be concatenated at each iteration, such that a flat list is always returned.  For each match iteration, the command will append the overall match data, plus one element for each subexpression in the regular expression.  Examples are:

    ```
    regexp -inline -- {\w(\w)} " inlined "
          \(-> in n
    regexp -all -inline -- {\w(\w)} " inlined "
          \(-> in n li i ne e
    ```

**-start** \fIindex\fR
: Specifies a character index offset into the string to start matching the regular expression at. The \fIindex\fR value is interpreted in the same manner as the \fIindex\fR argument to \fBstring index\fR. When using this switch, "^" will not match the beginning of the line, and \A will still match the start of the string at \fIindex\fR.  If \fB-indices\fR is specified, the indices will be indexed starting from the absolute beginning of the input string. \fIindex\fR will be constrained to the bounds of the input string.

**-\|-**
: Marks the end of switches.  The argument following this one will be treated as \fIexp\fR even if it starts with a \fB-\fR.


If there are more \fIsubMatchVar\fRs than parenthesized subexpressions within \fIexp\fR, or if a particular subexpression in \fIexp\fR does not match the string (e.g. because it was in a portion of the expression that was not matched), then the corresponding \fIsubMatchVar\fR will be set to "**-1 -1**" if \fB-indices\fR has been specified or to an empty string otherwise.

# Examples

Find the first occurrence of a word starting with \fBfoo\fR in a string that is not actually an instance of \fBfoobar\fR, and get the letters following it up to the end of the word into a variable:

```
regexp {\mfoo(?!bar\M)(\w*)} $string \-> restOfWord
```

Note that the whole matched substring has been placed in the variable "**->**", which is a name chosen to look nice given that we are not actually interested in its contents.

Find the index of the word \fBbadger\fR (in any case) within a string and store that in the variable \fBlocation\fR:

```
regexp -indices {(?i)\mbadger\M} $string location
```

This could also be written as a \fIbasic\fR regular expression (as opposed to using the default syntax of \fIadvanced\fR regular expressions) match by prefixing the expression with a suitable flag:

```
regexp -indices {(?ib)\<badger\>} $string location
```

This counts the number of octal digits in a string:

```
regexp -all {[0-7]} $string
```

This lists all words (consisting of all sequences of non-whitespace characters) in a string, and is useful as a more powerful version of the \fBsplit\fR command:

```
regexp -all -inline {\S+} $string
```

