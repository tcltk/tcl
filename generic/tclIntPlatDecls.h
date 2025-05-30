/*
 * tclIntPlatDecls.h --
 *
 *	This file contains the declarations for all platform dependent
 *	unsupported functions that are exported by the Tcl library.  These
 *	interfaces are not guaranteed to remain the same between
 *	versions.  Use at your own risk.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * All rights reserved.
 */

#ifndef _TCLINTPLATDECLS
#define _TCLINTPLATDECLS

#undef TCL_STORAGE_CLASS
#ifdef BUILD_tcl
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   ifdef USE_TCL_STUBS
#      define TCL_STORAGE_CLASS
#   else
#      define TCL_STORAGE_CLASS DLLIMPORT
#   endif
#endif

/*
 * WARNING: This file is automatically generated by the tools/genStubs.tcl
 * script.  Any modifications to the function declarations below should be made
 * in the generic/tclInt.decls script.
 */

/* !BEGIN!: Do not edit below this line. */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Exported function declarations:
 */

/* Slot 0 is reserved */
/* 1 */
EXTERN int		TclpCloseFile(TclFile file);
/* 2 */
EXTERN Tcl_Channel	TclpCreateCommandChannel(TclFile readFile,
				TclFile writeFile, TclFile errorFile,
				size_t numPids, Tcl_Pid *pidPtr);
/* 3 */
EXTERN int		TclpCreatePipe(TclFile *readPipe, TclFile *writePipe);
/* 4 */
EXTERN void *		TclWinGetTclInstance(void);
/* 5 */
EXTERN int		TclUnixWaitForFile(int fd, int mask, int timeout);
/* 6 */
EXTERN TclFile		TclpMakeFile(Tcl_Channel channel, int direction);
/* 7 */
EXTERN TclFile		TclpOpenFile(const char *fname, int mode);
/* 8 */
EXTERN Tcl_Size		TclpGetPid(Tcl_Pid pid);
/* 9 */
EXTERN TclFile		TclpCreateTempFile(const char *contents);
/* Slot 10 is reserved */
/* 11 */
EXTERN void		TclGetAndDetachPids(Tcl_Interp *interp,
				Tcl_Channel chan);
/* Slot 12 is reserved */
/* Slot 13 is reserved */
/* Slot 14 is reserved */
/* 15 */
EXTERN int		TclpCreateProcess(Tcl_Interp *interp, size_t argc,
				const char **argv, TclFile inputFile,
				TclFile outputFile, TclFile errorFile,
				Tcl_Pid *pidPtr);
/* 16 */
EXTERN int		TclpIsAtty(int fd);
/* 17 */
EXTERN int		TclUnixCopyFile(const char *src, const char *dst,
				const Tcl_StatBuf *statBufPtr,
				int dontCopyAtts);
/* Slot 18 is reserved */
/* Slot 19 is reserved */
/* 20 */
EXTERN void		TclWinAddProcess(void *hProcess, Tcl_Size id);
/* Slot 21 is reserved */
/* Slot 22 is reserved */
/* Slot 23 is reserved */
/* 24 */
EXTERN char *		TclWinNoBackslash(char *path);
/* Slot 25 is reserved */
/* Slot 26 is reserved */
/* 27 */
EXTERN void		TclWinFlushDirtyChannels(void);
/* Slot 28 is reserved */
/* 29 */
EXTERN int		TclWinCPUID(int index, int *regs);
/* 30 */
EXTERN int		TclUnixOpenTemporaryFile(Tcl_Obj *dirObj,
				Tcl_Obj *basenameObj, Tcl_Obj *extensionObj,
				Tcl_Obj *resultingNameObj);

typedef struct TclIntPlatStubs {
    int magic;
    void *hooks;

    void (*reserved0)(void);
    int (*tclpCloseFile) (TclFile file); /* 1 */
    Tcl_Channel (*tclpCreateCommandChannel) (TclFile readFile, TclFile writeFile, TclFile errorFile, size_t numPids, Tcl_Pid *pidPtr); /* 2 */
    int (*tclpCreatePipe) (TclFile *readPipe, TclFile *writePipe); /* 3 */
    void * (*tclWinGetTclInstance) (void); /* 4 */
    int (*tclUnixWaitForFile) (int fd, int mask, int timeout); /* 5 */
    TclFile (*tclpMakeFile) (Tcl_Channel channel, int direction); /* 6 */
    TclFile (*tclpOpenFile) (const char *fname, int mode); /* 7 */
    Tcl_Size (*tclpGetPid) (Tcl_Pid pid); /* 8 */
    TclFile (*tclpCreateTempFile) (const char *contents); /* 9 */
    void (*reserved10)(void);
    void (*tclGetAndDetachPids) (Tcl_Interp *interp, Tcl_Channel chan); /* 11 */
    void (*reserved12)(void);
    void (*reserved13)(void);
    void (*reserved14)(void);
    int (*tclpCreateProcess) (Tcl_Interp *interp, size_t argc, const char **argv, TclFile inputFile, TclFile outputFile, TclFile errorFile, Tcl_Pid *pidPtr); /* 15 */
    int (*tclpIsAtty) (int fd); /* 16 */
    int (*tclUnixCopyFile) (const char *src, const char *dst, const Tcl_StatBuf *statBufPtr, int dontCopyAtts); /* 17 */
    void (*reserved18)(void);
    void (*reserved19)(void);
    void (*tclWinAddProcess) (void *hProcess, Tcl_Size id); /* 20 */
    void (*reserved21)(void);
    void (*reserved22)(void);
    void (*reserved23)(void);
    char * (*tclWinNoBackslash) (char *path); /* 24 */
    void (*reserved25)(void);
    void (*reserved26)(void);
    void (*tclWinFlushDirtyChannels) (void); /* 27 */
    void (*reserved28)(void);
    int (*tclWinCPUID) (int index, int *regs); /* 29 */
    int (*tclUnixOpenTemporaryFile) (Tcl_Obj *dirObj, Tcl_Obj *basenameObj, Tcl_Obj *extensionObj, Tcl_Obj *resultingNameObj); /* 30 */
} TclIntPlatStubs;

