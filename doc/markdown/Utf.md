---
CommandName: Utf
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - utf
 - unicode
 - backslash
Copyright:
 - Copyright (c) 1997 Sun Microsystems, Inc.
---

# Name

Tcl\_UniChar, Tcl\_UniCharToUtf, Tcl\_UtfToUniChar, Tcl\_UtfToChar16, Tcl\_UtfToWChar, Tcl\_UniCharToUtfDString, Tcl\_UtfToUniCharDString, Tcl\_Char16ToUtfDString, Tcl\_UtfToWCharDString, Tcl\_UtfToChar16DString, Tcl\_WCharToUtfDString, Tcl\_WCharLen, Tcl\_Char16Len, Tcl\_UniCharLen, Tcl\_UniCharNcmp, Tcl\_UniCharNcasecmp, Tcl\_UniCharCaseMatch, Tcl\_UtfNcmp, Tcl\_UtfNcasecmp, Tcl\_UtfCharComplete, Tcl\_NumUtfChars, Tcl\_UtfFindFirst, Tcl\_UtfFindLast, Tcl\_UtfNext, Tcl\_UtfPrev, Tcl\_UniCharAtIndex, Tcl\_UtfAtIndex, Tcl\_UtfBackslash - routines for manipulating TUTF-8 encoded byte sequences

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Size]{.ret} [Tcl\_UniCharToUtf]{.ccmd}[ch, buf]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_UtfToUniChar]{.ccmd}[src, chPtr]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_UtfToChar16]{.ccmd}[src, uPtr]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_UtfToWChar]{.ccmd}[src, wPtr]{.cargs}
[char \*]{.ret} [Tcl\_UniCharToUtfDString]{.ccmd}[uniStr, numUniChars, dsPtr]{.cargs}
[char \*]{.ret} [Tcl\_Char16ToUtfDString]{.ccmd}[utf16, numUtf16, dsPtr]{.cargs}
[char \*]{.ret} [Tcl\_WCharToUtfDString]{.ccmd}[wcharStr, numWChars, dsPtr]{.cargs}
[Tcl\_UniChar \*]{.ret} [Tcl\_UtfToUniCharDString]{.ccmd}[src, numBytes, dsPtr]{.cargs}
[unsigned short \*]{.ret} [Tcl\_UtfToChar16DString]{.ccmd}[src, numBytes, dsPtr]{.cargs}
[wchar\_t \*]{.ret} [Tcl\_UtfToWCharDString]{.ccmd}[src, numBytes, dsPtr]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_Char16Len]{.ccmd}[utf16]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_WCharLen]{.ccmd}[wcharStr]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_UniCharLen]{.ccmd}[uniStr]{.cargs}
[int]{.ret} [Tcl\_UniCharNcmp]{.ccmd}[ucs, uct, uniLength]{.cargs}
[int]{.ret} [Tcl\_UniCharNcasecmp]{.ccmd}[ucs, uct, uniLength]{.cargs}
[int]{.ret} [Tcl\_UniCharCaseMatch]{.ccmd}[uniStr, uniPattern, nocase]{.cargs}
[int]{.ret} [Tcl\_UtfNcmp]{.ccmd}[cs, ct, length]{.cargs}
[int]{.ret} [Tcl\_UtfNcasecmp]{.ccmd}[cs, ct, length]{.cargs}
[int]{.ret} [Tcl\_UtfCharComplete]{.ccmd}[src, numBytes]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_NumUtfChars]{.ccmd}[src, numBytes]{.cargs}
[const char \*]{.ret} [Tcl\_UtfFindFirst]{.ccmd}[src, ch]{.cargs}
[const char \*]{.ret} [Tcl\_UtfFindLast]{.ccmd}[src, ch]{.cargs}
[const char \*]{.ret} [Tcl\_UtfNext]{.ccmd}[src]{.cargs}
[const char \*]{.ret} [Tcl\_UtfPrev]{.ccmd}[src, start]{.cargs}
[int]{.ret} [Tcl\_UniCharAtIndex]{.ccmd}[src, index]{.cargs}
[const char \*]{.ret} [Tcl\_UtfAtIndex]{.ccmd}[src, index]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_UtfBackslash]{.ccmd}[src, readPtr, dst]{.cargs}
:::

