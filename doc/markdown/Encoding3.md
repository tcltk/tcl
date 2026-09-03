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

Tcl\_GetEncoding, Tcl\_FreeEncoding, Tcl\_GetEncodingFromObj, Tcl\_ExternalToUtfDString, Tcl\_ExternalToUtfDStringEx, Tcl\_ExternalToUtf,Tcl\_ExternalToUtfEx, Tcl\_UtfToExternalDString, Tcl\_UtfToExternalDStringEx, Tcl\_UtfToExternal, Tcl\_UtfToExternalEx, Tcl\_GetEncodingName, Tcl\_SetSystemEncoding, Tcl\_GetEncodingNameFromEnvironment, Tcl\_GetEncodingNameForUser, Tcl\_GetEncodingNames, Tcl\_CreateEncoding, Tcl\_GetEncodingSearchPath, Tcl\_SetEncodingSearchPath - procedures for creating and using encodings

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
[int]{.ret} [Tcl\_ExternalToUtfEx]{.ccmd}[interp, encoding, src, srcLen, flags, statePtr, dst, dstLen, srcReadPtr, dstWrotePtr, dstCharsPtr]{.cargs}
[int]{.ret} [Tcl\_ExternalToUtf]{.ccmd}[interp, encoding, src, srcLen, flags, statePtr, dst, dstLen, srcReadIntPtr, dstWroteIntPtr, dstCharsIntPtr]{.cargs}
[int]{.ret} [Tcl\_UtfToExternalEx]{.ccmd}[interp, encoding, src, srcLen, flags, statePtr, dst, dstLen, srcReadPtr, dstWrotePtr, dstCharsPtr]{.cargs}
[int]{.ret} [Tcl\_UtfToExternal]{.ccmd}[interp, encoding, src, srcLen, flags, statePtr, dst, dstLen, srcReadIntPtr, dstWroteIntPtr, dstCharsIntPtr]{.cargs}
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

::: {.arguments} :::

[\*interp]{.carg .in type="Tcl_Interp"}
: Interpreter to use for error reporting, or NULL if no error reporting is desired.

[\*name]{.carg .in type="const char"}
: Name of encoding to load.

[encoding]{.carg .in type="Tcl_Encoding"}
: The encoding to query, free, or use for converting text.  If *encoding* is NULL, the current system encoding is used.

[\*objPtr]{.carg .in type="Tcl_Obj"}
: Name of encoding to get token for.

[\*encodingPtr]{.carg .out type="Tcl_Encoding"}
: Points to storage where encoding token is to be written.

[\*src]{.carg .in type="const char"}
: For the **Tcl\_ExternalToUtf** functions, an array of bytes in the specified encoding that are to be converted to TUTF-8.  For the **Tcl\_UtfToExternal** function, a TUTF-8 byte sequence to be converted to the specified encoding.

[\*tsrc]{.carg .in type="const TCHAR"}
: An array of Windows TCHAR characters to convert to TUTF-8.

[srcLen]{.carg .in type="Tcl_Size"}
: Length of *src* or *tsrc* in bytes.  If the length is negative, the encoding-specific length of the string is used.

[\*dstPtr]{.carg .out type="Tcl_DString"}
: Pointer to an uninitialized or free **Tcl\_DString** in which the converted result will be stored.

[flags]{.carg .in type="int"}
: This is a bit mask passed in to control the operation of the encoding functions. Any bits not defined in the function descriptions below should be set to 0 as they are used internally by Tcl.

[\*statePtr]{.carg .in/out type="Tcl_EncodingState"}
: Used when converting a (generally long or indefinite length) byte stream in a piece-by-piece fashion.

[\*dst]{.carg .out type="char"}
: Buffer in which the converted result will be stored.  No more than *dstLen* bytes will be stored in *dst*.

[dstLen]{.carg .in type="Tcl_Size"}
: The maximum length of the output buffer *dst* in bytes.

[\*srcReadPtr]{.carg .out type="Tcl_Size"}
: Filled with the number of bytes from *src* that were converted. May be NULL.

[\*srcReadIntPtr]{.carg .out type="int"}
: Filled with the number of bytes from *src* that were converted. May be NULL.