extern const TclIntPlatStubs *tclIntPlatStubsPtr;

#ifdef __cplusplus
}
#endif

#if defined(USE_TCL_STUBS)

/*
 * Inline function declarations:
 */

/* Slot 0 is reserved */
#define TclpCloseFile \
	(tclIntPlatStubsPtr->tclpCloseFile) /* 1 */
#define TclpCreateCommandChannel \
	(tclIntPlatStubsPtr->tclpCreateCommandChannel) /* 2 */
#define TclpCreatePipe \
	(tclIntPlatStubsPtr->tclpCreatePipe) /* 3 */
#define TclWinGetTclInstance \
	(tclIntPlatStubsPtr->tclWinGetTclInstance) /* 4 */
#define TclUnixWaitForFile \
	(tclIntPlatStubsPtr->tclUnixWaitForFile) /* 5 */
#define TclpMakeFile \
	(tclIntPlatStubsPtr->tclpMakeFile) /* 6 */
#define TclpOpenFile \
	(tclIntPlatStubsPtr->tclpOpenFile) /* 7 */
#define TclpGetPid \
	(tclIntPlatStubsPtr->tclpGetPid) /* 8 */
#define TclpCreateTempFile \
	(tclIntPlatStubsPtr->tclpCreateTempFile) /* 9 */
/* Slot 10 is reserved */
#define TclGetAndDetachPids \
	(tclIntPlatStubsPtr->tclGetAndDetachPids) /* 11 */
/* Slot 12 is reserved */
/* Slot 13 is reserved */
/* Slot 14 is reserved */
#define TclpCreateProcess \
	(tclIntPlatStubsPtr->tclpCreateProcess) /* 15 */
#define TclpIsAtty \
	(tclIntPlatStubsPtr->tclpIsAtty) /* 16 */
#define TclUnixCopyFile \
	(tclIntPlatStubsPtr->tclUnixCopyFile) /* 17 */
/* Slot 18 is reserved */
/* Slot 19 is reserved */
#define TclWinAddProcess \
	(tclIntPlatStubsPtr->tclWinAddProcess) /* 20 */
/* Slot 21 is reserved */
/* Slot 22 is reserved */
/* Slot 23 is reserved */
#define TclWinNoBackslash \
	(tclIntPlatStubsPtr->tclWinNoBackslash) /* 24 */
/* Slot 25 is reserved */
/* Slot 26 is reserved */
#define TclWinFlushDirtyChannels \
	(tclIntPlatStubsPtr->tclWinFlushDirtyChannels) /* 27 */
/* Slot 28 is reserved */
#define TclWinCPUID \
	(tclIntPlatStubsPtr->tclWinCPUID) /* 29 */
#define TclUnixOpenTemporaryFile \
	(tclIntPlatStubsPtr->tclUnixOpenTemporaryFile) /* 30 */

#endif /* defined(USE_TCL_STUBS) */

/* !END!: Do not edit above this line. */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#ifdef MAC_OSX_TCL /* not accessible on Win32/UNIX */
MODULE_SCOPE int TclMacOSXGetFileAttribute(Tcl_Interp *interp,
	int objIndex, Tcl_Obj *fileName,
	Tcl_Obj **attributePtrPtr);
/* 16 */
MODULE_SCOPE int TclMacOSXSetFileAttribute(Tcl_Interp *interp,
	int objIndex, Tcl_Obj *fileName,
	Tcl_Obj *attributePtr);
/* 17 */
MODULE_SCOPE int TclMacOSXCopyFileAttributes(const char *src,
	const char *dst,
	const Tcl_StatBuf *statBufPtr);
/* 18 */
MODULE_SCOPE int TclMacOSXMatchType(Tcl_Interp *interp,
	const char *pathName, const char *fileName,
	Tcl_StatBuf *statBufPtr,
	Tcl_GlobTypeData *types);
#else
#undef TclMacOSXGetFileAttribute /* 15 */
#undef TclMacOSXSetFileAttribute /* 16 */
#undef TclMacOSXCopyFileAttributes /* 17 */
#undef TclMacOSXMatchType /* 18 */
#undef TclMacOSXNotifierAddRunLoopMode /* 19 */
#endif

#if defined(_WIN32)
#   ifndef TCL_NO_DEPRECATED
#	define TclWinConvertError Tcl_WinConvertError
#	define TclWinConvertWSAError Tcl_WinConvertError
#	define TclWinNToHS ntohs
#	define TclpInetNtoa inet_ntoa
#	define TclWinGetServByName getservbyname
#	define TclWinGetSockOpt getsockopt
#	define TclWinSetSockOpt setsockopt
#	define TclWinGetPlatformId() (2) /* VER_PLATFORM_WIN32_NT */
#	define TclWinResetInterfaces() /* nop */
#	define TclWinSetInterfaces(dummy) /* nop */
#   endif /* TCL_NO_DEPRECATED */
#else
#   undef TclpGetPid
#   define TclpGetPid(pid) ((Tcl_Size)(pid))
#endif

#endif /* _TCLINTPLATDECLS */
