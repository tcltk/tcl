---
CommandName: Tcl_RegExpMatch
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - re_syntax(n)
Keywords:
 - match
 - pattern
 - regular expression
 - string
 - subexpression
 - Tcl_RegExpIndices
 - Tcl_RegExpInfo
Copyright:
 - Copyright (c) 1994 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
 - Copyright (c) 1998-1999 Scriptics Corporation
---

# Name

Tcl\_RegExpMatch, Tcl\_RegExpCompile, Tcl\_RegExpExec, Tcl\_RegExpRange, Tcl\_GetRegExpFromObj, Tcl\_RegExpMatchObj, Tcl\_RegExpExecObj, Tcl\_RegExpGetInfo - Pattern matching with regular expressions

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_RegExpMatchObj]{.ccmd}[interp=, +textObj=, +patObj]{.cargs}
[int]{.ret} [Tcl\_RegExpMatch]{.ccmd}[interp=, +text=, +pattern]{.cargs}
[Tcl\_RegExp]{.ret} [Tcl\_RegExpCompile]{.ccmd}[interp=, +pattern]{.cargs}
[int]{.ret} [Tcl\_RegExpExec]{.ccmd}[interp=, +regexp=, +text=, +start]{.cargs}
[Tcl\_RegExpRange]{.ccmd}[regexp=, +index=, +startPtr=, +endPtr]{.cargs}
[Tcl\_RegExp]{.ret} [Tcl\_GetRegExpFromObj]{.ccmd}[interp=, +patObj=, +cflags]{.cargs}
[int]{.ret} [Tcl\_RegExpExecObj]{.ccmd}[interp=, +regexp=, +textObj=, +offset=, +nmatches=, +eflags]{.cargs}
[Tcl\_RegExpGetInfo]{.ccmd}[regexp=, +infoPtr]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Tcl interpreter to use for error reporting.  The interpreter may be NULL if no error reporting is desired. .AP Tcl\_Obj \*textObj in/out Refers to the value from which to get the text to search.  The internal representation of the value may be converted to a form that can be efficiently searched. .AP Tcl\_Obj \*patObj in/out Refers to the value from which to get a regular expression. The compiled regular expression is cached in the value. .AP "const char" \*text in Text to search for a match with a regular expression. .AP "const char" \*pattern in String in the form of a regular expression pattern. .AP Tcl\_RegExp regexp in Compiled regular expression.  Must have been returned previously by **Tcl\_GetRegExpFromObj** or **Tcl\_RegExpCompile**. .AP "const char" \*start in If *text* is just a portion of some other string, this argument identifies the beginning of the larger string. If it is not the same as *text*, then no "**^**" matches will be allowed. .AP Tcl\_Size index in Specifies which range is desired:  0 means the range of the entire match, 1 or greater means the range that matched a parenthesized sub-expression. .AP "const char" \*\*startPtr out The address of the first character in the range is stored here, or NULL if there is no such range. .AP "const char" \*\*endPtr out The address of the character just after the last one in the range is stored here, or NULL if there is no such range. .AP int cflags in OR-ed combination of the compilation flags **TCL\_REG\_ADVANCED**, **TCL\_REG\_EXTENDED**, **TCL\_REG\_BASIC**, **TCL\_REG\_EXPANDED**, **TCL\_REG\_QUOTE**, **TCL\_REG\_NOCASE**, **TCL\_REG\_NEWLINE**, **TCL\_REG\_NLSTOP**, **TCL\_REG\_NLANCH**, **TCL\_REG\_NOSUB**, and **TCL\_REG\_CANMATCH**. See below for more information. .AP Tcl\_Size offset in The character offset into the text where matching should begin. The value of the offset has no impact on **^** matches.  This behavior is controlled by *eflags*. .AP Tcl\_Size nmatches in The number of matching subexpressions that should be remembered for later use.  If this value is 0, then no subexpression match information will be computed.  If the value is negative, then all of the matching subexpressions will be remembered.  Any other value will be taken as the maximum number of subexpressions to remember. .AP int eflags in OR-ed combination of the execution flags **TCL\_REG\_NOTBOL** and **TCL\_REG\_NOTEOL**. See below for more information. .AP Tcl\_RegExpInfo \*infoPtr out The address of the location where information about a previous match should be stored by **Tcl\_RegExpGetInfo**.

# Description

**Tcl\_RegExpMatch** determines whether its *pattern* argument matches *regexp*, where *regexp* is interpreted as a regular expression using the rules in the **re\_syntax** reference page. If there is a match then **Tcl\_RegExpMatch** returns 1. If there is no match then **Tcl\_RegExpMatch** returns 0. If an error occurs in the matching process (e.g. *pattern* is not a valid regular expression) then **Tcl\_RegExpMatch** returns -1 and leaves an error message in the interpreter result. **Tcl\_RegExpMatchObj** is similar to **Tcl\_RegExpMatch** except it operates on the Tcl values *textObj* and *patObj* instead of UTF strings. **Tcl\_RegExpMatchObj** is generally more efficient than **Tcl\_RegExpMatch**, so it is the preferred interface.

