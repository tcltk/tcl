---
CommandName: Tcl_OpenFileChannel
ManualSection: 3
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - DString(3)
 - fconfigure(n)
 - filename(n)
 - fopen(3)
 - Tcl_CreateChannel(3)
Keywords:
 - access point
 - blocking
 - buffered I/O
 - channel
 - channel driver
 - end of file
 - flush
 - input
 - nonblocking
 - output
 - read
 - seek
 - write
Copyright:
 - Copyright (c) 1996-1997 Sun Microsystems, Inc.
---

# Name

Tcl\_OpenFileChannel, Tcl\_OpenCommandChannel, Tcl\_MakeFileChannel, Tcl\_GetChannel, Tcl\_GetChannelNames, Tcl\_GetChannelNamesEx, Tcl\_RegisterChannel, Tcl\_UnregisterChannel, Tcl\_DetachChannel, Tcl\_IsStandardChannel, Tcl\_Close, Tcl\_CloseEx, Tcl\_ReadChars, Tcl\_Read, Tcl\_GetsObj, Tcl\_Gets, Tcl\_WriteObj, Tcl\_WriteChars, Tcl\_Write, Tcl\_Flush, Tcl\_Seek, Tcl\_Tell, Tcl\_TruncateChannel, Tcl\_GetChannelOption, Tcl\_SetChannelOption, Tcl\_Eof, Tcl\_InputBlocked, Tcl\_InputBuffered, Tcl\_OutputBuffered, Tcl\_Ungets, Tcl\_ReadRaw, Tcl\_WriteRaw - buffered I/O facilities using channels

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Channel]{.ret} [Tcl\_OpenFileChannel]{.ccmd}[interp, fileName, mode, permissions]{.cargs}
[Tcl\_Channel]{.ret} [Tcl\_OpenCommandChannel]{.ccmd}[interp, argc, argv, flags]{.cargs}
[Tcl\_Channel]{.ret} [Tcl\_MakeFileChannel]{.ccmd}[handle, readOrWrite]{.cargs}
[Tcl\_Channel]{.ret} [Tcl\_GetChannel]{.ccmd}[interp, channelName, modePtr]{.cargs}
[int]{.ret} [Tcl\_GetChannelNames]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl\_GetChannelNamesEx]{.ccmd}[interp, pattern]{.cargs}
[Tcl\_RegisterChannel]{.ccmd}[interp, channel]{.cargs}
[int]{.ret} [Tcl\_UnregisterChannel]{.ccmd}[interp, channel]{.cargs}
[int]{.ret} [Tcl\_DetachChannel]{.ccmd}[interp, channel]{.cargs}
[int]{.ret} [Tcl\_IsStandardChannel]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl\_Close]{.ccmd}[interp, channel]{.cargs}
[int]{.ret} [Tcl\_CloseEx]{.ccmd}[interp, channel, closeFlags]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_ReadChars]{.ccmd}[channel, readObjPtr, charsToRead, appendFlag]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_Read]{.ccmd}[channel, readBuf, bytesToRead]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_GetsObj]{.ccmd}[channel, lineObjPtr]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_Gets]{.ccmd}[channel, lineRead]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_Ungets]{.ccmd}[channel, input, inputLen, addAtEnd]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_WriteObj]{.ccmd}[channel, writeObjPtr]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_WriteChars]{.ccmd}[channel, charBuf, bytesToWrite]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_Write]{.ccmd}[channel, byteBuf, bytesToWrite]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_ReadRaw]{.ccmd}[channel, readBuf, bytesToRead]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_WriteRaw]{.ccmd}[channel, byteBuf, bytesToWrite]{.cargs}
[int]{.ret} [Tcl\_Eof]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl\_Flush]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl\_InputBlocked]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl\_InputBuffered]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl\_OutputBuffered]{.ccmd}[channel]{.cargs}
[long long]{.ret} [Tcl\_Seek]{.ccmd}[channel, offset, seekMode]{.cargs}
[long long]{.ret} [Tcl\_Tell]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl\_TruncateChannel]{.ccmd}[channel, length]{.cargs}
[int]{.ret} [Tcl\_GetChannelOption]{.ccmd}[interp, channel, optionName, optionValue]{.cargs}
[int]{.ret} [Tcl\_SetChannelOption]{.ccmd}[interp, channel, optionName, newValue]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Used for error reporting and to look up a channel registered in it. .AP "const char" \*fileName in The name of a local or network file. .AP "const char" \*mode in Specifies how the file is to be accessed.  May have any of the values allowed for the *mode* argument to the Tcl [open] command. .AP int permissions in POSIX-style permission flags such as 0644.  If a new file is created, these permissions will be set on the created file. .AP Tcl\_Size argc in The number of elements in *argv*. .AP "const char" \*\*argv in Arguments for constructing a command pipeline.  These values have the same meaning as the non-switch arguments to the Tcl [exec] command. .AP int flags in Specifies the disposition of the stdio handles in pipeline: OR-ed combination of **TCL\_STDIN**, **TCL\_STDOUT**, **TCL\_STDERR**, and **TCL\_ENFORCE\_MODE**. If **TCL\_STDIN** is set, stdin for the first child in the pipe is the pipe channel, otherwise it is the same as the standard input of the invoking process; likewise for **TCL\_STDOUT** and **TCL\_STDERR**. If **TCL\_ENFORCE\_MODE** is not set, then the pipe can redirect stdio handles to override the stdio handles for which **TCL\_STDIN**, **TCL\_STDOUT** and **TCL\_STDERR** have been set.  If it is set, then such redirections cause an error. .AP void \*handle in Operating system specific handle for I/O to a file. For Unix this is a file descriptor, for Windows it is a HANDLE. .AP int readOrWrite in OR-ed combination of **TCL\_READABLE** and **TCL\_WRITABLE** to indicate what operations are valid on *handle*. .AP "const char" \*channelName in The name of the channel. .AP int \*modePtr out Points at an integer variable that will receive an OR-ed combination of **TCL\_READABLE** and **TCL\_WRITABLE** denoting whether the channel is open for reading and writing. .AP "const char" \*pattern in The pattern to match on, passed to Tcl\_StringMatch, or NULL. .AP Tcl\_Channel channel in A Tcl channel for input or output.  Must have been the return value from a procedure such as **Tcl\_OpenFileChannel**. .AP Tcl\_Obj \*readObjPtr in/out A pointer to a Tcl value in which to store the characters read from the channel. .AP Tcl\_Size charsToRead in The number of characters to read from the channel.  If the channel's encoding is [binary], this is equivalent to the number of bytes to read from the channel. .AP int closeFlags in If **0**, the channel is closed in both directions. If **TCL\_CLOSE\_READ**, the channel is only closed for reading. If **TCL\_CLOSE\_WRITE**, the channel is only closed for writing. These flags must not be combined. .AP int appendFlag in If non-zero, data read from the channel will be appended to the value. Otherwise, the data will replace the existing contents of the value. .AP char \*readBuf out A buffer in which to store the bytes read from the channel. .AP Tcl\_Size bytesToRead in The number of bytes to read from the channel.  The buffer *readBuf* must be large enough to hold this many bytes. .AP Tcl\_Obj \*lineObjPtr in/out A pointer to a Tcl value in which to store the line read from the channel.  The line read will be appended to the current value of the value. .AP Tcl\_DString \*lineRead in/out A pointer to a Tcl dynamic string in which to store the line read from the channel.  Must have been initialized by the caller.  The line read will be appended to any data already in the dynamic string. .AP "const char" \*input in The input to add to a channel buffer. .AP Tcl\_Size inputLen in Length of the input .AP int addAtEnd in Flag indicating whether the input should be added to the end or beginning of the channel buffer. .AP Tcl\_Obj \*writeObjPtr in A pointer to a Tcl value whose contents will be output to the channel. .AP "const char" \*charBuf in A buffer containing the characters to output to the channel. .AP "const char" \*byteBuf in A buffer containing the bytes to output to the channel. .AP Tcl\_Size bytesToWrite in The number of bytes to consume from *charBuf* or *byteBuf* and output to the channel. .AP "long long" offset in How far to move the access point in the channel at which the next input or output operation will be applied, measured in bytes from the position given by *seekMode*.  May be either positive or negative. .AP int seekMode in Relative to which point to seek; used with *offset* to calculate the new access point for the channel. Legal values are **SEEK\_SET**, **SEEK\_CUR**, and **SEEK\_END**. .AP "long long" length in The (non-negative) length to truncate the channel the channel to. .AP "const char" \*optionName in The name of an option applicable to this channel, such as **-blocking**. May have any of the values accepted by the [fconfigure] command. .AP Tcl\_DString \*optionValue in Where to store the value of an option or a list of all options and their values. Must have been initialized by the caller. .AP "const char" \*newValue in New value for the option given by *optionName*.

