---
CommandName: TclZlib
ManualSection: 3
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_NewByteArrayObj(3)
 - zlib(n)
Keywords:
 - compress
 - decompress
 - deflate
 - gzip
 - inflate
Copyright:
 - Copyright (c) 2008 Donal K. Fellows
---

# Name

Tcl\_ZlibAdler32, Tcl\_ZlibCRC32, Tcl\_ZlibDeflate, Tcl\_ZlibInflate, Tcl\_ZlibStreamChecksum, Tcl\_ZlibStreamClose, Tcl\_ZlibStreamEof, Tcl\_ZlibStreamGet, Tcl\_ZlibStreamGetCommandName, Tcl\_ZlibStreamInit, Tcl\_ZlibStreamPut - compression and decompression functions

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_ZlibDeflate]{.ccmd}[interp, format, dataObj, level, dictObj]{.cargs}
[int]{.ret} [Tcl\_ZlibInflate]{.ccmd}[interp, format, dataObj, bufferSize, dictObj]{.cargs}
[unsigned int]{.ret} [Tcl\_ZlibCRC32]{.ccmd}[initValue, bytes, length]{.cargs}
[unsigned int]{.ret} [Tcl\_ZlibAdler32]{.ccmd}[initValue, bytes, length]{.cargs}
[int]{.ret} [Tcl\_ZlibStreamInit]{.ccmd}[interp, mode, format, level, dictObj, zshandlePtr]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_ZlibStreamGetCommandName]{.ccmd}[zshandle]{.cargs}
[int]{.ret} [Tcl\_ZlibStreamEof]{.ccmd}[zshandle]{.cargs}
[int]{.ret} [Tcl\_ZlibStreamClose]{.ccmd}[zshandle]{.cargs}
[int]{.ret} [Tcl\_ZlibStreamReset]{.ccmd}[zshandle]{.cargs}
[int]{.ret} [Tcl\_ZlibStreamChecksum]{.ccmd}[zshandle]{.cargs}
[int]{.ret} [Tcl\_ZlibStreamPut]{.ccmd}[zshandle, dataObj, flush]{.cargs}
[int]{.ret} [Tcl\_ZlibStreamGet]{.ccmd}[zshandle, dataObj, count]{.cargs}
[Tcl\_ZlibStreamSetCompressionDictionary]{.ccmd}[zshandle, compDict]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in The interpreter to store resulting compressed or uncompressed data in. Also where any error messages are written. For **Tcl\_ZlibStreamInit**, this can be NULL to create a stream that is not bound to a command. .AP int format in What format of compressed data to work with. Must be one of **TCL\_ZLIB\_FORMAT\_ZLIB** for zlib-format data, **TCL\_ZLIB\_FORMAT\_GZIP** for gzip-format data, or **TCL\_ZLIB\_FORMAT\_RAW** for raw compressed data. In addition, for decompression only, **TCL\_ZLIB\_FORMAT\_AUTO** may also be chosen which can automatically detect whether the compressed data was in zlib or gzip format. .AP Tcl\_Obj \*dataObj in/out A byte-array value containing the data to be compressed or decompressed, or to which the data extracted from the stream is appended when passed to **Tcl\_ZlibStreamGet**. .AP int level in What level of compression to use. Should be a number from 0 to 9 or one of the following: **TCL\_ZLIB\_COMPRESS\_NONE** for no compression, **TCL\_ZLIB\_COMPRESS\_FAST** for fast but inefficient compression, **TCL\_ZLIB\_COMPRESS\_BEST** for slow but maximal compression, or **TCL\_ZLIB\_COMPRESS\_DEFAULT** for the level recommended by the zlib library. .AP Tcl\_Obj \*dictObj in/out A dictionary that contains, or which will be updated to contain, a description of the gzip header associated with the compressed data. Only useful when the *format* is **TCL\_ZLIB\_FORMAT\_GZIP** or **TCL\_ZLIB\_FORMAT\_AUTO**. If a NULL is passed, a default header will be used on compression and the header will be ignored (apart from integrity checks) on decompression. See the section [Gzip options dictionary] for details about the contents of this dictionary. .AP "unsigned int" initValue in The initial value for the checksum algorithm. .AP "Tcl\_Size" bufferSize in A hint as to what size of buffer is to be used to receive the data. Use 0 to use a geric guess based on the input data. .AP "unsigned char" \*bytes in An array of bytes to run the checksum algorithm over, or NULL to get the recommended initial value for the checksum algorithm. .AP Tcl\_Size length in The number of bytes in the array. .AP int mode in What mode to operate the stream in. Should be either **TCL\_ZLIB\_STREAM\_DEFLATE** for a compressing stream or **TCL\_ZLIB\_STREAM\_INFLATE** for a decompressing stream. .AP Tcl\_ZlibStream \*zshandlePtr out A pointer to a variable in which to write the abstract token for the stream upon successful creation. .AP Tcl\_ZlibStream zshandle in The abstract token for the stream to operate on. .AP int flush in Whether and how to flush the stream after writing the data to it. Must be one of: **TCL\_ZLIB\_NO\_FLUSH** if no flushing is to be done, **TCL\_ZLIB\_FLUSH** if the currently compressed data must be made available for access using **Tcl\_ZlibStreamGet**, **TCL\_ZLIB\_FULLFLUSH** if the stream must be put into a state where the decompressor can recover from on corruption, or **TCL\_ZLIB\_FINALIZE** to ensure that the stream is finished and that any trailer demanded by the format is written. .AP Tcl\_Size count in The maximum number of bytes to get from the stream, or -1 to get all remaining bytes from the stream's buffers. .AP Tcl\_Obj \*compDict in A byte array value that is the compression dictionary to use with the stream. Note that this is *not a Tcl dictionary*, and it is recommended that this only ever be used with streams that were created with their *format* set to **TCL\_ZLIB\_FORMAT\_ZLIB** because the other formats have no mechanism to indicate whether a compression dictionary was present other than to fail on decompression.

