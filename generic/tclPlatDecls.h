
/* !BEGIN!: Do not edit below this line. */

/*
 * Exported function declarations:
 */

#if !defined(__WIN32__) && !defined(MAC_TCL)
/* 0 */
EXTERN void		Tcl_CreateFileHandler _ANSI_ARGS_((int fd, int mask, 
				Tcl_FileProc * proc, ClientData clientData));
/* 1 */
EXTERN void		Tcl_DeleteFileHandler _ANSI_ARGS_((int fd));
/* 2 */
EXTERN int		Tcl_GetOpenFile _ANSI_ARGS_((Tcl_Interp * interp, 
				char * string, int write, int checkUsage, 
				ClientData * filePtr));
#endif /* UNIX */

typedef struct TclPlatStubs {
    int magic;
    struct TclPlatStubHooks *hooks;

#if !defined(__WIN32__) && !defined(MAC_TCL)
    void (*tcl_CreateFileHandler) _ANSI_ARGS_((int fd, int mask, Tcl_FileProc * proc, ClientData clientData)); /* 0 */
    void (*tcl_DeleteFileHandler) _ANSI_ARGS_((int fd)); /* 1 */
    int (*tcl_GetOpenFile) _ANSI_ARGS_((Tcl_Interp * interp, char * string, int write, int checkUsage, ClientData * filePtr)); /* 2 */
#endif /* UNIX */
} TclPlatStubs;

extern TclPlatStubs *tclPlatStubsPtr;

#if defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS)

/*
 * Inline function declarations:
 */

#if !defined(__WIN32__) && !defined(MAC_TCL)
#ifndef Tcl_CreateFileHandler
#define Tcl_CreateFileHandler(fd, mask, proc, clientData) \
	(tclPlatStubsPtr->tcl_CreateFileHandler)(fd, mask, proc, clientData) /* 0 */
#endif
#ifndef Tcl_DeleteFileHandler
#define Tcl_DeleteFileHandler(fd) \
	(tclPlatStubsPtr->tcl_DeleteFileHandler)(fd) /* 1 */
#endif
#ifndef Tcl_GetOpenFile
#define Tcl_GetOpenFile(interp, string, write, checkUsage, filePtr) \
	(tclPlatStubsPtr->tcl_GetOpenFile)(interp, string, write, checkUsage, filePtr) /* 2 */
#endif
#endif /* UNIX */

#endif /* defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS) */

/* !END!: Do not edit above this line. */
