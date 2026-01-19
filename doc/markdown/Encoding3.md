---
CommandName: Tcl_GetEncoding
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - encoding(n)
Keywords:
 - utf
 - encoding
 - convert
Copyright:
 - Copyright (c) 1997-1998 Sun Microsystems, Inc.
---

# Name

Tcl\_GetEncoding, Tcl\_FreeEncoding, Tcl\_GetEncodingFromObj, Tcl\_ExternalToUtfDString, Tcl\_ExternalToUtfDStringEx, Tcl\_ExternalToUtf, Tcl\_UtfToExternalDString, Tcl\_UtfToExternalDStringEx, Tcl\_UtfToExternal, Tcl\_GetEncodingName, Tcl\_SetSystemEncoding, Tcl\_GetEncodingNameFromEnvironment, Tcl\_GetEncodingNameForUser, Tcl\_GetEncodingNames, Tcl\_CreateEncoding, Tcl\_GetEncodingSearchPath, Tcl\_SetEncodingSearchPath - procedures for creating and using encodings

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Encoding]{.ret} [Tcl\_GetEncoding]{.ccmd}[interp, name]{.cargs}
[Tcl\_FreeEncoding]{.ccmd}[encoding]{.cargs}
[int]{.ret} [Tcl\_GetEncodingFromObj]{.ccmd}[interp, objPtr, encodingPtr]{.cargs}
[char \*]{.ret} [Tcl\_ExternalToUtfDString]{.ccmd}[encoding, src, srcLen, dstPtr]{.cargs}
[int]{.ret} [Tcl\_ExternalToUtfDStringEx]{.ccmd}[interp, encoding, src, srcLen, flags, dstPtr, errorIdxPtr]{.cargs}
[char \*]{.ret} [Tcl\_UtfToExternalDString]{.ccmd}[encoding, src, srcLen, dstPtr]{.cargs}
[int]{.ret} [Tcl\_UtfToExternalDStringEx]{.ccmd}[interp, encoding, src, srcLen, flags, dstPtr, errorIdxPtr]{.cargs}
[int]{.ret} [Tcl\_ExternalToUtf]{.ccmd}[interp, encoding, src, srcLen, flags, statePtr, dst, dstLen, srcReadPtr, dstWrotePtr, dstCharsPtr]{.cargs}
[int]{.ret} [Tcl\_UtfToExternal]{.ccmd}[interp, encoding, src, srcLen, flags, statePtr, dst, dstLen, srcReadPtr, dstWrotePtr, dstCharsPtr]{.cargs}
[const char \*]{.ret} [Tcl\_GetEncodingName]{.ccmd}[encoding]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_GetEncodingNulLength]{.ccmd}[encoding]{.cargs}
[int]{.ret} [Tcl\_SetSystemEncoding]{.ccmd}[interp, name]{.cargs}
[const char \*]{.ret} [Tcl\_GetEncodingNameFromEnvironment]{.ccmd}[bufPtr]{.cargs}
[const char \*]{.ret} [Tcl\_GetEncodingNameForUser]{.ccmd}[bufPtr]{.cargs}
[Tcl\_GetEncodingNames]{.ccmd}[interp]{.cargs}
[Tcl\_Encoding]{.ret} [Tcl\_CreateEncoding]{.ccmd}[typePtr]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_GetEncodingSearchPath]{.ccmd}[]{.cargs}
[int]{.ret} [Tcl\_SetEncodingSearchPath]{.ccmd}[searchPath]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Interpreter to use for error reporting, or NULL if no error reporting is desired. .AP "const char" \*name in Name of encoding to load. .AP Tcl\_Encoding encoding in The encoding to query, free, or use for converting text.  If *encoding* is NULL, the current system encoding is used. .AP Tcl\_Obj \*objPtr in Name of encoding to get token for. .AP Tcl\_Encoding \*encodingPtr out Points to storage where encoding token is to be written. .AP "const char" \*src in For the **Tcl\_ExternalToUtf** functions, an array of bytes in the specified encoding that are to be converted to TUTF-8.  For the **Tcl\_UtfToExternal** function, a TUTF-8 byte sequence to be converted to the specified encoding. .AP "const TCHAR" \*tsrc in An array of Windows TCHAR characters to convert to TUTF-8. .AP Tcl\_Size srcLen in Length of *src* or *tsrc* in bytes.  If the length is negative, the encoding-specific length of the string is used. .AP Tcl\_DString \*dstPtr out Pointer to an uninitialized or free **Tcl\_DString** in which the converted result will be stored. .AP int flags in This is a bit mask passed in to control the operation of the encoding functions. **TCL\_ENCODING\_START** signifies that the source buffer is the first block in a (potentially multi-block) input stream, telling the conversion routine to reset to an initial state and perform any initialization that needs to occur before the first byte is converted. **TCL\_ENCODING\_END** signifies that the source buffer is the last block in a (potentially multi-block) input stream, telling the conversion routine to perform any finalization that needs to occur after the last byte is converted and then to reset to an initial state. The **TCL\_PROFILE\_\*** bits defined in the **PROFILES** section below control the encoding profile to be used for dealing with invalid data or other errors in the encoding transform. The flag **TCL\_ENCODING\_STOPONERROR** has no effect, it only has meaning in Tcl 8.x. Some flags bits may not be usable with some functions as noted in the function descriptions below. .AP Tcl\_EncodingState \*statePtr in/out Used when converting a (generally long or indefinite length) byte stream in a piece-by-piece fashion.  The conversion routine stores its current state in *\*statePtr* after *src* (the buffer containing the current piece) has been converted; that state information must be passed back when converting the next piece of the stream so the conversion routine knows what state it was in when it left off at the end of the last piece.  May be NULL, in which case the value specified for *flags* is ignored and the source buffer is assumed to contain the complete string to convert. .AP char \*dst out Buffer in which the converted result will be stored.  No more than *dstLen* bytes will be stored in *dst*. .AP Tcl\_Size dstLen in The maximum length of the output buffer *dst* in bytes. .AP int \*srcReadPtr out Filled with the number of bytes from *src* that were actually converted.  This may be less than the original source length if there was a problem converting some source characters.  May be NULL. .AP int \*dstWrotePtr out Filled with the number of bytes that were actually stored in the output buffer as a result of the conversion.  May be NULL. .AP int \*dstCharsPtr out Filled with the number of characters that correspond to the number of bytes stored in the output buffer.  May be NULL. .AP Tcl\_Size \*errorIdxPtr out Filled with the index of the byte or character that caused the encoding transform to fail. May be NULL. .AP Tcl\_DString \*bufPtr out Storage for the prescribed system encoding name. .AP "const Tcl\_EncodingType" \*typePtr in Structure that defines a new type of encoding. .AP Tcl\_Obj \*searchPath in List of filesystem directories in which to search for encoding data files. .AP "const char" \*path in A path to the location of the encoding file.

