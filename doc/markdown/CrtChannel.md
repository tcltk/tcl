---
CommandName: Tcl_CreateChannel
ManualSection: 3
Version: 8.4
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_Close(3)
 - Tcl_OpenFileChannel(3)
 - Tcl_SetErrno(3)
 - Tcl_QueueEvent(3)
 - Tcl_StackChannel(3)
 - Tcl_GetStdChannel(3)
Keywords:
 - blocking
 - channel driver
 - channel registration
 - channel type
 - nonblocking
Copyright:
 - Copyright (c) 1996-1997 Sun Microsystems, Inc.
 - Copyright (c) 1997-2000 Ajuba Solutions.
---

# Name

Tcl_CreateChannel, Tcl_GetChannelInstanceData, Tcl_GetChannelType, Tcl_GetChannelName, Tcl_GetChannelHandle, Tcl_GetChannelMode, Tcl_GetChannelBufferSize, Tcl_SetChannelBufferSize, Tcl_NotifyChannel, Tcl_BadChannelOption, Tcl_ChannelName, Tcl_ChannelVersion, Tcl_ChannelBlockModeProc, Tcl_ChannelClose2Proc, Tcl_ChannelInputProc, Tcl_ChannelOutputProc, Tcl_ChannelWideSeekProc, Tcl_ChannelTruncateProc, Tcl_ChannelSetOptionProc, Tcl_ChannelGetOptionProc, Tcl_ChannelWatchProc, Tcl_ChannelGetHandleProc, Tcl_ChannelFlushProc, Tcl_ChannelHandlerProc, Tcl_ChannelThreadActionProc, Tcl_IsChannelShared, Tcl_IsChannelRegistered, Tcl_CutChannel, Tcl_SpliceChannel, Tcl_IsChannelExisting, Tcl_ClearChannelHandlers, Tcl_GetChannelThread, Tcl_ChannelBuffered - procedures for creating and manipulating channels

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Channel]{.ret} [Tcl_CreateChannel]{.ccmd}[typePtr, channelName, instanceData, mask]{.cargs}
[void *]{.ret} [Tcl_GetChannelInstanceData]{.ccmd}[channel]{.cargs}
[const Tcl_ChannelType *]{.ret} [Tcl_GetChannelType]{.ccmd}[channel]{.cargs}
[const char *]{.ret} [Tcl_GetChannelName]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl_GetChannelHandle]{.ccmd}[channel, direction, handlePtr]{.cargs}
[Tcl_ThreadId]{.ret} [Tcl_GetChannelThread]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl_GetChannelMode]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl_RemoveChannelMode]{.ccmd}[interp, channel, mode]{.cargs}
[int]{.ret} [Tcl_GetChannelBufferSize]{.ccmd}[channel]{.cargs}
[Tcl_SetChannelBufferSize]{.ccmd}[channel, size]{.cargs}
[Tcl_NotifyChannel]{.ccmd}[channel, mask]{.cargs}
[int]{.ret} [Tcl_BadChannelOption]{.ccmd}[interp, optionName, optionList]{.cargs}
[int]{.ret} [Tcl_IsChannelShared]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl_IsChannelRegistered]{.ccmd}[interp, channel]{.cargs}
[int]{.ret} [Tcl_IsChannelExisting]{.ccmd}[channelName]{.cargs}
[Tcl_CutChannel]{.ccmd}[channel]{.cargs}
[Tcl_SpliceChannel]{.ccmd}[channel]{.cargs}
[Tcl_ClearChannelHandlers]{.ccmd}[channel]{.cargs}
[int]{.ret} [Tcl_ChannelBuffered]{.ccmd}[channel]{.cargs}
[const char *]{.ret} [Tcl_ChannelName]{.ccmd}[typePtr]{.cargs}
[Tcl_ChannelTypeVersion]{.ret} [Tcl_ChannelVersion]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverBlockModeProc *]{.ret} [Tcl_ChannelBlockModeProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverClose2Proc *]{.ret} [Tcl_ChannelClose2Proc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverInputProc *]{.ret} [Tcl_ChannelInputProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverOutputProc *]{.ret} [Tcl_ChannelOutputProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverWideSeekProc *]{.ret} [Tcl_ChannelWideSeekProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverThreadActionProc *]{.ret} [Tcl_ChannelThreadActionProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverTruncateProc *]{.ret} [Tcl_ChannelTruncateProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverSetOptionProc *]{.ret} [Tcl_ChannelSetOptionProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverGetOptionProc *]{.ret} [Tcl_ChannelGetOptionProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverWatchProc *]{.ret} [Tcl_ChannelWatchProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverGetHandleProc *]{.ret} [Tcl_ChannelGetHandleProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverFlushProc *]{.ret} [Tcl_ChannelFlushProc]{.ccmd}[typePtr]{.cargs}
[Tcl_DriverHandlerProc *]{.ret} [Tcl_ChannelHandlerProc]{.ccmd}[typePtr]{.cargs}
:::

