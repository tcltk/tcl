---
CommandName: Tcl_UtfToNormalized
ManualSection: 3
Version: 9.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - unicode(n)
Keywords:
 - utf
 - Unicode
 - normalize
Copyright:
 - Copyright (c) 2025 Ashok P. Nadkarni
---

# Name

Tcl\_UtfToNormalized, Tcl\_UtfToNormalizedDString - procedures for Unicode normalization

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_UtfToNormalized]{.ccmd}[interp, src, numBytes, normForm, profile, dst, dstLen, dstWrotePtr]{.cargs}
[int]{.ret} [Tcl\_UtfToNormalizedDString]{.ccmd}[interp, src, numBytes, normForm, profile, dstPtr]{.cargs}
:::

# Arguments

::: {.arguments} :::

[\*interp]{.carg .in type="Tcl_Interp"}
: Interpreter to use for error reporting, or NULL if no error reporting is desired.

[\*src]{.carg .in type="const char"}
: An array of bytes in Tcl's internal UTF-8 based encoding.

[numBytes]{.carg .in type="Tcl_Size"}
: Length of *src* in bytes.  If the length is negative, the length includes all bytes until the first nul byte.

[\*dst]{.carg .out type="char"}
: Buffer in which the converted result will be stored.  No more than *dstLen* bytes will be stored in *dst*.

[dstLen]{.carg .in type="Tcl_Size"}
: The size of the output buffer *dst* in bytes.

[\*dstWrotePtr]{.carg .out type="Tcl_Size"}
: Filled with the number of bytes in the normalized string. This number does not include the terminating nul character. May be NULL.

[normForm]{.carg .in type="Tcl_UnicodeNormalizationForm"}
: Must be one of the **Tcl\_UnicodeNormalizationForm** members **TCL\_NFC**, **TCL\_NFD**, **TCL\_NFKC** or **TCL\_NFKD** specifying the Unicode normalization type.

[profile]{.carg .in/out type="int"}
: The encoding profile as described in the **Tcl\_GetEncoding** documentation. Must be either **TCL\_ENCODING\_PROFILE\_STRICT** or **TCL\_ENCODING\_PROFILE\_REPLACE**.

[\*dstPtr]{.carg .out type="Tcl_DString"}
: Pointer to an uninitialized or free **Tcl\_DString** in which the converted result, which is also encoded in Tcl's internal UTF-8 encoding, will be stored. The function initializes the storage and caller must call **Tcl\_DStringFree** on success.


:::

# Description

The **Tcl\_UtfToNormalized** and **Tcl\_UtfToNormalizedDString** functions transform the passed string into one of the standard normalization forms defined in the Unicode standard. The normalization form is specified via the *normForm* argument which must have one of the values **TCL\_NFC**, **TCL\_NFD**, **TCL\_NFKC** or **TCL\_NFKD** corresponding to the Unicode normalization forms **Normalization Form C** (NFC), **Normalization Form D** (NFD), **Normalization Form KC** (NFKC) and **Normalization Form KD** (NFKD) respectively. For details on the above normalization forms, refer to Section 3.11 of the Unicode standard (https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-3/).

The **Tcl\_UtfToNormalized** function stores the normalized result in the buffer provided by the caller through the *dst* argument. The result is nul terminated but the nul is not included in the count of stored bytes returned in *dstWrotePtr*. The function returns TCL\_OK on success, TCL\_CONVERT\_NOSPACE if there is insufficient room in the output buffer and TCL\_ERROR in case of other failures. In the latter two cases, an error message is stored in *interp* if it is not NULL.

The **Tcl\_UtfToNormalizedDString** function stores the normalized result in *dstPtr* which must eventually be freed by caller through **Tcl\_DStringFree**. The function returns TCL\_OK on success and TCL\_ERROR on failure with an error message in *interp* if it is not NULL.

