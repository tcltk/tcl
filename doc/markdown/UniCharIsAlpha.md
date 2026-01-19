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

Tcl\_UniCharIsAlnum, Tcl\_UniCharIsAlpha, Tcl\_UniCharIsControl, Tcl\_UniCharIsDigit, Tcl\_UniCharIsGraph, Tcl\_UniCharIsLower, Tcl\_UniCharIsPrint, Tcl\_UniCharIsPunct, Tcl\_UniCharIsSpace, Tcl\_UniCharIsUpper, Tcl\_UniCharIsWordChar - routines for classification of Tcl\_UniChar characters

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_UniCharIsAlnum]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharIsAlpha]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharIsControl]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharIsDigit]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharIsGraph]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharIsLower]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharIsPrint]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharIsPunct]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharIsSpace]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharIsUpper]{.ccmd}[ch]{.cargs}
[int]{.ret} [Tcl\_UniCharIsWordChar]{.ccmd}[ch]{.cargs}
:::

# Arguments

.AP int ch in The Unicode character to be examined. 

# Description

All of the routines described examine Unicode characters and return a boolean value. A non-zero return value means that the character does belong to the character class associated with the called routine. The rest of this document just describes the character classes associated with the various routines. 

# Character classes

**Tcl\_UniCharIsAlnum** tests if the character is an alphanumeric Unicode character.

**Tcl\_UniCharIsAlpha** tests if the character is an alphabetic Unicode character.

**Tcl\_UniCharIsControl** tests if the character is a Unicode control character.

**Tcl\_UniCharIsDigit** tests if the character is a numeric Unicode character.

**Tcl\_UniCharIsGraph** tests if the character is any Unicode print character except space.

**Tcl\_UniCharIsLower** tests if the character is a lowercase Unicode character.

**Tcl\_UniCharIsPrint** tests if the character is a Unicode print character.

**Tcl\_UniCharIsPunct** tests if the character is a Unicode punctuation character.

**Tcl\_UniCharIsSpace** tests if the character is a whitespace Unicode character.

**Tcl\_UniCharIsUpper** tests if the character is an uppercase Unicode character.

**Tcl\_UniCharIsWordChar** tests if the character is alphanumeric or a connector punctuation mark. 