# Arguments

.AP char \*buf out Buffer in which the TUTF-8 representation of the Tcl\_UniChar is stored.  At most **TCL\_UTF\_MAX** bytes are stored in the buffer. .AP int ch in The Unicode character to be converted or examined. .AP Tcl\_UniChar \*chPtr out Filled with the Tcl\_UniChar represented by the head of the TUTF-8 byte sequence. .AP unsigned short \*uPtr out Filled with the utf-16 represented by the head of the TUTF-8 byte sequence. .AP wchar\_t \*wPtr out Filled with the wchar\_t represented by the head of the TUTF-8 byte sequence. .AP "const char" \*src in Pointer to a TUTF-8 byte sequence. .AP "const char" \*cs in Pointer to a TUTF-8 byte sequence. .AP "const char" \*ct in Pointer to a TUTF-8 byte sequence. .AP "const Tcl\_UniChar" \*uniStr in A sequence of **Tcl\_UniChar** units with null-termination optional depending on function. .AP "const Tcl\_UniChar" \*ucs in A null-terminated sequence of **Tcl\_UniChar**. .AP "const Tcl\_UniChar" \*uct in A null-terminated sequence of **Tcl\_UniChar**. .AP "const Tcl\_UniChar" \*uniPattern in A null-terminated sequence of **Tcl\_UniChar**. .AP "const unsigned short" \*utf16 in A sequence of UTF-16 units with null-termination optional depending on function. .AP "const wchar\_t" \*wcharStr in A sequence of **wchar\_t** units with null-termination optional depending on function. .AP int numBytes in The length of the TUTF-8 input in bytes.  If negative, the length includes all bytes until the first null byte. .AP int numUtf16 in The length of the input in UTF-16 units. If negative, the length includes all bytes until the first null. .AP int numUniChars in The length of the input in Tcl\_UniChar units. If negative, the length includes all bytes until the first null. .AP int numWChars in The length of the input in wchar\_t units. If negative, the length includes all bytes until the first null. .AP "Tcl\_DString" \*dsPtr in/out A pointer to a previously initialized **Tcl\_DString**. .AP "const char" \*start in Pointer to the beginning of a TUTF-8 byte sequence. .AP Tcl\_Size index in The index of a character (not byte) in the TUTF-8 byte sequence. .AP int \*readPtr out If non-NULL, filled with the number of bytes in the backslash sequence, including the backslash character. .AP char \*dst out Buffer in which the bytes represented by the backslash sequence are stored. At most **TCL\_UTF\_MAX** bytes are stored in the buffer. .AP int nocase in Specifies whether the match should be done case-sensitive (0) or case-insensitive (1). 

# Description

N.B. The use of the term *TUTF-8* in this documentation refers to the modified UTF-8 encoding used internally by Tcl. This differs from the UTF-8 encoding defined in the Unicode standard with respect to the encoding of the NUL character U+0000 which Tcl encodes internally as the byte sequence 0xC0 0x80 and not a single 0x00 byte as per the standard. The term *TUTF-8 byte sequence* refers to a byte sequence representing one or more Unicode code points encoded in TUTF-8. The term *character* refers to a Unicode code point and is used interchangeably with it.

The routines described here convert between TUTF-8 encoded byte sequences and other representation forms.

The **Tcl\_UniChar** type is an C integer type wide enough to hold a single Unicode code point value. A TUTF-8 byte sequence encoding a single code point may have a maximum length of 4 bytes, defined as the C preprocessor symbol **TCL\_UTF\_MAX**. This is also the maximum number of bytes that **Tcl\_UtfToUniChar** can consume in a single call.

**Tcl\_UniCharToUtf** encodes the character *ch* as a TUTF-8 byte sequence, storing it starting at *buf*.  The return value is the number of bytes stored in *buf*. The character *ch* can be or'ed with the value TCL\_COMBINE to enable special behavior, compatible with Tcl 8.x. Then, if ch is a high surrogate (range U+D800 - U+DBFF), the return value will be 1 and a single byte in the range 0xF0 - 0xF4 will be stored. If *ch* is a low surrogate (range U+DC00 - U+DFFF), an attempt is made to combine the result with the earlier produced bytes, resulting in a 4-byte TUTF-8 byte sequence.