# Description

N.B. Refer to the **Tcl\_UniChar** documentation page for a description of the *TUTF-8* encoding and related terms referenced here.

The Tcl channel mechanism provides a device-independent and platform-independent mechanism for performing buffered input and output operations on a variety of file, socket, and device types. The channel mechanism is extensible to new channel types, by providing a low-level channel driver for the new type; the channel driver interface is described in the manual entry for **Tcl\_CreateChannel**. The channel mechanism provides a buffering scheme modeled after Unix's standard I/O, and it also allows for nonblocking I/O on channels.

The procedures described in this manual entry comprise the C APIs of the generic layer of the channel architecture. For a description of the channel driver architecture and how to implement channel drivers for new types of channels, see the manual entry for **Tcl\_CreateChannel**.

# Tcl\_openfilechannel

**Tcl\_OpenFileChannel** opens a file specified by *fileName* and returns a channel handle that can be used to perform input and output on the file. This API is modeled after the **fopen** procedure of the Unix standard I/O library. The syntax and meaning of all arguments is similar to those given in the Tcl [open] command when opening a file. If an error occurs while opening the channel, **Tcl\_OpenFileChannel** returns NULL and records a POSIX error code that can be retrieved with **Tcl\_GetErrno**. In addition, if *interp* is non-NULL, **Tcl\_OpenFileChannel** leaves an error message in *interp*'s result after any error. As of Tcl 8.4, the value-based API **Tcl\_FSOpenFileChannel** should be used in preference to **Tcl\_OpenFileChannel** wherever possible.