# Arguments

.AP "const Tcl_ChannelType" *typePtr in Points to a structure containing the addresses of procedures that can be called to perform I/O and other functions on the channel. .AP "const char" *channelName in The name of this channel, such as **file3**; must not be in use by any other channel. Can be NULL, in which case the channel is created without a name. If the created channel is assigned to one of the standard channels (**stdin**, **stdout** or **stderr**), the assigned channel name will be the name of the standard channel. .AP void *instanceData in Arbitrary one-word value to be associated with this channel.  This value is passed to procedures in *typePtr* when they are invoked. .AP int mask in OR-ed combination of **TCL_READABLE** and **TCL_WRITABLE** to indicate whether a channel is readable and writable. .AP Tcl_Channel channel in The channel to operate on. .AP int direction in **TCL_READABLE** means the input handle is wanted; **TCL_WRITABLE** means the output handle is wanted. .AP void **handlePtr out Points to the location where the desired OS-specific handle should be stored. .AP Tcl_Size size in The size, in bytes, of buffers to allocate in this channel. .AP int mask in An OR-ed combination of **TCL_READABLE**, **TCL_WRITABLE** and **TCL_EXCEPTION** that indicates events that have occurred on this channel. .AP Tcl_Interp *interp in Current interpreter. (can be NULL) .AP "const char" *optionName in Name of the invalid option. .AP "const char" *optionList in Specific options list (space separated words, without "-") to append to the standard generic options list. Can be NULL for generic options error message only.

# Description

Tcl uses a two-layered channel architecture. It provides a generic upper layer to enable C and Tcl programs to perform input and output using the same APIs for a variety of files, devices, sockets etc. The generic C APIs are described in the manual entry for **Tcl_OpenFileChannel**.

The lower layer provides type-specific channel drivers for each type of device supported on each platform.  This manual entry describes the C APIs used to communicate between the generic layer and the type-specific channel drivers.  It also explains how new types of channels can be added by providing new channel drivers.

Channel drivers consist of a number of components: First, each channel driver provides a **Tcl_ChannelType** structure containing pointers to functions implementing the various operations used by the generic layer to communicate with the channel driver. The **Tcl_ChannelType** structure and the functions referenced by it are described in the section **TCL_CHANNELTYPE**, below.

Second, channel drivers usually provide a Tcl command to create instances of that type of channel. For example, the Tcl **open** command creates channels that use the file and command channel drivers, and the Tcl **socket** command creates channels that use TCP sockets for network communication.

Third, a channel driver optionally provides a C function to open channel instances of that type. For example, **Tcl_OpenFileChannel** opens a channel that uses the file channel driver, and **Tcl_OpenTcpClient** opens a channel that uses the TCP network protocol.  These creation functions typically use **Tcl_CreateChannel** internally to open the channel.

To add a new type of channel you must implement a C API or a Tcl command that opens a channel by invoking **Tcl_CreateChannel**. When your driver calls **Tcl_CreateChannel** it passes in a **Tcl_ChannelType** structure describing the driver's I/O procedures. The generic layer will then invoke the functions referenced in that structure to perform operations on the channel.

**Tcl_CreateChannel** opens a new channel and associates the supplied *typePtr* and *instanceData* with it. The channel is opened in the mode indicated by *mask*. For a discussion of channel drivers, their operations and the **Tcl_ChannelType** structure, see the section **TCL_CHANNELTYPE**, below.

**Tcl_CreateChannel** interacts with the code managing the standard channels. Once a standard channel was initialized either through a call to **Tcl_GetStdChannel** or a call to **Tcl_SetStdChannel** closing this standard channel will cause the next call to **Tcl_CreateChannel** to make the new channel the new standard channel too. See **Tcl_StandardChannels** for a general treatise about standard channels and the behavior of the Tcl library with regard to them.