# Introduction

N.B. Refer to the **Tcl\_UniChar** documentation page for a description of the *TUTF-8* encoding and related terms referenced here.

These routines convert between TUTF-8 and character representations using encodings such as standard UTF-8, UTF-16, ASCII, or Shift-JIS that might be expected by system interfaces or other software components. For instance, on a Japanese Unix workstation, a user might obtain a filename represented in the EUC-JP file encoding and then translate the characters to the jisx0208 font encoding in order to display the filename in a Tk widget. The purpose of the encoding package is to help bridge the translation gap. TUTF-8 provides an intermediate staging ground for all the various encodings. In the example above, text would be translated into TUTF-8 from whatever file encoding the operating system is using. Then it would be translated from TUTF-8 into whatever font encoding the display routines require.

Some basic encodings are compiled into Tcl. Others can be defined by the user or dynamically loaded from encoding files in a platform-independent manner.

# Description

**Tcl\_GetEncoding** finds an encoding given its *name*.  The name may refer to a built-in Tcl encoding, a user-defined encoding registered by calling **Tcl\_CreateEncoding**, or a dynamically-loadable encoding file.  The return value is a token that represents the encoding and can be used in subsequent calls to procedures such as **Tcl\_GetEncodingName**, **Tcl\_FreeEncoding**, and **Tcl\_UtfToExternal**.  If the name did not refer to any known or loadable encoding, NULL is returned and an error message is returned in *interp*.

