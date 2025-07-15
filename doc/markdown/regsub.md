---
CommandName: regsub
ManualSection: n
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - regexp(n)
 - re_syntax(n)
 - subst(n)
 - string(n)
Keywords:
 - match
 - pattern
 - quoting
 - regular expression
 - substitution
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 2000 Scriptics Corporation.
---

# Name

regsub - Perform substitutions based on regular expression pattern matching

# Synopsis

::: {.synopsis} :::
[regsub]{.cmd} [switches]{.optarg} [exp]{.arg} [string]{.arg} [subSpec]{.arg} [varName]{.optarg}
:::

# Description

This command matches the regular expression \fIexp\fR against \fIstring\fR, and either copies \fIstring\fR to the variable whose name is given by \fIvarName\fR or returns \fIstring\fR if \fIvarName\fR is not present. (Regular expression matching is described in the \fBre_syntax\fR reference page.) If there is a match, then while copying \fIstring\fR to \fIvarName\fR (or to the result of this command if \fIvarName\fR is not present) the portion of \fIstring\fR that matched \fIexp\fR is replaced with \fIsubSpec\fR. If \fIsubSpec\fR contains a "&" or "\0", then it is replaced in the substitution with the portion of \fIstring\fR that matched \fIexp\fR. If \fIsubSpec\fR contains a "\\fIn\fR", where \fIn\fR is a digit between 1 and 9, then it is replaced in the substitution with the portion of \fIstring\fR that matched the \fIn\fR'th parenthesized subexpression of \fIexp\fR. Additional backslashes may be used in \fIsubSpec\fR to prevent special interpretation of "&", "\0", "\\fIn\fR" and backslashes. The use of backslashes in \fIsubSpec\fR tends to interact badly with the Tcl parser's use of backslashes, so it is generally safest to enclose \fIsubSpec\fR in braces if it includes backslashes. .LP If the initial arguments to \fBregsub\fR start with \fB-\fR then they are treated as switches.  The following switches are currently supported:

**-all**
: All ranges in \fIstring\fR that match \fIexp\fR are found and substitution is performed for each of these ranges. Without this switch only the first matching range is found and substituted. If \fB-all\fR is specified, then "&" and "\\fIn\fR" sequences are handled for each substitution using the information from the corresponding match.

**-command**
: Changes the handling of \fIsubSpec\fR so that it is not treated as a template for a substitution string and the substrings "&" and "\\fIn\fR" no longer have special meaning. Instead \fIsubSpec\fR must be a command prefix, that is, a non-empty list.  The substring of \fIstring\fR that matches \fIexp\fR, and then each substring that matches each capturing sub-RE within \fIexp\fR are appended as additional elements to that list. (The items appended to the list are much like what \fBregexp\fR \fB-inline\fR would return).  The completed list is then evaluated as a Tcl command, and the result of that command is the substitution string.  Any error or exception from command evaluation becomes an error or exception from the \fBregsub\fR command.
    If \fB-all\fR is not also given, the command callback will be invoked at most once (exactly when the regular expression matches). If \fB-all\fR is given, the command callback will be invoked for each matched location, in sequence. The exact location indices that matched are not made available to the script.
    See \fBEXAMPLES\fR below for illustrative cases.

**-expanded**
: Enables use of the expanded regular expression syntax where whitespace and comments are ignored.  This is the same as specifying the \fB(?x)\fR embedded option (see the \fBre_syntax\fR manual page).

**-line**
: Enables newline-sensitive matching.  By default, newline is a completely ordinary character with no special meaning.  With this flag, "[^" bracket expressions and "." never match newline, "^" matches an empty string after any newline in addition to its normal function, and "$" matches an empty string before any newline in addition to its normal function.  This flag is equivalent to specifying both \fB-linestop\fR and \fB-lineanchor\fR, or the \fB(?n)\fR embedded option (see the \fBre_syntax\fR manual page).

**-linestop**
: Changes the behavior of "[^" bracket expressions and "." so that they stop at newlines.  This is the same as specifying the \fB(?p)\fR embedded option (see the \fBre_syntax\fR manual page).

