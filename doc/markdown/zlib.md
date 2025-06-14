---
CommandName: zlib
ManualSection: n
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - binary(n)
 - chan(n)
 - encoding(n)
 - Tcl_ZlibDeflate(3)
 - RFC1950 \- RFC1952
Keywords:
 - compress
 - decompress
 - deflate
 - gzip
 - inflate
 - zlib
Copyright:
 - Copyright (c) 2008-2012 Donal K. Fellows
---

# Name

zlib - compression and decompression operations

# Synopsis

::: {.synopsis} :::
[zlib]{.cmd} [subcommand]{.arg} [arg]{.arg} [...]{.arg}
:::

# Description

The **zlib** command provides access to the compression and check-summing facilities of the Zlib library by Jean-loup Gailly and Mark Adler. It has the following subcommands.

## Compression subcommands

**zlib compress** *string* ?*level*?
: Returns the zlib-format compressed binary data of the binary string in *string*. If present, *level* gives the compression level to use (from 0, which is uncompressed, to 9, maximally compressed).

**zlib decompress** *string* ?*bufferSize*?
: Returns the uncompressed version of the raw compressed binary data in *string*. If present, *bufferSize* is a hint as to what size of buffer is to be used to receive the data.

**zlib deflate** *string* ?*level*?
: Returns the raw compressed binary data of the binary string in *string*. If present, *level* gives the compression level to use (from 0, which is uncompressed, to 9, maximally compressed).

**zlib gunzip** *string* ?**-headerVar** *varName*?
: Return the uncompressed contents of binary string *string*, which must have been in gzip format. If **-headerVar** is given, store a dictionary describing the contents of the gzip header in the variable called *varName*. The keys of the dictionary that may be present are:

**comment**
: The comment field from the header, if present.

**crc**
: A boolean value describing whether a CRC of the header is computed.

**filename**
: The filename field from the header, if present.

**os**
: The operating system type code field from the header (if not the QW unknown value). See RFC 1952 for the meaning of these codes.

**size**
: The size of the uncompressed data.

**time**
: The time field from the header if non-zero, expected to be time that the file named by the **filename** field was modified. Suitable for use with **clock format**.

**type**
: The type of the uncompressed data (**binary** or **text**) if known.


**zlib gzip** *string* ?**-level** *level*? ?**-header** *dict*?
: Return the compressed contents of binary string *string* in gzip format. If **-level** is given, *level* gives the compression level to use (from 0, which is uncompressed, to 9, maximally compressed). If **-header** is given, *dict* is a dictionary containing values used for the gzip header. The following keys may be defined:

**comment**
: Add the given comment to the header of the gzip-format data.

**crc**
: A boolean saying whether to compute a CRC of the header. Note that if the data is to be interchanged with the **gzip** program, a header CRC should *not* be computed.

**filename**
: The name of the file that the data to be compressed came from.

**os**
: The operating system type code, which should be one of the values described in RFC 1952.

**time**
: The time that the file named in the **filename** key was last modified. This will be in the same as is returned by **clock seconds** or **file mtime**.

**type**
: The type of the data being compressed, being **binary** or **text**.


**zlib inflate** *string* ?*bufferSize*?
: Returns the uncompressed version of the raw compressed binary data in *string*. If present, *bufferSize* is a hint as to what size of buffer is to be used to receive the data.


## Channel subcommand

**zlib push** *mode channel* ?*options ...*?
: Pushes a compressing or decompressing transformation onto the channel *channel*. The transformation can be removed again with **chan pop**. The *mode* argument determines what type of transformation is pushed; the following are supported:

**compress**
: The transformation will be a compressing transformation that produces zlib-format data on *channel*, which must be writable.

**decompress**
: The transformation will be a decompressing transformation that reads zlib-format data from *channel*, which must be readable.

**deflate**
: The transformation will be a compressing transformation that produces raw compressed data on *channel*, which must be writable.

**gunzip**
: The transformation will be a decompressing transformation that reads gzip-format data from *channel*, which must be readable.