The encoding package maintains a database of all encodings currently in use. The first time *name* is seen, **Tcl\_GetEncoding** returns an encoding with a reference count of 1.  If the same *name* is requested further times, then the reference count for that encoding is incremented without the overhead of allocating a new encoding and all its associated data structures.

When an *encoding* is no longer needed, **Tcl\_FreeEncoding** should be called to release it.  When an *encoding* is no longer in use anywhere (i.e., it has been freed as many times as it has been gotten) **Tcl\_FreeEncoding** will release all storage the encoding was using and delete it from the database.

**Tcl\_GetEncodingFromObj** treats the string representation of *objPtr* as an encoding name, and finds an encoding with that name, just as **Tcl\_GetEncoding** does. When an encoding is found, it is cached within the **objPtr** value for future reference, the **Tcl\_Encoding** token is written to the storage pointed to by *encodingPtr*, and the value **TCL\_OK** is returned. If no such encoding is found, the value **TCL\_ERROR** is returned, and no writing to **\****encodingPtr* takes place. Just as with **Tcl\_GetEncoding**, the caller should call **Tcl\_FreeEncoding** on the resulting encoding token when that token will no longer be used.

**Tcl\_ExternalToUtfDString** converts a source buffer *src* from the specified *encoding* into TUTF-8.  The converted bytes are stored in *dstPtr*, which is then null-terminated.  The caller should eventually call **Tcl\_DStringFree** to free any information stored in *dstPtr*. When converting, if any of the characters in the source buffer cannot be represented in the target encoding, a default fallback character will be used.  The return value is a pointer to the value stored in the DString.

**Tcl\_ExternalToUtfDStringEx** is a more flexible version of older **Tcl\_ExternalToUtfDString** function. It takes three additional parameters, [interp], **flags** and **errorIdxPtr**. The **flags** parameter may be used to specify the profile to be used for the transform. The **TCL\_ENCODING\_START** and **TCL\_ENCODING\_END** bits in **flags** are ignored as the function assumes the entire source string to be decoded is passed into the function. On success, the function returns **TCL\_OK** with the converted string stored in **\*dstPtr**. For errors *other than conversion errors*, such as invalid flags, the function returns **TCL\_ERROR** with an error message in [interp] if it is not NULL. For conversion errors, **Tcl\_ExternalToUtfDStringEx** returns one of the **TCL\_CONVERT\_\*** errors listed below for **Tcl\_ExternalToUtf**. When one of these conversion errors is returned, an error message is stored in [interp] only if **errorIdxPtr** is NULL. Otherwise, no error message is stored as the function expects the caller is interested the decoded data up to that point and not treating this as an immediate error condition. The index of the error location is stored in **\*errorIdxPtr**.

The caller must call **Tcl\_DStringFree** to free up the **\*dstPtr** resources irrespective of the return value from the function.

**Tcl\_ExternalToUtf** converts a source buffer *src* from the specified *encoding* into TUTF-8.  Up to *srcLen* bytes are converted from the source buffer and up to *dstLen* converted bytes are stored in *dst*. In all cases, *\*srcReadPtr* is filled with the number of bytes that were successfully converted from *src* and *\*dstWrotePtr* is filled with the corresponding number of bytes that were stored in *dst*.  The return value is one of the following:

**TCL\_OK**
: All bytes of *src* were converted.

**TCL\_CONVERT\_NOSPACE**
: The destination buffer was not large enough for all of the converted data; as many characters as could fit were converted though.