**Tcl_GetChannelInstanceData** returns the instance data associated with the channel in *channel*. This is the same as the *instanceData* argument in the call to **Tcl_CreateChannel** that created this channel.

**Tcl_GetChannelType** returns a pointer to the **Tcl_ChannelType** structure used by the channel in the *channel* argument. This is the same as the *typePtr* argument in the call to **Tcl_CreateChannel** that created this channel.

**Tcl_GetChannelName** returns a string containing the name associated with the channel, or NULL if the *channelName* argument to **Tcl_CreateChannel** was NULL.

**Tcl_GetChannelHandle** places the OS-specific device handle associated with *channel* for the given *direction* in the location specified by *handlePtr* and returns **TCL_OK**.  If the channel does not have a device handle for the specified direction, then **TCL_ERROR** is returned instead.  Different channel drivers will return different types of handle.  Refer to the manual entries for each driver to determine what type of handle is returned.

**Tcl_GetChannelThread** returns the id of the thread currently managing the specified *channel*. This allows channel drivers to send their file events to the correct event queue even for a multi-threaded core.

**Tcl_GetChannelMode** returns an OR-ed combination of **TCL_READABLE** and **TCL_WRITABLE**, indicating whether the channel is open for input and output.

**Tcl_RemoveChannelMode** removes an access privilege from the channel, either **TCL_READABLE** or **TCL_WRITABLE**, and returns a regular Tcl result code, **TCL_OK**, or **TCL_ERROR**. The function throws an error if either an invalid mode is specified or the result of the removal would be an inaccessible channel. In that case an error message is left in the interp argument, if not NULL.

**Tcl_GetChannelBufferSize** returns the size, in bytes, of buffers allocated to store input or output in *channel*. If the value was not set by a previous call to **Tcl_SetChannelBufferSize**, described below, then the default value of 4096 is returned.

**Tcl_SetChannelBufferSize** sets the size, in bytes, of buffers that will be allocated in subsequent operations on the channel to store input or output. The *size* argument should be between one and one million, allowing buffers of one byte to one million bytes. If *size* is outside this range, **Tcl_SetChannelBufferSize** sets the buffer size to 4096.

**Tcl_NotifyChannel** is called by a channel driver to indicate to the generic layer that the events specified by *mask* have occurred on the channel.  Channel drivers are responsible for invoking this function whenever the channel handlers need to be called for the channel (or other pending tasks like a write flush should be performed). See **WATCHPROC** below for more details.

**Tcl_BadChannelOption** is called from driver specific *setOptionProc* or *getOptionProc* to generate a complete error message.

**Tcl_ChannelBuffered** returns the number of bytes of input currently buffered in the internal buffer (push back area) of the channel itself. It does not report about the data in the overall buffers for the stack of channels the supplied channel is part of.

**Tcl_IsChannelShared** checks the refcount of the specified *channel* and returns whether the *channel* was shared among multiple interpreters (result == 1) or not (result == 0).

**Tcl_IsChannelRegistered** checks whether the specified *channel* is registered in the given *interp*reter (result == 1) or not (result == 0).

**Tcl_IsChannelExisting** checks whether a channel with the specified name is registered in the (thread)-global list of all channels (result == 1) or not (result == 0).

**Tcl_CutChannel** removes the specified *channel* from the (thread)global list of all channels (of the current thread). Application to a channel still registered in some interpreter is not allowed. Also notifies the driver if **Tcl_DriverThreadActionProc** is defined for it.

**Tcl_SpliceChannel** adds the specified *channel* to the (thread)global list of all channels (of the current thread). Application to a channel registered in some interpreter is not allowed. Also notifies the driver if **Tcl_DriverThreadActionProc** is defined for it.

**Tcl_ClearChannelHandlers** removes all channel handlers and event scripts associated with the specified *channel*, thus shutting down all event processing for this channel.

# Tcl_channeltype

A channel driver provides a **Tcl_ChannelType** structure that contains pointers to functions that implement the various operations on a channel; these operations are invoked as needed by the generic layer.  The structure was versioned starting in Tcl 8.3.2/8.4 to correct a problem with stacked channel drivers.  See the **OLD CHANNEL TYPES** section below for details about the old structure.

The **Tcl_ChannelType** structure contains the following fields:

```
typedef struct {
        const char *typeName;
        Tcl_ChannelTypeVersion version;
        void *closeProc; /* Not used any more*/
        Tcl_DriverInputProc *inputProc;
        Tcl_DriverOutputProc *outputProc;
        void *seekProc; /* Not used any more */
        Tcl_DriverSetOptionProc *setOptionProc;
        Tcl_DriverGetOptionProc *getOptionProc;
        Tcl_DriverWatchProc *watchProc;
        Tcl_DriverGetHandleProc *getHandleProc;
        Tcl_DriverClose2Proc *close2Proc;
        Tcl_DriverBlockModeProc *blockModeProc;
        Tcl_DriverFlushProc *flushProc;
        Tcl_DriverHandlerProc *handlerProc;
        Tcl_DriverWideSeekProc *wideSeekProc;
        Tcl_DriverThreadActionProc *threadActionProc;
        Tcl_DriverTruncateProc *truncateProc;
} Tcl_ChannelType;
```

It is not necessary to provide implementations for all channel operations.  Those which are not necessary may be set to NULL in the struct: *blockModeProc*, *seekProc*, *setOptionProc*, *getOptionProc*, *getHandleProc*, and *close2Proc*, in addition to *flushProc*, *handlerProc*, *threadActionProc*, and *truncateProc*.  Other functions that cannot be implemented in a meaningful way should return **EINVAL** when called, to indicate that the operations they represent are not available. Also note that *wideSeekProc* can be NULL if *seekProc* is.

The user should only use the above structure for **Tcl_ChannelType** instantiation.  When referencing fields in a **Tcl_ChannelType** structure, the following functions should be used to obtain the values: **Tcl_ChannelName**, **Tcl_ChannelVersion**, **Tcl_ChannelBlockModeProc**, **Tcl_ChannelClose2Proc**, **Tcl_ChannelInputProc**, **Tcl_ChannelOutputProc**, **Tcl_ChannelWideSeekProc**, **Tcl_ChannelThreadActionProc**, **Tcl_ChannelTruncateProc**, **Tcl_ChannelSetOptionProc**, **Tcl_ChannelGetOptionProc**, **Tcl_ChannelWatchProc**, **Tcl_ChannelGetHandleProc**, **Tcl_ChannelFlushProc**, or **Tcl_ChannelHandlerProc**.

The change to the structures was made in such a way that standard channel types are binary compatible.  However, channel types that use stacked channels (i.e. TLS, Trf) have new versions to correspond to the above change since the previous code for stacked channels had problems.

## Typename

The *typeName* field contains a null-terminated string that identifies the type of the device implemented by this driver, e.g. **file** or **socket**.

This value can be retrieved with **Tcl_ChannelName**, which returns a pointer to the string.

## Version

The *version* field should be set to the version of the structure that you require. **TCL_CHANNEL_VERSION_5** is the minimum supported.

This value can be retrieved with **Tcl_ChannelVersion**.

## Blockmodeproc

The *blockModeProc* field contains the address of a function called by the generic layer to set blocking and nonblocking mode on the device. *BlockModeProc* should match the following prototype:

```
typedef int Tcl_DriverBlockModeProc(
        void *instanceData,
        int mode);
```

The *instanceData* is the same as the value passed to **Tcl_CreateChannel** when this channel was created.  The *mode* argument is either **TCL_MODE_BLOCKING** or **TCL_MODE_NONBLOCKING** to set the device into blocking or nonblocking mode. The function should return zero if the operation was successful, or a nonzero POSIX error code if the operation failed.

If the operation is successful, the function can modify the supplied *instanceData* to record that the channel entered blocking or nonblocking mode and to implement the blocking or nonblocking behavior. For some device types, the blocking and nonblocking behavior can be implemented by the underlying operating system; for other device types, the behavior must be emulated in the channel driver.

This value can be retrieved with **Tcl_ChannelBlockModeProc**, which returns a pointer to the function.

A channel driver **not** supplying a *blockModeProc* has to be very, very careful. It has to tell the generic layer exactly which blocking mode is acceptable to it, and should this also document for the user so that the blocking mode of the channel is not changed to an unacceptable value. Any confusion here may lead the interpreter into a (spurious and difficult to find) deadlock.

## Close2proc

The *close2Proc* field contains the address of a function called by the generic layer to clean up driver-related information when the channel is closed. *Close2Proc* must match the following prototype:

```
typedef int Tcl_DriverClose2Proc(
        void *instanceData,
        Tcl_Interp *interp,
        int flags);
```

If *flags* is 0, the *instanceData* argument is the same as the value provided to **Tcl_CreateChannel** when the channel was created. The function should release any storage maintained by the channel driver for this channel, and close the input and output devices encapsulated by this channel. All queued output will have been flushed to the device before this function is called, and no further driver operations will be invoked on this instance after calling the *closeProc*. If the close operation is successful, the procedure should return zero; otherwise it should return a nonzero POSIX error code. In addition, if an error occurs and *interp* is not NULL, the procedure should store an error message in the interpreter's result.

Alternatively, channels that support closing the read and write sides independently may accept other flag values than 0. Then *close2Proc* will be called with *flags* set to an OR'ed combination of **TCL_CLOSE_READ** or **TCL_CLOSE_WRITE** to indicate that the driver should close the read and/or write side of the channel.  The channel driver may be invoked to perform additional operations on the channel after *close2Proc* is called to close one or both sides of the channel.  In all cases, the *close2Proc* function should return zero if the close operation was successful; otherwise it should return a nonzero POSIX error code. In addition, if an error occurs and *interp* is not NULL, the procedure should store an error message in the interpreter's result.

The *close2Proc* value can be retrieved with **Tcl_ChannelClose2Proc**, which returns a pointer to the function.

## Inputproc

The *inputProc* field contains the address of a function called by the generic layer to read data from the file or device and store it in an internal buffer. *InputProc* must match the following prototype:

```
typedef int Tcl_DriverInputProc(
        void *instanceData,
        char *buf,
        int bufSize,
        int *errorCodePtr);
```

*InstanceData* is the same as the value passed to **Tcl_CreateChannel** when the channel was created.  The *buf* argument points to an array of bytes in which to store input from the device, and the *bufSize* argument indicates how many bytes are available at *buf*.

The *errorCodePtr* argument points to an integer variable provided by the generic layer. If an error occurs, the function should set the variable to a POSIX error code that identifies the error that occurred.

The function should read data from the input device encapsulated by the channel and store it at *buf*.  On success, the function should return a nonnegative integer indicating how many bytes were read from the input device and stored at *buf*. On error, the function should return -1. If an error occurs after some data has been read from the device, that data is lost.

If *inputProc* can determine that the input device has some data available but less than requested by the *bufSize* argument, the function should only attempt to read as much data as is available and return without blocking. If the input device has no data available whatsoever and the channel is in nonblocking mode, the function should return an **EAGAIN** error. If the input device has no data available whatsoever and the channel is in blocking mode, the function should block for the shortest possible time until at least one byte of data can be read from the device; then, it should return as much data as it can read without blocking.

This value can be retrieved with **Tcl_ChannelInputProc**, which returns a pointer to the function.

## Outputproc

The *outputProc* field contains the address of a function called by the generic layer to transfer data from an internal buffer to the output device. *OutputProc* must match the following prototype:

```
typedef int Tcl_DriverOutputProc(
        void *instanceData,
        const char *buf,
        int toWrite,
        int *errorCodePtr);
```

*InstanceData* is the same as the value passed to **Tcl_CreateChannel** when the channel was created. The *buf* argument contains an array of bytes to be written to the device, and the *toWrite* argument indicates how many bytes are to be written from the *buf* argument.

The *errorCodePtr* argument points to an integer variable provided by the generic layer. If an error occurs, the function should set this variable to a POSIX error code that identifies the error.

The function should write the data at *buf* to the output device encapsulated by the channel. On success, the function should return a nonnegative integer indicating how many bytes were written to the output device.  The return value is normally the same as *toWrite*, but may be less in some cases such as if the output operation is interrupted by a signal. If an error occurs the function should return -1.  In case of error, some data may have been written to the device.

If the channel is nonblocking and the output device is unable to absorb any data whatsoever, the function should return -1 with an **EAGAIN** error without writing any data.

This value can be retrieved with **Tcl_ChannelOutputProc**, which returns a pointer to the function.

## Wideseekproc

The *wideSeekProc* field contains the address of a function called by the generic layer to move the access point at which subsequent input or output operations will be applied. *WideSeekProc* must match the following prototype:

```
typedef long long Tcl_DriverWideSeekProc(
        void *instanceData,
        long long offset,
        int seekMode,
        int *errorCodePtr);
```