The newly created channel is not registered in the supplied interpreter; to register it, use **Tcl\_RegisterChannel**, described below. If one of the standard channels, **stdin**, **stdout** or **stderr** was previously closed, the act of creating the new channel also assigns it as a replacement for the standard channel.

# Tcl\_opencommandchannel

**Tcl\_OpenCommandChannel** provides a C-level interface to the functions of the [exec] and [open] commands. It creates a sequence of subprocesses specified by the *argv* and *argc* arguments and returns a channel that can be used to communicate with these subprocesses. The *flags* argument indicates what sort of communication will exist with the command pipeline.

If the **TCL\_STDIN** flag is set then the standard input for the first subprocess will be tied to the channel: writing to the channel will provide input to the subprocess.  If **TCL\_STDIN** is not set, then standard input for the first subprocess will be the same as this application's standard input.  If **TCL\_STDOUT** is set then standard output from the last subprocess can be read from the channel; otherwise it goes to this application's standard output.  If **TCL\_STDERR** is set, standard error output for all subprocesses is returned to the channel and results in an error when the channel is closed; otherwise it goes to this application's standard error.  If **TCL\_ENFORCE\_MODE** is not set, then *argc* and *argv* can redirect the stdio handles to override **TCL\_STDIN**, **TCL\_STDOUT**, and **TCL\_STDERR**; if it is set, then it is an error for argc and argv to override stdio channels for which **TCL\_STDIN**, **TCL\_STDOUT**, and **TCL\_STDERR** have been set.

If an error occurs while opening the channel, **Tcl\_OpenCommandChannel** returns NULL and records a POSIX error code that can be retrieved with **Tcl\_GetErrno**. In addition, **Tcl\_OpenCommandChannel** leaves an error message in the interpreter's result. *interp* cannot be NULL.

The newly created channel is not registered in the supplied interpreter; to register it, use **Tcl\_RegisterChannel**, described below. If one of the standard channels, **stdin**, **stdout** or **stderr** was previously closed, the act of creating the new channel also assigns it as a replacement for the standard channel.

# Tcl\_makefilechannel

**Tcl\_MakeFileChannel** makes a **Tcl\_Channel** from an existing, platform-specific, file handle. The newly created channel is not registered in the supplied interpreter; to register it, use **Tcl\_RegisterChannel**, described below. If one of the standard channels, **stdin**, **stdout** or **stderr** was previously closed, the act of creating the new channel also assigns it as a replacement for the standard channel.

# Tcl\_getchannel