**TCL\_CONVERT\_MULTIBYTE**
: The last few bytes in the source buffer were the beginning of a multibyte sequence, but more bytes were needed to complete this sequence.  A subsequent call to the conversion routine should pass a buffer containing the unconverted bytes that remained in *src* plus some further bytes from the source stream to properly convert the formerly split-up multibyte sequence.

**TCL\_CONVERT\_SYNTAX**
: The source buffer contained an invalid byte or character sequence.  This may occur if the input stream has been damaged or if the input encoding method was misidentified.

**TCL\_CONVERT\_UNKNOWN**
: The source buffer contained a character that could not be represented in the target encoding.


**Tcl\_UtfToExternalDString** converts a source buffer *src* from TUTF-8 into the specified *encoding*.  The converted bytes are stored in *dstPtr*, which is then terminated with the appropriate encoding-specific null.  The caller should eventually call **Tcl\_DStringFree** to free any information stored in *dstPtr*.  When converting, if any of the characters in the source buffer cannot be represented in the target encoding, a default fallback character will be used.  The return value is a pointer to the value stored in the DString.

**Tcl\_UtfToExternalDStringEx** is an enhanced version of **Tcl\_UtfToExternalDString** that transforms TUTF-8 encoded source data to a specified *encoding*. Except for the direction of the transform, the parameters and return values are identical to those of **Tcl\_ExternalToUtfDStringEx**. See that function above for details about the same.

Irrespective of the return code from the function, the caller must free resources associated with **\*dstPtr** when the function returns.

**Tcl\_UtfToExternal** converts a source buffer *src* from TUTF-8 into the specified *encoding*.  Up to *srcLen* bytes are converted from the source buffer and up to *dstLen* converted bytes are stored in *dst*.  In all cases, *\*srcReadPtr* is filled with the number of bytes that were successfully converted from *src* and *\*dstWrotePtr* is filled with the corresponding number of bytes that were stored in *dst*.  The return values are the same as the return values for **Tcl\_ExternalToUtf**.

**Tcl\_GetEncodingName** is roughly the inverse of **Tcl\_GetEncoding**. Given an *encoding*, the return value is the *name* argument that was used to create the encoding.  The string returned by **Tcl\_GetEncodingName** is only guaranteed to persist until the *encoding* is deleted.  The caller must not modify this string.

**Tcl\_GetEncodingNulLength** returns the length of the terminating nul byte sequence for strings in the specified encoding.

**Tcl\_SetSystemEncoding** sets the default encoding that should be used whenever the user passes a NULL value for the *encoding* argument to any of the other encoding functions.  If *name* is NULL, the system encoding is reset to the default system encoding, [binary].  If the name did not refer to any known or loadable encoding, **TCL\_ERROR** is returned and an error message is left in *interp*.  Otherwise, this procedure increments the reference count of the new system encoding, decrements the reference count of the old system encoding, and returns **TCL\_OK**.

**Tcl\_GetEncodingNameFromEnvironment** retrieves the encoding name to use as the system encoding. On non-Windows platforms, this is derived from the **nl\_langinfo** system call if available, and environment variables **LC\_ALL**, **LC\_CTYPE** or **LANG** otherwise. On Windows versions Windows 10 Build 18362 and later the returned value is always **utf-8**. On earlier Windows versions, it is derived from the user settings in the Windows registry. **Tcl\_GetEncodingNameForUser** retrieves the encoding name based on the user settings for the current user and is derived in the same manner as **Tcl\_GetEncodingNameFromEnvironment** on non-Windows platforms. On Windows, unlike **Tcl\_GetEncodingNameFromEnvironment**, it returns the encoding name as per the Windows registry settings irrespective of the Windows version. Both functions accept *bufPtr*, a pointer to an uninitialized or freed **Tcl\_DString** and write the encoding name to it. They return **Tcl\_DStringValue(bufPtr)** which points to the stored name.

**Tcl\_GetEncodingNames** sets the *interp* result to a list consisting of the names of all the encodings that are currently defined or can be dynamically loaded, searching the encoding path specified by **Tcl\_SetEncodingSearchPath**.  This procedure does not ensure that the dynamically-loadable encoding files contain valid data, but merely that they exist.

