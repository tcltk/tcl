---
CommandName: encoding
ManualSection: n
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - Tcl_GetEncoding(3)
 - fconfigure(n)
Keywords:
 - encoding
 - unicode
Copyright:
 - Copyright (c) 1998 Scriptics Corporation.
---

# Name

encoding - Manipulate encodings

# Synopsis

::: {.synopsis} :::
[encoding]{.cmd} [option]{.arg} [arg arg]{.optdot}
:::

# Introduction

Strings in Tcl are logically a sequence of Unicode characters. These strings are represented in memory as a sequence of bytes that may be in one of several encodings: modified UTF-8 (which uses 1 to 4 bytes per character), or a custom encoding start as 8 bit binary data.

Different operating system interfaces or applications may generate strings in other encodings such as Shift-JIS.  The **encoding** command helps to bridge the gap between Unicode and these other formats.

# Description

Performs one of several encoding related operations, depending on *option*.  The legal *option*s are:

**encoding convertfrom** ?*encoding*? *data*
: ...see next...

**encoding convertfrom** ?**-profile** *profile*? ?**-failindex var**? *encoding data*
: Converts *data*, which should be in binary string encoded as per *encoding*, to a Tcl string. If *encoding* is not specified, the current system encoding is used.


::: {.info version="TIP607, TIP656"}
The **-profile** option determines the command behavior in the presence of conversion errors. See the **PROFILES** section below for details. Any premature termination of processing due to errors is reported through an exception if the **-failindex** option is not specified.
:::

If the **-failindex** is specified, instead of an exception being raised on premature termination, the result of the conversion up to the point of the error is returned as the result of the command. In addition, the index of the source byte triggering the error is stored in **var**. If no errors are encountered, the entire result of the conversion is returned and the value **-1** is stored in **var**.

**encoding convertto** ?*encoding*? *data*
: ...see next...

**encoding convertto** ?**-profile** *profile*? ?**-failindex var**? *encoding data*
: Convert *string* to the specified *encoding*. The result is a Tcl binary string that contains the sequence of bytes representing the converted string in the specified encoding. If *encoding* is not specified, the current system encoding is used.


::: {.info version="TIP607, TIP656"}
The **-profile** and **-failindex** options have the same effect as described for the **encoding convertfrom** command.
:::

**encoding dirs** ?*directoryList*?
: Tcl can load encoding data files from the file system that describe additional encodings for it to work with. This command sets the search path for ***.enc** encoding data files to the list of directories *directoryList*. If *directoryList* is omitted then the command returns the current list of directories that make up the search path. It is an error for *directoryList* to not be a valid list. If, when a search for an encoding data file is happening, an element in *directoryList* does not refer to a readable, searchable directory, that element is ignored.

**encoding names**
: Returns a list containing the names of all of the encodings that are currently available. The encodings "utf-8" and "iso8859-1" are guaranteed to be present in the list.

**encoding profiles**
: Returns a list of the names of encoding profiles. See **PROFILES** below.

**encoding system** ?*encoding*?
: Set the system encoding to *encoding*. If *encoding* is omitted then the command returns the current system encoding.  The system encoding is used whenever Tcl passes strings to system calls.

**encoding user**
: Returns the name of encoding as per the user's preferences. On Windows systems, this is based on the user's code page settings in the registry. On other platforms, the returned value is the same as returned by **encoding system**.


# Profiles

::: {.info version="TIP656"}
Operations involving encoding transforms may encounter several types of errors such as invalid sequences in the source data, characters that cannot be encoded in the target encoding and so on. A *profile* prescribes the strategy for dealing with such errors in one of two ways:
:::

- Terminating further processing of the source data. The profile does not determine how this premature termination is conveyed to the caller. By default, this is signalled by raising an exception. If the **-failindex** option is specified, errors are reported through that mechanism.

- Continue further processing of the source data using a fallback strategy such as replacing or discarding the offending bytes in a profile-defined manner.


The following profiles are currently implemented with **strict** being the default if the **-profile** is not specified.

**strict**
: The **strict** profile always stops processing when an conversion error is encountered. The error is signalled via an exception or the **-failindex** option mechanism. The **strict** profile implements a Unicode standard conformant behavior.

**tcl8**
: The **tcl8** profile always follows the first strategy above and corresponds to the behavior of encoding transforms in Tcl 8.6. When converting from an external encoding **other than utf-8** to Tcl strings with the **encoding convertfrom** command, invalid bytes are mapped to their numerically equivalent code points. For example, the byte 0x80 which is invalid in ASCII would be mapped to code point U+0080. When converting from **utf-8**, invalid bytes that are defined in CP1252 are mapped to their Unicode equivalents while those that are not fall back to the numerical equivalents. For example, byte 0x80 is defined by CP1252 and is therefore mapped to its Unicode equivalent U+20AC while byte 0x81 which is not defined by CP1252 is mapped to U+0081. As an additional special case, the sequence 0xC0 0x80 is mapped to U+0000.  When converting from Tcl strings to an external encoding format using **encoding convertto**, characters that cannot be represented in the target encoding are replaced by an encoding-dependent character, usually the question mark **?**.

**replace**
: Like the **tcl8** profile, the **replace** profile always continues processing on conversion errors but follows a Unicode standard conformant method for substitution of invalid source data.  When converting an encoded byte sequence to a Tcl string using **encoding convertfrom**, invalid bytes are replaced by the U+FFFD REPLACEMENT CHARACTER code point.  When encoding a Tcl string with **encoding convertto**, code points that cannot be represented in the target encoding are transformed to an encoding-specific fallback character, U+FFFD REPLACEMENT CHARACTER for UTF targets and generally `?` for other encodings.


# Examples

These examples use the utility proc below that prints the Unicode code points comprising a Tcl string.

```
proc codepoints s {join [lmap c [split $s {}] {
    string cat U+ [format %.6X [scan $c %c]]}]
}
```

Example 1: convert a byte sequence in Japanese euc-jp encoding to a TCL string:

```
% codepoints [encoding convertfrom euc-jp "\xA4\xCF"]
U+00306F
```

The result is the unicode codepoint "\u306F", which is the Hiragana letter HA.

Example 2: Error handling based on profiles:

The letter **A** is Unicode character U+0041 and the byte "\x80" is invalid in ASCII encoding.

```
% codepoints [encoding convertfrom -profile tcl8 ascii A\x80]
U+000041 U+000080
% codepoints [encoding convertfrom -profile replace ascii A\x80]
U+000041 U+00FFFD
% codepoints [encoding convertfrom -profile strict ascii A\x80]
unexpected byte sequence starting at index 1: '\x80'
```

Example 3: Get partial data and the error location:

```
% codepoints [encoding convertfrom -profile strict -failindex idx ascii AB\x80]
U+000041 U+000042
% set idx
2
```

Example 4: Encode a character that is not representable in ISO8859-1:

```
% encoding convertto iso8859-1 A\u0141
A?
% encoding convertto -profile strict iso8859-1 A\u0141
unexpected character at index 1: 'U+000141'
% encoding convertto -profile strict -failindex idx iso8859-1 A\u0141
A
% set idx
1
```