**Tcl\_GetChannel** returns a channel given the *channelName* used to create it with **Tcl\_CreateChannel** and a pointer to a Tcl interpreter in *interp*. If a channel by that name is not registered in that interpreter, the procedure returns NULL. If the *modePtr* argument is not NULL, it points at an integer variable that will receive an OR-ed combination of **TCL\_READABLE** and **TCL\_WRITABLE** describing whether the channel is open for reading and writing.

**Tcl\_GetChannelNames** and **Tcl\_GetChannelNamesEx** write the names of the registered channels to the interpreter's result as a list value.  **Tcl\_GetChannelNamesEx** will filter these names according to the *pattern*.  If *pattern* is NULL, then it will not do any filtering.  The return value is **TCL\_OK** if no errors occurred writing to the result, otherwise it is **TCL\_ERROR**, and the error message is left in the interpreter's result.

# Tcl\_registerchannel

**Tcl\_RegisterChannel** adds a channel to the set of channels accessible in *interp*. After this call, Tcl programs executing in that interpreter can refer to the channel in input or output operations using the name given in the call to **Tcl\_CreateChannel**.  After this call, the channel becomes the property of the interpreter, and the caller should not call **Tcl\_Close** for the channel; the channel will be closed automatically when it is unregistered from the interpreter.

Code executing outside of any Tcl interpreter can call **Tcl\_RegisterChannel** with *interp* as NULL, to indicate that it wishes to hold a reference to this channel. Subsequently, the channel can be registered in a Tcl interpreter and it will only be closed when the matching number of calls to **Tcl\_UnregisterChannel** have been made. This allows code executing outside of any interpreter to safely hold a reference to a channel that is also registered in a Tcl interpreter.

This procedure interacts with the code managing the standard channels. If no standard channels were initialized before the first call to **Tcl\_RegisterChannel**, they will get initialized by that call. See **Tcl\_StandardChannels** for a general treatise about standard channels and the behavior of the Tcl library with regard to them.

# Tcl\_unregisterchannel

**Tcl\_UnregisterChannel** removes a channel from the set of channels accessible in *interp*. After this call, Tcl programs will no longer be able to use the channel's name to refer to the channel in that interpreter. If this operation removed the last registration of the channel in any interpreter, the channel is also closed and destroyed.

Code not associated with a Tcl interpreter can call **Tcl\_UnregisterChannel** with *interp* as NULL, to indicate to Tcl that it no longer holds a reference to that channel. If this is the last reference to the channel, it will now be closed.  **Tcl\_UnregisterChannel** is very similar to **Tcl\_DetachChannel** except that it will also close the channel if no further references to it exist.

# Tcl\_detachchannel

**Tcl\_DetachChannel** removes a channel from the set of channels accessible in *interp*. After this call, Tcl programs will no longer be able to use the channel's name to refer to the channel in that interpreter. Beyond that, this command has no further effect.  It cannot be used on the standard channels (**stdout**, **stderr**, **stdin**), and will return **TCL\_ERROR** if passed one of those channels.

Code not associated with a Tcl interpreter can call **Tcl\_DetachChannel** with *interp* as NULL, to indicate to Tcl that it no longer holds a reference to that channel. If this is the last reference to the channel, unlike **Tcl\_UnregisterChannel**, it will not be closed.

# Tcl\_isstandardchannel

**Tcl\_IsStandardChannel** tests whether a channel is one of the three standard channels, **stdin**, **stdout** or **stderr**. If so, it returns 1, otherwise 0.

No attempt is made to check whether the given channel or the standard channels are initialized or otherwise valid.

# Tcl\_close and tcl\_closeex

**Tcl\_Close** destroys the channel *channel*, which must denote a currently open channel. The channel should not be registered in any interpreter when **Tcl\_Close** is called. Buffered output is flushed to the channel's output device prior to destroying the channel, and any buffered input is discarded.  If this is a blocking channel, the call does not return until all buffered data is successfully sent to the channel's output device.  If this is a nonblocking channel and there is buffered output that cannot be written without blocking, the call returns immediately; output is flushed in the background and the channel will be closed once all of the buffered data has been output.  In this case errors during flushing are not reported.

**Tcl\_CloseEx** allows for both full closing and half-closing of channels depending on its **closeFlags** parameter. See the description of the parameter above. It is an error to attempt to close the channel for a direction for which it is not open. The channel is destroyed only when it has been closed for both reading and writing. Only socket and command pipe channels support half-closing.