**Tcl\_CreateEncoding** defines a new encoding and registers the C procedures that are called back to convert between the encoding and TUTF-8.  Encodings created by **Tcl\_CreateEncoding** are thereafter visible in the database used by **Tcl\_GetEncoding**.  Just as with the **Tcl\_GetEncoding** procedure, the return value is a token that represents the encoding and can be used in subsequent calls to other encoding functions.  **Tcl\_CreateEncoding** returns an encoding with a reference count of 1. If an encoding with the specified *name* already exists, then its entry in the database is replaced with the new encoding; the token for the old encoding will remain valid and continue to behave as before, but users of the new token will now call the new encoding procedures.

The *typePtr* argument to **Tcl\_CreateEncoding** contains information about the name of the encoding and the procedures that will be called to convert between this encoding and TUTF-8.  It is defined as follows:

```
typedef struct {
    const char *encodingName;
    Tcl_EncodingConvertProc *toUtfProc;
    Tcl_EncodingConvertProc *fromUtfProc;
    Tcl_EncodingFreeProc *freeProc;
    void *clientData;
    Tcl_Size nullSize;
} Tcl_EncodingType;
```

The *encodingName* provides a string name for the encoding, by which it can be referred in other procedures such as **Tcl\_GetEncoding**.  The *toUtfProc* refers to a callback procedure to invoke to convert text from this encoding into TUTF-8. The *fromUtfProc* refers to a callback procedure to invoke to convert text from TUTF-8 into this encoding.  The *freeProc* refers to a callback procedure to invoke when this encoding is deleted.  The *freeProc* field may be NULL.  The *clientData* contains an arbitrary one-word value passed to *toUtfProc*, *fromUtfProc*, and *freeProc* whenever they are called.  Typically, this is a pointer to a data structure containing encoding-specific information that can be used by the callback procedures.  For instance, two very similar encodings such as **ascii** and **macRoman** may use the same callback procedure, but use different values of *clientData* to control its behavior.  The *nullSize* specifies the number of zero bytes that signify end-of-string in this encoding.  It must be **1** (for single-byte or multi-byte encodings like ASCII or Shift-JIS) or **2** (for double-byte encodings like Unicode). Constant-sized encodings with 3 or more bytes per character (such as CNS11643) are not accepted.

The callback procedures *toUtfProc* and *fromUtfProc* should match the type **Tcl\_EncodingConvertProc**:

```
typedef int Tcl_EncodingConvertProc(
        void *clientData,
        const char *src,
        int srcLen,
        int flags,
        Tcl_EncodingState *statePtr,
        char *dst,
        int dstLen,
        int *srcReadPtr,
        int *dstWrotePtr,
        int *dstCharsPtr);
```

The *toUtfProc* and *fromUtfProc* procedures are called by the **Tcl\_ExternalToUtf** or **Tcl\_UtfToExternal** family of functions to perform the actual conversion.  The *clientData* parameter to these procedures is the same as the *clientData* field specified to **Tcl\_CreateEncoding** when the encoding was created.  The remaining arguments to the callback procedures are the same as the arguments, documented at the top, to **Tcl\_ExternalToUtf** or **Tcl\_UtfToExternal**, with the following exceptions.  If the *srcLen* argument to one of those high-level functions is negative, the value passed to the callback procedure will be the appropriate encoding-specific string length of *src*.  If any of the *srcReadPtr*, *dstWrotePtr*, or *dstCharsPtr* arguments to one of the high-level functions is NULL, the corresponding value passed to the callback procedure will be a non-NULL location.

The callback procedure *freeProc*, if non-NULL, should match the type **Tcl\_EncodingFreeProc**:

```
typedef void Tcl_EncodingFreeProc(
        void *clientData);
```

This *freeProc* function is called when the encoding is deleted.  The *clientData* parameter is the same as the *clientData* field specified to **Tcl\_CreateEncoding** when the encoding was created.