**gzip**
: The transformation will be a compressing transformation that produces gzip-format data on *channel*, which must be writable.

**inflate**
: The transformation will be a decompressing transformation that reads raw compressed data from *channel*, which must be readable.

    The following options may be set when creating a transformation via the "*options ...*" to the **zlib push** command:

**-dictionary** *binData*
: Sets the compression dictionary to use when working with compressing or decompressing the data to be *binData*. Not valid for transformations that work with gzip-format data.  The dictionary should consist of strings (byte sequences) that are likely to be encountered later in the data to be compressed, with the most commonly used strings preferably put towards the end of the dictionary. Tcl provides no mechanism for choosing a good such dictionary for a particular data sequence.

**-header** *dictionary*
: Passes a description of the gzip header to create, in the same format that **zlib gzip** understands.

**-level** *compressionLevel*
: How hard to compress the data. Must be an integer from 0 (uncompressed) to 9 (maximally compressed).

**-limit** *readaheadLimit*
: The maximum number of bytes ahead to read when decompressing.

    This option has become **irrelevant**. It was originally introduced to prevent Tcl from reading beyond the end of a compressed stream in multi-stream channels to ensure that the data after was left alone for further reading, at the cost of speed.
    Tcl now automatically returns any bytes it has read beyond the end of a compressed stream back to the channel, making them appear as unread to further readers.


Both compressing and decompressing channel transformations add extra configuration options that may be accessed through **chan configure**. The options are:

**-checksum** *checksum*
: This read-only option gets the current checksum for the uncompressed data that the compression engine has seen so far. It is valid for both compressing and decompressing transforms, but not for the raw inflate and deflate formats. The compression algorithm depends on what format is being produced or consumed.

**-dictionary** *binData*
: This read-write options gets or sets the initial compression dictionary to use when working with compressing or decompressing the data to be *binData*. It is not valid for transformations that work with gzip-format data, and should not normally be set on compressing transformations other than at the point where the transformation is stacked. Note that this cannot be used to get the current active compression dictionary mid-stream, as that information is not exposed by the underlying library.

**-flush** *type*
: This write-only operation flushes the current state of the compressor to the underlying channel. It is only valid for compressing transformations. The *type* must be either **sync** or **full** for a normal flush or an expensive flush respectively. Flushing degrades the compression ratio, but makes it easier for a decompressor to recover more of the file in the case of data corruption.

**-header** *dictionary*
: This read-only option, only valid for decompressing transforms that are processing gzip-format data, returns the dictionary describing the header read off the data stream.

**-limit** *readaheadLimit*
: This read-write option is used by decompressing channels to control the maximum number of bytes ahead to read from the underlying data source. See above for more information.


## Streaming subcommand

**zlib stream** *mode* ?*options*?
: Creates a streaming compression or decompression command based on the *mode*, and return the name of the command. For a description of how that command works, see **STREAMING INSTANCE COMMAND** below. The following modes and *options* are supported:

**zlib stream compress** ?**-dictionary** *bindata*? ?**-level** *level*?
: The stream will be a compressing stream that produces zlib-format output, using compression level *level* (if specified) which will be an integer from 0 to 9, and the compression dictionary *bindata* (if specified).

**zlib stream decompress** ?**-dictionary** *bindata*?
: The stream will be a decompressing stream that takes zlib-format input and produces uncompressed output. If *bindata* is supplied, it is a compression dictionary to use if required.

**zlib stream deflate** ?**-dictionary** *bindata*? ?**-level** *level*?
: The stream will be a compressing stream that produces raw output, using compression level *level* (if specified) which will be an integer from 0 to 9, and the compression dictionary *bindata* (if specified). Note that the raw compressed data includes no metadata about what compression dictionary was used, if any; that is a feature of the zlib-format data.

**zlib stream gunzip**
: The stream will be a decompressing stream that takes gzip-format input and produces uncompressed output.

**zlib stream gzip** ?**-header** *header*? ?**-level** *level*?
: The stream will be a compressing stream that produces gzip-format output, using compression level *level* (if specified) which will be an integer from 0 to 9, and the header descriptor dictionary *header* (if specified; for keys see **zlib gzip**).