# Description

These functions form the interface from the Tcl library to the Zlib library by Jean-loup Gailly and Mark Adler.

**Tcl\_ZlibDeflate** and **Tcl\_ZlibInflate** respectively compress and decompress the data contained in the *dataObj* argument, according to the *format* and, for compression, *level* arguments. The dictionary in the *dictObj* parameter is used to convey additional header information about the compressed data when the compression format supports it; currently, the dictionary is only used when the *format* parameter is **TCL\_ZLIB\_FORMAT\_GZIP** or **TCL\_ZLIB\_FORMAT\_AUTO**. For details of the contents of the dictionary, see the [Gzip options dictionary] section below. Upon success, both functions leave the resulting compressed or decompressed data in a byte-array value that is the Tcl interpreter's result; the returned value is a standard Tcl result code.

**Tcl\_ZlibAdler32** and **Tcl\_ZlibCRC32** compute checksums on arrays of bytes, returning the computed checksum. Checksums are computed incrementally, allowing data to be processed one block at a time, but this requires the caller to maintain the current checksum and pass it in as the *initValue* parameter; the initial value to use for this can be obtained by using NULL for the *bytes* parameter instead of a pointer to the array of bytes to compute the checksum over. Thus, typical usage in the single data block case is like this:

```
checksum = Tcl_ZlibCRC32(Tcl_ZlibCRC32(0,NULL,0), data, length);
```

Note that the Adler-32 algorithm is not a real checksum, but instead is a related type of hash that works best on longer data.

## Zlib streams

**Tcl\_ZlibStreamInit** creates a compressing or decompressing stream that is linked to a Tcl command, according to its arguments, and provides an abstract token for the stream and returns a normal Tcl result code; **Tcl\_ZlibStreamGetCommandName** returns the name of that command given the stream token, or NULL if the stream has no command. Streams are not designed to be thread-safe; each stream should only ever be used from the thread that created it. When working with gzip streams, a dictionary (fields as given in the [Gzip options dictionary] section below) can be given via the *dictObj* parameter that on compression allows control over the generated headers, and on decompression allows discovery of the existing headers. Note that the dictionary will be written to on decompression once sufficient data has been read to have a complete header. This means that the dictionary must be an unshared value in that case; a blank value created with **Tcl\_NewObj** is suggested.

Once a stream has been constructed, **Tcl\_ZlibStreamPut** is used to add data to the stream and **Tcl\_ZlibStreamGet** is used to retrieve data from the stream after processing. Both return normal Tcl result codes and leave an error message in the result of the interpreter that the stream is registered with in the error case (if such a registration has been performed). With **Tcl\_ZlibStreamPut**, the data buffer value passed to it should not be modified afterwards. With **Tcl\_ZlibStreamGet**, the data buffer value passed to it will have the data bytes appended to it. Internally to the stream, data is kept compressed so as to minimize the cost of buffer space.