**Tcl\_GetEncodingSearchPath** and **Tcl\_SetEncodingSearchPath** are called to access and set the list of filesystem directories searched for encoding data files.

The value returned by **Tcl\_GetEncodingSearchPath** is the value stored by the last successful call to **Tcl\_SetEncodingSearchPath**.  If no calls to **Tcl\_SetEncodingSearchPath** have occurred, Tcl will compute an initial value based on the environment.  There is one encoding search path for the entire process, shared by all threads in the process.

**Tcl\_SetEncodingSearchPath** stores *searchPath* and returns **TCL\_OK**, unless *searchPath* is not a valid Tcl list, which causes **TCL\_ERROR** to be returned.  The elements of *searchPath* are not verified as existing readable filesystem directories.  When searching for encoding data files takes place, and non-existent or non-readable filesystem directories on the *searchPath* are silently ignored.

# Encoding files

Space would prohibit precompiling into Tcl every possible encoding algorithm, so many encodings are stored on disk as dynamically-loadable encoding files.  This behavior also allows the user to create additional encoding files that can be loaded using the same mechanism.  These encoding files contain information about the tables and/or escape sequences used to map between an external encoding and Unicode.  The external encoding may consist of single-byte, multi-byte, or double-byte characters.

Each dynamically-loadable encoding is represented as a text file.  The initial line of the file, beginning with a "#" symbol, is a comment that provides a human-readable description of the file.  The next line identifies the type of encoding file.  It can be one of the following letters:

[1] **S**
: A single-byte encoding, where one character is always one byte long in the encoding.  An example is **iso8859-1**, used by many European languages.

[2] **D**
: A double-byte encoding, where one character is always two bytes long in the encoding.  An example is **big5**, used for Chinese text.

[3] **M**
: A multi-byte encoding, where one character may be either one or two bytes long. Certain bytes are lead bytes, indicating that another byte must follow and that together the two bytes represent one character.  Other bytes are not lead bytes and represent themselves.  An example is **shiftjis**, used by many Japanese computers.

[4] **E**
: An escape-sequence encoding, specifying that certain sequences of bytes do not represent characters, but commands that describe how following bytes should be interpreted.


The rest of the lines in the file depend on the type.

Cases [1], [2], and [3] are collectively referred to as table-based encoding files.  The lines in a table-based encoding file are in the same format as this example taken from the **shiftjis** encoding (this is not the complete file):

```
# Encoding file: shiftjis, multi-byte
M
003F 0 40
00
0000000100020003000400050006000700080009000A000B000C000D000E000F
0010001100120013001400150016001700180019001A001B001C001D001E001F
0020002100220023002400250026002700280029002A002B002C002D002E002F
0030003100320033003400350036003700380039003A003B003C003D003E003F
0040004100420043004400450046004700480049004A004B004C004D004E004F
0050005100520053005400550056005700580059005A005B005C005D005E005F
0060006100620063006400650066006700680069006A006B006C006D006E006F
0070007100720073007400750076007700780079007A007B007C007D203E007F
0080000000000000000000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000000000000000000000
0000FF61FF62FF63FF64FF65FF66FF67FF68FF69FF6AFF6BFF6CFF6DFF6EFF6F
FF70FF71FF72FF73FF74FF75FF76FF77FF78FF79FF7AFF7BFF7CFF7DFF7EFF7F
FF80FF81FF82FF83FF84FF85FF86FF87FF88FF89FF8AFF8BFF8CFF8DFF8EFF8F
FF90FF91FF92FF93FF94FF95FF96FF97FF98FF99FF9AFF9BFF9CFF9DFF9EFF9F
0000000000000000000000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000000000000000000000
81
0000000000000000000000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000000000000000000000
0000000000000000000000000000000000000000000000000000000000000000
300030013002FF0CFF0E30FBFF1AFF1BFF1FFF01309B309C00B4FF4000A8FF3E
FFE3FF3F30FD30FE309D309E30034EDD30053006300730FC20152010FF0F005C
301C2016FF5C2026202520182019201C201DFF08FF0930143015FF3BFF3DFF5B
FF5D30083009300A300B300C300D300E300F30103011FF0B221200B100D70000
00F7FF1D2260FF1CFF1E22662267221E22342642264000B0203220332103FFE5
FF0400A200A3FF05FF03FF06FF0AFF2000A72606260525CB25CF25CE25C725C6
25A125A025B325B225BD25BC203B301221922190219121933013000000000000
000000000000000000000000000000002208220B2286228722822283222A2229
000000000000000000000000000000002227222800AC21D221D4220022030000
0000000000000000000000000000000000000000222022A52312220222072261
2252226A226B221A223D221D2235222B222C0000000000000000000000000000
212B2030266F266D266A2020202100B6000000000000000025EF000000000000
```

