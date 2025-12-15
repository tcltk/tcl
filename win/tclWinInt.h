/*
 * tclWinInt.h --
 *
 *	Declarations of Windows-specific shared variables and procedures.
 *
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLWININT
#define _TCLWININT

#include "tclInt.h"

/*
 * Declarations of functions that are not accessible by way of the
 * stubs table.
 */

MODULE_SCOPE char	TclWinDriveLetterForVolMountPoint(
			    const WCHAR *mountPoint);
MODULE_SCOPE void	TclWinEncodingsCleanup(void);
MODULE_SCOPE void	TclWinInit(HINSTANCE hInst);
MODULE_SCOPE TclFile	TclWinMakeFile(HANDLE handle);
MODULE_SCOPE Tcl_Channel TclWinOpenConsoleChannel(HANDLE handle,
			    char *channelName, int permissions);
MODULE_SCOPE Tcl_Channel TclWinOpenSerialChannel(HANDLE handle,
			    char *channelName, int permissions);
MODULE_SCOPE HANDLE	TclWinSerialOpen(HANDLE handle, const WCHAR *name,
			    DWORD access);
MODULE_SCOPE int	TclWinSymLinkCopyDirectory(const WCHAR *LinkOriginal,
			    const WCHAR *LinkCopy);
MODULE_SCOPE int	TclWinSymLinkDelete(const WCHAR *LinkOriginal,
			    int linkOnly);
MODULE_SCOPE int	TclWinFileOwned(Tcl_Obj *);
MODULE_SCOPE void	TclWinGenerateChannelName(char channelName[],
			    const char *channelTypeName, void *channelImpl);
MODULE_SCOPE const char*TclpGetUserName(Tcl_DString *bufferPtr);

MODULE_SCOPE void	TclWinAppendSystemError(Tcl_Interp *, DWORD error);

/* Needed by tclWinFile.c and tclWinFCmd.c */
#ifndef FILE_ATTRIBUTE_REPARSE_POINT
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400
#endif

/*
 *----------------------------------------------------------------------
 * Declarations of helper-workers threaded facilities for a pipe based channel.
 *
 * Corresponding functionality provided in "tclWinPipe.c".
 *----------------------------------------------------------------------
 */

typedef struct TclPipeThreadInfo {
    HANDLE evControl;		/* Auto-reset event used by the main thread to
				 * signal when the pipe thread should attempt
				 * to do read/write operation. Additionally
				 * used as signal to stop (state set to -1) */
    volatile LONG state;	/* Indicates current state of the thread */
    void *clientData;		/* Referenced data of the main thread */
} TclPipeThreadInfo;

/* If pipe-workers will use some tcl subsystem, we can use Tcl_Alloc without
 * more overhead for finalize thread (should be executed anyway)
 *
 * #define _PTI_USE_CKALLOC 1
 */

/*
 * State of the pipe-worker.
 *
 * State PTI_STATE_STOP possible from idle state only, worker owns TI structure.
 * Otherwise PTI_STATE_END used (main thread hold ownership of the TI).
 */
enum PipeWorkerStates {
    PTI_STATE_IDLE = 0,		/* idle or not yet initialzed */
    PTI_STATE_WORK = 1,		/* in work */
    PTI_STATE_STOP = 2,		/* thread should stop work (owns TI structure) */
    PTI_STATE_END = 4,		/* thread should stop work (worker is busy) */
    PTI_STATE_DOWN = 8		/* worker is down */
};

MODULE_SCOPE
TclPipeThreadInfo *	TclPipeThreadCreateTI(TclPipeThreadInfo **pipeTIPtr,
			    void *clientData);
MODULE_SCOPE int	TclPipeThreadWaitForSignal(
			    TclPipeThreadInfo **pipeTIPtr);

static inline void
TclPipeThreadSignal(
    TclPipeThreadInfo **pipeTIPtr)
{
    TclPipeThreadInfo *pipeTI = *pipeTIPtr;
    if (pipeTI) {
	SetEvent(pipeTI->evControl);
    }
};

static inline int
TclPipeThreadIsAlive(
    TclPipeThreadInfo **pipeTIPtr)
{
    TclPipeThreadInfo *pipeTI = *pipeTIPtr;
    return (pipeTI && pipeTI->state != PTI_STATE_DOWN);
};

MODULE_SCOPE int	TclPipeThreadStopSignal(TclPipeThreadInfo **pipeTIPtr);
MODULE_SCOPE void	TclPipeThreadStop(TclPipeThreadInfo **pipeTIPtr,
			    HANDLE hThread);
MODULE_SCOPE void	TclPipeThreadExit(TclPipeThreadInfo **pipeTIPtr);

/*
 * Utilities dealing with path buffers
 */
typedef struct TclWinPath {
    WCHAR buffer[MAX_PATH];	/* buffer for path */
    WCHAR *bufferPtr;		/* pointer to buffer (may be same as buffer
				 * or a heap-allocated extension) */
} TclWinPath;

/* Initialize a path buffer. Never returns NULL. */
static inline WCHAR *
TclWinPathInit(
    TclWinPath *pathBufPtr,	/* Structure to be initialized */
    DWORD *capacityPtr)		/* On return, capacity in WCHARS
				   Must NOT be NULL */
{
    pathBufPtr->bufferPtr = pathBufPtr->buffer;
    *capacityPtr = (DWORD)(sizeof(pathBufPtr->buffer) / sizeof(WCHAR));
    return pathBufPtr->bufferPtr;
}

/* Returns pointer to the current buffer */
static inline WCHAR *
TclWinPathGet(
    TclWinPath *pathBufPtr)
{
    return pathBufPtr->bufferPtr;
}

/* Frees a previously initialized path buffer, reseting its state */
static inline void
TclWinPathFree(
    TclWinPath *pathBufPtr)	/* Structure to be freed */
{
    if (pathBufPtr->bufferPtr != pathBufPtr->buffer) {
	Tcl_Free(pathBufPtr->bufferPtr);
    }
    pathBufPtr->bufferPtr = pathBufPtr->buffer;
}

MODULE_SCOPE WCHAR *	TclWinPathResize(TclWinPath *winPathPtr,
			    DWORD capacityNeeded);
MODULE_SCOPE WCHAR *	TclWinGetFullPathName(const WCHAR *pathPtr,
			    TclWinPath *winPathPtr,
			    WCHAR **filePartPtrPtr);
MODULE_SCOPE WCHAR *	TclWinGetCurrentDirectory(TclWinPath *winPathPtr);

#endif	/* _TCLWININT */