[\*dstWrotePtr]{.carg .out type="Tcl_Size"}
: Filled with the number of bytes that were stored in the output buffer as a result of the conversion. May be NULL.

[\*dstWroteIntPtr]{.carg .out type="int"}
: Filled with the number of bytes that were stored in the output buffer as a result of the conversion.  May be NULL.

[\*dstCharsPtr]{.carg .out type="Tcl_Size"}
: Filled with the number of characters that correspond to the number of bytes stored in the output buffer. May be NULL.

[\*dstCharsIntPtr]{.carg .out type="int"}
: Filled with the number of characters that correspond to the number of bytes stored in the output buffer. May be NULL.

[\*errorIdxPtr]{.carg .out type="Tcl_Size"}
: Filled with the index of the byte or character that caused the encoding transform to fail. May be NULL.

[\*bufPtr]{.carg .out type="Tcl_DString"}
: Storage for the prescribed system encoding name.

[\*typePtr]{.carg .in type="const Tcl_EncodingType"}
: Structure that defines a new type of encoding.

[\*searchPath]{.carg .in type="Tcl_Obj"}
: List of filesystem directories in which to search for encoding data files.

[\*path]{.carg .in type="const char"}
: A path to the location of the encoding file.


:::

# Introduction

N.B. Refer to the [Tcl\_UniChar][Utf] documentation page for a description of the *TUTF-8* encoding and related terms referenced here.

These routines convert between TUTF-8 and character representations using encodings such as standard UTF-8, UTF-16, ASCII, or Shift-JIS that might be expected by system interfaces or other software components. For instance, on a Japanese Unix workstation, a user might obtain a filename represented in the EUC-JP file encoding and then translate the characters to the jisx0208 font encoding in order to display the filename in a Tk widget. The purpose of the encoding package is to help bridge the translation gap. TUTF-8 provides an intermediate staging ground for all the various encodings. In the example above, text would be translated into TUTF-8 from whatever file encoding the operating system is using. Then it would be translated from TUTF-8 into whatever font encoding the display routines require.

Some basic encodings are compiled into Tcl. Others can be defined by the user or dynamically loaded from encoding files in a platform-independent manner.

# Managing encodings

**Tcl\_GetEncoding** finds an encoding given its *name*. The name may refer to a built-in Tcl encoding, a user-defined encoding registered by calling **Tcl\_CreateEncoding**, or a dynamically-loadable encoding file. The return value is a token that represents the encoding and can be used in subsequent calls to functions that expect an argument of type **Tcl\_Encoding**. If the name did not refer to any known or loadable encoding, NULL is returned and an error message is stored in *interp*.

The encoding package maintains a database of all encodings currently in use. The first time *name* is seen, **Tcl\_GetEncoding** returns an encoding with a reference count of 1.  If the same *name* is requested further times, then the reference count for that encoding is incremented without the overhead of allocating a new encoding and all its associated data structures.

When an *encoding* is no longer needed, **Tcl\_FreeEncoding** should be called to release it.  When an *encoding* is no longer in use anywhere (i.e., it has been freed as many times as it has been gotten) **Tcl\_FreeEncoding** will release all storage the encoding was using and delete it from the database.

**Tcl\_GetEncodingFromObj** treats the string representation of *objPtr* as an encoding name, and finds an encoding with that name, just as **Tcl\_GetEncoding** does. When an encoding is found, it is cached within the **objPtr** value for future reference, the **Tcl\_Encoding** token is written to the storage pointed to by *encodingPtr*, and the value [TCL\_OK][catch] is returned. If no such encoding is found, the value [TCL\_ERROR][catch] is returned, and no writing to **\****encodingPtr* takes place. Just as with **Tcl\_GetEncoding**, the caller should call **Tcl\_FreeEncoding** on the resulting encoding token when that token will no longer be used.

**Tcl\_GetEncodingName** is roughly the inverse of **Tcl\_GetEncoding**. Given an *encoding*, the return value is the *name* argument that was used to create the encoding.  The string returned by **Tcl\_GetEncodingName** is only guaranteed to persist until the *encoding* is deleted.  The caller must not modify this string.

**Tcl\_GetEncodingNulLength** returns the length of the terminating nul byte sequence for strings in the specified encoding.