If the channel was closed successfully, **Tcl\_Close** and **Tcl\_CloseEx** return **TCL\_OK**. If an error occurs, they return **TCL\_ERROR** and record a POSIX error code that can be retrieved with **Tcl\_GetErrno**. If the channel is being closed synchronously and an error occurs during closing of the channel and *interp* is not NULL, an error message is left in the interpreter's result.

Note that it is not safe to call the channel closing functions on a channel that has been registered using **Tcl\_RegisterChannel**; see the documentation for **Tcl\_RegisterChannel**, above, for details. If the channel has ever been given as the [chan] argument in a call to **Tcl\_RegisterChannel**, you should instead use **Tcl\_UnregisterChannel**, which will internally call **Tcl\_Close** when all calls to **Tcl\_RegisterChannel** have been matched by corresponding calls to **Tcl\_UnregisterChannel**.

# Tcl\_readchars and tcl\_read

**Tcl\_ReadChars** consumes bytes from *channel*, converting the bytes to TUTF-8 based on the channel's encoding and storing the produced data in *readObjPtr*'s string representation.  The return value of **Tcl\_ReadChars** is the number of characters, up to *charsToRead*, that were stored in *readObjPtr*.  If an error occurs while reading, the return value is -1 and **Tcl\_ReadChars** records a POSIX error code that can be retrieved with **Tcl\_GetErrno**. If an encoding error happens while the channel is in blocking mode with -profile strict, the characters retrieved until the encoding error happened will be stored in *readObjPtr*.

Setting *charsToRead* to -1 will cause the command to read all characters currently available (non-blocking) or everything until eof (blocking mode).

The return value may be smaller than the value to read, indicating that less data than requested was available.  This is called a *short read*.  In blocking mode, this can only happen on an end-of-file.  In nonblocking mode, a short read can also occur if an encoding error is encountered (with -profile strict) or if there is not enough input currently available: **Tcl\_ReadChars** returns a short count rather than waiting for more data.

If the channel is in blocking mode, a return value of zero indicates an end-of-file condition.  If the channel is in nonblocking mode, a return value of zero indicates either that no input is currently available or an end-of-file condition.  Use **Tcl\_Eof** and **Tcl\_InputBlocked** to tell which of these conditions actually occurred.

**Tcl\_ReadChars** translates the various end-of-line representations into the canonical **\\n** internal representation according to the current end-of-line recognition mode.  End-of-line recognition and the various platform-specific modes are described in the manual entry for the Tcl [fconfigure] command.

As a performance optimization, when reading from a channel with the encoding [binary], the bytes are not converted to TUTF-8 as they are read. Instead, they are stored in *readObjPtr*'s internal representation as a byte-array value.  The string representation of this value will only be constructed if it is needed (e.g., because of a call to **Tcl\_GetStringFromObj**).  In this way, byte-oriented data can be read from a channel, manipulated by calling **Tcl\_GetByteArrayFromObj** and related functions, and then written to a channel without the expense of ever converting to or from TUTF-8.

**Tcl\_Read** is similar to **Tcl\_ReadChars**, except that it does not do encoding conversions, regardless of the channel's encoding.  It is deprecated and exists for backwards compatibility with non-internationalized Tcl extensions.  It consumes bytes from *channel* and stores them in *readBuf*, performing end-of-line translations on the way.  The return value of **Tcl\_Read** is the number of bytes, up to *bytesToRead*, written in *readBuf*.  The buffer produced by **Tcl\_Read** is not null-terminated. Its contents are valid from the zeroth position up to and excluding the position indicated by the return value.

**Tcl\_ReadRaw** is the same as **Tcl\_Read** but does not compensate for stacking. While **Tcl\_Read** (and the other functions in the API) always get their data from the topmost channel in the stack the supplied channel is part of, **Tcl\_ReadRaw** does not. Thus this function is **only** usable for transformational channel drivers, i.e. drivers used in the middle of a stack of channels, to move data from the channel below into the transformation.

# Tcl\_getsobj and tcl\_gets

**Tcl\_GetsObj** consumes bytes from *channel*, based on the channel's encoding, until a full line of input has been seen.  If the channel's encoding is [binary], each byte read from the channel is treated as an individual Unicode character.  All of the characters of the line except for the terminating end-of-line character(s) are appended to *lineObjPtr*'s string representation.  The end-of-line character(s) are read and discarded.

