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

The \fBzlib\fR command provides access to the compression and check-summing facilities of the Zlib library by Jean-loup Gailly and Mark Adler. It has the following subcommands.

## Compression subcommands

**zlib compress** \fIstring\fR ?\fIlevel\fR?
: Returns the zlib-format compressed binary data of the binary string in \fIstring\fR. If present, \fIlevel\fR gives the compression level to use (from 0, which is uncompressed, to 9, maximally compressed).

**zlib decompress** \fIstring\fR ?\fIbufferSize\fR?
: Returns the uncompressed version of the raw compressed binary data in \fIstring\fR. If present, \fIbufferSize\fR is a hint as to what size of buffer is to be used to receive the data.

**zlib deflate** \fIstring\fR ?\fIlevel\fR?
: Returns the raw compressed binary data of the binary string in \fIstring\fR. If present, \fIlevel\fR gives the compression level to use (from 0, which is uncompressed, to 9, maximally compressed).

**zlib gunzip** \fIstring\fR ?\fB-headerVar\fR \fIvarName\fR?
: Return the uncompressed contents of binary string \fIstring\fR, which must have been in gzip format. If \fB-headerVar\fR is given, store a dictionary describing the contents of the gzip header in the variable called \fIvarName\fR. The keys of the dictionary that may be present are:

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
: The time field from the header if non-zero, expected to be time that the file named by the \fBfilename\fR field was modified. Suitable for use with \fBclock format\fR.

**type**
: The type of the uncompressed data (\fBbinary\fR or \fBtext\fR) if known.


**zlib gzip** \fIstring\fR ?\fB-level\fR \fIlevel\fR? ?\fB-header\fR \fIdict\fR?
: Return the compressed contents of binary string \fIstring\fR in gzip format. If \fB-level\fR is given, \fIlevel\fR gives the compression level to use (from 0, which is uncompressed, to 9, maximally compressed). If \fB-header\fR is given, \fIdict\fR is a dictionary containing values used for the gzip header. The following keys may be defined:

**comment**
: Add the given comment to the header of the gzip-format data.

**crc**
: A boolean saying whether to compute a CRC of the header. Note that if the data is to be interchanged with the \fBgzip\fR program, a header CRC should \fInot\fR be computed.

**filename**
: The name of the file that the data to be compressed came from.

**os**
: The operating system type code, which should be one of the values described in RFC 1952.

**time**
: The time that the file named in the \fBfilename\fR key was last modified. This will be in the same as is returned by \fBclock seconds\fR or \fBfile mtime\fR.

**type**
: The type of the data being compressed, being \fBbinary\fR or \fBtext\fR.


**zlib inflate** \fIstring\fR ?\fIbufferSize\fR?
: Returns the uncompressed version of the raw compressed binary data in \fIstring\fR. If present, \fIbufferSize\fR is a hint as to what size of buffer is to be used to receive the data.


## Channel subcommand

**zlib push** \fImode channel\fR ?\fIoptions ...\fR?
: Pushes a compressing or decompressing transformation onto the channel \fIchannel\fR. The transformation can be removed again with \fBchan pop\fR. The \fImode\fR argument determines what type of transformation is pushed; the following are supported:

**compress**
: The transformation will be a compressing transformation that produces zlib-format data on \fIchannel\fR, which must be writable.

**decompress**
: The transformation will be a decompressing transformation that reads zlib-format data from \fIchannel\fR, which must be readable.

**deflate**
: The transformation will be a compressing transformation that produces raw compressed data on \fIchannel\fR, which must be writable.

**gunzip**
: The transformation will be a decompressing transformation that reads gzip-format data from \fIchannel\fR, which must be readable.

**gzip**
: The transformation will be a compressing transformation that produces gzip-format data on \fIchannel\fR, which must be writable.

**inflate**
: The transformation will be a decompressing transformation that reads raw compressed data from \fIchannel\fR, which must be readable.

    The following options may be set when creating a transformation via the "*options ...*" to the \fBzlib push\fR command:

**-dictionary** \fIbinData\fR
: Sets the compression dictionary to use when working with compressing or decompressing the data to be \fIbinData\fR. Not valid for transformations that work with gzip-format data.  The dictionary should consist of strings (byte sequences) that are likely to be encountered later in the data to be compressed, with the most commonly used strings preferably put towards the end of the dictionary. Tcl provides no mechanism for choosing a good such dictionary for a particular data sequence.

**-header** \fIdictionary\fR
: Passes a description of the gzip header to create, in the same format that \fBzlib gzip\fR understands.

**-level** \fIcompressionLevel\fR
: How hard to compress the data. Must be an integer from 0 (uncompressed) to 9 (maximally compressed).

**-limit** \fIreadaheadLimit\fR
: The maximum number of bytes ahead to read when decompressing.

    This option has become \fBirrelevant\fR. It was originally introduced to prevent Tcl from reading beyond the end of a compressed stream in multi-stream channels to ensure that the data after was left alone for further reading, at the cost of speed.
    Tcl now automatically returns any bytes it has read beyond the end of a compressed stream back to the channel, making them appear as unread to further readers.


Both compressing and decompressing channel transformations add extra configuration options that may be accessed through \fBchan configure\fR. The options are:

**-checksum** \fIchecksum\fR
: This read-only option gets the current checksum for the uncompressed data that the compression engine has seen so far. It is valid for both compressing and decompressing transforms, but not for the raw inflate and deflate formats. The compression algorithm depends on what format is being produced or consumed.

**-dictionary** \fIbinData\fR
: This read-write options gets or sets the initial compression dictionary to use when working with compressing or decompressing the data to be \fIbinData\fR. It is not valid for transformations that work with gzip-format data, and should not normally be set on compressing transformations other than at the point where the transformation is stacked. Note that this cannot be used to get the current active compression dictionary mid-stream, as that information is not exposed by the underlying library.

**-flush** \fItype\fR
: This write-only operation flushes the current state of the compressor to the underlying channel. It is only valid for compressing transformations. The \fItype\fR must be either \fBsync\fR or \fBfull\fR for a normal flush or an expensive flush respectively. Flushing degrades the compression ratio, but makes it easier for a decompressor to recover more of the file in the case of data corruption.

**-header** \fIdictionary\fR
: This read-only option, only valid for decompressing transforms that are processing gzip-format data, returns the dictionary describing the header read off the data stream.

**-limit** \fIreadaheadLimit\fR
: This read-write option is used by decompressing channels to control the maximum number of bytes ahead to read from the underlying data source. See above for more information.


## Streaming subcommand

**zlib stream** \fImode\fR ?\fIoptions\fR?
: Creates a streaming compression or decompression command based on the \fImode\fR, and return the name of the command. For a description of how that command works, see \fBSTREAMING INSTANCE COMMAND\fR below. The following modes and \fIoptions\fR are supported:

**zlib stream compress** ?\fB-dictionary\fR \fIbindata\fR? ?\fB-level\fR \fIlevel\fR?
: The stream will be a compressing stream that produces zlib-format output, using compression level \fIlevel\fR (if specified) which will be an integer from 0 to 9, and the compression dictionary \fIbindata\fR (if specified).

**zlib stream decompress** ?\fB-dictionary\fR \fIbindata\fR?
: The stream will be a decompressing stream that takes zlib-format input and produces uncompressed output. If \fIbindata\fR is supplied, it is a compression dictionary to use if required.

**zlib stream deflate** ?\fB-dictionary\fR \fIbindata\fR? ?\fB-level\fR \fIlevel\fR?
: The stream will be a compressing stream that produces raw output, using compression level \fIlevel\fR (if specified) which will be an integer from 0 to 9, and the compression dictionary \fIbindata\fR (if specified). Note that the raw compressed data includes no metadata about what compression dictionary was used, if any; that is a feature of the zlib-format data.

**zlib stream gunzip**
: The stream will be a decompressing stream that takes gzip-format input and produces uncompressed output.

**zlib stream gzip** ?\fB-header\fR \fIheader\fR? ?\fB-level\fR \fIlevel\fR?
: The stream will be a compressing stream that produces gzip-format output, using compression level \fIlevel\fR (if specified) which will be an integer from 0 to 9, and the header descriptor dictionary \fIheader\fR (if specified; for keys see \fBzlib gzip\fR).

