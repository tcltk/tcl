
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
#ifdef MAC_TCL
/* 0 */
EXTERN void		Tcl_MacSetEventProc _ANSI_ARGS_((
				Tcl_MacConvertEventPtr procPtr));
/* 1 */
EXTERN char *		Tcl_MacConvertTextResource _ANSI_ARGS_((
				Handle resource));
/* 2 */
EXTERN int		Tcl_MacEvalResource _ANSI_ARGS_((Tcl_Interp * interp, 
				char * resourceName, int resourceNumber, 
				char * fileName));
/* 3 */
EXTERN Handle		Tcl_MacFindResource _ANSI_ARGS_((Tcl_Interp * interp, 
				long resourceType, char * resourceName, 
				int resourceNumber, char * resFileRef, 
				int * releaseIt));
/* 4 */
EXTERN int		Tcl_GetOSTypeFromObj _ANSI_ARGS_((
				Tcl_Interp * interp, Tcl_Obj * objPtr, 
				OSType * osTypePtr));
/* 5 */
EXTERN void		Tcl_SetOSTypeObj _ANSI_ARGS_((Tcl_Obj * objPtr, 
				OSType osType));
/* 6 */
EXTERN Tcl_Obj *	Tcl_NewOSTypeObj _ANSI_ARGS_((OSType osType));
/* 7 */
EXTERN int		strncasecmp _ANSI_ARGS_((CONST char * s1, 
				CONST char * s2, size_t n));
/* 8 */
EXTERN int		strcasecmp _ANSI_ARGS_((CONST char * s1, 
				CONST char * s2));
#endif /* MAC_TCL */

typedef struct TclPlatStubs {
    int magic;
    struct TclPlatStubHooks *hooks;

#if !defined(__WIN32__) && !defined(MAC_TCL)
    void (*tcl_CreateFileHandler) _ANSI_ARGS_((int fd, int mask, Tcl_FileProc * proc, ClientData clientData)); /* 0 */
    void (*tcl_DeleteFileHandler) _ANSI_ARGS_((int fd)); /* 1 */
    int (*tcl_GetOpenFile) _ANSI_ARGS_((Tcl_Interp * interp, char * string, int write, int checkUsage, ClientData * filePtr)); /* 2 */
#endif /* UNIX */
#ifdef MAC_TCL
    void (*tcl_MacSetEventProc) _ANSI_ARGS_((Tcl_MacConvertEventPtr procPtr)); /* 0 */
    char * (*tcl_MacConvertTextResource) _ANSI_ARGS_((Handle resource)); /* 1 */
    int (*tcl_MacEvalResource) _ANSI_ARGS_((Tcl_Interp * interp, char * resourceName, int resourceNumber, char * fileName)); /* 2 */
    Handle (*tcl_MacFindResource) _ANSI_ARGS_((Tcl_Interp * interp, long resourceType, char * resourceName, int resourceNumber, char * resFileRef, int * releaseIt)); /* 3 */
    int (*tcl_GetOSTypeFromObj) _ANSI_ARGS_((Tcl_Interp * interp, Tcl_Obj * objPtr, OSType * osTypePtr)); /* 4 */
    void (*tcl_SetOSTypeObj) _ANSI_ARGS_((Tcl_Obj * objPtr, OSType osType)); /* 5 */
    Tcl_Obj * (*tcl_NewOSTypeObj) _ANSI_ARGS_((OSType osType)); /* 6 */
    int (*strncasecmp) _ANSI_ARGS_((CONST char * s1, CONST char * s2, size_t n)); /* 7 */
    int (*strcasecmp) _ANSI_ARGS_((CONST char * s1, CONST char * s2)); /* 8 */
#endif /* MAC_TCL */
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
#ifdef MAC_TCL
#ifndef Tcl_MacSetEventProc
#define Tcl_MacSetEventProc(procPtr) \
	(tclPlatStubsPtr->tcl_MacSetEventProc)(procPtr) /* 0 */
#endif
#ifndef Tcl_MacConvertTextResource
#define Tcl_MacConvertTextResource(resource) \
	(tclPlatStubsPtr->tcl_MacConvertTextResource)(resource) /* 1 */
#endif
#ifndef Tcl_MacEvalResource
#define Tcl_MacEvalResource(interp, resourceName, resourceNumber, fileName) \
	(tclPlatStubsPtr->tcl_MacEvalResource)(interp, resourceName, resourceNumber, fileName) /* 2 */
#endif
#ifndef Tcl_MacFindResource
#define Tcl_MacFindResource(interp, resourceType, resourceName, resourceNumber, resFileRef, releaseIt) \
	(tclPlatStubsPtr->tcl_MacFindResource)(interp, resourceType, resourceName, resourceNumber, resFileRef, releaseIt) /* 3 */
#endif
#ifndef Tcl_GetOSTypeFromObj
#define Tcl_GetOSTypeFromObj(interp, objPtr, osTypePtr) \
	(tclPlatStubsPtr->tcl_GetOSTypeFromObj)(interp, objPtr, osTypePtr) /* 4 */
#endif
#ifndef Tcl_SetOSTypeObj
#define Tcl_SetOSTypeObj(objPtr, osType) \
	(tclPlatStubsPtr->tcl_SetOSTypeObj)(objPtr, osType) /* 5 */
#endif
#ifndef Tcl_NewOSTypeObj
#define Tcl_NewOSTypeObj(osType) \
	(tclPlatStubsPtr->tcl_NewOSTypeObj)(osType) /* 6 */
#endif
#ifndef strncasecmp
#define strncasecmp(s1, s2, n) \
	(tclPlatStubsPtr->strncasecmp)(s1, s2, n) /* 7 */
#endif
#ifndef strcasecmp
#define strcasecmp(s1, s2) \
	(tclPlatStubsPtr->strcasecmp)(s1, s2) /* 8 */
#endif
#endif /* MAC_TCL */

#endif /* defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS) */

/* !END!: Do not edit above this line. */