If a line was successfully read, the return value is greater than or equal to zero and indicates the number of bytes stored in *lineObjPtr*.  If an error occurs, **Tcl\_GetsObj** returns -1 and records a POSIX error code that can be retrieved with **Tcl\_GetErrno**.  **Tcl\_GetsObj** also returns -1 if the end of the file is reached; the **Tcl\_Eof** procedure can be used to distinguish an error from an end-of-file condition.

If the channel is in nonblocking mode, the return value can also be -1 if no data was available or the data that was available did not contain an end-of-line character.  When -1 is returned, the **Tcl\_InputBlocked** procedure may be invoked to determine if the channel is blocked because of input unavailability.

**Tcl\_Gets** works similarly to **Tcl\_GetsObj** except the read bytes are are appended as a TUTF-8 encoded byte sequence to the dynamic string given by *lineRead* rather than a Tcl value.

# Tcl\_ungets

**Tcl\_Ungets** is used to add data to the input queue of a channel, at either the head or tail of the queue.  The pointer *input* points to the data that is to be added.  The length of the input to add is given by *inputLen*.  A non-zero value of *addAtEnd* indicates that the data is to be added at the end of queue; otherwise it will be added at the head of the queue.  If *channel* has a "sticky" EOF set, no data will be added to the input queue.  **Tcl\_Ungets** returns *inputLen* or -1 if an error occurs.

# Tcl\_writechars, tcl\_writeobj, and tcl\_write

**Tcl\_WriteChars** accepts *bytesToWrite* bytes of character data at *charBuf*.  The TUTF-8 encoded bytes in the buffer are converted to the channel's encoding and queued for output to *channel*.  If *bytesToWrite* is negative, **Tcl\_WriteChars** expects *charBuf* to be null-terminated and it outputs everything up to the null.

Data queued for output may not appear on the output device immediately, due to internal buffering.  If the data should appear immediately, call **Tcl\_Flush** after the call to **Tcl\_WriteChars**, or set the **-buffering** option on the channel to **none**.  If you wish the data to appear as soon as a complete line is accepted for output, set the **-buffering** option on the channel to **line** mode.

The return value of **Tcl\_WriteChars** is a count of how many bytes were accepted for output to the channel.  This is either -1 to indicate that an error occurred or another number greater than zero to indicate success.  If an error occurs, **Tcl\_WriteChars** records a POSIX error code that may be retrieved with **Tcl\_GetErrno**.

Newline characters in the output data are translated to platform-specific end-of-line sequences according to the **-translation** option for the channel.  This is done even if the channel has no encoding.

**Tcl\_WriteObj** is similar to **Tcl\_WriteChars** except it accepts a Tcl value whose contents will be output to the channel.  The characters in *writeObjPtr*'s string representation are converted to the channel's encoding and queued for output to *channel*. As a performance optimization, when writing to a channel with the encoding [binary], characters are not converted as they are written. Instead, the bytes in *writeObjPtr*'s internal representation as a byte-array value are written to the channel.  The byte-array representation of the value will be constructed if it is needed.  In this way, byte-oriented data can be read from a channel, manipulated by calling **Tcl\_GetByteArrayFromObj** and related functions, and then written to a channel without the expense of encoding conversion.

**Tcl\_Write** is similar to **Tcl\_WriteChars** except that it does not do encoding conversions, regardless of the channel's encoding.  It is deprecated and exists for backwards compatibility with non-internationalized Tcl extensions.  It accepts *bytesToWrite* bytes of data at *byteBuf* and queues them for output to *channel*.  If *bytesToWrite* is negative, **Tcl\_Write** expects *byteBuf* to be null-terminated and it outputs everything up to the null.

**Tcl\_WriteRaw** is the same as **Tcl\_Write** but does not compensate for stacking. While **Tcl\_Write** (and the other functions in the API) always feed their input to the topmost channel in the stack the supplied channel is part of, **Tcl\_WriteRaw** does not. Thus this function is **only** usable for transformational channel drivers, i.e. drivers used in the middle of a stack of channels, to move data from the transformation into the channel below it.

# Tcl\_flush

**Tcl\_Flush** causes all of the buffered output data for *channel* to be written to its underlying file or device as soon as possible. If the channel is in blocking mode, the call does not return until all the buffered data has been sent to the channel or some error occurred. The call returns immediately if the channel is nonblocking; it starts a background flush that will write the buffered data to the channel eventually, as fast as the channel is able to absorb it.

The return value is normally **TCL\_OK**. If an error occurs, **Tcl\_Flush** returns **TCL\_ERROR** and records a POSIX error code that can be retrieved with **Tcl\_GetErrno**.