**-lineanchor**
: Changes the behavior of "^" and "$" (the "anchors") so they match the beginning and end of a line respectively.  This is the same as specifying the \fB(?w)\fR embedded option (see the \fBre_syntax\fR manual page).

**-nocase**
: Upper-case characters in \fIstring\fR will be converted to lower-case before matching against \fIexp\fR;  however, substitutions specified by \fIsubSpec\fR use the original unconverted form of \fIstring\fR.

**-start** \fIindex\fR
: Specifies a character index offset into the string to start matching the regular expression at. The \fIindex\fR value is interpreted in the same manner as the \fIindex\fR argument to \fBstring index\fR. When using this switch, "^" will not match the beginning of the line, and \A will still match the start of the string at \fIindex\fR. \fIindex\fR will be constrained to the bounds of the input string.

**-\|-**
: Marks the end of switches.  The argument following this one will be treated as \fIexp\fR even if it starts with a \fB-\fR.


If \fIvarName\fR is supplied, the command returns a count of the number of matching ranges that were found and replaced, otherwise the string after replacement is returned. See the manual entry for \fBregexp\fR for details on the interpretation of regular expressions.

# Examples

Replace (in the string in variable \fIstring\fR) every instance of \fBfoo\fR which is a word by itself with \fBbar\fR:

```
regsub -all {\mfoo\M} $string bar string
```

or (using the "basic regular expression" syntax):

```
regsub -all {(?b)\<foo\>} $string bar string
```

Insert double-quotes around the first instance of the word \fBinteresting\fR, however it is capitalized.

```
regsub -nocase {\yinteresting\y} $string {"&"} string
```

Convert all non-ASCII and Tcl-significant characters into \u escape sequences by using \fBregsub\fR and \fBsubst\fR in combination:

```
# This RE is just a character class for almost everything "bad"
set RE {[][{};#\\\$ \r\t\u0080-\uffff]}

# We will substitute with a fragment of Tcl script in brackets
set substitution {[format \\\\u%04x [scan "\\&" %c]]}

# Now we apply the substitution to get a subst-string that
# will perform the computational parts of the conversion. Note
# that newline is handled specially through string map since
# backslash-newline is a special sequence.
set quoted [subst [string map {\n {\\u000a}} \
        [regsub -all $RE $string $substitution]]]
```

::: {.info -version="TIP463"}
The above operation can be done using \fBregsub -command\fR instead, which is often faster. (A full pre-computed \fBstring map\fR would be faster still, but the cost of computing the map for a transformation as complex as this can be quite large.)
:::

```
# This RE is just a character class for everything "bad"
set RE {[][{};#\\\$\s\u0080-\uffff]}

# This encodes what the RE described above matches
proc encodeChar {ch} {
    # newline is handled specially since backslash-newline is a
    # special sequence.
    if {$ch eq "\n"} {
        return "\\u000a"
    }
    # No point in writing this as a one-liner
    scan $ch %c charNumber
    format "\\u%04x" $charNumber
}

set quoted [regsub -all -command $RE $string encodeChar]
```

Decoding a URL-encoded string using \fBregsub -command\fR, a lambda term and the \fBapply\fR command.

```
# Match one of the sequences in a URL-encoded string that needs
# fixing, converting + to space and %XX to the right character
# (e.g., %7e becomes ~)
set RE {(\+)|%([0-9A-Fa-f]{2})}

# Note that -command uses a command prefix, not a command name
set decoded [regsub -all -command $RE $string {apply {{- p h} {
    # + is a special case; handle directly
    if {$p eq "+"} {
        return " "
    }
    # convert hex to a char
    scan $h %x charNumber
    format %c $charNumber
}}}]
```

The \fB-command\fR option can also be useful for restricting the range of commands such as \fBstring totitle\fR:

```
set message "the quIck broWn fOX JUmped oVer the laZy dogS..."
puts [regsub -all -command {\w+} $message {string totitle}]
# \(-> The Quick Brown Fox Jumped Over The Lazy Dogs..
```

