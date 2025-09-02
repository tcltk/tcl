/*
 * tclWinChan.c
 *
 *	Channel drivers for Windows channels based on files, command pipes and
 *	TCP sockets.
 *
 * Copyright © 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclWinInt.h"
#include "tclFileSystem.h"
#include "tclIO.h"

/*
 * State flags used in the info structures below.
 */

#define FILE_PENDING	(1<<0)	/* Message is pending in the queue. */
#define FILE_ASYNC	(1<<1)	/* Channel is non-blocking. */
#define FILE_APPEND	(1<<2)	/* File is in append mode. */

#define FILE_TYPE_SERIAL  (FILE_TYPE_PIPE+1)
#define FILE_TYPE_CONSOLE (FILE_TYPE_PIPE+2)

/*
 * The following structure contains per-instance data for a file based
 * channel.
 */

typedef struct FileInfo {
    Tcl_Channel channel;	/* Pointer to channel structure. */
    int validMask;		/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, or TCL_EXCEPTION: indicates
				 * which operations are valid on the file. */
    int watchMask;		/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, or TCL_EXCEPTION: indicates
				 * which events should be reported. */
    int flags;			/* State flags, see above for a list. */
    HANDLE handle;		/* Input/output file. */
    struct FileInfo *nextPtr;	/* Pointer to next registered file. */
    int dirty;			/* Boolean flag. Set if the OS may have data
				 * pending on the channel. */
} FileInfo;

