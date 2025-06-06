---
CommandName: fcopy
ManualSection: n
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - eof(n)
 - fblocked(n)
 - fconfigure(n)
 - file(n)
Keywords:
 - blocking
 - channel
 - end of line
 - end of file
 - nonblocking
 - read
 - translation
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

fcopy - Copy data from one channel to another

# Synopsis

::: {.synopsis} :::
[fcopy]{.cmd} [inputChan]{.arg} [outputChan]{.arg} [[-size]{.lit} [size]{.arg}]{.optarg} [[-command]{.lit} [callback]{.arg}]{.optarg}
:::

# Description

The **fcopy** command copies data from one I/O channel, *inchan*, to another I/O channel, *outchan*. The **fcopy** command leverages the buffering in the Tcl I/O system to avoid extra copies and to avoid buffering too much data in main memory when copying large files to destinations like network sockets.

## Data quantity

All data until *EOF* is copied. In addition, the quantity of copied data may be specified by the option **-size**. The given size is in bytes, if the input channel is in binary mode. Otherwise, it is in characters.

Depreciated feature: the transfer is treated as a binary transfer, if the encoding profile is set to "tcl8" and the input encoding matches the output encoding. In this case, eventual encoding errors are not handled. An eventually given size is in bytes in this case.

## Blocking operation mode

Without the **-command** option, **fcopy** blocks until the copy is complete and returns the number of bytes or characters (using the same rules as for the **-size** option) written to *outchan*.

## Background operation mode

The **-command** argument makes **fcopy** work in the background. In this case it returns immediately and the *callback* is invoked later when the copy completes. The *callback* is called with one or two additional arguments that indicates how many bytes were written to *outchan*. If an error occurred during the background copy, the second argument is the error string associated with the error. With a background copy, it is not necessary to put *inchan* or *outchan* into non-blocking mode; the **fcopy** command takes care of that automatically. However, it is necessary to enter the event loop by using the **vwait** command or by using Tk.

You are not allowed to do other input operations with *inchan*, or output operations with *outchan*, during a background **fcopy**. The converse is entirely legitimate, as exhibited by the bidirectional fcopy example below.

If either *inchan* or *outchan* get closed while the copy is in progress, the current copy is stopped and the command callback is *not* made. If *inchan* is closed, then all data already queued for *outchan* is written out.

Note that *inchan* can become readable during a background copy. You should turn off any **fileevent** handlers during a background copy so those handlers do not interfere with the copy. Any wrong-sided I/O attempted (by a **fileevent** handler or otherwise) will get a "channel busy" error.

## Channel translation options

**Fcopy** translates end-of-line sequences in *inchan* and *outchan* according to the **-translation** option for these channels. See the manual entry for **fconfigure** for details on the **-translation** option. The translations mean that the number of bytes read from *inchan* can be different than the number of bytes written to *outchan*. Only the number of bytes written to *outchan* is reported, either as the return value of a synchronous **fcopy** or as the argument to the callback for an asynchronous **fcopy**.

## Channel encoding options

**Fcopy** obeys the encodings, profiles and character translations configured for the channels. This means that the incoming characters are converted internally first UTF-8 and then into the encoding of the channel **fcopy** writes to. See the manual entry for **fconfigure** for details on the **-encoding** and **-profile** options. No conversion is done if both channels are set to encoding "binary" and have matching translations. If only the output channel is set to encoding "binary" the system will write the internal UTF-8 representation of the incoming characters. If only the input channel is set to encoding "binary" the system will assume that the incoming bytes are valid UTF-8 characters and convert them according to the output encoding. The behaviour of the system for bytes which are not valid UTF-8 characters is undefined in this case.

**Fcopy** may throw encoding errors (error code **EILSEQ**), if input or output channel is configured to the "strict" encoding profile.

If an encoding error arises on the input channel, any data before the error byte is written to the output channel. The input file pointer is located just before the values causing the encoding error. Error inspection or recovery is possible by changing the encoding parameters and invoking a file command (**read**, **fcopy**).

If an encoding error arises on the output channel, the erroneous data is lost. To make the difference between the input error case and the output error case, only the error message may be inspected (read or write), as both throw the error code *EILSEQ*.

# Examples

The first example transfers the contents of one channel exactly to another. Note that when copying one file to another, it is better to use **file copy** which also copies file metadata (e.g. the file access permissions) where possible.

```
fconfigure $in -translation binary
fconfigure $out -translation binary
fcopy $in $out
```

This second example shows how the callback gets passed the number of bytes transferred. It also uses vwait to put the application into the event loop. Of course, this simplified example could be done without the command callback.

```
proc Cleanup {in out bytes {error {}}} {
    global total
    set total $bytes
    close $in
    close $out
    if {[string length $error] != 0} {
        # error occurred during the copy
    }
}
set in [open $file1]
set out [socket $server $port]
fcopy $in $out -command [list Cleanup $in $out]
vwait total
```

The third example copies in chunks and tests for end of file in the command callback.

```
proc CopyMore {in out chunk bytes {error {}}} {
    global total done
    incr total $bytes
    if {([string length $error] != 0) || [eof $in]} {
        set done $total
        close $in
        close $out
    } else {
        fcopy $in $out -size $chunk \
                -command [list CopyMore $in $out $chunk]
    }
}
set in [open $file1]
set out [socket $server $port]
set chunk 1024
set total 0
fcopy $in $out -size $chunk \
        -command [list CopyMore $in $out $chunk]
vwait done
```

The fourth example starts an asynchronous, bidirectional fcopy between two sockets. Those could also be pipes from two [open "|hal 9000" r+] (though their conversation would remain secret to the script, since all four fileevent slots are busy).

```
set flows 2
proc Done {dir args} {
     global flows done
     puts "$dir is over."
     incr flows -1
     if {$flows<=0} {set done 1}
}
fcopy $sok1 $sok2 -command [list Done UP]
fcopy $sok2 $sok1 -command [list Done DOWN]
vwait done
```