The *instanceData* argument is the same as the value given to **Tcl_CreateChannel** when this channel was created.  *Offset* and *seekMode* have the same meaning as for the **Tcl_Seek** procedure (described in the manual entry for **Tcl_OpenFileChannel**).

The *errorCodePtr* argument points to an integer variable provided by the generic layer for returning **errno** values from the function.  The function should set this variable to a POSIX error code if an error occurs. The function should store an **EINVAL** error code if the channel type does not implement seeking.

The return value is the new access point or -1 in case of error. If an error occurred, the function should not move the access point.

The *wideSseekProc* value can be retrieved with **Tcl_ChannelWideSeekProc**, which returns a pointer to the function.

## Setoptionproc

The *setOptionProc* field contains the address of a function called by the generic layer to set a channel type specific option on a channel. *setOptionProc* must match the following prototype:

```
typedef int Tcl_DriverSetOptionProc(
        void *instanceData,
        Tcl_Interp *interp,
        const char *optionName,
        const char *newValue);
```

*optionName* is the name of an option to set, and *newValue* is the new value for that option, as a string. The *instanceData* is the same as the value given to **Tcl_CreateChannel** when this channel was created. The function should do whatever channel type specific action is required to implement the new value of the option.

Some options are handled by the generic code and this function is never called to set them, e.g. **-blockmode**. Other options are specific to each channel type and the *setOptionProc* procedure of the channel driver will get called to implement them. The *setOptionProc* field can be NULL, which indicates that this channel type supports no type specific options.

If the option value is successfully modified to the new value, the function returns **TCL_OK**. It should call **Tcl_BadChannelOption** which itself returns **TCL_ERROR** if the *optionName* is unrecognized. If *newValue* specifies a value for the option that is not supported or if a system call error occurs, the function should leave an error message in the result of *interp* if *interp* is not NULL. The function should also call **Tcl_SetErrno** to store an appropriate POSIX error code.

This value can be retrieved with **Tcl_ChannelSetOptionProc**, which returns a pointer to the function.

## Getoptionproc

The *getOptionProc* field contains the address of a function called by the generic layer to get the value of a channel type specific option on a channel. *getOptionProc* must match the following prototype:

```
typedef int Tcl_DriverGetOptionProc(
        void *instanceData,
        Tcl_Interp *interp,
        const char *optionName,
        Tcl_DString *optionValue);
```

*OptionName* is the name of an option supported by this type of channel. If the option name is not NULL, the function stores its current value, as a string, in the Tcl dynamic string *optionValue*. If *optionName* is NULL, the function stores in *optionValue* an alternating list of all supported options and their current values. On success, the function returns **TCL_OK**. It should call **Tcl_BadChannelOption** which itself returns **TCL_ERROR** if the *optionName* is unrecognized. If a system call error occurs, the function should leave an error message in the result of *interp* if *interp* is not NULL. The function should also call **Tcl_SetErrno** to store an appropriate POSIX error code.

Some options are handled by the generic code and this function is never called to retrieve their value, e.g. **-blockmode**. Other options are specific to each channel type and the *getOptionProc* procedure of the channel driver will get called to implement them. The *getOptionProc* field can be NULL, which indicates that this channel type supports no type specific options.

This value can be retrieved with **Tcl_ChannelGetOptionProc**, which returns a pointer to the function.

## Watchproc

The *watchProc* field contains the address of a function called by the generic layer to initialize the event notification mechanism to notice events of interest on this channel. *WatchProc* should match the following prototype:

```
typedef void Tcl_DriverWatchProc(
        void *instanceData,
        int mask);
```

The *instanceData* is the same as the value passed to **Tcl_CreateChannel** when this channel was created. The *mask* argument is an OR-ed combination of **TCL_READABLE**, **TCL_WRITABLE** and **TCL_EXCEPTION**; it indicates events the caller is interested in noticing on this channel.

The function should initialize device type specific mechanisms to notice when an event of interest is present on the channel.  When one or more of the designated events occurs on the channel, the channel driver is responsible for calling **Tcl_NotifyChannel** to inform the generic channel module.  The driver should take care not to starve other channel drivers or sources of callbacks by invoking Tcl_NotifyChannel too frequently.  Fairness can be insured by using the Tcl event queue to allow the channel event to be scheduled in sequence with other events.  See the description of **Tcl_QueueEvent** for details on how to queue an event.