**Tcl\_ZlibStreamChecksum** returns the checksum computed over the uncompressed data according to the format, and **Tcl\_ZlibStreamEof** returns a boolean value indicating whether the end of the uncompressed data has been reached.

**Tcl\_ZlibStreamSetCompressionDictionary** is used to control the compression dictionary used with the stream, a compression dictionary being an array of bytes (such as might be created with **Tcl\_NewByteArrayObj**) that is used to initialize the compression engine rather than leaving it to create it on the fly from the data being compressed. Setting a compression dictionary allows for more efficient compression in the case where the start of the data is highly regular, but it does require both the compressor and the decompressor to agree on the value to use. Compression dictionaries are only fully supported for zlib-format data; on compression, they must be set before any data is sent in with **Tcl\_ZlibStreamPut**, and on decompression they should be set when **Tcl\_ZlibStreamGet** produces an [error] with its **-errorcode** set to "**ZLIB NEED\_DICT** *code*"; the *code* will be the Adler-32 checksum (see **Tcl\_ZlibAdler32**) of the compression dictionary sought. (Note that this is only true for zlib-format streams; gzip streams ignore compression dictionaries as the format specification doesn't permit them, and raw streams just produce a data error if the compression dictionary is missing or incorrect.)

If you wish to clear a stream and reuse it for a new compression or decompression action, **Tcl\_ZlibStreamReset** will do this and return a normal Tcl result code to indicate whether it was successful; if the stream is registered with an interpreter, an error message will be left in the interpreter result when this function returns TCL\_ERROR. Finally, **Tcl\_ZlibStreamClose** will clean up the stream and delete the associated command: using **Tcl\_DeleteCommand** on the stream's command is equivalent (when such a command exists).

# Gzip options dictionary

The *dictObj* parameter to **Tcl\_ZlibDeflate**, **Tcl\_ZlibInflate** and **Tcl\_ZlibStreamInit** is used to pass a dictionary of options about that is used to describe the gzip header in the compressed data. When creating compressed data, the dictionary is read and when unpacking compressed data the dictionary is written (in which case the *dictObj* parameter must refer to an unshared dictionary value).

The following fields in the dictionary value are understood. All other fields are ignored. No field is required when creating a gzip-format stream.

**comment**
: This holds the comment field of the header, if present. If absent, no comment was supplied (on decompression) or will be created (on compression).

**crc**
: A boolean value describing whether a CRC of the header is computed. Note that the **gzip** program does *not* use or allow a CRC on the header.

**filename**
: The name of the file that held the uncompressed data. This should not contain any directory separators, and should be sanitized before use on decompression with [file tail][file].

**os**
: The operating system type code field from the header (if not the "unknown" value). See RFC 1952 for the meaning of these codes. On compression, if this is absent then the field will be set to the "unknown" value.

**size**
: The size of the uncompressed data. This is ignored on compression; the size of the data compressed depends on how much data is supplied to the compression engine.

[time]
: The time field from the header if non-zero, expected to be the time that the file named by the **filename** field was modified. Suitable for use with [clock format][clock]. On creation, the right value to use is that from [clock seconds][clock] or [file mtime][file].

**type**
: The type of the uncompressed data (either **binary** or **text**) if known.


# Reference count management

**Tcl\_ZlibDeflate** and **Tcl\_ZlibInflate** take a value with arbitrary reference count for their *dataObj* and *dictObj* arguments (the latter often being NULL instead), and set the interpreter result with their output value (or an error).  The existing interpreter result should not be passed as any argument value unless an additional reference is held.

**Tcl\_ZlibStreamInit** takes a value with arbitrary reference count for its *dictObj* argument; it only reads from it. The existing interpreter result should not be passed unless an additional reference is held.

**Tcl\_ZlibStreamGetCommandName** returns a zero reference count value, much like **Tcl\_NewObj**.

The *dataObj* argument to **Tcl\_ZlibStreamPut** is a value with arbitrary reference count; it is only ever read from.

The *dataObj* argument to **Tcl\_ZlibStreamGet** is an unshared value (see **Tcl\_IsShared**) that will be updated by the function.

The *compDict* argument to **Tcl\_ZlibStreamSetCompressionDictionary**, if non-NULL, may be duplicated or may have its reference count incremented. Using a zero reference count value is not recommended. 

# Portability notes

These functions will fail gracefully if Tcl is not linked with the zlib library.


[clock]: clock.md
[error]: error.md
[file]: file.md
[time]: time.md

