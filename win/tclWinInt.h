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

#ifdef HAVE_NO_SEH
/*
 * Unlike Borland and Microsoft, we don't register exception handlers by
 * pushing registration records onto the runtime stack. Instead, we register
 * them by creating an TCLEXCEPTION_REGISTRATION within the activation record.
 */

typedef struct TCLEXCEPTION_REGISTRATION {
    struct TCLEXCEPTION_REGISTRATION *link;
    EXCEPTION_DISPOSITION (*handler)(
	    struct _EXCEPTION_RECORD*, void*, struct _CONTEXT*, void*);
    void *ebp;
    void *esp;
    int status;
} TCLEXCEPTION_REGISTRATION;
#endif

/*
 * The following specifies how much stack space TclpCheckStackSpace()
 * ensures is available.  TclpCheckStackSpace() is called by Tcl_EvalObj()
 * to help avoid overflowing the stack in the case of infinite recursion.
 */

#define TCL_WIN_STACK_THRESHOLD 0x8000

/*
 * Some versions of Borland C have a define for the OSVERSIONINFO for
 * Win32s and for NT, but not for Windows 95.
 * Define VER_PLATFORM_WIN32_CE for those without newer headers.
 */

#ifndef VER_PLATFORM_WIN32_WINDOWS
#define VER_PLATFORM_WIN32_WINDOWS 1
#endif
#ifndef VER_PLATFORM_WIN32_CE
#define VER_PLATFORM_WIN32_CE 3
#endif

#ifdef _WIN64
#         define TCL_I_MODIFIER        "I"
#else
#         define TCL_I_MODIFIER        ""
#endif

/*
 * The following structure keeps track of whether we are using the 
 * multi-byte or the wide-character interfaces to the operating system.
 * System calls should be made through the following function table.
 */

typedef union {
    WIN32_FIND_DATAA a;
    WIN32_FIND_DATAW w;
} WIN32_FIND_DATAT;