**zlib stream inflate** ?\fB-dictionary\fR \fIbindata\fR?
: The stream will be a decompressing stream that takes raw compressed input and produces uncompressed output. If \fIbindata\fR is supplied, it is a compression dictionary to use. Note that there are no checks in place to determine whether the compression dictionary is correct.



## Checksumming subcommands

**zlib adler32** \fIstring\fR ?\fIinitValue\fR?
: Compute a checksum of binary string \fIstring\fR using the Adler-32 algorithm. If given, \fIinitValue\fR is used to initialize the checksum engine.

**zlib crc32** \fIstring\fR ?\fIinitValue\fR?
: Compute a checksum of binary string \fIstring\fR using the CRC-32 algorithm. If given, \fIinitValue\fR is used to initialize the checksum engine.


# Streaming instance command

Streaming compression instance commands are produced by the \fBzlib stream\fR command. They are used by calling their \fBput\fR subcommand one or more times to load data in, and their \fBget\fR subcommand one or more times to extract the transformed data.

The full set of subcommands supported by a streaming instance command, \fIstream\fR, is as follows:

*stream* \fBadd\fR ?\fIoption...\fR? \fIdata\fR
: A short-cut for "*stream* \fBput\fR ?\fIoption...\fR? \fIdata\fR" followed by "*stream* \fBget\fR".

*stream* \fBchecksum\fR
: Returns the checksum of the uncompressed data seen so far by this stream.

*stream* \fBclose\fR
: Deletes this stream and frees up all resources associated with it.

*stream* \fBeof\fR
: Returns a boolean indicating whether the end of the stream (as determined by the compressed data itself) has been reached. Not all formats support detection of the end of the stream.

*stream* \fBfinalize\fR
: A short-cut for "*stream* \fBput -finalize {}\fR".

*stream* \fBflush\fR
: A short-cut for "*stream* \fBput -flush {}\fR".

*stream* \fBfullflush\fR
: A short-cut for "*stream* \fBput -fullflush {}\fR".

*stream* \fBget\fR ?\fIcount\fR?
: Return up to \fIcount\fR bytes from \fIstream\fR's internal buffers with the transformation applied. If \fIcount\fR is omitted, the entire contents of the buffers are returned.

*stream* \fBheader\fR
: Return the gzip header description dictionary extracted from the stream. Only supported for streams created with their \fImode\fR parameter set to \fBgunzip\fR.

*stream* \fBput\fR ?\fIoption...\fR? \fIdata\fR
: Append the contents of the binary string \fIdata\fR to \fIstream\fR's internal buffers while applying the transformation. The following \fIoption\fRs are supported (or an unambiguous prefix of them), which are used to modify the way in which the transformation is applied:

**-dictionary** \fIbinData\fR
: Sets the compression dictionary to use when working with compressing or decompressing the data to be \fIbinData\fR.

**-finalize**
: Mark the stream as finished, ensuring that all bytes have been wholly compressed or decompressed. For gzip streams, this also ensures that the footer is written to the stream. The stream will need to be reset before having more data written to it after this, though data can still be read out of the stream with the \fBget\fR subcommand.

    This option is mutually exclusive with the \fB-flush\fR and \fB-fullflush\fR options.

**-flush**
: Ensure that a decompressor consuming the bytes that the current (compressing) stream is producing will be able to produce all the bytes that have been compressed so far, at some performance penalty.
    This option is mutually exclusive with the \fB-finalize\fR and \fB-fullflush\fR options.

**-fullflush**
: Ensure that not only can a decompressor handle all the bytes produced so far (as with \fB-flush\fR above) but also that it can restart from this point if it detects that the stream is partially corrupt. This incurs a substantial performance penalty.
    This option is mutually exclusive with the \fB-finalize\fR and \fB-flush\fR options.

*stream* \fBreset\fR
: Puts any stream, including those that have been finalized or that have reached eof, back into a state where it can process more data. Throws away all internally buffered data.


# Examples

To compress a Tcl string, it should be first converted to a particular charset encoding since the \fBzlib\fR command always operates on binary strings.

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

