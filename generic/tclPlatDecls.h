/*
 * tclPlatDecls.h --
 *
 *	Declarations of platform specific Tcl APIs.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * All rights reserved.
 */

#ifndef _TCLPLATDECLS
#define _TCLPLATDECLS

/*
 * TCHAR is needed here for win32, so if it is not defined yet do it here.
 * This way, we don't need to include <tchar.h> just for one define.
 */
#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(_TCHAR_DEFINED)
#   if defined(_UNICODE)
	typedef wchar_t TCHAR;
#   else
	typedef char TCHAR;
#   endif
#   define _TCHAR_DEFINED
#endif

/* !BEGIN!: Do not edit below this line. */

/*
 * Exported function declarations:
 */

#if defined(__WIN32__) || defined(__CYGWIN__) /* WIN */
/* 0 */
EXTERN TCHAR *		Tcl_WinUtfToTChar _ANSI_ARGS_((CONST char *str,
				int len, Tcl_DString *dsPtr));
/* 1 */
EXTERN char *		Tcl_WinTCharToUtf _ANSI_ARGS_((CONST TCHAR *str,
				int len, Tcl_DString *dsPtr));
#endif /* WIN */
#ifdef MAC_TCL
/* 0 */
EXTERN void		Tcl_MacSetEventProc _ANSI_ARGS_((
				Tcl_MacConvertEventPtr procPtr));
/* 1 */
EXTERN char *		Tcl_MacConvertTextResource _ANSI_ARGS_((
				Handle resource));
/* 2 */
EXTERN int		Tcl_MacEvalResource _ANSI_ARGS_((Tcl_Interp *interp,
				CONST char *resourceName, int resourceNumber,
				CONST char *fileName));
/* 3 */
EXTERN Handle		Tcl_MacFindResource _ANSI_ARGS_((Tcl_Interp *interp,
				long resourceType, CONST char *resourceName,
				int resourceNumber, CONST char *resFileRef,
				int *releaseIt));
/* 4 */
EXTERN int		Tcl_GetOSTypeFromObj _ANSI_ARGS_((Tcl_Interp *interp,
				Tcl_Obj *objPtr, OSType *osTypePtr));
/* 5 */
EXTERN void		Tcl_SetOSTypeObj _ANSI_ARGS_((Tcl_Obj *objPtr,
				OSType osType));
/* 6 */
EXTERN Tcl_Obj *	Tcl_NewOSTypeObj _ANSI_ARGS_((OSType osType));
/* 7 */
EXTERN int		strncasecmp _ANSI_ARGS_((CONST char *s1,
				CONST char *s2, size_t n));
/* 8 */
EXTERN int		strcasecmp _ANSI_ARGS_((CONST char *s1,
				CONST char *s2));
#endif /* MAC_TCL */
#ifdef MAC_OSX_TCL /* MACOSX */
/* 0 */
EXTERN int		Tcl_MacOSXOpenBundleResources _ANSI_ARGS_((
				Tcl_Interp *interp, CONST char *bundleName,
				int hasResourceFile, int maxPathLen,
				char *libraryPath));
/* 1 */
EXTERN int		Tcl_MacOSXOpenVersionedBundleResources _ANSI_ARGS_((
				Tcl_Interp *interp, CONST char *bundleName,
				CONST char *bundleVersion,
				int hasResourceFile, int maxPathLen,
				char *libraryPath));
#endif /* MACOSX */

typedef struct TclPlatStubs {
    int magic;
    struct TclPlatStubHooks *hooks;

#if defined(__WIN32__) || defined(__CYGWIN__) /* WIN */
    TCHAR * (*tcl_WinUtfToTChar) _ANSI_ARGS_((CONST char *str, int len, Tcl_DString *dsPtr)); /* 0 */
    char * (*tcl_WinTCharToUtf) _ANSI_ARGS_((CONST TCHAR *str, int len, Tcl_DString *dsPtr)); /* 1 */
#endif /* WIN */
#ifdef MAC_TCL
    void (*tcl_MacSetEventProc) _ANSI_ARGS_((Tcl_MacConvertEventPtr procPtr)); /* 0 */
    char * (*tcl_MacConvertTextResource) _ANSI_ARGS_((Handle resource)); /* 1 */
    int (*tcl_MacEvalResource) _ANSI_ARGS_((Tcl_Interp *interp, CONST char *resourceName, int resourceNumber, CONST char *fileName)); /* 2 */
    Handle (*tcl_MacFindResource) _ANSI_ARGS_((Tcl_Interp *interp, long resourceType, CONST char *resourceName, int resourceNumber, CONST char *resFileRef, int *releaseIt)); /* 3 */
    int (*tcl_GetOSTypeFromObj) _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *objPtr, OSType *osTypePtr)); /* 4 */
    void (*tcl_SetOSTypeObj) _ANSI_ARGS_((Tcl_Obj *objPtr, OSType osType)); /* 5 */
    Tcl_Obj * (*tcl_NewOSTypeObj) _ANSI_ARGS_((OSType osType)); /* 6 */
    int (*strncasecmp) _ANSI_ARGS_((CONST char *s1, CONST char *s2, size_t n)); /* 7 */
    int (*strcasecmp) _ANSI_ARGS_((CONST char *s1, CONST char *s2)); /* 8 */