**Tcl\_UtfToUniChar** reads a TUTF-8 byte sequence starting at *src* and encoding a single code point, and stores it as a Tcl\_UniChar in *\*chPtr*.  The return value is the number of bytes read from *src*.  The caller must ensure that the source buffer is long enough such that this routine does not run off the end and dereference non-existent or random memory; if the source buffer is known to be null-terminated, this will not happen.  If the input starts with a byte in the range 0x80 - 0x9F, **Tcl\_UtfToUniChar** assumes the cp1252 encoding, stores the corresponding Tcl\_UniChar in *\*chPtr* and returns 1. If the input is otherwise not in proper TUTF-8 format, **Tcl\_UtfToUniChar** will store the first byte of *src* in *\*chPtr* as a Tcl\_UniChar between 0x00A0 and 0x00FF and return 1.

**Tcl\_UniCharToUtfDString** converts the input in the form of a sequence of **Tcl\_UniChar** code points to TUTF-8, appending the result to the previously initialized output **Tcl\_DString**. The return value is a pointer to the TUTF-8 encoded representation of the **appended** string.

**Tcl\_UtfToUniCharDString** converts the input in the form of a TUTF-8 byte sequence to a **Tcl\_UniChar** sequence appending the result in the previously initialized **Tcl\_DString**. The return value is a pointer to the appended result which is also terminated with a **Tcl\_UniChar** NUL character.

**Tcl\_WCharToUtfDString** and **Tcl\_UtfToWCharDString** are similar to **Tcl\_UniCharToUtfDString** and **Tcl\_UtfToUniCharDString** except they operate on sequences of **wchar\_t** instead of **Tcl\_UniChar**.

**Tcl\_Char16ToUtfDString** and **Tcl\_UtfToChar16DString** are similar to **Tcl\_UniCharToUtfDString** and **Tcl\_UtfToUniCharDString** except they operate on sequences of **UTF-16** units instead of **Tcl\_UniChar**.

**Tcl\_Char16Len** corresponds to **strlen** for UTF-16 characters.  It accepts a null-terminated UTF-16 sequence and returns the number of UTF-16 units until the null.

**Tcl\_WCharLen** corresponds to **strlen** for **wchar\_t** characters.  It accepts a null-terminated **wchar\_t** sequence and returns the number of **wchar\_t** units until the null.

**Tcl\_UniCharLen** corresponds to **strlen** for Tcl\_UniChar characters.  It accepts a null-terminated Tcl\_UniChar string and returns the number of Tcl\_UniChar's (not bytes) in that string.

**Tcl\_UniCharNcmp** and **Tcl\_UniCharNcasecmp** correspond to **strncmp** and **strncasecmp**, respectively, for Tcl\_UniChar code points. They accept two null-terminated Tcl\_UniChar strings and the number of Tcl\_UniChar code points to compare. Both strings are assumed to be at least *uniLength* characters long. **Tcl\_UniCharNcmp** compares the code points in two strings in order according to the Unicode character ordering. It returns an integer greater than, equal to, or less than 0 if the first string is greater than, equal to, or less than the second string respectively. **Tcl\_UniCharNcasecmp** is the case insensitive variant of **Tcl\_UniCharNcmp**.

**Tcl\_UniCharCaseMatch** is the Unicode equivalent to **Tcl\_StringCaseMatch**.  It accepts a null-terminated Tcl\_UniChar string, a Tcl\_UniChar pattern, a boolean value specifying whether the match should be case sensitive and returns whether the string matches the pattern.

**Tcl\_UtfNcmp** corresponds to **strncmp** and accepts two null-terminated TUTF-8 encoded strings each of which should represent a sequence of at least *length* code points. **Tcl\_UtfNcmp** compares the code points represented by each of the encoded strings in order. It returns an integer greater than, equal to, or less than 0 if the first string is greater than, equal to, or less than the second string respectively.