**Tcl\_RegExpCompile**, **Tcl\_RegExpExec**, and **Tcl\_RegExpRange** provide lower-level access to the regular expression pattern matcher. **Tcl\_RegExpCompile** compiles a regular expression string into the internal form used for efficient pattern matching. The return value is a token for this compiled form, which can be used in subsequent calls to **Tcl\_RegExpExec** or **Tcl\_RegExpRange**. If an error occurs while compiling the regular expression then **Tcl\_RegExpCompile** returns NULL and leaves an error message in the interpreter result. Note that the return value from **Tcl\_RegExpCompile** is only valid up to the next call to **Tcl\_RegExpCompile**;  it is not safe to retain these values for long periods of time.

**Tcl\_RegExpExec** executes the regular expression pattern matcher. It returns 1 if *text* contains a range of characters that match *regexp*, 0 if no match is found, and -1 if an error occurs. In the case of an error, **Tcl\_RegExpExec** leaves an error message in the interpreter result. When searching a string for multiple matches of a pattern, it is important to distinguish between the start of the original string and the start of the current search. For example, when searching for the second occurrence of a match, the *text* argument might point to the character just after the first match;  however, it is important for the pattern matcher to know that this is not the start of the entire string, so that it does not allow "**^**" atoms in the pattern to match. The *start* argument provides this information by pointing to the start of the overall string containing *text*. *Start* will be less than or equal to *text*;  if it is less than *text* then no **^** matches will be allowed.

**Tcl\_RegExpRange** may be invoked after **Tcl\_RegExpExec** returns;  it provides detailed information about what ranges of the string matched what parts of the pattern. **Tcl\_RegExpRange** returns a pair of pointers in *\*startPtr* and *\*endPtr* that identify a range of characters in the source string for the most recent call to **Tcl\_RegExpExec**. *Index* indicates which of several ranges is desired: if *index* is 0, information is returned about the overall range of characters that matched the entire pattern;  otherwise, information is returned about the range of characters that matched the *index*'th parenthesized subexpression within the pattern. If there is no range corresponding to *index* then NULL is stored in *\*startPtr* and *\*endPtr*.

**Tcl\_GetRegExpFromObj**, **Tcl\_RegExpExecObj**, and **Tcl\_RegExpGetInfo** are value interfaces that provide the most direct control of Henry Spencer's regular expression library.  For users that need to modify compilation and execution options directly, it is recommended that you use these interfaces instead of calling the internal regexp functions.  These interfaces handle the details of UTF to Unicode translations as well as providing improved performance through caching in the pattern and string values.

**Tcl\_GetRegExpFromObj** attempts to return a compiled regular expression from the *patObj*.  If the value does not already contain a compiled regular expression it will attempt to create one from the string in the value and assign it to the internal representation of the *patObj*.  The return value of this function is of type **Tcl\_RegExp**.  The return value is a token for this compiled form, which can be used in subsequent calls to **Tcl\_RegExpExecObj** or **Tcl\_RegExpGetInfo**.  If an error occurs while compiling the regular expression then **Tcl\_GetRegExpFromObj** returns NULL and leaves an error message in the interpreter result.  The regular expression token can be used as long as the internal representation of *patObj* refers to the compiled form.  The *cflags* argument is a bit-wise OR of zero or more of the following flags that control the compilation of *patObj*:

**TCL\_REG\_ADVANCED**
: Compile advanced regular expressions ("ARE"s). This mode corresponds to the normal regular expression syntax accepted by the Tcl [regexp] and [regsub] commands.

**TCL\_REG\_EXTENDED**
: Compile extended regular expressions ("ERE"s). This mode corresponds to the regular expression syntax recognized by Tcl 8.0 and earlier versions.

**TCL\_REG\_BASIC**
: Compile basic regular expressions ("BRE"s). This mode corresponds to the regular expression syntax recognized by common Unix utilities like **sed** and **grep**.  This is the default if no flags are specified.

**TCL\_REG\_EXPANDED**
: Compile the regular expression (basic, extended, or advanced) using an expanded syntax that allows comments and whitespace.  This mode causes non-backslashed non-bracket-expression white space and #-to-end-of-line comments to be ignored.

**TCL\_REG\_QUOTE**
: Compile a literal string, with all characters treated as ordinary characters.

**TCL\_REG\_NOCASE**
: Compile for matching that ignores upper/lower case distinctions.

**TCL\_REG\_NEWLINE**
: Compile for newline-sensitive matching.  By default, newline is a completely ordinary character with no special meaning in either regular expressions or strings.  With this flag, "[^" bracket expressions and "." never match newline, "^" matches an empty string after any newline in addition to its normal function, and "$" matches an empty string before any newline in addition to its normal function. **REG\_NEWLINE** is the bit-wise OR of **REG\_NLSTOP** and **REG\_NLANCH**.