# Tcl\_seek

**Tcl\_Seek** moves the access point in *channel* where subsequent data will be read or written. Buffered output is flushed to the channel and buffered input is discarded, prior to the seek operation.

**Tcl\_Seek** normally returns the new access point. If an error occurs, **Tcl\_Seek** returns -1 and records a POSIX error code that can be retrieved with **Tcl\_GetErrno**. After an error, the access point may or may not have been moved.

# Tcl\_tell

**Tcl\_Tell** returns the current access point for a channel. The returned value is -1 if the channel does not support seeking.

# Tcl\_truncatechannel

**Tcl\_TruncateChannel** truncates the file underlying *channel* to a given *length* of bytes. It returns **TCL\_OK** if the operation succeeded, and **TCL\_ERROR** otherwise.

# Tcl\_getchanneloption

**Tcl\_GetChannelOption** retrieves, in *optionValue*, the value of one of the options currently in effect for a channel, or a list of all options and their values.  The *channel* argument identifies the channel for which to query an option or retrieve all options and their values. If *optionName* is not NULL, it is the name of the option to query; the option's value is copied to the Tcl dynamic string denoted by *optionValue*. If *optionName* is NULL, the function stores an alternating list of option names and their values in *optionValue*, using a series of calls to **Tcl\_DStringAppendElement**. The various preexisting options and their possible values are described in the manual entry for the Tcl [fconfigure] command. Other options can be added by each channel type. These channel type specific options are described in the manual entry for the Tcl command that creates a channel of that type; for example, the additional options for TCP-based channels are described in the manual entry for the Tcl [socket] command. The procedure normally returns **TCL\_OK**. If an error occurs, it returns **TCL\_ERROR** and calls **Tcl\_SetErrno** to store an appropriate POSIX error code.

# Tcl\_setchanneloption

**Tcl\_SetChannelOption** sets a new value *newValue* for an option *optionName* on *channel*. The procedure normally returns **TCL\_OK**.  If an error occurs, it returns **TCL\_ERROR**;  in addition, if *interp* is non-NULL, **Tcl\_SetChannelOption** leaves an error message in the interpreter's result.

# Tcl\_eof

**Tcl\_Eof** returns a nonzero value if *channel* encountered an end of file during the last input operation.

# Tcl\_inputblocked

**Tcl\_InputBlocked** returns a nonzero value if *channel* is in nonblocking mode and the last input operation returned less data than requested because there was insufficient data available. The call always returns zero if the channel is in blocking mode.

# Tcl\_inputbuffered

**Tcl\_InputBuffered** returns the number of bytes of input currently buffered in the internal buffers for a channel. If the channel is not open for reading, this function always returns zero.

# Tcl\_outputbuffered

**Tcl\_OutputBuffered** returns the number of bytes of output currently buffered in the internal buffers for a channel. If the channel is not open for writing, this function always returns zero.

# Platform issues

The handles returned from **Tcl\_GetChannelHandle** depend on the platform and the channel type.  On Unix platforms, the handle is always a Unix file descriptor as returned from the [open] system call.  On Windows platforms, the handle is a file **HANDLE** when the channel was created with **Tcl\_OpenFileChannel**, **Tcl\_OpenCommandChannel**, or **Tcl\_MakeFileChannel**.  Other channel types may return a different type of handle on Windows platforms.

# Reference count management

The *readObjPtr* argument to **Tcl\_ReadChars** must be an unshared value; it will be modified by this function.  Using the interpreter result for this purpose is *strongly* not recommended; the preferred pattern is to use a new value from **Tcl\_NewObj** to receive the data and only to pass it to **Tcl\_SetObjResult** if this function succeeds.

The *lineObjPtr* argument to **Tcl\_GetsObj** must be an unshared value; it will be modified by this function.  Using the interpreter result for this purpose is *strongly* not recommended; the preferred pattern is to use a new value from **Tcl\_NewObj** to receive the data and only to pass it to **Tcl\_SetObjResult** if this function succeeds.

The *writeObjPtr* argument to **Tcl\_WriteObj** should be a value with any reference count. This function will not modify the reference count. Using the interpreter result without adding an additional reference to it is not recommended.


[binary]: binary.md
[chan]: chan.md
[exec]: exec.md
[fconfigure]: fconfigure.md
[open]: open.md
[socket]: socket.md

