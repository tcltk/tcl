---
CommandName: Tcl_UniCharIsAlpha
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - unicode
 - classification
Copyright:
 - Copyright (c) 1997 Sun Microsystems, Inc.
---

# Name

Tcl_UniCharIsAlnum, Tcl_UniCharIsAlpha, Tcl_UniCharIsControl, Tcl_UniCharIsDigit, Tcl_UniCharIsGraph, Tcl_UniCharIsLower, Tcl_UniCharIsPrint, Tcl_UniCharIsPunct, Tcl_UniCharIsSpace, Tcl_UniCharIsUpper, Tcl_UniCharIsWordChar - routines for classification of Tcl_UniChar characters

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_UniCharIsAlnum]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl_UniCharIsAlpha]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl_UniCharIsControl]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl_UniCharIsDigit]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl_UniCharIsGraph]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl_UniCharIsLower]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl_UniCharIsPrint]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl_UniCharIsPunct]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl_UniCharIsSpace]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl_UniCharIsUpper]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl_UniCharIsWordChar]{.ccmd}[ch]{.cargs}
:::

# Arguments

.AP int ch in The Unicode character to be examined. 

# Description

All of the routines described examine Unicode characters and return a boolean value. A non-zero return value means that the character does belong to the character class associated with the called routine. The rest of this document just describes the character classes associated with the various routines. 

# Character classes

**Tcl_UniCharIsAlnum** tests if the character is an alphanumeric Unicode character.

**Tcl_UniCharIsAlpha** tests if the character is an alphabetic Unicode character.

**Tcl_UniCharIsControl** tests if the character is a Unicode control character.

**Tcl_UniCharIsDigit** tests if the character is a numeric Unicode character.

**Tcl_UniCharIsGraph** tests if the character is any Unicode print character except space.

**Tcl_UniCharIsLower** tests if the character is a lowercase Unicode character.

**Tcl_UniCharIsPrint** tests if the character is a Unicode print character.

**Tcl_UniCharIsPunct** tests if the character is a Unicode punctuation character.

**Tcl_UniCharIsSpace** tests if the character is a whitespace Unicode character.

**Tcl_UniCharIsUpper** tests if the character is an uppercase Unicode character.

**Tcl_UniCharIsWordChar** tests if the character is alphanumeric or a connector punctuation mark. 