**Tcl\_SetSystemEncoding** sets the default encoding that should be used whenever the user passes a NULL value for the *encoding* argument to any of the other encoding functions.  If *name* is NULL, the system encoding is reset to the default system encoding, [binary].  If the name did not refer to any known or loadable encoding, [TCL\_ERROR][catch] is returned and an error message is left in *interp*.  Otherwise, this procedure increments the reference count of the new system encoding, decrements the reference count of the old system encoding, and returns [TCL\_OK][catch].

**Tcl\_GetEncodingNameFromEnvironment** retrieves the encoding name to use as the system encoding. On non-Windows platforms, this is derived from the **nl\_langinfo** system call if available, and environment variables **LC\_ALL**, **LC\_CTYPE** or **LANG** otherwise. On Windows versions Windows 10 Build 18362 and later the returned value is always **utf-8**. On earlier Windows versions, it is derived from the user settings in the Windows registry. **Tcl\_GetEncodingNameForUser** retrieves the encoding name based on the user settings for the current user and is derived in the same manner as **Tcl\_GetEncodingNameFromEnvironment** on non-Windows platforms. On Windows, unlike **Tcl\_GetEncodingNameFromEnvironment**, it returns the encoding name as per the Windows registry settings irrespective of the Windows version. Both functions accept *bufPtr*, a pointer to an uninitialized or freed **Tcl\_DString** and write the encoding name to it. They return **[Tcl\_DStringValue][DString](bufPtr)** which points to the stored name.

**Tcl\_GetEncodingNames** sets the *interp* result to a list consisting of the names of all the encodings that are currently defined or can be dynamically loaded, searching the encoding path specified by **Tcl\_SetEncodingSearchPath**.  This procedure does not ensure that the dynamically-loadable encoding files contain valid data, but merely that they exist.

# Converting between tutf-8 and other encodings

There are two sets of functions to convert data between TUTF-8 and any external encoding known to Tcl. They differ primarily in the form in which converted data is returned to the caller. The **Tcl\_ExternalToUtfEx** and **Tcl\_UtfToExternalEx** functions return the data in buffers supplied by the caller. The **Tcl\_ExternalToUtfDStringEx** and **Tcl\_UtfToExternalDStringEx** return the data in a **Tcl\_DString** structure. For backwards compatibility, Tcl also provides "non-Ex" variants of these such as **Tcl\_ExternalToUtf**.

## Conversion using output buffers

The **Tcl\_ExternalToUtfEx** function converts bytes in the encoding *encoding* at address *src* into TUTF-8 encoding, storing them in the buffer at address *dst*. Conversely, **Tcl\_UtfToExternalEx** converts bytes encoded in TUTF-8 at address *src* into the encoding given by *encoding*, storing them in the output buffer at address *dst*. In both cases, *srcLen* specifies the number of bytes to be converted and *dstLen* specifies the size of the output buffer.

The *flags* parameter is a bitmask that controls operation of the conversion and should be a bitwise OR of zero or more of the following values:

**TCL\_ENCODING\_PROFILE\_xxx**
: At most one of the profile selection flags listed in the [Profiles] section of this manpage.

**TCL\_ENCODING\_NO\_TERMINATE**
: Disables null termination of the output. By default, the output buffer *dst* is terminated with an encoding-appropriate null.

**TCL\_ENCODING\_START**
: Indicate whether the source bytes correspond to the first or last blocks, respectively, in a source stream. **TCL\_ENCODING\_START** will cause the conversion routine to reset to an initial state ready to process the first byte of an encoded stream. **TCL\_ENCODING\_END** indicates the source buffer is the last block in an input stream allowing any required finalization to be performed. Any incomplete trailing characters will then be treated as per the encoding profile in effect. Both flags may be specified in the same call when all data to be converted is passed in a single block. Both flags are also presumed to be implicitly set if the *statePtr* parameter is passed as NULL.

**TCL\_ENCODING\_CHAR\_LIMIT**
: Specifies that the functions should not convert more characters than the number passed through the *dstCharsPtr* argument, if not NULL. This flag is only supported by the **Tcl\_ExternalToUtfEx** function and should not be passed to other functions.