**zlib stream inflate** ?**-dictionary** *bindata*?
: The stream will be a decompressing stream that takes raw compressed input and produces uncompressed output. If *bindata* is supplied, it is a compression dictionary to use. Note that there are no checks in place to determine whether the compression dictionary is correct.



## Checksumming subcommands

**zlib adler32** *string* ?*initValue*?
: Compute a checksum of binary string *string* using the Adler-32 algorithm. If given, *initValue* is used to initialize the checksum engine.

**zlib crc32** *string* ?*initValue*?
: Compute a checksum of binary string *string* using the CRC-32 algorithm. If given, *initValue* is used to initialize the checksum engine.


# Streaming instance command

Streaming compression instance commands are produced by the **zlib stream** command. They are used by calling their **put** subcommand one or more times to load data in, and their **get** subcommand one or more times to extract the transformed data.

The full set of subcommands supported by a streaming instance command, *stream*, is as follows:

*stream* **add** ?*option...*? *data*
: A short-cut for "*stream* **put** ?*option...*? *data*" followed by "*stream* **get**".

*stream* **checksum**
: Returns the checksum of the uncompressed data seen so far by this stream.

*stream* **close**
: Deletes this stream and frees up all resources associated with it.

*stream* **eof**
: Returns a boolean indicating whether the end of the stream (as determined by the compressed data itself) has been reached. Not all formats support detection of the end of the stream.

*stream* **finalize**
: A short-cut for "*stream* **put -finalize {}**".

*stream* **flush**
: A short-cut for "*stream* **put -flush {}**".

*stream* **fullflush**
: A short-cut for "*stream* **put -fullflush {}**".

*stream* **get** ?*count*?
: Return up to *count* bytes from *stream*'s internal buffers with the transformation applied. If *count* is omitted, the entire contents of the buffers are returned.

*stream* **header**
: Return the gzip header description dictionary extracted from the stream. Only supported for streams created with their *mode* parameter set to **gunzip**.

*stream* **put** ?*option...*? *data*
: Append the contents of the binary string *data* to *stream*'s internal buffers while applying the transformation. The following *option*s are supported (or an unambiguous prefix of them), which are used to modify the way in which the transformation is applied:

**-dictionary** *binData*
: Sets the compression dictionary to use when working with compressing or decompressing the data to be *binData*.

**-finalize**
: Mark the stream as finished, ensuring that all bytes have been wholly compressed or decompressed. For gzip streams, this also ensures that the footer is written to the stream. The stream will need to be reset before having more data written to it after this, though data can still be read out of the stream with the **get** subcommand.

    This option is mutually exclusive with the **-flush** and **-fullflush** options.

**-flush**
: Ensure that a decompressor consuming the bytes that the current (compressing) stream is producing will be able to produce all the bytes that have been compressed so far, at some performance penalty.
    This option is mutually exclusive with the **-finalize** and **-fullflush** options.

**-fullflush**
: Ensure that not only can a decompressor handle all the bytes produced so far (as with **-flush** above) but also that it can restart from this point if it detects that the stream is partially corrupt. This incurs a substantial performance penalty.
    This option is mutually exclusive with the **-finalize** and **-flush** options.

*stream* **reset**
: Puts any stream, including those that have been finalized or that have reached eof, back into a state where it can process more data. Throws away all internally buffered data.


# Examples

To compress a Tcl string, it should be first converted to a particular charset encoding since the **zlib** command always operates on binary strings.

```
set binData [encoding convertto utf-8 $string]
set compData [zlib compress $binData]
```

When converting back, it is also important to reverse the charset encoding:

```
set binData [zlib decompress $compData]
set string [encoding convertfrom utf-8 $binData]
```

The compression operation from above can also be done with streams, which is especially helpful when you want to accumulate the data by stages:

```
set strm [zlib stream compress]
$strm put [encoding convertto utf-8 $string]
# ...
$strm finalize
set compData [$strm get]
$strm close
```

