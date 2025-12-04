/*
 * tclWinReg.c --
 *
 *      This file implements the "wintestext" extension used to test the loading
 *      of extensions.
 *
 * Copyright (c) 2025 Ashok P. Nadkarni
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#undef STATIC_BUILD
#ifndef USE_TCL_STUBS
#   define USE_TCL_STUBS
#endif
#include <windows.h>
#include "tcl.h"
#include <stdlib.h>
#if defined (__clang__) && (__clang_major__ > 20)
#pragma clang diagnostic ignored "-Wc++-keyword"
#endif

#ifdef __cplusplus
extern "C" {
#endif
DLLEXPORT int		Tclwintestextension_Init(Tcl_Interp *interp);
#ifdef __cplusplus
}
#endif

static int ExtensionPathObjCmd(
    void *clientData,
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const objv[])
{
    HMODULE hModule = NULL;
    WCHAR wpath[MAX_PATH];
    DWORD len;
    Tcl_DString ds;
    (void)clientData;
    (void)objc;
    (void)objv;

    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR) ExtensionPathObjCmd, &hModule)) {
	Tcl_SetObjResult(
	    interp,
	    Tcl_ObjPrintf("unable to get module handle: Windows error %lu", GetLastError()));
	return TCL_ERROR;
    }

    len = GetModuleFileNameW(hModule, wpath, MAX_PATH);
    if (len == 0) {
	Tcl_SetObjResult(
	    interp,
	    Tcl_ObjPrintf("unable to get module handle: Windows error %lu", GetLastError()));
        return TCL_ERROR;
    }

    Tcl_DStringInit(&ds);
    Tcl_Char16ToUtfDString(wpath, len, &ds);
    Tcl_DStringResult(interp, &ds);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tclwintestextension_Init --
 *
 *	This function initializes the wintestextension extension.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tclwintestextension_Init(
    Tcl_Interp *interp)
{
    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
	return TCL_ERROR;
    }

    Tcl_CreateObjCommand2(interp, "wintestextension::path", ExtensionPathObjCmd,
	interp, NULL);
    return Tcl_PkgProvideEx(interp, "wintestextension", "0.1", NULL);
}