The third line of the file is three numbers.  The first number is the fallback character (in base 16) to use when converting from TUTF-8 to this encoding.  The second number is a **1** if this file represents the encoding for a symbol font, or **0** otherwise.  The last number (in base 10) is how many pages of data follow.

Subsequent lines in the example above are pages that describe how to map from the encoding into 2-byte Unicode.  The first line in a page identifies the page number.  Following it are 256 double-byte numbers, arranged as 16 rows of 16 numbers.  Given a character in the encoding, the high byte of that character is used to select which page, and the low byte of that character is used as an index to select one of the double-byte numbers in that page - the value obtained being the corresponding Unicode character. By examination of the example above, one can see that the characters 0x7E and 0x8163 in **shiftjis** map to 203E and 2026 in Unicode, respectively.

Following the first page will be all the other pages, each in the same format as the first: one number identifying the page followed by 256 double-byte Unicode characters.  If a character in the encoding maps to the Unicode character 0000, it means that the character does not actually exist. If all characters on a page would map to 0000, that page can be omitted.

Case [4] is the escape-sequence encoding file.  The lines in an this type of file are in the same format as this example taken from the **iso2022-jp** encoding:

```
# Encoding file: iso2022-jp, escape-driven
E
init		{}
final		{}
iso8859-1	\x1B(B
jis0201		\x1B(J
jis0208		\x1B$@
jis0208		\x1B$B
jis0212		\x1B$(D
gb2312		\x1B$A
ksc5601		\x1B$(C
```

In the file, the first column represents an option and the second column is the associated value.  **init** is a string to emit or expect before the first character is converted, while **final** is a string to emit or expect after the last character.  All other options are names of table-based encodings; the associated value is the escape-sequence that marks that encoding.  Tcl syntax is used for the values; in the above example, for instance, "**{}**" represents the empty string and "**\\x1B**" represents character 27.

When **Tcl\_GetEncoding** encounters an encoding *name* that has not been loaded, it attempts to load an encoding file called *name***.enc** from the [encoding] subdirectory of each directory that Tcl searches for its script library.  If the encoding file exists, but is malformed, an error message will be left in *interp*.

# Reference count management

**Tcl\_GetEncodingFromObj** does not modify the reference count of its *objPtr* argument; it only reads. Note however that this function may set the interpreter result; if that is the only place that is holding a reference to the object, it will be deleted.

**Tcl\_GetEncodingSearchPath** returns an object with a reference count of at least 1.

# Profiles

Encoding profiles define the manner in which errors in the encoding transforms are handled by the encoding functions. An application can specify the profile to be used by OR-ing the **flags** parameter passed to the function with at most one of **TCL\_ENCODING\_PROFILE\_TCL8**, **TCL\_ENCODING\_PROFILE\_STRICT** or **TCL\_ENCODING\_PROFILE\_REPLACE**. These correspond to the **tcl8**, **strict** and **replace** profiles respectively. If none are specified, a version-dependent default profile is used. For Tcl 9.0, the default profile is **strict**.

For details about profiles, see the **PROFILES** section in the documentation of the [encoding] command.


[binary]: binary.md
[encoding]: encoding.md
[interp]: interp.md