The *statePtr* parameter is an opaque pointer to a location used by the encoding functions to store intermediate state when the data to be converted is passed in multiple chunks. The same location should be passed in *statePtr* for all related calls with the first chunk passed with the **TCL\_ENCODING\_START** flag set and the last with **TCL\_ENCODING\_END** set. If *statePtr* is passed as NULL, all data is presumed to all be contained in that single call and the functions behave as if **TCL\_ENCODING\_START** and **TCL\_ENCODING\_END** were both set in the *flags* parameter.

The *srcReadPtr*, *dstWrotePtr* and *dstCharsPtr* point to locations to hold the number of bytes in the source that were processed successfully, the number of bytes written to the output buffer (excluding terminating nulls), and the number of characters written to the output buffer (again excluding terminating nulls) respectively. All three are optional and may be passed as NULL. Further, in the case of the **Tcl\_ExternalToUtfEx** function, the *dstCharsPtr* may be used with the **TCL\_ENCODING\_CHAR\_LIMIT** flags to limit the number of characters processed as described earlier.

With the exceptions noted below, the counts returned in *srcReadPtr*, *dstWrotePtr* and *dstCharsPtr* are valid for all return codes listed below other than [TCL\_ERROR][catch].

[TCL\_OK][catch]
: The function completed without any exceptional conditions. Note this does not mean all passed input in *src* was processed or verified. In particular, in the case of the caller passing **TCL\_ENCODING\_CHAR\_LIMIT** to limit the number of characters converted, only the corresponding number of bytes in the source input would have been processed as indicated by the value in *srcReadPtr*.

**TCL\_CONVERT\_NOSPACE**
: The output buffer had insufficient space. The output buffer will contain as much converted data as it could fit and will be null terminated as appropriate unless the buffer was too small to even contain a null terminator by itself. *srcReadPtr* will hold number of processed source bytes and caller should call again to process the remaining bytes. \*Note: As a quirk of implementation, in some cases the destination buffer needs to be **TCL\_UTF\_MAX** bytes greater than the actual size needed. This is an existing quirk present in both 8.6 and 9.0 that is not addressed in this TIP.\*

**TCL\_CONVERT\_MULTIBYTE**
: The trailing bytes in the source input formed an incomplete encoding sequence. Caller should call the function again with additional source bytes appended to the tail at offset *\*srcReadPtr* of the original source bytes and with the same *statePtr*. Note that if *flags* had the **TCL\_ENCODING\_END** flag set, indicating no more data is forthcoming, the functions will return **TCL\_CONVERT\_SYNTAX** instead of **TCL\_CONVERT\_MULTIBYTE**.

**TCL\_CONVERT\_SYNTAX**
: An invalid byte sequence was detected in the source input. What constitutes an "invalid" sequence is subject to the encoding profile as specified by the *flags* parameter. The *\*srcReadPtr* count will contain the number of bytes successfully processed and is therefore also the offset of the start of the invalid sequence. The output buffer will contain the converted data up to that point.

**TCL\_CONVERT\_UNKNOWN**
: The input byte sequence represented a character that cannot be encoded in the output encoding for the encoding profile in effect. Treatment is similar to that of **TCL\_CONVERT\_SYNTAX**.

[TCL\_ERROR][catch]
: An error message is stored in *interp* if it is not NULL. The output locations at *srcReadPtr*, *dstWrotePtr* and *dstCharsPtr* may have been modified but should not be considered valid.


The functions **Tcl\_ExternalToUtf** and **Tcl\_UtfToExternal** are variants of **Tcl\_ExternalToUtfEx** and **Tcl\_UtfToExternalEx**. They differ in that their output counts have a limit of INT\_MAX and therefore cannot handle the full string lengths supported by Tcl on 64-bit platforms. They will return [TCL\_ERROR][catch] in such cases.

## Conversion using tcl\_dstring

**Tcl\_ExternalToUtfDStringEx** and **Tcl\_UtfToExternalDStringEx** convert *srcLen* bytes at address *src* from the specified *encoding* into TUTF-8 and the reverse respectively. The converted bytes are stored in *dstPtr*, which is then null-terminated. The caller should eventually call [Tcl\_DStringFree][DString] to free any information stored in *dstPtr* irrespective of the return value from the function.