**Tcl\_UtfNcasecmp** corresponds to **strncasecmp** for TUTF-8 encoded strings.  It is similar to **Tcl\_UtfNcmp** except comparisons ignore differences in case when comparing upper, lower or title case characters.

**Tcl\_UtfCharComplete** returns 1 if the source TUTF-8 byte sequence *src* of *numBytes* bytes is long enough to be decoded by **Tcl\_UtfToUniChar**/**Tcl\_UtfNext**, or 0 otherwise.  This function does not guarantee that the TUTF-8 byte sequence is properly formed.  This routine is used by procedures that are operating on a byte at a time and need to know if a full Unicode character has been seen.

**Tcl\_NumUtfChars** corresponds to **strlen** for TUTF-8 byte sequences. It returns the number of Tcl\_UniChars that are represented by the TUTF-8 byte sequence *src*. The length of the source string is *length* bytes. If the length is negative, all bytes up to the first null byte are used.

**Tcl\_UtfFindFirst** corresponds to **strchr** for TUTF-8 byte sequences. It returns a pointer to the first occurrence of the Unicode character *ch* in the null-terminated TUTF-8 byte sequence *src*. The null terminator is considered part of the byte sequence.

**Tcl\_UtfFindLast** corresponds to **strrchr** for TUTF-8 byte sequences. It returns a pointer to the last occurrence of the Unicode character *ch* in the null-terminated TUTF-8 byte sequence *src*. The null terminator is considered part of the byte sequence.

Given *src*, a pointer to some location in a TUTF-8 byte sequence, **Tcl\_UtfNext** returns a pointer to the start of the TUTF-8 byte sequence corresponding to the next character The caller must not ask for the next character after the last character in the string if the string is not terminated by a null character. **Tcl\_UtfCharComplete** can be used in that case to make sure enough bytes are available before calling **Tcl\_UtfNext**.

**Tcl\_UtfPrev** is used to step backward through but not beyond the TUTF-8 byte sequence that begins at *start*. If the byte sequence is made up entirely of complete and well-formed characters, and *src* points to the lead byte of one of those characters (or to the location one byte past the end of the string), then repeated calls of **Tcl\_UtfPrev** will return pointers to the lead bytes of each character in the string, one character at a time, terminating when it returns *start*.

When the conditions of completeness and well-formedness may not be satisfied, a more precise description of the function of **Tcl\_UtfPrev** is necessary. It always returns a pointer greater than or equal to *start*; that is, always a pointer to a location in the string. It always returns a pointer to a byte that begins a character when scanning for characters beginning from *start*. When *src* is greater than *start*, it always returns a pointer less than *src* and greater than or equal to (*src* - 4).  The character that begins at the returned pointer is the first one that either includes the byte *src[-1]*, or might include it if the right trail bytes are present at *src* and greater. **Tcl\_UtfPrev** never reads the byte *src[0]* nor the byte *start[-1]* nor the byte *src[-5]*.

**Tcl\_UniCharAtIndex** corresponds to a C string array dereference or the Pascal Ord() function.  It returns the Unicode code point represented at the specified character (not byte) *index* in the TUTF-8 byte sequence *src*.  The source string must contain at least *index* characters.  If *index* is negative it returns -1.

**Tcl\_UtfAtIndex** returns a pointer to the specified character (not byte) *index* in the TUTF-8 byte sequence *src*.  The source must contain at least *index* characters.  This is equivalent to calling **Tcl\_UtfToUniChar** *index* times.  If *index* is negative, the return pointer points to the first character in the source.

**Tcl\_UtfBackslash** is a utility procedure used by several of the Tcl commands.  It parses a backslash sequence and stores the properly formed TUTF-8 encoding of the character represented by the backslash sequence in the output buffer *dst*.  At most **TCL\_UTF\_MAX** bytes are stored in the buffer. **Tcl\_UtfBackslash** modifies *\*readPtr* to contain the number of bytes in the backslash sequence, including the backslash character. The return value is the number of bytes stored in the output buffer. See the **Tcl** manual entry for information on the valid backslash sequences.  All of the sequences described in the Tcl manual entry are supported by **Tcl\_UtfBackslash**. 