typedef struct TclWinProcs {
    int useWide;

    BOOL (WINAPI *buildCommDCBProc)(CONST TCHAR *, LPDCB);
    TCHAR *(WINAPI *charLowerProc)(TCHAR *);
    BOOL (WINAPI *copyFileProc)(CONST TCHAR *, CONST TCHAR *, BOOL);
    BOOL (WINAPI *createDirectoryProc)(CONST TCHAR *, LPSECURITY_ATTRIBUTES);
    HANDLE (WINAPI *createFileProc)(CONST TCHAR *, DWORD, DWORD, 
	    LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    BOOL (WINAPI *createProcessProc)(CONST TCHAR *, TCHAR *, 
	    LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, 
	    LPVOID, CONST TCHAR *, LPSTARTUPINFOA, LPPROCESS_INFORMATION);
    BOOL (WINAPI *deleteFileProc)(CONST TCHAR *);
    HANDLE (WINAPI *findFirstFileProc)(CONST TCHAR *, WIN32_FIND_DATAT *);
    BOOL (WINAPI *findNextFileProc)(HANDLE, WIN32_FIND_DATAT *);
    BOOL (WINAPI *getComputerNameProc)(WCHAR *, LPDWORD);
    DWORD (WINAPI *getCurrentDirectoryProc)(DWORD, WCHAR *);
    DWORD (WINAPI *getFileAttributesProc)(CONST TCHAR *);
    DWORD (WINAPI *getFullPathNameProc)(CONST TCHAR *, DWORD nBufferLength, 
	    WCHAR *, TCHAR **);
    DWORD (WINAPI *getModuleFileNameProc)(HMODULE, WCHAR *, int);
    DWORD (WINAPI *getShortPathNameProc)(CONST TCHAR *, WCHAR *, DWORD); 
    UINT (WINAPI *getTempFileNameProc)(CONST TCHAR *, CONST TCHAR *, UINT, 
	    WCHAR *);
    DWORD (WINAPI *getTempPathProc)(DWORD, WCHAR *);
    BOOL (WINAPI *getVolumeInformationProc)(CONST TCHAR *, WCHAR *, DWORD, 
	    LPDWORD, LPDWORD, LPDWORD, WCHAR *, DWORD);
    HINSTANCE (WINAPI *loadLibraryExProc)(CONST TCHAR *, HANDLE, DWORD);
    TCHAR (WINAPI *lstrcpyProc)(WCHAR *, CONST TCHAR *);
    BOOL (WINAPI *moveFileProc)(CONST TCHAR *, CONST TCHAR *);
    BOOL (WINAPI *removeDirectoryProc)(CONST TCHAR *);
    DWORD (WINAPI *searchPathProc)(CONST TCHAR *, CONST TCHAR *, 
	    CONST TCHAR *, DWORD, WCHAR *, TCHAR **);
    BOOL (WINAPI *setCurrentDirectoryProc)(CONST TCHAR *);
    BOOL (WINAPI *setFileAttributesProc)(CONST TCHAR *, DWORD);
    /* 
     * These two function pointers will only be set when
     * Tcl_FindExecutable is called.  If you don't ever call that
     * function, the application will crash whenever WinTcl tries to call
     * functions through these null pointers.  That is not a bug in Tcl
     * -- Tcl_FindExecutable is obligatory in recent Tcl releases.
     */
    BOOL (WINAPI *getFileAttributesExProc)(CONST TCHAR *, 
	    GET_FILEEX_INFO_LEVELS, LPVOID);
    BOOL (WINAPI *createHardLinkProc)(CONST TCHAR*, CONST TCHAR*, 
				      LPSECURITY_ATTRIBUTES);
    
    /* deleted INT (__cdecl *utimeProc)(CONST TCHAR*, struct _utimbuf *); */
    /* These two are also NULL at start; see comment above */
    HANDLE (WINAPI *findFirstFileExProc)(CONST TCHAR*, UINT,
					 LPVOID, UINT,
					 LPVOID, DWORD);
    BOOL (WINAPI *getVolumeNameForVMPProc)(CONST TCHAR*, TCHAR*, DWORD);
    DWORD (WINAPI *getLongPathNameProc)(CONST TCHAR*, TCHAR*, DWORD);
    /* 
     * These six are for the security sdk to get correct file
     * permissions on NT, 2000, XP, etc.  On 95,98,ME they are
     * always null.
     */
    BOOL (WINAPI *getFileSecurityProc)(LPCTSTR lpFileName,
		     SECURITY_INFORMATION RequestedInformation,
		     PSECURITY_DESCRIPTOR pSecurityDescriptor,
		     DWORD nLength, 
		     LPDWORD lpnLengthNeeded);
    BOOL (WINAPI *impersonateSelfProc) (SECURITY_IMPERSONATION_LEVEL 
		      ImpersonationLevel);
    BOOL (WINAPI *openThreadTokenProc) (HANDLE ThreadHandle,
		      DWORD DesiredAccess, BOOL OpenAsSelf,
		      PHANDLE TokenHandle);
    BOOL (WINAPI *revertToSelfProc) (void);
    VOID (WINAPI *mapGenericMaskProc) (PDWORD AccessMask,
		      PGENERIC_MAPPING GenericMapping);
    BOOL (WINAPI *accessCheckProc)(PSECURITY_DESCRIPTOR pSecurityDescriptor,
		    HANDLE ClientToken, DWORD DesiredAccess,
		    PGENERIC_MAPPING GenericMapping,
		    PPRIVILEGE_SET PrivilegeSet,
		    LPDWORD PrivilegeSetLength,
		    LPDWORD GrantedAccess,
		    LPBOOL AccessStatus);
    /*
     * Unicode console support. WriteConsole and ReadConsole
     */
    BOOL (WINAPI *readConsoleProc)(
      HANDLE hConsoleInput,
      LPVOID lpBuffer,
      DWORD nNumberOfCharsToRead,
      LPDWORD lpNumberOfCharsRead,
      LPVOID lpReserved
    );
    BOOL (WINAPI *writeConsoleProc)(
      HANDLE hConsoleOutput,
      const VOID* lpBuffer,
      DWORD nNumberOfCharsToWrite,
      LPDWORD lpNumberOfCharsWritten,
      LPVOID lpReserved
    );
    BOOL (WINAPI *getUserName)(LPTSTR lpBuffer, LPDWORD lpnSize);
    /*
     * Windows version dependend functions
     */
    BOOL (WINAPI *cancelSynchronousIo)(HANDLE);
} TclWinProcs;

MODULE_SCOPE TclWinProcs *tclWinProcs;

/*
 * Declarations of functions that are not accessible by way of the
 * stubs table.
 */

MODULE_SCOPE char	TclWinDriveLetterForVolMountPoint(
			    CONST WCHAR *mountPoint);
MODULE_SCOPE void	TclWinEncodingsCleanup();
MODULE_SCOPE void	TclWinInit(HINSTANCE hInst);
MODULE_SCOPE TclFile	TclWinMakeFile(HANDLE handle);
MODULE_SCOPE Tcl_Channel TclWinOpenConsoleChannel(HANDLE handle,
			    char *channelName, int permissions);
MODULE_SCOPE Tcl_Channel TclWinOpenFileChannel(HANDLE handle, char *channelName,
			    int permissions, int appendMode);
MODULE_SCOPE Tcl_Channel TclWinOpenSerialChannel(HANDLE handle,
			    char *channelName, int permissions);
MODULE_SCOPE void	TclWinResetInterfaceEncodings();
MODULE_SCOPE HANDLE	TclWinSerialOpen(HANDLE handle, CONST TCHAR *name,
			    DWORD access);
MODULE_SCOPE int	TclWinSymLinkCopyDirectory(CONST TCHAR* LinkOriginal,
			    CONST TCHAR* LinkCopy);
MODULE_SCOPE int	TclWinSymLinkDelete(CONST TCHAR* LinkOriginal, 
			    int linkOnly);
#if defined(TCL_THREADS) && defined(USE_THREAD_ALLOC)
MODULE_SCOPE void	TclWinFreeAllocCache(void);
MODULE_SCOPE void	TclFreeAllocCache(void *);
MODULE_SCOPE Tcl_Mutex *TclpNewAllocMutex(void);
MODULE_SCOPE void *	TclpGetAllocCache(void);
MODULE_SCOPE void	TclpSetAllocCache(void *);
#endif /* TCL_THREADS */

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
    ClientData clientData;	/* Referenced data of the main thread */
    HANDLE evWakeUp;		/* Optional wake-up event worker set by shutdown */
} TclPipeThreadInfo;


/* If pipe-workers will use some tcl subsystem, we can use ckalloc without
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

#define PTI_STATE_IDLE	0	/* idle or not yet initialzed */
#define PTI_STATE_WORK	1	/* in work */
#define PTI_STATE_STOP	2	/* thread should stop work (owns TI structure) */
#define PTI_STATE_END	4	/* thread should stop work (worker is busy) */
#define PTI_STATE_DOWN  8	/* worker is down */


MODULE_SCOPE 
TclPipeThreadInfo *	TclPipeThreadCreateTI(TclPipeThreadInfo **pipeTIPtr, 
			    ClientData clientData, HANDLE wakeEvent);
MODULE_SCOPE int	TclPipeThreadWaitForSignal(TclPipeThreadInfo **pipeTIPtr);

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

MODULE_SCOPE int	TclPipeThreadStopSignal(TclPipeThreadInfo **pipeTIPtr, HANDLE wakeEvent);
MODULE_SCOPE void	TclPipeThreadStop(TclPipeThreadInfo **pipeTIPtr, HANDLE hThread);
MODULE_SCOPE void	TclPipeThreadExit(TclPipeThreadInfo **pipeTIPtr);

#endif	/* _TCLWININT */
