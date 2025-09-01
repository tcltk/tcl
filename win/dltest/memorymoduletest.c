/*
 * memorymoduletest.c --
 *
 *	This file contains a simple Tcl package "memorymoduletest" that is intended for
 *	testing the Tcl dynamic loading facilities using MemoryModule.
 *
 * Copyright © 2024 Jan Nijtmans
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#undef STATIC_BUILD
#include <windows.h>
#include <stdbool.h>
#include "tcl.h"

#ifdef __cplusplus
extern "C" {
#endif


static HMODULE hModule = NULL;

static bool threadAttachCalled = false;

BOOL APIENTRY
DllMain(
    HINSTANCE hInst,		/* Library instance handle. */
    DWORD reason,		/* Reason this function is being called. */
    LPVOID unused)
{
    (void)reason;
    (void)unused;

    switch (reason) {
    case DLL_PROCESS_ATTACH:
	hModule = hInst;
	break;
    case DLL_THREAD_ATTACH:
	threadAttachCalled = true;
	break;
    default:
	break;
    }
    return TRUE;
}

#ifdef __cplusplus
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * Mm_ModuleFileNameACmd --
 *
 *	This procedure is invoked to process the "GetModuleFileNameA" Tcl command.
 *	It calls the Windows GetModuleFileNameA(), and gives back the result.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
Mmt_ModuleFileNameACmd(
    void *dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    char buffer[MAX_PATH];
    (void)dummy;

    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }
    if (GetModuleFileNameA(hModule, buffer, MAX_PATH) == 0) {
	Tcl_WinConvertError(GetLastError());
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("could not determine ModuleFileName: %s",
		Tcl_PosixError(interp)));
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(buffer, -1));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Mm_ModuleFileNameWCmd --
 *
 *	This procedure is invoked to process the "GetModuleFileNameW" Tcl command.
 *	It calls the Windows GetModuleFileNameW(), and gives back the result.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
Mmt_ModuleFileNameWCmd(
    void *dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument strings. */
{
    WCHAR buffer[MAX_PATH];
    Tcl_DString ds;
    (void)dummy;

    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }
    if (GetModuleFileNameW(hModule, buffer, MAX_PATH) == 0) {
	Tcl_WinConvertError(GetLastError());
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("could not determine ModuleFileName: %s",
		Tcl_PosixError(interp)));
	return TCL_ERROR;
    }
    Tcl_DStringInit(&ds);
    Tcl_WCharToUtfDString(buffer, -1, &ds);
    Tcl_SetObjResult(interp, Tcl_DStringToObj(&ds));
    return TCL_OK;
}

static int
Mmt_ThreadAttachCalled(
    void *dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument strings. */
{
    (void)dummy;

    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(threadAttachCalled));
    return TCL_OK;
}

#ifdef _MSC_VER

extern void thrower(); /* This symbol is in thrower.dll */

static int
Mmt_NestedException(
    void *dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument strings. */
{
    (void)dummy;
    int result = -1;

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }
    __try
    {
	thrower();
	result = 0; /* should not be executed*/
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
	/* this should be executed */
	result = GetExceptionCode();
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(result));
    return TCL_OK;
}
#endif /* _MSC_VER */

/*
 *----------------------------------------------------------------------
 *
 * Memorymoduletest_Init --
 *
 *	This is a package initialization procedure, which is called by Tcl
 *	when this package is to be added to an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

DLLEXPORT int
Memorymoduletest_Init(
    Tcl_Interp *interp)		/* Interpreter in which the package is to be
				 * made available. */
{
    int code;

    if (Tcl_InitStubs(interp, "8.7-", 0) == NULL) {
	/* Tcl 8.6 doesn't have Tcl_DStringToObj() */
	return TCL_ERROR;
    }
    code = Tcl_PkgProvide(interp, "memorymoduletest", "1.0.0");
    if (code != TCL_OK) {
	return code;
    }
    Tcl_CreateObjCommand(interp, "GetModuleFileNameA", Mmt_ModuleFileNameACmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "GetModuleFileNameW", Mmt_ModuleFileNameWCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "ThreadAttachCalled", Mmt_ThreadAttachCalled, NULL, NULL);
#ifdef _MSC_VER
    Tcl_CreateObjCommand(interp, "NestedException", Mmt_NestedException, NULL, NULL);
#endif /* _MSC_VER */
    return TCL_OK;
}