**TCL\_REG\_NLSTOP**
: Compile for partial newline-sensitive matching, with the behavior of "[^" bracket expressions and "." affected, but not the behavior of "^" and "$". In this mode, "[^" bracket expressions and "." never match newline.

**TCL\_REG\_NLANCH**
: Compile for inverse partial newline-sensitive matching, with the behavior of "^" and "$" (the "anchors") affected, but not the behavior of "[^" bracket expressions and ".". In this mode "^" matches an empty string after any newline in addition to its normal function, and "$" matches an empty string before any newline in addition to its normal function.

**TCL\_REG\_NOSUB**
: Compile for matching that reports only success or failure, not what was matched.  This reduces compile overhead and may improve performance.  Subsequent calls to **Tcl\_RegExpGetInfo** or **Tcl\_RegExpRange** will not report any match information.

**TCL\_REG\_CANMATCH**
: Compile for matching that reports the potential to complete a partial match given more text (see below).


Only one of **TCL\_REG\_EXTENDED**, **TCL\_REG\_ADVANCED**, **TCL\_REG\_BASIC**, and **TCL\_REG\_QUOTE** may be specified.

**Tcl\_RegExpExecObj** executes the regular expression pattern matcher.  It returns 1 if *objPtr* contains a range of characters that match *regexp*, 0 if no match is found, and -1 if an error occurs.  In the case of an error, **Tcl\_RegExpExecObj** leaves an error message in the interpreter result.  The *nmatches* value indicates to the matcher how many subexpressions are of interest.  If *nmatches* is 0, then no subexpression match information is recorded, which may allow the matcher to make various optimizations. If the value is -1, then all of the subexpressions in the pattern are remembered.  If the value is a positive integer, then only that number of subexpressions will be remembered.  Matching begins at the specified Unicode character index given by *offset*.  Unlike **Tcl\_RegExpExec**, the behavior of anchors is not affected by the offset value.  Instead the behavior of the anchors is explicitly controlled by the *eflags* argument, which is a bit-wise OR of zero or more of the following flags:

**TCL\_REG\_NOTBOL**
: The starting character will not be treated as the beginning of a line or the beginning of the string, so "^" will not match there. Note that this flag has no effect on how "**\\A**" matches.

**TCL\_REG\_NOTEOL**
: The last character in the string will not be treated as the end of a line or the end of the string, so "$" will not match there. Note that this flag has no effect on how "**\\Z**" matches.


**Tcl\_RegExpGetInfo** retrieves information about the last match performed with a given regular expression *regexp*.  The *infoPtr* argument contains a pointer to a structure that is defined as follows:

```
typedef struct {
    Tcl_Size nsubs;
    Tcl_RegExpIndices *matches;
    Tcl_Size extendStart;
} Tcl_RegExpInfo;
```

The *nsubs* field contains a count of the number of parenthesized subexpressions within the regular expression.  If the **TCL\_REG\_NOSUB** was used, then this value will be zero.  The *matches* field points to an array of *nsubs*+1 values that indicate the bounds of each subexpression matched.  The first element in the array refers to the range matched by the entire regular expression, and subsequent elements refer to the parenthesized subexpressions in the order that they appear in the pattern.  Each element is a structure that is defined as follows:

```
typedef struct {
    Tcl_Size start;
    Tcl_Size end;
} Tcl_RegExpIndices;
```

The *start* and *end* values are Unicode character indices relative to the offset location within *objPtr* where matching began. The *start* index identifies the first character of the matched subexpression.  The *end* index identifies the first character after the matched subexpression.  If the subexpression matched the empty string, then *start* and *end* will be equal.  If the subexpression did not participate in the match, then *start* and *end* will be set to -1.

The *extendStart* field in **Tcl\_RegExpInfo** is only set if the **TCL\_REG\_CANMATCH** flag was used.  It indicates the first character in the string where a match could occur.  If a match was found, this will be the same as the beginning of the current match. If no match was found, then it indicates the earliest point at which a match might occur if additional text is appended to the string.  If it is no match is possible even with further text, this field will be set to -1.

# Reference count management

The *textObj* and *patObj* arguments to **Tcl\_RegExpMatchObj** must have reference counts of at least 1.  Note however that this function may set the interpreter result; neither argument should be the direct interpreter result without an additional reference being taken.

The *patObj* argument to **Tcl\_GetRegExpFromObj** must have a reference count of at least 1.  Note however that this function may set the interpreter result; the argument should not be the direct interpreter result without an additional reference being taken.

The *textObj* argument to **Tcl\_RegExpExecObj** must have a reference count of at least 1.  Note however that this function may set the interpreter result; the argument should not be the direct interpreter result without an additional reference being taken.


[regexp]: regexp.md
[regsub]: regsub.md