typedef struct {
    /*
     * List of all file channels currently open.
     */

    FileInfo *firstFilePtr;
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

/*
 * The following structure is what is added to the Tcl event queue when file
 * events are generated.
 */

typedef struct {
    Tcl_Event header;		/* Information that is standard for all
				 * events. */
    FileInfo *infoPtr;		/* Pointer to file info structure. Note that
				 * we still have to verify that the file
				 * exists before dereferencing this
				 * pointer. */
} FileEvent;

/*
 * Static routines for this file:
 */

static int		FileBlockProc(void *instanceData, int mode);
static void		FileChannelExitHandler(void *clientData);
static void		FileCheckProc(void *clientData, int flags);
static int		FileCloseProc(void *instanceData,
			    Tcl_Interp *interp, int flags);
static int		FileEventProc(Tcl_Event *evPtr, int flags);
static int		FileGetHandleProc(void *instanceData,
			    int direction, void **handlePtr);
static int		FileGetOptionProc(void *instanceData,
			    Tcl_Interp *interp, const char *optionName,
			    Tcl_DString *dsPtr);
static ThreadSpecificData *FileInit(void);
static int		FileInputProc(void *instanceData, char *buf,
			    int toRead, int *errorCode);
static int		FileOutputProc(void *instanceData,
			    const char *buf, int toWrite, int *errorCode);
static long long	FileWideSeekProc(void *instanceData,
			    long long offset, int mode, int *errorCode);
static void		FileSetupProc(void *clientData, int flags);
static void		FileWatchProc(void *instanceData, int mask);
static void		FileThreadActionProc(void *instanceData,
			    int action);
static int		FileTruncateProc(void *instanceData,
			    long long length);
static DWORD		FileGetType(HANDLE handle);
static int		NativeIsComPort(const WCHAR *nativeName);
static Tcl_Channel	OpenFileChannel(HANDLE handle, char *channelName,
			    int permissions, int appendMode);

/*
 * This structure describes the channel type structure for file based IO.
 */

static const Tcl_ChannelType fileChannelType = {
    "file",
    TCL_CHANNEL_VERSION_5,
    NULL,			/* Deprecated. */
    FileInputProc,
    FileOutputProc,
    NULL,			/* Deprecated. */
    NULL,			/* Set option proc. */
    FileGetOptionProc,
    FileWatchProc,
    FileGetHandleProc,
    FileCloseProc,
    FileBlockProc,
    NULL,			/* Flush proc. */
    NULL,			/* Bubbled event handler proc. */
    FileWideSeekProc,
    FileThreadActionProc,
    FileTruncateProc
};

/*
 * General useful clarification macros.
 */

#define SET_FLAG(var, flag)	((var) |= (flag))
#define CLEAR_FLAG(var, flag)	((var) &= ~(flag))
#define TEST_FLAG(value, flag)	(((value) & (flag)) != 0)

/*
 * The number of 100-ns intervals between the Windows system epoch (1601-01-01
 * on the proleptic Gregorian calendar) and the Posix epoch (1970-01-01).
 */

#define POSIX_EPOCH_AS_FILETIME	\
	((long long) 116444736 * (long long) 1000000000)

/*
 *----------------------------------------------------------------------
 *
 * TclWinGenerateChannelName --
 *
 *	This function generates names for channels.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new window and creates an exit handler.
 *
 *----------------------------------------------------------------------
 */
void
TclWinGenerateChannelName(
    char channelName[],		/* Buffer to accept the name. */
    const char *channelTypeName,/* Name of type of channel. */
    void *channelImpl)		/* Pointer to channel implementation
				 * structure, used to generate a unique
				 * ID. */
{
    snprintf(channelName, 16 + TCL_INTEGER_SPACE, "%s%" TCL_Z_MODIFIER "x",
	    channelTypeName, (size_t) channelImpl);
}

/*
 *----------------------------------------------------------------------
 *
 * FileInit --
 *
 *	This function creates the window used to simulate file events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new window and creates an exit handler.
 *
 *----------------------------------------------------------------------
 */

static ThreadSpecificData *
FileInit(void)
{
    ThreadSpecificData *tsdPtr =
	    (ThreadSpecificData *) TclThreadDataKeyGet(&dataKey);

    if (tsdPtr == NULL) {
	tsdPtr = TCL_TSD_INIT(&dataKey);
	tsdPtr->firstFilePtr = NULL;
	Tcl_CreateEventSource(FileSetupProc, FileCheckProc, NULL);
	Tcl_CreateThreadExitHandler(FileChannelExitHandler, NULL);
    }
    return tsdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FileChannelExitHandler --
 *
 *	This function is called to cleanup the channel driver before Tcl is
 *	unloaded.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the communication window.
 *
 *----------------------------------------------------------------------
 */

static void
FileChannelExitHandler(
    TCL_UNUSED(void *))
{
    Tcl_DeleteEventSource(FileSetupProc, FileCheckProc, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * FileSetupProc --
 *
 *	This function is invoked before Tcl_DoOneEvent blocks waiting for an
 *	event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adjusts the block time if needed.
 *
 *----------------------------------------------------------------------
 */

void
FileSetupProc(
    TCL_UNUSED(void *),
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    FileInfo *infoPtr;
    Tcl_Time blockTime = { 0, 0 };
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (!TEST_FLAG(flags, TCL_FILE_EVENTS)) {
	return;
    }

    /*
     * Check to see if there is a ready file. If so, poll.
     */

    for (infoPtr = tsdPtr->firstFilePtr; infoPtr != NULL;
	    infoPtr = infoPtr->nextPtr) {
	if (infoPtr->watchMask) {
	    Tcl_SetMaxBlockTime(&blockTime);
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FileCheckProc --
 *
 *	This function is called by Tcl_DoOneEvent to check the file event
 *	source for events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May queue an event.
 *
 *----------------------------------------------------------------------
 */

static void
FileCheckProc(
    TCL_UNUSED(void *),
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    FileEvent *evPtr;
    FileInfo *infoPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (!TEST_FLAG(flags, TCL_FILE_EVENTS)) {
	return;
    }

    /*
     * Queue events for any ready files that don't already have events queued
     * (caused by persistent states that won't generate WinSock events).
     */

    for (infoPtr = tsdPtr->firstFilePtr; infoPtr != NULL;
	    infoPtr = infoPtr->nextPtr) {
	if (infoPtr->watchMask && !TEST_FLAG(infoPtr->flags, FILE_PENDING)) {
	    SET_FLAG(infoPtr->flags, FILE_PENDING);
	    evPtr = (FileEvent *)Tcl_Alloc(sizeof(FileEvent));
	    evPtr->header.proc = FileEventProc;
	    evPtr->infoPtr = infoPtr;
	    Tcl_QueueEvent((Tcl_Event *) evPtr, TCL_QUEUE_TAIL);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FileEventProc --
 *
 *	This function is invoked by Tcl_ServiceEvent when a file event reaches
 *	the front of the event queue. This function invokes Tcl_NotifyChannel
 *	on the file.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed from
 *	the queue. Returns 0 if the event was not handled, meaning it should
 *	stay on the queue. The only time the event isn't handled is if the
 *	TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the notifier callback does.
 *
 *----------------------------------------------------------------------
 */

static int
FileEventProc(
    Tcl_Event *evPtr,		/* Event to service. */
    int flags)			/* Flags that indicate what events to handle,
				 * such as TCL_FILE_EVENTS. */
{
    FileEvent *fileEvPtr = (FileEvent *)evPtr;
    FileInfo *infoPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (!TEST_FLAG(flags, TCL_FILE_EVENTS)) {
	return 0;
    }

    /*
     * Search through the list of watched files for the one whose handle
     * matches the event. We do this rather than simply dereferencing the
     * handle in the event so that files can be deleted while the event is in
     * the queue.
     */

    for (infoPtr = tsdPtr->firstFilePtr; infoPtr != NULL;
	    infoPtr = infoPtr->nextPtr) {
	if (fileEvPtr->infoPtr == infoPtr) {
	    CLEAR_FLAG(infoPtr->flags, FILE_PENDING);
	    Tcl_NotifyChannel(infoPtr->channel, infoPtr->watchMask);
	    break;
	}
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * FileBlockProc --
 *
 *	Set blocking or non-blocking mode on channel.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or non-blocking mode.
 *
 *----------------------------------------------------------------------
 */

static int
FileBlockProc(
    void *instanceData,		/* Instance data for channel. */
    int mode)			/* TCL_MODE_BLOCKING or
				 * TCL_MODE_NONBLOCKING. */
{
    FileInfo *infoPtr = (FileInfo *)instanceData;

    /*
     * Files on Windows can not be switched between blocking and nonblocking,
     * hence we have to emulate the behavior. This is done in the input
     * function by checking against a bit in the state. We set or unset the
     * bit here to cause the input function to emulate the correct behavior.
     */

    if (mode == TCL_MODE_NONBLOCKING) {
	SET_FLAG(infoPtr->flags, FILE_ASYNC);
    } else {
	CLEAR_FLAG(infoPtr->flags, FILE_ASYNC);
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * FileCloseProc --
 *
 *	Closes the IO channel.
 *
 * Results:
 *	0 if successful, the value of errno if failed.
 *
 * Side effects:
 *	Closes the physical channel
 *
 *----------------------------------------------------------------------
 */

static int
FileCloseProc(
    void *instanceData,		/* Pointer to FileInfo structure. */
    TCL_UNUSED(Tcl_Interp *),
    int flags)
{
    FileInfo *fileInfoPtr = (FileInfo *)instanceData;
    FileInfo *infoPtr;
    ThreadSpecificData *tsdPtr;
    int errorCode = 0;

    if ((flags & (TCL_CLOSE_READ | TCL_CLOSE_WRITE)) != 0) {
	return EINVAL;
    }

    /*
     * Remove the file from the watch list.
     */

    FileWatchProc(instanceData, 0);

    /*
     * Don't close the Win32 handle if the handle is a standard channel during
     * the thread exit process. Otherwise, one thread may kill the stdio of
     * another.
     */

    if (!TclInThreadExit()
	    || ((GetStdHandle(STD_INPUT_HANDLE) != fileInfoPtr->handle)
	    &&  (GetStdHandle(STD_OUTPUT_HANDLE) != fileInfoPtr->handle)
	    &&  (GetStdHandle(STD_ERROR_HANDLE) != fileInfoPtr->handle))) {
	if (CloseHandle(fileInfoPtr->handle) == FALSE) {
	    Tcl_WinConvertError(GetLastError());
	    errorCode = errno;
	}
    }

    /*
     * See if this FileInfo* is still on the thread local list.
     */

    tsdPtr = TCL_TSD_INIT(&dataKey);
    for (infoPtr = tsdPtr->firstFilePtr; infoPtr != NULL;
	    infoPtr = infoPtr->nextPtr) {
	if (infoPtr == fileInfoPtr) {
	    /*
	     * This channel exists on the thread local list. It should have
	     * been removed by an earlier Threadaction call, but do that now
	     * since just deallocating fileInfoPtr would leave an deallocated
	     * pointer on the thread local list.
	     */

	    FileThreadActionProc(fileInfoPtr,TCL_CHANNEL_THREAD_REMOVE);
	    break;
	}
    }
    Tcl_Free(fileInfoPtr);
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * FileWideSeekProc --
 *
 *	Seeks on a file-based channel. Returns the new position.
 *
 * Results:
 *	-1 if failed, the new position if successful. If failed, it also sets
 *	*errorCodePtr to the error code.
 *
 * Side effects:
 *	Moves the location at which the channel will be accessed in future
 *	operations.
 *
 *----------------------------------------------------------------------
 */

static long long
FileWideSeekProc(
    void *instanceData,		/* File state. */
    long long offset,		/* Offset to seek to. */
    int mode,			/* Relative to where should we seek? */
    int *errorCodePtr)		/* To store error code. */
{
    FileInfo *infoPtr = (FileInfo *)instanceData;
    DWORD moveMethod;
    LONG newPos, newPosHigh;

    *errorCodePtr = 0;
    if (mode == SEEK_SET) {
	moveMethod = FILE_BEGIN;
    } else if (mode == SEEK_CUR) {
	moveMethod = FILE_CURRENT;
    } else {
	moveMethod = FILE_END;
    }

    newPosHigh = (LONG)(offset >> 32);
    newPos = SetFilePointer(infoPtr->handle, (LONG)offset,
	    &newPosHigh, moveMethod);
    if (newPos == (LONG) INVALID_SET_FILE_POINTER) {
	DWORD winError = GetLastError();

	if (winError != NO_ERROR) {
	    Tcl_WinConvertError(winError);
	    *errorCodePtr = errno;
	    return -1;
	}
    }
    return (((long long)((unsigned)newPos))
	    | ((long long)newPosHigh << 32));
}

/*
 *----------------------------------------------------------------------
 *
 * FileTruncateProc --
 *
 *	Truncates a file-based channel. Returns the error code.
 *
 * Results:
 *	0 if successful, POSIX-y error code if it failed.
 *
 * Side effects:
 *	Truncates the file, may move file pointers too.
 *
 *----------------------------------------------------------------------
 */

static int
FileTruncateProc(
    void *instanceData,		/* File state. */
    long long length)		/* Length to truncate at. */
{
    FileInfo *infoPtr = (FileInfo *)instanceData;
    LONG newPos, newPosHigh, oldPos, oldPosHigh;

    /*
     * Save where we were...
     */

    oldPosHigh = 0;
    oldPos = SetFilePointer(infoPtr->handle, 0, &oldPosHigh, FILE_CURRENT);
    if (oldPos == (LONG) INVALID_SET_FILE_POINTER) {
	DWORD winError = GetLastError();

	if (winError != NO_ERROR) {
	    Tcl_WinConvertError(winError);
	    return errno;
	}
    }

    /*
     * Move to where we want to truncate
     */

    newPosHigh = (LONG)(length >> 32);
    newPos = SetFilePointer(infoPtr->handle, (LONG)length,
	    &newPosHigh, FILE_BEGIN);
    if (newPos == (LONG) INVALID_SET_FILE_POINTER) {
	DWORD winError = GetLastError();

	if (winError != NO_ERROR) {
	    Tcl_WinConvertError(winError);
	    return errno;
	}
    }

    /*
     * Perform the truncation (unlike POSIX ftruncate(), we needed to move to
     * the location to truncate at first).
     */

    if (!SetEndOfFile(infoPtr->handle)) {
	Tcl_WinConvertError(GetLastError());
	return errno;
    }

    /*
     * Move back. If this last step fails, we don't care; it's just a "best
     * effort" attempt to restore our file pointer to where it was.
     */

    SetFilePointer(infoPtr->handle, oldPos, &oldPosHigh, FILE_BEGIN);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * FileInputProc --
 *
 *	Reads input from the IO channel into the buffer given. Returns count
 *	of how many bytes were actually read, and an error indication.
 *
 * Results:
 *	A count of how many bytes were read is returned and an error
 *	indication is returned in an output argument.
 *
 * Side effects:
 *	Reads input from the actual channel.
 *
 *----------------------------------------------------------------------
 */

static int
FileInputProc(
    void *instanceData,		/* File state. */
    char *buf,			/* Where to store data read. */
    int bufSize,		/* Num bytes available in buffer. */
    int *errorCode)		/* Where to store error code. */
{
    FileInfo *infoPtr = (FileInfo *)instanceData;
    DWORD bytesRead;

    *errorCode = 0;

    /*
     * TODO: This comment appears to be out of date. We *do* have a console
     * driver, over in tclWinConsole.c. After some Windows developer confirms,
     * this comment should be revised.
     *
     * Note that we will block on reads from a console buffer until a full
     * line has been entered. The only way I know of to get around this is to
     * write a console driver. We should probably do this at some point, but
     * for now, we just block. The same problem exists for files being read
     * over the network.
     */

    if (ReadFile(infoPtr->handle, (LPVOID) buf, (DWORD) bufSize, &bytesRead,
	    (LPOVERLAPPED) NULL) != FALSE) {
	return (int)bytesRead;
    }

    Tcl_WinConvertError(GetLastError());
    *errorCode = errno;
    if (errno == EPIPE) {
	return 0;
    }
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * FileOutputProc --
 *
 *	Writes the given output on the IO channel. Returns count of how many
 *	characters were actually written, and an error indication.
 *
 * Results:
 *	A count of how many characters were written is returned and an error
 *	indication is returned in an output argument.
 *
 * Side effects:
 *	Writes output on the actual channel.
 *
 *----------------------------------------------------------------------
 */

static int
FileOutputProc(
    void *instanceData,		/* File state. */
    const char *buf,		/* The data buffer. */
    int toWrite,		/* How many bytes to write? */
    int *errorCode)		/* Where to store error code. */
{
    FileInfo *infoPtr = (FileInfo *)instanceData;
    DWORD bytesWritten;

    *errorCode = 0;

    /*
     * If we are writing to a file that was opened with O_APPEND, we need to
     * seek to the end of the file before writing the current buffer.
     */

    if (TEST_FLAG(infoPtr->flags, FILE_APPEND)) {
	SetFilePointer(infoPtr->handle, 0, NULL, FILE_END);
    }

    if (WriteFile(infoPtr->handle, (LPVOID) buf, (DWORD) toWrite,
	    &bytesWritten, (LPOVERLAPPED) NULL) == FALSE) {
	Tcl_WinConvertError(GetLastError());
	*errorCode = errno;
	return -1;
    }
    infoPtr->dirty = 1;
    return (int)bytesWritten;
}

/*
 *----------------------------------------------------------------------
 *
 * FileWatchProc --
 *
 *	Called by the notifier to set up to watch for events on this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
FileWatchProc(
    void *instanceData,		/* File state. */
    int mask)			/* What events to watch for; OR-ed combination
				 * of TCL_READABLE, TCL_WRITABLE and
				 * TCL_EXCEPTION. */
{
    FileInfo *infoPtr = (FileInfo *)instanceData;
    Tcl_Time blockTime = { 0, 0 };

    /*
     * Since the file is always ready for events, we set the block time to
     * zero so we will poll.
     */

    infoPtr->watchMask = mask & infoPtr->validMask;
    if (infoPtr->watchMask) {
	Tcl_SetMaxBlockTime(&blockTime);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FileGetHandleProc --
 *
 *	Called from Tcl_GetChannelHandle to retrieve OS handles from a file
 *	based channel.
 *
 * Results:
 *	Returns TCL_OK with the fd in handlePtr, or TCL_ERROR if there is no
 *	handle for the specified direction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
FileGetHandleProc(
    void *instanceData,		/* The file state. */
    int direction,		/* TCL_READABLE or TCL_WRITABLE */
    void **handlePtr)		/* Where to store the handle. */
{
    FileInfo *infoPtr = (FileInfo *)instanceData;

    if (!TEST_FLAG(direction, infoPtr->validMask)) {
	return TCL_ERROR;
    }

    *handlePtr = (void *)infoPtr->handle;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FileGetOptionProc --
 *
 *	Gets an option associated with an open file. If the optionName arg is
 *	non-NULL, retrieves the value of that option. If the optionName arg is
 *	NULL, retrieves a list of alternating option names and values for the
 *	given channel.
 *
 * Results:
 *	A standard Tcl result. Also sets the supplied DString to the string
 *	value of the option(s) returned.  Sets error message if needed
 *	(by calling Tcl_BadChannelOption).
 *
 *----------------------------------------------------------------------
 */

static inline ULONGLONG
CombineDwords(
    DWORD hi,
    DWORD lo)
{
    ULARGE_INTEGER converter;

    converter.LowPart = lo;
    converter.HighPart = hi;
    return converter.QuadPart;
}

static inline time_t
ToCTime(
    FILETIME fileTime)		/* UTC time */
{
    LARGE_INTEGER convertedTime;

    convertedTime.LowPart = fileTime.dwLowDateTime;
    convertedTime.HighPart = (LONG) fileTime.dwHighDateTime;

    return (time_t) ((convertedTime.QuadPart -
	    (long long) POSIX_EPOCH_AS_FILETIME) / (long long) 10000000);
}

static Tcl_Obj *
StatOpenFile(
    FileInfo *infoPtr)
{
    DWORD attr;
    int dev, nlink = 1;
    int mode;
    unsigned long long size, inode;
    long long atime, ctime, mtime;
    BY_HANDLE_FILE_INFORMATION data;
    Tcl_Obj *dictObj;

    if (GetFileInformationByHandle(infoPtr->handle, &data) != TRUE) {
	Tcl_SetErrno(ENOENT);
	return NULL;
    }

    atime = ToCTime(data.ftLastAccessTime);
    mtime = ToCTime(data.ftLastWriteTime);
    ctime = ToCTime(data.ftCreationTime);
    attr = data.dwFileAttributes;
    size = CombineDwords(data.nFileSizeHigh, data.nFileSizeLow);
    nlink = data.nNumberOfLinks;

    /*
     * Unfortunately our stat definition's inode field (unsigned short) will
     * throw away most of the precision we have here, which means we can't
     * rely on inode as a unique identifier of a file. We'd really like to do
     * something like how we handle 'st_size'.
     */

    inode = CombineDwords(data.nFileIndexHigh, data.nFileIndexLow);

    dev = data.dwVolumeSerialNumber;

    /*
     * Note that this code has no idea whether the file can be executed.
     */

    mode = (attr & FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR|S_IEXEC : S_IFREG;
    mode = mode | (attr & FILE_ATTRIBUTE_READONLY) ? S_IREAD : S_IREAD|S_IWRITE;
    mode = mode | (unsigned short)((mode & (S_IREAD|S_IWRITE|S_IEXEC)) >> 3);
    mode = mode | (unsigned short)((mode & (S_IREAD|S_IWRITE|S_IEXEC)) >> 6);

    /*
     * We don't construct a Tcl_StatBuf; we're using the info immediately.
     */

    TclNewObj(dictObj);
#define STORE_ELEM(name, value) TclDictPut(NULL, dictObj, name, value)

    STORE_ELEM("dev",      Tcl_NewWideIntObj((long) dev));
    STORE_ELEM("ino",      Tcl_NewWideIntObj((long long) inode));
    STORE_ELEM("nlink",    Tcl_NewIntObj(nlink));
    STORE_ELEM("uid",      Tcl_NewIntObj(0));
    STORE_ELEM("gid",      Tcl_NewIntObj(0));
    STORE_ELEM("size",     Tcl_NewWideIntObj((long long) size));
    STORE_ELEM("atime",    Tcl_NewWideIntObj(atime));
    STORE_ELEM("mtime",    Tcl_NewWideIntObj(mtime));
    STORE_ELEM("ctime",    Tcl_NewWideIntObj(ctime));
    STORE_ELEM("mode",     Tcl_NewWideIntObj(mode));

    /*
     * Windows only has files and directories, as far as we're concerned.
     * Anything else and we definitely couldn't have got here anyway.
     */
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
	STORE_ELEM("type", Tcl_NewStringObj("directory", TCL_INDEX_NONE));
    } else {
	STORE_ELEM("type", Tcl_NewStringObj("file", TCL_INDEX_NONE));
    }
#undef STORE_ELEM

    return dictObj;
}

static int
FileGetOptionProc(
    void *instanceData,		/* The file state. */
    Tcl_Interp *interp,		/* For error reporting. */
    const char *optionName,	/* What option to read, or NULL for all. */
    Tcl_DString *dsPtr)		/* Where to write the value read. */
{
    FileInfo *infoPtr = (FileInfo *)instanceData;
    int valid = 0;		/* Flag if valid option parsed. */
    size_t len;

    if (optionName == NULL) {
	len = 0;
	valid = 1;
    } else {
	len = strlen(optionName);
    }

    /*
     * Get option -stat
     * Option is readonly and returned by [fconfigure chan -stat] but not
     * returned by [fconfigure chan] without explicit option name.
     */

    if ((len > 1) && (strncmp(optionName, "-stat", len) == 0)) {
	Tcl_Obj *dictObj = StatOpenFile(infoPtr);
	const char *dictContents;
	Tcl_Size dictLength;

	if (dictObj == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "couldn't read file channel status: %s",
		    Tcl_PosixError(interp)));
	    return TCL_ERROR;
	}

	/*
	 * Transfer dictionary to the DString. Note that we don't do this as
	 * an element as this is an option that can't be retrieved with a
	 * general probe.
	 */

	dictContents = TclGetStringFromObj(dictObj, &dictLength);
	Tcl_DStringAppend(dsPtr, dictContents, dictLength);
	Tcl_DecrRefCount(dictObj);
	return TCL_OK;
    }

    if (valid) {
	return TCL_OK;
    }
    return Tcl_BadChannelOption(interp, optionName,
		"stat");
}

/*
 *----------------------------------------------------------------------
 *
 * TclpOpenFileChannel --
 *
 *	Open an File based channel on Unix systems.
 *
 * Results:
 *	The new channel or NULL. If NULL, the output argument errorCodePtr is
 *	set to a POSIX error.
 *
 * Side effects:
 *	May open the channel and may cause creation of a file on the file
 *	system.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclpOpenFileChannel(
    Tcl_Interp *interp,		/* Interpreter for error reporting; can be
				 * NULL. */
    Tcl_Obj *pathPtr,		/* Name of file to open. */
    int mode,			/* POSIX mode. */
    int permissions)		/* If the open involves creating a file, with
				 * what modes to create it? */
{
    Tcl_Channel channel = 0;
    int channelPermissions = 0;
    DWORD accessMode = 0, createMode, shareMode, flags;
    const WCHAR *nativeName;
    HANDLE handle;
    char channelName[16 + TCL_INTEGER_SPACE];
    TclFile readFile = NULL, writeFile = NULL;

    nativeName = (const WCHAR *)Tcl_FSGetNativePath(pathPtr);
    if (nativeName == NULL) {
	if (interp) {
	    /*
	     * We need this just to ensure we return the correct error messages under
	     * some circumstances (relative paths only), so because the normalization
	     * is very expensive, don't invoke it for native or absolute paths.
	     * Note: since paths starting with ~ are relative in 9.0 for windows,
	     * it doesn't need to consider tilde expansion (in opposite to 8.x).
	     */
	    if (!TclFSCwdIsNative()
		    && (Tcl_FSGetPathType(pathPtr) != TCL_PATH_ABSOLUTE)
		    && Tcl_FSGetNormalizedPath(interp, pathPtr) == NULL) {
		return NULL;
	    }

	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "couldn't open \"%s\": filename is invalid on this platform",
		    TclGetString(pathPtr)));
	}
	return NULL;
    }

    switch (mode & O_ACCMODE) {
    case O_RDONLY:
	accessMode = GENERIC_READ;
	channelPermissions = TCL_READABLE;
	break;
    case O_WRONLY:
	accessMode = GENERIC_WRITE;
	channelPermissions = TCL_WRITABLE;
	break;
    case O_RDWR:
	accessMode = (GENERIC_READ | GENERIC_WRITE);
	channelPermissions = (TCL_READABLE | TCL_WRITABLE);
	break;
    default:
	Tcl_Panic("TclpOpenFileChannel: invalid mode value");
	break;
    }

    /*
     * Map the creation flags to the NT create mode.
     */

    switch (mode & (O_CREAT | O_EXCL | O_TRUNC)) {
    case (O_CREAT | O_EXCL):
    case (O_CREAT | O_EXCL | O_TRUNC):
	createMode = CREATE_NEW;
	break;
    case (O_CREAT | O_TRUNC):
	createMode = CREATE_ALWAYS;
	break;
    case O_CREAT:
	createMode = OPEN_ALWAYS;
	break;
    case O_TRUNC:
    case (O_TRUNC | O_EXCL):
	createMode = TRUNCATE_EXISTING;
	break;
    default:
	createMode = OPEN_EXISTING;
	break;
    }

    /*
     * [2413550] Avoid double-open of serial ports on Windows.  Special
     * handling for Windows serial ports by a "name-hint" to directly open it
     * with the OVERLAPPED flag set.
     */

    if (NativeIsComPort(nativeName)) {
	handle = TclWinSerialOpen(INVALID_HANDLE_VALUE, nativeName, accessMode);
	if (handle == INVALID_HANDLE_VALUE) {
	    Tcl_WinConvertError(GetLastError());
	    if (interp) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"couldn't open serial \"%s\": %s",
			TclGetString(pathPtr), Tcl_PosixError(interp)));
	    }
	    return NULL;
	}

	/*
	 * For natively-named Windows serial ports we are done.
	 */

	channel = TclWinOpenSerialChannel(handle, channelName,
		channelPermissions);

	return channel;
    }

    /*
     * If the file is being created, get the file attributes from the
     * permissions argument, else use the existing file attributes.
     */

    if (TEST_FLAG(mode, O_CREAT)) {
	if (TEST_FLAG(permissions, S_IWRITE)) {
	    flags = FILE_ATTRIBUTE_NORMAL;
	} else {
	    flags = FILE_ATTRIBUTE_READONLY;
	}
    } else {
	flags = GetFileAttributesW(nativeName);
	if (flags == 0xFFFFFFFF) {
	    flags = 0;
	}
    }

    /*
     * Set up the file sharing mode.  We want to allow simultaneous access.
     */

    shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    /*
     * Now we get to create the file.
     */

    handle = CreateFileW(nativeName, accessMode, shareMode,
	    NULL, createMode, flags, (HANDLE) NULL);

    if (handle == INVALID_HANDLE_VALUE) {
	DWORD err = GetLastError();

	if ((err & 0xFFFFL) == ERROR_OPEN_FAILED) {
	    err = TEST_FLAG(mode, O_CREAT) ? ERROR_FILE_EXISTS
		    : ERROR_FILE_NOT_FOUND;
	}
	Tcl_WinConvertError(err);
	if (interp) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "couldn't open \"%s\": %s",
		    TclGetString(pathPtr), Tcl_PosixError(interp)));
	}
	return NULL;
    }

    channel = NULL;

    switch (FileGetType(handle)) {
    case FILE_TYPE_SERIAL:
	/*
	 * Natively named serial ports "com1-9", "\\\\.\\comXX" are already
	 * done with the code above.  Here we handle all other serial port
	 * names.
	 *
	 * Reopen channel for OVERLAPPED operation. Normally this shouldn't
	 * fail, because the channel exists.
	 */

	handle = TclWinSerialOpen(handle, nativeName, accessMode);
	if (handle == INVALID_HANDLE_VALUE) {
	    Tcl_WinConvertError(GetLastError());
	    if (interp) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"couldn't reopen serial \"%s\": %s",
			TclGetString(pathPtr), Tcl_PosixError(interp)));
	    }
	    return NULL;
	}
	channel = TclWinOpenSerialChannel(handle, channelName,
		channelPermissions);
	break;
    case FILE_TYPE_CONSOLE:
	channel = TclWinOpenConsoleChannel(handle, channelName,
		channelPermissions);
	break;
    case FILE_TYPE_PIPE:
	if (TEST_FLAG(channelPermissions, TCL_READABLE)) {
	    readFile = TclWinMakeFile(handle);
	}
	if (TEST_FLAG(channelPermissions, TCL_WRITABLE)) {
	    writeFile = TclWinMakeFile(handle);
	}
	channel = TclpCreateCommandChannel(readFile, writeFile, NULL, 0, NULL);
	break;
    case FILE_TYPE_CHAR:
    case FILE_TYPE_DISK:
    case FILE_TYPE_UNKNOWN:
	channel = OpenFileChannel(handle, channelName,
		channelPermissions,
		TEST_FLAG(mode, O_APPEND) ? FILE_APPEND : 0);
	break;

    default:
	/*
	 * The handle is of an unknown type, probably /dev/nul equivalent or
	 * possibly a closed handle.
	 */

	channel = NULL;
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"couldn't open \"%s\": bad file type",
		TclGetString(pathPtr)));
	Tcl_SetErrorCode(interp, "TCL", "VALUE", "CHANNEL", "BAD_TYPE",
		(char *)NULL);
	break;
    }

    return channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MakeFileChannel --
 *
 *	Creates a Tcl_Channel from an existing platform specific file handle.
 *
 * Results:
 *	The Tcl_Channel created around the preexisting file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_MakeFileChannel(
    void *rawHandle,		/* OS level handle */
    int mode)			/* OR'ed combination of TCL_READABLE and
				 * TCL_WRITABLE to indicate file mode. */
{
#if defined(HAVE_NO_SEH) && !defined(_WIN64) && !defined(__clang__)
    TCLEXCEPTION_REGISTRATION registration;
#endif
    char channelName[16 + TCL_INTEGER_SPACE];
    Tcl_Channel channel = NULL;
    HANDLE handle = (HANDLE) rawHandle;
    HANDLE dupedHandle;
    TclFile readFile = NULL, writeFile = NULL;
    BOOL result;

    if ((mode & (TCL_READABLE|TCL_WRITABLE)) == 0) {
	return NULL;
    }

    switch (FileGetType(handle)) {
    case FILE_TYPE_SERIAL:
	channel = TclWinOpenSerialChannel(handle, channelName, mode);
	break;
    case FILE_TYPE_CONSOLE:
	channel = TclWinOpenConsoleChannel(handle, channelName, mode);
	break;
    case FILE_TYPE_PIPE:
	if (TEST_FLAG(mode, TCL_READABLE)) {
	    readFile = TclWinMakeFile(handle);
	}
	if (TEST_FLAG(mode, TCL_WRITABLE)) {
	    writeFile = TclWinMakeFile(handle);
	}
	channel = TclpCreateCommandChannel(readFile, writeFile, NULL, 0, NULL);
	break;

    case FILE_TYPE_DISK:
    case FILE_TYPE_CHAR:
	channel = OpenFileChannel(handle, channelName, mode, 0);
	break;

    case FILE_TYPE_UNKNOWN:
    default:
	/*
	 * The handle is of an unknown type. Test the validity of this OS
	 * handle by duplicating it, then closing the dupe. The Win32 API
	 * doesn't provide an IsValidHandle() function, so we have to emulate
	 * it here. This test will not work on a console handle reliably,
	 * which is why we can't test every handle that comes into this
	 * function in this way.
	 */

	result = DuplicateHandle(GetCurrentProcess(), handle,
		GetCurrentProcess(), &dupedHandle, 0, FALSE,
		DUPLICATE_SAME_ACCESS);

	if (result == 0) {
	    /*
	     * Unable to make a duplicate. It's definitely invalid at this
	     * point.
	     */

	    return NULL;
	}

	/*
	 * Use structured exception handling (Win32 SEH) to protect the close
	 * of this duped handle which might throw EXCEPTION_INVALID_HANDLE.
	 */

	result = 0;
#if defined(HAVE_NO_SEH) && !defined(_WIN64) && !defined(__clang__)
	/*
	 * Don't have SEH available, do things the hard way. Note that this
	 * needs to be one block of asm, to avoid stack imbalance; also, it is
	 * illegal for one asm block to contain a jump to another.
	 */

	__asm__ __volatile__ (

	    /*
	     * Pick up parameters before messing with the stack
	     */

	    "movl       %[dupedHandle], %%ebx"          "\n\t"

	    /*
	     * Construct an TCLEXCEPTION_REGISTRATION to protect the call to
	     * CloseHandle.
	     */

	    "leal       %[registration], %%edx"         "\n\t"
	    "movl       %%fs:0,         %%eax"          "\n\t"
	    "movl       %%eax,          0x0(%%edx)"     "\n\t" /* link */
	    "leal       1f,             %%eax"          "\n\t"
	    "movl       %%eax,          0x4(%%edx)"     "\n\t" /* handler */
	    "movl       %%ebp,          0x8(%%edx)"     "\n\t" /* ebp */
	    "movl       %%esp,          0xC(%%edx)"     "\n\t" /* esp */
	    "movl       $0,             0x10(%%edx)"    "\n\t" /* status */

	    /*
	     * Link the TCLEXCEPTION_REGISTRATION on the chain.
	     */

	    "movl       %%edx,          %%fs:0"         "\n\t"

	    /*
	     * Call CloseHandle(dupedHandle).
	     */

	    "pushl      %%ebx"                          "\n\t"
	    "call       _CloseHandle@4"                 "\n\t"

	    /*
	     * Come here on normal exit. Recover the TCLEXCEPTION_REGISTRATION
	     * and put a TRUE status return into it.
	     */

	    "movl       %%fs:0,         %%edx"          "\n\t"
	    "movl	$1,		%%eax"		"\n\t"
	    "movl       %%eax,          0x10(%%edx)"    "\n\t"
	    "jmp        2f"                             "\n"

	    /*
	     * Come here on an exception. Recover the TCLEXCEPTION_REGISTRATION
	     */

	    "1:"                                        "\t"
	    "movl       %%fs:0,         %%edx"          "\n\t"
	    "movl       0x8(%%edx),     %%edx"          "\n\t"

	    /*
	     * Come here however we exited. Restore context from the
	     * TCLEXCEPTION_REGISTRATION in case the stack is unbalanced.
	     */

	    "2:"                                        "\t"
	    "movl       0xC(%%edx),     %%esp"          "\n\t"
	    "movl       0x8(%%edx),     %%ebp"          "\n\t"
	    "movl       0x0(%%edx),     %%eax"          "\n\t"
	    "movl       %%eax,          %%fs:0"         "\n\t"

	    :
	    /* No outputs */
	    :
	    [registration]  "m"     (registration),
	    [dupedHandle]   "m"	    (dupedHandle)
	    :
	    "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory"
	    );
	result = registration.status;
#else
#ifndef HAVE_NO_SEH
	__try {
#endif
	    CloseHandle(dupedHandle);
	    result = 1;
#ifndef HAVE_NO_SEH
	} __except (EXCEPTION_EXECUTE_HANDLER) {}
#endif
#endif
	if (result == FALSE) {
	    return NULL;
	}

	/*
	 * Fall through, the handle is valid.
	 *
	 * Create the undefined channel, anyways, because we know the handle
	 * is valid to something.
	 */

	channel = OpenFileChannel(handle, channelName, mode, 0);
    }

    return channel;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetDefaultStdChannel --
 *
 *	Constructs a channel for the specified standard OS handle.
 *
 * Results:
 *	Returns the specified default standard channel, or NULL.
 *
 * Side effects:
 *	May cause the creation of a standard channel and the underlying file.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclpGetDefaultStdChannel(
    int type)			/* One of TCL_STDIN, TCL_STDOUT, or
				 * TCL_STDERR. */
{
    Tcl_Channel channel;
    HANDLE handle;
    int mode = -1;
    const char *bufMode = NULL;
    DWORD handleId = (DWORD) -1;
				/* Standard handle to retrieve. */

    switch (type) {
    case TCL_STDIN:
	handleId = STD_INPUT_HANDLE;
	mode = TCL_READABLE;
	bufMode = "line";
	break;
    case TCL_STDOUT:
	handleId = STD_OUTPUT_HANDLE;
	mode = TCL_WRITABLE;
	bufMode = "line";
	break;
    case TCL_STDERR:
	handleId = STD_ERROR_HANDLE;
	mode = TCL_WRITABLE;
	bufMode = "none";
	break;
    default:
	Tcl_Panic("TclGetDefaultStdChannel: Unexpected channel type");
	break;
    }

    handle = GetStdHandle(handleId);

    /*
     * Note that we need to check for 0 because Windows may return 0 if this
     * is not a console mode application, even though this is not a valid
     * handle.
     */

    if ((handle == INVALID_HANDLE_VALUE) || (handle == 0)) {
	return (Tcl_Channel) NULL;
    }

    channel = Tcl_MakeFileChannel(handle, mode);

    if (channel == NULL) {
	return (Tcl_Channel) NULL;
    }

    /*
     * Set up the normal channel options for stdio handles.
     */

    if (Tcl_SetChannelOption(NULL,channel,"-translation","auto")!=TCL_OK ||
	    Tcl_SetChannelOption(NULL,channel,"-buffering",bufMode)!=TCL_OK) {
	Tcl_CloseEx(NULL, channel, 0);
	return (Tcl_Channel) NULL;
    }
    return channel;
}

/*
 *----------------------------------------------------------------------
 *
 * OpenFileChannel --
 *
 *	Constructs a File channel for the specified standard OS handle. This
 *	is a helper function to break up the construction of channels into
 *	File, Console, or Serial.
 *
 * Results:
 *	Returns the new channel, or NULL.
 *
 * Side effects:
 *	May open the channel and may cause creation of a file on the file
 *	system.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
OpenFileChannel(
    HANDLE handle,		/* Win32 HANDLE to swallow */
    char *channelName,		/* Buffer to receive channel name */
    int permissions,		/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, or TCL_EXCEPTION, indicating
				 * which operations are valid on the file. */
    int appendMode)		/* OR'ed combination of bits indicating what
				 * additional configuration of the channel is
				 * present. */
{
    FileInfo *infoPtr;
    ThreadSpecificData *tsdPtr = FileInit();

    /*
     * See if a channel with this handle already exists.
     */

    for (infoPtr = tsdPtr->firstFilePtr; infoPtr != NULL;
	    infoPtr = infoPtr->nextPtr) {
	if (infoPtr->handle == (HANDLE) handle) {
	    return ((permissions & (TCL_READABLE|TCL_WRITABLE|TCL_EXCEPTION))==infoPtr->validMask)
		    ? infoPtr->channel : NULL;
	}
    }

    infoPtr = (FileInfo *)Tcl_Alloc(sizeof(FileInfo));

    /*
     * TIP #218. Removed the code inserting the new structure into the global
     * list. This is now handled in the thread action callbacks, and only
     * there.
     */

    infoPtr->nextPtr = NULL;
    infoPtr->validMask = permissions & (TCL_READABLE|TCL_WRITABLE|TCL_EXCEPTION);
    infoPtr->watchMask = 0;
    infoPtr->flags = appendMode;
    infoPtr->handle = handle;
    infoPtr->dirty = 0;
    TclWinGenerateChannelName(channelName, "file", infoPtr);
    infoPtr->channel = Tcl_CreateChannel(&fileChannelType, channelName,
	    infoPtr, permissions);

    /*
     * Files have default translation of AUTO and ^Z eof char, which means
     * that a ^Z will be accepted as EOF when reading.
     */

    Tcl_SetChannelOption(NULL, infoPtr->channel, "-translation", "auto");

    return infoPtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinFlushDirtyChannels --
 *
 *	Flush all dirty channels to disk, so that requesting the size of any
 *	file returns the correct value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information is actually written to disk now, rather than later. Don't
 *	call this too often, or there will be a performance hit (i.e. only
 *	call when we need to ask for the size of a file).
 *
 *----------------------------------------------------------------------
 */

void
TclWinFlushDirtyChannels(void)
{
    FileInfo *infoPtr;
    ThreadSpecificData *tsdPtr = FileInit();

    /*
     * Flush all channels which are dirty, i.e. may have data pending in the
     * OS.
     */

    for (infoPtr = tsdPtr->firstFilePtr; infoPtr != NULL;
	    infoPtr = infoPtr->nextPtr) {
	if (infoPtr->dirty) {
	    FlushFileBuffers(infoPtr->handle);
	    infoPtr->dirty = 0;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FileThreadActionProc --
 *
 *	Insert or remove any thread local refs to this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes thread local list of valid channels.
 *
 *----------------------------------------------------------------------
 */

static void
FileThreadActionProc(
    void *instanceData,
    int action)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    FileInfo *infoPtr = (FileInfo *)instanceData;

    if (action == TCL_CHANNEL_THREAD_INSERT) {
	infoPtr->nextPtr = tsdPtr->firstFilePtr;
	tsdPtr->firstFilePtr = infoPtr;
    } else {
	FileInfo **nextPtrPtr;
	int removed = 0;

	for (nextPtrPtr = &(tsdPtr->firstFilePtr); (*nextPtrPtr) != NULL;
		nextPtrPtr = &((*nextPtrPtr)->nextPtr)) {
	    if ((*nextPtrPtr) == infoPtr) {
		(*nextPtrPtr) = infoPtr->nextPtr;
		removed = 1;
		break;
	    }
	}

	/*
	 * This could happen if the channel was created in one thread and then
	 * moved to another without updating the thread local data in each
	 * thread.
	 */

	if (!removed) {
	    Tcl_Panic("file info ptr not on thread channel list");
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FileGetType --
 *
 *	Given a file handle, return its type
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

DWORD
FileGetType(
    HANDLE handle)		/* Opened file handle */
{
    DWORD type;

    type = GetFileType(handle);

    /*
     * If the file is a character device, we need to try to figure out whether
     * it is a serial port, a console, or something else. We test for the
     * console case first because this is more common.
     */

    if ((type == FILE_TYPE_CHAR)
	    || ((type == FILE_TYPE_UNKNOWN) && !GetLastError())) {
	DWORD consoleParams;

	if (GetConsoleMode(handle, &consoleParams)) {
	    type = FILE_TYPE_CONSOLE;
	} else {
	    DCB dcb;

	    dcb.DCBlength = sizeof(DCB);
	    if (GetCommState(handle, &dcb)) {
		type = FILE_TYPE_SERIAL;
	    }
	}
    }

    return type;
}

 /*
 *----------------------------------------------------------------------
 *
 * NativeIsComPort --
 *
 *	Determines if a path refers to a Windows serial port.  A simple and
 *	efficient solution is to use a "name hint" to detect COM ports by
 *	their filename instead of resorting to a syscall to detect serialness
 *	after the fact.
 *
 *	The following patterns cover common serial port names:
 *	    COM[1-9]
 *	    \\.\COM[0-9]+
 *
 * Results:
 *	1 = serial port, 0 = not.
 *
 *----------------------------------------------------------------------
 */

static int
NativeIsComPort(
    const WCHAR *nativePath)	/* Path of file to access, native encoding. */
{
    const WCHAR *p = (const WCHAR *) nativePath;
    size_t i, len = wcslen(p);

    /*
     * 1. Look for com[1-9]:?
     */

    if ((len == 4) && (_wcsnicmp(p, L"com", 3) == 0)) {
	/*
	 * The 4th character must be a digit 1..9
	 */

	if ((p[3] < '1') || (p[3] > '9')) {
	    return 0;
	}
	return 1;
    }

    /*
     * 2. Look for \\.\com[0-9]+
     */

    if ((len >= 8) && (_wcsnicmp(p, L"\\\\.\\com", 7) == 0)) {
	/*
	 * Charaters 8..end must be a digits 0..9
	 */

	for (i=7; i<len; i++) {
	    if ((p[i] < '0') || (p[i] > '9')) {
		return 0;
	    }
	}
	return 1;
    }
    return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