This value can be retrieved with **Tcl_ChannelWatchProc**, which returns a pointer to the function.

## Gethandleproc

The *getHandleProc* field contains the address of a function called by the generic layer to retrieve a device-specific handle from the channel. *GetHandleProc* should match the following prototype:

```
typedef int Tcl_DriverGetHandleProc(
        void *instanceData,
        int direction,
        void **handlePtr);
```

*InstanceData* is the same as the value passed to **Tcl_CreateChannel** when this channel was created. The *direction* argument is either **TCL_READABLE** to retrieve the handle used for input, or **TCL_WRITABLE** to retrieve the handle used for output.

If the channel implementation has device-specific handles, the function should retrieve the appropriate handle associated with the channel, according the *direction* argument.  The handle should be stored in the location referred to by *handlePtr*, and **TCL_OK** should be returned.  If the channel is not open for the specified direction, or if the channel implementation does not use device handles, the function should return **TCL_ERROR**.

This value can be retrieved with **Tcl_ChannelGetHandleProc**, which returns a pointer to the function.

## Flushproc

The *flushProc* field is currently reserved for future use. It should be set to NULL. *FlushProc* should match the following prototype:

```
typedef int Tcl_DriverFlushProc(
        void *instanceData);
```

This value can be retrieved with **Tcl_ChannelFlushProc**, which returns a pointer to the function.

## Handlerproc

The *handlerProc* field contains the address of a function called by the generic layer to notify the channel that an event occurred.  It should be defined for stacked channel drivers that wish to be notified of events that occur on the underlying (stacked) channel. *HandlerProc* should match the following prototype:

```
typedef int Tcl_DriverHandlerProc(
        void *instanceData,
        int interestMask);
```

*InstanceData* is the same as the value passed to **Tcl_CreateChannel** when this channel was created.  The *interestMask* is an OR-ed combination of **TCL_READABLE** or **TCL_WRITABLE**; it indicates what type of event occurred on this channel.

This value can be retrieved with **Tcl_ChannelHandlerProc**, which returns a pointer to the function. 

## Threadactionproc

The *threadActionProc* field contains the address of the function called by the generic layer when a channel is created, closed, or going to move to a different thread, i.e. whenever thread-specific driver state might have to initialized or updated. It can be NULL. The action *TCL_CHANNEL_THREAD_REMOVE* is used to notify the driver that it should update or remove any thread-specific data it might be maintaining for the channel.

The action *TCL_CHANNEL_THREAD_INSERT* is used to notify the driver that it should update or initialize any thread-specific data it might be maintaining using the calling thread as the associate. See **Tcl_CutChannel** and **Tcl_SpliceChannel** for more detail.

```
typedef void Tcl_DriverThreadActionProc(
        void *instanceData,
        int action);
```

*InstanceData* is the same as the value passed to **Tcl_CreateChannel** when this channel was created.

These values can be retrieved with **Tcl_ChannelThreadActionProc**, which returns a pointer to the function.

## Truncateproc

The *truncateProc* field contains the address of the function called by the generic layer when a channel is truncated to some length. It can be NULL.

```
typedef int Tcl_DriverTruncateProc(
        void *instanceData,
        long long length);
```

*InstanceData* is the same as the value passed to **Tcl_CreateChannel** when this channel was created, and *length* is the new length of the underlying file, which should not be negative. The result should be 0 on success or an errno code (suitable for use with **Tcl_SetErrno**) on failure.

These values can be retrieved with **Tcl_ChannelTruncateProc**, which returns a pointer to the function.

# Tcl_badchanneloption

This procedure generates a "bad option" error message in an (optional) interpreter.  It is used by channel drivers when an invalid Set/Get option is requested. Its purpose is to concatenate the generic options list to the specific ones and factorize the generic options error message string.

It always returns **TCL_ERROR**

An error message is generated in *interp*'s result value to indicate that a command was invoked with a bad option. The message has the form

```
    bad option "blah": should be one of
    <...generic options...>+<...specific options...>
```

so you get for instance:

```
    bad option "-blah": should be one of -blocking,
    -buffering, -buffersize, -eofchar, -translation,
    -peername, or -sockname
```

when called with *optionList* equal to "peername sockname"

"blah" is the *optionName* argument and "<specific options>" is a space separated list of specific option words. The function takes good care of inserting minus signs before each option, commas after, and an "or" before the last option.