The *flags* argument to the functions should be 0 or one of the profile selection flags described above to select the profile to use for conversion. The other flags should be cleared. The functions assume the entire source string to be converted is passed into the function.

On success, the function returns [TCL\_OK][catch] with the converted string stored in **\*dstPtr**. For errors *other than conversion errors*, such as invalid flags, the function returns [TCL\_ERROR][catch] with an error message in [interp] if it is not NULL. For conversion errors, **Tcl\_ExternalToUtfDStringEx** returns one of the **TCL\_CONVERT\_\*** errors listed above. When one of these conversion errors is returned, an error message is stored in [interp] only if **errorIdxPtr** is NULL. Otherwise, no error message is stored as the function expects the caller is only interested the decoded data up to that point and not treating this as an immediate error condition. The index of the error location is stored in **\*errorIdxPtr**.

**Tcl\_ExternalToUtfDString** and **Tcl\_UtfToExternalDString** are older, less flexible variants of the above functions that do not support profiles. The return value from the functions is a pointer to the value stored in the **Tcl\_DString**. When converting, if any of the characters in the source buffer cannot be represented in the target encoding, a default fallback character will be used. The return value is a pointer to the value stored in the DString. **Tcl\_UtfToExternalDString** converts a source buffer *src* from TUTF-8 into the specified *encoding*. The converted bytes are stored in *dstPtr*, which is then terminated with the appropriate encoding-specific null. The caller should eventually call [Tcl\_DStringFree][DString] to free any information stored in *dstPtr*. When converting, if any of the characters in the source buffer cannot be represented in the target encoding, an encoding-specific default fallback character will be used.

# Creating new encodings

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
        int *srcReadIntPtr,
        int *dstWroteIntPtr,
        int *dstCharsIntPtr);
```

The *toUtfProc* and *fromUtfProc* procedures are called by the **Tcl\_ExternalToUtf** or **Tcl\_UtfToExternal** family of functions to perform the actual conversion.  The *clientData* parameter to these procedures is the same as the *clientData* field specified to **Tcl\_CreateEncoding** when the encoding was created.  The remaining arguments to the callback procedures are the same as the arguments, documented at the top, to **Tcl\_ExternalToUtf** or **Tcl\_UtfToExternal**, with the following exceptions.  If the *srcLen* argument to one of those high-level functions is negative, the value passed to the callback procedure will be the appropriate encoding-specific string length of *src*.  The *srcReadIntPtr*, *dstWroteIntPtr*, and *dstCharsIntPtr* arguments will always be non-NULL, even if the corresponding argument to one of the high-level functions is NULL.

The callback procedure *freeProc*, if non-NULL, should match the type **Tcl\_EncodingFreeProc**:

```
typedef void Tcl_EncodingFreeProc(
        void *clientData);
```

This *freeProc* function is called when the encoding is deleted.  The *clientData* parameter is the same as the *clientData* field specified to **Tcl\_CreateEncoding** when the encoding was created.

**Tcl\_GetEncodingSearchPath** and **Tcl\_SetEncodingSearchPath** are called to access and set the list of filesystem directories searched for encoding data files.

The value returned by **Tcl\_GetEncodingSearchPath** is the value stored by the last successful call to **Tcl\_SetEncodingSearchPath**.  If no calls to **Tcl\_SetEncodingSearchPath** have occurred, Tcl will compute an initial value based on the environment.  There is one encoding search path for the entire process, shared by all threads in the process.

**Tcl\_SetEncodingSearchPath** stores *searchPath* and returns [TCL\_OK][catch], unless *searchPath* is not a valid Tcl list, which causes [TCL\_ERROR][catch] to be returned.  The elements of *searchPath* are not verified as existing readable filesystem directories.  When searching for encoding data files takes place, and non-existent or non-readable filesystem directories on the *searchPath* are silently ignored.

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

For details about profiles, see the [Profiles] section in the documentation of the [encoding] command.


[binary]: binary.md
[catch]: catch.md
[DString]: DString.md
[encoding]: encoding.md
[interp]: interp.md
[Utf]: Utf.md

