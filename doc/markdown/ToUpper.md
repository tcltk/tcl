---
CommandName: Tcl_UtfToUpper
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - utf
 - unicode
 - toupper
 - tolower
 - totitle
 - case
Copyright:
 - Copyright (c) 1997 Sun Microsystems, Inc.
---

# Name

Tcl\_UniCharToUpper, Tcl\_UniCharToLower, Tcl\_UniCharToTitle, Tcl\_UtfToUpper, Tcl\_UtfToLower, Tcl\_UtfToTitle - routines for manipulating the case of Unicode characters and UTF-8 strings

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_UniCharToUpper]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharToLower]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharToTitle]{.ccmd}[ch]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_UtfToUpper]{.ccmd}[str]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_UtfToLower]{.ccmd}[str]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_UtfToTitle]{.ccmd}[str]{.cargs}
:::

# Arguments

.AP int ch in The Unicode character to be converted. .AP char \*str in/out Pointer to the TUTF-8 byte sequence to be converted in place. 

# Description

N.B. Refer to the **Tcl\_UniChar** documentation page for a description of the *TUTF-8* encoding and related terms referenced here.

The first three routines convert the case of individual Unicode characters:

If *ch* represents a lower-case character, **Tcl\_UniCharToUpper** returns the corresponding upper-case character.  If no upper-case character is defined, it returns the character unchanged.

If *ch* represents an upper-case character, **Tcl\_UniCharToLower** returns the corresponding lower-case character.  If no lower-case character is defined, it returns the character unchanged.

If *ch* represents a lower-case character, **Tcl\_UniCharToTitle** returns the corresponding title-case character.  If no title-case character is defined, it returns the corresponding upper-case character.  If no upper-case character is defined, it returns the character unchanged.  Title-case is defined for a small number of characters that have a different appearance when they are at the beginning of a capitalized word.

The next three routines convert the case of null-terminated TUTF-8 byte sequences in place in memory:

**Tcl\_UtfToUpper** changes every TUTF-8 encoded character in *str* to upper-case.  Because changing the case of a character may change its size, the byte offset of each character in the resulting string may differ from its original location.  **Tcl\_UtfToUpper** writes a null byte at the end of the converted string.  **Tcl\_UtfToUpper** returns the new length of the string in bytes.  This new length is guaranteed to be no longer than the original string length.

**Tcl\_UtfToLower** is the same as **Tcl\_UtfToUpper** except it turns each character in the string into its lower-case equivalent.

**Tcl\_UtfToTitle** is the same as **Tcl\_UtfToUpper** except it turns the first character in the string into its title-case equivalent and all following characters into their lower-case equivalents. 