#endif /* MAC_TCL */
#ifdef MAC_OSX_TCL /* MACOSX */
    int (*tcl_MacOSXOpenBundleResources) _ANSI_ARGS_((Tcl_Interp *interp, CONST char *bundleName, int hasResourceFile, int maxPathLen, char *libraryPath)); /* 0 */
    int (*tcl_MacOSXOpenVersionedBundleResources) _ANSI_ARGS_((Tcl_Interp *interp, CONST char *bundleName, CONST char *bundleVersion, int hasResourceFile, int maxPathLen, char *libraryPath)); /* 1 */
#endif /* MACOSX */
} TclPlatStubs;

#ifdef __cplusplus
extern "C" {
#endif
extern TclPlatStubs *tclPlatStubsPtr;
#ifdef __cplusplus
}
#endif

#if defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS)

/*
 * Inline function declarations:
 */

#if defined(__WIN32__) || defined(__CYGWIN__) /* WIN */
#ifndef Tcl_WinUtfToTChar
#define Tcl_WinUtfToTChar \
	(tclPlatStubsPtr->tcl_WinUtfToTChar) /* 0 */
#endif
#ifndef Tcl_WinTCharToUtf
#define Tcl_WinTCharToUtf \
	(tclPlatStubsPtr->tcl_WinTCharToUtf) /* 1 */
#endif
#endif /* WIN */
#ifdef MAC_TCL
#ifndef Tcl_MacSetEventProc
#define Tcl_MacSetEventProc \
	(tclPlatStubsPtr->tcl_MacSetEventProc) /* 0 */
#endif
#ifndef Tcl_MacConvertTextResource
#define Tcl_MacConvertTextResource \
	(tclPlatStubsPtr->tcl_MacConvertTextResource) /* 1 */
#endif
#ifndef Tcl_MacEvalResource
#define Tcl_MacEvalResource \
	(tclPlatStubsPtr->tcl_MacEvalResource) /* 2 */
#endif
#ifndef Tcl_MacFindResource
#define Tcl_MacFindResource \
	(tclPlatStubsPtr->tcl_MacFindResource) /* 3 */
#endif
#ifndef Tcl_GetOSTypeFromObj
#define Tcl_GetOSTypeFromObj \
	(tclPlatStubsPtr->tcl_GetOSTypeFromObj) /* 4 */
#endif
#ifndef Tcl_SetOSTypeObj
#define Tcl_SetOSTypeObj \
	(tclPlatStubsPtr->tcl_SetOSTypeObj) /* 5 */
#endif
#ifndef Tcl_NewOSTypeObj
#define Tcl_NewOSTypeObj \
	(tclPlatStubsPtr->tcl_NewOSTypeObj) /* 6 */
#endif
#ifndef strncasecmp
#define strncasecmp \
	(tclPlatStubsPtr->strncasecmp) /* 7 */
#endif
#ifndef strcasecmp
#define strcasecmp \
	(tclPlatStubsPtr->strcasecmp) /* 8 */
#endif
#endif /* MAC_TCL */
#ifdef MAC_OSX_TCL /* MACOSX */
#ifndef Tcl_MacOSXOpenBundleResources
#define Tcl_MacOSXOpenBundleResources \
	(tclPlatStubsPtr->tcl_MacOSXOpenBundleResources) /* 0 */
#endif
#ifndef Tcl_MacOSXOpenVersionedBundleResources
#define Tcl_MacOSXOpenVersionedBundleResources \
	(tclPlatStubsPtr->tcl_MacOSXOpenVersionedBundleResources) /* 1 */
#endif
#endif /* MACOSX */

#endif /* defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS) */

/* !END!: Do not edit above this line. */

#endif /* _TCLPLATDECLS */


